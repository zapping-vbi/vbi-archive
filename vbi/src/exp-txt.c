/*
 *  libzvbi - Text export functions
 *
 *  Copyright (C) 2001, 2002 Michael H. Schimek
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

/* $Id: exp-txt.c,v 1.10.2.3 2003-10-16 18:15:07 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <iconv.h>
#include <assert.h>

#include "lang.h"
#include "export.h"
#include "exp-txt.h"

typedef struct text_instance {
	vbi_export		export;

	/* Options */
	int			format;
	char *			charset;
	unsigned		color : 1;
	int			term;
	int			gfx_chr;
	int			def_fg;
	int			def_bg;

	iconv_t			cd;
	char			buf[32];
} text_instance;

static vbi_export *
text_new(void)
{
	text_instance *text;

	if (!(text = calloc(1, sizeof(*text))))
		return NULL;

	return &text->export;
}

static void
text_delete(vbi_export *e)
{
	text_instance *text = PARENT(e, text_instance, export);

	if (text->charset)
		free(text->charset);

	free(text);
}

#define elements(array) (sizeof(array) / sizeof(array[0]))

static const char *
formats[] = {
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
iconv_formats[] = {
	"ASCII", "ISO-8859-1", "ISO-8859-2", "ISO-8859-4",
	"ISO-8859-5", "ISO-8859-7", "ISO-8859-8", "ISO-8859-9",
        "KOI8-R", "KOI8-U", "UTF-8"
};

static const char *
color_names[] __attribute__ ((unused)) = {
	N_("Black"), N_("Red"), N_("Green"), N_("Yellow"),
	N_("Blue"), N_("Magenta"), N_("Cyan"), N_("White"),
	N_("Any")
};

static const char *
terminal[] __attribute__ ((unused)) = {
	/* TRANSLATORS: Terminal control codes menu */
	N_("None"), N_("ANSI X3.64 / VT 100"), N_("VT 200")
};

enum {
	TERMINAL_NONE,
	TERMINAL_VT100,
	TERMINAL_VT200,
};

static vbi_option_info
text_options[] = {
	VBI_OPTION_MENU_INITIALIZER
	  /* TRANSLATORS: Text export format (ASCII, Unicode, ...) menu */
	  ("format", N_("Format"),
	   0, formats, elements(formats), NULL),
        /* one for users, another for programs */
	VBI_OPTION_STRING_INITIALIZER
	  ("charset", NULL, "", NULL),
	VBI_OPTION_STRING_INITIALIZER
	  ("gfx_chr", N_("Graphics char"),
	   "#", N_("Replacement for block graphic characters: "
		   "a single character or decimal (32) or hex (0x20) code")),
	VBI_OPTION_MENU_INITIALIZER
	  ("control", N_("Control codes"),
	   0, terminal, elements(terminal), NULL),
#if 0 /* obsolete (I think) */
	VBI_OPTION_MENU_INITIALIZER
	  ("fg", N_("Foreground"),
	   8 /* any */, color_names, elements(color_names),
	   N_("Assumed terminal foreground color")),
	VBI_OPTION_MENU_INITIALIZER
	  ("bg", N_("Background"),
	   8 /* any */, color_names, elements(color_names),
	   N_("Assumed terminal background color"))
#endif
};

static vbi_option_info *
option_enum(vbi_export *e, int index)
{
	if (index < 0 || index >= (int) elements(text_options))
		return NULL;

	return text_options + index;
}

#define KEYWORD(str) (strcmp (keyword, str) == 0)

static vbi_bool
option_get(vbi_export *e, const char *keyword, vbi_option_value *value)
{
	text_instance *text = PARENT(e, text_instance, export);

	if (KEYWORD("format")) {
		value->num = text->format;
	} else if (KEYWORD("charset")) {
		if (!(value->str = vbi_export_strdup(e, NULL, text->charset)))
			return FALSE;
	} else if (KEYWORD("gfx_chr")) {
		if (!(value->str = vbi_export_strdup(e, NULL, "x")))
			return FALSE;
		value->str[0] = text->gfx_chr;
	} else if (KEYWORD("control")) {
		value->num = text->term;
	} else if (KEYWORD("fg")) {
		value->num = text->def_fg;
	} else if (KEYWORD("bg")) {
		value->num = text->def_bg;
	} else {
		vbi_export_unknown_option(e, keyword);
		return FALSE;
	}

	return TRUE; /* success */
}

static vbi_bool
option_set(vbi_export *e, const char *keyword, va_list args)
{
	text_instance *text = PARENT(e, text_instance, export);

	if (KEYWORD("format")) {
		unsigned int format = va_arg(args, unsigned int);

		if (format >= elements(formats)) {
			vbi_export_invalid_option(e, keyword, format);
			return FALSE;
		}
		text->format = format;
	} else if (KEYWORD("charset")) {
		char *string = va_arg(args, char *);

		if (!string) {
			vbi_export_invalid_option(e, keyword, string);
			return FALSE;
		} else if (!vbi_export_strdup(e, &text->charset, string))
			return FALSE;
	} else if (KEYWORD("gfx_chr")) {
		char *s, *string = va_arg(args, char *);
		int value;

		if (!string || !string[0]) {
			vbi_export_invalid_option(e, keyword, string);
			return FALSE;
		}
		if (strlen(string) == 1) {
			value = string[0];
		} else {
			value = strtol(string, &s, 0);
			if (s == string)
				value = string[0];
		}
		text->gfx_chr = (value < 0x20 || value > 0xE000) ? 0x20 : value;
	} else if (KEYWORD("control")) {
		int term = va_arg(args, int);

		if (term < 0 || term > 2) {
			vbi_export_invalid_option(e, keyword, term);
			return FALSE;
		}
		text->term = term;
	} else if (KEYWORD("fg")) {
		int col = va_arg(args, int);

		if (col < 0 || col > 8) {
			vbi_export_invalid_option(e, keyword, col);
			return FALSE;
		}
		text->def_fg = col;
	} else if (KEYWORD("bg")) {
		int col = va_arg(args, int);

		if (col < 0 || col > 8) {
			vbi_export_invalid_option(e, keyword, col);
			return FALSE;
		}
		text->def_bg = col;
	} else {
		vbi_export_unknown_option(e, keyword);
		return FALSE;
	}

	return TRUE;
}

static __inline__ vbi_bool
print_spaces			(iconv_t		cd,
				 char **		pp,
				 char *			end,
				 unsigned int		spaces)
{
	for (; spaces > 0; spaces--)
		if (!vbi_iconv_unicode (cd, pp, end - *pp, 0x0020))
			return FALSE;
	return TRUE;
}

/**
 * @param pg Source page.
 * @param buf Buffer to hold the output.
 * @param buf_size Size of the buffer in bytes. The function
 *   fails with return value 0 when the text would exceed the
 *   buffer capacity.
 * @param format Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is "UTF-8".
 * @param separator Copy this string verbatim into the buffer
 *   to separate rows. When @c NULL a default is provided,
 *   converted to the requested @a format. When @a flags contains
 *   @c VBI_TABLE the default separator is 0x0A, otherwise a
 *   space 0x20.
 * @param sep_size Length of the separator string in bytes.
 *   This permits separators containing zero bytes.
 * @param flags Optional set of the following flags:
 *   - @c VBI_TABLE: Scan page in table mode, printing all characters
 *     within the source rectangle including runs of spaces at
 *     the start and end of rows. When not given, scan all characters
 *     from @a column, @a row to @a column + @a width - 1,
 *     @a row + @a height - 1 and all intermediate rows to their
 *     full pg->columns width. In this mode runs of spaces at
 *     the start and end of rows collapse into single spaces,
 *     blank lines are suppressed.
 *   - @c VBI_RTL: Scan the page right to left (Hebrew, Arabic),
 *     otherwise left to right.
 *   - @c VBI_REVEAL: Reveal hidden characters otherwise
 *     printed as space.
 *   - @c VBI_FLASH_OFF: Print flashing characters in off state,
 *     i. e. as space.
 * @param column First source column, 0 ... pg->columns - 1.
 * @param row First source row, 0 ... pg->rows - 1.
 * @param width Number of columns to print, 1 ... pg->columns.
 * @param height Number of rows to print, 1 ... pg->rows.
 * 
 * Print a subsection of a Teletext or Closed Caption vbi_page,
 * rows separated, in the desired format. All character attributes
 * and colors will be lost. (Conversion to terminal control codes
 * is possible using the text export module.) Graphics characters,
 * DRCS and all characters not representable in the target format
 * will be replaced by spaces.
 * 
 * @return
 * Number of bytes written into @a buf, a value of zero when
 * some error occurred. In this case @a buf may contain incomplete
 * data. Note this function does not append a terminating zero
 * character.
 */
unsigned int
vbi_print_page_region		(vbi_page *		pg,
				 char *			buf,
				 unsigned int		buf_size,
				 const char *		format,
				 const char *		separator,
				 unsigned int		sep_size,
				 vbi_export_flags	flags,
				 unsigned int		column,
				 unsigned int		row,
				 unsigned int		width,
				 unsigned int		height)
{
	unsigned int y, row0, row1;
	unsigned int x, column0, column1;
	unsigned int doubleh, doubleh0;
	iconv_t cd;
	char *p, *end;

	if (0)
		fprintf (stderr, "vbi_print_page_region '%s' "
		         "flags=0x%x col=%d row=%d width=%d height=%d\n",
			 format, column, row, width, height);

	column0 = column;
	row0 = row;
	column1 = column + width - 1;
	row1 = row + height - 1;

	if (pg == NULL
	    || buf == NULL || buf_size == 0
	    || column1 >= pg->columns
	    || row1 >= pg->rows)
		return 0;

	p = buf;
	end = p + buf_size;

	cd = vbi_iconv_open (format, &p, buf_size);

	if (cd == (iconv_t) -1)
		return 0;

	doubleh = 0;

	for (y = row0; y <= row1; y++) {
		unsigned int xs, xe, xl, yw;
		unsigned int chars, spaces;
		int adv;

		xs = ((flags & VBI_TABLE) || y == row0) ? column0 : 0;
		xe = ((flags & VBI_TABLE) || y == row1) ? column1 : (pg->columns - 1);
		yw = xe - xs;

		if (flags & VBI_RTL) {
			SWAP (xs, xe);
			adv = -1;
		} else {
			adv = +1;
		}

		xe += adv;

		if (!(flags & VBI_TABLE) && height == 2 && y == row0)
			xl = (flags & VBI_RTL) ? column0 : column1;
		else
			xl = INT_MAX;

		doubleh0 = doubleh;

		chars = 0;
		spaces = 0;
		doubleh = 0;

		for (x = xs; x != xe; x += adv) {
			vbi_char ac = pg->text[y * pg->columns + x];

			if (ac.flash && (flags & VBI_FLASH_OFF))
				ac.unicode = 0x0020;

			if (ac.conceal && !(flags & VBI_REVEAL))
				ac.unicode = 0x0020;

			if (flags & VBI_TABLE) {
				if (ac.size > VBI_DOUBLE_SIZE)
					ac.unicode = 0x0020;
			} else {
				switch (ac.size) {
				case VBI_NORMAL_SIZE:
				case VBI_DOUBLE_WIDTH:
					break;

				case VBI_DOUBLE_HEIGHT:
				case VBI_DOUBLE_SIZE:
					doubleh++;
					break;

				case VBI_OVER_TOP:
				case VBI_OVER_BOTTOM:
					continue;

				case VBI_DOUBLE_HEIGHT2:
				case VBI_DOUBLE_SIZE2:
					if (y > row0)
						ac.unicode = 0x0020;
					break;
				}

				/*
				 *  Special case two lines row0 ... row1, and all chars
				 *  in row0, column0 ... column1 are double height: Skip
				 *  row1, don't wrap around.
				 */
				if (x == xl && doubleh >= chars) {
					xe = xl;
					y = row1;
				}

				if (ac.unicode == 0x0020 || !vbi_is_print (ac.unicode)) {
					spaces++;
					chars++;
					continue;
				} else {
					if (spaces < chars || y == row0) {
						if (!print_spaces (cd, &p, end, spaces))
							goto failure;
					} else {
						/* discard leading spaces */
						spaces = 0;
					}
				}
			}

			if (!vbi_iconv_unicode (cd, &p, end - p, ac.unicode))
				goto failure;

			chars++;
		}

		if (y < row1) {
			/* Note flags & TABLE implies spaces == 0 */

			if (spaces >= yw) {
				; /* suppress blank line */
			} else {
				; /* discard trailing spaces > 0 */

				if (separator) {
					if (sep_size > (end - p))
						goto failure;

					memcpy (p, separator, sep_size);
					p += sep_size;
				} else {
					if (!vbi_iconv_unicode (cd, &p, end - p,
								(flags & VBI_TABLE) ?
								0x000a : 0x0020))
						goto failure;
				}
			}
		} else {
			if (doubleh0 > 0) {
				; /* pretend this is a blank double height lower row */
			} else {
				/* flush trailing spaces */
				if (!print_spaces (cd, &p, end, spaces))
					goto failure;
			}
		}
	}

	vbi_iconv_close (cd);

	return p - buf;

 failure:
	iconv_close (cd);

	return 0;
}

/**
 * @param pg Source page.
 * @param buf Buffer to hold the output.
 * @param buf_size Size of the buffer in bytes. The function
 *   fails with return value 0 when the text would exceed the
 *   buffer capacity.
 * @param format Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL, the default is "UTF-8".
 * @param flags Set of the following flags.
 *   - @c VBI_TABLE: Scan page in table mode, printing all characters
 *     within the source rectangle including runs of spaces at
 *     the start and end of rows. When @c not given, scan all characters
 *     from @a column, @a row to @a column + @a width - 1,
 *     @a row + @a height - 1 and all intermediate rows to their
 *     full pg->columns width. In this mode runs of spaces at
 *     the start and end of rows collapse into single spaces,
 *     blank lines are suppressed.
 *   - @c VBI_RTL: Scan the page right to left (Hebrew, Arabic),
 *     otherwise left to right.
 *   - @c VBI_REVEAL: Reveal hidden characters otherwise
 *     printed as space.
 *   - @c VBI_FLASH_OFF: Print flashing characters in off state,
 *     i. e. as space.
 *
 * Print a Teletext or Closed Caption vbi_page in the desired
 * format. All character attributes and colors will be lost.
 * (Conversion to terminal control codes is possible using the
 * text export module.) Graphics characters, DRCS and all
 * characters not representable in the target format will be
 * replaced by spaces.
 *
 * This is a specialization of vbi_print_page_region().
 *
 * @return
 * Number of bytes written into @a buf, a value of zero when
 * some error occurred. In this case @a buf may contain incomplete
 * data. Note this function does not append a terminating zero
 * character.
 */
unsigned int
vbi_print_page			(vbi_page *		pg,
				 char *			buf,
				 unsigned int		buf_size,
				 const char *		format,
				 vbi_export_flags	flags)
{
	return vbi_print_page_region (pg, buf, buf_size, format,
				      /* separator */ NULL, /* sep_size */ 0,
				      flags,
				      /* column */ 0, /* row */ 0,
				      pg->columns, pg->rows);
}

static inline uint16_t *
stpcpy2				(uint16_t *		dst,
				 const char *		src)
{
	while ((*dst = *src++))
		dst++;

	return dst;
}

static int
match_color8			(vbi_rgba		color)
{
	unsigned int i, imin = 0;
	int d, dmin = INT_MAX;

	for (i = 0; i < 8; i++) {
		d  = ABS(       (i & 1) * 0xFF - VBI_R (color));
		d += ABS(((i >> 1) & 1) * 0xFF - VBI_G (color));
		d += ABS( (i >> 2)      * 0xFF - VBI_B (color));

		if (d < dmin) {
			dmin = d;
			imin = i;
		}
	}

	return imin;
}

static int
print_char			(text_instance *	text,
				 vbi_page *		pg,
				 vbi_char		old,
				 vbi_char		this)
{
	uint16_t buf[32];
	uint16_t *p;
	vbi_char chg, off;
	char *d;

	p = buf;

	if (text->term != TERMINAL_NONE) {
		assert (sizeof (vbi_char) == 8);
 		union {
			vbi_char		c;
			uint64_t		i;
		} u_old, u_tmp, u_this;

 		u_old.c = old;
 		u_this.c = this;
  
 		u_tmp.i = u_old.i ^ u_this.i; chg = u_tmp.c;
 		u_tmp.i = u_tmp.i &~u_this.i; off = u_tmp.c;

		/* Control sequences based on ECMA-48,
		   http://www.ecma-international.org/ */

		if (chg.size)
			switch (this.size) {
			case VBI_NORMAL_SIZE:
				p = stpcpy2 (p, "\e#5");
				break;
			case VBI_DOUBLE_WIDTH:
				p = stpcpy2 (p, "\e#6");
				break;
			case VBI_DOUBLE_HEIGHT:
			case VBI_DOUBLE_HEIGHT2:
				break; /* ignore */
			case VBI_DOUBLE_SIZE:
				p = stpcpy2 (p, "\e#3");
				break;
			case VBI_DOUBLE_SIZE2:
				p = stpcpy2 (p, "\e#4");
				break;
			case VBI_OVER_TOP:
			case VBI_OVER_BOTTOM:
				return -1; /* don't print */
			}

		/* SGR sequence */

		p = stpcpy2 (p, "\e["); /* CSI */

		if (text->term == TERMINAL_VT100) {
			if (off.underline || off.bold || off.flash) {
				*p++ = ';'; /* \e[0; reset */
				chg.underline = this.underline;
				chg.bold = this.bold;
				chg.flash = this.flash;
				chg.foreground = ~0;
				chg.background = ~0;
			}
		}

		if (chg.bold) {
			if (this.bold)
				p = stpcpy2 (p, "1;"); /* bold */
			else
				p = stpcpy2 (p, "22;"); /* bold off */
		}

		if (chg.italic) {
			if (!this.italic)
				*p++ = '2'; /* off */
			p = stpcpy2 (p, "3;"); /* italic */
		}

		if (chg.underline) {
			if (!this.underline)
				*p++ = '2'; /* off */
			p = stpcpy2 (p, "4;"); /* underline */
		}

		if (chg.flash) {
			if (!this.flash)
				*p++ = '2'; /* steady */
			p = stpcpy2 (p, "5;"); /* slowly blinking */
		}


		/* ECMA-48 SGR offers conceal/reveal code, but we don't
		   know if the terminal implements this correctly as
		   requested by the caller. Proportional spacing needs
		   further investigation. */

		if (chg.foreground) {
			p = stpcpy2 (p, "30;");
			p[-2] += match_color8 (pg->color_map[this.foreground]);
		}

		if (chg.background) {
			p = stpcpy2 (p, "40;");
			p[-2] += match_color8 (pg->color_map[this.background]);
		}

		if (p[-1] == '[')
			p -= 2; /* no change */
		else
			p[-1] = 'm'; /* replace last semicolon */
	}

	if (!vbi_is_print (this.unicode)) {
		if (vbi_is_gfx (this.unicode))
			*p = text->gfx_chr;
		else
			*p = 0x0020;
	} else {
		*p = this.unicode;
	}

	p++;

	d = text->buf;

	if (!vbi_iconv (text->cd, &d, sizeof (text->buf), buf, p - buf)) {
		vbi_export_write_error (&text->export);
		return 0;
	}

	return d - text->buf;
}

static vbi_bool
export				(vbi_export *		e,
				 FILE *			fp,
				 vbi_page *		pg)
{
	text_instance *text = PARENT (e, text_instance, export);
	vbi_page page;
	vbi_char *cp, old;
	int column, row, n;
	const char *charset;

	if (text->charset && text->charset[0])
		charset = text->charset;
	else
		charset = iconv_formats[text->format];

	text->cd = vbi_iconv_open (charset, NULL, 0); // XXX dstp

	if (text->cd == (iconv_t) -1) {
		vbi_export_error_printf (&text->export,
			_("Character conversion Unicode (UCS-2) to %s not supported."),
			charset);

		return FALSE;
	}

	page = *pg;

	/* optimize */

	SET (old);

	for (cp = page.text, row = 0;;) {
		char *d;

		for (column = 0; column < pg->columns; column++) {
			n = print_char(text, &page, old, *cp);

			if (n < 0) {
				; /* skipped */
			} else if (n == 0) {
				goto error2;
			} else if (n == 1) {
				fputc (text->buf[0], fp);
			} else {
				fwrite (text->buf, 1, n, fp);
			}

			old = *cp++;
		}

		row++;

		d = text->buf;

		if (row >= pg->rows) {
			if (text->term > 0) {
				static const uint16_t buf[] = { '\e', '[', 'm', '\n' };

				if (!vbi_iconv (text->cd, &d, sizeof (text->buf), buf, 4))
					goto error;
			} else {
				if (!vbi_iconv_unicode (text->cd, &d, sizeof (text->buf), '\n'))
					goto error;
			}

			fwrite (text->buf, 1, d - text->buf, fp);

			break;
		} else {
			if (!vbi_iconv_unicode (text->cd, &d, sizeof (text->buf), '\n'))
				goto error;

			fwrite (text->buf, 1, d - text->buf, fp);
		}
	}

	vbi_iconv_close (text->cd);

	return !ferror (fp);

 error:
	vbi_export_write_error (&text->export);
 error2:
	iconv_close (text->cd);
	return FALSE;
}

static vbi_export_info
info_text = {
	.keyword	= "text",
	.label		= N_("Text"),
	.tooltip	= N_("Export this page as text file"),

	.mime_type	= "text/plain",
	.extension	= "txt",
};

vbi_export_class
vbi_export_class_text = {
	._public		= &info_text,
	._new			= text_new,
	._delete		= text_delete,
	.option_enum		= option_enum,
	.option_get		= option_get,
	.option_set		= option_set,
	.export			= export
};
