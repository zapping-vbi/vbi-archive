/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
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

/* $Id: systems.h,v 1.10 2001-01-24 22:48:52 mschimek Exp $ */

#ifndef __SYSTEMS_H__
#define __SYSTEMS_H__

//extern mucon			mux_mucon;
extern int			bytes_out;

extern double			get_idle(void);

extern void *			stream_sink(void *unused);
extern void *			mpeg1_system_mux(void *unused);
extern void *			mpeg2_program_stream_mux(void *unused);
extern void *			elementary_stream_bypass(void *unused);
extern void *			vcd_system_mux(void *unused);

extern char *			mpeg_header_name(unsigned int code);

extern bool			init_output_stdout(void);
extern void *                   output_thread(void * unused);
extern int                      output_init(void);
extern void                     output_end(void);

extern list			mux_input_streams;
extern void			mux_cleanup(void);
extern fifo *			mux_add_input_stream(int stream_id,
					int max_size, int buffers,
					double frame_rate, int bit_rate, fifo *);

#endif
