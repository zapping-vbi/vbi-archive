/*
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: pixel.h,v 1.1 2004-12-11 11:46:25 mschimek Exp $ */

#undef NDEBUG

#include <stdio.h>
#include <assert.h>
#include "libtv/image_format.h"
#include "libtv/misc.h"		/* FFS() */

static unsigned int
get_packed_pixel		(const uint8_t *	s,
				 tv_pixfmt		pixfmt)
{
	unsigned int t;

#define RGB32(r, g, b, a)			\
	(+ (s[r] << 0)				\
	 + (s[g] << 8)				\
	 + (s[b] << 16)				\
	 + (s[a] << 24))

#define RGB24(r, g, b)				\
	(+ (s[r] << 0)				\
	 + (s[g] << 8)				\
	 + (s[b] << 16))

/* v & m, then shift msb of m to bit n (1 == lsb). */
/* MAX() prevents a bogus warning. */
#define MSHIFT(v, m, n)				\
	((FFS (m) > n) ?			\
	 ((v & m) >> (FFS (m) - n)) :		\
	 ((v & m) << MAX(n - FFS (m), 31)))

#define RGB16(r, g, b, a, e)			\
	(t = s[0] + s[1] * 256,			\
	 + MSHIFT (t, r, 8)			\
	 + MSHIFT (t, g, 16)			\
	 + MSHIFT (t, b, 24)			\
	 + MSHIFT (t, a, 32))

#define RGB8(r, g, b, a)			\
	(+ MSHIFT (s[0], r, 8)			\
	 + MSHIFT (s[0], g, 16)			\
	 + MSHIFT (s[0], b, 24)			\
	 + MSHIFT (s[0], a, 32))

	switch (pixfmt) {
	case TV_PIXFMT_YUVA32_LE: return RGB32 (0, 1, 2, 3);
	case TV_PIXFMT_YUVA32_BE: return RGB32 (3, 2, 1, 0);
	case TV_PIXFMT_YVUA32_LE: return RGB32 (0, 2, 1, 3);
	case TV_PIXFMT_YVUA32_BE: return RGB32 (3, 1, 2, 0);

	case TV_PIXFMT_YUV24_LE: return RGB24 (0, 1, 2);
	case TV_PIXFMT_YUV24_BE: return RGB24 (2, 1, 0);
	case TV_PIXFMT_YVU24_LE: return RGB24 (0, 2, 1);
	case TV_PIXFMT_YVU24_BE: return RGB24 (2, 0, 1);

	case TV_PIXFMT_Y8: return s[0] | 0x808000;

	case TV_PIXFMT_RGBA32_LE: return RGB32 (0, 1, 2, 3);
	case TV_PIXFMT_RGBA32_BE: return RGB32 (3, 2, 1, 0);
	case TV_PIXFMT_BGRA32_LE: return RGB32 (2, 1, 0, 3);
	case TV_PIXFMT_BGRA32_BE: return RGB32 (1, 2, 3, 0);

	case TV_PIXFMT_RGB24_LE: return RGB24 (0, 1, 2);
	case TV_PIXFMT_BGR24_LE: return RGB24 (2, 1, 0);

	case TV_PIXFMT_RGB16_LE: return RGB16 (0x001F, 0x07E0, 0xF800, 0, 0);
	case TV_PIXFMT_RGB16_BE: return RGB16 (0x001F, 0x07E0, 0xF800, 0, 1);
	case TV_PIXFMT_BGR16_LE: return RGB16 (0xF800, 0x07E0, 0x001F, 0, 0);
	case TV_PIXFMT_BGR16_BE: return RGB16 (0xF800, 0x07E0, 0x001F, 0, 1);

#define CASE_RGB16(fmt, r, g, b, a)					\
	case TV_PIXFMT_ ## fmt ## _LE: return RGB16 (r, g, b, a, 0);	\
	case TV_PIXFMT_ ## fmt ## _BE: return RGB16 (r, g, b, a, 1);

	CASE_RGB16 (RGBA16, 0x001F, 0x03E0, 0xEC00, 0x8000);
	CASE_RGB16 (BGRA16, 0xEC00, 0x03E0, 0x001F, 0x8000);
	CASE_RGB16 (ARGB16, 0x003E, 0x07C0, 0xF800, 0x0001);
	CASE_RGB16 (ABGR16, 0xF800, 0x07C0, 0x003E, 0x0001);

	CASE_RGB16 (RGBA12, 0x000F, 0x00F0, 0x0F00, 0xF000);
	CASE_RGB16 (BGRA12, 0x0F00, 0x00F0, 0x000F, 0xF000);
	CASE_RGB16 (ARGB12, 0x00F0, 0x0F00, 0xF000, 0x000F);
	CASE_RGB16 (ABGR12, 0xF000, 0x0F00, 0x00F0, 0x000F);

	case TV_PIXFMT_RGB8: return RGB8 (0x07, 0x38, 0xC0, 0);
	case TV_PIXFMT_BGR8: return RGB8 (0xE0, 0x1C, 0x03, 0);

	case TV_PIXFMT_RGBA8: return RGB8 (0x03, 0x1C, 0x60, 0x80);
	case TV_PIXFMT_BGRA8: return RGB8 (0x60, 0x1C, 0x03, 0x80);
	case TV_PIXFMT_ARGB8: return RGB8 (0x06, 0x38, 0xC0, 0x01);
	case TV_PIXFMT_ABGR8: return RGB8 (0xC0, 0x38, 0x06, 0x01);

	default:
		assert (!"reached");
		return 0;
	}
}
