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

/* $Id: rte.c,v 1.16 2002-06-18 02:26:38 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "rtepriv.h"

/*
[enum context]
rte_context_new (which format)
  for each option not left at defaults
    rte_option_set
  [enum codecs]
  for each elementary stream to be encoded
    rte_codec_set (which format)
      [enum options]
      for each option not left at defaults
        rte_option_set
      negotiate sample parameters (changing options after this unlocks
                                   parameters)
      select input interface (which also puts the codec in ready
                              state, changing parameters unlocks)
  select output interface (this puts the whole context in ready state,
                           replacing rte_init)
start/pause/restart/stop
delete 
 */

#define xc context->class
#define dc codec->class

/**
 * rte_set_input_callback_active:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * @read_cb: Function called by the codec to read more data to encode.
 * @unref_cb: Optional function called by the codec to free the data.
 * @queue_length: When non-zero, the codec queue length is returned here. That
 *   is the maximum number of buffers read before freeing the oldest.
 *   When for example @read_cb and @unref_cb calls always pair, this number
 *   is 1.
 *
 * Sets the input mode for the codec and puts the codec into ready state.
 * Using this method, when the @codec needs more data it will call @read_cb
 * with a rte_buffer to be initialized by the rte client. After using the
 * data, it is released by calling @unref_cb. See #rte_buffer_callback for
 * the handshake details.
 *
 * Attention: A codec may read more than once before freeing the data, and it
 * may also free the data in a different order than it has been read.
 *
 * <example><title>Typical usage of rte_set_input_callback_active()</title>
 * <programlisting>
 * rte_bool
 * my_read_cb(rte_context *context, rte_codec *codec, rte_buffer *buffer)
 * {
 * &nbsp;    buffer->data = malloc();
 * &nbsp;    read(buffer->data, &amp;buffer->timestamp);
 * &nbsp;    return TRUE;
 * }
 * &nbsp;
 * rte_bool
 * my_unref_cb(rte_context *context, rte_codec *codec, rte_buffer *buffer)
 * {
 * &nbsp;    free(buffer->data);
 * }
 * </programlisting></example>
 *
 * Return value:
 * Before selecting an input method you must negotiate sample parameters with
 * rte_codec_set_parameters(), else this function fails with a return value
 * of %FALSE. Setting codec options invalidates previously selected sample
 * parameters, and thus also the input method selection. The function can
 * also fail when the codec does not support this input method.
 **/
rte_bool
rte_set_input_callback_active(rte_codec *codec,
			      rte_buffer_callback read_cb,
			      rte_buffer_callback unref_cb,
			      int *queue_length)
{
	rte_context *context = NULL;
	rte_bool r = FALSE;
	int ql = 0;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	nullcheck(read_cb, return FALSE);

	if (!queue_length)
		queue_length = &ql;

	if (xc->set_input)
		r = xc->set_input(codec, RTE_CALLBACK_ACTIVE,
				  read_cb, unref_cb, queue_length);
	else if (dc->set_input)
		r = dc->set_input(codec, RTE_CALLBACK_ACTIVE,
				  read_cb, unref_cb, queue_length);
	else
		assert(!"codec bug");

	if (r) {
		codec->input_method = RTE_CALLBACK_ACTIVE;
		codec->input_fd = -1;
	}

	return r;
}

/**
 * rte_set_input_callback_passive:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * @read_cb: Function called by the codec to read more data to encode.
 *
 * Sets the input mode for the codec and puts the codec into ready state.
 * Using this method the codec allocates the necessary buffers, when it
 * needs more data it calls @data_cb, passing a pointer to the buffer
 * space where the client shall copy the data.
 *
 * <example><title>Typical usage of rte_set_input_callback_passive()</title>
 * <programlisting>
 * rte_bool
 * my_read_cb(rte_context *context, rte_codec *codec, rte_buffer *buffer)
 * {
 * &nbsp;    read(buffer->data, &amp;buffer->timestamp);
 * &nbsp;    return TRUE;
 * }
 * </programlisting></example>
 *
 * Return value:
 * Before selecting an input method you must negotiate sample parameters with
 * rte_codec_set_parameters(), else this function fails with a return value
 * of %FALSE. Setting codec options invalidates previously selected sample
 * parameters, and thus also the input method selection. The function can
 * also fail when the codec does not support this input method.
 **/
rte_bool
rte_set_input_callback_passive(rte_codec *codec,
			       rte_buffer_callback read_cb)
{
	rte_context *context = NULL;
	rte_bool r = FALSE;
	int ql = 0;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	nullcheck(read_cb, return FALSE);

	if (xc->set_input)
		r = xc->set_input(codec, RTE_CALLBACK_PASSIVE, read_cb, NULL, &ql);
	else if (dc->set_input)
		r = dc->set_input(codec, RTE_CALLBACK_PASSIVE, read_cb, NULL, &ql);
	else
		assert(!"codec bug");

	if (r) {
		codec->input_method = RTE_CALLBACK_PASSIVE;
		codec->input_fd = -1;
	}

	return r;
}

/**
 * rte_set_input_push_active:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * @unref_cb: Optional function called as subroutine of rte_push_buffer()
 *   to free the data.
 * @queue_request: The minimum number of buffers you will be able to push before
 *   rte_push_buffer() blocks.
 * @queue_length: When non-zero the codec queue length is returned here. This is
 *   at least the number of buffers read before freeing the oldest, and at most
 *   @queue_request. When for example rte_push_buffer() and @unref_cb calls
 *   always pair, the minimum length is 1.
 *
 * Sets the input mode for the codec and puts the codec into ready state.
 * Using this method, when the codec needs data it waits until the rte client
 * called rte_push_buffer(). After using the data, it is released by calling
 * @unref_cb. See #rte_buffer_callback for the handshake details.
 *
 * Attention: A codec may wait for more than one buffer before releasing the
 * oldest, it may also free in a different order than has been pushed.
 *
 * <example><title>Typical usage of rte_set_input_push_active()</title>
 * <programlisting>
 * rte_bool
 * my_unref_cb(rte_context *context, rte_codec *codec, rte_buffer *buffer)
 * {
 * &nbsp;    free(buffer->data);
 * }
 * &nbsp;
 * while (have_data) {
 * &nbsp;    rte_buffer buffer;
 * &nbsp;
 * &nbsp;    buffer.data = malloc();
 * &nbsp;    read(buffer.data, &amp;buffer.timestamp);
 * &nbsp;    if (!rte_push_buffer(codec, &amp;buffer, FALSE)) {
 * &nbsp;        // The codec is not fast enough, we drop the frame.
 * &nbsp;        free(buffer.data);
 * &nbsp;    }
 * }
 * </programlisting></example>
 *
 * Return value:
 * Before selecting an input method you must negotiate sample parameters with
 * rte_codec_set_parameters(), else this function fails with a return value
 * of %FALSE. Setting codec options invalidates previously selected sample
 * parameters, and thus also the input method selection. The function can
 * also fail when the codec does not support this input method.
 **/
rte_bool
rte_set_input_push_active(rte_codec *codec,
			  rte_buffer_callback unref_cb,
			  int queue_request, int *queue_length)
{
	rte_context *context = NULL;
	rte_bool r = FALSE;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	if (!queue_length)
		queue_length = &queue_request;
	else
		*queue_length = queue_request;

	if (xc->set_input)
		r = xc->set_input(codec, RTE_PUSH_PULL_ACTIVE,
				  NULL, unref_cb, queue_length);
	else if (dc->set_input)
		r = dc->set_input(codec, RTE_PUSH_PULL_ACTIVE,
				  NULL, unref_cb, queue_length);
	else
		assert(!"codec bug");

	if (r) {
		codec->input_method = RTE_PUSH_PULL_ACTIVE;
		codec->input_fd = -1;
	}

	return r;
}

/**
 * rte_set_input_push_passive:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * @queue_request: The minimum number of buffers you will be able to push before
 *   rte_push_buffer() blocks.
 * @queue_length: When non-zero the actual codec queue length is returned here,
 *   this may be more or less than @queue_request.
 *
 * Sets the input mode for the codec and puts the codec into ready state.
 * Using this method the codec allocates the necessary buffers, when it needs more
 * data it waits until the rte client called rte_push_buffer(). In buffer.data
 * this function always returns a pointer to buffer space where the rte client
 * shall store the data. You can pass %NULL as buffer.data to start the cycle.
 *
 * <example><title>Typical usage of rte_set_input_push_passive()</title>
 * <programlisting>
 * rte_buffer buffer;
 * &nbsp;
 * buffer.data = NULL;
 * rte_push_buffer(codec, &amp;buffer, FALSE); // cannot fail
 * &nbsp;
 * while (have_data) {
 * &nbsp;    read(buffer.data, &amp;buffer.timestamp);
 * &nbsp;    if (!rte_push_buffer(codec, &amp;buffer, FALSE)) {
 * &nbsp;         // The codec is not fast enough, we drop the frame.
 * &nbsp;    }
 * }
 * </programlisting></example>
 *
 * Return value:
 * Before selecting an input method you must negotiate sample parameters with
 * rte_codec_set_parameters(), else this function fails with a return value
 * of %FALSE. Setting codec options invalidates previously selected sample
 * parameters, and thus also the input method selection. The function can
 * also fail when the codec does not support this input method.
 **/
rte_bool
rte_set_input_push_passive(rte_codec *codec, int queue_request, int *queue_length)
{
	rte_context *context = NULL;
	rte_bool r = FALSE;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	if (!queue_length)
		queue_length = &queue_request;
	else
		*queue_length = queue_request;

	if (xc->set_input)
		r = xc->set_input(codec, RTE_PUSH_PULL_PASSIVE,
				  NULL, NULL, queue_length);
	else if (dc->set_input)
		r = dc->set_input(codec, RTE_PUSH_PULL_PASSIVE,
				  NULL, NULL, queue_length);
	else
		assert(!"codec bug");

	if (r) {
		codec->input_method = RTE_PUSH_PULL_PASSIVE;
		codec->input_fd = -1;
	}

	return r;
}

/**
 * rte_push_buffer:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * @buffer: Pointer to a #rte_buffer, can be %NULL.
 * @blocking: %TRUE to enable blocking behaviour.
 * 
 * Passes data for encoding to the codec when the input method is 'push'.
 * When the codec input queue is full and @blocking is %TRUE this function waits
 * until space becomes available, when @blocking is %FALSE it immediately
 * returns %FALSE.
 *
 * Return value:
 * %FALSE if the function would block. In active push mode and when the function
 * fails, the contents of @buffer are unmodified on return. Otherwise, that is in
 * passive push mode, buffer.data points to the next buffer space to be filled.
 * You can always obtain a pointer by calling rte_push_buffer() with buffer.data
 * set to %NULL, in active push mode this has no effect.
 **/
rte_bool
rte_push_buffer(rte_codec *codec, rte_buffer *buffer, rte_bool blocking)
{
	rte_context *context = NULL;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	if (xc->push_buffer)
		return xc->push_buffer(codec, buffer, blocking);
	else if (dc->push_buffer)
		return dc->push_buffer(codec, buffer, blocking);
	else
		return FALSE;
}

/**
 * rte_set_output_callback_passive:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @write_cb: Function called by the context to write encoded data.
 *   The codec parameter of the callback is not used (%NULL).
 * @seek_cb: Optional function called by the context to move the output
 *  file pointer, for example to complete a header.
 *
 * Sets the output mode for the context and makes the context ready
 * to start encoding. Using this method the codec allocates the necessary
 * buffers, when data is available for writing it calls @write_cb with
 * buffer.data and buffer.size initialized.
 *
 * <example><title>Typical usage of rte_set_output_callback()</title>
 * <programlisting>
 * rte_bool
 * my_write_cb(rte_context *context, rte_codec *codec, rte_buffer *buffer)
 * {
 * &nbsp;    ssize_t actual;
 * &nbsp;
 * &nbsp;    if (!buffer) // EOF
 * &nbsp;        return TRUE;
 * &nbsp;
 * &nbsp;    do actual = write(STDOUT_FILENO, buffer->data, buffer->size);
 * &nbsp;    while (actual == -1 && errno == EINTR);
 * &nbsp;
 * &nbsp;    return actual == buffer->size; // no error
 * }
 * &nbsp;
 * rte_bool
 * my_seek_cb(rte_context *context, off64_t offset, int whence)
 * {
 * &nbsp;    return lseek64(STDOUT_FILENO, offset, whence) != (off64_t) -1;
 * }
 * </programlisting></example>
 *
 * Return value:
 * Before selecting an output method you must select input methods for all
 * codecs, else this function fails with a return value of %FALSE. Setting
 * context options or selecting or removing codecs cancels the output method
 * selection.
 **/
rte_bool
rte_set_output_callback_passive(rte_context *context,
				rte_buffer_callback write_cb,
				rte_seek_callback seek_cb)
{
	rte_bool r;

	nullcheck(context, return FALSE);

	rte_error_reset(context);

	nullcheck(write_cb, return FALSE);

	r = xc->set_output(context, write_cb, seek_cb);

	if (r) {
		context->output_method = RTE_CALLBACK_PASSIVE;
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
seek_cb(rte_context *context, off64_t offset, int whence)
{
	// XXX error propagation?
	return lseek64(context->output_fd, offset, whence) != (off64_t) -1;
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
 * rte_set_output_stdio:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @file_name: File descriptor to write to.
 *
 * Sets the output mode for the context and makes the context ready
 * to start encoding. All output of the codec will be written into the
 * given file. Use 64 bit mode when opening the file, it must be
 * seek()able too.
 *
 * Return value:
 * Before selecting an output method you must select input methods for all
 * codecs, else this function fails with a return value of %FALSE. Setting
 * context options or selecting or removing codecs cancels the output method
 * selection.
 **/
rte_bool
rte_set_output_stdio(rte_context *context, int fd)
{
	nullcheck(context, return FALSE);

	rte_error_reset(context);

	if (fd < 0)
		return FALSE;

	if (rte_set_output_callback_passive(context, write_cb, seek_cb)) {
		new_output_fd(context, RTE_STDIO, fd);
		return TRUE;
	}

	return FALSE;
}

/**
 * rte_set_output_file:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @filename: Name of the file to create.
 *
 * Sets the output mode for the context and makes the context ready
 * to start encoding. This function creates a file where all output
 * of the codec will be written to.
 *
 * Return value:
 * Before selecting an output method you must select input methods for all
 * codecs, else this function fails with a return value of %FALSE. Setting
 * context options or selecting or removing codecs cancels the output method
 * selection. When the file could not be created this function also returns
 * %FALSE.
 **/
rte_bool
rte_set_output_file(rte_context *context, const char *filename)
{
	int fd;

	nullcheck(context, return FALSE);

	rte_error_reset(context);

	fd = open(filename,
		  O_CREAT | O_WRONLY | O_TRUNC | O_LARGEFILE,
		  S_IRUSR | S_IWUSR |
		  S_IRGRP | S_IWGRP |
		  S_IROTH | S_IWOTH);

	if (fd == -1) {
		rte_error_printf(context, "Cannot create file '%s': %s.",
				 filename, strerror(errno));
		return FALSE;
	}

	if (rte_set_output_callback_passive(context, write_cb, seek_cb)) {
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
discard_seek_cb(rte_context *context, off64_t offset, int whence)
{
	return TRUE;
}

/**
 * rte_set_output_discard:
 * @context: Initialized #rte_context as returned by rte_context_new().
 *
 * Sets the output mode for the context and makes the context ready
 * to start encoding. All output of the codec will be discarded (for
 * testing purposes).
 *
 * Return value:
 * Before selecting an output method you must select input methods for all
 * codecs, else this function fails with a return value of %FALSE. Setting
 * context options or selecting or removing codecs cancels the output method
 * selection.
 **/
rte_bool
rte_set_output_discard(rte_context *context)
{
	nullcheck(context, return FALSE);

	rte_error_reset(context);

	if (rte_set_output_callback_passive(context, discard_write_cb, discard_seek_cb)) {
		new_output_fd(context, RTE_DISCARD, -1);
		return TRUE;
	}

	return FALSE;
}

/*
 *  Start / Stop
 */

/**
 * rte_start:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @timestamp: Start instant, pass 0.0.
 * @sync_ref: Pass %NULL.
 * @async: Pass %TRUE.
 *
 * XXX describe me
 *
 * Return value:
 * %FALSE on error.
 **/
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
 * rte_stop:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @timestamp: Stop instant.
 * 
 * XXX describe me
 *
 * Do not call this from a signal handler.
 *
 * Return value:
 * %FALSE on error.
 **/
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

/* no public prototype */
void rte_status_query(rte_context *context, rte_codec *codec,
		      rte_status *status, int size);

/*
 *  This functions returns context or codec status. It is not
 *  client visible but wrapped in inline rte_context_status() and
 *  rte_codec_status(). The @size argument is a compile time
 *  constant on the client side, this permits upwards compatible
 *  extensions to #rte_status.
 *
 *  No keywords because the app must look up values anyway, we're
 *  in critical path and a struct is much faster than enum & strcmp.
 *  A copy of #rte_status is returned because the values are calculated
 *  on the fly and must be r/w locked.
 */
void
rte_status_query(rte_context *context, rte_codec *codec,
		 rte_status *status, int size)
{
	assert(status != NULL);
	assert(size >= sizeof(status->valid));

	if (codec)
		context = codec->context;
	if (!context || !xc->status) {
		status->valid = 0;
		return;
	}

	if (context->state != RTE_STATE_RUNNING) {
		status->valid = 0;
		return;
	}

	if (size > sizeof(*status))
		size = sizeof(*status);

	xc->status(context, codec, status, size);
}

#if 0 /* obsolete */

void
rte_status_free(rte_status_info *status)
{
	rte_context *context = NULL;

	nullcheck(status, return);

	if (status->type == RTE_STRING)
		free(status->val.str);

	free(status);
}

#endif

/**
 * rte_option_string:
 * @context: Initialized #rte_context.
 * @codec: Pointer to #rte_codec if these are codec options,
 *   %NULL if context options.
 * @optstr: Option string.
 *
 * RTE internal function to parse an option string and set
 * the options accordingly.
 *
 * Return value:
 * %FALSE if the string is invalid or setting some option failed.
 **/
rte_bool
rte_option_string(rte_context *context, rte_codec *codec, const char *optstr)
{
	rte_option_info *oi;
	char *s, *s1, *keyword, *string, quote;
	rte_bool r = TRUE;

	assert(context != NULL);
	assert(optstr != NULL);

	s = s1 = strdup(optstr);

	if (!s) {
		rte_error_printf(context, _("Out of memory."));
		return FALSE;
	}

	do {
		while (isspace(*s))
			s++;

		if (*s == ',' || *s == ';') {
			s++;
			continue;
		}

		if (!*s)
			break;

		keyword = s;

		while (isalnum(*s) || *s == '_')
			s++;

		if (!*s)
			goto invalid;

		*s++ = 0;

		while (isspace(*s) || *s == '=')
			s++;

		if (!*s) {
 invalid:
			rte_error_printf(context, "Invalid option string \"%s\".",
					 optstr);
			break;
		}

		if (codec)
			oi = rte_codec_option_info_keyword(codec, keyword);
		else
			oi = rte_context_option_info_keyword(context, keyword);

		if (!oi)
			break;

		switch (oi->type) {
		case RTE_OPTION_BOOL:
		case RTE_OPTION_INT:
		case RTE_OPTION_MENU:
			if (codec)
				r = rte_codec_option_set(codec,
					keyword, (int) strtol(s, &s, 0));
			else
				r = rte_context_option_set(context,
					keyword, (int) strtol(s, &s, 0));
			break;

		case RTE_OPTION_REAL:
			if (codec)
				r = rte_codec_option_set(codec,
					keyword, (double) strtod(s, &s));
			else
				r = rte_context_option_set(context,
					keyword, (double) strtod(s, &s));
			break;

		case RTE_OPTION_STRING:
			quote = 0;
			if (*s == '\'' || *s == '"')
				quote = *s++;
			string = s;

			while (*s && *s != quote
			       && (quote || (*s != ',' && *s != ';')))
				s++;
			if (*s)
				*s++ = 0;

			if (codec)
				r = rte_codec_option_set(codec, keyword, string);
			else
				r = rte_context_option_set(context, keyword, string);
			break;

		default:
			fprintf(stderr,	"rte:%s: unknown export option type %d\n",
				__PRETTY_FUNCTION__, oi->type);
			exit(EXIT_FAILURE);
		}

	} while (r);

	free(s1);

	return r;
}

/*
 *  Error functions
 */

/**
 * rte_asprintf:
 * @errstr: Place to store the allocated string or %NULL.
 * @templ: See printf().
 * @Varargs: See printf(). 
 * 
 * RTE internal helper function.
 *
 * Identical to GNU or BSD libc asprintf().
 **/
void
rte_asprintf(char **errstr, const char *templ, ...)
{
	char buf[512];
	va_list ap;
	int temp;

	if (!errstr)
		return;

	temp = errno;

	va_start(ap, templ);

	vsnprintf(buf, sizeof(buf) - 1, templ, ap);

	va_end(ap);

	*errstr = strdup(buf);

	errno = temp;
}

/**
 * rte_errstr:
 * @context: Initialized #rte_context as returned by rte_context_new().
 *
 * When a RTE function failed you can use this function to get a
 * verbose description of the failure cause, if available.
 *
 * Return value:
 * Static pointer, string not to be freed, describing the error. The pointer
 * remains valid until the next call of a RTE function for this context
 * and all its codecs.
 **/
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
 * rte_error_printf:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @templ: See printf().
 * @Varargs: See printf().
 * 
 * Store an error description in the @context which can be retrieved
 * with rte_errstr().
 **/
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

static char *
whois(rte_context *context, rte_codec *codec)
{
	char name[80];

	if (codec) {
		rte_codec_info *ci = &codec->class->public;

		snprintf(name, sizeof(name) - 1,
			 "codec %s", ci->label ? _(ci->label) : ci->keyword);
	} else if (context) {
		rte_context_info *ci = &context->class->public;

		snprintf(name, sizeof(name) - 1,
			 "context %s", ci->label ? _(ci->label) : ci->keyword);
	} else {
		fprintf(stderr, "rte bug: unknown context or codec called error function\n");
		return NULL;
	}

	return strdup(name);
}

/**
 * rte_unknown_option:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @codec: Pointer to #rte_codec if this refers to a codec option,
 *  %NULL if context option.
 * @keyword: Keyword of the option, can be %NULL or "".
 * 
 * RTE internal helper function.
 *
 * Sets the @context error string.
 **/
void
rte_unknown_option(rte_context *context, rte_codec *codec, const char *keyword)
{
	char *name = whois(context, codec);

	if (!name)
		return;

	if (!keyword)
		rte_error_printf(context, "No option keyword for %s.", name);
	else
		rte_error_printf(context, "'%s' is no option of %s.", keyword, name);

	free(name);
}

/**
 * rte_invalid_option:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @codec: Pointer to #rte_codec if this refers to a codec option,
 *  %NULL if context option.
 * @keyword: Keyword of the option, can be %NULL or "".
 * @Varargs: If the option is known, the invalid data (int, double, char *).
 * 
 * RTE internal helper function.
 *
 * Sets the @context error string.
 **/
void
rte_invalid_option(rte_context *context, rte_codec *codec, const char *keyword, ...)
{
	char buf[256], *name = whois(context, codec);
	rte_option_info *oi;

	if (!keyword || !keyword[0])
		return rte_unknown_option(context, codec, keyword);

	if (!name)
		return;

	if (codec)
		oi = rte_codec_option_info_keyword(codec, keyword);
	else
		oi = rte_context_option_info_keyword(context, keyword);

	if (oi) {
		va_list args;
		char *s;

		va_start(args, keyword);

		switch (oi->type) {
		case RTE_OPTION_BOOL:
		case RTE_OPTION_INT:
		case RTE_OPTION_MENU:
			snprintf(buf, sizeof(buf) - 1, "'%d'", va_arg(args, int));
			break;
		case RTE_OPTION_REAL:
			snprintf(buf, sizeof(buf) - 1, "'%f'", va_arg(args, double));
			break;
		case RTE_OPTION_STRING:
			s = va_arg(args, char *);
			if (s == NULL)
				strncpy(buf, "NULL", 4);
			else
				snprintf(buf, sizeof(buf) - 1, "'%s'", s);
			break;
		default:
			fprintf(stderr,	"rte:%s: unknown export option type %d\n",
				__PRETTY_FUNCTION__, oi->type);
			strncpy(buf, "?", 1);
			break;
		}

		va_end(args);
	} else
		strncpy(buf, "??", 2);

	rte_error_printf(context, "Invalid argument %s for option %s of %s.",
			 buf, keyword, name);
	free(name);
}

/**
 * rte_strdup:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @d: If non-zero, store pointer to allocated string here. When *d
 *   is non-zero, free(*d) the old string first.
 * @s: String to be duplicated.
 * 
 * RTE internal helper function.
 *
 * Same as the libc strdup(), except for @d argument and setting
 * the @context error string on failure.
 * 
 * Return value: 
 * %NULL on failure, pointer to malloc()ated string otherwise.
 **/
char *
rte_strdup(rte_context *context, char **d, const char *s)
{
	char *new = strdup(s ? s : "");

	if (!new) {
		rte_error_printf(context, _("Out of memory."));
		errno = ENOMEM;
		return NULL;
	}

	if (d) {
		if (*d)
			free(*d);
		*d = new;
	}

	return new;
}
