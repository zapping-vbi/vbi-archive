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

# $Id: vlc_mmx.s,v 1.3 2001-10-07 10:55:51 mschimek Exp $

# int
# p6_mpeg1_encode_intra(void)

	.text
	.align		16
	.globl		mp1e_p6_mpeg1_encode_intra

mp1e_p6_mpeg1_encode_intra:

	pushl		%ebp;
	pushl		%edi;
	pushl		%edx;
	leal		mp1e_dc_vlc_intra,%edi;
	pushl		%esi;
	leal		mblock+0*128+768,%esi;
	pushl		%ebx;
	movl		video_out,%ebp;
	pushl		%ecx;
	movl		dc_dct_pred,%ebx;
	call		1f;
	movswl		mblock+0*128+768,%ebx;
	leal		mblock+2*128+768,%esi;
	leal		mp1e_dc_vlc_intra+12*8,%edi;
	call		1f;
	movswl		mblock+2*128+768,%ebx;
	leal		mblock+1*128+768,%esi;
	leal		mp1e_dc_vlc_intra+12*8,%edi;
	call		1f;
	movswl		mblock+1*128+768,%ebx;
	leal		mblock+3*128+768,%esi;
	leal		mp1e_dc_vlc_intra+12*8,%edi;
	call		1f;
	movl		dc_dct_pred+4,%ebx;
	leal		mblock+4*128+768,%esi;
	leal		mp1e_dc_vlc_intra+24*8,%edi;
	call		1f;
	movl		dc_dct_pred+8,%ebx;
	leal		mblock+5*128+768,%esi;
	leal		mp1e_dc_vlc_intra+24*8,%edi;
	call		1f;
	movswl		mblock+3*128+768,%eax;
	movswl		mblock+4*128+768,%ebx;
	movl		%eax,dc_dct_pred;
	movswl		mblock+5*128+768,%ecx;
	movl		%ebx,dc_dct_pred+4;
	movl		%ecx,dc_dct_pred+8;
	movl		%ebp,video_out;
	movl		$video_out,%eax;
	movl		$2,%ecx;
	movl		$2,%edx;
	call		mmx_bputl;
	movl		(%esp),%ecx;
	movl		4(%esp),%ebx;
	movl		8(%esp),%esi;
	xorl		%eax,%eax;
	movl		12(%esp),%edx;
	movl		16(%esp),%edi;
	movl		20(%esp),%ebp;
	leal		24(%esp),%esp;
	ret;

	.align	16

1:	movd		%esp,%mm6;
	movl		$0,%ecx;
	movswl		(%esi),%eax;			
	subl		%ebx,%eax;
	movl		%eax,%ebx;
	cdq;
	xorl		%edx,%eax;
	subl		%edx,%eax;			
	bsrl		%eax,%ecx;
	setnz		%al;
	addl		%edx,%ebx;
	movl		$-63,%esp;
	addb		%al,%cl;
	sall		%cl,%edx;
	xorl		%edx,%ebx;			
	orl		(%edi,%ecx,8),%ebx;
	addl		4(%edi,%ecx,8),%ebp;
	jmp		4f;

	.align 16

2:	movswl		(%esi,%ebx,2),%eax;		
	movzbl		1(%edi),%ecx;
	testl		%eax,%eax;			
	jne		3f;
	movzbl		mp1e_iscan+63(%esp),%ebx;		
	incl		%esp;
	leal		(%edi,%ecx,2),%edi;
	jle		2b;
	movd		%mm6,%esp;
	ret;

3:	cdq;
	xorl		%edx,%eax;
	subl		%edx,%eax;			
	cmpl		%ecx,%eax;			
	jge		5f;
	movzbl		(%edi,%eax,2),%ebx;
	movzbl		1(%edi,%eax,2),%ecx;			
	subl		%edx,%ebx;
	addl		%ecx,%ebp;
4:	movl		$64,%edi;
	movd		%ebx,%mm2;			
	subl		%ebp,%edi;
	movd		%edi,%mm1;			
	jle		7f;
	leal		mp1e_ac_vlc_zero,%edi;
	psllq		%mm1,%mm2;
	movzbl		mp1e_iscan+63(%esp),%ebx;		
	incl		%esp;			
	por		%mm2,%mm7;
	jle		2b;
	movd		%mm6,%esp;
	ret;

5:	movzbl		(%edi),%ecx;			
	movswl		(%esi,%ebx,2),%edx;		
	cmpl		$127,%eax;
	jg		6f;
	andl		$255,%edx;			
	sall		$8,%ecx;
	leal		16384(%ecx,%edx),%ebx;
	addl		$20,%ebp;
	jmp		4b;

6:	sall		$16,%ecx;			
	andl		$33023,%edx;			
	cmpl		$255,%eax;			
	leal		4194304(%ecx,%edx),%ebx;
	addl		$28,%ebp;
	jle		4b;

	movd		%mm6,%esp;
	addl		$4,%esp;
	movl		$1,%eax;
	popl		%ecx;				
	popl		%ebx;
	popl		%esi;				
	popl		%edx;
	popl		%edi;				
	popl		%ebp;
	ret;

	.align 16

7:	movq		video_out+16,%mm3;		
	movq		%mm2,%mm5;
	leal		mp1e_ac_vlc_zero,%edi;		
	pxor		%mm4,%mm4;
	psubd		%mm1,%mm4;
	movd		%mm4,%ebp;			
	psubd		%mm4,%mm3;			
	psrld		%mm4,%mm5;
	movl		video_out+4,%ecx;
	por		%mm5,%mm7;			
	movd		%mm7,%eax;			
	movzbl		mp1e_iscan+63(%esp),%ebx;		
	psrlq		$32,%mm7;
	bswap		%eax;
	leal		8(%ecx),%edx;
	movl		%eax,4(%ecx);
	movd		%mm7,%eax;			
	bswap		%eax;
	psllq		%mm3,%mm2;
	incl		%esp;
	movq		%mm2,%mm7;
	movl		%eax,(%ecx);			
	movl		%edx,video_out+4;		
	jle		2b;
	movd		%mm6,%esp;
	ret;

# int
# p6_mpeg1_encode_inter(short mblock[6][8][8], unsigned int cbp)

	.text
	.align		16
	.globl		mp1e_p6_mpeg1_encode_inter

mp1e_p6_mpeg1_encode_inter:

	testl		$32,1*4+4(%esp);
	pushl		%esi
	movl		2*4+0(%esp),%esi;
	pushl		%ebp
	pushl		%edi
	pushl		%ebx
	je		2f;
	call		1f;
	movl		5*4+0(%esp),%esi;
2:	testl		$8,5*4+4(%esp);
	je		2f;
	leal		2*128(%esi),%esi;
	call		1f;
	movl		5*4+0(%esp),%esi;
2:	testl		$16,5*4+4(%esp);
	je		2f;
	leal		1*128(%esi),%esi;
	call		1f;
	movl		5*4+0(%esp),%esi;
2:	testl		$4,5*4+4(%esp);
	je		2f;
	leal		3*128(%esi),%esi;
	call		1f;
	movl		5*4+0(%esp),%esi;
2:	testl		$2,5*4+4(%esp);
	je		2f;
	leal		4*128(%esi),%esi;
	call		1f;
	movl		5*4+0(%esp),%esi;
2:	testl		$1,5*4+4(%esp);
	je		2f;
	leal		5*128(%esi),%esi;
	call		1f;
2:
	xorl		%eax,%eax
	popl		%ebx
	popl		%edi
	popl		%ebp
	popl		%esi
	ret

	.align	16

1:	movswl		(%esi),%eax;
	movl		$0,%ebp;
	movd		%esp,%mm6;	
	movl		video_out,%ebx;
	movl		$-63,%esp;
	leal		mp1e_ac_vlc_zero,%edi;		
	cdq;
	xorl		%edx,%eax;			
	subl		%edx,%eax;
	decl		%eax;
	jne		3f
	movl		$2,%ebp;
	subl		%edx,%ebp;
	addl		$2,%ebx;
	jmp		9f;

	.align 16

3:	movswl		(%esi,%ebp,2),%eax;		
	testl		%eax,%eax;			
	movzbl		1(%edi),%ecx;
	jne		4f;
	movzbl		mp1e_iscan+63(%esp),%ebp;		
	incl		%esp;
	leal		(%edi,%ecx,2),%edi;		
	jle		3b;

0:	movl		%ebx,video_out;
	movd		%mm6,%esp;
	movl		$video_out,%eax;
	movl		$2,%ecx;
	movl		$2,%edx;		
	jmp		mmx_bputl;

4:	cdq;
	xorl		%edx,%eax;
	subl		%edx,%eax;
	cmpl		%ecx,%eax;			
	jge		5f;
	movzbl		(%edi,%eax,2),%ebp;
	addb		1(%edi,%eax,2),%bl;
	subl		%edx,%ebp;
9:	movl		$64,%edi;
	movd		%ebp,%mm2;
	subl		%ebx,%edi;
	movd		%edi,%mm1;		
	jle		8f;
	leal		mp1e_ac_vlc_zero,%edi;
	movzbl		mp1e_iscan+63(%esp),%ebp;		
	psllq		%mm1,%mm2;
	incl		%esp;
	por		%mm2,%mm7;
	jle 		3b;
	jmp		0b;

	.align 16

5:	movswl		(%esi,%ebp,2),%ebp;
	movzbl		(%edi),%ecx;
	cmpl		$127,%eax;
	jg		6f;
	sall		$8,%ecx;
	andl		$255,%ebp;			
	leal		16384(%ecx,%ebp),%ebp;		
	addb		$20,%bl;
	jmp		9b;

6:	cmpl		$255,%eax;
	sall		$16,%ecx;			
	andl		$33023,%ebp;			
	leal		4194304(%ecx,%ebp),%ebp;	
	addb		$28,%bl;
	jle		9b;

	movd		%mm6,%esp;
	addl		$4,%esp;
	popl		%ebx;
	popl		%edi;
	movl		$1,%eax;
	popl		%ebp;
	popl		%esi;
	ret;

	.align 16

8:	leal		mp1e_ac_vlc_zero,%edi;		
	movq		video_out+16,%mm3;		
	movq		%mm2,%mm5;
	pxor		%mm4,%mm4;
	psubd		%mm1,%mm4;
	movd		%mm4,%ebx;			
	psrld		%mm4,%mm5;
	movl		video_out+4,%ecx;
	por		%mm5,%mm7;			
	psubd		%mm4,%mm3;			
	movzbl		mp1e_iscan+63(%esp),%ebp;
	movd		%mm7,%eax;			
	psrlq		$32,%mm7;
	psllq		%mm3,%mm2;
	leal		8(%ecx),%edx;
	bswap		%eax;
	movl		%eax,4(%ecx);
	movd		%mm7,%eax;			
	incl		%esp;
	movl		%edx,video_out+4;		
	movq		%mm2,%mm7;
	bswap		%eax;
	movl		%eax,(%ecx);			
	jle		3b;
	jmp		0b;
