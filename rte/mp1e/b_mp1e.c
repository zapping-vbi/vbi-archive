/*
 *  Real Time Encoder lib
 *  mp1e backend
 *
 *  Copyright (C) 2000, 2001 Iñaki García Etxebarria
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
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

/* $Id: b_mp1e.c,v 1.42 2002-09-26 20:39:22 mschimek Exp $ */

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
#include "systems/libsystems.h"
#include "systems/mpeg.h"
#include "common/profile.h"
#include "common/log.h"
#include "common/mmx.h"
#include "common/bstream.h"

int				cpu_type;

static rte_option_info *	vcd_mpeg1_options;
static rte_option_info *	vcd_layer2_options;

static int			vcd_mpeg1_num_options;
static int			vcd_layer2_num_options;

extern rte_context_class	mp1e_mpeg1_ps_context;
extern rte_context_class	mp1e_mpeg1_vcd_context;
extern rte_context_class	mp1e_mpeg1_video_context;
extern rte_context_class	mp1e_mpeg1_audio_context;

/* FIXME */
static const int		video_buffers = 2*8;	/* video compression -> mux */
static const int		audio_buffers = 2*32;	/* audio compression -> mux */

/* Legacy, will be removed */

int				verbose = 0;
int				test_mode = 0;
int				filter_mode;
int				preview;
int				width, height;
int				motion_min, motion_max;
double				frame_rate;
int				luma_only;
int				outFileFD;
int				mux_syn;
int				split_sequence = 0;
long long			part_length = 0;

void packed_preview(unsigned char *buffer, int mb_cols, int mb_rows) { }
void preview_init(int *argc, char ***argv) { }
void break_sequence(void) { }

static void
status(rte_context *context, rte_codec *codec,
       rte_status *status, unsigned int size)
{
	mp1e_context *mx = MX(context);
	rte_context_class *xc = mx->context._class;

	if (   xc == &mp1e_mpeg1_video_context
	    || xc == &mp1e_mpeg1_audio_context) {
		codec = mx->codecs;
	}

	if (codec) {
		pthread_mutex_lock(&codec->mutex);
		memcpy(status, &MD(codec)->status, size);
		pthread_mutex_unlock(&codec->mutex);
	} else {
		pthread_mutex_lock(&mx->context.mutex);

		mx->mux.status.frames_dropped = 0;

		for (codec = mx->codecs; codec; codec = codec->next) {
			pthread_mutex_lock(&codec->mutex);
			mx->mux.status.frames_dropped += MD(codec)->status.frames_dropped; 
			pthread_mutex_unlock(&codec->mutex);
		}

		memcpy(status, &mx->mux.status, size);

		pthread_mutex_unlock(&mx->context.mutex);

		status->valid |= RTE_STATUS_FRAMES_DROPPED;
	}
}

/* Start / Stop */

/* forward */ static void reset_input(rte_codec *codec);
/* forward */ static void reset_output(rte_context *context);

static rte_bool
stop(rte_context *context, double timestamp)
{
	mp1e_context *mx = MX(context);
	rte_context_class *xc = context->_class;
	rte_codec *codec;

	if (context->state != RTE_STATE_RUNNING) {
		rte_error_printf(context, "Context %s not running.",
				 xc->_public.keyword);
		return FALSE;
	}

	mp1e_sync_stop(&mx->sync, timestamp);

	for (codec = mx->codecs; codec; codec = codec->next) {
		mp1e_codec *md = MD(codec);
		rte_codec_class *dc = md->codec._class;

		// XXX timeout && force
		pthread_join(md->thread_id, NULL);

		md->codec.state = RTE_STATE_READY;

		/* Destroy input, reset codec, -> RTE_STATE_PARAM */

		reset_input(&md->codec);

		dc->parameters_set(&md->codec, &md->codec.params);
	}

	if (xc != &mp1e_mpeg1_video_context
	    && xc != &mp1e_mpeg1_audio_context) {
		// XXX timeout && force
		pthread_join(mx->thread_id, NULL);
	}

	reset_output(context);

	context->state = RTE_STATE_NEW;

	return TRUE;
}

static rte_bool
start(rte_context *context, double timestamp,
      rte_codec *time_ref, rte_bool async)
{
	mp1e_context *mx = MX(context);
	rte_context_class *xc = context->_class;
	int error;

	switch (context->state) {
	case RTE_STATE_READY:
		break;

	case RTE_STATE_RUNNING:
		rte_error_printf(context, "Context %s already running.",
				 xc->_public.keyword);
		return FALSE;

	default:
		rte_error_printf(context, "Cannot start context %s, initialization unfinished.",
				 xc->_public.keyword);
		return FALSE;
	}

	if (xc == &mp1e_mpeg1_video_context) {
		mp1e_codec *md = MD(mx->codecs);

		md->codec.state = RTE_STATE_RUNNING;

		error = pthread_create(&md->thread_id, NULL, mp1e_mpeg1, &md->codec);

		if (error != 0) {
			md->codec.state = RTE_STATE_READY;
			rte_error_printf(context, _("Insufficient resources to start "
						    "video encoding thread.\n"));
			return FALSE;
		}
	} else if (xc == &mp1e_mpeg1_audio_context) {
		mp1e_codec *md = MD(mx->codecs);

		md->codec.state = RTE_STATE_RUNNING;

		error = pthread_create(&md->thread_id, NULL, mp1e_mp2, &md->codec);

		if (error != 0) {
			md->codec.state = RTE_STATE_READY;
			rte_error_printf(context, _("Insufficient resources to start "
						    "audio encoding thread.\n"));
			return FALSE;
		}
	} else {
		rte_codec *codec, *ref = NULL;

		for (codec = mx->codecs; codec; codec = codec->next) {
		     /* if (codec == time_ref) {
				/x requested stream is ok x/
				ref = time_ref;
				break;
			} else */ if (!ref) {
				/* first stream, any */
				ref = codec;
			} else if (codec->_class->_public.stream_type
				   == ref->_class->_public.stream_type) {
				/* lowest index, any */
				if (codec->stream_index < ref->stream_index)
					ref = codec;
			} else if (codec->_class->_public.stream_type == RTE_STREAM_VIDEO) {
				/* video preferred */
				ref = codec;
			}
		}

		mx->sync.time_base = MD(ref)->sstr.this_module;

		for (codec = mx->codecs; codec; codec = codec->next) {
			mp1e_codec *md = MD(codec);
			rte_codec_class *dc = md->codec._class;

			md->codec.state = RTE_STATE_RUNNING;

			error = -1;

			switch (dc->_public.stream_type) {
			case RTE_STREAM_VIDEO:
				error = pthread_create(&md->thread_id, NULL,
						       mp1e_mpeg1, &md->codec);
				break;

			case RTE_STREAM_AUDIO:
				error = pthread_create(&md->thread_id, NULL,
						       mp1e_mp2, &md->codec);
				break;

			default:
				assert(!"reached");
			}

			if (error != 0) {
				md->codec.state = RTE_STATE_READY;
				rte_error_printf(context, _("Insufficient resources to start "
							    "codec thread.\n"));
				/* FIXME */
				fprintf(stderr, "Oops. -554\n");
				exit(EXIT_FAILURE);
			}
		}

		error = -1;

		if (xc == &mp1e_mpeg1_ps_context)
			error = pthread_create(&mx->thread_id, NULL,
					       mpeg1_system_mux, &mx->mux);
		else if (xc == &mp1e_mpeg1_vcd_context)
			error = pthread_create(&mx->thread_id, NULL,
					       vcd_system_mux, &mx->mux);
		else
			assert(!"reached");

		if (error != 0) {
			rte_error_printf(context, _("Insufficient resources to start "
						    "context thread.\n"));
			/* FIXME */
			fprintf(stderr, "Oops. -555\n");
			exit(EXIT_FAILURE);
		}
	}

	mp1e_sync_start(&mx->sync, timestamp);

	context->state = RTE_STATE_RUNNING;

	return TRUE;
}

/* Input / Output */

/* Elementary stream output */
static void
send_full_cp(producer *p, buffer *b)
{
	mp1e_codec *md = p->fifo->user_data;
	rte_buffer wb;

	if (b->used != 0) {
		wb.data = b->data;
		wb.size = b->used;

		if (!md->write_cb(md->codec.context, &md->codec, &wb)) {
			/* XXX what now? */
		}
	} else { /* EOF */
		md->write_cb(md->codec.context, &md->codec, NULL);
	}

	add_head(&p->fifo->empty, &b->node);
}

/* Need this since muxes have no output fifo yet */
static buffer *
mux_out(struct multiplexer *mux, buffer *b)
{
	mp1e_context *mx = PARENT(mux, mp1e_context, mux);
	rte_buffer wb;

	if (!b)
		return &mx->mux_buffer;

	if (b->used != 0) {
		wb.data = b->data;
		wb.size = b->used;
		
		if (!mx->write_cb(&mx->context, NULL, &wb)) {
				/* XXX what now? */
		}
	} else { /* EOF */
		mx->write_cb(&mx->context, NULL, NULL);
	}

	return b;
}

static void
reset_output(rte_context *context)
{
	mp1e_context *mx = MX(context);
	rte_context_class *xc = context->_class;

	if (xc == &mp1e_mpeg1_video_context
	    || xc == &mp1e_mpeg1_audio_context) {
		mp1e_codec *md = MD(mx->codecs);

		rem_consumer(&md->cons);
		destroy_fifo(md->output);
	} else {
		mx->mux.mux_output = NULL;

		destroy_buffer(&mx->mux_buffer);

		mux_destroy(&mx->mux);
	}

	context->state = RTE_STATE_NEW;
}

static rte_bool
set_output(rte_context *context,
	   rte_buffer_callback write_cb, rte_seek_callback seek_cb)
{
	mp1e_context *mx = MX(context);
	rte_context_class *xc = context->_class;
	sync_set modules = 0;
	char buf[256];
	int queue, i, j;

	switch (context->state) {
	case RTE_STATE_NEW:
		break;

	case RTE_STATE_READY:
		reset_output(context);
		break;

	default:
		rte_error_printf(context, "Cannot change %s output, context is busy.",
				 xc->_public.keyword);
		break;
	}

	if (!mx->codecs) {
		rte_error_printf(context, "No codec allocated for context %s.",
				 xc->_public.keyword);
		return FALSE;
	}

	for (i = 0, j = 0; i <= RTE_STREAM_MAX; i++) {
		rte_codec *codec;
		int count = 0;

		for (codec = mx->codecs; codec; codec = codec->next) {
			rte_codec_class *dc = codec->_class;

			if (dc->_public.stream_type != i)
				continue;

			if (codec->state != RTE_STATE_READY) {
				rte_error_printf(context, "Codec %s, elementary stream #%d, "
						 "initialization unfinished.",
						 dc->_public.keyword, codec->stream_index);
				return FALSE;
			}

			modules |= MD(codec)->sstr.this_module = 1 << j++;

			count++;
		}

		if (count < xc->_public.min_elementary[i]) {
			rte_error_printf(context, "Not enough elementary streams of rte stream type %d "
					 "for context %s. %d required, %d allocated.",
					 xc->_public.keyword, xc->_public.min_elementary[i], count);
			return FALSE;
		}
	}

	if (xc == &mp1e_mpeg1_video_context || xc == &mp1e_mpeg1_audio_context) {
		mp1e_codec *md = MD(mx->codecs);
		rte_codec_class *dc = md->codec._class;

		snprintf(buf, sizeof(buf) - 1, "%s-output-cp", dc->_public.keyword);
		queue = init_callback_fifo(&md->out_fifo, buf,
					   NULL, send_full_cp, NULL, NULL,
					   md->io_stack_size,
					   md->output_buffer_size);

		if (queue < md->io_stack_size) {
			rte_error_printf(context, _("Out of memory."));
			return FALSE;
		}

		md->output = &md->out_fifo;
		md->out_fifo.user_data = md;

		add_consumer(&md->out_fifo, &md->cons);

		md->write_cb = write_cb;

		mp1e_sync_init(&mx->sync, modules, modules);
       	} else {
		rte_codec *codec;

		if (!mux_init(&mx->mux, NULL)) {
			return FALSE;
		}

		for (codec = mx->codecs; codec; codec = codec->next) {
			mp1e_codec *md = MD(codec);
			rte_codec_class *dc = md->codec._class;

			switch (dc->_public.stream_type) {
			case RTE_STREAM_VIDEO:
				md->output = mux_add_input_stream(&mx->mux,
					VIDEO_STREAM + md->codec.stream_index,
					"mp1e-coded-video-mpeg1",
					md->output_buffer_size,
					video_buffers,
					md->output_frame_rate,
					md->output_bit_rate);
				break;

			case RTE_STREAM_AUDIO:
				md->output = mux_add_input_stream(&mx->mux,
					AUDIO_STREAM + md->codec.stream_index,
					"mp1e-coded-audio-mp2",
					1 << (ffsr(md->output_buffer_size) + 1),
					audio_buffers,
					md->output_frame_rate,
					md->output_bit_rate);
				break;

			default:
				assert(!"reached");
			}

			if (!md->output) {
				mux_destroy(&mx->mux);
				rte_error_printf(context, _("Out of memory."));
				return FALSE;
			}
		}

		/* vcd buffer is hardcoded 2324, hd default 2048 */
		/* XXX recalculate codec->mux fifo lenght before changing */
		if (!init_buffer(&mx->mux_buffer,
				 (xc == &mp1e_mpeg1_video_context) ? 4096 : 2048)) {
			mux_destroy(&mx->mux);
			rte_error_printf(context, _("Out of memory."));
			return FALSE;
		}

		mx->write_cb = write_cb;
		mx->mux.mux_output = mux_out;

		mp1e_sync_init(&mx->sync, modules, 1 /* refined in start() */);
	}

	context->state = RTE_STATE_READY;

	return TRUE;
}

static void
wait_full_ca(fifo *f)
{
	mp1e_codec *md = f->user_data;
	buffer *b = PARENT(rem_head(&f->empty), buffer, node);
	rte_buffer rb;

	assert(b != NULL);

	rb.data = NULL;
	rb.size = 0;

	if (md->read_cb(md->codec.context, &md->codec, &rb)) {
		assert(rb.data != NULL && rb.size > 0);

		b->data = rb.data;
		b->used = rb.size;
		b->time = rb.timestamp;
		b->user_data = rb.user_data;
	} else {
		b->used = 0; /* EOF */
	}

	add_tail(&f->full, &b->node);
	PARENT(f->consumers.head, consumer, node)->next_buffer = b;
}

static void
send_empty_ca(consumer *c, buffer *b)
{
	mp1e_codec *md = c->fifo->user_data;

	if (md->unref_cb) {
		rte_buffer rb;

		rb.data = b->data;
		rb.size = b->used;
		rb.user_data = b->user_data;

		md->unref_cb(md->codec.context, &md->codec, &rb);
	}

	unlink_node(&c->fifo->full, &b->node);
	add_head(&c->fifo->empty, &b->node);
}

static void
wait_full_cp(fifo *f)
{
	mp1e_codec *md = f->user_data;
	buffer *b = PARENT(rem_head(&f->empty), buffer, node);
	rte_buffer rb;

	rb.data = b->data;
	rb.size = b->size;

	if (md->read_cb(md->codec.context, &md->codec, &rb)) {
		assert(rb.data == b->data && rb.size <= b->size);

		b->used = rb.size;
		b->time = rb.timestamp;
		b->user_data = rb.user_data;
	} else {
		b->used = 0; /* EOF */
	}

	send_full_buffer(&md->prod, b);
}

static rte_bool
push_buffer(rte_codec *codec, rte_buffer *wb, rte_bool blocking)
{
	mp1e_codec *md = MD(codec);
	buffer *b = NULL;

	if (md->codec.state != RTE_STATE_RUNNING) {
		rte_error_printf(codec->context, "Codec %s not running.",
				 codec->_class->_public.keyword);
		return FALSE;
	}

	if (!wb || wb->data) {
		if (blocking)
			b = wait_empty_buffer(&md->prod);
		else {
			if (!(b = recv_empty_buffer(&md->prod))) {
				errno = EAGAIN;
				return FALSE;
			}
		}

		/* FIXME out-of-order */
		if (md->unref_cb && b->data && b->used) {
			rte_buffer rb;

			rb.data = b->data;
			rb.size = b->used;
			rb.user_data = b->user_data;

			md->unref_cb(md->codec.context, &md->codec, &rb);
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

			send_full_buffer(&md->prod, b);
		}

		if (md->input_method == RTE_PUSH_SLAVE) {
			/* FIXME */
			wb->data = b->allocated;
			wb->size = b->size;
			wb->user_data = b->user_data;
		}
	} else {
		b->used = 0; /* EOF */

		send_full_buffer(&md->prod, b);
	}

	return TRUE;
}

static void
reset_input(rte_codec *codec)
{
	mp1e_codec *md = MD(codec);

	rem_producer(&md->prod);
	destroy_fifo(md->input);

	codec->state = RTE_STATE_PARAM;
}

static rte_bool
set_input(rte_codec *codec, rte_io_method input_method,
	  rte_buffer_callback read_cb, rte_buffer_callback unref_cb,
	  unsigned int *queue_length)
{
	mp1e_codec *md = MD(codec);
	rte_codec_class *dc = codec->_class;
	rte_context *context = codec->context;
	char buf[256];
	int queue;

	switch (codec->state) {
	case RTE_STATE_NEW:
		rte_error_printf(context, "Attempt to select input method with "
					  "uninitialized sample parameters.");
		return FALSE;

	case RTE_STATE_PARAM:
		break;

	case RTE_STATE_READY:
		reset_input(codec);
		break;

	default:
		rte_error_printf(context, "Cannot change %s input, codec is busy.",
				 dc->_public.keyword);
		break;
	}

	if (input_method != RTE_CALLBACK_MASTER) {
		rte_error_printf(context, "Input method not supported (broken).");
		return FALSE;
	}

	/* FIXME broken */

	switch (input_method) {
	case RTE_CALLBACK_MASTER:
		queue = MAX(*queue_length, md->io_stack_size);
		snprintf(buf, sizeof(buf) - 1, "%s-input-ca", dc->_public.keyword);
		queue = init_callback_fifo(&md->in_fifo, buf, NULL, NULL,
					   wait_full_ca, send_empty_ca,
					   md->io_stack_size, 0);
		break;

	case RTE_CALLBACK_SLAVE:
		queue = MAX(*queue_length, md->io_stack_size);
		snprintf(buf, sizeof(buf) - 1, "%s-input-cp", dc->_public.keyword);
		queue = init_callback_fifo(&md->in_fifo, buf, NULL, NULL,
					   wait_full_cp, NULL, queue,
					   md->input_buffer_size);
		break;

	case RTE_PUSH_MASTER:
		queue = MAX(*queue_length, md->io_stack_size + 1);
		snprintf(buf, sizeof(buf) - 1, "%s-input-pa", dc->_public.keyword);
		queue = init_buffered_fifo(&md->in_fifo, buf, queue, 0);
		break;

	case RTE_PUSH_SLAVE:
		queue = MAX(*queue_length, md->io_stack_size);
		snprintf(buf, sizeof(buf) - 1, "%s-input-pp", dc->_public.keyword);
		queue = init_buffered_fifo(&md->in_fifo, buf,
					   queue, md->input_buffer_size);
		unref_cb = NULL;
		break;

	default:
		assert(!"rte bug");
	}

	if (queue < md->io_stack_size) {
		destroy_fifo(&md->in_fifo);
		return FALSE;
	}

	md->input = &md->in_fifo;
	md->in_fifo.user_data = md;

	add_producer(&md->in_fifo, &md->prod);

	*queue_length = queue;

	md->input_method = input_method;
	md->read_cb = read_cb;
	md->unref_cb = unref_cb;

	codec->state = RTE_STATE_READY;

	return TRUE;
}

/* Sampling parameters */

static rte_bool
parameters_set(rte_codec *codec, rte_stream_parameters *rsp)
{
	rte_codec_class *dc = codec->_class;

	switch (codec->state) {
	case RTE_STATE_NEW:
	case RTE_STATE_PARAM:
		break;
	case RTE_STATE_READY:
		reset_input(codec);
		break;
	default:
		rte_error_printf(codec->context, "Cannot change %s parameters, codec is busy.",
				 dc->_public.keyword);
		return FALSE;
	}

	if (codec->context->_class == &mp1e_mpeg1_vcd_context) {
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

/* Codec options */

static rte_bool
option_set(rte_codec *codec, const char *keyword, va_list args)
{
	if (codec->state == RTE_STATE_READY)
		reset_input(codec);

	return codec->_class->option_set(codec, keyword, args);
}

static rte_option_info *
option_enum(rte_codec *codec, unsigned int index)
{
	rte_codec_class *dc = codec->_class;

	if (codec->context->_class == &mp1e_mpeg1_vcd_context) {
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

/* Codec allocation */

static rte_codec_class *
codec_table[] = {
	&mp1e_mpeg1_video_codec,
	&mp1e_mpeg1_layer2_codec,
	&mp1e_mpeg2_layer2_codec,
};

static const int num_codecs = sizeof(codec_table) / sizeof(codec_table[0]);

static rte_codec *
codec_get(rte_context *context, rte_stream_type stream_type,
	  unsigned int stream_index)
{
	mp1e_context *mx = MX(context);
	rte_codec *codec;

	for (codec = mx->codecs; codec; codec = codec->next)
		if (codec->_class->_public.stream_type == stream_type
		    && codec->stream_index == stream_index)
			return codec;

	return NULL;
}

static rte_codec *
codec_set(rte_context *context, const char *keyword,
	  rte_stream_type stream_type, unsigned int stream_index)
{
	mp1e_context *mx = MX(context);
	rte_context_class *xc = context->_class;
	rte_codec_class *dc;
	rte_codec *old, **oldpp, *codec = NULL;
	int i;

	for (oldpp = &mx->codecs; (old = *oldpp); oldpp = &old->next) {
		dc = old->_class;

		if (keyword) {
			if (strcmp(dc->_public.keyword, keyword) == 0)
				break;
		} else {
			if (dc->_public.stream_type == stream_type
			    && old->stream_index == stream_index)
				break;
		}
	}

	if (keyword) {
		char *error = NULL;
		int max_elem;

		for (i = 0; i < num_codecs; i++)
			if (strcmp(codec_table[i]->_public.keyword, keyword) == 0)
				break;

		if (i >= num_codecs) {
			rte_error_printf(context, "'%s' is no codec of the %s encoder.",
					 keyword, xc->_public.keyword);
			return NULL;
		}

		dc = codec_table[i];

		stream_type = dc->_public.stream_type;

		max_elem = xc->_public.max_elementary[stream_type];

		if (max_elem == 0) {
			rte_error_printf(context, "'%s' is no valid codec for the %s encoder, "
					 "wrong stream type.", dc->_public.keyword, xc->_public.keyword);
			return NULL;
		} else if (stream_index >= max_elem) {
			rte_error_printf(context, "'%s' selected for elementary stream %d "
					 "of %s encoder, but only %d available.",
					 dc->_public.keyword, stream_index,
					 xc->_public.keyword, max_elem);
			return NULL;
		}

		if (!old && mx->num_codecs >= MAX_ELEMENTARY_STREAMS) {
			rte_error_printf(context, "Limit of %s codecs (%d) reached.",
					 xc->_public.keyword, MAX_ELEMENTARY_STREAMS);
			return NULL;
		}

		if (!(codec = dc->_new(dc, &error))) {
			if (error) {
				rte_error_printf(context, _("Cannot create new codec instance '%s'. %s"),
						 dc->_public.keyword, error);
				free(error);
			} else {
				rte_error_printf(context, _("Cannot create new codec instance '%s'."),
						 dc->_public.keyword);
			}

			return NULL;
		}

		assert(error == NULL);

		codec->context = context;

		if (!rte_codec_options_reset(codec)) {
			dc->_delete(codec);
			return NULL;
		}
	}

	if (old) {
		*oldpp = old->next;

		if (old->state == RTE_STATE_READY)
			reset_input(old);

		old->_class->_delete(old);
		mx->num_codecs--;
	}

	if (codec) {
		codec->next = mx->codecs;
		mx->codecs = codec;
		mx->num_codecs++;
	}

	return codec;
}

static rte_codec_info *
codec_enum(rte_context *context, unsigned int index)
{
	rte_context_class *xc = context->_class;
	rte_context_info *xi = &xc->_public;
	int i;

	if (index < 0)
		return NULL;

	for (i = 0; i < num_codecs; i++) {
		if (xi->max_elementary[codec_table[i]->_public.stream_type] <= 0)
			continue;
		if (xc == &mp1e_mpeg1_vcd_context
		    && codec_table[i] != &mp1e_mpeg1_video_codec
		    && codec_table[i] != &mp1e_mpeg1_layer2_codec)
			continue;
		if (index-- == 0)
			return &codec_table[i]->_public;
	}

	return NULL;
}

/* Context allocation */

static rte_context_class
mp1e_mpeg1_ps_context = {
	._public = {
		.keyword	= "mp1e_mpeg1_ps",
		.label		= N_("MPEG-1 Program Stream"),
		.mime_type	= "video/x-mpeg",
		.extension	= "mpg,mpe,mpeg",
		.min_elementary	= { 0, 0, 0 },
		.max_elementary	= { 0, 1, 1 }, /* to be { 0, 16, 32, 1 }, */
	}
};

static rte_context_class
mp1e_mpeg1_vcd_context = {
	._public = {
		.keyword	= "mp1e_mpeg1_vcd",
		.label		= N_("MPEG-1 VCD Program Stream"),
		.mime_type	= "video/x-mpeg",
		.extension	= "mpg,mpe,mpeg",
		.min_elementary	= { 0, 1, 1 },
		.max_elementary	= { 0, 1, 1 },
	}
};

static rte_context_class
mp1e_mpeg1_video_context = {
	._public = {
		.keyword	= "mp1e_mpeg_video",
		.label		= N_("MPEG Video Elementary Stream"),
		.mime_type	= "video/x-mpeg",
		.extension	= "mpg,mpe,mpeg",
		.min_elementary	= { 0, 1, 0 },
		.max_elementary	= { 0, 1, 0 },
	}
};

static rte_context_class
mp1e_mpeg1_audio_context = {
	._public = {
		.keyword	= "mp1e_mpeg_audio",
		.label		= N_("MPEG Audio Elementary Stream"),
		.mime_type	= "audio/x-mpeg",
		.extension	= "mp2,mpga", /* note */
		.min_elementary	= { 0, 0, 1 },
		.max_elementary	= { 0, 0, 1 },
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
	mp1e_context *mx = MX(context);

	switch (context->state) {
	case RTE_STATE_RUNNING:
	case RTE_STATE_PAUSED:
		assert(!"reached");

	case RTE_STATE_READY:
		reset_output(context);
		break;

	default:
		break;
	}

	/* Delete codecs */

	while (mx->codecs) {
		rte_codec *codec = mx->codecs;

		codec_set(context, NULL,
			  codec->_class->_public.stream_type,
			  codec->stream_index);
	}

	pthread_mutex_destroy(&context->mutex);

	free(mx);
}

static rte_context *
context_new(rte_context_class *xc, char **errstr)
{
	mp1e_context *mx;
	rte_context *context;

	if (!(mx = calloc(1, sizeof(*mx)))) {
		rte_asprintf(errstr, _("Out of memory."));
		return NULL;
	}

	context = &mx->context;

	context->_class = xc;

	pthread_mutex_init(&context->mutex, NULL);

	context->state = RTE_STATE_NEW;

	return context;
}

/* Backend initialization */

static rte_option_info *
clone_option(rte_codec *codec, int index, rte_option_info **options, int *num)
{
	rte_option_info *old, *new;

	if (index >= *num) {
		assert(codec->_class->option_enum != NULL);

		if (!(old = codec->_class->option_enum(codec, index)))
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
		context_table[i]->_public.backend = "mp1e 1.9.2";

		context_table[i]->_new = context_new;
		context_table[i]->_delete = context_delete;

		context_table[i]->codec_enum = codec_enum;
		context_table[i]->codec_get = codec_get;
		context_table[i]->codec_set = codec_set;

		context_table[i]->codec_option_set = option_set;
		context_table[i]->parameters_set = parameters_set;

		context_table[i]->set_input = set_input;
		context_table[i]->push_buffer = push_buffer;

		context_table[i]->set_output = set_output;

		context_table[i]->start = start;
		context_table[i]->stop = stop;

		context_table[i]->status = status;
	}

	mp1e_mpeg1_vcd_context.codec_option_enum = option_enum;

	codec._class = &mp1e_mpeg1_video_codec;

	for (i = 0; (oi = clone_option(&codec, i, &vcd_mpeg1_options, &vcd_mpeg1_num_options)); i++) {
		if (KEYWORD("bit_rate")) {
			oi->def.num = oi->min.num = oi->max.num = 1150000;
		}
	}

	codec._class = &mp1e_mpeg1_layer2_codec;

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

	filter_mode = -1;
}

static rte_context_class *
context_enum(unsigned int index, char **errstr)
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
