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

/* $Id: exp-gfx.c,v 1.23 2001-02-18 07:37:26 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <sys/stat.h>
#include <unistd.h>

#include "lang.h"
#include "export.h"

#include "wstfont.xbm"

/* future */
#undef _
#define _(String) (String)

#undef WW
#undef WH
#undef CW
#undef CH


/* Character cell dimensions - hardcoded (DRCS) */

#define CW		12		
#define CH		10
#define WW		(W * CW)
#define WH		(H * CH)

#define CPL		(wstfont_width / CW * wstfont_height / CH)

static unsigned char *fimg;
static void init_gfx(void) __attribute__ ((constructor));

static void
init_gfx(void)
{
	unsigned char *p;
	int i, j;

	/* de-interleave font image (puts all chars in row 0) */

	if (!(fimg = calloc(CPL * CW * (CH + 1) / 8, 1)))
		exit(EXIT_FAILURE);

	p = fimg;

	for (i = 0; i < CH; i++)
		for (j = 0; j < wstfont_height; p += wstfont_width / 8, j += CH)
			memcpy(p, wstfont_bits + (j + i) * wstfont_width / 8,
				wstfont_width / 8);
}

#if CW < 9 || CW > 16
#error CW out of range
#endif

#define peek(p, i)							\
((canvas_type == sizeof(u8)) ? ((u8 *)(p))[i] :				\
    ((canvas_type == sizeof(u16)) ? ((u16 *)(p))[i] :			\
	((u32 *)(p))[i]))

#define poke(p, i, v)							\
((canvas_type == sizeof(u8)) ? (((u8 *)(p))[i] = (v)) :			\
    ((canvas_type == sizeof(u16)) ? (((u16 *)(p))[i] = (v)) :		\
	(((u32 *)(p))[i] = (v))))

static inline void
draw_char(int canvas_type, u8 *canvas, u8 *pen,
	int glyph, int bold, int underline, int italic, attr_size size)
{
	unsigned char *src1, *src2;
	unsigned short g = glyph & 0xFFFF;
	int shift1, shift2;
	int x, y, base = 0;
	int ch = CH;

	if (italic && g < 0x200)
		base = GL_ITALICS;

	x = (base + g) * CW;
	shift1 = x & 7;
	src1 = fimg + (x >> 3);

	x = (base + 0xC0 + (glyph >> 20)) * CW;
	shift2 = (x & 7) + ((glyph >> 18) & 1);
	src2 = fimg + (x >> 3);
	if (glyph & (1 << 19))
		src2 += CPL * CW / 8;

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
				poke(canvas, x, peek(pen, bits & 1));

			canvas += WW * canvas_type;

			break;

		case DOUBLE_HEIGHT:
			for (x = 0; x < CW; bits >>= 1, x++) {
				unsigned int col = peek(pen, bits & 1);

				poke(canvas, x, col);
				poke(canvas, x + WW, col);
			}

			canvas += 2 * WW * canvas_type;

			break;

		case DOUBLE_WIDTH:
			for (x = 0; x < CW * 2; bits >>= 1, x += 2) {
				unsigned int col = peek(pen, bits & 1);

				poke(canvas, x + 0, col);
				poke(canvas, x + 1, col);
			}

			canvas += WW * canvas_type;

			break;

		case DOUBLE_SIZE:
			for (x = 0; x < CW * 2; bits >>= 1, x += 2) {
				unsigned int col = peek(pen, bits & 1);

				poke(canvas, x + 0, col);
				poke(canvas, x + 1, col);
				poke(canvas, x + WW + 0, col);
				poke(canvas, x + WW + 1, col);
			}

			canvas += 2 * WW * canvas_type;

			break;

		default:
			break;
		}

		src1 += CPL * CW / 8;
		src2 += CPL * CW / 8;
	}
}

static inline void
draw_drcs(int canvas_type, u8 *canvas, u8 *pen,
	unsigned char *src, int glyph, attr_size size)
{
	unsigned int col;
	int x, y;

	src += (glyph & 0x3F) * 60;
	pen += (glyph >> 16) * canvas_type;

	switch (size) {
	case NORMAL:
		for (y = 0; y < CH; canvas += WW * canvas_type, y++)
			for (x = 0; x < 12; src++, x += 2) {
				poke(canvas, x + 0, peek(pen, *src & 15));
				poke(canvas, x + 1, peek(pen, *src >> 4));
			}
		break;

	case DOUBLE_HEIGHT:
		for (y = 0; y < CH; canvas += 2 * WW * canvas_type, y++)
			for (x = 0; x < 12; src++, x += 2) {
				col = peek(pen, *src & 15);
				poke(canvas, x + 0, col);
				poke(canvas, x + WW + 0, col);

				col = peek(pen, *src >> 4);
				poke(canvas, x + 1, col);
				poke(canvas, x + WW + 1, col);
			}
		break;

	case DOUBLE_WIDTH:
		for (y = 0; y < CH; y++)
			for (x = 0; x < 12 * 2; src++, x += 4) {
				col = peek(pen, *src & 15);
				poke(canvas, x + 0, col);
				poke(canvas, x + 1, col);

				col = peek(pen, *src >> 4);
				poke(canvas, x + 2, col);
				poke(canvas, x + 3, col);
			}
		break;

	case DOUBLE_SIZE:
		for (y = 0; y < CH; canvas += 2 * WW * canvas_type, y++)
			for (x = 0; x < 12 * 2; src++, x += 4) {
				col = peek(pen, *src & 15);
				poke(canvas, x + 0, col);
				poke(canvas, x + 1, col);
				poke(canvas, x + WW + 0, col);
				poke(canvas, x + WW + 1, col);

				col = peek(pen, *src >> 4);
				poke(canvas, x + 2, col);
				poke(canvas, x + 3, col);
				poke(canvas, x + WW + 2, col);
				poke(canvas, x + WW + 3, col);
			}
	default:
		break;
	}
}

static inline void
draw_blank(int canvas_type, u8 *canvas, unsigned int colour)
{
	int x, y;

	for (y = 0; y < CH; y++) {
		for (x = 0; x < CW; x++)
			poke(canvas, x, colour);

		canvas += WW * canvas_type;
	}
}

void
vbi_draw_page_region(struct fmt_page *pg, void *data, int conceal,
		     int scol, int srow, int width, int height)
{
	unsigned int *canvas = (unsigned int *) data;
	unsigned int pen[64];
	int row, column;
	attr_char *ac;
	int glyph, i;

	conceal = !!conceal;

	for (i = 2; i < 2 + 8 + 32; i++)
		pen[i] = pg->colour_map[pg->drcs_clut[i]];

	for (row = srow; row < srow+height;
	     canvas += W * CW * CH - W * CW, row++) {
		for (column = scol; column < scol+width;
		     canvas += CW, column++) {
			ac = &pg->text[row * pg->columns + column];

			glyph = (ac->conceal & conceal) ? GL_SPACE : ac->glyph;

			pen[0] = pg->colour_map[ac->background];
			pen[1] = pg->colour_map[ac->foreground];

			if (ac->size <= DOUBLE_SIZE) {
				if ((glyph & 0xFFFF) >= GL_DRCS) {
					draw_drcs(sizeof(*canvas), (u8 *) canvas, (u8 *) pen,
						pg->drcs[(glyph & 0x1F00) >> 8], glyph, ac->size);
				} else {
					draw_char(sizeof(*canvas), (u8 *) canvas, (u8 *) pen,
						glyph, ac->bold, ac->underline, ac->italic, ac->size);
				}
			}
		}

		canvas += (W-width)*CW;
	}
}

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
	 *  real TV closer to 4:3 (11 MHz pixel clock), but I {mhs}
	 *  think one should export raw, not scaled data.
	 */
} gfx_data;

static int
gfx_open(struct export *e)
{
	gfx_data *d = (gfx_data *) e->data;

	d->double_height = 1;

	return 0;
}

static int
gfx_option(struct export *e, int opt, char *arg)
{
	gfx_data *d = (gfx_data *) e->data;

	switch (opt) {
	case 1: // aspect
		d->double_height = !d->double_height;
		break;
	}

	return 0;
}

static char *
gfx_opts[] =
{
	"aspect",	// line doubling
	0
};

/*
 *  PPM - Portable Pixmap File (raw)
 */

static int
ppm_output(struct export *e, char *name, struct fmt_page *pg)
{
	gfx_data *d = (gfx_data *) e->data;
	unsigned int *image;
	unsigned char *body;
	struct stat st;
	FILE *fp;
	int i;

	if (!(image = malloc(WH * WW * sizeof(*image)))) {
		export_error(e, _("unable to allocate %d KB image buffer"),
			WH * WW * sizeof(*image) / 1024);
		return 0;
	}

	vbi_draw_page(pg, image, !e->reveal);

	if (!(fp = fopen(name, "wb"))) {
		export_error(e, _("cannot create file '%s': %s"), name, strerror(errno));
		free(image);
		return -1;
	}

	fprintf(fp, "P6 %d %d 15\n", WW, WH << (!!d->double_height));

	if (ferror(fp))
		goto write_error;

	body = (unsigned char *) image;

	for (i = 0; i < WH * WW; body += 3, i++) {
		unsigned int n = (image[i] >> 4) & 0x0F0F0F;

		body[0] = n;
		body[1] = n >> 8;
		body[2] = n >> 16;
	}

	if (d->double_height) {
		body = (unsigned char *) image;

		for (i = 0; i < WH; body += WW * 3, i++) {
			if (!fwrite(body, WW * 3, 1, fp))
				goto write_error;
			if (!fwrite(body, WW * 3, 1, fp))
				goto write_error;
		}
	} else
		if (!fwrite(image, WH * WW * 3, 1, fp))
			goto write_error;

	free(image);
	image = NULL;

	if (fclose(fp)) {
		fp = NULL;
		goto write_error;
	}

	return 0;

write_error:
	export_error(e, errno ?
		_("error while writing file '%s': %s") :
		_("error while writing file '%s'"), name, strerror(errno));

	if (image)
		free(image);

	if (fp)
		fclose(fp);

	if (!stat(name, &st) && S_ISREG(st.st_mode))
		remove(name);

	return -1;
}

struct export_module
export_ppm[1] =			// exported module definition
{
    {
	"ppm",			// id
	"ppm",			// extension
	gfx_opts,		// options
	sizeof(gfx_data),	// size
	gfx_open,		// open
	0,			// close
	gfx_option,		// option
	ppm_output		// output
    }
};

/*
 *  PNG - Portable Network Graphics File
 */

#ifdef HAVE_LIBPNG

#include "png.h"
#include "setjmp.h"

static void
draw_char_indexed(png_bytep canvas, png_bytep pen, int glyph, attr_char *ac)
{
	draw_char(sizeof(png_byte), (u8 *) canvas, (u8 *) pen,
		glyph, ac->bold, ac->underline, ac->italic, ac->size);
}

static void
draw_drcs_indexed(png_bytep canvas, png_bytep pen,
	unsigned char *src, int glyph, attr_size size)
{
	draw_drcs(sizeof(png_byte), (u8 *) canvas, (u8 *) pen, src, glyph, size);
}

static int
png_output(struct export *e, char *name, struct fmt_page *pg)
{
	gfx_data *d = (gfx_data *) e->data;
	FILE *fp;
	struct stat st;
	png_structp png_ptr;
	png_infop info_ptr;
	png_color palette[80];
	png_byte alpha[80];
	png_text text[4];
	char title[80];
	png_bytep row_pointer[WH * 2];
	png_bytep image;
	int i;

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

				if (ac->size > DOUBLE_SIZE)
					continue;

				glyph = (ac->conceal & conceal) ? GL_SPACE : ac->glyph;

				switch (ac->opacity) {
				case TRANSPARENT_SPACE:
					/*
					 *  Transparent foreground and background.
					 */
					draw_blank(sizeof(*canvas), (u8 *) canvas, TRANSPARENT_BLACK);
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

						draw_drcs_indexed(canvas, pen, 
							pg->drcs[(glyph & 0x1F00) >> 8], glyph, ac->size);
					} else {
						pen[0] = TRANSPARENT_BLACK;
						pen[1] = ac->foreground;

						draw_char_indexed(canvas, pen, glyph, ac);
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

						draw_drcs_indexed(canvas, pen + 64, 
							pg->drcs[(glyph & 0x1F00) >> 8], glyph, ac->size);
					} else {
						pen[0] = ac->background + 40;
						pen[1] = ac->foreground;

						draw_char_indexed(canvas, pen, glyph, ac);
					}

					break;

				case OPAQUE:
					pen[0] = ac->background;
					pen[1] = ac->foreground;

					if ((glyph & 0xFFFF) >= GL_DRCS) {
						draw_drcs_indexed(canvas, pen, 
							pg->drcs[(glyph & 0x1F00) >> 8], glyph, ac->size);
					} else {
						draw_char_indexed(canvas, pen, glyph, ac);
					}

					break;
				}
			}
		}
	} else {
		export_error(e, _("unable to allocate %d KB image buffer"),
			WH * WW * sizeof(*image) / 1024);
		return -1;
	}

	if (!(fp = fopen(name, "wb"))) {
		export_error(e, _("cannot create file '%s': %s"), name, strerror(errno));
		free(image);
		return -1;
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

	/*
	 *  ISO 8859-1 (Latin-1) character set required,
	 *  see png spec for other
	 */
	memset(text, 0, sizeof(text));
	snprintf(title, sizeof(title) - 1,
		_("Teletext Page %3x/%04x"),
		pg->pgno, pg->subno); // XXX make it "station_short Teletext ..."

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

	if (fclose(fp)) {
		fp = NULL;
		goto write_error;
	}

	return 0;

write_error:
	export_error(e, errno ?
		_("error while writing '%s': %s") :
		_("error while writing '%s'"), name, strerror(errno));

unknown_error:
	if (image)
		free(image);

	if (fp)
		fclose(fp);

	if (!stat(name, &st) && S_ISREG(st.st_mode))
		remove(name);

	return -1;
}

struct export_module
export_png[1] =			// exported module definition
{
    {
	"png",			// id
	"png",			// extension
	gfx_opts,		// options
	sizeof(gfx_data),	// size
	gfx_open,		// open
	0,			// close
	gfx_option,		// option
	png_output		// output
    }
};

#endif /* HAVE_LIBPNG */
