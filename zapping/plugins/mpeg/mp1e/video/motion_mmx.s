#
#  MPEG-1 Real Time Encoder
# 
#  Copyright (C) 1999-2000 Michael H. Schimek
# 
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) version 2.
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

# $Id: motion_mmx.s,v 1.8 2001-07-12 01:22:06 mschimek Exp $

		.text
		.align		16
		.globl		mmx_predict_forward_packed

mmx_predict_forward_packed:

		pxor		%mm5,%mm5;
		movl		$mblock,%ecx
		movq		(%eax),%mm0;
		pxor		%mm6,%mm6;
		movq		(%ecx),%mm2;
		pxor		%mm7,%mm7;
		movq		8(%ecx),%mm3;

		.align 16
1:
		movq		128(%eax),%mm4;
		movq		%mm0,%mm1;
		punpcklbw	%mm5,%mm0;
		punpckhbw	%mm5,%mm1;
		psubw		%mm0,%mm2;
		paddw		%mm2,%mm6;
		psubw		%mm1,%mm3;
		paddw		%mm3,%mm6;
		movq		%mm0,768*3+0(%ecx);
		movq		%mm1,768*3+8(%ecx);
		movq		%mm2,768*1+0(%ecx);
		movq		%mm3,768*1+8(%ecx);
		pmaddwd		%mm2,%mm2;
		pmaddwd		%mm3,%mm3;
		paddd		%mm2,%mm7;
		paddd		%mm3,%mm7;
		movq		256+0(%ecx),%mm0;
		movq		256+8(%ecx),%mm1;
		movq		256(%eax),%mm3;
		movq		%mm4,%mm2;
		punpcklbw	%mm5,%mm4;
		punpckhbw	%mm5,%mm2;
		psubw		%mm4,%mm0;
		paddw		%mm0,%mm6;
		psubw		%mm2,%mm1;
		paddw		%mm1,%mm6;
		movq		%mm4,768*3+256+0(%ecx);
		movq		%mm2,768*3+256+8(%ecx);
		movq		%mm0,768*1+256+0(%ecx);
		movq		%mm1,768*1+256+8(%ecx);
		cmpl		$mblock+15*16-16,%ecx;
		pmaddwd		%mm0,%mm0;
		pmaddwd		%mm1,%mm1;
		paddd		%mm0,%mm7;
		paddd		%mm1,%mm7;
		movq		512+0(%ecx),%mm2;
		movq		512+8(%ecx),%mm4;
		movq		%mm3,%mm1;
		punpcklbw	%mm5,%mm3;
		punpckhbw	%mm5,%mm1;
		psubw		%mm3,%mm2;
		psubw		%mm1,%mm4;
		movq		%mm3,768*3+512+0(%ecx);
		movq		%mm1,768*3+512+8(%ecx);
		movq		%mm2,768*1+512+0(%ecx);
		movq		%mm4,768*1+512+8(%ecx);
		movq		8(%eax),%mm0;
		leal		8(%eax),%eax;
		movq		0+16(%ecx),%mm2;
		movq		8+16(%ecx),%mm3;
		leal		16(%ecx),%ecx;
		jne		1b;

		movq		128(%eax),%mm4;
		movq		%mm0,%mm1;
		punpcklbw	%mm5,%mm0;
		punpckhbw	%mm5,%mm1;
		psubw		%mm0,%mm2;
		paddw		%mm2,%mm6;
		psubw		%mm1,%mm3;
		paddw		%mm3,%mm6;
		movq		%mm0,768*3+0(%ecx);
		movq		%mm1,768*3+8(%ecx);
		movq		%mm2,768*1+0(%ecx);
		movq		%mm3,768*1+8(%ecx);
		pmaddwd		%mm2,%mm2;
		pmaddwd		%mm3,%mm3;
		paddd		%mm2,%mm7;
		paddd		%mm3,%mm7;
		movq		256+0(%ecx),%mm2;
		movq		256+8(%ecx),%mm3;
		movq		256(%eax),%mm0;
		movq		%mm4,%mm1;
		punpcklbw	%mm5,%mm4;
		punpckhbw	%mm5,%mm1;
		psubw		%mm4,%mm2;
		paddw		%mm2,%mm6;
		psubw		%mm1,%mm3;
		paddw		%mm3,%mm6;
		pmaddwd		c1,%mm6;
		movq		%mm4,768*3+256+0(%ecx);
		movq		%mm1,768*3+256+8(%ecx);
		movq		%mm2,768*1+256+0(%ecx);
		movq		%mm3,768*1+256+8(%ecx);
		pmaddwd		%mm2,%mm2;
		pmaddwd		%mm3,%mm3;
		paddd		%mm2,%mm7;
		paddd		%mm3,%mm7;
		movq		%mm7,%mm4;
		psrlq		$32,%mm7;
		paddd		%mm4,%mm7;
		movq		512+0(%ecx),%mm2;
		movq		512+8(%ecx),%mm3;
		movq		%mm0,%mm1;
		punpcklbw	%mm5,%mm0;
		punpckhbw	%mm5,%mm1;
		movq		%mm6,%mm5;
		psubw		%mm0,%mm2;
		psubw		%mm1,%mm3;
		movq		%mm0,768*3+512+0(%ecx);
		movq		%mm1,768*3+512+8(%ecx);
		psrlq		$32,%mm6;
		paddd		%mm5,%mm6;
		movq		%mm2,768*1+512+0(%ecx);
		movq		%mm3,768*1+512+8(%ecx);
		movd		%mm6,%ecx;
		imul		%ecx,%ecx;
		pslld		$8,%mm7;
		movd		%mm7,%eax;
		subl		%ecx,%eax;
		ret

		.text
		.align		16
		.globl		mmx_predict_forward_planar

mmx_predict_forward_planar:

		movq		(%eax),%mm0;
		pxor		%mm5,%mm5;
		movq		8(%eax),%mm4;
		pushl		%edi
		pxor		%mm6,%mm6;
		pushl		%esi
		pxor		%mm7,%mm7;
		pushl		%ebx
		movl		$mblock,%ebx
		movl		mb_address+4,%ecx
		leal		8(%eax,%ecx,8),%esi

		.align 16
1:
		addl		%ecx,%eax;
		movq		(%ebx),%mm2;
		movq		8(%ebx),%mm3;
		movq		%mm0,%mm1;
		punpcklbw	%mm5,%mm0;
		punpckhbw	%mm5,%mm1;
		psubw		%mm0,%mm2;
		psubw		%mm1,%mm3;
		movq		%mm0,3*768(%ebx);
		movq		(%eax),%mm0;
		movq		%mm1,3*768+8(%ebx);
		movq		%mm2,1*768(%ebx);
		movq		%mm3,1*768+8(%ebx);
		paddw		%mm2,%mm6;
		paddw		%mm3,%mm6;
		pmaddwd		%mm2,%mm2;
		pmaddwd		%mm3,%mm3;
		paddd		%mm2,%mm7;
		paddd		%mm3,%mm7;

		movq		256(%ebx),%mm2;
		movq		256+8(%ebx),%mm3;
		cmpl		$mblock+256-32,%ebx;
		movq		%mm4,%mm1;
		punpcklbw	%mm5,%mm4;
		punpckhbw	%mm5,%mm1;
		psubw		%mm4,%mm2;
		psubw		%mm1,%mm3;
		movq		%mm4,3*768+256(%ebx);
		movq		8(%eax),%mm4;
		movq		%mm1,3*768+256+8(%ebx);
		movq		%mm2,1*768+256(%ebx);
		movq		%mm3,1*768+256+8(%ebx);
		leal		16(%ebx),%ebx;
		paddw		%mm2,%mm6;
		paddw		%mm3,%mm6;
		pmaddwd		%mm2,%mm2;
		pmaddwd		%mm3,%mm3;
		paddd		%mm2,%mm7;
		paddd		%mm3,%mm7;

		jne		1b;

		movq		(%ebx),%mm2;
		movq		8(%ebx),%mm3;
		movq		%mm0,%mm1;
		punpcklbw	%mm5,%mm0;
		punpckhbw	%mm5,%mm1;
		psubw		%mm0,%mm2;
		psubw		%mm1,%mm3;
		movq		%mm0,3*768(%ebx);
		movq		%mm1,3*768+8(%ebx);
		movq		%mm2,1*768(%ebx);
		movq		%mm3,1*768+8(%ebx);
		paddw		%mm2,%mm6;
		paddw		%mm3,%mm6;
		pmaddwd		%mm2,%mm2;
		pmaddwd		%mm3,%mm3;
		paddd		%mm2,%mm7;
		paddd		%mm3,%mm7;

		movq		256(%ebx),%mm2;
		movq		256+8(%ebx),%mm3;
		movq		%mm4,%mm1;
		punpcklbw	%mm5,%mm4;
		punpckhbw	%mm5,%mm1;
		psubw		%mm4,%mm2;
		psubw		%mm1,%mm3;
		paddw		%mm2,%mm6;
		paddw		%mm3,%mm6;
		pmaddwd		c1,%mm6;
		movq		%mm4,3*768+256(%ebx);
		movq		%mm1,3*768+256+8(%ebx);
		movq		%mm2,1*768+256(%ebx);
		movq		%mm3,1*768+256+8(%ebx);
		pmaddwd		%mm2,%mm2;
		pmaddwd		%mm3,%mm3;
		paddd		%mm2,%mm7;
		paddd		%mm3,%mm7;

		movq		%mm7,%mm4;
		psrlq		$32,%mm7;
		paddd		%mm4,%mm7;
		movq		%mm6,%mm1;
		psrlq		$32,%mm6;
		paddd		%mm1,%mm6;
		movd		%mm6,%edi;
		imul		%edi,%edi;
		addl		mb_address+32,%esi
		movl		$mblock+512,%ecx
		movl		mb_address+40,%ebx
		pslld		$8,%mm7;
		movd		%mm7,%eax;
		subl		%edi,%eax;
		movl		mb_address+36,%edi;

1:
		movq		(%esi),%mm0;
		movq		(%esi,%ebx),%mm4;
		addl		%edi,%esi;
		movq		(%ecx),%mm2;
		movq		8(%ecx),%mm3;
		movq		%mm0,%mm1;
		punpcklbw	%mm5,%mm0;
		punpckhbw	%mm5,%mm1;
		movq		%mm0,3*768(%ecx);
		psubw		%mm0,%mm2;
		movq		(%esi),%mm0;
		psubw		%mm1,%mm3;
		movq		%mm1,3*768+8(%ecx);
		movq		%mm2,1*768(%ecx);
		movq		%mm3,1*768+8(%ecx);

		movq		128(%ecx),%mm2;
		movq		128+8(%ecx),%mm3;
		movq		%mm4,%mm1;
		punpcklbw	%mm5,%mm4;
		punpckhbw	%mm5,%mm1;
		movq		%mm4,3*768+128(%ecx);
		psubw		%mm4,%mm2;
		movq		(%esi,%ebx),%mm4;
		addl		%edi,%esi;
		psubw		%mm1,%mm3;
		movq		%mm1,3*768+128+8(%ecx);
		movq		%mm2,1*768+128(%ecx);
		movq		%mm3,1*768+128+8(%ecx);

		movq		16(%ecx),%mm2;
		movq		24(%ecx),%mm3;
		cmpl		$mblock+5*128-32,%ecx;
		movq		%mm0,%mm1;
		punpcklbw	%mm5,%mm0;
		punpckhbw	%mm5,%mm1;
		psubw		%mm0,%mm2;
		psubw		%mm1,%mm3;
		movq		%mm0,3*768+16(%ecx);
		movq		%mm1,3*768+24(%ecx);
		movq		%mm2,1*768+16(%ecx);
		movq		%mm3,1*768+24(%ecx);

		movq		128+16(%ecx),%mm2;
		movq		128+24(%ecx),%mm3;
		movq		%mm4,%mm1;
		punpcklbw	%mm5,%mm4;
		punpckhbw	%mm5,%mm1;
		psubw		%mm4,%mm2;
		psubw		%mm1,%mm3;
		movq		%mm4,3*768+128+16(%ecx);
		movq		%mm1,3*768+128+24(%ecx);
		movq		%mm2,1*768+128+16(%ecx);
		movq		%mm3,1*768+128+24(%ecx);

		leal		32(%ecx),%ecx;
		jne		1b;

		popl		%ebx
		popl		%esi
		popl		%edi
		ret

		.text
		.align		16
		.globl		mmx_predict_bidirectional_packed

# XXX needs work

mmx_predict_bidirectional_packed:

	subl $28,%esp
	movl $mblock,%eax
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	xorl %edi,%edi
	movl $32,%ebp
	movl 48(%esp),%ebx
	movl 52(%esp),%esi
#APP
		pxor		%mm5,%mm5;
		pxor		%mm6,%mm6;
		pxor		%mm7,%mm7;
1:
		movq		(%ebx,%edi),%mm0;
		punpcklbw	c0,%mm0;
		movq		(%esi,%edi),%mm1;
		punpcklbw	c0,%mm1;
		movq		(%eax,%edi,2),%mm2;
		movq		%mm2,%mm3;
		movq		%mm2,%mm4;
		psubw		%mm0,%mm3;
		movq		%mm3,768(%eax,%edi,2);
		pmaddwd		%mm3,%mm3;
		paddd		%mm3,%mm5;
		psubw		%mm1,%mm4;
		movq		%mm4,1536(%eax,%edi,2);
		pmaddwd		%mm4,%mm4;
		paddd		%mm4,%mm6;
		paddw		%mm1,%mm0;
		paddw		c1,%mm0;
		psrlw		$1,%mm0;
		psubw		%mm0,%mm2;
		movq		%mm2,2304(%eax,%edi,2);
		pmaddwd		%mm2,%mm2;
		paddd		%mm2,%mm7;		
		
		movq		(%ebx,%edi),%mm0;
		punpckhbw	c0,%mm0;
		movq		(%esi,%edi),%mm1;
		punpckhbw	c0,%mm1;
		movq		8(%eax,%edi,2),%mm2;
		movq		%mm2,%mm3;
		movq		%mm2,%mm4;
		psubw		%mm0,%mm3;
		movq		%mm3,768+8(%eax,%edi,2);
		pmaddwd		%mm3,%mm3;
		paddd		%mm3,%mm5;
		psubw		%mm1,%mm4;
		movq		%mm4,1536+8(%eax,%edi,2);
		pmaddwd		%mm4,%mm4;
		paddd		%mm4,%mm6;
		paddw		%mm1,%mm0;
		paddw		c1,%mm0;
		psrlw		$1,%mm0;
		psubw		%mm0,%mm2;
		movq		%mm2,2304+8(%eax,%edi,2);
		pmaddwd		%mm2,%mm2;
		paddd		%mm2,%mm7;

		addl		$8,%edi;
		decl		%ebp;
		jne		1b;

		movq		%mm7,%mm0;		psrlq		$32,%mm7;
		paddd		%mm7,%mm0;		movq		%mm5,%mm1;
		psrlq		$32,%mm5;		movq		%mm6,%mm2;
		movd		%mm0,28(%esp);		paddd		%mm5,%mm1;
		psrlq		$32,%mm6;		pxor		%mm7,%mm7;
		movd		%mm1,%edx;		paddd		%mm6,%mm2;		
		movl		$256,%edi;		movl		$16,%ebp;
		movd		%mm2,%ecx;
2:
		movq		(%ebx,%edi),%mm0;
		movq		(%esi,%edi),%mm1;	movq		%mm0,%mm5;
		movq		%mm1,%mm6;		punpcklbw	%mm7,%mm0;
		movq		(%eax,%edi,2),%mm2;	punpckhbw	%mm7,%mm5;
		movq		%mm2,%mm3;		punpcklbw	%mm7,%mm1;
		psubw		%mm0,%mm3;		punpckhbw	%mm7,%mm6;		
		movq		%mm2,%mm4;		paddw		%mm1,%mm0;
		movq		%mm3,768(%eax,%edi,2);	psubw		%mm1,%mm4;
		paddw		c1,%mm0;
		movq		%mm4,1536(%eax,%edi,2);
		psrlw		$1,%mm0;
		psubw		%mm0,%mm2;
		movq		%mm2,2304(%eax,%edi,2);

		movq		8(%eax,%edi,2),%mm2;
		movq		%mm2,%mm3;
		movq		%mm2,%mm4;
		psubw		%mm5,%mm3;
		movq		%mm3,768+8(%eax,%edi,2);
		psubw		%mm6,%mm4;
		movq		%mm4,1536+8(%eax,%edi,2);
		paddw		%mm6,%mm5;
		paddw		c1,%mm5;
		psrlw		$1,%mm5;
		psubw		%mm5,%mm2;
		movq		%mm2,2304+8(%eax,%edi,2);

		addl		$8,%edi;
		decl		%ebp;
		jne		2b;
	
#NO_APP
	movl 56(%esp),%eax
	movl %edx,(%eax)
	movl 60(%esp),%eax
	movl %ecx,(%eax)
	movl 28(%esp),%eax
	popl %ebx
	popl %esi
	popl %edi
	popl %ebp
	addl $28,%esp
	ret

#	.align 16
#	.type	 rwmsr,@function
#	.globl	rwmsr
#rwmsr:
#	pushl %ebp
#	movl %esp,%ebp
#	pushl %esi
#	movl $192,%eax
#	pushl %ebx
#	movl 8(%ebp),%ebx
#	movl 12(%ebp),%ecx
#	movl 16(%ebp),%edx
#	int $0x80
#	movl %eax,%esi
#	cmpl $-126,%esi
#	jbe .L312
#	call __errno_location
#	negl %esi
#	movl %esi,(%eax)
#	movl $-1,%esi
#	.align 4
#.L312:
#	movl %esi,%eax
#	leal -8(%ebp),%esp
#	popl %ebx
#	popl %esi
#	movl %ebp,%esp
#	popl %ebp
#	ret

# 3.1 motion routines

		.text
		.align		16
		.globl		mmx_sad

mmx_sad:
		movq		(%edx),%mm0;
		pushl		%ebx;
		movq		(%edx,%ecx),%mm2;
		pxor		%mm6,%mm6;
		movq		8(%edx),%mm1;
		pxor		%mm7,%mm7;
		movq		8(%edx,%ecx),%mm3;
		leal		(%edx,%ecx,2),%edx;
		movq		(%eax),%mm4;
		movl		$7,%ebx;

		.align 4
1:
		movq		%mm0,%mm5;
		psubusb		%mm4,%mm0;
		psubusb		%mm5,%mm4;
		movq		(%edx),%mm5;
		por		%mm4,%mm0;
		movq		%mm0,%mm4;
		punpcklbw	%mm6,%mm0;
		paddw		%mm0,%mm7;
		movq		8(%eax),%mm0;
		punpckhbw	%mm6,%mm4;
		paddw		%mm4,%mm7;
		movq		%mm1,%mm4;
		psubusb		%mm0,%mm1;
		psubusb		%mm4,%mm0;
		movq		(%edx,%ecx),%mm4;
		por		%mm0,%mm1;
		movq		%mm1,%mm0;
		punpcklbw	%mm6,%mm1;
		paddw		%mm1,%mm7;
		movq		16(%eax),%mm1;
		punpckhbw	%mm6,%mm0;
		paddw		%mm0,%mm7;
		movq		%mm5,%mm0;
		movq		%mm2,%mm5;
		psubusb		%mm1,%mm2;
		psubusb		%mm5,%mm1;
		movq		24(%eax),%mm5;
		addl		$32,%eax;
		por		%mm1,%mm2;
		movq		%mm2,%mm1;
		punpckhbw	%mm6,%mm1;
		paddw		%mm1,%mm7;
		movq		8(%edx),%mm1;
		punpcklbw	%mm6,%mm2;
		paddw		%mm2,%mm7;
		movq		%mm4,%mm2;
		movq		%mm3,%mm4;
		psubusb		%mm5,%mm3;
		psubusb		%mm4,%mm5;
		movq		(%eax),%mm4;
		por		%mm5,%mm3;
		movq		%mm3,%mm5;
		punpcklbw	%mm6,%mm3;
		paddw		%mm3,%mm7;
		movq		8(%edx,%ecx),%mm3;
		leal		(%edx,%ecx,2),%edx;
		punpckhbw	%mm6,%mm5;
		paddw		%mm5,%mm7;

		decl		%ebx;
		jne		1b;

		movq		%mm0,%mm5;
		psubusb		%mm4,%mm0;
		psubusb		%mm5,%mm4;
		movq		8(%eax),%mm5;
		por		%mm4,%mm0;
		movq		%mm0,%mm4;
		punpcklbw	%mm6,%mm0;
		punpckhbw	%mm6,%mm4;
		paddw		%mm0,%mm7;
		movq		16(%eax),%mm0;
		paddw		%mm4,%mm7;
		movq		%mm1,%mm4;
		psubusb		%mm5,%mm1;
		psubusb		%mm4,%mm5;
		por		%mm5,%mm1;
		movq		24(%eax),%mm4;
		movq		%mm1,%mm5;
		punpcklbw	%mm6,%mm1;
		punpckhbw	%mm6,%mm5;
		paddw		%mm1,%mm7;
		paddw		%mm5,%mm7;
		movq		%mm2,%mm5;
		psubusb		%mm0,%mm2;
		psubusb		%mm5,%mm0;
		movq		c1,%mm5;
		por		%mm0,%mm2;
		movq		%mm2,%mm0;
		punpcklbw	%mm6,%mm2;
		punpckhbw	%mm6,%mm0;
		paddw		%mm2,%mm7;
		paddw		%mm0,%mm7;
		movq		%mm3,%mm1;
		psubusb		%mm4,%mm3;
		psubusb		%mm1,%mm4;
		por		%mm4,%mm3;
		movq		%mm3,%mm4;
		punpcklbw	%mm6,%mm3;
		punpckhbw	%mm6,%mm4;
		paddw		%mm3,%mm7;
		paddw		%mm4,%mm7;
		pmaddwd		%mm5,%mm7;
		popl		%ebx;
		movq		%mm7,%mm0;
		psrlq		$32,%mm7;
		paddd		%mm0,%mm7;
		movd		%mm7,%eax;
		ret;

		.text
		.align		16
		.globl		mmx_mbsum

mmx_mbsum:
		pushl		%edi
		movl		mb_col,%edx
		sall		$4,%edx
		pushl		%esi
		addl		%edx,%eax;
		movq		c1b,%mm7;
		pushl		%ebp
		movq		%mm7,%mm6;
		pushl		%ebx
		movl		mb_address+4,%ebp
		psllq		$7,%mm7;
		movl		mb_col,%esi
		psrlw		$8-4,%mm6;
		testl		%esi,%esi
		jz		.L899

		movl		$mblock+3072,%edx
		sall		$5,%esi
		addl		mm_mbrow,%esi
		subl		$32,%esi
		movq		(%edx),%mm0;		// mblock[0][0][0][0]
		movq		8(%edx),%mm1;
		movq		256+0(%edx),%mm3;
		movq		256+8(%edx),%mm4;
		movq		-3072(%edx),%mm5;	// mblock[4][0][0][0]

		cmpl		$0,mb_row
		jne		.L900
		movl		$mblock+3088,%ebx
		movl		$7,%ecx

		.p2align 4,,7
.L904:
		paddw		(%ebx),%mm0;
		movq		%mm0,(%esi);
		paddw		8(%ebx),%mm1;
		movq		%mm1,8(%esi);
		paddw		256+0(%ebx),%mm3;
		movq		%mm3,16(%esi);
		paddw		256+8(%ebx),%mm4;
		movq		%mm4,24(%esi);
		paddw		-3072(%ebx),%mm5;
		addl		$16,%ebx
		decl		%ecx
		jne		.L904
		jmp		.L906

		.align 16
.L900:
		paddw		(%esi),%mm0;
		movq		%mm0,(%esi);
		paddw		8(%esi),%mm1;
		movq		%mm1,8(%esi);
		paddw		16(%esi),%mm3;
		movq		%mm3,16(%esi);
		paddw		24(%esi),%mm4;
		movq		%mm4,24(%esi);
		paddw		32(%esi),%mm5;

		movl		$mblock+3088,%edx
		movl		mb_col,%ebx
		sall		$5,%ebx
		addl		mm_mbrow,%ebx;
		leal		-32(%ebx,%ebp,2),%ebx;

		movl		mb_row,%edi
		sall		$4,%edi
		addl		$9-16,%edi
		imull		%ebp,%edi
		addl		%eax,%edi
		movl		$7,%ecx

		.p2align 4,,7
.L910:
		movq		(%esi),%mm0;
		paddw		2(%esi),%mm0;
		movq		%mm4,%mm2;		// dcba
		paddw		4(%esi),%mm0;
		psrlq		$16,%mm2;
		paddw		6(%esi),%mm0;
		paddw		%mm6,%mm0;
		movq		8(%esi),%mm1;
		paddw		10(%esi),%mm1;
		psrlw		$5,%mm0;
		paddw		12(%esi),%mm1;
		paddw		14(%esi),%mm1;
		paddw		%mm6,%mm1;
		psrlw		$5,%mm1;
		packuswb	%mm1,%mm0;
		pxor		%mm7,%mm0;
		movq		%mm0,-16(%edi);
		movq		18(%esi),%mm0;
		paddw		%mm3,%mm0;
		paddw		20(%esi),%mm0;
		movq		%mm4,%mm1;
		paddw		22(%esi),%mm0;
		paddw		%mm6,%mm0;
		psrlw		$5,%mm0;
		paddw		%mm2,%mm1;		// 0dcb
		psrlq		$16,%mm2;
		paddw		%mm2,%mm1;		// 00dc
		psrlq		$16,%mm2;
		paddw		%mm2,%mm1;		// 000d
		movq		%mm5,%mm2;
		psllq		$16,%mm2;
		paddw		%mm2,%mm1;		// gfe0
		psllq		$16,%mm2;
		paddw		%mm2,%mm1;		// fe00
		psllq		$16,%mm2;
		paddw		%mm2,%mm1;		// e000
		paddw		%mm6,%mm1;
		psrlw		$5,%mm1;
		packuswb	%mm1,%mm0;
		pxor		%mm7,%mm0;
		movq		%mm0,-16+8(%edi);
		addl		%ebp,%edi
		movq		(%esi),%mm0;
		psubw		(%ebx),%mm0;
		movq		8(%esi),%mm1;
		psubw		8(%ebx),%mm1;
		psubw		16(%ebx),%mm3;
		decl		%ecx
		psubw		24(%ebx),%mm4;
		psubw		32(%ebx),%mm5;
		leal		(%ebx,%ebp,2),%ebx
		paddw		(%edx),%mm0;
		movq		%mm0,(%esi);
		paddw		8(%edx),%mm1;
		movq		%mm1,8(%esi);
		paddw		256+0(%edx),%mm3;
		movq		%mm3,16(%esi);
		paddw		256+8(%edx),%mm4;
		movq		%mm4,24(%esi);
		paddw		-3072(%edx),%mm5;
		leal		16(%edx),%edx
		jne		.L910
.L906:
		movl		mb_row,%edi
		sall		$4,%edi
		imull		%ebp,%edi
		addl		%eax,%edi
		movl		$mblock+3072,%ebx
		movl		$8,%ecx

		.p2align 4,,7
.L915:
		movq		(%esi),%mm0;
		paddw		2(%esi),%mm0;
		movq		%mm4,%mm2;
		paddw		4(%esi),%mm0;
		psrlq		$16,%mm2;
		paddw		6(%esi),%mm0;
		paddw		%mm6,%mm0;
		movq		8(%esi),%mm1;
		paddw		10(%esi),%mm1;
		psrlw		$5,%mm0;
		paddw		12(%esi),%mm1;
		paddw		14(%esi),%mm1;
		paddw		%mm6,%mm1;
		psrlw		$5,%mm1;
		packuswb	%mm1,%mm0;
		pxor		%mm7,%mm0;
		movq		%mm0,-16(%edi);
		movq		18(%esi),%mm0;
		paddw		%mm3,%mm0;
		paddw		20(%esi),%mm0;
		movq		%mm4,%mm1;
		paddw		22(%esi),%mm0;
		paddw		%mm6,%mm0;
		psrlw		$5,%mm0;
		paddw		%mm2,%mm1;
		psrlq		$16,%mm2;
		paddw		%mm2,%mm1;
		psrlq		$16,%mm2;
		paddw		%mm2,%mm1;
		movq		%mm5,%mm2;
		psllq		$16,%mm2;
		paddw		%mm2,%mm1;
		psllq		$16,%mm2;
		paddw		%mm2,%mm1;
		psllq		$16,%mm2;
		paddw		%mm2,%mm1;
		paddw		%mm6,%mm1;
		psrlw		$5,%mm1;
		packuswb	%mm1,%mm0;
		pxor		%mm7,%mm0;
		movq		%mm0,-16+8(%edi);
		movq		(%esi),%mm0;
		psubw		(%ebx),%mm0;		// mblock[4][0][y+0][0]
		movq		8(%esi),%mm1;
		psubw		8(%ebx),%mm1;
		psubw		256+0(%ebx),%mm3;
		addl		%ebp,%edi
		psubw		256+8(%ebx),%mm4;
		paddw		128(%ebx),%mm0;		// mblock[4][0][y+8][0]
		movq		%mm0,(%esi);
		paddw		128+8(%ebx),%mm1;
		movq		%mm1,8(%esi);
		paddw		128+256+0(%ebx),%mm3;
		movq		%mm3,16(%esi);
		paddw		128+256+8(%ebx),%mm4;
		movq		%mm4,24(%esi);
		psubw		-3072(%ebx),%mm5;
		decl		%ecx
		paddw		-3072+128(%ebx),%mm5;	// mblock[0][0][y+8][0]
		leal		16(%ebx),%ebx

		jne		.L915

		movl		mb_row,%edi
		sall		$4,%edi
		addl		$8,%edi
		imull		%ebp,%edi
		addl		%eax,%edi
		movl		$mblock+3200,%edx

		movq		(%esi),%mm0;
		paddw		2(%esi),%mm0;
		movq		%mm4,%mm2;
		paddw		4(%esi),%mm0;
		psrlq		$16,%mm2;
		paddw		6(%esi),%mm0;
		paddw		%mm6,%mm0;
		movq		8(%esi),%mm1;
		paddw		10(%esi),%mm1;
		psrlw		$5,%mm0;
		paddw		12(%esi),%mm1;
		psllq		$16,%mm5;
		paddw		14(%esi),%mm1;
		paddw		%mm6,%mm1;
		psrlw		$5,%mm1;
		packuswb	%mm1,%mm0;
		pxor		%mm7,%mm0;
		movq		%mm0,-16(%edi);
		movq		18(%esi),%mm0;
		paddw		%mm3,%mm0;
		paddw		20(%esi),%mm0;
		movq		%mm4,%mm1;
		paddw		22(%esi),%mm0;
		paddw		%mm6,%mm0;
		psrlw		$5,%mm0;
		paddw		%mm2,%mm1;
		psrlq		$16,%mm2;
		paddw		%mm2,%mm1;
		psrlq		$16,%mm2;
		paddw		%mm2,%mm1;
		paddw		%mm5,%mm1;
		psllq		$16,%mm5;
		paddw		%mm5,%mm1;
		psllq		$16,%mm5;
		paddw		%mm5,%mm1;
		paddw		%mm6,%mm1;
		psrlw		$5,%mm1;
		packuswb	%mm1,%mm0;
		pxor		%mm7,%mm0;
		movq		%mm0,-16+8(%edi);
		movq		(%esi),%mm0;
		psubw		(%edx),%mm0;		// mblock[4][0][8+0][0]
		movq		%mm0,(%esi);
		movq		8(%esi),%mm1;
		psubw		8(%edx),%mm1;
		movq		%mm1,8(%esi);
		psubw		256+0(%edx),%mm3;
		movq		%mm3,16(%esi);
		psubw		256+8(%edx),%mm4;
		movq		%mm4,24(%esi);
.L899:
		cmpl		$21,mb_col
		jne		.L917

		movl		$mblock,%edx
		movq		(%edx),%mm2;
		movl		mm_mbrow,%esi;
		leal		-32(%esi,%ebp,2),%esi;
		movq		8(%edx),%mm3;
		movq		256+0(%edx),%mm4;
		movq		256+8(%edx),%mm5;
		movl		mb_row,%edi
		testl		%edi,%edi
		jne		.L918

		movl		$mblock+16,%edx
		movl		$7,%ecx

		.p2align 4,,7
.L922:
		paddw		(%edx),%mm2;
		movq		%mm2,(%esi);
		paddw		8(%edx),%mm3;
		movq		%mm3,8(%esi);
		paddw		256+0(%edx),%mm4;
		movq		%mm4,16(%esi);
		paddw		256+8(%edx),%mm5;
		addl		$16,%edx
		movq		%mm5,24(%esi);
		decl		%ecx
		jne		.L922
		jmp		.L924

		.p2align 4,,7
.L918:
		paddw		(%esi),%mm2;
		movq		%mm2,(%esi);
		paddw		8(%esi),%mm3;
		movq		%mm3,8(%esi);
		paddw		16(%esi),%mm4;
		movq		%mm4,16(%esi);
		paddw		24(%esi),%mm5;
		movq		%mm5,24(%esi);

		sall		$4,%edi
		addl		$9-16,%edi
		imull		%ebp,%edi
		movl		mb_col,%edx
		addl		%eax,%edi
		sall		$5,%edx
		addl		mm_mbrow,%edx
		leal		(%edx,%ebp,2),%edx;

		movl		$mblock+16,%ebx
		movl		$7,%ecx

		.p2align 4,,7
.L928:
		movq		(%esi),%mm0;
		paddw		2(%esi),%mm0;
		movq		%mm5,%mm2;
		paddw		4(%esi),%mm0;
		psrlq		$16,%mm2;
		paddw		6(%esi),%mm0;
		paddw		%mm6,%mm0;
		movq		10(%esi),%mm1;
		paddw		%mm3,%mm1;
		paddw		12(%esi),%mm1;
		psrlw		$5,%mm0;
		paddw		14(%esi),%mm1;
		paddw		%mm6,%mm1;
		psrlw		$5,%mm1;
		packuswb	%mm1,%mm0;
		pxor		%mm7,%mm0;
		movq		%mm0,(%edi);
		movq		18(%esi),%mm0;
		paddw		%mm4,%mm0;
		paddw		20(%esi),%mm0;
		movq		%mm5,%mm1;
		paddw		22(%esi),%mm0;
		paddw		%mm6,%mm0;
		psrlw		$5,%mm0;
		paddw		%mm2,%mm1;
		psrlq		$16,%mm2;
		paddw		%mm2,%mm1;
		psrlq		$16,%mm2;
		paddw		%mm2,%mm1;
		paddw		%mm6,%mm1;
		psrlw		$5,%mm1;
		packuswb	%mm1,%mm0;
		pxor		%mm7,%mm0;
		movq		%mm0,8(%edi);
		movq		(%esi),%mm0;
		psubw		(%edx),%mm0;
		psubw		8(%edx),%mm3;
		psubw		16(%edx),%mm4;
		addl		%ebp,%edi
		psubw		24(%edx),%mm5;
		leal		(%edx,%ebp,2),%edx
		paddw		(%ebx),%mm0;
		movq		%mm0,(%esi);
		paddw		8(%ebx),%mm3;
		movq		%mm3,8(%esi);
		paddw		256+0(%ebx),%mm4;
		movq		%mm4,16(%esi);
		paddw		256+8(%ebx),%mm5;
		addl		$16,%ebx
		movq		%mm5,24(%esi);
		decl		%ecx
		jne		.L928
.L924:
		movl		mb_row,%edi
		sall		$4,%edi
		imull		%ebp,%edi
		addl		%eax,%edi
		movl		$mblock,%ebx
		movl		$8,%ecx

		.p2align 4,,7
.L933:
		movq		(%esi),%mm0;
		paddw		2(%esi),%mm0;
		movq		%mm5,%mm2;
		paddw		4(%esi),%mm0;
		psrlq		$16,%mm2;
		paddw		6(%esi),%mm0;
		paddw		%mm6,%mm0;
		movq		10(%esi),%mm1;
		paddw		%mm3,%mm1;
		paddw		12(%esi),%mm1;
		psrlw		$5,%mm0;
		paddw		14(%esi),%mm1;
		paddw		%mm6,%mm1;
		psrlw		$5,%mm1;
		packuswb	%mm1,%mm0;
		pxor		%mm7,%mm0;
		movq		%mm0,(%edi);
		movq		18(%esi),%mm0;
		paddw		%mm4,%mm0;
		paddw		20(%esi),%mm0;
		movq		%mm5,%mm1;
		paddw		22(%esi),%mm0;
		paddw		%mm6,%mm0;
		psrlw		$5,%mm0;
		paddw		%mm2,%mm1;
		psrlq		$16,%mm2;
		paddw		%mm2,%mm1;
		psrlq		$16,%mm2;
		paddw		%mm2,%mm1;
		paddw		%mm6,%mm1;
		psrlw		$5,%mm1;
		packuswb	%mm1,%mm0;
		pxor		%mm7,%mm0;
		movq		%mm0,8(%edi);
		movq		(%esi),%mm0;
		psubw		(%ebx),%mm0;		// mblock[0][0][y+0][0]
		psubw		8(%ebx),%mm3;
		psubw		256+0(%ebx),%mm4;
		addl		mb_address+4,%edi
		psubw		256+8(%ebx),%mm5;
		paddw		128(%ebx),%mm0;		// mblock[0][0][y+8][0]
		movq		%mm0,(%esi);
		paddw		128+8(%ebx),%mm3;
		movq		%mm3,8(%esi);
		paddw		128+256+0(%ebx),%mm4;
		movq		%mm4,16(%esi);
		paddw		128+256+8(%ebx),%mm5;
		addl		$16,%ebx
		movq		%mm5,24(%esi);
		decl		%ecx
		jne		.L933

		movq		(%esi),%mm0;
		paddw		2(%esi),%mm0;
		movq		%mm5,%mm2;
		paddw		4(%esi),%mm0;
		psrlq		$16,%mm2;
		paddw		6(%esi),%mm0;
		paddw		%mm6,%mm0;
		movq		10(%esi),%mm1;
		paddw		%mm3,%mm1;
		paddw		12(%esi),%mm1;
		psrlw		$5,%mm0;
		paddw		14(%esi),%mm1;
		paddw		%mm6,%mm1;
		psrlw		$5,%mm1;
		packuswb	%mm1,%mm0;
		pxor		%mm7,%mm0;
		movq		%mm0,(%edi);
		movq		18(%esi),%mm0;
		paddw		%mm4,%mm0;
		paddw		20(%esi),%mm0;
		movq		%mm5,%mm1;
		paddw		22(%esi),%mm0;
		paddw		%mm6,%mm0;
		psrlw		$5,%mm0;
		paddw		%mm2,%mm1;
		psrlq		$16,%mm2;
		paddw		%mm2,%mm1;
		psrlq		$16,%mm2;
		paddw		%mm2,%mm1;
		paddw		%mm6,%mm1;
		psrlw		$5,%mm1;
		packuswb	%mm1,%mm0;
		pxor		%mm7,%mm0;
		movq		%mm0,8(%edi);
		movq		(%esi),%mm0;
		movl		$mblock+144,%edx
		psubw		(%ebx),%mm0;
		movq		%mm0,(%esi);
		psubw		8(%ebx),%mm3;
		movq		%mm3,8(%esi);
		psubw		256+0(%ebx),%mm4;
		movq		%mm4,16(%esi);
		movl		$7,%ecx
		psubw		256+8(%ebx),%mm5;
		movq		%mm5,24(%esi);

		movl		mb_col,%ebx
		sall		$5,%ebx
		addl		mm_mbrow,%ebx;
		leal		(%ebx,%ebp,2),%ebx;

		.p2align 4,,7
.L938:
		movq		(%edx),%mm0;
		movq		%mm0,(%ebx);
		movq		8(%edx),%mm1;
		movq		%mm1,8(%ebx);
		movq		256+0(%edx),%mm0;
		movq		%mm0,16(%ebx);
		movq		256+8(%edx),%mm1;
		addl		$16,%edx
		movq		%mm1,24(%ebx);
		leal		(%ebx,%ebp,2),%ebx;
		decl		%ecx
		jne		.L938
.L917:
		movl		mb_col,%ebx
		testl		%ebx,%ebx
		jz		.L940

		movl		$mblock+3216,%edx
		sall		$5,%ebx
		addl		mm_mbrow,%ebx
		leal		-32(%ebx,%ebp,2),%ebx;
		movl		$7,%ecx

		.p2align 4,,7
.L944:
		movq		(%edx),%mm0;
		movq		%mm0,(%ebx);
		movq		8(%edx),%mm1;
		movq		%mm1,8(%ebx);
		movq		256+0(%edx),%mm0;
		movq		%mm0,16(%ebx);
		movq		256+8(%edx),%mm1;
		addl		$16,%edx
		movq		%mm1,24(%ebx);
		leal		(%ebx,%ebp,2),%ebx
		decl		%ecx
		jne		.L944
.L940:
		movl		$mblock,%esi
		movl		$mblock+3072,%edi
		movl		$16,%ecx

		.p2align 4,,7
.L946:
		movq		(%esi),%mm0;
		movq		%mm0,(%edi);
		movq		8(%esi),%mm0;
		movq		%mm0,8(%edi);
		movq		16(%esi),%mm0;
		movq		%mm0,16(%edi);
		movq		24(%esi),%mm0;
		movq		%mm0,24(%edi);
		addl		$32,%esi;
		addl		$32,%edi;
		decl		%ecx;
		jne		.L946

//		movl		$512,%ecx
//		cld
//		rep movsb	// WA or WT?

		popl %ebx
		popl %ebp
		popl %esi
		popl %edi
		ret

		.text
		.align		16
		.globl		sse_sad
sse_sad:
		movq		(%edx),%mm0;
		movq		8(%edx),%mm1;
		pxor		%mm7,%mm7;
		movq		(%edx,%ecx),%mm2;
		movq		8(%edx,%ecx),%mm3;
		leal		(%edx,%ecx,2),%edx;
		psadbw		(%eax),%mm0;
		paddw		%mm0,%mm7;
		movq		(%edx),%mm0;
		psadbw		8(%eax),%mm1;
		paddw		%mm1,%mm7;
		movq		8(%edx),%mm1;
		psadbw		16(%eax),%mm2;
		paddw		%mm2,%mm7;
		movq		(%edx,%ecx),%mm2;
		psadbw		24(%eax),%mm3;
		paddw		%mm3,%mm7;
		movq		8(%edx,%ecx),%mm3;
		leal		(%edx,%ecx,2),%edx;
		psadbw		1*32(%eax),%mm0;
		paddw		%mm0,%mm7;
		movq		(%edx),%mm0;
		psadbw		1*32+8(%eax),%mm1;
		paddw		%mm1,%mm7;
		movq		8(%edx),%mm1;
		psadbw		1*32+16(%eax),%mm2;
		paddw		%mm2,%mm7;
		movq		(%edx,%ecx),%mm2;
		psadbw		1*32+24(%eax),%mm3;
		paddw		%mm3,%mm7;
		movq		8(%edx,%ecx),%mm3;
		leal		(%edx,%ecx,2),%edx;
		psadbw		2*32(%eax),%mm0;
		paddw		%mm0,%mm7;
		movq		(%edx),%mm0;
		psadbw		2*32+8(%eax),%mm1;
		paddw		%mm1,%mm7;
		movq		8(%edx),%mm1;
		psadbw		2*32+16(%eax),%mm2;
		paddw		%mm2,%mm7;
		movq		(%edx,%ecx),%mm2;
		psadbw		2*32+24(%eax),%mm3;
		paddw		%mm3,%mm7;
		movq		8(%edx,%ecx),%mm3;
		leal		(%edx,%ecx,2),%edx;
		psadbw		3*32(%eax),%mm0;
		paddw		%mm0,%mm7;
		movq		(%edx),%mm0;
		psadbw		3*32+8(%eax),%mm1;
		paddw		%mm1,%mm7;
		movq		8(%edx),%mm1;
		psadbw		3*32+16(%eax),%mm2;
		paddw		%mm2,%mm7;
		movq		(%edx,%ecx),%mm2;
		psadbw		3*32+24(%eax),%mm3;
		paddw		%mm3,%mm7;
		movq		8(%edx,%ecx),%mm3;
		leal		(%edx,%ecx,2),%edx;
		psadbw		4*32(%eax),%mm0;
		paddw		%mm0,%mm7;
		movq		(%edx),%mm0;
		psadbw		4*32+8(%eax),%mm1;
		paddw		%mm1,%mm7;
		movq		8(%edx),%mm1;
		psadbw		4*32+16(%eax),%mm2;
		paddw		%mm2,%mm7;
		movq		(%edx,%ecx),%mm2;
		psadbw		4*32+24(%eax),%mm3;
		paddw		%mm3,%mm7;
		movq		8(%edx,%ecx),%mm3;
		leal		(%edx,%ecx,2),%edx;
		psadbw		5*32(%eax),%mm0;
		paddw		%mm0,%mm7;
		movq		(%edx),%mm0;
		psadbw		5*32+8(%eax),%mm1;
		paddw		%mm1,%mm7;
		movq		8(%edx),%mm1;
		psadbw		5*32+16(%eax),%mm2;
		paddw		%mm2,%mm7;
		movq		(%edx,%ecx),%mm2;
		psadbw		5*32+24(%eax),%mm3;
		paddw		%mm3,%mm7;
		movq		8(%edx,%ecx),%mm3;
		leal		(%edx,%ecx,2),%edx;
		psadbw		6*32(%eax),%mm0;
		paddw		%mm0,%mm7;
		movq		(%edx),%mm0;
		psadbw		6*32+8(%eax),%mm1;
		paddw		%mm1,%mm7;
		movq		8(%edx),%mm1;
		psadbw		6*32+16(%eax),%mm2;
		paddw		%mm2,%mm7;
		movq		(%edx,%ecx),%mm2;
		psadbw		6*32+24(%eax),%mm3;
		paddw		%mm3,%mm7;
		movq		8(%edx,%ecx),%mm3;
		leal		(%edx,%ecx,2),%edx;
		psadbw		7*32(%eax),%mm0;
		paddw		%mm0,%mm7;
		psadbw		7*32+8(%eax),%mm1;
		paddw		%mm1,%mm7;
		psadbw		7*32+16(%eax),%mm2;
		paddw		%mm2,%mm7;
		psadbw		7*32+24(%eax),%mm3;
		paddw		%mm3,%mm7;
		movd		%mm7,%eax;
		ret;
