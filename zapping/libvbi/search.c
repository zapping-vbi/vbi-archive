#include <sys/types.h>	// for freebsd
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

struct search
{
	struct vbi *		vbi;

	struct anchor		start;
	struct anchor		stop;

	int			(* progress)(struct fmt_page *pg);

	struct fmt_page		pg;

	ure_buffer_t		ub;
	ure_dfa_t		ud;
	ucs2_t			haystack[25 * (40 + 1) + 1];
};

#define SEPARATOR 0x000A

static int
search_page(struct search *s, struct vt_page *vtp)
{
	attr_char *acp;
	int row1, row2, this, start, stop;
	ucs2_t *hp, *first, *last;
	unsigned long ms, me;
	int i, j;

	if (vtp->function != PAGE_FUNCTION_LOP)
		return 0; // try next

	this  = (vtp->pgno << 16) + vtp->subno;
	start = (s->start.pgno << 16) + s->start.subno;
	stop  = (s->stop.pgno << 16) + s->stop.subno;

	if (start >= stop) {
		if (this < start)
			return -1; // all done, abort
	} else {
		if (this >= stop)
			return -1; // all done, abort
	}

	if (!fmt_page(1, &s->pg, vtp, 25))
		return -3; // formatting error, abort

	if (s->progress)
		if (!s->progress(&s->pg)) {
			if (this != start) {
				s->start.pgno = vtp->pgno;
				s->start.subno = vtp->subno;
				s->start.row = 1;
				s->start.col = 0;
			}

			return -2; // canceled
		}

	/* To Unicode */

	acp = &s->pg.data[1][0];
	hp = s->haystack;
	first = last = hp;
	row1 = (this == start) ? s->start.row - 1 : -1;
	row2 = (this == stop) ? s->stop.row - 1 : -1;

	for (i = 0; i < 23; i++) {
		for (j = 0; j < 40; acp++, j++) {
			if (i == row1 && j <= s->start.col)
				first = hp;
			if (i == row2 && j <= s->stop.col)
				last = hp + 1;

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

	i = 0; // return: try next page

	if (this == start && this == stop) {
		if (s->start.row > s->stop.row
		    || (s->start.row == s->stop.row
			&& s->start.col >= s->stop.col))
			stop = -1; // -->stop start-->
		else // start-->stop
			if (first >= hp || first >= last)
				return -1; // all done
	}

	if (this == stop) {
		// -->stop
		if (last < hp)
			*last = 0;
		else
			last = hp;
		i = -1; // return: all done
	} else if (this  == start) {
		// start-->
		if (first >= hp)
			return 0; // try next page
		last = hp;
	} else {
		// -->
		last = hp;
	}
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
	if (!ure_exec(s->ud, 0, first, last - first, &ms, &me)) {
		return i;
	}

	/* Highlight */

	acp = &s->pg.data[1][0];
	hp = s->haystack;

	s->start.pgno = vtp->pgno;
	s->start.subno = vtp->subno;

	for (i = 0; i < 23; i++) {
		for (j = 0; j < 40; acp++, j++) {
			int offset = hp - first;
 
			if (offset >= (signed long) me) {
				i = 50;
				break;
			} else if (offset == ms) {
				if (j == 39) {
					s->start.row = i + 2;
					s->start.col = 0;
				} else {
					s->start.row = i + 1;
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

		if ((hp - first) == ms) {
			s->start.row = i + 2;
			s->start.col = 0;
		}

		hp++;
	}

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
 *  Prepare for the search. Start is row 1, column 0,
 *  current page (pgno, subno); Stops at same location (exclusive)
 *  after all pages have been visited.
 *
 *  <progress> shall return FALSE to abort the search,
 *  pg->vtp->pgno etc.; progress can be NULL.
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

	if (subno == ANY_SUB)
		subno = 0;

	s->start.pgno = pgno;
	s->start.subno = subno;
	s->start.row = 1;
	s->start.col = 0;

	// Start can be any page, row and col,
	// stop assumed in row 1, col 0 exclusive.
	s->stop = s->start;

	s->vbi = vbi;
	s->progress = progress;

	return s;
}

/*
 *  Find the next occurance of the pattern.
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
		s->start.pgno, s->start.subno, 1, search_page, s)) {
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

void
hiccup(struct vbi *vbi)
{
	char *pat1 = "por";
	ucs2_t pat2[100];
	static void *s;
	struct fmt_page *pg;
	struct export *exp;
	int i;
 
	for (i = 0; i < strlen(pat1) + 1; i++)
		pat2[i] = pat1[i]; // :-))

	if (!s) {
		s = vbi_new_search(vbi, 0x100, ANY_SUB, pat2, 1, NULL);

		if (!s) {
		    fprintf(stderr, "no search\n");
		    return;
		} else
		    fprintf(stderr, "search\n");
	}

	switch(vbi_next_search(s, &pg)) {
	case 1:
		fprintf(stderr, "found on page %x/%x\n", pg->vtp->pgno, pg->vtp->subno);
		if ((exp = export_open("ansi"))) {
			exp->mod->output(exp, "foo", pg);
			export_close(exp);
		}
		break;

	case 0:
		fprintf(stderr, "not found\n");
		break;

	case -1:
		fprintf(stderr, "canceled\n");
		break;

	case -2:
		fprintf(stderr, "error\n");
		break;
	}

//	vbi_delete_search(s);
}

/*
  struct vbi *vbi = zvbi_get_object();
  extern void hiccup(struct vbi *);
  hiccup(vbi);
  return;
*/
