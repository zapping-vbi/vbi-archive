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

/* $Id: copy_block.c,v 1.5 2006-03-06 01:49:31 mschimek Exp $ */

#include <inttypes.h>
#include <mmintrin.h>
#include "libtv/macros.h"
#include "mmx.h"

void
memcpy_mmx			(void *			dst,
				 const void *		src,
				 size_t			n_bytes)
{
	__m64 *d = (__m64 *) dst;
	const __m64 *s = (const __m64 *) src;
	unsigned int count;

	for (count = n_bytes / 64; count > 0; --count) {
		__m64 m0, m1, m2, m3, m4, m5, m6, m7;

		m0 = s[0];
		m1 = s[1];
		m2 = s[2];
		m3 = s[3];
		m4 = s[4];
		m5 = s[5];
		m6 = s[6];
		m7 = s[7];
		s += 8;

		d[0] = m0;
		d[1] = m1;
		d[2] = m2;
		d[3] = m3;
		d[4] = m4;
		d[5] = m5;
		d[6] = m6;
		d[7] = m7;
		d += 8;
	}

	count = n_bytes % 64;

	__asm__ __volatile__ (" cld\n"
			      " rep movsb\n"
			      : "+D" (d), "+S" (s), "+c" (count)
			      :: "cc", "memory");    
}

void
copy_plane_MMX			(uint8_t *		dst,
				 const uint8_t *	src,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		dst_padding,
				 unsigned long		src_padding)
{
	unsigned long align;

	/* XXX check that in caller. */
	align = ((unsigned long) dst |
		 (unsigned long) src |
		 (width + dst_padding) |
		 (width + src_padding) |
		 width);

	if (unlikely ((align & 7) > 0)) {
		return copy_plane_SCALAR (dst, src,
					  width, height,
					  dst_padding,
					  src_padding);
	}

	for (; height > 0; --height) {
		__m64 *d = (__m64 *) dst;
		const __m64 *s = (const __m64 *) src;
		unsigned int count;

		for (count = width / 64; count > 0; --count) {
			__m64 m0, m1, m2, m3, m4, m5, m6, m7;

			m0 = s[0];
			m1 = s[1];
			m2 = s[2];
			m3 = s[3];
			m4 = s[4];
			m5 = s[5];
			m6 = s[6];
			m7 = s[7];
			s += 8;

			d[0] = m0;
			d[1] = m1;
			d[2] = m2;
			d[3] = m3;
			d[4] = m4;
			d[5] = m5;
			d[6] = m6;
			d[7] = m7;
			d += 8;
		}

		count = width % 64;

		__asm__ __volatile__ (" cld\n"
				      " rep movsb\n"
				      : "+D" (d), "+S" (s), "+c" (count)
				      :: "cc", "memory");

		dst = ((uint8_t *) d) + dst_padding;
		src = ((const uint8_t *) s) + src_padding;
	}

	_mm_empty ();
}
