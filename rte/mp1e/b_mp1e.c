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

/* $Id: b_mp1e.c,v 1.12 2001-09-25 09:29:13 mschimek Exp $ */

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
#include "common/sync.h"
#include "common/fifo.h"
#include "main.h"

#include "rtepriv.h"

typedef struct {
	rte_context_private	priv;

	multiplexer *mux;

	pthread_t mux_thread; /* mp1e multiplexer thread */
	pthread_t video_thread_id; /* video encoder thread */
	pthread_t audio_thread_id; /* audio encoder thread */

	/* Experimental */

	unsigned int codec_set;
	rte_codec *audio_codec;

} backend_private;

/* translates the context options to the global mp1e options */
static int rte_fake_options(rte_context * context);
/* prototypes for main initialization (mp1e startup) */
/* These routines are called in this order, they come from mp1e's main.c */
static void rte_audio_startup(void); /* Startup video parameters */
/* init routines */
static void rte_audio_init(backend_private *priv); /* init audio capture */
static void rte_video_init(backend_private *priv); /* init video capture */

/*
 * Global options from rte.
 */
int			verbose=0;
int			stereo;

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
	backend_private *priv = (backend_private*)context->private;

	context->format = strdup("mpeg1");

	priv->codec_set = 0;
}

static void
context_destroy			(rte_context	*context)
{
	free(context->format);
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

	if (context->mode == 0)
		return 0;

	if (context->mode & RTE_AUDIO)
	{
		/* x8, let's see */
		context->audio_bytes = 8 * SAMPLES_PER_FRAME * sizeof(short);

		switch (context->audio_mode)
		{
		case RTE_AUDIO_MODE_STEREO:
			context->audio_bytes *= 2;
			break;

		default:
			break;
		}
	}

	if (!rte_fake_options(context))
		return 0;

	/* Init the mp1e engine, as main would do */
	if (!(priv->mux = mux_alloc(context)))
	{
		rte_error(context, "Cannot init output");
		return 0;
	}

	rte_audio_startup();

	rte_audio_init(priv);
	rte_video_init(priv);

	if (!output_init())
	{
		rte_error(context, "Cannot init output");
		mux_free(priv->mux);
		return 0;
	}

	return 1;
}

static void
uninit_context			(rte_context	*context)
{
	backend_private *priv = (backend_private*)context->private;

	output_end();

	mux_free(priv->mux);
}

static int
start			(rte_context	*context)
{
	backend_private *priv = (backend_private*)context->private;

	if (modules & MOD_VIDEO)
		/* Use video as time base (broadcast and v4l2 assumed) */
		mp1e_sync_init(modules, MOD_VIDEO);
	else if (modules & MOD_SUBTITLES)
		mp1e_sync_init(modules, MOD_SUBTITLES);
	else
		mp1e_sync_init(modules, MOD_AUDIO);
	/* else mp1e_sync_init(modules, 0); use TOD */

	if (modules & MOD_AUDIO) {
		ASSERT("create audio compression thread",
			!pthread_create(&priv->audio_thread_id,
					NULL, mp1e_mp2_thread, priv->audio_codec));

		printv(2, "Audio compression thread launched\n");
	}

	if (modules & MOD_VIDEO) {
		ASSERT("create video compression thread",
			!pthread_create(&priv->video_thread_id,
					NULL,
					mpeg1_video_ipb,
					(fifo *) &(context->private->vid)));

		printv(2, "Video compression thread launched\n");
	}

	switch (modules) {
	case MOD_VIDEO:
	case MOD_AUDIO:
		ASSERT("create elementary stream thread",
		       !pthread_create(&priv->mux_thread, NULL,
				       elementary_stream_bypass, priv->mux));
		break;

	default: /* both */
		printv(1, "MPEG-1 Program Stream\n");
		ASSERT("create mpeg1 system mux",
		       !pthread_create(&priv->mux_thread, NULL,
				       mpeg1_system_mux, priv->mux));
		break;
	}

	mp1e_sync_start(0.0);

	return 1;
}

static void
stop			(rte_context	*context)
{
	backend_private *priv = (backend_private*)context->private;

	/* Tell the mp1e threads to shut down */
	mp1e_sync_stop(0.0); // now

	if (context->mode & RTE_VIDEO) {
		printv(2, "joining video\n");
		pthread_join(priv->video_thread_id, NULL);
		printv(2, "video joined\n");
	}

	if (context->mode & RTE_AUDIO) {
		printv(2, "joining audio\n");
		pthread_join(priv->audio_thread_id, NULL);
		printv(2, "audio joined\n");
	}

	/* Join the mux thread */
	printv(2, "joining mux\n");
	pthread_join(priv->mux_thread, NULL);
	printv(2, "mux joined\n");

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
	if (context->mode == RTE_AUDIO) {
		status->processed_frames = audio_frame_count;
		status->dropped_frames = audio_frames_dropped;
	} else {
		status->processed_frames = video_frame_count;
		status->dropped_frames = video_frames_dropped;
	}
}

static int rte_fake_options(rte_context * context)
{
//	backend_private *priv = (backend_private*)context->private;
	int pitch;

	ASSERT("guiroppaaaaa!\n", context != NULL);

	gop_sequence = context->gop_sequence;

	modules = context->mode;

	if (context->width < 1 || context->width > MAX_WIDTH
	    || context->height < 1 || context->height > MAX_HEIGHT) {
		rte_error(context, "Image size %dx%d out of bounds",
			  context->width, context->height);
		return 0;
	}

	grab_width = context->width;
	grab_height = context->height;

	if ((context->video_rate == RTE_RATE_NORATE) ||
	    (context->video_rate > RTE_RATE_8)) {
		rte_error(context, "Invalid frame rate: %d",
			  context->video_rate);
		context->video_rate = RTE_RATE_3; /* default to PAL */
	}

	vseg.frame_rate_code = context->video_rate;

	switch (context->video_format) {
	case RTE_YUYV_PROGRESSIVE:
	case RTE_YUYV_PROGRESSIVE_TEMPORAL:
		vseg.frame_rate_code += 3;
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
		rte_error(context, "Video format %d temporarily out of order",
			  context->video_rate);
		return 0;
		break;
	case RTE_YUYV:
		filter_mode = CM_YUYV;
		pitch = grab_width * 2;
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

static void rte_audio_init(backend_private *priv) /* init audio capture */
{
	if (modules & MOD_AUDIO) {
		long long n = llroundn(((double) video_num_frames /
					frame_rate_value[vseg.frame_rate_code])
				       / (1152.0 / sampling_rate));
		
		if (modules & MOD_VIDEO)
			audio_num_frames = MIN(n, (long long) INT_MAX);

		/* preliminary */

		if (!priv->audio_codec) {

		if (sampling_rate < 32000)
			priv->audio_codec = mp1e_mpeg2_layer2_codec.new();
		else
			priv->audio_codec = mp1e_mpeg1_layer2_codec.new();

		assert(priv->audio_codec);

		rte_set_option(priv->audio_codec,
			       "sampling_rate", sampling_rate);
		rte_set_option(priv->audio_codec,
			       "bit_rate", audio_bit_rate);
		rte_set_option(priv->audio_codec,
			       "audio_mode", (int) "\1\3\2\0"[audio_mode]);
		rte_set_option(priv->audio_codec,
			       "psycho", (int) psycho_loops);
		}

		mp1e_mp2_init(priv->audio_codec, &priv->priv.aud, priv->mux);
	}
}

static void rte_video_init(backend_private *priv) /* init video capture */
{
	if (modules & MOD_VIDEO) {
		video_coding_size(width, height);

		if (frame_rate > frame_rate_value[vseg.frame_rate_code])
			frame_rate = frame_rate_value[vseg.frame_rate_code];

		video_init(priv->mux);
	}
}

/* Experimental */

static rte_codec_class
mp1e_video_codec = {
	.public = {
		.stream_type = RTE_STREAM_VIDEO,
		.stream_formats = RTE_PIXFMTS_YUV420 |
		                  RTE_PIXFMTS_YUYV,
		.keyword = "mpeg1_video",
		.label = "MPEG-1 Video",
	}
};

static rte_codec_class
mp1e_vbi_codec = {
	.public = {
		.stream_type = RTE_STREAM_SLICED_VBI,
		.stream_formats = RTE_VBIFMTS_TELETEXT_B_L10_625 |
		                  RTE_VBIFMTS_TELETEXT_B_L25_625,
		.keyword = "dvb_vbi",
		.label = "DVB VBI Stream (Subtitles)",
		.tooltip = "Note the recording of Teletext and Closed Caption "
			"services in *MPEG-1* Program Streams is not covered "
			"by the DVB standard. Unaware players should ignore "
			"VBI data.",
	},
};

static rte_codec_class *
codec_table[] = {
	&mp1e_video_codec,
	&mp1e_mpeg1_layer2_codec,
	&mp1e_mpeg2_layer2_codec,
	&mp1e_vbi_codec,
};

#define NUM_CODECS (sizeof(codec_table) / sizeof(codec_table[0]))

static rte_codec_info *
enum_codec(rte_context *context, int index)
{
	if (index < 0 || index >= NUM_CODECS)
		return NULL;

	return &codec_table[index]->public;
}

static rte_codec *
get_codec(rte_context *context, rte_stream_type stream_type,
	  int stream_index, char **codec_keyword_p)
{
	backend_private *priv = (backend_private *) context->private;
	rte_codec_class *info;
	rte_codec *codec;

	if (stream_index != 0)
		goto bad;

	switch (stream_type) {
	case RTE_STREAM_VIDEO:
		codec = (rte_codec *) 1;
		if (priv->codec_set & 0x01)
			info = &mp1e_video_codec;
		else
			goto bad;
		break;

	case RTE_STREAM_AUDIO:
		/* find in stream table */
		if ((codec = priv->audio_codec))
			info = priv->audio_codec->class;
		else
			goto bad;
		break;

	case RTE_STREAM_SLICED_VBI:
		codec = (rte_codec *) 4;
		if (priv->codec_set & 0x08)
			info = &mp1e_vbi_codec;
		else
			goto bad;
		break;

	default:
	bad:
		if (codec_keyword_p)
			*codec_keyword_p = NULL;
		return NULL;
	}

	if (codec_keyword_p)
		*codec_keyword_p = info->public.keyword;

	return codec;
}

static rte_codec *
set_codec(rte_context *context, rte_stream_type stream_type,
	  int stream_index, char *codec_keyword)
{
	backend_private *priv = (backend_private *) context->private;
	rte_codec *codec = NULL;
	int i;

	if (stream_index != 0)
		return NULL; /* TODO */

	for (i = 0; i < NUM_CODECS; i++)
		if (codec_table[i]->public.stream_type == stream_type
		    && (!codec_keyword
			|| 0 == strcmp(codec_table[i]->public.keyword,
				       codec_keyword)))
			break;

	if (i >= NUM_CODECS)
		return NULL;

	switch (stream_type) {
	case RTE_STREAM_VIDEO:
		/* preliminary */
		if (codec_keyword) {
			codec = (rte_codec *) 1;
			priv->codec_set |= 1 << 0;
		} else {
			priv->codec_set &= ~(1 << 0);
		}
		break;

	case RTE_STREAM_AUDIO:
		if (priv->audio_codec) {
			priv->audio_codec->class->delete(priv->audio_codec);
			priv->audio_codec = NULL;
			/* preliminary */ priv->codec_set &= ~(1 << 1);
		}

		if (codec_keyword) {
			codec = codec_table[i]->new();

			if ((priv->audio_codec = codec)) {
				codec->context = context;
				priv->codec_set |= 1 << 0;
			}
		}

		break;

	case RTE_STREAM_SLICED_VBI:
	default:
		return NULL; /* todo */
	}

	switch (priv->codec_set) {
	case 0:
		context->mode = 0;
		break;
	case 1:
		context->mode = RTE_VIDEO;
		break;
	case 2:
	case 4:
		context->mode = RTE_AUDIO;
		break;
	case 3:
	case 5:
		context->mode = RTE_AUDIO_AND_VIDEO;
		break;
	default:
		assert(!"reached");
	}

	return codec;
}

static rte_option *
enum_option(rte_codec *codec, int index)
{
//	rte_context *context = codec->context;

	/* Preliminary */

	switch ((int) codec) {
	case 1: /* MPEG-1 Video */
		return NULL; /* TODO */

	case 4: /* VBI */
		return NULL; /* TODO */

	default:
		return codec->class->enum_option(codec, index);
	}
}

#include <stdarg.h>

static int
get_option(rte_codec *codec, char *keyword, rte_option_value *v)
{
//	rte_context *context = codec->context;

	/* Preliminary */

	switch ((int) codec) {
	case 1: /* MPEG-1 Video */
		return 0; /* TODO */

	case 4: /* VBI */
		return 0; /* TODO */

	default:
		return codec->class->get_option(codec, keyword, v);
	}
}

static int
set_option(rte_codec *codec, char *keyword, va_list args)
{
//	rte_context *context = codec->context;

	/* Preliminary */

	switch ((int) codec) {
	case 1: /* MPEG-1 Video */
		return 0; /* TODO */

	case 4: /* VBI */
		return 0; /* TODO */

	default:
		return codec->class->set_option(codec, keyword, args);
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
	status,

	.enum_codec	= enum_codec,
	.get_codec	= get_codec,
	.set_codec	= set_codec,

	.enum_option	= enum_option,
	.get_option	= get_option,
	.set_option	= set_option,
};
