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

/* $Id: audio.h,v 1.6 2000-10-17 21:55:42 garetxe Exp $ */

#include <pthread.h>
#include "../common/fifo.h"

extern fifo *		audio_fifo;
extern fifo *		audio_cap_fifo;
extern pthread_t	audio_thread_id;

extern void *		mpeg_audio_layer_ii_mono(void *unused);
extern void *		mpeg_audio_layer_ii_stereo(void *unused);
extern void		audio_parameters(int *sampling_freq, int *bit_rate);

/* oss.c */

extern void		pcm_init(void);
extern void		mix_init(void);
extern char *		mix_sources(void);

/* alsa.c */

extern void		alsa_pcm_init(void);

/* tsp.c */

extern void		tsp_init(void);

/* esd.c */

extern void		esd_pcm_init(void);
