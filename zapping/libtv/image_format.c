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

/* $Id: image_format.c,v 1.2 2004-11-03 06:39:06 mschimek Exp $ */

#include "misc.h"
#include "image_format.h"

tv_bool
tv_image_format_init		(tv_image_format *	format,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned int		bytes_per_line,
				 tv_pixfmt		pixfmt,
				 tv_color_space		color_space)
{
	tv_pixel_format pf;
	unsigned int min_bpl;

	assert (NULL != format);

	if (!tv_pixel_format_from_pixfmt (&pf, pixfmt, 0))
		return FALSE;

	if (0 == width || 0 == height)
		return FALSE;

	if (pf.planar) {
		/* Round size up to U and V scale factors. */

		width += pf.uv_hscale - 1;
		width -= width % pf.uv_hscale;

		height += pf.uv_vscale - 1;
		height -= height % pf.uv_vscale;
	}

	format->width = width;
	format->height = height;

	min_bpl = (width * pf.bits_per_pixel + 7) >> 3;

	format->bytes_per_line = MAX (bytes_per_line, min_bpl);

	format->offset = 0;

	if (pf.planar) {
		unsigned int y_size;
		unsigned int uv_size;

		/* No padding. */
		format->uv_bytes_per_line =
			format->bytes_per_line / pf.uv_hscale;

		y_size = format->bytes_per_line * height;
		uv_size = y_size / (pf.uv_hscale * pf.uv_vscale);

		if (pf.vu_order) {
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
	format->color_space = color_space;

	return TRUE;
}

tv_bool
tv_image_format_is_valid	(const tv_image_format *format)
{
	tv_pixel_format pf;
	unsigned int min_bpl;
	unsigned int min_size;

	assert (NULL != format);

	if (!tv_pixel_format_from_pixfmt (&pf, format->pixfmt, 0))
		return FALSE;

	if (0 == format->width
	    || 0 == format->height)
		return FALSE;

	min_bpl = (format->width * pf.bits_per_pixel + 7) >> 3;

	if (format->bytes_per_line < min_bpl)
		return FALSE;

	/* We don't enforce bytes_per_line padding on the last
	   line, in case offset and bytes_per_line were adjusted
	   for cropping. */
	min_size = format->bytes_per_line * (format->height - 1) + min_bpl;

	if (pf.planar) {
		unsigned int min_uv_bytes_per_line;
		unsigned int min_uv_size;
		unsigned int p1_offset;
		unsigned int p2_offset;

		if (0 != format->width % pf.uv_hscale
		    || 0 != format->height % pf.uv_vscale)
			return FALSE;

		if (0 != format->bytes_per_line % pf.uv_hscale)
			return FALSE;

		/* U and V bits_per_pixel assumed 8. */
		min_uv_bytes_per_line = format->width / pf.uv_hscale;

		if (format->uv_bytes_per_line < min_uv_bytes_per_line)
			return FALSE;

		/* We don't enforce bytes_per_line padding on the last line. */
		min_uv_size = format->uv_bytes_per_line
			* (format->height / pf.uv_vscale - 1)
			+ min_uv_bytes_per_line;

		if (pf.vu_order != (format->v_offset > format->u_offset))
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
