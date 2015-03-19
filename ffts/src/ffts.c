/*

This file is part of FFTS -- The Fastest Fourier Transform in the South

Copyright (c) 2012, Anthony M. Blake <amb@anthonix.com>
Copyright (c) 2012, The University of Waikato

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
* Neither the name of the organization nor the
names of its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ANTHONY M. BLAKE BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "ffts.h"

#include "ffts_internal.h"
#include "ffts_static.h"
#include "macros.h"
#include "patterns.h"

#ifndef DYNAMIC_DISABLED
#include "codegen.h"
#endif

#if _WIN32
#include <windows.h>
#else
#if __APPLE__
#include <libkern/OSCacheControl.h>
#endif

#if HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#endif

#if defined(__arm__) && !defined(DYNAMIC_DISABLED)
static const FFTS_ALIGN(64) float w_data[16] = {
     0.70710678118654757273731092936941f,
     0.70710678118654746171500846685376f,
    -0.70710678118654757273731092936941f,
    -0.70710678118654746171500846685376f,
     1.0f,
     0.70710678118654757273731092936941f,
    -0.0f,
    -0.70710678118654746171500846685376f,
     0.70710678118654757273731092936941f,
     0.70710678118654746171500846685376f,
     0.70710678118654757273731092936941f,
     0.70710678118654746171500846685376f,
     1.0f,
     0.70710678118654757273731092936941f,
     0.0f,
     0.70710678118654746171500846685376f
};
#endif

static FFTS_INLINE int ffts_allow_execute(void *start, size_t len)
{
    int result;

#ifdef _WIN32
    DWORD old_protect;
    result = !VirtualProtect(start, len, PAGE_EXECUTE_READ, &old_protect);
#else
    result = mprotect(start, len, PROT_READ | PROT_EXEC);
#endif

    return result;
}

static FFTS_INLINE int ffts_deny_execute(void *start, size_t len)
{
    int result;

#ifdef _WIN32
    DWORD old_protect;
    result = (int) VirtualProtect(start, len, PAGE_READWRITE, &old_protect);
#else
    result = mprotect(start, len, PROT_READ | PROT_WRITE);
#endif

    return result;
}

static FFTS_INLINE int ffts_flush_instruction_cache(void *start, size_t length)
{
#ifdef _WIN32
    return !FlushInstructionCache(GetCurrentProcess(), start, length);
#else
#ifdef __APPLE__
    sys_icache_invalidate(start, length);
#elif __ANDROID__
    cacheflush((long) start, (long) start + length, 0);
#elif __linux__
#if GCC_VERSION_AT_LEAST(4,3)
    __builtin___clear_cache(start, (char*) start + length);
#elif __GNUC__
    __clear_cache((long) start, (long) start + length);
#endif
    return 0;
#endif
#endif
}

static FFTS_INLINE void *ffts_vmem_alloc(size_t length)
{
#if __APPLE__
    return mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
#elif _WIN32
    return VirtualAlloc(NULL, length, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif

    return mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
#endif
}

static FFTS_INLINE void ffts_vmem_free(void *addr, size_t length)
{
#ifdef _WIN32
    (void) length;
    VirtualFree(addr, 0, MEM_RELEASE);
#else
    munmap(addr, length);
#endif
}

void ffts_execute(ffts_plan_t *p, const void *in, void *out)
{
    /* TODO: Define NEEDS_ALIGNED properly instead */
#if defined(HAVE_SSE) || defined(HAVE_NEON)
    if (((uintptr_t) in % 16) != 0) {
        LOG("ffts_execute: input buffer needs to be aligned to a 128bit boundary\n");
    }

    if (((uintptr_t) out % 16) != 0) {
        LOG("ffts_execute: output buffer needs to be aligned to a 128bit boundary\n");
    }
#endif

    p->transform(p, (const float*) in, (float*) out);
}

void ffts_free(ffts_plan_t *p)
{
    if (p) {
        p->destroy(p);
    }
}

void ffts_free_1d(ffts_plan_t *p)
{
#if !defined(DYNAMIC_DISABLED)
    if (p->transform_base) {
        ffts_deny_execute(p->transform_base, p->transform_size);
        ffts_vmem_free(p->transform_base, p->transform_size);
    }
#endif

    if (p->ws_is) {
        free(p->ws_is);
    }

    if (p->ws) {
        FFTS_FREE(p->ws);
    }

    if (p->is) {
        free(p->is);
    }

    if (p->offsets) {
        free(p->offsets);
    }

    free(p);
}

static int
ffts_generate_luts(ffts_plan_t *p, size_t N, size_t leaf_N, int sign)
{
    V4SF MULI_SIGN;
    size_t n_luts;
    ffts_cpx_32f *w;
    ffts_cpx_32f *tmp;
    size_t i, j, m, n;
    int stride;

    if (sign < 0) {
        MULI_SIGN = V4SF_LIT4(-0.0f, 0.0f, -0.0f, 0.0f);
    } else {
        MULI_SIGN = V4SF_LIT4(0.0f, -0.0f, 0.0f, -0.0f);
    }

    /* LUTS */
    n_luts = ffts_ctzl(N / leaf_N);
    if (n_luts >= 32) {
        n_luts = 0;
    }

    if (n_luts) {
        size_t lut_size;

#if defined(__arm__) && !defined(DYNAMIC_DISABLED)
        lut_size = leaf_N * (((1 << n_luts) - 2) * 3 + 1) * sizeof(ffts_cpx_32f) / 2;
#else
        lut_size = leaf_N * (((1 << n_luts) - 2) * 3 + 1) * sizeof(ffts_cpx_32f);
#endif

        p->ws = FFTS_MALLOC(lut_size, 32);
        if (!p->ws) {
            goto cleanup;
        }

        p->ws_is = (size_t*) malloc(n_luts * sizeof(*p->ws_is));
        if (!p->ws_is) {
            goto cleanup;
        }
    }

    w = p->ws;
    n = leaf_N * 2;

#ifdef HAVE_NEON
    V4SF neg = (sign < 0) ? V4SF_LIT4(0.0f, 0.0f, 0.0f, 0.0f) : V4SF_LIT4(-0.0f, -0.0f, -0.0f, -0.0f);
#endif

    /* calculate factors */
    m = leaf_N << (n_luts - 2);
    tmp = FFTS_MALLOC(m * sizeof(ffts_cpx_32f), 32);

    for (i = 0; i < m; i++) {
        tmp[i][0] = W_re(4*m, i);
        tmp[i][1] = W_im(4*m, i);
    }

	/* generate lookup tables */
    stride = 1 << (n_luts - 1);
    for (i = 0; i < n_luts; i++) {
        p->ws_is[i] = w - (ffts_cpx_32f*) p->ws;

        if (!i) {
            ffts_cpx_32f *w0 = FFTS_MALLOC(n/4 * sizeof(ffts_cpx_32f), 32);
            float *fw0 = (float*) w0;
            float *fw = (float*) w;

            for (j = 0; j < n/4; j++) {
                w0[j][0] = tmp[j * stride][0];
                w0[j][1] = tmp[j * stride][1];
            }

#if defined(__arm__) && !defined(DYNAMIC_DISABLED)
#ifdef HAVE_NEON
            for (j = 0; j < n/4; j += 4) {
                V4SF2 temp0 = V4SF2_LD(fw0 + j*2);
                temp0.val[1] = V4SF_XOR(temp0.val[1], neg);
                V4SF2_STORE_SPR(fw + j*2, temp0);
            }
#else
            for (j = 0; j < n/4; j++) {
                fw[j*2+0] = fw0[j*2+0];
                fw[j*2+1] = (sign < 0) ? fw0[j*2+1] : -fw0[j*2+1];
            }
#endif
            w += n/4;
#else
            for (j = 0; j < n/4; j += 2) {
                V4SF re, im, temp0;
                temp0 = V4SF_LD(fw0 + j*2);
                re = V4SF_DUPLICATE_RE(temp0);
                im = V4SF_DUPLICATE_IM(temp0);
                im = V4SF_XOR(im, MULI_SIGN);
                V4SF_ST(fw + j*4 + 0, re);
                V4SF_ST(fw + j*4 + 4, im);
            }

            w += n/4 * 2;
#endif

            FFTS_FREE(w0);
        } else {
            ffts_cpx_32f *w0 = (ffts_cpx_32f*) FFTS_MALLOC(n/8 * sizeof(ffts_cpx_32f), 32);
            ffts_cpx_32f *w1 = (ffts_cpx_32f*) FFTS_MALLOC(n/8 * sizeof(ffts_cpx_32f), 32);
            ffts_cpx_32f *w2 = (ffts_cpx_32f*) FFTS_MALLOC(n/8 * sizeof(ffts_cpx_32f), 32);

            float *fw0 = (float*) w0;
            float *fw1 = (float*) w1;
            float *fw2 = (float*) w2;

            float *fw = (float *)w;

            for (j = 0; j < n/8; j++) {
                w0[j][0] = tmp[2 * j * stride][0];
                w0[j][1] = tmp[2 * j * stride][1];

                w1[j][0] = tmp[j * stride][0];
                w1[j][1] = tmp[j * stride][1];

                w2[j][0] = tmp[(j + (n/8)) * stride][0];
                w2[j][1] = tmp[(j + (n/8)) * stride][1];
            }

#if defined(__arm__) && !defined(DYNAMIC_DISABLED)
#ifdef HAVE_NEON
            for (j = 0; j < n/8; j += 4) {
                V4SF2 temp0, temp1, temp2;

                temp0 = V4SF2_LD(fw0 + j*2);
                temp0.val[1] = V4SF_XOR(temp0.val[1], neg);
                V4SF2_STORE_SPR(fw + j*2*3, temp0);
                
                temp1 = V4SF2_LD(fw1 + j*2);
                temp1.val[1] = V4SF_XOR(temp1.val[1], neg);
                V4SF2_STORE_SPR(fw + j*2*3 + 8,  temp1);
                
                temp2 = V4SF2_LD(fw2 + j*2);
                temp2.val[1] = V4SF_XOR(temp2.val[1], neg);
                V4SF2_STORE_SPR(fw + j*2*3 + 16, temp2);
            }
#else
            for (j = 0; j < n/8; j++) {
                fw[j*6+0] = fw0[j*2+0];
                fw[j*6+1] = (sign < 0) ? fw0[j*2+1] : -fw0[j*2+1];
                fw[j*6+2] = fw1[j*2+0];
                fw[j*6+3] = (sign < 0) ? fw1[j*2+1] : -fw1[j*2+1];
                fw[j*6+4] = fw2[j*2+0];
                fw[j*6+5] = (sign < 0) ? fw2[j*2+1] : -fw2[j*2+1];
            }
#endif
            w += n/8 * 3;
#else
            for (j = 0; j < n/8; j += 2) {
                V4SF temp0, temp1, temp2, re, im;

                temp0 = V4SF_LD(fw0 + j*2);
                re = V4SF_DUPLICATE_RE(temp0);
                im = V4SF_DUPLICATE_IM(temp0);
                im = V4SF_XOR(im, MULI_SIGN);
                V4SF_ST(fw + j*2*6+0, re);
                V4SF_ST(fw + j*2*6+4, im);

                temp1 = V4SF_LD(fw1 + j*2);
                re = V4SF_DUPLICATE_RE(temp1);
                im = V4SF_DUPLICATE_IM(temp1);
                im = V4SF_XOR(im, MULI_SIGN);
                V4SF_ST(fw + j*2*6+8 , re);
                V4SF_ST(fw + j*2*6+12, im);

                temp2 = V4SF_LD(fw2 + j*2);
                re = V4SF_DUPLICATE_RE(temp2);
                im = V4SF_DUPLICATE_IM(temp2);
                im = V4SF_XOR(im, MULI_SIGN);
                V4SF_ST(fw + j*2*6+16, re);
                V4SF_ST(fw + j*2*6+20, im);
            }

            w += n/8 * 3 * 2;
#endif

            FFTS_FREE(w0);
            FFTS_FREE(w1);
            FFTS_FREE(w2);
        }

        n *= 2;
        stride >>= 1;
    }

#if defined(__arm__) && !defined(DYNAMIC_DISABLED)
    if (sign < 0) {
        p->oe_ws = (void*)(&w_data[4]);
        p->ee_ws = (void*)(w_data);
        p->eo_ws = (void*)(&w_data[4]);
    } else {
        p->oe_ws = (void*)(w_data + 12);
        p->ee_ws = (void*)(w_data + 8);
        p->eo_ws = (void*)(w_data + 12);
    }
#endif

    FFTS_FREE(tmp);

    p->lastlut = w;
    p->n_luts = n_luts;
    return 0;

cleanup:
    return -1;
}

ffts_plan_t*
ffts_init_1d(size_t N, int sign)
{
    const size_t leaf_N = 8;
    ffts_plan_t *p;

    if (N < 2 || (N & (N - 1)) != 0) {
        LOG("FFT size must be a power of two\n");
        return NULL;
    }

    p = calloc(1, sizeof(*p));
    if (!p) {
        return NULL;
    }

    p->destroy = ffts_free_1d;
    p->N = N;

    if (N >= 32) {
        /* generate lookup tables */
        if (ffts_generate_luts(p, N, leaf_N, sign)) {
            goto cleanup;
        }

        p->offsets = ffts_init_offsets(N, leaf_N);
        if (!p->offsets) {
            goto cleanup;
        }

        p->is = ffts_init_is(N, leaf_N, 1);
        if (!p->is) {
            goto cleanup;
        }

        p->i0 = N/leaf_N/3 + 1;
        p->i1 = p->i2 = N/leaf_N/3;
        if ((N/leaf_N) % 3 > 1) {
            p->i1++;
        }

#if !defined(HAVE_VFP) || defined(DYNAMIC_DISABLED)
        p->i0 /= 2;
        p->i1 /= 2;
#endif

#ifdef DYNAMIC_DISABLED
        if (sign < 0) {
            p->transform = ffts_static_transform_f_32f;
        } else {
            p->transform = ffts_static_transform_i_32f;
        }
#else
        /* determinate transform size */
#if defined(__arm__)
        if (N < 8192) {
            p->transform_size = 8192;
        } else {
            p->transform_size = N;
        }
#else
        if (N < 2048) {
            p->transform_size = 16384;
        } else {
            p->transform_size = 16384 + 2*N/8 * ffts_ctzl(N);
        }
#endif

        /* allocate code/function buffer */
        p->transform_base = ffts_vmem_alloc(p->transform_size);
        if (!p->transform_base) {
            goto cleanup;
        }

        /* generate code */
        p->transform = ffts_generate_func_code(p, N, leaf_N, sign);
        if (!p->transform) {
            goto cleanup;
        }

        /* enable execution with read access for the block */
        if (ffts_allow_execute(p->transform_base, p->transform_size)) {
            goto cleanup;
        }

        /* flush from the instruction cache */
        if (ffts_flush_instruction_cache(p->transform_base, p->transform_size)) {
            goto cleanup;
        }
#endif
    } else {
        switch (N) {
        case 2:
            p->transform = &ffts_small_2_32f;
            break;
        case 4:
            if (sign == -1) {
                p->transform = &ffts_small_forward4_32f;
            } else if (sign == 1) {
                p->transform = &ffts_small_backward4_32f;
            }
            break;
        case 8:
            if (sign == -1) {
                p->transform = &ffts_small_forward8_32f;
            } else if (sign == 1) {
                p->transform = &ffts_small_backward8_32f;
            }
            break;
        case 16:
        default:
            if (sign == -1) {
                p->transform = &ffts_small_forward16_32f;
            } else {
                p->transform = &ffts_small_backward16_32f;
            }
            break;
        }
    }

    return p;

cleanup:
    ffts_free_1d(p);
    return NULL;
}