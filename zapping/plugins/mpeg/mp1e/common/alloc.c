/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
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

/* $Id: alloc.c,v 1.3 2001-05-31 19:40:49 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include "math.h"

#ifndef HAVE_MEMALIGN

void *
alloc_aligned(size_t size, int align, bool clear)
{
	void *p, *b;

	if (align < sizeof(void *))
		align = sizeof(void *);

	if (!(b = malloc(size + align)))
		return NULL;

	p = (void *)(((long)((char *) b + align)) & -align);

	((void **) p)[-1] = b;

	if (clear)
		memset(p, 0, size);

	return p;
}

#endif // !HAVE_MEMALIGN
