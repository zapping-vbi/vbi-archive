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

/* $Id: exp-gfx.c,v 1.27 2001-02-27 12:26:47 mschimek Exp $ */

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

/* Character cell dimensions - hardcoded (DRCS) */

#define W               40
#define H               25
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

#define peek(p, i)							                \
((canvas_type == sizeof(unsigned char)) ? ((unsigned char *)(p))[i] :			\
    ((canvas_type == sizeof(unsigned short)) ? ((unsigned char *)(p))[i] :		\
	((unsigned int *)(p))[i]))

#define poke(p, i, v)							                \
((canvas_type == sizeof(unsigned char)) ? (((unsigned char *)(p))[i] = (v)) :		\
    ((canvas_type == sizeof(unsigned short)) ? (((unsigned short *)(p))[i] = (v)) :	\
	(((unsigned int *)(p))[i] = (v))))

static inline void
draw_char(int canvas_type, unsigned char *canvas, unsigned int rowstride,
	unsigned char *pen, unsigned char *font, int cpl, int cw, int ch,
	int glyph, int bold, int italic, unsigned int underline, attr_size size)
{
	unsigned char *src1, *src2;
	unsigned short g = glyph & 0xFFFF;
	int shift1, shift2;
	int x, y, base = 0;

	/* bold = !!bold; */

	if (italic && g < 0x200)
		base = GL_ITALICS;

	x = (base + g) * cw;
	shift1 = x & 7;
	src1 = font + (x >> 3);

	x = (base + 0xC0 + (glyph >> 20)) * cw;
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
	}

	for (y = 0; y < ch; underline >>= 1, y++) {
		int bits = ~0;

		if (!(underline & 1)) {
#if #cpu (i386)
			bits = (*((unsigned short *) src1) >> shift1)
		    	     | (*((unsigned short *) src2) >> shift2);
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
draw_drcs(int canvas_type, unsigned char *canvas, unsigned char *pen,
	unsigned char *src, int glyph, attr_size size, unsigned int rowstride)
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
	}
}

static inline void
draw_blank(int canvas_type, unsigned char *canvas,
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
vbi_draw_page_region(struct fmt_page *pg, void *data, int reveal,
		     int scol, int srow, int width, int height,
		     int rowstride, int flash_on)
{
	unsigned int *canvas = (unsigned int *) data;
	unsigned int pen[64];
	int conceal, row, column;
	attr_char *ac;
	int glyph, i;
	int ww;

	if (rowstride == -1)
		rowstride = WW*sizeof(*canvas);

	ww = rowstride / sizeof(*canvas);

	conceal = !reveal;

	for (i = 2; i < 2 + 8 + 32; i++)
		pen[i] = pg->colour_map[pg->drcs_clut[i]];

	for (row = srow; row < srow+height; canvas += ww * (CH - 1), row++) {
		for (column = scol; column < scol+width; canvas += CW, column++) {
			ac = &pg->text[row * pg->columns + column];

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
					draw_drcs(sizeof(*canvas), (unsigned char *) canvas,
						(unsigned char *) pen,
						pg->drcs[(glyph & 0x1F00) >> 8], glyph, ac->size, rowstride);
				} else {
					draw_char(sizeof(*canvas), (unsigned char *) canvas, rowstride,
						(unsigned char *) pen, fimg, CPL, CW, CH,
						glyph, ac->bold, ac->italic,
						ac->underline << 9 /* cell row 9 */, ac->size);
				}
			}
		}

		canvas += (ww/CW-width)*CW;
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
draw_char_indexed(png_bytep canvas, png_bytep pen, int glyph, attr_char *ac,
		  int rowstride)
{
	draw_char(sizeof(png_byte), (unsigned char *) canvas, rowstride,
		(unsigned char *) pen, fimg, CPL, CW, CH,
		glyph, ac->bold, ac->italic,
		ac->underline << 9 /* cell row 9 */, ac->size);
}

static void
draw_drcs_indexed(png_bytep canvas, png_bytep pen,
	unsigned char *src, int glyph, attr_size size, int rowstride)
{
	draw_drcs(sizeof(png_byte), (unsigned char *) canvas, (unsigned char *) pen, src, glyph,
		  size, rowstride);
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
	int rowstride = WW*sizeof(*image);

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
					draw_blank(sizeof(*canvas), (unsigned char *) canvas,
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

/* PRELIMINARY */

#include <assert.h>
#include "ccfont.xbm"

#define CELL_WIDTH 16
#define CELL_HEIGHT 26

#define NUM_COLS 34
#define NUM_ROWS 15

static const unsigned char palette[8][3] = {
  {0x00, 0x00, 0x00},
  {0xff, 0x00, 0x00},
  {0x00, 0xff, 0x00},
  {0xff, 0xff, 0x00},
  {0x00, 0x00, 0xff},
  {0xff, 0x00, 0xff},
  {0x00, 0xff, 0xff},
  {0xff, 0xff, 0xff}
};

static inline void
cc_draw_char(unsigned char *canvas, unsigned int c, unsigned char *pen,
	  int underline, int rowstride)
{
  unsigned short *s = ((unsigned short *) bitmap_bits)
    + (c & 31) + (c >> 5) * 32 * CELL_HEIGHT;
  int x, y, b;
  
  for (y = 0; y < CELL_HEIGHT; y++) {
    b = *s;
    s += 32;
    
    if (underline && (y >= 24 && y <= 25))
      b = ~0;
    
    for (x = 0; x < CELL_WIDTH; x++) {
      canvas[x*4+0] = pen[(b & 1)*3+0];
      canvas[x*4+1] = pen[(b & 1)*3+1];
      canvas[x*4+2] = pen[(b & 1)*3+2];
      canvas[x*4+3] = 0xFF;
      b >>= 1;
    }

    canvas += rowstride;
  }
}

static void
draw_row(unsigned char *canvas, attr_char *line, int width, int rowstride)
{
  int i;
  unsigned char pen[6];
  
  for (i = 0; i < width; i++)
    {
      switch (line[i].opacity)
	{
	case TRANSPARENT_SPACE:
	  assert(0);
	  break;

	case TRANSPARENT:
	case SEMI_TRANSPARENT:
	  /* Transparency not implemented */
	  /* It could be done in some cases by setting the background
	     to the XVideo chroma */
/*	  pen[0] = palette[0][0];
	  pen[1] = palette[0][1];
	  pen[2] = palette[0][2];
	  pen[3] = palette[line[i].foreground][0];
	  pen[4] = palette[line[i].foreground][1];
	  pen[5] = palette[line[i].foreground][2];
	  break;
*/	    
	default:
	  pen[0] = palette[line[i].background][0];
	  pen[1] = palette[line[i].background][1];
	  pen[2] = palette[line[i].background][2];
	  pen[3] = palette[line[i].foreground][0];
	  pen[4] = palette[line[i].foreground][1];
	  pen[5] = palette[line[i].foreground][2];
	  break;
	}

      cc_draw_char(canvas, line[i].glyph & 0xFF, pen, line[i].underline,
		rowstride);
      canvas += CELL_WIDTH*4;
    }
}

void
vbi_draw_cc_page_region(struct fmt_page *pg, void *data,
	int scol, int srow, int width, int height, int rowstride)
{
	unsigned char *canvas = (unsigned char *) data;
	int row;

	if (rowstride == -1)
		rowstride = NUM_COLS * CELL_WIDTH * 4; // sizeof(*canvas);

	for (row = srow; row < srow+height; row++) {
		draw_row(canvas, pg->text + row * pg->columns + scol,
			width, rowstride);

		canvas += rowstride * CELL_HEIGHT;
	}
}
