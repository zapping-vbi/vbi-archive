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

/* $Id: libaudio.h,v 1.2 2001-08-08 05:24:36 mschimek Exp $ */

#include "../common/fifo.h"
#include "../systems/libsystems.h"

extern fifo *		audio_fifo;

extern void *		mpeg_audio_layer_ii_mono(void *cap_fifo);
extern void *		mpeg_audio_layer_ii_stereo(void *cap_fifo);
extern void		audio_parameters(int *sampling_freq, int *bit_rate);
extern void		audio_init(int sampling_freq, int stereo,
				int audio_mode, int bit_rate,
				int psycho_loops, multiplexer *mux);

extern int		audio_frame_count;
extern int		audio_frames_dropped;

/* oss.c */

extern void		mix_init(void);
extern char *		mix_sources(void);

extern fifo *		open_pcm_oss(char *dev_name, int sampling_rate, bool stereo);
extern fifo *		open_pcm_alsa(char *dev_name, int sampling_rate, bool stereo);
extern fifo *		open_pcm_esd(char *, int sampling_rate, bool stereo);
extern fifo *		open_pcm_afl(char *name, int, bool);
