/*
 *  Zapzilla - Closed Caption and Teletext graphical
 *             rendering and export functions
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

/* $Id: exp-gfx.c,v 1.33 2001-06-23 02:50:44 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <errno.h>

#include <sys/stat.h>
#include <unistd.h>

#include "lang.h"
#include "export.h"

#include "wstfont.xbm"
#include "ccfont.xbm"

#define TRANSPARENT_BLACK 8

/* Character cell dimensions - hardcoded (DRCS) */

#define W               40
#define H               25
#define CW		12		
#define CH		10
#define WW		(W * CW)
#define WH		(H * CH)

#define CPL		(wstfont_width / CW * wstfont_height / CH)

static uint8_t *fimg;
static void init_gfx(void) __attribute__ ((constructor));

static void
init_gfx(void)
{
	uint8_t *t;
	uint8_t *p;
	int i, j;

	/* de-interleave font image (puts all chars in row 0) */

	if (!(fimg = calloc(CPL * CW * (CH + 1) / 8, 1)))
		exit(EXIT_FAILURE);

	p = fimg;

	for (i = 0; i < CH; i++)
		for (j = 0; j < wstfont_height; p += wstfont_width / 8, j += CH)
			memcpy(p, wstfont_bits + (j + i) * wstfont_width / 8,
				wstfont_width / 8);

	if (!(t = malloc(ccfont_width * ccfont_height / 8)))
		exit(EXIT_FAILURE);

	p = t;

	for (i = 0; i < 26; i++)
		for (j = 0; j < ccfont_height; p += ccfont_width / 8, j += 26)
			memcpy(p, ccfont_bits + (j + i) * ccfont_width / 8,
				ccfont_width / 8);

	memcpy(ccfont_bits, t, ccfont_width * ccfont_height / 8);

	free(t);
}

#define peek(p, i)							\
((canvas_type == sizeof(uint8_t)) ? ((uint8_t *)(p))[i] :		\
    ((canvas_type == sizeof(uint16_t)) ? ((uint16_t *)(p))[i] :		\
	((uint32_t *)(p))[i]))

#define poke(p, i, v)							\
((canvas_type == sizeof(uint8_t)) ? (((uint8_t *)(p))[i] = (v)) :	\
    ((canvas_type == sizeof(uint16_t)) ? (((uint16_t *)(p))[i] = (v)) :	\
	(((uint32_t *)(p))[i] = (v))))

/*
 *  draw_char template
 */
static inline void
draw_char(int canvas_type, uint8_t *canvas, unsigned int rowstride,
	uint8_t *pen, uint8_t *font, int cpl, int cw, int ch,
	int glyph, int bold, int italic, unsigned int underline, attr_size size)
{
	uint8_t *src1, *src2;
	int g = glyph & 0xFFFF;
	int shift1, shift2;
	int x, y, base = 0;

	/* bold = !!bold; */
	/* assert(cw >= 8 && cw <= 16); */
	/* assert(ch >= 1 && cw <= 31); */

	if (italic && g < 0x200)
		base = GL_ITALICS;

	x = (base + g) * cw;
	shift1 = x & 7;
	src1 = font + (x >> 3);

	x = (base + (glyph >> 20)) * cw;
	shift2 = (x & 7) + ((glyph >> 18) & 1);
	src2 = font + (x >> 3);

	if (glyph & (1 << 19))
		src2 += cpl * cw / 8;

	switch (size) {
	case DOUBLE_HEIGHT2:
	case DOUBLE_SIZE2:
		src1 += cpl * cw / 8 * ch / 2;
		src2 += cpl * cw / 8 * ch / 2;
		underline >>= ch / 2;

	case DOUBLE_HEIGHT:
	case DOUBLE_SIZE: 
		ch >>= 1;

	default:
		break;
	}

	for (y = 0; y < ch; underline >>= 1, y++) {
		int bits = ~0;

		if (!(underline & 1)) {
#if #cpu (i386)
			bits = (*((uint16_t *) src1) >> shift1)
		    	     | (*((uint16_t *) src2) >> shift2);
#else
			bits = ((src1[1] * 256 + src1[0]) >> shift1)
			     | ((src2[1] * 256 + src2[0]) >> shift2); /* unaligned/little endian */
#endif
			bits |= bits << bold;
		}

		switch (size) {
		case NORMAL:
			for (x = 0; x < cw; bits >>= 1, x++)
				poke(canvas, x, peek(pen, bits & 1));

			canvas += rowstride;

			break;

		case DOUBLE_HEIGHT:
		case DOUBLE_HEIGHT2:
			for (x = 0; x < cw; bits >>= 1, x++) {
				unsigned int col = peek(pen, bits & 1);

				poke(canvas, x, col);
				poke(canvas, x + rowstride / canvas_type, col);
			}

			canvas += rowstride * 2;

			break;

		case DOUBLE_WIDTH:
			for (x = 0; x < cw * 2; bits >>= 1, x += 2) {
				unsigned int col = peek(pen, bits & 1);

				poke(canvas, x + 0, col);
				poke(canvas, x + 1, col);
			}

			canvas += rowstride;

			break;

		case DOUBLE_SIZE:
		case DOUBLE_SIZE2:
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

		src1 += cpl * cw / 8;
		src2 += cpl * cw / 8;
	}
}

static inline void
draw_drcs(int canvas_type, uint8_t *canvas, uint8_t *pen,
	uint8_t *src, int glyph, attr_size size, unsigned int rowstride)
{
	unsigned int col;
	int x, y;

	src += (glyph & 0x3F) * 60;
	pen += (glyph >> 16) * canvas_type;

	switch (size) {
	case NORMAL:
		for (y = 0; y < CH; canvas += rowstride, y++)
			for (x = 0; x < 12; src++, x += 2) {
				poke(canvas, x + 0, peek(pen, *src & 15));
				poke(canvas, x + 1, peek(pen, *src >> 4));
			}
		break;

	case DOUBLE_HEIGHT2:
		src += 30;

	case DOUBLE_HEIGHT:
		for (y = 0; y < CH / 2; canvas += rowstride * 2, y++)
			for (x = 0; x < 12; src++, x += 2) {
				col = peek(pen, *src & 15);
				poke(canvas, x + 0, col);
				poke(canvas, x + rowstride / canvas_type + 0, col);

				col = peek(pen, *src >> 4);
				poke(canvas, x + 1, col);
				poke(canvas, x + rowstride / canvas_type + 1, col);
			}
		break;

	case DOUBLE_WIDTH:
		for (y = 0; y < CH; canvas += rowstride, y++)
			for (x = 0; x < 12 * 2; src++, x += 4) {
				col = peek(pen, *src & 15);
				poke(canvas, x + 0, col);
				poke(canvas, x + 1, col);

				col = peek(pen, *src >> 4);
				poke(canvas, x + 2, col);
				poke(canvas, x + 3, col);
			}
		break;

	case DOUBLE_SIZE2:
		src += 30;

	case DOUBLE_SIZE:
		for (y = 0; y < CH / 2; canvas += rowstride * 2, y++)
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

static inline void
draw_blank(int canvas_type, uint8_t *canvas,
	unsigned int colour, unsigned int rowstride)
{
	int x, y;

	for (y = 0; y < CH; y++) {
		for (x = 0; x < CW; x++)
			poke(canvas, x, colour);

		canvas += rowstride;
	}
}

void
vbi_draw_cc_page_region(struct fmt_page *pg, uint32_t *canvas,
	int column, int row, int width, int height, unsigned int rowstride)
{
	uint32_t pen[2];
	int count, row_adv;
	attr_char *ac;

	if (rowstride == -1)
		rowstride = pg->columns * 16 * sizeof(*canvas);

	row_adv = rowstride * 26 - width * 16 * sizeof(*canvas);

	for (; height > 0; height--, row++) {
		ac = &pg->text[row * pg->columns + column];

		for (count = width; count > 0; count--, ac++) {
			pen[0] = pg->colour_map[ac->background];
			pen[1] = pg->colour_map[ac->foreground];

			draw_char(sizeof(*canvas), (uint8_t *) canvas, rowstride,
				(uint8_t *) pen, ccfont_bits, 256, 16, 26,
				ac->glyph & 0xFF, 0 /* bold */, 0 /* italic, coded in glyph */,
				ac->underline * (3 << 24) /* cell row 24, 25 */, NORMAL /* size */);

			canvas += 16;
		}

		canvas += row_adv / sizeof(*canvas);
	}
}

void
vbi_draw_vt_page_region(struct fmt_page *pg, uint32_t *canvas,
	int column, int row, int width, int height, unsigned int rowstride,
	int reveal, int flash_on)
{
	uint32_t pen[64];
	int count, row_adv;
	int conceal, glyph;
	attr_char *ac;
	int i;

	if (rowstride == -1)
		rowstride = pg->columns * 12 * sizeof(*canvas);

	row_adv = rowstride * 10 - width * 12 * sizeof(*canvas);

	conceal = !reveal;

	for (i = 2; i < 2 + 8 + 32; i++)
		pen[i] = pg->colour_map[pg->drcs_clut[i]];

	for (; height > 0; height--, row++) {
		ac = &pg->text[row * pg->columns + column];

		for (count = width; count > 0; count--, ac++) {
			glyph = ((ac->conceal & conceal) || !flash_on) ?
				GL_SPACE : ac->glyph;

			pen[0] = pg->colour_map[ac->background];
			pen[1] = pg->colour_map[ac->foreground];

			switch (ac->size) {
			case OVER_TOP:
			case OVER_BOTTOM:
				break;

			default:
				if ((glyph & 0xFFFF) >= GL_DRCS) {
					draw_drcs(sizeof(*canvas), (uint8_t *) canvas,
						(uint8_t *) pen, pg->drcs[(glyph & 0x1F00) >> 8],
						glyph, ac->size, rowstride);
				} else {
					draw_char(sizeof(*canvas), (uint8_t *) canvas, rowstride,
						(uint8_t *) pen, fimg, CPL, CW, CH,
						glyph + 0xC000000, ac->bold, ac->italic,
						ac->underline << 9 /* cell row 9 */, ac->size);
				}
			}

			canvas += CW;
		}

		canvas += row_adv / sizeof(*canvas);
	}
}

/* XXX */
/* We could just export WW and WH too.. */
void vbi_get_rendered_size(int *w, int *h)
{
  if (w)
    *w = WW;
  if (h)
    *h = WH;
}

/*
 *  Shared export options
 */

typedef struct
{
	int	double_height;
	/*
	 *  The raw image contains the same information a real TV
	 *  would show, however a TV overlays the image on both fields.
	 *  So raw pixel aspect is 2:1, and this option will double
	 *  lines adding redundant information. The resulting images
	 *  with pixel aspect 2:2 are still too narrow compared to a
	 *  real TV closer to 4:3 (11 MHz TXT pixel clock), but I {mhs}
	 *  think one should export raw, not scaled data.
	 */
} gfx_data;

static bool
gfx_set_option(vbi_export *e, int opt, char *str_arg, int num_arg)
{
	gfx_data *d = (gfx_data *) e->data;

	switch (opt) {
	case 0: // aspect
		d->double_height = !!num_arg;
		break;
	}

	return TRUE;
}

static vbi_export_option gfx_opts[] = {
	{
		VBI_EXPORT_BOOL,	"aspect",	N_("Correct aspect ratio"),
		{ .num = TRUE }, FALSE, TRUE, NULL, N_("Approach an image aspect ratio similar to a real TV, this will add redundant information")
	}, {
		0
	}
};

/*
 *  PPM - Portable Pixmap File (raw)
 */

static bool
ppm_output(vbi_export *e, FILE *fp, char *name, struct fmt_page *pg)
{
	gfx_data *d = (gfx_data *) e->data;
	uint32_t *image;
	uint8_t *body;
	int cw, ch, size, scale;
	struct stat st;
	int i;

	if (pg->columns < 40) {
		cw = 16;
		ch = 26;
		scale = !!d->double_height;
	} else {
		cw = 12;
		ch = 10;
		scale = 1 + !!d->double_height;
	}

	size = cw * pg->columns * ch * pg->rows;

	if (!(image = malloc(size * sizeof(*image)))) {
		vbi_export_error(e, _("Unable to allocate %d KB image buffer"),
			size * sizeof(*image) / 1024);
		return FALSE;
	}

	if (pg->columns < 40)
		vbi_draw_cc_page_region(pg, image, 0, 0, pg->columns, pg->rows, -1);
	else
		vbi_draw_vt_page_region(pg, image, 0, 0, pg->columns, pg->rows, -1,
			!e->reveal, 1 /* flash_on */);

	if (name && !(fp = fopen(name, "wb"))) {
		vbi_export_error(e, _("Cannot create file '%s': %s"), name, strerror(errno));
		free(image);
		return FALSE;
	}

	fprintf(fp, "P6 %d %d 15\n",
		cw * pg->columns, (ch * pg->rows / 2) << scale);

	if (ferror(fp))
		goto write_error;

	body = (uint8_t *) image;

	if (scale == 0) {
		unsigned int n;
		int stride = cw * pg->columns;

		for (i = 0; i < size; body += 3, i++) {
			n = (((image[i] & 0xF0F0F0) +
			      (image[i + stride] & 0xF0F0F0) +
			      0x101010) >> 5) & 0x0F0F0F;

			body[0] = n;
			body[1] = n >> 8;
			body[2] = n >> 16;
		}
	} else
		for (i = 0; i < size; body += 3, i++) {
			unsigned int n = (image[i] >> 4) & 0x0F0F0F;

			body[0] = n;
			body[1] = n >> 8;
			body[2] = n >> 16;
		}

	switch (scale) {
		int rows, stride;

	case 0:
		body = (uint8_t *) image;
		rows = ch * pg->rows / 2;
		stride = cw * pg->columns * 3;

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
		rows = ch * pg->rows;
		stride = cw * pg->columns * 3;

		for (i = 0; i < rows; body += stride, i++) {
			if (!fwrite(body, stride, 1, fp))
				goto write_error;
			if (!fwrite(body, stride, 1, fp))
				goto write_error;
		}

		break;
	}

	free(image);
	image = NULL;

	if (name && fclose(fp)) {
		fp = NULL;
		goto write_error;
	}

	return TRUE;

write_error:
	vbi_export_write_error(e, name);

	if (image)
		free(image);

	if (name) {
		if (fp)
			fclose(fp);

		if (!stat(name, &st) && S_ISREG(st.st_mode))
			remove(name);
	}

	return FALSE;
}

vbi_export_module_priv
export_ppm = {
	.pub = {
		.keyword	= "ppm",
		.label		= N_("PPM"),
		.tooltip	= N_("Export this page as PPM image"),
	},
	.extension		= "ppm",
	.options		= gfx_opts,
	.local_size		= sizeof(gfx_data),
	.set_option		= gfx_set_option,
	.output			= ppm_output
};

VBI_AUTOREG_EXPORT_MODULE(export_ppm)

/*
 *  PNG - Portable Network Graphics File
 */

#ifdef HAVE_LIBPNG

#include "png.h"
#include "setjmp.h"

static void
draw_char_indexed(png_bytep canvas, png_bytep pen, int glyph, attr_char *ac,
		  int rowstride)
{
	draw_char(sizeof(png_byte), (uint8_t *) canvas, rowstride,
		(uint8_t *) pen, fimg, CPL, CW, CH,
		glyph + 0xC000000, ac->bold, ac->italic,
		ac->underline << 9 /* cell row 9 */, ac->size);
}

static void
draw_drcs_indexed(png_bytep canvas, png_bytep pen,
	uint8_t *src, int glyph, attr_size size, int rowstride)
{
	draw_drcs(sizeof(png_byte), (uint8_t *) canvas, (uint8_t *) pen, src, glyph,
		  size, rowstride);
}

static bool
png_output(vbi_export *e, FILE *fp, char *name, struct fmt_page *pg)
{
	gfx_data *d = (gfx_data *) e->data;
	struct stat st;
	png_structp png_ptr;
	png_infop info_ptr;
	png_color palette[80];
	png_byte alpha[80];
	png_text text[4];
	char title[80];
	png_bytep row_pointer[WH * 2];
	png_bytep image;
	int rowstride = WW * sizeof(*image);
	int i;

	if (pg->columns < 40) {
		vbi_export_error(e, "Oops - caption PNG not ready");
		return FALSE;
	}

	if ((image = malloc(WH * WW * sizeof(*image)))) {
		png_bytep canvas = image;
		png_byte pen[128];
		int row, column;
		attr_char *ac;
		int glyph, conceal = !e->reveal;

		for (i = 2; i < 2 + 8 + 32; i++) {
			pen[i]      = pg->drcs_clut[i];
			pen[i + 64] = pg->drcs_clut[i] + 40;
		}

		for (row = 0; row < H; canvas += W * CW * CH - W * CW, row++) {
			for (column = 0; column < W; canvas += CW, column++) {
				ac = &pg->text[row * pg->columns + column];

				if (ac->size == OVER_TOP
				    || ac->size == OVER_BOTTOM)
					continue;

				glyph = (ac->conceal & conceal) ? GL_SPACE : ac->glyph;

				switch (ac->opacity) {
				case TRANSPARENT_SPACE:
					/*
					 *  Transparent foreground and background.
					 */
					draw_blank(sizeof(*canvas), (uint8_t *) canvas,
						TRANSPARENT_BLACK, rowstride);
					break;

				case TRANSPARENT:
					/*
					 *  Transparent background, opaque foreground. Currently not used.
					 *  Mind Teletext level 2.5 foreground and background transparency
					 *  by referencing colourmap entry 8, TRANSPARENT_BLACK.
					 *  The background of multicolour DRCS is ambiguous, so we make
					 *  them opaque.
					 */
					if ((glyph & 0xFFFF) >= GL_DRCS) {
						pen[0] = TRANSPARENT_BLACK;
						pen[1] = ac->foreground;

						draw_drcs_indexed(canvas, pen, pg->drcs[(glyph & 0x1F00) >> 8],
							glyph, ac->size, rowstride);
					} else {
						pen[0] = TRANSPARENT_BLACK;
						pen[1] = ac->foreground;

						draw_char_indexed(canvas, pen, glyph, ac, rowstride);
					}

					break;

				case SEMI_TRANSPARENT:
					/*
					 *  Translucent background (for 'boxed' text), opaque foreground.
					 *  The background of multicolour DRCS is ambiguous, so we make
					 *  them completely translucent. 
					 */
					if ((glyph & 0xFFFF) >= GL_DRCS) {
						pen[64] = ac->background + 40;
						pen[65] = ac->foreground;

						draw_drcs_indexed(canvas, pen + 64, pg->drcs[(glyph & 0x1F00) >> 8],
							glyph, ac->size, rowstride);
					} else {
						pen[0] = ac->background + 40;
						pen[1] = ac->foreground;

						draw_char_indexed(canvas, pen, glyph, ac, rowstride);
					}

					break;

				case OPAQUE:
					pen[0] = ac->background;
					pen[1] = ac->foreground;

					if ((glyph & 0xFFFF) >= GL_DRCS) {
						draw_drcs_indexed(canvas, pen, pg->drcs[(glyph & 0x1F00) >> 8],
							glyph, ac->size, rowstride);
					} else {
						draw_char_indexed(canvas, pen, glyph, ac, rowstride);
					}

					break;
				}
			}
		}
	} else {
		vbi_export_error(e, _("Unable to allocate %d KB image buffer"),
			WH * WW * sizeof(*image) / 1024);
		return FALSE;
	}

	if (name && !(fp = fopen(name, "wb"))) {
		vbi_export_error(e, _("Cannot create file '%s': %s"), name, strerror(errno));
		free(image);
		return FALSE;
	}

	if (!(png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)))
		goto unknown_error;

	if (!(info_ptr = png_create_info_struct(png_ptr))) {
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		goto unknown_error;
	}

	if (setjmp(png_ptr->jmpbuf))
		goto write_error;

	png_init_io(png_ptr, fp);

	png_set_IHDR(png_ptr, info_ptr, WW, WH << (!!d->double_height),
		8 /* bit_depth */,
		PNG_COLOR_TYPE_PALETTE,
		(d->double_height) ?
			PNG_INTERLACE_ADAM7 : PNG_INTERLACE_NONE,
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

	{
		char *s = title;

		if (e->network.name[0])
			s += sprintf(title, "%s ", e->network.name);
		else
			*s = 0;

		/*
		 *  ISO 8859-1 (Latin-1) character set required,
		 *  see png spec for other
		 */
		if (pg->pgno < 0x100)
			sprintf(s, "Closed Caption"); /* no i18n */
		else if (pg->subno != ANY_SUB)
			sprintf(s, _("Teletext Page %3x.%x"), pg->pgno, pg->subno);
		else
			sprintf(s, _("Teletext Page %3x"), pg->pgno);
	}

	memset(text, 0, sizeof(text));

	text[0].key = "Title";
	text[0].text = title;
	text[0].compression = PNG_TEXT_COMPRESSION_NONE;
	text[1].key = "Software";
	text[1].text = "Zapzilla " VERSION;
	text[1].compression = PNG_TEXT_COMPRESSION_NONE;

	png_set_text(png_ptr, info_ptr, text, 2);

	png_write_info(png_ptr, info_ptr);

	if (d->double_height)
		for (i = 0; i < WH; i++)
			row_pointer[i * 2 + 0] =
			row_pointer[i * 2 + 1] = image + i * WW;
	else
		for (i = 0; i < WH; i++)
			row_pointer[i] = image + i * WW;

	png_write_image(png_ptr, row_pointer);

	png_write_end(png_ptr, info_ptr);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	free(image);
	image = NULL;

	if (name && fclose(fp)) {
		fp = NULL;
		goto write_error;
	}

	return TRUE;

write_error:
	vbi_export_write_error(e, name);

unknown_error:
	if (image)
		free(image);

	if (name) {
		if (fp)
			fclose(fp);

		if (!stat(name, &st) && S_ISREG(st.st_mode))
			remove(name);
	}

	return FALSE;
}

vbi_export_module_priv
export_png = {
	.pub = {
		.keyword	= "png",
		.label		= N_("PNG"),
		.tooltip	= N_("Export this page as PNG image"),
	},
	.extension		= "png",
	.options		= gfx_opts,
	.local_size		= sizeof(gfx_data),
	.set_option		= gfx_set_option,
	.output			= png_output
};

VBI_AUTOREG_EXPORT_MODULE(export_png)

#endif /* HAVE_LIBPNG */
