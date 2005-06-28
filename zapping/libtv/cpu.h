/*
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: cpu.h,v 1.2 2005-06-28 00:52:54 mschimek Exp $ */

#ifndef CPU_H
#define CPU_H

/* x86 features */
#define CPU_FEATURE_TSC		(1 << 0)
#define CPU_FEATURE_CMOV	(1 << 1)
#define CPU_FEATURE_MMX		(1 << 2)
#define CPU_FEATURE_SSE		(1 << 3)
#define CPU_FEATURE_SSE2	(1 << 4)
#define CPU_FEATURE_AMD_MMX	(1 << 5)
#define CPU_FEATURE_3DNOW	(1 << 6)
#define CPU_FEATURE_3DNOW_EXT	(1 << 7)
#define CPU_FEATURE_CYRIX_MMX	(1 << 8)
#define CPU_FEATURE_SSE3	(1 << 10)

/* powerpc features */
#define CPU_FEATURE_ALTIVEC	(1 << 9)

typedef unsigned int cpu_feature_set;

extern cpu_feature_set		cpu_features;

extern cpu_feature_set
cpu_detection			(void);
extern cpu_feature_set
cpu_feature_set_from_string	(const char *		s);

#endif /* CPU_H */
