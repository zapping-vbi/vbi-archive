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

/* $Id: pixel_format.c,v 1.1 2004-09-10 04:56:05 mschimek Exp $ */

#include "../config.h"		/* BYTE_ORDER */

#include "misc.h"
#include "pixel_format.h"

const char *
tv_pixfmt_name			(tv_pixfmt		pixfmt)
{
	switch (pixfmt) {

#undef CASE
#define CASE(s) case TV_PIXFMT_##s : return #s ;

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
	CASE (YUVA32_LE)
	CASE (YUVA32_BE)
	CASE (YVUA32_LE)
	CASE (YVUA32_BE)
	/* AVUY32_BE synonyms */
	/* AVUY32_LE */
	/* AUVY32_BE */
	/* AUVY32_LE */
	CASE (YUV24_LE)
	CASE (YUV24_BE)
	CASE (YVU24_LE)
	CASE (YVU24_BE)
	/* VUY24_BE */
	/* VUY24_LE */
	/* UVY24_BE */
	/* UVY24_LE */
	CASE (YUYV)
	CASE (YVYU)
	CASE (UYVY)
	CASE (VYUY)
	CASE (Y8)
	CASE (RGBA32_LE)
	CASE (RGBA32_BE)
	CASE (BGRA32_LE)
	CASE (BGRA32_BE)
	/* ABGR32_BE synonyms */
	/* ABGR32_LE */
	/* ARGB32_BE */
	/* ARGB32_LE */
	CASE (RGB24_LE)
	CASE (BGR24_LE)
	/* BGR24_BE */
	/* RGB24_BE */
	CASE (RGB16_LE)
	CASE (RGB16_BE)
	CASE (BGR16_LE)
	CASE (BGR16_BE)
	CASE (RGBA16_LE)
	CASE (RGBA16_BE)
	CASE (BGRA16_LE)
	CASE (BGRA16_BE)
	CASE (ARGB16_LE)
	CASE (ARGB16_BE)
	CASE (ABGR16_LE)
	CASE (ABGR16_BE)
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
	CASE (RGBA8)
	CASE (BGRA8)
	CASE (ARGB8)
	CASE (ABGR8)

	case TV_PIXFMT_RESERVED0:
	case TV_PIXFMT_RESERVED1:
	case TV_PIXFMT_RESERVED2:
	case TV_PIXFMT_RESERVED3:
		break;
	}

	return NULL;
}

unsigned int
tv_pixfmt_bytes_per_pixel	(tv_pixfmt		pixfmt)
{
	switch (pixfmt) {
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
		return 1;

	case TV_PIXFMT_YUVA32_LE:
	case TV_PIXFMT_YUVA32_BE:
	case TV_PIXFMT_YVUA32_LE:
	case TV_PIXFMT_YVUA32_BE:
		return 4;

	case TV_PIXFMT_YUV24_LE:
	case TV_PIXFMT_YUV24_BE:
	case TV_PIXFMT_YVU24_LE:
	case TV_PIXFMT_YVU24_BE:
		return 3;

	case TV_PIXFMT_YUYV:
	case TV_PIXFMT_YVYU:
	case TV_PIXFMT_UYVY:
	case TV_PIXFMT_VYUY:
		return 2;

	case TV_PIXFMT_Y8:
		return 1;

	case TV_PIXFMT_RGBA32_LE:
	case TV_PIXFMT_RGBA32_BE:
	case TV_PIXFMT_BGRA32_LE:
	case TV_PIXFMT_BGRA32_BE:
		return 4;

	case TV_PIXFMT_RGB24_LE:
	case TV_PIXFMT_BGR24_LE:
		return 3;

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
		return 2;

	case TV_PIXFMT_RGB8:
	case TV_PIXFMT_BGR8:
	case TV_PIXFMT_RGBA8:
	case TV_PIXFMT_BGRA8:
	case TV_PIXFMT_ARGB8:
	case TV_PIXFMT_ABGR8:
		return 1;

	case TV_PIXFMT_NONE:
	case TV_PIXFMT_RESERVED0:
	case TV_PIXFMT_RESERVED1:
	case TV_PIXFMT_RESERVED2:
	case TV_PIXFMT_RESERVED3:
		break;
	}

	return 0;
}

/* XXX check this, esp LE/BE, see also libzvbi */
tv_bool
tv_pixel_format_from_pixfmt	(tv_pixel_format *	format,
				 tv_pixfmt		pixfmt,
				 unsigned int		reserved)
{
	/* unused, VU, msb A, msb B,
	   unused, unused, BE on BE, BE on LE machine */
	static const uint8_t attr_table [] = {
		[TV_PIXFMT_YUV444]	= 0x00,
		[TV_PIXFMT_YVU444]	= 0x40,
		[TV_PIXFMT_YUV422]	= 0x00,
		[TV_PIXFMT_YVU422]	= 0x40,
		[TV_PIXFMT_YUV411]	= 0x00,
		[TV_PIXFMT_YVU411]	= 0x40,
		[TV_PIXFMT_YUV420]	= 0x00,
		[TV_PIXFMT_YVU420]	= 0x40,
		[TV_PIXFMT_YUV410]	= 0x00,
		[TV_PIXFMT_YVU410]	= 0x40,

		[TV_PIXFMT_YUVA32_LE]	= 0x02,
		[TV_PIXFMT_YUVA32_BE]	= 0x01,
		[TV_PIXFMT_YVUA32_LE]	= 0x42,
		[TV_PIXFMT_YVUA32_BE]	= 0x41,

		[TV_PIXFMT_YUV24_LE]	= 0x02,
		[TV_PIXFMT_YUV24_BE]	= 0x01,
		[TV_PIXFMT_YVU24_LE]	= 0x42,
		[TV_PIXFMT_YVU24_BE]	= 0x41,

		[TV_PIXFMT_YUYV]	= 0x00,
		[TV_PIXFMT_YVYU]	= 0x40,
		[TV_PIXFMT_UYVY]	= 0x00,
		[TV_PIXFMT_VYUY]	= 0x40,

		[TV_PIXFMT_Y8]		= 0x00,

		[TV_PIXFMT_RGBA32_LE]	= 0x02,
		[TV_PIXFMT_RGBA32_BE]	= 0x01,
		[TV_PIXFMT_BGRA32_LE]	= 0x12,
		[TV_PIXFMT_BGRA32_BE]	= 0x11,

		[TV_PIXFMT_RGB24_LE]	= 0x02,
		[TV_PIXFMT_RGB24_BE]	= 0x01,

		[TV_PIXFMT_RGB16_LE]	= 0x00,
		[TV_PIXFMT_RGB16_BE]	= 0x03,
		[TV_PIXFMT_BGR16_LE]	= 0x10,
		[TV_PIXFMT_BGR16_BE]	= 0x13,

		[TV_PIXFMT_RGBA16_LE]	= 0x00,
		[TV_PIXFMT_RGBA16_BE]	= 0x03,
		[TV_PIXFMT_BGRA16_LE]	= 0x10,
		[TV_PIXFMT_BGRA16_BE]	= 0x13,
		[TV_PIXFMT_ARGB16_LE]	= 0x20,
		[TV_PIXFMT_ARGB16_BE]	= 0x23,
		[TV_PIXFMT_ABGR16_LE]	= 0x30,
		[TV_PIXFMT_ABGR16_BE]	= 0x33,

		[TV_PIXFMT_RGBA12_LE]	= 0x00,
		[TV_PIXFMT_RGBA12_BE]	= 0x03,
		[TV_PIXFMT_BGRA12_LE]	= 0x10,
		[TV_PIXFMT_BGRA12_BE]	= 0x13,
		[TV_PIXFMT_ARGB12_LE]	= 0x20,
		[TV_PIXFMT_ARGB12_BE]	= 0x23,
		[TV_PIXFMT_ABGR12_LE]	= 0x30,
		[TV_PIXFMT_ABGR12_BE]	= 0x33,

		[TV_PIXFMT_RGB8]	= 0x00,
		[TV_PIXFMT_BGR8]	= 0x00,

		[TV_PIXFMT_RGBA8]	= 0x00,
		[TV_PIXFMT_BGRA8]	= 0x10,
		[TV_PIXFMT_ARGB8]	= 0x20,
		[TV_PIXFMT_ABGR8]	= 0x30,
	};
	unsigned int attr;

	assert (NULL != format);

	if ((unsigned int) pixfmt >= N_ELEMENTS (attr_table))
		return FALSE;

	attr = attr_table [pixfmt];

	format->pixfmt			= pixfmt;
	format->_reserved1		= reserved;

	format->uv_hscale		= 1;
	format->uv_vscale		= 1;

#if Z_BYTE_ORDER == Z_BIG_ENDIAN
	format->big_endian		= !!(attr & 3);
#elif Z_BYTE_ORDER == Z_LITTLE_ENDIAN
	format->big_endian		= attr & 1;
#else
#  error Unknown or unsupported endianess.
#endif

	format->planar			= FALSE;

	format->mask.rgb.a		= 0U;

	switch (pixfmt) {
	case TV_PIXFMT_YUV444:
	case TV_PIXFMT_YVU444:
		format->color_depth = 24;
		goto yuv_planar;

	case TV_PIXFMT_YUV422:
	case TV_PIXFMT_YVU422:
		format->color_depth = 16;
		format->uv_hscale = 2;
		goto yuv_planar;

	case TV_PIXFMT_YUV411:
	case TV_PIXFMT_YVU411:
		format->color_depth = 12;
		format->uv_hscale = 4;
		goto yuv_planar;

	case TV_PIXFMT_YUV420:
	case TV_PIXFMT_YVU420:
		format->color_depth = 12;
		format->uv_hscale = 2;
		format->uv_vscale = 2;
		goto yuv_planar;

	case TV_PIXFMT_YUV410:
	case TV_PIXFMT_YVU410:
		format->color_depth = 9;
		format->uv_hscale = 4;
		format->uv_vscale = 4;

	yuv_planar:
		format->bits_per_pixel = 8; /* Y plane */
		format->planar = TRUE;
		format->mask.yuv.y = 0xFF;
		format->mask.yuv.u = 0xFF;
		format->mask.yuv.v = 0xFF;
		break;

	case TV_PIXFMT_YUVA32_LE: /* LE 0xAAVVUUYY BE 0xYYUUVVAA */
	case TV_PIXFMT_YUVA32_BE: /* LE 0xYYUUVVAA BE 0xAAVVUUYY */
	case TV_PIXFMT_YVUA32_LE: /* LE 0xAAUUVVYY BE 0xYYVVUUAA */
	case TV_PIXFMT_YVUA32_BE: /* LE 0xYYVVUUAA BE 0xAAUUVVYY */
		format->bits_per_pixel = 32;
		format->color_depth = 24;

		if (format->big_endian) {
			format->mask.yuv.y = 0xFFU << 24;
			format->mask.yuv.u = 0xFFU << 16;
			format->mask.yuv.v = 0xFFU << 8;
			format->mask.yuv.a = 0xFFU;
		} else {
			format->mask.yuv.y = 0xFFU;
			format->mask.yuv.u = 0xFFU << 8;
			format->mask.yuv.v = 0xFFU << 16;
			format->mask.yuv.a = 0xFFU << 24;
		}
		break;

	case TV_PIXFMT_YUV24_LE: /* LE 0xVVUUYY BE 0xYYUUVV */
	case TV_PIXFMT_YUV24_BE: /* LE 0xYYUUVV BE 0xVVUUYY */
	case TV_PIXFMT_YVU24_LE: /* LE 0xUUVVYY BE 0xYYVVUU */
	case TV_PIXFMT_YVU24_BE: /* LE 0xYYVVUU BE 0xUUVVYY */
		format->bits_per_pixel = 24;
		format->color_depth = 24;

		if (format->big_endian) {
			format->mask.yuv.y = 0xFFU << 16;
			format->mask.yuv.v = 0xFFU;
		} else {
			format->mask.yuv.y = 0xFFU;
			format->mask.yuv.v = 0xFFU << 16;
		}

		format->mask.yuv.u = 0xFFU << 8;

		break;

	case TV_PIXFMT_YUYV:
	case TV_PIXFMT_YVYU:
	case TV_PIXFMT_UYVY:
	case TV_PIXFMT_VYUY:
		format->bits_per_pixel = 16;
		format->color_depth = 16;
		format->uv_hscale = 2;
		format->mask.yuv.y = 0xFFU; /* << 0, << 8 ? */
		format->mask.yuv.u = 0xFFU;
		format->mask.yuv.v = 0xFFU;
		break;

	case TV_PIXFMT_Y8:
		format->bits_per_pixel = 8;
		format->color_depth = 8;
		format->mask.yuv.y = 0xFFU;
		format->mask.yuv.u = 0U;
		format->mask.yuv.v = 0U;
		break;

	case TV_PIXFMT_RGBA32_LE: /* LE 0xAABBGGRR BE 0xRRGGBBAA */
	case TV_PIXFMT_RGBA32_BE: /* LE 0xRRGGBBAA BE 0xAABBGGRR */
	case TV_PIXFMT_BGRA32_LE: /* LE 0xAARRGGBB BE 0xBBGGRRAA */
	case TV_PIXFMT_BGRA32_BE: /* LE 0xBBGGRRAA BE 0xAARRGGBB */
		format->bits_per_pixel = 32;
		format->color_depth = 24;

		if (format->big_endian) {
			format->mask.rgb.r = 0xFFU << 24;
			format->mask.rgb.g = 0xFFU << 16;
			format->mask.rgb.b = 0xFFU << 8;
			format->mask.rgb.a = 0xFFU;
		} else {
			format->mask.rgb.r = 0xFFU;
			format->mask.rgb.g = 0xFFU << 8;
			format->mask.rgb.b = 0xFFU << 16;
			format->mask.rgb.a = 0xFFU << 24;
		}
		break;

	case TV_PIXFMT_RGB24_LE: /* LE 0xBBGGRR BE 0xRRGGBB */
	case TV_PIXFMT_BGR24_LE: /* LE 0xRRGGBB BE 0xBBGGRR */
		format->bits_per_pixel = 24;
		format->color_depth = 24;

		if (format->big_endian) {
			format->mask.rgb.r = 0xFFU << 16;
			format->mask.rgb.b = 0xFFU;
		} else {
			format->mask.rgb.r = 0xFFU;
			format->mask.rgb.b = 0xFFU << 16;
		}

		format->mask.rgb.g = 0xFFU << 8;

		break;

	case TV_PIXFMT_RGB16_LE:
	case TV_PIXFMT_RGB16_BE:
	case TV_PIXFMT_BGR16_LE:
	case TV_PIXFMT_BGR16_BE:
		format->bits_per_pixel = 16;
		format->color_depth = 16;
		format->mask.rgb.r = 0x1FU;
		format->mask.rgb.g = 0x3FU << 5;
		format->mask.rgb.b = 0x1FU << 11;
		break;

	case TV_PIXFMT_RGBA16_LE:
	case TV_PIXFMT_RGBA16_BE:
	case TV_PIXFMT_BGRA16_LE:
	case TV_PIXFMT_BGRA16_BE:
		format->bits_per_pixel = 16;
		format->color_depth = 15;
		format->mask.rgb.r = 0x1FU;
		format->mask.rgb.g = 0x1FU << 5;
		format->mask.rgb.b = 0x1FU << 10;
		format->mask.rgb.a = 0x01U << 15;
		break;

	case TV_PIXFMT_ARGB16_LE:
	case TV_PIXFMT_ARGB16_BE:
	case TV_PIXFMT_ABGR16_LE:
	case TV_PIXFMT_ABGR16_BE:
		format->bits_per_pixel = 16;
		format->color_depth = 15;
		format->mask.rgb.r = 0x1FU << 1;
		format->mask.rgb.g = 0x1FU << 6;
		format->mask.rgb.b = 0x1FU << 11;
		format->mask.rgb.a = 0x01U;
		break;

	case TV_PIXFMT_RGBA12_LE:
	case TV_PIXFMT_RGBA12_BE:
	case TV_PIXFMT_BGRA12_LE:
	case TV_PIXFMT_BGRA12_BE:
		format->bits_per_pixel = 16;
		format->color_depth = 12;
		format->mask.rgb.r = 0x0FU;
		format->mask.rgb.g = 0x0FU << 4;
		format->mask.rgb.b = 0x0FU << 8;
		format->mask.rgb.a = 0x0FU << 12;
		break;

	case TV_PIXFMT_ARGB12_LE:
	case TV_PIXFMT_ARGB12_BE:
	case TV_PIXFMT_ABGR12_LE:
	case TV_PIXFMT_ABGR12_BE:
		format->bits_per_pixel = 16;
		format->color_depth = 12;
		format->mask.rgb.r = 0x0FU << 4;
		format->mask.rgb.g = 0x0FU << 8;
		format->mask.rgb.b = 0x0FU << 12;
		format->mask.rgb.a = 0x0FU;
		break;

	case TV_PIXFMT_RGB8:
		format->bits_per_pixel = 8;
		format->color_depth = 8;
		format->mask.rgb.r = 0x07U;
		format->mask.rgb.g = 0x07U << 3;
		format->mask.rgb.b = 0x03U << 6;
		break;

	case TV_PIXFMT_BGR8:
		format->bits_per_pixel = 8;
		format->color_depth = 8;
		format->mask.rgb.r = 0x03U << 6;
		format->mask.rgb.g = 0x07U << 3;
		format->mask.rgb.b = 0x07U;
		break;

	case TV_PIXFMT_RGBA8:
	case TV_PIXFMT_BGRA8:
		format->bits_per_pixel = 8;
		format->color_depth = 7;
		format->mask.rgb.r = 0x03U;
		format->mask.rgb.g = 0x07U << 2;
		format->mask.rgb.b = 0x03U << 5;
		format->mask.rgb.a = 0x01U << 7;
		break;

	case TV_PIXFMT_ARGB8:
	case TV_PIXFMT_ABGR8:
		format->bits_per_pixel = 8;
		format->color_depth = 7;
		format->mask.rgb.r = 0x03U << 1;
		format->mask.rgb.g = 0x07U << 3;
		format->mask.rgb.b = 0x03U << 6;
		format->mask.rgb.a = 0x01U;
		break;

	default:
		return FALSE;
	}

	if (attr & 0x10)
		SWAP (format->mask.rgb.r, format->mask.rgb.b);

	if (attr & 0x40) {
		SWAP (format->mask.yuv.u, format->mask.yuv.v);
		format->vu_order = TRUE;
	} else {
		format->vu_order = FALSE;
	}

	return TRUE;
}

/* Number of set bits. */
static unsigned int
popcnt				(unsigned int		x)
{
	x -= ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	x = (x + (x >> 4)) & 0x0F0F0F0F;

	return (x * 0x01010101) >> 24;
}

/* Note this works only for RGB formats. */
tv_bool
tv_pixel_format_to_pixfmt	(tv_pixel_format *	format)
{
	unsigned int mask;
	unsigned int r_msb;
	unsigned int a_lsb;

	assert (NULL != format);

	if (format->bits_per_pixel < 7
	    || 0 == format->mask.rgb.r
	    || 0 == format->mask.rgb.g
	    || 0 == format->mask.rgb.b)
		return FALSE;

	mask = format->mask.rgb.r | format->mask.rgb.g | format->mask.rgb.b;

	if (format->mask.rgb.g > format->mask.rgb.r) {
		if (format->mask.rgb.g > format->mask.rgb.b)
			return FALSE; /* GRB, GBR */
	} else {
		if (format->mask.rgb.b > format->mask.rgb.g)
			return FALSE; /* RBG, BRG */
	}

	if (0 == format->color_depth)
		format->color_depth = popcnt (mask);

	if (0 == format->mask.rgb.a)
		format->mask.rgb.a = mask
			^ (0xFFFFFFFFUL >> (32 - format->bits_per_pixel));

	if (mask > format->mask.rgb.a) {
		if (format->mask.rgb.a > format->mask.rgb.r
		    || format->mask.rgb.a > format->mask.rgb.b)
			return FALSE; /* XGAX, XAGX */
	}

	a_lsb = (mask >= format->mask.rgb.a);
	r_msb = (format->mask.rgb.r >= format->mask.rgb.b);

	switch (format->color_depth) {
	case 24:
		if (32 == format->bits_per_pixel) {
			static tv_pixfmt mapping [] = {
				TV_PIXFMT_RGBA32_LE, TV_PIXFMT_RGBA32_BE,
				TV_PIXFMT_BGRA32_LE, TV_PIXFMT_BGRA32_BE,
				TV_PIXFMT_ARGB32_LE, TV_PIXFMT_ARGB32_BE,
				TV_PIXFMT_ABGR32_LE, TV_PIXFMT_ABGR32_BE,
			};

			format->pixfmt = mapping [a_lsb * 4 + r_msb * 2
						  + format->big_endian];
		} else {
			static tv_pixfmt mapping [] = {
				TV_PIXFMT_RGB24_LE, TV_PIXFMT_BGR24_LE
			};

			format->pixfmt = mapping [r_msb];
		}

		break;

	case 16:
	{
		 static tv_pixfmt mapping [] = {
			TV_PIXFMT_RGB16_LE, TV_PIXFMT_RGB16_BE,
			TV_PIXFMT_BGR16_LE, TV_PIXFMT_BGR16_BE,
		};

		format->pixfmt = mapping [r_msb * 2 + format->big_endian];
		break;
	}

	case 15:
	{
		static tv_pixfmt mapping [] = {
			TV_PIXFMT_RGBA16_LE, TV_PIXFMT_RGBA16_BE,
			TV_PIXFMT_BGRA16_LE, TV_PIXFMT_BGRA16_BE,
			TV_PIXFMT_ARGB16_LE, TV_PIXFMT_ARGB16_BE,
			TV_PIXFMT_ABGR16_LE, TV_PIXFMT_ABGR16_BE,
		};

		format->pixfmt = mapping [a_lsb * 4 + r_msb * 2
					  + format->big_endian];
		break;
	}

	case 12:
	{
		static tv_pixfmt mapping [] = {
			TV_PIXFMT_RGBA12_LE, TV_PIXFMT_RGBA12_BE,
			TV_PIXFMT_BGRA12_LE, TV_PIXFMT_BGRA12_BE,
			TV_PIXFMT_ARGB12_LE, TV_PIXFMT_ARGB12_BE,
			TV_PIXFMT_ABGR12_LE, TV_PIXFMT_ABGR12_BE,
		};

		format->pixfmt = mapping [a_lsb * 4 + r_msb * 2
					  + format->big_endian];
		break;
	}

	case 8:
	{
		static tv_pixfmt mapping [] = {
			TV_PIXFMT_RGB8, TV_PIXFMT_BGR8
		};

		format->pixfmt = mapping [r_msb];
		break;
	}

	case 7:
	{
		static tv_pixfmt mapping [] = {
			TV_PIXFMT_RGBA8, TV_PIXFMT_BGRA8,
			TV_PIXFMT_ARGB8, TV_PIXFMT_ABGR8,
		};

		format->pixfmt = mapping [a_lsb * 2 + r_msb];
		break;
	}

	default:
		return FALSE;
	}

	format->_reserved1 = 0;

	format->uv_hscale = 1;
	format->uv_vscale = 1;

	format->planar = 0;
	format->vu_order = 0;

	return TRUE;
}
