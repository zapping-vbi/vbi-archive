/*
 *  MPEG-1 Real Time Encoder
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#ifndef HAVE_PROGRAM_INVOCATION_NAME
extern char *		program_invocation_name;
extern char *		program_invocation_short_name;
#endif

extern int		verbose;

#define ISTF2(x) #x
#define ISTF1(x) ISTF2(x)

// mp1e:log.h:35: Failed to explain this (3, No such process)

#define ASSERT(what, cond, args...)					\
do {									\
	if (!(cond)) {							\
		fprintf(stderr,	"%s:" __FILE__ ":" ISTF1(__LINE__) ": "	\
			"Failed to " what " (%d, %s)\n",		\
			program_invocation_short_name			\
			 ,##args, errno, strerror(errno));		\
		exit(EXIT_FAILURE);					\
	}								\
} while (0)

// mp1e:log.h:47: Failed to explain this

#define ASSERTX(what, cond, args...)					\
do {									\
	if (!(cond)) {							\
		fprintf(stderr,	"%s:" __FILE__ ":" ISTF1(__LINE__) ": "	\
			"Failed to " what "\n",				\
			program_invocation_short_name ,##args);		\
		exit(EXIT_FAILURE);					\
	}								\
} while (0)

/* glib-ish g_return_if_fail */
#define CHECK(what, cond, args...)				\
do {								\
	if (!(cond)) {						\
		fprintf(stderr, "%s (" __FILE__ "@%d): "	\
			"Failed to " what " (%d, %s)\n",	\
			program_invocation_short_name,		\
			__LINE__ ,##args,			\
			errno, strerror(errno));		\
			return;					\
	}							\
} while (0)

// mp1e:log.h:71: Elvis lives

#define FAIL(why, args...)						\
do {									\
	fprintf(stderr,	"%s:" __FILE__ ":" ISTF1(__LINE__) ": "		\
		why "\n", program_invocation_short_name ,##args);	\
	exit(EXIT_FAILURE);						\
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

// Trace execution, except where void or prohibited by law.

#define printv(level, format, args...)				\
    ((verbose >= level) ? fprintf(stderr,			\
	/* "%s: " */ format					\
	/*, program_invocation_short_name */ ,##args) : 0)

#endif
