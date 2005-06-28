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

/* $Id: clear_block.h,v 1.3 2005-06-28 01:01:50 mschimek Exp $ */

#include <inttypes.h>
#include "libtv/macros.h"

/* Template for MMX clear_block functions. */

#undef STORE
#undef NAME3
#undef NAME2
#undef NAME

#if SSE_NON_TEMPORAL
#  include <xmmintrin.h>
#  define STORE(p, m) _mm_stream_pi (&(p), m)
#  define NAME3(b) static void clear_block ## b ## _mmx_nt
#else
#  include <mmintrin.h>
#  define STORE(p, m) (p) = (m)
#  define NAME3(b) static void clear_block ## b ## _mmx
#endif

#define NAME2(b) NAME3 (b)
#define NAME NAME2 (BPP)

#if 3 == BPP

NAME				(void *			dst,
				 unsigned int		value,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		bytes_per_line)
{
	__m64 m0, m1, m2;
	unsigned long padding;

	m0 = _mm_cvtsi32_si64 (value & 0xFFFFFF);		/*      210 */
	m0 = _mm_or_si64 (m0, _mm_slli_si64 (m0, 24));		/*   210210 */
	m0 = _mm_or_si64 (m0, _mm_slli_si64 (m0, 24));		/* 10210210 */
	m1 = _mm_unpacklo_pi32 (_mm_srli_si64 (m0, 16), m0);	/* 02102102 */
	m2 = _mm_unpacklo_pi32 (_mm_srli_si64 (m1, 16), m1);	/* 21021021 */

	padding = bytes_per_line - width * 3;

	if (likely (0 == padding)) {
		width *= height;
		height = 1;

		if (likely (0 == width % 16)) {
			__m64 *d = (__m64 *) dst;
			unsigned int count;

			for (count = width / 16; count > 0; --count) {
				STORE (d[0], m0);
				STORE (d[1], m1);
				STORE (d[2], m2);
				STORE (d[3], m0);
				STORE (d[4], m1);
				STORE (d[5], m2);
				d += 6;
			}

			_mm_empty ();

			return;
		}
	}

	for (; height > 0; --height) {
		__m64 *d = (__m64 *) dst;
		uint8_t *d1;
		unsigned int count;

		for (count = width / 16; count > 0; --count) {
			STORE (d[0], m0);
			STORE (d[1], m1);
			STORE (d[2], m2);
			STORE (d[3], m0);
			STORE (d[4], m1);
			STORE (d[5], m2);
			d += 6;
		}

		d1 = (uint8_t *) d;

		for (count = width % 16; count > 0; --count) {
			uint16_t *d2 = (uint16_t *) d1;

			d2[0] = value;
			d1[2] = value >> 16;
			d1 += 3;
		}

		dst = ((uint8_t *) d1) + padding;
	}

	_mm_empty ();
}

#else

NAME				(void *			dst,
				 unsigned int		value,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		bytes_per_line)
{
	__m64 m0;
	unsigned long padding;

	switch (BPP) {
	case 1:
		value = (value & 0xFF) * 0x01010101;
		break;

	case 2:
		value = (value & 0xFFFF) * 0x00010001;
		break;

	case 4:
		break;

	default:
		assert (!"reached");
	}

	m0 = _mm_cvtsi32_si64 (value);
	m0 = _mm_unpacklo_pi32 (m0, m0);	/* 32103210 */

	padding = bytes_per_line - width * BPP;

	if (likely (0 == padding)) {
		width *= height;
		height = 1;

		if (likely (0 == width % (64 / BPP))) {
			__m64 *d = (__m64 *) dst;
			unsigned int count;

			for (count = width / (64 / BPP); count > 0; --count) {
				STORE (d[0], m0);
				STORE (d[1], m0);
				STORE (d[2], m0);
				STORE (d[3], m0);
				STORE (d[4], m0);
				STORE (d[5], m0);
				STORE (d[6], m0);
				STORE (d[7], m0);
				d += 8;
			}

			_mm_empty ();

			return;
		}
	}

	for (; height > 0; --height) {
		__m64 *d = (__m64 *) dst;
		unsigned int count;

		for (count = width / (64 / BPP); count > 0; --count) {
			STORE (d[0], m0);
			STORE (d[1], m0);
			STORE (d[2], m0);
			STORE (d[3], m0);
			STORE (d[4], m0);
			STORE (d[5], m0);
			STORE (d[6], m0);
			STORE (d[7], m0);
			d += 8;
		}

		/* Note accessing padding bytes, including bytes not written
		   by maskmovq, can cause a page fault. */

		count = width % (64 / BPP);

		switch (BPP) {
		case 1:
			__asm__ __volatile__
				(" cld\n"
				 " rep stosb\n"
				 : "+D" (d), "+c" (count)
				 : "a" (value)
				 : "cc", "memory");
			break;

		case 2:
			__asm__ __volatile__
				(" cld\n"
				 " rep stosw\n"
				 : "+D" (d), "+c" (count)
				 : "a" (value)
				 : "cc", "memory");
			break;

		case 4:
			__asm__ __volatile__
				(" cld\n"
				 " rep stosl\n"
				 : "+D" (d), "+c" (count)
				 : "a" (value)
				 : "cc", "memory");
			break;

		default:
			assert (!"reached");
		}

		dst = ((uint8_t *) d) + padding;
	}

	_mm_empty ();
}

#endif

#undef BPP
