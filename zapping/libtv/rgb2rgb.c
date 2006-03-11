/*
 *  Copyright (C) 2006 Michael H. Schimek
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

/* $Id: rgb2rgb.c,v 1.4 2006-03-11 13:12:21 mschimek Exp $ */

/* RGB to RGB image format conversion functions:

   TV_PIXFMT_RGBA32_LE (= TV_PIXFMT_ABGR32_BE),
   TV_PIXFMT_RGBA32_BE (= TV_PIXFMT_ABGR32_LE),
   TV_PIXFMT_BGRA32_LE (= TV_PIXFMT_ARGB32_BE),
   TV_PIXFMT_BGRA32_BE (= TV_PIXFMT_ARGB32_LE),
   TV_PIXFMT_RGB24_LE (= TV_PIXFMT_BGR24_BE),
   TV_PIXFMT_RGB24_BE (= TV_PIXFMT_BGR24_LE),
   TV_PIXFMT_SBGGR,
    to
   TV_PIXFMT_RGBA32_LE (= TV_PIXFMT_ABGR32_BE),
   TV_PIXFMT_RGBA32_BE (= TV_PIXFMT_ABGR32_LE),
   TV_PIXFMT_BGRA32_LE (= TV_PIXFMT_ARGB32_BE),
   TV_PIXFMT_BGRA32_BE (= TV_PIXFMT_ARGB32_LE),
   TV_PIXFMT_RGB24_LE (= TV_PIXFMT_BGR24_BE),
   TV_PIXFMT_RGB24_BE (= TV_PIXFMT_BGR24_LE),
   TV_PIXFMT_BGR16_LE,
   TV_PIXFMT_BGR16_BE,
   TV_PIXFMT_BGRA16_LE,
   TV_PIXFMT_BGRA16_BE,
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "lut_rgb16.h"
#include "simd-conv.h"
#include "rgb2rgb.h"
#include "yuv2yuv.h"		/* shuffle routines */

#define Z_LE Z_LITTLE_ENDIAN
#define Z_BE Z_BIG_ENDIAN

#if !SIMD

tv_bool
_tv_rgb32_to_rgb16		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format)
{
	uint16_t *dst;
	const uint8_t *src;
	unsigned int width;
	unsigned int height;
	unsigned long dst_padding;
	unsigned long src_padding;
	const uint16_t (* lp)[256];
	uint16_t alpha1;

	width = MIN (dst_format->width, src_format->width);
	height = MIN (dst_format->height, src_format->height);

	if (0 == width || 0 == height)
		return FALSE;

	dst = (uint16_t *)((uint8_t *) dst_image + dst_format->offset[0]);
	src = (const uint8_t *) src_image + src_format->offset[0];

	dst_padding = dst_format->bytes_per_line[0] - width * 2;

	switch (dst_format->pixel_format->pixfmt) {
#undef DST_CASE
#define DST_CASE(fmt, depth, en, lut, a1)				\
	case TV_PIXFMT_ ## fmt ## depth ## _ ## en:			\
		lp = lut[Z_BYTE_ORDER != Z_ ## en];			\
		alpha1 = (Z_BYTE_ORDER == Z_ ## en) ? a1 : SWAB16 (a1);	\
		goto fmt;

#if 0
	DST_CASE (RGBA, 12, LE, _tv_lut_rgb12, 0xF000)
	DST_CASE (RGBA, 12, BE, _tv_lut_rgb12, 0xF000)
	DST_CASE (BGRA, 12, LE, _tv_lut_rgb12, 0xF000)
	DST_CASE (BGRA, 12, BE, _tv_lut_rgb12, 0xF000)

	DST_CASE (RGB,  16, LE, _tv_lut_rgb16, 0)
	DST_CASE (RGB,  16, BE, _tv_lut_rgb16, 0)
	DST_CASE (RGBA, 16, LE, _tv_lut_rgb16, 0x8000)
	DST_CASE (RGBA, 16, BE, _tv_lut_rgb16, 0x8000)
#endif
	DST_CASE (BGR,  16, LE, _tv_lut_rgb16, 0)
	DST_CASE (BGR,  16, BE, _tv_lut_rgb16, 0)
	DST_CASE (BGRA, 16, LE, _tv_lut_rgb16, 0x8000)
	DST_CASE (BGRA, 16, BE, _tv_lut_rgb16, 0x8000)

	default:
		return FALSE;
	}

#undef LOOP
#define LOOP(c0, c1, c2, c3, src_bpp)					\
do {									\
	src_padding = src_format->bytes_per_line[0] - width * src_bpp;	\
									\
	if ((long)(src_padding | dst_padding) < 0) {			\
		return FALSE;						\
	} else if (0 == (src_padding | dst_padding)) {			\
		width *= height;					\
		height = 1;						\
	}								\
									\
	while (height-- > 0) {						\
		const uint8_t *end;					\
									\
		for (end = src + width * src_bpp; src < end;) {		\
			if (c0 < 0) {					\
				*dst++ = (lp[c1][src[1]] |		\
					  lp[c2][src[2]] |		\
					  lp[c3][src[3]] |		\
					  alpha1);			\
			} else if (c3 < 0) {				\
				*dst++ = (lp[c0][src[0]] |		\
					  lp[c1][src[1]] |		\
					  lp[c2][src[2]] |		\
					  alpha1);			\
			} else {					\
				*dst++ = (lp[c0][src[0]] |		\
					  lp[c1][src[1]] |		\
					  lp[c2][src[2]] |		\
					  lp[c3][src[3]]);		\
			}						\
									\
			src += src_bpp;					\
		}							\
									\
		dst = (uint16_t *)((uint8_t *) dst + dst_padding);	\
		src += src_padding;					\
	}								\
} while (0)

#undef SRC_SWITCH
#define SRC_SWITCH(r, g, b, a)						\
do {									\
	switch (src_format->pixel_format->pixfmt) {			\
	case TV_PIXFMT_RGBA32_LE:					\
		LOOP (r, g, b, a, 4);					\
		break;							\
									\
	case TV_PIXFMT_RGBA32_BE:					\
		LOOP (a, b, g, r, 4);					\
		break;							\
									\
	case TV_PIXFMT_BGRA32_LE:					\
		LOOP (b, g, r, a, 4);					\
		break;							\
									\
	case TV_PIXFMT_BGRA32_BE:					\
		LOOP (a, r, g, b, 4);					\
		break;							\
									\
	case TV_PIXFMT_RGB24_LE: 					\
		LOOP (r, g, b, -1, 3);					\
		break;							\
									\
	case TV_PIXFMT_RGB24_BE: 					\
		LOOP (b, g, r, -1, 3);					\
		break;							\
									\
	default:							\
		return FALSE;						\
	}								\
									\
	return TRUE;							\
} while (0)

#if 0
 RGB:
	SRC_SWITCH (4, 2, 0, -1);
 RGBA:	
	SRC_SWITCH (4, 3, 1, 5);
#endif

 BGR:
	SRC_SWITCH (0, 2, 4, -1);
 BGRA:	
	SRC_SWITCH (1, 3, 4, 5);
}

tv_bool
_tv_rgb32_to_rgb32		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format)
{
	static const char matrix [6][6] = {
		{ 0,  1, 2, 12,  8, 10 },
		{ 1,  0, 3, 13,  9, 11 },
		{ 2, 12, 0,  1, 10,  8 },
		{ 3, 13, 1,  0, 11,  9 },
		{ 4,  5, 6,  7,  0, 10 },
		{ 6,  7, 4,  5, 10,  0 }
	};
	uint8_t *dst;
	const uint8_t *src;
	unsigned int width;
	unsigned int height;
	long dst_padding;
	long src_padding;
	unsigned int src_bpp;
	tv_pixfmt dst_pixfmt;
	tv_pixfmt src_pixfmt;

	width = MIN (dst_format->width, src_format->width);
	height = MIN (dst_format->height, src_format->height);

	if (0 == width || 0 == height)
		return FALSE;

	dst_padding = dst_format->bytes_per_line[0]
		- ((width * dst_format->pixel_format->bits_per_pixel) >> 3);
	src_padding = src_format->bytes_per_line[0]
		- ((width * src_format->pixel_format->bits_per_pixel) >> 3);

	if (likely (0 == (src_padding | dst_padding))) {
		width *= height;
		height = 1;
	} else if (unlikely ((long)(src_padding | dst_padding) < 0)) {
		return FALSE;
	}

	dst = (uint8_t *) dst_image + dst_format->offset[0];
	src = (const uint8_t *) src_image + src_format->offset[0];

#undef LOOP
#define LOOP(op, src_bpp, dst_bpp)					\
do {									\
	while (height-- > 0) {						\
		const uint8_t *end;					\
									\
		for (end = src + width * src_bpp; src < end;) {		\
			op;						\
			src += src_bpp;					\
			dst += dst_bpp;					\
		}							\
									\
		src += src_padding;					\
		dst += dst_padding;					\
	}								\
} while (0)

	dst_pixfmt = dst_format->pixel_format->pixfmt;
	src_pixfmt = src_format->pixel_format->pixfmt;

	switch (matrix[src_pixfmt - TV_PIXFMT_RGBA32_LE]
		      [dst_pixfmt - TV_PIXFMT_RGBA32_LE]) {
	case 0: /* identity */
		return tv_copy_image (dst_image,
				      dst_format,
				      src_image,
				      src_format);

	case 1: /* ABGR -> RGBA, ARGB -> BGRA */
		_tv_shuffle_3210_SCALAR (dst, src, width * 4, height,
					 dst_padding, src_padding);
		break;

	case 2: /* BGRA -> RGBA, RGBA -> BGRA */
		_tv_shuffle_2103_SCALAR (dst, src, width * 4, height,
					 dst_padding, src_padding);
		break;

	case 3: /* ARGB -> RGBA, ABGR -> BGRA */
		_tv_shuffle_1230_SCALAR (dst, src, width * 4, height,
					 dst_padding, src_padding);
		break;

	case 12: /* BGRA -> ABGR, RGBA -> ARGB */
		_tv_shuffle_3012_SCALAR (dst, src, width * 4, height,
					 dst_padding, src_padding);
		break;

	case 13: /* ARGB -> ABGR, ABGR -> ARGB */
		_tv_shuffle_0321_SCALAR (dst, src, width * 4, height,
					 dst_padding, src_padding);
		break;

	case 4: /* RGB -> RGBA, BGR -> BGRA */
		LOOP ({ dst[0] = src[0];
		        dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = 0xFF; }, 3, 4);
		break;

	case 5: /* RGB -> ABGR, BGR -> ARGB */
		LOOP ({ dst[0] = 0xFF;
			dst[1] = src[2];
			dst[2] = src[1];
			dst[3] = src[0]; }, 3, 4);
		break;

	case 6: /* RGB -> BGRA, BGR -> RGBA */
		LOOP ({ dst[0] = src[2];
			dst[1] = src[1];
			dst[2] = src[0];
			dst[3] = 0xFF; }, 3, 4);
		break;

	case 7: /* RGB -> ARGB, BGR -> ABGR */
		LOOP ({ dst[0] = 0xFF;
			dst[1] = src[0];
			dst[2] = src[1];
			dst[3] = src[2]; }, 3, 4);
		break;

	case 11: /* ARGB -> RGB, ABGR -> BGR */
		++src;
	case 8: /* RGBA -> RGB, BGRA -> BGR */
		LOOP ({ dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2]; }, 4, 3);
		break;

	case 9: /* ABGR -> RGB, ARGB -> BGR */
		++src;
	case 10: /* BGRA -> RGB, RGBA -> BGR, BGR <-> RGB */
		src_bpp = src_format->pixel_format->bits_per_pixel >> 3;
		LOOP ({ dst[0] = src[2];
			dst[1] = src[1];
			dst[2] = src[0]; }, src_bpp, 3);
		break;

	default:
		return FALSE;
	}

	return TRUE;
}

#endif /* !SIMD */

typedef tv_bool
sbggr_to_rgb_fn			(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format);

SIMD_FN_PROTOS (sbggr_to_rgb_fn, _tv_sbggr_to_rgb)

#if SIMD

/* Memory: 89abcdefghij (src + offset points at a)
   mm = 9bdf extended to 16 bits, bbdf in first column
   m0 = aceg extended to 16 bits
   m1 = bdfh extended to 16 bits
   m2 = cegi extended to 16 bits, cegg in last column

   We should really calculate the average of three pixels in first
   and last column, but this is easier. */
static always_inline void
bggr_load			(v16 *			mm,
				 v16 *			m0,
				 v16 *			m1,
				 v16 *			m2,
				 const uint8_t *	src,
				 long			offset,
				 tv_bool		in_first_column,
				 tv_bool		in_last_column)
{
	*m0 = vload (src, offset);

#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW)

	*m1 = vsru16 ((vu16) *m0, 8);
	*m0 = vand (*m0, vsplat16_255);

	if (mm) {
		if (in_first_column) {
			*mm = vor (vsl (*m1, 16),
				   vand (*m1, _mm_set_pi16 (0, 0, 0, -1)));
		} else {
			*mm = vsru16 (vload (src, offset - 2), 8);
		}
	}

	if (m2) {
		if (in_last_column) {
			*m2 = vor (vsru (*m0, 16),
				   vand (*m0, _mm_set_pi16 (-1, 0, 0, 0)));
		} else {
			*m2 = vsru16 (vload (src, offset + 1), 8);
		}
	}

#elif SIMD & CPU_FEATURE_SSE

	*m1 = vsru16 ((vu16) *m0, 8);
	*m0 = vand (*m0, vsplat16_255);

	if (mm) {
		*mm = _mm_shuffle_pi16 (*m1, _MM_SHUFFLE (2, 1, 0, 0));
		if (!in_first_column) {
			unsigned int t = src[offset - 1];
			*mm = _mm_insert_pi16 (vsl (*mm, 16), t, 0);
		}
	}

	if (m2) {
		*m2 = _mm_shuffle_pi16 (*m0, _MM_SHUFFLE (3, 3, 2, 1));
		if (!in_last_column) {
			unsigned int t = src[offset + sizeof (vu8)];
			*m2 = _mm_insert_pi16 (vsru (*m2, 16), t, 3);
		}
	}

#elif SIMD & CPU_FEATURE_SSE2

	*m1 = vsru16 ((vu16) *m0, 8);
	*m0 = vand (*m0, vsplat16_255);

	if (mm) {
		*mm = _mm_slli_si128 (*m1, 2);
		if (in_first_column) {
			*mm = _mm_shufflelo_epi16 (*m1,
						   _MM_SHUFFLE (2, 1, 0, 0));
		} else {
			unsigned int t = src[offset - 1];
			*mm = _mm_insert_epi16 (*mm, t, 0);
		}
	}

	if (m2) {
		*m2 = _mm_srli_si128 (*m0, 2);
		if (in_last_column) {
			*m2 = _mm_shufflehi_epi16 (*m0,
						   _MM_SHUFFLE (3, 3, 2, 1));
		} else {
			unsigned int t = src[offset + sizeof (vu8)];
			*m2 = _mm_insert_epi16 (*m2, t, 7);
		}
	}

#elif SIMD & CPU_FEATURE_ALTIVEC

	*m1 = vand (*m0, vsplat16_255);
	*m0 = vsru16 ((vu16) *m0, 8);

	if (mm} {
		if (in_first_column) {
			const vu8 sel = { 0x00, 0x01, 0x00, 0x01, 0x02, 0x03,
					  0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
					  0x0A, 0x0B, 0x0C, 0x0D };
			*mm = vec_perm (*m1, *m1, sel);
		} else {
			*mm = vload (src, offset - sizeof (v16));
			*mm = vand (*mm, vsplat16_255);
			*mm = vec_perm (*mm, *m1, vec_lvsl (-2, src));
		}
	}

	if (m2) {
		if (in_last_column) {
			const vu8 sel = { 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
					  0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
					  0x0E, 0x0F, 0x0E, 0x0F };
			*m2 = vec_perm (*m0, *m0, sel);
		} else {
			*m2 = vload (src, offset + sizeof (v16));
			*m2 = vsru16 ((vu16) *m2, 8);
			*m2 = vec_perm (*m0, *m2, vec_lvsl (+2, src));
		}
	}

#endif /* SIMD */

}

#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW)

/* Memory layout:
  xxx xxx xxx xxx xxx xxx ...
  xxx Gmm Bm0 Gm1 Bm2 Gm3 ...
  xxx R0m(G00 R01)G02 R03 ...
  xxx G1m(B10 G11)B12 G13 ...
  xxx R2m G20 R21 G22 R22 ...

  Converts four bytes of GRBG to RGB.  src points at G00.
*/
static always_inline void
sbggr_odd_kernel		(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned long		dst_bpl,
				 unsigned long		src_bpl,
				 tv_pixfmt		pixfmt,
				 tv_bool		in_first_column,
				 tv_bool		in_last_column)
{
	v16 bm0, gm1, bm2;
	v16 g1m, b10, g11, b12;
	v16 r0m, g00, r01, g02;
	v16 gx;
	v16 r2m, g20, r21;

	dst_bpl = dst_bpl;

	bggr_load (&g1m, &b10, &g11, &b12, src, +1 * src_bpl,
		   in_first_column, in_last_column);

	b12 = vadd16 (b12, vsplat16_1);

	bggr_load (NULL, &bm0, &gm1, &bm2, src, -1 * src_bpl,
		   in_first_column, in_last_column);

	bm0 = vadd16 (vadd16 (bm0, b10), vsplat16_1);
	bm2 = vsr16 (vadd16 (vadd16 (bm2, bm0), b12), 2);

	bggr_load (&r0m, &g00, &r01, &g02, src, 0 * src_bpl,
		   in_first_column, in_last_column);

	r0m = vadd16 (r0m, vsplat16_1);

	gx = vadd16 (vadd16 (g00, g11), vsplat16_2);
	gm1 = vsr16 (vadd16 (vadd16 (gm1, g02), gx), 2);
	g1m = vadd16 (g1m, gx);

	store_rgb16 (dst, /* offset */ 0, pixfmt,
		     /* saturate */ FALSE,
		     /* r */ vsr16 (vadd16 (r0m, r01), 1), r01,
		     /* g */ g00, gm1,
		     /* b */ vsr16 (bm0, 1), bm2);

	bggr_load (&r2m, &g20, &r21, NULL, src, +2 * src_bpl,
		   in_first_column, in_last_column);

	r21 = vadd16 (vadd16 (r21, r01), vsplat16_1);
	r2m = vsr16 (vadd16 (vadd16 (r2m, r21), r0m), 2);
	g1m = vsr16 (vadd16 (g1m, g20), 2);

	store_rgb16 (dst, /* offset */ dst_bpl, pixfmt,
		     /* saturate */ FALSE,
		     /* r */ r2m, vsr16 (r21, 1),
		     /* g */ g1m, g11,
		     /* b */ b10, vsr16 (vadd16 (b12, b10), 1));
}

#elif SIMD & (CPU_FEATURE_SSE | CPU_FEATURE_SSE2 | CPU_FEATURE_ALTIVEC)

/* Memory layout:
  xxx xxx xxx xxx xxx xxx ...
  xxx Gmm Bm0 Gm1 Bm2 Gm3 ...
  xxx R0m(G00 R01)G02 R03 ...
  xxx G1m(B10 G11)B12 G13 ...
  xxx R2m G20 R21 G22 R22 ...

  Converts four bytes of GRBG to RGB.  src points at G00.
*/
static always_inline void
sbggr_odd_kernel		(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned long		dst_bpl,
				 unsigned long		src_bpl,
				 tv_pixfmt		pixfmt,
				 tv_bool		in_first_column,
				 tv_bool		in_last_column)
{
	v16 bm0, gm1, bm2;
	v16 g1m, b10, g11, b12;
	v16 r0m, g00, r01, g02;
	v16 gx;
	v16 r2m, g20, r21;

	dst_bpl = dst_bpl;

	bggr_load (NULL, &bm0, &gm1, &bm2, src, -1 * src_bpl,
		   in_first_column, in_last_column);

	bggr_load (&g1m, &b10, &g11, &b12, src, +1 * src_bpl,
		   in_first_column, in_last_column);

	bm2 = vadd16 (vadd16 (vadd16 (bm2, bm0), b12), b10);
	bm2 = vsr16 (vadd16 (bm2, vsplat16_2), 2);

	bggr_load (&r0m, &g00, &r01, &g02, src, 0 * src_bpl,
		   in_first_column, in_last_column);

	gx = vadd16 (vadd16 (g00, g11), vsplat16_2);
	gm1 = vsr16 (vadd16 (vadd16 (gm1, g02), gx), 2);
	g1m = vadd16 (g1m, gx);

	store_rgb16 (dst, /* offset */ 0, pixfmt,
		     /* saturate */ FALSE,
		     /* r */ vavgu16 (r0m, r01), r01,
		     /* g */ g00, gm1,
		     /* b */ vavgu16 (bm0, b10), bm2);

	bggr_load (&r2m, &g20, &r21, NULL, src, +2 * src_bpl,
		   in_first_column, in_last_column);

	r2m = vadd16 (vadd16 (vadd16 (r2m, r21), r0m), r01);
	r2m = vsr16 (vadd16 (r2m, vsplat16_2), 2);

	g1m = vsr16 (vadd16 (g1m, g20), 2);

	store_rgb16 (dst, /* offset */ dst_bpl, pixfmt,
		     /* saturate */ FALSE,
		     /* r */ r2m, vavgu16 (r01, r21),
		     /* g */ g1m, g11,
		     /* b */ b10, vavgu16 (b12, b10));
}

#endif /* SIMD & (CPU_FEATURE_SSE | CPU_FEATURE_SSE2 | CPU_FEATURE_ALTIVEC) */

static always_inline void
sbggr_odd_top			(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned long		dst_bpl,
				 unsigned long		src_bpl,
				 tv_pixfmt		pixfmt,
				 tv_bool		in_first_column,
				 tv_bool		in_last_column)
{
	v16 g1m, b10, g11, b12;
	v16 r2m, g20, r21;

	dst_bpl = dst_bpl;

	bggr_load (&g1m, &b10, &g11, &b12, src, 0 * src_bpl,
		   in_first_column, in_last_column);

	g1m = vadd16 (vadd16 (g1m, g11), vsplat16_2);

	bggr_load (&r2m, &g20, &r21, NULL, src, 1 * src_bpl,
		   in_first_column, in_last_column);

	/* Should be (G1m + G20 + G11 + 1) / 3, but this is easier. */
	g1m = vsr16 (vadd16 (g1m, vadd16 (g20, g20)), 2);

	store_rgb16 (dst, /* offset */ 0, pixfmt,
		     /* saturate */ FALSE,
		     /* r */ small_vavgu16 (r2m, r21), r21,
		     /* g */ g1m, g11,
		     /* b */ b10, small_vavgu16 (b12, b10));
}

static always_inline void
sbggr_odd_bottom		(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned long		dst_bpl,
				 unsigned long		src_bpl,
				 tv_pixfmt		pixfmt,
				 tv_bool		in_first_column,
				 tv_bool		in_last_column)
{
	v16 r0m, g00, r01, g02;
	v16 bm0, gm1, bm2;

	dst_bpl = dst_bpl;

	bggr_load (&r0m, &g00, &r01, &g02, src, /* offset */ 0,
		   in_first_column, in_last_column);

	g02 = vadd16 (vadd16 (g02, g00), vsplat16_2);

	bggr_load (NULL, &bm0, &gm1, &bm2, src, -1 * src_bpl,
		   in_first_column, in_last_column);

	/* Should be (Gm1 + G00 + G02 + 1) / 3, but this is easier. */
	g02 = vsr16 (vadd16 (g02, vadd16 (gm1, gm1)), 2);

	store_rgb16 (dst, /* offset */ 0, pixfmt,
		     /* saturate */ FALSE,
		     /* r */ small_vavgu16 (r0m, r01), r01,
		     /* g */ g00, g02,
		     /* b */ bm0, small_vavgu16 (bm2, bm0));
}

#define sbggr_row(kernel, fmt)						\
do {									\
	unsigned int bpp;						\
	const uint8_t *end;						\
									\
	bpp = TV_PIXFMT_BYTES_PER_PIXEL (TV_PIXFMT_ ## fmt);		\
									\
	kernel (dst, src, dst_bpl, src_bpl, TV_PIXFMT_ ## fmt,		\
		/* in_first_column */ TRUE,				\
		/* in_last_column */ FALSE);				\
	dst += sizeof (vu8) * bpp;					\
	src += sizeof (vu8);						\
									\
	for (end = src + width - sizeof (vu8) * 2; src < end;) {	\
		kernel (dst, src, dst_bpl, src_bpl,			\
			TV_PIXFMT_ ## fmt,				\
			/* in_first_column */ FALSE,			\
			/* in_last_column */ FALSE);			\
		dst += sizeof (vu8) * bpp;				\
		src += sizeof (vu8);					\
	}								\
									\
	kernel (dst, src, dst_bpl, src_bpl, TV_PIXFMT_ ## fmt,		\
		/* in_first_column */ FALSE,				\
		/* in_last_column */ TRUE);				\
} while (0)

#define sbggr_to_rgb_loop(fmt)						\
static tv_bool								\
sbggr_to_ ## fmt ## _loop	(uint8_t *		dst,		\
				 const uint8_t *	src,		\
				 unsigned int		width,		\
				 unsigned int		height,		\
				 unsigned long		dst_bpl,	\
				 unsigned long		src_bpl)	\
{									\
	unsigned long run;						\
	long dst_padding;						\
	long src_padding;						\
									\
	sbggr_row (sbggr_odd_top, fmt);					\
									\
	run = (width - sizeof (vu8))					\
		* TV_PIXFMT_BYTES_PER_PIXEL (TV_PIXFMT_ ## fmt);	\
									\
	dst += dst_bpl - run;						\
	src += src_bpl - width + sizeof (vu8);				\
									\
	dst_padding = dst_bpl * 2 - run;				\
	src_padding = src_bpl * 2 - width + sizeof (vu8);		\
									\
	for (height /= 2; --height > 0;) {				\
		sbggr_row (sbggr_odd_kernel, fmt);			\
		dst += dst_padding;					\
		src += src_padding;					\
	}								\
									\
	sbggr_row (sbggr_odd_bottom, fmt);				\
									\
	vempty ();							\
									\
	return TRUE;							\
}

sbggr_to_rgb_loop (RGBA32_LE)
sbggr_to_rgb_loop (RGBA32_BE)
sbggr_to_rgb_loop (BGRA32_LE)
sbggr_to_rgb_loop (BGRA32_BE)
sbggr_to_rgb_loop (RGB24_LE)
sbggr_to_rgb_loop (RGB24_BE)
sbggr_to_rgb_loop (BGR16_LE)
sbggr_to_rgb_loop (BGR16_BE)
sbggr_to_rgb_loop (BGRA16_LE)
sbggr_to_rgb_loop (BGRA16_BE)

tv_bool
SIMD_NAME (_tv_sbggr_to_rgb)	(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format)
{
	uint8_t *dst;
	const uint8_t *src;
	unsigned int width;
	unsigned int height;
	unsigned long dst_bpl;
	unsigned long src_bpl;

	dst = (uint8_t *) dst_image + dst_format->offset[0];
	src = (const uint8_t *) src_image + src_format->offset[0];

	width = MIN (dst_format->width, src_format->width);
	height = MIN (dst_format->height, src_format->height);

	dst_bpl = dst_format->bytes_per_line[0];
	src_bpl = src_format->bytes_per_line[0];

	switch (dst_format->pixel_format->pixfmt) {
#undef DST_CASE
#define DST_CASE(fmt)							\
	case TV_PIXFMT_ ## fmt:						\
		return sbggr_to_ ## fmt ## _loop (dst, src,		\
						  width, height,	\
						  dst_bpl, src_bpl);	\
		break;

	DST_CASE (RGBA32_LE)
	DST_CASE (RGBA32_BE)
	DST_CASE (BGRA32_LE)
	DST_CASE (BGRA32_BE)
	DST_CASE (RGB24_LE)
	DST_CASE (RGB24_BE)
	DST_CASE (BGR16_LE)
	DST_CASE (BGR16_BE)
	DST_CASE (BGRA16_LE)
	DST_CASE (BGRA16_BE)

	default:
		return FALSE;
	}
}

#else /* !SIMD */

#define store(fmt, en, r, g, b)						\
do {									\
	uint16_t *dst16 = (void *) dst;     				\
									\
	switch (TV_PIXFMT_ ## fmt ## _ ## en) {				\
	case TV_PIXFMT_RGBA32_LE:					\
		dst[0] = r; dst[1] = g; dst[2] = b; dst[3] = -1;	\
		break;							\
									\
	case TV_PIXFMT_RGBA32_BE:					\
		dst[0] = -1; dst[1] = b; dst[2] = g; dst[3] = r;	\
		break;							\
									\
	case TV_PIXFMT_BGRA32_LE:					\
		dst[0] = b; dst[1] = g; dst[2] = r; dst[3] = -1;	\
		break;							\
									\
	case TV_PIXFMT_BGRA32_BE:					\
		dst[0] = -1; dst[1] = r; dst[2] = g; dst[3] = b;	\
		break;							\
									\
	case TV_PIXFMT_RGB24_LE:					\
		dst[0] = r; dst[1] = g; dst[2] = b;			\
		break;							\
									\
	case TV_PIXFMT_RGB24_BE:					\
		dst[0] = b; dst[1] = g; dst[2] = r;			\
		break;							\
									\
	case TV_PIXFMT_BGR16_LE:					\
	case TV_PIXFMT_BGR16_BE:					\
		*dst16 = (lp[0][r] |					\
			  lp[2][g] |					\
			  lp[4][b]);					\
		break;							\
									\
	case TV_PIXFMT_BGRA16_LE:					\
	case TV_PIXFMT_BGRA16_BE:					\
		*dst16 = (lp[1][r] |					\
			  lp[3][g] |					\
			  lp[4][b] |					\
			  ((Z_BYTE_ORDER == Z_ ## en) ? 0x8000 : 0x80)); \
		break;							\
									\
	default:							\
		assert (0);						\
	}								\
									\
	dst += TV_PIXFMT_BYTES_PER_PIXEL (TV_PIXFMT_ ## fmt ## _ ## en); \
} while (0)

/* Memory layout:
   xxx xxx xxx xxx xxx xxx ...
   xxx Gmm Bm0 Gm1 Bm2 Gm3 ...
   xxx R0m(G00 R01)G02 R03 ...
   xxx G1m(B10 G11)B12 G13 ...
   xxx R2m G20 R21 G22 R22 ... */
#define sbggr_to_rgb_loop(fmt, en, a1)					\
static tv_bool								\
sbggr_to_ ## fmt ## _ ## en ## _loop					\
				(uint8_t *		dst,		\
				 const tv_image_format *dst_format,	\
				 const uint8_t *	src,		\
				 const tv_image_format *src_format,	\
				 unsigned int		width,		\
				 unsigned int		height)		\
{									\
	const uint16_t (* lp)[256];					\
	unsigned long dst_bpl;						\
	unsigned long src_bpl;						\
	unsigned long dst_padding;					\
	unsigned long src_padding;					\
	unsigned int count;						\
									\
	dst_bpl = dst_format->bytes_per_line[0];			\
	src_bpl = src_format->bytes_per_line[0];			\
									\
	dst_padding = dst_bpl - width					\
		* TV_PIXFMT_BYTES_PER_PIXEL (TV_PIXFMT_ ## fmt ## _ ## en); \
	src_padding = src_bpl - width + 2 /* left & right */;		\
									\
	if ((long)(dst_padding | src_padding) < 0)			\
		return FALSE;						\
									\
	lp = _tv_lut_rgb16[Z_BYTE_ORDER != Z_ ## en];			\
									\
	/* Top left pixel (Bm0). */					\
	store (fmt, en, src[1 + src_bpl],				\
	       (src[1] + src[src_bpl] + 1) >> 1, src[0]);		\
									\
	/* First line (Gm1, Bm2). */					\
	for (count = width / 2; --count > 0;) {				\
		store (fmt, en, src[1 + src_bpl], src[1],		\
		       (src[0] + src[2] + 1) >> 1);			\
		/* Should be src[1] + src[3] + src[2+src_bpl] + 1) / 3,	\
		   but this is easier. */				\
		store (fmt, en,						\
		       (src[1 + src_bpl] + src[3 + src_bpl] + 1) >> 1, 	\
		       (src[1] + src[3] + src[2 + src_bpl] * 2 + 2)	\
		       >> 2, src[2]);					\
		src += 2;						\
	}								\
									\
	/* Top right pixel (Gm3). */					\
	store (fmt, en, src[1 + src_bpl], src[1], src[0]);		\
									\
	src += src_padding;						\
	dst += dst_padding;						\
									\
	for (height /= 2; --height > 0;) {				\
		unsigned int v0, v1;					\
									\
		v0 = src[-src_bpl] + src[src_bpl] + 1;			\
									\
		/* First column (G00). */				\
		store (fmt, en, src[1], src[0], v0 >> 1);		\
									\
		/* Odd line (R01, G02). */				\
		for (count = width / 2; --count > 0;) {			\
			v1 = src[2 - src_bpl] + src[2 + src_bpl] + 1;	\
			store (fmt, en, src[1], (src[1 - src_bpl]	\
			       + src[0] + src[2] + src[1 + src_bpl]	\
			       + 2) >> 2, (v0 + v1) >> 2);		\
			store (fmt, en, (src[1] + src[3] + 1) >> 1,	\
			       src[2], v1 >> 1);			\
			src += 2;					\
			v0 = v1;					\
		}							\
									\
		/* Last column (R03). */				\
		store (fmt, en, src[1], (src[1 - src_bpl] + src[0] * 2	\
		       + src[1 + src_bpl] + 2) >> 2, v0 >> 1);		\
									\
		src += src_padding;					\
		dst += dst_padding;					\
									\
		v0 = src[1 - src_bpl] + src[1 + src_bpl] + 1;		\
									\
		/* First column (B10). */				\
		store (fmt, en, v0 >> 1, (src[-src_bpl] + src[1] * 2	\
		       + src[src_bpl] + 2) >> 2, src[0]);		\
									\
		/* Even line (G11, B12). */				\
		for (count = width / 2; --count > 0;) {			\
			v1 = src[3 - src_bpl] + src[3 + src_bpl] + 1;	\
			store (fmt, en, v0 >> 1, src[1],		\
			       (src[0] + src[2] + 1) >> 1);		\
			store (fmt, en, (v0 + v1) >> 2,			\
			       (src[2 - src_bpl] + src[1] + src[3]	\
				+ src[2 + src_bpl] + 2) >> 2, src[2]);	\
			src += 2;					\
			v0 = v1;					\
		}							\
									\
		/* Last column (G13). */				\
		store (fmt, en, v0 >> 1, src[1], src[0]);		\
									\
		src += src_padding;					\
		dst += dst_padding;					\
	}								\
									\
	/* Bottom left pixel (G20). */					\
	store (fmt, en, src[1], src[0], src[-src_bpl]);			\
									\
	/* Last line (R21, G22). */					\
	for (count = width / 2; --count > 0;) {				\
		store (fmt, en, src[1], (src[1 - src_bpl] * 2 + src[0]	\
		       + src[2] + 2) >> 2,				\
		       (src[-src_bpl] + src[2 - src_bpl] + 1) >> 1);	\
		store (fmt, en, (src[1] + src[3] + 1) >> 1, src[2],	\
		       src[2 - src_bpl]);				\
		src += 2;						\
	}								\
									\
	/* Bottom right pixel (R23). */					\
	store (fmt, en, src[1], (src[0] + src[1 - src_bpl] + 1) >> 1,	\
	       src[-src_bpl]);						\
									\
	return TRUE;							\
}

sbggr_to_rgb_loop (RGBA32, LE, 0)
sbggr_to_rgb_loop (RGBA32, BE, 0)
sbggr_to_rgb_loop (BGRA32, LE, 0)
sbggr_to_rgb_loop (BGRA32, BE, 0)
sbggr_to_rgb_loop (RGB24, LE, 0)
sbggr_to_rgb_loop (RGB24, BE, 0)
sbggr_to_rgb_loop (BGR16, LE, 0)
sbggr_to_rgb_loop (BGR16, BE, 0)
sbggr_to_rgb_loop (BGRA16, LE, 0x8000)
sbggr_to_rgb_loop (BGRA16, BE, 0x8000)

tv_bool
_tv_sbggr_to_rgb		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format)
{
	uint8_t *dst;
	const uint8_t *src;
	unsigned int width;
	unsigned int height;
	unsigned long align;

	width = MIN (dst_format->width, src_format->width);
	height = MIN (dst_format->height, src_format->height);

	if (0 == width || 0 == height
	    || ((width | height) & 1)
	    || width > src_format->bytes_per_line[0]
	    || (((width * dst_format->pixel_format->bits_per_pixel) >> 3)
		> dst_format->bytes_per_line[0]))
		return FALSE;

	dst = (uint8_t *) dst_image + dst_format->offset[0];
	src = (const uint8_t *) src_image + src_format->offset[0];

	align = ((unsigned long) dst |
		 (unsigned long) src |
		 dst_format->bytes_per_line[0] |
		 src_format->bytes_per_line[0] |
		 width);

	if (likely (0 == (align % 16) && width >= 3 * 16)) {
#if 0 /* defined CAN_COMPILE_ALTIVEC */
		if (cpu_features & CPU_FEATURE_ALTIVEC)
			return _tv_sbggr_to_rgb_ALTIVEC (dst_image,
							 dst_format,
							 src_image,
							 src_format);
		else
#endif
#if 0 /* defined CAN_COMPILE_SSE2 */
		if (cpu_features & CPU_FEATURE_SSE2)
			return _tv_sbggr_to_rgb_SSE2 (dst_image, dst_format,
						      src_image, src_format);
		else
#endif
			(void) 0;
	}

	if (likely (0 == (align % 8) && width >= 3 * 8)) {
#if 0 /* defined CAN_COMPILE_SSE */
		if (cpu_features & CPU_FEATURE_SSE)
			return _tv_sbggr_to_rgb_SSE (dst_image, dst_format,
						     src_image, src_format);
		else
#endif
#ifdef CAN_COMPILE_MMX
		if (cpu_features & CPU_FEATURE_MMX)
			return _tv_sbggr_to_rgb_MMX (dst_image, dst_format,
						     src_image, src_format);
		else
#endif
			(void) 0;
	}

	/* Scalar implementation. */

	switch (dst_format->pixel_format->pixfmt) {
#undef DST_CASE
#define DST_CASE(fmt)							\
	case TV_PIXFMT_ ## fmt:						\
		return sbggr_to_ ## fmt ## _loop (dst, dst_format,	\
						  src, src_format,	\
						  width, height);	\
		break;

	DST_CASE (RGBA32_LE)
	DST_CASE (RGBA32_BE)
	DST_CASE (BGRA32_LE)
	DST_CASE (BGRA32_BE)
	DST_CASE (RGB24_LE)
	DST_CASE (RGB24_BE)
	DST_CASE (BGR16_LE)
	DST_CASE (BGR16_BE)
	DST_CASE (BGRA16_LE)
	DST_CASE (BGRA16_BE)

	default:
		return FALSE;
	}
}

#endif /* !SIMD */
