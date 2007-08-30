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

/* $Id: image_format.h,v 1.11 2007-08-30 14:14:09 mschimek Exp $ */

#ifndef __ZTV_IMAGE_FORMAT_H__
#define __ZTV_IMAGE_FORMAT_H__

#include <stdio.h>		/* FILE */
#include <stdlib.h>		/* free() */
#include "pixel_format.h"

TV_BEGIN_DECLS

typedef enum {
	TV_FIELD_UNKNOWN = 0,
	TV_FIELD_PROGRESSIVE,
	TV_FIELD_INTERLACED,
	TV_FIELD_SEQUENTIAL,
	TV_FIELD_TOP,
	TV_FIELD_BOTTOM,
} tv_field;

typedef struct {
	/* Image width in pixels. For planar formats this refers to
	   the largest plane. For YUV formats this must be a multiple
	   of 1 << tv_pixel_format.uv_hscale. */
	unsigned int		width;

	/* Image height in pixels. For planar formats this refers to
	   the largest plane. For YUV formats this must be a multiple
	   of 1 << tv_pixel_format.uv_vscale. */
	unsigned int		height;

	/* Offset in bytes from the buffer start to the top left pixel
	   of the first, second, third and fourth plane. */
	unsigned long		offset[4];

	/* Bytes per line of the first, second, third and fourth plane.
	   Must be bytes_per_line[i] >= (plane width
	   * bits per pixel + 7) / 8. */
	unsigned long		bytes_per_line[4];

	/* Buffer size. All planes must fit within this size:
	   offset[i] + (plane height - 1) * bytes_per_line[i]
	   + (plane width * bits per pixel + 7) / 8 <= size. */
  /* XXX does this really belong here? Or rather in a buffer structure. */
	unsigned long		size;

	const tv_pixel_format *	pixel_format;
	tv_colspc		colspc;
	tv_field		field;
} tv_image_format;

extern tv_bool
tv_image_format_init		(tv_image_format *	format,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		bytes_per_line,
				 tv_pixfmt		pixfmt,
				 tv_colspc		colspc)
  __attribute__ ((_tv_nonnull (1)));
extern tv_bool
tv_image_format_is_valid	(const tv_image_format *format)
  __attribute__ ((_tv_nonnull (1)));
extern void
_tv_image_format_dump		(const tv_image_format *format,
				 FILE *			fp)
  __attribute__ ((_tv_nonnull (1, 2)));

extern tv_bool
tv_clear_image			(void *			image,
				 const tv_image_format *format)
  __attribute__ ((_tv_nonnull (1, 2)));
extern void
tv_memcpy			(void *			dst,
				 const void *		src,
				 size_t			n_bytes)
  __attribute__ ((_tv_nonnull (1, 2)));
extern tv_bool
tv_copy_image			(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format)
  __attribute__ ((_tv_nonnull (1, 2)));
/* Replace free() by a more descriptive name for documentation purposes. */
static __inline__ void
tv_delete_image			(void *			image)
{
  free (image);
}

extern void *
tv_new_image			(const void *		src_image,
				 const tv_image_format *src_format)
  __attribute__ ((malloc,
		  _tv_nonnull (2)));

TV_END_DECLS

#endif /* __ZTV_IMAGE_FORMAT_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
