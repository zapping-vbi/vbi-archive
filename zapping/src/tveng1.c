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

#include "site_def.h"

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
#define xioctl(p_info, cmd, arg)					\
	(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),				\
	 ((0 == device_ioctl ((p_info)->info.log_fp,			\
			      fprint_v4l_ioctl_arg,			\
			      (p_info)->info.fd, cmd, (void *)(arg))) ?	\
	  0 : (ioctl_failure (&(p_info)->info, __FILE__, __FUNCTION__,	\
			      __LINE__, # cmd), -1)))

#define xioctl_may_fail(p_info, cmd, arg)				\
	(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),				\
	 device_ioctl ((p_info)->info.log_fp, fprint_v4l_ioctl_arg,	\
		       (p_info)->info.fd, cmd, (void *)(arg)))

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

#define bttv_xioctl_may_fail(p_info, cmd, arg)				\
	(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),				\
	 device_ioctl ((p_info)->info.log_fp,				\
		       fprint_bttv_ioctl_arg,				\
		       (p_info)->info.fd, cmd, (void *)(arg)))

/* PWC driver extensions. */

#include "common/pwc-ioctl.h"
#include "common/_pwc-ioctl.h"

#define pwc_xioctl(p_info, cmd, arg)					\
	(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),				\
	 ((0 == device_ioctl ((p_info)->info.log_fp,			\
			      fprint_pwc_ioctl_arg,			\
			      (p_info)->info.fd, cmd, (void *)(arg))) ?	\
	  0 : (ioctl_failure (&(p_info)->info, __FILE__, __FUNCTION__,	\
			      __LINE__, # cmd), -1)))

#define pwc_xioctl_may_fail(p_info, cmd, arg)				\
	(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),				\
	 device_ioctl ((p_info)->info.log_fp, fprint_pwc_ioctl_arg,	\
		       (p_info)->info.fd, cmd, (void *)(arg)))

/* PWC TODO:
   Pan & tilt, real image size (cropping), DNR, flickerless,
   backlight compensation, sharpness, LEDs, whitebalance speed,
   shutter speed, AGC, serial number, compression quality,
   factory reset, user settings, bayer format, decompression.
*/

#ifndef TVENG1_RIVATV_TEST
#  define TVENG1_RIVATV_TEST 0
#endif
#ifndef TVENG1_XV_TEST
#  define TVENG1_XV_TEST 0
#endif

/* Copy a NUL-(un)terminated string from a char array. */
#define XSTRADUP(s) _tv_strndup ((s), N_ELEMENTS (s))

/* These drivers need special attention. */
enum driver {
	DRIVER_BTTV = 1,
	DRIVER_PWC,
	DRIVER_RIVATV
};

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
		       CONTROL_COLOUR |					\
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

/* Accessible through XVideo-V4L. */
#define XV_CONTROLS (CONTROL_BRIGHTNESS |				\
		     CONTROL_CONTRAST |					\
		     CONTROL_COLOUR |					\
		     CONTROL_HUE |					\
		     CONTROL_MUTE |					\
		     CONTROL_VOLUME)

struct control {
	tv_control		pub;
	control_id		id;
	Atom			atom;
};

#define C(l) PARENT (l, struct control, pub)




/** @internal */
struct xbuffer {
	void *			data;

	struct xbuffer *	next_queued;

	int			frame_number;

	struct timeval		sample_time;
	int64_t			stream_time;

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

  /* OV511 camera */
  int ogb_fd;

	tv_bool			exclusive_open;

	unsigned int		temp_open_count;

#if defined (HAVE_XV_EXTENSION) && TVENG1_XV_TEST
	/* Grabbed the info->overlay.xv_port_id for overlay. */
	tv_bool			grabbed_xv_port;

	Window			xwindow;
	GC			xgc;

	Atom			xa_xv_brightness;
	Atom			xa_xv_freq;
#endif

	tv_bool			overlay_active;

	/* See set_overlay_buffer(). */
	tv_image_format		overlay_buffer_format;
	tv_bool			use_overlay_limits;

	/* Info about mapped buffers. */
	void *			mapped_addr;
	struct video_mbuf	mbuf;

	struct xbuffer *	buffers;
	unsigned int		n_buffers;

	struct xbuffer *	first_queued;

	tv_bool			capture_active;

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
	tv_bool			read_back_format;

	enum driver		driver;
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

#if defined (HAVE_XV_EXTENSION) && TVENG1_XV_TEST

static tv_bool
put_video			(struct private_tveng1_device_info *p_info)
{
	unsigned int src_width, src_height;
	const tv_video_standard *s;

	src_width = 640;
	src_height = 480;

	s = p_info->info.panel.cur_video_standard;
	if (NULL != s) {
		src_width = s->frame_width;
		src_height = s->frame_height;
	}

	if (_tv_xv_put_video (&p_info->info,
			      p_info->xwindow,
			      p_info->xgc,
			      /* src_x, y */ 0, 0,
			      src_width, src_height))
		return TRUE;

	if (-1 != p_info->info.fd) {
		/* Error ignored. */
		device_close (p_info->info.log_fp, p_info->info.fd);
		p_info->info.fd = -1;
	}

	if (_tv_xv_put_video (&p_info->info,
			      p_info->xwindow,
			      p_info->xgc,
			      /* src_x, y */ 0, 0,
			      src_width, src_height)) {
		p_info->exclusive_open = TRUE;
		return TRUE;
	}

	return FALSE;
}

#endif

/* V4L prohibits multiple opens. This code temporarily opens and closes
   the device. */
static tv_bool
temp_close			(struct private_tveng1_device_info *p_info,
				 unsigned int		old_open_count)
{
	tv_bool success;

	/* Usually 0 == old_open_count. */
	if (p_info->temp_open_count <= old_open_count)
		return TRUE;

	if (--p_info->temp_open_count > 0)
		return TRUE;

	assert (-1 != p_info->info.fd);

	success = TRUE;

#if defined (HAVE_XV_EXTENSION) && TVENG1_XV_TEST
	if (p_info->overlay_active
	    & p_info->grabbed_xv_port
	    & p_info->exclusive_open) {
		/* Error ignored. */
		device_close (p_info->info.log_fp,
			      p_info->info.fd);
		p_info->info.fd = -1;

		success = put_video (p_info);

		p_info->overlay_active = success;
	} else
#endif
	if (TVENG_ATTACH_CONTROL == p_info->info.attach_mode) {
		/* Error ignored. */
		device_close (p_info->info.log_fp,
			      p_info->info.fd);
		p_info->info.fd = -1;

		success = TRUE;
	}

	return success;
}

static tv_bool
temp_open			(struct private_tveng1_device_info *p_info)
{
	tv_bool stop_overlay;

	if (-1 != p_info->info.fd) {
		++p_info->temp_open_count;
		return TRUE;
	}

	assert (0 == p_info->temp_open_count);

#if defined (HAVE_XV_EXTENSION) && TVENG1_XV_TEST
	stop_overlay = (p_info->overlay_active
			& p_info->grabbed_xv_port
			& p_info->exclusive_open);

	if (stop_overlay) {
		if (!_tv_xv_stop_video (&p_info->info, p_info->xwindow))
			return FALSE;
	}
#endif

	p_info->info.fd = device_open (p_info->info.log_fp,
				       p_info->info.file_name,
				       O_RDWR, /* mode */ 0);
	if (-1 != p_info->info.fd) {
		p_info->temp_open_count = 1;
		return TRUE;
	}

#if defined (HAVE_XV_EXTENSION) && TVENG1_XV_TEST
	if (stop_overlay) { 
		tv_bool success;

		/* Restore overlay if possible. */
		success = put_video (p_info);
		p_info->overlay_active = success;
	}
#endif

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

	if (TVENG1_RIVATV_TEST)
		return TRUE;

	assert (-1 != p_info->info.fd);

	CLEAR (audio);

	if (-1 == xioctl_may_fail (p_info, VIDIOCGAUDIO, &audio)) {
		switch (errno) {
		case EINVAL:
		case ENODEV:
			/* Tim: rivatv driver returns ENODEV when the card
			   has no audio. */
			return TRUE; /* success, no audio support */

		default:
			ioctl_failure (&p_info->info,
				       __FILE__,
				       __PRETTY_FUNCTION__,
				       __LINE__,
				       "VIDIOCGAUDIO");
			return FALSE;
		}
	}

	old_mode = audio.mode;
	received = audio.mode;

	fst_mode = (unsigned int) -1;
	cur_mode = (unsigned int) -1;

	capability = 0;

	p_info->audio_mode_reads_rx =
		(DRIVER_BTTV == p_info->driver || !SINGLE_BIT (audio.mode));

	/* To determine capabilities let's see which modes we can select. */
	for (mode = 1; mode <= (VIDEO_SOUND_LANG2 << 1); mode <<= 1) {
		audio.mode = mode >> 1; /* 0 == automatic */

		audio.flags |= VIDEO_AUDIO_MUTE;

		if (0 == xioctl_may_fail (p_info, VIDIOCSAUDIO, &audio)) {
			capability |= mode;

			if ((unsigned int) -1 == fst_mode)
				fst_mode = audio.mode;

			cur_mode = audio.mode;

			if (!p_info->audio_mode_reads_rx) {
				if (-1 == xioctl (p_info,
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

		if (-1 == xioctl (p_info, VIDIOCSAUDIO, &audio))
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

	if (TVENG1_RIVATV_TEST)
		return FALSE;

	if (!temp_open (p_info))
		return FALSE;

	CLEAR (audio);

	if (-1 == xioctl (p_info, VIDIOCGAUDIO, &audio))
		goto failure;

	if (!p_info->mute_flag_readable) {
		if (p_info->control_mute
		    && p_info->control_mute->value)
			audio.flags |= VIDEO_AUDIO_MUTE;
		else
			audio.flags &= ~VIDEO_AUDIO_MUTE;
	}

	audio.mode = tv_audio_mode_to_v4l_mode (mode);

	if (-1 == xioctl (p_info, VIDIOCSAUDIO, &audio))
		goto failure;

	if (p_info->info.panel.audio_mode != mode) {
		p_info->info.panel.audio_mode = mode;
		tv_callback_notify (&p_info->info, &p_info->info,
				    p_info->info.panel.audio_callback);
	}

	return temp_close (p_info, 0);

 failure:
	/* Error ignored. */
	temp_close (p_info, 0);

	return FALSE;
}

/*
	Controls
*/

static void
update_control_set_v4l		(struct private_tveng1_device_info *p_info,
				 const struct video_picture *pict,
				 const struct video_window *win,
				 const struct video_audio *audio,
				 control_id		control_set)
{
	tv_control *control;

	for_all (control, p_info->info.panel.controls) {
		struct control *c = C(control);
		int value;

		if (c->pub._parent != p_info)
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
			tv_callback_notify (&p_info->info, &c->pub,
					    c->pub._callback);
		}
	}
}

#if defined (HAVE_XV_EXTENSION) && TVENG1_XV_TEST

static tv_bool
get_control_set_xv		(struct private_tveng1_device_info *p_info,
				 control_id		control_set)
{
	tv_control *control;

	for_all (control, p_info->info.panel.controls) {
		struct control *c = C(control);
		int value;

		if (c->pub._parent != p_info)
			continue;

		if (TV_CONTROL_ID_AUDIO_MODE == c->pub.id)
			continue;

		if (0 == (control_set & c->id))
			continue;

		if (None == c->atom) {
			t_warn ("No atom for control 0x%x\n", c->id);
			p_info->info.tveng_errno = -1; /* unknown */
			return FALSE;
		}

		if (!_tv_xv_get_port_attribute (&p_info->info,
						c->atom, &value))
			return FALSE;

		if (CONTROL_MUTE != c->id)
			value = value * 65536 / 2000 + 32768;

		if (c->pub.value != value) {
			c->pub.value = value;
			tv_callback_notify (&p_info->info, &c->pub,
					    c->pub._callback);
		}
	}

	return TRUE;
}

#endif

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

	if (tc)
		control_set = C(tc)->id;
	else
		control_set = p_info->all_controls;

#if defined (HAVE_XV_EXTENSION) && TVENG1_XV_TEST
	if ((-1 == p_info->info.fd)
	    && (p_info->overlay_active
		& p_info->grabbed_xv_port
		& p_info->exclusive_open)
	    && 0 == (control_set & ~XV_CONTROLS)) {
		/* Don't stop and restart overlay just to get a
		   control we can also access through XVideo. */
		return get_control_set_xv (p_info, control_set);
	}
#endif

	if (!temp_open (p_info))
		return FALSE;

	if (control_set & PICT_CONTROLS) {
		CLEAR (pict);

		if (-1 == xioctl (p_info, VIDIOCGPICT, &pict))
			goto failure;

		control_set |= PICT_CONTROLS;
	}

	if (control_set & WINDOW_CONTROLS) {
		CLEAR (window);	

		if (-1 == xioctl (p_info, VIDIOCGWIN, &window))
			goto failure;

		control_set |= WINDOW_CONTROLS;
	}

	if (control_set & AUDIO_CONTROLS) {
		CLEAR (audio);

		if (-1 == xioctl (p_info, VIDIOCGAUDIO, &audio))
			goto failure;

		if (p_info->audio_mode_reads_rx) {
			set_audio_reception (p_info, audio.mode);
		}

		control_set |= AUDIO_CONTROLS;
	}

	update_control_set_v4l (p_info, &pict, &window, &audio, control_set);

	return temp_close (p_info, 0);

 failure:
	/* Error ignored. */
	temp_close (p_info, 0);

	return FALSE;
}

#if defined (HAVE_XV_EXTENSION) && TVENG1_XV_TEST

static tv_bool
set_control_xv			(struct private_tveng1_device_info *p_info,
				 tv_control *		tc,
				 int			value)
{
	struct control *c = C(tc);

	if (None == c->atom) {
		tveng_device_info *info = &p_info->info;

		t_warn ("No atom for control 0x%x\n", c->id);
		info->tveng_errno = -1; /* unknown */
		return FALSE;
	}

	if (CONTROL_MUTE != c->id)
		value = value * 2000 / 65536 - 1000;

	if (!_tv_xv_set_port_attribute (&p_info->info, c->atom, value))
		return FALSE;

	if (CONTROL_MUTE != c->id)
		value = value * 65536 / 2000 + 32768;

	if (c->pub.value != value) {
		c->pub.value = value;
		tv_callback_notify (&p_info->info, &c->pub, c->pub._callback);
	}

	return TRUE;
}

#endif

static tv_bool
set_control			(tveng_device_info *	info,
				 tv_control *		tc,
				 int			value)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct control *c = C(tc);

	if (p_info->control_audio_dec == tc)
		return set_audio_mode_control (&p_info->info, tc, value);

#if defined (HAVE_XV_EXTENSION) && TVENG1_XV_TEST
	if ((-1 == p_info->info.fd)
	    && (p_info->overlay_active
		& p_info->grabbed_xv_port
		& p_info->exclusive_open)
	    && 0 == (c->id & ~XV_CONTROLS))
		return set_control_xv (p_info, tc, value);
#endif

	if (!temp_open (p_info))
		return FALSE;

	if (c->id & PICT_CONTROLS) {
		struct video_picture pict;

		CLEAR (pict);

		if (-1 == xioctl (p_info, VIDIOCGPICT, &pict))
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

		if (-1 == xioctl (p_info, VIDIOCSPICT, &pict))
			goto failure;

		if (p_info->read_back_controls) {
			/* Error ignored */
			xioctl (p_info, VIDIOCGPICT, &pict);
		}

		update_control_set_v4l (p_info,
					&pict, NULL, NULL,
					PICT_CONTROLS);

	} else if (c->id & WINDOW_CONTROLS) {
		struct video_window window;
		unsigned int new_flags;

		CLEAR (window);

		if (-1 == xioctl (p_info, VIDIOCGWIN, &window))
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
			p_info->info.tveng_errno = -1; /* unknown */
			goto failure;
		}

		if (window.flags != new_flags) {
			window.flags = new_flags;

			if (-1 == xioctl (p_info, VIDIOCSWIN, &window))
				goto failure;

			if (p_info->read_back_controls) {
				/* Error ignored */
				xioctl (p_info, VIDIOCGWIN, &window);
			}
		}

		update_control_set_v4l (p_info,
					NULL, &window, NULL,
					WINDOW_CONTROLS);

	} else if (c->id & AUDIO_CONTROLS) {
		struct video_audio audio;
		unsigned int rx_mode;
		tv_bool no_read;

		CLEAR (audio);

		if (-1 == xioctl (p_info, VIDIOCGAUDIO, &audio))
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
			p_info->info.tveng_errno = -1; /* unknown */
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

		if (-1 == xioctl (p_info, VIDIOCSAUDIO, &audio))
			goto failure;

		if (p_info->read_back_controls) {
			/* Error ignored */
			xioctl (p_info, VIDIOCGAUDIO, &audio);
		}

		if (no_read && c->pub.value != value) {
			c->pub.value = value;
			tv_callback_notify (&p_info->info, &c->pub,
					    c->pub._callback);
		}

		update_control_set_v4l (p_info,
					NULL, NULL, &audio,
					AUDIO_CONTROLS);
	} else {
		t_warn ("Invalid c->id 0x%x\n", c->id);
		p_info->info.tveng_errno = -1; /* unknown */
		goto failure;
	}

	return temp_close (p_info, 0);

 failure:
	/* Error ignored. */
	temp_close (p_info, 0);

	return FALSE;
}

static tv_control *
add_control			(struct private_tveng1_device_info *p_info,
				 unsigned int		id,
				 const char *		label,
				 const char *		atom,
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

	if (atom) {
		/* Error handled in get/set functions. */
		c.atom = XInternAtom (p_info->info.display, atom,
				      /* only_if_exists */ False);
	}

	if ((tc = append_panel_control (&p_info->info, &c.pub, sizeof (c)))) {
		p_info->all_controls |= id;
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
#define ADD_STD_CONTROL(name, label, id, atom, value)			\
	add_control (p_info, CONTROL_##name, _(label), atom,		\
		     TV_CONTROL_ID_##id, TV_CONTROL_TYPE_INTEGER,	\
		     value, 32768, 0, 65535, 256)

static tv_bool
get_video_control_list		(struct private_tveng1_device_info *p_info)
{
	struct video_picture pict;

	CLEAR (pict);

	if (-1 == xioctl (p_info, VIDIOCGPICT, &pict))
		return FALSE;

	ADD_STD_CONTROL (BRIGHTNESS, "Brightness",
			 BRIGHTNESS, "XV_BRIGHTNESS", pict.brightness);
	ADD_STD_CONTROL (CONTRAST,   "Contrast",
			 CONTRAST,   "XV_CONTRAST",   pict.contrast);
	ADD_STD_CONTROL (COLOUR,     "Saturation",
			 SATURATION, "XV_SATURATION", pict.colour);
	ADD_STD_CONTROL (HUE,        "Hue",
			 HUE,        "XV_HUE",        pict.hue);

	return TRUE;
}

static tv_bool
get_audio_control_list		(struct private_tveng1_device_info *p_info)
{
	struct video_audio audio;
	tv_bool rewrite;

	assert (-1 != p_info->info.fd);

	CLEAR (audio);

	if (-1 == xioctl_may_fail (p_info, VIDIOCGAUDIO, &audio)) {
		switch (errno) {
		case EINVAL:
		case ENODEV:
			/* Tim: rivatv driver returns ENODEV when the card
			   has no audio. */
			return TRUE; /* success, no audio support */

		default:
			ioctl_failure (&p_info->info,
				       __FILE__,
				       __PRETTY_FUNCTION__,
				       __LINE__,
				       "VIDIOCGAUDIO");
			return FALSE;
		}
	}

	rewrite = FALSE;

	if (audio.flags & VIDEO_AUDIO_MUTABLE) {
		if (!p_info->mute_flag_readable) {
			audio.flags |= VIDEO_AUDIO_MUTE;
			rewrite = TRUE;
		}

		p_info->control_mute =
			add_control (p_info, CONTROL_MUTE,
				     _("Mute"),
				     "XV_MUTE",
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
		ADD_STD_CONTROL (VOLUME,  "Volume",
				 VOLUME,  "XV_VOLUME", audio.volume);

	if (audio.flags & VIDEO_AUDIO_BASS)
		ADD_STD_CONTROL (BASS,    "Bass",
				 BASS,    NULL,        audio.bass);

	if (audio.flags & VIDEO_AUDIO_TREBLE)
		ADD_STD_CONTROL (TREBLE,  "Treble",
				 TREBLE,  NULL,        audio.treble);

#ifdef VIDEO_AUDIO_BALANCE
	if (audio.flags & VIDEO_AUDIO_BALANCE)
		ADD_STD_CONTROL (BALANCE, "Balance",
				 UNKNOWN, NULL,        audio.balance);
#endif

	p_info->control_audio_dec = NULL;
	if (0 != p_info->tuner_audio_capability
	    && !SINGLE_BIT (p_info->tuner_audio_capability))
		p_info->control_audio_dec =
			append_audio_mode_control
			(&p_info->info, p_info->tuner_audio_capability);

	if (rewrite) {
		/* Can't read values, we must write to
		   synchronize. Error ignored. */
		xioctl (p_info, VIDIOCSAUDIO, &audio);
	}

	return TRUE;
}

static tv_bool
get_pwc_control_list		(struct private_tveng1_device_info *p_info)
{
	struct video_window window;

	assert (-1 != p_info->info.fd);

	CLEAR (window);

	if (-1 == xioctl (p_info, VIDIOCGWIN, &window))
		return FALSE;

	p_info->control_pwc_fps =
		add_control (p_info, CONTROL_PWC_FPS,
			     _("Frame Rate"),
			     /* atom */ NULL,
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
		add_control (p_info, CONTROL_PWC_SNAPSHOT,
			     _("Snapshot Mode"),
			     /* atom */ NULL,
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
get_control_list		(struct private_tveng1_device_info *p_info)
{
	free_panel_controls (&p_info->info);

	p_info->all_controls = 0;

	if (!get_video_control_list (p_info))
		return FALSE;

	if (!get_audio_control_list (p_info))
		return FALSE;

	if (DRIVER_PWC == p_info->driver) {
		if (!get_pwc_control_list (p_info))
			return FALSE;
	}

	return TRUE;
}

/*
	Video standards
*/

/* Test whether struct video_channel.norm can be used to get and set
   the current video standard. */
static tv_bool
channel_norm_test		(struct private_tveng1_device_info *p_info)
{
	struct video_channel channel;
	tv_video_line *l;
	unsigned int old_norm;
	unsigned int new_norm;
	tv_bool success;

	if (DRIVER_BTTV == p_info->driver
	    || DRIVER_RIVATV == p_info->driver)
		return TRUE;

	for_all (l, p_info->info.panel.video_inputs)
		if (l->type == TV_VIDEO_LINE_TYPE_BASEBAND)
			break;
	if (!l)
		return FALSE;

	assert (-1 != p_info->info.fd);

	CLEAR (channel);

	channel.channel = VI (l)->channel;

	if (-1 == xioctl (p_info, VIDIOCGCHAN, &channel))
		return FALSE;

	old_norm = channel.norm;
	new_norm = (old_norm == 0); /* PAL -> NTSC, NTSC -> PAL */

	channel.norm = new_norm;

	if (-1 == xioctl (p_info, VIDIOCSCHAN, &channel))
		return FALSE;

	if (0 == xioctl (p_info, VIDIOCGCHAN, &channel)) {
		success = (channel.norm == new_norm);
	} else {
		success = FALSE;
	}

	channel.norm = old_norm;

	if (-1 == xioctl (p_info, VIDIOCSCHAN, &channel))
		return FALSE;

	return success;
}

static tv_bool
update_capture_limits		(struct private_tveng1_device_info *p_info)
{
	struct video_capability caps;

	if (p_info->use_overlay_limits)
		return TRUE;

	if (TVENG1_RIVATV_TEST) {
		tv_video_standard *s;

		s = p_info->info.panel.cur_video_standard;
		if (NULL != s) {
			p_info->info.caps.maxwidth = s->frame_width;
			p_info->info.caps.maxheight = s->frame_height;
			return TRUE;
		}
	}

	if (!temp_open (p_info))
		return FALSE;

	if (0 == xioctl (p_info, VIDIOCGCAP, &caps)) {
		/* XXX all conversion routines
		   cannot handle arbitrary widths yet. */
		p_info->info.caps.minwidth = (caps.minwidth + 7) & ~7;
		p_info->info.caps.minheight = caps.minheight;
		p_info->info.caps.maxwidth = caps.maxwidth & ~7;
		p_info->info.caps.maxheight = caps.maxheight;
	} else {
		/* Let's hope this is ok. */
		p_info->info.caps.minwidth = 352;
		p_info->info.caps.minheight = 240;
		p_info->info.caps.maxwidth = 640;
		p_info->info.caps.maxheight = 480;
	}

	return temp_close (p_info, 0);
}

static tv_bool
get_video_standard		(tveng_device_info *	info)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	tv_video_standard *s;
	unsigned int old_open_count;
	unsigned int norm;

	old_open_count = p_info->temp_open_count;

	s = NULL; /* unknown */

	if (!info->panel.video_standards)
		goto store_s;

	if (info->panel.cur_video_input
	    && P_INFO (info)->channel_norm_usable) {
		struct video_channel channel;

		if (!temp_open (p_info))
			goto failure;

		CLEAR (channel);

		channel.channel = VI(info->panel.cur_video_input)->channel;

		if (-1 == xioctl (p_info, VIDIOCGCHAN, &channel))
			goto failure;

		norm = channel.norm;
	} else if (IS_TUNER_LINE (info->panel.cur_video_input)) {
		struct video_tuner tuner;

		if (!temp_open (p_info))
			goto failure;

		CLEAR (tuner);

		tuner.tuner = VI (info->panel.cur_video_input)->tuner;

		if (-1 == xioctl (p_info, VIDIOCGTUNER, &tuner))
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

		if (NULL == l)
			goto store_s;

		if (!temp_open (p_info))
			goto failure;

		CLEAR (tuner);

		tuner.tuner = VI (l)->tuner;

		if (-1 == xioctl (p_info, VIDIOCGTUNER, &tuner))
			goto failure;

		norm = tuner.mode;
	}

	for_all (s, info->panel.video_standards)
		if (S(s)->norm == norm)
			break;

 store_s:
	store_cur_video_standard (info, s);

	set_audio_capability (P_INFO (info));

	if (!update_capture_limits (p_info))
		goto failure;

	/* Close if we opened. */
	return temp_close (p_info, old_open_count);

 failure:
	/* Error ignored. */
	temp_close (p_info, old_open_count);

	return FALSE;
}

static tv_bool
set_video_standard		(tveng_device_info *	info,
				 tv_video_standard *	s)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	unsigned int old_open_count;
	unsigned int norm;
	int r;

	if (p_info->overlay_active
	    | p_info->capture_active)
		return FALSE;

	old_open_count = p_info->temp_open_count;

	norm = CS(s)->norm;

	if (info->panel.cur_video_input
	    && p_info->channel_norm_usable) {
		struct video_channel channel;

		if (!temp_open (p_info))
			goto failure;

		CLEAR (channel);

		channel.channel = VI (info->panel.cur_video_input)->channel;

		if (-1 == xioctl (p_info, VIDIOCGCHAN, &channel))
			goto failure;

		if (channel.norm == norm)
			goto success;

		channel.norm = norm;

		r = xioctl (p_info, VIDIOCSCHAN, &channel);
		if (0 == r) {
			store_cur_video_standard (info, s);

			set_audio_capability (P_INFO (info));

			if (!update_capture_limits (p_info))
				goto failure;
		}
	} else if (IS_TUNER_LINE (info->panel.cur_video_input)) {
		struct video_tuner tuner;

		if (!temp_open (p_info))
			goto failure;

		CLEAR (tuner);

		tuner.tuner = VI (info->panel.cur_video_input)->tuner;

		if (-1 == xioctl (p_info, VIDIOCGTUNER, &tuner))
			goto failure;

		if (tuner.mode == norm)
			goto success;

		if (!(tuner.flags & VIDEO_TUNER_NORM)) {
			errno = -1; /* FIXME */
			goto failure; /* not setable */
		}

		tuner.mode = norm;

		r = xioctl (p_info, VIDIOCSTUNER, &tuner);
		if (0 == r) {
			store_cur_video_standard (info, s);

			set_audio_capability (P_INFO (info));

			if (!update_capture_limits (p_info))
				goto failure;
		}
	} else {
		struct video_channel channel;
		struct video_tuner tuner;
		tv_video_line *l;
		tv_bool switched;

		/* Switch to an input with tuner,
		   change video standard and then switch back. */

		switched = FALSE;

		for_all (l, info->panel.video_inputs)
			if (l->type == TV_VIDEO_LINE_TYPE_TUNER)
				break;

		if (!l) {
			errno = -1; /* FIXME */
			goto failure;
		}

		if (!temp_open (p_info))
			goto failure;

		CLEAR (channel);

		channel.channel = VI (l)->channel;

		if (-1 == xioctl (p_info, VIDIOCGCHAN, &channel))
			goto failure;

		if (channel.norm == norm)
			goto success;

		if (-1 == xioctl (p_info, VIDIOCSCHAN, &channel))
			goto failure;

		CLEAR (tuner);

		tuner.tuner = VI (l)->tuner;

		r = xioctl (p_info, VIDIOCGTUNER, &tuner);
		if (-1 == r)
			goto restore;

		if (!(tuner.flags & VIDEO_TUNER_NORM)) {
			r = -1;
			goto restore;
		}

		tuner.mode = norm;

		r = xioctl (p_info, VIDIOCSTUNER, &tuner);
		switched = (0 == r);

	restore:
		channel.channel = VI (info->panel.cur_video_input)->channel;

		r = xioctl (p_info, VIDIOCSCHAN, &channel);
		if (-1 == r) {
			/* Notify about accidental video input change. */
			store_cur_video_input (info, l);
		}

		if (switched) {
			if (get_video_standard (info)) {
				store_cur_video_standard (info, s);

				set_audio_capability (P_INFO (info));

				if (!update_capture_limits (p_info))
					goto failure;
			} else {
				r = -1;
			}
		}

		if (-1 == r)
			goto failure;
	}

 success:
	/* Close if we opened. */
	return temp_close (p_info, old_open_count);

 failure:
	/* Error ignored. */
	temp_close (p_info, old_open_count);

	return FALSE;
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
	/* We need video standard parameters (frame size, rate) and
	   it's impossible (I think) to determine which standard has
	   been detected, so this is pretty much useless. Also drivers
	   supporting AUTO may not really detect standards but only
	   distinguish between 525 and 625 systems. */
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
get_video_standard_list		(struct private_tveng1_device_info *p_info)
{
	const struct standard_map *table;
	unsigned int flags;
	unsigned int i;

	if (!p_info->info.panel.cur_video_input) {
		free_video_standards (&p_info->info);
		return TRUE;
	}

	if (DRIVER_BTTV == p_info->driver) {
		if (p_info->info.panel.video_standards)
			goto get_current; /* invariable */

		table = bttv_standards;
	} else {
		table = v4l_standards;
	}

	free_video_standards (&p_info->info);

	if (IS_TUNER_LINE (p_info->info.panel.cur_video_input)) {
		/* For API compatibility bttv's VIDICGTUNER reports
		   only PAL, NTSC, SECAM. */
		if (DRIVER_BTTV == p_info->driver) {
			/* XXX perhaps we can probe supported standards? */
			flags = (unsigned int) ~0;
		} else {
			struct video_tuner tuner;
			tv_video_line *ci;

			assert (-1 != p_info->info.fd);

			CLEAR (tuner);

			ci = p_info->info.panel.cur_video_input;
			tuner.tuner = VI (ci)->tuner;

			if (-1 == xioctl (p_info, VIDIOCGTUNER, &tuner))
				return FALSE;

			flags = tuner.flags;
		}
	} else {
		/* Supported video standards of baseband inputs are
		   not reported. */

		flags = (unsigned int) ~0;
	}

	for (i = 0; table[i].label; ++i) {
		struct standard *s;

		if (!(flags & (1 << i)))
			continue; /* unsupported standard */

		if (!(s = S(append_video_standard
			    (&p_info->info.panel.video_standards,
			     table[i].set,
			     table[i].label,
			     table[i].label,
			     sizeof (*s)))))
			goto failure;

		s->norm = i;
	}

 get_current:
	if (TVENG1_RIVATV_TEST) {
		struct video_channel channel;

		assert (-1 != p_info->info.fd);

		CLEAR (channel);

		if (0 == xioctl (p_info, VIDIOCGCHAN, &channel)) {
			channel.norm = VIDEO_MODE_AUTO;

			/* Error ignored. */
			xioctl (p_info, VIDIOCSCHAN, &channel);
		}
	}

	if (get_video_standard (&p_info->info)) {
		if (NULL == p_info->info.panel.cur_video_standard) {
			tv_video_standard *s;

			/* AUTO not acceptable. */

			s = p_info->info.panel.video_standards; /* first */

			/* Error ignored. */
			set_video_standard (&p_info->info, s);
		}

		return TRUE;
	}

 failure:
	free_video_standard_list (&p_info->info.panel.video_standards);

	return FALSE;
}

/*
	Video inputs
*/

#define SCALE_FREQUENCY(vi, freq)					\
	((((freq) & ~ (unsigned long) vi->step_shift)			\
	   * vi->pub.u.tuner.step) >> vi->step_shift)

static void
store_frequency			(struct private_tveng1_device_info *p_info,
				 struct video_input *	vi,
				 unsigned long		freq)
{
	unsigned int frequency = SCALE_FREQUENCY (vi, freq);

	if (vi->pub.u.tuner.frequency != frequency) {
		vi->pub.u.tuner.frequency = frequency;
		tv_callback_notify (&p_info->info, &vi->pub,
				    vi->pub._callback);
	}
}

static tv_bool
get_tuner_frequency		(tveng_device_info *	info,
				 tv_video_line *	l)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	unsigned long freq;
	int r;

	/* cur_video_input may not be up-to-date, but there
	   is no ioctl to verify. */
	if (info->panel.cur_video_input != l)
		return TRUE;

	freq = 0;

#if defined (HAVE_XV_EXTENSION) && TVENG1_XV_TEST
	if ((-1 == p_info->info.fd)
	    & p_info->overlay_active
	    & p_info->grabbed_xv_port
	    & p_info->exclusive_open) {
		int value;

		if (None == p_info->xa_xv_freq) {
			t_warn ("No freq atom\n");
			info->tveng_errno = -1; /* unknown */
			return FALSE;
		}

		r = !!_tv_xv_get_port_attribute
			(info, p_info->xa_xv_freq, &value) - 1;

		freq = value;
	} else
#endif
	{
		if (!temp_open (p_info))
			return FALSE;

		r = xioctl (p_info, VIDIOCGFREQ, &freq);

		if (!temp_close (p_info, 0))
			return FALSE;
	}

	if (-1 == r)
		return FALSE;

	store_frequency (p_info, VI (l), freq);

	return TRUE;
}

static tv_bool
set_tuner_frequency		(tveng_device_info *	info,
				 tv_video_line *	l,
				 unsigned int		frequency)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_input *vi = VI (l);
	unsigned int old_open_count;
	unsigned long old_freq;
	unsigned long new_freq;

	old_open_count = p_info->temp_open_count;

	new_freq = (frequency << vi->step_shift) / vi->pub.u.tuner.step;

	/* cur_video_input may not be up-to-date, but there
	   is no ioctl to verify. */
	if (info->panel.cur_video_input != l)
		goto store_new_freq;

#if defined (HAVE_XV_EXTENSION) && TVENG1_XV_TEST
	if ((-1 == p_info->info.fd)
	    & p_info->overlay_active
	    & p_info->grabbed_xv_port
	    & p_info->exclusive_open) {
		if (None == p_info->xa_xv_freq) {
			t_warn ("No freq atom\n");
			info->tveng_errno = -1; /* unknown */
			goto failure;
		}

		if (!_tv_xv_set_port_attribute
		    (info, p_info->xa_xv_freq, new_freq))
			goto failure;
	} else
#endif
	{
		if (!temp_open (p_info))
			goto failure;

		old_freq = 0;

		if (0 == xioctl (p_info, VIDIOCGFREQ, &old_freq))
			if (old_freq == new_freq)
				goto store_new_freq;

		if (-1 == xioctl (p_info, VIDIOCSFREQ, &new_freq)) {
			store_frequency (p_info, vi, old_freq);

			goto failure;
		}
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

 store_new_freq:
	store_frequency (p_info, vi, new_freq);

	/* Close if we opened. */
	return temp_close (p_info, old_open_count);

 failure:
	/* Error ignored. */
	temp_close (p_info, old_open_count);

	return FALSE;
}

static tv_bool
get_signal_strength		(tveng_device_info *	info,
				 int *			strength,
				 int *			afc _unused_)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_tuner tuner;

	if (NULL == strength)
		return TRUE;

	if (!temp_open (p_info))
		return FALSE;

	CLEAR (tuner);
	tuner.tuner = 0; /* XXX correct? */

	if (-1 == xioctl (p_info, VIDIOCGTUNER, &tuner)) {
		/* Error ignored. */
		temp_close (p_info, 0);

		return FALSE;
	}

	*strength = tuner.signal;

	return temp_close (p_info, 0);
}

static tv_bool
set_video_input			(tveng_device_info *	info,
				 tv_video_line *	l)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_channel channel;

	if (p_info->overlay_active
	    | p_info->capture_active)
		return FALSE;

	if (!temp_open (p_info))
		return FALSE;

	CLEAR (channel);

	channel.channel = CVI (l)->channel;

	if (-1 == xioctl (p_info, VIDIOCGCHAN, &channel))
		goto failure;

	/* There is no ioctl to query the current video input,
	   so unfortunately we cannot take a shortcut. */

	if (-1 == xioctl (p_info, VIDIOCSCHAN, &channel))
		goto failure;

	store_cur_video_input (info, l);

	/* Implies get_video_standard() and set_audio_capability(). */
	get_video_standard_list (p_info);

	/* V4L does not promise per-tuner frequency setting as we do.
	   XXX in panel mode ignores the possibility that a third
	   party changed the frequency from the value we know. */
	if (IS_TUNER_LINE (l)) {
		tv_video_line *ci;

		ci = info->panel.cur_video_input;
		set_tuner_frequency (info, ci, ci->u.tuner.frequency);
	}

	return temp_close (p_info, 0);

 failure:
	/* Error ignored. */
	temp_close (p_info, 0);

	return FALSE;
}

static tv_bool
tuner_bounds			(struct private_tveng1_device_info *p_info,
				 struct video_input *	vi)
{
	struct video_tuner tuner;
	unsigned long freq;

	assert (-1 != p_info->info.fd);

	CLEAR (tuner);

	tuner.tuner = vi->tuner;

	if (-1 == xioctl (p_info, VIDIOCGTUNER, &tuner))
		return FALSE;

	assert (tuner.rangelow <= tuner.rangehigh);

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

	if (-1 == xioctl (p_info, VIDIOCGFREQ, &freq))
		return FALSE;

	store_frequency (p_info, vi, freq);

	return TRUE;
}

static tv_bool
get_video_input_list		(struct private_tveng1_device_info *p_info)
{
	struct video_channel channel;
	unsigned int i;

	free_video_inputs (&p_info->info);

	for (i = 0; i < (unsigned int) p_info->info.caps.channels; ++i) {
		struct video_input *vi;
		char buf[sizeof (channel.name) + 1];
		tv_video_line_type type;

		assert (-1 != p_info->info.fd);

		CLEAR (channel);

		channel.channel = i;

		if (-1 == xioctl (p_info, VIDIOCGCHAN, &channel))
			continue;

		switch (channel.type) {
		case VIDEO_TYPE_TV:
			if (TVENG1_RIVATV_TEST)
				continue;

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
			       (&p_info->info.panel.video_inputs,
				type, buf, buf, sizeof (*vi)))))
			goto failure;

		vi->pub._parent = p_info;

		vi->channel = i;

		/* FIXME allocate one video_line for each tuner. */
		vi->tuner = 0;

		if (channel.type == VIDEO_TYPE_TV) {
			if (-1 == xioctl (p_info, VIDIOCSCHAN, &channel))
				return FALSE;

			if (!tuner_bounds (p_info, vi))
				goto failure;
		}
	}

	return TRUE;

 failure:
	free_video_line_list (&p_info->info.panel.video_inputs);

	return FALSE;
}

/*
	Capture and Overlay
 */

static tv_pixfmt
palette_to_pixfmt		(unsigned int		palette)
{
	switch (Z_BYTE_ORDER) {
	case Z_LITTLE_ENDIAN:
		switch (palette) {
		case VIDEO_PALETTE_GREY:	return TV_PIXFMT_Y8;
		case VIDEO_PALETTE_HI240:	return TV_PIXFMT_UNKNOWN;

		case VIDEO_PALETTE_RGB565:	return TV_PIXFMT_BGR16_LE;
		case VIDEO_PALETTE_RGB24:	return TV_PIXFMT_BGR24_LE;
		case VIDEO_PALETTE_RGB32:	return TV_PIXFMT_BGRA32_LE;
		case VIDEO_PALETTE_RGB555:	return TV_PIXFMT_BGRA16_LE;

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
		}

		break;

	case Z_BIG_ENDIAN:
		switch (palette) {
		case VIDEO_PALETTE_GREY:	return TV_PIXFMT_Y8;
		case VIDEO_PALETTE_HI240:	return TV_PIXFMT_UNKNOWN;

		case VIDEO_PALETTE_RGB565:	return TV_PIXFMT_BGR16_BE;
		case VIDEO_PALETTE_RGB24:	return TV_PIXFMT_BGR24_BE;
		case VIDEO_PALETTE_RGB32:	return TV_PIXFMT_BGRA32_BE;
		case VIDEO_PALETTE_RGB555:	return TV_PIXFMT_BGRA16_BE;

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
		}

		break;
	}

	return TV_PIXFMT_UNKNOWN;
}

static unsigned int
pixfmt_to_palette		(struct private_tveng1_device_info *p_info,
				 tv_pixfmt		pixfmt)
{
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

static unsigned int
tv_to_v4l_chromakey		(unsigned int		chromakey)
{
	switch (Z_BYTE_ORDER) {
	case Z_LITTLE_ENDIAN:
		/* XXX correct? 0xAARRGGBB */
		return chromakey & 0xFFFFFF;

	case Z_BIG_ENDIAN:
		/* XXX correct? 0xBBGGRRAA */
		return (((chromakey & 0xFF) << 24) |
			((chromakey & 0xFF00) << 8) |
			((chromakey & 0xFF0000) >> 8));

	default:
		assert (0);
	}

	return 0;
}

static unsigned int
v4l_to_tv_chromakey		(unsigned int		chromakey)
{
	switch (Z_BYTE_ORDER) {
	case Z_LITTLE_ENDIAN:
		/* XXX correct? 0xAARRGGBB */
		return chromakey & 0xFFFFFF;

	case Z_BIG_ENDIAN:
		/* XXX correct? 0xBBGGRRAA */
		return (((chromakey & 0xFF00) << 8) |
			((chromakey & 0xFF0000) >> 8) |
			((chromakey & 0xFF000000) >> 24));

	default:
		assert (0);
	}

	return 0;
}

/* Struct video_picture and video_window determine parameters
   for capturing and overlay. */
static tv_bool
get_capture_and_overlay_parameters
				(struct private_tveng1_device_info *p_info)
{
	struct video_picture pict;
	struct video_window window;

	if (!temp_open (p_info))
		return FALSE;

	CLEAR (pict);

	if (-1 == xioctl (p_info, VIDIOCGPICT, &pict))
		goto failure;

	CLEAR (window);

	if (-1 == xioctl (p_info, VIDIOCGWIN, &window))
		goto failure;

	/* Current capture format. */

	if (!tv_image_format_init (&p_info->info.capture.format,
				   window.width,
				   window.height,
				   /* bytes_per_line: minimum */ 0,
				   palette_to_pixfmt (pict.palette),
				   TV_COLSPC_UNKNOWN)) {
		p_info->info.tveng_errno = -1; /* unknown */
		tv_error_msg(&p_info->info, "Cannot understand the palette");
		goto failure;
	}

	/* Current overlay window. */

	p_info->info.overlay.window.x		= window.x;
	p_info->info.overlay.window.y		= window.y;
	p_info->info.overlay.window.width	= window.width;
	p_info->info.overlay.window.height	= window.height;

	/* Overlay clips cannot be read back, we assume no change.
	   tveng.c takes care of p_info->info.overlay.clip_vector. */

	p_info->info.overlay.chromakey =
		v4l_to_tv_chromakey (window.chromakey);

	update_control_set_v4l (p_info,
				&pict, &window, NULL,
				PICT_CONTROLS | WINDOW_CONTROLS);

	return temp_close (p_info, 0);

 failure:
	/* Error ignored. */
	temp_close (p_info, 0);

	return FALSE;
}

/*
	Overlay
*/

static tv_bool
get_overlay_buffer		(tveng_device_info *	info)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_buffer buffer;
	tv_pixfmt pixfmt;

	CLEAR (p_info->info.overlay.buffer);

	if (!(p_info->info.caps.flags & TVENG_CAPS_OVERLAY))
		return FALSE;

	if (!temp_open (p_info))
		return FALSE;

	CLEAR (buffer);

	if (-1 == xioctl (p_info, VIDIOCGFBUF, &buffer))
		goto failure;

	p_info->info.overlay.buffer.base = (unsigned long) buffer.base;

	if (0 && DRIVER_RIVATV == p_info->driver) {
		p_info->info.overlay.buffer.format =
			p_info->overlay_buffer_format;
		goto success;
	}

	pixfmt = pig_depth_to_pixfmt ((unsigned int) buffer.depth);

	if (tv_image_format_init (&info->overlay.buffer.format,
				  (unsigned int) buffer.width,
				  (unsigned int) buffer.height,
				  (unsigned int) buffer.bytesperline,
				  pixfmt,
				  TV_COLSPC_UNKNOWN)) {
		if (TVENG1_RIVATV_TEST) {
			/* This is just a simulation, must not exceed
			   the real limits. */
		} else if (TVENG_ATTACH_XV == p_info->info.attach_mode
			   && DRIVER_RIVATV == p_info->driver) {
			/* Overlay has no limits, but VIDIOCGCAP tells only
			   when overlay is already enabled. */
			p_info->info.caps.maxwidth = buffer.width;
			p_info->info.caps.maxheight = buffer.height;

			p_info->use_overlay_limits = TRUE;
		}
	} else {
		tv_error_msg (info, _("Driver %s returned an unknown or "
				      "invalid frame buffer format."),
			      p_info->info.node.label);

		p_info->info.tveng_errno = EINVAL;
	}

 success:
	return temp_close (p_info, 0);

 failure:
	/* Error ignored. */
	temp_close (p_info, 0);

	return FALSE;
}

static tv_bool
set_overlay_buffer		(tveng_device_info *	info,
				 const tv_overlay_buffer *target)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_buffer buffer;
	const tv_pixel_format *pf;

	if (!(p_info->info.caps.flags & TVENG_CAPS_OVERLAY))
		return FALSE;

	if (0 && DRIVER_RIVATV == p_info->driver) {
		P_INFO (info)->overlay_buffer_format = target->format;
		return get_overlay_buffer (info);
	}

	if (-1 == p_info->info.fd) {
		/* In attach_mode CONTROL or called XvPutVideo() before. */
		p_info->info.tveng_errno = errno;
		t_error ("Wrong mode", info);
		return FALSE;
	}

	buffer.base = (void *) target->base;
	buffer.width = target->format.width;
	buffer.height = target->format.height;

	pf = target->format.pixel_format;

	if (32 == pf->bits_per_pixel)
		buffer.depth = 32; /* depth 24 bpp 32 */
	else
		buffer.depth = pf->color_depth; /* 15, 16, 24 */

	buffer.bytesperline = target->format.bytes_per_line[0];

	if (-1 == xioctl (p_info, VIDIOCSFBUF, &buffer))
		return FALSE;

	return get_overlay_buffer (info);
}

static tv_bool
get_overlay_window		(tveng_device_info *	info)
{
	return get_capture_and_overlay_parameters (P_INFO (info));
}

/*
  Sets the preview window dimensions to the given window.
  Success doesn't mean that the requested dimensions are used, maybe
  they are different, check the returned fields to see if they are suitable
  info   : Device we are controlling
*/
static tv_bool
set_overlay_window		(tveng_device_info *	info,
				 const tv_window *	w,
				 const tv_clip_vector *	v,
				 unsigned int		chromakey)
{
	struct private_tveng1_device_info * p_info = P_INFO (info);
	struct video_window window;
	struct video_clip *clips;

	if (-1 == p_info->info.fd) {
		/* In attach_mode CONTROL or called XvPutVideo() before. */
		p_info->info.tveng_errno = errno;
		t_error ("Wrong mode", info);
		return FALSE;
	}

	if (v->size > 0) {
		struct video_clip *vc;
		const tv_clip *tc;
		unsigned int i;

		clips = malloc (v->size * sizeof (*clips));
		if (NULL == clips) {
			p_info->info.tveng_errno = errno;
			t_error("malloc", info);
			return FALSE;
		}

		vc = clips;
		tc = v->vector;

		for (i = 0; i < v->size; ++i) {
			vc->next	= vc + 1; /* just in case */
			vc->x		= tc->x1;
			vc->y		= tc->y1;
			vc->width	= tc->x2 - tc->x1;
			vc->height	= tc->y2 - tc->y1;
			++vc;
			++tc;
		}

		vc[-1].next = NULL;
	} else {
		clips = NULL;
	}

	CLEAR (window);

	window.x		= w->x - p_info->info.overlay.buffer.x;
	window.y		= w->y - p_info->info.overlay.buffer.y;

	window.width		= w->width;
	window.height		= w->height;

	window.clips		= clips;
	window.clipcount	= v->size;

	window.chromakey	= tv_to_v4l_chromakey (chromakey);

	if (-1 == xioctl (p_info, VIDIOCSWIN, &window)) {
		free (clips);
		return FALSE;
	}

	free (clips);

	/* Actual window size. */

	if (-1 == xioctl (p_info, VIDIOCGWIN, &window))
		return FALSE;

	p_info->info.overlay.window.x		= window.x;
	p_info->info.overlay.window.y		= window.y;
	p_info->info.overlay.window.width	= window.width;
	p_info->info.overlay.window.height	= window.height;

	/* Clips cannot be read back, we assume no change.
	   tveng.c takes care of p_info->info.overlay.clip_vector. */

	p_info->info.overlay.chromakey =
		v4l_to_tv_chromakey (window.chromakey);

	return TRUE;
}

#if defined (HAVE_XV_EXTENSION) && TVENG1_XV_TEST

static tv_bool
set_overlay_xwindow		(tveng_device_info *	info,
				 Window			window,
				 GC			gc,
				 unsigned int		chromakey)
{
	struct private_tveng1_device_info * p_info = P_INFO (info);

	/* The XVideo V4L wrapper supports only clip-list overlay.
	   (Or does it?) */
	/* XXX tell the caller. */
	chromakey = chromakey;

	if (!p_info->grabbed_xv_port) {
		/* XXX shouldn't attach_device report this? */
		p_info->info.tveng_errno = EBUSY;
		t_error("XvGrabPort", info);
		return FALSE;
	}

	p_info->xwindow = window;
	p_info->xgc = gc;

	return TRUE;
}

#endif

static tv_bool
enable_overlay			(tveng_device_info *	info,
				 tv_bool		on)
{
	struct private_tveng1_device_info * p_info = P_INFO (info);
	int value = !!on;

#if defined (HAVE_XV_EXTENSION) && TVENG1_XV_TEST
	if (p_info->grabbed_xv_port
	    && 0 != p_info->xwindow
	    && 0 != p_info->xgc) {
		tv_bool success;

		if (p_info->overlay_active == on)
			return TRUE;

		if (on) {
			success = put_video (p_info);
			p_info->overlay_active = success;
		} else {
			success = _tv_xv_stop_video (info, p_info->xwindow);
			p_info->overlay_active = !success;
		}

		return success;
	} else
#endif
	{
		if (0 == xioctl (p_info, VIDIOCCAPTURE, &value)) {
			/* Caller shall use a timer instead. */
			/* usleep (50000); */
			p_info->overlay_active = TRUE;
			return TRUE;
		} else {
			p_info->overlay_active = FALSE;
			return FALSE;
		}
	}
}

/*
	Capture
*/

static tv_bool
get_capture_format		(tveng_device_info * info)
{
	return get_capture_and_overlay_parameters (P_INFO (info));
}

static tv_bool
set_capture_format		(tveng_device_info *	info,
				 const tv_image_format *fmt)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_picture pict1;
	struct video_picture pict2;
	struct video_window window;
	int r;

	if (-1 == p_info->info.fd) {
		/* In attach_mode CONTROL or called XvPutVideo() before. */
		p_info->info.tveng_errno = errno;
		t_error ("Wrong mode", info);
		return FALSE;
	}

	CLEAR (pict1);

	if (-1 == xioctl (p_info, VIDIOCGPICT, &pict1))
		return FALSE;

	pict1.palette = pixfmt_to_palette (p_info, fmt->pixel_format->pixfmt);

	if (0 == pict1.palette) {
		p_info->info.tveng_errno = EINVAL;
		tv_error_msg (info, "%s not supported",
			      fmt->pixel_format->name);
		return FALSE;
	}

	r = xioctl (p_info, VIDIOCSPICT, &pict1);
	if (-1 == r)
		return FALSE;

	if (p_info->read_back_format) {
		CLEAR (pict2);

		r = xioctl (p_info, VIDIOCGPICT, &pict2);
		if (-1 == r)
			return FALSE;

		/* v4l1-compat: Discards VIDIOC_S_FMT error. */
		if (pict1.palette != pict2.palette) {
			errno = EINVAL;
			return FALSE;
		}
	}

	p_info->info.capture.format.width =
		(fmt->width + 3) & (unsigned int) -4;
	p_info->info.capture.format.height =
		(fmt->height + 3) & (unsigned int) -4;

	p_info->info.capture.format.width =
		SATURATE (p_info->info.capture.format.width,
			  p_info->info.caps.minwidth,
			  p_info->info.caps.maxwidth);
	p_info->info.capture.format.height =
		SATURATE (p_info->info.capture.format.height,
			  p_info->info.caps.minheight,
			  p_info->info.caps.maxheight);

	CLEAR (window);

	window.width = p_info->info.capture.format.width;
	window.height = p_info->info.capture.format.height;

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

	if (-1 == xioctl_may_fail (p_info, VIDIOCSWIN, &window)) {
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

				if (0 == xioctl_may_fail (p_info, VIDIOCSWIN,
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

				if (0 == xioctl_may_fail (p_info, VIDIOCSWIN,
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

static tv_bool
supported_pixfmt		(tveng_device_info *	info,
				 const struct video_picture *pict,
				 tv_pixfmt		pixfmt)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_picture pict1;
	struct video_picture pict2;

	pict1 = *pict;

	pict1.palette = pixfmt_to_palette (p_info, pixfmt);
	if (0 == pict1.palette)
		return FALSE;

	if (0 == xioctl_may_fail (p_info, VIDIOCSPICT, &pict1)) {
		if (!p_info->read_back_format)
			return TRUE;

		CLEAR (pict2);

		if (-1 == xioctl_may_fail (p_info, VIDIOCGPICT, &pict2))
			return FALSE;

		/* v4l1-compat bug: Discards VIDIOC_S_FMT error. */
		if (pict1.palette == pict2.palette)
			return TRUE;

		/* These are synonyms, some drivers
		   understand only one. */
		if (VIDEO_PALETTE_YUYV == pict1.palette
		    && VIDEO_PALETTE_YUV422 == pict2.palette) {
			p_info->palette_yuyv = VIDEO_PALETTE_YUV422;
			return TRUE;
		}

		return FALSE;
	}

	if (VIDEO_PALETTE_YUYV != pict1.palette)
		return FALSE;

	pict1.palette = VIDEO_PALETTE_YUV422;

	if (0 == xioctl_may_fail (p_info, VIDIOCSPICT, &pict1)) {
		if (!p_info->read_back_format) {
			p_info->palette_yuyv = VIDEO_PALETTE_YUV422;
			return TRUE;
		}

		CLEAR (pict2);

		if (-1 == xioctl_may_fail (p_info, VIDIOCGPICT, &pict2))
			return FALSE;

		if (pict1.palette == pict2.palette) {
			p_info->palette_yuyv = VIDEO_PALETTE_YUV422;
			return TRUE;
		}
	}

	return FALSE;
}

static tv_pixfmt_set
get_supported_pixfmt_set	(tveng_device_info *	info)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_picture pict;
	tv_pixfmt_set pixfmt_set;
	tv_pixfmt pixfmt;

	p_info->palette_yuyv = VIDEO_PALETTE_YUYV;

	if (DRIVER_RIVATV == p_info->driver) {
		/* rivatv 0.8.6 feature: does not EINVAL if the palette is
		   unsupported. UYVY is always supported. Other formats
		   if software conversion or DMA is enabled? Another ioctl
		   testing palette is VIDIOCMCAPTURE. */
		return TV_PIXFMT_SET (TV_PIXFMT_UYVY);
	}

	assert (-1 != p_info->info.fd);

	CLEAR (pict);

	if (-1 == xioctl (p_info, VIDIOCGPICT, &pict))
		return TV_PIXFMT_SET_EMPTY;

	pixfmt_set = TV_PIXFMT_SET_EMPTY;

	for (pixfmt = 0; pixfmt < TV_MAX_PIXFMTS; ++pixfmt) {
		if (supported_pixfmt (info, &pict, pixfmt))
			pixfmt_set |= TV_PIXFMT_SET (pixfmt);
	}

	return pixfmt_set;
}

/* FIXME this is not reentrant and my conflict with other uses. */
static sig_atomic_t timeout_alarm;

static void
alarm_handler			(int			signum _unused_)
{
	timeout_alarm = TRUE;
}

static int
dequeue_xbuffer			(struct private_tveng1_device_info *p_info,
				 struct xbuffer **	buffer,
				 const struct timeval *	timeout)
{
	struct xbuffer *b;
	int frame;
	int r;

	assert (-1 != p_info->info.fd);

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
			p_info->info.tveng_errno = errno;
			t_error("setitimer()", &p_info->info);
			return -1; /* error */
		}
	} else {
		/* Block forever. */
	}

	frame = b->frame_number;

	/* XXX must bypass device_ioctl() to get EINTR. */
	while (-1 == ioctl (p_info->info.fd, VIDIOCSYNC, &frame)) {
		switch (errno) {
		case EINTR:
			if (timeout_alarm)
				return 0; /* timeout */

			continue;

		default:
			return -1; /* error */
		}
	}

	r = gettimeofday (&b->sample_time, /* timezone */ NULL);
	assert (0 == r);

	b->stream_time = 0; /* FIXME */
	
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
queue_xbuffer			(struct private_tveng1_device_info *p_info,
				 struct xbuffer *	b)
{
	struct video_mmap bm;
	struct xbuffer **xp;

	assert (!b->queued);

	if (b->clear) {
		if (!tv_clear_image (b->data, &p_info->info.capture.format))
			return FALSE;

		b->clear = FALSE;
	}

	CLEAR (bm);
  
	bm.format = pixfmt_to_palette (p_info, p_info->info.capture.format
				       .pixel_format->pixfmt);
	if (0 == bm.format) {
		p_info ->info. tveng_errno = -1;
		tv_error_msg(&p_info->info, "Cannot understand the palette");
		return FALSE;
	}

	bm.frame = b->frame_number;
	bm.width = p_info ->info. capture.format.width;
	bm.height = p_info ->info. capture.format.height;

	if (-1 == xioctl (p_info, VIDIOCMCAPTURE, &bm)) {
		/* This comes from xawtv, it isn't in the V4L API */
		if (errno == EAGAIN)
			tv_error_msg(&p_info->info, "VIDIOCMCAPTURE: "
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
queue_xbuffers			(struct private_tveng1_device_info *p_info)
{
	unsigned int i;

	for (i = 0; i < p_info->n_buffers; ++i) {
		if (p_info->buffers[i].queued)
			continue;

		if (p_info->buffers[i].dequeued)
			continue;

		if (!queue_xbuffer (p_info, &p_info->buffers[i]))
			return FALSE;
	}

	return TRUE;
}

static tv_bool
unmap_xbuffers			(struct private_tveng1_device_info *p_info,
				 tv_bool		ignore_errors)
{
	tv_bool success;

	assert (-1 != p_info->info.fd);

	success = TRUE;

	if ((void *) -1 != p_info->mapped_addr) {
		if (-1 == device_munmap (p_info->info.log_fp,
					 p_info->mapped_addr,
					 p_info->mbuf.size)) {
			if (!ignore_errors) {
				p_info->info.tveng_errno = errno;
				t_error("munmap()", &p_info->info);

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
map_xbuffers			(struct private_tveng1_device_info *p_info)
{
	unsigned int i;

	assert (-1 != p_info->info.fd);

	assert ((void *) -1 == p_info->mapped_addr
		&& NULL == p_info->buffers);

	CLEAR (p_info->mbuf);

	if (-1 == xioctl (p_info, VIDIOCGMBUF, &p_info->mbuf))
		return FALSE;

	if (0 == p_info->mbuf.frames) {
		p_info->info.tveng_errno = ENOMEM;
		return FALSE;
	}

	/* Limited by the size of the mbuf.offset[] array. */
	p_info->n_buffers = MIN (p_info->mbuf.frames, VIDEO_MAX_FRAME);

	p_info->buffers = calloc (p_info->n_buffers, sizeof (struct xbuffer));
	if (NULL == p_info->buffers) {
		p_info->n_buffers = 0;
		p_info->info.tveng_errno = ENOMEM;
		return FALSE;
	}

	p_info->mapped_addr = device_mmap (p_info->info.log_fp,
					   /* start: any */ NULL,
					   (size_t) p_info->mbuf.size,
					   PROT_READ | PROT_WRITE,
					   MAP_SHARED,
					   p_info->info.fd,
					   (off_t) 0);

	if (MAP_FAILED == p_info->mapped_addr) {
		p_info->info.tveng_errno = errno;
		t_error("mmap()", &p_info->info);

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

/*
  Sets up the capture device so any read() call after this one
  succeeds. Returns -1 on error.
*/
static int
tveng1_start_capturing(struct private_tveng1_device_info *p_info)
{
  gboolean dummy;

  p_tveng_stop_everything(&p_info->info, &dummy);
  assert (CAPTURE_MODE_NONE == p_info ->info. capture_mode);

	if (!map_xbuffers (p_info)) {
		return -1;
	}

	if (!queue_xbuffers (p_info)) {
		unmap_xbuffers (p_info, /* ignore_errors */ TRUE);
		return -1;
	}

  p_info->info.capture_mode = CAPTURE_MODE_READ;

  p_info->capture_active = TRUE;

  return 0;
}

/* Tries to stop capturing. -1 on error. */
static int
tveng1_stop_capturing(struct private_tveng1_device_info *p_info)
{
  struct timeval timeout;
  int r;

  if (p_info->info.capture_mode == CAPTURE_MODE_NONE)
    {
      fprintf(stderr, 
	      "Warning: trying to stop capture with no capture active\n");
      return 0; /* Nothing to be done */
    }

  r = 0;

  assert (CAPTURE_MODE_READ == p_info->info.capture_mode);

	/* Dequeue all buffers. */

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	while (p_info->first_queued) {
		struct xbuffer *b;

		if (dequeue_xbuffer (p_info, &b, &timeout) <= 0) {
			/* FIXME caller cannot properly
			   handle stop error yet. */
			r = -1; /* error or timeout */
		}
	}

	if (!unmap_xbuffers (p_info, /* ignore_errors */ FALSE)) {
		/* FIXME caller cannot properly
		   handle stop error yet. */
		r = -1;
	}

  p_info->info.capture_mode = CAPTURE_MODE_NONE;

  p_info->capture_active = FALSE;

  return r;
}

static tv_bool
capture_enable			(tveng_device_info *	info,
				 tv_bool		enable)
{
  int r;

  if (enable)
    r = tveng1_start_capturing (P_INFO (info));
  else
    r = tveng1_stop_capturing (P_INFO (info));

  return (0 == r);
}


static int
read_frame			(tveng_device_info *	info,
				 tv_capture_buffer *	buffer,
				 const struct timeval *	timeout)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct xbuffer *b;
	int r;

  assert (NULL != info);

  if (info -> capture_mode != CAPTURE_MODE_READ)
    {
      info -> tveng_errno = -1;
      tv_error_msg(info, "Current capture mode is not READ");
      return -1; /* error */
    }

	if ((r = dequeue_xbuffer (p_info, &b, timeout)) <= 0)
		return r; /* error or timeout */

	if (buffer) {
		const tv_image_format *dst_format;
		tv_bool success;

		dst_format = buffer->format;
		if (!dst_format)
			dst_format = &p_info->info.capture.format;

		success = tv_copy_image (buffer->data, dst_format,
					 b->data,
					 &p_info->info.capture.format);
		assert (success);

		buffer->sample_time = b->sample_time;
		buffer->stream_time = b->stream_time;
	}

	if (!queue_xbuffer (p_info, b))
		return -1; /* error */

	return 1; /* success */
}

static int
tveng1_ioctl			(tveng_device_info *	info,
				 unsigned int		cmd,
				 char *			arg)
{
	return device_ioctl (info->log_fp, fprint_v4l_ioctl_arg,
			     info->fd, cmd, arg);
}

static int
ov511_get_button_state		(tveng_device_info *	info)
{
  struct private_tveng1_device_info *p_info = P_INFO (info);
  char button_state;

  if (p_info -> ogb_fd < 1)
    return -1; /* Unsupported feature */

  lseek(p_info -> ogb_fd, 0, SEEK_SET);
  if (read(p_info -> ogb_fd, &button_state, 1) < 1)
    return -1;

  return (button_state - '0');
}

static void
free_capabilities		(struct private_tveng1_device_info *p_info)
{
	free (p_info->info.node.device);
	p_info->info.node.device = NULL;

	free (p_info->info.node.label);
	p_info->info.node.label = NULL;
}

static void
shut_down			(struct private_tveng1_device_info *p_info)
{
	if (-1 != p_info->ogb_fd) {
		device_close (p_info->info.log_fp,
			      p_info->ogb_fd);
		p_info->ogb_fd = -1;
	}

#if defined (HAVE_XV_EXTENSION) && TVENG1_XV_TEST
	if (p_info->grabbed_xv_port) {
		if (!_tv_xv_ungrab_port (&p_info->info)) {
			p_info->info.overlay.xv_port_id = NO_PORT;
			p_info->info.caps.flags &= ~TVENG_CAPS_XVIDEO;
		}

		p_info->grabbed_xv_port = FALSE;
	}
#endif

	signal (SIGALRM, SIG_DFL);

	free_panel_controls (&p_info->info);

	free_video_standards (&p_info->info);

	free_video_inputs (&p_info->info);

	p_info->info.current_controller = TVENG_CONTROLLER_NONE;

	free_capabilities (p_info);

	if (-1 != p_info->info.fd) {
		device_close (p_info->info.log_fp,
			      p_info->info.fd);
		p_info->info.fd = -1;
	}

	free (p_info->info.file_name);
	p_info->info.file_name = NULL;

	p_info->info.attach_mode = TVENG_ATTACH_UNKNOWN;
	p_info->info.capture_mode = CAPTURE_MODE_NONE;
}

static void
tveng1_close_device		(tveng_device_info * info)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	gboolean dummy;

	p_tveng_stop_everything (info, &dummy);

	shut_down (p_info);

	if (info->debug_level > 0)
		fprintf (stderr, "\nTVeng: V4L1 controller unloaded\n");
}

static tv_bool
ov511_init			(struct private_tveng1_device_info *p_info)
{
	char filename[256];
	struct stat st;
	int minor;

	p_info->ogb_fd = -1;

	if (NULL == strstr (p_info->info.caps.name, "OV51")
	    || NULL == strstr (p_info->info.caps.name, "USB"))
		return FALSE;

	minor = -1;

	if (0 == fstat (p_info->info.fd, &st))
		minor = MINOR (st.st_rdev);

	if (-1 == minor)
		return FALSE;

	snprintf (filename, sizeof (filename),
		  "/proc/video/ov511/%d/button", minor);

	p_info->ogb_fd = device_open (p_info->info.log_fp,
				      filename, O_RDONLY, /* mode */ 0);
	if (-1 == p_info->ogb_fd)
		return FALSE;

	if (-1 == flock (p_info->ogb_fd, LOCK_EX | LOCK_NB)) {
		device_close (p_info->info.log_fp,
			      p_info->ogb_fd);
		p_info->ogb_fd = -1;
		return FALSE;
	}

	return TRUE;
}

static void
grab_xv_port			(struct private_tveng1_device_info *p_info)
{
#if defined (HAVE_XV_EXTENSION) && TVENG1_XV_TEST
	if (NO_PORT == p_info->info.overlay.xv_port_id)
		return;

	if (_tv_xv_grab_port (&p_info->info)) {
		p_info->grabbed_xv_port = TRUE;
		p_info->xwindow = 0;
		p_info->xgc = 0;
#if 0
		/* XXX see tveng25.c's grab_xv_port(). */
		p_info->xa_colorkey = XInternAtom (p_info->info.display,
						   "XV_COLORKEY",
						   /* only_if_exists */ False);
#endif
	}
#endif
}

#if TVENG1_XV_TEST

#ifdef HAVE_XV_EXTENSION

static tv_bool
test_xv_port			(struct private_tveng1_device_info *p_info,
				 tv_bool *		restore_brightness)
{
	static const unsigned int magic = 0x16; /* 010110 */
	unsigned int sensed;
	unsigned int last;
	unsigned int i;

	if (!_tv_xv_grab_port (p_info)) {
		return FALSE;
	}

	sensed = 0;
	last = 0;

	for (i = 0; i < 6; ++i) {
		unsigned int bit = magic & (1 << i);
		struct video_picture pict;

		*restore_brightness = TRUE;

		if (!_tv_xv_set_port_attribute (&p_info->info,
						p_info->xa_xv_brightness,
						bit ? +1000 : -1000,
						/* sync */ TRUE)) {
			sensed = 0;
			goto failure;
		}

		if (-1 == xioctl (p_info, VIDIOCGPICT, &pict)) {
			sensed = 0;
			goto failure;
		}

		sensed |= (pict.brightness >= last) << i;
		last = pict.brightness;
	}

 failure:
	/* Error ignored. */
	_tv_xv_ungrab_port (p_info);

	return (0 == ((sensed ^ magic) & ~1));
}

static void
xvideo_probe			(struct private_tveng1_device_info *p_info)
{
	XErrorHandler old_error_handler;
	unsigned int version;
	unsigned int revision;
	unsigned int major_opcode;
	unsigned int event_base;
	unsigned int error_base;
	Window root_window;
	XvAdaptorInfo *adaptors;
	unsigned int n_adaptors;
	struct video_picture pict;
	tv_bool restore_brightness;
	unsigned int i;

	adaptors = NULL;

	p_info->info.overlay.xv_port_id = NO_PORT;

	old_error_handler = XSetErrorHandler (x11_error_handler);

	/* Error ignored. */
	p_info->xa_xv_freq = XInternAtom (p_info->info.display,
					  "XV_FREQ",
					  /* only_if_exists */ False);

	p_info->xa_xv_brightness = XInternAtom (p_info->info.display,
						"XV_BRIGHTNESS",
						/* only_if_exists */ True);
	if (None == p_info->xa_xv_brightness) {
		goto failure;
	}

	if (Success != XvQueryExtension (p_info->info.display,
					 &version, &revision,
					 &major_opcode,
					 &event_base, &error_base)) {
		goto failure;
	}

	if (version < 2 || (version == 2 && revision < 2)) {
		goto failure;
	}

	root_window = DefaultRootWindow (p_info->info.display);

	if (Success != XvQueryAdaptors (p_info->info.display,
					root_window,
					&n_adaptors, &adaptors)) {
		goto failure;
	}

	if (0 == n_adaptors) {
		goto failure;
	}

	/* XXX this is probably redundant. */
	if (-1 == xioctl (p_info, VIDIOCGPICT, &pict)) {
		goto failure;
	}

	restore_brightness = FALSE;

	for (i = 0; i < n_adaptors; ++i) {
		unsigned int j;

		if (0 != strcmp (adaptors[i].name, "video4linux")) {
			continue;
		}

		for (j = 0; j < adaptors[i].num_ports; ++j) {
			p_info->info.overlay.xv_port_id =
				(XvPortID)(adaptors[i].base_id + j);

			if (test_xv_port (p_info, &restore_brightness)) {
				goto found;
			}
		}
	}

 found:
	if (restore_brightness) {
		/* Error ignored. */
		xioctl (p_info, VIDIOCSPICT, &pict);
	}

	if (io_debug_msg > 0) {
		fprintf (stderr, "Found Xv port %d.\n",
			 (int) p_info->info.overlay.xv_port_id);
	}

 failure:
	if (NULL != adaptors) {
		XvFreeAdaptorInfo (adaptors);

		adaptors = NULL;
		n_adaptors = 0;
	}

	XSetErrorHandler (old_error_handler);
}

#else /* !HAVE_XV_EXTENSION */

static void
xvideo_probe			(struct private_tveng1_device_info *p_info)
{
	/* Nothing to do. */
}

#endif /* !HAVE_XV_EXTENSION */

#endif /* 0 */

static void
identify_driver			(struct private_tveng1_device_info *p_info)
{
	struct pwc_probe probe;

	if (strstr (p_info->info.caps.name, "rivatv")) {
		p_info->driver = DRIVER_RIVATV;
		return;
	}

	CLEAR (probe);

	if (0 == pwc_xioctl_may_fail (p_info, VIDIOCPWCPROBE, &probe)) {
		if (0 == strncmp (p_info->info.caps.name,
				  probe.name,
				  MIN (sizeof (p_info->info.caps.name),
				       sizeof (probe.name)))) {
			p_info->driver = DRIVER_PWC;
			return;
		}
	}

	/* Other drivers may accept the same private ioctl code. Check
	   the caps.name for more confidence. */
	if (strstr (p_info->info.caps.name, "bt")
	    || strstr (p_info->info.caps.name, "BT")) {
		int version;
		int dummy;

		dummy = -1;

		version = bttv_xioctl_may_fail (p_info, BTTV_VERSION, &dummy);
		if (version > 0) {
			p_info->driver = DRIVER_BTTV;
			return;
		}
	}

	p_info->driver = 0; /* unknown */
}

static tv_bool
get_capabilities		(struct private_tveng1_device_info *p_info)
{
	struct video_capability caps;

	CLEAR (p_info->info.caps);

	p_info->info.node.label		= NULL; /* unknown */
	p_info->info.node.bus		= NULL;
	p_info->info.node.driver	= NULL;
	p_info->info.node.version	= NULL;
	p_info->info.node.device	= NULL;

	p_info->info.node.destroy	= NULL;

	assert (-1 != p_info->info.fd);

	CLEAR (caps);

	if (-1 == xioctl (p_info, VIDIOCGCAP, &caps))
		goto failure;

	if (0 == (caps.type & (VID_TYPE_CAPTURE |
			       VID_TYPE_OVERLAY))) {
		p_info->info.tveng_errno = -1;
		snprintf(p_info->info.error, 256, 
			 "%s doesn't look like a valid capture device",
			 p_info->info.file_name);
		goto failure;
	}

	/* XXX localize (encoding). */
	p_info->info.node.label		= XSTRADUP (caps.name);

	p_info->info.node.device	= strdup (p_info->info.file_name);

	if (NULL == p_info->info.node.label
	    || NULL == p_info->info.node.device)
		goto failure;

	z_strlcpy (p_info->info.caps.name, caps.name,
		   MIN (sizeof (p_info->info.caps.name),
			sizeof (caps.name)));
	p_info->info.caps.name [N_ELEMENTS (p_info->info.caps.name) - 1] = 0;

	p_info->info.caps.channels	= caps.channels;
	p_info->info.caps.audios	= caps.audios;
	/* XXX all conversion routines
	   cannot handle arbitrary widths yet. */
	p_info->info.caps.maxwidth	= caps.maxwidth & ~7;
	p_info->info.caps.minwidth	= (caps.minwidth + 7) & ~7;
	p_info->info.caps.maxheight	= caps.maxheight;
	p_info->info.caps.minheight	= caps.minheight;

	/* BTTV doesn't return properly the maximum width */
#if 0 && defined (TVENG1_BTTV_PRESENT) /* or does it? */
	if (p_info->info.caps.maxwidth > 768)
		p_info->info.caps.maxwidth = 768;
#endif

	if (caps.type & VID_TYPE_CAPTURE)
		p_info->info.caps.flags |= TVENG_CAPS_CAPTURE;
	if (caps.type & VID_TYPE_TUNER)
		p_info->info.caps.flags |= TVENG_CAPS_TUNER;
	if (caps.type & VID_TYPE_TELETEXT)
		p_info->info.caps.flags |= TVENG_CAPS_TELETEXT;
	if (caps.type & VID_TYPE_OVERLAY)
		p_info->info.caps.flags |= TVENG_CAPS_OVERLAY;
	if (caps.type & VID_TYPE_CHROMAKEY)
		p_info->info.caps.flags |= TVENG_CAPS_CHROMAKEY;
	if (caps.type & VID_TYPE_CLIPPING)
		p_info->info.caps.flags |= TVENG_CAPS_CLIPPING;
	if (caps.type & VID_TYPE_FRAMERAM)
		p_info->info.caps.flags |= TVENG_CAPS_FRAMERAM;
	if (caps.type & VID_TYPE_SCALES)
		p_info->info.caps.flags |= TVENG_CAPS_SCALES;
	if (caps.type & VID_TYPE_MONOCHROME)
		p_info->info.caps.flags |= TVENG_CAPS_MONOCHROME;
	if (caps.type & VID_TYPE_SUBCAPTURE)
		p_info->info.caps.flags |= TVENG_CAPS_SUBCAPTURE;

	p_info->read_back_controls	= FALSE;
	p_info->read_back_format	= TRUE;
	p_info->audio_mode_reads_rx	= TRUE;	/* bttv TRUE, other devices? */

	/* XXX should be autodetected */
#ifdef TVENG1_BTTV_MUTE_BUG_WORKAROUND
	p_info->mute_flag_readable	= FALSE;
#else
	p_info->mute_flag_readable	= TRUE;
#endif

	if (TVENG1_RIVATV_TEST) {
		p_info->info.caps.channels	= 2;
		p_info->info.caps.audios	= 0;

		p_info->info.caps.minwidth	= 64;
		p_info->info.caps.minheight	= 32;

		/* Will be adjusted later in get_video_standard(). */
		p_info->info.caps.maxwidth	= 704;
		p_info->info.caps.maxheight	= 576;

		p_info->info.caps.flags		= (TVENG_CAPS_CAPTURE |
						   TVENG_CAPS_OVERLAY |
						   TVENG_CAPS_CHROMAKEY);

		p_info->driver			= DRIVER_RIVATV;
	} else {
		identify_driver (p_info);
	}

	return TRUE;

 failure:
	free_capabilities (p_info);

	return FALSE;
}

static tv_bool
do_open				(struct private_tveng1_device_info *p_info)
{
	p_info->info.fd = -1;

	/* XXX see zapping_setup_fb for a safer version. */
	p_info->info.fd = device_open (p_info->info.log_fp,
				       p_info->info.file_name,
				       O_RDWR, /* mode */ 0);
	if (-1 == p_info->info.fd) {
		p_info->info.tveng_errno = errno;
		t_error("open()", &p_info->info);
		goto failure;
	}

	return TRUE;

 failure:
	if (-1 != p_info->info.fd) {
		device_close (p_info->info.log_fp,
			      p_info->info.fd);
		p_info->info.fd = -1;
	}

	return FALSE;
}

static int
tveng1_change_attach_mode	(tveng_device_info * info,
				 Window window,
				 enum tveng_attach_mode attach_mode)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	gboolean dummy;

	window = window;

	p_tveng_stop_everything (info, &dummy);

#if defined (HAVE_XV_EXTENSION) && TVENG1_XV_TEST
	if (p_info->grabbed_xv_port) {
		if (!_tv_xv_ungrab_port (info)) {
			info->overlay.xv_port_id = NO_PORT;
			info->caps.flags &= ~TVENG_CAPS_XVIDEO;
		}

		p_info->grabbed_xv_port = FALSE;
	}
#endif

	if (-1 != info->fd) {
		/* Error ignored. */
		device_close (info->log_fp, info->fd);
		info->fd = -1;
	}

	switch (attach_mode) {
	case TVENG_ATTACH_XV: /* i.e. overlay */
		info->attach_mode = TVENG_ATTACH_XV;
		/* Attn: get_overlay_buffer() behaves
		   differently in this mode. */
		if (!do_open (p_info))
			goto failure;
		grab_xv_port (p_info);
		break;

	case TVENG_ATTACH_CONTROL:
	case TVENG_ATTACH_VBI:
		/* In V4L there is no control-only mode (but we'll pretend in
		   the future). VBI mode is unnecessary. */

		/* fall through */

	case TVENG_ATTACH_READ:
		info->attach_mode = TVENG_ATTACH_READ;
		if (!do_open (p_info))
			goto failure;
		break;

	default:
		tv_error_msg(info, "Unknown attach mode for the device");
		goto failure;
	}

	info->capture_mode = CAPTURE_MODE_NONE;

	return 0; /* ok */

 failure:
	shut_down (p_info);

	return -1;
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
			 Window window,
			 enum tveng_attach_mode attach_mode,
			 tveng_device_info * info)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);

	window = window;

	assert (NULL != device_file);
	assert (NULL != info);

	memset ((char *) p_info + sizeof (p_info->info), 0,
		sizeof (*p_info) - sizeof (*info));

	if (-1 != info->fd) {
		tveng_close_device (info);
		info->fd = -1;
	}

	info->capture_mode = CAPTURE_MODE_NONE;

	info->file_name = strdup (device_file);
	if (NULL == info->file_name) {
		perror("strdup");
		info->tveng_errno = ENOMEM;
		snprintf(info->error, 256, "Cannot duplicate device name");
		return -1;
	}

	switch (attach_mode) {
	case TVENG_ATTACH_XV: /* i.e. overlay */
		info->attach_mode = TVENG_ATTACH_XV;
		/* Attn: get_overlay_buffer() behaves
		   differently in this mode. */
		if (!do_open (p_info))
			goto failure;
		if (!get_capabilities (p_info))
			goto failure;
		grab_xv_port (p_info);
		break;

	case TVENG_ATTACH_CONTROL:
	case TVENG_ATTACH_VBI:
		/* In V4L there is no control-only mode (but we'll pretend in
		   the future). VBI mode is unnecessary. */

		/* fall through */

	case TVENG_ATTACH_READ:
		info->attach_mode = TVENG_ATTACH_READ;
		if (!do_open (p_info))
			goto failure;
		if (!get_capabilities (p_info))
			goto failure;
		break;

	default:
		tv_error_msg(info, "Unknown attach mode for the device");
		goto failure;
	}

	info->current_controller = TVENG_CONTROLLER_V4L1;

	info->panel.set_video_input = set_video_input;
	info->panel.set_tuner_frequency	= set_tuner_frequency;
	info->panel.get_tuner_frequency	= get_tuner_frequency;
	info->panel.get_signal_strength	= get_signal_strength;
	info->panel.set_video_standard = set_video_standard;
	info->panel.get_video_standard = get_video_standard;
	info->panel.set_control	= set_control;
	info->panel.get_control = get_control;
	info->panel.set_audio_mode = set_audio_mode;

	info->panel.video_inputs = NULL;
	info->panel.cur_video_input = NULL;

	info->panel.video_standards = NULL;
	info->panel.cur_video_standard = NULL;

	/* XXX error ignored */
	if (get_video_input_list (p_info)) {
		p_info->channel_norm_usable = channel_norm_test (p_info);

		if (info->panel.video_inputs) {
			/* There is no ioctl to query the current
			   video input, we can only reset to a
			   known channel. */
			/* XXX error ignored */
			set_video_input (info, info->panel.video_inputs);
		}
	}

	if (!init_audio (p_info))
		goto failure;

	info->panel.controls = NULL;

	if (!get_control_list (p_info))
		goto failure;

#ifdef TVENG1_BTTV_MUTE_BUG_WORKAROUND
	/* Mute the device, so we know for sure which is the mute value on
	   startup */
	/*  tveng1_set_mute(0, info); */
#endif

	CLEAR (info->capture);

	if (info->caps.flags & TVENG_CAPS_CAPTURE) {
		struct sigaction sa;

		info->capture.get_format = get_capture_format;
		info->capture.set_format = set_capture_format;
		info->capture.enable = capture_enable;
		info->capture.read_frame = read_frame;

		p_info->mapped_addr = (void *) -1;
		p_info->buffers = NULL;
		p_info->first_queued = NULL;

		info->capture.supported_pixfmt_set =
			get_supported_pixfmt_set (info);

		/* Set up an alarm handler for read timeouts. */

		CLEAR (sa);
		sa.sa_handler = alarm_handler;
		/* no sa_flags = SA_RESTART to cause EINTR. */

		sigaction (SIGALRM, &sa, NULL);
	}

	CLEAR (info->overlay);

	if (info->caps.flags & TVENG_CAPS_OVERLAY) {
		info->overlay.set_buffer = set_overlay_buffer;
		info->overlay.get_buffer = get_overlay_buffer;
		info->overlay.set_window = set_overlay_window;
		info->overlay.get_window = get_overlay_window;
		info->overlay.enable = enable_overlay;

		/* XXX error ignored. */
		/* FIXME cx88 0.0.5 sets the overlay capability flag
		   although it doesn't really support overlay, returning
		   EINVAL on VIDIOCGFBUF. (However a work-around is
		   already in tveng25.c.) */
		get_overlay_buffer (info);

#if defined (HAVE_XV_EXTENSION) && TVENG1_XV_TEST
		/* The XVideo V4L wrapper supports only clip-list overlay. */
		if (info->caps.flags & TVENG_CAPS_CLIPPING) {
			xvideo_probe (p_info);
			/* XXX if failed or not done, probe and set
			   exclusive_open instead of assuming yes. */

			if (NO_PORT != info->overlay.xv_port_id) {
				info->caps.flags |= TVENG_CAPS_XVIDEO;

				info->overlay.set_xwindow =
					set_overlay_xwindow;
			}
		}
#endif
	}

	if (info->caps.flags & (TVENG_CAPS_CAPTURE |
				TVENG_CAPS_OVERLAY)) {
		/* XXX error ignored. */
		get_capture_and_overlay_parameters (p_info);
	}

	ov511_init (p_info);

	if (info->debug_level > 0)
		fprintf(stderr, "TVeng: V4L1 controller loaded\n");

	return info->fd;

 failure:
	shut_down (p_info);

	return -1;
}

static struct tveng_module_info tveng1_module_info = {
  .attach_device =		tveng1_attach_device,
  .close_device =		tveng1_close_device,
  .ioctl =			tveng1_ioctl,
  .change_mode =		tveng1_change_attach_mode,

  .ov511_get_button_state =	ov511_get_button_state,

  .interface_label		= "Video4Linux",

  .private_size =		sizeof(struct private_tveng1_device_info)
};

/*
  Inits the V4L1 module, and fills in the given table.
*/
void tveng1_init_module(struct tveng_module_info *module_info)
{
  assert (NULL != module_info);

  memcpy(module_info, &tveng1_module_info,
	 sizeof(struct tveng_module_info)); 
}

#else /* !ENABLE_V4L */

#include "tveng1.h"

void tveng1_init_module(struct tveng_module_info *module_info)
{
  assert (NULL != module_info);

  CLEAR (*module_info);
}

#endif /* ENABLE_V4L */

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
