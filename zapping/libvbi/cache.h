/*
 *  Zapzilla/libvbi - Teletext Page Cache
 *
 *  Copyright (C) 2001 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998,1999 Edgar Toernig (froese@gmx.de)
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

/* $Id: cache.h,v 1.12 2001-09-02 03:25:58 mschimek Exp $ */

#ifndef CACHE_H
#define CACHE_H

#include "../common/list.h"
#include "vt.h"

#define HASH_SIZE 113

struct cache {
	/* TODO: thread safe */
	list			hash[HASH_SIZE];

	int			npages;

	/* TODO */
	unsigned long		mem_used;
	unsigned long		mem_max;

	/* TODO: multi-station cache */
};

typedef int foreach_callback(void *, struct vt_page *, int); 

extern void             vbi_cache_init(struct vbi *);
extern void		vbi_cache_destroy(struct vbi *);
extern struct vt_page * vbi_cache_put(struct vbi *, struct vt_page *vtp);
extern struct vt_page * vbi_cache_get(struct vbi *, int pgno,
				      int subno, int subno_mask);
extern int              vbi_cache_foreach(struct vbi *, int pgno, int subno,
					  int dir, foreach_callback *func,
					  void *data);
extern void             vbi_cache_flush(struct vbi *);

#endif /* CACHE_H */
