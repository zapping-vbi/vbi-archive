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

/* $Id: cache.h,v 1.2.2.9 2004-05-01 13:51:35 mschimek Exp $ */

#ifndef __ZVBI_CACHE_H__
#define __ZVBI_CACHE_H__

#include "network.h"		/* vbi_nuid */
#include "macros.h"		/* vbi_bool */

VBI_BEGIN_DECLS

typedef struct _vbi_cache vbi_cache;

typedef void
vbi_lock_fn			(void *			user_data);
typedef void
vbi_unlock_fn			(void *			user_data);

extern void
_vbi_cache_purge_by_nuid	(vbi_cache *		ca,
				 vbi_nuid		nuid);
extern void
_vbi_cache_purge		(vbi_cache *		ca);
extern void
_vbi_cache_normal_network	(vbi_cache *		ca,
				 vbi_nuid		nuid,
				 vbi_bool		force);
extern void
_vbi_cache_current_network	(vbi_cache *		ca,
				 vbi_nuid		nuid,
				 vbi_bool		exclusive);
extern void
_vbi_cache_set_lock_functions	(vbi_cache *		ca,
				 vbi_lock_fn *		lock,
				 vbi_unlock_fn *	unlock,
				 void *			user_data);
extern void
_vbi_cache_set_memory_limit	(vbi_cache *		ca,
				 unsigned int		limit);
extern void
_vbi_cache_set_network_limit	(vbi_cache *		ca,
				 unsigned int		limit);
extern vbi_cache *
_vbi_cache_dup_ref		(vbi_cache *		ca);
extern void
vbi_cache_delete		(vbi_cache *		ca);
extern vbi_cache *
vbi_cache_new			(void) vbi_alloc;

VBI_END_DECLS

#endif /* __ZVBI_CACHE_H__ */
