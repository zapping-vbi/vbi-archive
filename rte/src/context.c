/*
 *  Real Time Encoder
 *
 *  Copyright (C) 2000, 2001 Iñaki García Etxebarria
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
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

/* $Id: context.c,v 1.1 2002-02-08 15:03:11 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "rtepriv.h"

#define xc context->class

extern const rte_backend_class rte_backend_mp1e;
extern const rte_backend_class rte_backend_ffmpeg;

static const rte_backend_class *
backends[] = {
#ifdef MP1E
	&rte_backend_mp1e,
#endif
#ifdef FFMPEG
	&rte_backend_ffmpeg,
#endif
	/* more */
};

static const int num_backends = sizeof(backends) / sizeof(backends[0]);

/* XXX this function has no reentrance protection */
static void
init_backends(void)
{
	static rte_bool inited = FALSE;

	if (!inited) {
		int i;

		for (i = 0; i < num_backends; i++)
			if (backends[i]->backend_init)
				backends[i]->backend_init();

		inited = TRUE;
	}
}

/**
 * rte_context_info_enum:
 * @index: Index into the context format table.
 *
 * Enumerates available backends / file formats / multiplexers. You
 * should start at index 0, incrementing.
 *
 * Some codecs may depend on machine features such as SIMD instructions
 * or the presence of certain libraries, thus the list can vary from
 * session to session.
 *
 * Return value:
 * Static pointer, data not to be freed, to a #rte_context_info
 * structure. %NULL if the index is out of bounds.
 **/
rte_context_info *
rte_context_info_enum(int index)
{
	rte_context_class *rxc;
	int i, j;

	init_backends();

	for (j = 0; j < num_backends; j++)
		if (backends[j]->context_enum)
			for (i = 0; (rxc = backends[j]->context_enum(i, NULL)); i++)
				if (rxc->new)
					if (index-- == 0)
						return &rxc->public;
	return NULL;
}

/**
 * rte_context_info_keyword:
 * @keyword: Context format identifier as in #rte_context_info and
 *           rte_context_new().
 * 
 * Similar to rte_context_info_enum(), but this function attempts
 * to find a context info by keyword.
 * 
 * Return value:
 * Static pointer to a #rte_context_info structure, %NULL if the named
 * context format has not been found.
 **/
rte_context_info *
rte_context_info_keyword(const char *keyword)
{
	rte_context_class *rxc;
	int i, j;

	init_backends();

	if (!keyword)
		return NULL;

	for (j = 0; j < num_backends; j++)
		if (backends[j]->context_enum)
			for (i = 0; (rxc = backends[j]->context_enum(i, NULL)); i++)
				if (rxc->new)
					if (strcmp(keyword, rxc->public.keyword) == 0)
						return &rxc->public;
	return NULL;
}

/**
 * rte_context_info_context:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * 
 * Returns the context info for the given @context.
 * 
 * Return value:
 * Static pointer to a #rte_context_info structure, %NULL if the
 * @context is %NULL.
 **/
rte_context_info *
rte_context_info_context(rte_context *context)
{
	nullcheck(context, return NULL);

	return &xc->public;
}

/**
 * rte_context_new:
 * @keyword: Context format identifier as in #rte_context_info.
 * @errstr: If non-zero and the function fails, a pointer to
 *   an error description will be stored here. You must free()
 *   this string when no longer needed.
 * @user_data: Pointer stored in the context, can be retrieved
 *   with rte_context_user_data().
 * 
 * Creates a new rte context, encoding files in the specified format.
 * As a special service you can initialize <emphasis>context</>
 * options by appending to the @keyword like this:
 * 
 * <informalexample><programlisting>
 * rte_context_new("keyword; quality=75.5, comment=\"example\"", NULL);  
 * </programlisting></informalexample>
 * 
 * Return value:
 * Pointer to a newly allocated #rte_context structure, which must be
 * freed by calling rte_context_delete(). %NULL is returned when the
 * named context format is unavailable, the option string or option
 * values are incorrect or some other error occurred. See also
 * rte_errstr().
 **/
rte_context *
rte_context_new(const char *keyword, char **errstr, void *user_data)
{
	char key[256], *error;
	rte_context_class *rxc = NULL;
	rte_context *context = NULL;
	int keylen, i, j;

	if (errstr)
		*errstr = NULL;

	if (!keyword) {
		rte_asprintf(errstr, _("No format keyword\n"));
		nullcheck(keyword, return NULL);
	}

	init_backends();

	for (keylen = 0; keyword[keylen] && keylen < (sizeof(key) - 1)
	     && keyword[keylen] != ';' && keyword[keylen] != ','; keylen++)
	     key[keylen] = keyword[keylen];
	key[keylen] = 0;

	error = NULL;

	for (j = 0; j < num_backends; j++)
		if (backends[j]->context_enum)
			for (i = 0; (rxc = backends[j]->context_enum(i, &error)); i++) {
				if (strcmp(key, rxc->public.keyword) == 0)
					break;

				if (error) {
					free(error);
					error = NULL;
				}
			}

	if (!rxc) {
		rte_asprintf(errstr, _("No such encoder: '%s'."), key);
		assert(error == NULL);
		return NULL;
	} else if (!rxc->new || error) {
		if (errstr) {
			if (error)
				rte_asprintf(errstr, _("Encoder '%s' not available: %s"),
					     rxc->public.label ? _(rxc->public.label) : key, error);
			else
				rte_asprintf(errstr, _("Encoder '%s' not available."),
					     rxc->public.label ? _(rxc->public.label) : key);
		}

		if (error)
			free(error);

		return NULL;
	}

	context = rxc->new(rxc, &error);

	if (!context) {
		if (error) {
			rte_asprintf(errstr, _("Cannot create new encoding context '%s': %s"),
				     rxc->public.label ? _(rxc->public.label) : key, error);
			free(error);
		} else {
			rte_asprintf(errstr, _("Cannot create new encoding context '%s'."),
				     rxc->public.label ? _(rxc->public.label) : key);
		}

		return NULL;
	}

	assert(error == NULL);

	context->user_data = user_data;

	if (rte_context_options_reset(context))
		if (!keyword[keylen] || rte_option_string(context, NULL, keyword + keylen + 1))
			return context;

	if (context->error && errstr) {
		*errstr = context->error;
		context->error = NULL;
	}

	xc->delete(context);

	return NULL;
}

/*
 *  Removed rte_context_set_user_data because when we set only
 *  at rte_context_new() we can save context->mutex locking on
 *  every access.
 */

/**
 * rte_context_user_data:
 * @context: Initialized #rte_context as returned by rte_context_new().
 *
 * Retrieves the pointer stored in the user data field of the context.
 *
 * Return value:
 * Pointer.
 **/
void *
rte_context_user_data(rte_context *context)
{
	nullcheck(context, return NULL);

	return context->user_data;
}

/**
 * rte_context_delete:
 * @context: Initialized #rte_context previously allocated
 *   with rte_context_new().
 *
 * This function stops encoding if active, then frees all resources
 * associated with the context.
 **/
void
rte_context_delete(rte_context *context)
{
	if (context == NULL)
		return;

	if (context->status == RTE_STATUS_RUNNING)
		rte_stop(context);
	else if (context->status == RTE_STATUS_PAUSED)
		/* FIXME */;

	if (context->error) {
		free(context->error);
		context->error = NULL;
	}

	xc->delete(context);
}

/*
 *  Options
 */

/**
 * rte_context_option_info_enum:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @index: Index into the option table.
 * 
 * Enumerates the options available of the given context.
 * You should start at index 0, incrementing by one.
 *
 * Return value:
 * Static pointer, data not to be freed, to a #rte_option_info
 * structure. %NULL if the @index is out of bounds.
 **/
rte_option_info *
rte_context_option_info_enum(rte_context *context, int index)
{
	nullcheck(context, return NULL);
	rte_error_reset(context);

	if (!xc->context_option_enum)
		return NULL;

	return xc->context_option_enum(context, index);
}

/**
 * rte_context_option_info_keyword:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @keyword: Keyword identifying the option as in #rte_option_info.
 * 
 * Similar to rte_context_option_info_enum() but this function tries
 * to find the option info by keyword.
 * 
 * Return value:
 * Static pointer to a #rte_option_info structure, %NULL if
 * the keyword was not found.
 **/
rte_option_info *
rte_context_option_info_keyword(rte_context *context, const char *keyword)
{
	rte_option_info *roi;
	int i;

	nullcheck(context, return NULL);
	rte_error_reset(context);

	if (!xc->context_option_enum)
		return NULL;

	for (i = 0;; i++)
	        if (!(roi = xc->context_option_enum(context, i))
		    || strcmp(keyword, roi->keyword) == 0)
			break;
	return roi;
}

/**
 * rte_context_option_get:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @keyword: Keyword identifying the option as in #rte_option_info.
 * @value: A place to store the option value.
 * 
 * This function queries the current value of the option. When the
 * option is a string, you must free() @value.str when no longer
 * needed.
 *
 * Return value:
 * %TRUE on success, otherwise @value remained unchanged.
 **/
rte_bool
rte_context_option_get(rte_context *context, const char *keyword,
		       rte_option_value *value)
{
	nullcheck(context, return FALSE);
	rte_error_reset(context);
	rte_bool r;

	nullcheck(value, return FALSE);

	if (!xc->context_option_get || !keyword) {
		rte_unknown_option(context, NULL, keyword);
		return FALSE;
	}

	pthread_mutex_lock(&context->mutex);

	r = xc->context_option_get(context, keyword, value);

	pthread_mutex_unlock(&context->mutex);

	return r;
}

/**
 * rte_context_option_set:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @keyword: Keyword identifying the option as in #rte_option_info.
 * @Varargs: New value to set.
 *
 * Sets the value of the option. Make sure you are casting the
 * value to the correct type (int, double, char *).
 *
 * Typical usage is:
 * <informalexample><programlisting>
 * rte_context_option_set(context, "frame_rate", (double) 3.141592);
 * </programlisting></informalexample>
 *
 * Return value:
 * %TRUE on success.
 **/
rte_bool
rte_context_option_set(rte_context *context, const char *keyword, ...)
{
	va_list args;
	rte_bool r;

	nullcheck(context, return FALSE);
	rte_error_reset(context);

	if (!xc->context_option_set || !keyword) {
		rte_unknown_option(context, NULL, keyword);
		return FALSE;
	}

	va_start(args, keyword);

	pthread_mutex_lock(&context->mutex);

	r = xc->context_option_set(context, keyword, args);

	pthread_mutex_unlock(&context->mutex);

	va_end(args);

	return r;
}

/**
 * rte_context_option_print:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @keyword: Keyword identifying the option as in #rte_option_info.
 * @Varargs: Option value.
 *
 * Return a string representation of the option value. When for example
 * the option is a memory size, a value of 2048 may result in a string
 * "2 KB". Make sure you are casting the value to the correct type
 * (int, double, char *). You must free() the returned string when
 * no longer needed.
 *
 * Return value:
 * String pointer or %NULL on failure.
 **/
char *
rte_context_option_print(rte_context *context, const char *keyword, ...)
{
	va_list args;
	char *r;

	nullcheck(context, return NULL);
	rte_error_reset(context);

	if (!xc->context_option_print || !keyword) {
		rte_unknown_option(context, NULL, keyword);
		return NULL;
	}

	va_start(args, keyword);

	r = xc->context_option_print(context, keyword, args);

	va_end(args);

	return r;
}

/**
 * rte_context_option_menu_get:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @keyword: Keyword identifying the option as in #rte_option_info.
 * @entry: A place to store the current menu entry.
 * 
 * Similar to rte_context_option_get() this function queries the current
 * value of the named option, but returns this value as number of the
 * corresponding menu entry. Naturally this must be an option with
 * menu or the function will fail.
 * 
 * Return value: 
 * %TRUE on success, otherwise @value remained unchanged.
 **/
rte_bool
rte_context_option_menu_get(rte_context *context, const char *keyword, int *entry)
{
	rte_option_info *oi;
	rte_option_value val;
	rte_bool r;
	int i;

	nullcheck(context, return FALSE);
	rte_error_reset(context);

	nullcheck(entry, return FALSE);

	if (!(oi = rte_context_option_info_keyword(context, keyword)))
		return FALSE;

	if (!rte_context_option_get(context, keyword, &val))
		return FALSE;

	r = FALSE;

	for (i = oi->min.num; i <= oi->max.num; i++) {
		switch (oi->type) {
		case RTE_OPTION_BOOL:
		case RTE_OPTION_INT:
			if (!oi->menu.num)
				return FALSE;
			r = (oi->menu.num[i] == val.num);
			break;

		case RTE_OPTION_REAL:
			if (!oi->menu.dbl)
				return FALSE;
			r = (oi->menu.dbl[i] == val.dbl);
			break;

		case RTE_OPTION_MENU:
			r = (i == val.num);
			break;

		default:
			fprintf(stderr, __PRETTY_FUNCTION__
				": unknown export option type %d\n", oi->type);
			exit(EXIT_FAILURE);
		}

		if (r) {
			*entry = i;
			break;
		}
	}

	return r;
}

/**
 * rte_context_option_menu_set:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @keyword: Keyword identifying the option as in #rte_option_info.
 * @entry: Menu entry to be selected.
 * 
 * Similar to rte_context_option_set() this function sets the value of
 * the named option, however it does so by number of the corresponding
 * menu entry. Naturally this must be an option with menu, or the
 * function will fail.
 * 
 * Return value: 
 * %TRUE on success, otherwise the option is not changed.
 **/
rte_bool
rte_context_option_menu_set(rte_context *context, const char *keyword, int entry)
{
	rte_option_info *oi;

	nullcheck(context, return FALSE);

	if (!(oi = rte_context_option_info_keyword(context, keyword)))
		return FALSE;

	if (entry < oi->min.num || entry > oi->max.num)
		return FALSE;

	switch (oi->type) {
	case RTE_OPTION_BOOL:
	case RTE_OPTION_INT:
		if (!oi->menu.num)
			return FALSE;
		return rte_context_option_set(context,
			keyword, oi->menu.num[entry]);

	case RTE_OPTION_REAL:
		if (!oi->menu.dbl)
			return FALSE;
		return rte_context_option_set(context,
			keyword, oi->menu.dbl[entry]);

	case RTE_OPTION_MENU:
		return rte_context_option_set(context, keyword, entry);

	default:
		fprintf(stderr, __PRETTY_FUNCTION__
			": unknown export option type %d\n", oi->type);
		exit(EXIT_FAILURE);
	}
}

/**
 * rte_context_options_reset:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * 
 * Resets all options of the context to their respective default, that
 * is the value they have after calling rte_context_new().
 * 
 * Return value: 
 * %TRUE on success, otherwise some options may be reset and some not.
 **/
rte_bool
rte_context_options_reset(rte_context *context)
{
	rte_option_info *oi;
	rte_bool r = TRUE;
	int i;

	nullcheck(context, return FALSE);

	pthread_mutex_lock(&context->mutex);

	for (i = 0; r && (oi = rte_context_option_info_enum(context, i)); i++) {
		switch (oi->type) {
		case RTE_OPTION_BOOL:
		case RTE_OPTION_INT:
			if (oi->menu.num)
				r = rte_context_option_set(context,
			        	oi->keyword, oi->menu.num[oi->def.num]);
			else
				r = rte_context_option_set(context,
					oi->keyword, oi->def.num);
			break;

		case RTE_OPTION_REAL:
			if (oi->menu.dbl)
				r = rte_context_option_set(context,
					oi->keyword, oi->menu.dbl[oi->def.num]);
			else
				r = rte_context_option_set(context,
					oi->keyword, oi->def.dbl);
			break;

		case RTE_OPTION_STRING:
			if (oi->menu.str)
				r = rte_context_option_set(context,
					oi->keyword, oi->menu.str[oi->def.num]);
			else
				r = rte_context_option_set(context,
					oi->keyword, oi->def.str);
			break;

		case RTE_OPTION_MENU:
			r = rte_context_option_set(context,
					oi->keyword, oi->def.num);
			break;

		default:
			fprintf(stderr, __PRETTY_FUNCTION__
				": unknown context option type %d\n", oi->type);
			exit(EXIT_FAILURE);
		}
	}

	pthread_mutex_unlock(&context->mutex);

	return r;
}

/*
 *  Status (keep? change?)
 */

/**
 * rte_context_status_enum:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @n: Status token index.
 *
 * Enumerates available status statistics for the context, starting
 * from 0.
 *
 * Return value: A rte_status_info object that you should free with
 * rte_status_free(), or %NULL if the index is out of bounds.
 **/
rte_status_info *
rte_context_status_enum(rte_context *context, int n)
{
	nullcheck(context, return NULL);
	rte_error_reset(context);

	if (!xc->status_enum)
		return NULL;

	return xc->status_enum(context, n);
}

/**
 * rte_context_status_keyword:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @keyword: Status token keyword.
 *
 * Tries to find the status token by keyword.
 *
 * Return value: A rte_status_info object that you should free with
 * rte_status_free(), or %NULL if @keyword couldn't be found.
 **/
rte_status_info *
rte_context_status_keyword(rte_context *context, const char *keyword)
{
	rte_status_info *si;
	int i;

	nullcheck(context, return NULL);
	rte_error_reset(context);

	nullcheck(keyword, return NULL);

	if (!xc->status_enum)
		return NULL;

	for (i = 0;; i++)
	        if (!(si = xc->status_enum(context, i))
		    || strcmp(keyword, si->keyword) == 0)
			break;

	return si;
}
