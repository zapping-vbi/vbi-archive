/*
 *  Real Time Encoder library - mp1e backend
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

/* $Id: b_mp1e.h,v 1.4 2002-03-19 19:26:29 mschimek Exp $ */

#ifndef B_MP1E_H
#define B_MP1E_H

#include "rtepriv.h"
#include "common/sync.h"
#include "systems/libsystems.h"

#define MAX_ELEMENTARY_STREAMS ((sizeof(sync_set) * 8) - 1)

/* Backend specific rte_codec and rte_context extensions */

typedef struct {
	rte_codec		codec;

	sync_stream 		sstr;

	/* I/O parameters reported by codec */

	int			io_stack_size;
	int			input_buffer_size;
	int			output_buffer_size;	/* maximum */
	int			output_bit_rate;	/* maximum */
	double			output_frame_rate;	/* exact */

	/* I/O fifos */

	fifo *			input;
	fifo *			output;

	/* Backend side I/O stuff */

	pthread_t		thread_id;

	fifo			in_fifo;
	fifo			out_fifo;

	producer		prod;
	consumer		cons;

	rte_io_method		input_method;

	rte_buffer_callback	read_cb;
	rte_buffer_callback	write_cb;
	rte_buffer_callback	unref_cb;
} mp1e_codec;

typedef struct {
	rte_context		context;

	sync_main		sync;

	rte_codec *		codecs;
	int			num_codecs;

	multiplexer		mux;
	buffer			mux_buffer;
	rte_buffer_callback	write_cb;
} mp1e_context;

#endif /* B_MP1E_H */






