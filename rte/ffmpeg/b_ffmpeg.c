/*
 *  Real Time Encoder lib
 *  ffmpeg backend
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

/* $Id: b_ffmpeg.c,v 1.11 2002-09-26 20:43:14 mschimek Exp $ */

#include <limits.h>
#include "b_ffmpeg.h"

/* Dummies */

/* libav/util.c */
URLProtocol udp_protocol;
URLProtocol http_protocol;

static void
status				(rte_context *		context,
				 rte_codec *		codec,
				 rte_status *		status,
				 unsigned int		size)
{
	status->valid = 0;
}

/* Start / Stop */

/* forward */ static void reset_input (ffmpeg_codec *);
/* forward */ static void reset_output (ffmpeg_context *);

static void
do_write			(void *			opaque,
				 UINT8 *		buf,
				 int			buf_size)
{
	ffmpeg_context *fx = opaque;
	rte_buffer wb;

	wb.data = buf;
	wb.size = buf_size;

	if (!fx->write_cb(&fx->context, NULL, &wb)) {
		/* XXX what now? */
		exit(0);
	}
}

static int
do_seek				(void *			opaque,
				 offset_t		offset,
				 int			whence)
{
	ffmpeg_context *fx = opaque;

	/* XXX error */
	fx->seek_cb(&fx->context, offset, whence);

	return 0; // XXX
}

#define MAX_AUDIO_PACKET_SIZE 16384
#define MAX_VIDEO_PACKET_SIZE (1024 * 1024)

static inline void
do_audio_out			(ffmpeg_context *	fx,
				 ffmpeg_codec *		fd,
				 unsigned char *	buf,
				 int			size)
{
	AVCodecContext *enc = &fd->str.codec;
	static const int force_pts = 0;
	int coded_size;

	if (enc->frame_size > 1) {
		coded_size = avcodec_encode_audio (enc, fd->packet_buffer,
						   size, (short *) buf);
		fx->av.format->write_packet (&fx->av, fd->stream_index, fd->packet_buffer,
					     coded_size, force_pts);
	} else {
		switch(enc->codec->id) {
		case CODEC_ID_PCM_S16LE:
		case CODEC_ID_PCM_S16BE:
		case CODEC_ID_PCM_U16LE:
		case CODEC_ID_PCM_U16BE:
			break;

		default:
			size >>= 1;
			break;
		}

		coded_size = avcodec_encode_audio (enc, fd->packet_buffer,
						   size, (short *) buf);
		fx->av.format->write_packet (&fx->av, fd->stream_index, fd->packet_buffer,
					     coded_size, force_pts);
	}
}

static void
do_video_out			(ffmpeg_context *	fx,
				 ffmpeg_codec *		fd,
				 AVPicture *		picture)
{
	AVCodecContext *enc = &fd->str.codec;
	AVPicture temp, *pict;
	static const int force_pts = 0;
	int coded_size;

	if (fd->input_pix_fmt != enc->pix_fmt) {
		avpicture_fill (&temp, fd->temp_picture,
				enc->pix_fmt, enc->width, enc->height);

		if (img_convert (&temp, enc->pix_fmt,
				 picture, fd->input_pix_fmt,
				 enc->width, enc->height) < 0)
			assert (!"pixel format conversion not handled");

		pict = &temp;
	} else {
		pict = picture;
	}

	if (enc->codec_id == CODEC_ID_RAWVIDEO) {
		assert (0);
		/* write_picture (s, ost->index, pict, enc->pix_fmt, enc->width, enc->height); */
	} else {
		coded_size = avcodec_encode_video (enc, fd->packet_buffer, 0, pict);
		fx->av.format->write_packet (&fx->av, fd->stream_index, fd->packet_buffer,
					     coded_size, force_pts);
	}
}

static void *
mainloop			(void *			p)
{
	ffmpeg_context *fx = p;
	ffmpeg_context_class *fxc = FXC (fx->context._class);
	ffmpeg_codec *fd;
	ffmpeg_codec_class *fdc;
	rte_codec *codec;

	memset (&fx->av, 0, sizeof (fx->av));

	init_put_byte (&fx->av.pb, fx->buf,
		       sizeof (fx->buf), /* write_flag */ TRUE,
		       fx, NULL, do_write, do_seek);

	fx->av.format = fxc->av;
	fx->av.nb_streams = 0;

	for (codec = fx->codecs; codec; codec = codec->next) {
		fd = FD (codec);
		fdc = FDC (fd->codec._class);

		fx->av.streams[fx->av.nb_streams] = &fd->str;

		fd->stream_index = fx->av.nb_streams++;

		fd->pts = 0;
		fd->eof = 0;
		fd->pts_increment = 0;
		fd->frame_number = 0;
		fd->sample_index = 0;

		fd->str.codec.codec_id = fdc->av->id;

		switch (fdc->rte._public.stream_type) {
		case RTE_STREAM_AUDIO:
			fd->str.codec.codec_type = CODEC_TYPE_AUDIO;
			ticker_init (&fd->pts_ticker,
				     (INT64) fd->str.codec.sample_rate,
				     (INT64) (1000000));
			break;

		case RTE_STREAM_VIDEO:
			fd->str.codec.codec_type = CODEC_TYPE_VIDEO;
			ticker_init (&fd->pts_ticker,
				     (INT64) fd->str.codec.frame_rate,
				     ((INT64) 1000000 * FRAME_RATE_BASE));
			break;

		default:
			assert (!"reached");
		}
	}

	/* Header */

	if (fxc->av->write_header (&fx->av) < 0) {
		/* XXX what now? */
		exit (1);
	}

	/* Body */

	for (;;) {
		rte_buffer rb;
		ffmpeg_codec *min_fd = NULL;
		int64_t min_pts = INT64_MAX;

		pthread_mutex_lock (&fx->context.mutex);

		/* find input stream with lowest PTS */

		for (codec = fx->codecs; codec; codec = codec->next) {
			fd = FD (codec);

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

		fdc = FDC (fd->codec._class);

		/* fetch data */

		rb.data = NULL;
		rb.size = 0;

		if (fd->read_cb (fd->codec.context, &fd->codec, &rb)) {
			assert (rb.data != NULL && rb.size > 0);
		} else {
			fd->eof = TRUE;
			pthread_mutex_unlock (&fx->context.mutex);
			continue;
		}

		/* increment PTS */

		switch (fdc->rte._public.stream_type) {
		case RTE_STREAM_AUDIO:
			fd->pts = ticker_tick (&fd->pts_ticker, fd->sample_index);
			fd->sample_index += rb.size / (2 * fd->str.codec.channels);
			fd->pts_increment = (INT64) (rb.size / (2 * fd->str.codec.channels))
				* 1000000 / fd->str.codec.sample_rate;
			break;

		case RTE_STREAM_VIDEO:
			fd->frame_number++;
			fd->pts = ticker_tick (&fd->pts_ticker, fd->frame_number);
			fd->pts_increment = ((INT64) 1000000 * FRAME_RATE_BASE)
				/ fd->str.codec.frame_rate;
			break;

		default:
			assert (!"reached");
		}

		pthread_mutex_unlock (&fx->context.mutex);

		/* encode packet */

		switch (fdc->rte._public.stream_type) {
		case RTE_STREAM_AUDIO:
			do_audio_out (fx, fd, rb.data, rb.size);
			break;

		case RTE_STREAM_VIDEO:
		{
			AVPicture pict;

			switch (fd->codec.params.video.pixfmt) {
			case RTE_PIXFMT_YUV420:
				pict.data[0] = rb.data + fd->codec.params.video.offset;
				pict.data[1] = rb.data + fd->codec.params.video.u_offset;
				pict.data[2] = rb.data + fd->codec.params.video.v_offset;
				pict.linesize[0] = fd->codec.params.video.stride;
				pict.linesize[1] = fd->codec.params.video.uv_stride;
				pict.linesize[2] = fd->codec.params.video.uv_stride;
				break;

			case RTE_PIXFMT_YUYV:
				pict.data[0] = rb.data + fd->codec.params.video.offset;
				pict.data[1] = 0;
				pict.data[2] = 0;
				pict.linesize[0] = fd->codec.params.video.stride;
				pict.linesize[1] = 0;
				pict.linesize[2] = 0;
				break;

			default:
				assert (!"reached");
			}

			do_video_out (fx, fd, &pict);

			break;
		}

		default:
			assert (!"reached");
		}

		/* unref data */

		if (fd->unref_cb)
			fd->unref_cb (fd->codec.context, &fd->codec, &rb);
	}

	pthread_mutex_unlock (&fx->context.mutex);

	/* Trailer */

        fxc->av->write_trailer (&fx->av);

	/* EOF */

	fx->write_cb (&fx->context, NULL, NULL);

	return NULL;
}

static rte_bool
stop				(rte_context *		context,
				 double			timestamp)
{
	ffmpeg_context *fx = FX (context);
	ffmpeg_context_class *fxc = FXC (fx->context._class);
	ffmpeg_codec *fd, *max_fd = NULL;
	int64_t max_pts = 0;
	rte_codec *codec;

	if (context->state != RTE_STATE_RUNNING) {
		rte_error_printf (&fx->context, "Context %s not running.",
				  fxc->rte._public.keyword);
		return FALSE;
	}

	pthread_mutex_lock (&fx->context.mutex);

	/* find input stream with highest PTS */

	for (codec = fx->codecs; codec; codec = codec->next) {
		fd = FD (codec);

		if (fd->pts > max_pts && !fd->eof) {
			max_pts = fd->pts;
			max_fd = fd;
		}
	}

	if (max_fd) {
		fx->stop_pts = max_pts;
	} /* else done already */

	pthread_mutex_unlock (&fx->context.mutex);

	/* XXX timeout && force */
	pthread_join (fx->thread_id, NULL);

	for (codec = fx->codecs; codec; codec = codec->next) {
		static rte_bool parameters_set (rte_codec *, rte_stream_parameters *);
		ffmpeg_codec *fd = FD (codec);

		fd->codec.state = RTE_STATE_READY;

		/* Destroy input, reset codec, -> RTE_STATE_PARAM */

		reset_input (fd);

		parameters_set (&fd->codec, &fd->codec.params);
	}

	reset_output (fx);

	fx->context.state = RTE_STATE_NEW;

	return TRUE;
}

static rte_bool
start				(rte_context *		context,
				 double			timestamp,
				 rte_codec *		time_ref,
				 rte_bool		async)
{
	ffmpeg_context *fx = FX (context);
	ffmpeg_context_class *fxc = FXC (fx->context._class);
	rte_codec *codec;
	int error;

	switch (fx->context.state) {
	case RTE_STATE_READY:
		break;

	case RTE_STATE_RUNNING:
		rte_error_printf (&fx->context, "Context %s already running.",
				  fxc->rte._public.keyword);
		return FALSE;

	default:
		rte_error_printf (&fx->context, "Cannot start context %s, "
				  "initialization unfinished.",
				  fxc->rte._public.keyword);
		return FALSE;
	}

	for (codec = fx->codecs; codec; codec = codec->next)
		if (codec->state != RTE_STATE_READY) {
			rte_error_printf (&fx->context, "Cannot start context %s, initialization "
					  "of codec %s unfinished.",
					  fxc->rte._public.keyword,
					  codec->_class->_public.keyword);
			return FALSE;
		}

	fx->context.state = RTE_STATE_RUNNING;

	for (codec = fx->codecs; codec; codec = codec->next)
		codec->state = RTE_STATE_RUNNING;

	fx->stop_pts = INT64_MAX;

	error = pthread_create (&fx->thread_id, NULL, mainloop, fx);

	if (error != 0) {
		for (codec = fx->codecs; codec; codec = codec->next)
			codec->state = RTE_STATE_READY;

		fx->context.state = RTE_STATE_READY;

		rte_error_printf (&fx->context, _("Insufficient resources to start "
						  "encoding thread.\n"));
		return FALSE;
	}

        return TRUE;
}

/* Input / Output */

static void
reset_output			(ffmpeg_context *	fx)
{
	fx->context.state = RTE_STATE_NEW;
}

static rte_bool
set_output			(rte_context *		context,
				 rte_buffer_callback	write_cb,
				 rte_seek_callback	seek_cb)
{
	ffmpeg_context *fx = FX (context);
	ffmpeg_context_class *fxc = FXC (fx->context._class);
	int i;

	switch (fx->context.state) {
	case RTE_STATE_NEW:
		break;

	case RTE_STATE_READY:
		reset_output (fx);
		break;

	default:
		rte_error_printf (&fx->context, "Cannot change %s output, context is busy.",
				  fxc->rte._public.keyword);
		break;
	}

	if (!fx->codecs) {
		rte_error_printf (context, "No codec allocated for context %s.",
				  fxc->rte._public.keyword);
		return FALSE;
	}

	for (i = 0; i <= RTE_STREAM_MAX; i++) {
		rte_codec *codec;
		int count = 0;

		for (codec = fx->codecs; codec; codec = codec->next) {
			rte_codec_class *dc = codec->_class;

			if (dc->_public.stream_type != i)
				continue;

			if (codec->state != RTE_STATE_READY) {
				rte_error_printf (&fx->context, "Codec %s, elementary stream #%d, "
						  "initialization unfinished.",
						  dc->_public.keyword, codec->stream_index);
				return FALSE;
			}

			count++;
		}

		if (count < fxc->rte._public.min_elementary[i]) {
			rte_error_printf (&fx->context, "Not enough elementary streams of rte stream type %d "
					  "for context %s. %d required, %d allocated.",
					  fxc->rte._public.keyword,
					  fxc->rte._public.min_elementary[i], count);
			return FALSE;
		}
	}

	fx->write_cb = write_cb;
	fx->seek_cb = seek_cb;

	fx->context.state = RTE_STATE_READY;

	return TRUE;
}

static void
reset_input			(ffmpeg_codec *		fd)
{
	fd->codec.state = RTE_STATE_PARAM;
}

static rte_bool
set_input			(rte_codec *		codec,
				 rte_io_method		input_method,
				 rte_buffer_callback	read_cb,
				 rte_buffer_callback	unref_cb,
				 unsigned int *		queue_length)
{
	ffmpeg_codec *fd = FD (codec);
	ffmpeg_codec_class *fdc = FDC (fd->codec._class);
	rte_context *context = fd->codec.context;

	switch (fd->codec.state) {
	case RTE_STATE_NEW:
		rte_error_printf (context, "Attempt to select input method with "
					   "uninitialized sample parameters.");
		return FALSE;

	case RTE_STATE_PARAM:
		break;

	case RTE_STATE_READY:
		reset_input (fd);
		break;

	default:
		rte_error_printf (context, "Cannot change %s input, codec is busy.",
				  fdc->rte._public.keyword);
		break;
	}

	switch (input_method) {
	case RTE_CALLBACK_MASTER:
		*queue_length = 1;
		break;

	case RTE_CALLBACK_SLAVE:
	case RTE_PUSH_MASTER:
	case RTE_PUSH_SLAVE:
		rte_error_printf (context, "Selected input method not supported yet.");
		return FALSE;

	default:
		assert (!"rte bug");
	}

	fd->input_method = input_method;
	fd->read_cb = read_cb;
	fd->unref_cb = unref_cb;

	fd->codec.state = RTE_STATE_READY;

	return TRUE;
}

static inline int
saturate			(int			val,
				 int			min,
				 int			max)
{
	if (val < min)
		val = min;
	else if (val > max)
		val = max;

	return val;
}

static rte_bool
realloc_buffer			(rte_context *		context,
				 void **		pp,
				 unsigned int		size)
{
	assert (pp != NULL);

	if (*pp)
		free (*pp);
	*pp = 0;

	if (size == 0)
		return TRUE;

	if ((*pp = malloc (size)) != NULL)
		return TRUE;

	rte_error_printf (context, _("Out of memory."));

	return FALSE;
}

/* Sampling parameters */

static rte_bool
parameters_set			(rte_codec *		codec,
				 rte_stream_parameters *rsp)
{
	static const double aspects[] = { 1, 54/59.0, 11/10.0, 81/118.0, 33/40.0 };
	static const int aspect_type[] = { FF_ASPECT_SQUARE, FF_ASPECT_4_3_625, FF_ASPECT_4_3_525,
					   FF_ASPECT_16_9_625, FF_ASPECT_16_9_525 };
	static const int16_t x = 1;
	ffmpeg_codec *fd = FD (codec);
	ffmpeg_codec_class *fdc = FDC (fd->codec._class);
	struct AVCodecContext *avcc = &fd->str.codec;

	switch (fdc->rte._public.stream_type) {
	case RTE_STREAM_AUDIO:
		rsp->audio.sndfmt = (((int8_t *) &x)[0] == 1) ?
			RTE_SNDFMT_S16_LE : RTE_SNDFMT_S16_BE; /* machine endian */

		rsp->audio.sampling_freq = avcc->sample_rate;
		rsp->audio.channels = avcc->channels;

		if (avcodec_open (avcc, fdc->av) < 0)
			goto failed1;

		if (avcc->frame_size == 1)
			rsp->audio.fragment_size = 4096 * 2 * avcc->channels;
		else
			rsp->audio.fragment_size = avcc->frame_size * 2 * avcc->channels;

		if (!realloc_buffer (fd->codec.context, &fd->packet_buffer,
				     MAX_AUDIO_PACKET_SIZE))
			goto failed2;

		break;

	case RTE_STREAM_VIDEO:
		rsp->video.framefmt = RTE_FRAMEFMT_PROGRESSIVE;
		rsp->video.spatial_order = 0;
		rsp->video.temporal_order = 0;

		avcc->frame_rate = (int)(rsp->video.frame_rate * FRAME_RATE_BASE);

		avcc->aspect_ratio_info = aspect_type[rte_closest_double
			(aspects, 5, rsp->video.sample_aspect)];

		avcc->width = rsp->video.width =
			(saturate (rsp->video.width, 16, 768) + 8) & -16;
		avcc->height = rsp->video.height =
			(saturate (rsp->video.height, 16, 576) + 8) & -16;

		rsp->video.offset = 0;

		switch (rsp->video.pixfmt) {
		default:
			rsp->video.pixfmt = RTE_PIXFMT_YUV420;
			/* fall through */

		case RTE_PIXFMT_YUV420:
			fd->input_pix_fmt =
			avcc->pix_fmt = PIX_FMT_YUV420P;
			rsp->video.u_offset = rsp->video.width * rsp->video.height;
			rsp->video.v_offset = rsp->video.u_offset >> 2;
			rsp->video.stride = rsp->video.width;
			rsp->video.uv_stride = rsp->video.width >> 1;
			break;

		case RTE_PIXFMT_YUYV:
			fd->input_pix_fmt =
			avcc->pix_fmt = PIX_FMT_YUV422;
			rsp->video.stride = rsp->video.width * 2;
			break;
		}

		if (avcc->max_b_frames + 1 > avcc->gop_size)
			avcc->max_b_frames = avcc->gop_size - 1;
		if (avcc->max_b_frames < 0)
			avcc->max_b_frames = 0;

		/* hello? */
		avcc->bit_rate_tolerance = 0;
		avcc->rtp_mode = 0;
		avcc->qmin = 1;
		avcc->qmax = 31;
		avcc->max_qdiff = 31;
		avcc->qcompress = 16;
		avcc->qblur = 16;
		avcc->b_quant_factor = 10;
		avcc->flags = 0;
		avcc->rc_strategy = 0;
		avcc->b_frame_strategy = 0;

		if (avcodec_open (avcc, fdc->av) < 0)
			goto failed1;

		if (!realloc_buffer (fd->codec.context, &fd->packet_buffer,
				     MAX_VIDEO_PACKET_SIZE))
			goto failed2;

		if (fd->input_pix_fmt != fd->str.codec.pix_fmt) {
			unsigned int size = avpicture_get_size (fd->str.codec.pix_fmt,
								fd->str.codec.width,
								fd->str.codec.height);
			assert (size > 0);

			if (!realloc_buffer (fd->codec.context, &fd->temp_picture, size))
				goto failed2;
		}

		break;

	default:
		assert (!"reached");
	}

	fd->status.valid = 0;

	/* Parameters accepted */
	memcpy (&fd->codec.params, rsp, sizeof (fd->codec.params));

	fd->codec.state = RTE_STATE_PARAM;

	return TRUE;

failed2:
	avcodec_close (&fd->str.codec);
	return FALSE;

failed1:
	rte_error_printf (fd->codec.context, _("Codec initialization failed."));
	return FALSE;
}

/* Codec options */

#define OPTION_OPEN_SAMPLING		(1 << 0)
#define OPTION_MPEG1_SAMPLING		(1 << 1)
#define OPTION_MPEG2_SAMPLING		(1 << 2)
#define OPTION_STEREO			(1 << 3)
#define OPTION_MPEG1_AUDIO_BITRATE	(1 << 4)
#define OPTION_MPEG2_AUDIO_BITRATE	(1 << 5)
#define OPTION_AC3_SAMPLING		(1 << 6)
#define OPTION_AC3_BITRATE		(1 << 7)
#define OPTION_OPEN_BITRATE		(1 << 8)
#define OPTION_MOTION_TYPE		(1 << 9)
#define OPTION_I_DIST			(1 << 10)
#define OPTION_P_DIST			(1 << 11)

/* Attention: Canonical order */
static char *
menu_audio_mode[] = {
	/* 0 */ N_("Mono"),
	/* 1 */ N_("Stereo"),
	/* NLS: Bilingual for example left audio channel English, right French */
	/* 2 */ N_("Bilingual"), 
	/* 3 TODO N_("Joint Stereo"), */
};

static int
mpeg_audio_sampling[2][4] = {
	{ 44100, 48000, 32000 },
	{ 22050, 24000,	16000 }
};

static int
ac3_audio_sampling[6] = {
	48000, 44100, 32000, 24000, 22050, 16000
};

static int
mpeg_audio_bit_rate[2][16] = {
	{ 0, 32000, 48000, 56000, 64000, 80000, 96000, 112000,
	  128000, 160000, 192000, 224000, 256000, 320000, 384000 },
	{ 0, 8000, 16000, 24000, 32000, 40000, 48000, 56000,
	  64000, 80000, 96000, 112000, 128000, 144000, 160000 }
};

static int
ac3_audio_bit_rate[24] = {
	16000, 20000, 24000, 28000, /* XXX depends */
	32000, 40000, 48000, 56000,
	64000, 80000, 96000, 112000,
	128000, 160000, 192000, 224000, 228000, /* XXX */
	256000, 320000, 384000, 448000, /* XXX */
	512000, 576000, 640000 /* XXX */
};

/* Attention: Canonical order */
static char *
menu_motion_mode[] = {
	/* NLS: Motion estimation */
	N_("Disabled (fastest, worst quality)"),
	("PHODS"),
	("LOG"),
	("X1"),
	("EPZS"),
	N_("Full search (slowest, best quality)"),
};

static const enum Motion_Est_ID
motion_type[] = {
	ME_ZERO, ME_PHODS, ME_LOG,
	ME_X1, ME_EPZS, ME_FULL,
};

static struct {
	unsigned int		opt;
	rte_option_info		info;
} options[] = {
	{ OPTION_OPEN_SAMPLING,		RTE_OPTION_INT_RANGE_INITIALIZER
	  ("sampling_freq", N_("Sampling frequency"),
	   44100, 8000, 48000, 100, NULL) },
	{ OPTION_STEREO,		RTE_OPTION_MENU_INITIALIZER
	  ("audio_mode", N_("Mode"),
	   0, menu_audio_mode, 2, NULL) },
	{ OPTION_MPEG1_AUDIO_BITRATE,	RTE_OPTION_INT_MENU_INITIALIZER
	  ("bit_rate", N_("Bit rate"),
	   7 /* 128k */, &mpeg_audio_bit_rate[0][1], 14,
	   N_("Output bit rate, all channels together")) },
	{ OPTION_MPEG2_AUDIO_BITRATE,	RTE_OPTION_INT_MENU_INITIALIZER
	  ("bit_rate", N_("Bit rate"),
	   7 /* 64k */, &mpeg_audio_bit_rate[1][1], 14,
	   N_("Output bit rate, all channels together")) },
	{ OPTION_MPEG1_SAMPLING,	RTE_OPTION_INT_MENU_INITIALIZER
	  ("sampling_freq", N_("Sampling frequency"),
	   0 /* 44100 */, &mpeg_audio_sampling[0][0], 3, NULL) },
	{ OPTION_MPEG2_SAMPLING,	RTE_OPTION_INT_MENU_INITIALIZER
	  ("sampling_freq", N_("Sampling frequency"),
	   0 /* 22050 */, &mpeg_audio_sampling[1][0], 3, NULL) },
	{ OPTION_AC3_SAMPLING,		RTE_OPTION_INT_MENU_INITIALIZER
	  ("sampling_freq", N_("Sampling frequency"),
	   1 /* 44100 */, &ac3_audio_sampling[0], 6, NULL) },
	{ OPTION_AC3_BITRATE,		RTE_OPTION_INT_MENU_INITIALIZER
	  ("bit_rate", N_("Bit rate"),
	   8 /* 128k */, ac3_audio_bit_rate, 24,
	   N_("Output bit rate, all channels together")) },
	{ OPTION_OPEN_BITRATE,		RTE_OPTION_INT_RANGE_INITIALIZER
	  ("bit_rate", N_("Bit rate"),
	   1000000, 30000, 8000000, 1000, NULL) },
	{ OPTION_MOTION_TYPE,		RTE_OPTION_MENU_INITIALIZER
	  ("motion_estimation", N_("Motion estimation"),
	   4 /* EPSZ */, menu_motion_mode, 6, NULL) },
	{ OPTION_I_DIST,		RTE_OPTION_INT_RANGE_INITIALIZER
	  ("i_dist", N_("Intra picture distance (M)"),
	   12, 0, 1024, 1, NULL) },
	{ OPTION_P_DIST,		RTE_OPTION_INT_RANGE_INITIALIZER
	  ("p_dist", N_("Predicted picture distance (N)"),
	   0, 0, 3, 1, NULL) },
};

static const int num_options = sizeof(options) / sizeof(* options);

#define KEYWORD(name) (strcmp(keyword, name) == 0)
#define KEYOPT(type, name) ((fdc->options & type) && KEYWORD(name))

#define sprintf_intvec(vec, len, pre, label)				\
	snprintf(buf, sizeof(buf), _(label),				\
		 rte_closest_int_val(vec, len, va_arg(args, int)) / pre);

static char *
option_print			(rte_codec *		codec,
				 const char *		keyword,
				 va_list		args)
{
        ffmpeg_codec *fd = FD (codec);
        ffmpeg_codec_class *fdc = FDC (fd->codec._class);
	rte_context *context = fd->codec.context;
	char buf[80];

	if (KEYOPT (OPTION_OPEN_SAMPLING, "sampling_freq")) {
		snprintf(buf, sizeof(buf), _("%u Hz"), va_arg(args, int));
	} else if (KEYWORD ("audio_mode")) {
		return rte_strdup(context, NULL, _(menu_audio_mode[
			RTE_OPTION_ARG_MENU(menu_audio_mode)]));
	} else if (KEYOPT(OPTION_MPEG1_AUDIO_BITRATE, "bit_rate")) {
		sprintf_intvec(&mpeg_audio_bit_rate[0][1], 14, 1000, "%u kbit/s");
	} else if (KEYOPT(OPTION_MPEG2_AUDIO_BITRATE, "bit_rate")) {
		sprintf_intvec(&mpeg_audio_bit_rate[1][1], 14, 1000, "%u kbit/s");
	} else if (KEYOPT(OPTION_MPEG1_SAMPLING, "sampling_freq")) {
		sprintf_intvec(mpeg_audio_sampling[0], 3, 1, "%u Hz");
	} else if (KEYOPT(OPTION_MPEG2_SAMPLING, "sampling_freq")) {
		sprintf_intvec(mpeg_audio_sampling[1], 3, 1, "%u Hz");
	} else if (KEYOPT(OPTION_AC3_SAMPLING, "sampling_freq")) {
		sprintf_intvec(ac3_audio_sampling, 6, 1, "%u Hz");
	} else if (KEYOPT(OPTION_AC3_BITRATE, "bit_rate")) {
		sprintf_intvec(ac3_audio_bit_rate, 24, 1000, "%u kbit/s");
	} else if (KEYOPT(OPTION_OPEN_BITRATE, "bit_rate")) {
	        snprintf(buf, sizeof(buf), _("%5.3f Mbit/s"), va_arg(args, int) / 1e6);
	} else if (KEYWORD("motion_estimation")) {
		return rte_strdup(context, NULL, _(menu_motion_mode[
			RTE_OPTION_ARG_MENU(menu_motion_mode)]));
	} else if (KEYOPT(OPTION_I_DIST, "i_dist")) {
		snprintf(buf, sizeof(buf), "%u", va_arg(args, int));
	} else if (KEYOPT(OPTION_P_DIST, "p_dist")) {
		snprintf(buf, sizeof(buf), "%u", va_arg(args, int));
	} else {
		rte_unknown_option(context, codec, keyword);
	failed:
		return NULL;
	}

	return rte_strdup(context, NULL, buf);
}

static rte_bool
option_get			(rte_codec *		codec,
				 const char *		keyword,
				 rte_option_value *	v)
{
        ffmpeg_codec *fd = FD(codec);
        ffmpeg_codec_class *fdc = FDC(fd->codec._class);
	rte_context *context = fd->codec.context;

	if ((fdc->options & (OPTION_OPEN_SAMPLING |
			     OPTION_MPEG1_SAMPLING |
			     OPTION_MPEG2_SAMPLING |
			     OPTION_AC3_SAMPLING))
	    && KEYWORD("sampling_freq")) {
		v->num = fd->str.codec.sample_rate;
	} else if (KEYOPT(OPTION_STEREO, "audio_mode")) {
		v->num = fd->str.codec.channels - 1;
	} else if ((fdc->options & (OPTION_MPEG1_AUDIO_BITRATE |
				    OPTION_MPEG2_AUDIO_BITRATE |
				    OPTION_AC3_BITRATE |
				    OPTION_OPEN_BITRATE))) {
		v->num = fd->str.codec.bit_rate;
	} else if (KEYOPT(OPTION_MOTION_TYPE, "motion_estimation")) {
		unsigned int i;

		for (i = 0; i < 6; i++)
			if (motion_type[i] == fd->str.codec.me_method)
				v->num = i;
		assert (i < 6);
	} else if (KEYOPT(OPTION_I_DIST, "i_dist")) {
		v->num = fd->str.codec.gop_size;
	} else if (KEYOPT(OPTION_P_DIST, "p_dist")) {
		v->num = fd->str.codec.max_b_frames;
	} else {
		rte_unknown_option(context, codec, keyword);
		return FALSE;
	}

        return TRUE;
}

static rte_bool
option_set			(rte_codec *		codec,
				 const char *		keyword,
				 va_list		args)
{
	ffmpeg_codec *fd = FD(codec);
	ffmpeg_codec_class *fdc = FDC(fd->codec._class);
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
	} else if (KEYOPT(OPTION_MPEG1_AUDIO_BITRATE, "bit_rate")) {
		fd->str.codec.bit_rate = 
			rte_closest_int_val(&mpeg_audio_bit_rate[0][1], 14, va_arg(args, int));
	} else if (KEYOPT(OPTION_MPEG2_AUDIO_BITRATE, "bit_rate")) {
		fd->str.codec.bit_rate =
			rte_closest_int_val(&mpeg_audio_bit_rate[1][1], 14, va_arg(args, int));
	} else if (KEYOPT(OPTION_MPEG1_SAMPLING, "sampling_freq")) {
		fd->str.codec.sample_rate = 
			rte_closest_int_val(mpeg_audio_sampling[0], 3, va_arg(args, int));
	} else if (KEYOPT(OPTION_MPEG2_SAMPLING, "sampling_freq")) {
		fd->str.codec.sample_rate = 
			rte_closest_int_val(mpeg_audio_sampling[1], 3, va_arg(args, int));
	} else if (KEYOPT(OPTION_AC3_SAMPLING, "sampling_freq")) {
		fd->str.codec.sample_rate = 
			rte_closest_int_val(ac3_audio_sampling, 6, va_arg(args, int));
	} else if (KEYOPT(OPTION_AC3_BITRATE, "bit_rate")) {
		fd->str.codec.bit_rate = 
			rte_closest_int_val(ac3_audio_bit_rate, 24, va_arg(args, int));
	} else if (KEYOPT(OPTION_OPEN_BITRATE, "bit_rate")) {
		fd->str.codec.bit_rate = RTE_OPTION_ARG(int, 30000, 8000000);
	} else if (KEYOPT(OPTION_MOTION_TYPE, "motion_estimation")) {
		fd->str.codec.me_method = motion_type[RTE_OPTION_ARG(int, 0, 5)];
	} else if (KEYOPT(OPTION_I_DIST, "i_dist")) {
		fd->str.codec.gop_size = RTE_OPTION_ARG(int, 0, 1024);
	} else if (KEYOPT(OPTION_P_DIST, "p_dist")) {
		fd->str.codec.max_b_frames = RTE_OPTION_ARG(int, 0, 3);
	} else {
		rte_unknown_option(context, codec, keyword);
	failed:
		return FALSE;
	}

        return TRUE;
}

static rte_option_info *
option_enum			(rte_codec *		codec,
				 unsigned int		index)
{
	ffmpeg_codec *fd = FD(codec);
	ffmpeg_codec_class *fdc = FDC(fd->codec._class);
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
extern AVCodec pcm_s16be_encoder;
extern AVCodec pcm_u8_encoder;
extern AVCodec pcm_alaw_encoder;
extern AVCodec pcm_mulaw_encoder;
extern AVCodec mp2_encoder;

extern AVCodec mpeg1video_encoder;
extern AVCodec h263_encoder;
extern AVCodec h263p_encoder;
extern AVCodec rv10_encoder;
extern AVCodec mjpeg_encoder;
extern AVCodec mpeg4_encoder;
extern AVCodec msmpeg4v1_encoder;
extern AVCodec msmpeg4v2_encoder;
extern AVCodec msmpeg4v3_encoder;

ffmpeg_codec_class
pcm_s16le_codec = {
	.av 		= &pcm_s16le_encoder,
	.options	= OPTION_OPEN_SAMPLING |
			  OPTION_STEREO,
        .rte._public = {
                .stream_type    = RTE_STREAM_AUDIO,
                .keyword        = "pcm_s16le",
                .label          = N_("PCM 16 Bit Signed Little Endian"),
        },
};

ffmpeg_codec_class
pcm_s16be_codec = {
	.av 		= &pcm_s16be_encoder,
	.options	= OPTION_OPEN_SAMPLING |
			  OPTION_STEREO,
        .rte._public = {
                .stream_type    = RTE_STREAM_AUDIO,
                .keyword        = "pcm_s16be",
                .label          = N_("PCM 16 Bit Signed Big Endian"),
        },
};

ffmpeg_codec_class
pcm_u8_codec = {
	.av		= &pcm_u8_encoder,
	.options	= OPTION_OPEN_SAMPLING | 
			  OPTION_STEREO,
        .rte._public = {
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
        .rte._public = {
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
        .rte._public = {
                .stream_type    = RTE_STREAM_AUDIO,
                .keyword        = "pcm_mulaw",
                .label          = N_("PCM mu-Law"),
	},
};

ffmpeg_codec_class
mpeg1_mp2_codec = {
	.av		= &mp2_encoder,
	.options	= OPTION_MPEG1_AUDIO_BITRATE |
			  OPTION_MPEG1_SAMPLING |
			  OPTION_STEREO,
        .rte._public = {
                .stream_type    = RTE_STREAM_AUDIO,
                .keyword        = "mpeg1_audio_layer2",
                .label          = N_("MPEG-1 Audio Layer II"),
	},
};

ffmpeg_codec_class
mpeg2_mp2_codec = {
	.av		= &mp2_encoder,
	.options	= OPTION_MPEG2_AUDIO_BITRATE |
			  OPTION_MPEG2_SAMPLING |
			  OPTION_STEREO,
        .rte._public = {
                .stream_type    = RTE_STREAM_AUDIO,
                .keyword        = "mpeg2_audio_layer2",
                .label          = N_("MPEG-2 Audio Layer II LSF"),
		.tooltip	= N_("MPEG-2 Low Sampling Frequency extension to MPEG-1 "
				     "Audio Layer II. Be warned not all MPEG video and "
				     "audio players support MPEG-2 audio."),
	},
};

ffmpeg_codec_class
ac3_codec = {
	.av		= &ac3_encoder,
	.options	= OPTION_AC3_BITRATE |
			  OPTION_AC3_SAMPLING |
			  OPTION_STEREO,
        .rte._public = {
                .stream_type    = RTE_STREAM_AUDIO,
                .keyword        = "ac3_audio",
                .label          = N_("AC3 Audio"),
	},
};

ffmpeg_codec_class
mpeg1_codec = {
	.av		= &mpeg1video_encoder,
	.options	= OPTION_OPEN_BITRATE |
			  OPTION_MOTION_TYPE |
			  OPTION_I_DIST |
			  OPTION_P_DIST,
        .rte._public = {
                .stream_type    = RTE_STREAM_VIDEO,
                .keyword        = "mpeg1_video",
                .label          = N_("MPEG-1 Video"),
	},
};

ffmpeg_codec_class
h263_codec = {
	.av		= &h263_encoder,
	.options	= OPTION_OPEN_BITRATE |
			  OPTION_MOTION_TYPE |
			  OPTION_I_DIST |
			  OPTION_P_DIST,
        .rte._public = {
                .stream_type    = RTE_STREAM_VIDEO,
                .keyword        = "h263_video",
                .label          = N_("H.263 Video"),
	},
};

ffmpeg_codec_class
h263p_codec = {
	.av		= &h263p_encoder,
	.options	= OPTION_OPEN_BITRATE |
			  OPTION_MOTION_TYPE |
			  OPTION_I_DIST |
			  OPTION_P_DIST,
        .rte._public = {
                .stream_type    = RTE_STREAM_VIDEO,
                .keyword        = "h263p_video",
                .label          = N_("H.263+ Video"),
	},
};

ffmpeg_codec_class
rv10_codec = {
	.av		= &rv10_encoder,
	.options	= OPTION_OPEN_BITRATE |
			  OPTION_MOTION_TYPE |
			  OPTION_I_DIST |
			  OPTION_P_DIST,
        .rte._public = {
                .stream_type    = RTE_STREAM_VIDEO,
                .keyword        = "rv10_video",
                .label          = N_("RealVideo 1.0"),
	},
};

ffmpeg_codec_class
mjpeg_codec = {
	.av		= &mjpeg_encoder,
	.options	= OPTION_OPEN_BITRATE,
        .rte._public = {
                .stream_type    = RTE_STREAM_VIDEO,
                .keyword        = "mjpeg_video",
                .label          = N_("Motion JPEG"),
	},
};

ffmpeg_codec_class
mpeg4_codec = {
	.av		= &mpeg4_encoder,
	.options	= OPTION_OPEN_BITRATE |
			  OPTION_MOTION_TYPE |
			  OPTION_I_DIST |
			  OPTION_P_DIST,
        .rte._public = {
                .stream_type    = RTE_STREAM_VIDEO,
                .keyword        = "mpeg4_video",
                .label          = N_("MPEG-4 Video"),
	},
};

ffmpeg_codec_class
msmpeg4v1_codec = {
	.av		= &msmpeg4v1_encoder,
	.options	= OPTION_OPEN_BITRATE |
			  OPTION_MOTION_TYPE |
			  OPTION_I_DIST |
			  OPTION_P_DIST,
        .rte._public = {
                .stream_type    = RTE_STREAM_VIDEO,
                .keyword        = "msmpeg4v1_video",
                .label          = N_("MS MPEG-4 V1 Video"),
	},
};

ffmpeg_codec_class
msmpeg4v2_codec = {
	.av		= &msmpeg4v2_encoder,
	.options	= OPTION_OPEN_BITRATE |
			  OPTION_MOTION_TYPE |
			  OPTION_I_DIST |
			  OPTION_P_DIST,
        .rte._public = {
                .stream_type    = RTE_STREAM_VIDEO,
                .keyword        = "msmpeg4v2_video",
                .label          = N_("MS MPEG-4 V2 Video"),
	},
};

ffmpeg_codec_class
msmpeg4v3_codec = {
	.av		= &msmpeg4v3_encoder,
	.options	= OPTION_OPEN_BITRATE |
			  OPTION_MOTION_TYPE |
			  OPTION_I_DIST |
			  OPTION_P_DIST,
        .rte._public = {
                .stream_type    = RTE_STREAM_VIDEO,
                .keyword        = "msmpeg4v3_video",
                .label          = N_("MS MPEG-4 V3 (DivX;-) Video"),
	},
};

static void
codec_delete			(rte_codec *		codec)
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

	realloc_buffer (NULL, &fd->temp_picture, 0);
	realloc_buffer (NULL, &fd->packet_buffer, 0);

        pthread_mutex_destroy(&fd->codec.mutex);

        free(fd);
}

static rte_codec *
codec_new			(rte_codec_class *		cc,
				 char **			errstr)
{
        ffmpeg_codec *fd;

	if (!(fd = calloc(sizeof(*fd), 1))) {
		rte_asprintf(errstr, _("Out of memory."));
		return NULL;
	}

        fd->codec._class = cc;

        pthread_mutex_init(&fd->codec.mutex, NULL);

	memset(&fd->str.codec, 0, sizeof(fd->str.codec));

        fd->codec.state = RTE_STATE_NEW;

        return &fd->codec;
}

static rte_codec *
codec_get			(rte_context *		context,
				 rte_stream_type	stream_type,
				 unsigned int		stream_index)
{
	ffmpeg_context *fx = FX(context);
	rte_codec *codec;

	for (codec = fx->codecs; codec; codec = codec->next)
		if (codec->_class->_public.stream_type == stream_type
		    && codec->stream_index == stream_index)
			return codec;

	return NULL;
}

static rte_codec *
codec_set			(rte_context *		context,
				 const char *		keyword,
				 rte_stream_type	stream_type,
				 unsigned int		stream_index)
{
	ffmpeg_context *fx = FX(context);
	ffmpeg_context_class *fxc = FXC(fx->context._class);
	ffmpeg_codec *fd = NULL;
	ffmpeg_codec_class *fdc;
	rte_codec *old, **oldpp;
	int i;

	for (oldpp = &fx->codecs; (old = *oldpp); oldpp = &old->next) {
		fdc = FDC(old->_class);

		if (keyword) {
			if (strcmp(fdc->rte._public.keyword, keyword) == 0)
				break;
		} else {
			if (fdc->rte._public.stream_type == stream_type
			    && old->stream_index == stream_index)
				break;
		}
	}

	if (keyword) {
		rte_codec *codec;
		char *error = NULL;
		int max_elem;

		for (i = 0; (fdc = fxc->codecs[i]); i++)
			if (strcmp(fdc->rte._public.keyword, keyword) == 0)
				break;

		if (!fdc) {
			rte_error_printf(&fx->context, "'%s' is no codec of the %s encoder.",
					 keyword, fxc->rte._public.keyword);
			return NULL;
		}

		stream_type = fdc->rte._public.stream_type;
		max_elem = fxc->rte._public.max_elementary[stream_type];

		assert(max_elem > 0);

		if (stream_index >= max_elem) {
			rte_error_printf(&fx->context, "'%s' selected for "
					 "elementary stream %d of %s encoder, "
					 "but only %d available.",
					 fdc->rte._public.keyword, stream_index,
					 fxc->rte._public.keyword, max_elem);
			return NULL;
		}

		if (!(codec = codec_new(&fdc->rte, &error))) {
			if (error) {
				rte_error_printf(&fx->context,
						 _("Cannot create new codec instance '%s'. %s"),
						 fdc->rte._public.keyword, error);
				free(error);
			} else {
				rte_error_printf(&fx->context,
						 _("Cannot create new codec instance '%s'."),
						 fdc->rte._public.keyword);
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
codec_enum			(rte_context *		context,
				 unsigned int		index)
{
	ffmpeg_context_class *fxc = FXC(context->_class);
	int i;

	if (index < 0)
		return NULL;

	for (i = 0;; i++)
		if (fxc->codecs[i] == NULL)
			return NULL;
		else if (i >= index)
			break;

	return &fxc->codecs[i]->rte._public;
}

extern AVFormat wav_format;
extern AVFormat mp2_format;
extern AVFormat ac3_format;
extern AVFormat au_format;
extern AVFormat mpeg1video_format;	/* mpeg video elementary */
extern AVFormat mpeg_mux_format;	/* mpeg-1 ps */
extern AVFormat rm_format;
extern AVFormat asf_format;
extern AVFormat swf_format;
extern AVFormat avi_format;

static ffmpeg_context_class
ffmpeg_riff_wave_context = {
	.rte._public = {
		.keyword	= "ffmpeg_riff_wave",
		.label		= N_("RIFF-WAVE Audio"),
		.min_elementary	= { 0, 0, 1 },
		.max_elementary	= { 0, 0, 1 },
		.flags		= RTE_FLAG_SEEKS,
	},
	.av = &wav_format,
	.codecs = {
		&mpeg1_mp2_codec,
		&mpeg2_mp2_codec,
	     /* &mp3lame_codec, */
		&ac3_codec,
		&pcm_s16le_codec,
		&pcm_u8_codec,
		&pcm_alaw_codec,
		&pcm_mulaw_codec,
		NULL
	}
};

static ffmpeg_context_class
ffmpeg_mpeg_audio_context = {
	.rte._public = {
		.keyword	= "ffmpeg_mpeg_audio",
		.label		= N_("MPEG Audio Elementary Stream"),
		.min_elementary	= { 0, 0, 1 },
		.max_elementary	= { 0, 0, 1 },
	},
	.av = &mp2_format,
	.codecs = {
		&mpeg1_mp2_codec,
		&mpeg2_mp2_codec,
	     /* &mp3lame_codec, */
		NULL
	}
};

static ffmpeg_context_class
ffmpeg_ac3_audio_context = {
	.rte._public = {
		.keyword	= "ffmpeg_ac3_audio",
		.label		= N_("Dolby AC3 Audio Elementary Stream"),
		.min_elementary	= { 0, 0, 1 },
		.max_elementary	= { 0, 0, 1 },
	},
	.av = &ac3_format,
	.codecs = {
		&ac3_codec,
		NULL
	}
};

static ffmpeg_context_class
ffmpeg_au_audio_context = {
	.rte._public = {
		.keyword	= "ffmpeg_au_audio",
		.label		= N_("Sun AU Audio File"),
		.min_elementary	= { 0, 0, 1 },
		.max_elementary	= { 0, 0, 1 },
		.flags		= RTE_FLAG_SEEKS,
	},
	.av = &au_format,
	.codecs = {
		&pcm_s16be_codec,
		NULL
	}
};

static ffmpeg_context_class
ffmpeg_mpeg_video_context = {
	.rte._public = {
		.keyword	= "ffmpeg_mpeg_video",
		.label		= N_("MPEG Video Elementary Stream"),
		.min_elementary	= { 0, 1, 0 },
		.max_elementary	= { 0, 1, 0 },
	},
	.av = &mpeg1video_format,
	.codecs = {
		&mpeg1_codec,
		&h263_codec,
		&h263p_codec,
		&mpeg4_codec,
		&msmpeg4v1_codec,
		&msmpeg4v2_codec,
		&msmpeg4v3_codec,
		&mjpeg_codec,
		&rv10_codec,
		NULL
	}
};

static ffmpeg_context_class
ffmpeg_mpeg1_context = {
	.rte._public = {
		.keyword	= "ffmpeg_mpeg1_ps",
		.label		= N_("MPEG-1 Program Stream"),
		.min_elementary	= { 0, 1, 1 },
		.max_elementary	= { 0, 1, 1 },
	},
	.av = &mpeg_mux_format,
	.codecs = {
		&mpeg1_codec,
		&mpeg1_mp2_codec,
		NULL
	}
};

static ffmpeg_context_class
ffmpeg_real_context = {
	.rte._public = {
		.keyword	= "ffmpeg_real",
		.label		= N_("Real Audio/Video Stream"),
		.min_elementary	= { 0, 1, 1 },
		.max_elementary	= { 0, 1, 1 },
		.flags		= RTE_FLAG_SEEKS,
	},
	.av = &rm_format,
	.codecs = {
		&rv10_codec,
		&ac3_codec,
		NULL
	}
};

static ffmpeg_context_class
ffmpeg_asf_context = {
	.rte._public = {
		.keyword	= "ffmpeg_asf",
		.label		= N_("ASF Audio/Video Stream"),
		.min_elementary	= { 0, 1, 1 },
		.max_elementary	= { 0, 1, 1 },
		.flags		= RTE_FLAG_SEEKS,
	},
	.av = &asf_format,
	.codecs = {
		&msmpeg4v3_codec,
		&mpeg1_mp2_codec,
		NULL
	}
};

static ffmpeg_context_class
ffmpeg_swf_context = {
	.rte._public = {
		.keyword	= "ffmpeg_swf",
		.label		= N_("Shockwave Animation"),
		.min_elementary	= { 0, 1, 1 },
		.max_elementary	= { 0, 1, 1 },
		.flags		= RTE_FLAG_SEEKS,
	},
	.av = &swf_format,
	.codecs = {
		&mjpeg_codec,
		&mpeg1_mp2_codec,
		NULL
	}
};

static ffmpeg_context_class
ffmpeg_avi_context = {
	.rte._public = {
		.keyword	= "ffmpeg_avi",
		.label		= N_("AVI File"),
		.min_elementary	= { 0, 1, 1 },
		.max_elementary	= { 0, 1, 1 },
		.flags		= RTE_FLAG_SEEKS,
	},
	.av = &avi_format,
	.codecs = {
		&msmpeg4v3_codec,
		&mpeg1_mp2_codec,
		NULL
	}
};

static ffmpeg_context_class *
context_table[] = {
	&ffmpeg_mpeg1_context,
	&ffmpeg_real_context,
	&ffmpeg_asf_context,
	&ffmpeg_swf_context,
	&ffmpeg_avi_context,
	&ffmpeg_mpeg_video_context,
	&ffmpeg_riff_wave_context,
	&ffmpeg_mpeg_audio_context,
	&ffmpeg_ac3_audio_context,
	&ffmpeg_au_audio_context,
};

static const int num_contexts =
        sizeof(context_table) / sizeof(context_table[0]);

static void
context_delete			(rte_context *		context)
{
	ffmpeg_context *fx = FX (context);

	switch (fx->context.state) {
	case RTE_STATE_RUNNING:
	case RTE_STATE_PAUSED:
		assert (!"reached");

	case RTE_STATE_READY:
		reset_output (fx);
		break;

	default:
		break;
	}

	/* Delete codecs */

	while (fx->codecs) {
		rte_codec *codec = fx->codecs;

		codec_set (&fx->context,
			   NULL,
			   codec->_class->_public.stream_type,
			   codec->stream_index);
	}

	pthread_mutex_destroy (&fx->context.mutex);

	free (fx);
}

static rte_context *
context_new			(rte_context_class *	xc,
				 char **		errstr)
{
	ffmpeg_context *fx;
	rte_context *context;

	if (!(fx = calloc (1, sizeof (*fx)))) {
		rte_asprintf (errstr, _("Out of memory."));
		return NULL;
	}

	context = &fx->context;

	context->_class = xc;

	pthread_mutex_init (&context->mutex, NULL);

	context->state = RTE_STATE_NEW;

	return context;
}

static rte_context_class *
context_enum			(unsigned int		index,
				 char **		errstr)
{
	if (index < 0 || index >= num_contexts)
		return NULL;

	return &context_table[index]->rte;
}

/* Backend initialization */

static void
backend_init			(void)
{
	int i;

	avcodec_init ();

	for (i = 0; i < num_contexts; i++) {
		ffmpeg_context_class *fxc = context_table[i];
		rte_context_class *xc = &fxc->rte;

		xc->_public.backend	= "ffmpeg 0.4.6";

		xc->_public.mime_type	= fxc->av->mime_type;
		xc->_public.extension	= fxc->av->extensions;
		
		xc->_new		= context_new;
		xc->_delete		= context_delete;

		xc->codec_enum		= codec_enum;
		xc->codec_get		= codec_get;
		xc->codec_set		= codec_set;

		xc->codec_option_set	= option_set;
		xc->codec_option_get	= option_get;
		xc->codec_option_print	= option_print;
		xc->codec_option_enum	= option_enum;

		xc->parameters_set	= parameters_set;

		xc->set_input		= set_input;
		xc->set_output		= set_output;

		xc->start		= start;
		xc->stop		= stop;

		xc->status		= status;
	}
}

const rte_backend_class
rte_backend_ffmpeg = {
	.name			= "ffmpeg",
	.backend_init		= backend_init,
	.context_enum		= context_enum,
};
