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

/* $Id: search.h,v 1.2.2.3 2004-07-09 16:10:54 mschimek Exp $ */

#ifndef SEARCH_H
#define SEARCH_H

#include "vbi_decoder.h"
#ifndef VBI_DECODER
#define VBI_DECODER
//typedef struct vbi_decoder vbi_decoder;
#endif

/* Public */

#include <stdarg.h>
#include "format.h"

/**
 * @ingroup Search
 * Return codes of the vbi_search_next() function.
 */
typedef enum {
	/**
	 * Some error occured, condition unclear. Call vbi_search_delete().
	 */
	VBI_SEARCH_ERROR = -3,
	/**
	 * No pages in the cache, @a pg is invalid.
	 */
	VBI_SEARCH_CACHE_EMPTY,
	/**
	 * The search has been aborted by the progress callback function.
	 * @a pg points to the current page as in success case, except
	 * no text is highlighted. Another vbi_search_next() call
	 * continues from this page.
	 */
	VBI_SEARCH_ABORTED,
	/**
	 * Pattern not found, @a pg is invalid. Another vbi_search_next()
	 * call restarts from the original starting point.
	 */
	VBI_SEARCH_NOT_FOUND = 0,
	/**
	 * Pattern found. @a pg points to the page ready for display with
	 * the found pattern highlighted.
	 */
	VBI_SEARCH_SUCCESS
} vbi_search_status;

/**
 * @ingroup Search
 * @brief Search context.
 *
 * The contents of this structure are private.
 *
 * Call vbi_search_new_ucs2() or vbi_search_new_utf8() to
 * allocate a search context.
 */
typedef struct vbi_search vbi_search;

/**
 * @ingroup Search
 * @param search Search context passed to vbi_search_next().
 * @param pg Current page being searched, formatted as requested
 *   with vbi_search_next(). Do not modify or free this page, if
 *   necessary you can call vbi_page_copy().
 * @param user_data User data pointer passed to vbi_search_next().
 *
 * Search progress callback function.
 *
 * @returns:
 * @c FALSE to abort the search, then vbi_search_next() will return
 * @c VBI_SEARCH_ABORTED.
 */
typedef vbi_bool
vbi_search_progress_cb		(vbi_search *		search,
				 const vbi_page *	pg,
				 void *			user_data);

/**
 * @addtogroup Search
 * @{
 */
extern vbi_search *
vbi_search_new_ucs2		(vbi_decoder *		vbi,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 const uint16_t *	pattern,
				 unsigned int		pattern_size,
				 vbi_bool		casefold,
				 vbi_bool		regexp,
				 vbi_search_progress_cb *progress,
				 void *			user_data);
extern vbi_search *
vbi_search_new_utf8		(vbi_decoder *		vbi,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 const char *		pattern,
				 vbi_bool		casefold,
				 vbi_bool		regexp,
				 vbi_search_progress_cb *progress,
				 void *			user_data);
extern void
vbi_search_delete		(vbi_search *		search);
extern vbi_search_status
vbi_search_next_va_list		(vbi_search *		search,
				 vbi_page **		pg,
				 int			dir,
				 va_list		format_options);
extern vbi_search_status
vbi_search_next			(vbi_search *		search,
				 vbi_page **		pg,
				 int			dir,
				 ...);
/** @} */

/* Private */

#endif /* SEARCH_H */
