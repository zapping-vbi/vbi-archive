/*
 *  libzvbi
 *
 *  Copyright (C) 2003, 2004 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

/* $Id: image_format.c,v 1.1.2.2 2007-11-01 00:21:23 mschimek Exp $ */

#include "image_format.h"

/**
 * @param pixfmt Pixel format.
 *
 * Returns the name of a pixel format like VBI3_PIXFMT_YUYV ->
 * "YUYV". This is mainly intended for debugging.
 * 
 * @return
 * Static ASCII string, NULL if @a pixfmt is invalid.
 */
const char *
vbi3_pixfmt_name			(vbi3_pixfmt		pixfmt)
{
	switch (pixfmt) {

#undef CASE
#define CASE(fmt) case VBI3_PIXFMT_##fmt : return #fmt ;

	CASE (NONE)
	CASE (YUV444)
	CASE (YVU444)
	CASE (YUV422)
	CASE (YVU422)
	CASE (YUV411)
	CASE (YVU411)
	CASE (YUV420)
	CASE (YVU420)
	CASE (YUV410)
	CASE (YVU410)
	CASE (YUVA24_LE)
	CASE (YUVA24_BE)
	CASE (YVUA24_LE)
	CASE (YVUA24_BE)
	CASE (YUV24_LE)
	CASE (YUV24_BE)
	CASE (YVU24_LE)
	CASE (YVU24_BE)
	CASE (YUYV)
	CASE (YVYU)
	CASE (UYVY)
	CASE (VYUY)
	CASE (Y8)
	CASE (RGBA24_LE)
	CASE (RGBA24_BE)
	CASE (BGRA24_LE)
	CASE (BGRA24_BE)
	CASE (RGB24_LE)
	CASE (BGR24_LE)
	CASE (RGB16_LE)
	CASE (RGB16_BE)
	CASE (BGR16_LE)
	CASE (BGR16_BE)
	CASE (RGBA15_LE)
	CASE (RGBA15_BE)
	CASE (BGRA15_LE)
	CASE (BGRA15_BE)
	CASE (ARGB15_LE)
	CASE (ARGB15_BE)
	CASE (ABGR15_LE)
	CASE (ABGR15_BE)
	CASE (RGBA12_LE)
	CASE (RGBA12_BE)
	CASE (BGRA12_LE)
	CASE (BGRA12_BE)
	CASE (ARGB12_LE)
	CASE (ARGB12_BE)
	CASE (ABGR12_LE)
	CASE (ABGR12_BE)
	CASE (RGB8)
	CASE (BGR8)
	CASE (RGBA7)
	CASE (BGRA7)
	CASE (ARGB7)
	CASE (ABGR7)

	case VBI3_PIXFMT_RESERVED0:
	case VBI3_PIXFMT_RESERVED1:
	case VBI3_PIXFMT_RESERVED2:
	case VBI3_PIXFMT_RESERVED3:
		break;

		/* No default, gcc warns. */
	}

	return NULL;
}

unsigned int
_vbi3_pixfmt_bytes_per_pixel	(vbi3_pixfmt		pixfmt)
{
	vbi3_pixfmt_set set = VBI3_PIXFMT_SET (pixfmt);

	if (set & (_VBI3_PIXFMT_SET_4 | _VBI3_PIXFMT_SET_3))
		return (set & _VBI3_PIXFMT_SET_4) ? 4 : 3;
	else if (set & (_VBI3_PIXFMT_SET_2 | _VBI3_PIXFMT_SET_1))
		return (set & _VBI3_PIXFMT_SET_2) ? 2 : 1;

	return 0;
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
