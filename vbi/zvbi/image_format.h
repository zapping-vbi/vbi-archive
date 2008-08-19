/*
 *  libzvbi - Image definitions
 *
 *  Copyright (C) 2003, 2004 Michael H. Schimek
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the 
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301  USA.
 */

/* $Id: image_format.h,v 1.1.2.2 2008-08-19 16:36:40 mschimek Exp $ */

#ifndef __ZVBI_IMAGE_FORMAT_H__
#define __ZVBI_IMAGE_FORMAT_H__

#include <inttypes.h>		/* uint64_t */
#include "macros.h"

VBI_BEGIN_DECLS

/**
 * @addtogroup Types
 * @{
 */

/** Pixel format identifier. */
typedef enum {
	VBI_PIXFMT_NONE,			/**< */
	VBI_PIXFMT_UNKNOWN = VBI_PIXFMT_NONE,	/**< */

	VBI_PIXFMT_YUV444,	/**< 4x4 4x4 4x4 */
	VBI_PIXFMT_YUV422,	/**< 4x4 2x4 2x4 */
	VBI_PIXFMT_YUV420,	/**< 4x4 2x2 2x2 */

	VBI_PIXFMT_YUVA24_LE,	/**< */

	VBI_PIXFMT_YUYV,	 /**< */
	VBI_PIXFMT_UYVY,	 /**< */

	VBI_PIXFMT_Y8,		/**< */

	VBI_PIXFMT_RGBA24_LE,	/**< R G B A */
	VBI_PIXFMT_RGBA24_BE,	/**< A B G R */
	VBI_PIXFMT_BGRA24_LE,	/**< B G R A */
	VBI_PIXFMT_BGRA24_BE,	/**< A R G B */

	VBI_PIXFMT_ABGR24_BE = VBI_PIXFMT_RGBA24_LE, /**< */
	VBI_PIXFMT_ABGR24_LE,	/**< */
	VBI_PIXFMT_ARGB24_BE,	/**< */
	VBI_PIXFMT_ARGB24_LE,	/**< */

	VBI_PIXFMT_RGB24_LE,	/**< R G B */
	VBI_PIXFMT_RGB24_BE,	/**< B G R */
	VBI_PIXFMT_BGR24_BE = VBI_PIXFMT_RGB24_LE, /**< */
	VBI_PIXFMT_BGR24_LE,	/**< */

	VBI_PIXFMT_BGR16_LE,	/**< gggbbbbb rrrrrggg */
	VBI_PIXFMT_BGR16_BE,	/**< rrrrrggg gggbbbbb */

	VBI_PIXFMT_BGRA15_LE,	/**< gggbbbbb arrrrrgg */
	VBI_PIXFMT_BGRA15_BE,	/**< arrrrrgg gggbbbbb */

	VBI_PIXFMT_BGRA12_LE,	/**< ggggbbbb aaaarrrr */
	VBI_PIXFMT_BGRA12_BE,	/**< aaaarrrr ggggbbbb */

        VBI_PIXFMT_STATIC_8,	/**< i */
        VBI_PIXFMT_PSEUDO_8,	/**< pal8[i] */
} vbi_pixfmt;

/** Color space identifier. No values defined yet. */
typedef enum {
	VBI_COLOR_SPACE_NONE,				/**< */
	VBI_COLOR_SPACE_UNKNOWN = VBI_COLOR_SPACE_NONE	/**< */
} vbi_color_space;

/** This structure describes an image buffer. */
typedef struct {
	unsigned int		width;
	unsigned int		height;

	unsigned long		offset[4];
	unsigned long		bytes_per_line[4];

	/** Pixel format used by the buffer. */
	vbi_pixfmt		pixfmt;

	/** Color space used by the buffer. */
	vbi_color_space		color_space;
} vbi_image_format;

/** @} */

VBI_END_DECLS

#endif /* __ZVBI_IMAGE_FORMAT_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
