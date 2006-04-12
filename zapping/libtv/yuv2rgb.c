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

/* $Id: yuv2rgb.c,v 1.2 2006-04-12 01:46:51 mschimek Exp $ */

/* YUV to RGB image format conversion functions:

   TV_PIXFMT_YUV420,
   TV_PIXFMT_YVU420,
   TV_PIXFMT_YUYV,
   TV_PIXFMT_YVYU,
   TV_PIXFMT_UYVY,
   TV_PIXFMT_VYUY,
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

   Assumed color sample position:

   YUV420 progressive	interlaced
   L   L		Lt    Lt
     C
   L   L		Lb Ct Lb

   L   L		Lt Cb Lt
     C
   L   L		Lb    Lb

   YUYV
   L C L   L C L
   L C L   L C L
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "lut_yuv2rgb.h"
#include "yuv2rgb.h"
#include "simd-conv.h"

#define Z_LE Z_LITTLE_ENDIAN
#define Z_BE Z_BIG_ENDIAN

SIMD_FN_ARRAY_PROTOS (copy_plane_fn *, yuyv_to_rgb_loops, [4 * 10])

#if SIMD & CPU_FEATURE_MMX

#define YUYV_RGB(dst_fmt, src_fmt)					\
static void								\
SIMD_NAME (src_fmt ## _to_ ## dst_fmt ## _loop)				\
				(uint8_t *		dst,		\
				 const uint8_t *	src,		\
				 unsigned int		width,		\
				 unsigned int		height,		\
				 unsigned long		dst_padding,	\
				 unsigned long		src_padding)	\
{									\
	while (height-- > 0) {						\
		const uint8_t *end;					\
									\
		for (end = src + width; src < end;) {			\
			v16 ye, yo, u, v;				\
			v16 re, ro, ge, go, be, bo;			\
									\
			load_yuyv16 (&ye, &yo, &u, &v,			\
				     src,  /* offset */ 0,		\
				     TV_PIXFMT_ ## src_fmt);		\
									\
			src += sizeof (vu8) * 2;			\
									\
			fast_yuv2rgb (&re, &ro, &ge, &go, &be, &bo,	\
				      NULL, NULL, NULL, NULL,		\
				      NULL, NULL,			\
				      ye, yo, ye, yo, u, v);		\
									\
			store_rgb16 (dst, /* offset */ 0,		\
				     TV_PIXFMT_ ## dst_fmt,		\
				     /* saturate */ TRUE,		\
				     re, ro, ge, go, be, bo);		\
									\
			dst += sizeof (vu8)				\
				* TV_PIXFMT_BYTES_PER_PIXEL		\
				   (TV_PIXFMT_ ## dst_fmt);		\
		}							\
									\
		src += src_padding;					\
		dst += dst_padding;					\
	}								\
									\
	sfence ();							\
	vempty ();							\
}

#define YUYV_RGB_DST(src_fmt)						\
	YUYV_RGB (RGBA32_LE, src_fmt)					\
	YUYV_RGB (RGBA32_BE, src_fmt)					\
	YUYV_RGB (BGRA32_LE, src_fmt)					\
	YUYV_RGB (BGRA32_BE, src_fmt)					\
	YUYV_RGB (RGB24_LE, src_fmt)					\
	YUYV_RGB (RGB24_BE, src_fmt)					\
	YUYV_RGB (BGR16_LE, src_fmt)					\
	YUYV_RGB (BGR16_BE, src_fmt)					\
	YUYV_RGB (BGRA16_LE, src_fmt)					\
	YUYV_RGB (BGRA16_BE, src_fmt)

#endif /* MMX */

#if !SIMD

#define YUYV_RGB32(dst_fmt, src_fmt, r, g, b, a)			\
static void								\
src_fmt ## _to_ ## dst_fmt ## _loop_SCALAR				\
				(uint8_t *		dst,		\
				 const uint8_t *	src,		\
				 unsigned int		width,		\
				 unsigned int		height,		\
				 unsigned long		dst_padding,	\
				 unsigned long		src_padding)	\
{									\
	/* YUYV, UYVY, YVYU, VYUY */					\
	const unsigned int n = 3 & TV_PIXFMT_ ## src_fmt;		\
									\
	while (height-- > 0) {						\
		const uint8_t *end;					\
									\
		for (end = src + width; src < end;) {			\
			const unsigned int dst_bpp =			\
				TV_PIXFMT_BYTES_PER_PIXEL		\
				   (TV_PIXFMT_ ## dst_fmt);		\
			unsigned int Y, U, V;				\
			int rv, gugv, bu;				\
									\
			U = src[n ^ 1];					\
			V = src[n ^ 3];					\
			gugv = _tv_lut_yuv2rgb_gu[U]			\
				+ _tv_lut_yuv2rgb_gv[V];		\
			rv = _tv_lut_yuv2rgb_rv[V];			\
			bu = _tv_lut_yuv2rgb_bu[U];			\
									\
			Y = src[n & 1];					\
			dst[r + 0] = _tv_lut_yuv2rgb8[Y + rv];		\
			dst[g + 0] = _tv_lut_yuv2rgb8[Y + gugv];	\
			dst[b + 0] = _tv_lut_yuv2rgb8[Y + bu];		\
			if (a >= 0) dst[a + 0] = 0xFF;			\
									\
			Y = src[n | 2];					\
			src += 4;					\
			dst[r + dst_bpp] = _tv_lut_yuv2rgb8[Y + rv];	\
			dst[g + dst_bpp] = _tv_lut_yuv2rgb8[Y + gugv];	\
			dst[b + dst_bpp] = _tv_lut_yuv2rgb8[Y + bu];	\
			if (a >= 0) dst[a + dst_bpp] = 0xFF;		\
			dst += dst_bpp * 2;				\
		}							\
									\
		src += src_padding;					\
		dst += dst_padding;					\
	}								\
}

#define YUYV_RGB16(dst_fmt, src_fmt, lp, c0, c1, c2, alpha1)		\
static void								\
src_fmt ## _to_ ## dst_fmt ## _loop_SCALAR				\
				(uint8_t *		dst,		\
				 const uint8_t *	src,		\
				 unsigned int		width,		\
				 unsigned int		height,		\
				 unsigned long		dst_padding,	\
				 unsigned long		src_padding)	\
{									\
	/* YUYV, UYVY, YVYU, VYUY */					\
	const unsigned int n = 3 & TV_PIXFMT_ ## src_fmt;		\
									\
	while (height-- > 0) {						\
		const uint8_t *end;					\
									\
		for (end = src + width; src < end;) {			\
			uint16_t *dst16 = (uint16_t *) dst;		\
			unsigned int Y, U, V;				\
			int rv, gugv, bu;				\
									\
			U = src[n ^ 1];					\
			V = src[n ^ 3];					\
			gugv = _tv_lut_yuv2rgb_gu[U]			\
				+ _tv_lut_yuv2rgb_gv[V];		\
			rv = _tv_lut_yuv2rgb_rv[V];			\
			bu = _tv_lut_yuv2rgb_bu[U];			\
									\
			Y = src[n & 1];					\
			dst16[0] = (+ lp[c0][Y + rv]			\
				    + lp[c1][Y + gugv]			\
				    + lp[c2][Y + bu]			\
				    + (alpha1));			\
									\
			Y = src[n | 2];					\
			src += 4;					\
			dst16[1] = (+ lp[c0][Y + rv]			\
				    + lp[c1][Y + gugv]			\
				    + lp[c2][Y + bu]			\
				    + (alpha1));			\
			dst += 2 * 2;					\
		}							\
									\
		src += src_padding;					\
		dst += dst_padding;					\
	}								\
}

#define YUYV_RGB_DST(src_fmt)						\
	YUYV_RGB32 (RGBA32_LE, src_fmt, 0, 1, 2, 3)			\
	YUYV_RGB32 (RGBA32_BE, src_fmt, 3, 2, 1, 0)			\
	YUYV_RGB32 (BGRA32_LE, src_fmt, 2, 1, 0, 3)			\
	YUYV_RGB32 (BGRA32_BE, src_fmt, 1, 2, 3, 0)			\
	YUYV_RGB32 (RGB24_LE, src_fmt, 0, 1, 2, -1)			\
	YUYV_RGB32 (RGB24_BE, src_fmt, 2, 1, 0, -1)			\
	YUYV_RGB16 (BGR16_LE, src_fmt,					\
		    _tv_lut_yuv2rgb16[Z_BYTE_ORDER != Z_LITTLE_ENDIAN],	\
		    0, 2, 4, 0)						\
	YUYV_RGB16 (BGR16_BE, src_fmt,					\
		    _tv_lut_yuv2rgb16[Z_BYTE_ORDER != Z_BIG_ENDIAN],	\
		    0, 2, 4, 0)						\
	YUYV_RGB16 (BGRA16_LE, src_fmt,					\
		    _tv_lut_yuv2rgb16[Z_BYTE_ORDER != Z_LITTLE_ENDIAN],	\
		    1, 3, 4,						\
		    (Z_BYTE_ORDER != Z_LITTLE_ENDIAN) ? 0x80 : 0x8000)	\
	YUYV_RGB16 (BGRA16_BE, src_fmt,					\
		    _tv_lut_yuv2rgb16[Z_BYTE_ORDER != Z_BIG_ENDIAN],	\
		    1, 3, 4,						\
		    (Z_BYTE_ORDER != Z_BIG_ENDIAN) ? 0x80 : 0x8000)

#endif /* !SIMD */

YUYV_RGB_DST (YUYV)
YUYV_RGB_DST (UYVY)
YUYV_RGB_DST (YVYU)
YUYV_RGB_DST (VYUY)

#define YUYV_RGB_DST_NAME(src_fmt)					\
        SIMD_NAME (src_fmt ## _to_RGBA32_LE_loop),			\
	SIMD_NAME (src_fmt ## _to_RGBA32_BE_loop),			\
	SIMD_NAME (src_fmt ## _to_BGRA32_LE_loop),			\
	SIMD_NAME (src_fmt ## _to_BGRA32_BE_loop),			\
	SIMD_NAME (src_fmt ## _to_RGB24_LE_loop),			\
	SIMD_NAME (src_fmt ## _to_RGB24_BE_loop),			\
	SIMD_NAME (src_fmt ## _to_BGR16_LE_loop),			\
	SIMD_NAME (src_fmt ## _to_BGR16_BE_loop),			\
	SIMD_NAME (src_fmt ## _to_BGRA16_LE_loop),			\
	SIMD_NAME (src_fmt ## _to_BGRA16_BE_loop)

copy_plane_fn *
SIMD_NAME (yuyv_to_rgb_loops) [4 * 10] = {
	YUYV_RGB_DST_NAME (YUYV),
	YUYV_RGB_DST_NAME (UYVY),
	YUYV_RGB_DST_NAME (YVYU),
	YUYV_RGB_DST_NAME (VYUY),
};

#if !SIMD

static const char
matrix [64] = {
	[TV_PIXFMT_RGBA32_LE] = 0,
	[TV_PIXFMT_RGBA32_BE] = 1,
	[TV_PIXFMT_BGRA32_LE] = 2,
	[TV_PIXFMT_BGRA32_BE] = 3,
	[TV_PIXFMT_RGB24_LE] = 4,
	[TV_PIXFMT_RGB24_BE] = 5,
	[TV_PIXFMT_BGR16_LE] = 6,
	[TV_PIXFMT_BGR16_BE] = 7,
	[TV_PIXFMT_BGRA16_LE] = 8,
	[TV_PIXFMT_BGRA16_BE] = 9
};

tv_bool
_tv_yuyv_to_rgb			(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format)
{
	copy_plane_fn *loop;
	const tv_pixel_format *dst_pf;
	uint8_t *dst;
	const uint8_t *src;
	unsigned int width;
	unsigned int height;
	unsigned long dst_padding;
	unsigned long src_padding;
	unsigned long align;
	unsigned int from;
	unsigned int to;

	width = MIN (dst_format->width, src_format->width);
	height = MIN (dst_format->height, src_format->height);

	if (unlikely (0 == width
		      || 0 == height
		      || (width & 1)))
		return FALSE;

	dst = (uint8_t *) dst_image + dst_format->offset[0];
	src = (const uint8_t *) src_image + src_format->offset[0];

	/* SIMD: 8 / 16 byte aligned. */
	align = (unsigned long) dst;
	align |= (unsigned long) src;

	dst_pf = dst_format->pixel_format;

	dst_padding = dst_format->bytes_per_line[0] -
		((width * dst_pf->bits_per_pixel) >> 3);
	src_padding = src_format->bytes_per_line[0] - width * 2;

	if (likely (0 == (dst_padding | src_padding))) {
		width *= height;
		height = 1;
	} else if (unlikely ((long)(dst_padding | src_padding) < 0)) {
		return FALSE;
	} else {
		align |= (dst_format->bytes_per_line[0] |
			  src_format->bytes_per_line[0]);
	}

	/* SIMD: 8 / 16 pixels at once. */
	align |= width;

	to = matrix[dst_pf->pixfmt];
	from = src_format->pixel_format->pixfmt - TV_PIXFMT_YUYV;

	loop = SIMD_FN_ALIGNED_SELECT (yuyv_to_rgb_loops, align,
				       (CPU_FEATURE_MMX |
					SCALAR))[from * 10 + to];

	loop (dst, src,
	      width * 2 /* src bytes */, height,
	      dst_padding, src_padding);

	return TRUE;
}

#endif /* !SIMD */

typedef void
yuv420_to_rgb_loop_fn		(uint8_t *		dst,
				 const uint8_t *	src,
				 const uint8_t *	usrc,
				 const uint8_t *	vsrc,
				 unsigned int		width,
				 unsigned int		uv_height,
				 unsigned long		dst_bpl,
				 unsigned long		src_bpl,
				 unsigned long		dst_padding,
				 unsigned long		src_padding,
				 unsigned long		usrc_padding,
				 unsigned long		vsrc_padding);

SIMD_FN_ARRAY_PROTOS (yuv420_to_rgb_loop_fn *, yuv420_to_rgb_loops, [10])

#if SIMD & CPU_FEATURE_MMX

#define YUV420_RGB(dst_fmt)						\
static void								\
SIMD_NAME (YUV420_to_## dst_fmt ## _loop)				\
				(uint8_t *		dst,		\
				 const uint8_t *	src,		\
				 const uint8_t *	usrc,		\
				 const uint8_t *	vsrc,		\
				 unsigned int		width,		\
				 unsigned int		uv_height,	\
				 unsigned long		dst_bpl,	\
				 unsigned long		src_bpl,	\
				 unsigned long		dst_padding,	\
				 unsigned long		src_padding,	\
				 unsigned long		usrc_padding,	\
				 unsigned long		vsrc_padding)	\
{									\
	while (uv_height-- > 0) {					\
		const uint8_t *end;					\
									\
		for (end = src + width; src < end;) {			\
			v16 ye0, yo0, ye1, yo1, u, v;			\
			v16 re0, ro0, ge0, go0, be0, bo0;		\
			v16 re1, ro1, ge1, go1, be1, bo1;		\
									\
			u = vunpacklo8 (load_lo (usrc), vzerou8 ());	\
			usrc += sizeof (vu8) / 2;			\
			v = vunpacklo8 (load_lo (vsrc), vzerou8 ());	\
			vsrc += sizeof (vu8) / 2;			\
									\
			load_16 (&ye0, &yo0, src, /* offset */ 0);	\
			load_16 (&ye1, &yo1, src, src_bpl);		\
			src += sizeof (vu8);				\
									\
			fast_yuv2rgb (&re0, &ro0, &ge0, &go0,		\
				      &be0, &bo0,			\
				      &re1, &ro1, &ge1, &go1,		\
				      &be1, &bo1,			\
				      ye0, yo0, ye1, yo1, u, v);	\
									\
			store_rgb16 (dst, /* offset */ 0,		\
				     TV_PIXFMT_ ## dst_fmt,		\
				     /* saturate */ TRUE,		\
				     re0, ro0, ge0, go0, be0, bo0);	\
									\
			store_rgb16 (dst, dst_bpl,			\
				     TV_PIXFMT_ ## dst_fmt,		\
				     /* saturate */ TRUE,		\
				     re1, ro1, ge1, go1, be1, bo1);	\
									\
			dst += sizeof (vu8)				\
				* TV_PIXFMT_BYTES_PER_PIXEL		\
				   (TV_PIXFMT_ ## dst_fmt);		\
		}							\
									\
		usrc += usrc_padding;					\
		vsrc += vsrc_padding;					\
		src += src_padding;					\
		dst += dst_padding;					\
	}								\
									\
	sfence ();							\
	vempty ();							\
}

YUV420_RGB (RGBA32_LE)
YUV420_RGB (RGBA32_BE)
YUV420_RGB (BGRA32_LE)
YUV420_RGB (BGRA32_BE)
YUV420_RGB (RGB24_LE)
YUV420_RGB (RGB24_BE)
YUV420_RGB (BGR16_LE)
YUV420_RGB (BGR16_BE)
YUV420_RGB (BGRA16_LE)
YUV420_RGB (BGRA16_BE)

#endif /* MMX */

#if !SIMD

#define YUV420_RGB32(dst_fmt, r, g, b, a)				\
static void								\
YUV420_to_ ## dst_fmt ## _loop_SCALAR					\
				(uint8_t *		dst,		\
				 const uint8_t *	src,		\
				 const uint8_t *	usrc,		\
				 const uint8_t *	vsrc,		\
				 unsigned int		width,		\
				 unsigned int		uv_height,	\
				 unsigned long		dst_bpl,	\
				 unsigned long		src_bpl,	\
				 unsigned long		dst_padding,	\
				 unsigned long		src_padding,	\
				 unsigned long		usrc_padding,	\
				 unsigned long		vsrc_padding)	\
{									\
	while (uv_height-- > 0) {					\
		const uint8_t *end;					\
									\
		for (end = src + width; src < end;) {			\
			const unsigned int dst_bpp =			\
				TV_PIXFMT_BYTES_PER_PIXEL		\
				   (TV_PIXFMT_ ## dst_fmt);		\
			unsigned int Y, U, V;				\
			int rv, gugv, bu;				\
									\
			U = *usrc++;					\
			V = *vsrc++;					\
			gugv = _tv_lut_yuv2rgb_gu[U]			\
				+ _tv_lut_yuv2rgb_gv[V];		\
			rv = _tv_lut_yuv2rgb_rv[V];			\
			bu = _tv_lut_yuv2rgb_bu[U];			\
									\
			Y = src[0];					\
			dst[r + 0] = _tv_lut_yuv2rgb8[Y + rv];		\
			dst[g + 0] = _tv_lut_yuv2rgb8[Y + gugv];	\
			dst[b + 0] = _tv_lut_yuv2rgb8[Y + bu];		\
			if (a >= 0) dst[a + 0] = 0xFF;			\
									\
			Y = src[1];					\
			dst[r + dst_bpp] = _tv_lut_yuv2rgb8[Y + rv];	\
			dst[g + dst_bpp] = _tv_lut_yuv2rgb8[Y + gugv];	\
			dst[b + dst_bpp] = _tv_lut_yuv2rgb8[Y + bu];	\
			if (a >= 0) dst[a + dst_bpp] = 0xFF;		\
									\
			Y = src[src_bpl];				\
			dst[r + dst_bpl] = _tv_lut_yuv2rgb8[Y + rv];	\
			dst[g + dst_bpl] = _tv_lut_yuv2rgb8[Y + gugv];	\
			dst[b + dst_bpl] = _tv_lut_yuv2rgb8[Y + bu];	\
			if (a >= 0) dst[a + dst_bpl] = 0xFF;		\
									\
			Y = src[1 + src_bpl];				\
			src += 2;					\
			dst[r + dst_bpp + dst_bpl] =			\
				_tv_lut_yuv2rgb8[Y + rv];		\
			dst[g + dst_bpp + dst_bpl] =			\
				_tv_lut_yuv2rgb8[Y + gugv];		\
			dst[b + dst_bpp + dst_bpl] =			\
				_tv_lut_yuv2rgb8[Y + bu];		\
			if (a >= 0) dst[a + dst_bpp + dst_bpl] = 0xFF;	\
			dst += dst_bpp * 2;				\
		}							\
									\
		usrc += usrc_padding;					\
		vsrc += vsrc_padding;					\
		src += src_padding;					\
		dst += dst_padding;					\
	}								\
}

#define YUV420_RGB16(dst_fmt, lp, c0, c1, c2, alpha1)			\
static void								\
YUV420_to_ ## dst_fmt ## _loop_SCALAR					\
				(uint8_t *		dst,		\
				 const uint8_t *	src,		\
				 const uint8_t *	usrc,		\
				 const uint8_t *	vsrc,		\
				 unsigned int		width,		\
				 unsigned int		uv_height,	\
				 unsigned long		dst_bpl,	\
				 unsigned long		src_bpl,	\
				 unsigned long		dst_padding,	\
				 unsigned long		src_padding,	\
				 unsigned long		usrc_padding,	\
				 unsigned long		vsrc_padding)	\
{									\
	while (uv_height-- > 0) {					\
		const uint8_t *end;					\
									\
		for (end = src + width; src < end;) {			\
			uint16_t *dst16;				\
			unsigned int Y, U, V;				\
			int rv, gugv, bu;				\
									\
			U = *usrc++;					\
			V = *vsrc++;					\
			gugv = _tv_lut_yuv2rgb_gu[U]			\
				+ _tv_lut_yuv2rgb_gv[V];		\
			rv = _tv_lut_yuv2rgb_rv[V];			\
			bu = _tv_lut_yuv2rgb_bu[U];			\
									\
			Y = src[0];					\
			dst16 = (uint16_t *) dst;			\
			dst16[0] = (+ lp[c0][Y + rv]			\
				    + lp[c1][Y + gugv]			\
				    + lp[c2][Y + bu]			\
				    + (alpha1));			\
									\
			Y = src[1];					\
			dst16[1] = (+ lp[c0][Y + rv]			\
				    + lp[c1][Y + gugv]			\
				    + lp[c2][Y + bu]			\
				    + (alpha1));			\
									\
			Y = src[src_bpl];				\
			dst16 = (uint16_t *) &dst[dst_bpl];		\
			dst16[0] = (+ lp[c0][Y + rv]			\
				    + lp[c1][Y + gugv]			\
				    + lp[c2][Y + bu]			\
				    + (alpha1));			\
									\
			Y = src[src_bpl + 1];				\
			src += 2;					\
			dst16[1] = (+ lp[c0][Y + rv]			\
				    + lp[c1][Y + gugv]			\
				    + lp[c2][Y + bu]			\
				    + (alpha1));			\
			dst += 2 * 2;					\
		}							\
									\
		usrc += usrc_padding;					\
		vsrc += vsrc_padding;					\
		src += src_padding;					\
		dst += dst_padding;					\
	}								\
}

YUV420_RGB32 (RGBA32_LE, 0, 1, 2, 3)
YUV420_RGB32 (RGBA32_BE, 3, 2, 1, 0)
YUV420_RGB32 (BGRA32_LE, 2, 1, 0, 3)
YUV420_RGB32 (BGRA32_BE, 1, 2, 3, 0)
YUV420_RGB32 (RGB24_LE, 0, 1, 2, -1)
YUV420_RGB32 (RGB24_BE, 2, 1, 0, -1)
YUV420_RGB16 (BGR16_LE, _tv_lut_yuv2rgb16[Z_BYTE_ORDER != Z_LITTLE_ENDIAN],
	      0, 2, 4, 0)
YUV420_RGB16 (BGR16_BE,	_tv_lut_yuv2rgb16[Z_BYTE_ORDER != Z_BIG_ENDIAN],
	      0, 2, 4, 0)
YUV420_RGB16 (BGRA16_LE, _tv_lut_yuv2rgb16[Z_BYTE_ORDER != Z_LITTLE_ENDIAN],
	      1, 3, 4, (Z_BYTE_ORDER != Z_LITTLE_ENDIAN) ? 0x80 : 0x8000)
YUV420_RGB16 (BGRA16_BE, _tv_lut_yuv2rgb16[Z_BYTE_ORDER != Z_BIG_ENDIAN],
	      1, 3, 4, (Z_BYTE_ORDER != Z_BIG_ENDIAN) ? 0x80 : 0x8000)

#endif /* !SIMD */

yuv420_to_rgb_loop_fn *
SIMD_NAME (yuv420_to_rgb_loops) [10] = {
	SIMD_NAME (YUV420_to_RGBA32_LE_loop),
	SIMD_NAME (YUV420_to_RGBA32_BE_loop),
	SIMD_NAME (YUV420_to_BGRA32_LE_loop),
	SIMD_NAME (YUV420_to_BGRA32_BE_loop),
	SIMD_NAME (YUV420_to_RGB24_LE_loop),
	SIMD_NAME (YUV420_to_RGB24_BE_loop),
	SIMD_NAME (YUV420_to_BGR16_LE_loop),
	SIMD_NAME (YUV420_to_BGR16_BE_loop),
	SIMD_NAME (YUV420_to_BGRA16_LE_loop),
	SIMD_NAME (YUV420_to_BGRA16_BE_loop)
};

#if !SIMD

tv_bool
_tv_yuv420_to_rgb		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format)
{
	yuv420_to_rgb_loop_fn *loop;
	const tv_pixel_format *dst_pf;
	uint8_t *dst;
	const uint8_t *src;
	const uint8_t *usrc;
	const uint8_t *vsrc;
	unsigned int width;
	unsigned int height;
	unsigned int uv_width;
	unsigned long dst_bpl;
	unsigned long src_bpl;
	unsigned long dst_padding;
	unsigned long src_padding;
	unsigned long usrc_padding;
	unsigned long vsrc_padding;
	unsigned long align;
	unsigned int to;

	if (TV_FIELD_INTERLACED == src_format->field) {
		tv_image_format d_format;
		tv_image_format s_format;

		/* Convert fields separately to make sure we
		   average U, V of the same parity field. */

		if (unlikely (0 != ((dst_format->height |
				     src_format->height) & 3)))
			return FALSE;

		d_format = *dst_format;
		d_format.height /= 2;
		d_format.bytes_per_line[0] *= 2;

		s_format = *src_format;
		s_format.height /= 2;
		s_format.bytes_per_line[0] *= 2;
		s_format.bytes_per_line[1] *= 2;
		s_format.bytes_per_line[2] *= 2;
		s_format.field = TV_FIELD_PROGRESSIVE;

		/* Convert top field. */

		if (!_tv_yuv420_to_rgb (dst_image, &d_format,
					src_image, &s_format))
			return FALSE;

		/* Convert bottom field. */

		d_format.offset[0] += dst_format->bytes_per_line[0];

		s_format.offset[0] += src_format->bytes_per_line[0];
		s_format.offset[1] += src_format->bytes_per_line[1];
		s_format.offset[2] += src_format->bytes_per_line[2];

		dst_format = &d_format;
		src_format = &s_format;
	}

	width = MIN (dst_format->width, src_format->width);
	height = MIN (dst_format->height, src_format->height);

	if (unlikely (0 == width
		      || 0 == height
		      || ((width | height) & 1)))
		return FALSE;

	/* SIMD: 8 / 16 pixels at once. */
	align = width;

	/* SIMD: 8 / 16 byte aligned. */
	align |= dst_bpl = dst_format->bytes_per_line[0];
	align |= src_bpl = src_format->bytes_per_line[0];

	dst_pf = dst_format->pixel_format;

	dst_padding = dst_bpl -	((width * dst_pf->bits_per_pixel) >> 3);
	src_padding = src_bpl - width;

	usrc = (const uint8_t *) src_image + src_format->offset[1];
	vsrc = (const uint8_t *) src_image + src_format->offset[2];

	align |= (unsigned long) usrc;
	align |= (unsigned long) vsrc;

	uv_width = width >> 1;

	switch (src_format->pixel_format->pixfmt) {
	case TV_PIXFMT_YUV420:
		usrc_padding = src_format->bytes_per_line[1] - uv_width;
		vsrc_padding = src_format->bytes_per_line[2] - uv_width;
		break;

	case TV_PIXFMT_YVU420:
		SWAP (usrc, vsrc);
		vsrc_padding = src_format->bytes_per_line[1] - uv_width;
		usrc_padding = src_format->bytes_per_line[2] - uv_width;
		break;

	default:
		return FALSE;
	}

	align |= src_format->bytes_per_line[1];
	align |= src_format->bytes_per_line[2];

	if (unlikely ((long)(dst_padding |
			     src_padding |
			     usrc_padding |
			     vsrc_padding) < 0))
		return FALSE;

	dst = (uint8_t *) dst_image + dst_format->offset[0];
	src = (const uint8_t *) src_image + src_format->offset[0];

	align |= (unsigned long) dst;
	align |= (unsigned long) src;

	to = matrix[dst_pf->pixfmt];

	loop = SIMD_FN_ALIGNED_SELECT (yuv420_to_rgb_loops, align,
				       (CPU_FEATURE_MMX |
					SCALAR))[to];

	dst_padding += dst_bpl; /* two rows at once */
	src_padding += src_bpl;

	loop (dst, src, usrc, vsrc,
	      width, height >> 1, dst_bpl, src_bpl,
	      dst_padding, src_padding, usrc_padding, vsrc_padding);

	return TRUE;
}

#endif /* !SIMD */

typedef void
nv_to_rgb_loop_fn		(uint8_t *		dst,
				 const uint8_t *	src,
				 const uint8_t *	uvsrc,
				 unsigned int		width,
				 unsigned int		uv_height,
				 unsigned long		dst_bpl,
				 unsigned long		src_bpl,
				 unsigned long		dst_padding,
				 unsigned long		src_padding,
				 unsigned long		uvsrc_padding);

SIMD_FN_ARRAY_PROTOS (nv_to_rgb_loop_fn *, nv_to_rgb_loops, [10])

#if SIMD & CPU_FEATURE_MMX

#define NV_RGB(dst_fmt)							\
static void								\
SIMD_NAME (NV_to_## dst_fmt ## _loop)					\
				(uint8_t *		dst,		\
				 const uint8_t *	src,		\
				 const uint8_t *	uvsrc,		\
				 unsigned int		width,		\
				 unsigned int		uv_height,	\
				 unsigned long		dst_bpl,	\
				 unsigned long		src_bpl,	\
				 unsigned long		dst_padding,	\
				 unsigned long		src_padding,	\
				 unsigned long		uvsrc_padding)	\
{									\
	while (uv_height-- > 0) {					\
		const uint8_t *end;					\
									\
		for (end = src + width; src < end;) {			\
			v16 ye0, yo0, ye1, yo1, u, v;			\
			v16 re0, ro0, ge0, go0, be0, bo0;		\
			v16 re1, ro1, ge1, go1, be1, bo1;		\
									\
			load_16 (&u, &v, uvsrc, /* offset */ 0);	\
			uvsrc += sizeof (vu8);				\
									\
			load_16 (&ye0, &yo0, src, /* offset */ 0);	\
			load_16 (&ye1, &yo1, src, src_bpl);		\
			src += sizeof (vu8);				\
									\
			fast_yuv2rgb (&re0, &ro0, &ge0, &go0,		\
				      &be0, &bo0,			\
				      &re1, &ro1, &ge1, &go1,		\
				      &be1, &bo1,			\
				      ye0, yo0, ye1, yo1, u, v);	\
									\
			store_rgb16 (dst, /* offset */ 0,		\
				     TV_PIXFMT_ ## dst_fmt,		\
				     /* saturate */ TRUE,		\
				     re0, ro0, ge0, go0, be0, bo0);	\
									\
			store_rgb16 (dst, dst_bpl,			\
				     TV_PIXFMT_ ## dst_fmt,		\
				     /* saturate */ TRUE,		\
				     re1, ro1, ge1, go1, be1, bo1);	\
									\
			dst += sizeof (vu8)				\
				* TV_PIXFMT_BYTES_PER_PIXEL		\
				   (TV_PIXFMT_ ## dst_fmt);		\
		}							\
									\
		uvsrc += uvsrc_padding;					\
		src += src_padding;					\
		dst += dst_padding;					\
	}								\
									\
	sfence ();							\
	vempty ();							\
}

NV_RGB (RGBA32_LE)
NV_RGB (RGBA32_BE)
NV_RGB (BGRA32_LE)
NV_RGB (BGRA32_BE)
NV_RGB (RGB24_LE)
NV_RGB (RGB24_BE)
NV_RGB (BGR16_LE)
NV_RGB (BGR16_BE)
NV_RGB (BGRA16_LE)
NV_RGB (BGRA16_BE)

#endif /* MMX */

#if !SIMD

#define NV_RGB32(dst_fmt, r, g, b, a)					\
static void								\
NV_to_ ## dst_fmt ## _loop_SCALAR					\
				(uint8_t *		dst,		\
				 const uint8_t *	src,		\
				 const uint8_t *	uvsrc,		\
				 unsigned int		width,		\
				 unsigned int		uv_height,	\
				 unsigned long		dst_bpl,	\
				 unsigned long		src_bpl,	\
				 unsigned long		dst_padding,	\
				 unsigned long		src_padding,	\
				 unsigned long		uvsrc_padding)	\
{									\
	while (uv_height-- > 0) {					\
		const uint8_t *end;					\
									\
		for (end = src + width; src < end;) {			\
			const unsigned int dst_bpp =			\
				TV_PIXFMT_BYTES_PER_PIXEL		\
				   (TV_PIXFMT_ ## dst_fmt);		\
			unsigned int Y, U, V;				\
			int rv, gugv, bu;				\
									\
			U = uvsrc[0];					\
			V = uvsrc[1];					\
			uvsrc += 2;					\
			gugv = _tv_lut_yuv2rgb_gu[U]			\
				+ _tv_lut_yuv2rgb_gv[V];		\
			rv = _tv_lut_yuv2rgb_rv[V];			\
			bu = _tv_lut_yuv2rgb_bu[U];			\
									\
			Y = src[0];					\
			dst[r + 0] = _tv_lut_yuv2rgb8[Y + rv];		\
			dst[g + 0] = _tv_lut_yuv2rgb8[Y + gugv];	\
			dst[b + 0] = _tv_lut_yuv2rgb8[Y + bu];		\
			if (a >= 0) dst[a + 0] = 0xFF;			\
									\
			Y = src[1];					\
			dst[r + dst_bpp] = _tv_lut_yuv2rgb8[Y + rv];	\
			dst[g + dst_bpp] = _tv_lut_yuv2rgb8[Y + gugv];	\
			dst[b + dst_bpp] = _tv_lut_yuv2rgb8[Y + bu];	\
			if (a >= 0) dst[a + dst_bpp] = 0xFF;		\
									\
			Y = src[src_bpl];				\
			dst[r + dst_bpl] = _tv_lut_yuv2rgb8[Y + rv];	\
			dst[g + dst_bpl] = _tv_lut_yuv2rgb8[Y + gugv];	\
			dst[b + dst_bpl] = _tv_lut_yuv2rgb8[Y + bu];	\
			if (a >= 0) dst[a + dst_bpl] = 0xFF;		\
									\
			Y = src[1 + src_bpl];				\
			src += 2;					\
			dst[r + dst_bpp + dst_bpl] =			\
				_tv_lut_yuv2rgb8[Y + rv];		\
			dst[g + dst_bpp + dst_bpl] =			\
				_tv_lut_yuv2rgb8[Y + gugv];		\
			dst[b + dst_bpp + dst_bpl] =			\
				_tv_lut_yuv2rgb8[Y + bu];		\
			if (a >= 0) dst[a + dst_bpp + dst_bpl] = 0xFF;	\
			dst += dst_bpp * 2;				\
		}							\
									\
		uvsrc += uvsrc_padding;					\
		src += src_padding;					\
		dst += dst_padding;					\
	}								\
}

#define NV_RGB16(dst_fmt, lp, c0, c1, c2, alpha1)			\
static void								\
NV_to_ ## dst_fmt ## _loop_SCALAR					\
				(uint8_t *		dst,		\
				 const uint8_t *	src,		\
				 const uint8_t *	uvsrc,		\
				 unsigned int		width,		\
				 unsigned int		uv_height,	\
				 unsigned long		dst_bpl,	\
				 unsigned long		src_bpl,	\
				 unsigned long		dst_padding,	\
				 unsigned long		src_padding,	\
				 unsigned long		uvsrc_padding)	\
{									\
	while (uv_height-- > 0) {					\
		const uint8_t *end;					\
									\
		for (end = src + width; src < end;) {			\
			uint16_t *dst16;				\
			unsigned int Y, U, V;				\
			int rv, gugv, bu;				\
									\
			U = uvsrc[0];					\
			V = uvsrc[1];					\
			uvsrc += 2;					\
			gugv = _tv_lut_yuv2rgb_gu[U]			\
				+ _tv_lut_yuv2rgb_gv[V];		\
			rv = _tv_lut_yuv2rgb_rv[V];			\
			bu = _tv_lut_yuv2rgb_bu[U];			\
									\
			Y = src[0];					\
			dst16 = (uint16_t *) dst;			\
			dst16[0] = (+ lp[c0][Y + rv]			\
				    + lp[c1][Y + gugv]			\
				    + lp[c2][Y + bu]			\
				    + (alpha1));			\
									\
			Y = src[1];					\
			dst16[1] = (+ lp[c0][Y + rv]			\
				    + lp[c1][Y + gugv]			\
				    + lp[c2][Y + bu]			\
				    + (alpha1));			\
									\
			Y = src[src_bpl];				\
			dst16 = (uint16_t *) &dst[dst_bpl];		\
			dst16[0] = (+ lp[c0][Y + rv]			\
				    + lp[c1][Y + gugv]			\
				    + lp[c2][Y + bu]			\
				    + (alpha1));			\
									\
			Y = src[src_bpl + 1];				\
			src += 2;					\
			dst16[1] = (+ lp[c0][Y + rv]			\
				    + lp[c1][Y + gugv]			\
				    + lp[c2][Y + bu]			\
				    + (alpha1));			\
			dst += 2 * 2;					\
		}							\
									\
		uvsrc += uvsrc_padding;					\
		src += src_padding;					\
		dst += dst_padding;					\
	}								\
}

NV_RGB32 (RGBA32_LE, 0, 1, 2, 3)
NV_RGB32 (RGBA32_BE, 3, 2, 1, 0)
NV_RGB32 (BGRA32_LE, 2, 1, 0, 3)
NV_RGB32 (BGRA32_BE, 1, 2, 3, 0)
NV_RGB32 (RGB24_LE, 0, 1, 2, -1)
NV_RGB32 (RGB24_BE, 2, 1, 0, -1)
NV_RGB16 (BGR16_LE, _tv_lut_yuv2rgb16[Z_BYTE_ORDER != Z_LITTLE_ENDIAN],
	  0, 2, 4, 0)
NV_RGB16 (BGR16_BE, _tv_lut_yuv2rgb16[Z_BYTE_ORDER != Z_BIG_ENDIAN],
	  0, 2, 4, 0)
NV_RGB16 (BGRA16_LE, _tv_lut_yuv2rgb16[Z_BYTE_ORDER != Z_LITTLE_ENDIAN],
	  1, 3, 4, (Z_BYTE_ORDER != Z_LITTLE_ENDIAN) ? 0x80 : 0x8000)
NV_RGB16 (BGRA16_BE, _tv_lut_yuv2rgb16[Z_BYTE_ORDER != Z_BIG_ENDIAN],
	  1, 3, 4, (Z_BYTE_ORDER != Z_BIG_ENDIAN) ? 0x80 : 0x8000)

#endif /* !SIMD */

nv_to_rgb_loop_fn *
SIMD_NAME (nv_to_rgb_loops) [10] = {
	SIMD_NAME (NV_to_RGBA32_LE_loop),
	SIMD_NAME (NV_to_RGBA32_BE_loop),
	SIMD_NAME (NV_to_BGRA32_LE_loop),
	SIMD_NAME (NV_to_BGRA32_BE_loop),
	SIMD_NAME (NV_to_RGB24_LE_loop),
	SIMD_NAME (NV_to_RGB24_BE_loop),
	SIMD_NAME (NV_to_BGR16_LE_loop),
	SIMD_NAME (NV_to_BGR16_BE_loop),
	SIMD_NAME (NV_to_BGRA16_LE_loop),
	SIMD_NAME (NV_to_BGRA16_BE_loop)
};

#if !SIMD

tv_bool
_tv_nv_to_rgb			(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format)
{
	nv_to_rgb_loop_fn *loop;
	const tv_pixel_format *dst_pf;
	uint8_t *dst;
	const uint8_t *src;
	const uint8_t *uvsrc;
	unsigned int width;
	unsigned int height;
	unsigned long dst_bpl;
	unsigned long src_bpl;
	unsigned long uvsrc_bpl;
	unsigned long dst_padding;
	unsigned long src_padding;
	unsigned long uvsrc_padding;
	unsigned long align;
	unsigned int to;

	if (TV_FIELD_INTERLACED == src_format->field) {
		tv_image_format d_format;
		tv_image_format s_format;

		/* Convert fields separately to make sure we
		   average U, V of the same parity field. */

		if (unlikely (0 != ((dst_format->height |
				     src_format->height) & 3)))
			return FALSE;

		d_format = *dst_format;
		d_format.height /= 2;
		d_format.bytes_per_line[0] *= 2;

		s_format = *src_format;
		s_format.height /= 2;
		s_format.bytes_per_line[0] *= 2;
		s_format.bytes_per_line[1] *= 2;
		s_format.bytes_per_line[2] *= 2;
		s_format.field = TV_FIELD_PROGRESSIVE;

		/* Convert top field. */

		if (!_tv_nv_to_rgb (dst_image, &d_format,
				    src_image, &s_format))
			return FALSE;

		/* Convert bottom field. */

		d_format.offset[0] += dst_format->bytes_per_line[0];

		s_format.offset[0] += src_format->bytes_per_line[0];
		s_format.offset[1] += src_format->bytes_per_line[1];
		s_format.offset[2] += src_format->bytes_per_line[2];

		dst_format = &d_format;
		src_format = &s_format;
	}

	width = MIN (dst_format->width, src_format->width);
	height = MIN (dst_format->height, src_format->height);

	if (unlikely (0 == width
		      || 0 == height
		      || ((width | height) & 1)))
		return FALSE;

	/* SIMD: 8 / 16 pixels at once. */
	align = width;

	/* SIMD: 8 / 16 byte aligned. */
	align |= dst_bpl = dst_format->bytes_per_line[0];
	align |= src_bpl = src_format->bytes_per_line[0];
	align |= uvsrc_bpl = src_format->bytes_per_line[1];

	dst_pf = dst_format->pixel_format;

	dst_padding = dst_bpl -	((width * dst_pf->bits_per_pixel) >> 3);
	src_padding = src_bpl - width;
	uvsrc_padding =  uvsrc_bpl - width;

	if (unlikely ((long)(dst_padding |
			     src_padding |
			     uvsrc_padding) < 0))
		return FALSE;

	dst = (uint8_t *) dst_image + dst_format->offset[0];
	src = (const uint8_t *) src_image + src_format->offset[0];
	uvsrc = (const uint8_t *) src_image + src_format->offset[1];

	align |= (unsigned long) dst;
	align |= (unsigned long) src;
	align |= (unsigned long) uvsrc;

	to = matrix[dst_pf->pixfmt];

	loop = SIMD_FN_ALIGNED_SELECT (nv_to_rgb_loops, align,
				       (CPU_FEATURE_MMX |
					SCALAR))[to];

	dst_padding += dst_bpl; /* two rows at once */
	src_padding += src_bpl;

	loop (dst, src, uvsrc,
	      width, height >> 1, dst_bpl, src_bpl,
	      dst_padding, src_padding, uvsrc_padding);

	return TRUE;
}

#endif /* !SIMD */
