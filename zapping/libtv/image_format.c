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

/* $Id: image_format.c,v 1.8 2005-02-06 21:42:07 mschimek Exp $ */

#include <string.h>		/* memset() */
#include <assert.h>
#include "src/cpu.h"
#include "mmx/mmx.h"
#include "sse/sse.h"
#include "avec/avec.h"
#include "image_format.h"
#include "misc.h"

void
_tv_image_format_dump		(const tv_image_format *format,
				 FILE *			fp)
{
	assert (NULL != format);

	fprintf (fp, "width=%u height=%u "
		 "offset=%u,%u,%u,%u bpl=%u,%u,%u,%u "
		 "size=%u pixfmt=%s",
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
				 unsigned int		bytes_per_line,
				 tv_pixfmt		pixfmt,
				 tv_colspc		colspc)
{
	const tv_pixel_format *pf;
	unsigned int min_bpl;

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

	format->width = width;
	format->height = height;

	min_bpl = (width * pf->bits_per_pixel + 7) >> 3;

	format->bytes_per_line[0] = MAX (bytes_per_line, min_bpl);

	format->offset[0] = 0;

	if (pf->planar) {
		unsigned int uv_bpl;
		unsigned int y_size;
		unsigned int uv_size;

		/* No padding. */
		uv_bpl = format->bytes_per_line[0] >> pf->uv_hshift;
		format->bytes_per_line[1] = uv_bpl;
		format->bytes_per_line[2] = uv_bpl;
		format->bytes_per_line[3] = 0;

		y_size = format->bytes_per_line[0] * height;
		uv_size = y_size >> (pf->uv_hshift + pf->uv_vshift);

		if (pf->vu_order) {
			format->offset[1] = y_size + uv_size;
			format->offset[2] = y_size; 
		} else {
			format->offset[1] = y_size; 
			format->offset[2] = y_size + uv_size;
		}

		format->offset[3] = 0;

		format->size = y_size + uv_size * 2;
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
	unsigned int min_bpl;
	unsigned int min_size;

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
		unsigned int min_uv_bytes_per_line;
		unsigned int min_u_size;
		unsigned int min_v_size;
		unsigned int p1_offset;
		unsigned int p2_offset;
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
		min_uv_bytes_per_line = format->width >> pf->uv_hshift;

		if (format->bytes_per_line[1] < min_uv_bytes_per_line
		    || format->bytes_per_line[2] < min_uv_bytes_per_line)
			return FALSE;

		/* We don't enforce bytes_per_line padding on the last line. */
		min_u_size = format->bytes_per_line[1]
			* ((format->height >> pf->uv_vshift) - 1)
			+ min_uv_bytes_per_line;

		min_v_size = format->bytes_per_line[2]
			* ((format->height >> pf->uv_vshift) - 1)
			+ min_uv_bytes_per_line;

		if (pf->vu_order != (format->offset[2] > format->offset[1]))
			return FALSE;

		if (format->offset[1] > format->offset[2]) {
			p1_offset = format->offset[2];
			p2_offset = format->offset[1];
		} else {
			p1_offset = format->offset[1];
			p2_offset = format->offset[2];
		}

		/* Y before U and V, planes must not overlap. */
		if (format->offset[0] + min_size >= p1_offset)
			return FALSE;

		/* U and V planes must not overlap. */
		if (p1_offset + min_u_size >= p2_offset)
			return FALSE;

		/* All planes must fit in buffer. */
		if (p2_offset + min_v_size >= format->size)
			return FALSE;
	} else {
		if (format->offset[0] + min_size >= format->size)
			return FALSE;
	}

	return TRUE;
}

#if Z_BYTE_ORDER == Z_LITTLE_ENDIAN
#  define SWAB16(m) (m)
#  define SWAB32(m) (m)
#elif Z_BYTE_ORDER == Z_BIG_ENDIAN
#  define SWAB16(m)							\
	(+ (((m) & 0xFF00) >> 8)					\
	 + (((m) & 0x00FF) << 8))
#  define SWAB32(m)							\
	(+ (((m) & 0xFF000000) >> 24)					\
	 + (((m) & 0x00FF0000) >> 8)					\
	 + (((m) & 0x0000FF00) << 8)					\
	 + (((m) & 0x000000FF) << 24))
#else
#  error unknown endianess
#endif

static void
clear_block1			(void *			d,
				 unsigned int		value,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned int		bytes_per_line)
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
				 unsigned int		bytes_per_line)
{
	uint8_t *p;
	unsigned int padding;

	padding = bytes_per_line - width * 3;

	if (0 == padding) {
		width *= height;
		height = 1;
	}

	p = (uint8_t *) d;

	for (; height > 0; --height) {
		unsigned int count;

		for (count = width; count > 0; --count) {
			p[0] = value;
			p[1] = value >> 8;
			p[2] = value >> 16;
			p += 3;
		}

		p += padding;
	}
}

static void
clear_block4			(void *			d,
				 unsigned int		value,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned int		bytes_per_line)
{
	uint32_t *p;
	unsigned int padding;

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

	assert (NULL != image);
	assert (NULL != format);

#ifdef HAVE_ALTIVEC
	if (0 /* UNTESTED */ &&
	    0 == ((unsigned long) image | format->bytes_per_line[0]) % 16)
		clear_block = clear_block_altivec;
	else
#endif
#ifdef HAVE_SSE
	if (0 /* UNTESTED */ &&
	    cpu_features & CPU_FEATURE_SSE)
		clear_block = clear_block_mmx_nt;
	else
#endif
#ifdef HAVE_MMX
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
		clear_block[0] (data + format->offset[2], 0x80,
				uv_width, uv_height,
				format->bytes_per_line[2]);

		return TRUE;
	}

	switch (pf->pixfmt) {
	case TV_PIXFMT_NONE:
	case TV_PIXFMT_RESERVED0:
	case TV_PIXFMT_RESERVED1:
	case TV_PIXFMT_RESERVED2:
	case TV_PIXFMT_RESERVED3:
		break;

	case TV_PIXFMT_YUV24_LE:
	case TV_PIXFMT_YVU24_LE:
		/* Note value is always LE: 0xVVUUYY */
		clear_block[2] (data + format->offset[0], 0x808000,
				format->width, format->height,
				format->bytes_per_line[0]);
		return TRUE;

	case TV_PIXFMT_YUV24_BE:
	case TV_PIXFMT_YVU24_BE:
		/* Note value is always LE: 0xYYUUVV */
		clear_block[2] (data + format->offset[0], 0x008080,
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

		clear_block[3] (data + format->offset[0], SWAB32 (0x80008000),
				format->width >> 1, format->height,
				format->bytes_per_line[0]);
		return TRUE;

	case TV_PIXFMT_UYVY:
	case TV_PIXFMT_VYUY:
		if (format->width & 1)
			return FALSE;

		clear_block[3] (data + format->offset[0], SWAB32 (0x00800080),
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
		assert (!"reached");

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
 * XXX unchecked
 */
void
tv_memcpy			(void *			dst,
				 const void *		src,
				 size_t			n_bytes)
{
	if (__builtin_expect (dst == src, FALSE))
		return;

#ifdef HAVE_SSE
	if (0 /* UNTESTED */ &&
	    cpu_features & CPU_FEATURE_SSE)
		if (0 == ((unsigned long) dst | (unsigned long) src) % 16)
			return memcpy_sse_nt (dst, src, n_bytes);
#endif
#ifdef HAVE_MMX
	/* Is this really faster? */
	if (cpu_features & CPU_FEATURE_MMX)
		return memcpy_mmx (dst, src, n_bytes);
#endif
	memcpy (dst, src, n_bytes);
}

static void
copy_block1_generic		(void *			dst,
				 const void *		src,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned int		dst_bytes_per_line,
				 unsigned int		src_bytes_per_line)
{
	unsigned int dst_padding;
	unsigned int src_padding;
	uint8_t *d;
	const uint8_t *s;

	dst_padding = dst_bytes_per_line - width * 1;
	src_padding = src_bytes_per_line - width * 1;

	if (__builtin_expect (0 == (dst_padding | src_padding), TRUE)) {
		memcpy (dst, src, width * height);
		return;
	}

	d = (uint8_t *) dst;
	s = (const uint8_t *) src;

	for (; height > 0; --height) {
		memcpy (d, s, width);

		d += dst_padding;
		s += src_padding;
	}
}

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

	assert (NULL != dst_image);
	assert (NULL != dst_format);

	if (NULL == src_image)
		return tv_clear_image (dst_image, dst_format);

	assert (NULL != src_format);

	assert (dst_format->pixel_format == src_format->pixel_format);

#ifdef HAVE_SSE
	if (cpu_features & CPU_FEATURE_SSE)
		copy_block = copy_block1_sse_nt;
	else
#endif
		copy_block = copy_block1_generic;

	d = (uint8_t *) dst_image;
	s = (const uint8_t *) src_image;

	width = MIN (dst_format->width, src_format->width); 
	height = MIN (dst_format->height, src_format->height); 

	pf = dst_format->pixel_format;

	if (TV_PIXFMT_IS_PLANAR (pf->pixfmt)) {
		unsigned int uv_width;
		unsigned int uv_height;

		uv_width = width >> pf->uv_hshift;
		uv_height = height >> pf->uv_vshift;

		copy_block (d + dst_format->offset[0],
			    s + src_format->offset[0],
			    width, height,
			    dst_format->bytes_per_line[0],
			    src_format->bytes_per_line[0]);

		copy_block (d + dst_format->offset[1],
			    s + src_format->offset[1],
			    uv_width, uv_height,
			    dst_format->bytes_per_line[1],
			    src_format->bytes_per_line[1]);

		copy_block (d + dst_format->offset[2],
			    s + src_format->offset[2],
			    uv_width, uv_height,
			    dst_format->bytes_per_line[2],
			    src_format->bytes_per_line[2]);

		return TRUE;
	}

	copy_block (d + dst_format->offset[0],
		    s + src_format->offset[0],
		    (width * pf->bits_per_pixel) >> 3,
		    height,
		    dst_format->bytes_per_line[0],
		    src_format->bytes_per_line[0]);

	return TRUE;
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
