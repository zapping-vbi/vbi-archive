/*
 *  libzvbi
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

/* $Id: pdc.c,v 1.1.2.12 2006-05-07 06:04:58 mschimek Exp $ */

#include "../site_def.h"

#include <stdlib.h>		/* malloc() */
#include <ctype.h>
#include <assert.h>
#include "misc.h"
#include "hamm.h"		/* vbi3_ipar8() */
#include "bcd.h"		/* vbi3_is_bcd() */
#include "pdc.h"
#include "conv.h"
#include "page-priv.h"

/**
 * @addtogroup ProgramID PDC/XDS Program ID Decoder
 * @ingroup LowDec
 */

/**
 * @internal
 * @param pil vbi3_pil to print.
 * @param fp Destination stream.
 *
 * Prints a vbi3_pil as service code or date and time string without
 * trailing newline. This is intended for debugging.
 */
void
_vbi3_pil_dump			(vbi3_pil		pil,
				 FILE *			fp)
{
	switch (pil) {
	case VBI3_PIL_TIMER_CONTROL:
		fputs ("Timer-control (no PDC)", fp);
		break;

	case VBI3_PIL_INHIBIT_TERMINATE:
		fputs ("Recording inhibit/terminate", fp);
		break;

	case VBI3_PIL_INTERRUPT:
		fputs ("Interruption", fp);
		break;

	case VBI3_PIL_CONTINUE:
		fputs ("Continue", fp);
		break;

	default:
		fprintf (fp, "%05x (%02u-%02u %02u:%02u)",
			 pil,
			 VBI3_PIL_MONTH (pil),
			 VBI3_PIL_DAY (pil),
			 VBI3_PIL_HOUR (pil),
			 VBI3_PIL_MINUTE (pil));
		break;
	}
}

/**
 * @internal
 * @param pid vbi3_program_id structure to print.
 * @param fp Destination stream.
 *
 * Prints the contents of a vbi3_program_id structure as a string
 * without trailing newline. This is intended for debugging.
 */
void
_vbi3_program_id_dump		(const vbi3_program_id *	pid,
				 FILE *			fp)
{
	static const char *pcs_audio [] = {
		"UNKNOWN",
		"MONO",
		"STEREO",
		"BILINGUAL"
	};

	fprintf (fp, "cni=%04x (%s) ch=%u ",
		 pid->cni,
		 vbi3_cni_type_name (pid->cni_type),
		 pid->channel);

	if (0) {
		fprintf (fp, "%02u-%02u %02u:%02u pil=",
			 pid->month + 1, pid->day + 1,
			 pid->hour, pid->minute);
	} else {
		fprintf (fp, "pil=");
	}

	_vbi3_pil_dump (pid->pil, fp);

	fprintf (fp, " length=%u "
		 "luf=%u mi=%u prf=%u "
		 "pcs=%s pty=%02x td=%u",
		 pid->length,
		 pid->luf, pid->mi, pid->prf,
		 pcs_audio[pid->pcs_audio],
		 pid->pty, pid->tape_delayed);
}

vbi3_bool
_vbi3_str_to_pil		(vbi3_pil *		pil,
				 const char **		inout_s)
{
	static const _vbi3_key_value_pair symbols [] = {
		{ "timer",	VBI3_PIL_TIMER_CONTROL },
		{ "inhibit",	VBI3_PIL_INHIBIT_TERMINATE },
		{ "interrupt",	VBI3_PIL_INTERRUPT },
		{ "continue",	VBI3_PIL_CONTINUE },
		{ NULL, 0 }
	};
	const char *s;
	unsigned int value[4];
	unsigned int i;

	assert (NULL != pil);

	if (_vbi3_keyword_lookup (pil, inout_s, symbols))
		return TRUE;

	s = *inout_s;

	while (isspace (*s))
		++s;

	for (i = 0; i < 4; ++i) {
		if (!isdigit (s[0])
		    || !isdigit (s[1]))
			return FALSE;

		value[i] = (s[0] - '0') * 10 + s[1] - '0';

		s += 2;

		if (i < 3) {
			while (isspace (*s))
				++s;
			if (*s == "-T:"[i]) {
				do ++s;
				while (isspace (*s));
			}
		}
	}

	if ((value[0] - 1) > 11
	    || (value[1] - 1) > 30
	    || value[2] > 23
	    || value[3] > 59)
		return FALSE;

	*inout_s = s;

	*pil = VBI3_PIL (value[0], value[1], value[2], value[3]);

	return TRUE;
}

/**
 * @param pid vbi3_program_id structure initialized with vbi3_program_id_init().
 *
 * Frees all resources associated with @a pid, except the structure itself.
 */
void
vbi3_program_id_destroy		(vbi3_program_id *	pid)
{
	assert (NULL != pid);

	CLEAR (*pid);
}

/**
 * @param pid vbi3_program_id structure to initialize.
 * @param channel Logical channel number to be stored in the
 *   channel field.
 *
 * Initializes a vbi3_program_id structure.
 */
void
vbi3_program_id_init		(vbi3_program_id *	pid,
				 vbi3_pid_channel	channel)
{
	assert (NULL != pid);

	pid->cni_type		= VBI3_CNI_TYPE_UNKNOWN;
	pid->cni		= 0;

	pid->channel		= channel;

	pid->month		= 0;
	pid->day		= 0;
	pid->hour		= 0;
	pid->minute		= 0;

	pid->pil		= VBI3_PIL_TIMER_CONTROL;

	pid->length		= 0;

	pid->luf		= FALSE;
	pid->mi			= FALSE;
	pid->prf		= FALSE;

	pid->pcs_audio		= VBI3_PCS_AUDIO_UNKNOWN;

	pid->pty		= 0; /* none / unknown */

	pid->tape_delayed	= FALSE;
}

/**
 * @internal
 * @param pid vbi3_preselection structure to print.
 * @param fp Destination stream.
 *
 * Prints the contents of a vbi3_preselection structure as a string
 * without trailing newline. This is intended for debugging.
 */
void
_vbi3_preselection_dump		(const vbi3_preselection *p,
				 FILE *			fp)
{
	unsigned int i;
	unsigned int end_hour;
	unsigned int end_minute;

	end_hour = p->at1_hour;
	end_minute = p->at1_minute;

	end_minute += p->length;
	if (end_minute >= 60) {
		end_hour = (end_hour + end_minute / 60) % 24;
		end_minute %= 60;
	}

	fprintf (fp,
		 "cni=%05x (%s) "
		 "%04d-%02d-%02d %02d:%02d (%02d:%02d) %02d:%02d "
		 "pty=%02x lto=%d caf=%u at1/ptl=",
		 p->cni,
		 vbi3_cni_type_name (p->cni_type),
		 p->year, p->month, p->day,
		 p->at1_hour, p->at1_minute,
		 p->at2_hour, p->at2_minute,
		 end_hour, end_minute,
		 p->pty, p->lto, p->caf);

	for (i = 0; i < 4; ++i)
		fprintf (fp, "%02d:%02d-%02d%c",
			 p->_at1_ptl[i].row,
			 p->_at1_ptl[i].column_begin,
			 p->_at1_ptl[i].column_end,
			 (i == 2) ? ' ' : ',');

	fprintf (fp, "pgno=%03x title=\"", p->pgno);

	vbi3_fputs_locale_utf8 (fp, p->title, VBI3_NUL_TERMINATED);

	fputc ('"', fp);
}

/**
 * @internal
 * @param pid Array of vbi3_preselection structures to print.
 * @param n_elements Number of structures in the array.
 * @param fp Destination stream.
 *
 * Prints the contents of a vbi3_preselection structure array,
 * one structure per line. This is intended for debugging.
 */
void
_vbi3_preselection_array_dump	(const vbi3_preselection *p,
				 unsigned int		n_elements,
				 FILE *			fp)
{
	unsigned int count;

	count = 0;

	while (n_elements-- > 0) {
		fprintf (fp, "%2u: ", count++);
		_vbi3_preselection_dump (p++, fp);
		fputc ('\n', fp);
	}
}

/**
 */
void
vbi3_preselection_destroy	(vbi3_preselection *	p)
{
	assert (NULL != p);

	vbi3_free (p->title);

	CLEAR (*p);
}

/**
 */
vbi3_bool
vbi3_preselection_copy		(vbi3_preselection *	dst,
				 const vbi3_preselection *src)
{
	if (dst == src)
		return TRUE;

	assert (NULL != dst);

	if (src) {
		char *title;

		title = NULL;
		if (NULL != src->title) {
			title = strdup (src->title);
			if (NULL == title)
				return FALSE;
		}

		*dst = *src;
		dst->title = title;
	} else {
		CLEAR (*dst);
	}

	return TRUE;
}

/**
 */
vbi3_bool
vbi3_preselection_init		(vbi3_preselection *	p)
{
	assert (NULL != p);

	CLEAR (*p);

	return TRUE;
}

/**
 * @internal
 */
void
_vbi3_preselection_array_delete	(vbi3_preselection *	p,
				 unsigned int		n_elements)
{
	unsigned int i;

	if (NULL == p || 0 == n_elements)
		return;

	for (i = 0; i < n_elements; ++i)
		vbi3_preselection_destroy (p + i);

	vbi3_free (p);
}

/**
 * @internal
 */
vbi3_preselection *
_vbi3_preselection_array_dup	(const vbi3_preselection *p,
				 unsigned int		n_elements)
{
	vbi3_preselection *new_p;
	unsigned int i;

	if (!(new_p = vbi3_malloc (n_elements * sizeof (*new_p))))
		return NULL;

	memcpy (new_p, p, n_elements * sizeof (*new_p));

	for (i = 0; i < n_elements; ++i) {
		if (NULL != p[i].title) {
			new_p[i].title = strdup (p[i].title);
			if (NULL == new_p[i].title) {
				goto failure;
			}
		}
	}

	return new_p;

 failure:
	while (i-- > 0) {
		vbi3_free (new_p[i].title);
	}

	vbi3_free (new_p);

	return NULL;
}

/**
 * @internal
 */
vbi3_preselection *
_vbi3_preselection_array_new	(unsigned int		n_elements)
{
	vbi3_preselection *p;

	p = vbi3_malloc (n_elements * sizeof (vbi3_preselection));

	memset (p, 0, n_elements * sizeof (vbi3_preselection));

	return p;
}

/*
    PDC Preselection method "A"
    ETS 300 231, Section 7.3.1.

    (Method "B" code has been merged into the Teletext
     enhancement routines in teletext.c)
*/

/*
type	pre	text		____          post           ____
        1)                     /                                 \
AT-1	+	zz.zz		+	%			<	2)
AT-1	+	zz.zz-zz.zz	+	%			<	2)
PTL	++	title		++	%%	::	:%	<	6)
PW	%	hh		+	%			<	3)
LTO	%	0zz		+	%			<
LTO	%	9zz		+	%			<
PTY	%	Fhh		+	%			<	4)
AT-2	%	zzzz		+	%			<
AT-1	%	zz.zz		+	%			<	2)
CNI	%	hhzzz		+	%			<	3)
AD	%	zzzzzz		+	%			<	3)
PTL	%%	title		++	%%	::	:%	<	6)
AT-2	:	zzzz		+	%			<
AT-1	:	zz.zz		+	%			<	5)
AD	:	zzzzzz		+	%			<

Combined form: <pre><text><pre><text>...<pre><text><post>

+  colour code except magenta
:  magenta
%  conceal
<  end of row

Some observations:

0) Table 4 is wrong: '%' = 0x18; ',' = '+' | '%' | '<'

1) Any control or other code can appear before the prefix, in
   particular :% (as noted in spec) and :: (equal to single
   colon) are valid prefixes.

2) I encountered pages which have proper CNI and AD, but no AT-1.
   Here random non-digit characters (usually space 0x20) can
   appear in place of the + before and/or after AT-1. Also
   encountered spaces before and/or after the minus, and a colon
   instead of the period.

3) The % prefix form is permitted only when CNI, AD, PW combine,
   otherwise :% should appear. Some stations also use space
   0x20 as delimiter between combined CNI, AD, PW
   (<pre><text>0x20<text>...).

4) According to spec only the :% form is valid. (No evidence
   to the contrary, but that would be an unnecessary exception.
   Let's go by rule 1) instead.)

5) Encountered as end time of the last program on a page.
   Note 2) applies accordingly.

6) Postfix :% encountered in combined form ++PTL:%CNI.

*/

#define M_COLOR ((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3) | \
		 (1 << 4) | (1 << 6) | (1 << 7))
#define M_MAGENTA (1 << 0x05)
#define M_CONCEAL (1 << 0x18)

#define is_num(c) ((c) >= 0x30 && (c) <= 0x39)
/* NB uppercase only, unlike ctype.h isxdigit() */
#define is_xnum(c) (is_num (c) || ((c) >= 0x41 && (c) <= 0x46))
#define is_colon(c) ((c) == 0x3A)
#define is_period(c) ((c) == 0x2E)
#define is_minus(c) ((c) == 0x2D)

/* Caution! These macros assume c is a C0 (0x00 ... 0x1F) */
#define is_control(c) ((M_COLOR | M_MAGENTA | M_CONCEAL) & (1 << (c)))
#define is_magenta(c) ((c) == 0x05)
#define is_conceal(c) ((c) == 0x18)
#define is_colour_conceal(c) ((M_COLOR | M_CONCEAL) & (1 << (c)))
#define is_magenta_conceal(c) ((M_MAGENTA | M_CONCEAL) & (1 << (c)))

#ifndef PDC_LOG
#  define PDC_LOG 0
#endif

#define pdc_log(templ, args...)						\
do {									\
	if (PDC_LOG)							\
		fprintf (stderr, templ , ##args);			\
} while (0)

/**
 * @internal
 * @param hour Returns hour 0 ... 23.
 * @param minute Returns minute 0 ... 59.
 * @param bcd Four digit time value, e.g. 0x2359.
 *
 * Converts BCD coded time to pdc_time.
 *
 * @returns
 * FALSE if @a bcd is invalid.
 */
static vbi3_bool
pdc_time_from_bcd		(unsigned int *		hour,
				 unsigned int *		minute,
				 int			bcd,
				 const struct _vbi3_network_pdc *pdc)
{
	int h, m;

	if (vbi3_bcd_digits_greater (bcd, 0x2959))
		return FALSE;

	m = (bcd & 15) + ((bcd >> 4) & 15) * 10;

	bcd >>= 8;

	h = (bcd & 15) + (bcd >> 4) * 10;

	if (h >= 24)
		return FALSE;

	if (NULL != pdc) {
		m += pdc->time_offset;
		if (m >= 60) {
			h = (h + m / 60) % 24;
			m %= 60;
		}
	}

	*hour = h;
	*minute = m;

	return TRUE;
}

static unsigned int
pdc_duration			(unsigned int		begin_hour,
				 unsigned int		begin_minute,
				 unsigned int		end_hour,
				 unsigned int		end_minute)
{
	int d;

	d = (end_hour - begin_hour) * 60
		+ end_minute - begin_minute;

	if (d < 0)
		d += 24 * 60;

	return (unsigned int) d;
}

/* Record position of up to three PTLs. Entry #0
   stores AT-1 position in case there is no PTL. */
static void
store_ptl			(vbi3_preselection *	table,
				 vbi3_preselection *	p,
				 unsigned int		row,
				 unsigned int		column_begin,
				 unsigned int		column_end)
{
	if (p > table) {
		unsigned int i;

		for (i = 1; i < 4; ++i) {
			if (p[-1]._at1_ptl[i].row <= 0) {
				if (i >= 2) {
					if (p[-1]._at1_ptl[i - 1].row
					    < (row - 1))
						break; /* probably unrelated */
				}

				p[-1]._at1_ptl[i].row = row;
				p[-1]._at1_ptl[i].column_begin = column_begin;
				p[-1]._at1_ptl[i].column_end = column_end;

				break;
			}
		}
	}
}

/**
 * @internal
 * @param table Table we write.
 * @param p Current vbi3_preselection entry in table, store PTL info here.
 * @param raw Raw level one page, current row.
 * @param row Current position.
 * @param column Current position >= 1.
 *
 * Evaluates a PTL, subfunction of pdc_method_a().
 *
 * @returns
 * Next column to process >= @a column, or 0 on error.
 */
static unsigned int
pdc_ptl				(vbi3_preselection *	table,
				 vbi3_preselection *	p,
				 const uint8_t *	raw,
				 unsigned int		row,
				 unsigned int		column)
{
	unsigned int column_begin;
	unsigned int column_end;
	unsigned int column_at1;
	int ctrl1;
	int ctrl2;

	column_begin = column;
	column_end = 40;
	column_at1 = 40;

	ctrl2 = vbi3_unpar8 (raw[column]);

	for (;;) {
		if (column >= 39) {
			/* Line end postfix */
			column = 40;
			break;
		}

		ctrl1 = ctrl2;
		ctrl2 = vbi3_unpar8 (raw[column + 1]);

		if (0)
			pdc_log ("%d %d %02x-%02x\n",
				 row, column, ctrl1, ctrl2);

		if (ctrl2 < 0)
			return 0; /* hamming error */

		if (ctrl1 < 0x20 && is_control (ctrl1)) {
			if (ctrl1 == ctrl2) {
				column_end = column;
				column += 2;
				break;
			} else if (is_magenta (ctrl1)
				   && is_conceal (ctrl2)) {
				/* Observation 6) */
				column_end = column;
				break;
			}
		} else if (is_num (ctrl1)
			   && is_num (ctrl2)
			   && column <= 35
			   && column_at1 == 40) {
			int separator = vbi3_unpar8 (raw[column + 2]);

			if (is_colon (separator)
			    || is_period (separator)) {
				int digit3 = vbi3_unpar8 (raw[column + 3]);
				int digit4 = vbi3_unpar8 (raw[column + 4]);
				unsigned int delimiter;

				delimiter = (column < 35) ?
					vbi3_unpar8 (raw[column + 5]) : 0x20;

				if (is_num (digit3)
				    && is_num (digit4)
				    && delimiter < 0x30) {
					/* Unmarked AT-1 (end time?),
					   observation 2), within PTL. */
					column_at1 = column;
				}
			}
		}

		++column;
	}

	pdc_log ("PTL %d %d-%d\n", row, column_begin, column_end);

	store_ptl (table, p, row, column_begin, column_end);

	/* No explicit postfix, must scan for unmarked
	   elements within PTL. */
	if (40 == column)
		column = column_at1;

	return column;
}

/**
 * @internal
 * @param value Output BCD value of number, 2, 3, 4, 6 or 8 digits.
 * @param digits Output number of digits plus number of
 *   separators * 256. For example "23.59" -> 0x104.
 * @param raw Raw level one page, current row.
 * @param column In & out current position.
 *
 * Evaluates a number, subfunction of pdc_method_a().
 *
 * @returns
 * FALSE on error.
 */
static unsigned int
pdc_number			(unsigned int * const	value,
				 unsigned int * const	digits,
				 const uint8_t *	raw,
				 unsigned int * const	column)
{
	*value = 0;
	*digits = 0;

	while (*column <= 39) {
		int c = vbi3_unpar8 (raw[(*column)++]);

		if (0)
			pdc_log ("%d %02x value=%x digits=%x\n",
				 *column - 1, c, *value, *digits);

		switch (c) {
		case 0x41 ... 0x46: /* xdigit */
			c += 0x3A - 0x41;
			/* fall through */

		case 0x30 ... 0x39: /* digit */
			*value = *value * 16 + c - 0x30;
			*digits += 1;
			break;

		case 0x01 ... 0x07: /* color */
		case 0x18: /* conceal */
			/* Postfix control code */
			--(*column);
			return TRUE;

		case 0x20: /* space, observation 2) */
			for (; *column <= 39; ++(*column)) {
				c = vbi3_par8 (raw[*column]);
				if (c != 0x20)
					break;
			}

			switch (*digits) {
			case 0x104: /* zz.zz */
				if (*column <= 34 && is_minus (c))
					continue; /* zz.zz<spaces>- */
				else
					return TRUE;

			case 0x204: /* zz.zz- */
				if (*column <= 35 && is_num (c))
					continue; /* zz.zz-<spaces>z */
				else
					return TRUE;

			default:
				break;
			}

			return TRUE;

		case 0x2E: /* period */
		case 0x3A: /* colon, observation 2) */
			switch (*digits) {
			case 0x001: /*  z. */
			case 0x002: /* zz. */
			case 0x206: /* zz.zz-zz. */
				*digits += 0x100;
				break;

			default:
				return FALSE;
			}

			break;

		case 0x2D: /* minus */
			if (0x104 == *digits) {
				*digits = 0x204; /* zz.zz- */
			} else {
				return FALSE;
			}
			
			break;

		default: /* junk (incl parity error), observation 2) */
			switch (*digits) {
			case 0x103: /*  z.zz (seen on RTL2, not PDC-A) */
			case 0x104: /* zz.zz */
			case 0x308: /* zz.zz-zz.zz */
				--(*column);
				return TRUE;

			default:
				return FALSE;
			}

			break;
		}
	}

	return TRUE;
}

static void
add_n_days			(unsigned int *		year_inout,
				 unsigned int *		month_inout,
				 unsigned int *		day_inout,
				 unsigned int		n_days)
{
	struct tm tm;
	time_t t;

	if (0 == n_days)
		return;
				 
	CLEAR (tm);
	tm.tm_mday = *day_inout;
	tm.tm_mon = *month_inout - 1;
	tm.tm_year = *year_inout - 1900;

	t = mktime (&tm) + n_days * (24 * 60 * 60);
	localtime_r (&t, &tm);

	*day_inout = tm.tm_mday;
	*month_inout = tm.tm_mon + 1;
	*year_inout = tm.tm_year + 1900;
}

/** @internal */
static vbi3_preselection *
pdc_at2_fill			(vbi3_preselection *		table,
				 vbi3_preselection *		begin,
				 vbi3_preselection *		end,
				 vbi3_preselection *		src,
				 const struct _vbi3_network_pdc *pdc)
{
	vbi3_preselection *p;
	unsigned int prev_hour;

	/* FIXME start of day (default 0600),
	   make this variable. */
	prev_hour = 0x06;
	if (begin > table)
		prev_hour = begin[-1].at1_hour;

	for (p = begin; p < end; ++p) {
		p->cni_type	= src->cni_type;
		p->cni		= src->cni;
		p->year		= src->year;
		p->month	= src->month;
		p->day		= src->day;
		p->lto		= src->lto;

		p->at2_hour	= p->at1_hour;
		p->at2_minute	= p->at1_minute;

		if (pdc && pdc->date_format) {
			if (p->at1_hour < prev_hour) {
				add_n_days (&p->year, &p->month, &p->day, 1);
				src = p;
			}

			prev_hour = p->at1_hour;
		}
	}

	return p;
}

/**
 * Removes the pointer to another Teletext page from a PDC title,
 * returning the page number or 0. Recognized formats are (regex):
 *
 * (\s+\.|\s*\.\.+)?\s*>?NNN\s*$
 */
static vbi3_pgno
title_remove_pgno		(const vbi3_char *	begin,
				 const vbi3_char **	pend)
{
	const vbi3_char *cp;
	const vbi3_char *cp_pgno;

	cp = *pend - 1;
	while (cp >= begin && 0x0020 == cp->unicode)
		--cp;

	*pend = cp + 1;

	if (cp - begin < 2
	    || cp[-2].unicode < 0x0031 || cp[-2].unicode > 0x0038
	    || cp[-1].unicode < 0x0030 || cp[-1].unicode > 0x0039
	    || cp[ 0].unicode < 0x0030 || cp[ 0].unicode > 0x0039)
		return 0;

	cp_pgno = cp -= 3;

	if (cp >= begin && '>' == cp->unicode)
		--cp;

	while (cp >= begin && 0x0020 == cp->unicode)
		--cp;

	if (cp - begin >= 1
	    && '.' == cp[ 0].unicode) {
		if ('.' == cp[-1].unicode) {
			cp -= 2;

			while (cp >= begin && '.' == cp->unicode)
				--cp;

			while (cp >= begin && 0x0020 == cp->unicode)
				--cp;
		} else if (0x0020 == cp[-1].unicode) {
			cp -= 2;

			while (cp >= begin && 0x0020 == cp->unicode)
				--cp;
		}
	}

	if (cp < cp_pgno) {
		*pend = cp + 1;

		return (((cp_pgno[1].unicode & 0xF) << 8) |
			((cp_pgno[2].unicode & 0xF) << 4) |
			 (cp_pgno[3].unicode & 0xF));
	}

	return 0;
}

static void
title_append			(vbi3_preselection *	p,
				 uint16_t *		buffer,
				 uint16_t **		pup,
				 const vbi3_char *	cp,
				 const vbi3_char *	end)
{
	uint16_t *up = *pup;

	if (0 == p->pgno) {
		p->pgno = title_remove_pgno (cp, &end);
	} else {
		while (end > cp && 0x0020 == end[-1].unicode)
			--end;
	}

	if (buffer < up - 2
	    && cp < end
	    && '-' == up[-1]
	    && up[-2] < 256 && isalpha (up[-2])
	    && cp[0].unicode < 256 && isalpha (cp[0].unicode)) {
		/* XXX "co-operate", other languages? */
		--up;
	} else {
		if (up > buffer)
			*up++ = 0x0020;
	}

	while (cp < end) {
		*up++ = cp->unicode;
		++cp;
	}

	*pup = up;
}

/**
 * Determine if the title continues in this row and just isn't
 * marked. Must start in the same column, but we ignore a visible AT-2.
 */
static vbi3_bool
title_continues			(const vbi3_char *	cp,
				 unsigned int		column)
{
	const vbi3_char *end;

	for (end = cp + column; cp < end; ++cp) {
		if (VBI3_UNDERLINE & cp->attr)
			return FALSE;

		if (0x0020 == cp->unicode)
			continue;

		if (VBI3_CONCEAL & cp->attr)
			continue;

		/* Seen on RTL as a "bullet". */
		if (vbi3_is_gfx (cp->unicode))
			continue;

		if (end - cp >= 5) {
			unsigned int at2;
			unsigned int i;

			at2 = 0;

			for (i = 0; i < 5; ++i) {
				if (VBI3_MAGENTA != cp[i].foreground
				    || cp[i].unicode < '0'
				    || cp[i].unicode > '9')
					return FALSE;
				at2 = at2 * 16 + cp[i].unicode - '0';
			}

			if (at2 > 0x23FF
			    || (at2 & 0xFF) > 0x59
			    || 0x0020 != cp[5].unicode)
				return FALSE;

			cp += 5;

			continue;
		}

		return FALSE;
	}

	return (0 == (VBI3_UNDERLINE & cp->attr)
		&& !vbi3_is_gfx (cp->unicode)
		&& 0x0020 != cp->unicode);
}

void
_vbi3_pdc_title_post_proc	(vbi3_page *		pg,
				 vbi3_preselection *	p)
{
	struct _vbi3_at1_ptl at1_ptl[N_ELEMENTS (p->_at1_ptl)];
	uint16_t *buffer;
	uint16_t *up;
	unsigned int cont_column;
	unsigned int i;

	p->pgno = 0;

	buffer = malloc ((pg->rows * pg->columns) * sizeof (*buffer));
	if (NULL == buffer)
		return;

	up = buffer;
	cont_column = pg->columns;

	memcpy (at1_ptl, p->_at1_ptl, sizeof (at1_ptl));

	if (0 == at1_ptl[1].row
	    && 0 != at1_ptl[0].row) {
		/* Unmarked title. Maybe there's something useful
		   after the start time. */
		at1_ptl[1].row = at1_ptl[0].row;
		at1_ptl[1].column_begin = at1_ptl[0].column_end;
		at1_ptl[1].column_end = 40;
	}

	for (i = 1; i < 4; ++i) {
		const vbi3_char *cp;
		unsigned int j;

		if (0 == at1_ptl[i].row) {
			unsigned int row;

			if (cont_column >= pg->columns - 1)
				break;

			row = at1_ptl[i - 1].row + i - 1;
			cp = pg->text + row * pg->columns;

			while (row < pg->rows
			       && title_continues (cp, cont_column)) {
				title_append (p, buffer, &up,
					      cp + cont_column,
					      cp + 40);
				cp += pg->columns;
				++row;
			}

			break;
		} else if (at1_ptl[i].row >= pg->rows
			   || at1_ptl[i].column_begin >= pg->columns
			   || at1_ptl[i].column_end > pg->columns) {
			cont_column = pg->columns;
			continue;
		}

		cp = pg->text + at1_ptl[i].row * pg->columns;

		for (j = at1_ptl[i].column_begin;
		     j < at1_ptl[i].column_end; ++j) {
			if (0x0020 != cp[j].unicode)
				break;
		}

		if (40 != at1_ptl[i].column_end) {
			cont_column = pg->columns;
		} else if (1 == i) {
			cont_column = j;
		} else if (j != cont_column) {
			cont_column = pg->columns;
		}

		title_append (p, buffer, &up,
			      cp + j,
			      cp + at1_ptl[i].column_end);
	}

	vbi3_free (p->title);

	/* Error ignored. */
	p->title = vbi3_strndup_utf8_ucs2 (buffer, up - buffer);

	vbi3_free (buffer);
	buffer = NULL;
}

/**
 * Extracts a free-format date string from a Teletext page.
 */
static vbi3_bool
ff_date_string			(vbi3_preselection *	p2,
				 const struct _vbi3_network_pdc *pdc,
				 const uint8_t		lop_raw[26][40])
{
	int16_t raw[40];
	int16_t *s1;
	int16_t *end;
	const uint8_t *q;
	unsigned int day;
	unsigned int month;
	unsigned int year;
	int err;

	s1 = raw;
	end = raw + N_ELEMENTS (raw);

	if ((unsigned int) pdc->date_row > 24)
		return FALSE;

	q = lop_raw[pdc->date_row];

	err = 0;

	while (s1 < end) {
		int c;

		c = vbi3_unpar8 (*q++);
		*s1++ = c;
		err |= c;
	}

	if (err < 0)
		return FALSE; /* hamming error */

	day = 0;
	month = 0;
	year = 0;

	for (s1 = raw; s1 < end; ++s1) {
		const int16_t *s;
		const char *f;

		f = pdc->date_format;

		for (s = s1; s < end; ++s) {
			switch (*f++) {
				unsigned int number;
				unsigned int n_digits;
				struct tm tm;
				time_t t;

			case 0:
				if (0 == day || 0 == month)
					break;

				p2->day = day;
				p2->month = month;

				if (0 == year) {
					t = time (NULL); 
					gmtime_r (&t, &tm);

					if ((unsigned int) tm.tm_mon + 1
					    < month) {
						year = 1901 + tm.tm_year;
					} else {
						year = 1900 + tm.tm_year;
					}
				}

				p2->year = year;

				add_n_days (&p2->year,
					    &p2->month,
					    &p2->day,
					    pdc->time_offset / (24 * 60));

				return TRUE;

			case 0x20:
				if (*s <= 0x20)
					continue;
				break;

			case '%':
				if ('%' == *f) {
					if ('%' == *s) {
						++f;
						continue;
					}

					break;
				}

				number = 0;
				n_digits = 0;

				for (; s < end; ++s) {
					if (*s < '0' || *s > '9')
						break;

					number = number * 10 + *s - '0';
					++n_digits;
				}

				if (n_digits <= 0)
					break;

				--s;

				switch (*f++) {
				case 'd':
					if (n_digits <= 2
					    && number >= 1
					    && number <= 31) {
						day = number;
						continue;
					}

					break;

				case 'm':
					if (n_digits <= 2
					    && number >= 1
					    && number <= 12) {
						month = number;
						continue;
					}

					break;

				case 'y':
					if (2 == n_digits) {
						year = 2000 + number;
						continue;
					}

					break;

				default:
					break;
				}

			default:
				if (*s == f[-1])
					continue;
				break;
			}

			s = end;

			day = 0;
			month = 0;
			year = 0;
		}
	}

	return FALSE;
}

static void
pdc_cni_from_network		(vbi3_preselection *	p2,
				 const vbi3_network *	nk)
{
	if (0 != nk->cni_pdc_a) {
		p2->cni_type = VBI3_CNI_TYPE_PDC_A;
		p2->cni = nk->cni_pdc_a;
	} else if (0 != nk->cni_pdc_b) {
		p2->cni_type = VBI3_CNI_TYPE_PDC_B;
		p2->cni = nk->cni_pdc_b;
	} else if (0 != nk->cni_8301) {
		p2->cni_type = VBI3_CNI_TYPE_8301;
		p2->cni = nk->cni_8301;
	} else if (0 != nk->cni_8302) {
		p2->cni_type = VBI3_CNI_TYPE_8302;
		p2->cni = nk->cni_8302;
	} else if (0 != nk->cni_vps) {
		p2->cni_type = VBI3_CNI_TYPE_VPS;
		p2->cni = nk->cni_vps;
	}
}

static const struct _vbi3_network_pdc *
network_pdc_data		(const vbi3_network *	nk,
				 vbi3_pgno		pgno)
{
	const struct _vbi3_network_pdc *pdc;

	pdc = _vbi3_network_get_pdc (nk);
	if (NULL == pdc)
		return NULL;

	while (pdc->first_pgno > 0) {
		if (pgno >= pdc->first_pgno
		    && pgno <= pdc->last_pgno) {
			if (pdc->date_format)
				return pdc;

			return NULL;
		}

		++pdc;
	}

	return NULL;
}

/**
 * @internal
 * @param table Store PDC data here.
 * @param n_elements Free space in the table.
 * @param lop_raw Raw Teletext level one page.
 *
 * Scans a raw Teletext level one page for PDC data and stores the
 * data in a vbi3_preselection table.
 *
 * @returns
 * Number of entries written in table.
 */
unsigned int
_vbi3_pdc_method_a		(vbi3_preselection *	table,
				 unsigned int		n_elements,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 const uint8_t		lop_raw[26][40])
{
	vbi3_preselection *p1;
	vbi3_preselection *p2;
	vbi3_preselection *pend;
	const struct _vbi3_network_pdc *pdc;
	unsigned int row;
	vbi3_bool have_at2;
	vbi3_bool open_title;
	int pw_sum;
	int pw;

	assert (NULL != table);
	assert (NULL != nk);
	assert (NULL != lop_raw);

	memset (table, 0, n_elements * sizeof (*table));

	p1 = table; /* next AT-1 row */
	p2 = table; /* next AT-2 row */

	pend = table + n_elements;

	p2->year = 0; /* not found */
	p2->cni_type = VBI3_CNI_TYPE_PDC_A;
	p2->cni = 0; /* not found */

	have_at2 = FALSE;
	open_title = FALSE;

	pw_sum = 0;
	pw = -1; /* XXX checksum to do */

	pdc = network_pdc_data (nk, pgno);
	if (NULL != pdc) {
		if (ff_date_string (p2, pdc, lop_raw)) {
			pdc_cni_from_network (p2, nk);
		}
	}

	for (row = 1; row <= 23; ++row) {
		const uint8_t *raw;
		unsigned int column;
		unsigned int column_begin;
		unsigned int value;
		unsigned int digits;
		unsigned int non_space;
		vbi3_bool combine;
		int ctrl0; /* previous */
		int ctrl1; /* current */
		int ctrl2; /* next */

		raw = lop_raw[row];

		ctrl1 = 0xFF; /* nothing at raw[-1] */
		ctrl2 = vbi3_unpar8 (raw[0]);

		combine = FALSE;

		non_space = 0;

		for (column = 0; column <= 38;) {
			int char1;

			ctrl0 = ctrl1;
			ctrl1 = ctrl2;
			char1 = ctrl2;
			ctrl2 = vbi3_unpar8 (raw[column + 1]);

			if (0)
				pdc_log ("%d,%d %02x-%02x-%02x %d\n",
					 row, column, ctrl0, ctrl1,
					 ctrl2, combine);

			if ((ctrl0 | ctrl1 | ctrl2) < 0) {
				return 0; /* hamming error */
			} else if (ctrl1 < 0x20 && is_control (ctrl1)) {
				if (ctrl0 == ctrl1
				    && is_colour_conceal (ctrl1)
				    && ctrl2 >= 0x20) {
					column = pdc_ptl (table,
							  p1, raw,
							  row, column + 1);
					if (column == 0)
						return 0; /* hamming error */
					goto not_number;
				} else if (is_xnum (ctrl2)) {
					++column; /* Numeric item? */
					char1 = ctrl2;
				} else {
					++column;
					combine = FALSE;
					continue;
				}
			} else if (is_xnum (ctrl1) && ctrl0 >= 0x20) {
				/* Possible unmarked AT-1 2),
				   combined CNI, AD, PW 3) */
				char1 = ctrl1;
				ctrl1 = ctrl0;
			} else if (open_title
				   && ctrl1 >= 0x40
				   && ctrl1 <= 0x7E) {
				column_begin = column;
				goto store_open_title;
			} else {
				++column;
				non_space += (ctrl1 > 0x20);
				combine = FALSE;
				continue;
			}

			column_begin = column;
			
			if (!pdc_number (&value, &digits, raw, &column)) {
				goto bad;
			}

			switch (digits) {
			case 0x002:
				if (!((combine && ctrl1 == 0x20)
				      || is_magenta_conceal (ctrl1)))
					goto bad;

				pdc_log ("PW %02x\n", value);

				pw = value;

				combine = TRUE;

				break;

			case 0x003:
				if (!is_conceal (ctrl1))
					goto bad;

				switch (value >> 8) {
				case 0x0:
				case 0x9:
				{
					unsigned int d;

					if (!vbi3_is_bcd (value))
						goto bad;

					pdc_log ("LTO %03x %d\n",
						 value, p2->lto);

					d = (value & 15)
						+ ((value >> 4) & 15) * 10;

					if (d >= 12 * 4)
						goto bad; /* implausible */

					if (!have_at2 && p1 > p2) /* see AD */
						p2 = pdc_at2_fill
							(table, p2, p1,
							 p2, pdc);

					/* 0x999 = -0x001 (ok?) */
					p2->lto = (value >= 0x900) ?
						d - 100: d;
					p2->lto *= 15; /* to minutes */

					break;
				}

				case 0xF:
					if (!is_magenta (ctrl0))
						goto bad;

					pdc_log ("PTY %02x\n", value);

					p2->pty = value & 0xFF;

					break;

				default:
					goto bad;
				}

				break;

			case 0x004:
				if (!is_magenta_conceal (ctrl1))
					goto bad;

				if (value == 0x2500) {
					/* Skip flag */
					p2->at2_hour = 25;
				} else {
					if (!pdc_time_from_bcd
					    (&p2->at2_hour,
					     &p2->at2_minute,
					     value, /* pdc */ NULL))
						goto bad;
				}

				pdc_log ("AT-2 %04x\n", value);

				have_at2 = TRUE;

				if (++p2 >= pend)
					goto finish;

				p2->year	= p2[-1].year;
				p2->month	= p2[-1].month;
				p2->day		= p2[-1].day;
				p2->cni		= p2[-1].cni;
				p2->cni_type	= p2[-1].cni_type;
				p2->lto		= p2[-1].lto;

				break;

			case 0x103: /* RTL2: z.zz (not PDC-A) */
			case 0x104:
				if (pdc && pdc->date_format
				    && non_space > 0)
					goto bad;

				if (!pdc_time_from_bcd (&p1->at1_hour,
							&p1->at1_minute,
							value, pdc))
					goto bad;

				pdc_log ("AT-1 %04x\n", value);

				/* Shortcut: No date, not valid PDC-A */
				if (0 == table[0].year)
					return 0;

				p1->_at1_ptl[0].row = row;
				p1->_at1_ptl[0].column_begin = column_begin;
				p1->_at1_ptl[0].column_end = column;

				if (pdc && pdc->date_format)
					open_title = TRUE;

				if (++p1 >= pend)
					goto finish;

				break;

			case 0x005:
				if (!((combine && ctrl1 == 0x20)
				      || is_magenta_conceal (ctrl1)))
					goto bad;

				if ((value & 0x00F00) > 0x00900)
					goto bad;

				pdc_log ("CNI %05x\n", value);

				if (0 == value)
					return 0;

				if (!have_at2 && p1 > p2) /* see AD */
					p2 = pdc_at2_fill (table, p2, p1,
							   p2, pdc);

				p2->cni = value;
				p2->cni_type = VBI3_CNI_TYPE_PDC_A;

				combine = TRUE;

				break;

			case 0x006:
			{
				unsigned int y, m, d;

				if (!((combine && ctrl1 == 0x20)
				      || is_magenta_conceal (ctrl1)))
					goto bad;

				if (!vbi3_is_bcd (value))
					goto bad;

				y = (value & 15) + ((value >> 4) & 15) * 10;
				value >>= 8;
				m = (value & 15) + ((value >> 4) & 15) * 10;
				value >>= 8;
				d = (value & 15) + (value >> 4) * 10;

				if (d == 0 || d > 31
				    || m == 0 || m > 12)
					goto bad;

				pdc_log ("AD %02d-%02d-%02d\n", y, m, d);

				/* ETS 300 231: "5) CNI, AD, LTO and PTY data
				   are entered in the next row in the table on
				   which no AT-2 has as yet been written;"
				   Literally this would forbid two ADs on a
				   page without AT-2s, but such pages have
				   been observed. */

				if (!have_at2 && p1 > p2)
					p2 = pdc_at2_fill (table, p2, p1,
							   p2, pdc);

				p2->year = y + 2000;
				p2->month = m;
				p2->day = d;

				combine = TRUE;

				break;
			}

			case 0x308:
			{
				unsigned int end_hour;
				unsigned int end_minute;

				if (is_magenta_conceal (ctrl1))
					goto bad;

				if (pdc && pdc->date_format
				    && non_space > 0)
					goto bad;

				if (!pdc_time_from_bcd (&end_hour,
							&end_minute,
							value & 0xFFFF, pdc))
					goto bad;
				if (!pdc_time_from_bcd (&p1->at1_hour,
							&p1->at1_minute,
							value >> 16, pdc))
					goto bad;

				pdc_log ("AT-1 %08x\n", value);

				/* Shortcut: No date, not valid PDC-A */
				if (0 == table[0].year)
					return 0;

				p1->length = pdc_duration
					(p1->at1_hour,
					 p1->at1_minute,
					 end_hour,
					 end_minute);

				p1->_at1_ptl[0].row = row;
				p1->_at1_ptl[0].column_begin = column_begin;
				p1->_at1_ptl[0].column_end = column;

				if (++p1 >= pend)
					goto finish;

				if (pdc && pdc->date_format)
					open_title = TRUE;

				break;
			}

			default:
				goto bad;
			}

			ctrl1 = (column > 0) ?
				vbi3_unpar8 (raw[column - 1]) : 0xFF;
			ctrl2 = (column < 40) ?
				vbi3_unpar8 (raw[column]) : 0xFF;
			continue;

		bad:
			if (open_title
			    && char1 >= 0x40
			    && char1 <= 0x7E) {
			store_open_title:
				store_ptl (table, p1, row,
					   column_begin,
					   /* column_end */ 40);
				open_title = FALSE;
				column = 40;
			}

		not_number:
			ctrl1 = (column > 0) ?
				vbi3_unpar8 (raw[column - 1]) : 0xFF;
			ctrl2 = (column < 40) ?
				vbi3_unpar8 (raw[column]) : 0xFF;

			digits = 0;
		}
	}

 finish:
	if (p2 > p1
	    || p1 == table
	    || 0 == table[0].year
	    || 0 == table[0].cni) {
		return 0; /* invalid */
	}

	pdc_at2_fill (table, p2, p1, p2, pdc);

	for (p2 = table; p2 < p1; ++p2) {
		vbi3_preselection *pp;

		/* AT-2 hour 25 to skip a program. */
		if (25 == p2[0].at2_hour) {
			memmove (p2, p2 + 1, (p1 - p2 - 1) * sizeof (*p2));
			--p1;
			--p2;
			continue;
		}

		if (p2[0].length <= 0) {
			for (pp = p2 + 1; pp < p1; ++pp) {
				if (p2[0].cni == pp[0].cni) {
					p2[0].length = pdc_duration
						(p2[0].at1_hour,
						 p2[0].at1_minute,
						 pp[0].at1_hour,
						 pp[0].at1_minute);
					break;
				}
			}
		}

		if (p2[0].length <= 0) {
			for (pp = p2 + 1; pp < p1; ++pp) {
				if (pp[0].cni == pp[-1].cni) {
					p2[0].length = pdc_duration
						(p2[0].at1_hour,
						 p2[0].at1_minute,
						 pp[0].at1_hour,
						 pp[0].at1_minute);
					break;
				}
			}
		}

		if (p2[0].length <= 0) {
			if (pdc && pdc->date_format
			    && p2 > table
			    && (p2[-1]._at1_ptl[0].column_begin
				== p2[0]._at1_ptl[0].column_begin))
			    continue;
		}

		if (p2[0].length <= 0) {
			memmove (p2, p2 + 1, (p1 - p2 - 1) * sizeof (*p2));
			--p1;
			--p2;
			continue;
		}
	}

	if (0)
		_vbi3_preselection_array_dump (table, p2 - table, stderr);

	return p2 - table;
}

#if 0

/*
   Problem: vbi3_preselection gives network local time, which may
   not be our time zone. p->lto should tell, but is it always
   transmitted? mktime OTOH assumes our timezone, tm_gmtoff is
   a GNU extension.
*/

time_t
vbi3_preselection_time		(const vbi3_preselection *p)
{
	struct tm t;

	assert (NULL != p);

	CLEAR (t);

	t.tm_year	= p->year - 1900;
	t.tm_mon	= p->month - 1;
	t.tm_mday	= p->day;
	t.tm_hour	= p->at1_hour;
	t.tm_min	= p->at1_minute;

	t.tm_isdst	= -1; /* unknown */

	t.tm_gmtoff	= pl->lto * 60; /* XXX sign? */

	return mktime (&t);
}

#else

/**
 * @param pid vbi3_preselection structure.
 *
 * Returns the date and time (at1) in the vbi3_preselection
 * structure as time_t value.
 *
 * @returns
 * time_t value, (time_t) -1 if the date and time cannot
 * be represented as time_t value.
 *
 * @bugs
 * Not implemented yet.
 */
time_t
vbi3_preselection_time		(const vbi3_preselection *p)
{
	assert (NULL != p);

	return (time_t) -1;
}

#endif
