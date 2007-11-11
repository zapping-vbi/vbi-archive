/*
 *  libzvbi - Text export functions
 *
 *  Copyright (C) 2001, 2002, 2004 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
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

/* $Id: exp-txt.c,v 1.10.2.14 2007-11-11 03:06:12 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <limits.h>		/* INT_MAX */
#include <setjmp.h>

#include "misc.h"
#include "page.h"		/* vbi3_page */
#include "conv.h"
#include "lang.h"		/* vbi3_is_print() */
#ifdef ZAPPING8
#  include "common/intl-priv.h"
#else
#  include "version.h"
#  include "intl-priv.h"
#endif
#include "export-priv.h"	/* vbi3_export */
#include "exp-txt.h"

typedef struct {
	uint16_t *		buffer;
        uint16_t *		bp;
	uint16_t *		end;
} vec;

#define MAX_COLORS N_ELEMENTS (((vbi3_page *) 0)->color_map)

typedef struct text_instance {
	vbi3_export		export;

	/* Options */

	int			encoding;
	char *			charset;
	vbi3_bool		color;
	int			term;
	int			gfx_chr;
	vbi3_bool		ascii_art;

	/* Not used anymore, stored for compatibility. */
	int			def_fg;
	int			def_bg;

	jmp_buf			main;

	vec			text;

	char			palette[MAX_COLORS];
} text_instance;

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

static const char *
terminal [] = {
	/* TRANSLATORS: Terminal control codes menu */
	N_("None"),
	N_("ANSI X3.64 / VT 100"),
	N_("VT 200")
};

enum {
	TERMINAL_NONE,
	TERMINAL_VT100,
	TERMINAL_VT200,
};

static const vbi3_option_info
option_info [] = {
	_VBI3_OPTION_MENU_INITIALIZER
	/* TRANSLATORS: Text export encoding (ASCII, Unicode, ...) menu */
	("format", N_("Encoding"),
	 10, user_encodings, N_ELEMENTS (user_encodings), NULL),
        /* one for users, another for programs */
	_VBI3_OPTION_STRING_INITIALIZER
	("charset", NULL, "locale",
	 N_("Character set, for example ISO-8859-1, UTF-8")),
	_VBI3_OPTION_STRING_INITIALIZER
	("gfx_chr", N_("Graphics char"),
	 "#", N_("Replacement for block graphic characters: "
		 "a single character or decimal (32) or hex (0x20) code")),
	_VBI3_OPTION_BOOL_INITIALIZER
	("ascii_art", N_("ASCII art"),
	 FALSE, N_("Replace graphic characters by ASCII art")),
	_VBI3_OPTION_MENU_INITIALIZER
	("control", N_("Control codes"),
	 0, terminal, N_ELEMENTS (terminal),
	 N_("Insert color and text style control codes")),
};

static vbi3_export *
text_new			(const _vbi3_export_module *em)
{
	text_instance *text;

	assert (sizeof (user_encodings) == sizeof (iconv_encodings));

	em = em;

	if (!(text = vbi3_malloc (sizeof (*text))))
		return NULL;

	CLEAR (*text);

	return &text->export;
}

static void
text_delete			(vbi3_export *		e)
{
	text_instance *text = PARENT (e, text_instance, export);

	vbi3_free (text->text.buffer);
	vbi3_free (text->charset);
	vbi3_free (text);
}

#define KEYWORD(str) (0 == strcmp (keyword, str))

static vbi3_bool
option_get			(vbi3_export *		e,
				 const char *		keyword,
				 vbi3_option_value *	value)
{
	text_instance *text = PARENT (e, text_instance, export);

	if (KEYWORD ("format") || KEYWORD ("encoding")) {
		value->num = text->encoding;
	} else if (KEYWORD ("charset")) {
		const char *codeset;

		codeset = _vbi3_export_codeset (text->charset);
		value->str = _vbi3_export_strdup (e, NULL, codeset);
		if (!value->str)
			return FALSE;
	} else if (KEYWORD ("gfx_chr")) {
		if (!(value->str = _vbi3_export_strdup (e, NULL, "x")))
			return FALSE;
		value->str[0] = text->gfx_chr;
	} else if (KEYWORD ("ascii_art")) {
		value->num = text->ascii_art;
	} else if (KEYWORD ("control")) {
		value->num = text->term;
	} else if (KEYWORD ("fg")) {
		value->num = text->def_fg;
	} else if (KEYWORD ("bg")) {
		value->num = text->def_bg;
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
	text_instance *text = PARENT (e, text_instance, export);

	if (KEYWORD ("format") || KEYWORD ("encoding")) {
		unsigned int encoding = va_arg (ap, unsigned int);

		if (encoding >= N_ELEMENTS (user_encodings)) {
			_vbi3_export_invalid_option (e, keyword, encoding);
			return FALSE;
		}

		if (!_vbi3_export_strdup (e, &text->charset,
					 iconv_encodings[encoding]))
			return FALSE;

		text->encoding = encoding;
	} else if (KEYWORD ("charset")) {
		const char *string = va_arg (ap, const char *);

		if (!string) {
			_vbi3_export_invalid_option (e, keyword, string);
			return FALSE;
		}

		if (!_vbi3_export_strdup (e, &text->charset, string))
			return FALSE;
	} else if (KEYWORD ("gfx_chr")) {
		const char *string = va_arg (ap, const char *);
		char *s;
		int value;

		if (!string || !string[0]) {
			_vbi3_export_invalid_option (e, keyword, string);
			return FALSE;
		}

		if (1 == strlen (string)) {
			value = string[0];
		} else {
			value = strtol (string, &s, 0);

			if (s == string)
				value = string[0];
		}

		text->gfx_chr = (value < 0x20 || value > 0xE000) ?
			0x20 : value;
	} else if (KEYWORD ("ascii_art")) {
		text->ascii_art = !!va_arg (ap, vbi3_bool);
	} else if (KEYWORD ("control")) {
		unsigned int term = va_arg (ap, unsigned int);

		if (term > N_ELEMENTS (terminal)) {
			_vbi3_export_invalid_option (e, keyword, term);
			return FALSE;
		}

		text->term = term;
	} else if (KEYWORD ("fg")) {
		unsigned int col = va_arg (ap, unsigned int);

		if (col > 8) {
			_vbi3_export_invalid_option (e, keyword, col);
			return FALSE;
		}

		text->def_fg = col;
	} else if (KEYWORD ("bg")) {
		unsigned int col = va_arg (ap, unsigned int);

		if (col > 8) {
			_vbi3_export_invalid_option (e, keyword, col);
			return FALSE;
		}

		text->def_bg = col;
	} else {
		_vbi3_export_unknown_option (e, keyword);
		return FALSE;
	}

	return TRUE;
}

static void
extend				(text_instance *	text,
				 vec *			v)
{
	uint16_t *buffer;
	unsigned int n;

	n = v->end - v->buffer + 2048;

	if (!(buffer = vbi3_realloc (v->buffer, n * sizeof (*v->buffer)))) {
		longjmp (text->main, -1);
	}

	v->bp = buffer + (v->bp - v->buffer);
	v->buffer = buffer;
	v->end = buffer + n;
}

static void
putw_spaces			(text_instance *	text,
				 unsigned int		n)
{
	uint16_t *d;

	if (text->text.bp + n > text->text.end)
		extend (text, &text->text);

	d = text->text.bp;

	while (n-- > 0)
		*d++ = 0x0020;

	text->text.bp = d;
}

static void
putwc				(text_instance *	text,
				 unsigned int		c)
{
	if (text->text.bp >= text->text.end)
		extend (text, &text->text);

	*text->text.bp++ = c;
}

static void
create_palette			(text_instance *	text,
				 const vbi3_page *	pg)
{
	unsigned int j;

	for (j = 0; j < N_ELEMENTS (text->palette); ++j) {
		vbi3_rgba color;
		unsigned int i;
		unsigned int imin;
		int d;
		int dmin;

		color = pg->color_map[j];
		imin = 0;
		dmin = INT_MAX;

		/* VT.100 defines only 8 pure colors. Try to find one
		   close to the color_map entry (yeah, that's probably
		   too simple). */
		for (i = 0; i < 8; ++i) {
			d  = ABS ((int)((i & 1) * 0xFF - VBI3_R (color)));
			d += ABS ((int)(((i >> 1) & 1) * 0xFF
					- VBI3_G (color)));
			d += ABS ((int)((i >> 2) * 0xFF - VBI3_B (color)));

			if (d < dmin) {
				dmin = d;
				imin = i;
			}
		}

		text->palette[j] = '0' + imin;
	}
}

static vbi3_bool
putw_attr			(text_instance *	text,
				 vbi3_char		old,
				 vbi3_char		cur)
{
	uint16_t *d;

	if (text->text.bp + 32 >= text->text.end)
		extend (text, &text->text);

	d = text->text.bp;

	/* Control sequences based on ECMA-48,
	   http://www.ecma-international.org/ */

	if (old.size != cur.size) {
		switch (cur.size) {
		case VBI3_NORMAL_SIZE:
			d[0] = 27;
			d[1] = '#';
			d[2] = '5';
			d += 3;
			break;

		case VBI3_DOUBLE_WIDTH:
			d[0] = 27;
			d[1] = '#';
			d[2] = '6';
			d += 3;
			break;
			
		case VBI3_DOUBLE_HEIGHT:
		case VBI3_DOUBLE_HEIGHT2:
			break; /* ignore */
			
		case VBI3_DOUBLE_SIZE:
			d[0] = 27;
			d[1] = '#';
			d[2] = '3';
			d += 3;
			break;

		case VBI3_DOUBLE_SIZE2:
			d[0] = 27;
			d[1] = '#';
			d[2] = '4';
			d += 3;
			break;

		case VBI3_OVER_TOP:
		case VBI3_OVER_BOTTOM:
			return FALSE; /* don't print */
		}
	} else {
		switch (cur.size) {
		case VBI3_OVER_TOP:
		case VBI3_OVER_BOTTOM:
			return FALSE; /* don't print */

		default:
			break; /* no change */
		}
	}

	/* SGR sequence */

	d[0] = 27; /* CSI */
	d[1] = '[';
	d += 2;

	if (TERMINAL_VT100 == text->term) {
		if ((old.attr ^ cur.attr)
		    & (VBI3_UNDERLINE | VBI3_BOLD | VBI3_FLASH)) {
			*d++ = ';'; /* \e[0; reset */
			old.attr &= ~(VBI3_UNDERLINE | VBI3_BOLD | VBI3_FLASH);
			old.foreground = ~cur.foreground;
			old.background = ~cur.background;
		}
	}

	if ((old.attr ^ cur.attr) & VBI3_BOLD) {
		if (cur.attr & VBI3_BOLD) {
			d[0] = '1'; /* bold */
			d[1] = ';';
			d += 2;
		} else {
			d[0] = '2'; /* bold off */
			d[1] = '2';
			d[2] = ';';
			d += 3;
		}
	}

	if ((old.attr ^ cur.attr) & VBI3_ITALIC) {
		if (!(cur.attr & VBI3_ITALIC))
			*d++ = '2'; /* off */
		d[0] = '3'; /* italic */
		d[1] = ';';
		d += 2;
	}

	if ((old.attr ^ cur.attr) & VBI3_UNDERLINE) {
		if (!(cur.attr & VBI3_UNDERLINE))
			*d++ = '2'; /* off */
		d[0] = '4'; /* underline */
		d[1] = ';';
		d += 2;
	}
	
	if ((old.attr ^ cur.attr) & VBI3_FLASH) {
		if (!(cur.attr & VBI3_FLASH))
			*d++ = '2'; /* steady */
		d[0] = '5'; /* slowly blinking */
		d[1] = ';';
		d += 2;
	}

	/* ECMA-48 SGR offers conceal/reveal code, but we don't
	   know if the terminal implements this correctly as
	   requested by the caller. Proportional spacing needs
	   further investigation. */
	
	if (old.foreground != cur.foreground) {
		d[0] = '3';
		d[1] = text->palette[cur.foreground];
		d[2] = ';';
		d += 3;
	}
	
	if (old.background != cur.background) {
		d[0] = '4';
		d[1] = text->palette[cur.background];
		d[2] = ';';
		d += 3;
	}

	if ('[' == d[-1])
		d -= 2; /* no change, remove CSI */
	else
		d[-1] = 'm'; /* replace last semicolon */

	text->text.bp = d;

	return TRUE;
}

/**
 * Like vbi3_print_page_region(), but takes export options as va_list.
 */
unsigned int
vbi3_print_page_region_va_list	(vbi3_page *		pg,
				 char *			buffer,
				 unsigned int		buffer_size,
				 const char *		format,
				 const char *		separator,
				 unsigned int		separator_size,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		width,
				 unsigned int		height,
				 va_list		export_options)
{
	text_instance text;
	vbi3_bool option_table;
	vbi3_bool option_rtl;
	unsigned int option_space_attr;
	unsigned int y;
	unsigned int row0;
	unsigned int row1;
	unsigned int column0;
	unsigned int column1;
	unsigned int doubleh;	/* current row */
	unsigned int doubleh0;  /* previous row */
	vbi3_iconv_t *cd;
	char *p;
	char *buffer_end;
	const vbi3_char *acp;

	assert (NULL != pg);
	assert (NULL != buffer);

	if (0 == buffer_size)
		return 0;

	if (0)
		fprintf (stderr, "vbi3_print_page_region '%s' "
		         "col=%d row=%d width=%d height=%d\n",
			 format, column, row, width, height);

	CLEAR (text.text);

	option_table = FALSE;
	option_rtl = FALSE;
	option_space_attr = 0;

	for (;;) {
		vbi3_export_option option;

		option = va_arg (export_options, vbi3_export_option);

		switch (option) {
		case VBI3_TABLE:
			option_table = va_arg (export_options, vbi3_bool);
			break;

		case VBI3_RTL:
			option_rtl = va_arg (export_options, vbi3_bool);
			break;

		case VBI3_REVEAL:
			if (va_arg (export_options, vbi3_bool))
				option_space_attr &= ~VBI3_CONCEAL;
			else
				option_space_attr |= VBI3_CONCEAL;
			break;

		case VBI3_FLASH_ON:
			if (va_arg (export_options, vbi3_bool))
				option_space_attr &= ~VBI3_FLASH;
			else
				option_space_attr |= VBI3_FLASH;
			break;

		case VBI3_SCALE:
			(void) va_arg (export_options, vbi3_bool);
			break;

		default:
			option = 0;
			break;
		}

		if (0 == option)
			break;
	}

	row0	= row;
	row1	= row + height - 1;
	column0	= column;
	column1	= column + width - 1;

	if (row1 >= pg->rows
	    || column1 >= pg->columns)
		return 0;

	p = buffer;
	buffer_end = buffer + buffer_size;

	cd = _vbi3_iconv_open (format, "UCS-2", &p, buffer_size, '?');
	if (NULL == cd) {
		return 0;
	}

	if (setjmp (text.main))
		goto failure;

	doubleh = 0;

	acp = pg->text + row0 * pg->columns;

	for (y = row0; y <= row1; ++y) {
		unsigned int x, xs, xe, xl, xw;
		unsigned int chars, spaces;
		int dir;

		xs = (option_table || y == row0) ? column0 : 0;
		xe = (option_table || y == row1) ? column1 : (pg->columns - 1);
		xw = xe - xs;

		dir = +1;

		if (option_rtl) {
			SWAP (xs, xe);
			dir = -1;
		}

		xe += dir;

		if (!option_table && y == row0 && 2 == height)
			xl = option_rtl ? column0 : column1;
		else
			xl = INT_MAX;

		doubleh0 = doubleh;
		doubleh = 0;

		chars = 0;
		spaces = 0;

		for (x = xs; x != xe; x += dir) {
			vbi3_char ac = acp[x];

			if (ac.attr & option_space_attr)
				ac.unicode = 0x0020;

			if (option_table) {
				if (ac.size > VBI3_DOUBLE_SIZE)
					ac.unicode = 0x0020;
			} else {
				switch (ac.size) {
				case VBI3_NORMAL_SIZE:
				case VBI3_DOUBLE_WIDTH:
					break;

				case VBI3_DOUBLE_HEIGHT:
				case VBI3_DOUBLE_SIZE:
					doubleh++;
					break;

				case VBI3_OVER_TOP:
				case VBI3_OVER_BOTTOM:
					continue;

				case VBI3_DOUBLE_HEIGHT2:
				case VBI3_DOUBLE_SIZE2:
					if (y > row0)
						ac.unicode = 0x0020;
					break;
				}

				/* Special case two lines row0 ... row1, and
				   all chars in row0, column0 ... column1 are
				   double height: Skip row1, don't wrap
				   around. */
				if (x == xl && doubleh >= chars) {
					xe = xl + dir;
					y = row1;
				}

				if (0x0020 == ac.unicode
				    || !vbi3_is_print (ac.unicode)) {
					++spaces;
					++chars;
					continue;
				} else {
					if (spaces < chars || y == row0)
						putw_spaces (&text, spaces);
					/* else discard leading spaces */

					spaces = 0;
				}
			}

			putwc (&text, ac.unicode);

			++chars;
		}

		if (y < row1) {
			/* Note option_table implies 0 == spaces. */
			if (spaces >= xw) {
				; /* suppress blank line */
			} else {
				; /* discard trailing spaces */

				if (separator) {
					unsigned int size;

					size = text.text.bp - text.text.buffer;

					if (!_vbi3_iconv_ucs2
					    (cd, &p,
					     (unsigned int)(buffer_end - p),
					     text.text.buffer,
					     size))
						goto failure;

					text.text.bp = text.text.buffer;

					if (separator_size
					    > (unsigned int)(buffer_end - p))
						goto failure;

					memcpy (p, separator, separator_size);
					p += separator_size;
				} else {
					putwc (&text, option_table ?
					       0x000AU : 0x0020U);
				}
			}
		} else {
			/* Last row. */

			if (doubleh0 > 0) {
				; /* pretend this is the lower half of an
				     all blank double height row */
			} else {
				/* Trailing spaces. */
				putw_spaces (&text, spaces);
			}
		}

		acp += pg->columns;
	}

	if (!_vbi3_iconv_ucs2 (cd, &p,
			       (unsigned int)(buffer_end - p),
			       text.text.buffer,
			       (unsigned int)(text.text.bp - text.text.buffer)))
		goto failure;

	_vbi3_iconv_close (cd);
	cd = NULL;

	return p - buffer;

 failure:
	vbi3_free (text.text.buffer);

	_vbi3_iconv_close (cd);
	cd = NULL;

	return 0;
}

/**
 * @param pg Source page.
 * @param buffer Output buffer.
 * @param buffer_size Size of the buffer in bytes.
 * @param format Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is "UTF-8".
 * @param separator This string is copied verbatim into the buffer
 *   to separate rows. When @c NULL a default is provided,
 *   converted to the requested @a format. In table mode the
 *   default separator is character 0x0A, otherwise a space 0x20.
 * @param separator_size Length of the separator string in bytes.
 *   This permits separators containing zero bytes.
 * @param export_options Array of export options, these are
 *   vbi3_export_option codes followed by a value. The last
 *   option code must be @a 0.
 *   The following export options are presently recognized:
 *   - @c VBI3_TABLE (vbi3_bool): Scan page in table mode, printing
 *     all characters within the source rectangle including runs of
 *     spaces at the start and end of rows. When @c FALSE, scan
 *     all characters from @a column, @a row to @a column + @a width - 1,
 *     @a row + @a height - 1 and all intermediate rows to their
 *     full pg->columns width. In this mode runs of spaces at
 *     the start and end of rows collapse into single spaces,
 *     blank lines are suppressed.
 *   - @c VBI3_RTL (vbi3_bool): When @c TRUE scan the page right to
 *     left (Hebrew, Arabic), otherwise left to right.
 *   - @c VBI3_REVEAL (vbi3_bool): When @c TRUE reveal hidden
 *     characters otherwise printed as space.
 *   - @c VBI3_FLASH_ON (vbi3_bool): Print flashing characters in on (TRUE) or
 *     off (FALSE) state.
 * @param column First source column, 0 ... pg->columns - 1.
 * @param row First source row, 0 ... pg->rows - 1.
 * @param width Number of columns to print, 1 ... pg->columns.
 * @param height Number of rows to print, 1 ... pg->rows.
 * 
 * Prints a subsection of a Teletext or Closed Caption vbi3_page
 * into a buffer. All character attributes and colors will be lost.
 * (Conversion to terminal control codes is possible using the text
 * export module.) Graphics characters and DRCS will be replaced
 * by spaces.
 * 
 * @return
 * Number of bytes written into @a buffer, a value of zero when
 * some error occurred. In this case @a buffer may contain incomplete
 * data. Note this function does not append a terminating zero
 * character.
 */
unsigned int
vbi3_print_page_region		(vbi3_page *		pg,
				 char *			buffer,
				 unsigned int		buffer_size,
				 const char *		format,
				 const char *		separator,
				 unsigned int		separator_size,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		width,
				 unsigned int		height,
				 ...)
{
	unsigned int r;
	va_list export_options;

	va_start (export_options, height);
	r = vbi3_print_page_region_va_list (pg, buffer, buffer_size,
					   format,
					   separator, separator_size,
					   column, row, width, height,
					   export_options);
	va_end (export_options);

	return r;
}

/**
 * Like vbi3_print_page(), but takes export options as va_list.
 */
unsigned int
vbi3_print_page_va_list		(vbi3_page *		pg,
				 char *			buffer,
				 unsigned int		buffer_size,
				 const char *		format,
				 va_list		export_options)
{
	return vbi3_print_page_region_va_list (pg, buffer, buffer_size,
					      format,
					      /* separator */ NULL,
					      /* separator_size */ 0,
					      /* column */ 0, /* row */ 0,
					      pg->columns, pg->rows,
					      export_options);
}

/**
 * @param pg Source page.
 * @param buffer Output buffer.
 * @param buffer_size Size of the buffer in bytes.
 * @param format Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is "UTF-8".
 * @param export_options Array of export options, these are
 *   vbi3_export_option codes followed by a value. The last
 *   option code must be @a 0.
 *   The following export options are presently recognized:
 *   - @c VBI3_TABLE (vbi3_bool): Scan page in table mode, printing
 *     all characters within the source rectangle including runs of
 *     spaces at the start and end of rows. When @c FALSE, scan
 *     all characters from @a column, @a row to @a column + @a width - 1,
 *     @a row + @a height - 1 and all intermediate rows to their
 *     full pg->columns width. In this mode runs of spaces at
 *     the start and end of rows collapse into single spaces,
 *     blank lines are suppressed.
 *   - @c VBI3_RTL (vbi3_bool): When @c TRUE scan the page right to
 *     left (Hebrew, Arabic), otherwise left to right.
 *   - @c VBI3_REVEAL (vbi3_bool): When @c TRUE reveal hidden
 *     characters otherwise printed as space.
 *   - @c VBI3_FLASH_ON (vbi3_bool): Print flashing characters in on (TRUE) or
 *     off (FALSE) state.
 *
 * Prints a Teletext or Closed Caption vbi3_page into a buffer.
 * All character attributes and colors will be lost.
 * (Conversion to terminal control codes is possible using the
 * text export module.) Graphics characters and DRCS will be
 * replaced by spaces.
 *
 * This is a specialization of vbi3_print_page_region().
 *
 * @return
 * Number of bytes written into @a buffer, a value of zero when
 * some error occurred. In this case @a buffer may contain incomplete
 * data. Note this function does not append a terminating zero
 * character.
 */
unsigned int
vbi3_print_page			(vbi3_page *		pg,
				 char *			buffer,
				 unsigned int		buffer_size,
				 const char *		format,
				 ...)
{
	unsigned int r;
	va_list export_options;

	va_start (export_options, format);
	r = vbi3_print_page_region_va_list (pg, buffer, buffer_size,
					   format,
					   /* separator */ NULL,
					   /* separator_size */ 0,
					   /* column */ 0, /* row */ 0,
					   pg->columns, pg->rows,
					   export_options);
	va_end (export_options);

	return r;
}

static void
xputwc				(text_instance *	text,
				 unsigned int		c)
{
	if (vbi3_is_gfx (c)) {
		if (text->ascii_art) {
			c = _vbi3_teletext_ascii_art (c);

			if (!vbi3_is_print (c))
				c = text->gfx_chr;
		} else {
			c = text->gfx_chr;
		}
	} else if (!vbi3_is_print (c)) {
		c = 0x0020;
	}

	putwc (text, c);
}

static vbi3_bool
export				(vbi3_export *		e,
				 const vbi3_page *	pg)
{
	text_instance *text = PARENT (e, text_instance, export);
	const vbi3_char *acp;
	const char *codeset;
	char *buffer;
	vbi3_char last;
	unsigned int row;
	unsigned int column;
	unsigned int size;
	vbi3_bool success;

	create_palette (text, pg);

	text->text.bp = text->text.buffer;

	acp = pg->text;

	SET (last);

	for (row = 0; row < pg->rows; ++row) {
		uint16_t *d;

		for (column = 0; column < pg->columns; ++column) {
			if (TERMINAL_NONE != text->term) {
				if (putw_attr (text, last, *acp))
					xputwc (text, acp->unicode);

				last = *acp;
			} else {
				xputwc (text, acp->unicode);
			}

			++acp;
		}

		if (text->text.bp + 4 >= text->text.end)
			extend (text, &text->text);

		d = text->text.bp;

		/* Reset background color because terminals scroll
		   that color in if the display is wider than the page. */
		if (1 || row + 1 >= pg->rows) {
			if (TERMINAL_NONE != text->term) {
				d[0] = 27; /* reset */
				d[1] = '[';
				d[2] = 'm';
				d += 3;

				SET (last);
			}
		}

		d[0] = '\n';

		text->text.bp = d + 1;
	}

	size = text->text.bp - text->text.buffer;

	codeset = _vbi3_export_codeset (text->charset);

	buffer = vbi3_strndup_iconv_ucs2 (codeset,
					  text->text.buffer,
					  size,
					  /* repl_char */ '?');
	if (NULL == buffer) {
		_vbi3_export_malloc_error (e);
		return FALSE;
	}

#warning FIXME buffer may not contain an ASCII or UTF-8 string
	success = vbi3_export_write (&text->export, buffer,
				     strlen (buffer));

	free (buffer);
	buffer = NULL;

	return success;
}

static const vbi3_export_info
export_info = {
	.keyword		= "text",
	.label			= N_("Text"),
	.tooltip		= N_("Export the page as text file"),

	.mime_type		= "text/plain",
	.extension		= "txt",
};

const _vbi3_export_module
_vbi3_export_module_text = {
	.export_info		= &export_info,

	._new			= text_new,
	._delete		= text_delete,

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
