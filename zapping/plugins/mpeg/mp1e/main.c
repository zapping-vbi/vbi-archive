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

/* $Id: main.c,v 1.2 2000-07-04 19:46:05 garetxe Exp $ */

#define MAIN_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <string.h>
#include <getopt.h>
#include <math.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <asm/types.h>
#include "videodev2.h"
#include <linux/soundcard.h>
#include "profile.h"
#include "audio/mpeg.h"
#include "video/mpeg.h"
#include "video/video.h"
#include "audio/audio.h"
#include "systems/systems.h"
#include "misc.h"
#include "log.h"
#include "options.h"
#include "mmx.h"
#include "bstream.h"

char *			my_name;
int			verbose;

#define 		PACKET_SIZE		2048
#define 		PACKETS_PER_PACK	16

pthread_mutex_t		mux_mutex;
pthread_cond_t		mux_cond;
int			bytes_out;
unsigned char *		mux_buffer;
double			video_stop_time = 1e30;
double			audio_stop_time = 1e30;

fifo			aud;
pthread_t		audio_thread_id;
short *			(* audio_read)(double *);
void			(* audio_unget)(short *);
int			stereo;

fifo			vid;
pthread_t		video_thread_id;
void			(* video_start)(void);
unsigned char *		(* video_wait_frame)(double *, int *);
void			(* video_frame_done)(int);
void			(* video_unget_frame)(int);
int			min_cap_buffers;

pthread_t               output_thread_id;

pthread_t		tk_main_id;
extern void *		tk_main(void *);

extern int		mux_mode;
extern int		psycho_loops;
extern int		audio_num_frames;
extern int		video_num_frames;

extern void options(int ac, char **av);

extern void preview_init(void);

extern void audio_init(void);
extern void video_init(void);

volatile int quit_please = 0;
volatile int program_shutdown = 0;

void
terminate(int signum)
{
	struct timeval tv;
	static volatile int entry = 0;

	printv(3, "Received termination signal\n");

	entry++;

	ASSERT("re-install termination handler", signal(signum, terminate) != SIG_ERR);

	if (entry == 1) {
		gettimeofday(&tv, NULL);
		video_stop_time = tv.tv_sec + tv.tv_usec / 1e6;
		
		if (mux_mode == 2)
			audio_stop_time = video_stop_time;

		printv(1, "\nStop\n");
	}

	quit_please = 1;
}

int
main(int ac, char **av)
{
	my_name = av[0];

	options(ac, av);

	if (!cpu_id(ARCH_PENTIUM_MMX))
		FAIL("Sorry, this program requires an MMX enhanced CPU");

	ASSERT("install termination handler", signal(SIGINT, terminate) != SIG_ERR);

	pthread_mutex_init(&mux_mutex, NULL);
	pthread_cond_init(&mux_cond, NULL);

#if TEST_PREVIEW
	if (preview > 0)
		ASSERT("create tk thread",
			!pthread_create(&tk_main_id, NULL, tk_main, NULL));
#endif

	/* Capture init */

	if (mux_mode & 2) {
		struct stat st;
		int psy_level = audio_mode / 10;

		audio_mode %= 10;

		stereo = (audio_mode != AUDIO_MODE_MONO);

		ASSERT("probe '%s'", !stat(pcm_dev, &st), pcm_dev);

		if (S_ISCHR(st.st_mode)) {
			audio_parameters(&sampling_rate, &audio_bit_rate);
			mix_init();
			pcm_init();
		} else {
			tsp_init();
			// pick up file parameters
			audio_parameters(&sampling_rate, &audio_bit_rate);
		}

		if ((audio_bit_rate >> stereo) < 80000 || psy_level >= 1) {
			psycho_loops = MAX(psycho_loops, 1);

			if (sampling_rate < 32000 || psy_level >= 2)
				psycho_loops = 2;

			psycho_loops = MAX(psycho_loops, 2);
		}
	}

	if (mux_mode & 1) {
		struct stat st;

		{
			char *s = gop_sequence;
			int count = 0;

			min_cap_buffers = 0;

			do {
				if (*s == 'B')
					count++;
				else {
					if (count > min_cap_buffers)
						min_cap_buffers = count;
					count = 0;
				}
			} while (*s++);

			min_cap_buffers++;
		}

		if (!stat(cap_dev, &st) && S_ISCHR(st.st_mode))
#ifdef V4L2_MAJOR_VERSION
			v4l2_init();
#else
			v4l_init();
#endif
		else
			file_init();
	}

	/* Compression init */

	if (mux_mode & 2) {
		char *modes[] = { "stereo", "joint stereo", "dual channel", "mono" };

		printv(1, "Audio compression %2.1f kHz%s %s at %d kbits/s (%1.1f : 1)\n",
			sampling_rate / (double) 1000, sampling_rate < 32000 ? " (MPEG-2)" : "", modes[audio_mode],
			audio_bit_rate / 1000, (double) sampling_rate * (16 << stereo) / audio_bit_rate);

		aud_buffers = init_fifo(&aud, "audio compression", 2048 << stereo, aud_buffers);

		printv(2, "Allocated %d audio compression buffers\n", aud_buffers);

		if (mux_mode & 1)
			audio_num_frames = INT_MAX;

		audio_init();
	}

	if (mux_mode & 1) {
		video_coding_size(width, height);

		if (frame_rate > frame_rate_value[frame_rate_code])
			frame_rate = frame_rate_value[frame_rate_code];

		printv(2, "Macroblocks %d x %d\n", mb_width, mb_height);

		printv(1, "Video compression %d x %d, %2.1f frames/s at %1.2f Mbits/s (%1.1f : 1)\n",
			width, height, (double) frame_rate,
			video_bit_rate / 1e6, (width * height * 1.5 * 8 * frame_rate) / video_bit_rate);

		vid_buffers = init_fifo(&vid, "video compression", mb_num * 384 * 4, vid_buffers);

		printv(2, "Allocated %d video compression buffers\n", vid_buffers);

		video_init();

#if TEST_PREVIEW
		if (preview > 0)
			preview_init();
#endif
		video_start();
	}

	ASSERT("open output files", output_init("temp.mpeg") >= 0);

	ASSERT("create output thread",
	       !pthread_create(&output_thread_id, NULL, output_thread, NULL));

	printv(2, "Output thread launched\n");

	if ((mux_mode & 3) == 3)
		mpeg1_system_run_in();

	if (mux_mode & 2) {
		ASSERT("create audio compression thread",
			!pthread_create(&audio_thread_id, NULL,
			stereo ? stereo_audio_compression_thread : audio_compression_thread, NULL));

		printv(2, "Audio compression thread launched\n");
	}

	if (mux_mode & 1) {
		ASSERT("create video compression thread",
			!pthread_create(&video_thread_id, NULL, video_compression_thread, NULL));

		printv(2, "Video compression thread launched\n");
	}

	if ((mux_mode & 3) == 3)
		mpeg1_system_mux(NULL);
	else
		stream_output((mux_mode == 1) ? &vid : &aud);

	printv(2, "Doing cleanup\n");

	program_shutdown = 1;

	printv(3, "\noutput thread... ");

	output_end();
	printv(3, "done\n");

	/* i cannot get this to work */
/*	if (mux_mode & 1)
	printv(3, "\nvideo thread... "),
			pthread_join(video_thread_id, NULL),
			printv(3, "done\n");*/

	if (mux_mode & 2)
		printv(3, "\naudio thread... "),
			pthread_join(audio_thread_id, NULL),
			printv(3, "done\n");

	printv(2, "\nCleanup done, bye...\n");
	
	pr_report();

	return EXIT_SUCCESS;
}
