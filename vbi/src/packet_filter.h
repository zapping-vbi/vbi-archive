/*
 *  libzvbi
 *
 *  Copyright (C) 2006 Michael H. Schimek
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

/* $Id: packet_filter.h,v 1.1.2.2 2006-05-18 16:49:19 mschimek Exp $ */

#ifndef __ZVBI3_PACKET_FILTER_H__
#define __ZVBI3_PACKET_FILTER_H__

#include "macros.h"
#include "bcd.h"
#include "sliced.h"		/* vbi3_sliced, vbi3_service_set */

VBI3_BEGIN_DECLS

typedef struct _vbi3_packet_filter vbi3_packet_filter;

typedef vbi3_bool
vbi3_packet_filter_cb		(vbi3_packet_filter *	pf,
				 const vbi3_sliced *	sliced,
				 unsigned int		n_lines,
				 void *			user_data);

extern void
vbi3_packet_filter_keep_system_pages
				(vbi3_packet_filter *	pf,
				 vbi3_bool		keep)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_packet_filter_keep_pages	(vbi3_packet_filter *	pf,
				 vbi3_pgno		first_pgno,
				 vbi3_pgno		last_pgno)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_packet_filter_keep_subpages
				(vbi3_packet_filter *	pf,
				 vbi3_pgno		pgno,
				 vbi3_subno		first_subno,
				 vbi3_subno		last_subno)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_service_set
vbi3_packet_filter_keep_services
				(vbi3_packet_filter *	pf,
				 vbi3_service_set	services)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_packet_filter_drop_pages	(vbi3_packet_filter *	pf,
				 vbi3_pgno		first_pgno,
				 vbi3_pgno		last_pgno)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_packet_filter_drop_subpages
				(vbi3_packet_filter *	pf,
				 vbi3_pgno		pgno,
				 vbi3_subno		first_subno,
				 vbi3_subno		last_subno)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_service_set
vbi3_packet_filter_drop_services
				(vbi3_packet_filter *	pf,
				 vbi3_service_set	services)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_packet_filter_reset	(vbi3_packet_filter *	pf)
  __attribute__ ((_vbi3_nonnull (1)));
extern vbi3_bool
vbi3_packet_filter_cor		(vbi3_packet_filter *	pf,
				 vbi3_sliced *		sliced_out,
				 unsigned int *		n_lines_out,
				 unsigned int		max_lines_out,
				 const vbi3_sliced *	sliced_in,
				 unsigned int 		n_lines_in)
  __attribute__ ((_vbi3_nonnull (1, 2, 3, 5)));
extern vbi3_bool
vbi3_packet_filter_feed		(vbi3_packet_filter *	pf,
				 const vbi3_sliced *	sliced,
				 unsigned int		n_lines)
  __attribute__ ((_vbi3_nonnull (1, 2)));
extern const char *
vbi3_packet_filter_errstr	(vbi3_packet_filter *	pf)
  __attribute__ ((_vbi3_nonnull (1)));
extern void
vbi3_packet_filter_delete	(vbi3_packet_filter *	pf);
extern vbi3_packet_filter *
vbi3_packet_filter_new		(vbi3_packet_filter_cb *callback,
				 void *			user_data)
  __attribute__ ((_vbi3_alloc));

VBI3_END_DECLS

#endif /* __ZVBI3_PACKET_FILTER_H__ */
