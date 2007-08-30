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

/* $Id: pixel_format.c,v 1.9 2007-08-30 14:14:09 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"		/* Z_BYTE_ORDER */
#endif

#include "misc.h"
#include "pixel_format.h"

const char *
tv_pixfmt_name			(tv_pixfmt		pixfmt)
{
	switch (pixfmt) {

#undef CASE
#define CASE(s) case TV_PIXFMT_##s : return #s ;

	CASE (NONE)
	CASE (NV12)
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
	CASE (HM12)
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
	CASE (SBGGR)

	case TV_PIXFMT_RESERVED1:
	case TV_PIXFMT_RESERVED3:
		break;
	}

	return NULL;
}

#define PIXEL_FORMAT(pixfmt, colspc, bits_per_pixel, color_depth,	\
		     hmask, vmask, uv_hshift, uv_vshift,		\
		     big_endian, n_planes, vu_order, r, g, b, a)	\
	{ #pixfmt,							\
	  TV_PIXFMT_##pixfmt,						\
	  colspc,							\
	  bits_per_pixel,						\
	  color_depth,							\
	  hmask,							\
	  vmask,							\
	  uv_hshift,							\
	  uv_vshift,							\
	  big_endian,							\
	  n_planes,							\
	  vu_order,							\
	  .mask = { .rgb = { r, g, b, a } } }

#if Z_BYTE_ORDER == Z_LITTLE_ENDIAN
#  define PACKED(fmt, colspc, bits_per_pixel, color_depth, big_endian,	\
                 vu_order, x, y, z, a)					\
	[TV_PIXFMT_##fmt] = PIXEL_FORMAT (fmt, colspc,			\
        	bits_per_pixel, color_depth, 0, 0, 0, 0, big_endian,	\
		1, vu_order, x, y, z, a)

#elif Z_BYTE_ORDER == Z_BIG_ENDIAN
#  define PACKED(fmt, colspc, bits_per_pixel, color_depth, big_endian,	\
	         vu_order, x, y, z, a)					\
	[TV_PIXFMT_##fmt] = PIXEL_FORMAT (fmt, colspc,			\
        	bits_per_pixel, color_depth, 0, 0, 0, 0, !(big_endian),	\
		1, vu_order, x, y, z, a)
#else
#  error unknown endianess
#endif

#define PLANAR(fmt, color_depth, uv_hshift, uv_vshift, vu_order)	\
	[TV_PIXFMT_##fmt] = PIXEL_FORMAT (fmt, TV_COLSPC_YUV, 8,	\
		color_depth,						\
		(1 << uv_hshift) - 1, (1 << uv_vshift) - 1,		\
		uv_hshift, uv_vshift, /* BE */ FALSE, /* n_planes */ 3,	\
		vu_order, 0xFF, 0xFF, 0xFF, 0)

#define PACKED_YUV24(fmt, bpp, vu_order, x, y, z, a)			\
	PACKED(fmt##_LE, TV_COLSPC_YUV,					\
	       bpp, 24, FALSE, vu_order, x, y, z, a),			\
	PACKED(fmt##_BE, TV_COLSPC_YUV,					\
	       bpp, 24,  TRUE, vu_order, x, y, z, a)

#define YUYV(fmt, vu_order)						\
	[TV_PIXFMT_##fmt] = PIXEL_FORMAT (fmt, TV_COLSPC_YUV, 16, 16,	\
		/* mask */ 0x1, 0x0, /* shift */ 0, 0,			\
		/* BE */ FALSE, /* n_planes */ 1, vu_order,		\
		0xFF, 0xFF, 0xFF, 0)

#define PACKED_RGB24(fmt, bpp, x, y, z, a)				\
	PACKED(fmt##_LE, TV_COLSPC_RGB,					\
	       bpp, 24, FALSE, FALSE, x, y, z, a),			\
	PACKED(fmt##_BE, TV_COLSPC_RGB,					\
	       bpp, 24,  TRUE, FALSE, x, y, z, a)

#define PACKED16(fmt, x, y, z, a)					\
	PACKED(fmt##_LE, TV_COLSPC_RGB,					\
	       16, 16, FALSE, FALSE, x, y, z, a),			\
	PACKED(fmt##_BE, TV_COLSPC_RGB,					\
	       16, 16,  TRUE, FALSE, x, y, z, a)

#define PACKED8(fmt, x, y, z, a)					\
	[TV_PIXFMT_##fmt] = PIXEL_FORMAT (fmt, TV_COLSPC_RGB, 8, 8,	\
		0, 0, 0, 0, FALSE, 1, FALSE, x, y, z, a)

static const tv_pixel_format
pixel_formats [] = {
	PLANAR (YUV444, 24, 0, 0, FALSE), 
	PLANAR (YVU444, 24, 0, 0, TRUE), 
	PLANAR (YUV422, 16, 1, 0, FALSE), 
	PLANAR (YVU422, 16, 1, 0, TRUE), 
	PLANAR (YUV411, 12, 2, 0, FALSE), 
	PLANAR (YVU411, 12, 2, 0, TRUE), 
	PLANAR (YUV420, 12, 1, 1, FALSE), 
	PLANAR (YVU420, 12, 1, 1, TRUE), 
	PLANAR (YUV410,  9, 2, 2, FALSE), 
	PLANAR (YVU410,  9, 2, 2, TRUE),
	[TV_PIXFMT_NV12] = PIXEL_FORMAT (NV12, TV_COLSPC_YUV, 8, 12,
					 /* mask */ 0x1, 0x1,
					 /* shift */ 0, 1,
					 /* BE */ FALSE, /* n_planes */ 2,
					 /* vu */ FALSE, 0xFF, 0xFF, 0xFF, 0),

	PACKED_YUV24 (YUVA32, 32, FALSE, 0xFF, 0xFF00, 0xFF0000, 0xFF000000),
	PACKED_YUV24 (YVUA32, 32,  TRUE, 0xFF, 0xFF0000, 0xFF00, 0xFF000000),

	PACKED_YUV24 (YUV24, 24, FALSE, 0xFF, 0xFF00, 0xFF0000, 0),
	PACKED_YUV24 (YVU24, 24, FALSE, 0xFF, 0xFF0000, 0xFF00, 0),

	YUYV (YUYV, FALSE),
	YUYV (YVYU,  TRUE),
	YUYV (UYVY, FALSE),
	YUYV (VYUY,  TRUE),

	[TV_PIXFMT_Y8] = PIXEL_FORMAT (Y8, TV_COLSPC_YUV, 8, 8,
				       /* mask */ 0, 0,
				       /* shift */ 0, 0,
				       /* BE */ FALSE, /* n_planes */ 1,
				       /* vu */ FALSE, 0xFF, 0, 0, 0),

	[TV_PIXFMT_HM12] = PIXEL_FORMAT (HM12, TV_COLSPC_YUV, 8, 12,
					 /* mask */ 0xF, 0xF,
					 /* shift */ 0, 1,
					 /* BE */ FALSE, /* n_planes */ 2,
					 /* vu */ FALSE, 0xFF, 0xFF, 0xFF, 0),

	PACKED_RGB24 (RGBA32, 32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000),
	PACKED_RGB24 (BGRA32, 32, 0xFF0000, 0xFF00, 0xFF, 0xFF000000),

	PACKED (RGB24_LE, TV_COLSPC_RGB,
		24, 24, FALSE, FALSE, 0xFF, 0xFF00, 0xFF0000, 0),
	PACKED (BGR24_LE, TV_COLSPC_RGB,
		24, 24, FALSE, FALSE, 0xFF0000, 0xFF00, 0xFF, 0),

	PACKED16 (RGB16, 0x001F, 0x07E0, 0xF800, 0),
	PACKED16 (BGR16, 0xF800, 0x07E0, 0x001F, 0),

	PACKED16 (RGBA16, 0x001F, 0x03E0, 0x7C00, 0x8000),
	PACKED16 (BGRA16, 0x7C00, 0x03E0, 0x001F, 0x8000),
	PACKED16 (ARGB16, 0x003E, 0x07C0, 0xF800, 0x0001),
	PACKED16 (ABGR16, 0xF800, 0x07C0, 0x003E, 0x0001),

	PACKED16 (RGBA12, 0x000F, 0x00F0, 0x0F00, 0xF000),
	PACKED16 (BGRA12, 0x0F00, 0x00F0, 0x000F, 0xF000),
	PACKED16 (ARGB12, 0x00F0, 0x0F00, 0xF000, 0x000F),
	PACKED16 (ABGR12, 0xF000, 0x0F00, 0x00F0, 0x000F),

	PACKED8 (RGB8, 0x07, 0x38, 0xC0, 0),
	PACKED8 (BGR8, 0xE0, 0x1C, 0x03, 0),

	PACKED8 (RGBA8, 0x03, 0x1C, 0x60, 0x80),
	PACKED8 (BGRA8, 0x60, 0x1C, 0x03, 0x80),
	PACKED8 (ARGB8, 0x06, 0x38, 0xC0, 0x01),
	PACKED8 (ABGR8, 0xC0, 0x38, 0x06, 0x01),

	[TV_PIXFMT_SBGGR] = PIXEL_FORMAT (SBGGR, TV_COLSPC_RGB, 8, 24,
					  /* mask */ 0x1, 0x1,
					  /* shift */ 0, 0,
					  /* BE */ FALSE, /* n_planes */ 1,
					  /* vu */ FALSE, 0xFF, 0xFF, 0xFF, 0),
};

unsigned int
tv_pixfmt_bytes_per_pixel	(tv_pixfmt		pixfmt)
{
	unsigned int index = (unsigned int) pixfmt;

	if (index > N_ELEMENTS (pixel_formats))
		return 0;

	return pixel_formats[index].bits_per_pixel >> 3;
}

const tv_pixel_format *
tv_pixel_format_from_pixfmt	(tv_pixfmt		pixfmt)
{
	unsigned int index = (unsigned int) pixfmt;
	const tv_pixel_format *pf;

	if (index > N_ELEMENTS (pixel_formats))
		return NULL;

	pf = &pixel_formats[index];

	if (0 == pf->bits_per_pixel)
		return NULL;

	return pf;
}

#ifdef HAVE_BUILTIN_POPCOUNT
#  define popcnt(x) __builtin_popcount (x)
#else

/* Number of set bits. */
static unsigned int
popcnt				(unsigned int		x)
{
	x -= ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	x = (x + (x >> 4)) & 0x0F0F0F0F;

	return (x * 0x01010101) >> 24;
}

#endif

/* Note this works only for RGB formats. */
tv_pixfmt
tv_pixel_format_to_pixfmt	(const tv_pixel_format *format)
{
	unsigned int color_depth;
	unsigned int mask;
	unsigned int mask_a;
	unsigned int r_msb;
	unsigned int a_lsb;


	assert (NULL != format);

	if (format->bits_per_pixel < 7
	    || 0 == format->mask.rgb.r
	    || 0 == format->mask.rgb.g
	    || 0 == format->mask.rgb.b)
		return TV_PIXFMT_UNKNOWN;

	mask = format->mask.rgb.r | format->mask.rgb.g | format->mask.rgb.b;

	if (format->mask.rgb.g > format->mask.rgb.r) {
		if (format->mask.rgb.g > format->mask.rgb.b)
			return TV_PIXFMT_UNKNOWN; /* GRB, GBR */
	} else {
		if (format->mask.rgb.b > format->mask.rgb.g)
			return TV_PIXFMT_UNKNOWN; /* RBG, BRG */
	}

	color_depth = format->color_depth;
	if (0 == color_depth)
		color_depth = popcnt (mask);

	mask_a = format->mask.rgb.a;
	if (0 == mask_a)
		mask_a = mask
			^ (0xFFFFFFFFUL >> (32 - format->bits_per_pixel));

	if (mask > mask_a) {
		if (mask_a > format->mask.rgb.r
		    || format->mask.rgb.a > format->mask.rgb.b)
			return TV_PIXFMT_UNKNOWN; /* XGAX, XAGX */
	}

	a_lsb = (mask >= mask_a);
	r_msb = (format->mask.rgb.r >= format->mask.rgb.b);

	switch (color_depth) {
	case 24:
		if (32 == format->bits_per_pixel) {
			static tv_pixfmt mapping [] = {
				TV_PIXFMT_RGBA32_LE, TV_PIXFMT_RGBA32_BE,
				TV_PIXFMT_BGRA32_LE, TV_PIXFMT_BGRA32_BE,
				TV_PIXFMT_ARGB32_LE, TV_PIXFMT_ARGB32_BE,
				TV_PIXFMT_ABGR32_LE, TV_PIXFMT_ABGR32_BE,
			};

			return mapping [a_lsb * 4 + r_msb * 2
					+ format->big_endian];
		} else {
			static tv_pixfmt mapping [] = {
				TV_PIXFMT_RGB24_LE, TV_PIXFMT_BGR24_LE
			};

			return mapping [r_msb];
		}

	case 16:
	{
		static tv_pixfmt mapping [] = {
			TV_PIXFMT_RGB16_LE, TV_PIXFMT_RGB16_BE,
			TV_PIXFMT_BGR16_LE, TV_PIXFMT_BGR16_BE,
		};

		return mapping [r_msb * 2 + format->big_endian];
	}

	case 15:
	{
		static tv_pixfmt mapping [] = {
			TV_PIXFMT_RGBA16_LE, TV_PIXFMT_RGBA16_BE,
			TV_PIXFMT_BGRA16_LE, TV_PIXFMT_BGRA16_BE,
			TV_PIXFMT_ARGB16_LE, TV_PIXFMT_ARGB16_BE,
			TV_PIXFMT_ABGR16_LE, TV_PIXFMT_ABGR16_BE,
		};

		return mapping [a_lsb * 4 + r_msb * 2 + format->big_endian];
	}

	case 12:
	{
		static tv_pixfmt mapping [] = {
			TV_PIXFMT_RGBA12_LE, TV_PIXFMT_RGBA12_BE,
			TV_PIXFMT_BGRA12_LE, TV_PIXFMT_BGRA12_BE,
			TV_PIXFMT_ARGB12_LE, TV_PIXFMT_ARGB12_BE,
			TV_PIXFMT_ABGR12_LE, TV_PIXFMT_ABGR12_BE,
		};

		return mapping [a_lsb * 4 + r_msb * 2 + format->big_endian];
	}

	case 8:
	{
		static tv_pixfmt mapping [] = {
			TV_PIXFMT_RGB8, TV_PIXFMT_BGR8
		};

		return mapping [r_msb];
	}

	case 7:
	{
		static tv_pixfmt mapping [] = {
			TV_PIXFMT_RGBA8, TV_PIXFMT_BGRA8,
			TV_PIXFMT_ARGB8, TV_PIXFMT_ABGR8,
		};

		return mapping [a_lsb * 2 + r_msb];
		break;
	}
	
	default:
		break;
	}

	return TV_PIXFMT_UNKNOWN;
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
