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

/* $Id: predict.h,v 1.2 2000-09-25 17:08:57 mschimek Exp $ */

#include "mblock.h"

/*
 *  Forward prediction (P frames only)
 *
 *  mblock[1] = org - old_ref;
 *  mblock[3] = old_ref; 	// for reconstruction by idct_inter
 */
static inline int
predict_forward_packed(unsigned char *from)
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

static inline int
predict_forward_planar(unsigned char *from)
{
	int i, j, n, s = 0, s2 = 0;
	unsigned char *p;

	p = from;

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 8; j++) {
			mblock[1][0][i][j] = n = mblock[0][0][i][j] - p[j];
			mblock[3][0][i][j] = p[j];
			s += n;
			s2 += n * n;
			mblock[1][0][i + 16][j] = n = mblock[0][0][i + 16][j] - p[j + 8];
			mblock[3][0][i + 16][j] = p[j + 8];
			s += n;
			s2 += n * n;
		}

		p += mb_address.block[0].pitch;
	}

	p = from + (mb_address.block[0].pitch + 1) * 8 + mb_address.block[4].offset;

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			mblock[1][4][i][j] = mblock[0][4][i][j] - p[j];
			mblock[3][4][i][j] = p[j];
			mblock[1][5][i][j] = mblock[0][5][i][j] - p[j + mb_address.block[5].offset];
			mblock[3][5][i][j] = p[j + mb_address.block[5].offset];
		}

		p += mb_address.block[4].pitch;
	}

	return s2 * 256 - (s * s);
}

/*
 *  Backward prediction (B frames only, no reconstruction)
 *
 *  mblock[1] = org - new_ref;
 */
static inline int
predict_backward_packed(unsigned char *from)
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
predict_bidirectional_packed(unsigned char *from1, unsigned char *from2, int *vmc1, int *vmc2)
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

static inline int
predict_bidirectional_planar(unsigned char *from1, unsigned char *from2, int *vmc1, int *vmc2)
{
	int i, j, n, si = 0, sf = 0, sb = 0;
	unsigned char *p1, *p2;

	p1 = from1;
	p2 = from2;

	for (i = 0; i < 16; i++) {
		for (j = 0; j < 8; j++) {
			mblock[1][0][i][j] = n = mblock[0][0][i][j] - p1[j];
			sf += n * n; // forward
			mblock[2][0][i][j] = n = mblock[0][0][i][j] - p2[j];
			sb += n * n; // backward
			mblock[3][0][i][j] = n = mblock[0][0][i][j] - ((p1[j] + p2[j] + 1) >> 1);
			si += n * n; // interpolated	                   unsigned -> pavgb
			mblock[1][0][i + 16][j] = n = mblock[0][0][i + 16][j] - p1[j + 8];
			sf += n * n; // forward
			mblock[2][0][i + 16][j] = n = mblock[0][0][i + 16][j] - p2[j + 8];
			sb += n * n; // backward
			mblock[3][0][i + 16][j] = n = mblock[0][0][i + 16][j] - ((p1[j + 8] + p2[j + 8] + 1) >> 1);
			si += n * n; // interpolated	                   unsigned -> pavgb
		}

		p1 += mb_address.block[0].pitch;
		p2 += mb_address.block[0].pitch;
	}

	p1 = from1 + (mb_address.block[0].pitch + 1) * 8 + mb_address.block[4].offset;
	p2 = from2 + (mb_address.block[0].pitch + 1) * 8 + mb_address.block[4].offset;

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			mblock[1][4][i][j] = mblock[0][4][i][j] - p1[j];
			mblock[2][4][i][j] = mblock[0][4][i][j] - p2[j];
			mblock[3][4][i][j] = mblock[0][4][i][j] - ((p1[j] + p2[j] + 1) >> 1);
			mblock[1][5][i][j] = mblock[0][5][i][j] - p1[j + mb_address.block[5].offset];
			mblock[2][5][i][j] = mblock[0][5][i][j] - p2[j + mb_address.block[5].offset];
			mblock[3][5][i][j] = mblock[0][5][i][j] - ((p1[j + mb_address.block[5].offset] + p2[j + mb_address.block[5].offset] + 1) >> 1);
		}

		p1 += mb_address.block[4].pitch;
		p2 += mb_address.block[4].pitch;
	}

	*vmc1 = sf;
	*vmc2 = sb;

	return si;
}

/*
 *  MMX
 */

static inline int
mmx_predict_bidirectional_packed(unsigned char *from1, unsigned char *from2, int *vmc1, int *vmc2)
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
