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

#ifndef MMX_H
#define MMX_H

#include <stdlib.h>

typedef	union {
	long long		q;
	unsigned long long	uq;
	int			d[2];
	unsigned int		ud[2];
	short			w[4];
	unsigned short		uw[4];
	char			b[8];
	unsigned char		ub[8];
} __attribute__ ((aligned (8))) mmx_t;

#define MMXQ(a) ((mmx_t)((long long)(a)))
#define MMXD(a,b) ((mmx_t)(((((unsigned long long)(b))&0xFFFFFFFFULL)<<32)|((((unsigned long long)(a))&0xFFFFFFFFULL))))
#define MMXW(a,b,c,d) ((mmx_t)(((((unsigned long long)(d))&0xFFFFULL)<<48)|((((unsigned long long)(c))&0xFFFFULL)<<32)|((((unsigned long long)(b))&0xFFFFULL)<<16)|((((unsigned long long)(a))&0xFFFFULL))))
#define MMXB(a,b,c,d,e,f,g,h) ((mmx_t)(((((unsigned long long)(h))&0xFFULL)<<56)|((((unsigned long long)(g))&0xFFULL)<<48)|((((unsigned long long)(f))&0xFFULL)<<40)|((((unsigned long long)(e))&0xFFULL)<<32)|((((unsigned long long)(d))&0xFFULL)<<24)|((((unsigned long long)(c))&0xFFULL)<<16)|((((unsigned long long)(b))&0xFFULL)<<8)|((((unsigned long long)(a))&0xFFULL))))

#define MMXRD(a) MMXD(a,a)
#define MMXRW(a) MMXW(a,a,a,a)
#define MMXRB(a) MMXB(a,a,a,a,a,a,a,a)

/*
 *  These are optimization classes rather than
 *  identifying the actual model or brand name.
 *  Keep order (options.c)
 */
#define CPU_UNKNOWN		0	/* no MMX */
#define	CPU_PENTIUM_MMX		1	/* MMX; late P5 core */
#define CPU_PENTIUM_II		2	/* MMX, CMOV; any P6 core */
#define	CPU_PENTIUM_III		3	/* MMX, CMOV, SSE; any P6 core and Itanium x86 */
#define	CPU_PENTIUM_4		4	/* MMX, CMOV, SSE, SSE2; any P8 core */
#define CPU_K6_2		5	/* MMX, 3DNOW; K6-2/K6-III */
#define CPU_ATHLON		6	/* MMX, 3DNOW, AMD 3DNOW ext, CMOV, SSE int; K7 core */
#define CPU_CYRIX_MII		7	/* MMX, CMOV */
#define CPU_CYRIX_III		8	/* MMX, Cyrix MMX ext, 3DNOW, CMOV */
#define CPU_CYRIX_NEHEMIAH      9       /* MMX, Cyrix MMX ext, SSE, CMOV */

extern int cpu_detection(void);

#if __GNUC__ == 3
# if __GNUC_MINOR__ > 4
#  warning Compilation with your version of gcc is untested,
#  warning may fail or create incorrect code.
# endif
# define FPU_REGS , "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)"
# define SECTION(x) section (x), 
# define emms() do { asm volatile ("\temms\n" ::: "cc" FPU_REGS); } while(0)
#elif __GNUC__ == 2
# if __GNUC_MINOR__ < 90 /* gcc [2.7.2.3] */
#  define FPU_REGS
#  define SECTION(x)
#  define emms() asm("\temms\n")
#  define __builtin_expect(exp, c) (exp)
# elif __GNUC_MINOR__ < 95 /* egcs [2.91.66] */
#  define FPU_REGS
#  define SECTION(x)
#  define emms() asm("\temms\n")
#  define __builtin_expect(exp, c) (exp)
# else /* egcs [2.95.2] */
#  define FPU_REGS , "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)"
#  define SECTION(x) section (x), 
#  define emms() do { asm volatile ("\temms\n" ::: "cc" FPU_REGS); } while(0)
#  define __builtin_expect(exp, c) (exp)
# endif
#else
# error Sorry, your GCC does not exist.
#endif

#define CACHE_LINE 32 // power of two

static inline const unsigned int
swab32(unsigned int x)
{
	if (__builtin_constant_p(x))
		return (((x) & 0xFFUL) << 24) | (((x) & 0xFF00UL) << 8)
			| (((x) & 0xFF0000UL) >> 8) | (((x) & 0xFF000000UL) >> 24);

	asm volatile ("bswap %0" : "=r" (x) : "0" (x));

	return x;
}

static inline const unsigned short
swab16(unsigned short x)
{
	if (__builtin_constant_p(x))
		return (((x) & 0xFFUL) << 8) | (((x) & 0xFF00UL) >> 8);

	asm volatile ("xchgb %b0,%h0" : "=q" (x) : "0" (x));

	return x;
}

#endif // MMX_H
