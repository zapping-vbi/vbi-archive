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
 * $Id: rtepriv.h,v 1.9 2002-02-08 15:03:11 mschimek Exp $
 * Private stuff in the context.
 */

#ifndef __RTEPRIV_H__
#define __RTEPRIV_H__
#include "rte.h"
#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include "../common/fifo.h"

/*
  Private things we don't want people to see, we can play with this
  without breaking any compatibility.
*/

/* public typedef struct rte_codec rte_codec; */
/* public typedef struct rte_context rte_context; */

typedef struct rte_codec_class rte_codec_class;
typedef struct rte_context_class rte_context_class;
typedef struct rte_backend_class rte_backend_class;

typedef enum {
	/*
	 *  On success of:
	 *  codec_class.new()
	 *  codec_class.option_set()
	 *  context_class.new()
	 *  context_class.option_set()
	 */
	RTE_STATUS_NEW = 0,
	/*
	 *  codec_class.parameters_set()
	 *  (no state of contexts)
	 */
	RTE_STATUS_PARAM,
	/*
	 *  codec_class.set_input()
	 *  context_class.set_output()
	 */
	RTE_STATUS_READY,
	/* start -> */
	RTE_STATUS_RUNNING,
	/* stop -> RTE_STATUS_READY
	   pause -> */
	RTE_STATUS_PAUSED
	/* resume -> RTE_STATUS_RUNNING */
} rte_status;

typedef enum {
	RTE_INPUT_CA = 1,	/* Callback active */
	RTE_INPUT_CP,		/* Callback passive */
	RTE_INPUT_PA,		/* Push active */
	RTE_INPUT_PP		/* Push passive */
} rte_input;

/*
 *  Codec instance.
 */
struct rte_codec {
	rte_codec *		next;		/* backend use, list of context's codec instances */

	rte_context *		context;	/* parent */
	rte_codec_class *	class;

	void *			user_data;
	int			stream_index;	/* from rte_codec_set() */

	/*
	 *  The mutex is used by the rte_codec_class functions
	 *  and protects all fields below. Attention: not recursive.
	 */
	pthread_mutex_t		mutex;

	/*
	 *  Maintained by codec, elsewhere read only. Stream
	 *  parameters are only valid at status RTE_STATUS_PARAM
	 *  or higher.
	 */
	rte_status		status;

	rte_stream_parameters	params;



// <<


	/* Valid when RTE_STATUS_RUNNING; mutex protected, read only. */
	int64_t			frame_input_count;
	int64_t			frame_input_missed;	/* excl. intent. skipped */
	int64_t			frame_output_count;
	int64_t			byte_output_count;
	double			coded_time_elapsed;
	// XXX?
	double			frame_output_rate;	/* invariable, 1/s */
};

/*
 *  Methods of a codec. No fields may change while a codec->class
 *  is referencing this structure.
 */
struct rte_codec_class {
	rte_codec_class *	next;		/* backend use, list of codec classes */
	rte_codec_info		public;

	/*
	 *  Allocate new codec instance. Returns all fields zero except
	 *  rte_codec.class, .status (RTE_STATUS_NEW) and .mutex (initialized).
	 *  Options to be reset by caller, ie. backend.
	 */
	rte_codec *		(* new)(rte_codec_class *, char **errstr);
	void			(* delete)(rte_codec *);

	/*
	 *  Same as frontend version, collectively optional. The frontend
	 *  locks codec->mutex while calling option_set or option_get.
	 */
	rte_option_info *	(* option_enum)(rte_codec *, int);
	int			(* option_get)(rte_codec *, const char *, rte_option_value *);
	int			(* option_set)(rte_codec *, const char *, va_list);
	char *			(* option_print)(rte_codec *, const char *, va_list);

	/*
	 *  Same as frontend versions. parameters_set() is mandatory,
	 *  parameters_get() is optional. Then the frontend or backend reads
	 *  the codec.params directly.
	 */
	rte_bool		(* parameters_set)(rte_codec *, rte_stream_parameters *);
	rte_bool		(* parameters_get)(rte_codec *, rte_stream_parameters *);

	/*
	 *  Select input method and put codec into RTE_STATUS_READY, mandatory
	 *  function. Only certain parameter combinations are valid, depending
	 *  on rte_input:
	 *  RTE_INPUT_CA - read_cb, optional (NULL) unref_cb
	 *  RTE_INPUT_CP - read_cb
	 *  RTE_INPUT_PA - optional (NULL) unref_cb
	 *  RTE_INPUT_PP - nothing
	 *  A codec need not support all input methods. queue_length (input and
	 *  output), the pointer always valid, applies as defined for the
	 *  rte_set_input_*() functions. Return FALSE on error.
	 */
	rte_bool		(* set_input)(rte_codec *, rte_input,
					      rte_buffer_callback read_cb,
					      rte_buffer_callback unref_cb,
					      int *queue_length);

	/* Same as the frontend version, optional. */

	rte_bool		(* push_buffer)(rte_codec *codec, rte_buffer *buffer, rte_bool blocking);

//<<

	rte_status_info *	(* status_enum)(rte_codec *, int);
};

struct rte_context {
	rte_context *		next;		/* backend use, list of context instances */
	rte_context_class *	class;

	void *			user_data;
	char *			error;

	/*
	 *  Except as noted the mutex is locked by the rte_context_class
	 *  functions and protects all fields below. Attention: not recursive.
	 */
	pthread_mutex_t		mutex;

	rte_status		status;


//<<

	rte_seek_callback seek; /* seek in file callback */
};

/*
 *  Methods of a context. No fields may change while a context.class
 *  is referencing this structure.
 */
struct rte_context_class {
	rte_context_class *	next;		/* backend use, list of context classes */
	rte_context_info	public;

	/*
	 *  Allocate new context instance. Returns all fields zero except
	 *  rte_codec.class, .status (RTE_STATUS_NEW) and .mutex (initialized).
	 *  Context options to be reset by the frontend.
	 */
	rte_context *		(* new)(rte_context_class *, char **errstr);
	void			(* delete)(rte_context *);

	/*
	 *  Same as frontend version, collectively optional. The frontend
	 *  locks context->mutex while calling context_option_set or
	 *  context_option_get.
	 */
	rte_option_info *	(* context_option_enum)(rte_context *, int);
	int			(* context_option_get)(rte_context *, const char *,
						       rte_option_value *);
	int			(* context_option_set)(rte_context *, const char *, va_list);
	char *			(* context_option_print)(rte_context *, const char *, va_list);

	/*
	 *  Same as frontend versions, mandatory. Note codec_set:
	 *  Either keyword & index (set/replace) or type & index (remove) are
	 *  passed. Codec options reset by the backend, not replacing if that
	 *  fails. The frontend locks context->mutex while calling codec_set or
	 *  codec_get.
	 */
	rte_codec_info *	(* codec_enum)(rte_context *, int);
	rte_codec *		(* codec_get)(rte_context *, rte_stream_type, int);
	rte_codec *		(* codec_set)(rte_context *, const char *, rte_stream_type, int);

	/*
	 *  Same as frontend versions, insulates codec calls in the
	 *  backend to permit context specific filtering. Individually
	 *  optional, then the frontend calls the respective codec
	 *  functions directly. The frontend locks codec->mutex while
	 *  calling codec_option_set or codec_option_get.
	 */
	rte_option_info *	(* codec_option_enum)(rte_codec *, int);
	int			(* codec_option_get)(rte_codec *, const char *,
						     rte_option_value *);
	int			(* codec_option_set)(rte_codec *, const char *, va_list);
	char *			(* codec_option_print)(rte_codec *, const char *, va_list);

	rte_bool		(* parameters_set)(rte_codec *, rte_stream_parameters *);
	rte_bool		(* parameters_get)(rte_codec *, rte_stream_parameters *);

	/*
	 *  Same as the codec versions, optional. Then the frontend calls the
	 *  respective codec functions directly.
	 */
	rte_bool		(* set_input)(rte_codec *, rte_input,
					      rte_buffer_callback read_cb,
					      rte_buffer_callback unref_cb,
					      int *queue_length);

	rte_bool		(* push_buffer)(rte_codec *codec, rte_buffer *buffer, rte_bool blocking);

	rte_bool		(* set_output)(rte_context *context,
					       rte_buffer_callback write_cb,
					       rte_seek_callback seek_cb);

//<<

	rte_status_info *	(* status_enum)(rte_context *, int);


	rte_bool		(* start)(rte_context *);
	void			(* stop)(rte_context *);
	void			(* pause)(rte_context *);
	rte_bool		(* resume)(rte_context *);
};

struct rte_backend_class {
	char *			name;

	/* Called once by frontend before context_enum. */

	void			(* backend_init)(void);

	/*
	 *  Same as frontend version. Contexts can be inactive due lack of
	 *  resources for example, then (at least) rte_context_class->new
	 *  is NULL and (if non-zero) errstr should explain why.
	 *  rte_context_class->public.keyword must always be valid when
	 *  the class is enumerated, rest don't care if inactive.
	 */
	rte_context_class *	(* context_enum)(int index, char **errstr);
};

/*
 *  Helper functions
 */

#ifndef _
#ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String) gettext (String)
#    ifdef gettext_noop
#        define N_(String) gettext_noop (String)
#    else
#        define N_(String) (String)
#    endif
#else
/* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#endif
#endif

#define RTE_OPTION_BOUNDS_INITIALIZER_(type_, def_, min_, max_, step_)	\
  { type_ = def_ }, { type_ = min_ }, { type_ = max_ }, { type_ = step_ }

#define RTE_OPTION_BOOL_INITIALIZER(key_, label_, def_, tip_)		\
  { RTE_OPTION_BOOL, key_, label_, RTE_OPTION_BOUNDS_INITIALIZER_(	\
  .num, def_, 0, 1, 1),	{ .num = NULL }, tip_ }

#define RTE_OPTION_INT_RANGE_INITIALIZER(key_, label_, def_, min_,	\
  max_,	step_, tip_) { RTE_OPTION_INT, key_, label_,			\
  RTE_OPTION_BOUNDS_INITIALIZER_(.num, def_, min_, max_, step_),	\
  { .num = NULL }, tip_ }

#define RTE_OPTION_INT_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, entries_, tip_) { RTE_OPTION_INT, key_, label_,		\
  RTE_OPTION_BOUNDS_INITIALIZER_(.num, def_, 0, (entries_) - 1, 1),	\
  { .num = menu_ }, tip_ }

#define RTE_OPTION_REAL_RANGE_INITIALIZER(key_, label_, def_, min_,	\
  max_, step_, tip_) { RTE_OPTION_REAL, key_, label_,			\
  RTE_OPTION_BOUNDS_INITIALIZER_(.dbl, def_, min_, max_, step_),	\
  { .dbl = NULL }, tip_ }

#define RTE_OPTION_REAL_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, entries_, tip_) { RTE_OPTION_REAL, key_, label_,		\
  RTE_OPTION_BOUNDS_INITIALIZER_(.num, def_, 0, (entries_) - 1, 1),	\
  { .dbl = menu_ }, tip_ }

#define RTE_OPTION_STRING_INITIALIZER(key_, label_, def_, tip_)		\
  { RTE_OPTION_STRING, key_, label_, RTE_OPTION_BOUNDS_INITIALIZER_(	\
  .str, def_, NULL, NULL, NULL), { .str = NULL }, tip_ }

#define RTE_OPTION_STRING_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, entries_, tip_) { RTE_OPTION_STRING, key_, label_,		\
  RTE_OPTION_BOUNDS_INITIALIZER_(.str, def_, 0, (entries_) - 1, 1),	\
  { .str = menu_ }, tip_ }

#define RTE_OPTION_MENU_INITIALIZER(key_, label_, def_, menu_,		\
  entries_, tip_) { RTE_OPTION_MENU, key_, label_,			\
  RTE_OPTION_BOUNDS_INITIALIZER_(.num, def_, 0, (entries_) - 1, 1),	\
  { .str = menu_ }, tip_ }

#define RTE_OPTION_ARG(type, min, max)					\
({									\
	type val = va_arg(args, type);					\
									\
	if (val < (min) || val > (max)) {				\
		rte_invalid_option(context, codec, keyword, val);	\
		goto failed;						\
	}								\
		val;							\
})

#define RTE_OPTION_ARG_MENU(menu)					\
	RTE_OPTION_ARG(int, 0, sizeof(menu) / sizeof(menu[0]))

void
rte_unknown_option(rte_context *context, rte_codec *codec, const char *keyword);
void
rte_invalid_option(rte_context *context, rte_codec *codec, const char *keyword, ...);
void
rte_asprintf(char **errstr, const char *templ, ...);
char *
rte_strdup(rte_context *context, char **d, const char *s);

rte_bool
rte_option_string(rte_context *context, rte_codec *codec, const char *optstr);

static inline void
rte_error_reset(rte_context *context)
{
	if (context->error) {
		free(context->error);
		context->error = NULL;
	}
}

#define RC(X) ((rte_context*)X)

#define rte_error(context, format, args...) \
do { \
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
} while(0)

#define nullcheck(X, whattodo)						\
do {									\
	if ((X) == NULL) {						\
		rte_error(NULL, #X " == NULL");				\
		whattodo;						\
	}								\
} while (0)

#endif /* rtepriv.h */
