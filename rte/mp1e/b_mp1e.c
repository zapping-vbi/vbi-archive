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

/* $Id: b_mp1e.c,v 1.7 2001-09-07 05:09:35 mschimek Exp $ */
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
	if (!(priv->mux = mux_alloc()))
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

	sync_init(modules);

	if (modules & MOD_AUDIO) {
		ASSERT("create audio compression thread",
			!pthread_create(&priv->audio_thread_id,
					NULL,
					stereo ? mpeg_audio_layer_ii_stereo :
					         mpeg_audio_layer_ii_mono,
					(fifo *) &(context->private->aud)));

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

	sync_start(0.0);

	return 1;
}

static void
stop			(rte_context	*context)
{
	backend_private *priv = (backend_private*)context->private;

	/* Tell the mp1e threads to shut down */
	sync_stop(0.0); // now

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
					frame_rate_value[frame_rate_code])
				       / (1152.0 / sampling_rate));
		
		if (modules & MOD_VIDEO)
			audio_num_frames = MIN(n, (long long) INT_MAX);
		
		audio_init(sampling_rate, stereo, /* pcm_context */
			audio_mode, audio_bit_rate, psycho_loops, priv->mux);
	}
}

static void rte_video_init(backend_private *priv) /* init video capture */
{
	if (modules & MOD_VIDEO) {
		video_coding_size(width, height);

		if (frame_rate > frame_rate_value[frame_rate_code])
			frame_rate = frame_rate_value[frame_rate_code];

		video_init(priv->mux);
	}
}

/* Experimental */

static rte_codec_info
codec_table[] = {
	{
		.stream_type = RTE_STREAM_VIDEO,
		.stream_formats = RTE_PIXFMTS_YUV420 |
				  RTE_PIXFMTS_YUYV,
		.keyword = "mpeg1-video",
		.label = "MPEG-1 Video",
	}, {
		.stream_type = RTE_STREAM_AUDIO,
		.stream_formats = RTE_SNDFMTS_S16LE,
		.keyword = "mpeg1-audio-layer2",
		.label = "MPEG-1 Audio Layer II",
	}, {
		.stream_type = RTE_STREAM_AUDIO,
		.stream_formats = RTE_SNDFMTS_S16LE,
		.keyword = "mpeg2-audio-layer2",
		.label = "MPEG-2 Audio Layer II LFE",
		.tooltip = "MPEG-2 Low (Sampling) Frequency Extension to MPEG-1 "
			   "Audio Layer II. Caution: Not all MPEG video and "
			   "audio players support MPEG-2 audio."
	}, {
		.stream_type = RTE_STREAM_SLICED_VBI,
		.stream_formats = RTE_VBIFMTS_TELETEXT_B_L10_625 |
		                  RTE_VBIFMTS_TELETEXT_B_L25_625,
		.keyword = "dvb-vbi",
		.label = "DVB VBI Stream (Subtitles)",
		.tooltip = "Note the recording of Teletext and Closed Caption "
		           "services in *MPEG-1* Program Streams is not covered "
		           "by the DVB standard. Unaware players (all at this "
			   "time I have to admit) will ignore VBI data.",
	},
};

#define NUM_CODECS (sizeof(codec_table) / sizeof(codec_table[0]))

static rte_codec_info *
enum_codec(rte_context *context, int index)
{
	if (index < 0 || index >= NUM_CODECS)
		return NULL;

	return codec_table + index;
}

static rte_codec *
set_codec(rte_context *context, rte_stream_type stream_type,
	  int stream_index, char *codec_keyword)
{
	backend_private *priv = (backend_private *) context->private;
	int i;

	if (stream_index != 0)
		return NULL; /* TODO */

	for (i = 0; i < NUM_CODECS; i++)
		if (codec_table[i].stream_type == stream_type
		    && (!codec_keyword
			|| 0 == strcmp(codec_table[i].keyword,
				       codec_keyword)))
			break;

	if (i >= NUM_CODECS)
		return NULL;

	if (i == 3)
		return NULL; /* VBI TODO */

	if (codec_keyword) {
		if (i > 0) /* toggle MPEG-1/2 audio */
			priv->codec_set &= ~(1 << (3 - i));
		priv->codec_set |= 1 << i;
	} else
		priv->codec_set &= ~(1 << i);

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

	return (rte_codec *) i; /* preliminary */
}

/*
   Options are not supposed to be handled at this level,
   rather a private (of the backend) extension of rte_codec_info
   should point to the apropriate stuff. The backend may even
   gather rte_codec_infos from the respective libsoandso,
   and relate rte_codec (another backend private thing)
   to codec private runtime data.
 */

#define elements(array) (sizeof(array) / sizeof((array)[0]))

/* Attention: Canonical order */
static char *
menu_audio_mode[] = {
	/* 0 */ "Mono",
	/* 1 */ "Stereo",
	/* 2 */ "Bilingual",
	/* 3 TODO "Joint Stereo", */
};

/* Attention: Canonical order */
static char *
menu_audio_psycho[] = {
	/* 0 */ "Static",
	/* 1 */ "Fast",
	/* 2 */ "Accurate",
};

/* Random */
static char *menu_audio_bitrate[2][15];
static char *menu_audio_sampling[2][3];

static rte_option
audio1_options[] = {
		/*
		 *  type, unique keyword (for command line etc), label,
		 *  default (union), minimum, maximum, menu, tooltip
		 */
	{
		RTE_OPTION_MENU,	"bit-rate",	"Bit rate",
		{ .num = 4 }, 0, 14, menu_audio_bitrate[0],
		"Output bit rate, all channels together"
	}, {
		RTE_OPTION_MENU,	"sampling-rate", "Sampling frequency",
		{ .num = 0 }, 0, 2, menu_audio_sampling[0], NULL
	}, {
		RTE_OPTION_MENU,	"mode",		"Mode",
		{ .num = 0 }, 0, elements(menu_audio_mode) - 1,
		menu_audio_mode, NULL
	}, {
		RTE_OPTION_MENU,	"psycho",	"Psychoacoustic analysis",
		{ .num = 0 }, 0, elements(menu_audio_psycho) - 1,
		menu_audio_psycho,
		"Speed/quality trade-off. Selecting 'Accurate' is recommended "
		"below 80 kbit/s per channel, if you have golden ears or a "
		"little more CPU load doesn't matter."
	},
};

static rte_option audio2_options[elements(audio1_options)];

static void init_audio_menu(void) __attribute__ ((constructor));

static void
init_audio_menu(void)
{
	char buf[256];
	int i, j;

	for (i = 0; i < 2; i++) {
		int mpeg = i ? MPEG_VERSION_2 : MPEG_VERSION_1;

		for (j = 0; j < 15; j++) {
			snprintf(buf, sizeof(buf), "%d kbit/s",
				 bit_rate_value[mpeg][1 + j] / 1000);
			assert((menu_audio_bitrate[mpeg][j] = strdup(buf)));
		}

		for (j = 0; j < 3; j++) {
			snprintf(buf, sizeof(buf), "%f kHz",
				 sampling_freq_value[mpeg][2 - j] / 1000.0);
			assert((menu_audio_sampling[mpeg][j] = strdup(buf)));
		}
	}

	memcpy(audio2_options, audio1_options, sizeof(audio2_options));

	audio2_options[0].menu = menu_audio_bitrate[1];
	audio2_options[1].menu = menu_audio_sampling[1];
}

static rte_option *
enum_option(rte_context *context, rte_codec *codec, int index)
{
	/* Preliminary */

	switch ((int) codec) {
	case 0: /* MPEG-1 Video */
		return NULL; /* TODO */

	case 1: /* MPEG-1 Audio */
		if (index < 0 || index >= elements(audio1_options))
			return NULL;
		return audio1_options + index;

	case 2: /* MPEG-2 Audio */
		if (index < 0 || index >= elements(audio2_options))
			return NULL;
		return audio2_options + index;

	case 3: /* VBI */
		return NULL; /* TODO */

	default:
		return NULL;
	}
}

#include <stdarg.h>

static int
set_option(rte_context *context, rte_codec *codec,
	   char *keyword, va_list args)
{
	rte_option *option;
	int mpeg, val, i;


	/* Preliminary */

	switch ((int) codec) {
	case 0: /* MPEG-1 Video */
		return 0; /* TODO */

	case 1: /* MPEG-1 Audio */
		option = audio1_options;
		mpeg = MPEG_VERSION_1;
		break;

	case 2: /* MPEG-2 Audio */
		option = audio2_options;
		mpeg = MPEG_VERSION_2;
		break;

	case 3: /* VBI */
		return 0; /* TODO */

	default:
		return 0;
	}

	for (i = 0; i < elements(audio1_options); i++)
		if (strcmp(option[i].keyword, keyword) == 0)
			break;

	if (i >= elements(audio1_options))
		return 0;

	switch (i) {
	case 0: /* bit-rate */
		val = va_arg(args, int);

		if (val < 0)
			return 0;
		else if (val <= 14)
			audio_bit_rate = bit_rate_value[mpeg][1 + val];
		else
			audio_bit_rate = val; /* -> audio_parameters() */
		break;

	case 1: /* sampling-rate */
		val = va_arg(args, int);

		if (val < 0)
			return 0;
		else if (val <= 2)
			sampling_rate = sampling_freq_value[mpeg][2 - val];
		else
			sampling_rate = val; /* -> audio_parameters() */
		break;

	case 2: /* mode */
		val = va_arg(args, int);
		if (val < 0 || val > 2)
			return 0;
		/* mo, st, bi (user) -> st, jst, bi, mo (mpeg) */
		audio_mode = "\3\0\2\1"[val];
		break;

	case 3: /* psycho */
		val = va_arg(args, int);
		if (val < 0 || val > 2)
			return 0;
		psycho_loops = val;
		break;

	default:
		assert(!"reached");
	}

	return 1;
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
	.set_codec	= set_codec,

	.enum_option	= enum_option,
	.set_option	= set_option,
};
