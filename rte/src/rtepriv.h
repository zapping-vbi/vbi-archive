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
 * $Id: rtepriv.h,v 1.2 2001-12-15 18:48:56 garetxe Exp $
 * Private stuff in the context.
 */

#ifndef __RTEPRIV_H__
#define __RTEPRIV_H__
#include "rte.h"
#include <pthread.h>
#include <stdarg.h>
#include <assert.h>

/*
  Private things we don't want people to see, we can play with this
  without breaking any compatibility.
*/

typedef enum {
	/* new ->
	   options changed -> */
	RTE_STATUS_NEW = 0,
	/* set parameters -> */
	RTE_STATUS_PARAM,
	/* i/o -> */
	RTE_STATUS_READY,
	/* start -> */
	RTE_STATUS_RUNNING
	/* stop -> RTE_STATUS_READY */
} rte_status;

/* maybe one should add this stuff under a #ifdef RTE_BACKEND to rte.h? */

typedef struct _rte_codec_class rte_codec_class;

struct _rte_codec_class {
	rte_codec_class *	next;
	rte_codec_info		public;

	/*
	 *  Allocate new codec instance. All fields zero except
	 *  rte_codec.class, .status (RTE_STATUS_NEW), .mutex (initialized),
	 *  and all codec properties reset to defaults.
	 */
	rte_codec *		(* new)(void);
	void			(* delete)(rte_codec *);

	rte_option_info *	(* option_enum)(rte_codec *, int);
	int			(* option_get)(rte_codec *, const char *, rte_option_value *);
	int			(* option_set)(rte_codec *, const char *, va_list);
	char *			(* option_print)(rte_codec *, const char *, va_list);

	rte_bool		(* parameters)(rte_codec *, rte_stream_parameters *);

	rte_status_info *	(* status_enum)(rte_codec *, int);

	/* result unused (yet) */
	void *			(* mainloop)(void *rte_codec);
};

struct _rte_codec {
	rte_codec *		next;

	rte_context *		context;	/* parent context */
	rte_codec_class *	class;		/* read only */

	pthread_mutex_t		mutex;		/* locked by class funcs */

	rte_status		status;		/* ro */
	rte_pointer		user_data;

	enum {
		RTE_INPUT_CB, /* Callback buffered */
		RTE_INPUT_CD, /* Callback data */
		RTE_INPUT_PB, /* Push buffered */
		RTE_INPUT_PD /* Push data */
	} input_mode;

	union {
		/* Callbacks buffered */
		struct {
			rteBufferCallback	get;
			rteBufferCallback	unref;
		} cb;
		/* Callbacks data */
		struct {
			rteDataCallback		get;
		} cd;
	} input;

	/* FIXME: this ought to be codec private */
	int			stream;		/* multiplexer substream */

	/* Valid when RTE_STATUS_RUNNING; mutex protected, read only. */
	int64_t			frame_input_count;
	int64_t			frame_input_missed;	/* excl. intent. skipped */
	int64_t			frame_output_count;
	int64_t			byte_output_count;
	double			coded_time_elapsed;
	// XXX?
	double			frame_output_rate;	/* invariable, 1/s */
};

typedef struct _rte_context_class rte_context_class;

struct _rte_context_class {
	rte_context_class *	next;
	rte_context_info	public;

	void			(* delete)(rte_context *);

	rte_codec_info *	(* codec_enum)(rte_context *, int);
	rte_codec *		(* codec_get)(rte_context *, rte_stream_type, int);
	rte_codec *		(* codec_set)(rte_context *, int,
					      const char *);

	rte_option_info *	(* option_enum)(rte_context *, int);
	int			(* option_get)(rte_context *, const char *, rte_option_value *);
	int			(* option_set)(rte_context *, const char *, va_list);
	char *			(* option_print)(rte_context *, const char *, va_list);

	rte_status_info *	(* status_enum)(rte_context *, int);
};

struct _rte_context {
	void *		user_data; /* user data given to the callback */
	char *		error; /* Last error */
	rte_status	status; /* context status */

	rteWriteCallback write; /* save-data Callback */
	rteSeekCallback seek; /* seek in file callback */

	rte_context_class *	class; /* backend specific */
};

typedef struct {
	void			(* init)(void);
	rte_context_info *	(* context_enum)(int n);
	rte_context *		(* context_new)(const char *keyword);
} rte_backend_info;

/*
 *  Helper functions
 *
 *  (note the backend is bypassed, may change)
 */
#define RTE_OPTION_BOUNDS_INITIALIZER_(type_, def_, min_, max_, step_)	\
  { type_ = def_ }, { type_ = min_ }, { type_ = max_ }, { type_ = step_ }

#define RTE_OPTION_BOOL_INITIALIZER(key_, label_, def_, tip_)		\
  { RTE_OPTION_BOOL, key_, label_,					\
    RTE_OPTION_BOUNDS_INITIALIZER_(.num, def_, 0, 1, 1),		\
    { .num = NULL }, 0, tip_ }

#define RTE_OPTION_INT_INITIALIZER(key_, label_, def_, min_, max_,	\
  step_, menu_, entries_, tip_) { RTE_OPTION_INT, key_, label_,		\
    RTE_OPTION_BOUNDS_INITIALIZER_(.num, def_, min_, max_, step_),	\
    { .num = menu_ }, entries_, tip_ }

#define RTE_OPTION_REAL_INITIALIZER(key_, label_, def_, min_, max_,	\
  step_, menu_, entries_, tip_) { RTE_OPTION_REAL, key_, label_,	\
    RTE_OPTION_BOUNDS_INITIALIZER_(.dbl, def_, min_, max_, step_),	\
    { .dbl = menu_ }, entries_, tip_ }

#define RTE_OPTION_STRING_INITIALIZER(key_, label_, def_, menu_,	\
  entries_, tip_) { RTE_OPTION_STRING, key_, label_,			\
    RTE_OPTION_BOUNDS_INITIALIZER_(.str, def_, NULL, NULL, NULL),	\
    { .str = menu_ }, entries_, tip_ }

#define RTE_OPTION_MENU_INITIALIZER(key_, label_, def_, menu_,		\
  entries_, tip_) { RTE_OPTION_MENU, key_, label_,			\
    RTE_OPTION_BOUNDS_INITIALIZER_(.num, def_, 0, (entries_) - 1, 1),	\
    { .str = menu_ }, entries_, tip_ }

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
	rte_option_info *option;
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


