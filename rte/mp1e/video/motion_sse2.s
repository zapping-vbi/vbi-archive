#
#  MPEG-1 Real Time Encoder
# 
#  Copyright (C) 2001 Michael H. Schimek
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

# $Id: motion_sse2.s,v 1.2 2001-08-22 01:28:10 mschimek Exp $

		.text
		.align		16
		.globl		sse2_sad

# (%eax) assumed 16 byte aligned 

sse2_sad:
		movdqu		(%edx),%xmm0;
		pxor		%xmm7,%xmm7;
		movdqu		(%edx,%ecx),%xmm1;
		leal		(%edx,%ecx,2),%edx;
		movdqu		(%edx),%xmm2;
		movdqu		(%edx,%ecx),%xmm3;
		leal		(%edx,%ecx,2),%edx;
		psadbw		(%eax),%xmm0;
		paddw		%xmm0,%xmm7;
		movdqu		(%edx),%xmm0;
		psadbw		1*16(%eax),%xmm1;
		paddw		%xmm1,%xmm7;
		movdqu		(%edx,%ecx),%xmm1;
		leal		(%edx,%ecx,2),%edx;
		psadbw		2*16(%eax),%xmm2;
		paddw		%xmm2,%xmm7;
		movdqu		(%edx),%xmm2;
		psadbw		3*16(%eax),%xmm3;
		paddw		%xmm3,%xmm7;
		movdqu		(%edx,%ecx),%xmm3;
		leal		(%edx,%ecx,2),%edx;
		psadbw		4*16(%eax),%xmm0;
		paddw		%xmm0,%xmm7;
		movdqu		(%edx),%xmm0;
		psadbw		5*16(%eax),%xmm1;
		paddw		%xmm1,%xmm7;
		movdqu		(%edx,%ecx),%xmm1;
		leal		(%edx,%ecx,2),%edx;
		psadbw		6*16(%eax),%xmm2;
		paddw		%xmm2,%xmm7;
		movdqu		(%edx),%xmm2;
		psadbw		7*16(%eax),%xmm3;
		paddw		%xmm3,%xmm7;
		movdqu		(%edx,%ecx),%xmm3;
		leal		(%edx,%ecx,2),%edx;
		psadbw		8*16(%eax),%xmm0;
		paddw		%xmm0,%xmm7;
		movdqu		(%edx),%xmm0;
		psadbw		9*16(%eax),%xmm1;
		paddw		%xmm1,%xmm7;
		movdqu		(%edx,%ecx),%xmm1;
		leal		(%edx,%ecx,2),%edx;
		psadbw		10*16(%eax),%xmm2;
		paddw		%xmm2,%xmm7;
		movdqu		(%edx),%xmm2;
		psadbw		11*16(%eax),%xmm3;
		paddw		%xmm3,%xmm7;
		movdqu		(%edx,%ecx),%xmm3;
		psadbw		12*16(%eax),%xmm0;
		paddw		%xmm0,%xmm7;
		psadbw		13*16(%eax),%xmm1;
		paddw		%xmm1,%xmm7;
		psadbw		14*16(%eax),%xmm2;
		paddw		%xmm2,%xmm7;
		psadbw		15*16(%eax),%xmm3;
		paddw		%xmm3,%xmm7;
		pshufd		$1*64+0*16+3*4+2,%xmm7,%xmm6;
		paddw		%xmm6,%xmm7;
		movd		%xmm7,%eax;
		ret;
