/*
 *  libzvbi - VBI decoding library
 *
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
 *  Copyright (C) 2000, 2001 Iñaki García Etxebarria
 *
 *  Based on AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
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

/* $Id: vbi.h,v 1.5.2.14 2004-05-12 01:40:44 mschimek Exp $ */

#ifndef VBI_H
#define VBI_H

#include <pthread.h>
#include <stdarg.h>
#include "vt.h"

extern void
vbi_init			(void) __attribute__ ((constructor));
extern void
vbi_version			(unsigned int *		major,
				 unsigned int *		minor,
				 unsigned int *		micro);
/* Private */
extern void
_vbi_asprintf			(char **		errstr,
				 char *			templ,
				 ...);

/*
  vbi_page private stuff (-> format.h, page-priv.h)
 */

struct _vbi_page_private {
	vbi_page		pg;

	const vt_network *	network;

	const magazine *	mag;
	const extension *	ext;

	const vt_page *		vtp;

	vbi_wst_level		max_level;

	vbi_preselection *	pdc_table;
	unsigned int		pdc_table_size;

	const vt_page *		drcs_vtp[32];

	const vbi_character_set *char_set[2];

	/* 0 header, 1 other rows. */
	vbi_opacity		page_opacity[2];
	vbi_opacity		boxed_opacity[2];

	/* Navigation related, see vbi_page_nav_link(). For
	   simplicity nav_index[] points from each character
	   in the TOP/FLOF row 25 (max 64 columns) to the
	   corresponding nav_link element. */
	pagenum		nav_link[6];
	uint8_t			nav_index[64];
};

extern void
vbi_page_private_dump		(const vbi_page_private *pgp,
				 FILE *			fp,
				 unsigned int		mode);

/* teletext.h */

extern vbi_bool
_vbi_page_from_vt_page_va_list	(vbi_page_private *	pgp,
				 vbi_cache *		cache,
				 const vt_network *	vtn,
				 const vt_page *	vtp,
				 va_list		format_options);
extern vbi_bool
_vbi_page_from_vt_page		(vbi_page_private *	pgp,
				 vbi_cache *		cache,
				 const vt_network *	vtn,
				 const vt_page *	vtp,
				 ...);

#endif /* VBI_H */
