/*
 *  libzvbi - Tables Of Pages
 *
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: top.c,v 1.1.2.1 2004-07-09 16:10:54 mschimek Exp $ */

#include <stdlib.h>		/* malloc(), qsort() */
#include "conv.h"		/* _vbi_strdup_locale_teletext() */
#include "cache-priv.h"

#ifndef TOP_LOG
#define TOP_LOG 0
#endif

#define log(templ, args...)						\
do {									\
	if (TOP_LOG)							\
		fprintf (stderr, templ , ##args);			\
} while (0)

/**
 */
void
vbi_top_title_destroy		(vbi_top_title *	tt)
{
	assert (NULL != tt);

	free (tt->title);

	CLEAR (*tt);
}

/**
 */
vbi_bool
vbi_top_title_copy		(vbi_top_title *	dst,
				 const vbi_top_title *	src)
{
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
void
vbi_top_title_init		(vbi_top_title *	tt)
{
	assert (NULL != tt);

	CLEAR (*tt);
}

/**
 */
void
vbi_top_title_array_delete	(vbi_top_title *	tt,
				 unsigned int		tt_size)
{
	unsigned int i;

	if (NULL == tt || 0 == tt_size)
		return;

	for (i = 0; i < tt_size; ++i)
		vbi_top_title_destroy (tt + i);

	free (tt);
}

/**
 * @internal
 */
const ait_title *
cache_network_get_ait_title	(cache_network *	cn,
				 cache_page **		ait_cp,
				 vbi_pgno		pgno,
				 vbi_subno		subno)
{
	unsigned int i;

	for (i = 0; i < 8; ++i) {
		cache_page *cp;
		const ait_title *ait;
		unsigned int j;

		if (cn->btt_link[i].function != PAGE_FUNCTION_AIT)
			continue;

		cp = _vbi_cache_get_page (cn->cache, cn,
					  cn->btt_link[i].pgno,
					  cn->btt_link[i].subno, 0x3f7f);
		if (!cp) {
			log ("...top ait page %x not cached\n",
			     cn->btt_link[i].pgno);
			continue;
		} else if (cp->function != PAGE_FUNCTION_AIT) {
			log ("...no ait page %x\n", cp->pgno);
			cache_page_release (cp);
			continue;
		}

		ait = cp->data.ait.title;

		for (j = 0; j < N_ELEMENTS (cp->data.ait.title); ++j) {
			if (ait->page.pgno == pgno
			    && (VBI_ANY_SUBNO == subno
				|| ait->page.subno == subno)) {
				*ait_cp = cp;
				return ait;
			}

			++ait;
		}

		cache_page_release (cp);
	}

	*ait_cp = NULL;

	return NULL;
}

static vbi_bool
_vbi_top_title_from_ait_title	
				(vbi_top_title *	tt,
				 const cache_network *	cn,
				 const ait_title *	ait,
				 const vbi_character_set *cs)
{
	const page_stat *ps;
	char *title;

	title = _vbi_strdup_locale_teletext
		(ait->text, N_ELEMENTS (ait->text), cs);

	if (!title) {
		/* Make vbi_top_title_destroy() safe. */
		vbi_top_title_init (tt);
		return FALSE;
	}

	tt->title = title;

	tt->pgno = ait->page.pgno;
	tt->subno = ait->page.subno;

	ps = cache_network_const_page_stat (cn, ait->page.pgno);

	tt->group = (VBI_TOP_GROUP == ps->page_type);

	return TRUE;
}

/**
 * @internal
 */
vbi_bool
cache_network_get_top_title	(cache_network *	cn,
				 vbi_top_title *	tt,
				 vbi_pgno		pgno,
				 vbi_subno		subno)
{
	cache_page *ait_cp;
	const ait_title *ait;
	const vbi_character_set *char_set[2];
	vbi_bool r;

	assert (NULL != cn);
	assert (NULL != tt);

	if (!(ait = cache_network_get_ait_title (cn, &ait_cp, pgno, subno))) {
		/* Make vbi_top_title_destroy() safe. */
		vbi_top_title_init (tt);
		return FALSE;
	}

	if (NO_PAGE (ait->page.pgno)) {
		cache_page_release (ait_cp);
		vbi_top_title_init (tt);
		return FALSE;
	}

	_vbi_character_set_init (char_set, /* default en */ 0,
				 /* extension */ NULL, ait_cp);

	r = _vbi_top_title_from_ait_title (tt, cn, ait, char_set[0]);

	cache_page_release (ait_cp);

	return r;
}

/**
 */
vbi_bool
vbi_cache_get_top_title		(vbi_cache *		ca,
				 vbi_top_title *	tt,
				 const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno)
{
	cache_network *cn;
	vbi_bool r;

	assert (NULL != ca);
	assert (NULL != tt);
	assert (NULL != nk);

	if (!(cn = _vbi_cache_get_network (ca, nk))) {
		/* Make vbi_top_title_destroy() safe. */
		vbi_top_title_init (tt);
		return FALSE;
	}

	r = cache_network_get_top_title (cn, tt, pgno, subno);

	cache_network_release (cn);
	
	return r;
}

static int
top_title_cmp			(const void *		p1,
				 const void *		p2)
{
	const vbi_top_title *tt1 = p1;
	const vbi_top_title *tt2 = p2;

	if (tt1->pgno == tt2->pgno)
		return tt2->pgno - tt1->pgno;
	else
		return tt2->subno - tt2->subno;
}

/**
 * @internal
 */
vbi_top_title *
cache_network_get_top_titles	(cache_network *	cn,
				 unsigned int *		array_size)
{
	vbi_top_title *tt;
	unsigned int size;
	unsigned int capacity;
	unsigned int i;

	assert (NULL != cn);
	assert (NULL != array_size);

	capacity = 64;
	size = 0;

	if (!(tt = malloc (capacity * sizeof (*tt))))
		return NULL;

	for (i = 0; i < 8; ++i) {
		cache_page *cp;
		const ait_title *ait;
		const vbi_character_set *char_set[2];
		unsigned int j;

		if (cn->btt_link[i].function != PAGE_FUNCTION_AIT)
			continue;

		cp = _vbi_cache_get_page (cn->cache, cn,
					  cn->btt_link[i].pgno,
					  cn->btt_link[i].subno, 0x3f7f);
		if (!cp) {
			log ("...top ait page %x not cached\n",
			     cn->btt_link[i].pgno);
			continue;
		} else if (cp->function != PAGE_FUNCTION_AIT) {
			log ("...no ait page %x\n", cp->pgno);
			cache_page_release (cp);
			continue;
		}

		_vbi_character_set_init (char_set, /* default en */ 0,
					 /* extension */ NULL, cp);

		ait = cp->data.ait.title;

		for (j = 0; j < N_ELEMENTS (cp->data.ait.title); ++j) {
			if (NO_PAGE (ait->page.pgno)) {
				++ait;
				continue;
			}

			if (size + 1 >= capacity) {
				vbi_top_title *tt1;
				unsigned int n;

				n = sizeof (*tt) * 2 * capacity;
				if (!(tt1 = realloc (tt, n))) {
					vbi_top_title_array_delete (tt, size);
					cache_page_release (cp);
					return NULL;
				}

				tt = tt1;
				capacity *= 2;
			}

			if (_vbi_top_title_from_ait_title
			    (tt + size, cn, ait, char_set[0])) {
				++size;
			}

			++ait;
		}

		cache_page_release (cp);
	}

	/* Last element empty. */
	vbi_top_title_init (tt + size);

	/* Make sure we're sorted by pgno. */
	qsort (tt, size, sizeof (*tt), top_title_cmp);

	*array_size = size;

	return tt;
}

/**
 */
vbi_top_title *
vbi_cache_get_top_titles	(vbi_cache *		ca,
				 const vbi_network *	nk,
				 unsigned int *		array_size)
{
	cache_network *cn;
	vbi_top_title *tt;

	assert (NULL != ca);
	assert (NULL != nk);
	assert (NULL != array_size);

	*array_size = 0;

	if (!(cn = _vbi_cache_get_network (ca, nk)))
		return NULL;

	tt = cache_network_get_top_titles (cn, array_size);

	cache_network_release (cn);

	return tt;
}
