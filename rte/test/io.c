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

/* $Id: io.c,v 1.1 2002-02-25 06:22:20 mschimek Exp $ */

#undef NDEBUG

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <getopt.h>
#include <math.h>

#include "../src/rte.h"

#ifndef BYTE_ORDER /* BSD feat */
/* FIXME a GNU libc feature */
#define BYTE_ORDER __BYTE_ORDER
#define LITTLE_ENDIAN __LITTLE_ENDIAN
#define BIG_ENDIAN __BIG_ENDIAN
#endif

/* Globals */

static rte_context *		context;
static rte_codec *		codec;
static rte_codec_info *		dinfo;
static rte_stream_parameters	params;

static void			(* fake)(void *data, int size);
static double			byte_period;
static int			buffer_size;

static double			time = 1000.0;

/* Options */

static int			io_mode = 1;
static int			blocking;
static int			queue;
static int			sleep_secs = 3;
static char *			context_key = "mp1e_mpeg_audio";
static char *			codec_key = "mpeg1_audio_layer2";

/* Sine wave generator */

#define AUDIO_FREQ		440 /* Hz */

static int			sample_count = 0;
static double			w;

static void
fake_audio(void *data, int size)
{
	int16_t *sample = data;

//	fprintf(stderr, "read %p %d #%d\n", data, size, sample_count);

	for (size /= sizeof(int16_t); size--; sample_count++)
		*sample++ = sin(sample_count * w) * 25000;
}

static void
init_audio(void)
{
	memset(&params, 0, sizeof(params));

#if BYTE_ORDER == LITTLE_ENDIAN
	params.audio.sndfmt = RTE_SNDFMT_S16_LE;
#elif BYTE_ORDER == BIG_ENDIAN
	params.audio.sndfmt = RTE_SNDFMT_S16_BE;
#else
#error Unknown machine endianess
#endif
	params.audio.sampling_freq = 44100;
	params.audio.channels = 1;

	if (!rte_codec_parameters_set(codec, &params)) {
		fprintf(stderr, "Sampling parameter negotiation failed: %s\n", rte_errstr(context));
		exit(EXIT_FAILURE);
	}

	switch (params.audio.sndfmt) {
	case RTE_SNDFMT_S16_LE:
	case RTE_SNDFMT_S16_BE:
		if (params.audio.channels == 1)
			break;
	default:
		fprintf(stderr, "Sorry, cannot generate sample format required by codec.\n");
		exit(EXIT_FAILURE);
	}

	fake = fake_audio;

	w = AUDIO_FREQ / (double) params.audio.sampling_freq * M_PI * 2;
	byte_period = 1.0 / (params.audio.sampling_freq * sizeof(int16_t)); 
	buffer_size = params.audio.fragment_size; /* recommended minimum */
}

/* Image generator */

static uint32_t			yuyv_band[8 * 8 * 8 + 1];
static double			frame_var = 0;
static double			frame_vel = 0.05;

static void
fake_video(void *data, int size)
{
	uint8_t *canvas = data;
	int x, y, i;

//	fprintf(stderr, "read %p %d %f\n", data, size, time);

	frame_var += frame_vel;

	for (y = 0; y < params.video.height; y += 4) {
		for (x = 0; x < params.video.width; x += 4) {
#if BYTE_ORDER == LITTLE_ENDIAN
			uint32_t yuyv = ((int)((x ^ y) * frame_var) & 0xFF) * 0x00010001 + 0x80008000;
#elif BYTE_ORDER == BIG_ENDIAN
			uint32_t yuyv = ((int)((x ^ y) * frame_var) & 0xFF) * 0x01000100 + 0x00800080;
#else
#error Unknown machine endianess
#endif
			for (i = 0; i < 4; i++) {
				uint32_t *d = (uint32_t *)(canvas
					+ params.video.offset
					+ (y + i) * params.video.stride
					+ x * 2);

				d[0] = yuyv;
				d[1] = yuyv;
			}
		}
	}

	i = (params.video.height >> 1) & -16;

	for (y = i - 16; y < i + 17; y++) {
		uint32_t *d = (uint32_t *)(canvas
			+ params.video.offset + y * params.video.stride);

		memcpy(d, yuyv_band, params.video.width * 2);
	}
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
init_video(void)
{
	double r, g, b, i;
	double Y, Cb, Cr;
	int x;

	memset(&params, 0, sizeof(params));

	params.video.pixfmt = RTE_PIXFMT_YUYV;
	params.video.frame_rate = 24.0;
	params.video.pixel_aspect = 1.0;
	params.video.width = 352;
	params.video.height = 288;

	if (!rte_codec_parameters_set(codec, &params)) {
		fprintf(stderr, "Sampling parameter negotiation failed: %s\n", rte_errstr(context));
		exit(EXIT_FAILURE);
	}

	if (params.video.pixfmt != RTE_PIXFMT_YUYV) {
		fprintf(stderr, "Sorry, cannot generate sample format #%d "
			"required by codec, only YUYV.\n", params.video.pixfmt);
		exit(EXIT_FAILURE);
	}

	fake = fake_video;

	buffer_size = params.video.stride * params.video.height * 2;
	byte_period = 1.0 / (params.video.frame_rate * buffer_size);

	for (x = 0; x < params.video.width >> 1; x++) {
		i = x / (double)(params.video.width >> 1);
		r = 1.0 - max(fabs(i - 0.25) * 2);
		g = 1.0 - max(fabs(i - 0.5) * 2);
		b = 1.0 - max(fabs(i - 0.75) * 2);

		/* ITU-R Rec. 601 */

		Y  = (+ 0.2989 * r + 0.5866 * g + 0.1145 * b) * 255.0;
		Cb = (- 0.1687 * r - 0.3312 * g + 0.5000 * b) * 255.0;
		Cr = (+ 0.5000 * r - 0.4183 * g - 0.0816 * b) * 255.0;

		yuyv_band[x] =
#if BYTE_ORDER == LITTLE_ENDIAN
			+ (saturate(Y * 219.0 / 255.0 + 16) << 0)
			+ (saturate(Cb * 224.0 / 255.0 + 128) << 8)
			+ (saturate(Y * 219.0 / 255.0 + 16) << 16)
			+ (saturate(Cr * 224.0 / 255.0 + 128) << 24);
#elif BYTE_ORDER == BIG_ENDIAN
			+ (saturate(Y * 219.0 / 255.0 + 16) << 24)
			+ (saturate(Cb * 224.0 / 255.0 + 128) << 16)
			+ (saturate(Y * 219.0 / 255.0 + 16) << 8)
			+ (saturate(Cr * 224.0 / 255.0 + 128) << 0);
#else
#error Unknown machine endianess
#endif
	}
}

/* Input method #1 callback active */

static rte_bool
read_ca_cb(rte_context *context, rte_codec *codec, rte_buffer *buffer)
{
	/* A real recording app allocates in main(), this is just a test. */
	buffer->data = malloc(buffer_size);
	buffer->size = buffer_size;

	fake(buffer->data, buffer->size);

	buffer->timestamp = time;
	time += buffer->size * byte_period;

	return TRUE;
}

static rte_bool
unref_cb(rte_context *context, rte_codec *codec, rte_buffer *buffer)
{
//	fprintf(stderr, "unref %p\n", buffer->data);

	free(buffer->data);
}

/* Input method #2 callback passive */

static rte_bool
read_cp_cb(rte_context *context, rte_codec *codec, rte_buffer *buffer)
{
	fake(buffer->data, buffer->size);

	buffer->timestamp = time;
	time += buffer->size * byte_period;

	return TRUE;
}

/* Input method #3 push active */

rte_buffer buffer;

static void
mainloop_pa(void)
{
	int count;

	for (count = 0; count < 200; count++) {
		read_ca_cb(NULL, NULL, &buffer);

		/*
		 *  NB the codec may or may not be another thread, so this
		 *  function may or may not call write_cb().
		 */
		if (!rte_push_buffer(codec, &buffer, blocking)) {
			fprintf(stderr, "The codec is not fast enough, we drop frame #%d.\n", count);
			unref_cb(NULL, NULL, &buffer);
		}
	}

	fprintf(stderr, "Mainloop-pa finished.\n");
}

/* Input method #4 push passive */

static void
mainloop_pp(void)
{
	int count;

	for (count = 0; count < 200; count++) {
		read_cp_cb(NULL, NULL, &buffer);

		if (!rte_push_buffer(codec, &buffer, blocking)) {
			fprintf(stderr, "The codec is not fast enough, we drop frame #%d.\n", count);
		} else {
			/* buffer.data pointer changed */
		}
	}

	fprintf(stderr, "Mainloop-pp finished.\n");
}

/* Output method #2 callback passive */

static rte_bool
write_cb(rte_context *context, rte_codec *codec, rte_buffer *buffer)
{
	if (!buffer) /* EOF */
		return TRUE;

//	fprintf(stderr, "write %p %d\n", buffer->data, buffer->size);

	write(STDOUT_FILENO, buffer->data, buffer->size);
	return TRUE;
}

static rte_bool
seek_cb(rte_context *context, off64_t offset, int whence)
{
	assert(0);
}

static const struct option
long_options[] = {
	{ "ca",			no_argument,		&io_mode,		1 },
	{ "cp",			no_argument,		&io_mode,		2 },
	{ "pa",			no_argument,		&io_mode,		3 },
	{ "pp",			no_argument,		&io_mode,		4 },
	{ "block",		no_argument,		&blocking,		TRUE },
	{ "queue",		required_argument,	NULL,			'q' },
	{ "sleep",		required_argument,	NULL,			's' },
	{ "context",		required_argument,	NULL,			'x' },
	{ "codec",		required_argument,	NULL,			'd' },
};

int
main(int argc, char **argv)
{
	int index, c;
	int queue_length;
	char *errstr;
	rte_bool r;

	/* Options */

	while ((c = getopt_long(argc, argv, "", long_options, &index)) != -1)
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
			codec_key = strdup(optarg);
			break;

		default:
			exit(EXIT_FAILURE);
		}

	/* Context */

	if (!(context = rte_context_new(context_key, NULL, &errstr))) {
		fprintf(stderr, "Cannot create context: %s\n", errstr);
		exit(EXIT_FAILURE);
	}

	/* Codec */

	if (!(codec = rte_codec_set(context, codec_key, 0, NULL))) {
		fprintf(stderr, "Cannot select codec: %s\n", rte_errstr(context));
		exit(EXIT_FAILURE);
	}

	/* No options, we take the defaults (or the keyword option string) */

	/* Sampling parameters */

	dinfo = rte_codec_info_codec(codec);

	switch (dinfo->stream_type) {
	case RTE_STREAM_AUDIO:
		init_audio();
		break;

	case RTE_STREAM_VIDEO:
		init_video();
		break;

	default:
		fprintf(stderr, "Sorry, can only feed audio and video codecs.\n");
		exit(EXIT_FAILURE);
	}

	/* Input method */

	switch (io_mode) {
	case 1:
		r = rte_set_input_callback_active(codec, read_ca_cb, unref_cb, &queue_length);
		/* That's the number of buffers we'd normally allocate here. */
		r && fprintf(stderr, "Callback-active queue: %d buffers\n", queue_length);
		break;

	case 2:
		r = rte_set_input_callback_passive(codec, read_cp_cb);
		break;

	case 3:
		r = rte_set_input_push_active(codec, unref_cb, queue, &queue_length);
		r && fprintf(stderr, "Push-active queue: %d buffers requested, %d needed\n",
			     queue, queue_length);
		break;

	case 4:
		r = rte_set_input_push_passive(codec, queue, &queue_length);
		if (r) {
			fprintf(stderr, "Push-passive queue: %d buffers requested, %d needed\n",
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
		fprintf(stderr, "Unable to set input method: %s\n", rte_errstr(context));
		exit(EXIT_FAILURE);
	}

	/* Output method */

	if (!rte_set_output_callback_passive(context, write_cb, seek_cb)) {
		fprintf(stderr, "Unable to set output method: %s\n", rte_errstr(context));
		exit(EXIT_FAILURE);
	}

	/* Start */

	if (!rte_start(context, 0.0, NULL, TRUE)) {
		fprintf(stderr, "Start failed: %s\n", rte_errstr(context));
		exit(EXIT_FAILURE);
	}

	/* Main loop */

	switch (io_mode) {
	case 1: /* callback-active */
	case 2: /* callback-passive */
		sleep(sleep_secs);
		break;

	case 3:
		mainloop_pa();
		break;

	case 4:
		mainloop_pp();
		break;

	default:
		fprintf(stderr, "I/O mode %d?\n", io_mode);
		exit(EXIT_FAILURE);
	}

	/* Stop */

	rte_stop(context, 0.0);

	rte_context_delete(context);

	fprintf(stderr, "Done.\n");

	exit(EXIT_SUCCESS);
}
