/*
 *  libzvbi
 *
 *  Copyright (C) 2000, 2001 Michael H. Schimek
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

/* $Id: page-priv.h,v 1.1.2.2 2004-07-16 00:08:18 mschimek Exp $ */

#ifndef PAGE_PRIV_H
#define PAGE_PRIV_H

#include "format.h"		/* vbi_page, vbi_opacity */
#include "vt.h"			/* magazine, extension, pagenum */
#include "pdc.h"		/* vbi_preselection */
#include "lang.h"		/* vbi_character_set */
#include "cache-priv.h"		/* cache_network, cache_page, vbi_wst_level */

struct _vbi_page_priv {
	vbi_page		pg;

	/* Source network, implicitely reference counted. */
	cache_network *		cn;

	/* Only used in _vbi_page_priv_from_cache_page_va_list(). */
	const magazine *	mag;
	const extension *	ext;

	/* Source page, reference counted. */
	cache_page *		cp;

	/* _vbi_page_priv_from_cache_page_va_list() parameter. */
	vbi_wst_level		max_level;

	/* PDC preselection data, if requested and available. */
	vbi_preselection *	pdc_table;
	unsigned int		pdc_table_size;

	/* Referenced DRCS download pages. */
	cache_page *		drcs_cp[32];

	/** Default primary and secondary character set. */
	const vbi_character_set *char_set[2];
	/** 0 header, 1 other rows. */
	vbi_opacity		page_opacity[2];
	vbi_opacity		boxed_opacity[2];
	/** TOP navigation. */
	vbi_link		link[6];
	/**
	 * Points from each character in TOP/FLOF row 25 (max 64 columns)
	 * to a link[] element. -1 if no link.
	 */
	int8_t			link_ref[64];
};

/* in teletext.c */
extern vbi_bool
_vbi_page_priv_from_cache_page_va_list
				(vbi_page_priv *	pgp,
				 cache_page *		cp,
				 va_list		format_options);
extern vbi_bool
_vbi_page_priv_from_cache_page
				(vbi_page_priv *	pgp,
				 cache_page *		cp,
				 ...);
extern void
_vbi_page_priv_dump		(const vbi_page_priv *	pgp,
				 FILE *			fp,
				 unsigned int		mode);
extern void
_vbi_page_priv_destroy		(vbi_page_priv *	pgp);
extern void
_vbi_page_priv_init		(vbi_page_priv *	pgp);

#endif /* PAGE_PRIV_H */
