/*
 *  Copyright (C) 2004 Michael H. Schimek
 *
 *  Based on efence (C) Bruce Perens
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

/* $Id: guard.h,v 1.2 2005-02-15 17:26:08 mschimek Exp $ */

#undef NDEBUG

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <assert.h>

/* 386 BSD has MAP_ANON instead of MAP_ANONYMOUS. */
#if ( !defined(MAP_ANONYMOUS) && defined(MAP_ANON) )
#  define MAP_ANONYMOUS MAP_ANON
#endif

static int
dev_zero			(void)
{
	static int fd = -1;

	if (-1 != fd)
		return fd;
		
	fd = open ("/dev/zero", O_RDWR);
	if (-1 == fd) {
		fprintf (stderr, "Could not open /dev/zero: %d, %s\n",
			 errno, strerror (errno));
		exit (EXIT_FAILURE);
	}

	return fd;
}

/* Allocates a block of memory surrounded by an inaccessible guard area.
   n_bytes must be page aligned. */
static void *
guard_alloc			(size_t			n_bytes)
{
	size_t page_size;
	char *p;
	int r;

	page_size = getpagesize ();
	assert (0 == n_bytes % page_size);

#ifdef MAP_ANONYMOUS
	p = mmap (/* start: any */ NULL,
		  n_bytes * 3,
		  PROT_READ | PROT_WRITE,
		  MAP_PRIVATE | MAP_ANONYMOUS,
		  /* fd: n/a */ -1,
		  /* offset */ 0);
#else
	p = mmap (/* start: any */ NULL,
		  n_bytes * 3,
		  PROT_READ | PROT_WRITE,
		  MAP_PRIVATE,
		  dev_zero (),
		  /* offset */ 0);
#endif

	if (MAP_FAILED == p) {
		fprintf (stderr, "Guarded memory allocation failed: %d, %s\n",
			 errno, strerror (errno));
		exit (EXIT_FAILURE);
	}

	r = mprotect (p, n_bytes, PROT_NONE);
	assert (-1 != r);

	p += n_bytes;
	assert (0 == (unsigned long) p % 16);

	r = mprotect (p + n_bytes, n_bytes, PROT_NONE);
	assert (-1 != r);

	return p;
}
