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
  This is the library in charge of simplyfying Video Access API (I
  don't want to use thirteen lines of code with ioctl's every time I
  want to change tuning freq).
  the name is TV Engine, since it is intended mainly for TV viewing.
  This file is separated so zapping doesn't need to know about V4L[2]
*/

#include "zmisc.h"

#ifdef ENABLE_V4L
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#undef WNOHANG
#undef WUNTRACED
#include <errno.h>
#include <math.h>
#include <endian.h>
#include <limits.h>		/* INT_MAX */

#include "common/fifo.h" /* current_time() */

#define MINOR(dev) ((dev) & 0xff)

/* 
   This works around a bug bttv appears to have with the mute
   property. Comment out the line if your V4L driver isn't buggy.
*/
#define TVENG1_BTTV_MUTE_BUG_WORKAROUND 1

#define TVENG1_PROTOTYPES 1
#include "tveng1.h"

/*
	Kernel interface
*/

#include "common/videodev.h"
#include "common/_videodev.h"

/* This macro checks at compile time if the arg type is correct,
   device_ioctl() repeats the ioctl if interrupted (EINTR) and logs
   the args and result if log_fp is non-zero. When the ioctl failed
   ioctl_failure() stores the cmd, caller and errno in info. */
#define xioctl(info, cmd, arg)						\
	(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),				\
	 ((0 == device_ioctl ((info)->log_fp, fprint_v4l_ioctl_arg,	\
			      (info)->fd, cmd, (void *)(arg))) ?	\
	  0 : (ioctl_failure (info, __FILE__, __FUNCTION__,		\
			      __LINE__, # cmd), -1)))

#define xioctl_may_fail(info, cmd, arg)					\
	(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),				\
	 device_ioctl ((info)->log_fp, fprint_v4l_ioctl_arg,		\
		       (info)->fd, cmd, (void *)(arg)))

/* Bttv driver extensions. */

#define BTTV_VERSION _IOR ('v' , BASE_VIDIOCPRIVATE + 6, int)
static __inline__ void IOCTL_ARG_TYPE_CHECK_BTTV_VERSION
	(int *arg __attribute__ ((unused))) {}

static void
fprint_bttv_ioctl_arg		(FILE *			fp,
				 unsigned int		cmd,
				 int			rw _unused_,
				 void *			arg)
{
	switch (cmd) {
	case BTTV_VERSION:
		if (!arg)
			fputs ("BTTV_VERSION", fp);
		break;

	default:
		if (!arg)
			fprint_unknown_ioctl (fp, cmd, arg);
		break;
	}
}

#define bttv_xioctl_may_fail(info, cmd, arg)				\
(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
 device_ioctl ((info)->log_fp, fprint_bttv_ioctl_arg,			\
		      (info)->fd, cmd, (void *)(arg)))

/* PWC driver extensions. */

#include "common/pwc-ioctl.h"
#include "common/_pwc-ioctl.h"

#define pwc_xioctl(info, cmd, arg)					\
(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
 ((0 == device_ioctl ((info)->log_fp, fprint_pwc_ioctl_arg,		\
		      (info)->fd, cmd, (void *)(arg))) ?		\
  0 : (ioctl_failure (info, __FILE__, __PRETTY_FUNCTION__,		\
		      __LINE__, # cmd), -1)))

#define pwc_xioctl_may_fail(info, cmd, arg)				\
(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
 device_ioctl ((info)->log_fp, fprint_pwc_ioctl_arg,			\
		      (info)->fd, cmd, (void *)(arg)))

/* PWC TODO:
   Pan & tilt, real image size (cropping), DNR, flickerless,
   backlight compensation, sharpness, LEDs, whitebalance speed,
   shutter speed, AGC, serial number, compression quality,
   factory reset, user settings, bayer format, decompression.
*/




struct video_input {
	tv_video_line		pub;

	int			channel;	/* struct video_channel */
	int			tuner;		/* struct video_tuner */

	unsigned int		step_shift;
};

#define VI(l) PARENT (l, struct video_input, pub)
#define CVI(l) CONST_PARENT (l, struct video_input, pub)

struct standard {
	tv_video_standard	pub;

	unsigned int		norm;		/* struct video_channel */
};

#define S(l) PARENT (l, struct standard, pub)
#define CS(l) CONST_PARENT (l, struct standard, pub)

/* Private control IDs. In v4l the control concept doesn't exist. */
typedef enum {
	/* Generic video controls. */
	CONTROL_BRIGHTNESS	= (1 << 0),
	CONTROL_CONTRAST	= (1 << 1),
	CONTROL_COLOUR		= (1 << 2),
	CONTROL_HUE		= (1 << 3),

	/* Generic audio controls. */
	CONTROL_MUTE		= (1 << 8),
	CONTROL_VOLUME		= (1 << 9),
	CONTROL_BASS		= (1 << 10),
	CONTROL_TREBLE		= (1 << 11),
	CONTROL_BALANCE		= (1 << 12),

	/* PWC driver. */
	CONTROL_PWC_FPS		= (1 << 16),
	CONTROL_PWC_SNAPSHOT	= (1 << 17),
} control_id;

/* In struct video_pict. */
#define PICT_CONTROLS (CONTROL_BRIGHTNESS |				\
			CONTROL_CONTRAST |				\
			CONTROL_COLOUR |				\
			CONTROL_HUE)

/* In struct video_window. */
#define WINDOW_CONTROLS (CONTROL_PWC_FPS |				\
			 CONTROL_PWC_SNAPSHOT)

/* In struct video_audio. */
#define AUDIO_CONTROLS (CONTROL_MUTE |					\
			CONTROL_VOLUME |				\
			CONTROL_BASS |					\
			CONTROL_TREBLE |				\
			CONTROL_BALANCE)

#define ALL_CONTROLS (PICT_CONTROLS | WINDOW_CONTROLS | AUDIO_CONTROLS)

struct control {
	tv_control		pub;
	control_id		id;
};

#define C(l) PARENT (l, struct control, pub)




/** @internal */
struct xbuffer {
	void *			data;

	struct xbuffer *	next_queued;

	int			frame_number;

	/** Queued with VIDIOCMCAPTURE. */
	tv_bool			queued;

	/** Client got a cb pointer with dequeue_buffer(). */
	tv_bool			dequeued;

	/** Clear this buffer before next VIDIOCMCAPTURE. */
	tv_bool			clear;
};

/*
  If this is enabled, some specific features of the bttv driver are
  enabled, but they are non-standard
*/
#define TVENG1_BTTV_PRESENT 1

struct private_tveng1_device_info
{
  tveng_device_info info; /* Info field, inherited */
#ifdef TVENG1_BTTV_MUTE_BUG_WORKAROUND
#if 0
   int muted; /* 0 if the device is muted, 1 otherwise. A workaround
		for a bttv problem. */
#endif
#endif
  int audio_mode; /* auto, mono, stereo, ... */

  double last_timestamp; /* Timestamp of the last frame captured */

  double capture_time;
  double frame_period_near;
  double frame_period_far;

  uint32_t chroma; /* Pixel value for the chromakey */
  uint32_t r, g, b; /* 0-65535 components for the chroma */

  /* OV511 camera */
  int ogb_fd;

	/* Info about mapped buffers. */
	void *			mapped_addr;
	struct video_mbuf	mbuf;

	struct xbuffer *	buffers;
	unsigned int		n_buffers;

	struct xbuffer *	first_queued;

	tv_bool			streaming;

	control_id		all_controls;

	tv_control *		control_mute;
	tv_control *		control_audio_dec;

	tv_audio_capability	tuner_audio_capability;

	tv_control *		control_pwc_fps;
	tv_control *		control_pwc_snapshot;

	/**
	 * VIDEO_PALETTE_YUYV or VIDEO_PALETTE_YUV422.
	 * These are synonyms, some drivers understand only one.
	 */
	unsigned int		palette_yuyv;

	tv_bool			read_back_controls;
	tv_bool			mute_flag_readable;
	tv_bool			audio_mode_reads_rx;
	tv_bool			channel_norm_usable;

	tv_bool			bttv_driver;
	tv_bool			pwc_driver;
};

#define P_INFO(p) PARENT (p, struct private_tveng1_device_info, info)

static tv_bool
get_capture_format	(tveng_device_info * info);

const struct {
	unsigned int		width;
	unsigned int		height;
} common_sizes [] = {
	{ 128,  96 },
        { 160, 120 },	/* QSIF */
        { 176, 144 },	/* QCIF */
        { 320, 240 },   /* SIF */
        { 352, 240 },	/* NTSC CIF */
        { 352, 288 },	/* CIF */
        { 640, 480 },	/* NTSC square, 4SIF */
	{ 704, 480 },	/* NTSC 4CIF */
	{ 704, 576 },	/* PAL SECAM 4CIF */
	{ 720, 480 },	/* NTSC 601 */
	{ 720, 576 },	/* PAL SECAM 601 */
	{ 768, 576 },   /* PAL SECAM square */
};

static tv_pixfmt
palette_to_pixfmt		(unsigned int		palette)
{
	switch (palette) {
	case VIDEO_PALETTE_GREY:	return TV_PIXFMT_Y8;
	case VIDEO_PALETTE_HI240:	return TV_PIXFMT_UNKNOWN;

#if BYTE_ORDER == BIG_ENDIAN
	case VIDEO_PALETTE_RGB565:	return TV_PIXFMT_BGR16_BE;
	case VIDEO_PALETTE_RGB24:	return TV_PIXFMT_BGR24_BE;
	case VIDEO_PALETTE_RGB32:	return TV_PIXFMT_BGRA32_BE;
	case VIDEO_PALETTE_RGB555:	return TV_PIXFMT_BGRA16_BE;
#else
	case VIDEO_PALETTE_RGB565:	return TV_PIXFMT_BGR16_LE;
	case VIDEO_PALETTE_RGB24:	return TV_PIXFMT_BGR24_LE;
	case VIDEO_PALETTE_RGB32:	return TV_PIXFMT_BGRA32_LE;
	case VIDEO_PALETTE_RGB555:	return TV_PIXFMT_BGRA16_LE;
#endif
	case VIDEO_PALETTE_YUV422:	return TV_PIXFMT_YUYV;
	case VIDEO_PALETTE_YUYV:	return TV_PIXFMT_YUYV;
	case VIDEO_PALETTE_UYVY:	return TV_PIXFMT_UYVY;

	case VIDEO_PALETTE_YUV420:	return TV_PIXFMT_UNKNOWN;
	case VIDEO_PALETTE_YUV411:	return TV_PIXFMT_UNKNOWN;
	case VIDEO_PALETTE_RAW:		return TV_PIXFMT_UNKNOWN;

	case VIDEO_PALETTE_YUV422P:	return TV_PIXFMT_YUV422;
	case VIDEO_PALETTE_YUV411P:	return TV_PIXFMT_YUV411;
	case VIDEO_PALETTE_YUV420P:	return TV_PIXFMT_YUV420;
	case VIDEO_PALETTE_YUV410P:	return TV_PIXFMT_YUV410;

	default:			return TV_PIXFMT_UNKNOWN;
	}
}

static unsigned int
pixfmt_to_palette		(tveng_device_info *	info,
				 tv_pixfmt		pixfmt)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);

	switch (pixfmt) {
	case TV_PIXFMT_Y8:		return VIDEO_PALETTE_GREY;

	case TV_PIXFMT_BGR16_LE:	return VIDEO_PALETTE_RGB565;
	case TV_PIXFMT_BGR24_LE:	return VIDEO_PALETTE_RGB24;
	case TV_PIXFMT_BGRA32_LE:	return VIDEO_PALETTE_RGB32;
	case TV_PIXFMT_BGRA16_LE:	return VIDEO_PALETTE_RGB555;

	case TV_PIXFMT_YUYV:		return p_info->palette_yuyv;
	case TV_PIXFMT_UYVY:		return VIDEO_PALETTE_UYVY;

	case TV_PIXFMT_YUV422:		return VIDEO_PALETTE_YUV422P;
	case TV_PIXFMT_YUV411:		return VIDEO_PALETTE_YUV411P;
	case TV_PIXFMT_YUV420:		return VIDEO_PALETTE_YUV420P;
	case TV_PIXFMT_YUV410:		return VIDEO_PALETTE_YUV410P;

	default:			return 0;
	}
}

/* V4L prohibits multiple opens. In panel mode (no i/o, access to
   controls only) this code is supposed to temporarily open the
   device for the access, or to return an error if the device
   is already in use. */
static tv_bool
panel_close			(tveng_device_info *	info _unused_)
{
	/* to do */

	return FALSE;
}

static tv_bool
panel_open			(tveng_device_info *	info)
{
	t_assert (-1 == info->fd);

	/* to do */

	return FALSE;
}

/*
	Audio matrix
 */

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

static tv_audio_mode
tv_audio_mode_from_v4l_mode	(unsigned int		mode)
{
	switch (mode) {
	case VIDEO_SOUND_MONO:
	case VIDEO_SOUND_LANG1:
		return TV_AUDIO_MODE_LANG1_MONO;

	case VIDEO_SOUND_STEREO:
		return TV_AUDIO_MODE_LANG1_STEREO;

	case VIDEO_SOUND_LANG2:
		return TV_AUDIO_MODE_LANG2_MONO;

	default:
		break;
	}

	return TV_AUDIO_MODE_AUTO;
}

static void
set_audio_capability		(struct private_tveng1_device_info *p_info)
{
	static const tv_audio_capability lang2 =
		TV_AUDIO_CAPABILITY_SAP |
		TV_AUDIO_CAPABILITY_BILINGUAL;
	tv_audio_capability cap;

	cap = TV_AUDIO_CAPABILITY_EMPTY;

	if (p_info->info.panel.cur_video_input
	    && IS_TUNER_LINE (p_info->info.panel.cur_video_input)) {
		cap = p_info->tuner_audio_capability;

		if (cap & lang2) {
			const tv_video_standard *std;

			std = p_info->info.panel.cur_video_standard;

			if (std && (std->videostd_set
				    & TV_VIDEOSTD_SET (TV_VIDEOSTD_NTSC_M))) {
				cap = (cap & ~lang2) | TV_AUDIO_CAPABILITY_SAP;
			} else {
				cap = (cap & ~lang2)
					| TV_AUDIO_CAPABILITY_BILINGUAL;
			}
		}
	}

	if (p_info->info.panel.audio_capability != cap) {
		p_info->info.panel.audio_capability = cap;
		tv_callback_notify (&p_info->info, &p_info->info,
				    p_info->info.panel.audio_callback);
	}
}

static void
set_audio_reception		(struct private_tveng1_device_info *p_info,
				 unsigned int		mode)
{
	unsigned int rec[2];

	rec[0] = 0;
	rec[1] = 0;

	if (mode & VIDEO_SOUND_STEREO)
		rec[0] = 2;
	else if (mode & VIDEO_SOUND_MONO)
		rec[0] = 1;

	if (mode & VIDEO_SOUND_LANG2)
		rec[1] = 1;

	if (p_info->info.panel.audio_reception[0] != rec[0]
	    || p_info->info.panel.audio_reception[1] != rec[1]) {
		p_info->info.panel.audio_reception[0] = rec[0];
		p_info->info.panel.audio_reception[1] = rec[1];

		tv_callback_notify (&p_info->info, &p_info->info,
				    p_info->info.panel.audio_callback);
	}
}

static tv_bool
init_audio			(struct private_tveng1_device_info *p_info)
{
	struct video_audio audio;
	unsigned int capability;
	unsigned int old_mode;
	unsigned int received;
	unsigned int fst_mode;
	unsigned int cur_mode;
	unsigned int mode;

	p_info->info.panel.audio_capability	= TV_AUDIO_CAPABILITY_EMPTY;
	p_info->tuner_audio_capability	= TV_AUDIO_CAPABILITY_EMPTY;
	p_info->info.panel.audio_mode		= TV_AUDIO_MODE_UNKNOWN;
	p_info->info.panel.audio_reception[0]	= 0; /* primary language */ 
	p_info->info.panel.audio_reception[1]	= 0; /* secondary language */ 

	p_info->audio_mode_reads_rx	= FALSE;

	/* According to /linux/Documentation/video4linux/API.html
	   audio.mode is the "mode the audio input is in". The bttv driver
	   returns the received audio on read, a set, not the selected
	   mode and not capabilities. On write a single bit must be
	   set, zero selects autodetection. NB VIDIOCSAUDIO is not w/r. */

	CLEAR (audio);

	if (-1 == xioctl_may_fail (&p_info->info, VIDIOCGAUDIO, &audio)) {
		if (EINVAL == errno)
			return TRUE; /* no audio? */

		ioctl_failure (&p_info->info,
			       __FILE__,
			       __PRETTY_FUNCTION__,
			       __LINE__,
			       "VIDIOCGAUDIO");
		return FALSE;
	}

	old_mode = audio.mode;
	received = audio.mode;

	fst_mode = (unsigned int) -1;
	cur_mode = (unsigned int) -1;

	capability = 0;

	p_info->audio_mode_reads_rx =
		(p_info->bttv_driver || !SINGLE_BIT (audio.mode));

	/* To determine capabilities let's see which modes we can select. */
	for (mode = 1; mode <= (VIDEO_SOUND_LANG2 << 1); mode <<= 1) {
		audio.mode = mode >> 1; /* 0 == automatic */

		audio.flags |= VIDEO_AUDIO_MUTE;

		if (0 == xioctl_may_fail (&p_info->info, VIDIOCSAUDIO, &audio)) {
			capability |= mode;

			if ((unsigned int) -1 == fst_mode)
				fst_mode = audio.mode;

			cur_mode = audio.mode;

			if (!p_info->audio_mode_reads_rx) {
				if (-1 == xioctl (&p_info->info,
						     VIDIOCGAUDIO, &audio))
					return FALSE;

				if (audio.mode != (mode >> 1))
					p_info->audio_mode_reads_rx = TRUE;
			}
		} else {
			if (EINVAL != errno) {
				ioctl_failure (&p_info->info,
					       __FILE__,
					       __PRETTY_FUNCTION__,
					       __LINE__,
					       "VIDIOCSAUDIO");
				return FALSE;
			}
		}
	}

	if (0 == capability || (unsigned int) -1 == fst_mode)
		return TRUE;

	if (p_info->audio_mode_reads_rx) {
		/* Don't know what old mode was,
		   restore to first working one. */
		old_mode = fst_mode;
	} else {
		/* Don't know received audio. */
		received = 0; /* auto */
	}

	if (old_mode != cur_mode) {
		audio.mode = old_mode;

		if (-1 == xioctl (&p_info->info, VIDIOCSAUDIO, &audio))
			return FALSE;
	}

	if (capability & 1)
		p_info->tuner_audio_capability |= TV_AUDIO_CAPABILITY_AUTO;

	capability >>= 1;

	if ((capability & VIDEO_SOUND_MONO)
	    || (capability & VIDEO_SOUND_LANG1))
		p_info->tuner_audio_capability |= TV_AUDIO_CAPABILITY_MONO;

	if (capability & VIDEO_SOUND_STEREO)
		p_info->tuner_audio_capability |= TV_AUDIO_CAPABILITY_STEREO;

	if (capability & VIDEO_SOUND_LANG2) {
		/* We assume both. */
		p_info->tuner_audio_capability |=
			TV_AUDIO_CAPABILITY_SAP |
			TV_AUDIO_CAPABILITY_BILINGUAL;
	}

	p_info->info.panel.audio_mode = tv_audio_mode_from_v4l_mode (cur_mode);

	set_audio_capability (p_info);

	set_audio_reception (p_info, received);

	return TRUE;
}

static tv_bool
set_audio_mode			(tveng_device_info *	info,
				 tv_audio_mode		mode)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_audio audio;

	if (-1 == xioctl (&p_info->info, VIDIOCGAUDIO, &audio))
		return FALSE;

	if (!p_info->mute_flag_readable) {
		if (p_info->control_mute
		    && p_info->control_mute->value)
			audio.flags |= VIDEO_AUDIO_MUTE;
		else
			audio.flags &= ~VIDEO_AUDIO_MUTE;
	}

	audio.mode = tv_audio_mode_to_v4l_mode (mode);

	if (-1 == xioctl (info, VIDIOCSAUDIO, &audio))
		return FALSE;

	if (p_info->info.panel.audio_mode != mode) {
		p_info->info.panel.audio_mode = mode;
		tv_callback_notify (&p_info->info, &p_info->info,
				    p_info->info.panel.audio_callback);
	}

	return TRUE;
}

/*
	Controls
*/

static void
update_control_set		(tveng_device_info *	info,
				 const struct video_picture *pict,
				 const struct video_window *win,
				 const struct video_audio *audio,
				 control_id		control_set)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	tv_control *control;

	for_all (control, p_info->info.panel.controls) {
		struct control *c = C(control);
		int value;

		if (c->pub._parent != info)
			continue;

		if (TV_CONTROL_ID_AUDIO_MODE == c->pub.id)
			continue;

		if (0 == (control_set & c->id))
			continue;

		switch (c->id) {

			/* Picture controls. */

		case CONTROL_BRIGHTNESS:
			value = pict->brightness;
			break;
		case CONTROL_HUE:
			value = pict->hue;
			break;
		case CONTROL_COLOUR:
			value = pict->colour;
			break;
		case CONTROL_CONTRAST:
			value = pict->contrast;
			break;

			/* Window controls. */

		case CONTROL_PWC_FPS:
			value = (win->flags & PWC_FPS_FRMASK) >> PWC_FPS_SHIFT;
			break;

		case CONTROL_PWC_SNAPSHOT:
			value = !!(win->flags & PWC_FPS_SNAPSHOT);
			break;

			/* Audio controls. */

		case CONTROL_MUTE:
			if (!p_info->mute_flag_readable)
				continue;

			value = ((audio->flags & VIDEO_AUDIO_MUTE) != 0);
			break;

		case CONTROL_VOLUME:
			value = audio->volume;
			break;
		case CONTROL_BASS:
			value = audio->bass;
			break;
		case CONTROL_TREBLE:
			value = audio->treble;
			break;
		case CONTROL_BALANCE:
			value = audio->balance;
			break;

		default:
			t_warn ("Invalid c->id 0x%x\n", c->id);
			p_info->info.tveng_errno = -1; /* unknown */
			return;
		}

		if (c->pub.value != value) {
			c->pub.value = value;
			tv_callback_notify (info, &c->pub, c->pub._callback);
		}
	}
}

static tv_bool
get_control			(tveng_device_info *	info,
				 tv_control *		tc)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_picture pict;
	struct video_window window;
	struct video_audio audio;
	control_id control_set;

	if (p_info->control_audio_dec == tc)
		return TRUE; /* XXX */

	if (TVENG_ATTACH_CONTROL == info->attach_mode)
		if (!panel_open (info))
			return FALSE;

	if (tc)
		control_set = C(tc)->id;
	else
		control_set = p_info->all_controls;

	if (control_set & PICT_CONTROLS) {
		if (-1 == xioctl (info, VIDIOCGPICT, &pict))
			goto failure;

		control_set |= PICT_CONTROLS;
	}

	if (control_set & WINDOW_CONTROLS) {
		if (-1 == xioctl (info, VIDIOCGWIN, &window))
			goto failure;

		control_set |= WINDOW_CONTROLS;
	}

	if (control_set & AUDIO_CONTROLS) {
		if (-1 == xioctl (info, VIDIOCGAUDIO, &audio))
			goto failure;

		if (p_info->audio_mode_reads_rx) {
			set_audio_reception (p_info, audio.mode);
		}

		control_set |= AUDIO_CONTROLS;
	}

	update_control_set (info, &pict, &window, &audio, control_set);

	if (TVENG_ATTACH_CONTROL == info->attach_mode)
		return panel_close (info);
	else
		return TRUE;

 failure:
	if (TVENG_ATTACH_CONTROL == info->attach_mode)
		panel_close (info);

	return FALSE;
}

static tv_bool
set_control			(tveng_device_info *	info,
				 tv_control *		tc,
				 int			value)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct control *c = C(tc);

	if (p_info->control_audio_dec == tc)
		return set_audio_mode_control (info, tc, value);

	if (TVENG_ATTACH_CONTROL == info->attach_mode)
		if (!panel_open (info))
			return FALSE;

	if (c->id & PICT_CONTROLS) {
		struct video_picture pict;

		if (-1 == xioctl (info, VIDIOCGPICT, &pict))
			goto failure;

		switch (c->id) {
		case CONTROL_BRIGHTNESS:
			pict.brightness = value;
			break;

		case CONTROL_HUE:
			pict.hue = value;
			break;

		case CONTROL_COLOUR:
			pict.colour = value;
			break;

		case CONTROL_CONTRAST:
			pict.contrast = value;
			break;

		default:
			t_warn ("Invalid c->id 0x%x\n", c->id);
			info->tveng_errno = -1; /* unknown */
			goto failure;
		}

		if (-1 == xioctl (info, VIDIOCSPICT, &pict))
			goto failure;

		if (p_info->read_back_controls) {
			/* Error ignored */
			xioctl (info, VIDIOCGPICT, &pict);
		}

		update_control_set (&p_info->info, &pict, NULL, NULL,
				    PICT_CONTROLS);

	} else if (c->id & WINDOW_CONTROLS) {
		struct video_window window;
		unsigned int new_flags;

		if (-1 == xioctl (info, VIDIOCGWIN, &window))
			goto failure;

		new_flags = window.flags;

		switch (c->id) {
		case CONTROL_PWC_FPS:
			new_flags &= ~PWC_FPS_FRMASK;
			new_flags |= value << PWC_FPS_SHIFT;
			break;

		case CONTROL_PWC_SNAPSHOT:
			if (value)
				new_flags |= PWC_FPS_SNAPSHOT;
			else
				new_flags &= ~PWC_FPS_SNAPSHOT;
			break;

		default:
			t_warn ("Invalid c->id 0x%x\n", c->id);
			info->tveng_errno = -1; /* unknown */
			goto failure;
		}

		if (window.flags != new_flags) {
			window.flags = new_flags;

			if (-1 == xioctl (info, VIDIOCSWIN, &window))
				goto failure;

			if (p_info->read_back_controls) {
				/* Error ignored */
				xioctl (info, VIDIOCGWIN, &window);
			}
		}

		update_control_set (&p_info->info, NULL, &window, NULL,
				    WINDOW_CONTROLS);

	} else if (c->id & AUDIO_CONTROLS) {
		struct video_audio audio;
		unsigned int rx_mode;
		tv_bool no_read;

		if (-1 == xioctl (info, VIDIOCGAUDIO, &audio))
			goto failure;

		no_read = FALSE;
		rx_mode = audio.mode;

		switch (c->id) {
		case CONTROL_VOLUME:
			audio.volume = value;
			break;

		case CONTROL_BASS:
			audio.bass = value;
			break;

		case CONTROL_TREBLE:
			audio.treble = value;
			break;

		case CONTROL_BALANCE:
			audio.balance = value;
			break;

		case CONTROL_MUTE:
			if (value)
				audio.flags |= VIDEO_AUDIO_MUTE;
			else
				audio.flags &= ~VIDEO_AUDIO_MUTE;
			no_read = !p_info->mute_flag_readable;
			break;

		default:
			t_warn ("Invalid c->id 0x%x\n", c->id);
			info->tveng_errno = -1; /* unknown */
			goto failure;
		}

		if (CONTROL_MUTE != c->id
		    && !p_info->mute_flag_readable) {
			if (p_info->control_mute
			    && p_info->control_mute->value)
				audio.flags |= VIDEO_AUDIO_MUTE;
			else
				audio.flags &= ~VIDEO_AUDIO_MUTE;
 		}

		if (p_info->audio_mode_reads_rx) {
			audio.mode = tv_audio_mode_to_v4l_mode
				(p_info->info.panel.audio_mode);
		}

		if (-1 == xioctl (info, VIDIOCSAUDIO, &audio))
			goto failure;

		if (p_info->read_back_controls) {
			/* Error ignored */
			xioctl (info, VIDIOCGAUDIO, &audio);
		}

		if (no_read && c->pub.value != value) {
			c->pub.value = value;
			tv_callback_notify (info, &c->pub, c->pub._callback);
		}

		update_control_set (info, NULL, NULL, &audio,
				    AUDIO_CONTROLS);
	} else {
		t_warn ("Invalid c->id 0x%x\n", c->id);
		info->tveng_errno = -1; /* unknown */
		goto failure;
	}

	if (TVENG_ATTACH_CONTROL == info->attach_mode)
		return panel_close (info);
	else
		return TRUE;

 failure:
	if (TVENG_ATTACH_CONTROL == info->attach_mode)
		panel_close (info);

	return FALSE;
}

static tv_control *
add_control			(tveng_device_info *	info,
				 unsigned int		id,
				 const char *		label,
				 tv_control_id		tcid,
				 tv_control_type	type,
				 int			cur,
				 int			def,
				 int			minimum,
				 int			maximum,
				 int			step)
{
	struct control c;
	tv_control *tc;

	CLEAR (c);

	c.pub.type	= type;
	c.pub.id	= tcid;

	if (!(c.pub.label = strdup (_(label))))
		return NULL;

	c.pub.minimum	= minimum;
	c.pub.maximum	= maximum;
	c.pub.step	= step;
	c.pub.reset	= def;

	c.pub.value	= cur;

	c.id		= id;

	if ((tc = append_control (info, &c.pub, sizeof (c)))) {
		P_INFO (info)->all_controls |= id;
		return tc;
	} else {
		free (c.pub.label);
		return NULL;
	}
}

/* The range 0 ... 65535 is mandated by the v4l spec. We add a reset
   value of 32768 and a step value of 256. V4L does not report the
   actual reset value and hardware resolution but for a UI these
   values should do. */
#define ADD_STD_CONTROL(name, label, id, value)				\
	add_control (info, CONTROL_##name, _(label),			\
		     TV_CONTROL_ID_##id, TV_CONTROL_TYPE_INTEGER,	\
		     value, 32768, 0, 65535, 256)

static tv_bool
get_video_control_list		(tveng_device_info *	info)
{
	struct video_picture pict;

	if (-1 == xioctl (info, VIDIOCGPICT, &pict))
		return FALSE;

	ADD_STD_CONTROL (BRIGHTNESS,"Brightness", BRIGHTNESS, pict.brightness);
	ADD_STD_CONTROL (CONTRAST,  "Contrast",   CONTRAST,   pict.contrast);
	ADD_STD_CONTROL (COLOUR,    "Saturation", SATURATION, pict.colour);
	ADD_STD_CONTROL (HUE,       "Hue",        HUE,        pict.hue);

	return TRUE;
}

static tv_bool
get_audio_control_list		(tveng_device_info *	info)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_audio audio;
	tv_bool rewrite;

	if (-1 == xioctl_may_fail (info, VIDIOCGAUDIO, &audio)) {
		if (EINVAL == errno)
			return TRUE; /* no audio support */

		ioctl_failure (info,
			       __FILE__,
			       __PRETTY_FUNCTION__,
			       __LINE__,
			       "VIDIOCGAUDIO");
		return FALSE;
	}

	rewrite = FALSE;

	if (audio.flags & VIDEO_AUDIO_MUTABLE) {
		if (!p_info->mute_flag_readable) {
			audio.flags |= VIDEO_AUDIO_MUTE;
			rewrite = TRUE;
		}

		p_info->control_mute =
			add_control (info, CONTROL_MUTE, _("Mute"),
				     TV_CONTROL_ID_MUTE,
				     TV_CONTROL_TYPE_BOOLEAN,
				     (audio.flags & VIDEO_AUDIO_MUTE) != 0,
				     /* default */ 0,
				     /* minimum */ 0,
				     /* maximum */ 1,
				     /* step */ 1);
	}

	/* XXX drivers may not support them all, should probe. */

	if (audio.flags & VIDEO_AUDIO_VOLUME)
		ADD_STD_CONTROL (VOLUME, "Volume", VOLUME, audio.volume);

	if (audio.flags & VIDEO_AUDIO_BASS)
		ADD_STD_CONTROL (BASS, "Bass", BASS, audio.bass);

	if (audio.flags & VIDEO_AUDIO_TREBLE)
		ADD_STD_CONTROL (TREBLE, "Treble", TREBLE, audio.treble);

#ifdef VIDEO_AUDIO_BALANCE
	if (audio.flags & VIDEO_AUDIO_BALANCE)
		ADD_STD_CONTROL (BALANCE, "Balance", UNKNOWN, audio.balance);
#endif

	p_info->control_audio_dec = NULL;
	if (0 != p_info->tuner_audio_capability
	    && !SINGLE_BIT (p_info->tuner_audio_capability))
		p_info->control_audio_dec =
			append_audio_mode_control
			(info, p_info->tuner_audio_capability);

	if (rewrite) {
		/* Can't read values, we must write to
		   synchronize. Error ignored. */
		xioctl (info, VIDIOCSAUDIO, &audio);
	}

	return TRUE;
}

static tv_bool
get_pwc_control_list		(tveng_device_info *	info)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_window window;

	if (-1 == xioctl (info, VIDIOCGWIN, &window))
		return FALSE;

	p_info->control_pwc_fps =
		add_control (info, CONTROL_PWC_FPS, _("Frame Rate"),
			     TV_CONTROL_ID_UNKNOWN,
			     TV_CONTROL_TYPE_INTEGER,
			     (window.flags & PWC_FPS_FRMASK) >> PWC_FPS_SHIFT,
			     /* default */ 10,
			     /* minimum */ 5,
			     /* maximum */ 30,
			     /* step */ 5);

	p_info->control_pwc_snapshot =
		/* TRANSLATORS: PWC driver control.  In 'Snapshot' mode the
		   camera freezes its automatic exposure and colour balance
		   controls. */
		add_control (info, CONTROL_PWC_SNAPSHOT, _("Snapshot Mode"),
			     TV_CONTROL_ID_UNKNOWN,
			     TV_CONTROL_TYPE_BOOLEAN,
			     !!(window.flags & PWC_FPS_SNAPSHOT),
			     /* default */ 0,
			     /* minimum */ 0,
			     /* maximum */ 1,
			     /* step */ 1);

	return TRUE;
}

static tv_bool
get_control_list		(tveng_device_info *	info)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);

	free_controls (info);

	p_info->all_controls = 0;

	if (!get_video_control_list (info))
		return FALSE;

	if (!get_audio_control_list (info))
		return FALSE;

	if (p_info->pwc_driver) {
		if (!get_pwc_control_list (info))
			return FALSE;
	}

	return TRUE;
}

/*
 *  Video standards
 */

/* Test whether struct video_channel.norm can be used to get and set
   the current video standard. */
static tv_bool
channel_norm_test		(tveng_device_info *	info)
{
	struct video_channel channel;
	tv_video_line *l;
	unsigned int old_norm;
	unsigned int new_norm;

	if (P_INFO (info)->bttv_driver)
		return TRUE;

	for_all (l, info->panel.video_inputs)
		if (l->type == TV_VIDEO_LINE_TYPE_BASEBAND)
			break;
	if (!l)
		return FALSE;

	CLEAR (channel);

	channel.channel = VI (l)->channel;

	if (-1 == xioctl (info, VIDIOCGCHAN, &channel))
		return FALSE;

	old_norm = channel.norm;
	new_norm = (old_norm == 0); /* PAL -> NTSC, NTSC -> PAL */

	channel.norm = new_norm;

	if (-1 == xioctl (info, VIDIOCSCHAN, &channel))
		return FALSE;

	if (channel.norm != new_norm)
		return FALSE;

	channel.norm = old_norm;

	return (0 == xioctl (info, VIDIOCSCHAN, &channel));
}

static tv_bool
get_video_standard		(tveng_device_info *	info)
{
	tv_video_standard *s;
	unsigned int norm;

	if (!info->panel.video_standards) {
		store_cur_video_standard (info, NULL /* unknown */);
		set_audio_capability (P_INFO (info));
		return TRUE;
	}

	if (TVENG_ATTACH_CONTROL == info->attach_mode)
		if (!panel_open (info))
			return FALSE;

	if (info->panel.cur_video_input
	    && P_INFO (info)->channel_norm_usable) {
		struct video_channel channel;

		CLEAR (channel);

		channel.channel = VI(info->panel.cur_video_input)->channel;

		if (-1 == xioctl (info, VIDIOCGCHAN, &channel))
			goto failure;

		norm = channel.norm;
	} else if (IS_TUNER_LINE (info->panel.cur_video_input)) {
		struct video_tuner tuner;

		CLEAR (tuner);

		tuner.tuner = VI (info->panel.cur_video_input)->tuner;

		if (-1 == xioctl (info, VIDIOCGTUNER, &tuner))
			goto failure;

		norm = tuner.mode;
	} else {
		struct video_tuner tuner;
		tv_video_line *l;

		/* Apparently some V4L drivers (still?) do not report
		   the video standard used by the current video input
		   unless it has a tuner. We query the first video
		   input with tuner, if any. */

		for_all (l, info->panel.video_inputs)
			if (l->type == TV_VIDEO_LINE_TYPE_TUNER)
				break;

		if (!l) {
			s = NULL; /* unknown */
			goto store;
		}

		CLEAR (tuner);

		tuner.tuner = VI (l)->tuner;

		if (-1 == xioctl (info, VIDIOCGTUNER, &tuner))
			goto failure;

		norm = tuner.mode;
	}

	for_all (s, info->panel.video_standards)
		if (S(s)->norm == norm)
			break;

 store:
	store_cur_video_standard (info, s);
	set_audio_capability (P_INFO (info));

	if (TVENG_ATTACH_CONTROL == info->attach_mode)
		return panel_close (info);
	else
		return TRUE;

 failure:
	if (TVENG_ATTACH_CONTROL == info->attach_mode)
		panel_close (info);

	return FALSE;
}

static tv_bool
set_video_standard		(tveng_device_info *	info,
				 tv_video_standard *	s)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	unsigned int norm;
	int r;

	if (TVENG_ATTACH_CONTROL == info->attach_mode) {
		if (!panel_open (info))
			return FALSE;
	} else if (p_info->streaming) {
		return FALSE;
	}

	norm = CS(s)->norm;

	if (info->panel.cur_video_input
	    && p_info->channel_norm_usable) {
		struct video_channel channel;

		CLEAR (channel);

		channel.channel = VI (info->panel.cur_video_input)->channel;

		if (-1 == xioctl (info, VIDIOCGCHAN, &channel))
			goto failure;

		if (channel.norm == norm)
			goto success;

		channel.norm = norm;

		if (0 == (r = xioctl (info, VIDIOCSCHAN, &channel))) {
			store_cur_video_standard (info, s);
			set_audio_capability (P_INFO (info));
		}
	} else if (IS_TUNER_LINE (info->panel.cur_video_input)) {
		struct video_tuner tuner;

		CLEAR (tuner);

		tuner.tuner = VI (info->panel.cur_video_input)->tuner;

		if (-1 == xioctl (info, VIDIOCGTUNER, &tuner))
			goto failure;

		if (tuner.mode == norm)
			goto success;

		if (!(tuner.flags & VIDEO_TUNER_NORM)) {
			errno = -1; /* FIXME */
			goto failure; /* not setable */
		}

		tuner.mode = norm;

		if (0 == (r = xioctl (info, VIDIOCSTUNER, &tuner))) {
			store_cur_video_standard (info, s);
			set_audio_capability (P_INFO (info));
		}
	} else {
		struct video_channel channel;
		struct video_tuner tuner;
		tv_video_line *l;
		tv_bool switched;

		/* Switch to an input with tuner,
		   change video standard and then switch back. */

		for_all (l, info->panel.video_inputs)
			if (l->type == TV_VIDEO_LINE_TYPE_TUNER)
				break;

		if (!l) {
			errno = -1; /* FIXME */
			goto failure;
		}

		CLEAR (channel);

		channel.channel = VI (l)->channel;

		if (-1 == xioctl (info, VIDIOCGCHAN, &channel))
			goto failure;

		if (channel.norm == norm)
			goto success;

		switched = FALSE;

		if (-1 == (r = xioctl (info, VIDIOCSCHAN, &channel)))
			goto finish;

		CLEAR (tuner);

		tuner.tuner = VI (l)->tuner;

		if (-1 == (r = xioctl (info, VIDIOCGTUNER, &tuner)))
			goto restore;

		if (!(tuner.flags & VIDEO_TUNER_NORM)) {
			r = -1;
			goto restore;
		}

		tuner.mode = norm;

		if (0 == (r = xioctl (info, VIDIOCSTUNER, &tuner)))
			switched = TRUE;

	restore:
		channel.channel = VI (info->panel.cur_video_input)->channel;

		if (-1 == (r = xioctl (info, VIDIOCSCHAN, &channel))) {
			/* Notify about accidental video input change. */
			store_cur_video_input (info, l);
		}

		if (switched) {
			if (get_video_standard (info)) {
				store_cur_video_standard (info, s);
				set_audio_capability (P_INFO (info));
			} else {
				r = -1;
			}
		}
	}

 finish:
	if (r == 0) {
 success:
		if (TVENG_ATTACH_CONTROL == info->attach_mode)
			return panel_close (info);
		else
			return TRUE;
	} else {
 failure:
		if (TVENG_ATTACH_CONTROL == info->attach_mode)
			panel_close (info);

		return FALSE;
	}
}

struct standard_map {
	const char *		label;
	tv_videostd_set		set;
};

/* Standards defined by V4L, VIDEO_MODE_ order. */
static const struct standard_map
v4l_standards [] = {
	/* We don't really know what exactly these videostandards are,
	   it depends on the hardware and driver configuration. */
	{ "PAL",	TV_VIDEOSTD_SET_PAL },
	{ "NTSC",	TV_VIDEOSTD_SET_NTSC },
	{ "SECAM",	TV_VIDEOSTD_SET_SECAM },
#if 0
	{ "AUTO",	TV_VIDEOSTD_SET_UNKNOWN },
#endif
	{ NULL,		0 }
};

/* Standards defined by bttv driver. */
static const struct standard_map
bttv_standards [] = {
	{ "PAL",	TV_VIDEOSTD_SET_PAL },
	{ "NTSC",	TV_VIDEOSTD_SET (TV_VIDEOSTD_NTSC_M) },
	{ "SECAM",	TV_VIDEOSTD_SET_SECAM },
	{ "PAL-NC",	TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_NC) },
	{ "PAL-M",	TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_M) },
	{ "PAL-N",	TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_N) },
	{ "NTSC-JP",	TV_VIDEOSTD_SET (TV_VIDEOSTD_NTSC_M_JP) },
	{ NULL,		0 }
};

static tv_bool
get_video_standard_list		(tveng_device_info *	info)
{
	const struct standard_map *table;
	unsigned int flags;
	unsigned int i;

	free_video_standards (info);

	if (!info->panel.cur_video_input)
		return TRUE;

	if (P_INFO (info)->bttv_driver) {
		if (info->panel.video_standards)
			return TRUE; /* invariable */

		table = bttv_standards;
	} else {
		table = v4l_standards;
	}

	if (IS_TUNER_LINE (info->panel.cur_video_input)) {
		struct video_tuner tuner;

		CLEAR (tuner);

		tuner.tuner = VI (info->panel.cur_video_input)->tuner;

		if (-1 == xioctl (info, VIDIOCGTUNER, &tuner))
			return FALSE;

		flags = tuner.flags;
	} else {
		/* Supported video standards of baseband inputs are
		   not reported. */

		flags = (unsigned int) ~0;
	}

	for (i = 0; table[i].label; ++i) {
		struct standard *s;

		if (!(flags & (1 << i)))
			continue; /* unsupported standard */

		if (!(s = S(append_video_standard (&info->panel.video_standards,
						   table[i].set,
						   table[i].label,
						   table[i].label,
						   sizeof (*s)))))
			goto failure;

		s->norm = i;
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
	unsigned long freq;

	/* Err. cur_video_input may not be up-to-date, but there
	   is no ioctl to verify. */
	if (info->panel.cur_video_input == l) {
		int r;

		if (TVENG_ATTACH_CONTROL == info->attach_mode) {
			if (!panel_open (info))
				return FALSE;

			r = xioctl (info, VIDIOCGFREQ, &freq);

			if (!panel_close (info))
				return FALSE;
		} else {
			r = xioctl (info, VIDIOCGFREQ, &freq);
		}

		if (-1 == r)
			return FALSE;

		store_frequency (info, VI (l), freq);
	}

	return TRUE;
}

static tv_bool
set_tuner_frequency		(tveng_device_info *	info,
				 tv_video_line *	l,
				 unsigned int		frequency)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_input *vi = VI (l);
	unsigned long old_freq;
	unsigned long new_freq;

	new_freq = (frequency << vi->step_shift) / vi->pub.u.tuner.step;

	/* Err. cur_video_input may not be up-to-date, but there
	   is no ioctl to verify. */
	if (info->panel.cur_video_input != l) {
		store_frequency (info, vi, new_freq);
		return TRUE;
	}

	if (TVENG_ATTACH_CONTROL == info->attach_mode)
		if (!panel_open (info))
			return FALSE;

	if (0 == xioctl (info, VIDIOCGFREQ, &old_freq))
		if (old_freq == new_freq)
			goto store;

	if (-1 == xioctl (info, VIDIOCSFREQ, &new_freq)) {
		store_frequency (info, vi, old_freq);

		if (TVENG_ATTACH_CONTROL == info->attach_mode)
			panel_close (info);

		return FALSE;
	}

	/* Bttv mutes the input if the signal strength is too
	   low, we don't want that. However usually the quiet
	   switch will be set anyway. */
	if (p_info->control_mute
	    && !info->quiet)
	  set_control (info, p_info->control_mute,
		       p_info->control_mute->value);

	if (CAPTURE_MODE_READ == info->capture_mode) {
		unsigned int i;

		/* XXX do it like v4l2 */
		for (i = 0; i < p_info->n_buffers; ++i) {
			tv_clear_image (p_info->buffers[i].data,
					&info->capture.format);
		}
	}

 store:
	store_frequency (info, vi, new_freq);

	if (TVENG_ATTACH_CONTROL == info->attach_mode)
		return panel_close (info);
	else
		return TRUE;
}

static tv_bool
get_signal_strength		(tveng_device_info *	info,
				 int *			strength,
				 int *			afc _unused_)
{
	if (strength) {
		struct video_tuner tuner;

		CLEAR (tuner);
		tuner.tuner = 0; /* XXX correct? */

		if (-1 == xioctl (info, VIDIOCGTUNER, &tuner))
			return FALSE;

		*strength = tuner.signal;
	}

	return TRUE; /* Success */
}

static tv_bool
set_video_input			(tveng_device_info *	info,
				 tv_video_line *	l)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_channel channel;

	if (TVENG_ATTACH_CONTROL == info->attach_mode) {
		if (!panel_open (info))
			return FALSE;
	} else if (p_info->streaming) {
		return FALSE;
	}

	CLEAR (channel);

	channel.channel = CVI (l)->channel;

	if (-1 == xioctl (info, VIDIOCGCHAN, &channel))
		goto failure;

	/* There is no ioctl to query the current video input,
	   so unfortunately we cannot take a shortcut. */

	if (-1 == xioctl (info, VIDIOCSCHAN, &channel))
		goto failure;

	store_cur_video_input (info, l);

	/* Implies get_video_standard() and set_audio_capability(). */
	get_video_standard_list (info);

	/* V4L does not promise per-tuner frequency setting as we do.
	   XXX in panel mode ignores the possibility that a third
	   party changed the frequency from the value we know. */
	if (IS_TUNER_LINE (l)) {
		set_tuner_frequency (info, info->panel.cur_video_input,
				     info->panel.cur_video_input->u.tuner.frequency);
	}

	if (TVENG_ATTACH_CONTROL == info->attach_mode)
		return panel_close (info);
	else
		return TRUE;

 failure:
	if (TVENG_ATTACH_CONTROL == info->attach_mode)
		panel_close (info);

	return FALSE;
}

static tv_bool
tuner_bounds			(tveng_device_info *	info,
				 struct video_input *	vi)
{
	struct video_tuner tuner;
	unsigned long freq;

	tuner.tuner = vi->tuner;

	if (-1 == xioctl (info, VIDIOCGTUNER, &tuner))
		return FALSE;

	t_assert (tuner.rangelow <= tuner.rangehigh);

	if (tuner.flags & VIDEO_TUNER_LOW) {
		/* Actually step is 62.5 Hz, but why
		   unnecessarily complicate things. */
		vi->pub.u.tuner.step = 125;
		vi->step_shift = 1;

		tuner.rangelow = MIN ((unsigned int) tuner.rangelow,
				      UINT_MAX / 125);
		tuner.rangehigh = MIN ((unsigned int) tuner.rangehigh,
				       UINT_MAX / 125);
	} else {
		vi->pub.u.tuner.step = 62500;
		vi->step_shift = 0;

		tuner.rangelow = MIN ((unsigned int) tuner.rangelow,
				      UINT_MAX / 62500);
		tuner.rangehigh = MIN ((unsigned int) tuner.rangehigh,
				       UINT_MAX / 62500);
	}

	vi->pub.u.tuner.minimum = SCALE_FREQUENCY (vi, tuner.rangelow);
	vi->pub.u.tuner.maximum = SCALE_FREQUENCY (vi, tuner.rangehigh);

	if (-1 == xioctl (info, VIDIOCGFREQ, &freq))
		return FALSE;

	store_frequency (info, vi, freq);

	return TRUE;
}

static tv_bool
get_video_input_list		(tveng_device_info *	info)
{
	struct video_channel channel;
	unsigned int i;

	free_video_inputs (info);

	for (i = 0; i < (unsigned int) info->caps.channels; ++i) {
		struct video_input *vi;
		char buf[sizeof (channel.name) + 1];
		tv_video_line_type type;

		CLEAR (channel);

		channel.channel = i;

		if (-1 == xioctl (info, VIDIOCGCHAN, &channel))
			continue;

		switch (channel.type) {
		case VIDEO_TYPE_TV:
			type = TV_VIDEO_LINE_TYPE_TUNER;
			break;

		case VIDEO_TYPE_CAMERA:
			type = TV_VIDEO_LINE_TYPE_BASEBAND;
			break;

		default: /* ? */
			continue;
		}

		/* Sometimes NUL is missing. */
		z_strlcpy (buf, channel.name, sizeof (buf));

		if (!(vi = VI (append_video_line
			       (&info->panel.video_inputs,
				type, buf, buf, sizeof (*vi)))))
			goto failure;

		vi->pub._parent = info;

		vi->channel = i;

		/* FIXME allocate one video_line for each tuner. */
		vi->tuner = 0;

		if (channel.type == VIDEO_TYPE_TV) {
			if (-1 == xioctl (info, VIDIOCSCHAN, &channel))
				return FALSE;

			if (!tuner_bounds (info, vi))
				goto failure;
		}
	}

	if (info->panel.video_inputs) {
		/* There is no ioctl to query the current video input,
		   we can only reset to a known channel. */
		if (!set_video_input (info, info->panel.video_inputs))
			goto failure;
	}

	return TRUE;

 failure:
	free_video_line_list (&info->panel.video_inputs);
	return FALSE;
}


/* Struct video_picture and video_window determine parameters
   for capturing and overlay. */
static tv_bool
get_capture_and_overlay_parameters
				(tveng_device_info * info)
{
	struct video_picture pict;
	struct video_window window;

	CLEAR (pict);

	if (-1 == xioctl (info, VIDIOCGPICT, &pict))
		return FALSE;

	CLEAR (window);

	if (-1 == xioctl (info, VIDIOCGWIN, &window))
		return FALSE;

	/* Current capture format. */

	if (!tv_image_format_init (&info->capture.format,
				   window.width,
				   window.height,
				   /* bytes_per_line: minimum */ 0,
				   palette_to_pixfmt (pict.palette),
				   TV_COLSPC_UNKNOWN)) {
		info->tveng_errno = -1; /* unknown */
		tv_error_msg(info, "Cannot understand the palette");
		return FALSE;
	}

	/* Current overlay window. */

	info->overlay.window.x = window.x;
	info->overlay.window.y = window.y;
	info->overlay.window.width = window.width;
	info->overlay.window.height = window.height;

	update_control_set (info, &pict, &window, NULL,
			    PICT_CONTROLS | WINDOW_CONTROLS);

	return TRUE;
}





/*
 *  Overlay
 */

static tv_bool
get_overlay_buffer		(tveng_device_info *	info)
{
	struct video_buffer buffer;
  
	if (!(info->caps.flags & TVENG_CAPS_OVERLAY))
		goto failure;

	if (-1 == xioctl (info, VIDIOCGFBUF, &buffer))
		goto failure;

	info->overlay.buffer.base = (unsigned long) buffer.base;

	if (!tv_image_format_init (&info->overlay.buffer.format,
				   (unsigned int) buffer.width,
				   (unsigned int) buffer.height,
				   0,
				   pig_depth_to_pixfmt ((unsigned) buffer.depth),
				   TV_COLSPC_UNKNOWN))
		goto failure;

	assert ((unsigned) buffer.bytesperline
		>= info->overlay.buffer.format.bytes_per_line[0]);

	if ((unsigned) buffer.bytesperline
	    > info->overlay.buffer.format.bytes_per_line[0]) {
		assert (TV_PIXFMT_IS_PACKED
			(info->overlay.buffer.format.pixel_format->pixfmt));

		info->overlay.buffer.format.bytes_per_line[0] =
		  buffer.bytesperline;
		info->overlay.buffer.format.size =
		  buffer.bytesperline * buffer.height;
	}

	return TRUE;

 failure:
	CLEAR (info->overlay.buffer);

	return FALSE;
}



/*
  According to the V4L spec we should return a host order RGB32
  value. Using the pixel value directly would make much more sense,
  not to mention "host order RGB32" doesn't mean anything till you
  define what RGB32 means :-)
  Hope this works, i have no way of testing apart from feedback.
*/
static uint32_t calc_chroma (tveng_device_info * info)
{
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;
  uint32_t r, g, b, pixel;

  r = p_info->r;
  g = p_info->g;
  b = p_info->b;

#if __BYTE_ORDER == __LITTLE_ENDIAN
  /* ARGB */
  pixel = (r<<16) + (g<<8) + b;
#elif __BYTE_ORDER == __BIG_ENDIAN
  /* ABGR or BGRA ??? Try with BGRA */
  pixel = (b<<24) + (g<<16) + (r<<8);
#else /* pdp endian */
  /* GBAR */
  pixel = (g<<24) + (b<<16) + r;
#endif

  return pixel;
}

/*
  Sets the preview window dimensions to the given window.
  Success doesn't mean that the requested dimensions are used, maybe
  they are different, check the returned fields to see if they are suitable
  info   : Device we are controlling
*/
static tv_bool
set_overlay_window_clipvec	(tveng_device_info *	info,
				 const tv_window *	w,
				 const tv_clip_vector *	v)
{
	struct video_window window;
	struct video_clip *clips;

	if (v->size > 0) {
		struct video_clip *vc;
		const tv_clip *tc;
		unsigned int i;

		clips = malloc (v->size * sizeof (*clips));
		if (!clips) {
			info->tveng_errno = errno;
			t_error("malloc", info);
			return FALSE;
		}

		vc = clips;
		tc = v->vector;

		for (i = 0; i < v->size; ++i) {
			vc->x		= tc->x1;
			vc->y		= tc->y1;
			vc->width	= tc->x2 - tc->x1;
			vc->height	= tc->y2 - tc->y1;
			++vc;
			++tc;
		}
	} else {
		clips = NULL;
	}

	CLEAR (window);

	window.x		= w->x - info->overlay.buffer.x;
	window.y		= w->y - info->overlay.buffer.y;

	window.width		= w->width;
	window.height		= w->height;

	window.clips		= clips;
	window.clipcount	= v->size;

	window.chromakey	= calc_chroma (info); // XXX check this

	/* Up to the caller to call _on */
	p_tv_enable_overlay (info, FALSE);

	if (-1 == xioctl (info, VIDIOCSWIN, &window)) {
		free (clips);
		return FALSE;
	}

	free (clips);

	if (-1 == xioctl (info, VIDIOCGWIN, &window))
		return FALSE;

	info->overlay.window.x		= window.x;
	info->overlay.window.y		= window.y;
	info->overlay.window.width	= window.width;
	info->overlay.window.height	= window.height;

	/* Clips cannot be read back, we assume no change. */

	return TRUE;
}

static tv_bool
get_overlay_window		(tveng_device_info *	info)
{
  return get_capture_and_overlay_parameters (info);
}

static tv_bool
enable_overlay			(tveng_device_info *	info,
				 tv_bool		on)
{
	int value = on;

	if (0 == xioctl (info, VIDIOCCAPTURE, &value)) {
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
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;
  XColor color;
  Display *dpy = info->display;
  tv_clip_vector vec;

  color.pixel = chromakey;
  XQueryColor (dpy, DefaultColormap(dpy, DefaultScreen(dpy)),
	       &color);

  p_info->chroma = chromakey;
  p_info->r = color.red>>8;
  p_info->g = color.green>>8;
  p_info->b = color.blue>>8;

  CLEAR (vec);

  return set_overlay_window_clipvec (info, window, &vec);
}

static tv_bool
get_overlay_chromakey		(tveng_device_info *info)
{
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;

  /* We aren't returning the chromakey currently used by the driver,
     but the one previously set. The reason for this is that it is
     unclear whether calc_chroma works correctly or not, and that
     color precision could be lost during the V4L->X conversion. In
     other words, this is prolly good enough. */
  info->overlay.chromakey = p_info->chroma;

  return TRUE;
}

static int
tveng1_ioctl			(tveng_device_info *	info,
				 unsigned int		cmd,
				 char *			arg)
{
	return device_ioctl (info->log_fp, fprint_v4l_ioctl_arg,
			     info->fd, cmd, arg);
}











static tv_bool
get_capture_format		(tveng_device_info * info)
{
	return get_capture_and_overlay_parameters (info);
}

static tv_bool
set_capture_format		(tveng_device_info *	info,
				 const tv_image_format *fmt)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_picture pict;
	struct video_window window;
	int r;

	CLEAR (pict);

	if (-1 == xioctl (info, VIDIOCGPICT, &pict))
		return FALSE;

	pict.palette = pixfmt_to_palette (info, fmt->pixel_format->pixfmt);

	if (0 == pict.palette) {
		info->tveng_errno = EINVAL;
		tv_error_msg (info, "%s not supported",
			      fmt->pixel_format->name);
		return FALSE;
	}

	/* Set this values for the picture properties */
	r = xioctl (info, VIDIOCSPICT, &pict);
	if (-1 == r)
		return FALSE;

	info->capture.format.width = (fmt->width + 3) & (unsigned int) -4;
	info->capture.format.height = (fmt->height + 3) & (unsigned int) -4;

	info->capture.format.width = SATURATE (info->capture.format.width,
					       info->caps.minwidth,
					       info->caps.maxwidth);
	info->capture.format.height = SATURATE (info->capture.format.height,
						info->caps.minheight,
						info->caps.maxheight);

	CLEAR (window);

	window.width = info->capture.format.width;
	window.height = info->capture.format.height;

	window.clips = NULL;
	window.clipcount = 0;

	if (p_info->control_pwc_fps) {
		unsigned int value;

		value = p_info->control_pwc_fps->value;
		window.flags |= value << PWC_FPS_SHIFT;
	}

	if (p_info->control_pwc_snapshot) {
		if (p_info->control_pwc_snapshot->value)
			window.flags |= PWC_FPS_SNAPSHOT;
	}

	if (-1 == xioctl_may_fail (info, VIDIOCSWIN, &window)) {
		unsigned int size;
		int smaller;
		int larger;

		if (EINVAL != errno) {
			ioctl_failure (info,
				       __FILE__,
				       __PRETTY_FUNCTION__,
				       __LINE__,
				       "VIDIOCSWIN");
			return FALSE;
		}

		/* May fail due to unsupported capture size (e.g. pwc driver).
		   Let's try some common values, closest first. */

		size = window.width * window.height;

		for (smaller = -1; smaller < (int) N_ELEMENTS (common_sizes);
		     ++smaller) {
			if (size <= (common_sizes[smaller + 1].width
				     * common_sizes[smaller + 1].height))
				break;
		}

		for (larger = N_ELEMENTS (common_sizes);
		     larger >= 0; --larger) {
			if (size >= (common_sizes[larger - 1].width
				     * common_sizes[larger - 1].height))
				break;
		}

		for (;;) {
			if (smaller >= 0) {
				window.width = common_sizes[smaller].width;
				window.height = common_sizes[smaller].height;

				if (0 == xioctl_may_fail (info, VIDIOCSWIN,
							  &window))
					break;

				--smaller;
			} else if (larger >= (int) N_ELEMENTS (common_sizes)) {
				ioctl_failure (info,
					       __FILE__,
					       __PRETTY_FUNCTION__,
					       __LINE__,
					       "VIDIOCSWIN");
				return FALSE;
			}

			if (larger < (int) N_ELEMENTS (common_sizes)) {
				window.width = common_sizes[larger].width;
				window.height = common_sizes[larger].height;

				if (0 == xioctl_may_fail (info, VIDIOCSWIN,
							  &window))
					break;

				++larger;
			}
 		}
	}

	/* Actual image size. */

	if (!get_capture_format (info))
		return FALSE;

	return TRUE;
}

static tv_pixfmt_set
get_supported_pixfmt_set	(tveng_device_info *	info)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_picture pict;
	tv_pixfmt_set pixfmt_set;
	tv_pixfmt pixfmt;

	p_info->palette_yuyv = VIDEO_PALETTE_YUYV;

	CLEAR (pict);

	if (-1 == xioctl (info, VIDIOCGPICT, &pict))
		return TV_PIXFMT_SET_EMPTY;

	pixfmt_set = TV_PIXFMT_SET_EMPTY;

	for (pixfmt = 0; pixfmt < TV_MAX_PIXFMTS; ++pixfmt) {
		pict.palette = pixfmt_to_palette (info, pixfmt);
		if (0 == pict.palette)
			continue;

		if (0 == xioctl_may_fail (info, VIDIOCSPICT, &pict)) {
			pixfmt_set |= TV_PIXFMT_SET (pixfmt);
		} else if (VIDEO_PALETTE_YUYV == pict.palette) {
			/* These are synonyms, some drivers
			   understand only one. */
			pict.palette = VIDEO_PALETTE_YUV422;

			if (0 == xioctl_may_fail (info, VIDIOCSPICT, &pict)) {
				p_info->palette_yuyv = VIDEO_PALETTE_YUV422;
				pixfmt_set |= TV_PIXFMT_SET (pixfmt);
			}
		}
	}

	return pixfmt_set;
}

/*
 *  From rte/mp1e since it now needs much more stable
 *  time stamps than v4l/gettimeofday can provide. 
 */
static inline double
p_tveng1_timestamp(struct private_tveng1_device_info *p_info)
{
  double now = zf_current_time();
  double stamp;

  if (p_info->capture_time > 0) {
    double dt = now - p_info->capture_time;
    double ddt = p_info->frame_period_far - dt;

    if (fabs(p_info->frame_period_near)
	< p_info->frame_period_far * 1.5) {
      p_info->frame_period_near =
	(p_info->frame_period_near - dt) * 0.8 + dt;
      p_info->frame_period_far = ddt * 0.9999 + dt;
      stamp = p_info->capture_time += p_info->frame_period_far;
    } else {
      /* Frame dropping detected & confirmed */
      p_info->frame_period_near = p_info->frame_period_far;
      stamp = p_info->capture_time = now;
    }
  } else {
    /* First iteration */
    stamp = p_info->capture_time = now;
  }

  return stamp;
}

static void
p_tveng1_timestamp_init(tveng_device_info *info)
{
  struct private_tveng1_device_info *p_info =
    (struct private_tveng1_device_info *) info;
  double rate = info->panel.cur_video_standard ?
	  info->panel.cur_video_standard->frame_rate : 25; /* XXX*/

  p_info->capture_time = 0.0;
  p_info->frame_period_near = p_info->frame_period_far = 1.0 / rate;
}

static sig_atomic_t timeout_alarm;

static void
alarm_handler			(int			signum _unused_)
{
	timeout_alarm = TRUE;
}

static int
dequeue_xbuffer			(tveng_device_info *	info,
				 struct xbuffer **	buffer,
				 const struct timeval *	timeout)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct xbuffer *b;
	int frame;

	*buffer = NULL;

	if (!(b = p_info->first_queued))
		return 0; /* all buffers dequeued, timeout */

	timeout_alarm = FALSE;

	if (timeout) {
		struct itimerval iv;

		/* Sets the timer to expire (SIGALRM) if we do not
		   receive a frame within timeout. */
		/* XXX there's only ITIMER_REAL, may conflict with
		   caller use of timer, isn't reentrant. */

		iv.it_interval.tv_sec = 0;
		iv.it_interval.tv_usec = 0;

		if (0 == (timeout->tv_sec | timeout->tv_usec)) {
			/* XXX can we temporarily switch to nonblocking? */

			iv.it_value.tv_sec = 0;
			iv.it_value.tv_usec = 1000;
		} else {
			iv.it_value = *timeout;
		}

		if (-1 == setitimer (ITIMER_REAL, &iv, NULL)) {
			info->tveng_errno = errno;
			t_error("setitimer()", info);
			return -1; /* error */
		}
	} else {
		/* Block forever. */
	}

	frame = b->frame_number;

	/* XXX must bypass device_ioctl() to get EINTR. */
	while (-1 == ioctl (info->fd, VIDIOCSYNC, &frame)) {
		switch (errno) {
		case EINTR:
			if (timeout_alarm)
				return 0; /* timeout */

			continue;

		default:
			return -1; /* error */
		}
	}

  p_info->last_timestamp = p_tveng1_timestamp(p_info);

	if (timeout) {
		struct itimerval iv;

		CLEAR (iv); /* cancel alarm */

		/* Error ignored. */
		setitimer (ITIMER_REAL, &iv, NULL);
	}

	*buffer = b;

	p_info->first_queued = b->next_queued;

	b->next_queued = NULL;
	b->queued = FALSE;

	return 1; /* success */
}

static tv_bool
queue_xbuffer			(tveng_device_info *	info,
				 struct xbuffer *	b)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_mmap bm;
	struct xbuffer **xp;

	assert (!b->queued);

	if (b->clear) {
		if (!tv_clear_image (b->data, &info->capture.format))
			return FALSE;

		b->clear = FALSE;
	}

	CLEAR (bm);
  
	bm.format = pixfmt_to_palette (info, info->capture.format
				       .pixel_format->pixfmt);
	if (0 == bm.format) {
		info -> tveng_errno = -1;
		tv_error_msg(info, "Cannot understand the palette");
		return FALSE;
	}

	bm.frame = b->frame_number;
	bm.width = info -> capture.format.width;
	bm.height = info -> capture.format.height;

	if (-1 == xioctl (info, VIDIOCMCAPTURE, &bm)) {
		/* This comes from xawtv, it isn't in the V4L API */
		if (errno == EAGAIN)
			tv_error_msg(info, "VIDIOCMCAPTURE: "
				     "Grabber chip can't sync "
				     "(no station tuned in?)");
		return FALSE;
	}

	for (xp = &p_info->first_queued; *xp; xp = &(*xp)->next_queued)
		;

	*xp = b;

	b->next_queued = NULL;
	b->queued = TRUE;

	return TRUE;
}

static tv_bool
queue_xbuffers			(tveng_device_info *	info)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	unsigned int i;

	for (i = 0; i < p_info->n_buffers; ++i) {
		if (p_info->buffers[i].queued)
			continue;

		if (p_info->buffers[i].dequeued)
			continue;

		if (!queue_xbuffer (info, &p_info->buffers[i]))
			return FALSE;
	}

	return TRUE;
}

static tv_bool
unmap_xbuffers			(tveng_device_info *	info,
				 tv_bool		ignore_errors)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	tv_bool success;

	success = TRUE;

	if ((void *) -1 != p_info->mapped_addr) {
		if (-1 == device_munmap (info->log_fp,
					 p_info->mapped_addr,
					 p_info->mbuf.size)) {
			if (!ignore_errors) {
				info->tveng_errno = errno;
				t_error("munmap()", info);

				success = FALSE;
			}
		}

		p_info->mapped_addr = (void *) -1;
	}

	if (p_info->buffers) {
		free (p_info->buffers);

		p_info->buffers = NULL;
		p_info->n_buffers = 0;
	}

	return success;
}

static tv_bool
map_xbuffers			(tveng_device_info *	info)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	unsigned int i;

	assert ((void *) -1 == p_info->mapped_addr
		&& NULL == p_info->buffers);

	CLEAR (p_info->mbuf);

	if (-1 == xioctl (info, VIDIOCGMBUF, &p_info->mbuf))
		return FALSE;

	if (0 == p_info->mbuf.frames)
		return FALSE;

	/* Limited by the size of the mbuf.offset[] array. */
	p_info->n_buffers = MIN (p_info->mbuf.frames, VIDEO_MAX_FRAME);

	p_info->buffers = calloc (p_info->n_buffers, sizeof (struct xbuffer));
	if (!p_info->buffers) {
		p_info->n_buffers = 0;
		return FALSE;
	}

	p_info->mapped_addr = device_mmap (info->log_fp,
					   /* start: any */ NULL,
					   (size_t) p_info->mbuf.size,
					   PROT_READ | PROT_WRITE,
					   MAP_SHARED,
					   info->fd,
					   (off_t) 0);

	if ((void *) -1 == p_info->mapped_addr) {
		info->tveng_errno = errno;
		t_error("mmap()", info);

		free (p_info->buffers);

		p_info->buffers = NULL;
		p_info->n_buffers = 0;

		return FALSE;
	}

	for (i = 0; i < p_info->n_buffers; ++i) {
		p_info->buffers[i].data =
			(char *) p_info->mapped_addr
			+ p_info->mbuf.offsets[i];

		p_info->buffers[i].frame_number = i;
	}

	return TRUE;
}

static void p_tveng1_timestamp_init(tveng_device_info *info);

/*
  Sets up the capture device so any read() call after this one
  succeeds. Returns -1 on error.
*/
static int
tveng1_start_capturing(tveng_device_info * info)
{
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;
  gboolean dummy;

  p_tveng_stop_everything(info, &dummy);
  t_assert(info -> capture_mode == CAPTURE_MODE_NONE);

  p_tveng1_timestamp_init(info);

	if (!map_xbuffers (info)) {
		return -1;
	}

	if (!queue_xbuffers (info)) {
		unmap_xbuffers (info, /* ignore_errors */ TRUE);
		return -1;
	}

  info->capture_mode = CAPTURE_MODE_READ;

  p_info->streaming = TRUE;

  return 0;
}

/* Tries to stop capturing. -1 on error. */
static int
tveng1_stop_capturing(tveng_device_info * info)
{
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;
  struct timeval timeout;
  int r;

  if (info -> capture_mode == CAPTURE_MODE_NONE)
    {
      fprintf(stderr, 
	      "Warning: trying to stop capture with no capture active\n");
      return 0; /* Nothing to be done */
    }

  r = 0;

  t_assert(info->capture_mode == CAPTURE_MODE_READ);

	/* Dequeue all buffers. */

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	while (p_info->first_queued) {
		struct xbuffer *b;

		if (dequeue_xbuffer (info, &b, &timeout) <= 0) {
			/* FIXME caller cannot properly
			   handle stop error yet. */
			r = -1; /* error or timeout */
		}
	}

	if (!unmap_xbuffers (info, /* ignore_errors */ FALSE)) {
		/* FIXME caller cannot properly
		   handle stop error yet. */
		r = -1;
	}

  info->capture_mode = CAPTURE_MODE_NONE;

  p_info->streaming = FALSE;

  return r;
}

static tv_bool
capture_enable			(tveng_device_info *	info,
				 tv_bool		enable)
{
  int r;

  if (enable)
    r = tveng1_start_capturing (info);
  else
    r = tveng1_stop_capturing (info);

  return (0 == r);
}


static int
read_frame			(tveng_device_info *	info,
				 tv_capture_buffer *	buffer,
				 const struct timeval *	timeout)
{
	struct xbuffer *b;
	int r;

  t_assert(info != NULL);

  if (info -> capture_mode != CAPTURE_MODE_READ)
    {
      info -> tveng_errno = -1;
      tv_error_msg(info, "Current capture mode is not READ");
      return -1; /* error */
    }

	if ((r = dequeue_xbuffer (info, &b, timeout)) <= 0)
		return r; /* error or timeout */

	if (buffer) {
		const tv_image_format *dst_format;

		dst_format = buffer->format;
		if (!dst_format)
			dst_format = &info->capture.format;

		tv_copy_image (buffer->data, dst_format,
			       b->data, &info->capture.format);
	}

	if (!queue_xbuffer (info, b))
		return -1; /* error */

	return 1; /* success */
}

static int
ov511_get_button_state		(tveng_device_info	*info)
{
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;
  char button_state;

  if (p_info -> ogb_fd < 1)
    return -1; /* Unsupported feature */

  lseek(p_info -> ogb_fd, 0, SEEK_SET);
  if (read(p_info -> ogb_fd, &button_state, 1) < 1)
    return -1;

  return (button_state - '0');
}

/*
  Return fd for the device file opened. Checks if the device is a
  valid video device. -1 on error.
  Flags will be used for open()'ing the file 
*/
static
int p_tveng1_open_device_file(int flags, tveng_device_info * info)
{
  struct private_tveng1_device_info *p_info = P_INFO (info);
  struct video_capability caps;
  
  t_assert(info != NULL);
  t_assert(info -> file_name != NULL);

	info->fd = -1;

	if (!(info->node.device = strdup (info->file_name)))
		goto failure;

  info -> fd = device_open (info->log_fp, info -> file_name, flags, 0);
  if (-1 == info -> fd)
    {
      info->tveng_errno = errno; /* Just to put something other than 0 */
      t_error("open()", info);
      goto failure;
    }

  /* We check the capabilities of this video device */
  if (-1 == xioctl(info, VIDIOCGCAP, &caps))
    goto failure;

  /* Check if this device is convenient for capturing */
  if ( !(caps.type & VID_TYPE_CAPTURE) )
    {
      info->tveng_errno = -1;
      snprintf(info->error, 256, 
	       "%s doesn't look like a valid capture device", info
	       -> file_name);
      goto failure;
    }

#undef STRCOPY
#define STRCOPY(s) _tv_strndup ((s), N_ELEMENTS (s))

	/* We copy in case the string array lacks a NUL. */
	/* XXX localize (encoding). */
	if (!(info->node.label = STRCOPY (caps.name)))
		goto failure;

	info->node.bus = NULL; /* unknown */
	info->node.driver = NULL; /* unknown */
	info->node.version = NULL; /* unknown */

  /* Copy capability info*/
  snprintf(info->caps.name, 32, caps.name);
  info->caps.channels = caps.channels;
  info->caps.audios = caps.audios;
  info->caps.maxwidth = caps.maxwidth;
  info->caps.minwidth = caps.minwidth;
  info->caps.maxheight = caps.maxheight;
  info->caps.minheight = caps.minheight;
  info->caps.flags = 0;

  /* BTTV doesn't return properly the maximum width */
#ifdef TVENG1_BTTV_PRESENT
  if (info->caps.maxwidth > 768)
    info->caps.maxwidth = 768;
#endif

  /* Sets up the capability flags */
  if (caps.type & VID_TYPE_CAPTURE)
    info ->caps.flags |= TVENG_CAPS_CAPTURE;
  if (caps.type & VID_TYPE_TUNER)
    info ->caps.flags |= TVENG_CAPS_TUNER;
  if (caps.type & VID_TYPE_TELETEXT)
    info ->caps.flags |= TVENG_CAPS_TELETEXT;
  if (1)
    {
  if (caps.type & VID_TYPE_OVERLAY)
    info ->caps.flags |= TVENG_CAPS_OVERLAY;
  if (caps.type & VID_TYPE_CHROMAKEY)
    info ->caps.flags |= TVENG_CAPS_CHROMAKEY;
  if (caps.type & VID_TYPE_CLIPPING)
    info ->caps.flags |= TVENG_CAPS_CLIPPING;
    }
  if (caps.type & VID_TYPE_FRAMERAM)
    info ->caps.flags |= TVENG_CAPS_FRAMERAM;
  if (caps.type & VID_TYPE_SCALES)
    info ->caps.flags |= TVENG_CAPS_SCALES;
  if (caps.type & VID_TYPE_MONOCHROME)
    info ->caps.flags |= TVENG_CAPS_MONOCHROME;
  if (caps.type & VID_TYPE_SUBCAPTURE)
    info ->caps.flags |= TVENG_CAPS_SUBCAPTURE;

  p_info->mapped_addr = (char*) -1;
  p_info->buffers = NULL;
  p_info->first_queued = NULL;

	p_info->pwc_driver = FALSE;

	{
		struct pwc_probe probe;

		CLEAR (probe);

		if (0 == pwc_xioctl_may_fail (info, VIDIOCPWCPROBE, &probe)) {
			if (0 == strncmp (info->caps.name,
					  probe.name,
					  MIN (sizeof (info->caps.name),
					       sizeof (probe.name)))) {
				p_info->pwc_driver = TRUE;
			}
		}
	}

	p_info->bttv_driver = FALSE;

	if (!p_info->pwc_driver) {
		/* Rather poor, but we must not send a private
		   ioctl to innocent drivers. */
		if (strstr (info->caps.name, "bt")
		    || strstr (info->caps.name, "BT")) {
			int version;
			int dummy;

			version = bttv_xioctl_may_fail (info, BTTV_VERSION,
							&dummy);
			if (version != -1)
				p_info->bttv_driver = TRUE;
		}
	}

  /* This tries to fill the fb_info field */
  get_overlay_buffer (info);

  /* Set some flags for this device */
  fcntl( info -> fd, F_SETFD, FD_CLOEXEC );

	{
		struct sigaction sa;

		CLEAR (sa);
		sa.sa_handler = alarm_handler;
		/* no sa_flags = SA_RESTART to cause EINTR. */

		sigaction (SIGALRM, &sa, NULL);
	}

  /* Set the controller */
  info -> current_controller = TVENG_CONTROLLER_V4L1;

  p_info->read_back_controls	= FALSE;
  p_info->audio_mode_reads_rx	= TRUE;	/* bttv TRUE, other devices? */

  /* XXX should be autodetected */
#ifdef TVENG1_BTTV_MUTE_BUG_WORKAROUND
  p_info->mute_flag_readable	= FALSE;
#else
  p_info->mute_flag_readable	= TRUE;
#endif

  p_info->channel_norm_usable	= channel_norm_test (info);

  /* Everything seems to be OK with this device */
  return (info -> fd);

 failure:
	free (info->node.label);
	info->node.label = NULL;

	if (-1 != info->fd) {
		device_close (0, info->fd);
		info->fd = -1;
	}

	free (info->node.device);
	info->node.device = NULL;

	return -1;
}

/* Closes a device opened with tveng_init_device */
static void tveng1_close_device(tveng_device_info * info)
{
  struct private_tveng1_device_info *p_info=
    (struct private_tveng1_device_info*) info;

  t_assert(info != NULL);

  if (-1 != info->fd) {
    gboolean dummy;

    p_tveng_stop_everything(info, &dummy);
    device_close(info->log_fp, info -> fd);
    info -> fd = -1;
  }

  signal (SIGALRM, SIG_DFL);

  info -> current_controller = TVENG_CONTROLLER_NONE;

  if (info -> file_name)
    free(info -> file_name);

	free_controls (info);

	free_video_standards (info);

	free_video_inputs (info);

  if (p_info -> ogb_fd > 0)
    device_close(info->log_fp, p_info->ogb_fd);
  p_info ->ogb_fd = -1;

  info -> file_name = NULL;

  free (info->node.label);
  free (info->node.bus);
  free (info->node.driver);
  free (info->node.version);
  free (info->node.device);

  CLEAR (info->node);

  if (info->debug_level > 0)
    fprintf(stderr, "\nTVeng: V4L1 controller unloaded\n");
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
int tveng1_attach_device(const char* device_file,
			Window window _unused_,
			enum tveng_attach_mode attach_mode,
			tveng_device_info * info)
{
  int error;
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info *)info;
  struct stat st;
  int minor = -1;

  t_assert(device_file != NULL);
  t_assert(info != NULL);

  if (-1 != info -> fd) /* If the device is already attached, detach it */
    tveng_close_device(info);

  info -> file_name = strdup(device_file);
  if (!(info -> file_name))
    {
      perror("strdup");
      info->tveng_errno = errno;
      snprintf(info->error, 256, "Cannot duplicate device name");
      return -1;
    }

  if ((attach_mode == TVENG_ATTACH_XV) ||
      (attach_mode == TVENG_ATTACH_CONTROL) ||
      (attach_mode == TVENG_ATTACH_VBI))
    attach_mode = TVENG_ATTACH_READ;

  switch (attach_mode)
    {
      /* In V4L there is no control-only mode */
    case TVENG_ATTACH_READ:
      info -> fd = p_tveng1_open_device_file(O_RDWR, info);
      break;
    default:
      tv_error_msg(info, "Unknown attach mode for the device");
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

  /* We have a valid device, get some info about it */

	info->panel.set_video_input = set_video_input;
	info->panel.set_tuner_frequency	= set_tuner_frequency;
	info->panel.get_tuner_frequency	= get_tuner_frequency;
	info->panel.get_signal_strength	= get_signal_strength;
	info->panel.set_video_standard = set_video_standard;
	info->panel.get_video_standard = get_video_standard;
	info->panel.set_control	= set_control;
	info->panel.get_control = get_control;
	info->panel.set_audio_mode = set_audio_mode;

	/* Video inputs & standards */

	info->panel.video_inputs = NULL;
	info->panel.cur_video_input = NULL;

	info->panel.video_standards = NULL;
	info->panel.cur_video_standard = NULL;

	/* XXX error */
	get_video_input_list (info);

	if (!init_audio (P_INFO (info)))
		return -1;

  /* Query present controls */
  info->panel.controls = NULL;
  if (!get_control_list (info))
      return -1;

#ifdef TVENG1_BTTV_MUTE_BUG_WORKAROUND
  /* Mute the device, so we know for sure which is the mute value on
     startup */
/*  tveng1_set_mute(0, info); */
#endif

  CLEAR (info->capture);

  info->capture.get_format = get_capture_format;
  info->capture.set_format = set_capture_format;
  info->capture.enable = capture_enable;
  info->capture.read_frame = read_frame;

  info->capture.supported_pixfmt_set = get_supported_pixfmt_set (info);

  CLEAR (info->overlay);

  info->overlay.get_buffer = get_overlay_buffer;
  info->overlay.set_window_clipvec = set_overlay_window_clipvec;
  info->overlay.get_window = get_overlay_window;
  info->overlay.set_window_chromakey = set_overlay_window_chromakey;
  info->overlay.get_chromakey = get_overlay_chromakey;
  info->overlay.enable = enable_overlay;


  /* Set up the palette according to the one present in the system */
  error = info->current_bpp;

  if (error == -1)
    {
      tveng1_close_device(info);
      return -1;
    }

  if (!tv_image_format_init (&info->capture.format,
			     (info->caps.minwidth + info->caps.maxwidth) / 2, 
			     (info->caps.minheight + info->caps.maxheight) / 2,
			     0,
			     pig_depth_to_pixfmt ((unsigned) error),
			     0)) {
    info -> tveng_errno = -1;
    tv_error_msg(info,
		"Cannot find appropiate palette for current display");
    tveng1_close_device(info);
    return -1;
  }

  set_capture_format (info, &info->capture.format);

  /* init the private struct */
  p_info->mapped_addr = (void *) -1;
  p_info->buffers = NULL;
  p_info->first_queued = NULL;

  /* get the minor device number for accessing the appropiate /proc
     entry */
  if (!fstat(info -> fd, &st))
    minor = MINOR(st.st_rdev);

  p_info -> ogb_fd = -1;
  if (strstr(info -> caps.name, "OV51") &&
      strstr(info -> caps.name, "USB") &&
      minor > -1)
    {
      char filename[256];

      filename[sizeof(filename)-1] = 0;
      snprintf(filename, sizeof(filename)-1,
	       "/proc/video/ov511/%d/button", minor);
      p_info -> ogb_fd = device_open(0, filename, O_RDONLY, 0);
      if (p_info -> ogb_fd > 0 &&
	  flock (p_info->ogb_fd, LOCK_EX | LOCK_NB) == -1)
	{
	  device_close(0, p_info -> ogb_fd);
	  p_info -> ogb_fd = -1;
	}
    }

  if (info->debug_level > 0)
    fprintf(stderr, "TVeng: V4L1 controller loaded\n");

  return info -> fd;
}

static struct tveng_module_info tveng1_module_info = {
  .attach_device =		tveng1_attach_device,
  .close_device =		tveng1_close_device,
  .ioctl =			tveng1_ioctl,

  .ov511_get_button_state =	ov511_get_button_state,

  .interface_label		= "Video4Linux",

  .private_size =		sizeof(struct private_tveng1_device_info)
};

/*
  Inits the V4L1 module, and fills in the given table.
*/
void tveng1_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

  memcpy(module_info, &tveng1_module_info,
	 sizeof(struct tveng_module_info)); 
}

#else /* !ENABLE_V4L */

#include "tveng1.h"

void tveng1_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

  CLEAR (*module_info);
}

#endif /* ENABLE_V4L */
