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

/* $Id: main.c,v 1.25 2000-10-17 06:18:45 mschimek Exp $ */

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
#include "common/remote.h"
#include "options.h"

char *			my_name;
int			verbose;

double			video_stop_time = 1e30;
double			audio_stop_time = 1e30;
double			vbi_stop_time = 1e30;

pthread_t		audio_thread_id;
fifo *			audio_cap_fifo;
int			stereo;

pthread_t		video_thread_id;
fifo *			video_cap_fifo;
void			(* video_start)(void);

pthread_t		vbi_thread_id;
fifo *			vbi_cap_fifo;
extern void		vbi_v4l2_init(void);
extern void		vbi_init(void);
extern void *		vbi_thread(void *);

pthread_t               output_thread_id;

pthread_t		tk_main_id;
extern void *		tk_main(void *);

extern int		psycho_loops;
extern int		audio_num_frames;
extern int		video_num_frames;

extern void options(int ac, char **av);

extern void preview_init(void);

extern void audio_init(void);
extern void video_init(void);


static void
terminate(int signum)
{
	struct timeval tv;
	double now;

	printv(3, "Received termination signal\n");

	ASSERT("re-install termination handler", signal(signum, terminate) != SIG_ERR);

	gettimeofday(&tv, NULL);

	now = tv.tv_sec + tv.tv_usec / 1e6;

#if USE_REMOTE
	if (1)
		remote_stop(0.0); // past times: stop asap
		// XXX mutexes and signals don't mix
	else {
		printv(0, "Deferred stop in 3 seconds\n");
		remote_stop(now + 3.0);
		// XXX allow cancelling
	}
#else
	video_stop_time =
	vbi_stop_time =
	audio_stop_time = tv.tv_sec + tv.tv_usec / 1e6;
// XXX unsafe? atomic set, once only, potential rc
#endif

	printv(1, "\nStop at %f\n", now);
}

int
main(int ac, char **av)
{
	sigset_t block_mask;
	pthread_t mux_thread; /* Multiplexer thread */

	my_name = av[0];

	if (!cpu_id(ARCH_PENTIUM_MMX))
		FAIL("Sorry, this program requires an MMX enhanced CPU");

	options(ac, av);

	if (subtitle_pages)
		modules |= MOD_SUBTITLES;
	else
		modules &= ~MOD_SUBTITLES;

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

	if (modules & MOD_AUDIO) {
		struct stat st;
		int psy_level = audio_mode / 10;

		audio_mode %= 10;

		stereo = (audio_mode != AUDIO_MODE_MONO);

		if (!strncmp(pcm_dev, "alsa", 4)) {
			audio_parameters(&sampling_rate, &audio_bit_rate);
			mix_init(); // XXX OSS
			alsa_pcm_init();
		} else {
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
		}

		if ((audio_bit_rate >> stereo) < 80000 || psy_level >= 1) {
			psycho_loops = MAX(psycho_loops, 1);

			if (sampling_rate < 32000 || psy_level >= 2)
				psycho_loops = 2;

			psycho_loops = MAX(psycho_loops, 2);
		}
	}

	if (modules & MOD_VIDEO) {
		struct stat st;

		if (!stat(cap_dev, &st) && S_ISCHR(st.st_mode))
#ifdef V4L2_MAJOR_VERSION
			v4l2_init();
#else
			v4l_init();
#endif
		else
			file_init();
	}

	if (modules & MOD_SUBTITLES) {
		vbi_v4l2_init();
	}

	/* Compression init */

	mucon_init(&mux_mucon);

	if (modules & MOD_AUDIO) {
		char *modes[] = { "stereo", "joint stereo", "dual channel", "mono" };
		long long n = llroundn(((double) video_num_frames / frame_rate_value[frame_rate_code])
			/ (1152.0 / sampling_rate));

		printv(1, "Audio compression %2.1f kHz%s %s at %d kbits/s (%1.1f : 1)\n",
			sampling_rate / (double) 1000, sampling_rate < 32000 ? " (MPEG-2)" : "", modes[audio_mode],
			audio_bit_rate / 1000, (double) sampling_rate * (16 << stereo) / audio_bit_rate);

		if (modules & MOD_VIDEO)
			audio_num_frames = MIN(n, (long long) INT_MAX);

		audio_init();
	}

	if (modules & MOD_VIDEO) {
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
	}

	if (modules & MOD_SUBTITLES) {
		if (!*subtitle_pages)
			printv(1, "Recording Teletext, verbatim\n");
		else
			printv(1, "Recording Teletext, page %s\n", subtitle_pages);

		vbi_init();
	}

	ASSERT("initialize output routine", init_output_stdout());

#if USE_REMOTE
	// pause loop? >>

	remote_init(modules);
#else
	synchronize_capture_modules(TRUE);
#endif

	if (modules & MOD_AUDIO) {
		ASSERT("create audio compression thread",
			!pthread_create(&audio_thread_id, NULL,
			stereo ? mpeg_audio_layer_ii_stereo :
				 mpeg_audio_layer_ii_mono, NULL));

		printv(2, "Audio compression thread launched\n");
	}

	if (modules & MOD_VIDEO) {
		ASSERT("create video compression thread",
			!pthread_create(&video_thread_id, NULL,
				mpeg1_video_ipb, NULL));

		printv(2, "Video compression thread launched\n");
	}

	if (modules & MOD_SUBTITLES) {
		ASSERT("create vbi thread",
			!pthread_create(&vbi_thread_id, NULL,
				vbi_thread, NULL));

		printv(2, "VBI thread launched\n");
	}

	sigprocmask(SIG_UNBLOCK, &block_mask, NULL);
	// Unblock only in main thread

	if ((modules == MOD_VIDEO || modules == MOD_AUDIO)
		&& mux_syn >= 2)
		mux_syn = 1; // compatibility

	switch (mux_syn) {
	case 0:
		ASSERT("create stream nirvana thread",
		       !pthread_create(&mux_thread, NULL,
				       stream_sink, NULL));
		break;
	case 1:
		ASSERT("create elementary stream thread",
		       !pthread_create(&mux_thread, NULL,
				       elementary_stream_bypass, NULL));
		break;
	case 2:
		ASSERT("create mpeg1 system mux",
		       !pthread_create(&mux_thread, NULL,
				       mpeg1_system_mux, NULL));
		break;
	case 3:
		printv(1, "MPEG-2 Program Stream\n");
		ASSERT("create mpeg2 system mux",
		       !pthread_create(&mux_thread, NULL,
				       mpeg2_program_stream_mux, NULL));
		break;
	}

#if USE_REMOTE
	/*
	 *  Engines are running (ie. capturing),
	 *  let's hit the record button.
	 */
	if (1)
    		remote_start(0.0); // past times: start as soon as possible
	else {
		struct timeval tv;

		printv(0, "Deferred start in 3 seconds\n");

		gettimeofday(&tv, NULL);

    		remote_start(3.0 + tv.tv_sec + tv.tv_usec / 1e6);
	}
#endif

#if 0
	// unsafe: numframes, stop_time < mux finish time, cuts off end code
	/* wait until completition (SIGINT handler) */
	do {
		usleep(100000); /* 0.1 s*/
		gettimeofday(&tv, NULL);
	} while (audio_stop_time > (tv.tv_sec + tv.tv_usec / 1e6));
#endif
/*
   Suffice to suspend execution until mux_thread terminates.

   #1, still no way to wake up eg. an X11 event loop when
   num_frames have been done. Only a stop button routine
   could suspend until termination. fd? For async feedback
   in general?

   #2, stop_time must be protected by a mutex to guarantee
   atomic reads. Problem: mutexes & signal handlers don't mix.

   #3, compression from files fakes timestamps, Ctrl-C no workee.
*/
	pthread_join(mux_thread, NULL);

	// cancel & join all threads here
	// << pause loop? XXX make threads re-enter safe, counters etc.

	mux_cleanup();

	printv(1, "\n%s: Done.\n", my_name);

	pr_report();

	return EXIT_SUCCESS;
}
