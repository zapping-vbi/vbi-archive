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

#include "site_def.h"

/*
  This is the library in charge of simplifying Video Access API (I
  don't want to use thirteen lines of code with ioctl's every time I
  want to change tuning freq).
  the name is TV Engine, since it is intended mainly for TV viewing.
  This file is separated so zapping doesn't need to know about V4L[2]
*/


#ifdef HAVE_CONFIG_H
#  include "config.h"
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

/* Kernel interface */
#include <linux/types.h>	/* __u32 etc */
#include "common/videodev.h"
#include "common/_videodev.h"
#include "common/videodev25.h"
#include "common/_videodev25.h"

static void
fprint_ioctl_arg		(FILE *			fp,
				 unsigned int		cmd,
				 int			rw,
				 void *			arg)
{
	if (0 == ((cmd ^ VIDIOC_QUERYCAP) & (_IOC_TYPEMASK << _IOC_TYPESHIFT)))
		fprint_v4l25_ioctl_arg (fp, cmd, rw, arg);
	else
		fprint_v4l_ioctl_arg (fp, cmd, rw, arg);
}

/* This macro checks at compile time if the arg type is correct,
   device_ioctl() repeats the ioctl if interrupted (EINTR) and logs
   the args and result if log_fp is non-zero. When the ioctl failed
   ioctl_failure() stores the cmd, caller and errno in info. */
#define xioctl(info, cmd, arg)						\
	(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),				\
	 ((0 == device_ioctl ((info)->log_fp, fprint_ioctl_arg,		\
			      (info)->fd, cmd, (void *)(arg))) ?	\
	  0 : (ioctl_failure (info, __FILE__, __FUNCTION__,		\
			      __LINE__, # cmd), -1)))

#define xioctl_may_fail(info, cmd, arg)					\
	(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),				\
	 device_ioctl ((info)->log_fp, fprint_ioctl_arg,		\
		       (info)->fd, cmd, (void *)(arg)))

/** @internal */
struct xcontrol {
	tv_control		pub;

	/** V4L2_CID_. */
	unsigned int		id;
};

#define C(l) PARENT (l, struct xcontrol, pub)

struct video_input {
	tv_video_line		pub;

	int			index;		/* struct v4l2_input */
	unsigned int		tuner;		/* struct v4l2_tuner */

	unsigned int		step_shift;

	/* Only applicable if tuner. */
	tv_audio_capability	audio_capability;
};

#define VI(l) PARENT (l, struct video_input, pub)
#define CVI(l) CONST_PARENT (l, struct video_input, pub)

/** @internal */
struct xbuffer {
	tv_capture_buffer	cb;

	/** Data from VIDIOC_REQBUFS, updated after
	    VIDIOC_QBUF, VIDIOC_DQBUF. */
	struct v4l2_buffer	vb;

	/** Client got a cb pointer with dequeue_buffer(). */
	tv_bool			dequeued;

	/** Clear this buffer before next VIDIOC_QBUF. */
	tv_bool			clear;
};

struct private_tveng25_device_info
{
  tveng_device_info info; /* Info field, inherited */
  struct timeval last_timestamp; /* The timestamp of the last captured buffer */
  uint32_t chroma;
  int audio_mode; /* 0 mono */
	struct v4l2_capability	caps;

	struct xbuffer *	buffers;
	unsigned int		n_buffers;

	tv_control *		mute;

	tv_bool			bttv_driver;
	tv_bool			read_back_controls;
	tv_bool			use_v4l_audio;
	tv_bool			use_s_ctrl_old;

	tv_bool			streaming;
};

#define P_INFO(p) PARENT (p, struct private_tveng25_device_info, info)

/* FIXME must be >= capture.c/N_BUNDLES. */
#define N_BUFFERS 8

#ifndef TVENG25_BAYER_TEST
#define TVENG25_BAYER_TEST 0
#endif

static tv_bool
queue_xbuffers			(tveng_device_info *	info);

/*
	Audio matrix
 */

static unsigned int
tv_audio_mode_to_audmode	(tv_audio_mode		mode)
{
	switch (mode) {
	case TV_AUDIO_MODE_AUTO:
	case TV_AUDIO_MODE_LANG1_MONO:
		return V4L2_TUNER_MODE_MONO;

	case TV_AUDIO_MODE_LANG1_STEREO:
		return V4L2_TUNER_MODE_STEREO;

	case TV_AUDIO_MODE_LANG2_MONO:
		return V4L2_TUNER_MODE_LANG2;
	}

	assert (!"reached");

	return 0;
}

static unsigned int
tv_audio_mode_to_v4l_mode	(tv_audio_mode		mode)
{
	switch (mode) {
	case TV_AUDIO_MODE_AUTO:
		return 0;

	case TV_AUDIO_MODE_LANG1_MONO:
		return VIDEO_SOUND_MONO;

	case TV_AUDIO_MODE_LANG1_STEREO:
		return VIDEO_SOUND_STEREO;

	case TV_AUDIO_MODE_LANG2_MONO:
		return VIDEO_SOUND_LANG2;
	}

	assert (!"reached");

	return 0;
}

#if 0 /* Not used yet. */

static tv_audio_mode
tv_audio_mode_from_audmode	(unsigned int		mode)
{
	switch (mode) {
	case V4L2_TUNER_MODE_MONO:
	case V4L2_TUNER_MODE_LANG1:
	default:
		return TV_AUDIO_MODE_LANG1_MONO;

	case V4L2_TUNER_MODE_STEREO:
		return TV_AUDIO_MODE_LANG1_STEREO;

	case V4L2_TUNER_MODE_LANG2:
		return TV_AUDIO_MODE_LANG2_MONO;
	}
}

#endif

static void
store_audio_capability		(tveng_device_info *	info)
{
	static const tv_audio_capability lang2 =
		TV_AUDIO_CAPABILITY_SAP |
		TV_AUDIO_CAPABILITY_BILINGUAL;
	tv_audio_capability cap;

	cap = TV_AUDIO_CAPABILITY_EMPTY;

	if (info->panel.cur_video_input
	    && IS_TUNER_LINE (info->panel.cur_video_input)) {
		cap = VI(info->panel.cur_video_input)->audio_capability;

		if (cap & lang2) {
			const tv_video_standard *s;

			cap &= ~lang2;

			s = info->panel.cur_video_standard;

			if (s && (s->videostd_set
				  & TV_VIDEOSTD_SET (TV_VIDEOSTD_NTSC_M))) {
				cap |= TV_AUDIO_CAPABILITY_SAP;
			} else {
				cap |= TV_AUDIO_CAPABILITY_BILINGUAL;
			}
		}
	}

	if (info->panel.audio_capability != cap) {
		info->panel.audio_capability = cap;

		tv_callback_notify (info, info, info->panel.audio_callback);
	}
}

static void
store_audio_reception		(tveng_device_info *	info,
				 unsigned int		rxsubchans)
{
	unsigned int rec[2];

	rec[0] = 0;
	rec[1] = 0;

	if (rxsubchans & V4L2_TUNER_SUB_STEREO)
		rec[0] = 2;
	else if (rxsubchans & V4L2_TUNER_SUB_MONO)
		rec[0] = 1;

	if (rxsubchans & V4L2_TUNER_SUB_LANG2)
		rec[1] = 1;

	if (info->panel.audio_reception[0] != rec[0]
	    || info->panel.audio_reception[1] != rec[1]) {
		info->panel.audio_reception[0] = rec[0];
		info->panel.audio_reception[1] = rec[1];

		tv_callback_notify (info, info, info->panel.audio_callback);
	}
}

static tv_bool
set_audio_mode			(tveng_device_info *	info,
				 tv_audio_mode		mode)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);

	if (info->panel.cur_video_input
	    && IS_TUNER_LINE (info->panel.cur_video_input)) {
		if (p_info->use_v4l_audio) {
			struct video_audio audio;

			if (-1 == xioctl (info, VIDIOCGAUDIO, &audio)) {
				return FALSE;
			}

			if (p_info->mute && p_info->mute->value)
				audio.flags |= VIDEO_AUDIO_MUTE;
			else
				audio.flags &= ~VIDEO_AUDIO_MUTE;

			audio.mode = tv_audio_mode_to_v4l_mode (mode);

			if (-1 == xioctl (info, VIDIOCSAUDIO, &audio)) {
				return FALSE;
			}
		} else {
			struct v4l2_tuner tuner;
			unsigned int audmode;

			CLEAR (tuner);
			tuner.index = VI(info->panel.cur_video_input)->tuner;

			if (-1 == xioctl (info, VIDIOC_G_TUNER, &tuner))
				return FALSE;

			store_audio_reception (info, tuner.rxsubchans);

			audmode = tv_audio_mode_to_audmode (mode);

			if (tuner.audmode != audmode) {
				tuner.audmode = audmode;

				if (-1 == xioctl (info, VIDIOC_S_TUNER,
						  &tuner)) {
					return FALSE;
				}
			}
		}
	}

	if (info->panel.audio_mode != mode) {
		info->panel.audio_mode = mode;

		tv_callback_notify (info, info, info->panel.audio_callback);
	}

	return TRUE;
}

/*
	Controls
*/

static tv_bool
get_xcontrol			(tveng_device_info *	info,
				 struct xcontrol *	c)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	struct v4l2_control ctrl;

	if (c->pub.id == TV_CONTROL_ID_AUDIO_MODE)
		return TRUE; /* XXX */

	if (c->id == V4L2_CID_AUDIO_MUTE) {
		/* Doesn't seem to work with bttv.
		   FIXME we should check at runtime. */
		return TRUE;
	}

	CLEAR (ctrl);
	ctrl.id = c->id;

	if (-1 == xioctl (&p_info->info, VIDIOC_G_CTRL, &ctrl))
		return FALSE;

	if (c->pub.value != ctrl.value) {
		c->pub.value = ctrl.value;
		tv_callback_notify (&p_info->info, &c->pub, c->pub._callback);
	}

	return TRUE;
}

static tv_bool
get_control			(tveng_device_info *	info,
				 tv_control *		c)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);

	if (c)
		return get_xcontrol (info, C(c));

	for_all (c, p_info->info.panel.controls)
		if (c->_parent == info)
			if (!get_xcontrol (info, C(c)))
				return FALSE;

	return TRUE;
}

static tv_bool
set_control			(tveng_device_info *	info,
				 tv_control *		c,
				 int			value)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	struct v4l2_control ctrl;

	if (c->id == TV_CONTROL_ID_AUDIO_MODE)
		return set_audio_mode_control (info, c, value);

	CLEAR (ctrl);
	ctrl.id = C(c)->id;
	ctrl.value = value;

	if (p_info->use_s_ctrl_old) {
		/* Incorrectly defined as _IOW. */
		if (-1 == xioctl (info, VIDIOC_S_CTRL_OLD, &ctrl)) {
			return FALSE;
		}
	} else if (-1 == xioctl_may_fail (info, VIDIOC_S_CTRL, &ctrl)) {
		if (EINVAL != errno) {
			ioctl_failure (info,
				       __FILE__,
				       __PRETTY_FUNCTION__,
				       __LINE__,
				       "VIDIOC_S_CTRL");

			return FALSE;
		}

		CLEAR (ctrl);
		ctrl.id = C(c)->id;
		ctrl.value = value;

		if (-1 == xioctl (info, VIDIOC_S_CTRL_OLD, &ctrl)) {
			return FALSE;
		}

		/* We call this function quite often,
		   next time take a shortcut. */
		p_info->use_s_ctrl_old = TRUE;
	}

	if (p_info->read_back_controls) {
		/* Doesn't seem to work with bttv.
		   FIXME we should check at runtime. */
		if (ctrl.id == V4L2_CID_AUDIO_MUTE) {
			if (c->value != value) {
				c->value = value;
				tv_callback_notify (info, c, c->_callback);
			}
			return TRUE;
		}

		return get_xcontrol (info, C(c));
	} else {
		if (c->value != value) {
			c->value = value;
			tv_callback_notify (info, c, c->_callback);
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

static tv_bool
add_control			(tveng_device_info *	info,
				 unsigned int		id)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	struct v4l2_queryctrl qc;
	struct xcontrol *xc;
	tv_control *tc;
	unsigned int i;

	CLEAR (qc);
	qc.id = id;

	/* XXX */
	if (-1 == xioctl_may_fail (info, VIDIOC_QUERYCTRL, &qc)) {
		if (EINVAL != errno)
			ioctl_failure (info,
				       __FILE__,
				       __PRETTY_FUNCTION__,
				       __LINE__,
				       "VIDIOC_QUERYCTRL");

		return FALSE;
	}

	if (qc.flags & (V4L2_CTRL_FLAG_DISABLED | V4L2_CTRL_FLAG_GRABBED))
		return TRUE;

	if (!(xc = calloc (1, sizeof (*xc))))
		return TRUE;

	for (i = 0; i < N_ELEMENTS (controls); ++i)
		if (qc.id == controls[i].cid)
			break;

	if (i < N_ELEMENTS (controls)) {
		xc->pub.label = strdup (_(controls[i].label));
		xc->pub.id = controls[i].id;
	} else {
		xc->pub.label = strndup (qc.name, 32);
		xc->pub.id = TV_CONTROL_ID_UNKNOWN;
	}

	if (!xc->pub.label)
		goto failure;

	xc->pub.minimum	= qc.minimum;
	xc->pub.maximum	= qc.maximum;
	xc->pub.step	= qc.step;
	xc->pub.reset	= qc.default_value;
	xc->id		= qc.id;

	switch (qc.type) {
	case V4L2_CTRL_TYPE_INTEGER:
		xc->pub.type = TV_CONTROL_TYPE_INTEGER;
		break;

	case V4L2_CTRL_TYPE_BOOLEAN:
		xc->pub.type = TV_CONTROL_TYPE_BOOLEAN;

		/* Some drivers do not properly initialize these. */
		xc->pub.minimum	= 0;
		xc->pub.maximum	= 1;
		xc->pub.step	= 1;

		/* Just in case. */
		xc->pub.reset	= !!xc->pub.reset;

		break;

	case V4L2_CTRL_TYPE_BUTTON:
		xc->pub.type = TV_CONTROL_TYPE_ACTION;

		xc->pub.minimum	= INT_MIN;
		xc->pub.maximum	= INT_MAX;
		xc->pub.step	= 0;
		xc->pub.reset	= 0;

		break;

	case V4L2_CTRL_TYPE_MENU:
	{
		unsigned int n_items;
		unsigned int i;

		xc->pub.type = TV_CONTROL_TYPE_CHOICE;

		n_items = (unsigned int) qc.maximum;

		if (!(xc->pub.menu = calloc (n_items + 2, sizeof (char *))))
			goto failure;

		for (i = 0; i <= n_items; ++i) {
			struct v4l2_querymenu qm;

			CLEAR (qm);
			qm.id = qc.id;
			qm.index = i;

			if (0 == xioctl (info, VIDIOC_QUERYMENU, &qm)) {
				xc->pub.menu[i] = strndup (qm.name, 32);
				if (NULL == xc->pub.menu[i])
					goto failure;
			} else {
				goto failure;
			}
		}

		break;
	}

	default:
		fprintf (stderr, "V4L2: Unknown control type 0x%x (%s)\n",
			 qc.type, qc.name);
		goto failure;
	}

	if (!(tc = append_control (info, &xc->pub, /* allocate size */ 0)))
		goto failure;

	get_xcontrol (info, C(tc));

	if (qc.id == V4L2_CID_AUDIO_MUTE) {
		p_info->mute = tc;
	}

	return TRUE;

 failure:
	free_control (&xc->pub);

	return TRUE; /* no control, but not end of enum */
}

static tv_bool
get_control_list		(tveng_device_info *	info)
{
	unsigned int cid;

	free_controls (info);

	for (cid = V4L2_CID_BASE; cid < V4L2_CID_LASTP1; ++cid) {
		/* EINVAL ignored */
		add_control (info, cid);
	}

	/* Artificial control (preliminary) */
	/* XXX don't add if device has no audio. */
	append_audio_mode_control (info, (TV_AUDIO_CAPABILITY_AUTO |
					  TV_AUDIO_CAPABILITY_MONO |
					  TV_AUDIO_CAPABILITY_STEREO |
					  TV_AUDIO_CAPABILITY_SAP |
					  TV_AUDIO_CAPABILITY_BILINGUAL));

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
get_video_standard		(tveng_device_info *	info)
{
	v4l2_std_id std_id;
	tv_video_standard *s;

	if (!info->panel.video_standards)
		return TRUE;

	std_id = 0;

	if (-1 == xioctl (info, VIDIOC_G_STD, &std_id))
		return FALSE;

	for_all (s, info->panel.video_standards)
		if (s->videostd_set == std_id)
			break;

	/* s = NULL = unknown. */

	store_cur_video_standard (info, s);
	store_audio_capability (info);

	return TRUE;
}

static tv_bool
set_video_standard		(tveng_device_info *	info,
				 tv_video_standard *	s)
{
	v4l2_std_id std_id;
	tv_videostd_set videostd_set;
	int r;

	std_id = 0;

	if (!TVENG25_BAYER_TEST
	    && 0 == xioctl_may_fail (info, VIDIOC_G_STD, &std_id)) {
		const tv_video_standard *t;

		for_all (t, info->panel.video_standards)
			if (t->videostd_set == std_id)
				break;

		if (t == s)
			return TRUE;
	}

	if (P_INFO (info)->streaming)
		return FALSE;

	videostd_set = s->videostd_set;

	r = -1;
	if (!TVENG25_BAYER_TEST)
		r = xioctl (info, VIDIOC_S_STD, &videostd_set);

	if (0 == r) {
		store_cur_video_standard (info, s);

		store_audio_capability (info);
	}

	return (0 == r);
}

static tv_bool
get_video_standard_list		(tveng_device_info *	info)
{
	unsigned int i;

	free_video_standards (info);

	if (!info->panel.cur_video_input)
		return TRUE;

	if (TVENG25_BAYER_TEST)
		goto failure;

	for (i = 0;; ++i) {
		struct v4l2_standard standard;
		tv_video_standard *s;

		CLEAR (standard);
		standard.index = i;

		if (-1 == xioctl_may_fail (info, VIDIOC_ENUMSTD, &standard)) {
			if (errno == EINVAL)
				break; /* end of enumeration */

			ioctl_failure (info,
				       __FILE__,
				       __PRETTY_FUNCTION__,
				       __LINE__,
				       "VIDIOC_ENUMSTD");

			goto failure;
		}

		/* FIXME check if the standard is supported by the current input */

		if (!(s = append_video_standard (&info->panel.video_standards,
						 standard.id,
						 standard.name,
						 standard.name,
						 sizeof (*s))))
			goto failure;

		s->frame_rate =
			standard.frameperiod.denominator
			/ (double) standard.frameperiod.numerator;

		s->frame_ticks =
			90000 * standard.frameperiod.numerator
			/ standard.frameperiod.denominator;
	}

	if (get_video_standard (info))
		return TRUE;

 failure:
	free_video_standard_list (&info->panel.video_standards);
	return FALSE;
}

/*
 *  Video inputs
 */

#define SCALE_FREQUENCY(vi, freq)					\
	((((freq) & ~ (unsigned long) vi->step_shift)			\
	   * vi->pub.u.tuner.step) >> vi->step_shift)

static void
store_frequency			(tveng_device_info *	info,
				 struct video_input *	vi,
				 unsigned long		freq)
{
	unsigned int frequency = SCALE_FREQUENCY (vi, freq);

	if (vi->pub.u.tuner.frequency != frequency) {
		vi->pub.u.tuner.frequency = frequency;
		tv_callback_notify (info, &vi->pub, vi->pub._callback);
	}
}

static tv_bool
get_tuner_frequency		(tveng_device_info *	info,
				 tv_video_line *	l)
{
	struct v4l2_frequency vfreq;

	CLEAR (vfreq);
	vfreq.tuner = VI (l)->tuner;
	vfreq.type = V4L2_TUNER_ANALOG_TV;

	if (-1 == xioctl (info, VIDIOC_G_FREQUENCY, &vfreq))
		return FALSE;

	store_frequency (info, VI (l), vfreq.frequency);

	return TRUE;
}

static tv_bool
set_tuner_frequency		(tveng_device_info *	info,
				 tv_video_line *	l,
				 unsigned int		frequency)
{
	struct private_tveng25_device_info * p_info = P_INFO (info);
	struct video_input *vi = VI (l);
	struct v4l2_frequency vfreq;
	tv_bool restart;
	int buf_type;
	tv_bool r;

	CLEAR (vfreq);
	vfreq.tuner = vi->tuner;
	vfreq.type = V4L2_TUNER_ANALOG_TV;
	vfreq.frequency = (frequency << vi->step_shift) / vi->pub.u.tuner.step;

	buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	restart = (info->freq_change_restart
		   && p_info->streaming
		   && info->panel.cur_video_input == l);

	if (restart) {
		if (-1 == xioctl (info, VIDIOC_STREAMOFF, &buf_type))
			return FALSE;

		p_info->streaming = FALSE;
	}

	r = TRUE;

	if (0 == xioctl (info, VIDIOC_S_FREQUENCY, &vfreq))
		store_frequency (info, vi, vfreq.frequency);
	else
		r = FALSE;

	if (restart) {
		unsigned int i;

		/* Some drivers do not completely fill buffers when
		   tuning to an empty channel. */
		for (i = 0; i < p_info->n_buffers; ++i)
			p_info->buffers[i].clear = TRUE;

		if (!queue_xbuffers (info)) {
			/* XXX what now? */
			return FALSE;
		}

		if (0 == xioctl (info, VIDIOC_STREAMON, &buf_type)) {
			p_info->streaming = TRUE;
		} else {
			/* XXX what now? */
			r = FALSE;
		}
	}

	return r;
}

static tv_bool
get_signal_strength		(tveng_device_info *	info,
				 int *			strength,
				 int *			afc)
{
  struct v4l2_tuner tuner;

  CLEAR (tuner);
  tuner.index = VI(info->panel.cur_video_input)->tuner;

  /* XXX bttv 0.9.14 returns invalid .name? */
  if (-1 == xioctl (info, VIDIOC_G_TUNER, &tuner))
      return FALSE;

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

  return TRUE;
}

static tv_bool
get_video_input			(tveng_device_info *	info)
{
	tv_video_line *l;

	if (info->panel.video_inputs) {
		int index;

		/* sn9c102 bug. */
		index = 0;

		if (TVENG25_BAYER_TEST)
			return FALSE;

		if (-1 == xioctl (info, VIDIOC_G_INPUT, &index))
			return FALSE;

		if (info->panel.cur_video_input)
			if (VI (info->panel.cur_video_input)->index == index)
				return TRUE;

		for_all (l, info->panel.video_inputs)
			if (VI (l)->index == index)
				break;
	} else {
		l = NULL;

		if (info->panel.cur_video_input == NULL)
			return TRUE;
	}

	info->panel.cur_video_input = l;
	tv_callback_notify (info, info, info->panel.video_input_callback);

	if (l) {
		/* Implies get_video_standard() and store_audio_capability(). */
		get_video_standard_list (info);
	} else {
		free_video_standards (info);
	}

	return TRUE;
}

static tv_bool
set_video_input			(tveng_device_info *	info,
				 tv_video_line *	l)
{
	int index;

	index = 0;

	if (info->panel.cur_video_input) {
		if (0 == xioctl (info, VIDIOC_G_INPUT, &index))
			if (CVI (l)->index == index)
				return TRUE;
	}

	if (P_INFO (info)->streaming)
		return FALSE;

	index = CVI (l)->index;

	if (-1 == xioctl (info, VIDIOC_S_INPUT, &index))
		return FALSE;

	store_cur_video_input (info, l);

	/* Implies get_video_standard() and store_audio_capability(). */
	/* XXX error? */
	get_video_standard_list (info);

	if (IS_TUNER_LINE (l)) {
		/* Set tuner audmode. */
		set_audio_mode (info, info->panel.audio_mode);
	}

	return TRUE;
}

static tv_bool
tuner_bounds_audio_capability	(tveng_device_info *	info,
				 struct video_input *	vi)
{
	struct v4l2_tuner tuner;

	CLEAR (tuner);
	tuner.index = vi->tuner;

	/* XXX bttv 0.9.14 returns invalid .name? */
	if (-1 == xioctl (info, VIDIOC_G_TUNER, &tuner))
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

	vi->audio_capability = TV_AUDIO_CAPABILITY_MONO;

	if (tuner.capability & V4L2_TUNER_CAP_STEREO) {
		vi->audio_capability |= TV_AUDIO_CAPABILITY_STEREO;
	}

	if (tuner.capability & V4L2_TUNER_CAP_LANG2) {
		vi->audio_capability |=
			TV_AUDIO_CAPABILITY_SAP |
			TV_AUDIO_CAPABILITY_BILINGUAL;
	}

	return get_tuner_frequency (info, &vi->pub);
}

static tv_bool
get_video_input_list		(tveng_device_info *	info)
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

		if (-1 == xioctl_may_fail (info, VIDIOC_ENUMINPUT, &input)) {
			if (errno == EINVAL && i > 0)
				break; /* end of enumeration */

			ioctl_failure (info,
				       __FILE__,
				       __PRETTY_FUNCTION__,
				       __LINE__,
				       "VIDIOC_ENUMINPUT");

			free_video_line_list (&info->panel.video_inputs);

			return FALSE;
		}

		/* Sometimes NUL is missing. */
		z_strlcpy (buf, input.name, sizeof (buf));

		if (input.type & V4L2_INPUT_TYPE_TUNER)
			type = TV_VIDEO_LINE_TYPE_TUNER;
		else
			type = TV_VIDEO_LINE_TYPE_BASEBAND;

		if (!(vi = VI(append_video_line (&info->panel.video_inputs,
						 type, buf, buf,
						 sizeof (*vi)))))
			goto failure;

		vi->pub._parent = info;

		vi->index = input.index;
		vi->tuner = input.tuner;

		if (input.type & V4L2_INPUT_TYPE_TUNER)
			if (!tuner_bounds_audio_capability (info, vi))
				goto failure;
	}

	get_video_input (info);

	return TRUE;

 failure:
	free_video_line_list (&info->panel.video_inputs);
	return FALSE;
}

static tv_pixfmt
pixelformat_to_pixfmt		(unsigned int		pixelformat)
{
	switch (pixelformat) {
	  /* Note defines and Spec (0.4) are wrong: r <-> b */
	case V4L2_PIX_FMT_RGB332:	return TV_PIXFMT_BGR8;
	case V4L2_PIX_FMT_RGB555:	return TV_PIXFMT_BGRA16_LE;
	case V4L2_PIX_FMT_RGB565:	return TV_PIXFMT_BGR16_LE;
	case V4L2_PIX_FMT_RGB555X:	return TV_PIXFMT_BGRA16_BE;
	case V4L2_PIX_FMT_RGB565X:	return TV_PIXFMT_BGR16_BE;

	  /* Note Spec (0.4) is wrong: r <-> b, RGB32 wrong in bttv 0.9 */
	case V4L2_PIX_FMT_BGR24:	return TV_PIXFMT_BGR24_LE;
	case V4L2_PIX_FMT_RGB24:	return TV_PIXFMT_BGR24_BE;
	case V4L2_PIX_FMT_BGR32:	return TV_PIXFMT_BGRA32_LE;
	case V4L2_PIX_FMT_RGB32:	return TV_PIXFMT_BGRA32_BE;

	case V4L2_PIX_FMT_GREY:		return TV_PIXFMT_Y8;
	case V4L2_PIX_FMT_YUYV:		return TV_PIXFMT_YUYV;
	case V4L2_PIX_FMT_UYVY:		return TV_PIXFMT_UYVY;
	case V4L2_PIX_FMT_YVU420:	return TV_PIXFMT_YVU420;
	case V4L2_PIX_FMT_YUV420:	return TV_PIXFMT_YUV420;
	case V4L2_PIX_FMT_YVU410:	return TV_PIXFMT_YVU410;
	case V4L2_PIX_FMT_YUV410:	return TV_PIXFMT_YUV410;
	case V4L2_PIX_FMT_YUV422P:	return TV_PIXFMT_YUV422;
	case V4L2_PIX_FMT_YUV411P:	return TV_PIXFMT_YUV411;

	case V4L2_PIX_FMT_SBGGR8:	return TV_PIXFMT_SBGGR;

	default:			return TV_PIXFMT_UNKNOWN;
	}
}

static unsigned int
pixfmt_to_pixelformat		(tv_pixfmt		pixfmt)
{
	switch (pixfmt) {
	case TV_PIXFMT_BGR8:		return V4L2_PIX_FMT_RGB332;	
	case TV_PIXFMT_BGRA16_LE:	return V4L2_PIX_FMT_RGB555;	
	case TV_PIXFMT_BGR16_LE:	return V4L2_PIX_FMT_RGB565;	
	case TV_PIXFMT_BGRA16_BE:	return V4L2_PIX_FMT_RGB555X;	
	case TV_PIXFMT_BGR16_BE:	return V4L2_PIX_FMT_RGB565X;	

	case TV_PIXFMT_BGR24_LE:	return V4L2_PIX_FMT_BGR24;	
	case TV_PIXFMT_BGR24_BE:	return V4L2_PIX_FMT_RGB24;	
	case TV_PIXFMT_BGRA32_LE:	return V4L2_PIX_FMT_BGR32;	
	case TV_PIXFMT_BGRA32_BE:	return V4L2_PIX_FMT_RGB32;	

	case TV_PIXFMT_Y8:		return V4L2_PIX_FMT_GREY;
	case TV_PIXFMT_YUYV:		return V4L2_PIX_FMT_YUYV;
	case TV_PIXFMT_UYVY:		return V4L2_PIX_FMT_UYVY;
	case TV_PIXFMT_YVU420:		return V4L2_PIX_FMT_YVU420;	
	case TV_PIXFMT_YUV420:		return V4L2_PIX_FMT_YUV420;	
	case TV_PIXFMT_YVU410:		return V4L2_PIX_FMT_YVU410;	
	case TV_PIXFMT_YUV410:		return V4L2_PIX_FMT_YUV410;	
	case TV_PIXFMT_YUV422:		return V4L2_PIX_FMT_YUV422P;	
	case TV_PIXFMT_YUV411:		return V4L2_PIX_FMT_YUV411P;	

	case TV_PIXFMT_SBGGR:		return V4L2_PIX_FMT_SBGGR8;

	default:			return 0;
	}
}

/*
	Overlay
*/

static tv_bool
get_overlay_buffer		(tveng_device_info *	info)
{
	struct v4l2_framebuffer fb;
	tv_pixfmt pixfmt;

	CLEAR (fb);

	if (-1 == xioctl (info, VIDIOC_G_FBUF, &fb))
		return FALSE;

	/* XXX fb.capability, fb.flags ignored */

	CLEAR (info->overlay.buffer);

	pixfmt = pixelformat_to_pixfmt (fb.fmt.pixelformat);
	if (TV_PIXFMT_UNKNOWN == pixfmt)
		return TRUE;

	info->overlay.buffer.format.pixel_format =
		tv_pixel_format_from_pixfmt (pixfmt);

	info->overlay.buffer.base = (unsigned long) fb.base;

	info->overlay.buffer.format.width		= fb.fmt.width;
	info->overlay.buffer.format.height		= fb.fmt.height;

	info->overlay.buffer.format.bytes_per_line[0]	= fb.fmt.bytesperline;

	if (0 == fb.fmt.sizeimage) {
		info->overlay.buffer.format.size =
			fb.fmt.bytesperline * fb.fmt.height;
	} else {
		info->overlay.buffer.format.size = fb.fmt.sizeimage;
	}

	return TRUE;
}

/* XXX should have set_overlay_buffer with EPERM check in tveng.c */

static tv_bool
get_overlay_window		(tveng_device_info *	info)
{
	struct v4l2_format format;

	CLEAR (format);
	format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;

	if (-1 == xioctl (info, VIDIOC_G_FMT, &format))
		return FALSE;

	info->overlay.window.x		= format.fmt.win.w.left;
	info->overlay.window.y		= format.fmt.win.w.top;
	info->overlay.window.width	= format.fmt.win.w.width;
	info->overlay.window.height	= format.fmt.win.w.height;

	/* Clips cannot be read back, we assume no change. */

	return TRUE;
}

static tv_bool
set_overlay_window_clipvec	(tveng_device_info *	info,
				 const tv_window *	w,
				 const tv_clip_vector *	v)
{
	struct private_tveng25_device_info * p_info = P_INFO (info);
	struct v4l2_format format;
	struct v4l2_clip *clips;

	if (v->size > 0) {
		struct v4l2_clip *vc;
		const tv_clip *tc;
		unsigned int i;

		clips = malloc (v->size * sizeof (*clips));
		if (!clips) {
			info->tveng_errno = ENOMEM;
			t_error("malloc", info);
			return FALSE;
		}

		vc = clips;
		tc = v->vector;

		for (i = 0; i < v->size; ++i) {
			vc->next	= vc + 1;
			vc->c.left	= tc->x1;
			vc->c.top	= tc->y1;
			vc->c.width	= tc->x2 - tc->x1;
			vc->c.height	= tc->y2 - tc->y1;
			++vc;
			++tc;
		}

		vc[-1].next = NULL;
	} else {
		clips = NULL;
	}

	CLEAR (format);

	format.type			= V4L2_BUF_TYPE_VIDEO_OVERLAY;

	format.fmt.win.w.left		= w->x - info->overlay.buffer.x;
	format.fmt.win.w.top		= w->y - info->overlay.buffer.y;

	format.fmt.win.w.width		= w->width;
	format.fmt.win.w.height		= w->height;

	format.fmt.win.clips		= clips;
	format.fmt.win.clipcount	= v->size;

	format.fmt.win.chromakey	= p_info->chroma;

	if (-1 == xioctl (info, VIDIOC_S_FMT, &format)) {
		free (clips);
		return FALSE;
	}

	free (clips);

	/* Actual window size. */

	info->overlay.window.x		= format.fmt.win.w.left;
	info->overlay.window.y		= format.fmt.win.w.top;
	info->overlay.window.width	= format.fmt.win.w.width;
	info->overlay.window.height	= format.fmt.win.w.height;

	return TRUE;
}

static tv_bool
enable_overlay			(tveng_device_info *	info,
				 tv_bool		on)
{
	int value = on;

	if (0 == xioctl (info, VIDIOC_OVERLAY, &value)) {
		usleep (50000);
		return TRUE;
	} else {
		return FALSE;
	}
}

static tv_bool
set_overlay_window_chromakey	(tveng_device_info *	info,
				 const tv_window *	window,
				 unsigned int		chromakey)
{
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;
  tv_clip_vector vec;

  p_info->chroma = chromakey;

  CLEAR (vec);

  return set_overlay_window_clipvec (info, window, &vec);
}

static tv_bool
get_overlay_chromakey		(tveng_device_info *	info)
{
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;

  info->overlay.chromakey = p_info->chroma;

  return TRUE;
}

/*
	Capture
*/

static tv_bool
image_format_from_format	(tveng_device_info *	info _unused_,
				 tv_image_format *	f,
				 const struct v4l2_format *vfmt)
{
	tv_pixfmt pixfmt;
	unsigned int bytes_per_line;

	CLEAR (*f);

	if (TVENG25_BAYER_TEST) {
		pixfmt = TV_PIXFMT_SBGGR;
		bytes_per_line = vfmt->fmt.pix.width;
	} else {
		pixfmt = pixelformat_to_pixfmt (vfmt->fmt.pix.pixelformat);

		if (TV_PIXFMT_UNKNOWN == pixfmt)
			return FALSE;

		/* bttv 0.9.12 bug:
		   returns bpl = width * bpp, w/ bpp > 1 if planar YUV */
		bytes_per_line = vfmt->fmt.pix.width
			* tv_pixfmt_bytes_per_pixel (pixfmt);
	}

	tv_image_format_init (f,
			      vfmt->fmt.pix.width,
			      vfmt->fmt.pix.height,
			      bytes_per_line,
			      pixfmt,
			      TV_COLSPC_UNKNOWN);

	/* bttv 0.9.5 bug:
	   assert (f->fmt.pix.sizeimage >= info->capture.format.size); */

	if (vfmt->fmt.pix.sizeimage > f->size)
		f->size = vfmt->fmt.pix.sizeimage;

	return TRUE;
}

static tv_bool
get_capture_format		(tveng_device_info *	info)
{
	struct v4l2_format format;

	CLEAR (format);
	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl (info, VIDIOC_G_FMT, &format))
		return FALSE;

	/* Error ignored. */
	image_format_from_format (info, &info->capture.format, &format);

	return TRUE;
}

static tv_bool
set_capture_format		(tveng_device_info *	info,
				 const tv_image_format *fmt)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	struct v4l2_format format;
	unsigned int pixelformat;

	if (p_info->streaming)
		return FALSE;

	CLEAR (format);

	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	/* XXX bttv 0.9.14 bug: with YUV 4:2:0 activation of VBI
	   can cause a SCERR & driver reset, DQBUF -> EIO. */
	pixelformat = pixfmt_to_pixelformat (fmt->pixel_format->pixfmt);

	if (0 == pixelformat) {
		info->tveng_errno = -1; /* unknown */
		t_error_msg ("", "Bad pixfmt %u %s", info,
			     fmt->pixel_format->pixfmt,
			     fmt->pixel_format->name);
		return FALSE;
	}

	if (TVENG25_BAYER_TEST) {
		format.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
		pixelformat = V4L2_PIX_FMT_GREY;

		format.fmt.pix.width	= 352;
		format.fmt.pix.height	= 288;
	} else {
		format.fmt.pix.pixelformat = pixelformat;

		format.fmt.pix.width	= fmt->width;
		format.fmt.pix.height	= fmt->height;
	}

	format.fmt.pix.bytesperline	= 0; /* minimum please */
	format.fmt.pix.sizeimage	= 0; /* ditto */

	if (format.fmt.pix.height > 288)
		format.fmt.pix.field = V4L2_FIELD_INTERLACED;
	else
		format.fmt.pix.field = V4L2_FIELD_BOTTOM;

	if (-1 == xioctl (info, VIDIOC_S_FMT, &format))
		return FALSE;

	/* Actual image size (and pixfmt?). */

	/* Error ignored. */
	image_format_from_format (info, &info->capture.format, &format);

	if (format.fmt.pix.pixelformat != pixelformat)
		return FALSE;

	return TRUE;
}

static void
init_format_generic		(struct v4l2_format *	format,
				 unsigned int		pixelformat)
{
	CLEAR (*format);

	format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	format->fmt.pix.width		= 352;
	format->fmt.pix.height		= 288;
	format->fmt.pix.field		= V4L2_FIELD_BOTTOM;
	format->fmt.pix.bytesperline	= 0; /* minimum please */
	format->fmt.pix.sizeimage	= 0; /* ditto */
	format->fmt.pix.colorspace	= 0;
	format->fmt.pix.pixelformat	= pixelformat;
}

static tv_pixfmt_set
get_supported_pixfmt_set	(tveng_device_info *	info)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);

	tv_pixfmt_set pixfmt_set;
	tv_pixfmt pixfmt;
	unsigned int i;

	if (TVENG25_BAYER_TEST) {
		return TV_PIXFMT_SET (TV_PIXFMT_SBGGR);
	}

	pixfmt_set = TV_PIXFMT_SET_EMPTY;

	for (i = 0;; ++i) {
		struct v4l2_fmtdesc fmtdesc;

		CLEAR (fmtdesc);
		fmtdesc.index = i;

		if (-1 == xioctl_may_fail (info, VIDIOC_ENUM_FMT, &fmtdesc))
			break;

		pixfmt = pixelformat_to_pixfmt (fmtdesc.pixelformat);

		if (TV_PIXFMT_UNKNOWN != pixfmt)
			pixfmt_set |= TV_PIXFMT_SET (pixfmt);
	}

	if (0 != pixfmt_set)
		return pixfmt_set;

	/* Gross! Let's see if TRY_FMT gives more information. */

	/* sn9c102 1.0.8 bug: TRY_FMT doesn't copy format back to user. */
	if (0 == strcmp (p_info->caps.driver, "sn9c102")) {
		return TV_PIXFMT_SET (TV_PIXFMT_SBGGR); /* is an USB camera */
	}

	for (pixfmt = 0; pixfmt < TV_MAX_PIXFMTS; ++pixfmt) {
		struct v4l2_format format;
		unsigned int pixelformat;

 		/* XXX bttv 0.9.14 bug: with YUV 4:2:0 activation of VBI
		   can cause a SCERR & driver reset, DQBUF -> EIO. */
		pixelformat = pixfmt_to_pixelformat (pixfmt);
		if (0 == pixelformat)
			continue;

		init_format_generic (&format, pixelformat);

		if (-1 == xioctl_may_fail (info, VIDIOC_TRY_FMT, &format))
			continue;

		/* sn9c102 1.0.1 feature: changes pixelformat to SBGGR.
		   Though unexpected this is not prohibited by the spec. */
		if (format.fmt.pix.pixelformat == pixelformat)
			pixfmt_set |= TV_PIXFMT_SET (pixfmt);
	}

	if (0 != pixfmt_set)
		return pixfmt_set;

	/* TRY_FMT is optional, let's see if S_FMT works. */

	for (pixfmt = 0; pixfmt < TV_MAX_PIXFMTS; ++pixfmt) {
		struct v4l2_format format;
		unsigned int pixelformat;

 		/* XXX bttv 0.9.14 bug: with YUV 4:2:0 activation of VBI
		   can cause a SCERR & driver reset, DQBUF -> EIO. */
		pixelformat = pixfmt_to_pixelformat (pixfmt);
		if (0 == pixelformat)
			continue;

		init_format_generic (&format, pixelformat);

		if (-1 == xioctl_may_fail (info, VIDIOC_S_FMT, &format))
			continue;

		/* Error ignored. */
		image_format_from_format (info, &info->capture.format,
					  &format);

		if (format.fmt.pix.pixelformat == pixelformat)
			pixfmt_set |= TV_PIXFMT_SET (pixfmt);
	}

	return pixfmt_set;
}

static tv_bool
queue_xbuffer			(tveng_device_info *	info,
				 struct xbuffer *	b)
{
	struct v4l2_buffer buffer;

	if (b->clear) {
		if (!tv_clear_image (b->cb.data, &info->capture.format))
			return FALSE;

		b->clear = FALSE;
	}

	CLEAR (buffer);
	buffer.type = b->vb.type;
	buffer.memory = V4L2_MEMORY_MMAP;
	buffer.index = b->vb.index;

	if (-1 == xioctl (info, VIDIOC_QBUF, &buffer)) {
		return FALSE;
	}

	b->vb.flags = buffer.flags;

	return TRUE;
}

static tv_bool
queue_xbuffers			(tveng_device_info *	info)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	unsigned int i;

	for (i = 0; i < p_info->n_buffers; ++i) {
		if (p_info->buffers[i].dequeued)
			continue;

		if (!queue_xbuffer (info, &p_info->buffers[i]))
			return FALSE;
	}

	return TRUE;
}

static tv_bool
queue_buffer			(tveng_device_info *	info,
				 tv_capture_buffer *	buffer)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	struct xbuffer *b;
	int index;

	b = PARENT (buffer, struct xbuffer, cb);

	index = b - p_info->buffers;
	assert ((unsigned int) index < p_info->n_buffers);
	assert (b == &p_info->buffers[index]);

	assert (b->dequeued);

	if (!queue_xbuffer (info, b))
		return FALSE;

	b->dequeued = FALSE;

	return TRUE;
}

/* bttv returns EIO on internal timeout and dequeues
   the buffer it was about to write, does not
   dequeue on subsequent timeouts.  On bt8x8 SCERR it
   also resets.  To be safe we restart now, although
   really the caller should do that. */
static tv_bool
restart				(tveng_device_info *	info)
{
	struct private_tveng25_device_info * p_info = P_INFO (info);
	int saved_errno;
	int buf_type;

	saved_errno = errno;

	buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl (info, VIDIOC_STREAMOFF, &buf_type))
		return FALSE;

	p_info->streaming = FALSE;

	if (!queue_xbuffers (info))
		return FALSE;

	if (-1 == xioctl (info, VIDIOC_STREAMON, &buf_type))
		return FALSE;

	p_info->streaming = TRUE;

	errno = saved_errno;

	return TRUE;
}

static int
dequeue_buffer			(tveng_device_info *	info,
				 tv_capture_buffer **	buffer,
				 const struct timeval *	timeout)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	struct v4l2_buffer vbuf;
	struct xbuffer *b;
	struct timeval start;
	struct timeval now;
	struct timeval tv;

	*buffer = NULL;

	if (CAPTURE_MODE_READ != info->capture_mode) {
		info->tveng_errno = -1;
		t_error_msg("check", "Current capture mode is not READ (%d)",
			    info, info->capture_mode);
		return -1;
	}

	start.tv_sec = 0;
	start.tv_usec = 0;

	/* When timeout is zero elapsed time doesn't matter. */
	if (timeout && (timeout->tv_sec | timeout->tv_usec) > 0)
		gettimeofday (&start, /* timezone */ NULL);

	now = start;

 select_again:
	timeout_subtract_elapsed (&tv, timeout, &now, &start);

	if ((tv.tv_sec | tv.tv_usec) > 0) {
		fd_set set;
		int r;

 select_again_with_timeout:
		FD_ZERO (&set);
		FD_SET (info->fd, &set);

		r = select (info->fd + 1,
			    /* readable */ &set,
			    /* writeable */ NULL,
			    /* in exception */ NULL,
			    timeout ? &tv : NULL);

		switch (r) {
		case -1: /* error */
			switch (errno) {
			case EINTR:
				if (!timeout) /* infinite */
					goto select_again_with_timeout;

				gettimeofday (&now, /* timezone */ NULL);

				goto select_again;

			default:
				restart (info);

				info->tveng_errno = errno;
				t_error("select()", info);

				return -1;
			}

		case 0: /* timeout */
			return 0;

		default:
			break;
		}
	}

/* read_again: */
	CLEAR (vbuf);
	vbuf.type = p_info->buffers[0].vb.type;
	vbuf.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl (info, VIDIOC_DQBUF, &vbuf)) {
		switch (errno) {
		case EAGAIN:
		case EBUSY:
			if (!timeout) /* infinite */
				goto select_again_with_timeout;

			if ((timeout->tv_sec | timeout->tv_usec) <= 0)
				return 0; /* timeout */

			gettimeofday (&now, /* timezone */ NULL);
			timeout_subtract_elapsed (&tv, timeout, &now, &start);

			if ((tv.tv_sec | tv.tv_usec) <= 0)
				return 0; /* timeout */

			goto select_again_with_timeout;

		/* xioctl/device_ioctl() handles EINTR. */
		/* case EINTR:
			goto read_again; */

		default:
			restart (info);

			info->tveng_errno = errno;
			t_error("VIDIOC_DQBUF", info);

			return -1;
		}
	}

	assert (vbuf.index < p_info->n_buffers);

	b = p_info->buffers + vbuf.index; 

	b->cb.sample_time = vbuf.timestamp;

	if (info->panel.cur_video_standard) {
		b->cb.stream_time = vbuf.sequence
			* info->panel.cur_video_standard->frame_ticks;
	} else {
		b->cb.stream_time = 0;
	}

	p_info->last_timestamp = b->cb.sample_time;

	b->vb.flags	= vbuf.flags;
	b->vb.field	= vbuf.field;
	b->vb.timestamp	= vbuf.timestamp;
	b->vb.timecode	= vbuf.timecode;
	b->vb.sequence	= vbuf.sequence;
 
	b->dequeued = TRUE;

	*buffer = &b->cb;

	return 1;
}

static tv_bool
flush_buffers			(tveng_device_info *	info)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	unsigned int count;

	if (!p_info->streaming) {
		return FALSE;
	}

	for (count = p_info->n_buffers; count-- > 0;) {
		struct v4l2_buffer buffer;

		CLEAR (buffer);
		buffer.type = p_info->buffers[0].vb.type;
		buffer.memory = V4L2_MEMORY_MMAP;

		if (-1 == xioctl (info, VIDIOC_DQBUF, &buffer)) {
			switch (errno) {
			case EAGAIN:
			case EBUSY:
				return TRUE;

			default:
				restart (info);

				info->tveng_errno = errno;
				t_error ("VIDIOC_DQBUF", info);

				return FALSE;
			}
		}

		if (-1 == xioctl (&p_info->info, VIDIOC_QBUF, &buffer)) {
			/* XXX what now? */
			return FALSE;
		}
	}

	return TRUE;
}

static int
read_frame			(tveng_device_info *	info,
				 tv_capture_buffer *	buffer,
				 const struct timeval *	timeout)
{
	tv_capture_buffer *qbuffer;
	int r;

	r = dequeue_buffer (info, &qbuffer, timeout);
	if (r <= 0) /* error or timeout */
		return r;

	assert (NULL != qbuffer);

	if (buffer) {
		const tv_image_format *dst_format;

		dst_format = buffer->format;
		if (!dst_format)
			dst_format = &info->capture.format;

		tv_copy_image (buffer->data, dst_format,
			       qbuffer->data, &info->capture.format);

		buffer->sample_time = qbuffer->sample_time;
		buffer->stream_time = qbuffer->stream_time;
	}

	/* Queue the buffer again for processing */
	if (!queue_buffer (info, qbuffer))
		return -1; /* error */

	return 1; /* success */
}

static tv_bool
unmap_xbuffers			(tveng_device_info *	info,
				 tv_bool		ignore_errors)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	unsigned int i;
	tv_bool success;

	success = TRUE;

	for (i = p_info->n_buffers; i-- > 0;) {
		if (-1 == device_munmap (info->log_fp,
					 p_info->buffers[i].cb.data,
					 p_info->buffers[i].vb.length)) {
			if (!ignore_errors) {
				info->tveng_errno = errno;
				t_error("munmap()", info);

				ignore_errors = TRUE;
			}

			success = FALSE;
		}
	}

	memset (p_info->buffers, 0,
		p_info->n_buffers * sizeof (* p_info->buffers));

	free (p_info->buffers);

	p_info->buffers = NULL;
	p_info->n_buffers = 0;

	return success;
}

static tv_bool
map_xbuffers			(tveng_device_info *	info,
				 enum v4l2_buf_type	buf_type,
				 unsigned int		n_buffers)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);
	struct v4l2_requestbuffers rb;
	unsigned int i;

	p_info->buffers = NULL;
	p_info->n_buffers = 0;

	CLEAR (rb);

	rb.type = buf_type;
	rb.memory = V4L2_MEMORY_MMAP;
	rb.count = n_buffers;

	if (-1 == xioctl (info, VIDIOC_REQBUFS, &rb))
		return FALSE;

	if (rb.count <= 0)
		return FALSE;

	p_info->buffers = calloc (rb.count, sizeof (* p_info->buffers));
	p_info->n_buffers = 0;

	for (i = 0; i < rb.count; ++i) {
		struct xbuffer *b;
		void *data;

		b = p_info->buffers + i;

		b->vb.index = i;
		b->vb.type = buf_type;
		b->vb.memory = V4L2_MEMORY_MMAP;

		if (-1 == xioctl (info, VIDIOC_QUERYBUF, &b->vb))
			goto failure;

		data = device_mmap (info->log_fp,
				    /* start: any */ NULL,
				    b->vb.length,
				    PROT_READ | PROT_WRITE,
				    MAP_SHARED,
				    info->fd,
				    (off_t) b->vb.m.offset);

		if ((void *) -1 == data)
			goto failure;

		b->cb.data = data;
		b->cb.size = b->vb.length;

		b->cb.sample_time.tv_sec = 0;
		b->cb.sample_time.tv_usec = 0;
		b->cb.stream_time = 0;

		b->cb.format = &info->capture.format;

		b->dequeued = FALSE;
		b->clear = TRUE;

		++p_info->n_buffers;
	}

	if (queue_xbuffers (info))
		return TRUE;

 failure:
	info->tveng_errno = errno;
	t_error("mmap()", info);

	if (p_info->n_buffers > 0) {
		/* Driver bug, out of memory? May still work. */

		if (queue_xbuffers (info))
			return TRUE;
	}

	unmap_xbuffers (info, /* ignore_errors */ TRUE);

	return FALSE;
}

static tv_bool
enable_capture			(tveng_device_info *	info,
				 tv_bool		enable)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);

	if (enable) {
		gboolean dummy;
		unsigned int n_buffers;
		int buf_type;

		p_tveng_stop_everything(info,&dummy);

		t_assert(info -> capture_mode == CAPTURE_MODE_NONE);
		t_assert(p_info->n_buffers == 0);
		t_assert(p_info->buffers == NULL);

		/* sn9c102 1.0.8 bug: mmap fails for > 1 buffers, apparently
		   because m.offset isn't page aligned. */
		if (0 == strcmp (p_info->caps.driver, "sn9c102"))
			n_buffers = 1;
		else
			n_buffers = N_BUFFERS;

		if (!map_xbuffers (info,
				   V4L2_BUF_TYPE_VIDEO_CAPTURE,
				   n_buffers))
			return FALSE;

		if (p_info->n_buffers < 1
		    /* FIXME must be >= capture.c/N_BUNDLES. */
		    || ((info->caps.flags & TVENG_CAPS_QUEUE)
			&& p_info->n_buffers < n_buffers)) {
			info->tveng_errno = -1;
			t_error_msg("check()", "Not enough buffers", info);
			return FALSE;
		}

		buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (-1 == xioctl (info, VIDIOC_STREAMON, &buf_type)) {
			unmap_xbuffers (info, /* ignore_errors */ TRUE);
			return FALSE;
		}

		p_info->streaming = TRUE;
		info->capture_mode = CAPTURE_MODE_READ;

		CLEAR (p_info->last_timestamp);

		return TRUE;
	} else {
		int buf_type;

		t_assert(info->capture_mode == CAPTURE_MODE_READ);

		buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (-1 == xioctl (info, VIDIOC_STREAMOFF, &buf_type)) {
			return FALSE;
		}

		p_info->streaming = FALSE;
		info->capture_mode = CAPTURE_MODE_NONE;

		if (!unmap_xbuffers (info, /* ignore_errors */ FALSE)) {
			return FALSE;
		}

		return TRUE;
	}
}

/* Closes a device opened with tveng_init_device */
static void tveng25_close_device(tveng_device_info * info)
{
  gboolean dummy;
 
  p_tveng_stop_everything(info,&dummy);

  device_close(info->log_fp, info->fd);
  info -> fd = -1;
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

  free (info->node.label);
  free (info->node.bus);
  free (info->node.driver);
  free (info->node.version);
  free (info->node.device);

  CLEAR (info->node);
}

static tv_bool
get_capabilities		(tveng_device_info *	info)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);

	CLEAR (p_info->caps);

	if (-1 == xioctl_may_fail (info, VIDIOC_QUERYCAP, &p_info->caps))
		goto failure;

	p_info->bttv_driver = (0 == strcmp (p_info->caps.driver, "bttv"));

	/* bttv 0.9.14 bug: S_TUNER ignores v4l2_tuner.audmode. */
	p_info->use_v4l_audio = p_info->bttv_driver;

	if (!(p_info->caps.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		info->tveng_errno = -1;
		snprintf(info->error, 256, 
			 _("%s doesn't look like a valid capture device"), info
			 -> file_name);
		goto failure;
	}

	/* Check if we can mmap() this device */
	/* XXX REQBUFS must be checked too */
	if (!(p_info->caps.capabilities & V4L2_CAP_STREAMING)) {
		info -> tveng_errno = -1;
		snprintf(info->error, 256,
			 _("Sorry, but \"%s\" cannot do streaming"),
			 info -> file_name);
		goto failure;
	}

#undef STRCOPY
#define STRCOPY(s) _tv_strndup ((s), N_ELEMENTS (s))

	/* We copy in case the string array lacks a NUL. */
	/* XXX localize (encoding). */
	if (!(info->node.label = STRCOPY (p_info->caps.card)))
		goto failure;

	if (!(info->node.bus = STRCOPY (p_info->caps.bus_info)))
		goto failure;

	if (!(info->node.driver = STRCOPY (p_info->caps.driver)))
		goto failure;

	if (_tv_asprintf (&info->node.version,
			  "%u.%u.%u",
			  (p_info->caps.version >> 16) & 0xFF,
			  (p_info->caps.version >> 8) & 0xFF,
			  (p_info->caps.version >> 0) & 0xFF) < 0)
		goto failure;

	snprintf(info->caps.name, 32, p_info->caps.card);
	/* XXX get these elsewhere */
	info->caps.channels = 0; /* FIXME video inputs */
	info->caps.audios = 0;
	info->caps.maxwidth = 768;
	info->caps.minwidth = 16;
	info->caps.maxheight = 576;
	info->caps.minheight = 16;
	info->caps.flags = 0;
	
	/* This has been tested before */
	info->caps.flags |= TVENG_CAPS_CAPTURE;
	info->caps.flags |= TVENG_CAPS_QUEUE;

	if (p_info->caps.capabilities & V4L2_CAP_TUNER)
		info->caps.flags |= TVENG_CAPS_TUNER;
	if (p_info->caps.capabilities & V4L2_CAP_VBI_CAPTURE)
		info->caps.flags |= TVENG_CAPS_TELETEXT;

	/* XXX get elsewhere */
	if (p_info->caps.capabilities & 0)
		info->caps.flags |= TVENG_CAPS_MONOCHROME;

	return TRUE;

 failure:
	free (info->node.label);
	info->node.label = NULL;

	free (info->node.bus);
	info->node.bus = NULL;

	free (info->node.driver);
	info->node.driver = NULL;

	free (info->node.version);
	info->node.version = NULL;

	return FALSE;
}

/*
  Return fd for the device file opened. Checks if the device is a
  valid video device. -1 on error.
  Flags will be used for open()'ing the file 
*/
static int p_tveng25_open_device_file(int flags, tveng_device_info * info)
{
  struct private_tveng25_device_info *p_info = P_INFO (info);
  struct v4l2_framebuffer fb;
  extern int disable_overlay;

  t_assert(info != NULL);
  t_assert(info->file_name != NULL);

  if (!(info->node.device = strdup (info->file_name)))
    return -1;

  /* sn9c102 1.0.8 bug: NONBLOCK open fails with EAGAIN if the device has
     users, and it seems the counter is never decremented when the USB device
     is disconnected at close time. */
  if (0 != strcmp (p_info->caps.driver, "sn9c102"))
    flags |= O_NONBLOCK;

  info -> fd = device_open(info->log_fp, info -> file_name, flags, 0);
  if (-1 == info -> fd)
    {
      info->tveng_errno = errno;
      t_error("open()", info);
      free (info->node.device);
      info->node.device = NULL;
      return -1;
    }

  if (!get_capabilities (info))
    {
      device_close(info->log_fp, info->fd);
      free (info->node.device);
      info->node.device = NULL;
      return -1;
    }

  if (TVENG25_BAYER_TEST)
	  p_info->caps.capabilities &= ~V4L2_CAP_VIDEO_OVERLAY;

  CLEAR (fb);

  if (!disable_overlay && (p_info->caps.capabilities & V4L2_CAP_VIDEO_OVERLAY))
    {
      info->caps.flags |= TVENG_CAPS_OVERLAY;

      /* Collect more info about the overlay mode */
      if (xioctl (info, VIDIOC_G_FBUF, &fb) != 0)
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

static void
reset_crop_rect			(tveng_device_info *	info)
{
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;

	CLEAR (cropcap);
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == xioctl_may_fail (info, VIDIOC_CROPCAP, &cropcap)) {
		if (EINVAL != errno) {
			/* Error ignored. */
			return;
		}

		/* Incorrectly defined as _IOR. */
		if (-1 == xioctl_may_fail (info, VIDIOC_CROPCAP_OLD,
					   &cropcap)) {
			/* Error ignored. */
			return;
		}
	}

	CLEAR (crop);
	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	crop.c = cropcap.defrect;

	if (-1 == xioctl_may_fail (info, VIDIOC_S_CROP, &crop)) {
		switch (errno) {
		case EINVAL:
			/* Cropping not supported. */
			return;
		default:
			/* Errors ignored. */
			return;
		}
	}
}

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
			  Window window _unused_,
			  enum tveng_attach_mode attach_mode,
			 tveng_device_info * info)
{
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;

  t_assert(device_file != NULL);
  t_assert(info != NULL);

  if (-1 != info -> fd) /* If the device is already attached, detach it */
    tveng_close_device(info);

  info->audio_mutable = 0;

  info -> file_name = strdup(device_file);
  if (!(info -> file_name))
    {
      info -> tveng_errno = errno;
      t_error("strdup()", info);
      return -1;
    }

  switch (attach_mode)
    {
    case TVENG_ATTACH_CONTROL:
    case TVENG_ATTACH_VBI:
      attach_mode = TVENG_ATTACH_CONTROL;
      info -> fd = p_tveng25_open_device_file(0, info);
      break;

    case TVENG_ATTACH_READ:
    case TVENG_ATTACH_XV:
      attach_mode = TVENG_ATTACH_READ;
      /* NB must be RDWR since client may write mmapped buffers. */
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
  if (-1 == info -> fd)
    {
      free(info->file_name);
      info->file_name = NULL;
      return -1;
    }
  
  info -> attach_mode = attach_mode;
  /* Current capture mode is no capture at all */
  info -> capture_mode = CAPTURE_MODE_NONE;

	info->panel.set_video_input = set_video_input;
	info->panel.get_video_input = get_video_input;
	info->panel.set_tuner_frequency = set_tuner_frequency;
	info->panel.get_tuner_frequency = get_tuner_frequency;
	info->panel.get_signal_strength = get_signal_strength;
	info->panel.set_video_standard = set_video_standard;
	info->panel.get_video_standard = get_video_standard;
	info->panel.set_control = set_control;
	info->panel.get_control = get_control;
	info->panel.set_audio_mode = set_audio_mode;

	/* Video inputs & standards */

	info->panel.video_inputs = NULL;
	info->panel.cur_video_input = NULL;

	info->panel.video_standards = NULL;
	info->panel.cur_video_standard = NULL;

	if (!get_video_input_list (info))
		goto failure;

	/* Controls */

	info->panel.controls = NULL;

	if (!get_control_list (info))
		goto failure;


	if (info->caps.flags & (TVENG_CAPS_OVERLAY |
				TVENG_CAPS_CAPTURE)) {
		if (TVENG_ATTACH_READ == attach_mode)
			reset_crop_rect	(info);
	}

	/* Overlay */

	CLEAR (info->overlay);

	if (info->caps.flags & TVENG_CAPS_OVERLAY) {
		info->overlay.get_buffer = get_overlay_buffer;
		info->overlay.set_window_clipvec =
			set_overlay_window_clipvec;
		info->overlay.get_window = get_overlay_window;
		info->overlay.set_window_chromakey =
			set_overlay_window_chromakey;
		info->overlay.get_chromakey = get_overlay_chromakey;
		info->overlay.enable = enable_overlay;

		if (!get_overlay_buffer (info))
			goto failure;

		if (!get_overlay_window (info))
			goto failure;
	}

	/* Capture */

	CLEAR (info->capture);

	if (info->caps.flags & TVENG_CAPS_CAPTURE) {
		info->capture.get_format = get_capture_format;
		info->capture.set_format = set_capture_format;
		info->capture.read_frame = read_frame;
		info->capture.queue_buffer = queue_buffer;
	      	info->capture.dequeue_buffer = dequeue_buffer;
		info->capture.flush_buffers = flush_buffers;
		info->capture.enable = enable_capture;

		if (!get_capture_format (info))
			goto failure;

		info->capture.supported_pixfmt_set =
			get_supported_pixfmt_set (info);
	}

  /* Init the private info struct */
  p_info->n_buffers = 0;
  p_info->buffers = NULL;

  return info -> fd;

 failure:
  tveng25_close_device (info);
  return -1;
}

static struct tveng_module_info tveng25_module_info = {
  .attach_device =		tveng25_attach_device,
  .close_device =		tveng25_close_device,

  .interface_label		= "Video4Linux 2",

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
