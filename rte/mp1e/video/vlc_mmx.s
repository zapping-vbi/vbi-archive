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

# $Id: vlc_mmx.s,v 1.7 2002-09-14 04:18:29 mschimek Exp $

# vlc_rec
bs_rec_n	= 0
bs_rec_p	= 4
bs_rec_uq64	= 16
dc_dct_pred_y	= 32
dc_dct_pred_u	= 36
dc_dct_pred_v	= 40

vlc_recp	= 20
mblockp		= 24
iscanp		= 28
count		= 32

# int (vlc_rec *, mblock *, iscan *, int count);

	.macro intra mpeg=1 vlc=mp1e_ac_vlc_zero

	subl		$16,%esp;
	movl		%ebp,(%esp);
	movl		vlc_recp(%esp),%ebp;
	movl		%esi,4(%esp);
	movl		mblockp(%esp),%esi;
	movl		%ebx,8(%esp);
	movl		dc_dct_pred_y(%ebp),%ebx;
	movl		%edi,12(%esp);
	movd		%esp,%mm0;
	call		4f;
	movl		mblockp(%esp),%esi;
	leal		mp1e_dc_vlc_intra+24*8,%edi;
	movl		dc_dct_pred_u(%ebp),%ebx;
	leal		4*128(%esi),%esi;
	call		5f;
	movl		mblockp(%esp),%esi;
	leal		mp1e_dc_vlc_intra+24*8,%edi;
	movl		dc_dct_pred_v(%ebp),%ebx;
	leal		5*128(%esi),%esi;
	call		5f;
#	decl		count(%esp);
#	je		3f;
#
#2:	movl		mblockp(%esp),%esi;
#	leal		mp1e_dc_vlc_intra,%edi;
#	movswl		3*128(%esi),%ebx;
#	leal		0*128+768(%esi),%esi;
#	movl		%esi,mblockp(%esp);
#	call		4f;
#	movl		mblockp(%esp),%esi;
#	leal		mp1e_dc_vlc_intra+24*8,%edi;
#	movl		4*128-768(%esi),%ebx;
#	leal		4*128(%esi),%esi;
#	call		5f;
#	movl		mblockp(%esp),%esi;
#	leal		mp1e_dc_vlc_intra+24*8,%edi;
#	movl		5*128-768(%esi),%ebx;
#	leal		5*128(%esi),%esi;
#	call		5f;
#	decl		count(%esp);
#	jne		2b;

3:	movl		mblockp(%esp),%esi;
	movl		%ebp,%eax;
	movswl		3*128(%esi),%edx;
	movl		$2,%ecx;
	movswl		4*128(%esi),%ebx;
	movl		%edx,dc_dct_pred_y(%ebp);
	movswl		5*128(%esi),%esi;
	movl		%ebx,dc_dct_pred_u(%ebp);
	movl		$2,%edx;
	movl		%esi,dc_dct_pred_v(%ebp);
	call		mmx_bputl;
	movl		(%esp),%ebp;
	movl		4(%esp),%esi;
	xorl		%eax,%eax;
	movl		8(%esp),%ebx;
	movl		12(%esp),%edi;
	addl		$16,%esp;
	ret

	.align 16

4:	leal		mp1e_dc_vlc_intra,%edi;
	leal		0*128(%esi),%esi;
	movl		iscanp+4(%esp),%edx;
	call		6f;
	movl		mblockp+4(%esp),%esi;
	leal		mp1e_dc_vlc_intra+12*8,%edi;
	movswl		0*128(%esi),%ebx;
	leal		2*128(%esi),%esi;
	movl		iscanp+4(%esp),%edx;
	call		6f;
	movl		mblockp+4(%esp),%esi;
	leal		mp1e_dc_vlc_intra+12*8,%edi;
	movswl		2*128(%esi),%ebx;
	leal		1*128(%esi),%esi;
	movl		iscanp+4(%esp),%edx;
	call		6f;
	movl		mblockp+4(%esp),%esi;
	leal		mp1e_dc_vlc_intra+12*8,%edi;
	movswl		1*128(%esi),%ebx;
	leal		3*128(%esi),%esi;
5:	movl		iscanp+4(%esp),%edx;
6:	movd		%esp,%mm6;
	movl		$0,%ecx;
	movswl		(%esi),%eax;			
	subl		%ebx,%eax;
	movl		%eax,%ebx;
	movl		%edx,%esp
	cdq;
	xorl		%edx,%eax;
	subl		%edx,%eax;			
	bsrl		%eax,%ecx;
	setnz		%al;
	addl		%edx,%ebx;
	addb		%al,%cl;
	sall		%cl,%edx;
	xorl		%edx,%ebx;			
	orl		(%edi,%ecx,8),%ebx;
	movl		4(%edi,%ecx,8),%ecx;
	jmp		8f;

	.align 16

2:	movswl		(%esi,%ebx,2),%eax;		# slevel
	testl		%eax,%eax;
	leal		1(%esp),%esp;
	jne		3f;
	movzbl		(%esp),%ebx;			# scan
	testl		%ebx,%ebx;
	leal		2(%edi),%edi;			# run++
	jne		2b;
	movd		%mm6,%esp;
	ret;

3:	movzbl		1(%edi),%ecx;			# level limit
	cdq;
	xorl		%edx,%eax;
	subl		%edx,%eax;			# ulevel
	cmpl		%ecx,%eax;
	movzbl		(%edi),%ecx;
	leal		(%eax,%ecx),%ecx;		# vlc_table[run][level] offset
	jge		5f;
	movzbl		(%edi,%ecx,2),%ebx;		# code
	subl		%edx,%ebx;			# sign
	movzbl		1(%edi,%ecx,2),%ecx;		# length
8:	addl		bs_rec_n(%ebp),%ecx;
4:	movl		$64,%edi;
	movl		%ecx,bs_rec_n(%ebp);
	movd		%ebx,%mm2;
	subl		%ecx,%edi;
	movd		%edi,%mm1;
	leal		\vlc,%edi;			# run = 0
	jle		7f;
	movzbl		(%esp),%ebx;
	psllq		%mm1,%mm2;
	testl		%ebx,%ebx;
	por		%mm2,%mm7;
	jne		2b;
	movd		%mm6,%esp;
	ret;

	.ifeq \mpeg-1

5:	subl		$\vlc,%edi;			# run
	movswl		(%esi,%ebx,2),%edx;		# slevel		
	cmpl		$127,%eax;
	jg		6f;
	andl		$255,%edx;			
	sall		$8-1,%edi;
	movl		bs_rec_n(%ebp),%ecx;
	leal		16384(%edi,%edx),%ebx;		# escape
	leal		20(%ecx),%ecx;			# length
	jmp		4b;

6:	sall		$16-1,%edi;			
	andl		$33023,%edx;			
	cmpl		$255,%eax;			
	movl		bs_rec_n(%ebp),%ecx;
	leal		4194304(%edi,%edx),%ebx;	# escape
	leal		28(%ecx),%ecx;			# length
	jle		4b;

	.else

5:	subl		$\vlc,%edi;			# run
	movswl		(%esi,%ebx,2),%edx;		# slevel		
	sall		$12-1,%edi;
	andl		$4095,%edx;
	cmpl		$255,%eax;
	movl		bs_rec_n(%ebp),%ecx;
	leal		262144(%edi,%edx),%ebx;		# escape
	leal		24(%ecx),%ecx;			# length
	jle		4b;

	.endif

	movd		%mm0,%esp;
	movl		(%esp),%ebp;
	movl		4(%esp),%esi;
	movl		$1,%eax;
	movl		8(%esp),%ebx;
	movl		12(%esp),%edi;
	addl		$16,%esp;
	ret

	.align 16

7:	movq		bs_rec_uq64(%ebp),%mm3;
	pxor		%mm4,%mm4;
	movq		%mm2,%mm5;
	psubd		%mm1,%mm4;
	movd		%mm4,bs_rec_n(%ebp);
	psubd		%mm4,%mm3;
	psrld		%mm4,%mm5;
	movl		bs_rec_p(%ebp),%ecx;
	por		%mm5,%mm7;
	movd		%mm7,%eax;
	movzbl		(%esp),%ebx;
	psrlq		$32,%mm7;
	bswap		%eax;				# sse? gain is small
	leal		8(%ecx),%edx;
	movl		%eax,4(%ecx);
	movd		%mm7,%eax;
	bswap		%eax;
	psllq		%mm3,%mm2;
	testl		%ebx,%ebx;
	movq		%mm2,%mm7;
	movl		%eax,(%ecx);
	movl		%edx,bs_rec_p(%ebp);
	jne		2b;
	movd		%mm6,%esp;
	ret;

	.endm

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
	jne		4f;
	movzbl		mp1e_iscan0+63(%esp),%ebp;		
	addl		$2,%edi;
	incl		%esp;
	jle		3b;

0:	movl		%ebx,video_out;
	movd		%mm6,%esp;
	movl		$video_out,%eax;
	movl		$2,%ecx;
	movl		$2,%edx;		
	jmp		mmx_bputl;

4:	movzbl		1(%edi),%ecx;
	cdq;
	xorl		%edx,%eax;
	subl		%edx,%eax;
	cmpl		%ecx,%eax;			
	movzbl		(%edi),%ecx;
	leal		(%eax,%ecx),%ecx;
	jge		5f;
	movzbl		(%edi,%ecx,2),%ebp;
	subl		%edx,%ebp;
	addb		1(%edi,%ecx,2),%bl;
9:	movl		$64,%edi;
	movd		%ebp,%mm2;
	subl		%ebx,%edi;
	movd		%edi,%mm1;		
	jle		8f;
	leal		mp1e_ac_vlc_zero,%edi;
	movzbl		mp1e_iscan0+63(%esp),%ebp;		
	psllq		%mm1,%mm2;
	incl		%esp;
	por		%mm2,%mm7;
	jle 		3b;
	jmp		0b;

	.align 16

5:	subl		$mp1e_ac_vlc_zero,%edi;
	cmpl		$127,%eax;
	movswl		(%esi,%ebp,2),%ebp;
	jg		6f;
	sall		$8-1,%edi;
	andl		$255,%ebp;			
	leal		16384(%edi,%ebp),%ebp;		
	addb		$20,%bl;
	jmp		9b;

6:	sall		$16-1,%edi;
	addb		$28,%bl;
	cmpl		$255,%eax;			
	andl		$33023,%ebp;			
	leal		4194304(%edi,%ebp),%ebp;	
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
	movzbl		mp1e_iscan0+63(%esp),%ebp;
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

	.text
	.align		16
	.globl		mp1e_p6_mpeg1_encode_intra

mp1e_p6_mpeg1_encode_intra:
	intra		mpeg=1 vlc=mp1e_ac_vlc_zero

	.text
	.align		16
	.globl		mp1e_p6_mpeg2_encode_intra_14

mp1e_p6_mpeg2_encode_intra_14:
	intra		mpeg=2 vlc=mp1e_ac_vlc_zero

#	.text
#	.align		16
#	.globl		mp1e_p6_mpeg2_encode_intra_15
#
#mp1e_p6_mpeg2_encode_intra_15:
#	intra		mpeg=2 vlc=mp1e_ac_vlc_one
