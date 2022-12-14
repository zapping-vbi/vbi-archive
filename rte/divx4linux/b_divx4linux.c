/*
 *  Real Time Encoder library
 *  divx4linux backend
 *
 *  Copyright (C) 2002 Michael H. Schimek
 *  Copyright (C) 2002 FFmpeg authors (AVI code)
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

/* $Id: b_divx4linux.c,v 1.8 2005-02-25 18:18:07 mschimek Exp $ */

#include <dlfcn.h>
#include "b_divx4linux.h"

#ifdef DIVX4LINUX_DEBUG
#define dprintf(templ, args...) fprintf(stderr, "d4l: " templ ,##args)
#else
#define DIVX4LINUX_DEBUG 0
#define dprintf(templ, args...)
#endif

static void *		encore_lib;
static int		(* encore_fn)(void *		handle,
				      int		enc_opt,
				      void *		param1,
				      void *		param2);
static int		encore_vers;

#if ENCORE_VERSION == 20010807

static int
encore_init			(d4l_context *		dx)
{
	
	ENC_PARAM param;
	int r;

	switch (dx->codec.params.video.pixfmt) {
	case RTE_PIXFMT_BGR24:
		dx->enc_frame.colorspace = ENC_CSP_RGB24;
		break;
	case RTE_PIXFMT_YUV420:
		if (dx->codec.params.video.v_offset
		    < dx->codec.params.video.u_offset)
			dx->enc_frame.colorspace = ENC_CSP_YV12;
		else
			dx->enc_frame.colorspace = ENC_CSP_I420;
		break;
	case RTE_PIXFMT_YUYV:
		dx->enc_frame.colorspace = ENC_CSP_YUY2;
		break;
	case RTE_PIXFMT_UYVY:
		dx->enc_frame.colorspace = ENC_CSP_UYVY;
		break;
	default:
		assert (!"reached");
	}

	memset (&param, 0, sizeof (param));

	param.x_dim		= dx->x_dim;
	param.y_dim		= dx->y_dim;
	param.framerate		= dx->frame_rate;
	param.bitrate		= dx->bit_rate;
	param.max_key_interval	= dx->max_key_interval;
	param.use_bidirect	= dx->use_bidirect;
	param.deinterlace	= dx->deinterlace;
	param.quality		= dx->quality;
	param.obmc		= dx->obmc;

	r = encore_fn (NULL, ENC_OPT_INIT, &param, NULL);

	dx->handle = param.handle;

	return r;
}

#elif ENCORE_VERSION == 20020304

static int
encore_init			(d4l_context *		dx)
{
	ENC_PARAM param;
	int r;

	switch (dx->codec.params.video.pixfmt) {
	case RTE_PIXFMT_BGR24:
		dx->enc_frame.colorspace = ENC_CSP_RGB24;
		break;
	case RTE_PIXFMT_YUV420:
		if (dx->codec.params.video.v_offset
		    < dx->codec.params.video.u_offset)
			dx->enc_frame.colorspace = ENC_CSP_YV12;
		else
			dx->enc_frame.colorspace = ENC_CSP_I420;
		break;
	case RTE_PIXFMT_YUYV:
		dx->enc_frame.colorspace = ENC_CSP_YUY2;
		break;
	case RTE_PIXFMT_UYVY:
		dx->enc_frame.colorspace = ENC_CSP_UYVY;
		break;
	default:
		assert (!"reached");
	}

	memset (&param, 0, sizeof (param));

	param.x_dim		= dx->x_dim;
	param.y_dim		= dx->y_dim;
	param.framerate		= dx->frame_rate;
	param.bitrate		= dx->bit_rate;
	param.max_key_interval	= dx->max_key_interval;
	param.deinterlace	= dx->deinterlace;
	param.quality		= dx->quality;

	param.extensions.use_bidirect	= dx->use_bidirect;
	param.extensions.obmc		= dx->obmc;

	r = encore_fn (NULL, ENC_OPT_INIT, &param, NULL);

	dx->handle = param.handle;

	return r;
}

#elif ENCORE_VERSION == 20021024

#define FOURCC(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))

static int
encore_init			(d4l_context *		dx)
{
	DivXBitmapInfoHeader format;
	SETTINGS param;
	int r;

	memset (&format, 0, sizeof (format));

	format.biSize		= sizeof (format);
	format.biWidth		= dx->x_dim;
	format.biHeight		= dx->y_dim;

	switch (dx->codec.params.video.pixfmt) {
	case RTE_PIXFMT_BGR24:
		format.biBitCount = 24;
		break;
	case RTE_PIXFMT_YUV420:
		if (dx->codec.params.video.v_offset
		    < dx->codec.params.video.u_offset) {
			format.biCompression = FOURCC('Y','V','1','2');
		} else {
			format.biCompression = FOURCC('I','4','2','0');
		}
		break;
	case RTE_PIXFMT_YUYV:
		format.biCompression = FOURCC('Y','U','Y','2');
		break;
	case RTE_PIXFMT_UYVY:
		format.biCompression = FOURCC('U','Y','V','Y');
		break;
	default:
		assert (!"reached");
	}

	memset (&param, 0, sizeof (param));

	if (dx->frame_rate > 27.0) {
		param.input_clock = 30000;
		param.input_frame_period = 1001;
	} else {
		param.input_clock = 100;
		param.input_frame_period = 4;
	}

	param.bitrate		= dx->bit_rate;
	param.max_key_interval	= dx->max_key_interval;
	param.deinterlace	= dx->deinterlace;
	param.quality		= dx->quality;

	param.use_bidirect	= dx->use_bidirect;
	param.use_gmc		= dx->obmc;

	r = encore_fn ((void *) &dx->handle,
		       ENC_OPT_INIT, &format, &param);

	return r;
}

#else
#warning

static int
encore_init			(d4l_context *		dx)
{
	return -1;
}

#endif

static int
encore_release			(void *			handle)
{
	return encore_fn (handle, ENC_OPT_RELEASE, NULL, NULL);
}

static int
encore_encode			(void *			handle,
				 ENC_FRAME *		frame,
				 ENC_RESULT *		result)
{
	return encore_fn (handle, ENC_OPT_ENCODE, frame, result);
}

static int
encore_version			(void)
{
#ifdef ENC_OPT_VERSION
	return encore_fn (NULL, ENC_OPT_VERSION, NULL, NULL);
#else
	return 0;
#endif
}

static void reset_input		(rte_codec *		codec);
static void reset_output	(d4l_context *		dx);
static rte_bool parameters_set	(rte_codec *		codec,
				 rte_stream_parameters *rsp);

static void
status				(rte_context *		context,
				 rte_codec *		codec,
				 rte_status *		status,
				 unsigned int		size)
{
	d4l_context *dx = DX (context);

	pthread_mutex_lock (&dx->codec.mutex);

	memcpy (status, &dx->status, size);

	pthread_mutex_unlock (&dx->codec.mutex);
}

/*
 *  AVI container code based on ffmpeg
 */

#define AVIF_HASINDEX		0x00000010	/* Index at end of file? */
#define AVIF_MUSTUSEINDEX	0x00000020
#define AVIF_ISINTERLEAVED	0x00000100
#define AVIF_TRUSTCKTYPE	0x00000800      /* Use CKType to find key frames? */
#define AVIF_WASCAPTUREFILE	0x00010000
#define AVIF_COPYRIGHTED	0x00020000

/* FIXME endianess */

#define ins_le16(ptr, val) (*((uint16_t *)(ptr)) = (val))
#define ins_le32(ptr, val) (*((uint32_t *)(ptr)) = (val))

#define put_le16(val) (ins_le16(p, val), p += 2)
#define put_le32(val) (ins_le32(p, val), p += 4)

/* gcc optimizes to movl */
#define put_tag(str) (memcpy (p, str, 4), p += 4)

#define start_tag(str) (memcpy (p, str, 4), p += 8, p - 4)
#define end_tag(ptr) (*((uint32_t *)(ptr)) = (p - (ptr) - 4))

static int		nframes1;
static int		nframes2;
static int		movi;
static int		hdrlen;

static const char *	codec_tag = "DIVX";

static inline void
avi_write_header		(d4l_context *		dx,
				 uint8_t *		p0)
{
	uint8_t *p, *start, *list1, *list2, *avih, *strh, *strf;

	p = p0;

	start = start_tag ("RIFF");		/* file length */
	put_tag ("AVI ");

	/* header list */
	list1 = start_tag ("LIST");
	put_tag ("hdrl");

	/* avi header */
	avih = start_tag ("avih");
	put_le32 ((unsigned int)(1000000 / dx->frame_rate));
	put_le32 ((dx->bit_rate + 7) >> 3);
	put_le32 (0);				/* padding */
	put_le32 (AVIF_TRUSTCKTYPE
		  | AVIF_HASINDEX
		  | AVIF_ISINTERLEAVED);	/* flags */
	nframes1 = p - p0;
	put_le32 (0);				/* num frames (later) */
	put_le32 (0);				/* initial frame */
	put_le32 (1);				/* num streams */
	put_le32 (1024 * 1024);			/* suggested buffer size */
	put_le32 (dx->x_dim);
	put_le32 (dx->y_dim);
	put_le32 (0);				/* reserved */
	put_le32 (0);				/* reserved */
	put_le32 (0);				/* reserved */
	put_le32 (0);				/* reserved */
	end_tag (avih);

	/* stream list */
	list2 = start_tag ("LIST");
	put_tag ("strl");

	strh = start_tag ("strh");		/* stream generic header */
	/* video stream */
	put_tag ("vids");
	put_tag (codec_tag);
	put_le32 (0);				/* flags */
	put_le16 (0);				/* priority */
	put_le16 (0);				/* language */
	put_le32 (0);				/* initial frame */
	put_le32 (1000);			/* scale */
	put_le32 ((unsigned int)(1000 * dx->frame_rate));
	put_le32 (0);				/* start */
	nframes2 = p - p0;
	put_le32 (0);				/* num frames (later) */
	put_le32 (1024 * 1024);			/* suggested buffer size */
	put_le32 (-1);				/* quality */
	put_le32 (3 * dx->x_dim * dx->y_dim);	/* sample size */
	put_le16 (0);
	put_le16 (0);
	put_le16 (dx->x_dim);
	put_le16 (dx->y_dim);
	end_tag (strh);

	strf = start_tag ("strf");
	put_le32 (40);				/* BITMAPINFOHEADER size */
	put_le32 (dx->x_dim);
	put_le32 (dx->y_dim);
	put_le16 (1);				/* planes */
	put_le16 (24);				/* depth */
	put_tag (codec_tag);			/* compression type */
	put_le32 (3 * dx->x_dim * dx->y_dim);
	put_le32 (0);
	put_le32 (0);
	put_le32 (0);
	put_le32 (0);
        end_tag (strf);

        end_tag (list2);			/* - stream list */
	end_tag (list1);			/* - header list */

	movi = start_tag ("LIST") - p0;
	put_tag ("movi");

	hdrlen = p - p0;
}

static inline void
avi_write_trailer		(d4l_context *		dx,
				 uint8_t *		p,
				 uint32_t		movi_bytes)
{
	ins_le32 (p + 4, hdrlen + movi_bytes - 8);
	ins_le32 (p + nframes1, dx->status.frames_out);
	ins_le32 (p + nframes2, dx->status.frames_out);
	ins_le32 (p + movi, movi_bytes + 4);
}

extern rte_context_class divx_avi_context;

static void *
mainloop			(void *			p)
{
	d4l_context *dx = p;
	uint8_t header[1024];
	uint32_t movi_bytes = 0;

	/* No interruption btw read & unref, mutex */
	pthread_setcancelstate (PTHREAD_CANCEL_DISABLE, NULL);

	dprintf ("mainloop\n");

	if (dx->context._class == &divx_avi_context) {
		rte_buffer wb;

		avi_write_header (dx, header);

		wb.data = header;
		wb.size = hdrlen;

		if (!dx->write_cb (&dx->context, NULL, &wb)) {
			return NULL; /* XXX */
		}

		movi_bytes = 0;
	}

	while (!dx->stopped) {
		rte_buffer rb, wb;

		/* FIXME frame dropping */

		rb.data = NULL;
		rb.size = 0;

		if (dx->read_cb (&dx->context, &dx->codec, &rb)) {
			assert (rb.data != NULL && rb.size > 0);
		} else { /* EOF */
			break; /* XXX */
		}

		dprintf ("compressing %p %u\n", rb.data, rb.size);

		if (dx->context._class == &divx_avi_context) {
			dx->enc_frame.image = rb.data;
			dx->enc_frame.bitstream = 8 + (uint8_t *) dx->buffer;
		} else {
			dx->enc_frame.image = rb.data;
			dx->enc_frame.bitstream = dx->buffer;
		}

		encore_encode (dx->handle,
			       &dx->enc_frame,
			       &dx->enc_result);

		if (dx->context._class == &divx_avi_context) {
			static const int stream_index = 0;
			uint8_t *p = dx->buffer;

			p[0] = '0';
			p[1] = '0' + stream_index;
		        p[2] = 'd';
		        p[3] = 'c';

			ins_le32 (p + 4, dx->enc_frame.length);

			wb.data = dx->buffer;
			wb.size = 8 + dx->enc_frame.length;

			if (dx->enc_frame.length & 1)
				((uint8_t *) wb.data)[wb.size++] = 0;

			movi_bytes += wb.size;
		} else {
			wb.data = dx->buffer;
			wb.size = dx->enc_frame.length;
		}

		pthread_mutex_lock (&dx->codec.mutex);

		dx->status.frames_in += 1;
		dx->status.frames_out = dx->status.frames_in;
		dx->status.bytes_in += rb.size;
		dx->status.bytes_out += wb.size;
		dx->status.captured_time = rb.timestamp;
		dx->status.coded_time += dx->status.time_per_frame_out;

		pthread_mutex_unlock (&dx->codec.mutex);

		if (!dx->write_cb (&dx->context, NULL, &wb)) {
			break; /* XXX */
		}

		if (dx->unref_cb)
			dx->unref_cb (&dx->context, &dx->codec, &rb);
	}

	if (dx->context._class == &divx_avi_context) {
		rte_buffer wb;

		/* XXX error */
		dx->seek_cb (&dx->context, 0, SEEK_SET);

		avi_write_trailer (dx, header, movi_bytes);

		wb.data = header;
		wb.size = hdrlen;

		if (!dx->write_cb (&dx->context, NULL, &wb)) {
			return NULL; /* XXX */
		}
	}

	return NULL;
}

static rte_bool
stop				(rte_context *		context,
				 double			timestamp)
{
	d4l_context *dx = DX (context);

	if (dx->context.state != RTE_STATE_RUNNING) {
		rte_error_printf (&dx->context, "Context %s not running.",
				  dx->context._class->_public->keyword);
		return FALSE;
	}

	dx->stopped = TRUE;

	pthread_join (dx->thread_id, NULL);

	encore_release (dx->handle);

	dx->codec.state = RTE_STATE_READY;

	/* Destroy input, reset codec, -> RTE_STATE_PARAM */
	reset_input (&dx->codec);
	parameters_set (&dx->codec, &dx->codec.params);

	reset_output (dx);

	dx->context.state = RTE_STATE_NEW;

	return TRUE;
}

static rte_bool
start				(rte_context *		context,
				 double			timestamp,
				 rte_codec *		time_ref,
				 rte_bool		async)
{
	d4l_context *dx = DX (context);
	int error;

	switch (dx->context.state) {
	case RTE_STATE_READY:
		break;

	case RTE_STATE_RUNNING:
		rte_error_printf (&dx->context, "Context %s already running.",
				  dx->context._class->_public->keyword);
		return FALSE;

	default:
		rte_error_printf (&dx->context, "Cannot start context %s, "
				  "initialization unfinished.",
				  dx->context._class->_public->keyword);
		return FALSE;
	}

	if (dx->codec.state != RTE_STATE_READY) {
		rte_error_printf (&dx->context, "Cannot start context %s, initialization "
						"of codec is unfinished.",
				  dx->context._class->_public->keyword);
		return FALSE;
	}

	encore_init (dx);

	dx->context.state = RTE_STATE_RUNNING;
	dx->codec.state = RTE_STATE_RUNNING;

	dx->stopped = FALSE;

	error = pthread_create (&dx->thread_id, NULL, mainloop, dx);

	if (error != 0) {
		dx->context.state = RTE_STATE_READY;
		dx->codec.state = RTE_STATE_READY;

		rte_error_printf (&dx->context, _("Insufficient resources to start "
						  "encoding thread.\n"));
		return FALSE;
	}

        return TRUE;
}

/* Input / Output */

static void
reset_output			(d4l_context *		dx)
{
	free (dx->buffer);
	dx->buffer = NULL;

	dx->context.state = RTE_STATE_NEW;
}

static rte_bool
set_output			(rte_context *		context,
				 rte_buffer_callback	write_cb,
				 rte_seek_callback	seek_cb)
{
	d4l_context *dx = DX (context);

	switch (dx->context.state) {
	case RTE_STATE_NEW:
		break;

	case RTE_STATE_READY:
		reset_output (dx);
		break;

	default:
		rte_error_printf (&dx->context, "Cannot change %s output, context is busy.",
				  dx->context._class->_public->keyword);
		break;
	}

	if (!dx->codec_set) {
		rte_error_printf (context, "No codec allocated for context %s.",
				  dx->context._class->_public->keyword);
		return FALSE;
	}

	if (dx->codec.state != RTE_STATE_READY) {
		rte_error_printf (&dx->context, "Codec initialization unfinished.");
		return FALSE;
	}

	assert (dx->buffer == NULL);

	if (!(dx->buffer = malloc (8
				   * dx->codec.params.video.width
				   * dx->codec.params.video.height))) {
		rte_error_printf (&dx->context, _("Out of memory."));
		return FALSE;
	}

	dx->seek_cb = seek_cb;
	dx->write_cb = write_cb;
	dx->context.state = RTE_STATE_READY;

	return TRUE;
}

static void
reset_input			(rte_codec *		codec)
{
	codec->state = RTE_STATE_PARAM;
}

static rte_bool
set_input			(rte_codec *		codec,
				 rte_io_method		input_method,
			         rte_buffer_callback	read_cb,
				 rte_buffer_callback	unref_cb,
				 unsigned int *		queue_length)
{
	rte_context *context = codec->context;
	d4l_context *dx = DX (context);

	switch (codec->state) {
	case RTE_STATE_NEW:
		rte_error_printf (context, "Attempt to select input method with "
					   "uninitialized input parameters.");
		return FALSE;

	case RTE_STATE_PARAM:
		break;

	case RTE_STATE_READY:
		reset_input (codec);
		break;

	default:
		rte_error_printf (context, "Cannot change input, codec is busy.");
		break;
	}

	switch (input_method) {
	case RTE_CALLBACK_MASTER:
		*queue_length = 1; /* XXX yes? */
		break;

	case RTE_CALLBACK_SLAVE:
	case RTE_PUSH_MASTER:
	case RTE_PUSH_SLAVE:
		rte_error_printf (context, "Selected input method not supported yet.");
		return FALSE;

	default:
		assert (!"rte bug");
	}

	dx->input_method = input_method;
	dx->read_cb = read_cb;
	dx->unref_cb = unref_cb;

	codec->state = RTE_STATE_READY;

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

/* Sampling parameters */

static rte_bool
parameters_set			(rte_codec *		codec,
				 rte_stream_parameters *rsp)
{
	rte_context *context = codec->context;
	d4l_context *dx = DX (context);

	if (codec->state != RTE_STATE_NEW) {
		codec->state = RTE_STATE_NEW;
	}

	rsp->video.framefmt = RTE_FRAMEFMT_PROGRESSIVE;
	rsp->video.spatial_order = 0;
	rsp->video.temporal_order = 0;

	dx->frame_rate = rsp->video.frame_rate;

	/* aspect ratio ignored */

	dx->x_dim = rsp->video.width =
		(saturate (rsp->video.width, 16, 768) + 8) & -16;
	dx->y_dim = rsp->video.height =
		(saturate (rsp->video.height, 16, 576) + 8) & -16;

	rsp->video.offset = 0;

	switch (rsp->video.pixfmt) {
	case RTE_PIXFMT_BGR24:
		rsp->video.stride = rsp->video.width * 3;
		break;

	case RTE_PIXFMT_YUV420:
		if (rsp->video.v_offset < rsp->video.u_offset) {
	default:
			rsp->video.v_offset = rsp->video.width * rsp->video.height;
			rsp->video.u_offset = (rsp->video.v_offset * 5) >> 2;
		} else {
			rsp->video.u_offset = rsp->video.width * rsp->video.height;
			rsp->video.v_offset = (rsp->video.u_offset * 5) >> 2;
		}
		rsp->video.stride = rsp->video.width;
		rsp->video.uv_stride = rsp->video.width >> 1;
		break;

	case RTE_PIXFMT_YUYV:
		rsp->video.stride = rsp->video.width * 2;
		break;

	case RTE_PIXFMT_UYVY:
		rsp->video.stride = rsp->video.width * 2;
		break;
	}

	rsp->video.frame_size = 0;

	memset (&dx->status, 0, sizeof (&dx->status));

	dx->status.time_per_frame_out = 1.0 / rsp->video.frame_rate;

	dx->status.valid = 0
		+ RTE_STATUS_FRAMES_IN
		+ RTE_STATUS_FRAMES_OUT
		+ RTE_STATUS_BYTES_IN
		+ RTE_STATUS_BYTES_OUT
		+ RTE_STATUS_CAPTURED_TIME
		+ RTE_STATUS_CODED_TIME;

	/* Parameters accepted */
	memcpy (&codec->params, rsp, sizeof (codec->params));

	codec->state = RTE_STATE_PARAM;

	return TRUE;
}

/* Codec options */

static char *
menu_quality[] = {
	/* NLS: DivX encoding quality */
	N_("Fastest encoding, worst quality"),
	N_("Lower quality"),
	N_("Medium quality"),
	N_("Higher quality"),
	N_("Slowest encoding, best quality"),
};

static rte_option_info
options[] = {
	RTE_OPTION_INT_RANGE_INITIALIZER
	  ("bit_rate", N_("Bit rate"),
	   1000000, 30000, 8000000, 1000, NULL),
	RTE_OPTION_INT_RANGE_INITIALIZER
	  ("i_dist", N_("Key frame distance"),
	   12, 0, 1024, 1, NULL),
	RTE_OPTION_MENU_INITIALIZER
	  ("quality", N_("Compression quality"),
	   2 /* medium */, menu_quality, 5, NULL),
	RTE_OPTION_BOOL_INITIALIZER /* NLS: DivX option */
	  ("bidirect", N_("Bidirectional encoding"), FALSE, NULL),
	RTE_OPTION_BOOL_INITIALIZER /* NLS: DivX option */
	  ("obmc", N_("Overlapped block motion compensation"), FALSE, NULL),
	RTE_OPTION_BOOL_INITIALIZER
	  ("deinterlace", N_("Deinterlace"), FALSE, NULL),
};

static const int num_options = sizeof (options) / sizeof (* options);

#define KEYWORD(name) (strcmp (keyword, name) == 0)

static char *
option_print			(rte_codec *		codec,
				 const char *		keyword,
				 va_list		args)
{
	rte_context *context = codec->context;
	char buf[80];

	if (KEYWORD ("bit_rate")) {
		snprintf (buf, sizeof (buf), _("%5.3f Mbit/s"), va_arg (args, int) / 1e6);
	} else if (KEYWORD ("i_dist")) {
		snprintf (buf, sizeof (buf), "%u", va_arg (args, int));
	} else if (KEYWORD ("quality")) {
		return rte_strdup (context, NULL, _(menu_quality[
			RTE_OPTION_ARG_MENU (menu_quality)]));
	} else if (KEYWORD ("bidirect")
		   || KEYWORD ("obmc")
		   || KEYWORD ("deinterlace")) {
		return rte_strdup (context, NULL, va_arg (args, int) ?
				   _("on") : _("off"));
	} else {
		rte_unknown_option(context, codec, keyword);
	failed:
		return NULL;
	}

	return rte_strdup (context, NULL, buf);
}

static rte_bool
option_get			(rte_codec *		codec,
				 const char *		keyword,
				 rte_option_value *	v)
{
	rte_context *context = codec->context;
        d4l_context *dx = DX (context);

	if (KEYWORD ("bit_rate")) {
		v->num = dx->bit_rate;
	} else if (KEYWORD ("i_dist")) {
		v->num = dx->max_key_interval;
	} else if (KEYWORD ("quality")) {
		v->num = dx->quality - 1;
	} else if (KEYWORD ("bidirect")) {
		v->num = dx->use_bidirect;
	} else if (KEYWORD ("obmc")) {
		v->num = dx->obmc;
	} else if (KEYWORD ("deinterlace")) {
		v->num = dx->deinterlace;
	} else {
		rte_unknown_option (context, codec, keyword);
		return FALSE;
	}

        return TRUE;
}

static rte_bool
option_set			(rte_codec *		codec,
				 const char *		keyword,
				 va_list		args)
{
	rte_context *context = codec->context;
        d4l_context *dx = DX (context);

	if (KEYWORD ("bit_rate")) {
		dx->bit_rate = RTE_OPTION_ARG (int, 30000, 8000000);
	} else if (KEYWORD ("i_dist")) {
		dx->max_key_interval = RTE_OPTION_ARG (int, 0, 1024);
	} else if (KEYWORD ("quality")) {
		dx->quality = RTE_OPTION_ARG_SAT(int, 0, 4) - 1;
	} else if (KEYWORD ("bidirect")) {
		dx->use_bidirect = !!va_arg (args, int);
	} else if (KEYWORD ("obmc")) {
		dx->obmc = !!va_arg (args, int);
	} else if (KEYWORD ("deinterlace")) {
		dx->deinterlace = !!va_arg (args, int);
	} else {
		rte_unknown_option (context, codec, keyword);
		return FALSE;
	}

        return TRUE;

 failed:
	return FALSE;
}

static rte_option_info *
option_enum			(rte_codec *		codec,
				 unsigned int		index)
{
	if (index >= num_options)
		return NULL;

	return options + index;
}

/* Codec allocation */

static rte_codec *
codec_get			(rte_context *		context,
				 rte_stream_type	stream_type,
				 unsigned int		stream_index)
{
	d4l_context *dx = DX (context);
	rte_codec *codec = &dx->codec;

	if (codec->_class->_public->stream_type == stream_type
	    && codec->stream_index == stream_index)
		return codec;

	return NULL;
}

static rte_codec_info
d4l_info = {
	.stream_type	= RTE_STREAM_VIDEO,
	.keyword	= "divx4_video",
	.label		= N_("DivX 4.x Video"),
};

static rte_codec_class d4l_codec = { ._public = &d4l_info };

static rte_codec *
codec_set			(rte_context *		context,
				 const char *		keyword,
				 rte_stream_type	stream_type,
				 unsigned int		stream_index)
{
	d4l_context *dx = DX (context);

	if (dx->codec_set) {
		switch (dx->codec.state) {
		case RTE_STATE_PARAM:
			break;

		case RTE_STATE_READY:
			reset_input (&dx->codec);
			break;

		case RTE_STATE_RUNNING:
		case RTE_STATE_PAUSED:
			fprintf (stderr, "divx4linux bug warning: attempt to delete "
				 "running codec ignored\n");
			return NULL;

		default:
			break;
		}

		pthread_mutex_destroy (&dx->codec.mutex);

		dx->codec_set = FALSE;
	}

	if (keyword) {
		if (strcmp (d4l_codec._public->keyword, keyword) != 0) {
			rte_error_printf (&dx->context, "'%s' is no codec of the %s context.",
				          keyword, dx->context._class->_public->keyword);
			return NULL;
		}

		dx->codec._class = &d4l_codec;
		dx->codec.context = &dx->context;

		pthread_mutex_init (&dx->codec.mutex, NULL);

		dx->codec.state = RTE_STATE_NEW;

		if (!rte_codec_options_reset (&dx->codec)) {
			pthread_mutex_destroy (&dx->codec.mutex);
			return NULL;
		}

		dx->codec_set = TRUE;

		return &dx->codec;
	}

	return NULL;
}

static rte_codec_info *
codec_enum			(rte_context *		context,
				 unsigned int		index)
{
	if (index != 0)
		return NULL;

	return d4l_codec._public;
}

static void
context_delete			(rte_context *		context)
{
	d4l_context *dx = DX (context);
	rte_codec *codec = &dx->codec;

	switch (dx->context.state) {
	case RTE_STATE_RUNNING:
	case RTE_STATE_PAUSED:
		assert (!"reached");

	case RTE_STATE_READY:
		reset_output (dx);
		break;

	default:
		break;
	}

	if (dx->codec_set)
		codec_set (&dx->context, NULL,
			   codec->_class->_public->stream_type,
			   codec->stream_index);

	pthread_mutex_destroy (&dx->context.mutex);

	free (dx);
}

static rte_context *
context_new			(rte_context_class *	xc,
				 char **		errstr)
{
	d4l_context *dx;
	rte_context *context;

	if (!(dx = calloc (1, sizeof (*dx)))) {
		rte_asprintf (errstr, _("Out of memory."));
		return NULL;
	}

	context = &dx->context;
	context->_class = xc;

	pthread_mutex_init (&context->mutex, NULL);

	context->state = RTE_STATE_NEW;

	return context;
}

/* Backend initialization */

static rte_context_info
divx_video_info = {
	.backend		= "divx4linux",
	.keyword		= "divx4linux_video",
	.label			= N_("DivX Video Elementary Stream"),
	.mime_type		= "video/x-mpeg",
	.extension		= "divx",
	.min_elementary		= { 0, 1, 0 },
	.max_elementary		= { 0, 1, 0 },
};

static rte_context_class
divx_video_context = {
	._public		= &divx_video_info,

	.codec_enum		= codec_enum,
	.codec_get		= codec_get,
	.codec_set		= codec_set,

	.codec_option_set	= option_set,
	.codec_option_get	= option_get,
	.codec_option_print	= option_print,
	.codec_option_enum	= option_enum,

	.parameters_set		= parameters_set,

	.set_input		= set_input,
	.set_output		= set_output,

	.start			= start,
	.stop			= stop,

	.status			= status,
};

static rte_context_info
divx_avi_info = {
	.backend		= "divx4linux",
	.keyword		= "divx4linux_avi",
	.label			= N_("DivX AVI Stream"),
	.mime_type		= "video/x-mpeg",
	.extension		= "divx",
	.min_elementary		= { 0, 1, 0 },
	.max_elementary		= { 0, 1, 0 },
};

rte_context_class
divx_avi_context = {
	._public		= &divx_avi_info,

	.codec_enum		= codec_enum,
	.codec_get		= codec_get,
	.codec_set		= codec_set,

	.codec_option_set	= option_set,
	.codec_option_get	= option_get,
	.codec_option_print	= option_print,
	.codec_option_enum	= option_enum,

	.parameters_set		= parameters_set,

	.set_input		= set_input,
	.set_output		= set_output,

	.start			= start,
	.stop			= stop,

	.status			= status,
};

static char *open_error;

static void
backend_init			(void)
{
	if (!(encore_lib = dlopen ("libdivxencore.so", RTLD_NOW))) {
		open_error = dlerror();
		dprintf ("dlopen failed: %s\n", open_error);
		return;
	}

	if (!(encore_fn = dlsym (encore_lib, "encore"))) {
		dprintf ("dlsym failed: %s\n", dlerror());
		dlclose (encore_lib);
		encore_lib = NULL;
		return;
	}

	encore_vers = encore_version ();

#ifdef ENCORE_VERSION
	if (encore_vers != ENCORE_VERSION) {
#else
	if (1) {
#endif
		rte_asprintf (&open_error, _("Version %d not supported\n"), encore_vers);
		dprintf ("version mismatch: %d\n", encore_vers);
		dlclose (encore_lib);
		encore_lib = NULL;
		return;
	}

	divx_video_context._new = context_new;
	divx_video_context._delete = context_delete;

	divx_avi_context._new = context_new;
	divx_avi_context._delete = context_delete;

	if (encore_vers >= 20020304) {
		codec_tag = "DX50";
		d4l_codec._public->keyword = "divx5_video";
		d4l_codec._public->label = N_("DivX 5.x Video");
	}

	dprintf ("backend_init ok, divx version %d\n", encore_vers);
}

static rte_context_class *
context_enum			(unsigned int		index,
				 char **		errstr)
{
	if (index >= 2)
		return NULL;

	if (!encore_lib) {
		rte_asprintf (errstr, _("DivX4Linux library not available. %s\n"),
			      open_error);
	}

	return index ? &divx_avi_context : &divx_video_context;
}

const rte_backend_class
rte_backend_divx4linux = {
	.name			= "divx4linux",
	.backend_init		= backend_init,
	.context_enum		= context_enum,
};
