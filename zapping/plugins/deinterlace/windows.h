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

/* $Id: windows.h,v 1.6 2005-03-30 21:28:26 mschimek Exp $ */

#ifndef WINDOWS_H
#define WINDOWS_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stddef.h>		/* size_t */
#include <inttypes.h>		/* int64_t */
#include <assert.h>
#include "common/intl-priv.h"	/* i18n */
#include "libtv/cpu.h"		/* cpu_features */

#ifdef SIMD
#  include "libtv/simd.h"

static __inline__ v16
yuyv2yy				(vu8			yuyv)
{
	if (Z_BYTE_ORDER == Z_LITTLE_ENDIAN) {
		/* 0xVVY1UUY0 -> 0x00Y100Y0 */
		return vand ((v16) yuyv, vsplat16_255);
	} else {
		/* 0xY0UUY1VV -> 0x00Y000Y1 */
		return (v16) vsru16 ((vu16) yuyv, 8);
	}
}

#if SIMD == AVEC

/* Do an aligned load from s, and two unaligned loads from
    s + offs - dist and s + offs + dist.  Offs and dist are given
    in bytes, offs must be a multiple of sizeof (vu8). */
static __inline__ void
uloadxt				(vu8 *			left,
				 vu8 *			center,
				 vu8 *			right,
				 const vu8 *		s,
				 unsigned int		offs,
				 unsigned int		dist)
{
	vu8 t1, t2, t3;

	s += offs / sizeof (*s);

	t1 = s[-1];
	t2 = s[0];
	t3 = s[+1];

	*left = vec_perm (t1, t2, vec_lvsl (-dist, (uint8_t *) s));
	*center = t2;
	*right = vec_perm (t2, t3, vec_lvsl (+dist, (uint8_t *) s));
}

static __inline__ void
uload24t			(vu8 *			m4,
				 vu8 *			m2,
				 vu8 *			center,
				 vu8 *			p2,
				 vu8 *			p4,
				 const vu8 *		s,
				 unsigned int		offs)
{
	vu8 t1, t2, t3;

	s += offs / sizeof (*s);

	t1 = s[-1];
	t2 = s[0];
	t3 = s[+1];

	*m4 = vec_perm (t1, t2, vec_lvsl (-4, (uint8_t *) s));
	*m2 = vec_perm (t1, t2, vec_lvsl (-2, (uint8_t *) s));
	*center = t2;
	*p2 = vec_perm (t2, t3, vec_lvsl (+2, (uint8_t *) s));
	*p4 = vec_perm (t2, t3, vec_lvsl (+4, (uint8_t *) s));
}

#elif SIMD == SSE3

static __inline__ void
uloadxt				(vu8 *			left,
				 vu8 *			center,
				 vu8 *			right,
				 const vu8 *		s,
				 unsigned int		offs,
				 unsigned int		dist)
{
	s += offs / sizeof (*s);

	/* XXX is this faster? */
	*left   = _mm_lddqu_si128 ((const vu8 *)((const uint8_t *) s - dist));
	*center = *(const vu8 *)((uint8_t *) s); /* movdqu */
	*right  = _mm_lddqu_si128 ((const vu8 *)((const uint8_t *) s + dist));
}

#elif SIMD == SSE2

#if 1

static __inline__ void
uloadxt				(vu8 *			left,
				 vu8 *			center,
				 vu8 *			right,
				 const vu8 *		s,
				 unsigned int		offs,
				 unsigned int		dist)
{
	const uint16_t *w;
	const uint32_t *d;
	const __m64 *q;
	vu8 m1, m2, m3;

	s += offs / sizeof (*s);

	w = (const uint16_t *) s;
	d = (const uint32_t *) s;
	q = (const __m64 *) s;

	m1 = s[0]; /* ponmlkji */
	*center = m1;

	switch (dist) {
	case 2:
		*left = _mm_insert_epi16 (_mm_slli_si128 (m1, 2), w[-1], 0);
		*right = _mm_insert_epi16 (_mm_srli_si128 (m1, 2), w[8], 7);
		break;

	case 4:
		m2 = _mm_cvtsi32_si128 (d[-1]); /* movd */
		*left = vor (_mm_slli_si128 (m1, 4), m2);
		m2 = _mm_slli_si128 (_mm_cvtsi32_si128 (d[4]), 12);
		*right = vor (_mm_srli_si128 (m1, 4), m2);
		break;

	case 8:
		m2 = _mm_set_epi64 ((__m64) 0LL, q[-1]); /* 0000hgfe movq */
		m3 = _mm_set_epi64 ((__m64) 0LL, q[2]);  /* 0000tsrq */
		m2 = _mm_unpacklo_epi32 (m2, m3);        /* tsrqhgfe */
		*left = _mm_unpacklo_epi32 (m2, m1);     /* lkjihgfe */
		*right = _mm_unpackhi_epi32 (m1, m2);    /* tsrqponm */
		break;

	default:
		*left = vor (_mm_slli_si128 (m1, dist),
			     _mm_srli_si128 (s[-1], 16 - dist));
		*right = vor (_mm_srli_si128 (m1, dist),
			      _mm_slli_si128 (s[1], 16 - dist));
		break;
	}
}

static __inline__ void
uload24t			(vu8 *			m4,
				 vu8 *			m2,
				 vu8 *			center,
				 vu8 *			p2,
				 vu8 *			p4,
				 const vu8 *		s,
				 unsigned int		offs)
{
	const uint16_t *w;
	vu8 t1;

	s += offs / sizeof (*s);

	w = (const uint16_t *) s;

	/* FIXME */
	*center = s[0];
	t1 = _mm_slli_si128 (s[0], 2);
	*m2 = _mm_insert_epi16 (t1, w[-1], 0);
	t1 = _mm_slli_si128 (t1, 2);
	*m4 = _mm_insert_epi16 (t1, w[-2], 0);
	t1 = _mm_srli_si128 (s[0], 2);
	*p2 = _mm_insert_epi16 (t1, w[8], 7);
	t1 = _mm_srli_si128 (t1, 2);
	*p4 = _mm_insert_epi16 (t1, w[9], 7);
}

#else /* XXX is this faster? */

static __inline__ void
uloadxt				(vu8 *			left,
				 vu8 *			center,
				 vu8 *			right,
				 const vu8 *		s,
				 unsigned int		offs,
				 unsigned int		dist)
{
	s += offs / sizeof (*s);

	*left   = _mm_loadu_ps ((const vu8 *)((const uint8_t *) s - dist));
	*center = *(const vu8 *)((const uint8_t *) s); /* movdqu */
	*right  = _mm_loadu_ps ((const vu8 *)((const uint8_t *) s + dist));
}

#endif

#elif SIMD == MMX || SIMD == _3DNOW || SIMD == SSE

static __inline__ void
uloadxt				(vu8 *			left,
				 vu8 *			center,
				 vu8 *			right,
				 const vu8 *		s,
				 unsigned int		offs,
				 unsigned int		dist)
{
	s += offs / sizeof (*s);

	/* MMX permits unaligned loads.  There's a few cycle penalty
	   when the load crosses a cache line boundary, but given 32 or 64
	   byte cache lines a shift-load probably doesn't pay. */
	*left   = *(const vu8 *)((const uint8_t *) s - dist);
	*center = *(const vu8 *)((const uint8_t *) s);
	*right  = *(const vu8 *)((const uint8_t *) s + dist);
}

static __inline__ void
uload24t			(vu8 *			m4,
				 vu8 *			m2,
				 vu8 *			center,
				 vu8 *			p2,
				 vu8 *			p4,
				 const vu8 *		s,
				 unsigned int		offs)
{
	s += offs / sizeof (*s);

	*m4     = *(const vu8 *)((const uint8_t *) s - 4);
	*m2     = *(const vu8 *)((const uint8_t *) s - 2);
	*center = *(const vu8 *)((const uint8_t *) s);
	*p2     = *(const vu8 *)((const uint8_t *) s + 2);
	*p4     = *(const vu8 *)((const uint8_t *) s + 4);
}

#endif /* uloadxt */

#define uload2t(_l, _c, _r, _s, _o) uloadxt (_l, _c, _r, _s, _o, 2)
#define uload4t(_l, _c, _r, _s, _o) uloadxt (_l, _c, _r, _s, _o, 4)
#define uload6t(_l, _c, _r, _s, _o) uloadxt (_l, _c, _r, _s, _o, 6)
#define uload8t(_l, _c, _r, _s, _o) uloadxt (_l, _c, _r, _s, _o, 8)

/* Defined here to enable register parameters.
   NOTE n_bytes must be a multiple of sizeof (vu8). */
static void
copy_line			(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		n_bytes)
{
    vu8 *d = (vu8 *) dst;
    const vu8 *s = (const vu8 *) src;
    vu8 m0, m1, m2, m3;

    for (; n_bytes & -(sizeof (vu8) * 8); n_bytes -= sizeof (vu8) * 8) {
	m0 = s[0];
	m1 = s[1];
	m2 = s[2];
	m3 = s[3];
	vstorent (d + 0, m0);
	vstorent (d + 1, m1);
	vstorent (d + 2, m2);
	vstorent (d + 3, m3);
	m0 = s[4];
	m1 = s[5];
	m2 = s[6];
	m3 = s[7];
	s += 8;
	vstorent (d + 4, m0);
	vstorent (d + 5, m1);
	vstorent (d + 6, m2);
	vstorent (d + 7, m3);
	d += 8;
    }

    for (; n_bytes > 0; n_bytes -= sizeof (vu8)) {
	vstorent (d, *s++);
	++d;
    }
}

/* Copies src to dst and dst + dst_bpl.  Defined here to enable register
   parameters.  NOTE dst_bpl and n_bytes must be a multiple of sizeof (vu8). */
static void
copy_line_pair			(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		n_bytes,
				 unsigned int		dst_bpl)
{
    vu8 *d = (vu8 *) dst;
    vu8 *d2;
    const vu8 *s = (const vu8 *) src;
    vu8 m0, m1, m2, m3;

    d2 = (vu8 *)(dst + dst_bpl);

    for (; n_bytes & -(sizeof (vu8) * 8); n_bytes -= sizeof (vu8) * 8) {
	m0 = s[0];
	m1 = s[1];
	m2 = s[2];
	m3 = s[3];
	vstorent (d + 0, m0);
	vstorent (d + 1, m1);
	vstorent (d + 2, m2);
	vstorent (d + 3, m3);
	vstorent (d2 + 0, m0);
	vstorent (d2 + 1, m1);
	vstorent (d2 + 2, m2);
	vstorent (d2 + 3, m3);
	m0 = s[4];
	m1 = s[5];
	m2 = s[6];
	m3 = s[7];
	s += 8;
	vstorent (d + 4, m0);
	vstorent (d + 5, m1);
	vstorent (d + 6, m2);
	vstorent (d + 7, m3);
	d += 8;
	vstorent (d2 + 4, m0);
	vstorent (d2 + 5, m1);
	vstorent (d2 + 6, m2);
	vstorent (d2 + 7, m3);
	d2 += 8;
    }

    for (; n_bytes > 0; n_bytes -= sizeof (vu8)) {
	m0 = *s++;
	vstorent (d, m0);
	vstorent ((vu8 *)((uint8_t *) d + dst_bpl), m0);
	++d;
    }
}

#endif /* defined (SIMD) */

/* Helpers to switch between deinterlace implementations. */

#define SIMD_PROTOS(name)						\
extern BOOL name ## __MMX (TDeinterlaceInfo* pInfo);			\
extern BOOL name ## __3DNOW (TDeinterlaceInfo* pInfo);			\
extern BOOL name ## __SSE (TDeinterlaceInfo* pInfo);			\
extern BOOL name ## __SSE2 (TDeinterlaceInfo* pInfo);			\
extern BOOL name ## __AVEC (TDeinterlaceInfo* pInfo);

#if defined (HAVE_MMX)
#  define SIMD_SELECT_MMX(name)						\
	(cpu_features & CPU_FEATURE_MMX) ? name ## __MMX
#else
#  define SIMD_SELECT_MMX(name) 0 ? NULL
#endif

/* Untested */
#if 0 && defined (HAVE_3DNOW)
#  define SIMD_SELECT_3DNOW(name)					\
	(cpu_features & CPU_FEATURE_3DNOW) ? name ## __3DNOW
#else
#  define SIMD_SELECT_3DNOW(name) 0 ? NULL
#endif

#if defined (HAVE_SSE)
#  define SIMD_SELECT_SSE(name)						\
	(cpu_features & CPU_FEATURE_SSE) ? name ## __SSE
#else
#  define SIMD_SELECT_SSE(name) 0 ? NULL
#endif

/* Untested */
#if 0 && defined (HAVE_SSE2)
#  define SIMD_SELECT_SSE2(name)					\
	(cpu_features & CPU_FEATURE_SSE2) ? name ## __SSE2
#else
#  define SIMD_SELECT_SSE2(name) 0 ? NULL
#endif

/* Untested */
#if 0 && defined (HAVE_ALTIVEC)
#  define SIMD_SELECT_ALTIVEC(name)					\
	(cpu_features & CPU_FEATURE_ALTIVEC) ? name ## __AVEC
#else
#  define SIMD_SELECT_ALTIVEC(name) 0 ? NULL
#endif

#define SIMD_SELECT(name)						\
	(SIMD_SELECT_ALTIVEC (name) :					\
	 SIMD_SELECT_SSE2 (name) :					\
	 SIMD_SELECT_SSE (name)	:					\
	 SIMD_SELECT_3DNOW (name) :					\
	 SIMD_SELECT_MMX (name) :					\
	 (assert (0), NULL))

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

#define __min(x, y) ({							\
  __typeof__ (x) _x = (x);						\
  __typeof__ (y) _y = (y);						\
  (_x < _y) ? _x : _y;							\
})

#define __max(x, y) ({							\
  __typeof__ (x) _x = (x);						\
  __typeof__ (y) _y = (y);						\
  (_x > _y) ? _x : _y;							\
})

#define WINAPI
#define APIENTRY

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int UINT;
typedef long LONG;
typedef long DWORD;
typedef unsigned long ULONG;
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

#endif /* WINDOWS_H */
