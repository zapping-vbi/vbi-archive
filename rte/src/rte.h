/*
 *  Real Time Encoder
 *
 *  Copyright (C) 2000-2001 Iñaki García Etxebarria
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
#ifndef __RTE_H__
#define __RTE_H__

/* Public */

/*
  A nice set of HTML rendered docs can be found here:
  http://zapping.sf.net/docs/rte/index.html
  FIXME: Upload docs before the release.
  FIXME: Document unions.
  FIXME: Document codec status.
*/

#include "rte-enums.h"
#include "rte-types.h"
#include "rte-version.h"

/* Context */

extern rte_context_info *	rte_context_info_enum(int index);
extern rte_context_info *	rte_context_info_keyword(const char *keyword);
extern rte_context_info *	rte_context_info_context(rte_context *context);

extern rte_context *		rte_context_new(const char *keyword, char **errstr, void *user_data);
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

/* Codec */

extern rte_codec_info *		rte_codec_info_enum(rte_context *context, int index);
extern rte_codec_info *		rte_codec_info_keyword(rte_context *context, const char *keyword);
extern rte_codec_info *		rte_codec_info_codec(rte_codec *codec);

extern rte_codec *		rte_codec_set(rte_context *context, const char *keyword, int stream_index, void *user_data);
extern void			rte_codec_remove(rte_context *context, rte_stream_type stream_type, int stream_index);
extern rte_codec *		rte_codec_get(rte_context *context, rte_stream_type stream_type, int stream_index);

extern void *			rte_codec_user_data(rte_codec *codec);

extern rte_option_info *	rte_codec_option_info_enum(rte_codec *codec, int index);
extern rte_option_info *	rte_codec_option_info_keyword(rte_codec *codec, const char *keyword);
extern rte_bool			rte_codec_option_get(rte_codec *codec, const char *keyword, rte_option_value *v);
extern rte_bool			rte_codec_option_set(rte_codec *codec, const char *keyword, ...);
extern char *			rte_codec_option_print(rte_codec *codec, const char *keyword, ...);
extern rte_bool			rte_codec_option_menu_get(rte_codec *codec, const char *keyword, int *entry);
extern rte_bool			rte_codec_option_menu_set(rte_codec *codec, const char *keyword, int entry);
extern rte_bool			rte_codec_options_reset(rte_codec *codec);

extern rte_bool			rte_codec_parameters_set(rte_codec *codec, rte_stream_parameters *rsp);
extern rte_bool			rte_codec_parameters_get(rte_codec *codec, rte_stream_parameters *rsp);

extern rte_status_info *	rte_codec_status_enum(rte_codec *codec, int n);
extern rte_status_info *	rte_codec_status_keyword(rte_codec *codec, const char *keyword);

/* I/O */

extern rte_bool			rte_set_input_callback_active(rte_codec *codec, rte_buffer_callback read_cb, rte_buffer_callback unref_cb, int *queue_length);
extern rte_bool			rte_set_input_callback_passive(rte_codec *codec, rte_buffer_callback read_cb);
extern rte_bool			rte_set_input_push_active(rte_codec *codec, rte_buffer_callback unref_cb, int queue_request, int *queue_length);
extern rte_bool			rte_set_input_push_passive(rte_codec *codec, int queue_request, int *queue_length);

extern rte_bool			rte_set_output_callback(rte_context *context, rte_buffer_callback write_cb, rte_seek_callback seek_cb);
/* active, passive, pull? */

extern rte_bool			rte_push_buffer(rte_codec *codec, rte_buffer *buffer, rte_bool blocking);




/**
 * rte_status_free:
 * @status: Pointer to a rte_status object, or %NULL.
 *
 * Frees all the memory associated with @status. Does nothing if
 * @status is %NULL.
 **/
void
rte_status_free(rte_status_info *status);

/**
 * rte_start:
 * @context: Initialized rte_context
 *
 * Tries to start encoding.
 *
 * Return value: %TRUE if everything worked, %FALSE otherwise.
 **/
rte_bool
rte_start(rte_context *context);

/**
 * rte_stop:
 * @context: Initialized rte_context.
 *
 * Stops encoding.
 **/
void
rte_stop(rte_context *context);

/**
 * rte_pause:
 * @context: Initialized rte_context.
 *
 * Pauses recording.
 **/
void
rte_pause(rte_context *context);

/**
 * rte_resume:
 * @context: Initialized rte_context.
 *
 * Resumes a paused recording.
 *
 * Return value: %TRUE if the recording could be resumed, FALSE otherwise.
 **/
rte_bool
rte_resume(rte_context *context);

/* Private */

#endif /* rte.h */
