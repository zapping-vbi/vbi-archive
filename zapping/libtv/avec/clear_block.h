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

/* $Id: clear_block.h,v 1.3.2.2 2005-06-17 02:54:19 mschimek Exp $ */

#include <inttypes.h>
#include <altivec.h>
#include "libtv/macros.h"
#include "avec.h"

/* Template for AltiVec clear_block functions. */

/* ATTENTION d and bytes_per_line must be 16 byte aligned. */

#undef STORE
#undef NAME3
#undef NAME2
#undef NAME

#define NAME3(b) clear_block ## b ## _altivec
#define NAME2(b) NAME3 (b)
#define NAME NAME2 (BPP)

#if 3 == BPP

static void
NAME				(void *			d,
				 unsigned int		value,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		bytes_per_line)
{
	const vector char sel0 = { 1,2,3,1, 2,3,1,2, 3,1,2,3, 1,2,3,1 };
	const vector char sel1 = { 2,3,1,2, 3,1,2,3, 1,2,3,1, 2,3,1,2 };
	const vector char sel2 = { 3,1,2,3, 1,2,3,1, 2,3,1,2, 3,1,2,3 };
	vector char v0, v1, v2, v3;
	unsigned long padding;

	v3 = (vector char)((vector int){ value, value, value, value });

	v0 = vec_perm (v3, v3, sel0);
	v1 = vec_perm (v3, v3, sel1);
	v2 = vec_perm (v3, v3, sel2);

	padding = bytes_per_line - width * 3;

	if (__builtin_expect (0 == padding, TRUE)) {
		width *= height;
		height = 1;

		if (__builtin_expect (0 == width % 16, TRUE)) {
			vector char *p = d;
			unsigned int count;

			for (count = width / 16; count > 0; --count) {
				/* This is vec_st, not vec_stl (which marks
				   the cache line as LRU). */
				p[0] = v0;
				p[1] = v1;
				p[2] = v2;
				p += 3;
			}

			return;
		}
	}

	for (; height > 0; --height) {
		vector char *p = d;
		uint8_t *q;
		unsigned int count;

		for (count = width / 16; count > 0; --count) {
			p[0] = v0;
			p[1] = v1;
			p[2] = v2;
			p += 3;
		}

		q = (uint8_t *) p;

		for (count = width % 16; count > 0; --count) {
			q[0] = value;
			q[1] = value >> 8;
			q[2] = value >> 16;
			q += 3;
		}

		d = ((uint8_t *) q) + padding;
	}
}

#else /* 3 != BPP */

static void
NAME				(void *			d,
				 unsigned int		value,
				 unsigned int		width,
				 unsigned int		height,
				 unsigned long		bytes_per_line)
{
	vector char v0;
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
		assert (0);
	}

	v0 = (vector char)((vector int){ value, value, value, value });

	padding = bytes_per_line - width * BPP;

	if (__builtin_expect (0 == padding, TRUE)) {
		width *= height;
		height = 1;

		if (__builtin_expect (0 == width % (64 / BPP), TRUE)) {
			vector char *p = d;
			unsigned int count;

			for (count = width / (64 / BPP); count > 0; --count) {
				p[0] = v0;
				p[1] = v0;
				p[2] = v0;
				p[3] = v0;
				p += 4;
			}
			
			return;
		}
	}

	for (; height > 0; --height) {
		vector char *p = d;
		unsigned int count;

		for (count = width / (64 / BPP); count > 0; --count) {
			p[0] = v0;
			p[1] = v0;
			p[2] = v0;
			p[3] = v0;
			p += 4;
		}

		count = width % (64 / BPP);

		switch (BPP) {
		case 1:
		{
			uint8_t *q;

			for (q = (uint8_t *) p; count > 0; --count)
				*q++ = value;

			p = (vector char *) q;

			break;
		}

		case 2:
		{
			uint16_t *q;

			for (q = (uint16_t *) p; count > 0; --count)
				*q++ = value;

			p = (vector char *) q;

			break;
		}

		case 4:
		{
			uint32_t *q;

			for (q = (uint32_t *) p; count > 0; --count)
				*q++ = value;

			p = (vector char *) q;

			break;
		}

		default:
			assert (0);
		}

		d = ((uint8_t *) p) + padding;
	}
}

#endif

#undef BPP
