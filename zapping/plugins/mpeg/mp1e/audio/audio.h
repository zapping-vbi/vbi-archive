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

/* $Id: audio.h,v 1.2 2000-08-09 09:41:36 mschimek Exp $ */

#include <pthread.h>
#include "../common/fifo.h"

extern _fifo		aud;
extern pthread_t	audio_thread_id;

extern void *		audio_compression_thread(void *unused);
extern void *		stereo_audio_compression_thread(void *unused);
extern void		audio_parameters(int *sampling_freq, int *bit_rate);

extern short *		(* audio_read)(double *);
extern void		(* audio_unget)(short *);

/* oss.c */

extern void		pcm_init(void);
extern void		mix_init(void);
extern char *		mix_sources(void);

/* tsp.c */

extern void		tsp_init(void);
