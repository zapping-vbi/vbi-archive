/*
 *  Real Time Encoder library
 *  divx4linux backend
 *
 *  Copyright (C) 2002 Michael H. Schimek
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

/* $Id: b_divx4linux.c,v 1.3 2002-10-04 13:51:19 mschimek Exp $ */

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

static __inline__ int
encore_init4			(ENC_PARAM4 *		param)
{
	return encore_fn (NULL, ENC_OPT_INIT, param, NULL);
}

static __inline__ int
encore_init5			(ENC_PARAM *		param)
{
	return encore_fn (NULL, ENC_OPT_INIT, param, NULL);
}

static __inline__ int
encore_release			(void *			handle)
{
	return encore_fn (handle, ENC_OPT_RELEASE, NULL, NULL);
}

static __inline__ int
encore_encode			(void *			handle,
				 ENC_FRAME *		frame,
				 ENC_RESULT *		result)
{
	return encore_fn (handle, ENC_OPT_ENCODE, frame, result);
}

static __inline__ int
encore_version			(void)
{
	return encore_fn (NULL, ENC_OPT_VERSION, NULL, NULL);
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

static void *
mainloop			(void *			p)
{
	d4l_context *dx = p;

	dprintf ("mainloop\n");

	for (;;) {
		rte_buffer rb, wb;

		pthread_testcancel ();

		/* FIXME frame dropping */

		rb.data = NULL;
		rb.size = 0;

		if (dx->read_cb (&dx->context, &dx->codec, &rb)) {
			assert (rb.data != NULL && rb.size > 0);
		} else { /* EOF */
			break; /* XXX */
		}

		dprintf ("compressing %p %u\n", rb.data, rb.size);

		dx->enc_frame.image = rb.data;
		dx->enc_frame.bitstream = dx->buffer;

		encore_encode (dx->handle,
			       &dx->enc_frame,
			       &dx->enc_result);

		wb.data = dx->buffer;
		wb.size = dx->enc_frame.length;

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

	pthread_cancel (dx->thread_id);
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

#if ENCORE_VERSION >= ENCORE5_VERSION
	if (encore_vers >= ENCORE5_VERSION) {
		encore_init5 (&dx->enc_param5);
		dx->handle = dx->enc_param5.handle;
	} else
#endif
	{
		encore_init4 (&dx->enc_param4);
		dx->handle = dx->enc_param4.handle;
	}

	dx->context.state = RTE_STATE_RUNNING;
	dx->codec.state = RTE_STATE_RUNNING;

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

	if (!(dx->buffer = malloc (8 * dx->codec.params.video.width
				  * dx->codec.params.video.height))) {
		rte_error_printf (&dx->context, _("Out of memory."));
		return FALSE;
	}

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

	dx->enc_param4.framerate = rsp->video.frame_rate;

	/* aspect ratio ignored */

	dx->enc_param4.x_dim = rsp->video.width =
		(saturate (rsp->video.width, 16, 768) + 8) & -16;
	dx->enc_param4.y_dim = rsp->video.height =
		(saturate (rsp->video.height, 16, 576) + 8) & -16;

	rsp->video.offset = 0;

	switch (rsp->video.pixfmt) {
	case RTE_PIXFMT_BGR24:
		dx->enc_frame.colorspace = ENC_CSP_RGB24;
		rsp->video.stride = rsp->video.width * 3;
		break;

	case RTE_PIXFMT_YUV420:
		if (rsp->video.v_offset < rsp->video.u_offset) {
	default:
			rsp->video.pixfmt = RTE_PIXFMT_YUV420;
			dx->enc_frame.colorspace = ENC_CSP_YV12;
			rsp->video.v_offset = rsp->video.width * rsp->video.height;
			rsp->video.u_offset = (rsp->video.v_offset * 5) >> 2;
		} else {
			dx->enc_frame.colorspace = ENC_CSP_I420;
			rsp->video.u_offset = rsp->video.width * rsp->video.height;
			rsp->video.v_offset = (rsp->video.u_offset * 5) >> 2;
		}
		rsp->video.stride = rsp->video.width;
		rsp->video.uv_stride = rsp->video.width >> 1;
		break;

	case RTE_PIXFMT_YUYV:
		dx->enc_frame.colorspace = ENC_CSP_YUY2;
		rsp->video.stride = rsp->video.width * 2;
		break;

	case RTE_PIXFMT_UYVY:
		dx->enc_frame.colorspace = ENC_CSP_UYVY;
		rsp->video.stride = rsp->video.width * 2;
		break;
	}

#if ENCORE_VERSION >= ENCORE5_VERSION
	if (encore_vers >= ENCORE5_VERSION) {
		dx->enc_param5.x_dim			= dx->enc_param4.x_dim;
		dx->enc_param5.y_dim			= dx->enc_param4.y_dim;
		dx->enc_param5.framerate		= dx->enc_param4.framerate;
		dx->enc_param5.bitrate			= dx->enc_param4.bitrate;
		dx->enc_param5.max_key_interval		= dx->enc_param4.max_key_interval;
		dx->enc_param5.quality			= dx->enc_param4.quality;
		dx->enc_param5.extensions.use_bidirect	= dx->enc_param4.use_bidirect;
		dx->enc_param5.extensions.obmc		= dx->enc_param4.obmc;
		dx->enc_param5.deinterlace		= dx->enc_param4.deinterlace;
	}
#endif

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
		v->num = dx->enc_param4.bitrate;
	} else if (KEYWORD ("i_dist")) {
		v->num = dx->enc_param4.max_key_interval;
	} else if (KEYWORD ("quality")) {
		v->num = dx->enc_param4.quality - 1;
	} else if (KEYWORD ("bidirect")) {
		v->num = dx->enc_param4.use_bidirect;
	} else if (KEYWORD ("obmc")) {
		v->num = dx->enc_param4.obmc;
	} else if (KEYWORD ("deinterlace")) {
		v->num = dx->enc_param4.deinterlace;
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
		dx->enc_param4.bitrate = RTE_OPTION_ARG (int, 30000, 8000000);
	} else if (KEYWORD ("i_dist")) {
		dx->enc_param4.max_key_interval = RTE_OPTION_ARG (int, 0, 1024);
	} else if (KEYWORD ("quality")) {
		dx->enc_param4.quality = RTE_OPTION_ARG_SAT(int, 0, 4) - 1;
	} else if (KEYWORD ("bidirect")) {
		dx->enc_param4.use_bidirect = !!va_arg (args, int);
	} else if (KEYWORD ("obmc")) {
		dx->enc_param4.obmc = !!va_arg (args, int);
	} else if (KEYWORD ("deinterlace")) {
		dx->enc_param4.deinterlace = !!va_arg (args, int);
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
	if (index < 0 || index >= num_options)
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
divx_info = {
	.backend		= "divx4linux",
	.keyword		= "divx4linux_video",
	.label			= N_("DivX Video Elementary Stream"),
	.mime_type		= "video/x-mpeg",
	.extension		= ".divx",
	.min_elementary		= { 0, 1, 0 },
	.max_elementary		= { 0, 1, 0 },
};

static rte_context_class
divx_context = {
	._public		= &divx_info,

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

	if (encore_vers != ENCORE4_VERSION
#if ENCORE_VERSION >= ENCORE5_VERSION
	    && encore_vers != ENCORE5_VERSION
#endif
	    ) {
		rte_asprintf (&open_error, _("Version %d not supported\n"), encore_vers);
		dprintf ("version mismatch: %d\n", encore_vers);
		dlclose (encore_lib);
		encore_lib = NULL;
		return;
	}

	divx_context._new = context_new;
	divx_context._delete = context_delete;

	if (encore_vers >= ENCORE5_VERSION) {
		d4l_codec._public->keyword = "divx5_video";
		d4l_codec._public->label = N_("DivX 5.x Video");
	}

	dprintf ("backend_init ok, divx version %d\n", encore_vers);
}

static rte_context_class *
context_enum			(unsigned int		index,
				 char **		errstr)
{
	if (index != 0)
		return NULL;

	if (!encore_lib) {
		rte_asprintf (errstr, _("DivX4Linux library not available. %s\n"),
			      open_error);
	}

	return &divx_context;
}

const rte_backend_class
rte_backend_divx4linux = {
	.name			= "divx4linux",
	.backend_init		= backend_init,
	.context_enum		= context_enum,
};
