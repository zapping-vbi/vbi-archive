/*
 *  Copyright (C) 2004, 2006 Michael H. Schimek
 *
 *  AltiVec test based on code from Xine
 *  Copyright (C) 1999-2001 Aaron Holtzman
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

/* $Id: cpu.c,v 1.6 2007-08-30 14:14:02 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>		/* strtoul() */
#include <string.h>		/* strcmp() */
#include <ctype.h>		/* isspace() */
#include <signal.h>		/* signal() */
#include <setjmp.h>		/* sigsetjmp () */
#include <assert.h>
#include "misc.h"
#include "cpu.h"

cpu_feature_set			cpu_features;

static sigjmp_buf		jmpbuf;

#ifdef CAN_COMPILE_MMX

/*
  References

  "Intel Processor Identification and the CPUID Instruction",
  Application Note AP-485, February 2005, order no. 241618-028,
  http://developer.intel.com

  "AMD Processor Recognition Application Note",
  publication # 20734, Rev. R, June 2000.
  http://www.amd.com/products/cpg/athlon/techdocs/index.html

  "Cyrix CPU Detection Guide",
  Application Note 112, Rev. 1.9, July 21, 1998
  formerly available from http://www.cyrix.com/html/developers/index.htm
  when Cyrix was part of National Semiconductor.
  VIA has no similar document available as of Jan 2001.
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

#endif /* x86-64 */

/* Function 0x1 */
#define INTEL_EDX_TSC	(1 << 4)
#define INTEL_EDX_CMOV	(1 << 15)
#define INTEL_EDX_MMX	(1 << 23)
#define INTEL_EDX_SSE	(1 << 25)
#define INTEL_EDX_SSE2	(1 << 26)
#define INTEL_ECX_SSE3	(1 << 0)

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

#endif /* CAN_COMPILE_MMX */

static void
sigill_handler			(int			sig)
{
	sig = sig; /* unused */

	siglongjmp (jmpbuf, 1);
}

cpu_feature_set
cpu_detection			(void)
{
	cpu_features = 0;

#if defined (CAN_COMPILE_MMX)
	if (!toggle_eflags_id ()) {
		/* Has no CPUID. */
	} else {
		cpuid_t c;

		cpuid (&c, 0);

		if (0 == strncmp (c.s + 4, "GenuineIntel", 12)) {
			cpuid (&c, 1);

			if (c.r.edx & INTEL_EDX_TSC)
				cpu_features |= CPU_FEATURE_TSC;
			if (c.r.edx & INTEL_EDX_CMOV)
				cpu_features |= CPU_FEATURE_CMOV;
			if (c.r.edx & INTEL_EDX_MMX)
				cpu_features |= CPU_FEATURE_MMX;
			if (c.r.edx & INTEL_EDX_SSE)
				cpu_features |= (CPU_FEATURE_SSE_INT |
						 CPU_FEATURE_SSE_FLT);
			if (c.r.edx & INTEL_EDX_SSE2)
				cpu_features |= CPU_FEATURE_SSE2;
			if (c.r.ecx & INTEL_ECX_SSE3)
				cpu_features |= CPU_FEATURE_SSE3;
		} else if (0 == strncmp (c.s + 4, "AuthenticAMD", 12)) {
			cpuid (&c, 1);

			if (c.r.edx & INTEL_EDX_TSC)
				cpu_features |= CPU_FEATURE_TSC;
			if (c.r.edx & INTEL_EDX_CMOV)
				cpu_features |= CPU_FEATURE_CMOV;
			if (c.r.edx & INTEL_EDX_MMX)
				cpu_features |= CPU_FEATURE_MMX;
			if (c.r.edx & INTEL_EDX_SSE)
				cpu_features |= (CPU_FEATURE_SSE_INT |
						 CPU_FEATURE_SSE_FLT);
			if (c.r.edx & INTEL_EDX_SSE2)
				cpu_features |= CPU_FEATURE_SSE2;
			if (c.r.ecx & INTEL_ECX_SSE3)
				cpu_features |= CPU_FEATURE_SSE3;

			if (cpuid (&c, 0x80000000) > 0x80000000) {
				cpuid (&c, 0x80000001);

				if (c.r.edx & AMD_MMX_EXT)
					cpu_features |= CPU_FEATURE_AMD_MMX;
				if (c.r.edx & AMD_MMX)
					cpu_features |= CPU_FEATURE_MMX;
				if (c.r.edx & AMD_SSE)
					cpu_features |= (CPU_FEATURE_SSE_INT |
							 CPU_FEATURE_SSE_FLT);
				if (c.r.edx & AMD_3DNOW_EXT)
					cpu_features |= CPU_FEATURE_3DNOW_EXT;
				if (c.r.edx & AMD_3DNOW)
					cpu_features |= CPU_FEATURE_3DNOW;
			}
		} else if (0 == strncmp (c.s + 4, "CyrixInstead", 12)) {
			if (cpuid (&c, 0x80000000) > 0x80000000) {
				cpuid (&c, 0x80000001);

				if (c.r.edx & INTEL_EDX_TSC)
					cpu_features |= CPU_FEATURE_TSC;
				if (c.r.edx & CYRIX_MMX)
					cpu_features |= CPU_FEATURE_MMX;
				if (c.r.edx & CYRIX_MMX_EXT)
					cpu_features |= CPU_FEATURE_CYRIX_MMX;
				if (c.r.edx & CYRIX_3DNOW)
					cpu_features |= CPU_FEATURE_3DNOW;
			} else {
				cpuid (&c, 1);

				if (c.r.edx & INTEL_EDX_TSC)
					cpu_features |= CPU_FEATURE_TSC;
				if (c.r.edx & INTEL_EDX_MMX)
					cpu_features |= CPU_FEATURE_MMX;
			}
		} else if (0 == strncmp (c.s + 4, "CentaurHauls", 12)) {
			cpuid (&c, 1);

			if (c.r.edx & INTEL_EDX_TSC)
				cpu_features |= CPU_FEATURE_TSC;
			if (c.r.edx & INTEL_EDX_CMOV)
				cpu_features |= CPU_FEATURE_CMOV;
			if (c.r.edx & INTEL_EDX_MMX)
				cpu_features |= CPU_FEATURE_MMX;
			if (c.r.edx & INTEL_EDX_SSE)
				cpu_features |= (CPU_FEATURE_SSE_INT |
						 CPU_FEATURE_SSE_FLT);
		}

		if (cpu_features & (CPU_FEATURE_SSE_INT |
				    CPU_FEATURE_SSE_FLT |
				    CPU_FEATURE_SSE2 |
				    CPU_FEATURE_SSE3)) {
			/* Test if SSE is supported by the kernel. */

			if (0 == sigsetjmp (jmpbuf, 1)) {
				signal (SIGILL, sigill_handler);

				/* Suggested by Intel AP-485 chapter 9. */
				__asm__ __volatile__ (" orps %xmm2,%xmm1\n");
			} else {
				cpu_features &= ~(CPU_FEATURE_SSE_INT |
						  CPU_FEATURE_SSE_FLT |
						  CPU_FEATURE_SSE2 |
						  CPU_FEATURE_SSE3);
			}

			signal (SIGILL, SIG_DFL);
		}
	}

#elif defined (CAN_COMPILE_ALTIVEC)
	if (0 == sigsetjmp (jmpbuf, 1)) {
		signal (SIGILL, sigill_handler);

		__asm__ __volatile__ (" mtspr 256, %0\n"
				      " vand %%v0, %%v0, %%v0\n"
				      :: "r" (-1));

		cpu_features |= CPU_FEATURE_ALTIVEC;
	}

	signal (SIGILL, SIG_DFL);
#endif

	return cpu_features;
}

static struct {
	const char *		name;
	cpu_feature_set		feature;
} features [] = {
	{ "none",		0 },
	{ "tsc",		CPU_FEATURE_TSC },
	{ "cmov",		CPU_FEATURE_CMOV },
	{ "mmx",		CPU_FEATURE_MMX },
	{ "sse",		(CPU_FEATURE_SSE_INT |
				 CPU_FEATURE_SSE_FLT) },
	{ "sse2",		CPU_FEATURE_SSE2 },
	{ "amd-mmx",		CPU_FEATURE_AMD_MMX },
	{ "3dnow",		CPU_FEATURE_3DNOW },
	{ "3dnow-ext",		CPU_FEATURE_3DNOW_EXT },
	{ "cyrix-mmx",		CPU_FEATURE_CYRIX_MMX },
	{ "altivec",		CPU_FEATURE_ALTIVEC },
	{ "sse3",		CPU_FEATURE_SSE3 },
};

cpu_feature_set
cpu_feature_set_from_string	(const char *		s)
{
	cpu_feature_set cpu_features;

	assert (NULL != s);

	cpu_features = 0;

	while (0 != *s) {
		unsigned int i;
		char *tail;

		if ('|' == *s || isspace (*s)) {
			++s;
			continue;
		}

		for (i = 0; i < N_ELEMENTS (features); ++i) {
			if (0 == strcmp (features[i].name, s)) {
				unsigned int n;

				n = strlen (features[i].name);

				if (0 == s[n]
				    || '|' == s[n]
				    || isspace (s[n])) {
					s += n;
					break;
				}
			}
		}

		if (i < N_ELEMENTS (features)) {
			cpu_features |= features[i].feature;
			continue;
		}

		/* No keyword. */

		cpu_features |= (cpu_feature_set) strtoul (s, &tail, 0);

		if (tail > s) {
			s = tail;
			continue;
		}

		/* No number. */

		return (cpu_feature_set) 0;
	}

	return cpu_features;
}

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
