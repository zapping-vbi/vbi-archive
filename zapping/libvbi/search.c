/*
 *  Zapzilla - Search functions
 *
 *  Copyright (C) 2000 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998,1999 Edgar Toernig (froese@gmx.de)
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

/* $Id: search.c,v 1.3 2001-01-13 23:53:39 mschimek Exp $ */

#include <stdlib.h>
#include "vt.h"
#include "vbi.h"
#include "misc.h"
#include "cache.h"
#include "export.h"
#include "../common/ucs-2.h"
#include "../common/ure.h"

struct anchor {
	int			pgno;
	int			subno;
	int			row;
	int			col;
};

struct search {
	struct vbi *		vbi;

	struct anchor		start;
	struct anchor		stop;

	int			dir;

	int			(* progress)(struct fmt_page *pg);

	struct fmt_page		pg;

	ure_buffer_t		ub;
	ure_dfa_t		ud;
	ucs2_t			haystack[25 * (40 + 1) + 1];
};

#define SEPARATOR 0x000A

#define FIRST_ROW 1
#define LAST_ROW 24

static int
search_page_fwd(struct search *s, struct vt_page *vtp, int wrapped)
{
	attr_char *acp;
	int row, this, start, stop;
	ucs2_t *hp, *first;
	unsigned long ms, me;
	int i, j;

	this  = (vtp->pgno << 16) + vtp->subno;
	start = (s->start.pgno << 16) + s->start.subno;
	stop  = (s->stop.pgno << 16) + s->stop.subno;

	if (start >= stop) {
		if (wrapped && this >= stop)
			return -1; // all done, abort
	} else if (this < start || this >= stop)
		return -1; // all done, abort

	if (vtp->function != PAGE_FUNCTION_LOP)
		return 0; // try next

	if (!fmt_page(1, &s->pg, vtp, 25))
		return -3; // formatting error, abort

	if (s->progress)
		if (!s->progress(&s->pg)) {
			if (this != start) {
				s->start.pgno = vtp->pgno;
				s->start.subno = vtp->subno;
				s->start.row = FIRST_ROW;
				s->start.col = 0;
			}

			return -2; // canceled
		}

	/* To Unicode */

	acp = &s->pg.data[FIRST_ROW][0];
	hp = s->haystack;
	first = hp;
	row = (this == start) ? s->start.row : -1;

	for (i = FIRST_ROW; i < LAST_ROW; i++) {
		for (j = 0; j < 40; acp++, j++) {
			if (i == row && j <= s->start.col)
				first = hp;

			if (!gl_isalnum(acp->glyph))
				continue; /* gfx | drcs, insignificant */

			if (acp->size == DOUBLE_WIDTH
			    || acp->size == DOUBLE_SIZE) {
				// "ZZAAPPZILLA" -> "ZAPZILLA"
				acp++; // skip left half
				j++;
			} else if (acp->size > DOUBLE_SIZE) {
				// *hp++ = 0x0020;
				continue; // ignore lower halves (?)
			}

			*hp++ = glyph2unicode(acp->glyph) ? : 0x0020;
		}

		*hp++ = SEPARATOR;
	}

	/* Search */

	if (first >= hp)
		return 0; // try next page
/*
#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))
fprintf(stderr, "exec: %x/%x; %c%c%c...\n",
	vtp->pgno, vtp->subno,
	printable(first[0]),
	printable(first[1]),
	printable(first[2])
);
*/
	// no REG_NOTBOL | REG_NOTEOL (libc regexp) ?
	if (!ure_exec(s->ud, 0, first, hp - first, &ms, &me))
		return 0; // try next page

	/* Highlight */

	acp = &s->pg.data[FIRST_ROW][0];
	hp = s->haystack;

	s->start.pgno = vtp->pgno;
	s->start.subno = vtp->subno;
	s->start.row = LAST_ROW + 1;
	s->start.col = 0;

	for (i = FIRST_ROW; i < LAST_ROW; i++) {
		for (j = 0; j < 40; acp++, j++) {
			int offset = hp - first;
 
			if (offset >= (signed long) me) {
				if (j == 39) {
					s->start.row = i + 1;
					s->start.col = 0;
				} else {
					s->start.row = i;
					s->start.col = j;
				}

				goto break2;
			}

			if (!gl_isalnum(acp->glyph))
				continue; /* gfx | drcs, insignificant */

			switch (acp->size) {
			case DOUBLE_SIZE:
				if (offset >= (signed long) ms) {
					// XXX black/yellow not good because 3.5 can
					// redefine and it could be confused
					// with transmitted black/yellow. Solution:
					// private 33rd and 34th colour. Same issue
					// TOP.
					acp[40].foreground = BLACK;
					acp[40].background = YELLOW;
					acp[41].foreground = BLACK;
					acp[41].background = YELLOW;
				}

				/* fall through */

			case DOUBLE_WIDTH:
				if (offset >= (signed long) ms) {
					acp[0].foreground = BLACK;
					acp[0].background = YELLOW;
					acp[1].foreground = BLACK;
					acp[1].background = YELLOW;
				}

				hp++;
				acp++;
				j++;

				break;

			case DOUBLE_HEIGHT:
				if (offset >= (signed long) ms) {
					acp[40].foreground = BLACK;
					acp[40].background = YELLOW;
				}

				/* fall through */

			case NORMAL:	
				if (offset >= (signed long) ms) {
					acp[0].foreground = BLACK;
					acp[0].background = YELLOW;
				}

				hp++;
				break;

			default:
				// *hp++ = 0x0020; // ignore lower halves (?)
			}
		}

		hp++;
	}
break2:

	return 1; // success, abort
}

static int
search_page_rev(struct search *s, struct vt_page *vtp, int wrapped)
{
	attr_char *acp;
	int row, this, start, stop;
	unsigned long ms, me;
	ucs2_t *hp;
	int i, j;

	this  = (vtp->pgno << 16) + vtp->subno;
	start = (s->start.pgno << 16) + s->start.subno;
	stop  = (s->stop.pgno << 16) + s->stop.subno;

	if (start <= stop) {
		if (wrapped && this <= stop)
			return -1; // all done, abort
	} else if (this > start || this <= stop)
		return -1; // all done, abort

	if (vtp->function != PAGE_FUNCTION_LOP)
		return 0; // try next page

	if (!fmt_page(1, &s->pg, vtp, 25))
		return -3; // formatting error, abort

	if (s->progress)
		if (!s->progress(&s->pg)) {
			if (this != start) {
				s->start.pgno = vtp->pgno;
				s->start.subno = vtp->subno;
				s->start.row = LAST_ROW + 1;
				s->start.col = 0;
			}

			return -2; // canceled
		}

	/* To Unicode */

	acp = &s->pg.data[FIRST_ROW][0];
	hp = s->haystack;
	row = (this == start) ? s->start.row : -1;

	if (s->start.row <= 0)
		goto break2;

	for (i = FIRST_ROW; i < LAST_ROW; i++) {
		for (j = 0; j < 40; acp++, j++) {
			if (i == row && j >= s->start.col)
				goto break2;

			if (!gl_isalnum(acp->glyph))
				continue; /* gfx | drcs, insignificant */

			if (acp->size == DOUBLE_WIDTH
			    || acp->size == DOUBLE_SIZE) {
				// "ZZAAPPZILLA" -> "ZAPZILLA"
				acp++; // skip left half
				j++;
			} else if (acp->size > DOUBLE_SIZE) {
				// *hp++ = 0x0020;
				continue; // ignore lower halves (?)
			}

			*hp++ = glyph2unicode(acp->glyph) ? : 0x0020;
		}

		*hp++ = SEPARATOR;
	}
break2:

	if (hp <= s->haystack)
		return 0; // try next page

	/* Search */

	me = 0;

	for (i = 0; s->haystack + me < hp; i++) {
		unsigned long ms1, me1;
/*
#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))
fprintf(stderr, "exec: %x/%x; %d, %d; '%c%c%c...'\n",
	vtp->pgno, vtp->subno, i, me,
	printable(s->haystack[me + 0]),
	printable(s->haystack[me + 1]),
	printable(s->haystack[me + 2])
);
*/
		if (!ure_exec(s->ud, 0, s->haystack + me, hp - s->haystack - me, &ms1, &me1))
			break;

		ms = me + ms1;
		me = me + me1;
	}

	if (i == 0)
		return 0; // try next page

	/* Highlight */

	acp = &s->pg.data[FIRST_ROW][0];
	hp = s->haystack;

	s->start.pgno = vtp->pgno;
	s->start.subno = vtp->subno;

	for (i = FIRST_ROW; i < LAST_ROW; i++) {
		for (j = 0; j < 40; acp++, j++) {
			int offset = hp - s->haystack;
 
			if (offset >= (signed long) me)
				goto break2b;

			if (offset < (signed long) ms) {
				if (j == 39) {
					s->start.row = i + 1;
					s->start.col = 0;
				} else {
					s->start.row = i;
					s->start.col = j + 1;
				}
			}

			if (!gl_isalnum(acp->glyph))
				continue; /* gfx | drcs, insignificant */

			switch (acp->size) {
			case DOUBLE_SIZE:
				if (offset >= (signed long) ms) {
					// XXX black/yellow not good because 3.5 can
					// redefine and it could be confused
					// with transmitted black/yellow. Solution:
					// private 33rd and 34th colour. Same issue
					// TOP.
					acp[40].foreground = BLACK;
					acp[40].background = YELLOW;
					acp[41].foreground = BLACK;
					acp[41].background = YELLOW;
				}

				/* fall through */

			case DOUBLE_WIDTH:
				if (offset >= (signed long) ms) {
					acp[0].foreground = BLACK;
					acp[0].background = YELLOW;
					acp[1].foreground = BLACK;
					acp[1].background = YELLOW;
				}

				hp++;
				acp++;
				j++;

				break;

			case DOUBLE_HEIGHT:
				if (offset >= (signed long) ms) {
					acp[40].foreground = BLACK;
					acp[40].background = YELLOW;
				}

				/* fall through */

			case NORMAL:	
				if (offset >= (signed long) ms) {
					acp[0].foreground = BLACK;
					acp[0].background = YELLOW;
				}

				hp++;
				break;

			default:
				// *hp++ = 0x0020; // ignore lower halves (?)
			}
		}

		hp++;
	}
break2b:

	return 1; // success, abort
}

/*
 *  Delete the search context created by vbi_new_search().
 */
void
vbi_delete_search(void *p)
{
	struct search *s = p;

	if (!s)
		return;

	if (s->ud)
		ure_dfa_free(s->ud);

	if (s->ub)
		ure_buffer_free(s->ub);

	free(s);
}

/*
 *  Prepare for the search.
 *
 *  <casefold>
 *    Search case insensitive if TRUE.
 *  <dir>
 *    +1 start on page <pgno>, <subno>, row 1, column 0.
 *       Search forward until all pages have been visited,
 *       stop *before* this page, row 1, column 0.
 *    -1 start on the page immediately preceding <pgno>,
 *       <subno>, in row 24, column 39. Search backwards
 *       until all pages have been visited, stop *at* this
 *       page, row 1, column 0.
 *  <progress> (NULL)
 *    A function called for each page scanned. Return
 *    FALSE to abort the search, <pg> is valid for display
 *    (e.g. pg->vtp->pgno).
 *
 *  Returns an opaque search context pointer or NULL.
 */
void *
vbi_new_search(struct vbi *vbi,
	int pgno, int subno,
	ucs2_t *pattern, int casefold, int dir,
	int (* progress)(struct fmt_page *pg))
{
	struct search *s;

	if (!(s = calloc(1, sizeof(*s))))
		return NULL;

	if (!(s->ub = ure_buffer_create())) {
		vbi_delete_search(s);
		return NULL;
	}

	if (!(s->ud = ure_compile(pattern, ucs2_strlen(pattern), casefold, s->ub))) {
		vbi_delete_search(s);
		return NULL;
	}

	if (subno == ANY_SUB)
		subno = 0;

	s->dir = dir;

	if (dir > 0) {
		s->start.pgno = pgno;
		s->start.subno = subno;
		s->start.row = FIRST_ROW;
		s->start.col = 0;
	} else {
		s->start.pgno = (pgno <= 0x100) ? 0x8FF : (pgno - 1);
		s->start.subno = 0x3F7F;
		s->start.row = LAST_ROW + 1;
		s->start.col = 0;
	}

	s->stop = s->start;

	s->vbi = vbi;
	s->progress = progress;

	return s;
}

/*
 *  Find the next occurance of the pattern.
 *
 *  <p>
 *    Search context.
 *
 *  Return codes:
 *  1	Success. *pg points to the page ready for display
 *      with the pattern highlighted, pg->vtp->pgno etc.
 *  0   Not found, *pg invalid. Another vbi_next_search()
 *      will restart from the original starting point.
 *  -1  Canceled by <progress>. *pg the current page,
 *      same as in success case except highlight. Another
 *      vbi_next_search() continues from this page.
 *  -2  An error occured, condition unclear. Call
 *      vbi_delete_search and forget it.
 */
int
vbi_next_search(void *p, struct fmt_page **pg)
{
	struct search *s = p;

	*pg = NULL;

	switch (s->vbi->cache->op->foreach_pg2(s->vbi->cache,
		s->start.pgno, s->start.subno, s->dir,
		(s->dir > 0) ? search_page_fwd : search_page_rev, s)) {
	case 1:
		*pg = &s->pg;
		return 1;

	case -1:
		s->start = s->stop;
		return 0;

	case -2:
		return -1;

	default:
	}

	return -2;
}
