/*
 *  Real Time Encoder lib
 *  ffmpeg backend
 *
 *  Copyright (C) 2000, 2001 I? Garc?Etxebarria
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

/* $Id: b_ffmpeg.c,v 1.6 2002-06-14 07:57:40 mschimek Exp $ */

#include "b_ffmpeg.h"

/*
 *  This is just an early prototype, lots of things to do.
 *
 *  + options
 *  + video codecs
 *  + a+v formats
 *  + status
 *  + errors
 *  + sync
 *  + need a solution to external libs ffmpeg can use (lame, ogg)
 */

/* Dummies */

/* libav/util.c */
URLProtocol udp_protocol;
URLProtocol http_protocol;


static void
status(rte_context *context, rte_codec *codec,
       rte_status *status, int size)
{
	status->valid = 0;
}

/* Start / Stop */

/* forward */ static void reset_input(ffmpeg_codec *);
/* forward */ static void reset_output(ffmpeg_context *);

static void
do_write(void *opaque, UINT8 *buf, int buf_size)
{
	ffmpeg_context *fx = opaque;
	rte_buffer wb;

	wb.data = buf;
	wb.size = buf_size;

	if (!fx->write_cb(&fx->context, NULL, &wb)) {
		/* XXX what now? */
	}
}

static int
do_seek(void *opaque, offset_t offset, int whence)
{
	ffmpeg_context *fx = opaque;

	/* XXX error */
	fx->seek_cb(&fx->context, offset, whence);

	return 0; // XXX
}

#define MAX_AUDIO_PACKET_SIZE 16384

static void
do_audio_out(ffmpeg_context *fx, ffmpeg_codec *fd,
	     unsigned char *buf, int size)
{
    UINT8 *buftmp;
    UINT8 audio_buf[2*MAX_AUDIO_PACKET_SIZE]; /* XXX: allocate it */
    UINT8 audio_out[MAX_AUDIO_PACKET_SIZE]; /* XXX: allocate it */
    int size_out, frame_bytes, ret;
    AVCodecContext *enc;

    enc = &fd->str.codec;

    buftmp = buf;
    size_out = size;

    /* now encode as many frames as possible */
    if (enc->frame_size > 1) {
	assert(0);
    } else {
        /* output a pcm frame */
        /* XXX: change encoding codec API to avoid this ? */
        switch(enc->codec->id) {
        case CODEC_ID_PCM_S16LE:
        case CODEC_ID_PCM_S16BE:
        case CODEC_ID_PCM_U16LE:
        case CODEC_ID_PCM_U16BE:
            break;
        default:
            size_out = size_out >> 1;
            break;
        }
        ret = avcodec_encode_audio(enc, audio_out, size_out, (short *)buftmp);
	fx->av.format->write_packet(&fx->av, /* index */ 0, audio_out, ret, 0);
    }
}

static void *
mainloop(void *p)
{
	ffmpeg_context *fx = p;
	ffmpeg_context_class *fxc = FXC(fx->context.class);
	ffmpeg_codec *fd;
	ffmpeg_codec_class *fdc;
	rte_codec *codec;
	int i;

	memset(&fx->av, 0, sizeof(fx->av));

	init_put_byte(&fx->av.pb, fx->buf,
		      sizeof(fx->buf), /* write_flag */ TRUE,
		      fx, NULL, do_write, do_seek);

	fx->av.format = fxc->av;

	for (codec = fx->codecs, i = 0; codec; codec = codec->next, i++) {
		fd = FD(codec);
		fdc = FDC(fd->codec.class);

		fx->av.streams[i] = &fd->str;

		fd->pts = 0;
		fd->eof = 0;
		fd->pts_increment = 0;
		fd->frame_number = 0;
		fd->sample_index = 0;

		fd->str.codec.codec_id = fdc->av->id;

		switch (fdc->rte.public.stream_type) {
		case RTE_STREAM_AUDIO:
			fd->str.codec.codec_type = CODEC_TYPE_AUDIO;
			ticker_init(&fd->pts_ticker,
				    (INT64) fd->str.codec.sample_rate,
				    (INT64) (1000000));
			break;

		case RTE_STREAM_VIDEO:
			fd->str.codec.codec_type = CODEC_TYPE_VIDEO;
			ticker_init(&fd->pts_ticker,
				    (INT64) fd->str.codec.frame_rate,
				    ((INT64) 1000000 * FRAME_RATE_BASE));
			break;

		default:
			assert(!"reached");
		}
	}

	/* Header */

        if (fxc->av->write_header(&fx->av) < 0) {
#warning TODO
		/* "Could not write header for output file "
		   "#%d (incorrect codec paramters ?)\n" */
		exit(0);
        }

	/* Body */

	for (;;) {
		rte_buffer rb;
		ffmpeg_codec *min_fd = NULL;
		int64_t min_pts = INT64_MAX;

		pthread_mutex_lock(&fx->context.mutex);

		/* find input stream with lowest PTS */

		for (codec = fx->codecs; codec; codec = codec->next) {
			fd = FD(codec);

			if (fd->pts < min_pts && !fd->eof) {
				min_pts = fd->pts;
				min_fd = fd;
			}
		}

		if (!(fd = min_fd))
			break; /* all elementary streams done */

		if (min_pts >= fx->stop_pts) {
			fd->eof = TRUE;
			pthread_mutex_unlock(&fx->context.mutex);
			continue;
		}

		fdc = FDC(fd->codec.class);

		/* fetch data */

		rb.data = NULL;
		rb.size = 0;

		if (fd->read_cb(fd->codec.context, &fd->codec, &rb)) {
			assert(rb.data != NULL && rb.size > 0);
		} else {
			fd->eof = TRUE;
			pthread_mutex_unlock(&fx->context.mutex);
			continue;
		}

		/* increment PTS */

		switch (fdc->rte.public.stream_type) {
		case RTE_STREAM_AUDIO:
			fd->pts = ticker_tick(&fd->pts_ticker, fd->sample_index);
			fd->sample_index += rb.size / (2 * fd->str.codec.channels);
			fd->pts_increment = (INT64) (rb.size / (2 * fd->str.codec.channels))
				* 1000000 / fd->str.codec.sample_rate;
			break;

		case RTE_STREAM_VIDEO:
			fd->frame_number++;
			fd->pts = ticker_tick(&fd->pts_ticker, fd->frame_number);
			fd->pts_increment = ((INT64) 1000000 * FRAME_RATE_BASE)
				/ fd->str.codec.frame_rate;
			break;

		default:
			assert(!"reached");
		}

		pthread_mutex_unlock(&fx->context.mutex);

		/* encode packet */

		switch (fdc->rte.public.stream_type) {
		case RTE_STREAM_AUDIO:
			do_audio_out(fx, fd, rb.data, rb.size);
			break;

		case RTE_STREAM_VIDEO:
			/* do_video_out(os, ost, ist, &picture, &frame_size); */
			break;

		default:
			assert(!"reached");
		}
	}

	pthread_mutex_unlock(&fx->context.mutex);

	/* Trailer */

        fxc->av->write_trailer(&fx->av);

	/* EOF */

	fx->write_cb(&fx->context, NULL, NULL);

	return NULL;
}

static rte_bool
stop(rte_context *context, double timestamp)
{
	ffmpeg_context *fx = FX(context);
	ffmpeg_context_class *fxc = FXC(fx->context.class);
	ffmpeg_codec *fd, *max_fd = NULL;
	int64_t max_pts = 0;
	rte_codec *codec;

	if (context->state != RTE_STATE_RUNNING) {
		rte_error_printf(&fx->context, "Context %s not running.",
				 fxc->rte.public.keyword);
		return FALSE;
	}

	pthread_mutex_lock(&fx->context.mutex);

	/* find input stream with highest PTS */

	for (codec = fx->codecs; codec; codec = codec->next) {
		fd = FD(codec);

		if (fd->pts > max_pts && !fd->eof) {
			max_pts = fd->pts;
			max_fd = fd;
		}
	}

	if (max_fd) {
		fx->stop_pts = max_pts;
	} /* else done already */

	pthread_mutex_unlock(&fx->context.mutex);

	// XXX timeout && force
	pthread_join(fx->thread_id, NULL);

	for (codec = fx->codecs; codec; codec = codec->next) {
		static rte_bool parameters_set(rte_codec *, rte_stream_parameters *);
		ffmpeg_codec *fd = FD(codec);

		fd->codec.state = RTE_STATE_READY;

		/* Destroy input, reset codec, -> RTE_STATE_PARAM */

		reset_input(fd);

		parameters_set(&fd->codec, &fd->codec.params);
	}

	reset_output(fx);

	fx->context.state = RTE_STATE_NEW;

	return TRUE;
}

static rte_bool
start(rte_context *context, double timestamp,
      rte_codec *time_ref, rte_bool async)
{
	ffmpeg_context *fx = FX(context);
	ffmpeg_context_class *fxc = FXC(fx->context.class);
	rte_codec *codec;
	int error;

	switch (fx->context.state) {
	case RTE_STATE_READY:
		break;

	case RTE_STATE_RUNNING:
		rte_error_printf(&fx->context, "Context %s already running.",
				 fxc->rte.public.keyword);
		return FALSE;

	default:
		rte_error_printf(&fx->context, "Cannot start context %s, initialization unfinished.",
				 fxc->rte.public.keyword);
		return FALSE;
	}

	for (codec = fx->codecs; codec; codec = codec->next)
		if (codec->state != RTE_STATE_READY) {
			rte_error_printf(&fx->context, "Cannot start context %s, initialization "
					 "of codec %s unfinished.",
					 fxc->rte.public.keyword, codec->class->public.keyword);
			return FALSE;
		}

	fx->context.state = RTE_STATE_RUNNING;

	for (codec = fx->codecs; codec; codec = codec->next)
		codec->state = RTE_STATE_RUNNING;

	fx->stop_pts = INT64_MAX;

	error = pthread_create(&fx->thread_id, NULL, mainloop, fx);

	if (error != 0) {
		for (codec = fx->codecs; codec; codec = codec->next)
			codec->state = RTE_STATE_READY;

		fx->context.state = RTE_STATE_READY;

		rte_error_printf(&fx->context, _("Insufficient resources to start "
						 "video encoding thread.\n"));
		return FALSE;
	}

        return TRUE;
}

/* Input / Output */

static void
reset_output(ffmpeg_context *fx)
{
	fx->context.state = RTE_STATE_NEW;
}

static rte_bool
set_output(rte_context *context,
           rte_buffer_callback write_cb, rte_seek_callback seek_cb)
{
	ffmpeg_context *fx = FX(context);
	ffmpeg_context_class *fxc = FXC(fx->context.class);
	int i;

	switch (fx->context.state) {
	case RTE_STATE_NEW:
		break;

	case RTE_STATE_READY:
		reset_output(fx);
		break;

	default:
		rte_error_printf(&fx->context, "Cannot change %s output, context is busy.",
				 fxc->rte.public.keyword);
		break;
	}

	if (!fx->codecs) {
		rte_error_printf(context, "No codec allocated for context %s.",
				 fxc->rte.public.keyword);
		return FALSE;
	}

	for (i = 0; i <= RTE_STREAM_MAX; i++) {
		rte_codec *codec;
		int count = 0;

		for (codec = fx->codecs; codec; codec = codec->next) {
			rte_codec_class *dc = codec->class;

			if (dc->public.stream_type != i)
				continue;

			if (codec->state != RTE_STATE_READY) {
				rte_error_printf(&fx->context, "Codec %s, elementary stream #%d, "
						 "initialization unfinished.",
						 dc->public.keyword, codec->stream_index);
				return FALSE;
			}

			count++;
		}

		if (count < fxc->rte.public.min_elementary[i]) {
			rte_error_printf(&fx->context, "Not enough elementary streams of rte stream type %d "
					 "for context %s. %d required, %d allocated.",
					 fxc->rte.public.keyword,
					 fxc->rte.public.min_elementary[i], count);
			return FALSE;
		}
	}

	fx->write_cb = write_cb;
	fx->seek_cb = seek_cb;

	fx->context.state = RTE_STATE_READY;

	return TRUE;
}

static void
reset_input(ffmpeg_codec *fd)
{
	fd->codec.state = RTE_STATE_PARAM;
}

static rte_bool
set_input(rte_codec *codec, rte_io_method input_method,
          rte_buffer_callback read_cb, rte_buffer_callback unref_cb, int *queue_length)
{
	ffmpeg_codec *fd = FD(codec);
	ffmpeg_codec_class *fdc = FDC(fd->codec.class);
	rte_context *context = fd->codec.context;

	switch (fd->codec.state) {
	case RTE_STATE_NEW:
		rte_error_printf(context, "Attempt to select input method with "
					  "uninitialized sample parameters.");
		return FALSE;

	case RTE_STATE_PARAM:
		break;

	case RTE_STATE_READY:
		reset_input(fd);
		break;

	default:
		rte_error_printf(context, "Cannot change %s input, codec is busy.",
				 fdc->rte.public.keyword);
		break;
	}

	switch (input_method) {
	case RTE_CALLBACK_ACTIVE:
	  *queue_length = 1;
	  break;

	case RTE_CALLBACK_PASSIVE:
	case RTE_PUSH_PULL_ACTIVE:
	case RTE_PUSH_PULL_PASSIVE:
		rte_error_printf(context, "Selected input method not supported yet.");
		return FALSE;

	default:
		assert(!"rte bug");
	}

	fd->input_method = input_method;
	fd->read_cb = read_cb;
	fd->unref_cb = unref_cb;

	fd->codec.state = RTE_STATE_READY;

	return TRUE;
}

/* Sampling parameters */

static rte_bool
parameters_set(rte_codec *codec, rte_stream_parameters *rsp)
{
	ffmpeg_codec *fd = FD(codec);
	ffmpeg_codec_class *fdc = FDC(fd->codec.class);
	struct AVCodecContext *avcc = &fd->str.codec;

	switch (fdc->rte.public.stream_type) {
	case RTE_STREAM_AUDIO:
		rsp->audio.sndfmt = RTE_SNDFMT_S16_LE; /* XXX machine endian */

		rsp->audio.sampling_freq = avcc->sample_rate;
		rsp->audio.channels = avcc->channels;

		if (avcodec_open(avcc, fdc->av) < 0) {
			// XXX
			fprintf(stderr, "could not open codec\n");
			exit(1);
		}

		if (avcc->frame_size == 1)
			rsp->audio.fragment_size = 4096 * 2 * avcc->channels;
		else
			rsp->audio.fragment_size = avcc->frame_size * 2 * avcc->channels;

		break;

	default:
		assert (!"reached");
	}

	fd->status.valid = 0;

	fd->codec.state = RTE_STATE_PARAM;

	return TRUE;
}

/* Codec options */

#define OPTION_OPEN_SAMPLING	(1 << 0)
#define OPTION_STEREO		(1 << 1)

/* Attention: Canonical order */
static const char *
menu_audio_mode[] = {
	/* 0 */ N_("Mono"),
	/* 1 */ N_("Stereo"),
	/* NLS: Bilingual for example left audio channel English, right French */
	/* 2 */ N_("Bilingual"), 
	/* 3 TODO N_("Joint Stereo"), */
};

static const struct {
	unsigned int		opt;
	rte_option_info		info;
} options[] = {
	{ OPTION_OPEN_SAMPLING,		RTE_OPTION_INT_RANGE_INITIALIZER
	  ("sampling_freq", N_("Sampling frequency"),
	   44100, 8000, 48000, 100, NULL) },
	{ OPTION_STEREO,		RTE_OPTION_MENU_INITIALIZER
	  ("audio_mode", N_("Mode"),
	   0, menu_audio_mode, 2, NULL) },
};

static const int num_options = sizeof(options) / sizeof(* options);

#define KEYWORD(name) (strcmp(keyword, name) == 0)
#define KEYOPT(type, name) ((fdc->options & type) && KEYWORD(name))

static char *
option_print(rte_codec *codec, const char *keyword, va_list args)
{
        ffmpeg_codec *fd = FD(codec);
        ffmpeg_codec_class *fdc = FDC(fd->codec.class);
	rte_context *context = fd->codec.context;
	char buf[80];

	if (KEYWORD("sampling_freq")) {
		snprintf(buf, sizeof(buf), _("%u Hz"), va_arg(args, int));
	} else if (KEYWORD("audio_mode")) {
		return rte_strdup(context, NULL, _(menu_audio_mode[
			RTE_OPTION_ARG_MENU(menu_audio_mode)]));
	} else {
		rte_unknown_option(context, codec, keyword);
	failed:
		return NULL;
	}

	return rte_strdup(context, NULL, buf);
}

static rte_bool
option_get(rte_codec *codec, const char *keyword, rte_option_value *v)
{
        ffmpeg_codec *fd = FD(codec);
        ffmpeg_codec_class *fdc = FDC(fd->codec.class);
	rte_context *context = fd->codec.context;

	if (KEYOPT(OPTION_OPEN_SAMPLING, "sampling_freq")) {
		v->num = fd->str.codec.sample_rate;
	} else if (KEYOPT(OPTION_STEREO, "audio_mode")) {
		v->num = fd->str.codec.channels - 1;
	} else {
		rte_unknown_option(context, codec, keyword);
		return FALSE;
	}

        return TRUE;
}

static rte_bool
option_set(rte_codec *codec, const char *keyword, va_list args)
{
	ffmpeg_codec *fd = FD(codec);
	ffmpeg_codec_class *fdc = FDC(fd->codec.class);
	rte_context *context = fd->codec.context;

	if (fd->codec.state == RTE_STATE_READY)
		reset_input(fd);

	if (fd->codec.state == RTE_STATE_PARAM) {
		avcodec_close(&fd->str.codec);
		fd->codec.state = RTE_STATE_NEW;
	}

	if (KEYOPT(OPTION_OPEN_SAMPLING, "sampling_freq")) {
		fd->str.codec.sample_rate = RTE_OPTION_ARG(int, 8000, 48000);
	} else if (KEYOPT(OPTION_STEREO, "audio_mode")) {
		fd->str.codec.channels = RTE_OPTION_ARG(int, 0, 2) + 1;
	} else {
		rte_unknown_option(context, codec, keyword);
	failed:
		return FALSE;
	}

        return TRUE;
}

static const rte_option_info *
option_enum(rte_codec *codec, int index)
{
	ffmpeg_codec *fd = FD(codec);
	ffmpeg_codec_class *fdc = FDC(fd->codec.class);
	int i;

	for (i = 0; i < num_options; i++)
		if (options[i].opt & fdc->options) {
			if (index == 0)
				return &options[i].info;
			else
				index--;
		}

	return NULL;
}

/* Codec allocation */

extern AVCodec pcm_s16le_encoder;
extern AVCodec pcm_u8_encoder;
extern AVCodec pcm_alaw_encoder;
extern AVCodec pcm_mulaw_encoder;

ffmpeg_codec_class
pcm_s16le_codec = {
	.av 		= &pcm_s16le_encoder,
	.options	= OPTION_OPEN_SAMPLING |
			  OPTION_STEREO,
        .rte.public = {
                .stream_type    = RTE_STREAM_AUDIO,
                .keyword        = "pcm_s16le",
                .label          = N_("PCM 16 Bit Signed Little Endian"),
        },
};

ffmpeg_codec_class
pcm_u8_codec = {
	.av		= &pcm_u8_encoder,
	.options	= OPTION_OPEN_SAMPLING | 
			  OPTION_STEREO,
        .rte.public = {
                .stream_type    = RTE_STREAM_AUDIO,
                .keyword        = "pcm_u8",
                .label          = N_("PCM 8 Bit Unsigned"),
        },
};

ffmpeg_codec_class
pcm_alaw_codec = {
	.av		= &pcm_alaw_encoder,
	.options	= OPTION_OPEN_SAMPLING | 
			  OPTION_STEREO,
        .rte.public = {
                .stream_type    = RTE_STREAM_AUDIO,
                .keyword        = "pcm_alaw",
                .label          = N_("PCM a-Law"),
        },
};

ffmpeg_codec_class
pcm_mulaw_codec = {
	.av		= &pcm_mulaw_encoder,
	.options	= OPTION_OPEN_SAMPLING | 
			  OPTION_STEREO,
        .rte.public = {
                .stream_type    = RTE_STREAM_AUDIO,
                .keyword        = "pcm_mulaw",
                .label          = N_("PCM mu-Law"),
	},
};

static void
codec_delete(rte_codec *codec)
{
        ffmpeg_codec *fd = FD(codec);

        switch (fd->codec.state) {
        case RTE_STATE_READY:
                assert(!"reached");
                break;

        case RTE_STATE_RUNNING:
        case RTE_STATE_PAUSED:
                fprintf(stderr, "ffmpeg bug warning: attempt to delete "
                        "running codec ignored\n");
                return;

	case RTE_STATE_PARAM:
		avcodec_close(&fd->str.codec);
		break;

        default:
                break;
        }

        pthread_mutex_destroy(&fd->codec.mutex);

        free(fd);
}

static rte_codec *
codec_new(rte_codec_class *cc, char **errstr)
{
        ffmpeg_codec *fd;

	if (!(fd = calloc(sizeof(*fd), 1))) {
		rte_asprintf(errstr, _("Out of memory."));
		return NULL;
	}

        fd->codec.class = cc;

        pthread_mutex_init(&fd->codec.mutex, NULL);

	memset(&fd->str.codec, 0, sizeof(fd->str.codec));

        fd->codec.state = RTE_STATE_NEW;

        return &fd->codec;
}

static rte_codec *
codec_get(rte_context *context, rte_stream_type stream_type, int stream_index)
{
	ffmpeg_context *fx = FX(context);
	rte_codec *codec;

	for (codec = fx->codecs; codec; codec = codec->next)
		if (codec->class->public.stream_type == stream_type
		    && codec->stream_index == stream_index)
			return codec;

	return NULL;
}

static rte_codec *
codec_set(rte_context *context, const char *keyword,
          rte_stream_type stream_type, int stream_index)
{
	ffmpeg_context *fx = FX(context);
	ffmpeg_context_class *fxc = FXC(fx->context.class);
	ffmpeg_codec *fd = NULL;
	ffmpeg_codec_class *fdc;
	rte_codec *old, **oldpp;
	int i;

	for (oldpp = &fx->codecs; (old = *oldpp); oldpp = &old->next) {
		fdc = FDC(old->class);

		if (keyword) {
			if (strcmp(fdc->rte.public.keyword, keyword) == 0)
				break;
		} else {
			if (fdc->rte.public.stream_type == stream_type
			    && old->stream_index == stream_index)
				break;
		}
	}

	if (keyword) {
		rte_codec *codec;
		char *error = NULL;
		int max_elem;

		for (i = 0; (fdc = fxc->codecs[i]); i++)
			if (strcmp(fdc->rte.public.keyword, keyword) == 0)
				break;

		if (!fdc) {
			rte_error_printf(&fx->context, "'%s' is no codec of the %s encoder.",
					 keyword, fxc->rte.public.keyword);
			return NULL;
		}

		stream_type = fdc->rte.public.stream_type;
		max_elem = fxc->rte.public.max_elementary[stream_type];

		assert(max_elem > 0);

		if (stream_index >= max_elem) {
			rte_error_printf(&fx->context, "'%s' selected for "
					 "elementary stream %d of %s encoder, "
					 "but only %d available.",
					 fdc->rte.public.keyword, stream_index,
					 fxc->rte.public.keyword, max_elem);
			return NULL;
		}

		if (!(codec = codec_new(&fdc->rte, &error))) {
			if (error) {
				rte_error_printf(&fx->context,
						 _("Cannot create new codec instance '%s': %s"),
						 fdc->rte.public.keyword, error);
				free(error);
			} else {
				rte_error_printf(&fx->context,
						 _("Cannot create new codec instance '%s'."),
						 fdc->rte.public.keyword);
			}

			return NULL;
		}

		fd = FD(codec);
		fd->codec.context = &fx->context;

		if (!rte_codec_options_reset(&fd->codec)) {
			codec_delete(&fd->codec);
			return NULL;
		}
	}

	if (old) {
		*oldpp = old->next;

		if (old->state == RTE_STATE_READY)
			reset_input(FD(old));

		codec_delete(old);
    
		fx->num_codecs--;
	}

	if (fd) {
		fd->codec.next = fx->codecs;
		fx->codecs = &fd->codec;
		fx->num_codecs++;
	}

	return &fd->codec;
}

static rte_codec_info *
codec_enum(rte_context *context, int index)
{
	ffmpeg_context_class *fxc = FXC(context->class);
	int i;

	if (index < 0)
		return NULL;

	for (i = 0;; i++)
		if (fxc->codecs[i] == NULL)
			return NULL;
		else if (i >= index)
			break;

	return &fxc->codecs[i]->rte.public;
}

extern AVFormat wav_format;

static ffmpeg_context_class
ffmpeg_riff_wave_context = {
	.rte.public = {
		.keyword	= "ffmpeg_riff_wave",
		.label		= N_("RIFF-WAVE Audio"),
		.min_elementary	= { 0, 0, 1 },
		.max_elementary	= { 0, 0, 1 },
		.flags		= RTE_FLAG_SEEKS,
	},
	.av = &wav_format,
	.codecs = {
//		&mp2_codec,
//		&mp3lame_codec,
//		&ac3_codec,
		&pcm_s16le_codec,
		&pcm_u8_codec,
		&pcm_alaw_codec,
		&pcm_mulaw_codec,
		NULL
	}
};

static ffmpeg_context_class *
context_table[] = {
	&ffmpeg_riff_wave_context,
};

static const int num_contexts =
        sizeof(context_table) / sizeof(context_table[0]);

static void
context_delete(rte_context *context)
{
	ffmpeg_context *fx = FX(context);

	switch (fx->context.state) {
	case RTE_STATE_RUNNING:
	case RTE_STATE_PAUSED:
		assert(!"reached");

	case RTE_STATE_READY:
		reset_output(fx);
		break;

	default:
		break;
	}

	/* Delete codecs */

	while (fx->codecs) {
		rte_codec *codec = fx->codecs;

		codec_set(&fx->context, NULL,
			  codec->class->public.stream_type,
			  codec->stream_index);
	}

	pthread_mutex_destroy(&fx->context.mutex);

	free(fx);
}

static rte_context *
context_new(rte_context_class *xc, char **errstr)
{
	ffmpeg_context *fx;
	rte_context *context;

	if (!(fx = calloc(1, sizeof(*fx)))) {
		rte_asprintf(errstr, _("Out of memory."));
		return NULL;
	}

	context = &fx->context;

	context->class = xc;

	pthread_mutex_init(&context->mutex, NULL);

	context->state = RTE_STATE_NEW;

	return context;
}

/* Backend initialization */

static void
backend_init(void)
{
	int i;

	for (i = 0; i < num_contexts; i++) {
		ffmpeg_context_class *fxc = context_table[i];
		rte_context_class *xc = &fxc->rte;

		xc->public.backend = "ffmpeg 0.4.6";

		xc->public.mime_type = fxc->av->mime_type;
		xc->public.extension = fxc->av->extensions;
		
		xc->new = context_new;
		xc->delete = context_delete;

		xc->codec_enum = codec_enum;
		xc->codec_get = codec_get;
		xc->codec_set = codec_set;

		xc->codec_option_set = option_set;
		xc->codec_option_get = option_get;
		xc->codec_option_print = option_print;
		xc->codec_option_enum = option_enum;

		xc->parameters_set = parameters_set;

		xc->set_input = set_input;
		xc->set_output = set_output;

		xc->start = start;
		xc->stop = stop;

		xc->status = status;
	}
}

static rte_context_class *
context_enum(int index, char **errstr)
{
	if (index < 0 || index >= num_contexts)
		return NULL;

	return &context_table[index]->rte;
}

const rte_backend_class
rte_backend_ffmpeg = {
	.name		= "ffmpeg (prototype)",
	.backend_init	= backend_init,
	.context_enum	= context_enum,
};
