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

/* $Id: bstream.h,v 1.1 2000-07-04 17:40:20 garetxe Exp $ */

#ifndef BSTREAM_H
#define BSTREAM_H

#include "mmx.h"

struct bs_rec
{
	int		n;	/* Number of bits accumulated */
	mmx_t *		p;	/* Output buffer */
	mmx_t		buf;	/* Accumulated bits (usually cached in mm7), left justified */
	mmx_t		uq64;	/* 64ULL */
	mmx_t *		p1;
	char		pad[4];
};

#define bepilog(b) asm("\tmovq %0,%%mm7\n" :: "m" ((b)->buf) : "st")
#define bprolog(b) asm("\tmovq %%mm7,%0\n" :: "m" ((b)->buf) : "st")

extern void		binit(struct bs_rec *b);
#define			bstart(b, p0) ((b)->p1 = (b)->p = (mmx_t *)(p0));
extern void		bputl(struct bs_rec *b, unsigned int v, int n) __attribute__ ((regparm (3)));
// bputq(struct bs_rec *b, unsigned long long reg mm0 v, int n) __attribute__ ((regparm (3)));
#define			bwritten(b) ((((char *)(b)->p - (char *)(b)->p1) * 8) + ((b)->n))
extern int		bflush(struct bs_rec *b);
#define			brewind(bd, bs) (*(bd) = *(bs))

static inline const unsigned long
bswap(unsigned long x)
{
	if (__builtin_constant_p(x))
		return (((x) & 0xFFUL) << 24) | (((x) & 0xFF00UL) << 8)
			| (((x) & 0xFF0000UL) >> 8) | (((x) & 0xFF000000UL) >> 24);

	asm("bswap %0" : "=r" (x) : "0" (x));

	return x;
}

#endif
