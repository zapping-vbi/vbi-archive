/*
 *  Real Time Encoder library
 *  ffmpeg backend
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

/* $Id: b_ffmpeg.h,v 1.7 2002-10-02 02:18:02 mschimek Exp $ */

#ifndef B_FFMPEG_H
#define B_FFMPEG_H

#include "../site_def.h"
#include "../config.h"
#include "../src/rtepriv.h"

#include <stddef.h>

#define be2me_32(n) (n) /* not here, thanks. */
#include "libav/avformat.h"
#include "libav/tick.h"

#define PARENT(ptr, type, member)					\
        ((type *)(((char *) ptr) - offsetof(type, member)))

#define PCAST(name, to, from, member)					\
static inline to * name (from *p) {					\
	return PARENT(p, to, member);					\
}

/* Backend specific rte_codec and rte_context extensions */

typedef struct {
	rte_codec		codec;
  	AVStream		str;

	int			stream_index;
	Ticker			pts_ticker;		/* Ticker for PTS calculation */
	INT64			pts;			/* current pts */
	int			pts_increment;		/* expected pts increment for next packet */
	int			frame_number;		/* current frame */
	INT64			sample_index;		/* current sample */
	int			eof;

	enum PixelFormat	input_pix_fmt;
	rte_stream_parameters	params;

	void *			temp_picture;
	void *			packet_buffer;

/* I/O parameters reported by codec */

//int			io_stack_size;
//int			input_buffer_size;
//int			output_buffer_size;	/* maximum */
//int			output_bit_rate;	/* maximum */
//double			output_frame_rate;	/* exact */

	/* Backend side I/O stuff */

	pthread_t		thread_id;

	rte_io_method		input_method;

	rte_buffer_callback	read_cb;
	rte_buffer_callback	unref_cb;

	rte_status		status;
} ffmpeg_codec;

PCAST(FD, ffmpeg_codec, rte_codec, codec);

typedef struct {
	rte_context		context;
	AVFormatContext		av;

	unsigned char		buf[1 << 16];

	rte_codec *		codecs;
	int			num_codecs;

	pthread_t		thread_id;

	INT64			stop_pts;

	rte_buffer_callback	write_cb;
	rte_seek_callback	seek_cb;

	rte_status		status;
} ffmpeg_context;

PCAST(FX, ffmpeg_context, rte_context, context);

typedef struct {
	rte_codec_class		rte;
	AVCodec *		av;  
	unsigned int		options;
} ffmpeg_codec_class;

PCAST(FDC, ffmpeg_codec_class, rte_codec_class, rte);

typedef struct {
	rte_context_class	rte;
	AVFormat *		av;
	unsigned int		options;
	ffmpeg_codec_class *	codecs[16];
} ffmpeg_context_class;

PCAST(FXC, ffmpeg_context_class, rte_context_class, rte);

#endif /* B_FFMPEG_H */
