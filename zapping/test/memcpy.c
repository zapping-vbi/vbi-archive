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

/* $Id: memcpy.c,v 1.5 2005-06-28 01:09:44 mschimek Exp $ */

#undef NDEBUG

#include <inttypes.h>
#include <string.h>
#include "libtv/image_format.h"
#include "libtv/cpu.h"

#include "guard.h"

static uint8_t *		sbuffer;
static uint8_t *		sbuffer_end;
static uint8_t *		dbuffer;
static uint8_t *		dbuffer_end;

static void
test1				(uint8_t *		dst,
				 uint8_t *		src,
				 unsigned int		n_bytes)
{
	unsigned int i;
	unsigned int size;
	uint8_t *p;

	memset (dbuffer, 0xAA, dbuffer_end - dbuffer);

	size = sbuffer_end - sbuffer;
	for (i = 0; i < size; ++i)
		sbuffer[i] = i;

	tv_memcpy (dst, src, n_bytes);

	for (p = dbuffer; p < dst;)
		assert (0xAA == *p++);

	for (i = 0; i < n_bytes; ++i)
		assert (*p++ == *src++);

	while (p < dbuffer_end)
		assert (0xAA == *p++);
}

int
main				(int			argc,
				 char **		argv)
{
	unsigned int buffer_size;
	unsigned int i;

	buffer_size = 64 << 10;

	sbuffer = guard_alloc (buffer_size);
	sbuffer_end = sbuffer + buffer_size;

	dbuffer = guard_alloc (buffer_size);
	dbuffer_end = dbuffer + buffer_size;

	/* Use generic version. */
	cpu_features = (cpu_feature_set) 0;

	if (argc > 1) {
		/* Use optimized version, if available. */
		cpu_features = cpu_feature_set_from_string (argv[1]);
	}

	for (i = 0; i < 33; ++i) {
		test1 (dbuffer, sbuffer, i);
		test1 (dbuffer, sbuffer, i + 16384);
		test1 (dbuffer_end - i, sbuffer_end - i, i);
		test1 (dbuffer_end - i - 16384,
		       sbuffer_end - i - 16384, i + 16384);
		test1 (dbuffer + i, sbuffer + (i ^ 31), i * 3);
	}

	return EXIT_SUCCESS;
}
