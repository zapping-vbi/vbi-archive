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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef ENABLE_BKTR

#include <sys/time.h>		/* struct timeval, gettimeofday() */
#include <signal.h>
#include <math.h>
#include <assert.h>
#include "tvengbktr.h"
#include "zmisc.h"
#include "tveng_private.h"
#include "common/fifo.h"
#include "common/ioctl_meteor.h"
#include "common/ioctl_bt848.h"
#include "common/_bktr.h"

struct private_tvengbktr_device_info
{
	tveng_device_info	info; /* Info field, inherited */

	tv_bool			bktr_driver;

	int			tuner_fd;

	tv_control *		mute_control;

	/* Maps tv_pixfmt to METEORSACTPIXFMT index, -1 if none. */
	int			pixfmt_lut[TV_MAX_PIXFMTS];

	unsigned int		ovl_bits_per_pixel;

        tv_bool                 ovl_ready;
	tv_bool			ovl_active;

	tv_bool			cap_ready;
	tv_bool			cap_active;

	/* mmap()ed image buffer. */ 
	char *			mmapped_data;

  double capture_time;
  double frame_period_near;
  double frame_period_far;

	double last_timestamp;
};

#define P_INFO(p) PARENT (p, struct private_tvengbktr_device_info, info)

/* This macro checks at compile time if the arg type is correct,
   device_ioctl() repeats the ioctl if interrupted (EINTR) and logs
   the args and result if log_fp is non-zero. When the ioctl failed
   ioctl_failure() stores the cmd, caller and errno in info. */
#define bktr_ioctl(info, cmd, arg)					\
	(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),				\
	 ((0 == device_ioctl ((info)->log_fp, fprint_ioctl_arg,		\
			      (info)->fd, cmd, (void *)(arg))) ?	\
	  0 : (ioctl_failure (info, __FILE__, __PRETTY_FUNCTION__,	\
			      __LINE__, # cmd), -1)))

#define bktr_ioctl_may_fail(info, cmd, arg)				\
	(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),				\
	 device_ioctl ((info)->log_fp, fprint_ioctl_arg,		\
		       (info)->fd, cmd, (void *)(arg)))

#define tuner_ioctl(info, cmd, arg)					\
	(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),				\
	 ((0 == device_ioctl ((info)->log_fp, fprint_ioctl_arg,		\
			      P_INFO (info)->tuner_fd, cmd,		\
			      (void *)(arg))) ?				\
	  0 : (ioctl_failure (info, __FILE__, __PRETTY_FUNCTION__,	\
			      __LINE__, # cmd), -1)))

/* Private control IDs. */
typedef enum {
	CONTROL_BRIGHTNESS	= (1 << 0),
	CONTROL_CONTRAST	= (1 << 1),
	CONTROL_UV_SATURATION	= (1 << 2),
	CONTROL_U_SATURATION	= (1 << 3),
	CONTROL_V_SATURATION	= (1 << 4),
	CONTROL_UV_GAIN		= (1 << 5),
	CONTROL_HUE		= (1 << 6),
	CONTROL_MUTE		= (1 << 7),
} control_id;

struct control {
	tv_control		pub;
	control_id		id;
};

#define C(l) PARENT (l, struct control, pub)

struct standard {
	tv_video_standard	pub;
	unsigned long		fmt;
};

#define S(l) PARENT (l, struct standard, pub)

struct video_input {
	tv_video_line		pub;
	unsigned long		dev;
};

#define VI(l) PARENT (l, struct video_input, pub)

struct audio_input {
	tv_audio_line		pub;
	int			dev;
};

#define AI(l) PARENT (l, struct audio_input, pub)

static const struct meteor_video	no_video;
static const struct _bktr_clip		no_clips;

static const int cap_stop_cont		= METEOR_CAP_STOP_CONT;
static const int cap_continuous		= METEOR_CAP_CONTINOUS;

/* NB METEOR_SIG_MODE_MASK must be < 0 but is defined as uint32. */
static const int signal_usr1		= SIGUSR1;
static const int signal_none		= ~0xFFFF; /* METEOR_SIG_MODE_MASK */

static tv_bool
set_capture_format		(tveng_device_info *	info,
				 const tv_image_format *fmt);

/*
	Controls
*/

static tv_bool
do_get_control			(struct private_tvengbktr_device_info * p_info,
				 struct control *	c)
{
	signed char sc;
	unsigned char uc;
	int value;
	int cmd;
	int r;

	cmd = 0;

	if (p_info->bktr_driver) {
		switch (c->id) {
		case CONTROL_BRIGHTNESS:
			r = tuner_ioctl (&p_info->info, BT848_GBRIG, &value);
			break;

		case CONTROL_CONTRAST:
			r = tuner_ioctl (&p_info->info, BT848_GCONT, &value);
			break;

		case CONTROL_UV_SATURATION:
			r = tuner_ioctl (&p_info->info, BT848_GCSAT, &value);
			break;

		case CONTROL_U_SATURATION:
			r = tuner_ioctl (&p_info->info, BT848_GUSAT, &value);
			break;

		case CONTROL_V_SATURATION:
			r = tuner_ioctl (&p_info->info, BT848_GVSAT, &value);
			break;

		case CONTROL_HUE:
			r = tuner_ioctl (&p_info->info, BT848_GHUE, &value);
			break;

		case CONTROL_MUTE:
			r = tuner_ioctl (&p_info->info, BT848_GAUDIO, &value);
			value = !!(value & 0x80 /* mute flag */);
			break;

		default:
			t_warn ("Invalid c->id 0x%x\n", c->id);
			p_info->info.tveng_errno = -1; /* unknown */
			return FALSE;
		}
	} else {
		switch (c->id) {
		case CONTROL_BRIGHTNESS:
			r = bktr_ioctl (&p_info->info, METEORGBRIG, &uc);
			value = uc;
			break;

		case CONTROL_CONTRAST:
			r = bktr_ioctl (&p_info->info, METEORGCONT, &uc);
			value = uc;
			break;

		case CONTROL_UV_SATURATION:
			r = bktr_ioctl (&p_info->info, METEORGCSAT, &uc);
			value = uc;
			break;

		case CONTROL_UV_GAIN:
			r = bktr_ioctl (&p_info->info, METEORGCHCV, &uc);
			value = uc;
			break;

		case CONTROL_HUE:
			r = bktr_ioctl (&p_info->info, METEORGHUE, &sc);
			value = sc;
			break;

		default:
			t_warn ("Invalid c->id 0x%x\n", c->id);
			p_info->info.tveng_errno = -1; /* unknown */
			return FALSE;
		}
	}

	if (-1 == r)
		return FALSE;

	if (c->pub.value != value) {
		c->pub.value = value;
		tv_callback_notify (NULL, &c->pub, c->pub._callback);
	}

	return TRUE;
}

static tv_bool
get_control			(tveng_device_info *	info,
				 tv_control *		c)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);

	if (c)
		return do_get_control (p_info, C(c));

	for_all (c, p_info->info.panel.controls)
		if (c->_parent == info)
			if (!do_get_control (p_info, C(c)))
				return FALSE;

	return TRUE;
}

static int
set_mute			(struct private_tvengbktr_device_info *p_info,
				 int			value)
{
	value = value ? AUDIO_MUTE : AUDIO_UNMUTE;

	return tuner_ioctl (&p_info->info, BT848_SAUDIO, &value);
}

static tv_bool
set_control			(tveng_device_info *	info,
				 tv_control *		c,
				 int			value)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);
	signed char sc;
	unsigned char uc;
	int r;

	if (p_info->bktr_driver) {
		/* Range of the bt848 controls is defined by
		   BT848_.*(MIN|MAX|CENTER|RANGE) floats in some technical
		   units and BT848_.*(REGMIN|REGMAX|STEPS) ints for the
		   hardware. The ioctls naturally take the latter values.
		   Only /dev/tuner understands these ioctls, not /dev/bktr. */
		switch (C(c)->id) {
		case CONTROL_BRIGHTNESS:
			r = tuner_ioctl (&p_info->info, BT848_SBRIG, &value);
			break;

		case CONTROL_CONTRAST:
			r = tuner_ioctl (&p_info->info, BT848_SCONT, &value);
			break;

		case CONTROL_UV_SATURATION:
			r = tuner_ioctl (&p_info->info, BT848_SCSAT, &value);
			break;

		case CONTROL_U_SATURATION:
			r = tuner_ioctl (&p_info->info, BT848_SUSAT, &value);
			break;

		case CONTROL_V_SATURATION:
			r = tuner_ioctl (&p_info->info, BT848_SVSAT, &value);
			break;

		case CONTROL_HUE:
			r = tuner_ioctl (&p_info->info, BT848_SHUE, &value);
			break;

		case CONTROL_MUTE:
			r = set_mute (p_info, value);
			break;

		default:
			t_warn ("Invalid c->id 0x%x\n", C(c)->id);
			p_info->info.tveng_errno = -1; /* unknown */
			return FALSE;
		}
	} else {
		switch (C(c)->id) {
		case CONTROL_BRIGHTNESS:
			uc = value; /* range 0 ... 255 */
			r = bktr_ioctl (&p_info->info, METEORSBRIG, &uc);
			break;

		case CONTROL_CONTRAST:
			uc = value;
			r = bktr_ioctl (&p_info->info, METEORSCONT, &uc);
			break;

		case CONTROL_UV_SATURATION:
			uc = value;
			r = bktr_ioctl (&p_info->info, METEORSCSAT, &uc);
			break;

		case CONTROL_UV_GAIN:
			uc = value;
			r = bktr_ioctl (&p_info->info, METEORSCHCV, &uc);
			break;

		case CONTROL_HUE:
			sc = value; /* range -128 ... +127 */
			r = bktr_ioctl (&p_info->info, METEORSHUE, &sc);
			break;

		default:
			t_warn ("Invalid c->id 0x%x\n", C(c)->id);
			p_info->info.tveng_errno = -1; /* unknown */
			return FALSE;
		}
	}

	if (-1 == r)
		return FALSE;

	if (c->value != value) {
		c->value = value;
		tv_callback_notify (NULL, &c, c->_callback);
	}

	return TRUE;
}

struct control_map {
	control_id		priv_id;
	const char *		label;
	tv_control_id		tc_id;
	int			minimum;
	int			maximum;
	int			reset;
};

#define CONTROL_MAP_END { 0, NULL, 0, 0, 0, 0 }

static const struct control_map
meteor_controls [] = {
	{ CONTROL_BRIGHTNESS,	 N_("Brightness"), TV_CONTROL_ID_BRIGHTNESS,
	  0, 255, 128 },
	{ CONTROL_CONTRAST,	 N_("Contrast"),   TV_CONTROL_ID_CONTRAST,
	  0, 255, 128 },
	{ CONTROL_UV_SATURATION, N_("Saturation"), TV_CONTROL_ID_SATURATION,
	  0, 255, 128 },
#if 0
	{ CONTROL_UV_GAIN,	 N_("U/V Gain"),   TV_CONTROL_ID_UNKNOWN,
	  0, 255, 128 },
#endif
	{ CONTROL_HUE,		 N_("Hue"),        TV_CONTROL_ID_HUE,
	  -128, 127, 0 },
	CONTROL_MAP_END
};

static const struct control_map
bktr_controls [] = {
	{ CONTROL_BRIGHTNESS,	 N_("Brightness"),   TV_CONTROL_ID_BRIGHTNESS,
	  -128, 127, 0 },
	{ CONTROL_CONTRAST,	 N_("Contrast"),     TV_CONTROL_ID_CONTRAST,
	  0, 511, 216 },
	{ CONTROL_UV_SATURATION, N_("Saturation"),   TV_CONTROL_ID_SATURATION,
	  0, 511, 216 },
#if 0
	{ CONTROL_U_SATURATION,	 N_("U Saturation"), TV_CONTROL_ID_UNKNOWN,
	  0, 511, 256 },
	{ CONTROL_V_SATURATION,  N_("V Saturation"), TV_CONTROL_ID_UNKNOWN,
	  0, 511, 180 },
#endif
	{ CONTROL_HUE,		 N_("Hue"),          TV_CONTROL_ID_HUE,
	  -128, 127, 0 },
	{ CONTROL_MUTE,		 N_("Mute"),         TV_CONTROL_ID_MUTE,
	  0, 1, 1 },
	CONTROL_MAP_END
};

static tv_bool
get_control_list		(tveng_device_info *	info)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);
	const struct control_map *table;
	struct control *c;
	tv_control *tc;

	if (p_info->bktr_driver)
		table = bktr_controls;
	else
		table = meteor_controls;

	for (; 0 != table->priv_id; ++table) {
		if (!(c = calloc (1, sizeof (*c))))
			return FALSE;

		c->id		= table->priv_id;

		c->pub.type	= TV_CONTROL_TYPE_INTEGER;
		c->pub.id	= table->tc_id;

		if (!(c->pub.label = strdup (_(table->label))))
			goto failure;

		c->pub.minimum	= table->minimum;
		c->pub.maximum	= table->maximum;
		c->pub.step	= 1;
		c->pub.reset	= table->reset;

		if (!(tc = append_control (info, &c->pub, 0))) {
		failure:
			free_control (&c->pub);
			return FALSE;
		}

		do_get_control (p_info, C(tc));

		if (CONTROL_MUTE == table->priv_id)
			p_info->mute_control = &c->pub;
	}

	return TRUE;
}

/*
	Video standards
*/

static tv_bool
get_video_standard		(tveng_device_info *	info)
{
	tv_video_standard *s;

	s = NULL; /* unknown */

	if (info->panel.video_standards) {
		unsigned long fmt;

		if (P_INFO (info)->bktr_driver) {
			if (-1 == bktr_ioctl (info, BT848GFMT, &fmt))
				return FALSE;
		} else {
			if (-1 == bktr_ioctl (info, METEORGFMT, &fmt))
				return FALSE;
		}

		for (s = info->panel.video_standards; s; s = s->_next)
			if (S(s)->fmt == fmt)
				break;
	}

	store_cur_video_standard (info, s);

	return TRUE;
}

static tv_bool
set_video_standard		(tveng_device_info *	info,
				 tv_video_standard *	s)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);
	capture_mode current_mode;
	gboolean was_active;
	int r;

	if (p_info->ovl_active | p_info->cap_active)
		return FALSE;

	if (p_info->bktr_driver) {
		r = bktr_ioctl (info, BT848SFMT, &S(s)->fmt);
	} else {
		r = bktr_ioctl (info, METEORSFMT, &S(s)->fmt);
	}

	if (0 == r) {
		store_cur_video_standard (info, s);
		return TRUE;
	} else {
		return FALSE;
	}
}

struct standard_map {
	unsigned int		fmt;
	const char *		label;
	tv_videostd_set         videostd_set;
};

#define STANDARD_MAP_END { 0, NULL, 0 }

static const struct standard_map
meteor_standards [] = {
	/* XXX should investigate what exactly these videostandards are. */
	{ METEOR_FMT_PAL,		"PAL",	 TV_VIDEOSTD_SET_PAL },
	{ METEOR_FMT_NTSC,		"NTSC",	 TV_VIDEOSTD_SET_NTSC },
	{ METEOR_FMT_SECAM,		"SECAM", TV_VIDEOSTD_SET_SECAM },
	STANDARD_MAP_END
};

static const struct standard_map
bktr_standards [] = {
	{ BT848_IFORM_F_PALBDGHI,	"PAL",	   TV_VIDEOSTD_SET_PAL },
	{ BT848_IFORM_F_NTSCM,		"NTSC",
	  TV_VIDEOSTD_SET (TV_VIDEOSTD_NTSC_M) },
	{ BT848_IFORM_F_SECAM,		"SECAM",   TV_VIDEOSTD_SET_SECAM },
	{ BT848_IFORM_F_PALM,		"PAL-M",
	  TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_M) },
	{ BT848_IFORM_F_PALN,		"PAL-N",
	  TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_N) },
	{ BT848_IFORM_F_NTSCJ,		"NTSC-JP",
	  TV_VIDEOSTD_SET (TV_VIDEOSTD_NTSC_M_JP) },
#if 0
	{ BT848_IFORM_F_AUTO,		"AUTO",	   TV_VIDEOSTD_SET_UNKNOWN },
	{ BT848_IFORM_F_RSVD,		"RSVD",	   TV_VIDEOSTD_SET_UNKNOWN },
#endif
	STANDARD_MAP_END
};

static tv_bool
get_standard_list		(tveng_device_info *	info)
{
	const struct standard_map *table;

	if (info->panel.video_standards)
		return TRUE; /* invariable */

	if (P_INFO (info)->bktr_driver)
		table = bktr_standards;
	else
		table = meteor_standards;

	for (; table->label; ++table) {
		struct standard *s;

		if (!(s = S(append_video_standard
			    (&info->panel.video_standards,
			     table->videostd_set,
			     table->label,
			     table->label,
			     sizeof (*s))))) {
			free_video_standard_list
				(&info->panel.video_standards);
			return FALSE;
		}

		s->fmt = table->fmt;
	}

	return get_video_standard (info);
}

/*
	Video inputs
*/

/* Note the bktr interface has the ioctls
   TVTUNER_(GET|SET)TYPE (unsigned int) to select a frequency
     table from the CHNLSET_.* enum, and
   TVTUNER_(GET|SET)CHNL (unsigned int) to select a channel number.
   We have our own tables, so we don't use them. */

#define FREQ_UNIT 62500 /* Hz */

static tv_bool
get_video_input			(tveng_device_info *	info);

static void
store_frequency			(struct video_input *	vi,
				 unsigned int		freq)
{
	unsigned int frequency = freq * FREQ_UNIT;

	if (vi->pub.u.tuner.frequency != frequency) {
		vi->pub.u.tuner.frequency = frequency;
		tv_callback_notify (NULL, &vi->pub, vi->pub._callback);
	}
}

static tv_bool
get_tuner_frequency		(tveng_device_info *	info,
				 tv_video_line *	l)
{
	unsigned int freq;

	if (!get_video_input (info))
		return FALSE;

	/* XXX should we look up the only tuner input instead? */
	if (info->panel.cur_video_input == l) {
		if (-1 == tuner_ioctl (info, TVTUNER_GETFREQ, &freq))
			return FALSE;

		store_frequency (VI(l), freq);
	}

	return TRUE;
}

static tv_bool
set_tuner_frequency		(tveng_device_info *	info,
				 tv_video_line *	l,
				 unsigned int		frequency)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);
	struct video_input *vi = VI(l);
	unsigned int freq;

	freq = (frequency + (FREQ_UNIT >> 1)) / FREQ_UNIT;

	if (-1 == tuner_ioctl (&p_info->info, TVTUNER_SETFREQ, &freq))
		return FALSE;

	/* Bug: driver mutes on frequency change (according to fxtv). */
	if (p_info->mute_control
	    && 0 == p_info->mute_control->value) {
		/* Error ignored. */
		set_mute (p_info, 0);
	}

	store_frequency (vi, freq);

	return TRUE;
}

static int
get_signal_strength		(tveng_device_info *	info,
				 int *			strength,
				 int *			afc)
{
/*  unsigned int status; */
/*  unsigned short status2; */
	unsigned int status3;

	if (!strength)
		return TRUE;

/*
  if (-1 == tuner_ioctl (info, TVTUNER_GETSTATUS, &status))
    return -1;
  if (-1 == bktr_ioctl (info, METEORSTATUS, &status2))
    return -1;
*/
	if (-1 == bktr_ioctl (info, BT848_GSTATUS, &status3))
		return FALSE;

	/* Presumably we should query the tuner, but
	   what do we know about the tuner? */
	*strength = (status3 & 0x00005000) ? 0 : 65535;

	return TRUE; /* Success */
}

static tv_bool
get_video_input			(tveng_device_info *	info)
{
	const tv_video_line *l = NULL;

	if (info->panel.video_inputs) {
		unsigned long dev;

		if (-1 == bktr_ioctl (info, METEORGINPUT, &dev))
			return FALSE;

		for_all (l, info->panel.video_inputs)
			if (VI(l)->dev == dev)
				break;
	}

	store_cur_video_input (info, l);

	if (l)
		get_standard_list (info);
	else
		free_video_standards (info);

	return TRUE;
}

static tv_bool
set_video_input			(tveng_device_info *	info,
				 tv_video_line *	l)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);
	capture_mode current_mode;
	gboolean was_active;
	tv_pixfmt pixfmt;

	if (p_info->info.panel.cur_video_input) {
		unsigned long dev;

		if (0 == bktr_ioctl (&p_info->info, METEORGINPUT, &dev))
			if (VI(l)->dev == dev)
				return TRUE;
	}

	if (p_info->ovl_active | p_info->cap_active)
		return FALSE;

	if (-1 == bktr_ioctl (&p_info->info, METEORSINPUT, &VI(l)->dev))
		return FALSE;

	/* Bug: driver mutes on input change (according to fxtv). */
	if (p_info->mute_control
	    && 0 == p_info->mute_control->value) {
		/* Error ignored. */
		set_mute (p_info, 0);
	}

	store_cur_video_input (&p_info->info, l);

	/* Standards are invariable. */
	if (0)
		get_standard_list (&p_info->info);

	/* There is only one tuner, if any.
	   XXX ignores the possibility (?) that a third party changed
	   the frequency from the value we know. */
	if (0 && IS_TUNER_LINE (l))
		set_tuner_frequency (&p_info->info,
				     p_info->info.panel.cur_video_input,
				     p_info->info.panel.cur_video_input
				     ->u.tuner.frequency);

	return TRUE;
}

struct video_input_map {
	unsigned int		dev;
	const char *		label;
	tv_video_line_type	type;
};

#define VIDEO_INPUT_MAP_END { 0, NULL, 0 }

static const struct video_input_map
meteor_video_inputs [] = {
	{ METEOR_INPUT_DEV_RCA,	   "Composite",	TV_VIDEO_LINE_TYPE_BASEBAND },
	{ METEOR_INPUT_DEV_SVIDEO, "S-Video",   TV_VIDEO_LINE_TYPE_BASEBAND },
	{ METEOR_INPUT_DEV1,	   "Dev1",	TV_VIDEO_LINE_TYPE_BASEBAND },
	{ METEOR_INPUT_DEV2,	   "Dev2",	TV_VIDEO_LINE_TYPE_BASEBAND },
	{ METEOR_INPUT_DEV3,	   "Dev3",	TV_VIDEO_LINE_TYPE_BASEBAND },
	VIDEO_INPUT_MAP_END
};

static const struct video_input_map
bktr_video_inputs [] = {
	{ METEOR_INPUT_DEV1,	   "Television",          TV_VIDEO_LINE_TYPE_TUNER },
	{ METEOR_INPUT_DEV_RCA,	   "Composite",	          TV_VIDEO_LINE_TYPE_BASEBAND },
	{ METEOR_INPUT_DEV_SVIDEO, "S-Video",	          TV_VIDEO_LINE_TYPE_BASEBAND },
	{ METEOR_INPUT_DEV2,	   "Composite (S-Video)", TV_VIDEO_LINE_TYPE_BASEBAND },
	{ METEOR_INPUT_DEV3,	   "Composite 3",         TV_VIDEO_LINE_TYPE_BASEBAND },
	VIDEO_INPUT_MAP_END
};

static tv_bool
get_video_input_list		(tveng_device_info *	info)
{
	const struct video_input_map *table;

	if (info->panel.video_inputs)
		return TRUE; /* invariable */

	if (P_INFO (info)->bktr_driver)
		table = bktr_video_inputs;
	else
		table = meteor_video_inputs;

	for (; table->label; ++table) {
		struct video_input *vi;

		if (!(vi = VI (append_video_line (&info->panel.video_inputs,
						  table->type,
						  table->label,
						  table->label,
						  sizeof (*vi)))))
			goto failure;

		vi->pub._parent = info;

		vi->dev = table->dev;

		if (TV_VIDEO_LINE_TYPE_TUNER == table->type) {
			unsigned int freq;

			/* No ioctl to query bounds, we guess. */

			vi->pub.u.tuner.minimum = 100000000;
			vi->pub.u.tuner.maximum = 900000000;
			vi->pub.u.tuner.step = FREQ_UNIT;

			if (-1 == tuner_ioctl (info, TVTUNER_GETFREQ, &freq))
				goto failure;

			store_frequency (vi, freq);
		}
	}

	if (info->panel.video_inputs) {
		get_video_input (info);
	}

	return TRUE;

 failure:
	free_video_line_list (&info->panel.video_inputs);
	return FALSE;
}

/*
	Audio inputs
*/

static tv_bool
get_audio_input			(tveng_device_info *	info)
{
	const tv_audio_line *l = NULL;

	if (info->panel.audio_inputs) {
		int dev;

		if (-1 == tuner_ioctl (info, BT848_GAUDIO, &dev))
			return FALSE;

		dev &= 0x7F; /* without mute flag */

		for_all (l, info->panel.audio_inputs)
			if (AI(l)->dev == dev)
				break;
	}

	store_cur_audio_input (info, l);

	return TRUE;
}

static tv_bool
set_audio_input			(tveng_device_info *	info,
				 tv_audio_line *	l)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);

	if (-1 == tuner_ioctl (&p_info->info, BT848_SAUDIO, &AI(l)->dev))
		return FALSE;

	store_cur_audio_input (&p_info->info, l);

	return TRUE;
}

struct audio_input_map {
	int			dev;
	const char *		label;
};

#define AUDIO_INPUT_MAP_END { 0, NULL }

static const struct audio_input_map
bktr_audio_inputs [] = {
	{ AUDIO_TUNER,		   "Television" },
	{ AUDIO_INTERN,		   "Intern" },
	{ AUDIO_EXTERN,		   "Extern" },
	AUDIO_INPUT_MAP_END
};

static tv_bool
get_audio_input_list		(tveng_device_info *	info)
{
	const struct audio_input_map *table;

	if (info->panel.audio_inputs)
		return TRUE; /* invariable */

	for (table = bktr_audio_inputs; table->label; ++table) {
		struct audio_input *ai;

		if (!(ai = AI (append_audio_line (&info->panel.audio_inputs,
						  TV_AUDIO_LINE_TYPE_NONE,
						  table->label,
						  table->label,
						  /* minimum */ 0,
						  /* maximum */ 0,
						  /* step */ 0,
						  /* reset */ 0,
						  sizeof (*ai)))))
			goto failure;

		ai->pub._parent = info;

		ai->dev = table->dev;
	}

	if (info->panel.audio_inputs) {
		get_audio_input (info);
	}

	return TRUE;

 failure:
	free_audio_line_list (&info->panel.audio_inputs);
	return FALSE;
}

/*
	Pixel formats
*/

static unsigned int
swap816				(unsigned int		x,
				 tv_bool		swap_8,
				 tv_bool		swap_16)
{
	if (swap_8)
		x = ((x >> 8) & 0x00FF00FF) | ((x & 0x00FF00FF) << 8);

	if (swap_16)
		x = (x >> 16) | ((x & 0xFFFF) << 16);

	return x;
}

static tv_bool
init_pixfmt_lut			(tveng_device_info *	info)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);
	tv_pixfmt_set pixfmt_set;
	unsigned int i;

	memset (&p_info->pixfmt_lut, -1, sizeof (p_info->pixfmt_lut));

	pixfmt_set = 0;

	i = 0;

	for (;;) {
		struct meteor_pixfmt pixfmt;
		tv_pixel_format pf;

		if (i > 300) {
			/* Bug? */
			return FALSE;
		}

		pixfmt.index = i;

		if (-1 == bktr_ioctl_may_fail
		    (info, METEORGSUPPIXFMT, &pixfmt)) {
			if (EINVAL == errno)
				break;

			ioctl_failure (info,
				       __FILE__,
				       __FUNCTION__,
				       __LINE__,
				       "METEORGSUPPIXFMT");

			return FALSE;
		}

		if (0)
			fprintf (stderr, "pixfmt index=%u type=%u Bpp=%u "
				 "mask[]=%08x,%08x,%08x "
				 "swap_bytes=%u swap_shorts=%u\n",
				 pixfmt.index,
				 pixfmt.type,
				 pixfmt.Bpp,
				 (unsigned int) pixfmt.masks[0],
				 (unsigned int) pixfmt.masks[1],
				 (unsigned int) pixfmt.masks[2],
				 pixfmt.swap_bytes,
				 pixfmt.swap_shorts);

		switch (pixfmt.type) {
		case METEOR_PIXTYPE_RGB:
			CLEAR (pf);

			pf.bits_per_pixel = pixfmt.Bpp * 8;

			pf.mask.rgb.r = pixfmt.masks[0];
			pf.mask.rgb.g = pixfmt.masks[1];
			pf.mask.rgb.b = pixfmt.masks[2];

			/* XXX Why is this necessary? */
			pixfmt.swap_bytes ^= 1;
			pixfmt.swap_shorts ^= 1;

			if (2 == pixfmt.Bpp) {
				pf.big_endian = pixfmt.swap_bytes;
			} else if (4 == pixfmt.Bpp) {
				pf.mask.rgb.r = swap816 (pixfmt.masks[0],
							 pixfmt.swap_bytes,
							 pixfmt.swap_shorts);
				pf.mask.rgb.g = swap816 (pixfmt.masks[1],
							 pixfmt.swap_bytes,
							 pixfmt.swap_shorts);
				pf.mask.rgb.b = swap816 (pixfmt.masks[2],
							 pixfmt.swap_bytes,
							 pixfmt.swap_shorts);
			}

			pf.pixfmt = tv_pixel_format_to_pixfmt (&pf);
			if (TV_PIXFMT_UNKNOWN != pf.pixfmt) {
				p_info->pixfmt_lut[pf.pixfmt] = i;
				pixfmt_set |= TV_PIXFMT_SET (pf.pixfmt);
			}

			break;

		case METEOR_PIXTYPE_YUV: /* YUV 4:2:2 planar */
			p_info->pixfmt_lut[TV_PIXFMT_YUV422] = i;
			pixfmt_set |= TV_PIXFMT_SET (TV_PIXFMT_YUV422);
			break;

		case METEOR_PIXTYPE_YUV_PACKED:	/* YUV 4:2:2 packed (UYVY) */
			p_info->pixfmt_lut[TV_PIXFMT_YUYV] = i;
			pixfmt_set |= TV_PIXFMT_SET (TV_PIXFMT_YUYV);
			break;

		case METEOR_PIXTYPE_YUV_12: /* YUV 4:2:0 planar */
			p_info->pixfmt_lut[TV_PIXFMT_YUV420] = i;
			pixfmt_set |= TV_PIXFMT_SET (TV_PIXFMT_YUV420);
			break;

		default:
			break;
		}

		++i;
	}

	info->capture.supported_pixfmt_set = pixfmt_set;

	return TRUE;
}

static void
signal_handler			(int			unused)
{
	if (0) {
		fprintf (stderr, ",");
		fflush (stderr);
	}
}

static void
init_signal			(void (*handler)(int))
{
	struct sigaction new_action;
	struct sigaction old_action;

	CLEAR (new_action);

	sigemptyset (&new_action.sa_mask);
	new_action.sa_handler = handler;

	sigaction (SIGUSR1, &new_action, &old_action);
	/* See read_frame(). */
	sigaction (SIGALRM, &new_action, &old_action);
}

static tv_bool
set_format			(struct private_tvengbktr_device_info *p_info,
				 tv_image_format *	fmt,
				 const tv_video_standard *std)
{
	unsigned int frame_width;
	unsigned int frame_height;
	struct meteor_geomet geom;
	int bktr_pixfmt;

	if (p_info->bktr_driver
	    && TV_PIXFMT_IS_YUV (fmt->pixel_format->pixfmt)) {
		const tv_pixel_format *oldfmt;

		/* Bug: the driver initializes BKTR_E|O_VTC for
		   RGB capture but not for YUV. When we switch from RGB
		   width <= 385 (sic) to YUV >= 384 the selected VFILT
		   overflows the FIFO and we get a distorted image. At
		   higher resolutions the driver crashes. May not happen
		   when we read VBI at the same time. */

		oldfmt = fmt->pixel_format;
		fmt->pixel_format =
			tv_pixel_format_from_pixfmt (TV_PIXFMT_BGR16_LE);

		if (!set_format (p_info, fmt, std)) {
			fmt->pixel_format = oldfmt;
			return FALSE;
		}

		fmt->pixel_format = oldfmt;

		if (-1 == bktr_ioctl (&p_info->info,
				      METEORSVIDEO, &no_video)) {
			return FALSE;
		}

		if (-1 == bktr_ioctl (&p_info->info,
				      METEORSSIGNAL, &signal_none)) {
			return FALSE;
		}

		if (-1 == bktr_ioctl (&p_info->info,
				      METEORCAPTUR, &cap_continuous)) {
			return FALSE;
		}

		/* Now initialized. */

		if (-1 == bktr_ioctl (&p_info->info,
				      METEORCAPTUR, &cap_stop_cont)) {
			return FALSE;
		}
	}

	if (std) {
		frame_width = std->frame_width;
		frame_height = std->frame_height;

		assert (frame_width <= 768);
		assert (frame_height <= 576);
	} else {
		frame_width = 640; 
		frame_height = 480;
	}

	/* bktr SETGEO limits. */
	fmt->width = (SATURATE (fmt->width, 32, frame_width) + 1) & ~1;
	fmt->height = (SATURATE (fmt->height, 24, frame_height) + 1) & ~1;

	geom.rows = fmt->height;
	geom.columns = fmt->width;
	geom.frames = 1;

	if (p_info->bktr_driver) {
		const tv_pixel_format *pf;
		unsigned int avg_bpp;

		pf = fmt->pixel_format;

		bktr_pixfmt = p_info->pixfmt_lut[pf->pixfmt];

		if (-1 == bktr_pixfmt)
			return FALSE;

		/* No padding possible. */
		fmt->bytes_per_line[0] = fmt->width * pf->bits_per_pixel / 8;
		fmt->bytes_per_line[1] =
			fmt->bytes_per_line[0] >> pf->uv_hshift;
		fmt->bytes_per_line[2] = fmt->bytes_per_line[1];

		avg_bpp = pf->bits_per_pixel;
		if (TV_PIXFMT_IS_PLANAR (pf->pixfmt))
			avg_bpp += 16 >> (pf->uv_hshift + pf->uv_vshift);

		if (avg_bpp > 16) {
			/* Bug: the driver allocates a buffer with two or four
			   bytes per pixel depending on this flag, not the
			   SACTPIXFMT value. */
			geom.oformat = METEOR_GEO_RGB24;
		} else {
			/* METEORSACTPIXFMT overrides geom.oformat except for
			   the METEOR_ONLY_ODD|EVEN_FIELDS flags. */
			geom.oformat = 0;
		}
	} else {
		/* Correct? */
		switch (fmt->pixel_format->pixfmt) {
		case TV_PIXFMT_BGR16_LE:
			geom.oformat = METEOR_GEO_RGB16;
			fmt->bytes_per_line[0] = fmt->width * 2;
			break;

		case TV_PIXFMT_BGRA32_LE:
			geom.oformat = METEOR_GEO_RGB24;
			fmt->bytes_per_line[0] = fmt->width * 4;
			break;

		case TV_PIXFMT_YUYV:
			geom.oformat = METEOR_GEO_YUV_PACKED;
			fmt->bytes_per_line[0] = fmt->width * 2;
			break;

		case TV_PIXFMT_YUV422:
			geom.oformat = METEOR_GEO_YUV_PLANAR;
			fmt->bytes_per_line[0] = fmt->width * 2;
			break;

		default:
			return FALSE;
		}
	}

	if (fmt->height <= (frame_height >> 1)) {
		/* EVEN, ODD, interlaced (0) */
		geom.oformat |= METEOR_GEO_ODD_ONLY;
	}

	if (p_info->bktr_driver) {
		/* Must call this first to override geom.oformat. */
		if (-1 == bktr_ioctl (&p_info->info,
				      METEORSACTPIXFMT, &bktr_pixfmt)) {
			return FALSE;
		}
	}

	if (-1 == bktr_ioctl (&p_info->info, METEORSETGEO, &geom)) {
		return FALSE;
	}

	return TRUE;
}

/*
 *  Overlay
 */

static tv_bool
set_overlay_buffer		(tveng_device_info *	info,
				 const tv_overlay_buffer *t)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);
	tv_pixel_format format;

	if (-1 == p_info->pixfmt_lut[t->format.pixel_format->pixfmt]) {
		return FALSE;
	}

	p_info->info.overlay.buffer = *t;
	p_info->ovl_bits_per_pixel = t->format.pixel_format->bits_per_pixel;

	return TRUE;
}

static tv_bool
get_overlay_buffer		(tveng_device_info *	info)
{
	/* Nothing to do. */

	return TRUE;
}

static tv_bool
set_clips			(struct private_tvengbktr_device_info *p_info,
				 const tv_window *	w,
				 const tv_clip_vector *	v,
				 unsigned int		scale)
{
	tv_clip_vector vec;
	const tv_clip *clip;
	struct _bktr_clip clips;
	unsigned int band;
	unsigned int vec_size;
	unsigned int i;

	if (scale) {
		const tv_clip *end;

		if (!tv_clip_vector_copy (&vec, v))
			return FALSE;

		end = v->vector + v->size;

		/* Apparently the same vector is used on both fields.
		   We scale by 1/2 and merge clips which overlap now. */
		for (clip = v->vector; clip < end; ++clip) {
			if (!tv_clip_vector_add_clip_xy
			    (&vec, clip->x1, clip->y1 >> 1,
			     clip->x2, (clip->y2 + 1) >> 1))
				goto failure;
		}

		clip = vec.vector;
		vec_size = vec.size;
	} else {
		clip = v->vector;
		vec_size = v->size;
	}

	assert (N_ELEMENTS (clips.x) >= 2);

	band = 0;

	for (i = 0; i < vec_size; ++i) {
		if (i < N_ELEMENTS (clips.x) - 1) {
			/* Bug: naming is backwards. */
			/* Bug: bktr clipping is broken in at least two ways.
			   When a clip reaches the right window boundary
			   the driver extends it to full window width.
			   At high resolution it clips where we didn't ask
			   for it, perhaps scaling vertical coordinates
			   differently for first and second field? */
			clips.x[i].y_min = clip->x1;
			clips.x[i].x_min = clip->y1;
			clips.x[i].y_max = clip->x2;
			clips.x[i].x_max = clip->y2;

			if (0)
				fprintf(stderr, "%u: %u,%u - %u,%u\n",
					i,
					clip->x1,
					clip->y1,
					clip->x2,
					clip->y2);

			++clip;
		} else {
			/* When the rest of the rectangles won't fit we clip
			   away the current band and everything below. */

		  	clips.x[band].y_min = 0; /* x1 */
			clips.x[band].y_max = w->width;
			clips.x[band].x_max = w->height;

			i = band + 1;

			break;
		}
	}

	CLEAR (clips.x[i]); /* end */

	if (scale)
		tv_clip_vector_destroy (&vec);

	if (-1 == bktr_ioctl (&p_info->info, BT848SCLIP, &clips)) {
		return FALSE;
	}

	return TRUE;

 failure:
	if (scale)
		tv_clip_vector_destroy (&vec);

	return FALSE;
}

static tv_bool
enable_overlay			(tveng_device_info *	info,
				 tv_bool		on);

static tv_bool
set_overlay_window		(tveng_device_info *	info,
				 const tv_window *	w,
				 const tv_clip_vector *	v)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);
	const tv_image_format *bf;
	tv_image_format fmt;
	struct meteor_video video;
	int start_line;
	int start_byte;
	unsigned int size;
	int wx1;
	int wy1;
	int wx2;
	int wy2;

	if (p_info->cap_active)
		return FALSE;

	/* Disable overlay if active. */
	if (!enable_overlay (info, FALSE))
		return FALSE;

	wx1 = w->x - p_info->info.overlay.buffer.x;
	wy1 = w->y - p_info->info.overlay.buffer.y;

	wx2 = wx1 + w->width;
	wy2 = wy1 + w->height;

	bf = &p_info->info.overlay.buffer.format;

	if (!p_info->bktr_driver && v->size > 0) {
		/* Clipping not supported. */
		return FALSE;
	}

	/* XXX addr must be dword aligned? */

	start_line = wy1 * (int) bf->bytes_per_line;
	start_byte = (wx1 * (int) p_info->ovl_bits_per_pixel) / 8;

	video.addr = p_info->info.overlay.buffer.base
		+ start_line + start_byte;
	video.width = bf->bytes_per_line[0];

	size = (bf->size - ((char *) video.addr
			    - (char *) p_info->info.overlay.buffer.base));

	video.banksize = size;
	video.ramsize = (size + 1023) >> 10;

	p_info->cap_ready = FALSE;
	p_info->ovl_ready = FALSE;

	if (-1 == bktr_ioctl (info, METEORSVIDEO, &video))
		return FALSE;

	fmt = *bf;

	fmt.width = w->width;
	fmt.height = w->height;

	if (!set_format (p_info, &fmt, info->panel.cur_video_standard))
		return FALSE;

	if (p_info->bktr_driver) {
		unsigned int frame_height;
		unsigned int scale;

		frame_height = 480;
		if (info->panel.cur_video_standard) {
			frame_height = info->panel.cur_video_standard
				->frame_height;
		}

		scale = (w->height > (frame_height >> 1));

		if (!set_clips (p_info, w, v, scale)) {
			return FALSE;
		}
	}

	info->overlay.window.x		= w->x;
	info->overlay.window.y		= w->y;
	info->overlay.window.width	= w->width;
	info->overlay.window.height	= w->height;

	p_info->ovl_ready = TRUE;

	return TRUE;
}

static tv_bool
get_overlay_window		(tveng_device_info *	info)
{
	/* TODO */
	return FALSE;
}

static tv_bool
enable_overlay			(tveng_device_info *	info,
				 tv_bool		enable)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);

	if (p_info->cap_active)
		return FALSE;

	if (p_info->ovl_active == enable)
		return TRUE;

	if (enable) {
	        if (!p_info->ovl_ready)
		        return FALSE;

		if (-1 == bktr_ioctl (info, METEORCAPTUR, &cap_continuous)) {
			return FALSE;
		}
	} else {
		if (-1 == bktr_ioctl (info, METEORCAPTUR, &cap_stop_cont)) {
			return FALSE;
		}
	}

	usleep (50000);

	p_info->ovl_active = enable;

	return TRUE;
}

static tv_bool
get_capture_format		(tveng_device_info * info)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);
	struct meteor_geomet geom;
	tv_pixfmt pixfmt;

	CLEAR (geom);

	if (p_info->bktr_driver) {
		int bktr_pixfmt;

		/* Oddly bktr supports SETGEO but not GETGEO. */
		geom.columns	= info->capture.format.width;
		geom.rows	= info->capture.format.height;

		bktr_pixfmt = -1;

		if (-1 == bktr_ioctl (info, METEORGACTPIXFMT, &bktr_pixfmt))
			return FALSE;

		pixfmt = 0;
		while (pixfmt < N_ELEMENTS (p_info->pixfmt_lut)) {
			if (bktr_pixfmt == p_info->pixfmt_lut[pixfmt])
				break;
			++pixfmt;
		}

		if (pixfmt >= N_ELEMENTS (p_info->pixfmt_lut)) {
			return FALSE;
		}
	} else {
		if (-1 == bktr_ioctl (info, METEORGETGEO, &geom))
			return FALSE;

		switch (geom.oformat) {
		case METEOR_GEO_RGB16:
			pixfmt = TV_PIXFMT_BGR16_LE;
			break;

		case METEOR_GEO_RGB24:
			pixfmt = TV_PIXFMT_BGRA32_LE;
			break;

		case METEOR_GEO_YUV_PACKED:
			pixfmt = TV_PIXFMT_YUYV;
			break;

		default:
			return FALSE;
		}
	}

	if (0 == geom.columns || 0 == geom.rows) {
		CLEAR (info->capture.format);
		info->capture.format.pixel_format =
			tv_pixel_format_from_pixfmt (pixfmt);
	} else {
		if (!tv_image_format_init (&info->capture.format,
					   geom.columns,
					   geom.rows,
					   /* bytes_per_line (minimum) */ 0,
					   pixfmt,
					   /* reserved */ 0)) {
			return FALSE;
		}
	}

	if (0)
		_tv_image_format_dump (&info->capture.format, stderr);

	return TRUE;
}

static tv_bool
set_capture_format		(tveng_device_info *	info,
				 const tv_image_format *fmt)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);
	capture_mode current_mode;
	gboolean overlay_was_active;

	if (p_info->ovl_active | p_info->cap_active)
		return FALSE;

	p_info->cap_ready = FALSE;
	p_info->ovl_ready = FALSE;

	if (-1 == bktr_ioctl (info, METEORSVIDEO, &no_video)) {
		return FALSE;
	}

	if (p_info->bktr_driver) {
		if (-1 == bktr_ioctl (info, BT848SCLIP, &no_clips)) {
			return FALSE;
		}
	}

	info->capture.format = *fmt;

	if (!set_format (p_info,
			 &info->capture.format,
			 info->panel.cur_video_standard)) {
		return FALSE;
	}

	usleep (50000);

	p_info->cap_ready = TRUE;

	return TRUE;
}


static void
timestamp_init(tveng_device_info *info)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);
	double rate = info->panel.cur_video_standard ?
		info->panel.cur_video_standard->frame_rate : 25; /* XXX*/

  p_info->capture_time = 0.0;
  p_info->frame_period_near = p_info->frame_period_far = 1.0 / rate;
}

static tv_bool
start_capturing			(tveng_device_info *	info)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);
	gboolean dummy;

	p_info->cap_active = FALSE;

	if (!p_info->cap_ready) {
		if (!set_capture_format (info, &info->capture.format))
			return FALSE;

		assert (p_info->cap_ready);
	}

	p_tveng_stop_everything(info, &dummy);
	t_assert(CAPTURE_MODE_NONE == info ->capture_mode);

	timestamp_init(info);

	p_info->mmapped_data = device_mmap (info->log_fp,
					    /* start */ 0,
					    /* length */ 768 * 576 * 8,
					    PROT_READ,
					    MAP_SHARED,
					    info->fd,
					    /* offset */ 0);

	if ((char *) -1 == p_info->mmapped_data) {
		return FALSE;
	}

	if (-1 == bktr_ioctl (info, METEORSVIDEO, &no_video)) {
		return FALSE;
	}

	if (-1 == bktr_ioctl (info, BT848SCLIP, &no_clips)) {
		return FALSE;
	}

	init_signal (signal_handler);

	if (-1 == bktr_ioctl (info, METEORSSIGNAL, &signal_usr1)) {
		return FALSE;
	}

	if (-1 == bktr_ioctl (info, METEORCAPTUR, &cap_continuous)) {
		return FALSE;
	}

	info->capture_mode = CAPTURE_MODE_READ;

	p_info->cap_active = TRUE;

	return TRUE;
}

static tv_bool
stop_capturing			(tveng_device_info *	info)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);

	p_info->cap_active = FALSE;

	if (CAPTURE_MODE_NONE == info->capture_mode) {
		fprintf(stderr, "Warning: trying to stop capture with "
			"no capture active\n");
		return TRUE; /* Nothing to be done */
	}

	t_assert(CAPTURE_MODE_READ == info->capture_mode);

	if (-1 == bktr_ioctl (info, METEORCAPTUR, &cap_stop_cont)) {
		return FALSE;
	}

	if (-1 == bktr_ioctl (info, METEORSSIGNAL, &signal_none)) {
		return FALSE;
	}

	if ((char *) -1 != p_info->mmapped_data
	    && (char *) 0 != p_info->mmapped_data)
		if (-1 == device_munmap (info->log_fp,
					 p_info->mmapped_data,
					 /* length */ 768 * 576 * 8))
		{
			info -> tveng_errno = errno;
			t_error("munmap()", info);
		}

	p_info->mmapped_data = 0;

	info->capture_mode = CAPTURE_MODE_NONE;

	return TRUE;
}

static tv_bool
enable_capture			(tveng_device_info *	info,
				 tv_bool		enable)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);
	int r;

	if (p_info->ovl_active)
		return FALSE;

	if (p_info->cap_active == enable)
		return TRUE;

	if (enable) {
		return start_capturing (info);
	} else {
		return stop_capturing (info);
	}
}

static inline double
timestamp(struct private_tvengbktr_device_info *p_info)
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
meteor_mem_dump			(const struct meteor_mem *mm,
				 FILE *			fp)
{
	fprintf (fp,
		 "frame_size=%d num_bufs=%d lowat=%d hiwat=%d "
		 "active=%08x num_active_bufs=%d buf=%p\n",
		 mm->frame_size,
		 mm->num_bufs,
		 mm->lowat,
		 mm->hiwat,
		 mm->active,
		 mm->num_active_bufs,
		 (void *) mm->buf);
}

static int
read_frame			(tveng_device_info *	info,
				 tv_capture_buffer *	buffer,
				 const struct timeval *	timeout)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);
	sigset_t sa_mask;
	struct timeval timestamp;
	int r;

	if (!p_info->cap_active) {
		return -1; /* error */
	}

	/* Partially stolen from xawtv. Blocks all signals except USR1
	   (new frame) and ALRM (timeout), then waits for any signal. */

	if (timeout) {
		struct itimerval nvalue;
		struct itimerval ovalue;

		CLEAR (nvalue);

		if ((timeout->tv_sec | timeout->tv_usec) > 0) {
			nvalue.it_value = *timeout;
		} else {
			/* XXX non-blocking impossible. */

			nvalue.it_value.tv_sec = 0;
			nvalue.it_value.tv_usec = 1000;
		}

		setitimer (ITIMER_REAL, &nvalue, &ovalue);

		sigfillset (&sa_mask);
		sigdelset (&sa_mask, SIGUSR1);
		sigdelset (&sa_mask, SIGALRM);
	} else {
		/* Infinite timeout. */

		sigfillset (&sa_mask);
		sigdelset (&sa_mask, SIGUSR1);
	}

	sigsuspend (&sa_mask);

	/* XXX did we get USR1 (success) or ALRM (timeout, return 0) ? */

	/* XXX this is inaccurate. */
	r = gettimeofday (&timestamp, /* timezone */ NULL);
	assert (0 == r);

	if (timeout) {
		struct itimerval nvalue;
		struct itimerval ovalue;

		CLEAR (nvalue);

		/* Cancel alarm. */
		setitimer (ITIMER_REAL, &nvalue, &ovalue);
	}

	if (0) {
		/* Double buffering not supported by bktr. :-( */
		meteor_mem_dump ((struct meteor_mem *)
				 p_info->mmapped_data, stderr);
	}

	if (buffer) {
		const tv_image_format *dst_format;

		dst_format = buffer->format;
		if (!dst_format)
			dst_format = &info->capture.format;

		tv_copy_image (buffer->data, dst_format,
			       p_info->mmapped_data, &info->capture.format);

		buffer->sample_time = timestamp;
		buffer->stream_time = 0; /* FIXME */
	}

	return 1; /* success */
}

#if 0
/* Doesn't work right, we use the default 0,0,768,576 or 0,0,640,480. */
{
	struct bktr_capture_area ca;

	bktr_ioctl (info, BT848_GCAPAREA, &ca);
	bktr_ioctl (info, BT848_SCAPAREA, &ca);
}
#endif

/* Closes a device opened with tveng_init_device */
static void
tvengbktr_close_device (tveng_device_info * info)
{
	gboolean was_active;

  p_tveng_stop_everything (info, &was_active);

  if (-1 != P_INFO (info)->tuner_fd) {
	  device_close (info->log_fp, P_INFO (info)->tuner_fd);
	  P_INFO (info)->tuner_fd = -1;
  }

  device_close (info->log_fp, info->fd);
  info->fd = -1;

  info->current_controller = TVENG_CONTROLLER_NONE;

  if (info->file_name)
    {
      free (info->file_name);
      info->file_name = NULL;
    }

  free_controls (info);
  free_video_standards (info);
  free_video_inputs (info);
}

/*
*/

static int
p_tvengbktr_open_device_file(int flags,
			     tveng_device_info * info)
{
  unsigned int gstatus;
  /* unsigned short mstatus; */

  t_assert(info != NULL);
  t_assert(info->file_name != NULL);

  info->fd = device_open (info->log_fp, info->file_name, flags, 0);
  if (-1 == info->fd)
    {
      info->tveng_errno = errno;
      t_error("open()", info);
      return -1;
    }

  if (0 == bktr_ioctl (info, BT848_GSTATUS, &gstatus))
    {
      z_strlcpy (info->caps.name, "Brooktree",
		 N_ELEMENTS (info->caps.name));
      info->caps.flags =
	TVENG_CAPS_TUNER |
	TVENG_CAPS_TELETEXT |
	TVENG_CAPS_OVERLAY |
	TVENG_CAPS_CLIPPING;
      info->caps.channels = 5;
      info->caps.audios = 0;
      info->caps.minwidth = 32; /* XXX */
      info->caps.minheight = 32; /* XXX */
      info->caps.maxwidth = 768; /* XXX */
      info->caps.maxheight = 576; /* XXX */

      /* FIXME /dev/bktrN -> /dev/tunerN */
      P_INFO (info)->tuner_fd =
	      device_open (info->log_fp, "/dev/tuner", flags, 0);
      if (-1 == P_INFO (info)->tuner_fd) {
	      info->tveng_errno = errno;
	      t_error("Bad device", info);
	      device_close(info->log_fp, info->fd);
	      return -1;
      }

      P_INFO (info)->bktr_driver = TRUE;
    }
#if 0 /* NEEDS TESTING */
  else if (0 == bktr_ioctl (info, METEORSTATUS, &mstatus))
    {
      z_strlcpy (info->caps.name, "Meteor",
		 N_ELEMENTS (info->caps.name));
      info->caps.flags = 0;
      info->caps.channels = 5;
      info->caps.audios = 0;
      info->caps.minwidth = 32; /* XXX */
      info->caps.minheight = 32; /* XXX */
      info->caps.maxwidth = 768; /* XXX */
      info->caps.maxheight = 576; /* XXX */

      P_INFO (info)->bktr_driver = FALSE;
    }
#endif
  else
    {
      info->tveng_errno = errno;
      t_error("Bad device", info);
      device_close(info->log_fp, info->fd);
      return -1;
    }

  info->current_controller = TVENG_CONTROLLER_V4L2;
  
  return (info->fd);
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
static int
tvengbktr_attach_device (const char* device_file,
			 Window window,
			 enum tveng_attach_mode attach_mode,
			 tveng_device_info * info)
{
  if (-1 != info->fd)
    tveng_close_device (info);

  if (!(info->file_name = strdup (device_file)))
    {
      info->tveng_errno = errno;
      t_error ("strdup()", info);
      return -1;
    }

  if (attach_mode == TVENG_ATTACH_XV)
    attach_mode = TVENG_ATTACH_READ;

  switch (attach_mode)
    {
    case TVENG_ATTACH_CONTROL:
    case TVENG_ATTACH_READ:
    case TVENG_ATTACH_VBI:
      info->fd = p_tvengbktr_open_device_file(O_RDWR, info);
      break;

    default:
      t_error_msg ("switch()", "Unknown attach mode for the device",
		   info);
      free(info->file_name);
      info->file_name = NULL;
      return -1;
    }

  if (-1 == info->fd)
    {
      free(info->file_name);
      info->file_name = NULL;
      return -1;
    }

  info->attach_mode = attach_mode;
  info->capture_mode = CAPTURE_MODE_NONE;

	CLEAR (info->panel);

	info->panel.set_video_input = set_video_input;
	info->panel.get_video_input = get_video_input;
	info->panel.set_tuner_frequency = set_tuner_frequency;
	info->panel.get_tuner_frequency = get_tuner_frequency;
	info->panel.get_signal_strength = get_signal_strength;

	if (!get_video_input_list (info))
		goto failure;

	info->panel.set_audio_input = set_audio_input;
	info->panel.get_audio_input = get_audio_input;

	if (P_INFO (info)->bktr_driver) {
		if (!get_audio_input_list (info))
			goto failure;
	}

	info->panel.set_video_standard = set_video_standard;
	info->panel.get_video_standard = get_video_standard;

	if (!get_standard_list (info))
		goto failure;

	info->panel.set_control = set_control;
	info->panel.get_control = get_control;

	if (!get_control_list (info))
		goto failure;

	CLEAR (info->overlay);

	info->overlay.set_buffer = set_overlay_buffer;
	info->overlay.get_buffer = get_overlay_buffer;
	info->overlay.set_window_clipvec = set_overlay_window;
	info->overlay.get_window = get_overlay_window;
	info->overlay.enable = enable_overlay;

	CLEAR (info->capture);

	info->capture.get_format = get_capture_format;
	info->capture.set_format = set_capture_format;
	info->capture.read_frame = read_frame;
	info->capture.enable = enable_capture;

	if (P_INFO (info)->bktr_driver) {
		init_pixfmt_lut (info);
	} else {
		/* Correct? */
		info->capture.supported_pixfmt_set =
			(TV_PIXFMT_SET (TV_PIXFMT_BGR16_LE) |
			 TV_PIXFMT_SET (TV_PIXFMT_BGRA32_LE) |
			 TV_PIXFMT_SET (TV_PIXFMT_YUYV) |
			 TV_PIXFMT_SET (TV_PIXFMT_YUV422));
	}

	tv_image_format_init (&info->capture.format, 160, 120, 0,
			      TV_PIXFMT_BGR16_LE, 0);

	/* Bug: VBI capturing works only if we capture video
	   at the same time. Additionally the video capture
	   format must be RGB. */
	if (TVENG_ATTACH_VBI == attach_mode) {
		unsigned long ul;

		/* We need this only for Teletext, which is PAL. */
		ul = BT848_IFORM_F_PALBDGHI;
		bktr_ioctl (info, BT848SFMT, &ul);

		if (!set_capture_format (info, &info->capture.format))
			goto failure;

		if (!start_capturing (info))
			goto failure;
	}

  return info->fd;

 failure:
  tvengbktr_close_device (info);
  return -1;
}

static struct tveng_module_info tvengbktr_module_info = {
  .attach_device =		tvengbktr_attach_device,
  .close_device =		tvengbktr_close_device,

  .interface_label		= "BKTR/Meteor",

  .private_size			= sizeof (struct private_tvengbktr_device_info)
};

void tvengbktr_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

  *module_info = tvengbktr_module_info;
}

#else /* !ENABLE_BKTR */

#include "tvengbktr.h"

void tvengbktr_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

  CLEAR (*module_info);
}

#endif /* ENABLE_BKTR */
