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

/* $Id: pdc.c,v 1.1.2.10 2004-09-14 04:52:00 mschimek Exp $ */

#include "../site_def.h"

#include <stdlib.h>		/* malloc() */
#include <assert.h>
#include "misc.h"
#include "hamm.h"		/* vbi_ipar8() */
#include "bcd.h"		/* vbi_is_bcd() */
#include "pdc.h"

/**
 * @addtogroup ProgramID PDC/XDS Program ID Decoder
 * @ingroup LowDec
 */

/**
 * @internal
 * @param pil vbi_pil to print.
 * @param fp Destination stream.
 *
 * Prints a vbi_pil as service code or date and time string without
 * trailing newline. This is intended for debugging.
 */
void
_vbi_pil_dump			(vbi_pil		pil,
				 FILE *			fp)
{
	switch (pil) {
	case VBI_PIL_TIMER_CONTROL:
		fputs ("Timer-control (no PDC)", fp);
		break;

	case VBI_PIL_INHIBIT_TERMINATE:
		fputs ("Recording inhibit/terminate", fp);
		break;

	case VBI_PIL_INTERRUPT:
		fputs ("Interruption", fp);
		break;

	case VBI_PIL_CONTINUE:
		fputs ("Continue", fp);
		break;

	default:
		fprintf (fp, "%05x (%02u-%02u %02u:%02u)",
			 pil,
			 VBI_PIL_MONTH (pil),
			 VBI_PIL_DAY (pil),
			 VBI_PIL_HOUR (pil),
			 VBI_PIL_MINUTE (pil));
		break;
	}
}

/**
 * @internal
 * @param pid vbi_program_id structure to print.
 * @param fp Destination stream.
 *
 * Prints the contents of a vbi_program_id structure as a string
 * without trailing newline. This is intended for debugging.
 */
void
_vbi_program_id_dump		(const vbi_program_id *	pid,
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
		 vbi_cni_type_name (pid->cni_type),
		 pid->channel);

	if (0) {
		fprintf (fp, "%02u-%02u %02u:%02u pil=",
			 pid->month + 1, pid->day + 1,
			 pid->hour, pid->minute);
	} else {
		fprintf (fp, "pil=");
	}

	_vbi_pil_dump (pid->pil, fp);

	fprintf (fp, " length=%u "
		 "luf=%u mi=%u prf=%u "
		 "pcs=%s pty=%02x td=%u",
		 pid->length,
		 pid->luf, pid->mi, pid->prf,
		 pcs_audio[pid->pcs_audio],
		 pid->pty, pid->tape_delayed);
}

/**
 * @param pid vbi_program_id structure initialized with vbi_program_id_init().
 *
 * Frees all resources associated with @a pid, except the structure itself.
 */
void
vbi_program_id_destroy		(vbi_program_id *	pid)
{
	assert (NULL != pid);

	CLEAR (*pid);
}

/**
 * @param pid vbi_program_id structure to initialize.
 * @param channel Logical channel number to be stored in the
 *   channel field.
 *
 * Initializes a vbi_program_id structure.
 */
void
vbi_program_id_init		(vbi_program_id *	pid,
				 vbi_pid_channel	channel)
{
	assert (NULL != pid);

	pid->cni_type		= VBI_CNI_TYPE_UNKNOWN;
	pid->cni		= 0;

	pid->channel		= channel;

	pid->month		= 0;
	pid->day		= 0;
	pid->hour		= 0;
	pid->minute		= 0;

	pid->pil		= VBI_PIL_TIMER_CONTROL;

	pid->length		= 0;

	pid->luf		= FALSE;
	pid->mi			= FALSE;
	pid->prf		= FALSE;

	pid->pcs_audio		= VBI_PCS_AUDIO_UNKNOWN;

	pid->pty		= 0; /* none / unknown */

	pid->tape_delayed	= FALSE;
}

/**
 * @internal
 * @param pid vbi_preselection structure to print.
 * @param fp Destination stream.
 *
 * Prints the contents of a vbi_preselection structure as a string
 * without trailing newline. This is intended for debugging.
 */
void
_vbi_preselection_dump		(const vbi_preselection *p,
				 FILE *			fp)
{
	unsigned int i;

	fprintf (fp,
		 "cni=%04x (%s) "
		 "%04d-%02d-%02d %02d:%02d (%02d:%02d) %3d min "
		 "pty=%02x lto=%d caf=%u at1/ptl=",
		 p->cni,
		 vbi_cni_type_name (p->cni_type),
		 p->year, p->month, p->day,
		 p->at1_hour, p->at1_minute,
		 p->at2_hour, p->at2_minute,
		 p->length,
		 p->pty, p->lto, p->caf);

	for (i = 0; i < 3; ++i)
		fprintf (fp, "%d:%d-%d%c",
			 p->_at1_ptl[i].row,
			 p->_at1_ptl[i].column_begin,
			 p->_at1_ptl[i].column_end,
			 (i == 2) ? ' ' : ',');
}

/**
 * @internal
 * @param pid Array of vbi_preselection structures to print.
 * @param size Number of structures in the array.
 * @param fp Destination stream.
 *
 * Prints the contents of a vbi_preselection structure array,
 * one structure per line. This is intended for debugging.
 */
void
_vbi_preselection_array_dump	(const vbi_preselection *p,
				 unsigned int		size,
				 FILE *			fp)
{
	unsigned int count;

	count = 0;

	while (size-- > 0) {
		fprintf (fp, "%2u: ", count++);
		_vbi_preselection_dump (p++, fp);
		fputc ('\n', fp);
	}
}

/**
 */
void
vbi_preselection_destroy	(vbi_preselection *	p)
{
	assert (NULL != p);

	free (p->title);

	CLEAR (*p);
}

/**
 */
vbi_bool
vbi_preselection_copy		(vbi_preselection *	dst,
				 const vbi_preselection *src)
{
	if (dst == src)
		return TRUE;

	assert (NULL != dst);

	if (src) {
		char *title;

		if (!(title = strdup (src->title)))
			return FALSE;

		*dst = *src;
		dst->title = title;
	} else {
		CLEAR (*dst);
	}

	return TRUE;
}

/**
 */
vbi_bool
vbi_preselection_init		(vbi_preselection *	p)
{
	assert (NULL != p);

	CLEAR (*p);

	return TRUE;
}

/**
 * @internal
 */
void
_vbi_preselection_array_delete	(vbi_preselection *	p,
				 unsigned int		size)
{
	unsigned int i;

	if (NULL == p || 0 == size)
		return;

	for (i = 0; i < size; ++i)
		vbi_preselection_destroy (p + i);

	free (p);
}

/**
 * @internal
 */
vbi_preselection *
_vbi_preselection_array_dup	(const vbi_preselection *p,
				 unsigned int		size)
{
	vbi_preselection *new_p;
	unsigned int i;

	if (!(new_p = malloc (size * sizeof (*new_p))))
		return NULL;

	memcpy (new_p, p, size * sizeof (*new_p));

	for (i = 0; i < size; ++i) {
		if (p[i].title) {
			if (!(new_p[i].title = strdup (p[i].title))) {
				while (i-- > 0)
					free (new_p[i].title);

				free (new_p);

				return NULL;
			}
		}
	}

	return new_p;
}

/**
 * @internal
 */
vbi_preselection *
_vbi_preselection_array_new	(unsigned int		size)
{
	return calloc (1, size * sizeof (vbi_preselection));
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
   in place of the period separating hour and minute digits.

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

#define M_COLOR ((1 << 1) | (1 << 2) | (1 << 3) | \
		 (1 << 4) | (1 << 6) | (1 << 7))
#define M_MAGENTA (1 << 0x05)
#define M_CONCEAL (1 << 0x18)

#define isnum(c) ((c) >= 0x30 && (c) <= 0x39)
/* NB uppercase only, unlike ctype.h isxdigit() */
#define isxnum(c) (isnum (c) || ((c) >= 0x41 && (c) <= 0x46))
#define iscolon(c) ((c) == 0x3A)
#define isperiod(c) ((c) == 0x2E)
#define isminus(c) ((c) == 0x2D)

/* Caution! These macros assume c is a C0 (0x00 ... 0x1F) */
#define iscontrol(c) ((M_COLOR | M_MAGENTA | M_CONCEAL) & (1 << (c)))
#define ismagenta(c) ((c) == 0x05)
#define isconceal(c) ((c) == 0x18)
#define iscolour_conceal(c) ((M_COLOR | M_CONCEAL) & (1 << (c)))
#define ismagenta_conceal(c) ((M_MAGENTA | M_CONCEAL) & (1 << (c)))

#ifndef PDC_LOG
#define PDC_LOG 0
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
static vbi_bool
pdc_time_from_bcd		(unsigned int *		hour,
				 unsigned int *		minute,
				 int			bcd)
{
	int h, m;

	if (vbi_bcd_digits_greater (bcd, 0x2959))
		return FALSE;

	m = (bcd & 15) + ((bcd >> 4) & 15) * 10;

	bcd >>= 8;

	h = (bcd & 15) +  (bcd >> 4)       * 10;

	if (h >= 24)
		return FALSE;

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

/**
 * @internal
 * @param table Table we write.
 * @param p Current vbi_preselection entry in table, store PTL info here.
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
pdc_ptl				(vbi_preselection *	table,
				 vbi_preselection *	p,
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

	ctrl2 = vbi_ipar8 (raw[column]);

	for (;;) {
		if (column >= 39) {
			/* Line end postfix */
			column = 40;
			break;
		}

		ctrl1 = ctrl2;
		ctrl2 = vbi_ipar8 (raw[column + 1]);

		if (0)
			pdc_log ("%d %d %02x-%02x\n",
				 row, column, ctrl1, ctrl2);

		if (ctrl2 < 0)
			return 0; /* hamming error */

		if (ctrl1 < 0x20 && iscontrol (ctrl1)) {
			if (ctrl1 == ctrl2) {
				column_end = column;
				column += 2;
				break;
			} else if (ismagenta (ctrl1)
				   && isconceal (ctrl2)) {
				/* Observation 6) */
				column_end = column;
				break;
			}
		} else if (isnum (ctrl1)
			   && isnum (ctrl2)
			   && column <= 35
			   && column_at1 == 40) {
			int separator = vbi_ipar8 (raw[column + 2]);

			if (iscolon (separator)
			    || isperiod (separator)) {
				int digit3 = vbi_ipar8 (raw[column + 3]);
				int digit4 = vbi_ipar8 (raw[column + 4]);
				unsigned int delimiter;

				delimiter = (column < 35) ?
					vbi_ipar8 (raw[column + 5]) : 0x20;

				if (isnum (digit3)
				    && isnum (digit4)
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

	/* Record position of up to three PTLs. Entry #0
	   stores AT-1 position in case there is no PTL. */

	if (p > table) {
		unsigned int i;

		for (i = 1; i < 4; ++i) {
			if (p[-1]._at1_ptl[i].row <= 0) {
				if (i >= 2)
					if (p[-1]._at1_ptl[i - 1].row
					    < (row - 1))
						break; /* probably unrelated */

				p[-1]._at1_ptl[i].row		= row;
				p[-1]._at1_ptl[i].column_begin	= column_begin;
				p[-1]._at1_ptl[i].column_end	= column_end;

				break;
			}
		}
	}

	/* No explicit postfix, must scan for unmarked
	   elements within PTL. */
	if (column == 40)
		column = column_at1;

	return column;
}

/**
 * @internal
 * @param value Output BCD value of number, 2, 4, 6 or 8 digits.
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
		int c = vbi_ipar8 (raw[(*column)++]);

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
			while (*column <= 39) {
				c = vbi_ipar8 (raw[*column]);

				if (c != 0x20)
					break;

				++*column;
			}

			if (*digits == 0x104) {
				if (*column <= 34 && isminus (c))
					continue; /* zz.zz<spaces>- */
				else
					return TRUE;
			} else if (*digits == 0x204) {
				if (*column <= 35 && isnum (c))
					continue; /* zz.zz-<spaces>z */
				else
					return TRUE;
			}

			return TRUE;

		case 0x2E: /* period */
		case 0x3A: /* colon, observation 2) */
			if (*digits == 0x002 || *digits == 0x206) {
				*digits += 0x100; /* [zz.zz-]zz. */
			} else {
				return FALSE;
			}
			break;

		case 0x2D: /* minus */
			if (*digits == 0x104) {
				*digits = 0x204; /* zz.zz- */
			} else {
				return FALSE;
			}
			
			break;

		default: /* junk (incl parity error), observation 2) */
			if (*digits == 0x104 || *digits == 0x308) {
				/* Junk after [zz.zz-]zz.zz */
				--(*column);
				return TRUE;
			} else {
				return FALSE;
			}
		}
	}

	return TRUE;
}

/** @internal */
static vbi_preselection *
pdc_at2_fill			(vbi_preselection *		begin,
				 vbi_preselection *		end,
				 vbi_preselection *		src)
{
	vbi_preselection *p;

	for (p = begin; p < end; ++p) {
		p->cni_type	= VBI_CNI_TYPE_PDC_A;
		p->cni		= src->cni;
		p->year		= src->year;
		p->month	= src->month;
		p->day		= src->day;
		p->lto		= src->lto;

		p->at2_hour	= p->at1_hour;
		p->at2_minute	= p->at2_minute;
	}

	return p;
}

/**
 * @internal
 * @param table Store PDC data here.
 * @param table_size Free space in the table, number of elements.
 * @param lop_raw Raw Teletext level one page.
 *
 * Scans a raw Teletext level one page for PDC data and stores the
 * data in a vbi_preselection table.
 *
 * @returns
 * Number of entries written in table.
 */
unsigned int
_vbi_pdc_method_a		(vbi_preselection *	table,
				 unsigned int		table_size,
				 const uint8_t		lop_raw[26][40])
{
	vbi_preselection *p1;
	vbi_preselection *p2;
	vbi_preselection *pend;
	unsigned int row;
	vbi_bool have_at2;
	int pw_sum;
	int pw;

	memset (table, 0, sizeof (*table) * table_size);

	p1 = table; /* next AT-1 row */
	p2 = table; /* next AT-2 row */

	pend = table + table_size;

	p2->year = 0; /* not found */
	p2->cni_type = VBI_CNI_TYPE_PDC_A;
	p2->cni = 0; /* not found */

	have_at2 = FALSE;

	pw_sum = 0;
	pw = -1; /* XXX checksum to do */

	for (row = 1; row <= 23; ++row) {
		const uint8_t *raw;
		unsigned int column;
		unsigned int column_begin;
		unsigned int value;
		unsigned int digits;
		vbi_bool combine;
		int ctrl0; /* previous */
		int ctrl1; /* current */
		int ctrl2; /* next */

		raw = lop_raw[row];

		ctrl1 = 0xFF; /* nothing at raw[-1] */
		ctrl2 = vbi_ipar8 (raw[0]);

		combine = FALSE;

		for (column = 0; column <= 38;) {
			ctrl0 = ctrl1;
			ctrl1 = ctrl2;
			ctrl2 = vbi_ipar8 (raw[column + 1]);

			if (0)
				pdc_log ("%d,%d %02x-%02x-%02x %d\n",
					 row, column, ctrl0, ctrl1,
					 ctrl2, combine);

			if ((ctrl0 | ctrl1 | ctrl2) < 0) {
				return 0; /* hamming error */
			} else if (ctrl1 < 0x20 && iscontrol (ctrl1)) {
				if (ctrl0 == ctrl1
				    && iscolour_conceal (ctrl1)
				    && ctrl2 >= 0x20) {
					column = pdc_ptl (table,
							  p1, raw,
							  row, column + 1);
					if (column == 0)
						return 0; /* hamming error */
					goto bad; /* i.e. not a number */
				} else if (isxnum (ctrl2)) {
					++column; /* Numeric item */
				} else {
					++column;
					combine = FALSE;
					continue;
				}
			} else if (isxnum (ctrl1) && ctrl0 >= 0x20) {
				/* Possible unmarked AT-1 2),
				   combined CNI, AD, PW 3) */
				ctrl1 = ctrl0;
			} else {
				++column;
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
				      || ismagenta_conceal (ctrl1)))
					goto bad;

				pdc_log ("PW %02x\n", value);

				pw = value;

				combine = TRUE;

				break;

			case 0x003:
				if (!isconceal (ctrl1))
					goto bad;

				switch (value >> 8) {
				case 0x0:
				case 0x9:
				{
					unsigned int d;

					if (!vbi_is_bcd (value))
						goto bad;

					pdc_log ("LTO %03x %d\n",
						 value, p2->lto);

					d = (value & 15)
						+ ((value >> 4) & 15) * 10;

					if (d >= 12 * 4)
						goto bad; /* implausible */

					if (!have_at2 && p1 > p2) /* see AD */
						p2 = pdc_at2_fill (p2, p1, p2);

					/* 0x999 = -0x001 (ok?) */
					p2->lto = (value >= 0x900) ?
						d - 100: d;
					p2->lto *= 15; /* to minutes */

					break;
				}

				case 0xF:
					if (!ismagenta (ctrl0))
						goto bad;

					pdc_log ("PTY %02x\n", value);

					p2->pty = value & 0xFF;

					break;

				default:
					goto bad;
				}

				break;

			case 0x004:
				if (!ismagenta_conceal (ctrl1))
					goto bad;

				if (value == 0x2500) {
					/* Skip flag */
					p2->at2_hour = 25;
				} else {
					if (!pdc_time_from_bcd
					    (&p2->at2_hour,
					     &p2->at2_minute,
					     value))
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
				p2->cni_type	= VBI_CNI_TYPE_PDC_A;
				p2->lto		= p2[-1].lto;

				break;

			case 0x104:
				if (!pdc_time_from_bcd (&p1->at1_hour,
							&p1->at1_minute,
							value))
					goto bad;

				pdc_log ("AT-1 %04x\n", value);

				/* Shortcut: No date, not valid PDC-A */
				if (0 == table[0].year)
					return 0;

				p1->_at1_ptl[0].row = row;
				p1->_at1_ptl[0].column_begin = column_begin;
				p1->_at1_ptl[0].column_end = column;

				if (++p1 >= pend)
					goto finish;

				break;

			case 0x005:
				if (!((combine && ctrl1 == 0x20)
				      || ismagenta_conceal (ctrl1)))
					goto bad;

				if ((value & 0x00F00) > 0x00900)
					goto bad;

				pdc_log ("CNI %05x\n", value);

				if (0 == value)
					return 0;

				if (!have_at2 && p1 > p2) /* see AD */
					p2 = pdc_at2_fill (p2, p1, p2);

				p2->cni = value;
				p2->cni_type = VBI_CNI_TYPE_PDC_A;

				combine = TRUE;

				break;

			case 0x006:
			{
				unsigned int y, m, d;

				if (!((combine && ctrl1 == 0x20)
				      || ismagenta_conceal (ctrl1)))
					goto bad;

				if (!vbi_is_bcd (value))
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
					p2 = pdc_at2_fill (p2, p1, p2);

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

				if (ismagenta_conceal (ctrl1))
					goto bad;

				if (!pdc_time_from_bcd (&end_hour,
							&end_minute,
							value & 0xFFFF))
					goto bad;
				if (!pdc_time_from_bcd (&p1->at1_hour,
							&p1->at1_minute,
							value >> 16))
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

				break;
			}

			default:
				goto bad;
			}

			ctrl1 = (column > 0) ?
				vbi_ipar8 (raw[column - 1]) : 0xFF;
			ctrl2 = (column < 40) ?
				vbi_ipar8 (raw[column]) : 0xFF;
			continue;

		bad:
			ctrl1 = (column > 0) ?
				vbi_ipar8 (raw[column - 1]) : 0xFF;
			ctrl2 = (column < 40) ?
				vbi_ipar8 (raw[column]) : 0xFF;

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

	pdc_at2_fill (p2, p1, p2);

	for (p2 = table; p2 < p1; ++p2) {
		vbi_preselection *pp;

		if (p2[0].length <= 0)
			for (pp = p2 + 1; pp < p1; ++pp)
				if (p2[0].cni == pp[0].cni) {
					p2[0].length = pdc_duration
						(p2[0].at1_hour,
						 p2[0].at1_minute,
						 pp[0].at1_hour,
						 pp[0].at1_minute);
					break;
				}
		if (p2[0].length <= 0)
			for (pp = p2 + 1; pp < p1; ++pp)
				if (pp[0].cni == pp[-1].cni) {
					p2[0].length = pdc_duration
						(p2[0].at1_hour,
						 p2[0].at1_minute,
						 pp[0].at1_hour,
						 pp[0].at1_minute);
					break;
				}

		/* AT-2 hour 25 to skip a program. */
		if (p2[0].length <= 0 || 25 == p2[0].at2_hour) {
			memmove (p2, p2 + 1, (p1 - p2 - 1) * sizeof (*p2));
			--p1;
			--p2;
			continue;
		}
	}

	if (0)
		_vbi_preselection_array_dump (table, p2 - table, stderr);

	return p2 - table;
}

#if 0

/*
   Problem: vbi_preselection gives network local time, which may
   not be our time zone. p->lto should tell, but is it always
   transmitted? mktime OTOH assumes our timezone, tm_gmtoff is
   a GNU extension.
*/

time_t
vbi_preselection_time		(const vbi_preselection *p)
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
 * @param pid vbi_preselection structure.
 *
 * Returns the date and time (at1) in the vbi_preselection
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
vbi_preselection_time		(const vbi_preselection *p)
{
	assert (NULL != p);

	return (time_t) -1;
}

#endif
