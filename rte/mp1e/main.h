/*
 *  Exported prototypes, they are needed by rte
 *
 *  Copyright (C) 2000 Iñaki G.E.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 2.
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
#ifndef __MAIN_H__
#define __MAIN_H__

/* fixme: leave only neccessary prototypes */

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

#endif // MAIN.H
