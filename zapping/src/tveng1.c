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
 *  Kernel interface
 */
#include "common/videodev.h"
#include "common/_videodev.h"

#define BTTV_VERSION _IOR ('v' , BASE_VIDIOCPRIVATE + 6, int)
static __inline__ void IOCTL_ARG_TYPE_CHECK_BTTV_VERSION
	(int *arg __attribute__ ((unused))) {}

/* This macro checks the ioctl argument type at compile time. It
   executes the ioctl, when it fails stores an error message
   containing the ioctl name, and returns the ioctl() return value
   and errno code. */
#define v4l_ioctl(info, cmd, arg)					\
(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
 ((0 == device_ioctl ((info)->log_fp, fprint_ioctl_arg,			\
		      (info)->fd, cmd, (void *)(arg))) ?		\
  0 : (ioctl_failure (info, __FILE__, __PRETTY_FUNCTION__,		\
		      __LINE__, # cmd), -1)))

#define v4l_ioctl_nf(info, cmd, arg)					\
(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
 device_ioctl ((info)->log_fp, fprint_ioctl_arg,			\
		      (info)->fd, cmd, (void *)(arg)))

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

/*
 *  Private control IDs. In v4l the control concept doesn't exist.
 */
typedef enum {
	CONTROL_BRIGHTNESS	= (1 << 0),
	CONTROL_CONTRAST	= (1 << 1),
	CONTROL_COLOUR		= (1 << 2),
	CONTROL_HUE		= (1 << 3),
	CONTROL_MUTE		= (1 << 16),
	CONTROL_VOLUME		= (1 << 17),
	CONTROL_BASS		= (1 << 18),
	CONTROL_TREBLE		= (1 << 19),
	CONTROL_BALANCE		= (1 << 20),
} control_id;

#define VIDEO_CONTROLS (CONTROL_BRIGHTNESS | CONTROL_CONTRAST |		\
			CONTROL_COLOUR | CONTROL_HUE)

#define AUDIO_CONTROLS (CONTROL_MUTE | CONTROL_VOLUME | CONTROL_BASS |	\
			CONTROL_TREBLE | CONTROL_BALANCE)

#define ALL_CONTROLS (VIDEO_CONTROLS | AUDIO_CONTROLS)

struct control {
	tv_control		pub;
	control_id		id;
};

#define C(l) PARENT (l, struct control, pub)




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
  char * mmaped_data; /* A pointer to the data mmap() returned */
  struct video_mbuf mmbuf; /* Info about the location of the frames */
  int queued, dequeued; /* The index of the [de]queued frames */
  double last_timestamp; /* Timestamp of the last frame captured */

  double capture_time;
  double frame_period_near;
  double frame_period_far;

  uint32_t chroma; /* Pixel value for the chromakey */
  uint32_t r, g, b; /* 0-65535 components for the chroma */

  /* OV511 camera */
  int ogb_fd;

	control_id		all_controls;

	tv_control *		control_mute;
	tv_control *		control_audio_dec;

	tv_audio_capability	tuner_audio_capability;

	tv_bool			read_back_controls;
	tv_bool			mute_flag_readable;
	tv_bool			audio_mode_reads_rx;
	tv_bool			channel_norm_usable;
	tv_bool			bttv_driver;
};

#define P_INFO(p) PARENT (p, struct private_tveng1_device_info, info)

static tv_bool
get_capture_format	(tveng_device_info * info);


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
pixfmt_to_palette		(tv_pixfmt		pixfmt)
{
	switch (pixfmt) {
	case TV_PIXFMT_Y8:		return VIDEO_PALETTE_GREY;

	case TV_PIXFMT_BGR16_LE:	return VIDEO_PALETTE_RGB565;
	case TV_PIXFMT_BGR24_LE:	return VIDEO_PALETTE_RGB24;
	case TV_PIXFMT_BGRA32_LE:	return VIDEO_PALETTE_RGB32;
	case TV_PIXFMT_BGRA16_LE:	return VIDEO_PALETTE_RGB555;

	/* Synonyms. May have to try both. */
	case TV_PIXFMT_YUYV:		return VIDEO_PALETTE_YUYV;
     /* case TV_PIXFMT_YUYV:		return VIDEO_PALETTE_YUV422; */

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

	if (p_info->info.cur_video_input
	    && IS_TUNER_LINE (p_info->info.cur_video_input)) {
		cap = p_info->tuner_audio_capability;

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
				    p_info->info.audio_callback);
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

	if (p_info->info.audio_reception[0] != rec[0]
	    || p_info->info.audio_reception[1] != rec[1]) {
		p_info->info.audio_reception[0] = rec[0];
		p_info->info.audio_reception[1] = rec[1];

		tv_callback_notify (&p_info->info, &p_info->info,
				    p_info->info.audio_callback);
	}
}

#define SINGLE_BIT(n) ((n) > 0 && ((n) & ((n) - 1)) == 0)

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

	p_info->info.audio_capability	= TV_AUDIO_CAPABILITY_EMPTY;
	p_info->tuner_audio_capability	= TV_AUDIO_CAPABILITY_EMPTY;
	p_info->info.audio_mode		= TV_AUDIO_MODE_UNKNOWN;
	p_info->info.audio_reception[0]	= 0; /* primary language */ 
	p_info->info.audio_reception[1]	= 0; /* secondary language */ 

	p_info->audio_mode_reads_rx	= FALSE;

	/* According to /linux/Documentation/video4linux/API.html
	   audio.mode is the "mode the audio input is in". The bttv driver
	   returns the received audio on read, a set, not the selected
	   mode and not capabilities. On write a single bit must be
	   set, zero selects autodetection. NB VIDIOCSAUDIO is not w/r. */

	CLEAR (audio);

	if (-1 == v4l_ioctl_nf (&p_info->info, VIDIOCGAUDIO, &audio)) {
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

		if (0 == v4l_ioctl_nf (&p_info->info, VIDIOCSAUDIO, &audio)) {
			capability |= mode;

			if ((unsigned int) -1 == fst_mode)
				fst_mode = audio.mode;

			cur_mode = audio.mode;

			if (!p_info->audio_mode_reads_rx) {
				if (-1 == v4l_ioctl (&p_info->info,
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

		if (-1 == v4l_ioctl (&p_info->info, VIDIOCSAUDIO, &audio))
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

	p_info->info.audio_mode = tv_audio_mode_from_v4l_mode (cur_mode);

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

	if (-1 == v4l_ioctl (&p_info->info, VIDIOCGAUDIO, &audio))
		return FALSE;

	if (!p_info->mute_flag_readable) {
		if (p_info->control_mute
		    && p_info->control_mute->value)
			audio.flags |= VIDEO_AUDIO_MUTE;
		else
			audio.flags &= ~VIDEO_AUDIO_MUTE;
	}

	audio.mode = tv_audio_mode_to_v4l_mode (mode);

	if (-1 == v4l_ioctl (info, VIDIOCSAUDIO, &audio))
		return FALSE;

	if (p_info->info.audio_mode != mode) {
		p_info->info.audio_mode = mode;
		tv_callback_notify (&p_info->info, &p_info->info,
				    p_info->info.audio_callback);
	}

	return TRUE;
}

/*
	Controls
*/

static void
update_control_set		(tveng_device_info *	info,
				 struct video_picture *	pict,
				 struct video_audio *	audio,
				 control_id		control_set)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	tv_control *control;

	for_all (control, p_info->info.controls) {
		struct control *c = C(control);
		int value;

		if (c->pub._parent != info)
			continue;

		if (TV_CONTROL_ID_AUDIO_MODE == c->pub.id)
			continue;

		if (0 == (control_set & c->id))
			continue;

		switch (c->id) {
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

	if (control_set & VIDEO_CONTROLS) {
		if (-1 == v4l_ioctl (info, VIDIOCGPICT, &pict))
			goto failure;

		control_set |= VIDEO_CONTROLS;
	}

	if (control_set & AUDIO_CONTROLS) {
		if (-1 == v4l_ioctl (info, VIDIOCGAUDIO, &audio))
			goto failure;

		control_set |= AUDIO_CONTROLS;

		if (p_info->audio_mode_reads_rx) {
			set_audio_reception (p_info, audio.mode);
		}
	}

	update_control_set (&p_info->info, &pict, &audio, control_set);

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

	if (c->id & VIDEO_CONTROLS) {
		struct video_picture pict;

		if (-1 == v4l_ioctl (info, VIDIOCGPICT, &pict))
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

		if (-1 == v4l_ioctl (info, VIDIOCSPICT, &pict))
			goto failure;

		if (p_info->read_back_controls) {
			/* Error ignored */
			v4l_ioctl (info, VIDIOCGPICT, &pict);
		}

		update_control_set (&p_info->info, &pict,
				    NULL, VIDEO_CONTROLS);

	} else if (c->id & AUDIO_CONTROLS) {
		struct video_audio audio;
		unsigned int rx_mode;
		tv_bool no_read;

		if (-1 == v4l_ioctl (info, VIDIOCGAUDIO, &audio))
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
				(p_info->info.audio_mode);
		}

		if (-1 == v4l_ioctl (info, VIDIOCSAUDIO, &audio))
			goto failure;

		if (p_info->read_back_controls) {
			/* Error ignored */
			v4l_ioctl (info, VIDIOCGAUDIO, &audio);
		}

		if (no_read && c->pub.value != value) {
			c->pub.value = value;
			tv_callback_notify (info, &c->pub, c->pub._callback);
		}

		update_control_set (&p_info->info, NULL,
				    &audio, AUDIO_CONTROLS);
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

static tv_bool
get_control_list		(tveng_device_info *	info)
{
	struct private_tveng1_device_info *p_info = P_INFO (info);
	struct video_picture pict;
	struct video_audio audio;
	tv_bool rewrite;

	free_controls (info);

	p_info->all_controls = 0;

	/* The range 0 ... 65535 is mandated by the v4l spec. We add a reset
	   value of 32768 and a step value of 256. V4L does not report the
	   actual reset value and hardware resolution but for a UI these
	   values should do. */
#define ADD_STD_CONTROL(name, label, id, value)				\
	add_control (info, CONTROL_##name, _(label),			\
		     TV_CONTROL_ID_##id, TV_CONTROL_TYPE_INTEGER,	\
		     value, 32768, 0, 65535, 256)

	if (-1 == v4l_ioctl (info, VIDIOCGPICT, &pict))
		return FALSE;

	ADD_STD_CONTROL (BRIGHTNESS,"Brightness", BRIGHTNESS, pict.brightness);
	ADD_STD_CONTROL (CONTRAST,  "Contrast",   CONTRAST,   pict.contrast);
	ADD_STD_CONTROL (COLOUR,    "Saturation", SATURATION, pict.colour);
	ADD_STD_CONTROL (HUE,       "Hue",        HUE,        pict.hue);

	if (-1 == v4l_ioctl_nf (info, VIDIOCGAUDIO, &audio)) {
		if (EINVAL == errno)
			return TRUE;

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
				     0, 0, 1, 1);
	}

	if (audio.flags & VIDEO_AUDIO_VOLUME)
		ADD_STD_CONTROL (VOLUME, "Volume", VOLUME, audio.volume);

	if (audio.flags & VIDEO_AUDIO_BASS)
		ADD_STD_CONTROL (BASS, "Bass", BASS, audio.bass);

	if (audio.flags & VIDEO_AUDIO_TREBLE)
		ADD_STD_CONTROL (TREBLE, "Treble", TREBLE, audio.treble);

#ifdef VIDEO_AUDIO_BALANCE /* In the V4L API, but not present in bttv */
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
		v4l_ioctl (info, VIDIOCSAUDIO, &audio);
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

	for_all (l, info->video_inputs)
		if (l->type == TV_VIDEO_LINE_TYPE_BASEBAND)
			break;
	if (!l)
		return FALSE;

	CLEAR (channel);

	channel.channel = VI (l)->channel;

	if (-1 == v4l_ioctl (info, VIDIOCGCHAN, &channel))
		return FALSE;

	old_norm = channel.norm;
	new_norm = (old_norm == 0); /* PAL -> NTSC, NTSC -> PAL */

	channel.norm = new_norm;

	if (-1 == v4l_ioctl (info, VIDIOCSCHAN, &channel))
		return FALSE;

	if (channel.norm != new_norm)
		return FALSE;

	channel.norm = old_norm;

	return (0 == v4l_ioctl (info, VIDIOCSCHAN, &channel));
}

static tv_bool
get_video_standard		(tveng_device_info *	info)
{
	tv_video_standard *s;
	unsigned int norm;

	if (!info->video_standards) {
		store_cur_video_standard (info, NULL /* unknown */);
		set_audio_capability (P_INFO (info));
		return TRUE;
	}

	if (TVENG_ATTACH_CONTROL == info->attach_mode)
		if (!panel_open (info))
			return FALSE;

	if (info->cur_video_input
	    && P_INFO (info)->channel_norm_usable) {
		struct video_channel channel;

		CLEAR (channel);

		channel.channel = VI(info->cur_video_input)->channel;

		if (-1 == v4l_ioctl (info, VIDIOCGCHAN, &channel))
			goto failure;

		norm = channel.norm;
	} else if (IS_TUNER_LINE (info->cur_video_input)) {
		struct video_tuner tuner;

		CLEAR (tuner);

		tuner.tuner = VI (info->cur_video_input)->tuner;

		if (-1 == v4l_ioctl (info, VIDIOCGTUNER, &tuner))
			goto failure;

		norm = tuner.mode;
	} else {
		struct video_tuner tuner;
		tv_video_line *l;

		/* Apparently some V4L drivers (still?) do not report
		   the video standard used by the current video input
		   unless it has a tuner. We query the first video
		   input with tuner, if any. */

		for_all (l, info->video_inputs)
			if (l->type == TV_VIDEO_LINE_TYPE_TUNER)
				break;

		if (!l) {
			s = NULL; /* unknown */
			goto store;
		}

		CLEAR (tuner);

		tuner.tuner = VI (l)->tuner;

		if (-1 == v4l_ioctl (info, VIDIOCGTUNER, &tuner))
			goto failure;

		norm = tuner.mode;
	}

	for_all (s, info->video_standards)
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
				 const tv_video_standard *s)
{
	capture_mode current_mode;
	gboolean overlay_was_active;
	unsigned int norm;
	int r;

	if (TVENG_ATTACH_CONTROL == info->attach_mode)
		if (!panel_open (info))
			return FALSE;

	norm = CS(s)->norm;

	if (info->cur_video_input
	    && P_INFO(info)->channel_norm_usable) {
		struct video_channel channel;

		CLEAR (channel);

		channel.channel = VI (info->cur_video_input)->channel;

		if (-1 == v4l_ioctl (info, VIDIOCGCHAN, &channel))
			goto failure;

		if (channel.norm == norm)
			goto success;

		/* XXX */
		current_mode = p_tveng_stop_everything(info,
						       &overlay_was_active);

		channel.norm = norm;

		if (0 == (r = v4l_ioctl (info, VIDIOCSCHAN, &channel))) {
			store_cur_video_standard (info, s);
			set_audio_capability (P_INFO (info));
		}
	} else if (IS_TUNER_LINE (info->cur_video_input)) {
		struct video_tuner tuner;

		CLEAR (tuner);

		tuner.tuner = VI (info->cur_video_input)->tuner;

		if (-1 == v4l_ioctl (info, VIDIOCGTUNER, &tuner))
			goto failure;

		if (tuner.mode == norm)
			goto success;

		if (!(tuner.flags & VIDEO_TUNER_NORM)) {
			errno = -1; /* FIXME */
			goto failure; /* not setable */
		}

		/* XXX */
		current_mode = p_tveng_stop_everything(info,
						       &overlay_was_active);

		tuner.mode = norm;

		if (0 == (r = v4l_ioctl (info, VIDIOCSTUNER, &tuner))) {
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

		for_all (l, info->video_inputs)
			if (l->type == TV_VIDEO_LINE_TYPE_TUNER)
				break;

		if (!l) {
			errno = -1; /* FIXME */
			goto failure;
		}

		CLEAR (channel);

		channel.channel = VI (l)->channel;

		if (-1 == v4l_ioctl (info, VIDIOCGCHAN, &channel))
			goto failure;

		if (channel.norm == norm)
			goto success;

		switched = FALSE;

		/* XXX */
		current_mode = p_tveng_stop_everything(info,
						       &overlay_was_active);

		if (-1 == (r = v4l_ioctl (info, VIDIOCSCHAN, &channel)))
			goto finish;

		CLEAR (tuner);

		tuner.tuner = VI (l)->tuner;

		if (-1 == (r = v4l_ioctl (info, VIDIOCGTUNER, &tuner)))
			goto restore;

		if (!(tuner.flags & VIDEO_TUNER_NORM)) {
			r = -1;
			goto restore;
		}

		tuner.mode = norm;

		if (0 == (r = v4l_ioctl (info, VIDIOCSTUNER, &tuner)))
			switched = TRUE;

	restore:
		channel.channel = VI (info->cur_video_input)->channel;

		if (-1 == (r = v4l_ioctl (info, VIDIOCSCHAN, &channel))) {
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
	/* Start capturing again as if nothing had happened */
	/* XXX stop yes, restarting is not our business (eg. frame geometry change). */
	p_tveng_restart_everything(current_mode, overlay_was_active, info);

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

	if (!info->cur_video_input)
		return TRUE;

	if (P_INFO (info)->bttv_driver) {
		if (info->video_standards)
			return TRUE; /* invariable */

		table = bttv_standards;
	} else {
		table = v4l_standards;
	}

	if (IS_TUNER_LINE (info->cur_video_input)) {
		struct video_tuner tuner;

		CLEAR (tuner);

		tuner.tuner = VI (info->cur_video_input)->tuner;

		if (-1 == v4l_ioctl (info, VIDIOCGTUNER, &tuner))
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

		if (!(s = S(append_video_standard (&info->video_standards,
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
	unsigned long freq;

	/* Err. cur_video_input may not be up-to-date, but there
	   is no ioctl to verify. */
	if (info->cur_video_input == l) {
		int r;

		if (TVENG_ATTACH_CONTROL == info->attach_mode) {
			if (!panel_open (info))
				return FALSE;

			r = v4l_ioctl (info, VIDIOCGFREQ, &freq);

			if (!panel_close (info))
				return FALSE;
		} else {
			r = v4l_ioctl (info, VIDIOCGFREQ, &freq);
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
	if (info->cur_video_input != l) {
		store_frequency (info, vi, new_freq);
		return TRUE;
	}

	if (TVENG_ATTACH_CONTROL == info->attach_mode)
		if (!panel_open (info))
			return FALSE;

	if (0 == v4l_ioctl (info, VIDIOCGFREQ, &old_freq))
		if (old_freq == new_freq)
			goto store;

	if (-1 == v4l_ioctl (info, VIDIOCSFREQ, &new_freq)) {
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

		for (i = 0; i < (unsigned int) p_info->mmbuf.frames; ++i) {
			tv_clear_image (p_info->mmaped_data
					+ p_info->mmbuf.offsets[i], 0,
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
set_video_input			(tveng_device_info *	info,
				 const tv_video_line *	l)
{
	struct video_channel channel;
	capture_mode current_mode;
	gboolean overlay_was_active;

	if (TVENG_ATTACH_CONTROL == info->attach_mode)
		if (!panel_open (info))
			return FALSE;

	current_mode = p_tveng_stop_everything (info, &overlay_was_active);

	CLEAR (channel);

	channel.channel = CVI (l)->channel;

	if (-1 == v4l_ioctl (info, VIDIOCGCHAN, &channel))
		goto failure;

	/* There is no ioctl to query the current video input,
	   so unfortunately we cannot take a shortcut. */

	if (-1 == v4l_ioctl (info, VIDIOCSCHAN, &channel))
		goto failure;

	store_cur_video_input (info, l);

	/* Implies get_video_standard() and set_audio_capability(). */
	get_video_standard_list (info);

	/* V4L does not promise per-tuner frequency setting as we do.
	   XXX in panel mode ignores the possibility that a third
	   party changed the frequency from the value we know. */
	if (IS_TUNER_LINE (l)) {
		set_tuner_frequency (info, info->cur_video_input,
				     info->cur_video_input->u.tuner.frequency);
	}

	/* XXX bad idea Start capturing again as if nothing had happened */
	p_tveng_restart_everything (current_mode, overlay_was_active, info);

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

	if (-1 == v4l_ioctl (info, VIDIOCGTUNER, &tuner))
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

	if (-1 == v4l_ioctl (info, VIDIOCGFREQ, &freq))
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

		if (-1 == v4l_ioctl (info, VIDIOCGCHAN, &channel))
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
			       (&info->video_inputs,
				type, buf, buf, sizeof (*vi)))))
			goto failure;

		vi->pub._parent = info;

		vi->channel = i;

		/* FIXME allocate one video_line for each tuner. */
		vi->tuner = 0;

		if (channel.type == VIDEO_TYPE_TV) {
			if (-1 == v4l_ioctl (info, VIDIOCSCHAN, &channel))
				return FALSE;

			if (!tuner_bounds (info, vi))
				goto failure;
		}
	}

	if (info->video_inputs) {
		/* There is no ioctl to query the current video input,
		   we can only reset to a known channel. */
		if (!set_video_input (info, info->video_inputs))
			goto failure;
	}

	return TRUE;

 failure:
	free_video_line_list (&info->video_inputs);
	return FALSE;
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

	if (-1 == v4l_ioctl (info, VIDIOCGFBUF, &buffer))
		goto failure;

	info->overlay.buffer.base = (unsigned long) buffer.base;

	if (!tv_image_format_init (&info->overlay.buffer.format,
				   (unsigned int) buffer.width,
				   (unsigned int) buffer.height,
				   0,
				   pig_depth_to_pixfmt ((unsigned) buffer.depth),
				   TV_COLOR_SPACE_UNKNOWN))
		goto failure;

	assert ((unsigned) buffer.bytesperline
		>= info->overlay.buffer.format.bytes_per_line);

	if ((unsigned) buffer.bytesperline
	    > info->overlay.buffer.format.bytes_per_line) {
		assert (TV_PIXFMT_IS_PACKED
			(info->overlay.buffer.format.pixfmt));

		info->overlay.buffer.format.bytes_per_line =
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

	if (-1 == v4l_ioctl (info, VIDIOCSWIN, &window)) {
		free (clips);
		return FALSE;
	}

	free (clips);

	if (-1 == v4l_ioctl (info, VIDIOCGWIN, &window))
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
  return get_capture_format (info);
}

static tv_bool
enable_overlay			(tveng_device_info *	info,
				 tv_bool		on)
{
	int value = on;

	if (0 == v4l_ioctl (info, VIDIOCCAPTURE, &value)) {
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

  info -> fd = device_open (info->log_fp, info -> file_name, flags, 0);
  if (-1 == info -> fd)
    {
      info->tveng_errno = errno; /* Just to put something other than 0 */
      t_error("open()", info);
      return -1;
    }

  /* We check the capabilities of this video device */
  if (v4l_ioctl(info, VIDIOCGCAP, &caps))
    {
      device_close (0, info -> fd);
      return -1;
    }

  /* Check if this device is convenient for capturing */
  if ( !(caps.type & VID_TYPE_CAPTURE) )
    {
      info->tveng_errno = -1;
      snprintf(info->error, 256, 
	       "%s doesn't look like a valid capture device", info
	       -> file_name);
      device_close(0, info -> fd);
      return -1;
    }

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

  p_info->bttv_driver = FALSE;

#ifdef TVENG1_BTTV_PRESENT
  /* Rather poor, but we shouldn't send a private
     ioctl to innocent drivers */
  if (strstr (info->caps.name, "bt")
      || strstr (info->caps.name, "BT")) {
	  int version;
	  int dummy;

	  version = v4l_ioctl_nf (info, BTTV_VERSION, &dummy);

	  if (version != -1)
		  p_info->bttv_driver = TRUE;
  }
#endif

  /* This tries to fill the fb_info field */
  get_overlay_buffer (info);

  /* Set some flags for this device */
  fcntl( info -> fd, F_SETFD, FD_CLOEXEC );

  /* Ignore the alarm signal */
  signal(SIGALRM, SIG_IGN);

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
tveng1_describe_controller(const char ** short_str, const char ** long_str,
			   tveng_device_info * info)
{
  t_assert(info != NULL);
  if (short_str)
    *short_str = "V4L1";
  if (long_str)
    *long_str = "Video4Linux 1";
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

  if (info->debug_level > 0)
    fprintf(stderr, "\nTVeng: V4L1 controller unloaded\n");
}

static int
tveng1_ioctl			(tveng_device_info *	info,
				 unsigned int		cmd,
				 char *			arg)
{
	return device_ioctl (info->log_fp, fprint_ioctl_arg,
			     info->fd, cmd, arg);
}










/* Updates the current capture format info. */
static tv_bool
get_capture_format	(tveng_device_info * info)
{
	struct video_picture pict;
	struct video_window window;

	CLEAR (window);

	if (-1 == v4l_ioctl (info, VIDIOCGPICT, &pict))
		return FALSE;

	if (-1 == v4l_ioctl (info, VIDIOCGWIN, &window))
		return FALSE;

	if (!tv_image_format_init (&info->capture.format,
				   window.width,
				   window.height,
				   0,
				   palette_to_pixfmt (pict.palette),
				   0)) {
		info->tveng_errno = -1; /* unknown */
		t_error_msg("switch()",
			    "Cannot understand the palette", info);
		return FALSE;
	}

	info->overlay.window.x = window.x;
	info->overlay.window.y = window.y;
	info->overlay.window.width = window.width;
	info->overlay.window.height = window.height;

	return TRUE;
}

static tv_bool
set_capture_format		(tveng_device_info *	info,
				 const tv_image_format *fmt)
{
	struct video_picture pict;
	struct video_window window;
	int r;

	CLEAR (pict);
	CLEAR (window);

	if (-1 == v4l_ioctl (info, VIDIOCGPICT, &pict))
		return FALSE;

	pict.palette = pixfmt_to_palette (fmt->pixfmt);
  
	if (0 == pict.palette) {
		info->tveng_errno = EINVAL;
		tv_error_msg (info, "%s not supported",
			      tv_pixfmt_name (fmt->pixfmt));
		return FALSE;
	}

	/* Set this values for the picture properties */
	r = v4l_ioctl (info, VIDIOCSPICT, &pict);

	if (-1 == r
	    && EINVAL == errno 
	    && VIDEO_PALETTE_YUYV == pict.palette) {
		/* These are synonyms, some drivers understand only one. */
		pict.palette = VIDEO_PALETTE_YUV422;

		r = v4l_ioctl (info, VIDIOCSPICT, &pict);
	}

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

	window.width = info->capture.format.width;
	window.height = info->capture.format.height;
	window.clips = NULL;
	window.clipcount = 0;

	/* Ok, now set the video window dimensions */
	if (-1 == v4l_ioctl (info, VIDIOCSWIN, &window))
		return FALSE;

	/* Actual image size. */

	if (!get_capture_format (info))
		return FALSE;

	return TRUE;
}




/*
  Gets the signal strength and the afc code. The afc code indicates
  how to get a better signal, if negative, tune higher, if negative,
  tune lower. 0 means no idea of feature not present in the current
  controller (i.e. V4L1). Strength and/or afc can be NULL pointers,
  that would mean ignore that parameter.
*/
static int
tveng1_get_signal_strength (int *strength, int * afc,
			   tveng_device_info * info)
{
  struct video_tuner tuner;

  t_assert(info != NULL);
  t_assert(info->cur_video_input != NULL);

  /* Check that there are tuners in the current input */
  if (!IS_TUNER_LINE (info->cur_video_input))
    return -1;

  CLEAR (tuner);

  /* Get info about the current tuner (usually the 0 tuner) */
  tuner.tuner = 0;
  if (v4l_ioctl(info, VIDIOCGTUNER, &tuner))
    goto failure;

  if (strength)
    *strength = tuner.signal;

  if (afc)
    *afc = 0; /* No such thing in the V4L1 spec */

  return 0; /* Success */

 failure:
  return -1;
}



/* Two internal functions, both return -1 on error */
static int p_tveng1_queue(tveng_device_info * info);
static int p_tveng1_dequeue(tveng_image_data * where,
			    tveng_device_info * info);

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

  /* Make the pointer a invalid pointer */
  p_info -> mmaped_data = (char*) -1;

  /* 
     When this function is called, the desired capture format should
     have been set.
  */
  if (v4l_ioctl(info, VIDIOCGMBUF, &(p_info->mmbuf)))
      return -1;

  t_assert (p_info->mmbuf.frames > 0);

  /* NB PROT_WRITE required since bttv 0.8,
     client may write mmapped buffers too. */
  p_info->mmaped_data = (char *) mmap (0, (unsigned int) p_info->mmbuf.size,
				       PROT_READ | PROT_WRITE,
				       MAP_SHARED, info->fd, 0);

  if (p_info->mmaped_data == (char *) -1)
    p_info->mmaped_data = (char *) mmap (0, (unsigned int) p_info->mmbuf.size,
					 PROT_READ, MAP_SHARED,
					 info->fd, 0);

  if (p_info->mmaped_data == (char *) -1)
    {
      info->tveng_errno = errno;
      t_error("mmap()", info);
      return -1;
    }

  p_info -> queued = p_info -> dequeued = 0;

  info->capture_mode = CAPTURE_MODE_READ;

  /* Queue first buffer */
  if (p_tveng1_queue(info) == -1)
    return -1;

  return 0;
}

/* Tries to stop capturing. -1 on error. */
static int
tveng1_stop_capturing(tveng_device_info * info)
{
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;

  if (info -> capture_mode == CAPTURE_MODE_NONE)
    {
      fprintf(stderr, 
	      "Warning: trying to stop capture with no capture active\n");
      return 0; /* Nothing to be done */
    }
  t_assert(info->capture_mode == CAPTURE_MODE_READ);

  /* Dequeue last buffer */
  p_tveng1_dequeue(NULL, info);

  if (p_info -> mmaped_data != ((char*)-1))
    if (munmap(p_info->mmaped_data, (unsigned int) p_info->mmbuf.size) == -1)
      {
	info -> tveng_errno = errno;
	t_error("munmap()", info);
      }

  info->capture_mode = CAPTURE_MODE_NONE;

  return 0;
}

static int p_tveng1_queue(tveng_device_info * info)
{
  struct video_mmap bm;
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;

  t_assert(info != NULL);
  t_assert(info -> capture_mode == CAPTURE_MODE_READ);

  CLEAR (bm);
  
  /* XXX we may have to address YUYV, YUV422 synonyms. */
  bm.format = pixfmt_to_palette (info->capture.format.pixfmt);
  
  if (0 == bm.format) {
      info -> tveng_errno = -1;
      t_error_msg("switch()", "Cannot understand actual palette",
		  info);
      return -1;
    }

  bm.frame = (p_info -> queued) % p_info->mmbuf.frames;
  bm.width = info -> capture.format.width;
  bm.height = info -> capture.format.height;

  if (v4l_ioctl(info, VIDIOCMCAPTURE, &bm) == -1)
    {
      /* This comes from xawtv, it isn't in the V4L API */
      if (errno == EAGAIN)
	t_error_msg("VIDIOCMCAPTURE", 
		    "Grabber chip can't sync (no station tuned in?)",
		    info);
      return -1;
    }

  /* increase the queued index */
  p_info -> queued ++;

  return 0; /* Success */
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
  double rate = info->cur_video_standard ?
	  info->cur_video_standard->frame_rate : 25; /* XXX*/

  p_info->capture_time = 0.0;
  p_info->frame_period_near = p_info->frame_period_far = 1.0 / rate;
}

static int p_tveng1_dequeue(tveng_image_data * where,
			    tveng_device_info * info)
{
  struct private_tveng1_device_info *p_info = P_INFO (info);
  int frame;

  t_assert(info != NULL);
  t_assert(info -> capture_mode == CAPTURE_MODE_READ);

  if (p_info -> dequeued == p_info -> queued)
    return 0; /* All queued frames have been dequeued */

  frame = p_info->dequeued % p_info->mmbuf.frames;

  while (-1 == v4l_ioctl(info, VIDIOCSYNC, &frame))
    {
      if (errno == EINTR)
	continue;

      return -1;
    }

  p_info->last_timestamp = p_tveng1_timestamp(p_info);

  /* Copy the mmaped data to the data struct, if it is not null */
  if (where)
    tveng_copy_frame (p_info->mmaped_data +
		      p_info->mmbuf.offsets[frame], where, info);

  p_info -> dequeued ++;

  return 0;
}

/* 
   Reads a frame from the video device, storing the read data in
   the location pointed to by where.
   time: time to wait using select() in miliseconds
   info: pointer to the video device info structure
   This call was originally intended to wrap a single read() call, but
   since i cannot get it to work, now encapsulates the dqbuf/qbuf
   logic.
   Returns -1 on error, anything else on success
*/
static
int tveng1_read_frame(tveng_image_data *where, 
		      unsigned int time, tveng_device_info * info)
{
  struct itimerval iv;

  t_assert(info != NULL);

  if (info -> capture_mode != CAPTURE_MODE_READ)
    {
      info -> tveng_errno = -1;
      t_error_msg("check", "Current capture mode is not READ",
		  info);
      return -1;
    }

  /* This should be inmediate */
  if (p_tveng1_queue(info) == -1)
    return -1;

  /* Dequeue previously queued frame */
  /* Sets the timer to expire (SIGALARM) in the given time */
  iv.it_interval.tv_sec = 0;
  iv.it_interval.tv_usec = 0;
  iv.it_value.tv_sec = 0;
  iv.it_value.tv_usec = time;

  if (setitimer(ITIMER_REAL, &iv, NULL) == -1)
    {
      info->tveng_errno = errno;
      t_error("setitimer()", info);
      return -1;
    }

  if (p_tveng1_dequeue(where, info) == -1)
    return -1;

  /* Everything has been OK, return 0 (success) */
  return 0;
}

static double tveng1_get_timestamp(tveng_device_info * info)
{
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info *) info;

  t_assert(info != NULL);
  if (info->capture_mode != CAPTURE_MODE_READ)
    return -1;

  return (p_info -> last_timestamp);
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

  /* We have a valid device, get some info about it */

	/* Video inputs & standards */

	info->video_inputs = NULL;
	info->cur_video_input = NULL;

	info->video_standards = NULL;
	info->cur_video_standard = NULL;

	/* XXX error */
	get_video_input_list (info);

	if (!init_audio (P_INFO (info)))
		return -1;

  /* Query present controls */
  info->controls = NULL;
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
  info->capture.start_capturing = tveng1_start_capturing;
  info->capture.stop_capturing = tveng1_stop_capturing;
  info->capture.read_frame = tveng1_read_frame;
  info->capture.get_timestamp = tveng1_get_timestamp;

  CLEAR (info->overlay);

  info->overlay.get_buffer           = get_overlay_buffer;
  info->overlay.set_window_clipvec   = set_overlay_window_clipvec;
  info->overlay.get_window           = get_overlay_window;
  info->overlay.set_window_chromakey = set_overlay_window_chromakey;
  info->overlay.get_chromakey        = get_overlay_chromakey;
  info->overlay.enable               = enable_overlay;


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
    t_error_msg("switch()", 
		"Cannot find appropiate palette for current display",
		info);
    tveng1_close_device(info);
    return -1;
  }

  set_capture_format (info, &info->capture.format);

  /* init the private struct */
  p_info->mmaped_data = NULL;
  p_info->queued = p_info->dequeued = 0;

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
  .describe_controller =	tveng1_describe_controller,
  .close_device =		tveng1_close_device,
  .ioctl =			tveng1_ioctl,

  .set_video_input		= set_video_input,
  .set_tuner_frequency		= set_tuner_frequency,
  .get_tuner_frequency		= get_tuner_frequency,
  .set_video_standard		= set_video_standard,
  .get_video_standard		= get_video_standard,
  .set_control			= set_control,
  .get_control			= get_control,
  .set_audio_mode		= set_audio_mode,

  .get_signal_strength		= tveng1_get_signal_strength,

  .ov511_get_button_state =	ov511_get_button_state,

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
