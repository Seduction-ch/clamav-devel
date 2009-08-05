/*
 *  Copyright (C) 2009 Sourcefire, Inc.
 *
 *  Authors: aCaB
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

/* zlib-alike state interface to LZMA */

#if HAVE_CONFIG_H
#include "clamav-config.h"
#endif

#include "lzma_iface.h"
#include "7z/LzmaDec.h"
#include "cltypes.h"
#include "others.h"

static void *__wrap_alloc(void *unused, size_t size) { 
    unused = unused;
    return cli_malloc(size);
}
static void *__wrap_free(void *unused, void *freeme) {
    unused = unused;
    free(freeme);
}
static ISzAlloc g_Alloc = { __wrap_alloc, __wrap_free };

struct CLI_LZMA {
    CLzmaDec state;
    unsigned char header[LZMA_PROPS_SIZE];
    unsigned int p_cnt;
    unsigned int s_cnt;
    unsigned int freeme;
    uint64_t usize;
    ELzmaFinishMode finish;
};

static unsigned char lzma_getbyte(CLI_LZMA *L, int *fail) {
    unsigned char *c = (unsigned char *)L->next_in;
    if(!c || !L->avail_in) {
	*fail = 1;
	return 0;
    }
    *fail = 0;
    L->next_in = &c[1];
    L->avail_in--;
    return *c;
}
    

int cli_LzmaInit(CLI_LZMA **Lp, uint64_t size_override) {
    CLI_LZMA *L = *Lp;
    int fail;

    if(!L) {
	*Lp = L = cli_calloc(sizeof(*L), 1);
	if(!L) return CL_EMEM;
	L->p_cnt = LZMA_PROPS_SIZE;
	if(size_override)
	    L->usize = size_override;
	else
	    L->s_cnt = 8;
    } else if(size_override)
	cli_warnmsg("cli_LzmaInit: ignoring late size override\n");

    if(L->freeme) return LZMA_RESULT_OK;

    while(L->p_cnt) {
	L->header[LZMA_PROPS_SIZE - L->p_cnt] = lzma_getbyte(L, &fail);
	if(fail) return LZMA_RESULT_OK;
	L->p_cnt--;
    }

    while(L->s_cnt) {
	uint64_t c = (uint64_t)lzma_getbyte(L, &fail);
	if(fail) return LZMA_RESULT_OK;
	L->usize = c << (8 * (8 - L->s_cnt));
	L->s_cnt--;
    }

    LzmaDec_Construct(&L->state);
    if(LzmaDec_Allocate(&L->state, L->header, LZMA_PROPS_SIZE, &g_Alloc) != SZ_OK)
	return CL_EMEM;
    LzmaDec_Init(&state);

    L->freeme = 1;
    if(~L-usize) L->finish = LZMA_FINISH_END;
    else L->finish = LZMA_FINISH_ANY;
    return LZMA_RESULT_OK;
}
	

void cli_LzmaShutdown(CLI_LZMA **Lp) {
    CLI_LZMA *L;

    if(!Lp) return;
    L = *Lp;
    if(L->freeme)
	LzmaDec_Free(&L->state, &g_Alloc);
    free(L);
    *Lp = NULL;
    return;
}


int cli_LzmaDecode(CLI_LZMA **Lp) {
    CLI_LZMA *L = *Lp;

    if(!L->freeme) return cli_LzmaInit(LP, 0);

    SRes res;
    SizeT outbytes = L->avail_out;
    SizeT inbytes = L->avail_in;
    ELzmaStatus status;
    res = LzmaDec_DecodeToBuf(&L->state, L->next_out, &outbytes, L->next_in, &inbytes, L->finish, &status);
    L->next_in += inbytes;
    L->next_out += outbytes;
    L->usize -= outbytes;

}

/* int cli_LzmaInitUPX(CLI_LZMA **Lp, uint32_t dictsz) { */
/*   CLI_LZMA *L = *Lp; */

/*   if(!L) { */
/*     *Lp = L = cli_calloc(sizeof(*L), 1); */
/*     if(!L) { */
/*       return LZMA_RESULT_DATA_ERROR; */
/*     } */
/*   } */

/*   L->state.Properties.pb = 2; /\* FIXME: these  *\/ */
/*   L->state.Properties.lp = 0; /\* values may    *\/ */
/*   L->state.Properties.lc = 3; /\* not be static *\/ */

/*   L->state.Properties.DictionarySize = dictsz; */

/*   if (!(L->state.Probs = (CProb *)cli_malloc(LzmaGetNumProbs(&L->state.Properties) * sizeof(CProb)))) */
/*     return LZMA_RESULT_DATA_ERROR; */

/*   if (!(L->state.Dictionary = (unsigned char *)cli_malloc(L->state.Properties.DictionarySize))) { */
/*     free(L->state.Probs); */
/*     return LZMA_RESULT_DATA_ERROR; */
/*   } */

/*   L->initted = 1; */

/*   LzmaDecoderInit(&L->state); */
/*   return LZMA_RESULT_OK; */
/* } */
