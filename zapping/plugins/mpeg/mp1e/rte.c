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
/* $Id: rte.c,v 1.26 2000-10-12 22:06:24 garetxe Exp $ */
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
#include "video/video.h" /* fixme: video_unget_frame and friends */
#include "audio/audio.h" /* fixme: audio_read prots. */
#include "audio/mpeg.h"
#include "video/mpeg.h"
#include "systems/systems.h"
#include "common/profile.h"
#include "common/math.h"
#include "common/log.h"
#include "common/mmx.h"
#include "common/bstream.h"
#include "main.h"

/*
  BUGS:
      . Callbacks + push doesn't work yet.
      . It isn't reentrant.
      . Plenty of unknown bugs
  TODO:
      . We should allow to use different interfaces for audio or video (easy)
*/

#define NUM_VIDEO_BUFFERS 8 /* video buffers in the video fifo */
#define NUM_AUDIO_BUFFERS 8 /* audio buffers in the audio fifo */

rte_context * rte_global_context = NULL;

/*
 * Global options from rte.
 */
char *			my_name="rte";
int			verbose=1;

double			video_stop_time = 1e30;
double			audio_stop_time = 1e30;
double			vbi_stop_time = 1e30;

fifo *			audio_cap_fifo;
int			stereo;

fifo *			video_cap_fifo;

int			min_cap_buffers = MIN(NUM_AUDIO_BUFFERS,
					      NUM_VIDEO_BUFFERS);

/* prototypes for main initialization (mp1e startup) */
/* fixme: preview support (whew, XV is really nice!) */
/* These routines are called in this order, they come from mp1e's main.c */
static void rte_audio_startup(void); /* Startup video parameters */
static void rte_video_startup(void); /* Startup audio parameters */
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
	mucon * consumer;
	rteDataCallback * data_callback;
	
	nullcheck(context, return NULL);

	if (video) {
		f = &context->private->vid;
		data_callback = &context->private->video_data_callback;
	}
	else {
		f = &context->private->aud;
		data_callback = &context->private->audio_data_callback;
	}

	consumer = f->consumer;

	/* do we have an available buffer from the push interface? */
	pthread_mutex_lock(&consumer->mutex);
	b = (buffer*) rem_head(&(f->full));
	
	if (b) {
		pthread_mutex_unlock(&consumer->mutex);
		return b; /* yep, return it */
	}

	while (!(b = (buffer*) rem_head(&f->full))) {
		if (*data_callback) { /* no, callback
					 interface */
			b = wait_empty_buffer(f);
			
			ASSERT("size checks",
			       b->_size == (video ? context->video_bytes :
					    context->audio_bytes));

			(*data_callback)(b->data, &(b->time), video, context,
					 context->private->user_data);
			
			pthread_mutex_unlock(&consumer->mutex);
			return b;
		}

		/* wait for the push interface */
		pthread_cond_wait(&consumer->cond, &consumer->mutex);
	}

	pthread_mutex_unlock(&consumer->mutex);

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

/* Do nothing callback */
static void
dead_end_callback(void *data, double *time, int video, rte_context *
		  context, void * user_data)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	*time = tv.tv_sec + tv.tv_usec/1e6;

	// done
}

/* The default write data callback */
static void default_write_callback ( void * data, size_t size,
				     rte_context * context,
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
			       enum rte_frame_rate rate,
			       char * file,
			       rteEncodeCallback encode_callback,
			       rteDataCallback audio_data_callback,
			       rteDataCallback video_data_callback,
			       void * user_data)
{
	rte_context * context;

	if (rte_global_context)
	{
		rte_error(NULL, "There is already a context");
		return NULL;
	}

	if ((file) && (encode_callback)) {
		rte_error(NULL,
			  "use either an encode callbacks or a filename");
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

	context->mode = RTE_MUX_VIDEO_AND_AUDIO;

	if (!rte_set_video_parameters(context, frame_format, width,
				      height, rate, 2.3e6)) {
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

	if (encode_callback)
		context->private->encode_callback = encode_callback;
	else {
		if (!file) {
			rte_error(NULL, "file == callback == NULL");
			return rte_context_destroy(context);
		}
		
		context->file_name = strdup(file);
	}

	context->private->audio_data_callback = audio_data_callback;
	context->private->video_data_callback = video_data_callback;

	context->private->user_data = user_data;
	context->private->fd = -1;

	return (context);
}

void * rte_context_destroy ( rte_context * context )
{
	nullcheck(context, return NULL);

	if (context != rte_global_context)
	{
		rte_error(NULL, "Sorry, the given context hasn't been created by rte");
		return NULL;
	}

	if (context->file_name)
		free(context->file_name);

	if (context->private->encoding)
		rte_stop(context);

	if (context->private->inited) {
		if (context->mode & RTE_MUX_VIDEO_ONLY) {
			uninit_fifo(&context->private->aud);
			mucon_destroy(&context->private->vid_consumer);
		}
		if (context->mode & RTE_MUX_AUDIO_ONLY) {
			uninit_fifo(&context->private->vid);
			mucon_destroy(&context->private->aud_consumer);
		}
	}

	if (context->private->rgbmem) {
		free(context->private->rgbmem);
		context->private->rgbmem = NULL;
	}

	if (context->error)
		free(context->error);

	free(context->private);
	free(context);

	rte_global_context = NULL;

	return NULL;
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

	if (!(context->mode & RTE_MUX_VIDEO_ONLY)) {
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
		free(context->private->rgbmem);

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

	if (!(context->mode & RTE_MUX_AUDIO_ONLY)) {
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

void rte_set_data_callbacks (rte_context * context,
			     rteDataCallback audio_callback,
			     rteDataCallback video_callback)
{
	nullcheck(context, return);

	if (context->private->inited)
	{
		rte_error(context, "context already inited");
		return;
	}

	context->private->audio_data_callback = audio_callback;
	context->private->video_data_callback = video_callback;
}

void rte_get_data_callbacks (rte_context * context,
			     rteDataCallback * audio_callback,
			     rteDataCallback * video_callback)
{
	nullcheck(context, return);

	if (audio_callback)
		*audio_callback = context->private->audio_data_callback;
	if (video_callback)
		*video_callback = context->private->video_data_callback;
}

void rte_set_encode_callback (rte_context * context,
			      rteEncodeCallback callback)
{
	nullcheck(context, return);

	if (context->private->inited)
	{
		rte_error(context, "context already inited");
		return;
	}

	context->private->encode_callback = callback;
}

rteEncodeCallback rte_get_encode_callback (rte_context * context)
{
	if (!context)
	{
		rte_error(NULL, "context == NULL");
		return NULL;
	}
	return (context->private->encode_callback);
}

void rte_set_file_name(rte_context * context, const char * file_name)
{
	nullcheck(context, return);

	if (context->private->inited)
	{
		rte_error(context, "context already inited");
		return;
	}

	if (context->file_name)
		free(context->file_name);

	if (file_name)
		context->file_name = strdup(file_name);
	else
		context->file_name = NULL;
}

char * rte_get_file_name(rte_context * context)
{
	nullcheck(context, return NULL);

	return context->file_name;
}

void rte_set_user_data(rte_context * context, void * user_data)
{
	nullcheck(context, return);

	/* the user data isn't fixed evem when inited */

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
	if ((context->private->encode_callback) && (context->file_name)) {
		rte_error(context,
			  "Use either an encode callback or a file name, not both");
		return 0;
	}
	if ((!context->private->encode_callback) && (!context->file_name)) {
		rte_error(context, "You need to specify an encode callback or a filename");
		return 0;
	}

	if (!rte_fake_options(context))
		return 0;

	/* Now init the mp1e engine, as main would do */
	rte_audio_startup();
	rte_video_startup();
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

	if (context->mode & RTE_MUX_AUDIO_ONLY)
		mucon_init(&(context->private->aud_consumer));
	if (context->mode & RTE_MUX_VIDEO_ONLY)
		mucon_init(&(context->private->vid_consumer));

	/* create needed fifos */
	if (context->mode & RTE_MUX_VIDEO_ONLY) {
		alloc_bytes = context->video_bytes;
		switch (context->video_format)
		{
		case RTE_RGB555:
		case RTE_RGB565:
		case RTE_RGB24:
		case RTE_BGR24:
		case RTE_RGB32:
		case RTE_BGR32:
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

		if (4 > init_buffered_fifo(&(context->private->vid),
			&(context->private->vid_consumer), alloc_bytes,
					   NUM_VIDEO_BUFFERS)) {
			rte_error(context, "not enough mem");
			return 0;
		}

		context->private->vid.user_data = context;
		context->private->vid.wait_full = video_wait_full;
	}

	if (context->mode & RTE_MUX_AUDIO_ONLY) {
		if (4 > init_buffered_fifo(&(context->private->aud),
		      &(context->private->aud_consumer), context->audio_bytes,
					   NUM_AUDIO_BUFFERS)) {
			uninit_fifo(&(context->private->vid));
			rte_error(context, "not enough mem");
			return 0;
		}

		context->private->aud.user_data = context;
		context->private->aud.wait_full = audio_wait_full;
	}

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

	/* sync threads */
	if (popcnt(modules) > 1)
		synchronize_capture_modules();

	if (modules & MOD_AUDIO) {
		ASSERT("create audio compression thread",
			!pthread_create(&context->private->audio_thread_id,
					NULL,
			stereo ? mpeg_audio_layer_ii_stereo :
				 mpeg_audio_layer_ii_mono, NULL));

		printv(2, "Audio compression thread launched\n");
	}

	if (modules & MOD_VIDEO) {
		ASSERT("create video compression thread",
			!pthread_create(&context->private->video_thread_id,
					NULL,
				mpeg1_video_ipb, NULL));

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

	context->private->encoding = 1;

	return 1;
}

void rte_stop ( rte_context * context )
{
	struct timeval tv;
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

	/* set to dead ends */
	context->private->audio_data_callback = dead_end_callback;
	context->private->video_data_callback = dead_end_callback;

	/* Signal the conditions so any thread waiting for an
	   audio/video push() wake up, and use the dead end */
	if (context->mode & RTE_MUX_AUDIO_ONLY)
		pthread_cond_broadcast(&(context->private->aud_consumer.cond));
	if (context->mode & RTE_MUX_VIDEO_ONLY)
		pthread_cond_broadcast(&(context->private->vid_consumer.cond));

	/* Tell the mp1e threads to shut down */
	gettimeofday(&tv, NULL);

	video_stop_time =
	vbi_stop_time =
	audio_stop_time = tv.tv_sec + tv.tv_usec / 1e6;

	/* Join the mux thread */
	pthread_join(context->private->mux_thread, NULL);

	mux_cleanup();

	output_end();

	/* restore callbacks, as if nothing had happened */
	context->private->audio_data_callback = audio_callback;
	context->private->video_data_callback = video_callback;

	if (context->mode & RTE_MUX_VIDEO_ONLY) {
		uninit_fifo(&context->private->aud);
		mucon_destroy(&context->private->vid_consumer);
	}
	if (context->mode & RTE_MUX_AUDIO_ONLY) {
		uninit_fifo(&context->private->vid);
		mucon_destroy(&context->private->aud_consumer);
	}

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
	if (!(context->mode & RTE_MUX_VIDEO_ONLY)) {
		rte_error(context, "Mux isn't prepared to encode video!");
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
	if (!(context->mode & RTE_MUX_AUDIO_ONLY)) {
		rte_error(context, "Mux isn't prepared to encode audio!");
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

int rte_init ( void )
{
	if (!cpu_id(ARCH_PENTIUM_MMX))
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

/* Startup audio parameters */
static void rte_video_startup(void)
{
	if (modules & MOD_VIDEO) {
		char *s = gop_sequence;
		int count = 0;
		
		min_cap_buffers = 0;
		
		do {
			if (*s == 'B')
				count++;
			else {
				if (count > min_cap_buffers)
					min_cap_buffers = count;
					count = 0;
			}
		} while (*s++);
		
		min_cap_buffers++;
	}
}

/* FIXME: Subtitles support (when it gets into the bttv 2 driver?) */

/* Compression parameters */
static void rte_compression_startup(void)
{
	mucon_init(&mux_mucon);
}

static void rte_audio_init(void) /* init audio capture */
{
	if (modules & MOD_AUDIO) {
		long long n = llroundn(((double) video_num_frames /
					frame_rate_value[frame_rate_code])
				       / (1152.0 / sampling_rate));
		
		if (modules & MOD_VIDEO)
			audio_num_frames = MIN(n, (long long) INT_MAX);
		
		audio_init();
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
