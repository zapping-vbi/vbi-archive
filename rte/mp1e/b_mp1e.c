/*
 *  Real Time Encoder lib
 *  mp1e backend
 *
 *  Copyright (C) 2000-2001 Iñaki García Etxebarria
 *  Modified 2001 Michael H. Schimek
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

/* $Id: b_mp1e.c,v 1.27 2001-12-18 18:24:04 garetxe Exp $ */
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
	rte_context context; /* Parent */

	multiplexer *mux;

	pthread_t mux_thread; /* mp1e multiplexer thread */
	pthread_t video_thread_id; /* video encoder thread */
	pthread_t audio_thread_id; /* audio encoder thread */	

	unsigned int codec_set;
	rte_codec *video_codec;
	rte_codec *audio_codec;
} backend_private;

static void context_delete (rte_context *context)
{
	free(context);
}

static rte_codec_class
mp1e_vbi_codec = {
	.public = {
		.stream_type = RTE_STREAM_SLICED_VBI,
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
	&mp1e_mpeg1_video_codec,
	&mp1e_mpeg1_layer2_codec,
	&mp1e_mpeg2_layer2_codec,
	&mp1e_vbi_codec,
};

#define NUM_CODECS (sizeof(codec_table) / sizeof(codec_table[0]))

static rte_codec_info * codec_enum (rte_context *context, int index)
{
	int i, j;

	if (index < 0 || index >= NUM_CODECS)
		return NULL;

	/* Enum just the available codecs for the context */
	for (i=j=0; i<NUM_CODECS; i++)
		if (context->class->public.elementary
		    [codec_table[i]->public.stream_type]) {
			if (j == index)
				break;
			else
				j++;
		}

	if (i==NUM_CODECS)
		return NULL; /* nothing else found */

	return &codec_table[i]->public;
}

static rte_codec * codec_get (rte_context *context, rte_stream_type
			      stream_type, int index)
{
	backend_private *priv = (backend_private*)context;
	if (index)
		return NULL; /* only one stream supported for now */

	switch (stream_type) {
	case RTE_STREAM_VIDEO:
		return priv->video_codec;
	case RTE_STREAM_AUDIO:
		return priv->audio_codec;
	default:
		break;
	}       

	return NULL;
}

static rte_codec * codec_set (rte_context * context,
			      rte_stream_type stream_type,
			      int index,
			      const char *keyword)
{
	backend_private *priv = (backend_private*)context;
	rte_codec_class *cclass = NULL;
	rte_codec *codec = NULL;
	int i;

	if (index)
		return NULL; /* Unsupported */

	for (i=0; keyword && i<NUM_CODECS; i++)
		if (!strcmp(keyword, codec_table[i]->public.keyword)) {
			cclass = codec_table[i];
			if (stream_type != cclass->public.stream_type) {
				rte_error(context, "Bad stream type "
					  "for codec %s", keyword);
				return NULL;
			}
			break;
		}

	if (!cclass && keyword) {
		rte_error(context, "Codec %s not found", keyword);
		return NULL;
	}

	switch (stream_type) {
	case RTE_STREAM_VIDEO:
		if (priv->video_codec) {
			priv->video_codec->class->delete(priv->video_codec);
			priv->video_codec = NULL;
			priv->codec_set &= ~(1 << RTE_STREAM_VIDEO);
		}

		if (keyword) {
			codec = cclass->new();
			priv->video_codec = codec;
			codec->context = context;
			priv->codec_set |= (1 << RTE_STREAM_VIDEO);
		}
		break;
	case RTE_STREAM_AUDIO:
		if (priv->audio_codec) {
			priv->audio_codec->class->delete(priv->audio_codec);
			priv->audio_codec = NULL;
			priv->codec_set &= ~(1 << RTE_STREAM_AUDIO);
		}

		if (keyword) {
			codec = cclass->new();
			priv->audio_codec = codec;
			codec->context = context;
			priv->codec_set |= (1 << RTE_STREAM_AUDIO);
		}
		break;
	default:
		assert(!"reached");
		break;
	}

	return codec;
}

/* Prepare to start recording, codecs haven't been inited yet (done by
   rte) */
static rte_bool pre_init (rte_context *context)
{
	return TRUE;
}

/* Undo anything pre_init does */
static void uninit (rte_context *context)
{
}

/* Start recording */
static rte_bool context_start (rte_context *context)
{
	return FALSE;
}

static void context_stop (rte_context *context)
{
}

static void context_pause (rte_context *context)
{
}

static rte_bool context_resume (rte_context *context)
{
	return FALSE;
}

#define N_(x) x

static rte_context_class
mp1e_mpeg1_ps_context = {
	.public = {
		.keyword	= "mp1e-mpeg1-ps",
		.backend	= "mp1e 1.9.2",
		.label		= N_("MPEG-1 Program Stream"),

		.mime_type	= "video/x-mpeg",
		.extension	= "mpg,mpe,mpeg",

		.elementary	= { 1, 1 }, /* to be { 16, 32 }, */
	},
};

static rte_context_class
mp1e_mpeg1_vcd_context = {
	.public = {
		.keyword	= "mp1e-mpeg1-vcd",
		.backend	= "mp1e 1.9.2",
		.label		= N_("MPEG-1 VCD Program Stream"),

		.mime_type	= "video/x-mpeg",
		.extension	= "mpg,mpe,mpeg",

		.elementary	= { 1, 1 }, /* to be { 16, 32 }, */
	},
};

static rte_context_class
mp1e_mpeg1_video_context = {
	.public = {
		.keyword	= "mp1e-mpeg-video",
		.backend	= "mp1e 1.9.2",
		.label		= N_("MPEG Video Elementary Stream"),

		.mime_type	= "video/mpeg",
		.extension	= "mpg,mpe,mpeg",

		.elementary	= { 1, 0 },
	},
};

static rte_context_class
mp1e_mpeg1_audio_context = {
	.public = {
		.keyword	= "mp1e-mpeg-audio",
		.backend	= "mp1e 1.9.2",
		.label		= N_("MPEG Audio Elementary Stream"),

		.mime_type	= "audio/mpeg",
		.extension	= "mp2,mpga", /* note */

		.elementary	= { 0, 1 },
	},
};

static rte_context_class *
context_table[] = {
	&mp1e_mpeg1_ps_context,
	&mp1e_mpeg1_vcd_context,
	&mp1e_mpeg1_video_context,
	&mp1e_mpeg1_audio_context
};

#define NUM_CONTEXTS (sizeof(context_table) / sizeof(context_table[0]))

static rte_bool backend_available = FALSE;

static void
init_backend			(void)
{
	int i;

	cpu_type = cpu_detection();

	if (cpu_type == CPU_UNKNOWN)
	{
		rte_error(NULL, "mp1e backend requires MMX");
		return;
	}

	mp1e_mp2_module_init(0);

	/* Common context calls */
	for (i = 0; i<NUM_CONTEXTS; i++) {
		context_table[i]->delete = context_delete;
		context_table[i]->codec_enum = codec_enum;
		context_table[i]->codec_get = codec_get;
		context_table[i]->codec_set = codec_set;

		context_table[i]->pre_init = pre_init;
		context_table[i]->uninit = uninit;

		context_table[i]->start = context_start;
		context_table[i]->stop = context_stop;
		context_table[i]->pause = context_pause;
		context_table[i]->resume = context_resume;
	}

	backend_available = TRUE;
}

static rte_context_info *
context_enum(int index)
{
	if ((!backend_available) || index < 0 || index >= NUM_CONTEXTS)
		return NULL;

	return &context_table[index]->public;
}

static rte_context *context_new(const char *keyword)
{
	rte_context *context;
	int i;
	rte_context_class *klass = NULL;

	if (!backend_available)
		return NULL;

	for (i=0; i<NUM_CONTEXTS; i++)
		if (!strcmp(context_table[i]->public.keyword, keyword)) {
			klass = context_table[i];
			break;
		}

	if (!klass)
		return NULL;

	if (!(context = calloc(1, sizeof(backend_private)))) {
		return NULL;
	}

	context->class = klass;

	return context;
}

/* Globals required by mp1e */
int verbose = 0;

const
rte_backend_info b_mp1e_info =
{
	name:		"mp1e",
	init:		init_backend,
	context_enum:	context_enum,
	context_new:	context_new
};
