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

# $Id: vlc_mmx.s,v 1.1 2000-07-05 18:09:34 mschimek Exp $

# int
# mmx_mpeg1_encode_intra(void)

	.text
	.align		16
	.globl		mmx_mpeg1_encode_intra

mmx_mpeg1_encode_intra:

	pushl		%edi;
	pushl		%edx;
	leal		dc_vlc_intra,%edi;
	pushl		%esi;
	leal		mblock+0*128+768,%esi;
	pushl		%ebx;
	pushl		%ecx;
	movl		dc_dct_pred,%ebx;
	call		1f;
	movswl		mblock+0*128+768,%ebx;
	leal		mblock+2*128+768,%esi;
	leal		dc_vlc_intra+12*8,%edi;
	call		1f;
	movswl		mblock+2*128+768,%ebx;
	leal		mblock+1*128+768,%esi;
	leal		dc_vlc_intra+12*8,%edi;
	call		1f;
	movswl		mblock+1*128+768,%ebx;
	leal		mblock+3*128+768,%esi;
	leal		dc_vlc_intra+12*8,%edi;
	call		1f;
	movl		dc_dct_pred+4,%ebx;
	leal		mblock+4*128+768,%esi;
	leal		dc_vlc_intra+24*8,%edi;
	call		1f;
	movl		dc_dct_pred+8,%ebx;
	leal		mblock+5*128+768,%esi;
	leal		dc_vlc_intra+24*8,%edi;
	call		1f;
	movswl		mblock+3*128+768,%eax;
	movswl		mblock+4*128+768,%ebx;
	movl		%eax,dc_dct_pred;
	movswl		mblock+5*128+768,%ecx;
	movl		%ebx,dc_dct_pred+4;
	movl		%ecx,dc_dct_pred+8;
	movl		$video_out,%eax;
	movl		$2,%ecx;
	movl		$2,%edx;
	call		bputl;
	popl		%ecx;
	popl		%ebx;
	xorl		%eax,%eax
	popl		%esi;
	popl		%edx;
	popl		%edi;
	ret;

	.align	16

1:	movswl		(%esi),%edx;			subl		%ebx,%edx;
	movl		%edx,%ebx;			movl		%edx,%eax;
	sarl		$31,%ebx;			xorl		%ebx,%edx;
	subl		%ebx,%edx;			movl		$0,%ecx;
	bsrl		%edx,%ecx;
	setnz		%dl;
	addl		%ebx,%eax;			addb		%dl,%cl;
	sall		%cl,%ebx;
	xorl		%ebx,%eax;			movl		$63,%edx;
	orl		(%edi,%ecx,8),%eax;		movl		4(%edi,%ecx,8),%ebx;
	jmp		7f;

	.align 16

3:	movswl		(%esi,%ebx,2),%eax;		movzbl		1(%edi),%ecx;
	testl		%eax,%eax;			jne		4f;
	movzbl		iscan-1(%edx),%ebx;		decl		%edx;
	leal		(%edi,%ecx,2),%edi;		jns		3b;
0:	ret;

4:	movl		%eax,%ebx;			sarl		$31,%eax;
	movd		%ebx,%mm5;			xorl		%eax,%ebx;
	subl		%eax,%ebx;			shrl		$31,%eax;
	cmpl		%ecx,%ebx;			leal		(%edi,%ebx,2),%ecx;
	jge		5f;				
	movzbl		(%ecx),%edi;
	movzbl		1(%ecx),%ebx;			movl		$64,%ecx;
	addl		video_out,%ebx;			orl		%edi,%eax;
	movd		%eax,%mm2;			subl		%ebx,%ecx;
	movd		%ecx,%mm1;			jle		8f;
	movl		%ebx,video_out;			leal		ac_vlc_zero,%edi;
	movzbl		iscan-1(%edx),%ebx;		psllq		%mm1,%mm2;
	decl		%edx;				por		%mm2,%mm7;
	jns		3b;
	jmp		0b;

5:	movzbl		(%edi),%ecx;			cmpl		$127,%ebx;
	movd		%mm5,%eax;			jg		6f;
	andl		$255,%eax;			sall		$8,%ecx;
	movl		$20,%ebx;			leal		16384(%ecx,%eax),%eax;
7:	addl		video_out,%ebx;			movl		$64,%ecx;
	movd		%eax,%mm2;			subl		%ebx,%ecx;
	movd		%ecx,%mm1;			jle		8f;
	movl		%ebx,video_out;			leal		ac_vlc_zero,%edi;
	movzbl		iscan-1(%edx),%ebx;		psllq		%mm1,%mm2;
	decl		%edx;				por		%mm2,%mm7;
	jns		3b;
	jmp		0b;

6:	sall		$16,%ecx;			andl		$33023,%eax;			
	cmpl		$255,%ebx;			leal		4194304(%ecx,%eax),%eax;
	movl		$28,%ebx;			jle		7b;
	leal		4(%esp),%esp;			movl		$1,%eax;
	popl		%ecx;				popl		%ebx;
	popl		%esi;				popl		%edx;
	popl		%edi;
	ret;

	.align 16

8:	movq		video_out+16,%mm3;		negl		%ecx;
	movd		%ecx,%mm4;			movq		%mm2,%mm5;
	psubd		%mm4,%mm3;			movl		video_out+4,%ecx;
	movzbl		iscan-1(%edx),%ebx;		decl		%edx;
	movd		%mm4,video_out;			psrld		%mm4,%mm5;
	por		%mm5,%mm7;			psllq		%mm3,%mm2;
	movd		%mm7,%eax;			psrlq		$32,%mm7;
	bswap		%eax;
	movd		%mm7,%edi;			movl		%eax,4(%ecx);
	bswap		%edi;
	movl		%edi,(%ecx);			movq		%mm2,%mm7;
	leal		ac_vlc_zero,%edi;		leal		8(,%ecx,1),%ecx;
	movl		%ecx,video_out+4;		jns		3b;
	jmp		0b;

# int
# mmx_mpeg1_encode_inter(short mblock[6][8][8], unsigned int cbp)

	.text
	.align		16
	.globl		mmx_mpeg1_encode_inter

mmx_mpeg1_encode_inter:

	pushl		%edi
	pushl		%ebx
	testl		$32,3*4+4(%esp);
	pushl		%esi
	movl		4*4+0(%esp),%esi;
	je		2f;
	leal		0*128(%esi),%esi;
	call		1f;
	movl		4*4+0(%esp),%esi;
2:	testl		$8,4*4+4(%esp);
	je		2f;
	leal		2*128(%esi),%esi;
	call		1f;
	movl		4*4+0(%esp),%esi;
2:	testl		$16,4*4+4(%esp);
	je		2f;
	leal		1*128(%esi),%esi;
	call		1f;
	movl		4*4+0(%esp),%esi;
2:	testl		$4,4*4+4(%esp);
	je		2f;
	leal		3*128(%esi),%esi;
	call		1f;
	movl		4*4+0(%esp),%esi;
2:	testl		$2,4*4+4(%esp);
	je		2f;
	leal		4*128(%esi),%esi;
	call		1f;
	movl		4*4+0(%esp),%esi;
2:	testl		$1,4*4+4(%esp);
	je		2f;
	leal		5*128(%esi),%esi;
	call		1f;
2:	popl		%esi
	xorl		%eax,%eax
	popl		%ebx
	popl		%edi
	ret

	.align	16

1:	movswl		(%esi),%eax;			movl		$63,%edx;
	movl		%eax,%ebx;			sarl		$31,%eax;
	xorl		%eax,%ebx;			subl		%eax,%ebx;
	decl		%ebx;				jne		2f
	movl		$2,%ebx;			shrl		$31,%eax;
	orl		%ebx,%eax;			jmp		7f;
2:	leal		ac_vlc_zero,%edi;		movl		$0,%ebx;

	.align 16

3:	movswl		(%esi,%ebx,2),%eax;		movzbl		1(%edi),%ecx;
	testl		%eax,%eax;			jne		4f;
	movzbl		iscan-1(%edx),%ebx;		decl		%edx;
	leal		(%edi,%ecx,2),%edi;		jns		3b;
0:	movl		$video_out,%eax;		movl		$2,%ecx;
	movl		$2,%edx;			jmp		bputl;

4:	movl		%eax,%ebx;			sarl		$31,%eax;
	movd		%ebx,%mm5;			xorl		%eax,%ebx;
	subl		%eax,%ebx;			shrl		$31,%eax;
	cmpl		%ecx,%ebx;			leal		(%edi,%ebx,2),%ecx;
	jge		5f;				movzbl		(%ecx),%edi;
	movzbl		1(%ecx),%ebx;			movl		$64,%ecx;
	addl		video_out,%ebx;			orl		%edi,%eax;
	movd		%eax,%mm2;			subl		%ebx,%ecx;
	movd		%ecx,%mm1;			jle		8f;
	movl		%ebx,video_out;			leal		ac_vlc_zero,%edi;
	movzbl		iscan-1(%edx),%ebx;		psllq		%mm1,%mm2;
	decl		%edx;				por		%mm2,%mm7;
	jns 		3b;
	jmp		0b;

	.align 16

5:	movzbl		(%edi),%ecx;			cmpl		$127,%ebx;
	movd		%mm5,%eax;			jg		6f;
	andl		$255,%eax;			sall		$8,%ecx;
	leal		16384(%ecx,%eax),%eax;		movl		$20,%ebx;
7:	addl		video_out,%ebx;			movl		$64,%ecx;
	movd		%eax,%mm2;			subl		%ebx,%ecx;
	movd		%ecx,%mm1;			jle		8f;
	movl		%ebx,video_out;			leal		ac_vlc_zero,%edi;
	movzbl		iscan-1(%edx),%ebx;		psllq		%mm1,%mm2;
	decl		%edx;				por		%mm2,%mm7;
	jns		3b;
	jmp		0b;

	.align 16

6:	sall		$16,%ecx;			cmpl		$255,%ebx;
	andl		$33023,%eax;			movl		$28,%ebx;
	leal		4194304(%ecx,%eax),%eax;	jle		7b;
	leal		4(%esp),%esp;			movl		$1,%eax;
	popl		%ebx;
	popl		%esi;
	popl		%edi;
	ret;

	.align 16

8:	movq		video_out+16,%mm3;		negl		%ecx;
	movd		%ecx,%mm4;			movq		%mm2,%mm5;
	psubd		%mm4,%mm3;			movl		video_out+4,%ecx;
	movzbl		iscan-1(%edx),%ebx;		decl		%edx;
	movd		%mm4,video_out;			psrld		%mm4,%mm5;
	por		%mm5,%mm7;			psllq		%mm3,%mm2;
	movd		%mm7,%eax;			psrlq		$32,%mm7;
	bswap		%eax;
	movd		%mm7,%edi;			movl		%eax,4(%ecx);
	bswap		%edi;
	movl		%edi,(%ecx);			movq		%mm2,%mm7;
	leal		ac_vlc_zero,%edi;		leal		8(,%ecx,1),%ecx;
	movl		%ecx,video_out+4;		jns		3b;
	jmp		0b;
