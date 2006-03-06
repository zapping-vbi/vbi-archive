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

/* $Id: yuv2yuv.h,v 1.1 2006-03-06 01:50:48 mschimek Exp $ */

/* YUV to YUV image format conversion functions. */

#ifndef YUV2YUV_H
#define YUV2YUV_H

#include "image_format.h"
#include "simd.h"
#include "misc.h"

extern copy_plane_fn 		copy_plane_SCALAR;

SIMD_FN_PROTOS (copy_plane_fn, _tv_shuffle_0321);
SIMD_FN_PROTOS (copy_plane_fn, _tv_shuffle_1032);
SIMD_FN_PROTOS (copy_plane_fn, _tv_shuffle_1230);
SIMD_FN_PROTOS (copy_plane_fn, _tv_shuffle_2103);
SIMD_FN_PROTOS (copy_plane_fn, _tv_shuffle_2130);
SIMD_FN_PROTOS (copy_plane_fn, _tv_shuffle_3012);
SIMD_FN_PROTOS (copy_plane_fn, _tv_shuffle_3210);

extern tv_bool
_tv_yuyv_to_yuyv		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format);
extern tv_bool
_tv_yuyv_to_yuv420		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format);
extern tv_bool
_tv_yuv420_to_yuyv		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format);
extern tv_bool
_tv_yuv420_to_yuv420		(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format);

#endif /* YUV2YUV_H */
