/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 Iñaki García Etxebarria
 * Copyright (C) 2003 Michael H. Schimek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <site_def.h>

/*
  This is the library in charge of simplifying Video Access API (I
  don't want to use thirteen lines of code with ioctl's every time I
  want to change tuning freq).
  the name is TV Engine, since it is intended mainly for TV viewing.
  This file is separated so zapping doesn't need to know about V4L[2]
*/


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef ENABLE_V4L
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <linux/kernel.h>

#include "tveng.h"
#define TVENG25_PROTOTYPES 1
#include "tveng25.h"

#include "zmisc.h"

/*
 *  Kernel interface
 */
#include <linux/types.h> /* __u32 etc */
#include "common/videodev25.h"
#include "common/fprintf_videodev25.h"

#define v4l25_ioctl(info, cmd, arg)					\
(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
 ((0 == device_ioctl ((info)->log_fp, fprintf_ioctl_arg,		\
		      (info)->fd, cmd, (void *)(arg))) ?		\
  0 : (ioctl_failure (info, __FILE__, __PRETTY_FUNCTION__,		\
		      __LINE__, # cmd), -1)))

#define v4l25_ioctl_nf(info, cmd, arg)					\
(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
 device_ioctl ((info)->log_fp, fprintf_ioctl_arg,			\
		      (info)->fd, cmd, (void *)(arg)))

struct video_input {
	tv_video_line		pub;

	unsigned int		index;		/* struct v4l2_input */
	unsigned int		tuner;		/* struct v4l2_tuner */

	unsigned int		step_shift;
};

#define VI(l) PARENT (l, struct video_input, pub)

struct control {
	tv_control		pub;
	unsigned int		id;		/* V4L2_CID_ */
};

#define C(l) PARENT (l, struct control, pub)


struct tveng25_vbuf
{
  void * vmem; /* Captured image in this buffer */
  struct v4l2_buffer vidbuf; /* Info about the buffer */
};

struct private_tveng25_device_info
{
  tveng_device_info info; /* Info field, inherited */
  int num_buffers; /* Number of mmaped buffers */
  struct tveng25_vbuf * buffers; /* Array of buffers */
  double last_timestamp; /* The timestamp of the last captured buffer */
  uint32_t chroma;
  int audio_mode; /* 0 mono */
	tv_control *		mute;
	unsigned		read_back_controls	: 1;
};

#define P_INFO(p) PARENT (p, struct private_tveng25_device_info, info)




/*
 *  Controls
 */

/* Preliminary */
#define TVENG25_AUDIO_MAGIC 0x1234

static tv_bool
do_update_control		(struct private_tveng25_device_info *p_info,
				 struct control *	c)
{
	struct v4l2_control ctrl;

	if (c->id == TVENG25_AUDIO_MAGIC) {
		/* FIXME actual value */
		c->pub.value = p_info->audio_mode;
		return TRUE;
	} else if (c->id == V4L2_CID_AUDIO_MUTE) {
		/* Doesn't seem to work with bttv.
		   FIXME we should check at runtime. */
		return TRUE;
	}

	ctrl.id = c->id;

	if (-1 == v4l25_ioctl (&p_info->info, VIDIOC_G_CTRL, &ctrl))
		return FALSE;

	if (c->pub.value != ctrl.value) {
		c->pub.value = ctrl.value;
		tv_callback_notify (&c->pub, c->pub._callback);
	}

	return TRUE;
}

static tv_bool
update_control			(tveng_device_info *	info,
				 tv_control *		c)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);

	if (c)
		return do_update_control (p_info, C(c));

	for_all (c, p_info->info.controls)
		if (c->_parent == info)
			if (!do_update_control (p_info, C(c)))
				return FALSE;

	return TRUE;
}

static tv_bool
set_control			(tveng_device_info *	info,
				 tv_control *		c,
				 int			value)
{
	struct private_tveng25_device_info * p_info = P_INFO (info);
	struct v4l2_control ctrl;

	if (C(c)->id == TVENG25_AUDIO_MAGIC) {
		if (IS_TUNER_LINE (info->cur_video_input)) {
			struct v4l2_tuner tuner;

			CLEAR (tuner);
			tuner.index = VI(info->cur_video_input)->tuner;

			/* XXX */
			if (0 == v4l25_ioctl(info, VIDIOC_G_TUNER, &tuner)) {
				tuner.audmode = "\0\1\3\2"[value];

				if (0 == v4l25_ioctl(info, VIDIOC_S_TUNER, &tuner)) {
					p_info->audio_mode = value;
				}
			}
		} else {
			p_info->audio_mode = 0; /* mono */
		}

		return TRUE;
	}

	ctrl.id = C(c)->id;
	ctrl.value = value;

	if (-1 == v4l25_ioctl(info, VIDIOC_S_CTRL, &ctrl))
		return FALSE;

	if (p_info->read_back_controls) {
		/*
		  Doesn't seem to work with bttv.
		  FIXME we should check at runtime.
		*/
		if (ctrl.id == V4L2_CID_AUDIO_MUTE) {
			if (c->value != value) {
				c->value = value;
				tv_callback_notify (c, c->_callback);
			}
			return TRUE;
		}

		return do_update_control (p_info, C(c));
	} else {
		if (c->value != value) {
			c->value = value;
			tv_callback_notify (c, c->_callback);
		}

		return TRUE;
	}
}

static const struct {
	unsigned int		cid;
	const char *		label;
	tv_control_id		id;
} controls [] = {
	{ V4L2_CID_BRIGHTNESS,		N_("Brightness"),	TV_CONTROL_ID_BRIGHTNESS },
	{ V4L2_CID_CONTRAST,		N_("Contrast"),		TV_CONTROL_ID_CONTRAST },
	{ V4L2_CID_SATURATION,		N_("Saturation"),	TV_CONTROL_ID_SATURATION },
	{ V4L2_CID_HUE,			N_("Hue"),		TV_CONTROL_ID_HUE },
	{ V4L2_CID_WHITENESS,		N_("Whiteness"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_BLACK_LEVEL,		N_("Black level"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUTO_WHITE_BALANCE,	N_("White balance"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_DO_WHITE_BALANCE,	N_("White balance"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_RED_BALANCE,		N_("Red balance"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_BLUE_BALANCE,	N_("Blue balance"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_GAMMA,		N_("Gamma"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_EXPOSURE,		N_("Exposure"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUTOGAIN,		N_("Auto gain"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_GAIN,		N_("Gain"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_HCENTER,		N_("HCenter"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_VCENTER,		N_("VCenter"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_HFLIP,		N_("Hor. flipping"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_VFLIP,		N_("Vert. flipping"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUDIO_VOLUME,	N_("Volume"),		TV_CONTROL_ID_VOLUME },
	{ V4L2_CID_AUDIO_MUTE,		N_("Mute"),		TV_CONTROL_ID_MUTE },
	{ V4L2_CID_AUDIO_MUTE,		N_("Audio Mute"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUDIO_BALANCE,	N_("Balance"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUDIO_BALANCE,	N_("Audio Balance"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUDIO_TREBLE,	N_("Treble"),		TV_CONTROL_ID_TREBLE },
	{ V4L2_CID_AUDIO_LOUDNESS, 	N_("Loudness"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUDIO_BASS,		N_("Bass"),		TV_CONTROL_ID_BASS },
};

static const char *
audio_decoding_modes [] = {
	N_("Automatic"),
	N_("Mono"),
	N_("Stereo"),
	N_("Alternate 1"),
	N_("Alternate 2"),
};

static tv_bool
add_control			(tveng_device_info *	info,
				 unsigned int		id)
{
	struct private_tveng25_device_info * p_info = P_INFO (info);
	struct v4l2_queryctrl qc;
	struct v4l2_querymenu qm;
	struct control *c;
	tv_control *tc;
	unsigned int i, j;

	CLEAR (qc);

	qc.id = id;

	/* XXX */
	if (-1 == v4l25_ioctl_nf (info, VIDIOC_QUERYCTRL, &qc)) {
		if (EINVAL != errno) 
			ioctl_failure (info, __FILE__, __PRETTY_FUNCTION__,
				       __LINE__, "VIDIOC_QUERYCTRL");
		return FALSE;
	}

	if (qc.flags & (V4L2_CTRL_FLAG_DISABLED | V4L2_CTRL_FLAG_GRABBED))
		return TRUE;

	if (!(c = calloc (1, sizeof (*c))))
		goto failure;

	for (i = 0; i < N_ELEMENTS (controls); i++)
		if (qc.id == controls[i].cid)
			break;

	if (i < N_ELEMENTS (controls)) {
		c->pub.label = strdup (_(controls[i].label));
		c->pub.id = controls[i].id;
	} else {
		c->pub.label = strndup (qc.name, 32);
		c->pub.id = TV_CONTROL_ID_UNKNOWN;
	}

	if (!c->pub.label)
		goto failure;

	c->pub.minimum	= qc.minimum;
	c->pub.maximum	= qc.maximum;
	c->pub.step	= qc.step;
	c->pub.reset	= qc.default_value;
	c->id		= qc.id;

	switch (qc.type) {
	case V4L2_CTRL_TYPE_INTEGER:
		c->pub.type = TV_CONTROL_TYPE_INTEGER;
		break;

	case V4L2_CTRL_TYPE_BOOLEAN:
		c->pub.type = TV_CONTROL_TYPE_BOOLEAN;
		/* Yeah, some drivers suck. */
		c->pub.minimum	= 0;
		c->pub.maximum	= 1;
		c->pub.step	= 1;
		break;

	case V4L2_CTRL_TYPE_BUTTON:
		c->pub.type = TV_CONTROL_TYPE_ACTION;
		c->pub.minimum	= INT_MIN;
		c->pub.maximum	= INT_MAX;
		c->pub.step	= 0;
		c->pub.reset	= 0;
		break;

	case V4L2_CTRL_TYPE_MENU:
		c->pub.type = TV_CONTROL_TYPE_CHOICE;

		if (!(c->pub.menu = calloc (qc.maximum + 2, sizeof (char *))))
			goto failure;

		for (j = 0; j <= qc.maximum; j++) {
			qm.id = qc.id;
			qm.index = j;

			if (0 == v4l25_ioctl (info, VIDIOC_QUERYMENU, &qm)) {
				if (!(c->pub.menu[j] = strndup(_(qm.name), 32)))
					goto failure;
			} else {
				goto failure;
			}
		}

	      break;

	default:
		fprintf(stderr, "V4L2: Unknown control type 0x%x (%s)\n", qc.type, qc.name);
		goto failure;
	}

	if (!(tc = append_control (info, &c->pub, 0))) {
	failure:
		free_control (&c->pub);
		return TRUE; /* no control, but not end of enum */
	}

	do_update_control (p_info, C(tc));

	if (qc.id == V4L2_CID_AUDIO_MUTE) {
		p_info->mute = tc;
	}

	return TRUE;
}

static tv_bool
update_control_list		(tveng_device_info *	info)
{
	unsigned int cid;

	free_controls (info);

	for (cid = V4L2_CID_BASE; cid < V4L2_CID_LASTP1; ++cid) {
		/* EINVAL ignored */
		add_control (info, cid);
	}

	/* Artificial control (preliminary) */

	if (IS_TUNER_LINE (info->cur_video_input)) {
		struct v4l2_tuner tuner;

		memset(&tuner, 0, sizeof(tuner));
		tuner.index = VI(info->cur_video_input)->tuner;

		if (v4l25_ioctl(info, VIDIOC_G_TUNER, &tuner) == 0) {
			/* NB this is not implemented in bttv 0.8.45 */
			if (tuner.capability & (V4L2_TUNER_CAP_STEREO
						| V4L2_TUNER_CAP_LANG2)) {
				struct control *c;
				unsigned int i;

				/* XXX this needs refinement */

				if (!(c = calloc (1, sizeof (*c))))
					goto failure;

				if (!(c->pub.label = strdup (_("Audio"))))
					goto failure;

				c->id = TVENG25_AUDIO_MAGIC;
				c->pub.type = TV_CONTROL_TYPE_CHOICE;
				c->pub.maximum = 4;

				if (!(c->pub.menu = calloc (6, sizeof (char *))))
					goto failure;

				for (i = 0; i < 5; i++) {
					if (!(c->pub.menu[i] = strdup (_(audio_decoding_modes[i]))))
						goto failure;
				}

				if (!append_control (info, &c->pub, 0)) {
				failure:
					free_control (&c->pub);
				}

				do_update_control (P_INFO(info), c);
			}
		}
	}

	for (cid = V4L2_CID_PRIVATE_BASE;
	     cid < V4L2_CID_PRIVATE_BASE + 100; ++cid) {
		if (!add_control (info, cid))
			break; /* end of enumeration */
	}

	return TRUE;
}

/*
 *  Video standards
 */

static tv_bool
update_standard			(tveng_device_info *	info)
{
	v4l2_std_id std_id;
	tv_video_standard *s;

	if (!info->video_standards)
		return TRUE;

	if (-1 == v4l25_ioctl (info, VIDIOC_G_STD, &std_id))
		return FALSE;

	for_all (s, info->video_standards)
		if (s->id == std_id)
			break;

	/* s = NULL = unknown. */

	store_cur_video_standard (info, s);

	return TRUE;
}

static tv_bool
set_standard			(tveng_device_info *	info,
				 const tv_video_standard *s)
{
  enum tveng_capture_mode current_mode;
  enum tveng_frame_pixformat pixformat;
  int r;

  pixformat = info->format.pixformat;
  current_mode = tveng_stop_everything(info);

	r = v4l25_ioctl (info, VIDIOC_S_STD, &s->id);

	if (0 == r)
		store_cur_video_standard (info, s);

// XXX bad idea
  info->format.pixformat = pixformat;
  tveng_set_capture_format(info);
  tveng_restart_everything(current_mode, info);

  return (0 == r);
}

static tv_bool
update_standard_list		(tveng_device_info *	info)
{
	unsigned int i;

	free_video_standards (info);

	if (!info->cur_video_input)
		return TRUE;

	for (i = 0;; ++i) {
		struct v4l2_standard standard;
		tv_video_standard *s;

		CLEAR (standard);

		standard.index = i;

		if (-1 == v4l25_ioctl (info, VIDIOC_ENUMSTD, &standard)) {
			if (errno == EINVAL && i > 0)
				break; /* end of enumeration */

			ioctl_failure (info, __FILE__, __PRETTY_FUNCTION__,
				       __LINE__, "VIDIOC_ENUMSTD");

			goto failure;
		}

		/* FIXME check if the standard is supported by the current input */

		if (!(s = append_video_standard (&info->video_standards,
						 standard.id,
						 standard.name,
						 standard.name,
						 sizeof (*s))))
			goto failure;

		s->frame_rate =
			standard.frameperiod.denominator
			/ (double) standard.frameperiod.numerator;
	}

	if (update_standard (info))
		return TRUE;

 failure:
	free_video_standard_list (&info->video_standards);
	return FALSE;
}

/*
 *  Video inputs
 */

#define SCALE_FREQUENCY(vi, freq)					\
	((((freq) & ~ (unsigned long) vi->step_shift)			\
	   * vi->pub.u.tuner.step) >> vi->step_shift)

static void
store_frequency			(struct video_input *	vi,
				 unsigned long		freq)
{
	unsigned int frequency = SCALE_FREQUENCY (vi, freq);

	if (vi->pub.u.tuner.frequency != frequency) {
		vi->pub.u.tuner.frequency = frequency;
		tv_callback_notify (&vi->pub, vi->pub._callback);
	}
}

static tv_bool
update_tuner_frequency		(tveng_device_info *	info,
				 tv_video_line *	l)
{
	struct v4l2_frequency vfreq;

	vfreq.tuner = VI (l)->tuner;
	vfreq.type = V4L2_TUNER_ANALOG_TV;

	if (-1 == v4l25_ioctl (info, VIDIOC_G_FREQUENCY, &vfreq))
		return FALSE;

	store_frequency (VI (l), vfreq.frequency);

	return TRUE;
}

static tv_bool
set_tuner_frequency		(tveng_device_info *	info,
				 tv_video_line *	l,
				 unsigned int		frequency)
{
	struct video_input *vi = VI (l);
	struct v4l2_frequency vfreq;

	CLEAR (vfreq);

	vfreq.tuner = vi->tuner;
	vfreq.type = V4L2_TUNER_ANALOG_TV;

	vfreq.frequency = (frequency << vi->step_shift) / vi->pub.u.tuner.step;

	if (-1 == v4l25_ioctl (info, VIDIOC_S_FREQUENCY, &vfreq))
		return FALSE;

	store_frequency (vi, vfreq.frequency);

	return TRUE;
}

static tv_bool
update_video_input		(tveng_device_info *	info)
{
	tv_video_line *l;

	if (info->video_inputs) {
		int index;

		if (-1 == v4l25_ioctl (info, VIDIOC_G_INPUT, &index))
			return FALSE;

		if (info->cur_video_input)
			if (VI (info->cur_video_input)->index == index)
				return TRUE;

		for_all (l, info->video_inputs)
			if (VI (l)->index == index)
				break;
	} else {
		l = NULL;

		if (info->cur_video_input == NULL)
			return TRUE;
	}

	info->cur_video_input = l;
	tv_callback_notify (info, info->priv->video_input_callback);

	if (l)
		update_standard_list (info);
	else
		free_video_standards (info);

	return TRUE;
}

static tv_bool
set_video_input			(tveng_device_info *	info,
				 const tv_video_line *	l)
{
	enum tveng_capture_mode current_mode;
	enum tveng_frame_pixformat pixformat;

	if (info->cur_video_input) {
		int index;

		if (0 == v4l25_ioctl (info, VIDIOC_G_INPUT, &index))
			if (VI (l)->index == index)
				return TRUE;
	}

	pixformat = info->format.pixformat;
	current_mode = tveng_stop_everything(info);

	if (-1 == v4l25_ioctl (info, VIDIOC_S_INPUT, &VI (l)->index))
		return FALSE;

	store_cur_video_input (info, l);

	// XXX error?
	update_standard_list (info);

	info->format.pixformat = pixformat;
	tveng_set_capture_format(info);

	/* XXX Start capturing again as if nothing had happened */
	tveng_restart_everything (current_mode, info);

	return TRUE;
}

static tv_bool
tuner_bounds			(tveng_device_info *	info,
				 struct video_input *	vi)
{
	struct v4l2_tuner tuner;

	tuner.index = vi->tuner;

	if (-1 == v4l25_ioctl (info, VIDIOC_G_TUNER, &tuner))
		return FALSE;

	t_assert (tuner.rangelow <= tuner.rangehigh);

	if (tuner.capability & V4L2_TUNER_CAP_LOW) {
		/* Actually step is 62.5 Hz, but why
		   unecessarily complicate things. */
		vi->pub.u.tuner.step = 125;
		vi->step_shift = 1;

		/* A check won't do, some drivers report (0, INT_MAX). */
		tuner.rangelow = MIN (tuner.rangelow, UINT_MAX / 125);
		tuner.rangehigh = MIN (tuner.rangehigh, UINT_MAX / 125);
	} else {
		vi->pub.u.tuner.step = 62500;
		vi->step_shift = 0;

		tuner.rangelow = MIN (tuner.rangelow, UINT_MAX / 62500);
		tuner.rangehigh = MIN (tuner.rangehigh, UINT_MAX / 62500);
	}

	vi->pub.u.tuner.minimum = SCALE_FREQUENCY (vi, tuner.rangelow);
	vi->pub.u.tuner.maximum = SCALE_FREQUENCY (vi, tuner.rangehigh);

	return update_tuner_frequency (info, &vi->pub);
}

static tv_bool
update_video_input_list		(tveng_device_info *	info)
{
	unsigned int i;

	free_video_inputs (info);

	for (i = 0;; ++i) {
		struct v4l2_input input;
		char buf[sizeof (input.name) + 1];
		struct video_input *vi;
		tv_video_line_type type;

		CLEAR (input);

		input.index = i;

		if (-1 == v4l25_ioctl (info, VIDIOC_ENUMINPUT, &input)) {
			if (errno == EINVAL && i > 0)
				break; /* end of enumeration */

			ioctl_failure (info, __FILE__, __PRETTY_FUNCTION__,
				       __LINE__, "VIDIOC_ENUMINPUT");

			free_video_line_list (&info->video_inputs);

			return FALSE;
		}

		/* Sometimes NUL is missing. */
		z_strlcpy (buf, input.name, sizeof (buf));

		if (input.type & V4L2_INPUT_TYPE_TUNER)
			type = TV_VIDEO_LINE_TYPE_TUNER;
		else
			type = TV_VIDEO_LINE_TYPE_BASEBAND;

		if (!(vi = VI(append_video_line (&info->video_inputs,
						 type, buf, buf,
						 sizeof (*vi)))))
			goto failure;

		vi->pub._parent = info;

		vi->index = input.index;
		vi->tuner = input.tuner;

		if (input.type & V4L2_INPUT_TYPE_TUNER)
			if (!tuner_bounds (info, vi))
				goto failure;
	}

	update_video_input (info);

	return TRUE;

 failure:
	free_video_line_list (&info->video_inputs);
	return FALSE;
}

#if 0
if (input.audioset != 0)
info->inputs[i].flags |= TVENG_INPUT_AUDIO;
#endif

/*
 *  Overlay
 */

static tv_bool
get_overlay_buffer		(tveng_device_info *	info,
				 tv_overlay_buffer *	t)
{
	struct v4l2_framebuffer fb;
  
	if (!(info->caps.flags & TVENG_CAPS_OVERLAY))
		return FALSE;

	if (-1 == v4l25_ioctl (info, VIDIOC_G_FBUF, &fb))
		return FALSE;

	// XXX fb.capability, fb.flags ignored

	t->base			= fb.base;

	t->bytes_per_line	= fb.fmt.bytesperline;
	t->size			= fb.fmt.sizeimage;

	t->width		= fb.fmt.width;
	t->height		= fb.fmt.height;

	if (t->size == 0) /* huh? */
		t->size = fb.fmt.bytesperline * fb.fmt.height;

	switch (fb.fmt.pixelformat) {
	case 0:
		CLEAR (*t);
		break;

		// XXX inexact
	case V4L2_PIX_FMT_RGB555:
		t->pixfmt = TVENG_PIX_RGB555;
		//		t->depth = 15;
		break;
	case V4L2_PIX_FMT_RGB565:
		t->pixfmt = TVENG_PIX_RGB565;
		//		t->depth = 16;
		break;
	case V4L2_PIX_FMT_RGB24:
		t->pixfmt = TVENG_PIX_RGB24;
		//		t->depth = 24;
		break;
	case V4L2_PIX_FMT_BGR24:
		t->pixfmt = TVENG_PIX_BGR24;
		//		t->depth = 24;
		break;
	case V4L2_PIX_FMT_RGB32:
		t->pixfmt = TVENG_PIX_RGB32;
		//		t->depth = 32;
		break;
	case V4L2_PIX_FMT_BGR32:
		t->pixfmt = TVENG_PIX_BGR32;
		//		t->depth = 32;
		break;
	case V4L2_PIX_FMT_RGB332:
	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_RGB565X:
	default:
		CLEAR (*t);
		return FALSE;
	}

	//	t->bits_per_pixel	= (t->depth + 7) & -8;

	return TRUE;
}

/*
  Sets the preview window dimensions to the given window.
  Returns -1 on error, something else on success.
  Success doesn't mean that the requested dimensions are used, maybe
  they are different, check the returned fields to see if they are suitable
  info   : Device we are controlling
*/
static int
tveng25_set_preview_window(tveng_device_info * info)
{
	struct private_tveng25_device_info * p_info = P_INFO (info);
	struct v4l2_format format;

t_assert(info != NULL);

	CLEAR (format);

	format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;

	format.fmt.win.w.left	 = info->overlay_window.x;
	format.fmt.win.w.top	 = info->overlay_window.y;
	format.fmt.win.w.width	 = info->overlay_window.width;
	format.fmt.win.w.height	 = info->overlay_window.height;
	format.fmt.win.clipcount = info->overlay_window.clip_vector.size;
	format.fmt.win.chromakey = p_info->chroma;

	if (info->overlay_window.clip_vector.size > 0) {
		struct v4l2_clip *vclip;
		const tv_clip *tclip;
		unsigned int i;

		vclip =	calloc (info->overlay_window.clip_vector.size,
				sizeof (*vclip));
		if (!vclip) {
			info->tveng_errno = errno;
			t_error("malloc", info);
			return -1;
		}

		format.fmt.win.clips = vclip;
		tclip = info->overlay_window.clip_vector.vector;

		for (i = 0; i < info->overlay_window.clip_vector.size; ++i) {
			vclip->next	= vclip + 1;
			vclip->c.left	= tclip->x;
			vclip->c.top	= tclip->y;
			vclip->c.width	= tclip->width;
			vclip->c.height	= tclip->height;
			++vclip;
			++tclip;
		}

		vclip[-1].next = NULL;
	}

	tveng_set_preview_off (info);

	/* Set the new window */
	if (-1 == v4l25_ioctl (info, VIDIOC_S_FMT, &format)) {
		free (format.fmt.win.clips);
		return -1;
	}

	free (format.fmt.win.clips);

	if (-1 == v4l25_ioctl (info, VIDIOC_G_FMT, &format))
		return -1;

	info->overlay_window.x		= format.fmt.win.w.left;
	info->overlay_window.y		= format.fmt.win.w.top;
	info->overlay_window.width	= format.fmt.win.w.width;
	info->overlay_window.height	= format.fmt.win.w.height;

	/* Clips cannot be read back, we assume no change. */

	return 0;
}

/*
  Gets the current overlay window parameters.
  Returns -1 on error, and any other value on success.
  info   : The device to use
*/
static int
tveng25_get_preview_window(tveng_device_info * info)
{
  /* Updates the entire capture format, since there is no
     difference */
  return (tveng25_update_capture_format(info));
}

static tv_bool
set_overlay			(tveng_device_info *	info,
				 tv_bool		on)
{
	int value = on;

	if (-1 == v4l25_ioctl (info, VIDIOC_OVERLAY, &value))
		return FALSE;

	return TRUE;
}



static void
tveng25_set_chromakey		(uint32_t chroma, tveng_device_info *info)
{
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;

  p_info->chroma = chroma;

  /* Will be set in the next set_window call */
}

static int
tveng25_get_chromakey		(uint32_t *chroma, tveng_device_info *info)
{
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;

  *chroma = p_info->chroma;

  return 0;
}







static int p_tveng25_open_device_file(int flags, tveng_device_info * info);
/*
  Return fd for the device file opened. Checks if the device is a
  valid video device. -1 on error.
  Flags will be used for open()'ing the file 
*/
static int p_tveng25_open_device_file(int flags, tveng_device_info * info)
{
  struct v4l2_capability caps;
  struct v4l2_framebuffer fb;
  extern int disable_overlay;

  t_assert(info != NULL);
  t_assert(info->file_name != NULL);

  info -> fd = device_open(info->log_fp, info -> file_name, flags, 0);
  if (info -> fd < 0)
    {
      info->tveng_errno = errno;
      t_error("open()", info);
      return -1;
    }

  /* We check the capabilities of this video device */
  CLEAR (caps);
  CLEAR (fb);

  if (-1 == v4l25_ioctl (info, VIDIOC_QUERYCAP, &caps))
    {
      device_close(info->log_fp, info->fd);
      return -1;
    }

  /* Check if this device is convenient for us */
  if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
      info->tveng_errno = -1;
      snprintf(info->error, 256, 
	       _("%s doesn't look like a valid capture device"), info
	       -> file_name);
      device_close(info->log_fp, info->fd);
      return -1;
    }

  /* Check if we can mmap() this device */
  /* XXX REQBUFS must be checked too */
  if (!(caps.capabilities & V4L2_CAP_STREAMING))
    {
      info -> tveng_errno = -1;
      snprintf(info->error, 256,
	       _("Sorry, but \"%s\" cannot do streaming"),
	       info -> file_name);
      device_close(info->log_fp, info->fd);
      return -1;
    }

  /* Copy capability info */
  snprintf(info->caps.name, 32, caps.card);
  /* XXX get these elsewhere */
  info->caps.channels = 0; /* FIXME video inputs */
  info->caps.audios = 0;
  info->caps.maxwidth = 768;
  info->caps.minwidth = 16;
  info->caps.maxheight = 576;
  info->caps.minheight = 16;
  info->caps.flags = 0;

  info->caps.flags |= TVENG_CAPS_CAPTURE; /* This has been tested before */

  if (caps.capabilities & V4L2_CAP_TUNER)
    info->caps.flags |= TVENG_CAPS_TUNER;
  if (caps.capabilities & V4L2_CAP_VBI_CAPTURE)
    info->caps.flags |= TVENG_CAPS_TELETEXT;
  /* XXX get elsewhere */
  if (caps.capabilities & 0)
    info->caps.flags |= TVENG_CAPS_MONOCHROME;

  if (!disable_overlay && (caps.capabilities & V4L2_CAP_VIDEO_OVERLAY))
    {
      info->caps.flags |= TVENG_CAPS_OVERLAY;

      /* Collect more info about the overlay mode */
      if (v4l25_ioctl (info, VIDIOC_G_FBUF, &fb) != 0)
	{
	  if (fb.capability & V4L2_FBUF_CAP_CHROMAKEY)
	    info->caps.flags |= TVENG_CAPS_CHROMAKEY;

	  if (fb.capability & (V4L2_FBUF_CAP_LIST_CLIPPING
			       | V4L2_FBUF_CAP_BITMAP_CLIPPING))
	    info->caps.flags |= TVENG_CAPS_CLIPPING;

	  if (!(fb.capability & V4L2_FBUF_CAP_EXTERNOVERLAY))
	    info->caps.flags |= TVENG_CAPS_FRAMERAM;

	  /* XXX get elsewhere
	  if ((fb.flags & V4L2_FBUF_CAP_SCALEUP) ||
	      (fb.flags & V4L2_FBUF_CAP_SCALEDOWN))
	      info->caps.flags |= TVENG_CAPS_SCALES; */
	}
    }

  info -> current_controller = TVENG_CONTROLLER_V4L2;
  
  /* Everything seems to be OK with this device */
  return (info -> fd);
}

static int
set_capture_format(tveng_device_info * info);

/*
  Associates the given tveng_device_info with the given video
  device. On error it returns -1 and sets info->tveng_errno, info->error to
  the correct values.
  device_file: The file used to access the video device (usually
  /dev/video)
  attach_mode: Specifies the mode to open the device file
  depth: The color depth the capture will be in, -1 means let tveng
  decide based on the current display depth.
  info: The structure to be associated with the device
*/
static
int tveng25_attach_device(const char* device_file,
			 enum tveng_attach_mode attach_mode,
			 tveng_device_info * info)
{
  int error;
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;

  t_assert(device_file != NULL);
  t_assert(info != NULL);

  if (info -> fd) /* If the device is already attached, detach it */
    tveng_close_device(info);

  info->audio_mutable = 0;

  info -> file_name = strdup(device_file);
  if (!(info -> file_name))
    {
      info -> tveng_errno = errno;
      t_error("strdup()", info);
      return -1;
    }

  if (attach_mode == TVENG_ATTACH_XV)
    attach_mode = TVENG_ATTACH_READ;

  switch (attach_mode)
    {
    case TVENG_ATTACH_CONTROL:
      info -> fd = p_tveng25_open_device_file(0, info);
      break;

    case TVENG_ATTACH_READ:
      info -> fd = p_tveng25_open_device_file(O_RDWR, info);
      break;
    default:
      t_error_msg("switch()", "Unknown attach mode for the device",
		  info);
      free(info->file_name);
      info->file_name = NULL;
      return -1;
    };

  /*
    Errors (if any) are already aknowledged when we reach this point,
    so we don't show them again
  */
  if (info -> fd < 0)
    {
      free(info->file_name);
      info->file_name = NULL;
      return -1;
    }
  
  info -> attach_mode = attach_mode;
  /* Current capture mode is no capture at all */
  info -> current_mode = TVENG_NO_CAPTURE;

  /* We have a valid device, get some info about it */

	/* Video inputs & standards */

	info->video_inputs = NULL;
	info->cur_video_input = NULL;

	info->video_standards = NULL;
	info->cur_video_standard = NULL;

	// XXX error
	update_video_input_list (info);

  /* Query present controls */
  info->controls = NULL;
  if (!update_control_list (info))
      return -1;

  /* Set up the palette according to the one present in the system */
  error = info->priv->current_bpp;

  if (error == -1)
    {
      tveng25_close_device(info);
      return -1;
    }

  switch(error)
    {
    case 15:
      info->format.pixformat = TVENG_PIX_RGB555;
      break;
    case 16:
      info->format.pixformat = TVENG_PIX_RGB565;
      break;
    case 24:
      info->format.pixformat = TVENG_PIX_BGR24;
      break;
    case 32:
      info->format.pixformat = TVENG_PIX_BGR32;
      break;
    default:
      info -> tveng_errno = -1;
      t_error_msg("switch()", 
		  "Cannot find appropiate palette for current display",
		  info);
      tveng25_close_device(info);
      return -1;
    }

  /* Get overlay_buffer */
  get_overlay_buffer (info, &info->overlay_buffer);

  /* Pass some dummy values to the driver, so g_win doesn't fail */
  CLEAR (info->overlay_window);

  info->overlay_window.width = info->overlay_window.height = 16;

  tveng_set_preview_window(info);

  /* Set our desired size, make it halfway */
  info -> format.width = (info->caps.minwidth + info->caps.maxwidth)/2;
  info -> format.height = (info->caps.minheight +
			   info->caps.maxheight)/2;

  /* Set some capture format (not important) */
  set_capture_format(info);

  /* Init the private info struct */
  p_info->num_buffers = 0;
  p_info->buffers = NULL;

  return info -> fd;
}

/*
  Stores in short_str and long_str (if they are non-null) the
  description of the current controller. The enum value can be found in
  info->current_controller.
  For example, V4L2 controller would say:
  short_str: 'V4L2'
  long_str: 'Video4Linux 2'
  info->current_controller: TVENG_CONTROLLER_V4L2
  This function always succeeds.
*/
static void
tveng25_describe_controller(char ** short_str, char ** long_str,
			   tveng_device_info * info)
{
  t_assert(info != NULL);
  if (short_str)
    *short_str = "V4L25";
  if (long_str)
    *long_str = "Video4Linux 2.5";
}

/* Closes a device opened with tveng_init_device */
static void tveng25_close_device(tveng_device_info * info)
{
  tveng_stop_everything(info);

  device_close(info->log_fp, info->fd);
  info -> fd = 0;
  info -> current_controller = TVENG_CONTROLLER_NONE;

  if (info -> file_name)
    {
      free(info -> file_name);
      info->file_name = NULL;
    }

	free_controls (info);
	free_video_standards (info);
	free_video_inputs (info);

  info->file_name = NULL;
}

/*
  Functions for controlling the video capture. All of them return -1
  in case of error, so any value != -1 should be considered valid
  (unless explicitly stated in the description of the function) 
*/

static int
tveng25_update_capture_format(tveng_device_info * info)
{
  struct v4l2_format format;

  t_assert(info != NULL);

  CLEAR (format);

  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (-1 == v4l25_ioctl (info, VIDIOC_G_FMT, &format))
      return -1;

  switch (format.fmt.pix.pixelformat)
    {
    case V4L2_PIX_FMT_RGB555:
      info->format.depth = 15;
      info->format.bpp = 2;
      info->format.pixformat = TVENG_PIX_RGB555;
      break;
    case V4L2_PIX_FMT_RGB565:
      info->format.depth = 16;
      info->format.bpp = 2;
      info->format.pixformat = TVENG_PIX_RGB565;
      break;
    case V4L2_PIX_FMT_RGB24:
      info->format.depth = 24;
      info->format.bpp = 3;
      info->format.pixformat = TVENG_PIX_RGB24;
      break;
    case V4L2_PIX_FMT_BGR24:
      info->format.depth = 24;
      info->format.bpp = 3;
      info->format.pixformat = TVENG_PIX_BGR24;
      break;
    case V4L2_PIX_FMT_RGB32:
      info->format.depth = 32;
      info->format.bpp = 4;
      info->format.pixformat = TVENG_PIX_RGB32;
      break;
    case V4L2_PIX_FMT_BGR32:
      info->format.depth = 32;
      info->format.bpp = 4;
      info->format.pixformat = TVENG_PIX_BGR32;
      break;
    case V4L2_PIX_FMT_YVU420:
      info->format.depth = 12;
      info->format.bpp = 1.5;
      info->format.pixformat = TVENG_PIX_YVU420;
      break;
#ifdef V4L2_PIX_FMT_YUV420 /* not in the spec, but very common */
    case V4L2_PIX_FMT_YUV420:
      info->format.depth = 12;
      info->format.bpp = 1.5;
      info->format.pixformat = TVENG_PIX_YUV420;
      break;
#endif
    case V4L2_PIX_FMT_UYVY:
      info->format.depth = 16;
      info->format.bpp = 2;
      info->format.pixformat = TVENG_PIX_UYVY;
      break;
    case V4L2_PIX_FMT_YUYV:
      info->format.depth = 16;
      info->format.bpp = 2;
      info->format.pixformat = TVENG_PIX_YUYV;
      break;
    case V4L2_PIX_FMT_GREY:
      info->format.depth = 8;
      info->format.bpp = 1;
      info->format.pixformat = TVENG_PIX_GREY;
      break;
    default:
      info->tveng_errno = -1; /* unknown */
      t_error_msg("switch()",
		  "Cannot understand the actual palette", info);
      return -1;    
    };

  info->format.width = format.fmt.pix.width;
  info->format.height = format.fmt.pix.height;

  /* bttv */
  format.fmt.pix.bytesperline =
    MAX ((unsigned int)(format.fmt.pix.width * info->format.bpp),
	 format.fmt.pix.bytesperline);

  info->format.bytesperline = format.fmt.pix.bytesperline;
  info->format.sizeimage = format.fmt.pix.sizeimage;

  CLEAR (format);

  format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;

  /* mhs: moved down here because tveng25_read_frame blamed
     info -> format.sizeimage != size after G_WIN failed */
  if (-1 == v4l25_ioctl (info, VIDIOC_G_FMT, &format))
      return -1;

  info->overlay_window.x	= format.fmt.win.w.left;
  info->overlay_window.y	= format.fmt.win.w.top;
  info->overlay_window.width	= format.fmt.win.w.width;
  info->overlay_window.height	= format.fmt.win.w.height;
  /* These two are defined as write-only */
  info->overlay_window.clip_vector.vector = NULL;
  info->overlay_window.clip_vector.size = 0;
  info->overlay_window.clip_vector.capacity = 0;

  return 0;
}

/* -1 if failed. Sets the pixformat and fills in info -> pix_format
   with the correct values  */
static int
set_capture_format(tveng_device_info * info)
{
  struct v4l2_format format;

  t_assert(info != NULL);

  CLEAR (format);

  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  /* Transform the given palette value into a V4L value */
  switch(info->format.pixformat)
    {
    case TVENG_PIX_RGB555:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB555;
      break;
    case TVENG_PIX_RGB565:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
      break;
    case TVENG_PIX_RGB24:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
      break;
    case TVENG_PIX_BGR24:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
      break;
    case TVENG_PIX_RGB32:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB32;
      break;
    case TVENG_PIX_BGR32:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR32;
      break;
    case TVENG_PIX_YUV420:
#ifdef V4L2_PIX_FMT_YUV420 /* not in the spec, but very common */
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
      break;
#endif
    case TVENG_PIX_YVU420:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_YVU420;
      break;
    case TVENG_PIX_UYVY:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
      break;
    case TVENG_PIX_YUYV:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
      break;
    default:
      info->tveng_errno = -1; /* unknown */
      t_error_msg("switch()", "Cannot understand the given palette",
		  info);
      return -1;
    }

  /* Adjust the given dimensions */
  /* FIXME
  if (info->format.height < info->caps.minheight)
    info->format.height = info->caps.minheight;

  if (info->format.height > info->caps.maxheight)
    info->format.height = info->caps.maxheight;

  if (info->format.width < info->caps.minwidth)
    info->format.width = info->caps.minwidth;

  if (info->format.width > info->caps.maxwidth)
    info->format.width = info->caps.maxwidth;
  */
  
  format.fmt.pix.width = info->format.width;
  format.fmt.pix.height = info->format.height;

  format.fmt.pix.bytesperline = 0; /* minimum please */
  format.fmt.pix.sizeimage = 0; /* ditto */

  /* XXX */
  if (format.fmt.pix.height > 288)
    format.fmt.pix.field = V4L2_FIELD_INTERLACED;
  else
    format.fmt.pix.field = V4L2_FIELD_TOP;

  if (-1 == v4l25_ioctl (info, VIDIOC_S_FMT, &format))
      return -1;

  /* Check fill in info with the current values (may not be the ones
     requested) */
  tveng25_update_capture_format(info);

  return 0; /* Success */
}

static int
tveng25_set_capture_format(tveng_device_info * info)
{
  enum tveng_capture_mode current_mode;
  enum tveng_frame_pixformat pixformat;

  pixformat = info->format.pixformat;
  current_mode = tveng_stop_everything(info);
  info->format.pixformat = pixformat;

  set_capture_format(info);

  /* Start capturing again as if nothing had happened */
  tveng_restart_everything(current_mode, info);

  return 0; /* Success */
}


















/*
  Gets the signal strength and the afc code. The afc code indicates
  how to get a better signal, if negative, tune higher, if negative,
  tune lower. 0 means no idea or feature not present in the current
  controller (i.e. V4L1). Strength and/or afc can be NULL pointers,
  that would mean ignore that parameter.
*/
static int
tveng25_get_signal_strength (int *strength, int * afc,
			   tveng_device_info * info)
{
  struct v4l2_tuner tuner;

  t_assert(info != NULL);

  if (!IS_TUNER_LINE (info->cur_video_input))
    return -1;

  tuner.index = VI(info->cur_video_input)->tuner;

  if (-1 == v4l25_ioctl (info, VIDIOC_G_TUNER, &tuner))
      return -1;

  if (strength)
    {
      /*
	Properly we should only return the signal field, but it doesn't
	always work :-/
	This has the advantage that it will find most stations (with a
	good reception) and the disadvantage that it will find too
	many stations... but better too many than too few :-)
      */
#if 0
      /* update: bttv2 does use the signal field, so lets use it
	 instead */
      if (tuner.signal)
	*strength = tuner.signal;
      else if (tuner.afc == 0)
	*strength = 65535;
      else
	*strength = 0;
#else
      /* This is the correct method, but it doesn't always work */
      *strength = tuner.signal;
#endif
    }

  if (afc)
    *afc = tuner.afc;

  return 0; /* Success */
}


/* Some private functions */
/* Queues an specific buffer. -1 on error */
static int p_tveng25_qbuf(int index, tveng_device_info * info)
{
  struct v4l2_buffer tmp_buffer;
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;

  t_assert(info != NULL);

  tmp_buffer.type = p_info -> buffers[0].vidbuf.type;
  tmp_buffer.index = index;

  if (-1 == v4l25_ioctl (info, VIDIOC_QBUF, &tmp_buffer))
      return -1;

  return 0;
}

/* dequeues next available buffer and returns it's id. -1 on error */
static int p_tveng25_dqbuf(tveng_device_info * info)
{
  struct v4l2_buffer tmp_buffer;
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;
  
  t_assert(info != NULL);

  tmp_buffer.type = p_info -> buffers[0].vidbuf.type;

  /* NB this blocks */
  if (-1 == v4l25_ioctl (info, VIDIOC_DQBUF, &tmp_buffer))
      return -1;

  p_info -> last_timestamp =
    tmp_buffer.timestamp.tv_sec
    + tmp_buffer.timestamp.tv_usec * (1 / 1e6);

  return (tmp_buffer.index);
}

/*
  Sets up the capture device so any read() call after this one
  succeeds. Returns -1 on error.
*/
static int
tveng25_start_capturing(tveng_device_info * info)
{
  struct v4l2_requestbuffers rb;
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;
  int i;

  t_assert(info != NULL);

  tveng_stop_everything(info);

  t_assert(info -> current_mode == TVENG_NO_CAPTURE);
  t_assert(p_info->num_buffers == 0);
  t_assert(p_info->buffers == NULL);

  p_info -> buffers = NULL;
  p_info -> num_buffers = 0;

  rb.count = 8; /* This is a good number(tm) */
  rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  rb.memory = V4L2_MEMORY_MMAP;

  if (-1 == v4l25_ioctl (info, VIDIOC_REQBUFS, &rb))
      return -1;

  if (rb.count <= 2)
    {
      info->tveng_errno = -1;
      t_error_msg("check()", "Not enough buffers", info);
      return -1;
    }

  p_info -> buffers = (struct tveng25_vbuf*)
    malloc(rb.count*sizeof(struct tveng25_vbuf));
  p_info -> num_buffers = rb.count;

  for (i = 0; i < rb.count; i++)
    {
      p_info -> buffers[i].vidbuf.index = i;
      p_info -> buffers[i].vidbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      if (-1 == v4l25_ioctl (info, VIDIOC_QUERYBUF,
			     &p_info->buffers[i].vidbuf))
	  return -1;

      /* bttv 0.8.x wants PROT_WRITE although AFAIK we don't. */
      p_info->buffers[i].vmem =
	mmap (0, p_info->buffers[i].vidbuf.length,
	      PROT_READ | PROT_WRITE,
	      MAP_SHARED, info->fd,
	      p_info->buffers[i].vidbuf.m.offset);

      if (p_info->buffers[i].vmem == (void *) -1)
	p_info->buffers[i].vmem =
	  mmap(0, p_info->buffers[i].vidbuf.length,
	       PROT_READ, MAP_SHARED, info->fd, 
	       p_info->buffers[i].vidbuf.m.offset);

      if (p_info->buffers[i].vmem == (void *) -1)
	{
	  info->tveng_errno = errno;
	  t_error("mmap()", info);
	  return -1;
	}

	/* Queue the buffer */
      if (p_tveng25_qbuf(i, info) == -1)
	return -1;
    }

  /* Turn on streaming */
  i = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == v4l25_ioctl (info, VIDIOC_STREAMON, &i))
      return -1;

  p_info -> last_timestamp = -1;

  info->current_mode = TVENG_CAPTURE_READ;

  return 0;
}

/* Tries to stop capturing. -1 on error. */
static int
tveng25_stop_capturing(tveng_device_info * info)
{
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;
  int i;

  if (info -> current_mode == TVENG_NO_CAPTURE)
    {
      fprintf(stderr,
	      "Warning: trying to stop capture with no capture active\n");
      return 0; /* Nothing to be done */
    }

  t_assert(info->current_mode == TVENG_CAPTURE_READ);

  /* Turn streaming off */
  i = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == v4l25_ioctl (info, VIDIOC_STREAMOFF, &i))
    {
      /* No critical error, go on munmapping */
    }

  for (i = 0; i < p_info -> num_buffers; i++)
    {
      if (munmap(p_info -> buffers[i].vmem,
		 p_info -> buffers[i].vidbuf.length))
	{
	  info->tveng_errno = errno;
	  t_error("munmap()", info);
	}
    }

  if (p_info -> buffers)
    {
      free(p_info -> buffers);
      p_info->buffers = NULL;
    }
  p_info->num_buffers = 0;

  p_info -> last_timestamp = -1;

  info->current_mode = TVENG_NO_CAPTURE;

  return 0;
}

/* 
   Reads a frame from the video device, storing the read data in
   the location pointed to by where.
   time: time to wait using select() in miliseconds
   info: pointer to the video device info structure
   Returns -1 on error, anything else on success
*/
static
int tveng25_read_frame(tveng_image_data *where, 
		      unsigned int time, tveng_device_info * info)
{
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;
  int n; /* The dequeued buffer */
  fd_set rdset;
  struct timeval timeout;

  if (info -> current_mode != TVENG_CAPTURE_READ)
    {
      info -> tveng_errno = -1;
      t_error_msg("check", "Current capture mode is not READ (%d)",
		  info, info->current_mode);
      return -1;
    }

  /* Fill in the rdset structure */
  FD_ZERO(&rdset);
  FD_SET(info->fd, &rdset);
  timeout.tv_sec = 0;
  timeout.tv_usec = time*1000;
  n = select(info->fd +1, &rdset, NULL, NULL, &timeout);
  if (n == -1)
    {
      info->tveng_errno = errno;
      t_error("select()", info);
      return -1;
    }
  else if (n == 0)
    return 0; /* This isn't properly an error, just a timeout */

  t_assert(FD_ISSET(info->fd, &rdset)); /* Some sanity check */

  n = p_tveng25_dqbuf(info);
  if (n == -1)
    return -1;

  /* Ignore frames we haven't been able to process */
  do{
    FD_ZERO(&rdset);
    FD_SET(info->fd, &rdset);
    timeout.tv_sec = timeout.tv_usec = 0;
    if (select(info->fd +1, &rdset, NULL, NULL, &timeout) < 1)
      break;
    p_tveng25_qbuf(n, info);
    n = p_tveng25_dqbuf(info);
  } while (1);

  /* Copy the data to the address given */
  if (where)
    tveng_copy_frame (p_info->buffers[n].vmem, where, info);

  /* Queue the buffer again for processing */
  if (p_tveng25_qbuf(n, info))
    return -1;

  /* Everything has been OK, return 0 (success) */
  return 0;
}

/*
  Gets the timestamp of the last read frame in seconds.
  Returns -1 on error, if the current mode isn't capture, or if we
  haven't captured any frame yet.
*/
static double tveng25_get_timestamp(tveng_device_info * info)
{
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info *) info;

  t_assert(info != NULL);

  if (info->current_mode != TVENG_CAPTURE_READ)
    return -1;

  return (p_info -> last_timestamp);
}

/* 
   Sets the capture buffer to an specific size. returns -1 on
   error. Remember to check the value of width and height since it can
   be different to the one requested. 
*/
static
int tveng25_set_capture_size(int width, int height, tveng_device_info * info)
{
  enum tveng_capture_mode current_mode;
  int retcode;

  t_assert(info != NULL);
  t_assert(width > 0);
  t_assert(height > 0);

  tveng25_update_capture_format(info);

  current_mode = tveng_stop_everything(info);

  if (width < info->caps.minwidth)
    width = info->caps.minwidth;
  else if (width > info->caps.maxwidth)
    width = info->caps.maxwidth;
  if (height < info->caps.minheight)
    height = info->caps.minheight;
  else if (height > info->caps.maxheight)
    height = info->caps.maxheight;
  
  info -> format.width = width;
  info -> format.height = height;
  retcode = set_capture_format(info);

  /* Restart capture again */
  if (tveng_restart_everything(current_mode, info) == -1)
    retcode = -1;

  return retcode;
}

/* 
   Gets the actual size of the capture buffer in width and height.
   -1 on error
*/
static
int tveng25_get_capture_size(int *width, int *height, tveng_device_info * info)
{
  t_assert(info != NULL);

  if (tveng25_update_capture_format(info))
    return -1;

  if (width)
    *width = info->format.width;
  if (height)
    *height = info->format.height;

  return 0; /* Success */
}

static struct tveng_module_info tveng25_module_info = {
  .attach_device =		tveng25_attach_device,
  .describe_controller =	tveng25_describe_controller,
  .close_device =		tveng25_close_device,
  .set_video_input		= set_video_input,
  .update_video_input		= update_video_input,
  .set_tuner_frequency		= set_tuner_frequency,
  .update_tuner_frequency	= update_tuner_frequency,
  .set_standard			= set_standard,
  .update_standard		= update_standard,
  .update_capture_format =	tveng25_update_capture_format,
  .set_capture_format =		tveng25_set_capture_format,
  .update_control		= update_control,
  .set_control			= set_control,
  .get_signal_strength =	tveng25_get_signal_strength,
  .start_capturing =		tveng25_start_capturing,
  .stop_capturing =		tveng25_stop_capturing,
  .read_frame =			tveng25_read_frame,
  .get_timestamp =		tveng25_get_timestamp,
  .set_capture_size =		tveng25_set_capture_size,
  .get_capture_size =		tveng25_get_capture_size,
  .get_overlay_buffer		= get_overlay_buffer,
  .set_preview_window =		tveng25_set_preview_window,
  .get_preview_window =		tveng25_get_preview_window,
  .set_overlay			= set_overlay,
  .get_chromakey =		tveng25_get_chromakey,
  .set_chromakey =		tveng25_set_chromakey,

  .private_size =		sizeof(struct private_tveng25_device_info)
};

/*
  Inits the V4L2 module, and fills in the given table.
*/
void tveng25_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

  memcpy(module_info, &tveng25_module_info,
	 sizeof(struct tveng_module_info)); 
}

#else /* !ENABLE_V4L */

#include "tveng25.h"

void tveng25_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

  CLEAR (module_info);
}

#endif /* ENABLE_V4L */
