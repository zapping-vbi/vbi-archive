/*
 *  libzvbi - Teletext page cache search functions
 *
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
 *  Copyright (C) 2000, 2001 Iñaki G. Etxebarria
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

/* $Id: search.c,v 1.6.2.11 2006-05-18 16:49:20 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "misc.h"
#include "search.h"
#include "ure.h"
#include "conv.h"
#include "page-priv.h"

/**
 * @addtogroup Search
 * @ingroup Cache
 * @brief Search the Teletext page cache.
 */

#if defined(HAVE_GLIBC21) || defined(HAVE_LIBUNICODE)

struct _vbi3_search {
	vbi3_cache *		cache;

	cache_network *		network;

	vbi3_pgno		start_pgno;
	vbi3_subno		start_subno;
	vbi3_pgno		stop_pgno[2];
	vbi3_subno		stop_subno[2];
	int			row[2];
	int			col[2];

	int			dir;

	vbi3_search_progress_cb *progress;
	void *			user_data;

	vbi3_page_priv		pgp;

	va_list			format_options;

	ure_buffer_t		ub;
	ure_dfa_t		ud;

	ucs2_t			haystack[25 * (40 + 1) + 1];
};

#define SEPARATOR 0x000A

#define FIRST_ROW 1
#define LAST_ROW 24

static void
highlight			(vbi3_search *		s,
				 const cache_page *	cp,
				 ucs2_t *		first,
				 long			ms,
				 long			me)
{
	vbi3_page *pg;
	ucs2_t *hp;
	int i, j;

	pg = &s->pgp.pg;
	hp = s->haystack;

	s->start_pgno = cp->pgno;
	s->start_subno = cp->subno;
	s->row[0] = LAST_ROW + 1;
	s->col[0] = 0;

	for (i = FIRST_ROW; i < LAST_ROW; i++) {
		vbi3_char *cp;

		cp = &pg->text[i * pg->columns];

		for (j = 0; j < 40; ++j) {
			int offset = hp - first;
 
			if (offset >= me) {
				s->row[0] = i;
				s->col[0] = j;
				return;
			}

			if (offset < ms) {
				if (39 == j) {
					s->row[1] = i + 1;
					s->col[1] = 0;
				} else {
					s->row[1] = i;
					s->col[1] = j + 1;
				}
			}

			switch (cp->size) {
			case VBI3_DOUBLE_SIZE:
				if (offset >= ms) {
					cp[pg->columns].foreground =
						32 + VBI3_BLACK;
					cp[pg->columns].background =
						32 + VBI3_YELLOW;
					cp[pg->columns + 1].foreground =
						32 + VBI3_BLACK;
					cp[pg->columns + 1].background =
						32 + VBI3_YELLOW;
				}

				/* fall through */

			case VBI3_DOUBLE_WIDTH:
				if (offset >= ms) {
					cp[0].foreground = 32 + VBI3_BLACK;
					cp[0].background = 32 + VBI3_YELLOW;
					cp[1].foreground = 32 + VBI3_BLACK;
					cp[1].background = 32 + VBI3_YELLOW;
				}

				++hp;
				++cp;
				++j;

				break;

			case VBI3_DOUBLE_HEIGHT:
				if (offset >= ms) {
					cp[pg->columns].foreground =
						32 + VBI3_BLACK;
					cp[pg->columns].background =
						32 + VBI3_YELLOW;
				}

				/* fall through */

			case VBI3_NORMAL_SIZE:
				if (offset >= ms) {
					cp[0].foreground = 32 + VBI3_BLACK;
					cp[0].background = 32 + VBI3_YELLOW;
				}

				++hp;

				break;

			default:
				/* skipped */
				/* ++hp; */
				break;
			}

			++cp;
		}

		++hp;
	}
}

static int
search_page_fwd			(cache_page *		cp,
				 vbi3_bool		wrapped,
				 void *			user_data)
{
	vbi3_search *s = user_data;
	unsigned int here;
	unsigned int start;
	unsigned int stop;
	int row;
	ucs2_t *hp;
	ucs2_t *first;
	unsigned long ms;
	unsigned long me;
	int flags;
	int i;

	here  = (cp->pgno << 16) + cp->subno;
	start = (s->start_pgno << 16) + s->start_subno;
	stop  = (s->stop_pgno[0] << 16) + s->stop_subno[0];

	if (start >= stop) {
		if (wrapped && here >= stop)
			return -1; /* all done, abort */
	} else if (here < start || here >= stop)
		return -1; /* all done, abort */

	if (cp->function != PAGE_FUNCTION_LOP)
		return 0; /* try next */

	_vbi3_page_priv_destroy (&s->pgp);
	_vbi3_page_priv_init (&s->pgp);

	if (!_vbi3_page_priv_from_cache_page_va_list (&s->pgp,
						      cp,
						      s->format_options))
		return -3; /* formatting error, abort */

	if (s->progress)
		if (!s->progress (s, &s->pgp.pg, s->user_data)) {
			if (here != start) {
				s->start_pgno = cp->pgno;
				s->start_subno = cp->subno;
				s->row[0] = FIRST_ROW;
				s->row[1] = LAST_ROW + 1;
				s->col[0] = s->col[1] = 0;
			}

			return -2; /* canceled */
		}

	/* To Unicode */

	hp = s->haystack;
	first = hp;
	row = (here == start) ? s->row[0] : -1;
	flags = 0;

	if (row > LAST_ROW)
		return 0; /* try next page */

	for (i = FIRST_ROW; i < LAST_ROW; i++) {
		vbi3_char *cp;
		int j;

		cp = &s->pgp.pg.text[i * s->pgp.pg.columns];

		for (j = 0; j < 40; ++cp, ++j) {
			if (i == row && j <= s->col[0])
				first = hp;

			if (cp->size == VBI3_DOUBLE_WIDTH
			    || cp->size == VBI3_DOUBLE_SIZE) {
				/* "ZZAAPPZILLA" -> "ZAPZILLA" */
				++cp; /* skip left half */
				++j;
			} else if (cp->size > VBI3_DOUBLE_SIZE) {
				/* skip */
				/* *hp++ = 0x0020; */
				continue;
			}

			*hp++ = cp->unicode;
			flags = URE_NOTBOL;
		}

		*hp++ = SEPARATOR;
		flags = 0;
	}

	/* Search */

	if (first >= hp)
		return 0; /* try next page */

	if (0)
		fprintf (stderr, "exec: %x/%x; start %d,%d; %c%c%c...\n",
			 cp->pgno, cp->subno,
			 s->row[0], s->col[0],
			 _vbi3_to_ascii (first[0]),
			 _vbi3_to_ascii (first[1]),
			 _vbi3_to_ascii (first[2]));

	if (!ure_exec (s->ud, flags, first, hp - first, &ms, &me))
		return 0; /* try next page */

	highlight (s, cp, first, ms, me);

	return 1; /* success, abort */
}

static int
search_page_rev			(cache_page *		cp,
				 vbi3_bool		wrapped,
				 void *			user_data)
{
	vbi3_search *s = user_data;
	unsigned int here;
	unsigned int start;
	unsigned int stop;
	int row;
	unsigned long ms;
	unsigned long me;
	ucs2_t *hp;
	int flags;
	int i;

	here  = (cp->pgno << 16) + cp->subno;
	start = (s->start_pgno << 16) + s->start_subno;
	stop  = (s->stop_pgno[1] << 16) + s->stop_subno[1];

	if (start <= stop) {
		if (wrapped && here <= stop)
			return -1; /* all done, abort */
	} else if (here > start || here <= stop)
		return -1; /* all done, abort */

	if (cp->function != PAGE_FUNCTION_LOP)
		return 0; /* try next page */

	_vbi3_page_priv_destroy (&s->pgp);

	if (!_vbi3_page_priv_from_cache_page_va_list (&s->pgp,
						      cp,
						      s->format_options))
		return -3; /* formatting error, abort */

	if (s->progress)
		if (!s->progress (s, &s->pgp.pg, s->user_data)) {
			if (here != start) {
				s->start_pgno = cp->pgno;
				s->start_subno = cp->subno;
				s->row[0] = FIRST_ROW;
				s->row[1] = LAST_ROW + 1;
				s->col[0] = s->col[1] = 0;
			}

			return -2; /* canceled */
		}

	/* To Unicode */

	hp = s->haystack;
	row = (here == start) ? s->row[1] : 100;
	flags = 0;

	if (row < FIRST_ROW)
		goto break2;

	for (i = FIRST_ROW; i < LAST_ROW; i++) {
		vbi3_char *cp;
		int j;

		cp = &s->pgp.pg.text[i * s->pgp.pg.columns];

		for (j = 0; j < 40; ++cp, ++j) {
			if (i == row && j >= s->col[1])
				goto break2;

			if (cp->size == VBI3_DOUBLE_WIDTH
			    || cp->size == VBI3_DOUBLE_SIZE) {
				/* "ZZAAPPZILLA" -> "ZAPZILLA" */
				++cp; /* skip left half */
				++j;
			} else if (cp->size > VBI3_DOUBLE_SIZE) {
				/* skip */
				/* *hp++ = 0x0020; */
				continue;
			}

			*hp++ = cp->unicode;
			flags = URE_NOTEOL;
		}

		*hp++ = SEPARATOR;
		flags = 0;
	}

break2:
	if (hp <= s->haystack)
		return 0; /* try next page */

	/* Search */

	ms = 0;
	me = 0;

	for (i = 0; s->haystack + me < hp; ++i) {
		unsigned long ms1;
		unsigned long me1;

		if (0)
			fprintf (stderr, "exec: %x/%x; %d, %ld; '%c%c%c...'\n",
				 cp->pgno, cp->subno, i, me,
				 _vbi3_to_ascii (s->haystack[me + 0]),
				 _vbi3_to_ascii (s->haystack[me + 1]),
				 _vbi3_to_ascii (s->haystack[me + 2]));

		if (!ure_exec (s->ud, (me > 0) ? (flags | URE_NOTBOL) : flags,
			       s->haystack + me,
			       hp - s->haystack - me,
			       &ms1, &me1))
			break;

		ms = me + ms1;
		me = me + me1;
	}

	if (i == 0)
		return 0; /* try next page */

	highlight (s, cp, s->haystack, ms, me);

	return 1; /* success, abort */
}

/**
 * @param search Initialized search context.
 * @param pg Place to store the formatted (as with vbi3_fetch_vt_page())
 *   Teletext page containing the found pattern. Do <em>not</em>
 *   call vbi3_unref_page() for this page. Also the page must not
 *   be modified. See vbi3_search_status for semantics.
 * @param dir Search direction +1 forward or -1 backward.
 * @param format_options blah
 *
 * Find the next occurence of the search pattern.
 *
 * @return
 * vbi3_search_status.
 */
vbi3_search_status
vbi3_search_next_va_list	(vbi3_search *		s,
				 const vbi3_page **	pg,
				 int			dir,
				 va_list		format_options)
{
	assert (NULL != s);
	assert (NULL != pg);

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
	} else if (dir != s->dir) {
		s->dir = dir;

		s->stop_pgno[0] = s->start_pgno;
		s->stop_subno[0] =
			(s->start_subno == VBI3_ANY_SUBNO) ?
			0 : s->start_subno;
		s->stop_pgno[1] = s->start_pgno;
		s->stop_subno[1] = s->start_subno;
	}

#ifdef __va_copy
	__va_copy (s->format_options, format_options);
#else
	s->format_options = format_options;
#endif

	switch (_vbi3_cache_foreach_page (s->cache,
					  s->network,
					  s->start_pgno,
					  s->start_subno,
					  dir,
					  (dir > 0) ?
					  search_page_fwd
					  : search_page_rev,
					  /* user_data */ s)) {
	case 1:
		*pg = &s->pgp.pg;
		return VBI3_SEARCH_SUCCESS;

	case 0:
		return VBI3_SEARCH_CACHE_EMPTY;

	case -1:
		s->dir = 0;
		return VBI3_SEARCH_NOT_FOUND;

	case -2:
		return VBI3_SEARCH_ABORTED;

	default:
		break;
	}

	return VBI3_SEARCH_ERROR;
}

vbi3_search_status
vbi3_search_next		(vbi3_search *		search,
				 const vbi3_page **	pg,
				 int			dir,
				 ...)
{
	vbi3_search_status s;
	va_list format_options;

	va_start (format_options, dir);
	s = vbi3_search_next_va_list (search, pg, dir, format_options);
	va_end (format_options);

	return s;
}

/**
 * @param search vbi3_search context.
 * 
 * Delete the search context created by vbi3_search_new().
 */
void
vbi3_search_delete		(vbi3_search *		s)
{
	if (NULL == s)
		return;

	if (s->ud)
		ure_dfa_free (s->ud);

	if (s->ub)
		ure_buffer_free (s->ub);

	_vbi3_page_priv_destroy (&s->pgp);

	if (s->network)
		cache_network_unref (s->network);

	if (s->cache)
		vbi3_cache_unref (s->cache);

	CLEAR (*s);

	vbi3_free (s);
}

/**
 * @param pgno 
 * @param subno Page and subpage number of the first (forward) or
 *   last (backward) page to visit. Optional @c VBI3_ANY_SUBNO. 
 * @param pattern Unicode (UCS-2) search pattern.
 * @param pattern_size Number of characters (not bytes) in the pattern
 *   buffer.
 * @param casefold Boolean, search case insensitive.
 * @param regexp Boolean, the search pattern is a regular expression.
 * @param progress A function called for each page scanned, can be
 *   \c NULL. Shall return @c FALSE to abort the search. @a pg is valid
 *   for display (e. g. @a pg->pgno), do <em>not</em> call
 *   vbi3_unref_page() or modify this page.
 * @param user_data blah.
 *
 * Allocate a vbi3_search context and prepare for searching
 * the Teletext page cache. The context must be freed with
 * vbi3_search_delete().
 * 
 * Regular expression searching supports the standard set
 * of operators and constants, with these extensions:
 *
 * <table>
 * <tr><td>\x....</td><td>hexadecimal number of up to 4 digits</td></tr>
 * <tr><td>\X....</td><td>hexadecimal number of up to 4 digits</td></tr>
 * <tr><td>\u....</td><td>hexadecimal number of up to 4 digits</td></tr>
 * <tr><td>\U....</td><td>hexadecimal number of up to 4 digits</td></tr>
 * <tr><td>:title:</td><td>Unicode specific character class</td></tr>
 * <tr><td>:gfx:</td><td>Teletext G1 or G3 graphics</td></tr>
 * <tr><td>:drcs:</td><td>Teletext DRCS</td></tr>
 * <tr><td>\pN1,N2,...,Nn</td><td>Character properties class</td></tr>
 * <tr><td>\PN1,N2,...,Nn</td><td>Negated character properties class</td></tr>
 * </table>
 *
 * <table>
 * <tr><td><b>N</b></td><td><b>Property</b></td></tr>
 * <tr><td>1</td><td>alphanumeric</td></tr>
 * <tr><td>2</td><td>alpha</td></tr>
 * <tr><td>3</td><td>control</td></tr>
 * <tr><td>4</td><td>digit</td></tr>
 * <tr><td>5</td><td>graphical</td></tr>
 * <tr><td>6</td><td>lowercase</td></tr>
 * <tr><td>7</td><td>printable</td></tr>
 * <tr><td>8</td><td>punctuation</td></tr>
 * <tr><td>9</td><td>space</td></tr>
 * <tr><td>10</td><td>uppercase</td></tr>
 * <tr><td>11</td><td>hex digit</td></tr>
 * <tr><td>12</td><td>title</td></tr>
 * <tr><td>13</td><td>defined</td></tr>
 * <tr><td>14</td><td>wide</td></tr>
 * <tr><td>15</td><td>nonspacing</td></tr>
 * <tr><td>16</td><td>Teletext G1 or G3 graphics</td></tr>
 * <tr><td>17</td><td>Teletext DRCS</td></tr>
 * </table>
 *
 * Character classes can contain literals, constants, and character
 * property classes. Example: [abc\U10A\p1,3,4]. Note double height
 * and size characters will match twice, on the upper and lower row,
 * and double width and size characters count as one (reducing the
 * line width) so one can find combinations of normal and enlarged
 * characters.
 *
 * @bug
 * In a multithreaded application the data service decoder may receive
 * and cache new pages during a search session. When these page numbers
 * have been visited already the pages are not searched. At a channel
 * switch (and in future at any time) pages can be removed from cache.
 * All this has yet to be addressed.
 *
 * @return
 * A vbi3_search context or @c NULL on error.
 */
vbi3_search *
vbi3_search_ucs2_new		(vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 const uint16_t *	pattern,
				 unsigned long		pattern_size,
				 vbi3_bool		casefold,
				 vbi3_bool		regexp,
				 vbi3_search_progress_cb *progress,
				 void *			user_data)
{
	vbi3_search *s;
	uint16_t *esc_pattern;

	assert (NULL != ca);
	assert (NULL != nk);
	assert (NULL != pattern);

	esc_pattern = NULL;

	if (0 == pattern_size) {
		return NULL;
	}

	if (!(s = vbi3_malloc (sizeof (*s)))) {
		return NULL;
	}

	CLEAR (*s);

	s->cache = vbi3_cache_ref (ca);

	if (!(s->network = _vbi3_cache_get_network (ca, nk)))
		goto failure;

	_vbi3_page_priv_init (&s->pgp);

	s->progress = progress;
	s->user_data = user_data;

	if (!regexp) {
		unsigned int size;
		unsigned int i;
		unsigned int j;

		size = pattern_size * 2 * sizeof (*esc_pattern);
		if (!(esc_pattern = vbi3_malloc (size))) {
			goto failure;
		}

		j = 0;

		for (i = 0; i < pattern_size; ++i) {
			if (strchr ("!\"#$%&()*+,-./:;=?@[\\]^_{|}~",
				    pattern[i]))
				esc_pattern[j++] = '\\';

			esc_pattern[j++] = pattern[i];
		}

		pattern = esc_pattern;
		pattern_size = j;
	}

	if (!(s->ub = ure_buffer_create ()))
		goto failure;

	if (!(s->ud = ure_compile (pattern, pattern_size, casefold, s->ub)))
		goto failure;

	vbi3_free (esc_pattern);
	esc_pattern = NULL;

	s->stop_pgno[0] = pgno;
	s->stop_subno[0] = (subno == VBI3_ANY_SUBNO) ? 0 : subno;

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

	return s;

 failure:
	vbi3_free (esc_pattern);

	vbi3_search_delete (s);

	return NULL;
}

vbi3_search *
vbi3_search_utf8_new		(vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 const char *		pattern,
				 vbi3_bool		casefold,
				 vbi3_bool		regexp,
				 vbi3_search_progress_cb *progress,
				 void *			user_data)
{
	uint16_t *ucs2_pattern;
	vbi3_search *s;

	assert (NULL != pattern);

	ucs2_pattern = (uint16_t *) vbi3_strndup_iconv ("UCS-2", "UTF-8",
							pattern,
							strlen (pattern) + 1);
	if (!ucs2_pattern)
		return NULL;

	s = vbi3_search_ucs2_new (ca,
				  nk, pgno, subno,
				  ucs2_pattern,
				  vbi3_strlen_ucs2 (ucs2_pattern),
				  casefold, regexp,
				  progress, user_data);

	vbi3_free (ucs2_pattern);

	return s;
}

#else /* !HAVE_GLIBC21 && !HAVE_LIBUNICODE */

vbi3_search_status
vbi3_search_next_va_list	(vbi3_search *		s,
				 const vbi3_page **	pg,
				 int			dir,
				 va_list		format_options)
{
	assert (NULL != s);
	assert (NULL != pg);

	return VBI3_SEARCH_ERROR;
}

vbi3_search_status
vbi3_search_next		(vbi3_search *		s,
				 const vbi3_page **	pg,
				 int			dir,
				 ...)
{
	assert (NULL != s);
	assert (NULL != pg);

	return VBI3_SEARCH_ERROR;
}

void
vbi3_search_delete		(vbi3_search *		s)
{
	assert (NULL == s);
}

vbi3_search *
vbi3_search_ucs2_new		(vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 const uint16_t *	pattern,
				 unsigned long		pattern_size,
				 vbi3_bool		casefold,
				 vbi3_bool		regexp,
				 vbi3_search_progress_cb *progress,
				 void *			user_data)
{
	return NULL;
}

vbi3_search *
vbi3_search_utf8_new		(vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 const char *		pattern,
				 vbi3_bool		casefold,
				 vbi3_bool		regexp,
				 vbi3_search_progress_cb *progress,
				 void *			user_data)
{
	return NULL;
}

#endif /* !HAVE_GLIBC21 && !HAVE_LIBUNICODE */
