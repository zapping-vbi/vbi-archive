/*
 *  libzvbi - Closed Caption and Teletext rendering
 *
 *  Copyright (C) 2000-2008 Michael H. Schimek
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

/* $Id: exp-gfx.c,v 1.1.2.2 2008-08-19 16:36:54 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "misc.h"
#include "exp-gfx.h"
#include "ttx_charset.h"
#include "ttx_decoder.h"
#include "wstfont2.xbm"

/* Teletext character cell dimensions - hardcoded (DRCS) */

#define TCW 12
#define TCH 10

#define TCPL (wstfont2_width / TCW * wstfont2_height / TCH)

static unsigned int
_vbi_pixfmt_bytes_per_pixel	(vbi_pixfmt		pixfmt)
{
	switch (pixfmt) {
	case VBI_PIXFMT_YUVA24_LE:
	case VBI_PIXFMT_RGBA24_LE:
	case VBI_PIXFMT_RGBA24_BE:
	case VBI_PIXFMT_BGRA24_LE:
	case VBI_PIXFMT_BGRA24_BE:
		return 4;

	case VBI_PIXFMT_RGB24_LE:
	case VBI_PIXFMT_RGB24_BE:
		return 3;

	case VBI_PIXFMT_YUYV:
	case VBI_PIXFMT_UYVY:
	case VBI_PIXFMT_BGR16_LE:
	case VBI_PIXFMT_BGR16_BE:
	case VBI_PIXFMT_BGRA15_LE:
	case VBI_PIXFMT_BGRA15_BE:
	case VBI_PIXFMT_BGRA12_LE:
	case VBI_PIXFMT_BGRA12_BE:
		return 2;

	case VBI_PIXFMT_YUV444:
	case VBI_PIXFMT_YUV422:
	case VBI_PIXFMT_YUV420:
	case VBI_PIXFMT_Y8:
        case VBI_PIXFMT_STATIC_8:
        case VBI_PIXFMT_PSEUDO_8:
		return 1;

	case VBI_PIXFMT_NONE:
		return 0;
	}

	return 0;
}

/**
 * @internal
 *
 * unicode_wstfont2() subroutine.
 */
_vbi_inline unsigned int
unicode_wstfont2_special	(unsigned int		c,
				 vbi_bool		italic)
{
	static const uint16_t specials [] = {
		0x01B5, 0x2016, 0x01CD, 0x01CE, 0x0229, 0x0251, 0x02DD, 0x02C6,
		0x02C7, 0x02C9, 0x02CA, 0x02CB, 0x02CD, 0x02CF, 0x02D8, 0x02D9,
		0x02DA, 0x02DB, 0x02DC, 0x2014, 0x2018, 0x2019, 0x201C,	0x201D,
		0x20A0, 0x2030, 0x20AA, 0x2122, 0x2126, 0x215B, 0x215C, 0x215D,
		0x215E, 0x2190, 0x2191, 0x2192, 0x2193, 0x25A0, 0x266A, 0x20A4,
		0xE75F };
	const unsigned int invalid = 357;
	unsigned int i;

	for (i = 0; i < N_ELEMENTS (specials); ++i)
		if (c == specials[i]) {
			if (italic)
				return i + 41 * 32;
			else
				return i + 10 * 32;
		}

	return invalid;
}

/**
 * @internal
 * @param c Unicode.
 * @param italic @c TRUE to switch to slanted character set (doesn't affect
 *   Hebrew and Arabic). If this is a G1 block graphic character switch
 *   to separated block mosaic set.
 * 
 * Translates Unicode character to glyph number in wstfont2 image. 
 * 
 * @return
 * Glyph number.
 */
static unsigned int
unicode_wstfont2		(unsigned int		c,
				 vbi_bool		italic)
{
	const unsigned int invalid = 357;

	if (c < 0x0180) {
		if (c < 0x0080) {
			if (c < 0x0020)
				return invalid;
			else /* %3 Basic Latin (ASCII) 0x0020 ... 0x007F */
				c = c - 0x0020 + 0 * 32;
		} else if (c < 0x00A0)
			return invalid;
		else /* %3 Latin-1 Sup, Latin Ext-A 0x00A0 ... 0x017F */
			c = c - 0x00A0 + 3 * 32;
	} else if (c < 0xEE00) {
		if (c < 0x0460) {
			if (c < 0x03D0) {
				if (c < 0x0370)
					return unicode_wstfont2_special
						(c, italic);
				else /* %5 Greek 0x0370 ... 0x03CF */
					c = c - 0x0370 + 12 * 32;
			} else if (c < 0x0400)
				return invalid;
			else /* %5 Cyrillic 0x0400 ... 0x045F */
				c = c - 0x0400 + 15 * 32;
		} else if (c < 0x0620) {
			if (c < 0x05F0) {
				if (c < 0x05D0)
					return invalid;
				else /* %6 Hebrew 0x05D0 ... 0x05EF */
					return c - 0x05D0 + 18 * 32;
			} else if (c < 0x0600)
				return invalid;
			else /* %6 Arabic 0x0600 ... 0x061F */
				return c - 0x0600 + 19 * 32;
		} else if (c >= 0xE600 && c < 0xE740)
			return c - 0xE600 + 19 * 32; /* %6 Arabic (TTX) */
		else
			return unicode_wstfont2_special (c, italic);
	} else if (c < 0xEF00) { /* %3 G1 Graphics */
		return (c ^ 0x20) - 0xEE00 + 23 * 32;
	} else if (c < 0xF000) { /* %4 G3 Graphics */
		return c - 0xEF20 + 27 * 32;
	} else /* 0xF000 ... 0xF7FF reserved for DRCS */
		return invalid;

	if (italic)
		return c + 31 * 32;
	else
		return c;
}

/**
 * @internal
 *
 * Copies even lines to odd lines.
 */
static void
line_doubler			(void *			buffer,
				 const vbi_image_format *format,
				 unsigned int		x,
				 unsigned int		y,
				 unsigned int		width,
				 unsigned int		height)
{
	uint8_t *canvas;
	unsigned int byte_width;
	unsigned int bytes_per_line;
	unsigned int bytes_per_pixel;

//	assert (VBI_PIXFMT_IS_PACKED (format->pixfmt));
	assert (x + width <= format->width);
	assert (y + height <= format->height);
	assert (0 == (height % 2));

	bytes_per_pixel = _vbi_pixfmt_bytes_per_pixel (format->pixfmt);
	byte_width = width * bytes_per_pixel;

	bytes_per_line = format->bytes_per_line[0];

	if (bytes_per_line <= 0) {
		bytes_per_line = byte_width;
	} else {
		assert (byte_width <= bytes_per_line);
	}

	canvas = ((uint8_t *) buffer)
		+ format->offset[0]
		+ y * bytes_per_line
		+ x * bytes_per_pixel;

	while (height > 0) {
		memcpy (canvas + bytes_per_line, canvas, byte_width);
		canvas += bytes_per_line * 2;
		height -= 2;
	}
}

#define TRANS(v)							\
	SATURATE((((((int)((v) & 0xFF) - 128)				\
		  * contrast) >> 6) + brightness), 0, 255)

/* Find first set bit in constant, e.g.
   0xFFFFFFFF -> 32, 0x123 -> 9, 1 -> 1, 0 -> 1 */
#define FFS2(m) ((m) & 0x2 ? 2 : 1)
#define FFS4(m) ((m) & 0xC ? 2 + FFS2 ((m) >> 2) : FFS2 (m))
#define FFS8(m) ((m) & 0xF0 ? 4 + FFS4 ((m) >> 4) : FFS4 (m))
#define FFS16(m) ((m) & 0xFF00 ? 8 + FFS8 ((m) >> 8) : FFS8 (m))
#define FFS32(m) ((m) & 0xFFFF0000 ? 16 + FFS16 ((m) >> 16) : FFS16 (m))

#define CONV(v, n, m)							\
	((FFS32 (m) > n) ?						\
	 ((v) << (FFS32 (m) - n)) & m :					\
	 ((v) >> (n - FFS32 (m))) & m)

#define TRANS_CONV(v, n, m)						\
	((FFS32 (m) > 8) ?						\
	 (TRANS ((v) >> (n - 8)) << (FFS32 (m) - 8)) & m :		\
	 (TRANS ((v) >> (n - 8)) >> (8 - FFS32 (m))) & m)

/* Converts 0xBBGGRR value v to another value, transposing by
   brightness and contrast, then shifting and masking
   color bits to positions given by r, g, b, a mask */  
#define RGBA_CONV(v, r, g, b, a)					\
	(+ TRANS_CONV (v, 8, r)						\
	 + TRANS_CONV (v, 16, g)					\
	 + TRANS_CONV (v, 24, b)					\
	 + CONV (alpha, 8, a))

/* Like RGBA_CONV for reversed endian */
#define RGBA_CONV_SWAB32(v, r, g, b, a)					\
	(+ TRANS_CONV (v, 8, SWAB32 (r))				\
	 + TRANS_CONV (v, 16, SWAB32 (g))				\
	 + TRANS_CONV (v, 24, SWAB32 (b))				\
	 + CONV (alpha, 8, SWAB32 (a)))

#define PUSH(p, type, value)						\
	*((type *) p) = value; p += sizeof (type);

#define RGBA_CONV4(r, g, b, a, endian)					\
	while (color_size-- > 0) {					\
		unsigned int value = *color++;				\
									\
		switch (Z_BYTE_ORDER) {					\
		case Z_LITTLE_ENDIAN:					\
			if (0 == endian)				\
				value = RGBA_CONV (value, r, g, b, a);	\
			else						\
				value = RGBA_CONV_SWAB32 (value, r, g, b, a); \
			break;						\
		case Z_BIG_ENDIAN:					\
			if (0 == endian)				\
				value = RGBA_CONV_SWAB32 (value, r, g, b, a); \
			else						\
				value = RGBA_CONV (value, r, g, b, a);	\
			break;						\
		}							\
		PUSH (d, uint32_t, value);				\
	}

#define RGBA_CONV2(r, g, b, a, endian)					\
	while (color_size-- > 0) {					\
		unsigned int value = *color++;				\
									\
		value = RGBA_CONV (value, r, g, b, a);			\
		switch (Z_BYTE_ORDER) {					\
		case Z_LITTLE_ENDIAN:					\
			if (0 == endian) {				\
				PUSH (d, uint16_t, value);		\
			} else {					\
				d[0] = value >> 8;			\
				d[1] = value;				\
				d += 2;					\
			}						\
			break;						\
		case Z_BIG_ENDIAN:					\
			if (1 == endian) {				\
				PUSH (d, uint16_t, value);		\
			} else {					\
				d[0] = value;				\
				d[1] = value >> 8;			\
				d += 2;					\
			}						\
			break;						\
		}							\
	}

#define RGBA_CONV1(r, g, b, a)						\
	while (color_size-- > 0) {					\
		unsigned int value = *color++;				\
									\
		*d++ = RGBA_CONV (value, r, g, b, a);			\
	}

/**
 * @internal
 * @param buffer Output. The buffer must be large enough to hold
 *   @a n entries of vbi_pixfmt_bytes_per_pixel(@a pixfmt) each.
 * @param pixfmt Pixel format to convert to.
 * @param color Array of vbi_rgba types.
 * @param color_size Number of elements in @a color array.
 * @param brightness Change brightness: 0 dark ... 255 bright, 128
 *   for no change.
 * @param contrast Change contrast: -128 inverse ... 0 none ...
 *   127 maximum, 64 for no change.
 * @param alpha Replacement alpha value: 0 ... 255.
 *
 * Converts vbi_rgba types to pixels of desired format.
 *
 * @returns
 * @c FALSE if parameters are invalid.
 *
 * @bug
 * YUV pixel formats are not supported.
 */
static vbi_bool
vbi_rgba_conv			(void *			buffer,
				 vbi_pixfmt		pixfmt,
				 const vbi_rgba *	color,
				 unsigned int		color_size,
				 int			brightness,
				 int			contrast,
				 int			alpha)
{
	uint8_t *d = buffer;
	unsigned int value;

	switch (pixfmt) {
	case VBI_PIXFMT_RGBA24_LE:
		RGBA_CONV4 (0xFF, 0xFF00, 0xFF0000, 0xFF000000, 0);
		break;
	case VBI_PIXFMT_RGBA24_BE:
		RGBA_CONV4 (0xFF, 0xFF00, 0xFF0000, 0xFF000000, 3);
		break;
	case VBI_PIXFMT_BGRA24_LE:
		RGBA_CONV4 (0xFF0000, 0xFF00, 0xFF, 0xFF000000, 0);
		break;
	case VBI_PIXFMT_BGRA24_BE:
		RGBA_CONV4 (0xFF0000, 0xFF00, 0xFF, 0xFF000000, 3);
		break;

	case VBI_PIXFMT_RGB24_LE:
		while (color_size-- > 0) {
			value = *color++;

			d[0] = TRANS (value & 0xFF);
			d[1] = TRANS ((value >> 8) & 0xFF);
			d[2] = TRANS ((value >> 16) & 0xFF);
			d += 3;
		}
		break;

	case VBI_PIXFMT_RGB24_BE:
		while (color_size-- > 0) {
			value = *color++;

			d[0] = TRANS ((value >> 16) & 0xFF);
			d[1] = TRANS ((value >> 8) & 0xFF);
			d[2] = TRANS (value & 0xFF);
			d += 3;
		}
		break;

//	case VBI_PIXFMT_RGB16_LE:
//		RGBA_CONV2 (0x001F, 0x07E0, 0xF800, 0, 0);
//		break;
//	case VBI_PIXFMT_RGB16_BE:
//		RGBA_CONV2 (0x001F, 0x07E0, 0xF800, 0, 1);
//		break;
	case VBI_PIXFMT_BGR16_LE:
		RGBA_CONV2 (0xF800, 0x07E0, 0x001F, 0, 0);
		break;
	case VBI_PIXFMT_BGR16_BE:
		RGBA_CONV2 (0xF800, 0x07E0, 0x001F, 0, 1);
		break;

//	case VBI_PIXFMT_RGBA15_LE:
//		RGBA_CONV2 (0x001F, 0x03E0, 0x7C00, 0x8000, 0);
//		break;
//	case VBI_PIXFMT_RGBA15_BE:
//		RGBA_CONV2 (0x001F, 0x03E0, 0x7C00, 0x8000, 1);
//		break;
	case VBI_PIXFMT_BGRA15_LE:
		RGBA_CONV2 (0x7C00, 0x03E0, 0x001F, 0x8000, 0);
		break;
	case VBI_PIXFMT_BGRA15_BE:
		RGBA_CONV2 (0x7C00, 0x03E0, 0x001F, 0x8000, 1);
		break;
//	case VBI_PIXFMT_ARGB15_LE:
//		RGBA_CONV2 (0x003E, 0x07C0, 0xF800, 0x0001, 0);
//		break;
//	case VBI_PIXFMT_ARGB15_BE:
//		RGBA_CONV2 (0x003E, 0x07C0, 0xF800, 0x0001, 1);
//		break;
//	case VBI_PIXFMT_ABGR15_LE:
//		RGBA_CONV2 (0xF800, 0x07C0, 0x003E, 0x0001, 0);
//		break;
//	case VBI_PIXFMT_ABGR15_BE:
//		RGBA_CONV2 (0xF800, 0x07C0, 0x003E, 0x0001, 1);
//		break;

//	case VBI_PIXFMT_RGBA12_LE:
//		RGBA_CONV2 (0x000F, 0x00F0, 0x0F00, 0xF000, 0);
//		break;
//	case VBI_PIXFMT_RGBA12_BE:
//		RGBA_CONV2 (0x000F, 0x00F0, 0x0F00, 0xF000, 1);
//		break;
	case VBI_PIXFMT_BGRA12_LE:
		RGBA_CONV2 (0x0F00, 0x00F0, 0x000F, 0xF000, 0);
		break;
	case VBI_PIXFMT_BGRA12_BE:
		RGBA_CONV2 (0x0F00, 0x00F0, 0x000F, 0xF000, 1);
		break;
//	case VBI_PIXFMT_ARGB12_LE:
//		RGBA_CONV2 (0x00F0, 0x0F00, 0xF000, 0x000F, 0);
//		break;
//	case VBI_PIXFMT_ARGB12_BE:
//		RGBA_CONV2 (0x00F0, 0x0F00, 0xF000, 0x000F, 1);
//		break;
//	case VBI_PIXFMT_ABGR12_LE:
//		RGBA_CONV2 (0xF000, 0x0F00, 0x000F, 0x000F, 0);
//		break;
//	case VBI_PIXFMT_ABGR12_BE:
//		RGBA_CONV2 (0xF000, 0x0F00, 0x000F, 0x000F, 1);
//		break;

//	case VBI_PIXFMT_RGB8:
//		RGBA_CONV1 (0x07, 0x38, 0xC0, 0);
//		break;
//	case VBI_PIXFMT_BGR8:
//		RGBA_CONV1 (0xE0, 0x1C, 0x03, 0);
//		break;

//	case VBI_PIXFMT_RGBA7:
//		RGBA_CONV1 (0x03, 0x1C, 0x60, 0x80);
//		break;
//	case VBI_PIXFMT_BGRA7:
//		RGBA_CONV1 (0x60, 0x1C, 0x03, 0x80);
//		break;
//	case VBI_PIXFMT_ARGB7:
//		RGBA_CONV1 (0x06, 0x38, 0xC0, 0x01);
//		break;
//	case VBI_PIXFMT_ABGR7:
//		RGBA_CONV1 (0xC0, 0x38, 0x06, 0x01);
//		break;

	case VBI_PIXFMT_STATIC_8:
		value = 0;
		while (color_size-- > 0)
			*d++ = value++;
		break;

	case VBI_PIXFMT_PSEUDO_8:
		while (color_size-- > 0)
			*d++ = *color++;
		break;

	default:
		warning (NULL, "Invalid pixfmt %u (%s).",
			 (unsigned int) pixfmt,
			 vbi_pixfmt_name (pixfmt));

		return FALSE;
	}

	return TRUE;
}

#define COLOR_MAP_ELEMENTS N_ELEMENTS (((vbi_page *) 0)->color_map)

struct color_map {
	uint8_t			map [3 * COLOR_MAP_ELEMENTS * 4];
	void *			fg [4];
	void *			bg [4];
};

static vbi_bool
color_map_init			(struct color_map *	cm,
				 const vbi_page *	pg,
				 vbi_pixfmt		pixfmt,
				 unsigned int		bytes_per_pixel,
				 int			brightness,
				 int			contrast,
				 vbi_bool		alpha)
{
	if (!vbi_rgba_conv (cm->map, pixfmt,
			    pg->color_map, COLOR_MAP_ELEMENTS,
			    brightness, contrast, /* alpha */ 0xFF))
		return FALSE;

	if (!alpha) {
		unsigned int i;

		for (i = 0; i < 4; ++i) {
			cm->fg[i] = cm->map;
			cm->bg[i] = cm->map;
		}

		return TRUE;
	}

	if (VBI_PIXFMT_STATIC_8 == pixfmt
	    || VBI_PIXFMT_PSEUDO_8 == pixfmt) {
		return FALSE;
	}

	/* Opaque fg. and bg. */
	cm->fg[VBI_OPAQUE] = cm->map;
	cm->bg[VBI_OPAQUE] = cm->map;

	/* Opaque fg., transl. bg. */
	cm->fg[VBI_TRANSLUCENT] = cm->map;
	cm->bg[VBI_TRANSLUCENT] = (uint8_t *) cm->map
		+ COLOR_MAP_ELEMENTS * 1 * bytes_per_pixel;

	/* Opaque fg., transp. bg. */
	cm->fg[VBI_TRANSPARENT] = cm->map;
	cm->bg[VBI_TRANSPARENT] = (uint8_t *) cm->map
		+ COLOR_MAP_ELEMENTS * 2 * bytes_per_pixel;

	/* Transp. fg. and bg. */
	cm->fg[VBI_TRANSPARENT_SPACE] = cm->bg[VBI_TRANSPARENT];
	cm->bg[VBI_TRANSPARENT_SPACE] = cm->bg[VBI_TRANSPARENT];

	if (!vbi_rgba_conv (cm->bg[VBI_TRANSLUCENT], pixfmt,
			    pg->color_map, COLOR_MAP_ELEMENTS,
			    brightness, contrast, /* alpha */ 0x7F))
		return FALSE;

	if (!vbi_rgba_conv (cm->bg[VBI_TRANSPARENT], pixfmt,
			    pg->color_map, COLOR_MAP_ELEMENTS,
			    brightness, contrast, /* alpha */ 0x00))
		return FALSE;

	return TRUE;
}

#define PIXEL(d, i, s, j)						\
	(1 == bpp ? ((uint8_t *)(d))[i] =				\
		    ((const uint8_t *)(s))[j] :				\
	 (2 == bpp ? ((uint16_t *)(d))[i] =				\
		     ((const uint16_t *)(s))[j] :			\
	  (3 == bpp ? memcpy (((uint8_t *)(d)) + (i) * 3,		\
			      ((const uint8_t *)(s)) + (j) * 3, 3) :	\
	   (4 == bpp ? ((uint32_t *)(d))[i] =				\
		       ((const uint32_t *)(s))[j] :			\
	    assert (0)))))

#define FONT_BITS (s[1] * 256 + s[0])

#ifdef __GNUC__
#  if #cpu (i386) /* unaligned / little endian */
#    undef FONT_BITS
#    define FONT_BITS *((const uint16_t *) s)
#  endif
#endif

#define DRAW_CHAR(canvas, bytes_per_pixel, bpl, pen,			\
 		  font, cpl, cw, ch,					\
		  glyph, bold, underline, size)				\
do {									\
	const unsigned int bpp = bytes_per_pixel;			\
	uint8_t *d;							\
	const uint8_t *s;						\
	unsigned int x;							\
	unsigned int y;							\
	unsigned int shift;						\
	unsigned int bold1;						\
	unsigned int under1;						\
	unsigned int ch1;						\
									\
	d = canvas;							\
									\
	assert ((cw) >= 8 && (cw) <= 16);				\
	assert ((ch) >= 1 && (ch) <= 31);				\
									\
	x = (glyph) * (cw);						\
	shift = x & 7;							\
									\
	s = font;							\
	s += (x >> 3);							\
									\
	ch1 = (ch);							\
									\
	bold1 = !!(bold);						\
	under1 = (underline);						\
									\
	switch (size) {							\
	case VBI_DOUBLE_HEIGHT2:					\
	case VBI_DOUBLE_SIZE2:						\
		s += (cpl) * (cw) / 8 * (ch) / 2;			\
		under1 >>= (ch) / 2;					\
									\
	case VBI_DOUBLE_HEIGHT:					\
	case VBI_DOUBLE_SIZE: 						\
		ch1 >>= 1;						\
									\
	default:							\
		break;							\
	}								\
									\
	for (y = 0; y < ch1; under1 >>= 1, ++y) {			\
		unsigned int bits = ~0;					\
									\
		if (!(under1 & 1)) {					\
			bits = FONT_BITS >> shift;			\
			bits |= bits << bold1;				\
		}							\
									\
		s += (cpl) * (cw) / 8;					\
									\
		switch (size) {						\
		case VBI_NORMAL_SIZE:					\
			for (x = 0; x < (cw); bits >>= 1, ++x)		\
				PIXEL (d, x, pen, bits & 1);		\
			d += bpl;					\
			break;						\
									\
		case VBI_DOUBLE_HEIGHT:				\
		case VBI_DOUBLE_HEIGHT2:				\
			for (x = 0; x < (cw); bits >>= 1, ++x) {	\
				PIXEL (d, x, pen, bits & 1);		\
				PIXEL (d + bpl, x, pen, bits & 1);	\
			}						\
			d += bpl * 2;					\
			break;						\
									\
		case VBI_DOUBLE_WIDTH:					\
			for (x = 0; x < (cw) * 2; bits >>= 1, x += 2) {	\
				PIXEL (d, x, pen, bits & 1);		\
				PIXEL (d, x + 1, pen, bits & 1);	\
			}						\
			d += bpl;					\
			break;						\
									\
		case VBI_DOUBLE_SIZE:					\
		case VBI_DOUBLE_SIZE2:					\
			for (x = 0; x < (cw) * 2; bits >>= 1, x += 2) {	\
				unsigned int col = bits & 1;		\
									\
				PIXEL (d, x, pen, col);			\
				PIXEL (d, x + 1, pen, col);		\
				PIXEL (d + bpl, x, pen, col);		\
				PIXEL (d + bpl, x + 1, pen, col);	\
			}						\
			d += bpl * 2;					\
			break;						\
									\
		default:						\
			break;						\
		}							\
	}								\
} while (0)

#define DRAW_DRCS(canvas, bytes_per_pixel, bpl, pen, font, color, size)	\
do {									\
	const unsigned int bpp = bytes_per_pixel;			\
	uint8_t *d;							\
	const uint8_t *s;						\
	const uint8_t *pen2;						\
	unsigned int col;						\
	unsigned int x;							\
	unsigned int y;							\
									\
	d = canvas;							\
	s = font;							\
									\
	pen2 = (const uint8_t *)(pen);					\
	pen2 += (color) * bpp; /* 2, 4, 16 entry palette */		\
									\
	switch (size) {							\
	case VBI_NORMAL_SIZE:						\
		for (y = 0; y < TCH; d += bpl, ++y)			\
			for (x = 0; x < 12; ++s, x += 2) {		\
				PIXEL (d, x, pen2, *s & 15);		\
				PIXEL (d, x + 1, pen2, *s >> 4);	\
			}						\
		break;							\
									\
	case VBI_DOUBLE_HEIGHT2:					\
		s += 30;						\
									\
	case VBI_DOUBLE_HEIGHT:					\
		for (y = 0; y < TCH / 2; d += bpl * 2, ++y)		\
			for (x = 0; x < 12; ++s, x += 2) {		\
				col = *s & 15;				\
				PIXEL (d, x, pen2, col);		\
				PIXEL (d + bpl, x, pen2, col);		\
				col = *s >> 4;				\
				PIXEL (d, x + 1, pen2, col);		\
				PIXEL (d + bpl, x + 1, pen2, col);	\
			}						\
		break;							\
									\
	case VBI_DOUBLE_WIDTH:						\
		for (y = 0; y < TCH; d += bpl, ++y)			\
			for (x = 0; x < 12 * 2; ++s, x += 4) {		\
				col = *s & 15;				\
				PIXEL (d, x + 0, pen2, col);		\
				PIXEL (d, x + 1, pen2, col);		\
				col = *s >> 4;				\
				PIXEL (d, x + 2, pen2, col);		\
				PIXEL (d, x + 3, pen2, col);		\
			}						\
		break;							\
									\
	case VBI_DOUBLE_SIZE2:						\
		s += 30;						\
									\
	case VBI_DOUBLE_SIZE:						\
		for (y = 0; y < TCH / 2; d += bpl * 2, ++y)		\
			for (x = 0; x < 12 * 2; ++s, x += 4) {		\
				col = *s & 15;				\
				PIXEL (d, x, pen2, col);		\
				PIXEL (d, x + 1, pen2, col);		\
				PIXEL (d + bpl, x, pen2, col);		\
				PIXEL (d + bpl, x + 1, pen2, col);	\
				col = *s >> 4;				\
				PIXEL (d, x + 2, pen2, col);		\
				PIXEL (d, x + 3, pen2, col);		\
				PIXEL (d + bpl, x + 2, pen2, col);	\
				PIXEL (d + bpl, x + 3, pen2, col);	\
			}						\
		break;							\
									\
	default:							\
		break;							\
	}								\
} while (0)

#define DRAW_BLANK(canvas, bytes_per_pixel, bpl, pen, cw, ch)		\
do {									\
	const unsigned int bpp = bytes_per_pixel;			\
	uint8_t *d;							\
	unsigned int x;							\
	unsigned int y;							\
									\
	d = canvas;							\
									\
	for (y = 0; y < (ch); d += (bpl), ++y)				\
		for (x = 0; x < (cw); ++x)				\
			PIXEL (d, x, pen, 0);				\
} while (0)

#define DRAW_CC_PAGE(bytes_per_pixel)					\
do {									\
	for (rowct = n_rows; rowct-- > 0; ++row) {			\
		const vbi_char *ac;					\
		unsigned int colct;					\
									\
		ac = pg->text + row * pg->columns + column;		\
									\
		for (colct = n_columns; colct-- > 0; ++ac) {		\
			const unsigned int bpp = bytes_per_pixel;	\
			unsigned int pen[2];				\
									\
			PIXEL (pen, 0, cm.bg[ac->opacity], ac->background); \
			PIXEL (pen, 1, cm.fg[ac->opacity], ac->foreground); \
									\
			DRAW_CHAR (canvas, bytes_per_pixel,		\
				   bytes_per_line, pen,			\
				   ccfont3_bits, CCPL, CCW, CCH,	\
				   unicode_ccfont3 (ac->unicode,	\
				    ac->attr & VBI_ITALIC),		\
				   /* bold */ 0,			\
				   (!!(ac->attr & VBI_UNDERLINE))	\
				   << 12 /* cell row */,		\
				   VBI_NORMAL_SIZE);			\
									\
			canvas += CCW * bytes_per_pixel;		\
		}							\
									\
		canvas += row_adv;					\
	}								\
} while (0)

#define DRAW_TTX_CHAR(bytes_per_line, bytes_per_pixel)			\
do {									\
	const unsigned int bpp = bytes_per_pixel;			\
	unsigned int unicode;						\
									\
	PIXEL (pen, 0, cm.bg[ac->opacity], ac->background); 		\
	PIXEL (pen, 1, cm.fg[ac->opacity], ac->foreground);		\
									\
	unicode = (ac->attr & option_space_attr) ?			\
		0x0020 : ac->unicode;					\
									\
	switch (ac->size) {						\
	case VBI_OVER_TOP:						\
	case VBI_OVER_BOTTOM:						\
		break;							\
									\
	default:							\
		if (vbi_is_drcs (unicode)) {				\
			const uint8_t *font;				\
									\
			font = vbi_ttx_page_get_drcs_data (pg, unicode); \
									\
			if (NULL != font)				\
				DRAW_DRCS (canvas, bytes_per_pixel,	\
					   bytes_per_line, pen, font,	\
					   ac->drcs_clut_offs,		\
					   ac->size);			\
			else /* shouldn't happen */			\
				DRAW_BLANK (canvas, bytes_per_pixel,	\
					    bytes_per_line, pen,	\
					    TCW, TCH);			\
		} else {						\
			DRAW_CHAR (canvas, bytes_per_pixel,		\
				   bytes_per_line, pen,			\
				   wstfont2_bits, TCPL, TCW, TCH,	\
				   unicode_wstfont2 (unicode,		\
				     ac->attr & VBI_ITALIC),		\
				   ac->attr & VBI_BOLD,		\
				   (!!(ac->attr & VBI_UNDERLINE)) << 9	\
					/* cell row 9 */,		\
				   ac->size);				\
		}							\
	}								\
} while (0)

#define DRAW_TTX_PAGE(bytes_per_pixel)					\
do {									\
	uint8_t pen [(2 + 8 + 32) * bytes_per_pixel];			\
	const unsigned int bpp = bytes_per_pixel;			\
	unsigned int rowct;						\
	unsigned int i;							\
									\
	if (pg->drcs_clut)						\
		for (i = 2; i < 2 + 8 + 32; i++)			\
			PIXEL (pen, i, cm.map, pg->drcs_clut[i]);	\
									\
	for (rowct = n_rows; rowct-- > 0; ++row) {			\
		const vbi_char *ac;					\
		unsigned int colct;					\
									\
		ac = pg->text + row * pg->columns + column;		\
									\
		for (colct = n_columns; colct-- > 0; ++ac) {		\
			DRAW_TTX_CHAR (bytes_per_line, bytes_per_pixel);	\
			canvas += TCW * bytes_per_pixel;		\
		}							\
									\
		canvas += row_adv;					\
	}								\
} while (0)

/**
 */
vbi_bool
vbi_ttx_page_draw_region_va	(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 unsigned int		x,
				 unsigned int		y,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		n_columns,
				 unsigned int		n_rows,
				 va_list		options)
{
	struct color_map cm;
	vbi_bool option_scale;
	vbi_bool option_alpha;
	unsigned int option_space_attr;
	int brightness;
	int contrast;
	uint8_t *canvas;
	unsigned int scaled_height;
	unsigned int bytes_per_pixel;
	unsigned long bytes_per_line;
	unsigned long size;
	unsigned int row_adv;

	_vbi_null_check (pg, FALSE);
	_vbi_null_check (buffer, FALSE);
	_vbi_null_check (format, FALSE);

	if (0) {
		unsigned int i, j;

		for (i = 0; i < pg->rows; ++i) {
			const vbi_char *ac;

			fprintf (stderr, "%2d: ", i);

			ac = pg->text + i * pg->columns;

			for (j = 0; j < pg->columns; ++j)
				fprintf (stderr, "%04x ", ac[j].unicode);

			fputs ("\n", stderr);
		}
	}

	option_scale = FALSE;
	option_alpha = FALSE;
	option_space_attr = 0;
	brightness = 128;
	contrast = 64;

	for (;;) {
		vbi_export_option option;

		option = va_arg (options, vbi_export_option);

		switch (option) {
		case VBI_TABLE:
		case VBI_RTL:
			(void) va_arg (options, vbi_bool);
			break;

		case VBI_REVEAL:
			COPY_SET_COND (option_space_attr, VBI_CONCEAL,
				       !va_arg (options, vbi_bool));
			break;

		case VBI_FLASH_ON:
			COPY_SET_COND (option_space_attr, VBI_FLASH,
				       !va_arg (options, vbi_bool));
			break;

		case VBI_SCALE:
			option_scale = va_arg (options, vbi_bool);
			break;

		case VBI_BRIGHTNESS:
			brightness = va_arg (options, int);
			break;

		case VBI_CONTRAST:
			contrast = va_arg (options, int);
			break;

		case VBI_ALPHA:
			option_alpha = va_arg (options, vbi_bool);
			break;

		default:
			option = 0;
			break;
		}

		if (0 == option)
			break;
	}

	if (x >= format->width
	    || y >= format->height) {
		warning (NULL, 
			 "Position x %u, y %u is beyond image size %u x %u.",
			 x, y, format->width, format->height);
		return FALSE;
	}

	if (column + n_columns > pg->columns
	    || row + n_rows > pg->rows) {
		warning (NULL, 
			 "Columns %u ... %u, rows %u ... %u beyond "
			 "page size of %u x %u characters.",
			 column, column + n_columns - 1,
			 row, row + n_rows - 1,
			 pg->columns, pg->rows);
		return FALSE;
	}

	scaled_height = option_scale ? 20 : 10;

	if (n_columns * 12 > format->width - x
	    || n_rows * scaled_height > format->height - x) {
		warning (NULL, 
			 "Image size %u x %u too small to draw %u x %u "
			 "characters (%u x %u pixels) at x %u, y %u.",
			 format->width, format->height,
			 n_columns, n_rows,
			 n_columns * 12, n_rows * scaled_height,
			 x, y);
		return FALSE;
	}

	bytes_per_pixel = _vbi_pixfmt_bytes_per_pixel (format->pixfmt);

	color_map_init (&cm, pg, format->pixfmt, bytes_per_pixel,
			brightness, contrast, option_alpha);

	bytes_per_line = format->bytes_per_line[0];

	if (bytes_per_line <= 0) {
		bytes_per_line = pg->columns * TCW * bytes_per_pixel;
	} else if ((format->width * bytes_per_pixel) > bytes_per_line) {
		warning (NULL, 
			 "Image width %u (%s) > bytes_per_line %lu.",
			 format->width, vbi_pixfmt_name (format->pixfmt),
			 bytes_per_line);
		return FALSE;
	}

	canvas = ((uint8_t *) buffer)
		+ format->offset[0]
		+ y * bytes_per_line
		+ x * bytes_per_pixel;

	size = format->offset[0] + bytes_per_line * format->height;

#if 0
	if (size > format->size) {
		warning (NULL, 
			 "Image %u x %u, offset %lu, bytes_per_line %lu "
			 "> buffer size %lu = 0x%08lx.",
			 format->width, format->height,
			 format->offset, bytes_per_line,
			 format->size, format->size);
		return FALSE;
	}
#endif

	if (option_scale)
		bytes_per_line *= 2;

	row_adv = bytes_per_line * TCH - bytes_per_pixel * n_columns * TCW;

	switch (bytes_per_pixel) {
	case 1:
		DRAW_TTX_PAGE (1);
		break;
	case 2:
		DRAW_TTX_PAGE (2);
		break;
	case 3:
		DRAW_TTX_PAGE (3);
		break;
	case 4:
		DRAW_TTX_PAGE (4);
		break;
	default:
		assert (0);
	}

	if (option_scale) {
		line_doubler (buffer, format, x, y,
			      n_columns * 12, n_rows * scaled_height);
	}

	return TRUE;
}

vbi_bool
vbi_ttx_page_draw_region	(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 unsigned int		x,
				 unsigned int		y,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		n_columns,
				 unsigned int		n_rows,
				 ...)
{
	vbi_bool success;
	va_list options;

	va_start (options, n_rows);
	success = vbi_ttx_page_draw_region_va (pg, buffer, format,
					       x, y,
					       column, row,
					       n_columns, n_rows,
					       options);
	va_end (options);

	return success;
}

/**
 */
vbi_bool
vbi_ttx_page_draw_va		(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 va_list		options)
{
	return vbi_ttx_page_draw_region_va (pg, buffer, format,
					    /* x */ 0,
					    /* y */ 0,
					    /* column */ 0,
					    /* row */ 0,
					    pg->columns,
					    pg->rows,
					    options);
}

vbi_bool
vbi_ttx_page_draw		(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 ...)
{
	vbi_bool success;
	va_list options;

	va_start (options, format);
	success = vbi_ttx_page_draw_region_va (pg, buffer, format,
					       /* x */ 0,
					       /* y */ 0,
					       /* column */ 0,
					       /* row */ 0,
					       pg->columns,
					       pg->rows,
					       options);
	va_end (options);

	return success;
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
