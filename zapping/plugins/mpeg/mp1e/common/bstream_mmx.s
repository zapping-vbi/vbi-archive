#
#  MPEG-1 Real Time Encoder
# 
#  Copyright (C) 1999-2001 Michael H. Schimek
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

# $Id: bstream_mmx.s,v 1.1 2001-07-18 06:32:37 mschimek Exp $
	.text
	.align		16
	.globl		mmx_bputl
	.globl		mmx_bputq

# void
# mmx_bputl(struct bs_rec *b [eax], int n [edx], unsigned int v [ecx])

mmx_bputl:
	movd		%ecx,%mm0;	// value
mmx_bputq:
	addl		(%eax),%edx;	// bs_rec->n + nbits
	movl		$64,%ecx;
	subl		%edx,%ecx;
	movd		%ecx,%mm1;
	jle		1f;
	psllq		%mm1,%mm0;
	movl		%edx,(%eax);	// bs_rec->n
	por		%mm0,%mm7;
	ret
1:
	movd		16(%eax),%mm3;	// bs_rec->uq64 (64ULL)
	pxor		%mm2,%mm2;
	movq		%mm0,%mm4;
	movl		4(%eax),%ecx;	// bs_rec->p
	paddd		%mm1,%mm3;
	psubd		%mm1,%mm2;
	psllq		%mm3,%mm0;
	leal		8(%ecx),%edx;
	psrlq		%mm2,%mm4;
	movd		%mm2,(%eax);	// bs_rec->n
	por		%mm7,%mm4;
	movl		%edx,4(%eax);	// bs_rec->p
	movd		%mm4,%edx;
	psrlq		$32,%mm4;
	bswap		%edx;
	movd		%mm4,%eax;
	movl		%edx,4(%ecx)
	movq		%mm0,%mm7;
	bswap		%eax;
	movl		%eax,(%ecx);
	ret
