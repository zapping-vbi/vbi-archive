/*
 *  Real Time Encoder
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

/* $Id: codec.c,v 1.2 2002-02-25 06:22:20 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "rtepriv.h"

#define xc context->class
#define dc codec->class

/**
 * rte_codec_info_enum:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @index: Index into the codec table.
 * 
 * Enumerates elementary stream codecs available for the selected
 * backend / file format / mux format. You should start at index
 * 0, incrementing.
 * 
 * Return value:
 * Static pointer, data not to be freed, to a #rte_codec_info
 * structure, %NULL if the context is invalid or the index is out
 * of bounds.
 **/
rte_codec_info *
rte_codec_info_enum(rte_context *context, int index)
{
	nullcheck(context, return NULL);
	rte_error_reset(context);

	if (!xc->codec_enum)
		return NULL;

	return xc->codec_enum(context, index);
}

/**
 * rte_codec_info_keyword:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @keyword: Codec identifier as in #rte_codec_info,
 *   rte_codec_get() or rte_codec_set().
 * 
 * Similar to rte_codec_enum(), but this function attempts to find a
 * codec by keyword.
 * 
 * Return value:
 * Static pointer to a #rte_codec_info structure, %NULL if the
 * context is invalid or the named codec has not been found.
 **/
rte_codec_info *
rte_codec_info_keyword(rte_context *context,
		       const char *keyword)
{
	rte_codec_info *ci;
	int i;

	nullcheck(context, return NULL);
	rte_error_reset(context);

	nullcheck(keyword, return NULL);

	if (!xc->codec_enum)
		return NULL;

	for (i = 0;; i++)
	        if (!(ci = xc->codec_enum(context, i))
		    || strcmp(keyword, ci->keyword) == 0)
			break;
	return ci;
}

/**
 * rte_codec_info_codec:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 *
 * Returns the codec info for the given @codec.
 *
 * Return value:
 * Static pointer to a #rte_codec_info structure, %NULL if the
 * @codec is %NULL.
 **/
rte_codec_info *
rte_codec_info_codec(rte_codec *codec)
{
	rte_context *context = NULL;

	nullcheck(codec, return NULL);

	return &dc->public;
}

/**
 * rte_codec_set:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @keyword: Codec identifier as in #rte_codec_info.
 * @stream_index: Elementary stream number.
 * @user_data: Pointer stored in the codec, can be retrieved
 *   with rte_codec_user_data().
 * 
 * Assigns the codec to encode data for the elementary stream of the
 * codec's type and @stream_index of this @context. The stream number refers
 * for example to one of the 16 video or 32 audio streams in a MPEG-1 program
 * stream. The required and permitted number of elementary streams of each
 * type is available from #rte_context_info. The first and default stream has
 * index number 0. Naturally a context needs at least one elementary stream.
 * When you already selected a codec for this stream it will be replaced.
 * All properties of the new codec instance are reset to their defaults.
 *
 * <example><title>Possible mp1e backend initialization (error checks omitted)
 * </title><programlisting>
 * context = rte_context_new("mp1e_mpeg1_ps", NULL, NULL);  // MPEG-1 Program stream
 * rte_codec_set(context, "mp1e_mpeg1_video", 0, NULL);     // MPEG-1 Video (first elementary)
 * rte_codec_set(context, "mp1e_mpeg2_layer2", 0, NULL);    // MPEG-2 Audio (first elementary)
 * rte_codec_set(context, "mp1e_mpeg1_layer2", 1, NULL);    // MPEG-1 Audio (second elementary)
 * </programlisting></example>
 * 
 * As a special service you can set <emphasis>codec</> options
 * by appending to the keyword like this:
 * 
 * <informalexample><programlisting>
 * rte_codec_set(context, 0, "mp1e_mpeg2_layer_2; bit_rate=128000, comment=\"example\"");
 * </programlisting></informalexample>
 * 
 * Return value: 
 * Static pointer, data not to be freed, to an opaque #rte_codec object.
 * On error %NULL is returned, which may be caused by invalid parameters, an
 * unknown @codec_keyword, a stream type of the codec not suitable for the
 * context or an invalid option string. See also rte_errstr(). 
 **/
rte_codec *
rte_codec_set(rte_context *context, const char *keyword,
	      int stream_index, void *user_data)
{
	char key[256];
	rte_codec *codec;
	int keylen;

	nullcheck(context, return NULL);
	rte_error_reset(context);

	nullcheck(keyword, return NULL);

	for (keylen = 0; keyword[keylen] && keylen < (sizeof(key) - 1)
	     && keyword[keylen] != ';' && keyword[keylen] != ','; keylen++)
	     key[keylen] = keyword[keylen];
	key[keylen] = 0;

	assert(xc->codec_set != NULL);

	codec = xc->codec_set(context, key, 0, stream_index);

	if (codec) {
		codec->user_data = user_data;

		if (keyword[keylen] && !rte_option_string(
			context, codec, keyword + keylen + 1)) {
			xc->codec_set(context, NULL,
				      codec->class->public.stream_type,
				      codec->stream_index);
			codec = NULL;
		}
	}

	return codec;
}

/*
 *  Removed rte_codec_set_user_data because when we set only
 *  at rte_codec_set() we can save codec->mutex locking on
 *  every access. Then again this whole locking business
 *  sucks, removed.
 */

/**
 * rte_codec_user_data:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * 
 * Retrieves the pointer stored in the user data field of the codec.
 * 
 * Return value:
 * User pointer.
 **/
void *
rte_codec_user_data(rte_codec *codec)
{
	rte_context *context = NULL;

	nullcheck(codec, return NULL);

	return codec->user_data;
}

/**
 * rte_codec_remove:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @stream_type: RTE_STREAM_VIDEO, RTE_STREAM_AUDIO, ...
 * @stream_index: Elementary stream number.
 * 
 * Removes the codec previously assigned with rte_codec_set() to encode
 * the respective elementary stream type and number.
 **/
void
rte_codec_remove(rte_context *context,
		 rte_stream_type stream_type,
		 int stream_index)
{
	nullcheck(context, return);
	rte_error_reset(context);

	assert(xc->codec_set != NULL);

	xc->codec_set(context, NULL, stream_type, stream_index);
}

/**
 * rte_codec_remove_codec:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * 
 * Similar to rte_codec_remove(), but this function removes the
 * @codec from the stream type and number it has been assigned to.
 **/
void
rte_codec_remove_codec(rte_codec *codec)
{
	rte_context *context = NULL;

	nullcheck(codec, return);

	context = codec->context;
	rte_error_reset(context);

	assert(xc->codec_set != NULL);

	xc->codec_set(context, NULL,
		      codec->class->public.stream_type,
		      codec->stream_index);
}

/**
 * rte_codec_get:
 * @context: Initialized #rte_context as returned by rte_context_new().
 * @stream_type: RTE_STREAM_VIDEO, RTE_STREAM_AUDIO, ...
 * @stream_index: Elementary stream number.
 *
 * Returns a pointer to the #rte_codec assigned with rte_codec_set()
 * to encode this elementary stream. This is the same pointer
 * rte_codec_set() returns.
 *
 * Return value:
 * Static pointer to an opaque #rte_codec object, %NULL if no
 * codec has been assigned or the codec has been removed with
 * rte_codec_remove().
 **/
rte_codec *
rte_codec_get(rte_context *context,
	      rte_stream_type stream_type,
	      int stream_index)
{
	rte_codec *codec;

	nullcheck(context, return NULL);
	rte_error_reset(context);

	assert(xc->codec_get != NULL);

	codec = xc->codec_get(context, stream_type, stream_index);

	return codec;
}

/*
 *  Options
 */

/**
 * rte_codec_option_info_enum:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * @index: Index into the option table.
 * 
 * Enumerates the options available of the given codec.
 * You should start at index 0, incrementing by one.
 *
 * Return value:
 * Static pointer, data not to be freed, to a #rte_option_info
 * structure. %NULL if the @index is out of bounds.
 **/
rte_option_info *
rte_codec_option_info_enum(rte_codec *codec, int index)
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
 * rte_codec_option_info_keyword:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * @keyword: Keyword identifying the option as in #rte_option_info.
 * 
 * Similar to rte_codec_option_info_enum() but this function tries
 * to find the option info by keyword.
 * 
 * Return value:
 * Static pointer to a #rte_option_info structure, %NULL if
 * the keyword was not found.
 **/
rte_option_info *
rte_codec_option_info_keyword(rte_codec *codec, const char *keyword)
{
	rte_context *context = NULL;
	rte_option_info *(* enumerate)(rte_codec *, int);
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
 * rte_codec_option_get:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * @keyword: Keyword identifying the option as in #rte_option_info.
 * @value: A place to store the option value.
 * 
 * This function queries the current value of the option. When the
 * option is a string, you must free() @value.str when not longer
 * needed.
 *
 * Return value:
 * %TRUE on success, otherwise @value remained unchanged.
 **/
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
 * rte_codec_option_set:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * @keyword: Keyword identifying the option as in #rte_option_info.
 * @Varargs: New value to set.
 * 
 * Sets the value of the option. Make sure you are casting the
 * value to the correct type (int, double, char *).
 * 
 * Typical usage is:
 * <informalexample><programlisting>
 * rte_codec_option_set(codec, "frame_rate", (double) 3.141592);
 * </programlisting></informalexample>
 * 
 * Note setting an option invalidates prior sample parameter
 * negotiation with rte_codec_set_parameters(), so you should
 * initialize the codec options first.
 * 
 * Return value:
 * %TRUE on success.
 **/
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
 * rte_codec_option_print:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * @keyword: Keyword identifying the option as in #rte_option_info.
 * @Varargs: Option value.
 *
 * Return a string representation of the option value. When for example
 * the option is a memory size, a value of 2048 may result in a string
 * "2 KB". Make sure you are casting the value to the correct type
 * (int, double, char *). You must free() the returned string when
 * no longer needed.
 *
 * Return value:
 * String pointer or %NULL on failure.
 **/
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
 * rte_codec_option_menu_get:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * @keyword: Keyword identifying the option as in #rte_option_info.
 * @entry: A place to store the current menu entry.
 * 
 * Similar to rte_codec_option_get() this function queries the current
 * value of the named option, but returns this value as number of the
 * corresponding menu entry. Naturally this must be an option with
 * menu or the function will fail.
 * 
 * Return value: 
 * %TRUE on success, otherwise @value remained unchanged.
 **/
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

	if (!(oi = rte_codec_option_info_keyword(codec, keyword)))
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
			fprintf(stderr, "rte:" __PRETTY_FUNCTION__
				": unknown export option type %d\n", oi->type);
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
 * rte_codec_option_menu_set:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * @keyword: Keyword identifying the option as in #rte_option_info.
 * @entry: Menu entry to be selected.
 * 
 * Similar to rte_codec_option_set() this function sets the value of
 * the named option, however it does so by number of the corresponding
 * menu entry. Naturally this must be an option with menu, or the
 * function will fail.
 * 
 * Note setting an option invalidates prior sample parameter
 * negotiation with rte_codec_set_parameters(), so you should
 * initialize the codec options first.
 *
 * Return value: 
 * %TRUE on success, otherwise the option is not changed.
 **/
rte_bool
rte_codec_option_menu_set(rte_codec *codec, const char *keyword, int entry)
{
	rte_context *context = NULL;
	rte_option_info *oi;

	nullcheck(codec, return FALSE);

	if (!(oi = rte_codec_option_info_keyword(codec, keyword)))
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
		fprintf(stderr, "rte:" __PRETTY_FUNCTION__
			": unknown export option type %d\n", oi->type);
		exit(EXIT_FAILURE);
	}
}

/**
 * rte_codec_options_reset:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * 
 * Resets all options of the codec to their respective default, that
 * is the value they have after calling rte_codec_set().
 * 
 * Return value: 
 * %TRUE on success, otherwise some options may be reset and some not.
 **/
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
			fprintf(stderr, "rte:" __PRETTY_FUNCTION__
				": unknown codec option type %d\n", oi->type);
			exit(EXIT_FAILURE);
		}
	}

	return r;
}

/*
 *  Parameters
 */

/**
 * rte_codec_parameters_set:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * @params: Parameters describing the source data.
 * 
 * This function is used to negotiate the source data parameters such
 * as image width and height or audio sampling frequency. The @params
 * structure must be cleared before calling this function, you can
 * propose parameters by setting the respective fields. On return all
 * fields will be set to the nearest possible value, for example
 * the image width and height rounded to a multiple of 16. The cycle
 * can repeat until a suitable set of parameters has been negotiated.
 *
 * <example><title>Typical usage of rte_codec_parameters_set()</title>
 * <programlisting>
 * rte_stream_parameters params;
 * &nbsp;
 * memset(&amp;params, 0, sizeof(params));
 * &nbsp;
 * params.video.pixfmt = RTE_PIXFMT_YUYV;
 * params.video.frame_rate = 24.0;
 * params.video.width = 384;
 * params.video.height = 288;
 * &nbsp;
 * rte_codec_parameters_set(video_codec, &amp;params);
 * </programlisting></example> 
 *
 * When the codec supports clipping, scaling and resampling
 * you can do it this:
 *
 * <example><title>Video clipping</title>
 * <programlisting>
 * rte_codec_option_set(video_codec, "width", dest_x1 - dest_x0 + 1);
 * rte_codec_option_set(video_codec, "height", dest_y1 - dest_y0 + 1);
 * &nbsp;
 * params.video.stride = source_y1 - source_y0 + 1;
 * params.video.offset = dest_x0 + dest_y0 * params.video.stride;
 * params.video.width  = dest_x1 - dest_x0 + 1;
 * params.video.height = dest_y1 - dest_y0 + 1;
 * </programlisting></example> 
 * <example><title>Video scaling</title>
 * <programlisting>
 * rte_codec_option_set(video_codec, "width", dest_x1 - dest_x0 + 1);
 * rte_codec_option_set(video_codec, "height", dest_y1 - dest_y0 + 1);
 * &nbsp;
 * params.video.stride = source_y1 - source_y0 + 1;
 * params.video.offset = 0;
 * params.video.width  = source_x1 - source_x0 + 1;
 * params.video.height = source_y1 - source_y0 + 1;
 * </programlisting></example> 
 * <example><title>Audio resampling</title>
 * <programlisting>
 * rte_codec_option_set(audio_codec, "sampling_rate", 44100.0);
 * &nbsp;
 * params.audio.sampling_rate = 32000.0;
 * </programlisting></example>
 *
 * Return value:
 * %FALSE if the parameters are somehow ambiguous.
 **/
rte_bool
rte_codec_parameters_set(rte_codec *codec, rte_stream_parameters *params)
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
 * rte_codec_parameters_get:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * @params: Parameters describing the source data.
 *
 * Query the negotiated data parameters.
 *
 * Return value:
 * %FALSE if no parameters have been negotiated.
 **/
rte_bool
rte_codec_parameters_get(rte_codec *codec, rte_stream_parameters *params)
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
		if (codec->status == RTE_STATUS_NEW) {
			r = FALSE;
		} else {
			memcpy(params, &codec->params, sizeof(*params));
			r = TRUE;
		}
	}

	return r;
}

/*
 *  Status
 */

/**
 * rte_codec_status_enum:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * @n: Status token index.
 *
 * Enumerates available status statistics for the codec, starting
 * from 0.
 *
 * Return value: A rte_status_info object that you should free with
 * rte_status_free(), or %NULL if the index is out of bounds.
 **/
rte_status_info *
rte_codec_status_enum(rte_codec *codec, int n)
{
	rte_context *context = NULL;

	nullcheck(codec, return NULL);

	context = codec->context;
	rte_error_reset(context);

	nullcheck(dc->status_enum, return NULL);

	return dc->status_enum(codec, n);
}

/**
 * rte_codec_status_keyword:
 * @codec: Pointer to a #rte_codec returned by rte_codec_get() or rte_codec_set().
 * @keyword: Status token keyword.
 *
 * Tries to find the status token by keyword.
 *
 * Return value: A rte_status object that you should free with
 * rte_status_free(), or %NULL if @keyword couldn't be found.
 **/
rte_status_info *
rte_codec_status_keyword(rte_codec *codec, const char *keyword)
{
	rte_context *context = NULL;
	rte_status_info *si;
	int i;

	nullcheck(codec, return NULL);

	context = codec->context;
	rte_error_reset(context);

	nullcheck(dc->status_enum, return NULL);

	for (i = 0;; i++)
	        if (!(si = dc->status_enum(codec, i))
		    || strcmp(keyword, si->keyword) == 0)
			break;

	return si;
}
