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
/*
 * $Id: rtepriv.h,v 1.11 2000-11-04 20:38:02 garetxe Exp $
 * Private stuff in the context.
 */

#ifndef __RTEPRIV_H__
#define __RTEPRIV_H__
#include "rte.h"
#include <pthread.h>

extern rte_context * rte_global_context;

typedef void (*_rte_filter)(const char * src, char * dest, int width,
			    int height);

#define RC(X) ((rte_context*)X)

#define rte_error(context, format, args...) \
{ \
	if (context) { \
		if (!RC(context)->error) \
			RC(context)->error = malloc(256); \
		RC(context)->error[255] = 0; \
		snprintf(RC(context)->error, 255, \
			 "rte:%s:%s(%d): " format, \
			 __FILE__, __PRETTY_FUNCTION__, __LINE__ ,##args); \
	} \
	else \
		fprintf(stderr, "rte:%s:%s(%d): " format ".\n", \
			__FILE__, __PRETTY_FUNCTION__, __LINE__ ,##args); \
}

/*
  Private things we don't want people to see, we can play with this
  without breaking any compatibility.
  Eventually all the global data will move here, except for the
  tables.
*/
struct _rte_context_private {
	int encoding; /* 0 if not encoding */
	int inited; /* 0 if not inited */
	rteEncodeCallback encode_callback; /* save-data Callback */
	rteDataCallback audio_data_callback; /* need audio data */
	rteDataCallback video_data_callback; /* need video data */
	rteBufferCallback audio_buffer_callback; /* need audio buffer */
	rteBufferCallback video_buffer_callback; /* need video buffer */
	rteUnrefCallback audio_unref_callback; /* audio release */
	rteUnrefCallback video_unref_callback; /* video release */
	enum rte_interface audio_interface; /* audio interface */
	enum rte_interface video_interface; /* video interface */
	int audio_buffered; /* whether the audio uses buffers or memcpy */
	int video_buffered; /* whether the video uses buffers or memcpy */
	pthread_t mux_thread; /* mp1e multiplexer thread */
	pthread_t video_thread_id; /* video encoder thread */
	pthread_t audio_thread_id; /* audio encoder thread */
	int fd; /* file descriptor of the file we are saving */
	void * user_data; /* user data given to the callback */
	fifo aud, vid; /* callback fifos for pushing */
	mucon aud_consumer; /* consumer mucon for audio */
	mucon vid_consumer; /* consumer mucon for video */
	int depth; /* video bit depth (bytes per pixel, includes
		      packing) */
	buffer * last_video_buffer; /* video buffer the app should be
				       encoding to */
	buffer * last_audio_buffer; /* audio buffer */
	_rte_filter rgbfilter; /* the filter used for conversion, if any */
	char * rgbmem; /* allocated mem for the rgb image */
};

/* Some macros for avoiding repetitive typing */
#define nullcheck(X, whattodo) \
do { \
	if (!X) { \
		rte_error(NULL, #X " == NULL"); \
		whattodo; \
	} \
} while (0)

#endif /* rtepriv.h */
