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

/* $Id: simd.c,v 1.1.2.4 2005-06-17 02:54:21 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#undef NDEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libtv/simd.h"

typedef void
test_fn				(void);

SIMD_FN_PROTOS (test_fn, test);

#if SIMD

union va {
	uint8_t		a8[sizeof (vu8) / 1];
	uint8_t		au8[sizeof (vu8) / 1];
	uint16_t	a16[sizeof (vu8) / 2];
	uint16_t	au16[sizeof (vu8) / 2];
	uint32_t	a32[sizeof (vu8) / 4];
	uint32_t	au32[sizeof (vu8) / 4];
};

#define PASS(type, scalar, vector)					\
do {									\
	union va x;							\
	v ## type y;							\
	unsigned int i;							\
	for (i = 0; i < sizeof (vu8) / sizeof (x.a ## type [0]); ++i)	\
		x.a ## type [i] = (scalar);				\
	y = (vector);							\
	assert (((void) #vector,					\
		 0 == memcmp (x.a ## type, &y, sizeof (y))));		\
} while (0)

#define PASS3(scalar, vector)						\
do {									\
	PASS (8, scalar, vector ## 8 (va8, vb8));			\
	PASS (16, scalar, vector ## 16 (va16, vb16));			\
	PASS (32, scalar, vector ## 32 (va32, vb32));			\
} while (0)

#define FAIL(type, scalar, vector)					\
do {									\
	union va x;							\
	v ## type y;							\
	unsigned int i;							\
	for (i = 0; i < sizeof (vu8) / sizeof (x.a ## type [0]); ++i)	\
		x.a ## type [i] = scalar;				\
	y = vector;							\
	assert (((void) #vector,					\
		 0 != memcmp (x.a ## type, &y, sizeof (y))));		\
} while (0)

#define MIN(n, max) (((n) < (max)) ? (n) : (max))
#define MAX(n, min) (((n) > (min)) ? (n) : (min))

/* Note that we always clip against both bounds as SIMD does.
   An if-else-else yields different results when max < min. */
#define SATURATE(n, min, max) MIN (MAX (n, min), max)

#define SAT8(n) SATURATE (n, -0x80, 0x7F)
#define SATU8(n) SATURATE (n, 0, 0xFF)
#define SAT16(n) SATURATE (n, -0x8000, 0x7FFF)
#define SATU16(n) SATURATE (n, 0, 0xFFFF)
#define SAT32(n) SATURATE (n, -0x8000000, 0x7FFFFFFF)
#define SATU32(n) SATURATE (n, 0, 0xFFFFFFFFU)

void
SIMD_NAME (test)		(void)
{
	unsigned int i;

	for (i = 0; i < 1000; ++i) {
		unsigned int a, b, c;
		int8_t sa8, sb8, sc8;
		uint8_t sau8, sbu8, scu8;
		int16_t sa16, sb16, sc16;
		uint16_t sau16, sbu16, scu16;
		int32_t sa32, sb32, sc32;
		uint32_t sau32, sbu32, scu32;
		v8 va8, vb8, vc8;
		v16 va16, vb16, vc16;
		v32 va32, vb32, vc32;
		vu8 vau8, vbu8, vcu8;
		vu16 vau16, vbu16, vcu16;
		vu32 vau32, vbu32, vcu32;

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

		sa32 = (int32_t) a;
		sb32 = (int32_t) b;
		sc32 = (int32_t) c;

		sau32 = (uint32_t) a;
		sbu32 = (uint32_t) b;
		scu32 = (uint32_t) c;

		va8 = vsplat8 (a);
		vb8 = vsplat8 (b);
		vc8 = vsplat8 (c);

		vau8 = vsplatu8 (a);
		vbu8 = vsplatu8 (b);
		vcu8 = vsplatu8 (c);

		va16 = vsplat16 (a);
		vb16 = vsplat16 (b);
		vc16 = vsplat16 (c);

		vau16 = vsplatu16 (a);
		vbu16 = vsplatu16 (b);
		vcu16 = vsplatu16 (c);

		va32 = vsplat32 (a);
		vb32 = vsplat32 (b);
		vc32 = vsplat32 (c);

		vau32 = vsplatu32 (a);
		vbu32 = vsplatu32 (b);
		vcu32 = vsplatu32 (c);

		/* Make sure the simd macros do what I think they do. */

		PASS (u8, a, vau8);
		FAIL (u8, ~a, vau8);

		PASS (8, 0, vzero8 ());
		PASS (16, 0, vzero16 ());
		PASS (32, 0, vzero32 ());

		PASS (u8, 0, vzerou8 ());
		PASS (u16, 0, vzerou16 ());
		PASS (u32, 0, vzerou32 ());

#if 1
		PASS (8, -1, vminus18 ());
		PASS (16, -1, vminus116 ());
		PASS (32, -1, vminus132 ());

		PASS (8, 1, vsplat8_1);
		PASS (8, -1, vsplat8_m1);
		PASS (8, 15, vsplat8_15);
		PASS (8, 127, vsplat8_127);
		PASS (16, 255, vsplat16_255);
		PASS (16, 256, vsplat16_256);
		PASS (16, -256, vsplat16_m256);
		PASS (32, 1, vsplat32_1);
		PASS (32, 2, vsplat32_2);

		// vsplat8,16,32,u,i
		// vload,32,64,u,nt
		// vstore,u,nt

		PASS (u8, a & b, vand (vau8, vbu8));
		PASS (u8, a & ~b, vandnot (vau8, vbu8));
		PASS (u8, a | b, vor (vau8, vbu8));
		PASS (u8, a ^ b, vxor (vau8, vbu8));
		PASS (u8, ~a, vnot (vau8));
		PASS (u8, ~(a & b), vnand (vau8, vbu8));
		PASS (u8, ~(a | b), vnor (vau8, vbu8));

		PASS (u8, (b & a) | (c & ~a), vsel (vau8, vbu8, vcu8));

		// vsl, vsru

		PASS (8, sa8 << 1, vsl18 (va8));
		PASS (8, sa8 >> 1, vsr18 (va8));
		PASS (u8, sau8 >> 1, vsr1u8 (vau8));

		PASS (16, sa16 << 7, vsl16 (va16, 7));
		PASS (16, sa16 >> 7, vsr16 (va16, 7));
		PASS (u16, sau16 >> 7, vsru16 (vau16, 7));

#if !(SIMD & CPU_FEATURE_ALTIVEC)
		PASS (16, sa16 << (sb8 & 15), vsl16 (va16, (sb8 & 15)));
		PASS (16, sa16 >> (sb8 & 15), vsr16 (va16, (sb8 & 15)));
		PASS (u16, sau16 >> (sb8 & 15), vsru16 (vau16, (sb8 & 15)));
#endif
		// vlsr
		// vshiftu2x

#if SIMD & (CPU_FEATURE_SSE |CPU_FEATURE_SSE2 | CPU_FEATURE_SSE3)
		// unpack
#endif

		PASS3 (a + b, vadd);
		PASS3 (a - b, vsub);

		PASS (16, SAT16 (sa16 + sb16), vadds16 (va16, vb16));
		PASS (16, SAT16 (sa16 - sb16), vsubs16 (va16, vb16));

		PASS (u8, SATU8 (sau8 + sbu8), vaddsu8 (vau8, vbu8));
		PASS (u8, SATU8 (sau8 - sbu8), vsubsu8 (vau8, vbu8));
		PASS (u16, SATU16 (sau16 + sbu16), vaddsu16 (vau16, vbu16));
		PASS (u16, SATU16 (sau16 - sbu16), vsubsu16 (vau16, vbu16));

		PASS (u8, SATURATE (sau8, sbu8, scu8),
		      vsatu8 (vau8, vbu8, vcu8));

		PASS (8, 0 - (sa8 == sb8), (v8) vcmpeq8 (va8, vb8));
		PASS (16, 0 - (sa16 == sb16), (v16) vcmpeq16 (va16, vb16));
		PASS (32, 0 - (sa32 == sb32), (v32) vcmpeq32 (va32, vb32));

		PASS (8, 0 - (0 == sb8), (v8) vcmpz8 (vb8));
		PASS (16, 0 - (0 == sb16), (v16) vcmpz16 (vb16));
		PASS (32, 0 - (0 == sb32), (v32) vcmpz32 (vb32));

		PASS (8, 0 - (0 != sb8), (v8) vcmpnz8 (vb8));
		PASS (16, 0 - (0 != sb16), (v16) vcmpnz16 (vb16));
		PASS (32, 0 - (0 != sb32), (v32) vcmpnz32 (vb32));

		PASS (8, 0 - (sa8 > sb8), (v8) vcmpgt8 (va8, vb8));
		PASS (16, 0 - (sa16 > sb16), (v16) vcmpgt16 (va16, vb16));
		PASS (32, 0 - (sa32 > sb32), (v32) vcmpgt32 (va32, vb32));
		PASS (8, 0 - (sa8 < sb8), (v8) vcmplt8 (va8, vb8));
		PASS (16, 0 - (sa16 < sb16), (v16) vcmplt16 (va16, vb16));
		PASS (32, 0 - (sa32 < sb32), (v32) vcmplt32 (va32, vb32));

		PASS (8, 0 - (sau8 >= sbu8), (v8) vcmpgeu8 (vau8, vbu8));
		PASS (8, 0 - (sau8 <= sbu8), (v8) vcmpleu8 (vau8, vbu8));

		PASS (u16, (a * b) & 0xFFFF, vmullo16 (va16, vb16));

		PASS (u8, abs (sau8 - sbu8), vabsdiffu8 (vau8, vbu8));

		PASS (u8, (sau8 + sbu8 + 1) / 2, vavgu8 (vau8, vbu8));

#if SIMD == CPU_FEATURE_MMX
			PASS (u8, (sau8 / 2) + (sbu8 / 2),
			      fast_vavgu8 (vau8, vbu8));
#else
			PASS (u8, (sau8 + sbu8 + 1) / 2,
			      fast_vavgu8 (vau8, vbu8));
#endif

#endif
		PASS (u8, (sau8 > sbu8) ? a : b, vmaxu8 (vau8, vbu8));
		PASS (u8, (sau8 < sbu8) ? a : b, vminu8 (vau8, vbu8));

		{
			vu8 min, max, x;
			unsigned int i;

			vminmaxu8 (&min, &max, vau8, vbu8);
			i = (sau8 < sbu8) ? a : b;
			x = vsplatu8 (i);
			assert (0 == memcmp (&x, &min, sizeof (x)));
			i = (sau8 > sbu8) ? a : b;
			x = vsplatu8 (i);
			assert (0 == memcmp (&x, &max, sizeof (x)));
		}

#if SIMD & (CPU_FEATURE_MMX | CPU_FEATURE_3DNOW)
		PASS (u16, 0, vminu16i (va16, 0));
		PASS (u16, (sau16 < 1234) ? a : 1234, vminu16i (va16, 1234));
		PASS (u16, (sau16 < 65535) ? a : 65535,
		      vminu16i (va16, 65535));
#else
		PASS (16, (sa16 > sb16) ? a : b, vmax16 (va16, vb16));
		PASS (16, (sa16 < sb16) ? a : b, vmin16 (va16, vb16));
#endif

		vempty ();
	}
}

#else /* !SIMD */

#define s8(n)  { n * 0x0101010101010101ULL, n * 0x0101010101010101ULL }
#define s16(n) { n * 0x0001000100010001ULL, n * 0x0001000100010001ULL }
#define s32(n) { n * 0x0000000100000001ULL, n * 0x0000000100000001ULL }
const int64_t vsplat8_m1[2]	= s8 (0xFF);
const int64_t vsplat8_1[2]	= s8 (1);
const int64_t vsplat8_127[2]	= s8 (127);
const int64_t vsplat8_15[2]	= s8 (15);
const int64_t vsplat16_255[2]	= s16 (255);
const int64_t vsplat16_256[2]	= s16 (256);
const int64_t vsplat16_m256[2]	= s16 (0xFF00);
const int64_t vsplat32_1[2]	= s32 (1);
const int64_t vsplat32_2[2]	= s32 (2);

int
main				(int			argc,
				 char **		argv)
{
	test_fn *testp;

	assert (2 == argc);

	cpu_features = cpu_feature_set_from_string (argv[1]);

	testp = SIMD_FN_SELECT (test,
				CPU_FEATURE_MMX | CPU_FEATURE_3DNOW |
				CPU_FEATURE_SSE | CPU_FEATURE_SSE2 |
				CPU_FEATURE_ALTIVEC);

	assert (NULL != testp);

	testp ();

	return EXIT_SUCCESS;
}

#endif /* !SIMD */
