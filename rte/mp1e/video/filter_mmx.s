#
#  MPEG-1 Real Time Encoder
# 
#  Copyright (C) 1999, 2000, 2001, 2002 Michael H. Schimek
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

# $Id: filter_mmx.s,v 1.6 2002-08-22 22:04:22 mschimek Exp $

dest		= 0
src		= 4
offset		= 12
u_offset	= 16
v_offset	= 20
stride		= 24
uv_stride	= 28

		.text
		.align		16
		.globl		pmmx_YUV420_0

pmmx_YUV420_0:
		pushl		%ebx;
		movl		uv_stride(%eax),%ebx;
		imull		%ecx,%ebx;
		pushl		%esi;
		addl		%edx,%ebx;		// row * uv_stride + col
		movl		src(%eax),%esi;
		leal		(%esi,%ebx,8),%ebx;	// src + row * 8 * uv_stride + col * 8
		imull		stride(%eax),%ecx;
		addl		%edx,%ecx;
		pushl		%edi;
		movl		dest(%eax),%edi;
		addl		%ecx,%ecx;		// row * 2 * stride + col * 2
		addl		offset(%eax),%esi;
		leal		(%esi,%ecx,8),%esi;	// Y = src + offset + row * 16 * stride + col * 16
		movl		u_offset(%eax),%ecx;
		addl		%ebx,%ecx;		// U = src + u_offset + row * 8 * uv_stride + col * 8
		addl		v_offset(%eax),%ebx;	// V = src + v_offset + row * 8 * uv_stride + col * 8
		movl		uv_stride(%eax),%edx;
		movl		stride(%eax),%eax;

		/* Cb, Cr */
0:
		movq		(%ecx),%mm0;
		movq		(%ebx),%mm1;			pxor		%mm7,%mm7;
		movq		(%ecx,%edx),%mm4;		movq		%mm0,%mm2;
		movq		(%ebx,%edx),%mm5;		punpcklbw	%mm7,%mm0;
		movq		(%ecx,%edx,2),%mm3;		punpckhbw	%mm7,%mm2;
		movq		%mm0,512+0*16+0(%edi);		movq		%mm1,%mm0;
		movq		%mm2,512+0*16+8(%edi);		punpcklbw	%mm7,%mm1;
		movq		(%ebx,%edx,2),%mm2;		punpckhbw	%mm7,%mm0;
		movq		%mm1,640+0*16+0(%edi);		movq		%mm4,%mm1;
		movq		%mm0,640+0*16+8(%edi);		punpcklbw	%mm7,%mm4;
		lea		(%ecx,%edx,2),%ecx;		punpckhbw	%mm7,%mm1;
		movq		%mm4,512+1*16+0(%edi);		movq		%mm5,%mm4;
		movq		%mm1,512+1*16+8(%edi);		punpcklbw	%mm7,%mm5;
		lea		(%ebx,%edx,2),%ebx;		punpckhbw	%mm7,%mm4;
		movq		%mm5,640+1*16+0(%edi);		movq		%mm3,%mm1;
		movq		%mm4,640+1*16+8(%edi);		punpcklbw	%mm7,%mm3;
		movq		(%ecx,%edx),%mm4;		punpckhbw	%mm7,%mm1;
		movq		%mm3,512+2*16+0(%edi);		movq		%mm2,%mm3;
		movq		(%ebx,%edx),%mm5;		punpcklbw	%mm7,%mm2;
		movq		%mm1,512+2*16+8(%edi);		punpckhbw	%mm7,%mm3;
		movq		%mm2,640+2*16+0(%edi);		movq		%mm4,%mm2;
		movq		%mm3,640+2*16+8(%edi);		punpcklbw	%mm7,%mm4;
		movq		(%ecx,%edx,2),%mm0;		punpckhbw	%mm7,%mm2;
		movq		%mm4,512+3*16+0(%edi);		movq		%mm5,%mm4;
		movq		%mm2,512+3*16+8(%edi);		punpcklbw	%mm7,%mm5;
		movq		(%ebx,%edx,2),%mm1;		punpckhbw	%mm7,%mm4;
		movq		%mm5,640+3*16+0(%edi);		lea		(%ecx,%edx,2),%ecx;
		movq		%mm4,640+3*16+8(%edi);		lea		(%ebx,%edx,2),%ebx;
		movq		(%ecx,%edx),%mm4;		movq		%mm0,%mm2;
		movq		(%ebx,%edx),%mm5;		punpcklbw	%mm7,%mm0;
		movq		(%ecx,%edx,2),%mm3;		punpckhbw	%mm7,%mm2;
		movq		%mm0,512+4*16+0(%edi);		movq		%mm1,%mm0;
		movq		%mm2,512+4*16+8(%edi);		punpcklbw	%mm7,%mm1;
		movq		(%ebx,%edx,2),%mm2;		punpckhbw	%mm7,%mm0;
		movq		%mm1,640+4*16+0(%edi);		movq		%mm4,%mm1;
		movq		%mm0,640+4*16+8(%edi);		punpcklbw	%mm7,%mm4;
		lea		(%ecx,%edx,2),%ecx;		punpckhbw	%mm7,%mm1;
		movq		%mm4,512+5*16+0(%edi);		movq		%mm5,%mm4;
		movq		%mm1,512+5*16+8(%edi);		punpcklbw	%mm7,%mm5;
		lea		(%ebx,%edx,2),%ebx;		punpckhbw	%mm7,%mm4;
		movq		%mm5,640+5*16+0(%edi);		movq		%mm3,%mm1;
		movq		%mm4,640+5*16+8(%edi);		punpcklbw	%mm7,%mm3;
		movq		(%ecx,%edx),%mm4;		punpckhbw	%mm7,%mm1;
		movq		%mm3,512+6*16+0(%edi);		movq		%mm2,%mm3;
		movq		(%ebx,%edx),%mm5;		punpcklbw	%mm7,%mm2;
		movq		%mm1,512+6*16+8(%edi);		punpckhbw	%mm7,%mm3;
		movq		%mm2,640+6*16+0(%edi);		movq		%mm4,%mm2;
		movq		%mm3,640+6*16+8(%edi);		punpcklbw	%mm7,%mm4;
		movq		0(%esi),%mm0;			punpckhbw	%mm7,%mm2;
		movq		%mm4,512+7*16+0(%edi);		movq		%mm5,%mm4;
		movq		%mm2,512+7*16+8(%edi);		punpcklbw	%mm7,%mm5;
		movl		$7,%ecx;			punpckhbw	%mm7,%mm4;
		movq		%mm5,640+7*16+0(%edi);		movq		%mm7,%mm6;
		movq		%mm4,640+7*16+8(%edi);		movq		%mm7,%mm5;

		.p2align 4,,7

		/* Y */
1:
		movq		(%esi,%eax),%mm2;		movq		%mm0,%mm1;
		movq		8(%esi),%mm3;			punpcklbw	%mm7,%mm0;
		paddw		%mm0,%mm5;			punpckhbw	%mm7,%mm1;
		paddw		%mm1,%mm5;			movq		%mm0,(%edi);
		pmaddwd		%mm0,%mm0;			movq		%mm1,8(%edi);
		pmaddwd		%mm1,%mm1;			movq		%mm3,%mm4;
		decl		%ecx;				punpcklbw	%mm7,%mm3;
		paddd		%mm0,%mm6;			punpckhbw	%mm7,%mm4;
		paddd		%mm1,%mm6; 			paddw		%mm3,%mm5;
		movq		%mm3,256(%edi);			paddw		%mm4,%mm5;
		movq		%mm4,256+8(%edi);		pmaddwd		%mm3,%mm3;
		movq		(%esi,%eax,2),%mm0;		pmaddwd		%mm4,%mm4;
		movq		%mm2,%mm1;			punpcklbw	%mm7,%mm2;
		paddd		%mm3,%mm6;			punpckhbw	%mm7,%mm1;
		movq		8(%esi,%eax),%mm3;		paddd		%mm4,%mm6;
		paddw		%mm2,%mm5;			movq		%mm2,16(%edi);
		paddw		%mm1,%mm5;			movq		%mm1,24(%edi);
		pmaddwd		%mm2,%mm2;			movq		%mm3,%mm4;
		pmaddwd		%mm1,%mm1;			punpcklbw	%mm7,%mm3;
		leal		(%esi,%eax,2),%esi;		punpckhbw	%mm7,%mm4;
		paddd		%mm2,%mm6;			paddw		%mm3,%mm5;
		paddd		%mm1,%mm6;			paddw		%mm4,%mm5;
		movq		%mm3,256+16(%edi);		pmaddwd		%mm3,%mm3;
		movq		%mm4,256+24(%edi);		pmaddwd		%mm4,%mm4;
		paddd		%mm3,%mm6;			leal		32(%edi),%edi;
		paddd		%mm4,%mm6;			jne		1b;
2:
		movq		(%esi,%eax),%mm2;		movq		%mm0,%mm1;
		movq		8(%esi),%mm3;			punpcklbw	%mm7,%mm0;
		paddw		%mm0,%mm5;			punpckhbw	%mm7,%mm1;
		paddw		%mm1,%mm5;			movq		%mm0,(%edi);
		pmaddwd		%mm0,%mm0;			movq		%mm1,8(%edi);
		movq		%mm3,%mm4;			punpcklbw	%mm7,%mm3;
		pmaddwd		%mm1,%mm1;			punpckhbw	%mm7,%mm4;
		paddd		%mm0,%mm6;			paddw		%mm3,%mm5;
		movq		%mm3,256(%edi);			paddw		%mm4,%mm5;
		paddd		%mm1,%mm6; 			pmaddwd		%mm3,%mm3;
		movq		%mm4,256+8(%edi);		pmaddwd		%mm4,%mm4;
		movq		%mm2,%mm1;			punpcklbw	%mm7,%mm2;
		paddd		%mm3,%mm6;			punpckhbw	%mm7,%mm1;
		paddd		%mm4,%mm6; 			paddw		%mm2,%mm5;
		movq		%mm2,16(%edi);			paddw		%mm1,%mm5;
		movq		%mm1,24(%edi);			pmaddwd		%mm2,%mm2;
		movq		8(%esi,%eax),%mm3;		pmaddwd		%mm1,%mm1;
		movq		%mm3,%mm4;			punpcklbw	%mm7,%mm3;
		paddd		%mm2,%mm6;			punpckhbw	%mm7,%mm4;
		paddd		%mm1,%mm6;			paddw		%mm3,%mm5;
		movq		%mm3,256+16(%edi);		paddw		%mm4,%mm5;
		movq		%mm4,256+24(%edi);		pmaddwd		%mm3,%mm3;
		paddd		%mm3,%mm6;			pmaddwd		%mm4,%mm4;
		pmaddwd		cw1,%mm5;			paddd		%mm4,%mm6;
		movq		%mm6,%mm0;			psrlq		$32,%mm6;
		paddd		%mm0,%mm6;			popl		%edi;
		movq		%mm5,%mm4;			psrlq		$32,%mm5;
		paddd		%mm4,%mm5;			pslld		$8,%mm6;
		movd		%mm5,%edx;			popl		%esi;
		movd		%mm6,%eax;			imull		%edx,%edx;
		subl		%edx,%eax;			popl		%ebx;
		ret

		.text
		.align	16
		.globl	pmmx_YUV420_2

pmmx_YUV420_2:
		subl		$12,%esp
		movl		%edi,(%esp)
		movl		28(%eax),%edi
		imull		%ecx,%edi
		leal		0(%edx,%edi,2),%edi
		movl		%ebx,1*4(%esp)
		movl		4(%eax),%ebx
		leal		0(%ebx,%edi,8),%edi
		imull		24(%eax),%ecx
		leal		0(%edx,%ecx,2),%ecx
		movl		(%eax),%edx
		movl		%esi,2*4(%esp)
		addl		%ecx,%ecx
		addl		12(%eax),%ebx
		leal		0(%ebx,%ecx,8),%ebx
		movl		16(%eax),%ecx
		addl		%edi,%ecx
		addl		20(%eax),%edi
		movl		28(%eax),%esi

		movq		(%ecx),%mm0;			addl		%esi,%esi;
		movq		(%edi),%mm1;			pxor		%mm7,%mm7;
		movq		(%ecx,%esi),%mm4;		movq		%mm0,%mm2;
		movq		(%edi,%esi),%mm5;		punpcklbw	%mm7,%mm0;
		movq		(%ecx,%esi,2),%mm3;		punpckhbw	%mm7,%mm2;
		movq		%mm0,512+0*16+0(%edx);		movq		%mm1,%mm0;
		movq		%mm2,512+0*16+8(%edx);		punpcklbw	%mm7,%mm1;
		movq		(%edi,%esi,2),%mm2;		punpckhbw	%mm7,%mm0;
		movq		%mm1,640+0*16+0(%edx);		movq		%mm4,%mm1;
		movq		%mm0,640+0*16+8(%edx);		punpcklbw	%mm7,%mm4;
		lea		(%ecx,%esi,2),%ecx;		punpckhbw	%mm7,%mm1;
		movq		%mm4,512+1*16+0(%edx);		movq		%mm5,%mm4;
		movq		%mm1,512+1*16+8(%edx);		punpcklbw	%mm7,%mm5;
		lea		(%edi,%esi,2),%edi;		punpckhbw	%mm7,%mm4;
		movq		%mm5,640+1*16+0(%edx);		movq		%mm3,%mm1;
		movq		%mm4,640+1*16+8(%edx);		punpcklbw	%mm7,%mm3;
		movq		(%ecx,%esi),%mm4;		punpckhbw	%mm7,%mm1;
		movq		%mm3,512+2*16+0(%edx);		movq		%mm2,%mm3;
		movq		(%edi,%esi),%mm5;		punpcklbw	%mm7,%mm2;
		movq		%mm1,512+2*16+8(%edx);		punpckhbw	%mm7,%mm3;
		movq		%mm2,640+2*16+0(%edx);		movq		%mm4,%mm2;
		movq		%mm3,640+2*16+8(%edx);		punpcklbw	%mm7,%mm4;
		movq		(%ecx,%esi,2),%mm0;		punpckhbw	%mm7,%mm2;
		movq		%mm4,512+3*16+0(%edx);		movq		%mm5,%mm4;
		movq		%mm2,512+3*16+8(%edx);		punpcklbw	%mm7,%mm5;
		movq		(%edi,%esi,2),%mm1;		punpckhbw	%mm7,%mm4;
		movq		%mm5,640+3*16+0(%edx);		lea		(%ecx,%esi,2),%ecx;
		movq		%mm4,640+3*16+8(%edx);		lea		(%edi,%esi,2),%edi;
		movq		(%ecx,%esi),%mm4;		movq		%mm0,%mm2;
		movq		(%edi,%esi),%mm5;		punpcklbw	%mm7,%mm0;
		movq		(%ecx,%esi,2),%mm3;		punpckhbw	%mm7,%mm2;
		movq		%mm0,512+4*16+0(%edx);		movq		%mm1,%mm0;
		movq		%mm2,512+4*16+8(%edx);		punpcklbw	%mm7,%mm1;
		movq		(%edi,%esi,2),%mm2;		punpckhbw	%mm7,%mm0;
		movq		%mm1,640+4*16+0(%edx);		movq		%mm4,%mm1;
		movq		%mm0,640+4*16+8(%edx);		punpcklbw	%mm7,%mm4;
		lea		(%ecx,%esi,2),%ecx;		punpckhbw	%mm7,%mm1;
		movq		%mm4,512+5*16+0(%edx);		movq		%mm5,%mm4;
		movq		%mm1,512+5*16+8(%edx);		punpcklbw	%mm7,%mm5;
		lea		(%edi,%esi,2),%edi;		punpckhbw	%mm7,%mm4;
		movq		%mm5,640+5*16+0(%edx);		movq		%mm3,%mm1;
		movq		%mm4,640+5*16+8(%edx);		punpcklbw	%mm7,%mm3;
		movq		(%ecx,%esi),%mm4;		punpckhbw	%mm7,%mm1;
		movq		%mm3,512+6*16+0(%edx);		movq		%mm2,%mm3;
		movq		(%edi,%esi),%mm5;		punpcklbw	%mm7,%mm2;
		movq		%mm1,512+6*16+8(%edx);		punpckhbw	%mm7,%mm3;
		movq		%mm2,640+6*16+0(%edx);		movq		%mm4,%mm2;
		movq		%mm3,640+6*16+8(%edx);		punpcklbw	%mm7,%mm4;
		movq		(%ebx),%mm0; 			punpckhbw	%mm7,%mm2
		movq		%mm4,512+7*16+0(%edx);		movq		%mm5,%mm4;
		movl		24(%eax),%ecx;			punpcklbw	%mm7,%mm5;
		movq		%mm2,512+7*16+8(%edx);		punpckhbw	%mm7,%mm4
		movq		0(%ebx,%ecx),%mm2; 		movq		%mm7,%mm6;
		movq		%mm5,640+7*16+0(%edx);		movq		%mm7,%mm5;
		movq		%mm4,640+7*16+8(%edx);		movq		%mm7,%mm3;
		movq		cb1,%mm4; 			leal		(%ecx,%ecx,2),%edi;
		movl		$7,%esi;

		.p2align	4,,7
1:
		movq		0(%ebx,%ecx,2),%mm1; 		paddd		%mm3,%mm6
		movq		%mm0,%mm3; 			por		%mm4,%mm0
		por		%mm2,%mm3; 			por		%mm4,%mm2
		pand		%mm4,%mm3; 			psrlq		$1,%mm0
		paddb		%mm3,%mm0; 			psrlq		$1,%mm2
		paddb		%mm2,%mm0; 			movq		8(%ebx),%mm2
		movq		%mm0,%mm3; 			punpcklbw	%mm7,%mm0
		paddw		%mm0,%mm5; 			movq		%mm0,(%edx)
		pmaddwd		%mm0,%mm0; 			punpckhbw	%mm7,%mm3
		paddw		%mm3,%mm5; 			movq		%mm3,8(%edx)
		pmaddwd		%mm3,%mm3; 			paddd		%mm0,%mm6
		movq		8(%ebx,%ecx),%mm0; 		paddd		%mm3,%mm6
		movq		%mm0,%mm3; 			por		%mm4,%mm0
		por		%mm2,%mm3; 			por		%mm4,%mm2
		pand		%mm4,%mm3; 			psrlq		$1,%mm0
		paddb		%mm3,%mm0; 			psrlq		$1,%mm2
		paddb		%mm2,%mm0; 			movq		0(%ebx,%edi),%mm2
		movq		%mm0,%mm3; 			punpcklbw	%mm7,%mm0
		paddw		%mm0,%mm5; 			movq		%mm0,256(%edx)
		pmaddwd		%mm0,%mm0; 			punpckhbw	%mm7,%mm3
		paddw		%mm3,%mm5; 			movq		%mm3,264(%edx)
		pmaddwd		%mm3,%mm3; 			paddd		%mm0,%mm6
		movq		0(%ebx,%ecx,4),%mm0; 		paddd		%mm3,%mm6
		movq		%mm1,%mm3; 			por		%mm4,%mm1
		por		%mm2,%mm3; 			por		%mm4,%mm2
		pand		%mm4,%mm3; 			psrlq		$1,%mm1
		paddb		%mm3,%mm1; 			psrlq		$1,%mm2
		paddb		%mm2,%mm1; 			movq		8(%ebx,%ecx,2),%mm2
		movq		%mm1,%mm3; 			punpcklbw	%mm7,%mm1
		paddw		%mm1,%mm5; 			movq		%mm1,16(%edx)
		pmaddwd		%mm1,%mm1; 			punpckhbw	%mm7,%mm3
		paddw		%mm3,%mm5; 			movq		%mm3,24(%edx)
		pmaddwd		%mm3,%mm3; 			decl		%esi
		paddd		%mm1,%mm6; 			movq		8(%ebx,%edi),%mm1
		leal		0(%ebx,%ecx,4),%ebx; 		paddd		%mm3,%mm6
		movq		%mm1,%mm3; 			por		%mm4,%mm1
		por		%mm2,%mm3; 			por		%mm4,%mm2
		pand		%mm4,%mm3; 			psrlq		$1,%mm1
		paddb		%mm3,%mm1; 			psrlq		$1,%mm2
		paddb		%mm2,%mm1; 			movq		0(%ebx,%ecx),%mm2
		movq		%mm1,%mm3; 			punpcklbw	%mm7,%mm1
		paddw		%mm1,%mm5; 			movq		%mm1,272(%edx)
		pmaddwd		%mm1,%mm1; 			punpckhbw	%mm7,%mm3
		paddw		%mm3,%mm5; 			movq		%mm3,280(%edx)
		pmaddwd		%mm3,%mm3; 			leal		32(%edx),%edx
		paddd		%mm1,%mm6; 			jne		1b
		movq		0(%ebx,%ecx,2),%mm1; 		paddd		%mm3,%mm6
		movq		%mm0,%mm3;			por		%mm4,%mm0
		por		%mm2,%mm3;			por		%mm4,%mm2
		pand		%mm4,%mm3;			psrlq		$1,%mm0
		paddb		%mm3,%mm0;			psrlq		$1,%mm2
		paddb		%mm2,%mm0;			movq		8(%ebx),%mm2
		movq		%mm0,%mm3;			punpcklbw	%mm7,%mm0
		paddw		%mm0,%mm5;			movq		%mm0,(%edx)
		pmaddwd		%mm0,%mm0;			punpckhbw	%mm7,%mm3
		paddw		%mm3,%mm5;			movq		%mm3,8(%edx)
		pmaddwd		%mm3,%mm3;			paddd		%mm0,%mm6
		movq		8(%ebx,%ecx),%mm0; 		paddd		%mm3,%mm6
		movq		%mm0,%mm3;			por		%mm4,%mm0
		por		%mm2,%mm3;			por		%mm4,%mm2
		pand		%mm4,%mm3;			psrlq		$1,%mm0
		paddb		%mm3,%mm0;			psrlq		$1,%mm2
		paddb		%mm2,%mm0;			movq		0(%ebx,%edi),%mm2
		movq		%mm0,%mm3;			punpcklbw	%mm7,%mm0
		paddw		%mm0,%mm5;			movq		%mm0,256(%edx)
		pmaddwd		%mm0,%mm0;			punpckhbw	%mm7,%mm3
		paddw		%mm3,%mm5;			movq		%mm3,264(%edx)
		pmaddwd		%mm3,%mm3;			paddd		%mm0,%mm6
		paddd		%mm3,%mm6;			movq		%mm1,%mm3
		por		%mm4,%mm1;			por		%mm2,%mm3
		por		%mm4,%mm2;			pand		%mm4,%mm3
		psrlq		$1,%mm1;
		paddb		%mm3,%mm1; 			psrlq		$1,%mm2
		paddb		%mm2,%mm1; 			movq		8(%ebx,%ecx,2),%mm2
		movq		%mm1,%mm3; 			punpcklbw	%mm7,%mm1
		paddw		%mm1,%mm5; 			movq		%mm1,16(%edx)
		pmaddwd		%mm1,%mm1; 			punpckhbw	%mm7,%mm3
		paddw		%mm3,%mm5; 			movq		%mm3,24(%edx)
		pmaddwd		%mm3,%mm3; 			paddd		%mm1,%mm6
		movq		8(%ebx,%edi),%mm1; 		leal		0(%ebx,%ecx,4),%ebx
		paddd		%mm3,%mm6; 			movq		%mm1,%mm3
		por		%mm4,%mm1; 			por		%mm2,%mm3
		por		%mm4,%mm2; 			psrlq		$1,%mm1
		pand		%mm4,%mm3; 			psrlq		$1,%mm2
		paddb		%mm3,%mm1; 			paddb		%mm2,%mm1
		movq		%mm1,%mm3; 			punpcklbw	%mm7,%mm1
		paddw		%mm1,%mm5; 			movq		%mm1,272(%edx)
		pmaddwd		%mm1,%mm1; 			punpckhbw	%mm7,%mm3
		paddw		%mm3,%mm5; 			movq		%mm3,280(%edx)
		pmaddwd		%mm3,%mm3; 			paddd		%mm1,%mm6
		paddd		%mm3,%mm6; 			pmaddwd		cw1,%mm5
		movq		%mm6,%mm4; 			psrlq		$32,%mm6
		paddd		%mm4,%mm6; 			movq		%mm5,%mm2
		popl		%edi; 				psrlq		$32,%mm5
		paddd		%mm2,%mm5; 			pslld		$8,%mm6
		popl		%ebx; 				movd		%mm5,%ecx
		imull		%ecx,%ecx; 			movd		%mm6,%eax
		popl		%esi; 				subl		%ecx,%eax
		ret

		.text
		.align	16
		.globl	sse_YUV420_0

sse_YUV420_0:
		subl		$12,%esp
		movl		%edi,(%esp)
		movl		28(%eax),%edi
		imull		%ecx,%edi
		addl		%edx,%edi
		movl		%ebx,1*4(%esp)
		movl		4(%eax),%ebx
		leal		0(%ebx,%edi,8),%edi
		imull		24(%eax),%ecx
		addl		%edx,%ecx
		movl		(%eax),%edx
		movl		%esi,2*4(%esp)
		addl		%ecx,%ecx
		addl		12(%eax),%ebx
		leal		0(%ebx,%ecx,8),%ebx
		movl		16(%eax),%ecx
		addl		%edi,%ecx
		addl		20(%eax),%edi
		movl		28(%eax),%esi
		pxor		%mm7,%mm7
		movq		(%ecx),%mm0
		movq		0(%ecx,%esi),%mm1
		movq		0(%ecx,%esi,2),%mm2
		movq		0(%ecx,%esi,4),%mm3
		addl		%esi,%ecx
		movq		%mm0,%mm6
		punpcklbw	%mm7,%mm0
		punpckhbw	%mm7,%mm6
		movq		%mm0,512(%edx)
		movq		%mm6,520(%edx)
		movq		%mm1,%mm6
		punpcklbw	%mm7,%mm1
		punpckhbw	%mm7,%mm6
		movq		%mm1,528(%edx)
		movq		%mm6,536(%edx)
		movq		%mm2,%mm6
		punpcklbw	%mm7,%mm2
		punpckhbw	%mm7,%mm6
		movq		%mm2,544(%edx)
		movq		%mm6,552(%edx)
		movq		0(%ecx,%esi,2),%mm4
		leal		0(%ecx,%esi,4),%ecx
		movq		%mm4,%mm6
		punpcklbw	%mm7,%mm4
		punpckhbw	%mm7,%mm6
		movq		%mm4,560(%edx)
		movq		%mm6,568(%edx)
		movq		%mm3,%mm6
		punpcklbw	%mm7,%mm3
		punpckhbw	%mm7,%mm6
		movq		%mm3,576(%edx)
		movq		%mm6,584(%edx)
		movq		(%ecx),%mm3
		movq		0(%ecx,%esi),%mm4
		movq		0(%ecx,%esi,2),%mm5
		movq		%mm3,%mm6
		punpcklbw	%mm7,%mm3
		punpckhbw	%mm7,%mm6
		movq		%mm3,592(%edx)
		movq		%mm6,600(%edx)
		movq		%mm4,%mm6
		punpcklbw	%mm7,%mm4
		punpckhbw	%mm7,%mm6
		movq		%mm4,608(%edx)
		movq		%mm6,616(%edx)
		movq		%mm5,%mm6
		punpcklbw	%mm7,%mm5
		punpckhbw	%mm7,%mm6
		movq		%mm5,624(%edx)
		movq		%mm6,632(%edx)
		movl		24(%eax),%ecx
		movq		(%edi),%mm0
		movq		0(%edi,%esi),%mm1
		movq		0(%edi,%esi,2),%mm2
		movq		0(%edi,%esi,4),%mm3
		addl		%esi,%edi
		movq		%mm0,%mm6
		punpcklbw	%mm7,%mm0
		punpckhbw	%mm7,%mm6
		movq		%mm0,640(%edx)
		movq		%mm6,648(%edx)
		movq		(%ebx),%mm0
		movq		%mm1,%mm6
		punpcklbw	%mm7,%mm1
		punpckhbw	%mm7,%mm6
		movq		%mm1,656(%edx)
		movq		%mm6,664(%edx)
		movq		%mm2,%mm6
		punpcklbw	%mm7,%mm2
		punpckhbw	%mm7,%mm6
		movq		%mm2,672(%edx)
		movq		%mm6,680(%edx)
		movq		0(%edi,%esi,2),%mm4
		leal		0(%edi,%esi,4),%edi
		movq		%mm4,%mm6
		punpcklbw	%mm7,%mm4
		punpckhbw	%mm7,%mm6
		movq		%mm4,688(%edx)
		movq		%mm6,696(%edx)
		movq		0(%ebx,%ecx),%mm1
		movq		%mm3,%mm6
		punpcklbw	%mm7,%mm3
		punpckhbw	%mm7,%mm6
		movq		%mm3,704(%edx)
		movq		%mm6,712(%edx)
		movq		(%edi),%mm3
		movq		0(%edi,%esi),%mm4
		movq		0(%edi,%esi,2),%mm5
		movq		%mm3,%mm6
		punpcklbw	%mm7,%mm3
		punpckhbw	%mm7,%mm6
		movq		%mm3,720(%edx)
		movq		%mm6,728(%edx)
		movq		%mm4,%mm6
		punpcklbw	%mm7,%mm4
		punpckhbw	%mm7,%mm6
		movq		%mm4,736(%edx)
		movq		%mm6,744(%edx)
		movq		%mm5,%mm6
		punpcklbw	%mm7,%mm5
		punpckhbw	%mm7,%mm6
		movq		%mm5,752(%edx)
		movq		%mm6,760(%edx)
		leal		(%ecx,%ecx,2),%edi
		pxor		%mm4,%mm4
		pxor		%mm5,%mm5
		movl		$4,%esi
		pxor		%mm2,%mm2
		pxor		%mm6,%mm6

		.p2align	4,,7
1:
		paddd		%mm2,%mm6
		movq		0(%ebx,%ecx,2),%mm2
		movq		%mm0,%mm3
		punpcklbw	%mm7,%mm0
		paddw		%mm0,%mm5
		punpckhbw	%mm7,%mm3
		movq		%mm0,(%edx)
		pmaddwd		%mm0,%mm0
		paddd		%mm4,%mm6
		paddw		%mm3,%mm5
		movq		%mm3,8(%edx)
		pmaddwd		%mm3,%mm3
		paddd		%mm0,%mm6
		movq		8(%ebx),%mm0
		movq		%mm0,%mm4
		punpcklbw	%mm7,%mm0
		paddw		%mm0,%mm5
		punpckhbw	%mm7,%mm4
		movq		%mm0,256(%edx)
		pmaddwd		%mm0,%mm0
		paddd		%mm3,%mm6
		paddw		%mm4,%mm5
		movq		%mm4,264(%edx)
		pmaddwd		%mm4,%mm4
		paddd		%mm0,%mm6
		movq		0(%ebx,%edi),%mm0
		movq		%mm1,%mm3
		punpcklbw	%mm7,%mm1
		paddw		%mm1,%mm5
		punpckhbw	%mm7,%mm3
		movq		%mm1,16(%edx)
		pmaddwd		%mm1,%mm1
		paddd		%mm4,%mm6
		paddw		%mm3,%mm5
		movq		%mm3,24(%edx)
		pmaddwd		%mm3,%mm3
		paddd		%mm1,%mm6
		movq		8(%ebx,%ecx),%mm1
		movq		%mm1,%mm4
		punpcklbw	%mm7,%mm1
		paddw		%mm1,%mm5
		punpckhbw	%mm7,%mm4
		movq		%mm1,272(%edx)
		pmaddwd		%mm1,%mm1
		paddd		%mm3,%mm6
		paddw		%mm4,%mm5
		movq		%mm4,280(%edx)
		pmaddwd		%mm4,%mm4
		paddd		%mm1,%mm6
		movq		0(%ebx,%ecx,4),%mm1
		movq		%mm2,%mm3
		punpcklbw	%mm7,%mm2
		paddw		%mm2,%mm5
		punpckhbw	%mm7,%mm3
		movq		%mm2,32(%edx)
		pmaddwd		%mm2,%mm2
		paddd		%mm4,%mm6
		paddw		%mm3,%mm5
		movq		%mm3,40(%edx)
		pmaddwd		%mm3,%mm3
		paddd		%mm2,%mm6
		movq		8(%ebx,%ecx,2),%mm2
		addl		%edi,%ebx
		decl		%esi
		movq		%mm2,%mm4
		punpcklbw	%mm7,%mm2
		paddw		%mm2,%mm5
		punpckhbw	%mm7,%mm4
		movq		%mm2,288(%edx)
		pmaddwd		%mm2,%mm2
		paddd		%mm3,%mm6
		paddw		%mm4,%mm5
		movq		%mm4,296(%edx)
		pmaddwd		%mm4,%mm4
		leal		48(%edx),%edx
		jne		1b
		paddd		%mm2,%mm6
		movq		0(%ebx,%ecx,2),%mm2
		movq		%mm0,%mm3
		punpcklbw	%mm7,%mm0
		paddw		%mm0,%mm5
		punpckhbw	%mm7,%mm3
		movq		%mm0,(%edx)
		pmaddwd		%mm0,%mm0
		paddd		%mm4,%mm6
		paddw		%mm3,%mm5
		movq		%mm3,8(%edx)
		pmaddwd		%mm3,%mm3
		paddd		%mm0,%mm6
		movq		8(%ebx),%mm0
		movq		%mm0,%mm4
		punpcklbw	%mm7,%mm0
		paddw		%mm0,%mm5
		punpckhbw	%mm7,%mm4
		movq		%mm0,256(%edx)
		pmaddwd		%mm0,%mm0
		paddd		%mm3,%mm6
		paddw		%mm4,%mm5
		movq		%mm4,264(%edx)
		pmaddwd		%mm4,%mm4
		paddd		%mm0,%mm6
		movq		0(%ebx,%edi),%mm0
		movq		%mm1,%mm3
		punpcklbw	%mm7,%mm1
		paddw		%mm1,%mm5
		punpckhbw	%mm7,%mm3
		movq		%mm1,16(%edx)
		pmaddwd		%mm1,%mm1
		paddd		%mm4,%mm6
		paddw		%mm3,%mm5
		movq		%mm3,24(%edx)
		pmaddwd		%mm3,%mm3
		paddd		%mm1,%mm6
		movq		8(%ebx,%ecx),%mm1
		movq		%mm1,%mm4
		punpcklbw	%mm7,%mm1
		paddw		%mm1,%mm5
		punpckhbw	%mm7,%mm4
		movq		%mm1,272(%edx)
		pmaddwd		%mm1,%mm1
		paddd		%mm3,%mm6
		paddw		%mm4,%mm5
		movq		%mm4,280(%edx)
		pmaddwd		%mm4,%mm4
		paddd		%mm1,%mm6
		movq		%mm2,%mm3
		punpcklbw	%mm7,%mm2
		paddw		%mm2,%mm5
		punpckhbw	%mm7,%mm3
		movq		%mm2,32(%edx)
		pmaddwd		%mm2,%mm2
		paddd		%mm4,%mm6
		paddw		%mm3,%mm5
		movq		%mm3,40(%edx)
		pmaddwd		%mm3,%mm3
		paddd		%mm2,%mm6
		movq		8(%ebx,%ecx,2),%mm2
		movq		%mm2,%mm4
		punpcklbw	%mm7,%mm2
		paddw		%mm2,%mm5
		punpckhbw	%mm7,%mm4
		movq		%mm2,288(%edx)
		pmaddwd		%mm2,%mm2
		paddd		%mm3,%mm6
		paddw		%mm4,%mm5
		movq		%mm4,296(%edx)
		pmaddwd		%mm4,%mm4
		addl		%edi,%ebx
		leal		48(%edx),%edx
		paddd		%mm2,%mm6
		movq		%mm0,%mm3
		punpcklbw	%mm7,%mm0
		paddw		%mm0,%mm5
		punpckhbw	%mm7,%mm3
		movq		%mm0,(%edx)
		pmaddwd		%mm0,%mm0
		paddd		%mm4,%mm6
		paddw		%mm3,%mm5
		movq		%mm3,8(%edx)
		pmaddwd		%mm3,%mm3
		paddd		%mm0,%mm6
		movq		8(%ebx),%mm0
		movq		%mm0,%mm4
		punpcklbw	%mm7,%mm0
		paddw		%mm0,%mm5
		punpckhbw	%mm7,%mm4
		movq		%mm0,256(%edx)
		pmaddwd		%mm0,%mm0
		paddd		%mm3,%mm6
		paddw		%mm4,%mm5
		movq		%mm4,264(%edx)
		pmaddwd		%mm4,%mm4
		paddd		%mm0,%mm6
		paddd		%mm4,%mm6
		pmaddwd		cw1,%mm5
		pshufw		$238,%mm6,%mm4
		paddd		%mm4,%mm6
		pshufw		$238,%mm5,%mm4
		paddd		%mm4,%mm5
		pslld		$8,%mm6
		movd		%mm5,%ecx
		movd		%mm6,%eax
		imull		%ecx,%ecx
		subl		%ecx,%eax
		popl		%edi
		popl		%ebx
		popl		%esi
		ret

		.text
		.align	16
		.globl	sse_YUV420_2

sse_YUV420_2:
		subl		$12,%esp
		movl		%edi,(%esp)
		movl		28(%eax),%edi
		imull		%ecx,%edi
		leal		0(%edx,%edi,2),%edi
		movl		%ebx,1*4(%esp)
		movl		4(%eax),%ebx
		leal		0(%ebx,%edi,8),%edi
		imull		24(%eax),%ecx
		leal		0(%edx,%ecx,2),%ecx
		movl		(%eax),%edx
		movl		%esi,2*4(%esp)
		addl		%ecx,%ecx
		addl		12(%eax),%ebx
		leal		0(%ebx,%ecx,8),%ebx
		movl		16(%eax),%ecx
		addl		%edi,%ecx
		addl		20(%eax),%edi
		movl		28(%eax),%esi
		pxor		%mm7,%mm7
		movq		(%ecx),%mm0
		pavgb		0(%ecx,%esi),%mm0
		movq		0(%ecx,%esi,2),%mm1
		movq		0(%ecx,%esi,4),%mm2
		movq		0(%ecx,%esi,8),%mm4
		addl		%esi,%ecx
		movq		%mm0,%mm6
		punpcklbw	%mm7,%mm0
		punpckhbw	%mm7,%mm6
		movq		%mm0,512(%edx)
		movq		%mm6,520(%edx)
		pavgb		0(%ecx,%esi,2),%mm1
		pavgb		0(%ecx,%esi,4),%mm2
		pavgb		0(%ecx,%esi,8),%mm4
		leal		0(%ecx,%esi,4),%ecx
		movq		%mm1,%mm6
		punpcklbw	%mm7,%mm1
		punpckhbw	%mm7,%mm6
		movq		%mm1,528(%edx)
		movq		%mm6,536(%edx)
		movq		%mm2,%mm6
		punpcklbw	%mm7,%mm2
		punpckhbw	%mm7,%mm6
		movq		%mm2,544(%edx)
		movq		%mm6,552(%edx)
		movq		0(%ecx,%esi),%mm3
		pavgb		0(%ecx,%esi,2),%mm3
		movq		0(%ecx,%esi,8),%mm5
		leal		0(%ecx,%esi,2),%ecx
		movq		%mm3,%mm6
		punpcklbw	%mm7,%mm3
		punpckhbw	%mm7,%mm6
		movq		%mm3,560(%edx)
		movq		%mm6,568(%edx)
		movq		%mm4,%mm6
		punpcklbw	%mm7,%mm4
		punpckhbw	%mm7,%mm6
		movq		%mm4,576(%edx)
		movq		%mm6,584(%edx)
		movq		0(%ecx,%esi,4),%mm3
		movq		0(%ecx,%esi,8),%mm4
		addl		%esi,%ecx
		pavgb		0(%ecx,%esi,2),%mm3
		pavgb		0(%ecx,%esi,4),%mm5
		leal		0(%ecx,%esi,4),%ecx
		movq		%mm3,%mm6
		punpcklbw	%mm7,%mm3
		punpckhbw	%mm7,%mm6
		movq		%mm3,592(%edx)
		movq		%mm6,600(%edx)
		pavgb		0(%ecx,%esi,2),%mm4
		movq		%mm5,%mm6
		punpcklbw	%mm7,%mm5
		punpckhbw	%mm7,%mm6
		movq		%mm5,608(%edx)
		movq		%mm6,616(%edx)
		movq		%mm4,%mm6
		punpcklbw	%mm7,%mm4
		punpckhbw	%mm7,%mm6
		movq		%mm4,624(%edx)
		movq		%mm6,632(%edx)
		movl		24(%eax),%ecx
		movq		(%edi),%mm0
		pavgb		0(%edi,%esi),%mm0
		movq		0(%edi,%esi,2),%mm1
		movq		0(%edi,%esi,4),%mm2
		movq		0(%edi,%esi,8),%mm4
		addl		%esi,%edi
		movq		%mm0,%mm6
		punpcklbw	%mm7,%mm0
		punpckhbw	%mm7,%mm6
		movq		%mm0,640(%edx)
		movq		%mm6,648(%edx)
		pavgb		0(%edi,%esi,2),%mm1
		pavgb		0(%edi,%esi,4),%mm2
		pavgb		0(%edi,%esi,8),%mm4
		leal		0(%edi,%esi,4),%edi
		movq		%mm1,%mm6
		punpcklbw	%mm7,%mm1
		punpckhbw	%mm7,%mm6
		movq		%mm1,656(%edx)
		movq		%mm6,664(%edx)
		movq		(%ebx),%mm0
		movq		%mm2,%mm6
		punpcklbw	%mm7,%mm2
		punpckhbw	%mm7,%mm6
		movq		%mm2,672(%edx)
		movq		%mm6,680(%edx)
		movq		0(%edi,%esi),%mm3
		pavgb		0(%edi,%esi,2),%mm3
		movq		0(%edi,%esi,8),%mm5
		leal		0(%edi,%esi,2),%edi
		movq		%mm3,%mm6
		punpcklbw	%mm7,%mm3
		punpckhbw	%mm7,%mm6
		movq		%mm3,688(%edx)
		movq		%mm6,696(%edx)
		movq		0(%ebx,%ecx),%mm2
		movq		%mm4,%mm6
		punpcklbw	%mm7,%mm4
		punpckhbw	%mm7,%mm6
		movq		%mm4,704(%edx)
		movq		%mm6,712(%edx)
		movq		0(%edi,%esi,4),%mm3
		movq		0(%edi,%esi,8),%mm4
		addl		%esi,%edi
		pavgb		0(%edi,%esi,2),%mm3
		pavgb		0(%edi,%esi,4),%mm5
		leal		0(%edi,%esi,4),%edi
		movq		%mm3,%mm6
		punpcklbw	%mm7,%mm3
		punpckhbw	%mm7,%mm6
		movq		%mm3,720(%edx)
		movq		%mm6,728(%edx)
		pavgb		0(%edi,%esi,2),%mm4
		movq		%mm5,%mm6
		punpcklbw	%mm7,%mm5
		punpckhbw	%mm7,%mm6
		movq		%mm5,736(%edx)
		movq		%mm6,744(%edx)
		movq		%mm4,%mm6
		punpcklbw	%mm7,%mm4
		punpckhbw	%mm7,%mm6
		movq		%mm4,752(%edx)
		movq		%mm6,760(%edx)
		leal		(%ecx,%ecx,2),%edi
		pxor		%mm5,%mm5
		pxor		%mm4,%mm4
		pxor		%mm6,%mm6
		movl		$7,%esi
		.p2align	4,,7
1:
		movq		0(%ebx,%ecx,2),%mm1
		movq		8(%ebx),%mm3
		paddd		%mm4,%mm6
		pavgb		%mm2,%mm0
		movq		8(%ebx,%ecx),%mm2
		movq		%mm0,%mm4
		punpcklbw	%mm7,%mm0
		paddw		%mm0,%mm5
		movq		%mm0,(%edx)
		pmaddwd		%mm0,%mm0
		punpckhbw	%mm7,%mm4
		paddw		%mm4,%mm5
		movq		%mm4,8(%edx)
		pmaddwd		%mm4,%mm4
		pavgb		%mm3,%mm2
		paddd		%mm0,%mm6
		paddd		%mm4,%mm6
		movq		%mm2,%mm4
		punpcklbw	%mm7,%mm2
		paddw		%mm2,%mm5
		movq		%mm2,256(%edx)
		pmaddwd		%mm2,%mm2
		punpckhbw	%mm7,%mm4
		paddw		%mm4,%mm5
		movq		%mm4,264(%edx)
		pmaddwd		%mm4,%mm4
		paddd		%mm2,%mm6
		movq		0(%ebx,%edi),%mm2
		movq		0(%ebx,%ecx,4),%mm0
		movq		8(%ebx,%ecx,2),%mm3
		paddd		%mm4,%mm6
		pavgb		%mm2,%mm1
		movq		8(%ebx,%edi),%mm2
		movq		%mm1,%mm4
		punpcklbw	%mm7,%mm1
		paddw		%mm1,%mm5
		movq		%mm1,16(%edx)
		pmaddwd		%mm1,%mm1
		punpckhbw	%mm7,%mm4
		paddw		%mm4,%mm5
		movq		%mm4,24(%edx)
		pmaddwd		%mm4,%mm4
		decl		%esi
		leal		0(%ebx,%ecx,4),%ebx
		pavgb		%mm3,%mm2
		paddd		%mm1,%mm6
		paddd		%mm4,%mm6
		movq		%mm2,%mm4
		punpcklbw	%mm7,%mm2
		paddw		%mm2,%mm5
		movq		%mm2,272(%edx)
		pmaddwd		%mm2,%mm2
		punpckhbw	%mm7,%mm4
		paddw		%mm4,%mm5
		movq		%mm4,280(%edx)
		pmaddwd		%mm4,%mm4
		paddd		%mm2,%mm6
		movq		0(%ebx,%ecx),%mm2
		leal		32(%edx),%edx
		jne		1b
		movq		0(%ebx,%ecx,2),%mm1
		movq		8(%ebx),%mm3
		paddd		%mm4,%mm6
		pavgb		%mm2,%mm0
		movq		8(%ebx,%ecx),%mm2
		movq		%mm0,%mm4
		punpcklbw	%mm7,%mm0
		paddw		%mm0,%mm5
		movq		%mm0,(%edx)
		pmaddwd		%mm0,%mm0
		punpckhbw	%mm7,%mm4
		paddw		%mm4,%mm5
		movq		%mm4,8(%edx)
		pmaddwd		%mm4,%mm4
		pavgb		%mm3,%mm2
		paddd		%mm0,%mm6
		paddd		%mm4,%mm6
		movq		%mm2,%mm4
		punpcklbw	%mm7,%mm2
		paddw		%mm2,%mm5
		movq		%mm2,256(%edx)
		pmaddwd		%mm2,%mm2
		punpckhbw	%mm7,%mm4
		paddw		%mm4,%mm5
		movq		%mm4,264(%edx)
		pmaddwd		%mm4,%mm4
		paddd		%mm2,%mm6
		movq		0(%ebx,%edi),%mm2
		movq		8(%ebx,%ecx,2),%mm3
		paddd		%mm4,%mm6
		pavgb		%mm2,%mm1
		movq		8(%ebx,%edi),%mm2
		movq		%mm1,%mm4
		punpcklbw	%mm7,%mm1
		paddw		%mm1,%mm5
		movq		%mm1,16(%edx)
		pmaddwd		%mm1,%mm1
		punpckhbw	%mm7,%mm4
		paddw		%mm4,%mm5
		movq		%mm4,24(%edx)
		pmaddwd		%mm4,%mm4
		leal		0(%ebx,%ecx,4),%ebx
		pavgb		%mm3,%mm2
		paddd		%mm1,%mm6
		paddd		%mm4,%mm6
		movq		%mm2,%mm4
		punpcklbw	%mm7,%mm2
		paddw		%mm2,%mm5
		movq		%mm2,272(%edx)
		pmaddwd		%mm2,%mm2
		punpckhbw	%mm7,%mm4
		paddw		%mm4,%mm5
		movq		%mm4,280(%edx)
		pmaddwd		%mm4,%mm4
		paddd		%mm2,%mm6
		leal		32(%edx),%edx
		paddd		%mm4,%mm6
		pmaddwd		cw1,%mm5
		pshufw		$238,%mm6,%mm4
		paddd		%mm4,%mm6
		pshufw		$238,%mm5,%mm4
		paddd		%mm4,%mm5
		pslld		$8,%mm6
		movd		%mm5,%ecx
		movd		%mm6,%eax
		imull		%ecx,%ecx
		subl		%ecx,%eax
		popl		%edi
		popl		%ebx
		popl		%esi
		ret

		.text
		.align		16
		.globl		pmmx_YUYV_0

pmmx_YUYV_0:
		imull		stride(%eax),%ecx;
		pushl		%ebx;
		leal		(%ecx,%edx,2),%ecx;	// row * stride + col * 2
		movq		cw255,%mm7;
		sall		$4,%ecx;
		pushl		%esi;
		movl		src(%eax),%esi;		// src
		addl		%ecx,%esi;		
		addl		offset(%eax),%esi;	// src + offset + row * 16 * stride + col * 32
		movl		$7,%ecx;
		movq		(%esi),%mm0;
		pxor		%mm6,%mm6;
		movl		stride(%eax),%edx;	// stride
		pxor		%mm5,%mm5;
		movl		dest(%eax),%eax;	// dest
		leal		512(%eax),%ebx;		// dest chroma
		movq		(%esi,%edx),%mm2;

		.p2align 4,,7
1:
		movq		%mm0,%mm3;			pand		%mm7,%mm0;
		psrlw		$8,%mm3;			paddw		%mm0,%mm5;	// v1u1v0u0
		movq		%mm0,(%eax);			pmaddwd		%mm0,%mm0;
		movq		%mm2,%mm1;			pand		%mm7,%mm2;
		psrlw		$8,%mm1;			paddw		%mm2,%mm5;
		paddw		%mm1,%mm3;    			paddd		%mm0,%mm6;
		movq		8(%esi),%mm0;			psrlw		$1,%mm3;
		movq		%mm2,16(%eax);			pmaddwd		%mm2,%mm2;
		movq		%mm0,%mm4;			pand		%mm7,%mm0;
		psrlw		$8,%mm4;			paddw		%mm0,%mm5;	// v3u3v2u2
		movq		%mm0,8(%eax);	    		paddd		%mm2,%mm6;
		movq		8(%esi,%edx),%mm2;		pmaddwd		%mm0,%mm0;
		movq		%mm2,%mm1;			pand		%mm7,%mm2;
		psrlw		$8,%mm1;			paddw		%mm2,%mm5;
		paddw		%mm1,%mm4;			paddd		%mm0,%mm6;
		movq		16(%esi),%mm0;			psrlw		$1,%mm4;
		movq		%mm3,%mm1;			punpcklwd	%mm4,%mm3;	// v2v0u2u0
		movq		%mm2,24(%eax);			punpckhwd	%mm4,%mm1;	// v3v1u3u1
		movq		%mm3,%mm4;			punpcklwd	%mm1,%mm3;	// v3v2v1v0
		pmaddwd		%mm2,%mm2;			punpckhwd	%mm1,%mm4;	// u3u2u1u0
		movq		%mm3,(%ebx);			movq		%mm0,%mm3;	
		movq		%mm4,128+0(%ebx);		pand		%mm7,%mm0;
		psrlw		$8,%mm3;			paddw		%mm0,%mm5;
		movq		%mm0,256(%eax);			paddd		%mm2,%mm6;
		movq		16(%esi,%edx),%mm2; 		pmaddwd		%mm0,%mm0;
		movq		%mm2,%mm1;			pand		%mm7,%mm2;
		psrlw		$8,%mm1;			paddw		%mm2,%mm5;
		paddw		%mm1,%mm3;			paddd		%mm0,%mm6;
		movq		24(%esi),%mm0;			psrlw		$1,%mm3;
		movq		%mm2,256+16(%eax);		pmaddwd		%mm2,%mm2;
		movq		%mm0,%mm4;			pand		%mm7,%mm0;
		psrlw		$8,%mm4;			paddw		%mm0,%mm5;
		movq		%mm0,256+8(%eax);		paddd		%mm2,%mm6;
		movq		24(%esi,%edx),%mm2;		pmaddwd		%mm0,%mm0;
		movq		%mm2,%mm1;			pand		%mm7,%mm2;
		psrlw		$8,%mm1;			paddw		%mm2,%mm5;
		paddw		%mm1,%mm4;			paddd		%mm0,%mm6;
		movq		(%esi,%edx,2),%mm0;		psrlw		$1,%mm4;
		movq		%mm2,256+24(%eax);		pmaddwd		%mm2,%mm2;
		leal		(%esi,%edx,2),%esi;		paddd		%mm2,%mm6;
		movq		%mm3,%mm1;			punpcklwd	%mm4,%mm3;
		movq		(%esi,%edx),%mm2;		punpckhwd	%mm4,%mm1;
		movq		%mm3,%mm4;			punpcklwd	%mm1,%mm3;
		addl		$32,%eax;			punpckhwd	%mm1,%mm4;
		movq		%mm3,8(%ebx);			decl		%ecx;
		movq		%mm4,128+8(%ebx);		leal		16(%ebx),%ebx;
		jne 		1b;

		movq		%mm0,%mm3;			pand		%mm7,%mm0;
		psrlw		$8,%mm3;			paddw		%mm0,%mm5;
		movq		%mm0,(%eax);			pmaddwd		%mm0,%mm0;
		movq		%mm2,%mm1;			pand		%mm7,%mm2;
		psrlw		$8,%mm1;			paddw		%mm2,%mm5;
		paddw		%mm1,%mm3;    			paddd		%mm0,%mm6;
		movq		8(%esi),%mm0;			psrlw		$1,%mm3;
		movq		%mm2,16(%eax);			pmaddwd		%mm2,%mm2;
		movq		%mm0,%mm4;			pand		%mm7,%mm0;
		psrlw		$8,%mm4;			paddw		%mm0,%mm5;
		movq		%mm0,8(%eax);	    		paddd		%mm2,%mm6;
		movq		8(%esi,%edx),%mm2;		pmaddwd		%mm0,%mm0;
		movq		%mm2,%mm1;			pand		%mm7,%mm2;
		psrlw		$8,%mm1;			paddw		%mm2,%mm5;
		paddw		%mm1,%mm4;			paddd		%mm0,%mm6;
		movq		16(%esi),%mm0;			psrlw		$1,%mm4;
		movq		%mm3,%mm1;			punpcklwd	%mm4,%mm3;
		movq		%mm2,24(%eax);			punpckhwd	%mm4,%mm1;
		movq		%mm3,%mm4;			punpcklwd	%mm1,%mm3;
		pmaddwd		%mm2,%mm2;			punpckhwd	%mm1,%mm4;
		movq		%mm3,(%ebx);			movq		%mm0,%mm3;	
		movq		%mm4,128+0(%ebx);		pand		%mm7,%mm0;
		psrlw		$8,%mm3;			paddw		%mm0,%mm5;
		movq		%mm0,256(%eax);			paddd		%mm2,%mm6;
		movq		16(%esi,%edx),%mm2; 		pmaddwd		%mm0,%mm0;
		movq		%mm2,%mm1;			pand		%mm7,%mm2;
		psrlw		$8,%mm1;			paddw		%mm2,%mm5;
		paddw		%mm1,%mm3;			paddd		%mm0,%mm6;
		movq		24(%esi),%mm0;			psrlw		$1,%mm3;
		movq		%mm2,256+16(%eax);		pmaddwd		%mm2,%mm2;
		movq		%mm0,%mm4;			pand		%mm7,%mm0;
		psrlw		$8,%mm4;			paddw		%mm0,%mm5;
		movq		%mm0,256+8(%eax);		paddd		%mm2,%mm6;
		movq		24(%esi,%edx),%mm2;		pmaddwd		%mm0,%mm0;
		movq		%mm2,%mm1;			pand		%mm7,%mm2;
		psrlw		$8,%mm1;			paddw		%mm2,%mm5;
		paddw		%mm1,%mm4;			paddd		%mm0,%mm6;
		movq		%mm3,%mm1;			psrlw		$1,%mm4;
		movq		%mm2,256+24(%eax);		pmaddwd		%mm2,%mm2;
		punpcklwd	%mm4,%mm3;			paddd		%mm2,%mm6;
		punpckhwd	%mm4,%mm1;			movq		%mm3,%mm4;
		pmaddwd		cw1,%mm5;			punpcklwd	%mm1,%mm3;
		movq		%mm6,%mm0;			punpckhwd	%mm1,%mm4;
		movq		%mm3,8(%ebx);			psrlq		$32,%mm6;
		movq		%mm5,%mm3;			psrlq		$32,%mm5;
		movq		%mm4,128+8(%ebx);		paddd		%mm0,%mm6;	
		paddd		%mm3,%mm5;			pslld		$8,%mm6;
		movd		%mm5,%edx;			popl		%esi;
		movd		%mm6,%eax;			imull		%edx,%edx;
		subl		%edx,%eax;			popl		%ebx;
		ret

		.text
		.align		16
		.globl		sse_YUYV_0

sse_YUYV_0: /* preliminary */

		imull		stride(%eax),%ecx;
		pushl		%ebx;
		leal		(%ecx,%edx,2),%ecx;	// row * stride + col * 2
		movq		cw255,%mm7;
		sall		$4,%ecx;
		pushl		%esi;
		movl		src(%eax),%esi;		// src
		addl		%ecx,%esi;		
		addl		offset(%eax),%esi;	// src + offset + row * 16 * stride + col * 32
		movl		$7,%ecx;
		movq		(%esi),%mm0;
		pxor		%mm6,%mm6;
		movl		stride(%eax),%edx;	// stride
		pxor		%mm5,%mm5;
		movl		dest(%eax),%eax;	// dest
		leal		512(%eax),%ebx;		// dest chroma
		movq		(%esi,%edx),%mm2;

		.p2align 4,,7
1:
		movq		%mm0,%mm3;
		pand		%mm7,%mm0;
		psrlw		$8,%mm3;
		paddw		%mm0,%mm5; 
		movq		%mm0,(%eax);
		pmaddwd		%mm0,%mm0;
		movq		%mm2,%mm1;
		pand		%mm7,%mm2;
		psrlw		$8,%mm1;
		paddw		%mm2,%mm5;
		paddd		%mm0,%mm6;
		movq		8(%esi),%mm0;
		pavgw		%mm1,%mm3;
		movq		%mm2,16(%eax);
		pmaddwd		%mm2,%mm2;
		movq		%mm0,%mm4;
		pand		%mm7,%mm0;
		psrlw		$8,%mm4;
		paddw		%mm0,%mm5;
		movq		%mm0,8(%eax);
		paddd		%mm2,%mm6;
		movq		8(%esi,%edx),%mm2;
		pmaddwd		%mm0,%mm0;
		movq		%mm2,%mm1;
		pand		%mm7,%mm2;
		psrlw		$8,%mm1;
		paddw		%mm2,%mm5;
		paddd		%mm0,%mm6;
		movq		16(%esi),%mm0;
		pavgw		%mm1,%mm4;
		movq		%mm3,%mm1;
		punpcklwd	%mm4,%mm3;
		movq		%mm2,24(%eax);
		punpckhwd	%mm4,%mm1;
		movq		%mm3,%mm4;
		punpcklwd	%mm1,%mm3;
		pmaddwd		%mm2,%mm2;
		punpckhwd	%mm1,%mm4;
		movq		%mm3,(%ebx);
		movq		%mm0,%mm3;	
		movq		%mm4,128+0(%ebx);
		pand		%mm7,%mm0;
		psrlw		$8,%mm3;
		paddw		%mm0,%mm5;
		movq		%mm0,256(%eax);
		paddd		%mm2,%mm6;
		movq		16(%esi,%edx),%mm2;
		pmaddwd		%mm0,%mm0;
		movq		%mm2,%mm1;
		pand		%mm7,%mm2;
		psrlw		$8,%mm1;
		paddw		%mm2,%mm5;
		paddd		%mm0,%mm6;
		movq		24(%esi),%mm0;
		pavgw		%mm1,%mm3;
		movq		%mm2,256+16(%eax);
		pmaddwd		%mm2,%mm2;
		movq		%mm0,%mm4;
		pand		%mm7,%mm0;
		psrlw		$8,%mm4;
		paddw		%mm0,%mm5;
		movq		%mm0,256+8(%eax);
		paddd		%mm2,%mm6;
		movq		24(%esi,%edx),%mm2;
		pmaddwd		%mm0,%mm0;
		movq		%mm2,%mm1;
		pand		%mm7,%mm2;
		psrlw		$8,%mm1;
		paddw		%mm2,%mm5;
		paddd		%mm0,%mm6;
		movq		(%esi,%edx,2),%mm0;
		pavgw		%mm1,%mm4;
		movq		%mm2,256+24(%eax);
		pmaddwd		%mm2,%mm2;
		addl		$32,%eax;
		leal		(%esi,%edx,2),%esi;
		paddd		%mm2,%mm6;
		movq		%mm3,%mm1;
		punpcklwd	%mm4,%mm3;
		movq		(%esi,%edx),%mm2;
		punpckhwd	%mm4,%mm1;
		movq		%mm3,%mm4;
		punpcklwd	%mm1,%mm3;
		punpckhwd	%mm1,%mm4;
		movq		%mm3,8(%ebx);
		decl		%ecx;
		movq		%mm4,128+8(%ebx);
		leal		16(%ebx),%ebx;
		jne 		1b;

		movq		%mm0,%mm3;
		pand		%mm7,%mm0;
		psrlw		$8,%mm3;
		paddw		%mm0,%mm5;
		movq		%mm0,(%eax);
		pmaddwd		%mm0,%mm0;
		movq		%mm2,%mm1;
		pand		%mm7,%mm2;
		psrlw		$8,%mm1;
		paddw		%mm2,%mm5;
		paddd		%mm0,%mm6;
		movq		8(%esi),%mm0;
		pavgw		%mm1,%mm3;
		movq		%mm2,16(%eax);
		pmaddwd		%mm2,%mm2;
		movq		%mm0,%mm4;
		pand		%mm7,%mm0;
		psrlw		$8,%mm4;
		paddw		%mm0,%mm5;
		movq		%mm0,8(%eax);
		paddd		%mm2,%mm6;
		movq		8(%esi,%edx),%mm2;
		pmaddwd		%mm0,%mm0;
		movq		%mm2,%mm1;
		pand		%mm7,%mm2;
		psrlw		$8,%mm1;
		paddw		%mm2,%mm5;
		paddd		%mm0,%mm6;
		movq		16(%esi),%mm0;
		pavgw		%mm1,%mm4;
		movq		%mm3,%mm1;
		punpcklwd	%mm4,%mm3;
		movq		%mm2,24(%eax);
		punpckhwd	%mm4,%mm1;
		movq		%mm3,%mm4;
		punpcklwd	%mm1,%mm3;
		pmaddwd		%mm2,%mm2;
		punpckhwd	%mm1,%mm4;
		movq		%mm3,(%ebx);
		movq		%mm0,%mm3;	
		movq		%mm4,128+0(%ebx);
		pand		%mm7,%mm0;
		psrlw		$8,%mm3;
		paddw		%mm0,%mm5;
		movq		%mm0,256(%eax);
		paddd		%mm2,%mm6;
		movq		16(%esi,%edx),%mm2;
		pmaddwd		%mm0,%mm0;
		movq		%mm2,%mm1;
		pand		%mm7,%mm2;
		psrlw		$8,%mm1;
		paddw		%mm2,%mm5;
		paddd		%mm0,%mm6;
		movq		24(%esi),%mm0;
		pavgw		%mm1,%mm3;
		movq		%mm2,256+16(%eax);
		pmaddwd		%mm2,%mm2;
		movq		%mm0,%mm4;
		pand		%mm7,%mm0;
		psrlw		$8,%mm4;
		paddw		%mm0,%mm5;
		movq		%mm0,256+8(%eax);
		paddd		%mm2,%mm6;
		movq		24(%esi,%edx),%mm2;
		pmaddwd		%mm0,%mm0;
		movq		%mm2,%mm1;
		pand		%mm7,%mm2;
		psrlw		$8,%mm1;
		paddw		%mm2,%mm5;
		paddd		%mm0,%mm6;
		pavgw		%mm1,%mm4;
		movq		%mm3,%mm1;
		movq		%mm2,256+24(%eax);
		pmaddwd		%mm2,%mm2;
		punpcklwd	%mm4,%mm3;
		paddd		%mm2,%mm6;
		punpckhwd	%mm4,%mm1;
		movq		%mm3,%mm4;
		pshufw		$238,%mm6,%mm2
		pmaddwd		cw1,%mm5
		punpcklwd	%mm1,%mm3;
		punpckhwd	%mm1,%mm4;
		paddd		%mm2,%mm6
		pshufw		$238,%mm5,%mm2
		paddd		%mm2,%mm5
		movq		%mm3,8(%ebx);
		pslld		$8,%mm6
		movd		%mm5,%ecx
		movq		%mm4,128+8(%ebx);
		imull		%ecx,%ecx
		movd		%mm6,%eax
		popl		%esi;
		subl		%ecx,%eax
		popl		%ebx;
		ret

		.text
		.align		16
		.globl		pmmx_YUYV_2

pmmx_YUYV_2: /* preliminary */

		imull		stride(%eax),%ecx;
		pushl		%ebx;
		addl		%edx,%ecx;		// row * stride + col
		sall		$5,%ecx;		// row * 16 * stride * 2 + col * 32
		pushl		%esi;
		movl		src(%eax),%esi;
		addl		%ecx,%esi;		
		addl		offset(%eax),%esi;	// src + offset + row * 32 * stride + col * 32
		movl		stride(%eax),%edx;	// stride
		pushl		%edi; 
		movl		dest(%eax),%edi;	// dest
		leal		512(%edi),%ebx;		// dest chroma
		leal		(%esi,%edx),%eax; 	// src2 = src1 + stride
		addl		%edx,%edx; 		// stride *= 2

		.align 16

pmmx_filter_s2t:
		movl		$8,%ecx;		pxor		%mm6,%mm6;
		movq		cw255,%mm5;		pxor		%mm7,%mm7;
1:
		movq		(%esi),%mm0;		leal		16(%ebx),%ebx;
		movq		(%eax),%mm1;		movq		%mm0,%mm2;
		pand		%mm5,%mm0;		movq		%mm1,%mm4;
		paddw		cw1,%mm0;		pand		%mm5,%mm1;
		movq		(%eax,%edx),%mm3;	paddw		%mm0,%mm1;
		movq		(%esi,%edx),%mm0;	psrlw		$1,%mm1;
		paddw		%mm1,%mm6;		psrlw		$8,%mm2;
		movq		%mm1,(%edi);		pmaddwd		%mm1,%mm1;
		decl		%ecx;			psrlw		$8,%mm4;
		paddw		%mm2,%mm4;		movq		%mm0,%mm2;
		paddd		%mm1,%mm7;		pand		%mm5,%mm0;
		paddw		cw1,%mm0;		movq		%mm3,%mm1;
		pand		%mm5,%mm1;		psrlw		$8,%mm2;
		paddw		%mm0,%mm1;		psrlw		$8,%mm3;
		movq		8(%esi),%mm0;		psrlw		$1,%mm1;
		paddw		%mm1,%mm6;		paddw		%mm2,%mm4;
		movq		%mm1,16(%edi);		pmaddwd		%mm1,%mm1;
		movq		8(%eax),%mm5;		movq		%mm0,%mm2;
		paddw		%mm3,%mm4;		movq		%mm5,%mm3;
		pand		cw255,%mm0;		paddd		%mm1,%mm7;
		paddw		cw1,%mm0;		psrlw		$8,%mm2;
		pand		cw255,%mm3;		psrlw		$8,%mm5;
		paddw		%mm0,%mm3;		paddw		%mm2,%mm5;
		movq		8(%esi,%edx),%mm0;	psrlw		$1,%mm3;
		paddw		%mm3,%mm6;		movq		%mm0,%mm2;
		movq		%mm3,8(%edi);		pmaddwd		%mm3,%mm3;
		pand		cw255,%mm0;		psrlw		$8,%mm2;
		movq		8(%eax,%edx),%mm1;	paddw		%mm2,%mm5;
		paddd		%mm3,%mm7;		movq		%mm1,%mm3;
		pand		cw255,%mm1;		psrlw		$8,%mm3;
		paddw		%mm0,%mm1;		paddw		%mm3,%mm5;
		paddw		cw1,%mm1;		movq		%mm4,%mm3;
		movq		16(%esi),%mm0;		psrlw		$1,%mm1;
		paddw		%mm1,%mm6;		punpcklwd	%mm5,%mm4;
		movq		%mm1,24(%edi);		pmaddwd		%mm1,%mm1;
		punpckhwd	%mm5,%mm3;		movq		%mm4,%mm5;
		movq		%mm0,%mm2;		punpcklwd	%mm3,%mm4;
		paddw		cw2,%mm4;		punpckhwd	%mm3,%mm5;
		paddw		cw2,%mm5;		paddd		%mm1,%mm7;
		movq		16(%eax),%mm1;		psraw		$2,%mm4;
		movq		cw255,%mm3;		psraw		$2,%mm5;
		movq		%mm4,-16(%ebx);		pand		%mm3,%mm0;
		movq		%mm5,128+0-16(%ebx);	movq		%mm1,%mm4;
		paddw		cw1,%mm0;		pand		%mm3,%mm1;
		paddw		%mm0,%mm1;		psrlw		$8,%mm2;
		movq		16(%esi,%edx),%mm0;	psrlw		$1,%mm1;
		movq		%mm1,256(%edi);		paddw		%mm1,%mm6;
		pmaddwd		%mm1,%mm1;		psrlw		$8,%mm4;
		movq		16(%eax,%edx),%mm5;	paddw		%mm2,%mm4;
		movq		%mm0,%mm2;		pand		%mm3,%mm0;
		paddd		%mm1,%mm7;		movq		%mm5,%mm1;
		pand		%mm3,%mm1;		psrlw		$8,%mm2;
		paddw		%mm0,%mm1;		psrlw		$8,%mm5;
		paddw		cw1,%mm1;		paddw		%mm2,%mm4;
		movq		24(%esi),%mm0;		psrlw		$1,%mm1;
		movq		%mm1,256+16(%edi);	paddw		%mm1,%mm6;
		pmaddwd		%mm1,%mm1;		paddw		%mm5,%mm4;
		movq		24(%eax),%mm3;		movq		%mm0,%mm2;
		pand		cw255,%mm0;		movq		%mm3,%mm5;
		paddw		cw1,%mm0;		psrlw		$8,%mm2;
		pand		cw255,%mm5;		psrlw		$8,%mm3;
		paddw		%mm0,%mm5;		paddw		%mm2,%mm3;
		movq		24(%esi,%edx),%mm0;	psrlw		$1,%mm5;
		movq		%mm5,256+8(%edi);	paddw		%mm5,%mm6;
		paddd		%mm1,%mm7;		pmaddwd		%mm5,%mm5;
		movq		24(%eax,%edx),%mm1;	movq		%mm0,%mm2;
		psrlw		$8,%mm2;		leal		(%esi,%edx,2),%esi;
		pand		cw255,%mm0;		paddw		%mm2,%mm3;
		paddd		%mm5,%mm7;		leal		(%eax,%edx,2),%eax;
		movq		%mm1,%mm5;		movq		%mm4,%mm2;
		pand		cw255,%mm1;		psrlw		$8,%mm5;
		paddw		%mm5,%mm3;		paddw		%mm0,%mm1;
		paddw		cw1,%mm1;		punpcklwd	%mm3,%mm4;
		psrlw		$1,%mm1;		punpckhwd	%mm3,%mm2;
		movq		%mm4,%mm3;		punpcklwd	%mm2,%mm4;
		paddw		%mm1,%mm6;		punpckhwd	%mm2,%mm3;
		movq		%mm1,256+24(%edi);	pmaddwd		%mm1,%mm1;
		paddw		cw2,%mm4;		leal		32(%edi),%edi;
		paddw		cw2,%mm3;		psraw		$2,%mm4;
		movq		cw255,%mm5;		psraw		$2,%mm3;
		movq		%mm4,8-16(%ebx);	paddd		%mm1,%mm7;
		movq		%mm3,128+8-16(%ebx);	jne		1b;
		pmaddwd		cw1,%mm6;		movq		%mm7,%mm0;
		psrlq		$32,%mm7;		popl		%edi;
		paddd		%mm0,%mm7;		popl		%esi;
		movq		%mm6,%mm5;		psrlq		$32,%mm6;
		paddd		%mm5,%mm6;		pslld		$8,%mm7;
		movd		%mm6,%edx;		popl		%ebx;
		movd		%mm7,%eax;		imul		%edx,%edx;
		subl		%edx,%eax;
		ret

		.text
		.align		16
		.globl		pmmx_YUYV_6

pmmx_YUYV_6: /* preliminary */
		cmpl		mb_last_row,%ecx; 	// FIXME
		jge		pmmx_YUYV_0;
		imull		stride(%eax),%ecx;
		pushl		%ebx;
		leal		(%ecx,%edx,2),%ecx;
		sall		$4,%ecx;		// row * 16 * stride + col * 32
		pushl		%esi;
		movl		src(%eax),%esi;
		addl		%ecx,%esi;		
		addl		offset(%eax),%esi;	// src + offset + row * 16 * stride + col * 32
		movl		stride(%eax),%edx;	// stride
		pushl		%edi; 
		movl		dest(%eax),%edi;	// dest
		leal		512(%edi),%ebx;		// dest chroma
		leal		(%esi,%edx),%eax; 	// src2 = src1 + stride
		jmp		pmmx_filter_s2t;

		.text
		.align		16
		.globl		sse_YUYV_2

sse_YUYV_2: /* preliminary */

		imull		stride(%eax),%ecx;
		pushl		%ebx;
		addl		%edx,%ecx;		// row * stride + col
		sall		$5,%ecx;		// row * 16 * stride * 2 + col * 32
		pushl		%esi;
		movl		src(%eax),%esi;
		addl		%ecx,%esi;		
		addl		offset(%eax),%esi;	// src + offset + row * 32 * stride + col * 32
		movl		stride(%eax),%edx;	// stride
		pushl		%edi; 
		movl		dest(%eax),%edi;	// dest
		leal		512(%edi),%ebx;		// dest chroma
		leal		(%esi,%edx),%eax; 	// src2 = src1 + stride
		addl		%edx,%edx; 		// stride *= 2

		.align 16

sse_filter_s2t:
		movl		$8,%ecx;
		pxor		%mm6,%mm6;
		movq		cw255,%mm5;
		pxor		%mm7,%mm7;
1:
		movq		(%esi),%mm0;
		leal		16(%ebx),%ebx;
		movq		(%eax),%mm1;
		movq		%mm0,%mm2;
		pand		%mm5,%mm0;
		movq		%mm1,%mm4;
		pand		%mm5,%mm1;
		movq		(%eax,%edx),%mm3;
		pavgw		%mm0,%mm1;
		movq		(%esi,%edx),%mm0;
		paddw		%mm1,%mm6;
		psrlw		$8,%mm2;
		movq		%mm1,(%edi);
		pmaddwd		%mm1,%mm1;
		decl		%ecx;
		psrlw		$8,%mm4;
		paddw		%mm2,%mm4;
		movq		%mm0,%mm2;
		paddd		%mm1,%mm7;
		pand		%mm5,%mm0;
		movq		%mm3,%mm1;
		pand		%mm5,%mm1;
		psrlw		$8,%mm2;
		pavgw		%mm0,%mm1;
		psrlw		$8,%mm3;
		movq		8(%esi),%mm0;
		paddw		%mm1,%mm6;
		paddw		%mm2,%mm4;
		movq		%mm1,16(%edi);
		pmaddwd		%mm1,%mm1;
		movq		8(%eax),%mm5;
		movq		%mm0,%mm2;
		paddw		%mm3,%mm4;
		movq		%mm5,%mm3;
		pand		cw255,%mm0;
		paddd		%mm1,%mm7;
		psrlw		$8,%mm2;
		pand		cw255,%mm3;
		psrlw		$8,%mm5;
		pavgw		%mm0,%mm3;
		paddw		%mm2,%mm5;
		movq		8(%esi,%edx),%mm0;
		paddw		%mm3,%mm6;
		movq		%mm0,%mm2;
		movq		%mm3,8(%edi);
		pmaddwd		%mm3,%mm3;
		pand		cw255,%mm0;
		psrlw		$8,%mm2;
		movq		8(%eax,%edx),%mm1;
		paddw		%mm2,%mm5;
		paddd		%mm3,%mm7;
		movq		%mm1,%mm3;
		pand		cw255,%mm1;
		psrlw		$8,%mm3;
		pavgw		%mm0,%mm1;
		paddw		%mm3,%mm5;
		movq		%mm4,%mm3;
		movq		16(%esi),%mm0;
		paddw		%mm1,%mm6;
		punpcklwd	%mm5,%mm4;
		movq		%mm1,24(%edi);
		pmaddwd		%mm1,%mm1;
		punpckhwd	%mm5,%mm3;
		movq		%mm4,%mm5;
		movq		%mm0,%mm2;
		punpcklwd	%mm3,%mm4;
		paddw		cw2,%mm4;
		punpckhwd	%mm3,%mm5;
		paddw		cw2,%mm5;
		paddd		%mm1,%mm7;
		movq		16(%eax),%mm1;
		psraw		$2,%mm4;
		movq		cw255,%mm3;
		psraw		$2,%mm5;
		movq		%mm4,-16(%ebx);
		pand		%mm3,%mm0;
		movq		%mm5,128+0-16(%ebx);
		movq		%mm1,%mm4;
		pand		%mm3,%mm1;
		pavgw		%mm0,%mm1;
		psrlw		$8,%mm2;
		movq		16(%esi,%edx),%mm0;
		movq		%mm1,256(%edi);
		paddw		%mm1,%mm6;
		pmaddwd		%mm1,%mm1;
		psrlw		$8,%mm4;
		movq		16(%eax,%edx),%mm5;
		paddw		%mm2,%mm4;
		movq		%mm0,%mm2;
		pand		%mm3,%mm0;
		paddd		%mm1,%mm7;
		movq		%mm5,%mm1;
		pand		%mm3,%mm1;
		psrlw		$8,%mm2;
		pavgw		%mm0,%mm1;
		psrlw		$8,%mm5;
		paddw		%mm2,%mm4;
		movq		24(%esi),%mm0;
		movq		%mm1,256+16(%edi);
		paddw		%mm1,%mm6;
		pmaddwd		%mm1,%mm1;
		paddw		%mm5,%mm4;
		movq		24(%eax),%mm3;
		movq		%mm0,%mm2;
		pand		cw255,%mm0;
		movq		%mm3,%mm5;
		psrlw		$8,%mm2;
		pand		cw255,%mm5;
		psrlw		$8,%mm3;
		pavgw		%mm0,%mm5;
		paddw		%mm2,%mm3;
		movq		24(%esi,%edx),%mm0;
		movq		%mm5,256+8(%edi);
		paddw		%mm5,%mm6;
		paddd		%mm1,%mm7;
		pmaddwd		%mm5,%mm5;
		movq		24(%eax,%edx),%mm1;
		movq		%mm0,%mm2;
		psrlw		$8,%mm2;
		leal		(%esi,%edx,2),%esi;
		pand		cw255,%mm0;
		paddw		%mm2,%mm3;
		paddd		%mm5,%mm7;
		leal		(%eax,%edx,2),%eax;
		movq		%mm1,%mm5;
		movq		%mm4,%mm2;
		pand		cw255,%mm1;
		psrlw		$8,%mm5;
		paddw		%mm5,%mm3;
		pavgw		%mm0,%mm1;
		punpcklwd	%mm3,%mm4;
		punpckhwd	%mm3,%mm2;
		movq		%mm4,%mm3;
		punpcklwd	%mm2,%mm4;
		paddw		%mm1,%mm6;
		punpckhwd	%mm2,%mm3;
		movq		%mm1,256+24(%edi);
		pmaddwd		%mm1,%mm1;
		paddw		cw2,%mm4;
		leal		32(%edi),%edi;
		paddw		cw2,%mm3;
		psraw		$2,%mm4;
		movq		cw255,%mm5;
		psraw		$2,%mm3;
		movq		%mm4,8-16(%ebx);
		paddd		%mm1,%mm7;
		movq		%mm3,128+8-16(%ebx);
		jne		1b;
		pmaddwd		cw1,%mm6
		popl		%edi
		pshufw		$238,%mm7,%mm4
		paddd		%mm4,%mm7
		pshufw		$238,%mm6,%mm4
		paddd		%mm4,%mm6
		pslld		$8,%mm7
		movd		%mm6,%ecx
		popl		%esi
		movd		%mm7,%eax
		imull		%ecx,%ecx
		popl		%ebx
		subl		%ecx,%eax
		ret

		.text
		.align		16
		.globl		sse_YUYV_6

sse_YUYV_6: /* preliminary */
		cmpl		mb_last_row,%ecx; 	// FIXME
		jge		sse_YUYV_0;
		imull		stride(%eax),%ecx;
		pushl		%ebx;
		leal		(%ecx,%edx,2),%ecx;
		sall		$4,%ecx;		// row * 16 * stride + col * 32
		pushl		%esi;
		movl		src(%eax),%esi;
		addl		%ecx,%esi;		
		addl		offset(%eax),%esi;	// src + offset + row * 16 * stride + col * 32
		movl		stride(%eax),%edx;	// stride
		pushl		%edi; 
		movl		dest(%eax),%edi;	// dest
		leal		512(%edi),%ebx;		// dest chroma
		leal		(%esi,%edx),%eax; 	// src2 = src1 + stride
		jmp		sse_filter_s2t;

		.text
		.align	16
		.globl	pmmx_YUYV_3

pmmx_YUYV_3: /* preliminary */
		subl		$12,%esp
		movl		%esi,2*4(%esp)
		movl		24(%eax),%esi
		shll		$2,%ecx
		imull		%esi,%ecx
		movl		%ebx,1*4(%esp)
		movl		4(%eax),%ebx
		movl		%edi,(%esp)
		addl		12(%eax),%ebx
		leal		0(%ecx,%edx,8),%ecx
		movl		(%eax),%edx
		leal		0(%ebx,%ecx,8),%ebx
		leal		512(%edx),%ecx
		pxor		%mm5,%mm5
		movl		$8,%eax
		pxor		%mm6,%mm6
		movq		cw255,%mm7
		leal		(%esi,%esi,2),%edi
		.p2align	4,,7
1:
		pxor		%mm4,%mm4
		pxor		%mm3,%mm3
		movq		(%ebx),%mm0
		movq		8(%ebx),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm0,%mm3
		paddw		%mm2,%mm3
		movq		0(%ebx,%esi),%mm0
		movq		8(%ebx,%esi),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm0,%mm3
		paddw		%mm2,%mm3
		paddw		cw2,%mm3
		psrlw		$2,%mm3
		paddw		%mm3,%mm5
		movq		%mm3,(%edx)
		pmaddwd		%mm3,%mm3
		paddd		%mm3,%mm6
		pxor		%mm3,%mm3
		movq		0(%ebx,%esi,2),%mm0
		movq		8(%ebx,%esi,2),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm0,%mm3
		paddw		%mm2,%mm3
		movq		0(%ebx,%edi),%mm0
		movq		8(%ebx,%edi),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm0,%mm3
		paddw		%mm2,%mm3
		paddw		cw2,%mm3
		psrlw		$2,%mm3
		paddw		%mm3,%mm5
		movq		%mm3,16(%edx)
		pmaddwd		%mm3,%mm3
		paddd		%mm3,%mm6
		movq		%mm4,(%ecx)
		pxor		%mm4,%mm4
		pxor		%mm3,%mm3
		movq		16(%ebx),%mm0
		movq		24(%ebx),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm0,%mm3
		paddw		%mm2,%mm3
		movq		16(%ebx,%esi),%mm0
		movq		24(%ebx,%esi),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm0,%mm3
		paddw		%mm2,%mm3
		paddw		cw2,%mm3
		psrlw		$2,%mm3
		paddw		%mm3,%mm5
		movq		%mm3,8(%edx)
		pmaddwd		%mm3,%mm3
		paddd		%mm3,%mm6
		pxor		%mm3,%mm3
		movq		16(%ebx,%esi,2),%mm0
		movq		24(%ebx,%esi,2),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm0,%mm3
		paddw		%mm2,%mm3
		movq		16(%ebx,%edi),%mm0
		movq		24(%ebx,%edi),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm0,%mm3
		paddw		%mm2,%mm3
		paddw		cw2,%mm3
		psrlw		$2,%mm3
		paddw		%mm3,%mm5
		movq		%mm3,24(%edx)
		pmaddwd		%mm3,%mm3
		paddd		%mm3,%mm6
		movq		(%ecx),%mm3
		movq		cw4,%mm2
		paddw		%mm2,%mm3
		psrlw		$3,%mm3
		paddw		%mm2,%mm4
		psrlw		$3,%mm4
		movq		%mm3,%mm2
		punpcklwd	%mm4,%mm3
		punpckhwd	%mm4,%mm2
		movq		%mm3,%mm4
		punpcklwd	%mm2,%mm3
		punpckhwd	%mm2,%mm4
		movq		%mm3,(%ecx)
		movq		%mm4,128(%ecx)
		pxor		%mm4,%mm4
		pxor		%mm3,%mm3
		movq		32(%ebx),%mm0
		movq		40(%ebx),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm0,%mm3
		paddw		%mm2,%mm3
		movq		32(%ebx,%esi),%mm0
		movq		40(%ebx,%esi),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm0,%mm3
		paddw		%mm2,%mm3
		paddw		cw2,%mm3
		psrlw		$2,%mm3
		paddw		%mm3,%mm5
		movq		%mm3,256(%edx)
		pmaddwd		%mm3,%mm3
		paddd		%mm3,%mm6
		pxor		%mm3,%mm3
		movq		32(%ebx,%esi,2),%mm0
		movq		40(%ebx,%esi,2),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm0,%mm3
		paddw		%mm2,%mm3
		movq		32(%ebx,%edi),%mm0
		movq		40(%ebx,%edi),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm0,%mm3
		paddw		%mm2,%mm3
		paddw		cw2,%mm3
		psrlw		$2,%mm3
		paddw		%mm3,%mm5
		movq		%mm3,272(%edx)
		pmaddwd		%mm3,%mm3
		paddd		%mm3,%mm6
		movq		%mm4,8(%ecx)
		pxor		%mm4,%mm4
		pxor		%mm3,%mm3
		movq		48(%ebx),%mm0
		movq		56(%ebx),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm0,%mm3
		paddw		%mm2,%mm3
		movq		48(%ebx,%esi),%mm0
		movq		56(%ebx,%esi),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm0,%mm3
		paddw		%mm2,%mm3
		paddw		cw2,%mm3
		psrlw		$2,%mm3
		paddw		%mm3,%mm5
		movq		%mm3,264(%edx)
		pmaddwd		%mm3,%mm3
		paddd		%mm3,%mm6
		pxor		%mm3,%mm3
		movq		48(%ebx,%esi,2),%mm0
		movq		56(%ebx,%esi,2),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm0,%mm3
		paddw		%mm2,%mm3
		movq		48(%ebx,%edi),%mm0
		movq		56(%ebx,%edi),%mm1
		leal		0(%ebx,%esi,4),%ebx
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm0,%mm3
		paddw		%mm2,%mm3
		paddw		cw2,%mm3
		psrlw		$2,%mm3
		paddw		%mm3,%mm5
		movq		%mm3,280(%edx)
		pmaddwd		%mm3,%mm3
		addl		$32,%edx
		paddd		%mm3,%mm6
		movq		8(%ecx),%mm3
		movq		cw4,%mm2
		paddw		%mm2,%mm3
		psrlw		$3,%mm3
		paddw		%mm2,%mm4
		psrlw		$3,%mm4
		movq		%mm3,%mm2
		punpcklwd	%mm4,%mm3
		punpckhwd	%mm4,%mm2
		movq		%mm3,%mm4
		punpcklwd	%mm2,%mm3
		punpckhwd	%mm2,%mm4
		movq		%mm3,8(%ecx)
		movq		%mm4,136(%ecx)
		addl		$16,%ecx
		decl		%eax
		jne		1b
		pmaddwd		cw1,%mm5
		movq		%mm6,%mm4
		psrlq		$32,%mm6
		movq		%mm5,%mm2
		paddd		%mm4,%mm6
		psrlq		$32,%mm5
		pslld		$8,%mm6
		paddd		%mm2,%mm5
		movd		%mm5,%ecx
		imull		%ecx,%ecx
		movd		%mm6,%eax
		subl		%ecx,%eax
		popl		%edi
		popl		%ebx
		popl		%esi
		ret

		.text
		.align	16
		.globl	pmmx_YUYV_1

pmmx_YUYV_1: /* preliminary */
		subl		$12,%esp
		movl		%esi,2*4(%esp)
		movl		24(%eax),%esi
		addl		%ecx,%ecx
		imull		%esi,%ecx
		movl		%ebx,1*4(%esp)
		movl		4(%eax),%ebx
		movl		%edi,(%esp)
		addl		12(%eax),%ebx
		leal		0(%ecx,%edx,8),%ecx
		movl		(%eax),%edx
		leal		0(%ebx,%ecx,8),%ebx
		leal		512(%edx),%ecx
		pxor		%mm5,%mm5
		movl		$8,%eax
		pxor		%mm6,%mm6
		movq		cw255,%mm7
		leal		(%esi,%esi,2),%edi
		.p2align	4,,7
1:
		pxor		%mm3,%mm3
		movq		(%ebx),%mm0
		movq		8(%ebx),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm3
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm3
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm2,%mm0
		paddw		cw1,%mm0
		psrlw		$1,%mm0
		paddw		%mm0,%mm5
		movq		%mm0,(%edx)
		pmaddwd		%mm0,%mm0
		paddd		%mm0,%mm6
		movq		0(%ebx,%esi),%mm0
		movq		8(%ebx,%esi),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm3
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm3
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm2,%mm0
		paddw		cw1,%mm0
		psrlw		$1,%mm0
		paddw		%mm0,%mm5
		movq		%mm0,16(%edx)
		pmaddwd		%mm0,%mm0
		paddd		%mm0,%mm6
		pxor		%mm4,%mm4
		movq		16(%ebx),%mm0
		movq		24(%ebx),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm2,%mm0
		paddw		cw1,%mm0
		psrlw		$1,%mm0
		paddw		%mm0,%mm5
		movq		%mm0,8(%edx)
		pmaddwd		%mm0,%mm0
		paddd		%mm0,%mm6
		movq		16(%ebx,%esi),%mm0
		movq		24(%ebx,%esi),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm2,%mm0
		paddw		cw1,%mm0
		psrlw		$1,%mm0
		paddw		%mm0,%mm5
		movq		%mm0,24(%edx)
		pmaddwd		%mm0,%mm0
		paddd		%mm0,%mm6
		movq		cw2,%mm2
		paddw		%mm2,%mm3
		psrlw		$2,%mm3
		paddw		%mm2,%mm4
		psrlw		$2,%mm4
		movq		%mm3,%mm2
		punpcklwd	%mm4,%mm3
		punpckhwd	%mm4,%mm2
		movq		%mm3,%mm4
		punpcklwd	%mm2,%mm3
		punpckhwd	%mm2,%mm4
		movq		%mm3,(%ecx)
		movq		%mm4,128(%ecx)
		pxor		%mm3,%mm3
		movq		32(%ebx),%mm0
		movq		40(%ebx),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm3
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm3
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm2,%mm0
		paddw		cw1,%mm0
		psrlw		$1,%mm0
		paddw		%mm0,%mm5
		movq		%mm0,256(%edx)
		pmaddwd		%mm0,%mm0
		paddd		%mm0,%mm6
		movq		32(%ebx,%esi),%mm0
		movq		40(%ebx,%esi),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm3
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm3
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm2,%mm0
		paddw		cw1,%mm0
		psrlw		$1,%mm0
		paddw		%mm0,%mm5
		movq		%mm0,272(%edx)
		pmaddwd		%mm0,%mm0
		paddd		%mm0,%mm6
		pxor		%mm4,%mm4
		movq		48(%ebx),%mm0
		movq		56(%ebx),%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm2,%mm0
		paddw		cw1,%mm0
		psrlw		$1,%mm0
		paddw		%mm0,%mm5
		movq		%mm0,264(%edx)
		pmaddwd		%mm0,%mm0
		paddd		%mm0,%mm6
		movq		48(%ebx,%esi),%mm0
		movq		56(%ebx,%esi),%mm1
		leal		0(%ebx,%esi,2),%ebx
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		movq		%mm0,%mm1
		pand		%mm7,%mm0
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm2,%mm1
		pand		%mm7,%mm2
		psrlw		$8,%mm1
		paddw		%mm1,%mm4
		movq		%mm0,%mm1
		punpcklwd	%mm2,%mm0
		punpckhwd	%mm2,%mm1
		movq		%mm0,%mm2
		punpckldq	%mm1,%mm0
		punpckhdq	%mm1,%mm2
		paddw		%mm2,%mm0
		paddw		cw1,%mm0
		psrlw		$1,%mm0
		paddw		%mm0,%mm5
		movq		%mm0,280(%edx)
		pmaddwd		%mm0,%mm0
		addl		$32,%edx
		paddd		%mm0,%mm6
		movq		cw2,%mm2
		paddw		%mm2,%mm3
		psrlw		$2,%mm3
		paddw		%mm2,%mm4
		psrlw		$2,%mm4
		movq		%mm3,%mm2
		punpcklwd	%mm4,%mm3
		punpckhwd	%mm4,%mm2
		movq		%mm3,%mm4
		punpcklwd	%mm2,%mm3
		punpckhwd	%mm2,%mm4
		movq		%mm3,8(%ecx)
		movq		%mm4,136(%ecx)
		addl		$16,%ecx
		decl		%eax
		jne		1b
		pmaddwd		cw1,%mm5
		movq		%mm6,%mm4
		psrlq		$32,%mm6
		movq		%mm5,%mm2
		paddd		%mm4,%mm6
		psrlq		$32,%mm5
		pslld		$8,%mm6
		paddd		%mm2,%mm5
		movd		%mm5,%ecx
		imull		%ecx,%ecx
		movd		%mm6,%eax
		subl		%ecx,%eax
		popl		%edi
		popl		%ebx
		popl		%esi
		ret
