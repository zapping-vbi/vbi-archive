/*
 *  Real Time Encoding Library
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

/* $Id: context.c,v 1.12 2002-12-14 00:48:50 mschimek Exp $ */

#include "config.h"

#ifdef HAVE_LARGEFILE64
#define _LARGEFILE64_SOURCE
#endif

#include "rtepriv.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>

#define xc context->_class

extern const rte_backend_class rte_backend_mp1e;
extern const rte_backend_class rte_backend_ffmpeg;
extern const rte_backend_class rte_backend_divx4linux;

static const rte_backend_class *
backends[] = {
#ifdef BACKEND_MP1E
	&rte_backend_mp1e,
#endif
#ifdef BACKEND_FFMPEG
	&rte_backend_ffmpeg,
#endif
#ifdef BACKEND_DIVX4LINUX
	&rte_backend_divx4linux,
#endif
	/* more */
};

static const unsigned int num_backends =
	sizeof(backends) / sizeof(backends[0]);

static pthread_once_t init_once = PTHREAD_ONCE_INIT;

static void
init_backends			(void)
{
	unsigned int i;

#ifdef ENABLE_NLS
	bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
#endif
	for (i = 0; i < num_backends; i++)
		if (backends[i]->backend_init)
			backends[i]->backend_init();
}

/**
 * @param index Index into the context format table.
 *
 * Enumerates available contexts (backends/file formats/multiplexers). You
 * should start at index 0, incrementing.
 *
 * Some codecs may depend on machine features such as SIMD instructions
 * or the presence of certain libraries, thus the list can vary from
 * session to session.
 *
 * @return
 * Static pointer, data not to be freed, to a rte_context_info
 * structure. @c NULL if the index is out of bounds.
 */
rte_context_info *
rte_context_info_enum(unsigned int index)
{
	rte_context_class *rxc;
	unsigned int i, j;

	pthread_once (&init_once, init_backends);

	for (j = 0; j < num_backends; j++)
		if (backends[j]->context_enum)
			for (i = 0; (rxc = backends[j]->context_enum(i, NULL)); i++)
				if (rxc->_new)
					if (index-- == 0)
						return rxc->_public;
	return NULL;
}

/**
 * @param keyword Context format identifier as in rte_context_info and
 *           rte_context_new().
 * 
 * Similar to rte_context_info_enum(), but this function attempts
 * to find a context info by keyword.
 * 
 * @return
 * Static pointer to a rte_context_info structure, @c NULL if the named
 * context format has not been found.
 */
rte_context_info *
rte_context_info_by_keyword(const char *keyword)
{
	rte_context_class *rxc;
	unsigned int i, j, keylen;

	pthread_once (&init_once, init_backends);

	if (!keyword)
		return NULL;

	for (keylen = 0; keyword[keylen]; keylen++)
		if (keyword[keylen] == ';' || keyword[keylen] == ',')
			break;

	for (j = 0; j < num_backends; j++)
		if (backends[j]->context_enum)
			for (i = 0; (rxc = backends[j]->context_enum(i, NULL)); i++)
				if (rxc->_new)
					if (strncmp(keyword, rxc->_public->keyword, keylen) == 0)
						return rxc->_public;
	return NULL;
}

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * 
 * Returns the context info for the given @a context.
 * 
 * @return
 * Static pointer to a rte_context_info structure, @c NULL if the
 * @a context is @c NULL.
 */
rte_context_info *
rte_context_info_by_context(rte_context *context)
{
	nullcheck(context, return NULL);

	return xc->_public;
}

/**
 * @param keyword Context format identifier as in rte_context_info.
 * @param user_data Pointer stored in the context, can be retrieved
 *   with rte_context_user_data().
 * @param errstr If non-zero and the function fails, a pointer to
 *   an error description will be stored here. You must free()
 *   this string when no longer needed.
 * 
 * Creates a new rte context encoding files in the format specified
 * by @a keyword. As a special service you can initialize <em>context</em>
 * options by appending to the @a keyword like this:
 * 
 * @code
 * rte_context_new ("keyword; quality=75.5, comment=\"example\"", NULL, NULL);  
 * @endcode
 * 
 * RTE is thread aware: Multiple threads can allocate contexts but
 * sharing the same context between threads is not safe unless you
 * implement your own mutual exclusion mechanism. <!-- Removed
 * because once started this locking business gets out of hand,
 * while a thread safe api is rather useless for most apps.
 * Better an exception to this rule where it really matters. -->
 *
 * @return
 * Pointer to a newly allocated rte_context structure, which must be
 * freed by calling rte_context_delete(). @c NULL is returned when the
 * named context format is unavailable, the option string or option
 * values are incorrect or some other error occurred. See also
 * rte_errstr().
 */
rte_context *
rte_context_new(const char *keyword, void *user_data, char **errstr)
{
	char key[256], *error;
	rte_context_class *rxc = NULL;
	rte_context *context = NULL;
	unsigned int keylen, i, j;

	if (errstr)
		*errstr = NULL;

	if (!keyword) {
		rte_asprintf(errstr, "No format keyword\n");
		nullcheck(keyword, return NULL);
	}

	pthread_once (&init_once, init_backends);

	for (keylen = 0; keyword[keylen] && keylen < (sizeof(key) - 1)
	     && keyword[keylen] != ';' && keyword[keylen] != ',';
	     keylen++)
		key[keylen] = keyword[keylen];
	key[keylen] = 0;

	error = NULL;

	for (j = 0; j < num_backends; j++)
		if (backends[j]->context_enum)
			for (i = 0; (rxc = backends[j]->context_enum(i, &error)); i++) {
				if (strcmp(key, rxc->_public->keyword) == 0) {
					j = num_backends + 1;
					break;
				}

				if (error) {
					free(error);
					error = NULL;
				}
			}

	if (!rxc) {
		rte_asprintf(errstr, _("No such encoder '%s'."), key);
		assert(error == NULL);
		return NULL;
	} else if (!rxc->_new || error) {
		if (errstr) {
			if (error)
				rte_asprintf(errstr, _("Encoder '%s' not available. %s"),
					     rxc->_public->label ? _(rxc->_public->label) : key, error);
			else
				rte_asprintf(errstr, _("Encoder '%s' not available."),
					     rxc->_public->label ? _(rxc->_public->label) : key);
		}

		if (error)
			free(error);

		return NULL;
	}

	context = rxc->_new(rxc, &error);

	if (!context) {
		if (error) {
			rte_asprintf(errstr, _("Cannot create new encoding context '%s'. %s"),
				     rxc->_public->label ? _(rxc->_public->label) : key, error);
			free(error);
		} else {
			rte_asprintf(errstr, _("Cannot create new encoding context '%s'."),
				     rxc->_public->label ? _(rxc->_public->label) : key);
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

	xc->_delete(context);

	return NULL;
}

/*
 *  Removed rte_context_set_user_data because when we set only
 *  once at rte_context_new() we can save context->mutex locking
 *  on every access.
 */

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 *
 * Retrieves the pointer stored in the user data field of the context.
 *
 * @return
 * User pointer.
 */
void *
rte_context_user_data(rte_context *context)
{
	nullcheck(context, return NULL);

	return context->user_data;
}

/**
 * @param context Initialized rte_context previously allocated
 *   with rte_context_new().
 *
 * This function stops encoding if active, then frees all resources
 * associated with the context.
 */
void
rte_context_delete(rte_context *context)
{
	if (context == NULL)
		return;

	switch (context->state) {
	case RTE_STATE_RUNNING:
	case RTE_STATE_PAUSED:
		rte_stop(context, 0.0 /* immediate */);
		break;

	default:
		break;
	}

	if (context->error) {
		free(context->error);
		context->error = NULL;
	}

	xc->_delete(context);
}

/*
 *  Codec assignment
 */

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param keyword Codec identifier as in rte_codec_info.
 * @param stream_index Elementary stream number.
 * @param user_data Pointer stored in the codec, can be retrieved
 *   with rte_codec_user_data().
 * 
 * Allocates a codec instance and assigns it to encode some track of the
 * @a context, the audio, video, ... (depending on the codec type)
 * elementary stream number @a stream_index.
 *
 * The stream number refers for example to one of the 16 video or 32 audio
 * streams in a MPEG-1 program stream. The required and permitted number of
 * elementary streams of each type is listed in rte_context_info.
 * Naturally a context needs at least one elementary stream.
 *
 * The first and default stream has index number 0. When you already selected
 * a codec for this stream type and index it will be replaced.
 * All properties of the new codec instance are reset to their defaults.
 *
 * Possible mp1e backend initialization (error checks omitted):
 * @code
 * context = rte_context_new ("mp1e_mpeg1_ps", NULL, NULL);  // MPEG-1 Program stream
 *
 * rte_set_codec (context, "mp1e_mpeg1_video", 0, NULL);     // MPEG-1 Video (first elementary)
 * rte_set_codec (context, "mp1e_mpeg2_layer2", 0, NULL);    // MPEG-2 Audio (first elementary)
 * rte_set_codec (context, "mp1e_mpeg1_layer2", 1, NULL);    // MPEG-1 Audio (second elementary)
 * @endcode
 * 
 * As a special service you can set <em>codec</em> options
 * by appending to the keyword like this:
 * @code
 * rte_set_codec (context, 0, "mp1e_mpeg2_layer_2; bit_rate=128000, comment="example");
 * @endcode
 * 
 * @return 
 * Static pointer, data not to be freed, to an opaque rte_codec object.
 * On error @c NULL is returned, which may be caused by invalid parameters, an
 * unknown @a codec_keyword, a stream type of the codec not suitable for the
 * context or an invalid option string. See also rte_errstr(). 
 */
rte_codec *
rte_set_codec(rte_context *context, const char *keyword,
	      unsigned int stream_index, void *user_data)
{
	char key[256];
	rte_codec *codec;
	int keylen;

	nullcheck(context, return NULL);
	rte_error_reset(context);

	nullcheck(keyword, return NULL);

	for (keylen = 0; keyword[keylen] && keylen < (sizeof(key) - 1)
	     && keyword[keylen] != ';' && keyword[keylen] != ','; keylen++)
	     key[keylen] = keyword[keylen];
	key[keylen] = 0;

	assert(xc->codec_set != NULL);

	codec = xc->codec_set(context, key, 0, stream_index);

	if (codec) {
		codec->user_data = user_data;

		if (keyword[keylen] && !rte_option_string(
			context, codec, keyword + keylen + 1)) {
			xc->codec_set(context, NULL,
				      codec->_class->_public->stream_type,
				      codec->stream_index);
			codec = NULL;
		}
	}

	return codec;
}

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param stream_type RTE_STREAM_VIDEO, RTE_STREAM_AUDIO, ...
 * @param stream_index Elementary stream number.
 *
 * Returns a pointer to the rte_codec assigned with rte_set_codec()
 * to encode this elementary stream type and index.
 *
 * @return
 * Static pointer to an opaque rte_codec object, @c NULL if no
 * codec has been assigned or the codec has been removed with
 * rte_remove_codec() or rte_codec_delete().
 */
rte_codec *
rte_get_codec(rte_context *context,
	      rte_stream_type stream_type,
	      int stream_index)
{
	rte_codec *codec;

	nullcheck(context, return NULL);
	rte_error_reset(context);

	assert(xc->codec_get != NULL);

	codec = xc->codec_get(context, stream_type, stream_index);

	return codec;
}

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 * 
 * Removes the codec from the rte_context it has been assigned to and
 * deletes the codec.
 */
void
rte_codec_delete(rte_codec *codec)
{
	rte_context *context = NULL;

	nullcheck(codec, return);

	context = codec->context;
	rte_error_reset(context);

	assert(xc->codec_set != NULL);

	xc->codec_set(context, NULL,
		      codec->_class->_public->stream_type,
		      codec->stream_index);
}

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param stream_type RTE_STREAM_VIDEO, RTE_STREAM_AUDIO, ...
 * @param stream_index Elementary stream number.
 * 
 * Removes the codec previously assigned with rte_set_codec() to encode
 * the given elementary stream type and number.
 */
void
rte_remove_codec(rte_context *context,
		 rte_stream_type stream_type,
		 unsigned int stream_index)
{
	nullcheck(context, return);
	rte_error_reset(context);

	assert(xc->codec_set != NULL);

	xc->codec_set(context, NULL, stream_type, stream_index);
}

/*
 *  Options
 */

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param index Index into the option table.
 * 
 * Enumerates the options available of the given context.
 * You should start at index 0, incrementing by one.
 *
 * @return
 * Static pointer, data not to be freed, to a rte_option_info
 * structure. @c NULL if the @a index is out of bounds.
 */
rte_option_info *
rte_context_option_info_enum(rte_context *context, unsigned int index)
{
	nullcheck(context, return NULL);
	rte_error_reset(context);

	if (!xc->context_option_enum)
		return NULL;

	return xc->context_option_enum(context, index);
}

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param keyword Keyword identifying the option as in rte_option_info.
 * 
 * Similar to rte_context_option_info_enum() but this function tries
 * to find the option info by keyword.
 * 
 * @return
 * Static pointer to a rte_option_info structure, @c NULL if
 * the keyword was not found.
 */
rte_option_info *
rte_context_option_info_by_keyword(rte_context *context, const char *keyword)
{
	rte_option_info *oi;
	int i;

	nullcheck(context, return NULL);
	rte_error_reset(context);

	if (!xc->context_option_enum)
		return NULL;

	for (i = 0;; i++)
	        if (!(oi = xc->context_option_enum(context, i))
		    || strcmp(keyword, oi->keyword) == 0)
			break;
	return oi;
}

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param keyword Keyword identifying the option as in rte_option_info.
 * @param value A place to store the option value.
 * 
 * This function queries the current value of the option. When the
 * option is a string, you must free() @a value.str when no longer
 * needed.
 *
 * @return
 * @c TRUE on success, otherwise @a value remained unchanged.
 */
rte_bool
rte_context_option_get(rte_context *context, const char *keyword,
		       rte_option_value *value)
{
	rte_bool r;

	nullcheck(context, return FALSE);
	rte_error_reset(context);

	nullcheck(value, return FALSE);

	if (!xc->context_option_get || !keyword) {
		rte_unknown_option(context, NULL, keyword);
		return FALSE;
	}

	r = xc->context_option_get(context, keyword, value);

	return r;
}

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param keyword Keyword identifying the option as in rte_option_info.
 * @param Varargs New value to set.
 *
 * Sets the value of the option. Make sure you are casting the
 * value to the correct type (int, double, char *).
 *
 * Typical usage is:
 *
 * @code
 * rte_context_option_set (context, "frame_rate", (double) 3.141592);
 * @endcode
 *
 * @return
 * @c TRUE on success.
 */
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

	r = xc->context_option_set(context, keyword, args);

	va_end(args);

	return r;
}

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param keyword Keyword identifying the option as in rte_option_info.
 * @param Varargs Option value.
 *
 * Return a string representation of the option value. When for example
 * the option is a memory size, a value of 2048 may result in a string
 * "2 KB". Make sure you are casting the value to the correct type
 * (int, double, char *). You must free() the returned string when
 * no longer needed.
 *
 * @return
 * String pointer or @c NULL on failure.
 */
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
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param keyword Keyword identifying the option as in rte_option_info.
 * @param entry A place to store the current menu entry.
 * 
 * Similar to rte_context_option_get() this function queries the current
 * value of the named option, but returns this value as number of the
 * corresponding menu entry. Naturally this must be an option with
 * menu or the function will fail.
 * 
 * @return 
 * @c TRUE on success, otherwise @a value remained unchanged.
 */
rte_bool
rte_context_option_menu_get(rte_context *context, const char *keyword,
			    unsigned int *entry)
{
	rte_option_info *oi;
	rte_option_value val;
	rte_bool r;
	int i;

	nullcheck(context, return FALSE);
	rte_error_reset(context);

	nullcheck(entry, return FALSE);

	if (!(oi = rte_context_option_info_by_keyword(context, keyword)))
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
			fprintf(stderr,	"rte:%s: unknown export option type %d\n",
				__PRETTY_FUNCTION__, oi->type);
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
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param keyword Keyword identifying the option as in rte_option_info.
 * @param entry Menu entry to be selected.
 * 
 * Similar to rte_context_option_set() this function sets the value of
 * the named option, however it does so by number of the corresponding
 * menu entry. Naturally this must be an option with menu, or the
 * function will fail.
 * 
 * @return 
 * @c TRUE on success, otherwise the option is not changed.
 */
rte_bool
rte_context_option_menu_set(rte_context *context, const char *keyword,
			    unsigned int entry)
{
	rte_option_info *oi;

	nullcheck(context, return FALSE);

	if (!(oi = rte_context_option_info_by_keyword(context, keyword)))
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
		fprintf(stderr,	"rte:%s: unknown export option type %d\n",
			__PRETTY_FUNCTION__, oi->type);
		exit(EXIT_FAILURE);
	}
}

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * 
 * Resets all options of the context to their respective default, that
 * is the value they have after calling rte_context_new().
 * 
 * @return 
 * @c TRUE on success, on failure some options may be reset and some not.
 */
rte_bool
rte_context_options_reset(rte_context *context)
{
	rte_option_info *oi;
	rte_bool r = TRUE;
	int i;

	nullcheck(context, return FALSE);

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
			fprintf(stderr,	"rte:%s: unknown context option type %d\n",
				__PRETTY_FUNCTION__, oi->type);
			exit(EXIT_FAILURE);
		}
	}

	return r;
}

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param write_cb Function called by the context to write encoded data.
 *   The codec parameter of the callback is not used (@c NULL).
 * @param seek_cb Optional function called by the context to move the output
 *  file pointer, for example to complete a header.
 *
 * Sets the output mode for the context and makes the context ready
 * to start encoding. Using this method the codec allocates the necessary
 * buffers, when data is available for writing it calls @a write_cb with
 * buffer->data and buffer->size initialized.
 *
 * Typical usage of rte_set_output_callback_slave():
 * @code
 * rte_bool
 * my_write_cb (rte_context *context, rte_codec *codec, rte_buffer *buffer)
 * {
 *         ssize_t actual;
 * 
 *         if (!buffer) // EOF
 *                 return TRUE;
 * 
 *         do actual = write (STDOUT_FILENO, buffer->data, buffer->size);
 *         while (actual == -1 && errno == EINTR);
 * 
 *         return actual == buffer->size; // no error
 * }
 * 
 * rte_bool
 * my_seek_cb (rte_context *context, long long offset, int whence)
 * {
 *         return lseek64 (STDOUT_FILENO, offset, whence) != (off64_t) -1;
 * }
 * @endcode
 *
 * @return
 * Before selecting an output method you must select input methods for all
 * codecs, else this function fails with a return value of @c FALSE. Setting
 * context options or selecting or removing codecs cancels the output method
 * selection.
 */
rte_bool
rte_set_output_callback_slave(rte_context *context,
				rte_buffer_callback write_cb,
				rte_seek_callback seek_cb)
{
	rte_bool r;

	nullcheck(context, return FALSE);

	rte_error_reset(context);

	nullcheck(write_cb, return FALSE);

	r = xc->set_output(context, write_cb, seek_cb);

	if (r) {
		context->output_method = RTE_CALLBACK_SLAVE;
		context->output_fd = -1;
	}

	return r;
}

static rte_bool
write_cb(rte_context *context, rte_codec *codec, rte_buffer *buffer)
{
	ssize_t actual;

	if (!buffer) /* EOF */
		return TRUE;

	do actual = write(context->output_fd, buffer->data, buffer->size);
	while (actual == (ssize_t) -1 && errno == EINTR);

	// XXX error propagation?
	// Aborting encoding is bad for Zapping. It should a)
	// activate its backup plan and b) notify the user before
	// data is lost. Have to use private callback.

	return actual == buffer->size; /* no error */
}

static rte_bool
seek_cb(rte_context *context, long long offset, int whence)
{
	// XXX error propagation?

#if defined(HAVE_LARGEFILE) && defined(O_LARGEFILE)
	return lseek64(context->output_fd, (off64_t) offset, whence) != (off64_t) -1;
#else
	if (offset < INT_MIN || offset > INT_MAX)
		return FALSE; 

	return lseek(context->output_fd, (off_t) offset, whence) != (off_t) -1;
#endif
}

static void
new_output_fd(rte_context *context, rte_io_method new_method, int new_fd)
{
	switch (context->output_method) {
	case RTE_FILE:
		// XXX can fail
		close(context->output_fd);
		break;

	default:
		break;
	}

	context->output_method = new_method;
	context->output_fd = new_fd;
}

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param file_name File descriptor to write to.
 *
 * Sets the output mode for the context and makes the context ready
 * to start encoding. All output of the codec will be written into the
 * given file. Where possible the file must be opened in 64 bit mode
 * (O_LARGEFILE flag). The context may need to seek(), see rte_context_info.
 *
 * @return
 * Before selecting an output method you must select input methods for all
 * codecs, else this function fails with a return value of @c FALSE. Setting
 * context options or selecting or removing codecs cancels the output method
 * selection.
 */
rte_bool
rte_set_output_stdio(rte_context *context, int fd)
{
	nullcheck(context, return FALSE);

	rte_error_reset(context);

	if (fd < 0)
		return FALSE;

	if (rte_set_output_callback_slave(context, write_cb, seek_cb)) {
		new_output_fd(context, RTE_STDIO, fd);
		return TRUE;
	}

	return FALSE;
}

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param filename Name of the file to create.
 *
 * Sets the output mode for the context and makes the context ready
 * to start encoding. This function creates a file where all output
 * of the codec will be written to.
 *
 * @return
 * Before selecting an output method you must select input methods for all
 * codecs, else this function fails with a return value of @c FALSE. Setting
 * context options or selecting or removing codecs cancels the output method
 * selection. When the file could not be created this function also returns
 * @c FALSE.
 */
rte_bool
rte_set_output_file(rte_context *context, const char *filename)
{
	int fd;

	nullcheck(context, return FALSE);

	rte_error_reset(context);

	fd = open(filename,
#if defined(HAVE_LARGEFILE) && defined(O_LARGEFILE)
		  O_CREAT | O_WRONLY | O_TRUNC | O_LARGEFILE,
#else
		  O_CREAT | O_WRONLY | O_TRUNC,
#endif
		  S_IRUSR | S_IWUSR |
		  S_IRGRP | S_IWGRP |
		  S_IROTH | S_IWOTH);

	if (fd == -1) {
		rte_error_printf(context, "Cannot create file '%s': %s.",
				 filename, strerror(errno));
		return FALSE;
	}

	if (rte_set_output_callback_slave(context, write_cb, seek_cb)) {
		new_output_fd(context, RTE_FILE, fd);
		return TRUE;
	} else {
		close(fd);
		unlink(filename);
		return FALSE;
	}
}

static rte_bool
discard_write_cb(rte_context *context, rte_codec *codec, rte_buffer *buffer)
{
	return TRUE;
}

static rte_bool
discard_seek_cb(rte_context *context, long long offset, int whence)
{
	return TRUE;
}

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 *
 * Sets the output mode for the context and makes the context ready
 * to start encoding. All output of the codec will be discarded (for
 * testing purposes).
 *
 * @return
 * Before selecting an output method you must select input methods for all
 * codecs, else this function fails with a return value of @c FALSE. Setting
 * context options or selecting or removing codecs cancels the output method
 * selection.
 */
rte_bool
rte_set_output_discard(rte_context *context)
{
	nullcheck(context, return FALSE);

	rte_error_reset(context);

	if (rte_set_output_callback_slave(context, discard_write_cb, discard_seek_cb)) {
		new_output_fd(context, RTE_DISCARD, -1);
		return TRUE;
	}

	return FALSE;
}

/*
 *  Start / Stop
 */

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param timestamp Start instant, pass 0.0 for now.
 * @param sync_ref Pass @c NULL.
 * @param async Pass @c TRUE.
 *
 * Start encoding. XXX describe me
 *
 * @return
 * @c FALSE on error.
 */
rte_bool
rte_start(rte_context *context, double timestamp, rte_codec *sync_ref, rte_bool async)
{
	rte_bool r;

	nullcheck(context, return FALSE);
	rte_error_reset(context);

	if (!async)
		return FALSE;

	r = xc->start(context, timestamp, sync_ref, async);

	return r;
}

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param timestamp Stop instant, pass 0.0 for now.
 * 
 * Stop encoding. XXX describe me
 *
 * Do not call this from a Unix signal handler.
 *
 * @return
 * @c FALSE on error.
 */
rte_bool
rte_stop(rte_context *context, double timestamp)
{
	rte_bool r;

	nullcheck(context, return FALSE);
	rte_error_reset(context);

	r = xc->stop(context, timestamp);

	if (r)
		switch (context->output_method) {
		case RTE_FILE:
			// XXX this can fail, notify caller
			new_output_fd(context, 0, -1); /* close */
			break;

		default:
			break;
		}

	return r;
}

/* rte_pause() TODO */
/* rte_resume() TODO */

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param status Status structure to be filled.
 * 
 * Fill a rte_status structure with information about the encoding
 * process. Data applies to the context (i. e. multiplexer, file format
 * encoder). @see rte_codec_status().
 */
static_inline void
rte_context_status(rte_context *context, rte_status *status);

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 *
 * When a RTE function failed you can use this function to get a
 * verbose description of the failure cause, if available.
 *
 * @return
 * Static pointer, string not to be freed, describing the error. The pointer
 * remains valid until the next call of a RTE function for this context
 * or any of its codecs.
 */
char *
rte_errstr(rte_context *context)
{
	if (!context)
		return "Invalid RTE context.";

	if (!context->error)
		return _("Unknown error.");

	return context->error;
}

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param templ See printf().
 * @param ... See printf().
 * 
 * Store an error description in the @a context which can be retrieved
 * with rte_errstr(). This is primarily intended for use by RTE backends.
 */
/* XXX Methinks in the long run we'll need an error handler. */
void
rte_error_printf(rte_context *context, const char *templ, ...)
{
	char buf[512], *s, *t;
	va_list ap;
	int temp;

	if (!context)
		return;

	temp = errno;

	va_start(ap, templ);
	vsnprintf(buf, sizeof(buf) - 1, templ, ap);
	va_end(ap);

	s = strdup(buf);

	t = context->error;
	context->error = s;

	if (t)
		free(t);

	errno = temp;
}
