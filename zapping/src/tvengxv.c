/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
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
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#include <tveng.h>

#ifdef USE_XV
#define TVENGXV_PROTOTYPES 1
#include "tvengxv.h"

#include "globals.h" /* xv_overlay_port */
#include "zmisc.h"

struct video_input {
	tv_video_line		pub;
	char			name[64];
	unsigned int		num;		/* random standard */
};

#define VI(l) PARENT (l, struct video_input, pub)

struct standard {
	tv_video_standard	pub;
	char			name[64];
	unsigned int		num;
};

#define S(l) PARENT (l, struct standard, pub)

struct control {
	tv_control		pub;
	Atom			atom;
};

#define C(l) PARENT (l, struct control, pub)

struct private_tvengxv_device_info
{
  tveng_device_info info; /* Info field, inherited */
  XvPortID	port; /* port id */
  XvEncodingInfo *ei; /* list of encodings, for reference */
  int encodings; /* number of encodings */
  int cur_encoding;
  /* This atoms define the controls */
	Atom			xa_encoding;
  int encoding_max, encoding_min, encoding_gettable;
  	Atom			xa_freq;
  int freq_max, freq_min;
	Atom			xa_mute;
	Atom			xa_volume;
	Atom			xa_colorkey;
  int colorkey_max, colorkey_min;
	Atom			xa_signal_strength;

	tv_bool			active;

	Window			window;
	GC			gc;

  Window last_win;
  GC last_gc;
  int last_w, last_h;
};

#define P_INFO(p) PARENT (p, struct private_tvengxv_device_info, info)

static int
find_encoding			(tveng_device_info *	info,
				 const char *		input,
				 const char *		standard)
{
	struct private_tvengxv_device_info * p_info = P_INFO (info);
	char encoding[200];
	unsigned int i;

	snprintf (encoding, 199, "%s-%s", standard, input);
	encoding[199] = 0;

	for (i = 0; i < p_info->encodings; ++i)
		if (0 == strcasecmp (encoding, p_info->ei[i].name))
			return i;

	return -1;
}

/* Splits "standard-input" storing "standard" in buffer d of size,
   returning pointer to "input". */
static const char *
split_encoding			(char *			d,
				 size_t			size,
				 const char *		s)
{
	for (; *s && *s != '-'; ++s)
		if (size > 1) {
			*d++ = *s;
			--size;
		}

	if (size > 0)
		*d = 0;

	if (*s == 0)
		return NULL;

	return ++s; /* skip '-' */
}

static int
p_tvengxv_open_device(tveng_device_info *info)
{
  Display *dpy = info->priv->display;
  Window root_window = DefaultRootWindow(dpy);
  unsigned int version, revision, major_opcode, event_base,
    error_base;
  int nAdaptors;
  int i,j;
  XvAttribute *at;
  int attributes;
  struct private_tvengxv_device_info *p_info =
    (struct private_tvengxv_device_info*) info;
  XvAdaptorInfo *pAdaptors, *pAdaptor;

  if (Success != XvQueryExtension(dpy, &version, &revision,
				  &major_opcode, &event_base,
				  &error_base))
    goto error1;

  if (info->debug_level > 0)
    fprintf(stderr, "tvengxv.c: XVideo major_opcode: %d\n",
	    major_opcode);

  if (version < 2 || (version == 2 && revision < 2))
    goto error1;

  if (Success != XvQueryAdaptors(dpy, root_window, &nAdaptors,
				 &pAdaptors))
    goto error1;

  if (nAdaptors <= 0)
    goto error1;

 retry:
  for (i=0; i<nAdaptors; i++)
    {
      pAdaptor = pAdaptors + i;
      if ((pAdaptor->type & XvInputMask) &&
	  (pAdaptor->type & XvVideoMask))
	{ /* available port found */
	  for (j=0; j<pAdaptor->num_ports; j++)
	    {
	      p_info->port = pAdaptor->base_id + j;

	      /* --xv-port option hack */
	      if (xv_overlay_port >= 0
		  && p_info->port != xv_overlay_port)
		continue;

	      if (Success == XvGrabPort(dpy, p_info->port, CurrentTime))
		goto adaptor_found;
	    }
	}
    }

  if (xv_overlay_port >= 0)
    {
      fprintf (stderr, "Xvideo overlay port #%d not found, "
	       "will try default. Available are:\n", xv_overlay_port);
      for (i=0; i<nAdaptors; i++)
	{
	  pAdaptor = pAdaptors + i;
	  if ((pAdaptor->type & XvInputMask) &&
	      (pAdaptor->type & XvVideoMask))
	    {
	      for (j=0; j<pAdaptor->num_ports; j++)
		fprintf (stderr, "%3d %s\n",
			 (int)(pAdaptor->base_id + j),
			 pAdaptor->name);
	    }
	}
      xv_overlay_port = -1;
      goto retry;
    }

  goto error2; /* no adaptors found */

  /* success */
 adaptor_found:
  /* Check that it supports querying controls and encodings */
  if (Success != XvQueryEncodings(dpy, p_info->port,
				  &p_info->encodings, &p_info->ei))
    goto error3;

  if (p_info->encodings <= 0)
    {
      info->tveng_errno = -1;
      t_error_msg("encodings",
		  "You have no encodings available",
		  info);
      goto error3;
    }

  /* create the atom that handles the encoding */
  at = XvQueryPortAttributes(dpy, p_info->port, &attributes);
  if ((!at) && (attributes <= 0))
    goto error4;

  XvFreeAdaptorInfo(pAdaptors);
  XvUngrabPort(dpy, p_info->port, CurrentTime);
  return 0xbeaf; /* the port seems to work ok, success */

 error4:
  if (p_info->ei)
    {
      XvFreeEncodingInfo(p_info->ei);
      p_info->ei = NULL;
    }
 error3:
  XvUngrabPort(dpy, p_info->port, CurrentTime);
 error2:
  XvFreeAdaptorInfo(pAdaptors);
 error1:
  return -1; /* failure */
}








/*
 *  Overlay
 */

static tv_bool
set_overlay_xwindow		(tveng_device_info *	info,
				 Window			window,
				 GC			gc)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);

	assert (!p_info->active);

	p_info->window = window;
	p_info->gc = gc;

	return TRUE;
}

static int
tvengxv_set_preview_window(tveng_device_info * info)
{
	/* Not used yet. */

	return 0;
}

static int
tvengxv_get_preview_window(tveng_device_info * info)
{
	/* Nothing to do. */

	return 0;
}

static tv_bool
set_overlay			(tveng_device_info *	info,
				 tv_bool		on)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);
	Window root;
	int encoding_num;
	int x, y;
	unsigned int width, height;
	unsigned int dummy;

  t_assert(info != NULL);

  	if (p_info->window == 0 || p_info->gc == 0) {
		info->tveng_errno = -1;
		t_error_msg("win", "The window value hasn't been set", info);
		return -1;
	}

	XGetGeometry (info->priv->display,
		      p_info->window,
		      &root, &x, &y, &width, &height,
		      /* border width */ &dummy,
		      /* depth */ &dummy);

	encoding_num = 0;

	if (p_info->xa_encoding != None
	    && p_info->encoding_gettable)
		XvGetPortAttribute (info->priv->display,
				    p_info->port,
				    p_info->xa_encoding,
				    &encoding_num);
	if (on) {
		XvPutVideo (info->priv->display,
			    p_info->port,
			    p_info->window,
			    p_info->gc,
			    /* src_x */ 0,
			    /* src_y */ 0,
			    /* src */ p_info->ei[encoding_num].width,
			    /* src */ p_info->ei[encoding_num].height,
			    /* dest */
			    0, 0, width, height);
	} else {
		XvStopVideo (info->priv->display,
			     p_info->port,
			     p_info->window);
	}

	XSync (info->priv->display, False);

	return 0;
}


static void
tvengxv_set_chromakey (uint32_t chroma, tveng_device_info *info)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);

  if (p_info->xa_colorkey != None)
    XvSetPortAttribute (info->priv->display, p_info->port,
			p_info->xa_colorkey, chroma);
}

static int
tvengxv_get_chromakey (uint32_t *chroma, tveng_device_info *info)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);

  if (p_info->xa_colorkey == None)
    return -1;

  XvGetPortAttribute (info->priv->display, p_info->port,
		      p_info->xa_colorkey, chroma);
  return 0;
}








/*
 *  Controls
 */

static const struct {
	const char *		atom;
	const char *		label;
	tv_control_id		id;
	tv_control_type		type;
} xv_attr_meta [] = {
	{ "XV_BRIGHTNESS", N_("Brightness"), TV_CONTROL_ID_BRIGHTNESS, TV_CONTROL_TYPE_INTEGER },
	{ "XV_CONTRAST",   N_("Contrast"),   TV_CONTROL_ID_CONTRAST,   TV_CONTROL_TYPE_INTEGER },
	{ "XV_SATURATION", N_("Saturation"), TV_CONTROL_ID_SATURATION, TV_CONTROL_TYPE_INTEGER },
	{ "XV_HUE",	   N_("Hue"),	     TV_CONTROL_ID_HUE,	       TV_CONTROL_TYPE_INTEGER },
	{ "XV_COLOR",	   N_("Color"),	     TV_CONTROL_ID_UNKNOWN,    TV_CONTROL_TYPE_INTEGER },
	{ "XV_INTERLACE",  N_("Interlace"),  TV_CONTROL_ID_UNKNOWN,    TV_CONTROL_TYPE_CHOICE },
	{ "XV_MUTE",	   N_("Mute"),	     TV_CONTROL_ID_MUTE,       TV_CONTROL_TYPE_BOOLEAN },
	{ "XV_VOLUME",	   N_("Volume"),     TV_CONTROL_ID_VOLUME,     TV_CONTROL_TYPE_INTEGER },
};

static tv_bool
do_update_control		(struct private_tvengxv_device_info *p_info,
				 struct control *	c)
{
	int value;

	// XXX check at runtime
	if (c->atom == p_info->xa_mute)
		return TRUE; /* no read-back (bttv bug) */
	else
		XvGetPortAttribute (p_info->info.priv->display,
				    p_info->port,
				    c->atom,
				    &value);

	if (c->pub.value != value) {
		c->pub.value = value;
		tv_callback_notify (&c->pub, c->pub._callback);
	}

	return TRUE; /* ? */
}

static tv_bool
update_control			(tveng_device_info *	info,
				 tv_control *		tc)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);

	if (tc) {
		return do_update_control (p_info, C(tc));
	} else {
		for_all (tc, p_info->info.controls)
			if (tc->_parent == info)
				if (!do_update_control (p_info, C(tc)))
					return FALSE;
	}

	return TRUE;
}

static int
set_control			(tveng_device_info *	info,
				 tv_control *		tc,
				 int			value)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);

	XvSetPortAttribute (info->priv->display,
			    p_info->port,
			    C(tc)->atom,
			    value);

	if (C(tc)->atom == p_info->xa_mute) {
		if (tc->value != value) {
			tc->value = value;
			tv_callback_notify (tc, tc->_callback);
		}

		return TRUE;
	}

	return do_update_control (p_info, C(tc));
}

static tv_bool
add_control			(struct private_tvengxv_device_info *p_info,
				 const char *		atom,
				 const char *		label,
				 tv_control_id		id,
				 tv_control_type	type,
				 int			minimum,
				 int			maximum,
				 int			step)
{
	struct control c;
	Atom xatom;

	CLEAR (c);

	xatom = XInternAtom (p_info->info.priv->display, atom, False);

	if (xatom == None)
		return TRUE;

	c.pub.type	= type;
	c.pub.id	= id;

	if (!(c.pub.label = strdup (_(label))))
		goto failure;

	c.pub.minimum	= minimum;
	c.pub.maximum	= maximum;
	c.pub.step	= step;

	c.atom		= xatom;

	if (0 == strcmp (atom, "XV_INTERLACE")) {
		if (!(c.pub.menu = calloc (4, sizeof (char *))))
			goto failure;

		if (!(c.pub.menu[0] = strdup (_("No")))
		    || !(c.pub.menu[1] = strdup (_("Yes")))
		    || !(c.pub.menu[2] = strdup (_("Doublescan"))))
			goto failure;
	}

	if (append_control (&p_info->info, &c.pub, sizeof (c)))
		return TRUE;

 failure:
	if (c.pub.menu) {
		free (c.pub.menu[0]);
		free (c.pub.menu[1]);
		free (c.pub.menu[2]);
		free (c.pub.menu);
	}
	
	free (c.pub.label);
	
	return FALSE;
}

/*
 *  Video standards
 */

/* Encodings we can translate to tv_video_standard_id. Other
   encodings will be flagged as custom standard. */
static const struct {
	const char *		name;
	const char *		label;
	tv_video_standard_id	id;
} standards [] = {
	{ "pal",	"PAL",		TV_VIDEOSTD_PAL },
	{ "ntsc",	"NTSC",		TV_VIDEOSTD_NTSC_M },
	{ "secam",	"SECAM",	TV_VIDEOSTD_SECAM },
	{ "palnc",	"PAL-NC",	TV_VIDEOSTD_PAL_NC },
	{ "palm",	"PAL-M",	TV_VIDEOSTD_PAL_M },
	{ "paln",	"PAL-N",	TV_VIDEOSTD_PAL_N },
	{ "ntscjp",	"NTSC-JP",	TV_VIDEOSTD_NTSC_M_JP },
};

static tv_bool
update_standard_list		(tveng_device_info *	info)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);
	const char *cur_input;
	unsigned int custom;
	unsigned int i;

	free_video_standards (info);

	if (p_info->xa_encoding == None)
		return TRUE;

	if (!(cur_input = split_encoding
	      (NULL, 0, p_info->ei[p_info->cur_encoding].name)))
		return TRUE;

	custom = 32;

	for (i = 0; i < p_info->encodings; ++i) {
		struct standard *s;
		char buf[sizeof (s->name)];
		const char *input;
		unsigned int j;

		if (!(input = split_encoding (buf, sizeof (buf),
					      p_info->ei[i].name)))
			continue;

		if (0 != strcmp (input, cur_input))
			continue;

		if (buf[0] == 0)
			continue;

		for (j = 0; j < N_ELEMENTS (standards); ++j)
			if (0 == strcmp (buf, standards[j].name))
				break;

		if (j < N_ELEMENTS (standards)) {
			s = S(append_video_standard (&info->video_standards,
						     standards[j].id,
						     standards[j].label,
						     standards[j].name,
						     sizeof (*s)));
		} else {
			char up[sizeof (buf)];

			if (custom >= sizeof (tv_video_standard_id) * 8)
				continue;

			for (j = 0; buf[j]; ++j)
				up[j] = toupper (buf[j]);

			up[j] = 0;

			s = S(append_video_standard (&info->video_standards,
						     1 << (custom++),
						     up, buf,
						     sizeof (*s)));
		}

		if (s == NULL) {
			free_video_standard_list (&info->video_standards);
			return FALSE;
		}

		z_strlcpy (s->name, buf, sizeof (s->name));

		s->num = i;
	}

	return TRUE;
}

static tv_bool
set_standard			(tveng_device_info *	info,
				 const tv_video_standard *ts)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);

	XvSetPortAttribute (info->priv->display,
			    p_info->port,
			    p_info->xa_encoding,
			    S(ts)->num);

	p_info->cur_encoding = S(ts)->num;

	set_cur_video_standard (info, ts);

	return TRUE;
}

/*
 *  Video inputs
 */

static struct video_input *
find_video_input		(tv_video_line *	list,
				 const char *		input)
{
	for_all (list, list) {
		struct video_input *vi = VI(list);

		if (0 == strcmp (vi->name, input))
			return vi;
	}

	return NULL;
}

static tv_bool
update_video_input_list		(tveng_device_info *	info)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);
	unsigned int i;

	free_video_inputs (info);

	if (p_info->xa_encoding == None)
		return TRUE;

	for (i = 0; i < p_info->encodings; ++i) {
		struct video_input *vi;
		char buf[100];
		const char *input;
		tv_video_line_type type;

		if (info->debug_level > 0)
			fprintf (stderr, "  TVeng Xv input #%d: %s\n",
				 i, p_info->ei[i].name);

		if (!(input = split_encoding (NULL, 0,
					      p_info->ei[i].name)))
			continue;

		if (find_video_input (info->video_inputs, input))
			continue;

		/* FIXME */
		if (p_info->xa_freq != None)
			type = TV_VIDEO_LINE_TYPE_TUNER;
		else
			type = TV_VIDEO_LINE_TYPE_BASEBAND;

		z_strlcpy (buf, input, sizeof (buf));
		buf[0] = toupper (buf[0]);

		vi = VI(append_video_line (&info->video_inputs,
					   type, buf, input, sizeof (*vi)));

		if (!vi) {
			free_video_line_list (&info->video_inputs);
			return FALSE;
		}

		z_strlcpy (vi->name, input, sizeof (vi->name));

		vi->num = i;
	}

	return TRUE;
}

#if 0
      /* The XVideo extension provides very little info about encodings,
	 we must just make something up */
      if (p_info->freq != None)
        {
	  /* this encoding may refer to a baseband input though */
          info->inputs[info->num_inputs].tuners = 1;
          info->inputs[info->num_inputs].flags |= TVENG_INPUT_TUNER;
          info->inputs[info->num_inputs].type = TVENG_INPUT_TYPE_TV;
	}
      else
        {
          info->inputs[info->num_inputs].tuners = 0;
          info->inputs[info->num_inputs].type = TVENG_INPUT_TYPE_CAMERA;
	}
      if (p_info->volume != None || p_info->mute != None)
        info->inputs[info->num_inputs].flags |= TVENG_INPUT_AUDIO;
      snprintf(info->inputs[info->num_inputs].name, 32, input);
      info->inputs[info->num_inputs].name[31] = 0;
      info->inputs[info->num_inputs].hash =
	tveng_build_hash(info->inputs[info->num_inputs].name);
#endif

/* Cannot use the generic helper functions, we must set video
   standard and input at the same time. */
static void
set_source			(tveng_device_info *	info,
				 const tv_video_line *	input,
				 const tv_video_standard *standard)
{
	tv_video_line *old_input;
	tv_video_standard *old_standard;

	old_input = info->cur_video_input;
	info->cur_video_input = (tv_video_line *) input;

	old_standard = info->cur_video_standard;
	info->cur_video_standard = (tv_video_standard *) standard;

	if (old_input != input)
		tv_callback_notify (info, info->priv->video_input_callback);

	if (old_standard != standard)
		tv_callback_notify (info, info->priv->video_standard_callback);
}

static tv_bool
update_current_video_input	(tveng_device_info *	info)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);
	struct video_input *vi;
	const char *input;
	tv_video_standard *ts;

	if (p_info->xa_encoding == None
	    || !p_info->encoding_gettable) {
		set_source (info, NULL, NULL);
		return TRUE;
	}

	XvGetPortAttribute (info->priv->display,
			    p_info->port,
			    p_info->xa_encoding,
			    &p_info->cur_encoding);

#warning
	/* Xv/v4l BUG? */
	if (p_info->cur_encoding < 0 || p_info->cur_encoding > 10 /*XXX*/)
		p_info->cur_encoding = 0;

	input = split_encoding (NULL, 0, p_info->ei[p_info->cur_encoding].name);

	vi = find_video_input (info->video_inputs, input);

	assert (vi != NULL);

	update_standard_list (info);

	for_all (ts, info->video_standards)
		if (S(ts)->num == p_info->cur_encoding)
			break;

	set_source (info, &vi->pub, ts);

	return TRUE;
}

static tv_bool
set_video_input			(tveng_device_info *	info,
				 const tv_video_line *	tl)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);
	struct video_input *vi;
	const tv_video_standard *ts;
	int num;

	vi = VI(tl);
	num = -1;

	if (info->cur_video_standard) {
		struct standard *s;

		/* Keep standard if possible. */

		s = S(info->cur_video_standard);
		num = find_encoding (info, vi->name, s->name);
	}

	if (num == -1) {
		num = vi->num; /* random standard */

		XvSetPortAttribute (info->priv->display,
				    p_info->port,
				    p_info->xa_encoding,
				    num);

		update_standard_list (info);
	} else {
		XvSetPortAttribute (info->priv->display,
				    p_info->port,
				    p_info->xa_encoding,
				    num);
	}

	p_info->cur_encoding = num;

	for_all (ts, info->video_standards)
		if (S(ts)->num == num)
			break;

	set_source (info, tl, ts);

	return TRUE;
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
int tvengxv_attach_device(const char* device_file,
			  enum tveng_attach_mode attach_mode,
			  tveng_device_info * info)
{
  struct private_tvengxv_device_info * p_info =
    (struct private_tvengxv_device_info *)info;
  Display *dpy;
  extern int disable_overlay;
  XvAttribute *at;
  int num_attributes;
  int i, j;

  t_assert(info != NULL);

  if (info->priv->disable_xv || disable_overlay)
    {
      info->tveng_errno = -1;
      t_error_msg("disable_xv",
		  "XVideo support has been disabled", info);
      return -1;
    }

  dpy = info->priv->display;

  if (info -> fd) /* If the device is already attached, detach it */
    tveng_close_device(info);

  /* clear the atoms */
  p_info->xa_encoding = None;
  p_info->xa_freq = None;
  p_info->xa_mute = None;
  p_info->xa_volume = None;
  p_info->xa_colorkey = None;
  p_info->xa_signal_strength = None;
  p_info->ei = NULL;
  p_info->cur_encoding = 0;

  /* In this module, the given device file doesn't matter */
  info -> file_name = strdup("XVideo");
  if (!(info -> file_name))
    {
      perror("strdup");
      info->tveng_errno = errno;
      snprintf(info->error, 256, "Cannot duplicate device name");
      goto error1;
    }
  switch (attach_mode)
    {
      /* In V4L there is no control-only mode */
    case TVENG_ATTACH_XV:
      info -> fd = p_tvengxv_open_device(info);
      break;
    default:
      t_error_msg("switch()", "This module only supports TVENG_ATTACH_XV",
		  info);
      goto error1;
    };

  if (info -> fd < 0)
    goto error1;

  info->fd = 0;
  
  info -> attach_mode = attach_mode;
  /* Current capture mode is no capture at all */
  info -> current_mode = TVENG_NO_CAPTURE;

  info->caps.flags = TVENG_CAPS_OVERLAY | TVENG_CAPS_CLIPPING;
  info->caps.audios = 0;

  /* Atoms & controls */

  info->controls = NULL;

  at = XvQueryPortAttributes (dpy, p_info->port, &num_attributes);

  for (i = 0; i < num_attributes; i++) {
	  if (info->debug_level > 0)
		  fprintf(stderr, "  TVeng Xv atom: %s %c/%c (%i -> %i)\n",
			  at[i].name,
			  (at[i].flags & XvGettable) ? 'r' : '-',
			  (at[i].flags & XvSettable) ? 'w' : '-',
			  at[i].min_value, at[i].max_value);

	  if (!strcmp("XV_ENCODING", at[i].name)) {
		  if (!(at[i].flags & XvSettable))
			  continue;

		  p_info->xa_encoding = XInternAtom (dpy, "XV_ENCODING", False);
		  p_info->encoding_max = at[i].max_value;
		  p_info->encoding_min = at[i].min_value;
		  p_info->encoding_gettable = at[i].flags & XvGettable;
		  continue;
	  } else if (!strcmp("XV_SIGNAL_STRENGTH", at[i].name)) {
		  if (!(at[i].flags & XvGettable))
			  continue;

		  p_info->xa_signal_strength =
		  	XInternAtom (dpy, "XV_SIGNAL_STRENGTH", False);
		  continue;
	  }

	  if ((at[i].flags & (XvGettable | XvSettable)) != (XvGettable | XvSettable))
		  continue;
		  
	  if (!strcmp("XV_FREQ", at[i].name)) {
		  info->caps.flags |= TVENG_CAPS_TUNER;

		  p_info->xa_freq = XInternAtom (dpy, "XV_FREQ", False);
		  p_info->freq_max = at[i].max_value;
		  p_info->freq_min = at[i].min_value;
		  continue;
	  } else if (!strcmp("XV_COLORKEY", at[i].name)) {
		  info->caps.flags = TVENG_CAPS_CHROMAKEY;

		  p_info->xa_colorkey = XInternAtom (dpy, "XV_COLORKEY", False);
		  p_info->colorkey_max = at[i].max_value;
		  p_info->colorkey_min = at[i].min_value;
		  continue;
	  }

	  if (0 == strcmp ("XV_MUTE", at[i].name)) {
		  p_info->xa_mute = XInternAtom (dpy, "XV_MUTE", False);
	  } else if (0 == strcmp ("XV_VOLUME", at[i].name)) {
		  p_info->xa_volume = XInternAtom (dpy, "XV_VOLUME", False);
	  }

	  for (j = 0; j < N_ELEMENTS (xv_attr_meta); j++) {
		  if (0 == strcmp (xv_attr_meta[j].atom, at[i].name)) {
			  int step;

			  /* Not reported, let's make something up. */
			  step = (at[i].max_value - at[i].min_value) / 100;

			  /* Error ignored */
			  add_control (p_info,
				       xv_attr_meta[j].atom,
				       xv_attr_meta[j].label,
				       xv_attr_meta[j].id,
				       xv_attr_meta[j].type,
				       at[i].min_value,
				       at[i].max_value,
				       step);
		  }
	  }
  }

      /* Set the mute control to OFF (workaround for BTTV bug) */
//      tveng_set_control(&control, 0, info);

  /* fill in with the proper values */
  update_control (info, NULL /* all */);

  /* We have a valid device, get some info about it */
  info->current_controller = TVENG_CONTROLLER_XV;

	/* Video inputs & standards */

	info->video_inputs = NULL;
	info->cur_video_input = NULL;

	info->video_standards = NULL;
	info->cur_video_standard = NULL;

	if (!update_video_input_list (info))
		goto error1; // XXX

	if (!update_current_video_input (info))
		goto error1; // XXX

  /* fill in capabilities info */
	info->caps.channels = 0; // FIXME info->num_inputs;
  /* Let's go creative! */
  snprintf(info->caps.name, 32, "XVideo device");
  info->caps.minwidth = 1;
  info->caps.minheight = 1;
  info->caps.maxwidth = 32768;
  info->caps.maxheight = 32768;

  return info -> fd;

 error1:
  if (info->file_name)
    free(info->file_name);
  info->file_name = NULL;
  return -1;
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
tvengxv_describe_controller(char ** short_str, char ** long_str,
			   tveng_device_info * info)
{
  t_assert(info != NULL);
  if (short_str)
    *short_str = "XV";
  if (long_str)
    *long_str = "XVideo extension";
}

/* Closes a device opened with tveng_init_device */
static void tvengxv_close_device(tveng_device_info * info)
{
  struct private_tvengxv_device_info *p_info=
    (struct private_tvengxv_device_info*) info;

  t_assert(info != NULL);

  tveng_stop_everything(info);

  if (p_info->ei)
    XvFreeEncodingInfo(p_info->ei);
  p_info->ei = NULL;

  info -> fd = 0;
  info -> current_controller = TVENG_CONTROLLER_NONE;

  if (info -> file_name)
    free(info -> file_name);

	free_video_standards (info);
	free_video_inputs (info);
	free_controls (info);

  /* clear the atoms */

  info -> file_name = NULL;
}





static int
tvengxv_set_capture_format(tveng_device_info * info)
{
  return 0; /* this just doesn't make sense in XVideo */
}

static int
tvengxv_update_capture_format(tveng_device_info * info)
{
  return 0; /* This one was easy too :-) */
}









static int
tvengxv_tune_input(uint32_t freq, tveng_device_info *info)
{
  struct private_tvengxv_device_info * p_info =
    (struct private_tvengxv_device_info*)info;

  t_assert(info != NULL);

  if (p_info->xa_freq != None)
    XvSetPortAttribute(info->priv->display,
		       p_info->port,
		       p_info->xa_freq,
		       freq*0.016);

  XSync(info->priv->display, False);

  return 0;
}

static int
tvengxv_get_signal_strength(int *strength, int *afc,
			    tveng_device_info * info)
{
  struct private_tvengxv_device_info * p_info =
    (struct private_tvengxv_device_info*)info;

  if (p_info->xa_signal_strength == None)
    {
      info->tveng_errno = -1;
      t_error_msg("XVideo",
		  "\"XV_SIGNAL_STRENGTH\" atom not provided by "
		  "the XVideo driver", info);
      return -1;
    }

  if (strength)
    XvGetPortAttribute(info->priv->display,
		       p_info->port,
		       p_info->xa_signal_strength,
		       strength);

  if (afc)
    *afc = 0;
  return 0;
}

static int
tvengxv_get_tune(uint32_t * freq, tveng_device_info *info)
{
  struct private_tvengxv_device_info * p_info =
    (struct private_tvengxv_device_info*)info;

  t_assert(info != NULL);
  if (!freq || p_info->xa_freq == None)
    return 0;

  XvGetPortAttribute(info->priv->display,
		     p_info->port,
		     p_info->xa_freq,
		     (int*)(freq));

  *freq = *freq / 0.016;

  return 0;
}

static int
tvengxv_get_tuner_bounds(uint32_t * min, uint32_t * max, tveng_device_info *
			 info)
{
  struct private_tvengxv_device_info * p_info =
    (struct private_tvengxv_device_info*)info;

  t_assert(info != NULL);

  if (min)
    *min = p_info->freq_min;
  if (max)
    *max = p_info->freq_max;

  return 0;
}



static struct tveng_module_info tvengxv_module_info = {
  .attach_device =		tvengxv_attach_device,
  .describe_controller =	tvengxv_describe_controller,
  .close_device =		tvengxv_close_device,
  .update_video_input		= update_current_video_input,
  .set_video_input		= set_video_input,
  /* Video input and standard combine as "encoding", update_video_input
     also determines the current standard. */
  .update_standard		= NULL,
  .set_standard			= set_standard,
  .update_capture_format =	tvengxv_update_capture_format,
  .set_capture_format =		tvengxv_set_capture_format,
  .update_control		= update_control,
  .set_control			= set_control,
  .tune_input =			tvengxv_tune_input,
  .get_signal_strength =	tvengxv_get_signal_strength,
  .get_tune =			tvengxv_get_tune,
  .get_tuner_bounds =		tvengxv_get_tuner_bounds,
  .get_overlay_buffer		= NULL,
  .set_overlay_xwindow		= set_overlay_xwindow,
  .set_preview_window =		tvengxv_set_preview_window,
  .get_preview_window =		tvengxv_get_preview_window,
  .set_overlay			= set_overlay,
  .get_chromakey =		tvengxv_get_chromakey,
  .set_chromakey =		tvengxv_set_chromakey,

  .private_size =		sizeof(struct private_tvengxv_device_info)
};

/*
  Inits the XV module, and fills in the given table.
*/
void tvengxv_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

  memcpy(module_info, &tvengxv_module_info,
	 sizeof(struct tveng_module_info));
}
#else /* do not use the XVideo extension */
#include "tvengxv.h"
void tvengxv_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

  memset(module_info, 0, sizeof(struct tveng_module_info));
}
#endif
