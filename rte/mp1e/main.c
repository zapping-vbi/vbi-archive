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

/* $Id: main.c,v 1.41 2002-12-14 00:43:43 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif
#include <math.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
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
#include "video/libvideo.h"
#include "video/mpeg.h"
#include "vbi/libvbi.h"
#include "systems/systems.h"
#include "systems/mpeg.h"
#include "common/profile.h"
#include "common/math.h"
#include "common/log.h"
#include "common/mmx.h"
#include "common/bstream.h"
#include "options.h"

#ifndef HAVE_PROGRAM_INVOCATION_NAME
char *			program_invocation_name;
char *			program_invocation_short_name;
#endif

int			verbose;
int			debug_msg; /* v4lx.c */

static pthread_t	audio_thread_id;
static rte_codec *	audio_codec;
static int		stereo;

pthread_t		video_thread_id;
void			(* video_start)(void);
static rte_codec *	video_codec;

static pthread_t	vbi_thread_id;
static fifo *		vbi_cap_fifo;

static multiplexer *	mux;
pthread_t               output_thread_id;

pthread_t		gtk_main_id;
extern void *		gtk_main_thread(void *);

extern int		psycho_loops;

static mp1e_context		context;

//static rte_stream_parameters	audio_params;
static rte_stream_parameters	video_params;
static fifo *			afifo;
static fifo *			vfifo;

extern void options(int ac, char **av);

extern void preview_init(int *argc, char ***argv);

/*
 *  These are functions needed to link without the rte frontend.
 */

static void
vfailure(const char **templ, ... /* to make the compiler happy */)
{
	va_list ap;

	va_start(ap, *templ);
	fprintf(stderr, "Failure: ");
	vfprintf(stderr, *templ, ap);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

void rte_error_printf(rte_context *context, const char *templ, ...)
{ vfailure(&templ); }

void rte_asprintf(char **errstr, const char *templ, ...)
{ vfailure(&templ); }

void rte_unknown_option(rte_context *context, rte_codec *codec, const char *keyword)
{ FAIL("Huh? Unknown option? '%s'", keyword); }

void rte_invalid_option(rte_context *context, rte_codec *codec, const char *keyword, ...)
{ FAIL("Huh? Invalid option? '%s'", keyword); }

char *
rte_strdup(rte_context *context, char **d, const char *s)
{
	char *_new;

	ASSERT("duplicate string", (_new = strdup(s ? s : "")));

	if (d) {
		if (*d) free(*d);
		*d = _new;
	}

	return _new;
}

unsigned int
rte_closest_int(const int *vec, unsigned int len, int val)
{
	unsigned int i, imin = 0;
	int dmin = INT_MAX;

	assert(vec != NULL && len > 0);

	for (i = 0; i < len; i++) {
		int d = fabs(val - vec[i]);

		if (d < dmin) {
			dmin = d;
		        imin = i;
		}
	}

	return imin;
}

unsigned int
rte_closest_double(const double *vec, unsigned int len, double val)
{
	unsigned int i, imin = 0;
	double dmin = DBL_MAX;

	assert(vec != NULL && len > 0);

	for (i = 0; i < len; i++) {
		double d = fabs(val - vec[i]);

		if (d < dmin) {
			dmin = d;
		        imin = i;
		}
	}

	return imin;
}

static void
set_option(rte_codec *codec, char *keyword, ...)
{
	va_list args;

	va_start(args, keyword);
	ASSERT("set option %s", !!codec->_class->option_set(
		codec, keyword, args), keyword);
	va_end(args);
}

static void
get_option(rte_codec *codec, char *keyword, void *r)
{
	ASSERT("get option %s", !!codec->_class->option_get(
		codec, keyword, (rte_option_value *) r), keyword);
}

static void
reset_options(rte_codec *codec)
{
	rte_option_info *option;
	int i = 0;

	while ((option = codec->_class->option_enum(codec, i++))) {
		switch (option->type) {
		case RTE_OPTION_BOOL:
		case RTE_OPTION_INT:
			if (option->menu.num)
				set_option(codec, option->keyword,
					   option->menu.num[option->def.num]);
			else
				set_option(codec, option->keyword,
					   option->def.num);
			break;

		case RTE_OPTION_REAL:
			if (option->menu.dbl)
				set_option(codec, option->keyword,
					   option->menu.dbl[option->def.num]);
			else
				set_option(codec, option->keyword, 
					   option->def.dbl);
			break;

		case RTE_OPTION_STRING:
			if (option->menu.str)
				set_option(codec, option->keyword,
					   option->menu.str[option->def.num]);
			else
				set_option(codec, option->keyword, 
					   option->def.str);
			break;

		case RTE_OPTION_MENU:
			set_option(codec, option->keyword, option->def.num);
			break;

		default:
			fprintf(stderr, "%s: unknown codec option type %d\n",
				__PRETTY_FUNCTION__, option->type);
			exit(EXIT_FAILURE);
		}
	}
}

/* Historic audio option */
void
audio_parameters(int *sampling_freq, int *bit_rate)
{
	int i, imin;
	unsigned int dmin;
	int mpeg_version;

	imin = 0;
        dmin = UINT_MAX;

	for (i = 0; i < 16; i++)
		if (sampling_freq_value[0][i] > 0)
		{
			unsigned int d = nbabs(*sampling_freq - sampling_freq_value[0][i]);

			if (d < dmin) {	    
				if (i / 4 == MPEG_VERSION_2_5)
					continue; // not supported

				dmin = d;
	    			imin = i;
			}
		}

	mpeg_version = imin / 4;

	*sampling_freq = sampling_freq_value[0][imin];

	imin = 0;
        dmin = UINT_MAX;

	// total bit_rate, not per channel

	for (i = 0; i < 16; i++)
		if (bit_rate_value[mpeg_version][i] > 0)
		{
			unsigned int d = nbabs(*bit_rate - bit_rate_value[mpeg_version][i]);

			if (d < dmin) {
				dmin = d;
			        imin = i;
			}
		}

	*bit_rate = bit_rate_value[mpeg_version][imin];
}

static void
incr_file_name(void)
{
	char buf[sizeof(outFile)];
	char *s, *ext;
	int len;

	strcpy(buf, outFile);

	s = buf + strlen(buf);

	for (ext = s - 1; ext >= buf; ext--)
		if (*ext == '.')
			break;

	for (s = (ext >= buf) ? ext : s; s > buf; s--)
		if (!isdigit(s[-1]))
			break;

	len = s - buf;

	snprintf(outFile + len, sizeof(outFile) - len - 1,
		 "%ld%s", strtol(s, NULL, 0) + 1,
		 (ext >= buf) ? ext : "");
}

void break_sequence(void);

void
break_sequence(void)
{
	if (close(outFileFD) == -1) {
		fprintf(stderr,	"%s:" __FILE__ ":" ISTF1(__LINE__) ": "
			"Error when closing output file '%s' - "
			"%d, %s (ignored)\n",
			program_invocation_short_name,
			outFile, errno, strerror(errno));
	}

	incr_file_name();

	do
		outFileFD = open(outFile,
				 O_CREAT | O_TRUNC | O_WRONLY | O_LARGEFILE,
			      /* O_CREAT | O_EXCL | O_WRONLY | O_LARGEFILE, */
				 S_IRUSR | S_IWUSR | S_IRGRP
				 | S_IWGRP | S_IROTH | S_IWOTH);
	while (outFileFD == -1 && errno == EINTR);

	if (outFileFD == -1)
		switch (errno) {
		case EEXIST:
			/* could try another name */

		case ENOSPC:
			/* ditto */

		default:
			ASSERT("open output file '%s'", outFileFD != -1, outFile);
		}

	printv(1, "\rSwitching to output file '%s'\n", outFile);
}


static void
terminate(int signum)
{
	double now;

	printv(3, "Received termination signal\n");

	ASSERT("re-install termination handler", signal(signum, terminate) != SIG_ERR);

	now = current_time();

	if (1)
		mp1e_sync_stop(&context.sync, 0.0); // past times: stop asap
		// XXX NOT SAFE mutexes and signals don't mix
	else {
		printv(0, "Deferred stop in 3 seconds\n");
		mp1e_sync_stop(&context.sync, now + 3.0);
		// XXX allow cancelling
	}

	printv(1, "\nStop at %f\n", now);
}

int
main(int ac, char **av)
{
	struct sigaction action;
	sigset_t block_mask, endm;
	/* XXX encapsulation */
	struct pcm_context *pcm = 0;
	struct filter_param fp[2];
	char *errstr = NULL;

#ifndef HAVE_PROGRAM_INVOCATION_NAME
	program_invocation_short_name = av[0]
	program_invocation_name = av[0];
#endif

	options(ac, av);

#if 0
	if (mux_syn != 4) {
		FAIL("Temporarily out of order. No bug reports please.");
	}
#endif

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

	mp1e_mp2_module_init(test_mode & 1);
	mp1e_mpeg1_module_init(test_mode & 2);
	mp1e_mpeg2_module_init(test_mode & 2);

	if (test_mode >= 1 && test_mode <= 3) {
		printv(1, "Tests passed\n");
		exit(EXIT_SUCCESS);
	}

	if (subtitle_pages)
		modules |= MOD_SUBTITLES;
	else
		modules &= ~MOD_SUBTITLES;

	sigemptyset(&block_mask);
	sigaddset(&block_mask, SIGINT);
	sigprocmask(SIG_BLOCK, &block_mask, NULL);

	sigemptyset(&endm);
	sigaddset(&endm, SIGUSR1);
	action.sa_mask = endm;
	action.sa_flags = 0;
	action.sa_handler = terminate;
	sigaction(SIGUSR1, &action, NULL);

	ASSERT("install termination handler", signal(SIGINT, terminate) != SIG_ERR);

	/* Capture init */

	debug_msg = (verbose >= 3);

	if (modules & MOD_AUDIO) {
		struct stat st;
		int psy_level = audio_mode / 10;

		audio_mode %= 10;

		if (audio_mode == 1) /* joint stereo */
			audio_mode = 0; /* stereo */

		stereo = (audio_mode != AUDIO_MODE_MONO);

		if (!strncasecmp(pcm_dev, "alsa", 4)) {
			audio_parameters(&sampling_rate, &audio_bit_rate);
			mix_init(); // OSS
			open_pcm_alsa(pcm_dev, sampling_rate, stereo, &afifo);
			/* XXX */ pcm = (struct pcm_context *) afifo->user_data;
		} else if (!strcasecmp(pcm_dev, "esd")) {
			audio_parameters(&sampling_rate, &audio_bit_rate);
			mix_init();
			open_pcm_esd(pcm_dev, sampling_rate, stereo, &afifo);
			/* XXX */ pcm = (struct pcm_context *) afifo->user_data;
		} else if (!strcasecmp(pcm_dev, "arts")) {
			audio_parameters(&sampling_rate, &audio_bit_rate);
			mix_init();
			open_pcm_arts(pcm_dev, sampling_rate, stereo, &afifo);
			/* XXX */ pcm = (struct pcm_context *) afifo->user_data;
		} else {
			ASSERT("test file type of '%s'", !stat(pcm_dev, &st), pcm_dev);

			if (S_ISCHR(st.st_mode)) {
				audio_parameters(&sampling_rate, &audio_bit_rate);
				mix_init();
				open_pcm_oss(pcm_dev, sampling_rate, stereo, &afifo);
				/* XXX */ pcm = (struct pcm_context *) afifo->user_data;
			} else {
				open_pcm_afl(pcm_dev, sampling_rate, stereo, &afifo);
				pcm = (struct pcm_context *) afifo->user_data;
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

		video_params.video.width = grab_width;
		video_params.video.height = grab_height;

		if (!stat(cap_dev, &st) && S_ISCHR(st.st_mode)) {
			if (!(vfifo = v4l2_init(&video_params.video, fp)))
				vfifo = v4l_init(&video_params.video, fp);
		} else if (!strncmp(cap_dev, "raw:", 4)) {
			vfifo = raw_init(&video_params.video, fp);
		} else {
			vfifo = file_init(&video_params.video, fp);
		}

		if (sample_aspect > 0.0)
			video_params.video.sample_aspect = sample_aspect;
	}

	if (modules & MOD_SUBTITLES) {
		FAIL("vbi broken, sorry.");
#if 0
		vbi_cap_fifo = vbi_open_v4lx(vbi_dev, -1, FALSE, 30);

		if (vbi_cap_fifo == NULL) {
			if (errstr)
				FAIL("Failed to access vbi device:\n%s\n", errstr);
			else
				FAIL("Failed to access vbi device: Cause unknown\n");
		}
#endif
	}

	/* Compression init */

	mux = mux_alloc(NULL);

	if (audio_num_secs < (audio_num_frames * sampling_rate / 1152.0))
		audio_num_frames = audio_num_secs * sampling_rate / 1152.0;
	if (video_num_secs < (video_num_frames * video_params.video.frame_rate))
		video_num_frames = video_num_secs * video_params.video.frame_rate;

	if (modules & MOD_AUDIO) {
		char *modes[] = { "stereo", "joint stereo", "dual channel", "mono" };
		double n = (video_num_frames / video_params.video.frame_rate)
			/ (1152.0 / sampling_rate);
		rte_stream_parameters rsp;

		printv(1, "Audio compression %2.1f kHz%s %s at %d kbits/s (%1.1f : 1)\n",
			sampling_rate / (double) 1000, sampling_rate < 32000 ? " (MPEG-2)" : "", modes[audio_mode],
			audio_bit_rate / 1000, (double) sampling_rate * (16 << stereo) / audio_bit_rate);

		if (modules & MOD_VIDEO)
			audio_num_frames = n;

		/* Initialize audio codec */

		if (sampling_rate < 32000)
			audio_codec = mp1e_mpeg2_layer2_codec._new(&mp1e_mpeg2_layer2_codec, &errstr);
		else
			audio_codec = mp1e_mpeg1_layer2_codec._new(&mp1e_mpeg1_layer2_codec, &errstr);

		ASSERT("create audio context: %s", audio_codec, errstr);

		audio_codec->context = &context.context;

		reset_options(audio_codec);

		set_option(audio_codec, "sampling_freq", sampling_rate);
		set_option(audio_codec, "bit_rate", audio_bit_rate);
		set_option(audio_codec, "audio_mode", (int) "\1\3\2\0"[audio_mode]);
		set_option(audio_codec, "psycho", psycho_loops);
		set_option(audio_codec, "num_frames", audio_num_frames);

		memset(&rsp, 0, sizeof(rsp));
		rsp.audio.sndfmt = pcm->format;

		ASSERT("set audio parameters",
		       audio_codec->_class->parameters_set(audio_codec, &rsp));

		/* XXX move into codec */
		if (   (sampling_rate >= 32000 && audio_mode == 3 && audio_bit_rate > 192000)
		    || (sampling_rate >= 32000 && audio_mode != 3 && audio_bit_rate < 64000))
			printv(0, "Warning: audio bit rate %d bits/s exceeds decoder requirements, "
			       "stream will not be standard compliant.\n", audio_bit_rate);

		/* preliminary */
		{
			mp1e_codec *meta = PARENT(audio_codec, mp1e_codec, codec);
			int buffer_size = 1 << (ffsr(meta->output_buffer_size) + 1);

			meta->sstr.this_module = MOD_AUDIO;
			meta->output =
				mux_add_input_stream(mux, AUDIO_STREAM, "audio-mp2",
						     buffer_size, aud_buffers,
						     meta->output_frame_rate,
						     meta->output_bit_rate);
			meta->input = afifo;
			meta->codec.state = RTE_STATE_RUNNING;
		}
	}

	if (modules & MOD_VIDEO) {
		double coded_rate;

		video_coding_size(width, height, m2i);

		/* Initialize video codec */

		if (m2i)
			video_codec = mp1e_mpeg2_video_codec._new(&mp1e_mpeg2_video_codec, &errstr);
		else
			video_codec = mp1e_mpeg1_video_codec._new(&mp1e_mpeg1_video_codec, &errstr);

		ASSERT("create video context: %s", video_codec, errstr);

		video_codec->context = &context.context;

		reset_options(video_codec);

		set_option(video_codec, "bit_rate", video_bit_rate);
		set_option(video_codec, "coded_frame_rate",
			/* w/o MAX we'd get 23.976 which isn't nice */
			MAX(24.0, video_params.video.frame_rate));
		get_option(video_codec, "coded_frame_rate", &coded_rate);		

		if (frame_rate > coded_rate || mux_syn == 4 /* vcd */)
			frame_rate = coded_rate;

		printv(2, "Macroblocks %d x %d\n", mb_width, mb_height);

		printv(1, "Video compression %d x %d, %2.1f frames/s at %1.2f Mbits/s (%1.1f : 1)\n",
			width, height, (double) frame_rate,
			video_bit_rate / 1e6, (width * height * 1.5 * 8 * frame_rate) / video_bit_rate);

		if (motion_min == 0 || motion_max == 0)
			printv(1, "Motion compensation disabled\n");
		else
			printv(1, "Motion compensation %d-%d\n", motion_min, motion_max);

		set_option(video_codec, "virtual_frame_rate", frame_rate);
		set_option(video_codec, "skip_method", skip_method);
		set_option(video_codec, "gop_sequence", gop_sequence);
//	        set_option(video_codec, "motion_compensation", motion_min > 0 && motion_max > 0);
//		set_option(video_codec, "desaturate", !!luma_only);
		set_option(video_codec, "anno", anno);
		set_option(video_codec, "num_frames", video_num_frames);

		memcpy(&PARENT(video_codec, mpeg1_context, codec.codec)->filter_param, fp, sizeof(fp));

		ASSERT("set video parameters",
		       video_codec->_class->parameters_set(video_codec, &video_params));

		/* preliminary */
		{
			mp1e_codec *meta = PARENT(video_codec, mp1e_codec, codec);

			meta->sstr.this_module = MOD_VIDEO;
			meta->output =
				mux_add_input_stream(mux, VIDEO_STREAM, "video-mpeg1",
						     meta->output_buffer_size, vid_buffers,
						     meta->output_frame_rate,
						     meta->output_bit_rate);
			meta->input = vfifo;
			meta->codec.state = RTE_STATE_RUNNING;
		}

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

	if (outFile[0]) {
		/* increment file name instead of overwriting files?
		   for now compatibility rules */
		do
			outFileFD = open(outFile,
					 O_CREAT | O_TRUNC | O_WRONLY | O_LARGEFILE,
				      /* O_CREAT | O_EXCL | O_WRONLY | O_LARGEFILE, */
					 S_IRUSR | S_IWUSR | S_IRGRP
					 | S_IWGRP | S_IROTH | S_IWOTH);
		while (outFileFD == -1 && errno == EINTR);

		ASSERT("open output file '%s'", outFileFD != -1, outFile);
	}

	ASSERT("initialize output routine", init_output_stdout(mux));

	// pause loop? >>

#if 0 /* TEST */
	mp1e_sync_init(&context.sync, modules, 0); /* TOD reference */
#else
	if (modules & MOD_VIDEO)
		/* Use video as time base (broadcast and v4l2 assumed) */
		mp1e_sync_init(&context.sync, modules, MOD_VIDEO);
	else if (modules & MOD_SUBTITLES)
		mp1e_sync_init(&context.sync, modules, MOD_SUBTITLES);
	else
		mp1e_sync_init(&context.sync, modules, MOD_AUDIO);
#endif

	if (modules & MOD_AUDIO) {
		ASSERT("create audio compression thread",
			!pthread_create(&audio_thread_id, NULL,
			mp1e_mp2, audio_codec));

		printv(2, "Audio compression thread launched\n");
	}

	if (modules & MOD_VIDEO) {
		if (m2i)
			ASSERT("create video compression thread",
			       !pthread_create(&video_thread_id, NULL,
					       mp1e_mpeg2, video_codec));
		else
			ASSERT("create video compression thread",
			       !pthread_create(&video_thread_id, NULL,
					       mp1e_mpeg1, video_codec));

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

	if ((split_sequence || part_length) && mux_syn >= 3)
		FAIL("Sorry, -z and -X%d do not combine\n", mux_syn); 

	mp1e_sync_start(&context.sync, 0.0);

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
		FAIL("MPEG-2 mux out of order, sorry.\n");
		printv(1, "MPEG-2 Program Stream\n");
		mpeg2_program_stream_mux(mux);
		break;
	case 4:
		if (modules != MOD_VIDEO + MOD_AUDIO)
			FAIL("Please add option -m3 (audio + video) for VCD\n");
		if (audio_mode == 3)
		  	printv(0, "Warning: audio mode mono is not VCD 1.x compliant.\n");
		if (audio_bit_rate != 224000)
		  	printv(0, "Warning: audio bit rate %d kbit/s is not VCD 1.x compliant. "
			       "Should be 224 kbit/s.\n", audio_bit_rate / 1000);
		if (sampling_rate < 32000)
		  	printv(0, "Warning: audio sampling rate %d Hz is not VCD 1.x compliant.\n",
			       sampling_rate);
		else if (sampling_rate != 44100)
		  	printv(0, "Warning: audio sampling rate %d Hz is probably not "
			       "VCD 1.x compliant. Suggest 44100 Hz.\n", sampling_rate);
		if (width > 352
		    || (frame_rate < 29 && height > 288)
		    || (frame_rate >= 29 && height > 240))
		  	printv(0, "Warning: image size %d x %d pixels is not VCD 1.x compliant, "
			       "Should be 352 x 240 (PAL/NTSC) or 352 x 288 (PAL).\n", width, height);
		if (fabs(video_bit_rate - 1152000) > 900)
		  	printv(0, "Warning: video bit rate %d kbit/s is not VCD 1.x compliant. "
			       "Should be 1152 kbit/s.\n", video_bit_rate / 1000);
		printv(1, "VCD %s MPEG-1 Stream\n",
		       (frame_rate < 29) ? "1.1" : "1.0");
		vcd_system_mux(mux);
		break;
	}

	// << pause loop? XXX make threads re-enter safe, counters etc.

	mux_free(mux);

	printv(1, "\n%s: Done.\n", program_invocation_short_name);

	pr_report();

	return EXIT_SUCCESS;
}
