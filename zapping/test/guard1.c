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

/* $Id: guard1.c,v 1.2 2007-08-30 14:14:37 mschimek Exp $ */

#include "guard.h"

int
main				(int			argc,
				 char **		argv)
{
	char *buffer;

	(void) argc;
	(void) argv;
	
	buffer = guard_alloc (1 << 20);
	++buffer[-1]; /* should cause a segfault */

	return EXIT_SUCCESS;
}
