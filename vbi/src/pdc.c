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

/* $Id: pdc.c,v 1.1.2.1 2004-02-13 02:15:27 mschimek Exp $ */

#include "../site_def.h"

#include "hamm.h"
#include "vbi.h"

#define PGP_CHECK(ret_value)						\
do {									\
	assert (NULL != pg);						\
									\
	pgp = CONST_PARENT (pg, vbi_page_private, pg);			\
									\
	if (VBI_PAGE_PRIVATE_MAGIC != pgp->magic)			\
		return ret_value;					\
} while (0)

/**
 * @internal
 */
void
vbi_pil_dump			(vbi_pil		pil,
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

	case VBI_PIL_NO_TIME:
		fputs ("No time", fp);
		break;

	default:
		fprintf (fp, "%05x (%02u-%02u %02u:%02u)",
			 pil,
			 VBI_PIL_MONTH (pil), VBI_PIL_DAY (pil),
			 VBI_PIL_HOUR (pil), VBI_PIL_MINUTE (pil));
	}
}

/**
 * @internal
 */
void
pdc_program_dump		(const pdc_program *	p,
				 FILE *			fp)
{
	unsigned int i;

	fprintf (fp, "%02d-%02d-%02d %02d:%02d (%02d:%02d) %3d min "
		 "cni=%08x pty=%02x lto=%d caf=%u at1/ptl=",
		 p->ad.year, p->ad.month + 1, p->ad.day + 1,
		 p->at1.hour, p->at1.min,
		 p->at2.hour, p->at2.min,
		 p->length,
		 p->cni, p->pty, p->lto, p->caf);

	for (i = 0; i < 3; ++i)
		fprintf (fp, "%d:%d-%d%c",
			 p->at1_ptl_pos[i].row,
			 p->at1_ptl_pos[i].column_begin,
			 p->at1_ptl_pos[i].column_end,
			 (i == 2) ? ' ' : ',');
}

/**
 * @internal
 */
void
pdc_program_array_dump		(const pdc_program *	p,
				 unsigned int		size,
				 FILE *			fp)
{
	unsigned int count = 0;

	while (size-- > 0) {
		fprintf (fp, "%2u: ", count++);
		pdc_program_dump (p++, fp);
		fputc ('\n', fp);
	}
}

/**
 * @internal
 */
void
vbi_preselection_dump		(const vbi_preselection *pl,
				 FILE *			fp)
{
	fprintf (fp, "%llx %04u-%02u-%02u %02u:%02u %3u min "
		 "pdc=%02u%02u\n",
		 pl->nuid,
		 pl->year, pl->month + 1, pl->day + 1,
		 pl->at1_hour, pl->at1_minute,
		 pl->length,
		 pl->at2_hour, pl->at2_minute);
}

/*
 *  PDC Preselection method "A"
 *  ETS 300 231, Section 7.3.1.
 *
 *  (Method "B" code has been merged into the Teletext
 *   enhancement routines in teletext.c)
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
 * @param t Output
 * @param bcd Four digit time value, e.g. 0x2359.
 *
 * Converts BCD coded time to pdc_time.
 *
 * @returns
 * FALSE if @a bcd is invalid.
 */
static vbi_bool
pdc_time_from_bcd		(pdc_time *		t,
				 int			bcd)
{
	int h, m;

	if (vbi_bcd_vec_greater (bcd, 0x2959))
		return FALSE;

	m = (bcd & 15) + ((bcd >> 4) & 15) * 10;

	bcd >>= 8;

	h = (bcd & 15) +  (bcd >> 4)       * 10;

	if (h >= 24)
		return FALSE;

	t->hour = h;
	t->min = m;

	return TRUE;
}

/**
 * @internal
 * @param table Table we write.
 * @param p Current pdc_program entry in table, store PTL info here.
 * @param raw Raw level one page, current row.
 * @param row Current position.
 * @param column Current position >= 1.
 *
 * Evaluates a PTL, subfunction of pdc_method_a().
 *
 * @returns
 * Next column to process >= @a column, or 0 on error.
 */
static __inline__ unsigned int
pdc_ptl				(pdc_program *		table,
				 pdc_program *		p,
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
	  	pdc_position *tab;
		pdc_position *pp;

		tab = p[-1].at1_ptl_pos;

		for (pp = &tab[1]; pp <= &tab[3]; ++pp) {
			if (pp->row < 0) {
				if (pp >= &tab[2]
				    && ((unsigned int) pp[-1].row
					< (row - 1)))
					break; /* probably unrelated */

				pp->row		 = row;
				pp->column_begin = column_begin;
				pp->column_end   = column_end;
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
static __inline__ unsigned int
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

/**
 * @internal
 * @param table Table we write.
 * @param p2 Source and destination.
 * @param pend End of destination.
 *
 * Copies AD, CNI, LTO in row p2 to rows p2 ... pend - 1.
 *
 * @returns
 * MAX (p2, pend). 
 */
static pdc_program *
pdc_at2_fill			(pdc_program *		table,
				 pdc_program *		p2,
				 pdc_program *		pend)
{
	while (p2 < pend) {
		if (p2 > table) {
			p2->ad	= p2[-1].ad;
			p2->cni	= p2[-1].cni;
			p2->lto	= p2[-1].lto;
		}

		p2->at2	= p2->at1;

		++p2;
	}

	return p2;
}

/**
 * @internal
 * @param pg Formatted page.
 * @param pbegin Take AT-1 and PTL positions from this table.
 * @param pend End of the table.
 *
 * Adds PDC flags to the text.
 */
void
vbi_page_mark_pdc		(vbi_page *		pg,
				 const pdc_program *	pbegin,
				 const pdc_program *	pend)
{
	while (pbegin < pend) {
		unsigned int i;

		for (i = 0; i < 4; ++i) {
			const pdc_position *pp;

			pp = pbegin->at1_ptl_pos + i;

			if (pp->row > 0) {
				vbi_char *acp;
				unsigned int j;

				acp = pg->text + pg->columns * pp->row;

				/* We could mark exactly the AT-1 and PTL
				   characters, but from a usability
				   standpoint it's better to make the
				   sensitive area larger. */
				for (j = 0; j < pg->columns; ++j) {
					acp[j].pdc = TRUE;

					switch (acp[j].size) {
					case VBI_DOUBLE_HEIGHT:
					case VBI_DOUBLE_SIZE:
					case VBI_OVER_TOP:
						acp[pg->columns].pdc = TRUE;
						break;

					default:
						break;
					}
				}
			}
		}

		++pbegin;
	}
}

/**
 * @internal
 * @param table Store PDC data here.
 * @param table_size Size of the table.
 * @param lop_raw Raw level one page.
 *
 * Scans a raw level one page for PDC data and stores the
 * data in a pdc_program table.
 *
 * @returns
 * Number of entries written in table.
 */
unsigned int
pdc_method_a			(pdc_program *		table,
				 unsigned int		table_size,
				 const uint8_t		lop_raw[26][40])
{
	pdc_program *p1;
	pdc_program *p2;
	pdc_program *pend;
	unsigned int row;
	vbi_bool have_at2;
	int pw_sum;
	int pw;

	memset (table, 0, sizeof (*table) * table_size);

	p1 = table; /* next AT-1 row */
	p2 = table; /* next AT-2 row */

	pend = table + table_size;

	p2->ad.year = -1; /* not found */
	p2->cni = -1;

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
				unsigned int d, m, y;
				struct pdc_time at;

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
					if (!vbi_is_bcd (value))
						goto bad;

					pdc_log ("LTO %03x %d\n", value, p2->lto);

					d = (value & 15) + ((value >> 4) & 15) * 10;

					if (d >= 12 * 4)
						goto bad; /* implausible */

					if (!have_at2 && p1 > p2) /* see AD */
						p2 = pdc_at2_fill (p2, p2, p1);

					/* 0x999 = -0x001 (ok?) */
					p2->lto = (value >= 0x900) ? d - 100: d;

					break;

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
					p2->at2.hour = 25;
				} else {
					if (!pdc_time_from_bcd (&p2->at2, value))
						goto bad;
				}

				pdc_log ("AT-2 %04x\n", value);

				have_at2 = TRUE;

				if (++p2 >= pend)
					goto finish;

				p2->ad  = p2[-1].ad;
				p2->cni = p2[-1].cni;
				p2->lto = p2[-1].lto;

				break;

			case 0x104:
				if (!pdc_time_from_bcd (&p1->at1, value))
					goto bad;

				pdc_log ("AT-1 %04x\n", value);

				/* Shortcut: No date, not valid PDC-A */
				if (table[0].ad.year < 0)
					return 0;

				p1->at1_ptl_pos[0].row = row;
				p1->at1_ptl_pos[0].column_begin = column_begin;
				p1->at1_ptl_pos[0].column_end = column;

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

				if (!have_at2 && p1 > p2) /* see AD */
					p2 = pdc_at2_fill (p2, p2, p1);

				p2->cni = value;

				combine = TRUE;

				break;

			case 0x006:
				if (!((combine && ctrl1 == 0x20)
				      || ismagenta_conceal (ctrl1)))
					goto bad;

				if (!vbi_is_bcd (value))
					goto bad;

				y = (value & 15) + ((value >> 4) & 15) * 10; value >>= 8;
				m = (value & 15) + ((value >> 4) & 15) * 10; value >>= 8;
				d = (value & 15) + (value >> 4) * 10;

				if (d == 0 || d > 31
				    || m == 0 || m > 12)
					goto bad;

				pdc_log ("AD %02d-%02d-%02d\n", y, m, d);

				/* ETS 300 231: "5) CNI, AD, LTO and PTY data are 
				   entered in the next row in the table on which
				   no AT-2 has as yet been written;" Literally
				   this would forbid two ADs on a page without
				   AT-2s, but such pages have been observed. */

				if (!have_at2 && p1 > p2)
					p2 = pdc_at2_fill (p2, p2, p1);

				p2->ad.year = y;
				p2->ad.month = m - 1;
				p2->ad.day = d - 1;

				combine = TRUE;

				break;

			case 0x308:
				if (ismagenta_conceal (ctrl1))
					goto bad;

				if (!pdc_time_from_bcd (&at, value & 0xFFFF))
					goto bad;
				if (!pdc_time_from_bcd (&p1->at1, value >> 16))
					goto bad;

				pdc_log ("AT-1 %08x\n", value);

				/* Shortcut: No date, not valid PDC-A */
				if (table[0].ad.year < 0)
					return 0;

				p1->length = pdc_time_diff (&p1->at1, &at);

				p1->at1_ptl_pos[0].row = row;
				p1->at1_ptl_pos[0].column_begin = column_begin;
				p1->at1_ptl_pos[0].column_end = column;

				if (++p1 >= pend)
					goto finish;

				break;

			default:
				goto bad;
			}

			ctrl1 = (column > 0) ? vbi_ipar8 (raw[column - 1]) : 0xFF;
			ctrl2 = (column < 40) ? vbi_ipar8 (raw[column]) : 0xFF;
			continue;
		bad:
			ctrl1 = (column > 0) ? vbi_ipar8 (raw[column - 1]) : 0xFF;
			ctrl2 = (column < 40) ? vbi_ipar8 (raw[column]) : 0xFF;
			digits = 0;
		}
	}

 finish:
	if (p2 > p1
	    || p1 == table
	    || table[0].ad.year < 0
	    || table[0].cni < 0) {
		return 0; /* invalid */
	}

	pdc_at2_fill (have_at2 ? table : p2, p2, p1);

	for (p2 = table; p2 < p1; ++p2) {
		pdc_program *pp;

		if (p2[0].length <= 0)
			for (pp = p2 + 1; pp < p1; ++pp)
				if (p2[0].cni == pp[0].cni) {
					p2[0].length = pdc_time_diff
						(&p2[0].at1, &pp[0].at1);
					break;
				}
		if (p2[0].length <= 0)
			for (pp = p2 + 1; pp < p1; ++pp)
				if (pp[0].cni == pp[-1].cni) {
					p2[0].length = pdc_time_diff
						(&p2[0].at1, &pp[0].at1);
					break;
				}
		if (p2[0].length <= 0 || p2[0].at2.hour == 25) {
			memmove (p2, p2 + 1, (p1 - p2 - 1) * sizeof (*p2));
			--p1;
			--p2;
			continue;
		}
	}

	if (0)
		pdc_program_array_dump (table, p2 - table, stderr);

	return p2 - table;
}

/**
 * @param pl Blah
 * 
 * Converts the date and time in @a pl to a time_t value.
 *
 * Note this function initializes the global tzname variable
 * from the TZ environment variable as a side effect of
 * calling mktime().
 *
 * @returns
 * time_t value or (time_t) -1 if the date or time cannot
 * be represented.
 */
time_t
vbi_preselection_time		(const vbi_preselection *pl)
{
	struct tm t;

	CLEAR (t);

	t.tm_year	= pl->year - 1900;
	t.tm_mon	= pl->month;
	t.tm_mday	= pl->day + 1;
	t.tm_hour	= pl->at1_hour;
	t.tm_min	= pl->at1_minute;

	t.tm_isdst	= -1; /* unknown */

// ?	t.tm_gmtoff	= pl->lto * 60; /* sign? */

	return mktime (&t);
}

static vbi_bool
vbi_preselection_from_pdc_program
				(vbi_preselection *	pl,
				 const vbi_page_private *pgp,
				 const pdc_program *	p)
{
	if (p->at1.hour >= 0) {
		pl->nuid = vbi_nuid_from_cni (VBI_CNI_TYPE_PDC_A, p->cni);

		pl->at1_hour = p->at1.hour;
		pl->at1_minute = p->at1.min;
	} else {
		pl->nuid = vbi_nuid_from_cni (VBI_CNI_TYPE_PDC_B, p->cni);

		pl->at1_hour = p->at2.hour;
		pl->at1_minute = p->at2.min;
	}

	if (p->ad.year >= 0) {
		if (p->ad.year > 80)
			pl->year = 1900 + p->ad.year;
		else
			pl->year = 2000 + p->ad.year;
	} else {
		time_t now;
		struct tm t;
		int d;

		time (&now);

		if ((time_t) -1 == now)
			return FALSE;

		localtime_r (&now, &t);

		pl->year = t.tm_year + 1900;

		d = t.tm_mon - p->ad.month;

		if (d > 0 && d <= 6)
			++pl->year;
	}

	pl->month = p->ad.month;
	pl->day = p->ad.day;

	pl->at2_hour = p->at2.hour;
	pl->at2_minute = p->at2.min;

	pl->length = p->length;

#warning more

	return TRUE;
}

/**
 * @param pg With vbi_fetch_vt_page() obtained vbi_page.
 * @param pl Place to store information about the link.
 * @param column Column 0 ... pg->columns - 1 of the character in question.
 * @param row Row 0 ... pg->rows - 1 of the character in question.
 * 
 * Describe me.
 *
 * @returns
 * TRUE if the link is valid.
 */
vbi_bool
vbi_page_pdc_link		(const vbi_page *	pg,
				 vbi_preselection *	pl,
				 unsigned int		column,
				 unsigned int		row)
{
	const vbi_page_private *pgp;
	const pdc_program *p;
	const pdc_program *pend;
	const pdc_program *pmatch;

	PGP_CHECK (FALSE);

	assert (NULL != pl);

	if (0 == row
	    || row >= pgp->pg.rows
	    || column >= pgp->pg.columns)
		return FALSE;

	pend = pgp->pdc_table + pgp->pdc_table_size;
	pmatch = NULL;

	for (p = pgp->pdc_table; p < pend; ++p) {
		unsigned int i;

		for (i = 0; i < N_ELEMENTS (p->at1_ptl_pos); ++i) {
			const pdc_position *pp = p->at1_ptl_pos + i;

			if (row != (unsigned int) pp->row)
				continue;

			pmatch = p;

			if (column >= (unsigned int) pp->column_begin
			    && column < (unsigned int) pp->column_end)
				goto finish;
		}
	}

	if (!pmatch)
		return FALSE;

 finish:
	return vbi_preselection_from_pdc_program (pl, pgp, pmatch);
}

/**
 * @param pg With vbi_fetch_vt_page() obtained vbi_page.
 * @param pl Place to store information about the link.
 * @param index Number 0 ... n of item.
 * 
 * Describe me.
 *
 * @returns
 * FALSE if @a num is out of bounds.
 */
vbi_bool
vbi_page_pdc_enum		(const vbi_page *	pg,
				 vbi_preselection *	pl,
				 unsigned int		index)
{
	const vbi_page_private *pgp;

	PGP_CHECK (FALSE);
	assert (NULL != pl);

	if (index >= pgp->pdc_table_size)
		return FALSE;

	return vbi_preselection_from_pdc_program
		(pl, pgp, pgp->pdc_table + index);
}
