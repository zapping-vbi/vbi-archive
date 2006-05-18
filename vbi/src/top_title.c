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

/* $Id: top_title.c,v 1.1.2.3 2006-05-18 16:49:20 mschimek Exp $ */

#include <stdlib.h>		/* malloc(), qsort() */
#include "conv.h"		/* _vbi3_strdup_locale_teletext() */
#include "lang.h"		/* vbi3_character_set_code */
#include "cache-priv.h"


#ifndef TOP_TITLE_LOG
#define TOP_TITLE_LOG 0
#endif

#define log(templ, args...)						\
do {									\
	if (TOP_TITLE_LOG)						\
		fprintf (stderr, templ , ##args);			\
} while (0)

/**
 * Frees all resources associate with this vbi3_top_title
 * except the structure itself.
 */
void
vbi3_top_title_destroy		(vbi3_top_title *	tt)
{
	assert (NULL != tt);

	vbi3_free (tt->xtitle);

	CLEAR (*tt);
}

/**
 * @param dst A copy of @a src will be stored here.
 * @param src vbi3_top_title to be copied. If @c NULL this function
 *   has the same effect as vbi3_top_title_init().
 *
 * Creates a deep copy of @a src in @a dst, overwriting the data
 * previously stored at @a dst.
 *
 * @returns
 * @c FALSE on failure (out of memory).
 */
vbi3_bool
vbi3_top_title_copy		(vbi3_top_title *	dst,
				 const vbi3_top_title *	src)
{
	if (dst == src)
		return TRUE;

	assert (NULL != dst);

	if (src) {
		char *title;

		/* XXX uses locale encoding. */
		title = strdup (src->xtitle);
		if (NULL == title)
			return FALSE;

		*dst = *src;
		dst->xtitle = title;
	} else {
		CLEAR (*dst);
	}

	return TRUE;
}

/**
 * @param tt vbi3_top_title to be initialized.
 *
 * Initializes (clears) a vbi3_top_title structure.
 */
void
vbi3_top_title_init		(vbi3_top_title *	tt)
{
	assert (NULL != tt);

	CLEAR (*tt);
}

/**
 * @param tt Array of vbi3_top_title structures, can be @c NULL.
 * @param n_elements Number of structures in the array.
 *
 * Deletes an array of vbi3_top_title structures and all resources
 * associated with it.
 */
void
vbi3_top_title_array_delete	(vbi3_top_title *	tt,
				 unsigned int		n_elements)
{
	unsigned int i;

	if (NULL == tt || 0 == n_elements)
		return;

	for (i = 0; i < n_elements; ++i)
		vbi3_top_title_destroy (tt + i);

	vbi3_free (tt);
}

/**
 * @internal
 */
const struct ait_title *
cache_network_get_ait_title	(cache_network *	cn,
				 cache_page **		ait_cp,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno)
{
	unsigned int i;

	for (i = 0; i < 8; ++i) {
		cache_page *cp;
		const struct ait_title *ait;
		unsigned int j;

		if (cn->btt_link[i].function != PAGE_FUNCTION_AIT)
			continue;

		cp = _vbi3_cache_get_page (cn->cache, cn,
					  cn->btt_link[i].pgno,
					  cn->btt_link[i].subno, 0x3f7f);
		if (!cp) {
			log ("...top ait page %x not cached\n",
			     cn->btt_link[i].pgno);
			continue;
		} else if (cp->function != PAGE_FUNCTION_AIT) {
			log ("...no ait page %x\n", cp->pgno);
			cache_page_unref (cp);
			continue;
		}

		ait = cp->data.ait.title;

		for (j = 0; j < N_ELEMENTS (cp->data.ait.title); ++j) {
			if (ait->page.pgno == pgno
			    && (VBI3_ANY_SUBNO == subno
				|| ait->page.subno == subno)) {
				*ait_cp = cp;
				return ait;
			}

			++ait;
		}

		cache_page_unref (cp);
	}

	*ait_cp = NULL;

	return NULL;
}

static vbi3_bool
_vbi3_top_title_from_ait_title	
				(vbi3_top_title *	tt,
				 const cache_network *	cn,
				 const struct ait_title *ait,
				 const vbi3_character_set *cs)
{
	const struct page_stat *ps;
	char *title;

	title = vbi3_strndup_iconv_teletext (vbi3_locale_codeset (),
					     ait->text, N_ELEMENTS (ait->text),
					     cs);
	if (NULL == title) {
		/* Make vbi3_top_title_destroy() safe. */
		vbi3_top_title_init (tt);
		return FALSE;
	}

	tt->xtitle = title;

	tt->pgno = ait->page.pgno;
	tt->subno = ait->page.subno;

	ps = cache_network_const_page_stat (cn, ait->page.pgno);

	tt->group = (VBI3_TOP_GROUP == ps->page_type);

	return TRUE;
}

/**
 * @internal
 */
vbi3_bool
cache_network_get_top_title	(cache_network *	cn,
				 vbi3_top_title *	tt,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno)
{
	cache_page *ait_cp;
	const struct ait_title *ait;
	const vbi3_character_set *char_set[2];
	vbi3_bool r;

	assert (NULL != cn);
	assert (NULL != tt);

	if (!(ait = cache_network_get_ait_title (cn, &ait_cp, pgno, subno))) {
		/* Make vbi3_top_title_destroy() safe. */
		vbi3_top_title_init (tt);
		return FALSE;
	}

	if (NO_PAGE (ait->page.pgno)) {
		cache_page_unref (ait_cp);
		vbi3_top_title_init (tt);
		return FALSE;
	}

	_vbi3_character_set_init (char_set,
				 /* default en */ 0,
				 /* default en */ 0,
				 /* extension */ NULL,
				 ait_cp);

	r = _vbi3_top_title_from_ait_title (tt, cn, ait, char_set[0]);

	cache_page_unref (ait_cp);

	return r;
}

/**
 */
vbi3_bool
vbi3_cache_get_top_title		(vbi3_cache *		ca,
				 vbi3_top_title *	tt,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno)
{
	cache_network *cn;
	vbi3_bool r;

	assert (NULL != ca);
	assert (NULL != tt);
	assert (NULL != nk);

	if (!(cn = _vbi3_cache_get_network (ca, nk))) {
		/* Make vbi3_top_title_destroy() safe. */
		vbi3_top_title_init (tt);
		return FALSE;
	}

	r = cache_network_get_top_title (cn, tt, pgno, subno);

	cache_network_unref (cn);
	
	return r;
}

static int
top_title_cmp			(const void *		p1,
				 const void *		p2)
{
	const vbi3_top_title *tt1 = p1;
	const vbi3_top_title *tt2 = p2;

	if (tt1->pgno == tt2->pgno)
		return tt2->pgno - tt1->pgno;
	else
		return tt2->subno - tt2->subno;
}

/**
 * @internal
 */
vbi3_top_title *
cache_network_get_top_titles	(cache_network *	cn,
				 unsigned int *		n_elements)
{
	vbi3_top_title *tt;
	unsigned int size;
	unsigned int capacity;
	unsigned int i;

	assert (NULL != cn);
	assert (NULL != n_elements);

	capacity = 64;
	size = 0;

	if (!(tt = vbi3_malloc (capacity * sizeof (*tt))))
		return NULL;

	for (i = 0; i < 8; ++i) {
		cache_page *cp;
		const struct ait_title *ait;
		const vbi3_character_set *char_set[2];
		unsigned int j;

		if (cn->btt_link[i].function != PAGE_FUNCTION_AIT)
			continue;

		cp = _vbi3_cache_get_page (cn->cache, cn,
					  cn->btt_link[i].pgno,
					  cn->btt_link[i].subno, 0x3f7f);
		if (!cp) {
			log ("...top ait page %x not cached\n",
			     cn->btt_link[i].pgno);
			continue;
		} else if (cp->function != PAGE_FUNCTION_AIT) {
			log ("...no ait page %x\n", cp->pgno);
			cache_page_unref (cp);
			continue;
		}

		_vbi3_character_set_init (char_set,
					 /* default en */ 0,
					 /* default en */ 0,
					 /* extension */ NULL,
					 cp);

		ait = cp->data.ait.title;

		for (j = 0; j < N_ELEMENTS (cp->data.ait.title); ++j) {
			if (NO_PAGE (ait->page.pgno)) {
				++ait;
				continue;
			}

			if (size + 1 >= capacity) {
				vbi3_top_title *tt1;
				unsigned long n;

				n = sizeof (*tt) * 2 * capacity;
				if (!(tt1 = vbi3_realloc (tt, n))) {
					vbi3_top_title_array_delete (tt, size);
					cache_page_unref (cp);
					return NULL;
				}

				tt = tt1;
				capacity *= 2;
			}

			if (_vbi3_top_title_from_ait_title
			    (tt + size, cn, ait, char_set[0])) {
				++size;
			}

			++ait;
		}

		cache_page_unref (cp);
	}

	/* Last element empty. */
	vbi3_top_title_init (tt + size);

	if (0) {
		/* Make sure we're sorted by pgno. */
		qsort (tt, size, sizeof (*tt), top_title_cmp);
	}

	*n_elements = size;

	return tt;
}

/**
 */
vbi3_top_title *
vbi3_cache_get_top_titles	(vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 unsigned int *		n_elements)
{
	cache_network *cn;
	vbi3_top_title *tt;

	assert (NULL != ca);
	assert (NULL != nk);
	assert (NULL != n_elements);

	*n_elements = 0;

	if (!(cn = _vbi3_cache_get_network (ca, nk)))
		return NULL;

	tt = cache_network_get_top_titles (cn, n_elements);

	cache_network_unref (cn);

	return tt;
}
