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

/* $Id: teletext.c,v 1.7.2.2 2003-04-29 17:12:05 mschimek Exp $ */

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

#define DEBUG 0

#if DEBUG
#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? \
                      '.' : ((c) & 0x7F))
#define printv(templ, args...) fprintf(stderr, templ ,##args)
#else
#define printv(templ, args...)
#endif

#define ROWS			25
#define COLUMNS			40
#define EXT_COLUMNS		41
#define LAST_ROW		((ROWS - 1) * EXT_COLUMNS)

struct pdc_date {
	signed			year		: 8;	/* 0 ... 99 */
	signed			month		: 8;	/* 0 ... 11 */
	signed			day		: 8;	/* 0 ... 30 */
};
struct pdc_time {
	signed			hour		: 8;	/* 0 ... 23 */
	signed			min		: 8;	/* 0 ... 59 */
};
struct pdc_pos {
	signed			row		: 8;	/* 1 ... 23 */
	signed			column_begin	: 8;	/* 0 ... 39 */
	signed			column_end	: 8;	/* 0 ... 40 */
};
struct pdc_prog {
	struct pdc_date		ad;			/* Method B: year -1 */
	unsigned		pty		: 8;
	struct pdc_time		at1;			/* Method B: not given */
	struct pdc_time		at2;
	signed			cni		: 32;
	signed			length		: 16;	/* min */
	/* Method A: position of AT-1 and up to three PTLs.
	   Method B: position of PTL, element 1 ... 3 unused. */
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
	vbi_bool		header_only;



	struct pdc_prog		pdc_table[25];

// stuff below not used yet, but it belongs here
// (future private portion of vbi_page)

	struct vbi_font_descr *	font[2];

	uint32_t		double_height_lower;	/* legacy */

	vbi_opacity		page_opacity[2];
	vbi_opacity		boxed_opacity[2];

	/* Navigation */

	struct {
		vbi_pgno		pgno;
		vbi_subno		subno;
	}			link[6];

	int8_t			nav_index[64];

};

/*
 *  FLOF navigation
 */

static const vbi_color
flof_link_col[4] = { VBI_RED, VBI_GREEN, VBI_YELLOW, VBI_CYAN };

static inline void
flof_navigation_bar(vbi_page *pg, const vt_page *vtp)
{
	vbi_char ac;
	int n, i, k, ii;

	CLEAR (ac);

	ac.foreground	= VBI_WHITE;
	ac.background	= VBI_BLACK;
	ac.opacity	= pg->page_opacity[1];
	ac.unicode	= 0x0020;

	for (i = 0; i < EXT_COLUMNS; i++)
		pg->text[LAST_ROW + i] = ac;

	ac.link = TRUE;

	for (i = 0; i < 4; i++) {
		ii = i * 10 + 3;

		for (k = 0; k < 3; k++) {
			n = ((vtp->data.lop.link[i].pgno >> ((2 - k) * 4)) & 15) + '0';

			if (n > '9')
				n += 'A' - '9';

			ac.unicode = n;
			ac.foreground = flof_link_col[i];
			pg->text[LAST_ROW + ii + k] = ac;
			pg->nav_index[ii + k] = i;
		}

		pg->nav_link[i].pgno = vtp->data.lop.link[i].pgno;
		pg->nav_link[i].subno = vtp->data.lop.link[i].subno;
	}
}

static inline void
flof_links(vbi_page *pg, const vt_page *vtp)
{
	vbi_char *acp = pg->text + LAST_ROW;
	int i, j, k, col = -1, start = 0;

	for (i = 0; i < COLUMNS + 1; i++) {
		if (i == COLUMNS || (acp[i].foreground & 7) != col) {
			for (k = 0; k < 4; k++)
				if (flof_link_col[k] == (unsigned int) col)
					break;

			if (k < 4 && !NO_PAGE(vtp->data.lop.link[k].pgno)) {
				/* Leading and trailing spaces not sensitive */

				for (j = i - 1; j >= start && acp[j].unicode == 0x0020; j--);

				for (; j >= start; j--) {
					acp[j].link = TRUE;
					pg->nav_index[j] = k;
				}

		    		pg->nav_link[k].pgno = vtp->data.lop.link[k].pgno;
		    		pg->nav_link[k].subno = vtp->data.lop.link[k].subno;
			}

			if (i >= COLUMNS)
				break;

			col = acp[i].foreground & 7;
			start = i;
		}

		if (start == i && acp[i].unicode == 0x0020)
			start++;
	}
}

/*
 *  TOP navigation
 */

static void		character_set_designation (struct vbi_font_descr **	font,
						   const vt_extension *		ext,
						   const vt_page *		vtp);
static void screen_color(vbi_page *pg, int flags, int color);

static vbi_bool
top_label(vbi_decoder *vbi, vbi_page *pg, struct vbi_font_descr *font,
	  int index, int pgno, int foreground, int ff)
{
	const ait_entry *ait;
	int column = index * 13 + 1;
	vbi_char *acp;
	int i, j;

	acp = &pg->text[LAST_ROW + column];

	for (i = 0; i < 8; i++) {
		const vt_page *vtp;

		if (vbi->vt.btt_link[i].type != 2)
			continue;

		vtp = vbi_cache_get (vbi, NUID0,
				     vbi->vt.btt_link[i].pgno,
				     vbi->vt.btt_link[i].subno, 0x3f7f,
				     /* user access */ FALSE);
		if (!vtp) {
			printv ("top ait page %x not cached\n",
				vbi->vt.btt_link[i].pgno);
			continue;
		} else if (vtp->function != PAGE_FUNCTION_AIT) {
			printv ("no ait page %x\n", vtp->pgno);
			goto end;
		}

		for (ait = vtp->data.ait, j = 0; j < 46; ait++, j++)
			if (ait->page.pgno == pgno) {
				pg->nav_link[index].pgno = pgno;
				pg->nav_link[index].subno = VBI_ANY_SUBNO;

				for (i = 11; i >= 0; i--)
					if (ait->text[i] > 0x20)
						break;

				if (ff && (i <= (11 - ff))) {
					acp += (11 - ff - i) >> 1;
					column += (11 - ff - i) >> 1;

					acp[i + 1].link = TRUE;
					pg->nav_index[column + i + 1] = index;

					acp[i + 2].unicode = 0x003E;
					acp[i + 2].foreground = foreground;
					acp[i + 2].link = TRUE;
					pg->nav_index[column + i + 2] = index;

					if (ff > 1) {
						acp[i + 3].unicode = 0x003E;
						acp[i + 3].foreground = foreground;
						acp[i + 3].link = TRUE;
						pg->nav_index[column + i + 3] = index;
					}
				} else {
					acp += (11 - i) >> 1;
					column += (11 - i) >> 1;
				}

				for (; i >= 0; i--) {
					acp[i].unicode = vbi_teletext_unicode
						(font->G0, font->subset,
						 (ait->text[i] < 0x20) ? 0x20 : ait->text[i]);
					acp[i].foreground = foreground;
					acp[i].link = TRUE;
					pg->nav_index[column + i] = index;
				}

				vbi_cache_unref (vbi, vtp);
				return TRUE;
			}
	end:
		vbi_cache_unref (vbi, vtp);
	}
	
	return FALSE;
}

static inline void
top_navigation_bar(vbi_decoder *vbi, vbi_page *pg,
		   const vt_page *vtp)
{
	vbi_char ac;
	int i, got;

	printv("PAGE MIP/BTT: %d\n", vbi->vt.page_info[vtp->pgno - 0x100].code);

	CLEAR (ac);

	ac.foreground	= 32 + VBI_WHITE;
	ac.background	= 32 + VBI_BLACK;
	ac.opacity	= pg->page_opacity[1];
	ac.unicode	= 0x0020;

	for (i = 0; i < EXT_COLUMNS; i++)
		pg->text[LAST_ROW + i] = ac;

	if (pg->page_opacity[1] != VBI_OPAQUE)
		return;

	for (i = vtp->pgno; i != (int) vtp->pgno + 1; i = (i == 0) ? 0x89a : i - 1)
		if (vbi->vt.page_info[i - 0x100].code == VBI_TOP_BLOCK ||
		    vbi->vt.page_info[i - 0x100].code == VBI_TOP_GROUP) {
			top_label(vbi, pg, pg->font[0], 0, i, 32 + VBI_WHITE, 0);
			break;
		}

	for (i = vtp->pgno + 1, got = FALSE; i != (int) vtp->pgno; i = (i == 0x899) ? 0x100 : i + 1)
		switch (vbi->vt.page_info[i - 0x100].code) {
		case VBI_TOP_BLOCK:
			top_label(vbi, pg, pg->font[0], 2, i, 32 + VBI_YELLOW, 2);
			return;

		case VBI_TOP_GROUP:
			if (!got) {
				top_label(vbi, pg, pg->font[0], 1, i, 32 + VBI_GREEN, 1);
				got = TRUE;
			}

			break;
		}
}

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
			printv("top ait page %x not cached\n",
			       vbi->vt.btt_link[i].pgno);
			continue;
		} else if (vtp->function != PAGE_FUNCTION_AIT) {
			printv("no ait page %x\n", vtp->pgno);
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
top_index(vbi_decoder *vbi, vbi_page *pg, int subno)
{
	const vt_page *vtp;
	vbi_char ac, *acp;
	const ait_entry *ait;
	int i, j, k, n, lines;
	int xpgno, xsubno;
	const vt_extension *ext;
	char *index_str;

	pg->vbi = vbi;

	subno = vbi_bcd2dec(subno);

	pg->rows = ROWS;
	pg->columns = EXT_COLUMNS;

	pg->dirty.y0 = 0;
	pg->dirty.y1 = ROWS - 1;
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

	for (i = 0; i < EXT_COLUMNS * ROWS; i++)
		pg->text[i] = ac;

	ac.size = VBI_DOUBLE_SIZE;

	/* FIXME */
	/* TRANSLATORS: Title of TOP Index page,
	   for now please Latin-1 or ASCII only */
	index_str = _("TOP Index");

	for (i = 0; index_str[i]; i++) {
		ac.unicode = index_str[i];
		pg->text[1 * EXT_COLUMNS + 2 + i * 2] = ac;
	}

	ac.size = VBI_NORMAL_SIZE;

	acp = &pg->text[4 * EXT_COLUMNS];
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

		acp += EXT_COLUMNS;
	}

	if (vtp)
		vbi_cache_unref (vbi, vtp);

	return 1;
}

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

Combined form: <pre><text><pre><text>...<text><post>

+  colour code except magenta
:  magenta
%  conceal
<  end of row

Some observations:

0) Table 4 is wrong: '%' = 0x18; ',' = '+' | '%' | '<'
1) Any control or other code can appear before the prefix, in
   particular :% (as noted in spec) and :: (equal to single :)
   are valid prefixes.
2) Encountered pages which have proper CNI and AD, but no AT-1.
   Here random non-digit characters (usually space 0x20) can
   appear in place of the + before and/or after AT-1. Also
   encountered spaces before and/or after the minus, and a colon
   in place of the period separating hour and minute digits.
3) The % prefix form is permitted only when CNI, AD, PW combine
   (<pre><text><pre><text>...<text><post>), otherwise :% should
   appear. Some stations also use space 0x20 as delimiter between
   combined CNI, AD, PW (<pre><text>0x20<text>...).
4) According to spec only the :% form is valid.
5) Encountered as end time of the last program on a page.
   Note 2) applies accordingly.
6) Postfix :% encountered in combined form ++PTL:%CNI.
*/

#define M_COLOR ((1 << 1) | (1 << 2) | (1 << 3) | (1 << 4) | (1 << 6) | (1 << 7))
#define M_MAGENTA (1 << 0x05)
#define M_CONCEAL (1 << 0x18)

#define iscontrol(c) ((M_COLOR | M_MAGENTA | M_CONCEAL) & (1 << (c)))
#define ismagenta(c) ((c) == 0x05)
#define isconceal(c) ((c) == 0x18)
#define iscolour_conceal(c) ((M_COLOR | M_CONCEAL) & (1 << (c)))
#define ismagenta_conceal(c) ((M_MAGENTA | M_CONCEAL) & (1 << (c)))
#define iscolon(c) ((c) == 0x3A)
#define isperiod(c) ((c) == 0x2E)
#define isminus(c) ((c) == 0x2D)

#ifndef TELETEXT_PDC_LOG
#define TELETEXT_PDC_LOG 0
#endif

#define pdc_log(templ, args...)						\
do { /* Check syntax in any case */					\
	if (TELETEXT_PDC_LOG)						\
		fprintf (stderr, templ , ##args);			\
} while (0)

static vbi_bool
pdc_bcd_time			(struct pdc_time *	t,
				 int			bcd)
{
	int h, m;

	if (!vbi_is_bcd (bcd))
		return FALSE;

	m = (bcd & 15) + ((bcd >> 4) & 15) * 10; bcd >>= 8;
	h = (bcd & 15) +  (bcd >> 4)       * 10;

	if (m >= 60 || h >= 24)
		return FALSE;

	t->hour = h;
	t->min = m;

	return TRUE;
}

static __inline__ int
pdc_time_diff			(struct pdc_time *	begin,
				 struct pdc_time *	end)
{
	int t;

	t = (end->hour - begin->hour) * 60 + end->min - begin->min;

	if (t < 0)
		return t + 24 * 60;
	else
		return t;
}

static __inline__ unsigned int
pdc_ptl				(struct temp *		t,
				 struct pdc_prog *	p,
				 uint8_t *		raw,
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

	/* Skip PTL */

	ctrl2 = vbi_parity (raw[column]);

	for (;;) {
		if (column >= 39) {
			/* Line end postfix */
			column = 40;
			break;
		}

		ctrl1 = ctrl2;
		ctrl2 = vbi_parity (raw[column + 1]);

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
			} else if (ismagenta (ctrl1) && isconceal (ctrl2)) {
				/* Observation 6) */
				column_end = column;
				break;
			}
		} else if (isdigit (ctrl1) && isdigit (ctrl2)
			   && column <= 35 && column_at1 == 40) {
			int separator = vbi_parity (raw[column + 2]);

			if (iscolon (separator) || isperiod (separator)) {
				int digit3 = vbi_parity (raw[column + 3]);
				int digit4 = vbi_parity (raw[column + 4]);
				unsigned int delimiter = (column < 35) ?
					vbi_parity (raw[column + 5]) : 0x20;

				if (isdigit (digit3) && isdigit (digit4)
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
		struct pdc_pos *pp;

		for (pp = p[-1].at1_ptl_pos + 1;
		     pp < p[-1].at1_ptl_pos + 4; ++pp)
			if (pp->row < 0) {
				if (pp >= (p[-1].at1_ptl_pos + 2)
				    && pp[-1].row < (row - 1))
					break; /* probably unrelated */

				pp->row		 = row;
				pp->column_begin = column_begin;
				pp->column_end   = column_end;
				break;
			}
	}

	if (column == 40)
		column = column_at1;

	return column;
}

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

	p1 = t->pdc_table; /* next AT-1 table row */
	p2 = t->pdc_table; /* next AT-2 table row */

	pend = t->pdc_table + N_ELEMENTS (t->pdc_table);

	p2->ad.year = -1;
	p2->cni = -1;

	have_at2 = FALSE;

	pw_sum = 0;
	pw = -1; /* XXX checksum to do */

	for (row = 1; row <= 23; ++row) {
		uint8_t *p = t->vtp->data.lop.raw[row];
		unsigned int column;
		unsigned int column_begin;
		unsigned int value;
		unsigned int digits;
		vbi_bool combine;
		int ctrl0;
		int ctrl1;
		int ctrl2;

		ctrl1 = 0xFF;
		ctrl2 = vbi_parity (p[0]);

		combine = FALSE;

		for (column = 0; column <= 38;) {
			ctrl0 = ctrl1;
			ctrl1 = ctrl2;
			ctrl2 = vbi_parity (p[column + 1]);

			if (0)
				pdc_log ("%d,%d %02x-%02x-%02x %d\n",
					 row, column, ctrl0, ctrl1, ctrl2, combine);

			if ((ctrl0 | ctrl1 | ctrl2) < 0) {
				return 0; /* hamming error */
			} else if (ctrl1 < 0x20 && iscontrol (ctrl1)) {
				if (ctrl0 == ctrl1
				    && iscolour_conceal (ctrl1)
				    && ctrl2 >= 0x20) {
					column = pdc_ptl (t, p1, p, row, column + 1);
					if (column == 0)
						return 0; /* hamming error */
					goto bad;
				} else if (isxdigit (ctrl2)) {
					++column; /* Numeric item */
				} else {
					++column;
					combine = FALSE;
					continue;
				}
			} else if (isxdigit (ctrl1) && ctrl0 >= 0x20) {
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
				int c = vbi_parity (p[column]);

				if (0)
					pdc_log ("%d %02x value=%x digits=%x\n",
						 column, c, value, digits);

				switch (c) {
				case 0x41 ... 0x46: /* xdigit */
					c += 0x3A - 0x41;
					/* fall through */

				case 0x30 ... 0x39: /* digit */
					value = value * 16 + c - 0x30;
					digits += 1;
					break;

				case 0x01 ... 0x07: /* colour */
				case 0x18: /* conceal */
					/* Postfix control code */
					goto eval;

				case 0x20: /* space, observation 2) */
					for (; column <= 38; ++column) {
						c = vbi_parity (p[column + 1]);

						if (c != 0x20)
							break;
					}

					if (digits == 0x104) {
						if (column <= 38 && isminus (c))
							continue; /* zz.zz<spaces> */
						else
							goto eval;
					} else if (digits == 0x204) {
						if (column <= 38 && isdigit (c))
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
				   no AT-2 has as yet been written;". Literally
				   this would forbid two ADs on a page without AT-2s,
				   but such pages have been observed. */

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

			ctrl1 = (column > 0) ? vbi_parity (p[column - 1]) : 0xFF;
			ctrl2 = (column < 40) ? vbi_parity (p[column]) : 0xFF;
			continue;
		bad:
			ctrl1 = (column > 0) ? vbi_parity (p[column - 1]) : 0xFF;
			ctrl2 = (column < 40) ? vbi_parity (p[column]) : 0xFF;
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
					p2[0].length = pdc_time_diff (&p2[0].at1, &pp[0].at1);
					break;
				}
		if (p2[0].length <= 0)
			for (pp = p2 + 1; pp < p1; ++pp)
				if (pp[0].cni == pp[-1].cni) {
					p2[0].length = pdc_time_diff (&p2[0].at1, &pp[0].at1);
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

	return p2 - t->pdc_table;
}

/*
 *  Zapzilla navigation links
 */

static int
keyword(vbi_link *ld, uint8_t *p, int column,
	int pgno, int subno, int *back)
{
	extern int strncasecmp (const char *, const char *, int); // XXX
	uint8_t *s = p + column;
	int i, j, k, l;

	ld->type = VBI_LINK_NONE;
	ld->name[0] = 0;
	ld->url[0] = 0;
	ld->pgno = 0;
	ld->subno = VBI_ANY_SUBNO;
	*back = 0;

	if (isdigit(*s)) {
		for (i = 0; isdigit(s[i]); i++)
			ld->pgno = ld->pgno * 16 + (s[i] & 15);

		if (isdigit(s[-1]) || i > 3)
			return i;

		if (i == 3) {
			if (ld->pgno >= 0x100 && ld->pgno <= 0x899)
				ld->type = VBI_LINK_PAGE;

			return i;
		}

		if (s[i] != '/' && s[i] != ':')
			return i;

		s += i += 1;

		for (ld->subno = j = 0; isdigit(s[j]); j++)
			ld->subno = ld->subno * 16 + (s[j] & 15);

		if (j > 1 || subno != (int) ld->pgno || ld->subno > 0x99)
			return i + j;

		if (ld->pgno == ld->subno)
			ld->subno = 0x01;
		else
			ld->subno = vbi_add_bcd(ld->pgno, 0x01);

		ld->type = VBI_LINK_SUBPAGE;
		ld->pgno = pgno;

		return i + j;
	} else if (!strncasecmp(s, "https://", i = 8)) {
		ld->type = VBI_LINK_HTTP;
	} else if (!strncasecmp(s, "http://", i = 7)) {
		ld->type = VBI_LINK_HTTP;
	} else if (!strncasecmp(s, "www.", i = 4)) {
		ld->type = VBI_LINK_HTTP;
		strcpy(ld->url, "http://");
	} else if (!strncasecmp(s, "ftp://", i = 6)) {
		ld->type = VBI_LINK_FTP;
	} else if (*s == '@' || *s == 0xA7) {
		ld->type = VBI_LINK_EMAIL;
		strcpy(ld->url, "mailto:");
		i = 1;
	} else if (!strncasecmp(s, "(at)", i = 4)) {
		ld->type = VBI_LINK_EMAIL;
		strcpy(ld->url, "mailto:");
	} else if (!strncasecmp(s, "(a)", i = 3)) {
		ld->type = VBI_LINK_EMAIL;
		strcpy(ld->url, "mailto:");
	} else
		return 1;

	for (j = k = l = 0;;) {
		// RFC 1738
		while (isalnum(s[i + j]) || strchr("%&/=?+-~:;@_", s[i + j])) {
			j++;
			l++;
		}

		if (s[i + j] == '.') {
			if (l < 1)
				return i;		
			l = 0;
			j++;
			k++;
		} else
			break;
	}

	if (k < 1 || l < 1) {
		ld->type = VBI_LINK_NONE;
		return i;
	}

	k = 0;

	if (ld->type == VBI_LINK_EMAIL) {
		for (; isalnum(s[k - 1]) || strchr("-~._", s[k - 1]); k--);

		if (k == 0) {
			ld->type = VBI_LINK_NONE;
			return i;
		}

		*back = k;

		strncat(ld->url, s + k, -k);
		strcat(ld->url, "@");
		strncat(ld->url, s + i, j);
	} else
		strncat(ld->url, s + k, i + j - k);

	return i + j;
}

static inline void
zap_links(vbi_page *pg, int row)
{
	unsigned char buffer[43]; /* One row, two spaces on the sides and NUL */
	vbi_link ld;
	vbi_char *acp;
	vbi_bool link[43];
	int i, j, n, b;

	acp = &pg->text[row * EXT_COLUMNS];

	for (i = j = 0; i < COLUMNS; i++) {
		if (acp[i].size == VBI_OVER_TOP || acp[i].size == VBI_OVER_BOTTOM)
			continue;
		buffer[j + 1] = (acp[i].unicode >= 0x20 && acp[i].unicode <= 0xFF) ?
			acp[i].unicode : 0x20;
		j++;
	}

	buffer[0] = ' '; 
	buffer[j + 1] = ' ';
	buffer[j + 2] = 0;

	for (i = 0; i < COLUMNS; i += n) { 
		n = keyword(&ld, buffer, i + 1,
			pg->pgno, pg->subno, &b);

		for (j = b; j < n; j++)
			link[i + j] = (ld.type != VBI_LINK_NONE);
	}

	for (i = j = 0; i < COLUMNS; i++) {
		acp[i].link = link[j];

		if (acp[i].size == VBI_OVER_TOP || acp[i].size == VBI_OVER_BOTTOM)
			continue;
		j++;
	}
}

/**
 * @param pg With vbi_fetch_vt_page() obtained vbi_page.
 * @param column Column 0 ... pg->columns - 1 of the character in question.
 * @param row Row 0 ... pg->rows - 1 of the character in question.
 * @param ld Place to store information about the link.
 * 
 * A vbi_page (in practice only Teletext pages) may contain hyperlinks
 * such as HTTP URLs, e-mail addresses or links to other pages. Characters
 * being part of a hyperlink have a set vbi_char->link flag, this function
 * returns a more verbose description of the link.
 */
void
vbi_resolve_link(vbi_page *pg, int column, int row, vbi_link *ld)
{
	unsigned char buffer[43];
	vbi_char *acp;
	int i, j, b;

	assert(column >= 0 && column < EXT_COLUMNS);

	ld->nuid = pg->nuid;

	acp = &pg->text[row * EXT_COLUMNS];

	if (row == (ROWS - 1) && acp[column].link) {
		i = pg->nav_index[column];

		ld->type = VBI_LINK_PAGE;
		ld->pgno = pg->nav_link[i].pgno;
		ld->subno = pg->nav_link[i].subno;

		return;
	}

	if (row < 1 || row > 23 || column >= COLUMNS || pg->pgno < 0x100) {
		ld->type = VBI_LINK_NONE;
		return;
	}

	for (i = j = b = 0; i < COLUMNS; i++) {
		if (acp[i].size == VBI_OVER_TOP || acp[i].size == VBI_OVER_BOTTOM)
			continue;
		if (i < column && !acp[i].link)
			j = b = -1;

		buffer[j + 1] = (acp[i].unicode >= 0x20 && acp[i].unicode <= 0xFF) ?
			acp[i].unicode : 0x20;

		if (b <= 0) {
			if (buffer[j + 1] == ')' && j > 2) {
				if (!strncasecmp(buffer + j + 1 - 3, "(at", 3))
					b = j - 3;
				else if (!strncasecmp(buffer + j + 1 - 2, "(a", 2))
					b = j - 2;
			} else if (buffer[j + 1] == '@' || buffer[j + 1] == 167)
				b = j;
		}

		j++;
	}

	buffer[0] = ' ';
	buffer[j + 1] = ' ';
	buffer[j + 2] = 0;

	keyword(ld, buffer, 1, pg->pgno, pg->subno, &i);

	if (ld->type == VBI_LINK_NONE)
		keyword(ld, buffer, b + 1, pg->pgno, pg->subno, &i);
}

/**
 * @param pg With vbi_fetch_vt_page() obtained vbi_page.
 * @param ld Place to store information about the link.
 * 
 * All Teletext pages have a built-in home link, by default
 * page 100, but can also be the magazine intro page or another
 * page selected by the editor.
 */
void
vbi_resolve_home(vbi_page *pg, vbi_link *ld)
{
	if (pg->pgno < 0x100) {
		ld->type = VBI_LINK_NONE;
		return;
	}

	ld->type = VBI_LINK_PAGE;
	ld->pgno = pg->nav_link[5].pgno;
	ld->subno = pg->nav_link[5].subno;
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
				printv("p/t top ait page %x not cached\n",
				       vbi->vt.btt_link[i].pgno);
				continue;
			}

			if (vtp->function != PAGE_FUNCTION_AIT) {
				printv("p/t no ait page %x\n", vtp->pgno);
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
 *  Teletext page formatting
 */

static void
character_set_designation	(struct vbi_font_descr **font,
				 const vt_extension *	ext,
				 const vt_page *	vtp)
{
	int i;

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
				 int			flags,
				 int			color)
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

	printv ("obj invocation, source page %03x/%04x, "
		"pointer packet %d triplet %d\n",
		pgno, subno, packet + 1, i);

	if (!(vtp = vbi_cache_get (t->pg.vbi, NUID0, pgno, subno, 0x000F,
				   /* user access */ FALSE))) {
		printv ("... page not cached\n");
		goto failure;
	}

	if (vtp->function == PAGE_FUNCTION_UNKNOWN) {
		if (!vbi_convert_cached_page (t->pg.vbi, &vtp, function)) {
			printv ("... no g/pop page or hamming error\n");
			goto failure;
		}
	} else if (vtp->function == PAGE_FUNCTION_POP) {
		/* vtp->function = function; */ /* POP/GPOP */
	} else if (vtp->function != function) {
		printv ("... source page wrong function %d, expected %d\n",
			vtp->function, function);
		goto failure;
	}

	pointer = vtp->data.pop.pointer[packet * 24 + i * 2 + ((address >> 4) & 1)];

	printv ("... triplet pointer %d\n", pointer);

	if (pointer > 506) {
		printv ("... triplet pointer out of bounds (%d)\n", pointer);
		goto failure;
	}

	if (DEBUG) {
		packet = (pointer / 13) + 3;

		if (packet <= 25) {
			printv ("... object start in packet %d, triplet %d (pointer %d)\n",
				packet, pointer % 13, pointer);
		} else {
			printv ("... object start in packet 26/%d, triplet %d (pointer %d)\n",
				packet - 26, pointer % 13, pointer);	
		}
	}

	*trip = vtp->data.pop.triplet + pointer;
	*remaining = N_ELEMENTS (vtp->data.pop.triplet) - (pointer + 1);

	printv ("... obj def: ad 0x%02x mo 0x%04x dat %d=0x%x\n",
		(*trip)->address, (*trip)->mode, (*trip)->data, (*trip)->data);

	address ^= (*trip)->address << 7;
	address ^= (*trip)->data;

	if ((*trip)->mode != (type + 0x14) || (address & 0x1FF)) {
		printv ("... no object definition\n");
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
		printv ("... page not cached\n");
		goto failure;
	}

	if (vtp->function == PAGE_FUNCTION_UNKNOWN) {
		if (!vbi_convert_cached_page (t->pg.vbi, &vtp, function)) {
			printv ("... no g/drcs page or hamming error\n");
			goto failure;
		}
	} else if (vtp->function == PAGE_FUNCTION_DRCS) {
		/* vtp->function = function; */ /* DRCS/GDRCS */
	} else if (vtp->function != function) {
		printv ("... source page wrong function %d, expected %d\n",
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

	if (row >= ROWS)
		return;

	if (type == OBJ_TYPE_PASSIVE && !mac->unicode) {
		*active_column = column;
		return;
	}

	printv("flush [%04x%c,F%d%c,B%d%c,S%d%c,O%d%c,H%d%c] %d ... %d\n",
	       ac->unicode, mac->unicode ? '*' : ' ',
	       ac->foreground, mac->foreground ? '*' : ' ',
	       ac->background, mac->background ? '*' : ' ',
	       ac->size, mac->size ? '*' : ' ',
	       ac->opacity, mac->opacity ? '*' : ' ',
	       ac->flash, mac->flash ? '*' : ' ',
	       *active_column, column - 1);

	for (i = inv_column + *active_column; i < inv_column + column;) {
		vbi_char c;

		if (i > 39)
			break;

		c = acp[i];

		if (mac->underline) {
			int u = ac->underline;

			if (!mac->unicode)
				ac->unicode = c.unicode;

			if (vbi_is_gfx(ac->unicode)) {
				if (u)
					ac->unicode &= ~0x20; /* separated */
				else
					ac->unicode |= 0x20; /* contiguous */
				mac->unicode = ~0;
				u = 0;
			}

			c.underline = u;
		}
		if (mac->foreground)
			c.foreground = (ac->foreground == VBI_TRANSPARENT_BLACK) ?
				row_color : ac->foreground;
		if (mac->background)
			c.background = (ac->background == VBI_TRANSPARENT_BLACK) ?
				row_color : ac->background;
		if (invert) {
			int t = c.foreground;

			c.foreground = c.background;
			c.background = t;
		}
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

		acp[i] = c;

		if (type == OBJ_TYPE_PASSIVE)
			break;

		i++;

		if (type != OBJ_TYPE_PASSIVE
		    && type != OBJ_TYPE_ADAPTIVE) {
			int raw;

			raw = (row == 0 && i < 9) ?
				0x20 : vbi_parity(t->vtp->data.lop.raw[row][i - 1]);

			/* set-after spacing attributes cancelling non-spacing */

			switch (raw) {
			case 0x00 ... 0x07:	/* alpha + foreground color */
			case 0x10 ... 0x17:	/* mosaic + foreground color */
				printv("... fg term %d %02x\n", i, raw);
				mac->foreground = 0;
				mac->conceal = 0;
				break;

			case 0x08:		/* flash */
				mac->flash = 0;
				break;

			case 0x0A:		/* end box */
			case 0x0B:		/* start box */
				if (i < COLUMNS && vbi_parity(t->vtp->data.lop.raw[row][i]) == raw) {
					printv("... boxed term %d %02x\n", i, raw);
					mac->opacity = 0;
				}

				break;

			case 0x0D:		/* double height */
			case 0x0E:		/* double width */
			case 0x0F:		/* double size */
				printv("... size term %d %02x\n", i, raw);
				mac->size = 0;
				break;
			}

			if (i > 39)
				break;

			raw = (row == 0 && i < 8) ?
				0x20 : vbi_parity(t->vtp->data.lop.raw[row][i]);

			/* set-at spacing attributes cancelling non-spacing */

			switch (raw) {
			case 0x09:		/* steady */
				mac->flash = 0;
				break;

			case 0x0C:		/* normal size */
				printv("... size term %d %02x\n", i, raw);
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
				printv("... bg term %d %02x\n", i, raw);
				mac->background = 0;
				break;
			}
		}
	}

	*active_column = column;
}

#define enhance_flush_row() _enhance_flush_row (t, type,		\
 	&ac, &mac, acp, inv_row, inv_column, 				\
	&active_row, &active_column, row_color, invert)

static __inline__ void
_enhance_flush_row		(struct temp *		t,
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
	int column;

	if (type == OBJ_TYPE_PASSIVE || type == OBJ_TYPE_ADAPTIVE)
		column = *active_column + 1;
	else
		column = COLUMNS;

	_enhance_flush (column,
			t, type, ac, mac, acp,
			inv_row, inv_column,
			active_row, active_column,
			row_color, invert);

	if (type != OBJ_TYPE_PASSIVE)
		CLEAR (*mac);
}

/* FIXME: panels */

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

	active_column = 0;
	active_row = 0;

	acp = &t->pg.text[(inv_row + 0) * EXT_COLUMNS];

	offset_column = 0;
	offset_row = 0;

	row_color =
	next_row_color = t->ext->def_row_color;

	drcs_s1[0] = 0; /* global */
	drcs_s1[1] = 0; /* normal */

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

	font = t->pg.font[0];

	/*
	 *  PDC Preselection method "B"
	 *  ETS 300 231, Section 7.3.2.
	 *
	 *  XXX Should we read
	 *   AT-1 & PTL from the lop?
	 */
	CLEAR (pdc_tmp);

	pdc_tmp.ad.year		= -1; /* not given */
	pdc_tmp.ad.month	= -1;
	pdc_tmp.at1.hour	= -1; /* not given */
	pdc_tmp.at2.hour	= -1;

	p1 = t->pdc_table;

	pdc_hour_mode = 0; /* mode of previous hour triplet */

	for (; n_triplets > 0; p++, n_triplets--) {
		if (p->address >= COLUMNS) {
			/*
			 *  Row address triplets
			 */

			int s = p->data >> 5;
			int row = (p->address - COLUMNS) ? : (ROWS - 1);
			int column = 0;

			if (pdc_hour_mode) {
				/* Invalid. Minute triplet ought to
				   follow hour triplet */
				return FALSE;
			}

			switch (p->mode) {
			case 0x00:		/* full screen color */
				if (t->max_level >= VBI_WST_LEVEL_2p5
				    && s == 0 && type <= OBJ_TYPE_ACTIVE)
					screen_color(&t->pg, t->vtp->flags, p->data & 0x1F);

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
					row_color =
					next_row_color = p->data & 0x1F;
				}

				goto set_active;

			case 0x02:		/* reserved */
			case 0x03:		/* reserved */
				break;

			case 0x04:		/* set active position */
				if (t->max_level >= VBI_WST_LEVEL_2p5) {
					if (p->data >= COLUMNS)
						break; /* reserved */

					column = p->data;
				}

				if (row > active_row)
					row_color = next_row_color;

			set_active:
				if (t->header_only && row > 0) {
					for (;n_triplets>1; p++, n_triplets--)
						if (p[1].address >= COLUMNS) {
							if (p[1].mode == 0x07)
								break;
							else if ((unsigned int) p[1].mode >= 0x1F)
								goto terminate;
						}
					break;
				}

				printv("enh set_active row %d col %d\n", row, column);

				if (row > active_row)
					enhance_flush_row();

				active_row = row;
				active_column = column;

				acp = &t->pg.text[(inv_row + active_row) * EXT_COLUMNS];

				break;

			case 0x05:		/* reserved */
			case 0x06:		/* reserved */
				break;

			case 0x08:		/* PDC data - Country of Origin and Programme Source */
				pdc_tmp.cni = p->address * 256 + p->data;
				break;

			case 0x09:		/* PDC data - Month and Day */
				pdc_tmp.ad.month = (p->address & 15) - 1;
				pdc_tmp.ad.day = (p->data >> 4) * 10 + (p->data & 15) - 1;
				break;

			case 0x0A:		/* PDC data - Cursor Row and Announced Starting Time Hours */
				if (pdc_tmp.ad.month < 0 || pdc_tmp.cni == 0) {
					return FALSE;
				}

				pdc_tmp.at2.hour = ((p->data & 0x30) >> 4) * 10 + (p->data & 15);
				pdc_tmp.length = 0;
				pdc_tmp.at1_ptl_pos[1].row = row;
				pdc_tmp.caf = !!(p->data & 0x40);

				pdc_hour_mode = p->mode;
				break;

			case 0x0B:		/* PDC data - Cursor Row and Announced Finishing Time Hours */
				pdc_tmp.at2.hour = ((p->data & 0x70) >> 4) * 10 + (p->data & 15);

				pdc_hour_mode = p->mode;
				break;

			case 0x0C:		/* PDC data - Cursor Row and Local Time Offset */
				pdc_tmp.lto = p->data;

				if (p->data & 0x40) /* 6 bit two's complement */
					pdc_tmp.lto |= ~0x7F;

				break;

			case 0x0D:		/* PDC data - Series Identifier and Series Code */
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
				offset_row = p->address - COLUMNS;

				printv("enh origin modifier col %+d row %+d\n",
					offset_column, offset_row);

				break;

			case 0x11 ... 0x13:	/* object invocation */
			{
				int source = (p->address >> 3) & 3;
				object_type new_type = p->mode & 3;
				const vt_page *trip_vtp = NULL;
				const vt_triplet *trip;
				int remaining_n_triplets = 0;
				vbi_bool success;

				if (t->max_level < VBI_WST_LEVEL_2p5)
					break;

				printv("enh obj invocation source %d type %d\n", source, new_type);

				if (new_type <= type) { /* 13.2++ */
					printv("... priority violation\n");
					break;
				}

				if (source == 0) /* illegal */
					break;
				else if (source == 1) { /* local */
					int designation = (p->data >> 4) + ((p->address & 1) << 4);
					int triplet = p->data & 15;

					if (type != LOCAL_ENHANCEMENT_DATA || triplet > 12)
						break; /* invalid */

					printv("... local obj %d/%d\n", designation, triplet);

					if (!(t->vtp->enh_lines & 1)) {
						printv("... no packet %d\n", designation);
						return FALSE;
					}

					trip = t->vtp->data.enh_lop.enh + designation * 13 + triplet;
					remaining_n_triplets = N_ELEMENTS (t->vtp->data.enh_lop.enh)
						- (designation * 13 + triplet);
				}
				else /* global / public */
				{
					page_function function;
					int pgno, i = 0;

					if (source == 3) {
						function = PAGE_FUNCTION_GPOP;
						pgno = t->vtp->data.lop.link[24].pgno;

						if (NO_PAGE(pgno)) {
							if (t->max_level < VBI_WST_LEVEL_3p5
							    || NO_PAGE(pgno = t->mag->pop_link[8].pgno))
								pgno = t->mag->pop_link[0].pgno;
						} else {
							printv("... X/27/4 GPOP overrides MOT\n");
						}
					} else {
						function = PAGE_FUNCTION_POP;
						pgno = t->vtp->data.lop.link[25].pgno;

						if (NO_PAGE(pgno)) {
							if ((i = t->mag->pop_lut[t->vtp->pgno & 0xFF]) == 0) {
								printv("... MOT pop_lut empty\n");
								return FALSE; /* has no link (yet) */
							}

							if (t->max_level < VBI_WST_LEVEL_3p5
							    || NO_PAGE(pgno = t->mag->pop_link[i + 8].pgno))
								pgno = t->mag->pop_link[i + 0].pgno;
						} else {
							printv("... X/27/4 POP overrides MOT\n");
						}
					}

					if (NO_PAGE(pgno)) {
						printv("... dead MOT link %d\n", i);
						return FALSE; /* has no link (yet) */
					}

					printv("... %s obj\n", (source == 3) ? "global" : "public");

					trip_vtp = resolve_obj_address (t,
						&trip, &remaining_n_triplets,
						new_type, pgno,
						(p->address << 7) + p->data,
						function);

					if (!trip_vtp)
						return FALSE;
				}

				row = inv_row + active_row;
				column = inv_column + active_column;

				success = enhance (t, new_type, trip,
						   remaining_n_triplets,
						   row + offset_row,
						   column + offset_column);

				vbi_cache_unref (t->pg.vbi, trip_vtp);

				if (!success)
					return FALSE;

				printv ("... object done\n");

				offset_row = 0;
				offset_column = 0;

				break;
			}

			case 0x14:		/* reserved */
				break;

			case 0x15 ... 0x17:	/* object definition */
				enhance_flush_row();
				printv("enh obj definition 0x%02x 0x%02x\n", p->mode, p->data);
				printv("enh terminated\n");
				goto finish;

			case 0x18:		/* drcs mode */
				printv("enh DRCS mode 0x%02x\n", p->data);
				drcs_s1[p->data >> 6] = p->data & 15;
				break;

			case 0x19 ... 0x1E:	/* reserved */
				break;

			case 0x1F:		/* termination marker */
			default:
	                terminate:
				enhance_flush_row();
				printv("enh terminated %02x\n", p->mode);
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
						enhance_flush(column);

					ac.foreground = p->data & 0x1F;
					mac.foreground = ~0;

					printv("enh col %d foreground %d\n", active_column, ac.foreground);
				}

				break;

			case 0x01:		/* G1 block mosaic character */
				if (t->max_level >= VBI_WST_LEVEL_2p5) {
					if (column > active_column)
						enhance_flush(column);

					if (p->data & 0x20) {
						unicode = 0xEE00 + p->data; /* G1 contiguous */
						goto store;
					} else if (p->data >= 0x40) {
						unicode = vbi_teletext_unicode(
							font->G0, NO_SUBSET, p->data);
						goto store;
					}
				}

				break;

			case 0x0B:		/* G3 smooth mosaic or line drawing character */
				if (t->max_level < VBI_WST_LEVEL_2p5)
					break;

				/* fall through */

			case 0x02:		/* G3 smooth mosaic or line drawing character */
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

					printv("enh col %d background %d\n", active_column, ac.background);
				}

				break;

			case 0x04:		/* reserved */
			case 0x05:		/* reserved */
				break;

			case 0x06:		/* PDC data - Cursor Column and Announced Starting */
						/* and Finishing Time Minutes */

				pdc_tmp.at2.min = (p->data >> 4) * 10 + (p->data & 15);

				switch (pdc_hour_mode) {
				case 0x0A: /* Starting time */
					if (p1 > t->pdc_table && p1[-1].length == 0) {
						p1[-1].length = pdc_time_diff (&p1[-1].at2, &pdc_tmp.at2);

						if (p1[-1].length >= 12 * 60) {
							/* nonsense */
							--p1;
						}
					}

					pdc_tmp.at1_ptl_pos[1].column_begin = column;
					pdc_tmp.at1_ptl_pos[1].column_end = 40;

					if (p1 >= t->pdc_table + N_ELEMENTS (t->pdc_table))
						return FALSE;

					*p1++ = pdc_tmp;

					pdc_tmp.pty = 0;

					break;

				case 0x0B: /* Finishing time */
					if (p1 == t->pdc_table)
						return FALSE;

					if (pdc_tmp.at2.hour >= 40) {
						/* Duration */
						p1[-1].length = (pdc_tmp.at2.hour - 40) * 60
							+ pdc_tmp.at2.min;
					} else {
						/* End time */
						p1[-1].length = pdc_time_diff
							(&p1[-1].at2, &pdc_tmp.at2);
					}

					break;

				default:
					/* Hour triplet missing */
					return FALSE;
				}

				pdc_hour_mode = 0;

				break;

			case 0x07:		/* additional flash functions */
				if (t->max_level >= VBI_WST_LEVEL_2p5) {
					if (column > active_column)
						enhance_flush(column);

					/*
					 *  Only one flash function (if any) implemented:
					 *  Mode 1 - Normal flash to background color
					 *  Rate 0 - Slow rate (1 Hz)
					 */
					ac.flash = !!(p->data & 3);
					mac.flash = ~0;

					printv("enh col %d flash 0x%02x\n", active_column, p->data);
				}

				break;

			case 0x08:		/* modified G0 and G2 character set designation */
				if (t->max_level >= VBI_WST_LEVEL_2p5) {
					if (column > active_column)
						enhance_flush(column);

					if (VALID_CHARACTER_SET(p->data))
						font = vbi_font_descriptors + p->data;
					else
						font = t->pg.font[0];

					printv("enh col %d modify character set %d\n", active_column, p->data);
				}

				break;

			case 0x09:		/* G0 character */
				if (t->max_level >= VBI_WST_LEVEL_2p5 && p->data >= 0x20) {
					if (column > active_column)
						enhance_flush(column);

					unicode = vbi_teletext_unicode(font->G0, NO_SUBSET, p->data);
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

				printv("enh col %d display attr 0x%02x\n", active_column, p->data);

				break;

			case 0x0D:		/* drcs character invocation */
			{
				int normal = p->data >> 6;
				int offset = p->data & 0x3F;
				const vt_page *dvtp;
				page_function function;
				int pgno, page, i = 0;

				if (t->max_level < VBI_WST_LEVEL_2p5)
					break;

				if (offset >= 48)
					break; /* invalid */

				if (column > active_column)
					enhance_flush(column);

				page = normal * 16 + drcs_s1[normal];

				printv("enh col %d DRCS %d/0x%02x\n", active_column, page, p->data);

				/* if (!pg->drcs[page]) */ {
					if (!normal) {
						function = PAGE_FUNCTION_GDRCS;
						pgno = t->vtp->data.lop.link[26].pgno;

						if (NO_PAGE(pgno)) {
							if (t->max_level < VBI_WST_LEVEL_3p5
							    || NO_PAGE(pgno = t->mag->drcs_link[8]))
								pgno = t->mag->drcs_link[0];
						} else {
							printv("... X/27/4 GDRCS overrides MOT\n");
						}
					} else {
						function = PAGE_FUNCTION_DRCS;
						pgno = t->vtp->data.lop.link[25].pgno;

						if (NO_PAGE(pgno)) {
							if ((i = t->mag->drcs_lut[t->vtp->pgno & 0xFF]) == 0) {
								printv("... MOT drcs_lut empty\n");
								return FALSE; /* has no link (yet) */
							}

							if (t->max_level < VBI_WST_LEVEL_3p5
							    || NO_PAGE(pgno = t->mag->drcs_link[i + 8]))
								pgno = t->mag->drcs_link[i + 0];
						} else {
							printv("... X/27/4 DRCS overrides MOT\n");
						}
					}

					if (NO_PAGE(pgno)) {
						printv("... dead MOT link %d\n", i);
						return FALSE; /* has no link (yet) */
					}

					printv("... %s drcs from page %03x/%04x\n",
						normal ? "normal" : "global", pgno, drcs_s1[normal]);
					if (!(dvtp = get_drcs_page (t, NUID0,
								    pgno, drcs_s1[normal],
								    function)))
						return FALSE;

					if (dvtp->data.drcs.invalid & (1ULL << offset)) {
						printv("... invalid drcs, prob. tx error\n");
						vbi_cache_unref (t->pg.vbi, dvtp);
						return FALSE;
					}

					t->pg.drcs[page] = dvtp->data.drcs.bits[0];

					vbi_cache_unref (t->pg.vbi, dvtp);
				}

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
				acp = &t->pg.text[row * EXT_COLUMNS];

				proportional = (p->data >> 0) & 1;
				bold = (p->data >> 1) & 1;
				italic = (p->data >> 2) & 1;

				while (row < ROWS && count > 0) {
					for (col = inv_column + column; col < COLUMNS; col++) {
						acp[col].italic = italic;
		    				acp[col].bold = bold;
						acp[col].proportional = proportional;
					}

					acp += EXT_COLUMNS;
					row++;
					count--;
				}

				printv("enh col %d font style 0x%02x\n", active_column, p->data);

				break;
			}

			case 0x0F:		/* G2 character */
				if (p->data >= 0x20) {
					if (column > active_column)
						enhance_flush(column);

					unicode = vbi_teletext_unicode(font->G2, NO_SUBSET, p->data);
					goto store;
				}

				break;

			case 0x10 ... 0x1F:	/* characters including diacritical marks */
				if (p->data >= 0x20) {
					if (column > active_column)
						enhance_flush(column);

					unicode = vbi_teletext_composed_unicode(
						p->mode - 0x10, p->data);
			store:
					printv("enh row %d col %d print 0x%02x/0x%02x -> 0x%04x %c\n",
						active_row, active_column, p->mode, p->data,
						gl, unicode & 0xFF);

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

	if (0) {
		acp = t->pg.text;

		for (active_row = 0; active_row < ROWS; active_row++) {
			printv ("%2d: ", active_row);

			for (active_column = 0; active_column < COLUMNS;
			     acp++, active_column++) {
				printv("%04x ", acp->unicode);
			}

			printv("\n");

			acp += EXT_COLUMNS - COLUMNS;
		}
	}

	return TRUE;
}

static void
post_enhance			(struct temp *		t)
{
	vbi_char ac, *acp;
	unsigned int row;
	unsigned int column;
	unsigned int last_row;

	acp = t->pg.text;

	last_row = t->header_only ? 0 : (ROWS - 2);

	for (row = 0; row <= last_row; row++) {
		for (column = 0; column < COLUMNS; acp++, column++) {
			if (1) {
				printv ("%c", printable(acp->unicode));
			} else {
				printv ("%04xF%dB%dS%dO%d ",
					acp->unicode,
					acp->foreground, acp->background,
					acp->size, acp->opacity);
			}

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
				    && (acp[EXT_COLUMNS].size == VBI_DOUBLE_HEIGHT2
					|| acp[EXT_COLUMNS].size == VBI_DOUBLE_SIZE2)) {
					acp[EXT_COLUMNS].unicode = 0x0020;
					acp[EXT_COLUMNS].size = VBI_NORMAL_SIZE;
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
					acp[EXT_COLUMNS] = ac;
				}

				break;

			case VBI_DOUBLE_SIZE:
				if (row < last_row) {
					ac = acp[0];
					ac.size = VBI_DOUBLE_SIZE2;
					acp[EXT_COLUMNS] = ac;
					ac.size = VBI_OVER_BOTTOM;
					acp[EXT_COLUMNS + 1] = ac;
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

		printv ("\n");

		acp += EXT_COLUMNS - COLUMNS;
	}
}

static __inline__ vbi_bool
default_object_invocation	(struct temp *		t)
{
	const vt_pop_link *pop;
	unsigned int i;
	unsigned int order;

	i = t->mag->pop_lut[t->vtp->pgno & 0xFF];

	if (i == 0)
		return FALSE; /* has no link (yet) */

	pop = t->mag->pop_link + i + 8;

	if (t->max_level < VBI_WST_LEVEL_3p5 || NO_PAGE (pop->pgno)) {
		pop = t->mag->pop_link + i;

		if (NO_PAGE (pop->pgno)) {
			printv ("default object has dead MOT pop link %d\n", i);
			return FALSE;
		}
	}

	order = (pop->default_obj[0].type > pop->default_obj[1].type);

	for (i = 0; i < 2; i++) {
		object_type type;
		const vt_page *trip_vtp;
		const vt_triplet *trip;
		unsigned int n_triplets;
		vbi_bool success;

		type = pop->default_obj[i ^ order].type;

		if (type == OBJ_TYPE_NONE)
			continue;

		printv ("default object #%d invocation, type %d\n", i ^ order, type);

		trip_vtp = resolve_obj_address (t, &trip, &n_triplets,
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

	return TRUE;
}

/**
 * @internal
 * @param vbi Initialized vbi_decoder context.
 * @param pg Place to store the formatted page.
 * @param vtp Raw Teletext page. 
 * @param max_level Format the page at this Teletext implementation level.
 * @param display_rows Number of rows to format, between 1 ... 25.
 * @param navigation Analyse the page and add navigation links,
 *   including TOP and FLOF.
 * 
 * Format a page @a pg from a raw Teletext page @a vtp. This function is
 * used internally by libzvbi only.
 * 
 * @return
 * @c TRUE if the page could be formatted.
 */
int
vbi_format_vt_page(vbi_decoder *vbi,
		   vbi_page *xpg, const vt_page *xvtp,
		   vbi_wst_level max_level,
		   int display_rows, vbi_bool navigation)
{
	char buf[16];
	struct temp t;
	unsigned int column;
	unsigned int row;
	unsigned int i;

	if (xvtp->function != PAGE_FUNCTION_LOP &&
	    xvtp->function != PAGE_FUNCTION_TRIGGER)
		return FALSE;

	printv("\nFormatting page %03x/%04x pg=%p lev=%d rows=%d nav=%d\n",
	       vtp->pgno, vtp->subno, xpg, max_level, display_rows, navigation);

	display_rows = SATURATE(display_rows, 1, ROWS);

	t.max_level = max_level;
	t.header_only = (display_rows == 1);
	t.vtp = xvtp;

	t.pg.vbi = vbi;

	t.pg.nuid = vbi->network.ev.network.nuid;
	t.pg.pgno = t.vtp->pgno;
	t.pg.subno = t.vtp->subno;

	t.pg.rows = display_rows;
	t.pg.columns = EXT_COLUMNS;

	t.pg.dirty.y0 = 0;
	t.pg.dirty.y1 = ROWS - 1;
	t.pg.dirty.roll = 0;

	t.mag = (t.max_level <= VBI_WST_LEVEL_1p5) ?
		vbi->vt.magazine : vbi->vt.magazine + (t.vtp->pgno >> 8);

	if (t.vtp->data.lop.ext)
		t.ext = &t.vtp->data.ext_lop.ext;
	else
		t.ext = &t.mag->extension;

	/* Character set designation */

	character_set_designation(t.pg.font, t.ext, t.vtp);

	/* Colors */

	screen_color(&t.pg, t.vtp->flags, t.ext->def_screen_color);

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

	/* DRCS */

	CLEAR (t.pg.drcs);

	/* Current page number in header */

	sprintf (buf, "\2%x.%02x\7", t.vtp->pgno, t.vtp->subno & 0xff);

	/* Level 1 formatting */

	i = 0;
	t.pg.double_height_lower = 0;

	for (row = 0; row < display_rows; row++) {
		struct vbi_font_descr *font;
		unsigned int mosaic_unicodes; /* 0xEE00 separate, 0xEE20 contiguous */
		unsigned int held_mosaic_unicode;
		vbi_bool hold;
		vbi_bool mosaic;
		vbi_bool double_height;
		vbi_bool wide_char;
		vbi_char ac, *acp;

		acp = &t.pg.text[row * EXT_COLUMNS];

		held_mosaic_unicode = 0xEE20; /* G1 block mosaic, blank, contiguous */

		CLEAR (ac);

		ac.unicode	= 0x0020;
		ac.foreground	= t.ext->foreground_clut + VBI_WHITE;
		ac.background	= t.ext->background_clut + VBI_BLACK;
		mosaic_unicodes	= 0xEE20; /* contiguous */
		ac.opacity	= t.pg.page_opacity[row > 0];
		font		= t.pg.font[0];
		hold		= FALSE;
		mosaic		= FALSE;

		double_height	= FALSE;
		wide_char	= FALSE;

		acp[COLUMNS] = ac; /* artificial column 41 */

		for (column = 0; column < COLUMNS; column++) {
			int raw;

			if (row == 0 && column < 8) {
				raw = buf[column];
				i++;
			} else if ((raw = vbi_parity (t.vtp->data.lop.raw[0][i++])) < 0)
				raw = ' ';

			/* set-at spacing attributes */

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
				mosaic_unicodes = 0xEE20;
				break;

			case 0x1A:		/* separated mosaics */
				mosaic_unicodes = 0xEE00;
				break;

			case 0x1C:		/* black background */
				ac.background = t.ext->background_clut + VBI_BLACK;
				break;

			case 0x1D:		/* new background */
				ac.background = t.ext->background_clut + (ac.foreground & 7);
				break;

			case 0x1E:		/* hold mosaic */
				hold = TRUE;
				break;

			default:
				break;
			}

			if (raw <= 0x1F) {
				ac.unicode = (hold & mosaic) ? held_mosaic_unicode : 0x0020;
			} else {
				if (mosaic && (raw & 0x20)) {
					held_mosaic_unicode = mosaic_unicodes + raw - 0x20;
					ac.unicode = held_mosaic_unicode;
				} else {
					ac.unicode = vbi_teletext_unicode
						(font->G0, font->subset, raw);
				}
			}

			if (!wide_char) {
				acp[column] = ac;

				wide_char = /*!!*/(ac.size & VBI_DOUBLE_WIDTH);

				if (wide_char && column < (COLUMNS - 1)) {
					acp[column + 1] = ac;
					acp[column + 1].size = VBI_OVER_TOP;
				}
			}

			/* set-after spacing attributes */

			switch (raw) {
			case 0x00 ... 0x07:	/* alpha + foreground color */
				ac.foreground = t.ext->foreground_clut + (raw & 7);
				ac.conceal = FALSE;
				mosaic = FALSE;
				break;

			case 0x08:		/* flash */
				ac.flash = TRUE;
				break;

			case 0x0A:		/* end box */
				if (column < (COLUMNS - 1)
				    && vbi_parity (t.vtp->data.lop.raw[0][i]) == 0x0A)
					ac.opacity = t.pg.page_opacity[row > 0];
				break;

			case 0x0B:		/* start box */
				if (column < (COLUMNS - 1)
				    && vbi_parity(t.vtp->data.lop.raw[0][i]) == 0x0B)
					ac.opacity = t.pg.boxed_opacity[row > 0];
				break;

			case 0x0D:		/* double height */
				if (row == 0 || row >= 23)
					break;
				ac.size = VBI_DOUBLE_HEIGHT;
				double_height = TRUE;
				break;

			case 0x0E:		/* double width */
				printv ("spacing col %d row %d double width\n", column, row);
				if (column < (COLUMNS - 1))
					ac.size = VBI_DOUBLE_WIDTH;
				break;

			case 0x0F:		/* double size */
				printv ("spacing col %d row %d double size\n", column, row);
				if (column >= (COLUMNS - 1) || row == 0 || row >= 23)
					break;
				ac.size = VBI_DOUBLE_SIZE;
				double_height = TRUE;
				break;

			case 0x10 ... 0x17:	/* mosaic + foreground color */
				ac.foreground = t.ext->foreground_clut + (raw & 7);
				ac.conceal = FALSE;
				mosaic = TRUE;
				break;

			case 0x1B:		/* ESC */
				font = (font == t.pg.font[0]) ?
					t.pg.font[1] : t.pg.font[0];
				break;

			case 0x1F:		/* release mosaic */
				hold = FALSE;
				break;
			}
		}

		if (double_height) {
			for (column = 0; column < EXT_COLUMNS; column++) {
				ac = acp[column];

				switch (ac.size) {
				case VBI_DOUBLE_HEIGHT:
					ac.size = VBI_DOUBLE_HEIGHT2;
					acp[EXT_COLUMNS + column] = ac;
					break;
		
				case VBI_DOUBLE_SIZE:
					ac.size = VBI_DOUBLE_SIZE2;
					acp[EXT_COLUMNS + column] = ac;
					ac.size = VBI_OVER_BOTTOM;
					acp[EXT_COLUMNS + (++column)] = ac;
					break;

				default: /* NORMAL, DOUBLE_WIDTH, OVER_TOP */
					ac.size = VBI_NORMAL_SIZE;
					ac.unicode = 0x0020;
					acp[EXT_COLUMNS + column] = ac;
					break;
				}
			}

			i += COLUMNS;
			row++;

			t.pg.double_height_lower |= 1 << row;
		}
	}

	if (0) {
		if (row < ROWS) {
			vbi_char ac;

			CLEAR (ac);

			ac.foreground	= t.ext->foreground_clut + VBI_WHITE;
			ac.background	= t.ext->background_clut + VBI_BLACK;
			ac.opacity	= t.pg.page_opacity[1];
			ac.unicode	= 0x0020;

			for (i = row * EXT_COLUMNS; i < ROWS * EXT_COLUMNS; i++)
				t.pg.text[i] = ac;
		}
	}

	/* PDC Preselection method "A" links */

	if (0 && !t.header_only)
		pdc_method_a (&t);

	/* Local enhancement data and objects */

	if (t.max_level >= VBI_WST_LEVEL_1p5 && !t.header_only) {
		vbi_page page;
		vbi_bool success;

		page = t.pg;

		if (!(t.vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE))) {
			t.pg.boxed_opacity[0] = VBI_TRANSPARENT_SPACE;
			t.pg.boxed_opacity[1] = VBI_TRANSPARENT_SPACE;
		}

		if (t.vtp->enh_lines & 1) {
			printv ("enhancement packets %08x\n", t.vtp->enh_lines);

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

	/* Navigation enhancements */

	if (navigation) {
		t.pg.nav_link[5].pgno = vbi->vt.initial_page.pgno;
		t.pg.nav_link[5].subno = vbi->vt.initial_page.subno;

		for (row = 1; row < MIN(ROWS - 1, display_rows); row++)
			zap_links(&t.pg, row);

		if (display_rows >= ROWS) {
			if (t.vtp->data.lop.flof) {
				if (t.vtp->data.lop.link[5].pgno >= 0x100
				    && t.vtp->data.lop.link[5].pgno <= 0x899
				    && (t.vtp->data.lop.link[5].pgno & 0xFF) != 0xFF) {
					t.pg.nav_link[5].pgno = t.vtp->data.lop.link[5].pgno;
					t.pg.nav_link[5].subno = t.vtp->data.lop.link[5].subno;
				}

				if (t.vtp->lop_lines & (1 << 24))
					flof_links(&t.pg, t.vtp);
				else
					flof_navigation_bar(&t.pg, t.vtp);
			} else if (vbi->vt.top) {
				top_navigation_bar(vbi, &t.pg, t.vtp);
			}

		}
	}

	if (0) {
		vbi_char *acp;

		for (row = ROWS - 1, acp = t.pg.text + EXT_COLUMNS * row; row < ROWS; row++) {
			fprintf(stderr, "%2d: ", row);

			for (column = 0; column < COLUMNS; acp++, column++) {
				fprintf(stderr, "%04x ", acp->unicode);
			}

			fprintf(stderr, "\n");

			acp += EXT_COLUMNS - COLUMNS;
		}
	}

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
		  vbi_pgno pgno, vbi_subno subno,
		  vbi_wst_level max_level,
		  int display_rows, vbi_bool navigation)
{
	const vt_page *vtp;
	struct temp t;
	vbi_bool r;
	int row;

	switch (pgno) {
	case 0x900:
		if (subno == VBI_ANY_SUBNO)
			subno = 0;

		if (!vbi->vt.top || !top_index(vbi, pg, subno))
			return FALSE;

		t.pg.nuid = vbi->network.ev.network.nuid;
		t.pg.pgno = 0x900;
		t.pg.subno = subno;

		post_enhance (&t);

		for (row = 1; row < ROWS; row++)
			zap_links(&t.pg, row);

		// XXX
		memcpy (pg, &t.pg, sizeof (*pg));
 
		return TRUE;

	default:
		vtp = vbi_cache_get (vbi, NUID0,
				     pgno, subno, ~0,
				     /* user access */ FALSE);
		if (!vtp)
			return FALSE;

		r = vbi_format_vt_page (vbi, pg, vtp, max_level, display_rows, navigation);

		vbi_cache_unref (vbi, vtp);

		return r;
	}
}
