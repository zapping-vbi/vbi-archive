#
#  MPEG-1 Real Time Encoder
# 
#  Copyright (C) 1999-2000 Michael H. Schimek
# 
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
# 
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#

# $Id: filter_mmx.s,v 1.1 2000-07-05 18:09:34 mschimek Exp $

	.text
	.align		16
	.globl		mmx_filterbank

mmx_filterbank:

	pushl %edx;
	pushl %ebx;					movl $filterbank_tables+1792,%edx;
	pushl %ecx;					movl $filterbank_tables+1728,%ebx;
	movq		(%edx),%mm0;			movl $filterbank_tables+1024,%ecx;		// 03 02 01 00
	movq		8(%edx),%mm1;			movq		%mm0,%mm2;	// 07 06 05 04
	movq		16(%edx),%mm6;			punpcklwd	%mm1,%mm0;	// 05 01 04 00
	movq		24(%edx),%mm7;			punpckhwd	%mm1,%mm2;	// 07 03 06 02
	movq		%mm0,%mm4;			punpcklwd	%mm2,%mm4;	// 06 04 02 00
	movq		%mm6,%mm3;			punpckhwd	%mm2,%mm0;	// 07 05 03 01
	movq		40(%edx),%mm1;			punpcklwd	%mm7,%mm6;
	movq		%mm0,32(%ebx);			punpckhwd	%mm7,%mm3;
	movq		%mm6,%mm5;			punpcklwd	%mm3,%mm5;
	movq		32(%edx),%mm0;			punpckhwd	%mm3,%mm6;
	movq		%mm0,%mm2;			punpcklwd	%mm1,%mm0;
	movq		%mm6,32+8(%ebx);		punpckhwd	%mm1,%mm2;
	movq		%mm0,%mm6;			punpcklwd	%mm2,%mm6;
	movq		48(%edx),%mm3;			punpckhwd	%mm2,%mm0;
	movq		56(%edx),%mm1;			movq		%mm3,%mm2;
	movq		%mm0,32+16(%ebx);		punpcklwd	%mm1,%mm3;
	movq		%mm3,%mm7;			punpckhwd	%mm1,%mm2;
	movq		%mm4,%mm0;			punpcklwd	%mm2,%mm7;
	punpckhwd	%mm2,%mm3;			punpcklwd	%mm5,%mm0;	// 10 02 08 00
	movq		%mm3,32+24(%ebx);		punpckhwd	%mm5,%mm4;	// 14 06 12 04
	movq		%mm0,%mm5;			punpcklwd	%mm4,%mm0;	// 12 08 04 00
	movq		%mm6,%mm1;			punpckhwd	%mm4,%mm5;	// 14 10 06 02 =
	punpcklwd	%mm7,%mm1;			punpckhwd	%mm7,%mm6;	// 26 18 24 16; 30 22 28 20
	movq		%mm1,%mm7;			punpcklwd	%mm6,%mm1;	// 28 24 20 16
	movq		%mm5,64(%eax);			punpckhwd	%mm6,%mm7;	// 30 26 22 18 =
	movq		%mm0,%mm2;			punpcklwd	%mm1,%mm0;	// 20 04 16 00 =
	movq		%mm7,72(%eax);			punpckhwd	%mm1,%mm2;	// 28 12 24 08 =
	movq		%mm0,%mm6;			punpckldq	%mm0,%mm0;	// 16 00 16 00
	pmaddwd		(%ecx),%mm0;			punpckhwd	%mm2,%mm6;	// 28 20 12 04 =
	movq		%mm6,%mm4;			punpckldq	%mm2,%mm2;	// 28 20 12  4; 24 08 24 08
	pmaddwd		8(%ecx),%mm2;			punpckldq	%mm4,%mm4;	// 12  4 12  4
	paddd		16(%ebx),%mm0;			punpckhdq	%mm6,%mm6;	// 28 20 28 20
	movq		24+16(%ecx),%mm3;		movq		%mm0,%mm1;
	paddd		%mm2,%mm0;			psubd		%mm2,%mm1;
	movq		16(%ecx),%mm5;			psrad		$3,%mm0;
	movq		8+16(%ecx),%mm7;		pmaddwd		%mm6,%mm3;
	movq		16+16(%ecx),%mm2;		pmaddwd		%mm4,%mm5;
	pmaddwd		%mm6,%mm7;			movq		%mm1,%mm6;
	pmaddwd		%mm4,%mm2;			psllq		$32,%mm6;
	movq		%mm0,%mm4;			psrlq		$32,%mm1;
	por		%mm6,%mm1;			paddd		%mm7,%mm5;	// 1 0
	paddd		%mm3,%mm2;			psrad		$3,%mm1;
	psrad		$3,%mm2;			psrad		$3,%mm5;
	paddd		%mm5,%mm0;			movq		%mm1,%mm3;
	movq		%mm0,(%eax);			paddd		%mm2,%mm1;
	movq		32+16(%ecx),%mm0;		psubd		%mm5,%mm4;	// 5 4
	movq		%mm1,8(%eax);			psubd		%mm2,%mm3;	// 7 6
	movq		%mm3,%mm6;			punpckldq	%mm4,%mm6;	// 4 6
	movq		16+32+16(%ecx),%mm2;		punpckhdq	%mm4,%mm3;	// 5 7
	movq		8+32+16(%ecx),%mm1;		movq		%mm3,%mm4;
	movq		64(%eax),%mm5;			punpckldq	%mm6,%mm3;	// 6 7
	movq		72(%eax),%mm7;			punpckhdq	%mm6,%mm4;	// 4 5
	movq		%mm3,16(%eax);			pmaddwd		%mm5,%mm0;	// 14 10  6  2
	movq		%mm4,24(%eax);			pmaddwd		%mm7,%mm1;	// 30 26 22 18
	movq		24+32+16(%ecx),%mm3;		pmaddwd		%mm5,%mm2;
	movq		0+80(%ecx),%mm4;		pmaddwd		%mm7,%mm3;	// 0 0
	movq		8+80(%ecx),%mm6;		pmaddwd		%mm5,%mm4;	// 14 10 06 02
	paddd		%mm1,%mm0;			pmaddwd		%mm7,%mm6;	// 30 26 22 18
	paddd		%mm3,%mm2;			movq		%mm0,%mm1;	// 1 1
	movq		(%eax),%mm3;			punpckldq	%mm2,%mm1;	// 1 0
	paddd		%mm4,%mm6;	    		punpckhdq	%mm2,%mm0;	// 1 0
	movq		16+80(%ecx),%mm2;		psrad		$3,%mm1;
	pmaddwd		%mm5,%mm2;			psrad		$3,%mm0;
	movq		%mm3,%mm4;			paddd		%mm0,%mm1;
	movq		24+80(%ecx),%mm0;		paddd		%mm1,%mm3;
	pmaddwd		%mm7,%mm0;			psubd		%mm1,%mm4;	// 0 0
	movq		%mm3,(%eax);			psrad		$3,%mm6;
	movq		24+112(%ecx),%mm3;		movq		%mm6,%mm1;	// 1 1
	pmaddwd		%mm7,%mm3;			paddd		%mm0,%mm2;	// 0 0
	movq		16+112(%ecx),%mm0;		psrad		$3,%mm2;
	pmaddwd		%mm5,%mm0;			punpckldq	%mm2,%mm1;	// 1 0
	punpckhdq	%mm2,%mm6;			leal		32+16(%ecx),%ecx; // 1 0
	movq		8(%eax),%mm2;			paddd		%mm6,%mm1;
	paddd		%mm3,%mm0;			movq		%mm2,%mm3;
	psubd		%mm1,%mm2;			paddd		%mm1,%mm3;
	movq		%mm2,%mm6;			punpckldq	%mm4,%mm2;	// 0 2
	movq		8+64(%ecx),%mm1;		punpckhdq	%mm4,%mm6;	// 1 3
	pmaddwd		%mm7,%mm1;			movq		%mm6,%mm4;
	movq		%mm3,8(%eax);			punpckldq	%mm2,%mm6;	// 0 1
	movq		64(%ecx),%mm3;			punpckhdq	%mm2,%mm4;	// 2 3
	movq		16(%eax),%mm2;			pmaddwd		%mm5,%mm3;
	movq		%mm4,56(%eax);			movq		%mm2,%mm4;
	movq		%mm6,48(%eax);			leal		30*4(%eax),%edx;
	movq		8+96(%ecx),%mm6;		paddd		%mm1,%mm3;
	movq		%mm3,%mm1;			punpckhdq	%mm0,%mm3;
	pmaddwd		%mm7,%mm6;			punpckldq	%mm0,%mm1;
	movq		96(%ecx),%mm0;			psrad		$3,%mm1;
	pmaddwd		%mm5,%mm0;			psrad		$3,%mm3;
	pmaddwd		24+96(%ecx),%mm7;		paddd		%mm3,%mm1;
	pmaddwd		16+96(%ecx),%mm5;		paddd		%mm1,%mm2;
	paddd		%mm6,%mm0;			psubd		%mm1,%mm4;
	movq		%mm2,16(%eax);			psrad		$3,%mm0;
	paddd		%mm7,%mm5;			psrad		$3,%mm5;
	movq		24(%eax),%mm7;			movq		%mm0,%mm3;
	movq		%mm7,%mm6;			punpckhdq	%mm5,%mm0;
	leal		128(%ecx),%ecx;			punpckldq	%mm5,%mm3;
	movq		32+8(%ebx),%mm1;		paddd		%mm0,%mm3;
	paddd		%mm3,%mm7;			psubd		%mm3,%mm6;
	movq		%mm6,%mm2;			punpckldq	%mm4,%mm6;	// 0 2
	movq		%mm7,24(%eax);			punpckhdq	%mm4,%mm2;	// 1 3
	movq		%mm2,%mm4;			punpckldq	%mm6,%mm2;	// 0 1
	movq		32(%ebx),%mm0;			punpckhdq	%mm6,%mm4;	// 2 3
	movq		%mm2,32(%eax);			movq		%mm0,%mm2;
	movq		%mm4,40(%eax);			jmp		2f;

	.align		16
2:	
	pmaddwd		(%ecx),%mm0;			movq		%mm1,%mm3;
	pmaddwd		8(%ecx),%mm1;			movq		%mm3,%mm5;
	pmaddwd		32(%ecx),%mm2;			leal		16(%eax),%eax;
	pmaddwd		40(%ecx),%mm3;			leal		-16(%edx),%edx;
	movq		32+16(%ebx),%mm4;		paddd		%mm1,%mm0;
	movq		32+24(%ebx),%mm1;		movq		%mm4,%mm6;
	pmaddwd		16(%ecx),%mm4;			movq		%mm1,%mm7;
	pmaddwd		24(%ecx),%mm1;			paddd		%mm3,%mm2;
	pmaddwd		48(%ecx),%mm6;			psrad		$3,%mm0;
	pmaddwd		56(%ecx),%mm7;			psrad		$3,%mm2;
	paddd		%mm1,%mm4;			movq		%mm0,%mm3;
	movq		-16(%eax),%mm1;			psrad		$3,%mm4;
	paddd		%mm7,%mm6;			movq		%mm5,%mm7;
	pmaddwd		8+64(%ecx),%mm5;		psrad		$3,%mm6;
	pmaddwd		40+64(%ecx),%mm7;		punpckhdq	%mm2,%mm0;
	punpckldq	%mm2,%mm3;			movq		%mm4,%mm2;
	paddd		%mm0,%mm3;			punpckhdq	%mm6,%mm4;
	paddd		%mm4,%mm3;			punpckldq	%mm6,%mm2;
	movq		32+24(%ebx),%mm4;		paddd		%mm2,%mm3;
	movq		32(%ebx),%mm2;			movq		%mm1,%mm0;
	movq		%mm4,%mm6;			psubd		%mm3,%mm0;
	pmaddwd		24+64(%ecx),%mm4;		paddd		%mm3,%mm1;
	movq		%mm1,-16(%eax);			movq		%mm2,%mm1;
	pmaddwd		64(%ecx),%mm1;			cmpl		%edx,%eax;
	pmaddwd		32+64(%ecx),%mm2;		leal		32*4(%ecx),%ecx;
	movd		%mm0,20(%edx);			psrlq		$32,%mm0;
	movq		32+16(%ebx),%mm3;		paddd		%mm5,%mm1;
	movd		%mm0,16(%edx);			movq		%mm3,%mm0;
	pmaddwd		56-64(%ecx),%mm6;		paddd		%mm7,%mm2;
	pmaddwd		16-64(%ecx),%mm0;		psrad		$3,%mm1;
	pmaddwd		48-64(%ecx),%mm3;		psrad		$3,%mm2;
	movq		%mm1,%mm7;			punpckhdq	%mm2,%mm1;
	paddd		%mm4,%mm0;			punpckldq	%mm2,%mm7;
	movq		-8(%eax),%mm4;			paddd		%mm6,%mm3;
	movq		%mm0,%mm2;			punpckhdq	%mm3,%mm0;
	paddd		%mm1,%mm7;			punpckldq	%mm3,%mm2;
	psrad		$3,%mm2;			movq		%mm4,%mm3;
	psrad		$3,%mm0;			paddd		%mm2,%mm7;
	movq		32+8(%ebx),%mm1;		paddd		%mm0,%mm7;
	paddd		%mm7,%mm4;			psubd		%mm7,%mm3;
	movq		32(%ebx),%mm0;			movq		%mm3,%mm6;
	movq		%mm4,-8(%eax);			psrlq		$32,%mm6;
	movd		%mm3,12(%edx);			movq		%mm0,%mm2;
	movd		%mm6,8(%edx);			jle		2b
	popl		%ecx;
	popl		%ebx;
	popl		%edx;
	ret

# void
# mmx_window_left(short *z [eax])
	
	.text
	.align		16
	.globl		mmx_window_mono

mmx_window_mono:

	pushl %edi;					leal 32(%eax),%eax;			// read z[0], z[1], ...
	pushl %esi;					movl $0,filterbank_tables+1744;		// .so.ud[0] shift-out
	pushl %ebx;					movl $filterbank_tables+1858,%edi;	// .y[31-2+4]
	pushl %ecx;					movl $filterbank_tables,%esi;		// .window_coeff
	pushl %edx;					movl $filterbank_tables+1728,%ebx;	// .c
	movl $8,%ecx;					movl $28*2,%edx;

	.align		16
1:
	movq		128*0(%eax),%mm0;		andl		$127,%edx;	// a3 a2 a1 a0
	movq		128*1(%eax),%mm1;		movq		%mm0,%mm4;	// b3 b2 b1 b0;
	movq		128*2(%eax),%mm2;		punpcklwd	%mm1,%mm0;	// c3 c2 c1 c0; b1 a1 b0 a0
	movq		128*3(%eax),%mm3;		movq		%mm2,%mm5;	// d3 d2 d1 d0;
	pmaddwd		(%esi),%mm0;			punpcklwd	%mm3,%mm2;	// 1 0 ba; d1 c1 d0 c0
	pmaddwd		16(%esi),%mm2;			punpckhwd	%mm1,%mm4;	// 1 0 dc; b3 a3 b2 a2
	movq		128*4(%eax),%mm1;		punpckhwd	%mm3,%mm5;	// e3 e2 e1 e0; d3 c3 d2 c2
	movq		128*5(%eax),%mm3;		movq		%mm1,%mm6;	// f3 f2 f1 f0;
	pmaddwd		8(%esi),%mm4;			punpcklwd	%mm3,%mm1;	// 3 2 ba; f1 e1 f0 e0
	pmaddwd		24(%esi),%mm5;			punpckhwd	%mm3,%mm6;	// 3 2 dc; f3 e3 f2 e2
	movq		128*6(%eax),%mm3;		paddd		%mm2,%mm0;	// ; 1 0 dcba
	movq		128*7(%eax),%mm7;		movq		%mm3,%mm2;
	pmaddwd		32(%esi),%mm1;			punpcklwd	%mm7,%mm3;
	pmaddwd		40(%esi),%mm6;			punpckhwd	%mm7,%mm2;
	pmaddwd		48(%esi),%mm3;			paddd		%mm5,%mm4;	// ; 3 2 dcba
	pmaddwd		56(%esi),%mm2;			paddd		%mm1,%mm0;	// ; 1 0 fedcba
	movq		128*0-64(%eax,%edx),%mm1;	paddd		%mm6,%mm4;	// A3 A2 A1 A0; 3 2 fedcba;
	movq		128*1-64(%eax,%edx),%mm5;	paddd		%mm3,%mm0;	// B3 B2 B1 B0; 1 0 hgfedcba =
	movq		128*2-64(%eax,%edx),%mm3;	movq		%mm1,%mm6;	// C3 C2 C1 C0;
	movq		128*3-64(%eax,%edx),%mm7;	punpcklwd	%mm5,%mm1;	// D3 D2 D1 D0; B1 A1 B0 A0
	movq		%mm0,24(%ebx);			movq		%mm3,%mm0;
	pmaddwd		64(%esi),%mm1;			punpcklwd	%mm7,%mm3;	// 1' 0' ba; D1 C1 D0 C0
	pmaddwd		64+16(%esi),%mm3;		punpckhwd	%mm5,%mm6;	// 1' 0' dc; B3 A3 B2 A2
	movq		128*4-64(%eax,%edx),%mm5;	punpckhwd	%mm7,%mm0;	// E3 E2 E1 E0; D3 C3 D2 C2
	movq		128*5-64(%eax,%edx),%mm7;	paddd		%mm2,%mm4;	// F3 F2 F1 F0; 3 2 hgfedcba =
	pmaddwd		64+8(%esi),%mm6;		paddd		%mm3,%mm1;	// 3' 2' ba; 1' 0' dcba 
	pmaddwd		64+24(%esi),%mm0;		movq		%mm5,%mm2;	// 3' 2' dc;
	movq		128*6-64(%eax,%edx),%mm3;	punpcklwd	%mm7,%mm5;	// ; F1 E1 F0 E0
	pmaddwd		64+32(%esi),%mm5;   		punpckhwd	%mm7,%mm2;	// 1' 0' fe; F3 E3 F2 E2 
	movq		128*7-64(%eax,%edx),%mm7;	paddd		%mm0,%mm6;	// ; 3' 2' dcba 
	pmaddwd		64+40(%esi),%mm2;		movq		%mm3,%mm0;	// 3' 2' fe;
	paddd		%mm5,%mm1;			punpcklwd	%mm7,%mm3;	// 1' 0' fedcba;  
	pmaddwd		64+48(%esi),%mm3;		punpckhwd	%mm7,%mm0;	// 1' 0' hg;
	pmaddwd		64+56(%esi),%mm0;		paddd		%mm2,%mm6;	// 3' 2' hg; 3' 2' fedcba
	movq		24(%ebx),%mm5;			addl		$8,%eax;	// 1 0 hgfedcba =
	movq		16(%ebx),%mm7;			paddd		%mm3,%mm1;	// so 0; 1' 0' hgfedcba =
	pxor		%mm5,%mm7;			paddd		%mm0,%mm6;	// c^B A; 3' 2' hgfedcba =
	pand		(%ebx),%mm5;			subl		$8,%edi;	// B 0
	pxor		%mm5,%mm7;			pxor		%mm4,%mm5;	// so A; B^D C
	pand		(%ebx),%mm4;			paddd		%mm7,%mm6;	// D 0; 31 32
	paddd		8(%ebx),%mm6;			pxor		%mm4,%mm5;	// B C
	movq		%mm4,16(%ebx);			paddd		%mm5,%mm1;	// so 0; 29 30
	paddd		8(%ebx),%mm1;			psrad		$16,%mm6;
	psrad		$16,%mm1;			subb		$16,%dl;
	packssdw	%mm6,%mm1;			decl		%ecx;
	leal		128(%esi),%esi;			punpckhdq	%mm4,%mm4;
	movq		%mm1,(%edi);			jnz		1b;
	psrad		$1,%mm4;			popl		%edx;		// (#0 * (1.0 * exp2(15))) >> 16
	movq		%mm4,16(%ebx);			popl		%ecx;
	popl		%ebx;
	popl		%esi;
	popl		%edi;
	ret

	.text
	.align		16
	.globl		mmx_window_left

mmx_window_left:

	pushl %edi;					leal 64(%eax),%eax;			// read z[0], z[2], ...
	pushl %esi;					movl $0,filterbank_tables+1744;		// .so.ud[0] shift-out
	pushl %ebx;					movl $filterbank_tables+1858,%edi;	// .y[31-2+4]
	pushl %ecx;					movl $filterbank_tables,%esi;		// .window_coeff
	pushl %edx;					movl $filterbank_tables+1728,%ebx;	// .c
	movl $8,%ecx;					movl $28*4,%edx;

	.align		16
1:
	movq		256*0(%eax),%mm0;		andl		$255,%edx;	// xx a1 xx a0
	movq		256*1(%eax),%mm1;		movq		%mm0,%mm2;	// xx b1 xx b0
	movq		256*2(%eax),%mm3;		punpcklwd	%mm1,%mm0;	// xx c1 xx c0; xx xx b0 a0
	movq		256*3(%eax),%mm4;		punpckhwd	%mm1,%mm2;	// xx d1 xx d0; xx xx b1 a1
	movq		%mm3,%mm5;			punpckldq	%mm2,%mm0;	// b1 a1 b0 a0
	pmaddwd		(%esi),%mm0;			punpcklwd	%mm4,%mm3;	// 1 0 ba; xx xx d0 c0
	movq		256*4(%eax),%mm1;		punpckhwd	%mm4,%mm5;	// xx e1 xx e0; xx xx d1 c1
	movq		256*5(%eax),%mm2;		punpckldq	%mm5,%mm3;	// xx f1 xx f0; d1 c1 d0 c0
	movq		%mm1,%mm6;			punpcklwd	%mm2,%mm1;	// xx xx f2 e2
	pmaddwd		16(%esi),%mm3;			punpckhwd	%mm2,%mm6;	// 1 0 dc; xx xx f3 e3
	movq		256*6(%eax),%mm7;		punpckldq	%mm6,%mm1;	// xx g1 xx g0; f3 e3 f2 e2
	pmaddwd		32(%esi),%mm1;			movq		%mm7,%mm5;	// 1 0 fe
	movq		256*7(%eax),%mm6;		paddd		%mm3,%mm0;	// xx h1 xx h0
	movq		256*0+8(%eax),%mm4;		punpcklwd	%mm6,%mm7;	// xx a3 xx a2; xx xx h2 g2
	paddd		%mm1,%mm0;			punpckhwd	%mm6,%mm5;	// xx xx h3 g3
	movq		256*1+8(%eax),%mm1;		punpckldq	%mm5,%mm7;	// xx b3 xx b2; h3 g3 h2 g2
	pmaddwd		48(%esi),%mm7;			movq		%mm4,%mm6;	// 1 0 hg
	movq		256*2+8(%eax),%mm5;		punpcklwd	%mm1,%mm4;	// xx c3 xx c2; xx xx b2 a2
	movq		256*3+8(%eax),%mm3;		punpckhwd	%mm1,%mm6;	// xx d3 xx d2; xx xx b3 a3
	paddd		%mm7,%mm0;			punpckldq	%mm6,%mm4;	// b3 a3 b2 a2
	movq		%mm5,%mm7;			punpcklwd	%mm3,%mm5;	// xx xx d2 c2
	pmaddwd		8(%esi),%mm4;			punpckhwd	%mm3,%mm7;	// 3 2 ba; xx xx d3 c3
	movq		256*4+8(%eax),%mm2;		punpckldq	%mm7,%mm5;	// xx e3 xx e2; d3 c3 d2 c2
	movq		256*5+8(%eax),%mm1;		movq		%mm2,%mm6;	// xx f3 xx f2
	pmaddwd		24(%esi),%mm5;			punpcklwd	%mm1,%mm2;	// 3 2 dc; xx xx f2 e2
	movq		256*6+8(%eax),%mm7;		punpckhwd	%mm1,%mm6;	// xx g3 xx g2; xx xx f3 e3
	movq		256*7+8(%eax),%mm3;		punpckldq	%mm6,%mm2;	// xx h3 xx h2; f3 e3 f2 e2
	pmaddwd		40(%esi),%mm2;			paddd		%mm5,%mm4;	// 3 2 fe
	movq		256*0-128(%eax,%edx),%mm1;	movq		%mm7,%mm5;	// xx A1 xx A0
	movq		256*1-128(%eax,%edx),%mm6;	punpcklwd	%mm3,%mm5;	// xx B1 xx B0; xx xx h2 g2
	paddd		%mm2,%mm4;			punpckhwd	%mm3,%mm7;	// xx xx h3 g3
	movq		%mm1,%mm2;			punpckldq	%mm7,%mm5;	// h3 g3 h2 g2
	pmaddwd		56(%esi),%mm5;			punpcklwd	%mm6,%mm1;	// 3 2 hg; xx xx B0 A0
	movq		256*2-128(%eax,%edx),%mm3;	punpckhwd	%mm6,%mm2;	// xx C1 xx C0; xx xx B1 A1
	movq		256*3-128(%eax,%edx),%mm7;	punpckldq	%mm2,%mm1;	// xx D1 xx D0; B1 A1 B0 A0
	pmaddwd		64+0(%esi),%mm1;		paddd		%mm5,%mm4;	// 1 0 BA; mm4 = 3 2 hgfedcba
	movq		%mm3,%mm6;			punpcklwd	%mm7,%mm3;	// xx xx D0 C0
	movq		256*4-128(%eax,%edx),%mm2;	punpckhwd	%mm7,%mm6;	// xx E1 xx E0; xx xx D1 C1
	movq		256*5-128(%eax,%edx),%mm5;	punpckldq	%mm6,%mm3;	// xx F1 xx F0; D1 C1 D0 C0
	movq		%mm0,24(%ebx);			movq		%mm2,%mm6;	
	pmaddwd		64+16(%esi),%mm3;		punpcklwd	%mm5,%mm2;	// 1 0 DC; xx xx F0 E0
	movq		256*6-128(%eax,%edx),%mm7;	punpckhwd	%mm5,%mm6;	// xx xx F1 E1
	movq		256*7-128(%eax,%edx),%mm5;	punpckldq	%mm6,%mm2;	// xx E1 xx E0; F1 E1 F0 E0
	pmaddwd		64+32(%esi),%mm2;		movq		%mm7,%mm6;	// xx F1 xx F0
	paddd		%mm3,%mm1;			punpcklwd	%mm5,%mm7;	// 1 0 FE; xx xx F0 E0
	movq		256*0-128+8(%eax,%edx),%mm0;	punpckhwd	%mm5,%mm6;	// xx A1 xx A0; xx xx F1 E1
	paddd		%mm2,%mm1;			punpckldq	%mm6,%mm7;	// F1 E1 F0 E0
	movq		256*1-128+8(%eax,%edx),%mm3;	movq		%mm0,%mm6;	// xx B1 xx B0
	pmaddwd		64+48(%esi),%mm7;		punpcklwd	%mm3,%mm0;	// 1 0 FE; xx xx B0 A0
	movq		256*2-128+8(%eax,%edx),%mm2;	punpckhwd	%mm3,%mm6;	// xx C1 xx C0; xx xx B1 A1
	movq		256*3-128+8(%eax,%edx),%mm5;	punpckldq	%mm6,%mm0;	// xx D1 xx D0; B1 A1 B0 A0
	pmaddwd		64+8(%esi),%mm0;		paddd		%mm7,%mm1;	// 1 0 BA; mm1 = 1' 0' hgfedcba
	movq		%mm2,%mm7;			punpcklwd	%mm5,%mm2;	// xx xx D0 C0
	movq		256*4-128+8(%eax,%edx),%mm3;	punpckhwd	%mm5,%mm7;	// xx E1 xx E0; xx xx D1 C1
	movq		256*5-128+8(%eax,%edx),%mm5;	punpckldq	%mm7,%mm2;	// xx F1 xx F0; D1 C1 D0 C0
	movq		%mm3,%mm7;			punpcklwd	%mm5,%mm3;	// xx xx F0 E0
	pmaddwd		64+24(%esi),%mm2;		punpckhwd	%mm5,%mm7;	// 1 0 DC; xx xx F1 E1
	movq		256*6-128+8(%eax,%edx),%mm6;	punpckldq	%mm7,%mm3;	// xx E1 xx E0; F1 E1 F0 E0
	movq		256*7-128+8(%eax,%edx),%mm5;	movq		%mm6,%mm7;	// xx F1 xx F0
	pmaddwd		64+40(%esi),%mm3;		punpcklwd	%mm5,%mm6;	// 1 0 FE; xx xx F0 E0
	paddd		%mm2,%mm0;			punpckhwd	%mm5,%mm7;	// xx xx F1 E1
	movq		24(%ebx),%mm5;			punpckldq	%mm7,%mm6;	// F1 E1 F0 E0; mm5 = 1 0 hgfedcba
	pmaddwd		64+56(%esi),%mm6;		paddd		%mm3,%mm0;	// 1 0 FE
	movq		16(%ebx),%mm7;			addl		$16,%eax;	// so
	pxor		%mm5,%mm7;			subl		$8,%edi;
	pand		(%ebx),%mm5;			paddd		%mm6,%mm0;	// B 0; mm0 = 3' 2' hgfedcba
	pxor		%mm5,%mm7;			pxor		%mm4,%mm5;	// so A; B^D C
	pand		(%ebx),%mm4;			paddd		%mm7,%mm0;	// D 0; 31 32
	paddd		8(%ebx),%mm0;			pxor		%mm4,%mm5;	// B C
	movq		%mm4,16(%ebx);			paddd		%mm5,%mm1;	// so 0; 29 30
	paddd		8(%ebx),%mm1;			psrad		$16,%mm0;
	psrad		$16,%mm1;			subl		$32,%edx;
	packssdw	%mm0,%mm1;			decl		%ecx;
	leal		128(%esi),%esi;			punpckhdq	%mm4,%mm4;
	movq		%mm1,(%edi);			jnz		1b;
	psrad		$1,%mm4;			popl		%edx;		// (#0 * (1.0 * exp2(15))) >> 16
	movq		%mm4,16(%ebx);			popl		%ecx;
	popl		%ebx;
	popl		%esi;
	popl		%edi;
	ret

	.text
	.align		16
	.globl		mmx_window_right

mmx_window_right:

	pushl %edi;					leal 64(%eax),%eax;			// read z[1], z[3], ...
	pushl %esi;					movl $0,filterbank_tables+1744;		// .so.ud[0] shift-out
	pushl %ebx;					movl $filterbank_tables+1858,%edi;	// .y[31-2+4]
	pushl %ecx;					movl $filterbank_tables,%esi;		// .window_coeff
	pushl %edx;					movl $filterbank_tables+1728,%ebx;	// .c
	movl $8,%ecx;					movl $28*4,%edx;

	.align		16
1:
	movq		256*0(%eax),%mm0;		andl		$255,%edx;	// a1 xx a0 xx
	movq		256*1(%eax),%mm1;		movq		%mm0,%mm2;	// b1 xx b0 xx
	movq		256*2(%eax),%mm3;		punpcklwd	%mm1,%mm0;	// c1 xx c0 xx; b0 a0 xx xx
	movq		256*3(%eax),%mm4;		punpckhwd	%mm1,%mm2;	// d1 xx d0 xx; b1 a1 xx xx
	movq		%mm3,%mm5;			punpckhdq	%mm2,%mm0;	// b1 a1 b0 a0
	pmaddwd		(%esi),%mm0;			punpcklwd	%mm4,%mm3;	// 1 0 ba; d0 c0 xx xx
	movq		256*4(%eax),%mm1;		punpckhwd	%mm4,%mm5;	// e1 xx e0 xx; d1 c1 xx xx
	movq		256*5(%eax),%mm2;		punpckhdq	%mm5,%mm3;	// f1 xx f0 xx; d1 c1 d0 c0
	movq		%mm1,%mm6;			punpcklwd	%mm2,%mm1;	// f2 e2 xx xx
	pmaddwd		16(%esi),%mm3;			punpckhwd	%mm2,%mm6;	// 1 0 dc; f3 e3 xx xx
	movq		256*6(%eax),%mm7;		punpckhdq	%mm6,%mm1;	// g1 xx g0 xx; f3 e3 f2 e2
	pmaddwd		32(%esi),%mm1;			movq		%mm7,%mm5;	// 1 0 fe
	movq		256*7(%eax),%mm6;		paddd		%mm3,%mm0;	// h1 xx h0 xx
	movq		256*0+8(%eax),%mm4;		punpcklwd	%mm6,%mm7;	// a3 xx a2 xx; h2 g2 xx xx
	paddd		%mm1,%mm0;			punpckhwd	%mm6,%mm5;	// h3 g3 xx xx
	movq		256*1+8(%eax),%mm1;		punpckhdq	%mm5,%mm7;	// b3 xx b2 xx; h3 g3 h2 g2
	pmaddwd		48(%esi),%mm7;			movq		%mm4,%mm6;	// 1 0 hg
	movq		256*2+8(%eax),%mm5;		punpcklwd	%mm1,%mm4;	// c3 xx c2 xx; b2 a2 xx xx
	movq		256*3+8(%eax),%mm3;		punpckhwd	%mm1,%mm6;	// d3 xx d2 xx; b3 a3 xx xx
	paddd		%mm7,%mm0;			punpckhdq	%mm6,%mm4;	// b3 a3 b2 a2
	movq		%mm5,%mm7;			punpcklwd	%mm3,%mm5;	// d2 c2 xx xx
	pmaddwd		8(%esi),%mm4;			punpckhwd	%mm3,%mm7;	// 3 2 ba; d3 c3 xx xx
	movq		256*4+8(%eax),%mm2;		punpckhdq	%mm7,%mm5;	// e3 xx e2 xx; d3 c3 d2 c2
	movq		256*5+8(%eax),%mm1;		movq		%mm2,%mm6;	// f3 xx f2 xx
	pmaddwd		24(%esi),%mm5;			punpcklwd	%mm1,%mm2;	// 3 2 dc; f2 e2 xx xx
	movq		256*6+8(%eax),%mm7;		punpckhwd	%mm1,%mm6;	// g3 xx g2 xx; f3 e3 xx xx
	movq		256*7+8(%eax),%mm3;		punpckhdq	%mm6,%mm2;	// h3 xx h2 xx; f3 e3 f2 e2
	pmaddwd		40(%esi),%mm2;			paddd		%mm5,%mm4;	// 3 2 fe
	movq		256*0-128(%eax,%edx),%mm1;	movq		%mm7,%mm5;	// xx A1 xx A0
	movq		256*1-128(%eax,%edx),%mm6;	punpcklwd	%mm3,%mm5;	// xx B1 xx B0; xx xx h2 g2
	paddd		%mm2,%mm4;			punpckhwd	%mm3,%mm7;	// xx xx h3 g3
	movq		%mm1,%mm2;			punpckhdq	%mm7,%mm5;	// h3 g3 h2 g2
	pmaddwd		56(%esi),%mm5;			punpcklwd	%mm6,%mm1;	// 3 2 hg; xx xx B0 A0
	movq		256*2-128(%eax,%edx),%mm3;	punpckhwd	%mm6,%mm2;	// xx C1 xx C0; xx xx B1 A1
	movq		256*3-128(%eax,%edx),%mm7;	punpckhdq	%mm2,%mm1;	// xx D1 xx D0; B1 A1 B0 A0
	pmaddwd		64+0(%esi),%mm1;		paddd		%mm5,%mm4;	// 1 0 BA; mm4 = 3 2 hgfedcba
	movq		%mm3,%mm6;			punpcklwd	%mm7,%mm3;	// xx xx D0 C0
	movq		256*4-128(%eax,%edx),%mm2;	punpckhwd	%mm7,%mm6;	// xx E1 xx E0; xx xx D1 C1
	movq		256*5-128(%eax,%edx),%mm5;	punpckhdq	%mm6,%mm3;	// xx F1 xx F0; D1 C1 D0 C0
	movq		%mm0,24(%ebx);			movq		%mm2,%mm6;	
	pmaddwd		64+16(%esi),%mm3;		punpcklwd	%mm5,%mm2;	// 1 0 DC; xx xx F0 E0
	movq		256*6-128(%eax,%edx),%mm7;	punpckhwd	%mm5,%mm6;	// xx xx F1 E1
	movq		256*7-128(%eax,%edx),%mm5;	punpckhdq	%mm6,%mm2;	// xx E1 xx E0; F1 E1 F0 E0
	pmaddwd		64+32(%esi),%mm2;		movq		%mm7,%mm6;	// xx F1 xx F0
	paddd		%mm3,%mm1;			punpcklwd	%mm5,%mm7;	// 1 0 FE; xx xx F0 E0
	movq		256*0-128+8(%eax,%edx),%mm0;	punpckhwd	%mm5,%mm6;	// xx A1 xx A0; xx xx F1 E1
	paddd		%mm2,%mm1;			punpckhdq	%mm6,%mm7;	// F1 E1 F0 E0
	movq		256*1-128+8(%eax,%edx),%mm3;	movq		%mm0,%mm6;	// xx B1 xx B0
	pmaddwd		64+48(%esi),%mm7;		punpcklwd	%mm3,%mm0;	// 1 0 FE; xx xx B0 A0
	movq		256*2-128+8(%eax,%edx),%mm2;	punpckhwd	%mm3,%mm6;	// xx C1 xx C0; xx xx B1 A1
	movq		256*3-128+8(%eax,%edx),%mm5;	punpckhdq	%mm6,%mm0;	// xx D1 xx D0; B1 A1 B0 A0
	pmaddwd		64+8(%esi),%mm0;		paddd		%mm7,%mm1;	// 1 0 BA; mm1 = 1' 0' hgfedcba
	movq		%mm2,%mm7;			punpcklwd	%mm5,%mm2;	// xx xx D0 C0
	movq		256*4-128+8(%eax,%edx),%mm3;	punpckhwd	%mm5,%mm7;	// xx E1 xx E0; xx xx D1 C1
	movq		256*5-128+8(%eax,%edx),%mm5;	punpckhdq	%mm7,%mm2;	// xx F1 xx F0; D1 C1 D0 C0
	movq		%mm3,%mm7;			punpcklwd	%mm5,%mm3;	// xx xx F0 E0
	pmaddwd		64+24(%esi),%mm2;		punpckhwd	%mm5,%mm7;	// 1 0 DC; xx xx F1 E1
	movq		256*6-128+8(%eax,%edx),%mm6;	punpckhdq	%mm7,%mm3;	// xx E1 xx E0; F1 E1 F0 E0
	movq		256*7-128+8(%eax,%edx),%mm5;	movq		%mm6,%mm7;	// xx F1 xx F0
	pmaddwd		64+40(%esi),%mm3;		punpcklwd	%mm5,%mm6;	// 1 0 FE; xx xx F0 E0
	paddd		%mm2,%mm0;			punpckhwd	%mm5,%mm7;	// xx xx F1 E1
	movq		24(%ebx),%mm5;			punpckhdq	%mm7,%mm6;	// F1 E1 F0 E0; mm5 = 1 0 hgfedcba
	pmaddwd		64+56(%esi),%mm6;		paddd		%mm3,%mm0;	// 1 0 FE
	movq		16(%ebx),%mm7;			addl		$16,%eax;	// so
	pxor		%mm5,%mm7;			subl		$8,%edi;
	pand		(%ebx),%mm5;			paddd		%mm6,%mm0;	// B 0; mm0 = 3' 2' hgfedcba
	pxor		%mm5,%mm7;			pxor		%mm4,%mm5;	// so A; B^D C
	pand		(%ebx),%mm4;			paddd		%mm7,%mm0;	// D 0; 31 32
	paddd		8(%ebx),%mm0;			pxor		%mm4,%mm5;	// B C
	movq		%mm4,16(%ebx);			paddd		%mm5,%mm1;	// so 0; 29 30
	paddd		8(%ebx),%mm1;			psrad		$16,%mm0;
	psrad		$16,%mm1;			subl		$32,%edx;
	packssdw	%mm0,%mm1;			decl		%ecx;
	leal		128(%esi),%esi;			punpckhdq	%mm4,%mm4;
	movq		%mm1,(%edi);			jnz		1b;
	psrad		$1,%mm4;			popl		%edx;		// (#0 * (1.0 * exp2(15))) >> 16
	movq		%mm4,16(%ebx);			popl		%ecx;
	popl		%ebx;
	popl		%esi;
	popl		%edi;
	ret
