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

/* $Id: b_mp1e.c,v 1.29 2002-02-08 15:03:10 mschimek Exp $ */

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
#include "video/libvideo.h"
#include "audio/libaudio.h"
#include "systems/systems.h"
#include "common/profile.h"
#include "common/log.h"
#include "common/mmx.h"
#include "common/bstream.h"
#include "main.h"

/* Globals required by mp1e */
int verbose = 0;

static int		cpu_type;

static rte_option_info *vcd_mpeg1_options;
static rte_option_info *vcd_layer2_options;

static int		vcd_mpeg1_num_options;
static int		vcd_layer2_num_options;

extern rte_context_class mp1e_mpeg1_ps_context;
extern rte_context_class mp1e_mpeg1_vcd_context;
extern rte_context_class mp1e_mpeg1_video_context;
extern rte_context_class mp1e_mpeg1_audio_context;









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

void
packed_preview(unsigned char *buffer, int mb_cols, int mb_rows)

{
}

void
preview_init(int *argc, char ***argv)
{
}






static void
wait_empty_cp(fifo *f)
{
	mp1e_codec *meta = f->user_data;
	buffer *b = wait_full_buffer(&meta->cons);
	rte_buffer wb;

	if (b->used != 0) {
		wb.data = b->data;
		wb.size = b->used;

		if (!meta->write_cb(meta->codec.context, &meta->codec, &wb)) {
			/* XXX what now? */
		}
	} else { /* EOF */
		meta->write_cb(meta->codec.context, &meta->codec, NULL); 
	}

	send_empty_buffer(&meta->cons, b);
}

static rte_bool
set_output(rte_context *context,
	   rte_buffer_callback write_cb, rte_seek_callback seek_cb)
{
	mp1e_context *priv = PARENT(context, mp1e_context, context);	
	rte_context_class *xc = context->class;
	char buf[256];
	int queue, i;

	for (i = 0; i <= RTE_STREAM_MAX; i++) {
		rte_codec *codec;
		int count = 0;

		for (codec = priv->codecs; codec; codec = codec->next) {
			rte_codec_class *dc = codec->class;
			char *cname = dc->public.label ? _(dc->public.label) : dc->public.keyword;

			if (dc->public.stream_type != i)
				continue;

			if (codec->status != RTE_STATUS_READY) {
				rte_error_printf(context, "Codec %s, elementary stream #%d, not ready.",
						 cname, codec->stream_index);
				return FALSE;
			}

			count++;
		}

		if (count < xc->public.min_elementary[i]) {
			rte_error_printf(context, "Not enough elementary streams of rte stream type %d "
					 "for context %s. %d required, %d allocated.",
					 xc->public.keyword, xc->public.min_elementary[i], count);
			return FALSE;
		}
	}

	if (xc == &mp1e_mpeg1_video_context || xc == &mp1e_mpeg1_audio_context) {
		mp1e_codec *meta = PARENT(priv->codecs, mp1e_codec, codec);
		rte_codec_class *dc = meta->codec.class;

		snprintf(buf, sizeof(buf) - 1, "%s-output-cp", dc->public.keyword);
		queue = init_callback_fifo(&meta->out_fifo, buf,
					   wait_empty_cp, NULL, NULL, NULL,
					   1, meta->output_buffer_size);
		if (queue < 1) {
			rte_error_printf(context, _("Out of memory."));
			return FALSE;
		}

		meta->output = &meta->out_fifo;
		meta->out_fifo.user_data = meta;

		assert(add_consumer(&meta->out_fifo, &meta->cons));

		meta->write_cb = write_cb;
       	} else {
		/* FIXME */
	}

	context->status = RTE_STATUS_READY;

	return TRUE;
}

static void
wait_full_ca(fifo *f)
{
	mp1e_codec *meta = f->user_data;
	buffer *b = PARENT(rem_head(&f->empty), buffer, node);
	rte_buffer rb;

	rb.data = NULL;
	rb.size = 0;

	if (meta->read_cb(meta->codec.context, &meta->codec, &rb)) {
		assert(rb.data != NULL && rb.size > 0);

		b->data = rb.data;
		b->used = rb.size;
		b->time = rb.timestamp;
		b->user_data = rb.user_data;
	} else {
		b->used = 0; /* EOF */
	}

	send_full_buffer(&meta->prod, b);
}

static void
send_empty_ca(consumer *c, buffer *b)
{
	mp1e_codec *meta = c->fifo->user_data;

	if (meta->unref_cb) {
		rte_buffer rb;

		rb.data = b->data;
		rb.size = b->used;
		rb.user_data = b->user_data;

		meta->unref_cb(meta->codec.context, &meta->codec, &rb);
	}

	add_head(&c->fifo->empty, &b->node);
}

static void
wait_full_cp(fifo *f)
{
	mp1e_codec *meta = f->user_data;
	buffer *b = PARENT(rem_head(&f->empty), buffer, node);
	rte_buffer rb;

	rb.data = b->data;
	rb.size = b->size;

	if (meta->read_cb(meta->codec.context, &meta->codec, &rb)) {
		assert(rb.data == b->data && rb.size < b->size);

		b->used = rb.size;
		b->time = rb.timestamp;
		b->user_data = rb.user_data;
	} else {
		b->used = 0; /* EOF */
	}

	send_full_buffer(&meta->prod, b);
}

static rte_bool
push_buffer(rte_codec *codec, rte_buffer *wb, rte_bool blocking)
{
	mp1e_codec *meta = PARENT(codec, mp1e_codec, codec);
	buffer *b;

	pthread_mutex_lock(&meta->codec.mutex);

	if (meta->codec.status != RTE_STATUS_RUNNING) {
		return FALSE;
	}

	if (!wb || wb->data) {
		if (blocking)
			b = wait_empty_buffer(&meta->prod);
		else {
			if (!(b = recv_empty_buffer(&meta->prod)))
				return FALSE;
		}

		/* FIXME out-of-order */
		if (meta->unref_cb && b->used) {
			rte_buffer rb;

			rb.data = b->data;
			rb.size = b->used;
			rb.user_data = b->user_data;

			meta->unref_cb(meta->codec.context, &meta->codec, &rb);
		}
	}

	if (wb) {
		if (wb->data) {
			assert(wb->size > 0);

			/* XXX PP: check wb->data */

			b->data = wb->data;
			b->used = wb->size;
			b->time = wb->timestamp;
			b->user_data = wb->user_data;

			send_full_buffer(&meta->prod, b);
		}

		if (meta->input_mode == RTE_INPUT_PP) {
			wb->data = b->allocated;
			wb->size = b->size;
			wb->user_data = b->user_data;
		}
	} else {
		b->used = 0; /* EOF */

		send_full_buffer(&meta->prod, b);
	}

	pthread_mutex_unlock(&meta->codec.mutex);

	return TRUE;
}

static void
reset_input(rte_codec *codec)
{
	mp1e_codec *meta = PARENT(codec, mp1e_codec, codec);

	rem_producer(&meta->prod);
	destroy_fifo(meta->input);

	codec->status = RTE_STATUS_PARAM;
}

static rte_bool
set_input(rte_codec *codec, rte_input input_mode,
	  rte_buffer_callback read_cb, rte_buffer_callback unref_cb, int *queue_length)
{
	mp1e_codec *meta = PARENT(codec, mp1e_codec, codec);
	rte_codec_class *dc = codec->class;
	rte_context *context = codec->context;
	char buf[256];
	int queue;

	pthread_mutex_lock(&codec->mutex);

	switch (codec->status) {
	case RTE_STATUS_PARAM:
		break;
	case RTE_STATUS_READY:
		reset_input(codec);
		break;
	default:
		rte_error_printf(context, "Attempt to select input method with "
					  "uninitialized sample parameters.");
		goto failed;
	}

	queue = MAX(*queue_length, meta->input_stack_size);

	switch (input_mode) {
	case RTE_INPUT_CA:
		snprintf(buf, sizeof(buf) - 1, "%s-input-ca", dc->public.keyword);
		queue = init_callback_fifo(&meta->in_fifo, buf, NULL, NULL,
					   wait_full_ca, send_empty_ca,
					   meta->input_stack_size, 0);
		break;

	case RTE_INPUT_CP:
		snprintf(buf, sizeof(buf) - 1, "%s-input-cp", dc->public.keyword);
		queue = init_callback_fifo(&meta->in_fifo, buf, NULL, NULL,
					   wait_full_cp, NULL, queue,
					   meta->input_buffer_size);
		break;

	case RTE_INPUT_PA:
		snprintf(buf, sizeof(buf) - 1, "%s-input-pa", dc->public.keyword);
		queue = init_buffered_fifo(&meta->in_fifo, buf, queue, 0);
		break;

	case RTE_INPUT_PP:
		snprintf(buf, sizeof(buf) - 1, "%s-input-pp", dc->public.keyword);
		queue = init_buffered_fifo(&meta->in_fifo, buf,
					   queue, meta->input_buffer_size);
		unref_cb = NULL;
		break;

	default:
		goto failed;
	}

	if (queue < meta->input_stack_size) {
		destroy_fifo(&meta->in_fifo);
		goto failed;
	}

	meta->input = &meta->in_fifo;
	meta->in_fifo.user_data = meta;

	assert(add_producer(&meta->in_fifo, &meta->prod));

	*queue_length = queue;

	meta->input_mode = input_mode;
	meta->read_cb = read_cb;
	meta->unref_cb = unref_cb;

	pthread_mutex_unlock(&codec->mutex);
	return TRUE;

 failed:
	pthread_mutex_unlock(&codec->mutex);
	return FALSE;
}

static rte_bool
parameters_set(rte_codec *codec, rte_stream_parameters *rsp)
{
	rte_codec_class *dc = codec->class;

	pthread_mutex_lock(&codec->mutex);

	if (codec->status == RTE_STATUS_READY)
		reset_input(codec);

	pthread_mutex_unlock(&codec->mutex);

	if (codec->context->class == &mp1e_mpeg1_vcd_context) {
		if (dc == &mp1e_mpeg1_video_codec) {
			if (rsp->video.frame_rate < 25.0)
				rsp->video.frame_rate = 25.0;
			else if (rsp->video.frame_rate > 30.0)
				rsp->video.frame_rate = 30.0;

			if (rsp->video.width != 352) {
				rsp->video.width = 352;
				rsp->video.stride = 0;
				rsp->video.uv_stride = 0;
			}

			if (rsp->video.height != 240) {
				if (rsp->video.frame_rate > (25 + 30) / 2.0) {
					rsp->video.height = 240;
					rsp->video.offset = 0;
					rsp->video.u_offset = 0;
					rsp->video.v_offset = 0;
				} else if (rsp->video.height != 288) {
					rsp->video.height = (rsp->video.height
						< (240 + 288) / 2) ? 240 : 288;
					rsp->video.offset = 0;
					rsp->video.u_offset = 0;
					rsp->video.v_offset = 0;
				}
			}
		}
	}

	assert(dc->parameters_set != NULL);

	return dc->parameters_set(codec, rsp);
}

static rte_bool
option_set(rte_codec *codec, const char *keyword, va_list args)
{
	if (codec->status == RTE_STATUS_READY)
		reset_input(codec);

	return codec->class->option_set(codec, keyword, args);
}

static rte_option_info *
option_enum(rte_codec *codec, int index)
{
	rte_codec_class *dc = codec->class;

	if (codec->context->class == &mp1e_mpeg1_vcd_context) {
		if (dc == &mp1e_mpeg1_video_codec) {
			if (index < 0 || index >= vcd_mpeg1_num_options)
				return NULL;
			return vcd_mpeg1_options + index;
		} else if (dc == &mp1e_mpeg1_layer2_codec) {
			if (index < 0 || index >= vcd_layer2_num_options)
				return NULL;
			return vcd_layer2_options + index;
		}
	}

	return dc->option_enum ? dc->option_enum(codec, index) : NULL;
}

static rte_codec_class *
codec_table[] = {
	&mp1e_mpeg1_video_codec,
	&mp1e_mpeg1_layer2_codec,
	&mp1e_mpeg2_layer2_codec,
};

static const int num_codecs = sizeof(codec_table) / sizeof(codec_table[0]);

static rte_codec *
codec_get(rte_context *context, rte_stream_type stream_type, int stream_index)
{
	mp1e_context *priv = PARENT(context, mp1e_context, context);
	rte_codec *codec;

	for (codec = priv->codecs; codec; codec = codec->next)
		if (codec->class->public.stream_type == stream_type
		    && codec->stream_index == stream_index)
			return codec;

	return NULL;
}

static rte_codec *
codec_set(rte_context *context, const char *keyword,
	  rte_stream_type stream_type, int stream_index)
{
	mp1e_context *priv = PARENT(context, mp1e_context, context);
	rte_context_class *xc = context->class;
	rte_codec_class *dc;
	rte_codec *old, **oldpp, *codec = NULL;
	int i;

	for (oldpp = &priv->codecs; (old = *oldpp); oldpp = &old->next) {
		dc = old->class;

		if (keyword) {
			if (strcmp(dc->public.keyword, keyword) == 0)
				break;
		} else {
			if (dc->public.stream_type == stream_type
			    && old->stream_index == stream_index)
				break;
		}
	}

	if (keyword) {
		char *error = NULL;
		int max_elem;

		for (i = 0; i < num_codecs; i++)
			if (strcmp(codec_table[i]->public.keyword, keyword) == 0)
				break;

		if (i >= num_codecs) {
			rte_error_printf(context, "'%s' is no codec of the %s encoder.",
					 keyword, xc->public.keyword);
			return NULL;
		}

		dc = codec_table[i];

		stream_type = dc->public.stream_type;

		max_elem = xc->public.max_elementary[stream_type];

		if (max_elem == 0) {
			rte_error_printf(context, "'%s' is no valid codec for the %s encoder, "
					 "wrong stream type.", dc->public.keyword, xc->public.keyword);
			return NULL;
		} else if (stream_index >= max_elem) {
			rte_error_printf(context, "'%s' selected for elementary stream %d "
					 "of %s encoder, but only %d available.",
					 dc->public.keyword, stream_index,
					 xc->public.keyword, max_elem);
			return NULL;
		}

		if (!old && priv->num_codecs >= 32) {
			rte_error_printf(context, "Limit of %s codecs (32) reached.",
					 xc->public.keyword);
			return NULL;
		}

		if (!(codec = dc->new(dc, &error))) {
			if (error) {
				rte_error_printf(context, _("Cannot create new codec instance '%s': %s"),
						 dc->public.keyword, error);
				free(error);
			} else {
				rte_error_printf(context, _("Cannot create new codec instance '%s'."),
						 dc->public.keyword);
			}

			return NULL;
		}

		assert(error == NULL);

		codec->context = context;

		if (!rte_codec_options_reset(codec)) {
			dc->delete(codec);
			return NULL;
		}
	}

	if (old) {
		*oldpp = old->next;

		if (old->status == RTE_STATUS_READY)
			reset_input(old);

		old->class->delete(old);
		priv->num_codecs--;
	}

	if (codec) {
		codec->next = priv->codecs;
		priv->codecs = codec;
		priv->num_codecs++;
	}

	return codec;
}

static rte_codec_info *
codec_enum(rte_context *context, int index)
{
	rte_context_class *xc = context->class;
	rte_context_info *xi = &xc->public;
	int i;

	if (index < 0)
		return NULL;

	for (i = 0; i < num_codecs; i++) {
		if (xi->max_elementary[codec_table[i]->public.stream_type] <= 0)
			continue;
		if (xc == &mp1e_mpeg1_vcd_context
		    && codec_table[i] != &mp1e_mpeg1_video_codec
		    && codec_table[i] != &mp1e_mpeg1_layer2_codec)
			continue;
		if (index-- == 0)
			return &codec_table[i]->public;
	}

	return NULL;
}

static rte_context_class
mp1e_mpeg1_ps_context = {
	.public = {
		.keyword	= "mp1e_mpeg1_ps",
		.label		= N_("MPEG-1 Program Stream"),
		.mime_type	= "video/x-mpeg",
		.extension	= "mpg,mpe,mpeg",
		.min_elementary	= { 0, 0 },
		.max_elementary	= { 1, 1 }, /* to be { 16, 32, 1 }, */
	}
};

static rte_context_class
mp1e_mpeg1_vcd_context = {
	.public = {
		.keyword	= "mp1e_mpeg1_vcd",
		.label		= N_("MPEG-1 VCD Program Stream"),
		.mime_type	= "video/x-mpeg",
		.extension	= "mpg,mpe,mpeg",
		.min_elementary	= { 1, 1 },
		.max_elementary	= { 1, 1 },
	}
};

static rte_context_class
mp1e_mpeg1_video_context = {
	.public = {
		.keyword	= "mp1e_mpeg_video",
		.label		= N_("MPEG Video Elementary Stream"),
		.mime_type	= "video/mpeg",
		.extension	= "mpg,mpe,mpeg",
		.min_elementary	= { 1, 0 },
		.max_elementary	= { 1, 0 },
	}
};

static rte_context_class
mp1e_mpeg1_audio_context = {
	.public = {
		.keyword	= "mp1e_mpeg_audio",
		.label		= N_("MPEG Audio Elementary Stream"),
		.mime_type	= "audio/mpeg",
		.extension	= "mp2,mpga", /* note */
		.min_elementary	= { 0, 1 },
		.max_elementary	= { 0, 1 },
	}
};

static rte_context_class *
context_table[] = {
	&mp1e_mpeg1_ps_context,
	&mp1e_mpeg1_vcd_context,
	&mp1e_mpeg1_video_context,
	&mp1e_mpeg1_audio_context,
};

static const int num_contexts = sizeof(context_table) / sizeof(context_table[0]);

static void
context_delete(rte_context *context)
{
	mp1e_context *priv = PARENT(context, mp1e_context, context);

	pthread_mutex_lock(&context->mutex);

	switch (context->status) {
	case RTE_STATUS_RUNNING:
		fprintf(stderr, "mp1e bug warning: attempt to delete "
			"running %s context ignored\n", context->class->public.keyword);
		pthread_mutex_unlock(&context->mutex);
		return;
	default:
		break;
	}

#warning free codecs

	pthread_mutex_unlock(&context->mutex);
	pthread_mutex_destroy(&context->mutex);

	free(priv);
}

static rte_context *
context_new(rte_context_class *xc, char **errstr)
{
	mp1e_context *priv;
	rte_context *context;

	if (!(priv = calloc(1, sizeof(*priv)))) {
		rte_asprintf(errstr, _("Out of memory."));
		return NULL;
	}

	context = &priv->context;

	context->class = xc;

	pthread_mutex_init(&context->mutex, NULL);

	context->status = RTE_STATUS_NEW;

	return context;
}

static rte_option_info *
clone_option(rte_codec *codec, int index, rte_option_info **options, int *num)
{
	rte_option_info *old, *new;

	if (index >= *num) {
		assert(codec->class->option_enum != NULL);

		if (!(old = codec->class->option_enum(codec, index)))
			return NULL;

		if (!(new = realloc(*options, (index + 1) * sizeof(rte_option_info))))
			return NULL;

		*options = new;
		*num = index + 1;
		new += index;

		*new = *old;
	} else
		new = *options + index;

	return new;
}

#define KEYWORD(name) strcmp(oi->keyword, name) == 0

static void
backend_init(void)
{
	rte_codec codec;
	rte_option_info *oi;
	int i;

	cpu_type = cpu_detection();

	if (cpu_type == CPU_UNKNOWN) {
		return;
	}

	mp1e_mp2_module_init(0);
	mp1e_mpeg1_module_init(0);

	for (i = 0; i < num_contexts; i++) {
		context_table[i]->public.backend = "mp1e 1.9.2";

		context_table[i]->new = context_new;
		context_table[i]->delete = context_delete;

		context_table[i]->codec_enum = codec_enum;
		context_table[i]->codec_get = codec_get;
		context_table[i]->codec_set = codec_set;

		context_table[i]->codec_option_set = option_set;
		context_table[i]->parameters_set = parameters_set;

		context_table[i]->set_input = set_input;
		context_table[i]->push_buffer = push_buffer;

		context_table[i]->set_output = set_output;
// <<

		context_table[i]->start = context_start;
		context_table[i]->stop = context_stop;
		context_table[i]->pause = context_pause;
		context_table[i]->resume = context_resume;
	}

	mp1e_mpeg1_vcd_context.codec_option_enum = option_enum;

	codec.class = &mp1e_mpeg1_video_codec;

	for (i = 0; (oi = clone_option(&codec, i, &vcd_mpeg1_options, &vcd_mpeg1_num_options)); i++) {
		if (KEYWORD("bit_rate")) {
			oi->def.num = oi->min.num = oi->max.num = 1150000;
		}
	}

	codec.class = &mp1e_mpeg1_layer2_codec;

	for (i = 0; (oi = clone_option(&codec, i, &vcd_layer2_options, &vcd_layer2_num_options)); i++) {
		if (KEYWORD("bit_rate")) {
			oi->type = RTE_OPTION_INT;
			oi->def.num = oi->min.num = oi->max.num = 224000;
			oi->menu.num = NULL;
		} else if (KEYWORD("sampling_rate")) {
			oi->type = RTE_OPTION_INT;
			oi->def.num = oi->min.num = oi->max.num = 44100;
			oi->menu.num = NULL;
		} else if (KEYWORD("audio_mode")) {
			oi->def.num = oi->min.num = oi->max.num = 1; /* stereo */
		}
	}
}

static rte_context_class *
context_enum(int index, char **errstr)
{
	if (index < 0 || index >= num_contexts)
		return NULL;

	if (cpu_type == CPU_UNKNOWN) {
		rte_asprintf(errstr, _("MP1E backend requires MMX but the "
				       "CPU type is unknown, backend disabled."));

		/* Not NULL, want to tell who's disabled. */
	}

	return context_table[index];
}

const rte_backend_class
rte_backend_mp1e = {
	.name		= "mp1e",
	.backend_init	= backend_init,
	.context_enum	= context_enum,
};
