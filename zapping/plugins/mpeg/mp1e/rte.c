/*
 *  MPEG-1 Real Time Encoder lib wrapper api
 *
 *  Copyright (C) 2000 I�aki Garc�a Etxebarria
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
*/

rte_context * rte_global_context = NULL;
pthread_t mux_thread; /* Multiplexer thread */ 

#define RC(X)  ((rte_context*)X)

/* prototypes for main initialization (mp1e core startup) */
/* These routines are called in this order, they come from mp1e's main.c */
static void rte_audio_startup(void); /* Startup video parameters */
static void rte_video_startup(void); /* Startup audio parameters */
static void rte_compression_startup(void); /* Compression parameters */

#define rte_error(context, format, args...) \
{ \
	if (context) { \
		if (!RC(context)->error) \
			RC(context)->error = malloc(256); \
		RC(context)->error[255] = 0; \
		snprintf(RC(context)->error, 255, \
			 "rte:%s(%d): " format, \
			 __PRETTY_FUNCTION__, __LINE__ ,##args); \
	} \
	else \
		fprintf(stderr, "rte:%s(%d): " format ".\n", \
			__PRETTY_FUNCTION__, __LINE__ ,##args); \
}

static inline void
fetch_data(rte_context * context, int video)
{
	_buffer * buf;
	_fifo * f;
	
	if (!context->private->data_callback)
		return;

	if (video)
		f = &(context->private->vid);
	else
		f = &(context->private->aud);

	buf = _new_buffer(f);

	context->private->data_callback(buf->data, &(buf->time), video,
					context, context->private->user_data);
	_send_out_buffer(f, buf);
}

/* Tells the video fetcher thread to fetch a new frame */
static inline void
schedule_video_fetch(rte_context * context)
{
	if (!context->private->data_callback)
		return;

	if (context->private->video_pending < 5) {
		pthread_mutex_lock(&(context->private->video_mutex));
		context->private->video_pending ++;
		if (context->private->video_pending == 1)
			context->private->video_pending ++;
		pthread_cond_broadcast(&(context->private->video_cond));
		pthread_mutex_unlock(&(context->private->video_mutex));
	}
}

static inline void
schedule_audio_fetch(rte_context * context)
{
	if (!context->private->data_callback)
		return;

	if (context->private->audio_pending < 5) {
		pthread_mutex_lock(&(context->private->audio_mutex));
		context->private->audio_pending ++;
		if (context->private->audio_pending == 1)
			context->private->audio_pending ++;
		pthread_cond_broadcast(&(context->private->audio_cond));
		pthread_mutex_unlock(&(context->private->audio_mutex));
	}
}

static void *
video_input_thread ( void * ptr )
{
	rte_context * context = ptr;
	rte_context_private * priv = context->private;

	pthread_mutex_lock(&(priv->video_mutex));

	while (priv->encoding) {
		while (priv->video_pending == 0)
			pthread_cond_wait(&(priv->video_cond),
					  &(priv->video_mutex));

		/* < 0 means exit thread */
		while (priv->video_pending > 0) {
			pthread_mutex_unlock(&(priv->video_mutex));
			fetch_data(context, 1);
			pthread_mutex_lock(&(priv->video_mutex));
			priv->video_pending --;
			pthread_cond_broadcast(&(priv->video_cond));
		}

	}

	pthread_mutex_unlock(&(priv->video_mutex));

	return NULL;
}

static void *
audio_input_thread ( void * ptr )
{
	rte_context * context = ptr;
	rte_context_private * priv = context->private;

	pthread_mutex_lock(&(priv->audio_mutex));

	while (priv->encoding) {
		while (priv->audio_pending == 0)
			pthread_cond_wait(&(priv->audio_cond),
					  &(priv->audio_mutex));

		/* < 0 means exit thread */
		while (priv->audio_pending > 0) {
			pthread_mutex_unlock(&(priv->audio_mutex));
			fetch_data(context, 0);
			pthread_mutex_lock(&(priv->audio_mutex));
			priv->audio_pending --;
			pthread_cond_broadcast(&(priv->audio_cond));
		}
	}

	pthread_mutex_unlock(&(priv->audio_mutex));

	return NULL;
}

/* The default write data callback */
static void default_write_callback ( void * data, size_t size,
				     rte_context * context,
				     void * user_data )
{
	int r;

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

rte_context * rte_context_new (char * file,
			       int width, int height,
			       enum rte_frame_rate rate,
			       rteEncodeCallback encode_callback,
			       rteDataCallback data_callback,
			       void * user_data)
{
	rte_context * context;
	int stereo;

	if (rte_global_context)
	{
		rte_error(NULL, "There is already a context");
		return NULL;
	}

	if ((file) && (encode_callback)) {
		rte_error(NULL, "You need to specify file OR encode_callback");
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
	if (rate == RTE_RATE_NORATE)
	{
		free(context->private);
		free(context);
		rte_error(NULL, "frame rate can't be 0");
		return NULL;
	}
	if ((width % 16) || (height % 16) ||
	    (width <= 0) || (height <= 0)) {
		free(context->private);
		free(context);
		rte_error(NULL, "dimensions must be 16xN (%d, %d)",
			  width, height);
		return NULL;
	}

	if (encode_callback)
		context->private->encode_callback = encode_callback;
	else {
		if (!file) {
			free(context->private);
			free(context);
			rte_error(NULL, "file == callback == NULL");
			return NULL;
		}
		
		context->file_name = strdup(file);
	}

	context->private->data_callback = data_callback;

	context->private->user_data = user_data;
	context->video_rate = rate;
	context->width = MIN(width, MAX_WIDTH);
	context->height = MIN(height, MAX_HEIGHT);
	context->private->fd = -1;

	context->audio_rate = 44100;
	context->output_audio_bits = 80000;
	context->output_video_bits = 2000000;

	/* No padding needed, YUYV by default */
	context->video_bytes = context->width * context->height * 2;

	context->mode = RTE_MUX_VIDEO_AND_AUDIO;

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

	rte_global_context = context;

	return (context);
}

void * rte_context_destroy ( rte_context * context )
{
	if (!context)
	{
		rte_error(NULL, "context == NULL");
		return NULL;
	}

	if (context != rte_global_context)
	{
		rte_error(NULL, "Sorry, the given context hasn't been created by rte");
		return NULL;
	}

	if (context->file_name)
		free(context->file_name);

	if (context->private->encoding)
		rte_stop(context);

	if (context->error)
		free(context->error);
	free(context->private);
	free(context);

	rte_global_context = NULL;

	return NULL;
}

#define DECIMATING(mode) (mode == RTE_YUYV_VERTICAL_DECIMATION ||	\
			  mode == RTE_YUYV_EXP_VERTICAL_DECIMATION)

void rte_set_video_parameters (rte_context * context,
			       enum rte_pixformat frame_format,
			       int width, int height,
			       enum rte_frame_rate video_rate,
			       size_t output_video_bits)
{
	if (!context)
	{
		rte_error(NULL, "context == NULL");
		return;
	}
	if (context->private->encoding)
	{
		rte_error(context, "already encoding");
		return;
	}
	if ((width % 16) || (height % 16) ||
	    (width <= 0) || (height <= 0))
	{
		rte_error(context, "the given dimensions aren't 16 multiplus");
		return;
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
		break;
	case RTE_RGB565:
		context->video_bytes *= 2;
		context->private->rgbfilter = convert_rgb565_ycbcr420;
		context->private->rgbmem =
			malloc(context->width*context->height*2);
		break;
	case RTE_RGB24:
		context->video_bytes *= 3;
		context->private->rgbfilter = convert_rgb24_ycbcr420;
		context->private->rgbmem =
			malloc(context->width*context->height*3);
		break;
	case RTE_BGR24:
		context->video_bytes *= 3;
		context->private->rgbfilter = convert_bgr24_ycbcr420;
		context->private->rgbmem =
			malloc(context->width*context->height*3);
		break;
	case RTE_RGB32:
		context->video_bytes *= 4;
		context->private->rgbfilter = convert_rgb32_ycbcr420;
		context->private->rgbmem =
			malloc(context->width*context->height*4);
		break;
	case RTE_BGR32:
		context->video_bytes *= 4;
		context->private->rgbfilter = convert_bgr32_ycbcr420;
		context->private->rgbmem =
			malloc(context->width*context->height*4);
		break;
	default:
		rte_error(context, "unhandled pixformat: %d", frame_format);
		break;
	}
}

/* Sets the audio parameters */
void rte_set_audio_parameters (rte_context * context,
			       int audio_rate,
			       enum rte_audio_mode audio_mode,
			       size_t output_audio_bits)
{
	int stereo;
	if (!context)
	{
		rte_error(NULL, "context == NULL");
		return;
	}
	if (context->private->encoding)
	{
		rte_error(context, "already encoding");
		return;
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
}

void rte_set_mode (rte_context * context, enum rte_mux_mode mode)
{
	if (!context)
	{
		rte_error(NULL, "context == NULL");
		return;
	}
	if (context->private->encoding)
	{
		rte_error(context, "already encoding");
		return;
	}

	context->mode = mode;
}

void rte_set_data_callback (rte_context * context, rteDataCallback
			    callback)
{
	if (!context)
	{
		rte_error(NULL, "context == NULL");
		return;
	}
	if (context->private->encoding)
	{
		rte_error(context, "already encoding");
		return;
	}

	context->private->data_callback = callback;
}

rteDataCallback rte_get_data_callback (rte_context * context)
{
	if (!context)
	{
		rte_error(NULL, "context == NULL");
		return NULL;
	}
	return (context->private->data_callback);
}

void rte_set_encode_callback (rte_context * context,
			      rteEncodeCallback callback)
{
	if (!context)
	{
		rte_error(NULL, "context == NULL");
		return;
	}
	if (context->private->encoding)
	{
		rte_error(context, "already encoding");
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
	if (!context)
	{
		rte_error(NULL, "context == NULL");
		return;
	}
	if (context->private->encoding)
	{
		rte_error(context, "already encoding");
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
	if (!context)
	{
		rte_error(NULL, "context == NULL");
		return NULL;
	}
	return context->file_name;
}

void rte_set_user_data(rte_context * context, void * user_data)
{
	if (!context)
	{
		rte_error(NULL, "context == NULL");
		return;
	}
	if (context->private->encoding)
	{
		rte_error(context, "already encoding");
		return;
	}

	context->private->user_data = user_data;
}

void * rte_get_user_data(rte_context * context)
{
	if (!context)
	{
		rte_error(NULL, "context == NULL");
		return NULL;
	}
	return context->private->user_data;
}


/* translates the context options to the global mp1e options */
static int rte_fake_options(rte_context * context);

int rte_start ( rte_context * context )
{
	int alloc_bytes; /* bytes allocated for the video fifo */

	if (!context)
	{
		rte_error(NULL, "context == NULL");
		return 0;
	}
	if (context->private->encoding)
	{
		rte_error(context, "already encoding");
		return 0;
	}
	if ((context->private->encode_callback) && (context->file_name)) {
		rte_error(context, "You need an encode callback OR a file name");
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

	/* fixme: clean this up */
	if (modules & 2) {
		char *modes[] = { "stereo", "joint stereo", "dual channel", "mono" };
		long long n = llroundn(((double) video_num_frames / frame_rate_value[frame_rate_code])
			/ (1152.0 / sampling_rate));

		printv(1, "Audio compression %2.1f kHz%s %s at %d kbits/s (%1.1f : 1)\n",
			sampling_rate / (double) 1000, sampling_rate < 32000 ? " (MPEG-2)" : "", modes[audio_mode],
			audio_bit_rate / 1000, (double) sampling_rate * (16 << stereo) / audio_bit_rate);

		if (modules & 1)
			audio_num_frames = MIN(n, (long long) INT_MAX);

		audio_init();
	}

	if (modules & 1) {
		video_coding_size(width, height);

		if (frame_rate > frame_rate_value[frame_rate_code])
			frame_rate = frame_rate_value[frame_rate_code];

		printv(2, "Macroblocks %d x %d\n", mb_width, mb_height);

		printv(1, "Video compression %d x %d, %2.1f frames/s at %1.2f Mbits/s (%1.1f : 1)\n",
			width, height, (double) frame_rate,
			video_bit_rate / 1e6, (width * height * 1.5 * 8 * frame_rate) / video_bit_rate);

		video_init();

#if TEST_PREVIEW
		if (preview > 0)
			preview_init();
#endif
	}

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

	context->private->v_ubuffer = -1;
	context->private->a_ubuffer = NULL;
	context->private->encoding = 1;

	/* Hopefully 8 frames is more than enough (we should only go
	   2-3 frames ahead). With 16 buffers we lose fewer frames */
	if (context->mode & 1)
	{
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
		_init_fifo(&(context->private->vid), "video input",
			  alloc_bytes, 8);
		context->private->video_pending = 0;
		if (context->private->data_callback) {
			pthread_mutex_init(&(context->private->video_mutex),
					   NULL);
			pthread_cond_init(&(context->private->video_cond),
					  NULL);
			pthread_create(&(context->private->video_fetcher_id),
				       NULL, video_input_thread, context);
//			schedule_video_fetch(context);
			/* Wait until we get the frames (avoid frame drops) */
			pthread_mutex_lock(&(context->private->video_mutex));
			while (context->private->video_pending)
				pthread_cond_wait(&(context->private->video_cond), &(context->private->video_mutex));

			pthread_mutex_unlock(&(context->private->video_mutex));
		}
	}

	if (context->mode & 2) {
		_init_fifo(&(context->private->aud), "audio input",
			  context->audio_bytes, 8);
		context->private->audio_pending = 0;
		if (context->private->data_callback) {
			pthread_mutex_init(&(context->private->audio_mutex),
					   NULL);
			pthread_cond_init(&(context->private->audio_cond),
					  NULL);
			pthread_create(&(context->private->audio_fetcher_id),
				       NULL, audio_input_thread, context);
			schedule_audio_fetch(context);
			/* Wait until we get the samples */
			pthread_mutex_lock(&(context->private->audio_mutex));
			while (context->private->audio_pending)
				pthread_cond_wait(&(context->private->audio_cond), &(context->private->audio_mutex));
			pthread_mutex_unlock(&(context->private->audio_mutex));
		}
	}

	ASSERT("open output files", output_init() >= 0);

	ASSERT("create output thread",
	       !pthread_create(&output_thread_id, NULL, output_thread, NULL));

//	if ((modules & 3) == 3)
//		synchronize_capture_modules();

	printv(3, "\nWir sind jetzt hier.\n");

	if (modules & 2) {
		ASSERT("create audio compression thread",
			!pthread_create(&audio_thread_id, NULL,
			stereo ? mpeg_audio_layer_ii_stereo :
				 mpeg_audio_layer_ii_mono, NULL));

		printv(2, "Audio compression thread launched\n");
	}

	if (modules & 1) {
		ASSERT("create video compression thread",
			!pthread_create(&video_thread_id, NULL,
				mpeg1_video_ipb, NULL));

		printv(2, "Video compression thread launched\n");
	}

	if ((modules & 3) != 3)
		mux_syn = 0;

	/*
	 *  XXX Thread these, not UI.
	 *  Async completion indicator?? 
	 */
	switch (mux_syn) {
	case 0:
		ASSERT("create elementary stream thread",
		       !pthread_create(&mux_thread, NULL,
				       elementary_stream_bypass, NULL));
		break;
	case 1:
		ASSERT("create mpeg1 system mux",
		       !pthread_create(&mux_thread, NULL,
				       mpeg1_system_mux, NULL));
		break;
	case 2:
		printv(1, "MPEG-2 Program Stream");
		ASSERT("create mpeg1 system mux",
		       !pthread_create(&mux_thread, NULL,
				       mpeg2_program_stream_mux, NULL));
		break;
	}

	return 1;
}

void rte_stop ( rte_context * context )
{
	struct timeval tv;

	if (!context)
	{
		rte_error(NULL, "context == NULL");
		return;
	}

	context->private->encoding = 0;

	/* Tell the mp1e threads to shut down */
	gettimeofday(&tv, NULL);

	video_stop_time =
	audio_stop_time = tv.tv_sec + tv.tv_usec / 1e6;

	/* Join them */
	pthread_join(mux_thread, NULL);

	mux_cleanup();

	output_end();
	/* mp1e is done (fixme: close preview too) */

	/* Free the mem in the fifos */
	if (context->mode & 1) {
		if (context->private->data_callback) {
			pthread_mutex_lock(&(context->private->video_mutex));
			context->private->video_pending = -10000;
			pthread_cond_broadcast(&(context->private->video_cond));
			pthread_mutex_unlock(&(context->private->video_mutex));
			pthread_join(context->private->video_fetcher_id, NULL);
			pthread_cond_destroy(&(context->private->video_cond));
			pthread_mutex_destroy(&(context->private->video_mutex));
		}
		_free_fifo(&(context->private->vid));
	}

	if (context->mode & 2) {
		if (context->private->data_callback) {
			pthread_mutex_lock(&(context->private->audio_mutex));
			context->private->audio_pending = -10000;
			pthread_cond_broadcast(&(context->private->audio_cond));
			pthread_mutex_unlock(&(context->private->audio_mutex));
			pthread_join(context->private->audio_fetcher_id, NULL);
			pthread_cond_destroy(&(context->private->audio_cond));
			pthread_mutex_destroy(&(context->private->audio_mutex));
		}
		_free_fifo(&(context->private->aud));
	}

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
	_buffer * buf;

	if (!context) {
		rte_error(NULL, "context == NULL");
		return NULL;
	}
	if (!context->private->encoding) {
		rte_error(context, "the context isn't encoding, call rte_start");
		return NULL;
	}
	if (!(context->mode & 1)) {
		rte_error(context, "Mux isn't prepared to encode video!");
		return NULL;
	}
	if (context->private->data_callback) {
		rte_error(context,
			  "The push and the callbacks interfaces don't work well together");
		return NULL;
	}

	buf = _new_buffer(&(context->private->vid));

	ASSERT("Arrr... stick to the usage, please\n",
	       (context->private->last_video_buffer && data)
	       || (!context->private->last_video_buffer && !data));

	if (data) {
		if (!context->private->rgbfilter)
			ASSERT("you haven't written to the provided buffer!\n",
			       data == context->private->last_video_buffer->data);
		else
			context->private->rgbfilter(data, context->private->last_video_buffer->data, context->width, context->height);

		context->private->last_video_buffer->time = time;

		_send_out_buffer(&(context->private->vid),
				context->private->last_video_buffer);
	}

	context->private->last_video_buffer = buf;

	if (context->private->data_callback) {
		pthread_mutex_lock(&(context->private->video_mutex));
		if (context->private->video_pending > 0)
			context->private->video_pending --;
		pthread_cond_broadcast(&(context->private->video_cond));
		pthread_mutex_unlock(&(context->private->video_mutex));
	}

	if (context->private->rgbfilter)
		return context->private->rgbmem;

	return buf->data;
}

void * rte_push_audio_data ( rte_context * context, void * data,
			     double time )
{
	_buffer * buf;

	if (!context) {
		rte_error(NULL, "context == NULL");
		return NULL;
	}
	if (!context->private->encoding) {
		rte_error(context, "the context isn't encoding, call rte_start");
		return NULL;
	}
	if (!(context->mode & 2)) {
		rte_error(context, "Mux isn't prepared to encode audio!");
		return NULL;
	}
	if (context->private->data_callback) {
		rte_error(context,
			  "The push and the callbacks interfaces don't work well together");
		return NULL;
	}

	buf = _new_buffer(&(context->private->aud));

	ASSERT("Arrr... stick to the usage, please\n",
	       (context->private->last_audio_buffer && data)
	       || (!context->private->last_audio_buffer && !data));

	if (data) {
		ASSERT("you haven't written to the provided buffer!\n",
		       data == context->private->last_audio_buffer->data);

		context->private->last_audio_buffer->time = time;

		_send_out_buffer(&(context->private->aud),
				context->private->last_audio_buffer);
	}

	context->private->last_audio_buffer = buf;

	if (context->private->data_callback) {
		pthread_mutex_lock(&(context->private->audio_mutex));
		if (context->private->audio_pending > 0)
			context->private->audio_pending --;
		pthread_cond_broadcast(&(context->private->audio_cond));
		pthread_mutex_unlock(&(context->private->audio_mutex));
	}

	return buf->data;
}

/* video_start = video_input_start */
static void
video_input_start ( void )
{
	/* FIXME: Nothing needs to be done here, it will probably be
	   removed in the future */
}

/* video_wait_frame = video_input_wait_frame */
static buffer *
video_input_wait_frame (fifo *f1)
{
	rte_context * context = rte_global_context; /* FIXME: this shoudn't be global */
	_buffer * b;
	_fifo * f;
	buffer *b1 = calloc(sizeof(buffer), 1); // XXX forward b iff buffer *

	// XXX obsolete, new fifo handles ungetting
	if (context->private->v_ubuffer > -1) {
		b1->index = context->private->v_ubuffer;
		ASSERT("Checking that i'm sane\n",
		       context->private->vid.num_buffers > b1->index);

		b = &(context->private->vid.buffer[b1->index]);
		b1->time = b->time;
		context->private->v_ubuffer = -1;
		b1->data = b->data;

		return b1;
	}

	f = &(context->private->vid);

	pthread_mutex_lock(&f->mutex);

	b = (_buffer*) rem_head(&f->full);
	
	pthread_mutex_unlock(&(f->mutex));
	
	schedule_video_fetch(context);

	if (!b) {
		pthread_mutex_lock(&f->mutex);
		
		while (!(b = (_buffer*) rem_head(&f->full)))
			pthread_cond_wait(&(f->cond), &(f->mutex));
		
		pthread_mutex_unlock(&(f->mutex));
	}

	b1->index = b->index;
	b1->time = b->time;
	b1->data = b->data;

	return b1;
}

/* video_frame_done = video_input_frame_done */
static void
video_input_frame_done(fifo *f1, buffer *b1)
{
	rte_context * context = rte_global_context; /* fixme: avoid global */
	int buf_index = b1->index;

	free(b1);

	ASSERT("Checking that i'm sane\n",
	       context->private->vid.num_buffers > buf_index);

	_empty_buffer(&(context->private->vid),
		     &(context->private->vid.buffer[buf_index]));
}

// XXX obsolete
/* video_unget_frame = video_input_unget_frame */
static void
video_input_unget_frame(int buf_index)
{
	rte_context * context = rte_global_context; /* fixme: avoid global */

	ASSERT("You shouldn't unget() twice, core!\n",
	       context->private->v_ubuffer < 0);

	context->private->v_ubuffer = buf_index;
}

/* audio_read = audio_input_read */
static buffer *
audio_input_read(fifo *f1)
{
	rte_context * context = rte_global_context; /* FIXME: this shoudn't be global */
	_buffer * b;
	_fifo * f;
	buffer *b1 = calloc(sizeof(buffer), 1); // XXX use b iff buffer *

	b = context->private->a_ubuffer;

	// XXX obsolete, new fifo handles ungetting
	if (context->private->a_again) {
		ASSERT("re-using audio before using it\n", b != NULL);
		context->private->a_again = 0;
		b1->time = b->time;
		b1->data = b->data;
		return b1;
	}

	f = &(context->private->aud);

	if (b)
		_empty_buffer(f, b);

	pthread_mutex_lock(&f->mutex);

	b = (_buffer*) rem_head(&f->full);
	
	pthread_mutex_unlock(&(f->mutex));
	
	schedule_audio_fetch(context);

	if (!b) {
		pthread_mutex_lock(&f->mutex);
		
		while (!(b = (_buffer*) rem_head(&f->full)))
			pthread_cond_wait(&(f->cond), &(f->mutex));
		
		pthread_mutex_unlock(&(f->mutex));
	}

	context->private->a_ubuffer = b;

	b1->time = b->time;
	b1->data = b->data;
	return b1;
}

static void
audio_input_done(fifo *f, buffer *b)
{
	free(b);
}

static fifo		rte_audio_cap_fifo;
static fifo		rte_video_cap_fifo;

int rte_init ( void )
{
	if (!cpu_id(ARCH_PENTIUM_MMX))
		return 0;

	rte_global_context = NULL;

	video_start = video_input_start;

	init_callback_fifo(video_cap_fifo = &rte_video_cap_fifo,
		video_input_wait_frame, video_input_frame_done,
		NULL, NULL, 0, 0);

	init_callback_fifo(audio_cap_fifo = &rte_audio_cap_fifo,
		audio_input_read, audio_input_done,
		NULL, NULL, 0, 0);

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
		rte_error(context, "Invalid frame rate: %d", context->video_rate);
	}
	frame_rate_code = context->video_rate;
	switch (context->video_format) {
	case RTE_YUYV_VERTICAL_DECIMATION:
	case RTE_YUYV_TEMPORAL_INTERPOLATION:
	case RTE_YUYV_VERTICAL_INTERPOLATION:
	case RTE_YUYV_PROGRESSIVE:
	case RTE_YUYV_PROGRESSIVE_TEMPORAL:
	case RTE_YUYV_EXP:
	case RTE_YUYV_EXP_VERTICAL_DECIMATION:
	case RTE_YUYV_EXP2:
		pitch = grab_width*2;
		filter_mode = /* errr... a bit hackish, maybe :-) */
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
	return 1;
}

/* Startup video parameters */
static void rte_audio_startup(void)
{
	if (modules & 2) {
		struct stat st;
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
	if (modules & 1) {
		struct stat st;
		{
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
}

/* Compression parameters */
static void rte_compression_startup(void)
{
	mucon_init(&mux_mucon);
}
