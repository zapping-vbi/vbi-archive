/*
 *  Real Time Encoding Library
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

/* $Id: context.h,v 1.1 2002-03-16 16:35:37 mschimek Exp $ */

#ifndef CONTEXT_H
#define CONTEXT_H

#include "option.h"

/* Public */

/**
 * rte_context:
 *
 * Opaque rte_context object. You can allocate an rte_context with
 * rte_context_new().
 **/
typedef struct rte_context rte_context;

typedef struct {
  char *		keyword;	/* eg. "mp1e-mpeg1-ps" */
  char *		backend;	/* no NLS b/c proper name */

  char *		label;		/* gettext()ized _N() */
  char *		tooltip;	/* or NULL, gettext()ized _N() */

  /*
   *  Multiple strings allowed, separated by comma. The first
   *  string is preferred. Ex "video/x-mpeg", "mpg,mpeg".
   */
  char *		mime_type;	/* or NULL */
  char *		extension;	/* or NULL */

  /*
   *  Permitted number of elementary streams of each type, for example
   *  MPEG-1 PS: video 0-16, audio 0-32, sliced vbi 0-1, to select rte_codec_set
   *  substream number 0 ... n-1.
   */
  char			min_elementary[16];
  char			max_elementary[16];

/* should we have flags like can pause, needs seek(), syncs, etc? */

} rte_context_info;

extern rte_context_info *	rte_context_info_enum(int index);
extern rte_context_info *	rte_context_info_keyword(const char *keyword);
extern rte_context_info *	rte_context_info_context(rte_context *context);

extern rte_context *		rte_context_new(const char *keyword, void *user_data, char **errstr);
extern void			rte_context_delete(rte_context *context);

extern void *			rte_context_user_data(rte_context *context);

extern rte_option_info *	rte_context_option_info_enum(rte_context *context, int index);
extern rte_option_info *	rte_context_option_info_keyword(rte_context *context, const char *keyword);
extern rte_bool			rte_context_option_get(rte_context *context, const char *keyword, rte_option_value *value);
extern rte_bool			rte_context_option_set(rte_context *context, const char *keyword, ...);
extern char *			rte_context_option_print(rte_context *context, const char *keyword, ...);
extern rte_bool			rte_context_option_menu_get(rte_context *context, const char *keyword, int *entry);
extern rte_bool			rte_context_option_menu_set(rte_context *context, const char *keyword, int entry);
extern rte_bool			rte_context_options_reset(rte_context *context);

extern rte_status_info *	rte_context_status_enum(rte_context *context, int n);
extern rte_status_info *	rte_context_status_keyword(rte_context *context, const char *keyword);

extern void			rte_error_printf(rte_context *context, const char *templ, ...);
extern char *			rte_errstr(rte_context *context);

/* Private */

#include "codec.h"
#include "rte.h"

typedef struct rte_context_class rte_context_class;

/*
 *  Context instance.
 */
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

#endif /* CONTEXT_H */
