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

/* $Id: top_title.h,v 1.1 2004-11-03 06:49:31 mschimek Exp $ */

#ifndef __ZVBI3_TOP_TITLE_H__
#define __ZVBI3_TOP_TITLE_H__

#include <stdarg.h>
#include "bcd.h"		/* vbi3_pgno, vbi3_subno */

VBI3_BEGIN_DECLS

/**
 * DOCUMENT ME
 */
typedef struct {
	/** */
	char *			title;
	/** */
	vbi3_pgno		pgno;
	/** */
	vbi3_subno		subno;
	/** */
	vbi3_bool		group;
	int			reserved[2];
} vbi3_top_title;

extern void
vbi3_top_title_destroy		(vbi3_top_title *	tt);
extern vbi3_bool
vbi3_top_title_copy		(vbi3_top_title *	dst,
				 const vbi3_top_title *	src);
extern void
vbi3_top_title_init		(vbi3_top_title *	tt);
extern void
vbi3_top_title_array_delete	(vbi3_top_title *	tt,
				 unsigned int		n_elements);

VBI3_END_DECLS

#endif /* __ZVBI3_TOP_TITLE_H__ */
