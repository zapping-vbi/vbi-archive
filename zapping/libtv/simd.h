/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2005 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: simd.h,v 1.1 2005-03-15 03:56:56 mschimek Exp $ */

#ifndef SIMD_H
#define SIMD_H

#include <assert.h>
#include <inttypes.h>

/*
   This is a simple vector intrinsics abstraction.  The idea is to use
   the Intel/AMD/AltiVec intrinsics where you must and these macros
   where you can.  Intrinsics are halfway portable and easier to
   maintain than pure or inline asm.  These macros can save typing
   if the SIMD routine is basically the same for MMX, SSE2 and
   AltiVec.

   To select an implementation #define SIMD x before including this
   header, where x is one of:

   MMX	    x86 and x86_64 MMX extension
   _3DNOW   x86 and x86_64 3DNow! extension
   SSE	    x86 and x86_64 SSE extension
   SSE2	    x86 and x86_64 SSE2 extension
   SSE3	    x86 and x86_64 SSE3 extension
   ALTIVEC  powerpc AltiVec extension

   Keep a few subtle differences in mind:
   - SSE2 and AltiVec vectors are 128 bit wide, the rest 64 bit.
     Be careful when doing pointer math.  sizeof() is your friend.
   - SSE2 and AltiVec loads and stores must be 16 byte aligned.
     MMX accesses can be unaligned but incur a penalty when crossing
     a cache line boundary, typically at 32 or 64 byte multiples.
   - AltiVec loads and stores are big endian, the rest little endian.

   Compiling:
   The configure script defines HAVE_MMX, HAVE_SSE, ... (both #defines
   and automake conditionals) if the compiler recognizes these intrinsics.

   To compile intrinsics GCC needs a target CPU switch, e. g. -mmmx,
   -msse or -march=pentium4.  The resulting code will be unportable.
   Since we want to build different versions of a SIMD routine and choose
   an implementation at runtime, each version must go into another object
   file compiled with the appropriate flags.
 */

/* For gcc bug checks. */
#ifndef GCC_VERSION
#  define GCC_VERSION (__GNUC__ * 10000					\
		       + __GNUC_MINOR__ * 100				\
		       + __GNUC_PATCHLEVEL__)
#endif

/* Assign values to the keywords so we can compare. */
#define MMX	1
#define _3DNOW	2
#define SSE	3
#define SSE2	4
#define SSE3	5
#define AVEC	6

/* ------------------------------------------------------------------------- */

#if SIMD == MMX || SIMD == _3DNOW || SIMD == SSE

#include <mmintrin.h>

/* AltiVec intrinsics automatically distinguish between element
   size and signedness by the vector type.  Intel intrinsics use macro
   suffixes.  For portability we define it both ways. */

typedef __m64 v8;		/* vector of 8 or 16 int8_t */
typedef __m64 vu8;		/* vector of 8 or 16 uint8_t */
typedef __m64 v16;		/* vector of 4 or 8 int16_t */
typedef __m64 vu16;		/* vector of 4 or 8 uint16_t */
typedef __m64 v32;		/* vector of 2 or 4 int32_t */
typedef __m64 vu32;		/* vector of 2 or 4 uint32_t */

/* Common subexpression elimination doesn't seem to work in gcc 3.x
   with _mm_set1(), so use these constants instead. */
extern const v8 vsplat8_m1;	/* vsplat8(-1) (all ones) */
extern const v8 vsplat8_1;	/* vsplat8(+1) */
extern const v8 vsplat8_127;	/* vsplat8(127) */
extern const v16 vsplat16_255;	/* vsplat16(255) */
extern const v32 vsplat32_1;	/* vsplat32(1) */
extern const v32 vsplat32_2;	/* vsplat32(2) */

/* Constant zero. */
#define vzero8() _mm_setzero_si64 ()
#define vzero16() _mm_setzero_si64 ()
#define vzero32() _mm_setzero_si64 ()

/* gcc bug: would compile to SSE instruction pshufw. */
#if (SIMD == MMX || SIMD == _3DNOW)					\
    && (GCC_VERSION >= 40000 && GCC_VERSION < 40200)

static __inline__ __m64
vsplatu8			(uint8_t		i)
{
	uint64_t t = i;

	t |= t << 8;
	t |= t << 16;
	t |= t << 32;

    	return (__m64) t;
}

static __inline__ __m64
vsplat8				(int8_t			i)
{
	return vsplatu8 ((uint8_t) i);
}

static __inline__ __m64
vsplatu16			(uint16_t		i)
{
	uint64_t t = i;

	t |= t << 16;
	t |= t << 32;

    	return (__m64) t;
}

static __inline__ __m64
vsplat16			(int16_t		i)
{
	return vsplatu16 ((uint16_t) i);
}

#else
   /* Set each element to _i. */
#  define vsplat8(_i) _mm_set1_pi8 (_i)
#  define vsplatu8(_i) _mm_set1_pi8 (_i)
#  define vsplat16(_i) _mm_set1_pi16 (_i)
#  define vsplatu16(_i) _mm_set1_pi16 (_i)
#endif

/* Logical ops. */
#define vand(_a, _b) _mm_and_si64 (_a, _b)
/* NOTE: a & ~b. */
#define vandnot(_a, _b) _mm_andnot_si64 (_b, _a)
#define vor(_a, _b) _mm_or_si64 (_a, _b)
#define vxor(_a, _b) _mm_xor_si64 (_a, _b)
/* AltiVec has vnot, vnor instructions. */
#define vnot(_a) vxor (_a, vsplat8_m1)
#define vnand(_a, _b) vnot (vand (_a, _b))
#define vnor(_a, _b) vnot (vor (_a, _b))

/* Shift right logical or arithmetical by immediate. */
#define vsr16(_a, _i) _mm_srai_pi16 (_a, _i)
#define vsru16(_a, _i) _mm_srli_pi16 (_a, _i)

/* Unsigned _a >>= 1 (MMX/3DNow/SSE/SSE2 have no psrlb instruction). */
#define vsr1u8(_a) vand (vsru16 (_a, 1), vsplat8_127)

/* _a + _b, _a - _b with wrap-around. */
#define vadd8(_a, _b) _mm_add_pi8 (_a, _b)
#define vadd32(_a, _b) _mm_add_pi32 (_a, _b)
#define vsub16(_a, _b) _mm_sub_pi16 (_a, _b)

/* Add or subtract with signed saturation. */
#define vadds16(_a, _b) _mm_adds_pi16 (_a, _b)
#define vsubs16(_a, _b) _mm_subs_pi16 (_a, _b)

/* Add or subtract with unsigned saturation. */
#define vaddsu8(_a, _b) _mm_adds_pu8 (_a, _b)
#define vaddsu16(_a, _b) _mm_adds_pu16 (_a, _b)
#define vsubsu8(_a, _b) _mm_subs_pu8 (_a, _b)
#define vsubsu16(_a, _b) _mm_subs_pu16 (_a, _b)

/* Compare (_a == _b) ? 0xFF : 0x00. */
#define vcmpeq8(_a, _b) _mm_cmpeq_pi8 (_a, _b)
#define vcmpeq16(_a, _b) _mm_cmpeq_pi16 (_a, _b)
#define vcmpeq32(_a, _b) _mm_cmpeq_pi32 (_a, _b)

/* Compare (_a == 0) ? 0xFF : 0x00. */
#define vcmpz8(_a) vcmpeq8 (_a, vzero8 ())
#define vcmpz16(_a) vcmpeq16 (_a, vzero16 ())
#define vcmpz32(_a) vcmpeq32 (_a, vzero32 ())

/* Compare signed (_a > _b) ? 0xFF : 0x00. */
#define vcmpgt8(_a, _b) _mm_cmpgt_pi8 (_a, _b)
#define vcmpgt16(_a, _b) _mm_cmpgt_pi16 (_a, _b)
#define vcmpgt32(_a, _b) _mm_cmpgt_pi32 (_a, _b)

/* Compare unsigned (_a > _b) ? 0xFF : 0x00. */
#define vcmpgtu16(_a, _b) (assert (!"vcmpgtu16"), vzero16 ())

/* Compare unsigned (_a >= _b) ? 0xFF : 0x00. */
#define vcmpgeu8(_a, _b) vcmpz8 (vsubsu8 (_b, _a))

/* Multiply v16 giving low 16 bit of result (vu16). */
#define vmullo16(_a, _b) _mm_mullo_pi16 (_a, _b)

/* Saturate against variable bounds. */
#define vsatu8(_a, _min, _max) vminu8 (vmaxu8 (_a, _min), _max)

/* Clear MMX state (emms).  SSE2 and AltiVec no-op. */
#define vempty() _mm_empty ()

/* Non-temporal load and store (SSE, SSE2, AltiVec), to be
   used if we do not access the same data soon. */
#define vloadnt(_p) (*(_p))
#define vstorent(_p, _a) (*(_p) = (_a))

/* For each bit: (1 == _mask) ? _b : _a (AltiVec single op). */
static __inline__ vu8
vsel				(vu8			_a,
				 vu8			_b,
				 vu8			_mask)
{
	return vor (vand (_b, _mask), vandnot (_a, _mask));
}

/* abs (_a - _b). */
static __inline__ vu8
vabsdiffu8			(vu8			_a,
				 vu8			_b)
{
	return vor (vsubsu8 (_a, _b), vsubsu8 (_b, _a));
}

/* (_a + _b + 1) / 2 (single instruction on all but MMX). */
static __inline__ vu8
vavgu8				(vu8			_a,
				 vu8			_b)
{
	vu8 carry;

	/* ((_a & 1) + (_b & 1) + 1) >> 1 */ 
	carry = vand (vor (_a, _b), vsplat8_1);

	_a = vsru16 (vor (_a, vsplat8_1), 1);
	_b = vsru16 (vor (_b, vsplat8_1), 1);

	return vadd8 (vadd8 (_a, _b), carry);
}

/* (_a + _b + 1) / 2. */
static __inline__ vu8
fast_vavgu8			(vu8			_a,
				 vu8			_b)
{
	/* Fast but inaccurate. */
	_a = vsru16 (vor (_a, vsplat8_1), 1);
	_b = vsru16 (vor (_b, vsplat8_1), 1);

	return vadd8 (_a, _b);
}

/* min (_a, _b) (single instruction on all but MMX, 3DNow). */
static __inline__ vu8
vminu8				(vu8			_a,
				 vu8			_b)
{
	vu8 t;
				/* a > b   a <= b */ 
	t = vsubsu8 (_a, _b);	/* a - b   0 */
	_a = vxor (_a, _b);	/* a ^ b   a ^ b */
	t = vaddsu8 (t, _b);	/* a       b */
	return vxor (t, _a);	/* b       a */
}

/* max (_a, _b) (single instruction on all but MMX, 3DNow). */
static __inline__ vu8
vmaxu8				(vu8			_a,
				 vu8			_b)
{
				 /* a > b   a <= b */ 
	_a = vsubsu8 (_a, _b);	 /* a - b   0 */
	return vaddsu8 (_a, _b); /* a       b */
}

/* min (_a, _b), max (_a, _b).
   With MMX this is faster vmin() and vmax(). */
static __inline__ void
vminmaxu8			(vu8 *			_min,
				 vu8 *			_max,
				 vu8			_a,
				 vu8			_b)
{
	vu8 t;
				/* a > b   a <= b */ 
	t = vsubsu8 (_a, _b);	/* a - b   0 */
	_a = vxor (_a, _b);	/* a ^ b   a ^ b */
	t = vaddsu8 (t, _b);	/* a       b */
	*_max = t;
	*_min = vxor (_a, t);	/* b       a */
}

/* ------------------------------------------------------------------------- */

#if SIMD == MMX

#define SUFFIX __MMX

/* ------------------------------------------------------------------------- */

#elif SIMD == _3DNOW

#define SUFFIX __3DNOW

#include <mm3dnow.h>

#define vavgu8(_a, _b) _m_pavgusb (_a, _b)
#define fast_vavgu8(_a, _b) _m_pavgusb (_a, _b)

/* Fast emms. */
#undef vempty
#define vempty() _m_femms ()

/* ------------------------------------------------------------------------- */

#elif SIMD == SSE

#define SUFFIX __SSE

#include <xmmintrin.h>

#define vavgu8(_a, _b) _mm_avg_pu8 (_a, _b)
#define fast_vavgu8(_a, _b) vavgu8 (_a, _b)
#define vminu8(_a, _b) _mm_min_pu8 (_a, _b)
#define vmaxu8(_a, _b) _mm_max_pu8 (_a, _b)

#undef vstorent
#define vstorent(_p, _a) _mm_stream_pi (_p, _a)

/* Override MMX inline function vminmaxu8. */
#define vminmaxu8(_minp, _maxp, _a, _b) sse_vminmaxu8 (_minp, _maxp, _a, _b)

static __inline__ void
sse_vminmaxu8			(vu8 *			_min,
				 vu8 *			_max,
				 vu8			_a,
				 vu8			_b)
{
	*_min = _mm_min_pu8 (_a, _b);
	*_max = _mm_max_pu8 (_a, _b);
}

#endif /* SIMD == SSE */

/* ========================================================================= */

#elif SIMD == SSE2 || SIMD == SSE3

/* Basically the same as MMX/SSE except vectors are 128 bit. */

#include <emmintrin.h>

typedef __m128i v8;
typedef __m128i vu8;
typedef __m128i v16;
typedef __m128i vu16;
typedef __m128i v32;
typedef __m128i vu32;

extern const v8 vsplat8_m1;	/* vsplat8(-1) */
extern const v8 vsplat8_1;	/* vsplat8(+1) */
extern const v8 vsplat8_127;	/* vsplat8(127) */
extern const v16 vsplat16_255;	/* vsplat16(255) */
extern const v32 vsplat32_1;	/* vsplat32(1) */
extern const v32 vsplat32_2;	/* vsplat32(2) */

#define vzero8() _mm_setzero_si128 ()
#define vzero16() _mm_setzero_si128 ()
#define vzero32() _mm_setzero_si128 ()

#define vsplat8(_i) _mm_set1_epi8 (_i)
#define vsplat16(_i) _mm_set1_epi16 (_i)
#define vsplatu8(_i) _mm_set1_epi8 (_i)
#define vsplatu16(_i) _mm_set1_epi16 (_i)

#define vand(_a, _b) _mm_and_si128 (_a, _b)
#define vandnot(_a, _b) _mm_andnot_si128 (_b, _a)
#define vor(_a, _b) _mm_or_si128 (_a, _b)
#define vxor(_a, _b) _mm_xor_si128 (_a, _b)
#define vnot(_a) vxor (_a, vsplat8_m1)
#define vnand(_a, _b) vnot (vand (_a, _b))
#define vnor(_a, _b) vnot (vor (_a, _b))

#define vsr16(_a, _i) _mm_srai_epi16 (_a, _i)
#define vsru16(_a, _i) _mm_srli_epi16 (_a, _i)
#define vsr1u8(_a) vand (vsru16 (_a, 1), vsplat8_127)

#define vadd8(_a, _b) _mm_add_epi8 (_a, _b)
#define vadd32(_a, _b) _mm_add_epi32 (_a, _b)
#define vsub16(_a, _b) _mm_sub_epi16 (_a, _b)
#define vadds16(_a, _b) _mm_adds_epi16 (_a, _b)
#define vsubs16(_a, _b) _mm_subs_epi16 (_a, _b)
#define vaddsu8(_a, _b) _mm_adds_epu8 (_a, _b)
#define vaddsu16(_a, _b) _mm_adds_epu16 (_a, _b)
#define vsubsu8(_a, _b) _mm_subs_epu8 (_a, _b)
#define vsubsu16(_a, _b) _mm_subs_epu16 (_a, _b)

#define vcmpeq8(_a, _b) _mm_cmpeq_epi8 (_a, _b)
#define vcmpeq16(_a, _b) _mm_cmpeq_epi16 (_a, _b)
#define vcmpeq32(_a, _b) _mm_cmpeq_epi32 (_a, _b)

#define vcmpz8(_a) vcmpeq8 (_a, vzero8 ())
#define vcmpz16(_a) vcmpeq16 (_a, vzero16 ())
#define vcmpz32(_a) vcmpeq32 (_a, vzero32 ())

#define vcmpgt8(_a, _b) _mm_cmpgt_epi8 (_a, _b)
#define vcmpgt16(_a, _b) _mm_cmpgt_epi16 (_a, _b)
#define vcmpgt32(_a, _b) _mm_cmpgt_epi32 (_a, _b)

/* FIXME Compare unsigned (_a > _b) ? 0xFF : 0x00. */
#define vcmpgtu16(_a, _b) (assert (!"vcmpgtu16"), vzero16 ())

/* Compare unsigned (_a >= _b) ? 0xFF : 0x00. */
#define vcmpgeu8(_a, _b) vcmpz8 (vsubsu8 (_b, _a))

#define vmullo16(_a, _b) _mm_mullo_epi16 (_a, _b)
#define vsatu8(_a, _min, _max) vminu8 (vmaxu8 (_a, _min), _max)
#define vempty() do {} while (0)
#define vavgu8(_a, _b) _mm_avg_epu8 (_a, _b)
#define fast_vavgu8(_a, _b) _mm_avg_epu8 (_a, _b)
#define vminu8(_a, _b) _mm_min_epu8 (_a, _b)
#define vmaxu8(_a, _b) _mm_max_epu8 (_a, _b)
#define vloadnt(_p) (*(_p))
#define vstorent(_p, _a) _mm_stream_si128 (_p, _a)

/* For each bit: (1 == _mask) ? _b : _a. */
static __inline__ vu8
vsel				(vu8			_a,
				 vu8			_b,
				 vu8			_mask)
{
	return vor (vand (_b, _mask), vandnot (_a, _mask));
}

/* abs (_a - _b). */
static __inline__ vu8
vabsdiffu8			(vu8			_a,
				 vu8			_b)
{
	return vor (vsubsu8 (_a, _b), vsubsu8 (_b, _a));
}

/* min (_a, _b), max (_a, _b). */
static __inline__ void
vminmaxu8			(vu8 *			_min,
				 vu8 *			_max,
				 vu8			_a,
				 vu8			_b)
{
	*_min = _mm_min_epu8 (_a, _b);
	*_max = _mm_max_epu8 (_a, _b);
}

/* ------------------------------------------------------------------------- */

#if SIMD == SSE2

#define SUFFIX __SSE2

/* ------------------------------------------------------------------------- */

#elif SIMD == SSE3

#define SUFFIX __SSE3

#include <pmmintrin.h>

#endif

/* ========================================================================= */

#elif SIMD == AVEC

#define SUFFIX __AVEC

/* AltiVec equivalent of the MMX/SSE macros. */

#include <altivec.h>

typedef vector char v8;
typedef vector unsigned char vu8;
typedef vector short v16;
typedef vector unsigned short vu16;
typedef vector int v32;
typedef vector unsigned int vu32;

extern const v8 vsplat8_m1;	/* vsplat8(-1) */
extern const v8 vsplat8_1;	/* vsplat8(+1) */
extern const v8 vsplat8_127;	/* vsplat8(127) */
extern const v16 vsplat16_255;	/* vsplat16(255) */
extern const v32 vsplat32_1;	/* vsplat32(1) */
extern const v32 vsplat32_2;	/* vsplat32(2) */

/* FIXME these macros load a scalar variable into each element
   of the vector.  AltiVec has another instruction to load an
   immediate, but it's limited to 0 ... 15. */
#define vsplat8(_i) vec_splat ((v8) vec_lde (0, &(_i)), 0);
#define vsplat16(_i) vec_splat ((v16) vec_lde (0, &(_i)), 0);
#define vsplatu8(_i) vec_splat ((vu8) vec_lde (0, &(_i)), 0);
#define vsplatu16(_i) vec_splat ((vu16) vec_lde (0, &(_i)), 0);

#define vzero8() vec_splat_s8 (0)
#define vzero16() vec_splat_s16 (0)
#define vzero32() vec_splat_s32 (0)

#define vand(_a, _b) vec_and (_a, _b)
#define vandnot(_a, _b) vec_andc (_a, _b)
#define vor(_a, _b) vec_or (_a, _b)
#define vxor(_a, _b) vec_xor (_a, _b)
#define vnot(_a) vec_nor (_a, _a)
#define vnand(_a, _b) vnot (vand (_a, _b))
#define vnor(_a, _b) vec_nor (_a, _b)

#define vadd8(_a, _b) vec_add (_a, _b)
#define vadd32(_a, _b) vec_add (_a, _b)
#define vsub16(_a, _b) vec_sub (_a, _b)
#define vadds16(_a, _b) vec_adds (_a, _b)
#define vsubs16(_a, _b) vec_subs (_a, _b)
#define vaddsu8(_a, _b) vec_adds (_a, _b)
#define vaddsu16(_a, _b) vec_adds (_a, _b)
#define vsubsu8(_a, _b) vec_subs (_a, _b)
#define vsubsu16(_a, _b) vec_subs (_a, _b)

#define vsr16(_a, _i) vec_sra (_a, vec_splat_u16 (_i))
#define vsru16(_a, _i) vec_sr (_a, vec_splat_u16 (_i))
#define vsr1u8(_a, _i) vec_sr (_a, vec_splat_u8 (1))

#define vcmpeq8(_a, _b) vec_cmpeq (_a, _b)
#define vcmpeq16(_a, _b) vec_cmpeq (_a, _b)
#define vcmpeq32(_a, _b) vec_cmpeq (_a, _b)

#define vcmpgt8(_a, _b) vec_cmpgt (_a, _b)
#define vcmpgt16(_a, _b) vec_cmpgt (_a, _b)
#define vcmpgt32(_a, _b) vec_cmpgt (_a, _b)

#define vcmpgtu8(_a, _b) vec_cmpgt (_a, _b)
#define vcmpgtu16(_a, _b) vec_cmpgt (_a, _b)
#define vcmpgtu32(_a, _b) vec_cmpgt (_a, _b)

#define vmullo16(_a, _b) vec_mladd (_a, _b, vec_splat_s16 (0))
#define vsel(_a, _b, _mask) vec_sel (_a, _b, _mask)
#define vsatu8(_a, _min, _max) vminu8 (vmaxu8 (_a, _min), _max)
#define vempty() do {} while (0)
#define vavgu8(_a, _b) vec_avg (_a, _b)
#define fast_vavgu8(_a, _b) vavgu8 (_a, _b)
#define vminu8(_a, _b) vec_min (_a, _b)
#define vmaxu8(_a, _b) vec_max (_a, _b)
#define vloadnt(_p) vec_ldl (0, _p)
#define vstorent(_p, _a) vec_stl (_a, 0, _p)

static __inline__ vu8
vabsdiffu8			(vu8			_a,
				 vu8			_b)
{
	return vec_sub (vec_max (_a, _b), vec_min (_a, _b));
}

static __inline__ void
vminmaxu8			(vu8 *			_min,
				 vu8 *			_max,
				 vu8			_a,
				 vu8			_b)
{
	*_min = vec_min (_a, _b);
	*_max = vec_max (_a, _b);
}

#else

#  error No or unknown SIMD extension selected.

#endif /* SIMD */

/* ========================================================================= */

/* Compare signed (_a < _b) ? 0xFF : 0x00. */
#define vcmplt8(_a, _b) vcmpgt8(_b, _a)
#define vcmplt16(_a, _b) vcmpgt16 (_b, _a)
#define vcmplt32(_a, _b) vcmpgt32 (_b, _a)

/* Compare signed (_a <= _b) ? 0xFF : 0x00. */
#define vcmple8(_a, _b) vcmpge8(_b, _a)
#define vcmple16(_a, _b) vcmpge16 (_b, _a)
#define vcmple32(_a, _b) vcmpge32 (_b, _a)

/* Compare unsigned (_a < _b) ? 0xFF : 0x00. */
#define vcmpltu8(_a, _b) vcmpgtu8(_b, _a)
#define vcmpltu16(_a, _b) vcmpgtu16 (_b, _a)
#define vcmpltu32(_a, _b) vcmpgtu32 (_b, _a)

/* Compare unsigned (_a <= _b) ? 0xFF : 0x00. */
#define vcmpleu8(_a, _b) vcmpgeu8(_b, _a)
#define vcmpleu16(_a, _b) vcmpgeu16 (_b, _a)
#define vcmpleu32(_a, _b) vcmpgeu32 (_b, _a)

/* ------------------------------------------------------------------------- */

#define SIMD_NAME2(name, suffix) name ## suffix
#define SIMD_NAME1(name, suffix) SIMD_NAME2 (name, suffix)

/* Depending on the definition of SIMD this turns
   SIMD_NAME (foobar) into foobar__MMX, foobar__SSE etc. */
#define SIMD_NAME(name) SIMD_NAME1 (name, SUFFIX)

#endif /* SIMD_H */
