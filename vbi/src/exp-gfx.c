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

/* $Id: exp-gfx.c,v 1.7.2.2 2004-02-13 02:15:27 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "lang.h"
#include "export.h"
#include "exp-gfx.h"
#include "vt.h" /* VBI_TRANSPARENT_BLACK */

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
static __inline__ unsigned int
unicode_wstfont2_special	(unsigned int		c,
				 vbi_bool		italic)
{
	static const uint16_t specials [] = {
		0x01B5, 0x2016, 0x01CD, 0x01CE, 0x0229, 0x0251, 0x02DD, 0x02C6,
		0x02C7, 0x02C9, 0x02CA, 0x02CB, 0x02CD, 0x02CF, 0x02D8, 0x02D9,
		0x02DA, 0x02DB, 0x02DC, 0x2014, 0x2018, 0x2019, 0x201C,	0x201D,
		0x20A0, 0x2030, 0x20AA, 0x2122, 0x2126, 0x215B, 0x215C, 0x215D,
		0x215E, 0x2190, 0x2191, 0x2192, 0x2193, 0x25A0, 0x266A, 0xE800,
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
				 vbi_bool		italic)
{
	static const uint16_t specials[] = {
								0x00E1, 0x00E9,
		0x00ED, 0x00F3, 0x00FA, 0x00E7, 0x00F7, 0x00D1, 0x00F1, 0x25A0,
		0x00AE, 0x00B0, 0x00BD, 0x00BF, 0x2122, 0x00A2, 0x00A3, 0x266A,
		0x00E0, 0x0020, 0x00E8, 0x00E2, 0x00EA, 0x00EE, 0x00F4, 0x00FB
	};
	unsigned int i;

	if (c < 0x0020)
		c = 15; /* invalid */
	else if (c < 0x0080)
		c = c;
	else {
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
				 const vbi_image_format *format)
{
	uint8_t *canvas;
	unsigned int byte_width;
	unsigned int bytes_per_line;
	unsigned int height;

	assert (VBI_PIXFMT_IS_PACKED (format->pixfmt));

	canvas = ((uint8_t *) buffer) + format->offset;

	byte_width = format->width
		* vbi_pixfmt_bytes_per_pixel (format->pixfmt);
	bytes_per_line = format->bytes_per_line;

	if (bytes_per_line <= 0)
		bytes_per_line = byte_width;

	for (height = format->height; height > 0; height -= 2) {
		memcpy (canvas + bytes_per_line, canvas, byte_width);
		canvas += bytes_per_line * 2;
	}
}

/* Find first set bit in constant, e.g. 0x123 -> 9 with boundary
   cases 0xFFFF FFFF -> 32, 0x1 -> 1, 0 -> 1 */
#define FFS2(m) ((m) & 0x2 ? 2 : 1)
#define FFS4(m) ((m) & 0xC ? 2 + FFS2 ((m) >> 2) : FFS2 (m))
#define FFS8(m) ((m) & 0xF0 ? 4 + FFS4 ((m) >> 4) : FFS4 (m))
#define FFS16(m) ((m) & 0xFF00 ? 8 + FFS8 ((m) >> 8) : FFS8 (m))
#define FFS32(m) ((m) & 0xFFFF0000 ? 16 + FFS16 ((m) >> 16) : FFS16 (m))

/* 32 bit constant byte reverse, e.g. 0xAABBCCDD -> 0xDDCCBBAA */
#define SWAB32(m)							\
	(+ (((m) & 0xFF000000) >> 24)					\
	 + (((m) & 0xFF0000) >> 8)					\
	 + (((m) & 0xFF00) << 16)					\
	 + (((m) & 0xFF) << 8))

#define RGBA_CONVC(v, n, m)						\
	((FFS32 (m) > n) ?						\
	 (v << (FFS32 (m) - n)) & m :					\
	 (v >> (n - FFS32 (m))) & m)

/* Converts 0xAABBGGRR value v to another value, shifting and masking
   color bits to positions given by r, g, b, a mask */  
#define RGBA_CONV(v, r, g, b, a)					\
	(+ RGBA_CONVC (v, 8, r)						\
	 + RGBA_CONVC (v, 16, g)					\
	 + RGBA_CONVC (v, 24, b)					\
	 + RGBA_CONVC (v, 32, a))

/* Like RGBA_CONV for reversed endian */
#define RGBA_CONV_SWAB32(v, r, g, b, a)					\
	(+ RGBA_CONVC (v, 8, SWAB32 (r))				\
	 + RGBA_CONVC (v, 16, SWAB32 (g))				\
	 + RGBA_CONVC (v, 24, SWAB32 (b))				\
	 + RGBA_CONVC (v, 32, SWAB32 (a)))

#if __BYTE_ORDER == __LITTLE_ENDIAN

#define RGBA_CONV4(r, g, b, a, endian)					\
	while (n-- > 0) {						\
		unsigned int value = *color++;				\
									\
		if (0 == endian)					\
			value = RGBA_CONV (value, r, g, b, a);		\
		else							\
			value = RGBA_CONV_SWAB32 (value, r, g, b, a);	\
		*((uint32_t *) d)++ = value;				\
	}

#define RGBA_CONV2(r, g, b, a, endian)					\
	while (n-- > 0) {						\
		unsigned int value = *color++;				\
									\
		value = RGBA_CONV (value, r, g, b, a);			\
		if (0 == endian) {					\
			*((uint16_t *) d)++ = value;			\
		} else {						\
			d[0] = value >> 8;				\
			d[1] = value;					\
			d += 2;						\
		}							\
	}

#elif __BYTE_ORDER == __BIG_ENDIAN

#define RGBA_CONV4(r, g, b, a, endian)					\
	while (n-- > 0) {						\
		unsigned int value = *color++;				\
									\
		if (0 == endian)					\
			value = RGBA_CONV_SWAB32 (value, r, g, b, a);	\
		else							\
			value = RGBA_CONV (value, r, g, b, a);		\
		*((uint32_t *) d)++ = value;				\
	}

#define RGBA_CONV2(r, g, b, a, endian)					\
	while (n-- > 0) {						\
		unsigned int value = *color++;				\
									\
		value = RGBA_CONV (value, r, g, b, a);			\
		if (1 == endian) {					\
			*((uint16_t *) d)++ = value;			\
		} else {						\
			d[0] = value;					\
			d[1] = value >> 8;				\
			d += 2;						\
		}							\
	}

#else
#error unknown endianess
#endif

#define RGBA_CONV1(r, g, b, a)						\
	while (n-- > 0) {						\
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
 * @param color_size Size of @a color array, number of entries.
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
				 unsigned int		n)
{
	uint8_t *d = buffer;

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
		while (n-- > 0) {
			unsigned int value = *color++;

			d[0] = value;
			d[1] = value >> 8;
			d[2] = value >> 16;
			d += 3;
		}
		break;
	case VBI_PIXFMT_RGB24_BE:
		while (n-- > 0) {
			unsigned int value = *color++;

			d[0] = value >> 16;
			d[1] = value >> 8;
			d[2] = value;
			d += 3;
		}
		break;

	case VBI_PIXFMT_RGB16_LE:
		RGBA_CONV2 (0x001F, 0x07E0, 0xF800, 0, 0);
		break;
	case VBI_PIXFMT_RGB16_BE:
		RGBA_CONV2 (0x001F, 0x07E0, 0xF800, 0, 1);
		break;
	case VBI_PIXFMT_BGR16_LE:
		RGBA_CONV2 (0xF800, 0x07E0, 0x001F, 0, 0);
		break;
	case VBI_PIXFMT_BGR16_BE:
		RGBA_CONV2 (0xF800, 0x07E0, 0x001F, 0, 1);
		break;

	case VBI_PIXFMT_RGBA15_LE:
		RGBA_CONV2 (0x001F, 0x03E0, 0x7C00, 0x8000, 0);
		break;
	case VBI_PIXFMT_RGBA15_BE:
		RGBA_CONV2 (0x001F, 0x03E0, 0x7C00, 0x8000, 1);
		break;
	case VBI_PIXFMT_BGRA15_LE:
		RGBA_CONV2 (0x7C00, 0x03E0, 0x001F, 0x8000, 0);
		break;
	case VBI_PIXFMT_BGRA15_BE:
		RGBA_CONV2 (0x7C00, 0x03E0, 0x001F, 0x8000, 1);
		break;
	case VBI_PIXFMT_ARGB15_LE:
		RGBA_CONV2 (0x003E, 0x07C0, 0xF800, 0x0001, 0);
		break;
	case VBI_PIXFMT_ARGB15_BE:
		RGBA_CONV2 (0x003E, 0x07C0, 0xF800, 0x0001, 1);
		break;
	case VBI_PIXFMT_ABGR15_LE:
		RGBA_CONV2 (0xF800, 0x07C0, 0x003E, 0x0001, 0);
		break;
	case VBI_PIXFMT_ABGR15_BE:
		RGBA_CONV2 (0xF800, 0x07C0, 0x003E, 0x0001, 1);
		break;

	case VBI_PIXFMT_RGBA12_LE:
		RGBA_CONV2 (0x000F, 0x00F0, 0x0F00, 0xF000, 0);
		break;
	case VBI_PIXFMT_RGBA12_BE:
		RGBA_CONV2 (0x000F, 0x00F0, 0x0F00, 0xF000, 1);
		break;
	case VBI_PIXFMT_BGRA12_LE:
		RGBA_CONV2 (0x0F00, 0x00F0, 0x000F, 0xF000, 0);
		break;
	case VBI_PIXFMT_BGRA12_BE:
		RGBA_CONV2 (0x0F00, 0x00F0, 0x000F, 0xF000, 1);
		break;
	case VBI_PIXFMT_ARGB12_LE:
		RGBA_CONV2 (0x00F0, 0x0F00, 0xF000, 0x000F, 0);
		break;
	case VBI_PIXFMT_ARGB12_BE:
		RGBA_CONV2 (0x00F0, 0x0F00, 0xF000, 0x000F, 1);
		break;
	case VBI_PIXFMT_ABGR12_LE:
		RGBA_CONV2 (0xF000, 0x0F00, 0x000F, 0x000F, 0);
		break;
	case VBI_PIXFMT_ABGR12_BE:
		RGBA_CONV2 (0xF000, 0x0F00, 0x000F, 0x000F, 1);
		break;

	case VBI_PIXFMT_RGB8:
		RGBA_CONV1 (0x07, 0x38, 0xC0, 0);
		break;
	case VBI_PIXFMT_BGR8:
		RGBA_CONV1 (0xE0, 0x1C, 0x03, 0);
		break;

	case VBI_PIXFMT_RGBA7:
		RGBA_CONV1 (0x03, 0x1C, 0x60, 0x80);
		break;
	case VBI_PIXFMT_BGRA7:
		RGBA_CONV1 (0x60, 0x1C, 0x03, 0x80);
		break;
	case VBI_PIXFMT_ARGB7:
		RGBA_CONV1 (0x06, 0x38, 0xC0, 0x01);
		break;
	case VBI_PIXFMT_ABGR7:
		RGBA_CONV1 (0xC0, 0x38, 0x06, 0x01);
		break;

	default:
		vbi_log_printf (__FUNCTION__,
				"Invalid pixfmt %u (%s)",
				(unsigned int) pixfmt,
				vbi_pixfmt_name (pixfmt));
		return FALSE;
	}

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

#if #cpu (i386) /* unaligned / little endian */
#define FONT_BITS *((const uint16_t *) s)
#else
#define FONT_BITS (s[1] * 256 + s[0])
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
	case VBI_DOUBLE_HEIGHT:						\
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
		case VBI_DOUBLE_HEIGHT:					\
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
			d += bpl * 2;					\
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
	case VBI_DOUBLE_HEIGHT:						\
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

#define DRAW_CC_CHAR(bytes_per_pixel)					\
do {									\
	const unsigned int bpp = bytes_per_pixel;			\
	unsigned int pen[2];						\
									\
	PIXEL (pen, 0, color_map, ac->background);			\
	PIXEL (pen, 1, color_map, ac->foreground);			\
									\
	DRAW_CHAR (canvas, bytes_per_pixel, bytes_per_line,		\
		   pen,							\
		   ccfont3_bits, CCPL, CCW, CCH,			\
		   unicode_ccfont3 (ac->unicode, ac->italic),		\
		   /* bold */ 0,					\
		   ac->underline << 12 /* cell row 12 */,		\
		   VBI_NORMAL_SIZE);					\
} while (0)

/**
 * @param pg Source page.
 * @param buffer Image buffer.
 * @param format Pixel format and dimensions of the buffer.
 *   The buffer must be large enough for @a width x @a height characters
 *   of 16 x 13 pixels each.
 * @param flags Optional set of the following flags:
 *   - @c VBI_SCALE: Duplicate lines. In this case characters are 16 x 26
 *     pixels, suitable for frame (rather than field) overlay.
 * @param column First source column, 0 ... pg->columns - 1.
 * @param row First source row, 0 ... pg->rows - 1.
 * @param width Number of columns to draw, 1 ... pg->columns.
 * @param height Number of rows to draw, 1 ... pg->rows.
 * 
 * Draws a subsection of a Closed Caption vbi_page.
 *
 * @returns
 * @c FALSE if parameters are invalid.
 *
 * @bug
 * YUV pixel formats are not supported.
 */
vbi_bool
vbi_draw_cc_page_region		(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 vbi_export_flags	flags,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		width,
				 unsigned int		height)
{
	unsigned int color_map [N_ELEMENTS (pg->color_map)];
	uint8_t *canvas;
	unsigned int scaled_height;
	unsigned int bytes_per_pixel;
	unsigned int bytes_per_line;
	unsigned int size;
	unsigned int row_adv;

	if (0) {
		unsigned int i, j;

		for (i = 0; i < pg->rows; ++i) {
			const vbi_char *ac;

			fprintf (stderr, "%2d: ", i);

			ac = pg->text + i * pg->columns;

			for (j = 0; j < pg->columns; ++j)
				fprintf (stderr, "%d%d%02x ",
					 ac[j].foreground,
					 ac[j].background,
					 ac[j].unicode & 0xFF);

			fputs ("\n", stderr);
		}
	}

	scaled_height = (flags & VBI_SCALE) ? 26 : 13;

	if (width * 16 > format->width
	    || height * scaled_height > format->height) {
		vbi_log_printf (__FUNCTION__,
				"Image size %u x %u too small for "
				"%u x %u characters",
				format->width, format->height,
				width, height);
		return FALSE;
	}

	if (!vbi_rgba_conv (color_map, format->pixfmt,
			    pg->color_map, N_ELEMENTS (pg->color_map)))
		return FALSE;

	canvas = ((uint8_t *) buffer) + format->offset;

	bytes_per_pixel = vbi_pixfmt_bytes_per_pixel (format->pixfmt);
	bytes_per_line = format->bytes_per_line;

	if (bytes_per_line <= 0) {
		bytes_per_line = pg->columns * CCW * bytes_per_pixel;
	} else if ((format->width * bytes_per_pixel) > bytes_per_line) {
		vbi_log_printf (__FUNCTION__,
				"Image width %u (%s) > bytes_per_line %u",
				format->width,
				vbi_pixfmt_name (format->pixfmt),
				bytes_per_line);
		return FALSE;
	}

	size = format->offset + bytes_per_line * format->height;

	if (size > format->size) {
		vbi_log_printf (__FUNCTION__,
				"Image %u x %u, offset %u, bytes_per_line %u "
				"> buffer size %u = 0x%08x",
				format->width, format->height,
				format->offset, bytes_per_line,
				format->size, format->size);
		return FALSE;
	}

	if (flags & VBI_SCALE)
		bytes_per_line *= 2;

	row_adv = bytes_per_line * CCH - bytes_per_pixel * width * CCW;

	switch (bytes_per_pixel) {
	case 4:
		for (; height-- > 0; ++row) {
			const vbi_char *ac;
			unsigned int count;

			ac = pg->text + row * pg->columns + column;

			for (count = width; count-- > 0; ++ac) {
				DRAW_CC_CHAR (4);
				canvas += CCW * 4;
			}

			canvas += row_adv;
		}

		break;

	case 3:
		for (; height-- > 0; ++row) {
			const vbi_char *ac;
			unsigned int count;

			ac = pg->text + row * pg->columns + column;

			for (count = width; count-- > 0; ++ac) {
				DRAW_CC_CHAR (3);
				canvas += CCW * 3;
			}

			canvas += row_adv;
		}

		break;

	case 2:
		for (; height-- > 0; ++row) {
			const vbi_char *ac;
			unsigned int count;

			ac = pg->text + row * pg->columns + column;

			for (count = width; count-- > 0; ++ac) {
				DRAW_CC_CHAR (2);
				canvas += CCW * 2;
			}

			canvas += row_adv;
		}

		break;

	case 1:
		for (; height-- > 0; ++row) {
			const vbi_char *ac;
			unsigned int count;

			ac = pg->text + row * pg->columns + column;

			for (count = width; count-- > 0; ++ac) {
				DRAW_CC_CHAR (1);
				canvas += CCW;
			}

			canvas += row_adv;
		}

		break;

	default:
		assert (0);
	}

	if (flags & VBI_SCALE)
		line_doubler (buffer, format);

	return TRUE;
}

/**
 * @param pg Source page.
 * @param buffer Image buffer.
 * @param format Pixel format and dimensions of the buffer. The buffer must be
 *   large enough for pg->columns * pg->rows characters of 16 x 13 pixels
 *   each.
 * @param flags Optional set of the following flags:
 *   - @c VBI_SCALE: Duplicate lines. In this case characters are 16 x 26
 *     pixels, suitable for frame (rather than field) overlay.
 * @param column First source column, 0 ... pg->columns - 1.
 * @param row First source row, 0 ... pg->rows - 1.
 * @param width Number of columns to draw, 1 ... pg->columns.
 * @param height Number of rows to draw, 1 ... pg->rows.
 * 
 * Draws a subsection of a Closed Caption vbi_page.
 *
 * @returns
 * @c FALSE if parameters are invalid.
 *
 * @bug
 * YUV pixel formats are not supported.
 */
vbi_bool
vbi_draw_cc_page		(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 vbi_export_flags	flags)
{
	return vbi_draw_cc_page_region (pg, buffer, format, flags,
					0, 0, pg->columns, pg->rows);
}

#define DRAW_VT_CHAR(bytes_per_pixel)					\
do {									\
	const unsigned int bpp = bytes_per_pixel;			\
	unsigned int unicode;						\
									\
	PIXEL (pen, 0, color_map, ac->background);			\
	PIXEL (pen, 1, color_map, ac->foreground);			\
									\
	unicode = ((ac->conceal & conceal) || (ac->flash & off)) ?	\
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
			font = vbi_page_drcs_data (pg, unicode);	\
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
						     ac->italic),	\
				   ac->bold,				\
				   ac->underline << 9 /* cell row 9 */,	\
				   ac->size);				\
		}							\
	}								\
} while (0)

/**
 * @param pg Source page.
 * @param buffer Image buffer.
 * @param format Pixel format and dimensions of the buffer. The buffer must be
 *   large enough for @a width x @a height characters of 12 x 10 pixels
 *   each.
 * @param flags Optional set of the following flags:
 *   - @c VBI_REVEAL: Draw characters flagged 'conceal' (see vbi_char).
 *   - @c VBI_FLASH_OFF: Draw characters flagged 'flash' (see vbi_char)
 *     in off state, i. e. like a space (U+0020).
 *   - @c VBI_SCALE: Duplicate lines. In this case characters are 12 x 20
 *     pixels, suitable for frame (rather than field) overlay.
 * @param column First source column, 0 ... pg->columns - 1.
 * @param row First source row, 0 ... pg->rows - 1.
 * @param width Number of columns to draw, 1 ... pg->columns.
 * @param height Number of rows to draw, 1 ... pg->rows.
 * 
 * Draws a subsection of a Teletext vbi_page.
 *
 * @returns
 * @c FALSE if parameters are invalid.
 *
 * @bug
 * YUV pixel formats are not supported.
 */
vbi_bool
vbi_draw_vt_page_region		(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 vbi_export_flags	flags,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		width,
				 unsigned int		height)
{
	unsigned int color_map [N_ELEMENTS (pg->color_map)];
	uint8_t *canvas;
	unsigned int scaled_height;
	unsigned int bytes_per_pixel;
	unsigned int bytes_per_line;
	unsigned int size;
	unsigned int row_adv;
	unsigned int conceal;
	unsigned int off;

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

	scaled_height = (flags & VBI_SCALE) ? 20 : 10;

	if (width * 12 > format->width
	    || height * scaled_height > format->height) {
		vbi_log_printf (__FUNCTION__,
				"Image size %u x %u too small for "
				"%u x %u characters",
				format->width, format->height,
				width, height);
		return FALSE;
	}

	if (!vbi_rgba_conv (color_map, format->pixfmt,
			    pg->color_map, N_ELEMENTS (color_map)))
		return FALSE;

	canvas = ((uint8_t *) buffer) + format->offset;

	bytes_per_pixel = vbi_pixfmt_bytes_per_pixel (format->pixfmt);
	bytes_per_line = format->bytes_per_line;

	if (bytes_per_line <= 0) {
		bytes_per_line = pg->columns * TCW * bytes_per_pixel;
	} else if ((format->width * bytes_per_pixel) > bytes_per_line) {
		vbi_log_printf (__FUNCTION__,
				"Image width %u (%s) > bytes_per_line %u",
				format->width,
				vbi_pixfmt_name (format->pixfmt),
				bytes_per_line);
		return FALSE;
	}

	size = format->offset + bytes_per_line * format->height;

	if (size > format->size) {
		vbi_log_printf (__FUNCTION__,
				"Image %u x %u, offset %u, bytes_per_line %u "
				"> buffer size %u = 0x%08x",
				format->width, format->height,
				format->offset, bytes_per_line,
				format->size, format->size);
		return FALSE;
	}

	if (flags & VBI_SCALE)
		bytes_per_line *= 2;

	row_adv = bytes_per_line * TCH - bytes_per_pixel * width * TCW;

	conceal = !(flags & VBI_REVEAL);
	off = !!(flags & VBI_FLASH_OFF);

	switch (bytes_per_pixel) {
	case 4:
	{
		uint32_t pen [2 + 8 + 32];
		unsigned int i;

		if (pg->drcs_clut)
			for (i = 2; i < 2 + 8 + 32; i++)
				pen[i] = ((uint32_t *) color_map)
					[pg->drcs_clut[i]];

		for (; height-- > 0; ++row) {
			const vbi_char *ac;
			unsigned int count;

			ac = pg->text + row * pg->columns + column;

			for (count = width; count-- > 0; ++ac) {
				DRAW_VT_CHAR (4);
				canvas += TCW * 3;
			}

			canvas += row_adv;
		}

		break;
	}

	case 3:
	{
		const unsigned int bpp = 3;
		uint8_t pen [(2 + 8 + 32) * 3];
		unsigned int i;

		if (pg->drcs_clut)
			for (i = 2; i < 2 + 8 + 32; i++)
				PIXEL (pen, i, color_map, pg->drcs_clut[i]);

		for (; height-- > 0; ++row) {
			const vbi_char *ac;
			unsigned int count;

			ac = pg->text + row * pg->columns + column;

			for (count = width; count-- > 0; ++ac) {
				DRAW_VT_CHAR (3);
				canvas += TCW * 3;
			}

			canvas += row_adv;
		}

		break;
	}

	case 2:
	{
		uint16_t pen [2 + 8 + 32];
		unsigned int i;

		if (pg->drcs_clut)
			for (i = 2; i < 2 + 8 + 32; i++)
				pen[i] = ((uint16_t *) color_map)
					[pg->drcs_clut[i]];

		for (; height-- > 0; ++row) {
			const vbi_char *ac;
			unsigned int count;

			ac = pg->text + row * pg->columns + column;

			for (count = width; count-- > 0; ++ac) {
				DRAW_VT_CHAR (2);
				canvas += TCW * 2;
			}

			canvas += row_adv;
		}

		break;
	}

	case 1:
	{
		uint8_t pen [2 + 8 + 32];
		unsigned int i;

		if (pg->drcs_clut)
			for (i = 2; i < 2 + 8 + 32; i++)
				pen[i] = ((uint8_t *) color_map)
					[pg->drcs_clut[i]];

		for (; height-- > 0; ++row) {
			const vbi_char *ac;
			unsigned int count;

			ac = pg->text + row * pg->columns + column;

			for (count = width; count-- > 0; ++ac) {
				DRAW_VT_CHAR (1);
				canvas += TCW;
			}

			canvas += row_adv;
		}

		break;
	}

	default:
		assert (0);
	}

	if (flags & VBI_SCALE)
		line_doubler (buffer, format);

	return TRUE;
}

/**
 * @param pg Source page.
 * @param buffer Image buffer.
 * @param format Pixel format and dimensions of the buffer. The buffer must be
 *   large enough for pg->columns * pg->rows characters of 12 x 10 pixels
 *   each.
 * @param flags Optional set of the following flags:
 *   - @c VBI_REVEAL: Draw characters flagged 'conceal' (see vbi_char).
 *   - @c VBI_FLASH_OFF: Draw characters flagged 'flash' (see vbi_char)
 *     in off state, i. e. like a space (U+0020).
 *   - @c VBI_SCALE: Duplicate lines. In this case characters are 12 x 20
 *     pixels, suitable for frame (rather than field) overlay.
 * 
 * Draws a Teletext vbi_page.
 *
 * @returns
 * @c FALSE if parameters are invalid.
 *
 * @bug
 * YUV pixel formats are not supported.
 */
vbi_bool
vbi_draw_vt_page		(const vbi_page *	pg,
				 void *			buffer,
				 const vbi_image_format *format,
				 vbi_export_flags	flags)
{
	return vbi_draw_vt_page_region (pg, buffer, format, flags,
					0, 0, pg->columns, pg->rows);
}

/*
 *  Shared export options
 */

typedef struct gfx_instance
{
	vbi_export		export;

	/* Options */
	unsigned		double_height : 1;
	/*
	 *  The raw image contains the same information a real TV
	 *  would show, however a TV overlays the image on both fields.
	 *  So raw pixel aspect is 2:1, and this option will double
	 *  lines adding redundant information. The resulting images
	 *  with pixel aspect 2:2 are still too narrow compared to a
	 *  real TV closer to 4:3 (11 MHz TXT pixel clock), but I
	 *  think one should export raw, not scaled data (which is
	 *  still possible in Zapping using the screenshot plugin).
	 */
} gfx_instance;

static vbi_export *
gfx_new(void)
{
	gfx_instance *gfx;

	if (!(gfx = calloc(1, sizeof(*gfx))))
		return NULL;

	return &gfx->export;
}

static void
gfx_delete(vbi_export *e)
{
	free(PARENT(e, gfx_instance, export));
}


static vbi_option_info
gfx_options[] = {
	VBI_OPTION_BOOL_INITIALIZER
	  ("aspect", N_("Correct aspect ratio"),
	   TRUE, N_("Approach an image aspect ratio similar to "
		    "a real TV. This will double the image size."))
};

#define elements(array) (sizeof(array) / sizeof(array[0]))

static vbi_option_info *
option_enum(vbi_export *e, int index)
{
	if (index < 0 || index >= (int) elements(gfx_options))
		return NULL;
	else
		return gfx_options + index;
}

static vbi_bool
option_get(vbi_export *e, const char *keyword, vbi_option_value *value)
{
	gfx_instance *gfx = PARENT(e, gfx_instance, export);

	if (strcmp(keyword, "aspect") == 0) {
		value->num = gfx->double_height;
	} else {
		vbi_export_unknown_option(e, keyword);
		return FALSE;
	}

	return TRUE;
}

static vbi_bool
option_set(vbi_export *e, const char *keyword, va_list args)
{
	gfx_instance *gfx = PARENT(e, gfx_instance, export);

	if (strcmp(keyword, "aspect") == 0) {
		gfx->double_height = !!va_arg(args, int);
	} else {
		vbi_export_unknown_option(e, keyword);
		return FALSE;
	}

	return TRUE;
}

/*
 *  PPM - Portable Pixmap File (raw)
 */

static vbi_bool
ppm_export			(vbi_export *		e,
				 FILE *			fp,
				 vbi_page *		pg)
{
	gfx_instance *gfx = PARENT (e, gfx_instance, export);
	vbi_image_format format;
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
	format.pixfmt		= VBI_PIXFMT_RGB24_LE;

	if (!(image = malloc (format.size))) {
		vbi_export_error_printf
			(e, _("Unable to allocate %d KB image buffer."),
			 format.size / 1024);
		return FALSE;
	}

	fprintf (fp, "P6 %u %u 255\n",
		 format.width, (ch * pg->rows) << gfx->double_height);

	if (ferror (fp))
		goto write_error;

	for (row = 0; row < pg->rows; ++row) {
		vbi_bool success;

		if (pg->columns < 40)
			success = vbi_draw_cc_page_region
				(pg, image, &format, 0, 0, row,
				 pg->columns, /* rows */ 1);
		else
			success = vbi_draw_vt_page_region
				(pg, image, &format,
				 (e->reveal ? VBI_REVEAL : 0),
				 0, row, pg->columns, /* rows */ 1);

		assert (success);

		if (gfx->double_height) {
			uint8_t *body;
			unsigned int line;

			body = image;

			for (line = 0; line < ch; ++line) {
				if (format.width !=
				    fwrite (body, 3, format.width, fp))
					goto write_error;

				if (format.width !=
				    fwrite (body, 3, format.width, fp))
					goto write_error;

				body += format.width * 3;
			}
		} else {
			if (format.size != fwrite (image, 1, format.size, fp))
				goto write_error;
		}
	}

	free (image);

	return TRUE;

 write_error:

	vbi_export_write_error (e);

	free (image);

	return FALSE;
}

static vbi_export_info
info_ppm = {
	.keyword	= "ppm",
	.label		= N_("PPM"),
	.tooltip	= N_("Export this page as raw PPM image"),

	.mime_type	= "image/x-portable-pixmap",
	.extension	= "ppm",
};

vbi_export_class
vbi_export_class_ppm = {
	._public		= &info_ppm,
	._new			= gfx_new,
	._delete		= gfx_delete,
	.option_enum		= option_enum,
	.option_get		= option_get,
	.option_set		= option_set,
	.export			= ppm_export
};

VBI_AUTOREG_EXPORT_MODULE(vbi_export_class_ppm)

/*
 *  PNG - Portable Network Graphics File
 */

#ifdef HAVE_LIBPNG

#include "png.h"
#include "setjmp.h"

static void
png_draw_char			(uint8_t *		canvas,
				 unsigned int		bytes_per_line,
				 const vbi_page *       pg,
				 const vbi_char *	ac,
				 unsigned int		conceal,
				 uint8_t *		pen,
				 vbi_bool		is_ttx)
{
	unsigned int unicode;

	unicode = (ac->conceal & conceal) ?
		0x0020 : ac->unicode;

	switch (ac->opacity) {
	case VBI_TRANSPARENT_SPACE:
		/* Transparent foreground and background. */

	blank:
		pen[0] = VBI_TRANSPARENT_BLACK;

		if (is_ttx)
			DRAW_BLANK (canvas, 1, bytes_per_line, pen, TCW, TCH);
		else
			DRAW_BLANK (canvas, 1, bytes_per_line, pen, CCW, CCH);
		return;

	case VBI_TRANSPARENT_FULL:
		/* Transparent background, opaque foreground. Currently not
		   used. Mind Teletext level 2.5 foreground and background
		   transparency by referencing colormap entry 8,
		   VBI_TRANSPARENT_BLACK. The background of multicolor DRCS is
		   ambiguous, so we make them opaque. */

		pen[0] = VBI_TRANSPARENT_BLACK;
		pen[1] = ac->foreground;

		break;

	case VBI_SEMI_TRANSPARENT:
		/* Translucent background (for 'boxed' text), opaque
		   foreground. The background of multicolor DRCS is ambiguous,
		   so we make them completely translucent. */

		if (vbi_is_drcs (unicode))
			pen += 64; /* use translucent DRCS palette */

		pen[0] = ac->background + 40; /* translucent */
		pen[1] = ac->foreground;

		break;

	case VBI_OPAQUE:
		pen[0] = ac->background;
		pen[1] = ac->foreground;

		break;
	}

	if (vbi_is_drcs (unicode)) {
		const uint8_t *font;

		font = vbi_page_drcs_data (pg, unicode);

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
				   unicode_wstfont2 (unicode, ac->italic),
				   ac->bold,
				   ac->underline << 9 /* cell row 9 */,
				   ac->size);
		} else {
			DRAW_CHAR (canvas, 1, bytes_per_line,
				   pen, ccfont3_bits, CCPL, CCW, CCH,
				   unicode_ccfont3 (unicode, ac->italic),
				   /* bold */ 0,
				   ac->underline << 12 /* cell row 12 */,
				   VBI_NORMAL_SIZE);
		}
	}
}

static vbi_bool
png_export(vbi_export *e, FILE *fp, vbi_page *pg)
{
	gfx_instance *gfx = PARENT (e, gfx_instance, export);
	vbi_image_format format;
	unsigned int cw, ch;
	png_bytep *row_pointer;
	png_bytep image;
	unsigned int row_adv;
	png_byte pen[128];
	png_bytep canvas;
	unsigned int row;
	unsigned int i;
	png_structp png_ptr;
	png_infop info_ptr;
	png_color palette[80];
	png_byte alpha[80];
	png_text text[4];
	char title[80];

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
	format.pixfmt		= VBI_PIXFMT_RGB8;

	row_pointer = malloc (sizeof (*row_pointer) * format.height * 2);

	if (NULL == row_pointer) {
		vbi_export_error_printf
			(e, _("Unable to allocate %d byte buffer."),
			 sizeof (*row_pointer) * format.height * 2);
		return FALSE;
	}

	image = malloc (format.size);

	if (NULL == image) {
		vbi_export_error_printf
			(e, _("Unable to allocate %d KB image buffer."),
			 format.size / 1024);
		free(row_pointer);
		return FALSE;
	}

	row_adv = pg->columns * cw * (ch - 1);

	if (pg->drcs_clut)
		for (i = 2; i < 2 + 8 + 32; ++i) {
			pen[i]      = pg->drcs_clut[i]; /* opaque */
			pen[i + 64] = pg->drcs_clut[i] + 40; /* translucent */
		}

	canvas = image;

	for (row = 0; row < pg->rows; ++row) {
		unsigned int column;

		for (column = 0; column < pg->columns; ++column) {
			const vbi_char *ac;
	
			ac = pg->text + row * pg->columns + column;

			switch (ac->size) {
			case VBI_OVER_TOP:
			case VBI_OVER_BOTTOM:
				continue;

			default:
				break;
			}

			if (1)
				fprintf(stderr, "%2u %2u %04x %u\n",
					row, column, ac->unicode, ac->size);

			png_draw_char (canvas, format.bytes_per_line,
				       pg, ac, !e->reveal, pen,
				       pg->columns >= 40);

			switch (ac->size) {
			case VBI_DOUBLE_WIDTH:
			case VBI_DOUBLE_SIZE:
			case VBI_DOUBLE_SIZE2:
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

	/* Avoid possible longjmp breakage due to libpng ugliness */
	{ static int do_write() {

	if (setjmp (png_ptr->jmpbuf))
		return 1;

	png_init_io (png_ptr, fp);

	png_set_IHDR (png_ptr, info_ptr,
		      format.width,
		      format.height << gfx->double_height,
		      /* bit_depth */ 8,
		      PNG_COLOR_TYPE_PALETTE,
		      (gfx->double_height) ?
		              PNG_INTERLACE_ADAM7 : PNG_INTERLACE_NONE,
		      PNG_COMPRESSION_TYPE_DEFAULT,
		      PNG_FILTER_TYPE_DEFAULT);

	/* Could be optimized (or does libpng?) */
	for (i = 0; i < 40; ++i) {
		/* opaque */
		palette[i].red   = pg->color_map[i] & 0xFF;
		palette[i].green = (pg->color_map[i] >> 8) & 0xFF;
		palette[i].blue	 = (pg->color_map[i] >> 16) & 0xFF;
		alpha[i]	 = 255;

		/* translucent */
		palette[i + 40]  = palette[i];
		alpha[i + 40]	 = 128;
	}

	alpha[VBI_TRANSPARENT_BLACK] = 0;
	alpha[40 + VBI_TRANSPARENT_BLACK] = 0;

	png_set_PLTE (png_ptr, info_ptr, palette, 80);
	png_set_tRNS (png_ptr, info_ptr, alpha, 80, NULL);

	png_set_gAMA (png_ptr, info_ptr, 1.0 / 2.2);

	{
		unsigned int size = 0;

		if (e->network)
			size = snprintf (title, sizeof (title),
					 "%s ", e->network);
		else
			title[0] = 0;

		/*
		 *  FIXME
		 *  ISO 8859-1 (Latin-1) character set required,
		 *  see png spec for other
		 */
		if (pg->pgno < 0x100) {
			size += snprintf (title + size, sizeof (title) - size,
					  "Closed Caption"); /* proper name */
		} else if (VBI_ANY_SUBNO != pg->subno) {
			size += snprintf (title + size, sizeof(title) - size,
					  _("Teletext Page %3x.%x"),
					  pg->pgno, pg->subno);
		} else {
			size += snprintf (title + size, sizeof(title) - size,
					  _("Teletext Page %3x"), pg->pgno);
		}
	}

	CLEAR (text);

	text[0].key = "Title";
	text[0].text = title;
	text[0].compression = PNG_TEXT_COMPRESSION_NONE;
	text[1].key = "Software";
	text[1].text = e->creator;
	text[1].compression = PNG_TEXT_COMPRESSION_NONE;

	png_set_text (png_ptr, info_ptr, text, 2);

	png_write_info (png_ptr, info_ptr);

	if (gfx->double_height) {
		for (i = 0; i < format.height; ++i) {
			uint8_t *s;

			s = image + format.bytes_per_line * i;
			row_pointer[i * 2 + 0] = s;
			row_pointer[i * 2 + 1] = s;
		}
	} else {
		for (i = 0; i < format.height; ++i)
			row_pointer[i] = image + format.bytes_per_line * i;
	}

	png_write_image (png_ptr, row_pointer);

	png_write_end (png_ptr, info_ptr);

	png_destroy_write_struct (&png_ptr, &info_ptr);

	return 0;

	/* See setjmp above */
	} if (do_write ()) goto write_error; }

	free (row_pointer);
	free (image);

	return TRUE;

 write_error:
	vbi_export_write_error (e);

 unknown_error:
	if (row_pointer)
		free (row_pointer);

	if (image)
		free (image);

	return FALSE;
}

static vbi_export_info
info_png = {
	.keyword	= "png",
	.label		= N_("PNG"),
	.tooltip	= N_("Export this page as PNG image"),

	.mime_type	= "image/png",
	.extension	= "png",
};

vbi_export_class
vbi_export_class_png = {
	._public	= &info_png,
	._new		= gfx_new,
	._delete	= gfx_delete,
	.option_enum	= option_enum,
	.option_get	= option_get,
	.option_set	= option_set,
	.export		= png_export
};

VBI_AUTOREG_EXPORT_MODULE(vbi_export_class_png)

#endif /* HAVE_LIBPNG */
