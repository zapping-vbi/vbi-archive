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
  FIXME: Non-blocking push.
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
 * Select the codec identified by @codec_keyword to encode the data as
 * elementary stream number @stream_index of the stream type @codec_keyword
 * belongs to. The stream number refers
 * for example to one of the 16 video or 32 audio streams contained in a
 * MPEG-1 program stream, but you should pass 0 for now.
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
 * rte_option_info_enum:
 * @codec: Pointer to a rte_codec returned by rte_codec_get() or
 *	   rte_codec_get()
 * @index: Index in the option table 0,...n
 *
 * Enumerates the options available for the given codec. 
 * You should start at index 0, incrementing.
 * Assume a subsequent call to this function will overwrite the returned
 * control description.
 *
 * Return value: Static pointer to the option description, or %NULL if
 * @index is out of bounds.
 **/
rte_option_info *
rte_option_info_enum(rte_codec *codec, int index);

/**
 * rte_option_info_keyword:
 * @codec: Pointer to a rte_codec returned by rte_codec_get() or
 *	   rte_codec_get()
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

#endif /* rte.h */
