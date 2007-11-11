/*
 *  libzvbi - Closed Caption and Teletext HTML export functions
 *
 *  Copyright (C) 2001, 2002, 2004 Michael H. Schimek
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

/* $Id: exp-html.c,v 1.6.2.15 2007-11-11 03:06:12 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <unistd.h>		/* ssize_t */
#include <setjmp.h>

#include "misc.h"
#ifdef ZAPPING8
#  include "common/intl-priv.h"
#else
#  include "version.h"
#  include "intl-priv.h"
#endif
#include "export-priv.h"	/* vbi3_export */
#include "page.h"		/* vbi3_page */
#include "lang.h"		/* vbi3_ttx_charset, ... */

/* Resources:
   http://www.w3.org/TR/1998/REC-html40-19980424/
   http://www.w3.org/TR/html401/
   http://wiki.svg.org/Inline_SVG (maybe?)
*/

struct style {
	vbi3_char		ac;
	unsigned int		id;
	unsigned int		ref;
};

struct vec {
	char *			buffer;
        char *			bp;
	char *			end;
};

typedef struct html_instance {
	vbi3_export		export;

	/* Options */
	unsigned int		gfx_chr;
	vbi3_bool		ascii_art;
	vbi3_bool		color;
	vbi3_bool		header;

	jmp_buf			main;

	vbi3_char		cac;

	vbi3_bool		in_span;
	vbi3_bool		in_hyperlink;
	vbi3_bool		in_pdc_link;

	struct vec		text;
	struct vec		style;

	vbi3_link		link;
	const vbi3_preselection *pdc;
} html_instance;

static vbi3_export *
html_new			(const _vbi3_export_module *em)
{
	html_instance *html;

	em = em;

	if (!(html = vbi3_malloc (sizeof (*html))))
		return NULL;

	CLEAR (*html);

	vbi3_link_init (&html->link);

	return &html->export;
}

static void
html_delete			(vbi3_export *		e)
{
	html_instance *html = PARENT (e, html_instance, export);

	vbi3_free (html->text.buffer);
	vbi3_free (html->style.buffer);

	vbi3_link_destroy (&html->link);

	vbi3_free (html);
}

static const vbi3_option_info
option_info [] = {
	_VBI3_OPTION_STRING_INITIALIZER
	("gfx_chr", N_("Graphics char"),
	 "#", N_("Replacement for block graphic characters: "
		 "a single character or decimal (32) or hex (0x20) code")),
	_VBI3_OPTION_BOOL_INITIALIZER
	("ascii_art", N_("ASCII art"),
	 FALSE, N_("Replace graphic characters by ASCII art")),
	_VBI3_OPTION_BOOL_INITIALIZER
	("color", N_("Color (CSS)"),
	 TRUE, N_("Store the page colors using CSS attributes")),
	_VBI3_OPTION_BOOL_INITIALIZER
	("header", N_("HTML header"),
	 TRUE, N_("Include HTML page header"))
};

#define KEYWORD(str) (0 == strcmp (keyword, str))

static vbi3_bool
option_get			(vbi3_export *		e,
				 const char *		keyword,
				 vbi3_option_value *	value)
{
	html_instance *html = PARENT (e, html_instance, export);

	if (KEYWORD ("gfx_chr")) {
		if (!(value->str = _vbi3_export_strdup (e, NULL, "x")))
			return FALSE;

		value->str[0] = html->gfx_chr;
	} else if (KEYWORD ("ascii_art")) {
		value->num = html->ascii_art;
	} else if (KEYWORD ("color")) {
		value->num = html->color;
	} else if (KEYWORD ("header")) {
		value->num = html->header;
	} else {
		_vbi3_export_unknown_option (e, keyword);
		return FALSE;
	}

	return TRUE;
}

static vbi3_bool
option_set			(vbi3_export *		e,
				 const char *		keyword,
				 va_list		ap)
{
	html_instance *html = PARENT (e, html_instance, export);

	if (KEYWORD ("gfx_chr")) {
		const char *string = va_arg (ap, const char *);
		char *s;
		int value;

		if (!string || !string[0]) {
			_vbi3_export_invalid_option (e, keyword, string);
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
		html->ascii_art = !!va_arg (ap, vbi3_bool);
	} else if (KEYWORD ("color")) {
		html->color = !!va_arg (ap, vbi3_bool);
	} else if (KEYWORD ("header")) {
		html->header = !!va_arg (ap, vbi3_bool);
	} else {
		_vbi3_export_unknown_option (e, keyword);
		return FALSE;
	}

	return TRUE;
}

static void
extend				(html_instance *	html,
				 struct vec *		v,
				 unsigned int		incr,
				 unsigned int		size)
{
	char *buffer;
	unsigned int n;

	n = (v->end - v->buffer + incr) * size;

	if (!(buffer = vbi3_realloc (v->buffer, n))) {
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

static __inline__ void
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
				 unsigned int		c,
				 vbi3_bool		escape)
{
	char *d;

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
		putwc (html, (unsigned int) *s, TRUE);
		++s;
	}
}

static void
puts_printf			(html_instance *	html,
				 vbi3_bool		escape,
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

	if (escape)
		puts_escape (html, buffer);
	else
		puts (html, buffer);
}

static void
puts_color			(html_instance *	html,
				 const char *		label,
				 vbi3_rgba		color)
{
	puts_printf (html, FALSE, "%s#%02x%02x%02x",
		     label, VBI3_R (color), VBI3_G (color), VBI3_B (color));
}

static void
attr				(html_instance *	html,
				 const vbi3_page *       pg,
				 const vbi3_char *	ac)
{
	const vbi3_char *body;
	unsigned int ct = 0;

	body = &((struct style *) html->style.buffer)->ac;

	if (ac->foreground != body->foreground) {
		puts_color (html, "color:",
			    pg->color_map[ac->foreground]);
		ct = 1;
	}

	if (ac->background != body->background) {
		if (ct)
			puts (html, ";");
		ct = 1;
		puts_color (html, "background-color:",
			    pg->color_map[ac->background]);
	}

	if (ac->attr & VBI3_UNDERLINE) {
		if (ct)
			puts (html, ";");
		ct = 1;
		puts (html, "text-decoration:underline");
	}

	if (ac->attr & VBI3_BOLD) {
		if (ct)
			puts (html, ";");
		ct = 1;
		puts (html, "font-weight:bold");
	}

	if (ac->attr & VBI3_ITALIC) {
		if (ct)
			puts (html, ";");
		ct = 1;
		puts (html, "font-style:italic");
	}

	if (ac->attr & VBI3_FLASH) {
		if (ct)
			puts (html, ";");
		ct = 1;
		puts (html, "text-decoration:blink");
	}
}

static void
flush				(html_instance *	html)
{
	size_t n;

	n = html->text.bp - html->text.buffer;

	if (!vbi3_export_write (&html->export, html->text.buffer, n)) {
		longjmp (html->main, -1);
	}

	html->text.bp = html->text.buffer;
}

static __inline__ vbi3_bool
same_style			(const vbi3_char *	ac1,
				 const vbi3_char *	ac2)
{
	if (ac1->background != ac2->background
	    || ((ac1->attr ^ ac2->attr) & VBI3_UNDERLINE))
		return FALSE;

	if (0x0020 == ac1->unicode)
		return TRUE;

	if (ac1->foreground != ac2->foreground
	    || ((ac1->attr ^ ac2->attr)
		& (VBI3_BOLD | VBI3_ITALIC | VBI3_FLASH)))
		return FALSE;

	return TRUE;
}

static void
style_gen			(html_instance *	html,
				 vbi3_page *		dpg,
				 const vbi3_page *	spg,
				 vbi3_bool		conceal)
{
	vbi3_char *dp;
	vbi3_char *dend;
	const vbi3_char *sp;
	struct style *s;
	struct style *s0;
	unsigned int size;

	size = spg->rows * spg->columns;

	dend = dpg->text + size;
	sp = spg->text + size - 1;

	for (dp = dend - 1; dp >= dpg->text; --sp, --dp) {
		*dp = *sp;

		if (((dp->attr & VBI3_CONCEAL) && conceal)
		    || dp->size > VBI3_DOUBLE_SIZE) {
			dp->unicode = 0x0020;
			dp->attr &= ~(VBI3_LINK | VBI3_PDC);
		}

		if (0x0020 == dp->unicode
		    || 0x00A0 == dp->unicode) {
			dp->unicode = 0x0020;

			/* When the character is a space we can merge
			   foreground and text style changes with an
			   earlier background change. */
			if (dp + 1 < dend
			    && !((dp->attr ^ dp[1].attr) & VBI3_UNDERLINE)
			    && dp->background == dp[1].background) {
				COPY_SET_MASK (dp->attr, dp[1].attr,
					  VBI3_BOLD | VBI3_ITALIC | VBI3_FLASH);
				dp->foreground	= dp[1].foreground;
			}
		}
	}

	/* Body style. */

	if (!html->style.buffer)
		extend (html, &html->style, 32, sizeof (struct style));

	s = (struct style *) html->style.buffer;

	CLEAR (s->ac);

	s->ac.foreground = VBI3_WHITE;
	s->ac.background = spg->screen_color;
	s->id = 0;

	html->style.bp = (char *)(s + 1);

	if (!html->header)
		return;

	/* Text style. */

	for (dp = dpg->text; dp < dend; ++dp) {
		s0 = s;

		do {
			if (same_style (dp, &s->ac)) {
				++s->ref;
				goto next;
			}

			if (++s >= (struct style *) html->style.bp)
				s = (struct style *) html->style.buffer;
		} while (s != s0);

		if (html->style.bp >= html->style.end)
			extend (html, &html->style, 32,
				sizeof (struct style));

		s = (struct style *) html->style.bp;
		html->style.bp += sizeof (struct style);

		s->ac = *dp;
		s->id = s - (struct style *) html->style.buffer;
		s->ref = 1;

	next:
		;
	}
}

static void
title				(html_instance *	html,
				 const vbi3_page *	pg)
{
	if (pg->pgno < 0x100) {
		puts (html, "<title lang=\"en\">");
	} else {
		/* TRANSLATORS: lang=\"en\" refers to the page title
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
	} else if (pg->subno && pg->subno != VBI3_ANY_SUBNO) {
		puts_printf (html, TRUE, _("Teletext Page %3x.%x"),
			     pg->pgno, pg->subno);
	} else {
		puts_printf (html, TRUE, _("Teletext Page %3x"),
			     pg->pgno);
	}

	puts (html, "</title>\n");
}

static void
header				(html_instance *	html,
				 const vbi3_page *	pg)
{
	static const vbi3_ttx_charset *cs;
	const char *lang;
	const char *dir;

	cs = vbi3_page_get_ttx_charset (pg, 0);

	if (!cs || !cs->language_code[0]) {
		lang = "en";
		dir = NULL;
	} else {
		/* Could we guess [0, 1, 2] from network ID? */
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
		struct style *s;

		puts (html, "<style type=\"text/css\">\n<!--\n");

		s = (struct style *) html->style.buffer;
		puts_color (html, "body {color:",
			    pg->color_map[s->ac.foreground]);
		puts_color (html, ";background-color:",
			    pg->color_map[s->ac.background]);
		puts (html, "}\n");

		for (++s; s < (struct style *) html->style.bp; ++s)
			if (s->ref > 1) {
				puts_printf (html, FALSE, "span.c%u {", s->id);
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

static const struct style *
span_start			(html_instance *	html,
				 const vbi3_page *	pg,
				 const vbi3_char *	acp,
				 const struct style *	s0)
{
	const struct style *s;

	if (!html->header)
		goto inline_style;

	s = s0;

	while (!same_style (acp, &s->ac)) {
		if (++s >= (struct style *) html->style.bp)
			s = (struct style *) html->style.buffer;
		if (s == s0)
			goto inline_style;
	}

	if (s->id > 0) {
		if (1 == s->ref)
			goto inline_style;

		puts_printf (html, FALSE, "<span class=\"c%u\">", s->id);
		html->in_span = TRUE;
	} /* else body style */

	html->cac = s->ac;

	return (const struct style *) html->style.buffer;

 inline_style:
	html->cac = *acp;

	puts (html, "<span style=\"");
	attr (html, pg, &html->cac);
	puts (html, "\">");

	html->in_span = TRUE;

	return (const struct style *) html->style.buffer;
}

static void
link_end			(html_instance *	html,
				 vbi3_bool		pdc)
{
	vbi3_bool success;

	putwc (html, 0, FALSE);

	if (VBI3_EXPORT_TARGET_FP == html->export.target) {
		/* In case the client writes through the fp
		   instead of the export buffer functions. */
		vbi3_export_flush (&html->export);
	}

	if (pdc) {
		success = html->export.pdc_callback
			(&html->export,
			 html->export.pdc_user_data,
			 html->pdc,
			 html->text.buffer);

		html->in_pdc_link = FALSE;
	} else {
		html->link.name = html->text.buffer;

		success = html->export.link_callback
			(&html->export,
			 html->export.link_user_data,
			 &html->link);

		html->link.name = NULL;

		html->in_hyperlink = FALSE;
	}

	html->text.bp = html->text.buffer;

	if (!success)
		longjmp (html->main, -1);
}

static vbi3_bool
export				(vbi3_export *		e,
				 const vbi3_page *	pg)
{
	html_instance *html = PARENT(e, html_instance, export);
	vbi3_page page;
	vbi3_char *acp;
	vbi3_char *acpend;
	const struct style *s;
	unsigned int row;
	unsigned int column;

	if (setjmp (html->main)) {
		return FALSE;
	}

	style_gen (html, &page, pg, !e->reveal);

	html->text.bp = html->text.buffer;

	if (html->header)
		header (html, pg);

	puts (html, "<pre>");

	s = (struct style *) html->style.buffer;

	html->cac = s->ac;

	html->in_span = FALSE;
	html->in_hyperlink = FALSE;
	html->in_pdc_link = FALSE;

	row = 0;
	column = 0;

	acpend = page.text + pg->rows * pg->columns;

	for (acp = page.text; acp < acpend; ++acp) {
		if ((!!(acp->attr & VBI3_LINK)) != html->in_hyperlink
		    && e->link_callback) {
			if (html->in_span) {
				puts (html, "</span>");
				html->cac = ((struct style *)
					     html->style.buffer)->ac;
				html->in_span = FALSE;
			}

			if (html->in_hyperlink)
				link_end (html, FALSE);

			if (acp->attr & VBI3_LINK) {
				vbi3_bool r;

				vbi3_link_destroy (&html->link);

				r = vbi3_page_get_hyperlink (pg, &html->link,
							    column, row);

				flush (html);

				html->in_hyperlink = r;
			}
		}

		if ((!!(acp->attr & VBI3_PDC)) != html->in_pdc_link
		    && e->pdc_callback) {
			if (html->in_span) {
				puts (html, "</span>");
				html->cac = ((struct style *)
					     html->style.buffer)->ac;
				html->in_span = FALSE;
			}

			if (html->in_pdc_link)
				link_end (html, TRUE);

			if (acp->attr & VBI3_PDC) {
				html->pdc = vbi3_page_get_pdc_link
					(pg, column, row);

				flush (html);

				if (html->pdc)
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

		if (vbi3_is_print (acp->unicode)) {
			putwc (html, acp->unicode, TRUE);
		} else if (vbi3_is_gfx (acp->unicode)) {
			if (html->ascii_art) {
				unsigned int c;

				c = _vbi3_teletext_ascii_art (acp->unicode);

				if (vbi3_is_print (c))
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

	if (html->header)
		puts (html, "</body>\n</html>\n");

	flush (html);

	return TRUE;
}

static const vbi3_export_info
export_info = {
	.keyword		= "html",
	.label			= N_("HTML"),
	.tooltip		= N_("Export the page as HTML page"),

	.mime_type		= "text/html",
	.extension		= "html,htm",
};

const _vbi3_export_module
_vbi3_export_module_html = {
	.export_info		= &export_info,

	._new			= html_new,
	._delete		= html_delete,

	.option_info		= option_info,
	.option_info_size	= N_ELEMENTS (option_info),

	.option_get		= option_get,
	.option_set		= option_set,

	.export			= export
};

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
