/*
 *  Zapzilla - Closed Caption and Teletext HTML export functions
 *
 *  Copyright (C) 2001 Michael H. Schimek
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

/* $Id: exp-html.c,v 1.14 2001-03-17 17:18:27 garetxe Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <sys/stat.h>
#include <unistd.h>

#include <iconv.h>

#include "lang.h"
#include "export.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef struct style {
	struct style *		next;
	int			ref_count;
	int			foreground;
	int			background;
} style;

typedef struct html_data	// private data in struct export
{
  unsigned int gfx_chr;
  unsigned char headerless;

	char *			name;
	FILE *			fp;
	iconv_t			cd;
	int			foreground;
	int			background;
	unsigned int		underline : 1;
	unsigned int		bold : 1;
	unsigned int		italic : 1;
	unsigned int		flash : 1;
	unsigned int		span : 1;

	style *			styles;
	style			def;
} html_data;







#define D  ((struct html_data *)e->data)

static int
html_open(struct export *e)
{
    D->gfx_chr = '#';
    D->headerless = 0;
    //e->reveal=1;	// the default should be the same for all formats.
    return 0;
}

static int
html_option(struct export *e, int opt, char *str_arg, int num_arg)
{
	switch (opt) {
	case 0: /* gfx-chr */
		if (strlen(str_arg) == 1)
			num_arg = *str_arg;
		D->gfx_chr = MAX(num_arg, 0x20);
		break;
	case 1: /* header */
		D->headerless = !num_arg;
		break;
	}

	return 0;
}

static vbi_export_option html_opts[] = {
	{
		VBI_EXPORT_STRING,	"gfx-chr",	N_("Graphics char"),
		{ .str = "#" }, 0, 0, NULL, N_("Replacement for block graphic characters: a single character or decimal (32) or hex (0x20) code")
	}, {
		VBI_EXPORT_BOOL,	"header",	N_("HTML header"),
		{ .num = TRUE }, FALSE, TRUE, NULL, N_("Include HTML page header")
	}, {
		0
	}
};

#define TEST 0
#define LF "\n"	/* optional "" */

static void
hash_colour(FILE *fp, unsigned int colour)
{
	fprintf(fp, "#%02x%02x%02x",
		(colour >> 0) & 0xFF,
		(colour >> 8) & 0xFF,
		(colour >> 16) & 0xFF);		
}

static const char *	html_underline[]	= { "</u>", "<u>" };
static const char *	html_bold[]		= { "</b>", "<b>" };
static const char *	html_italic[]		= { "</i>", "<i>" };
static const char *	html_flash[]		= { "</blink>", "<blink>" };

static void
write_error(struct export *e)
{
	html_data *h = (html_data *) e->data;
	struct stat st;

	export_error(e, errno ?
		_("error while writing file '%s': %s") :
		_("error while writing file '%s'"), h->name, strerror(errno));

	if (h->fp)
		fclose(h->fp);

	if (!stat(h->name, &st) && S_ISREG(st.st_mode))
		remove(h->name);
}

/*
    Title: "<title lang=\"en\" dir=\"ltr\">Medieval Bee-Keeping</title>
 */
static bool
header(struct export *e, char *name, struct fmt_page *pg, char *title)
{
	html_data *h = (html_data *) e->data;
	char *charset, *lang = NULL, *dir = NULL;

	h->name = name;

	switch (pg->font[0] - font_descriptors) {
	case 0:	 /* English */
	case 16: /* English */
		lang = "en";

	case 1:	 /* German */
	case 9:	 /* German */
	case 17: /* German */
	case 33: /* German */
		if (!lang) lang = "de";

	case 2:	 /* Swedish/Finnish/Hungarian */
	case 10: /* Swedish/Finnish/Hungarian */
	case 18: /* Swedish/Finnish/Hungarian */
		if (!lang) lang = "";

	case 3:	 /* Italian */
	case 11: /* Italian */
	case 19: /* Italian */
		if (!lang) lang = "it";

	case 4:	 /* French */
	case 12: /* French */
	case 20: /* French */
		if (!lang) lang = "fr";

	case 5:	 /* Portuguese/Spanish */
	case 21: /* Portuguese/Spanish */
		if (!lang) lang = "es";

	default:
		charset = "iso-8859-1";
		break;

	case 6:	 /* Czech/Slovak */
	case 14: /* Czech/Slovak */
	case 38: /* Czech/Slovak */
		lang = "";

	case 8:	 /* Polish */
		if (!lang) lang = ""; /* ? */

	case 29: /* Serbian/Croatian/Slovenian */
		if (!lang) lang = "";

	case 31: /* Romanian */
		if (!lang) lang = ""; /* ? */
		charset = "iso-8859-2";
		break;

	case 34: /* Estonian */
		lang = ""; /* ? */

	case 35: /* Lettish/Lithuanian */
		charset = "iso-8859-4";
		break;

	case 32: /* Serbian/Croatian */
		lang = "";

	case 36: /* Russian/Bulgarian */
		if (!lang) lang = "ru";

	case 37: /* Ukranian */
		if (!lang) lang = ""; /* ? */
		charset = "iso-8859-5";
		break;

	case 64: /* Arabic/English */
	case 68: /* Arabic/French */
	case 71: /* Arabic */
	case 87: /* Arabic */
		lang = "ar";
		dir = ""; /* ? */
		charset = "iso-8859-6";	/* XXX needs further examination */
		break;

	case 55: /* Greek */
		lang = "el";
		charset = "iso-8859-7";
		break;

	case 85: /* Hebrew */
		lang = "he";
		dir = ""; /* ? */
		charset = "iso-8859-8";	/* XXX needs further examination */
		break;

	case 22: /* Turkish */
	case 54: /* Turkish */
		lang = ""; /* ? */
		charset = "iso-8859-9";
		break;

	case 99: /* Klingon */
		lang = "x-klingon";
		charset = "iso-10646";
		break;
	}

	if ((h->cd = iconv_open(charset, "UCS2")) == (iconv_t) -1) {
		export_error(e, "character conversion not supported, should not happen");
		return FALSE;
	}

	if (!(h->fp = fopen(name, "w"))) {
		export_error(e, _("cannot create file '%s': %s"), name, strerror(errno));
		iconv_close(h->cd);
		return FALSE;
	}

	if (!h->headerless) {
		style *s;
		int ord;

		fprintf(h->fp,
			"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\" "
				"\"http://www.w3.org/TR/REC-html40/loose.dtd\">" LF
			"<html>" LF "<head>" LF
			"<meta name=\"generator\" lang=\"en\" content=\"Zapzilla " VERSION "\">" LF
			"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=%s\">" LF
			"<style type=\"text/css\">" LF "<!--" LF,
			charset);

		for (s = h->styles, ord = 1; s; s = s->next)
			if (s != &h->def && s->ref_count > 1) {
				fprintf(h->fp, "span.c%d { color:", ord);
				hash_colour(h->fp, pg->colour_map[s->foreground]);
				fputs("; background-color:", h->fp);
				hash_colour(h->fp, pg->colour_map[s->background]);
				fputs(" }" LF, h->fp);
				ord++;
			}

		fprintf(h->fp,
			"//-->" LF "</style>" LF
			"%s" /* title */ LF
			"</head>" LF
			"<body ",
			title);

		if (lang && *lang)
			fprintf(h->fp, "lang=\"%s\" ", lang);

		if (dir && *dir)
			fprintf(h->fp, "dir=\"%s\" ", dir);

		fputs("text=\"#FFFFFF\" bgcolor=\"", h->fp);

		hash_colour(h->fp, pg->colour_map[pg->screen_colour]);

		fputs("\">" LF, h->fp);
	}

	if (ferror(h->fp)) {
		write_error(e);
		return FALSE;
	}

	h->foreground	= WHITE;
	h->background	= pg->screen_colour;
	h->underline	= FALSE;
	h->bold		= FALSE;
	h->italic	= FALSE;
	h->flash	= FALSE;
	h->span		= FALSE;

	return TRUE;
}

static int
html_output(struct export *e, char *name, struct fmt_page *pgp)
{
	html_data *h = (html_data *) e->data;
	struct fmt_page pg;
	attr_char *acp;
	int i, j;

	pg = *pgp;

#if TEST
	h->underline	= FALSE;
	h->bold		= FALSE;
	h->italic	= FALSE;
	h->flash	= FALSE;
#endif

	h->styles = &h->def;
	h->def.next = NULL;
	h->def.ref_count = 2;
	h->def.foreground = h->foreground;
	h->def.background = h->background;

	for (acp = pg.text, i = 0; i < pg.rows; acp += pg.columns, i++) {
		int blank = 0;

		for (j = 0; j < pg.columns; j++) {
			int glyph = (acp[j].conceal && !e->reveal) ? GL_SPACE : acp[j].glyph;
#if TEST
			acp[j].underline = underline;
			acp[j].bold	 = bold;
			acp[j].italic	 = italic;
			acp[j].flash	 = flash;

			if ((rand() & 15) == 0)
				h->underline = rand() & 1;
			if ((rand() & 15) == 1)
				h->bold	  = rand() & 1;
			if ((rand() & 15) == 2)
				h->italic = rand() & 1;
			if ((rand() & 15) == 3)
				h->flash  = rand() & 1;
#endif
			if (acp[j].size > DOUBLE_SIZE)
				glyph = 0x20;

			if (glyph == 0x20 || glyph == 0xA0) {
				blank++;
				continue;
			}

			if (blank > 0) {
				attr_char ac = acp[j];

				ac.glyph = 0x20;

				/* XXX should match fg and bg transitions */
				while (blank > 0) {
					ac.background = acp[j - blank].background;
					acp[j - blank] = ac;
					blank--;
				}
			}

			acp[j].glyph = glyph;
		}

		if (blank > 0) {
			attr_char ac;

			if (blank < pg.columns)
				ac = acp[pg.columns - 1 - blank];
			else {
				memset(&ac, 0, sizeof(ac));
				ac.foreground = 7;
			}

			ac.glyph = 0x20;

			while (blank > 0) {
				ac.background = acp[pg.columns - blank].background;
				acp[pg.columns - blank] = ac;
				blank--;
			}
		}

		for (j = 0; j < pg.columns; j++) {
			attr_char ac = acp[j];
			style *s, **sp;

			for (sp = &h->styles; (s = *sp); sp = &s->next) {
				if (s->background != ac.background)
					continue;
				if (ac.glyph == 0x20 || s->foreground == ac.foreground)
					break;
			}

			if (!s) {
				s = calloc(1, sizeof(style));
				*sp = s;
				s->foreground = ac.foreground;
				s->background = ac.background;
			}

			s->ref_count++;
		}
	}

	if (!header(e, name, &pg, "<title>Medieval Bee-Keeping</title>"))
		return -1;

	fputs("<pre>", h->fp);

	h->underline  = FALSE;
	h->bold	      = FALSE;
	h->italic     = FALSE;
	h->flash      = FALSE;
	h->span	      = FALSE;

	/* XXX this can get extremely large and ugly, should be improved. */
	for (acp = pg.text, i = 0; i < pg.rows; acp += pg.columns, i++) {
		for (j = 0; j < pg.columns; j++) {
			int code;

			if ((acp[j].glyph != 0x20 && acp[j].foreground != h->foreground)
			    || acp[j].background != h->background) {
				style *s;
				int ord;

				if (h->flash)
					fputs(html_flash[0], h->fp);
				if (h->italic)
					fputs(html_italic[0], h->fp);
				if (h->bold)
					fputs(html_bold[0], h->fp);
				if (h->underline)
					fputs(html_underline[0], h->fp);
				if (h->span)
					fputs("</span>", h->fp);

				h->underline  = FALSE;
				h->bold	      = FALSE;
				h->italic     = FALSE;
				h->flash      = FALSE;

				for (s = h->styles, ord = 0; s; s = s->next)
					if (s->ref_count > 1) {
						if ((acp[j].glyph == 0x20 || s->foreground == acp[j].foreground)
						    && s->background == acp[j].background)
							break;
						ord++;
					}

				if (s != &h->def) {
					if (s) {
						h->foreground = s->foreground;
						h->background = s->background;
						fprintf(h->fp, "<span class=\"c%d\">", ord);
					} else {
						h->foreground = acp[j].foreground;
						h->background = acp[j].background;
						fputs("<span style=\"color:", h->fp);
						hash_colour(h->fp, pg.colour_map[h->foreground]);
						fputs(";background-color:", h->fp);
						hash_colour(h->fp, pg.colour_map[h->background]);
						fputs("\">", h->fp);
					}

					h->span = TRUE;
				} else {
					h->foreground = s->foreground;
					h->background = s->background;
					h->span = FALSE;
				}
			}

			if (acp[j].underline != h->underline
			    || acp[j].bold != h->bold
			    || acp[j].italic != h->italic
			    || acp[j].flash != h->flash) {
				if (h->flash)
					fputs(html_flash[0], h->fp);
				if (h->italic)
					fputs(html_italic[0], h->fp);
				if (h->bold)
					fputs(html_bold[0], h->fp);
				if (h->underline)
					fputs(html_underline[0], h->fp);

				h->underline  = FALSE;
				h->bold	      = FALSE;
				h->italic     = FALSE;
				h->flash      = FALSE;
			}

			if (acp[j].underline != h->underline) {
				h->underline = acp[j].underline;
				fputs(html_underline[h->underline], h->fp);
			}

			if (acp[j].bold != h->bold) {
				h->bold = acp[j].bold;
				fputs(html_bold[h->bold], h->fp);
			}

			if (acp[j].italic != h->italic) {
				h->italic = acp[j].italic;
				fputs(html_italic[h->italic], h->fp);
			}

			if (acp[j].flash != h->flash) {
				h->flash = acp[j].flash;
				fputs(html_flash[h->flash], h->fp);
			}

#if TEST
			if (!(rand() & 15))
				code = glyph_iconv(cd, 0x100 + (rand() & 0xFF), D->gfx_chr);
			else
#endif
			code = glyph_iconv(h->cd, acp[j].glyph, D->gfx_chr);

			if (code < 0) {
				fprintf(h->fp, "&#%u;", -code);
			} else {
				switch (code) {
				case '<':
					fputs("&lt;", h->fp);
					break;

				case '>':
					fputs("&gt;", h->fp);
					break;

				case '&':
					fputs("&amp;", h->fp);
					break;

				default:
					fputc(code, h->fp);
				}
			}
		}

		fputc('\n', h->fp);
	}

	if (h->flash)
		fputs(html_flash[0], h->fp);
	if (h->italic)
		fputs(html_italic[0], h->fp);
	if (h->bold)
		fputs(html_bold[0], h->fp);
	if (h->underline)
		fputs(html_underline[0], h->fp);
	if (h->span)
		fputs("</span>", h->fp);

	fputs("</pre>", h->fp);

	{
		style *s;

		while ((s = h->styles)) {
			h->styles = s->next;
			if (s != &h->def)
				free(s);
		}
	}

	if (!D->headerless)
		fputs(LF "</body>" LF "</html>", h->fp);

	fputc('\n', h->fp);

	iconv_close(h->cd);

	if (ferror(h->fp)) {
		write_error(e);
		return -1;
	}

	if (fclose(h->fp)) {
		h->fp = NULL;
		write_error(e);
		return -1;
	}

	return 0;
}

struct export_module export_html[1] =	// exported module definition
{
  {
    "html",			// id
    "html",			// extension
    html_opts,			// options
    sizeof(struct html_data),	// size
    html_open,			// open
    0,				// close
    html_option,		// option
    html_output			// output
  }
};
