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
 * $Id: rtepriv.h,v 1.3 2001-09-07 05:09:34 mschimek Exp $
 * Private stuff in the context.
 */

#ifndef __RTEPRIV_H__
#define __RTEPRIV_H__
#include "rte.h"
#include <pthread.h>

#define BLANK_BUFFER	1 /* the buffer was created by blank_callback,
			     do not unref_callback it */

extern rte_context * rte_global_context;

typedef void (*_rte_filter)(const char * src, char * dest, int width,
			    int height);

/* for the sake of clarity, prototype of wait_data in rte.c */
typedef void (*_wait_data)(rte_context *context, int video);

typedef struct {
	char		*name;
	int		priv_size; /* sizeof(*context->priv)+backend data */
	/* basic backend functions */
	int		(*init_backend)(void);
	/* Init backend specific structs and context->format with trhe
	 default format */
	void		(*context_new)(rte_context * context);
	/* Free context specific structs and context->format */
	void		(*context_destroy)(rte_context * context);
	int		(*init_context)(rte_context * context);
	void		(*uninit_context)(rte_context * context);
	int		(*start)(rte_context * context);
	void		(*stop)(rte_context * context);
	char*		(*query_format)(rte_context *context,
					int n,
					enum rte_mux_mode *mux_mode);
	void		(*status)(rte_context * context,
				  struct rte_status_info *status);
	/* Experimental */
	rte_codec_info *(* enum_codec)(rte_context *context, int index);
	rte_codec *	(* set_codec)(rte_context *context,
				      rte_stream_type stream_type,
				      int stream_index, char *keyword);
	rte_option *	(* enum_option)(rte_context *, rte_codec *, int index);
	int		(* set_option)(rte_context *, rte_codec *, char *, va_list);
} rte_backend_info;

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
	int backend; /* backend to be used */
	rteEncodeCallback encode_callback; /* save-data Callback */
	rteSeekCallback seek_callback; /* seek in file callback */
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
	int fd; /* file descriptor of the file we are saving */
	void * user_data; /* user data given to the callback */
	fifo vid, aud; /* callback fifos for pushing */
	producer vid_prod, aud_prod;
	int depth; /* video bit depth (bytes per pixel, includes
		      packing) */
	buffer * last_video_buffer; /* video buffer the app should be
				       encoding to */
	buffer * last_audio_buffer; /* audio buffer */
/* XXX 64 bit please, 4 GB is easily exceeded */
	unsigned long int bytes_out; /* sent bytes */
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
