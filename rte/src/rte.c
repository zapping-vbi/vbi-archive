/*
 *  Real Time Encoder
 *
 *  Copyright (C) 2000-2001 Iñaki García Etxebarria
 *  Modified 2001 Michael H. Schimek
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
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "rtepriv.h"

/*
  TODO in no particular order:
  * context->bsize should be visible by the app.
  * Push modes not functional (think about termination).
  * better error reporting.
  * Pause, resume.
  * Status api.
  * Sliced vbi.
  * Finish b_mp1e, write b_divx4linux and b_ffmpeg.
  * i18n support.
*/

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
<<
  start/pause/restart/stop
delete 
 */

#define xc context->class
#define dc codec->class

/* Private flag for fifos */
#define BLANK_BUFFER 1

/* one function to iterate them all */
static rte_bool codec_forall(rte_context *context,
			     rte_bool (*func)(rte_codec*, rte_pointer),
			     rte_pointer udata)
{
	int i;
	int j;

	/* FIXME: Should this use codec->next instead?? */

	for (i=0; i<RTE_STREAM_MAX; i++)
#warning
		for (j=0; j<xc->public.max_elementary[i]; j++) {
			rte_codec *codec = rte_codec_get(context, i, j);
			if (codec && !func(codec, udata))
				return FALSE;
		}

	return TRUE;
}

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
	rte_context *context;
	int ql = 0;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	nullcheck(read_cb, return FALSE);

	if (!queue_length)
		queue_length = &ql;

	if (xc->set_input)
		return xc->set_input(codec, RTE_INPUT_CA,
				     read_cb, unref_cb, queue_length);
	else if (dc->option_enum)
		return dc->set_input(codec, RTE_INPUT_CA,
				     read_cb, unref_cb, queue_length);

	fprintf(stderr, "rte: context %s and codec %s lack mandatory "
		"set_input function\n", xc->public.keyword, dc->public.keyword);

	exit(EXIT_FAILURE);
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
	rte_context *context;
	int ql = 0;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	nullcheck(read_cb, return FALSE);

	if (xc->set_input)
		return xc->set_input(codec, RTE_INPUT_CP, read_cb, NULL, &ql);
	else if (dc->option_enum)
		return dc->set_input(codec, RTE_INPUT_CP, read_cb, NULL, &ql);

	fprintf(stderr, "rte: context %s and codec %s lack mandatory "
		"set_input function\n", xc->public.keyword, dc->public.keyword);

	exit(EXIT_FAILURE);
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
 * &nbsp;    buffer->data = malloc();
 * &nbsp;    read(buffer->data, &amp;buffer->timestamp);
 * &nbsp;    if (!rte_push_buffer(codec, &amp;buffer, FALSE)) {
 * &nbsp;        // The codec is not fast enough, we drop the frame.
 * &nbsp;        free(buffer->data);
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
	rte_context *context;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	if (!queue_length)
		queue_length = &queue_request;
	else
		*queue_length = queue_request;

	if (xc->set_input)
		return xc->set_input(codec, RTE_INPUT_PA,
				     NULL, unref_cb, queue_length);
	else if (dc->option_enum)
		return dc->set_input(codec, RTE_INPUT_PA,
				     NULL, unref_cb, queue_length);

	fprintf(stderr, "rte: context %s and codec %s lack mandatory "
		"set_input function\n", xc->public.keyword, dc->public.keyword);

	exit(EXIT_FAILURE);
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
 * &nbsp;    read(buffer->data, &amp;buffer->timestamp);
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
rte_set_input_push_data(rte_codec *codec,
			int queue_request, int *queue_length)
{
	rte_context *context;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	if (!queue_length)
		queue_length = &queue_request;
	else
		*queue_length = queue_request;

	if (xc->set_input)
		return xc->set_input(codec, RTE_INPUT_PP, NULL, NULL, queue_length);
	else if (dc->option_enum)
		return dc->set_input(codec, RTE_INPUT_PP, NULL, NULL, queue_length);

	fprintf(stderr, "rte: context %s and codec %s lack mandatory "
		"set_input function\n", xc->public.keyword, dc->public.keyword);

	exit(EXIT_FAILURE);
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
	rte_context *context;

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
 * @write_cb: Function called by the codec to write encoded data.
 * @seek_cb: Optional function called by the codec to move the output
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
 * &nbsp;    do actual = write(STDOUT_FILENO, buffer->data, buffer->size);
 * &nbsp;    while (actual == -1 && errno == EINTR);
 * &nbsp;
 * &nbsp;    return actual == buffer->size; // no error
 * }
 * &nbsp;
 * rte_bool
 * my_seek_cb(rte_context *context, rte_codec *codec, off64_t offset, int whence)
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
	nullcheck(context, return FALSE);

	rte_error_reset(context);

	nullcheck(write_cb, return FALSE);

	if (xc->set_output)
		return xc->set_output(context, write_cb, seek_cb);

	fprintf(stderr, "rte: context %s lacks mandatory "
		"set_output function\n", xc->public.keyword);

	exit(EXIT_FAILURE);
}







void
rte_status_free(rte_status_info *status)
{
	nullcheck(status, return);

	if (status->type == RTE_STRING)
	  free(status->val.str);

	free(status);
}

/* uninit all the codecs in the given context */
static void
codecs_uninit(rte_context *context)
{
#warning
#if 0
	static rte_bool uninit_codec(rte_codec *codec,
				     rte_pointer udata) {
		switch (codec->input_mode) {
		case RTE_INPUT_PD:
		{
			/* flush */
			buffer *b = codec->input.pd.last_buffer;
			if (b) {
				b->used = 0; /* EOF */
				send_full_buffer(&codec->prod, b);
			}
		}
			break;
		default:
			break;
		}
		dc->uninit(codec);

		rem_producer(&codec->prod);
		destroy_fifo(&codec->f);

		memset(&codec->input, 0, sizeof(codec->input));

		return TRUE;
	}

	codec_forall(context, uninit_codec, NULL);
	xc->uninit(context);
#endif
}

#if 0

/* Returns the required buffer size for the codec */
static int
buffer_size (rte_codec *codec)
{
	rte_stream_parameters par;
	rte_video_stream_params *vid = (rte_video_stream_params *) &par;

	rte_codec_parameters_get(codec, &par);

	if (dc->public.stream_type == RTE_STREAM_AUDIO)
		return par.audio.fragment_size;
	else if (dc->public.stream_type == RTE_STREAM_SLICED_VBI)
		return 0; /* FIXME */
#warning
	/* video */
	switch (vid->pixfmt) {
	case RTE_PIXFMT_YUV420:
		/* Assumes that there's no padding between Y, U, V
		   fields */
		/* FIXME */
		return vid->stride * vid->height +
			vid->uv_stride * vid->height * 2;
	default:
		return vid->stride * vid->height;
	}
}

/* Wait a buffer in callback buffered mode */
static inline void
rte_wait_cb(rte_codec *codec)
{
	producer *p = &(codec->prod);
	rte_buffer rbuf;
	buffer *b;

	codec->input.cb.get(codec->context, codec, &rbuf);

	b = wait_empty_buffer(p);
	b->data = rbuf.data;
	b->used = codec->bsize;
	b->time = rbuf.timestamp;
	b->user_data = rbuf.user_data;
	b->rte_flags |= ~BLANK_BUFFER;

	send_full_buffer(p, b);
}

/* callback data mode */
static inline void
rte_wait_cd(rte_codec *codec)
{
	producer *p = &(codec->prod);
	buffer *b;

	b = wait_empty_buffer(p);
	codec->input.cd.get(codec->context, codec, b->data, &(b->time));
	b->rte_flags |= ~BLANK_BUFFER;
	b->used = codec->bsize;

	send_full_buffer(p, b);
}

/* push (buffer or data, it's the same at this point) */
static inline void
rte_wait_push(rte_codec *codec)
{
	/* FIXME: To do */
	/*
	  Callback modes can be joined (in stop()) without problems
	  since callbacks will get the needed data for the codecs to
	  complete.
	  In push modes, when stop() is called, no more data might reach
	  the codec, so we should fake a few frames for the codec to
	  finish. We mark these as BLANK_BUFFER, indicating that
	  they shouldn't be unref'ed.
	 */
}

static void
rte_wait_full (fifo *f)
{
	rte_codec *codec = (rte_codec*)f->user_data;

	switch (codec->input_mode) {
	case RTE_INPUT_CB:
		rte_wait_cb(codec);
		break;
	case RTE_INPUT_CD:
		rte_wait_cd(codec);
		break;
	case RTE_INPUT_PB:
	case RTE_INPUT_PD:
		rte_wait_push(codec);
		break;
	default:
		assert(!"reached");
		break;
	}
}

static void
rte_send_empty (consumer *c, buffer *b)
{
	rte_codec *codec = (rte_codec*)c->fifo->user_data;
	rte_buffer rbuf;

	if (!(b->rte_flags & BLANK_BUFFER)) {
		rbuf.data = b->data;
		rbuf.timestamp = b->time;
		rbuf.user_data = b->user_data;

		switch (codec->input_mode) {
		case RTE_INPUT_PB:
			codec->input.pb.unref(codec->context, codec,
					      &rbuf);
			break;
		case RTE_INPUT_CB:
			codec->input.cb.unref(codec->context, codec,
					      &rbuf);
			break;
		default:
			/* Nothing to be done for unbuffered modes */
			break;
		}
	}

	/* XXX temporary hack */
	send_empty_buffered(c, b);
}

#endif

/* init one codec */
static rte_bool
init_codec (rte_codec *codec, rte_pointer udata)
{
#warning
#if 0
	rte_context *context = (rte_context*)udata;
	int buffers;
	int alloc_bytes = 0;
	int bsize = codec->bsize = buffer_size(codec);
	rte_bool retval = dc->pre_init(codec, &buffers);
	
	if (!retval) {
		rte_error(context, "Cannot pre_init %s",
			  dc->public.keyword);
		return FALSE;
	}
	
	switch (codec->input_mode) {
	case RTE_INPUT_PD:
		codec->input.pd.last_buffer = NULL;
	case RTE_INPUT_CD:
		alloc_bytes = bsize;
		break;
	default:
		alloc_bytes = 0;
		break;
	}

	if (buffers != init_callback_fifo
	    (&codec->f, dc->public.keyword, NULL, NULL,
	     rte_wait_full, rte_send_empty, buffers,
	     alloc_bytes)) {
		rte_error(context, "not enough mem");
		return FALSE;
	}

	codec->f.user_data = codec;

	retval = dc->post_init(codec);

	assert(add_producer(&codec->f, &codec->prod));

	if (!retval) {
		rem_producer(&codec->prod);
		destroy_fifo(&codec->f);
		rte_error(context, "Cannot post_init %s",
			  dc->public.keyword);
		return FALSE;
	}

	return retval;
#endif

	return 0;
}

rte_bool
rte_start(rte_context *context)
{
#warning
#if 0
	rte_bool result;
	int num_codecs = 0;
	char *failed_codec = NULL;

	/* check that all selected codecs are in ready state and count
	 selected codecs */
	static rte_bool check_ready (rte_codec *codec, rte_pointer udata) {
		if (codec->status != RTE_STATUS_READY) {
			failed_codec = dc->public.keyword;
			return FALSE; /* Stop forall */
		}
		num_codecs++;
		return TRUE;
	}

	nullcheck(context, return FALSE);

	if (context->status != RTE_STATUS_READY) {
		if (context->status == RTE_STATUS_RUNNING)
			rte_error(context, "Already encoding!");
		else if (context->status == RTE_STATUS_PAUSED)
			rte_error(context, "Paused, use rte_resume");
		else
			rte_error(context,
				  "You must context_set_output first");
		return FALSE;
	}

	if (!codec_forall(context, check_ready, NULL)) {
		rte_error(context, "Codec %s isn't ready to encode",
			  failed_codec);
		return FALSE;
	}

	if (!num_codecs) {
		rte_error(context, "No codecs set");
		return FALSE;
	}

	if (!xc->pre_init(context))
		return FALSE;

	if (!codec_forall(context, init_codec, NULL))
		return FALSE;

	result = xc->start(context);

	if (result)
		context->status = RTE_STATUS_RUNNING;
	else
		codecs_uninit(context);

	return result;
#endif
	return 0;
}

void
rte_stop(rte_context *context)
{
	nullcheck(context, return);

	if (context->status < RTE_STATUS_RUNNING) {
		rte_error(context, "Not running!!");
		return;
	}

	xc->stop(context);

	codecs_uninit(context);

	context->status = RTE_STATUS_READY;
}

void
rte_pause(rte_context *context)
{
	nullcheck(context, return);

	if (context->status != RTE_STATUS_RUNNING) {
		rte_error(context, "Not running!!");
		return;
	}

	/* FIXME: to do */

	context->status = RTE_STATUS_PAUSED;
}

rte_bool
rte_resume(rte_context *context)
{
	nullcheck(context, return FALSE);

	if (context->status != RTE_STATUS_PAUSED) {
		rte_error(context, "Not paused!!");
		return FALSE;
	}

	/* FIXME: to do */

	context->status = RTE_STATUS_RUNNING;

	return TRUE;
}

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

	if (codec)
		pthread_mutex_lock(&codec->mutex);
	else
		pthread_mutex_lock(&context->mutex);

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
			fprintf(stderr, __PRETTY_FUNCTION__
				": unknown export option type %d\n", oi->type);
			exit(EXIT_FAILURE);
		}

	} while (r);

	if (codec)
		pthread_mutex_unlock(&codec->mutex);
	else
		pthread_mutex_unlock(&context->mutex);

	free(s1);

	return r;
}

/*
 *  Error functions
 */

/**
 * rte_asprintf:
 * @errstr: 
 * @templ: See printf().
 * @Varargs: See printf(). 
 * 
 * RTE internal helper function.
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
 * Attention: This function is not thread safe. Never use it when
 * another thread may call a RTE function between the failed function
 * and the time you evaluated the error string.
 * <!-- This could be fixed by using thread keys. -->
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
 * with rte_errstr(). You can also include the current error description.
 *
 * FIXME: This function is not thread safe.
 **/
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

void
rte_invalid_option(rte_context *context, rte_codec *codec, const char *keyword, ...)
{
	char buf[256], *name = whois(context, codec);
	rte_option_info *oi;

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
			fprintf(stderr, __PRETTY_FUNCTION__
				": unknown export option type %d\n", oi->type);
			strncpy(buf, "?", 1);
			break;
		}

		va_end(args);
	} else
		buf[0] = 0;

	rte_error_printf(context, "Invalid argument %s for option %s of %s.",
			 buf, keyword, name);
	free(name);
}

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
