/*
 *  libzvbi -- Teletext Table Of Pages
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

/* $Id: top_title.c,v 1.1.2.1 2008-08-22 07:59:54 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "conv.h"		/* _vbi_strdup_locale_teletext() */
#include "ttx_charset.h"
#include "top_title-priv.h"

#ifndef TOP_TITLE_LOG
#  define TOP_TITLE_LOG 0
#endif

#define log(templ, args...)						\
do {									\
	if (TOP_TITLE_LOG)						\
		fprintf (stderr, templ , ##args);			\
} while (0)

/**
 * @internal
 */
const struct ttx_ait_title *
_vbi_cn_get_ait_title		(cache_network *	cn,
				 cache_page **		ait_cp,
				 vbi_pgno		pgno,
				 vbi_subno		subno)
{
	unsigned int i;

	for (i = 0; i < 8; ++i) {
		cache_page *cp;
		const struct ttx_ait_title *ait;
		unsigned int j;

		if (cn->btt_link[i].function != PAGE_FUNCTION_AIT)
			continue;

		cp = _vbi_cache_get_page (cn->cache, cn,
					  cn->btt_link[i].pgno,
					  cn->btt_link[i].subno,
					  /* subno_mask */ 0x3F7F);
		if (NULL == cp) {
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
			if (ait->link.pgno == pgno
			    && (VBI_ANY_SUBNO == subno
				|| ait->link.subno == subno)) {
				*ait_cp = cp;
				return ait;
			}

			++ait;
		}

		cache_page_unref (cp);
	}

	*ait_cp = NULL;

	errno = VBI_ERR_NO_TOP_TITLE;

	return NULL;
}

/**
 * @internal
 */
vbi_bool
_vbi_top_title_from_ait_title	(vbi_top_title *	tt,
				 const char *		codeset,
				 const cache_network *	cn,
				 const struct ttx_ait_title *ait,
				 const vbi_ttx_charset *cs)
{
	const struct ttx_page_stat *ps;
	char *title;
	unsigned len;
	
	for (len = N_ELEMENTS (ait->text); len > 0; --len) {
		if (ait->text[len - 1] > 0x20)
			break;
	}

	title = vbi_strndup_iconv_teletext (codeset,
					    cs,
					    ait->text,
					    len,
					    /* repl_char */ '_');
	if (unlikely (NULL == title)) {
#warning "use error code returned by vbi_strndup when available"
		errno = ENOMEM;
		return FALSE;
	}

	tt->title = title;

	tt->pgno = ait->link.pgno;
	tt->subno = ait->link.subno;

	ps = cache_network_const_page_stat (cn, ait->link.pgno);

	if (VBI_TOP_BLOCK == ps->page_type)
		tt->type = VBI_TOP_BLOCK;
	else
		tt->type = VBI_TOP_GROUP;

	CLEAR (tt->_reserved);

	return TRUE;
}

/**
 * @internal
 */
vbi_top_title *
_vbi_cn_get_top_title		(cache_network *	cn,
				 vbi_pgno		pgno,
				 vbi_subno		subno)
{
	const struct ttx_ait_title *ait;
	cache_page *ait_cp;
	vbi_top_title *tt;
	const char *codeset;
	const vbi_ttx_charset *cs;
	int saved_errno;

	_vbi_null_check (cn, NULL);

	ait = _vbi_cn_get_ait_title (cn, &ait_cp, pgno, subno);
	if (unlikely (NULL == ait)) {
		errno = VBI_ERR_NO_TOP_TITLE;
		return NULL;
	}

	saved_errno = VBI_ERR_NO_TOP_TITLE;
	if (NO_PAGE (ait->link.pgno))
		goto failed;

	tt = malloc (sizeof (*tt));
	saved_errno = ENOMEM;
	if (unlikely (NULL == tt))
		goto failed;

	codeset = vbi_locale_codeset ();

	cs = _vbi_cache_page_ttx_charset (ait_cp,
					  0 /* primary */,
					  /* default: en */ 0,
					  /* extension */ NULL);

	if (unlikely (!_vbi_top_title_from_ait_title (tt, codeset,
						      cn, ait, cs))) {
		saved_errno = errno;
		goto failed;
	}

	cache_page_unref (ait_cp);

	return tt;

 failed:
	cache_page_unref (ait_cp);

	errno = saved_errno;

	return NULL;
}

static vbi_bool
append_title			(vbi_top_title **	tt_array,
				 unsigned int *		capacity,
				 unsigned int *		n_elements,
				 const char *		codeset,
				 const cache_network *	cn,
				 const struct ttx_ait_title *ait,
				 const vbi_ttx_charset *cs)
{
	if (unlikely (*n_elements >= *capacity)) {
		if (!_vbi_grow_vector_capacity ((void **) tt_array,
						capacity,
						*n_elements + 1,
						sizeof (**tt_array)))
			return FALSE;
	}

	if (!_vbi_top_title_from_ait_title (*tt_array + *n_elements,
					    codeset,
					    cn, ait, cs)) {
		return FALSE;
	}

	++*n_elements;

	return TRUE;
}

/**
 * @internal
 */
vbi_bool
_vbi_cn_get_top_titles		(cache_network *	cn,
				 vbi_top_title **	tt_array_p,
				 unsigned int *		n_elements_p)
{
	const char *codeset;
	vbi_top_title *tt_array;
	unsigned int capacity;
	unsigned int n_elements;
	unsigned int i;
	int saved_errno;

	_vbi_null_check (cn, FALSE);
	_vbi_null_check (tt_array_p, FALSE);
	_vbi_null_check (n_elements_p, FALSE);

	tt_array = NULL;
	capacity = 0;

	if (!_vbi_grow_vector_capacity ((void **) &tt_array,
					&capacity,
					/* min_capacity */ 64,
					sizeof (*tt_array))) {
		return FALSE;
	}

	codeset = vbi_locale_codeset ();
	n_elements = 0;

	for (i = 0; i < 8; ++i) {
		cache_page *cp;
		const vbi_ttx_charset *cs;
		unsigned int j;

		if (cn->btt_link[i].function != PAGE_FUNCTION_AIT)
			continue;

		cp = _vbi_cache_get_page (cn->cache, cn,
					  cn->btt_link[i].pgno,
					  cn->btt_link[i].subno,
					  /* subno_mask */ 0x3F7F);
		if (NULL == cp) {
			log ("...top ait page %x not cached\n",
			     cn->btt_link[i].pgno);
			continue;
		} else if (cp->function != PAGE_FUNCTION_AIT) {
			log ("...no ait page %x\n", cp->pgno);
			cache_page_unref (cp);
			continue;
		}

		cs = _vbi_cache_page_ttx_charset (cp,
						  0 /* primary */,
						  /* default: en */ 0,
						  /* extension */ NULL);

		for (j = 0; j < N_ELEMENTS (cp->data.ait.title); ++j) {
			if (NO_PAGE (cp->data.ait.title[j].link.pgno))
				continue;

			if (!append_title (&tt_array,
					   &capacity,
					   &n_elements,
					   codeset,
					   cn,
					   &cp->data.ait.title[j],
					   cs)) {
				cache_page_unref (cp);
				goto failed;
			}
		}

		cache_page_unref (cp);
	}

	if (0 == n_elements) {
		vbi_free (tt_array);
		errno = VBI_ERR_NO_TOP_TITLE;
		return FALSE;
	}

	/* Error ignored. */
	_vbi_shrink_vector_capacity ((void **) &tt_array,
				     &capacity,
				     n_elements,
				     sizeof (*tt_array));

	*tt_array_p = tt_array;
	*n_elements_p = n_elements;

	return TRUE;

 failed:
	saved_errno = errno;
	vbi_top_title_array_delete (tt_array, n_elements);
	errno = saved_errno;

	return FALSE;
}

/**
 * @param tt vbi_top_title structure initialized with
 *   vbi_top_title_init() or vbi_top_title_copy().
 *
 * Frees all resources associated with @a tt, except for the structure
 * itself.
 *
 * @returns
 * The function returns @c FALSE and sets the errno variable to
 * @c VBI_ERR_NULL_ARG if @a tt is a @c NULL pointer.
 *
 * @since 0.3.1
 */
vbi_bool
vbi_top_title_destroy		(vbi_top_title *	tt)
{
	_vbi_null_check (tt, FALSE);

	vbi_free (tt->title);

	CLEAR (*tt);

	return TRUE;
}

/**
 * @param dst A copy of @a src will be stored here.
 * @param src vbi_top_title to be copied, can be @c NULL.
 *
 * Creates a deep copy of @a src in @a dst, overwriting the data
 * previously stored at @a dst. If @a src is @c NULL the function has
 * the same effect as vbi_top_title_init(). It @a src == @a dst the
 * function does nothing.
 *
 * @returns
 * On failure @a dst remains unmodified, the function returns @c FALSE
 * and sets the errno variable to indicate the error:
 * - @c ENOMEM - insufficient memory.
 * - @c VBI_ERR_NULL_ARG - @a dst is a @c NULL pointer.
 *
 * @since 0.3.1
 */
vbi_bool
vbi_top_title_copy		(vbi_top_title *	dst,
				 const vbi_top_title *	src)
{
	char *new_title;

	_vbi_null_check (dst, FALSE);

	if (dst == src)
		return TRUE;

	if (NULL == src) {
		CLEAR (*dst);
		dst->subno = VBI_ANY_SUBNO;
		return TRUE;
	}

	new_title = strdup (src->title);
	if (unlikely (NULL == new_title)) {
		errno = ENOMEM;
		return FALSE;
	}

	dst->title = new_title;
	dst->pgno = src->pgno;
	dst->subno = src->subno;
	dst->type = src->type;

	CLEAR (dst->_reserved);

	return TRUE;
}

/**
 * @param tt Pointer to the vbi_top_title structure to be initialized.
 *
 * Initializes all fields of @a tt.
 *
 * @returns
 * The function returns @c FALSE and sets the errno variable to
 * @c VBI_ERR_NULL_ARG if @a tt is a @c NULL pointer.
 *
 * @since 0.3.1
 */
vbi_bool
vbi_top_title_init		(vbi_top_title *	tt)
{
	_vbi_null_check (tt, FALSE);

	CLEAR (*tt);

	return TRUE;
}

/**
 * @param tt_array Pointer to the first element in an array of
 *   vbi_top_title structures.
 * @param n_elements Number of elements in the @a tt_array.
 *
 * Creates a deep copy of the @a tt_array in a newly allocated
 * array of vbi_top_title structures.
 *
 * @returns
 * On success the function returns a pointer to the first element
 * of the allocated vbi_top_title array. If insufficent memory is
 * available it returns @c NULL and sets the errno variable to
 * @c ENOMEM.
 *
 * @since 0.3.1
 */
vbi_top_title *
vbi_top_title_array_dup		(vbi_top_title *	tt_array,
				 unsigned int		n_elements)
{
	vbi_top_title *new_tt_array;
	int saved_errno;
	unsigned int i;

	_vbi_null_check (tt_array, NULL);

	if (0 == n_elements) {
		errno = VBI_ERR_INVALID_ARG;
		return NULL;
	}

	new_tt_array = vbi_malloc (n_elements * sizeof (vbi_top_title));
	if (unlikely (NULL == new_tt_array)) {
		return NULL;
	}

	for (i = 0; i < n_elements; ++i) {
		if (!vbi_top_title_copy (new_tt_array + i,
					 tt_array + i))
			break;
	}

	if (i >= n_elements)
		return new_tt_array;

	saved_errno = errno;

	while (i-- > 0) {
		vbi_top_title_destroy (new_tt_array + i);
	}

	vbi_free (new_tt_array);

	errno = saved_errno;

	return NULL;
}

/**
 * @param tt_array Pointer to the first element in an array of
 *   vbi_top_title structures allocated by vbi_top_title_array_dup().
 *   Can be @c NULL.
 * @param n_elements Number of elements in the @a tt_array.
 *
 * Frees all resources associated with the @a tt_array, and the
 * array itself.
 *
 * @since 0.3.1
 */
void
vbi_top_title_array_delete	(vbi_top_title *	tt_array,
				 unsigned int		n_elements)
{
	unsigned int i;

	if (NULL == tt_array)
		return;

	for (i = 0; i < n_elements; ++i)
		vbi_top_title_destroy (tt_array + i);

	vbi_free (tt_array);
}

/**
 * @param tt vbi_top_title structure, can be @c NULL.
 *
 * Creates a deep copy of @a tt in a newly allocated vbi_top_title
 * structure. If @a tt is @c NULL this function works like
 * vbi_top_title_new().
 *
 * @returns
 * On success the function returns a pointer to the allocated
 * vbi_top_title structure. If insufficent memory is available it
 * returns @c NULL and sets the errno variable to @c ENOMEM.
 *
 * @since 0.3.1
 */
vbi_top_title *
vbi_top_title_dup		(const vbi_top_title *	tt)
{
	vbi_top_title *new_tt;

	new_tt = vbi_malloc (sizeof (*new_tt));
	if (unlikely (NULL == new_tt))
		return NULL;

	if (unlikely (!vbi_top_title_copy (new_tt, tt))) {
		int saved_errno = errno;

		free (new_tt);
		new_tt = NULL;

		errno = saved_errno;
	}

	return new_tt;
}

/**
 * @param tt vbi_top_title structure allocated with
 *   vbi_top_title_new() or vbi_top_title_dup(), can be @c NULL.
 *
 * Frees all resources associated with @a tt, and the structure
 * itself.
 *
 * @since 0.3.1
 */
void
vbi_top_title_delete		(vbi_top_title *	tt)
{
	if (NULL != tt) {
		vbi_top_title_destroy (tt);
		free (tt);
	}
}

/**
 * Allocates a vbi_top_title structure and initializes all fields.
 *
 * @returns
 * On success the function returns a pointer to the allocated
 * vbi_top_title structure. If insufficent memory is available it
 * returns @c NULL and sets the errno variable to @c ENOMEM.
 *
 * @since 0.3.1
 */
vbi_top_title *
vbi_top_title_new		(void)
{
	return vbi_top_title_dup (NULL);
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
