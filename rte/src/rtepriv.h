/*
 *  MPEG-1 Real Time Encoder lib wrapper api
 *
 *  Copyright (C) 2000, 2001 Iñaki García Etxebarria
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
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

/* $Id: rtepriv.h,v 1.10 2002-02-25 06:22:20 mschimek Exp $ */

#ifndef __RTEPRIV_H__
#define __RTEPRIV_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "rte.h"

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
	 *  context_class.stop()
	 */
	RTE_STATUS_NEW = 0,
	/*
	 *  codec_class.parameters_set()
	 */
	RTE_STATUS_PARAM,
	/*
	 *  codec_class.set_input()
	 *  context_class.set_output()
	 */
	RTE_STATUS_READY,
	/*
	 *  context_class.start()
	 *  (-> stop, -> pause)
	 */
	RTE_STATUS_RUNNING,
	/*
	 *  context_class.pause()
	 *  (-> start, -> stop)
	 */
	RTE_STATUS_PAUSED
} rte_status;

typedef enum {
	RTE_CALLBACK_ACTIVE = 1,
	RTE_CALLBACK_PASSIVE,
	RTE_PUSH_PULL_ACTIVE,
	RTE_PUSH_PULL_PASSIVE,
	RTE_FIFO,
	/* Used by frontend */
	RTE_FILE,
	RTE_STDIO
} rte_io_method;

/*
 *  Codec instance.
 */
struct rte_codec {
	rte_codec *		next;		/* backend use, list of context's codec instances */

	rte_context *		context;	/* parent */
	rte_codec_class *	class;

	void *			user_data;
	int			stream_index;	/* from rte_codec_set() */

	pthread_mutex_t		mutex;

	/*
	 *  Maintained by codec, elsewhere read only. Stream
	 *  parameters are only valid at status RTE_STATUS_PARAM
	 *  or higher.
	 */
	rte_status		status;
	rte_stream_parameters	params;

	/*
	 *  Frontend private
	 */
	rte_io_method		input_method;
	int			input_fd;

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
	 *  Options to be reset by caller, i. e. backend.
	 */
	rte_codec *		(* new)(rte_codec_class *, char **errstr);
	void			(* delete)(rte_codec *);

	/* Same as frontend version, collectively optional. */
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
	 *  on rte_io_method:
	 *  RTE_CALLBACK_ACTIVE   - read_cb, optional (NULL) unref_cb
	 *  RTE_CALLBACK_PASSIVE  - read_cb
	 *  RTE_PUSH_PULL_ACTIVE  - optional (NULL) unref_cb
	 *  RTE_PUSH_PULL_PASSIVE - nothing
	 *  RTE_FIFO              - not defined yet
	 *  A codec need not support all input methods. queue_length (input and
	 *  output), the pointer always valid, applies as defined for the
	 *  rte_set_input_*() functions. Return FALSE on error.
	 */
	rte_bool		(* set_input)(rte_codec *, rte_io_method,
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

	pthread_mutex_t		mutex;

	rte_status		status;

	/*
	 *  Frontend private
	 */
	rte_io_method		output_method;
	int			output_fd;
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

	/* Same as frontend version, collectively optional. */
	rte_option_info *	(* context_option_enum)(rte_context *, int);
	int			(* context_option_get)(rte_context *, const char *,
						       rte_option_value *);
	int			(* context_option_set)(rte_context *, const char *, va_list);
	char *			(* context_option_print)(rte_context *, const char *, va_list);

	/*
	 *  Same as frontend versions, mandatory. Note codec_set:
	 *  Either keyword & index (set/replace) or type & index (remove) are
	 *  passed. Codec options reset by the backend, not replacing if that
	 *  fails.
	 */
	rte_codec_info *	(* codec_enum)(rte_context *, int);
	rte_codec *		(* codec_get)(rte_context *, rte_stream_type, int);
	rte_codec *		(* codec_set)(rte_context *, const char *, rte_stream_type, int);

	/*
	 *  Same as frontend versions, insulates codec calls in the
	 *  backend to permit context specific filtering. Individually
	 *  optional, then the frontend calls the respective codec
	 *  functions directly.
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
	rte_bool		(* set_input)(rte_codec *, rte_io_method,
					      rte_buffer_callback read_cb,
					      rte_buffer_callback unref_cb,
					      int *queue_length);

	rte_bool		(* push_buffer)(rte_codec *codec, rte_buffer *buffer, rte_bool blocking);

	rte_bool		(* set_output)(rte_context *context,
					       rte_buffer_callback write_cb,
					       rte_seek_callback seek_cb);

	/* Same as frontend versions, mandatory. */
	rte_bool		(* start)(rte_context *, double timestamp,
					  rte_codec *sync_ref, rte_bool async);
	rte_bool		(* pause)(rte_context *, double timestamp);
	rte_bool		(* stop)(rte_context *, double timestamp);


//<<

	rte_status_info *	(* status_enum)(rte_context *, int);
};

struct rte_backend_class {
	char *			name;

	/* Called once by frontend before context_enum. */
	void			(* backend_init)(void);

	/*
	 *  Same as frontend version. Contexts can be inactive due to lack
	 *  of resources for example, then (at least) rte_context_class->new
	 *  is NULL and (if non-zero) errstr should explain why.
	 *  rte_context_class->public.keyword must always be valid when
	 *  the class is enumerated, rest don't care if inactive.
	 */
	rte_context_class *	(* context_enum)(int index, char **errstr);
};

/*
 *  Helper functions
 */

/* Localization */

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

/* Option info building */

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

/* Option parsing */

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

extern void			rte_unknown_option(rte_context *context, rte_codec *codec, const char *keyword);
extern void			rte_invalid_option(rte_context *context, rte_codec *codec, const char *keyword, ...);

/* Misc helper functions */

extern void			rte_asprintf(char **errstr, const char *templ, ...);
extern char *			rte_strdup(rte_context *context, char **d, const char *s);

extern rte_bool			rte_option_string(rte_context *context, rte_codec *codec, const char *optstr);

/* Error functions */

static inline void
rte_error_reset(rte_context *context)
{
	if (context->error) {
		free(context->error);
		context->error = NULL;
	}
}

#define IRTF2(x) #x
#define IRTF1(x) IRTF2(x)

#define nullcheck(X, whattodo)						\
do {									\
	if ((X) == NULL) {						\
		const char *s = "rte:" __FILE__ ":" IRTF1(__LINE__) ":"	\
			__PRETTY_FUNCTION__ ": " #X " == NULL.\n";	\
		if (context)						\
			rte_error_printf(context, "%s", s);		\
		else							\
			fprintf(stderr, "%s", s);			\
		whattodo;						\
	}								\
} while (0)

#endif /* __RTEPRIV_H__ */
