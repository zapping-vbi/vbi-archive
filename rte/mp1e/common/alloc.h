/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

/* $Id: alloc.h,v 1.8 2004-04-09 05:16:45 mschimek Exp $ */

#ifndef ALLOC_H
#define ALLOC_H

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "types.h"

#ifdef HAVE_MEMALIGN
#define free_aligned(p) free(p)
#else
#define free_aligned(p) free(((void **) p)[-1])
#endif

extern void *alloc_aligned(size_t, int, rte_bool);

static inline void *
malloc_aligned(size_t size, int align)
{
	void *p;

#ifdef HAVE_MEMALIGN
	p = (void *) memalign(align, size);
#else
	p = alloc_aligned(size, align, FALSE);
/*
	if ((p = malloc(size + align)))
		(char *) p += align - ((int) p & (align - 1));
 */
#endif
	return p;
}

static inline void *
calloc_aligned(size_t size, int align)
{
	void *p;

#ifdef HAVE_MEMALIGN
	if ((p = (void *) memalign(align, size)))
		memset(p, 0, size);
#else
	p = alloc_aligned(size, align, TRUE);
/*
	if ((p = calloc(1, size + align)))
		(char *) p += align - ((int) p & (align - 1));
 */
#endif
	return p;
}

#endif // ALLOC_H
