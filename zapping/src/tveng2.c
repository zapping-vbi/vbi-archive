/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 Iñaki García Etxebarria
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

/*
  This is the library in charge of simplifying Video Access API (I
  don't want to use thirteen lines of code with ioctl's every time I
  want to change tuning freq).
  the name is TV Engine, since it is intended mainly for TV viewing.
  This file is separated so zapping doesn't need to know about V4L[2]
*/
#include <site_def.h>

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
#define TVENG2_PROTOTYPES 1
#include "tveng2.h"

#include "zmisc.h"

/*
 *  Kernel interface
 */
#include "../common/videodev2.h"
#include "../common/fprintf_videodev2.h"

#define v4l2_ioctl(info, cmd, arg)					\
(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
 ((0 == device_ioctl ((info)->log_fp, fprintf_ioctl_arg,		\
		      (info)->fd, cmd, (void *)(arg))) ?		\
  0 : (ioctl_failure (info, __FILE__, __PRETTY_FUNCTION__,		\
		      __LINE__, # cmd), -1)))

struct video_input {
	tv_video_line		pub;
	int			index;		/* struct v4l2_input */
};

#define VI(l) PARENT (l, struct video_input, pub)

struct standard {
	tv_video_standard	pub;
	struct v4l2_enumstd	enumstd;
};

#define S(l) PARENT (l, struct standard, pub)

struct control {
	tv_control		pub;
	unsigned int		id;		/* V4L2_CID_ */
};

#define C(l) PARENT (l, struct control, pub)



struct tveng2_vbuf
{
  void * vmem; /* Captured image in this buffer */
  struct v4l2_buffer vidbuf; /* Info about the buffer */
};

struct private_tveng2_device_info
{
  tveng_device_info info; /* Info field, inherited */
  int num_buffers; /* Number of mmaped buffers */
  struct tveng2_vbuf * buffers; /* Array of buffers */
  double last_timestamp; /* The timestamp of the last captured buffer */
  uint32_t chroma;
  int audio_mode; /* 0 mono */
	tv_control *		mute;
	unsigned		read_back_controls	: 1;
};

#define P_INFO(p) PARENT (p, struct private_tveng2_device_info, info)

/*
  Functions for controlling the video capture. All of them return -1
  in case of error, so any value != -1 should be considered valid
  (unless explicitly stated in the description of the function) 
*/











static int
tveng2_update_capture_format(tveng_device_info * info)
{
  struct v4l2_format format;
  struct v4l2_window window;

  t_assert(info != NULL);

  memset(&format, 0, sizeof(struct v4l2_format));
  memset(&window, 0, sizeof(struct v4l2_window));

  format.type = V4L2_BUF_TYPE_CAPTURE;
  if (v4l2_ioctl(info, VIDIOC_G_FMT, &format) != 0)
      return -1;

  info->format.bpp = ((double)format.fmt.pix.depth)/8;
  info->format.width = format.fmt.pix.width;
  info->format.height = format.fmt.pix.height;
  if (format.fmt.pix.flags & V4L2_FMT_FLAG_BYTESPERLINE)
    info->format.bytesperline = format.fmt.pix.bytesperline;
  else
    info->format.bytesperline = info->format.bpp * info->format.width;
  info->format.sizeimage = format.fmt.pix.sizeimage;
  switch (format.fmt.pix.pixelformat)
    {
    case V4L2_PIX_FMT_RGB555:
      info->format.depth = 15;
      info->format.pixformat = TVENG_PIX_RGB555;
      break;
    case V4L2_PIX_FMT_RGB565:
      info->format.depth = 16;
      info->format.pixformat = TVENG_PIX_RGB565;
      break;
    case V4L2_PIX_FMT_RGB24:
      info->format.depth = 24;
      info->format.pixformat = TVENG_PIX_RGB24;
      break;
    case V4L2_PIX_FMT_BGR24:
      info->format.depth = 24;
      info->format.pixformat = TVENG_PIX_BGR24;
      break;
    case V4L2_PIX_FMT_RGB32:
      info->format.depth = 32;
      info->format.pixformat = TVENG_PIX_RGB32;
      break;
    case V4L2_PIX_FMT_BGR32:
      info->format.depth = 32;
      info->format.pixformat = TVENG_PIX_BGR32;
      break;
    case V4L2_PIX_FMT_YVU420:
      info->format.depth = 12;
      info->format.pixformat = TVENG_PIX_YVU420;
      break;
#ifdef V4L2_PIX_FMT_YUV420 /* not in the spec, but very common */
    case V4L2_PIX_FMT_YUV420:
      info->format.depth = 12;
      info->format.pixformat = TVENG_PIX_YUV420;
      break;
#endif
    case V4L2_PIX_FMT_UYVY:
      info->format.depth = 16;
      info->format.pixformat = TVENG_PIX_UYVY;
      break;
    case V4L2_PIX_FMT_YUYV:
      info->format.depth = 16;
      info->format.pixformat = TVENG_PIX_YUYV;
      break;
    case V4L2_PIX_FMT_GREY:
      info->format.depth = 8;
      info->format.pixformat = TVENG_PIX_GREY;
      break;
    default:
      info->tveng_errno = -1; /* unknown */
      t_error_msg("switch()",
		  "Cannot understand the actual palette", info);

      return -1;    
    };

  /* mhs: moved down here because tveng2_read_frame blamed
     info -> format.sizeimage != size after G_WIN failed */
  if (v4l2_ioctl(info, VIDIOC_G_WIN, &window) != 0)
      return -1;

  info->overlay_window.x = window.x;
  info->overlay_window.y = window.y;
  info->overlay_window.width = window.width;
  info->overlay_window.height = window.height;
  /* These two are defined as read-only */
// tv_clip_vector_clear (&info->overlay_window.clip_vector);

  return 0;
}

/* -1 if failed. Sets the pixformat and fills in info -> pix_format
   with the correct values  */
static int
tveng2_set_capture_format(tveng_device_info * info)
{
  struct v4l2_format format;

  t_assert(info != NULL);

  memset(&format, 0, sizeof(struct v4l2_format));

  format.type = V4L2_BUF_TYPE_CAPTURE;

  /* Transform the given palette value into a V4L value */
  switch(info->format.pixformat)
    {
    case TVENG_PIX_RGB555:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB555;
      format.fmt.pix.depth = 15;
      break;
    case TVENG_PIX_RGB565:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
      format.fmt.pix.depth = 16;
      break;
    case TVENG_PIX_RGB24:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
      format.fmt.pix.depth = 24;
      break;
    case TVENG_PIX_BGR24:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
      format.fmt.pix.depth = 24;
      break;
    case TVENG_PIX_RGB32:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB32;
      format.fmt.pix.depth = 32;
      break;
    case TVENG_PIX_BGR32:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR32;
      format.fmt.pix.depth = 32;
      break;
    case TVENG_PIX_YUV420:
#ifdef V4L2_PIX_FMT_YUV420 /* not in the spec, but very common */
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
      format.fmt.pix.depth = 12;
      break;
#endif
    case TVENG_PIX_YVU420:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_YVU420;
      format.fmt.pix.depth = 12;
      break;
    case TVENG_PIX_UYVY:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
      format.fmt.pix.depth = 16;
      break;
    case TVENG_PIX_YUYV:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
      format.fmt.pix.depth = 16;
      break;
    default:
      info->tveng_errno = -1; /* unknown */
      t_error_msg("switch()", "Cannot understand the given palette",
		  info);
      return -1;
    }

  /* Adjust the given dimensions */
  if (info->format.height < info->caps.minheight)
    info->format.height = info->caps.minheight;
  if (info->format.height > info->caps.maxheight)
    info->format.height = info->caps.maxheight;
  if (info->format.width < info->caps.minwidth)
    info->format.width = info->caps.minwidth;
  if (info->format.width > info->caps.maxwidth)
    info->format.width = info->caps.maxwidth;
  
  format.fmt.pix.width = info->format.width;
  format.fmt.pix.height = info->format.height;
  format.fmt.pix.bytesperline = ((format.fmt.pix.depth+7)>>3) *
    format.fmt.pix.width;
  format.fmt.pix.sizeimage =
    format.fmt.pix.bytesperline * format.fmt.pix.height;
  format.fmt.pix.flags = V4L2_FMT_FLAG_INTERLACED;

  /* everything is set up */
  if (v4l2_ioctl(info, VIDIOC_S_FMT, &format) != 0)
      return -1;

  /* Check fill in info with the current values (may not be the ones
     requested) */
  tveng2_update_capture_format(info);

  return 0; /* Success */
}












/*
 *  Controls
 */

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

/* Preliminary */
#define TVENG2_AUDIO_MAGIC 0x1234

static const char *
audio_decoding_modes [] = {
	N_("Automatic"),
	N_("Mono"),
	N_("Stereo"),
	N_("Alternate 1"),
	N_("Alternate 2"),
};

static tv_bool
do_update_control		(struct private_tveng2_device_info *p_info,
				 struct control *	c)
{
	struct v4l2_control ctrl;

	if (c->id == TVENG2_AUDIO_MAGIC) {
		/* FIXME actual value */
		c->pub.value = p_info->audio_mode;
		return TRUE;
	} else if (c->id == V4L2_CID_AUDIO_MUTE) {
		/*
		  Doesn't seem to work with bttv.
		  FIXME we should check at runtime.
		*/
		return TRUE;
	}

	ctrl.id = c->id;

	if (-1 == v4l2_ioctl (&p_info->info, VIDIOC_G_CTRL, &ctrl))
		return FALSE;

	if (c->pub.value != ctrl.value) {
		c->pub.value = ctrl.value;
		tv_callback_notify (&c->pub, c->pub._callback);
	}

	return TRUE;
}

static tv_bool
update_control			(tveng_device_info *	info,
				 tv_control *		tc)
{
	struct private_tveng2_device_info * p_info = P_INFO (info);

	if (tc)
		return do_update_control (p_info, C(tc));

	for (tc = p_info->info.controls; tc; tc = tc->_next)
		if (tc->_parent == info)
			if (!do_update_control (p_info, C(tc)))
				return FALSE;

	return TRUE;
}

static tv_bool
set_control			(tveng_device_info *	info,
				 tv_control *		tc,
				 int			value)
{
	struct private_tveng2_device_info * p_info = P_INFO (info);
	struct v4l2_control ctrl;

	if (C(tc)->id == TVENG2_AUDIO_MAGIC) {
		if (info->cur_video_input
		    && (info->cur_video_input->type
			== TV_VIDEO_LINE_TYPE_TUNER)) {
			struct v4l2_tuner tuner;

			memset(&tuner, 0, sizeof(tuner));
			tuner.input = VI(info->cur_video_input)->index;

			/* XXX */
			if (0 == v4l2_ioctl(info, VIDIOC_G_TUNER, &tuner)) {
				tuner.audmode = "\0\1\3\2"[value];

				if (0 == v4l2_ioctl(info, VIDIOC_S_TUNER, &tuner)) {
					p_info->audio_mode = value;
				}
			}
		} else {
			p_info->audio_mode = 0; /* mono */
		}

		return TRUE;
	}

	ctrl.id = C(tc)->id;
	ctrl.value = value;

	if (-1 == v4l2_ioctl(&p_info->info, VIDIOC_S_CTRL, &ctrl)) {
		return FALSE;
	}

	if (p_info->read_back_controls) {
		/*
		  Doesn't seem to work with bttv.
		  FIXME we should check at runtime.
		*/
		if (ctrl.id == V4L2_CID_AUDIO_MUTE) {
			if (tc->value != value) {
				tc->value = value;
				tv_callback_notify (tc, tc->_callback);
			}
			return TRUE;
		}

		return do_update_control (p_info, C(tc));
	} else {
		if (tc->value != value) {
			tc->value = value;
			tv_callback_notify (tc, tc->_callback);
		}

		return TRUE;
	}
}

static tv_bool
add_control			(tveng_device_info *	info,
				 unsigned int		id)
{
	struct private_tveng2_device_info * p_info = P_INFO (info);
	struct v4l2_queryctrl qc;
	struct v4l2_querymenu qm;
	struct control *c;
	tv_control *tc;
	unsigned int i, j;

	CLEAR (qc);

	qc.id = id;

	/* XXX */
	if (-1 == v4l2_ioctl (info, VIDIOC_QUERYCTRL, &qc))
		return FALSE;

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
	case V4L2_CTRL_TYPE_INTEGER:	c->pub.type = TV_CONTROL_TYPE_INTEGER; break;
	case V4L2_CTRL_TYPE_BOOLEAN:	c->pub.type = TV_CONTROL_TYPE_BOOLEAN; break;
	case V4L2_CTRL_TYPE_BUTTON:	c->pub.type = TV_CONTROL_TYPE_ACTION; break;

	case V4L2_CTRL_TYPE_MENU:
		c->pub.type = TV_CONTROL_TYPE_CHOICE;

		if (!(c->pub.menu = calloc (qc.maximum + 2, sizeof (char *))))
			goto failure;

		for (j = 0; j <= qc.maximum; j++) {
			qm.id = qc.id;
			qm.index = j;

			if (0 == v4l2_ioctl (info, VIDIOC_QUERYMENU, &qm)) {
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

/* Private, builds the controls structure */
static int
p_tveng2_build_controls(tveng_device_info * info)
{
	unsigned int cid;

	for (cid = V4L2_CID_BASE;
	     cid < V4L2_CID_LASTP1; cid++) {
		/* EINVAL ignored */
		add_control (info, cid);
	}

	/* Artificial control (preliminary) */

	/* Check that there are tuners in the current input */
	if (info->cur_video_input
	    && (info->cur_video_input->type
		== TV_VIDEO_LINE_TYPE_TUNER)) {
		struct v4l2_tuner tuner;

		memset(&tuner, 0, sizeof(tuner));
		tuner.input = VI(info->cur_video_input)->index;

		if (v4l2_ioctl(info, VIDIOC_G_TUNER, &tuner) == 0) {
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

				c->id = TVENG2_AUDIO_MAGIC;
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
	     cid < V4L2_CID_PRIVATE_BASE + 100; cid++) {
		if (!add_control (info, cid))
			break; /* end of enumeration */
	}

	return 0;
}

/*
 *  Video standards
 */

/* The video_standard_id concept doesn't exist in v4l2 0.20, we
   have to derive the ID from other information. */
/* FIXME doesn't properly translate all bttv standards. */
static tv_video_standard_id
v4l2_standard_to_video_standard_id
				(struct v4l2_standard *	std,
				 unsigned int *		custom)
{
	if (std->framelines == 625) {
		switch (std->colorstandard) {
		case V4L2_COLOR_STD_PAL:
			switch (std->colorstandard_data.pal.colorsubcarrier) {
			case V4L2_COLOR_SUBC_PAL:
				return TV_VIDEOSTD_PAL; /* BGDKHI */
			case V4L2_COLOR_SUBC_PAL_N:
				return TV_VIDEOSTD_PAL_N;
			default:
				goto assume_custom;
			}
		case V4L2_COLOR_STD_NTSC:
			goto assume_custom; /* ? */
		case V4L2_COLOR_STD_SECAM:
			return TV_VIDEOSTD_SECAM; /* BDGHKK1L, although probably L */
		}
	} else if (std->framelines == 525) {
		switch (std->colorstandard) {
		case V4L2_COLOR_STD_PAL:
			switch (std->colorstandard_data.pal.colorsubcarrier) {
			case V4L2_COLOR_SUBC_PAL_M:
				return TV_VIDEOSTD_PAL_M;
			default:
				goto assume_custom; /* "PAL-60" */
			}

		case V4L2_COLOR_STD_NTSC:
			return TV_VIDEOSTD_NTSC; /* M/NTSC USA or Japan */
		}
	}

 assume_custom:
	if (*custom >= sizeof (tv_video_standard_id) * 8)
		return TV_VIDEOSTD_UNKNOWN;
	else
		return ((tv_video_standard_id) 1) << ((*custom)++);
}

static tv_bool
update_standard_list		(tveng_device_info *	info)
{
	unsigned int i;
	unsigned int custom;

	free_video_standards (info);

	if (!info->cur_video_input)
		return TRUE;

	custom = 32;

	for (i = 0;; ++i) {
		struct v4l2_enumstd enumstd;
		char buf[sizeof (enumstd.std.name) + 1];
		struct standard *s;
		tv_video_standard_id id;

		CLEAR (enumstd);

		enumstd.index = i;

		if (-1 == v4l2_ioctl (info, VIDIOC_ENUMSTD, &enumstd)) {
			if (errno == EINVAL && i > 0)
				break; /* end of enumeration */

			free_video_standard_list (&info->video_standards);
			return FALSE;
		}

		if ((enumstd.inputs
		     & (1 << VI(info->cur_video_input)->index)) == 0)
			continue; /* unsupported by the current input */

		id = v4l2_standard_to_video_standard_id (&enumstd.std, &custom);

		if (id == TV_VIDEOSTD_UNKNOWN)
			continue;

		/* Sometimes NUL is missing. */
		z_strlcpy (buf, enumstd.std.name, sizeof (buf));

		s = S(append_video_standard (&info->video_standards, id,
					     enumstd.std.name,
					     enumstd.std.name,
					     sizeof (*s)));
		if (!s) {
			free_video_standard_list (&info->video_standards);
			return FALSE;
		}

		s->pub.frame_rate =
			enumstd.std.framerate.denominator
			/ (double) enumstd.std.framerate.numerator;

		s->enumstd = enumstd;
	}

	return TRUE;
}	

static tv_bool
compare_standard		(struct v4l2_standard *	s1,
				 struct v4l2_standard *	s2)
{
	if (s1->framelines != s2->framelines
	    || s1->transmission != s2->transmission
	    || s1->colorstandard != s2->colorstandard
	    || s1->framerate.numerator != s2->framerate.numerator
	    || s1->framerate.denominator != s2->framerate.denominator)
		return FALSE;

	switch (s1->colorstandard) {
	case V4L2_COLOR_STD_PAL:
		return (s1->colorstandard_data.pal.colorsubcarrier
			== s1->colorstandard_data.pal.colorsubcarrier);

	case V4L2_COLOR_STD_NTSC:
		return (s1->colorstandard_data.ntsc.colorsubcarrier
			== s1->colorstandard_data.ntsc.colorsubcarrier);

	case V4L2_COLOR_STD_SECAM:
		return ((s1->colorstandard_data.secam.f0b
			 == s1->colorstandard_data.secam.f0b)
			&& (s1->colorstandard_data.secam.f0r
			    == s1->colorstandard_data.secam.f0r));

	default: /* ? */
		return FALSE;
	}
}

static tv_bool
update_current_standard		(tveng_device_info *	info)
{
	struct v4l2_standard standard;
	tv_video_standard *ts;

	ts = NULL;

	if (!info->video_standards)
		goto finish;

	CLEAR (standard);

	if (-1 == v4l2_ioctl (info, VIDIOC_G_STD, &standard))
		return FALSE;

	/* We're not really supposed to look up the standard
	   but use the parameters directly, permitting custom
	   parameter combinations. */
	for (ts = info->video_standards; ts; ts = ts->_next)
		if (compare_standard (&S(ts)->enumstd.std, &standard))
			break;

 finish:
	set_cur_video_standard (info, ts);

	return TRUE;
}

static tv_bool
set_standard			(tveng_device_info *	info,
				 const tv_video_standard *	ts)
{
  enum tveng_capture_mode current_mode;
  enum tveng_frame_pixformat pixformat;
  int r;

  t_assert(info != NULL);
  t_assert(ts != NULL);

  pixformat = info->format.pixformat;
  current_mode = tveng_stop_everything(info);

	r = v4l2_ioctl (info, VIDIOC_S_STD, &S(ts)->enumstd.std);

	if (0 == r)
		set_cur_video_standard (info, ts);

// XXX bad idea
  info->format.pixformat = pixformat;
  tveng_set_capture_format(info);
  tveng_restart_everything(current_mode, info);

  return (0 == r);
}

/*
 *  Video inputs
 */

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

		if (-1 == v4l2_ioctl (info, VIDIOC_ENUMINPUT, &input)) {
			if (errno == EINVAL && i > 0)
				break; /* end of enumeration */

			free_video_line_list (&info->video_inputs);
			return FALSE;
		}

		/* Sometimes NUL is missing. */
		z_strlcpy (buf, input.name, sizeof (buf));

		if (input.type & V4L2_INPUT_TYPE_TUNER)
			type = TV_VIDEO_LINE_TYPE_TUNER;
		else
			type = TV_VIDEO_LINE_TYPE_BASEBAND;

		if (!(vi = VI (append_video_line (&info->video_inputs,
						  type, buf, buf,
						  sizeof (*vi)))))
			goto failure;

		vi->index = i;
	}

	return TRUE;

 failure:
	free_video_line_list (&info->video_inputs);
	return FALSE;
}

#if 0
if (input.capability & V4L2_INPUT_CAP_AUDIO)
info->inputs[i].flags |= TVENG_INPUT_AUDIO;
#endif

static tv_bool
update_current_video_input	(tveng_device_info *	info)
{
	tv_video_line *tl;
	tv_video_line *old;
	int index;

	tl = NULL;

	if (!info->video_inputs)
		goto finish;

	if (-1 == v4l2_ioctl (info, VIDIOC_G_INPUT, &index)) {
		return FALSE;
	}

	for_all (tl, info->video_inputs)
		if (VI(tl)->index == index)
			break;

 finish:
	old = info->cur_video_input;

	set_cur_video_input (info, tl);

	if (tl) {
		if (old != tl)
			if (update_standard_list (info))
				update_current_standard (info);
	} else {
		free_video_standards (info);
	}

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

		if (0 == v4l2_ioctl (info, VIDIOC_G_INPUT, &index))
			if (VI(l)->index == index)
				return TRUE;
	}

	pixformat = info->format.pixformat;
	current_mode = tveng_stop_everything(info);

	if (-1 == v4l2_ioctl (info, VIDIOC_S_INPUT, &VI(l)->index))
		return FALSE;

	set_cur_video_input (info, l);

	// XXX error?
	if (update_standard_list (info))
		update_current_standard (info);

	info->format.pixformat = pixformat;
	tveng_set_capture_format(info);

	/* XXX Start capturing again as if nothing had happened */
	tveng_restart_everything (current_mode, info);

	return TRUE;
}





/*
  Tunes the current input to the given freq. Returns -1 on error.
*/
static int
tveng2_tune_input(uint32_t _freq, tveng_device_info * info)
{
  struct v4l2_tuner tuner_info;
  uint32_t freq; /* real frequence passed to v4l2 */

  t_assert(info != NULL);

  if (!TUNER_LINE (info->cur_video_input))
    return 0; /* Success (we shouldn't be tuning, anyway) */

  /* Get more info about this tuner */
  tuner_info.input = VI(info->cur_video_input)->index;
  if (v4l2_ioctl(info, VIDIOC_G_TUNER, &tuner_info) != 0)
      return -1;
  
  if (tuner_info.capability & V4L2_TUNER_CAP_LOW)
    freq = _freq / 0.0625;
  else
    freq = _freq / 62.5;

  if (freq > tuner_info.rangehigh)
    freq = tuner_info.rangehigh;
  if (freq < tuner_info.rangelow)
    freq = tuner_info.rangelow;
  
  /* OK, everything is set up, try to tune it */
  if (v4l2_ioctl(info, VIDIOC_S_FREQ, &freq) != 0)
      return -1;

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
tveng2_get_signal_strength (int *strength, int * afc,
			   tveng_device_info * info)
{
  struct v4l2_tuner tuner;

  t_assert(info != NULL);

  if (!TUNER_LINE (info->cur_video_input))
    return -1;

  tuner.input = VI(info->cur_video_input)->index;
  if (v4l2_ioctl(info, VIDIOC_G_TUNER, &tuner) != 0)
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

/*
  Stores in freq the currently tuned freq. Returns -1 on error.
*/
static int
tveng2_get_tune(uint32_t * freq, tveng_device_info * info)
{
  uint32_t real_freq;
  struct v4l2_tuner tuner;

  t_assert(info != NULL);
  t_assert(freq != NULL);

  if (!TUNER_LINE (info->cur_video_input))
    {
      if (freq)
	*freq = 0;
      info->tveng_errno = -1;
      t_error_msg("tuners check",
		  "There are no tuners for the active input",
		  info);
      return -1;
    }

  if (v4l2_ioctl(info, VIDIOC_G_FREQ, &real_freq) != 0)
      return -1;

  /* Get more info about this tuner */
  tuner.input = VI(info->cur_video_input)->index;
  if (v4l2_ioctl(info, VIDIOC_G_TUNER, &tuner) != 0)
      return -1;
  
  if (tuner.capability & V4L2_TUNER_CAP_LOW)
    *freq = real_freq * 0.0625;
  else
    *freq = real_freq * 62.5;

  return 0;
}

/*
  Gets the minimum and maximum freq that the current input can
  tune. If there is no tuner in this input, -1 will be returned.
  If any of the pointers is NULL, its value will not be filled.
*/
static int
tveng2_get_tuner_bounds(uint32_t * min, uint32_t * max, tveng_device_info *
			info)
{
  struct v4l2_tuner tuner;

  t_assert(info != NULL);

  if (!TUNER_LINE (info->cur_video_input))
    return -1;

  /* Get info about the current tuner */
  tuner.input = VI(info->cur_video_input)->index;
  if (v4l2_ioctl(info, VIDIOC_G_TUNER, &tuner) != 0)
      return -1;

  if (min)
    *min = tuner.rangelow;
  if (max)
    *max = tuner.rangehigh;

  if (tuner.capability & V4L2_TUNER_CAP_LOW)
    {
      if (min)
	*min *= 0.0625;
      if (max)
	*max *= 0.0625;
    }
  else
    {
      if (min)
	*min *= 62.5;
      if (max)
	*max *= 62.5;
    }

  return 0; /* Success */
}

/* Some private functions */
/* Queues an specific buffer. -1 on error */
static int p_tveng2_qbuf(int index, tveng_device_info * info)
{
  struct v4l2_buffer tmp_buffer;
  struct private_tveng2_device_info * p_info =
    (struct private_tveng2_device_info*) info;

  t_assert(info != NULL);

  tmp_buffer.type = p_info -> buffers[0].vidbuf.type;
  tmp_buffer.index = index;

  if (v4l2_ioctl(info, VIDIOC_QBUF, &tmp_buffer) != 0)
      return -1;

  return 0;
}

/* dequeues next available buffer and returns it's id. -1 on error */
static int p_tveng2_dqbuf(tveng_device_info * info)
{
  struct v4l2_buffer tmp_buffer;
  struct private_tveng2_device_info * p_info =
    (struct private_tveng2_device_info*) info;
  
  t_assert(info != NULL);

  tmp_buffer.type = p_info -> buffers[0].vidbuf.type;

  if (v4l2_ioctl(info, VIDIOC_DQBUF, &tmp_buffer) != 0)
      return -1;

  p_info -> last_timestamp = tmp_buffer.timestamp /1e9;

  return (tmp_buffer.index);
}

/*
  Sets up the capture device so any read() call after this one
  succeeds. Returns -1 on error.
*/
static int
tveng2_start_capturing(tveng_device_info * info)
{
  struct v4l2_requestbuffers rb;
  struct private_tveng2_device_info * p_info =
    (struct private_tveng2_device_info*) info;
  int i;

  t_assert(info != NULL);

  tveng_stop_everything(info);

  t_assert(info -> current_mode == TVENG_NO_CAPTURE);
  t_assert(p_info->num_buffers == 0);
  t_assert(p_info->buffers == NULL);

  p_info -> buffers = NULL;
  p_info -> num_buffers = 0;

  rb.count = 8; /* This is a good number(tm) */
  rb.type = V4L2_BUF_TYPE_CAPTURE;
  if (v4l2_ioctl(info, VIDIOC_REQBUFS, &rb) != 0)
      return -1;

  if (rb.count <= 2)
    {
      info->tveng_errno = -1;
      t_error_msg("check()", "Not enough buffers", info);
      return -1;
    }

  p_info -> buffers = (struct tveng2_vbuf*)
    malloc(rb.count*sizeof(struct tveng2_vbuf));
  p_info -> num_buffers = rb.count;

  for (i = 0; i < rb.count; i++)
    {
      p_info -> buffers[i].vidbuf.index = i;
      p_info -> buffers[i].vidbuf.type = V4L2_BUF_TYPE_CAPTURE;
      if (v4l2_ioctl(info, VIDIOC_QUERYBUF,
		&(p_info->buffers[i].vidbuf)) != 0)
	  return -1;

      /* bttv 0.8.x wants PROT_WRITE although AFAIK we don't. */
      p_info->buffers[i].vmem =
	mmap (0, p_info->buffers[i].vidbuf.length,
	      PROT_READ | PROT_WRITE,
	      MAP_SHARED, info->fd,
	      p_info->buffers[i].vidbuf.offset);

      if (p_info->buffers[i].vmem == (void *) -1)
	p_info->buffers[i].vmem =
	  mmap(0, p_info->buffers[i].vidbuf.length,
	       PROT_READ, MAP_SHARED, info->fd, 
	       p_info->buffers[i].vidbuf.offset);

      if (p_info->buffers[i].vmem == (void *) -1)
	{
	  info->tveng_errno = errno;
	  t_error("mmap()", info);
	  return -1;
	}

	/* Queue the buffer */
      if (p_tveng2_qbuf(i, info) == -1)
	return -1;
    }

  /* Turn on streaming */
  i = V4L2_BUF_TYPE_CAPTURE;
  if (v4l2_ioctl(info, VIDIOC_STREAMON, &i) != 0)
      return -1;

  p_info -> last_timestamp = -1;

  info->current_mode = TVENG_CAPTURE_READ;

  return 0;
}

/* Tries to stop capturing. -1 on error. */
static int
tveng2_stop_capturing(tveng_device_info * info)
{
  struct private_tveng2_device_info * p_info =
    (struct private_tveng2_device_info*) info;
  int i;

  if (info -> current_mode == TVENG_NO_CAPTURE)
    {
      fprintf(stderr,
	      "Warning: trying to stop capture with no capture active\n");
      return 0; /* Nothing to be done */
    }

  t_assert(info->current_mode == TVENG_CAPTURE_READ);

  /* Turn streaming off */
  i = V4L2_BUF_TYPE_CAPTURE;
  if (v4l2_ioctl(info, VIDIOC_STREAMOFF, &i) != 0)
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
int tveng2_read_frame(tveng_image_data * where, 
		      unsigned int time, tveng_device_info * info)
{
  struct private_tveng2_device_info * p_info =
    (struct private_tveng2_device_info*) info;
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

  n = p_tveng2_dqbuf(info);
  if (n == -1)
    return -1;

  /* Ignore frames we haven't been able to process */
  do{
    FD_ZERO(&rdset);
    FD_SET(info->fd, &rdset);
    timeout.tv_sec = timeout.tv_usec = 0;
    if (select(info->fd +1, &rdset, NULL, NULL, &timeout) < 1)
      break;
    p_tveng2_qbuf(n, info);
    n = p_tveng2_dqbuf(info);
  } while (1);

  /* Copy the data to the address given */
  if (where)
    tveng_copy_frame (p_info->buffers[n].vmem, where, info);

  /* Queue the buffer again for processing */
  if (p_tveng2_qbuf(n, info))
    return -1;

  /* Everything has been OK, return 0 (success) */
  return 0;
}

/*
  Gets the timestamp of the last read frame in seconds.
  Returns -1 on error, if the current mode isn't capture, or if we
  haven't captured any frame yet.
*/
static double tveng2_get_timestamp(tveng_device_info * info)
{
  struct private_tveng2_device_info * p_info =
    (struct private_tveng2_device_info *) info;

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
int tveng2_set_capture_size(int width, int height, tveng_device_info * info)
{
  enum tveng_capture_mode current_mode;
  int retcode;

  t_assert(info != NULL);
  t_assert(width > 0);
  t_assert(height > 0);

  tveng2_update_capture_format(info);

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
  retcode = tveng2_set_capture_format(info);

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
int tveng2_get_capture_size(int *width, int *height, tveng_device_info * info)
{
  t_assert(info != NULL);

  if (tveng2_update_capture_format(info))
    return -1;

  if (width)
    *width = info->format.width;
  if (height)
    *height = info->format.height;

  return 0; /* Success */
}

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

	if (-1 == v4l2_ioctl (info, VIDIOC_G_FBUF, &fb)) {
		return FALSE;
	}

	// XXX fb.capability, fb.flags ignored

	t->base			= fb.base[0];

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
		break;
	case V4L2_PIX_FMT_RGB565:
		t->pixfmt = TVENG_PIX_RGB565;
		break;
	case V4L2_PIX_FMT_RGB24:
		t->pixfmt = TVENG_PIX_RGB24;
		break;
	case V4L2_PIX_FMT_BGR24:
		t->pixfmt = TVENG_PIX_BGR24;
		break;
	case V4L2_PIX_FMT_RGB32:
		t->pixfmt = TVENG_PIX_RGB32;
		break;
	case V4L2_PIX_FMT_BGR32:
		t->pixfmt = TVENG_PIX_BGR32;
		break;
	case V4L2_PIX_FMT_RGB332:
	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_RGB565X:
	default:
		CLEAR (*t);
		return FALSE;
	}

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
tveng2_set_preview_window(tveng_device_info * info)
{
	struct private_tveng2_device_info * p_info = P_INFO (info);
	struct v4l2_window window;

	t_assert(info != NULL);

	CLEAR (window);

	window.x		= info->overlay_window.x;
	window.y		= info->overlay_window.y;
	window.width		= info->overlay_window.width;
	window.height		= info->overlay_window.height;
	window.clipcount	= info->overlay_window.clip_vector.size;
	window.chromakey	= p_info->chroma;

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

		window.clips = vclip;
		tclip = info->overlay_window.clip_vector.vector;

		for (i = 0; i < info->overlay_window.clip_vector.size; ++i) {
			vclip->next	= vclip + 1;
			vclip->x	= tclip->x;
			vclip->y	= tclip->y;
			vclip->width	= tclip->width;
			vclip->height	= tclip->height;
			++vclip;
			++tclip;
		}

		vclip[-1].next = NULL;
	}

	tveng_set_preview_off (info);

	/* Set the new window */
	if (-1 == v4l2_ioctl (info, VIDIOC_S_WIN, &window)) {
		free (window.clips);
		return -1;
	}

	free (window.clips);

	if (-1 == v4l2_ioctl (info, VIDIOC_G_WIN, &window)) {
		return -1;
	}

	info->overlay_window.x		= window.x;
	info->overlay_window.y		= window.y;
	info->overlay_window.width	= window.width;
	info->overlay_window.height	= window.height;

	/* Clips cannot be read back, we assume no change. */

	return 0;
}

/*
  Gets the current overlay window parameters.
  Returns -1 on error, and any other value on success.
  info   : The device to use
*/
static int
tveng2_get_preview_window(tveng_device_info * info)
{
  /* Updates the entire capture format, since there is no
     difference */
  return (tveng2_update_capture_format(info));
}

static tv_bool
set_overlay			(tveng_device_info *	info,
				 tv_bool		on)
{
	int value = on;

	if (-1 == v4l2_ioctl (info, VIDIOC_PREVIEW, &value)) {
		return FALSE;
	}

	return TRUE;
}


static void
tveng2_set_chromakey		(uint32_t chroma, tveng_device_info *info)
{
  struct private_tveng2_device_info * p_info =
    (struct private_tveng2_device_info*) info;

  p_info->chroma = chroma;

  /* Will be set in the next set_window call */
}

static int
tveng2_get_chromakey		(uint32_t *chroma, tveng_device_info *info)
{
  struct private_tveng2_device_info * p_info =
    (struct private_tveng2_device_info*) info;

  *chroma = p_info->chroma;

  return 0;
}

/* Private, builds the controls structure */
static int
p_tveng2_build_controls(tveng_device_info * info);

static int p_tveng2_open_device_file(int flags, tveng_device_info * info);
/*
  Return fd for the device file opened. Checks if the device is a
  valid video device. -1 on error.
  Flags will be used for open()'ing the file 
*/
static int p_tveng2_open_device_file(int flags, tveng_device_info * info)
{
  struct v4l2_capability caps;
  struct v4l2_framebuffer fb;
  
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
  memset(&caps, 0, sizeof(struct v4l2_capability));
  memset(&fb, 0, sizeof(struct v4l2_framebuffer));

  if (v4l2_ioctl(info, VIDIOC_QUERYCAP, &caps) != 0)
    {
      device_close (info->log_fp, info -> fd);
      return -1;
    }

  /* Check if this device is convenient for us */
  if (caps.type != V4L2_TYPE_CAPTURE)
    {
      info->tveng_errno = -1;
      snprintf(info->error, 256, 
	       _("%s doesn't look like a valid capture device"), info
	       -> file_name);
      device_close(info->log_fp, info -> fd);
      return -1;
    }

  /* Check if we can select() and mmap() this device */
  if (!(caps.flags & V4L2_FLAG_STREAMING))
    {
      info -> tveng_errno = -1;
      snprintf(info->error, 256,
	       _("Sorry, but \"%s\" cannot do streaming"),
	       info -> file_name);
      device_close(info->log_fp, info -> fd);
      return -1;
    }

  if (!(caps.flags & V4L2_FLAG_SELECT))
    {
      info->tveng_errno = -1;
      snprintf(info->error, 256, 
	       _("Sorry, but \"%s\" cannot do select() on file descriptors"),
	       info -> file_name);
      device_close(info->log_fp, info -> fd);
      return -1;
    }

  /* Copy capability info */
  snprintf(info->caps.name, 32, caps.name);
  info->caps.channels = caps.inputs;
  info->caps.audios = caps.audios;
  info->caps.maxwidth = caps.maxwidth;
  info->caps.minwidth = caps.minwidth;
  info->caps.maxheight = caps.maxheight;
  info->caps.minheight = caps.minheight;
  info->caps.flags = 0;

  info->caps.flags |= TVENG_CAPS_CAPTURE; /* This has been tested before */

  if (caps.flags & V4L2_FLAG_TUNER)
    info->caps.flags |= TVENG_CAPS_TUNER;
  if (caps.flags & V4L2_FLAG_DATA_SERVICE)
    info->caps.flags |= TVENG_CAPS_TELETEXT;
  if (caps.flags & V4L2_FLAG_MONOCHROME)
    info->caps.flags |= TVENG_CAPS_MONOCHROME;

  if (caps.flags & V4L2_FLAG_PREVIEW)
    {
      info->caps.flags |= TVENG_CAPS_OVERLAY;
      /* Collect more info about the overlay mode */
      if (v4l2_ioctl(info, VIDIOC_G_FBUF, &fb) != 0)
	{
	  if (fb.flags & V4L2_FBUF_CAP_CHROMAKEY)
	    info->caps.flags |= TVENG_CAPS_CHROMAKEY;
	  if (fb.flags & V4L2_FBUF_CAP_CLIPPING)
	    info->caps.flags |= TVENG_CAPS_CLIPPING;
	  if (!(fb.flags & V4L2_FBUF_CAP_EXTERNOVERLAY))
	    info->caps.flags |= TVENG_CAPS_FRAMERAM;
	  if ((fb.flags & V4L2_FBUF_CAP_SCALEUP) ||
	      (fb.flags & V4L2_FBUF_CAP_SCALEDOWN))
	    info->caps.flags |= TVENG_CAPS_SCALES;
	}
    }

  info -> current_controller = TVENG_CONTROLLER_V4L2;
  
  /* Everything seems to be OK with this device */
  return (info -> fd);
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
int tveng2_attach_device(const char* device_file,
			 enum tveng_attach_mode attach_mode,
			 tveng_device_info * info)
{
  int error;
  struct private_tveng2_device_info * p_info =
    (struct private_tveng2_device_info*) info;

  t_assert(device_file != NULL);
  t_assert(info != NULL);

  if (info -> fd) /* If the device is already attached, detach it */
    tveng_close_device(info);

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
      info -> fd = p_tveng2_open_device_file(O_NOIO, info);
      break;
    case TVENG_ATTACH_READ:
      info -> fd = p_tveng2_open_device_file(O_RDWR, info);
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

	// XXX error
	update_video_input_list (info);

	info->video_standards = NULL;
	info->cur_video_standard = NULL;

	// XXX error
	update_current_video_input (info);

  /* Query present controls */
  info->controls = NULL;
  error = p_tveng2_build_controls(info);
  if (error == -1)
      return -1;

  /* Set up the palette according to the one present in the system */
  error = info->priv->current_bpp;

  if (error == -1)
    {
      tveng2_close_device(info);
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
      tveng2_close_device(info);
      return -1;
    }

  /* Get fb_info */
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
  tveng2_set_capture_format(info);

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
tveng2_describe_controller(char ** short_str, char ** long_str,
			   tveng_device_info * info)
{
  t_assert(info != NULL);
  if (short_str)
    *short_str = "V4L2";
  if (long_str)
    *long_str = "Video4Linux 2";
}

/* Closes a device opened with tveng_init_device */
static void tveng2_close_device(tveng_device_info * info)
{
  tveng_stop_everything(info);

  close(info -> fd);
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


static struct tveng_module_info tveng2_module_info = {
  .attach_device =		tveng2_attach_device,
  .describe_controller =	tveng2_describe_controller,
  .close_device =		tveng2_close_device,
  .update_video_input		= update_current_video_input,
  .set_video_input		= set_video_input,
  .update_standard		= update_current_standard,
  .set_standard			= set_standard,
  .update_capture_format =	tveng2_update_capture_format,
  .set_capture_format =		tveng2_set_capture_format,
  .update_control		= update_control,
  .set_control			= set_control,
  .tune_input =			tveng2_tune_input,
  .get_signal_strength =	tveng2_get_signal_strength,
  .get_tune =			tveng2_get_tune,
  .get_tuner_bounds =		tveng2_get_tuner_bounds,
  .start_capturing =		tveng2_start_capturing,
  .stop_capturing =		tveng2_stop_capturing,
  .read_frame =			tveng2_read_frame,
  .get_timestamp =		tveng2_get_timestamp,
  .set_capture_size =		tveng2_set_capture_size,
  .get_capture_size =		tveng2_get_capture_size,
  .get_overlay_buffer		= get_overlay_buffer,
  .set_preview_window =		tveng2_set_preview_window,
  .get_preview_window =		tveng2_get_preview_window,
  .set_overlay			= set_overlay,
  .get_chromakey =		tveng2_get_chromakey,
  .set_chromakey =		tveng2_set_chromakey,

  .private_size =		sizeof(struct private_tveng2_device_info)
};

/*
  Inits the V4L2 module, and fills in the given table.
*/
void tveng2_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

  memcpy(module_info, &tveng2_module_info,
	 sizeof(struct tveng_module_info)); 
}

#else /* !ENABLE_V4L */

#include "tveng2.h"

void tveng2_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

  memset(module_info, 0, sizeof(struct tveng_module_info)); 
}

#endif /* ENABLE_V4L */
