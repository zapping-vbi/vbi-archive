/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2000, 2001, 2002 Iñaki García Etxebarria
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
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

/* $Id: page_num.h,v 1.2 2005-09-01 01:40:53 mschimek Exp $ */

#ifndef TELETEXT_PAGE_NUM_H
#define TELETEXT_PAGE_NUM_H

#include <assert.h>
#include "libvbi/bcd.h"		/* vbi3_pgno, vbi3_subno */
#include "libvbi/network.h"	/* vbi3_network */

typedef struct {
  vbi3_network		network;
  vbi3_pgno		pgno;
  vbi3_subno		subno;
} page_num;

vbi3_inline void
network_set			(vbi3_network *		dst,
				 const vbi3_network *	src)
{
  vbi3_bool success;
  
  success = vbi3_network_set (dst, src);
  assert (success);
}

vbi3_inline vbi3_bool
page_num_equal			(const page_num *	ad1,
				 const page_num *	ad2)
{
  return (vbi3_network_equal (&ad1->network, &ad2->network)
	  && ad1->pgno == ad2->pgno
	  && (ad1->subno == ad2->subno
	      || VBI3_ANY_SUBNO == ad1->subno
	      || VBI3_ANY_SUBNO == ad2->subno));
}

vbi3_inline vbi3_bool
page_num_equal2			(const page_num *	ad1,
				 const vbi3_network *	nk2,
				 vbi3_pgno		pgno2,
				 vbi3_subno		subno2)
{
  return (vbi3_network_equal (&ad1->network, nk2)
	  && ad1->pgno == pgno2
	  && (ad1->subno == subno2
	      || VBI3_ANY_SUBNO == ad1->subno
	      || VBI3_ANY_SUBNO == subno2));
}

vbi3_inline void
page_num_destroy		(page_num *		pn)
{
  vbi3_network_destroy (&pn->network);
}

vbi3_inline void
page_num_set			(page_num *		pn,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno)
{
  network_set (&pn->network, nk);
  pn->pgno = pgno;
  pn->subno = subno;
}

#endif /* TELETEXT_PAGE_NUM_H */
