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

/* $Id: context.h,v 1.12 2005-02-25 18:16:51 mschimek Exp $ */

#ifndef CONTEXT_H
#define CONTEXT_H

#include "option.h"

/* Public */

#include <inttypes.h>

/**
 * @ingroup Context
 * Opaque rte_context object. You can allocate an rte_context with
 * rte_context_new().
 */
typedef struct rte_context rte_context;

/**
 * @ingroup Context
 * Details about the encoding context.
 */
typedef struct {
	/**
	 * Uniquely identifies the context, may be stored
	 * in a config file. The keyword should include a
	 * backend identifier to keep the namespace clean, e. g.
	 * "mybackend_mpeg1". As all keywords, the string
	 * must consist of ASCII upper- and lowercase, digits
	 * and underscore only, is otherwise arbitrary.
	 */
	const char *		keyword;

	/**
	 * Uniquely identifies the backend (like "foobarlib 1.2.3"),
	 * not internationalized because a proper name. This may
	 * be used in the user interface. Only ASCII permitted.
	 */
	const char *		backend;

	/**
	 * @p label is a name for the context to be presented to the user,
	 *  can be localized with dgettext("rte", label).
	 */
	const char *		label;

	/**
	 * Gives additional info for the user, also localized.
	 *   This pointer can be @c NULL.
	 */
	const char *		tooltip;

	/**
	 * Gives a MIME type for the created file if applicable,
	 *   otherwise @c NULL. Example: "video/x-mpeg".
	 */
	const char *		mime_type;

	/**
	 * Suggests filename extensions to use. Can be @c NULL,
	 * and can contain multiple strings separated by comma. The first
	 *   string is preferred. Example: "mpg,mpeg".
	 */
	const char *		extension;

	/**
	 * @p min_elementary and @p max_elementary are the required and
	 *   permitted number of elementary streams of each rte_stream_type.
	 *   For example min_elementary [RTE_STREAM_VIDEO] = 0 and
	 *   max_elementary [RTE_STREAM_VIDEO] = 32 means: The file format requires
	 *   no video elementary stream and permits at most 32. You can only
	 *   rte_set_codec() codecs for elementary stream 0 ... max_elementary[] - 1.
	 *   Implied is the rule that each context requires at least one
	 *   codec / elementary stream, regardless of the min_elementary[] values.
	 */
	uint8_t			min_elementary[16];
	uint8_t			max_elementary[16];

	/**
	 * A set of RTE_FLAG_SEEK et al values.
	 */
	unsigned int		flags;
} rte_context_info;

/**
 * @ingroup Context
 * @name rte_context_info flags
 * @{
 */
/**
 * The context needs to seek back, i. e. it is not possible to
 * stream the output, it must be stored on a rewritable medium
 * until encoding is stopped. For instance this applies to the
 * RIFF-WAVE format.
 */
#define RTE_FLAG_SEEKS		(1 << 0)
/** @} */

/**
 * @addtogroup Context
 * @{
 **/
extern rte_context_info *	rte_context_info_enum(unsigned int index);
extern rte_context_info *	rte_context_info_by_keyword(const char *keyword);
extern rte_context_info *	rte_context_info_by_context(rte_context *context);

extern rte_context *		rte_context_new(const char *keyword, void *user_data, char **errstr);
extern void			rte_context_delete(rte_context *context);

extern void *			rte_context_user_data(rte_context *context);
/** @} */

/**
 * @addtogroup Option
 * @{
 **/
extern rte_option_info *	rte_context_option_info_enum(rte_context *context, unsigned int index);
extern rte_option_info *	rte_context_option_info_by_keyword(rte_context *context, const char *keyword);
extern rte_bool			rte_context_option_get(rte_context *context, const char *keyword, rte_option_value *value);
extern rte_bool			rte_context_option_set(rte_context *context, const char *keyword, ...);
extern char *			rte_context_option_print(rte_context *context, const char *keyword, ...);
extern rte_bool			rte_context_option_menu_get(rte_context *context, const char *keyword, unsigned int *entry);
extern rte_bool			rte_context_option_menu_set(rte_context *context, const char *keyword, unsigned int entry);
extern rte_bool			rte_context_options_reset(rte_context *context);
/** @} */

#ifndef DOXYGEN_SHOULD_SKIP_THIS
struct rte_codec; /* forward */
extern void			rte_status_query(rte_context *context, struct rte_codec *codec, rte_status *status, unsigned int size);
#endif

/**
 * @addtogroup Status
 * @{
 **/
static_inline void
rte_context_status(rte_context *context, rte_status *status)
{
	rte_status_query(context, 0, status, sizeof(rte_status));
}
/** @} */

/**
 * @addtogroup Error
 * @{
 **/
extern void
rte_error_printf		(rte_context *		context,
				 const char *		templ,
				 ...)
  __attribute__ ((format (printf, 2, 3)));
extern char *			rte_errstr(rte_context *context);
/** @} */

/* Private */

#include "codec.h"
#include "rte.h"

/**
 * @ingroup Backend
 * Part of the backend interface.
 */
typedef struct rte_context_class rte_context_class;

/**
 * @ingroup Backend
 * Context instance.
 * Part of the backend interface.
 */
struct rte_context {
	rte_context *		next;		/**< Backend use, list of context instances */

	/**
	 * Points back to parent class, to be set
	 * by the backend when creating the context.
	 */
	rte_context_class *	_class;

	void *			user_data;	/**< Frontend private */

	/**
	 * Frontend private. Backends must call rte_error_printf() or
	 * other helper functions to store an error string.
	 */
	char *			error;

	pthread_mutex_t		mutex;		/**< Backend/context use */

	/**
	 * The context module shall set this value as described
	 * in the rte_state documentation. Elsewhere read only.
	 */
	rte_state		state;

	rte_io_method		output_method;	/**< Frontend private */
	int			output_fd0;	/**< Frontend private */
	int			output_fdn;	/**< Frontend private */

	int64_t			fsize_limit;	/**< Frontend private */

	int64_t			part_size;	/**< Frontend private */
	unsigned int		part_count;	/**< Frontend private */

	char *			filename;	/**< Frontend private */
};

/**
 * @ingroup Backend
 * Methods of a context. No fields may change while a context->_class
 * is referencing this structure. Part of the backend interface.
 */
struct rte_context_class {
	rte_context_class *	next;		/**< Backend use, list of context classes */

	/**
	 * Backends can use this to store rte_context_info. The
	 * field is not directly accessed by the frontend.
	 */
	rte_context_info *	_public;

	/**
	 * Allocate new rte_context instance. Returns all fields zero except
	 * rte_context->_class, ->state (RTE_STATE_NEW) and ->mutex (initialized).
	 * Context options are reset by the frontend.
	 *
	 * When the allocation fails, return @c NULL and set @p errstr to a
	 * description of the problem for the user. Use rte_asprintf().
	 * After the initialization of rte_context succeeded, the backend/context
	 * module shall call rte_error_printf() et al to store error strings.
	 */
	rte_context *		(* _new)(rte_context_class *, char **errstr);
	/**
	 * Delete context and all data associated with it. Don't forget to
	 * uninitialize the mutex.
	 */
	void			(* _delete)(rte_context *);

	/**
	 * All the context_option functions behave as their frontend
	 * counterparts, and they are collectively optional (@c NULL).
	 */
	rte_option_info *	(* context_option_enum)(rte_context *, unsigned int);
	rte_bool		(* context_option_get)(rte_context *, const char *,
						       rte_option_value *);
	rte_bool		(* context_option_set)(rte_context *, const char *, va_list);
	char *			(* context_option_print)(rte_context *, const char *, va_list);

	/**
	 * The codec_enum, codec_get, codec_set functions behave as the frontend
	 * versions and are all mandatory.
	 */
	rte_codec_info *	(* codec_enum)(rte_context *, unsigned int);
	rte_codec *		(* codec_get)(rte_context *, rte_stream_type, unsigned int);
	/**
	 * Note either keyword and index (set/replace) or type and index (remove) are
	 * passed. Codec options are reset by the caller.
	 * When replacing a codec fails, the old one must remain unchanged.
	 */
	rte_codec *		(* codec_set)(rte_context *, const char *, rte_stream_type, unsigned int);

	/**
	 * All the codec_option functions behave as the frontend
	 * versions. Using these hooks backends can override codec option
	 * calls. Individually optional, then the frontend calls the
	 * respective codec function directly.
	 */
	rte_option_info *	(* codec_option_enum)(rte_codec *, unsigned int);
	rte_bool		(* codec_option_get)(rte_codec *, const char *,
						     rte_option_value *);
	rte_bool		(* codec_option_set)(rte_codec *, const char *, va_list);
	char *			(* codec_option_print)(rte_codec *, const char *, va_list);

	/**
	 * All the parameters_set and parameters_get functions behave as
	 * the frontend versions. Using these hooks backends can override
	 * codec parameter calls. Individually optional, then the frontend
	 * calls the respective codec function directly. When neither
	 * the context nor the codec defines a parameters_get function
	 * and rte_codec->state is RTE_STATE_PARAM or higher, the frontend will
	 * access the rte_codec->params field directly.
	 */
	rte_bool		(* parameters_set)(rte_codec *, rte_stream_parameters *);
	rte_bool		(* parameters_get)(rte_codec *, rte_stream_parameters *);

	/**
	 * The I/O functions set_input and push_buffer work as the codec versions.
	 * Both are optional, then the frontend calls the respective codec
	 * function directly.
	 */
	rte_bool		(* set_input)(rte_codec *, rte_io_method,
					      rte_buffer_callback read_cb,
					      rte_buffer_callback unref_cb,
					      unsigned int *queue_length);

	rte_bool		(* push_buffer)(rte_codec *codec, rte_buffer *buffer, rte_bool blocking);

	/**
	 * See rte_set_output_callback_slave().
	 */
	rte_bool		(* set_output)(rte_context *context,
					       rte_buffer_callback write_cb,
					       rte_seek_callback seek_cb);

	/**
	 * The start, pause and stop functions behave as their frontend versions. The
	 * pause function is not implemented yet, the other are mandatory.
	 */
	rte_bool		(* start)(rte_context *, double timestamp,
					  rte_codec *sync_ref, rte_bool async);
	rte_bool		(* pause)(rte_context *, double timestamp);
	rte_bool		(* stop)(rte_context *, double timestamp);

	/**
	 * When the codec parameter is @c NULL poll the context status,
	 * else the codec status. Only the first @a size bytes of
	 * rte_status shall be written, size is guaranteed to cover
	 * rte_status->valid. This function is optional.
	 */
	void			(* status)(rte_context *, rte_codec *,
					   rte_status *, unsigned int size);
};

#endif /* CONTEXT_H */
