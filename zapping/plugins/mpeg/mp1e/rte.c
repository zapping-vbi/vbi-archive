/*
 *  MPEG-1 Real Time Encoder lib wrapper api
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

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "options.h"
#include "fifo.h"
#include "mmx.h"
#include "rte.h"

/*
  Private things we don't want people to see, we can play with this
  without breaking any compatibility.
  Eventually all the global data will move here, except for the
  tables.
*/
struct _rte_context_private {
	int encoding; /* 1 if working, 0 if not */
	rteEncodeCallback encode_callback; /* save-data Callback */
	rteDataCallback data_callback; /* need-data Callback */
	int fd; /* file descriptor of the file we are saving */
	char * error; /* last error */
	void * user_data; /* user data given to the callback */
	fifo aud, vid; /* fifos for pushing */
	int depth; /* video bit depth (bytes per pixel, includes
		      packing) */
	buffer * last_video_buffer; /* video buffer the app should be
				       encoding to */
	buffer * last_audio_buffer; /* audio buffer */
};

#define RC(X)  ((rte_context*)X)

#define rte_error(context, format, args...) \
{ \
	if (context) { \
		if (!RC(context)->private->error) \
			RC(context)->private->error = malloc(256); \
		RC(context)->private->error[255] = 0; \
		snprintf(RC(context)->private->error, 255, \
			 "rte - %s (%d): " format, \
			 __PRETTY_FUNCTION__, __LINE__ ,##args); \
	} \
	else \
		fprintf(stderr, "rte - %s (%d): " format ".\n", \
			__PRETTY_FUNCTION__, __LINE__ ,##args); \
}

static inline void
fetch_data(rte_context * context, int video)
{
	buffer * buf;
	fifo * f;
	
	if (!context->private->data_callback)
		return;

	if (video)
		f = &(context->private->vid);
	else
		f = &(context->private->aud);

	buf = new_buffer(f);

	context->private->data_callback(buf->data, &(buf->time), video,
					context, context->private->user_data);

	send_out_buffer(f, buf);
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

int rte_init ( void )
{
	if (!cpu_id(ARCH_PENTIUM_MMX))
		return 0;

	return 1;
}

rte_context * rte_context_new (char * file,
			       int width, int height,
			       enum rte_frame_rate rate,
			       rteEncodeCallback encode_callback,
			       rteDataCallback data_callback,
			       void * user_data)
{
	rte_context * context =
		malloc(sizeof(rte_context));

	if (!context)
	{
		rte_error(NULL, "malloc(): [%d] %s", errno, strerror(errno));
		return NULL;
	}
	context->private = malloc(sizeof(rte_context_private));
	if (!context->private)
	{
		free(context);
		rte_error(NULL, "malloc(): [%d] %s", errno, strerror(errno));
		return NULL;
	}

	memset(context, 0, sizeof(rte_context));
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
	else
		context->private->encode_callback =
			RTE_ENCODE_CALLBACK(default_write_callback);

	context->private->data_callback = data_callback;

	context->private->user_data = user_data;
	context->video_rate = rate;
	context->width = width;
	context->height = height;
	context->private->fd = -1;

	if (!encode_callback) {
		if (!file) {
			free(context->private);
			free(context);
			rte_error(NULL, "file == callback == NULL");
			return NULL;
		}
		context->private->fd = creat(file, 00755);
		if (context->private->fd < 0) {
			rte_error(NULL, "creat(): [%d] %s", errno, strerror(errno));
			free(context->private);
			free(context);
			return NULL;
		}
	}

	context->audio_rate = 44100;
	context->bits = 16;
	context->output_audio_bits = 80000;
	context->output_video_bits = 2000000;

	/* FIXME: set up audio_bytes and video_bytes */

	context->mode = RTE_MUX_VIDEO_AND_AUDIO;

	return (context);
}

void * rte_context_destroy ( rte_context * context )
{
	if (!context)
	{
		rte_error(NULL, "context == NULL");
		return NULL;
	}

	if (context->private->encoding)
		rte_stop(context);

	if (context->private->error)
		free(context->private->error);
	free(context->private);
	free(context);

	return NULL;
}

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
	context->width = width;
	context->height = height;
	context->video_rate = video_rate;
	context->output_video_bits = output_video_bits;

	/* FIXME: Update video_bytes */
}

int rte_start ( rte_context * context )
{
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

	/* Hopefully 8 frames is more than enough (we should only go
	   2-3 frames ahead) */
	if (context->mode & 1)
	{
		init_fifo(&(context->private->vid), "video input",
			  context->video_bytes, 8);
		fetch_data(context, 1);
	}
	if (context->mode & 2)
	{
		init_fifo(&(context->private->aud), "audio input",
			  context->audio_bytes, 8);
		fetch_data(context, 0);
	}

	context->private->encoding = 1;
	return 1;
}

void rte_stop ( rte_context * context )
{
	if (!context)
	{
		rte_error(NULL, "context == NULL");
		return;
	}

	/* Free the mem in the fifos */
	if (context->mode & 1)
		free_fifo(&(context->private->vid));
	if (context->mode & 2)
		free_fifo(&(context->private->aud));

	context->private->encoding = 0;
}

/* Input handling functions */
void * rte_push_video_data ( rte_context * context, void * data,
			   double time )
{
	buffer * buf;

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

	buf = new_buffer(&(context->private->vid));

	ASSERT("Arrr... stick to the usage, please\n",
	       (context->private->last_video_buffer && data)
	       || (!context->private->last_video_buffer && !data));

	if (data) {
		ASSERT("you haven't written to the provided buffer!\n",
		       data == context->private->last_video_buffer->data);

		context->private->last_video_buffer->time = time;

		send_out_buffer(&(context->private->vid),
				context->private->last_video_buffer);
	}

	context->private->last_video_buffer = buf;

	return buf->data;
}

void * rte_push_audio_data ( rte_context * context, void * data,
			     double time )
{
	buffer * buf;

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

	buf = new_buffer(&(context->private->aud));

	ASSERT("Arrr... stick to the usage, please\n",
	       (context->private->last_audio_buffer && data)
	       || (!context->private->last_audio_buffer && !data));

	if (data) {
		ASSERT("you haven't written to the provided buffer!\n",
		       data == context->private->last_audio_buffer->data);

		context->private->last_audio_buffer->time = time;

		send_out_buffer(&(context->private->aud),
				context->private->last_audio_buffer);
	}

	context->private->last_audio_buffer = buf;

	return buf->data;
}

char * rte_last_error ( rte_context * context )
{
	return (context->private->error);
}

/* video_start = video_input_start */
static void
video_input_start ( void )
{
	/* Nothing needs to be done here, it will be removed */
}

/* video_wait_frame = video_input_wait_frame */
static unsigned char *
video_input_wait_frame (double *ftime, int *buf_index)
{
	return NULL;
}
