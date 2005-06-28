/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: windows.h,v 1.7 2005-06-28 00:49:29 mschimek Exp $ */

#ifndef WINDOWS_H
#define WINDOWS_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>		/* malloc() */
#include <stddef.h>		/* size_t */
#include <inttypes.h>		/* int64_t */
#include <assert.h>
#include <string.h>		/* memcpy() */
#include "common/intl-priv.h"	/* i18n */
#include "libtv/misc.h"
#include "libtv/cpu.h"		/* cpu_features */
#include "libtv/simd.h"

#if SIMD

static void
copy_line			(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		n_bytes)
__attribute__ ((unused));

static void
copy_line_pair			(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		n_bytes,
				 unsigned int		dst_bpl)
__attribute__ ((unused));

#  if Z_BYTE_ORDER == Z_LITTLE_ENDIAN

#    define YMask vsplat16_255
#    define UVMask vsplat16_m256
#    define yuyv2yy(yuyv) ((v16) vand ((v16)(yuyv), YMask))
#    define yuyv2uv(yuyv) ((v16) vsru16 ((v16)(yuyv), 8))

#  else

#    define YMask vsplat16_m256
#    define UVMask vsplat16_255
#    define yuyv2yy(yuyv) ((v16) vsru16 ((vu16)(yuyv), 8))
#    define yuyv2uv(yuyv) ((v16) vand ((v16)(yuyv), UVMask))

#  endif /* Z_BYTE_ORDER */

#if SIMD == CPU_FEATURE_ALTIVEC

/* Defined here to enable parameter passing in registers.
   NOTE n_bytes must be a multiple of sizeof (vu8). */
static void
copy_line			(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		n_bytes)
{
	unsigned int i = 0;

	for (n_bytes /= sizeof (vu8); n_bytes > 0; --n_bytes) {
		vstorent (dst, i, vload (src, i));
		i += sizeof (vu8);
	}
}

/* Copies src to dst and dst + dst_bpl.  Defined here to enable parameter
   passing in registers.  NOTE dst_bpl and n_bytes must be a multiple of
   sizeof (vu8). */
static void
copy_line_pair			(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		n_bytes,
				 unsigned int		dst_bpl)
{
	for (n_bytes /= sizeof (vu8); n_bytes > 0; --n_bytes) {
		vu8 m0;

		m0 = vload (src, 0);
		src += sizeof (vu8);

		vstorent (dst, 0, m0);
		vstorent (dst, dst_bpl, m0);
		dst += sizeof (vu8);
	}
}

/* Do an aligned load from s, and two unaligned loads from
    s + offs - dist and s + offs + dist.  Offs and dist are given
    in bytes, offs must be a multiple of sizeof (vu8). */
static always_inline void
uloadxt				(vu8 *			left,
				 vu8 *			center,
				 vu8 *			right,
				 const uint8_t *	src,
				 unsigned int		offs,
				 unsigned int		dist)
{
	vu8 t1, t2, t3;

	src += offs;

	t1 = vload (src, -1 * (long) sizeof (vu8));
	t2 = vload (src,  0 * (long) sizeof (vu8));
	t3 = vload (src, +1 * (long) sizeof (vu8));

	*left = vec_perm (t1, t2, vec_lvsl (-dist, src));
	*center = t2;
	*right = vec_perm (t2, t3, vec_lvsl (+dist, src));
}

static always_inline void
uload24t			(vu8 *			m4,
				 vu8 *			m2,
				 vu8 *			center,
				 vu8 *			p2,
				 vu8 *			p4,
				 const uint8_t *	src,
				 unsigned int		offs)
{
	vu8 t1, t2, t3;

	src += offs;

	t1 = vload (src, -1 * (long) sizeof (vu8));
	t2 = vload (src,  0 * (long) sizeof (vu8));
	t3 = vload (src, +1 * (long) sizeof (vu8));

	*m4 = vec_perm (t1, t2, vec_lvsl (-4, src));
	*m2 = vec_perm (t1, t2, vec_lvsl (-2, src));
	*center = t2;
	*p2 = vec_perm (t2, t3, vec_lvsl (+2, src));
	*p4 = vec_perm (t2, t3, vec_lvsl (+4, src));
}

#else /* SIMD != CPU_FEATURE_ALTIVEC */

/* Defined here to enable parameter passing in registers.
   NOTE n_bytes must be a multiple of sizeof (vu8). */
static void
copy_line			(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		n_bytes)
{
	vu8 m0, m1, m2, m3;

	/* Copies 64 (128) bytes. */
	for (; n_bytes & -(sizeof (vu8) * 8); n_bytes -= sizeof (vu8) * 8) {
		m0 = vload (src, 0 * sizeof (vu8));
		m1 = vload (src, 1 * sizeof (vu8));
		m2 = vload (src, 2 * sizeof (vu8));
		m3 = vload (src, 3 * sizeof (vu8));

		vstorent (dst, 0 * sizeof (vu8), m0);
		vstorent (dst, 1 * sizeof (vu8), m1);
		vstorent (dst, 2 * sizeof (vu8), m2);
		vstorent (dst, 3 * sizeof (vu8), m3);

		m0 = vload (src, 4 * sizeof (vu8));
		m1 = vload (src, 5 * sizeof (vu8));
		m2 = vload (src, 6 * sizeof (vu8));
		m3 = vload (src, 7 * sizeof (vu8));
		src += 8 * sizeof (vu8);

		vstorent (dst, 4 * sizeof (vu8), m0);
		vstorent (dst, 5 * sizeof (vu8), m1);
		vstorent (dst, 6 * sizeof (vu8), m2);
		vstorent (dst, 7 * sizeof (vu8), m3);
		dst += 8 * sizeof (vu8);
	}

	/* Remaining bytes, usually not needed. */
	for (; n_bytes > 0; n_bytes -= sizeof (vu8)) {
		vstorent (dst, 0, vload (src, 0));
		src += sizeof (vu8);
		dst += sizeof (vu8);
	}
}

/* Copies src to dst and dst + dst_bpl.  Defined here to enable parameter
   passing in registers.  NOTE dst_bpl and n_bytes must be a multiple of
   sizeof (vu8). */
static void
copy_line_pair			(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		n_bytes,
				 unsigned int		dst_bpl)
{
	uint8_t *dst2;
	vu8 m0, m1, m2, m3;

	dst2 = dst + dst_bpl;

	for (; n_bytes & -(sizeof (vu8) * 4); n_bytes -= sizeof (vu8) * 4) {
		m0 = vload (src, 0 * sizeof (vu8));
		m1 = vload (src, 1 * sizeof (vu8));
		m2 = vload (src, 2 * sizeof (vu8));
		m3 = vload (src, 3 * sizeof (vu8));
		src += 4 * sizeof (vu8);

		vstorent (dst, 0 * sizeof (vu8), m0);
		vstorent (dst, 1 * sizeof (vu8), m1);
		vstorent (dst, 2 * sizeof (vu8), m2);
		vstorent (dst, 3 * sizeof (vu8), m3);
		dst += 4 * sizeof (vu8);

		vstorent (dst2, 0 * sizeof (vu8), m0);
		vstorent (dst2, 1 * sizeof (vu8), m1);
		vstorent (dst2, 2 * sizeof (vu8), m2);
		vstorent (dst2, 3 * sizeof (vu8), m3);
		dst2 += 4 * sizeof (vu8);
	}

	for (; n_bytes > 0; n_bytes -= sizeof (vu8)) {
		m0 = vload (src, 0);
		src += sizeof (vu8);
		vstorent (dst, 0, m0);
		vstorent (dst, dst_bpl, m0);
		dst += sizeof (vu8);
	}
}

#if SIMD == CPU_FEATURE_SSE2

#if 1

/* _mm_sxli_si128 is misdefined under gcc -O0.  Arg 2 must be an immediate,
   not a variable which evaluates to one, not even a const variable. */

#define uloadxt(_left, _center, _right, _src, _offs, _dist)		\
({									\
	vu8 *left = _left;						\
	vu8 *center = _center;						\
	vu8 *right = _right;						\
	const uint8_t *src = _src;					\
	const uint16_t *w;						\
	vu8 c;								\
	src += _offs;							\
									\
	c = vload (src, 0); /* ponmlkji */				\
	*center = c;							\
									\
	switch (_dist) {						\
	case 2:								\
		w = (const uint16_t *) src;				\
		*left = _mm_insert_epi16				\
			(_mm_slli_si128 (c, 2), w[-1], 0);		\
		*right = _mm_insert_epi16				\
			(_mm_srli_si128 (c, 2), w[8], 7);		\
		break;							\
									\
	case 8:								\
		c = _mm_shuffle_epi32 (c, _MM_SHUFFLE (1, 0, 3, 2));	\
		/* lkjihgfe = (hgfedcba, lkjiponm) */			\
		*left = _mm_unpackhi_epi64 (vload (src, -16), c);	\
		/* tsrqponm = (lkjiponm, xwvutsrq) */			\
		*right = _mm_unpacklo_epi64 (c, vload (src, +16));	\
		break;							\
									\
	default:							\
		*left = vor (_mm_slli_si128 (*center, _dist),		\
			     _mm_srli_si128 (vload (src, -16),		\
					     16 - (_dist)));		\
		*right = vor (_mm_srli_si128 (*center, _dist),		\
			      _mm_slli_si128 (vload (src, 16),		\
					      16 - (_dist)));		\
		break;							\
	}								\
})

#define uload24t(_m4, _m2, _center, _p2, _p4, _src, _offs)		\
({									\
	vu8 *m4 = _m4;							\
	vu8 *m2 = _m2;							\
	vu8 *center = _center;						\
	vu8 *p2 = _p2;							\
	vu8 *p4 = _p4;							\
	const uint8_t *src = _src;					\
	const uint16_t *w;						\
	vu8 t1;								\
									\
	src += _offs;							\
									\
	w = (const uint16_t *) src;					\
									\
	*center = vload (src, 0);					\
									\
	/* XXX we don't use *center because these instructions mix	\
	   with calculations, x86 has not enough registers to save	\
	   the center value for later and gcc can't easily avoid	\
	   register spilling.  XXX May not apply to x86-64. */		\
	t1 = _mm_slli_si128 (vload (src, 0), 2);			\
	t1 = _mm_insert_epi16 (t1, w[-1], 0);				\
	*m2 = t1;							\
	t1 = _mm_slli_si128 (t1, 2);					\
	*m4 = _mm_insert_epi16 (t1, w[-2], 0);				\
									\
	t1 = _mm_srli_si128 (vload (src, 0), 2);			\
	t1 = _mm_insert_epi16 (t1, w[8], 7);				\
	*p2 = t1;							\
	t1 = _mm_srli_si128 (t1, 2);					\
	*p4 = _mm_insert_epi16 (t1, w[9], 7);				\
})

#else /* XXX is this faster? */

static always_inline void
uloadxt				(vu8 *			left,
				 vu8 *			center,
				 vu8 *			right,
				 const uint8_t *	src,
				 unsigned int		offs,
				 unsigned int		dist)
{
	src += offs;

	*left   = vloadu (src, - (long) dist); /* movdqu */
	*center = vload  (src,   (long) 0);    /* movdqa */
	*right  = vloadu (src, + (long) dist);
}

static always_inline void
uload24t			(vu8 *			m4,
				 vu8 *			m2,
				 vu8 *			center,
				 vu8 *			p2,
				 vu8 *			p4,
				 const uint8_t *	src,
				 unsigned int		offs)
{
	src += offs;

	*m4     = vloadu (src, -4);
	*m2     = vloadu (src, -2);
	*center = vload  (src,  0);
	*p2     = vloadu (src, +2);
	*p4     = vloadu (src, +4);
}

#endif /* 0 */

#elif SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW | CPU_FEATURE_SSE)

static always_inline void
uloadxt				(vu8 *			left,
				 vu8 *			center,
				 vu8 *			right,
				 const uint8_t *	src,
				 unsigned int		offs,
				 unsigned int		dist)
{
	src += offs;

	/* MMX permits unaligned loads.  There's a few cycle penalty
	   when the load crosses a cache line boundary, but given 32 or 64
	   byte cache lines a shift-load probably doesn't pay.  That aside
	   we reduce register pressure. */
	*left   = vload (src, - (long) dist);
	*center = vload (src,   (long) 0);
	*right  = vload (src, + (long) dist);
}

static always_inline void
uload24t			(vu8 *			m4,
				 vu8 *			m2,
				 vu8 *			center,
				 vu8 *			p2,
				 vu8 *			p4,
				 const uint8_t *	src,
				 unsigned int		offs)
{
	src += offs;

	*m4     = vload (src, -4);
	*m2     = vload (src, -2);
	*center = vload (src,  0);
	*p2     = vload (src, +2);
	*p4     = vload (src, +4);
}

#endif /* SIMD switch */

#endif /* SIMD != CPU_FEATURE_ALTIVEC */

#define uload2t(_l, _c, _r, _s, _o) uloadxt (_l, _c, _r, _s, _o, 2)
#define uload4t(_l, _c, _r, _s, _o) uloadxt (_l, _c, _r, _s, _o, 4)
#define uload6t(_l, _c, _r, _s, _o) uloadxt (_l, _c, _r, _s, _o, 6)
#define uload8t(_l, _c, _r, _s, _o) uloadxt (_l, _c, _r, _s, _o, 8)

#else /* !SIMD */

static __inline__ void
copy_line			(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		n_bytes)
__attribute__ ((unused));

static __inline__ void
copy_line_pair			(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		n_bytes,
				 unsigned int		dst_bpl)
__attribute__ ((unused));

static __inline__ void
copy_line			(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		n_bytes)
{
	memcpy (dst, src, n_bytes);
}

static __inline__ void
copy_line_pair			(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		n_bytes,
				 unsigned int		dst_bpl)
{
	memcpy (dst, src, n_bytes);
	memcpy (dst + dst_bpl, src, n_bytes);
}

#endif /* !SIMD */

/* MSVC compatibility (will be removed after all the DScaler routines
   have been converted to vector intrinsics). */

#define _cdecl			/* what's that? */
#define __cdecl

/* __declspec(align(n)) -> __attribute__((aligned(n))) */
/* __declspec(dllexport) -> __attribute__(()) */
#define __declspec(x) __attribute__((x))
#define align(x) aligned(x)
#define dllexport

/* __asm emms; or __asm { emms } -> __asm__ (" emms\n") */
#define _asm
#define __asm
#define emms __asm__ __volatile__ (" emms\n");

#define __min MIN
#define __max MAX

#define WINAPI
#define APIENTRY

typedef int BOOL; /* for SETTINGS value pointer compatibility. */
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int UINT;
typedef int LONG;
typedef int DWORD;
typedef unsigned int ULONG;
typedef int32_t __int32;
typedef int64_t __int64;
typedef void *LPVOID;
typedef char *LPCSTR;
typedef void *HWND;
typedef void *HMODULE;
typedef void *HANDLE;
typedef void *HINSTANCE;

typedef struct {
} RECT;

#ifndef FALSE
#  define FALSE 0
#endif

#ifndef TRUE
#  define TRUE 1
#endif

#ifndef NULL
#  define NULL ((void *) 0)
#endif

#define WM_APP 0

/* DScaler helpids.h */
enum {
  IDH_2FRAME,
  IDH_ADAPTIVE,
  IDH_BOB,
  IDH_BLENDEDCLIP,
  IDH_EVEN,
  IDH_GREEDY,
  IDH_GREEDY2,
  IDH_GREEDYHM,
  IDH_MOCOMP2,
  IDH_ODD,
  IDH_OLD_GAME,
  IDH_SCALER_BOB,
  IDH_TOMSMOCOMP,
  IDH_VIDEOBOB,
  IDH_VIDEOWEAVE,
  IDH_WEAVE,
};

#if 0

/* For _save(), _restore() below. */
#define _saved_regs unsigned int saved_regs[6]

/* NOTE inline asm shall not use global mutables. Global consts are
   safe, but only by absolute address. %[name] would use ebx (GOT)
   relative addressing and inline asm usually overwrites ebx. Static
   consts must be referenced with _m() though, or they optimize away.
   Locals with _m(), ebp or esp relative, are safe. _m(name) is a
   named asm operand (gcc 3.1+ feature), such that inline asm can
   refer to locals by %[name] instead of numbers. */
#define _m(x) [x] "m" (x)
#define _m_array(x) [x] "m" (x[0])

/* Replaces  mov eax,local+3  by  mov eax,%[local3]  and  _m_nth(local,3) */
#define _m_nth(x, nth) [x##nth] "m" (((char *) &x)[nth])

/* Some "as" dislike type mixing, eg. cmp QWORD PTR [eax], 0 */
#define _m_int(x) [Int_##x] "m" (* (int *) &x)

/* NOTE Intel syntax - dest first. */
#define _save(x) " mov %[saved_" #x "]," #x "\n"
#define _restore(x) " mov " #x ",%[saved_" #x "]\n"

/* We use Intel syntax because the code was written for MSVC, noprefix
   because regs have no % prefix. ebx is the -fPIC GOT pointer. We cannot
   add ebx to the clobber list, must save & restore by hand. */
#define _asm_begin							\
  __asm__ __volatile__ (						\
  ".intel_syntax noprefix\n"						\
  _save(ebx)
#define _asm_end							\
  _restore(ebx)								\
  ".intel_syntax\n"							\
  ::									\
  [saved_eax] "m" (saved_regs[0]),					\
  [saved_ebx] "m" (saved_regs[1]),					\
  [saved_ecx] "m" (saved_regs[2]),					\
  [saved_edx] "m" (saved_regs[3]),					\
  [saved_esi] "m" (saved_regs[4]),					\
  [saved_edi] "m" (saved_regs[5])

/* Stringify _strf(FOO) -> "FOO" */
#define _strf1(x) #x
#define _strf(x) _strf1(x)

#endif

#endif /* WINDOWS_H */
