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

/* $Id: simd.c,v 1.1 2005-03-30 21:20:19 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#undef NDEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libtv/cpu.h"

extern void test__MMX		(const char *		name);
extern void test__3DNOW		(const char *		name);
extern void test__SSE		(const char *		name);
extern void test__SSE2		(const char *		name);
extern void test__AVEC		(const char *		name);

#ifdef SIMD

#include "libtv/simd.h"

#define PASS(type, scalar, vector)					\
do {									\
	v ## type x, y;							\
	x = vsplat ## type (scalar);					\
	y = vector;							\
	assert (((void) #vector, 0 == memcmp (&x, &y, sizeof (x))));	\
} while (0)

#define FAIL(type, scalar, vector)					\
do {									\
	v ## type x, y;							\
	x = vsplat ## type (scalar);					\
	y = vector;							\
	assert (((void) #vector, 0 != memcmp (&x, &y, sizeof (x))));	\
} while (0)

#define MIN(n, max) (((n) < (max)) ? (n) : (max))
#define MAX(n, min) (((n) > (min)) ? (n) : (min))

/* Note that we always clip against both bounds as SIMD does.
   An if-else-else yields different results when max < min. */
#define SATURATE(n, min, max) MIN (MAX (n, min), max)

#define SAT16(n) SATURATE (n, -32768, 32767)
#define SATU16(n) SATURATE (n, 0, 65535)

void
SIMD_NAME (test)		(const char *		name)
{
	unsigned int i;

	fprintf (stderr, "simd %s\n", name);

	for (i = 0; i < 1000; ++i) {
		unsigned int a, b, c;
		int8_t sa8, sb8, sc8;
		int16_t sa16, sb16, sc16;
		uint8_t sau8, sbu8, scu8;
		uint16_t sau16, sbu16, scu16;
		v8 va8, vb8, vc8;
		v16 va16, vb16, vc16;
		vu8 vau8, vbu8, vcu8;
		vu16 vau16, vbu16, vcu16;

		a = rand ();
		b = rand ();
		c = rand ();

		sa8 = (int8_t) a;
		sb8 = (int8_t) b;
		sc8 = (int8_t) c;

		sau8 = (uint8_t) a;
		sbu8 = (uint8_t) b;
		scu8 = (uint8_t) c;

		sa16 = (int16_t) a;
		sb16 = (int16_t) b;
		sc16 = (int16_t) c;

		sau16 = (uint16_t) a;
		sbu16 = (uint16_t) b;
		scu16 = (uint16_t) c;

		va8 = vsplat8 (a);
		vb8 = vsplat8 (b);
		vc8 = vsplat8 (c);

		va16 = vsplat16 (a);
		vb16 = vsplat16 (b);
		vc16 = vsplat16 (c);

		vau8 = vsplatu8 (a);
		vbu8 = vsplatu8 (b);
		vcu8 = vsplatu8 (c);

		vau16 = vsplatu16 (a);
		vbu16 = vsplatu16 (b);
		vcu16 = vsplatu16 (c);

		/* Make sure the simd macros do what I think they do. */

		PASS (u8, a, vau8);
		FAIL (u8, ~a, vau8);

		PASS (u8, a & ~b, vandnot (vau8, vbu8));
		PASS (u8, ~a, vnot (vau8));
		PASS (u8, ~(a & b), vnand (vau8, vbu8));
		PASS (u8, ~(a | b), vnor (vau8, vbu8));
		PASS (u8, (b & c) | (a & ~c), vsel (vau8, vbu8, vcu8));
		PASS (u8, sau8 << 1, vsl18 (vau8));
		PASS (8, sa8 >> 1, vsr18 (vau8));
		PASS (u8, sau8 >> 1, vsr1u8 (vau8));
		PASS (u8, a - b, vsub8 (vau8, vbu8));
		PASS (16, SAT16 (sa16 - sb16), vsubs16 (va16, vb16));
		PASS (16, SATU16 (sau16 - sbu16), vsubsu16 (va16, vb16));
		PASS (u16, (a * b) & 0xFFFF, vmullo16 (va16, vb16));
		PASS (u8, SATURATE (sau8, sbu8, scu8),
		      vsatu8 (vau8, vbu8, vcu8));
		PASS (8, 0 - (sa8 > sb8), vcmpgt8 (va8, vb8));
		PASS (u8, 0 - (sau8 >= sbu8), vcmpgeu8 (vau8, vbu8));
		PASS (u8, abs (sau8 - sbu8), vabsdiffu8 (vau8, vbu8));

		PASS (u8, (sau8 + sbu8 + 1) / 2, vavgu8 (vau8, vbu8));

		if (SIMD == MMX) {
			PASS (u8, (sau8 / 2) + (sbu8 / 2),
			      fast_vavgu8 (vau8, vbu8));
		} else {
			PASS (u8, (sau8 + sbu8 + 1) / 2,
			      fast_vavgu8 (vau8, vbu8));
		}

		PASS (u8, (sau8 > sbu8) ? a : b, vmaxu8 (vau8, vbu8));
		PASS (u8, (sau8 < sbu8) ? a : b, vminu8 (vau8, vbu8));

		{
			vu8 min, max, x;

			vminmaxu8 (&min, &max, vau8, vbu8);
			x = vsplatu8 ((sau8 < sbu8) ? a : b);
			assert (0 == memcmp (&x, &min, sizeof (x)));
			x = vsplatu8 ((sau8 > sbu8) ? a : b);
			assert (0 == memcmp (&x, &max, sizeof (x)));
		}

		vempty ();
	}
}

#else /* !SIMD */

const int64_t vsplat8_m1[2] = { -1, -1 };
const int64_t vsplat8_1[2] = { 0x0101010101010101LL, 0x0101010101010101LL };
const int64_t vsplat8_127[2] = { 0x7F7F7F7F7F7F7F7FLL, 0x7F7F7F7F7F7F7F7FLL };

int
main				(int			argc,
				 char **		argv)
{
	cpu_feature_set features;

	(void) argc;
	(void) argv;

	features = cpu_detection ();

#ifdef HAVE_MMX
	if (features & CPU_FEATURE_MMX) {
		test__MMX ("mmx");
	}
#endif

#ifdef HAVE_SSE 
	if (features & CPU_FEATURE_SSE) {
		test__SSE ("sse");
	}
#endif

/* Untested */
/*
#ifdef HAVE_3DNOW 
	if (features & CPU_FEATURE_3DNOW) {
		test__3DNOW ("3dnow");
	}
#endif

#ifdef HAVE_SSE2 
	if (features & CPU_FEATURE_SSE2) {
		test__SSE2 ("sse2");
	}
#endif

#ifdef HAVE_ALTIVEC 
	if (features & CPU_FEATURE_ALTIVEC) {
		test__AVEC ("altivec");
	}
#endif
*/
	return EXIT_SUCCESS;
}

#endif /* !SIMD */
