/*
 *  libzvbi -- Links
 *
 *  Copyright (C) 2001-2008 Michael H. Schimek
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

/* $Id: link.c,v 1.1.2.2 2008-08-22 07:59:06 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "misc.h"
#include "link.h"

/**
 */
const char *
_vbi_link_type_name		(vbi_link_type		type)
{
#undef CASE
#define CASE(type) case VBI_LINK_##type : return #type ;

	switch (type) {
	CASE (NONE)
	CASE (MESSAGE)
	CASE (TTX_PAGE)
	CASE (HTTP)
	CASE (FTP)
	CASE (EMAIL)
	}

#undef CASE

	errno = VBI_ERR_INVALID_LINK_TYPE;

	return NULL;
}

/** @internal */
vbi_bool
_vbi_link_dump			(const vbi_link *	lk,
				 FILE *			fp)
{
	const char *type_name;
	struct tm tm;

	_vbi_null_check (lk, FALSE);
	_vbi_null_check (fp, FALSE);

	type_name = _vbi_link_type_name (lk->type);
	if (NULL == type_name)
		return FALSE;

	if (NULL != lk->name) {
		fprintf (fp, "%s name='%s' ",
			 type_name, lk->name);
	} else {
		fprintf (fp, "%s name=NULL ",
			 type_name);
	}

	switch (lk->type) {
	case VBI_LINK_TTX_PAGE:
		if (NULL != lk->lk.ttx.network) {
			fputs ("network={", fp);
			_vbi_network_dump (lk->lk.ttx.network, fp); 
			fprintf (fp, "} pgno=%X subno=%X ",
				 lk->lk.ttx.pgno,
				 lk->lk.ttx.subno);
		} else {
			fprintf (fp, "network=NULL pgno=%X subno=%X ",
				 lk->lk.ttx.pgno,
				 lk->lk.ttx.subno);
		}
		break;

	case VBI_LINK_HTTP:
	case VBI_LINK_FTP:
	case VBI_LINK_EMAIL:
		if (NULL != lk->lk.web.url)
			fprintf (fp, "url='%s' ", lk->lk.web.url);
		else
			fputs ("url=NULL ", fp);
		if (NULL != lk->lk.web.script)
			fprintf (fp, "script='%s' ", lk->lk.web.script);
		else
			fputs ("script=NULL ", fp);
		break;

	default:
		break;
	}

	if (NULL != gmtime_r (&lk->expiration_time, &tm)) {
		fprintf (fp, "expires=%04u-%02u-%02uT%02u:%02u:%02uZ",
			 tm.tm_year + 1900,
			 tm.tm_mon + 1,
			 tm.tm_mday,
			 tm.tm_hour,
			 tm.tm_min,
			 tm.tm_sec);
	} else {
		fputs ("expires=INVALID", fp);
	}

	return !ferror (fp);
}

/**
 * @param lk vbi_link structure initialized with
 *   vbi_link_init() or vbi_link_copy().
 *
 * Frees all resources associated with @a lk, except for the structure
 * itself.
 *
 * @returns
 * The function returns @c FALSE and sets the errno variable to
 * @c VBI_ERR_NULL_ARG if @a lk is a @c NULL pointer.
 *
 * @since 0.3.1
 */
vbi_bool
vbi_link_destroy		(vbi_link *		lk)
{
	_vbi_null_check (lk, FALSE);

	vbi_free (lk->name);

	switch (lk->type) {
	case VBI_LINK_TTX_PAGE:
		vbi_network_delete (lk->lk.ttx.network);
		break;

	case VBI_LINK_HTTP:
	case VBI_LINK_FTP:
	case VBI_LINK_EMAIL:
		vbi_free (lk->lk.web.url);
		vbi_free (lk->lk.web.script);
		break;

	default:
		errno = VBI_ERR_INVALID_LINK_TYPE;
		return FALSE;
	}

	CLEAR (*lk);

	return TRUE;
}

/**
 * @param dst A copy of @a src will be stored here.
 * @param src vbi_link to be copied, can be @c NULL.
 *
 * Creates a deep copy of @a src in @a dst, overwriting the data
 * previously stored at @a dst. If @a src is @c NULL the function has
 * the same effect as vbi_link_init(). It @a src == @a dst the
 * function does nothing.
 *
 * @returns
 * On failure @a *dst remains unmodified, the function returns @c FALSE
 * and sets the errno variable to indicate the error:
 * - @c ENOMEM - insufficient memory.
 * - @c VBI_ERR_NULL_ARG - @a dst is a @c NULL pointer.
 * - @c VBI_ERR_INVALID_LINK_TYPE - @a src->type is invalid.
 *
 * @since 0.3.1
 */
vbi_bool
vbi_link_copy			(vbi_link *		dst,
				 const vbi_link *	src)
{
	vbi_link new_link;
	int saved_errno;

	_vbi_null_check (dst, FALSE);

	CLEAR (new_link);

	if (NULL == src)
		goto finish;

	new_link.type = src->type;

	switch (src->type) {
	case VBI_LINK_NONE:
		goto finish;

	case VBI_LINK_MESSAGE:
		break;

	case VBI_LINK_TTX_PAGE:
		if (NULL != src->lk.ttx.network) {
			new_link.lk.ttx.network =
				vbi_network_dup (src->lk.ttx.network);
			if (unlikely (NULL == new_link.lk.ttx.network))
				goto failed;
		}
		new_link.lk.ttx.pgno = src->lk.ttx.pgno;
		new_link.lk.ttx.subno = src->lk.ttx.subno;
		break;

	case VBI_LINK_HTTP:
	case VBI_LINK_FTP:
	case VBI_LINK_EMAIL:
		if (NULL != src->lk.web.url) {
			new_link.lk.web.url = strdup (src->lk.web.url);
			if (unlikely (NULL == new_link.lk.web.url))
				goto failed;
		}
		if (VBI_LINK_EMAIL != src->type
		    && NULL != src->lk.web.script) {
			new_link.lk.web.script =
				strdup (src->lk.web.script);
			if (unlikely (NULL == new_link.lk.web.script))
				goto failed;
		}
		break;

	default:
		errno = VBI_ERR_INVALID_LINK_TYPE;
		return FALSE;
	}

	if (NULL != src->name) {
		new_link.name = strdup (src->name);
		if (unlikely (NULL == new_link.name))
			goto failed;
	}

	new_link.expiration_time = src->expiration_time;

 finish:
	*dst = new_link;

	return TRUE;

 failed:
	saved_errno = errno;
	vbi_link_destroy (&new_link);
	errno = saved_errno;

	return FALSE;
}

/**
 * @param lk Pointer to the vbi_link structure to be initialized.
 *
 * Initializes all fields of @a lk to zero.
 *
 * @returns
 * The function returns @c FALSE and sets the errno variable to
 * @c VBI_ERR_NULL_ARG if @a lk is a @c NULL pointer.
 *
 * @since 0.3.1
 */
vbi_bool
vbi_link_init		(vbi_link *		lk)
{
	return vbi_link_copy (lk, NULL);
}

/**
 * @param lk vbi_link structure, can be @c NULL.
 *
 * Creates a deep copy of @a lk in a newly allocated vbi_link
 * structure. If @a lk is @c NULL this function works like
 * vbi_link_new().
 *
 * @returns
 * On success the function returns a pointer to the allocated
 * vbi_link structure. On failure it returns @c NULL and sets
 * the errno variable to indicate the error:
 * - @c ENOMEM - insufficient memory.
 * - @c VBI_ERR_INVALID_LINK_TYPE - @a lk->type is invalid.
 *
 * @since 0.3.1
 */
vbi_link *
vbi_link_dup		(const vbi_link *	lk)
{
	vbi_link *new_lk;

	new_lk = vbi_malloc (sizeof (*new_lk));
	if (unlikely (NULL == new_lk))
		return NULL;

	if (unlikely (!vbi_link_copy (new_lk, lk))) {
		int saved_errno = errno;

		free (new_lk);
		new_lk = NULL;

		errno = saved_errno;
	}

	return new_lk;
}

/**
 * @param lk vbi_link structure allocated with
 *   vbi_link_new() or vbi_link_dup(), can be @c NULL.
 *
 * Frees all resources associated with @a lk, and the structure
 * itself.
 *
 * @since 0.3.1
 */
void
vbi_link_delete		(vbi_link *		lk)
{
	if (NULL != lk) {
		vbi_link_destroy (lk);
		free (lk);
	}
}

/**
 * @internal
 */
vbi_link *
_vbi_ttx_link_new		(const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno)
{
	vbi_link *lk;

	_vbi_null_check (nk, NULL);

	lk = vbi_malloc (sizeof (*lk));
	if (NULL == lk) {
		return NULL;
	}

	CLEAR (*lk);

	if (NULL != nk) {
		lk->lk.ttx.network = vbi_network_dup (nk);
		if (unlikely (NULL == lk->lk.ttx.network)) {
			int saved_errno = errno;

			vbi_free (lk);
			errno = saved_errno;
			return NULL;
		}
	}

	lk->type = VBI_LINK_TTX_PAGE;

	lk->lk.ttx.pgno = pgno;
	lk->lk.ttx.subno = subno;

	lk->expiration_time = TIME_MAX;

	return lk;
}

/**
 * Allocates a vbi_link structure and initializes all fields
 * to zero.
 *
 * @returns
 * On success the function returns a pointer to the allocated
 * vbi_link structure. If insufficent memory is available it
 * returns @c NULL and sets the errno variable to @c ENOMEM.
 *
 * @since 0.3.1
 */
vbi_link *
vbi_link_new			(void)
{
	return vbi_link_dup (NULL);
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
