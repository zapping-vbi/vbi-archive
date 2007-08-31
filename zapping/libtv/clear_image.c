/*
 *  Copyright (C) 2001-2004, 2006 Michael H. Schimek
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

/* $Id: clear_image.c,v 1.4 2007-08-31 05:52:31 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>		/* memset() */
#include "clear_image.h"
#include "misc.h"
#include "simd.h"

typedef void
clear_plane_3_fn		(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		padding);

SIMD_FN_PROTOS (clear_plane_fn, _tv_clear_plane_1)
SIMD_FN_PROTOS (clear_plane_fn, _tv_clear_plane_4)
SIMD_FN_PROTOS (clear_plane_3_fn, _tv_clear_plane_3)

#if SIMD

#  if SIMD == CPU_FEATURE_SSE_INT
     /* On a SSE machine we pretend to copy float vectors, so we
	can use 128 bit non-temporal moves. */
#    define vu8 vf
#    undef vloadnt
#    define vloadnt vloadfnt
#    undef vstorent
#    define vstorent vstorefnt
#    undef emms
#    define emms() /* nothing */
#  endif

void
SIMD_NAME (_tv_clear_plane_1)	(uint8_t *		dst,
				 unsigned int		value,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		padding)
{
	vu8 m0;

#if SIMD == CPU_FEATURE_MMX
	m0 = vsplatu32 (value);
#elif SIMD == CPU_FEATURE_SSE_INT
	{
		union {
			uint32_t	i4[4];
			vf		v4;
		} __attribute__ ((aligned (16))) val;

		val.i4[0] = value;
		val.i4[1] = value;
		val.i4[2] = value;
		val.i4[3] = value;

		/* We pretend val.v4 is a float vector, so we can
		   use 128 bit non-temporal moves. Real 128 bit int
		   ops were introduced with SSE2. */
		m0 = vloadfnt (&val.v4, 0);
  	}
#elif SIMD == CPU_FEATURE_ALTIVEC
	m0 = (vu8)((vector int){ value, value, value, value });
#endif

	do {
		uint8_t *end1;
		uint8_t *end2;

		end1 = dst + (width & (-8 * sizeof (vu8)));
		end2 = dst + width;

		for (; dst < end1; dst += 8 * sizeof (vu8)) {
			vstorent (dst, 0 * sizeof (vu8), m0);
			vstorent (dst, 1 * sizeof (vu8), m0);
			vstorent (dst, 2 * sizeof (vu8), m0);
			vstorent (dst, 3 * sizeof (vu8), m0);
			vstorent (dst, 4 * sizeof (vu8), m0);
			vstorent (dst, 5 * sizeof (vu8), m0);
			vstorent (dst, 6 * sizeof (vu8), m0);
			vstorent (dst, 7 * sizeof (vu8), m0);
		}

		for (; dst < end2; dst += sizeof (vu8))
			vstorent (dst, 0, m0);

		dst += padding;
	} while (--height > 0);

	sfence ();
	vempty ();
}

void
SIMD_NAME (_tv_clear_plane_3)	(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		padding)
{
	vu8 m0, m1, m2;

	m0 = vloadnt (src, 0 * sizeof (vu8));
	m1 = vloadnt (src, 1 * sizeof (vu8));
	m2 = vloadnt (src, 2 * sizeof (vu8));

	do {
		uint8_t *end = dst + width * 3;

		do {
			vstorent (dst, 0 * sizeof (vu8), m0);
			vstorent (dst, 1 * sizeof (vu8), m1);
			vstorent (dst, 2 * sizeof (vu8), m2);
			dst += 3 * sizeof (vu8);
		} while (dst < end);

		dst += padding;
	} while (--height > 0);

	sfence ();
	vempty ();
}

void
SIMD_NAME (_tv_clear_plane_4)	(uint8_t *		dst,
				 unsigned int		value,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		padding)
{
	SIMD_NAME (_tv_clear_plane_1) (dst, value, width * 4, height, padding);
}

#else /* !SIMD */

#define IMPL (SCALAR |							\
	      CPU_FEATURE_MMX |						\
	      CPU_FEATURE_SSE_FLT |					\
	      CPU_FEATURE_ALTIVEC)

static const uint8_t __attribute__ ((aligned (16)))
yuv3 [4 * 16] = {
	0x00, 0x80, 0x80, 0x00, 0x80, 0x80, 0x00, 0x80,
	0x80, 0x00, 0x80, 0x80, 0x00, 0x80, 0x80, 0x00,

	0x80, 0x80, 0x00, 0x80, 0x80, 0x00, 0x80, 0x80,
	0x00, 0x80, 0x80, 0x00, 0x80, 0x80, 0x00, 0x80,
	0x80, 0x00, 0x80, 0x80, 0x00, 0x80, 0x80, 0x00,
	0x80, 0x80, 0x00, 0x80, 0x80, 0x00, 0x80, 0x80,

	0x00, 0x80, 0x80, 0x00, 0x80, 0x80, 0x00, 0x80,
	0x80, 0x00, 0x80, 0x80, 0x00, 0x80, 0x80, 0x00,
};

void
_tv_clear_plane_1_SCALAR	(uint8_t *		dst,
				 unsigned int		value,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		padding)
{

	do {
		memset (dst, (int) value, width);
		dst += width + padding;
	} while (--height > 0);
}

void
_tv_clear_plane_3_SCALAR	(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		padding)
{
	int m0, m1, m2;

	m0 = src[0];
	m1 = src[1];
	m2 = src[2];

	do {
		unsigned int count = width;

		do {
			dst[0] = m0;
			dst[1] = m1;
			dst[2] = m2;
			dst += 3;
		} while (--count > 0);

		dst += padding;
	} while (--height > 0);
}

void
_tv_clear_plane_4_SCALAR	(uint8_t *		dst,
				 unsigned int		value,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		padding)
{
	uint32_t *dst32 = (uint32_t *) dst;

	do {
		unsigned int count = width;

		do {
			*dst32++ = value;
		} while (--count > 0);

		dst32 = (uint32_t *)((uint8_t *) dst32 + padding);
	} while (--height > 0);
}

tv_bool
tv_clear_image			(void *			image,
				 const tv_image_format *format)
{
	clear_plane_fn *clear_plane;
	clear_plane_3_fn *clear_plane_3;
	const tv_pixel_format *pf;
	uint8_t *dst;
	const uint8_t *src;
	unsigned int width;
	unsigned int height;
	unsigned long padding;
	unsigned long align;
	unsigned int value;

	assert (NULL != image);
	assert (NULL != format);

	width = format->width;
	height = format->height;

	if (unlikely (0 == width || 0 == height))
		return TRUE;

	pf = format->pixel_format;
	if (unlikely (NULL == pf))
		return FALSE;

	switch (pf->pixfmt) {
	case TV_PIXFMT_NONE:
	case TV_PIXFMT_RESERVED1:
	case TV_PIXFMT_RESERVED3:
		break;

	case TV_PIXFMT_YUV24_LE:
	case TV_PIXFMT_YVU24_LE:
		switch (Z_BYTE_ORDER) {
		case Z_LITTLE_ENDIAN: src = yuv3; break;
		case Z_BIG_ENDIAN:    src = &yuv3[16]; break;
		}

		goto clear3;

	case TV_PIXFMT_YUV24_BE:
	case TV_PIXFMT_YVU24_BE:
		switch (Z_BYTE_ORDER) {
		case Z_LITTLE_ENDIAN: src = &yuv3[16]; break;
		case Z_BIG_ENDIAN:    src = yuv3; break;
		}

	clear3:
		dst = (uint8_t *) image + format->offset[0];
		align = (unsigned long) dst;

		padding = format->bytes_per_line[0] - width * 3;

		if (likely (0 == padding)) {
			width *= height;
			height = 1;
		} else if (likely ((long) padding > 0)) {
			align |= format->bytes_per_line[1];
		} else {
			return FALSE;
		}

		align |= width;

		clear_plane_3 = SIMD_FN_ALIGNED_SELECT
			(_tv_clear_plane_3, align, IMPL);

		clear_plane_3 (dst, src, width, height, padding);

		return TRUE;

	case TV_PIXFMT_YUVA32_LE:
	case TV_PIXFMT_YUVA32_BE:
	case TV_PIXFMT_YVUA32_LE:
	case TV_PIXFMT_YVUA32_BE:
		value = 0x00808000;
		goto clear4;

	case TV_PIXFMT_YUYV:
	case TV_PIXFMT_YVYU:
		if (unlikely (width & 1))
			return FALSE;

		switch (Z_BYTE_ORDER) {
		case Z_LITTLE_ENDIAN: value = 0x80008000; break;
		case Z_BIG_ENDIAN:    value = 0x00800080; break;
		}

		goto clear2;

	case TV_PIXFMT_UYVY:
	case TV_PIXFMT_VYUY:
		if (unlikely (width & 1))
			return FALSE;

		switch (Z_BYTE_ORDER) {
		case Z_LITTLE_ENDIAN: value = 0x00800080; break;
		case Z_BIG_ENDIAN:    value = 0x80008000; break;
		}

	clear2:
		width >>= 1;

	clear4:
		dst = (uint8_t *) image + format->offset[0];
		align = (unsigned long) dst;

		padding = format->bytes_per_line[0] - width * 4;

		if (likely (0 == padding)) {
			width *= height;
			height = 1;
		} else if (likely ((long) padding > 0)) {
			align |= format->bytes_per_line[0];
		} else {
			return FALSE;
		}

		align |= width;

		clear_plane = SIMD_FN_ALIGNED_SELECT
			(_tv_clear_plane_4, align, IMPL);

		clear_plane (dst, value, width, height, padding);

		return TRUE;

	case TV_PIXFMT_YUV422:
	case TV_PIXFMT_YVU422:
		if (unlikely (0 != (width & 1)))
			return FALSE;
		
		goto clear_yuv_planar;

	case TV_PIXFMT_YUV411:
	case TV_PIXFMT_YVU411:
		if (unlikely (0 != (width & 3)))
			return FALSE;
		
		goto clear_yuv_planar;

	case TV_PIXFMT_YUV420:
	case TV_PIXFMT_YVU420:
	case TV_PIXFMT_NV12:
	case TV_PIXFMT_HM12:
		if (unlikely (0 != ((width | height) & 1)))
			return FALSE;
		
		goto clear_yuv_planar;

	case TV_PIXFMT_YUV410:
	case TV_PIXFMT_YVU410:
		if (unlikely (0 != ((width | height) & 3)))
			return FALSE;

		goto clear_yuv_planar;

	case TV_PIXFMT_YUV444:
	case TV_PIXFMT_YVU444:
	clear_yuv_planar:
		width >>= pf->uv_hshift;
		height >>= pf->uv_vshift;

		if (unlikely (width <= 0 || height <= 0))
			return FALSE;

		if (3 == pf->n_planes) {
			if (likely ((format->bytes_per_line[1]
				     == format->bytes_per_line[2])
				    && (format->offset[1] +
					height * format->bytes_per_line[1]
					== format->offset[2]))) {
				height *= 2;
			} else {
				unsigned int v_width;
				unsigned int v_height;

				v_width = width;
				v_height = height;

				dst = (uint8_t *) image + format->offset[2];
				align = (unsigned long) dst;

				padding = format->bytes_per_line[2] - v_width;

				if (likely (0 == padding)) {
					v_width *= v_height;
					v_height = 1;
				} else if (likely ((long) padding > 0)) {
					align |= format->bytes_per_line[2];
				} else {
					return FALSE;
				}

				align |= v_width;

				clear_plane = SIMD_FN_ALIGNED_SELECT
					(_tv_clear_plane_1, align, IMPL);

				clear_plane (dst, 0x80808080,
					     v_width, v_height,
					     padding);
			}
		}

		dst = (uint8_t *) image + format->offset[1];
		align = (unsigned long) dst;

		padding = format->bytes_per_line[1] - width;

		if (likely (0 == padding)) {
			width *= height;
			height = 1;
		} else if (likely ((long) padding > 0)) {
			align |= format->bytes_per_line[1];
		} else {
			return FALSE;
		}

		align |= width;

		clear_plane = SIMD_FN_ALIGNED_SELECT
			(_tv_clear_plane_1, align, IMPL);

		clear_plane (dst, 0x80808080, width, height, padding);

		width = format->width;
		height = format->height;

		goto clear1;

	case TV_PIXFMT_Y8:
		goto clear1;

	case TV_PIXFMT_RGBA32_LE:
	case TV_PIXFMT_RGBA32_BE:
	case TV_PIXFMT_BGRA32_LE:
	case TV_PIXFMT_BGRA32_BE:
	case TV_PIXFMT_RGB24_LE:
	case TV_PIXFMT_BGR24_LE:
	case TV_PIXFMT_RGB16_LE:
	case TV_PIXFMT_RGB16_BE:
	case TV_PIXFMT_BGR16_LE:
	case TV_PIXFMT_BGR16_BE:
	case TV_PIXFMT_RGBA16_LE:
	case TV_PIXFMT_RGBA16_BE:
	case TV_PIXFMT_BGRA16_LE:
	case TV_PIXFMT_BGRA16_BE:
	case TV_PIXFMT_ARGB16_LE:
	case TV_PIXFMT_ARGB16_BE:
	case TV_PIXFMT_ABGR16_LE:
	case TV_PIXFMT_ABGR16_BE:
	case TV_PIXFMT_RGBA12_LE:
	case TV_PIXFMT_RGBA12_BE:
	case TV_PIXFMT_BGRA12_LE:
	case TV_PIXFMT_BGRA12_BE:
	case TV_PIXFMT_ARGB12_LE:
	case TV_PIXFMT_ARGB12_BE:
	case TV_PIXFMT_ABGR12_LE:
	case TV_PIXFMT_ABGR12_BE:
	case TV_PIXFMT_RGB8:
	case TV_PIXFMT_BGR8:
	case TV_PIXFMT_RGBA8:
	case TV_PIXFMT_BGRA8:
	case TV_PIXFMT_ARGB8:
	case TV_PIXFMT_ABGR8:
		goto clear1;

	case TV_PIXFMT_SBGGR:
		if (unlikely ((width | height) & 1))
			return FALSE;

		/* fall through */

	clear1:
		width = (width * pf->bits_per_pixel) >> 3;
		if (unlikely (width <= 0))
			return FALSE;

		dst = (uint8_t *) image + format->offset[0];
		align = (unsigned long) dst;

		padding = format->bytes_per_line[0] - width;

		if (likely (0 == padding)) {
			width *= height;
			height = 1;
		} else if (likely ((long) padding > 0)) {
			align |= format->bytes_per_line[0];
		} else {
			return FALSE;
		}

		align |= width;

		clear_plane = SIMD_FN_ALIGNED_SELECT
			(_tv_clear_plane_1, align, IMPL);

		clear_plane (dst, 0, width, height, padding);

		return TRUE;
	}

	return FALSE;
}

#endif /* !SIMD */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
