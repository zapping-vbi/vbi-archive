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

/* $Id: image_format.c,v 1.3 2004-12-07 17:30:39 mschimek Exp $ */

#include <string.h>		/* memset() */
#include <assert.h>
#include "misc.h"
#include "image_format.h"

void
_tv_image_format_dump		(const tv_image_format *format,
				 FILE *			fp)
{
	assert (NULL != format);

	fprintf (fp, "width=%u height=%u "
		 "bpl=%u,%u offset=%u,%u,%u "
		 "size=%u pixfmt=%s",
		 format->width, format->height,
		 format->bytes_per_line, format->uv_bytes_per_line,
		 format->offset, format->u_offset, format->v_offset,
		 format->size, tv_pixfmt_name (format->pixfmt));
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

	format->bytes_per_line = MAX (bytes_per_line, min_bpl);

	format->offset = 0;

	if (pf->planar) {
		unsigned int y_size;
		unsigned int uv_size;

		/* No padding. */
		format->uv_bytes_per_line =
			format->bytes_per_line >> pf->uv_hshift;

		y_size = format->bytes_per_line * height;
		uv_size = y_size >> (pf->uv_hshift + pf->uv_vshift);

		if (pf->vu_order) {
			format->v_offset = y_size; 
			format->u_offset = y_size + uv_size;
		} else {
			format->u_offset = y_size; 
			format->v_offset = y_size + uv_size;
		}

		format->size = y_size + uv_size * 2;
	} else {
		format->uv_bytes_per_line = 0;

		format->u_offset = 0;
		format->v_offset = 0;

		format->size = format->bytes_per_line * height;
	}

	format->pixfmt = pixfmt;
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

	if (!(pf = tv_pixel_format_from_pixfmt (format->pixfmt)))
		return FALSE;

	if (0 == format->width
	    || 0 == format->height)
		return FALSE;

	min_bpl = (format->width * pf->bits_per_pixel + 7) >> 3;

	if (format->bytes_per_line < min_bpl)
		return FALSE;

	/* We don't enforce bytes_per_line padding on the last
	   line, in case offset and bytes_per_line were adjusted
	   for cropping. */
	min_size = format->bytes_per_line * (format->height - 1) + min_bpl;

	if (pf->planar) {
		unsigned int min_uv_bytes_per_line;
		unsigned int min_uv_size;
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

		if (format->bytes_per_line & hres)
			return FALSE;

		/* U and V bits_per_pixel assumed 8. */
		min_uv_bytes_per_line = format->width >> pf->uv_hshift;

		if (format->uv_bytes_per_line < min_uv_bytes_per_line)
			return FALSE;

		/* We don't enforce bytes_per_line padding on the last line. */
		min_uv_size = format->uv_bytes_per_line
			* ((format->height >> pf->uv_vshift) - 1)
			+ min_uv_bytes_per_line;

		if (pf->vu_order != (format->v_offset > format->u_offset))
			return FALSE;

		if (format->u_offset > format->v_offset) {
			p1_offset = format->v_offset;
			p2_offset = format->u_offset;
		} else {
			p1_offset = format->u_offset;
			p2_offset = format->v_offset;
		}

		/* Y before U and V, planes must not overlap. */
		if (format->offset + min_size >= p1_offset)
			return FALSE;

		/* U and V planes must not overlap. */
		if (p1_offset + min_uv_size >= p2_offset)
			return FALSE;

		/* All planes must fit in buffer. */
		if (p2_offset + min_uv_size >= format->size)
			return FALSE;
	} else {
		if (format->offset + min_size >= format->size)
			return FALSE;
	}

	return TRUE;
}

#if Z_BYTE_ORDER == Z_LITTLE_ENDIAN
#  define SWAB32(m) (m)
#elif Z_BYTE_ORDER == Z_BIG_ENDIAN
#  define SWAB32(m)							\
	(+ (((m) & 0xFF000000) >> 24)					\
	 + (((m) & 0x00FF0000) >> 8)					\
	 + (((m) & 0x0000FF00) << 8)					\
	 + (((m) & 0x000000FF) << 24))
#else
#  error unknown endianess
#endif

typedef void
clear_block_fn			(void *			d,
				 unsigned int		value,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned int		bytes_per_line);

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
function_table_generic [4] = {
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
	tv_pixfmt_set set;
	clear_block_fn **ftable;
	uint8_t *data;

	assert (NULL != image);
	assert (NULL != format);

	if (!(pf = tv_pixel_format_from_pixfmt (format->pixfmt)))
		return FALSE;

	ftable = function_table_generic;

	set = TV_PIXFMT_SET (format->pixfmt);

	data = (uint8_t *) image;

	if (set & TV_PIXFMT_SET_RGB) {
		ftable[0] (data + format->offset, 0,
			   (format->width * pf->bits_per_pixel) >> 3,
			   format->height,
			   format->bytes_per_line);

		return TRUE;
	}

	if (set & TV_PIXFMT_SET_YUV_PLANAR) {
		unsigned int uv_width = format->width >> pf->uv_hshift;
		unsigned int uv_height = format->height >> pf->uv_vshift;

		ftable[0] (data + format->offset, 0x00,
			   format->width, format->height,
			   format->bytes_per_line);
		ftable[0] (data + format->u_offset, 0x80,
			   uv_width, uv_height,
			   format->uv_bytes_per_line);
		ftable[0] (data + format->v_offset, 0x80,
			   uv_width, uv_height,
			   format->uv_bytes_per_line);

		return TRUE;
	}

	switch (format->pixfmt) {
	case TV_PIXFMT_NONE:
	case TV_PIXFMT_RESERVED0:
	case TV_PIXFMT_RESERVED1:
	case TV_PIXFMT_RESERVED2:
	case TV_PIXFMT_RESERVED3:
		break;

	case TV_PIXFMT_YUV24_LE:
	case TV_PIXFMT_YVU24_LE:
		ftable[2] (data + format->offset, 0x808000,
			   format->width, format->height,
			   format->bytes_per_line);
		return TRUE;

	case TV_PIXFMT_YUV24_BE:
	case TV_PIXFMT_YVU24_BE:
		ftable[2] (data + format->offset, 0x008080,
			   format->width, format->height,
			   format->bytes_per_line);
		return TRUE;

	case TV_PIXFMT_YUVA32_LE:
	case TV_PIXFMT_YUVA32_BE:
	case TV_PIXFMT_YVUA32_LE:
	case TV_PIXFMT_YVUA32_BE:
		ftable[3] (data + format->offset, 0x00808000,
			   format->width, format->height,
			   format->bytes_per_line);
		return TRUE;

	case TV_PIXFMT_YUYV:
	case TV_PIXFMT_YVYU:
		ftable[3] (data + format->offset, SWAB32 (0x80008000),
			   format->width >> 1, format->height,
			   format->bytes_per_line);
		return TRUE;

	case TV_PIXFMT_UYVY:
	case TV_PIXFMT_VYUY:
		ftable[3] (data + format->offset, SWAB32 (0x00800080),
			   format->width >> 1, format->height,
			   format->bytes_per_line);
		return TRUE;

	case TV_PIXFMT_Y8:
		ftable[0] (data + format->v_offset, 0x00,
			   format->width, format->height,
			   format->uv_bytes_per_line);
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

	}

	return FALSE;
}
