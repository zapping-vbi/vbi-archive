/*
 *  Copyright (C) 2005-2006 Michael H. Schimek
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

/* $Id: simd-conv.h,v 1.3 2006-03-11 13:13:24 mschimek Exp $ */

#include <assert.h>
#include "pixel_format.h"
#include "misc.h"
#include "simd.h"
#include "lut_yuv2rgb.h"

#if SIMD

#if SIMD == CPU_FEATURE_ALTIVEC

/* TO DO */

#else

#if SIMD & ~CPU_FEATURE_MMX
#  warning only MMX tested
#endif

/* out[i] = in[i * dist % sizeof (in) + i * dist / sizeof (in)] */
static always_inline void
interleave			(vu8 *			o0,
				 vu8 *			o1,
				 vu8			i0,
				 vu8			i1,
				 unsigned int		dist)
{
	vu8 t;

	if (SIMD == CPU_FEATURE_SSE2) {
		/* Another round because the vector is twice as wide. */

		t = i0;
		i0 = vunpacklo8 (t, i1);
		i1 = vunpackhi8 (t, i1);
	}

	switch (dist) {
	case 2:
		t = i0;
		i0 = vunpacklo8 (t, i1);
		i1 = vunpackhi8 (t, i1);

		/* fall through */

	case 4:
		t = i0;
		i0 = vunpacklo8 (t, i1);
		i1 = vunpackhi8 (t, i1);

		/* fall through */

	case 8:
		*o0 = vunpacklo8 (i0, i1);
		*o1 = vunpackhi8 (i0, i1);

		break;

	default:
		assert (0);
	}
}

static always_inline vu8
load_lo				(const uint8_t *	src)
{
#if SIMD == CPU_FEATURE_SSE2
	/* movq to xmm */
	return _mm_set_epi64 ((__m64) 0LL, * (const __m64 *) src);
#else
	/* movd to mmx */
	return _mm_cvtsi32_si64 (* (const uint32_t *) src);
#endif
}

static always_inline void
load_16				(v16 *			even,
				 v16 *			odd,
				 const uint8_t *	src,
				 unsigned long		offset)
{
	*even = vload (src, offset);
	*odd  = vsru16 ((vu16) *even, 8);
	*even = vand (*even, vsplat16_255);
}

static always_inline void
load_yuyv8			(vu8 *			y0,
				 vu8 *			y1,
				 vu8 *			u,
				 vu8 *			v,
				 const uint8_t *	src,
				 unsigned long		offset,
				 tv_pixfmt		pixfmt)
{
	vu8 yeu0, yov0, yeu1, yov1;

	interleave (&yeu0, &yov0,
		    vload (src, offset),
		    vload (src, offset + 1 * sizeof (vu8)), 4);

	interleave (&yeu1, &yov1,
		    vload (src, offset + 2 * sizeof (vu8)),
		    vload (src, offset + 3 * sizeof (vu8)), 4);

	switch (pixfmt) {
	case TV_PIXFMT_YVYU:
		SWAP (u, v);

		/* fall through */

	case TV_PIXFMT_YUYV:
		*y0 = vunpacklo8 (yeu0, yov0);
		*y1 = vunpacklo8 (yeu1, yov1);
		*u = vunpackhi (yeu0, yeu1);
		*v = vunpackhi (yov0, yov1);
		break;

	case TV_PIXFMT_VYUY:
		SWAP (u, v);

		/* fall through */

	case TV_PIXFMT_UYVY:
		*y0 = vunpackhi8 (yeu0, yov0);
		*y1 = vunpackhi8 (yeu1, yov1);
		*u = vunpacklo (yeu0, yeu1);
		*v = vunpacklo (yov0, yov1);
		break;

	default:
		assert (0);
	}
}

static always_inline void
load_yuyv16			(vu16 *			ye,
				 vu16 *			yo,
				 vu16 *			u,
				 vu16 *			v,
				 const uint8_t *	src,
				 unsigned long		offset,
				 tv_pixfmt		pixfmt)
{
	vu8 yeu, yov;

	interleave (&yeu, &yov,
		    vload (src, offset),
		    vload (src, offset + sizeof (vu8)), 4);

	switch (pixfmt) {
	case TV_PIXFMT_YVYU:
		SWAP (u, v);

		/* fall through */

	case TV_PIXFMT_YUYV:
		*ye = vunpacklo8 (yeu, vzerou8 ());
		*yo = vunpacklo8 (yov, vzerou8 ());
		*u = vunpackhi8 (yeu, vzerou8 ());
		*v = vunpackhi8 (yov, vzerou8 ());
		break;

	case TV_PIXFMT_VYUY:
		SWAP (u, v);

		/* fall through */

	case TV_PIXFMT_UYVY:
		*ye = vunpackhi8 (yeu, vzerou8 ());
		*yo = vunpackhi8 (yov, vzerou8 ());
		*u = vunpacklo8 (yeu, vzerou8 ());
		*v = vunpacklo8 (yov, vzerou8 ());
		break;

	default:
		assert (0);
	}
}

static always_inline void
load_rgb16			(vu16 *			r,
				 vu16 *			g,
				 vu16 *			b,
				 const uint8_t *	src,
				 unsigned long		offset,
				 tv_pixfmt		pixfmt)
{
	vu8 t0, t1;

	switch (pixfmt) {
	case TV_PIXFMT_BGRA32_LE:
	case TV_PIXFMT_BGRA32_BE:
		SWAP (r, b);

		/* fall through */

	case TV_PIXFMT_RGBA32_LE: /* .. A0 B0 G0 R0 */
	case TV_PIXFMT_RGBA32_BE: /* .. R0 G0 B0 A0 */
		interleave (&t0, &t1,
			    vload (src, offset),
			    vload (src, offset + sizeof (vu8)), 4);

		if (TV_PIXFMT_RGBA32_BE == pixfmt) {
			*r = vunpacklo16 (t0, vzero8 ());
			*g = vunpackhi16 (t0, vzero8 ());
			*b = vunpacklo16 (t1, vzero8 ());
		} else {
			*r = vunpackhi16 (t1, vzero8 ());
			*g = vunpacklo16 (t1, vzero8 ());
			*b = vunpackhi16 (t0, vzero8 ());
		}

		break;

	case TV_PIXFMT_RGB24_LE:
		SWAP (r, b);

		/* fall through */

	case TV_PIXFMT_RGB24_BE: /* .. R0 G0 B0 */
		/* TODO */
	case TV_PIXFMT_BGR16_LE: /* rrrrrggg gggbbbbb */
		/* TODO */
	case TV_PIXFMT_BGRA16_LE: /* arrrrrgg gggbbbbb */
		/* TODO */
	default:
		assert (0);
	}
}

/* Stores 8/16 RGB pixels at dst + offset, given
   rg = (GE-RE GC-RC GA-RA G8-R8) G6-R6 G4-R4 G2-R2 G0-R0
   gb = (BF-GF BD-GD BB-GB B9-G9) B7-G7 B5-G5 B3-G3 B1-G1
   br = (RF-BE RD-BC RB-BA R9-B8) R7-B6 R5-B4 R3-B2 R1-B0 */
static __inline__ void
store_rggbbr			(uint8_t *		dst,
				 unsigned long		offset,
				 vu8			rg,
				 vu8			gb,
				 vu8			br)
{
#if SIMD == CPU_FEATURE_SSE2

	vu8 t0;

	t0 = vunpacklo16 (rg, br);
	/* R7-B6 G6-R6 R5-B4-G4-R4 R3-B2 G2-R2 R1-B0-G0-R0 */
	t0 = _mm_shufflelo_epi16 (t0, _MM_SHUFFLE (1, 0, 3, 2));
	/* R7-B6 G6-R6 R5-B4-G4-R4 R1-B0-G0-R0 R3-B2 G2-R2 */ 
	br = vunpackhi16 (br, gb);
	/* BF-GF-RF-BE BD-GD RD-BC BB-GB-RB-BA B9-G9 R9-B8 */
	gb = vunpackhi32 (vsl (gb, 64), t0);
	/* R7-B6 G6-R6 B7-G7 B5-G5 R5-B4-G4-R4 B3-G3 B1-G1 */
	gb = _mm_shufflehi_epi16 (gb, _MM_SHUFFLE (1, 3, 2, 0));
	/* B7-G7-R7-B6-G6-R6-B5-G5 R5-B4-G4-R4 B3-G3 B1-G1 */
	rg = vunpacklo32 (vsru (rg, 64), br);
	/* BB-GB-RB-BA GE-RE GC-RC B9-G9 R9-B8 GA-RA G8-R8 */
	rg = _mm_shufflelo_epi16 (rg, _MM_SHUFFLE (1, 3, 2, 0));
	/* BB-GB-RB-BA GE-RE GC-RC GA-RA-B9-G9-R9-B8-G8-R8 */
	t0 = vunpacklo32 (t0, gb);
	/* R5-B4-G4-R4 R1-B0-G0-R0 B3-G3 B1-G1 R3-B2 G2-R2 */
	t0 = _mm_shufflelo_epi16 (t0, _MM_SHUFFLE (3, 1, 0, 2));
	/* R5-B4-G4-R4 R1-B0-G0-R0 B3-G3-R3-B2 G2-R2-B1-G1 */
	vstorent (dst, offset,
		  _mm_shuffle_epi32 (t0, _MM_SHUFFLE (3, 1, 0, 2)));
	/* R5-B4-G4-R4-B3-G3-R3-B2-G2-R2-B1-G1-R1-B0-G0-R0 */
	vstorent (dst, offset + 16,
		  vunpacklo32 (vsru (gb, 64), rg));
	/* GA-RA-B9-G9-R9-B8-G8-R8-B7-G7-R7-B6-G6-R6-B5-G5 */
	rg = vunpackhi32 (rg, br);
	/* BF-GF-RF-BE BB-GB-RB-BA BD-GD RD-BC GE-RE GC-RC */
	rg = _mm_shufflelo_epi16 (rg, _MM_SHUFFLE (1, 3, 2, 0));
	/* BF-GF-RF-BE BB-GB-RB-BA GE-RE-BD-GD RD-BC-GC-RC */
	vstorent (dst, offset + 32,
		  _mm_shuffle_epi32 (rg, _MM_SHUFFLE (3, 1, 0, 2)));
	/* BF-GF-RF-BE-GE-RE-BD-GD-RD-BC-GC-RC-BB-GB-RB-BA */

#elif SIMD == CPU_FEATURE_SSE

	vu8 t0;

	/* Saves a shift and two moves. */

	t0 = vsru (rg, 32);			/* 00 00 00 00 G6-R6 G4-R4 */
	rg = vunpacklo16 (rg, br);		/* R3-B2 G2-R2 R1-B0-G0-R0 */
	br = vunpackhi16 (br, gb);		/* B7-G7-R7-B6 B5-G5 R5-B4 */
	gb = vsl (gb, 32);			/* B3-G3 B1-G1 00 00 00 00 */
	gb = vunpackhi16 (gb, rg);		/* R3-B2 B3-G3 G2-R2-B1-G1 */
	vstorent (dst, offset, vunpacklo32 (rg, gb));
						/* G2-R2-B1-G1-R1-B0-G0-R0 */
        gb = _mm_shuffle_pi16 (gb, _MM_SHUFFLE (1, 0, 2, 3));
						/* G2-R2-B1-G1 B3-G3-R3-B2 */
	t0 = vunpacklo16 (t0, br);		/* B5-G5 G6-R6 R5-B4-G4-R4 */
	t0 = _mm_shuffle_pi16 (t0, _MM_SHUFFLE (2, 3, 1, 0));
						/* G6-R6-B5-G5 R5-B4-G4-R4 */
	vstorent (dst, offset + 8, vunpacklo32 (gb, t0));
						/* R5-B4-G4-R4-B3-G3-R3-B2 */
	vstorent (dst, offset + 16, vunpackhi32 (t0, br));
						/* B7-G7-R7-B6-G6-R6-B5-G5 */

#elif SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW)

	vu8 t0, t1;

	t0 = vsru (rg, 32);			/* 00 00 00 00 G6-R6 G4-R4 */
	rg = vunpacklo16 (rg, br);		/* R3-B2 G2-R2 R1-B0-G0-R0 */
	br = vunpackhi16 (br, gb);		/* B7-G7-R7-B6 B5-G5 R5-B4 */
	gb = vsl (gb, 32);			/* B3-G3 B1-G1 00 00 00 00 */
        t1 = vunpackhi16 (gb, rg);		/* R3-B2 B3-G3 G2-R2-B1-G1 */
	vstorent (dst, offset, vunpacklo32 (rg, t1));
						/* G2-R2-B1-G1-R1-B0-G0-R0 */
        t1 = vunpacklo16 (br, t0);		/* G6-R6-B5-G5 G4-R4 R5-B4 */
        vstorent (dst, offset + 16, vunpackhi32 (t1, br));
						/* B7-G7-R7-B6-G6-R6-B5-G5 */
        rg = vunpackhi16 (rg, gb);		/* B3-G3-R3-B2 B1-G1 G2-R2 */
        t0 = vunpacklo16 (t0, br);		/* B5-G5 G6-R6 R5-B4-G4-R4 */
        t0 = vsl (t0, 32);			/* R5-B4-G4-R4 00 00 00 00 */
        vstorent (dst, offset + 8, vunpackhi32 (rg, t0));
						/* R5-B4-G4-R4-B3-G3-R3-B2 */

#endif

}

/* Stores 8/16 RGB pixels at dst + offset in the given pixfmt.
   saturate: saturate values outside 0 ... 255.
   re: (RE RC RA R8) R6 R4 R2 R0
   ro: (RF RD RB R9) R7 R5 R3 R1
   etc */
static always_inline void
store_rgb16			(void *			dst,
				 unsigned long		offset,
				 tv_pixfmt		pixfmt,
				 tv_bool		saturate,
				 v16			re,
				 v16			ro,
				 v16			ge,
				 v16			go,
				 v16			be,
				 v16			bo)
{
	switch (pixfmt) {
	case TV_PIXFMT_BGRA32_LE:
	case TV_PIXFMT_BGRA32_BE:
		SWAP (re, be); /* optimizes away */
		SWAP (ro, bo);

		/* fall through */

	case TV_PIXFMT_RGBA32_LE:
	case TV_PIXFMT_RGBA32_BE:
		if (saturate) {
			vu8 r, g, b, a;
			vu8 rge, rgo, bae, bao;
			vu8 rgl, rgh, bal, bah;

			r = vpacksu16 (re, ro);	/* RF/R7 .. R1 RE/R6 .. R0 */
			g = vpacksu16 (ge, go);
			b = vpacksu16 (be, bo);
			a = vsplat8_m1;

			if (TV_PIXFMT_BGRA32_BE == pixfmt
			    || TV_PIXFMT_RGBA32_BE == pixfmt) {
				SWAP (r, a);
				SWAP (g, b);
			}

			rge = vunpacklo8 (r, g); /* .. G2 R2 G0 R0 */
			rgo = vunpackhi8 (r, g);
			rgl = vunpacklo16 (rge, rgo); /* .. G1 R1 G0 R0 */
			rgh = vunpackhi16 (rge, rgo);

			bae = vunpacklo8 (b, a);
			bao = vunpackhi8 (b, a);
			bal = vunpacklo16 (bae, bao); /* .. FF B1 FF B0 */
			bah = vunpackhi16 (bae, bao);

			/* .. FF B0 G0 R0 */
			vstorent (dst, offset + 0 * sizeof (vu8),
				  vunpacklo16 (rgl, bal));
			vstorent (dst, offset + 1 * sizeof (vu8),
				  vunpackhi16 (rgl, bal));
			vstorent (dst, offset + 2 * sizeof (vu8),
				  vunpacklo16 (rgh, bah));
			vstorent (dst, offset + 3 * sizeof (vu8),
				  vunpackhi16 (rgh, bah));
		} else {
			vu8 rge, rgo, bae, bao;
			vu8 rgl, rgh, bal, bah;

			/* Already saturated. */

			if (TV_PIXFMT_BGRA32_BE == pixfmt
			    || TV_PIXFMT_RGBA32_BE == pixfmt) {
				vu8 a;

				SWAP (re, ge);
				SWAP (ro, go);

				rge = vor (re, vsl16 (ge, 8)); /* .. G0 R0 */
				rgo = vor (ro, vsl16 (go, 8));

				a = vsplat16_255;

				bae = vor (vsl16 (be, 8), a); /* .. B0 FF */
				bao = vor (vsl16 (bo, 8), a);

				SWAP (rge, bae);
				SWAP (rgo, bao);
			} else {
				vu8 a;

				rge = vor (re, vsl16 (ge, 8));
				rgo = vor (ro, vsl16 (go, 8));

				a = vsplat16_m256;
	
				bae = vor (be, a);
				bao = vor (bo, a);
			}

			rgl = vunpacklo16 (rge, rgo); /* .. G1 R1 G0 R0 */
			rgh = vunpackhi16 (rge, rgo);
			bal = vunpacklo16 (bae, bao); /* .. FF B1 FF B0 */
			bah = vunpackhi16 (bae, bao);

			/* .. FF B0 G0 R0 */
			vstorent (dst, offset + 0 * sizeof (vu8),
				  vunpacklo16 (rgl, bal));
			vstorent (dst, offset + 1 * sizeof (vu8),
				  vunpackhi16 (rgl, bal));
			vstorent (dst, offset + 2 * sizeof (vu8),
				  vunpacklo16 (rgh, bah));
			vstorent (dst, offset + 3 * sizeof (vu8),
				  vunpackhi16 (rgh, bah));
		}

		break;

	case TV_PIXFMT_BGR24_LE:
		SWAP (re, be);
		SWAP (ro, bo);

		/* fall through */

	case TV_PIXFMT_RGB24_LE:
		if (saturate) {
			vu8 r, g, b, t;

			r = vpacksu16 (re, ro); /* RF/R7 .. R1 RE/R6 .. R0 */
			g = vpacksu16 (ge, go);
			b = vpacksu16 (be, bo);

			t = vsru (r, sizeof (vu8) * 4);
			/* 00 .. 00 (RF RD RB R9) R7 R5 R3 R1 */
			r = vunpacklo8 (r, g);
			/* (GE-RE GC-RC GA-RA G8-R8) G6-R6 G4-R4 G2-R2 G0-R0 */
			g = vunpackhi8 (g, b);
			/* (BF-GF BD-GD BB-GB B9-G9) B7-G7 B5-G5 B3-G3 B1-G1 */
			b = vunpacklo8 (b, t);
			/* (RF-BE RD-BC RB-BA R9-B8) R7-B6 R5-B4 R3-B2 R1-B0 */

			store_rggbbr (dst, offset, r, g, b);
		} else {
			re = vor (re, vsl16 (ge, 8));
			/* (GE-RE GC-RC GA-RA G8-R8) G6-R6 G4-R4 G2-R2 G0-R0 */
			go = vor (go, vsl16 (bo, 8));
			/* (BF-GF BD-GD BB-GB B9-G9) B7-G7 B5-G5 B3-G3 B1-G1 */
			be = vor (be, vsl16 (ro, 8));
			/* (RF-BE RD-BC RB-BA R9-B8) R7-B6 R5-B4 R3-B2 R1-B0 */

			store_rggbbr (dst, offset, re, go, be);
		}

		break;

	case TV_PIXFMT_RGB16_LE:
		SWAP (re, be);
		SWAP (ro, bo);

		/* fall through */

	case TV_PIXFMT_BGR16_LE: /* rrrrrggg gggbbbbb */
		if (saturate) {
			/* NB there's no vsru8(). */
			re = vand (vpacksu16 (re, ro), vsplatu8_F8);
			be = vsru (vand (vpacksu16 (be, bo), vsplatu8_F8), 3);
			ge = vand (vpacksu16 (ge, go), vsplatu8_FC);

			bo = vunpackhi8 (be, re);	/* rrrrr000 000bbbbb */
			be = vunpacklo8 (be, re);

			go = vunpackhi8 (ge, vzerou8 ());
			bo = vor (bo, vsl16 (go, 3));

			ge = vunpacklo8 (ge, vzerou8 ());
			be = vor (be, vsl16 (ge, 3));

			vstorent (dst, offset, vunpacklo16 (be, bo));
			vstorent (dst, offset + sizeof (vu8),
				  vunpackhi16 (be, bo));
		} else {
			re = vsl16 (vand (re, vsplatu16_F8), 8);
			re = vor (re, vsru16 (be, 3));
			re = vor (re, vsl16 (vsru16 (ge, 2), 3 + 2));

			ro = vsl16 (vand (ro, vsplatu16_F8), 8);
			ro = vor (ro, vsru16 (bo, 3));
			ro = vor (ro, vsl16 (vsru16 (go, 2), 3 + 2));

			vstorent (dst, offset, vunpacklo16 (re, ro));
			vstorent (dst, offset + sizeof (vu8),
				  vunpackhi16 (re, ro));
		}

		break;

	case TV_PIXFMT_RGB16_BE:
		SWAP (re, be);
		SWAP (ro, bo);

		/* fall through */

	case TV_PIXFMT_BGR16_BE:/* gggbbbbb rrrrrggg */
		if (saturate) {
			re = vand (vpacksu16 (re, ro), vsplatu8_F8);
			be = vsru (vand (vpacksu16 (be, bo), vsplatu8_F8), 3);
			ge = vand (vpacksu16 (ge, go), vsplatu8_FC);

			ro = vunpackhi8 (re, be);	/* 000bbbbb rrrrr000 */
			re = vunpacklo8 (re, be);

			go = vunpackhi8 (ge, vzerou8 ());
			ro = vor (ro, vsru16 (go, 5));
			ro = vor (ro, vsl16 (go, 16 - 5));

			ge = vunpacklo8 (ge, vzerou8 ());
			re = vor (re, vsru16 (ge, 5));
			re = vor (re, vsl16 (ge, 16 - 5));

			vstorent (dst, offset, vunpacklo16 (re, ro));
			vstorent (dst, offset + sizeof (vu8),
				  vunpackhi16 (re, ro));
		} else {
			re = vand (re, vsplatu16_F8);
			re = vor (re, vsl16 (vand (be, vsplatu16_F8), 5));
			re = vor (re, vsru16 (ge, 5));
			re = vor (re, vsl16 (vsru16 (ge, 2), 8 + 5));

			ro = vand (ro, vsplatu16_F8);
			ro = vor (ro, vsl16 (vand (bo, vsplatu16_F8), 5));
			ro = vor (ro, vsru16 (go, 5));
			ro = vor (ro, vsl16 (vsru16 (go, 2), 8 + 5));

			vstorent (dst, offset, vunpacklo16 (re, ro));
			vstorent (dst, offset + sizeof (vu8),
				  vunpackhi16 (re, ro));
		}

		break;

	case TV_PIXFMT_RGBA16_LE:
		SWAP (re, be);
		SWAP (ro, bo);

		/* fall through */

	case TV_PIXFMT_BGRA16_LE: /* arrrrrgg gggbbbbb */
		if (saturate) {
			vu8 _20 = vsplatu8 (0x20);

			re = vsru (vand (vpacksu16 (re, ro), vsplatu8_F8), 1);
			be = vsru (vand (vpacksu16 (be, bo), vsplatu8_F8), 3);
			ge = vand (vpacksu16 (ge, go), vsplatu8_F8);

			bo = vunpackhi8 (be, re);	/* 0rrrrr00 000bbbbb */
			be = vunpacklo8 (be, re);

			bo = vor (bo, vsl16 (vunpackhi8 (ge, _20), 2));
			be = vor (be, vsl16 (vunpacklo8 (ge, _20), 2));

			vstorent (dst, offset, vunpacklo16 (be, bo));
			vstorent (dst, offset + sizeof (vu8),
				  vunpackhi16 (be, bo));
		} else {
			re = vor (re, vsplatu16_256);
			re = vsl16 (vsru16 (re, 3), 7 + 3);
			re = vor (re, vsru16 (be, 3));
			re = vor (re, vsl (vand (ge, vsplatu16_F8), 2));

			ro = vor (ro, vsplatu16_256);
			ro = vsl16 (vsru16 (ro, 3), 7 + 3);
			ro = vor (ro, vsru16 (bo, 3));
			ro = vor (ro, vsl (vand (go, vsplatu16_F8), 2));

			vstorent (dst, offset, vunpacklo16 (re, ro));
			vstorent (dst, offset + sizeof (vu8),
				  vunpackhi16 (re, ro));
		}

		break;

	case TV_PIXFMT_RGBA16_BE:
		SWAP (re, be);
		SWAP (ro, bo);

		/* fall through */

	case TV_PIXFMT_BGRA16_BE: /* gggbbbbb arrrrrgg */
		if (saturate) {
			vu8 _20 = vsplatu8 (0x20);

			re = vsru (vand (vpacksu16 (re, ro), vsplatu8_F8), 1);
			be = vsru (vand (vpacksu16 (be, bo), vsplatu8_F8), 3);
			ge = vand (vpacksu16 (ge, go), vsplatu8_F8);

			ro = vunpackhi8 (re, be);	/* 000bbbbb 0rrrrr00 */
			re = vunpacklo8 (re, be);

			go = vunpackhi8 (ge, _20);	/* 00100000 ggggg000 */
			ro = vor (ro, vsru16 (go, 6));
			ro = vor (ro, vsl16 (go, 16 - 6));

			ge = vunpacklo8 (ge, _20);
			re = vor (re, vsru16 (ge, 6));
			re = vor (re, vsl16 (ge, 16 - 6));

			vstorent (dst, offset, vunpacklo16 (re, ro));
			vstorent (dst, offset + sizeof (vu8),
				  vunpackhi16 (re, ro));
		} else {
			re = vand (re, vsplatu16_F8);
			re = vsru16 (vor (re, vsplatu16_256), 1);
			re = vor (re, vsl16 (vand (be, vsplatu16_F8), 5));
			re = vor (re, vsl16 (vand (ge, vsplatu16_F8), 8 + 2));
			re = vor (re, vsru16 (ge, 6));

			ro = vand (ro, vsplatu16_F8);
			ro = vsru16 (vor (ro, vsplatu16_256), 1);
			ro = vor (ro, vsl16 (vand (bo, vsplatu16_F8), 5));
			ro = vor (ro, vsl16 (vand (go, vsplatu16_F8), 8 + 2));
			ro = vor (ro, vsru16 (go, 6));

			vstorent (dst, offset, vunpacklo16 (re, ro));
			vstorent (dst, offset + sizeof (vu8),
				  vunpackhi16 (re, ro));
		}

		break;

	default:
		assert (0);
	}
}

static always_inline void
fast_yuv2rgb			(v16 *			re0,
				 v16 *			ro0,
				 v16 *			ge0,
				 v16 *			go0,
				 v16 *			be0,
				 v16 *			bo0,
				 v16 *			re1,
				 v16 *			ro1,
				 v16 *			ge1,
				 v16 *			go1,
				 v16 *			be1,
				 v16 *			bo1,
				 v16			ye0,
				 v16			yo0,
				 v16			ye1,
				 v16			yo1,
				 v16			u,
				 v16			v)
{
	v16 bias, cr, cg, cb;

	bias = vsplat16_128;

	u = vsl16 (vsub16 (u, bias), 16 - GU_BU_SH);
	v = vsl16 (vsub16 (v, bias), 16 - RV_GV_SH);

	cb = vmulhi16 (u, _tv_vsplat16_yuv2rgb_bu);
	cr = vmulhi16 (v, _tv_vsplat16_yuv2rgb_rv);
	cg = vadd16 (vmulhi16 (u, _tv_vsplat16_yuv2rgb_gu),
		     vmulhi16 (v, _tv_vsplat16_yuv2rgb_gv));

	bias = vsru16 (bias, 3); /* vsplat16 (16); */

	ye0 = vmulhi16 (vsl16 (vsub16 (ye0, bias), 16 - CY_SH),
			_tv_vsplat16_yuv2rgb_cy);
	yo0 = vmulhi16 (vsl16 (vsub16 (yo0, bias), 16 - CY_SH),
			_tv_vsplat16_yuv2rgb_cy);

	*re0 = vadd16 (ye0, cr);
	*ro0 = vadd16 (yo0, cr);
	*ge0 = vsub16 (ye0, cg);
	*go0 = vsub16 (yo0, cg);
	*be0 = vadd16 (ye0, cb);
	*bo0 = vadd16 (yo0, cb);

	if (NULL != re1) {
		ye1 = vmulhi16 (vsl16 (vsub16 (ye1, bias), 16 - CY_SH),
				_tv_vsplat16_yuv2rgb_cy);
		yo1 = vmulhi16 (vsl16 (vsub16 (yo1, bias), 16 - CY_SH),
				_tv_vsplat16_yuv2rgb_cy);

		*re1 = vadd16 (ye1, cr);
		*ro1 = vadd16 (yo1, cr);
		*ge1 = vsub16 (ye1, cg);
		*go1 = vsub16 (yo1, cg);
		*be1 = vadd16 (ye1, cb);
		*bo1 = vadd16 (yo1, cb);
	}
}

#endif /* SIMD != ALTIVEC */

#endif /* SIMD */
