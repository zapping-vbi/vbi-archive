/*
 *  libzvbi - Teletext page cache
 *
 *  Copyright (C) 2001-2003 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig
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

/* $Id: cache-priv.h,v 1.1.2.1 2004-05-01 13:51:35 mschimek Exp $ */

#ifndef CACHE_PRIV_H
#define CACHE_PRIV_H

#include "cache.h"
#include "vt.h"			/* vt_page, vt_network */

extern void
_vbi_cache_release_page		(vbi_cache *		ca,
				 const vt_page *	vtp);
extern const vt_page *
_vbi_cache_dup_page_ref		(vbi_cache *		ca,
				 const vt_page *	vtp);
extern const vt_page *
_vbi_cache_get_page		(vbi_cache *		ca,
				 const vt_network *	vtn,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 vbi_subno		subno_mask,
				 vbi_bool		user_access);
extern const vt_page *
_vbi_cache_put_page		(vbi_cache *		ca,
				 const vt_network *	vtn,
				 const vt_page *	vtp,
				 vbi_bool		new_ref);
extern void
_vbi_cache_release_network	(vbi_cache *		ca,
				 const vt_network *	vtn);
extern vt_network *
_vbi_cache_get_network		(vbi_cache *		ca,
				 vbi_nuid		client_nuid);
extern vt_network *
_vbi_cache_new_network		(vbi_cache *		ca,
				 vbi_nuid		client_nuid);

#endif /* CACHE_PRIV_H */
