/*
 *  Copyright (C) 2001-2004 Michael H. Schimek
 *  Copyright (C) 2000-2003 Iñaki García Etxebarria
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

/* $Id: image_format.c,v 1.16 2006-02-25 17:37:42 mschimek Exp $ */

#include <string.h>		/* memset() */
#include <assert.h>
#include "cpu.h"
#include "mmx/mmx.h"
#include "sse/sse.h"
#include "avec/avec.h"
#include "simd-consts.h"
#include "image_format.h"
#include "misc.h"

void
_tv_image_format_dump		(const tv_image_format *format,
				 FILE *			fp)
{
	assert (NULL != format);

	fprintf (fp, "width=%u height=%u "
		 "offset=%lu,%lu,%lu,%lu bpl=%lu,%lu,%lu,%lu "
		 "size=%lu pixfmt=%s",
		 format->width,
		 format->height,
		 format->offset[0],
		 format->offset[1],
		 format->offset[2],
		 format->offset[3],
		 format->bytes_per_line[0],
		 format->bytes_per_line[1],
		 format->bytes_per_line[2],
		 format->bytes_per_line[3],
		 format->size,
		 format->pixel_format->name);
}

tv_bool
tv_image_format_init		(tv_image_format *	format,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		bytes_per_line,
				 tv_pixfmt		pixfmt,
				 tv_colspc		colspc)
{
	const tv_pixel_format *pf;
	unsigned long min_bpl;

	assert (NULL != format);

	if (!(pf = tv_pixel_format_from_pixfmt (pixfmt)))
		return FALSE;

	if (0 == width || 0 == height)
		return FALSE;

	if (pf->planar) {
		unsigned int hres;
		unsigned int vres;

		/* Round size up to U and V scale factors. */

		hres = (1 << pf->uv_hshift) - 1;
		width = (width + hres) & ~hres;

		vres = (1 << pf->uv_vshift) - 1;
		height = (height + vres) & ~vres;
	}

	min_bpl = (width * pf->bits_per_pixel + 7) >> 3;

	if (0 == bytes_per_line) {
		bytes_per_line = min_bpl;
	} else if (bytes_per_line < min_bpl) {
		return FALSE;
	}

	format->width = width;
	format->height = height;

	format->bytes_per_line[0] = bytes_per_line;

	format->offset[0] = 0;

	if (pf->planar) {
		unsigned long uv_bpl;
		unsigned long y_size;
		unsigned long uv_size;

		/* No padding. */
		uv_bpl = format->bytes_per_line[0] >> pf->uv_hshift;

		y_size = format->bytes_per_line[0] * height;
		uv_size = y_size >> (pf->uv_hshift + pf->uv_vshift);

		switch (pixfmt) {
		case TV_PIXFMT_NV12:
			format->bytes_per_line[1] = uv_bpl;
			format->bytes_per_line[2] = 0;
			format->bytes_per_line[3] = 0;

			format->offset[1] = y_size;
			format->offset[2] = 0;
			format->offset[3] = 0;

			format->size = y_size + uv_size;

			break;

		default:
			format->bytes_per_line[1] = uv_bpl;
			format->bytes_per_line[2] = uv_bpl;
			format->bytes_per_line[3] = 0;

			if (pf->vu_order) {
				format->offset[1] = y_size + uv_size;
				format->offset[2] = y_size; 
			} else {
				format->offset[1] = y_size; 
				format->offset[2] = y_size + uv_size;
			}

			format->offset[3] = 0;

			format->size = y_size + uv_size * 2;

			break;
		}
	} else {
		format->bytes_per_line[1] = 0;
		format->bytes_per_line[2] = 0;
		format->bytes_per_line[3] = 0;

		format->offset[1] = 0;
		format->offset[2] = 0;
		format->offset[3] = 0;

		format->size = format->bytes_per_line[0] * height;
	}

	format->pixel_format = tv_pixel_format_from_pixfmt (pixfmt);
	format->colspc = colspc;

	return TRUE;
}

tv_bool
tv_image_format_is_valid	(const tv_image_format *format)
{
	const tv_pixel_format *pf;
	unsigned long min_bpl;
	unsigned long min_size;

	assert (NULL != format);

	pf = format->pixel_format;

	if (0 == format->width
	    || 0 == format->height)
		return FALSE;

	min_bpl = (format->width * pf->bits_per_pixel + 7) >> 3;

	if (format->bytes_per_line[0] < min_bpl)
		return FALSE;

	/* We don't enforce bytes_per_line padding on the last
	   line, in case offset and bytes_per_line were adjusted
	   for cropping. */
	min_size = format->bytes_per_line[0] * (format->height - 1) + min_bpl;

	if (pf->planar) {
		unsigned long min_uv_bpl;
		unsigned long min_uv_size;
		unsigned long min_u_size;
		unsigned long min_v_size;
		unsigned long p1_offset;
		unsigned long p2_offset;
		unsigned int hres;
		unsigned int vres;

		hres = (1 << pf->uv_hshift) - 1;
		vres = (1 << pf->uv_vshift) - 1;

		if (format->width & hres)
			return FALSE;
		if (format->height & vres)
			return FALSE;

		if (format->bytes_per_line[0] & hres)
			return FALSE;

		/* U and V bits_per_pixel assumed 8. */
		min_uv_bpl = format->width >> pf->uv_hshift;

		switch (pf->pixfmt) {
		case TV_PIXFMT_NV12:
			if (format->bytes_per_line[1] < min_uv_bpl)
				return FALSE;

			/* We don't enforce bytes_per_line padding
			   on the last line. */
			min_uv_size = format->bytes_per_line[1]
				* ((format->height >> pf->uv_vshift) - 1)
				+ min_uv_bpl;

			/* Y before UV, planes must not overlap. */
			if (format->offset[0] + min_size > format->offset[1])
				return FALSE;

			/* All planes must fit in buffer. */
			if (format->offset[1] + min_uv_size >= format->size)
				return FALSE;

			break;

		default:
			if (format->bytes_per_line[1] < min_uv_bpl
			    || format->bytes_per_line[2] < min_uv_bpl)
				return FALSE;

			/* We don't enforce bytes_per_line padding
			   on the last line. */
			min_u_size = format->bytes_per_line[1]
				* ((format->height >> pf->uv_vshift) - 1)
				+ min_uv_bpl;

			min_v_size = format->bytes_per_line[2]
				* ((format->height >> pf->uv_vshift) - 1)
				+ min_uv_bpl;

			if (pf->vu_order
			    != (format->offset[2] > format->offset[1]))
				return FALSE;

			if (format->offset[1] > format->offset[2]) {
				p1_offset = format->offset[2];
				p2_offset = format->offset[1];
			} else {
				p1_offset = format->offset[1];
				p2_offset = format->offset[2];
			}

			/* Y before U and V, planes must not overlap. */
			if (format->offset[0] + min_size > p1_offset)
				return FALSE;

			/* U and V planes must not overlap. */
			if (p1_offset + min_u_size > p2_offset)
				return FALSE;

			/* All planes must fit in buffer. */
			if (p2_offset + min_v_size > format->size)
				return FALSE;

			break;
		}
	} else {
		if (format->offset[0] + min_size > format->size)
			return FALSE;
	}

	return TRUE;
}

static void
clear_block1			(void *			d,
				 unsigned int		value,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		bytes_per_line)
{
	if (width == bytes_per_line) {
		memset (d, (int) value, width * height);
	} else {
		for (; height > 0; --height) {
			memset (d, (int) value, width);
			d = (uint8_t *) d + bytes_per_line;
		}
	}
}

static void
clear_block3			(void *			d,
				 unsigned int		value,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		bytes_per_line)
{
	uint8_t *p;
	unsigned long padding;

	padding = bytes_per_line - width * 3;

	if (0 == padding) {
		width *= height;
		height = 1;
	}

	p = (uint8_t *) d;

	for (; height > 0; --height) {
		unsigned int count;

		switch (Z_BYTE_ORDER) {
		case Z_LITTLE_ENDIAN:
			for (count = width; count > 0; --count) {
				p[0] = value;
				p[1] = value >> 8;
				p[2] = value >> 16;
				p += 3;
			}

			break;

		case Z_BIG_ENDIAN:
			for (count = width; count > 0; --count) {
				p[0] = value >> 16;
				p[1] = value >> 8;
				p[2] = value;
				p += 3;
			}

			break;
		}

		p += padding;
	}
}

static void
clear_block4			(void *			d,
				 unsigned int		value,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		bytes_per_line)
{
	uint32_t *p;
	unsigned long padding;

	padding = bytes_per_line - width * 4;

	if (0 == padding) {
		width *= height;
		height = 1;
	}

	p = (uint32_t *) d;

	for (; height > 0; --height) {
		unsigned int count;

		for (count = width; count > 0; --count)
			*p++ = value;

		p = (uint32_t *)((uint8_t *) p + padding);
	}
}

static clear_block_fn *
clear_block_generic [4] = {
	clear_block1,
	NULL,
	clear_block3,
	clear_block4,
};

tv_bool
tv_clear_image			(void *			image,
				 const tv_image_format *format)
{
	const tv_pixel_format *pf;
	clear_block_fn **clear_block;
	tv_pixfmt_set set;
	uint8_t *data;
	unsigned int value;

	assert (NULL != image);
	assert (NULL != format);

#ifdef CAN_COMPILE_ALTIVEC
	if (UNTESTED_SIMD
	    && (cpu_features & CPU_FEATURE_ALTIVEC)
	    && 0 == ((unsigned long) image | format->bytes_per_line[0]) % 16)
		clear_block = clear_block_altivec;
	else
#endif
#ifdef CAN_COMPILE_SSE
	if (UNTESTED_SIMD
	    && (cpu_features & CPU_FEATURE_SSE))
		clear_block = clear_block_mmx_nt;
	else
#endif
#ifdef CAN_COMPILE_MMX
	if (cpu_features & CPU_FEATURE_MMX)
		clear_block = clear_block_mmx;
	else
#endif
		clear_block = clear_block_generic;

	pf = format->pixel_format;

	set = TV_PIXFMT_SET (pf->pixfmt);

	data = (uint8_t *) image;

	if (set & TV_PIXFMT_SET_RGB) {
		clear_block[0] (data + format->offset[0], 0,
				(format->width * pf->bits_per_pixel) >> 3,
				format->height,
				format->bytes_per_line[0]);

		return TRUE;
	}

	if (set & TV_PIXFMT_SET_YUV_PLANAR) {
		unsigned int uv_width;
		unsigned int uv_height;

		uv_width = format->width >> pf->uv_hshift;
		uv_height = format->height >> pf->uv_vshift;

		clear_block[0] (data + format->offset[0], 0x00,
				format->width, format->height,
				format->bytes_per_line[0]);
		clear_block[0] (data + format->offset[1], 0x80,
				uv_width, uv_height,
				format->bytes_per_line[1]);

		if (TV_PIXFMT_NV12 != pf->pixfmt) {
			clear_block[0] (data + format->offset[2], 0x80,
					uv_width, uv_height,
					format->bytes_per_line[2]);
		}

		return TRUE;
	}

	switch (pf->pixfmt) {
	case TV_PIXFMT_NONE:
	case TV_PIXFMT_RESERVED1:
	case TV_PIXFMT_RESERVED2:
	case TV_PIXFMT_RESERVED3:
		break;

	case TV_PIXFMT_YUV24_LE:
	case TV_PIXFMT_YVU24_LE:
		switch (Z_BYTE_ORDER) {
		case Z_LITTLE_ENDIAN: value = 0x808000; break;
		case Z_BIG_ENDIAN:    value = 0x008080; break;
		}

		clear_block[2] (data + format->offset[0], value,
				format->width, format->height,
				format->bytes_per_line[0]);
		return TRUE;

	case TV_PIXFMT_YUV24_BE:
	case TV_PIXFMT_YVU24_BE:
		switch (Z_BYTE_ORDER) {
		case Z_LITTLE_ENDIAN: value = 0x008080; break;
		case Z_BIG_ENDIAN:    value = 0x808000; break;
		}

		clear_block[2] (data + format->offset[0], value,
				format->width, format->height,
				format->bytes_per_line[0]);
		return TRUE;

	case TV_PIXFMT_YUVA32_LE:
	case TV_PIXFMT_YUVA32_BE:
	case TV_PIXFMT_YVUA32_LE:
	case TV_PIXFMT_YVUA32_BE:
		clear_block[3] (data + format->offset[0], 0x00808000,
				format->width, format->height,
				format->bytes_per_line[0]);
		return TRUE;

	case TV_PIXFMT_YUYV:
	case TV_PIXFMT_YVYU:
		if (format->width & 1)
			return FALSE;

		switch (Z_BYTE_ORDER) {
		case Z_LITTLE_ENDIAN: value = 0x80008000; break;
		case Z_BIG_ENDIAN:    value = 0x00800080; break;
		}

		clear_block[3] (data + format->offset[0], value,
				format->width >> 1, format->height,
				format->bytes_per_line[0]);
		return TRUE;

	case TV_PIXFMT_UYVY:
	case TV_PIXFMT_VYUY:
		if (format->width & 1)
			return FALSE;

		switch (Z_BYTE_ORDER) {
		case Z_LITTLE_ENDIAN: value = 0x00800080; break;
		case Z_BIG_ENDIAN:    value = 0x80008000; break;
		}

		clear_block[3] (data + format->offset[0], value,
				format->width >> 1, format->height,
				format->bytes_per_line[0]);
		return TRUE;

	case TV_PIXFMT_Y8:
		clear_block[0] (data + format->offset[0], 0x00,
				format->width, format->height,
				format->bytes_per_line[0]);
		return TRUE;

	case TV_PIXFMT_YUV444:
	case TV_PIXFMT_YVU444:
	case TV_PIXFMT_YUV422:
	case TV_PIXFMT_YVU422:
	case TV_PIXFMT_YUV411:
	case TV_PIXFMT_YVU411:
	case TV_PIXFMT_YUV420:
	case TV_PIXFMT_YVU420:
	case TV_PIXFMT_YUV410:
	case TV_PIXFMT_YVU410:
	case TV_PIXFMT_NV12:
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
		assert (0);

	case TV_PIXFMT_SBGGR:
		clear_block[0] (data + format->offset[0], 0,
				format->width * 2,
				format->height,
				format->bytes_per_line[0]);

		return TRUE;
	}

	return FALSE;
}

/**
 * Copies @a n_bytes from @a src to @a dst using vector instructions and
 * cache bypassing loads and stores, if available. The function works faster
 * if @a src and @a dst are multiples of 8 or 16.
 *
 * XXX unchecked
 */
void
tv_memcpy			(void *			dst,
				 const void *		src,
				 size_t			n_bytes)
{
	if (unlikely (dst == src))
		return;

#ifdef CAN_COMPILE_SSE
	if (UNTESTED_SIMD
	    && (cpu_features & CPU_FEATURE_SSE))
		if (0 == ((unsigned long) dst | (unsigned long) src) % 16)
			return memcpy_sse_nt (dst, src, n_bytes);
#endif
#ifdef CAN_COMPILE_MMX
	/* Is this really faster? */
	if (cpu_features & CPU_FEATURE_MMX)
		return memcpy_mmx (dst, src, n_bytes);
#endif
	memcpy (dst, src, n_bytes);
}

tv_bool
copy_block1_generic		(void *			dst,
				 const void *		src,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		dst_bytes_per_line,
				 unsigned long		src_bytes_per_line)
{
	uint8_t *d;
	const uint8_t *s;
	unsigned long dst_padding;
	unsigned long src_padding;

	dst_padding = dst_bytes_per_line - width;
	src_padding = src_bytes_per_line - width;

	if (unlikely ((long)(dst_padding | src_padding) < 0)) {
		return FALSE;
	} else if (likely (0 == (dst_padding | src_padding))) {
		memcpy (dst, src, width * height);
		return TRUE;
	}

	d = (uint8_t *) dst;
	s = (const uint8_t *) src;

	for (; height > 0; --height) {
		memcpy (d, s, width);

		d += dst_bytes_per_line;
		s += src_bytes_per_line;
	}

	return TRUE;
}

#define offset(start, format, plane, row)				\
	((start) + (format)->offset[plane]				\
	 + (row) * (format)->bytes_per_line[plane])

/*
 * XXX unchecked
 */
tv_bool
tv_copy_image			(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format)
{
	const tv_pixel_format *pf;
	copy_block_fn *copy_block;
	uint8_t *d;
	const uint8_t *s;
	unsigned int width;
	unsigned int height;
	unsigned int y0 = 0;
	unsigned int y1 = -1;

	assert (NULL != dst_image);
	assert (NULL != dst_format);

	if (unlikely (NULL == src_image))
		return tv_clear_image (dst_image, dst_format);

	assert (NULL != src_format);

	assert (dst_format->pixel_format == src_format->pixel_format);

#ifdef CAN_COMPILE_SSE
	if (UNTESTED_SIMD
	    && (cpu_features & CPU_FEATURE_SSE))
		copy_block = copy_block1_sse_nt;
	else
#endif
		copy_block = copy_block1_generic;

	d = (uint8_t *) dst_image;
	s = (const uint8_t *) src_image;

	width = MIN (dst_format->width, src_format->width); 
	height = MIN (dst_format->height, src_format->height); 

	y1 = MIN (y1, height);

	if (unlikely (y0 > y1))
		return FALSE;

	height = y1 - y0;

	assert (width > 0);
	assert (height > 0);

	pf = dst_format->pixel_format;

	if (TV_PIXFMT_IS_PLANAR (pf->pixfmt)) {
		unsigned int uv_width;
		unsigned int uv_height;
		unsigned int uv_y0;

		uv_width = width >> pf->uv_hshift;
		uv_height = height >> pf->uv_vshift;
		uv_y0 = y0 >> pf->uv_vshift;

		if (unlikely (!copy_block (offset (d, dst_format, 1, uv_y0),
					   offset (s, src_format, 1, uv_y0),
					   uv_width, uv_height,
					   dst_format->bytes_per_line[1],
					   src_format->bytes_per_line[1])))
			return FALSE;

		if (TV_PIXFMT_NV12 != pf->pixfmt) {
			if (unlikely
			    (!copy_block (offset (d, dst_format, 2, uv_y0),
					  offset (s, src_format, 2, uv_y0),
					  uv_width, uv_height,
					  dst_format->bytes_per_line[2],
					  src_format->bytes_per_line[2]))) {
				return FALSE;
			}
		}
	} else {
		width = (width * pf->bits_per_pixel) >> 3;
	}

	return copy_block (offset (d, dst_format, 0, y0),
			   offset (s, src_format, 0, y0),
			   width, height,
			   dst_format->bytes_per_line[0],
			   src_format->bytes_per_line[0]);
}

void *
tv_new_image			(const void *		src_image,
				 const tv_image_format *src_format)
{
	void *dst_image;

	assert (NULL != src_format);
	assert (src_format->size > 0);

	dst_image = malloc (src_format->size);

	if (dst_image && src_image) {
		if (!tv_copy_image (dst_image, src_format,
				    src_image, src_format)) {
			free (dst_image);
			dst_image = NULL;
		}
	}

	return dst_image;
}
