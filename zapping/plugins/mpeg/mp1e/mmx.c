/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: mmx.c,v 1.2 2000-07-05 18:09:34 mschimek Exp $ */

#include <stdlib.h>
#include "log.h"
#include "mmx.h"
#include "misc.h"

#if #cpu (i386)

#define le4cc(a,b,c,d) (((((unsigned long)(d))&0xFFUL)<<24)|((((unsigned long)(c))&0xFFUL)<<16)| \
			((((unsigned long)(b))&0xFFUL)<<8)|((((unsigned long)(a))&0xFFUL)))

#define INTEL_CMOV	(1 << 15)
#define INTEL_MMX	(1 << 23)
#define INTEL_XMM	(1 << 25)

#define AMD_MMX		(1 << 23)
#define AMD_MMX_EXT	(1 << 30)

#define FEATURE(bits)	((feature & (bits)) == (bits))

int
cpu_id(cpu_architecture arch)
{
	unsigned int vendor;
	unsigned int feature;

    	asm ("
		pushfl
		popl		%%ecx
		movl		%%ecx,%%edx
		xorl		$0x200000,%%edx
		pushl		%%edx
		popfl
		pushfl
		popl		%%edx
		pushl		%%ecx
		popfl
		xorl		%%ecx,%%edx
		andl		$0x200000,%%edx
		jne		1f
		movl		%%edx,%1	/* No CPUID, vendor 0 */
		jmp		2f
1:		movl		$0,%%eax
		cpuid
		movl		%%ebx,%1
		movl		$1,%%eax
		cpuid
2:
	"
	: "=d" (feature)
	: "m" (vendor)
	: "eax", "ebx", "ecx", "cc");

	/*
	 *  This is only a rough check for features
	 *  interesting for us.
	 */
	switch (vendor) {
	case 0:
		break;

	case le4cc('G', 'e', 'n', 'u'): /* "GenuineIntel" */
		switch (arch) {
		case ARCH_PENTIUM_MMX:
			return FEATURE(INTEL_MMX);
		
		case ARCH_KLAMATH:
			return FEATURE(INTEL_MMX | INTEL_CMOV);

		case ARCH_KATMAI:
			return FEATURE(INTEL_MMX | INTEL_CMOV | INTEL_XMM);
		
		default:
		}

		break;

	case le4cc('A', 'u', 't', 'h'): /* "AuthenticAMD" */
		switch (arch) {
		case ARCH_PENTIUM_MMX:
			return FEATURE(AMD_MMX);

		case ARCH_K6_2:
			return FEATURE(AMD_MMX_EXT);

		default:
		}

		break;

	default:
		ASSERT("identify CPU", 0);
	}

	return 0;
}

#else

int
cpu_id(cpu_architecture arch)
{
	return 0;
}

#endif // !cpu x86

#ifdef MMX_SOFTSIM

#define WRAP(n) (n)
#define SATB(n) saturate(n, -128, 127)
#define SATW(n) saturate(n, -32768, 32767)
#define SATUB(n) saturate(n, 0, 255)
#define SATUW(n) saturate(n, 0, 65535)

void
packsswb(mmx_t mm1, mmx_t *mm2)
{
	int i;
	mmx_t t;

	for (i = 7; i >= 0; i--)
		t.b[i] = SATB((i >= 4) ? mm1.w[i & 3] : mm2->w[i & 3]);
	*mm2 = t;
}

void
packssdw(mmx_t mm1, mmx_t *mm2)
{
	int i;
	mmx_t t;

	for (i = 3; i >= 0; i--)
		t.w[i] = SATW((i >= 2) ? mm1.d[i & 1] : mm2->d[i & 1]);
	*mm2 = t;
}

void
packuswb(mmx_t mm1, mmx_t *mm2)
{
	int i;
	mmx_t t;

	for (i = 7; i >= 0; i--)
		t.ub[i] = SATUB((i >= 4) ? mm1.uw[i & 3] : mm2->uw[i & 3]);
	*mm2 = t;
}

#define palu(name, n, fi, sat, op)				\
void name(mmx_t mm1, mmx_t *mm2)				\
{								\
	int i;							\
	for (i = 0; i < n; i++)					\
	        mm2->fi[i] = sat (mm2->fi[i] op mm1.fi[i]);	\
}

palu(paddb, 8, b, WRAP, +)
palu(paddw, 4, w, WRAP, +)
palu(paddd, 2, d, WRAP, +)
palu(paddsb, 8, b, SATB, +)
palu(paddsw, 4, w, SATW, +)
palu(paddusb, 8, ub, SATUB, +)
palu(paddusw, 4, uw, SATUW, +)

extern void pcmpeqb(mmx_t, mmx_t *);
extern void pcmpeqw(mmx_t, mmx_t *);
extern void pcmpeqd(mmx_t, mmx_t *);
extern void pcmpgtb(mmx_t, mmx_t *);
extern void pcmpgtw(mmx_t, mmx_t *);
extern void pcmpgtd(mmx_t, mmx_t *);

void
pmaddwd(mmx_t mm1, mmx_t *mm2)
{
	mm2->d[0] = mm2->w[0] * mm1.w[0] + mm2->w[1] * mm1.w[1];
	mm2->d[1] = mm2->w[2] * mm1.w[2] + mm2->w[3] * mm1.w[3];
}

void
pmulhw(mmx_t mm1, mmx_t *mm2)
{
	int i;
	
	for (i = 0; i < 4; i++)
		mm2->w[i] = (mm2->w[i] * mm1.w[i]) >> 16;
}

void
pmulhuw(mmx_t mm1, mmx_t *mm2)
{
	int i;
	
	for (i = 0; i < 4; i++)
		mm2->w[i] = (mm2->w[i] * mm1.uw[i]) >> 16;
}

void
pmullw(mmx_t mm1, mmx_t *mm2)
{
	int i;
	
	for (i = 0; i < 4; i++)
	        mm2->w[i] = (mm2->w[i] * mm1.w[i]) & 0x0FFFF;
}

#define pshift(name, m, fi, op)			\
void name(unsigned int n, mmx_t *mm2)		\
{						\
	int i;					\
						\
	if (n > ((64 / m) - 1))			\
		for (i = 0; i < m; i++)		\
			mm2->fi = 0;		\
	else					\
		for (i = 0; i < m; i++)		\
			mm2->fi op n;		\
}

pshift(psllb, 8, ub[i], <<=)
pshift(psllw, 4, uw[i], <<=)
pshift(pslld, 2, ud[i], <<=)
pshift(psllq, 1, uq,    <<=)
pshift(psrlb, 8, ub[i], >>=)
pshift(psrlw, 4, uw[i], >>=)
pshift(psrld, 2, ud[i], >>=)
pshift(psrlq, 1, uq,    >>=)

void
psraw(unsigned int n, mmx_t *mm2)
{
	int i;

	for (i = 0; i < 4; i++)
		mm2->w[i] >>= MIN(n, 15);
}

void
psrad(unsigned int n, mmx_t *mm2)
{
	int i;

	for (i = 0; i < 2; i++)
		mm2->d[i] >>= MIN(n, 31);
}

palu(psubb, 8, b, WRAP, -)
palu(psubw, 4, w, WRAP, -)
palu(psubd, 2, d, WRAP, -)
palu(psubsb, 8, b, SATB, -)
palu(psubsw, 4, w, SATW, -)
palu(psubusb, 8, ub, SATUB, -)
palu(psubusw, 4, uw, SATUW, -)

void
punpckhbw(mmx_t mm1, mmx_t *mm2)
{
	int i;
	mmx_t t;

	for (i = 0; i < 8; i++)
		t.ub[i] = (i & 1) ? mm1.ub[i / 2 + 4] : mm2->ub[i / 2 + 4];
	*mm2 = t;
}

void
punpckhwd(mmx_t mm1, mmx_t *mm2)
{
	int i;
	mmx_t t;

	for (i = 0; i < 4; i++)
		t.uw[i] = (i & 1) ? mm1.uw[i / 2 + 2] : mm2->uw[i / 2 + 2];
	*mm2 = t;
}

void
punpckhdq(mmx_t mm1, mmx_t *mm2)
{
	int i;
	mmx_t t;

	for (i = 0; i < 2; i++)
		t.ud[i] = (i & 1) ? mm1.ud[1] : mm2->ud[1];
	*mm2 = t;
}

void
punpcklbw(mmx_t mm1, mmx_t *mm2)
{
	int i;
	mmx_t t;

	for (i = 0; i < 8; i++)
		t.ub[i] = (i & 1) ? mm1.ub[i / 2] : mm2->ub[i / 2];
	*mm2 = t;
}

void
punpcklwd(mmx_t mm1, mmx_t *mm2)
{
	int i;
	mmx_t t;

	for (i = 0; i < 4; i++)
		t.uw[i] = (i & 1) ? mm1.uw[i / 2] : mm2->uw[i / 2];
	*mm2 = t;
}

void
punpckldq(mmx_t mm1, mmx_t *mm2)
{
	int i;
	mmx_t t;

	for (i = 0; i < 2; i++)
		t.ud[i] = (i & 1) ? mm1.ud[0] : mm2->ud[0];
	*mm2 = t;
}

#endif // MMX_SOFTSIM
