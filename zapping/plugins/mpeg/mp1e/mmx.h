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

typedef enum {
	ARCH_PENTIUM_MMX,
	ARCH_KLAMATH,
	ARCH_KATMAI,
	ARCH_K6_2,
	ARCH_CYRIX,
	ARCH_K7
} cpu_architecture;

extern int cpu_id(cpu_architecture arch);

#if __GNUC__ != 2 || __GNUC_MINOR__ < 6

#error Sorry, your GCC does not exist.

#elif __GNUC_MINOR__ < 90 /* gcc [2.7.2.3] */

#define FPU_REGS
#define SECTION(x)
#define emms() asm("\temms\n")

#elif __GNUC_MINOR__ < 95 /* egcs [2.91.66] */

#define FPU_REGS
#define SECTION(x)
#define emms() asm("\temms\n")

#else /* ? [2.95.2] */

#define FPU_REGS , "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)", "st(6)", "st(7)"
#define SECTION(x) section (x), 
#define emms() do { asm volatile ("\temms\n" ::: "cc" FPU_REGS); } while(0)

#endif

#define CACHE_LINE 64

/* MMX software simulation */

#define pload(p, mm2) ((mm2)->uq = *((unsigned long long *)(p)))
#define pstore(mm1, p) (*((unsigned long long *)(p)) = (mm1).uq)
#define pstored(mm1, p) (*((unsigned int *)(p)) = (mm1).ud[0])

#define movq(mm1, mm2) ((mm2)->uq = (mm1).uq)
#define movd(mm1, mm2) ((mm2)->ud[0] = (mm1).ud[0], (mm2)->ud[1] = 0)
// register versions

extern void packsswb(mmx_t, mmx_t *);
extern void packssdw(mmx_t, mmx_t *);
extern void packuswb(mmx_t, mmx_t *);
extern void paddb(mmx_t, mmx_t *);
extern void paddw(mmx_t, mmx_t *);
extern void paddd(mmx_t, mmx_t *);
extern void paddsb(mmx_t, mmx_t *);
extern void paddsw(mmx_t, mmx_t *);
extern void paddusb(mmx_t, mmx_t *);
extern void paddusw(mmx_t, mmx_t *);
#define pand(mm1, mm2) ((mm2)->uq &= (mm1).uq)
#define pandn(mm1, mm2) ((mm2)->uq = (~(mm2)->uq) & (mm1).uq)
extern void pcmpeqb(mmx_t, mmx_t *);
extern void pcmpeqw(mmx_t, mmx_t *);
extern void pcmpeqd(mmx_t, mmx_t *);
extern void pcmpgtb(mmx_t, mmx_t *);
extern void pcmpgtw(mmx_t, mmx_t *);
extern void pcmpgtd(mmx_t, mmx_t *);
extern void pmaddwd(mmx_t, mmx_t *);
extern void pmulhw(mmx_t, mmx_t *);
extern void pmullw(mmx_t, mmx_t *);
#define por(mm1, mm2) ((mm2)->uq |= (mm1).uq)
// ps***(mm1.uq, &mm2);
extern void psllb(unsigned int, mmx_t *);
extern void psllw(unsigned int, mmx_t *);
extern void psllq(unsigned int, mmx_t *);
extern void psraw(unsigned int, mmx_t *);
extern void psrad(unsigned int, mmx_t *);
extern void psrlb(unsigned int, mmx_t *);
extern void psrlw(unsigned int, mmx_t *);
extern void psrlq(unsigned int, mmx_t *);
extern void psubb(mmx_t, mmx_t *);
extern void psubw(mmx_t, mmx_t *);
extern void psubd(mmx_t, mmx_t *);
extern void psubsb(mmx_t, mmx_t *);
extern void psubsw(mmx_t, mmx_t *);
extern void psubusb(mmx_t, mmx_t *);
extern void psubusw(mmx_t, mmx_t *);
extern void punpckhbw(mmx_t, mmx_t *);
extern void punpckhwd(mmx_t, mmx_t *);
extern void punpckhdq(mmx_t, mmx_t *);
extern void punpcklbw(mmx_t, mmx_t *);
extern void punpcklwd(mmx_t, mmx_t *);
extern void punpckldq(mmx_t, mmx_t *);
#define pxor(mm1, mm2) ((mm2)->uq ^= (mm1).uq)

/* Macros */

static inline void
pmirrorw(mmx_t *mm0, mmx_t *mm1, mmx_t *mm2, mmx_t *mm3)
{
	mmx_t mm8, mm9;

	movq(*mm0,&mm8);
	punpcklwd(*mm1,mm0);	// 1b 0b 1a 0a
	punpckhwd(*mm1,&mm8);	// 1d 0d 1c 0c
	movq(*mm2,&mm9);
	punpcklwd(*mm3,mm2);	// 3b 2b 3a 2a
	punpckhwd(*mm3,&mm9);	// 3d 2d 3c 2c
	movq(*mm0,mm1);
	punpckldq(*mm2,mm0);	// 3a 2a 1a 0a
	punpckhdq(*mm2,mm1);	// 3b 2b 1b 0b
	movq(mm8,mm2);
	movq(mm8,mm3);
	punpckldq(mm9,mm2);	// 3c 2c 1c 0c
	punpckhdq(mm9,mm3);	// 3d 2d 1d 0d
}

/* AMD K6-2 extensions (3DNow!) */

// pavgusb	0F 0F BF
// pmulhrw	0F 0F B7
// prefetch	0F 0D ModR/M
// prefetchw	0F 0D ModR/M
// femms	0F 0E

/* Cyrix extensions */

// paveb	0F 50
// paddsiw	0F 51
// pmagw	0F 52
// pdistib	0F 54
// psubsiw	0F 55
// pmvzb	0F 58
// pmulhrw	0F 59
// pmvnzb	0F 5A	
// pmvlzb	0F 5B
// pmvgezb	0F 5C
// pmulhriw	0F 5D
// pmachriw	0F 5E

/* Intel Katmai extensions (SSE) */

extern void maskmovq(int, mmx_t, mmx_t *);
#define movntq(mm1, p) (*((unsigned long long *)(p)) = (mm1).uq)
extern void pavgb(mmx_t, mmx_t *);
extern void pavgw(mmx_t, mmx_t *);
extern void pextrw(int, mmx_t, int *);
extern void pinsrw(int, int, mmx_t *);
extern void pmaxsw(mmx_t, mmx_t *);
extern void pmaxub(mmx_t, mmx_t *);
extern void pmovmskb(mmx_t, int *);
extern void pmulhuw(mmx_t, mmx_t *);
extern void psadbw(mmx_t, mmx_t *);
extern void pshufw(int, mmx_t *);
// prefetch
// sfence

/* AMD Athlon extensions */

// maskmovq	0F F7
// movntq	0F E7
// pavgb	0F E0 ModR/M
// pavgw	0F E3 ModR/M
// pextrw	0F C5
// pinsrw	0F C4
// pmaxsw	0F EE ModR/M
// pmaxub	0F DE ModR/M
// pminsw	0F EA ModR/M
// pminub	0F DA ModR/M
// pmovmskb	0F D7
// pmulhuw	0F E4 ModR/M
// psadbw	0F F6 ModR/M
// pshufw	0F 70
// prefetchnta	0F 18
// prefetchnt0	0F 18
// prefetchnt1	0F 18
// prefetchnt2	0F 18
// sfence	0F AE
// pswapd	0F 0F BB

/* Intel Willamette extensions */

// pmuludq
// paddq
// psubq
// pshuflw
// pshufhw
// pshufd
// pslldq
// psrldq
// punpckhqdq
// punpcklqdq
// movmm2dq
// movdq2mm
// maskmovdqu
// movntdq
// movntpd
// movnti
// lfence
// mfence
// clflush
// xmmx versions

#endif
