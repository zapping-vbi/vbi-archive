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

/* $Id: rtepriv.h,v 1.14 2002-06-14 07:58:47 mschimek Exp $ */

#ifndef __RTEPRIV_H__
#define __RTEPRIV_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

typedef enum {
	/*
	 *  On success of:
	 *  codec_class.new()
	 *  codec_class.option_set()
	 *  context_class.new()
	 *  context_class.option_set()
	 *  context_class.stop()
	 */
	RTE_STATE_NEW = 0,
	/*
	 *  codec_class.parameters_set()
	 */
	RTE_STATE_PARAM,
	/*
	 *  codec_class.set_input()
	 *  context_class.set_output()
	 */
	RTE_STATE_READY,
	/*
	 *  context_class.start()
	 *  (-> stop, -> pause)
	 */
	RTE_STATE_RUNNING,
	/*
	 *  context_class.pause()
	 *  (-> start, -> stop)
	 */
	RTE_STATE_PAUSED
} rte_state;

typedef enum {
	RTE_CALLBACK_ACTIVE = 1,
	RTE_CALLBACK_PASSIVE,
	RTE_PUSH_PULL_ACTIVE,
	RTE_PUSH_PULL_PASSIVE,
	RTE_FIFO,
	/* Used by frontend */
	RTE_FILE,
	RTE_STDIO,
	RTE_DISCARD
} rte_io_method;

#include "context.h"
#include "codec.h"

typedef struct rte_backend_class rte_backend_class;

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
#  ifdef ENABLE_NLS
#    include <libintl.h>
#    define _(String) gettext (String)
#    ifdef gettext_noop
#      define N_(String) gettext_noop (String)
#    else
#      define N_(String) (String)
#    endif
#  else /* Stubs that do something close enough.  */
#    define textdomain(String) (String)
#    define gettext(String) (String)
#    define dgettext(Domain,Message) (Message)
#    define dcgettext(Domain,Message,Type) (Message)
#    define bindtextdomain(Domain,Directory) (Domain)
#    define _(String) (String)
#    define N_(String) (String)
#  endif
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

#define RTE_OPTION_ARG_SAT(type, min, max)				\
({									\
	type val = va_arg(args, type);					\
									\
	if (val < (min)) val = min;					\
	else if (val > (max)) val = max;				\
	val;								\
})

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

#define RTE_2(x) #x
#define RTE_1(x) RTE_2(x)

#define nullcheck(X, whattodo)						\
do {									\
	if ((X) == NULL) {						\
		const char *s = "rte:" __FILE__ ":" RTE_1(__LINE__)	\
				":%s: " #X " == NULL.\n";		\
		if (context)						\
			rte_error_printf(context, s,			\
					 __PRETTY_FUNCTION__);		\
		else							\
			fprintf(stderr, s, __PRETTY_FUNCTION__);	\
		whattodo;						\
	}								\
} while (0)

#endif /* __RTEPRIV_H__ */
