#
#  MPEG-1 Real Time Encoder
# 
#  Copyright (C) 1999-2001 Michael H. Schimek
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

# $Id: dct_mmx.s,v 1.7 2001-06-29 01:29:10 mschimek Exp $

	.text
	.align		16
	.globl		mmx_fdct_intra

# scheduled for PMMX, could be faster on P6/K7

mmx_fdct_intra:

	pushl		%esi;
	movzbl		ltp(%eax),%esi;
	xorl		%ecx,%ecx;
	movzbl		mmx_q_fdct_intra_sh(%eax),%eax;
	sall		$7,%esi;
	movl		%eax,csh;			decl		%eax;
	bts		%eax,%ecx;			addl		$mmx_q_fdct_intra_q_lut,%esi;
	pushl		%ebx;				addl		$16,%eax;
	bts		%eax,%ecx;			movl		$mblock,%eax;
	movl		%ecx,crnd;			movl		$mblock+1536,%ebx;
	movl		%ecx,crnd+4;			movl		$6,%ecx;

	.align		16
1:
	movq		7*16+0(%eax),%mm0;		movq		(%eax),%mm7;
	movq		1*16+0(%eax),%mm6;		decl		%ecx;
	movq		6*16+0(%eax),%mm1;		paddw		%mm7,%mm0;
	movq		2*16+0(%eax),%mm5;		paddw		%mm7,%mm7;
	movq		5*16+0(%eax),%mm2;		psubw		%mm0,%mm7;
	movq		3*16+0(%eax),%mm4;		paddw		%mm6,%mm1;
	movq		4*16+0(%eax),%mm3;		paddw		%mm6,%mm6;
	paddw		%mm5,%mm2;			psubw		%mm1,%mm6;
	paddw		%mm4,%mm3;			paddw		%mm4,%mm4;
	paddw		%mm5,%mm5;			psubw		%mm3,%mm4;
	psubw		%mm2,%mm5;			psubw		%mm2,%mm1;
	psubw		%mm3,%mm0;			paddw		%mm2,%mm2;
	paddw		%mm1,%mm2;			paddw		%mm3,%mm3;
	paddw		%mm0,%mm3;			paddw		%mm0,%mm1;
	paddw		%mm3,%mm2;			psllw		$3,%mm1;
	paddw		%mm5,%mm4;			psllw		$4,%mm3;
	paddw		%mm6,%mm5;			psllw		$3,%mm2;
	pmulhw		cC4_15,%mm1;			psllw		$3,%mm5;
	pmulhw		cC4C6_14,%mm5;			psubw		%mm2,%mm3;
	movq		%mm3,4*16+0(%ebx);		psllw		$2,%mm0;
	paddw		%mm7,%mm6;			psllw		$4,%mm7;
	paddw		%mm0,%mm1;			movq		%mm6,%mm3;
	pmulhw		c1C6_13,%mm7;			paddw		%mm0,%mm0;
	psubw		%mm1,%mm0;			psllw		$3,%mm3;
	movq		%mm0,6*16+0(%ebx);		paddw		%mm4,%mm6;
	pmulhw		cC4_15,%mm3;			movq		%mm4,%mm0;
	paddw		%mm6,%mm6;			psllw		$3,%mm0;
	paddw		%mm7,%mm5; 			psllw		$2,%mm4;
	pmulhw		cC4_15,%mm0;			paddw		%mm7,%mm7;
	psubw		%mm6,%mm4;	 		psubw		%mm5,%mm7;
	paddw		%mm7,%mm4;			paddw		%mm7,%mm7;
	paddw		%mm0,%mm4;			paddw		%mm3,%mm6;
	psubw		c128_6,%mm2;			paddw		%mm5,%mm6;
	psubw		%mm4,%mm7;			paddw		%mm5,%mm5;
	movq		%mm2,%mm0;			punpcklwd	%mm6,%mm2;
	psubw		%mm6,%mm5;			punpckhwd	%mm6,%mm0;
	movq		%mm1,%mm6;			punpcklwd	%mm7,%mm1;
	movq		c1,%mm3;			punpckhwd	%mm7,%mm6;
	movq		%mm2,%mm7;			punpckldq	%mm1,%mm2;
        movq		%mm2,(%ebx);			punpckhdq	%mm1,%mm7;
	movq		%mm0,%mm2;			punpckldq	%mm6,%mm0;
	paddw		%mm3,%mm7;			punpckhdq	%mm6,%mm2;
	movq		4*16+0(%ebx),%mm6;		paddw		%mm3,%mm0;
        movq		%mm7,1*16+0(%ebx);		paddw		%mm3,%mm2;
	movq		%mm6,%mm1;			punpcklwd	%mm4,%mm6;
	movq		6*16+0(%ebx),%mm7;		punpckhwd	%mm4,%mm1;
	movq		%mm7,%mm4;			punpcklwd	%mm5,%mm7;
	movq		%mm0,2*16+0(%ebx);		punpckhwd	%mm5,%mm4;
	movq		%mm6,%mm0;			punpckldq	%mm7,%mm6;
	movq		%mm2,3*16+0(%ebx);		punpckhdq	%mm7,%mm0;
	movq		%mm1,%mm7;			punpckldq	%mm4,%mm1;
	movq		%mm6,0*16+8(%ebx);		paddw		%mm3,%mm0;
	movq		6*16+8(%eax),%mm2;		punpckhdq	%mm4,%mm7;
	movq		%mm0,1*16+8(%ebx);		paddw		%mm3,%mm1;
	movq		1*16+8(%eax),%mm6;		paddw		%mm3,%mm7;
	movq		%mm1,2*16+8(%ebx);		paddw		%mm6,%mm2;
	movq		2*16+8(%eax),%mm5;		paddw		%mm6,%mm6;
	movq		5*16+8(%eax),%mm1; 		psubw		%mm2,%mm6;
	movq		%mm7,3*16+8(%ebx);		paddw		%mm5,%mm1;
	movq		0*16+8(%eax),%mm7;		paddw		%mm5,%mm5;
	movq		7*16+8(%eax),%mm0; 		psubw		%mm1,%mm5;
	movq		4*16+8(%eax),%mm3; 		paddw		%mm7,%mm0;
	movq		3*16+8(%eax),%mm4;		paddw		%mm7,%mm7;
	psubw		%mm0,%mm7;			paddw		%mm2,%mm1;
	paddw		%mm4,%mm3; 			paddw		%mm4,%mm4;
	psubw		%mm3,%mm4;			paddw		%mm0,%mm3;
	paddw		%mm0,%mm0; 			paddw		%mm2,%mm2;
	psubw		%mm3,%mm0;			psubw		%mm1,%mm2;
	paddw		%mm0,%mm2; 			psllw		$2,%mm0;
	paddw		%mm3,%mm1; 			psllw		$3,%mm2;
	pmulhw		cC4_15,%mm2;			psllw		$3,%mm1;
	paddw		%mm5,%mm4; 			psllw		$4,%mm3;
	psubw		%mm1,%mm3;			paddw		%mm6,%mm5;
	paddw		%mm7,%mm6;			psllw		$3,%mm5;
	movq		%mm3,4*16+8(%ebx); 		psllw		$4,%mm7;
	pmulhw		cC4C6_14,%mm5;			paddw		%mm0,%mm2;
	paddw		%mm0,%mm0; 			psubw		%mm2,%mm0;
	pmulhw		c1C6_13,%mm7;			movq		%mm6,%mm3;
	movq		%mm0,6*16+8(%ebx);		psllw		$3,%mm3;
	pmulhw		cC4_15,%mm3;			movq		%mm4,%mm0;
	paddw		%mm4,%mm6; 			psllw		$3,%mm0;
	pmulhw		cC4_15,%mm0;			paddw		%mm6,%mm6;
	paddw		%mm7,%mm5;			paddw		%mm7,%mm7;
	psubw		%mm5,%mm7;			psllw		$2,%mm4;
	psubw		c128_6,%mm1;			psubw		%mm6,%mm4;
	paddw		%mm3,%mm6;			paddw		%mm7,%mm4;
	paddw		%mm0,%mm4;			paddw		%mm7,%mm7;
	psubw		%mm4,%mm7;			paddw		%mm5,%mm6;
	movq		%mm1,%mm0;			punpcklwd	%mm6,%mm1;
	paddw		%mm5,%mm5; 			punpckhwd	%mm6,%mm0;
	movq		%mm2,%mm3;			punpcklwd	%mm7,%mm2;
	psubw		%mm6,%mm5;			punpckhwd	%mm7,%mm3;
	movq		%mm1,%mm6;			punpckldq	%mm2,%mm1;
	movq		%mm0,%mm7;			punpckldq	%mm3,%mm0;
	movq		%mm1,4*16+0(%ebx);		punpckhdq	%mm2,%mm6;
	movq		4*16+8(%ebx),%mm2;		punpckhdq	%mm3,%mm7;
	movq		%mm0,6*16+0(%ebx);		movq		%mm2,%mm0;
	movq		6*16+8(%ebx),%mm1;		punpcklwd	%mm4,%mm2;
	movq		%mm6,5*16+0(%ebx);		punpckhwd	%mm4,%mm0;
	movq		%mm1,%mm3;			punpcklwd	%mm5,%mm1;
	movq		%mm7,7*16+0(%ebx);    		punpckhwd	%mm5,%mm3;
	movq		%mm2,%mm4;			punpckldq	%mm1,%mm2;
	movq		1*16+8(%ebx),%mm6;    		punpckhdq	%mm1,%mm4;
	movq		%mm0,%mm5;			punpckldq	%mm3,%mm5;
	movq		0*16+8(%ebx),%mm7;		punpckhdq	%mm3,%mm0;
	movq		3*16+8(%ebx),%mm1;		paddw		%mm6,%mm5;
	movq		2*16+8(%ebx),%mm3;		paddw		%mm6,%mm6;
	paddw		%mm7,%mm0; 			psubw		%mm5,%mm6;
	paddw		%mm7,%mm7; 			paddw		%mm1,%mm2;
	psubw		%mm0,%mm7;			paddw		%mm1,%mm1;
	paddw		%mm3,%mm4; 			paddw		%mm3,%mm3;
	psubw		%mm2,%mm1;			psubw		%mm4,%mm3;
	paddw		%mm0,%mm2; 			paddw		%mm0,%mm0;
	paddw		%mm5,%mm4; 			paddw		%mm5,%mm5;
	psubw		%mm2,%mm0;			psraw		$1,%mm2;
	psubw		%mm4,%mm5;			psraw		$1,%mm4;
	paddw		%mm2,%mm4;			paddw		%mm2,%mm2;
	movq		%mm0,0*16+8(%ebx);		psubw		%mm4,%mm2;
	pmulhw		0*16+8(%esi),%mm4;		paddw		%mm3,%mm1;
	movq		crnd,%mm0;			paddw		%mm6,%mm3;
	pmulhw		4*16+8(%esi),%mm2;		paddw		%mm0,%mm4;
	psraw		csh,%mm4;			paddw		%mm7,%mm6;
	pmulhw		cC4C6_14,%mm3;			paddw		%mm0,%mm2;
        movq		%mm4,0*16+8+768(%eax);		movq		%mm1,%mm4;
        psraw		csh,%mm2;	        	psraw		$3,%mm1; 
	movq		%mm2,4*16+8+768(%eax);		movq		%mm6,%mm2;
	pmulhw		c1C6_13,%mm7;			psraw		$3,%mm6;
	pmulhw		cC2C61_13,%mm4;			paddw		%mm1,%mm6;
	pmulhw		cC2C61_13,%mm2;			paddw		%mm1,%mm1;
	psubw		%mm6,%mm1;			psraw		$1,%mm3; // red
	paddw		%mm4,%mm1; 			paddw		%mm7,%mm3;
	paddw		%mm2,%mm6;			paddw		%mm7,%mm7;
	psubw		%mm3,%mm7;			paddw		%mm3,%mm6;
	movq		csh,%mm4;			psraw		$1,%mm5;
	paddw		%mm7,%mm1; 			paddw		%mm7,%mm7;
	psubw		%mm1,%mm7;			paddw		%mm3,%mm3;
	movq		0*16+8(%ebx),%mm2;		psubw		%mm6,%mm3;
	pmulhw		5*16+8(%esi),%mm1;		psraw		$1,%mm2;
	pmulhw		3*16+8(%esi),%mm7;		paddw		%mm2,%mm5;
	pmulhw		cC4_15,%mm5; 			psraw		$1,%mm2;
	pmulhw		7*16+8(%esi),%mm3; 		paddw		%mm0,%mm1;
	pmulhw		1*16+8(%esi),%mm6;		psraw		%mm4,%mm1;
	paddw		%mm2,%mm5; 			paddw		%mm2,%mm2;
	movq		%mm1,5*16+8+768(%eax);		psubw		%mm5,%mm2;
	pmulhw		6*16+8(%esi),%mm2;		paddw		%mm0,%mm7;
	psraw		%mm4,%mm7;			paddw		%mm0,%mm3;
	pmulhw		2*16+8(%esi),%mm5;		paddw		%mm0,%mm6;
	paddw		%mm0,%mm2;			psraw		%mm4,%mm6;
	movq		%mm7,3*16+8+768(%eax);		psraw		%mm4,%mm3;
	movq		%mm3,7*16+8+768(%eax);		psraw		%mm4,%mm2;
	movq		7*16+0(%ebx),%mm1; 		paddw		%mm0,%mm5;
	movq		(%ebx),%mm7;			psraw		%mm4,%mm5;
	movq		%mm6,1*16+8+768(%eax);		paddw		%mm7,%mm1;
	movq		2*16+0(%ebx),%mm3;		paddw		%mm7,%mm7;
	movq		5*16+0(%ebx),%mm4; 		psubw		%mm1,%mm7;
	movq		%mm2,6*16+8+768(%eax);		paddw		%mm3,%mm4;
	movq		3*16+0(%ebx),%mm0;		paddw		%mm3,%mm3;
	movq		4*16+0(%ebx),%mm2; 		psubw		%mm4,%mm3;
	movq		%mm5,2*16+8+768(%eax);		paddw		%mm0,%mm2;
	movq		1*16+0(%ebx),%mm6;		paddw		%mm0,%mm0;
	movq		6*16+0(%ebx),%mm5; 		psubw		%mm2,%mm0;
	paddw		%mm1,%mm2; 			paddw		%mm1,%mm1;
	paddw		%mm6,%mm5; 			paddw		%mm6,%mm6;
	psubw		%mm5,%mm6;			psubw		%mm2,%mm1;
	paddw		%mm5,%mm4; 			paddw		%mm5,%mm5;
	psubw		%mm4,%mm5;			psraw		$1,%mm4; 
	movq		%mm5,(%ebx);			psraw		$1,%mm2;
	paddw		%mm2,%mm4; 			paddw		%mm2,%mm2;
	psubw		%mm4,%mm2;			psraw		$1,%mm1;
	pmulhw		4*16+0(%esi),%mm2;		movq		%mm4,%mm5;
	pmulhw		(%esi),%mm4;			paddw		%mm3,%mm0;
	paddw		c128,%mm5;			paddw		%mm6,%mm3;
	paddw		crnd,%mm2;			psraw		$8,%mm5;
	psraw		csh,%mm2;			psllq		$48,%mm5;
	paddw		crnd,%mm4;			psrlq		$48,%mm5;
	movq		%mm2,4*16+0+768(%eax);		paddw		%mm7,%mm6;
	psraw		csh,%mm4;			movq		%mm6,%mm2;
	pmulhw		cC2C61_13,%mm2;			por		%mm5,%mm4;
	pmulhw		cC4C6_14,%mm3;			psraw		$3,%mm6;
	movq		%mm4,768(%eax);			movq		%mm0,%mm4;
	pmulhw		c1C6_13,%mm7;			psraw		$3,%mm0; 
	pmulhw		cC2C61_13,%mm4;			paddw		%mm0,%mm6;
	paddw		%mm0,%mm0; 			psraw		$1,%mm3; // red
	paddw		%mm7,%mm3; 			psubw		%mm6,%mm0;
	paddw		%mm4,%mm0; 			paddw		%mm7,%mm7;
	paddw		%mm2,%mm6;			psubw		%mm3,%mm7;
	movq		crnd,%mm5;			paddw		%mm7,%mm0;
	paddw		%mm7,%mm7; 			leal		128(%eax),%eax;
	movq		(%ebx),%mm4;			paddw		%mm3,%mm6;
	psubw		%mm0,%mm7;			paddw		%mm3,%mm3;
	pmulhw		5*16+0(%esi),%mm0;		psraw		$1,%mm4;
	movq		csh,%mm2;			paddw		%mm1,%mm4;
	pmulhw		cC4_15,%mm4;			psubw		%mm6,%mm3;
	pmulhw		3*16+0(%esi),%mm7;		psraw		$1,%mm1;
	pmulhw		1*16+0(%esi),%mm6;		paddw		%mm1,%mm4;
	pmulhw		7*16+0(%esi),%mm3;		paddw		%mm1,%mm1;
	psubw		%mm4,%mm1;			paddw		%mm5,%mm0;
	pmulhw		2*16+0(%esi),%mm4;		psraw		%mm2,%mm0;
	pmulhw		6*16+0(%esi),%mm1;		paddw		%mm5,%mm7;
	paddw		%mm5,%mm6;			psraw		%mm2,%mm7;
	movq		%mm0,5*16-128+768(%eax);	psraw		%mm2,%mm6;
	movq		%mm7,3*16-128+768(%eax);	paddw		%mm5,%mm3;
	movq		%mm6,1*16-128+768(%eax);	psraw		%mm2,%mm3;
	paddw		%mm5,%mm4;			psraw		%mm2,%mm4;
	movq		%mm3,7*16-128+768(%eax);	paddw		%mm5,%mm1;
	movq		%mm4,2*16-128+768(%eax);	psraw		%mm2,%mm1;
	movq		%mm1,6*16-128+768(%eax);	jne		1b;

	popl		%ebx;				
	popl		%esi;
	ret

	.text
	.align		16
	.globl		mmx_fdct_inter

mmx_fdct_inter:

	pushl		%ebx;				movl		$mblock+768*2,%ebx;
	pushl		%esi;				cmpl		%eax,%ebx;
	pushl		%edi;				movl		%eax,%esi;
	movl		$0,%eax;
	movl		$mblock-128,%edi;		movl		$6,%ecx;
	jne		1f;
	movl		$mblock+768*1,%ebx;

	.align		16
1:
	movq		(%esi),%mm7;
	leal		128(%edi),%edi;
	movq		7*16+0(%esi),%mm0;
	movq		1*16+0(%esi),%mm6;
	paddw		%mm7,%mm0;
	movq		6*16+0(%esi),%mm1;
	paddw		%mm7,%mm7;
	movq		2*16+0(%esi),%mm5;
	psubw		%mm0,%mm7;
	movq		5*16+0(%esi),%mm2;
	paddw		%mm6,%mm1;
	movq		3*16+0(%esi),%mm4;
	paddw		%mm6,%mm6;
	movq		4*16+0(%esi),%mm3;
	psubw		%mm1,%mm6;
	paddw		%mm5,%mm2;
	paddw		%mm5,%mm5;
	paddw		%mm4,%mm3;
	psubw		%mm2,%mm5;
	paddw		%mm4,%mm4;
	psubw		%mm2,%mm1;
	psubw		%mm3,%mm4;
	paddw		%mm0,%mm3;
	paddw		%mm0,%mm0;
	paddw		%mm2,%mm2;
	psubw		%mm3,%mm0;
	paddw		%mm1,%mm2;
	paddw		%mm0,%mm1;
	psllw		$2,%mm0;
	psllw		$3,%mm1;
	paddw		%mm3,%mm2;
	paddw		c1,%mm1;
	paddw		%mm2,%mm2;
	paddw		%mm5,%mm4;
	psllw		$2,%mm3;
	pmulhw		cC4_15,%mm1;
	psubw		%mm2,%mm3;
	paddw		%mm6,%mm5;
	paddw		%mm7,%mm6;
	movq		%mm3,4*16+0(%ebx);
	psllw		$4,%mm5;
	pmulhw		cC4C6_14,%mm5;
	paddw		%mm0,%mm1;
	paddw		%mm0,%mm0;
	psllw		$5,%mm7;
	pmulhw		c1C6_13,%mm7;
	psubw		%mm1,%mm0;
	paddw		c2,%mm1;
	movq		%mm6,%mm3;
	movq		%mm0,6*16+0(%ebx);
	psraw		$2,%mm1;
	movq		%mm4,%mm0;
	psllw		$4,%mm3;
	paddw		%mm7,%mm5;
	psllw		$4,%mm0;
	pmulhw		cC4_15,%mm0;
	paddw		%mm7,%mm7;
	pmulhw		cC4_15,%mm3;
	psubw		%mm5,%mm7;
	paddw		%mm4,%mm6;
	psllw		$3,%mm4;
	paddw		%mm0,%mm4;
	psllw		$2,%mm6;
	psubw		%mm6,%mm4;
	paddw		%mm3,%mm6;
	paddw		%mm5,%mm6;
	paddw		%mm5,%mm5;
	paddw		%mm7,%mm4;
	psubw		%mm6,%mm5;
	paddw		c4,%mm6;
	paddw		%mm7,%mm7;
	movq		%mm2,%mm3;
	psraw		$3,%mm6;
	psubw		%mm4,%mm7;
	punpcklwd	%mm6,%mm2;
	paddw		c4,%mm7;
	punpckhwd	%mm6,%mm3;
	movq		%mm1,%mm0;
	psraw		$3,%mm7;
	paddw		c2,%mm4;
	punpcklwd	%mm7,%mm1;
	punpckhwd	%mm7,%mm0;
	movq		%mm2,%mm7;
	paddw		c1,%mm5;
	punpckldq	%mm1,%mm2;
	punpckhdq	%mm1,%mm7;
	movq		%mm3,%mm1;
	movq		%mm2,0*16+0(%ebx);
	punpckldq	%mm0,%mm1;
	movq		4*16+0(%ebx),%mm6;
	punpckhdq	%mm0,%mm3;
	movq		%mm7,1*16+0(%ebx);
	psraw		$2,%mm4;
	movq		6*16+0(%ebx),%mm0;
	psraw		$1,%mm5;
	movq		%mm0,%mm7;
	punpcklwd	%mm5,%mm0;
	movq		%mm1,2*16+0(%ebx);
	punpckhwd	%mm5,%mm7;
	movq		%mm3,3*16+0(%ebx);
	movq		%mm6,%mm3;
	movq		2*16+8(%esi),%mm5;
	punpcklwd	%mm4,%mm3;
	movq		5*16+8(%esi),%mm2;
	punpckhwd	%mm4,%mm6;
	movq		%mm3,%mm4;
	punpckldq	%mm0,%mm3;
	movq		0*16+8(%esi),%mm1;
	punpckhdq	%mm0,%mm4;
	movq		%mm6,%mm0;
	punpckldq	%mm7,%mm0;
	movq		%mm3,0*16+8(%ebx);
	punpckhdq	%mm7,%mm6;
	movq		7*16+8(%esi),%mm3;
	paddw		%mm5,%mm2;
	movq		%mm0,2*16+8(%ebx);
	paddw		%mm5,%mm5;
	movq		%mm4,1*16+8(%ebx);
	psubw		%mm2,%mm5;
	movq		4*16+8(%esi),%mm0;
	paddw		%mm1,%mm3;
	movq		3*16+8(%esi),%mm4;
	paddw		%mm1,%mm1;
	movq		%mm6,3*16+8(%ebx);
	psubw		%mm3,%mm1;
	movq		6*16+8(%esi),%mm7;
	paddw		%mm4,%mm0;
	movq		1*16+8(%esi),%mm6;
	leal		128(%esi),%esi;
	paddw		%mm4,%mm4;
	psubw		%mm0,%mm4;
	paddw		%mm6,%mm7;
	paddw		%mm6,%mm6;
	paddw		%mm3,%mm0;
	psubw		%mm7,%mm6;
	psubw		%mm2,%mm7;
	paddw		%mm3,%mm3;
	paddw		%mm2,%mm2;
	psubw		%mm0,%mm3;
	paddw		%mm7,%mm2;
	paddw		%mm3,%mm7;
	psllw		$2,%mm3;
	paddw		%mm0,%mm2;
	psllw		$3,%mm7;
	paddw		c1,%mm7;
	paddw		%mm2,%mm2;
	paddw		%mm5,%mm4;
	psllw		$2,%mm0;
	pmulhw		cC4_15,%mm7;
	psubw		%mm2,%mm0;
	paddw		%mm6,%mm5;
	paddw		%mm1,%mm6;
	movq		%mm0,4*16+8(%ebx);
	psllw		$4,%mm5;
	pmulhw		cC4C6_14,%mm5;
	psllw		$5,%mm1;
	paddw		%mm3,%mm7;
	paddw		%mm3,%mm3;
	pmulhw		c1C6_13,%mm1;
	psubw		%mm7,%mm3;
	paddw		c2,%mm7;
	movq		%mm6,%mm0;
	movq		%mm3,6*16+8(%ebx);
	psraw		$2,%mm7;
	paddw		%mm1,%mm5;
	movq		%mm4,%mm3;
	paddw		%mm1,%mm1;
	psllw		$4,%mm3;
	pmulhw		cC4_15,%mm3;
	psllw		$4,%mm0;
	pmulhw		cC4_15,%mm0;
	paddw		%mm4,%mm6;
	psubw		%mm5,%mm1;
	psllw		$3,%mm4;
	paddw		%mm3,%mm4;
	psllw		$2,%mm6;
	psubw		%mm6,%mm4;
	paddw		%mm0,%mm6;
	paddw		%mm5,%mm6;
	paddw		%mm5,%mm5;
	psubw		%mm6,%mm5;
	paddw		%mm1,%mm4;
	paddw		c4,%mm6;
	paddw		%mm1,%mm1;
	psubw		%mm4,%mm1;
	psraw		$3,%mm6;
	movq		%mm2,%mm0;
	punpcklwd	%mm6,%mm2;
	paddw		c4,%mm1;
	punpckhwd	%mm6,%mm0;
	movq		%mm7,%mm3;
	psraw		$3,%mm1;
	paddw		c2,%mm4;
	punpcklwd	%mm1,%mm7;
	movq		%mm2,%mm6;
	punpckhwd	%mm1,%mm3;
	paddw		c1,%mm5;
	punpckldq	%mm7,%mm2;
	punpckhdq	%mm7,%mm6;
	movq		%mm0,%mm7;
	movq		%mm2,4*16+0(%ebx);
	punpckldq	%mm3,%mm7;
	movq		6*16+8(%ebx),%mm1;
	punpckhdq	%mm3,%mm0;
	movq		%mm6,5*16+0(%ebx);
	psraw		$2,%mm4;
	movq		%mm7,6*16+0(%ebx);
	psraw		$1,%mm5;
	movq		%mm1,%mm3;
	punpckhwd	%mm5,%mm1;
	movq		4*16+8(%ebx),%mm7;
	punpcklwd	%mm5,%mm3;
	movq		%mm7,%mm6;
	punpcklwd	%mm4,%mm7;
	movq		3*16+8(%ebx),%mm5;
	punpckhwd	%mm4,%mm6;
	movq		%mm7,%mm4;
	punpckldq	%mm3,%mm7;
	movq		%mm0,7*16+0(%ebx);
	punpckhdq	%mm3,%mm4;
	movq		%mm6,%mm2;
	punpckhdq	%mm1,%mm6;
	movq		2*16+8(%ebx),%mm3;
	punpckldq	%mm1,%mm2;
	movq		0*16+8(%ebx),%mm0;
	paddw		%mm5,%mm7;
	movq		1*16+8(%ebx),%mm1;
	paddw		%mm5,%mm5;
	psubw		%mm7,%mm5;
	paddw		%mm3,%mm4;
	paddw		%mm0,%mm6;
	paddw		%mm3,%mm3;
	psubw		%mm4,%mm3;
	paddw		%mm0,%mm0;
	paddw		%mm1,%mm2;
	paddw		%mm1,%mm1;
	psubw		%mm6,%mm0;
	psubw		%mm2,%mm1;
	paddw		%mm6,%mm7;
	paddw		%mm6,%mm6;
	paddw		%mm2,%mm4;
	paddw		%mm2,%mm2;
	psubw		%mm7,%mm6;
	psubw		%mm4,%mm2;
	paddw		%mm7,%mm4;
	paddw		%mm7,%mm7;
	psubw		%mm4,%mm7;
	pmulhw		mmx_q_fdct_inter_lut0+8,%mm4;
	pmulhw		mmx_q_fdct_inter_lut0+8,%mm7;
	movq		%mm1,128+16(%ebx);
	movq		%mm3,128+32(%ebx);
	paddw		c2,%mm7;
	paddw		c2,%mm4;
	movq		%mm0,128+24(%ebx);
	movq		%mm5,128+40(%ebx);
	movq		%mm4,%mm1;
	punpcklwd	%mm4,%mm4;
	movq		cfae,%mm3;
	psrad		$18,%mm4;
	pmaddwd		%mm3,%mm4;
	punpckhwd	%mm1,%mm1;
	movq		%mm6,%mm5;
	psrad		$18,%mm1;
	pmaddwd		%mm3,%mm1;
	punpcklwd	%mm2,%mm6;
	movq		%mm7,%mm0;
	punpckhwd	%mm2,%mm5;
	movq		%mm6,%mm2;
	punpcklwd	%mm7,%mm7;
	pmaddwd		mmx_q_fdct_inter_lut+1*32+0+16,%mm6;
	psrad		$18,%mm7;
	pmaddwd		%mm3,%mm7;
	psrad		$15,%mm4;
	pmaddwd		mmx_q_fdct_inter_lut+4*32+0+16,%mm2;
	psrad		$15,%mm1;
	paddd		c1_16,%mm6;
	packssdw	%mm1,%mm4;
	movq		c1_16,%mm1;
	punpckhwd	%mm0,%mm0;
	paddd		%mm1,%mm2;
	psrad		$18,%mm0;
	pmaddwd		%mm3,%mm0;
	psrad		$15,%mm7;
	movq		%mm4,0*16+8(%edi);
	psrad		$17,%mm6;
	pmaddwd		%mm3,%mm6;
	psrad		$17,%mm2;
	pmaddwd		%mm3,%mm2;
	psrad		$15,%mm0;
	packssdw	%mm0,%mm7;
	movq		%mm5,%mm0;
	pmaddwd		mmx_q_fdct_inter_lut+1*32+8+16,%mm5;
	psrad		$15,%mm6;
	pmaddwd		mmx_q_fdct_inter_lut+4*32+8+16,%mm0;
	psrad		$15,%mm2;
	movq		%mm7,4*16+8(%edi);
	por		%mm7,%mm4;
	movq		128+24(%ebx),%mm7;
	paddd		%mm1,%mm5;
	psrad		$17,%mm5;
	pmaddwd		%mm3,%mm5;
	paddd		%mm1,%mm0;
	movq		128+16(%ebx),%mm1;
	psrad		$17,%mm0;
	pmaddwd		%mm3,%mm0;
	movq		128+32(%ebx),%mm3;
	psrad		$15,%mm5;
	packssdw	%mm5,%mm6;
	por		%mm6,%mm4;
	psrad		$15,%mm0;
	movq		%mm6,2*16+8(%edi);
	packssdw	%mm0,%mm2;
	movq		128+40(%ebx),%mm5;
	por		%mm2,%mm4;
	movq		%mm2,6*16+8(%edi);
	paddw		%mm3,%mm5;
	movq		%mm4,128+8(%ebx);
	paddw		%mm1,%mm3;
	paddw		%mm7,%mm1;
	paddw		%mm3,%mm3;
	paddw		c1,%mm3;
	pmulhw		cC4_15,%mm3;
	paddw		%mm7,%mm3;
	paddw		%mm7,%mm7;
	psubw		%mm3,%mm7;
	movq		%mm3,%mm6;
	movq		%mm7,%mm2;
	paddw		%mm5,%mm5;
	paddw		c1,%mm5;
	paddw		%mm1,%mm1;
	paddw		c1,%mm1;
	movq		%mm5,%mm4;
	punpckhwd	%mm1,%mm4;
	movq		%mm4,%mm0;
	punpcklwd	%mm1,%mm5;
	movq		%mm5,%mm1;
	pmaddwd		cC2626_15,%mm5;
	psrad		$16,%mm5;
	pmaddwd		cC6262_15,%mm1;
	psrad		$16,%mm1;
	pmaddwd		cC2626_15,%mm4;
	psrad		$16,%mm4;
	pmaddwd		cC6262_15,%mm0;
	psrad		$16,%mm0;
	packssdw	%mm4,%mm5;
	packssdw	%mm0,%mm1;
	punpcklwd	%mm1,%mm6;
	movq		%mm6,%mm0;
	punpcklwd	%mm5,%mm2;
	movq		%mm2,%mm4;
	pmaddwd		mmx_q_fdct_inter_lut+0*32+0+16,%mm6;
	paddd		c1_17,%mm6;
	psrad		$18,%mm6;
	pmaddwd		cfae,%mm6;
	psrad		$15,%mm6;
	pmaddwd		mmx_q_fdct_inter_lut+2*32+0+16,%mm2;
	paddd		c1_17,%mm2;
	psrad		$18,%mm2;
	pmaddwd		cfae,%mm2;
	psrad		$15,%mm2;
	pmaddwd		mmx_q_fdct_inter_lut+3*32+0+16,%mm4;
	paddd		c1_17,%mm4;
	psrad		$18,%mm4;
	pmaddwd		cfae,%mm4;
	psrad		$15,%mm4;
	pmaddwd		mmx_q_fdct_inter_lut+5*32+0+16,%mm0;
	paddd		c1_15,%mm0;
	psrad		$16,%mm0;
	pmaddwd		cfae,%mm0;
	psrad		$15,%mm0;
	punpckhwd	%mm1,%mm3;
	movq		%mm3,%mm1;
	punpckhwd	%mm5,%mm7;
	movq		%mm7,%mm5;
	pmaddwd		mmx_q_fdct_inter_lut+0*32+8+16,%mm3;
	paddd		c1_17,%mm3;
	psrad		$18,%mm3;
	pmaddwd		cfae,%mm3;
	psrad		$15,%mm3;
	pmaddwd		mmx_q_fdct_inter_lut+2*32+8+16,%mm7;
	paddd		c1_17,%mm7;
	psrad		$18,%mm7;
	pmaddwd		cfae,%mm7;
	psrad		$15,%mm7;
	pmaddwd		mmx_q_fdct_inter_lut+3*32+8+16,%mm5;
	paddd		c1_17,%mm5;
	psrad		$18,%mm5;
	pmaddwd		cfae,%mm5;
	psrad		$15,%mm5;
	pmaddwd		mmx_q_fdct_inter_lut+5*32+8+16,%mm1;	
	paddd		c1_15,%mm1;
	psrad		$16,%mm1;
	pmaddwd		cfae,%mm1;
	psrad		$15,%mm1;
	packssdw	%mm3,%mm6;
	packssdw	%mm7,%mm2;
	packssdw	%mm5,%mm4;
	packssdw	%mm1,%mm0;
	movq		%mm6,1*16+8(%edi);
	por		%mm2,%mm6;
	movq		%mm2,3*16+8(%edi);
	movq		%mm4,5*16+8(%edi);
	por		%mm0,%mm4;
	movq		%mm0,7*16+8(%edi);
	por		128+8(%ebx),%mm6;
	por		%mm4,%mm6;
	movq		%mm6,128+8(%ebx);
	movq		(%ebx),%mm7;
	movq		7*16+0(%ebx),%mm6;
	paddw		%mm7,%mm6;
	paddw		%mm7,%mm7;
	psubw		%mm6,%mm7;
	movq		1*16+0(%ebx),%mm1;
	movq		6*16+0(%ebx),%mm2;
	paddw		%mm1,%mm2;
	paddw		%mm1,%mm1;
	psubw		%mm2,%mm1;
	movq		2*16+0(%ebx),%mm3;
	movq		5*16+0(%ebx),%mm4;
	paddw		%mm3,%mm4;
	paddw		%mm3,%mm3;
	psubw		%mm4,%mm3;
	movq		3*16+0(%ebx),%mm5;
	movq		4*16+0(%ebx),%mm0;
	paddw		%mm5,%mm0;
	paddw		%mm5,%mm5;
	psubw		%mm0,%mm5;
	paddw		%mm6,%mm0;
	paddw		%mm6,%mm6;
	psubw		%mm0,%mm6;
	paddw		%mm2,%mm4;
	paddw		%mm2,%mm2;
	psubw		%mm4,%mm2;
	paddw		%mm0,%mm4;
	paddw		%mm0,%mm0;
	psubw		%mm4,%mm0;
	pmulhw		mmx_q_fdct_inter_lut0,%mm4;
	pmulhw		mmx_q_fdct_inter_lut0,%mm0;
	movq		%mm1,128+16(%ebx);
	paddw		c2,%mm4;
	paddw		c2,%mm0;
	movq		%mm4,%mm1;
	movq		%mm5,128+32(%ebx);
	punpckhwd	%mm1,%mm1;
	movq		%mm0,%mm5;
	punpcklwd	%mm4,%mm4;
	movq		%mm7,128+24(%ebx);
	punpckhwd	%mm5,%mm5;
	punpcklwd	%mm0,%mm0;
	psrad		$18,%mm1;
	movq		cfae,%mm7;
	psrad		$18,%mm4;
	pmaddwd		%mm7,%mm1;
	psrad		$18,%mm5;
	pmaddwd		%mm7,%mm4;
	psrad		$18,%mm0;
	pmaddwd		%mm7,%mm5;
	psrad		$15,%mm1;
	pmaddwd		%mm7,%mm0;
	psrad		$15,%mm4;
	psrad		$15,%mm5;
	psrad		$15,%mm0;
	packssdw	%mm1,%mm4;
	packssdw	%mm5,%mm0;
	movq		%mm4,0*16+0(%edi);
	movq		%mm6,%mm5;
	movq		c1_16,%mm1;
	punpcklwd	%mm2,%mm6;
	movq		%mm0,4*16+0(%edi);
	por		%mm4,%mm0;
	movq		%mm6,%mm4;
	pmaddwd		mmx_q_fdct_inter_lut+1*32+0,%mm6;
	punpckhwd	%mm2,%mm5;
	pmaddwd		mmx_q_fdct_inter_lut+4*32+0,%mm4;
	movq		%mm5,%mm2;
	pmaddwd		mmx_q_fdct_inter_lut+1*32+8,%mm5;
	paddd		%mm1,%mm6;
	pmaddwd		mmx_q_fdct_inter_lut+4*32+8,%mm2;
	psrad		$17,%mm6;
	paddd		%mm1,%mm4;
	psrad		$17,%mm4;
	paddd		%mm1,%mm5;
	por		128+8(%ebx),%mm0;
	paddd		%mm1,%mm2;
	psrad		$17,%mm5;
	pmaddwd		%mm7,%mm6;
	psrad		$17,%mm2;
	pmaddwd		%mm7,%mm5;
	movq		128+32(%ebx),%mm1;
	pmaddwd		%mm7,%mm4;
	pmaddwd		%mm7,%mm2;
	psrad		$15,%mm6;
	movq		128+16(%ebx),%mm7;
	psrad		$15,%mm5;
	paddw		%mm3,%mm1;
	packssdw	%mm5,%mm6;
	movq		128+24(%ebx),%mm5;
	psrad		$15,%mm4;
	movq		%mm6,2*16+0(%edi);
	psrad		$15,%mm2;
	paddw		%mm7,%mm3;
	packssdw	%mm2,%mm4;
	movq		%mm4,6*16+0(%edi);
	paddw		%mm3,%mm3;
	paddw		c1,%mm3;
	por		%mm4,%mm6;
	paddw		%mm5,%mm7;
	por		%mm0,%mm6;
	pmulhw		cC4_15,%mm3;
	paddw		%mm1,%mm1;
	paddw		c1,%mm1;
	paddw		%mm7,%mm7;
	paddw		c1,%mm7;
	movq		%mm1,%mm2;
	paddw		%mm5,%mm3;
	paddw		%mm5,%mm5;
	psubw		%mm3,%mm5;
	punpckhwd	%mm7,%mm2;
	movq		%mm0,128+8(%ebx);
	punpcklwd	%mm7,%mm1;
	movq		%mm3,%mm6;
	movq		%mm1,%mm7;
	pmaddwd		cC2626_15,%mm1;
	movq		%mm5,%mm4;
	pmaddwd		cC6262_15,%mm7;
	movq		%mm2,%mm0;
	pmaddwd		cC2626_15,%mm2;
	pmaddwd		cC6262_15,%mm0;
	psrad		$16,%mm1;
	movq		%mm5,128+32(%ebx);
	psrad		$16,%mm7;
	movq		c1_17,%mm5;
	psrad		$16,%mm2;
	psrad		$16,%mm0;
	packssdw	%mm2,%mm1;
	packssdw	%mm0,%mm7;
	movq		%mm1,128+40(%ebx);
	punpcklwd	%mm7,%mm6;
	movq		%mm6,%mm0;
	pmaddwd		mmx_q_fdct_inter_lut+0*32+0,%mm6;
	punpckhwd	%mm7,%mm3;
	pmaddwd		mmx_q_fdct_inter_lut+5*32+0,%mm0;
	movq		%mm3,%mm7;
	pmaddwd		mmx_q_fdct_inter_lut+0*32+8,%mm3;
	punpcklwd	%mm1,%mm4;
	pmaddwd		mmx_q_fdct_inter_lut+5*32+8,%mm7;
	movq		%mm4,%mm2;
	pmaddwd		mmx_q_fdct_inter_lut+2*32+0,%mm4;
	pmaddwd		mmx_q_fdct_inter_lut+3*32+0,%mm2;
	paddd		%mm5,%mm6;
	paddd		%mm5,%mm3;
	paddd		c1_15,%mm7;
	paddd		c1_15,%mm0;
	paddd		%mm5,%mm4;
	paddd		%mm5,%mm2;
	movq		cfae,%mm1;
	psrad		$18,%mm6;
	psrad		$18,%mm3;
	psrad		$16,%mm7;
	psrad		$16,%mm0;
	psrad		$18,%mm4;
	psrad		$18,%mm2;
	pmaddwd		%mm1,%mm6;
	pmaddwd		%mm1,%mm3;
	pmaddwd		%mm1,%mm7;
	pmaddwd		%mm1,%mm0;
	pmaddwd		%mm1,%mm4;
	pmaddwd		%mm1,%mm2;
	psrad		$15,%mm6;
	psrad		$15,%mm3;
	psrad		$15,%mm0;
	psrad		$15,%mm7;
	psrad		$15,%mm4;
	psrad		$15,%mm2;
	packssdw	%mm3,%mm6;
	packssdw	%mm7,%mm0;
	movq		%mm6,1*16+0(%edi);
	por		%mm0,%mm6;
	movq		%mm0,7*16+0(%edi);
	movq		128+32(%ebx),%mm3;
	punpckhwd	128+40(%ebx),%mm3;
	movq		%mm3,%mm7;
	pmaddwd		mmx_q_fdct_inter_lut+2*32+8,%mm3;
	pmaddwd		mmx_q_fdct_inter_lut+3*32+8,%mm7;
	paddd		%mm5,%mm3;
	paddd		%mm5,%mm7;
	psrad		$18,%mm3;
	psrad		$18,%mm7;
	pmaddwd		%mm1,%mm3;
	pmaddwd		%mm1,%mm7;
	psrad		$15,%mm3;
	psrad		$15,%mm7;
	packssdw	%mm3,%mm4;
	packssdw	%mm7,%mm2;
	movq		%mm4,3*16+0(%edi);
	por		%mm2,%mm4;
	movq		%mm2,5*16+0(%edi);
	por		%mm4,%mm6;
	por		128+8(%ebx),%mm6;
	movq		%mm6,%mm4;
	psrlq		$32,%mm6;
	por		%mm4,%mm6;
	movd		%mm6,%edx;
	cmpl		$1,%edx;
	rcll		%eax;
	decl		%ecx;
	jne		1b;

	popl		%edi;
	popl		%esi;
	xorb		$63,%al;
	popl		%ebx;
	ret

# not scheduled, will be 20% faster
# autosched still unfinished, sigh
	.text
	.align		16
	.globl		mmx_mpeg1_idct_inter

mmx_mpeg1_idct_inter:

	pushl %edi
	pushl %esi
	pushl %ebx

	movd		%eax,%mm1;
	punpcklwd	%mm1,%mm1;
	punpcklwd	%mm1,%mm1;
	movq		c1,%mm3;
	movq		%mm1,%mm0;
	movq		%mm1,%mm2;
	pand		%mm3,%mm0;
	paddw		%mm1,%mm1;
	psubw		%mm3,%mm2;
	movq		%mm1,mblock+768+14*8;	// quant2
	paddw		%mm0,%mm2;
	movq		%mm2,mblock+768+15*8;	// qodd

	xorl		%eax,%eax;
	movl		newref,%edi;
	sall		$26,%edx;		// cbp: 1 << (5 - i)
	movl		$mblock,%esi;

	.align		16
1:
	addl		mb_address(%eax),%edi
	sall		%edx
	movl		mb_address+4(%eax),%ecx
	leal		8(%eax),%eax
	jnc		2f

	movl		$mblock+768,%ebx

	movq 		0*16+0(%esi),%mm6;
	pxor		%mm0,%mm0;
	movq		mblock+768+14*8,%mm1;	// quant2	
	movq		%mm6,%mm4;		
	movq		mblock+768+15*8,%mm2;	// qodd
	movq		%mm4,%mm3;
	pmullw		%mm1,%mm4;
	pcmpeqw		%mm0,%mm6;
	movq		2*16+0(%esi),%mm5;	
	pandn		%mm2,%mm6;
	psraw		$15,%mm3;		
	pxor		%mm3,%mm6;		
	psubw		%mm3,%mm4;
	movq		4*16+0(%esi),%mm3;	
	paddw		%mm6,%mm4;		
	movq		%mm5,%mm7;
	psllw		$3,%mm4;		
	pmullw		%mm1,%mm5;
	paddsw		%mm4,%mm4;		
	movq		%mm7,%mm6;	
	psraw		$1,%mm4;		
	pcmpeqw		%mm0,%mm7;
	psraw		$15,%mm6;		
	pandn		%mm2,%mm7;	
	movq		%mm4,0*16+0(%esi);	
	pxor		%mm6,%mm7;	
	psubw		%mm6,%mm5;		
	movq		%mm3,%mm4;
	movq		6*16+0(%esi),%mm6;	
	paddw		%mm7,%mm5;		
	pmullw		%mm1,%mm4;		
	psllw		$3,%mm5;		
	movq		%mm3,%mm7;	
	paddsw		%mm5,%mm5;		
	pcmpeqw		%mm0,%mm3;
	pandn		%mm2,%mm3;		
	psraw		$15,%mm7;
	psraw		$4,%mm5;		
	pxor		%mm7,%mm3;
	psubw		%mm7,%mm4;		
	movq		%mm6,%mm7;		
	movq		%mm5,2*16+0(%esi);	
	pmullw		%mm1,%mm6;
	movq		3*16+0(%esi),%mm5;	
	paddw		%mm3,%mm4;		
	movq		%mm7,%mm3;
	pcmpeqw		%mm0,%mm7;		
	psraw		$15,%mm3;
	movq		%mm4,4*16+0(%esi);	
	pandn		%mm2,%mm7;	
	pxor		%mm3,%mm7;		
	psubw		%mm3,%mm6;
	paddw		%mm7,%mm6;		
	movq		%mm5,%mm4;
	pmullw		%mm1,%mm5;		
	movq		%mm4,%mm3;
	movq 		%mm6,6*16+0(%esi);	
	pcmpeqw		%mm0,%mm4;
	movq		7*16+0(%esi),%mm6;	
	psraw		$15,%mm3;		
	pandn		%mm2,%mm4;
	psubw		%mm3,%mm5;		
	pxor		%mm3,%mm4;
	paddw		%mm4,%mm5;		
	movq		%mm6,%mm7;	
	movq		1*16+0(%esi),%mm4;	
	pmullw		%mm1,%mm6;		
	movq		%mm7,%mm3;
	pcmpeqw		%mm0,%mm7;		
	psraw		$15,%mm3;
	pandn		%mm2,%mm7;		
	psubw		%mm3,%mm6;
	pxor		%mm3,%mm7;		
	movq		%mm4,%mm3;		
	paddw		%mm7,%mm6;		
	pmullw		%mm1,%mm4;
	movq		%mm3,%mm7;		
	psraw		$15,%mm3;
	pcmpeqw		%mm0,%mm7;
	pandn		%mm2,%mm7;		
	pxor		%mm3,%mm7;
	psubw		%mm3,%mm7;		
	paddw		%mm7,%mm4;
	movq		5*16+0(%esi),%mm7;	
	psllw		$3,%mm4;		
	paddsw		%mm4,%mm4;        	
	movq		%mm7,%mm2;	
	psraw		$4,%mm4;		
	pmullw		%mm1,%mm7;
	movq		mmx_q_idct_inter_tab+10*8,%mm1;		
	pcmpeqw		%mm2,%mm0;		
	psraw		$15,%mm2;	
	pandn		mblock+768+15*8,%mm0;	// qodd		
	movq		%mm4,%mm3;

	pxor		%mm2,%mm0;		
	punpcklwd	%mm6,%mm4;
	psubw		%mm2,%mm0;		
	punpckhwd 	%mm6,%mm3;
	movq		%mm4,%mm6;		
	paddw		%mm0,%mm7;		
	pmaddwd 	mmx_q_idct_inter_tab+0*8,%mm4;		
	movq		%mm3,%mm2;
	pmaddwd 	mmx_q_idct_inter_tab+0*8,%mm3;		
	movq		%mm5,%mm0;
	pmaddwd 	mmx_q_idct_inter_tab+1*8,%mm6;		
	punpcklwd 	%mm7,%mm5;
	pmaddwd 	mmx_q_idct_inter_tab+1*8,%mm2;		
	punpckhwd 	%mm7,%mm0;
	paddd		%mm1,%mm4;		
	movq		%mm5,%mm7;
	paddd		%mm1,%mm3;		
	psrad		$11,%mm4;
	pmaddwd 	mmx_q_idct_inter_tab+2*8,%mm5;		
	psrad		$11,%mm3;
	pmaddwd 	mmx_q_idct_inter_tab+3*8,%mm7;		
	packssdw 	%mm3,%mm4;
	paddd		%mm1,%mm6;		
	movq		%mm0,%mm3;
	paddd		%mm1,%mm2;		
	psrad		$11,%mm6;
	pmaddwd 	mmx_q_idct_inter_tab+2*8,%mm0;		
	psrad		$11,%mm2;
	pmaddwd 	mmx_q_idct_inter_tab+3*8,%mm3;		
	paddd		%mm1,%mm5;
	paddd		%mm1,%mm7;		
	packssdw	%mm2,%mm6;
	movq		2*16+0(%esi),%mm2;	
	psrad		$11,%mm5;
	paddd		%mm1,%mm0;		
	psrad		$11,%mm0;		
	paddd		%mm1,%mm3;		
	packssdw	%mm0,%mm5;		
	movq		%mm2,%mm0;
	movq		4*16+0(%esi),%mm1;	
	psubw		%mm5,%mm4;
	psrad		$11,%mm7;
	paddw		%mm5,%mm5;		
	psrad		$11,%mm3;
	paddw		%mm4,%mm5;		
	packssdw	%mm3,%mm7;
	movq		6*16+0(%esi),%mm3;	
	psubw		%mm7,%mm6;
	paddw		%mm7,%mm7;

	psubw		%mm3,%mm2;		
	psllw		$4,%mm3;
	pmulhw		mmx_q_idct_inter_tab+13*8,%mm3;		
	psllw		$3,%mm2;
	paddw		c2,%mm2;		
	psubw		%mm6,%mm4;
	pmulhw		mmx_q_idct_inter_tab+12*8,%mm2;		
	psllw		$4,%mm0;
	paddw		%mm6,%mm7;		
	psllw		$3,%mm1;
	pmulhw		mmx_q_idct_inter_tab+15*8,%mm0;		
	psllw		$2,%mm6;
	paddw		%mm3,%mm3;		
	psllw		$2,%mm2;
	paddw		%mm2,%mm3;		
	paddw		%mm0,%mm2;		
	movq		0*16+0(%esi),%mm0;	
	paddw		%mm4,%mm4;		
	psubw		%mm1,%mm0;		
	paddw		%mm1,%mm1;
	paddw		%mm4,%mm6;	 	
	paddw		%mm0,%mm1;
	psubw		%mm2,%mm0;		
	paddw		%mm2,%mm2;
	pmulhw		cC4_15,%mm4;		
	paddw		%mm0,%mm2;
	psubw		%mm3,%mm1;		
	paddw		%mm3,%mm3;
	pmulhw		cC4_15,%mm6;		
	paddw		%mm1,%mm3;

	psubw		%mm7,%mm1;		
	paddw		%mm7,%mm7;
	psubw		%mm5,%mm3;		
	paddw		%mm5,%mm5;
	paddw		%mm1,%mm7;		
	paddw		%mm3,%mm5;
	movq		%mm1,mm8;		
	psubw		%mm4,%mm0;		
	psubw		%mm6,%mm2;		
	paddw		%mm6,%mm6;
	movq		%mm5,%mm1;		
	paddw		%mm2,%mm6;
	paddw		%mm4,%mm4;		
	punpcklwd	%mm6,%mm5;
	paddw		%mm0,%mm4;		
	punpckhwd	%mm6,%mm1;
	movq		%mm4,%mm6;		
	punpcklwd	%mm7,%mm4;
	punpckhwd	%mm7,%mm6;
	movq		%mm5,%mm7;		
	punpckldq	%mm4,%mm5;
	movq		%mm5,(%ebx);	
	punpckhdq	%mm4,%mm7;
	movq		%mm1,%mm4;		
	punpckldq	%mm6,%mm1;
	movq		%mm7,1*8(%ebx);	
	punpckhdq	%mm6,%mm4;
	movq		%mm2,%mm7;		
	punpcklwd	%mm3,%mm2;
	movq 		%mm1,2*8(%ebx);	
	punpckhwd	%mm3,%mm7;
	movq		mm8,%mm1;		
	movq		%mm4,3*8(%ebx);
	movq		%mm1,%mm5;		
	punpcklwd	%mm0,%mm1;
	movq		0*16+8(%esi),%mm6;	
	punpckhwd	%mm0,%mm5;
	movq		%mm1,%mm0;		
	punpckldq	%mm2,%mm1;
	movq		%mm5,%mm3;		
	punpckhdq	%mm2,%mm0;
	movq		%mm1,4*8(%ebx);	
	punpckldq	%mm7,%mm5;
	movq		mblock+768+14*8,%mm1;	// quant2		
	punpckhdq	%mm7,%mm3;
	movq		%mm0,5*8(%ebx);	
	movq		%mm6,%mm4;
	movq		mblock+768+15*8,%mm2;	// qodd		
	pxor		%mm0,%mm0;

	pmullw		%mm1,%mm6;
	movq		%mm3,6*8(%ebx);	
	movq		%mm4,%mm3;
	movq		2*16+8(%esi),%mm7;	
	pcmpeqw		%mm0,%mm4;
	psraw		$15,%mm3;
	movq		%mm5,7*8(%ebx);	
	pandn		%mm2,%mm4;
	pxor		%mm3,%mm4;		
	psubw		%mm3,%mm6;
	paddw		%mm4,%mm6;		
	movq		%mm7,%mm5;
	movq		%mm6,0*16+8(%esi);	
	pmullw		%mm1,%mm7;
	movq		4*16+8(%esi),%mm6;	
	movq		%mm5,%mm3;		
	pcmpeqw		%mm0,%mm5;
	psraw		$15,%mm3;		
	pandn		%mm2,%mm5;
	pxor		%mm3,%mm5;		
	psubw		%mm3,%mm7;
	paddw		%mm5,%mm7;		
	movq		%mm6,%mm4;
	movq		%mm7,2*16+8(%esi);	
	pmullw		%mm1,%mm6;
	movq		6*16+8(%esi),%mm5;	
	movq		%mm4,%mm3;		
	pcmpeqw		%mm0,%mm4;
	psraw		$15,%mm3;		
	pandn		%mm2,%mm4;
	pxor		%mm3,%mm4;		
	psubw		%mm3,%mm6;
	paddw		%mm4,%mm6;		
	movq		%mm5,%mm7;
	movq		%mm6,4*16+8(%esi);	
	pmullw		%mm1,%mm5;
	movq		3*16+8(%esi),%mm6;	
	movq		%mm7,%mm3;		
	pcmpeqw		%mm0,%mm7;
	psraw		$15,%mm3;		
	pandn		%mm2,%mm7;
	pxor		%mm3,%mm7;		
	psubw		%mm3,%mm5;
	paddw		%mm7,%mm5;		
	movq		%mm6,%mm4;
	movq		%mm5,6*16+8(%esi);	
	pmullw		%mm1,%mm6;
	movq 		7*16+8(%esi),%mm5;	
	movq		%mm4,%mm3;		
	pcmpeqw		%mm0,%mm4;
	psraw		$15,%mm3;		
	pandn		%mm2,%mm4;
	pxor		%mm3,%mm4;		
	psubw		%mm3,%mm6;
	movq		%mm5,%mm7;		
	pmullw		%mm1,%mm5;
	paddw		%mm4,%mm6;		
	movq		%mm7,%mm3;
	movq		1*16+8(%esi),%mm4;	
	psraw		$15,%mm3;		
	pcmpeqw		%mm0,%mm7;		
	pandn		%mm2,%mm7;		
	psubw		%mm3,%mm5;
	pxor		%mm3,%mm7;		
	movq		%mm4,%mm3;
	paddw		%mm7,%mm5;		
	pmullw		%mm1,%mm4;
	movq		%mm3,%mm7;		
	psraw		$15,%mm3;
	pcmpeqw		%mm0,%mm7;		
	psubw		%mm3,%mm4;
	pandn		%mm2,%mm7;		
	pxor		%mm3,%mm7;
	paddw		%mm7,%mm4;		
	movq		%mm4,%mm2;
	movq		5*16+8(%esi),%mm7;	
	punpcklwd 	%mm5,%mm4;
	movq		%mm7,%mm3;		
	punpckhwd	%mm5,%mm2;
	pmullw		%mm1,%mm7;		
	movq		%mm4,%mm5;		
	pmaddwd 	mmx_q_idct_inter_tab+0*8,%mm4;		
	pcmpeqw		%mm3,%mm0;
	movq		mmx_q_idct_inter_tab+10*8,%mm1;		
	psraw		$15,%mm3;		
	psubw		%mm3,%mm7;
	pandn		mblock+768+15*8,%mm0;	// qodd		
	pxor		%mm3,%mm0;
	pmaddwd 	mmx_q_idct_inter_tab+1*8,%mm5;		
	movq		%mm2,%mm3;
	pmaddwd 	mmx_q_idct_inter_tab+1*8,%mm2;		
	paddw		%mm0,%mm7;
	pmaddwd 	mmx_q_idct_inter_tab+0*8,%mm3;		
	movq		%mm6,%mm0;
	paddd		%mm1,%mm4;		
	punpcklwd	%mm7,%mm6;
	paddd		%mm1,%mm5;		
	punpckhwd	%mm7,%mm0;
	movq		%mm6,%mm7;		
	psrad		$11,%mm4;
	paddd		%mm1,%mm2;		
	psrad		$11,%mm5;
	paddd		%mm1,%mm3;		
	psrad		$11,%mm2;
	pmaddwd		mmx_q_idct_inter_tab+2*8,%mm6;		
	packssdw	%mm2,%mm5;
	pmaddwd		mmx_q_idct_inter_tab+3*8,%mm7;		
	movq		%mm0,%mm2;
	pmaddwd		mmx_q_idct_inter_tab+2*8,%mm0;		
	psrad		$11,%mm3;
	paddd		%mm1,%mm6;		
	packssdw	%mm3,%mm4;
	pmaddwd		mmx_q_idct_inter_tab+3*8,%mm2;		
	psrad		$11,%mm6;
	paddd		%mm1,%mm7;		
	psrad		$11,%mm7;
	movq		6*16+8(%esi),%mm3;		
	paddd		%mm1,%mm0;		
	psrad		$11,%mm0;
	paddd		%mm1,%mm2;		
	packssdw	%mm0,%mm6;
	psubw		%mm6,%mm4;		
	psrad		$11,%mm2;
	paddw		%mm6,%mm6;		
	packssdw	%mm2,%mm7;

	movq		2*16+8(%esi),%mm2;		
	psubw		%mm7,%mm5;		
	paddw		%mm7,%mm7;
	paddw		%mm4,%mm6;		
	paddw		%mm5,%mm7;
	movq		%mm2,%mm0;		
	psubw		%mm3,%mm2;
	psllw		$3,%mm2;			
	psubw		%mm5,%mm4;
	paddw		c2,%mm2;			
	paddw		%mm4,%mm4;
	pmulhw		mmx_q_idct_inter_tab+12*8,%mm2;		
	psllw		$2,%mm5;
	paddw		%mm4,%mm5;		
	psllw		$4,%mm3;
	pmulhw		mmx_q_idct_inter_tab+13*8,%mm3;		
	psllw		$4,%mm0;
	pmulhw		cC4_15,%mm4;		
	psllw		$2,%mm2;
	pmulhw		mmx_q_idct_inter_tab+15*8,%mm0;		
	paddw		%mm3,%mm3;
	pmulhw		cC4_15,%mm5;		
	paddw		%mm2,%mm3;
	movq		4*16+8(%esi),%mm1;		
	paddw		%mm0,%mm2;		
	psllw		$3,%mm1;
	movq		0*16+8(%esi),%mm0;		
	psllw		$3,%mm0;			
	psubw		%mm1,%mm0;
	paddw		%mm1,%mm1;		
	paddw		%mm0,%mm1;
	psubw		%mm2,%mm0;		
	paddw		%mm2,%mm2;
	psubw		%mm3,%mm1;		
	paddw		%mm3,%mm3;
	paddw		%mm0,%mm2;		
	paddw		%mm1,%mm3;

	psubw		%mm4,%mm0;		
	paddw		%mm4,%mm4;
	psubw		%mm7,%mm1;		
	paddw		%mm7,%mm7;
	movq 		%mm1,mm8;		
	paddw		%mm1,%mm7;
	psubw		%mm5,%mm2;		
	paddw		%mm5,%mm5;
	psubw		%mm6,%mm3;		
	paddw		%mm6,%mm6;
	paddw		%mm2,%mm5;		
	paddw		%mm3,%mm6;
	movq		%mm6,%mm1;		
	punpcklwd	%mm5,%mm6;
	paddw		%mm0,%mm4;		
	punpckhwd	%mm5,%mm1;
	movq		%mm4,%mm5;		
	punpcklwd	%mm7,%mm4;
	punpckhwd	%mm7,%mm5;
	movq		%mm6,%mm7;		
	punpckldq	%mm4,%mm6;
	movq		%mm6,8*8(%ebx);	
	punpckhdq	%mm4,%mm7;
	movq		%mm1,%mm4;		
	punpckldq	%mm5,%mm1;
	movq		%mm7,9*8(%ebx);	
	punpckhdq	%mm5,%mm4;
	movq		%mm2,%mm7;		
	punpcklwd	%mm3,%mm2;
	movq		%mm1,10*8(%ebx);	
	punpckhwd	%mm3,%mm7;
	movq		mm8,%mm1;		
	movq		%mm4,11*8(%ebx);
	movq		%mm1,%mm6;		
	punpcklwd	%mm0,%mm1;
	movq		c2,%mm4;		
	punpckhwd	%mm0,%mm6;
	movq		%mm1,%mm0;		
	punpckldq	%mm2,%mm1;
	movq		%mm1,12*8(%ebx);	
	punpckhdq	%mm2,%mm0;
	movq		%mm6,%mm2;		
	punpckldq	%mm7,%mm6;
	movq		%mm6,13*8(%ebx);	
	punpckhdq	%mm7,%mm2;

	movq		5*8(%ebx),%mm5;	
	movq		%mm2,%mm7;
	movq 		6*8(%ebx),%mm6;	
	paddw		%mm4,%mm2;
	paddw		%mm5,%mm2;
	movq		c1,%mm1;		
	paddw		%mm1,%mm5;
	pmulhw		mmx_q_idct_inter_tab+4*8,%mm2;		
	psubw		%mm1,%mm7;
	pmulhw		mmx_q_idct_inter_tab+5*8,%mm5;		
	movq		%mm6,%mm3;
	pmulhw		mmx_q_idct_inter_tab+6*8,%mm7;		
	paddw		%mm4,%mm6;
	paddw		%mm0,%mm6;		
	paddw		%mm1,%mm0;
	pmulhw		mmx_q_idct_inter_tab+7*8,%mm6;		
	psubw		%mm2,%mm5;
	paddw		%mm7,%mm2;		
	paddw		%mm4,%mm3;
	movq		7*8(%ebx),%mm7;
	pmulhw		mmx_q_idct_inter_tab+8*8,%mm0;		
	paddw		%mm7,%mm4;
	pmulhw		mmx_q_idct_inter_tab+9*8,%mm3;
	psubw		%mm6,%mm0;		
	paddw		%mm3,%mm6;

	movq		13*8(%ebx),%mm3;		
	psubw		%mm6,%mm2;
	paddw		%mm6,%mm6;
	psubw		%mm0,%mm5;		
	paddw		%mm0,%mm0;
	psubw		%mm3,%mm4;		
	paddw		%mm5,%mm0;
	pmulhw		mmx_q_idct_inter_tab+12*8,%mm4;		
	paddw		%mm2,%mm6;
	pmulhw		mmx_q_idct_inter_tab+13*8,%mm3;		
	psubw		%mm1,%mm7;
	pmulhw		mmx_q_idct_inter_tab+14*8,%mm7;		
	psubw		%mm5,%mm2;
	movq		12*8(%ebx),%mm1;		
	psllw		$2,%mm5;
	paddw		%mm2,%mm2;
	paddw		%mm4,%mm3;		
	paddw		%mm2,%mm5;		
	pmulhw		cC4_15,%mm2;		
	paddw		%mm7,%mm4;
	movq		4*8(%ebx),%mm7;
	paddw		mmx_q_idct_inter_tab+11*8,%mm7;
	pmulhw		cC4_15,%mm5;
	psubw		%mm1,%mm7;		
	paddw		%mm1,%mm1;
	paddw		%mm7,%mm1;		
	psraw		$2,%mm7;
	psraw		$2,%mm1;
	psubw		%mm4,%mm7;
	paddw		%mm4,%mm4;
	psubw		%mm3,%mm1;
	paddw		%mm3,%mm3;
	paddw		%mm7,%mm4;
	paddw		%mm1,%mm3;

	psubw		%mm2,%mm7;
	paddw		%mm2,%mm2;
	psubw		%mm0,%mm1;
	paddw		%mm0,%mm0;
	paddw		%mm7,%mm2;
	paddw		%mm1,%mm0;
	psubw		%mm5,%mm4;
	paddw		%mm5,%mm5;
	psubw		%mm6,%mm3;
	paddw		%mm6,%mm6;
	paddw		%mm4,%mm5;
	paddw		%mm3,%mm6;
	psraw		$4,%mm6;
	paddsw		0*16+8+3*768(%esi),%mm6;
	movq		%mm6,0*16+8(%esi);
	psraw		$4,%mm5;
	paddsw		1*16+8+3*768(%esi),%mm5;
	movq		%mm5,1*16+8(%esi);
	psraw		$4,%mm2;
	paddsw		2*16+8+3*768(%esi),%mm2;
	movq		%mm2,2*16+8(%esi);
	psraw		$4,%mm0;
	paddsw		3*16+8+3*768(%esi),%mm0;
	movq		%mm0,3*16+8(%esi);
	psraw		$4,%mm1;
	paddsw		4*16+8+3*768(%esi),%mm1;
	movq		%mm1,4*16+8(%esi);
	psraw		$4,%mm7;
	paddsw		5*16+8+3*768(%esi),%mm7;
	movq		%mm7,5*16+8(%esi);
	psraw		$4,%mm4;
	paddsw		6*16+8+3*768(%esi),%mm4;
	movq		%mm4,6*16+8(%esi);
	psraw		$4,%mm3;
	paddsw		7*16+8+3*768(%esi),%mm3;
	movq		%mm3,7*16+8(%esi);
		
	movq		c2,%mm4;
	movq		11*8(%ebx),%mm2;
	movq		1*8(%ebx),%mm5;		
	movq		%mm2,%mm7;
	movq		c1,%mm1;
	paddw		%mm4,%mm2;
	paddw		%mm5,%mm2;
	pmulhw		mmx_q_idct_inter_tab+4*8,%mm2;
	paddw		%mm1,%mm5;
	psubw		%mm1,%mm7;
	movq		3*8(%ebx),%mm6;
	movq		%mm6,%mm3;
	pmulhw		mmx_q_idct_inter_tab+5*8,%mm5;
	paddw		%mm4,%mm6;
	pmulhw		mmx_q_idct_inter_tab+6*8,%mm7;
	movq		9*8(%ebx),%mm0;
	paddw		%mm0,%mm6;
	pmulhw		mmx_q_idct_inter_tab+7*8,%mm6;
	paddw		%mm1,%mm0;
	paddw		%mm4,%mm3;
	pmulhw		mmx_q_idct_inter_tab+8*8,%mm0;
	pmulhw		mmx_q_idct_inter_tab+9*8,%mm3;
	psubw		%mm2,%mm5;
	paddw		%mm7,%mm2;
	movq		2*8(%ebx),%mm7;
	psubw		%mm6,%mm0;
	paddw		%mm3,%mm6;
	movq		10*8(%ebx),%mm3;

	paddw		%mm7,%mm4;
	psubw		%mm1,%mm7;
	pmulhw		mmx_q_idct_inter_tab+14*8,%mm7;
	psubw		%mm3,%mm4;
	pmulhw		mmx_q_idct_inter_tab+13*8,%mm3;
	pmulhw		mmx_q_idct_inter_tab+12*8,%mm4;
	movq		8*8(%ebx),%mm1;
	psubw		%mm6,%mm2;
	paddw		%mm6,%mm6;
	paddw		%mm2,%mm6;
	psubw		%mm0,%mm5;
	paddw		%mm0,%mm0;
	paddw		%mm5,%mm0;
	psubw		%mm5,%mm2;
	paddw		%mm2,%mm2;
	psllw		$2,%mm5;
	paddw		%mm2,%mm5;
	pmulhw		cC4_15,%mm2;
	pmulhw		cC4_15,%mm5;
	paddw		%mm4,%mm3;
	paddw		%mm7,%mm4;
	movq		(%ebx),%mm7;
	paddw		mmx_q_idct_inter_tab+11*8,%mm7;
	psubw		%mm1,%mm7;
	paddw		%mm1,%mm1;
	paddw		%mm7,%mm1;
	psraw		$2,%mm7;
	psraw		$2,%mm1;
	psubw		%mm4,%mm7;
	paddw		%mm4,%mm4;
	psubw		%mm3,%mm1;
	paddw		%mm3,%mm3;
	paddw		%mm7,%mm4;
	paddw		%mm1,%mm3;

	psubw		%mm2,%mm7;
	paddw		%mm2,%mm2;
	psubw		%mm0,%mm1;
	paddw		%mm0,%mm0;
	paddw		%mm7,%mm2;
	paddw		%mm1,%mm0;
	psubw		%mm5,%mm4;
	paddw		%mm5,%mm5;
	psubw		%mm6,%mm3;
	paddw		%mm6,%mm6;
	paddw		%mm4,%mm5;
	paddw		%mm3,%mm6;
	leal		(%edi,%ecx),%ebx;
	psraw		$4,%mm6;
	paddsw		0*16+0+3*768(%esi),%mm6;
	packuswb	0*16+8(%esi),%mm6;
	psraw		$4,%mm5;
	paddsw		1*16+0+3*768(%esi),%mm5;
	packuswb	1*16+8(%esi),%mm5;
	movq		%mm6,(%edi);		// 0
	psraw		$4,%mm2;
	paddsw		2*16+0+3*768(%esi),%mm2;
	packuswb	2*16+8(%esi),%mm2;
	movq		%mm5,(%edi,%ecx);	// 1
	psraw		$4,%mm0;
	paddsw		3*16+0+3*768(%esi),%mm0;
	packuswb	3*16+8(%esi),%mm0;
	movq		%mm2,(%edi,%ecx,2);	// 2
	psraw		$4,%mm1;
	paddsw		4*16+0+3*768(%esi),%mm1;
	packuswb	4*16+8(%esi),%mm1;
	movq		%mm0,(%ebx,%ecx,2);	// 3
	leal		(%ebx,%ecx,4),%ebx;
	psraw		$4,%mm7;
	paddsw		5*16+0+3*768(%esi),%mm7;
	packuswb	5*16+8(%esi),%mm7;
	movq		%mm1,(%edi,%ecx,4);	// 4
	psraw		$4,%mm4;
	paddsw  	6*16+0+3*768(%esi),%mm4;
	packuswb	6*16+8(%esi),%mm4;
	movq		%mm7,(%ebx);		// 5
	psraw		$4,%mm3;
	paddsw		7*16+0+3*768(%esi),%mm3;
	packuswb	7*16+8(%esi),%mm3;
	movq		%mm4,(%ebx,%ecx);	// 6
	addl		$128,%esi
	movq		%mm3,(%ebx,%ecx,2);	// 7

	jmp		3f

	.align		16
2:
	addl		$3*768,%esi
	leal		(%edi,%ecx),%ebx;
	movq		(%esi),%mm0;
	movq		1*8(%esi),%mm1;
	movq		2*8(%esi),%mm2;		
	packuswb 	%mm1,%mm0;
	movq		3*8(%esi),%mm3;		
	movq 		%mm0,(%edi);		// 0
	movq		4*8(%esi),%mm4;		
	packuswb	%mm3,%mm2;
	movq		5*8(%esi),%mm5;		
	movq		%mm2,(%edi,%ecx);	// 1
	movq		6*8(%esi),%mm6;		
	packuswb 	%mm5,%mm4;
	movq		7*8(%esi),%mm7;		
	movq 		%mm4,(%edi,%ecx,2);	// 2
	movq		8*8(%esi),%mm0;		
	packuswb	%mm7,%mm6;
	movq		9*8(%esi),%mm1;		
	movq		%mm6,(%ebx,%ecx,2);	// 3
	leal		(%ebx,%ecx,4),%ebx;
	movq		10*8(%esi),%mm2;	
	packuswb 	%mm1,%mm0;
	movq		11*8(%esi),%mm3;	
	movq 		%mm0,(%edi,%ecx,4);	// 4
	movq		12*8(%esi),%mm4;	
	packuswb	%mm3,%mm2;
	movq		13*8(%esi),%mm5;	
	movq		%mm2,(%ebx);		// 5
	movq		14*8(%esi),%mm6;	
	packuswb 	%mm5,%mm4;
	movq		15*8(%esi),%mm7;	
	movq 		%mm4,(%ebx,%ecx);	// 6
	packuswb	%mm7,%mm6;
	movq		%mm6,(%ebx,%ecx,2);	// 7
	addl		$128-3*768,%esi
3:
	cmpl		$5*8,%eax
	jle		1b

	popl %ebx
	popl %esi
	popl %edi
	ret

# not scheduled, will be 20% faster
# autosched still unfinished, sigh
	.text
	.align		16
	.globl		mmx_mpeg1_idct_intra2

mmx_mpeg1_idct_intra2:

	pushl %ebp
	movl %esp,%ebp
	subl $44,%esp
	pushl %edi
	pushl %esi
	pushl %ebx
	movl %eax,-4(%ebp)
	movl newref,%eax
	movl %eax,-8(%ebp)
	movl $lts,%eax
	movl -4(%ebp),%edx
	movsbl (%edx,%eax),%ecx
	movl $31,%edx
	sarl %cl,%edx
	andl $65535,%edx
	movl %edx,-16(%ebp)
	sarl $31,%edx
	movl %edx,-12(%ebp)
	movl -16(%ebp),%edx
	movl -12(%ebp),%ecx
	movl %edx,%ecx
	xorl %edx,%edx
	sall $16,%ecx
	movl -16(%ebp),%ebx
	movl -12(%ebp),%esi
	movl %ebx,%esi
	xorl %ebx,%ebx
	orl %ebx,%edx
	orl %esi,%ecx
	movl -16(%ebp),%ebx
	movl -12(%ebp),%esi
	shldl $16,%ebx,%esi
	sall $16,%ebx
	orl %ebx,%edx
	orl %esi,%ecx
	orl -16(%ebp),%edx
	orl -12(%ebp),%ecx
	movl %edx,mask
	movl %ecx,mask+4
	movl %edx,mask0
	movl %ecx,mask0+4
	movw $0,mask0
	movl -4(%ebp),%ebx
	movsbw (%ebx,%eax),%dx
	movl $5,%eax
	subl %edx,%eax
	movw %ax,shift
	movl $mblock+768,-20(%ebp)
	movl $0,-36(%ebp)
	movl $5,-28(%ebp)

	.p2align 4,,7
.L893:
	movl -36(%ebp),%esi
	movl $ltp,%edi
	movl -4(%ebp),%edx
	movl -36(%ebp),%ecx
	movl -20(%ebp),%ebx
	movl mb_address(%esi),%esi
	addl %esi,-8(%ebp)
	movsbl (%edx,%edi),%eax
	sall $7,%eax
	addl $mmx_q_idct_intra_q_lut,%eax
	movl mb_address+4(%ecx),%ecx
	movl $mblock,%esi
	movl -8(%ebp),%edi

	movq 0*16+0(%ebx),%mm0;
	movq %mm0,%mm3;
	movq %mm0,%mm1;
	psraw $15,%mm1;
	pmullw 0*16+0(%eax),%mm0;
	pand mask0,%mm1;
	paddw %mm1,%mm0;
	psraw shift,%mm0;
	movq %mm0,%mm2;
	pandn c1,%mm2;
	psubusw %mm2,%mm0;
	pand %mm1,%mm2;
	paddw %mm2,%mm0;
	paddw %mm2,%mm0;
	psllw $4,%mm0;
	paddsw %mm0,%mm0;
	psraw $5,%mm0;
	psllq $48+3,%mm3;
	psrlq $48,%mm3;
	por %mm3,%mm0;
	movq %mm0,0*16+0(%ebx);
	movq 1*16+0(%ebx),%mm0;
	movq %mm0,%mm1;
	psraw $15,%mm1;
	pmullw 1*16+0(%eax),%mm0;
	pand mask,%mm1;
	paddw %mm1,%mm0;
	psraw shift,%mm0;
	movq %mm0,%mm2;
	pandn c1,%mm2;	
	psubusw %mm2,%mm0;
	pand %mm1,%mm2;
	paddw %mm2,%mm0;
	paddw %mm2,%mm0;
	psllw $4,%mm0;
	paddsw %mm0,%mm0;
	psraw $5,%mm0;
	movq %mm0,1*16+0(%ebx);
	movq 2*16+0(%ebx),%mm0;
	movq %mm0,%mm1;
	psraw $15,%mm1;
	pmullw 2*16+0(%eax),%mm0;
	pand mask,%mm1;
	paddw %mm1,%mm0;
	psraw shift,%mm0;
	movq %mm0,%mm2;
	pandn c1,%mm2;	
	psubusw %mm2,%mm0;
	pand %mm1,%mm2;
	paddw %mm2,%mm0;
	paddw %mm2,%mm0;
	psllw $4,%mm0;
	paddsw %mm0,%mm0;
	psraw $5,%mm0;
	movq %mm0,2*16+0(%ebx);
	movq 3*16+0(%ebx),%mm0;
	movq %mm0,%mm1;
	psraw $15,%mm1;
	pmullw 3*16+0(%eax),%mm0;
	pand mask,%mm1;
	paddw %mm1,%mm0;
	psraw shift,%mm0;
	movq %mm0,%mm2;
	pandn c1,%mm2;	
	psubusw %mm2,%mm0;
	pand %mm1,%mm2;
	paddw %mm2,%mm0;
	paddw %mm2,%mm0;
	movq %mm0,3*16+0(%ebx);
	movq 4*16+0(%ebx),%mm0;
	movq %mm0,%mm1;
	psraw $15,%mm1;
	pmullw 4*16+0(%eax),%mm0;
	pand mask,%mm1;
	paddw %mm1,%mm0;
	psraw shift,%mm0;
	movq %mm0,%mm2;
	pandn c1,%mm2;	
	psubusw %mm2,%mm0;
	pand %mm1,%mm2;
	paddw %mm2,%mm0;
	paddw %mm2,%mm0;
	movq %mm0,4*16+0(%ebx);
	movq 5*16+0(%ebx),%mm0;
	movq %mm0,%mm1;
	psraw $15,%mm1;
	pmullw 5*16+0(%eax),%mm0;
	pand mask,%mm1;
	paddw %mm1,%mm0;
	psraw shift,%mm0;
	movq %mm0,%mm2;
	pandn c1,%mm2;	
	psubusw %mm2,%mm0;
	pand %mm1,%mm2;
	paddw %mm2,%mm0;
	paddw %mm2,%mm0;
	movq %mm0,5*16+0(%ebx);
	movq 6*16+0(%ebx),%mm0;
	movq %mm0,%mm1;
	psraw $15,%mm1;
	pmullw 6*16+0(%eax),%mm0;
	pand mask,%mm1;
	paddw %mm1,%mm0;
	psraw shift,%mm0;
	movq %mm0,%mm2;
	pandn c1,%mm2;	
	psubusw %mm2,%mm0;
	pand %mm1,%mm2;
	paddw %mm2,%mm0;
	paddw %mm2,%mm0;
	movq %mm0,6*16+0(%ebx);
	movq 7*16+0(%ebx),%mm0;
	movq %mm0,%mm1;
	psraw $15,%mm1;
	pmullw 7*16+0(%eax),%mm0;
	pand mask,%mm1;
	paddw %mm1,%mm0;
	psraw shift,%mm0;
	movq %mm0,%mm2;
	pandn c1,%mm2;	
	psubusw %mm2,%mm0;
	pand %mm1,%mm2;
	paddw %mm2,%mm0;
	paddw %mm2,%mm0;
	movq %mm0,7*16+0(%ebx);

	movq mmx_q_idct_inter_tab+10*8,%mm1;
	movq 1*16+0(%ebx),%mm4;
	movq 7*16+0(%ebx),%mm5;
	movq 3*16+0(%ebx),%mm6;
	movq 5*16+0(%ebx),%mm7;
	movq %mm4,%mm3;
	punpcklwd %mm5,%mm4;
	punpckhwd %mm5,%mm3;
	movq %mm4,%mm5;
	movq %mm3,%mm2;
	pmaddwd mmx_q_idct_inter_tab+1*8,%mm5;
	pmaddwd mmx_q_idct_inter_tab+1*8,%mm2;
	paddd %mm1,%mm5;
	paddd %mm1,%mm2;
	psrad $11,%mm5;
	psrad $11,%mm2;
	packssdw %mm2,%mm5;
	pmaddwd mmx_q_idct_inter_tab+0*8,%mm4;
	pmaddwd mmx_q_idct_inter_tab+0*8,%mm3;
	paddd %mm1,%mm4;
	paddd %mm1,%mm3;
	psrad $11,%mm4;
	psrad $11,%mm3;
	packssdw %mm3,%mm4;
	movq %mm6,%mm3;
	punpcklwd %mm7,%mm6;
	punpckhwd %mm7,%mm3;
	movq %mm6,%mm7;
	movq %mm3,%mm2;
	pmaddwd mmx_q_idct_inter_tab+3*8,%mm7;
	pmaddwd mmx_q_idct_inter_tab+3*8,%mm2;
	paddd %mm1,%mm7;
	paddd %mm1,%mm2;
	psrad $11,%mm7;
	psrad $11,%mm2;
	packssdw %mm2,%mm7;
	pmaddwd mmx_q_idct_inter_tab+2*8,%mm6;
	pmaddwd mmx_q_idct_inter_tab+2*8,%mm3;
	paddd %mm1,%mm6;
	paddd %mm1,%mm3;
	psrad $11,%mm6;
	psrad $11,%mm3;
	packssdw %mm3,%mm6;
	psubw %mm6,%mm4;
	paddw %mm6,%mm6;
	paddw %mm4,%mm6;
	psubw %mm7,%mm5;
	paddw %mm7,%mm7;
	paddw %mm5,%mm7;
	psubw %mm5,%mm4;
	paddw %mm4,%mm4;
	psllw $2,%mm5;
	paddw %mm4,%mm5;
	pmulhw cC4_15,%mm4;
	pmulhw cC4_15,%mm5;
	movq 6*16+0(%ebx),%mm3;
	movq 2*16+0(%ebx),%mm2;
	movq %mm2,%mm0;
	psubw %mm3,%mm2;
	psllw $3,%mm2;
	paddw c2,%mm2;
	pmulhw mmx_q_idct_inter_tab+12*8,%mm2;	
	psllw $2,%mm2;
	psllw $4,%mm3;
	psllw $4,%mm0;
	pmulhw mmx_q_idct_inter_tab+13*8,%mm3;
	paddw %mm3,%mm3;
	paddw %mm2,%mm3;
	pmulhw mmx_q_idct_inter_tab+15*8,%mm0;
	paddw %mm0,%mm2;
	movq 0*16+0(%ebx),%mm0;
	psllw $3,%mm0;
	movq 4*16+0(%ebx),%mm1;
	psllw $3,%mm1;
	psubw %mm1,%mm0;
	paddw %mm1,%mm1;
	paddw %mm0,%mm1;
	psubw %mm2,%mm0;
	paddw %mm2,%mm2;
	paddw %mm0,%mm2;
	psubw %mm3,%mm1;
	paddw %mm3,%mm3;
	paddw %mm1,%mm3;
	psubw %mm4,%mm0;
	paddw %mm4,%mm4;
	paddw %mm0,%mm4;
	psubw %mm7,%mm1;
	paddw %mm7,%mm7;
	paddw %mm1,%mm7;
	psubw %mm5,%mm2;
	paddw %mm5,%mm5;
	paddw %mm2,%mm5;
	psubw %mm6,%mm3;
	paddw %mm6,%mm6;
	paddw %mm3,%mm6;
	movq		%mm0,mm8;
	movq		%mm6,%mm0;		
	punpcklwd	%mm5,%mm6;
	punpckhwd	%mm5,%mm0;
	movq		%mm4,%mm5;		
	punpcklwd	%mm7,%mm4;
	punpckhwd	%mm7,%mm5;
	movq		%mm6,%mm7;		
	punpckldq	%mm4,%mm6;
	punpckhdq	%mm4,%mm7;
	movq		%mm0,%mm4;		
	punpckldq	%mm5,%mm0;
	punpckhdq	%mm5,%mm4;
	movq %mm6,mblock+0*16+0;
	movq %mm7,mblock+1*16+0;
	movq %mm0,mblock+2*16+0;
	movq %mm4,mblock+3*16+0;

	movq		mm8,%mm0;
	movq		%mm1,%mm6;		
	punpcklwd	%mm0,%mm1;
	punpckhwd	%mm0,%mm6;
	movq		%mm2,%mm7;		
	punpcklwd	%mm3,%mm2;
	punpckhwd	%mm3,%mm7;
	movq		%mm1,%mm0;		
	punpckldq	%mm2,%mm1;
	movq		%mm6,%mm3;		
	punpckhdq	%mm2,%mm0;
	movq 		%mm1,mblock+0*16+8;	
	punpckldq	%mm7,%mm6;
	movq 		%mm0,mblock+1*16+8;	
	punpckhdq	%mm7,%mm3;
	movq %mm6,mblock+2*16+8;
	movq %mm3,mblock+3*16+8;

	movq 0*16+8(%ebx),%mm0;
	movq %mm0,%mm1;
	psraw $15,%mm1;
	pmullw 0*16+8(%eax),%mm0;
	pand mask,%mm1;
	paddw %mm1,%mm0;
	psraw shift,%mm0;
	movq %mm0,%mm2;
	pandn c1,%mm2;	
	psubusw %mm2,%mm0;
	pand %mm1,%mm2;
	paddw %mm2,%mm0;
	paddw %mm2,%mm0;
	movq %mm0,0*16+8(%ebx);
	movq 2*16+8(%ebx),%mm0;
	movq %mm0,%mm1;
	psraw $15,%mm1;
	pmullw 2*16+8(%eax),%mm0;
	pand mask,%mm1;
	paddw %mm1,%mm0;
	psraw shift,%mm0;
	movq %mm0,%mm2;
	pandn c1,%mm2;	
	psubusw %mm2,%mm0;
	pand %mm1,%mm2;
	paddw %mm2,%mm0;
	paddw %mm2,%mm0;
	movq %mm0,2*16+8(%ebx);
	movq 4*16+8(%ebx),%mm0;
	movq %mm0,%mm1;
	psraw $15,%mm1;
	pmullw 4*16+8(%eax),%mm0;
	pand mask,%mm1;
	paddw %mm1,%mm0;
	psraw shift,%mm0;
	movq %mm0,%mm2;
	pandn c1,%mm2;	
	psubusw %mm2,%mm0;
	pand %mm1,%mm2;
	paddw %mm2,%mm0;
	paddw %mm2,%mm0;
	movq %mm0,4*16+8(%ebx);
	movq 5*16+8(%ebx),%mm0;
	movq %mm0,%mm1;
	psraw $15,%mm1;
	pmullw 5*16+8(%eax),%mm0;
	pand mask,%mm1;
	paddw %mm1,%mm0;
	psraw shift,%mm0;
	movq %mm0,%mm2;
	pandn c1,%mm2;	
	psubusw %mm2,%mm0;
	pand %mm1,%mm2;
	paddw %mm2,%mm0;
	paddw %mm2,%mm0;
	movq %mm0,5*16+8(%ebx);
	movq 6*16+8(%ebx),%mm0;
	movq %mm0,%mm1;
	psraw $15,%mm1;
	pmullw 6*16+8(%eax),%mm0;
	pand mask,%mm1;
	paddw %mm1,%mm0;
	psraw shift,%mm0;
	movq %mm0,%mm2;
	pandn c1,%mm2;	
	psubusw %mm2,%mm0;
	pand %mm1,%mm2;
	paddw %mm2,%mm0;
	paddw %mm2,%mm0;
	movq %mm0,6*16+8(%ebx);
	movq		mask,%mm0;
	movq		shift,%mm3;
	movq 1*16+8(%ebx),%mm4;
	movq %mm4,%mm1;
	pmullw 1*16+8(%eax),%mm4;
	psraw $15,%mm1;
	pand %mm0,%mm1;
	paddw %mm1,%mm4;
	psraw %mm3,%mm4;
	movq %mm4,%mm2;
	pandn c1,%mm2;	
	psubusw %mm2,%mm4;
	pand %mm1,%mm2;
	paddw %mm2,%mm4;
	paddw %mm2,%mm4;
	movq 7*16+8(%ebx),%mm5;
	movq %mm5,%mm1;
	pmullw 7*16+8(%eax),%mm5;
	psraw $15,%mm1;
	pand %mm0,%mm1;
	paddw %mm1,%mm5;
	psraw %mm3,%mm5;
	movq %mm5,%mm2;
	pandn c1,%mm2;	
	psubusw %mm2,%mm5;
	pand %mm1,%mm2;
	paddw %mm2,%mm5;
	paddw %mm2,%mm5;
	movq 3*16+8(%ebx),%mm6;
	movq %mm6,%mm1;
	pmullw 3*16+8(%eax),%mm6;
	psraw $15,%mm1;
	pand %mm0,%mm1;
	paddw %mm1,%mm6;
	psraw %mm3,%mm6;
	movq %mm6,%mm2;
	pandn c1,%mm2;	
	psubusw %mm2,%mm6;
	pand %mm1,%mm2;
	paddw %mm2,%mm6;
	paddw %mm2,%mm6;

	movq mmx_q_idct_inter_tab+10*8,%mm1;
	movq 3*16+8(%ebx),%mm6;
	movq 5*16+8(%ebx),%mm7;
	movq %mm4,%mm3;
	punpcklwd %mm5,%mm4;
	punpckhwd %mm5,%mm3;
	movq %mm4,%mm5;
	movq %mm3,%mm2;
	pmaddwd mmx_q_idct_inter_tab+1*8,%mm5;
	pmaddwd mmx_q_idct_inter_tab+1*8,%mm2;
	paddd %mm1,%mm5;
	paddd %mm1,%mm2;
	psrad $11,%mm5;
	psrad $11,%mm2;
	packssdw %mm2,%mm5;
	pmaddwd mmx_q_idct_inter_tab+0*8,%mm4;
	pmaddwd mmx_q_idct_inter_tab+0*8,%mm3;
	paddd %mm1,%mm4;
	paddd %mm1,%mm3;
	psrad $11,%mm4;
	psrad $11,%mm3;
	packssdw %mm3,%mm4;
	movq %mm6,%mm3;
	punpcklwd %mm7,%mm6;
	punpckhwd %mm7,%mm3;
	movq %mm6,%mm7;
	movq %mm3,%mm2;
	pmaddwd mmx_q_idct_inter_tab+3*8,%mm7;
	pmaddwd mmx_q_idct_inter_tab+3*8,%mm2;
	paddd %mm1,%mm7;
	paddd %mm1,%mm2;
	psrad $11,%mm7;
	psrad $11,%mm2;
	packssdw %mm2,%mm7;
	pmaddwd mmx_q_idct_inter_tab+2*8,%mm6;
	pmaddwd mmx_q_idct_inter_tab+2*8,%mm3;
	paddd %mm1,%mm6;
	paddd %mm1,%mm3;
	psrad $11,%mm6;
	psrad $11,%mm3;
	packssdw %mm3,%mm6;
	psubw %mm6,%mm4;
	paddw %mm6,%mm6;
	paddw %mm4,%mm6;
	psubw %mm7,%mm5;
	paddw %mm7,%mm7;
	paddw %mm5,%mm7;
	psubw %mm5,%mm4;
	paddw %mm4,%mm4;
	psllw $2,%mm5;
	paddw %mm4,%mm5;
	pmulhw cC4_15,%mm4;
	pmulhw cC4_15,%mm5;

	movq 2*16+8(%ebx),%mm2;
	movq 6*16+8(%ebx),%mm3;
	movq %mm2,%mm0;
	psubw %mm3,%mm2;
	psllw $3,%mm2;
	paddw c2,%mm2;
	pmulhw mmx_q_idct_inter_tab+12*8,%mm2;	
	psllw $2,%mm2;
	psllw $4,%mm3;
	psllw $4,%mm0;
	pmulhw mmx_q_idct_inter_tab+13*8,%mm3;
	paddw %mm3,%mm3;
	paddw %mm2,%mm3;
	pmulhw mmx_q_idct_inter_tab+15*8,%mm0;
	paddw %mm0,%mm2;
	movq 0*16+8(%ebx),%mm0;
	psllw $3,%mm0;
	movq 4*16+8(%ebx),%mm1;
	psllw $3,%mm1;
	psubw %mm1,%mm0;
	paddw %mm1,%mm1;
	paddw %mm0,%mm1;
	psubw %mm2,%mm0;
	paddw %mm2,%mm2;
	paddw %mm0,%mm2;
	psubw %mm3,%mm1;
	paddw %mm3,%mm3;
	paddw %mm1,%mm3;

	psubw %mm4,%mm0;
	paddw %mm4,%mm4;
	paddw %mm0,%mm4;
	psubw %mm7,%mm1;
	paddw %mm7,%mm7;
	paddw %mm1,%mm7;
	psubw %mm5,%mm2;
	paddw %mm5,%mm5;
	paddw %mm2,%mm5;
	psubw %mm6,%mm3;
	paddw %mm6,%mm6;
	paddw %mm3,%mm6;
	movq		%mm0,mm8;
	movq		%mm1,mm9;
	movq		%mm6,%mm0;
	punpcklwd	%mm5,%mm6;	
	punpckhwd	%mm5,%mm0;	
	movq		%mm4,%mm1;
	punpcklwd	%mm7,%mm4;	
	punpckhwd	%mm7,%mm1;	
	movq		%mm6,%mm5;
	punpckldq	%mm4,%mm6;	
	punpckhdq	%mm4,%mm5;	
	movq		%mm0,%mm4;
	movq		%mm0,%mm7;
	punpckldq	%mm1,%mm4;	
	punpckhdq	%mm1,%mm7;	
	movq		mm8,%mm0;
	movq		mm9,%mm1;
	movq %mm6,mblock+4*16+0;
	movq %mm5,mblock+5*16+0;
	movq %mm4,mblock+6*16+0;
	movq %mm7,mblock+7*16+0;
	movq		%mm1,%mm6;
	punpcklwd	%mm0,%mm1;	
	punpckhwd	%mm0,%mm6;	
	movq		%mm2,%mm7;
	punpcklwd	%mm3,%mm2;	
	punpckhwd	%mm3,%mm7;	
	movq		%mm1,%mm0;
	punpckldq	%mm2,%mm1;	
	punpckhdq	%mm2,%mm0;	
	movq		%mm6,%mm2;
	movq		%mm6,%mm3;
	punpckldq	%mm7,%mm2;	
	punpckhdq	%mm7,%mm3;	
	movq %mm1,mblock+4*16+8;
	movq %mm0,mblock+5*16+8;
	movq %mm2,mblock+6*16+8;
	movq %mm3,mblock+7*16+8;

	movq c1,%mm1;
	movq c2,%mm2;
	movq mblock+7*16+8,%mm4;
	movq %mm4,%mm0;
	movq mblock+1*16+8,%mm5;
	paddw %mm2,%mm4;
	paddw %mm5,%mm4;
	pmulhw mmx_q_idct_inter_tab+4*8,%mm4;
	paddw %mm1,%mm5;
	psubw %mm1,%mm0;
	pmulhw mmx_q_idct_inter_tab+5*8,%mm5;
	psubw %mm4,%mm5;
	pmulhw mmx_q_idct_inter_tab+6*8,%mm0;
	paddw %mm0,%mm4;
	movq mblock+3*16+8,%mm6;
	movq %mm6,%mm3;
	movq mblock+5*16+8,%mm7;
	paddw %mm2,%mm6;
	paddw %mm7,%mm6;
	pmulhw mmx_q_idct_inter_tab+7*8,%mm6;
	paddw %mm1,%mm7;
	paddw %mm2,%mm3;
	pmulhw mmx_q_idct_inter_tab+8*8,%mm7;
	psubw %mm6,%mm7;
	pmulhw mmx_q_idct_inter_tab+9*8,%mm3;
	paddw %mm3,%mm6;
	psubw %mm6,%mm4;
	paddw %mm6,%mm6;
	paddw %mm4,%mm6;
	psubw %mm7,%mm5;
	paddw %mm7,%mm7;
	paddw %mm5,%mm7;
	psubw %mm5,%mm4;
	paddw %mm4,%mm4;
	psllw $2,%mm5;
	paddw %mm4,%mm5;
	pmulhw cC4_15,%mm4;
	pmulhw cC4_15,%mm5;

	movq mblock+2*16+8,%mm0;
	movq mblock+6*16+8,%mm3;
	paddw %mm0,%mm2;
	psubw %mm3,%mm2;
	pmulhw mmx_q_idct_inter_tab+12*8,%mm2;
	psubw %mm1,%mm0;
	pmulhw mmx_q_idct_inter_tab+13*8,%mm3;
	paddw %mm2,%mm3;
	pmulhw mmx_q_idct_inter_tab+14*8,%mm0;
	paddw %mm0,%mm2;
	movq mblock+0*16+8,%mm0;
	movq mblock+4*16+8,%mm1;
// paddw mmx_q_idct_inter_tab+11*8,%mm0;
	paddw c1x,%mm0;
	psubw %mm1,%mm0;
	paddw %mm1,%mm1;
	paddw %mm0,%mm1;
	psraw $2,%mm0;
	psraw $2,%mm1;
	psubw %mm2,%mm0;
	paddw %mm2,%mm2;
	paddw %mm0,%mm2;
	psubw %mm3,%mm1;
	paddw %mm3,%mm3;
	paddw %mm1,%mm3;

	psubw %mm4,%mm0;
	paddw %mm4,%mm4;
	paddw %mm0,%mm4;
	psubw %mm7,%mm1;
	paddw %mm7,%mm7;
	paddw %mm1,%mm7;
	psubw %mm5,%mm2;
	paddw %mm5,%mm5;
	paddw %mm2,%mm5;
	psubw %mm6,%mm3;
	paddw %mm6,%mm6;
	paddw %mm3,%mm6;
	psraw $4,%mm6;
	movq %mm6,0*16+8(%ebx);
	psraw $4,%mm5;
	movq %mm5,1*16+8(%ebx);
	psraw $4,%mm4;
	movq %mm4,2*16+8(%ebx);
	psraw $4,%mm7;
	movq %mm7,3*16+8(%ebx);
	psraw $4,%mm1;
	movq %mm1,4*16+8(%ebx);
	psraw $4,%mm0;
	movq %mm0,5*16+8(%ebx);
	psraw $4,%mm2;
	movq %mm2,6*16+8(%ebx);
	psraw $4,%mm3;
	movq %mm3,7*16+8(%ebx);

	movq c1,%mm1;
	movq c2,%mm2;
	movq mblock+7*16+0,%mm4;
	movq %mm4,%mm0;
	movq mblock+1*16+0,%mm5;
	paddw %mm2,%mm4;
	paddw %mm5,%mm4;
	pmulhw mmx_q_idct_inter_tab+4*8,%mm4;
	paddw %mm1,%mm5;
	psubw %mm1,%mm0;
	pmulhw mmx_q_idct_inter_tab+5*8,%mm5;
	psubw %mm4,%mm5;
	pmulhw mmx_q_idct_inter_tab+6*8,%mm0;
	paddw %mm0,%mm4;
	movq mblock+3*16+0,%mm6;
	movq %mm6,%mm3;
	movq mblock+5*16+0,%mm7;
	paddw %mm2,%mm6;
	paddw %mm7,%mm6;
	pmulhw mmx_q_idct_inter_tab+7*8,%mm6;
	paddw %mm1,%mm7;
	paddw %mm2,%mm3;
	pmulhw mmx_q_idct_inter_tab+8*8,%mm7;
	psubw %mm6,%mm7;
	pmulhw mmx_q_idct_inter_tab+9*8,%mm3;
	paddw %mm3,%mm6;
	psubw %mm6,%mm4;
	paddw %mm6,%mm6;
	paddw %mm4,%mm6;
	psubw %mm7,%mm5;
	paddw %mm7,%mm7;
	paddw %mm5,%mm7;
	psubw %mm5,%mm4;
	paddw %mm4,%mm4;
	psllw $2,%mm5;
	paddw %mm4,%mm5;
	pmulhw cC4_15,%mm4;
	pmulhw cC4_15,%mm5;

	movq mblock+2*16+0,%mm0;
	movq mblock+6*16+0,%mm3;
	paddw %mm0,%mm2;
	psubw %mm3,%mm2;
	pmulhw mmx_q_idct_inter_tab+12*8,%mm2;
	psubw %mm1,%mm0;
	pmulhw mmx_q_idct_inter_tab+13*8,%mm3;
	paddw %mm2,%mm3;
	pmulhw mmx_q_idct_inter_tab+14*8,%mm0;
	paddw %mm0,%mm2;
	movq mblock+0*16+0,%mm0;
	movq mblock+4*16+0,%mm1;
// paddw mmx_q_idct_inter_tab+11*8,%mm0;
	paddw c1x,%mm0;
	psubw %mm1,%mm0;
	paddw %mm1,%mm1;
	paddw %mm0,%mm1;
	psraw $2,%mm0;
	psraw $2,%mm1;
	psubw %mm2,%mm0;
	paddw %mm2,%mm2;
	paddw %mm0,%mm2;
	psubw %mm3,%mm1;
	paddw %mm3,%mm3;
	paddw %mm1,%mm3;

	psubw %mm4,%mm0;
	paddw %mm4,%mm4;
	paddw %mm0,%mm4;
	psubw %mm7,%mm1;
	paddw %mm7,%mm7;
	paddw %mm1,%mm7;
	psraw $4,%mm4;
	psraw $4,%mm0;
	packuswb  2*16+4*2(%ebx), %mm4;
	packuswb  5*16+4*2(%ebx), %mm0;
	psubw %mm5,%mm2;
	paddw %mm5,%mm5;
	paddw %mm2,%mm5;
	psraw $4,%mm7;
	psraw $4,%mm1;
	packuswb  3*16+4*2(%ebx), %mm7;
	packuswb  4*16+4*2(%ebx), %mm1;
	psubw %mm6,%mm3;
	paddw %mm6,%mm6;
	paddw %mm3,%mm6;
	psraw $4,%mm5;
	psraw $4,%mm2;
	packuswb  1*16+4*2(%ebx), %mm5;
	packuswb  6*16+4*2(%ebx), %mm2;
	psraw $4,%mm6;
	psraw $4,%mm3;
	packuswb  0*16+4*2(%ebx), %mm6;
	packuswb  7*16+4*2(%ebx), %mm3;
	leal	(%edi,%ecx),%esi;
	movq %mm6, (%edi);	// 0
	movq %mm5, (%edi,%ecx);	// 1
	movq %mm4, (%edi,%ecx,2);	// 2
	movq %mm7, (%esi,%ecx,2);	// 3
	leal	(%esi,%ecx,4),%esi;
	movq %mm1, (%edi,%ecx,4);	// 4
	movq %mm0, (%esi);	// 5
	movq %mm2, (%esi,%ecx);	// 6
	movq %mm3, (%esi,%ecx,2);	// 7

	subl $-128,%ebx
	movl %ebx,-20(%ebp)
	addl $8,-36(%ebp)
	decl -28(%ebp)
	jns .L893
	popl %ebx
	popl %esi
	popl %edi
	movl %ebp,%esp
	popl %ebp
	ret

	.text
	.align		16
	.globl		mmx_copy_refblock

mmx_copy_refblock:

	movl		$mblock+3*6*128,%eax;
	pushl		%ebx;
	movl		$mb_address,%ebx;
	movl		newref,%edx;
	pushl		%esi;			
	pushl		%edi;

	.align		16
1:
	addl		(%ebx),%edx;		movl		4(%ebx),%esi;
	movq		(%eax),%mm0;		addl		$8,%ebx;
	movq		1*8(%eax),%mm1;		leal		(%edx,%esi),%edi;
	movq		2*8(%eax),%mm2;		packuswb 	%mm1,%mm0;
	movq		3*8(%eax),%mm3;		movq 		%mm0,(%edx);		// 0
	movq		4*8(%eax),%mm4;		packuswb	%mm3,%mm2;
	movq		5*8(%eax),%mm5;		movq		%mm2,(%edx,%esi);	// 1
	movq		6*8(%eax),%mm6;		packuswb 	%mm5,%mm4;
	movq		7*8(%eax),%mm7;		movq 		%mm4,(%edx,%esi,2);	// 2
	movq		8*8(%eax),%mm0;		packuswb	%mm7,%mm6;
	movq		9*8(%eax),%mm1;		movq		%mm6,(%edi,%esi,2);	// 3
	movq		10*8(%eax),%mm2;	packuswb 	%mm1,%mm0;
	movq		11*8(%eax),%mm3;	movq 		%mm0,(%edx,%esi,4);	// 4
	movq		12*8(%eax),%mm4;	leal		(%edi,%esi,4),%edi;
	movq		13*8(%eax),%mm5;	packuswb	%mm3,%mm2;
	movq		14*8(%eax),%mm6;	movq		%mm2,(%edi);		// 5
	movq		15*8(%eax),%mm7;	packuswb 	%mm5,%mm4;
	leal		128(%eax),%eax;		movq 		%mm4,(%edi,%esi);	// 6	
	cmpl		$mb_address+6*8,%ebx;	packuswb	%mm7,%mm6;
	movq		%mm6,(%edi,%esi,2);	jne		1b;			// 7

	popl		%edi;			
	popl		%esi;
	popl		%ebx;			
	ret;
