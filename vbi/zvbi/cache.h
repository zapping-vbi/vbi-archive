/*
 *  libzvbi -- Teletext cache
 *
 *  Copyright (C) 2001, 2002, 2003, 2004, 2007 Michael H. Schimek
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

/* $Id: cache.h,v 1.1.2.1 2008-08-19 10:56:02 mschimek Exp $ */

#ifndef __ZVBI_CACHE_H__
#define __ZVBI_CACHE_H__

#include "zvbi/macros.h"

VBI_BEGIN_DECLS

typedef struct _vbi_cache vbi_cache;

extern void
vbi_cache_set_memory_limit	(vbi_cache *		ca,
				 unsigned long		limit)
  _vbi_nonnull ((1));
extern void
vbi_cache_unref			(vbi_cache *		ca);
extern vbi_cache *
vbi_cache_ref			(vbi_cache *		ca)
  _vbi_nonnull ((1));
extern void
vbi_cache_delete		(vbi_cache *		ca);
extern vbi_cache *
vbi_cache_new			(void)
  _vbi_alloc;

VBI_END_DECLS

#endif /* __ZVBI_CACHE_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
