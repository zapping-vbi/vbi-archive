/*
 *  libzvbi - Closed Caption and Teletext HTML export functions
 *
 *  Copyright (C) 2001, 2002 Michael H. Schimek
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

/* $Id: exp-html.c,v 1.6.2.5 2004-03-31 00:41:34 mschimek Exp $ */

#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <setjmp.h>

#include "lang.h"
#include "intl-priv.h"
#include "export.h"
#include "vt.h"
#include "vbi.h"

typedef struct {
	vbi_char		ac;
	unsigned int		id;
	unsigned int		ref;
} style;

typedef struct {
	char *			buffer;
        char *			bp;
	char *			end;
} vec;

typedef struct html_instance {
	vbi_export		export;

	/* Options */
	unsigned int		gfx_chr;
	vbi_bool		ascii_art;
	vbi_bool		color;
	vbi_bool		headerless;

	FILE *			fp;

	jmp_buf			main;

	vbi_char		cac;

	vbi_bool		in_span;
	vbi_bool		in_hyperlink;
	vbi_bool		in_pdc_link;

	vec			text;
	vec			style;

	vbi_link		link;
	vbi_preselection	pdc;
} html_instance;

static vbi_export *
html_new			(void)
{
	html_instance *html;

	if (!(html = calloc (1, sizeof (*html))))
		return NULL;

	return &html->export;
}

static void
html_delete			(vbi_export *		e)
{
	html_instance *html = PARENT (e, html_instance, export);

	free (html->text.buffer);
	free (html->style.buffer);

	free (html);
}

static const vbi_option_info
option_info [] = {
	VBI_OPTION_STRING_INITIALIZER
	("gfx_chr", N_("Graphics char"),
	 "#", N_("Replacement for block graphic characters: "
		 "a single character or decimal (32) or hex (0x20) code")),
	VBI_OPTION_BOOL_INITIALIZER
	("ascii_art", N_("ASCII art"),
	 FALSE, N_("Replace graphic characters by ASCII art")),
	VBI_OPTION_BOOL_INITIALIZER
	("color", N_("Color (CSS)"),
	 TRUE, N_("Store the page colors using CSS attributes")),
	VBI_OPTION_BOOL_INITIALIZER
	("header", N_("HTML header"),
	 TRUE, N_("Include HTML page header"))
};

#define KEYWORD(str) (0 == strcmp (keyword, str))

static vbi_bool
option_get			(vbi_export *		e,
				 const char *		keyword,
				 vbi_option_value *	value)
{
	html_instance *html = PARENT (e, html_instance, export);

	if (KEYWORD ("gfx_chr")) {
		if (!(value->str = vbi_export_strdup (e, NULL, "x")))
			return FALSE;

		value->str[0] = html->gfx_chr;
	} else if (KEYWORD ("ascii_art")) {
		value->num = html->ascii_art;
	} else if (KEYWORD ("color")) {
		value->num = html->color;
	} else if (KEYWORD ("header")) {
		value->num = !html->headerless;
	} else {
		vbi_export_unknown_option (e, keyword);
		return FALSE;
	}

	return TRUE;
}

static vbi_bool
option_set			(vbi_export *		e,
				 const char *		keyword,
				 va_list		ap)
{
	html_instance *html = PARENT (e, html_instance, export);

	if (KEYWORD ("gfx_chr")) {
		const char *string = va_arg (ap, const char *);
		char *s;
		int value;

		if (!string || !string[0]) {
			vbi_export_invalid_option (e, keyword, string);
			return FALSE;
		} else if (1 == strlen (string)) {
			value = string[0];
		} else {
			value = strtol (string, &s, 0);

			if (s == string)
				value = string[0];
		}

		html->gfx_chr = (value < 0x20 || value > 0xE000) ?
			0x20 : value;
	} else if (KEYWORD ("ascii_art")) {
		html->ascii_art = !!va_arg (ap, vbi_bool);
	} else if (KEYWORD ("color")) {
		html->color = !!va_arg (ap, vbi_bool);
	} else if (KEYWORD ("header")) {
		html->headerless = !va_arg (ap, vbi_bool);
	} else {
		vbi_export_unknown_option (e, keyword);
		return FALSE;
	}

	return TRUE;
}

static void
extend				(html_instance *	html,
				 vec *			v,
				 unsigned int		incr,
				 unsigned int		size)
{
	char *buffer;
	unsigned int n;

	n = (v->end - v->buffer + incr) * size;

	if (!(buffer = realloc (v->buffer, n))) {
		longjmp (html->main, -1);
	}

	v->bp = buffer + (v->bp - v->buffer);
	v->buffer = buffer;
	v->end = buffer + n;
}

static void
nputs				(html_instance *	html,
				 const char *		s,
				 unsigned int		n)
{
	if (html->text.bp + n > html->text.end)
		extend (html, &html->text, 4096, 1);

	memcpy (html->text.bp, s, n);

	html->text.bp += n;
}

vbi_inline void
cputs				(html_instance *	html,
				 const char *		s)
{
	nputs (html, s, strlen (s));
}

#ifdef __GNUC__
#define puts(html, s)							\
	(__builtin_constant_p (s) ?					\
	 nputs (html, s, (sizeof (s) - 1) * sizeof (char)) :		\
	 cputs (html, s))
#else
#define puts cputs
#endif

static void
putwc				(html_instance *	html,
				 int			c,
				 vbi_bool		escape)
{
	uint8_t *d;

	if (escape) {
		switch (c) {
		case '<':
			puts (html, "&lt;");
			return;

		case '>':
			puts (html, "&gt;");
			return;

		case '&':
			puts (html, "&amp;");
			return;

		default:
			break;
		}
	}

	if (html->text.bp + 3 > html->text.end)
		extend (html, &html->text, 4096, 1);

	d = html->text.bp;

	if (c < 0x80) {
		*d = c;
		html->text.bp = d + 1;
        } else if (c < 0x800) {
		d[0] = 0xC0 | (c >> 6);
		d[1] = 0x80 | (c & 0x3F);
		html->text.bp = d + 2;
	} else {
		d[0] = 0xE0 | (c >> 12);
		d[1] = 0x80 | ((c >> 6) & 0x3F);
		d[2] = 0x80 | (c & 0x3F);
		html->text.bp = d + 3;
	}
}

static void
puts_escape			(html_instance *	html,
				 const char *		s)
{
	while (*s) {
		putwc (html, *s, TRUE);
		++s;
	}
}

static void
puts_printf			(html_instance *	html,
				 const char *		template,
				 ...)
{
	char buffer[1024];
	va_list ap;
	int n;

	va_start (ap, template);
	n = vsnprintf (buffer, sizeof (buffer), template, ap);
	va_end (ap);

	if (n < 0 && n > (int) sizeof (buffer) - 1) {
		longjmp (html->main, -1);
	}

	puts (html, buffer);
}

static void
puts_color			(html_instance *	html,
				 const char *		label,
				 vbi_rgba		color)
{
	puts_printf (html, "%s#%02x%02x%02x",
		     label, VBI_R (color), VBI_G (color), VBI_B (color));
}

static void
attr				(html_instance *	html,
				 const vbi_page *       pg,
				 const vbi_char *	ac)
{
	const char *bp1;
	const vbi_char *body;

	bp1 = html->text.bp;
	body = &((style *) html->style.buffer)->ac;

	if (ac->foreground != body->foreground)
		puts_color (html, "color:",
			    pg->color_map[ac->foreground]);

	if (ac->background != body->background) {
		if (html->text.bp > bp1)
			puts (html, ";");
		puts_color (html, "background-color:",
			    pg->color_map[ac->background]);
	}

	if (ac->attr & VBI_UNDERLINE) {
		if (html->text.bp > bp1)
			puts (html, ";");
		puts (html, "text-decoration:underline");
	}

	if (ac->attr & VBI_BOLD) {
		if (html->text.bp > bp1)
			puts (html, ";");
		puts (html, "font-weight:bold");
	}

	if (ac->attr & VBI_ITALIC) {
		if (html->text.bp > bp1)
			puts (html, ";");
		puts (html, "font-style:italic");
	}

	if (ac->attr & VBI_FLASH) {
		if (html->text.bp > bp1)
			puts (html, ";");
		puts (html, "text-decoration:blink");
	}
}

static void
flush				(html_instance *	html)
{
	ssize_t n;
	ssize_t r;

	n = html->text.bp - html->text.buffer;
	r = fwrite (html->text.buffer, 1, n, html->fp);

	if (n != r) {
		vbi_export_write_error (&html->export);
		longjmp (html->main, -1);
	}

	html->text.bp = html->text.buffer;
}

vbi_inline vbi_bool
same_style			(const vbi_char *	ac1,
				 const vbi_char *	ac2)
{
	if (ac1->background != ac2->background
	    || ((ac1->attr ^ ac2->attr) & VBI_UNDERLINE))
		return FALSE;

	if (0x0020 == ac1->unicode)
		return TRUE;

	if (ac1->foreground != ac2->foreground
	    || ((ac1->attr ^ ac2->attr)
		& (VBI_BOLD | VBI_ITALIC | VBI_FLASH)))
		return FALSE;

	return TRUE;
}

vbi_inline void
style_gen			(html_instance *	html,
				 vbi_page *		dpg,
				 const vbi_page *	spg,
				 vbi_bool		conceal)
{
	vbi_char *dp;
	vbi_char *dend;
	const vbi_char *sp;
	style *s;
	style *s0;
	unsigned int size;

	size = spg->rows * spg->columns;

	dend = dpg->text + size;
	sp = spg->text + size - 1;

	for (dp = dend - 1; dp >= dpg->text; --sp, --dp) {
		*dp = *sp;

		if (((dp->attr & VBI_CONCEAL) && conceal)
		    || dp->size > VBI_DOUBLE_SIZE) {
			dp->unicode = 0x0020;
			dp->attr &= ~(VBI_LINK | VBI_PDC);
		}

		if (0x0020 == dp->unicode
		    || 0x00A0 == dp->unicode) {
			dp->unicode = 0x0020;

			/* When the character is a space we can merge
			   foreground and text style changes with an
			   earlier background change. */
			if (dp + 1 < dend
			    && !((dp->attr ^ dp[1].attr) & VBI_UNDERLINE)
			    && dp->background == dp[1].background) {
				COPY_SET_MASK (dp->attr, dp[1].attr,
					  VBI_BOLD | VBI_ITALIC | VBI_FLASH);
				dp->foreground	= dp[1].foreground;
			}
		}
	}

	if (html->headerless)
		return;

	/* Body style. */

	if (!html->style.buffer)
		extend (html, &html->style, 32, sizeof (style));

	s = (style *) html->style.buffer;

	CLEAR (s->ac);

	s->ac.foreground = VBI_WHITE;
	s->ac.background = spg->screen_color;
	s->id = 0;

	html->style.bp = (char *)(s + 1);

	/* Text style. */

	for (dp = dpg->text; dp < dend; ++dp) {
		s0 = s;

		do {
			if (same_style (dp, &s->ac)) {
				++s->ref;
				goto next;
			}

			if (++s >= (style *) html->style.bp)
				s = (style *) html->style.buffer;
		} while (s != s0);

		if (html->style.bp >= html->style.end)
			extend (html, &html->style, 32, sizeof (style));

		s = (style *) html->style.bp;
		html->style.bp += sizeof (style);

		s->ac = *dp;
		s->id = s - (style *) html->style.buffer;
		s->ref = 1;

	next:
		;
	}
}

vbi_inline void
title				(html_instance *	html,
				 const vbi_page *	pg)
{
	if (pg->pgno < 0x100) {
		puts (html, "<title lang=\"en\">");
	} else {
		/* TRANSLATORS: "lang=\"en\" refers to the page title
		   "Teletext Page ...". Please specify "de", "fr", "es"
		   etc. */
		puts (html, _("<title lang=\"en\">"));
	}

	if (html->export.network) {
		puts_escape (html, html->export.network);
		putwc (html, ' ', FALSE);
	}

	if (pg->pgno < 0x100) {
		puts (html, "Closed Caption");
	} else if (pg->subno != VBI_ANY_SUBNO) {
		puts_printf (html, _("Teletext Page %3x.%x"),
			     pg->pgno, pg->subno);
	} else {
		puts_printf (html, _("Teletext Page %3x"),
			     pg->pgno);
	}

	puts (html, "</title>\n");
}

vbi_inline void
header				(html_instance *	html,
				 const vbi_page *	pg)
{
	static const vbi_character_set *cs;
	const char *lang;
	const char *dir;

	cs = vbi_page_character_set (pg, 0);

	if (!cs || !cs->language_code[0]) {
		lang = "en";
		dir = NULL;
	} else {
		/* Could we guess [0, 1, 2] from nuid? */
		lang = cs->language_code[0];
		dir = NULL; /* Hebrew, Arabic visually ordered */
	}

	puts (html,
	      "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML "
	      "4.0 Transitional//EN\" "
	      "\"http://www.w3.org/TR/REC-html40/loose.dtd\">\n"
	      "<html>\n"
	      "<head>\n"
	      "<meta name=\"generator\" lang=\"en\" content=\"");

	puts_escape (html, html->export.creator);

	puts (html,
	      "\">\n"
	      "<meta http-equiv=\"Content-Type\" "
	      "content=\"text/html; charset=utf-8\">\n");

	if (html->color) {
		style *s;

		puts (html, "<style type=\"text/css\">\n<!--\n");

		s = (style *) html->style.buffer;
		puts_color (html, "body {color:",
			    pg->color_map[s->ac.foreground]);
		puts_color (html, ";background-color:",
			    pg->color_map[s->ac.background]);
		puts (html, "}\n");

		for (++s; s < (style *) html->style.bp; ++s)
			if (s->ref > 1) {
				puts_printf (html, "span.c%u {", s->id);
				attr (html, pg, &s->ac);
				puts (html, "}\n");
			}

		puts (html, "//-->\n</style>\n");
	}

	title (html, pg);

	puts (html, "</head>\n<body");

	if (lang && *lang) {
		puts (html, " lang=\"");
		puts (html, lang);
		puts (html, "\"");
	}

	if (dir && *dir) {
		puts (html, " dir=\"");
		puts (html, dir);
		puts (html, "\"");
	}

	puts (html, ">\n");
}

vbi_inline const style *
span_start			(html_instance *	html,
				 const vbi_page *	pg,
				 const vbi_char *	acp,
				 const style *		s0)
{
	const style *s;

	if (html->headerless)
		goto inline_style;

	s = s0;

	while (!same_style (acp, &s->ac)) {
		if (++s >= (style *) html->style.bp)
			s = (style *) html->style.buffer;
		if (s == s0)
			goto inline_style;
	}

	if (s->id > 0) {
		if (1 == s->ref)
			goto inline_style;

		puts_printf (html, "<span class=\"c%u\">", s->id);
		html->in_span = TRUE;
	} /* else body style */

	html->cac = s->ac;
	return (const style *) html->style.buffer;
	return s;

 inline_style:
	html->cac = *acp;

	puts (html, "<span style=\"");
	attr (html, pg, &html->cac);
	puts (html, "\">");

	html->in_span = TRUE;

	return (const style *) html->style.buffer;
}

static void
link_end			(html_instance *	html,
				 vbi_bool		pdc)
{
	vbi_bool success;

	putwc (html, 0, FALSE);

	if (pdc) {
		success = html->export.pdc_function
			(&html->export, html->export.pdc_user_data,
			 html->fp, &html->pdc, html->text.buffer);

		html->in_pdc_link = FALSE;
	} else {
		success = html->export.link_function
			(&html->export, html->export.link_user_data,
			 html->fp, &html->link, html->text.buffer);

		html->in_hyperlink = FALSE;
	}

	html->text.bp = html->text.buffer;

	if (!success)
		longjmp (html->main, -1);
}

static vbi_bool
export(vbi_export *e, FILE *fp, const vbi_page *pg)
{
	html_instance *html = PARENT(e, html_instance, export);
	vbi_page page;
	vbi_char *acp;
	vbi_char *acpend;
	const style *s;
	unsigned int row;
	unsigned int column;

	if (setjmp (html->main)) {
		return FALSE;
	}

	style_gen (html, &page, pg, !e->reveal);

	html->fp = fp;

	html->text.bp = html->text.buffer;

	if (!html->headerless)
		header (html, pg);

	puts (html, "<pre>");

	s = (style *) html->style.buffer;

	html->cac = s->ac;

	html->in_span = FALSE;
	html->in_hyperlink = FALSE;
	html->in_pdc_link = FALSE;

	row = 0;
	column = 0;

	acpend = page.text + pg->rows * pg->columns;

	for (acp = page.text; acp < acpend; ++acp) {
		if ((!!(acp->attr & VBI_LINK)) != html->in_hyperlink
		    && e->link_function) {
			if (html->in_span) {
				puts (html, "</span>");
				html->cac = ((style *) html->style.buffer)->ac;
				html->in_span = FALSE;
			}

			if (html->in_hyperlink)
				link_end (html, FALSE);

			if (acp->attr & VBI_LINK) {
				vbi_page_hyperlink (pg, &html->link,
						    column, row);
				flush (html);
				html->in_hyperlink = TRUE;
			}
		}

		if ((!!(acp->attr & VBI_PDC)) != html->in_pdc_link
		    && e->pdc_function) {
			if (html->in_span) {
				puts (html, "</span>");
				html->cac = ((style *) html->style.buffer)->ac;
				html->in_span = FALSE;
			}

			if (html->in_pdc_link)
				link_end (html, TRUE);

			if (acp->attr & VBI_PDC) {
				vbi_page_pdc_link (pg, &html->pdc,
						   column, row);
				flush (html);
				html->in_pdc_link = TRUE;
			}
		}

		if (html->color && !same_style (acp, &html->cac)) {
			if (html->in_span) {
				puts (html, "</span>");
				html->in_span = FALSE;
			}

			s = span_start (html, pg, acp, s);
		}

		if (vbi_is_print (acp->unicode)) {
			putwc (html, acp->unicode, TRUE);
		} else if (vbi_is_gfx (acp->unicode)) {
			if (html->ascii_art) {
				unsigned int c;

				c = vbi_teletext_ascii_art (acp->unicode);

				if (vbi_is_print (c))
					putwc (html, c, TRUE);
				else
					putwc (html, html->gfx_chr, TRUE);
			} else {
				putwc (html, html->gfx_chr, TRUE);
			}
		} else {
			putwc (html, 0x0020, FALSE);
		}

		if (++column == pg->columns) {
			if (html->text.bp > html->text.buffer
			    && 0x20 == html->text.bp[-1]) {
				--html->text.bp;
				puts (html, "&nbsp;\n"); 
			} else {
				puts (html, "\n");
			}

			column = 0;
			++row;
		}
	}

	if (html->in_span)
		puts (html, "</span>");

	if (html->in_hyperlink)
		link_end (html, FALSE);

	if (html->in_pdc_link)
		link_end (html, TRUE);

	puts (html, "</pre>\n");

	if (!html->headerless)
		puts (html, "</body>\n</html>\n");

	flush (html);

	return TRUE;
}

static const vbi_export_info
export_info = {
	.keyword		= "html",
	.label			= N_("HTML"),
	.tooltip		= N_("Export this page as HTML page"),

	.mime_type		= "text/html",
	.extension		= "html,htm",
};

const vbi_export_module
vbi_export_module_html = {
	.export_info		= &export_info,

	._new			= html_new,
	._delete		= html_delete,

	.option_info		= option_info,
	.option_info_size	= N_ELEMENTS (option_info),

	.option_get		= option_get,
	.option_set		= option_set,

	.export			= export
};
