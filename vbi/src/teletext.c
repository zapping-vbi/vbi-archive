/*
 *  libzvbi - Teletext formatter
 *
 *  Copyright (C) 2000-2003 Michael H. Schimek
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

/* $Id: teletext.c,v 1.7.2.4 2003-09-24 18:49:57 mschimek Exp $ */

#include "../config.h"
#include "site_def.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "bcd.h"
#include "vt.h"
#include "export.h"
#include "vbi.h"
#include "hamm.h"
#include "lang.h"

struct pdc_date {
	signed			year		: 8;	/* 0 ... 99 */
	signed			month		: 8;	/* 0 ... 11 */
	signed			day		: 8;	/* 0 ... 30 */
} PACKED;

struct pdc_time {
	signed			hour		: 8;	/* 0 ... 23 */
	signed			min		: 8;	/* 0 ... 59 */
} PACKED;

struct pdc_pos {
	signed			row		: 8;	/* 1 ... 23 */
	signed			column_begin	: 8;	/* 0 ... 39 */
	signed			column_end	: 8;	/* 0 ... 40 */
} PACKED;

struct pdc_prog {
	struct pdc_date		ad;			/* Method B: year -1 */
	unsigned		pty		: 8;
	struct pdc_time		at1;			/* Method B: not given */
	struct pdc_time		at2;
	signed			cni		: 32;
	signed			length		: 16;	/* min */
	/* Method A: position of AT-1 and up to three PTLs.
	   Method B: position of PTL, elements 0, 2, 3 unused. */
	struct pdc_pos		at1_ptl_pos[4];
	signed			lto		: 8;	/* +/- 15 min */
	unsigned		caf		: 1;	/* Method A: not given */
};

static void
dump_pdc_prog			(struct pdc_prog *	p,
				 unsigned int		n)
{
	unsigned int i;

	for (i = 0; i < n; ++i, ++p)
		fprintf (stderr, "%2u: %02d-%02d-%02d "
			 "%02d:%02d (%02d:%02d) %3d min "
			 "cni=%08x pty=%02x lto=%d "
			 "at1/ptl=%d:%d-%d,%d:%d-%d,%d:%d-%d,%d:%d-%d "
			 "caf=%u\n",
			 i, p->ad.year, p->ad.month + 1, p->ad.day + 1,
			 p->at1.hour, p->at1.min,
			 p->at2.hour, p->at2.min, p->length,
			 p->cni, p->pty, p->lto,
			 p->at1_ptl_pos[0].row,
			 p->at1_ptl_pos[0].column_begin,
			 p->at1_ptl_pos[0].column_end,
			 p->at1_ptl_pos[1].row,
			 p->at1_ptl_pos[1].column_begin,
			 p->at1_ptl_pos[1].column_end,
			 p->at1_ptl_pos[2].row,
			 p->at1_ptl_pos[2].column_begin,
			 p->at1_ptl_pos[2].column_end,
			 p->at1_ptl_pos[3].row,
			 p->at1_ptl_pos[3].column_begin,
			 p->at1_ptl_pos[3].column_end,
			 p->caf);
}

struct temp {
	vbi_page		pg;

	const vt_magazine *	mag;
	const vt_extension *	ext;

	const vt_page *		vtp;

	vbi_wst_level		max_level;
	vbi_format_flags	flags;

	struct pdc_prog		pdc_table [25];
	unsigned int		pdc_table_size;

// stuff below not used yet, but it belongs here
// (future private portion of vbi_page)

	struct vbi_font_descr *	font[2];

	uint32_t		double_height_lower;	/* legacy */

	/* 0 header, 1 other rows. */
	vbi_opacity		page_opacity[2];
	vbi_opacity		boxed_opacity[2];

	/* Navigation related, see vbi_page_nav_link(). For
	   simplicity nav_index[] points from each character
	   in the TOP/FLOF row 25 (max 64 columns) to the
	   corresponding nav_link element. */
	vt_pagenum		nav_link[6];
	int8_t			nav_index[64];
};

static void
dump_temp			(struct temp *		t,
				 unsigned int		mode)
{
	unsigned int row;
	unsigned int column;
	vbi_char *acp;

	acp = t->pg.text;

	for (row = 0; row < t->pg.rows; ++row) {
		fprintf (stderr, "%2d: ", row);

		for (column = 0; column < t->pg.columns; ++acp, ++column) {
			int c;

			switch (mode) {
			case 0:
				c = acp->unicode;
				if (c < 0x20 || c >= 0x7F)
					c = '.';
				fputc (c, stderr);
				break;

			case 1:
				fprintf (stderr, "%04x ", acp->unicode);
				break;

			case 2:
				fprintf (stderr, "%04xF%dB%dS%dO%d ",
					 acp->unicode,
					 acp->foreground, acp->background,
					 acp->size, acp->opacity);
				break;
			}
		}

		fputc ('\n', stderr);
	}
}

static void
character_set_designation	(vbi_font_descr **	font,
				 const vt_extension *	ext,
				 const vt_page *	vtp);
static void
screen_color			(vbi_page *		pg,
				 unsigned int		flags,
				 unsigned int		color);
static vbi_bool
enhance				(struct temp *		t,
				 object_type		type,
				 const vt_triplet *	p,
				 int			n_triplets,
				 int			inv_row,
				 int			inv_column);

/* PDC -------------------------------------------------------------------- */

/*
 *  PDC Preselection method "A"
 *  ETS 300 231, Section 7.3.1.
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
2) Encountered pages which have proper CNI and AD, but no AT-1.
   Here random non-digit characters (usually space 0x20) can
   appear in place of the + before and/or after AT-1. Also
   encountered spaces before and/or after the minus, and a colon
   in place of the period separating hour and minute digits.
3) The % prefix form is permitted only when CNI, AD, PW combine,
   otherwise :% should appear. Some stations also use space
   0x20 as delimiter between combined CNI, AD, PW
   (<pre><text>0x20<text>...).
4) According to spec only the :% form is valid. (No evidence
   to the contrary, but that would be an exception.)
5) Encountered as end time of the last program on a page.
   Note 2) applies accordingly.
6) Postfix :% encountered in combined form ++PTL:%CNI.
*/

#define M_COLOR ((1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 6) | (1 << 7))
#define M_MAGENTA (1 << 0x05)
#define M_CONCEAL (1 << 0x18)

#define isnum(c) ((c) >= 0x30 && (c) <= 0x39)
/* NB uppercase only, unlike isxdigit() */
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

#ifndef TELETEXT_PDC_LOG
#define TELETEXT_PDC_LOG 0
#endif

#define pdc_log(templ, args...)						\
do { /* Check syntax in any case */					\
	if (TELETEXT_PDC_LOG)						\
		fprintf (stderr, templ , ##args);			\
} while (0)

/* BCD coded time (e.g. 0x2359) to pdc_time. */
static vbi_bool
pdc_bcd_time			(struct pdc_time *	t,
				 int			bcd)
{
	int h, m;

	if (!vbi_is_bcd (bcd))
		return FALSE;

	m = (bcd & 15) + ((bcd >> 4) & 15) * 10;

	bcd >>= 8;

	h = (bcd & 15) +  (bcd >> 4)       * 10;

	if (m >= 60 || h >= 24)
		return FALSE;

	t->hour = h;
	t->min = m;

	return TRUE;
}

/* Absolute difference of begin and end in minutes. */
static __inline__ int
pdc_time_diff			(struct pdc_time *	begin,
				 struct pdc_time *	end)
{
	int t;

	t = (end->hour - begin->hour) * 60 + end->min - begin->min;

	if (t < 0)
		t += 24 * 60;

	return t;
}

/*
 *  Evaluates PTL, this is a subfunction of pdc_method_a().
 *  Returns next column to process, >= column parameter.
 *
 *  p		- store PTL info here
 *  raw		- raw level one page (current row)
 *  row, column	- current position
 */
static __inline__ unsigned int
pdc_ptl				(struct temp *		t,
				 struct pdc_prog *	p,
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

			if (iscolon (separator) || isperiod (separator)) {
				int digit3 = vbi_ipar8 (raw[column + 3]);
				int digit4 = vbi_ipar8 (raw[column + 4]);
				unsigned int delimiter = (column < 35) ?
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

	if (p > t->pdc_table) {
	  	struct pdc_pos *tab;
		struct pdc_pos *pp;

		tab = p[-1].at1_ptl_pos;

		for (pp = &tab[1]; pp <= &tab[3]; ++pp)
			if (pp->row < 0) {
				if (pp >= &tab[2]
				    && (unsigned int) pp[-1].row < (row - 1))
					break; /* probably unrelated */

				pp->row		 = row;
				pp->column_begin = column_begin;
				pp->column_end   = column_end;
				break;
			}
	}

	/* No explicit postfix, must scan for unmarked
	   elements within PTL. */
	if (column == 40)
		column = column_at1;

	return column;
}

/* Copies AD, CNI, LTO in row p2 to rows p2 to pend - 1. */
static struct pdc_prog *
pdc_at2_fill			(struct pdc_prog *	table,
				 struct pdc_prog *	p2,
				 struct pdc_prog *	pend)
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

/*
 *  Scans a raw level one page for PDC data and stores the
 *  data in t->pdc_table. Returns the number of entries
 *  written.
 */
static unsigned int
pdc_method_a			(struct temp *		t)
{
	struct pdc_prog *p1;
	struct pdc_prog *p2;
	struct pdc_prog *pend;
	unsigned int row;
	vbi_bool have_at2;
	int pw_sum;
	int pw;

	CLEAR (t->pdc_table);

	t->pdc_table_size = 0;

	p1 = t->pdc_table; /* next AT-1 row */
	p2 = t->pdc_table; /* next AT-2 row */

	pend = t->pdc_table + N_ELEMENTS (t->pdc_table);

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

		raw = t->vtp->data.lop.raw[row];

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
					column = pdc_ptl (t, p1, raw,
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

			value = 0;
			digits = 0;

			column_begin = column;

			for (; column <= 39; ++column) {
				int c = vbi_ipar8 (raw[column]);

				if (0)
					pdc_log ("%d %02x value=%x "
						 "digits=%x\n",
						 column, c, value, digits);

				switch (c) {
				case 0x41 ... 0x46: /* xdigit */
					c += 0x3A - 0x41;
					/* fall through */

				case 0x30 ... 0x39: /* digit */
					value = value * 16 + c - 0x30;
					digits += 1;
					break;

				case 0x01 ... 0x07: /* color */
				case 0x18: /* conceal */
					/* Postfix control code */
					goto eval;

				case 0x20: /* space, observation 2) */
					for (; column <= 38; ++column) {
						c = vbi_ipar8 (raw[column + 1]);

						if (c != 0x20)
							break;
					}

					if (digits == 0x104) {
						if (column <= 38 && isminus (c))
							continue; /* zz.zz<spaces> */
						else
							goto eval;
					} else if (digits == 0x204) {
						if (column <= 38 && isnum (c))
							continue; /* zz.zz-<spaces> */
						else
							goto eval;
					} else {
						goto eval;
					}

				case 0x2E: /* period */
				case 0x3A: /* colon, observation 2) */
					if (digits == 0x002 || digits == 0x206) {
						digits += 0x100;
					} else {
						goto bad;
					}
					break;

				case 0x2D: /* minus */
					if (digits == 0x104) {
						digits = 0x204;
					} else {
						goto bad;
					}
					break;

				default: /* junk, observation 2) */
					if (digits == 0x104 || digits == 0x308) {
						goto eval; /* junk after [zz.zz-]zz.zz */
					} else {
						goto bad;
					}
				}
			}

		eval:
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
					if (!pdc_bcd_time (&p2->at2, value))
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
				if (!pdc_bcd_time (&p1->at1, value))
					goto bad;

				pdc_log ("AT-1 %04x\n", value);

				/* Shortcut: No date, not valid PDC-A */
				if (t->pdc_table[0].ad.year < 0)
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

				if (!pdc_bcd_time (&at, value & 0xFFFF))
					goto bad;
				if (!pdc_bcd_time (&p1->at1, value >> 16))
					goto bad;

				pdc_log ("AT-1 %08x\n", value);

				/* Shortcut: No date, not valid PDC-A */
				if (t->pdc_table[0].ad.year < 0)
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
	    || p1 == t->pdc_table
	    || t->pdc_table[0].ad.year < 0
	    || t->pdc_table[0].cni < 0) {
		return 0; /* invalid */
	}

	pdc_at2_fill (have_at2 ? t->pdc_table : p2, p2, p1);

	for (p2 = t->pdc_table; p2 < p1; ++p2) {
		struct pdc_prog *pp;
		unsigned int i;

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

		for (i = 0; i < 4; ++i) {
			if (p2[0].at1_ptl_pos[i].row > 0) {
				vbi_char *cp;
				unsigned int j;

				row = p2[0].at1_ptl_pos[i].row;

				cp = t->pg.text + t->pg.columns * row;

				for (j = 0; j < 40; ++j)
					cp[j].pdc = TRUE;

				if (t->pg.double_height_lower & (2 << row)) {
					cp += t->pg.columns;

					for (j = 0; j < 40; ++j)
						cp[j].pdc = TRUE;
				}
			} else {
				break;
			}
		}
	}

	if (0)
		dump_pdc_prog (t->pdc_table, p2 - t->pdc_table);

	return t->pdc_table_size = p2 - t->pdc_table;
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
				 vbi_pdc_preselection *	pl,
				 unsigned int		column,
				 unsigned int		row)
{
/*
  XXX TODO

  a) find table entry
  b) if no year query system time
  c) if no at1 assume same as at2
  d) title
 */
	return FALSE;
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
				 vbi_pdc_preselection *	pl,
				 unsigned int		index)
{
#if 0 // XXX TODO
	struct temp *t = PARENT (pg, struct temp, pg);

	if (index > t->pdc_table_size)
		return FALSE;

	pdc_prog_to_presel (pl, t, index);

	return TRUE;
#else
#endif
	return FALSE;
}

/*  Teletext page formatting ----------------------------------------------- */

#ifndef TELETEXT_FMT_LOG
#define TELETEXT_FMT_LOG 0
#endif

#define fmt_log(templ, args...)						\
do { /* Check syntax in any case */					\
	if (TELETEXT_FMT_LOG)						\
		fprintf (stderr, templ , ##args);			\
} while (0)

static void
character_set_designation	(vbi_font_descr **	font,
				 const vt_extension *	ext,
				 const vt_page *	vtp)
{
	int i;

/* XXX TODO per-page font override */

#ifdef libzvbi_TTX_OVERRIDE_CHAR_SET

	font[0] = vbi_font_descriptors + libzvbi_TTX_OVERRIDE_CHAR_SET;
	font[1] = vbi_font_descriptors + libzvbi_TTX_OVERRIDE_CHAR_SET;

	fprintf(stderr, "override char set with %d\n",
		libzvbi_TTX_OVERRIDE_CHAR_SET);
#else

	font[0] = vbi_font_descriptors + 0;
	font[1] = vbi_font_descriptors + 0;

	for (i = 0; i < 2; i++) {
		int char_set = ext->char_set[i];

		if (VALID_CHARACTER_SET(char_set))
			font[i] = vbi_font_descriptors + char_set;

		char_set = (char_set & ~7) + vtp->national;

		if (VALID_CHARACTER_SET(char_set))
			font[i] = vbi_font_descriptors + char_set;
	}
#endif
}

static void
screen_color			(vbi_page *		pg,
				 unsigned int		flags,
				 unsigned int		color)
{ 
	pg->screen_color = color;

	if (color == VBI_TRANSPARENT_BLACK
	    || (flags & (C5_NEWSFLASH | C6_SUBTITLE)))
		pg->screen_opacity = VBI_TRANSPARENT_SPACE;
	else
		pg->screen_opacity = VBI_OPAQUE;
}

static const vt_page *
resolve_obj_address		(struct temp *		t,
				 const vt_triplet **	trip,
				 int *			remaining,
				 object_type		type,
				 vbi_pgno		pgno,
				 object_address		address,
				 page_function		function)
{
	const vt_page *vtp;
	vbi_subno subno;
	unsigned int packet;
	unsigned int pointer;
	unsigned int i;

	subno = address & 15;
	packet = ((address >> 7) & 3);
	i = ((address >> 5) & 3) * 3 + type;

	fmt_log ("obj invocation, source page %03x/%04x, "
		 "pointer packet %d triplet %d\n",
		 pgno, subno, packet + 1, i);

	vtp = vbi_cache_get (t->pg.vbi, NUID0, pgno, subno, 0x000F,
			     /* user access */ FALSE);

	if (!vtp) {
		fmt_log ("... page not cached\n");
		goto failure;
	}

	if (vtp->function == PAGE_FUNCTION_UNKNOWN) {
		if (!vbi_convert_cached_page (t->pg.vbi, &vtp, function)) {
			fmt_log ("... no g/pop page or hamming error\n");
			goto failure;
		}
	} else if (vtp->function == PAGE_FUNCTION_POP) {
		/* vtp->function = function; */ /* POP/GPOP */
	} else if (vtp->function != function) {
		fmt_log ("... source page wrong function %d, expected %d\n",
			 vtp->function, function);
		goto failure;
	}

	pointer = vtp->data.pop.pointer[packet * 24 + i * 2 
					+ ((address >> 4) & 1)];

	fmt_log ("... triplet pointer %d\n", pointer);

	if (pointer > 506) {
		fmt_log ("... triplet pointer out of bounds (%d)\n", pointer);
		goto failure;
	}

	if (TELETEXT_FMT_LOG) {
		packet = (pointer / 13) + 3;

		if (packet <= 25) {
			fmt_log ("... object start in packet %d, "
				 "triplet %d (pointer %d)\n",
				 packet, pointer % 13, pointer);
		} else {
			fmt_log ("... object start in packet 26/%d, "
				 "triplet %d (pointer %d)\n",
				 packet - 26, pointer % 13, pointer);	
		}
	}

	*trip = vtp->data.pop.triplet + pointer;
	*remaining = N_ELEMENTS (vtp->data.pop.triplet) - (pointer + 1);

	fmt_log ("... obj def: ad 0x%02x mo 0x%04x dat %d=0x%x\n",
		 (*trip)->address, (*trip)->mode, (*trip)->data, (*trip)->data);

	address ^= (*trip)->address << 7;
	address ^= (*trip)->data;

	if ((*trip)->mode != (type + 0x14) || (address & 0x1FF)) {
		fmt_log ("... no object definition\n");
		goto failure;
	}

	*trip += 1;

	return vtp;

 failure:
	vbi_cache_unref (t->pg.vbi, vtp);

	return NULL;
}

static __inline__ const vt_page *
get_drcs_page			(struct temp *		t,
				 vbi_nuid		nuid,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 page_function		function)
{
	const vt_page *vtp;

	vtp = vbi_cache_get (t->pg.vbi, nuid, pgno, subno, 0x000F,
			     /* user_access */ FALSE);

	if (!vtp) {
		fmt_log ("... page not cached\n");
		goto failure;
	}

	if (vtp->function == PAGE_FUNCTION_UNKNOWN) {
		if (!vbi_convert_cached_page (t->pg.vbi, &vtp, function)) {
			fmt_log ("... no g/drcs page or hamming error\n");
			goto failure;
		}
	} else if (vtp->function == PAGE_FUNCTION_DRCS) {
		/* vtp->function = function; */ /* DRCS/GDRCS */
	} else if (vtp->function != function) {
		fmt_log ("... source page wrong function %d, expected %d\n",
			 vtp->function, function);
		goto failure;
	}

	return vtp;

 failure:
	vbi_cache_unref (t->pg.vbi, vtp);

	return NULL;
}

#define enhance_flush(column) _enhance_flush (column, t, type,		\
 	&ac, &mac, acp, inv_row, inv_column, 				\
	&active_row, &active_column, row_color, invert)

/*
 *  Flushes accumulated attributes up to but excluding column,
 *  or -1 to end of row. This is a subfunction of enhance(), we use
 *  the macro above.
 *
 *  type	- current object type
 *  ac		- attributes
 *  mac		- attributes mask
 *  acp		- cursor position
 *  inv_        - position at object invocation
 *  active_     - current position (inv_ offset)
 */
static void
_enhance_flush			(int			column,
				 struct temp *		t,
				 object_type		type,
				 vbi_char *		ac,
				 vbi_char *		mac,
				 vbi_char *		acp,
				 const int		inv_row,
				 const int		inv_column,
				 int *			active_row,
				 int *			active_column,
				 int			row_color,
				 int			invert)
{
	int row = inv_row + *active_row;
	int i;

	if (row >= 25)
		return;

	if (column == -1) {
		/* Flush entire row */

		switch (type) {
		case OBJ_TYPE_PASSIVE:
		case OBJ_TYPE_ADAPTIVE:
			column = *active_column + 1;
			break;

		default:
			column = 40;
			break;
		}
	}

	if (type == OBJ_TYPE_PASSIVE && !mac->unicode) {
		*active_column = column;
		return;
	}

	fmt_log ("flush [%04x%c,F%d%c,B%d%c,S%d%c,O%d%c,H%d%c] %d ... %d\n",
		 ac->unicode, mac->unicode ? '*' : ' ',
		 ac->foreground, mac->foreground ? '*' : ' ',
		 ac->background, mac->background ? '*' : ' ',
		 ac->size, mac->size ? '*' : ' ',
		 ac->opacity, mac->opacity ? '*' : ' ',
		 ac->flash, mac->flash ? '*' : ' ',
		 *active_column, column - 1);

	i = inv_column + *active_column;

	while (i < inv_column + column && i < 40) {
		vbi_char c;
		int raw;

		c = acp[i];

		if (mac->underline) {
			int u = ac->underline;

			if (!mac->unicode)
				ac->unicode = c.unicode;

			if (vbi_is_gfx (ac->unicode)) {
				if (u)
					ac->unicode &= ~0x20; /* separated */
				else
					ac->unicode |= 0x20; /* contiguous */
				mac->unicode = ~0;
				u = 0;
			}

			c.underline = u;
		}

		/* TODO: vbi_char is 64 bit, a masked move may work. */

		if (mac->foreground)
			c.foreground = (ac->foreground == VBI_TRANSPARENT_BLACK) ?
				row_color : ac->foreground;
		if (mac->background)
			c.background = (ac->background == VBI_TRANSPARENT_BLACK) ?
				row_color : ac->background;
		if (invert)
			SWAP (c.foreground, c.background);
		if (mac->opacity)
			c.opacity = ac->opacity;
		if (mac->flash)
			c.flash = ac->flash;
		if (mac->conceal)
			c.conceal = ac->conceal;
		if (mac->unicode) {
			c.unicode = ac->unicode;
			mac->unicode = 0;

			if (mac->size)
				c.size = ac->size;
			else if (c.size > VBI_DOUBLE_SIZE)
				c.size = VBI_NORMAL_SIZE;
		}

		acp[i++] = c;

		if (type == OBJ_TYPE_PASSIVE)
			break;

		if (type == OBJ_TYPE_ADAPTIVE)
			continue;

		/* OBJ_TYPE_ACTIVE */

		raw = (row == 0 && i < 9) ?
			0x20 : vbi_ipar8 (t->vtp->data.lop.raw[row][i - 1]);

		/* Set-after spacing attributes cancelling non-spacing */

		switch (raw) {
		case 0x00 ... 0x07:	/* alpha + foreground color */
		case 0x10 ... 0x17:	/* mosaic + foreground color */
			fmt_log ("... fg term %d %02x\n", i, raw);
			mac->foreground = 0;
			mac->conceal = 0;
			break;

		case 0x08:		/* flash */
			mac->flash = 0;
			break;

		case 0x0A:		/* end box */
		case 0x0B:		/* start box */
			if (i < 40
			    && raw == vbi_ipar8 (t->vtp->data.lop.raw[row][i])) {
				fmt_log ("... boxed term %d %02x\n", i, raw);
				mac->opacity = 0;
			}
			
			break;
			
		case 0x0D:		/* double height */
		case 0x0E:		/* double width */
		case 0x0F:		/* double size */
			fmt_log ("... size term %d %02x\n", i, raw);
			mac->size = 0;
			break;
		}
		
		if (i > 39)
			break;
		
		raw = (row == 0 && i < 8) ?
			0x20 : vbi_ipar8 (t->vtp->data.lop.raw[row][i]);

		/* Set-at spacing attributes cancelling non-spacing */
		
		switch (raw) {
		case 0x09:		/* steady */
			mac->flash = 0;
			break;
			
		case 0x0C:		/* normal size */
			fmt_log ("... size term %d %02x\n", i, raw);
			mac->size = 0;
			break;
			
		case 0x18:		/* conceal */
			mac->conceal = 0;
			break;
			
		/*
		 *  Non-spacing underlined/separated display attribute
		 *  cannot be cancelled by a subsequent spacing attribute.
		 */
			
		case 0x1C:		/* black background */
		case 0x1D:		/* new background */
			fmt_log ("... bg term %d %02x\n", i, raw);
			mac->background = 0;
			break;
		}
	}

	*active_column = column;
}

/*
 *  Recursive object invocation at row, column. Returns success.
 *
 *  type	- current object type
 *  p		- triplet requesting object
 */
static __inline__ vbi_bool
object_invocation		(struct temp *		t,
				 object_type		type,
				 const vt_triplet *	p,
				 int			row,
				 int			column)
{
	object_type new_type;
	unsigned int source;
	const vt_page *trip_vtp;
	const vt_triplet *trip;
	unsigned int n_triplets;
	vbi_bool success;

	fmt_log ("enhancement obj invocation source %d type %d\n",
		 source, new_type);

	new_type = p->mode & 3;
	source = (p->address >> 3) & 3;

	if (new_type <= type) {
		/* ETS 300 706, Section 13.2ff */
		fmt_log ("... priority violation %d -> %d\n",
			 type, new_type);
		return FALSE;
	}

	trip_vtp = NULL;
	n_triplets = 0;

	if (source == 0) {
		fmt_log ("... invalid source\n");
		return FALSE;
	} else if (source == 1) {
		unsigned int designation;
		unsigned int triplet;
		unsigned int offset;

		designation = (p->data >> 4) + ((p->address & 1) << 4);
		triplet = p->data & 15;

		fmt_log ("... local obj %d/%d\n", designation, triplet);

		if (type != LOCAL_ENHANCEMENT_DATA || triplet > 12) {
			fmt_log("... invalid type %d or triplet\n", type);
			return FALSE;
		}

		if (!(t->vtp->enh_lines & 1)) {
			fmt_log("... no packet %d\n", designation);
			return FALSE;
		}

		offset = designation * 13 + triplet;

		trip = t->vtp->data.enh_lop.enh + offset;
		n_triplets = N_ELEMENTS (t->vtp->data.enh_lop.enh) - offset;
	} else {
		page_function function;
		vbi_pgno pgno;
		unsigned int link;

		link = 0;

		if (source == 3) {
			fmt_log ("... global obj\n");

			function = PAGE_FUNCTION_GPOP;
			pgno = t->vtp->data.lop.link[24].pgno;

			if (NO_PAGE (pgno)) {
				if (t->max_level < VBI_WST_LEVEL_3p5
				    || NO_PAGE (pgno = t->mag->pop_link[8].pgno))
					pgno = t->mag->pop_link[0].pgno;
			} else {
				fmt_log("... X/27/4 GPOP overrides MOT\n");
			}
		} else {
			fmt_log ("... public obj\n");

			function = PAGE_FUNCTION_POP;
			pgno = t->vtp->data.lop.link[25].pgno;

			if (NO_PAGE (pgno)) {
				link = t->mag->pop_lut[t->vtp->pgno & 0xFF];

				if (link == 0) {
					fmt_log("... MOT pop_lut empty\n");
					return FALSE;
				}

				if (t->max_level < VBI_WST_LEVEL_3p5
				    || NO_PAGE (pgno = t->mag->pop_link
						[link + 8].pgno))
					pgno = t->mag->pop_link[link + 0].pgno;
			} else {
				fmt_log("... X/27/4 POP overrides MOT\n");
			}
		}

		if (NO_PAGE (pgno)) {
			fmt_log("... dead MOT link %u\n", link);
			return FALSE;
		}

		trip_vtp = resolve_obj_address (t, &trip, &n_triplets,
						new_type, pgno,
						(p->address << 7) + p->data,
						function);

		if (!trip_vtp)
			return FALSE;
	}

	success = enhance (t, new_type, trip, n_triplets, row, column);

	vbi_cache_unref (t->pg.vbi, trip_vtp);

	fmt_log ("... object done, success %d\n", success);

	return success;
}

/*
 *  Like object_invocation(), but uses default object links if
 *  available. Called when a page has no enhancement packets.
 */
static __inline__ vbi_bool
default_object_invocation	(struct temp *		t)
{
	const vt_pop_link *pop;
	unsigned int link;
	unsigned int order;
	unsigned int i;

	fmt_log ("default obj invocation\n");

	link = t->mag->pop_lut[t->vtp->pgno & 0xFF];

	if (link == 0) {
		fmt_log ("...no pop link\n");
		return FALSE;
	}

	pop = t->mag->pop_link + link + 8;

	if (t->max_level < VBI_WST_LEVEL_3p5 || NO_PAGE (pop->pgno)) {
		pop = t->mag->pop_link + link;

		if (NO_PAGE (pop->pgno)) {
			fmt_log ("... dead MOT pop link %u\n", link);
			return FALSE;
		}
	}

	order = (pop->default_obj[0].type > pop->default_obj[1].type);

	for (i = 0; i < 2; ++i) {
		object_type type;
		const vt_page *trip_vtp;
		const vt_triplet *trip;
		unsigned int n_triplets;
		vbi_bool success;

		type = pop->default_obj[i ^ order].type;

		if (type == OBJ_TYPE_NONE)
			continue;

		fmt_log ("... invocation %d, type %d\n", i ^ order, type);

		trip_vtp = resolve_obj_address
			(t, &trip, &n_triplets,
			 type, pop->pgno,
			 pop->default_obj[i ^ order].address,
			 PAGE_FUNCTION_POP);

		if (!trip_vtp)
			return FALSE;

		success = enhance (t, type, trip, n_triplets, 0, 0);

		vbi_cache_unref (t->pg.vbi, trip_vtp);

		if (!success)
			return FALSE;
	}

	fmt_log ("... default object done\n");

	return TRUE;
}

static __inline__ vbi_bool
reference_drcs_page		(struct temp *		t,
				 unsigned int		normal,
				 unsigned int		page,
				 unsigned int		offset,
				 unsigned int		s1)
{
	/* if (!t->pg.drcs[page]) */ {
		const vt_page *drcs_vtp;
		page_function function;
		vbi_pgno pgno;
		unsigned int link;

		link = 0;

		if (!normal) {
			function = PAGE_FUNCTION_GDRCS;
			pgno = t->vtp->data.lop.link[26].pgno;

			if (NO_PAGE (pgno)) {
				if (t->max_level < VBI_WST_LEVEL_3p5
				    || NO_PAGE (pgno = t->mag->drcs_link[8]))
					pgno = t->mag->drcs_link[0];
			} else {
				fmt_log ("... X/27/4 GDRCS overrides MOT\n");
			}
		} else {
			function = PAGE_FUNCTION_DRCS;
			pgno = t->vtp->data.lop.link[25].pgno;

			if (NO_PAGE (pgno)) {
				link = t->mag->drcs_lut[t->vtp->pgno & 0xFF];

				if (link == 0) {
					fmt_log ("... MOT drcs_lut empty\n");
					return FALSE;
				}

				if (t->max_level < VBI_WST_LEVEL_3p5
				    || NO_PAGE (pgno = t->mag->drcs_link[link + 8]))
					pgno = t->mag->drcs_link[link + 0];
			} else {
				fmt_log ("... X/27/4 DRCS overrides MOT\n");
			}
		}

		if (NO_PAGE (pgno)) {
			fmt_log ("... dead MOT link %d\n", link);
			return FALSE;
		}

		fmt_log ("... %s drcs from page %03x/%04x\n",
			 normal ? "normal" : "global", pgno, s1);

		drcs_vtp = get_drcs_page (t, NUID0, pgno, s1, function);

		if (!drcs_vtp)
			return FALSE;

		if (drcs_vtp->data.drcs.invalid & (1ULL << offset)) {
			fmt_log ("... invalid drcs, prob. tx error\n");
			vbi_cache_unref (t->pg.vbi, drcs_vtp);
			return FALSE;
		}

		/* FIXME keep reference */

		t->pg.drcs[page] = drcs_vtp->data.drcs.bits[0];

		vbi_cache_unref (t->pg.vbi, drcs_vtp);
	}

	return TRUE;
}

/*
 *  Enhances a level one page with data from page enhancement
 *  packets. May be called recursively. Returns success.
 *
 *  type	- current object type
 *  p		- vector of enhancement triplets
 *  n_triplets	- length of vector
 *  inv_	- cursor position at object invocation, cursor
 *		  is moved relative to this (default 0, 0)
 *
 *  XXX panels not implemented
 */
static vbi_bool
enhance				(struct temp *		t,
				 object_type		type,
				 const vt_triplet *	p,
				 int			n_triplets,
				 int			inv_row,
				 int			inv_column)
{
	vbi_char ac, mac, *acp;
	int active_column, active_row;
	int offset_column, offset_row;
	int row_color, next_row_color;
	struct vbi_font_descr *font;
	int invert;
	int drcs_s1[2];
	struct pdc_prog *p1, pdc_tmp;
	int pdc_hour_mode;

	/* Current position rel. inv_row, inv_column) */

	active_column = 0;
	active_row = 0;

	acp = t->pg.text + inv_row * t->pg.columns;

	/* Origin modifier, reset after recurs. obj invocation */

	offset_column = 0;
	offset_row = 0;


	row_color = t->ext->def_row_color;
	next_row_color = row_color;

	drcs_s1[0] = 0; /* global */
	drcs_s1[1] = 0; /* normal */

	font = t->pg.font[0];

	/* Accumulated attributes and attr mask */

	CLEAR (ac);
	CLEAR (mac);

	invert = 0;

	if (type == OBJ_TYPE_PASSIVE) {
		ac.foreground	= VBI_WHITE;
		ac.background	= VBI_BLACK;
		ac.opacity	= t->pg.page_opacity[1];

		mac.foreground	= ~0;
		mac.background	= ~0;
		mac.opacity	= ~0;
		mac.size	= ~0;
		mac.underline	= ~0;
		mac.conceal	= ~0;
		mac.flash	= ~0;
	}

	/*
	 *  PDC Preselection method "B"
	 *  ETS 300 231, Section 7.3.2.
	 */

	CLEAR (pdc_tmp);

	pdc_tmp.ad.year		= -1; /* not given */
	pdc_tmp.ad.month	= -1;
	pdc_tmp.at1.hour	= -1; /* not given */
	pdc_tmp.at2.hour	= -1;

	p1 = t->pdc_table;

	pdc_hour_mode = 0; /* mode of previous hour triplet */


	for (; n_triplets > 0; ++p, --n_triplets) {
		if (p->address >= 40) {
			/*
			 *  Row address triplets
			 */

			int s = p->data >> 5;
			int row = (p->address - 40) ? : (25 - 1);
			int column = 0;

			if (pdc_hour_mode) {
				fmt_log ("invalid pdc hours, no minutes follow\n");
				return FALSE;
			}

			switch (p->mode) {
			case 0x00:		/* full screen color */
				if (t->max_level >= VBI_WST_LEVEL_2p5
				    && s == 0 && type <= OBJ_TYPE_ACTIVE)
					screen_color (&t->pg, t->vtp->flags,
						      p->data & 0x1F);
				break;

			case 0x07:		/* address display row 0 */
				if (p->address != 0x3F)
					break; /* reserved, no position */

				row = 0;

				/* fall through */

			case 0x01:		/* full row color */
				row_color = next_row_color;

				if (s == 0) {
					row_color = p->data & 0x1F;
					next_row_color = t->ext->def_row_color;
				} else if (s == 3) {
					row_color = p->data & 0x1F;
					next_row_color = row_color;
				}

				goto set_active;

			case 0x02:		/* reserved */
			case 0x03:		/* reserved */
				break;

			case 0x04:		/* set active position */
				if (t->max_level >= VBI_WST_LEVEL_2p5) {
					if (p->data >= 40)
						break; /* reserved */

					column = p->data;
				}

				if (row > active_row)
					row_color = next_row_color;

			set_active:
				if (row > 0 && (t->flags & VBI_HEADER_ONLY)) {
					for (; n_triplets > 1; ++p, --n_triplets)
						if (p[1].address >= 40) {
							unsigned int mode;

							mode = p[1].mode;

							if (mode == 0x07)
								break;
							else if (mode >= 0x1F)
								goto terminate;
						}
					break;
				}

				fmt_log ("enh set_active row %d col %d\n",
					 row, column);

				if (row > active_row) {
					enhance_flush (-1); /* flush row */

					if (type != OBJ_TYPE_PASSIVE)
						CLEAR (mac);
				}

				active_row = row;
				active_column = column;

				acp = &t->pg.text[(inv_row + active_row)
						  * t->pg.columns];
				break;

			case 0x05:		/* reserved */
			case 0x06:		/* reserved */
				break;

			case 0x08:		/* PDC data - Country of Origin
						   and Programme Source */
				pdc_tmp.cni = p->address * 256 + p->data;
				break;

			case 0x09:		/* PDC data - Month and Day */
				pdc_tmp.ad.month = (p->address & 15) - 1;
				pdc_tmp.ad.day = (p->data >> 4) * 10
					+ (p->data & 15) - 1;
				break;

			case 0x0A:		/* PDC data - Cursor Row and
						   Announced Starting Time Hours */
				if (!(t->flags & VBI_PDC_LINKS))
					break;

				if (pdc_tmp.ad.month < 0
				    || pdc_tmp.cni == 0) {
					return FALSE;
				}

				pdc_tmp.at2.hour = ((p->data & 0x30) >> 4) * 10
					+ (p->data & 15);
				pdc_tmp.length = 0;
				pdc_tmp.at1_ptl_pos[1].row = row;
				pdc_tmp.caf = !!(p->data & 0x40);

				pdc_hour_mode = p->mode;
				break;

			case 0x0B:		/* PDC data - Cursor Row and
						   Announced Finishing Time Hours */
				pdc_tmp.at2.hour =
					((p->data & 0x70) >> 4) * 10
					+ (p->data & 15);

				pdc_hour_mode = p->mode;
				break;

			case 0x0C:		/* PDC data - Cursor Row and
						   Local Time Offset */
				pdc_tmp.lto = p->data;

				if (p->data & 0x40) /* 6 bit two's complement */
					pdc_tmp.lto |= ~0x7F;

				break;

			case 0x0D:		/* PDC data - Series Identifier
						   and Series Code */
				if (p->address == 0x30) {
					break;
				}

				pdc_tmp.pty = 0x80 + p->data;

				break;

			case 0x0E:		/* reserved */
			case 0x0F:		/* reserved */
				break;

			case 0x10:		/* origin modifier */
				if (t->max_level < VBI_WST_LEVEL_2p5)
					break;

				if (p->data >= 72)
					break; /* invalid */

				offset_column = p->data;
				offset_row = p->address - 40;

				fmt_log ("enh origin modifier col %+d row %+d\n",
					 offset_column, offset_row);

				break;

			case 0x11 ... 0x13:	/* object invocation */
				if (t->max_level < VBI_WST_LEVEL_2p5)
					break;

				row = inv_row + active_row;
				column = inv_column + active_column;

				if (!object_invocation (t, type, p,
							row + offset_row,
							column + offset_column))
					return FALSE;

				offset_row = 0;
				offset_column = 0;

				break;

			case 0x14:		/* reserved */
				break;

			case 0x15 ... 0x17:	/* object definition */
				fmt_log ("enh obj definition 0x%02x 0x%02x\n",
					 p->mode, p->data);
				goto terminate;

			case 0x18:		/* drcs mode */
				fmt_log ("enh DRCS mode 0x%02x\n", p->data);
				drcs_s1[p->data >> 6] = p->data & 15;
				break;

			case 0x19 ... 0x1E:	/* reserved */
				break;

			case 0x1F:		/* termination marker */
			default:
	                terminate:
				enhance_flush (-1); /* flush row */
				fmt_log ("enh terminated %02x\n", p->mode);
				goto finish;
			}
		} else {
			/*
			 *  Column address triplets
			 */

			int s = p->data >> 5;
			int column = p->address;
			int unicode;

			switch (p->mode) {
			case 0x00:		/* foreground color */
				if (t->max_level >= VBI_WST_LEVEL_2p5 && s == 0) {
					if (column > active_column)
						enhance_flush (column);

					ac.foreground = p->data & 0x1F;
					mac.foreground = ~0;

					fmt_log ("enh col %d foreground %d\n",
						 active_column, ac.foreground);
				}

				break;

			case 0x01:		/* G1 block mosaic character */
				if (t->max_level >= VBI_WST_LEVEL_2p5) {
					if (column > active_column)
						enhance_flush(column);

					if (p->data & 0x20) {
						/* G1 contiguous */
						unicode = 0xEE00 + p->data;

						goto store;
					} else if (p->data >= 0x40) {
						unicode = vbi_teletext_unicode
						       (font->G0, NO_SUBSET, p->data);
						goto store;
					}
				}

				break;

			case 0x0B:		/* G3 smooth mosaic or
						   line drawing character */
				if (t->max_level < VBI_WST_LEVEL_2p5)
					break;

				/* fall through */

			case 0x02:		/* G3 smooth mosaic or
						   line drawing character */
				if (p->data >= 0x20) {
					if (column > active_column)
						enhance_flush(column);

					unicode = 0xEF00 + p->data;
					goto store;
				}

				break;

			case 0x03:		/* background color */
				if (t->max_level >= VBI_WST_LEVEL_2p5 && s == 0) {
					if (column > active_column)
						enhance_flush(column);

					ac.background = p->data & 0x1F;
					mac.background = ~0;

					fmt_log ("enh col %d background %d\n",
						 active_column, ac.background);
				}

				break;

			case 0x04:		/* reserved */
			case 0x05:		/* reserved */
				break;

			case 0x06:		/* PDC data - Cursor Column and
						   Announced Starting and
						   Finishing Time Minutes */
				if (!(t->flags & VBI_PDC_LINKS))
					break;

				pdc_tmp.at2.min = (p->data >> 4) * 10
					+ (p->data & 15);

				switch (pdc_hour_mode) {
				case 0x0A: /* Starting time */
					if (p1 > t->pdc_table && p1[-1].length == 0) {
						p1[-1].length = pdc_time_diff
							(&p1[-1].at2, &pdc_tmp.at2);

						if (p1[-1].length >= 12 * 60) {
							/* nonsense */
							--p1;
						}
					}

					pdc_tmp.at1_ptl_pos[1].column_begin = column;
					pdc_tmp.at1_ptl_pos[1].column_end = 40;

					if (p1 >= t->pdc_table
					    + N_ELEMENTS (t->pdc_table))
						return FALSE;

					*p1++ = pdc_tmp;

					pdc_tmp.pty = 0;

					break;

				case 0x0B: /* Finishing time */
					if (p1 == t->pdc_table)
						return FALSE;

					if (pdc_tmp.at2.hour >= 40) {
						/* Duration */
						p1[-1].length =
							(pdc_tmp.at2.hour - 40) * 60
							+ pdc_tmp.at2.min;
					} else {
						/* End time */
						p1[-1].length = pdc_time_diff
							(&p1[-1].at2, &pdc_tmp.at2);
					}

					break;

				default:
					fmt_log ("pdc hour triplet missing\n");
					return FALSE;
				}

				pdc_hour_mode = 0;

				break;

			case 0x07:		/* additional flash functions */
				if (t->max_level >= VBI_WST_LEVEL_2p5) {
					if (column > active_column)
						enhance_flush(column);

					/*
					 *  Only one flash function (if any)
					 *  implemented:
					 *  Mode 1 - Normal flash to background color
					 *  Rate 0 - Slow rate (1 Hz)
					 */
					ac.flash = !!(p->data & 3);
					mac.flash = ~0;

					fmt_log ("enh col %d flash 0x%02x\n",
						 active_column, p->data);
				}

				break;

			case 0x08:		/* modified G0 and G2
						   character set designation */
				if (t->max_level >= VBI_WST_LEVEL_2p5) {
					if (column > active_column)
						enhance_flush(column);

					if (VALID_CHARACTER_SET(p->data))
						font = vbi_font_descriptors + p->data;
					else
						font = t->pg.font[0];

					fmt_log ("enh col %d modify character "
						 "set %d\n", active_column, p->data);
				}

				break;

			case 0x09:		/* G0 character */
				if (t->max_level >= VBI_WST_LEVEL_2p5
				    && p->data >= 0x20) {
					if (column > active_column)
						enhance_flush(column);

					unicode = vbi_teletext_unicode
						(font->G0, NO_SUBSET, p->data);
					goto store;
				}

				break;

			case 0x0A:		/* reserved */
				break;

			case 0x0C:		/* display attributes */
				if (t->max_level < VBI_WST_LEVEL_2p5)
					break;

				if (column > active_column)
					enhance_flush(column);

				ac.size = ((p->data & 0x40) ? VBI_DOUBLE_WIDTH : 0)
					+ ((p->data & 1) ? VBI_DOUBLE_HEIGHT : 0);
				mac.size = ~0;

				if (p->data & 2) {
					if (t->vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE))
						ac.opacity = VBI_SEMI_TRANSPARENT;
					else
						ac.opacity = VBI_TRANSPARENT_SPACE;
				} else
					ac.opacity = t->pg.page_opacity[1];
				mac.opacity = ~0;

				ac.conceal = !!(p->data & 4);
				mac.conceal = ~0;

				/* (p->data & 8) reserved */

				invert = p->data & 0x10;

				ac.underline = !!(p->data & 0x20);
				mac.underline = ~0;

				fmt_log ("enh col %d display attr 0x%02x\n",
					 active_column, p->data);

				break;

			case 0x0D:		/* drcs character invocation */
			{
				unsigned int normal;
				unsigned int offset;
				unsigned int page;

				if (t->max_level < VBI_WST_LEVEL_2p5)
					break;

				normal = p->data >> 6;
				offset = p->data & 0x3F;

				if (offset >= 48) {
					fmt_log ("invalid drcs offset %u\n", offset);
					break;
				}

				if (column > active_column)
					enhance_flush (column);

				page = normal * 16 + drcs_s1[normal];

				fmt_log ("enh col %d DRCS %d/0x%02x\n",
					 active_column, page, p->data);

				if (!reference_drcs_page (t, normal, page, offset,
							  drcs_s1[normal]))
					return FALSE;

				unicode = 0xF000 + (page << 6) + offset;

				goto store;
			}

			case 0x0E:		/* font style */
			{
				int italic, bold, proportional;
				int col, row, count;
				vbi_char *acp;

				if (t->max_level < VBI_WST_LEVEL_3p5)
					break;

				row = inv_row + active_row;
				count = (p->data >> 4) + 1;
				acp = &t->pg.text[row * t->pg.columns];

				proportional = (p->data >> 0) & 1;
				bold = (p->data >> 1) & 1;
				italic = (p->data >> 2) & 1;

				while (row < 25 && count > 0) {
					for (col = inv_column + column;
					     col < 40; ++col) {
						acp[col].italic = italic;
		    				acp[col].bold = bold;
						acp[col].proportional = proportional;
					}

					acp += t->pg.columns;
					row++;
					count--;
				}

				fmt_log ("enh col %d font style 0x%02x\n",
					 active_column, p->data);

				break;
			}

			case 0x0F:		/* G2 character */
				if (p->data >= 0x20) {
					if (column > active_column)
						enhance_flush(column);

					unicode = vbi_teletext_unicode
						(font->G2, NO_SUBSET, p->data);
					goto store;
				}

				break;

			case 0x10 ... 0x1F:	/* characters including
						   diacritical marks */
				if (p->data >= 0x20) {
					if (column > active_column)
						enhance_flush(column);

					unicode = vbi_teletext_composed_unicode
						(p->mode - 0x10, p->data);
			store:
					fmt_log ("enh row %d col %d print "
						 "0x%02x/0x%02x -> 0x%04x %c\n",
						 active_row, active_column,
						 p->mode, p->data,
						 unicode, unicode & 0xFF);

					ac.unicode = unicode;
					mac.unicode = ~0;
				}

				break;
			}
		}
	}

finish:
	if (p1 > t->pdc_table) {
		struct pdc_prog *p2;

		if (pdc_hour_mode || p1[-1].length == 0)
			p1--; /* incomplete start or end tag */

		for (p2 = t->pdc_table; p2 < p1; ++p2) {
			vbi_char *cp;
			unsigned int j;

			cp = t->pg.text + t->pg.columns
				* p2->at1_ptl_pos[1].row;

			for (j = 0; j < 40; ++j)
				cp[j].pdc = TRUE;
		}

		if (0)
			dump_pdc_prog (t->pdc_table, p1 - t->pdc_table);
	}

	if (0)
		dump_temp (t, 2);

	return TRUE;
}

static void
post_enhance			(struct temp *		t)
{
	vbi_char ac, *acp;
	unsigned int row;
	unsigned int column;
	unsigned int last_row;
	unsigned int last_column;

	acp = t->pg.text;

	last_row = (t->flags & VBI_HEADER_ONLY) ? 0 : 25 - 2;
	last_column = t->pg.columns - 1;

	for (row = 0; row <= last_row; ++row) {
		for (column = 0; column < t->pg.columns; ++acp, ++column) {
			if (acp->opacity == VBI_TRANSPARENT_SPACE
			    || (acp->foreground == VBI_TRANSPARENT_BLACK
				&& acp->background == VBI_TRANSPARENT_BLACK)) {
				acp->opacity = VBI_TRANSPARENT_SPACE;
				acp->unicode = 0x0020;
			} else if (acp->background == VBI_TRANSPARENT_BLACK) {
				acp->opacity = VBI_SEMI_TRANSPARENT;
			} else {
				/* transparent foreground not implemented */
			}

			switch (acp->size) {
			case VBI_NORMAL_SIZE:
				if (row < last_row
				    && (acp[t->pg.columns].size == VBI_DOUBLE_HEIGHT2
					|| acp[t->pg.columns].size == VBI_DOUBLE_SIZE2)) {
					acp[t->pg.columns].unicode = 0x0020;
					acp[t->pg.columns].size = VBI_NORMAL_SIZE;
				}

				if (column < 39
				    && (acp[1].size == VBI_OVER_TOP
					|| acp[1].size == VBI_OVER_BOTTOM)) {
					acp[1].unicode = 0x0020;
					acp[1].size = VBI_NORMAL_SIZE;
				}

				break;

			case VBI_DOUBLE_HEIGHT:
				if (row < last_row) {
					ac = acp[0];
					ac.size = VBI_DOUBLE_HEIGHT2;
					acp[t->pg.columns] = ac;
				}

				break;

			case VBI_DOUBLE_SIZE:
				if (row < last_row) {
					ac = acp[0];
					ac.size = VBI_DOUBLE_SIZE2;
					acp[t->pg.columns] = ac;
					ac.size = VBI_OVER_BOTTOM;
					acp[t->pg.columns + 1] = ac;
				}

				/* fall through */

			case VBI_DOUBLE_WIDTH:
				if (column < 39) {
					ac = acp[0];
					ac.size = VBI_OVER_TOP;
					acp[1] = ac;
				}

				break;

			default:
				break;
			}
		}

		fmt_log ("\n");
	}
}

/*
 *  Often LOPs have a blank (black) first column, containing
 *  set-after attributes in all rows. Not so the last column.
 *
 *  For a more balanced view we optionally add a 41st column,
 *  if possible extending column 39.
 */
static void
column_41			(struct temp *		t)
{
	vbi_char ac, *acp;
	unsigned int row;
	unsigned int first_row;
	unsigned int last_row;

	first_row = 0;
	last_row = (t->flags & VBI_HEADER_ONLY) ? 0 : 25;

	acp = t->pg.text;

	for (row = 0; row <= last_row; ++row, acp += 41) {
		if ((acp[0].unicode == 0x0020
		     && acp[0].foreground == VBI_BLACK)
		    || (vbi_is_gfx (acp[39].unicode)
			&& (acp[38].unicode != acp[39].unicode
			    || acp[38].foreground != acp[39].foreground
			    || acp[38].background != acp[39].background))) {
			if (row == 0)
				first_row = 1;
			else
				break;
		}
	}

	CLEAR (ac);
		
	ac.unicode	= 0x0020;
	ac.foreground	= t->ext->foreground_clut + VBI_WHITE;
	ac.background	= t->ext->background_clut + VBI_BLACK;

	acp = t->pg.text;

	/* Header goes separately. */

	if (first_row > 0 || row <= last_row) {
		ac.opacity = t->pg.page_opacity[0];
		acp[40] = ac;
		acp += 41;
	}

	if (row <= last_row) {
		ac.opacity = t->pg.page_opacity[1];

		for (row = 1; row <= last_row; ++row, acp += 41)
			acp[40] = ac;
	} else {
		for (row = first_row; row <= last_row; ++row, acp += 41) {
			acp[40] = acp[39];

			if (!vbi_is_gfx (acp[39].unicode))
				acp[40].unicode = 0x0020;
		}
	}
}

/*
 *  Formats a level one page.
 */
static void
level_one_page			(struct temp *		t)
{
	static const unsigned int mosaic_separate = 0xEE00 - 0x20;
	static const unsigned int mosaic_contiguous = 0xEE20 - 0x20;
	char buf[16];
	unsigned int column;
	unsigned int row;
	unsigned int i;

	/* Current page number in header */

	sprintf (buf, "\2%03x.%02x\7",
		 t->pg.pgno & 0xFFF, t->pg.subno & 0xFF);

	t->pg.double_height_lower = 0;

	i = 0; /* t->vtp->data.lop.raw[] */

	for (row = 0; row < t->pg.rows; ++row) {
		struct vbi_font_descr *font;
		unsigned int mosaic_plane;
		unsigned int held_mosaic_unicode;
		vbi_bool hold;
		vbi_bool mosaic;
		vbi_bool double_height;
		vbi_bool wide_char;
		vbi_char ac, *acp;

		acp = t->pg.text + row * t->pg.columns;

		/* G1 block mosaic, blank, contiguous */
		held_mosaic_unicode = mosaic_contiguous + 0x20;

		CLEAR (ac);

		ac.unicode	= 0x0020;
		ac.foreground	= t->ext->foreground_clut + VBI_WHITE;
		ac.background	= t->ext->background_clut + VBI_BLACK;
		mosaic_plane	= mosaic_contiguous;
		ac.opacity	= t->pg.page_opacity[row > 0];
		font		= t->pg.font[0];
		hold		= FALSE;
		mosaic		= FALSE;

		double_height	= FALSE;
		wide_char	= FALSE;

		for (column = 0; column < 40; ++column) {
			int raw;

			if (row == 0 && column < 8) {
				raw = buf[column];
				++i;
			} else {
				raw = vbi_ipar8 (t->vtp->data.lop.raw[0][++i]);

				if (raw < 0) /* parity error */
					raw = 0x20;
			}

			/* Set-at spacing attributes */

			switch (raw) {
			case 0x09:		/* steady */
				ac.flash = FALSE;
				break;

			case 0x0C:		/* normal size */
				ac.size = VBI_NORMAL_SIZE;
				break;

			case 0x18:		/* conceal */
				ac.conceal = TRUE;
				break;

			case 0x19:		/* contiguous mosaics */
				mosaic_plane = mosaic_contiguous;
				break;

			case 0x1A:		/* separated mosaics */
				mosaic_plane = mosaic_separate;
				break;

			case 0x1C:		/* black background */
				ac.background = t->ext->background_clut
					+ VBI_BLACK;
				break;

			case 0x1D:		/* new background */
				ac.background = t->ext->background_clut
					+ (ac.foreground & 7);
				break;

			case 0x1E:		/* hold mosaic */
				hold = TRUE;
				break;

			default:
				break;
			}

			if (raw <= 0x1F) {
				ac.unicode = (hold & mosaic) ?
					held_mosaic_unicode : 0x0020;
			} else {
				if (mosaic && (raw & 0x20)) {
					held_mosaic_unicode = mosaic_plane + raw;
					ac.unicode = held_mosaic_unicode;
				} else {
					ac.unicode = vbi_teletext_unicode
						(font->G0, font->subset, raw);
				}
			}

			if (!wide_char) {
				acp[column] = ac;

				wide_char = /*!!*/(ac.size & VBI_DOUBLE_WIDTH);

				if (wide_char && column < 39) {
					acp[column + 1] = ac;
					acp[column + 1].size = VBI_OVER_TOP;
				}
			}

			/* Set-after spacing attributes */

			switch (raw) {
			case 0x00 ... 0x07:	/* alpha + foreground color */
				ac.foreground = t->ext->foreground_clut
					+ (raw & 7);
				ac.conceal = FALSE;
				mosaic = FALSE;
				break;

			case 0x08:		/* flash */
				ac.flash = TRUE;
				break;

			case 0x0A:		/* end box */
				if (column >= 39)
					break;
				if (0x0A == vbi_ipar8 (t->vtp->data.lop.raw[0][i]))
					ac.opacity = t->pg.page_opacity[row > 0];
				break;

			case 0x0B:		/* start box */
				if (column >= 39)
					break;
				if (0x0B == vbi_ipar8 (t->vtp->data.lop.raw[0][i]))
					ac.opacity = t->pg.boxed_opacity[row > 0];
				break;

			case 0x0D:		/* double height */
				if (row == 0 || row >= 23)
					break;
				ac.size = VBI_DOUBLE_HEIGHT;
				double_height = TRUE;
				break;

			case 0x0E:		/* double width */
				fmt_log ("spacing col %d row %d double width\n",
					column, row);
				if (column >= 39)
					break;
				ac.size = VBI_DOUBLE_WIDTH;
				break;

			case 0x0F:		/* double size */
				fmt_log ("spacing col %d row %d double size\n",
					column, row);
				if (column >= 39 || row == 0 || row >= 23)
					break;
				ac.size = VBI_DOUBLE_SIZE;
				double_height = TRUE;
				break;

			case 0x10 ... 0x17:	/* mosaic + foreground color */
				ac.foreground = t->ext->foreground_clut
					+ (raw & 7);
				ac.conceal = FALSE;
				mosaic = TRUE;
				break;

			case 0x1B:		/* ESC */
				font = (font == t->pg.font[0]) ?
					t->pg.font[1] : t->pg.font[0];
				break;

			case 0x1F:		/* release mosaic */
				hold = FALSE;
				break;
			}
		}

		if (double_height) {
			for (column = 0; column < t->pg.columns; ++column) {
				ac = acp[column];

				switch (ac.size) {
				case VBI_DOUBLE_HEIGHT:
					ac.size = VBI_DOUBLE_HEIGHT2;
					acp[t->pg.columns + column] = ac;
					break;
		
				case VBI_DOUBLE_SIZE:
					ac.size = VBI_DOUBLE_SIZE2;
					acp[t->pg.columns + column] = ac;
					++column;
					ac.size = VBI_OVER_BOTTOM;
					acp[t->pg.columns + column] = ac;
					break;

				default: /* NORMAL, DOUBLE_WIDTH, OVER_TOP */
					ac.size = VBI_NORMAL_SIZE;
					ac.unicode = 0x0020;
					acp[t->pg.columns + column] = ac;
					break;
				}
			}

			i += 40;
			++row;

			t->pg.double_height_lower |= 1 << row;
		}
	}

	if (0) {
		if (row < 25) {
			vbi_char ac;

			CLEAR (ac);

			ac.foreground	= t->ext->foreground_clut + VBI_WHITE;
			ac.background	= t->ext->background_clut + VBI_BLACK;
			ac.opacity	= t->pg.page_opacity[1];
			ac.unicode	= 0x0020;

			for (i = row * t->pg.columns;
			     i < t->pg.rows * t->pg.columns; ++i)
				t->pg.text[i] = ac;
		}
	}

	if (0)
		dump_temp (t, 0);
}

/*  Hyperlinks  ------------------------------------------------------------ */

static const uint8_t SECTION_SIGN = 0xA7;

static unsigned int
keycmp				(const char *		s1,
				 const char *		key)
{
	const char *s;
	char c;

	for (s = s1; (c = *key); ++s, ++key) {
		if (*s == c)
			continue;

		if (c >= 'a' && c <= 'z')
			if ((*s ^ c) == 0x20)
				continue;

		return 0;
	}

	return s - s1;
}

static vbi_bool
keyword				(vbi_link *		ld,
				 const char *		buf,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 unsigned int *		start,
				 unsigned int *		end)
{
	const char *s;
	unsigned int proto;
	unsigned int address;

	ld->type	= VBI_LINK_NONE;
	ld->name[0]	= 0;
	ld->url[0]	= 0;
	ld->pgno	= 0;
	ld->subno	= VBI_ANY_SUBNO;

	s = buf + *start;
	*end = *start + 1; /* unknown character */

	if (isdigit (*s)) {
		const char *s1;
		unsigned int n1, n2;
		unsigned int digits;

		s1 = s;
		n1 = 0;

		do n1 = n1 * 16 + (*s & 15);
		while (isdigit (*++s));

		digits = s - s1;
		*end += digits - 1;

		if (digits > 3 || isdigit (s1[-1]))
			return FALSE;

		if (digits == 3) {
			if (pgno == (vbi_pgno) n1)
				return FALSE;

			if (n1 < 0x100 || n1 > 0x899)
				return FALSE;

			ld->type = VBI_LINK_PAGE;
			ld->pgno = n1;

			return TRUE;
		}

		if (*s != '/' && *s != ':')
			return FALSE;

		s1 = ++s;
		n2 = 0;

		while (isdigit (*s))
			n2 = n2 * 16 + (*s++ & 15);

		digits = s - s1; 
		*end += digits + 1;

		if (digits == 0 || digits > 2
		    || subno != (vbi_subno) n1)
			return FALSE;

		ld->type = VBI_LINK_SUBPAGE;
		ld->pgno = pgno;

		if (n1 == n2)
			ld->subno = 0x01; /* subpage "n1/n2" -> "1/n2" */
		else
			ld->subno = vbi_add_bcd (n1, 0x01);

		return TRUE;
	} else if (*s == '>' && s[1] == '>' && s[-1] != '>') {
		for (s += 2; *s == 0x20; ++s)
			;

		*end = s - buf;

		if (*s)
			return FALSE;

		if (subno == 0 || subno == VBI_ANY_SUBNO) {
			if (pgno == 0x899)
				return FALSE;

			ld->type = VBI_LINK_PAGE;
			ld->pgno = vbi_add_bcd (pgno, 0x001);
			return TRUE;
		} else if (subno < 0x99) {
			/* XXX wrap? */

			ld->type = VBI_LINK_SUBPAGE;
			ld->pgno = pgno;
			ld->subno = vbi_add_bcd (subno, 0x01);
			return TRUE;
		}

		return FALSE;
	} else if (*s == 'h') {
		if (!(proto = keycmp (s, "https://"))
		    && !(proto = keycmp (s, "http://")))
			return FALSE;
		ld->type = VBI_LINK_HTTP;
	} else if (*s == '(') {
		if (!(proto = keycmp (s, "(at)"))
		    && !(proto = keycmp (s, "(a)")))
			return FALSE;
		ld->type = VBI_LINK_EMAIL;
		strcpy (ld->url, "mailto:");
	} else if ((proto = keycmp (s, "www."))) {
		ld->type = VBI_LINK_HTTP;
		strcpy (ld->url, "http://");
	} else if ((proto = keycmp (s, "ftp://"))) {
		ld->type = VBI_LINK_FTP;
	} else if (*s == '@' || *s == SECTION_SIGN) {
		ld->type = VBI_LINK_EMAIL;
		strcpy (ld->url, "mailto:");
		proto = 1;
	} else {
		return FALSE;
	}

	*end = *start + proto;

	{
		const char *s1;
		unsigned int domains;

		s += proto;
		s1 = s;
		domains = 0;

		for (;;) {
			static const char *valid = "%&/=?+-~:;@_"; /* RFC 1738 */
			const char *s1;
			
			s1 = s;

			while (isalnum (*s) || strchr (valid, *s))
				++s;

			if (s == s1)
				return FALSE;

			if (*s != '.')
				break;

			++s;
			++domains;
		}

		if (domains == 0) {
			ld->type = VBI_LINK_NONE;
			return FALSE;
		}

		address = s - s1;
	}

	*end += address;

	if (ld->type == VBI_LINK_EMAIL) {
		static const char *valid = "-~._";
		const char *s1;
		unsigned int recipient;

		s = buf + *start;
		s1 = s;

		while (isalnum (s[-1]) || strchr (valid, s[-1]))
			--s;

		recipient = s1 - s;

		if (recipient == 0) {
			ld->type = VBI_LINK_NONE;
			return FALSE;
		}

		*start -= recipient;

		assert (sizeof (ld->url) > 43);

		strncat (ld->url, s1 - recipient, recipient);
		strcat  (ld->url, "@");
		strncat (ld->url, s1 + proto, address);
	} else {
		assert (sizeof (ld->url) > 43);

		strncat (ld->url, buf + *start, proto + address);
	}

	return TRUE;
}

/*
 *  Analyses page contents and adds link flags when keywords appear:
 *  http and ftp urls, e-mail addresses, 3-digit page
 *  numbers, subpage numbers "n/m", next page ">>".
 */
static __inline__ void
hyperlinks			(struct temp *		t,
				 unsigned int		row)
{
	unsigned char buffer[43]; /* one row, two spaces on the sides and NUL */
	vbi_bool link[43];
	vbi_link ld;
	vbi_char *acp;
	unsigned int i;
	unsigned int j;

	acp = t->pg.text + row * t->pg.columns;

	for (i = j = 0; i < 40; ++i) {
		if (acp[i].size == VBI_OVER_TOP
		    || acp[i].size == VBI_OVER_BOTTOM)
			continue;
		++j;

		if (acp[i].unicode >= 0x20 && acp[i].unicode <= 0xFF)
			buffer[j] = acp[i].unicode;
		else
			buffer[j] = 0x20;
	}

	buffer[0] = ' '; 
	buffer[j + 1] = ' ';
	buffer[j + 2] = 0;

	i = 0;

	while (i < 40) {
		unsigned int end;

		if (keyword (&ld, buffer, t->pg.pgno, t->pg.subno, &i, &end)) {
			for (j = i; j < end; ++j)
				link[j] = TRUE;
		}

		i = end;
	}

	for (i = j = 0; i < 40; ++i) {
		if (acp[i].size == VBI_OVER_TOP
		    || acp[i].size == VBI_OVER_BOTTOM)
			continue;

		acp[i].link = link[j++];
	}
}

/**
 * @param pg With vbi_fetch_vt_page() obtained vbi_page.
 * @param ld Place to store information about the link.
 * @param column Column 0 ... pg->columns - 1 of the character in question.
 * @param row Row 0 ... pg->rows - 1 of the character in question.
 * 
 * A vbi_page (in practice only Teletext pages) may contain hyperlinks
 * such as HTTP URLs, e-mail addresses or links to other pages. Characters
 * being part of a hyperlink have a set vbi_char->link flag, this function
 * returns a more verbose description of the link.
 *
 * @returns
 * TRUE if the link is valid.
 */
vbi_bool
vbi_page_hyperlink		(const vbi_page *	pg,
				 vbi_link *		ld,
				 unsigned int		column,
				 unsigned int		row)
{
	char buffer[43];
	const vbi_char *acp;
	unsigned int i;
	unsigned int j;
	unsigned int start;
	unsigned int end;

	assert (pg != NULL);
	assert (ld != NULL);
	assert (column < pg->columns);
	assert (row < pg->rows);

	ld->nuid = pg->nuid;
	ld->type = VBI_LINK_NONE;

	acp = pg->text + row * pg->columns;

	if (row == 0 || pg->pgno < 0x100 || !acp[column].link)
		return FALSE;

	if (row == 25) {
		unsigned int i;

		i = pg->nav_index[column];

		ld->type	= VBI_LINK_PAGE;
		ld->pgno	= pg->nav_link[i].pgno;
		ld->subno	= pg->nav_link[i].subno;

		return TRUE;
	}

	start = 0;

	for (i = j = 0; i < 40; ++i) {
		if (acp[i].size == VBI_OVER_TOP
		    || acp[i].size == VBI_OVER_BOTTOM)
			continue;
		++j;

		if (i < column && !acp[i].link)
			start = j + 1;

		if (acp[i].unicode >= 0x20 && acp[i].unicode <= 0xFF)
			buffer[j] = acp[i].unicode;
		else
			buffer[j] = 0x20;
	}

	buffer[0] = ' '; 
	buffer[j + 1] = ' ';
	buffer[j + 2] = 0;

	return keyword (ld, buffer, pg->pgno, pg->subno, &start, &end);
}

/* Navigation enhancements ------------------------------------------------- */

#ifndef TELETEXT_NAV_LOG
#define TELETEXT_NAV_LOG 0
#endif

#define nav_log(templ, args...)						\
do { /* Check syntax in any case */					\
	if (TELETEXT_NAV_LOG)						\
		fprintf (stderr, templ , ##args);			\
} while (0)

/*
 *  FLOF navigation
 */

static const vbi_color
flof_link_col [4] = {
	VBI_RED,
	VBI_GREEN,
	VBI_YELLOW,
	VBI_CYAN
};

static __inline__ vbi_char *
navigation_row			(struct temp *		t)
{
	return t->pg.text + 25 * t->pg.columns;
}

static vbi_char *
clear_navigation_bar		(struct temp *		t)
{
	vbi_char ac, *acp;
	unsigned int i;

	acp = navigation_row (t);

	CLEAR (ac);

	ac.foreground	= 32 + VBI_WHITE; /* 32: immutable color */
	ac.background	= 32 + VBI_BLACK;
	ac.opacity	= t->pg.page_opacity[1];
	ac.unicode	= 0x0020;

	for (i = 0; i < t->pg.columns; ++i)
		acp[i] = ac;

	return acp;
}

/*
 *  We have FLOF links but no labels in row 25. This function replaces
 *  row 25 using the FLOF page numbers as labels.
 */
static __inline__ void
flof_navigation_bar		(struct temp *		t)
{
	vbi_char ac, *acp;
	unsigned int i;

	acp = clear_navigation_bar (t);

	ac = *acp;
	ac.link = TRUE;

	for (i = 0; i < 4; ++i) {
		unsigned int pos;
		unsigned int k;

		pos = i * 10 + 3;

		ac.foreground = flof_link_col[i];

		for (k = 0; k < 3; ++k) {
			unsigned int digit;

			digit = t->vtp->data.lop.link[i].pgno;
			digit = (digit >> ((2 - k) * 4)) & 15;

			if (digit > 9)
				ac.unicode = digit + 'A' - 9;
			else
				ac.unicode = digit + '0';

			acp[pos + k] = ac;

			t->pg.nav_index[pos + k] = i;
		}

		t->pg.nav_link[i].pgno = t->vtp->data.lop.link[i].pgno;
		t->pg.nav_link[i].subno = t->vtp->data.lop.link[i].subno;
	}
}

/*
 *  Adds link flags to a page navigation bar (row 25) from FLOF data.
 */
static __inline__ void
flof_links		(struct temp *		t)
{
	vbi_char *acp;
	unsigned int start;
	unsigned int end;
	unsigned int i;
	int color;

	acp = navigation_row (t);

	start = 0;
	color = -1;

	for (i = 0; i <= 40; ++i) {
		if (i == 40 || (acp[i].foreground & 7) != color) {
			unsigned int k;

			for (k = 0; k < 4; ++k)
				if (flof_link_col[k] == (unsigned int) color)
					break;

			if (k < 4 && !NO_PAGE (t->vtp->data.lop.link[k].pgno)) {
				/* Leading and trailing spaces not sensitive */

				for (end = i - 1; end >= start
					     && acp[end].unicode == 0x0020; --end)
					;

				for (; end >= start; --end) {
					acp[end].link = TRUE;
					t->pg.nav_index[end] = k;
				}

		    		t->pg.nav_link[k].pgno = t->vtp->data.lop.link[k].pgno;
		    		t->pg.nav_link[k].subno = t->vtp->data.lop.link[k].subno;
			}

			if (i >= 40)
				break;

			color = acp[i].foreground & 7;
			start = i;
		}

		if (start == i && acp[i].unicode == 0x0020)
			++start;
	}
}

/*
 *  TOP navigation
 */

/*
 *  Finds the TOP page label for pgno and stores in row 25 at
 *  index 0-2 using font and foreground color. ff adds 0-2 '>'
 *  chars. This is a subfunction of top_navigation_bar().
 */
static vbi_bool
top_label			(struct temp *		t,
				 vbi_font_descr *	font,
				 unsigned int		index,
				 vbi_pgno		pgno,
				 vbi_color		foreground,
				 unsigned int		ff)
{
	vbi_char *acp;
	unsigned int column;
	unsigned int i;

	acp = navigation_row (t);

	column = index * 13 + 1;

	for (i = 0; i < 8; ++i) {
		const vt_page *vtp;
		const ait_entry *ait;
		unsigned int j;

		vtp = NULL;

		if (t->pg.vbi->vt.btt_link[i].type != 2)
			goto cont;

		vtp = vbi_cache_get (t->pg.vbi, NUID0,
				     t->pg.vbi->vt.btt_link[i].pgno,
				     t->pg.vbi->vt.btt_link[i].subno, 0x3f7f,
				     /* user access */ FALSE);
		if (!vtp) {
			nav_log ("top ait page %x not cached\n",
				 t->pg.vbi->vt.btt_link[i].pgno);
			goto cont;
		} else if (vtp->function != PAGE_FUNCTION_AIT) {
			nav_log ("no ait page %x\n", vtp->pgno);
			goto cont;
		}

		for (ait = vtp->data.ait, j = 0; j < 46; ++ait, ++j) {
			unsigned int sh;
			int i;

			if (ait->page.pgno != pgno)
				continue;

			t->pg.nav_link[index].pgno = pgno;
			t->pg.nav_link[index].subno = VBI_ANY_SUBNO;

			for (i = 11; i >= 0; --i)
				if (ait->text[i] > 0x20)
					break;

			if (ff && (i <= (int)(11 - ff))) {
				sh = (11 - ff - i) >> 1;

				acp += sh;
				column += sh;

				acp[i + 1].link = TRUE;
				t->pg.nav_index[column + i + 1] = index;

				acp[i + 2].unicode = 0x003E;
				acp[i + 2].foreground = foreground;
				acp[i + 2].link = TRUE;
				t->pg.nav_index[column + i + 2] = index;

				if (ff > 1) {
					acp[i + 3].unicode = 0x003E;
					acp[i + 3].foreground = foreground;
					acp[i + 3].link = TRUE;
					t->pg.nav_index[column + i + 3] = index;
				}
			} else {
				sh = (11 - i) >> 1;

				acp += sh;
				column += sh;
			}

			for (; i >= 0; --i) {
				acp[i].unicode = vbi_teletext_unicode
					(font->G0, font->subset,
					 MAX (ait->text[i], (uint8_t) 0x20));

				acp[i].foreground = foreground;
				acp[i].link = TRUE;
				
				t->pg.nav_index[column + i] = index;
			}

			vbi_cache_unref (t->pg.vbi, vtp);

			return TRUE;
		}
		
	cont:
		vbi_cache_unref (t->pg.vbi, vtp);
	}

	return FALSE;
}

/*
 *  Replaces row 25 by labels and links from TOP data.
 */
static __inline__ void
top_navigation_bar		(struct temp *		t)
{
	vbi_pgno pgno;
	vbi_bool have_group;

	nav_log ("page mip/btt: %d\n",
		 t->pg.vbi->vt.page_info[t->vtp->pgno - 0x100].code);

	clear_navigation_bar (t);

	if (t->pg.page_opacity[1] != VBI_OPAQUE)
		return;

	/* Supposedly TOP bar should be formatted like
	   (prev page) (next group) (next block) (next page)
	   but Zapping already had page buttons in the GUI.
	   Will consider a switch later. */

	/* Item 1, current block/chapter */

	pgno = t->pg.pgno;

	do {
		vbi_page_type type;

		type = t->pg.vbi->vt.page_info[pgno - 0x100].code;

		if (type == VBI_TOP_BLOCK || type == VBI_TOP_GROUP) {
			// XXX font?
			top_label (t, t->pg.font[0], 0, pgno, 32 + VBI_WHITE, 0);
			break;
		}

		pgno = ((pgno - 0x100 - 1) & 0x7FF) + 0x100;
	} while (pgno != t->pg.pgno);

	/* Item 2 & 3, next group and block */

	pgno = t->pg.pgno;
	have_group = FALSE;

	for (;;) {
		pgno = ((pgno - 0x100 + 1) & 0x7FF) + 0x100;

		if (pgno == t->vtp->pgno)
			break;

		// XXX font?

		switch (t->pg.vbi->vt.page_info[pgno - 0x100].code) {
		case VBI_TOP_BLOCK:
			top_label (t, t->pg.font[0], 2,
				   pgno, 32 + VBI_YELLOW, 2);
			return;

		case VBI_TOP_GROUP:
			if (!have_group) {
				top_label (t, t->pg.font[0], 1,
					   pgno, 32 + VBI_GREEN, 1);
				have_group = TRUE;
			}
			break;
		}
	}
}

/*
 *  These functions build an index page (pgno 0x9nn) from TOP
 *  data. Actually we should make the TOP data itself available
 *  and let the client build a menu.
 *
 *  XXX
 */

static const ait_entry *
next_ait(vbi_decoder *vbi, int pgno, int subno, const vt_page **mvtp)
{
	const ait_entry *ait;
	const ait_entry *mait = NULL;
	int mpgno = 0xFFF, msubno = 0xFFFF;
	int i, j;

	*mvtp = NULL;

	for (i = 0; i < 8; i++) {
		const vt_page *vtp, *xvtp = NULL;

		if (vbi->vt.btt_link[i].type != 2)
			continue;

		vtp = vbi_cache_get (vbi, NUID0,
				     vbi->vt.btt_link[i].pgno, 
				     vbi->vt.btt_link[i].subno, 0x3f7f,
				     /* user access */ FALSE);
		if (!vtp) {
			nav_log ("top ait page %x not cached\n",
			       vbi->vt.btt_link[i].pgno);
			continue;
		} else if (vtp->function != PAGE_FUNCTION_AIT) {
			nav_log ("no ait page %x\n", vtp->pgno);
			goto end;
		}

		for (ait = vtp->data.ait, j = 0; j < 46; ait++, j++) {
			if (!ait->page.pgno)
				break;

			if (ait->page.pgno < pgno
			    || (ait->page.pgno == pgno && ait->page.subno <= subno))
				continue;

			if (ait->page.pgno > mpgno
			    || (ait->page.pgno == mpgno && ait->page.subno > msubno))
				continue;

			mait = ait;
			mpgno = ait->page.pgno;
			msubno = ait->page.subno;
			xvtp = vtp;
		}

		if (xvtp) {
			vbi_cache_unref (vbi, *mvtp);
			*mvtp = xvtp;
		} else {
		end:
			vbi_cache_unref (vbi, vtp);
		}
	}

	return mait;
}

static int
top_index(vbi_decoder *vbi, vbi_page *pg, int subno, int columns)
{
	const vt_page *vtp;
	vbi_char ac, *acp;
	const ait_entry *ait;
	int i, j, k, n, lines;
	int xpgno, xsubno;
	const vt_extension *ext;
	char *index_str;
	unsigned int ii;

	pg->vbi = vbi;

	subno = vbi_bcd2dec(subno);

	pg->rows = 25;
	pg->columns = columns;

	pg->dirty.y0 = 0;
	pg->dirty.y1 = 25 - 1;
	pg->dirty.roll = 0;

	ext = &vbi->vt.magazine[0].extension;

	screen_color(pg, 0, 32 + VBI_BLUE);

	vbi_transp_colormap (vbi, pg->color_map,
			     ext->color_map, N_ELEMENTS (pg->color_map));

	pg->drcs_clut = ext->drcs_clut;

	pg->page_opacity[0] = VBI_OPAQUE;
	pg->page_opacity[1] = VBI_OPAQUE;
	pg->boxed_opacity[0] = VBI_OPAQUE;
	pg->boxed_opacity[1] = VBI_OPAQUE;

	CLEAR (pg->drcs);

	CLEAR (ac);

	ac.foreground	= VBI_BLACK; // 32 + VBI_BLACK;
	ac.background	= 32 + VBI_BLUE;
	ac.opacity	= VBI_OPAQUE;
	ac.unicode	= 0x0020;
	ac.size		= VBI_NORMAL_SIZE;

	for (ii = 0; ii < pg->rows * pg->columns; ii++)
		pg->text[ii] = ac;

	ac.size = VBI_DOUBLE_SIZE;

	/* FIXME */
	/* TRANSLATORS: Title of TOP Index page,
	   for now please Latin-1 or ASCII only */
	index_str = _("TOP Index");

	for (i = 0; index_str[i]; i++) {
		ac.unicode = index_str[i];
		pg->text[1 * pg->columns + 2 + i * 2] = ac;
	}

	ac.size = VBI_NORMAL_SIZE;

	acp = &pg->text[4 * pg->columns];
	lines = 17;
	xpgno = 0;
	xsubno = 0;

	vtp = NULL;

	while ((ait = next_ait(vbi, xpgno, xsubno, &vtp))) {
		xpgno = ait->page.pgno;
		xsubno = ait->page.subno;

		/* No docs, correct? */
		character_set_designation(pg->font, ext, vtp);

		if (subno > 0) {
			if (lines-- == 0) {
				subno--;
				lines = 17;
			}

			continue;
		} else if (lines-- <= 0)
			continue;

		for (i = 11; i >= 0; i--)
			if (ait->text[i] > 0x20)
				break;

		switch (vbi->vt.page_info[ait->page.pgno - 0x100].code) {
		case VBI_TOP_GROUP:
			k = 3;
			break;

		default:
    			k = 1;
		}

		for (j = 0; j <= i; j++) {
			acp[k + j].unicode = vbi_teletext_unicode(pg->font[0]->G0,
				pg->font[0]->subset, (ait->text[j] < 0x20) ? 0x20 : ait->text[j]);
		}

		for (k += i + 2; k <= 33; k++)
			acp[k].unicode = '.';

		for (j = 0; j < 3; j++) {
			n = ((ait->page.pgno >> ((2 - j) * 4)) & 15) + '0';

			if (n > '9')
				n += 'A' - '9';

			acp[j + 35].unicode = n;
 		}

		acp += pg->columns;
	}

	if (vtp)
		vbi_cache_unref (vbi, vtp);

	return 1;
}

static inline void
ait_title			(vbi_decoder *		vbi,
				 const vt_page *	vtp,
	  const ait_entry *ait, char *buf)
{
	struct vbi_font_descr *font[2];
	int i;

	character_set_designation(font, &vbi->vt.magazine[0].extension, vtp);

	for (i = 11; i >= 0; i--)
		if (ait->text[i] > 0x20)
			break;
	buf[i + 1] = 0;

	for (; i >= 0; i--) {
		unsigned int unicode = vbi_teletext_unicode(
			font[0]->G0, font[0]->subset,
			(ait->text[i] < 0x20) ?	0x20 : ait->text[i]);

		buf[i] = (unicode >= 0x20 && unicode <= 0xFF) ? unicode : 0x20;
	}
}

/**
 * @param vbi Initialized vbi decoding context.
 * @param pgno Page number, see vbi_pgno.
 * @param subno Subpage number.
 * @param buf Place to store the title, Latin-1 format, at least
 *   41 characters including the terminating zero.
 * 
 * Given a Teletext page number this function tries to deduce a
 * page title for bookmarks or other purposes, mainly from navigation
 * data. (XXX TODO: FLOF)
 * 
 * @return
 * @c TRUE if a title has been found.
 */
vbi_bool
vbi_page_title(vbi_decoder *vbi, int pgno, int subno, char *buf)
{
	const ait_entry *ait;
	int i, j;

	subno = subno;

	if (vbi->vt.top) {
		for (i = 0; i < 8; i++) {
			const vt_page *vtp;

			if (vbi->vt.btt_link[i].type != 2)
				continue;

			vtp = vbi_cache_get (vbi, NUID0,
					     vbi->vt.btt_link[i].pgno, 
					     vbi->vt.btt_link[i].subno, 0x3f7f,
					     /* user access */ FALSE);
			if (!vtp) {
				nav_log ("p/t top ait page %x not cached\n",
				       vbi->vt.btt_link[i].pgno);
				continue;
			}

			if (vtp->function != PAGE_FUNCTION_AIT) {
				nav_log ("p/t no ait page %x\n", vtp->pgno);
				goto end;
			}

			for (ait = vtp->data.ait, j = 0; j < 46; ait++, j++)
				if (ait->page.pgno == pgno) {

					ait_title(vbi, vtp, ait, buf);
					vbi_cache_unref (vbi, vtp);
					return TRUE;
				}
		end:
			vbi_cache_unref (vbi, vtp);
		}
	} else {
		/* find a FLOF link and the corresponding label */
	}

	return FALSE;
}

/*
 *  Navigation (TOP and FLOF) enhancements.
 */
static void
navigation			(struct temp *		t)
{
	if (t->vtp->data.lop.flof) {
		vbi_pgno home_pgno = t->vtp->data.lop.link[5].pgno;

		if (home_pgno >= 0x100 && home_pgno <= 0x899
		    && (home_pgno & 0xFF) != 0xFF) {
			t->pg.nav_link[5].pgno = home_pgno;
			t->pg.nav_link[5].subno = t->vtp->data.lop.link[5].subno;
		}

		if (t->vtp->lop_lines & (1 << 24)) {
			flof_links (t);
		} else {
			flof_navigation_bar (t);
		}
	} else if (t->pg.vbi->vt.top) {
		top_navigation_bar (t);
	}
}

/**
 * @param pg With vbi_fetch_vt_page() obtained vbi_page.
 * @param ld Place to store information about the link.
 * @param index Number 0 ... 5 of the link.
 * 
 * When a vbi_page has been formatted with TOP or FLOF
 * navigation enabled the last row may contain four links
 * to other pages. Apart of being hyperlinks (see
 * vbi_page_hyperlink()) you can also query the links by
 * ordinal number, 0 refering to the leftmost and 3 to
 * the rightmost link displayed.
 *
 * Further all Teletext pages have a built-in home link,
 * by default page 100, but also the magazine intro page
 * or another page selected by the editor. This link has
 * number 5.
 *
 * Other numbers are currently unused.
 *
 * @returns
 * TRUE if the link is valid.
 */
vbi_bool
vbi_page_nav_enum		(const vbi_page *	pg,
				 vbi_link *		ld,
				 unsigned int		index)
{
	assert (pg != NULL);
	assert (ld != NULL);

	if (pg->pgno < 0x100 || index > 5
	    || pg->nav_link[index].pgno < 0x100) {
		ld->type = VBI_LINK_NONE;
		return FALSE;
	}

	ld->type = VBI_LINK_PAGE;
	ld->pgno = pg->nav_link[index].pgno;
	ld->subno = pg->nav_link[index].subno;

	return TRUE;
}

/* ------------------------------------------------------------------------- */

/**
 * @internal
 * @param vbi Initialized vbi_decoder context.
 * @param pg Place to store the formatted page.
 * @param vtp Raw Teletext page. 
 * @param max_level Format the page at this Teletext implementation level.
 * @flags
 * 
 * Format a page @a pg from a raw Teletext page @a vtp. This function is
 * used internally by libzvbi only.
 * 
 * @return
 * @c TRUE if the page could be formatted.
 */
int
vbi_format_vt_page		(vbi_decoder *vbi,
				 vbi_page *xpg,
				 const vt_page *xvtp,
				 vbi_wst_level		max_level,
				 vbi_format_flags	flags)
{
	struct temp t;

	if (xvtp->function != PAGE_FUNCTION_LOP &&
	    xvtp->function != PAGE_FUNCTION_TRIGGER)
		return FALSE;

	fmt_log ("\nFormatting page %03x/%04x pg=%p lev=%d flags=0x%x\n",
		 xvtp->pgno, xvtp->subno, xpg, max_level, flags);

	t.max_level		= max_level;
	t.flags			= flags;

	t.vtp			= xvtp;

	t.pg.vbi		= vbi;

	t.pg.nuid		= vbi->network.ev.network.nuid;
	t.pg.pgno		= t.vtp->pgno;
	t.pg.subno		= t.vtp->subno;

	t.pg.rows		= (flags & VBI_HEADER_ONLY) ? 1 : 25;
	t.pg.columns		= (flags & VBI_41_COLUMNS) ? 40 : 41;

	t.pg.dirty.y0		= 0;
	t.pg.dirty.y1		= t.pg.rows - 1;
	t.pg.dirty.roll		= 0;

	/* Magazine and extension defaults */

	t.mag = (t.max_level <= VBI_WST_LEVEL_1p5) ?
		vbi->vt.magazine /* default magazine */
		: vbi->vt.magazine + (t.vtp->pgno >> 8);

	if (t.vtp->data.lop.ext)
		t.ext = &t.vtp->data.ext_lop.ext;
	else
		t.ext = &t.mag->extension;

	/* Character set designation */

	character_set_designation (t.pg.font, t.ext, t.vtp);

	/* Colors */

	screen_color (&t.pg, t.vtp->flags, t.ext->def_screen_color);

// XXX here?
	vbi_transp_colormap (vbi, t.pg.color_map,
			     t.ext->color_map, N_ELEMENTS (t.pg.color_map));

	t.pg.drcs_clut = t.ext->drcs_clut;

	/* Opacity */

	t.pg.page_opacity[1] =
		(t.vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE | C10_INHIBIT_DISPLAY)) ?
			VBI_TRANSPARENT_SPACE : VBI_OPAQUE;

	t.pg.boxed_opacity[1] =
		(t.vtp->flags & C10_INHIBIT_DISPLAY) ?
			VBI_TRANSPARENT_SPACE : VBI_SEMI_TRANSPARENT;

	if (t.vtp->flags & C7_SUPPRESS_HEADER) {
		t.pg.page_opacity[0] = VBI_TRANSPARENT_SPACE;
		t.pg.boxed_opacity[0] = VBI_TRANSPARENT_SPACE;
	} else {
		t.pg.page_opacity[0] = t.pg.page_opacity[1];
		t.pg.boxed_opacity[0] = t.pg.boxed_opacity[1];
	}

	/* Format level one page */

	level_one_page (&t);

	/* PDC Preselection method "A" links */

	if ((t.flags & (VBI_HEADER_ONLY | VBI_PDC_LINKS)) == VBI_PDC_LINKS)
		pdc_method_a (&t);

	/* Local enhancement data and objects */

	if (t.max_level >= VBI_WST_LEVEL_1p5) {
		vbi_page page;
		vbi_bool success;

		page = t.pg;

		CLEAR (t.pg.drcs);

		if (!(t.vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE))) {
			t.pg.boxed_opacity[0] = VBI_TRANSPARENT_SPACE;
			t.pg.boxed_opacity[1] = VBI_TRANSPARENT_SPACE;
		}

		if (t.vtp->enh_lines & 1) {
			fmt_log ("enhancement packets %08x\n",
				t.vtp->enh_lines);

			success = enhance (&t, LOCAL_ENHANCEMENT_DATA,
					   t.vtp->data.enh_lop.enh,
					   N_ELEMENTS (t.vtp->data.enh_lop.enh),
					   0, 0);
		} else {
			success = default_object_invocation (&t);
		}

		if (success) {
			if (t.max_level >= VBI_WST_LEVEL_2p5)
				post_enhance (&t);
		} else {
			t.pg = page; /* restore level 1 page */
		}
	}

	t.pg.nav_link[5].pgno = vbi->vt.initial_page.pgno;
	t.pg.nav_link[5].subno = vbi->vt.initial_page.subno;

	if (!(t.flags & VBI_HEADER_ONLY)) {
		unsigned int row;

		if (t.flags & VBI_HYPERLINKS)
			for (row = 1; row < 25; ++row)
				hyperlinks (&t, row);

		if (t.flags & VBI_NAVIGATION)
			navigation (&t);
	}

	if (t.flags & VBI_41_COLUMNS)
		column_41 (&t);

	// XXX

	memcpy (xpg, &t.pg, sizeof (*xpg));

	return TRUE;
}

/**
 * @param vbi Initialized vbi_decoder context.
 * @param pg Place to store the formatted page.
 * @param pgno Page number of the page to fetch, see vbi_pgno.
 * @param subno Subpage number to fetch (optional @c VBI_ANY_SUBNO).
 * @param max_level Format the page at this Teletext implementation level.
 * @param display_rows Number of rows to format, between 1 ... 25.
 * @param navigation Analyse the page and add navigation links,
 *   including TOP and FLOF.
 * 
 * Fetches a Teletext page designated by @a pgno and @a subno from the
 * cache, formats and stores it in @a pg. Formatting is limited to row
 * 0 ... @a display_rows - 1 inclusive. The really useful values
 * are 1 (format header only) or 25 (everything). Likewise
 * @a navigation can be used to save unnecessary formatting time.
 * 
 * Although safe to do, this function is not supposed to be called from
 * an event handler since rendering may block decoding for extended
 * periods of time.
 *
 * @return
 * @c FALSE if the page is not cached or could not be formatted
 * for other reasons, for instance is a data page not intended for
 * display. Level 2.5/3.5 pages which could not be formatted e. g.
 * due to referencing data pages not in cache are formatted at a
 * lower level.
 */
vbi_bool
vbi_fetch_vt_page(vbi_decoder *vbi, vbi_page *pg,
		  		vbi_pgno		pgno,
				vbi_subno		subno,
				vbi_wst_level		max_level,
				vbi_format_flags	flags)
{
	const vt_page *vtp;
	struct temp t;
	vbi_bool r;
	int row;

	switch (pgno) {
	case 0x900:
		if (subno == VBI_ANY_SUBNO)
			subno = 0;

		if (!vbi->vt.top || !top_index(vbi, pg, subno,
					       (flags & VBI_41_COLUMNS) ? 41 : 40))
			return FALSE;

		t.pg.nuid = vbi->network.ev.network.nuid;
		t.pg.pgno = 0x900;
		t.pg.subno = subno;

		post_enhance (&t);

		for (row = 1; row < 25; row++)
			hyperlinks (&t, row);

		// XXX
		memcpy (pg, &t.pg, sizeof (*pg));
 
		return TRUE;

	default:
		vtp = vbi_cache_get (vbi, NUID0,
				     pgno, subno, ~0,
				     /* user access */ FALSE);
		if (!vtp)
			return FALSE;

		r = vbi_format_vt_page (vbi, pg, vtp, max_level, flags);

		vbi_cache_unref (vbi, vtp);

		return r;
	}
}
