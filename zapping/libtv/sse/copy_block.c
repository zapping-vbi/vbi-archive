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

/* $Id: copy_block.c,v 1.3 2005-06-28 01:01:30 mschimek Exp $ */

#include <inttypes.h>		/* uint8_t */
#include <xmmintrin.h>
#include "libtv/macros.h"	/* TRUE */
#include "libtv/mmx/mmx.h"	/* copy_block1_mmx() */
#include "sse.h"

/* ATTENTION src and dst must be 16 byte aligned. */
void
memcpy_sse_nt			(void *			dst,
				 const void *		src,
				 size_t			n_bytes)
{
	__m128 *d = (__m128 *) dst;
	const __m128 *s	= (const __m128 *) src;
	unsigned int count;

	for (count = n_bytes / 128; count > 0; --count) {
		__m128 m0, m1, m2, m3, m4, m5, m6, m7;

		m0 = s[0];
		m1 = s[1];
		m2 = s[2];
		m3 = s[3];
		m4 = s[4];
		m5 = s[5];
		m6 = s[6];
		m7 = s[7];
		s += 8;

		_mm_stream_ps ((float *)(d + 0), m0);
		_mm_stream_ps ((float *)(d + 1), m1);
		_mm_stream_ps ((float *)(d + 2), m2);
		_mm_stream_ps ((float *)(d + 3), m3);
		_mm_stream_ps ((float *)(d + 4), m4);
		_mm_stream_ps ((float *)(d + 5), m5);
		_mm_stream_ps ((float *)(d + 6), m6);
		_mm_stream_ps ((float *)(d + 7), m7);
		d += 8;
	}

	count = n_bytes % 128;

	__asm__ __volatile__ (" cld\n"
			      " rep movsb\n"
			      : "+D" (d), "+S" (s), "+c" (count)
			      :: "cc", "memory");    
}

void
copy_block1_sse_nt		(void *			dst,
				 const void *		src,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		dst_bytes_per_line,
				 unsigned long		src_bytes_per_line)
{
	unsigned long align;
	unsigned long dst_padding;
	unsigned long src_padding;

	align = ((unsigned long) dst |
		 (unsigned long) src |
		 dst_bytes_per_line |
		 src_bytes_per_line);

	if (unlikely (0 != align % 16)) {
#ifdef HAVE_MMX
		copy_block1_mmx (dst, src,
				 width, height,
				 dst_bytes_per_line,
				 src_bytes_per_line);
#else
		copy_block1_generic (dst, src,
				     width, height,
				     dst_bytes_per_line,
				     src_bytes_per_line);
#endif
		return;
	}

	dst_padding = dst_bytes_per_line - width * 1;
	src_padding = src_bytes_per_line - width * 1;

	if (likely (0 == (dst_padding | src_padding))) {
		width *= height;
		height = 1;
	}

	for (; height > 0; --height) {
		__m128 *d = (__m128 *) dst;
		const __m128 *s = (const __m128 *) src;
		unsigned int count;

		for (count = width / 128; count > 0; --count) {
			__m128 m0, m1, m2, m3, m4, m5, m6, m7;

			m0 = s[0];
			m1 = s[1];
			m2 = s[2];
			m3 = s[3];
			m4 = s[4];
			m5 = s[5];
			m6 = s[6];
			m7 = s[7];
			s += 8;

			_mm_stream_ps ((float *)(d + 0), m0);
			_mm_stream_ps ((float *)(d + 1), m1);
			_mm_stream_ps ((float *)(d + 2), m2);
			_mm_stream_ps ((float *)(d + 3), m3);
			_mm_stream_ps ((float *)(d + 4), m4);
			_mm_stream_ps ((float *)(d + 5), m5);
			_mm_stream_ps ((float *)(d + 6), m6);
			_mm_stream_ps ((float *)(d + 7), m7);
			d += 8;
		}

		count = width % 128;

		__asm__ __volatile__ (" cld\n"
				      " rep movsb\n"
				      : "+D" (d), "+S" (s), "+c" (count)
				      :: "cc", "memory");

		dst = ((uint8_t *) d) + dst_padding;
		src = ((const uint8_t *) s) + src_padding;
	}
}
