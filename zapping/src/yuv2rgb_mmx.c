/*
 * yuv2rgb_mmx.c, Software YUV to RGB coverter with Intel MMX "technology"
 *
 * Copyright (C) 2000, Silicon Integrated System Corp.
 * All Rights Reserved.
 *
 * Author: Olie Lho <ollie@sis.com.tw>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video decoder
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Make; see the file COPYING. If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#ifdef USE_MMX /* compile an empty file is mmx support is disabled */

#define CPU_UNKNOWN		0	/* no MMX */
#define	CPU_PENTIUM_MMX		1	/* MMX; late P5 core */
#define CPU_PENTIUM_II		2	/* MMX, CMOV; any P6 core */
#define	CPU_PENTIUM_III		3	/* MMX, CMOV, SSE; any P6 core and Itanium x86 (what a waste) */
#define	CPU_PENTIUM_4		4	/* MMX, CMOV, SSE, SSE2; any P8 core */
#define CPU_K6_2		5	/* MMX, 3DNOW; K6-2/K6-III */
#define CPU_ATHLON		6	/* MMX, 3DNOW, AMD 3DNOW ext, CMOV, SSE int; K7 core */
#define CPU_CYRIX_MII		7	/* MMX, CMOV */
#define CPU_CYRIX_III		8	/* MMX, Cyrix MMX ext, 3DNOW, CMOV */

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

static unsigned int
cpuid(cpuid_t *buf, unsigned int level)
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

static int
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

	return CPU_UNKNOWN;
}

#else /* !cpu x86 */

int
cpu_detection(cpu_architecture arch)
{
	return CPU_UNKNOWN;
}

#endif /* !cpu x86 */

#include "yuv2rgb.h"
#include "gen_conv.h"

yuv2rgb_fun yuv2rgb_init_mmx (int bpp, int mode)
{
  switch (cpu_detection(void))
    {
    case CPU_PENTIUM_MMX:
    case CPU_PENTIUM_II:
    case CPU_PENTIUM_III:
      /* removed */
    case CPU_PENTIUM_4:
      /* to do */
    case CPU_K6_2:
      /* removed */
    case CPU_CYRIX_MII:
    case CPU_CPU_CYRIX_III:
      /* removed */
      switch (bpp)
        {
        case 15:
          return (mode == MODE_BGR ? mmx_yuv420_rgb5551 :
    	      mmx_yuv420_bgr5551);
        case 16:
          return (mode == MODE_BGR ? mmx_yuv420_rgb565 :
    	      mmx_yuv420_bgr565);
        case 24:
          return (mode == MODE_BGR ? mmx_yuv420_rgb24 :
    	      mmx_yuv420_bgr24);
        case 32:
          return (mode == MODE_BGR ? mmx_yuv420_rgb32 :
	      mmx_yuv420_bgr32);
        default:
          break;
        }
      break;

    case CPU_ATHLON:
      switch (bpp)
        {
        case 15:
          return (mode == MODE_BGR ? amd_yuv420_rgb5551 :
    	      amd_yuv420_bgr5551);
        case 16:
          return (mode == MODE_BGR ? amd_yuv420_rgb565 :
    	      amd_yuv420_bgr565);
        case 24:
          return (mode == MODE_BGR ? amd_yuv420_rgb24 :
    	      amd_yuv420_bgr24);
        case 32:
          return (mode == MODE_BGR ? amd_yuv420_rgb32 :
	      amd_yuv420_bgr32);
        default:
          break;
        }
      break;

    default:
      break;
    }

  return NULL; // Fallback to C.
}

yuyv2rgb_fun yuyv2rgb_init_mmx (int bpp, int mode)
{
  switch (cpu_detection(void))
    {
    case CPU_PENTIUM_MMX:
    case CPU_PENTIUM_II:
    case CPU_PENTIUM_III:
      /* removed */
    case CPU_PENTIUM_4:
      /* to do */
    case CPU_K6_2:
      /* removed */
    case CPU_CYRIX_MII:
    case CPU_CPU_CYRIX_III:
      /* removed */
      switch (bpp)
        {
        case 15:
          return (mode == MODE_BGR ? mmx_yuyv_rgb5551 :
    	      mmx_yuyv_bgr5551);
        case 16:
          return (mode == MODE_BGR ? mmx_yuyv_rgb565 :
    	      mmx_yuyv_bgr565);
        case 24:
          return (mode == MODE_BGR ? mmx_yuyv_rgb24 :
    	      mmx_yuyv_bgr24);
        case 32:
          return (mode == MODE_BGR ? mmx_yuyv_rgb32 :
	      mmx_yuyv_bgr32);
        default:
          break;
        }
      break;

    case CPU_ATHLON:
      switch (bpp)
        {
        case 15:
          return (mode == MODE_BGR ? amd_yuyv_rgb5551 :
    	      amd_yuyv_bgr5551);
        case 16:
          return (mode == MODE_BGR ? amd_yuyv_rgb565 :
    	      amd_yuyv_bgr565);
        case 24:
          return (mode == MODE_BGR ? amd_yuyv_rgb24 :
    	      amd_yuyv_bgr24);
        case 32:
          return (mode == MODE_BGR ? amd_yuyv_rgb32 :
	      amd_yuyv_bgr32);
        default:
          break;
        }
      break;

    default:
      break;
    }

  return NULL; // Fallback to C.
}

#endif /* USE_MMX */
