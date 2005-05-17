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

/* $Id: cpu.c,v 1.1.2.1 2005-05-17 19:58:31 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdlib.h>		/* strtoul() */
#include <string.h>		/* strcmp() */
#include <ctype.h>		/* isspace() et al */

#include "cpu.h"
#include "mmx/mmx.h"
#include "avec/avec.h"

cpu_feature_set			cpu_features;

cpu_feature_set
cpu_detection			(void)
{
	cpu_features = 0;

#if defined (HAVE_MMX)
	cpu_features = cpu_detection_mmx ();
#elif defined (HAVE_ALTIVEC)
	cpu_features = cpu_detection_altivec ();
#endif

	return cpu_features;
}

static struct {
	const char *		name;
	cpu_feature_set		feature;
} features [] = {
	{ "tsc",		CPU_FEATURE_TSC },
	{ "cmov",		CPU_FEATURE_CMOV },
	{ "mmx",		CPU_FEATURE_MMX },
	{ "sse",		CPU_FEATURE_SSE },
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
