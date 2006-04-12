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

/* $Id: yuv2yuv.c,v 1.3 2006-04-12 01:45:56 mschimek Exp $ */

/* YUV to YUV image format conversion functions:

   TV_PIXFMT_YUV420,
   TV_PIXFMT_YVU420,
   TV_PIXFMT_YUYV,
   TV_PIXFMT_YVYU,
   TV_PIXFMT_UYVY,
   TV_PIXFMT_VYUY,
    to
   TV_PIXFMT_YUV420,
   TV_PIXFMT_YVU420,
   TV_PIXFMT_YUYV,
   TV_PIXFMT_YVYU,
   TV_PIXFMT_UYVY,
   TV_PIXFMT_VYUY,

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

#include "copy_image-priv.h"
#include "simd-conv.h"
#include "yuv2yuv.h"

SIMD_FN_ARRAY_PROTOS (copy_plane_fn *, yuyv_to_yuyv_loops, [4 * 4])

#if SIMD == CPU_FEATURE_ALTIVEC

#warning untested

#define SIMD_SHUFFLE(a, b, c, d)					\
void									\
_tv_shuffle_ ## a ## b ## c ## d ## _ALTIVEC				\
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
		for (end = s + width; s < end;) {			\
			static const vu8 sel = {			\
				a +  0, b +  0, c +  0, d +  0,		\
				a +  4, b +  4, c +  4, d +  4,		\
				a +  8, b +  8, c +  8, d +  8,		\
				a + 12, b + 12, c + 12, d + 12 };	\
			vu8 s0, s1;					\
									\
			s0 = vload (s, 0);				\
			s1 = vload (s, sizeof (vu8));			\
			s += sizeof (vu8) * 2;				\
			vstorent (d, 0, vec_perm (s0, s0, sel));	\
			vstorent (d, sizeof (vu8),			\
				  vec_perm (s1, s1, sel));		\
			d += sizeof (vu8) * 2;				\
		}							\
									\
		s += src_padding;					\
		d += dst_padding;					\
	}								\
									\
	sfence ();							\
	vempty ();							\
}

SIMD_SHUFFLE (0, 3, 2, 1)
SIMD_SHUFFLE (1, 0, 3, 2)
SIMD_SHUFFLE (1, 2, 3, 0)
SIMD_SHUFFLE (2, 1, 0, 3)
SIMD_SHUFFLE (2, 1, 3, 0)
SIMD_SHUFFLE (3, 0, 1, 2)
SIMD_SHUFFLE (3, 2, 1, 0)

#elif SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_SSE_INT | CPU_FEATURE_SSE2)

#define ASM_SHUFFLE(name, op)						\
void									\
SIMD_NAME (_tv_shuffle_ ## name) (uint8_t *		dst,		\
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
			uint32_t *d = (uint32_t *) dst;			\
			const uint32_t *s = (const uint32_t *) src;	\
			uint32_t t;					\
									\
			__asm__ (op : "=r" (t) : "0" (s[0])); d[0] = t; \
			__asm__ (op : "=r" (t) : "0" (s[1])); d[1] = t; \
			__asm__ (op : "=r" (t) : "0" (s[2])); d[2] = t; \
			__asm__ (op : "=r" (t) : "0" (s[3])); d[3] = t; \
									\
			src += 16;					\
			dst += 16;					\
		}							\
									\
		src += src_padding;					\
		dst += dst_padding;					\
	}								\
}

#define SIMD_SHUFFLE(name, op)						\
void									\
SIMD_NAME (_tv_shuffle_ ## name) (uint8_t *		dst,		\
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
			vu8 s0;						\
									\
			s0 = vload (src, 0);				\
			vstorent (dst, 0, op);				\
			s0 = vload (src, sizeof (vu8));			\
			src += sizeof (vu8) * 2;			\
			vstorent (dst, sizeof (vu8), op);		\
			dst += sizeof (vu8) * 2;			\
		}							\
									\
		src += src_padding;					\
		dst += dst_padding;					\
	}								\
									\
	sfence ();							\
	vempty ();							\
}

#if SIMD & (CPU_FEATURE_SSE_INT | CPU_FEATURE_SSE2)

#warning untested

SIMD_SHUFFLE (0321, vsel (vsplat16_255, s0, vwswap32 (s0)))
SIMD_SHUFFLE (2103, vsel (vsplat16_255, vwswap32 (s0), s0))

void
SIMD_NAME (_tv_shuffle_2130)	(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		dst_padding,
				 unsigned long		src_padding)
{
	while (height-- > 0) {
		const uint8_t *end;

		for (end = src + width; src < end;) {
			vu8 s0, s1, t0, t1;

			s0 = vload (src, 0);
			s1 = vload (src, sizeof (vu8));
			src += sizeof (vu8) * 2;
			t0 = vunpackhi8 (s0, vzero8 ());
			s0 = vunpacklo8 (s0, vzero8 ());
			t1 = vunpackhi8 (s1, vzero8 ());
			s1 = vunpacklo8 (s1, vzero8 ());

#if SIMD & CPU_FEATURE_SSE_INT
			t0 = _mm_shuffle_pi16 (t0, _MM_SHUFFLE (2, 1, 3, 0));
			s0 = _mm_shuffle_pi16 (s0, _MM_SHUFFLE (2, 1, 3, 0));
			t1 = _mm_shuffle_pi16 (t1, _MM_SHUFFLE (2, 1, 3, 0));
			s1 = _mm_shuffle_pi16 (s1, _MM_SHUFFLE (2, 1, 3, 0));
#else
			t0 = _mm_shuffle_epi16 (t0, _MM_SHUFFLE (2, 1, 3, 0));
			s0 = _mm_shuffle_epi16 (s0, _MM_SHUFFLE (2, 1, 3, 0));
			t1 = _mm_shuffle_epi16 (t1, _MM_SHUFFLE (2, 1, 3, 0));
			s1 = _mm_shuffle_epi16 (s1, _MM_SHUFFLE (2, 1, 3, 0));
#endif
			vstorent (dst, 0, vpacksu16 (s0, t0));
			vstorent (dst, sizeof (vu8), vpacksu16 (s1, t1));
			dst += sizeof (vu8) * 2;
		}

		src += src_padding;
		dst += dst_padding;
	}

	sfence ();
	vempty ();
}

#else /* MMX */

void
_tv_shuffle_0321_MMX		(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		dst_padding,
				 unsigned long		src_padding)
{
	while (height-- > 0) {
		const uint8_t *end;

		for (end = src + width; src < end;) {
			vu8 s0, s1, t0, t1;

			s0 = vload (src, 0);
			s1 = vload (src, sizeof (vu8));
			src += sizeof (vu8) * 2;
			t0 = vunpacklo8 (s0, s1); /* 2D0D2C0C 2B0B2A0A */
			t1 = vunpackhi8 (s0, s1); /* 3D1D3C1C 3B1B3A1A */
			s0 = vunpacklo8 (t0, t1); /* 3B2B1B0B 3A2A1A0A */
			s1 = vunpackhi8 (t0, t1); /* 3D2D1D0D 3C2C1C0C */
			t0 = vunpacklo8 (s0, s1); /* 3C3A2C2A 1C1A0C0A */
			t1 = vunpackhi8 (s1, s0); /* 3B3D2B2D 1B1D0B0D */
			s0 = vunpacklo8 (t0, t1); /*          0B0C0D0A */
			s1 = vunpackhi8 (t0, t1);
			vstorent (dst, 0, s0);
			vstorent (dst, sizeof (vu8), s1);

			dst += sizeof (vu8) * 2;
		}

		src += src_padding;
		dst += dst_padding;
	}

	sfence ();
	vempty ();
}

void
_tv_shuffle_2103_MMX		(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		dst_padding,
				 unsigned long		src_padding)
{
	while (height-- > 0) {
		const uint8_t *end;

		for (end = src + width; src < end;) {
			vu8 s0, s1, t0, t1;

			s0 = vload (src, 0);
			s1 = vload (src, sizeof (vu8));
			src += sizeof (vu8) * 2;
			t0 = vunpacklo8 (s0, s1); /* 1D0D1C0C 1B0B1A0A */
			t1 = vunpackhi8 (s0, s1); /* 3D2D3C2C 3B2B3A2A */
			s0 = vunpacklo8 (t0, t1); /* 3B2B1B0B 3A2A1A0A */
			s1 = vunpackhi8 (t0, t1); /* 3D2D1D0D 3C2C1C0C */
			t0 = vunpacklo8 (s1, s0); /* 3A3C2A2C 1A1C0A0C */
			t1 = vunpackhi8 (s0, s1); /* 3D3B2D2B 1D1B0D0B */
			s0 = vunpacklo8 (t0, t1); /*          0D0A0B0C */
			s1 = vunpackhi8 (t0, t1);
			vstorent (dst, 0, s0);
			vstorent (dst, sizeof (vu8), s1);
			dst += sizeof (vu8) * 2;
		}

		src += src_padding;
		dst += dst_padding;
	}

	sfence ();
	vempty ();
}

void
_tv_shuffle_2130_MMX		(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		dst_padding,
				 unsigned long		src_padding)
{
	while (height-- > 0) {
		const uint8_t *end;

		for (end = src + width; src < end;) {
			vu8 s0, s1, t0, t1, c, d;

			s0 = vload (src, 0);
			s1 = vload (src, sizeof (vu8));
			src += sizeof (vu8) * 2;
			t0 = vunpacklo8 (s0, s1); /* 1D0D1C0C 1B0B1A0A */
			t1 = vunpackhi8 (s0, s1); /* 3D2D3C2C 3B2B3A2A */
			s0 = vunpacklo8 (t0, t1); /* 3B2B1B0B 3A2A1A0A */
			s1 = vunpackhi8 (t0, t1); /* 3D2D1D0D 3C2C1C0C */
			d = vsru (s1, 32);        /*          3D2D1D0D */
			c = vsl (s1, 32);	  /* 3C2C1C0C */
			t0 = vunpackhi8 (c, s0);  /* 3B3C2B2C 1B1C0B0C */
			t1 = vunpacklo8 (d, s0);  /* 3A3D2A2D 1A1D0A0D */
			s0 = vunpacklo8 (t0, t1); /*          0A0D0B0C */
			s1 = vunpackhi8 (t0, t1);
			vstorent (dst, 0, s0);
			vstorent (dst, sizeof (vu8), s0);
			dst += sizeof (vu8) * 2;
		}

		src += src_padding;
		dst += dst_padding;
	}

	sfence ();
	vempty ();
}

#endif /* MMX */

/* XXX rol, ror faster? */
SIMD_SHUFFLE (1230, vor (vsl32 (s0, 24), vsru32 (s0, 8)))
SIMD_SHUFFLE (3012, vor (vsl32 (s0, 8), vsru32 (s0, 24)))

SIMD_SHUFFLE (1032, vbswap16 (s0))

ASM_SHUFFLE (3210, "bswap %0")

#elif !SIMD

#define SHUFFLE(a, b, c, d)						\
void									\
_tv_shuffle_ ## a ## b ## c ## d ## _SCALAR				\
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
			dst[0] = src[a];				\
			dst[1] = src[b];				\
			dst[2] = src[c];				\
			dst[3] = src[d];				\
			src += 4;					\
			dst += 4;					\
		}							\
									\
		src += src_padding;					\
		dst += dst_padding;					\
	}								\
}

SHUFFLE (0, 3, 2, 1)
SHUFFLE (1, 0, 3, 2)
SHUFFLE (1, 2, 3, 0)
SHUFFLE (2, 1, 0, 3)
SHUFFLE (2, 1, 3, 0)
SHUFFLE (3, 0, 1, 2)
SHUFFLE (3, 2, 1, 0)

#endif /* !SIMD */

copy_plane_fn *
SIMD_NAME (yuyv_to_yuyv_loops) [4 * 4] = {
	SIMD_NAME (_tv_copy_plane),	/* YUYV -> YUYV */
	SIMD_NAME (_tv_shuffle_1032),	/* YUYV -> UYVY */
	SIMD_NAME (_tv_shuffle_0321),	/* YUYV -> YVYU */
	SIMD_NAME (_tv_shuffle_3012),	/* YUYV -> VYUY */

	SIMD_NAME (_tv_shuffle_1032),
	SIMD_NAME (_tv_copy_plane),
	SIMD_NAME (_tv_shuffle_1230),
	SIMD_NAME (_tv_shuffle_2103),

	SIMD_NAME (_tv_shuffle_0321),
	SIMD_NAME (_tv_shuffle_3012),
	SIMD_NAME (_tv_copy_plane),
	SIMD_NAME (_tv_shuffle_1032),

	SIMD_NAME (_tv_shuffle_1230),
	SIMD_NAME (_tv_shuffle_2103),
	SIMD_NAME (_tv_shuffle_1032),
	SIMD_NAME (_tv_copy_plane)
};

#if !SIMD

tv_bool
_tv_yuyv_to_yuyv		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format)
{
	copy_plane_fn *loop;
	uint8_t *dst;
	const uint8_t *src;
	unsigned int width;
	unsigned int height;
	unsigned long dst_bpl;
	unsigned long src_bpl;
	unsigned long dst_padding;
	unsigned long src_padding;
	unsigned long align;
	unsigned int to;
	unsigned int from;

	width = MIN (dst_format->width, src_format->width);
	height = MIN (dst_format->height, src_format->height);

	if (unlikely (0 == width || 0 == height
		      || (width & 1)))
		return FALSE;

	dst = (uint8_t *) dst_image + dst_format->offset[0];
	src = (const uint8_t *) src_image + src_format->offset[0];

	/* SIMD: 8 / 16 byte aligned. */
	align = (unsigned long) dst;
	align |= (unsigned long) src;

	dst_bpl = dst_format->bytes_per_line[0];
	src_bpl = src_format->bytes_per_line[0];

	dst_padding = dst_bpl - width * 2;
	src_padding = src_bpl - width * 2;

	if (likely (0 == (dst_padding | src_padding))) {
		width *= height;
		height = 1;
	} else if (unlikely ((long)(dst_padding | src_padding) < 0)) {
		return FALSE;
	} else {
		align |= dst_bpl | src_bpl;
	}

	/* SIMD: 8 / 16 pixels at once. */
	align |= width;

	to = dst_format->pixel_format->pixfmt - TV_PIXFMT_YUYV;
	from = src_format->pixel_format->pixfmt - TV_PIXFMT_YUYV;

	loop = SIMD_FN_ALIGNED_SELECT (yuyv_to_yuyv_loops, align,
				       (CPU_FEATURE_MMX |
					SCALAR))[from * 4 + to];

	loop (dst, src,
	      width * 2 /* bytes */, height,
	      dst_padding, src_padding);

	return TRUE;
}

#endif /* !SIMD */

typedef void
yuyv_to_yuv420_loop_fn		(uint8_t *		dst,
				 uint8_t *		udst,
				 uint8_t *		vdst,
				 const uint8_t *	src,
				 unsigned int		width,
				 unsigned int		uv_height,
				 unsigned long		dst_bpl,
				 unsigned long		src_bpl,
				 unsigned long		dst_padding,
				 unsigned long		udst_padding,
				 unsigned long		vdst_padding,
				 unsigned long		src_padding);

SIMD_FN_ARRAY_PROTOS (yuyv_to_yuv420_loop_fn *, yuyv_to_yuv420_loops, [4])

#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW |			\
	    CPU_FEATURE_SSE_INT | CPU_FEATURE_SSE2 | CPU_FEATURE_ALTIVEC)

#define YUYV_YUV420(n, src_fmt)						\
static void								\
SIMD_NAME (yuyv_to_yuv420_loop_ ## n)					\
				(uint8_t *		dst,		\
				 uint8_t *		udst,		\
				 uint8_t *		vdst,		\
				 const uint8_t *	src,		\
				 unsigned int		width,		\
				 unsigned int		uv_height,	\
				 unsigned long		dst_bpl,	\
				 unsigned long		src_bpl,	\
				 unsigned long		dst_padding,	\
				 unsigned long		udst_padding,	\
				 unsigned long		vdst_padding,	\
				 unsigned long		src_padding)	\
{									\
	while (uv_height-- > 0) {					\
		const uint8_t *end;					\
									\
		for (end = src + width * 2; src < end;) {		\
			vu8 y0, y1, ut, ub, vt, vb;			\
									\
			load_yuyv8 (&y0, &y1, &ut, &vt,			\
				    src, /* offset */ 0,		\
				    TV_PIXFMT_ ## src_fmt);		\
									\
			vstorent (dst, 0, y0);				\
			vstorent (dst, sizeof (vu8), y1);		\
									\
			load_yuyv8 (&y0, &y1, &ub, &vb,			\
				    src, /* offset */ src_bpl,		\
				    TV_PIXFMT_ ## src_fmt);		\
									\
			src += sizeof (vu8) * 4;			\
									\
			vstorent (dst, dst_bpl, y0);			\
			vstorent (dst, sizeof (vu8) + dst_bpl, y1);	\
									\
			dst += sizeof (vu8) * 2;			\
									\
			vstorent (udst, 0, vavgu8 (ut, ub));		\
			vstorent (vdst, 0, vavgu8 (vt, vb));		\
									\
			udst += sizeof (vu8);				\
			vdst += sizeof (vu8);				\
		}							\
									\
		src += src_padding;					\
		dst += dst_padding;					\
		udst += udst_padding;					\
		vdst += vdst_padding;					\
	}								\
									\
	sfence ();							\
	vempty ();							\
}

YUYV_YUV420 (0, YUYV)
YUYV_YUV420 (1, UYVY)

#elif !SIMD

#define YUYV_YUV420(y0, u, y1, v)					\
static void								\
SIMD_NAME (yuyv_to_yuv420_loop_ ## y0)					\
				(uint8_t *		dst,		\
				 uint8_t *		udst,		\
				 uint8_t *		vdst,		\
				 const uint8_t *	src,		\
				 unsigned int		width,		\
				 unsigned int		uv_height,	\
				 unsigned long		dst_bpl,	\
				 unsigned long		src_bpl,	\
				 unsigned long		dst_padding,	\
				 unsigned long		udst_padding,	\
				 unsigned long		vdst_padding,	\
				 unsigned long		src_padding)	\
{									\
	while (uv_height-- > 0) {					\
		const uint8_t *end;					\
									\
		for (end = src + width * 2; src < end;) {		\
			dst[0] = src[y0];				\
			dst[1] = src[y1];				\
			dst[0 + dst_bpl] = src[y0 + src_bpl];		\
			dst[1 + dst_bpl] = src[y1 + src_bpl];		\
			dst += 2;					\
			*udst++ = (src[u] + src[u + src_bpl] + 1) >> 1;	\
			*vdst++ = (src[v] + src[v + src_bpl] + 1) >> 1;	\
			src += 4;					\
		}							\
									\
		dst += dst_padding;					\
		udst += udst_padding;					\
		vdst += vdst_padding;					\
		src += src_padding;					\
	}								\
}

YUYV_YUV420 (0, 1, 2, 3)
YUYV_YUV420 (1, 0, 3, 2)

#endif /* !SIMD */

yuyv_to_yuv420_loop_fn *
SIMD_NAME (yuyv_to_yuv420_loops) [4] = {
	SIMD_NAME (yuyv_to_yuv420_loop_0),
	SIMD_NAME (yuyv_to_yuv420_loop_1),
	SIMD_NAME (yuyv_to_yuv420_loop_0),
	SIMD_NAME (yuyv_to_yuv420_loop_1)
};

#if !SIMD

tv_bool
_tv_yuyv_to_yuv420		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format)
{
	yuyv_to_yuv420_loop_fn *loop;
	const tv_pixel_format *src_pf;
	uint8_t *dst;
	uint8_t *udst;
	uint8_t *vdst;
	const uint8_t *src;
	unsigned int width;
	unsigned int height;
	unsigned int uv_width;
	unsigned long dst_bpl;
	unsigned long src_bpl;
	unsigned long dst_padding;
	unsigned long udst_padding;
	unsigned long vdst_padding;
	unsigned long src_padding;
	unsigned long align;
	unsigned int from;

	if (TV_FIELD_INTERLACED == src_format->field) {
		tv_image_format d_format;
		tv_image_format s_format;

		/* Convert fields separately to make sure we
		   average U, V of the same parity field. */

		if (unlikely (0 != ((dst_format->height | src_format->height) & 3)))
			return FALSE;

		d_format = *dst_format;
		d_format.height /= 2;
		d_format.bytes_per_line[0] *= 2;
		d_format.bytes_per_line[1] *= 2;
		d_format.bytes_per_line[2] *= 2;

		s_format = *src_format;
		s_format.height /= 2;
		s_format.bytes_per_line[0] *= 2;
		s_format.field = TV_FIELD_PROGRESSIVE;

		/* Convert top field. */

		if (!_tv_yuyv_to_yuv420 (dst_image, &d_format,
					 src_image, &s_format))
			return FALSE;

		/* Convert bottom field. */

		d_format.offset[0] += dst_format->bytes_per_line[0];
		d_format.offset[1] += dst_format->bytes_per_line[1];
		d_format.offset[2] += dst_format->bytes_per_line[2];

		s_format.offset[0] += src_format->bytes_per_line[0];

		dst_format = &d_format;
		src_format = &s_format;
	}

	width = MIN (dst_format->width, src_format->width);
	height = MIN (dst_format->height, src_format->height);

	if (unlikely (0 == width
		      || 0 == height
		      || (width | height) & 1))
		return FALSE;

	udst = (uint8_t *) dst_image + dst_format->offset[1];
	vdst = (uint8_t *) dst_image + dst_format->offset[2];

	/* SIMD: 8 / 16 byte aligned. */
	align = (unsigned long) udst;
	align |= (unsigned long) vdst;

	uv_width = width >> 1;

	/* SIMD: 16 / 32 pixels at once. */
	align |= uv_width;

	src_pf = src_format->pixel_format;

	if ((TV_PIXFMT_YVU420 == dst_format->pixel_format->pixfmt)
	    != (TV_PIXFMT_YVYU == src_pf->pixfmt ||
		TV_PIXFMT_VYUY == src_pf->pixfmt)) {
		SWAP (udst, vdst);
		vdst_padding = dst_format->bytes_per_line[1] - uv_width;
		udst_padding = dst_format->bytes_per_line[2] - uv_width;
	} else {
		udst_padding = dst_format->bytes_per_line[1] - uv_width;
		vdst_padding = dst_format->bytes_per_line[2] - uv_width;
	}

	dst_bpl = dst_format->bytes_per_line[0];
	src_bpl = src_format->bytes_per_line[0];

	align |= (dst_format->bytes_per_line[1] |
		  dst_format->bytes_per_line[2] |
		  dst_bpl | src_bpl);

	dst_padding = dst_bpl - width;
	src_padding = src_bpl - width * 2; /* 2 bytes/pixel */

	if (unlikely ((long)(dst_padding |
			     udst_padding |
			     vdst_padding |
			     src_padding) < 0))
		return FALSE;

	dst = (uint8_t *) dst_image + dst_format->offset[0];
	src = (const uint8_t *) src_image + src_format->offset[0];

	align |= (unsigned long) dst;
	align |= (unsigned long) src;

	from = src_pf->pixfmt - TV_PIXFMT_YUYV;

	loop = SIMD_FN_ALIGNED_SELECT (yuyv_to_yuv420_loops, align,
				       (CPU_FEATURE_MMX |
					SCALAR))[from];

	dst_padding += dst_bpl; /* two rows at once */
	src_padding += src_bpl;

	loop (dst, udst, vdst, src,
	      width, height >> 1, dst_bpl, src_bpl,
	      dst_padding, udst_padding, vdst_padding, src_padding);

	return TRUE;
}

#endif /* !SIMD */

typedef void
yuv420_to_yuyv_loop_fn		(uint8_t *		dst,
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

SIMD_FN_ARRAY_PROTOS (yuv420_to_yuyv_loop_fn *, yuv420_to_yuyv_loops, [4])

#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_SSE_INT | CPU_FEATURE_SSE2)

#define YUV420_YUYV_HALF(dst_offset, src_offset, swap_yc)		\
do {									\
	y0 = vload (src, src_offset);					\
	y1 = vload (src, src_offset + sizeof (vu8));			\
									\
	if (swap_yc) {							\
		vstorent (dst, dst_offset, vunpacklo8 (uv0, y0));	\
		vstorent (dst, dst_offset + sizeof (vu8),		\
			  vunpackhi8 (uv0, y0));			\
		vstorent (dst, dst_offset + 2 * sizeof (vu8),		\
			  vunpacklo8 (uv1, y1));			\
		vstorent (dst, dst_offset + 3 * sizeof (vu8),		\
			  vunpackhi8 (uv1, y1));			\
	} else {							\
		vstorent (dst, dst_offset, vunpacklo8 (y0, uv0));	\
		vstorent (dst, dst_offset + sizeof (vu8),		\
			  vunpackhi8 (y0, uv0));			\
		vstorent (dst, dst_offset + 2 * sizeof (vu8),		\
			  vunpacklo8 (y1, uv1));			\
		vstorent (dst, dst_offset + 3 * sizeof (vu8),		\
			  vunpackhi8 (y1, uv1));			\
	}								\
} while (0)

#define YUV420_YUYV(swap_yc)						\
static void								\
SIMD_NAME (yuv420_to_yuyv_loop_ ## swap_yc)				\
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
			vu8 y0, u, y1, v, uv0, uv1;			\
									\
			u = vload (usrc, 0);				\
			v = vload (vsrc, 0);				\
			uv0 = vunpacklo8 (u, v);			\
			uv1 = vunpackhi8 (u, v);			\
			usrc += sizeof (vu8);				\
			vsrc += sizeof (vu8);				\
									\
			YUV420_YUYV_HALF (0, 0, swap_yc);		\
			YUV420_YUYV_HALF (dst_bpl, src_bpl, swap_yc);	\
			src += sizeof (vu8) * 2;			\
			dst += sizeof (vu8) * 4;			\
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

YUV420_YUYV (0)
YUV420_YUYV (1)

#elif !SIMD

#define YUV420_YUYV(y0, u, y1, v)					\
static void								\
SIMD_NAME (yuv420_to_yuyv_loop_ ## y0)					\
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
			unsigned int U;					\
			unsigned int V;					\
									\
			dst[y0] = src[0];				\
			dst[y1] = src[1];				\
			dst[u] = U = *usrc++;				\
			dst[v] = V = *vsrc++;				\
			dst[y0 + dst_bpl] = src[0 + src_bpl];		\
			dst[y1 + dst_bpl] = src[1 + src_bpl];		\
			src += 2;					\
			dst[u + dst_bpl] = U;				\
			dst[v + dst_bpl] = V;				\
			dst += 4;					\
		}							\
									\
		src += src_padding;					\
		usrc += usrc_padding;					\
		vsrc += vsrc_padding;					\
		dst += dst_padding;					\
	}								\
}

YUV420_YUYV (0, 1, 2, 3)
YUV420_YUYV (1, 0, 3, 2)

#endif /* !SIMD */

yuv420_to_yuyv_loop_fn *
SIMD_NAME (yuv420_to_yuyv_loops) [4] = {
	SIMD_NAME (yuv420_to_yuyv_loop_0),
	SIMD_NAME (yuv420_to_yuyv_loop_1),
	SIMD_NAME (yuv420_to_yuyv_loop_0),
	SIMD_NAME (yuv420_to_yuyv_loop_1)
};

#if !SIMD

tv_bool
_tv_yuv420_to_yuyv		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format)
{
	yuv420_to_yuyv_loop_fn *loop;
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

		if ((dst_format->height | src_format->height) & 3)
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

		if (!_tv_yuv420_to_yuyv (dst_image, &d_format,
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
		      || (width | height) & 1))
		return FALSE;

	uv_width = width >> 1;

	/* SIMD: 16 / 32 pixels at once. */
	align = uv_width;

	usrc = (const uint8_t *) src_image + src_format->offset[1];
	vsrc = (const uint8_t *) src_image + src_format->offset[2];

	/* SIMD: 8 / 16 byte aligned. */
	align |= (unsigned long) usrc;
	align |= (unsigned long) vsrc;

	align |= dst_bpl = dst_format->bytes_per_line[0];
	align |= src_bpl = src_format->bytes_per_line[0];

	dst_padding = dst_bpl - width * 2; /* 2 bytes/pixel */
	src_padding = src_bpl - width;

	dst_pf = dst_format->pixel_format;

	if ((TV_PIXFMT_YVU420 == src_format->pixel_format->pixfmt)
	    != (TV_PIXFMT_YVYU == dst_pf->pixfmt ||
		TV_PIXFMT_VYUY == dst_pf->pixfmt)) {
		SWAP (usrc, vsrc);
		vsrc_padding = src_format->bytes_per_line[1] - uv_width;
		usrc_padding = src_format->bytes_per_line[2] - uv_width;
	} else {
		usrc_padding = src_format->bytes_per_line[1] - uv_width;
		vsrc_padding = src_format->bytes_per_line[2] - uv_width;
	}

	align |= (src_format->bytes_per_line[1] |
		  src_format->bytes_per_line[2]);

	if (unlikely ((long)(dst_padding |
			     src_padding |
			     usrc_padding |
			     vsrc_padding) < 0))
		return FALSE;

	dst = (uint8_t *) dst_image + dst_format->offset[0];
	src = (const uint8_t *) src_image + src_format->offset[0];

	align |= (unsigned long) dst;
	align |= (unsigned long) src;

	to = dst_pf->pixfmt - TV_PIXFMT_YUYV;

	loop = SIMD_FN_ALIGNED_SELECT (yuv420_to_yuyv_loops, align,
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

#if !SIMD

tv_bool
_tv_yuv420_to_yuv420		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format)
{
	tv_image_format format;
	unsigned int width;
	unsigned int height;

	width = MIN (dst_format->width, src_format->width);
	height = MIN (dst_format->height, src_format->height);

	/* XXX check that in tv_copy_image(). */
	if (unlikely (0 == width
		      || 0 == height
		      || ((width | height) & 1)))
		return FALSE;

	if (dst_format->pixel_format != src_format->pixel_format) {
		/* YUV <-> YVU 4:2:0. Seems pointless, but some producers /
		   consumers may not support plane offsets and we clip too. */

		format = *src_format;
		format.pixel_format = dst_format->pixel_format;

		SWAP (format.offset[1], format.offset[2]);
		SWAP (format.bytes_per_line[1], format.bytes_per_line[2]);

		src_format = &format;
	}

	return tv_copy_image (dst_image, dst_format,
			      src_image, src_format);
}

#endif /* !SIMD */

typedef void
nv_to_yuyv_loop_fn		(uint8_t *		dst,
				 const uint8_t *	src,
				 const uint8_t *	uvsrc,
				 unsigned int		width,
				 unsigned int		uv_height,
				 unsigned long		dst_bpl,
				 unsigned long		src_bpl,
				 unsigned long		dst_padding,
				 unsigned long		src_padding,
				 unsigned long		uvsrc_padding);

SIMD_FN_ARRAY_PROTOS (nv_to_yuyv_loop_fn *, nv_to_yuyv_loops, [4])

#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_SSE_INT | CPU_FEATURE_SSE2)

#define NV_YUYV_HALF(dst_offset, src_offset, swap_yc)			\
do {									\
	y = vload (src, src_offset);					\
									\
	if (swap_yc) {							\
		vstorent (dst, dst_offset, vunpacklo8 (uv, y));		\
		vstorent (dst, dst_offset + sizeof (vu8),		\
			  vunpackhi8 (uv, y));				\
	} else {							\
		vstorent (dst, dst_offset, vunpacklo8 (y, uv));		\
		vstorent (dst, dst_offset + sizeof (vu8),		\
			  vunpackhi8 (y, uv));				\
	}								\
} while (0)

#define NV_YUYV(fmt, swap_yc, swap_uv)					\
static void								\
SIMD_NAME (NV12_to_ ## fmt ## _loop)					\
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
			vu8 y, uv;					\
									\
			uv = vload (uvsrc, 0);				\
			uvsrc += sizeof (vu8);				\
			if (swap_uv)					\
				uv = vbswap16 (uv);			\
			NV_YUYV_HALF (0, 0, swap_yc);			\
			NV_YUYV_HALF (dst_bpl, src_bpl, swap_yc);	\
			src += sizeof (vu8);				\
			dst += sizeof (vu8) * 2;			\
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

NV_YUYV (YUYV, 0, 0)
NV_YUYV (UYVY, 1, 0)
NV_YUYV (YVYU, 0, 1)
NV_YUYV (VYUY, 1, 1)

#elif !SIMD

#define NV_YUYV(fmt, y0, u, y1, v)					\
static void								\
NV12_to_ ## fmt ## _loop_SCALAR						\
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
			unsigned int U;					\
			unsigned int V;					\
									\
			dst[y0] = src[0];				\
			dst[y1] = src[1];				\
			dst[u] = U = uvsrc[0];				\
			dst[v] = V = uvsrc[1];				\
			uvsrc += 2;					\
			dst[y0 + dst_bpl] = src[0 + src_bpl];		\
			dst[y1 + dst_bpl] = src[1 + src_bpl];		\
			src += 2;					\
			dst[u + dst_bpl] = U;				\
			dst[v + dst_bpl] = V;				\
			dst += 4;					\
		}							\
									\
		uvsrc += uvsrc_padding;					\
		src += src_padding;					\
		dst += dst_padding;					\
	}								\
}

NV_YUYV (YUYV, 0, 1, 2, 3)
NV_YUYV (UYVY, 1, 0, 3, 2)
NV_YUYV (YVYU, 0, 3, 2, 1)
NV_YUYV (VYUY, 1, 2, 3, 0)

#endif /* !SIMD */

nv_to_yuyv_loop_fn *
SIMD_NAME (nv_to_yuyv_loops) [4] = {
	SIMD_NAME (NV12_to_YUYV_loop),
	SIMD_NAME (NV12_to_UYVY_loop),
	SIMD_NAME (NV12_to_YVYU_loop),
	SIMD_NAME (NV12_to_VYUY_loop),
};

#if !SIMD

tv_bool
_tv_nv_to_yuyv			(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format)
{
	nv_to_yuyv_loop_fn *loop;
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

		if ((dst_format->height | src_format->height) & 3)
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

		if (!_tv_nv_to_yuyv (dst_image, &d_format,
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

	if (unlikely (0 == width || 0 == height
		      || (width | height) & 1))
		return FALSE;

	/* SIMD: 8 / 16 pixels at once. */
	align = width;

	/* SIMD: 8 / 16 byte aligned. */
	align |= dst_bpl = dst_format->bytes_per_line[0];
	align |= src_bpl = src_format->bytes_per_line[0];
	align |= uvsrc_bpl = src_format->bytes_per_line[1];

	dst_padding = dst_bpl - width * 2; /* 2 bytes/pixel */
	src_padding = src_bpl - width;
	uvsrc_padding = uvsrc_bpl - width;

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

	to = dst_format->pixel_format->pixfmt - TV_PIXFMT_YUYV;

	loop = SIMD_FN_ALIGNED_SELECT (nv_to_yuyv_loops, align,
				       (CPU_FEATURE_MMX |
					SCALAR))[to];

	dst_padding += dst_bpl; /* two rows at once */
	src_padding += src_bpl;

	loop (dst, src, uvsrc,
	      width, height >> 1, dst_bpl, src_bpl,
	      dst_padding, src_padding, uvsrc_padding);

	return TRUE;
}

tv_bool
_tv_nv_to_yuv420		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format)
{
	uint8_t *dst;
	uint8_t *udst;
	uint8_t *vdst;
	const uint8_t *src;
	const uint8_t *uvsrc;
	unsigned int width;
	unsigned int height;
	unsigned int uv_width;
	unsigned long dst_padding;
	unsigned long udst_padding;
	unsigned long vdst_padding;
	unsigned long src_padding;
	unsigned long uvsrc_padding;

	width = MIN (dst_format->width, src_format->width);
	height = MIN (dst_format->height, src_format->height);

	if (unlikely (0 == width
		      || 0 == height
		      || (width | height) & 1))
		return FALSE;

	dst_padding = dst_format->bytes_per_line[0] - width;
	src_padding = src_format->bytes_per_line[0] - width;
	uvsrc_padding = src_format->bytes_per_line[1] - width;

	uv_width = width >> 1;

	udst_padding = dst_format->bytes_per_line[1] - uv_width;
	vdst_padding = dst_format->bytes_per_line[2] - uv_width;

	if (unlikely ((long)(dst_padding |
			     src_padding |
			     uvsrc_padding |
			     udst_padding |
			     vdst_padding) < 0))
		return FALSE;

	dst = (uint8_t *) dst_image + dst_format->offset[0];
	src = (const uint8_t *) src_image + src_format->offset[0];

	if (likely (0 == (dst_padding | src_padding))) {
		memcpy (dst, src, width * height);
	} else {
		_tv_copy_plane_SCALAR (dst, src, width, height,
				       dst_padding, src_padding);
	}

	udst = (uint8_t *) dst_image + dst_format->offset[1];
	vdst = (uint8_t *) dst_image + dst_format->offset[2];

	if (TV_PIXFMT_YVU420 == dst_format->pixel_format->pixfmt) {
		SWAP (udst, vdst);
		SWAP (udst_padding, vdst_padding);
	}

	uvsrc = (const uint8_t *) src_image + src_format->offset[1];

	for (height >>= 1; height > 0; --height) {
		const uint8_t *end;

		for (end = uvsrc + width; uvsrc < end; uvsrc += 2) {
			*udst++ = uvsrc[0];
			*vdst++ = uvsrc[1];
		}

		udst += udst_padding;
		vdst += vdst_padding;
		uvsrc += uvsrc_padding;
	}

	return TRUE;
}

#endif /* !SIMD */
