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

/* $Id: rte.h,v 1.15 2002-03-16 16:35:38 mschimek Exp $ */

#ifndef RTE_H
#define RTE_H

/* FIXME: This should be improved (requirements for off64_t) */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

/* Public */

/**
 * rte_buffer:
 *
 * This structure holds information about data packets exchanged
 * with the codec, for example one video frame or one block of audio
 * samples as defined with rte_codec_parameters_set().
 *
 * Depending on data direction @data points to the data and @size
 * is the size of the data in bytes, or @data points to buffer space
 * to store data and @size is the space available.
 *
 * When data is passed, @timestamp is the capture instant (of the
 * first byte if you wish) in seconds and fractions since 1970-01-01
 * 00:00. A codec may use the timestamps for synchronization and to
 * detect frame dropping. Timestamps must increment with each buffer
 * passed, and should increment by 1 / nominal buffer rate. That is
 * #rte_video_stream_params.frame_rate or
 * #rte_audio_stream_params.sampling_freq * channels and samples per
 * buffer.
 *
 * The @user_data will be returned along @data with rte_push_buffer()
 * in passive push mode, and the unreference callback in passive
 * callback mode, is otherwise ignored.
 **/
typedef struct {
	void *			data;
	unsigned int		size;
	double			timestamp;
	void *			user_data;
} rte_buffer;

/**
 * rte_buffer_callback:
 * @context: #rte_context this operation refers to.
 * @codec: #rte_codec this operation refers to (if any).
 * @buffer: Pointer to #rte_buffer.
 *
 * When a function of this type is called to read more data
 * in active callback mode, the rte client must initialize the
 * @buffer fields .data, .size and .timestamp. In passive
 * callback mode .data points to the buffer space to store
 * the data and .size is the free space, so the client
 * must initialize .timestamp and may set .size and
 * .user_data.
 *
 * When a function of this type is called to unreference data,
 * the same @buffer contents will be passed as to the read
 * callback or rte_push_buffer() before.
 *
 * When a function of this type is called to write data,
 * @codec is NULL and the @buffer fields .data and .size
 * are initialized. @buffer can be %NULL to indicate the
 * end of the stream.
 *
 * Do <emphasis>not</> depend on the value of the @buffer
 * pointer, use buffer.user_data instead.
 *
 * Attention: A codec may read more than once before freeing
 * the data, and it may also free the data in a different order
 * than it has been read.
 *
 * Return value:
 * The callback can return %FALSE to terminate (or in case
 * of an i/o error abort) the encoding.
 **/
typedef rte_bool (* rte_buffer_callback)(rte_context *context,
					 rte_codec *codec,
					 rte_buffer *buffer);

/**
 * rte_seek_callback:
 * @context: #rte_context this operation refers to.
 * @offset: Position to seek to.
 * @whence: SEEK_SET..., see man lseek.
 *
 * The context requests to seek to the given resulting stream
 * position. @offset and @whence follow lseek semantics.
 *
 * Return value:
 * %FALSE on error.
 **/
typedef rte_bool (*rte_seek_callback)(rte_context *context,
				      off64_t offset,
				      int whence);

extern rte_bool			rte_set_input_callback_active(rte_codec *codec, rte_buffer_callback read_cb, rte_buffer_callback unref_cb, int *queue_length);
extern rte_bool			rte_set_input_callback_passive(rte_codec *codec, rte_buffer_callback read_cb);
extern rte_bool			rte_set_input_push_active(rte_codec *codec, rte_buffer_callback unref_cb, int queue_request, int *queue_length);
extern rte_bool			rte_set_input_push_passive(rte_codec *codec, int queue_request, int *queue_length);

extern rte_bool			rte_set_output_callback_passive(rte_context *context, rte_buffer_callback write_cb, rte_seek_callback seek_cb);
extern rte_bool			rte_set_output_stdio(rte_context *context, int fd);
extern rte_bool			rte_set_output_file(rte_context *context, const char *filename);

extern rte_bool			rte_push_buffer(rte_codec *codec, rte_buffer *buffer, rte_bool blocking);

extern rte_bool			rte_start(rte_context *context, double timestamp, rte_codec *sync_ref, rte_bool async);
extern rte_bool			rte_stop(rte_context *context, double timestamp);



/* Private */

#endif /* RTE_H */
