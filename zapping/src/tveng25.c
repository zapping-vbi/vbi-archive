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
#include "common/videodev.h"
#include "common/videodev25.h"
#include "common/_videodev25.h"

#define v4l25_ioctl(info, cmd, arg)					\
(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
 ((0 == device_ioctl ((info)->log_fp, fprint_ioctl_arg,			\
		      (info)->fd, cmd, (void *)(arg))) ?		\
  0 : (ioctl_failure (info, __FILE__, __PRETTY_FUNCTION__,		\
		      __LINE__, # cmd), -1)))

#define v4l25_ioctl_nf(info, cmd, arg)					\
(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
 device_ioctl ((info)->log_fp, fprint_ioctl_arg,			\
		      (info)->fd, cmd, (void *)(arg)))

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
  unsigned int num_buffers; /* Number of mmaped buffers */
  struct tveng25_vbuf * buffers; /* Array of buffers */
  double last_timestamp; /* The timestamp of the last captured buffer */
  uint32_t chroma;
  int audio_mode; /* 0 mono */
	tv_control *		mute;

	tv_bool			bttv_driver;
	tv_bool			read_back_controls;
};

#define P_INFO(p) PARENT (p, struct private_tveng25_device_info, info)

#define N_BUFFERS 8

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
	default:			return 0;
	}
}

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
}

#if 0
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
set_audio_capability		(struct private_tveng25_device_info *p_info)
{
	static const tv_audio_capability lang2 =
		TV_AUDIO_CAPABILITY_SAP |
		TV_AUDIO_CAPABILITY_BILINGUAL;
	tv_audio_capability cap;

	cap = TV_AUDIO_CAPABILITY_EMPTY;

	if (p_info->info.cur_video_input
	    && IS_TUNER_LINE (p_info->info.cur_video_input)) {
		cap = VI(p_info->info.cur_video_input)->audio_capability;

		if (cap & lang2) {
			const tv_video_standard *std;

			std = p_info->info.cur_video_standard;

			if (std && (std->videostd_set
				    & TV_VIDEOSTD_SET (TV_VIDEOSTD_NTSC_M))) {
				cap = (cap & ~lang2) | TV_AUDIO_CAPABILITY_SAP;
			} else {
				cap = (cap & ~lang2)
					| TV_AUDIO_CAPABILITY_BILINGUAL;
			}
		}
	}

	if (p_info->info.audio_capability != cap) {
		p_info->info.audio_capability = cap;
		tv_callback_notify (&p_info->info, &p_info->info,
				    p_info->info.priv->audio_callback);
	}
}

static void
set_audio_reception		(struct private_tveng25_device_info *p_info,
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

	if (p_info->info.audio_reception[0] != rec[0]
	    || p_info->info.audio_reception[1] != rec[1]) {
		p_info->info.audio_reception[0] = rec[0];
		p_info->info.audio_reception[1] = rec[1];

		tv_callback_notify (&p_info->info, &p_info->info,
				    p_info->info.priv->audio_callback);
	}
}

#define SINGLE_BIT(n) ((n) > 0 && ((n) & ((n) - 1)) == 0)

static tv_bool
set_audio_mode			(tveng_device_info *	info,
				 tv_audio_mode		mode)
{
	struct private_tveng25_device_info *p_info = P_INFO (info);

	if (info->cur_video_input
	    && IS_TUNER_LINE (info->cur_video_input)) {
		/* bttv 0.9.14 bug: S_TUNER ignores v4l2_tuner.audmode. */
		if (p_info->bttv_driver) {
			struct video_audio audio;

			if (-1 == ioctl (p_info->info.fd,
					 VIDIOCGAUDIO, &audio)) {
				ioctl_failure (&p_info->info, __FILE__,
					       __PRETTY_FUNCTION__,
					       __LINE__, "VIDIOCGAUDIO");
				return FALSE;
			}

			if (p_info->mute && p_info->mute->value)
				audio.flags |= VIDEO_AUDIO_MUTE;
			else
				audio.flags &= ~VIDEO_AUDIO_MUTE;

			audio.mode = tv_audio_mode_to_v4l_mode (mode);

			if (-1 == ioctl (p_info->info.fd,
					 VIDIOCSAUDIO, &audio)) {
				ioctl_failure (&p_info->info, __FILE__,
					       __PRETTY_FUNCTION__,
					       __LINE__, "VIDIOCSAUDIO");
				return FALSE;
			}
		} else {
			struct v4l2_tuner tuner;
			unsigned int audmode;

			CLEAR (tuner);
			tuner.index = VI(info->cur_video_input)->tuner;

			if (-1 == v4l25_ioctl (info, VIDIOC_G_TUNER, &tuner))
				return FALSE;

			set_audio_reception (P_INFO (info), tuner.rxsubchans);

			audmode = tv_audio_mode_to_audmode (mode);

			if (tuner.audmode != audmode) {
				tuner.audmode = audmode;

				if (-1 == v4l25_ioctl (info, VIDIOC_S_TUNER,
						       &tuner))
					return FALSE;
			}
		}
	}

	if (info->audio_mode != mode) {
		info->audio_mode = mode;
		tv_callback_notify (&p_info->info, &p_info->info,
				    p_info->info.priv->audio_callback);
	}

	return TRUE;
}

/*
	Controls
*/

static tv_bool
do_get_control			(struct private_tveng25_device_info *p_info,
				 struct control *	c)
{
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

	if (-1 == v4l25_ioctl (&p_info->info, VIDIOC_G_CTRL, &ctrl))
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
		return do_get_control (p_info, C(c));

	for_all (c, p_info->info.controls)
		if (c->_parent == info)
			if (!do_get_control (p_info, C(c)))
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

	if (c->id == TV_CONTROL_ID_AUDIO_MODE)
		return set_audio_mode_control (info, c, value);

	CLEAR (ctrl);
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
				tv_callback_notify (info, c, c->_callback);
			}
			return TRUE;
		}

		return do_get_control (p_info, C(c));
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
	struct private_tveng25_device_info * p_info = P_INFO (info);
	struct v4l2_queryctrl qc;
	struct v4l2_querymenu qm;
	struct control *c;
	tv_control *tc;
	unsigned int n_items;
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

		n_items = (unsigned int) qc.maximum;

		if (!(c->pub.menu = calloc (n_items + 2, sizeof (char *))))
			goto failure;

		CLEAR (qm);

		for (j = 0; j <= n_items; j++) {
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

	do_get_control (p_info, C(tc));

	if (qc.id == V4L2_CID_AUDIO_MUTE) {
		p_info->mute = tc;
	}

	return TRUE;
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

	if (!info->video_standards)
		return TRUE;

	if (-1 == v4l25_ioctl (info, VIDIOC_G_STD, &std_id))
		return FALSE;

	for_all (s, info->video_standards)
		if (s->videostd_set == std_id)
			break;

	/* s = NULL = unknown. */

	store_cur_video_standard (info, s);
	set_audio_capability (P_INFO (info));

	return TRUE;
}

static tv_bool
set_video_standard		(tveng_device_info *	info,
				 const tv_video_standard *s)
{
	v4l2_std_id std_id;
	tv_videostd_set videostd_set;
	const tv_video_standard *t;
  capture_mode current_mode;
  gboolean overlay_was_active;
  tv_pixfmt pixfmt;
	int r;

	if (0 == v4l25_ioctl (info, VIDIOC_G_STD, &std_id)) {
		for_all (t, info->video_standards)
			if (t->videostd_set == std_id)
				break;

		if (t == s)
			return TRUE;
	}

  pixfmt = info->capture_format.pixfmt;
  current_mode = p_tveng_stop_everything(info, &overlay_was_active);

	videostd_set = s->videostd_set;

	r = v4l25_ioctl (info, VIDIOC_S_STD, &videostd_set);

	if (0 == r) {
		store_cur_video_standard (info, s);
		set_audio_capability (P_INFO (info));
	}

/* XXX bad idea */
  info->capture_format.pixfmt = pixfmt;
  p_tveng_set_capture_format(info);
  p_tveng_restart_everything(current_mode, overlay_was_active, info);

  return (0 == r);
}

static tv_bool
get_video_standard_list		(tveng_device_info *	info)
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

		if (-1 == v4l25_ioctl_nf (info, VIDIOC_ENUMSTD, &standard)) {
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

	if (get_video_standard (info))
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

	if (-1 == v4l25_ioctl (info, VIDIOC_G_FREQUENCY, &vfreq))
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
	int buf_type;
	tv_bool r;

	CLEAR (vfreq);
	vfreq.tuner = vi->tuner;
	vfreq.type = V4L2_TUNER_ANALOG_TV;
	vfreq.frequency = (frequency << vi->step_shift) / vi->pub.u.tuner.step;

	buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (info->cur_video_input == l
	    && CAPTURE_MODE_READ == info->capture_mode) {
		if (-1 == v4l25_ioctl (info, VIDIOC_STREAMOFF, &buf_type))
			return FALSE;
	}

	r = TRUE;

	if (0 == v4l25_ioctl (info, VIDIOC_S_FREQUENCY, &vfreq))
		store_frequency (info, vi, vfreq.frequency);
	else
		r = FALSE;

	if (info->cur_video_input == l
	    && CAPTURE_MODE_READ == info->capture_mode) {
		unsigned int i;

		/* Restart streaming (flush buffers). */

		for (i = 0; i < p_info->num_buffers; ++i) {
			struct v4l2_buffer buffer;

			tv_clear_image (p_info->buffers[i].vmem, 0,
					&info->capture_format);

			CLEAR (buffer);
			buffer.type = p_info->buffers[i].vidbuf.type;
			buffer.memory = V4L2_MEMORY_MMAP;
			buffer.index = i;

			if (-1 == v4l25_ioctl (info, VIDIOC_QBUF, &buffer)) {
				/* XXX suspended state */
				return FALSE;
			}
		}

		if (-1 == v4l25_ioctl (info, VIDIOC_STREAMON, &buf_type))
			r = FALSE; /* XXX suspended state */
	}

	return r;
}

static tv_bool
get_video_input			(tveng_device_info *	info)
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
	tv_callback_notify (info, info, info->priv->video_input_callback);

	if (l) {
		/* Implies get_video_standard() and set_audio_capability(). */
		get_video_standard_list (info);
	} else {
		free_video_standards (info);
	}

	return TRUE;
}

static tv_bool
set_video_input			(tveng_device_info *	info,
				 const tv_video_line *	l)
{
	capture_mode capture_mode;
	gboolean overlay_was_active;
	tv_pixfmt pixfmt;
	int index;

	if (info->cur_video_input) {
		if (0 == v4l25_ioctl (info, VIDIOC_G_INPUT, &index))
			if (CVI (l)->index == index)
				return TRUE;
	}

	pixfmt = info->capture_format.pixfmt;
	capture_mode = p_tveng_stop_everything(info, &overlay_was_active);

	index = CVI (l)->index;
	if (-1 == v4l25_ioctl (info, VIDIOC_S_INPUT, &index))
		return FALSE;

	store_cur_video_input (info, l);

	/* Implies get_video_standard() and set_audio_capability(). */
	/* XXX error? */
	get_video_standard_list (info);

	if (IS_TUNER_LINE (l)) {
		/* Set tuner audmode. */
		set_audio_mode (info, info->audio_mode);
	}

	info->capture_format.pixfmt = pixfmt;
	p_tveng_set_capture_format(info);

	/* XXX Start capturing again as if nothing had happened */
	p_tveng_restart_everything (capture_mode, overlay_was_active, info);

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

		if (-1 == v4l25_ioctl_nf (info, VIDIOC_ENUMINPUT, &input)) {
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
			if (!tuner_bounds_audio_capability (info, vi))
				goto failure;
	}

	get_video_input (info);

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
	Overlay
*/

static tv_bool
get_overlay_buffer		(tveng_device_info *	info)
{
	struct v4l2_framebuffer fb;

	if (-1 == v4l25_ioctl (info, VIDIOC_G_FBUF, &fb))
		return FALSE;

	/* XXX fb.capability, fb.flags ignored */

	CLEAR (info->overlay_buffer);

	info->overlay_buffer.format.pixfmt =
		pixelformat_to_pixfmt (fb.fmt.pixelformat);

	if (TV_PIXFMT_UNKNOWN == info->overlay_buffer.format.pixfmt)
		return TRUE;

	info->overlay_buffer.base = (unsigned long) fb.base;

	info->overlay_buffer.format.width		= fb.fmt.width;
	info->overlay_buffer.format.height		= fb.fmt.height;

	info->overlay_buffer.format.bytes_per_line	= fb.fmt.bytesperline;

	if (0 == fb.fmt.sizeimage) {
		info->overlay_buffer.format.size =
			fb.fmt.bytesperline * fb.fmt.height;
	} else {
		info->overlay_buffer.format.size = fb.fmt.sizeimage;
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

	if (-1 == v4l25_ioctl (info, VIDIOC_G_FMT, &format))
		return FALSE;

	info->overlay_window.x		= format.fmt.win.w.left;
	info->overlay_window.y		= format.fmt.win.w.top;
	info->overlay_window.width	= format.fmt.win.w.width;
	info->overlay_window.height	= format.fmt.win.w.height;

	/* Clips cannot be read back, we assume no change. */

	return TRUE;
}

static tv_bool
set_overlay_window		(tveng_device_info *	info,
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

	format.fmt.win.w.left		= w->x - info->overlay_buffer.x;
	format.fmt.win.w.top		= w->y - info->overlay_buffer.y;

	format.fmt.win.w.width		= w->width;
	format.fmt.win.w.height		= w->height;

	format.fmt.win.clips		= clips;
	format.fmt.win.clipcount	= v->size;

	format.fmt.win.chromakey	= p_info->chroma;

	if (-1 == v4l25_ioctl (info, VIDIOC_S_FMT, &format)) {
		free (clips);
		return FALSE;
	}

	free (clips);

	/* Actual window size. */

	info->overlay_window.x		= format.fmt.win.w.left;
	info->overlay_window.y		= format.fmt.win.w.top;
	info->overlay_window.width	= format.fmt.win.w.width;
	info->overlay_window.height	= format.fmt.win.w.height;

	return TRUE;
}

static tv_bool
enable_overlay			(tveng_device_info *	info,
				 tv_bool		on)
{
	int value = on;

	if (0 == v4l25_ioctl (info, VIDIOC_OVERLAY, &value)) {
		usleep (50000);
		return TRUE;
	} else {
		return FALSE;
	}
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

/*
	Capture
*/

static tv_bool
image_format_from_format	(tv_image_format *	f,
				 const struct v4l2_format *vfmt)
{
	tv_pixfmt pixfmt;
	unsigned int bytes_per_line;

	CLEAR (*f);

	pixfmt = pixelformat_to_pixfmt (vfmt->fmt.pix.pixelformat);

	if (TV_PIXFMT_UNKNOWN == pixfmt)
		return FALSE;

	/* bttv 0.9.12 bug:
	   returns bpl = width * bpp, w/bpp > 1 if planar YUV */
	bytes_per_line = vfmt->fmt.pix.width
		* tv_pixfmt_bytes_per_pixel (pixfmt);

	tv_image_format_init (f,
			      vfmt->fmt.pix.width,
			      vfmt->fmt.pix.height,
			      bytes_per_line,
			      pixfmt,
			      0);

	/* bttv 0.9.5 bug: */
	/* assert (f->fmt.pix.sizeimage >= info->capture_format.size); */

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

	if (-1 == v4l25_ioctl (info, VIDIOC_G_FMT, &format))
		return FALSE;

	/* Error ignored. */
	image_format_from_format (&info->capture_format, &format);

	return TRUE;
}

static tv_bool
set_capture_format		(tveng_device_info *	info,
				 const tv_image_format *f)
{
	struct v4l2_format format;

	CLEAR (format);

	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	/* bttv 0.9.14 YUV 4:2:0: see BUGS. */
	format.fmt.pix.pixelformat = pixfmt_to_pixelformat (f->pixfmt);

	if (0 == format.fmt.pix.pixelformat) {
		info->tveng_errno = -1; /* unknown */
		t_error_msg ("", "Bad pixfmt %u %s", info,
			     f->pixfmt, tv_pixfmt_name (f->pixfmt));
		return FALSE;
	}

	format.fmt.pix.width		= f->width;
	format.fmt.pix.height		= f->height;

	format.fmt.pix.bytesperline	= 0; /* minimum please */
	format.fmt.pix.sizeimage	= 0; /* ditto */

	if (format.fmt.pix.height > 288)
		format.fmt.pix.field = V4L2_FIELD_INTERLACED;
	else
		format.fmt.pix.field = V4L2_FIELD_BOTTOM;

	if (-1 == v4l25_ioctl (info, VIDIOC_S_FMT, &format))
		return FALSE;

	/* Actual image size. */

	/* Error ignored. */
	image_format_from_format (&info->capture_format, &format);

	return TRUE;
}

static tv_pixfmt_set
get_supported_pixfmt_set	(tveng_device_info *	info)
{
	struct v4l2_format format;
	tv_pixfmt_set pixfmt_set;
	tv_pixfmt pixfmt;

	CLEAR (format);

	format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	format.fmt.pix.width		= 352;
	format.fmt.pix.height		= 288;

	format.fmt.pix.bytesperline	= 0; /* minimum please */
	format.fmt.pix.sizeimage	= 0; /* ditto */

	format.fmt.pix.field = V4L2_FIELD_BOTTOM;

	pixfmt_set = TV_PIXFMT_SET_EMPTY;

	for (pixfmt = 0; pixfmt < TV_MAX_PIXFMTS; ++pixfmt) {
		/* bttv 0.9.14 YUV 4:2:0: see BUGS. */
		format.fmt.pix.pixelformat =
			pixfmt_to_pixelformat (pixfmt);

		if (0 == format.fmt.pix.pixelformat)
			continue;

		if (-1 == v4l25_ioctl_nf (info, VIDIOC_TRY_FMT, &format))
			continue;

		pixfmt_set |= TV_PIXFMT_SET (pixfmt);
	}

	if (0 != pixfmt_set)
		return pixfmt_set;

	/* TRY_FMT is optional, let's see if S_FMT works. */

	for (pixfmt = 0; pixfmt < TV_MAX_PIXFMTS; ++pixfmt) {
		/* bttv 0.9.14 YUV 4:2:0: see BUGS. */
		format.fmt.pix.pixelformat =
			pixfmt_to_pixelformat (pixfmt);

		if (0 == format.fmt.pix.pixelformat)
			continue;

		if (-1 == v4l25_ioctl_nf (info, VIDIOC_S_FMT, &format))
			continue;

		/* Error ignored. */
		image_format_from_format (&info->capture_format, &format);

		pixfmt_set |= TV_PIXFMT_SET (pixfmt);
	}

	return pixfmt_set;
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

  flags |= O_NONBLOCK;
  info -> fd = device_open(info->log_fp, info -> file_name, flags, 0);
  if (-1 == info -> fd)
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

  P_INFO (info)->bttv_driver = (0 == strcmp (caps.driver, "bttv"));

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
tveng25_describe_controller(const char ** short_str, const char ** long_str,
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
}


/*
  Functions for controlling the video capture. All of them return -1
  in case of error, so any value != -1 should be considered valid
  (unless explicitly stated in the description of the function) 
*/

static int
tveng25_update_capture_format(tveng_device_info * info)
{
	return get_capture_format (info) ? 0 : -1;
}


static int
tveng25_set_capture_format(tveng_device_info * info)
{
  capture_mode capture_mode;
  gboolean overlay_was_active;
  tv_pixfmt pixfmt;
  int result;

  pixfmt = info->capture_format.pixfmt;
  capture_mode = p_tveng_stop_everything(info, &overlay_was_active);
  info->capture_format.pixfmt = pixfmt;

  result = set_capture_format(info, &info->capture_format) ? 0 : -1;

  /* Start capturing again as if nothing had happened */
  p_tveng_restart_everything(capture_mode, overlay_was_active, info);

  return result;
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

  CLEAR (tuner);
  tuner.index = VI(info->cur_video_input)->tuner;

  /* XXX bttv 0.9.14 returns invalid .name? */
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
static int p_tveng25_qbuf(unsigned int index, tveng_device_info * info)
{
  struct v4l2_buffer tmp_buffer;
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;

  t_assert(info != NULL);

  CLEAR (tmp_buffer);
  tmp_buffer.type = p_info -> buffers[0].vidbuf.type;
  tmp_buffer.memory = V4L2_MEMORY_MMAP;
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

  CLEAR (tmp_buffer);
  tmp_buffer.type = p_info -> buffers[0].vidbuf.type;
  tmp_buffer.memory = V4L2_MEMORY_MMAP;

  /* XXX this blocks?? */
  if (-1 == v4l25_ioctl (info, VIDIOC_DQBUF, &tmp_buffer)) {
    int saved_errno;
    int buf_type;

    switch (errno) {
    case EAGAIN:
      break;

    default:
      saved_errno = errno;

      buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      /* bttv returns EIO on internal timeout and dequeues
	 the buffer it was about to write, does not
	 dequeue on subsequent timeouts.  On bt8x8 SCERR it
	 also resets.  To be safe we restart now, although
	 really the caller should do that. */
      /* XXX still not safe. */
      if (0 == v4l25_ioctl (info, VIDIOC_STREAMOFF, &buf_type)) {
	unsigned int i;
	
	for (i = 0; i < p_info->num_buffers; ++i) {
	  struct v4l2_buffer vbuf;
	  
	  CLEAR (vbuf);
	  vbuf.index = i;
	  vbuf.type = buf_type;
	  
	  if (-1 == v4l25_ioctl (info, VIDIOC_QBUF, &vbuf))
	    break;
	}

	if (i < p_info->num_buffers
	    || -1 == v4l25_ioctl (info, VIDIOC_STREAMOFF, &buf_type)) {
	  /* XXX suspended state */
	}
      } else {
	/* XXX suspended state */
      }

      errno = saved_errno;

      break;
    }

    return -1;
  }

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
  gboolean dummy;
  unsigned int i;

  t_assert(info != NULL);

  p_tveng_stop_everything(info,&dummy);

  t_assert(info -> capture_mode == CAPTURE_MODE_NONE);
  t_assert(p_info->num_buffers == 0);
  t_assert(p_info->buffers == NULL);

  p_info -> buffers = NULL;
  p_info -> num_buffers = 0;

  CLEAR (rb);
  rb.count = N_BUFFERS;
  rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  rb.memory = V4L2_MEMORY_MMAP;

  if (-1 == v4l25_ioctl (info, VIDIOC_REQBUFS, &rb))
      return -1;

  if (rb.count <= 1)
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
      CLEAR (p_info->buffers[i].vidbuf);

      p_info -> buffers[i].vidbuf.index = i;
      p_info -> buffers[i].vidbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      p_info -> buffers[i].vidbuf.memory = V4L2_MEMORY_MMAP;

      if (-1 == v4l25_ioctl (info, VIDIOC_QUERYBUF,
			     &p_info->buffers[i].vidbuf))
	  return -1;

      /* NB PROT_WRITE required since bttv 0.8,
         client may write mmapped buffers too. */
      p_info->buffers[i].vmem =
	mmap (0, p_info->buffers[i].vidbuf.length,
	      PROT_READ | PROT_WRITE,
	      MAP_SHARED, info->fd,
	      (int) p_info->buffers[i].vidbuf.m.offset);

      if (p_info->buffers[i].vmem == (void *) -1)
	p_info->buffers[i].vmem =
	  mmap(0, p_info->buffers[i].vidbuf.length,
	       PROT_READ, MAP_SHARED, info->fd, 
	       (int) p_info->buffers[i].vidbuf.m.offset);

      if (p_info->buffers[i].vmem == (void *) -1)
	{
	  info->tveng_errno = errno;
	  t_error("mmap()", info);
	  return -1;
	}

      tv_clear_image (p_info->buffers[i].vmem, 0,
		      &info->capture_format);

	/* Queue the buffer */
      if (p_tveng25_qbuf(i, info) == -1)
	return -1;
    }

  /* Turn on streaming */
  i = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == v4l25_ioctl (info, VIDIOC_STREAMON, &i))
      return -1;

  p_info -> last_timestamp = -1;

  info->capture_mode = CAPTURE_MODE_READ;

  return 0;
}

/* Tries to stop capturing. -1 on error. */
static int
tveng25_stop_capturing(tveng_device_info * info)
{
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;
  unsigned int i;

  if (info -> capture_mode == CAPTURE_MODE_NONE)
    {
      fprintf(stderr,
	      "Warning: trying to stop capture with no capture active\n");
      return 0; /* Nothing to be done */
    }

  t_assert(info->capture_mode == CAPTURE_MODE_READ);

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

  info->capture_mode = CAPTURE_MODE_NONE;

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
  int index; /* The dequeued buffer */
  fd_set rdset;
  struct timeval timeout;

  assert (time > 0);

  if (info -> capture_mode != CAPTURE_MODE_READ)
    {
      info -> tveng_errno = -1;
      t_error_msg("check", "Current capture mode is not READ (%d)",
		  info, info->capture_mode);
      return -1;
    }

  for (;;) {
    int r;

    /* Fill in the rdset structure */
    FD_ZERO(&rdset);
    FD_SET(info->fd, &rdset);
    timeout.tv_sec = 0;
    timeout.tv_usec = time*1000;

    r = select(info->fd +1, &rdset, NULL, NULL, &timeout);

    if (-1 == r)
      {
	info->tveng_errno = errno;
	t_error("select()", info);
	return -1;
      }
    else if (0 == r)
      {
	return 1; /* This isn't properly an error, just a timeout */
      }

    t_assert(FD_ISSET(info->fd, &rdset)); /* Some sanity check */

    if (-1 == (index = p_tveng25_dqbuf(info))) {
      if (EAGAIN == errno)
	continue;
      else
	return -1;
    }

    break;
  }

  /* Ignore frames we haven't been able to process */
  if (0) /* XXX? */
  do{
    int index2;

    FD_ZERO(&rdset);
    FD_SET(info->fd, &rdset);
    timeout.tv_sec = timeout.tv_usec = 0;
    if (select(info->fd +1, &rdset, NULL, NULL, &timeout) < 1)
      break;
    p_tveng25_qbuf((unsigned int) index, info);
    index2 = p_tveng25_dqbuf(info);
    if (index2 >= 0)
      index = index2;
  } while (1);

  /* Copy the data to the address given */
  if (where)
    tveng_copy_frame (p_info->buffers[index].vmem, where, info);

  /* Queue the buffer again for processing */
  if (p_tveng25_qbuf((unsigned int) index, info))
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

  if (info->capture_mode != CAPTURE_MODE_READ)
    return -1;

  return (p_info -> last_timestamp);
}


static void
reset_crop_rect			(tveng_device_info *	info)
{
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;

	CLEAR (cropcap);
	cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (-1 == v4l25_ioctl (info, VIDIOC_CROPCAP, &cropcap)) {
		/* Errors ignored. */
		return;
	}

	CLEAR (crop);
	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	crop.c = cropcap.defrect;

	if (-1 == v4l25_ioctl (info, VIDIOC_S_CROP, &crop)) {
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

  	if (TVENG_ATTACH_READ == attach_mode)
		reset_crop_rect	(info);

	/* We have a valid device, get some info about it */

	/* Video inputs & standards */

	info->video_inputs = NULL;
	info->cur_video_input = NULL;

	info->video_standards = NULL;
	info->cur_video_standard = NULL;

	if (!get_video_input_list (info))
		goto failure;

	/* Controls */

	info->controls = NULL;

	if (!get_control_list (info))
		goto failure;

	/* Overlay */

	if (info->caps.flags & TVENG_CAPS_OVERLAY) {
		if (!get_overlay_buffer (info))
			goto failure;

		if (!get_overlay_window (info))
			goto failure;
	} else {
		CLEAR (info->overlay_buffer);
		CLEAR (info->overlay_window);
	}

	/* Capture */

	if (info->caps.flags & TVENG_CAPS_CAPTURE) {
		if (!get_capture_format (info))
			goto failure;

		info->supported_pixfmt_set = get_supported_pixfmt_set (info);
	} else {
		CLEAR (info->capture_format);
		info->supported_pixfmt_set = TV_PIXFMT_SET_EMPTY;
	}

  /* Init the private info struct */
  p_info->num_buffers = 0;
  p_info->buffers = NULL;

  return info -> fd;

 failure:
  tveng25_close_device (info);
  return -1;
}

static struct tveng_module_info tveng25_module_info = {
  .attach_device =		tveng25_attach_device,
  .describe_controller =	tveng25_describe_controller,
  .close_device =		tveng25_close_device,

  .set_video_input		= set_video_input,
  .get_video_input		= get_video_input,
  .set_tuner_frequency		= set_tuner_frequency,
  .get_tuner_frequency		= get_tuner_frequency,
  .set_video_standard		= set_video_standard,
  .get_video_standard		= get_video_standard,
  .set_control			= set_control,
  .get_control			= get_control,
  .set_audio_mode		= set_audio_mode,

  .update_capture_format =	tveng25_update_capture_format,
  .set_capture_format =		tveng25_set_capture_format,
  .get_signal_strength =	tveng25_get_signal_strength,
  .start_capturing =		tveng25_start_capturing,
  .stop_capturing =		tveng25_stop_capturing,
  .read_frame =			tveng25_read_frame,
  .get_timestamp =		tveng25_get_timestamp,

  .get_overlay_buffer		= get_overlay_buffer,
  .set_overlay_window		= set_overlay_window,
  .get_overlay_window		= get_overlay_window,
  .enable_overlay		= enable_overlay,

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
