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

/* $Id: types.h,v 1.1 2000-12-11 04:12:53 mschimek Exp $ */

#ifndef TYPES_H
#define TYPES_H

#include <stddef.h>

enum { FALSE, TRUE };

typedef unsigned char bool;

#define PARENT(ptr, type, member) \
	((type *)(((char *) ptr) - offsetof(type, member)))

#endif /* TYPES_H */
