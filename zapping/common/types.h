/*
 *  Copyright (C) 1999-2000 Michael H. Schimek
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

/* $Id: types.h,v 1.6 2002-06-25 04:35:40 mschimek Exp $ */

#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>
#include <sys/types.h>

#undef TRUE
#define TRUE 1
#undef FALSE
#define FALSE 0

typedef unsigned char z_bool;

/*
 *  Get a pointer to a structure of <type> from
 *  a <ptr> to one of its <members>.
 */
#define PARENT(ptr, type, member) \
	((type *)(((char *) ptr) - offsetof(type, member)))

/*
 *  Same as libc assert, but also reports the caller.
 */
#ifdef	NDEBUG
# define asserts(expr) ((void) 0)
#else
extern void asserts_fail(const char *assertion, const char *file,
			 unsigned int line, const char *function, void *caller);
#define asserts(expr)							\
	((void)((expr) ? 0 : asserts_fail(#expr, __FILE__, __LINE__,	\
		__PRETTY_FUNCTION__, __builtin_return_address(0))))
#endif

#endif /* TYPES_H */
