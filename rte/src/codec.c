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

/* $Id: codec.c,v 1.7 2002-08-22 22:09:06 mschimek Exp $ */

#include "config.h"
#include "rtepriv.h"

#define xc context->_class
#define dc codec->_class

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param index Index into the codec table.
 * 
 * Enumerates elementary stream codecs available for the selected
 * context (backend/file format/mux format). You should start at index
 * 0, incrementing.
 * 
 * @return
 * Static pointer, data not to be freed, to a rte_codec_info
 * structure. @c NULL if the @a context is invalid or the @a index is out
 * of bounds.
 */
rte_codec_info *
rte_codec_info_enum(rte_context *context, unsigned int index)
{
	nullcheck(context, return NULL);
	rte_error_reset(context);

	if (!xc->codec_enum)
		return NULL;

	return xc->codec_enum(context, index);
}

/**
 * @param context Initialized rte_context as returned by rte_context_new().
 * @param keyword Codec identifier as in rte_codec_info,
 *   rte_set_codec() or rte_get_codec().
 * 
 * Similar to rte_codec_enum(), but this function attempts to find a
 * codec by keyword.
 * 
 * @return
 * Static pointer to a rte_codec_info structure, @c NULL if the
 * @a context is invalid or the named codec has not been found.
 */
rte_codec_info *
rte_codec_info_by_keyword(rte_context *context, const char *keyword)
{
	rte_codec_info *ci;
	int i, keylen;

	nullcheck(context, return NULL);
	rte_error_reset(context);

	nullcheck(keyword, return NULL);

	if (!xc->codec_enum)
		return NULL;

	for (keylen = 0; keyword[keylen]; keylen++)
		if (keyword[keylen] == ';' || keyword[keylen] == ',')
			break;

	for (i = 0;; i++)
	        if (!(ci = xc->codec_enum(context, i))
		    || strncmp(keyword, ci->keyword, keylen) == 0)
			break;
	return ci;
}

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 *
 * Returns the codec info for the given @a codec.
 *
 * @return
 * Static pointer to a rte_codec_info structure, @c NULL if the
 * @a codec is @c NULL.
 */
rte_codec_info *
rte_codec_info_by_codec(rte_codec *codec)
{
	rte_context *context = NULL;

	nullcheck(codec, return NULL);

	return &dc->_public;
}

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 * 
 * Retrieves the pointer stored in the user data field of the codec.
 * 
 * @return
 * User pointer.
 */
void *
rte_codec_user_data(rte_codec *codec)
{
	rte_context *context = NULL;

	nullcheck(codec, return NULL);

	return codec->user_data;
}

/*
 *  Options
 */

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 * @param index Index into the option table.
 * 
 * Enumerates the options available for the given @a codec.
 * You should start at index 0, incrementing by one.
 *
 * @return
 * Static pointer, data not to be freed, to a rte_option_info
 * structure. @c NULL if the @a index is out of bounds.
 */
rte_option_info *
rte_codec_option_info_enum(rte_codec *codec, unsigned int index)
{
	rte_context *context = NULL;

	nullcheck(codec, return 0);

	context = codec->context;
	rte_error_reset(context);

	if (xc->codec_option_enum)
		return xc->codec_option_enum(codec, index);
	else if (dc->option_enum)
		return dc->option_enum(codec, index);
	else
		return NULL;
}

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 * @param keyword Keyword identifying the option as in rte_option_info.
 * 
 * Similar to rte_codec_option_info_enum() but this function tries
 * to find the option info by keyword.
 * 
 * @return
 * Static pointer to a rte_option_info structure, @c NULL if
 * the @a keyword was not found.
 */
rte_option_info *
rte_codec_option_info_by_keyword(rte_codec *codec, const char *keyword)
{
	rte_context *context = NULL;
	rte_option_info *(* enumerate)(rte_codec *, unsigned int);
	rte_option_info *oi;
	int i;

	nullcheck(codec, return NULL);

	context = codec->context;
	rte_error_reset(context);

	nullcheck(keyword, return NULL);

	if (xc->codec_option_enum)
		enumerate = xc->codec_option_enum;
	else if (dc->option_enum)
		enumerate = dc->option_enum;
	else
		return NULL;

	for (i = 0;; i++)
	        if (!(oi = enumerate(codec, i))
		    || strcmp(keyword, oi->keyword) == 0)
			break;
	return oi;
}

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 * @param keyword Keyword identifying the option as in rte_option_info.
 * @param value A place to store the option value.
 * 
 * This function queries the current value of the option. When the
 * option is a string, you must free() @a value.str when not longer
 * needed.
 *
 * @return
 * @c TRUE on success, otherwise @a value remained unchanged.
 */
rte_bool
rte_codec_option_get(rte_codec *codec, const char *keyword,
		     rte_option_value *value)
{
	rte_context *context = NULL;
	rte_bool r;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	nullcheck(value, return FALSE);

	if (!keyword) {
		rte_unknown_option(context, codec, keyword);
		return FALSE;
	}

	if (xc->codec_option_get) {
		r = xc->codec_option_get(codec, keyword, value);
	} else if (dc->option_get) {
		r = dc->option_get(codec, keyword, value);
	} else {
		rte_unknown_option(context, codec, keyword);
		r = FALSE;
	}

	return r;
}

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 * @param keyword Keyword identifying the option as in rte_option_info.
 * @param Varargs New value to set.
 * 
 * Sets the value of the option. Make sure you are casting the
 * value to the correct type (int, double, char *).
 * 
 * Typical usage is:
 *
 * @code
 * rte_codec_option_set (codec, "frame_rate", (double) 3.141592);
 * @endcode
 * 
 * Note setting an option invalidates prior input stream parameter
 * negotiation with rte_parameters_set(), so you should
 * initialize the codec options first.
 * 
 * @return
 * @c TRUE on success.
 */
rte_bool
rte_codec_option_set(rte_codec *codec, const char *keyword, ...)
{
	rte_context *context = NULL;
	va_list args;
	rte_bool r;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	if (!keyword) {
		rte_unknown_option(context, codec, keyword);
		return FALSE;
	}

	va_start(args, keyword);

	if (xc->codec_option_set) {
		r = xc->codec_option_set(codec, keyword, args);
	} else if (dc->option_set) {
		r = dc->option_set(codec, keyword, args);
	} else {
		rte_unknown_option(context, codec, keyword);
		r = FALSE;
	}

	va_end(args);

	return r;
}

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 * @param keyword Keyword identifying the option as in rte_option_info.
 * @param Varargs Option value.
 *
 * Return a string representation of the option value. When for example
 * the option is a memory size, a value of 2048 may result in a string
 * "2 KB". Make sure you are casting the value to the correct type
 * (int, double, char *). You must free() the returned string when
 * no longer needed.
 *
 * @return
 * String pointer or @c NULL on failure.
 */
char *
rte_codec_option_print(rte_codec *codec, const char *keyword, ...)
{
	rte_context *context = NULL;
	va_list args;
	char *r;

	nullcheck(codec, return NULL);

	context = codec->context;
	rte_error_reset(context);

	if (!keyword) {
		rte_unknown_option(context, codec, keyword);
		return NULL;
	}

	va_start(args, keyword);

	if (xc->codec_option_print) {
		r = xc->codec_option_print(codec, keyword, args);
	} else if (dc->option_print) {
		r = dc->option_print(codec, keyword, args);
	} else {
		rte_unknown_option(context, codec, keyword);
		r = NULL;
	}

	va_end(args);

	return r;
}

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 * @param keyword Keyword identifying the option as in rte_option_info.
 * @param entry A place to store the current menu entry.
 * 
 * Similar to rte_codec_option_get() this function queries the current
 * value of the named option, but returns this value as number of the
 * corresponding menu entry. Naturally this must be an option with
 * menu or the function will fail.
 * 
 * @return 
 * @c TRUE on success, otherwise @a value remained unchanged.
 */
rte_bool
rte_codec_option_menu_get(rte_codec *codec, const char *keyword, int *entry)
{
	rte_context *context = NULL;
	rte_option_info *oi;
	rte_option_value val;
	rte_bool r;
	int i;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	nullcheck(entry, return FALSE);

	if (!(oi = rte_codec_option_info_by_keyword(codec, keyword)))
		return FALSE;

	if (!rte_codec_option_get(codec, keyword, &val))
		return FALSE;

	r = FALSE;

	for (i = 0; i <= oi->max.num; i++) {
		switch (oi->type) {
		case RTE_OPTION_BOOL:
		case RTE_OPTION_INT:
			if (!oi->menu.num)
				return FALSE;
			r = (oi->menu.num[i] == val.num);
			break;

		case RTE_OPTION_REAL:
			if (!oi->menu.dbl)
				return FALSE;
			r = (oi->menu.dbl[i] == val.dbl);
			break;

		case RTE_OPTION_MENU:
			r = (i == val.num);
			break;

		default:
			fprintf(stderr, "rte:%s: unknown export option type %d\n",
				__PRETTY_FUNCTION__, oi->type);
			exit(EXIT_FAILURE);
		}

		if (r) {
			*entry = i;
			break;
		}
	}

	return r;
}

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 * @param keyword Keyword identifying the option as in rte_option_info.
 * @param entry Menu entry to be selected.
 * 
 * Similar to rte_codec_option_set() this function sets the value of
 * the named option, however it does so by number of the corresponding
 * menu entry. Naturally this must be an option with menu, or the
 * function will fail.
 * 
 * Note setting an option invalidates prior sample parameter
 * negotiation with rte_parameters_set(), so you should
 * initialize the codec options first.
 *
 * @return 
 * @c TRUE on success, otherwise the option is not changed.
 */
rte_bool
rte_codec_option_menu_set(rte_codec *codec, const char *keyword, int entry)
{
	rte_context *context = NULL;
	rte_option_info *oi;

	nullcheck(codec, return FALSE);

	if (!(oi = rte_codec_option_info_by_keyword(codec, keyword)))
		return FALSE;

	if (entry < oi->min.num || entry > oi->max.num)
		return FALSE;

	switch (oi->type) {
	case RTE_OPTION_BOOL:
	case RTE_OPTION_INT:
		if (!oi->menu.num)
			return FALSE;
		return rte_codec_option_set(codec,
			keyword, oi->menu.num[entry]);

	case RTE_OPTION_REAL:
		if (!oi->menu.dbl)
			return FALSE;
		return rte_codec_option_set(codec,
			keyword, oi->menu.dbl[entry]);

	case RTE_OPTION_MENU:
		return rte_codec_option_set(codec, keyword, entry);

	default:
		fprintf(stderr, "rte:%s: unknown export option type %d\n",
			__PRETTY_FUNCTION__, oi->type);
		exit(EXIT_FAILURE);
	}
}

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 * 
 * Resets all options of the codec to their respective default, that
 * is the value they have after calling rte_set_codec().
 * 
 * @return 
 * @c TRUE on success, on failure not all options may be reset.
 */
rte_bool
rte_codec_options_reset(rte_codec *codec)
{
	rte_context *context = NULL;
	rte_option_info *oi;
	rte_bool r = TRUE;
	int i;

	nullcheck(codec, return FALSE);

	for (i = 0; r && (oi = rte_codec_option_info_enum(codec, i)); i++) {
		switch (oi->type) {
		case RTE_OPTION_BOOL:
		case RTE_OPTION_INT:
			if (oi->menu.num)
				r = rte_codec_option_set(codec, oi->keyword,
							 oi->menu.num[oi->def.num]);
			else
				r = rte_codec_option_set(codec, oi->keyword,
							 oi->def.num);
			break;

		case RTE_OPTION_REAL:
			if (oi->menu.dbl)
				r = rte_codec_option_set(codec, oi->keyword,
							 oi->menu.dbl[oi->def.num]);
			else
				r = rte_codec_option_set(codec, oi->keyword, 
							 oi->def.dbl);
			break;

		case RTE_OPTION_STRING:
			if (oi->menu.str)
				r = rte_codec_option_set(codec, oi->keyword,
							 oi->menu.str[oi->def.num]);
			else
				r = rte_codec_option_set(codec, oi->keyword, 
							 oi->def.str);
			break;

		case RTE_OPTION_MENU:
			r = rte_codec_option_set(codec, oi->keyword, oi->def.num);
			break;

		default:
			fprintf(stderr, "rte:%s: unknown codec option type %d\n",
				__PRETTY_FUNCTION__, oi->type);
			exit(EXIT_FAILURE);
		}
	}

	return r;
}

/*
 *  Parameters
 */

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 * @param params Parameters describing the source data.
 * 
 * This function is used to negotiate the source data parameters such
 * as image width and height or audio sampling frequency. The @a params
 * structure must be cleared before calling this function, you can
 * propose parameters by setting the respective fields. On return all
 * fields will be set to the nearest possible value, for example
 * the image width and height rounded to a multiple of 16. The cycle
 * can repeat until a suitable set of parameters has been negotiated.
 *
 * Typical usage of rte_parameters_set():
 *
 * @code
 * rte_stream_parameters params;
 * 
 * memset (&params, 0, sizeof(params));
 * 
 * params.video.pixfmt = RTE_PIXFMT_YUYV;
 * params.video.frame_rate = 24.0;
 * params.video.width = 384;
 * params.video.height = 288;
 * 
 * rte_parameters_set (video_codec, &params);
 * @endcode
 *
 * When the codec supports clipping, scaling and resampling
 * you can do this:
 *
 * Video clipping:
 * @code
 * rte_codec_option_set (video_codec, "width", dest_x1 - dest_x0 + 1);
 * rte_codec_option_set (video_codec, "height", dest_y1 - dest_y0 + 1);
 *
 * params.video.stride = source_y1 - source_y0 + 1;
 * params.video.offset = dest_x0 + dest_y0 * params.video.stride;
 * params.video.width  = dest_x1 - dest_x0 + 1;
 * params.video.height = dest_y1 - dest_y0 + 1;
 * @endcode
 *
 * Video scaling:
 * @code
 * rte_codec_option_set (video_codec, "width", dest_x1 - dest_x0 + 1);
 * rte_codec_option_set (video_codec, "height", dest_y1 - dest_y0 + 1);
 *
 * params.video.stride = source_y1 - source_y0 + 1;
 * params.video.offset = 0;
 * params.video.width  = source_x1 - source_x0 + 1;
 * params.video.height = source_y1 - source_y0 + 1;
 * @endcode
 *
 * Audio resampling:
 * @code
 * rte_codec_option_set (audio_codec, "sampling_freq", 44100.0);
 *
 * params.audio.sampling_freq = 32000.0;
 * @endcode
 *
 * @return
 * @c FALSE if the parameters are somehow ambiguous.
 */
rte_bool
rte_parameters_set(rte_codec *codec, rte_stream_parameters *params)
{
	rte_context *context = NULL;
	rte_bool r = FALSE;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	nullcheck(params, return FALSE);

	if (xc->parameters_set)
		r = xc->parameters_set(codec, params);
	else if (dc->parameters_set)
		r = dc->parameters_set(codec, params);
	else
		assert(!"rte bug");

	return r;
}

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 * @param params Parameters describing the source data.
 *
 * Query the negotiated data parameters.
 *
 * @return
 * @c FALSE if no parameters have been negotiated.
 */
rte_bool
rte_parameters_get(rte_codec *codec, rte_stream_parameters *params)
{
	rte_context *context = NULL;
	rte_bool r;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	nullcheck(params, return FALSE);

	if (xc->parameters_get)
		r = xc->parameters_get(codec, params);
	else if (dc->parameters_get)
		r = dc->parameters_get(codec, params);
	else {
		if (codec->state == RTE_STATE_NEW) {
			r = FALSE;
		} else {
			memcpy(params, &codec->params, sizeof(*params));
			r = TRUE;
		}
	}

	return r;
}

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 * @param read_cb Function called by the codec to read more data to encode.
 * @param unref_cb Optional function called by the codec to free the data.
 * @param queue_length When non-zero, the codec queue length is returned here. That
 *   is the maximum number of buffers read before freeing the oldest.
 *   When for example @a read_cb and @a unref_cb calls always pair, this number
 *   is 1.
 *
 * Sets the input mode for the codec and puts the codec into ready state.
 * Using this method, when the @a codec needs more data it will call @a read_cb
 * with a rte_buffer to be initialized by the rte client. After using the
 * data, it is released by calling @a unref_cb. See rte_buffer_callback for
 * the handshake details.
 *
 * @warning A codec may read more than once before freeing the data, and it
 * may also free the data in a different order than it has been read. As of
 * RTE version 0.5, this is <b>the only input method all codecs must implement</b>.
 *
 * Typical usage of rte_set_input_callback_master():
 * @code
 * rte_bool
 * my_read_cb (rte_context *context, rte_codec *codec, rte_buffer *buffer)
 * {
 *         buffer->data = malloc ();
 *         read (buffer->data, &buffer->timestamp);
 *         return TRUE;
 * }
 * 
 * rte_bool
 * my_unref_cb (rte_context *context, rte_codec *codec, rte_buffer *buffer)
 * {
 *         free (buffer->data);
 * }
 * @endcode
 *
 * @return
 * Before selecting an input method you must negotiate sample parameters with
 * rte_parameters_set(), else this function fails with a return value
 * of @c FALSE. Setting codec options invalidates previously selected sample
 * parameters, and thus also the input method selection. The function can
 * also fail when the codec does not support this input method.
 */
rte_bool
rte_set_input_callback_master(rte_codec *codec,
			      rte_buffer_callback read_cb,
			      rte_buffer_callback unref_cb,
			      unsigned int *queue_length)
{
	rte_context *context = NULL;
	rte_bool r = FALSE;
	int ql = 0;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	nullcheck(read_cb, return FALSE);

	if (!queue_length)
		queue_length = &ql;

	if (xc->set_input)
		r = xc->set_input(codec, RTE_CALLBACK_MASTER,
				  read_cb, unref_cb, queue_length);
	else if (dc->set_input)
		r = dc->set_input(codec, RTE_CALLBACK_MASTER,
				  read_cb, unref_cb, queue_length);
	else
		assert(!"codec bug");

	if (r) {
		codec->input_method = RTE_CALLBACK_MASTER;
		codec->input_fd = -1;
	}

	return r;
}

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 * @param read_cb Function called by the codec to read more data to encode.
 *
 * Sets the input mode for the codec and puts the codec into ready state.
 * Using this method the codec allocates the necessary buffers. When it
 * needs more data it calls @a data_cb, passing a pointer to the buffer
 * space where the client shall copy the data.
 *
 * Typical usage of rte_set_input_callback_slave():
 * @code
 * rte_bool
 * my_read_cb (rte_context *context, rte_codec *codec, rte_buffer *buffer)
 * {
 *         read (buffer->data, &buffer->timestamp);
 *         return TRUE;
 * }
 * @endcode
 *
 * @return
 * Before selecting an input method you must negotiate sample parameters with
 * rte_parameters_set(), else this function fails with a return value
 * of @c FALSE. Setting codec options invalidates previously selected sample
 * parameters, and thus also the input method selection. The function can
 * also fail when the codec does not support this input method.
 */
rte_bool
rte_set_input_callback_slave(rte_codec *codec,
			       rte_buffer_callback read_cb)
{
	rte_context *context = NULL;
	rte_bool r = FALSE;
	int ql = 0;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	nullcheck(read_cb, return FALSE);

	if (xc->set_input)
		r = xc->set_input(codec, RTE_CALLBACK_SLAVE, read_cb, NULL, &ql);
	else if (dc->set_input)
		r = dc->set_input(codec, RTE_CALLBACK_SLAVE, read_cb, NULL, &ql);
	else
		assert(!"codec bug");

	if (r) {
		codec->input_method = RTE_CALLBACK_SLAVE;
		codec->input_fd = -1;
	}

	return r;
}

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 * @param unref_cb Optional function called as subroutine of rte_push_buffer()
 *   to free the data.
 * @param queue_request The minimum number of buffers you will be able to push before
 *   rte_push_buffer() blocks.
 * @param queue_length When non-zero the codec queue length is returned here. This is
 *   at least the number of buffers read before freeing the oldest, and at most
 *   @a queue_request. When for example rte_push_buffer() and @a unref_cb calls
 *   always pair, the length is 1.
 *
 * Sets the input mode for the codec and puts the codec into ready state.
 * Using this method, when the codec needs data it waits until the rte client
 * calls rte_push_buffer(). After using the data, it is released by calling
 * @a unref_cb. See rte_buffer_callback for the handshake details.
 *
 * @warning A codec may wait for more than one buffer before releasing the
 * oldest, it may also unref in a different order than has been pushed.
 *
 * Typical usage of rte_set_input_push_master():
 * @code
 * rte_bool
 * my_unref_cb (rte_context *context, rte_codec *codec, rte_buffer *buffer)
 * {
 *         free (buffer->data);
 * }
 * 
 * while (have_data) {
 *         rte_buffer buffer;
 * 
 * 	   buffer.data = malloc ();
 *         read (buffer.data, &buffer.timestamp);
 *         if (!rte_push_buffer (codec, &buffer, FALSE)) {
 *                 // The codec is not fast enough, we drop the frame.
 *                 free (buffer.data);
 *         }
 * }
 * @endcode
 *
 * @return
 * Before selecting an input method you must negotiate sample parameters with
 * rte_parameters_set(), else this function fails with a return value
 * of @c FALSE. Setting codec options invalidates previously selected sample
 * parameters, and thus also the input method selection. The function can
 * also fail when the codec does not support this input method.
 */
rte_bool
rte_set_input_push_master(rte_codec *codec,
			  rte_buffer_callback unref_cb,
			  unsigned int queue_request,
			  unsigned int *queue_length)
{
	rte_context *context = NULL;
	rte_bool r = FALSE;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	if (!queue_length)
		queue_length = &queue_request;
	else
		*queue_length = queue_request;

	if (xc->set_input)
		r = xc->set_input(codec, RTE_PUSH_MASTER,
				  NULL, unref_cb, queue_length);
	else if (dc->set_input)
		r = dc->set_input(codec, RTE_PUSH_MASTER,
				  NULL, unref_cb, queue_length);
	else
		assert(!"codec bug");

	if (r) {
		codec->input_method = RTE_PUSH_MASTER;
		codec->input_fd = -1;
	}

	return r;
}

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 * @param queue_request The minimum number of buffers you will be able to push before
 *   rte_push_buffer() blocks.
 * @param queue_length When non-zero the actual codec queue length is returned here,
 *   this may be more or less than @a queue_request.
 *
 * Sets the input mode for the codec and puts the codec into ready state.
 * Using this method the codec allocates the necessary buffers. When it needs more
 * data it waits until the rte client calls rte_push_buffer(). In buffer->data
 * this function always returns a pointer to buffer space where the rte client
 * shall store the data. You can pass @c NULL as buffer->data to start the cycle.
 *
 * Typical usage of rte_set_input_push_slave():
 * @code
 * rte_buffer buffer;
 * 
 * buffer.data = NULL;
 *
 * rte_push_buffer (codec, &buffer, FALSE); // cannot fail
 * 
 * while (have_data) {
 *         read (buffer.data, &buffer.timestamp);
 *         if (!rte_push_buffer(codec, &buffer, FALSE)) {
 *         // The codec is not fast enough, we drop the frame.
 *         }
 * }
 * @endcode
 *
 * @return
 * Before selecting an input method you must negotiate sample parameters with
 * rte_parameters_set(), else this function fails with a return value
 * of @c FALSE. Setting codec options invalidates previously selected sample
 * parameters, and thus also the input method selection. The function can
 * also fail when the codec does not support this input method.
 */
rte_bool
rte_set_input_push_slave(rte_codec *codec,
			   unsigned int queue_request,
			   unsigned int *queue_length)
{
	rte_context *context = NULL;
	rte_bool r = FALSE;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	if (!queue_length)
		queue_length = &queue_request;
	else
		*queue_length = queue_request;

	if (xc->set_input)
		r = xc->set_input(codec, RTE_PUSH_SLAVE,
				  NULL, NULL, queue_length);
	else if (dc->set_input)
		r = dc->set_input(codec, RTE_PUSH_SLAVE,
				  NULL, NULL, queue_length);
	else
		assert(!"codec bug");

	if (r) {
		codec->input_method = RTE_PUSH_SLAVE;
		codec->input_fd = -1;
	}

	return r;
}

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 * @param buffer Pointer to a rte_buffer, can be @c NULL.
 * @param blocking @c TRUE to enable blocking behaviour.
 * 
 * Passes data for encoding to the codec when the input method is 'push'.
 * When the codec input queue is full and @a blocking is @c TRUE this function waits
 * until space becomes available, when @a blocking is @c FALSE it immediately
 * returns @c FALSE.
 *
 * @return
 * @c FALSE if the function would block. In master push mode and when the function
 * fails, the contents of @a buffer are unmodified on return. Otherwise in
 * slave push mode, buffer->data points to the next buffer space to be filled.
 * You can always obtain a pointer by calling rte_push_buffer() with buffer->data
 * set to @c NULL, in master push mode this does nothing.
 */
rte_bool
rte_push_buffer(rte_codec *codec, rte_buffer *buffer, rte_bool blocking)
{
	rte_context *context = NULL;

	nullcheck(codec, return FALSE);

	context = codec->context;
	rte_error_reset(context);

	if (xc->push_buffer)
		return xc->push_buffer(codec, buffer, blocking);
	else if (dc->push_buffer)
		return dc->push_buffer(codec, buffer, blocking);
	else
		return FALSE;
}

/**
 * @param codec Pointer to a rte_codec returned by rte_get_codec() or rte_set_codec().
 * @param status Status structure to be filled.
 * 
 * Fill a rte_status structure with information about the encoding
 * process. Data applies to this particular codec, not the context as a whole.
 * @see rte_context_status().
 */
static_inline void
rte_codec_status(rte_codec *codec, rte_status *status);
