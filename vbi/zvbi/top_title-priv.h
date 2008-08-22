/*
 *  libzvbi -- Teletext Table Of Pages
 *
 *  Copyright (C) 2004, 2008 Michael H. Schimek
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

/* $Id: top_title-priv.h,v 1.1.2.1 2008-08-22 07:59:50 mschimek Exp $ */

#ifndef TOP_TITLE_PRIV_H
#define TOP_TITLE_PRIV_H

#include "cache-priv.h"
#include "top_title.h"

extern const struct ttx_ait_title *
_vbi_cn_get_ait_title		(cache_network *	cn,
				 cache_page **		ait_cp,
				 vbi_pgno		pgno,
				 vbi_subno		subno)
  _vbi_nonnull ((1, 2));
extern vbi_bool
_vbi_top_title_from_ait_title	(vbi_top_title *	tt,
				 const char *		codeset,
				 const cache_network *	cn,
				 const struct ttx_ait_title *ait,
				 const vbi_ttx_charset *cs)
  _vbi_nonnull ((1, 2, 3, 4, 5));
extern vbi_top_title *
_vbi_cn_get_top_title		(cache_network *	cn,
				 vbi_pgno		pgno,
				 vbi_subno		subno)
  _vbi_nonnull ((1));
extern vbi_bool
_vbi_cn_get_top_titles		(cache_network *	cn,
				 vbi_top_title **	tt_array_p,
				 unsigned int *		n_elements_p)
  _vbi_nonnull ((1, 2, 3));

#endif /* TOP_TITLE_PRIV_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
