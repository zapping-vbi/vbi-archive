/*
 *  Zapzilla - Teletext HTML export functions
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

/* $Id: exp-html.c,v 1.9 2001-02-18 07:37:26 mschimek Exp $ */

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

/* future */
#undef _
#define _(String) (String)

struct html_data	// private data in struct export
{
  u8 gfx_chr;
  u8 bare;
};

#define D  ((struct html_data *)e->data)

static int
html_open(struct export *e)
{
    D->gfx_chr = '#';
    D->bare = 0;
    //e->reveal=1;	// the default should be the same for all formats.
    return 0;
}

static int
html_option(struct export *e, int opt, char *arg)
{
  switch (opt)
    {
    case 1: // gfx-chr=
	/* Supposed to be ASCII */
	if (*arg < 0x20 || *arg > 0x7E)
		D->gfx_chr = 0x20;
	else
	  D->gfx_chr = *arg;
      break;
    case 2: // bare (no headers)
      D->bare=1;
      break;
    }
  return 0;
}

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

static int
html_output(struct export *e, char *name, struct fmt_page *pgp)
{
	struct fmt_page pg;
	char *charset;
	iconv_t cd;
	attr_char *acp;
	int foreground;
	int background;
	bool underline, bold, italic, flash;
	bool span;
	struct stat st;
	FILE *fp;
	int i, j;

	pg = *pgp;

	switch (pg.font[0] - font_descriptors) {
	case 0:	 /* English */
	case 1:	 /* German */
	case 2:	 /* Swedish/Finnish/Hungarian */
	case 3:	 /* Italian */
	case 4:	 /* French */
	case 5:	 /* Portuguese/Spanish */
	case 9:	 /* German */
	case 10: /* Swedish/Finnish/Hungarian */
	case 11: /* Italian */
	case 12: /* French */
	case 16: /* English */
	case 17: /* German */
	case 18: /* Swedish/Finnish/Hungarian */
	case 19: /* Italian */
	case 20: /* French */
	case 21: /* Portuguese/Spanish */
	case 33: /* German */
	default:
		charset = "iso-8859-1";
		break;

	case 6:	 /* Czech/Slovak */
	case 8:	 /* Polish */
	case 14: /* Czech/Slovak */
	case 29: /* Serbian/Croatian/Slovenian */
	case 31: /* Romanian */
	case 38: /* Czech/Slovak */
		charset = "iso-8859-2";
		break;

	case 34: /* Estonian */
	case 35: /* Lettish/Lithuanian */
		charset = "iso-8859-4";
		break;

	case 32: /* Serbian/Croatian */
	case 36: /* Russian/Bulgarian */
	case 37: /* Ukranian */
		charset = "iso-8859-5";
		break;

	case 64: /* Arabic/English */
	case 68: /* Arabic/French */
	case 71: /* Arabic */
	case 87: /* Arabic */
		charset = "iso-8859-6";	/* XXX needs further examination */
		break;

	case 55: /* Greek */
		charset = "iso-8859-7";
		break;

	case 85: /* Hebrew */
		charset = "iso-8859-8";	/* XXX needs further examination */
		break;

	case 22: /* Turkish */
	case 54: /* Turkish */
		charset = "iso-8859-9";
		break;
	}

	if ((cd = iconv_open(charset, "UCS2")) == (iconv_t) -1) {
		export_error(e, "character conversion not supported, should not happen");
		return -1;
	}

#if TEST
	underline  = FALSE;
	bold	   = FALSE;
	italic	   = FALSE;
	flash      = FALSE;
#endif

	for (acp = pg.text, i = 0; i < 25; acp += pg.columns, i++) {
		int blank = 0;

		for (j = 0; j < 40; j++) {
			int glyph = (acp[j].conceal && !e->reveal) ? GL_SPACE : acp[j].glyph;
#if TEST
			acp[j].underline = underline;
			acp[j].bold	 = bold;
			acp[j].italic	 = italic;
			acp[j].flash	 = flash;

			if ((rand() & 15) == 0)
				underline = rand() & 1;
			if ((rand() & 15) == 1)
				bold	  = rand() & 1;
			if ((rand() & 15) == 2)
				italic	  = rand() & 1;
			if ((rand() & 15) == 3)
				flash	  = rand() & 1;
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

			if (blank < 40)
				ac = acp[39 - blank];
			else {
				memset(&ac, 0, sizeof(ac));
				ac.foreground = 7;
			}

			ac.glyph = 0x20;

			while (blank > 0) {
				ac.background = acp[40 - blank].background;
				acp[40 - blank] = ac;
				blank--;
			}
		}
	}

	if (!(fp = fopen(name, "w"))) {
		export_error(e, _("cannot create file '%s': %s"), name, strerror(errno));
		iconv_close(cd);
		return -1;
	}

	if (!D->bare) {
		fprintf(fp,
			"<!DOCTYPE html PUBLIC \"-//W3C//DTD html 4.0 transitional//EN\">" LF
			"<html>" LF "<head>" LF
			"<meta name=\"generator\" content=\"Zapzilla " VERSION "\">" LF
			"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=%s\">" LF
			"<style type=\"text/css\"></style>" LF
			"<title>Teletext Page %3x/%x</title>" LF /* XXX add station-short,
				mind reserved chars (quote) and character set */
			"</head>" LF,
			charset,
			pg.pgno, pg.subno);

		fputs("<body text=\"#FFFFFF\" bgcolor=\"", fp);
		hash_colour(fp, pg.colour_map[pg.screen_colour]);
		fputs("\">" LF "<pre>", fp);
	} else
		fputs("<pre>", fp);

	foreground = 7;
	background = pg.screen_colour;
	underline  = FALSE;
	bold	   = FALSE;
	italic	   = FALSE;
	flash      = FALSE;
	span	   = FALSE;

	/* XXX this can get extremely large and ugly, should be improved. */
	for (acp = pg.text, i = 0; i < 25; acp += pg.columns, i++) {
		for (j = 0; j < 40; j++) {
			int code;

			if (acp[j].foreground != foreground
			    || acp[j].background != background) {
				if (flash)
					fputs(html_flash[0], fp);
				if (italic)
					fputs(html_italic[0], fp);
				if (bold)
					fputs(html_bold[0], fp);
				if (underline)
					fputs(html_underline[0], fp);
				if (span)
					fputs("</span>", fp);

				foreground = acp[j].foreground;
				background = acp[j].background;
				underline  = FALSE;
				bold	   = FALSE;
				italic	   = FALSE;
				flash      = FALSE;

				fputs("<span style=\"color:", fp);
				hash_colour(fp, pg.colour_map[foreground]);
				fputs(";background-color:", fp);
				hash_colour(fp, pg.colour_map[background]);
				fputs("\">", fp);

				span = TRUE;
			}

			if (acp[j].underline != underline
			    || acp[j].bold != bold
			    || acp[j].italic != italic
			    || acp[j].flash != flash) {
				if (flash)
					fputs(html_flash[0], fp);
				if (italic)
					fputs(html_italic[0], fp);
				if (bold)
					fputs(html_bold[0], fp);
				if (underline)
					fputs(html_underline[0], fp);

				underline  = FALSE;
				bold	   = FALSE;
				italic	   = FALSE;
				flash      = FALSE;
			}

			if (acp[j].underline != underline) {
				underline = acp[j].underline;
				fputs(html_underline[underline], fp);
			}

			if (acp[j].bold != bold) {
				bold = acp[j].bold;
				fputs(html_bold[bold], fp);
			}

			if (acp[j].italic != italic) {
				italic = acp[j].italic;
				fputs(html_italic[italic], fp);
			}

			if (acp[j].flash != flash) {
				flash = acp[j].flash;
				fputs(html_flash[flash], fp);
			}

#if TEST
			if (!(rand() & 15))
				code = glyph_iconv(cd, 0x100 + (rand() & 0xFF), D->gfx_chr);
			else
#endif
			code = glyph_iconv(cd, acp[j].glyph, D->gfx_chr);

			if (code < 0) {
				fprintf(fp, "&#%u;", -code);
			} else {
				switch (code) {
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
					fputc(code, fp);
				}
			}
		}

		fputc('\n', fp);
	}

	if (flash)
		fputs(html_flash[0], fp);
	if (italic)
		fputs(html_italic[0], fp);
	if (bold)
		fputs(html_bold[0], fp);
	if (underline)
		fputs(html_underline[0], fp);
	if (span)
		fputs("</span>", fp);

	fputs("</pre>", fp);

	if (!D->bare)
		fputs(LF "</body>" LF "</html>", fp);

	fputc('\n', fp);

	iconv_close(cd);

	fclose(fp);

	if (ferror(fp)) {
		export_error(e, errno ?
			_("error while writing file '%s': %s") :
			_("error while writing file '%s'"), name, strerror(errno));

		if (!stat(name, &st) && S_ISREG(st.st_mode))
			remove(name);

		return -1;
	}

	return 0;
}

static char *html_opts[] =	// module options
{
  "gfx-chr=<char>",             // substitute <char> for gfx-symbols
  "bare",                     // no headers  
   0                      
};

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
