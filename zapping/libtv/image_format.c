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

/* $Id: image_format.c,v 1.20 2006-05-17 18:02:21 mschimek Exp $ */

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

	width = (width + pf->hmask) & ~pf->hmask;
	height = (height + pf->vmask) & ~pf->vmask;

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

	if (pf->n_planes > 1) {
		unsigned long uv_bpl;
		unsigned long y_size;
		unsigned long uv_size;

		/* No padding. */
		uv_bpl = format->bytes_per_line[0] >> pf->uv_hshift;

		y_size = format->bytes_per_line[0] * height;
		uv_size = y_size >> (pf->uv_hshift + pf->uv_vshift);

		switch (pixfmt) {
		case TV_PIXFMT_NV12:
		case TV_PIXFMT_HM12:
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
	unsigned long min_size[3];
	unsigned long min_bpl;

	assert (NULL != format);

	pf = format->pixel_format;

	if (0 == format->width
	    || 0 == format->height)
		return FALSE;

	if (0 != ((format->width & pf->hmask)
		  | (format->height & pf->vmask)))
		return FALSE;

	min_bpl = (format->width * pf->bits_per_pixel + 7) >> 3;

	if (format->bytes_per_line[0] < min_bpl)
		return FALSE;

	/* We don't enforce bytes_per_line padding on the last
	   line, in case offset and bytes_per_line were adjusted
	   for cropping. */
	min_size[0] = format->bytes_per_line[0] * (format->height - 1) + min_bpl;

	if (pf->n_planes > 1) {
		unsigned int order[3];
		unsigned long min_uv_bpl;

		/* U and V bits_per_pixel assumed 8. */
		min_uv_bpl = format->width >> pf->uv_hshift;

		if (format->bytes_per_line[1] < min_uv_bpl)
			return FALSE;

		/* We don't enforce bytes_per_line padding
		   on the last line. */
		min_size[1] = format->bytes_per_line[1]
			* ((format->height >> pf->uv_vshift) - 1)
			+ min_uv_bpl;

		order[0] = (format->offset[1] < format->offset[0]);
		order[1] = order[0] ^ 1;
		order[2] = 2;

		if (pf->n_planes > 2) {
			if (format->bytes_per_line[2] < min_uv_bpl)
				return FALSE;

			min_size[2] = format->bytes_per_line[2]
				* ((format->height >> pf->uv_vshift) - 1)
				+ min_uv_bpl;

			if (format->offset[2] < format->offset[order[1]]) {
				order[2] = order[1];
				if (format->offset[2] < format->offset[order[0]]) {
					order[1] = order[0];
					order[0] = 2;
				} else {
					order[1] = 2;
				}
			}

			/* Enforce plane order, planes must not overlap and
			   must fit in buffer. */
			if (format->offset[order[2]] + min_size[order[2]]
			    > format->size)
				return FALSE;
		}

		if (format->offset[order[1]] + min_size[order[1]]
		    > format->offset[order[2]])
			return FALSE;

		if (format->offset[order[0]] + min_size[order[0]]
		    > format->offset[order[1]])
			return FALSE;
	} else {
		if (format->offset[0] + min_size[0] > format->size)
			return FALSE;
	}

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
