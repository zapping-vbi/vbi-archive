/*
 *  MPEG-1 Real Time Encoder lib wrapper api
 *
 *  Copyright (C) 2000 Iñaki García Etxebarria
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
/*
 * $Id: rtepriv.h,v 1.12 2001-10-26 09:14:51 mschimek Exp $
 * Private stuff in the context.
 */

#ifndef __RTEPRIV_H__
#define __RTEPRIV_H__
#include "rte.h"
#include <pthread.h>
#include <stdarg.h>

#define BLANK_BUFFER	1 /* the buffer was created by blank_callback,
			     do not unref_callback it */

typedef void (*_rte_filter)(const char * src, char * dest, int width,
			    int height);

/* for the sake of clarity, prototype of wait_data in rte.c */
typedef void (*_wait_data)(rte_context *context, int video);






/* Experimental */

/* maybe one should add this stuff under a #ifdef RTE_BACKEND to rte.h? */

typedef struct rte_context_class rte_context_class;

struct rte_context_class {
	rte_context_class *	next;
	rte_context_info	public;

	/* ? */

	rte_codec_info *	(* codec_enum)(rte_context *, int);
	rte_codec *		(* codec_get)(rte_context *, rte_stream_type, int);
	rte_codec *		(* codec_set)(rte_context *, rte_stream_type, int, char *);

	rte_option *		(* option_enum)(rte_codec *, int);
	int			(* option_get)(rte_codec *, char *, rte_option_value *);
	int			(* option_set)(rte_codec *, char *, va_list);
	char *			(* option_print)(rte_codec *, char *, va_list);

	int			(* parameters)(rte_codec *, rte_stream_parameters *);
};

typedef struct rte_codec_class rte_codec_class;

struct rte_codec_class {
	rte_codec_class *	next;
	rte_codec_info		public;

	/*
	 *  Allocate new codec instance. All fields zero except
	 *  rte_codec.class, .status (RTE_STATUS_NEW), .mutex (initialized),
	 *  and all codec properties reset to defaults.
	 */
	rte_codec *		(* new)(void);
	void			(* delete)(rte_codec *);

	rte_option *		(* option_enum)(rte_codec *, int index);
	int			(* option_get)(rte_codec *, char *, rte_option_value *);
	int			(* option_set)(rte_codec *, char *, va_list);
	char *			(* option_print)(rte_codec *, char *, va_list);

	int			(* parameters)(rte_codec *, rte_stream_parameters *);

	/* result unused (yet) */
	void *			(* mainloop)(void *rte_codec);
};

typedef enum rte_codec_status {
	/* new -> */
	RTE_STATUS_NEW = 1,
	RTE_STATUS_RESERVED2,
	/* accept options,
           sample parameters -> */
	RTE_STATUS_PARAM,
	/* option change -> RTE_STATUS_NEW,
           params change -> RTE_STATUS_PARAM,
           i/o -> */
	RTE_STATUS_READY,
	RTE_STATUS_RESERVED5,
	RTE_STATUS_RESERVED6,
	/* option change -> RTE_STATUS_NEW,
	   params change -> RTE_STATUS_PARAM,
	   i/o change -> RTE_STATUS_READY,
           start -> */
	RTE_STATUS_RUNNING,
	RTE_STATUS_RESERVED8,
	/* stop -> */
	RTE_STATUS_STOPPED,
	/* ? -> ? */
	RTE_STATUS_RESERVED10,
} rte_codec_status;

struct rte_codec {
	rte_codec *		next;

	rte_context *		context;	/* parent context */
	rte_codec_class *	class;		/* read only */

	int			stream;		/* multiplexer substream */

	pthread_mutex_t		mutex;		/* locked by class funcs */

	rte_codec_status	status;		/* mutex protected, read only */

	/* Valid when RTE_STATUS_RUNNING; mutex protected, read only. */
	int64_t			frame_input_count;
	int64_t			frame_input_missed;	/* excl. intent. skipped */
	int64_t			frame_output_count;
	int64_t			byte_output_count;
	double			coded_time_elapsed;
	// XXX?
	double			frame_output_rate;	/* invariable, 1/s */

	/* append codec private stuff */
};



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
	int		(*pre_init_context)(rte_context * context);
	int		(*post_init_context)(rte_context * context);
	void		(*uninit_context)(rte_context * context);
	int		(*start)(rte_context * context);
	void		(*stop)(rte_context * context);
	char*		(*query_format)(rte_context *context,
					int n,
					enum rte_mux_mode *mux_mode);
	void		(*status)(rte_context * context,
				  struct rte_status_info *status);

	/* Experimental */

	rte_context_info *	(* context_enum)(int);
	rte_context *		(* context_new2)(char *);
	void			(* context_delete)(rte_context *);

	/* tbd context_class */

	rte_codec_info *	(* codec_enum)(rte_context *, int);
	rte_codec *		(* codec_get)(rte_context *, rte_stream_type, int);
	rte_codec *		(* codec_set)(rte_context *, rte_stream_type, int, char *);

	rte_option *		(* option_enum)(rte_codec *, int);
	int			(* option_get)(rte_codec *, char *, rte_option_value *);
	int			(* option_set)(rte_codec *, char *, va_list);
	char *			(* option_print)(rte_codec *, char *, va_list);

	int			(* parameters)(rte_codec *, rte_stream_parameters *);

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
	int fd64; /* file descriptor of the file we are saving */
	void * user_data; /* user data given to the callback */
	fifo vid, aud; /* callback fifos for pushing */
	producer vid_prod, aud_prod;
	int depth; /* video bit depth (bytes per pixel, includes
		      packing) */
	buffer * last_video_buffer; /* video buffer the app should be
				       encoding to */
	buffer * last_audio_buffer; /* audio buffer */
	unsigned long long bytes_out; /* sent bytes */

	rte_context_class *	class;
};

/*
 *  Helper functions
 *
 *  (note the backend is bypassed, may change)
 */

#define nullcheck(X, whattodo)						\
do {									\
	if (!X) {							\
		rte_error(NULL, #X " == NULL");				\
		whattodo;						\
	}								\
} while (0)

static inline int
rte_helper_set_option_va(rte_codec *codec, char *keyword, ...)
{
	va_list args;
	int r;

	va_start(args, keyword);
	r = codec->class->option_set(codec, keyword, args);
	va_end(args);

	return r;
}

static inline int
rte_helper_reset_options(rte_codec *codec)
{
	rte_option *option;
	int r = 1, i = 0;

	while (r && (option = codec->class->option_enum(codec, i++))) {
		switch (option->type) {
		case RTE_OPTION_INT:
		case RTE_OPTION_BOOL:
		case RTE_OPTION_MENU:
			r = rte_helper_set_option_va(
				codec, option->keyword, option->def.num);
			break;
		case RTE_OPTION_STRING:
			r = rte_helper_set_option_va(
				codec, option->keyword, option->def.str);
			break;
		case RTE_OPTION_REAL:
			r = rte_helper_set_option_va(
				codec, option->keyword, option->def.dbl);
			break;
		default:
			assert(!"reset option->type");
		}
	}

	return r;
}

#endif /* rtepriv.h */


