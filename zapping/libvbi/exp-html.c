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

/* $Id: exp-html.c,v 1.20 2001-07-31 12:59:50 mschimek Exp $ */

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

typedef struct html_data
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
	unsigned int		link : 1;

	style *			styles;
	style			def;
} html_data;

// XXX css option?

static bool
html_set_option(vbi_export *e, int opt, char *str_arg, int num_arg)
{
	html_data *d = (html_data *) e->data;

	switch (opt) {
	case 0: /* gfx-chr */
		if (strlen(str_arg) == 1)
			num_arg = *str_arg;
		d->gfx_chr = MAX(num_arg, 0x20);
		break;
	case 1: /* header */
		d->headerless = !num_arg;
		break;
	}

	return TRUE;
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

static void
escaped_fputc(FILE *fp, int c)
{
	if (c < 0)
		fprintf(fp, "&#%u;", -c);
	else
		switch (c) {
		case '<':
			fputs("&lt;", fp);
			break;

		case '>':
			fputs("&gt;", fp);
			break;

		case '&':
			fputs("&amp;", fp);
			break;

		default:
			putc(c, fp);
			break;
		}
}

static void
escaped_fputs(FILE *fp, char *s)
{
	while (*s) {
		if (*s < 0) /* Latin-1 */
			escaped_fputc(fp, - (unsigned char) *s);
		else
			escaped_fputc(fp, *s);
		s++;
	}
}

static const char *	html_underline[]	= { "</u>", "<u>" };
static const char *	html_bold[]		= { "</b>", "<b>" };
static const char *	html_italic[]		= { "</i>", "<i>" };
static const char *	html_flash[]		= { "</blink>", "<blink>" };

static void
write_error(vbi_export *e)
{
	html_data *d = (html_data *) e->data;
	struct stat st;

	vbi_export_write_error(e, d->name);

	if (d->name) {
		if (d->fp)
			fclose(d->fp);

		if (!stat(d->name, &st) && S_ISREG(st.st_mode))
			remove(d->name);
	}
}

static bool
header(vbi_export *e, FILE *fp, char *name, struct fmt_page *pg, char *title)
{
	html_data *d = (html_data *) e->data;
	char *charset, *lang = NULL, *dir = NULL;

	d->fp = fp;
	d->name = name;

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

	if ((d->cd = iconv_open(charset, "UCS2")) == (iconv_t) -1) {
		vbi_export_error(e, _("Character conversion Unicode -> %s not supported"), charset);
		return FALSE;
	}

	if (name && !(d->fp = fopen(name, "w"))) {
		vbi_export_error(e, _("Cannot create file '%s': %s"), name, strerror(errno));
		iconv_close(d->cd);
		return FALSE;
	}

	if (!d->headerless) {
		style *s;
		int ord;

		fprintf(d->fp,
			"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\" "
				"\"http://www.w3.org/TR/REC-html40/loose.dtd\">" LF
			"<html>" LF "<head>" LF
			"<meta name=\"generator\" lang=\"en\" content=\"Zapzilla " VERSION "\">" LF
			"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=%s\">" LF
			"<style type=\"text/css\">" LF "<!--" LF,
			charset);

		for (s = d->styles, ord = 1; s; s = s->next)
			if (s != &d->def && s->ref_count > 1) {
				fprintf(d->fp, "span.c%d { color:", ord);
				hash_colour(d->fp, pg->colour_map[s->foreground]);
				fputs("; background-color:", d->fp);
				hash_colour(d->fp, pg->colour_map[s->background]);
				fputs(" }" LF, d->fp);
				ord++;
			}

		fprintf(d->fp,
			"//-->" LF "</style>" LF
			"%s" /* title */ LF
			"</head>" LF
			"<body ",
			title);

		if (lang && *lang)
			fprintf(d->fp, "lang=\"%s\" ", lang);

		if (dir && *dir)
			fprintf(d->fp, "dir=\"%s\" ", dir);

		fputs("text=\"#FFFFFF\" bgcolor=\"", d->fp);

		hash_colour(d->fp, pg->colour_map[pg->screen_colour]);

		fputs("\">" LF, d->fp);
	}

	if (ferror(d->fp)) {
		write_error(e);
		return FALSE;
	}

	d->foreground	= WHITE;
	d->background	= pg->screen_colour;
	d->underline	= FALSE;
	d->bold		= FALSE;
	d->italic	= FALSE;
	d->flash	= FALSE;
	d->span		= FALSE;
	d->link		= FALSE;

	return TRUE;
}

static bool
html_output(vbi_export *e, FILE *fp, char *name, struct fmt_page *pgp)
{
	html_data *d = (html_data *) e->data;
	struct fmt_page pg;
	attr_char *acp;
	int i, j;

	pg = *pgp;

#if TEST
	d->underline	= FALSE;
	d->bold		= FALSE;
	d->italic	= FALSE;
	d->flash	= FALSE;
#endif

	d->styles = &d->def;
	d->def.next = NULL;
	d->def.ref_count = 2;
	d->def.foreground = d->foreground;
	d->def.background = d->background;

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
				d->underline = rand() & 1;
			if ((rand() & 15) == 1)
				d->bold	  = rand() & 1;
			if ((rand() & 15) == 2)
				d->italic = rand() & 1;
			if ((rand() & 15) == 3)
				d->flash  = rand() & 1;
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
					ac.link = acp[j - blank].link;
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
				ac.link = acp[pg.columns - blank].link;
				acp[pg.columns - blank] = ac;
				blank--;
			}
		}

		for (j = 0; j < pg.columns; j++) {
			attr_char ac = acp[j];
			style *s, **sp;

			for (sp = &d->styles; (s = *sp); sp = &s->next) {
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

	{
		char *s;
		size_t size;
		FILE *fp;

		fp = open_memstream(&s, &size);

		if (pg.pgno < 0x100)
			fprintf(fp, "<title lang=\"en\">");
		else
			/*
			 *  "lang=\"en\" refers to "Teletext Page..." below,
			 *  specify "de", "fr", "es" etc.
			 */
			fprintf(fp, _("<title lang=\"en\">"));

		if (e->network.name[0]) {
			escaped_fputs(fp, e->network.name);
			putc(' ', fp);
		}

		if (pg.pgno < 0x100)
			fprintf(fp, "Closed Caption"); /* no i18n, is a proper name */
		else if (pg.subno != ANY_SUB)
			fprintf(fp, _("Teletext Page %3x.%x"), pg.pgno, pg.subno);
		else
			fprintf(fp, _("Teletext Page %3x"), pg.pgno);

		fputs("</title>", fp);

		if (fclose(fp)) {
			free(s);
			write_error(e);
			return FALSE;
		}

		if (!header(e, fp, name, &pg, s)) {
			free(s);
			return FALSE;
		}

		free(s);
	}

	fputs("<pre>", d->fp);

	d->underline  = FALSE;
	d->bold	      = FALSE;
	d->italic     = FALSE;
	d->flash      = FALSE;
	d->span	      = FALSE;
	d->link	      = FALSE;

	/* XXX this can get extremely large and ugly, should be improved. */
	for (acp = pg.text, i = 0; i < pg.rows; acp += pg.columns, i++) {
		for (j = 0; j < pg.columns; j++) {
			int code;


			if ((acp[j].glyph != 0x20 && acp[j].foreground != d->foreground)
			    || acp[j].background != d->background) {
				style *s;
				int ord;

				if (d->flash)
					fputs(html_flash[0], d->fp);
				if (d->italic)
					fputs(html_italic[0], d->fp);
				if (d->bold)
					fputs(html_bold[0], d->fp);
				if (d->underline)
					fputs(html_underline[0], d->fp);
				if (d->span)
					fputs("</span>", d->fp);

				d->underline  = FALSE;
				d->bold	      = FALSE;
				d->italic     = FALSE;
				d->flash      = FALSE;

				for (s = d->styles, ord = 0; s; s = s->next)
					if (s->ref_count > 1) {
						if ((acp[j].glyph == 0x20 || s->foreground == acp[j].foreground)
						    && s->background == acp[j].background)
							break;
						ord++;
					}

				if (s != &d->def) {
					if (d->link != acp[j].link) {
						vbi_link link;

						vbi_resolve_link(pgp, j, i, &link);

						switch (link.type) {
						case VBI_LINK_HTTP:
						case VBI_LINK_FTP:
						case VBI_LINK_EMAIL:
							fprintf(d->fp, "<a href=\"%s\">", link.url);
							d->link = TRUE;

						default:
							break;
						}
					}

					if (s && !d->headerless) {
						d->foreground = s->foreground;
						d->background = s->background;
						fprintf(d->fp, "<span class=\"c%d\">", ord);
					} else {
						d->foreground = acp[j].foreground;
						d->background = acp[j].background;
						fputs("<span style=\"color:", d->fp);
						hash_colour(d->fp, pg.colour_map[d->foreground]);
						fputs(";background-color:", d->fp);
						hash_colour(d->fp, pg.colour_map[d->background]);
						fputs("\">", d->fp);
					}

					d->span = TRUE;
				} else {
					d->foreground = s->foreground;
					d->background = s->background;
					d->span = FALSE;
				}
			}
			
			if (acp[j].underline != d->underline
			    || acp[j].bold != d->bold
			    || acp[j].italic != d->italic
			    || acp[j].flash != d->flash) {
				if (d->flash)
					fputs(html_flash[0], d->fp);
				if (d->italic)
					fputs(html_italic[0], d->fp);
				if (d->bold)
					fputs(html_bold[0], d->fp);
				if (d->underline)
					fputs(html_underline[0], d->fp);

				d->underline  = FALSE;
				d->bold	      = FALSE;
				d->italic     = FALSE;
				d->flash      = FALSE;
			}

			if (d->link != acp[j].link) {
				if (d->link) {
					fputs("</a>", d->fp);
					d->link = FALSE;
				} else {
					vbi_link link;

					vbi_resolve_link(pgp, j, i, &link);

					switch (link.type) {
					case VBI_LINK_HTTP:
					case VBI_LINK_FTP:
					case VBI_LINK_EMAIL:
						fprintf(d->fp, "<a href=\"%s\">", link.url);
						d->link = TRUE;

					default:
						break;
					}
				}
			}

			if (acp[j].underline != d->underline) {
				d->underline = acp[j].underline;
				fputs(html_underline[d->underline], d->fp);
			}

			if (acp[j].bold != d->bold) {
				d->bold = acp[j].bold;
				fputs(html_bold[d->bold], d->fp);
			}

			if (acp[j].italic != d->italic) {
				d->italic = acp[j].italic;
				fputs(html_italic[d->italic], d->fp);
			}

			if (acp[j].flash != d->flash) {
				d->flash = acp[j].flash;
				fputs(html_flash[d->flash], d->fp);
			}

#if TEST
			if (!(rand() & 15))
				code = glyph_iconv(cd, 0x100 + (rand() & 0xFF), D->gfx_chr);
			else
#endif
			code = glyph_iconv(d->cd, acp[j].glyph, d->gfx_chr);

			escaped_fputc(d->fp, code);
		}

		fputc('\n', d->fp);
	}

	if (d->flash)
		fputs(html_flash[0], d->fp);
	if (d->italic)
		fputs(html_italic[0], d->fp);
	if (d->bold)
		fputs(html_bold[0], d->fp);
	if (d->underline)
		fputs(html_underline[0], d->fp);
	if (d->span)
		fputs("</span>", d->fp);

	fputs("</pre>", d->fp);

	{
		style *s;

		while ((s = d->styles)) {
			d->styles = s->next;
			if (s != &d->def)
				free(s);
		}
	}

	if (!d->headerless)
		fputs(LF "</body>" LF "</html>", d->fp);

	fputc('\n', d->fp);

	iconv_close(d->cd);

	if (ferror(d->fp)) {
		write_error(e);
		return FALSE;
	}

	if (d->name && fclose(d->fp)) {
		d->fp = NULL;
		write_error(e);
		return FALSE;
	}

	return TRUE;
}

vbi_export_module_priv
export_html = {
	.pub = {
		.keyword	= "html",
		.label		= N_("HTML"),
		.tooltip	= N_("Export this page as HTML page"),
	},

	.extension		= "html",
	.options		= html_opts,
	.local_size		= sizeof(struct html_data),
	.set_option		= html_set_option,
	.output			= html_output,
};

VBI_AUTOREG_EXPORT_MODULE(export_html)
