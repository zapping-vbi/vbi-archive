/*
 *  Real Time Encoder library
 *  divx4linux backend
 *
 *  Copyright (C) 2002 Michael H. Schimek
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

/* $Id: b_divx4linux.h,v 1.3 2002-12-14 00:48:49 mschimek Exp $ */

#ifndef B_DIVX4LINUX_H
#define B_DIVX4LINUX_H

#include "site_def.h"
#include "rtepriv.h"

#include <stddef.h>
#include <encore2.h>

#define ENCORE4_VERSION 20010807
#define ENCORE5_VERSION 20020304

typedef struct {
	int x_dim, y_dim;
	float framerate;
	int bitrate, unused0, unused1, unused2, unused3, unused4;
	int max_key_interval, use_bidirect, deinterlace, quality, obmc;
	void *handle;
} ENC_PARAM4;

#define PARENT(ptr, type, member)					\
        ((type *)(((char *) ptr) - offsetof(type, member)))

#define PCAST(name, to, from, member)					\
static inline to * name (from *p) {					\
	return PARENT(p, to, member);					\
}

typedef struct {
	rte_context		context;
	rte_codec		codec;
	rte_status		status;

	void *			handle;

	ENC_PARAM4		enc_param4;
	ENC_PARAM		enc_param5;
	ENC_FRAME		enc_frame;
	ENC_RESULT		enc_result;

	void *			buffer;

	rte_bool		codec_set;

	pthread_t		thread_id;
	rte_bool		stopped;

	rte_io_method		input_method;

	rte_buffer_callback	read_cb;
	rte_buffer_callback	unref_cb;

	rte_seek_callback	seek_cb;
	rte_buffer_callback	write_cb;
} d4l_context;

PCAST (DX, d4l_context, rte_context, context);

#endif /* B_DIVX4LINUX_H */
