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

/* $Id: bstream.c,v 1.1 2000-07-04 17:40:20 garetxe Exp $ */

#include "bstream.h"

void
binit(struct bs_rec *b)
{
	b->n		= 0;
	b->buf.uq	= 0;
	b->uq64.uq	= 64ULL;
}

// Encode rightmost n bits in v, with v < (1 << n) and 0 < n < 32

void
bputl(struct bs_rec *b, unsigned int v, int n)
{
	asm("
		movd		%0,%%mm2;		subl		%1,%2;
		movd		%2,%%mm1;		jle		1f;
		psllq		%%mm1,%%mm2;		movl		%1,%3;
		por		%%mm2,%%mm7;		jmp		2f;
1:		movl		%4,%2;			pxor		%%mm4,%%mm4;
		movd		%5,%%mm3;		addl		$8,%2;
		movq		%%mm2,%%mm5;		paddd		%%mm1,%%mm3;
		psubd		%%mm1,%%mm4;		psllq		%%mm3,%%mm2;
		movd		%%mm4,%3;		psrlq		%%mm4,%%mm5;
		por		%%mm7,%%mm5;		movq		%%mm2,%%mm7;
		movd		%%mm5,%0;		psrlq		$32,%%mm5;
		movd		%%mm5,%1;		movl		%2,%4;
		bswap		%0;			/**/
		bswap		%1;			/**/
		movl		%1,-8(%2);		movl		%0,-4(%2)
2:
	" :: "r" (v), "r" (n + b->n), "r" (64),
	     "m" (b->n), "m" (b->p), "m" (b->uq64)
	); //  : "%0", "%1", "%2"); regcall, caller saves
}

// Returns the number of bits encoded, multiple of 64

int
bflush(struct bs_rec *b)
{
	// Bits are shifted to msb already, filled up with padding
	// zeroes. Store as mmx.uq to maintain frame alignment.

	((unsigned int *) b->p)[0] = bswap(b->buf.ud[1]);
	((unsigned int *) b->p)[1] = bswap(b->buf.ud[0]);

	b->p++;
	b->n = 0;
	b->buf.uq = 0;

	return (b->p - b->p1) * 64;
}
