/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999, 2000, 2001, 2002 Michael H. Schimek
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

/* $Id: libaudio.h,v 1.14 2003-01-03 05:37:36 mschimek Exp $ */

#include "../b_mp1e.h"
#include "../common/fifo.h"
//#include "../systems/libsystems.h"

// XXX remove
struct pcm_context {
	fifo		fifo;
	producer	producer;

	rte_sndfmt      format;
	int		sampling_rate;
	rte_bool	stereo;
};

/* mp2.c */

extern rte_codec_class	mp1e_mpeg1_layer2_codec;
extern rte_codec_class	mp1e_mpeg2_layer2_codec;

extern void		mp1e_mp2_module_init(int test);
//extern void		mp1e_mp2_uninit(rte_codec *codec);
//extern void		mp1e_mp2_init(rte_codec *, unsigned int,
//				      fifo *cap_fifo, multiplexer *);
extern void *		mp1e_mp2(void *);

/* historic */
extern void		audio_parameters(int *sampling_freq, int *bit_rate);

/* oss.c (mp1e) */

extern void		mix_init(void);
extern char *		mix_sources(void);

/* misc (mp1e) */

extern void		open_pcm_oss(char *dev_name, int sampling_rate, rte_bool stereo, fifo **);
extern void
open_pcm_alsa			(const char *		dev_name,
				 unsigned int		sampling_rate,
				 rte_bool		stereo,
				 fifo **		f);
extern void		open_pcm_esd(char *, int sampling_rate, rte_bool stereo, fifo **);
extern void		open_pcm_arts(char *, int sampling_rate, rte_bool stereo, fifo **);
extern void		open_pcm_afl(char *name, int, rte_bool, fifo **);
