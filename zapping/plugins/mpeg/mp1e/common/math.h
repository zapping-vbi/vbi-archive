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

/* $Id: math.h,v 1.5 2001-06-18 12:33:58 mschimek Exp $ */

#ifndef MATH_H
#define MATH_H

#include <math.h>
#include <float.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

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

/*
 *  Integer sign, equv to saturate(val, -1, +1)
 */
#define sign(val) (((int)(val) >> 31) | ((int)(val) > 0))

/*
 *  Round to nearest int, halfway cases to the nearest even integer
 */
#ifdef __i386__
/* rtn is the default mode */
#define lroundn(val) ({ long res; asm volatile ("fistpl %0" : "=m" (res) : "t" (val) : "st"); res; })
#define llroundn(val) ({ long long res; asm volatile ("fistpll %0" : "=m" (res) : "t" (val) : "st"); res; })
#else
#define lroundn(val) ((long)(((val) < 0.0) ? -floor(0.5 - (val)) : floor((val) + 0.5)))
#define llroundn(val) ((long long)(((val) < 0.0) ? -floor(0.5 - (val)) : floor((val) + 0.5)))
#endif

/*
 *  Round integer v up to nearest integer multiple of a
 *  ?
 */
// #define lalign(v, a) ((v) + (a) - (int)(v) % (a))

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
 *  Number of set bits
 */
static inline unsigned int
popcnt(unsigned int v)
{
	v -= ((v >> 1) & 0x55555555);
	v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
	v = (v + (v >> 4)) & 0x0F0F0F0F;

	return (v * 0x01010101) >> 24;
}

#endif // MATH_H
