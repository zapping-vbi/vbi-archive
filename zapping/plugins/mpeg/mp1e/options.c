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

/* $Id: options.c,v 1.8 2000-09-30 19:38:44 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <limits.h>
#include <unistd.h>
#include <sys/time.h>
#include <asm/types.h>
#include <linux/videodev.h>
#include <linux/soundcard.h>
#include "common/types.h"
#include "common/log.h"
#include "common/fifo.h"
#include "video/video.h"
#include "audio/audio.h"
#include "audio/mpeg.h"

/*
 *  Factory defaults, use system wide configuration file to customize
 */

int			modules			= 3;			// 1 = Video, 2 = Audio, 4 = VBI
int			mux_syn			= 2;			// 0 = null, elementary, MPEG-1, MPEG-2 PS 

char *			cap_dev			= "/dev/video";
char *			pcm_dev			= "/dev/dsp";
char *			mix_dev			= "/dev/mixer";
char *			vbi_dev			= "/dev/vbi";

int			width			= 352;
int			height			= 288;
int			grab_width		= 352;
int			grab_height		= 288;
int			video_bit_rate		= 2300000;
int			video_num_frames	= INT_MAX;
char *			gop_sequence		= "IBBPBBPBBPBB";
int			frames_per_seqhdr	= INT_MAX;
int			filter_mode		= CM_YUV;
double			frame_rate		= 1000.0;
int			preview			= 0;			// 0 = none, XvImage/Tk, progressive
char *			anno			= NULL;
int			luma_only		= 0;			// boolean

int			audio_bit_rate		= 80000;
int			audio_bit_rate_stereo	= 160000;
int			audio_num_frames	= INT_MAX;
int			sampling_rate		= 44100;
int			mix_line		= SOUND_MIXER_LINE;	// soundcard.h
int			mix_volume		= 80;			// 0 <= n <= 100
int			audio_mode		= AUDIO_MODE_MONO;
int			psycho_loops		= 0;			// 0 = static, low, hi quality
int			mute			= 0;			// bttv specific, boolean

char *			subtitle_pages		= NULL;

int			cap_buffers		= 12;			// capture -> video compression
int			vid_buffers		= 8;			// video compression -> mux
int			aud_buffers		= 32;			// audio compression -> mux
/*
 *  NB vid_buffers and aud_buffers occupy physical memory only as fifo
 *  load requires. Consider page swapping delays. cap_buffers occupy
 *  physical memory or device memory permanently.
 */

static const char *mux_options[] = { "", "video", "audio", "video_and_audio" };
static const char *mux_syn_options[] = { "nirvana", "bypass", "mpeg1", "mpeg2-ps" };
static const char *audio_options[] = { "stereo", "", "dual_channel", "mono" };
static const char *mute_options[] = { "unmute", "mute", "ignore" };

void
usage(FILE *fi)
{
	fprintf(fi,
		"Real time MPEG-1 encoder " VERSION "\n"
		"Copyright (C) 1999-2000 Michael H. Schimek\n"
		"\n"
		"This is free software licensed without a fee under the terms of the\n"
		"GNU General Public License Version 2. NO WARRANTIES.\n"
		"\n"
		"Usage: %s [options]\n\n"
		"Option                                                      Default\n"
		" -m mode        Encode 1 = video, 2 = audio, 3 = both       %s\n"
		" -v             Increase verbosity level, try -v, -vv\n"
		"\n"
		" -b bps         Output video bits per second                %5.3f Mbits/s\n"
#ifdef V4L2_MAJOR_VERSION
		" -c name        Video capture device (V4L2 API)             %s\n"
		" -F mode        Filter mode\n"
#else
		" -c name        Video capture device (V4L API)              %s\n"
#endif
		" -f frames      Frames per second                           maximum\n"
		" -g string      Group of pictures sequence (display order)  %s\n"
		" -n frames      Number of video (audio) frames to encode\n"
		"                or until termination (Ctrl-C). To break\n"
		"                immediately hit Ctrl-\\                      years\n"
		" -s wxh         Image size (centred)                        %d x %d pixels\n"
		" -G wxh         Grab size, multiple of 16 x 16              %d x %d pixels\n"
		" -H frames      Repeat sequence header every n frames,\n"
		"                n > 0. Helps random access                  never repeated\n"
#if TEST_PREVIEW && defined(HAVE_LIBXV)
		" -P             XvImage Preview (test mode)                 disabled\n"
#endif
		"\n"
		" -a mode        Audio mode 0 = stereo, 2 = dual channel,\n"
		"                3 = mono                                    %s\n"
		" -p name        PCM sampling device (OSS API)               %s\n"
		" -B bps         Output audio bits per second                %d kbits/s\n"
		" -S rate        Audio sampling rate                         %2.1f kHz\n"
		"\n"
		" -r line,vol    Audio record source 1..30%s,\n"
		"                volume 0..100                               %d,%d\n"
		" -x name        Audio mixer device (OSS API)                %s\n"
#ifdef V4L2_MAJOR_VERSION
		" -M mode        RF audio 0 = unmute, 1 = mute, 2 = ignore   %s\n"
#endif
		"\n"
		"A configuration file can be piped in from standard input,\n"
		"the compressed stream will be sent to standard output.\n",

		my_name, mux_options[modules], (double) video_bit_rate / 1e6,
		cap_dev, gop_sequence, width, height, grab_width, grab_height,

		audio_options[audio_mode], pcm_dev, audio_bit_rate / 1000, sampling_rate / 1e3,

		mix_sources(), mix_line, mix_volume, mix_dev, mute_options[mute]);

	exit((fi == stderr) ? EXIT_FAILURE : EXIT_SUCCESS);
}

static const struct option
long_options[] = {
	{ "audio_mode",			required_argument, NULL, 'a' },
	{ "video_bit_rate",		required_argument, NULL, 'b' },
	{ "capture_device",		required_argument, NULL, 'c' },
	{ "frame_rate",			required_argument, NULL, 'f' },
	{ "gop_sequence",		required_argument, NULL, 'g' },
	{ "mux_mode",			required_argument, NULL, 'm' },
	{ "modules",			required_argument, NULL, 'm' },
	{ "num_frames",			required_argument, NULL, 'n' },
	{ "help",			no_argument,	   NULL, 'h' },
	{ "config",			required_argument, NULL, 'i' },
	{ "letterbox",			no_argument,	   NULL, 'l' },
	{ "pcm_device",			required_argument, NULL, 'p' },
	{ "rec_source",			required_argument, NULL, 'r' },
	{ "image_size",			required_argument, NULL, 's' },
	{ "verbose",			optional_argument, NULL, 'v' },
	{ "bw",				no_argument,	   NULL, 'w' },
	{ "mixer_device",		required_argument, NULL, 'x' },
	{ "anno",			required_argument, NULL, 'A' },
	{ "audio_bit_rate",		required_argument, NULL, 'B' },
	{ "filter",			required_argument, NULL, 'F' },
	{ "grab_size",			required_argument, NULL, 'G' },
	{ "frames_per_seq_header",	required_argument, NULL, 'H' },
	{ "vbi_device",			required_argument, NULL, 'I' },
	{ "mute",			required_argument, NULL, 'M' },
	{ "preview",			required_argument, NULL, 'P' },
	{ "sampling_rate",		required_argument, NULL, 'S' },
	{ "subtitle_pages",		required_argument, NULL, 'T' },
	{ "version",			no_argument,	   NULL, 'V' },
	{ "mux",			required_argument, NULL, 'X' },
	{ NULL }
};

static void
options_from_file(char *name, bool fail)
{
	static bool parse_option(int c);
	static int recursion = 0;
	char *s, *p, buffer[300];
	FILE *fi;
	int i;

	if (recursion++ > 20)
		FAIL("Recursion in option file"); // probably

	if (!strcmp(name, "stdin"))
		fi = stdin;
	else if (!(fi = fopen(name, "r")) && !fail) {
		printv(3, "Cannot open '%s' (ignored)\n", name);
		return;
	}

	ASSERT("open options file '%s'", fi != NULL, name);

	printv(2, "Opened '%s'\n", name ? name : "stdin");

	while (fgets(buffer, sizeof(buffer) - 2, fi)) {
		for (s = buffer; *s && *s != '#'; s++);
		*s = 0;

		while (s > buffer && isspace(s[-1])) s--;
		*s = 0;

		for (s = buffer; isspace(*s); s++);

		if (!*s)
			continue;

		p = s;

		while (!isspace(*s)) s++;
		while (isspace(*s)) *s++ = 0;
		if (*s == '=') *s++ = 0;
		while (isspace(*s)) s++;

		optarg = *s ? s : NULL;

		for (i = 0; long_options[i].name; i++)
			if (!strcmp(p, long_options[i].name))
				break;

		if (!long_options[i].val)
			FAIL("Unrecognized option '%s' in file '%s'", p, name);

		if (!optarg && long_options[i].has_arg == required_argument)
			FAIL("Option '%s' in file '%s' requires an argument", p, name);

		if (!parse_option(long_options[i].val))
			FAIL("Incorrect parameters for option '%s' in file '%s'", p, name);
	}

	ASSERT("read error in file '%s'", !ferror(fi), name);

	if (strcmp(name, "stdin"))
		fclose(fi);

	recursion--;
}

static int
suboption(const char **optp, int n, int def)
{
	int i;

	if (optarg) {
		if (isdigit(optarg[0]))
			return strtol(optarg, NULL, 0);

		for (i = 0; i < n; i++) {
			if (!optp[i])
				continue;

			if (!strcmp(optarg, optp[i]))
				return i;
		}

		return -1;
	}

	return def;
}

static bool have_audio_bit_rate = FALSE;
static bool have_image_size = FALSE;
static bool have_grab_size = FALSE;
static bool have_letterbox = FALSE;

static bool
parse_option(int c)
{
	switch (c) {
		case 'a':
			if ((audio_mode = suboption(audio_options, 4, audio_mode)) < 0)
				return FALSE;
			/*
			 *  0 = stereo, 1 = joint stereo, 2 = dual channel, 3 = mono;
			 *  To override psychoacoustic analysis trade-off:
			 *  +0  level 0 (no analysis)
			 *  +10 level 1
			 *  +20 level 2 (full analysis)
			 *
			 *  Higher levels are forced at low bit rates.
			 */
			break;

		case 'b': // video bit rate (bits/s, kbit/s or Mbits/s)
		{
			double rate = strtod(optarg, NULL);
			
			if (rate >= 20e3)
				video_bit_rate = rate;
			else if (rate >= 20)
				video_bit_rate = rate * 1e3;
			else
				video_bit_rate = rate * 1e6;

			if (video_bit_rate < 32000 ||
			    video_bit_rate > 20e6)
				return FALSE;

			break;
		}

		case 'c':
			cap_dev = strdup(optarg);
			break;

		case 'f':
			frame_rate = strtod(optarg, NULL);
			if (frame_rate <= 0.0)
				return FALSE;
			break;

		case 'g':
		{
			int i, len = strlen(optarg);

			if (len == 0)
				break;

			gop_sequence = strdup(optarg);

			for (i = 0; i < len; i++)
				gop_sequence[i] = toupper(gop_sequence[i]);

			break;
		}

		case 'm':
			modules = suboption(mux_options, 4, modules);
			if (modules <= 0 || modules > 7)
				return FALSE;
			break;

		case 'n':
			video_num_frames =
			audio_num_frames = strtol(optarg, NULL, 0);
			break;

		case 'h':
			usage(stdout);
			break;

		case 'i':
			options_from_file(optarg, TRUE);
			break;

		case 'l':
			have_letterbox = TRUE;
			break;

		case 'p':
			pcm_dev = strdup(optarg);
			break;

		case 'r':
			sscanf(optarg, "%d,%d", &mix_line, &mix_volume);
			if (mix_line < 0 || mix_line > 30 ||
			    mix_volume < 0 || mix_volume > 100)
				return FALSE;
			break;

	        case 's':
			have_image_size = TRUE;
			sscanf(optarg, "%dx%d", &width, &height);
			if (width < 1 || width > MAX_WIDTH ||
			    height < 1 || height > MAX_HEIGHT)
				return FALSE;
			break;

		case 'v':
			if (optarg)
				verbose = strtol(optarg, NULL, 0);
			else
				verbose++;
			break;

		case 'w':
			luma_only = !luma_only;
			break;

		case 'x':
			mix_dev = strdup(optarg);
			break;

		case 'A':
			anno = strdup(optarg);
			break;

		case 'B': // audio bit rate (bits/s or kbits/s)
		{
			double rate = strtod(optarg, NULL);

			if (rate >= 1e3)
				audio_bit_rate = rate;
			else
				audio_bit_rate = rate * 1e3;

			have_audio_bit_rate = TRUE;

			break;
		}

		case 'F':
			filter_mode = strtol(optarg, NULL, 0);
			if (filter_mode <= 0 ||
			    filter_mode >= CM_NUM_MODES)
				return FALSE;
			break;

	        case 'G':
			have_grab_size = TRUE;
			sscanf(optarg, "%dx%d", &grab_width, &grab_height);
			if (width < 1 || width > MAX_WIDTH ||
			    height < 1 || height > MAX_HEIGHT)
				return FALSE;
			break;

	        case 'H':
			frames_per_seqhdr = strtol(optarg, NULL, 0);
			if (frames_per_seqhdr < 1)
				return FALSE;
			break;

		case 'I':
			vbi_dev = strdup(optarg);
			break;

		case 'M':
			mute = suboption(mute_options, 3, mute);
			if (mute < 0 || mute > 2)
				return FALSE;
			break;

		case 'P':
#if !TEST_PREVIEW
			FAIL("Not configured for preview\n");
#endif
			preview++;
			break;

		case 'S': // audio sampling rate (Hz or kHz)
		{
			double rate = strtod(optarg, NULL);

			if (rate >= 1e3)
				sampling_rate = rate;
			else
				sampling_rate = rate * 1e3;

			break;
		}

		case 'T':
			subtitle_pages = strdup(optarg);
			break;

		case 'V':
			puts("mp1e" " " VERSION);
			exit(EXIT_SUCCESS);

		case 'X':
			mux_syn = suboption(mux_syn_options, 4, 2);
			if (mux_syn < 0 || mux_syn > 3)
				return FALSE;
			break;

		default:
			usage(stderr);
	}

	return TRUE;
}

static void
bark(void)
{
	if ((audio_mode & 3) != AUDIO_MODE_MONO && !have_audio_bit_rate)
		audio_bit_rate = audio_bit_rate_stereo;

	if (!have_image_size && have_grab_size) {
		width = grab_width;
		height = grab_height;
	} else if (have_image_size && !have_grab_size) {
		grab_width = width;
		grab_height = height;
	} else if (width > grab_width || height > grab_height)
		FAIL("Image size must be <= grab size\n");

	if (have_letterbox)
		height = height * 3 / 4;

	if (gop_sequence[0] != 'I' ||
	    strspn(gop_sequence, "IPB") != strlen(gop_sequence) ||
	    strlen(gop_sequence) > 1023)
		FAIL("Invalid group of pictures sequence: \"%s\".\n"
		     "A valid sequence can consist of the picture types 'I' (intra coded),\n"
		     "'P' (forward predicted), and 'B' (bidirectionally predicted) in any\n"
		     "order headed by an 'I' picture.", gop_sequence);

	have_audio_bit_rate = FALSE;
	have_image_size = FALSE;
	have_grab_size = FALSE;
	have_letterbox = FALSE;
}

void
options(int ac, char **av)
{
	int index, c;
	char *s, *mp1erc = NULL;

	if ((s = getenv("HOME"))) {
		if ((mp1erc = malloc(strlen(s) + 16))) {
			strcpy(mp1erc, s);
			strcat(mp1erc, "/.mp1erc");
		}
	}

	options_from_file("/usr/local/etc/mp1e.conf", FALSE);

	if (mp1erc) {
		options_from_file(mp1erc, FALSE);
		free(mp1erc);
	}

	bark();

	if (!isatty(STDIN_FILENO))
		options_from_file("stdin", FALSE);

	while ((c = getopt_long(ac, av, "a:b:c:f:g:hi:lm:n:p:r:s:vwx:A:B:F:G:H:I:M:PS:T:VX:",
		long_options, &index)) != -1)
		if (!parse_option(c))
			usage(stderr);

	bark();

	if (width * height < 128000)
		cap_buffers *= 2;
}
