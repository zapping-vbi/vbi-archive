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

/* $Id: alloc.c,v 1.8 2005-02-25 18:30:46 mschimek Exp $ */

#include <stdlib.h>
#include "math.h"
#include "alloc.h"

#ifndef HAVE_MEMALIGN

void *
alloc_aligned			(size_t			size,
				 size_t			align,
				 rte_bool		clear)
{
	void *p;
	void **pp;
	unsigned long addr;

	if (align < sizeof (void *))
		align = sizeof (void *);

	if (!(p = malloc (size + align)))
		return NULL;

	addr = align + (unsigned long) p;
	addr -= addr % align;

	pp = (void **) addr;
	pp[-1] = p;

	p = (void *) addr;

	if (clear)
		memset (p, 0, size);

	return p;
}

#endif /* !HAVE_MEMALIGN */
