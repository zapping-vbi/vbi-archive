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

/* $Id: misc.h,v 1.1 2000-07-04 17:40:20 garetxe Exp $ */

#ifndef MISC_H
#define MISC_H

#include <stdlib.h>
#include <malloc.h>
#include <math.h>

#define TRUE 1
#define FALSE 0

typedef unsigned char bool;

extern volatile int program_shutdown; /* 1 if all threads can exit */

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static inline int
saturate(int val, int min, int max)
{
#ifdef __i686__
	if (val < min)
		val = min;
	if (val > max)
		val = max;
#else
	if (val < min)
		val = min;
	else if (val > max)
		val = max;
#endif

	return val;
}

/*
 *  Integer sign, equv to saturate(val, -1, +1)
 */
#define sign(val) (((int)(val) >> 31) | ((int)(val) > 0))

/*
 *  Round to nearest int, halfway cases to the nearest even integer
 */
#if 0
#define lroundn(val) \
	((int)(((val) < 0.0) ? -floor(0.5 - (val)) : floor((val) + 0.5)))
#else
#define lroundn(val) ({ int res; asm volatile ("fistpl %0" : "=m" (res) : "t" (val) : "st"); res; })
#endif

/*
 *  Absolute value w/o a branch
 */
static inline unsigned int
nbabs(register int n)
{
	register int t = n;

        t >>= 31;
	n ^= t;
	n -= t;

	return n;
}

/*
 *  Find first set bit, starting at msb.
 *
 *  0x89abcdef -> 31, 0x456789ab -> 30
 *  0x00000001 -> 0, 0x00000000 undefined
 */
static inline int
ffsr(unsigned int n)
{
	int r;

	asm volatile ("\tbsrl %1,%0\n": "=r" (r) : "r" (n));

	return r;
}

/*
 *  Do not free() this
 */
static inline void *
malloc_aligned(size_t size, int align)
{
	void *p;

#ifdef HAVE_MEMALIGN
	p = (void *) memalign(align, size);
#else	
	if ((p = malloc(size + align)))
		(char *) p += align - ((int) p & (align - 1));
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
	if ((p = calloc(1, size + align)))
		(char *) p += align - ((int) p & (align - 1));
#endif
	return p;
}

#endif
