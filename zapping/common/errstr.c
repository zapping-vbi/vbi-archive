/*
 *  Copyright (C) 2001 Michael H. Schimek
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

/* $Id: errstr.c,v 1.1 2001-08-20 00:53:23 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdarg.h>
#include <assert.h>

#include "errstr.h"

static pthread_key_t errstr_key;

struct errstr_rec {
	char *			message;
	void			(* free)(void *);
};

/**
 * reset_errstr:
 * 
 * Reset the thread specific errstr object, freeing a stored
 * error message as desired. Subsequent calls to get_errstr
 * will return %NULL. All newly created threads have a reset
 * errstr.
 **/
void
reset_errstr(void)
{
	struct errstr_rec *e;

	if ((e = pthread_getspecific(errstr_key))) {
		if (e->message && e->free)
			e->free(e->message);
		free(e);
		pthread_setspecific(errstr_key, NULL);
	}
}

/**
 * set_errstr:
 * @message: Error message, any zero terminated string.
 *   Can be %NULL, which has the same effect as calling reset_errstr().
 * @free_func: Pointer to a function to free the error message
 *   (eg. 'free'), can be %NULL for a static error message.
 * 
 * Set the thread specific errstr to @message, freeing any previously
 * stored message. When this function fails, subsequent calls to
 * get_errstr will return %NULL. The error message is freed when
 * calling reset_errstr(), set_errstr() and at thread termination.
 **/
void
set_errstr(char *message, void (*free_func)(void *))
{
	struct errstr_rec *e;

	if ((e = pthread_getspecific(errstr_key))) {
		if (e->message && e->free)
			e->free(e->message);
	} else if (!(e = malloc(sizeof(*e))))
		return;

	e->message = message;
	e->free = free_func;

	if (pthread_setspecific(errstr_key, e) != 0) {
		if (e->message && e->free)
			e->free(e->message);
		free(e);
	}
}

void
set_errstr_printf(char *templ, ...)
{
	va_list ap;
	char *msg;

	va_start(ap, templ);
	vasprintf(&msg, templ, ap);
	va_end(ap);

	set_errstr(msg, free);
}

/**
 * get_errstr:
 *
 * Reads the thread specific errstr object, this is equivalent
 * to reading the 'errstr' pseudo variable (char *). Note only
 * set_errstr() can write 'errstr'.
 * 
 * Return value:
 * Error message (char *) or %NULL if not set.
 **/
char *
get_errstr(void)
{
	struct errstr_rec *e;

	if ((e = pthread_getspecific(errstr_key)))
		return e->message;

	return NULL;
}

static void
uninit_errstr(void *p)
{
	struct errstr_rec *e = p;

	if (e) {
		if (e->message && e->free)
			e->free(e->message);
		free(e);
	}
}

static void init_errstr(void) __attribute__ ((constructor));

static void
init_errstr(void)
{
	assert(pthread_key_create(&errstr_key, uninit_errstr) == 0);
}
