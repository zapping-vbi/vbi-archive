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

/* $Id: bstream.c,v 1.3 2001-06-23 02:50:44 mschimek Exp $ */

#include "bstream.h"

void
binit_write(struct bs_rec *b)
{
	b->n		= 0;
	b->buf.uq	= 0;
	b->uq64.uq	= 64ULL;
}

// XXX #@$! compiler complained about regs again, move into bstream_mmx.s
/*
 *  Encode rightmost n bits in v, with v < (1 << n) and 0 < n < 32
 */
void
bputl(struct bs_rec *b, unsigned int v, int n)
{
	asm volatile (
		" movd		%0,%%mm2;\n"
		" subl		%1,%2;\n"
		" movd		%2,%%mm1;\n"
		" jle		1f;\n"
		" psllq		%%mm1,%%mm2;\n"
		" movl		%1,%3;\n"
		" por		%%mm2,%%mm7;\n"
		" jmp		2f;\n"
		"1:\n"
		" movl		%4,%2;\n"		
		" pxor		%%mm4,%%mm4;\n"
		" movd		%5,%%mm3;\n"
		" addl		$8,%2;\n"
		" movq		%%mm2,%%mm5;\n"
		" paddd		%%mm1,%%mm3;\n"
		" psubd		%%mm1,%%mm4;\n"
		" psllq		%%mm3,%%mm2;\n"
		" movd		%%mm4,%3;\n"
		" psrlq		%%mm4,%%mm5;\n"
		" por		%%mm7,%%mm5;\n"	
		" movq		%%mm2,%%mm7;\n"
		" movd		%%mm5,%0;\n"
		" psrlq		$32,%%mm5;\n"
		" bswap		%0;\n"
		" movl		%0,-4(%2)\n"
		" movd		%%mm5,%1;\n"
		" movl		%2,%4;\n"
		" bswap		%1;\n"
		" movl		%1,-8(%2);\n"
		"2:\n"
	:: "r" (v), "r" (n + b->n), "r" (64),
	     "m" (b->n), "m" (b->p), "m" (b->uq64)
	  : "cc", "memory" FPU_REGS);
}

/*
 *  Encode rightmost n bits in mm0, with mm0.uq < (1 << n) and
 *  0 < n < 64
 */
void
bputq(struct bs_rec *b, int n)
{
	asm volatile (
		" movq		%%mm0,%%mm2;\n"
		" subl		%1,%2;\n"
		" movd		%2,%%mm1;\n"
		" jle		1f;\n"
		" psllq		%%mm1,%%mm2;\n"
		" movl		%1,%3;\n"
		" por		%%mm2,%%mm7;\n"
		" jmp		2f;\n"
		"1:\n"
		" movl		%4,%2;\n"		
		" pxor		%%mm4,%%mm4;\n"
		" movd		%5,%%mm3;\n"		
		" addl		$8,%2;\n"
		" movq		%%mm2,%%mm5;\n"		
		" paddd		%%mm1,%%mm3;\n"
		" psubd		%%mm1,%%mm4;\n"		
		" psllq		%%mm3,%%mm2;\n"
		" movd		%%mm4,%3;\n"		
		" psrlq		%%mm4,%%mm5;\n"
		" por		%%mm7,%%mm5;\n"
		" movq		%%mm2,%%mm7;\n"
		" movd		%%mm5,%1;\n"
		" psrlq		$32,%%mm5;\n"
		" bswap		%1;\n"
		" movl		%1,-4(%2)\n"
		" movd		%%mm5,%1;\n"		
		" movl		%2,%4;\n"
		" bswap		%1;\n"
		" movl		%1,-8(%2);\n"
		"2:\n"
	:: "r" (b->n), "r" (n + b->n), "r" (64),
	     "m" (b->n), "m" (b->p), "m" (b->uq64)
	: "cc", "memory" FPU_REGS);
}

/*
 *  Returns the number of bits encoded since bstart,
 *  granularity 64 bits
 */
int
bflush(struct bs_rec *b)
{
	// Bits are shifted to msb already, filled up with padding
	// zeroes. Store as mmx.uq to maintain frame alignment.

	((unsigned int *) b->p)[0] = swab32(b->buf.ud[1]);
	((unsigned int *) b->p)[1] = swab32(b->buf.ud[0]);

	b->p++;
	b->n = 0;
	b->buf.uq = 0;

	return (b->p - b->p1) * 64;
}
