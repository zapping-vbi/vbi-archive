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

#include "globals.h" /* xv_video_port */
#include "zmisc.h"

struct video_input {
	tv_video_line		pub;
	char			name[64];
	unsigned int		num;		/* random standard */
};

#define VI(l) PARENT (l, struct video_input, pub)
#define CVI(l) CONST_PARENT (l, struct video_input, pub)

struct standard {
	tv_video_standard	pub;
	char			name[64];
	unsigned int		num;
};

#define S(l) PARENT (l, struct standard, pub)
#define CS(l) CONST_PARENT (l, struct standard, pub)

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
  unsigned int cur_encoding;
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

	for (i = 0; i < (unsigned int) p_info->encodings; ++i)
		if (0 == strcasecmp (encoding, p_info->ei[i].name))
			return i;

	return -1;
}

/* Splits "standard-input" storing "standard" in buffer d of size
   (can be 0), returning pointer to "input". Returns NULL on error. */
static const char *
split_encoding			(char *			d,
				 size_t			size,
				 const char *		s)
{
	for (; *s && *s != '-'; ++s)
		if (size > 1) {
			*d++ = *s;
			--size;
		} else if (size > 0) {
			return NULL;
		}

	if (size > 0)
		*d = 0;

	if (*s == 0)
		return NULL;

	return ++s; /* skip '-' */
}

#define NO_PORT ((XvPortID) None)
#define ANY_PORT ((XvPortID) None)

static XvPortID
grab_port			(Display *		display,
				 const XvAdaptorInfo *	pAdaptors,
				 int			nAdaptors,
				 XvPortID		port_id)
{
  int i;

  for (i = 0; i < nAdaptors; ++i)
    {
      const XvAdaptorInfo *pAdaptor;
      unsigned int type;

      pAdaptor = pAdaptors + i;

      type = pAdaptor->type;

      if (0 == strcmp (pAdaptor->name, "NVIDIA Video Interface Port")
	  && type == (XvInputMask | XvVideoMask))
	type = XvOutputMask | XvVideoMask; /* Bug. This is TV out. */

      if ((XvInputMask | XvVideoMask)
	  == (type & (XvInputMask | XvVideoMask)))
	{
	  if (ANY_PORT == port_id)
	    {
	      unsigned int i;

	      for (i = 0; i < pAdaptor->num_ports; ++i)
		if (Success == XvGrabPort (display, pAdaptor->base_id + i,
					   CurrentTime))
		  return (XvPortID)(pAdaptor->base_id + i);
	    }
	  else
	    {
	      if (port_id >= pAdaptor->base_id
		  && port_id < (pAdaptor->base_id + pAdaptor->num_ports))
		{
		  if (Success == XvGrabPort (display, port_id, CurrentTime))
		    return port_id;
		  else
		    return NO_PORT;
		}
	    }
	}
    }

  return NO_PORT;
}

static int
p_tvengxv_open_device(tveng_device_info *info,
		      Window window)
{
  struct private_tvengxv_device_info *p_info = P_INFO (info);
  Display *display;
  unsigned int version;
  unsigned int revision;
  unsigned int major_opcode;
  unsigned int event_base;
  unsigned int error_base;
  XvAdaptorInfo *pAdaptors;
  int nAdaptors;
  XvAttribute *pAttributes;
  int nAttributes;

  printv ("xv_video_port 0x%x\n", xv_video_port);

  display = info->display;

  pAdaptors = NULL;
  pAttributes = NULL;

  nAdaptors = 0;
  nAttributes = 0;

  p_info->port = NO_PORT;
  p_info->encodings = 0;

  if (Success != XvQueryExtension (display,
				   &version, &revision,
				   &major_opcode,
				   &event_base, &error_base))
    {
      printv ("XVideo extension not available\n");
      goto failure;
    }

  printv ("XVideo opcode %d, base %d, %d, version %d.%d\n",
	  major_opcode,
	  event_base, error_base,
	  version, revision);

  if (version < 2 || (version == 2 && revision < 2))
    {
      printv ("XVideo extension not usable\n");
      goto failure;
    }

  /* We query adaptors which can render into this window. */
  if (None == window)
    window = DefaultRootWindow (display);

  if (Success != XvQueryAdaptors (display, window, &nAdaptors, &pAdaptors))
    {
      printv ("XvQueryAdaptors failed\n");
      goto failure;
    }

  if (nAdaptors <= 0)
    {
      printv ("No XVideo adaptors\n");
      goto failure;
    }

  p_info->port = grab_port (display, pAdaptors, nAdaptors,
			    (XvPortID) xv_video_port);

  if (NO_PORT == p_info->port
      && ANY_PORT != xv_video_port)
    {
      printv ("XVideo video input port 0x%x not found\n",
	      (unsigned int) xv_video_port);

      p_info->port = grab_port (display, pAdaptors, nAdaptors, ANY_PORT);
    }

  if (NO_PORT == p_info->port)
    {
      printv ("No XVideo input port found\n");
      goto failure;
    }

  printv ("Using XVideo video input port 0x%x\n",
	  (unsigned int) p_info->port);

  /* Check that it supports querying controls and encodings */
  if (Success != XvQueryEncodings(display, p_info->port,
				  &p_info->encodings,
				  &p_info->ei))
    goto failure;

  if (p_info->encodings <= 0)
    {
      info->tveng_errno = -1;
      t_error_msg("encodings",
		  "You have no encodings available",
		  info);
      goto failure;
    }

  /* create the atom that handles the encoding */
  pAttributes = XvQueryPortAttributes(display, p_info->port, &nAttributes);

  if (nAttributes <= 0)
    goto failure;

  XFree (pAttributes);

  XvFreeAdaptorInfo (pAdaptors);

  XvUngrabPort (display, p_info->port, CurrentTime);

  return 0xbeaf; /* the port seems to work ok, success */

 failure:
  if (nAttributes > 0)
    XFree (pAttributes);

  if (p_info->encodings > 0)
    {
      XvFreeEncodingInfo (p_info->ei);
      p_info->ei = NULL;
      p_info->encodings = 0;
    }

  if (NO_PORT != p_info->port)
    {
      XvUngrabPort (display, p_info->port, CurrentTime);
      p_info->port = NO_PORT;
    }

  if (nAdaptors > 0)
    XvFreeAdaptorInfo (pAdaptors);

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

static tv_bool
enable_overlay			(tveng_device_info *	info,
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
		return FALSE;
	}

	XGetGeometry (info->display,
		      p_info->window,
		      &root, &x, &y, &width, &height,
		      /* border width */ &dummy,
		      /* depth */ &dummy);

	encoding_num = 0;

	if (p_info->xa_encoding != None
	    && p_info->encoding_gettable)
		XvGetPortAttribute (info->display,
				    p_info->port,
				    p_info->xa_encoding,
				    &encoding_num);
	if (on) {
		XvPutVideo (info->display,
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
		XvStopVideo (info->display,
			     p_info->port,
			     p_info->window);
	}

	XSync (info->display, False);

	return TRUE;
}

static tv_bool
set_overlay_window_chromakey (tveng_device_info *info,
			      const tv_window *w _unused_,
			      unsigned int chromakey)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);

	if (p_info->xa_colorkey != None) {
		if (io_debug_msg > 0) {
			fprintf (stderr, "XvSetPortAttribute "
				 "XA_COLORKEY 0x%x\n", chromakey);
		}

		XvSetPortAttribute (info->display,
				    p_info->port,
				    p_info->xa_colorkey,
				    (int) chromakey);

		return TRUE;
	}

	return FALSE;
}

static tv_bool
get_overlay_chromakey (tveng_device_info *info)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);
	int result;

  if (p_info->xa_colorkey == None)
    return FALSE;

  XvGetPortAttribute (info->display, p_info->port,
		      p_info->xa_colorkey, &result);

  info->overlay.chromakey = result;

  return TRUE;
}




/*
 *  Controls
 */

static tv_bool
do_get_control			(struct private_tvengxv_device_info *p_info,
				 struct control *	c)
{
	int value;

	/* XXX check at runtime */
	if (c->atom == p_info->xa_mute) {
		return TRUE; /* no read-back (bttv bug) */
	} else {
		XvGetPortAttribute (p_info->info.display,
				    p_info->port,
				    c->atom,
				    &value);

		if (io_debug_msg > 0) {
			fprintf (stderr, "XvGetPortAttribute %s %d\n",
				 c->pub.label, value);
		}
	}

	if (c->pub.value != value) {
		c->pub.value = value;
		tv_callback_notify (&p_info->info, &c->pub, c->pub._callback);
	}

	return TRUE; /* ? */
}

static tv_bool
get_control			(tveng_device_info *	info,
				 tv_control *		c)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);

	if (c)
		return do_get_control (p_info, C(c));

	for_all (c, p_info->info.controls)
		if (c->_parent == info)
			if (!do_get_control (p_info, C(c)))
				return FALSE;
	return TRUE;
}

static int
set_control			(tveng_device_info *	info,
				 tv_control *		c,
				 int			value)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);

	if (io_debug_msg > 0) {
		fprintf (stderr, "XvSetPortAttribute %s %d\n",
			 c->label, value);
	}

	XvSetPortAttribute (info->display,
			    p_info->port,
			    C(c)->atom,
			    value);

	if (C(c)->atom == p_info->xa_mute) {
		if (c->value != value) {
			c->value = value;
			tv_callback_notify (info, c, c->_callback);
		}

		return TRUE;
	}

	return do_get_control (p_info, C(c));
}

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

	xatom = XInternAtom (p_info->info.display, atom, False);

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

static tv_bool
set_video_standard		(tveng_device_info *	info,
				 const tv_video_standard *s)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);

	if (io_debug_msg > 0) {
		fprintf (stderr, "XvSetPortAttribute XA_ENCODING %s\n",
			 s->label);
	}

	XvSetPortAttribute (info->display,
			    p_info->port,
			    p_info->xa_encoding,
			    (int) CS(s)->num);

	p_info->cur_encoding = CS(s)->num;

	store_cur_video_standard (info, s);

	return TRUE;
}

/* Encodings we can translate to tv_video_standard_id. Other
   encodings will be flagged as custom standard. */
static const struct {
	const char *		name;
	const char *		label;
	tv_videostd_set		set;
} standards [] = {
	{ "pal",	"PAL",		TV_VIDEOSTD_SET_PAL },
	{ "ntsc",	"NTSC",		TV_VIDEOSTD_SET (TV_VIDEOSTD_NTSC_M) },
	{ "secam",	"SECAM",	TV_VIDEOSTD_SET_SECAM },
	{ "palnc",	"PAL-NC",	TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_NC) },
	{ "palm",	"PAL-M",	TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_M) },
	{ "paln",	"PAL-N",	TV_VIDEOSTD_SET (TV_VIDEOSTD_PAL_N) },
	{ "ntscjp",	"NTSC-JP",	TV_VIDEOSTD_SET (TV_VIDEOSTD_NTSC_M_JP) },
};

static tv_bool
get_video_standard_list		(tveng_device_info *	info)
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

	for (i = 0; i < (unsigned int) p_info->encodings; ++i) {
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
						     standards[j].set,
						     standards[j].label,
						     standards[j].name,
						     sizeof (*s)));
		} else {
			char up[sizeof (buf)];

			if (custom >= TV_MAX_VIDEOSTDS)
				continue;

			for (j = 0; buf[j]; ++j)
				up[j] = toupper (buf[j]);

			up[j] = 0;

			s = S(append_video_standard
			      (&info->video_standards,
			       TV_VIDEOSTD_SET (1) << (custom++),
			       up, buf,
			       sizeof (*s)));
		}

		if (s == NULL)
			goto failure;

		z_strlcpy (s->name, buf, sizeof (s->name));

		s->num = i;
	}

	return TRUE;

 failure:
	free_video_standard_list (&info->video_standards);
	return FALSE;
}

/*
 *  Video inputs
 */

static tv_bool
get_video_input			(tveng_device_info *	info);

static void
store_frequency			(tveng_device_info *	info,
				 struct video_input *	vi,
				 int			freq)
{
	unsigned int frequency = freq * 62500;

	if (vi->pub.u.tuner.frequency != frequency) {
		vi->pub.u.tuner.frequency = frequency;
		tv_callback_notify (info, &vi->pub, vi->pub._callback);
	}
}

static tv_bool
get_tuner_frequency		(tveng_device_info *	info,
				 tv_video_line *	l)
{
	struct private_tvengxv_device_info * p_info = P_INFO (info);
	int freq;

	if (p_info->xa_freq == None)
		return FALSE;

	if (!get_video_input (info))
		return FALSE;

	if (info->cur_video_input == l) {
		XvGetPortAttribute (info->display,
				    p_info->port,
				    p_info->xa_freq,
				    &freq);

		store_frequency (info, VI (l), freq);
	}

	return TRUE;
}

static tv_bool
set_tuner_frequency		(tveng_device_info *	info,
				 tv_video_line *	l,
				 unsigned int		frequency)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);
	int freq;

	if (p_info->xa_freq == None)
		return FALSE;

	if (!get_video_input (info))
		return FALSE;

	freq = frequency / 62500;

	if (info->cur_video_input != l)
		goto store;

	if (io_debug_msg > 0) {
		fprintf (stderr, "XvSetPortAttribute XA_FREQ %d\n", freq);
	}

	XvSetPortAttribute (info->display,
			    p_info->port,
			    p_info->xa_freq,
			    freq);

	XSync (info->display, False);

 store:
	store_frequency (info, VI (l), freq);
	return TRUE;
}

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
		tv_callback_notify (info, info, info->video_input_callback);

	if (old_standard != standard)
		tv_callback_notify (info, info, info->video_standard_callback);
}

static tv_bool
get_video_input			(tveng_device_info *	info)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);
	struct video_input *vi;
	const char *input;
	tv_video_standard *ts;
	int enc;

	if (p_info->xa_encoding == None
	    || !p_info->encoding_gettable) {
		set_source (info, NULL, NULL);
		return TRUE;
	}

	XvGetPortAttribute (info->display,
			    p_info->port,
			    p_info->xa_encoding,
			    &enc);

	/* XXX Xv/v4l BUG? */
	if (enc < 0 || enc > 10 /*XXX*/)
		p_info->cur_encoding = 0;
	else
		p_info->cur_encoding = enc;

	input = split_encoding (NULL, 0,
				p_info->ei[p_info->cur_encoding].name);

	vi = find_video_input (info->video_inputs, input);

	assert (vi != NULL);

	get_video_standard_list (info);

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
	const struct video_input *vi;
	const tv_video_standard *ts;
	int num;

	vi = CVI(tl);
	num = -1;

	if (info->cur_video_standard) {
		struct standard *s;

		/* Keep standard if possible. */

		s = S(info->cur_video_standard);
		num = find_encoding (info, vi->name, s->name);
	}

	if (num == -1) {
		num = vi->num; /* random standard */

		if (io_debug_msg > 0) {
			fprintf (stderr, "XvSetPortAttribute XA_ENCODING %d\n",
				 num);
		}

		XvSetPortAttribute (info->display,
				    p_info->port,
				    p_info->xa_encoding,
				    num);

		get_video_standard_list (info);
	} else {
		if (io_debug_msg > 0) {
			fprintf (stderr, "XvSetPortAttribute XA_ENCODING %d\n",
				 num);
		}

		XvSetPortAttribute (info->display,
				    p_info->port,
				    p_info->xa_encoding,
				    num);
	}

	p_info->cur_encoding = num;

	for_all (ts, info->video_standards)
		if (CS(ts)->num == p_info->cur_encoding)
			break;

	set_source (info, tl, ts);

	/* Xv does not promise per-tuner frequency setting as we do.
	   XXX ignores the possibility that a third
	   party changed the frequency from the value we know. */
	if (IS_TUNER_LINE (tl))
		set_tuner_frequency (info, info->cur_video_input,
				     info->cur_video_input->u.tuner.frequency);

	return TRUE;
}

static tv_bool
get_video_input_list		(tveng_device_info *	info)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);
	unsigned int i;

	free_video_inputs (info);

	if (p_info->xa_encoding == None)
		return TRUE;

	for (i = 0; i < (unsigned int) p_info->encodings; ++i) {
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

		if (!(vi = VI(append_video_line (&info->video_inputs,
						 type, buf, input, sizeof (*vi)))))
			goto failure;

		vi->pub._parent = info;

		z_strlcpy (vi->name, input, sizeof (vi->name));

		vi->num = i;

		if (vi->pub.type == TV_VIDEO_LINE_TYPE_TUNER) {
			/* FIXME */
#if 0 /* Xv/v4l reports bogus maximum */
			vi->pub.u.tuner.minimum = p_info->freq_min * 1000;
			vi->pub.u.tuner.maximum = p_info->freq_max * 1000;
#else
			vi->pub.u.tuner.minimum = 0;
			vi->pub.u.tuner.maximum = INT_MAX - (INT_MAX % 62500);
			/* NB freq attr is int */
#endif
			vi->pub.u.tuner.step = 62500;
			vi->pub.u.tuner.frequency = vi->pub.u.tuner.minimum;
		}
	}

	if (!get_video_input (info))
		goto failure;

	return TRUE;

 failure:
	free_video_line_list (&info->video_inputs);
	return FALSE;
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
int tvengxv_attach_device(const char* device_file _unused_,
			  Window window,
			  enum tveng_attach_mode attach_mode,
			  tveng_device_info * info)
{
  struct private_tvengxv_device_info * p_info =
    (struct private_tvengxv_device_info *)info;
  Display *dpy;
  extern int disable_overlay;
  XvAttribute *at;
  int num_attributes;
  int i;
  unsigned int j;

  t_assert(info != NULL);

  if (info->disable_xv_video || disable_overlay)
    {
      info->tveng_errno = -1;
      t_error_msg("disable_xv",
		  "XVideo support has been disabled", info);
      return -1;
    }

  dpy = info->display;

  if (-1 != info -> fd) /* If the device is already attached, detach it */
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
      info -> fd = p_tvengxv_open_device(info,window);
      break;
    default:
      t_error_msg("switch()", "This module only supports TVENG_ATTACH_XV",
		  info);
      goto error1;
    };

  if (-1 == info -> fd)
    goto error1;

  info -> attach_mode = attach_mode;
  /* Current capture mode is no capture at all */
  info -> capture_mode = CAPTURE_MODE_NONE;

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

  /* Glint bug - XV_ENCODING not listed */
  if (p_info->encodings > 0
      && None == p_info->xa_encoding)
    {
      if (info->debug_level > 0)
	fprintf(stderr, "  TVeng Xv atom: XV_ENCODING (hidden) (%i -> %i)\n",
		0, p_info->encodings - 1);

      p_info->xa_encoding = XInternAtom (dpy, "XV_ENCODING", False);
      p_info->encoding_max = p_info->encodings - 1;
      p_info->encoding_min = 0;
      p_info->encoding_gettable = TRUE;
    }

      /* Set the mute control to OFF (workaround for BTTV bug) */
      /* tveng_set_control(&control, 0, info); */

  /* fill in with the proper values */
  get_control (info, NULL /* all */);

  /* We have a valid device, get some info about it */
  info->current_controller = TVENG_CONTROLLER_XV;

	/* Video inputs & standards */

	info->video_inputs = NULL;
	info->cur_video_input = NULL;

	info->video_standards = NULL;
	info->cur_video_standard = NULL;

	if (!get_video_input_list (info))
	  goto error1; /* XXX*/

	CLEAR (info->overlay);

	info->overlay.set_xwindow = set_overlay_xwindow;
	info->overlay.set_window_chromakey = set_overlay_window_chromakey;
	info->overlay.get_chromakey = get_overlay_chromakey;
	info->overlay.enable = enable_overlay;

	CLEAR (info->capture);


  /* fill in capabilities info */
	info->caps.channels = 0; /* FIXME info->num_inputs;*/
  /* Let's go creative! */
  snprintf(info->caps.name, 32, "XVideo device");
#if 0
  info->caps.minwidth = 1;
  info->caps.minheight = 1;
  info->caps.maxwidth = 32768;
  info->caps.maxheight = 32768;
#else
  /* XXX conservative limits. */
  info->caps.minwidth = 16;
  info->caps.minheight = 16;
  info->caps.maxwidth = 768;
  info->caps.maxheight = 576;
#endif

  return info->fd;

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
tvengxv_describe_controller(const char ** short_str, const char ** long_str,
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
  gboolean dummy;

  t_assert(info != NULL);

  p_tveng_stop_everything(info, &dummy);

  if (p_info->ei)
    XvFreeEncodingInfo(p_info->ei);
  p_info->ei = NULL;

  info -> fd = -1;
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
    XvGetPortAttribute(info->display,
		       p_info->port,
		       p_info->xa_signal_strength,
		       strength);

  if (afc)
    *afc = 0;
  return 0;
}




static struct tveng_module_info tvengxv_module_info = {
  .attach_device =		tvengxv_attach_device,
  .describe_controller =	tvengxv_describe_controller,
  .close_device =		tvengxv_close_device,

  .set_video_input		= set_video_input,
  .get_video_input		= get_video_input,
  .set_tuner_frequency		= set_tuner_frequency,
  .get_tuner_frequency		= get_tuner_frequency,
  /* Video input and standard combine as "encoding", get_video_input
     also determines the current standard, hence no get_video_standard. */
  .set_video_standard		= set_video_standard,
  .set_control			= set_control,
  .get_control			= get_control,

  .get_signal_strength =	tvengxv_get_signal_strength,

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

  CLEAR (*module_info);
}
#endif
