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

/* $Id: mmx.c,v 1.8 2001-06-23 02:50:44 mschimek Exp $ */

#include <stdlib.h>
#include "log.h"
#include "mmx.h"
#include "math.h"

#if #cpu (i386)

/*
 *  References
 *
 *  "Intel Processor Identification and the CPUID Instruction",
 *  Application Note AP-485, May 2000, order no. 241618-015,
 *  http://developer.intel.com/design/pentiumII/applnots/241618.htm
 *
 *  "AMD Processor Recognition Application Note",
 *  publication # 20734, Rev. R, June 2000.
 *  http://www.amd.com/products/cpg/athlon/techdocs/index.html
 *
 *  "Cyrix CPU Detection Guide",
 *  Application Note 112, Rev. 1.9, July 21, 1998
 *  formerly available from http://www.cyrix.com/html/developers/index.htm
 *  when Cyrix was part of National Semiconductor.
 *  VIA has no similar document available as of Jan 2001.
 */

typedef union {
	unsigned char		s[16];
	struct {
		unsigned int		eax;
		unsigned int		ebx;
		unsigned int		edx;
		unsigned int		ecx;
	}			r;
} cpuid_t;

static inline int
toggle_eflags_id(void)
{
	int success;

	__asm__ __volatile__ (
		" pushfl	\n"
		" popl		%%ecx\n"
		" movl		%%ecx,%%eax\n"
		" xorl		$0x200000,%%eax\n"
		" pushl		%%eax\n"
		" popfl		\n"
		" pushfl	\n"
		" popl		%%eax\n"
		" pushl		%%ecx\n"
		" popfl		\n"
		" xorl		%%ecx,%%eax\n"
		" andl		$0x200000,%%eax\n"
		" jz		1f\n"
		" movl		$1,%%eax\n"
		"1:\n"
	: "=a" (success) :: "ecx", "cc");

	return success;
}

static /*inline*/ unsigned int
cpuid(cpuid_t *buf, unsigned int level)
{
	unsigned int eax;

	/* ARRRR */
	__asm__ __volatile__ (
		" pushl		%%ebx\n"
		" pushl		%%ecx\n"
		" pushl		%%edx\n"
		" cpuid		\n"
		" movl		%%eax,(%%edi)\n"
		" movl		%%ebx,4(%%edi)\n"
		" movl		%%edx,8(%%edi)\n"
		" movl		%%ecx,12(%%edi)\n"
		" popl		%%edx\n"
		" popl		%%ecx\n"
		" popl		%%ebx\n"
	: "=a" (eax) : "D" (buf), "a" (level) /*: "ebx", "ecx", "edx", "cc", "memory"*/);

	return eax;
}

/* XXX check kernel version before advertising SSE */
#define INTEL_CMOV	(1 << 15)
#define INTEL_MMX	(1 << 23)
#define INTEL_SSE	(1 << 25)
#define INTEL_SSE2	(1 << 26)

#define AMD_MMXEXT	(1 << 22)
#define AMD_MMX		(1 << 23)
#define AMD_SSE		(1 << 25)
#define AMD_3DNOWEXT	(1 << 30)
#define AMD_3DNOW	(1 << 31)

#define CYRIX_MMX	(1 << 23)
#define CYRIX_MMXEXT	(1 << 24)
#define CYRIX_3DNOW	(1 << 31)

#define FEATURE(bits)	((c.r.edx & (bits)) == (bits))

int
cpu_detection(void)
{
	cpuid_t c;

	if (!toggle_eflags_id()) {
		ASSERT("identify CPU", 0);
		return CPU_UNKNOWN;
	}

	cpuid(&c, 0);

	if (!strncmp(c.s + 4, "GenuineIntel", 12)) {
		cpuid(&c, 1);

		if (FEATURE(INTEL_MMX | INTEL_CMOV | INTEL_SSE | INTEL_SSE2))
			return CPU_PENTIUM_4;
		if (FEATURE(INTEL_MMX | INTEL_CMOV | INTEL_SSE))
			return CPU_PENTIUM_III;
		if (FEATURE(INTEL_MMX | INTEL_CMOV))
			return CPU_PENTIUM_II;
		if (FEATURE(INTEL_MMX))
			return CPU_PENTIUM_MMX;
	} else if (!strncmp(c.s + 4, "AuthenticAMD", 12)) {
		if (cpuid(&c, 0x80000000) > 0x80000000) {
			cpuid(&c, 0x80000001);

			if (FEATURE(AMD_MMX | AMD_MMXEXT | AMD_3DNOW | AMD_3DNOWEXT))
				return CPU_ATHLON;
			if (FEATURE(AMD_MMX | AMD_3DNOW))
				return CPU_K6_2;
		}
	} else if (!strncmp(c.s + 4, "CyrixInstead", 12)) {
		if (cpuid(&c, 0x80000000) > 0x80000000) {
			cpuid(&c, 0x80000001);

			if (FEATURE(CYRIX_MMX | CYRIX_MMXEXT | CYRIX_3DNOW))
				return CPU_CYRIX_III;
		} else {
			cpuid(&c, 1);

			if (FEATURE(CYRIX_MMX))
				return CPU_CYRIX_MII;
		}
	}

	ASSERT("identify CPU", 0);

	return CPU_UNKNOWN;
}

#else

int
cpu_detection(cpu_architecture arch)
{
	return 0;
}

#endif /* !cpu x86 */
