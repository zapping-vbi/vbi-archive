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
#include "config.h"

#ifdef ENABLE_BKTR

#include "tvengbktr.h"

#include "zmisc.h"

#include "tveng_private.h"

#include "common/ioctl_meteor.h"
#include "common/ioctl_bt848.h"
#include "common/fprintf_bktr.h"

struct private_tvengbktr_device_info
{
	tveng_device_info	info; /* Info field, inherited */

	tv_overlay_buffer	overlay_buffer;
	unsigned int		overlay_bits_per_pixel;
	unsigned		bktr_driver		: 1;
};

#define P_INFO(p) PARENT (p, struct private_tvengbktr_device_info, info)

#define bktr_ioctl(info, cmd, arg)					\
(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
 ((0 == device_ioctl ((info)->log_fp, fprintf_ioctl_arg,		\
		      (info)->fd, cmd, (void *)(arg))) ?		\
  0 : (ioctl_failure (info, __FILE__, __PRETTY_FUNCTION__,		\
		      __LINE__, # cmd), -1)))

#define bktr_ioctl_nf(info, cmd, arg)					\
(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
 device_ioctl ((info)->log_fp, fprintf_ioctl_arg,			\
	       (info)->fd, cmd, (void *)(arg)))

struct control {
	tv_control		pub;
	unsigned int		id;
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

/*
 *  Controls
 */

static tv_bool
do_get_control		(struct private_tvengbktr_device_info * p_info,
				 struct control *	c)
{
	int value;

	switch (c->id) {
	case METEORGHUE: /* get hue */
	{
		signed char c; /* range -128 ... +127 */

		if (-1 == bktr_ioctl (&p_info->info, METEORGHUE, &c))
			return FALSE;
		value = c;
		break;
	}

	case METEORGBRIG: /* get brightness */
	case METEORGCSAT: /* get uv saturation */
	case METEORGCONT: /* get contrast */
	case METEORGCHCV: /* get uv gain */
	{
		unsigned char uc; /* range 0 ... 255 */

		if (-1 == device_ioctl (p_info->info.log_fp,
					fprintf_ioctl_arg,
					p_info->info.fd,
					/* cmd */ c->id, &uc)) {
			ioctl_failure (&p_info->info,
				       __FILE__, __PRETTY_FUNCTION__,
				       __LINE__, "METEORGXXX");
			return FALSE;
		}

		value = uc;
		break;
	}

	case BT848_GHUE: /* get hue */
	case BT848_GBRIG: /* get brightness */
	case BT848_GCSAT: /* get UV saturation */
	case BT848_GCONT: /* get contrast */
	case BT848_GVSAT: /* get V saturation */
	case BT848_GUSAT: /* get U saturation */
		/* Range varies, see below. */
		if (-1 == device_ioctl (p_info->info.log_fp,
					fprintf_ioctl_arg,
					p_info->info.fd,
					/* cmd */ c->id, &value)) {
			ioctl_failure (&p_info->info,
				       __FILE__, __PRETTY_FUNCTION__,
				       __LINE__, "BT848_GXXX");
			return FALSE;
		}

		break;

	default:
		t_warn ("Invalid c->id 0x%x\n", c->id);
		p_info->info.tveng_errno = -1; /* unknown */
		return FALSE;
	}

	if (c->pub.value != value) {
		c->pub.value = value;
		tv_callback_notify (&c->pub, c->pub._callback);
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
	struct private_tvengbktr_device_info *p_info = P_INFO (info);
	unsigned char uc;
	int r;

	switch (C(c)->id) {
	/* The interface has no control IDs, we use the get ioctl
	   numbers instead. These are here translated to the
	   corresponding set ioctls. */

	case METEORGHUE: /* -> set hue */
	{
		signed char c; /* range -128 ... +127 */

		c = value;
		r = bktr_ioctl (&p_info->info, METEORSHUE, &c);
		break;
	}

	case METEORGBRIG: /* -> set brightness */
		uc = value; /* range 0 ... 255 */
		r = bktr_ioctl (&p_info->info, METEORSBRIG, &uc);
		break;

	case METEORGCSAT: /* -> set chroma sat */
		uc = value;
		r = bktr_ioctl (&p_info->info, METEORSCSAT, &uc);
		break;

	case METEORGCONT: /* -> set contrast */
		uc = value;
		r = bktr_ioctl (&p_info->info, METEORSCONT, &uc);
		break;

	case METEORGCHCV: /* -> set uv gain */
		uc = value;
		r = bktr_ioctl (&p_info->info, METEORSCHCV, &uc);
		break;

	/* Range of the bt848 controls is defined by
	   BT848_.*(MIN|MAX|CENTER|RANGE) floats in some technical units
	   and BT848_.*(REGMIN|REGMAX|STEPS) ints for the hardware.
	   The ioctls naturally take the latter values. */

	case BT848_GHUE: /* -> set hue */
		r = bktr_ioctl (&p_info->info, BT848_SHUE, &value);
		break;

	case BT848_GBRIG: /* -> set brightness */
		r = bktr_ioctl (&p_info->info, BT848_SBRIG, &value);
		break;

	case BT848_GCSAT: /* -> set chroma sat */
		r = bktr_ioctl (&p_info->info, BT848_SCSAT, &value);
		break;

	case BT848_GCONT: /* -> set contrast */
		r = bktr_ioctl (&p_info->info, BT848_SCONT, &value);
		break;

	case BT848_GVSAT: /* -> set chroma V sat */
		r = bktr_ioctl (&p_info->info, BT848_SVSAT, &value);
		break;

	case BT848_GUSAT: /* -> set chroma U sat */
		r = bktr_ioctl (&p_info->info, BT848_SUSAT, &value);
		break;

	default:
		t_warn ("Invalid c->id 0x%x\n", C(c)->id);
		p_info->info.tveng_errno = -1; /* unknown */
		return FALSE;
	}

	if (-1 == r)
		return FALSE;

	if (c->value != value) {
		c->value = value;
		tv_callback_notify (&c, c->_callback);
	}

	return TRUE;
}

struct control_bridge {
	unsigned int		ioctl;
	const char *		label;
	tv_control_id		id;
	int			minimum;
	int			maximum;
	int			reset;
};

#define CONTROL_BRIDGE_END { 0, NULL, 0, 0, 0, 0 }

static const struct control_bridge
meteor_controls [] = {
	{ METEORGBRIG, N_("Brightness"), TV_CONTROL_ID_BRIGHTNESS,   0, 255, 128 },
	{ METEORGCONT, N_("Contrast"),   TV_CONTROL_ID_CONTRAST,     0, 255, 128 },
	{ METEORGCSAT, N_("Saturation"), TV_CONTROL_ID_SATURATION,   0, 255, 128 },
	{ METEORGHUE,  N_("Hue"),        TV_CONTROL_ID_HUE,       -128, 127,   0 },
#if 0
	{ METEORGCHCV, N_("U/V Gain"),   TV_CONTROL_ID_UNKNOWN,      0, 255, 128 },
#endif
	CONTROL_BRIDGE_END
};

static const struct control_bridge
bktr_controls [] = {
	{ BT848_GBRIG, N_("Brightness"),   TV_CONTROL_ID_BRIGHTNESS, -128, 127,   0 },
	{ BT848_GCONT, N_("Contrast"),     TV_CONTROL_ID_CONTRAST,      0, 511, 216 },
	{ BT848_GCSAT, N_("Saturation"),   TV_CONTROL_ID_SATURATION,    0, 511, 216 },
	{ BT848_GHUE,  N_("Hue"),          TV_CONTROL_ID_HUE,        -128, 127,   0 },
#if 0
	{ BT848_GUSAT, N_("U Saturation"), TV_CONTROL_ID_UNKNOWN,       0, 511, 256 },
	{ BT848_GVSAT, N_("V Saturation"), TV_CONTROL_ID_UNKNOWN,       0, 511, 180 },
#endif
	CONTROL_BRIDGE_END
};

static tv_bool
get_control_list		(tveng_device_info *	info)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);
	const struct control_bridge *table;
	struct control *c;
	tv_control *tc;

	if (P_INFO (info)->bktr_driver)
		table = bktr_controls;
	else
		table = meteor_controls;

	for (; table->ioctl; ++table) {
		if (!(c = calloc (1, sizeof (*c))))
			return FALSE;

		c->id		= table->ioctl;

		c->pub.type	= TV_CONTROL_TYPE_INTEGER;
		c->pub.id	= table->id;

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
	}

	return TRUE;
}

/*
 *  Video standards
 */

static tv_bool
get_standard			(tveng_device_info *	info)
{
	tv_video_standard *s;

	s = NULL; /* unknown */

	if (info->video_standards) {
		unsigned long fmt;

		if (P_INFO (info)->bktr_driver) {
			if (-1 == bktr_ioctl (info, BT848GFMT, &fmt))
				return FALSE;
		} else {
			if (-1 == bktr_ioctl (info, METEORGFMT, &fmt))
				return FALSE;
		}

		for (s = info->video_standards; s; s = s->_next)
			if (S(s)->fmt == fmt)
				break;
	}

	store_cur_video_standard (info, s);

	return TRUE;
}

static tv_bool
set_standard			(tveng_device_info *	info,
				 const tv_video_standard *s)
{
	enum tveng_capture_mode current_mode;
	int r;

	current_mode = p_tveng_stop_everything (info);

	if (P_INFO (info)->bktr_driver) {
		r = bktr_ioctl (info, BT848GFMT, &S(s)->fmt);
	} else {
		r = bktr_ioctl (info, METEORGFMT, &S(s)->fmt);
	}

	/* Start capturing again as if nothing had happened */
	/* XXX stop yes, restarting is not our business (eg. frame geometry change). */
	p_tveng_restart_everything (current_mode, info);

	return (0 == r);
}

struct standard_bridge {
	unsigned int		fmt;
	const char *		label;
	tv_video_standard_id	id;
};

#define STANDARD_BRIDGE_END { 0, NULL, 0 }

static const struct standard_bridge
meteor_standards [] = {
	/* XXX should investigate what exactly these videostandards are. */
	{ METEOR_FMT_PAL,		"PAL",		TV_VIDEOSTD_PAL },
	{ METEOR_FMT_NTSC,		"NTSC",		TV_VIDEOSTD_NTSC },
	{ METEOR_FMT_SECAM,		"SECAM",	TV_VIDEOSTD_SECAM },
	STANDARD_BRIDGE_END
};

static const struct standard_bridge
bktr_standards [] = {
	{ BT848_IFORM_F_PALBDGHI,	"PAL",		TV_VIDEOSTD_PAL },
	{ BT848_IFORM_F_NTSCM,		"NTSC",		TV_VIDEOSTD_NTSC_M },
	{ BT848_IFORM_F_SECAM,		"SECAM",	TV_VIDEOSTD_SECAM },
	{ BT848_IFORM_F_PALM,		"PAL-M",	TV_VIDEOSTD_PAL_M },
	{ BT848_IFORM_F_PALN,		"PAL-N",	TV_VIDEOSTD_PAL_N },
	{ BT848_IFORM_F_NTSCJ,		"NTSC-JP",	TV_VIDEOSTD_NTSC_M_JP },
#if 0
	{ BT848_IFORM_F_AUTO,		"AUTO",		TV_VIDEOSTD_UNKNOWN },
	{ BT848_IFORM_F_RSVD,		"RSVD",		TV_VIDEOSTD_UNKNOWN },
#endif
	STANDARD_BRIDGE_END
};

static tv_bool
get_standard_list		(tveng_device_info *	info)
{
	const struct standard_bridge *table;

	if (info->video_standards)
		return TRUE; /* invariable */

	if (P_INFO (info)->bktr_driver)
		table = bktr_standards;
	else
		table = meteor_standards;

	for (; table->label; ++table) {
		struct standard *s;

		if (!(s = S(append_video_standard (&info->video_standards,
						   table->id,
						   table->label,
						   table->label,
						   sizeof (*s))))) {
			free_video_standard_list (&info->video_standards);
			return FALSE;
		}

		s->fmt = table->fmt;
	}

	return get_standard (info);
}

/*
 *  Video inputs
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
		tv_callback_notify (&vi->pub, vi->pub._callback);
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
	if (info->cur_video_input == l) {
		if (-1 == bktr_ioctl (info, TVTUNER_GETFREQ, &freq))
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
	struct video_input *vi = VI(l);
	unsigned int freq;

	freq = (frequency + (FREQ_UNIT >> 1)) / FREQ_UNIT;

	if (-1 == bktr_ioctl (info, TVTUNER_SETFREQ, &freq))
		return FALSE;

	store_frequency (vi, freq);

	return TRUE;
}

static tv_bool
get_video_input			(tveng_device_info *	info)
{
	const tv_video_line *l = NULL;

	if (info->video_inputs) {
		unsigned long dev;

		if (-1 == bktr_ioctl (info, METEORGINPUT, &dev))
			return FALSE;

		for_all (l, info->video_inputs)
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
				 const tv_video_line *	l)
{
	enum tveng_capture_mode current_mode;
	enum tveng_frame_pixformat pixformat;

	if (info->cur_video_input) {
		unsigned long dev;

		if (0 == bktr_ioctl (info, METEORGINPUT, &dev))
			if (VI(l)->dev == dev)
				return TRUE;
	}

	pixformat = info->format.pixformat;
	current_mode = p_tveng_stop_everything(info);

	if (-1 == bktr_ioctl (info, METEORSINPUT, &VI(l)->dev))
		return FALSE;

	store_cur_video_input (info, l);

	/* Standards are invariable. */
	if (0)
		get_standard_list (info);


	/* There is only one tuner, if any.
	   XXX ignores the possibility (?) that a third party changed
	   the frequency from the value we know. */
	if (0 && IS_TUNER_LINE (l))
		set_tuner_frequency (info, info->cur_video_input,
				     info->cur_video_input->u.tuner.frequency);

	info->format.pixformat = pixformat;
	p_tveng_set_capture_format(info);

	/* XXX Start capturing again as if nothing had happened */
	p_tveng_restart_everything (current_mode, info);

	return TRUE;
}

struct video_input_bridge {
	unsigned int		dev;
	const char *		label;
	tv_video_line_type	type;
};

#define VIDEO_INPUT_BRIDGE_END { 0, NULL, 0 }

static const struct video_input_bridge
meteor_video_inputs [] = {
	{ METEOR_INPUT_DEV_RCA,	   "Composite",	TV_VIDEO_LINE_TYPE_BASEBAND },
	{ METEOR_INPUT_DEV_SVIDEO, "S-Video",   TV_VIDEO_LINE_TYPE_BASEBAND },
	{ METEOR_INPUT_DEV1,	   "Dev1",	TV_VIDEO_LINE_TYPE_BASEBAND },
	{ METEOR_INPUT_DEV2,	   "Dev2",	TV_VIDEO_LINE_TYPE_BASEBAND },
	{ METEOR_INPUT_DEV3,	   "Dev3",	TV_VIDEO_LINE_TYPE_BASEBAND },
	VIDEO_INPUT_BRIDGE_END
};

static const struct video_input_bridge
bktr_video_inputs [] = {
	{ METEOR_INPUT_DEV1,	   "Television",          TV_VIDEO_LINE_TYPE_TUNER },
	{ METEOR_INPUT_DEV_RCA,	   "Composite",	          TV_VIDEO_LINE_TYPE_BASEBAND },
	{ METEOR_INPUT_DEV_SVIDEO, "S-Video",	          TV_VIDEO_LINE_TYPE_BASEBAND },
	{ METEOR_INPUT_DEV2,	   "Composite (S-Video)", TV_VIDEO_LINE_TYPE_BASEBAND },
	{ METEOR_INPUT_DEV3,	   "Composite 3",         TV_VIDEO_LINE_TYPE_BASEBAND },
	VIDEO_INPUT_BRIDGE_END
};

static tv_bool
get_video_input_list		(tveng_device_info *	info)
{
	const struct video_input_bridge *table;

	if (info->video_inputs)
		return TRUE; /* invariable */

	if (P_INFO (info)->bktr_driver)
		table = bktr_video_inputs;
	else
		table = meteor_video_inputs;

	for (; table->label; ++table) {
		struct video_input *vi;

		if (!(vi = VI (append_video_line (&info->video_inputs,
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

			if (-1 == bktr_ioctl (info, TVTUNER_GETFREQ, &freq))
				goto failure;

			store_frequency (vi, freq);
		}
	}

	if (info->video_inputs) {
		get_video_input (info);
	}

	return TRUE;

 failure:
	free_video_line_list (&info->video_inputs);
	return FALSE;
}

#if 0

/*
  Gets the signal strength and the afc code. The afc code indicates
  how to get a better signal, if negative, tune higher, if negative,
  tune lower. 0 means no idea or feature not present in the current
  controller (i.e. V4L1). Strength and/or afc can be NULL pointers,
  that would mean ignore that parameter.
*/
static int
tvengbktr_get_signal_strength (int *strength, int * afc,
			   tveng_device_info * info)
{
  struct v4l2_tuner tuner;

  t_assert(info != NULL);
  t_assert(info->cur_input < info->num_inputs);
  t_assert(info->cur_input >= 0);

  /* Check that there are tuners in the current input */
  if (info->inputs[info->cur_input].tuners == 0)
    return -1;



  tuner.input = info->inputs[info->cur_input].id;
  if (IOCTL(info -> fd, VIDIOC_G_TUNER, &tuner) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_G_TUNER", info);
      return -1;
    }

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

#endif

/*
 *  Overlay
 */

static tv_bool
get_overlay_buffer		(tveng_device_info *	info,
				 tv_overlay_buffer *	t)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);

	*t = p_info->overlay_buffer;
	return TRUE;
}

static tv_bool
set_overlay_buffer		(tveng_device_info *	info,
				 const tv_overlay_buffer *t)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);
	tv_pixel_format format;

	/* XXX check if we support the pixfmt. */

	if (!tv_pixfmt_to_pixel_format (&format, t->pixfmt,
					TV_COLOR_SPACE_UNKNOWN))
		return FALSE;

	p_info->overlay_buffer = *t;
	p_info->overlay_bits_per_pixel = format.bits_per_pixel;

	return TRUE;
}

#if 0

/*
  Sets the preview window dimensions to the given window.
  Returns -1 on error, something else on success.
  Success doesn't mean that the requested dimensions are used, maybe
  they are different, check the returned fields to see if they are suitable
  info   : Device we are controlling
*/
static int
tvengbktr_set_preview_window	(tveng_device_info *	info,
				 x11_dga_parameters *	dga)
{
	struct private_tvengbktr_device_info *p_info = P_INFO (info);
	struct meteor_video video;
	struct meteor_geom geom;
	struct _bktr_clip clip;
	const tv_clip *tclip;
	unsigned int band;
	unsigned int size;
	unsigned int i;

	stop overlay;

	/* XXX addr must be dword aligned? */
	video.addr = (void *)((char *) p_info->overlay_buffer.base
			      + (info->overlay_window.y
				 * p_info->overlay_buffer.bytes_per_line)
			      + ((info->overlay_window.x
				  * p_info->overlay_bits_per_pixel) >> 3));

	video.width = p_info->overlay_buffer.bytes_per_line;

	size = (p_info->overlay_buffer.size
		- ((char *) video.addr
		   - (char *) p_info->overlay_buffer.base)) >> 10;

	video.banksize = size;
	video.ramsize = size;

	geom.rows = info->window.width;
	geom.columns = info->window.height;
	geom.frames = 1;
	geom.oformat = ;

	assert (N_ELEMENTS (clip.x) >= 2);

	tclip = info->overlay_window.clip_vector.vector;
	band = 0;

	for (i = 0; i < info->overlay_window.clip_vector.size; ++i) {
		if (i < N_ELEMENTS (clip.x) - 1) {
			clip.x[i].x_min = tclip->x;
			clip.x[i].y_min = tclip->y;
			clip.x[i].x_max = tclip->x + tclip->width;
			clip.x[i].y_max = tclip->y + tclip->height;

			if (clip.x[i].y_min != clip.x[band].y_min)
				band = i;
		} else {
			/* When the rest of the clipping rectangles won't fit,
			   clips are y-x sorted, we clip away the current
			   band and everything below. */

			clip.x[band].x_min = 0;
			clip.x[band].x_max = info->window.width;
			clip.x[band].y_max = info->window.height;

			i = band + 1;

			break;
		}
	}

	clip.x[i].y_min = 0;
	clip.x[i].y_max = 0;

  /* Update the info struct */
  return (tvengbktr_get_preview_window(info));
}

/*
  Gets the current overlay window parameters.
  Returns -1 on error, and any other value on success.
  info   : The device to use
*/
static int
tvengbktr_get_preview_window(tveng_device_info * info)
{
  /* Updates the entire capture format, since there is no
     difference */
  return (tvengbktr_update_capture_format(info));
}

static tv_bool
set_overlay			(tveng_device_info *	info,
				 tv_bool		on)
{
}

#endif



/* Closes a device opened with tveng_init_device */
static void
tvengbktr_close_device (tveng_device_info * info)
{
  p_tveng_stop_everything(info);

  device_close (info->log_fp, info->fd);
  info->fd = 0;

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

static int
p_tvengbktr_open_device_file(int flags,
			     tveng_device_info * info)
{
  unsigned int gstatus;
  unsigned short mstatus;

  t_assert(info != NULL);
  t_assert(info->file_name != NULL);

  if (-1 == (info->fd = open (info->file_name, flags)))
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
    }
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
    }
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
			 enum tveng_attach_mode attach_mode,
			 tveng_device_info * info)
{
  if (info->fd)
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
  info->current_mode = TVENG_NO_CAPTURE;

  info->video_inputs = NULL;
  info->cur_video_input = NULL;

  if (!get_video_input_list (info))
    {
      tvengbktr_close_device (info);
      return -1;
    }

  info->video_standards = NULL;
  info->cur_video_standard = NULL;

  if (!get_standard_list (info))
    {
      tvengbktr_close_device (info);
      return -1;
    }

  info->controls = NULL;

  if (!get_control_list (info))
    {
      tvengbktr_close_device (info);
      return -1;
    }

  return info->fd;
}


static void
tvengbktr_describe_controller(char ** short_str,
			      char ** long_str,
			      tveng_device_info * info)
{
  t_assert(info != NULL);

  if (short_str)
    *short_str = "BKTR";

  if (long_str)
    *long_str = "BKTR/Meteor";
}

static struct tveng_module_info tvengbktr_module_info = {
  .attach_device =		tvengbktr_attach_device,
  .describe_controller =	tvengbktr_describe_controller,
  .close_device =		tvengbktr_close_device,
  .set_video_input		= set_video_input,
  .update_video_input		= get_video_input,
  .set_tuner_frequency		= set_tuner_frequency,
  .update_tuner_frequency	= get_tuner_frequency,
  .set_standard			= set_standard,
  .update_standard		= get_standard,
  .set_control			= set_control,
  .update_control		= get_control,
//  .set_preview_window =		tvengbktr_set_preview_window,
//  .get_preview_window =		tvengbktr_get_preview_window,
//  .set_preview =		tvengbktr_set_preview,

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
