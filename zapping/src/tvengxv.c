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
#include <errno.h>

#include <tveng.h>

#ifdef USE_XV
#define TVENGXV_PROTOTYPES 1
#include "tvengxv.h"

#include "globals.h" /* xv_overlay_port */
#include "zmisc.h"

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
  /* This atoms define the controls */
  Atom	encoding;
  int encoding_max, encoding_min, encoding_gettable;
  Atom	freq;
  int freq_max, freq_min;
  Atom	mute;
  Atom	volume;
  Atom	colorkey;
  int colorkey_max, colorkey_min;
  Atom	signal_strength;
  Window last_win;
  GC last_gc;
  int last_w, last_h;
};

#define P_INFO(p) PARENT (p, struct private_tvengxv_device_info, info)

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

  memset (&c, 0, sizeof (c));

  xatom = XInternAtom (p_info->info.priv->display, atom, False);

  if (xatom == None)
    return TRUE;

  c.atom = xatom;

  c.pub.id = id;

  if (!(c.pub.label = strdup (_(label))))
	 goto failure;

  c.pub.minimum = minimum;
  c.pub.maximum = maximum;
  c.pub.step = step;
  
  c.pub.type = type;

  c.pub._device = &p_info->info;

  if (0 == strcmp (atom, "XV_INTERLACE")) {
	  c.pub.menu = calloc (4, sizeof (char *));

	  if (!c.pub.menu)
		  goto failure;

	  if (!(c.pub.menu[0] = strdup (_("No")))
	      || !(c.pub.menu[1] = strdup (_("Yes")))
	      || !(c.pub.menu[2] = strdup (_("Doublescan"))))
		  goto failure;
  }

  if (!append_control (&p_info->info, &c.pub, sizeof (c))) {
  failure:
	  if (c.pub.menu) {
		  free ((char *) c.pub.menu[0]);
		  free ((char *) c.pub.menu[1]);
		  free ((char *) c.pub.menu[2]);
		  free ((char **) c.pub.menu);
	  }

	  free ((char *) c.pub.label);
	  return FALSE;
  }

  return TRUE;
}

static tv_bool
tvengxv_update_control		(tveng_device_info *	info,
				 tv_control *		tc);

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
  p_info->encoding = None;
  p_info->freq = None;
  p_info->mute = None;
  p_info->volume = None;
  p_info->colorkey = None;
  p_info->signal_strength = None;
  p_info->ei = NULL;

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

		  p_info->encoding = XInternAtom (dpy, "XV_ENCODING", False);
		  p_info->encoding_max = at[i].max_value;
		  p_info->encoding_min = at[i].min_value;
		  p_info->encoding_gettable = at[i].flags & XvGettable;
		  continue;
	  } else if (!strcmp("XV_SIGNAL_STRENGTH", at[i].name)) {
		  if (!(at[i].flags & XvGettable))
			  continue;

		  p_info->signal_strength = XInternAtom (dpy, "XV_SIGNAL_STRENGTH", False);
		  continue;
	  }

	  if ((at[i].flags & (XvGettable | XvSettable)) != (XvGettable | XvSettable))
		  continue;
		  
	  if (!strcmp("XV_FREQ", at[i].name)) {
		  info->caps.flags |= TVENG_CAPS_TUNER;

		  p_info->freq = XInternAtom (dpy, "XV_FREQ", False);
		  p_info->freq_max = at[i].max_value;
		  p_info->freq_min = at[i].min_value;
		  continue;
	  } else if (!strcmp("XV_COLORKEY", at[i].name)) {
		  info->caps.flags = TVENG_CAPS_CHROMAKEY;

		  p_info->colorkey = XInternAtom (dpy, "XV_COLORKEY", False);
		  p_info->colorkey_max = at[i].max_value;
		  p_info->colorkey_min = at[i].min_value;
		  continue;
	  }

	  if (0 == strcmp ("XV_MUTE", at[i].name)) {
		  p_info->mute = XInternAtom (dpy, "XV_MUTE", False);
	  } else if (0 == strcmp ("XV_VOLUME", at[i].name)) {
		  p_info->volume = XInternAtom (dpy, "XV_VOLUME", False);
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
  tvengxv_update_control (info, NULL);

  /* We have a valid device, get some info about it */
  info->current_controller = TVENG_CONTROLLER_XV;

  /* Fill in inputs */
  info->inputs = NULL;
  info->cur_input = 0;
  tvengxv_get_inputs(info);

  /* Fill in standards */
  info->standards = NULL;
  info->cur_standard = 0;
  tvengxv_get_standards(info);

  /* fill in capabilities info */
  info->caps.channels = info->num_inputs;
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
  tv_control *tc;

  t_assert(info != NULL);

  tveng_stop_everything(info);

  if (p_info->ei)
    XvFreeEncodingInfo(p_info->ei);
  p_info->ei = NULL;

  info -> fd = 0;
  info -> current_controller = TVENG_CONTROLLER_NONE;

  if (info -> file_name)
    free(info -> file_name);
  if (info -> inputs)
    free(info -> inputs);
  if (info -> standards)
    free(info -> standards);

	while ((tc = info->controls)) {
		info->controls = tc->next;
		free_control (tc);
	}

  /* clear the atoms */
  info -> num_standards = 0;
  info -> num_inputs = 0;
  info -> inputs = NULL;
  info -> standards = NULL;
  info -> file_name = NULL;
}

/* Returns -1 if the input doesn't exist */
static inline int
tvengxv_find_input(const char *name, tveng_device_info *info)
{
  int i;

  for (i=0; i<info->num_inputs; i++)
    if (!strcasecmp(name, info->inputs[i].name))
      return i;

  return -1;
}

static int
tvengxv_get_inputs(tveng_device_info *info)
{
  Display *dpy;
  struct private_tvengxv_device_info *p_info =
    (struct private_tvengxv_device_info*) info;
  char norm[64], input[64];
  int i, val;

  t_assert(info != NULL);

  dpy = info->priv->display;

  norm[63] = input[63] = 0;

  if (info->inputs)
    free(info->inputs);

  info->inputs = NULL;
  info->num_inputs = 0;
  info->cur_input = 0;

  if (p_info->encoding == None)
    return 0; /* Nothing settable */

  for (i=0; i<p_info->encodings; i++)
    {
      if (info->debug_level > 0)
	fprintf(stderr, "  TVeng Xv input #%d: %s\n", i,
		p_info->ei[i].name);

      if (2 != sscanf(p_info->ei[i].name, "%63[^-]-%63s", norm, input))
	continue; /* not parseable */
      if (-1 != tvengxv_find_input(input, info))
	continue;
      /* norm not present, add to the list */
      info->inputs = realloc(info->inputs, (info->num_inputs+1)*
			     sizeof(struct tveng_enum_input));
      info->inputs[info->num_inputs].id = i;
      info->inputs[info->num_inputs].index = info->num_inputs;
      info->inputs[info->num_inputs].flags = 0;
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
      info->num_inputs++;
    }

  input_collisions(info);

  /* Get the current input */
  val = 0;
  if ((p_info->encoding != None) &&
      (p_info->encoding_gettable))
    XvGetPortAttribute(info->priv->display, p_info->port,
		       p_info->encoding, &val);
#warning
  /* Xv/v4l BUG? */
  if (val < 0 || val > 10 /*XXX*/)
    val = 0;
  if (p_info->ei)
    if ((2 == sscanf(p_info->ei[val].name, "%63[^-]-%63s", norm, input)) &&
	(-1 != (i=tvengxv_find_input(input, info))))
      info->cur_input = i;

  return (info->num_inputs);
}

static const struct {
	const char *		encoding;
	tv_videostd_id		id;
} xv_encoding_meta [] = {
	{ "pal",	TV_VIDEOSTD_PAL	},	/* somewhat broad, but who knows */
	{ "ntsc",	TV_VIDEOSTD_NTSC_M },
	{ "secam",	TV_VIDEOSTD_SECAM },	/* ditto */
	{ "palnc",	TV_VIDEOSTD_PAL_NC },
	{ "palm",	TV_VIDEOSTD_PAL_M },
	{ "paln",	TV_VIDEOSTD_PAL_N },
	{ "ntscjp",	TV_VIDEOSTD_NTSC_M_JP },
};

/*
  Finds the XV encoding giving this standard and this input. Returns
  -1 on error, the index in p_info->ei on success.
*/
static int
tvengxv_find_encoding(const char *standard, const char *input,
		      tveng_device_info *info)
{
  struct private_tvengxv_device_info * p_info =
    (struct private_tvengxv_device_info *) info;
  int i;
  char encoding_name[128];

  encoding_name[127] = 0;
  snprintf(encoding_name, 127, "%s-%s", standard, input);

  for (i=0; i<p_info->encodings; i++)
    {
      if (!strcasecmp(encoding_name, p_info->ei[i].name))
	return i;
    }

  return -1;
}

static int
tvengxv_set_input(struct tveng_enum_input * input,
		  tveng_device_info * info)
{
  int i=0;
  struct private_tvengxv_device_info * p_info =
    (struct private_tvengxv_device_info*) info;

  t_assert(info != NULL);
  t_assert(input != NULL);

  if (info->num_standards == 0)
    return 0; /* No settable standards */

  if (-1 ==
      (i = tvengxv_find_encoding(info->standards[info->cur_standard].name,
				 input->name, info)))
    {
      info->tveng_errno = -1;
      t_error_msg("find_encoding",
		  "the given encoding (%s, %s) couldn't be found",
		  info, info->standards[info->cur_standard], input->name);
      return 0; /* not found, no critical error though */
    }

  if (p_info->encoding != None)
    XvSetPortAttribute(info->priv->display, p_info->port,
		       p_info->encoding, i);

  info->cur_input = input->index;

  return 0;
}

/* Returns -1 if the input doesn't exist */
static inline int
tvengxv_find_standard(const char *name, tveng_device_info *info)
{
  int i;

  for (i=0; i<info->num_standards; i++)
    if (!strcasecmp(name, info->standards[i].name))
      return i;

  return -1;
}


static int
tvengxv_get_standards(tveng_device_info *info)
{
  Display *dpy;
  struct private_tvengxv_device_info *p_info =
    (struct private_tvengxv_device_info*) info;
  char norm[64], input[64];
  int i, j, val;

  t_assert(info != NULL);

  dpy = info->priv->display;

  norm[63] = input[63] = 0;

  if (info->standards)
    free(info->standards);

  info->standards = NULL;
  info->num_standards = 0;
  info->cur_standard = 0;

  if (p_info->encoding == None)
    return 0;

  for (i=0; i<p_info->encodings; i++)
    {
      if (2 != sscanf(p_info->ei[i].name, "%63[^-]-%63s", norm, input))
	continue; /* not parseable */
      if (-1 != tvengxv_find_standard(norm, info))
	continue;
      /* norm not present, add to the list */
      info->standards = realloc(info->standards,
				(info->num_standards+1)*
			     sizeof(struct tveng_enumstd));

      info->standards[info->num_standards].stdid = TV_VIDEOSTD_UNKNOWN;
      for (j = 0; j < N_ELEMENTS (xv_encoding_meta); j++)
	if (0 == strcmp (xv_encoding_meta[j].encoding, norm))
	    {
	      info->standards[info->num_standards].stdid = xv_encoding_meta[j].id;
	      break;
	    }

      info->standards[info->num_standards].id = info->num_standards;
      snprintf(info->standards[info->num_standards].name, 32,
	       norm);
      info->standards[info->num_standards].name[31] = 0;
      info->standards[info->num_standards].hash =
	tveng_build_hash(info->standards[info->num_standards].name);
      info->standards[info->num_standards].index = info->num_standards;
      info->standards[info->num_standards].width = p_info->ei[i].width;
      info->standards[info->num_standards].height = p_info->ei[i].height;
      /* rate here is the field period */
      info->standards[info->num_standards].frame_rate =
	p_info->ei[i].rate.denominator
	/ (2.0 * p_info->ei[i].rate.numerator);
      info->num_standards++;
    }

  standard_collisions(info);

  /* Get the current input */
  val = 0;
  if ((p_info->encoding != None) &&
      (p_info->encoding_gettable))
    XvGetPortAttribute(info->priv->display, p_info->port,
		       p_info->encoding, &val);

  if (p_info->ei)
    if ((2 == sscanf(p_info->ei[val].name, "%63[^-]-%63s", norm, input)) &&
	(-1 != (i=tvengxv_find_standard(norm, info))))
      info->cur_standard = i;

  return (info->num_standards);
}

static int
tvengxv_set_standard(struct tveng_enumstd * standard,
		     tveng_device_info * info)
{
  int i=0;
  struct private_tvengxv_device_info * p_info =
    (struct private_tvengxv_device_info*) info;

  t_assert(info != NULL);
  t_assert(standard != NULL);

  if (info->num_inputs == 0)
    return 0; /* No searchable inputs */

  if (-1 ==
      (i = tvengxv_find_encoding(standard->name,
				 info->inputs[info->cur_input].name,
				 info)))
    {
      info->tveng_errno = -1;
      t_error_msg("find_encoding",
		  "the given encoding (%s, %s) couldn't be found",
		  info, standard->name, info->inputs[info->cur_input]);
      return 0; /* not found, no critical error though */
    }

  if (p_info->encoding != None)
    XvSetPortAttribute(info->priv->display, p_info->port,
		       p_info->encoding, i);

  info->cur_standard = standard->index;

  return 0;
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


static tv_bool
update_control			(struct private_tvengxv_device_info *p_info,
				 struct control *	c)
{
	int value;

	if (c->atom == p_info->mute)
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
tvengxv_update_control		(tveng_device_info *	info,
				 tv_control *		tc)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);

	if (tc) {
		return update_control (p_info, C(tc));
	} else {
		for (tc = p_info->info.controls; tc; tc = tc->next)
			if (tc->_device == info)
				if (!update_control (p_info, C(tc)))
					return FALSE;
	}

	return TRUE;
}

static int
tvengxv_set_control		(tveng_device_info *	info,
				 tv_control *		tc,
				 int			value)
{
	struct private_tvengxv_device_info *p_info = P_INFO (info);

	XvSetPortAttribute (info->priv->display,
			    p_info->port,
			    C(tc)->atom,
			    value);

	if (C(tc)->atom == p_info->mute) {
		if (tc->value != value) {
			tc->value = value;
			tv_callback_notify (tc, tc->_callback);
		}

		return TRUE;
	}

	return update_control (p_info, C(tc));
}


static int
tvengxv_tune_input(uint32_t freq, tveng_device_info *info)
{
  struct private_tvengxv_device_info * p_info =
    (struct private_tvengxv_device_info*)info;

  t_assert(info != NULL);

  if (p_info->freq != None)
    XvSetPortAttribute(info->priv->display,
		       p_info->port,
		       p_info->freq,
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

  if (p_info->signal_strength == None)
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
		       p_info->signal_strength,
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
  if (!freq || p_info->freq == None)
    return 0;

  XvGetPortAttribute(info->priv->display,
		     p_info->port,
		     p_info->freq,
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

static int
tvengxv_detect_preview(tveng_device_info *info)
{
  return 1; /* we do support preview */
}

static int
tvengxv_set_preview_window(tveng_device_info * info)
{
  struct private_tvengxv_device_info * p_info =
    (struct private_tvengxv_device_info *)info;
  
  t_assert(info != NULL);
  
  /* Just reinit if necessary */
  if ((info->current_mode == TVENG_CAPTURE_WINDOW) &&
      (p_info->last_win != info->window.win ||
       p_info->last_gc != info->window.gc ||
       p_info->last_w != info->window.width ||
       p_info->last_h != info->window.height))
    {
      p_info->last_win = info->window.win;
      p_info->last_gc = info->window.gc;
      p_info->last_w = info->window.width;
      p_info->last_h = info->window.height;
      tveng_set_preview_off(info);
      tveng_set_preview_on(info);
    }

  return 0;
}

static int
tvengxv_get_preview_window(tveng_device_info * info)
{
  return 0;
}

static int
tvengxv_set_preview(int on, tveng_device_info * info)
{
  struct private_tvengxv_device_info * p_info =
    (struct private_tvengxv_device_info *)info;
  int val, width, height, dummy;
  Window win_ignore;

  t_assert(info != NULL);

  if ((info->window.win == 0) || (info->window.gc == 0))
    {
      info->tveng_errno = -1;
      t_error_msg("win",
		  "The window value hasn't been set", info);
      return -1;
    }

  XGetGeometry(info->priv->display, info->window.win, &win_ignore,
	       &dummy, &dummy, &width, &height, &dummy, &dummy);

  val = 0;
  if ((p_info->encoding != None) &&
      (p_info->encoding_gettable))
    XvGetPortAttribute(info->priv->display, p_info->port,
		       p_info->encoding, &val);

  if (on)
    {
      XvPutVideo(info->priv->display, p_info->port, info->window.win,
		 info->window.gc,
		 0, 0, p_info->ei[val].width, p_info->ei[val].height, /* src */
		 0, 0, width, height);
      info->current_mode = TVENG_CAPTURE_WINDOW;
    }
  else
    {
      XvStopVideo(info->priv->display, p_info->port,
		  info->window.win);
      info->current_mode = TVENG_NO_CAPTURE;
    }
  XSync(info->priv->display, False);

  return 0;
}

static int
tvengxv_start_previewing (tveng_device_info * info,
			  x11_dga_parameters *dga)
{
  int dummy;
  Window win_ignore;

  tveng_stop_everything(info);

  t_assert(info -> current_mode == TVENG_NO_CAPTURE);

  XGetGeometry(info->priv->display, info->window.win, &win_ignore,
	       &dummy, &dummy, &info->window.width,
	       &info->window.height, &dummy, &dummy);

  if (tveng_set_preview_on(info) == -1)
    return -1;

  info->current_mode = TVENG_CAPTURE_PREVIEW;

  return 0;
}

static int
tvengxv_stop_previewing (tveng_device_info * info)
{
  struct private_tvengxv_device_info * p_info =
    (struct private_tvengxv_device_info *)info;

  XvStopVideo(info->priv->display, p_info->port,
	      info->window.win);
  XSync(info->priv->display, False);

  info->current_mode = TVENG_NO_CAPTURE;

  return 0;
}

static void
tvengxv_set_chromakey (uint32_t chroma, tveng_device_info *info)
{
  struct private_tvengxv_device_info * p_info =
    (struct private_tvengxv_device_info *)info;

  if (p_info->colorkey != None)
    XvSetPortAttribute (info->priv->display, p_info->port,
			p_info->colorkey, chroma);
}

static int
tvengxv_get_chromakey (uint32_t *chroma, tveng_device_info *info)
{
  struct private_tvengxv_device_info * p_info =
    (struct private_tvengxv_device_info *)info;

  if (p_info->colorkey == None)
    return -1;

  XvGetPortAttribute (info->priv->display, p_info->port,
		      p_info->colorkey, chroma);
  return 0;
}

static struct tveng_module_info tvengxv_module_info = {
  .attach_device =		tvengxv_attach_device,
  .describe_controller =	tvengxv_describe_controller,
  .close_device =		tvengxv_close_device,
  .get_inputs =			tvengxv_get_inputs,
  .set_input =			tvengxv_set_input,
  .get_standards =		tvengxv_get_standards,
  .set_standard =		tvengxv_set_standard,
  .update_capture_format =	tvengxv_update_capture_format,
  .set_capture_format =		tvengxv_set_capture_format,
  .update_control =		tvengxv_update_control,
  .set_control =		tvengxv_set_control,
  .tune_input =			tvengxv_tune_input,
  .get_signal_strength =	tvengxv_get_signal_strength,
  .get_tune =			tvengxv_get_tune,
  .get_tuner_bounds =		tvengxv_get_tuner_bounds,
  .detect_preview =		tvengxv_detect_preview,
  .set_preview_window =		tvengxv_set_preview_window,
  .get_preview_window =		tvengxv_get_preview_window,
  .set_preview =		tvengxv_set_preview,
  .start_previewing =		tvengxv_start_previewing,
  .stop_previewing =		tvengxv_stop_previewing,
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
