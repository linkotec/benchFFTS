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

#ifndef FFTS_INTERNAL_H
#define FFTS_INTERNAL_H

//#include "config.h"
//#include "codegen.h"
#include "ffts_attributes.h"
#include "types.h"

#include <malloc.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define FFTS_PREFIX ffts

#ifndef FFTS_CAT_PREFIX2
#define FFTS_CAT_PREFIX2(a,b) a ## b
#endif

#ifndef FFTS_CAT_PREFIX
#define FFTS_CAT_PREFIX(a,b) FFTS_CAT_PREFIX2(a ## _, b)
#endif

/* prevent symbol name clashes */
#ifdef FFTS_PREFIX
#define FUNC_TO_REWRITE FFTS_CAT_PREFIX(FFTS_PREFIX, FUNC_TO_REWRITE)
#endif

#ifdef __ANDROID__
#include <android/log.h>
#define LOG(s) __android_log_print(ANDROID_LOG_ERROR, "FFTS", s)
#else
#define LOG(s) fprintf(stderr, s)
#endif

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795028841971693993751058209
#endif

typedef struct _ffts_plan_t ffts_plan_t;

typedef void (*transform_func_t)(ffts_plan_t *p, const void *in, void *out);

/**
 * Contains all the Information need to perform FFT
 *
 *
 * DO NOT CHANGE THE ORDER OF MEMBERS
 * ASSEMBLY CODE USES HARD CODED OFFSETS TO REFERENCE
 * SOME OF THESE VARIABES!!
 */
struct _ffts_plan_t {

    /**
     *
     */
    ptrdiff_t *offsets;
#ifdef DYNAMIC_DISABLED
    /**
     * Twiddle factors
     */
    void *ws;

    /**
     * ee - 2 size x  size8
     * oo - 2 x size4 in parallel
     * oe -
     */
    void  *oe_ws, *eo_ws, *ee_ws;
#else
    void FFTS_ALIGN(32) *ws;
    void FFTS_ALIGN(32) *oe_ws, *eo_ws, *ee_ws;
#endif

    /**
     * Pointer into an array of precomputed indexes for the input data array
     */
    ptrdiff_t *is;

    /**
     * Twiddle Factor Indexes
     */
    size_t *ws_is;

    /**
     * Size of the loops for the base cases
     */
    size_t i0, i1, n_luts;

    /**
     * Size fo the Transform
     */
    size_t N;
    void *lastlut;

    /**
     * Used in multidimensional Code ??
     */
    size_t *transforms;

    /**
     * Pointer to the dynamically generated function
     * that will execute the FFT
     */
    transform_func_t transform;

    /**
     * Pointer to the base memory address of
     * of the transform function
     */
    void *transform_base;

    /**
     * Size of the memory block contain the
     * generated code
     */
    size_t transform_size;

    /**
     * Points to the cosnant variables used by
     * the Assembly Code
     */
    void *constants;

    // multi-dimensional stuff:
    struct _ffts_plan_t **plans;
    int rank;
    size_t *Ns, *Ms;
    void *buf;

    void *transpose_buf;

    /**
     * Pointer to the destroy function
     * to clean up the plan after use
     * (differs for real and multi dimension transforms
     */
    void (*destroy)(ffts_plan_t *);

    /**
     * Coefficiants for the real valued transforms
     */
    float *A, *B;

    size_t i2;
};

static FFTS_INLINE void *ffts_aligned_malloc(size_t size)
{
#if defined(_MSC_VER)
    return _aligned_malloc(size, 32);
#else
    return valloc(size);
#endif
}

static FFTS_INLINE void ffts_aligned_free(void *p)
{
#if defined(_MSC_VER)
    _aligned_free(p);
#else
    free(p);
#endif
}

#if GCC_VERSION_AT_LEAST(3,3)
#define ffts_ctzl __builtin_ctzl
#elif defined(_MSC_VER)
#include <intrin.h>
#ifdef _M_X64
#pragma intrinsic(_BitScanForward64)
static __inline unsigned long ffts_ctzl(size_t N)
{
    unsigned long count;
    _BitScanForward64((unsigned long*) &count, N);
    return count;
}
#else
#pragma intrinsic(_BitScanForward)
static __inline unsigned long ffts_ctzl(size_t N)
{
    unsigned long count;
    _BitScanForward((unsigned long*) &count, N);
    return count;
}
#endif /* _WIN64 */
#endif /* _MSC_VER */

static FFTS_ALWAYS_INLINE float W_re(float N, float k)
{
    return cos(-2.0 * M_PI * k / N);
}

static FFTS_ALWAYS_INLINE float W_im(float N, float k)
{
    return sin(-2.0 * M_PI * k / N);
}

#endif /* FFTS_INTERNAL_H */
