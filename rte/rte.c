/*
 *  Real Time Encoder lib
 *
 *  Copyright (C) 2000-2001 Iñaki García Etxebarria
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

/* $Id: rte.c,v 1.11 2001-10-07 10:55:51 mschimek Exp $ */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>

#include "mp1e/common/fifo.h" /* fifos */
#include "mp1e/common/log.h" /* printv, verbose */
#include "mp1e/common/math.h" /* MAX, MIN */
#include "mp1e/video/video.h" /* video_look_ahead, MAX_WIDTH, MAX_HEIGHT */
#include "rtepriv.h"

#define BACKEND backends[context->private->backend]

/*
  BUGS:
      . mux cannot be joined if no data has been encoded
      . It isn't reentrant.
      . We shouldn't use ASSERTS if possible
  TODO:
      . Add VBI support
      . Add preview support.
        [mhs: preview interface will change]
      . Drop the lib dependencies to a minimum.
*/

#define NUM_AUDIO_BUFFERS 4 /* audio buffers in the audio fifo */

extern rte_backend_info b_mp1e_info;
extern rte_backend_info b_ffmpeg_info;

static rte_backend_info *backends[] = 
{
	&b_mp1e_info,
	&b_ffmpeg_info
};

static const int num_backends = sizeof(backends)/sizeof(*backends);

/* Just produces a blank buffer */
static void
blank_callback(rte_context * context, void *data, double *time,
	       enum rte_mux_mode stream, void * user_data)
{
	*time = current_time();

	/* set to 0's (avoid ugly noise on stop) */
	if (stream == RTE_AUDIO)
		memset(data, 0, context->audio_bytes);
}

/*
 * Waits for video or audio data.
 * Returns a buffer valid for encoding.
 * NULL on error.
 */
static inline void
wait_data(rte_context * context, int video)
{
	producer *p;
	buffer *b;
	/* *_callback are volatile, access by reference */
	rteDataCallback *data_callback;
	rteBufferCallback *buffer_callback;
	rte_buffer rbuf;
	enum rte_mux_mode stream = video ? RTE_VIDEO : RTE_AUDIO;
	int bytes;

	nullcheck(context, assert(0));

	if (video) {
		p = &context->private->vid_prod;
		bytes = context->video_bytes;
		data_callback = &(context->private->video_data_callback);
		buffer_callback = &(context->private->video_buffer_callback);
	}
	else {
		p = &context->private->aud_prod;
		bytes = context->audio_bytes;
		data_callback = &(context->private->audio_data_callback);
		buffer_callback = &(context->private->audio_buffer_callback);
	}

	if (*data_callback) {
		b = wait_empty_buffer(p);

		(*data_callback)(context, b->data, &(b->time),
				 stream, context->private->user_data);

		if (*data_callback == blank_callback)
			b->rte_flags |= BLANK_BUFFER;
		else
			b->rte_flags &= ~BLANK_BUFFER;

		b->used = bytes;
		send_full_buffer(p, b);
		return;
	} else if (*buffer_callback) {
		for (;;) {
			(*buffer_callback)(context, &rbuf, stream);

			if (rbuf.data) {
				b = wait_empty_buffer(p);

				b->data = rbuf.data;
				b->used = bytes;
				b->time = rbuf.time;
				b->user_data = rbuf.user_data;
				b->rte_flags &= ~BLANK_BUFFER;

				send_full_buffer(p, b);
				return;
			}
		}
	} else {
		fifo *f = p->fifo;
	
		/* XXX Oops, we are *not* a callback producer */

		pthread_mutex_lock(&f->consumer->mutex);

		pthread_cleanup_push((void (*)(void *))
			pthread_mutex_unlock, &f->consumer->mutex);
		pthread_cond_wait(&f->consumer->cond,
				  &f->consumer->mutex);
		pthread_cleanup_pop(0);

		pthread_mutex_unlock(&f->consumer->mutex);

		/* -> wait_full_buffer() (our caller) */
	}
}

static void
video_wait_full(fifo *f)
{
	wait_data((rte_context *) f->user_data, 1);
}

static void
audio_wait_full(fifo *f)
{
	wait_data((rte_context *) f->user_data, 0);
}

static void
video_send_empty(consumer *c, buffer *b)
{
	rte_buffer rbuf;
	fifo *f = c->fifo;
	rteUnrefCallback unref_callback =
		((rte_context*)f->user_data)->private->video_unref_callback;

	if (unref_callback && !(b->rte_flags & BLANK_BUFFER)) {
		rbuf.data = b->data;
		rbuf.time = b->time;
		rbuf.user_data = b->user_data;
		
		unref_callback((rte_context*)f->user_data, &rbuf);
	}

	/* XXX temporary hack */
	send_empty_buffered(c, b);
}

static void
audio_send_empty(consumer *c, buffer *b)
{
	rte_buffer rbuf;
	fifo *f = c->fifo;
	rteUnrefCallback unref_callback =
		((rte_context*)f->user_data)->private->audio_unref_callback;

	if (unref_callback && !(b->rte_flags & BLANK_BUFFER)) {
		rbuf.data = b->data;
		rbuf.time = b->time;
		rbuf.user_data = b->user_data;
		
		unref_callback((rte_context*)f->user_data, &rbuf);
	}

	/* XXX temporary hack */
	send_empty_buffered(c, b);
}

/* The default write data callback */
static void default_write_callback ( rte_context * context,
				     void * data, size_t size,
				     void * user_data )
{
	ssize_t r;

	while (size) {
		r = write(context->private->fd, data, size);
		if ((r < 0) && (errno == EINTR))
			continue;

		if (r < 0)
		{
			rte_error(context, "%s", strerror(errno));
			return;
		}

		(char *) data += r;
		size -= r;
	}
}

/* The default seek callback */
static off_t default_seek_callback ( rte_context * context,
				     off_t offset,
				     int whence,
				     void * user_data)
{
	return lseek(context->private->fd, offset, whence);
}

rte_context * rte_context_new (int width, int height,
			       const char *backend,
			       void * user_data)
{
	rte_context * context;
	int priv_bytes=0, i;

	context = malloc(sizeof(rte_context));

	if (!context)
	{
		rte_error(NULL, "malloc(): [%d] %s", errno, strerror(errno));
		return NULL;
	}
	memset(context, 0, sizeof(rte_context));

	for (i=0; i<num_backends; i++)
		priv_bytes = MAX(priv_bytes, backends[i]->priv_size);

	context->private = malloc(priv_bytes);
	if (!context->private)
	{
		rte_error(NULL, "malloc(): [%d] %s", errno, strerror(errno));
		free(context);
		return NULL;
	}
	memset(context->private, 0, priv_bytes);

	context->mode = RTE_AUDIO_AND_VIDEO;

	context->private->backend = -1;
	if (!backend)
		backend = "mp1e";
	for (i=0; i<num_backends; i++)
		if (!strcasecmp(backends[i]->name, backend))
			context->private->backend = i;

	if (context->private->backend == -1)
	{
		free(context->private);
		free(context);
		rte_error(NULL, "backend [%s] not found", backend);
		return NULL;
	}

	if (!rte_set_video_parameters(context, RTE_YUV420,
				      width, height, RTE_RATE_3, 2.3e6,
				      "IBBPBBPBBPBB")) {
		rte_error(NULL, "invalid video format: %s",
			  context->error);

		return rte_context_destroy(context);
	}

	if (!rte_set_audio_parameters(context, 44100,
				      RTE_AUDIO_MODE_MONO, 8e4)) {
		rte_error(NULL, "invalid audio format: %s",
			  context->error);

		return rte_context_destroy(context);
	}

	context->private->user_data = user_data;
	context->private->fd = -1;

	BACKEND->context_new(context);

	return (context);
}

void * rte_context_destroy ( rte_context * context )
{
	nullcheck(context, return NULL);

	if (context->private->encoding)
		rte_stop(context);

	if (context->private->inited) {
		BACKEND->uninit_context(context);

		if (context->mode & RTE_VIDEO) {
			destroy_fifo(&context->private->vid);
		}
		if (context->mode & RTE_AUDIO) {
			destroy_fifo(&context->private->aud);
		}
	}

	BACKEND->context_destroy(context);

	if (context->error)
	{
		free(context->error);
		context->error = NULL;
	}

	if (context->file_name) {
		free(context->file_name);
		context->file_name = NULL;
	}

	free(context->private);
	free(context);

	return NULL;
}

void rte_set_input (rte_context * context,
		    enum rte_mux_mode stream,
		    enum rte_interface interface, int buffered,
		    rteDataCallback data_callback,
		    rteBufferCallback buffer_callback,
		    rteUnrefCallback unref_callback)
{
	nullcheck(context, return);

	if (stream & RTE_AUDIO) {
		context->private->audio_interface = interface;
		context->private->audio_buffered = buffered;
		context->private->audio_data_callback = data_callback;
		context->private->audio_buffer_callback =
			buffer_callback;
		context->private->audio_unref_callback = unref_callback;
	}
	if (stream & RTE_VIDEO) {
		context->private->video_interface = interface;
		context->private->video_buffered = buffered;
		context->private->video_data_callback = data_callback;
		context->private->video_buffer_callback =
			buffer_callback;
		context->private->video_unref_callback = unref_callback;
	}
}

void rte_set_output (rte_context * context,
		     rteEncodeCallback encode_callback,
		     rteSeekCallback seek_callback,
		     const char * filename)
{
	nullcheck(context, return);

	context->private->encode_callback = encode_callback;
	context->private->seek_callback = seek_callback;;

	if (context->private->inited) {
		rte_error(NULL, "context already inited");
		return;
	}

	if (context->file_name)
		free(context->file_name);
	
	if (filename)
		context->file_name = strdup(filename);
	else
		context->file_name = NULL;
}

char * rte_query_format (rte_context * context,
			 int n,
			 enum rte_mux_mode * mux_mode)
{
	nullcheck(context, return NULL);

	return BACKEND->query_format(context, n, mux_mode);
}

int rte_set_format (rte_context *context,
		    const char *format)
{
	/* Validation is done at init_context time by the backend */
	nullcheck(context, return 0);
	nullcheck(format, return 0);

	if (context->format)
		free(context->format);

	context->format = strdup(format);

	return 1;
}

#define DECIMATING(mode) (mode == RTE_YUYV_VERTICAL_DECIMATION ||	\
			  mode == RTE_YUYV_EXP_VERTICAL_DECIMATION)

int rte_set_video_parameters (rte_context * context,
			      enum rte_pixformat frame_format,
			      int width, int height,
			      enum rte_frame_rate video_rate,
			      size_t output_video_bits,
			      const char *gop_sequence)
{
	int i;

	nullcheck(context, return 0);

	if (context->private->inited) {
		rte_error(context, "context already inited");
		return 0;
	}

	if (!(context->mode & RTE_VIDEO)) {
		rte_error(context, "current muxmode is without video");
		return 0;
	}

	if ((width % 16) || (height % 16) ||
	    (width <= 0) || (height <= 0))
	{
		rte_error(context, "the given dimensions aren't 16 multiplus");
		return 0;
	}
	if (video_rate <= RTE_RATE_NORATE) {
		rte_error(context, "You must set the video rate");
		return 0;
	}
	if (!gop_sequence) /* use current value, undocumented but logical */
		gop_sequence = context->gop_sequence;
	if (strlen(gop_sequence) > 1023)
	{
		rte_error(context, "gop too long (max 1023 pictures):\n%s",
			  gop_sequence);
		return 0;
	}

	for (i=0; i<strlen(gop_sequence); i++)
		context->gop_sequence[i] = toupper(gop_sequence[i]);
	context->gop_sequence[i] = 0;

	if (strspn(context->gop_sequence, "IPB") !=
	    strlen(context->gop_sequence))
	{
		rte_error(context, "Only I, P and B frames allowed");
		return 0;
	}

	context->video_format = frame_format;
	context->width = MIN(width, MAX_WIDTH);
	context->height = MIN(height, MAX_HEIGHT);
	if (DECIMATING(context->video_format))
		context->height *= 2;
	context->video_rate = video_rate;
	context->output_video_bits = output_video_bits;
	context->video_bytes = context->width * context->height;

	switch (frame_format)
	{
	case RTE_YUYV_VERTICAL_DECIMATION:
	case RTE_YUYV_TEMPORAL_INTERPOLATION:
	case RTE_YUYV_VERTICAL_INTERPOLATION:
	case RTE_YUYV_PROGRESSIVE:
	case RTE_YUYV_PROGRESSIVE_TEMPORAL:
	case RTE_YUYV_EXP:
	case RTE_YUYV_EXP_VERTICAL_DECIMATION:
	case RTE_YUYV_EXP2:
	case RTE_YUYV:
		context->video_bytes *= 2;
		break;
	case RTE_YUV420:
	case RTE_YVU420:
		context->video_bytes *= 1.5;
		break;
	default:
		rte_error(context, "unhandled pixformat: %d", frame_format);
		return 0;
	}

	return 1;
}

/* Sets the audio parameters */
int rte_set_audio_parameters (rte_context * context,
			      int audio_rate,
			      enum rte_audio_mode audio_mode,
			      size_t output_audio_bits)
{
	nullcheck(context, return 0);

	if (context->private->inited)
	{
		rte_error(context, "context already inited");
		return 0;
	}

	if (!(context->mode & RTE_AUDIO)) {
		rte_error(context, "current muxmode is without audio");
		return 0;
	}

	context->audio_rate = audio_rate;
	context->audio_mode = audio_mode;
	context->output_audio_bits = output_audio_bits;

	return 1;
}

void rte_set_mode (rte_context * context, enum rte_mux_mode mode)
{
	nullcheck(context, return);

	if (context->private->inited)
	{
		rte_error(context, "context already inited");
		return;
	}

	context->mode = mode;
}

void rte_set_motion (rte_context * context, int min, int max)
{
	int t;

	nullcheck(context, return);

	if (context->private->inited)
	{
		rte_error(context, "context already inited");
		return;
	}

	if (min > max) {
		t = min;
		min = max;
		max = t;
	}

	if (max == 0)
		min = 0;
	else if (max > 64)
		max = 64;

	context->motion_min = min;
	context->motion_max = max;
}

void rte_set_user_data(rte_context * context, void * user_data)
{
	nullcheck(context, return);

	context->private->user_data = user_data;
}

void * rte_get_user_data(rte_context * context)
{
	nullcheck(context, return NULL);

	return context->private->user_data;
}

/* Prepare the context for encoding */
int rte_init_context ( rte_context * context )
{
	int alloc_bytes;

	nullcheck(context, return 0);

	if (context->private->encoding)
	{
		rte_error(context, "already encoding");
		return 0;
	}

	/* check for parameter consistency */
	if ((!context->private->encode_callback) &&
	    (!context->file_name)) {
		rte_error(context, "encode_callback == file_name == NULL");
		return 0;
	}
	if ((context->private->encode_callback) &&
	    (context->file_name)) {
		rte_error(context, "encode_callback != NULL, file_name "
			  "!= NULL too");
		return 0;
	}

	/* Init the backend first */
	if (!BACKEND->init_context(context))
		return 0;

	/* create needed fifos */
	if (context->mode & RTE_VIDEO) {
		/*
		 *  One for them, one for us, and one for every picture
		 *  the encoder will stack up. XXX video_look_ahead
		 *  and gop_sequence are mp1e specific.
		 */
		int min_buffers = 2
			+ video_look_ahead(context->gop_sequence);

		if (context->private->video_buffered)
			alloc_bytes = 0; /* no need for preallocated
					    mem */
		else
			alloc_bytes = context->video_bytes;

		if (min_buffers > init_callback_fifo(
			&(context->private->vid), "rte-video",
			NULL, NULL, video_wait_full, video_send_empty,
			min_buffers, alloc_bytes)) {
			rte_error(context, "not enough mem");
			return 0;
		}

		assert(add_producer(&(context->private->vid),
				    &(context->private->vid_prod)));

		context->private->vid.user_data = context;
	}

	if (context->mode & RTE_AUDIO) {
		if (!context->private->audio_buffered)
			alloc_bytes = context->audio_bytes;
		else
			alloc_bytes = 0;

		if (4 > init_callback_fifo(
			&(context->private->aud), "rte-audio",
			NULL, NULL, audio_wait_full, audio_send_empty,
			NUM_AUDIO_BUFFERS, alloc_bytes)) {
			if (context->mode & RTE_VIDEO)
				destroy_fifo(&(context->private->vid));
			rte_error(context, "not enough mem");
			return 0;
		}

		assert(add_producer(&(context->private->aud),
				    &(context->private->aud_prod)));

		context->private->aud.user_data = context;
	}
	
	if (context->file_name) {
		context->private->fd = creat(context->file_name, 00644);
		if (context->private->fd == -1) {
			rte_error(context, "creat(%s): %s [%d]",
				  context->file_name, strerror(errno), errno);
			if (context->mode & RTE_VIDEO)
				destroy_fifo(&(context->private->vid));
			if (context->mode & RTE_AUDIO)
				destroy_fifo(&(context->private->aud));
			return 0;
		}

		context->private->encode_callback =
			RTE_ENCODE_CALLBACK(default_write_callback);
		context->private->seek_callback =
			RTE_SEEK_CALLBACK(default_seek_callback);
	}

	/* Set up some fields */
	context->private->last_video_buffer =
		context->private->last_audio_buffer = NULL;

	/* allow pushing from now */
	context->private->inited = 1;

	return 1; /* done, we are prepared for encoding */
}

int rte_start_encoding (rte_context * context)
{
	nullcheck(context, return 0);

	if (!context->private->inited) {
		rte_error(context, "you need to init the context first");
		return 0;
	}

	/* Set up some fields */
	context->private->last_video_buffer =
		context->private->last_audio_buffer = NULL;

	if (!BACKEND->start(context))
		return 0;

	context->private->encoding = 1;
	context->private->bytes_out = 0;

	return 1;
}

void rte_stop ( rte_context * context )
{
	rteDataCallback audio_callback;
	rteDataCallback video_callback;

	nullcheck(context, return);
	
	if (!context->private->encoding) {
		rte_error(context, "the context isn't encoding now");
		return;
	}
	if (!context->private->inited) {
		rte_error(context, "the context hasn't been inited");
		return;
	}

	context->private->encoding = 0;

	/* save for future use */
	audio_callback = context->private->audio_data_callback;
	video_callback = context->private->video_data_callback;

	if ((context->mode & RTE_AUDIO) &&
	    (context->private->audio_interface == RTE_PUSH)) {
		buffer *b = context->private->last_audio_buffer;

		/* set to dead end */
		context->private->audio_data_callback = blank_callback;

		if (b) {
			context->private->last_audio_buffer = NULL;
			b->used = 0; /* EOF */
			send_full_buffer(&(context->private->aud_prod), b);
		}
	}

	if ((context->mode & RTE_VIDEO) &&
	    (context->private->video_interface == RTE_PUSH)) {
		buffer *b = context->private->last_video_buffer;

		/* set to dead end */
		context->private->video_data_callback = blank_callback;

		if (b) {
			context->private->last_video_buffer = NULL;
			b->used = 0; /* EOF */
			send_full_buffer(&(context->private->vid_prod), b);
		}
	}

	BACKEND->stop(context);
	BACKEND->uninit_context(context);

	context->private->inited = 0;

	if (context->mode & RTE_VIDEO) {
		destroy_fifo(&context->private->vid);
	}
	if (context->mode & RTE_AUDIO) {
		destroy_fifo(&context->private->aud);
	}

	context->private->audio_data_callback = audio_callback;
	context->private->video_data_callback = video_callback;

	if (context->private->fd > 0) {
		close(context->private->fd);
		context->private->fd = -1;
	}
}

/* Input handling functions */
void * rte_push_video_data ( rte_context * context, void * data,
			   double time )
{
	nullcheck(context, return NULL);

	if (!context->private->inited) {
		rte_error(NULL, "context not inited\n"
			"The context must be encoding for push to work.");
		rte_error(context, "context not inited");
		return NULL;
	}
	if (!(context->mode & RTE_VIDEO)) {
		rte_error(context, "Mux isn't prepared to encode video!");
		return NULL;
	}
	if (context->private->video_buffered) {
		rte_error(NULL, "use push_buffer, not push_data");
		return NULL;
	}
	if (!context->private->encoding) {
		rte_error(context, "context not encoding, push_video_data"
			  " not allowed");
		return NULL;
	}
	ASSERT("Arrr... stick to the usage, please\n",
	       (context->private->last_video_buffer && data)
	       || (!context->private->last_video_buffer && !data));

	if (data) {
		ASSERT("you haven't written to the provided buffer!\n",
		       data == context->private->last_video_buffer->data);
			
		context->private->last_video_buffer->time = time;
		context->private->last_video_buffer->used =
			context->video_bytes;
		ASSERT("size check", context->video_bytes ==
		       context->private->last_video_buffer->size);

		send_full_buffer(&(context->private->vid_prod),
				 context->private->last_video_buffer);
	}

	context->private->last_video_buffer =
		wait_empty_buffer(&(context->private->vid_prod));

	return context->private->last_video_buffer->data;
}

void rte_push_video_buffer ( rte_context * context,
			     rte_buffer * rbuf )
{
	buffer * b;
	nullcheck(context, return);

	if (!context->private->inited) {
		rte_error(NULL, "context not inited\n"
			"The context must be encoding for push to work.");
		rte_error(context, "context not inited");
		return;
	}
	if (!(context->mode & RTE_VIDEO)) {
		rte_error(context, "Mux isn't prepared to encode video!");
		return;
	}
	if (!context->private->video_buffered) {
		rte_error(NULL, "use push_data, not push_buffer");
		return;
	}
	if (!context->private->encoding) {
		rte_error(context, "context not encoding, push_video_buffer"
			  " not allowed");
		return;
	}

	b = wait_empty_buffer(&(context->private->vid_prod));

	b->time = rbuf->time;
	b->data = rbuf->data;
	b->used = context->video_bytes;
	b->user_data = rbuf->user_data;

	send_full_buffer(&(context->private->vid_prod), b);
}

void * rte_push_audio_data ( rte_context * context, void * data,
			     double time )
{
	nullcheck(context, return NULL);

	if (!context->private->inited) {
		rte_error(NULL, "context not inited\n"
			"The context must be encoding for push to work");
		rte_error(context, "context not inited");
		return NULL;
	}
	if (!(context->mode & RTE_AUDIO)) {
		rte_error(context, "Mux isn't prepared to encode audio!");
		return NULL;
	}
	if (context->private->audio_buffered) {
		rte_error(NULL, "use push_buffer, not push_data");
		return NULL;
	}
	if (!context->private->encoding) {
		rte_error(context, "context not encoding, push_audio_data"
			  " not allowed");
		return NULL;
	}

	ASSERT("Arrr... stick to the usage, please\n",
	       (context->private->last_audio_buffer && data)
	       || (!context->private->last_audio_buffer && !data));

	if (data) {
		ASSERT("you haven't written to the provided buffer!\n",
		       data == context->private->last_audio_buffer->data);
			
		context->private->last_audio_buffer->time = time;
		context->private->last_audio_buffer->used = context->audio_bytes;

		send_full_buffer(&(context->private->aud_prod),
				 context->private->last_audio_buffer);
	}

	context->private->last_audio_buffer =
		wait_empty_buffer(&(context->private->aud_prod));

	return context->private->last_audio_buffer->data;
}

void rte_push_audio_buffer ( rte_context * context,
			     rte_buffer * rbuf )
{
	buffer * b;
	nullcheck(context, return);

	if (!context->private->inited) {
		rte_error(NULL, "context not inited\n"
			"The context must be encoding for push to work.");
		rte_error(context, "context not inited");
		return;
	}
	if (!(context->mode & RTE_AUDIO)) {
		rte_error(context, "Mux isn't prepared to encode video!");
		return;
	}
	if (!context->private->audio_buffered) {
		rte_error(NULL, "use push_data, not push_buffer");
		return;
	}
	if (!context->private->encoding) {
		rte_error(context, "context not encoding, push_audio_buffer"
			  " not allowed");
		return;
	}

	b = wait_empty_buffer(&(context->private->aud_prod));

	b->time = rbuf->time;
	b->data = rbuf->data;
	b->used = context->audio_bytes;
	b->user_data = rbuf->user_data;

	send_full_buffer(&(context->private->aud_prod), b);
}

void rte_set_verbosity ( rte_context * context, int level )
{
	verbose = level;
}

int rte_get_verbosity ( rte_context * context )
{
	return verbose;
}

int rte_init ( void )
{
	int i;

	for (i=0; i<num_backends; i++)
		if (!backends[i]->init_backend())
			return 0;

	return 1;
}

void rte_get_status ( rte_context * context,
		      struct rte_status_info * status )
{
	nullcheck(context, return);
	nullcheck(status, return);

	if (!context->private->inited ||
	    !context->private->encoding) {
		memset(status, 0, sizeof(struct rte_status_info));
		return;
	}

	BACKEND->status(context, status);
}

/* Experimental */

/**
 * rte_enum_codec:
 * @context: Initialized rte_context.
 * @index: Index into the codec table, 0 ... n.
 * 
 * Enumerates elementary stream codecs available for the selected
 * backend / file / mux format. You should start at index 0, incrementing.
 * Assume a subsequent call to this function will overwrite the returned
 * codec description.
 * 
 * Return value:
 * Pointer to a rte_codec_info structure, %NULL if the context is invalid
 * or the index is out of bounds.
 **/
rte_codec_info *
rte_enum_codec(rte_context *context, int index)
{
	nullcheck(context, return NULL);

	/* XXX check backend validity */

	if (!BACKEND->enum_codec)
		return NULL;

	return BACKEND->enum_codec(context, index);
}

rte_codec *
rte_get_codec(rte_context *context, rte_stream_type stream_type,
	      int stream_index, char **codec_keyword_p)
{
	nullcheck(context, return NULL);

	if (!context || !BACKEND->get_codec) {
		if (codec_keyword_p)
			codec_keyword_p = NULL;
		return NULL;
	}

	return BACKEND->get_codec(context, stream_type,
				  stream_index, codec_keyword_p);
}

/**
 * rte_set_codec:
 * @context: Initialized rte_context.
 * @stream_type: RTE_STREAM_VIDEO, _AUDIO, ...
 * @stream_index: Stream number.
 * @codec_keyword: Codec identifier, e.g. from rte_codec_info.
 * 
 * Select the codec identified by @codec_keyword to encode the @stream_type
 * data as elementary stream number @stream_index. The stream number refers
 * for example to one of the 16 video or 32 audio streams contained in a
 * MPEG-1 program stream, but you should pass 0 for now.
 *
 * Setting a codec resets all properties of the codec for this stream type
 * and index. Passing a %NULL pointer as @codec_keyword withdraws encoding
 * of this stream type and index.
 * 
 * Return value: 
 * Pointer to an opaque rte_codec object (not even the RTE frontend knows
 * for sure what this is :-). On error %NULL is returned, which may be caused,
 * apart of invalid parameters, by an unknown @codec_keyword or a @stream_type
 * not suitable for the selected backend / mux / file format.
 *
 * Compatibility: 
 * Calling this function with @stream_type VIDEO and/or AUDIO replaces
 * rte_set_mode and vice versa.
 **/
rte_codec *
rte_set_codec(rte_context *context, rte_stream_type stream_type,
	      int stream_index, char *codec_keyword)
{
	nullcheck(context, return NULL);

	if (!BACKEND->set_codec)
		return NULL;

	return BACKEND->set_codec(context, stream_type,
				  stream_index, codec_keyword);
}

rte_option *
rte_enum_option(rte_codec *codec, int index)
{
	rte_context *context = NULL;

	nullcheck(codec, return 0);
	nullcheck((context = codec->context), return 0);

	if (!BACKEND->enum_option)
		return NULL;

	return BACKEND->enum_option(codec, index);
}

int
rte_get_option(rte_codec *codec, char *keyword, rte_option_value *v)
{
	rte_context *context = NULL;

	nullcheck(codec, return 0);
	nullcheck((context = codec->context), return 0);

	if (!BACKEND->get_option)
		return 0;

	return BACKEND->get_option(codec, keyword, v);
}

int
rte_set_option(rte_codec *codec, char *keyword, ...)
{
	rte_context *context = NULL;
	va_list args;
	int r;

	nullcheck(codec, return 0);
	nullcheck((context = codec->context), return 0);

	if (!BACKEND->set_option)
		return 0;

	va_start(args, keyword);

	r = BACKEND->set_option(codec, keyword, args);

	va_end(args);

	return r;
}

int
rte_get_option_menu(rte_codec *codec, char *keyword, int *entry)
{
	rte_option *option;
	rte_option_value val;
	int r, i;

	nullcheck(codec, return 0);
	nullcheck(keyword, return 0);
	nullcheck(entry, return 0);

	*entry = 0;

	for (i = 0; (option = rte_enum_option(codec, i)); i++)
		if (strcmp(option->keyword, keyword) == 0)
			break;

	if (!option
	    || option->entries == 0
	    || !rte_get_option(codec, keyword, &val))
		return 0;

	for (r = i = 0; !r && i < option->entries; i++) {
		switch (option->type) {
		case RTE_OPTION_BOOL:
		case RTE_OPTION_INT:
			if (!option->menu.num)
				return 0;
			r = (option->menu.num[i] == val.num);
			break;
		case RTE_OPTION_REAL:
			if (!option->menu.dbl)
				return 0;
			r = (option->menu.dbl[i] == val.dbl);
			break;
		case RTE_OPTION_STRING:
			if (!option->menu.str)
				return 0;
			r = (strcmp(option->menu.str[i], val.str) == 0);
			break;
		default:
			break;
		}
	}

	if (r)
		*entry = i;

	if (option->type == RTE_OPTION_STRING)
		free(val.str);

	return r;
}

int
rte_set_option_menu(rte_codec *codec, char *keyword, int entry)
{
	rte_option *option;
	int i;

	nullcheck(codec, return 0);
	nullcheck(keyword, return 0);

	for (i = 0; (option = rte_enum_option(codec, i)); i++)
		if (strcmp(option->keyword, keyword) == 0)
			break;
	if (!option || entry < 0 || entry >= option->entries)
		return 0;

	switch (option->type) {
	case RTE_OPTION_BOOL:
	case RTE_OPTION_INT:
		if (!option->menu.num)
			return 0;
		return rte_set_option(codec, keyword,
				      option->menu.num[entry]);
	case RTE_OPTION_REAL:
		if (!option->menu.dbl)
			return 0;
		return rte_set_option(codec, keyword,
				      option->menu.dbl[entry]);
	case RTE_OPTION_STRING:
		if (!option->menu.str)
			return 0;
		return rte_set_option(codec, keyword,
				      option->menu.str[entry]);
	default:
		assert(!"reached");
		return 0;
	}
}

char *
rte_print_option(rte_codec *codec, char *keyword, ...)
{
	rte_context *context = NULL;
	va_list args;
	char *r;

	nullcheck(codec, return 0);
	nullcheck((context = codec->context), return 0);

	if (!BACKEND->print_option)
		return 0;

	va_start(args, keyword);

	r = BACKEND->print_option(codec, keyword, args);

	va_end(args);

	return r;
}
