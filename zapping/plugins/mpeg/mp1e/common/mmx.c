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

/* $Id: mmx.c,v 1.2 2000-10-15 21:24:48 mschimek Exp $ */

#include <stdlib.h>
#include "log.h"
#include "mmx.h"
#include "math.h"

#if #cpu (i386)

#define le4cc(a,b,c,d) (((((unsigned long)(d))&0xFFUL)<<24)|((((unsigned long)(c))&0xFFUL)<<16)| \
			((((unsigned long)(b))&0xFFUL)<<8)|((((unsigned long)(a))&0xFFUL)))

#define INTEL_CMOV	(1 << 15)
#define INTEL_MMX	(1 << 23)
#define INTEL_XMM	(1 << 25)

#define AMD_MMX		(1 << 23)
#define AMD_MMX_EXT	(1 << 30)

#define FEATURE(bits)	((feature & (bits)) == (bits))

// XXX rethink

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
