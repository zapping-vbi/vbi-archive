/*
 *  libzvbi - vbi3_page private stuff
 *
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

/* $Id: page-priv.h,v 1.1.2.6 2007-11-01 00:21:24 mschimek Exp $ */

#ifndef PAGE_PRIV_H
#define PAGE_PRIV_H

#include "page.h"		/* vbi3_page, vbi3_opacity */
#include "vt.h"			/* magazine, extension, pagenum */
#include "pdc.h"		/* vbi3_preselection */
#include "lang.h"		/* vbi3_ttx_charset */
#include "cache-priv.h"		/* cache_network, cache_page, vbi3_wst_level */

struct _vbi3_page_priv {
	vbi3_page		pg;

	/* Source network, implicitely reference counted. */
	cache_network *		cn;

	/* Only used in _vbi3_page_priv_from_cache_page_va_list(). */
	const struct magazine *	mag;
	const struct extension *ext;

	/* Source page, reference counted. */
	cache_page *		cp;

	/* _vbi3_page_priv_from_cache_page_va_list() parameter. */
	vbi3_wst_level		max_level;

	/* PDC preselection data, if requested and available. */
	vbi3_preselection *	pdc_table;
	unsigned int		pdc_table_size;

	/* Referenced DRCS download pages. */
	cache_page *		drcs_cp[32];

	/** Default primary and secondary character set. */
	const vbi3_ttx_charset *char_set[2];
	/** 0 header, 1 other rows. */
	vbi3_opacity		page_opacity[2];
	vbi3_opacity		boxed_opacity[2];
	/** TOP navigation. */
	vbi3_link		link[6];
	/**
	 * Points from each character in TOP/FLOF row 25 (max 64 columns)
	 * to a link[] element. -1 if no link.
	 */
	int8_t			link_ref[64];
};

/* in teletext.c */
extern vbi3_bool
_vbi3_page_priv_from_cache_page_va_list
				(vbi3_page_priv *	pgp,
				 cache_page *		cp,
				 va_list		format_options);
extern vbi3_bool
_vbi3_page_priv_from_cache_page
				(vbi3_page_priv *	pgp,
				 cache_page *		cp,
				 ...);
extern void
_vbi3_pdc_title_post_proc	(vbi3_page *		pg,
				 vbi3_preselection *	p);
extern void
_vbi3_page_priv_dump		(const vbi3_page_priv *	pgp,
				 FILE *			fp,
				 unsigned int		mode);
extern void
_vbi3_page_priv_destroy		(vbi3_page_priv *	pgp);
extern void
_vbi3_page_priv_init		(vbi3_page_priv *	pgp);

#endif /* PAGE_PRIV_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
