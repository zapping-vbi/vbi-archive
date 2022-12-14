/*
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

/* $Id: math.h,v 1.5 2005-01-08 14:54:19 mschimek Exp $ */

#ifndef MATH_H
#define MATH_H

#include <math.h>

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#define swap(a, b)						\
do {								\
	__typeof__ (a) _t = (b);				\
	(b) = (a);						\
	(a) = _t;						\
} while (0)

static inline int
saturate(int val, int min, int max)
{
#ifdef __i686__
	/* 2 x cmp cmov, typ. both evaluated */
	if (val < min)
		val = min;
	if (val > max)
		val = max;
#else
	/* 1-2 branches */
	if (val < min)
		val = min;
	else if (val > max)
		val = max;
#endif

	return val;
}

static inline unsigned int
nbabs(register int n)
{
	register int t = n;

        t >>= 31;
	n ^= t;
	n -= t;

	return n;
}

#ifndef acount
#define acount(array) (sizeof(array)/sizeof(array[0]))
#endif

#endif /* MATH_H */
