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

/*
  A nice set of HTML rendered docs can be found here:
  http://zapping.sf.net/docs/rte/index.html
  FIXME: Upload docs before the release.
*/

#include "rte-enums.h"
#include "rte-types.h"
#include "rte-version.h"

/**
 * rte_init:
 *
 * Inits the library and all backends.
 * Return value: %TRUE if the library is usable in this box.
 **/
rte_bool
rte_init(void);

/**
 * rte_context_info_enum:
 * @index: Index into the context format table, 0 ... n.
 *
 * Enumerates available backends / file formats / multiplexers. You
 * should start at index 0, incrementing. Assume a subsequent call to
 * this function will overwrite the returned context description.
 *
 * Some codecs may depend on machine features such as SIMD instructions
 * or the presence of certain libraries, thus the list can vary from
 * session to session.
 *
 * Return value:
 * Pointer to a rte_context_info structure (note: static pointer, no
 * need to be freed), %NULL if the index is out of bounds.
 **/
rte_context_info *
rte_context_info_enum(int index);

/**
 * rte_context_info_keyword:
 * @keyword: Context format identifier as in rte_context_info and
 *           rte_context_new().
 * 
 * Similar to rte_context_enum(), but this function attempts to find a
 * context info by keyword.
 * 
 * Return value:
 * Pointer to a rte_context_info structure, %NULL if the named context
 * format has not been found.
 **/
rte_context_info *
rte_context_info_keyword(const char *keyword);

/**
 * rte_context_info_context:
 * @context: Pointer to a rte_context previously allocated with
 *	     rte_context_new().
 *
 * Returns the context info for the given @context.
 *
 * Return value: A rte_context_info struct, or %NULL if it couldn't
 * be found.
 **/
rte_context_info *
rte_context_info_context (rte_context *context);

/**
 * rte_context_new:
 * @keyword: Context format identifier as in rte_context_info.
 * @user_data: Any custom data you want to attach to the created
 *	       context, can be read later on with rte_context_get_user_data()
 * 
 * Creates a new rte context, encoding files of the specified type.
 *
 * Return value:
 * Pointer to a newly allocated rte_context structure, which must be
 * freed by calling rte_context_delete(). %NULL is returned when the
 * named context format is unavailable or some other error occurred.
 **/
rte_context *
rte_context_new(const char *keyword, void *user_data);

/**
 * rte_context_delete:
 * @context: Pointer to a rte_context previously allocated with
 *	     rte_context_new().
 * 
 * This function stops encoding if active, then frees all resources
 * associated with the context.
 **/
void
rte_context_delete(rte_context *context);

/**
 * rte_context_set_user_data:
 * @context: Encoding context.
 * @user_data: Data to set.
 *
 * Sets the new custom data the context will hold, you can query it
 * with rte_context_get_user_data().
 **/
void
rte_context_set_user_data(rte_context *context, void *user_data);

/**
 * rte_context_get_user_data:
 * @context: Encoding context.
 *
 * Gets the custom data @context holds, it can be set with rte_context_new()
 * or rte_context_set_user_data().
 **/
void *
rte_context_get_user_data	(rte_context *	context);

/**
 * rte_codec_info_enum:
 * @context: Initialized rte_context.
 * @stream_type: RTE_STREAM_VIDEO, _AUDIO, ...
 * @index: Index into the codec table for @stream_type, 0 ... n.
 * 
 * Enumerates elementary stream codecs available for the selected
 * backend / file / mux format and stream type.
 * You should start at index 0, incrementing.
 * Assume a subsequent call to this function will overwrite the returned
 * codec description.
 * 
 * Return value:
 * Pointer to a static rte_codec_info structure, %NULL if the context
 * is invalid or the index is out of bounds.
 **/
rte_codec_info *
rte_codec_info_enum(rte_context *context, rte_stream_type stream_type,
		    int index);

/**
 * rte_codec_info_keyword:
 * @context: Initialized rte_context.
 * @keyword: Codec identifier as in rte_codec_info, rte_codec_get|set..
 * 
 * Similar to rte_codec_enum(), but this function attempts to find a
 * codec by keyword.
 * 
 * Return value:
 * Pointer to a rte_codec_info structure, %NULL if the context is invalid
 * or the named codec has not been found.
 **/
rte_codec_info *
rte_codec_info_keyword(rte_context *context, const char *keyword);

/**
 * rte_codec_info_codec:
 * @codec: Pointer to a rte_codec returned by rte_codec_get() or
 *	   rte_codec_get()
 *
 * Returns the codec info that matches @codec.
 *
 * Return value: the codec info for the given @codec.
 **/
rte_codec_info *
rte_codec_info_codec(rte_codec *codec);

/**
 * rte_codec_set:
 * @context: Initialized rte_context.
 * @stream_index: Elementary stream number.
 * @codec_keyword: Codec identifier, e.g. from rte_codec_info.
 * 
 * Assign this codec to encode data for the elementary stream of the
 * codec's type and @stream_index of the format this context stands for.
 * The stream number refers for example to one of the 16 video or 32 audio
 * streams contained in a MPEG-1 program stream. The valid number of elementary
 * streams of each type is available from &rte_context_info, see
 * rte_context_enum(). The first (default) stream has index number 0.
 *
 * Setting a codec resets all properties of the codec for this stream type
 * and index. Passing a %NULL pointer as @codec_keyword withdraws encoding
 * of this stream type and index.
 *
 * For example, if you wanted to set the codec for the first audio
 * elementary stream of the mp1e encoder you'd do:
 *
 * <programlisting>
 * rte_codec_set(context, 0, "mpeg2_audio_layer_2");
 * </programlisting>
 * 
 * Return value: 
 * Pointer to an opaque rte_codec object (not even the RTE frontend knows
 * for sure what this is :-). On error %NULL is returned, which may be caused,
 * apart of invalid parameters, by an unknown @codec_keyword or a stream type
 * associated with the codec not suitable for the selected backend /
 * mux / file format.
 **/
rte_codec *
rte_codec_set(rte_context *context, int stream_index,
	      const char *codec_keyword);

/**
 * rte_codec_get:
 * @context: Initialized rte_context.
 * @stream_type: RTE_STREAM_VIDEO, _AUDIO, ...
 * @stream_index: Stream number.
 *
 * Returns the codec currently associated with the given @stream_index
 * of type @stream_type. rte_codec_set() can be used to change the
 * current codec.
 *
 * Return value: a static pointer to the codec or %NULL if the stream
 * number is empty.
 **/
rte_codec *
rte_codec_get(rte_context *context, rte_stream_type stream_type,
	      int stream_index);


/**
 * rte_codec_set_parameters:
 * @codec: Pointer to a rte_codec returned by rte_codec_get() or
 *	   rte_codec_set()
 * @params: Parameters needed for describing the data source.
 * 
 * Use this function for describing how are you going to feed the data
 * into the encoder.
 * Make sure to zero out the struct before setting the fields you are
 * interested in, defaults will be used if that makes sense.
 * Upon return, the fields might be overwritten to reflect the nearest
 * possible match. For example, the width and height might be rounded
 * to 16-multiplus.
 *
 * Typical usage would be:
 * <programlisting>
 * memset(params, 0, sizeof(*params));
 * params->video.pixfmt = RTE_PIXFMT_YUV420;
 * params->video.frame_rate = 24;
 * params->video.width = 384;
 * params->video.height = 288;
 * params->feed_mode = RTE_PUSH_COPY;
 * rte_codec_set_parameters(codec, params);
 * </programlisting>
 *
 * Return value: %TRUE if a suitable match was found, make sure to
 * check whether the returned parameters are the same as the ones you passed.
 **/
rte_bool
rte_codec_set_parameters(rte_codec *codec, rte_stream_parameters *params);

/**
 * rte_option_info_enum:
 * @codec: Pointer to a rte_codec returned by rte_codec_get() or
 *	   rte_codec_set()
 * @index: Index in the option table 0,...n
 *
 * Enumerates the options available for the given codec. 
 * You should start at index 0, incrementing.
 * Assume a subsequent call to this function will overwrite the returned
 * option description.
 *
 * Return value: Static pointer to the option description, or %NULL if
 * @index is out of bounds.
 **/
rte_option_info *
rte_option_info_enum(rte_codec *codec, int index);

/**
 * rte_option_info_keyword:
 * @codec: Pointer to a rte_codec returned by rte_codec_get() or
 *	   rte_codec_set()
 * @keyword: Keyword of the relevant option, as in rte_option_info
 *
 * Like rte_option_info_enum() but tries to find the option info based
 * on the given keyword.
 *
 * Return value: Static pointer to the option description, or %NULL if
 * the keyword wasn't found.
 **/
rte_option_info *
rte_option_info_keyword(rte_codec *codec, const char *keyword);

/**
 * rte_option_get:
 * @codec: Pointer to a rte_codec returned by rte_codec_get() or
 *	   rte_codec_set()
 * @keyword: Keyword of the relevant option, as in rte_option_info
 * @value: A place to store info about the options.
 *
 * This function is used to query the current value for the given
 * option. If the option is a string, @value.str must be freed when
 * you don't need it any longer. If the option is a menu, then
 * @value.num contains the selected entry.
 *
 * Return value: %TRUE if the option was found, %FALSE otherwise.
 **/
rte_bool
rte_option_get(rte_codec *codec, const char *keyword,
	       rte_option_value *value);

/**
 * rte_option_set:
 * @codec: Pointer to a rte_codec returned by rte_codec_get() or
 *	   rte_codec_set()
 * @keyword: Keyword of the relevant option, as in rte_option_info.
 * @Varargs: New value to set.
 *
 * Sets the value of the given option. Make sure you are casting the
 * third parameter (the new value to set) to the correct type (int,
 * char*, double), otherwise you might get heisenbugs.
 *
 * Typical usage is:
 * <programlisting>
 * rte_option_set(codec, "frame_rate", (double)3.141592);
 * </programlisting>
 *
 * Return value: %TRUE if the option was found and it could be
 * correctly set.
 **/
rte_bool
rte_option_set(rte_codec *codec, const char *keyword, ...);

#endif /* rte.h */
