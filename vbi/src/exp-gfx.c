/*
 *  libzvbi - Closed Caption and Teletext rendering
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
 *  Copyright (C) 1999 Paul Ortyl <ortylp@from.pl>
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

/* $Id: exp-gfx.c,v 1.13.2.1 2007-11-03 21:10:29 tomzo Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
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
#include "ccfont2.xbm"

/* Teletext character cell dimensions - hardcoded (DRCS) */

#define TCW 12
#define TCH 10

#define TCPL (wstfont2_width / TCW * wstfont2_height / TCH)

/* Closed Caption character cell dimensions */

#define CCW 16
#define CCH 26 /* line doubled */

#define CCPL (ccfont2_width / CCW * ccfont2_height / CCH)

static void init_gfx(void) __attribute__ ((constructor));

static void
init_gfx(void)
{
	uint8_t *t, *p;
	int i, j;

	/* de-interleave font image (puts all chars in row 0) */

	if (!(t = malloc(wstfont2_width * wstfont2_height / 8)))
		exit(EXIT_FAILURE);

	for (p = t, i = 0; i < TCH; i++)
		for (j = 0; j < wstfont2_height; p += wstfont2_width / 8, j += TCH)
			memcpy(p, wstfont2_bits + (j + i) * wstfont2_width / 8,
			       wstfont2_width / 8);

	memcpy(wstfont2_bits, t, wstfont2_width * wstfont2_height / 8);
	free (t);

	if (!(t = malloc(ccfont2_width * ccfont2_height / 8)))
		exit(EXIT_FAILURE);

	for (p = t, i = 0; i < CCH; i++)
		for (j = 0; j < ccfont2_height; p += ccfont2_width / 8, j += CCH)
			memcpy(p, ccfont2_bits + (j + i) * ccfont2_width / 8,
			       ccfont2_width / 8);

	memcpy(ccfont2_bits, t, ccfont2_width * ccfont2_height / 8);
	free(t);
}

/**
 * @internal
 * @param c Unicode.
 * @param italic @c TRUE to switch to slanted character set (doesn't affect
 *          Hebrew and Arabic). If this is a G1 block graphic character
 *          switch to separated block mosaic set.
 * 
 * Translate Unicode character to glyph number in wstfont2 image. 
 * 
 * @return
 * Glyph number.
 */
static unsigned int
unicode_wstfont2(unsigned int c, int italic)
{
	static const unsigned short specials[] = {
		0x01B5, 0x2016, 0x01CD, 0x01CE, 0x0229, 0x0251, 0x02DD, 0x02C6,
		0x02C7, 0x02C9, 0x02CA, 0x02CB, 0x02CD, 0x02CF, 0x02D8, 0x02D9,
		0x02DA, 0x02DB, 0x02DC, 0x2014, 0x2018, 0x2019, 0x201C,	0x201D,
		0x20A0, 0x2030, 0x20AA, 0x2122, 0x2126, 0x215B, 0x215C, 0x215D,
		0x215E, 0x2190, 0x2191, 0x2192, 0x2193, 0x25A0, 0x266A, 0xE800,
		0xE75F };
	const unsigned int invalid = 357;
	unsigned int i;

	if (c < 0x0180) {
		if (c < 0x0080) {
			if (c < 0x0020)
				return invalid;
			else /* %3 Basic Latin (ASCII) 0x0020 ... 0x007F */
				c = c - 0x0020 + 0 * 32;
		} else if (c < 0x00A0)
			return invalid;
		else /* %3 Latin-1 Supplement, Latin Extended-A 0x00A0 ... 0x017F */
			c = c - 0x00A0 + 3 * 32;
	} else if (c < 0xEE00) {
		if (c < 0x0460) {
			if (c < 0x03D0) {
				if (c < 0x0370)
					goto special;
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
			goto special;
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
special:
	for (i = 0; i < sizeof(specials) / sizeof(specials[0]); i++)
		if (specials[i] == c) {
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
 * @param italic @c TRUE to switch to slanted character set.
 * 
 * Translate Unicode character to glyph number in ccfont2 image. 
 * 
 * @return
 * Glyph number.
 */
static unsigned int
unicode_ccfont2(unsigned int c, int italic)
{
	static const unsigned short specials[] = {
								0x00E1, 0x00E9,
		0x00ED, 0x00F3, 0x00FA, 0x00E7, 0x00F7, 0x00D1, 0x00F1, 0x25A0,
		0x00AE, 0x00B0, 0x00BD, 0x00BF, 0x2122, 0x00A2, 0x00A3, 0x266A,
		0x00E0, 0x0020, 0x00E8, 0x00E2, 0x00EA, 0x00EE, 0x00F4, 0x00FB };
	unsigned int i;

	if (c < 0x0020)
		c = 15; /* invalid */
	else if (c < 0x0080)
		c = c;
	else {
		for (i = 0; i < sizeof(specials) / sizeof(specials[0]); i++)
			if (specials[i] == c) {
				c = i + 6;
				goto slant;
			}

		c = 15; /* invalid */
	}

slant:
	if (italic)
		c += 4 * 32;

	return c;
}

/**
 * @internal
 * @param p Plane of @a canvas_type char, short, int.
 * @param i Index.
 *
 * @return
 * Pixel @a i in plane @a p.
 */
#define peek(p, i)							\
((canvas_type == sizeof(uint8_t)) ? ((uint8_t *)(p))[i] :		\
    ((canvas_type == sizeof(uint16_t)) ? ((uint16_t *)(p))[i] :		\
	((uint32_t *)(p))[i]))

/**
 * @internal
 * @param p Plane of @a canvas_type char, short, int.
 * @param i Index.
 * @param v Value.
 * 
 * Set pixel @a i in plane @a p to value @a v.
 */
#define poke(p, i, v)							\
((canvas_type == sizeof(uint8_t)) ? (((uint8_t *)(p))[i] = (v)) :	\
    ((canvas_type == sizeof(uint16_t)) ? (((uint16_t *)(p))[i] = (v)) :	\
	(((uint32_t *)(p))[i] = (v))))

/**
 * @internal
 * @param canvas_type sizeof(char, short, int).
 * @param canvas Pointer to image plane where the character is to be drawn.
 * @param rowstride @a canvas <em>byte</em> distance from line to line.
 * @param pen Pointer to color palette of @a canvas_type (index 0 background
 *   pixels, index 1 foreground pixels).
 * @param font Pointer to font image with width @a cpl x @a cw pixels, height
 *   @a ch pixels, depth one bit, bit '1' is foreground.
 * @param cpl Chars per line (number of characters in @a font image).
 * @param cw Character cell width in pixels.
 * @param ch Character cell height in pixels.
 * @param glyph Glyph number in font image, 0 ... @a cpl - 1.
 * @param bold Draw character bold (font image | font image << 1).
 * @param underline Bit mask of character rows. For each bit
 *   1 << (n = 0 ... @a ch - 1) set all of character row n to
 *   foreground color.
 * @param size Size of character, either NORMAL, DOUBLE_WIDTH (draws left
 *   and right half), DOUBLE_HEIGHT (draws upper half only),
 *   DOUBLE_SIZE (left and right upper half), DOUBLE_HEIGHT2
 *   (lower half), DOUBLE_SIZE2 (left and right lower half).
 * 
 * Draw one character (function template - define a static version with
 * constant @a canvas_type, @a font, @a cpl, @a cw, @a ch).
 */
static inline void
draw_char(int canvas_type, uint8_t *canvas, int rowstride,
	  uint8_t *pen, uint8_t *font, int cpl, int cw, int ch,
	  int glyph, int bold, unsigned int underline, vbi_size size)
{
	uint8_t *src;
	int shift, x, y;

	bold = !!bold;
	assert(cw >= 8 && cw <= 16);
	assert(ch >= 1 && cw <= 31);

	x = glyph * cw;
	shift = x & 7;
	src = font + (x >> 3);

	switch (size) {
	case VBI_DOUBLE_HEIGHT2:
	case VBI_DOUBLE_SIZE2:
		src += cpl * cw / 8 * ch / 2;
		underline >>= ch / 2;

	case VBI_DOUBLE_HEIGHT:
	case VBI_DOUBLE_SIZE: 
		ch >>= 1;

	default:
		break;
	}

	for (y = 0; y < ch; underline >>= 1, y++) {
		int bits = ~0;

		if (!(underline & 1)) {
#ifdef __GNUC__
#if #cpu (i386)
			bits = (*((uint16_t *) src) >> shift);
#else
                        /* unaligned/little endian */
			bits = ((src[1] * 256 + src[0]) >> shift);
#endif
#else
			bits = ((src[1] * 256 + src[0]) >> shift);
#endif
			bits |= bits << bold;
		}

		switch (size) {
		case VBI_NORMAL_SIZE:
			for (x = 0; x < cw; bits >>= 1, x++)
				poke(canvas, x, peek(pen, bits & 1));

			canvas += rowstride;

			break;

		case VBI_DOUBLE_HEIGHT:
		case VBI_DOUBLE_HEIGHT2:
			for (x = 0; x < cw; bits >>= 1, x++) {
				unsigned int col = peek(pen, bits & 1);

				poke(canvas, x, col);
				poke(canvas, x + rowstride / canvas_type, col);
			}

			canvas += rowstride * 2;

			break;

		case VBI_DOUBLE_WIDTH:
			for (x = 0; x < cw * 2; bits >>= 1, x += 2) {
				unsigned int col = peek(pen, bits & 1);

				poke(canvas, x + 0, col);
				poke(canvas, x + 1, col);
			}

			canvas += rowstride;

			break;

		case VBI_DOUBLE_SIZE:
		case VBI_DOUBLE_SIZE2:
			for (x = 0; x < cw * 2; bits >>= 1, x += 2) {
				unsigned int col = peek(pen, bits & 1);

				poke(canvas, x + 0, col);
				poke(canvas, x + 1, col);
				poke(canvas, x + rowstride / canvas_type + 0, col);
				poke(canvas, x + rowstride / canvas_type + 1, col);
			}

			canvas += rowstride * 2;

			break;

		default:
			break;
		}

		src += cpl * cw / 8;
	}
}

/**
 * @internal
 * @param canvas_type sizeof(char, short, int).
 * @param canvas Pointer to image plane where the character is to be drawn.
 * @param rowstride @a canvas <em>byte</em> distance from line to line.
 * @param pen Pointer to color palette of @a canvas_type (index 0 ... 1 for
 *   depth 1 DRCS, 0 ... 3 for depth 2, 0 ... 15 for depth 4).
 * @param color Offset into color palette.
 * @param font Pointer to DRCS image. Each pixel is coded in four bits, an
 *   index into the color palette, and stored in LE order (i. e. first
 *   pixel 0x0F, second pixel 0xF0). Character size is 12 x 10 pixels,
 *   60 bytes, without padding.
 * @param glyph Glyph number in font image, 0x00 ... 0x3F.
 * @param size Size of character, either NORMAL, DOUBLE_WIDTH (draws left
 *   and right half), DOUBLE_HEIGHT (draws upper half only),
 *   DOUBLE_SIZE (left and right upper half), DOUBLE_HEIGHT2
 *   (lower half), DOUBLE_SIZE2 (left and right lower half).
 * 
 * Draw one Teletext Dynamically Redefinable Character (function template -
 * define a static version with constant @a canvas_type, @a font).
 */
static inline void
draw_drcs(int canvas_type, uint8_t *canvas, unsigned int rowstride,
	  uint8_t *pen, int color, uint8_t *font, int glyph, vbi_size size)
{
	uint8_t *src;
	unsigned int col;
	int x, y;

	src = font + glyph * 60;
	pen = pen + color * canvas_type;

	switch (size) {
	case VBI_NORMAL_SIZE:
		for (y = 0; y < TCH; canvas += rowstride, y++)
			for (x = 0; x < 12; src++, x += 2) {
				poke(canvas, x + 0, peek(pen, *src & 15));
				poke(canvas, x + 1, peek(pen, *src >> 4));
			}
		break;

	case VBI_DOUBLE_HEIGHT2:
		src += 30;

	case VBI_DOUBLE_HEIGHT:
		for (y = 0; y < TCH / 2; canvas += rowstride * 2, y++)
			for (x = 0; x < 12; src++, x += 2) {
				col = peek(pen, *src & 15);
				poke(canvas, x + 0, col);
				poke(canvas, x + rowstride / canvas_type + 0, col);

				col = peek(pen, *src >> 4);
				poke(canvas, x + 1, col);
				poke(canvas, x + rowstride / canvas_type + 1, col);
			}
		break;

	case VBI_DOUBLE_WIDTH:
		for (y = 0; y < TCH; canvas += rowstride, y++)
			for (x = 0; x < 12 * 2; src++, x += 4) {
				col = peek(pen, *src & 15);
				poke(canvas, x + 0, col);
				poke(canvas, x + 1, col);

				col = peek(pen, *src >> 4);
				poke(canvas, x + 2, col);
				poke(canvas, x + 3, col);
			}
		break;

	case VBI_DOUBLE_SIZE2:
		src += 30;

	case VBI_DOUBLE_SIZE:
		for (y = 0; y < TCH / 2; canvas += rowstride * 2, y++)
			for (x = 0; x < 12 * 2; src++, x += 4) {
				col = peek(pen, *src & 15);
				poke(canvas, x + 0, col);
				poke(canvas, x + 1, col);
				poke(canvas, x + rowstride / canvas_type + 0, col);
				poke(canvas, x + rowstride / canvas_type + 1, col);

				col = peek(pen, *src >> 4);
				poke(canvas, x + 2, col);
				poke(canvas, x + 3, col);
				poke(canvas, x + rowstride / canvas_type + 2, col);
				poke(canvas, x + rowstride / canvas_type + 3, col);
			}
		break;

	default:
		break;
	}
}

/**
 * @internal
 * @param canvas_type sizeof(char, short, int).
 * @param canvas Pointer to image plane where the character is to be drawn.
 * @param rowstride @a canvas <em>byte</em> distance from line to line.
 * @param color Color value of @a canvas_type.
 * @param cw Character width in pixels.
 * @param ch Character height in pixels.
 * 
 * Draw blank character.
 */
static inline void
draw_blank(int canvas_type, uint8_t *canvas, unsigned int rowstride,
	   unsigned int color, int cw, int ch)
{
	int x, y;

	for (y = 0; y < ch; y++) {
		for (x = 0; x < cw; x++)
			poke(canvas, x, color);

		canvas += rowstride;
	}
}

/**
 * @param pg Source vbi_page, see vbi_fetch_cc_page().
 * @param fmt Target format. For now only VBI_PIXFMT_RGBA32_LE (vbi_rgba) permitted.
 * @param canvas Pointer to destination image (currently an array of vbi_rgba), this
 *   must be at least @a rowstride * @a height * 26 bytes large.
 * @param rowstride @a canvas <em>byte</em> distance from line to line.
 *   If this is -1, pg->columns * 16 * sizeof(vbi_rgba) bytes will be assumed.
 * @param column First source column, 0 ... pg->columns - 1.
 * @param row First source row, 0 ... pg->rows - 1.
 * @param width Number of columns to draw, 1 ... pg->columns.
 * @param height Number of rows to draw, 1 ... pg->rows.
 * 
 * Draw a subsection of a Closed Caption vbi_page. In this mode one
 * character occupies 16 x 26 pixels.
 */
void
vbi_draw_cc_page_region(vbi_page *pg,
			vbi_pixfmt fmt, void *canvas, int rowstride,
			int column, int row, int width, int height)
{
        union {
	        vbi_rgba        rgba[2];
	        uint8_t         pal8[2];
        } pen;
	int count, row_adv;
	vbi_char *ac;
        int canvas_type;

	if (fmt == VBI_PIXFMT_RGBA32_LE) {
                canvas_type = 4;
	} else if (fmt == VBI_PIXFMT_PAL8) {
                canvas_type = 1;
        } else {
		return;
        }

	if (0) {
		int i, j;

		for (i = 0; i < pg->rows; i++) {
			fprintf(stderr, "%2d: ", i);
			ac = &pg->text[i * pg->columns];
			for (j = 0; j < pg->columns; j++)
				fprintf(stderr, "%d%d%02x ",
					ac[j].foreground,
					ac[j].background,
					ac[j].unicode & 0xFF);
			fprintf(stderr, "\n");
		}
	}

	if (rowstride == -1)
		rowstride = pg->columns * CCW * canvas_type;

	row_adv = rowstride * CCH - width * CCW * canvas_type;

	for (; height > 0; height--, row++) {
		ac = &pg->text[row * pg->columns + column];

		for (count = width; count > 0; count--, ac++) {
                        if (canvas_type == 1) {
			        pen.pal8[0] = ac->background;
			        pen.pal8[1] = ac->foreground;
                        } else {
			        pen.rgba[0] = pg->color_map[ac->background];
			        pen.rgba[1] = pg->color_map[ac->foreground];
                        }

			draw_char (canvas_type,
				   (uint8_t *) canvas,
				   rowstride,
				   (uint8_t *) &pen,
				   (uint8_t *) ccfont2_bits,
				   CCPL, CCW, CCH,
				   unicode_ccfont2 (ac->unicode, ac->italic),
				   0 /* bold */,
				   (ac->underline
				    * (3 << 24)) /* cell row 24, 25 */,
				   VBI_NORMAL_SIZE);

			canvas += CCW * canvas_type;
		}

		canvas += row_adv;
	}
}

/**
 * @param pg Source page.
 * @param fmt Target format. For now only VBI_PIXFMT_RGBA32_LE (vbi_rgba) and
 *   VBI_PIXFMT_PAL8 (1-byte palette indices) are permitted.
 * @param canvas Pointer to destination image (depending on the format, either
 *   an array of vbi_rgba or uint8_t), this must be at least
 *   @a rowstride * @a height * 10 bytes large.
 * @param rowstride @a canvas <em>byte</em> distance from line to line.
 *   If this is -1, pg->columns * 12 * sizeof(vbi_rgba) bytes will be assumed.
 * @param column First source column, 0 ... pg->columns - 1.
 * @param row First source row, 0 ... pg->rows - 1.
 * @param width Number of columns to draw, 1 ... pg->columns.
 * @param height Number of rows to draw, 1 ... pg->rows.
 * @param reveal If FALSE, draw characters flagged 'concealed' (see vbi_char) as
 *   space (U+0020).
 * @param flash_on If FALSE, draw characters flagged 'blink' (see vbi_char) as
 *   space (U+0020).
 * 
 * Draw a subsection of a Teletext vbi_page. In this mode one
 * character occupies 12 x 10 pixels.  Note this function does
 * not consider transparency (e.g. on boxed pages)
 */
void
vbi_draw_vt_page_region(vbi_page *pg,
			vbi_pixfmt fmt, void *canvas, int rowstride,
			int column, int row, int width, int height,
			int reveal, int flash_on)
{
        union {
	        vbi_rgba        rgba[64];
	        uint8_t         pal8[64];
        } pen;
	int count, row_adv;
	int conceal, off, unicode;
	vbi_char *ac;
        int canvas_type;
	int i;

	if (fmt == VBI_PIXFMT_RGBA32_LE) {
                canvas_type = 4;
	} else if (fmt == VBI_PIXFMT_PAL8) {
                canvas_type = 1;
        } else {
		return;
        }

	if (0) {
		int i, j;

		for (i = 0; i < pg->rows; i++) {
			fprintf(stderr, "%2d: ", i);
			ac = &pg->text[i * pg->columns];
			for (j = 0; j < pg->columns; j++)
				fprintf(stderr, "%04x ", ac[j].unicode);
			fprintf(stderr, "\n");
		}
	}

	if (rowstride == -1)
		rowstride = pg->columns * 12 * canvas_type;

	row_adv = rowstride * 10 - width * 12 * canvas_type;

	conceal = !reveal;
	off = !flash_on;

	if (pg->drcs_clut)
		for (i = 2; i < 2 + 8 + 32; i++)
                        if (canvas_type == 1)
                                pen.pal8[i] = pg->drcs_clut[i];
                        else
                                pen.rgba[i] = pg->color_map[pg->drcs_clut[i]];

	for (; height > 0; height--, row++) {
		ac = &pg->text[row * pg->columns + column];

		for (count = width; count > 0; count--, ac++) {
			if ((ac->conceal & conceal) || (ac->flash & off))
				unicode = 0x0020;
			else
				unicode = ac->unicode;

                        if (canvas_type == 1) {
                                pen.pal8[0] = ac->background;
                                pen.pal8[1] = ac->foreground;
                        } else {
                                pen.rgba[0] = pg->color_map[ac->background];
                                pen.rgba[1] = pg->color_map[ac->foreground];
                        }

			switch (ac->size) {
			case VBI_OVER_TOP:
			case VBI_OVER_BOTTOM:
				break;

			default:
				if (vbi_is_drcs(unicode)) {
					uint8_t *font = pg->drcs[(unicode >> 6) & 0x1F];

					if (font)
						draw_drcs(canvas_type, canvas, rowstride,
							  (uint8_t *) &pen, ac->drcs_clut_offs,
							  font, unicode & 0x3F, ac->size);
					else /* shouldn't happen */
						draw_blank(canvas_type, canvas, rowstride,
							   ((canvas_type == 1) ? pen.pal8[0]: pen.rgba[0]),
                                                           TCW, TCH);
				} else {
					draw_char (canvas_type,
						   canvas,
						   rowstride,
						   (uint8_t *) &pen,
						   (uint8_t *) wstfont2_bits,
						   TCPL, TCW, TCH,
						   unicode_wstfont2 (unicode, ac->italic),
						   ac->bold,
						   ac->underline << 9 /* cell row 9 */,
						   ac->size);
				}
			}

			canvas = (uint8_t *)canvas + TCW * canvas_type;
		}

		canvas = (uint8_t *)canvas + row_adv;
	}
}

/*
 *  This won't scale with proportional spacing or custom fonts,
 *  to be removed.
 */

/**
 * @param w
 * @param h
 *
 * @deprecated
 * Character cells are 12 x 10 for Teletext and 16 x 26 for Caption.
 * Page size is in vbi_page.
 */
void
vbi_get_max_rendered_size(int *w, int *h)
{
  if (w) *w = 41 * TCW;
  if (h) *h = 25 * TCH;
}

/**
 * @param w
 * @param h
 *
 * @deprecated
 * Character cells are 12 x 10 for Teletext and 16 x 26 for Caption.
 */
void
vbi_get_vt_cell_size(int *w, int *h)
{
  if (w) *w = TCW;
  if (h) *h = TCH;
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
	e = e;

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

/**
 * @internal
 * @param e Pointer to export context
 * @param pg Page reference
 * @param title Output buffer for returning the title
 * @param title_max Size of @a title buffer
 * 
 * Determine a suitable label for the hardcopy.
 * The label is inserted as comment inside of XPM or PNG image files.
 */
static void
get_image_title(vbi_export *e, vbi_page *pg, char *title, int title_max)
{
        int size = 0;

        if (e->network)
                size = snprintf(title, title_max - 1, "%s ", e->network);
        else
                title[0] = 0;

        /*
         *  FIXME
         *  ISO 8859-1 (Latin-1) character set required,
         *  see png spec for other
         */
        if (pg->pgno < 0x100) {
                size += snprintf(title + size, title_max - size - 1,
                                 "Closed Caption"); /* no i18n, proper name */
        } else if (pg->subno != VBI_ANY_SUBNO) {
                size += snprintf(title + size, title_max - size - 1,
                                 _("Teletext Page %3x.%x"),
                                 pg->pgno, pg->subno);
        } else {
                size += snprintf(title + size, title_max - size - 1,
                                 _("Teletext Page %3x"), pg->pgno);
        }
}


/*
 *  PPM - Portable Pixmap File (raw)
 */

static vbi_bool
ppm_export(vbi_export *e, FILE *fp, vbi_page *pg)
{
	gfx_instance *gfx = PARENT(e, gfx_instance, export);
	int cw, ch, ww, size, scale, row;
	vbi_rgba *image;
	uint8_t *body;
	int i;

	if (pg->columns < 40) /* caption */ {
		cw = CCW;
		ch = CCH;
		/* characters already line-doubled */
		scale = !!gfx->double_height;
	} else {
		cw = TCW;
		ch = TCH;
		scale = 1 + !!gfx->double_height;
	}

	ww = cw * pg->columns;
	size = ww * ch * 1; /* one character row */

	if (!(image = malloc(size * sizeof(*image)))) {
		vbi_export_error_printf(e, _("Unable to allocate %d KB image buffer."),
					size * sizeof(*image) / 1024);
		return FALSE;
	}

	fprintf(fp, "P6 %d %d 255\n",
		cw * pg->columns, (ch * pg->rows / 2) << scale);

	if (ferror(fp))
		goto write_error;

	for (row = 0; row < pg->rows; row++) {
		if (pg->columns < 40)
			vbi_draw_cc_page_region(pg, VBI_PIXFMT_RGBA32_LE, image, -1,
						0, row, pg->columns, 1 /* rows */);
		else
			vbi_draw_vt_page_region(pg, VBI_PIXFMT_RGBA32_LE, image, -1,
						0, row, pg->columns, 1 /* rows */,
						!e->reveal, 1 /* flash_on */);
		body = (uint8_t *) image;

		if (scale == 0)
			for (i = 0; i < size; body += 3, i++) {
				body[0] = ((image[i] & 0xFF) + (image[i + ww] & 0xFF)
					   + 0x01) >> 1;
				body[1] = ((image[i] & 0xFF00) + (image[i + ww] & 0xFF00)
					   + 0x0100) >> 9;
				body[2] = ((image[i] & 0xFF0000) + (image[i + ww] & 0xFF0000)
					   + 0x010000) >> 17;
			}
		else
			for (i = 0; i < size; body += 3, i++) {
				unsigned int n = image[i];

				body[0] = n;
				body[1] = n >> 8;
				body[2] = n >> 16;
			}

		switch (scale) {
			int rows, stride;

		case 0:
			body = (uint8_t *) image;
			rows = ch / 2;
			stride = ww * 3;

			for (i = 0; i < rows; i++, body += stride * 2)
				if (!fwrite(body, stride, 1, fp))
					goto write_error;
			break;

		case 1:
			if (!fwrite(image, size * 3, 1, fp))
				goto write_error;
			break;

		case 2:
			body = (uint8_t *) image;
			stride = cw * pg->columns * 3;

			for (i = 0; i < ch; body += stride, i++) {
				if (!fwrite(body, stride, 1, fp))
					goto write_error;
				if (!fwrite(body, stride, 1, fp))
					goto write_error;
			}

			break;
		}
	}

	free(image);
	image = NULL;

	return TRUE;

write_error:
	vbi_export_write_error(e);

	if (image)
		free(image);

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
 * PNG and XPM drawing functions (palette-based)
 */
static void
draw_char_cc_indexed(uint8_t * canvas, int rowstride,  uint8_t * pen,
		     int unicode, vbi_char *ac)
{
	draw_char(sizeof(*canvas), canvas, rowstride,
		  pen, (uint8_t *) ccfont2_bits, CCPL, CCW, CCH,
		  unicode_ccfont2(unicode, ac->italic), 0 /* bold */,
		  ac->underline * (3 << 24) /* cell row 24, 25 */,
		  VBI_NORMAL_SIZE);
}

static void
draw_char_vt_indexed(uint8_t * canvas, int rowstride,  uint8_t * pen,
		     int unicode, vbi_char *ac)
{
	draw_char(sizeof(*canvas), canvas, rowstride,
		  pen, (uint8_t *) wstfont2_bits, TCPL, TCW, TCH,
		  unicode_wstfont2(unicode, ac->italic), ac->bold,
		  ac->underline << 9 /* cell row 9 */, ac->size);
}

static void
draw_drcs_indexed(uint8_t * canvas, int rowstride, uint8_t * pen,
		  uint8_t *font, int glyph, vbi_size size)
{
	draw_drcs(sizeof(*canvas), canvas, rowstride,
		  (uint8_t *) pen, 0, font, glyph, size);
}

static void
draw_row_indexed(vbi_page * pg, vbi_char * ac, uint8_t * canvas, uint8_t * pen,
                 int rowstride, vbi_bool conceal, vbi_bool is_cc, int scale)
{
        const int cw = is_cc ? CCW : TCW;
        const int ch = is_cc ? CCH : TCH;
	void (* draw_char_indexed)(uint8_t *, int, uint8_t *, int, vbi_char *)
                = is_cc ? draw_char_cc_indexed : draw_char_vt_indexed;
        int row, column;
        int unicode;

        for (column = 0; column < pg->columns ; canvas += cw, column++, ac++) {

                if (ac->size == VBI_OVER_TOP
                    || ac->size == VBI_OVER_BOTTOM)
                        continue;

                unicode = (ac->conceal & conceal) ? 0x0020u : ac->unicode;

                switch (ac->opacity) {
                case VBI_TRANSPARENT_SPACE:
                        /*
                         *  Transparent foreground and background.
                         */
                        draw_blank(sizeof(*canvas), canvas,
                                   rowstride, VBI_TRANSPARENT_BLACK, cw, ch);
                        break;

                case VBI_TRANSPARENT_FULL:
                        /*
                         *  Transparent background, opaque foreground. Currently not used.
                         *  Mind Teletext level 2.5 foreground and background transparency
                         *  by referencing colormap entry 8, VBI_TRANSPARENT_BLACK.
                         *  The background of multicolor DRCS is ambiguous, so we make
                         *  them opaque.
                         */
                        if (vbi_is_drcs(unicode)) {
                                uint8_t *font = pg->drcs[(unicode >> 6) & 0x1F];

                                pen[0] = VBI_TRANSPARENT_BLACK;
                                pen[1] = ac->foreground;

                                if (font && !is_cc)
                                        draw_drcs_indexed(canvas, rowstride, pen,
                                                          font, unicode & 0x3F, ac->size);
                                else /* shouldn't happen */
                                        draw_blank(sizeof(*canvas), (uint8_t *) canvas,
                                                   rowstride, VBI_TRANSPARENT_BLACK, cw, ch);
                        } else {
                                pen[0] = VBI_TRANSPARENT_BLACK;
                                pen[1] = ac->foreground;

                                draw_char_indexed(canvas, rowstride, pen, unicode, ac);
                        }

                        break;

                case VBI_SEMI_TRANSPARENT:
                        /*
                         *  Translucent background (for 'boxed' text), opaque foreground.
                         *  The background of multicolor DRCS is ambiguous, so we make
                         *  them completely translucent. 
                         */
                        if (vbi_is_drcs(unicode)) {
                                uint8_t *font = pg->drcs[(unicode >> 6) & 0x1F];

                                pen[64] = ac->background + 40;
                                pen[65] = ac->foreground;

                                if (font && !is_cc)
                                        draw_drcs_indexed(canvas, rowstride,
                                                          (uint8_t *)(pen + 64),
                                                          font, unicode & 0x3F, ac->size);
                                else /* shouldn't happen */
                                        draw_blank(sizeof(*canvas), (uint8_t *) canvas,
                                                   rowstride, VBI_TRANSPARENT_BLACK, cw, ch);
                        } else {
                                pen[0] = ac->background + 40; /* translucent */
                                pen[1] = ac->foreground;

                                draw_char_indexed(canvas, rowstride, pen, unicode, ac);
                        }

                        break;

                case VBI_OPAQUE:
                        pen[0] = ac->background;
                        pen[1] = ac->foreground;

                        if (vbi_is_drcs(unicode)) {
                                uint8_t *font = pg->drcs[(unicode >> 6) & 0x1F];

                                if (font && !is_cc)
                                        draw_drcs_indexed(canvas, rowstride, pen,
                                                          font, unicode & 0x3F, ac->size);
                                else /* shouldn't happen */
                                        draw_blank(sizeof(*canvas), (uint8_t *) canvas,
                                                   rowstride, pen[0], cw, ch);
                        } else
                                draw_char_indexed(canvas, rowstride, pen, unicode, ac);
                        break;
                }
        }
}

#define ABORT_ON_MALLOC_FAILURE(E,PTR,SZ) \
	do {if ((PTR) == NULL) { \
		vbi_export_error_printf((E), _("Unable to allocate %d KB image buffer."), \
					(SZ) / 1024); \
		goto abort; \
	}}while(0)

#if 0
#define xpm_append_data(IMG,BUF_LEN,FMT...) (IMG) += sprintf((IMG), ## FMT );
#else
/* hackery required because libzvbi shuns the friendly sprintf */
#define xpm_append_data(IMG,BUF_LEN,FMT...) \
        do { \
                if ((BUF_LEN)>0) { \
                        int __len = snprintf((IMG), (BUF_LEN), ## FMT); \
                        if ((__len < 0) || (__len >= (BUF_LEN))) { \
                                (BUF_LEN) = 0; \
                                return (IMG); \
                        } \
                        (IMG) += __len; \
                        (BUF_LEN) -= __len; \
                } else { \
                        /* already failed earlier */ \
                } \
        } while(0)
#endif

/*
 *  XPM - X Pixmap
 *
 *  According to "XPM Manual" version 3.4i, 1996-09-10, by Arnaud Le Hors
 */
static const uint8_t xpm_col_codes[40] = " 1234567.BCDEFGHIJKLMNOPabcdefghijklmnop";

/**
 * @internal
 * @param ww Image width in pixels
 * @param wh Image height in pixels
 * @param title Optional title to be included in extension data, or @c NULL.
 * @param creator Optional software name and version to be included in
 *   extension data, or @c NULL.
 * 
 * This function calculates and returns the size of an XPM image of the given
 * size and including optional extension data.  The result is intended to be
 * used for allocating a buffer. The result is not exact as a small
 * safety margin is included.
 */
static int
xpm_calc_img_buf_size(int ww, int wh, const char * title, const char *creator)
{
        int l = 109 +                   /* header (incl. 4-digit width & height & 2-digit col num) */
                15 * 40 +               /* color palette with 40 members */
                13 +                    /* row start comment */
                13 + (ww + 4) * wh +    /* data rows */
                3 +                     /* closing bracket */
                50;                     /* safety margin */

        if ((title != NULL) || (creator != NULL)) {
                l += 7 +                /* XPMEXT keyword in header */
                     ((title == NULL) ? 0: (strlen(title)+17)) + /* XPMEXT keywords + label + content */
                     ((creator == NULL) ? 0: (strlen(creator)+20)) +
                     12;                /* XPMENDEXT keyword */

        }

        return l;
}

/**
 * @internal
 * @param pg Page reference
 * @param img Image data output buffer pointer
 * @param ww Image width
 * @param wh Image height
 * @param title Optional title to be included in extension data, or @c NULL.
 * @param creator Optional software name and version to be included in
 *   extension data, or @c NULL.
 * @param buf_len Pointer to remaining space in @a img buffer. This value
 *   is updated after appending data to the buffer.
 * 
 * This function writes an XPM header and color palette for an image with
 * the given size and optional extension data.  The color palette is
 * retrieved from the page referenced by @a pg.
 */
static uint8_t *
xpm_write_img_header(vbi_page * pg, uint8_t * img, int ww, int wh,
                     const char * title, const char *creator, int * buf_len)
{
        vbi_bool do_ext = ((title != NULL) || (creator != NULL));
        int idx;

        /* warning: adapt buf size calculation when changing the text! */
        xpm_append_data(img, *buf_len,
                             "/* XPM */\n"
                             "static char *image[] = {\n"
                             "/* width height ncolors chars_per_pixel */\n"
                             "\"%d %d %d %d%s\",\n"
                             "/* colors */\n",
                             ww, wh, 40, 1,
                             do_ext ? " XPMEXT":"");

        /* write color palette (including unused colors - could be optimized) */
        for (idx = 0; idx < 40; idx++) {
                if (idx == 8) {
                        xpm_append_data(img, *buf_len,
                                        "\"%c c None\",\n", xpm_col_codes[idx]);
                } else {
                        xpm_append_data(img, *buf_len,
                                        "\"%c c #%02X%02X%02X\",\n",
                                        xpm_col_codes[idx],
                                        pg->color_map[idx] & 0xFF,
                                        (pg->color_map[idx] >> 8) & 0xFF,
                                        (pg->color_map[idx] >> 16) & 0xFF);
                }
        }

        xpm_append_data(img, *buf_len, "/* pixels */\n");

        return img;
}

/**
 * @internal
 * @param img Image data output buffer pointer
 * @param title Optional title to be included in extension data, or @c NULL.
 * @param creator Optional software name and version to be included in
 *   extension data, or @c NULL.
 * @param buf_len Pointer to remaining space in @a img buffer. This value
 *   is updated after appending data to the buffer.
 * 
 * This function writes an XPM "footer" (i.e. anything following the actual
 * image data) and optionally appends image title and software names as
 * extension data.
 */
static uint8_t *
xpm_write_img_footer(uint8_t * img, const char * title, const char *creator, int *buf_len)
{
        char * p;

        /* warning: adapt buf size calculation when changing the text! */
        if ((title != NULL) && (title[0] != 0)) {
                while ((p = strchr(title, '"')) != NULL)
                        *p = '\'';
                xpm_append_data(img, *buf_len, "\"XPMEXT title %s\",\n", title);
        }
        if ((creator != NULL) && (creator[0] != 0)) {
                while ((p = strchr(creator, '"')) != NULL)
                        *p = '\'';
                xpm_append_data(img, *buf_len, "\"XPMEXT software %s\",\n", creator);
        }
        if ((title != NULL) || (creator != NULL)) {
                xpm_append_data(img, *buf_len, "\"XPMENDEXT\"\n");
        }

        xpm_append_data(img, *buf_len, "};\n");

        return img;
}

/**
 * @internal
 * @param img Image data output buffer pointer
 * @param buf Image source data as written by @c draw_row_indexed()
 * @param title Optional title to be included in extension data, or @c NULL.
 * @param creator Optional software name and version to be included in
 *   extension data, or @c NULL.
 * @param buf_len Pointer to remaining space in @a img buffer. This value
 *   is updated after appending data to the buffer.
 * 
 * This function writes XPM image data for one teletext or closed caption
 * row (i.e. several pixel lines) into the given buffer. The image is scaled
 * vertically on the fly.
 *
 * The conversion consists of adding C-style framing at the start and end of
 * each pixel row and converting "binary" palette indices into color code
 * characters.  CLUT 1 color 0 is hard-coded as transparent; semi-transparent
 * colors are also mapped to the transparent color code since XPM does not
 * have an alpha channel.
 */
static uint8_t *
xpm_convert_img_row(uint8_t * img, uint8_t * buf, int ww, int ch, int scale, int *buf_len)
{
        uint8_t * startp = img;
        int row;
        int col;

        if (scale == 0)
                ch /= 2;
        else if (scale == 2)
                ch *= 2;

        if (*buf_len < ch * (ww + 4)) {
                *buf_len = 0;
                return img;
        }

        for (row = 0; row < ch; row++) {
                *(img++) = '"';

                for (col = 0; col < ww; col++) {
                        uint8_t c = *(buf++);
                        if (c < sizeof(xpm_col_codes)) {
                                *(img++) = xpm_col_codes[c];
                        } else {
                                *(img++) = '.';  /* transparent */
                        }
                }
                *(img++) = '"';
                *(img++) = ',';
                *(img++) = '\n';

                if (scale == 0) {
                        /* scale down - use every 2nd row */
                        buf += ww;
                } else if (scale == 2) {
                        /* scale up - double each line */
                        if ((row & 1) == 0)
                                buf -= ww;
                }
        }
        *buf_len -= (img - startp);

        return img;
}

static vbi_bool
xpm_export(vbi_export *e, FILE *fp, vbi_page *pg)
{
	gfx_instance *gfx = PARENT(e, gfx_instance, export);
	uint8_t *image = NULL;
        uint8_t *row_buf = NULL;
        uint8_t *canvas;
        uint8_t pen[128];
        char title[80];
	int ch, ww, wh, scale, woh;
        int img_size;
	int row;
	int i;
        vbi_bool result = FALSE;

	if (pg->columns < 40) /* caption */ {
		/* characters are already line-doubled */
		scale = gfx->double_height ? 1 : 0;
                ch = CCH;
	        ww = CCW * pg->columns;
	        wh = CCH * pg->rows;
	} else {
		scale = gfx->double_height ? 2 : 1;
                ch = TCH;
	        ww = TCW * pg->columns;
	        wh = TCH * pg->rows;
	}

        switch (scale) {
                case 0:  woh = wh / 2; break;
                case 2:  woh = wh * 2; break;
                default: woh = wh; break;
        }
        get_image_title(e, pg, title, sizeof(title));
        img_size = xpm_calc_img_buf_size(ww, woh, title, e->creator);

	image = canvas = malloc(img_size);
        ABORT_ON_MALLOC_FAILURE(e, image, img_size);

        row_buf = malloc(ww * ch);
        ABORT_ON_MALLOC_FAILURE(e, row_buf, ww * ch);

        if (pg->drcs_clut) {
                for (i = 2; i < 2 + 8 + 32; i++) {
                        pen[i]      = pg->drcs_clut[i]; /* opaque */
                        pen[i + 64] = 40; /* translucent */
                }
        }

        canvas = xpm_write_img_header(pg, image, ww, woh, title, e->creator, &img_size);

        for (row = 0; row < pg->rows; row++) {
                draw_row_indexed(pg, &pg->text[row * pg->columns],
                                 row_buf, pen, ww,
                                 !e->reveal, pg->columns < 40, scale);

                canvas = xpm_convert_img_row(canvas, row_buf, ww, ch, scale, &img_size);
        }

        canvas = xpm_write_img_footer(canvas, title, e->creator, &img_size);

        if (img_size <= 0) {
		vbi_export_error_printf(e, _("Internal error: XPM output buffer overflow.\n"
                                             "(Please report: Image size: %dx%d + EXT %d)"),
                                             ww, woh, strlen(title) + strlen(e->creator));
                goto abort;
        }

        img_size = (canvas - image) * sizeof(uint8_t);

        if (fwrite(image, 1, img_size, fp) < img_size) {
	        vbi_export_write_error(e);
                goto abort;
        }

        result = TRUE;

abort:
	if (row_buf != NULL) {
	        free(row_buf);
        }
	if (image != NULL) {
	        free(image);
        }

	return result;
}

static vbi_export_info
info_xpm = {
	.keyword	= "xpm",
	.label		= N_("XPM"),
	.tooltip	= N_("Export this page as XPM image"),

	.mime_type	= "image/xpm",
	.extension	= "xpm",
};

vbi_export_class
vbi_export_class_xpm = {
	._public	= &info_xpm,
	._new		= gfx_new,
	._delete	= gfx_delete,
	.option_enum	= option_enum,
	.option_get	= option_get,
	.option_set	= option_set,
	.export		= xpm_export
};

VBI_AUTOREG_EXPORT_MODULE(vbi_export_class_xpm)


/*
 *  PNG - Portable Network Graphics File
 */
#ifdef HAVE_LIBPNG

#include "png.h"
#include "setjmp.h"

static vbi_bool
png_export(vbi_export *e, FILE *fp, vbi_page *pg)
{
	gfx_instance *gfx = PARENT(e, gfx_instance, export);
	png_structp png_ptr;
	png_infop info_ptr;
	png_color palette[80];
	png_byte alpha[80];
	png_text text[4];
        uint8_t pen[128];
	char title[80];
	png_bytep *row_pointer;
	png_bytep image;
	int ww, wh, rowstride, row_adv, scale;
	int row;
	int i;

	assert((sizeof(png_byte) == sizeof(uint8_t)) && (sizeof(*image) == sizeof(uint8_t)));

	if (pg->columns < 40) /* caption */ {
		/* characters are already line-doubled */
		scale = !!gfx->double_height;
	        ww = CCW * pg->columns;
	        wh = CCH * pg->rows;
		row_adv = pg->columns * CCW * CCH;
	} else {
		scale = 1 + !!gfx->double_height;
	        ww = TCW * pg->columns;
	        wh = TCH * pg->rows;
		row_adv = pg->columns * TCW * TCH;
	}

	rowstride = ww * sizeof(*image);

	if (!(row_pointer = malloc(sizeof(*row_pointer) * wh * 2))) {
		vbi_export_error_printf(e, _("Unable to allocate %d byte buffer."),
					sizeof(*row_pointer) * wh * 2);
		return FALSE;
	}

	if (!(image = malloc(wh * ww * sizeof(*image)))) {
		vbi_export_error_printf(e, _("Unable to allocate %d KB image buffer."),
					wh * ww * sizeof(*image) / 1024);
		free(row_pointer);
		return FALSE;
	}

        /* draw the image */

        if (pg->drcs_clut) {
                for (i = 2; i < 2 + 8 + 32; i++) {
                        pen[i]      = pg->drcs_clut[i]; /* opaque */
                        pen[i + 64] = pg->drcs_clut[i] + 40; /* translucent */
                }
        }

        for (row = 0; row < pg->rows; row++) {
                draw_row_indexed(pg, &pg->text[row * pg->columns],
                                 image + row * row_adv,  pen, rowstride,
                                 !e->reveal, pg->columns < 40, scale);
	}

	/* Now save the image */

	if (!(png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)))
		goto unknown_error;

	if (!(info_ptr = png_create_info_struct(png_ptr))) {
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		goto unknown_error;
	}

	/* avoid possible longjmp breakage due to libpng ugliness */
	/* XXX not portable, to be removed */
	{ int do_write() {
	if (setjmp(png_ptr->jmpbuf))
		return 1;

	png_init_io(png_ptr, fp);

	png_set_IHDR(png_ptr, info_ptr, ww, (wh << scale) >> 1,
		8 /* bit_depth */,
		PNG_COLOR_TYPE_PALETTE,
		(gfx->double_height) ?
			PNG_INTERLACE_ADAM7 : PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);

	/* Could be optimized (or does libpng?) */
	for (i = 0; i < 40; i++) {
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

	png_set_PLTE(png_ptr, info_ptr, palette, 80);
	png_set_tRNS(png_ptr, info_ptr, alpha, 80, NULL);

	png_set_gAMA(png_ptr, info_ptr, 1.0 / 2.2);

        get_image_title(e, pg, title, sizeof(title));

	memset(text, 0, sizeof(text));

	text[0].key = "Title";
	text[0].text = title;
	text[0].compression = PNG_TEXT_COMPRESSION_NONE;
	text[1].key = "Software";
	text[1].text = e->creator;
	text[1].compression = PNG_TEXT_COMPRESSION_NONE;

	png_set_text(png_ptr, info_ptr, text, 2);

	png_write_info(png_ptr, info_ptr);

	switch (scale) {
	case 0:
		for (i = 0; i < wh / 2; i++)
			row_pointer[i] = image + i * 2 * ww;
		break;

	case 1:
		for (i = 0; i < wh; i++)
			row_pointer[i] = image + i * ww;
		break;

	case 2:
		for (i = 0; i < wh; i++)
			row_pointer[i * 2 + 0] =
			row_pointer[i * 2 + 1] = image + i * ww;
		break;
	}

	png_write_image(png_ptr, row_pointer);

	png_write_end(png_ptr, info_ptr);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	return 0;

	} if (do_write()) goto write_error; }

abort:
	free(row_pointer);
	row_pointer = NULL;

	free(image);
	image = NULL;

	return TRUE;

write_error:
	vbi_export_write_error(e);

unknown_error:
	if (row_pointer)
		free(row_pointer);

	if (image)
		free(image);

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

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
