/*
 *  libzvbi -- Teletext Tables Of Pages
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

/* $Id: top_title.h,v 1.1.2.1 2008-08-22 07:59:58 mschimek Exp $ */

#ifndef __ZVBI_TOP_TITLE_H__
#define __ZVBI_TOP_TITLE_H__

#include <stdarg.h>

#include "zvbi/bcd.h"		/* vbi_pgno, vbi_subno */
#include "zvbi/ttx_page_stat.h"	/* vbi_ttx_page_type */

VBI_BEGIN_DECLS

/**
 * @brief Title of a page in a Table Of Pages array.
 */
typedef struct {
	/**
	 * Title of the page. This is a NUL-terminated string in
	 * locale encoding. The maximum length is 12 characters.
	 */
	char *			title;

	/** Page number. */
	vbi_pgno		pgno;

	/** Subpage number or @c VBI_ANY_SUBNO. */
	vbi_subno		subno;

	/**
	 * Whether this is the first page in a group (VBI_TOP_GROUP)
	 * or block (VBI_TOP_BLOCK) of pages. Blocks are superior in
	 * the hierarchy.
	 */
	vbi_ttx_page_type	type;

	int			_reserved[2];
} vbi_top_title;

extern vbi_bool
vbi_top_title_copy		(vbi_top_title *	dst,
				 const vbi_top_title *	src)
  _vbi_nonnull ((1));
extern vbi_bool
vbi_top_title_destroy		(vbi_top_title *	tt)
  _vbi_nonnull ((1));
extern vbi_bool
vbi_top_title_init		(vbi_top_title *	tt)
  _vbi_nonnull ((1));
extern vbi_top_title *
vbi_top_title_array_dup		(vbi_top_title *	tt,
				 unsigned int		n_elements)
  _vbi_nonnull ((1));
extern void
vbi_top_title_array_delete	(vbi_top_title *	tt,
				 unsigned int		n_elements);
extern vbi_top_title *
vbi_top_title_dup		(const vbi_top_title *	tt)
  _vbi_alloc;
extern void
vbi_top_title_delete		(vbi_top_title *	tt)
  _vbi_nonnull ((1));
extern vbi_top_title *
vbi_top_title_new		(void)
  _vbi_alloc;

VBI_END_DECLS

#endif /* __ZVBI_TOP_TITLE_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
