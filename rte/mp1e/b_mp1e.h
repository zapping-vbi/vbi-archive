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

/* $Id: b_mp1e.h,v 1.1 2002-02-08 15:03:10 mschimek Exp $ */

#ifndef MP1E_H
#define MP1E_H

#include "../src/rtepriv.h"
#include "common/sync.h"

/* Backend specific rte_codec and rte_context extensions */

typedef struct {
	rte_codec		codec;
	synchr_stream 		sstr;

	int			input_stack_size;
	int			input_buffer_size;

	int			output_buffer_size;	/* maximum */
	int			output_bit_rate;	/* maximum */
	double			output_frame_rate;	/* exact */

	fifo *			input;
	fifo *			output;

	/***/

	fifo			in_fifo;
	fifo			out_fifo;

	producer		prod;
	consumer		cons;

	rte_input		input_mode;

	rte_buffer_callback	read_cb;
	rte_buffer_callback	write_cb;
	rte_buffer_callback	unref_cb;
} mp1e_codec;

typedef struct {
	rte_context		context;

	rte_codec *		codecs;

	int			num_codecs;

//	multiplexer *mux;
	rte_buffer_callback	write_cb;

} mp1e_context;




/* From main.h, to be removed. */
/*
extern double		video_stop_time;
extern double		audio_stop_time;

extern pthread_t	audio_thread_id;
extern int		stereo;

extern pthread_t	video_thread_id;
extern void		(* video_start)(void);

extern pthread_t        output_thread_id;

extern pthread_t	tk_main_id;
extern void *		tk_main(void *);

extern int		mux_mode;
extern int		psycho_loops;

extern void options(int ac, char **av);

extern void preview_init(int *acp, char ***avp);

#include "systems/libsystems.h"

extern volatile int program_shutdown;
*/
#endif /* MP1E_H */

