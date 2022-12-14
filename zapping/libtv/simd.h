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

/* $Id: simd.h,v 1.9 2007-08-30 14:14:09 mschimek Exp $ */

#ifndef SIMD_H
#define SIMD_H

#include <assert.h>
#include <inttypes.h>
#include "cpu.h"

/*
   This is a simple vector intrinsics abstraction.  The idea is to use
   the Intel/AMD/AltiVec intrinsics where you must and these macros
   where you can.  Intrinsics are halfway portable and easier to
   maintain than asm code or inline asm.  These macros can save typing
   if the SIMD routine is basically the same for MMX, SSE2 and
   AltiVec.

   To select an implementation #define SIMD x before including this
   header, where x is one of:

   CPU_FEATURE_MMX      x86 and x86_64 MMX extension
   CPU_FEATURE_3DNOW    x86 and x86_64 3DNow! extension
   CPU_FEATURE_SSE_INT  x86 and x86_64 SSE extension
   CPU_FEATURE_SSE_FLT  x86 and x86_64 SSE extension
   CPU_FEATURE_SSE2     x86 and x86_64 SSE2 extension
   CPU_FEATURE_SSE3     x86 and x86_64 SSE3 extension
   CPU_FEATURE_ALTIVEC  powerpc AltiVec extension

   Keep a few subtle differences in mind:
   - SSE2 and AltiVec vectors are 128 bit wide, the rest 64 bit.
     Be careful when doing pointer math.  sizeof() is your friend.
   - MMX loads and stores must be 8 byte aligned, SSE2 and AltiVec
     loads and stores must be 16 byte aligned.
     MMX will not segfault on unaligned accesses but they're slow when
     crossing a cache line boundary.  SSE has instructions for unaligned
     accesses, they're slow too.  AltiVec can shift across two registers
     in 1-2 instructions.
   - AltiVec is big endian, MMX/SSE little endian:
     BE:   v bit 0       v bit n,  MSB first in memory.
         0x01020304050607
     LE:   ^ bit n      ^ bit 0,  LSB first in memory.

   Compiling:
   The configure script defines CAN_COMPILE_MMX, CAN_COMPILE_SSE, ... (both
   #defines and automake conditionals) if the compiler recognizes these
   intrinsics.

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

#ifndef SIMD
#  define SIMD 0
#endif

#define SCALAR (1 << 30)

/* XXX GCC 4.1 doesn't support nested always_inline functions with -O0? */
#define always_inline __inline__
#define never_inline __attribute__ ((noinline))

/* ------------------------------------------------------------------------- */

#if SIMD

/* Common macros. */

#define vzero16() ((v16) vzero8 ())
#define vzero32() ((v32) vzero8 ())

#define vzerou8() ((vu8) vzero8 ())
#define vzerou16() ((vu16) vzero8 ())
#define vzerou32() ((vu32) vzero8 ())

#define vminus116() ((v16) vminus18 ())
#define vminus132() ((v32) vminus18 ())

/* Common subexpression elimination doesn't seem to work in gcc 3.x
   with _mm_set1(), so use these constants instead. */
#define SIMD_CONST_PROTOS						\
extern const v8 vsplat8_1;	/* vsplat8(1) */			\
extern const v8 vsplat8_m1;	/* vsplat8(-1) */			\
extern const v8 vsplat8_15;	/* vsplat8(15) */			\
extern const v8 vsplat8_127;	/* vsplat8(127 = 0x7F) */		\
extern const vu8 vsplatu8_F8;	/* vsplatu8(0xF8) */			\
extern const vu8 vsplatu8_FC;	/* vsplatu8(0xFC) */			\
extern const v16 vsplat16_1;	/* vsplat16(1) */			\
extern const v16 vsplat16_2;	/* vsplat16(2) */			\
extern const v16 vsplat16_128;	/* vsplat16(128) */			\
extern const v16 vsplat16_255;	/* vsplat16(255 = 0x00FF) */		\
extern const v16 vsplat16_256;	/* vsplat16(256 = 0x0100) */		\
extern const v16 vsplat16_m256;	/* vsplat16(-256 = 0xFF00) */		\
extern const vu16 vsplatu16_F8;	/* vsplatu16(0x00F8) */			\
extern const v32 vsplat32_1;	/* vsplat32(1) */			\
extern const v32 vsplat32_2;	/* vsplat32(2) */

#define vsplatu8_1 ((vu8) vsplat8_1)
#define vsplat16_m1 ((v16) vsplat8_m1)
#define vsplat32_m1 ((v32) vsplat8_m1)
#define vsplatu8_m1 ((vu8) vsplat8_m1)
#define vsplatu16_m1 ((vu16) vsplat8_m1)
#define vsplatu32_m1 ((vu32) vsplat8_m1)
#define vsplatu8_15 ((vu8) vsplat8_15)
#define vsplatu8_127 ((vu8) vsplat8_127)
#define vsplatu16_1 ((vu16) vsplat16_1)
#define vsplatu16_2 ((vu16) vsplat16_2)
#define vsplatu16_128 ((vu16) vsplat16_128)
#define vsplatu16_255 ((vu16) vsplat16_255)
#define vsplatu16_256 ((vu16) vsplat16_256)
#define vsplatu16_m256 ((vu16) vsplat16_m256)
#define vsplatu32_1 ((vu32) vsplat32_1)
#define vsplatu32_2 ((vu32) vsplat32_2)

/* Neither MMX nor AltiVec have cmplt instructions. */

/* Compare signed (_a < _b) ? 0xFF : 0x00. */
#define vcmplt8(_a, _b) vcmpgt8 (_b, _a)
#define vcmplt16(_a, _b) vcmpgt16 (_b, _a)
#define vcmplt32(_a, _b) vcmpgt32 (_b, _a)

/* Compare signed (_a <= _b) ? 0xFF : 0x00. */
#define vcmple8(_a, _b) vcmpge8 (_b, _a)
#define vcmple16(_a, _b) vcmpge16 (_b, _a)
#define vcmple32(_a, _b) vcmpge32 (_b, _a)

/* Compare unsigned (_a < _b) ? 0xFF : 0x00. */
#define vcmpltu8(_a, _b) vcmpgtu8 (_b, _a)
#define vcmpltu16(_a, _b) vcmpgtu16 (_b, _a)
#define vcmpltu32(_a, _b) vcmpgtu32 (_b, _a)

/* Compare unsigned (_a <= _b) ? 0xFF : 0x00. */
#define vcmpleu8(_a, _b) vcmpgeu8 (_b, _a)
#define vcmpleu16(_a, _b) vcmpgeu16 (_b, _a)
#define vcmpleu32(_a, _b) vcmpgeu32 (_b, _a)

#define vdump(_a)							\
{									\
	union { vu8 v; uint8_t s[sizeof (vu8)]; } _u;			\
	unsigned int _i;						\
									\
	_u.v = (vu8)(_a);						\
	fprintf (stderr, "%s:%u: %s = ", __FILE__, __LINE__, #_a);	\
	for (_i = 0; _i < sizeof (_u.s); ++_i)				\
		fprintf (stderr, "%02x", _u.s[_i]);			\
	fputc ('\n', stderr);						\
}

#else /* !SIMD */

#define SUFFIX _SCALAR

#define vempty() do {} while (0)
#define sfence() do {} while (0)

#endif /* !SIMD */

/* ------------------------------------------------------------------------- */

#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW |			\
	    CPU_FEATURE_SSE_INT | CPU_FEATURE_SSE_FLT)

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

/* Constant 0 (pxor or memory operand). */
#define vzero8() _mm_setzero_si64 ()

/* Constant -1 (should be pcmpeq or memory operand). */
#define vminus18() vsplat8_m1

SIMD_CONST_PROTOS

/* gcc 3.2 bug: _mm_set1_pi16 (_i) produces { _i, _i, 0, 0 } */
/* gcc 4.0 / 4.1 (experimental) bug: uses SSE instruction pshufw
   without -msse. */
/* gcc 4.1 (experimental) bug: simd.c:290: error: unrecognizable insn:
  (insn 743 265 268 17 (set (reg:V2SI 0 ax) (const_vector:V2SI [
   (const_int -1 [0xffffffffffffffff]) (const_int -1 [0xffffffffffffffff])
    ])) -1 (nil) (nil))
  simd.c:290: internal compiler error: in extract_insn, at recog.c:2082
  (That's mmx vsplat8 (-1). Should be pcmpeq I guess, other values work.) */
#if (SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW))			\
    && ((GCC_VERSION >= 30200 && GCC_VERSION < 30300)			\
	|| (GCC_VERSION >= 40000 && GCC_VERSION < 40200))

static always_inline __m64
vsplatu32			(uint32_t		_i)
{
	return (__m64)(_i * 0x0000000100000001ULL);
}

static always_inline __m64
vsplat32			(int32_t		_i)
{
	return vsplatu32 ((uint32_t) _i);
}

static always_inline __m64
vsplatu16			(uint16_t		_i)
{
	return (__m64)(_i * 0x0001000100010001ULL);
}

static always_inline __m64
vsplat16			(int16_t		_i)
{
	return vsplatu16 ((uint16_t) _i);
}

static always_inline __m64
vsplatu8			(uint8_t		_i)
{
	return (__m64)(_i * 0x0101010101010101ULL);
}

static always_inline __m64
vsplat8				(int8_t			_i)
{
	return vsplatu8 ((uint8_t) _i);
}

#else
   /* Set each element to _i. */
#  define vsplat8(_i) _mm_set1_pi8 (_i)
#  define vsplat16(_i) _mm_set1_pi16 (_i)
#  define vsplat32(_i) _mm_set1_pi32 (_i)
#  define vsplatu8(_i) _mm_set1_pi8 (_i)
#  define vsplatu16(_i) _mm_set1_pi16 (_i)
#  define vsplatu32(_i) _mm_set1_pi32 (_i)
#endif

#define vsplatu8i(_i) vsplatu8 (_i)
#define vsplatu16i(_i) vsplatu16 (_i)
#define vsplatu32i(_i) vsplatu32 (_i)

/* Load and store from address + offset (in bytes). */
#define vload(_p, _o) (* (const __m64 *)((const uint8_t *)(_p) + (_o)))
#define vstore(_p, _o, _a) (* (__m64 *)((uint8_t *)(_p) + (_o)) = (_a))

/* Slower unaligned load and store. */
#define vloadu(_p, _o) vload (_p, _o)
#define vstoreu(_p, _o, _a) vstore (_p, _o, _a)

/* Non-temporal load and store (SSE, SSE2, AltiVec), to be
   used if we do not access the same data / cache line soon. */
#define vloadnt(_p, _o) vload (_p, _o)
#define vstorent(_p, _o, _a) vstore (_p, _o, _a)

#define vand(_a, _b) _mm_and_si64 (_a, _b)
/* NOTE: a & ~b as the name suggests. */
#define vandnot(_a, _b) _mm_andnot_si64 (_b, _a)
#define vor(_a, _b) _mm_or_si64 (_a, _b)
#define vxor(_a, _b) _mm_xor_si64 (_a, _b)
/* AltiVec has a vnor (and by extension vnot) instruction.
   Note this is a bitwise not. Boolean not is cheaper with vcmpz
   if we already have 0 in a register. */
#define vnot(_a) vxor (_a, vminus18 ())
#define vnand(_a, _b) vnot (vand (_a, _b))
#define vnor(_a, _b) vnot (vor (_a, _b))

/* For each bit: (1 == _mask) ? _a : _b.  One AltiVec instruction
   but expensive with MMX/SSE/SSE2.  Note AltiVec's vec_sel() has
   the parameters reversed. */
static always_inline __m64
vsel				(__m64			_mask,
				 __m64			_a,
				 __m64			_b)
{
	return vor (vand (_a, _mask), vandnot (_b, _mask));
}

#define vsl(_a, _i) _mm_slli_si64 (_a, _i)
#define vsru(_a, _i) _mm_srli_si64 (_a, _i)
/* Unsigned _a <<= 1 (MMX/3DNow/SSEx have no byte shift ops). */
#define vsl18(_a) _mm_slli_pi16 (vand (_a, vsplat8_127), 1)
/* _a >>= 1. Somewhat expensive, better avoid it. */
#define vsr18(_a) vsel (vsplat8_127, _mm_srli_pi16 (_a, 1), _a)
#define vsr1u8(_a) vand (_mm_srli_pi16 (_a, 1), vsplat8_127)
/* Shift left by immediate. */
#define vsl16(_a, _i) _mm_slli_pi16 (_a, _i)
#define vsl32(_a, _i) _mm_slli_pi32 (_a, _i)
/* Shift right by immediate. */
#define vsr16(_a, _i) _mm_srai_pi16 (_a, _i)
#define vsru16(_a, _i) _mm_srli_pi16 (_a, _i)
#define vsr32(_a, _i) _mm_srai_pi32 (_a, _i)
#define vsru32(_a, _i) _mm_srli_pi32 (_a, _i)

static always_inline __m64
vbswap16			(__m64			_a)
{
	return vor (vsl16 (_a, 8), vsru16 (_a, 8));
}

static always_inline __m64
vwswap32			(__m64			_a)
{
	return vor (vsl32 (_a, 16), vsru32 (_a, 16));
}

/* Long shift right, e.g. (0x0123, 0x4567, 3) -> 0x1234 */
static always_inline __m64
vlsr				(__m64			_h,
				 __m64			_l,
				 const unsigned int	_i)
{
	assert (_i <= 64);

	if (0 == _i) {
		return _l;
	} else if (32 == _i) {
		return _mm_unpackhi_pi32 (_l, vsl (_h, 32));
	} else if (64 == _i) {
		return _h;
	} else {
		return vor (vsru (_l, _i), vsl (_h, 64 - _i));
	}
}

/* Given am = * (const vu8 *) &src[-sizeof (vu8)],
         a0 = * (const vu8 *) src,
         a1 = * (const vu8 *) &src[+sizeof (vu8)],
   where src is uint8_t* and dist is given in bytes
   this emulates an unaligned load from src - dist and src + dist */
static always_inline void
vshiftu2x			(__m64 *		_l,
				 __m64 *		_r,
				 __m64			_am,
				 __m64			_a0,
				 __m64			_a1,
				 const unsigned int	_dist)
{
	assert (_dist <= sizeof (vu8));

#if 0
	if (4 == _dist) {
		_a0 = vwswap32 (_a0);
		*_l = _mm_unpackhi_pi32 (_am, _a0);
		*_r = _mm_unpacklo_pi32 (_a0, _a1);
	} else
#endif
	{
		/* 7654 3210 -> 6543 */
		*_l = vlsr (_a0, _am, (sizeof (vu8) - _dist) * 8);
		/* BA98 7654 -> 8765 */
		*_r = vlsr (_a1, _a0, _dist * 8);
	}
}

#define vunpacklo8(_a, _b) _mm_unpacklo_pi8 (_a, _b)
#define vunpackhi8(_a, _b) _mm_unpackhi_pi8 (_a, _b)
#define vunpacklo16(_a, _b) _mm_unpacklo_pi16 (_a, _b)
#define vunpackhi16(_a, _b) _mm_unpackhi_pi16 (_a, _b)
#define vunpacklo32(_a, _b) _mm_unpacklo_pi32 (_a, _b)
#define vunpackhi32(_a, _b) _mm_unpackhi_pi32 (_a, _b)
#define vunpacklo(_a, _b) _mm_unpacklo_pi32 (_a, _b)
#define vunpackhi(_a, _b) _mm_unpackhi_pi32 (_a, _b)

#define vpacksu16(_a, _b) _mm_packs_pu16 (_a, _b)

/* _a + _b, _a - _b with wrap-around. */
#define vadd8(_a, _b) _mm_add_pi8 (_a, _b)
#define vadd16(_a, _b) _mm_add_pi16 (_a, _b)
#define vadd32(_a, _b) _mm_add_pi32 (_a, _b)
#define vsub8(_a, _b) _mm_sub_pi8 (_a, _b)
#define vsub16(_a, _b) _mm_sub_pi16 (_a, _b)
#define vsub32(_a, _b) _mm_sub_pi32 (_a, _b)

/* Add or subtract with signed saturation. */
#define vadds16(_a, _b) _mm_adds_pi16 (_a, _b)
#define vsubs16(_a, _b) _mm_subs_pi16 (_a, _b)

/* Add or subtract with unsigned saturation. */
#define vaddsu8(_a, _b) _mm_adds_pu8 (_a, _b)
#define vaddsu16(_a, _b) _mm_adds_pu16 (_a, _b)
#define vsubsu8(_a, _b) _mm_subs_pu8 (_a, _b)
#define vsubsu16(_a, _b) _mm_subs_pu16 (_a, _b)

/* Saturate against variable bounds. Expensive! */
#define vsatu8(_a, _min, _max) vminu8 (vmaxu8 (_a, _min), _max)

/* Compare (_a == _b) ? 0xFF : 0x00. */
#define vcmpeq8(_a, _b) _mm_cmpeq_pi8 (_a, _b)
#define vcmpeq16(_a, _b) _mm_cmpeq_pi16 (_a, _b)
#define vcmpeq32(_a, _b) _mm_cmpeq_pi32 (_a, _b)

/* Compare (_a == 0) ? 0xFF : 0x00. */
#define vcmpz8(_a) vcmpeq8 (_a, vzero8 ())
#define vcmpz16(_a) vcmpeq16 (_a, vzero16 ())
#define vcmpz32(_a) vcmpeq32 (_a, vzero32 ())

/* Compare (_a != 0) ? 0xFF : 0x00. */
#define vcmpnz8(_a) vcmpz8 (vcmpz8 (_a))
#define vcmpnz16(_a) vcmpz16 (vcmpz16 (_a))
#define vcmpnz32(_a) vcmpz32 (vcmpz32 (_a))

/* Compare signed (_a > _b) ? 0xFF : 0x00. */
#define vcmpgt8(_a, _b) _mm_cmpgt_pi8 (_a, _b)
#define vcmpgt16(_a, _b) _mm_cmpgt_pi16 (_a, _b)
#define vcmpgt32(_a, _b) _mm_cmpgt_pi32 (_a, _b)

/* Compare unsigned (_a >= _b) ? 0xFF : 0x00. */
#define vcmpgeu8(_a, _b) vcmpz8 (vsubsu8 (_b, _a))

/* Any ideas for cmpge and cmpgtu? :-) */

/* Multiply v16 giving low/high 16 bit of result (vu16). */
#define vmullo16(_a, _b) _mm_mullo_pi16 (_a, _b)
#define vmulhi16(_a, _b) _mm_mulhi_pi16 (_a, _b)

/* Clear MMX state (emms). */
#define vempty() _mm_empty ()

/* Store fence guarantees that every preceding store is globally visible
   before any subsequent store. */
#define sfence() do {} while (0)

/* abs (_a - _b). */
static always_inline vu8
vabsdiffu8			(vu8			_a,
				 vu8			_b)
{
	return vor (vsubsu8 (_a, _b), vsubsu8 (_b, _a));
}

/* (_a + _b + 1) / 2 (single instruction on all but MMX). */
static always_inline vu8
vavgu8				(vu8			_a,
				 vu8			_b)
{
	vu8 carry;

	/* ((_a & 1) + (_b & 1) + 1) >> 1 */ 
	carry = vand (vor (_a, _b), vsplat8_1);

	/* "or 1" instead of "and 127" because we already have the const
	   in a register.  The MSBs cancel out when added. */
	_a = vsru16 (vor (_a, vsplat8_1), 1);
	_b = vsru16 (vor (_b, vsplat8_1), 1);

	return vadd8 (vadd8 (_a, _b), carry);
}

static always_inline vu8
fast_vavgu8			(vu8			_a,
				 vu8			_b)
{
	/* Faster but inaccurate. */
	return vadd8 (vsr1u8 (_a), vsr1u8 (_b));
}

static always_inline vu16
small_vavgu16			(vu16			_a,
				 vu16			_b)
{
	/* For values < 16384. */
	return vsru16 (vadd16 (vadd16 (_a, vsplat16_1), _b), 1);
}

/* min (_a, _b) (single instruction on all but MMX, 3DNow). */
static always_inline vu8
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
static always_inline vu8
vmaxu8				(vu8			_a,
				 vu8			_b)
{
				 /* a > b   a <= b */ 
	_a = vsubsu8 (_a, _b);	 /* a - b   0 */
	return vaddsu8 (_a, _b); /* a       b */
}

/* min (_a, _b), max (_a, _b).
   With MMX this is faster than vmin(), vmax(). */
static always_inline void
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

static always_inline vu16
vminu16i			(vu16			_a,
				 const unsigned int	_i)
{
	vu16 t;

	assert (_i <= 65535);

	t = vsplatu16 (0xFFFF - _i);    /* a > i   a <= i */
	_a = vaddsu16 (_a, t);	 	/* 0xFFFF  a + 0xFFFF - i */
	return vsubsu16 (_a, t);  	/* i       a */
}

/* ------------------------------------------------------------------------- */

#if SIMD == CPU_FEATURE_MMX

#define SUFFIX _MMX

/* ------------------------------------------------------------------------- */

#elif SIMD == CPU_FEATURE_3DNOW

#define SUFFIX _3DNOW

#include <mm3dnow.h>

#define vavgu8(_a, _b) _m_pavgusb (_a, _b)
#define fast_vavgu8(_a, _b) vavgu8 (_a, _b)

/* Fast emms. */
#undef vempty
#define vempty() _m_femms ()

/* ------------------------------------------------------------------------- */

#elif (SIMD == CPU_FEATURE_SSE_INT || SIMD == CPU_FEATURE_SSE_FLT)

#define SUFFIX _SSE

#include <xmmintrin.h>

typedef __m128 vf;

#define vwswap32(_a) _mm_shuffle_pi16 (_a, _MM_SHUFFLE (1, 0, 3, 2))
#define vavgu8(_a, _b) _mm_avg_pu8 (_a, _b)
#define fast_vavgu8(_a, _b) vavgu8 (_a, _b)
#define vavgu16(_a, _b) _mm_avg_pu16 (_a, _b)
#define small_vavgu16(_a, _b) vavgu16 (_a, _b)
#define vminu8(_a, _b) _mm_min_pu8 (_a, _b)
#define vmaxu8(_a, _b) _mm_max_pu8 (_a, _b)
#define vmin16(_a, _b) _mm_min_pi16 (_a, _b)
#define vmax16(_a, _b) _mm_max_pi16 (_a, _b)

#undef sfence
#define sfence() _mm_sfence ()

/* movntq - don't load cache line and don't store _a in cache.
   Might be useful to aggregate stores (write combining, burst writes). */
#undef vstorent
#define vstorent(_p, _o, _a)						\
	_mm_stream_pi ((__m64 *)((uint8_t *)(_p) + (_o)), _a)

#define vloadf(_p, _o)							\
	_mm_load_ps ((const float *)((const uint8_t *)(_p) + (_o)))
#define vstoref(_p, _o, _a)						\
	_mm_store_ps ((float *)((uint8_t *)(_p) + (_o)), _a)

#define vloadfnt(_p, _o) vloadf (_p, _o)
#define vstorefnt(_p, _o, _a)						\
	_mm_stream_ps ((float *)((uint8_t *)(_p) + (_o)), _a)

/* Override MMX inline function vminmaxu8. */
#define vminmaxu8(_minp, _maxp, _a, _b) sse_vminmaxu8 (_minp, _maxp, _a, _b)

static always_inline void
sse_vminmaxu8			(vu8 *			_min,
				 vu8 *			_max,
				 vu8			_a,
				 vu8			_b)
{
	*_min = _mm_min_pu8 (_a, _b);
	*_max = _mm_max_pu8 (_a, _b);
}

#endif /* SIMD == CPU_FEATURE_SSE_INT or _FLT */

/* ========================================================================= */

#elif SIMD & (CPU_FEATURE_SSE2 | CPU_FEATURE_SSE3)

/* Basically the same as MMX/SSE except vectors are 128 bit. */

#include <emmintrin.h>

typedef __m128i v8;
typedef __m128i vu8;
typedef __m128i v16;
typedef __m128i vu16;
typedef __m128i v32;
typedef __m128i vu32;

#define vzero8() _mm_setzero_si128 ()

#define vminus18() vsplat8_m1

SIMD_CONST_PROTOS

#define vsplat8(_i) _mm_set1_epi8 (_i)
#define vsplat16(_i) _mm_set1_epi16 (_i)
#define vsplat32(_i) _mm_set1_epi32 (_i)
#define vsplatu8(_i) _mm_set1_epi8 (_i)
#define vsplatu16(_i) _mm_set1_epi16 (_i)
#define vsplatu32(_i) _mm_set1_epi32 (_i)

#define vsplatu8i(_i) vsplatu8 (_i)
#define vsplatu16i(_i) vsplatu16 (_i)
#define vsplatu32i(_i) vsplatu32 (_i)

/* movd to xmm */
#define vload32(_p, _o)							\
	_mm_cvtsi32_si128 (* (const uint32_t *)				\
			   ((const uint8_t *)(_p) + (_o)))

/* movq to xmm */
#define vload64(_p, _o)							\
	_mm_set_epi64 ((__m64) 0LL,					\
		       * (const __m64 *)((const uint8_t *)(_p) + (_o)))

/* Aligned load and store (movdqa). */
#define vload(_p, _o)							\
	_mm_load_si128 ((const __m128i *)((const uint8_t *)(_p) + (_o)))
#define vstore(_p, _o, _a)						\
	_mm_store_si128 ((__m128i *)((uint8_t *)(_p) + (_o)), _a)

/* Unaligned load and store (movdqu). */
#define vloadu(_p, _o)							\
	_mm_loadu_si128 ((const __m128i *)((const uint8_t *)(_p) + (_o)))
#define vstoreu(_p, _o, _a)						\
	_mm_storeu_si128 ((__m128i *)((uint8_t *)(_p) + (_o)), _a)

#define vloadnt(_p, _o) vload (_p, _o)

/* movntdq - don't load cache line and don't store _a in cache. */
#define vstorent(_p, _o, _a)						\
	_mm_stream_si128 ((__m128i *)((uint8_t *)(_p) + (_o)), _a)

#define vand(_a, _b) _mm_and_si128 (_a, _b)
#define vandnot(_a, _b) _mm_andnot_si128 (_b, _a)
#define vor(_a, _b) _mm_or_si128 (_a, _b)
#define vxor(_a, _b) _mm_xor_si128 (_a, _b)
#define vnot(_a) vxor (vminus18 (), _a)
#define vnand(_a, _b) vnot (vand (_a, _b))
#define vnor(_a, _b) vnot (vor (_a, _b))

/* For each bit: (1 == _mask) ? _a : _b.  One AltiVec instruction
   but expensive with MMX/SSE/SSE2.  Note AltiVec's vec_sel() has
   the parameters reversed. */
static always_inline __m128i
vsel				(__m128i		_mask,
				 __m128i		_a,
				 __m128i		_b)
{
	return vor (vand (_a, _mask), vandnot (_b, _mask));
}

#define vsl18(_a) _mm_slli_epi16 (vand (_a, vsplat8_127), 1)
#define vsr18(_a) vsel (vsplat8_127, _mm_srli_epi16 (_a, 1), _a)
#define vsr1u8(_a) vand (_mm_srli_epi16 (_a, 1), vsplat8_127)
#define vsl16(_a, _i) _mm_slli_epi16 (_a, _i)
#define vsl32(_a, _i) _mm_slli_epi32 (_a, _i)
#define vsr16(_a, _i) _mm_srai_epi16 (_a, _i)
#define vsru16(_a, _i) _mm_srli_epi16 (_a, _i)
#define vsr32(_a, _i) _mm_srai_epi32 (_a, _i)
#define vsru32(_a, _i) _mm_srli_epi32 (_a, _i)

static always_inline __m128i
vbswap16			(__m128i		_a)
{
	return vor (vsl16 (_a, 8), vsru16 (_a, 8));
}

#define vwswap32(_a) _mm_shuffle_epi16 (_a, _MM_SHUFFLE (1, 0, 3, 2))

/* _mm_sxli_si128 is misdefined under gcc -O0.  Arg 2 must be an immediate,
   not a variable which evaluates to one, not even a const variable. */

#if 0

static always_inline __m128i
vsl				(__m128i		_a,
				 const unsigned int	_i)
{
	assert (0 == (_i % 8));

	return _mm_slli_si128 (_a, _i / 8); /* sic */
}

static always_inline __m128i
vsru				(__m128i		_a,
				 const unsigned int	_i)
{
	assert (0 == (_i % 8));

	return _mm_srli_si128 (_a, _i / 8); /* sic */
}

/* Long shift right, e.g. (0x0123, 0x4567, 3) -> 0x1234 */
static always_inline __m128i
vlsr				(__m128i		_h,
				 __m128i		_l,
				 const unsigned int	_i)
{
	assert (_i <= 128);

	if (0 == _i) {
		return _l;
	} else if (64 == _i) {
		return _mm_unpackhi_epi64 (_l, vsl (_h, 64));
	} else if (128 == _i) {
		return _h;
	} else {
		return vor (vsru (_l, _i), vsl (_h, 128 - _i));
	}
}

static always_inline void
vshiftu2x			(__m128i *		_l,
				 __m128i *		_r,
				 __m128i		_am,
				 __m128i		_a0,
				 __m128i		_a1,
				 const unsigned int	_dist)
{
	assert (_dist <= sizeof (vu8));

	if (8 == _dist) {
		_a0 = _mm_shuffle_epi32 (_a0, _MM_SHUFFLE (1, 0, 3, 2));
		*_l = _mm_unpackhi_epi64 (_am, _a0);
		*_r = _mm_unpacklo_epi64 (_a0, _a1);
	} else {
		/* 7654 3210 -> 6543 */
		*_l = vlsr (_a0, _am, (sizeof (vu8) - _dist) * 8);
		/* BA98 7654 -> 8765 */
		*_r = vlsr (_a1, _a0, _dist * 8);
	}
}

#else

#  define vsl(_a, _i)							\
	(assert (0 == ((_i) % 8)), _mm_slli_si128 ((_a), (_i) / 8))
#  define vsru(_a, _i)							\
	(assert (0 == ((_i) % 8)), _mm_srli_si128 ((_a), (_i) / 8))

#define vlsr(_h, _l, _i)						\
({									\
	__m128i h = _h;							\
	__m128i l = _l;							\
									\
	assert (_i <= 128);						\
	(0 == (_i)) ? l :						\
	 (64 == (_i)) ? _mm_unpackhi_epi64 (l, vsl (h, 64)) :		\
	 (128 == (_i)) ? h :						\
	  vor (vsru (l, _i), vsl (h, 128 - (_i)));			\
})

#define vshiftu2x(_l, _r, _am, _a0, _a1, _dist)				\
({									\
    *_l = vlsr (_a0, _am, (sizeof (vu8) - _dist) * 8);			\
    *_r = vlsr (_a1, _a0, _dist * 8);					\
})

#endif

#define vunpacklo8(_a, _b) _mm_unpacklo_epi8 (_a, _b)
#define vunpackhi8(_a, _b) _mm_unpackhi_epi8 (_a, _b)
#define vunpacklo16(_a, _b) _mm_unpacklo_epi16 (_a, _b)
#define vunpackhi16(_a, _b) _mm_unpackhi_epi16 (_a, _b)
#define vunpacklo32(_a, _b) _mm_unpacklo_epi32 (_a, _b)
#define vunpackhi32(_a, _b) _mm_unpackhi_epi32 (_a, _b)
#define vunpacklo64(_a, _b) _mm_unpacklo_epi64 (_a, _b)
#define vunpackhi64(_a, _b) _mm_unpackhi_epi64 (_a, _b)
#define vunpacklo(_a, _b) _mm_unpacklo_epi64 (_a, _b)
#define vunpackhi(_a, _b) _mm_unpackhi_epi64 (_a, _b)

#define vpacksu16(_a, _b) _mm_packs_epu16 (_a, _b)

#define vadd8(_a, _b) _mm_add_epi8 (_a, _b)
#define vadd16(_a, _b) _mm_add_epi16 (_a, _b)
#define vadd32(_a, _b) _mm_add_epi32 (_a, _b)
#define vsub8(_a, _b) _mm_sub_epi8 (_a, _b)
#define vsub16(_a, _b) _mm_sub_epi16 (_a, _b)
#define vsub32(_a, _b) _mm_sub_epi32 (_a, _b)
#define vadds16(_a, _b) _mm_adds_epi16 (_a, _b)
#define vsubs16(_a, _b) _mm_subs_epi16 (_a, _b)
#define vaddsu8(_a, _b) _mm_adds_epu8 (_a, _b)
#define vaddsu16(_a, _b) _mm_adds_epu16 (_a, _b)
#define vsubsu8(_a, _b) _mm_subs_epu8 (_a, _b)
#define vsubsu16(_a, _b) _mm_subs_epu16 (_a, _b)

#define vsatu8(_a, _min, _max) vminu8 (vmaxu8 (_a, _min), _max)

#define vcmpeq8(_a, _b) _mm_cmpeq_epi8 (_a, _b)
#define vcmpeq16(_a, _b) _mm_cmpeq_epi16 (_a, _b)
#define vcmpeq32(_a, _b) _mm_cmpeq_epi32 (_a, _b)

#define vcmpz8(_a) vcmpeq8 (_a, vzero8 ())
#define vcmpz16(_a) vcmpeq16 (_a, vzero8 ())
#define vcmpz32(_a) vcmpeq32 (_a, vzero8 ())

#define vcmpnz8(_a) vcmpz8 (vcmpz8 (_a))
#define vcmpnz16(_a) vcmpz16 (vcmpz16 (_a))
#define vcmpnz32(_a) vcmpz32 (vcmpz32 (_a))

#define vcmpgt8(_a, _b) _mm_cmpgt_epi8 (_a, _b)
#define vcmpgt16(_a, _b) _mm_cmpgt_epi16 (_a, _b)
#define vcmpgt32(_a, _b) _mm_cmpgt_epi32 (_a, _b)

/* Compare unsigned (_a >= _b) ? 0xFF : 0x00. */
#define vcmpgeu8(_a, _b) vcmpz8 (vsubsu8 (_b, _a))

#define vmullo16(_a, _b) _mm_mullo_epi16 (_a, _b)
#define vmulhi16(_a, _b) _mm_mulhi_epi16 (_a, _b)

#define vempty() do {} while (0)
#define sfence() _mm_sfence ()

#define vavgu8(_a, _b) _mm_avg_epu8 (_a, _b)
#define fast_vavgu8(_a, _b) vavgu8 (_a, _b)
#define vavgu16(_a, _b) _mm_avg_epu16 (_a, _b)
#define small_vavgu16(_a, _b) vavgu16 (_a, _b)
#define vminu8(_a, _b) _mm_min_epu8 (_a, _b)
#define vmaxu8(_a, _b) _mm_max_epu8 (_a, _b)
#define vmin16(_a, _b) _mm_min_epi16 (_a, _b)
#define vmax16(_a, _b) _mm_max_epi16 (_a, _b)

/* abs (_a - _b). */
static always_inline vu8
vabsdiffu8			(vu8			_a,
				 vu8			_b)
{
	return vor (vsubsu8 (_a, _b), vsubsu8 (_b, _a));
}

/* min (_a, _b), max (_a, _b). */
static always_inline void
vminmaxu8			(vu8 *			_min,
				 vu8 *			_max,
				 vu8			_a,
				 vu8			_b)
{
	*_min = _mm_min_epu8 (_a, _b);
	*_max = _mm_max_epu8 (_a, _b);
}

/* ------------------------------------------------------------------------- */

#if SIMD == CPU_FEATURE_SSE2

#define SUFFIX _SSE2

/* ------------------------------------------------------------------------- */

#elif SIMD == CPU_FEATURE_SSE3

#define SUFFIX _SSE3

#include <pmmintrin.h>

#if 0
#undef vloadu
/* lddqu - loads 2x128 bits and shifts, might be faster than movdqu if
   the data crosses a cache line boundary. */
#define vloadu(_p, _o)							\
	_mm_lddqu_si128 ((__m128i *)((uint8_t *)(_p) + (_o)))
#endif

#endif /* SIMD == CPU_FEATURE_SSE3 */

/* ========================================================================= */

#elif SIMD == CPU_FEATURE_ALTIVEC

#define SUFFIX _ALTIVEC

/* AltiVec equivalent of the MMX/SSE macros. */
/* Please avoid macro nesting, that compiles much slower. */

#include <altivec.h>

typedef vector signed char v8;
typedef vector unsigned char vu8;
typedef vector signed short v16;
typedef vector unsigned short vu16;
typedef vector signed int v32;
typedef vector unsigned int vu32;

/* vec_splat_s8() broken in gcc 3.4? */
#define vzero8() ((v8) vec_splat_s16 (0))

#define vminus18() vec_splat_s8 (-1)

SIMD_CONST_PROTOS

#define vsplatu8i(_i) vec_splat_u8 (_i)
#define vsplatu16i(_i) vec_splat_u16 (_i)
#define vsplatu32i(_i) vec_splat_u32 (_i)

/* FIXME these macros load a scalar variable into each element
   of the vector.  AltiVec has another instruction to load an
   immediate, but it's limited to -16 ... 15. */
#define vsplat8(_i) vec_splat ((v8) vec_lde (0, &(_i)), 0)
#define vsplat16(_i) vec_splat ((v16) vec_lde (0, &(_i)), 0)
#define vsplat32(_i) vec_splat ((v32) vec_lde (0, &(_i)), 0)
#define vsplatu8(_i) vec_splat ((vu8) vec_lde (0, &(_i)), 0)
#define vsplatu16(_i) vec_splat ((vu16) vec_lde (0, &(_i)), 0)
#define vsplatu32(_i) vec_splat ((vu32) vec_lde (0, &(_i)), 0)

#define vload(_p, _o) vec_ld (_o, _p)
#define vstore(_p, _o, _a) vec_st (_a, _o, _p)

#define vloadnt(_p, _o) vec_ldl (_o, _p)
#define vstorent(_p, _o, _a) vec_stl (_a, _o, _p)

#define vand(_a, _b) vec_and (_a, _b)
#define vandnot(_a, _b) vec_andc (_a, _b)
#define vor(_a, _b) vec_or (_a, _b)
#define vxor(_a, _b) vec_xor (_a, _b)
#define vnot(_a) ({ __typeof__ (_a) __a = (_a); vec_nor (__a, __a); })
#define vnand(_a, _b) vnot (vand (_a, _b))
#define vnor(_a, _b) vec_nor (_a, _b)

/* NOTE I've reversed the parameters to be more like ?: */
#define vsel(_mask, _a, _b) vec_sel (_b, _a, _mask)

#define vadd8(_a, _b) vec_add (_a, _b)
#define vadd16(_a, _b) vec_add (_a, _b)
#define vadd32(_a, _b) vec_add (_a, _b)
#define vsub8(_a, _b) vec_sub (_a, _b)
#define vsub16(_a, _b) vec_sub (_a, _b)
#define vsub32(_a, _b) vec_sub (_a, _b)
#define vadds16(_a, _b) vec_adds (_a, _b)
#define vsubs16(_a, _b) vec_subs (_a, _b)
#define vaddsu8(_a, _b) vec_adds (_a, _b)
#define vaddsu16(_a, _b) vec_adds (_a, _b)
#define vsubsu8(_a, _b) vec_subs (_a, _b)
#define vsubsu16(_a, _b) vec_subs (_a, _b)

static always_inline vu8
vsatu8				(vu8			_a,
				 vu8			_min,
				 vu8			_max)
{
	vu8 m = vec_max (_a, _min);
	return vec_min (m, _max);
}

static always_inline v8
vsl18				(v8			_a)
{
	vu8 one = vec_splat_u8 (1);
	return vec_sl (_a, one);
}

static always_inline v8
vsr18				(v8			_a)
{
	vu8 one = vec_splat_u8 (1);
	return vec_sra (_a, one);
}

static always_inline vu8
vsr1u8				(vu8			_a)
{
	vu8 one = vec_splat_u8 (1);
	return vec_sr (_a, one);
}

static always_inline v16
vsl16				(v16			_a,
				 const unsigned int	_i)
{
	vu16 i = vec_splat_u16 (_i);
	return vec_sl (_a, i);
}

static always_inline v16
vsr16				(v16			_a,
				 const unsigned int	_i)
{
	vu16 i = vec_splat_u16 (_i);
	return vec_sra (_a, i);
}

static always_inline vu16
vsru16				(vu16			_a,
				 const unsigned int	_i)
{
	vu16 i = vec_splat_u16 (_i);
	return vec_sr (_a, i);
}

static always_inline v32
vsl32				(v32			_a,
				 const unsigned int	_i)
{
	vu32 i = vec_splat_u32 (_i);
	return vec_sl (_a, i);
}

static always_inline v32
vsr32				(v32			_a,
				 const unsigned int	_i)
{
	vu32 i = vec_splat_u32 (_i);
	return vec_sra (_a, i);
}

static always_inline vu32
vsru32				(vu32			_a,
				 const unsigned int	_i)
{
	vu32 i = vec_splat_u32 (_i);
	return vec_sr (_a, i);
}

/* Long shift right, e.g. (0x0123, 0x4567, 3) -> 0x1234 */
static always_inline vu8
vlsr				(vu8			_h,
				 vu8			_l,
				 const unsigned int	_i)
{
	assert (_i <= 128);

	if (0 == _i) {
		return _l;
	} else if (128 == _i) {
		return _h;
	} else {
		vu8 sel;

		assert (0 == (_i % 8));

		/* 0x000102..1E1F >> _i */
		sel = vec_lvsr (_i / 8, (const unsigned char *) 0);

		return vec_perm (_h, _l, sel);
				
	}
}

static always_inline void
vshiftu2x			(vu8 *			_l,
				 vu8 *			_r,
				 vu8			_am,
				 vu8			_a0,
				 vu8			_a1,
				 const unsigned int	_dist)
{
	assert (_dist <= sizeof (vu8));

	/* 0123 4567 -> 3456 */
	*_l = vlsr (_am, _a0, _dist * 8);
	/* 4567 89AB -> 5678 */
	*_r = vlsr (_a0, _a1, (sizeof (vu8) - _dist) * 8);
}

#define vcmpeq8(_a, _b) vec_cmpeq (_a, _b)
#define vcmpeq16(_a, _b) vec_cmpeq (_a, _b)
#define vcmpeq32(_a, _b) vec_cmpeq (_a, _b)

static always_inline vector bool char
vcmpz8				(v8			_a)
{
	v8 z = vzero8 ();
	return vec_cmpeq (_a, z);
}

static always_inline vector bool short
vcmpz16				(v16			_a)
{
	v16 z = vzero16 ();
	return vec_cmpeq (_a, z);
}

static always_inline vector bool int
vcmpz32				(v32			_a)
{
	v32 z = vzero32 ();
	return vec_cmpeq (_a, z);
}

static always_inline vector bool char
vcmpnz8				(v8			_a)
{
	vector bool char b = vcmpz8 (_a);
	return vnot (b);
}

static always_inline vector bool short
vcmpnz16			(v16			_a)
{
	vector bool short b = vcmpz16 (_a);
	return vnot (b);
}

static always_inline vector bool int
vcmpnz32			(v32			_a)
{
	vector bool b = vcmpz32 (_a);
	return vnot (b);
}

#define vcmpgt8(_a, _b) vec_cmpgt (_a, _b)
#define vcmpgt16(_a, _b) vec_cmpgt (_a, _b)
#define vcmpgt32(_a, _b) vec_cmpgt (_a, _b)

#define vcmpgtu8(_a, _b) vec_cmpgt (_a, _b)
#define vcmpgtu16(_a, _b) vec_cmpgt (_a, _b)
#define vcmpgtu32(_a, _b) vec_cmpgt (_a, _b)

/* Has no integer cmpge. */
static always_inline vector bool char
vcmpgeu8			(vu8			_a,
				 vu8			_b)
{
	v8 d = (v8) vsubsu8 (_b, _a);
	return vcmpz8 (d);
}

static always_inline vu16
vmullo16			(v16			_a,
				 v16			_b)
{
	v16 zero = vzero16 ();
	return (vu16) vec_mladd (_a, _b, zero);
}

#define vempty() do {} while (0)
#define sfence() do {} while (0)
#define vavgu8(_a, _b) vec_avg (_a, _b)
#define fast_vavgu8(_a, _b) vavgu8 (_a, _b)
#define vminu8(_a, _b) vec_min (_a, _b)
#define vmaxu8(_a, _b) vec_max (_a, _b)
#define vmin16(_a, _b) vec_min (_a, _b)
#define vmax16(_a, _b) vec_max (_a, _b)

static always_inline vu8
vabsdiffu8			(vu8			_a,
				 vu8			_b)
{
	/* Shorter than vec_abs (vec_sub (_a, _b)). */
	return vec_sub (vec_max (_a, _b), vec_min (_a, _b));
}

static always_inline void
vminmaxu8			(vu8 *			_min,
				 vu8 *			_max,
				 vu8			_a,
				 vu8			_b)
{
	*_min = vec_min (_a, _b);
	*_max = vec_max (_a, _b);
}

#endif /* SIMD == CPU_FEATURE_ALTIVEC */

/* ========================================================================= */

#define SIMD_NAME2(name, suffix) name ## suffix
#define SIMD_NAME1(name, suffix) SIMD_NAME2 (name, suffix)

/* Depending on the definition of SIMD this turns
   SIMD_NAME (foobar) into foobar_MMX, foobar_SSE etc. */
#define SIMD_NAME(name) SIMD_NAME1 (name, SUFFIX)

#define SIMD_FN_PROTOS(fn_type, name)					\
extern fn_type name ## _SCALAR;						\
extern fn_type name ## _MMX;						\
extern fn_type name ## _3DNOW;						\
extern fn_type name ## _SSE;						\
extern fn_type name ## _SSE2;						\
extern fn_type name ## _SSE3;						\
extern fn_type name ## _ALTIVEC;

#define SIMD_FN_ARRAY_PROTOS(fn_type, name, dimensions)			\
extern fn_type name ## _SCALAR dimensions;				\
extern fn_type name ## _MMX dimensions;					\
extern fn_type name ## _3DNOW dimensions;				\
extern fn_type name ## _SSE dimensions;					\
extern fn_type name ## _SSE2 dimensions;				\
extern fn_type name ## _SSE3 dimensions;				\
extern fn_type name ## _ALTIVEC dimensions;

#if defined (CAN_COMPILE_MMX)
#  define SIMD_FN_SELECT_MMX(name, avail)				\
	(((avail) & CPU_FEATURE_MMX) & cpu_features) ? name ## _MMX
#else
#  define SIMD_FN_SELECT_MMX(name, avail) 0 ? NULL
#endif

#if defined (CAN_COMPILE_3DNOW)
#  define SIMD_FN_SELECT_3DNOW(name, avail)				\
	(((avail) & CPU_FEATURE_3DNOW) & cpu_features) ? name ## _3DNOW
#else
#  define SIMD_FN_SELECT_3DNOW(name, avail) 0 ? NULL
#endif

#if defined (CAN_COMPILE_SSE)
#  define SIMD_FN_SELECT_SSE(name, avail)				\
	(((avail) & (CPU_FEATURE_SSE_INT |				\
		     CPU_FEATURE_SSE_FLT)) & cpu_features) ? name ## _SSE
#else
#  define SIMD_FN_SELECT_SSE(name, avail) 0 ? NULL
#endif

#if defined (CAN_COMPILE_SSE2)
#  define SIMD_FN_SELECT_SSE2(name, avail)				\
	(((avail) & CPU_FEATURE_SSE2) & cpu_features) ? name ## _SSE2
#else
#  define SIMD_FN_SELECT_SSE2(name, avail) 0 ? NULL
#endif

#if defined (CAN_COMPILE_SSE3)
#  define SIMD_FN_SELECT_SSE3(name, avail)				\
	(((avail) & CPU_FEATURE_SSE3) & cpu_features) ? name ## _SSE3
#else
#  define SIMD_FN_SELECT_SSE3(name, avail) 0 ? NULL
#endif

#if defined (CAN_COMPILE_ALTIVEC)
#  define SIMD_FN_SELECT_ALTIVEC(name, avail)				\
	(((avail) & CPU_FEATURE_ALTIVEC) & cpu_features) ? name ## _ALTIVEC
#else
#  define SIMD_FN_SELECT_ALTIVEC(name, avail) 0 ? NULL
#endif

/* Selects a SIMD function depending on CPU features. */
/* TODO: function automagically learning which of the executable
   versions works fastest on this machine. */
#define SIMD_FN_SELECT(name, avail)					\
	(SIMD_FN_SELECT_ALTIVEC (name, avail) :				\
	 SIMD_FN_SELECT_SSE3 (name, avail) :				\
	 SIMD_FN_SELECT_SSE2 (name, avail) :				\
	 SIMD_FN_SELECT_SSE (name, avail) :				\
	 SIMD_FN_SELECT_3DNOW (name, avail) :				\
	 SIMD_FN_SELECT_MMX (name, avail) :				\
	 ((avail) & SCALAR) ? name ## _SCALAR : NULL)

#define SIMD_FN_ALIGNED_SELECT(name, align, avail)			\
	(likely (0 == ((align) & 15)) ?					\
	 SIMD_FN_SELECT (name, avail) :					\
	 likely (0 == ((align) & 7)) ?					\
	 SIMD_FN_SELECT (name, (avail) & (CPU_FEATURE_SSE_INT |		\
					  CPU_FEATURE_3DNOW |		\
					  CPU_FEATURE_MMX |		\
					  SCALAR)) :			\
	 SIMD_FN_SELECT (name, (avail) & SCALAR))

#endif /* SIMD_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
