/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

/* $Id: libsystems.h,v 1.8 2002-12-14 00:43:44 mschimek Exp $ */

#ifndef LIBSYSTEMS_H
#define LIBSYSTEMS_H

#include "../common/fifo.h"
#include "rtepriv.h"

typedef struct multiplexer {
	xlist			streams;

	fifo *			output;
	producer 		prod;

	int			packet_size;
	void *			user_data;

	/* preliminary */
	buffer *		(* mux_output)(struct multiplexer* mux, buffer *b);

	rte_status		status;
} multiplexer;

extern rte_bool			mux_init	(multiplexer *mux, void *user_data);
extern void			mux_destroy	(multiplexer *mux);
extern multiplexer *		mux_alloc	(void *user_data);
extern void			mux_free	(multiplexer *mux);

extern fifo *			mux_add_input_stream(multiplexer *mux,
					int stream_id, char *name,
					int max_size, int buffers,
					double frame_rate, int bit_rate);
extern void			mux_rem_input_stream(fifo *);

extern void *			stream_sink(void *mux);
extern void *			mpeg1_system_mux(void *mux);
extern void *			mpeg2_program_stream_mux(void *mux);
extern void *			elementary_stream_bypass(void *mux);
extern void *			vcd_system_mux(void *mux);

extern double			get_idle(void);

extern rte_bool			init_output_stdout(multiplexer *mux);
extern void *                   output_thread(void * unused);
extern int                      output_init(void);
extern void                     output_end(void);

extern char *			mpeg_header_name(unsigned int code);

#endif
