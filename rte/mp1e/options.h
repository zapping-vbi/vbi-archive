/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2001 Michael H. Schimek
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

/* $Id: options.h,v 1.6 2001-11-22 17:51:07 mschimek Exp $ */

extern int		test_mode;

extern char *		cap_dev;
extern char *		pcm_dev;
extern char *		mix_dev;
extern char *		vbi_dev;

/* Video */

extern int		width;
extern int		height;
extern int		grab_width;
extern int		grab_height;
extern int		video_bit_rate;
extern long long	video_num_frames;
extern char *		gop_sequence;
// extern int		frames_per_seqhdr;
extern int		filter_mode;
extern double		frame_rate;
extern int		preview;
extern char *		anno;
extern int		luma_only;
extern int		motion_min;
extern int		motion_max;
extern int		skip_method;

/* Audio */

extern int		audio_bit_rate;
extern int		audio_bit_rate_stereo;
extern long long	audio_num_frames;
extern int		sampling_rate;
extern int		mix_line;
extern int		mix_volume;
extern int		audio_mode;
extern int		psycho_loops;
extern int		mute; // bttv specific

/* VBI */

extern char *		subtitle_pages;

/* Multiplexer */

#define MOD_VIDEO	0x01
#define MOD_AUDIO	0x02
#define MOD_SUBTITLES	0x04

extern int		modules;
extern int		mux_syn;

extern int		cap_buffers;
extern int		vid_buffers;
extern int		aud_buffers;

extern int		cpu_type;

extern void		options(int ac, char **av);
