/*
 *  Real Time Encoder lib
 *  ffmpeg backend
 *
 *  Copyright (C) 2001 Iñaki García Etxebarria
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

/* $Id: b_ffmpeg.c,v 1.2 2001-08-08 05:24:36 mschimek Exp $ */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#include "../mp1e/common/fifo.h"
#include "libav/avformat.h"

#include "rtepriv.h"

typedef struct {
	rte_context_private	priv;

	AVFormatContext		oc;

	pthread_t		enc;

	unsigned char		output_buffer[32768];
} backend_private;

static int
init_backend			(void)
{
	register_all();

	return 1;
}

static void
context_new			(rte_context	*context)
{
	/* default format */
	context->format = strdup("mpeg1");
}

static void
context_destroy			(rte_context	*context)
{
	free(context->format);
}

static void*
enc_thread			(void		*opaque)
{
	rte_context *context = opaque;
	backend_private *priv = (backend_private*)context->private;
	buffer *b;
	AVFormatContext *s = &priv->oc;
	int ret, pixels, i;
	UINT8 audio_buffer[4096];
	UINT8 video_buffer[100000];
	AVPicture picture;
	AVPacket pkt;
	consumer aud, vid;

	if (context->mode & RTE_AUDIO)
		assert(add_consumer(&context->private->aud, &aud));
	if (context->mode & RTE_VIDEO)
		assert(add_consumer(&context->private->vid, &vid));

	s->format->write_header(s);

	while (1)
		for (i=0; i<s->nb_streams; i++)
		{
			AVCodecContext *enc = &s->streams[i]->codec;
			if (enc->codec_type == CODEC_TYPE_AUDIO)
			{
				assert(context->mode & RTE_AUDIO);
				b = wait_full_buffer(&aud);
				if (b->rte_flags & BLANK_BUFFER)
				{
					send_empty_buffer(&aud, b);
					goto done;
				}
				ret = avcodec_encode_audio(enc,
						   audio_buffer,
						   sizeof(audio_buffer),
						   (short*)b->data);
				pkt.stream_index = i;
				pkt.data = audio_buffer;
				pkt.size = ret;
				av_write_packet(s, &pkt);
				send_empty_buffer(&aud, b);
			}
			else if (enc->codec_type == CODEC_TYPE_VIDEO)
			{
				assert(context->mode & RTE_VIDEO);
				b = wait_full_buffer(&vid);
				if (b->rte_flags & BLANK_BUFFER)
				{
					send_empty_buffer(&vid, b);
					goto done;
				}
				picture.data[0] = b->data;
				picture.linesize[0] = context->width;
				picture.linesize[1] =
					picture.linesize[2] = context->width/2;
				pixels = context->width * context->height;
				switch (context->video_format)
				{
				case RTE_YUV420:
					picture.data[1] =
						picture.data[0] +
						pixels;
					picture.data[2] =
						picture.data[1] +
						pixels/4;
					break;
				case RTE_YVU420:
					picture.data[2] =
						picture.data[0] +
						pixels;
					picture.data[1] =
						picture.data[2] +
						pixels/4;
					break;
				default:
					assert(0);
					break;
				}
				
				ret = avcodec_encode_video(enc,
						     video_buffer,
						     sizeof(video_buffer),
						     &picture);
				pkt.stream_index = i;
				pkt.data = video_buffer;
				pkt.size = ret;
				av_write_packet(s, &pkt);
				send_empty_buffer(&vid, b);
			}
		}

 done:
	for (i=0; i<s->nb_streams; i++)
		avcodec_close(&s->streams[i]->codec);

	s->format->write_trailer(s);

	if (context->mode & RTE_AUDIO)
		rem_consumer(&aud);
	if (context->mode & RTE_VIDEO)
		rem_consumer(&vid);

	/* FIXME: look for possible mem leaks in allocated structs */

	return NULL;
}

static int
init_context			(rte_context	*context)
{
	AVStream *st;
	backend_private *priv = (backend_private*)context->private;
	AVFormatContext *oc = &priv->oc;
	AVCodec *codec;
	int use_video, use_audio, nb_streams, i;
	char *file_format, buffer[256];

	if (!context->format)
	{
		rte_error(context, "format == NULL !!!");
		return 0;
	}

	memset(oc, 0, sizeof(*oc));

	file_format = context->format;

#if 0 /* fixme: test with 0.4.5cvs */
	/* ffmpeg segfaults when the format mux mode doesn't match
	   exactly the requested mux mode, adapt when it's possible */
	if (!strcmp(file_format, "mpeg1"))
	{
		if (!(context->mode & RTE_AUDIO))
			file_format = "mpeg1video";
		if (!(context->mode & RTE_VIDEO))
			file_format = "mp2";
	}
	if (!strcmp(file_format, "rm"))
	{
		/* Video only unsupported by rm */
		if (!(context->mode & RTE_AUDIO))
		{
			rte_error(context, "rm format doesn't support "
				  "video only.");
			return 0;
		}
		if (!(context->mode & RTE_VIDEO))
			file_format = "ra";
	}
	if (!strcmp(file_format, "asf"))
	{
		/* Video only unsupported by asf (would be divx) */
		if (!(context->mode & RTE_AUDIO))
		{
			rte_error(context, "asf format doesn't support "
				  "video only.");
			return 0;
		}
		if (!(context->mode & RTE_VIDEO))
			file_format = "mp2";
	}
	if (!strcmp(file_format, "avi"))
	{
		/* Video only unsupported by avi (would be divx) */
		if (!(context->mode & RTE_AUDIO))
		{
			rte_error(context, "avi format doesn't support "
				  "video only.");
			return 0;
		}
		if (!(context->mode & RTE_VIDEO))
			file_format = "mp2";
	}
	if (!strcmp(file_format, "swf"))
	{
		/* Video only unsupported by swf (would be mjpeg) */
		if (!(context->mode & RTE_AUDIO))
		{
			rte_error(context, "swf format doesn't support "
				  "video only.");
			return 0;
		}
		if (!(context->mode & RTE_VIDEO))
			file_format = "mp2";
	}
#endif

	/* FIXME: Can also match based on mime type, we should add
	   this to rte */
	oc->format = guess_format(file_format, context->file_name, NULL);

	if (!oc->format)
	{
		rte_error(context, "format \"%s\" not found",
			  file_format);
		return 0;
	}

	if (context->video_format != RTE_YUV420 &&
	    context->video_format != RTE_YVU420)
	{
		rte_error(context, "ffmpeg backend only supports"
			  " YUV420 and YVU420 pixformats");
		return 0;
	}

	use_video = oc->format->video_codec != CODEC_ID_NONE;
	use_audio = oc->format->audio_codec != CODEC_ID_NONE;

	if ((!use_video) && (context->mode & RTE_VIDEO))
	{
		rte_error(context, "The given encoding format doesn't"
			" support video encoding");
		return 0;
	}
	if ((!use_audio) && (context->mode & RTE_AUDIO))
	{
		rte_error(context, "The given encoding format doesn't"
			" support audio encoding");
		return 0;
	}

	if (!(context->mode & RTE_VIDEO))
		use_video = 0;
	if (!(context->mode & RTE_AUDIO))
		use_audio = 0;

	nb_streams = 0;

	if (use_video)
	{
		AVCodecContext *video_enc;

		st = av_mallocz(sizeof(*st));
		video_enc = &st->codec;
		video_enc->codec_id = oc->format->video_codec;
		video_enc->codec_type = CODEC_TYPE_VIDEO;
		
		video_enc->bit_rate = context->output_video_bits;
		switch (context->video_rate)
		{
		case RTE_RATE_1:
			video_enc->frame_rate = 23.976 * FRAME_RATE_BASE;
			break;
		case RTE_RATE_2:
			video_enc->frame_rate = 24 * FRAME_RATE_BASE;
			break;
		case RTE_RATE_4:
			video_enc->frame_rate = 29.97 *	FRAME_RATE_BASE;
			break;
		case RTE_RATE_5:
			video_enc->frame_rate = 30 * FRAME_RATE_BASE;
			break;
		case RTE_RATE_6:
			video_enc->frame_rate = 50 * FRAME_RATE_BASE;
			break;
		case RTE_RATE_7:
			video_enc->frame_rate = 59.97 *	FRAME_RATE_BASE;
			break;
		case RTE_RATE_8:
			video_enc->frame_rate = 60 * FRAME_RATE_BASE;
			break;
		default:
			video_enc->frame_rate = 25 * FRAME_RATE_BASE;
			break;
		}
		video_enc->width = context->width;
		video_enc->height = context->height;
		if (!strspn(context->gop_sequence, "PB"))
			video_enc->gop_size = 0;
		else
			/* FIXME: improve this, gop_size is number of
			   no-I frames */
			video_enc->gop_size =
				strlen(context->gop_sequence)-1;
		oc->streams[nb_streams] = st;
		nb_streams++;
	}

	if (use_audio)
	{
		AVCodecContext *audio_enc;

		st = av_mallocz(sizeof(*st));

		audio_enc = &st->codec;
		audio_enc->codec_id = oc->format->audio_codec;
		audio_enc->codec_type = CODEC_TYPE_AUDIO;

		audio_enc->bit_rate = context->output_audio_bits;
		audio_enc->sample_rate = context->audio_rate;
		switch (context->audio_mode)
		{
		case RTE_AUDIO_MODE_MONO:
			audio_enc->channels = 1;
			break;
		default:
			audio_enc->channels = 2;
			break;
		}
		oc->streams[nb_streams++] = st;
	}

	oc->nb_streams = nb_streams;
	/* FIXME: oc->[title, author, copyright, comment] are allowed */

	oc->filename[sizeof(oc->filename)-1] = 0;
	if (context->file_name)
		strncpy(oc->filename, context->file_name,
			sizeof(oc->filename)-1);
	else
		strcpy(oc->filename, "rte stream");

	/* a bit hackish, but lets us avoid rte_global_context in
	   ffmpeg/libav/rte.c */
	sprintf(buffer, "rte:%p", context);
	assert (url_fopen(&oc->pb, buffer, URL_WRONLY) >= 0);

	for (i=0; i<oc->nb_streams; i++)
	{
		AVCodecContext *enc = &oc->streams[i]->codec;
		codec =	avcodec_find_encoder(enc->codec_id);
		/* Unknown codec can only mean internal error */
		assert(codec);
		if (avcodec_open(enc, codec) < 0)
		{
			/* FIXME: something better here is needed */
			rte_error(context,
				  "Cannot open avencoder (bad format)");
			return 0;
		}
		if (enc->codec_type == CODEC_TYPE_AUDIO)
			context->audio_bytes = enc->frame_size * 2 *
				enc->channels;
		/* FIXME: The encoder can use a different av format
		   than us,  we should fail if the parameters aren't
		   compatible */
	}

	/* we are prepared to start encoding */

	return 1;
}

static void
uninit_context			(rte_context	*context)
{
}

static int
start				(rte_context	*context)
{
	backend_private *priv = (backend_private*)context->private;

	if (pthread_create(&priv->enc, NULL, enc_thread, context))
	{
		rte_error(context, "pthread_create [%d]: %s", errno,
			  strerror(errno));
		return 0;
	}

	return 1;
}

static void
stop			(rte_context	*context)
{
	backend_private *priv = (backend_private*)context->private;

	pthread_join(priv->enc, NULL);
}

static char*
query_format		(rte_context	*context,
			 int		n,
			 enum rte_mux_mode	*mux_mode)
{
	AVFormat *f = first_format;
	int k=0;

	if (n < 0 || !context)
		return NULL;

	while (f)
	{
		if (n == k++)
			break;
		f = f->next;
	}

	if (!f)
		return NULL; /* not found */

	if (*mux_mode)
	{
		*mux_mode = 0;

		if (f->audio_codec != CODEC_ID_NONE)
			*mux_mode |= RTE_AUDIO;

		if (f->video_codec != CODEC_ID_NONE)
			*mux_mode |= RTE_VIDEO;
	}

	return (char*)f->name;
}

static void
status			(rte_context	*context,
			 struct rte_status_info	*status)
{
	memset(status, 0, sizeof(*status));

	status->bytes_out = context->private->bytes_out;
}

const
rte_backend_info b_ffmpeg_info =
{
	"ffmpeg",
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
