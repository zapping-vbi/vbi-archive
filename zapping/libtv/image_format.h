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

/* $Id: image_format.h,v 1.4 2004-12-11 11:46:21 mschimek Exp $ */

#ifndef __ZTV_IMAGE_FORMAT_H__
#define __ZTV_IMAGE_FORMAT_H__

#include <stdio.h>		/* FILE */
#include "pixel_format.h"

TV_BEGIN_DECLS

typedef struct {
	/* Image width in pixels, for planar formats this refers to
	   the largest plane. For YUV formats this must be a multiple
	   of 1 << tv_pixel_format.uv_hscale. */
	unsigned int		width;

	/* Image height in pixels, for planar formats this refers to
	   the largest plane. For YUV formats this must be a multiple
	   of 1 << tv_pixel_format.uv_vscale. */
	unsigned int		height;

	/* For packed formats bytes_per_line >= (width * tv_pixel_format
	   .bits_per_pixel + 7) / 8. For planar formats this refers to
	   the Y plane only, with implied y_size = bytes_per_line * height. */
	unsigned int		bytes_per_line;

	/* For planar formats only, refers to the U and V plane. */
	unsigned int		uv_bytes_per_line;

/* ATTENTION [u_|v_]offset and [uv_]bytes_per_line aren't
   fully supported yet, don't use them. */

	/* For packed formats the image offset in bytes from the buffer
	   start. For planar formats this refers to the Y plane. */
	unsigned int		offset;

	/* For planar formats only, the byte offset of the U and V
	   plane from the start of the buffer. */
	unsigned int		u_offset;
	unsigned int		v_offset;

	/* Buffer size. For packed formats size >= offset + height
	   * bytes_per_line. For planar formats size >=
	   MAX (offset + y_size, u_offset + uv_size, v_offset + uv_size). */
	unsigned int		size;

	tv_pixfmt		pixfmt;
	tv_colspc		colspc;
} tv_image_format;

extern tv_bool
tv_image_format_init		(tv_image_format *	format,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned int		bytes_per_line,
				 tv_pixfmt		pixfmt,
				 tv_colspc		colspc);
extern tv_bool
tv_image_format_is_valid	(const tv_image_format *format);
extern void
_tv_image_format_dump		(const tv_image_format *format,
				 FILE *			fp);

extern tv_bool
tv_clear_image			(void *			image,
				 const tv_image_format *format);
extern void
tv_memcpy			(void *			dst,
				 const void *		src,
				 size_t			n_bytes);

TV_END_DECLS

#endif /* __ZTV_IMAGE_FORMAT_H__ */
