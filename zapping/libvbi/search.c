/*
 *  Zapzilla - Search functions
 *
 *  Copyright (C) 2000-2001 Michael H. Schimek
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

/* $Id: search.c,v 1.13 2001-03-28 07:48:15 mschimek Exp $ */

#include <stdlib.h>
#include <ctype.h>

#include "vt.h"
#include "vbi.h"
#include "lang.h"
#include "cache.h"

#include "../common/ucs-2.h"
#include "../common/ure.h"

struct search {
	struct vbi *		vbi;

	int			start_pgno;
	int			start_subno;
	int			stop_pgno[2];
	int			stop_subno[2];
	int			row[2], col[2];

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

static void
highlight(struct search *s, struct vt_page *vtp,
	  ucs2_t *first, long ms, long me)
{
	attr_char *acp;
	ucs2_t *hp;
	int i, j;

	acp = &s->pg.text[FIRST_ROW * s->pg.columns + 0];
	hp = s->haystack;

	s->start_pgno = vtp->pgno;
	s->start_subno = vtp->subno;
	s->row[0] = LAST_ROW + 1;
	s->col[0] = 0;

	for (i = FIRST_ROW; i < LAST_ROW; i++) {
		for (j = 0; j < 40; acp++, j++) {
			int offset = hp - first;
 
			if (offset >= me) {
				s->row[0] = i;
				s->col[0] = j;
				return;
			}

			if (offset < ms) {
				if (j == 39) {
					s->row[1] = i + 1;
					s->col[1] = 0;
				} else {
					s->row[1] = i;
					s->col[1] = j + 1;
				}
			}

			if (!gl_isalnum(acp->glyph))
				continue; /* gfx | drcs, insignificant */

			switch (acp->size) {
			case DOUBLE_SIZE:
				if (offset >= ms) {
					acp[40].foreground = 32 + BLACK;
					acp[40].background = 32 + YELLOW;
					acp[41].foreground = 32 + BLACK;
					acp[41].background = 32 + YELLOW;
				}

				/* fall through */

			case DOUBLE_WIDTH:
				if (offset >= ms) {
					acp[0].foreground = 32 + BLACK;
					acp[0].background = 32 + YELLOW;
					acp[1].foreground = 32 + BLACK;
					acp[1].background = 32 + YELLOW;
				}

				hp++;
				acp++;
				j++;

				break;

			case DOUBLE_HEIGHT:
				if (offset >= ms) {
					acp[40].foreground = 32 + BLACK;
					acp[40].background = 32 + YELLOW;
				}

				/* fall through */

			case NORMAL:	
				if (offset >= ms) {
					acp[0].foreground = 32 + BLACK;
					acp[0].background = 32 + YELLOW;
				}

				hp++;
				break;

			default:
				// *hp++ = 0x0020; // ignore lower halves (?)
			}
		}

		hp++;
	}
}

static int
search_page_fwd(struct search *s, struct vt_page *vtp, int wrapped)
{
	attr_char *acp;
	int row, this, start, stop;
	ucs2_t *hp, *first;
	unsigned long ms, me;
	int flags, i, j;

	this  = (vtp->pgno << 16) + vtp->subno;
	start = (s->start_pgno << 16) + s->start_subno;
	stop  = (s->stop_pgno[0] << 16) + s->stop_subno[0];

	if (start >= stop) {
		if (wrapped && this >= stop)
			return -1; // all done, abort
	} else if (this < start || this >= stop)
		return -1; // all done, abort

	if (vtp->function != PAGE_FUNCTION_LOP)
		return 0; // try next

	if (!vbi_format_page(s->vbi, &s->pg, vtp, s->vbi->vt.max_level, 25, 1))
		return -3; // formatting error, abort

	if (s->progress)
		if (!s->progress(&s->pg)) {
			if (this != start) {
				s->start_pgno = vtp->pgno;
				s->start_subno = vtp->subno;
				s->row[0] = FIRST_ROW;
				s->row[1] = LAST_ROW + 1;
				s->col[0] = s->col[1] = 0;
			}

			return -2; // canceled
		}

	/* To Unicode */

	acp = &s->pg.text[FIRST_ROW * s->pg.columns + 0];
	hp = s->haystack;
	first = hp;
	row = (this == start) ? s->row[0] : -1;
	flags = 0;

	if (row > LAST_ROW)
		return 0; // try next page

	for (i = FIRST_ROW; i < LAST_ROW; i++) {
		for (j = 0; j < 40; acp++, j++) {
			if (i == row && j <= s->col[0])
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
			flags = URE_NOTBOL;
		}

		*hp++ = SEPARATOR;
		flags = 0;
	}

	/* Search */

	if (first >= hp)
		return 0; // try next page
/*
#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))
fprintf(stderr, "exec: %x/%x; start %d,%d; %c%c%c...\n",
	vtp->pgno, vtp->subno,
	s->row[0], s->col[0],
	printable(first[0]),
	printable(first[1]),
	printable(first[2])
);
*/
	if (!ure_exec(s->ud, flags, first, hp - first, &ms, &me))
		return 0; // try next page

	highlight(s, vtp, first, ms, me);

	return 1; // success, abort
}

static int
search_page_rev(struct search *s, struct vt_page *vtp, int wrapped)
{
	attr_char *acp;
	int row, this, start, stop;
	unsigned long ms, me;
	ucs2_t *hp;
	int flags, i, j;

	this  = (vtp->pgno << 16) + vtp->subno;
	start = (s->start_pgno << 16) + s->start_subno;
	stop  = (s->stop_pgno[1] << 16) + s->stop_subno[1];

	if (start <= stop) {
		if (wrapped && this <= stop)
			return -1; // all done, abort
	} else if (this > start || this <= stop)
		return -1; // all done, abort

	if (vtp->function != PAGE_FUNCTION_LOP)
		return 0; // try next page

	if (!vbi_format_page(s->vbi, &s->pg, vtp, s->vbi->vt.max_level, 25, 1))
		return -3; // formatting error, abort

	if (s->progress)
		if (!s->progress(&s->pg)) {
			if (this != start) {
				s->start_pgno = vtp->pgno;
				s->start_subno = vtp->subno;
				s->row[0] = FIRST_ROW;
				s->row[1] = LAST_ROW + 1;
				s->col[0] = s->col[1] = 0;
			}

			return -2; // canceled
		}

	/* To Unicode */

	acp = &s->pg.text[FIRST_ROW * s->pg.columns + 0];
	hp = s->haystack;
	row = (this == start) ? s->row[1] : 100;
	flags = 0;

	if (row < FIRST_ROW)
		goto break2;

	for (i = FIRST_ROW; i < LAST_ROW; i++) {
		for (j = 0; j < 40; acp++, j++) {
			if (i == row && j >= s->col[1])
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
			flags = URE_NOTEOL;
		}

		*hp++ = SEPARATOR;
		flags = 0;
	}
break2:

	if (hp <= s->haystack)
		return 0; // try next page

	/* Search */

	ms = me = 0;

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
		if (!ure_exec(s->ud, (me > 0) ? (flags | URE_NOTBOL) : flags,
		    s->haystack + me, hp - s->haystack - me, &ms1, &me1))
			break;

		ms = me + ms1;
		me = me + me1;
	}

	if (i == 0)
		return 0; // try next page

	highlight(s, vtp, s->haystack, ms, me);

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
 *  <pgno>, <subno>
 *    The first (forward) or last (backward) page to visit.
 *  <casefold>
 *    Search case insensitive if TRUE.
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
	ucs2_t *pattern, int casefold,
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

	s->stop_pgno[0] = pgno;
	s->stop_subno[0] = (subno == ANY_SUB) ? 0 : subno;

	if (subno <= 0) {
		s->stop_pgno[1] = (pgno <= 0x100) ? 0x8FF : pgno - 1;
		s->stop_subno[1] = 0x3F7E;
	} else {
		s->stop_pgno[1] = pgno;

		if ((subno & 0x7F) == 0)
			s->stop_subno[1] = (subno - 0x100) | 0x7E;
		else
			s->stop_subno[1] = subno - 1;
	}

	s->vbi = vbi;
	s->progress = progress;

	return s;
}

/*
 *  Find the next occurance of the pattern.
 *
 *  <p>
 *    Search context.
 *  <dir>
 *    +1 forward
 *    -1 backward
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
vbi_next_search(void *p, struct fmt_page **pg, int dir)
{
	struct search *s = p;

	*pg = NULL;
	dir = (dir > 0) ? +1 : -1;

	if (!s->dir) {
		s->dir = dir;

		if (dir > 0) {
			s->start_pgno = s->stop_pgno[0];
			s->start_subno = s->stop_subno[0];
		} else {
			s->start_pgno = s->stop_pgno[1];
			s->start_subno = s->stop_subno[1];
		}

		s->row[0] = FIRST_ROW;
		s->row[1] = LAST_ROW + 1;
		s->col[0] = s->col[1] = 0;
	}
#if 1 /* should switch to a 'two frontiers meet' model, but ok for now */
	else if (dir != s->dir) {
		s->dir = dir;

		s->stop_pgno[0] = s->start_pgno;
		s->stop_subno[0] = (s->start_subno == ANY_SUB) ? 0 : s->start_subno;
		s->stop_pgno[1] = s->start_pgno;
		s->stop_subno[1] = s->start_subno;
	}
#endif
	switch (s->vbi->cache->op->foreach_pg2(s->vbi->cache,
		s->start_pgno, s->start_subno, dir,
		(dir > 0) ? search_page_fwd : search_page_rev, s)) {
	case 1:
		*pg = &s->pg;
		return 1;

	case -1:
		s->dir = 0;
		return 0;

	case -2:
		return -1;

	default:
	}

	return -2;
}
