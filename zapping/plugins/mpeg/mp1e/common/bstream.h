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

/* $Id: bstream.h,v 1.1 2000-08-09 09:40:14 mschimek Exp $ */

#ifndef BITSTREAM_H
#define BITSTREAM_H

#include "mmx.h"

struct bs_rec {
	int		n;	/* Number of bits accumulated */
	mmx_t *		p;	/* Output buffer */
	mmx_t		buf;	/* Accumulated bits (usually cached in mm7), left justified */
	mmx_t		uq64;	/* 64ULL */
	mmx_t *		p1;
	int		pad;
};

#define bepilog(b) asm volatile ("\tmovq %0,%%mm7\n" :: "m" ((b)->buf))
#define bprolog(b) asm volatile ("\tmovq %%mm7,%0\n" :: "m" ((b)->buf))

extern void		binit_write(struct bs_rec *b);
#define			bstart(b, p0) ((b)->p1 = (b)->p = (mmx_t *)(p0));
extern void		bputl(struct bs_rec *b, unsigned int v, int n) __attribute__ ((regparm (3)));
extern void		bputq(struct bs_rec *b, int n) __attribute__ ((regparm (2)));
#define			bwritten(b) ((((char *)(b)->p - (char *)(b)->p1) * 8) + ((b)->n))
extern int		bflush(struct bs_rec *b);
#define			brewind(bd, bs) (*(bd) = *(bs))

static inline void
bstartq(unsigned int v)
{
	asm volatile ("
		movd		%0,%%mm0;
	" :: "rm" (v) : "cc" FPU_REGS);
}

#define bcatq(v, n)							\
do {									\
	if (__builtin_constant_p(n))					\
		asm volatile (						\
			"\tpsllq %0,%%mm0;\n"				\
			:: "im" ((n)) : "cc" FPU_REGS);			\
			/* never m but suppress warning */		\
	else								\
		asm volatile (						\
			"\tmovd	%0,%%mm2;\n"				\
			"\tpsllq %%mm2,%%mm0;\n"			\
			:: "rm" ((unsigned int)(n)) : "cc" FPU_REGS);	\
									\
	asm volatile (							\
		"\tmovd	%0,%%mm1;\n"					\
		"\tpor %%mm1,%%mm0;\n"					\
		:: "rm" ((unsigned int)(v)) : "cc" FPU_REGS);		\
} while (0)

#endif // BITSTREAM_H
