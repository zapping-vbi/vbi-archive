/*
 *  Real Time Encoder lib
 *
 *  Copyright (C) 2000 Iñaki García Etxebarria
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

/* $Id: rte.c,v 1.45 2001-03-19 21:45:03 garetxe Exp $ */
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include "options.h"
#include "common/fifo.h"
#include "common/mmx.h"
#include "common/math.h"
#include "rtepriv.h"
#include "convert_ry.h"
#include "video/video.h"
#include "audio/libaudio.h"
#include "audio/mpeg.h"
#include "video/mpeg.h"
#include "systems/systems.h"
#include "common/profile.h"
#include "common/math.h"
#include "common/log.h"
#include "common/mmx.h"
#include "common/bstream.h"
#include "common/remote.h"
#include "main.h"

/*
  BUGS:
      . It isn't reentrant.
      . We shouldn't use ASSERTS if possible
      . When doing start/stop many times, the mux thread cannot be
      joined sometimes, why?
  TODO:
      . Add VBI support
      . Add preview support.
      . Drop the lib dependencies to a minimum.
*/

#define NUM_AUDIO_BUFFERS 8 /* audio buffers in the audio fifo */

rte_context * rte_global_context = NULL;

/*
 * Global options from rte.
 */
char *			my_name="rte";
int			verbose=0;

fifo *			audio_cap_fifo;
int			stereo;

fifo *			video_cap_fifo;

/* fixme: This is just to satisfy dependencies for now */
void
packed_preview(unsigned char *buffer, int mb_cols, int mb_rows)
{
}

/* prototypes for main initialization (mp1e startup) */
/* These routines are called in this order, they come from mp1e's main.c */
static void rte_audio_startup(void); /* Startup video parameters */
static void rte_compression_startup(void); /* Compression parameters */
/* init routines */
static void rte_audio_init(void); /* init audio capture */
static void rte_video_init(void); /* init video capture */

/*
 * Waits for video or audio data. Returns a buffer valid for
 * encoding. NULL on error.
 */
static inline buffer *
wait_data(rte_context * context, int video)
{
	fifo * f;
	buffer * b;
	mucon *consumer; list *full; coninfo *koninfo;
	rteDataCallback * data_callback;
	rteBufferCallback * buffer_callback;
	rte_buffer rbuf;
	enum rte_mux_mode stream = video ? RTE_VIDEO : RTE_AUDIO;
	
	nullcheck(context, return NULL);

	if (video) {
		f = &context->private->vid;
		data_callback = &context->private->video_data_callback;
		buffer_callback = &context->private->video_buffer_callback;
	}
	else {
		f = &context->private->aud;
		data_callback = &context->private->audio_data_callback;
		buffer_callback = &context->private->audio_buffer_callback;
	}

	pthread_rwlock_rdlock(&f->consumers_rwlock);
	koninfo = query_consumer(f);
	full = &koninfo->full;
	consumer = &koninfo->consumer;

	/* do we have an available buffer from the push interface? */
	pthread_mutex_lock(&consumer->mutex);

	while (!(b = (buffer*) rem_head(full))) {
		if (*data_callback) { /* no, callback
					 interface */
			b = wait_empty_buffer(f);
			
			ASSERT("size checks",
			       b->size == (video ? context->video_bytes :
					    context->audio_bytes));

			(*data_callback)(context, b->data, &(b->time),
					 stream, context->private->user_data);
			
			pthread_mutex_unlock(&consumer->mutex);
			pthread_rwlock_unlock(&f->consumers_rwlock);
			return b;
		} else if (*buffer_callback) {
			b = wait_empty_buffer(f);

			(*buffer_callback)(context, &rbuf, stream);
			b->data = rbuf.data;
			b->time = rbuf.time;
			b->user_data = rbuf.user_data;

			pthread_mutex_unlock(&consumer->mutex);
			pthread_rwlock_unlock(&f->consumers_rwlock);
			return b;
		}
		/* wait for the push interface */
		pthread_cond_wait(&consumer->cond, &consumer->mutex);
	}

	pthread_mutex_unlock(&consumer->mutex);
	pthread_rwlock_unlock(&f->consumers_rwlock);
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

	if (unref_callback) {
		rbuf.data = b->data;
		rbuf.time = b->time;
		rbuf.user_data = b->user_data;
		
		unref_callback((rte_context*)f->user_data, &rbuf);
	}

	pthread_mutex_lock(&f->producer.mutex);

	add_head(&f->empty, &b->node);

	pthread_mutex_unlock(&f->producer.mutex);
	pthread_cond_broadcast(&f->producer.cond);
}

static void
audio_send_empty(fifo *f, buffer *b)
{
	rte_buffer rbuf;
	rteUnrefCallback unref_callback =
		((rte_context*)f->user_data)->private->audio_unref_callback;

	if (unref_callback) {
		rbuf.data = b->data;
		rbuf.time = b->time;
		rbuf.user_data = b->user_data;

		unref_callback((rte_context*)f->user_data, &rbuf);
	}

	pthread_mutex_lock(&f->producer.mutex);

	add_head(&f->empty, &b->node);

	pthread_mutex_unlock(&f->producer.mutex);
	pthread_cond_broadcast(&f->producer.cond);
}

/* Do nothing callback */
static void
dead_end_callback(rte_context * context, void *data, double *time,
		  enum rte_mux_mode stream, void * user_data)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	*time = tv.tv_sec + tv.tv_usec/1e6;

	/* done */
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

rte_context * rte_context_new (int width, int height,
			       enum rte_pixformat frame_format,
			       void * user_data)
{
	rte_context * context;

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
	context->private = malloc(sizeof(rte_context_private));
	if (!context->private)
	{
		rte_error(NULL, "malloc(): [%d] %s", errno, strerror(errno));
		free(context);
		return NULL;
	}
	memset(context->private, 0, sizeof(rte_context_private));
	rte_global_context = context;

	context->mode = RTE_AUDIO | RTE_VIDEO;

	if (!rte_set_video_parameters(context, frame_format, width,
				      height, RTE_RATE_3, 2.3e6)) {
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
		if (context->mode & RTE_VIDEO) {
			uninit_fifo(&context->private->vid);
			mucon_destroy(&context->private->vid_consumer);
		}
		if (context->mode & RTE_AUDIO) {
			uninit_fifo(&context->private->aud);
			mucon_destroy(&context->private->aud_consumer);
		}
	}

	if (context->private->rgbmem) {
		free(context->private->rgbmem);
		context->private->rgbmem = NULL;
	}

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
		     const char * filename)
{
	nullcheck(context, return);

	context->private->encode_callback = encode_callback;

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


#define DECIMATING(mode) (mode == RTE_YUYV_VERTICAL_DECIMATION ||	\
			  mode == RTE_YUYV_EXP_VERTICAL_DECIMATION)

int rte_set_video_parameters (rte_context * context,
			      enum rte_pixformat frame_format,
			      int width, int height,
			      enum rte_frame_rate video_rate,
			      size_t output_video_bits)
{
	int check_malloc = 0;

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

	context->video_format = frame_format;
	context->width = MIN(width, MAX_WIDTH);
	context->height = MIN(height, MAX_HEIGHT);
	if (DECIMATING(context->video_format))
		context->height *= 2;
	context->video_rate = video_rate;
	context->output_video_bits = output_video_bits;
	context->video_bytes = context->width * context->height;
	context->private->rgbfilter = NULL;
	if (context->private->rgbmem)
	{
		free(context->private->rgbmem);
		context->private->rgbmem = NULL;
	}

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
		context->video_bytes *= 1.5;
		break;
	case RTE_RGB555:
		context->video_bytes *= 2;
		context->private->rgbfilter = convert_rgb555_ycbcr420;
		context->private->rgbmem =
			malloc(context->width*context->height*2);
		check_malloc = 1;
		break;
	case RTE_RGB565:
		context->video_bytes *= 2;
		context->private->rgbfilter = convert_rgb565_ycbcr420;
		context->private->rgbmem =
			malloc(context->width*context->height*2);
		check_malloc = 1;
		break;
	case RTE_RGB24:
		context->video_bytes *= 3;
		context->private->rgbfilter = convert_rgb24_ycbcr420;
		context->private->rgbmem =
			malloc(context->width*context->height*3);
		check_malloc = 1;
		break;
	case RTE_BGR24:
		context->video_bytes *= 3;
		context->private->rgbfilter = convert_bgr24_ycbcr420;
		context->private->rgbmem =
			malloc(context->width*context->height*3);
		check_malloc = 1;
		break;
	case RTE_RGB32:
		context->video_bytes *= 4;
		context->private->rgbfilter = convert_rgb32_ycbcr420;
		context->private->rgbmem =
			malloc(context->width*context->height*4);
		check_malloc = 1;
		break;
	case RTE_BGR32:
		context->video_bytes *= 4;
		context->private->rgbfilter = convert_bgr32_ycbcr420;
		context->private->rgbmem =
			malloc(context->width*context->height*4);
		check_malloc = 1;
		break;
	default:
		rte_error(context, "unhandled pixformat: %d", frame_format);
		return 0;
	}

	if ((check_malloc) && (!context->private->rgbmem)) {
		rte_error(context, "malloc: %d (%s)", errno,
			  strerror(errno));
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
	int stereo;

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

	switch (context->audio_mode) {
	case RTE_AUDIO_MODE_MONO:
		stereo = 0;
		break;
	case RTE_AUDIO_MODE_STEREO:
		stereo = 1;
		break;
		/* fixme:dual channel */
	default:
		stereo = 0;
		break;
	}

	context->audio_bytes = 2 * (stereo+1) * 1632;

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

/* translates the context options to the global mp1e options */
static int rte_fake_options(rte_context * context);

/* Prepare the context for encoding */
int rte_init_context ( rte_context * context )
{
	int alloc_bytes; /* bytes allocated for the video fifo */

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
	/* FIXME: we need some checks here for callbacks */
	if (!rte_fake_options(context))
		return 0;

	/* Now init the mp1e engine, as main would do */
	rte_audio_startup();
	rte_compression_startup();

	rte_audio_init();
	rte_video_init();

	if (!output_init())
		return 0;

	if (context->file_name) {
		context->private->fd = creat(context->file_name, 00644);
		if (context->private->fd == -1) {
			rte_error(context, "creat(%s): %s [%d]",
				  context->file_name, strerror(errno), errno);
			return 0;
		}

		context->private->encode_callback =
			RTE_ENCODE_CALLBACK(default_write_callback);
	}

	if (context->mode & RTE_AUDIO)
		mucon_init(&(context->private->aud_consumer));
	if (context->mode & RTE_VIDEO)
		mucon_init(&(context->private->vid_consumer));

	/* create needed fifos */
	if (context->mode & RTE_VIDEO) {
		alloc_bytes = context->video_bytes;
		switch (context->video_format)
		{
		case RTE_RGB555:
		case RTE_RGB565:
		case RTE_RGB24:
		case RTE_BGR24:
		case RTE_RGB32:
		case RTE_BGR32:
			/* fixme: add support for this */
			if (context->private->video_buffered) {
				rte_error(context, "buffering isn't"
					  "supported in RGB modes");
				return 0;
			}
		case RTE_YUV420:
			alloc_bytes = context->width*context->height*1.5;
			break;
		case RTE_YUYV_VERTICAL_DECIMATION:
		case RTE_YUYV_TEMPORAL_INTERPOLATION:
		case RTE_YUYV_VERTICAL_INTERPOLATION:
		case RTE_YUYV_PROGRESSIVE:
		case RTE_YUYV_PROGRESSIVE_TEMPORAL:
		case RTE_YUYV_EXP:
		case RTE_YUYV_EXP_VERTICAL_DECIMATION:
		case RTE_YUYV_EXP2:
		case RTE_YUYV:
			alloc_bytes = context->video_bytes;
			break;
		}

		if (context->private->video_buffered)
			alloc_bytes = 0; /* no need for preallocated
					    mem */
		if (2 > init_buffered_fifo(&(context->private->vid), "rte-video", NULL,
					   /*&(context->private->vid_consumer)*/
				       video_look_ahead(gop_sequence),
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
		if (4 > init_buffered_fifo(&(context->private->aud), "rte-audio",
					   NULL, /*&(context->private->aud_consumer)*/
				   NUM_AUDIO_BUFFERS, alloc_bytes)) {
			uninit_fifo(&(context->private->vid));
			rte_error(context, "not enough mem");
			return 0;
		}

		context->private->aud.user_data = context;
		context->private->aud.wait_full = audio_wait_full;
		context->private->aud.send_empty = audio_send_empty;
	}

	/* allow pushing from now */
	context->private->inited = 1;

	/* Set up some fields */
	context->private->last_video_buffer =
		context->private->last_audio_buffer = NULL;

	return 1; /* done, we are prepared for encoding */
}

int rte_start_encoding (rte_context * context)
{
	nullcheck(context, return 0);

	if (!context->private->inited) {
		rte_error(context, "you need to init the context first");
		return 0;
	}

	remote_init(modules); /* FIXME: remote_shutdown needed */

	if (modules & MOD_AUDIO) {
		ASSERT("create audio compression thread",
			!pthread_create(&context->private->audio_thread_id,
					NULL,
					stereo ? mpeg_audio_layer_ii_stereo :
					mpeg_audio_layer_ii_mono,
					NULL));

		printv(2, "Audio compression thread launched\n");
	}

	if (modules & MOD_VIDEO) {
		ASSERT("create video compression thread",
			!pthread_create(&context->private->video_thread_id,
					NULL,
					mpeg1_video_ipb,
					NULL));

		printv(2, "Video compression thread launched\n");
	}

	if ((modules == MOD_VIDEO || modules == MOD_AUDIO)
		&& mux_syn >= 2)
		mux_syn = 1; // compatibility

	switch (mux_syn) {
	case 0:
		ASSERT("create stream nirvana thread",
		       !pthread_create(&context->private->mux_thread, NULL,
				       stream_sink, NULL));
		break;
	case 1:
		ASSERT("create elementary stream thread",
		       !pthread_create(&context->private->mux_thread, NULL,
				       elementary_stream_bypass, NULL));
		break;
	case 2:
		printv(1, "MPEG-1 Program Stream\n");
		ASSERT("create mpeg1 system mux",
		       !pthread_create(&context->private->mux_thread, NULL,
				       mpeg1_system_mux, NULL));
		break;
	case 3:
		printv(1, "MPEG-2 Program Stream\n");
		ASSERT("create mpeg2 system mux",
		       !pthread_create(&context->private->mux_thread, NULL,
				       mpeg2_program_stream_mux, NULL));
		break;
	}

	remote_start(0.0); // now

	context->private->encoding = 1;

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
	context->private->inited = 0;

	/* save for future use */
	audio_callback = context->private->audio_data_callback;
	video_callback = context->private->video_data_callback;

	if ((context->mode & RTE_AUDIO) &&
	    (context->private->audio_interface == RTE_PUSH)) {
		/* set to dead end */
		context->private->audio_data_callback = dead_end_callback;

		/* Signal the conditions so any thread waiting for an
		   video push() wakes up, and uses the dead end */
		pthread_cond_broadcast(&(context->private->aud_consumer.cond));
	}

	if ((context->mode & RTE_VIDEO) &&
	    (context->private->video_interface == RTE_PUSH)) {
		/* set to dead end */
		context->private->video_data_callback = dead_end_callback;

		/* Signal the conditions so any thread waiting for an
		   video push() wakes up, and uses the dead end */
		pthread_cond_broadcast(&(context->private->vid_consumer.cond));
	}

	/* Tell the mp1e threads to shut down */
	remote_stop(0.0); // now

	/* Join the mux thread */
	printv(2, "joining mux\n");
	pthread_join(context->private->mux_thread, NULL);
	printv(2, "mux joined\n");

	if (context->mode & RTE_AUDIO) {
		printv(2, "joining audio\n");
		pthread_cancel(context->private->audio_thread_id);
		pthread_join(context->private->audio_thread_id, NULL);
		printv(2, "audio joined\n");
	}
	if (context->mode & RTE_VIDEO) {
		printv(2, "joining video\n");
		pthread_cancel(context->private->video_thread_id);
		pthread_join(context->private->video_thread_id, NULL);
		printv(2, "video joined\n");
	}

	mux_cleanup();
	output_end();

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
//	pr_report();

	/* mp1e is done (fixme: close preview if needed) */

	if (context->private->fd > 0) {
		close(context->private->fd);
		context->private->fd = -1;
	}
	if (context->private->rgbmem) {
		free(context->private->rgbmem);
		context->private->rgbmem = NULL;
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
	ASSERT("Arrr... stick to the usage, please\n",
	       (context->private->last_video_buffer && data)
	       || (!context->private->last_video_buffer && !data));

	if (data) {
		if (!context->private->rgbfilter)
			ASSERT("you haven't written to the provided buffer!\n",
			       data ==
			       context->private->last_video_buffer->data);
		else {
			ASSERT("you haven't written to the provided buffer!\n",
			       data == context->private->rgbmem);
			context->private->rgbfilter(data,
				 context->private->last_video_buffer->data,
						    context->width,
						    context->height);
		}
			
		context->private->last_video_buffer->time = time;

		send_full_buffer(&(context->private->vid),
				 context->private->last_video_buffer);
	}

	context->private->last_video_buffer =
		wait_empty_buffer(&(context->private->vid));

	if (context->private->rgbfilter)
		return context->private->rgbmem;

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

	b = context->private->last_video_buffer =
		wait_empty_buffer(&(context->private->vid));

	b->time = rbuf->time;
	b->data = rbuf->data;
	b->user_data = rbuf->user_data;

	send_full_buffer(&(context->private->vid),
			 context->private->last_video_buffer);
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

	ASSERT("Arrr... stick to the usage, please\n",
	       (context->private->last_audio_buffer && data)
	       || (!context->private->last_audio_buffer && !data));

	if (data) {
		ASSERT("you haven't written to the provided buffer!\n",
		       data == context->private->last_audio_buffer->data);
			
		context->private->last_audio_buffer->time = time;

		send_full_buffer(&(context->private->aud),
				 context->private->last_audio_buffer);
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

	b = context->private->last_audio_buffer =
		wait_empty_buffer(&(context->private->aud));

	b->time = rbuf->time;
	b->data = rbuf->data;
	b->user_data = rbuf->user_data;

	send_full_buffer(&(context->private->aud),
			 context->private->last_audio_buffer);
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
	if (!cpu_detection())
		return 0;

	rte_global_context = NULL;

	return 1;
}

static int rte_fake_options(rte_context * context)
{
	int pitch;

	ASSERT("guiroppaaaaa!\n", context != NULL);

	modules = context->mode;
	grab_width = saturate(context->width, 1, MAX_WIDTH);
	grab_height = saturate(context->height, 1, MAX_HEIGHT);
	if ((context->video_rate == RTE_RATE_NORATE) ||
	    (context->video_rate > RTE_RATE_8)) {
		rte_error(context, "Invalid frame rate: %d",
			  context->video_rate);
		context->video_rate = RTE_RATE_3; /* default to PAL */
	}
	frame_rate_code = context->video_rate;
	switch (context->video_format) {
	case RTE_YUYV_PROGRESSIVE:
	case RTE_YUYV_PROGRESSIVE_TEMPORAL:
		frame_rate_code += 3;
	case RTE_YUYV_VERTICAL_DECIMATION:
	case RTE_YUYV_TEMPORAL_INTERPOLATION:
	case RTE_YUYV_VERTICAL_INTERPOLATION:
	case RTE_YUYV_EXP:
	case RTE_YUYV_EXP_VERTICAL_DECIMATION:
	case RTE_YUYV_EXP2:
		pitch = grab_width*2;
		filter_mode =
			(context->video_format-RTE_YUYV_VERTICAL_DECIMATION) + 
			CM_YUYV_VERTICAL_DECIMATION;
		break;
	case RTE_YUYV:
		filter_mode = CM_YUYV;
		pitch = grab_width*2;
		break;
	case RTE_YUV420:
		/* the RGB modes get converted to this mode too */
	case RTE_RGB555:
	case RTE_RGB565:
	case RTE_RGB24:
	case RTE_BGR24:
	case RTE_RGB32:
	case RTE_BGR32:
		filter_mode = CM_YUV;
		pitch = grab_width;
		break;
	default:
		rte_error(context, "unknown pixformat %d", context->video_format);
		return 0;
	}
	width = context->width;
	height = context->height;

	filter_init(pitch);
	if ((width != grab_width) || (height != grab_height))
		rte_error(NULL, "requested %dx%d, got %dx%d",
			  grab_width, grab_height, width, height);
	video_bit_rate = context->output_video_bits;
	audio_bit_rate = context->output_audio_bits;
	audio_bit_rate_stereo = audio_bit_rate * 2;

	/* fixme: is audio_rate really needed? */
	sampling_rate = context->audio_rate;
	switch (context->audio_mode) {
	case RTE_AUDIO_MODE_MONO:
		audio_mode = AUDIO_MODE_MONO;
		break;
	case RTE_AUDIO_MODE_STEREO:
		audio_mode = AUDIO_MODE_STEREO;
		break;
	default:
		rte_error(context, "unknown audio mode %d",
			  context->audio_mode);
		return 0;
	}

	video_cap_fifo = &(context->private->vid);
	audio_cap_fifo = &(context->private->aud);

	return 1;
}

/* Startup video parameters */
static void rte_audio_startup(void)
{
	if (modules & MOD_AUDIO) {
		int psy_level = audio_mode / 10;
		
		audio_mode %= 10;
		
		stereo = (audio_mode != AUDIO_MODE_MONO);
		audio_parameters(&sampling_rate, &audio_bit_rate);
		
		if ((audio_bit_rate >> stereo) < 80000 || psy_level >= 1) {
			psycho_loops = MAX(psycho_loops, 1);
			
			if (sampling_rate < 32000 || psy_level >= 2)
				psycho_loops = 2;
			
			psycho_loops = MAX(psycho_loops, 2);
		}
	}
}

/* FIXME: Subtitles support */

/* Compression parameters */
static void rte_compression_startup(void)
{
//	mucon_init(&mux_mucon);
}

static void rte_audio_init(void) /* init audio capture */
{
	if (modules & MOD_AUDIO) {
		long long n = llroundn(((double) video_num_frames /
					frame_rate_value[frame_rate_code])
				       / (1152.0 / sampling_rate));
		
		if (modules & MOD_VIDEO)
			audio_num_frames = MIN(n, (long long) INT_MAX);
		
		audio_init(sampling_rate, stereo, /* pcm_context* */
			audio_mode, audio_bit_rate, psycho_loops);
	}
}

static void rte_video_init(void) /* init video capture */
{
	if (modules & MOD_VIDEO) {
		video_coding_size(width, height);

		if (frame_rate > frame_rate_value[frame_rate_code])
			frame_rate = frame_rate_value[frame_rate_code];

		video_init();
	}
}
