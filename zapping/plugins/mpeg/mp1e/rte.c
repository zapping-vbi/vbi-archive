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

/* $Id: rte.c,v 1.59 2001-07-26 05:41:31 mschimek Exp $ */
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

#include "common/fifo.h" /* fifos */
#include "common/log.h" /* printv, verbose */
#include "common/math.h" /* MAX, MIN */
#include "video/video.h" /* video_look_ahead, MAX_WIDTH, MAX_HEIGHT */
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
      . Drop the lib dependencies to a minimum.
*/

#define NUM_AUDIO_BUFFERS 4 /* audio buffers in the audio fifo */

rte_context * rte_global_context = NULL;

extern rte_backend_info b_mp1e_info;
//extern rte_backend_info b_ffmpeg_info;

static rte_backend_info *backends[] = 
{
	&b_mp1e_info/*,
		      &b_ffmpeg_info*/
};

static const int num_backends = sizeof(backends)/sizeof(*backends);

/* This must be kept in sync with fifo.h */
static inline buffer *
rte_recv_full_buffer(fifo *f)
{
	buffer *b;
	coninfo *consumer;

	pthread_rwlock_rdlock(&f->consumers_rwlock);
	consumer = query_consumer(f);

	pthread_mutex_lock(&consumer->consumer.mutex);
	b = (buffer*) rem_head(&consumer->full);
	if (b)
		consumer->waiting--;
	pthread_mutex_unlock(&consumer->consumer.mutex);

	pthread_rwlock_unlock(&f->consumers_rwlock);

	return b;
}

/* Just produces a blank buffer */
static void
blank_callback(rte_context * context, void *data, double *time,
	       enum rte_mux_mode stream, void * user_data)
{
	struct timeval tv;

	*time = current_time();

	/* set to 0's (avoid ugly noise on stop) */
	if (stream == RTE_AUDIO)
		memset(data, 0, context->audio_bytes);
}

/*
 * Waits for video or audio data. Returns a buffer valid for
 * encoding. NULL on error.
 */
static inline buffer *
wait_data(rte_context * context, int video)
{
	fifo * f;
	buffer * b;
	/* *_callback are volatile, access by reference */
	rteDataCallback *data_callback;
	rteBufferCallback *buffer_callback;
	rte_buffer rbuf;
	enum rte_mux_mode stream = video ? RTE_VIDEO : RTE_AUDIO;
	mucon * consumer;

	nullcheck(context, return NULL);

	if (video) {
		f = &context->private->vid;
		data_callback = &(context->private->video_data_callback);
		buffer_callback = &(context->private->video_buffer_callback);
		consumer = &(context->private->vid_consumer);
	}
	else {
		f = &context->private->aud;
		data_callback = &(context->private->audio_data_callback);
		buffer_callback = &(context->private->audio_buffer_callback);
		consumer = &(context->private->aud_consumer);
	}

	while (!(b = rte_recv_full_buffer(f))) {
		if (*data_callback) {
			b = wait_empty_buffer(f);
			(*data_callback)(context, b->data, &(b->time),
					 stream, context->private->user_data);
			if (*data_callback == blank_callback)
				b->rte_flags |= BLANK_BUFFER;
			else
				b->rte_flags &= ~BLANK_BUFFER;
			send_full_buffer(f, b);
			continue;
		} else if (*buffer_callback) {
			(*buffer_callback)(context, &rbuf, stream);
			if (rbuf.data) {
				b = wait_empty_buffer(f);

				b->data = rbuf.data;
				b->time = rbuf.time;
				b->user_data = rbuf.user_data;
				b->rte_flags &= ~BLANK_BUFFER;

				send_full_buffer(f, b);
			}
			continue;
		}

		/* wait for the push interface */
		pthread_mutex_lock(&consumer->mutex);
		pthread_cond_wait(&consumer->cond, &consumer->mutex);
		pthread_mutex_unlock(&consumer->mutex);
	}

	return b;
}

static buffer*
video_wait_full(fifo *f)
{
	return wait_data((rte_context*)f->user_data, 1);
}

static buffer*
audio_wait_full(fifo *f)
{
	return wait_data((rte_context*)f->user_data, 0);
}

static void
video_send_empty(fifo *f, buffer *b)
{
	rte_buffer rbuf;
	rteUnrefCallback unref_callback =
		((rte_context*)f->user_data)->private->video_unref_callback;

	if (unref_callback && !(b->rte_flags & BLANK_BUFFER)) {
		rbuf.data = b->data;
		rbuf.time = b->time;
		rbuf.user_data = b->user_data;
		
		unref_callback((rte_context*)f->user_data, &rbuf);
	}

	pthread_mutex_lock(&f->producer.mutex);
		
	add_head(&f->empty, &b->node);
	
	b->refcount = 0;
		
	pthread_mutex_unlock(&f->producer.mutex);
	pthread_cond_broadcast(&f->producer.cond);
}

static void
audio_send_empty(fifo *f, buffer *b)
{
	rte_buffer rbuf;
	rteUnrefCallback unref_callback =
		((rte_context*)f->user_data)->private->audio_unref_callback;

	if (unref_callback && !(b->rte_flags & BLANK_BUFFER)) {
		rbuf.data = b->data;
		rbuf.time = b->time;
		rbuf.user_data = b->user_data;
		
		unref_callback((rte_context*)f->user_data, &rbuf);
	}

	pthread_mutex_lock(&f->producer.mutex);
		
	add_head(&f->empty, &b->node);
	
	b->refcount = 0;
		
	pthread_mutex_unlock(&f->producer.mutex);
	pthread_cond_broadcast(&f->producer.cond);
}

/* The default write data callback */
static void default_write_callback ( rte_context * context,
				     void * data, size_t size,
				     void * user_data )
{
	size_t r;

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
static void default_seek_callback ( rte_context * context,
				    off_t offset,
				    int whence,
				    void * user_data)
{
	lseek(context->private->fd, offset, whence);
}

rte_context * rte_context_new (int width, int height,
			       const char *backend,
			       void * user_data)
{
	rte_context * context;
	int priv_bytes=0, i;

	if (rte_global_context)
	{
		rte_error(NULL, "There is already a context");
		return NULL;
	}

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
	rte_global_context = context;

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

	if (context != rte_global_context)
	{
		rte_error(NULL, "The given context hasn't been created by rte");
		return NULL;
	}

	if (context->private->encoding)
		rte_stop(context);

	if (context->private->inited) {
		BACKEND->uninit_context(context);

		if (context->mode & RTE_VIDEO) {
			uninit_fifo(&context->private->vid);
			mucon_destroy(&context->private->vid_consumer);
		}
		if (context->mode & RTE_AUDIO) {
			uninit_fifo(&context->private->aud);
			mucon_destroy(&context->private->aud_consumer);
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

	rte_global_context = NULL;

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
		rte_error(context, "gop too long (1023 chars max):\n%s",
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
		if (context->private->video_buffered)
			alloc_bytes = 0; /* no need for preallocated
					    mem */
		else
			alloc_bytes = context->video_bytes;
		if (2 > init_buffered_fifo(&(context->private->vid),
				   "rte-video",
				   video_look_ahead(context->gop_sequence),
				   alloc_bytes)) {
			rte_error(context, "not enough mem");
			return 0;
		}

		context->private->vid.user_data = context;
		context->private->vid.wait_full = video_wait_full;
		context->private->vid.send_empty = video_send_empty;
	}

	if (context->mode & RTE_AUDIO) {
		if (!context->private->audio_buffered)
			alloc_bytes = context->audio_bytes;
		else
			alloc_bytes = 0;
		if (4 > init_buffered_fifo(&(context->private->aud),
					   "rte-audio",
					   NUM_AUDIO_BUFFERS,
					   alloc_bytes)) {
			if (context->mode & RTE_VIDEO)
				uninit_fifo(&(context->private->vid));
			rte_error(context, "not enough mem");
			return 0;
		}

		context->private->aud.user_data = context;
		context->private->aud.wait_full = audio_wait_full;
		context->private->aud.send_empty = audio_send_empty;
	}
	
	if (context->file_name) {
		context->private->fd = creat(context->file_name, 00644);
		if (context->private->fd == -1) {
			rte_error(context, "creat(%s): %s [%d]",
				  context->file_name, strerror(errno), errno);
			if (context->mode & RTE_VIDEO)
				uninit_fifo(&(context->private->vid));
			if (context->mode & RTE_AUDIO)
				uninit_fifo(&(context->private->aud));
			return 0;
		}

		context->private->encode_callback =
			RTE_ENCODE_CALLBACK(default_write_callback);
		context->private->seek_callback =
			RTE_SEEK_CALLBACK(default_seek_callback);
	}

	if (context->mode & RTE_AUDIO)
		mucon_init(&(context->private->aud_consumer));
	if (context->mode & RTE_VIDEO)
		mucon_init(&(context->private->vid_consumer));

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
		/* set to dead end */
		context->private->audio_data_callback = blank_callback;

		/* send the used buffer as empty */
		if (context->private->last_audio_buffer)
			send_empty_buffer(&(context->private->aud),
					  context->private->last_audio_buffer);

		/* Signal the conditions so any thread waiting for a
		   audio push() wakes up, and uses the dead end */
		pthread_cond_broadcast(&(context->private->aud_consumer.cond));
	}

	if ((context->mode & RTE_VIDEO) &&
	    (context->private->video_interface == RTE_PUSH)) {
		/* set to dead end */
		context->private->video_data_callback = blank_callback;

		if (context->private->last_video_buffer)
			send_empty_buffer(&(context->private->vid),
					  context->private->last_video_buffer);

		/* Signal the conditions so any thread waiting for a
		   video push() wakes up, and uses the dead end */
		pthread_cond_broadcast(&(context->private->vid_consumer.cond));
	}

	BACKEND->stop(context);
	BACKEND->uninit_context(context);

	context->private->inited = 0;

	if (context->mode & RTE_VIDEO) {
		uninit_fifo(&context->private->vid);
		mucon_destroy(&context->private->vid_consumer);
	}
	if (context->mode & RTE_AUDIO) {
		uninit_fifo(&context->private->aud);
		mucon_destroy(&context->private->aud_consumer);
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
		       data ==
		       context->private->last_video_buffer->data);
			
		context->private->last_video_buffer->time = time;
		ASSERT("size check", context->video_bytes ==
		       context->private->last_video_buffer->size);

		send_full_buffer(&(context->private->vid),
				 context->private->last_video_buffer);

		/* aknowledge to wait_data */
		pthread_cond_broadcast(&(context->private->vid_consumer.cond));
	}

	context->private->last_video_buffer =
		wait_empty_buffer(&(context->private->vid));

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

	b = wait_empty_buffer(&(context->private->vid));

	b->time = rbuf->time;
	b->data = rbuf->data;
	b->user_data = rbuf->user_data;

	send_full_buffer(&(context->private->vid), b);

	pthread_cond_broadcast(&(context->private->vid_consumer.cond));
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

		send_full_buffer(&(context->private->aud),
				 context->private->last_audio_buffer);
		pthread_cond_broadcast(&(context->private->aud_consumer.cond));
	}

	context->private->last_audio_buffer =
		wait_empty_buffer(&(context->private->aud));

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

	b = wait_empty_buffer(&(context->private->aud));

	b->time = rbuf->time;
	b->data = rbuf->data;
	b->user_data = rbuf->user_data;

	send_full_buffer(&(context->private->aud), b);

	pthread_cond_broadcast(&(context->private->aud_consumer.cond));
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

	rte_global_context = NULL;

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
