/*
 *  Real Time Encoder lib
 *  mp1e backend
 *
 *  Copyright (C) 2000-2001 Iñaki García Etxebarria
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

/* $Id: b_mp1e.c,v 1.1 2001-07-24 20:47:04 garetxe Exp $ */
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>

#include "options.h"
#include "common/mmx.h"
#include "common/math.h"
#include "video/video.h"
#include "audio/libaudio.h"
#include "audio/mpeg.h"
#include "video/mpeg.h"
#include "systems/systems.h"
#include "common/profile.h"
#include "common/log.h"
#include "common/mmx.h"
#include "common/bstream.h"
#include "common/remote.h"
#include "common/fifo.h"
#include "main.h"

#include "rtepriv.h"

typedef struct {
	rte_context_private	priv;

	pthread_t mux_thread; /* mp1e multiplexer thread */
	pthread_t video_thread_id; /* video encoder thread */
	pthread_t audio_thread_id; /* audio encoder thread */

	fifo		aud; /* mp1e audio fifo */
	short		*p; /* somewhere in abuffer */
	int		left; /* bytes left (unread) in abuffer */
	double		time; /* timestamp for aud */
	int		stereo; /* 1 if stereo, 0 otherwise */
	int		sampling_rate; /* same as context->audio_rate */
	/* audio sample "geometry" */
	int		samples_per_frame;
	int		scan_range;
	int		look_ahead;
} backend_private;

/* translates the context options to the global mp1e options */
static int rte_fake_options(rte_context * context);
/* prototypes for main initialization (mp1e startup) */
/* These routines are called in this order, they come from mp1e's main.c */
static void rte_audio_startup(void); /* Startup video parameters */
/* init routines */
static void rte_audio_init(void); /* init audio capture */
static void rte_video_init(void); /* init video capture */

/*
 * Global options from rte.
 */
char *			my_name="rte";
int			verbose=0;

fifo *			audio_cap_fifo;
int			stereo;

fifo *			video_cap_fifo;

/* fixme: This is just to satisfy dependencies for now */
void
packed_preview(unsigned char *buffer, int mb_cols, int mb_rows)
{
}

static int
init_backend			(void)
{
	cpu_type = cpu_detection();

	if (cpu_type == CPU_UNKNOWN)
	{
		rte_error(NULL, "mp1e backend requires MMX");
		return 0;
	}

	return 1;
}

static void
context_new			(rte_context	*context)
{
	context->format = strdup("mpeg1");
}

static void
context_destroy			(rte_context	*context)
{
	free(context->format);
}

#define BUFFER_SIZE 8192 // bytes per read(), appx.

/*
 *  Read window: samples_per_frame (1152 * channels) + look_ahead
 *  (480 * channels); from subband window size 512 samples, step
 *  width 32 samples (32 * 3 * 12 total)
 */
static buffer *
wait_full(fifo *f)
{
	rte_context *context = f->user_data;
	backend_private *priv = (backend_private*)context->private;
	buffer *b = f->buffers;

	if (b->data)
		return NULL; // no queue

	if (priv->left <= 0) {
		buffer *c;

		memcpy(b->allocated, (short *) b->allocated + priv->scan_range,
		       priv->look_ahead * sizeof(short));

		c = wait_full_buffer(&(context->private->aud));
		memcpy(b->allocated + priv->look_ahead *
		       sizeof(short), c->data, context->audio_bytes);
		priv->time = c->time;
		send_empty_buffer(&(context->private->aud), c);

		priv->p = (short *) b->allocated;
		priv->left = priv->scan_range - priv->samples_per_frame;

		b->time = priv->time;
		b->data = b->allocated;

		return b;
	}

	b->time = priv->time
		+ ((priv->p - (short *) b->allocated) >> priv->stereo)
			/ (double) priv->sampling_rate;

	priv->p += priv->samples_per_frame;
	priv->left -= priv->samples_per_frame;

	b->data = (unsigned char *) priv->p;

	return b;
}

static void
send_empty			(fifo *f, buffer *b)
{
	b->data = NULL;
}

static int
init_context			(rte_context	*context)
{
	backend_private *priv = (backend_private*)context->private;

	if (!context->format)
	{
		rte_error(context, "format == NULL !!!");
		return 0;
	}
	if (strcmp(context->format, "mpeg1"))
	{
		rte_error(context, "mp1e backend only supports \"mpeg1\""
			" format.");
		return 0;
	}

	if (context->mode & RTE_AUDIO)
	{
		switch (context->audio_mode)
		{
		case RTE_AUDIO_MODE_MONO:
			priv->stereo = 0;
			break;
		case RTE_AUDIO_MODE_STEREO:
			priv->stereo = 1;
			break;
			/* fixme:dual channel */
		default:
			priv->stereo = 0;
			break;
		}

		priv->sampling_rate = context->audio_rate;
		priv->samples_per_frame = SAMPLES_PER_FRAME << priv->stereo;
		priv->scan_range = MAX(BUFFER_SIZE / sizeof(short) /
				       priv->samples_per_frame, 1) *
			priv->samples_per_frame;
		priv->look_ahead = (512 - 32) << priv->stereo;
		
		context->audio_bytes = priv->scan_range*sizeof(short);
		
		if (!init_callback_fifo(&(priv->aud),
					"rte-mp1e-audio", wait_full,
					send_empty, 
					1, (priv->scan_range+
					    priv->look_ahead)*sizeof(short)))
		{
			rte_error(context, "not enough mem");
			return 0;
		}

		priv->aud.buffers[0].data = NULL;
		priv->aud.buffers[0].used =
			(priv->samples_per_frame + priv->look_ahead) *
			sizeof(short);
		priv->aud.user_data = context;
		priv->left = 0;
	}

	if (!rte_fake_options(context))
	{
		if (context->mode & RTE_AUDIO)
			uninit_fifo(&(priv->aud));
		return 0;
	}

	/* Init the mp1e engine, as main would do */
	rte_audio_startup();

	rte_audio_init();
	rte_video_init();

	if (!output_init())
	{
		if (context->mode & RTE_AUDIO)
			uninit_fifo(&(priv->aud));

		rte_error(context, "Cannot init output");
		return 0;
	}

	return 1;
}

static void
uninit_context			(rte_context	*context)
{
	backend_private *priv = (backend_private*)context->private;

	output_end();

	if (context->mode & RTE_AUDIO)
		uninit_fifo(&(priv->aud));

	if (gop_sequence)
		free(gop_sequence);
	gop_sequence = NULL;
}

static int
start			(rte_context	*context)
{
	backend_private *priv = (backend_private*)context->private;

	remote_init(modules);

	mux_thread_done = 0;

	if (modules & MOD_AUDIO) {
		ASSERT("create audio compression thread",
			!pthread_create(&priv->audio_thread_id,
					NULL,
					stereo ? mpeg_audio_layer_ii_stereo :
					mpeg_audio_layer_ii_mono,
					NULL));

		printv(2, "Audio compression thread launched\n");
	}

	if (modules & MOD_VIDEO) {
		ASSERT("create video compression thread",
			!pthread_create(&priv->video_thread_id,
					NULL,
					mpeg1_video_ipb,
					NULL));

		printv(2, "Video compression thread launched\n");
	}

	if ((modules == MOD_VIDEO || modules == MOD_AUDIO)
		&& mux_syn >= 2)
		mux_syn = 1; // compatibility

	switch (mux_syn) {
	case 0:
		ASSERT("create stream nirvana thread",
		       !pthread_create(&priv->mux_thread, NULL,
				       stream_sink, NULL));
		break;
	case 1:
		ASSERT("create elementary stream thread",
		       !pthread_create(&priv->mux_thread, NULL,
				       elementary_stream_bypass, NULL));
		break;
	case 2:
		printv(1, "MPEG-1 Program Stream\n");
		ASSERT("create mpeg1 system mux",
		       !pthread_create(&priv->mux_thread, NULL,
				       mpeg1_system_mux, NULL));
		break;
	case 3:
		printv(1, "MPEG-2 Program Stream\n");
		ASSERT("create mpeg2 system mux",
		       !pthread_create(&priv->mux_thread, NULL,
				       mpeg2_program_stream_mux, NULL));
		break;
	}

	remote_start(0.0);

	return 1;
}

static void
stop			(rte_context	*context)
{
	backend_private *priv = (backend_private*)context->private;

	/* Tell the mp1e threads to shut down */
	remote_stop(0.0); // now

	/* Join the mux thread */
	printv(2, "joining mux\n");
	pthread_join(priv->mux_thread, NULL);
	mux_thread_done = 1;
	printv(2, "mux joined\n");

	if (context->mode & RTE_AUDIO) {
		printv(2, "joining audio\n");
		pthread_join(priv->audio_thread_id, NULL);
		printv(2, "audio joined\n");
	}
	if (context->mode & RTE_VIDEO) {
		printv(2, "joining video\n");
		pthread_join(priv->video_thread_id, NULL);
		printv(2, "video joined\n");
	}

	mux_cleanup();

//	pr_report();
}

static char*
query_format		(rte_context	*context,
			 int n,
			 enum rte_mux_mode	*mux_mode)
{
	if (n)
		return 0;

	if (mux_mode)
		*mux_mode = RTE_AUDIO_AND_VIDEO;

	return "mpeg1";
}

static void
status			(rte_context	*context,
			 struct rte_status_info	*status)
{
	status->bytes_out = context->private->bytes_out;
	status->processed_frames = video_frame_count;
	status->dropped_frames = video_frames_dropped;
}

static int rte_fake_options(rte_context * context)
{
	backend_private *priv = (backend_private*)context->private;
	int pitch;

	ASSERT("guiroppaaaaa!\n", context != NULL);

	if (gop_sequence)
		free(gop_sequence);

	gop_sequence = strdup(context->gop_sequence);

	modules = context->mode;
	grab_width = saturate(context->width, 1, MAX_WIDTH);
	grab_height = saturate(context->height, 1, MAX_HEIGHT);
	if ((context->video_rate == RTE_RATE_NORATE) ||
	    (context->video_rate > RTE_RATE_8)) {
		rte_error(context, "Invalid frame rate: %d",
			  context->video_rate);
		context->video_rate = RTE_RATE_3; /* default to PAL */
	}
	frame_rate_code = context->video_rate;
	switch (context->video_format) {
	case RTE_YUYV_PROGRESSIVE:
	case RTE_YUYV_PROGRESSIVE_TEMPORAL:
		frame_rate_code += 3;
	case RTE_YUYV_VERTICAL_DECIMATION:
	case RTE_YUYV_TEMPORAL_INTERPOLATION:
	case RTE_YUYV_VERTICAL_INTERPOLATION:
	case RTE_YUYV_EXP:
	case RTE_YUYV_EXP_VERTICAL_DECIMATION:
	case RTE_YUYV_EXP2:
		pitch = grab_width*2;
		filter_mode =
			(context->video_format-RTE_YUYV_VERTICAL_DECIMATION) + 
			CM_YUYV_VERTICAL_DECIMATION;
		break;
	case RTE_YUYV:
		filter_mode = CM_YUYV;
		pitch = grab_width*2;
		break;
	case RTE_YUV420:
		filter_mode = CM_YUV;
		pitch = grab_width;
		break;
	case RTE_YVU420:
		filter_mode = CM_YVU;
		pitch = grab_width;
		break;
	default:
		rte_error(context, "unknown pixformat %d", context->video_format);
		return 0;
	}
	width = context->width;
	height = context->height;

	filter_init(pitch);
	if ((width != grab_width) || (height != grab_height))
		rte_error(NULL, "requested %dx%d, got %dx%d",
			  grab_width, grab_height, width, height);
	video_bit_rate = context->output_video_bits;
	audio_bit_rate = context->output_audio_bits;
	audio_bit_rate_stereo = audio_bit_rate * 2;

	sampling_rate = context->audio_rate;
	switch (context->audio_mode) {
	case RTE_AUDIO_MODE_MONO:
		audio_mode = AUDIO_MODE_MONO;
		break;
	case RTE_AUDIO_MODE_STEREO:
		audio_mode = AUDIO_MODE_STEREO;
		break;
	default:
		rte_error(context, "unknown audio mode %d",
			  context->audio_mode);
		return 0;
	}

	motion_min = context->motion_min;
	motion_max = context->motion_max;

	video_cap_fifo = &(context->private->vid);
	audio_cap_fifo = &(priv->aud);

	return 1;
}

/* Startup audio parameters */
static void rte_audio_startup(void)
{
	if (modules & MOD_AUDIO) {
		int psy_level = audio_mode / 10;
		
		audio_mode %= 10;
		
		stereo = (audio_mode != AUDIO_MODE_MONO);
		audio_parameters(&sampling_rate, &audio_bit_rate);
		
		if ((audio_bit_rate >> stereo) < 80000 || psy_level >= 1) {
			psycho_loops = MAX(psycho_loops, 1);
			
			if (sampling_rate < 32000 || psy_level >= 2)
				psycho_loops = 2;
			
			psycho_loops = MAX(psycho_loops, 2);
		}
	}
}

/* FIXME: Subtitles support */

static void rte_audio_init(void) /* init audio capture */
{
	if (modules & MOD_AUDIO) {
		long long n = llroundn(((double) video_num_frames /
					frame_rate_value[frame_rate_code])
				       / (1152.0 / sampling_rate));
		
		if (modules & MOD_VIDEO)
			audio_num_frames = MIN(n, (long long) INT_MAX);
		
		audio_init(sampling_rate, stereo, /* pcm_context */
			audio_mode, audio_bit_rate, psycho_loops);
	}
}

static void rte_video_init(void) /* init video capture */
{
	if (modules & MOD_VIDEO) {
		video_coding_size(width, height);

		if (frame_rate > frame_rate_value[frame_rate_code])
			frame_rate = frame_rate_value[frame_rate_code];

		video_init();
	}
}

const
rte_backend_info b_mp1e_info =
{
	"mp1e",
	sizeof(backend_private),
	init_backend,
	context_new,
	context_destroy,
	init_context,
	uninit_context,
	start,
	stop,
	query_format,
	status
};
