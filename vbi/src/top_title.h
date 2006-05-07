/*
 *  libzvbi - Tables Of Pages
 *
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: top_title.h,v 1.1.2.1 2006-05-07 06:04:59 mschimek Exp $ */

#ifndef __ZVBI3_TOP_TITLE_H__
#define __ZVBI3_TOP_TITLE_H__

#include <stdarg.h>
#include "bcd.h"		/* vbi3_pgno, vbi3_subno */

VBI3_BEGIN_DECLS

/**
 * @brief Title of a page in a Table Of Pages array.
 */
typedef struct {
	/** Title of the page. This is a NUL-terminated UTF-8 string. */
	char *			title_;

	/** Page number. */
	vbi3_pgno		pgno;

	/** Subpage number or @c VBI3_ANY_SUBNO. */
	vbi3_subno		subno;

	/**
	 * Whether this is the first page in a group or block. More
	 * information is available through vbi3_cache_get_ttx_page_stat().
	 */
	vbi3_bool		group;

	int			reserved[2];
} vbi3_top_title;

extern void
vbi3_top_title_destroy		(vbi3_top_title *	tt)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_top_title_copy		(vbi3_top_title *	dst,
				 const vbi3_top_title *	src)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_top_title_init		(vbi3_top_title *	tt)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_top_title_array_delete	(vbi3_top_title *	tt,
				 unsigned int		n_elements);

VBI3_END_DECLS

#endif /* __ZVBI3_TOP_TITLE_H__ */
