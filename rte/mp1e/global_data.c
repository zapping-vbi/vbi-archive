/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 2000 Michael H. Schimek
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

/* $Id: global_data.c,v 1.17 2002-10-02 02:13:22 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
//#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <sys/time.h>
#include <asm/types.h>
#include <linux/soundcard.h>
#include "common/videodev2.h"
#include "common/types.h"
#include "common/log.h"
#include "common/fifo.h"
#include "video/video.h"
#include "audio/mpeg.h"
#include "options.h"

/*
 *  Factory defaults, use system wide configuration file to customize
 */
int			test_mode		= 0;

char 			outFile[256];
int			outFileFD		= STDOUT_FILENO;
int			split_sequence		= FALSE;
long long		part_length		= 0;

int			modules			= 3;			// 1 = Video, 2 = Audio, 4 = VBI
int			mux_syn			= 2;			// 0 = null, elementary, MPEG-1, MPEG-2 PS 

char *			cap_dev			= "/dev/video";

#if defined(HAVE_LIBASOUND)
/* alsa 0.5: card #0, device #0; 0.9: "default" */
char *			pcm_dev			= "alsa";
#elif defined(HAVE_OSS)
char *			pcm_dev			= "/dev/dsp";
#elif defined(USE_ESD)
char *			pcm_dev			= "esd";
#else
char *			pcm_dev			= "";
#endif

char *			mix_dev			= "/dev/mixer";
char *			vbi_dev			= "/dev/vbi";

int			m2i			= 0;
int			width			= 352;
int			height			= 288;
int			grab_width		= 352;
int			grab_height		= 288;
// defaults to width/height if given
int			video_bit_rate		= 2300000;
double			video_num_frames	= DBL_MAX;
long long		video_num_secs		= INT_MAX;
char *			gop_sequence		= "IBBPBBPBBPBB";
// int			frames_per_seqhdr	= 50;
int			filter_mode		= CM_YUV_VERTICAL_DECIMATION;
double			frame_rate		= 1000.0;
double			sample_aspect		= 0.0;			// autodetect
int			preview			= 0;			// 0 = none, XvImage/GTK, progressive
char *			anno			= NULL;
int			luma_only		= 0;			// boolean
int			motion_min		= 0;
int			motion_max		= 0;
int			skip_method		= 0;			// compatible

int			audio_bit_rate		= 80000;
int			audio_bit_rate_stereo	= 160000;
double			audio_num_frames	= DBL_MAX;
long long		audio_num_secs		= INT_MAX;
int			sampling_rate		= 44100;
int			mix_line		= SOUND_MIXER_LINE;	// soundcard.h
int			mix_volume		= 80;			// 0 <= n <= 100
int			audio_mode		= AUDIO_MODE_MONO;
int			psycho_loops		= 0;			// 0 = static psy, low, hi quality
int			mute			= 0;			// bttv specific, boolean

char *			subtitle_pages		= NULL;

#if LARGE_MEM // XXX make this an option
int			cap_buffers		= 4*12;			// capture -> video compression
int			vid_buffers		= 4*8;			// video compression -> mux
int			aud_buffers		= 4*32;			// audio compression -> mux
#else
int			cap_buffers		= 12;			// capture -> video compression
int			vid_buffers		= 8;			// video compression -> mux
int			aud_buffers		= 32;			// audio compression -> mux
#endif

int			cpu_type		= 0;			// detect

/* Work-arounds for the AIW v4l driver */
double                  source_fps              = -1.0;			// manually-defined source fps
int                     fix_interlaced          = 0;			// drop every 2nd sequential field read and enable horizontal decimation
