#
#  MPEG-1 Real Time Encoder
# 
#  Copyright (C) 1999-2000 Michael H. Schimek
# 
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 2 as
#  published by the Free Software Foundation.
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

# $Id: filter_mmx.s,v 1.2 2001-08-22 01:28:10 mschimek Exp $

# int
# mmx_YUV_420(unsigned char *buffer, unsigned char *unused)

	.text
	.align		16
	.globl		mmx_YUV_420

mmx_YUV_420:

	leal		-20(%esp),%esp;
	movl		%edi,16(%esp);
	movl		1*4+20(%esp),%edi;	// buffer
	movl		%edx,12(%esp);
	movl		filter_y_pitch,%edx;
	movl		%esi,8(%esp);
	movl		mb_row,%esi;
	movl		%ebx,4(%esp);
	movl		mb_col,%ebx;
	movl		%ecx,0(%esp);
	imull		%edx,%esi;		// row = filter_y_pitch * mb_row
	sall		$2,%esi;
	leal		(%esi,%ebx,8),%eax;
	leal		(%esi,%ebx,4),%esi;
	addl		%edi,%eax;		// chroma = buffer + row * 4 + mb_col * 8
	leal		(%edi,%esi,4),%esi;
	movl		%eax,%ebx;
	addl		filter_u_offs,%eax;	// filter_u_offs + chroma
	addl		filter_v_offs,%ebx;	// filter_v_offs + chroma
	addl		filter_y_offs,%esi;	// filter_y_offs + buffer + row * 16 + mb_col * 16
	movl		$mblock,%edi;

	/* Cb, Cr */

	movq		(%eax),%mm0;			shrl		$1,%edx;
	movq		(%ebx),%mm1;			pxor		%mm7,%mm7;
	movq		(%eax,%edx),%mm4;		movq		%mm0,%mm2;
	movq		(%ebx,%edx),%mm5;		punpcklbw	%mm7,%mm0;
	movq		(%ebx,%edx,2),%mm6;		punpckhbw	%mm7,%mm2;
	movq		%mm0,512+0*16+0(%edi);		movq		%mm1,%mm3;
	movq		(%eax,%edx,2),%mm0;		punpcklbw	%mm7,%mm1;
	movq		%mm2,512+0*16+8(%edi);		punpckhbw	%mm7,%mm3;
	movq		%mm1,640+0*16+0(%edi);		movq		%mm4,%mm2;
	lea		(%ebx,%edx,2),%ebx;		punpcklbw	%mm7,%mm4;
	movq		%mm3,640+0*16+8(%edi);		punpckhbw	%mm7,%mm2;
	lea		(%eax,%edx,2),%eax;		movq		%mm5,%mm3;
	movq		%mm4,512+1*16+0(%edi);		punpcklbw	%mm7,%mm5;
	movq		(%eax,%edx),%mm4;		punpckhbw	%mm7,%mm3;
	movq		%mm2,512+1*16+8(%edi);		lea		(%eax,%edx,2),%eax;
	movq		(%ebx,%edx),%mm2;		lea		(%ebx,%edx,2),%ebx;
	movq		%mm5,640+1*16+0(%edi);		movq		%mm0,%mm5;
	movq		%mm3,640+1*16+8(%edi);		punpcklbw	%mm7,%mm0;
	movq		(%eax),%mm1;			punpckhbw	%mm7,%mm5;
	movq		%mm0,512+2*16+0(%edi);		movq		%mm6,%mm3;
	movq		(%ebx),%mm0;			punpcklbw	%mm7,%mm6;
	movq		%mm5,512+2*16+8(%edi);		punpckhbw	%mm7,%mm3;
	movq		%mm6,640+2*16+0(%edi);		movq		%mm4,%mm5;
	movq		(%eax,%edx),%mm6;		punpcklbw	%mm7,%mm4;
	movq		%mm3,640+2*16+8(%edi);		punpckhbw	%mm7,%mm5;
	movq		%mm4,512+3*16+0(%edi);		movq		%mm2,%mm3;
	movq		(%ebx,%edx),%mm4;		punpcklbw	%mm7,%mm2;
	movq		%mm5,512+3*16+8(%edi);		punpckhbw	%mm7,%mm3;
	movq		%mm2,640+3*16+0(%edi);		movq		%mm1,%mm2;
	movq		%mm3,640+3*16+8(%edi);		punpcklbw	%mm7,%mm1;
	movq		(%eax,%edx,2),%mm3;		punpckhbw	%mm7,%mm2;
	movq		%mm1,512+4*16+0(%edi);		lea		(%eax,%edx,2),%eax;
	movq		(%ebx,%edx,2),%mm1;		movq		%mm0,%mm5;
	lea		(%ebx,%edx,2),%ebx;		punpcklbw	%mm7,%mm0;
	movq		%mm2,512+4*16+8(%edi);		punpckhbw	%mm7,%mm5;
	movq		%mm0,640+4*16+0(%edi);		movq		%mm6,%mm2;
	movq		(%eax,%edx),%mm0;		punpcklbw	%mm7,%mm6;
	movq		%mm5,640+4*16+8(%edi);		punpckhbw	%mm7,%mm2;
	movq		%mm6,512+5*16+0(%edi);		movq		%mm4,%mm6;
	movq		(%ebx,%edx),%mm5;		punpcklbw	%mm7,%mm4;
	movq		%mm2,512+5*16+8(%edi);		punpckhbw	%mm7,%mm6;
	movq		%mm3,%mm2;			punpcklbw	%mm7,%mm3;
	movq		%mm4,640+5*16+0(%edi);		punpckhbw	%mm7,%mm2;
	movq		%mm1,%mm4;			punpcklbw	%mm7,%mm1;
	movq		%mm6,640+5*16+8(%edi);		punpckhbw	%mm7,%mm4;
	movq		%mm3,512+6*16+0(%edi);		movq		%mm0,%mm3;
	movq		%mm2,512+6*16+8(%edi);		punpcklbw	%mm7,%mm0;
	movq		%mm1,640+6*16+0(%edi);		punpckhbw	%mm7,%mm3;
	movq		(%esi,%edx,2),%mm1;		movq		%mm5,%mm2;
	movq		%mm0,512+7*16+0(%edi);		punpcklbw	%mm7,%mm5;
	movq		(%esi),%mm0;			punpckhbw	%mm7,%mm2;
	movq		%mm3,512+7*16+8(%edi);		movl		%esi,%eax;
	movl		%esi,%ebx;			movl		$7,%ecx;
	movq		%mm4,640+6*16+8(%edi);		movq		%mm7,%mm6;
	movq		%mm5,640+7*16+0(%edi);		movq		%mm7,%mm5;
	movq		%mm2,640+7*16+8(%edi);		shll		$1,%edx;

	/* Y left 8 x 16 */
1:
	movq		%mm0,%mm2;			punpcklbw	%mm5,%mm0;
	paddw		%mm0,%mm6;			punpckhbw	%mm5,%mm2;
	paddw		%mm2,%mm6;			movq		%mm0,%mm3;
	movq		%mm2,%mm4;			pmaddwd		%mm3,%mm3;
	movq		%mm0,(%edi);			lea		(%eax,%edx,2),%eax;
	movq		(%eax),%mm0;			pmaddwd		%mm4,%mm4;
	movq		%mm2,8(%edi);			paddd		%mm3,%mm7;
	movq		%mm1,%mm2;			addl		$32,%edi;
	paddd		%mm4,%mm7;			punpcklbw	%mm5,%mm1;
	paddw		%mm1,%mm6;			punpckhbw	%mm5,%mm2;
	paddw		%mm2,%mm6;			movq		%mm1,%mm3;
	movq		%mm2,%mm4;			pmaddwd		%mm3,%mm3;
	movq		%mm1,-16(%edi);			pmaddwd		%mm4,%mm4;
	movq		(%eax,%edx),%mm1;		decl		%ecx;
	movq		%mm2,-8(%edi);			paddd		%mm3,%mm7;
	paddd		%mm4,%mm7;			jne		1b;

	movl		%esi,%eax;			movq		%mm0,%mm2;
	addl		$8,%eax;			punpcklbw	%mm5,%mm0;
	paddw		%mm0,%mm6;			punpckhbw	%mm5,%mm2;
	paddw		%mm2,%mm6;			movq		%mm0,%mm3;
	movq		%mm2,%mm4;			pmaddwd		%mm3,%mm3;
	movq		%mm0,(%edi);			pmaddwd		%mm4,%mm4;
	movq		(%eax),%mm0;			paddd		%mm3,%mm7;
	movq		%mm2,8(%edi);			movq		%mm1,%mm2;
	paddd		%mm4,%mm7;			punpcklbw	%mm5,%mm1;
	paddw		%mm1,%mm6;			punpckhbw	%mm5,%mm2;
	paddw		%mm2,%mm6;			movq		%mm1,%mm3;
	movq		%mm2,%mm4;			pmaddwd		%mm3,%mm3;
	movq		%mm1,16(%edi);			pmaddwd		%mm4,%mm4;
	movq		%mm2,24(%edi);			addl		$32,%edi
	paddd		%mm3,%mm7;			movl		$7,%ecx;
	movq		(%eax,%edx),%mm1;		paddd		%mm4,%mm7;

	/* Y right 8 x 16 */
2:
	movq		%mm0,%mm2;			punpcklbw	%mm5,%mm0;
	paddw		%mm0,%mm6;			punpckhbw	%mm5,%mm2;
	paddw		%mm2,%mm6;			movq		%mm0,%mm3;
	movq		%mm2,%mm4;			pmaddwd		%mm3,%mm3;
	movq		%mm0,(%edi);			lea		(%eax,%edx,2),%eax;
	movq		(%eax),%mm0;			pmaddwd		%mm4,%mm4;
	movq		%mm2,8(%edi);			paddd		%mm3,%mm7;
	movq		%mm1,%mm2;			addl		$32,%edi;
	paddd		%mm4,%mm7;			punpcklbw	%mm5,%mm1;
	paddw		%mm1,%mm6;			punpckhbw	%mm5,%mm2;
	paddw		%mm2,%mm6;			movq		%mm1,%mm3;
	movq		%mm2,%mm4;			pmaddwd		%mm3,%mm3;
	movq		%mm1,-16(%edi);			pmaddwd		%mm4,%mm4;
	movq		(%eax,%edx),%mm1;		decl		%ecx;
	movq		%mm2,-8(%edi);			paddd		%mm3,%mm7;
	paddd		%mm4,%mm7;			jne		2b;

	movq		%mm0,%mm2;			punpcklbw	%mm5,%mm0;
	paddw		%mm0,%mm6;			punpckhbw	%mm5,%mm2;
	paddw		%mm2,%mm6;			movq		%mm0,%mm3;
	movq		%mm2,%mm4;			pmaddwd		%mm3,%mm3;
	movq		%mm0,(%edi);			pmaddwd		%mm4,%mm4;
	movq		%mm2,8(%edi);			paddd		%mm3,%mm7;
	movq		%mm1,%mm2;			punpcklbw	%mm5,%mm1;
	paddd		%mm4,%mm7;			punpckhbw	%mm5,%mm2;
	paddw		%mm1,%mm6;			movq		%mm1,%mm3;
	paddw		%mm2,%mm6;			pmaddwd		%mm3,%mm3;
	movq		%mm2,%mm4;			paddd		%mm3,%mm7;
	movq		%mm1,16(%edi);			pmaddwd		%mm4,%mm4;
	movq		%mm2,24(%edi);			movq		%mm6,%mm2;
	paddd		%mm4,%mm7;			psllq		$32,%mm6;
	paddw		%mm2,%mm6;			movq		%mm7,%mm4;
	movq		%mm6,%mm3;			pslld		$16,%mm6;
	paddw		%mm3,%mm6;			psrlq		$32,%mm7;
	paddd		%mm4,%mm7;			psrlq		$48,%mm6;
	movd		%mm6,%eax;			pslld		$8,%mm7;
	popl		%ecx;				mull		%eax;
	popl		%ebx;				movd		%mm7,%edi;
	popl		%esi;				subl		%edi,%eax;
	popl		%edx;				negl		%eax;
	popl		%edi;
	ret

# int
# mmx_YUYV_422(unsigned char *buffer, unsigned char *unused)

	.text
	.align		16
	.globl		mmx_YUYV_422

mmx_YUYV_422:

	leal		-16(%esp),%esp;
	movl		%ecx,12(%esp);
	movl		mb_col,%eax;		// + mb_col * 16 * 2
	movl		%edx,8(%esp);
	sall		$5,%eax;
	movl		filter_y_pitch,%edx;
	movl		%ebx,4(%esp);
	imull		mb_row,%edx;		// + mb_row * 16 * filter_y_pitch
	movl		%edi,(%esp);
	pxor		%mm6,%mm6;
	sall		$4,%edx;
	movq		c255,%mm5;
	addl		%edx,%eax;
	addl		filter_y_offs,%eax;	// + filter_y_offs
	pxor		%mm7,%mm7;
	addl		1*4+16(%esp),%eax;	// + buffer
	movq		(%eax),%mm0;
	movl		$mblock+512,%ebx;	// mblock[0][4] (chroma)
	movl		$mblock,%edi;
	movl		$7,%ecx;
	movl		filter_y_pitch,%edx;
1:
	movq		8(%eax),%mm4;			movq		%mm0,%mm3;
	movq		%mm0,%mm1;			punpcklwd	%mm4,%mm3;
	movq		%mm3,%mm2;			punpckhwd	%mm4,%mm1;
	pand		%mm5,%mm0;			punpcklwd	%mm1,%mm3;
	pand		%mm5,%mm4;			punpckhwd	%mm1,%mm2;
	movq		%mm0,(%edi);			paddw		%mm0,%mm6;
	psrlw		$8,%mm3;			pmaddwd		%mm0,%mm0;
	movq		%mm4,8(%edi);			paddw		%mm4,%mm6;
	psrlw		$8,%mm2;			pmaddwd		%mm4,%mm4;
	movq		%mm3,(%ebx);			paddd		%mm0,%mm7;
	movq		16(%eax),%mm0;			leal		32(%edi),%edi;
	movq		%mm2,128+0(%ebx);		paddd		%mm4,%mm7;
	movq		24(%eax),%mm4;			movq		%mm0,%mm3;
	movq		%mm0,%mm1;			punpcklwd	%mm4,%mm3;
	decl		%ecx;				punpckhwd	%mm4,%mm1;
	movq		%mm3,%mm2;			punpcklwd	%mm1,%mm3;
	pand		%mm5,%mm0;			punpckhwd	%mm1,%mm2;
	movq		(%eax,%edx),%mm1;		pand		%mm5,%mm4;
	movq		%mm0,256-32(%edi);		paddw		%mm0,%mm6;
	psrlw		$8,%mm3;			pmaddwd		%mm0,%mm0;
	movq		%mm4,256+8-32(%edi);		paddw		%mm4,%mm6;
	psrlw		$8,%mm2;			pmaddwd		%mm4,%mm4;
	movq		%mm3,8(%ebx);			paddd		%mm0,%mm7;
	movq		(%eax,%edx,2),%mm0;		pand		%mm5,%mm1;
	movq		%mm2,128+8(%ebx);		paddd		%mm4,%mm7;
	movq		8(%eax,%edx),%mm2;		paddw		%mm1,%mm6;
	movq		16(%eax,%edx),%mm3;		pand		%mm5,%mm2;
	movq		%mm1,16-32(%edi);		paddw		%mm2,%mm6;
	movq		24(%eax,%edx),%mm4;		pmaddwd		%mm1,%mm1;
	leal		(%eax,%edx,2),%eax;		pand		%mm5,%mm3;
	movq		%mm2,24-32(%edi);		pmaddwd		%mm2,%mm2;
	pand		%mm5,%mm4;			paddw		%mm3,%mm6;
	paddd		%mm1,%mm7;			paddw		%mm4,%mm6;
	movq		%mm4,%mm1;			pmaddwd		%mm4,%mm4;
	movq		%mm3,256+16-32(%edi);		pmaddwd		%mm3,%mm3;
	leal		16(%ebx),%ebx;			paddd		%mm2,%mm7;
	movq		%mm1,256+24-32(%edi);		paddd		%mm4,%mm7;
	paddd		%mm3,%mm7;			jne		1b

	movq		8(%eax),%mm4;			movq		%mm0,%mm3;
	movq		%mm0,%mm1;			punpcklwd	%mm4,%mm3;
	movq		%mm3,%mm2;			punpckhwd	%mm4,%mm1;
	pand		%mm5,%mm0;			punpcklwd	%mm1,%mm3;
	pand		%mm5,%mm4;			punpckhwd	%mm1,%mm2;
	movq		%mm0,(%edi);			paddw		%mm0,%mm6;
	psrlw		$8,%mm3;			pmaddwd		%mm0,%mm0;
	movq		%mm4,8(%edi);			paddw		%mm4,%mm6;
	psrlw		$8,%mm2;			pmaddwd		%mm4,%mm4;
	movq		%mm3,(%ebx);			paddd		%mm0,%mm7;
	movq		16(%eax),%mm0;			leal		32(%edi),%edi;
	movq		%mm2,128+0(%ebx);		paddd		%mm4,%mm7;
	movq		24(%eax),%mm4;			movq		%mm0,%mm3;
	movq		%mm0,%mm1;			punpcklwd	%mm4,%mm3;
	decl		%ecx;				punpckhwd	%mm4,%mm1;
	movq		%mm3,%mm2;			punpcklwd	%mm1,%mm3;
	pand		%mm5,%mm0;			punpckhwd	%mm1,%mm2;
	movq		(%eax,%edx),%mm1;		pand		%mm5,%mm4;
	movq		%mm0,256-32(%edi);		paddw		%mm0,%mm6;
	psrlw		$8,%mm3;			pmaddwd		%mm0,%mm0;
	movq		%mm4,256+8-32(%edi);		paddw		%mm4,%mm6;
	psrlw		$8,%mm2;			pmaddwd		%mm4,%mm4;
	movq		%mm3,8(%ebx);			paddd		%mm0,%mm7;
	movq		(%eax,%edx,2),%mm0;		pand		%mm5,%mm1;
	movq		%mm2,128+8(%ebx);		paddd		%mm4,%mm7;
	movq		8(%eax,%edx),%mm2;		paddw		%mm1,%mm6;
	movq		16(%eax,%edx),%mm3;		pand		%mm5,%mm2;
	movq		%mm1,16-32(%edi);		paddw		%mm2,%mm6;
	movq		24(%eax,%edx),%mm4;		pmaddwd		%mm1,%mm1;
	movq		%mm2,24-32(%edi);		pand		%mm5,%mm3;
	paddw		%mm3,%mm6;			pmaddwd		%mm2,%mm2;
	paddd		%mm1,%mm7;			pand		%mm5,%mm4;
	movq		%mm3,256+16-32(%edi);		paddw		%mm4,%mm6;
	paddd		%mm2,%mm7;			pmaddwd		%mm3,%mm3;
	movq		%mm4,256+24-32(%edi);		pmaddwd		%mm4,%mm4;
	movq		%mm6,%mm2;			psllq		$32,%mm6;
	paddd		%mm3,%mm7;			paddw		%mm2,%mm6;
	paddd		%mm4,%mm7;			movq		%mm6,%mm3;
	movq		%mm7,%mm5;			psrlq		$32,%mm7;
	paddd		%mm5,%mm7;			pslld		$16,%mm6;
	paddw		%mm3,%mm6;			pslld		$8,%mm7;
	movd		%mm7,%ecx;			psrlq		$48,%mm6;
	movd		%mm6,%eax;
	popl		%edi;
	mull		%eax;
	popl		%ebx;
	subl		%ecx,%eax;
	popl		%edx;
	negl		%eax;
	popl		%ecx;
	ret

# int
# mmx_YUYV_422_2v(unsigned char *buffer, unsigned char *unused)

	.text
	.align		16
	.globl		mmx_YUYV_422_2v

mmx_YUYV_422_2v:

	leal		-20(%esp),%esp;
	movl		%edx,16(%esp);
	movl		filter_y_pitch,%edx;
	movl		%esi,12(%esp);
	movl		mb_row,%esi;
	movl		%ebx,8(%esp);
	sall		$5,%esi;
	movl		%ecx,4(%esp);
	imull		%edx,%esi;
	movl		mb_col,%eax;
	movl		%edi,(%esp);
	sall		$5,%eax;
	addl		filter_y_offs,%esi;
	addl		%eax,%esi;
	addl		1*4+20(%esp),%esi;	// s1 = buffer + filter_y_pitch * mb_row * 32 + mb_col * 32 + filter_y_offs 
	leal		(%esi,%edx),%eax;	// s2 = s1 + filter_y_pitch
	sall		$1,%edx;		// filter_y_pitch * 2

	.align 16

filter_s2t:

	movl		$mblock,%edi;			movl		$8,%ecx;
	movl		$mblock+512,%ebx;		pxor		%mm6,%mm6;
	movq		c255,%mm5;			pxor		%mm7,%mm7;
1:	
	movq		(%esi),%mm0;			leal		16(%ebx),%ebx;
	movq		(%eax),%mm1;			movq		%mm0,%mm2;
	pand		%mm5,%mm0;			movq		%mm1,%mm4;
	paddw		c1,%mm0;			pand		%mm5,%mm1;
	movq		(%eax,%edx),%mm3;		paddw		%mm0,%mm1;
	movq		(%esi,%edx),%mm0;		psrlw		$1,%mm1;
	paddw		%mm1,%mm6;			psrlw		$8,%mm2;
	movq		%mm1,(%edi);			pmaddwd		%mm1,%mm1;
	decl		%ecx;				psrlw		$8,%mm4;
	paddw		%mm2,%mm4;			movq		%mm0,%mm2;
	paddd		%mm1,%mm7;			pand		%mm5,%mm0;
	paddw		c1,%mm0;			movq		%mm3,%mm1;
	pand		%mm5,%mm1;			psrlw		$8,%mm2;
	paddw		%mm0,%mm1;			psrlw		$8,%mm3;
	movq		8(%esi),%mm0;			psrlw		$1,%mm1;
	paddw		%mm1,%mm6;			paddw		%mm2,%mm4;
	movq		%mm1,16(%edi);			pmaddwd		%mm1,%mm1;
	movq		8(%eax),%mm5;			movq		%mm0,%mm2;
	paddw		%mm3,%mm4;			movq		%mm5,%mm3;
	pand		c255,%mm0;			paddd		%mm1,%mm7;
	paddw		c1,%mm0;			psrlw		$8,%mm2;
	pand		c255,%mm3;			psrlw		$8,%mm5;
	paddw		%mm0,%mm3;			paddw		%mm2,%mm5;
	movq		8(%esi,%edx),%mm0;		psrlw		$1,%mm3;
	paddw		%mm3,%mm6;			movq		%mm0,%mm2;
	movq		%mm3,8(%edi);			pmaddwd		%mm3,%mm3;
	pand		c255,%mm0;			psrlw		$8,%mm2;
	movq		8(%eax,%edx),%mm1;		paddw		%mm2,%mm5;
	paddd		%mm3,%mm7;			movq		%mm1,%mm3;
	pand		c255,%mm1;			psrlw		$8,%mm3;
	paddw		%mm0,%mm1;			paddw		%mm3,%mm5;
	paddw		c1,%mm1;			movq		%mm4,%mm3;
	movq		16(%esi),%mm0;			psrlw		$1,%mm1;
	paddw		%mm1,%mm6;			punpcklwd	%mm5,%mm4;
	movq		%mm1,24(%edi);			pmaddwd		%mm1,%mm1;
	punpckhwd	%mm5,%mm3;			movq		%mm4,%mm5;
	movq		%mm0,%mm2;			punpcklwd	%mm3,%mm4;
	paddw		c2,%mm4;			punpckhwd	%mm3,%mm5;
	paddw		c2,%mm5;			paddd		%mm1,%mm7;
	movq		16(%eax),%mm1;			psraw		$2,%mm4;
	movq		c255,%mm3;			psraw		$2,%mm5;
	movq		%mm4,-16(%ebx);			pand		%mm3,%mm0;
	movq		%mm5,128+0-16(%ebx);		movq		%mm1,%mm4;
	paddw		c1,%mm0;			pand		%mm3,%mm1;
	paddw		%mm0,%mm1;			psrlw		$8,%mm2;
	movq		16(%esi,%edx),%mm0;		psrlw		$1,%mm1;
	movq		%mm1,256(%edi);			paddw		%mm1,%mm6;
	pmaddwd		%mm1,%mm1;			psrlw		$8,%mm4;
	movq		16(%eax,%edx),%mm5;		paddw		%mm2,%mm4;
	movq		%mm0,%mm2;			pand		%mm3,%mm0;
	paddd		%mm1,%mm7;			movq		%mm5,%mm1;
	pand		%mm3,%mm1;			psrlw		$8,%mm2;
	paddw		%mm0,%mm1;			psrlw		$8,%mm5;
	paddw		c1,%mm1;			paddw		%mm2,%mm4;
	movq		24(%esi),%mm0;			psrlw		$1,%mm1;
	movq		%mm1,256+16(%edi);		paddw		%mm1,%mm6;
	pmaddwd		%mm1,%mm1;			paddw		%mm5,%mm4;
	movq		24(%eax),%mm3;			movq		%mm0,%mm2;
	pand		c255,%mm0;			movq		%mm3,%mm5;
	paddw		c1,%mm0;			psrlw		$8,%mm2;
	pand		c255,%mm5;			psrlw		$8,%mm3;
	paddw		%mm0,%mm5;			paddw		%mm2,%mm3;
	movq		24(%esi,%edx),%mm0;		psrlw		$1,%mm5;
	movq		%mm5,256+8(%edi);		paddw		%mm5,%mm6;
	paddd		%mm1,%mm7;			pmaddwd		%mm5,%mm5;
	movq		24(%eax,%edx),%mm1;		movq		%mm0,%mm2;
	psrlw		$8,%mm2;			leal		(%esi,%edx,2),%esi;
	pand		c255,%mm0;			paddw		%mm2,%mm3;
	paddd		%mm5,%mm7;			leal		(%eax,%edx,2),%eax;
	movq		%mm1,%mm5;			movq		%mm4,%mm2;
	pand		c255,%mm1;			psrlw		$8,%mm5;
	paddw		%mm5,%mm3;			paddw		%mm0,%mm1;
	paddw		c1,%mm1;			punpcklwd	%mm3,%mm4;
	psrlw		$1,%mm1;			punpckhwd	%mm3,%mm2;
	movq		%mm4,%mm3;			punpcklwd	%mm2,%mm4;
	paddw		%mm1,%mm6;			punpckhwd	%mm2,%mm3;
	movq		%mm1,256+24(%edi);		pmaddwd		%mm1,%mm1;
	paddw		c2,%mm4;			leal		32(%edi),%edi;
	paddw		c2,%mm3;			psraw		$2,%mm4;
	movq		c255,%mm5;			psraw		$2,%mm3;
	movq		%mm4,8-16(%ebx);		paddd		%mm1,%mm7;
	movq		%mm3,128+8-16(%ebx);		jne		1b;

	pmaddwd		c1,%mm6;			movq		%mm7,%mm0;
	psrlq		$32,%mm7;			popl		%edi;
	paddd		%mm0,%mm7;			popl		%ecx;
	movq		%mm6,%mm5;			psrlq		$32,%mm6;
	paddd		%mm5,%mm6;			pslld		$8,%mm7;
	movd		%mm6,%edx;			popl		%ebx;
	movd		%mm7,%eax;			imul		%edx,%edx;
	subl		%edx,%eax;			popl		%esi;
	popl		%edx;
	ret

# int
# mmx_YUYV_422_ti(unsigned char *buffer1, unsigned char *buffer2)

	.text
	.align		16
	.globl		mmx_YUYV_422_ti

mmx_YUYV_422_ti:

	movl		2*4(%esp),%eax;
	leal		-20(%esp),%esp;
	movl		%edx,16(%esp);
	movl		filter_y_pitch,%edx;		// filter_y_pitch
	movl		%esi,12(%esp);
	movl		mb_row,%esi;
	movl		%ebx,8(%esp);
	sall		$4,%esi;
	movl		%ecx,4(%esp);
	imull		%edx,%esi;
	movl		%edi,(%esp);
	movl		mb_col,%ecx;
	sall		$5,%ecx;
	addl		filter_y_offs,%esi;
	addl		%ecx,%esi;
	addl		%esi,%eax;		// s2 = buffer2 + filter_y_pitch * mb_row * 16 + mb_col * 32 + filter_y_offs
	addl		1*4+20(%esp),%esi;	// s1 = buffer1 + filter_y_pitch * mb_row * 16 + mb_col * 32 + filter_y_offs
	jmp		filter_s2t;

# int
# mmx_YUYV_422_vi(unsigned char *buffer, unsigned char *unused)

	.text
	.align		16
	.globl		mmx_YUYV_422_vi

mmx_YUYV_422_vi:

	leal		-20(%esp),%esp;
	movl		%esi,12(%esp);
	movl		mb_row,%esi;
	movl		%edx,16(%esp);
	cmpl		mb_last_row,%esi;
	movl		%ebx,8(%esp);
	jl		1f;
	movl		12(%esp),%esi;
	leal		20(%esp),%esp;
	jmp		mmx_YUYV_422;

	.p2align 4,,7
1:
	movl		filter_y_pitch,%edx;		// filter_y_pitch
	movl		%ecx,4(%esp);
	movl		%edi,(%esp);
	sall		$4,%esi;
	movl		2*4+20(%esp),%eax;
	movl		mb_col,%ecx;
	imull		%edx,%esi;
	sall		$5,%ecx;
	addl		filter_y_offs,%esi;
	addl		%ecx,%esi;
	addl		1*4+20(%esp),%esi;	// s1 = buffer + filter_y_pitch * mb_row * 16 + mb_col * 32 + filter_y_offs
	leal		(%esi,%edx),%eax;	// s2 = buffer + filter_y_pitch * (mb_row * 16 + 1) + mb_col * 32 + filter_y_offs
	jmp		filter_s2t;
