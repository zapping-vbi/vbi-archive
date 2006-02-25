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

/* $Id: rgb2rgb.h,v 1.1 2006-02-25 17:37:43 mschimek Exp $ */

/* RGB to RGB image format conversion functions. */

#ifndef IMAGE_FORMAT_RGB2RGB_H
#define IMAGE_FORMAT_RGB2RGB_H

#include "image_format.h"

extern tv_bool
_tv_rgb32_to_rgb16		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format);
extern tv_bool
_tv_rgb32_to_rgb32		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format);
extern tv_bool
_tv_sbggr_to_rgb		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format);

#endif /* IMAGE_FORMAT_RGB2RGB_H */
