/*
 *  libzvbi - Closed Caption and Teletext
 *            caption / subtitle export functions
 *
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: exp-sub.c,v 1.1.2.11 2007-11-11 03:06:12 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <math.h>		/* floor() */
#include <setjmp.h>

#include "misc.h"
#include "page.h"		/* vbi3_page */
#include "conv.h"
#include "lang.h"		/* vbi3_ttx_charset, ... */
#ifdef ZAPPING8
#  include "common/intl-priv.h"
#else
#  include "version.h"
#  include "intl-priv.h"
#endif
#include "export-priv.h"	/* vbi3_export */

/*
    Resources:

    MPSub format
    http://www.mplayerhq.hu/DOCS/HTML/en/subosd.html

    QuickTime Text
    http://www.apple.com/quicktime/tools_tips/tutorials/texttracks.html
    http://developer.apple.com/documentation/QuickTime/QTFF/QTFFChap4/
	chapter_5_section_2.html

    RealText
    http://service.real.com/help/library/encoders.html

    Understanding SAMI 1.0
    http://msdn.microsoft.com/library/en-us/dnacc/html/atg_samiarticle.asp

    Adding Closed Captions to Digital Media
    http://msdn.microsoft.com/library/en-us/dnwmt/html/wmp7_sami.asp

    SubRip
    http://membres.lycos.fr/subrip/

    SubViewer
    http://www.subviewer.com/
*/

struct vec {
	uint16_t *		buffer;
        uint16_t *		bp;
	uint16_t *		end;
};

enum format {
	FORMAT_MPSUB,
	FORMAT_QTTEXT,
	FORMAT_REALTEXT,
	FORMAT_SAMI,
	FORMAT_SUBRIP,
	FORMAT_SUBVIEWER,
};

typedef struct sub_instance {
	vbi3_export		export;

	jmp_buf			main;

	struct vec		text1;
	struct vec		text2;
	vbi3_iconv_t *		cd;

	enum format		format;
	int			encoding;
	char *			charset;
	char *			font;

	vbi3_bool		have_header;

	vbi3_pgno		last_pgno;

	double			last_timestamp;
	double			last_t2;

	unsigned int		n_pages;
	unsigned int		blank_pages;

	vbi3_char		para_ac;
	vbi3_char		last_ac;

	unsigned int		last_just;

	vbi3_bool		in_span;
	vbi3_bool		in_underline;
	vbi3_bool		in_bold;
	vbi3_bool		in_italic;
} sub_instance;

static const char *
user_encodings [] = {
	N_("ASCII"),
	N_("ISO-8859-1 (Latin-1 Western languages)"),
	N_("ISO-8859-2 (Latin-2 Central and Eastern European languages)"),
	N_("ISO-8859-4 (Latin-3 Baltic languages)"),
	N_("ISO-8859-5 (Cyrillic)"),
	N_("ISO-8859-7 (Greek)"),
	N_("ISO-8859-8 (Hebrew)"),
	N_("ISO-8859-9 (Turkish)"),
	N_("KOI8-R (Russian and Bulgarian)"),
	N_("KOI8-U (Ukranian)"),
	N_("ISO-10646/UTF-8 (Unicode)"),
};

static const char *
iconv_encodings [] = {
	"ASCII",
	"ISO-8859-1",
	"ISO-8859-2",
	"ISO-8859-4",
	"ISO-8859-5",
	"ISO-8859-7",
	"ISO-8859-8",
	"ISO-8859-9",
        "KOI8-R",
	"KOI8-U",
	"UTF-8"
};

static const vbi3_option_info
option_info1 [] = {
	_VBI3_OPTION_MENU_INITIALIZER
	("format", N_("Encoding"),
	 10, user_encodings, N_ELEMENTS (user_encodings), NULL),
        /* one for users, another for programs */
	_VBI3_OPTION_STRING_INITIALIZER
	("charset", NULL, "locale",
	 N_("Character set, for example ISO-8859-1, UTF-8")),
};

static const vbi3_option_info
option_info2 [] = {
	_VBI3_OPTION_MENU_INITIALIZER
	("format", N_("Encoding"),
	 10, user_encodings, N_ELEMENTS (user_encodings), NULL),
        /* one for users, another for programs */
	_VBI3_OPTION_STRING_INITIALIZER
	("charset", NULL, "locale",
	 N_("Character set, for example ISO-8859-1, UTF-8")),
	_VBI3_OPTION_STRING_INITIALIZER
	("font", N_("Font face"), "Tahoma",
	 N_("Name of the font to be encoded into the file")),
};

#undef KEYWORD
#define KEYWORD(str) (0 == strcmp (em->export_info->keyword, str))

static vbi3_export *
sub_new			(const _vbi3_export_module *em)
{
	sub_instance *sub;

	assert (sizeof (user_encodings) == sizeof (iconv_encodings));

	if (!(sub = vbi3_malloc (sizeof (*sub))))
		return NULL;

	CLEAR (*sub);

	if (KEYWORD ("mpsub")) {
		sub->format = FORMAT_MPSUB;
	} else if (KEYWORD ("qttext")) {
		sub->format = FORMAT_QTTEXT;
	} else if (KEYWORD ("realtext")) {
		sub->format = FORMAT_REALTEXT;
	} else if (KEYWORD ("sami")) {
		sub->format = FORMAT_SAMI;
	} else if (KEYWORD ("subrip")) {
		sub->format = FORMAT_SUBRIP;
	} else if (KEYWORD ("subviewer")) {
		sub->format = FORMAT_SUBVIEWER;
	} else {
		assert (!"reached");
	}

	sub->cd = NULL;

	return &sub->export;
}

static void
sub_delete			(vbi3_export *		e)
{
	sub_instance *sub = PARENT (e, sub_instance, export);

	vbi3_free (sub->text1.buffer);
	vbi3_free (sub->text2.buffer);

	vbi3_free (sub->charset);
	vbi3_free (sub->font);

	_vbi3_iconv_close (sub->cd);

	CLEAR (*sub);

	vbi3_free (sub);
}

#undef KEYWORD
#define KEYWORD(str) (0 == strcmp (keyword, str))

static vbi3_bool
option_get			(vbi3_export *		e,
				 const char *		keyword,
				 vbi3_option_value *	value)
{
	sub_instance *sub = PARENT (e, sub_instance, export);

	if (KEYWORD ("format") || KEYWORD ("encoding")) {
		value->num = sub->encoding;
	} else if (KEYWORD ("charset")) {
		const char *codeset;

		codeset = _vbi3_export_codeset (sub->charset);
		value->str = _vbi3_export_strdup (e, NULL, codeset);
		if (!value->str)
			return FALSE;
	} else if (KEYWORD ("font")) {
		value->str = _vbi3_export_strdup (e, NULL, sub->font);
		if (!value->str)
			return FALSE;
	} else {
		_vbi3_export_unknown_option (e, keyword);
		return FALSE;
	}

	return TRUE; /* success */
}

static vbi3_bool
option_set			(vbi3_export *		e,
				 const char *		keyword,
				 va_list		ap)
{
	sub_instance *sub = PARENT (e, sub_instance, export);

	if (KEYWORD ("format") || KEYWORD ("encoding")) {
		unsigned int encoding = va_arg (ap, unsigned int);

		if (encoding >= N_ELEMENTS (user_encodings)) {
			_vbi3_export_invalid_option (e, keyword, encoding);
			return FALSE;
		}

		if (!_vbi3_export_strdup (e, &sub->charset,
					 iconv_encodings[encoding]))
			return FALSE;

		sub->encoding = encoding;
	} else if (KEYWORD ("charset")) {
		const char *string = va_arg (ap, const char *);

		if (!string) {
			_vbi3_export_invalid_option (e, keyword, string);
			return FALSE;
		}

		if (!_vbi3_export_strdup (e, &sub->charset, string))
			return FALSE;
	} else if (KEYWORD ("font")) {
		const char *string = va_arg (ap, const char *);

		if (!string) {
			_vbi3_export_invalid_option (e, keyword, string);
			return FALSE;
		}

		if (!_vbi3_export_strdup (e, &sub->font, string))
			return FALSE;
	} else {
		_vbi3_export_unknown_option (e, keyword);
		return FALSE;
	}

	return TRUE;
}

/* We write UCS-2 into an automatically growing buffer. */
static void
extend				(sub_instance *		sub,
				 struct vec *		v)
{
	uint16_t *buffer;
	unsigned int n;

	n = v->end - v->buffer + 2048;

	if (!(buffer = vbi3_realloc (v->buffer, n * sizeof (*v->buffer)))) {
		longjmp (sub->main, -1);
	}

	v->bp = buffer + (v->bp - v->buffer);
	v->buffer = buffer;
	v->end = buffer + n;
}

static void
putws				(sub_instance *		sub,
				 vbi3_bool		escape,
				 const char *		s);

static void
putwc				(sub_instance *		sub,
				 vbi3_bool		escape,
				 unsigned int		wc)
{
	if (escape) {
		switch (sub->format) {
		case FORMAT_REALTEXT:
		case FORMAT_SAMI:
			switch (wc) {
			case '<':
				putws (sub, FALSE, "&lt;");
				return;

			case '>':
				putws (sub, FALSE, "&gt;");
				return;

			case '&':
				putws (sub, FALSE, "&amp;");
				return;

			default:
				break;
			}

			break;

		default:
			break;
		}
	}

	if (sub->text1.bp >= sub->text1.end)
		extend (sub, &sub->text1);

	*sub->text1.bp++ = wc;
}

static void
putws				(sub_instance *		sub,
				 vbi3_bool		escape,
				 const char *		s)
{
	if (escape) {
		while (*s)
			putwc (sub, TRUE, (unsigned int) *s++);
	} else {
		uint16_t *d;
		unsigned int n;
		
		n = strlen (s);

		if (sub->text1.bp + n > sub->text1.end)
			extend (sub, &sub->text1);

		d = sub->text1.bp;

		while (n-- > 0)
			*d++ = *s++;

		sub->text1.bp = d;
	}
}

static void
wprintf				(sub_instance *		sub,
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

	if (n < 0 || n > (int) sizeof (buffer) - 1) {
		longjmp (sub->main, -1);
	}

	putws (sub, escape, buffer);
}

static void
color				(sub_instance *		sub,
				 const char *		label,
				 vbi3_rgba		color)
{
	switch (sub->format) {
	case FORMAT_QTTEXT:
		wprintf (sub, FALSE, "{%s%u,%u,%u}",
			 label,
			 VBI3_R (color) * 0x0101,
			 VBI3_G (color) * 0x0101,
			 VBI3_B (color) * 0x0101);
		break;

	case FORMAT_REALTEXT:
	case FORMAT_SAMI:
		wprintf (sub, FALSE, "%s#%02x%02x%02x",
			 label,
			 VBI3_R (color),
			 VBI3_G (color),
			 VBI3_B (color));
		break;

	default:
		assert (!"reached");
	}
}

static void
sami_span			(sub_instance *		sub,
				 const vbi3_page *	pg,
				 const vbi3_char *	ac)
{
	unsigned int ct = 0;

	putws (sub, FALSE, "<SPAN style=\"");

	if (ac->foreground != sub->last_ac.foreground) {
		color (sub, "color:",
		       pg->color_map[ac->foreground]);
		ct = 1;
	}

	if (ac->background != sub->last_ac.background) {
		if (ct)
			putwc (sub, FALSE, ';');
		ct = 1;
		color (sub, "background-color:",
		       pg->color_map[ac->background]);
	}

	if (ac->attr & VBI3_UNDERLINE) {
		if (ct)
			putwc (sub, FALSE, ';');
		ct = 1;
		putws (sub, FALSE, "text-decoration:underline");
	}

	if (ac->attr & VBI3_BOLD) {
		if (ct)
			putwc (sub, FALSE, ';');
		ct = 1;
		putws (sub, FALSE, "font-weight:bold");
	}

	if (ac->attr & VBI3_ITALIC) {
		if (ct)
			putwc (sub, FALSE, ';');
		ct = 1;
		putws (sub, FALSE, "font-style:italic");
	}

	if (ac->attr & VBI3_FLASH) {
		if (ct)
			putwc (sub, FALSE, ';');
		ct = 1;
		putws (sub, FALSE, "text-decoration:blink");
	}

	putws (sub, FALSE, "\">");

	sub->last_ac = *ac;

	sub->in_span = TRUE;
}

static void
qt_style			(sub_instance *		sub,
				 const vbi3_page *	pg,
				 const vbi3_char *	ac)
{
	unsigned int attr;

	if (ac->foreground != sub->last_ac.foreground)
		color (sub, "textColor:",
		       pg->color_map[ac->foreground]);

	if (ac->background != sub->last_ac.background)
		color (sub, "backColor:",
		       pg->color_map[ac->background]);

	attr = ac->attr ^ sub->last_ac.attr;

	if (attr) /* reset */
		putws (sub, FALSE, "{plain}");

	if (attr & VBI3_UNDERLINE)
		putws (sub, FALSE, "{underline}");

	if (attr & VBI3_BOLD)
		putws (sub, FALSE, "{bold}");

	if (attr & VBI3_ITALIC)
		putws (sub, FALSE, "{italic}");

	sub->last_ac = *ac;
}

static void
real_style			(sub_instance *		sub,
				 const vbi3_page *	pg,
				 const vbi3_char *	ac)
{
	if (ac->foreground != sub->last_ac.foreground
	    || ac->background != sub->last_ac.background) {
		unsigned int ct = 0;

		putws (sub, FALSE, "<font ");

		if (ac->foreground != sub->last_ac.foreground) {
			color (sub, "color=\"",
			       pg->color_map[ac->foreground]);
			ct = 1;
		}

		if (ac->background != sub->last_ac.background) {
			if (ct)
				putws (sub, FALSE, "\" ");
			color (sub, "bgcolor=\"",
			       pg->color_map[ac->background]);
		}

		putws (sub, FALSE, "\">");

		sub->in_span = TRUE;
	}

	if (ac->attr & VBI3_UNDERLINE) {
		putws (sub, FALSE, "<u>");
		sub->in_underline = TRUE;
	}

	if (ac->attr & VBI3_BOLD) {
		putws (sub, FALSE, "<b>");
		sub->in_bold = TRUE;
	}

	if (ac->attr & VBI3_ITALIC) {
		putws (sub, FALSE, "<i>");
		sub->in_italic = TRUE;
	}

	sub->last_ac = *ac;
}

static void
real_style_end			(sub_instance *		sub)
{
	if (sub->in_italic) {
		putws (sub, FALSE, "</i>");
		sub->in_italic = FALSE;
	}

	if (sub->in_bold) {
		putws (sub, FALSE, "</b>");
		sub->in_bold = FALSE;
	}

	if (sub->in_underline) {
		putws (sub, FALSE, "</u>");
		sub->in_underline = FALSE;
	}

	if (sub->in_span) {
		putws (sub, FALSE, "</font>");
		sub->in_span = FALSE;
	}

	sub->last_ac = sub->para_ac;
}

static void
flush				(sub_instance *		sub)
{
	char *buffer;
	char *d;
	size_t length;

	length = (long)(sub->text1.bp - sub->text1.buffer);

	buffer = vbi3_malloc (length * 8);
	if (NULL == buffer)
		goto failure;

	d = buffer;

	if (!_vbi3_iconv_ucs2 (sub->cd, &d, length * 8,
			       sub->text1.buffer, length))
		goto failure;

	length = d - buffer;

	if (!vbi3_export_write (&sub->export, buffer, length))
		goto failure;

	vbi3_free (buffer);
	buffer = NULL;

	sub->text1.bp = sub->text1.buffer;

	return;

 failure:
	vbi3_free (buffer);
	buffer = NULL;

	longjmp (sub->main, -1);

	return;
}

static void
header				(sub_instance *		sub,
				 const vbi3_page *	pg)
{
	switch (sub->format) {
	case FORMAT_MPSUB:
		putws (sub, FALSE, "TITLE=");

		if (sub->export.network) {
			putws (sub, TRUE, sub->export.network);
			putwc (sub, FALSE, 0x0020);
		}

		if (pg->pgno < 0x100) {
			putws (sub, FALSE, "Closed Caption");
		} else if (pg->subno && pg->subno != VBI3_ANY_SUBNO) {
			wprintf (sub, TRUE, _("Teletext Page %3x.%x"),
				 pg->pgno, pg->subno);
		} else {
			wprintf (sub, TRUE, _("Teletext Page %3x"),
				 pg->pgno);
		}

		putws (sub, FALSE, "\nAUTHOR=");
		putws (sub, TRUE, sub->export.creator);

		/* Times in seconds, not frames. */
		putws (sub, FALSE, "\nFORMAT=TIME\n");

		break;

	case FORMAT_QTTEXT:
	{
		/* Of course they have to use a proprietary
		   numbering scheme... */
		static const char *languages [] = {
			"en", "fr", "de", "it",
			"nl", "es", "da", "pt",
			"nn", "he", "jp", "ar",
			"fi", "el", "is", "mt",
			"tr", "hr", "zh", "ur",
			"th", "ko", "lt", "pl",
			"hu", "et", "lv", "lv",
			"??", "fo", "??", "??",
			"ru", "zh", "??", "ga",
			"sq", "ro", "cs", "sk",
			"sl", "yi", "sr", "mk",
			"bg", "uk",
		};
		static const vbi3_ttx_charset *cs;
		unsigned int lc;

		cs = vbi3_page_get_ttx_charset (pg, 0);

		if (!cs) {
			lc = 0;
		} else {
			for (lc = 0; lc < N_ELEMENTS (languages); ++lc)
				if (0 == strcmp (languages[lc],
						 cs->language_code[0]))
					break;

			if (lc >= N_ELEMENTS (languages))
				lc = 0;
		}

		CLEAR (sub->last_ac);

		sub->last_ac.foreground = 7;
		sub->last_ac.background = 0;

		sub->last_just = 0; /* center */

		wprintf (sub, FALSE,
			 "{QTtext}"
			 "{font:%s}"
			 "{plain}"
			 "{justify:center}",
			 sub->font);

		color (sub, "textColor:", pg->color_map[7]);
		color (sub, "backColor:", pg->color_map[0]);

		if (lc < N_ELEMENTS (languages))
			wprintf (sub, FALSE, "{language:%u}", lc);

		putws (sub, FALSE,
		       "{timeStamps:absolute}"
		       "{timeScale:100}"
		       "\n");

		break;
	}

	case FORMAT_REALTEXT:
	{
		const char *codeset;

		CLEAR (sub->para_ac);

		sub->para_ac.foreground = 7;
		sub->para_ac.background = 0;

		sub->last_just = 0; /* center */

		codeset = _vbi3_export_codeset (sub->charset);

		wprintf (sub, FALSE,
			 "<window "
			 "type=\"generic\" "
			 /* RealPlayer 7+ supports ISO-8859-1 */
			 "version=\"1.2\" "
			 "charset=\"%s\" "
			 "face=\"%s\" ",
			 codeset,
			 sub->font);

		color (sub, "color=\"", pg->color_map[7]);
		color (sub, "\" bgcolor=\"", pg->color_map[0]);

		putws (sub, FALSE, "\">\n");

		break;
	}

	case FORMAT_SAMI:
	{
		static const vbi3_ttx_charset *cs;
		const char *lang;

		lang = "en";

		cs = vbi3_page_get_ttx_charset (pg, 0);

		if (cs && cs->language_code[0])
			lang = cs->language_code[0];

		putws (sub, FALSE,
		       "<SAMI>\n"
		       "<head>\n"
		       "<title>");

		if (sub->export.network) {
			putws (sub, TRUE, sub->export.network);
			putwc (sub, FALSE, 0x0020);
		}

		if (pg->pgno < 0x100) {
			putws (sub, FALSE, "Closed Caption");
		} else if (pg->subno && pg->subno != VBI3_ANY_SUBNO) {
			/* XXX gettext encoding */
			wprintf (sub, TRUE, _("Teletext Page %3x.%x"),
				 pg->pgno, pg->subno);
		} else {
			wprintf (sub, TRUE, _("Teletext Page %3x"),
				 pg->pgno);
		}

		putws (sub, FALSE,
		       "</title>\n"
		       "<SAMIParam>\n"
		       "Metrics {time:ms}\n"
		       "Spec {MSFT:1.0}\n"
		       "</SAMIParam>\n"
		       "<style type=\"text/css\"><!--\n"
		       "P {lang:");

		putws (sub, FALSE, lang);

		putws (sub, FALSE,
		       ";text-align:center;");

		sub->para_ac.foreground = 7;
		sub->para_ac.background = 0;

		color (sub, "color:", pg->color_map[7]);
		color (sub, ";background-color:", pg->color_map[0]);

		putws (sub, FALSE,
		       ";padding:5px}\n"
		       "P.l {text-align:left}\n"
		       "P.r {text-align:right}\n"
		       "--></style>\n"
		       "</head>\n"
		       "<body>\n");

		break;
	}

	case FORMAT_SUBRIP:
		/* Nothing. */
		break;

	case FORMAT_SUBVIEWER:
		wprintf (sub, FALSE,
			 "[INFORMATION]\n"
			 "[TITLE]\n"
			 "[AUTHOR]%s\n"
			 "[SOURCE]",
			 sub->export.creator);

		if (sub->export.network)
			wprintf (sub, FALSE, "%s ", sub->export.network);

		if (pg->pgno < 0x100) {
			putws (sub, FALSE, "Closed Caption");
		} else if (pg->subno && pg->subno != VBI3_ANY_SUBNO) {
			/* XXX gettext encoding */
			wprintf (sub, FALSE, _("Teletext Page %3x.%x"),
				 pg->pgno, pg->subno);
		} else {
			wprintf (sub, FALSE, _("Teletext Page %3x"),
				 pg->pgno);
		}

		/* What's this? I have no idea, just copied. */
		wprintf (sub, FALSE,
			 "\n"
			 "[PRG]\n"
			 "[FILEPATH]\n"
			 "[DELAY]0\n"
			 "[CD TRACK]0\n"
			 "[COMMENT]\n"
			 "[END INFORMATION]\n"
			 "[SUBTITLE]\n"
			 "[COLF]&HFFFFFF,[STYLE]bd,[SIZE]24,[FONT]%s\n",
			 sub->font);

		break;
	}
}

static void
footer				(sub_instance *		sub)
{
	switch (sub->format) {
	case FORMAT_MPSUB:
	case FORMAT_QTTEXT:
	case FORMAT_SUBRIP:
	case FORMAT_SUBVIEWER:
		break;

	case FORMAT_REALTEXT:
		putws (sub, FALSE,
		       "</window>\n");
		break;

	case FORMAT_SAMI:
		putws (sub, FALSE,
		       "</body>\n"
		       "</SAMI>\n");
		break;
	}
}

static void
timestamp			(sub_instance *		sub)
{
	double t1, t1_frac;
	double t2, t2_frac;
	unsigned int s1;
	unsigned int s2;

	/* Time in seconds when the player shall turn the display
	   of this page on and off, relative to the stream start. */
	t1 = sub->last_timestamp
		- sub->export.stream.start_timestamp;
	t2 = sub->export.stream.timestamp
		- sub->export.stream.start_timestamp;

	/* Seconds and fractions (to prevent overflow). */
	s1 = floor (t1);
	t1_frac = t1 - s1;

	s2 = floor (t2);
	t2_frac = t2 - s2;

	switch (sub->format) {
	case FORMAT_MPSUB:
	{
		double delay_time;
		double show_time;

		/* Apparently delay_time must be an integer. We round down
		   and add the fraction to show_time to compensate
		   for the error. */

		delay_time = floor (t1 - sub->last_t2);

		show_time = t2 - t1;
		show_time += t1 - sub->last_t2 - delay_time;

		sub->last_t2 = t2; 

		/* wait hold\n
		   That is how long to wait after the previous
		   page disappeared and how long to display the
		   following page. */
		wprintf (sub, FALSE, "%u %f\n",
			 (unsigned int) delay_time, show_time);
		break;
	}

	case FORMAT_QTTEXT:
		/* [hh:mm:ss.xx]\n  presentation time */
		wprintf (sub, FALSE,
			 "[%02u:%02u:%02u.%02u]\n",
			 s1 / 3600, (s1 / 60) % 60, s1 % 60,
			 (unsigned int)(t1_frac * 100));
		break;

	case FORMAT_REALTEXT:
		/* <time begin="hh:mm:ss.xx" end="hh:mm:ss.xx"/>
		   <clear/> just to be safe. */
		wprintf (sub, FALSE,
			 "<time begin=\"%02u:%02u:%02u.%02u\" "
			 "end=\"%02u:%02u:%02u.%02u\"/><clear/>",
			 s1 / 3600, s1 / 60, s1 % 60,
			 (unsigned int)(t1_frac * 100),
			 s2 / 3600, s2 / 60, s2 % 60,
			 (unsigned int)(t2_frac * 100));
		break;

	case FORMAT_SAMI:
		/* Presentation time in ms since start. */
		/* Note MPlayer cannot handle "start=\"%llu\"". */
		wprintf (sub, FALSE,
			 "<SYNC Start=%llu>",
			 (uint64_t)(t2 /* sic */ * 1000));
		break;

	case FORMAT_SUBRIP:
		/* n  number of page, starting at one.
		   hh:mm:ss,xxx --> hh:mm:ss,xxx\n  display duration */
		/* XXX encoding/charset */
		wprintf (sub, FALSE,
			 "%u\n"
			 "%02u:%02u:%02u,%03u --> "
			 "%02u:%02u:%02u,%03u\n",
			 sub->n_pages + 1,
			 s1 / 3600, (s1 / 60) % 60, s1 % 60,
			 (unsigned int)(t1_frac * 1000),
			 s2 / 3600, (s2 / 60) % 60, s2 % 60,
			 (unsigned int)(t2_frac * 1000));
		break;

	case FORMAT_SUBVIEWER:
		/* hh:mm:ss.xx,hh:mm:ss.xx\n  display duration */
		/* XXX encoding/charset */
		wprintf (sub, FALSE,
			 "%02u:%02u:%02u.%02u,"
			 "%02u:%02u:%02u.%02u\n",
			 s1 / 3600, (s1 / 60) % 60, s1 % 60,
			 (unsigned int)(t1_frac * 100),
			 s2 / 3600, (s2 / 60) % 60, s2 % 60,
			 (unsigned int)(t2_frac * 100));
		break;

	default:
		assert (!"reached");
	}
}

static vbi3_bool
same_style			(sub_instance *		sub,
				 const vbi3_char *	ac1,
				 const vbi3_char *	ac2)
{
	if (ac1->background != ac2->background)
		return FALSE;

	if (0x0020 == ac1->unicode)
		return TRUE;

	if (ac1->foreground != ac2->foreground)
		return FALSE;

	switch (sub->format) {
	case FORMAT_MPSUB:
	case FORMAT_SUBRIP:
	case FORMAT_SUBVIEWER:
		/* Has no text attributes. */
		break;

	case FORMAT_QTTEXT:
	case FORMAT_REALTEXT:
		if ((ac1->attr ^ ac2->attr)
		    & (VBI3_UNDERLINE | VBI3_BOLD | VBI3_ITALIC))
			return FALSE;
		break;

	case FORMAT_SAMI:
		if ((ac1->attr ^ ac2->attr)
		    & (VBI3_UNDERLINE | VBI3_BOLD | VBI3_ITALIC | VBI3_FLASH))
			return FALSE;
		break;
	}

	return TRUE;
}

static void
style_change			(sub_instance *		sub,
				 const vbi3_page *	pg,
				 const vbi3_char *	ac)
{
	switch (sub->format) {
	case FORMAT_QTTEXT:
		if (!same_style (sub, ac, &sub->last_ac))
			qt_style (sub, pg, ac);
		break;

	case FORMAT_REALTEXT:
		if (!same_style (sub, ac, &sub->last_ac)) {
			real_style_end (sub);

			sub->last_ac = sub->para_ac;

			if (!same_style (sub, ac, &sub->last_ac))
				real_style (sub, pg, ac);
		}

		break;

	case FORMAT_SAMI:
		if (!same_style (sub, ac, &sub->last_ac)) {
			if (sub->in_span)
				putws (sub, FALSE, "</SPAN>");

			sub->in_span = FALSE;

			sub->last_ac = sub->para_ac;

			if (!same_style (sub, ac, &sub->last_ac))
				sami_span (sub, pg, ac);
		}

		break;
				
	default:
		break;
	}
}

static void
page_layout			(unsigned int *		top,
				 unsigned int *		bottom,
				 unsigned int *		left,
				 unsigned int *		right,
				 unsigned int *		hjust,
				 const vbi3_page *	pg)
{
	unsigned int row;
	unsigned int left_min;
	unsigned int left_max;
	unsigned int right_min;
	unsigned int right_max;

	*top = pg->rows;
	*bottom = 0;

	left_min = pg->columns;
	left_max = 0;

	right_min = pg->columns;
	right_max = 0;

	row = 0;
	if (pg->pgno >= 0x100)
		row = 1;

	for (; row < pg->rows; ++row) {
		const vbi3_char *cp;
		unsigned int column;

		cp = pg->text + row * pg->columns;

		for (column = 0; column < pg->columns; ++column)
			if (0x0020 != cp[column].unicode
			    && cp[column].size < VBI3_OVER_TOP
			    && vbi3_is_print (cp[column].unicode))
				break;

		if (column >= pg->columns)
			continue; /* empty row */

		*top = MIN (*top, row);
		*bottom = MAX (*bottom, row);

		left_min = MIN (left_min, column);
		left_max = MAX (left_max, column);

		for (column = pg->columns; column > 0; --column)
			if (0x0020 != cp[column - 1].unicode
			    && cp[column - 1].size < VBI3_OVER_TOP
			    && vbi3_is_print (cp[column - 1].unicode))
				break;

		right_min = MIN (right_min, column);
		right_max = MAX (right_max, column);
	}

	*left = left_min;
	*right = right_max - 1;

	*hjust = 0; /* not justified */

	if (left_min == left_max)
		*hjust |= 1; /* left */

	if (right_min == right_max)
		*hjust |= 2; /* right */
}

static void
paragraph			(sub_instance *		sub,
				 const vbi3_page *	pg,
				 unsigned int		top,
				 unsigned int		bottom,
				 unsigned int		left,
				 unsigned int		right)
{
	unsigned int row;
	unsigned int width;

	width = right - left + 1;

	for (row = top; row <= bottom; ++row) {
		const vbi3_char *cp;
		unsigned int column;
		unsigned int spaces;
		unsigned int fluff;

		cp = pg->text + row * pg->columns;

		spaces = 0;
		fluff = 0;

		for (column = left; column <= right; ++column) {
			unsigned int c;

			if (cp[column].size >= VBI3_OVER_TOP) {
				++fluff;
				continue;
			}

			c = cp[column].unicode;

			if (0x0020 == c || !vbi3_is_print (c)) {
				switch (sub->format) {
				case FORMAT_SAMI:
					if (sub->last_ac.background
					    != cp[column].background)
						break;

					/* fall through */

				default:
					++spaces;
					continue;
				}

				c = 0x0020;
			}

			if (spaces > 0) {
				if (spaces + fluff >= (column - left)) {
					/* Discard leading spaces. */
				} else if (spaces > 1) {
					/* Runs of spaces. */

					switch (sub->format) {
					case FORMAT_SAMI:
						while (spaces-- > 0)
							putws (sub, FALSE,
							       "&nbsp;");
						break;

					default:
						/* Collapse to single space. */
						putwc (sub, FALSE, 0x0020);
						break;
					}
				} else {
					putwc (sub, FALSE, 0x0020);
				}

				spaces = 0;
			}

			fluff = 0;

			style_change (sub, pg, cp + column);

			putwc (sub, /* escape */ TRUE, c);
		}

		if (spaces > 0) {
			/* Discard trailing spaces. */
		}

		/* Line separator. */

		switch (sub->format) {
		case FORMAT_MPSUB:
		case FORMAT_QTTEXT:
		case FORMAT_SUBRIP:
			if (spaces + fluff >= width) {
				/* Suppress blank line. */
			} else {
				putwc (sub, /* escape */ FALSE, 10);
			}

			break;

		case FORMAT_REALTEXT:
		case FORMAT_SAMI:
			if (row < bottom)
				putws (sub, FALSE, "<br/>");
			break;

		case FORMAT_SUBVIEWER:
			if (row < bottom)
				putws (sub, FALSE, "[br]");
			else
				putwc (sub, FALSE, 10);
			break;
		}
	}
}

static vbi3_bool
export				(vbi3_export *		e,
				 const vbi3_page *	pg)
{
	sub_instance *sub = PARENT (e, sub_instance, export);
	unsigned int top;
	unsigned int bottom;
	unsigned int left;
	unsigned int right;
	unsigned int hjust;

	if (setjmp (sub->main)) {
		/* Discard unfinished output. */
		sub->text1.bp = sub->text1.buffer;

		return FALSE;
	}

	if (!pg) {
		/* Finalize the stream. */

		if (!sub->have_header)
			return TRUE;

		if (sub->text1.bp > sub->text1.buffer) {
			/* Put timestamp in front of page (text1). */
			SWAP (sub->text1, sub->text2);
			timestamp (sub);
			flush (sub);
			SWAP (sub->text1, sub->text2);
		}

		footer (sub);
		flush (sub);

		sub->have_header = FALSE;

		_vbi3_iconv_close (sub->cd);
		sub->cd = NULL;

		return TRUE;
	}

	if (0 != sub->last_pgno
	    && pg->pgno != sub->last_pgno) {
		fprintf (stderr, "Multilingual subtitle recording "
				 "not supported yet: pgno=%x last_pgno=%x\n",
				 pg->pgno, sub->last_pgno);
		return FALSE;
	}

	sub->last_pgno = pg->pgno;

	if (!sub->have_header) {
		char buffer[256];
		char *d;
		const char *codeset;
		size_t n;

		d = buffer;

		codeset = _vbi3_export_codeset (sub->charset);

		sub->cd = _vbi3_iconv_open (codeset, "UCS-2",
					    &d, sizeof (buffer), '?');
		if (NULL == sub->cd) {
			return FALSE;
		}

		n = d - buffer;
		if (n > 0)
			if (!vbi3_export_write (&sub->export, buffer, n))
				longjmp (sub->main, -1);

		header (sub, pg);
		flush (sub);

		sub->have_header = TRUE;

		sub->last_timestamp = e->stream.start_timestamp;

//		sub->delay_time = e->stream.timestamp
//			- e->stream.start_timestamp;

		sub->n_pages = 0;
		sub->blank_pages = 0;
	}

	/* First let's determine where the text is, and if
	   it's left or right justified or centered. Not handled
	   are split cases, e. g. one para at top right, another
	   bottom centered. When the page is empty top > bottom. */

	page_layout (&top, &bottom, &left, &right, &hjust, pg);

	/* Finalize the previous page
	   and write text position/justification tags. */

	switch (sub->format) {
	case FORMAT_MPSUB:
		if (0 == sub->blank_pages) {
			if (sub->text1.bp > sub->text1.buffer) {
				SWAP (sub->text1, sub->text2);
				timestamp (sub);
				flush (sub);
				SWAP (sub->text1, sub->text2);
				flush (sub);
			}

			/* End of last non-blank page. */
			sub->last_timestamp = e->stream.timestamp;
		}

		if (top > bottom) {
			/* Ignore blank pages. */
			sub->blank_pages = 1;
			return TRUE;
		}

		/* Delay btw end of last and start of this non-blank page. */
//		sub->delay_time = e->stream.timestamp - sub->last_timestamp;
		sub->blank_pages = 0;

		/* MPSub is always centered at bottom. */
		hjust = 0;

		break;

	case FORMAT_QTTEXT:
	{
		static const char *hjust_attr [] = {
			"{justify:center}",
			"{justify:left}",
			"{justify:right}",
			"{justify:center}",
		};

		timestamp (sub);

		if (top > bottom)
			goto page_separator;

		if (1 && 0 == hjust) {
			if ((int)(left + right + 8) < (int) pg->columns * 2)
				hjust = 1; /* left */
			else
			if ((int)(left + right - 8) > (int) pg->columns * 2)
				hjust = 2; /* right */
			/* else center */
		}

		if (sub->last_just != hjust) {
			putws (sub, FALSE, hjust_attr[hjust]);
			sub->last_just = hjust;
		}

		break;
	}

	case FORMAT_REALTEXT:
		if (0 == sub->blank_pages
		    && sub->text1.bp > sub->text1.buffer) {
			SWAP (sub->text1, sub->text2);
			timestamp (sub);
			flush (sub);
			SWAP (sub->text1, sub->text2);
			flush (sub);
		}

		if (top > bottom) {
			/* Ignore blank pages. */
			sub->blank_pages = 1;
			return TRUE;
		}

		hjust = 0; /* ? */

		sub->last_timestamp = sub->export.stream.timestamp;

		sub->blank_pages = 0;

		sub->last_ac = sub->para_ac;

		break;

	case FORMAT_SAMI:
		timestamp (sub);

		if (top > bottom) {
			putws (sub, FALSE, "<P>&nbsp;");
			goto page_separator;
		}

		/* Pity we cannot just define a screen center
		   relative position. */

		if (1 && 0 == hjust) {
			if ((int)(left + right + 8) < (int) pg->columns * 2)
				hjust = 1; /* left */
			else
			if ((int)(left + right - 8) > (int) pg->columns * 2)
				hjust = 2; /* right */
			/* else center */
		}

		if (0 == hjust || 3 == hjust) {
			/* Can only justify left or right. */
			/* Note MPlayer cannot handle "<p>". */
			putws (sub, FALSE, "<P>");
		} else {
			wprintf (sub, FALSE,
				 "<P class=\"%c\">", "xlrx"[hjust]);
		}

		sub->last_ac = sub->para_ac;

		break;

	case FORMAT_SUBRIP:
	case FORMAT_SUBVIEWER:
		if (0 == sub->blank_pages
		    && sub->text1.bp > sub->text1.buffer) {
			SWAP (sub->text1, sub->text2);
			timestamp (sub);
			flush (sub);
			SWAP (sub->text1, sub->text2);
			flush (sub);

			++sub->n_pages;
		}

		if (top > bottom) {
			/* Ignore blank pages. */
			sub->blank_pages = 1;
			return TRUE;
		}

		sub->last_timestamp = sub->export.stream.timestamp;

		sub->blank_pages = 0;

		break;
	}

	/* Write paragraph. */

	paragraph (sub, pg, top, bottom, left, right);

 page_separator:

	/* Page separator. */

	switch (sub->format) {
	case FORMAT_MPSUB:
	case FORMAT_SUBRIP:
	case FORMAT_SUBVIEWER:
		/* Separator is a blank line. */
		putwc (sub, FALSE, 10);
		/* Don't flush, have to add timestamp. */
		break;

	case FORMAT_QTTEXT:
		/* Nothing. */
		flush (sub);
		break;

	case FORMAT_REALTEXT:
		real_style_end (sub);
		putwc (sub, FALSE, 10);
		/* Don't flush, have to add timestamp. */
		break;

	case FORMAT_SAMI:
		if (sub->in_span) {
			putws (sub, FALSE, "</SPAN>");
			sub->in_span = FALSE;
		}
		putws (sub, FALSE, "</P></SYNC>\n");
		flush (sub);
		break;
	}

	sub->last_timestamp = sub->export.stream.timestamp;

	return TRUE;
}

static const vbi3_export_info
export_info_mpsub = {
	.keyword		= "mpsub",
	.label			= "MPSub",
	/* TRANSLATORS:
	   Caption is an aid for the hearing impaired,
	   subtitles for foreign language viewers. */
	.tooltip		= N_("MPlayer caption/subtitle file"),
	.mime_type		= NULL,
	.extension		= "sub",
	.open_format		= TRUE,
};

const _vbi3_export_module
_vbi3_export_module_mpsub = {
	.export_info		= &export_info_mpsub,
	._new			= sub_new,
	._delete		= sub_delete,
	.option_info		= option_info1,
	.option_info_size	= N_ELEMENTS (option_info1),
	.option_get		= option_get,
	.option_set		= option_set,
	.export			= export
};

static const vbi3_export_info
export_info_qttext = {
	.keyword		= "qttext",
	.label			= "QTText",
	/* TRANSLATORS:
	   Styles like bold, italic, underlined, etc.
	   Justification to the left, right, centered. */
	.tooltip		= N_("QuickTime Text caption/subtitle file "
				     "preserving text styles, justification "
				     "and color"),
	.mime_type		= NULL,
	.extension		= "txt",
	.open_format		= TRUE,
};

const _vbi3_export_module
_vbi3_export_module_qttext = {
	.export_info		= &export_info_qttext,
	._new			= sub_new,
	._delete		= sub_delete,
	.option_info		= option_info2,
	.option_info_size	= N_ELEMENTS (option_info2),
	.option_get		= option_get,
	.option_set		= option_set,
	.export			= export
};

static const vbi3_export_info
export_info_realtext = {
	.keyword		= "realtext",
	.label			= "RealText",
	.tooltip		= N_("RealText caption/subtitle file "
				     "preserving text styles and color"),
	.mime_type		= NULL,
	.extension		= "rt",
	.open_format		= TRUE,
};

const _vbi3_export_module
_vbi3_export_module_realtext = {
	.export_info		= &export_info_realtext,
	._new			= sub_new,
	._delete		= sub_delete,
	.option_info		= option_info2,
	.option_info_size	= N_ELEMENTS (option_info2),
	.option_get		= option_get,
	.option_set		= option_set,
	.export			= export
};

static const vbi3_export_info
export_info_sami = {
	.keyword		= "sami",
	.label			= "SAMI",
	.tooltip		= N_("SAMI 1.0 caption/subtitle file "
				     "preserving text styles, justification "
				     "and color"),
	.mime_type		= NULL,
	.extension		= "sami,smi",
	.open_format		= TRUE,
};

const _vbi3_export_module
_vbi3_export_module_sami = {
	.export_info		= &export_info_sami,
	._new			= sub_new,
	._delete		= sub_delete,
	.option_info		= option_info1,
	.option_info_size	= N_ELEMENTS (option_info1),
	.option_get		= option_get,
	.option_set		= option_set,
	.export			= export
};

static const vbi3_export_info
export_info_subrip = {
	.keyword		= "subrip",
	.label			= "SubRip",
	.tooltip		= N_("SubRip caption/subtitle file"),
	.mime_type		= NULL,
	.extension		= "srt",
	.open_format		= TRUE,
};

const _vbi3_export_module
_vbi3_export_module_subrip = {
	.export_info		= &export_info_subrip,
	._new			= sub_new,
	._delete		= sub_delete,
	.option_info		= option_info1,
	.option_info_size	= N_ELEMENTS (option_info1),
	.option_get		= option_get,
	.option_set		= option_set,
	.export			= export
};

static const vbi3_export_info
export_info_subviewer = {
	.keyword		= "subviewer",
	.label			= "SubViewer",
	.tooltip		= N_("SubViewer 2.x caption/subtitle file"),
	.mime_type		= NULL,
	.extension		= "sub",
	.open_format		= TRUE,
};

const _vbi3_export_module
_vbi3_export_module_subviewer = {
	.export_info		= &export_info_subviewer,
	._new			= sub_new,
	._delete		= sub_delete,
	.option_info		= option_info2,
	.option_info_size	= N_ELEMENTS (option_info2),
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
