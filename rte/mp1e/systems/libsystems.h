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

/* $Id: libsystems.h,v 1.2 2001-08-22 01:28:08 mschimek Exp $ */

#ifndef LIBSYSTEMS_H
#define LIBSYSTEMS_H

#include "../common/fifo.h"

typedef struct multiplexer multiplexer; /* opaque */

extern int			bytes_out;

extern multiplexer *		mux_alloc(void);
extern void			mux_free(multiplexer *mux);
extern fifo *			mux_add_input_stream(multiplexer *mux,
					int stream_id, char *name,
					int max_size, int buffers,
					double frame_rate, int bit_rate);

extern void *			stream_sink(void *mux);
extern void *			mpeg1_system_mux(void *mux);
extern void *			mpeg2_program_stream_mux(void *mux);
extern void *			elementary_stream_bypass(void *mux);
extern void *			vcd_system_mux(void *mux);

extern double			get_idle(void);

extern bool			init_output_stdout(void);
extern void *                   output_thread(void * unused);
extern int                      output_init(void);
extern void                     output_end(void);

extern char *			mpeg_header_name(unsigned int code);

#endif
