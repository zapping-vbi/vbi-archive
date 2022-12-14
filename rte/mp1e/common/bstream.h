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

/* $Id: bstream.h,v 1.5 2005-02-25 18:30:47 mschimek Exp $ */

#ifndef BITSTREAM_H
#define BITSTREAM_H

#include "mmx.h"

struct bs_rec {
	int		n;	/* Number of bits accumulated */
	mmx_t *		p;	/* Output buffer */
	mmx_t		buf;	/* Accumulated bits (usually cached in mm7), msb justified */
	mmx_t		uq64;	/* 64ULL */
	mmx_t *		p1;
	int		pad;
};

extern void		binit_write(struct bs_rec *b);
#define			bstart(b, p0) ((b)->p1 = (b)->p = (mmx_t *)(p0));
#define bepilog(b)	__asm__ __volatile__ (" movq %0,%%mm7\n" :: "m" ((b)->buf))
#define bprolog(b)	__asm__ __volatile__ (" movq %%mm7,%0\n" :: "m" ((b)->buf))
/* Encode rightmost n bits in v, with v < (1 << n) and 0 < n <= 32 */
#define bputl(b, v, n)  mmx_bputl(b, n, v) /* to match reg assignment of bputq */
extern void		mmx_bputl(struct bs_rec *b, int n, unsigned int v) __attribute__ ((regparm (3)));
/* Encode rightmost n bits in mm0.uq, with mm0.uq < (1 << n) and 0 < n <= 64 */
#define bputq		mmx_bputq
extern void		mmx_bputq(struct bs_rec *b, int n) __attribute__ ((regparm (2)));
#define			bwritten(b) ((((char *)(b)->p - (char *)(b)->p1) * 8) + ((b)->n))
#define			btell(b) ((((char *)(b)->p - (char *)(b)->p1) * 8) + ((b)->n))
extern int		bflush(struct bs_rec *b);
#define			brewind(bd, bs) (*(bd) = *(bs))

static inline void
bstartq(unsigned int v)
{
	__asm__ __volatile__ ("\tmovd %0,%%mm0;\n"
		:: "rm" (v) : "cc" FPU_REGS);
}

#if __GNUC__ >= 4

#define bcatq(v, n)							\
do {									\
	if (__builtin_constant_p(n))					\
		__asm__ __volatile__ (					\
			"\tpsllq %0,%%mm0;\n"				\
			:: "i" ((n)) : "cc" FPU_REGS);			\
	else								\
		__asm__ __volatile__ (					\
			"\tmovd %0,%%mm2;\n"				\
			"\tpsllq %%mm2,%%mm0;\n"			\
			:: "rm" ((unsigned int)(n)) : "cc" FPU_REGS);	\
									\
	__asm__ __volatile__ (						\
			"\tmovd %0,%%mm1;\n"				\
			"\tpor %%mm1,%%mm0;\n"				\
			:: "rm" ((unsigned int)(v)) : "cc" FPU_REGS);	\
} while (0)

#else

#define bcatq(v, n)							\
do {									\
	if (__builtin_constant_p(n))					\
		__asm__ __volatile__ (					\
			"\tpsllq %0,%%mm0;\n"				\
			:: "im" ((n)) : "cc" FPU_REGS);			\
			/* never m but suppress warning */		\
	else								\
		__asm__ __volatile__ (					\
			"\tmovd %0,%%mm2;\n"				\
			"\tpsllq %%mm2,%%mm0;\n"			\
			:: "rm" ((unsigned int)(n)) : "cc" FPU_REGS);	\
									\
	if (0&&__builtin_constant_p(v))					\
		__asm__ __volatile__ ("\tpor %0,%%mm0;\n"		\
			:: "m" ((unsigned long long)(v))		\
			: "cc" FPU_REGS);				\
	else								\
		__asm__ __volatile__ (					\
			"\tmovd %0,%%mm1;\n"				\
			"\tpor %%mm1,%%mm0;\n"				\
			:: "rm" ((unsigned int)(v)) : "cc" FPU_REGS);	\
} while (0)

#endif

static inline void
balign (struct bs_rec *b)
{
	int n7 = b->n & 7;

	if (__builtin_expect(n7 > 0, 1))
		b->n += 8 - n7;
}

#endif /* BITSTREAM_H */
