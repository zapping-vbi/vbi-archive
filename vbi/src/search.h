/*
 *  libzvbi - Teletext page cache search functions
 *
 *  Copyright (C) 2000, 2001 Michael H. Schimek
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

/* $Id: search.h,v 1.2.2.6 2008-02-25 20:59:27 mschimek Exp $ */

#ifndef __ZVBI3_SEARCH_H__
#define __ZVBI3_SEARCH_H__

#include <stdarg.h>		/* va_list */
#include "page.h"

VBI3_BEGIN_DECLS

/**
 * @ingroup Search
 * Return codes of the vbi3_search_next() function.
 */
typedef enum {
	/**
	 * Some error occured, condition unclear. Call vbi3_search_delete().
	 */
	VBI3_SEARCH_ERROR = -3,
	/**
	 * No pages in the cache, @a pg is invalid.
	 */
	VBI3_SEARCH_CACHE_EMPTY,
	/**
	 * The search has been aborted by the progress callback function.
	 * @a pg points to the current page as in success case, except
	 * no text is highlighted. Another vbi3_search_next() call
	 * continues from this page.
	 */
	VBI3_SEARCH_ABORTED,
	/**
	 * Pattern not found, @a pg is invalid. Another vbi3_search_next()
	 * call restarts from the original starting point.
	 */
	VBI3_SEARCH_NOT_FOUND = 0,
	/**
	 * Pattern found. @a pg points to the page ready for display with
	 * the found pattern highlighted.
	 */
	VBI3_SEARCH_SUCCESS
} vbi3_search_status;

/**
 * @ingroup Search
 * @brief Search context.
 *
 * The contents of this structure are private.
 *
 * Call vbi3_search_new_ucs2() or vbi3_search_new_utf8() to
 * allocate a search context.
 */
typedef struct _vbi3_search vbi3_search;

/**
 * @ingroup Search
 * @param search Search context passed to vbi3_search_next().
 * @param pg Current page being searched, formatted as requested
 *   with vbi3_search_next(). Do not modify or free this page, if
 *   necessary you can call vbi3_page_copy().
 * @param user_data User data pointer passed to vbi3_search_next().
 *
 * Search progress callback function.
 *
 * @returns:
 * @c FALSE to abort the search, then vbi3_search_next() will return
 * @c VBI3_SEARCH_ABORTED.
 */
typedef vbi3_bool
vbi3_search_progress_cb		(vbi3_search *		search,
				 const vbi3_page *	pg,
				 void *			user_data);

/**
 * @addtogroup Search
 * @{
 */
extern vbi3_search_status
vbi3_search_next_va_list	(vbi3_search *		s,
				 const vbi3_page **	pg,
				 int			dir,
				 va_list		format_options)
  _vbi3_nonnull ((1, 2));
extern vbi3_search_status
vbi3_search_next		(vbi3_search *		s,
				 const vbi3_page **	pg,
				 int			dir,
				 ...)
  _vbi3_nonnull ((1, 2)) _vbi3_sentinel;
extern void
vbi3_search_delete		(vbi3_search *		s);
extern vbi3_search *
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
  _vbi3_alloc _vbi3_nonnull ((1, 2, 5));
extern vbi3_search *
vbi3_search_utf8_new		(vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 const char *		pattern,
				 vbi3_bool		casefold,
				 vbi3_bool		regexp,
				 vbi3_search_progress_cb *progress,
				 void *			user_data)
  _vbi3_alloc _vbi3_nonnull ((1, 2, 5));
/** @} */

VBI3_END_DECLS

#endif /* __ZVBI3_SEARCH_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
