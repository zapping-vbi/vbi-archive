/*
 *  MPEG-1 Real Time Encoder
 *  Zero displacement inter prediction macros
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

/* $Id: predict.h,v 1.1 2000-07-05 18:09:34 mschimek Exp $ */

extern short		mblock[7][6][8][8];

/*
 *  Forward prediction (P frames only)
 *
 *  mblock[1] = org - old_ref;
 *  mblock[3] = old_ref; 	// for reconstruction by idct_inter
 */
static inline int
predict_forward(unsigned char *from)
{
	int i, n, s = 0, s2 = 0;

	for (i = 0; i < 4 * 64; i++) {
		mblock[1][0][0][i] = n = mblock[0][0][0][i] - from[i];
		mblock[3][0][0][i] = from[i];
		s += n;
		s2 += n * n;
	}

	for (; i < 6 * 64; i++) {
		mblock[1][0][0][i] = mblock[0][0][0][i] - from[i];
		mblock[3][0][0][i] = from[i];
	}

	return s2 * 256 - (s * s);
}

/*
 *  Backward prediction (B frames only, no reconstruction)
 *
 *  mblock[1] = org - new_ref;
 */
static inline int
predict_backward(unsigned char *from)
{
	int i, n, s = 0;

	for (i = 0; i < 4 * 64; i++) {
		mblock[1][0][0][i] = n = mblock[0][0][0][i] - from[i];
		s += n * n;
	}

	for (; i < 6 * 64; i++)
		mblock[1][0][0][i] = mblock[0][0][0][i] - from[i];

	return s;
}

/*
 *  Bidirectional prediction (B frames only, no reconstruction)
 *
 *  mblock[1] = org - old_ref;
 *  mblock[2] = org - new_ref;
 *  mblock[3] = org - linear_interpolation(old_ref, new_ref);
 */
static inline int
predict_bidirectional(unsigned char *from1, unsigned char *from2, int *vmc1, int *vmc2)
{
	int i, n, si = 0, sf = 0, sb = 0;

	for (i = 0; i < 4 * 64; i++) {
		mblock[1][0][0][i] = n = mblock[0][0][0][i] - from1[i];
		sf += n * n; // forward
		mblock[2][0][0][i] = n = mblock[0][0][0][i] - from2[i];
		sb += n * n; // backward
		mblock[3][0][0][i] = n = mblock[0][0][0][i] - ((from1[i] + from2[i] + 1) >> 1);
		si += n * n; // interpolated	                   unsigned -> pavgb
	}

	for (; i < 6 * 64; i++) {
		mblock[1][0][0][i] = mblock[0][0][0][i] - from1[i];
		mblock[2][0][0][i] = mblock[0][0][0][i] - from2[i];
		mblock[3][0][0][i] = mblock[0][0][0][i] - ((from1[i] + from2[i] + 1) >> 1);
	}

	*vmc1 = sf;
	*vmc2 = sb;

	return si;
}

/*
 *  MMX
 */

static inline int
mmx_predict_forward(unsigned char *from)
{
	int n;

	asm volatile ("
		movq		(%2,%3),%%mm0;
		pxor		%%mm5,%%mm5;
		movq		(%1,%3,2),%%mm2;
		pxor		%%mm6,%%mm6;
		movq		8(%1,%3,2),%%mm3;
		pxor		%%mm7,%%mm7;

		.align 16
1:
		movq		%%mm0,%%mm1;
		punpcklbw	%%mm5,%%mm0;
		punpckhbw	%%mm5,%%mm1;
		psubw		%%mm0,%%mm2;
		paddw		%%mm2,%%mm6;
		psubw		%%mm1,%%mm3;
		paddw		%%mm3,%%mm6;
		movq		%%mm0,768*3+0(%1,%3,2);
		movq		%%mm1,768*3+8(%1,%3,2);
		movq		%%mm2,768*1+0(%1,%3,2);
		movq		%%mm3,768*1+8(%1,%3,2);
		pmaddwd		%%mm2,%%mm2;
		pmaddwd		%%mm3,%%mm3;
		paddd		%%mm2,%%mm7;
		paddd		%%mm3,%%mm7;
		movq		128(%2,%3),%%mm4;
		movq		256+0(%1,%3,2),%%mm0;
		movq		256+8(%1,%3,2),%%mm1;
		movq		%%mm4,%%mm2;
		punpcklbw	%%mm5,%%mm4;
		punpckhbw	%%mm5,%%mm2;
		psubw		%%mm4,%%mm0;
		paddw		%%mm0,%%mm6;
		psubw		%%mm2,%%mm1;
		paddw		%%mm1,%%mm6;
		movq		%%mm4,768*3+256+0(%1,%3,2);
		movq		%%mm2,768*3+256+8(%1,%3,2);
		movq		%%mm0,768*1+256+0(%1,%3,2);
		movq		%%mm1,768*1+256+8(%1,%3,2);
		pmaddwd		%%mm0,%%mm0;
		pmaddwd		%%mm1,%%mm1;
		paddd		%%mm0,%%mm7;
		paddd		%%mm1,%%mm7;
		movq		256(%2,%3),%%mm3;
		movq		512+0(%1,%3,2),%%mm2;
		movq		512+8(%1,%3,2),%%mm4;
		movq		%%mm3,%%mm1;
		punpcklbw	%%mm5,%%mm3;
		punpckhbw	%%mm5,%%mm1;
		psubw		%%mm3,%%mm2;
		psubw		%%mm1,%%mm4;
		movq		%%mm3,768*3+512+0(%1,%3,2);
		movq		%%mm1,768*3+512+8(%1,%3,2);
		movq		%%mm2,768*1+512+0(%1,%3,2);
		movq		%%mm4,768*1+512+8(%1,%3,2);
		movq		8(%2,%3),%%mm0;
		movq		0+16(%1,%3,2),%%mm2;
		movq		8+16(%1,%3,2),%%mm3;
		leal		8(%3),%3;
		decl		%4;
		jne		1b;

		movq		%%mm0,%%mm1;
		punpcklbw	%%mm5,%%mm0;
		punpckhbw	%%mm5,%%mm1;
		psubw		%%mm0,%%mm2;
		paddw		%%mm2,%%mm6;
		psubw		%%mm1,%%mm3;
		paddw		%%mm3,%%mm6;
		movq		%%mm0,768*3+0(%1,%3,2);
		movq		%%mm1,768*3+8(%1,%3,2);
		movq		%%mm2,768*1+0(%1,%3,2);
		movq		%%mm3,768*1+8(%1,%3,2);
		pmaddwd		%%mm2,%%mm2;
		pmaddwd		%%mm3,%%mm3;
		paddd		%%mm2,%%mm7;
		paddd		%%mm3,%%mm7;
		movq		128(%2,%3),%%mm0;
		movq		256+0(%1,%3,2),%%mm2;
		movq		256+8(%1,%3,2),%%mm3;
		movq		%%mm0,%%mm1;
		punpcklbw	%%mm5,%%mm0;
		punpckhbw	%%mm5,%%mm1;
		psubw		%%mm0,%%mm2;
		paddw		%%mm2,%%mm6;
		psubw		%%mm1,%%mm3;
		paddw		%%mm3,%%mm6;
		movq		%%mm0,768*3+256+0(%1,%3,2);
		movq		%%mm1,768*3+256+8(%1,%3,2);
		movq		%%mm2,768*1+256+0(%1,%3,2);
		movq		%%mm3,768*1+256+8(%1,%3,2);
		pmaddwd		%%mm2,%%mm2;
		pmaddwd		%%mm3,%%mm3;
		paddd		%%mm2,%%mm7;
		paddd		%%mm3,%%mm7;
		movq		256(%2,%3),%%mm0;
		movq		512+0(%1,%3,2),%%mm2;
		movq		512+8(%1,%3,2),%%mm3;
		movq		%%mm0,%%mm1;
		punpcklbw	%%mm5,%%mm0;
		punpckhbw	%%mm5,%%mm1;
		psubw		%%mm0,%%mm2;
		psubw		%%mm1,%%mm3;
		movq		%%mm0,768*3+512+0(%1,%3,2);
		movq		%%mm1,768*3+512+8(%1,%3,2);
		movq		%%mm2,768*1+512+0(%1,%3,2);
		movq		%%mm3,768*1+512+8(%1,%3,2);

		pmaddwd		c1,%%mm6;
		movq		%%mm7,%%mm4;
		psrlq		$32,%%mm7;
		paddd		%%mm4,%%mm7;
		movq		%%mm6,%%mm5;
		psrlq		$32,%%mm6;
		paddd		%%mm5,%%mm6;
		movd		%%mm6,%4;
		imul		%4,%4;
		pslld		$8,%%mm7;
		movd		%%mm7,%0;
		subl		%4,%0;
	"
	: "=&r" (n)
	: "r" (&mblock[0][0][0][0]), "r" (from), "r" (0), "r" (15)
	: "3", "4", "cc", "memory" FPU_REGS);

	return n;
}

#define mmx_predict_backward mmx_predict_forward

static inline int
mmx_predict_bidirectional(unsigned char *from1, unsigned char *from2, int *vmc1, int *vmc2)
{
	extern mmx_t c0, c1;
	int n;

	asm volatile ("
		pxor		%%mm5,%%mm5;
		pxor		%%mm6,%%mm6;
		pxor		%%mm7,%%mm7;
1:
		movq		(%4,%6),%%mm0;
		punpcklbw	%8,%%mm0;
		movq		(%5,%6),%%mm1;
		punpcklbw	%8,%%mm1;
		movq		(%3,%6,2),%%mm2;
		movq		%%mm2,%%mm3;
		movq		%%mm2,%%mm4;
		psubw		%%mm0,%%mm3;
		movq		%%mm3,768(%3,%6,2);
		pmaddwd		%%mm3,%%mm3;
		paddd		%%mm3,%%mm5;
		psubw		%%mm1,%%mm4;
		movq		%%mm4,1536(%3,%6,2);
		pmaddwd		%%mm4,%%mm4;
		paddd		%%mm4,%%mm6;
		paddw		%%mm1,%%mm0;
		paddw		%9,%%mm0;
		psrlw		$1,%%mm0;
		psubw		%%mm0,%%mm2;
		movq		%%mm2,2304(%3,%6,2);
		pmaddwd		%%mm2,%%mm2;
		paddd		%%mm2,%%mm7;		
		
		movq		(%4,%6),%%mm0;
		punpckhbw	%8,%%mm0;
		movq		(%5,%6),%%mm1;
		punpckhbw	%8,%%mm1;
		movq		8(%3,%6,2),%%mm2;
		movq		%%mm2,%%mm3;
		movq		%%mm2,%%mm4;
		psubw		%%mm0,%%mm3;
		movq		%%mm3,768+8(%3,%6,2);
		pmaddwd		%%mm3,%%mm3;
		paddd		%%mm3,%%mm5;
		psubw		%%mm1,%%mm4;
		movq		%%mm4,1536+8(%3,%6,2);
		pmaddwd		%%mm4,%%mm4;
		paddd		%%mm4,%%mm6;
		paddw		%%mm1,%%mm0;
		paddw		%9,%%mm0;
		psrlw		$1,%%mm0;
		psubw		%%mm0,%%mm2;
		movq		%%mm2,2304+8(%3,%6,2);
		pmaddwd		%%mm2,%%mm2;
		paddd		%%mm2,%%mm7;

		addl		$8,%6;
		decl		%7;
		jne		1b;

		movq		%%mm7,%%mm0;		psrlq		$32,%%mm7;
		paddd		%%mm7,%%mm0;		movq		%%mm5,%%mm1;
		psrlq		$32,%%mm5;		movq		%%mm6,%%mm2;
		movd		%%mm0,%0;		paddd		%%mm5,%%mm1;
		psrlq		$32,%%mm6;		pxor		%%mm7,%%mm7;
		movd		%%mm1,%1;		paddd		%%mm6,%%mm2;		
		movl		$256,%6;		movl		$16,%7;
		movd		%%mm2,%2;
2:
		movq		(%4,%6),%%mm0;
		movq		(%5,%6),%%mm1;		movq		%%mm0,%%mm5;
		movq		%%mm1,%%mm6;		punpcklbw	%%mm7,%%mm0;
		movq		(%3,%6,2),%%mm2;	punpckhbw	%%mm7,%%mm5;
		movq		%%mm2,%%mm3;		punpcklbw	%%mm7,%%mm1;
		psubw		%%mm0,%%mm3;		punpckhbw	%%mm7,%%mm6;		
		movq		%%mm2,%%mm4;		paddw		%%mm1,%%mm0;
		movq		%%mm3,768(%3,%6,2);	psubw		%%mm1,%%mm4;
		paddw		%9,%%mm0;
		movq		%%mm4,1536(%3,%6,2);
		psrlw		$1,%%mm0;
		psubw		%%mm0,%%mm2;
		movq		%%mm2,2304(%3,%6,2);

		movq		8(%3,%6,2),%%mm2;
		movq		%%mm2,%%mm3;
		movq		%%mm2,%%mm4;
		psubw		%%mm5,%%mm3;
		movq		%%mm3,768+8(%3,%6,2);
		psubw		%%mm6,%%mm4;
		movq		%%mm4,1536+8(%3,%6,2);
		paddw		%%mm6,%%mm5;
		paddw		%9,%%mm5;
		psrlw		$1,%%mm5;
		psubw		%%mm5,%%mm2;
		movq		%%mm2,2304+8(%3,%6,2);

		addl		$8,%6;
		decl		%7;
		jne		2b;
	"
	: "=&g" (n), "=&g" (*vmc1), "=&g" (*vmc2)
	: "r" (&mblock[0][0][0][0]), "r" (from1), "r" (from2), "r" (0), "r" (32),
	  "m" (c0), "m" (c1)
	: "6", "7", "cc", "memory" FPU_REGS);

	return n;
}
