/*
 *  libzvbi -- Table of Teletext page numbers
 *
 *  Copyright (C) 2006, 2007 Michael H. Schimek
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

/* $Id: page_table.h,v 1.2.2.2 2007-11-01 00:21:24 mschimek Exp $ */

#ifndef __ZVBI3_PAGE_TABLE_H__
#define __ZVBI3_PAGE_TABLE_H__

#include "macros.h"
#include "bcd.h"

VBI3_BEGIN_DECLS

typedef struct _vbi3_page_table vbi3_page_table;

extern vbi3_bool
vbi3_page_table_contains_all_subpages
				(const vbi3_page_table *pt,
				 vbi3_pgno		pgno)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_page_table_contains_subpage	(const vbi3_page_table *pt,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno)
  __attribute__ ((_vbi3_nonnull (1)));

/**
 * @param pt Teletext page table allocated with vbi3_page_table_new().
 * @param pgno The page number in question. Need not be a valid
 *   Teletext page number.
 *
 * The function returns @c TRUE if any subpages of page @a pgno
 * have been added to the page table.
 */
static __inline__ vbi3_bool
vbi3_page_table_contains_page	(const vbi3_page_table *pt,
				 vbi3_pgno		pgno)
{
	return vbi3_page_table_contains_subpage (pt, pgno, VBI3_ANY_SUBNO);
}

extern vbi3_bool
vbi3_page_table_next_subpage	(const vbi3_page_table *pt,
				 vbi3_pgno *		pgno,
				 vbi3_subno *		subno)
  __attribute__ ((_vbi3_nonnull (1, 2, 3)));
extern vbi3_bool
vbi3_page_table_next_page	(const vbi3_page_table *pt,
				 vbi3_pgno *		pgno)
  __attribute__ ((_vbi3_nonnull (1, 2)));
extern unsigned int
vbi3_page_table_num_pages	(const vbi3_page_table *pt)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_page_table_remove_subpages	(vbi3_page_table *	pt,
				 vbi3_pgno		pgno,
				 vbi3_subno		first_subno,
				 vbi3_subno		last_subno)
  __attribute__ ((_vbi3_nonnull (1)));

/**
 * @param pt Teletext page table allocated with vbi3_page_table_new().
 * @param pgno The page in question. Must be in range 0x100 to 0x8FF
 *   inclusive.
 * @param subno The subpage number to remove. Must be in range 0 to
 *   0x3F7E inclusive, or VBI3_ANY_SUBNO.
 *
 * This function removes subpage @a subno of page @a pgno from
 * the page table. When @a subno is @c VBI3_ANY_SUBNO, it removes
 * the page and all its subpages as vbi3_page_table_remove_page() does.
 *
 * @a returns
 * @c FALSE on failure (invalid page or subpage number or out of memory).
 */
static __inline__ vbi3_bool
vbi3_page_table_remove_subpage	(vbi3_page_table *	pt,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno)
{
	return vbi3_page_table_remove_subpages (pt, pgno, subno, subno);
}

extern vbi3_bool
vbi3_page_table_add_subpages	(vbi3_page_table *	pt,
				 vbi3_pgno		pgno,
				 vbi3_subno		first_subno,
				 vbi3_subno		last_subno)
  __attribute__ ((_vbi3_nonnull (1)));

/**
 * @param pt Teletext page table allocated with vbi3_page_table_new().
 * @param pgno The page in question. Must be in range 0x100 to 0x8FF
 *   inclusive.
 * @param subno The subpage number to add. Must be in range 0 to 0x3F7E
 *   inclusive, or VBI3_ANY_SUBNO.
 *
 * This function adds subpage @a subno of page @a pgno to
 * the page table. When @a subno is @c VBI3_ANY_SUBNO, it adds
 * all subpages of page @a pgno as vbi3_page_table_add_page() does.
 *
 * @a returns
 * @c FALSE on failure (invalid page or subpage number or out of memory).
 */
static __inline__ vbi3_bool
vbi3_page_table_add_subpage	(vbi3_page_table *	pt,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno)
{
	return vbi3_page_table_add_subpages (pt, pgno, subno, subno);
}

extern vbi3_bool
vbi3_page_table_remove_pages	(vbi3_page_table *	pt,
				 vbi3_pgno		first_pgno,
				 vbi3_pgno		last_pgno)
  __attribute__ ((_vbi3_nonnull (1)));

/**
 * @param pt Teletext page table allocated with vbi3_page_table_new().
 * @param pgno The page to remove. Must be in range 0x100 to 0x8FF
 *   inclusive.
 *
 * This function removes page @a pgno and all its subpages from the
 * page table.
 *
 * @a returns
 * @c FALSE on failure (invalid page number or out of memory).
 */
static __inline__ vbi3_bool
vbi3_page_table_remove_page	(vbi3_page_table *	pt,
				 vbi3_pgno		pgno)
{
	return vbi3_page_table_remove_pages (pt, pgno, pgno);
}

extern vbi3_bool
vbi3_page_table_add_pages	(vbi3_page_table *	pt,
				 vbi3_pgno		first_pgno,
				 vbi3_pgno		last_pgno)
  __attribute__ ((_vbi3_nonnull (1)));

/**
 * @param pt Teletext page table allocated with vbi3_page_table_new().
 * @param pgno The page to add. Must be in range 0x100 to 0x8FF
 *   inclusive.
 *
 * This function adds page @a pgno and all its subpages to the
 * page table.
 *
 * Note if a Teletext page has subpages the transmitted subpage number
 * is generally in range 1 to N. For pages without subpages (just a
 * single page) the subpage number zero is transmitted.
 *
 * Use this function to match page @a pgno regardless if it has
 * subpages. Do not explicitely add subpage zero of page @a pgno
 * (with vbi3_page_table_add_subpage()) unless you want to match
 * @a pgno only if it has no subpages, as subpage lookups are
 * considerably less efficient.
 *
 * @a returns
 * @c FALSE on failure (invalid page number or out of memory).
 */
static __inline__ vbi3_bool
vbi3_page_table_add_page		(vbi3_page_table *	pt,
				 vbi3_pgno		pgno)
{
	return vbi3_page_table_add_pages (pt, pgno, pgno);
}

extern void
vbi3_page_table_remove_all_pages	(vbi3_page_table *	pt)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_page_table_add_all_displayable_pages
				(vbi3_page_table *	pt)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_page_table_add_all_pages	(vbi3_page_table *	pt)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_page_table_delete		(vbi3_page_table *	pt);
extern vbi3_page_table *
vbi3_page_table_new		(void);

VBI3_END_DECLS

#endif /* __ZVBI3_PAGE_TABLE_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
