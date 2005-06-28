/*
 *  Copyright (C) 2004-2005 Michael H. Schimek
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

/* $Id: cpu.c,v 1.2 2005-06-28 00:59:02 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <string.h>
#include "mmx.h"

/*
 *  References
 *
 *  "Intel Processor Identification and the CPUID Instruction",
 *  Application Note AP-485, November 2000, order no. 241618-016,
 *  http://developer.intel.com
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
	char			s[16];
	struct {
		unsigned int		eax;
		unsigned int		ebx;
		unsigned int		edx;
		unsigned int		ecx;
	}			r;
} cpuid_t;

#ifdef HAVE_X86

static __inline__ int
toggle_eflags_id		(void)
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

static unsigned int
cpuid				(cpuid_t *		buf,
				 unsigned int		level)
{
	unsigned int eax;

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
	: "=a" (eax)
	: "D" (buf), "a" (level)
	: "cc", "memory");

	return eax;
}

#else /* x86-64 */

static __inline__ int
toggle_eflags_id		(void)
{
	return 1; /* they all have cpuid */
}

static unsigned long
cpuid				(cpuid_t *		buf,
				 unsigned long		level)
{
	unsigned long eax;

	__asm__ __volatile__ (
		" pushq		%%rbx\n"
		" pushq		%%rcx\n"
		" pushq		%%rdx\n"

		" cpuid		\n"

		" movl		%%eax,(%%rdi)\n"
		" movl		%%ebx,4(%%rdi)\n"
		" movl		%%edx,8(%%rdi)\n"
		" movl		%%ecx,12(%%rdi)\n"

		" popq		%%rdx\n"
		" popq		%%rcx\n"
		" popq		%%rbx\n"
	: "=a" (eax)
	: "D" (buf), "a" (level)
	: "cc", "memory");

	return eax;
}

#endif

/* Function 0x1 */
#define INTEL_TSC	(1 << 4)
#define INTEL_CMOV	(1 << 15)
#define INTEL_MMX	(1 << 23)
#define INTEL_SSE	(1 << 25)
#define INTEL_SSE2	(1 << 26)

/* Function 0x80000001 */
#define AMD_MMX_EXT	(1 << 22)
#define AMD_MMX		(1 << 23)
#define AMD_SSE		(1 << 25)
#define AMD_LONG_MODE	(1 << 29)
#define AMD_3DNOW_EXT	(1 << 30)
#define AMD_3DNOW	(1 << 31)

/* Function 0x80000001 */
#define CYRIX_MMX	(1 << 23)
#define CYRIX_MMX_EXT	(1 << 24)
#define CYRIX_3DNOW	(1 << 31)

cpu_feature_set
cpu_detection_mmx		(void)
{
	cpu_feature_set features;

	features = 0;

	if (!toggle_eflags_id ()) {
		/* Has no CPUID. */
	} else {
		cpuid_t c;

		cpuid (&c, 0);

		if (0 == strncmp (c.s + 4, "GenuineIntel", 12)) {
			cpuid (&c, 1);

			if (c.r.edx & INTEL_TSC)
				features |= CPU_FEATURE_TSC;
			if (c.r.edx & INTEL_CMOV)
				features |= CPU_FEATURE_CMOV;
			if (c.r.edx & INTEL_MMX)
				features |= CPU_FEATURE_MMX;
			if (c.r.edx & INTEL_SSE)
				features |= CPU_FEATURE_SSE;
			if (c.r.edx & INTEL_SSE2)
				features |= CPU_FEATURE_SSE2;
		} else if (0 == strncmp (c.s + 4, "AuthenticAMD", 12)) {
			cpuid (&c, 1);

			if (c.r.edx & INTEL_TSC)
				features |= CPU_FEATURE_TSC;
			if (c.r.edx & INTEL_CMOV)
				features |= CPU_FEATURE_CMOV;
			if (c.r.edx & INTEL_MMX)
				features |= CPU_FEATURE_MMX;
			if (c.r.edx & INTEL_SSE)
				features |= CPU_FEATURE_SSE;
			if (c.r.edx & INTEL_SSE2)
				features |= CPU_FEATURE_SSE2;

			if (cpuid (&c, 0x80000000) > 0x80000000) {
				cpuid (&c, 0x80000001);

				if (c.r.edx & AMD_MMX_EXT)
					features |= CPU_FEATURE_AMD_MMX;
				if (c.r.edx & AMD_MMX)
					features |= CPU_FEATURE_MMX;
				if (c.r.edx & AMD_SSE)
					features |= CPU_FEATURE_SSE;
				if (c.r.edx & AMD_3DNOW_EXT)
					features |= CPU_FEATURE_3DNOW_EXT;
				if (c.r.edx & AMD_3DNOW)
					features |= CPU_FEATURE_3DNOW;
			}
		} else if (0 == strncmp (c.s + 4, "CyrixInstead", 12)) {
			if (cpuid (&c, 0x80000000) > 0x80000000) {
				cpuid (&c, 0x80000001);

				if (c.r.edx & INTEL_TSC)
					features |= CPU_FEATURE_TSC;
				if (c.r.edx & CYRIX_MMX)
					features |= CPU_FEATURE_MMX;
				if (c.r.edx & CYRIX_MMX_EXT)
					features |= CPU_FEATURE_CYRIX_MMX;
				if (c.r.edx & CYRIX_3DNOW)
					features |= CPU_FEATURE_3DNOW;
			} else {
				cpuid (&c, 1);

				if (c.r.edx & INTEL_TSC)
					features |= CPU_FEATURE_TSC;
				if (c.r.edx & INTEL_MMX)
					features |= CPU_FEATURE_MMX;
			}
		} else if (0 == strncmp (c.s + 4, "CentaurHauls", 12)) {
			cpuid (&c, 1);

			if (c.r.edx & INTEL_TSC)
				features |= CPU_FEATURE_TSC;
			if (c.r.edx & INTEL_CMOV)
				features |= CPU_FEATURE_CMOV;
			if (c.r.edx & INTEL_MMX)
				features |= CPU_FEATURE_MMX;
			if (c.r.edx & INTEL_SSE)
				features |= CPU_FEATURE_SSE;
		}
	}

	return features;
}
