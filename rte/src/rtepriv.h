/*
 *  Real Time Encoding Library - Backend Interface
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

/* $Id: rtepriv.h,v 1.16 2002-08-22 22:10:47 mschimek Exp $ */

#ifndef __RTEPRIV_H__
#define __RTEPRIV_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

/**
 * @addtogroup Backend
 * @{
 */

/**
 * Context or codec state. The state field in rte_context and
 * rte_codec must be set by the context and codec functions
 * as documented below. 
 */
typedef enum {
	/**
	 * Set on success of:
	 * codec_class->_new()
	 * codec_class->option_set()
	 * context_class->_new()
	 * context_class->option_set()
	 * context_class->stop()
	 */
	RTE_STATE_NEW = 0,
	/**
	 * On success of:
	 * codec_class->parameters_set()
	 */
	RTE_STATE_PARAM,
	/**
	 * On success of:
	 * codec_class->set_input()
	 * context_class->set_output()
	 */
	RTE_STATE_READY,
	/**
	 * On success of:
	 * context_class->start()
	 * (-> stop, -> pause)
	 */
	RTE_STATE_RUNNING,
	/**
	 * On success of:
	 * context_class->pause()
	 * (-> start, -> stop)
	 */
	RTE_STATE_PAUSED
} rte_state;

/**
 * I/O mode.
 */
typedef enum {
	RTE_CALLBACK_MASTER = 1,	/**< rte_set_input_callback_master() */
	RTE_CALLBACK_SLAVE,		/**< rte_set_input_callback_slave() */
	RTE_PUSH_MASTER,		/**< rte_set_input_push_master() */
	RTE_PUSH_SLAVE,			/**< rte_set_input_push_slave() */
	RTE_FIFO,			/**< To be defined */
	RTE_FILE,			/**< Used by frontend only */
	RTE_STDIO,			/**< Used by frontend only */
	RTE_DISCARD			/**< Used by frontend only */
} rte_io_method;

#include "context.h"
#include "codec.h"

typedef struct rte_backend_class rte_backend_class;

/**
 * Backend methods.
 */
struct rte_backend_class {
	/**
	 * Name of the backend. This is only used for debugging, not client visible.
	 */
	char *			name;

	/**
	 * Called once by frontend before @p context_enum.
	 */
	void			(* backend_init)(void);

	/**
	 * Same behaviour as its frontend counterpart. Contexts (rather than
	 * the entire backend) can be inactive due to lack of resources for
	 * example, then this function must still return a valid
	 * @p rte_context_class->_public.keyword so we can enumerate the
	 * context and learn about its inavailability. Then @a errstr should
	 * explain what the problem is and @p rte_context_class->_new shall
	 * be @c NULL. Use rte_asprintf() to set @a errstr.
	 */
	rte_context_class *	(* context_enum)(unsigned int index, char **errstr);
};

/*
 *  Helper functions
 */

/* Internationalization */

#ifndef DOXYGEN_SHOULD_SKIP_THIS

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
#    define gettext(Msgid) ((const char *) (Msgid))
#    define dgettext(Domainname, Msgid) ((const char *) (Msgid))
#    define dcgettext(Domainname, Msgid, Category) ((const char *) (Msgid))
#    define ngettext(Msgid1, Msgid2, N) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define dngettext(Domainname, Msgid1, Msgid2, N) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define dcngettext(Domainname, Msgid1, Msgid2, N, Category) \
       ((N) == 1 ? (const char *) (Msgid1) : (const char *) (Msgid2))
#    define textdomain(Domainname) ((const char *) (Domainname))
#    define bindtextdomain(Domainname, Dirname) ((const char *) (Dirname))
#    define bind_textdomain_codeset(Domainname, Codeset) ((const char *) (Codeset))
#    define _(String) (String)
#    define N_(String) (String)
#  endif
#endif

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/* Option info building */

#define RTE_OPTION_BOUNDS_INITIALIZER_(type_, def_, min_, max_, step_)	\
  { type_ = def_ }, { type_ = min_ }, { type_ = max_ }, { type_ = step_ }

/**
 * Helper macro for backends to build option lists. Use like this:
 *
 * @code
 * rte_option_info myinfo = RTE_OPTION_BOOL_INITIALIZER
 *   ("mute", N_("Switch sound on/off"), FALSE, N_("I am a tooltip"));
 * @endcode
 *
 * N_() marks the string for i18n, see info gettext for details.
 */
#define RTE_OPTION_BOOL_INITIALIZER(key_, label_, def_, tip_)		\
  { RTE_OPTION_BOOL, key_, label_, RTE_OPTION_BOUNDS_INITIALIZER_(	\
  .num, def_, 0, 1, 1),	{ .num = NULL }, tip_ }

/**
 * Helper macro for backends to build option lists. Use like this:
 *
 * @code
 * rte_option_info myinfo = RTE_OPTION_INT_RANGE_INITIALIZER
 *   ("sampling", N_("Sampling rate"), 44100, 8000, 48000, 100, NULL);
 * @endcode
 *
 * Here we have no tooltip (@c NULL).
 */
#define RTE_OPTION_INT_RANGE_INITIALIZER(key_, label_, def_, min_,	\
  max_,	step_, tip_) { RTE_OPTION_INT, key_, label_,			\
  RTE_OPTION_BOUNDS_INITIALIZER_(.num, def_, min_, max_, step_),	\
  { .num = NULL }, tip_ }

/**
 * Helper macro for backends to build option lists. Use like this:
 *
 * @code
 * int mymenu[] = { 29, 30, 31 };
 *
 * rte_option_info myinfo = RTE_OPTION_INT_MENU_INITIALIZER
 *   ("days", NULL, 1, mymenu, 3, NULL);
 * @endcode
 *
 * No label and tooltip (@c NULL), i. e. this option is not to be
 * listed in the user interface. Default is entry 1 ("30") of 3 entries. 
 */
#define RTE_OPTION_INT_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, entries_, tip_) { RTE_OPTION_INT, key_, label_,		\
  RTE_OPTION_BOUNDS_INITIALIZER_(.num, def_, 0, (entries_) - 1, 1),	\
  { .num = menu_ }, tip_ }

/**
 * Helper macro for backends to build option lists. Use like
 * RTE_OPTION_INT_RANGE_INITIALIZER(), just with doubles but ints.
 */
#define RTE_OPTION_REAL_RANGE_INITIALIZER(key_, label_, def_, min_,	\
  max_, step_, tip_) { RTE_OPTION_REAL, key_, label_,			\
  RTE_OPTION_BOUNDS_INITIALIZER_(.dbl, def_, min_, max_, step_),	\
  { .dbl = NULL }, tip_ }

/**
 * Helper macro for backends to build option lists. Use like
 * RTE_OPTION_INT_MENU_INITIALIZER(), just with an array of doubles but ints.
 */
#define RTE_OPTION_REAL_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, entries_, tip_) { RTE_OPTION_REAL, key_, label_,		\
  RTE_OPTION_BOUNDS_INITIALIZER_(.num, def_, 0, (entries_) - 1, 1),	\
  { .dbl = menu_ }, tip_ }

/**
 * Helper macro for backends to build option lists. Use like this:
 *
 * @code
 * rte_option_info myinfo = RTE_OPTION_STRING_INITIALIZER
 *   ("comment", N_("Comment"), "bububaba", "Please enter a string");
 * @endcode
 */
#define RTE_OPTION_STRING_INITIALIZER(key_, label_, def_, tip_)		\
  { RTE_OPTION_STRING, key_, label_, RTE_OPTION_BOUNDS_INITIALIZER_(	\
  .str, def_, NULL, NULL, NULL), { .str = NULL }, tip_ }

/**
 * Helper macro for backends to build option lists. Use like this:
 *
 * @code
 * char *mymenu[] = { "txt", "html" };
 *
 * rte_option_info myinfo = RTE_OPTION_STRING_MENU_INITIALIZER
 *   ("extension", "Ext", 0, mymenu, 2, N_("Select an extension"));
 * @endcode
 *
 * Remember this is like RTE_OPTION_STRING_INITIALIZER() in the sense
 * that the rte client can pass any string as option value, not just those
 * proposed in the menu. In contrast a plain menu option as with
 * RTE_OPTION_MENU_INITIALIZER() expects menu indices as input.
 */
#define RTE_OPTION_STRING_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, entries_, tip_) { RTE_OPTION_STRING, key_, label_,		\
  RTE_OPTION_BOUNDS_INITIALIZER_(.str, def_, 0, (entries_) - 1, 1),	\
  { .str = menu_ }, tip_ }

/**
 * Helper macro for backends to build option lists. Use like this:
 *
 * @code
 * char *mymenu[] = { N_("Monday"), N_("Tuesday") };
 *
 * rte_option_info myinfo = RTE_OPTION_MENU_INITIALIZER
 *   ("weekday", "Weekday, 0, mymenu, 2, N_("Select a weekday"));
 * @endcode
 */
#define RTE_OPTION_MENU_INITIALIZER(key_, label_, def_, menu_,		\
  entries_, tip_) { RTE_OPTION_MENU, key_, label_,			\
  RTE_OPTION_BOUNDS_INITIALIZER_(.num, def_, 0, (entries_) - 1, 1),	\
  { .str = menu_ }, tip_ }

/* Option parsing */

/**
 * Helper macro for backends to parse options. Use like this:
 *
 * @code
 * myfunc (va_list args)
 * {
 *   int myval = RTE_OPTION_ARG (int, -100, +200);
 *
 *   :
 *
 *   failed:
 *     return FALSE;
 * }
 * @endcode
 */
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

/**
 * Helper macro for backends to parse options. Use like this:
 *
 * @code
 * char *mymenu[] = { N_("Monday"), N_("Tuesday") };
 *
 * myfunc (va_list args)
 * {
 *   int myval = RTE_OPTION_ARG_MENU (mymenu); // fails if not 0 ... 1
 *
 *   :
 *
 *   failed:
 *     return FALSE;
 * }
 * @endcode
 */
#define RTE_OPTION_ARG_MENU(menu)					\
	RTE_OPTION_ARG(int, 0, sizeof(menu) / sizeof(menu[0]))

/**
 * Helper macro for backends to parse options. Same as
 * RTE_OPTION_ARG(), but saturates argument to min...max instead
 * of failing.
 */
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
extern unsigned int		rte_closest_int(const int *vec, unsigned int len, int val);
extern unsigned int		rte_closest_double(const double *vec, unsigned int len, double val);
extern rte_bool			rte_option_string(rte_context *context, rte_codec *codec, const char *optstr);

/* Error functions */

/**
 * @internal
 */
static_inline void
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

/** @} */

#endif /* __RTEPRIV_H__ */
