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

/* $Id: rte.h,v 1.18 2002-12-14 00:48:50 mschimek Exp $ */

#ifndef RTE_H
#define RTE_H

/* Public */

/**
 * @ingroup IO
 * This structure holds information about data blocks exchanged
 * with the codec, one video frame or one block of audio
 * samples as negotiated with rte_parameters_set().
 *
 * Depending on data direction @a data points to the data and @a size
 * is the size of the data in bytes, or @a data points to buffer space
 * to store data and @a size is the space available.
 *
 * When data is passed, @a timestamp is the capture instant (of the
 * first byte if you wish) in seconds and fractions since 1970-01-01
 * 00:00. A codec may use the timestamps for synchronization and to
 * detect frame dropping. Timestamps must increment with each buffer
 * passed, and should increment by 1 / nominal input rate. That is
 * rte_video_stream_params->frame_rate or
 * rte_audio_stream_params->sampling_freq times channels divided
 * by samples per buffer.
 *
 * The @a user_data will be returned along @a data with rte_push_buffer()
 * in slave push mode, and the unreference callback in slave
 * callback mode. Otherwise @a user_data is ignored.
 */
typedef struct {
	void *			data;
	unsigned int		size;
	double			timestamp;
	void *			user_data;
} rte_buffer;

/**
 * @ingroup IO
 * @param context rte_context this operation refers to.
 * @param codec rte_codec this operation refers to (if any).
 * @param buffer Pointer to rte_buffer.
 *
 * When a function of this type is called to read more data
 * in master callback mode, the rte client must initialize the
 * @a buffer fields .data, .size and .timestamp. In slave
 * callback mode .data points to the buffer space to store
 * the data and .size is the free space, so the client
 * must set .timestamp and may set .size and .user_data.
 *
 * When a function of this type is called to unreference data,
 * it passes the same @a buffer contents (but not necessarily
 * the same @a buffer pointer) exchanged at the respective
 * read callback or rte_push_buffer() before.
 *
 * When a function of this type is called to write data,
 * @a codec is NULL and the @a buffer fields .data and .size
 * are initialized. @a buffer will be @c NULL once to indicate
 * the end of the stream.
 *
 * Do <em>not</em> depend on the value of the @a buffer
 * pointer, use buffer->user_data instead.
 *
 * @warning A codec may read more than once before unreferencing
 * the data, and it may also unreference the data in a different
 * order than it has been read.
 *
 * @return
 * On error the callback can return @c FALSE to abort encoding.
 */
typedef rte_bool (* rte_buffer_callback)(rte_context *context,
					 rte_codec *codec,
					 rte_buffer *buffer);

/**
 * @ingroup IO
 * @param context rte_context this operation refers to.
 * @param offset Position to seek to. NOTE: On GNU/Linux files
 *   are opened with O_LARGEFILE, so clients should use
 *   lseek64(). When only 32 bit I/O is available and offset
 *   is > INT_MAX the callback should return @c FALSE. (I would
 *   define this as off64_t if I knew a clean & portable way.)
 * @param whence SEEK_SET..., see man lseek.
 *
 * The context requests to seek to the given stream position.
 * @a offset and @a whence follow lseek semantics. The first
 * byte of the next buffer passed shall be stored at the new
 * position.
 *
 * @return
 * On error the callback can return @c FALSE to abort encoding.
 */
typedef rte_bool (*rte_seek_callback)(rte_context *context,
				      long long offset,
				      int whence);

/**
 * @addtogroup IO
 * @{
 */
extern rte_bool			rte_set_input_callback_master(rte_codec *codec, rte_buffer_callback read_cb, rte_buffer_callback unref_cb, unsigned int *queue_length);
extern rte_bool			rte_set_input_callback_slave(rte_codec *codec, rte_buffer_callback read_cb);
extern rte_bool			rte_set_input_push_master(rte_codec *codec, rte_buffer_callback unref_cb, unsigned int queue_request, unsigned int *queue_length);
extern rte_bool			rte_set_input_push_slave(rte_codec *codec, unsigned int queue_request, unsigned int *queue_length);

extern rte_bool			rte_push_buffer(rte_codec *codec, rte_buffer *buffer, rte_bool blocking);
/** @} */

/**
 * @addtogroup IO
 * @{
 */
extern rte_bool			rte_set_output_callback_slave(rte_context *context, rte_buffer_callback write_cb, rte_seek_callback seek_cb);
extern rte_bool			rte_set_output_stdio(rte_context *context, int fd);
extern rte_bool			rte_set_output_file(rte_context *context, const char *filename);
extern rte_bool			rte_set_output_discard(rte_context *context);
/** @} */

/**
 * @addtogroup Start
 * @{
 */
extern rte_bool			rte_start(rte_context *context, double timestamp, rte_codec *sync_ref, rte_bool async);
extern rte_bool			rte_stop(rte_context *context, double timestamp);
/** @} */

/* Private */

#endif /* RTE_H */
