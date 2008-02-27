/*
 *  libzvbi -- Sliced VBI data filter
 *
 *  Copyright (C) 2006, 2007 Michael H. Schimek
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the 
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301  USA.
 */

/* $Id: sliced_filter.h,v 1.2.2.4 2008-02-27 22:49:44 mschimek Exp $ */

#ifndef __ZVBI3_SLICED_FILTER_H__
#define __ZVBI3_SLICED_FILTER_H__

#include "macros.h"
#include "bcd.h"
#include "sliced.h"		/* vbi3_sliced, vbi3_service_set */

VBI3_BEGIN_DECLS

typedef struct _vbi3_sliced_filter vbi3_sliced_filter;

typedef vbi3_bool
vbi3_sliced_filter_cb		(vbi3_sliced_filter *	sf,
				 const vbi3_sliced *	sliced,
				 unsigned int		n_lines,
				 void *			user_data);

extern vbi3_service_set
vbi3_sliced_filter_keep_services
				(vbi3_sliced_filter *	sf,
				 vbi3_service_set	services)
  _vbi3_nonnull ((1));
extern vbi3_service_set
vbi3_sliced_filter_drop_services
				(vbi3_sliced_filter *	sf,
				 vbi3_service_set	services)
  _vbi3_nonnull ((1));

extern vbi3_bool
vbi3_sliced_filter_keep_ttx_pages
				(vbi3_sliced_filter *	sf,
				 vbi3_pgno		first_pgno,
				 vbi3_pgno		last_pgno)
  _vbi3_nonnull ((1));
extern vbi3_bool
vbi3_sliced_filter_drop_ttx_pages
				(vbi3_sliced_filter *	sf,
				 vbi3_pgno		first_pgno,
				 vbi3_pgno		last_pgno)
  _vbi3_nonnull ((1));
static __inline__ vbi3_bool
vbi3_sliced_filter_keep_ttx_page	(vbi3_sliced_filter *	sf,
				 vbi3_pgno		pgno)
{
	return vbi3_sliced_filter_keep_ttx_pages (sf, pgno, pgno);
}
static __inline__ vbi3_bool
vbi3_sliced_filter_drop_ttx_page	(vbi3_sliced_filter *	sf,
				 vbi3_pgno		pgno)
{
	return vbi3_sliced_filter_drop_ttx_pages (sf, pgno, pgno);
}
extern vbi3_bool
vbi3_sliced_filter_keep_ttx_subpages
				(vbi3_sliced_filter *	sf,
				 vbi3_pgno		pgno,
				 vbi3_subno		first_subno,
				 vbi3_subno		last_subno)
  _vbi3_nonnull ((1));
extern vbi3_bool
vbi3_sliced_filter_drop_ttx_subpages
				(vbi3_sliced_filter *	sf,
				 vbi3_pgno		pgno,
				 vbi3_subno		first_subno,
				 vbi3_subno		last_subno)
  _vbi3_nonnull ((1));
static __inline__ vbi3_bool
vbi3_sliced_filter_keep_ttx_subpage
				(vbi3_sliced_filter *	sf,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno)
{
	return vbi3_sliced_filter_keep_ttx_subpages (sf, pgno, subno, subno);
}
static __inline__ vbi3_bool
vbi3_sliced_filter_drop_ttx_subpage
				(vbi3_sliced_filter *	sf,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno)
{
	return vbi3_sliced_filter_drop_ttx_subpages (sf, pgno, subno, subno);
}
extern void
vbi3_sliced_filter_keep_ttx_system_pages
				(vbi3_sliced_filter *	sf,
				 vbi3_bool		keep)
  _vbi3_nonnull ((1));
extern void
vbi3_sliced_filter_reset	(vbi3_sliced_filter *	sf)
  _vbi3_nonnull ((1));
extern vbi3_bool
vbi3_sliced_filter_cor		(vbi3_sliced_filter *	sf,
				 vbi3_sliced *		sliced_out,
				 unsigned int *		n_lines_out,
				 unsigned int		max_lines_out,
				 const vbi3_sliced *	sliced_in,
				 unsigned int *		n_lines_in)
  _vbi3_nonnull ((1, 2, 3, 5, 6));
extern vbi3_bool
vbi3_sliced_filter_feed		(vbi3_sliced_filter *	sf,
				 const vbi3_sliced *	sliced,
				 unsigned int *		n_lines)
  _vbi3_nonnull ((1, 2, 3));

extern const char *
vbi3_sliced_filter_errstr	(vbi3_sliced_filter *	sf)
  _vbi3_nonnull ((1));
extern void
vbi3_sliced_filter_set_log_fn	(vbi3_sliced_filter *	sf,
				 vbi3_log_mask		mask,
				 vbi3_log_fn *		log_fn,
				 void *			user_data)
  _vbi3_nonnull ((1));
extern void
vbi3_sliced_filter_delete	(vbi3_sliced_filter *	sf);
extern vbi3_sliced_filter *
vbi3_sliced_filter_new		(vbi3_sliced_filter_cb *callback,
				 void *			user_data)
  _vbi3_alloc;

VBI3_END_DECLS

#endif /* __ZVBI3_SLICED_FILTER_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
