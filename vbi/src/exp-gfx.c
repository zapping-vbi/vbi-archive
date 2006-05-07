/*
 *  libzvbi - Closed Caption and Teletext rendering
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
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

/* $Id: exp-gfx.c,v 1.7.2.10 2006-05-07 06:04:58 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>		/* malloc() */
#include <string.h>		/* memcpy() */
#include "misc.h"
#include "page.h"		/* vbi3_page */
#include "lang.h"		/* vbi3_is_drcs() */
#ifdef ZAPPING8
#  include "common/intl-priv.h"
#else
#  include "intl-priv.h"
#endif
#include "export-priv.h"	/* vbi3_export */
#include "exp-gfx.h"
#include "vt.h"			/* VBI3_TRANSPARENT_BLACK */

#include "wstfont2.xbm"

/* Teletext character cell dimensions - hardcoded (DRCS) */

#define TCW 12
#define TCH 10

#define TCPL (wstfont2_width / TCW * wstfont2_height / TCH)

#include "ccfont3.xbm"

/* Closed Caption character cell dimensions */

#define CCW 16
#define CCH 13

#define CCPL (ccfont3_width / CCW * ccfont3_height / CCH)

/**
 * @internal
 *
 * unicode_wstfont2() subroutine.
 */
vbi3_inline unsigned int
unicode_wstfont2_special	(unsigned int		c,
				 vbi3_bool		italic)
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
				 vbi3_bool		italic)
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
 * @param c Unicode.
 * @param italic @c TRUE to switch to slanted character set.
 * 
 * Translates Unicode character to glyph number in ccfont3 image. 
 * 
 * @return
 * Glyph number.
 */
static unsigned int
unicode_ccfont3			(unsigned int		c,
				 vbi3_bool		italic)
{
	static const uint16_t specials[] = {
								0x00E1, 0x00E9,
		0x00ED, 0x00F3, 0x00FA, 0x00E7, 0x00F7, 0x00D1, 0x00F1, 0x25A0,
		0x00AE, 0x00B0, 0x00BD, 0x00BF, 0x2122, 0x00A2, 0x00A3, 0x266A,
		0x00E0, 0x0020, 0x00E8, 0x00E2, 0x00EA, 0x00EE, 0x00F4, 0x00FB
	};
	unsigned int i;

	if (c < 0x0020) {
		c = 15; /* invalid */
	} else if (c < 0x0080) {
		c = c;
	} else {
		for (i = 0; i < N_ELEMENTS (specials); ++i)
			if (c == specials[i]) {
				c = i + 6;
				break;
			}

		if (i >= N_ELEMENTS (specials))
			c = 15; /* invalid */
	}

	if (italic)
		c += 4 * 32;

	return c;
}

/**
 * @internal
 *
 * Copies even lines to odd lines.
 */
static void
line_doubler			(void *			buffer,
				 const vbi3_image_format *format,
				 unsigned int		x,
				 unsigned int		y,
				 unsigned int		width,
				 unsigned int		height)
{
	uint8_t *canvas;
	unsigned int byte_width;
	unsigned int bytes_per_line;
	unsigned int bytes_per_pixel;

	assert (VBI3_PIXFMT_IS_PACKED (format->pixfmt));
	assert (x + width <= format->width);
	assert (y + height <= format->height);
	assert (0 == (height % 2));

	bytes_per_pixel = vbi3_pixfmt_bytes_per_pixel (format->pixfmt);
	byte_width = width * bytes_per_pixel;

	bytes_per_line = format->bytes_per_line;

	if (bytes_per_line <= 0) {
		bytes_per_line = byte_width;
	} else {
		assert (byte_width <= bytes_per_line);
	}

	canvas = ((uint8_t *) buffer)
		+ format->offset
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
 *   @a n entries of vbi3_pixfmt_bytes_per_pixel(@a pixfmt) each.
 * @param pixfmt Pixel format to convert to.
 * @param color Array of vbi3_rgba types.
 * @param color_size Number of elements in @a color array.
 * @param brightness Change brightness: 0 dark ... 255 bright, 128
 *   for no change.
 * @param contrast Change contrast: -128 inverse ... 0 none ...
 *   127 maximum, 64 for no change.
 * @param alpha Replacement alpha value: 0 ... 255.
 *
 * Converts vbi3_rgba types to pixels of desired format.
 *
 * @returns
 * @c FALSE if parameters are invalid.
 *
 * @bug
 * YUV pixel formats are not supported.
 */
static vbi3_bool
vbi3_rgba_conv			(void *			buffer,
				 vbi3_pixfmt		pixfmt,
				 const vbi3_rgba *	color,
				 unsigned int		color_size,
				 int			brightness,
				 int			contrast,
				 int			alpha)
{
	uint8_t *d = buffer;

	switch (pixfmt) {
	case VBI3_PIXFMT_RGBA24_LE:
		RGBA_CONV4 (0xFF, 0xFF00, 0xFF0000, 0xFF000000, 0);
		break;
	case VBI3_PIXFMT_RGBA24_BE:
		RGBA_CONV4 (0xFF, 0xFF00, 0xFF0000, 0xFF000000, 3);
		break;
	case VBI3_PIXFMT_BGRA24_LE:
		RGBA_CONV4 (0xFF0000, 0xFF00, 0xFF, 0xFF000000, 0);
		break;
	case VBI3_PIXFMT_BGRA24_BE:
		RGBA_CONV4 (0xFF0000, 0xFF00, 0xFF, 0xFF000000, 3);
		break;

	case VBI3_PIXFMT_RGB24_LE:
		while (color_size-- > 0) {
			unsigned int value = *color++;

			d[0] = TRANS (value & 0xFF);
			d[1] = TRANS ((value >> 8) & 0xFF);
			d[2] = TRANS ((value >> 16) & 0xFF);
			d += 3;
		}
		break;

	case VBI3_PIXFMT_RGB24_BE:
		while (color_size-- > 0) {
			unsigned int value = *color++;

			d[0] = TRANS ((value >> 16) & 0xFF);
			d[1] = TRANS ((value >> 8) & 0xFF);
			d[2] = TRANS (value & 0xFF);
			d += 3;
		}
		break;

	case VBI3_PIXFMT_RGB16_LE:
		RGBA_CONV2 (0x001F, 0x07E0, 0xF800, 0, 0);
		break;
	case VBI3_PIXFMT_RGB16_BE:
		RGBA_CONV2 (0x001F, 0x07E0, 0xF800, 0, 1);
		break;
	case VBI3_PIXFMT_BGR16_LE:
		RGBA_CONV2 (0xF800, 0x07E0, 0x001F, 0, 0);
		break;
	case VBI3_PIXFMT_BGR16_BE:
		RGBA_CONV2 (0xF800, 0x07E0, 0x001F, 0, 1);
		break;

	case VBI3_PIXFMT_RGBA15_LE:
		RGBA_CONV2 (0x001F, 0x03E0, 0x7C00, 0x8000, 0);
		break;
	case VBI3_PIXFMT_RGBA15_BE:
		RGBA_CONV2 (0x001F, 0x03E0, 0x7C00, 0x8000, 1);
		break;
	case VBI3_PIXFMT_BGRA15_LE:
		RGBA_CONV2 (0x7C00, 0x03E0, 0x001F, 0x8000, 0);
		break;
	case VBI3_PIXFMT_BGRA15_BE:
		RGBA_CONV2 (0x7C00, 0x03E0, 0x001F, 0x8000, 1);
		break;
	case VBI3_PIXFMT_ARGB15_LE:
		RGBA_CONV2 (0x003E, 0x07C0, 0xF800, 0x0001, 0);
		break;
	case VBI3_PIXFMT_ARGB15_BE:
		RGBA_CONV2 (0x003E, 0x07C0, 0xF800, 0x0001, 1);
		break;
	case VBI3_PIXFMT_ABGR15_LE:
		RGBA_CONV2 (0xF800, 0x07C0, 0x003E, 0x0001, 0);
		break;
	case VBI3_PIXFMT_ABGR15_BE:
		RGBA_CONV2 (0xF800, 0x07C0, 0x003E, 0x0001, 1);
		break;

	case VBI3_PIXFMT_RGBA12_LE:
		RGBA_CONV2 (0x000F, 0x00F0, 0x0F00, 0xF000, 0);
		break;
	case VBI3_PIXFMT_RGBA12_BE:
		RGBA_CONV2 (0x000F, 0x00F0, 0x0F00, 0xF000, 1);
		break;
	case VBI3_PIXFMT_BGRA12_LE:
		RGBA_CONV2 (0x0F00, 0x00F0, 0x000F, 0xF000, 0);
		break;
	case VBI3_PIXFMT_BGRA12_BE:
		RGBA_CONV2 (0x0F00, 0x00F0, 0x000F, 0xF000, 1);
		break;
	case VBI3_PIXFMT_ARGB12_LE:
		RGBA_CONV2 (0x00F0, 0x0F00, 0xF000, 0x000F, 0);
		break;
	case VBI3_PIXFMT_ARGB12_BE:
		RGBA_CONV2 (0x00F0, 0x0F00, 0xF000, 0x000F, 1);
		break;
	case VBI3_PIXFMT_ABGR12_LE:
		RGBA_CONV2 (0xF000, 0x0F00, 0x000F, 0x000F, 0);
		break;
	case VBI3_PIXFMT_ABGR12_BE:
		RGBA_CONV2 (0xF000, 0x0F00, 0x000F, 0x000F, 1);
		break;

	case VBI3_PIXFMT_RGB8:
		RGBA_CONV1 (0x07, 0x38, 0xC0, 0);
		break;
	case VBI3_PIXFMT_BGR8:
		RGBA_CONV1 (0xE0, 0x1C, 0x03, 0);
		break;

	case VBI3_PIXFMT_RGBA7:
		RGBA_CONV1 (0x03, 0x1C, 0x60, 0x80);
		break;
	case VBI3_PIXFMT_BGRA7:
		RGBA_CONV1 (0x60, 0x1C, 0x03, 0x80);
		break;
	case VBI3_PIXFMT_ARGB7:
		RGBA_CONV1 (0x06, 0x38, 0xC0, 0x01);
		break;
	case VBI3_PIXFMT_ABGR7:
		RGBA_CONV1 (0xC0, 0x38, 0x06, 0x01);
		break;

	default:
/*
		debug ("Invalid pixfmt %u (%s)",
		       (unsigned int) pixfmt,
		       vbi3_pixfmt_name (pixfmt));
*/
		return FALSE;
	}

	return TRUE;
}

#define COLOR_MAP_ELEMENTS N_ELEMENTS (((vbi3_page *) 0)->color_map)

struct color_map {
	unsigned int		map [3 * COLOR_MAP_ELEMENTS];
	void *			fg [4];
	void *			bg [4];
};

static vbi3_bool
color_map_init			(struct color_map *	cm,
				 const vbi3_page *	pg,
				 vbi3_pixfmt		pixfmt,
				 unsigned int		bytes_per_pixel,
				 int			brightness,
				 int			contrast)
{
	cm->bg[VBI3_TRANSPARENT_SPACE] = (uint8_t *) cm->map
		+ COLOR_MAP_ELEMENTS * 2 * bytes_per_pixel;
	cm->bg[VBI3_TRANSLUCENT] = (uint8_t *) cm->map
		+ COLOR_MAP_ELEMENTS * 1 * bytes_per_pixel;

	if (!vbi3_rgba_conv (cm->bg[VBI3_TRANSPARENT_SPACE], pixfmt,
			     pg->color_map, COLOR_MAP_ELEMENTS,
			     brightness, contrast, /* alpha */ 0x00))
		return FALSE;

	if (!vbi3_rgba_conv (cm->bg[VBI3_TRANSLUCENT], pixfmt,
			     pg->color_map, COLOR_MAP_ELEMENTS,
			     brightness, contrast, /* alpha */ 0x7F))
		return FALSE;

	cm->bg[VBI3_TRANSPARENT_FULL] = cm->bg[VBI3_TRANSPARENT_SPACE];
	cm->bg[VBI3_OPAQUE] = cm->map;

	if (!vbi3_rgba_conv (cm->map, pixfmt,
			     pg->color_map, COLOR_MAP_ELEMENTS,
			     brightness, contrast, /* alpha */ 0xFF))
		return FALSE;

	cm->fg[VBI3_TRANSPARENT_SPACE] = cm->bg[VBI3_TRANSPARENT_SPACE];
	cm->fg[VBI3_TRANSPARENT_FULL] = cm->map;
	cm->fg[VBI3_TRANSLUCENT] = cm->map;
	cm->fg[VBI3_OPAQUE] = cm->map;

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
	case VBI3_DOUBLE_HEIGHT2:					\
	case VBI3_DOUBLE_SIZE2:						\
		s += (cpl) * (cw) / 8 * (ch) / 2;			\
		under1 >>= (ch) / 2;					\
									\
	case VBI3_DOUBLE_HEIGHT:					\
	case VBI3_DOUBLE_SIZE: 						\
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
		case VBI3_NORMAL_SIZE:					\
			for (x = 0; x < (cw); bits >>= 1, ++x)		\
				PIXEL (d, x, pen, bits & 1);		\
			d += bpl;					\
			break;						\
									\
		case VBI3_DOUBLE_HEIGHT:				\
		case VBI3_DOUBLE_HEIGHT2:				\
			for (x = 0; x < (cw); bits >>= 1, ++x) {	\
				PIXEL (d, x, pen, bits & 1);		\
				PIXEL (d + bpl, x, pen, bits & 1);	\
			}						\
			d += bpl * 2;					\
			break;						\
									\
		case VBI3_DOUBLE_WIDTH:					\
			for (x = 0; x < (cw) * 2; bits >>= 1, x += 2) {	\
				PIXEL (d, x, pen, bits & 1);		\
				PIXEL (d, x + 1, pen, bits & 1);	\
			}						\
			d += bpl;					\
			break;						\
									\
		case VBI3_DOUBLE_SIZE:					\
		case VBI3_DOUBLE_SIZE2:					\
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
	case VBI3_NORMAL_SIZE:						\
		for (y = 0; y < TCH; d += bpl, ++y)			\
			for (x = 0; x < 12; ++s, x += 2) {		\
				PIXEL (d, x, pen2, *s & 15);		\
				PIXEL (d, x + 1, pen2, *s >> 4);	\
			}						\
		break;							\
									\
	case VBI3_DOUBLE_HEIGHT2:					\
		s += 30;						\
									\
	case VBI3_DOUBLE_HEIGHT:					\
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
	case VBI3_DOUBLE_WIDTH:						\
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
	case VBI3_DOUBLE_SIZE2:						\
		s += 30;						\
									\
	case VBI3_DOUBLE_SIZE:						\
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
		const vbi3_char *ac;					\
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
				    ac->attr & VBI3_ITALIC),		\
				   /* bold */ 0,			\
				   (!!(ac->attr & VBI3_UNDERLINE))	\
				   << 12 /* cell row */,		\
				   VBI3_NORMAL_SIZE);			\
									\
			canvas += CCW * bytes_per_pixel;		\
		}							\
									\
		canvas += row_adv;					\
	}								\
} while (0)

/**
 * @param pg Source page.
 * @param buffer Image buffer.
 * @param format Pixel format and dimensions of the buffer.
 *   The buffer must be large enough for @a n_columns x @a height characters
 *   of 16 x 13 pixels each.
 * @param flags Optional set of the following flags:
 *   - @c VBI3_SCALE: Duplicate lines. In this case characters are 16 x 26
 *     pixels, suitable for frame (rather than field) overlay.
 * @param column First source column, 0 ... pg->columns - 1.
 * @param row First source row, 0 ... pg->rows - 1.
 * @param n_columns Number of columns to draw, 1 ... pg->columns.
 * @param n_rows Number of rows to draw, 1 ... pg->rows.
 * 
 * Draws a subsection of a Closed Caption vbi3_page.
 *
 * @returns
 * @c FALSE if parameters are invalid.
 *
 * @bug
 * YUV pixel formats are not supported.
 */
vbi3_bool
vbi3_page_draw_caption_region_va_list
				(const vbi3_page *	pg,
				 void *			buffer,
				 const vbi3_image_format *format,
				 unsigned int		x,
				 unsigned int		y,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		n_columns,
				 unsigned int		n_rows,
				 va_list		export_options)
{
	struct color_map cm;
	vbi3_bool option_scale;
	int brightness;
	int contrast;
	uint8_t *canvas;
	unsigned int scaled_height;
	unsigned int bytes_per_pixel;
	unsigned long bytes_per_line;
	unsigned long size;
	unsigned long row_adv;

	assert (NULL != pg);
	assert (NULL != buffer);
	assert (NULL != format);

	if (0) {
		unsigned int i, j;

		for (i = 0; i < pg->rows; ++i) {
			const vbi3_char *ac;

			fprintf (stderr, "%2d: ", i);

			ac = pg->text + i * pg->columns;

			for (j = 0; j < pg->columns; ++j)
				fprintf (stderr, "%d%d%d%02x ",
					 ac[j].opacity,
					 ac[j].foreground,
					 ac[j].background,
					 ac[j].unicode & 0xFF);

			fputs ("\n", stderr);
		}
	}

	option_scale = FALSE;
	brightness = 128;
	contrast = 64;

	for (;;) {
		vbi3_export_option option;

		option = va_arg (export_options, vbi3_export_option);

		switch (option) {
		case VBI3_TABLE:
		case VBI3_RTL:
		case VBI3_REVEAL:
		case VBI3_FLASH_ON:
			va_arg (export_options, vbi3_bool);
			break;

		case VBI3_SCALE:
			option_scale = va_arg (export_options, vbi3_bool);
			break;

		case VBI3_BRIGHTNESS:
			brightness = va_arg (export_options, int);
			break;

		case VBI3_CONTRAST:
			contrast = va_arg (export_options, int);
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
		debug ("Position x %u, y %u is beyond image size %u x %u",
		       x, y, format->width, format->height);
		return FALSE;
	}

	if (column + n_columns > pg->columns
	    || row + n_rows > pg->rows) {
		debug ("Columns %u ... %u, rows %u ... %u beyond "
		       "page size of %u x %u characters",
		       column, column + n_columns - 1,
		       row, row + n_rows - 1,
		       pg->columns, pg->rows);
		return FALSE;
	}

	scaled_height = option_scale ? 26 : 13;

	if (n_columns * 16 > format->width - x
	    || n_rows * scaled_height > format->height - y) {
		debug ("Image size %u x %u too small to draw %u x %u "
		       "characters (%u x %u pixels) at x %u, y %u",
		       format->width, format->height,
		       n_columns, n_rows,
		       n_columns * 16, n_rows * scaled_height,
		       x, y);
		return FALSE;
	}

	bytes_per_pixel = vbi3_pixfmt_bytes_per_pixel (format->pixfmt);

	color_map_init (&cm, pg, format->pixfmt, bytes_per_pixel,
			brightness, contrast);

	bytes_per_line = format->bytes_per_line;

	if (bytes_per_line <= 0) {
		bytes_per_line = pg->columns * CCW * bytes_per_pixel;
	} else if ((format->width * bytes_per_pixel) > bytes_per_line) {
		debug ("Image width %u (%s) > bytes_per_line %lu",
		       format->width, vbi3_pixfmt_name (format->pixfmt),
		       bytes_per_line);
		return FALSE;
	}

	canvas = ((uint8_t *) buffer)
		+ format->offset
		+ y * bytes_per_line
		+ x * bytes_per_pixel;

	size = format->offset + bytes_per_line * format->height;

	if (size > format->size) {
		debug ("Image %u x %u, offset %lu, bytes_per_line %lu " 
		       "> buffer size %lu = 0x%08lx",
		       format->width, format->height,
		       format->offset, bytes_per_line,
		       format->size, format->size);
		return FALSE;
	}

	if (option_scale)
		bytes_per_line *= 2;

	row_adv = bytes_per_line * CCH - bytes_per_pixel * n_columns * CCW;

	switch (bytes_per_pixel) {
		unsigned int rowct;

	case 1:
		DRAW_CC_PAGE (1);
		break;
	case 2:
		DRAW_CC_PAGE (2);
		break;
	case 3:
		DRAW_CC_PAGE (3);
		break;
	case 4:
		DRAW_CC_PAGE (4);
		break;
	default:
		assert (0);
	}

	if (option_scale)
  		line_doubler (buffer, format, x, y,
			      n_columns * 16, n_rows * scaled_height);

	return TRUE;
}

vbi3_bool
vbi3_page_draw_caption_region	(const vbi3_page *	pg,
				 void *			buffer,
				 const vbi3_image_format *format,
				 unsigned int		x,
				 unsigned int		y,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		n_columns,
				 unsigned int		n_rows,
				 ...)
{
	vbi3_bool r;
	va_list export_options;

	va_start (export_options, n_rows);

	r = vbi3_page_draw_caption_region_va_list
		(pg, buffer, format, x, y, column, row, n_columns, n_rows,
		 export_options);

	va_end (export_options);

	return r;
}

/**
 * @param pg Source page.
 * @param buffer Image buffer.
 * @param format Pixel format and dimensions of the buffer. The buffer must be
 *   large enough for pg->columns * pg->rows characters of 16 x 13 pixels
 *   each.
 * @param flags Optional set of the following flags:
 *   - @c VBI3_SCALE: Duplicate lines. In this case characters are 16 x 26
 *     pixels, suitable for frame (rather than field) overlay.
 * @param column First source column, 0 ... pg->columns - 1.
 * @param row First source row, 0 ... pg->rows - 1.
 * @param n_columns Number of columns to draw, 1 ... pg->columns.
 * @param n_rows Number of rows to draw, 1 ... pg->rows.
 * 
 * Draws a subsection of a Closed Caption vbi3_page.
 *
 * @returns
 * @c FALSE if parameters are invalid.
 *
 * @bug
 * YUV pixel formats are not supported.
 */
vbi3_bool
vbi3_page_draw_caption_va_list	(const vbi3_page *	pg,
				 void *			buffer,
				 const vbi3_image_format *format,
				 va_list		export_options)
{
	return vbi3_page_draw_caption_region_va_list
		(pg, buffer, format,
		 /* x */ 0, /* y */ 0,
		 /* column */ 0, /* row */ 0,
		 pg->columns, pg->rows,
		 export_options);
}

vbi3_bool
vbi3_page_draw_caption		(const vbi3_page *	pg,
				 void *			buffer,
				 const vbi3_image_format *format,
				 ...)
{
	vbi3_bool r;
	va_list export_options;

	va_start (export_options, format);

	r = vbi3_page_draw_caption_region_va_list
		(pg, buffer, format,
		 /* x */ 0, /* y */ 0,
		 /* column */ 0, /* row */ 0,
		 pg->columns, pg->rows,
		 export_options);

	va_end (export_options);

	return r;
}

#define DRAW_VT_CHAR(bytes_per_line, bytes_per_pixel)			\
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
	case VBI3_OVER_TOP:						\
	case VBI3_OVER_BOTTOM:						\
		break;							\
									\
	default:							\
		if (vbi3_is_drcs (unicode)) {				\
			const uint8_t *font;				\
									\
			font = vbi3_page_get_drcs_data (pg, unicode);	\
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
				     ac->attr & VBI3_ITALIC),		\
				   ac->attr & VBI3_BOLD,		\
				   (!!(ac->attr & VBI3_UNDERLINE)) << 9	\
					/* cell row 9 */,		\
				   ac->size);				\
		}							\
	}								\
} while (0)

#define DRAW_VT_PAGE(bytes_per_pixel)					\
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
		const vbi3_char *ac;					\
		unsigned int colct;					\
									\
		ac = pg->text + row * pg->columns + column;		\
									\
		for (colct = n_columns; colct-- > 0; ++ac) {		\
			DRAW_VT_CHAR (bytes_per_line, bytes_per_pixel);	\
			canvas += TCW * bytes_per_pixel;		\
		}							\
									\
		canvas += row_adv;					\
	}								\
} while (0)

/**
 * @param pg Source page.
 * @param buffer Image buffer.
 * @param format Pixel format and dimensions of the buffer. The buffer must be
 *   large enough for @a n_columns x @a n_rows characters of 12 x 10 pixels
 *   each.
 * @param flags Optional set of the following flags:
 *   - @c VBI3_REVEAL: Draw characters flagged 'conceal' (see vbi3_char).
 *   - @c VBI3_FLASH_ON: Draw characters flagged 'flash' (see vbi3_char)
 *     in on state, otherwise like a space (U+0020).
 *   - @c VBI3_SCALE: Duplicate lines. In this case characters are 12 x 20
 *     pixels, suitable for frame (rather than field) overlay.
 * @param column First source column, 0 ... pg->columns - 1.
 * @param row First source row, 0 ... pg->rows - 1.
 * @param n_columns Number of columns to draw, 1 ... pg->columns.
 * @param n_rows Number of rows to draw, 1 ... pg->rows.
 * 
 * Draws a subsection of a Teletext vbi3_page.
 *
 * @returns
 * @c FALSE if parameters are invalid.
 *
 * @bug
 * YUV pixel formats are not supported.
 */
vbi3_bool
vbi3_page_draw_teletext_region_va_list
				(const vbi3_page *	pg,
				 void *			buffer,
				 const vbi3_image_format *format,
				 unsigned int		x,
				 unsigned int		y,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		n_columns,
				 unsigned int		n_rows,
				 va_list		export_options)
{
	struct color_map cm;
	vbi3_bool option_scale;
	unsigned int option_space_attr;
	int brightness;
	int contrast;
	uint8_t *canvas;
	unsigned int scaled_height;
	unsigned int bytes_per_pixel;
	unsigned long bytes_per_line;
	unsigned long size;
	unsigned int row_adv;

	assert (NULL != pg);
	assert (NULL != buffer);
	assert (NULL != format);

	if (0) {
		unsigned int i, j;

		for (i = 0; i < pg->rows; ++i) {
			const vbi3_char *ac;

			fprintf (stderr, "%2d: ", i);

			ac = pg->text + i * pg->columns;

			for (j = 0; j < pg->columns; ++j)
				fprintf (stderr, "%04x ", ac[j].unicode);

			fputs ("\n", stderr);
		}
	}

	option_scale = FALSE;
	option_space_attr = 0;
	brightness = 128;
	contrast = 64;

	for (;;) {
		vbi3_export_option option;

		option = va_arg (export_options, vbi3_export_option);

		switch (option) {
		case VBI3_TABLE:
		case VBI3_RTL:
			va_arg (export_options, vbi3_bool);
			break;

		case VBI3_REVEAL:
			COPY_SET_COND (option_space_attr, VBI3_CONCEAL,
				       !va_arg (export_options, vbi3_bool));
			break;

		case VBI3_FLASH_ON:
			COPY_SET_COND (option_space_attr, VBI3_FLASH,
				       !va_arg (export_options, vbi3_bool));
			break;

		case VBI3_SCALE:
			option_scale = va_arg (export_options, vbi3_bool);
			break;

		case VBI3_BRIGHTNESS:
			brightness = va_arg (export_options, int);
			break;

		case VBI3_CONTRAST:
			contrast = va_arg (export_options, int);
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
		debug ("Position x %u, y %u is beyond image size %u x %u",
		       x, y, format->width, format->height);
		return FALSE;
	}

	if (column + n_columns > pg->columns
	    || row + n_rows > pg->rows) {
		debug ("Columns %u ... %u, rows %u ... %u beyond "
		       "page size of %u x %u characters",
		       column, column + n_columns - 1,
		       row, row + n_rows - 1,
		       pg->columns, pg->rows);
		return FALSE;
	}

	scaled_height = option_scale ? 20 : 10;

	if (n_columns * 12 > format->width - x
	    || n_rows * scaled_height > format->height - x) {
		debug ("Image size %u x %u too small to draw %u x %u "
		       "characters (%u x %u pixels) at x %u, y %u",
		       format->width, format->height,
		       n_columns, n_rows,
		       n_columns * 12, n_rows * scaled_height,
		       x, y);
		return FALSE;
	}

	bytes_per_pixel = vbi3_pixfmt_bytes_per_pixel (format->pixfmt);

	color_map_init (&cm, pg, format->pixfmt, bytes_per_pixel,
			brightness, contrast);

	bytes_per_line = format->bytes_per_line;

	if (bytes_per_line <= 0) {
		bytes_per_line = pg->columns * TCW * bytes_per_pixel;
	} else if ((format->width * bytes_per_pixel) > bytes_per_line) {
		debug ("Image width %u (%s) > bytes_per_line %lu",
		       format->width, vbi3_pixfmt_name (format->pixfmt),
		       bytes_per_line);
		return FALSE;
	}

	canvas = ((uint8_t *) buffer)
		+ format->offset
		+ y * bytes_per_line
		+ x * bytes_per_pixel;

	size = format->offset + bytes_per_line * format->height;

	if (size > format->size) {
		debug ("Image %u x %u, offset %lu, bytes_per_line %lu "
		       "> buffer size %lu = 0x%08lx",
		       format->width, format->height,
		       format->offset, bytes_per_line,
		       format->size, format->size);
		return FALSE;
	}

	if (option_scale)
		bytes_per_line *= 2;

	row_adv = bytes_per_line * TCH - bytes_per_pixel * n_columns * TCW;

	switch (bytes_per_pixel) {
	case 1:
		DRAW_VT_PAGE (1);
		break;
	case 2:
		DRAW_VT_PAGE (2);
		break;
	case 3:
		DRAW_VT_PAGE (3);
		break;
	case 4:
		DRAW_VT_PAGE (4);
		break;
	default:
		assert (0);
	}

	if (option_scale)
		line_doubler (buffer, format, x, y,
			      n_columns * 12, n_rows * scaled_height);

	return TRUE;
}

vbi3_bool
vbi3_page_draw_teletext_region	(const vbi3_page *	pg,
				 void *			buffer,
				 const vbi3_image_format *format,
				 unsigned int		x,
				 unsigned int		y,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		n_columns,
				 unsigned int		n_rows,
				 ...)
{
	vbi3_bool r;
	va_list export_options;

	va_start (export_options, n_rows);

	r = vbi3_page_draw_teletext_region_va_list
		(pg, buffer, format, x, y,
		 column, row, n_columns, n_rows,
		 export_options);

	va_end (export_options);

	return r;
}

/**
 * @param pg Source page.
 * @param buffer Image buffer.
 * @param format Pixel format and dimensions of the buffer. The buffer must be
 *   large enough for pg->columns * pg->rows characters of 12 x 10 pixels
 *   each.
 * @param flags Optional set of the following flags:
 *   - @c VBI3_REVEAL: Draw characters flagged 'conceal' (see vbi3_char).
 *   - @c VBI3_FLASH_ON: Draw characters flagged 'flash' (see vbi3_char)
 *     in off state, i. e. like a space (U+0020).
 *   - @c VBI3_SCALE: Duplicate lines. In this case characters are 12 x 20
 *     pixels, suitable for frame (rather than field) overlay.
 * 
 * Draws a Teletext vbi3_page.
 *
 * @returns
 * @c FALSE if parameters are invalid.
 *
 * @bug
 * YUV pixel formats are not supported.
 */
vbi3_bool
vbi3_page_draw_teletext_va_list	(const vbi3_page *	pg,
				 void *			buffer,
				 const vbi3_image_format *format,
				 va_list		export_options)
{
	return vbi3_page_draw_teletext_region_va_list
		(pg, buffer, format,
		 /* x */ 0, /* y */ 0,
		 /* column */ 0, /* row */ 0,
		 pg->columns, pg->rows,
		 export_options);
}

vbi3_bool
vbi3_page_draw_teletext		(const vbi3_page *	pg,
				 void *			buffer,
				 const vbi3_image_format *format,
				 ...)
{
	vbi3_bool r;
	va_list export_options;

	va_start (export_options, format);

	r = vbi3_page_draw_teletext_region_va_list
		(pg, buffer, format,
		 /* x */ 0, /* y */ 0,
		 /* column */ 0, /* row */ 0,
		 pg->columns, pg->rows,
		 export_options);

	va_end (export_options);

	return r;
}

/*
 *  Shared export options
 */

typedef struct {
	vbi3_export		export;

	/* Options */

	/* The raw image contains the same information a real TV
	   would show, however a TV overlays the image on both fields.
	   So raw pixel aspect is 2:1, and this option will double
	   lines adding redundant information. The resulting images
	   with pixel aspect 2:2 are still too narrow compared to a
	   real TV closer to 4:3 (11 MHz TXT pixel clock), but I
	   think one should export raw, not scaled data (which is
	   still possible in Zapping using the screenshot plugin). */
	vbi3_bool		double_height;
} gfx_instance;

static vbi3_export *
gfx_new				(const _vbi3_export_module *em)
{
	gfx_instance *gfx;

	em = em;

	if (!(gfx = vbi3_malloc (sizeof (*gfx))))
		return NULL;

	CLEAR (*gfx);

	return &gfx->export;
}

static void
gfx_delete			(vbi3_export *		e)
{
	vbi3_free (PARENT (e, gfx_instance, export));
}

static const vbi3_option_info
option_info [] = {
	_VBI3_OPTION_BOOL_INITIALIZER
	("aspect", N_("Correct aspect ratio"),
	 TRUE, N_("Approach an image aspect ratio similar to "
		  "a real TV. This will double the image size."))
};

static vbi3_bool
option_get			(vbi3_export *		e,
				 const char *		keyword,
				 vbi3_option_value *	value)
{
	gfx_instance *gfx = PARENT (e, gfx_instance, export);

	if (0 == strcmp (keyword, "aspect")) {
		value->num = gfx->double_height;
	} else {
		_vbi3_export_unknown_option (e, keyword);
		return FALSE;
	}

	return TRUE;
}

static vbi3_bool
option_set			(vbi3_export *		e,
				 const char *		keyword,
				 va_list		ap)
{
	gfx_instance *gfx = PARENT (e, gfx_instance, export);

	if (0 == strcmp (keyword, "aspect")) {
		gfx->double_height = !!va_arg (ap, int);
	} else {
		_vbi3_export_unknown_option (e, keyword);
		return FALSE;
	}

	return TRUE;
}

/*
 *  PPM - Portable Pixmap File (raw)
 */

static vbi3_bool
export_ppm			(vbi3_export *		e,
				 const vbi3_page *	pg)
{
	gfx_instance *gfx = PARENT (e, gfx_instance, export);
	vbi3_image_format format;
	unsigned int cw, ch;
	uint8_t *image;
	unsigned int row;

	if (pg->columns < 40) { /* caption */
		cw = CCW;
		ch = CCH;
	} else {
		cw = TCW;
		ch = TCH;
	}

	format.width		= cw * pg->columns;
	format.height		= ch;
	format.size		= format.width * format.height * 3;
	format.offset		= 0;
	format.bytes_per_line	= format.width * 3;
	format.pixfmt		= VBI3_PIXFMT_RGB24_LE;

	if (!(image = vbi3_malloc (format.size))) {
		_vbi3_export_malloc_error (e);
		return FALSE;
	}

	fprintf (e->fp, "P6 %u %u 255\n",
		 format.width, (ch * pg->rows) << gfx->double_height);

	if (ferror (e->fp))
		goto write_error;

	for (row = 0; row < pg->rows; ++row) {
		vbi3_bool success;

		if (pg->columns < 40)
			success = vbi3_page_draw_caption_region
				(pg, image, &format,
				 /* x */ 0, /* y */ 0,
				 /* column */ 0, /* row */ row,
				 pg->columns, /* rows */ 1,
				 /* options */ VBI3_END);
		else
			success = vbi3_page_draw_teletext_region
				(pg, image, &format,
				 /* x */ 0, /* y */ 0,
				 /* column */ 0, /* row */ row,
				 pg->columns, /* rows */ 1,
				 /* options: */
				 VBI3_REVEAL, e->reveal,
				 VBI3_END);

		assert (success);

		if (gfx->double_height) {
			uint8_t *body;
			unsigned int line;

			body = image;

			for (line = 0; line < ch; ++line) {
				if (format.width !=
				    fwrite (body, 3, format.width, e->fp))
					goto write_error;

				if (format.width !=
				    fwrite (body, 3, format.width, e->fp))
					goto write_error;

				body += format.width * 3;
			}
		} else {
			if (format.size != fwrite (image, 1,
						   format.size, e->fp))
				goto write_error;
		}
	}

	vbi3_free (image);

	return TRUE;

 write_error:
	_vbi3_export_write_error (e);

	vbi3_free (image);

	return FALSE;
}

static const vbi3_export_info
export_info_ppm = {
	.keyword		= "ppm",
	.label			= N_("PPM"),
	.tooltip		= N_("Export this page as raw PPM image"),

	.mime_type		= "image/x-portable-pixmap",
	.extension		= "ppm",
};

const _vbi3_export_module
_vbi3_export_module_ppm = {
	.export_info		= &export_info_ppm,

	._new			= gfx_new,
	._delete		= gfx_delete,

	.option_info		= option_info,
	.option_info_size	= N_ELEMENTS (option_info),

	.option_get		= option_get,
	.option_set		= option_set,

	.export			= export_ppm
};

/*
 *  PNG - Portable Network Graphics File
 */

#ifdef HAVE_LIBPNG

#include "png.h"
#include "setjmp.h"

#define COLOR_MAP_SIZE (N_ELEMENTS (pg->color_map))
#define TRANSLUCENT COLOR_MAP_SIZE
#define TRANSPARENT (COLOR_MAP_SIZE * 2)
#define PALETTE_SIZE (TRANSPARENT + 1)

static void
png_draw_char			(uint8_t *		canvas,
				 unsigned int		bytes_per_line,
				 const vbi3_page *       pg,
				 const vbi3_char *	ac,
				 unsigned int		conceal,
				 uint8_t *		pen,
				 vbi3_bool		is_ttx)
{
	unsigned int unicode;

	unicode = ((ac->attr & VBI3_CONCEAL) & conceal) ?
		0x0020 : ac->unicode;

	switch (ac->opacity) {
	case VBI3_TRANSPARENT_SPACE:
		/* Transparent foreground and background. */

	blank:
		pen[0] = TRANSPARENT;

		if (is_ttx)
			DRAW_BLANK (canvas, 1, bytes_per_line, pen, TCW, TCH);
		else
			DRAW_BLANK (canvas, 1, bytes_per_line, pen, CCW, CCH);

		return;

	case VBI3_TRANSPARENT_FULL:
		/* Transparent background, opaque foreground.
		   The background of Teletext multicolor DRCS is
		   ambiguous, so we make them opaque. */

		pen[0] = TRANSPARENT;
		pen[1] = ac->foreground;

		break;

	case VBI3_TRANSLUCENT:
		/* Translucent background (for 'boxed' text), opaque
		   foreground. The background of Teletext multicolor DRCS
		   is ambiguous, so we make them completely translucent. */

		if (vbi3_is_drcs (unicode))
			pen += 64; /* use translucent DRCS palette */

		pen[0] = ac->background + TRANSLUCENT;
		pen[1] = ac->foreground;

		break;

	case VBI3_OPAQUE:
		pen[0] = ac->background;
		pen[1] = ac->foreground;

		break;
	}

	if (vbi3_is_drcs (unicode)) {
		const uint8_t *font;

		font = vbi3_page_get_drcs_data (pg, unicode);

		if (font && is_ttx) {
			DRAW_DRCS (canvas, 1, bytes_per_line, pen, font,
				   ac->drcs_clut_offs, ac->size);
		} else {
			/* shouldn't happen */
			goto blank;
		}
	} else {
		if (is_ttx) {
			DRAW_CHAR (canvas, 1, bytes_per_line,
				   pen, wstfont2_bits, TCPL, TCW, TCH,
				   unicode_wstfont2 (unicode,
						     ac->attr & VBI3_ITALIC),
				   ac->attr & VBI3_BOLD,
				   (!!(ac->attr & VBI3_UNDERLINE)) << 9
				   	/* cell row 9 */,
				   ac->size);
		} else {
			DRAW_CHAR (canvas, 1, bytes_per_line,
				   pen, ccfont3_bits, CCPL, CCW, CCH,
				   unicode_ccfont3 (unicode,
						    ac->attr & VBI3_ITALIC),
				   /* bold */ 0,
				   (!!(ac->attr & VBI3_UNDERLINE)) << 12
				   	/* cell row 12 */,
				   VBI3_NORMAL_SIZE);
		}
	}
}

static vbi3_bool
write_png			(vbi3_export *		e,
				 const vbi3_page *	pg,
				 png_structp		png_ptr,
				 png_infop		info_ptr,
				 png_bytep		image,
				 png_bytep *		row_pointer,
				 const vbi3_image_format *format,
				 vbi3_bool		double_height)
{
	png_color palette[PALETTE_SIZE];
	png_byte alpha[PALETTE_SIZE];
	png_text text[4];
	char title[80];
	unsigned int i;
	unsigned int size;

	if (setjmp (png_ptr->jmpbuf))
		return FALSE;

	png_init_io (png_ptr, e->fp);

	png_set_IHDR (png_ptr, info_ptr,
		      format->width,
		      format->height << double_height,
		      /* bit_depth */ 8,
		      PNG_COLOR_TYPE_PALETTE,
		      double_height ? PNG_INTERLACE_ADAM7 : PNG_INTERLACE_NONE,
		      PNG_COMPRESSION_TYPE_DEFAULT,
		      PNG_FILTER_TYPE_DEFAULT);

	/* Could be optimized (or does libpng?) */
	for (i = 0; i < COLOR_MAP_SIZE; ++i) {
		/* Opaque. */
		palette[i].red   = VBI3_R (pg->color_map[i]);
		palette[i].green = VBI3_G (pg->color_map[i]);
		palette[i].blue	 = VBI3_B (pg->color_map[i]);
		alpha[i]	 = 255;

		/* Translucent. */
		palette[i + TRANSLUCENT] = palette[i];
		alpha[i + TRANSLUCENT] = 128;
	}

	/* Transparent (black) background. */
	palette[TRANSPARENT].red   = 0;
	palette[TRANSPARENT].green = 0;
	palette[TRANSPARENT].blue  = 0;
	alpha[TRANSPARENT] = 0;

	png_set_PLTE (png_ptr, info_ptr, palette, 80);
	png_set_tRNS (png_ptr, info_ptr, alpha, 80, NULL);

	png_set_gAMA (png_ptr, info_ptr, 1.0 / 2.2);

	size = 0;

	if (e->network)
		size = snprintf (title, sizeof (title),
				 "%s ", e->network);
	else
		title[0] = 0;

	/* FIXME
	   ISO 8859-1 (Latin-1) character set required,
	   see png spec for other */
	if (pg->pgno < 0x100) {
		size += snprintf (title + size, sizeof (title) - size,
				  "Closed Caption"); /* proper name */
	} else if (VBI3_ANY_SUBNO != pg->subno) {
		size += snprintf (title + size, sizeof (title) - size,
				  _("Teletext Page %3x.%x"),
				  pg->pgno, pg->subno);
	} else {
		size += snprintf (title + size, sizeof (title) - size,
				  _("Teletext Page %3x"), pg->pgno);
	}

	CLEAR (text);

	text[0].key = strdup ("Title");
	assert (NULL != text[0].key);
	text[0].text = title;
	text[0].compression = PNG_TEXT_COMPRESSION_NONE;

	text[1].key = strdup ("Software");
	assert (NULL != text[1].key);
	text[1].text = e->creator;
	text[1].compression = PNG_TEXT_COMPRESSION_NONE;

	png_set_text (png_ptr, info_ptr, text, 2);

	free (text[1].key);
	text[1].key = NULL;
	free (text[0].key);
	text[0].key = NULL;

	png_write_info (png_ptr, info_ptr);

	if (double_height) {
		for (i = 0; i < format->height; ++i) {
			uint8_t *s;

			s = image + format->bytes_per_line * i;
			row_pointer[i * 2 + 0] = s;
			row_pointer[i * 2 + 1] = s;
		}
	} else {
		for (i = 0; i < format->height; ++i)
			row_pointer[i] = image + format->bytes_per_line * i;
	}

	png_write_image (png_ptr, row_pointer);

	png_write_end (png_ptr, info_ptr);

	return TRUE;
}

static vbi3_bool
export_png			(vbi3_export *		e,
				 const vbi3_page *	pg)
{
	gfx_instance *gfx = PARENT (e, gfx_instance, export);
	vbi3_image_format format;
	unsigned int cw, ch;
	png_bytep image;
	png_bytep *row_pointer;
	unsigned int row_adv;
	png_byte pen[128];
	png_bytep canvas;
	unsigned int row;
	png_structp png_ptr;
	png_infop info_ptr;

	if (pg->columns < 40) { /* caption */
		cw = CCW;
		ch = CCH;
	} else {
		cw = TCW;
		ch = TCH;
	}

	format.width		= cw * pg->columns;
	format.height		= ch * pg->rows;
	format.size		= format.width * format.height;
	format.bytes_per_line	= format.width;
	format.pixfmt		= VBI3_PIXFMT_RGB8;

	row_pointer = vbi3_malloc (sizeof (*row_pointer) * format.height * 2);

	if (NULL == row_pointer) {
		_vbi3_export_malloc_error (e);
		return FALSE;
	}

	image = vbi3_malloc (format.size);

	if (NULL == image) {
		_vbi3_export_malloc_error (e);
		vbi3_free (row_pointer);
		return FALSE;
	}

	row_adv = pg->columns * cw * (ch - 1);

	if (pg->drcs_clut) {
		unsigned int i;

		for (i = 2; i < 2 + 8 + 32; ++i) {
			pen[i]      = pg->drcs_clut[i]; /* opaque */
			pen[i + 64] = pg->drcs_clut[i] + TRANSLUCENT;
		}
	}

	canvas = image;

	for (row = 0; row < pg->rows; ++row) {
		unsigned int column;

		for (column = 0; column < pg->columns; ++column) {
			const vbi3_char *ac;
	
			ac = pg->text + row * pg->columns + column;

			switch (ac->size) {
			case VBI3_OVER_TOP:
			case VBI3_OVER_BOTTOM:
				continue;

			default:
				break;
			}

			if (0)
				fprintf(stderr, "%2u %2u %04x %u\n",
					row, column, ac->unicode, ac->size);

			png_draw_char (canvas,
				       format.bytes_per_line,
				       pg,
				       ac,
				       (unsigned int) !e->reveal,
				       pen,
				       /* is_ttx */ pg->columns >= 40);

			switch (ac->size) {
			case VBI3_DOUBLE_WIDTH:
			case VBI3_DOUBLE_SIZE:
			case VBI3_DOUBLE_SIZE2:
				canvas += cw * 2;
				break;

			default:
				canvas += cw;
				break;
			}
		}

		canvas += row_adv;
	}

	/* Now save the image */

	png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING,
					   NULL, NULL, NULL);
	if (NULL == png_ptr)
		goto unknown_error;

	info_ptr = png_create_info_struct (png_ptr);

	if (NULL == info_ptr) {
		png_destroy_write_struct (&png_ptr, (png_infopp) NULL);
		goto unknown_error;
	}

	if (!write_png (e, pg, png_ptr, info_ptr,
			image, row_pointer, &format,
			gfx->double_height)) {
		png_destroy_write_struct (&png_ptr, &info_ptr);
		goto write_error;
	}

	png_destroy_write_struct (&png_ptr, &info_ptr);

	vbi3_free (row_pointer);
	vbi3_free (image);

	return TRUE;

 write_error:
	_vbi3_export_write_error (e);

 unknown_error:
	if (row_pointer)
		vbi3_free (row_pointer);

	if (image)
		vbi3_free (image);

	return FALSE;
}

static const vbi3_export_info
export_info_png = {
	.keyword		= "png",
	.label			= N_("PNG"),
	.tooltip		= N_("Export this page as PNG image"),

	.mime_type		= "image/png",
	.extension		= "png",
};

const _vbi3_export_module
_vbi3_export_module_png = {
	.export_info		= &export_info_png,

	._new			= gfx_new,
	._delete		= gfx_delete,

	.option_info		= option_info,
	.option_info_size	= N_ELEMENTS (option_info),

	.option_get		= option_get,
	.option_set		= option_set,

	.export			= export_png
};

#endif /* HAVE_LIBPNG */
