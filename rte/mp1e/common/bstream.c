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

/* $Id: bstream.c,v 1.3 2002-09-12 12:25:13 mschimek Exp $ */

#include <stdio.h>
#include "bstream.h"

void
binit_write(struct bs_rec *b)
{
	b->n		= 0;
	b->buf.uq	= 0;
	b->uq64.uq	= 64ULL;
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

