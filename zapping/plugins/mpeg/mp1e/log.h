/*
 *  MPEG-1 Real Time Encoder
 *  Nagging and babbling tools
 *
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

#ifndef LOG_H
#define LOG_H
 
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
 
extern char *		my_name;
extern int		verbose;

#define ASSERT(what, cond, args...)				\
do {								\
	if (!(cond)) {						\
		fprintf(stderr, "%s (" __FILE__ "@%d): "	\
			"Failed to " what " (%d, %s)\n",	\
			my_name, __LINE__ ,##args,		\
			errno, strerror(errno));		\
			exit(EXIT_FAILURE);			\
	}							\
} while (0)

#define ASSERTX(what, cond, args...)				\
do {								\
	if (!(cond)) {						\
		fprintf(stderr, "%s (" __FILE__ "@%d): "	\
			"Failed to " what "\n",			\
			my_name, __LINE__ ,##args);		\
			exit(EXIT_FAILURE);			\
	}							\
} while (0)

#define FAIL(why, args...)					\
do {								\
	fprintf(stderr, "%s (" __FILE__ "@%d): " why " \n",	\
		my_name, __LINE__ ,##args);			\
	exit(EXIT_FAILURE);					\
} while (0)

#define DUMP(array, from, to)					\
do {								\
	int i;							\
								\
	fprintf(stderr, __FILE__ "@%d:\n", __LINE__);		\
	for (i = (from); i < (to); i++)				\
		fprintf(stderr, #array "[%d]=%f\n",		\
			i, (double)((array)[i]));		\
} while (0)

#define printv(level, format, args...)				\
    ((verbose >= level) ? fprintf(stderr,			\
	/* "%s: " */ format /*, my_name */ ,##args) : 0)

#endif
