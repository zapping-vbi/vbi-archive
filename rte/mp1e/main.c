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

/* $Id: main.c,v 1.8 2001-09-20 23:35:07 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

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
#include "common/videodev2.h"
#include "audio/libaudio.h"
#include "audio/mpeg.h"
#include "video/video.h"
#include "video/mpeg.h"
#include "vbi/libvbi.h"
#include "systems/systems.h"
#include "common/profile.h"
#include "common/math.h"
#include "common/log.h"
#include "common/mmx.h"
#include "common/bstream.h"
#include "common/sync.h"
#include "common/errstr.h"
#include "options.h"

#ifndef HAVE_PROGRAM_INVOCATION_NAME
char *			program_invocation_name;
char *			program_invocation_short_name;
#endif

int			verbose;
int			debug_msg; /* v4lx.c */

static pthread_t	audio_thread_id;
static fifo *		audio_cap_fifo;
int			stereo;

pthread_t		video_thread_id;
static fifo *		video_cap_fifo;
void			(* video_start)(void);

static pthread_t	vbi_thread_id;
static fifo *		vbi_cap_fifo;

static multiplexer *	mux;
pthread_t               output_thread_id;

pthread_t		gtk_main_id;
extern void *		gtk_main_thread(void *);

extern int		psycho_loops;
extern int		audio_num_frames;
extern int		video_num_frames;

extern void options(int ac, char **av);

extern void preview_init(int *argc, char ***argv);

extern void video_init(multiplexer *mux);


static void
terminate(int signum)
{
	double now;

	printv(3, "Received termination signal\n");

	ASSERT("re-install termination handler", signal(signum, terminate) != SIG_ERR);

	now = current_time();

	if (1)
		mp1e_sync_stop(0.0); // past times: stop asap
		// XXX NOT SAFE mutexes and signals don't mix
	else {
		printv(0, "Deferred stop in 3 seconds\n");
		mp1e_sync_stop(now + 3.0);
		// XXX allow cancelling
	}

	printv(1, "\nStop at %f\n", now);
}

int
main(int ac, char **av)
{
	sigset_t block_mask;

#ifndef HAVE_PROGRAM_INVOCATION_NAME
	program_invocation_short_name =
	program_invocation_name = argv[0];
#endif

	options(ac, av);

	if (cpu_type == CPU_UNKNOWN) {
		cpu_type = cpu_detection();

		if (cpu_type == CPU_UNKNOWN)
			FAIL("Sorry, this program requires an MMX enhanced CPU");
	}
#ifdef HAVE_LIBMMXEMU
	else {
		extern void mmxemu_configure(int);

		mmxemu_configure(cpu_type);
	}
#endif

	switch (cpu_type) {
	case CPU_K6_2:
	case CPU_CYRIX_III:
		printv(2, "Using 3DNow! optimized routines.\n");
		break;

	case CPU_PENTIUM_III:
	case CPU_ATHLON:
		printv(2, "Using SSE optimized routines.\n");
		break;

	case CPU_PENTIUM_4:
		printv(2, "Using SSE2 optimized routines.\n");
		break;
	}

	if (subtitle_pages)
		modules |= MOD_SUBTITLES;
	else
		modules &= ~MOD_SUBTITLES;


	sigemptyset(&block_mask);
	sigaddset(&block_mask, SIGINT);
	sigprocmask(SIG_BLOCK, &block_mask, NULL);

	ASSERT("install termination handler", signal(SIGINT, terminate) != SIG_ERR);

	/* Capture init */

	debug_msg = (verbose >= 3);

	if (modules & MOD_AUDIO) {
		struct stat st;
		int psy_level = audio_mode / 10;

		audio_mode %= 10;

		stereo = (audio_mode != AUDIO_MODE_MONO);

		if (!strncasecmp(pcm_dev, "alsa", 4)) {
			audio_parameters(&sampling_rate, &audio_bit_rate);
			mix_init(); // OSS
			audio_cap_fifo = open_pcm_alsa(pcm_dev, sampling_rate, stereo);
		} else if (!strcasecmp(pcm_dev, "esd")) {
			audio_parameters(&sampling_rate, &audio_bit_rate);
			mix_init(); /* fixme: esd_mix_init? */
			audio_cap_fifo = open_pcm_esd(pcm_dev, sampling_rate, stereo);
		} else {
			ASSERT("test file type of '%s'", !stat(pcm_dev, &st), pcm_dev);

			if (S_ISCHR(st.st_mode)) {
				audio_parameters(&sampling_rate, &audio_bit_rate);
				mix_init();
				audio_cap_fifo = open_pcm_oss(pcm_dev, sampling_rate, stereo);
			} else {
				/* XXX encapsulation */
				struct pcm_context *pcm;

				audio_cap_fifo = open_pcm_afl(pcm_dev, sampling_rate, stereo);
				pcm = (struct pcm_context *) audio_cap_fifo->user_data;
				stereo = pcm->stereo;
				sampling_rate = pcm->sampling_rate;
				/* This not to override joint/bilingual */
				if (audio_mode == AUDIO_MODE_MONO && stereo)
					audio_mode = AUDIO_MODE_STEREO;
				else if (audio_mode != AUDIO_MODE_MONO && !stereo)
					audio_mode = AUDIO_MODE_MONO;
				audio_parameters(&sampling_rate, &audio_bit_rate);
				if (sampling_rate != pcm->sampling_rate) /* XXX */
					FAIL("Cannot encode file '%s' with sampling rate %d Hz, sorry.",
						pcm_dev, pcm->sampling_rate);
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

		if (!stat(cap_dev, &st) && S_ISCHR(st.st_mode)) {
			if (!(video_cap_fifo = v4l2_init()))
				video_cap_fifo = v4l_init();
		} else
			video_cap_fifo = file_init();
	}

	if (modules & MOD_SUBTITLES) {
		vbi_cap_fifo = vbi_open_v4lx(vbi_dev, -1, FALSE, 30);

		if (vbi_cap_fifo == NULL) {
			if (errstr)
				FAIL("Failed to access vbi device:\n%s\n", errstr);
			else
				FAIL("Failed to access vbi device: Cause unknown\n");
		}
	}

	/* Compression init */

	mux = mux_alloc(NULL);

	if (modules & MOD_AUDIO) {
		char *modes[] = { "stereo", "joint stereo", "dual channel", "mono" };
		long long n = llroundn(((double) video_num_frames / frame_rate_value[frame_rate_code])
			/ (1152.0 / sampling_rate));

		printv(1, "Audio compression %2.1f kHz%s %s at %d kbits/s (%1.1f : 1)\n",
			sampling_rate / (double) 1000, sampling_rate < 32000 ? " (MPEG-2)" : "", modes[audio_mode],
			audio_bit_rate / 1000, (double) sampling_rate * (16 << stereo) / audio_bit_rate);

		if (modules & MOD_VIDEO)
			audio_num_frames = MIN(n, (long long) INT_MAX);

		audio_init(sampling_rate, stereo, /* pcm_context* */
			audio_mode, audio_bit_rate, psycho_loops, mux);
	}

	if (modules & MOD_VIDEO) {
		video_coding_size(width, height);

		if (frame_rate > frame_rate_value[frame_rate_code])
			frame_rate = frame_rate_value[frame_rate_code];

		printv(2, "Macroblocks %d x %d\n", mb_width, mb_height);

		printv(1, "Video compression %d x %d, %2.1f frames/s at %1.2f Mbits/s (%1.1f : 1)\n",
			width, height, (double) frame_rate,
			video_bit_rate / 1e6, (width * height * 1.5 * 8 * frame_rate) / video_bit_rate);

		if (motion_min == 0 || motion_max == 0)
			printv(1, "Motion compensation disabled\n");
		else
			printv(1, "Motion compensation %d-%d\n", motion_min, motion_max);

		video_init(mux);

#if TEST_PREVIEW
		if (preview > 0) {
			printv(2, "Initialize preview\n");
			preview_init(&ac, &av);
			ASSERT("create gtk thread",
			       !pthread_create(&gtk_main_id, NULL,
					       gtk_main_thread, NULL));
		}
#endif
	}

	if (modules & MOD_SUBTITLES) {
		if (subtitle_pages && *subtitle_pages)
			printv(1, "Recording Teletext, page %s\n", subtitle_pages);
		else
			printv(1, "Recording Teletext, verbatim\n");

		vbi_init(vbi_cap_fifo, mux);
	}

	ASSERT("initialize output routine", init_output_stdout());

	// pause loop? >>

#if 0 /* TEST */
	mp1e_sync_init(modules, 0); /* TOD reference */
#else
	if (modules & MOD_VIDEO)
		/* Use video as time base (broadcast and v4l2 assumed) */
		mp1e_sync_init(modules, MOD_VIDEO);
	else if (modules & MOD_SUBTITLES)
		mp1e_sync_init(modules, MOD_SUBTITLES);
	else
		mp1e_sync_init(modules, MOD_AUDIO);
#endif

	if (modules & MOD_AUDIO) {
		ASSERT("create audio compression thread",
			!pthread_create(&audio_thread_id, NULL,
			stereo ? mpeg_audio_layer_ii_stereo :
				 mpeg_audio_layer_ii_mono,
				 audio_cap_fifo));

		printv(2, "Audio compression thread launched\n");
	}

	if (modules & MOD_VIDEO) {
		ASSERT("create video compression thread",
			!pthread_create(&video_thread_id, NULL,
				mpeg1_video_ipb, video_cap_fifo));

		printv(2, "Video compression thread launched\n");
	}

	if (modules & MOD_SUBTITLES) {
		ASSERT("create vbi thread",
			!pthread_create(&vbi_thread_id, NULL,
				vbi_thread, vbi_cap_fifo)); // XXX

		printv(2, "VBI thread launched\n");
	}

	// Unblock only in main thread
	sigprocmask(SIG_UNBLOCK, &block_mask, NULL);

	if ((modules == MOD_VIDEO || modules == MOD_AUDIO) && mux_syn >= 2)
		mux_syn = 1; // compatibility

	mp1e_sync_start(0.0);

	switch (mux_syn) {
	case 0:
		stream_sink(mux);
		break;
	case 1:
		if (popcnt(modules) > 1)
			FAIL("No elementary stream, change option -m or -X");
		elementary_stream_bypass(mux);
		break;
	case 2:
		mpeg1_system_mux(mux);
		break;
	case 3:
		printv(1, "MPEG-2 Program Stream\n");
		mpeg2_program_stream_mux(mux);
		break;
	case 4:
		if (modules != MOD_VIDEO + MOD_AUDIO)
			FAIL("Please add option -m3 (audio + video) for VCD\n");
		printv(1, "VCD MPEG-1 Stream\n");
		vcd_system_mux(mux);
		break;
	}

	// << pause loop? XXX make threads re-enter safe, counters etc.

	mux_free(mux);

	printv(1, "\n%s: Done.\n", program_invocation_short_name);

	pr_report();

	return EXIT_SUCCESS;
}
