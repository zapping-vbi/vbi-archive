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

/* $Id: main.c,v 1.20 2000-09-24 20:58:06 garetxe Exp $ */

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
#include <linux/videodev.h>
#include "audio/mpeg.h"
#include "video/mpeg.h"
#include "video/video.h"
#include "audio/audio.h"
#include "systems/systems.h"
#include "common/profile.h"
#include "common/math.h"
#include "common/log.h"
#include "common/mmx.h"
#include "common/bstream.h"
#include "options.h"

/* 
 * The code to test RTE has moved to rtemain.c (builds rte_test), mp1e
 * will be built from this file.
 * fixme: The RTE-related code should be removed, this is becoming a
 * bit of a mess. Probably it won't work with the current rte code
 * anyway.
 */
#define RTE 0

#if RTE

#include "rtepriv.h"
#include "main.h"

#endif // RTE

char *			my_name;
int			verbose;

double			video_stop_time = 1e30;
double			audio_stop_time = 1e30;

pthread_t		audio_thread_id;
fifo *			audio_cap_fifo;
int			stereo;

pthread_t		video_thread_id;
fifo *			video_cap_fifo;
void			(* video_start)(void);
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

#if RTE

volatile int program_shutdown = 0;

pthread_t video_emulation_thread_id;
pthread_t audio_emulation_thread_id;
pthread_mutex_t video_device_mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t audio_device_mutex=PTHREAD_MUTEX_INITIALIZER;

fifo *			ye_olde_audio_cap_fifo;

// obsolete
unsigned char *		(* ye_olde_wait_frame)(double *, int *);
void			(* ye_olde_frame_done)(int);

void emulation_data_callback(void * data, double * time, int video,
			     rte_context * context, void * user_data)
{
	int frame;
	void * misc_data;
	buffer *b;

	if (video) {
		pthread_mutex_lock(&video_device_mutex);
		misc_data = ye_olde_wait_frame(time, &frame);
		pthread_mutex_unlock(&video_device_mutex);
		memcpy(data, misc_data, context->video_bytes);
		ye_olde_frame_done(frame);
	}
	else {
		pthread_mutex_lock(&audio_device_mutex);
		b = wait_full_buffer(ye_olde_audio_cap_fifo);
		misc_data = b->data;
		*time = b->time;
		pthread_mutex_unlock(&audio_device_mutex);
		memcpy(data, misc_data, context->audio_bytes);
		send_empty_buffer(ye_olde_audio_cap_fifo, b);
	}
}

void * video_emulation_thread (void * ptr)
{
	int frame;
	double timestamp;
	unsigned char * data;
	void * video_data;
	rte_context * context = (rte_context *)ptr;

	data = rte_push_video_data(context, NULL, 0);
	for (;data;) {
		pthread_mutex_lock(&video_device_mutex);
		video_data = ye_olde_wait_frame(&timestamp, &frame);
		pthread_mutex_unlock(&video_device_mutex);
		memcpy(data, video_data, context->video_bytes);
		data = rte_push_video_data(context, data, timestamp);
		ye_olde_frame_done(frame);
	}
	fprintf(stderr, "video emulation: %s\n", context->error);

	return NULL;
}

void * audio_emulation_thread (void * ptr)
{
	double timestamp;
	short * data;
	short * audio_data;
	rte_context * context = (rte_context *)ptr;
	buffer *b;

	data = rte_push_audio_data(context, NULL, 0);
	for (;data;) {
		pthread_mutex_lock(&audio_device_mutex);
		b = wait_full_buffer(ye_olde_audio_cap_fifo);
		audio_data = (short *) b->data;
		timestamp = b->time;
		pthread_mutex_unlock(&audio_device_mutex);
		memcpy(data, audio_data, context->audio_bytes);
		data = rte_push_audio_data(context, data, timestamp);
		send_empty_buffer(ye_olde_audio_cap_fifo, b);
	}
	fprintf(stderr, "audio emulation: %s\n", context->error);
	return NULL;
}

/*
  This is just for a preliminary testing, loads of things need to be
  done before this gets really functional.
  push + callbacks don't work together yet, but separately they
  do. Does anybody really want to use them together?
  The push interface works great, not much more CPU usage and nearly
  no lost frames; the callbacks one needs a bit more CPU, and drops
  some more frames, but works fine too.
*/
int emulation_thread_init ( void )
{
	rte_context * context;
	int do_test = 1; /* 1 == push, 2 == callbacks, 3 == both */
	rteDataCallback callback;
	enum rte_pixformat format;

	ye_olde_wait_frame = video_wait_frame;
	ye_olde_frame_done = video_frame_done;

	ye_olde_audio_cap_fifo = audio_cap_fifo;

	if (!rte_init())
		return 0;

	if (do_test & 2)
		callback = RTE_DATA_CALLBACK(emulation_data_callback);
	else
		callback = NULL;

	context = rte_context_new("temp.mpeg", width, height, RTE_RATE_3,
				  NULL, callback, (void*)0xdeadbeef);

	if (!context) {
		fprintf(stderr, "%s\n", context->error);
		return 0;
	}

	switch (filter_mode) {
	case CM_YUYV:
		format = RTE_YUYV;
		break;
	case CM_YUV:
		format = RTE_YUV420;
		break;
	case CM_YUYV_VERTICAL_DECIMATION:
	case CM_YUYV_TEMPORAL_INTERPOLATION:
	case CM_YUYV_VERTICAL_INTERPOLATION:
	case CM_YUYV_PROGRESSIVE:
	case CM_YUYV_PROGRESSIVE_TEMPORAL:
	case CM_YUYV_EXP:
	case CM_YUYV_EXP_VERTICAL_DECIMATION:
	case CM_YUYV_EXP2:
		format = (filter_mode-CM_YUYV_VERTICAL_DECIMATION) + 
			RTE_YUYV_VERTICAL_DECIMATION;
		break;
	default:
		printv(1, "filter mode not supported: %d\nfalling back "
		       "to YUYV mode\n", filter_mode);
		format = RTE_YUYV;
		break;
	}

	rte_set_video_parameters(context, format, context->width,
				 context->height, context->video_rate,
				 context->output_video_bits);

	if (!rte_start(context))
	{
		fprintf(stderr, "%s\n", context->error);
		rte_context_destroy(context);
		return 0;
	}

	if (do_test & 1) {
		pthread_create(&video_emulation_thread_id, NULL,
			       video_emulation_thread, context);
		pthread_create(&audio_emulation_thread_id, NULL,
			       audio_emulation_thread, context);
	}

	return 1;
}

#endif // RTE

static void
terminate(int signum)
{
	struct timeval tv;

	printv(3, "Received termination signal\n");

	ASSERT("re-install termination handler", signal(signum, terminate) != SIG_ERR);

	gettimeofday(&tv, NULL);

	video_stop_time =
	audio_stop_time = tv.tv_sec + tv.tv_usec / 1e6;

// XXX unsafe? atomic set, once only, potential rc

	printv(1, "\nStop\n");
}

int
main(int ac, char **av)
{
	sigset_t block_mask;
	struct timeval tv;
	pthread_t mux_thread; /* Multiplexer thread */

	my_name = av[0];

	if (!cpu_id(ARCH_PENTIUM_MMX))
		FAIL("Sorry, this program requires an MMX enhanced CPU");

	options(ac, av);

	sigemptyset(&block_mask);
	sigaddset(&block_mask, SIGINT);
	sigprocmask(SIG_BLOCK, &block_mask, NULL);

	ASSERT("install termination handler", signal(SIGINT, terminate) != SIG_ERR);

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

	mucon_init(&mux_mucon);

	if (mux_mode & 2) {
		char *modes[] = { "stereo", "joint stereo", "dual channel", "mono" };
		long long n = llroundn(((double) video_num_frames / frame_rate_value[frame_rate_code])
			/ (1152.0 / sampling_rate));

		printv(1, "Audio compression %2.1f kHz%s %s at %d kbits/s (%1.1f : 1)\n",
			sampling_rate / (double) 1000, sampling_rate < 32000 ? " (MPEG-2)" : "", modes[audio_mode],
			audio_bit_rate / 1000, (double) sampling_rate * (16 << stereo) / audio_bit_rate);

		if (mux_mode & 1)
			audio_num_frames = MIN(n, (long long) INT_MAX);

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

		video_init();

#if TEST_PREVIEW
		if (preview > 0)
			preview_init();
#endif
		video_start(); // bah. need a start/restart function, prob. start_time
	}

#if RTE

	if (!emulation_thread_init())
		return 0;

	ASSERT("open output files", output_init() >= 0);

	ASSERT("create output thread",
	       !pthread_create(&output_thread_id, NULL, output_thread, NULL));

	printv(2, "Output thread launched\n");

#else // !RTE

	ASSERT("initialize output routine", init_output_stdout());

#endif // !RTE

	if ((mux_mode & 3) == 3)
		synchronize_capture_modules();

	if (mux_mode & 2) {
		ASSERT("create audio compression thread",
			!pthread_create(&audio_thread_id, NULL,
			stereo ? mpeg_audio_layer_ii_stereo :
				 mpeg_audio_layer_ii_mono, NULL));

		printv(2, "Audio compression thread launched\n");
	}

	if (mux_mode & 1) {
		ASSERT("create video compression thread",
			!pthread_create(&video_thread_id, NULL,
				mpeg1_video_ipb, NULL));

		printv(2, "Video compression thread launched\n");
	}

	sigprocmask(SIG_UNBLOCK, &block_mask, NULL);
	// Unblock only in main thread

	if ((mux_mode & 3) != 3)
		mux_syn = 0;

	/*
	 *  XXX Thread these, not UI.
	 *  Async completion indicator?? 
	 */
	switch (mux_syn) {
	case 0:
		ASSERT("create elementary stream thread",
		       !pthread_create(&mux_thread, NULL,
				       elementary_stream_bypass, NULL));
		break;
	case 1:
		ASSERT("create mpeg1 system mux",
		       !pthread_create(&mux_thread, NULL,
				       mpeg1_system_mux, NULL));
		break;
	case 2:
		printv(1, "MPEG-2 Program Stream");
		ASSERT("create mpeg1 system mux",
		       !pthread_create(&mux_thread, NULL,
				       mpeg2_program_stream_mux, NULL));
		break;
	}

	/* wait until completition (SIGINT handler) */
	do {
		usleep(100000); /* 0.1 s*/
		gettimeofday(&tv, NULL);
	} while (audio_stop_time > (tv.tv_sec + tv.tv_usec / 1e6));
	
	pthread_join(mux_thread, NULL);

	mux_cleanup();

	printv(1, "\n%s: Done.\n", my_name);

#if RTE

	printv(2, "Doing cleanup\n");

	program_shutdown = 1;

	if (mux_mode & 1) {
		printv(3, "\nvideo thread... ");
		pthread_cancel(video_thread_id);
		pthread_join(video_thread_id, NULL);
		printv(3, "done\n");
	}

	if (mux_mode & 2) {
		printv(3, "\naudio thread... ");
		pthread_cancel(audio_thread_id);
		pthread_join(audio_thread_id, NULL);
		printv(3, "done\n");
	}

	printv(3, "\noutput thread... ");

	output_end();
	printv(3, "done\n");

	printv(2, "\nCleanup done, bye...\n");

#endif // RTE

	pr_report();

	return EXIT_SUCCESS;
}
