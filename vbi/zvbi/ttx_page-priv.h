/*
 *  libzvbi -- vbi_page private stuff
 *
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
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

/* $Id: ttx_page-priv.h,v 1.1.2.2 2008-08-20 12:33:59 mschimek Exp $ */

#ifndef PAGE_PRIV_H
#define PAGE_PRIV_H

#include "ttx.h"
#include "ttx_charset.h"
#include "ttx_decoder.h"
#include "cache-priv.h"

#define TTX_PAGE_MAGIC 0x2D04AE14

struct ttx_page {
	vbi_page			pg;

	/* Source network, reference counted. */
	cache_network *			cn;

	/* Only used in _vbi_page_priv_from_cache_page_va(). */
	const struct ttx_magazine *	mag;
	const struct ttx_extension *	ext;

	/* Source page, reference counted. */
	cache_page *			cp;

	/* _vbi_page_priv_from_cache_page_va() parameter. */
	vbi_ttx_level			max_level;
#if 0
	/* PDC preselection data, if requested and available. */
	vbi_preselection *		pdc_table;
	unsigned int			pdc_table_size;
#endif
	/* Referenced DRCS download pages. */
	cache_page *			drcs_cp[32];

	/** Default primary and secondary character set. */
	const vbi_ttx_charset *		char_set[2];

	/** 0 header, 1 other rows. */
	vbi_opacity			page_opacity[2];
	vbi_opacity			boxed_opacity[2];

	/** TOP navigation. */
#if 0
	vbi_link			link[6];
#endif
	/**
	 * Points from each character in TOP/FLOF row 25 (max 64 columns)
	 * to a link[] element. -1 if no link.
	 */
	int8_t			link_ref[64];
};

extern vbi_bool
_vbi_ttx_page_from_cache_page_va
				(struct ttx_page *	tp,
				 cache_page *		cp,
				 va_list		format_options)
  _vbi_nonnull ((1, 2));
extern vbi_bool
_vbi_ttx_page_dump		(const struct ttx_page *tp,
				 FILE *			fp,
				 unsigned int		mode)
  _vbi_nonnull ((1, 2));
extern void
_vbi_ttx_page_destroy		(struct ttx_page *	tp)
  _vbi_nonnull ((1));
extern void
_vbi_ttx_page_init		(struct ttx_page *	tp)
  _vbi_nonnull ((1));
extern struct ttx_page *
_vbi_ttx_page_new		(void)
  _vbi_alloc;

#endif /* PAGE_PRIV_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
