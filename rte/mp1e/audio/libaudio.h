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

/* $Id: libaudio.h,v 1.9 2001-10-26 09:14:51 mschimek Exp $ */

#include "../common/fifo.h"
#include "../systems/libsystems.h"

#include "../rtepriv.h"

// XXX remove
struct pcm_context {
	fifo		fifo;
	producer	producer;

	rte_sndfmt      format;
	int		sampling_rate;
	bool		stereo;
};

/* mp2.c */

extern rte_codec_class	mp1e_mpeg1_layer2_codec;
extern rte_codec_class	mp1e_mpeg2_layer2_codec;

/* preliminary */
extern void		mp1e_mp2_init(rte_codec *, unsigned int,
				      fifo *cap_fifo, multiplexer *);
extern void *		mp1e_mp2_thread(void *foo);

/* historic */
extern void		audio_parameters(int *sampling_freq, int *bit_rate);
extern long long	audio_frame_count;
extern long long	audio_frames_dropped;

/* oss.c (mp1e) */

extern void		mix_init(void);
extern char *		mix_sources(void);

/* misc (mp1e) */

extern fifo *		open_pcm_oss(char *dev_name, int sampling_rate, bool stereo);
extern fifo *		open_pcm_alsa(char *dev_name, int sampling_rate, bool stereo);
extern fifo *		open_pcm_esd(char *, int sampling_rate, bool stereo);
extern fifo *		open_pcm_afl(char *name, int, bool);
