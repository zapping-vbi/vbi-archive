/*
 *  Zapzilla - Teletext graphical rendering and export functions
 *
 *  Copyright (C) 2000-2001 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998,1999 Edgar Toernig (froese@gmx.de)
 *  Copyright 1999 by Paul Ortyl <ortylp@from.pl>
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

/* $Id: exp-gfx.c,v 1.19 2001-01-30 23:27:16 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lang.h"
#include "export.h"

#include "wstfont.xbm"

#undef WW
#undef WH
#undef CW
#undef CH

#define CW 12		/* character cell dimensions - hardcoded (DRCS) */
#define CH 10
#define WW (W*CW)
#define WH (H*CH)

#if 0

static void init_gfx(void) __attribute__ ((constructor));

static void
init_gfx(void)
{
	unsigned char *buf, *p;
	int i, j;

	/* de-interleave font image (puts all chars in row 0) */
	/* could load font image here rather than inline */
	/* XXX need blank line #10 for c & (1 << 11) or diacr bitmap */

	if (!(buf = malloc(wstfont_width * wstfont_height / 8)))
		exit(EXIT_FAILURE);
	p = buf;

	for (i = 0; i < CH; i++)
		for (j = 0; j < wstfont_height; p += wstfont_width / 8, j += CH)
			memcpy(p, wstfont_bits + (j + i) * wstfont_width / 8,
				wstfont_width / 8);

	memcpy(wstfont_bits, buf, wstfont_width * wstfont_height / 8);

	free(buf);
}

#endif

#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))

///////////////////////////////////////////////////////
// COMMON ROUTINES FOR PPM AND PNG

#if CW < 9 || CW > 16
#error CW out of range
#endif

static inline void
draw_char(unsigned int *canvas, unsigned int *pen, int glyph,
	int bold, int underline, glyph_size size)
{
	unsigned char *src1, *src2;
	int shift1, shift2;
	int x, y, base = 0; // XXX italic ? GL_ITALICS : 0;
	int ch = CH;
#if 1
	x = (glyph & 31) * CW;
	shift1 = x & 7;
	src1 = wstfont_bits + ((glyph & 0x03FF) >> 5) * CH * CW * 32 / 8 + (x >> 3);

	x = (glyph >> 20) * CW;
	shift2 = (x & 7) + ((glyph >> 18) & 1);
	src2 = wstfont_bits + ((base + 0xC0) >> 5) * CH * CW * 32 / 8 + (x >> 3);
	if (glyph & 0x080000) src2 += CW * 32 / 8;
#else
	x = (glyph & 0xFFFF) * CW;
	shift1 = x & 7;
	src1 = wstfont_bits + (x >> 3);

	x = (base + 0xC0 + (glyph >> 20)) * CW;
	shift2 = (x & 7) + ((glyph >> 18) & 1);
	src2 = wstfont_bits + (x >> 3);
	if (glyph & (1 << 19))
		src2 += wstfont_width * (wstfont_height / CH) / 8;
#endif

	for (y = 0; y < ch; y++) {
#if #cpu (i386)
		int bits = (*((u16 *) src1) >> shift1) | (*((u16 *) src2) >> shift2);
#else
		int bits = ((src1[1] * 256 + src1[0]) >> shift1)
			 | ((src2[1] * 256 + src2[0]) >> shift2); /* unaligned/little endian */
#endif
		if (bold)
			bits |= bits << 1;

		if (underline && y == 9)
			bits |= 0x0FFF;

		switch (size) {
		case NORMAL:
			for (x = 0; x < CW; bits >>= 1, x++)
				canvas[x] = pen[bits & 1];

			canvas += WW;

			break;

		case DOUBLE_HEIGHT:
			for (x = 0; x < CW; bits >>= 1, x++) {
				unsigned int col = pen[bits & 1];

				canvas[x] = col;
				canvas[x + WW] = col;
			}

			canvas += 2 * WW;

			break;

		case DOUBLE_WIDTH:
			for (x = 0; x < CW * 2; bits >>= 1, x += 2) {
				unsigned int col = pen[bits & 1];

				canvas[x + 0] = col;
				canvas[x + 1] = col;
			}

			canvas += WW;

			break;

		case DOUBLE_SIZE:
			for (x = 0; x < CW * 2; bits >>= 1, x += 2) {
				unsigned int col = pen[bits & 1];

				canvas[x + 0] = col;
				canvas[x + 1] = col;
				canvas[x + WW + 0] = col;
				canvas[x + WW + 1] = col;
			}

			canvas += 2 * WW;

			break;

		default:
			break;
		}
#if 1
		src1 += CW * 32 / 8;
		src2 += CW * 32 / 8;
#else
		src1 += wstfont_width * (wstfont_height / CH) / 8;
		src2 += wstfont_width * (wstfont_height / CH) / 8;
#endif
	}
}

static inline void
draw_drcs(unsigned int *canvas, unsigned char *src, unsigned int *pen, int glyph, glyph_size size)
{
	unsigned int col;
	int x, y;

	src += (glyph & 0x3F) * 60;
	pen += (glyph >> 16);

	switch (size) {
	case NORMAL:
		for (y = 0; y < CH; canvas += WW, y++)
			for (x = 0; x < 12; src++, x += 2) {
				canvas[x + 0] = pen[*src & 15];
				canvas[x + 1] = pen[*src >> 4];
			}
		break;

	case DOUBLE_HEIGHT:
		for (y = 0; y < CH; canvas += 2 * WW, y++)
			for (x = 0; x < 12; src++, x += 2) {
				col = pen[*src & 15];
				canvas[x + 0] = col;
				canvas[x + WW + 0] = col;

				col = pen[*src >> 4];
				canvas[x + 1] = col;
				canvas[x + WW + 1] = col;
			}
		break;

	case DOUBLE_WIDTH:
		for (y = 0; y < CH; y++)
			for (x = 0; x < 12 * 2; src++, x += 4) {
				col = pen[*src & 15];
				canvas[x + 0] = col;
				canvas[x + 1] = col;

				col = pen[*src >> 4];
				canvas[x + 2] = col;
				canvas[x + 3] = col;
			}
		break;

	case DOUBLE_SIZE:
		for (y = 0; y < CH; canvas += 2 * WW, y++)
			for (x = 0; x < 12 * 2; src++, x += 4) {
				col = pen[*src & 15];
				canvas[x + 0] = col;
				canvas[x + 1] = col;
				canvas[x + WW + 0] = col;
				canvas[x + WW + 1] = col;

				col = pen[*src >> 4];
				canvas[x + 2] = col;
				canvas[x + 3] = col;
				canvas[x + WW + 2] = col;
				canvas[x + WW + 3] = col;
			}
	default:
		break;
	}
}

static inline void
draw_blank(unsigned int *canvas, unsigned int colour)
{
	int x, y;

	for (y = 0; y < CH; y++) {
		for (x = 0; x < CW; x++)
			canvas[x] = colour;

		canvas += WW;
	}
}


void
vbi_draw_page(struct fmt_page *pg, void *data)
{
	unsigned int pen[64];
	int row, column;
	attr_char *ac;
	int i;
	unsigned int *canvas = (unsigned int*)data;

	for (i = 2; i < 2 + 8 + 32; i++)
		pen[i] = pg->colour_map[pg->drcs_clut[i]];

	for (row = 0; row < H; canvas += W * CW * CH - W * CW, row++) {
		for (column = 0; column < W; canvas += CW, column++) {
			ac = &pg->data[row][column];

			pen[0] = pg->colour_map[ac->background];
			pen[1] = pg->colour_map[ac->foreground];

			if (ac->size <= DOUBLE_SIZE) {
				if ((ac->glyph & 0xFFFF) >= GL_DRCS) {
					draw_drcs(canvas, pg->drcs[(ac->glyph & 0x1F00) >> 8],
						pen, ac->glyph, ac->size);
				} else {
					draw_char(canvas, pen, ac->glyph,
						ac->bold, ac->underline, ac->size);
				}
			}
		}
	}
}

void
vbi_draw_page_indexed(struct fmt_page *pg, void *data)
{
	unsigned int pen[64];
	int row, column;
	attr_char *ac;
	int i;
	unsigned int *canvas = (unsigned int*)data;

	for (i = 2; i < 2 + 8 + 32; i++)
		pen[i] = pg->drcs_clut[i];

	for (row = 0; row < H; canvas += W * CW * CH - W * CW, row++) {
		for (column = 0; column < W; canvas += CW, column++) {
			ac = &pg->data[row][column];

			pen[0] = ac->background;
			pen[1] = ac->foreground;

			if (ac->size <= DOUBLE_SIZE) {
				if ((ac->glyph & 0xFFFF) >= GL_DRCS) {
					draw_drcs(canvas, pg->drcs[(ac->glyph & 0x1F00) >> 8],
						pen, ac->glyph, ac->size);
				} else
					draw_char(canvas, pen, ac->glyph,
						ac->bold, ac->underline, ac->size);
			}
		}
	}
}

/*
 *  PPM - Portable Pixmap File (raw)
 */

static int
ppm_output(struct export *e, char *name, struct fmt_page *pg)
{
	unsigned int *image;
	unsigned char *body;
	FILE *fp;
	int i;

	if (!(image = malloc(WH * WW * sizeof(*image)))) {
		export_error("cannot allocate memory");
		return 0;
	}

	vbi_draw_page(pg, image);

	if (!(fp = fopen(name, "w"))) {
		free(image);
		export_error("cannot create file");
		return -1;
	}

	fprintf(fp, "P6 %d %d 15\n", WW, WH);

	body = (unsigned char *) image;

	for (i = 0; i < WH * WW; body += 3, i++) {
		unsigned int n = (image[i] >> 4) & 0x0F0F0F;

		body[0] = n;
		body[1] = n >> 8;
		body[2] = n >> 16;
	}

	if (!fwrite(image, WH * WW * 3, 1, fp)) {
		export_error("error while writting to file");
		free(image);
		fclose(fp);
		return -1;
	}

	free(image);

	if (fclose(fp)) {
		export_error("cannot close file");
		return -1;
	}

	return 0;
}

struct export_module export_ppm[1] =	// exported module definition
{
  {
    "ppm",			// id
    "ppm",			// extension
    0,				// options
    0,				// size
    0,				// open
    0,				// close
    0,				// option
    ppm_output			// output
  }
};

/*
 *  PNG - Portable Network Graphics File
 */

#ifdef HAVE_LIBPNG

#include "png.h"
#include "setjmp.h"

static int
png_output(struct export *e, char *name, struct fmt_page *pg)
{
	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;
	png_color palette[80];
	png_byte alpha[80];
	png_text text[4];
	char title[80];
	png_bytep row_pointer[WH];
	unsigned int *image;
	int i;

	if ((image = malloc(WH * WW * sizeof(*image)))) {
		png_bytep body = (png_bytep) image;
		unsigned int *canvas = image;
		unsigned int pen[128];
		int row, column;
		attr_char *ac;

		for (i = 2; i < 2 + 8 + 32; i++) {
			pen[i]      = pg->drcs_clut[i];
			pen[i + 64] = pg->drcs_clut[i] + 40;
		}

		for (row = 0; row < H; canvas += W * CW * CH - W * CW, row++) {
			for (column = 0; column < W; canvas += CW, column++) {
				ac = &pg->data[row][column];

				if (ac->size > DOUBLE_SIZE)
					continue;

				switch (ac->opacity) {
				case TRANSPARENT_SPACE:
					/*
					 *  Transparent foreground and background.
					 */
					draw_blank(canvas, TRANSPARENT_BLACK);
					break;

				case TRANSPARENT:
					/*
					 *  Transparent background, opaque foreground. Currently not used.
					 *  Mind Teletext level 2.5 foreground and background transparency
					 *  by referencing colourmap entry 8, TRANSPARENT_BLACK.
					 *  The background of multicolour DRCS is ambiguous, so we make
					 *  them opaque.
					 */
					if ((ac->glyph & 0xFFFF) >= GL_DRCS) {
						pen[0] = TRANSPARENT_BLACK;
						pen[1] = ac->foreground;

						draw_drcs(canvas, pg->drcs[(ac->glyph & 0x1F00) >> 8],
							pen, ac->glyph, ac->size);
					} else {
						pen[0] = TRANSPARENT_BLACK;
						pen[1] = ac->foreground;

						draw_char(canvas, pen, ac->glyph,
							ac->bold, ac->underline, ac->size);
					}

					break;

				case SEMI_TRANSPARENT:
					/*
					 *  Translucent background (for 'boxed' text), opaque foreground.
					 *  The background of multicolour DRCS is ambiguous, so we make
					 *  them completely translucent. 
					 */
					if ((ac->glyph & 0xFFFF) >= GL_DRCS) {
						pen[64] = ac->background + 40;
						pen[65] = ac->foreground;

						draw_drcs(canvas, pg->drcs[(ac->glyph & 0x1F00) >> 8],
							pen + 64, ac->glyph, ac->size);
					} else {
						pen[0] = ac->background + 40;
						pen[1] = ac->foreground;

						draw_char(canvas, pen, ac->glyph,
							ac->bold, ac->underline, ac->size);
					}

					break;

				case OPAQUE:
					pen[0] = ac->background;
					pen[1] = ac->foreground;

					if ((ac->glyph & 0xFFFF) >= GL_DRCS) {
						draw_drcs(canvas, pg->drcs[(ac->glyph & 0x1F00) >> 8],
							pen, ac->glyph, ac->size);
					} else {
						draw_char(canvas, pen, ac->glyph,
							ac->bold, ac->underline, ac->size);
					}
					break;
				}
			}
		}

		/* XXX poor */
		for (i = 0; i < WH * WW; i++) {
			*body++ = image[i];
		}
	} else {
		export_error("cannot allocate memory");
		return 0;
	}

	if (!(fp = fopen(name, "wb"))) {
		export_error("cannot create file");
		free(image);
		return -1;
	}

	if (!(png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) {
		fclose(fp);
		free(image);
		return -1;
	}

	if (!(info_ptr = png_create_info_struct(png_ptr))) {
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		fclose(fp);
		free(image);
		return -1;
	}

	if (setjmp(png_ptr->jmpbuf)) {
		/* If we get here, we had a problem writing the file */
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fp);
		free(image);
		return -1;
	}

	png_init_io(png_ptr, fp);

	png_set_IHDR(png_ptr, info_ptr, WW, WH,
		8 /* bit_depth */,
		PNG_COLOR_TYPE_PALETTE,
		PNG_INTERLACE_NONE,
	     /* PNG_INTERLACE_ADAM7,   pretty much useless, impossible to read anything until pass 7 */
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);

	/* Could be optimized (or does libpng?) */
	for (i = 0; i < 40; i++) {
		palette[i].red   = pg->colour_map[i] & 0xFF;
		palette[i].green = (pg->colour_map[i] >> 8) & 0xFF;
		palette[i].blue	 = (pg->colour_map[i] >> 16) & 0xFF;
		alpha[i]	 = 255;

		palette[i + 40]  = palette[i];
		alpha[i + 40]	 = 128;
	}

	alpha[8] = alpha[8 + 40] = 0; /* TRANSPARENT_BLACK */

	png_set_PLTE(png_ptr, info_ptr, palette, 80);
	png_set_tRNS(png_ptr, info_ptr, alpha, 80, NULL);

	png_set_gAMA(png_ptr, info_ptr, 1.0 / 2.2);

	/*
	 *  ISO 8859-1 (Latin-1) character set required,
	 *  see png spec for other
	 */
	memset(text, 0, sizeof(text));
	snprintf(title, sizeof(title) - 1,
		"Teletext Page %3x/%04x",
		pg->vtp->pgno, pg->vtp->subno); // XXX make it "station_short Teletext ..."

	text[0].key = "Title";
	text[0].text = title;
	text[0].compression = PNG_TEXT_COMPRESSION_NONE;
	text[1].key = "Software";
	text[1].text = "Zapzilla " VERSION;
	text[1].compression = PNG_TEXT_COMPRESSION_NONE;

	png_set_text(png_ptr, info_ptr, text, 2);

	png_write_info(png_ptr, info_ptr);

	for (i = 0; i < WH; i++)
		row_pointer[i] = ((png_bytep) image) + i * WW;

	png_write_image(png_ptr, row_pointer);

	png_write_end(png_ptr, info_ptr);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	fclose(fp);

	free(image);

	return 0;
}

struct export_module export_png[1] =	// exported module definition
{
  {
    "png",			// id
    "png",			// extension
    0,				// options
    0,				// size
    0,				// open
    0,				// close
    0,				// option
    png_output			// output
  }
};

#endif /* HAVE_LIBPNG */


/* We could just export WW and WH too.. */
void vbi_get_rendered_size(int *w, int *h)
{
  if (w)
    *w = WW;
  if (h)
    *h = WH;
}
