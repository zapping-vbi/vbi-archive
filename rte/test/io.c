/*
 *  librte test
 *
 *  Copyright (C) 2002 Michael H. Schimek
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

/* $Id: io.c,v 1.12 2005-02-25 18:16:22 mschimek Exp $ */

#undef NDEBUG

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#endif

#include "librte.h"

#ifndef BYTE_ORDER /* BSD feat */
/* FIXME a GNU libc feature */
#define BYTE_ORDER __BYTE_ORDER
#define LITTLE_ENDIAN __LITTLE_ENDIAN
#define BIG_ENDIAN __BIG_ENDIAN
#endif

#define MAX_CODECS		16

/* Globals */

static rte_context *		context;
static rte_codec *		codec;
static rte_codec_info *		dinfo;

static int			audio_tracks = 0;
static int			video_tracks = 0;

static int			fd;

/* Options */

static int			io_mode = 1;
static int			blocking;
static int			queue;
static int			sleep_secs = 3;
static char *			context_key = "mp1e_mpeg_audio";
static char *			codec_key[MAX_CODECS];
static int			num_codecs = 0;

struct generator {
	void			(* func)(struct generator *,
					 void *, int, double *);
	double			byte_period;
	int			buffer_size;

	double			time;

	rte_stream_parameters	params;

	union {
		struct {
			double			freq;
			double			w;
			int			sample_count;
		}			audio;
		struct {
			uint32_t		yuyv_band[8 * 8 * 8 + 1];
			double			frame_var;
			double			frame_vel;
		}			video;
	}			u;
};

/* Sine wave generator */

static void
audio(struct generator *gen, void *data, int size, double *timep)
{
	int16_t *s;

	if (0)
		fprintf (stderr, "read %p %d #%d\n",
			 data, size, gen->u.audio.sample_count);

	*timep = gen->time;
	gen->time += size * gen->byte_period;

	/* FIXME */
	if (num_codecs > 1)
		usleep(size * gen->byte_period * 1e6);

	s = data;

	for (size /= sizeof (int16_t) * gen->params.audio.channels;
	     size--; ++gen->u.audio.sample_count) {
		int sample;
		unsigned int i;

		sample = sin (gen->u.audio.sample_count * gen->u.audio.w)
			* 25000;

		for (i = 0; i < gen->params.audio.channels; ++i)
			*s++ = sample;
	}
}

static void
init_audio(struct generator *g)
{
	fprintf(stderr, "Audio generator track #%d: %d Hz 16 bit mono\n",
		audio_tracks, 220 << audio_tracks);

	g->func = audio;
	g->time = 1000.0;
	g->u.audio.freq = 220 << audio_tracks;
	g->u.audio.sample_count = 0;

	audio_tracks++;

	memset(&g->params, 0, sizeof(g->params));

	switch (BYTE_ORDER) {
	case LITTLE_ENDIAN:
		g->params.audio.sndfmt = RTE_SNDFMT_S16_LE;
		break;

	case BIG_ENDIAN:
		g->params.audio.sndfmt = RTE_SNDFMT_S16_BE;
		break;

	default:
		assert (!"Unknown machine endianess");
		break;
	}

	g->params.audio.sampling_freq = 44100;
	g->params.audio.channels = 1;

	if (!rte_parameters_set(codec, &g->params)) {
		fprintf(stderr, "Sampling parameter negotiation failed: %s\n",
			rte_errstr(context));
		exit(EXIT_FAILURE);
	}

	switch (g->params.audio.sndfmt) {
	case RTE_SNDFMT_S16_LE:
	case RTE_SNDFMT_S16_BE:
		break;

	default:
		fprintf(stderr, "Sorry, cannot generate sample format %d "
			"required by codec.\n",
			g->params.audio.sndfmt);
		exit(EXIT_FAILURE);
	}

	g->u.audio.w = g->u.audio.freq
		/ (double) g->params.audio.sampling_freq * M_PI * 2;
	g->byte_period = 1.0 / (g->params.audio.sampling_freq
				* sizeof(int16_t)); 
	g->buffer_size = g->params.audio.fragment_size;
		/* recommended minimum */
}

/* Image generator */

static void
video(struct generator *g, void *data, int size, double *timep)
{
	uint8_t *canvas = data;
	int x, y, i;

//	fprintf(stderr, "read %p %d %f\n", data, size, g->time);

	g->u.video.frame_var += g->u.video.frame_vel;

	for (y = 0; y < g->params.video.height; y += 4) {
		for (x = 0; x < g->params.video.width; x += 4) {
			uint32_t yuyv;

			switch (BYTE_ORDER) {
			case LITTLE_ENDIAN:
				yuyv = ((int)((x ^ y) * g->u.video.frame_var)
					& 0xFF)	* 0x00010001 + 0x80008000;
				break;

			case BIG_ENDIAN:
				yuyv = ((int)((x ^ y) * g->u.video.frame_var)
					& 0xFF)	* 0x01000100 + 0x00800080;
				break;

			default:
				assert (!"Unknown machine endianess");
				break;
			}

			for (i = 0; i < 4; i++) {
				uint32_t *d = (uint32_t *)(canvas
					+ g->params.video.offset
					+ (y + i) * g->params.video.stride
					+ x * 2);

				d[0] = yuyv;
				d[1] = yuyv;
			}
		}
	}

	i = (g->params.video.height >> 1) & -16;

	for (y = i - 16; y < i + 17; y++) {
		uint32_t *d = (uint32_t *)(canvas
			+ g->params.video.offset + y * g->params.video.stride);

		memcpy(d, g->u.video.yuyv_band, g->params.video.width * 2);
	}

	*timep = g->time;
	g->time += size * g->byte_period;
}

static int
saturate(int val)
{
	if (val < 0)
		return 0;
	if (val > 255)
		return 255;

	return val;
}

static double
max(double val)
{
	if (val > 1.0)
		return 1.0;

	return val;
}

static void
init_video(struct generator *gen)
{
	double r, g, b, i;
	double Y, Cb, Cr;
	int x;

	fprintf(stderr, "Video generator track #%d: 352x288 YUYV 24 Hz\n",
		video_tracks);

	gen->func = video;
	gen->time = 1000.0;
	gen->u.video.frame_var = 0;
	gen->u.video.frame_vel = 0.05;

	video_tracks++;

	memset(&gen->params, 0, sizeof(gen->params));

	gen->params.video.pixfmt = RTE_PIXFMT_YUYV;
	gen->params.video.frame_rate = 24.0;
	gen->params.video.sample_aspect = 1.0;
	gen->params.video.width = 352;
	gen->params.video.height = 288;

	if (!rte_parameters_set(codec, &gen->params)) {
		fprintf(stderr, "Sampling parameter negotiation failed: %s\n",
			rte_errstr(context));
		exit(EXIT_FAILURE);
	}

	if (gen->params.video.pixfmt != RTE_PIXFMT_YUYV) {
		fprintf(stderr, "Sorry, cannot generate sample format #%d "
			"required by codec, only YUYV.\n", gen->params.video.pixfmt);
		exit(EXIT_FAILURE);
	}

	gen->buffer_size = gen->params.video.stride * gen->params.video.height * 2;
	gen->byte_period = 1.0 / (gen->params.video.frame_rate * gen->buffer_size);

	for (x = 0; x < gen->params.video.width >> 1; x++) {
		i = x / (double)(gen->params.video.width >> 1);
		r = 1.0 - max(fabs(i - 0.25) * 2);
		g = 1.0 - max(fabs(i - 0.5) * 2);
		b = 1.0 - max(fabs(i - 0.75) * 2);

		/* ITU-R Rec. 601 */

		Y  = (+ 0.2989 * r + 0.5866 * g + 0.1145 * b) * 255.0;
		Cb = (- 0.1687 * r - 0.3312 * g + 0.5000 * b) * 255.0;
		Cr = (+ 0.5000 * r - 0.4183 * g - 0.0816 * b) * 255.0;

		switch (BYTE_ORDER) {
		case LITTLE_ENDIAN:
			gen->u.video.yuyv_band[x] =
				+ (saturate(Y * 219.0 / 255.0 + 16) << 0)
				+ (saturate(Cb * 224.0 / 255.0 + 128) << 8)
				+ (saturate(Y * 219.0 / 255.0 + 16) << 16)
				+ (saturate(Cr * 224.0 / 255.0 + 128) << 24);
			break;

		case BIG_ENDIAN:
			gen->u.video.yuyv_band[x] =
				+ (saturate(Y * 219.0 / 255.0 + 16) << 24)
				+ (saturate(Cb * 224.0 / 255.0 + 128) << 16)
				+ (saturate(Y * 219.0 / 255.0 + 16) << 8)
				+ (saturate(Cr * 224.0 / 255.0 + 128) << 0);
			break;

		default:
			assert (!"Unknown machine endianess");
			break;
		}
	}
}

/* Input method #1 callback master */

static rte_bool
read_cm_cb(rte_context *context, rte_codec *codec, rte_buffer *buffer)
{
	struct generator *gen = rte_codec_user_data(codec);

	/* A real recording app allocates in main(), this is just a test. */
	buffer->data = malloc(gen->buffer_size);
	buffer->size = gen->buffer_size;

//	fprintf(stderr, "%p read_cm_cb %p\n", buffer, buffer->data);

	gen->func(gen, buffer->data, buffer->size, &buffer->timestamp);

	return TRUE;
}

static rte_bool
unref_cb(rte_context *context, rte_codec *codec, rte_buffer *buffer)
{
//	fprintf(stderr, "%p unref %p\n", buffer, buffer->data);

	free(buffer->data);

	return TRUE;
}

/* Input method #2 callback slave */

static rte_bool
read_cs_cb(rte_context *context, rte_codec *codec, rte_buffer *buffer)
{
	struct generator *gen = rte_codec_user_data(codec);

	gen->func(gen, buffer->data, buffer->size, &buffer->timestamp);

	return TRUE;
}

/* Input method #3 push master */

rte_buffer buffer;

static void
mainloop_pm(void)
{
	int count;

	for (count = 0; count < 200; count++) {
		read_cm_cb(NULL, NULL, &buffer);

		/*
		 *  NB the codec may or may not be another thread, so this
		 *  function not call write_cb() directly.
		 */
		if (!rte_push_buffer(codec, &buffer, blocking)) {
			fprintf(stderr, "The codec is not fast enough, we drop frame #%d.\n", count);
			unref_cb(NULL, NULL, &buffer);
		}
	}

	fprintf(stderr, "Mainloop-pa finished.\n");
}

/* Input method #4 push slave */

static void
mainloop_ps(void)
{
	int count;

	for (count = 0; count < 200; count++) {
		read_cs_cb(NULL, NULL, &buffer);

		if (!rte_push_buffer(codec, &buffer, blocking)) {
			fprintf(stderr, "The codec is not fast enough, we drop frame #%d.\n", count);
		} else {
			/* buffer.data pointer changed */
		}
	}

	fprintf(stderr, "Mainloop-pp finished.\n");
}

/* Output method #2 callback slave */

static rte_bool
write_cb(rte_context *context, rte_codec *codec, rte_buffer *buffer)
{
	if (!buffer) /* EOF */
		return TRUE;

//	fprintf(stderr, "write %p %d\n", buffer->data, buffer->size);

	write(fd, buffer->data, buffer->size);
	return TRUE;
}

static rte_bool
seek_cb(rte_context *context, long long offset, int whence)
{
	if (fd == STDOUT_FILENO) {
		fprintf(stderr, "Codec needs seeking which isn't possible on stdout.\n"
			        "Please give argument -o filename\n");
		exit(EXIT_FAILURE);
	}

#if defined(HAVE_LARGEFILE) && defined(O_LARGEFILE)
	lseek64(fd, offset, whence);
#else
	if (offset < INT_MIN || offset > INT_MAX)
		return FALSE; 

	lseek(fd, offset, whence);
#endif

	return TRUE;
}

static const char *short_options = "d:o:q:s:x:";

#ifdef HAVE_GETOPT_LONG
static const struct option
long_options[] = {
	{ "cm",		no_argument,		&io_mode,		1 },
	{ "cs",		no_argument,		&io_mode,		2 },
	{ "pm",		no_argument,		&io_mode,		3 },
	{ "ps",		no_argument,		&io_mode,		4 },
	{ "block",	no_argument,		&blocking,		TRUE },
	{ "queue",	required_argument,	NULL,			'q' },
	{ "sleep",	required_argument,	NULL,			's' },
	{ "context",	required_argument,	NULL,			'x' },
	{ "codec",	required_argument,	NULL,			'd' },
	{ "output",	required_argument,	NULL,			'o' },
	{ 0, 0, 0, 0 }
};
#else
#define getopt_long(ac, av, s, l, i) getopt(ac, av, s)
#endif

/* XXX remove */
void ccmalloc_atexit(void);
void ccmalloc_atexit(void) {}

int
main(int argc, char **argv)
{
	int c, i;
	int queue_length;
	char *errstr;
	rte_bool r;
	char *filename = NULL;

	/* Options */

	while ((c = getopt_long(argc, argv,
				short_options, long_options, NULL)) != -1)
		switch (c) {
		case 0:
			break;

		case 'q':
			queue = strtol(optarg, NULL, 0);
			break;

		case 's':
			sleep_secs = strtol(optarg, NULL, 0);
			break;

		case 'x':
			context_key = strdup(optarg);
			break;

		case 'd':
			codec_key[num_codecs++] = strdup(optarg);
			break;

		case 'o':
			filename = strdup(optarg);
			break;

		default:
			exit(EXIT_FAILURE);
		}

	if (num_codecs == 0) /* use default */
		codec_key[num_codecs++] = "mpeg1_audio_layer2";

	/* Context */

	if (!(context = rte_context_new(context_key, NULL, &errstr))) {
		fprintf(stderr, "Cannot create context: %s\n", errstr);
		exit(EXIT_FAILURE);
	}

	for (i = 0; i < num_codecs; i++) {
		struct generator *g = calloc(1, sizeof(*g));
		int track = 0;

		assert(g != NULL);

		if ((dinfo = rte_codec_info_by_keyword(context, codec_key[i]))) {
			track = (dinfo->stream_type == RTE_STREAM_AUDIO) ?
				audio_tracks : video_tracks;
		}

		/* Codec */

		if (!(codec = rte_set_codec(context, codec_key[i], track, g))) {
			fprintf(stderr, "Cannot select codec '%s': %s\n",
				codec_key[i],
				rte_errstr(context));
			exit(EXIT_FAILURE);
		}

		/* No options, we take the defaults (or the keyword option string) */

		/* Sampling parameters */

		assert(dinfo != NULL);

		switch (dinfo->stream_type) {
		case RTE_STREAM_AUDIO:
			init_audio(g);
			break;
			
		case RTE_STREAM_VIDEO:
			init_video(g);
			break;
			
		default:
			fprintf(stderr, "Sorry, can only feed audio and video codecs.\n");
			exit(EXIT_FAILURE);
		}

		/* Input method */

		switch (io_mode) {
		case 1:
			r = rte_set_input_callback_master(codec, read_cm_cb, unref_cb, &queue_length);
			/* That's the number of buffers we'd normally allocate here. */
			if (r) fprintf(stderr, "Callback-master queue: %d buffers\n", queue_length);
			break;

		case 2:
			r = rte_set_input_callback_slave(codec, read_cs_cb);
			break;

		case 3:
			r = rte_set_input_push_master(codec, unref_cb, queue, &queue_length);
			if (r) fprintf(stderr, "Push-master queue: %d buffers requested, %d needed\n",
				     queue, queue_length);
			break;

		case 4:
			r = rte_set_input_push_slave(codec, queue, &queue_length);
			if (r) {
				fprintf(stderr, "Push-slave queue: %d buffers requested, %d needed\n",
					queue, queue_length);
				memset(&buffer, 0, sizeof(buffer));
				rte_push_buffer(codec, &buffer, FALSE);
			}
			break;

		default:
			fprintf(stderr, "I/O mode %d?\n", io_mode);
			exit(EXIT_FAILURE);
		}

		if (!r) {
			fprintf(stderr, "Unable to set input method: %s\n",
				rte_errstr(context));
			exit(EXIT_FAILURE);
		}
	}

	/* Output method */

	if (!rte_set_output_callback_slave(context, write_cb, seek_cb)) {
		fprintf(stderr, "Unable to set output method: %s\n", rte_errstr(context));
		exit(EXIT_FAILURE);
	}

	/* Start */

	if (filename)
#if defined(HAVE_LARGEFILE) && defined(O_LARGEFILE)
		fd = open64(filename,
			    O_CREAT | O_WRONLY | O_TRUNC | O_LARGEFILE,
			    S_IRUSR | S_IWUSR |
			    S_IRGRP | S_IWGRP |
			    S_IROTH | S_IWOTH);
#else
		fd = open(filename,
			  O_CREAT | O_WRONLY | O_TRUNC,
			  S_IRUSR | S_IWUSR |
			  S_IRGRP | S_IWGRP |
			  S_IROTH | S_IWOTH);
#endif
	else
		fd = STDOUT_FILENO;

	if (fd == -1) {
		fprintf(stderr, "Open failed: %d, %s\n", errno,  strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (!rte_start(context, 0.0, NULL, TRUE)) {
		fprintf(stderr, "Start failed: %s\n", rte_errstr(context));
		exit(EXIT_FAILURE);
	}

	/* Main loop */

	switch (io_mode) {
	case 1: /* callback-master */
	case 2: /* callback-slave */
		sleep(sleep_secs);
		break;

	case 3:
		if (num_codecs > 1) {
			fprintf(stderr, "Sorry, can feed only one codec.\n");
			exit(EXIT_FAILURE);
		}
		mainloop_pm();
		break;

	case 4:
		if (num_codecs > 1) {
			fprintf(stderr, "Sorry, can feed only one codec.\n");
			exit(EXIT_FAILURE);
		}
		mainloop_ps();
		break;

	default:
		fprintf(stderr, "I/O mode %d?\n", io_mode);
		exit(EXIT_FAILURE);
	}

	/* Stop */

	fprintf(stderr, "Stopping.\n");

	rte_stop(context, 0.0);

	rte_context_delete(context);

	fprintf(stderr, "Done.\n");

	exit(EXIT_SUCCESS);
}
