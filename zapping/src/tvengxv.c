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
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <stdio.h>
#include <errno.h>

/* We need video extensions (DGA) */
#ifndef DISABLE_X_EXTENSIONS
#ifdef HAVE_LIBXV
#define USE_XV 1
#endif
#endif

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xfuncs.h>
#ifdef USE_XV
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#endif

#define TVENGXV_PROTOTYPES 1
#include "tvengxv.h"

/* fixme: ifdef USE_XV, debug info */

struct private_tvengxv_device_info
{
  tveng_device_info info; /* Info field, inherited */
  XvPortID	port; /* port id */
  XvEncodingInfo *ei; /* list of encodings, for reference */
  int encodings; /* number of encodings */
  /* This atoms define the controls */
  Atom	encoding;
  Atom	color;
  Atom	hue;
  Atom	saturation;
  Atom	brightness;
  Atom	contrast;
  Atom	freq;
  Atom	mute;
  Atom	volume;
  Atom	colorkey;
};

/* Private, builds the controls structure */
static int
p_tveng1_build_controls(tveng_device_info * info);

static int
p_tvengxv_open_device_file(tveng_device_info *info)
{
  Display *dpy = info->private->display;
  Window * root_window = DefaultRootWindow(dpy);
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

  if (version < 2 || revision < 2)
    goto error1;

  if (Success != XvQueryAdaptors(dpy, root_window, &nAdaptors,
				 &pAdaptors))
    goto error1;

  for (i=0; i<nAdaptors; i++)
    {
      pAdaptor = pAdaptors + i;
      if ((pAdaptor->type & XvInputMask) &&
	  (pAdaptor->type & XvVideoMask))
	{ /* available port found */
	  for (j=0; j<pAdaptor->num_ports; j++)
	    {
	      p_info->port = pAdaptor->base_id + j;

	      if (Success == XvGrabPort(dpy, p_info->port, CurrentTime))
		goto adaptor_found;
	    }
	}
    }

  goto error2; /* no adaptors found */

  /* success */
 adaptor_found:
  /* Check that it supports querying controls and encodings */
  if (Success != XvQueryEncodings(dpy, p_info->port,
				  &p_info->encodings, &p_info->ei))
    goto error3;

  /* create the atom that handles the encoding */

  at = XvQueryPortAttributes(dpy, p_info->port, &attributes);
  if (!at)
    goto error4;

  XvFreeAdaptorInfo(pAdaptors);
  return 0; /* the port seems to work ok, success */

 error4:
  XvFreeEncodingInfo(p_info->ei);
 error3:
  XvUngrabPort(dpy, p_info->port, CurrentTime);
 error2:
  XvFreeAdaptorInfo(pAdaptors);
 error1:
  return -1; /* failure */
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
  XvAttribute *at;

  t_assert(info != NULL);

  if (info -> fd) /* If the device is already attached, detach it */
    tveng_close_device(info);

  /* clear the atoms */
  p_info->encoding = p_info->color = p_info->hue = p_info->saturation
    = p_info->brightness = p_info->contrast = p_info->freq =
    p_info->mute = p_info->volume = p_info->colorkey = None;

  /* In this module, the given device file doesn't matter */
  info -> file_name = strdup(_("XVideo"));
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
      t_error_msg("switch()", "This module only support TVENG_ATTACH_XV",
		  info);
      goto error1;
    };

  /*
    Errors (if any) are already aknowledged when we reach this point,
    so we don't show them again
  */
  if (info -> fd < 0)
    goto error1;
  
  info -> attach_mode = attach_mode;
  /* Current capture mode is no capture at all */
  info -> current_mode = TVENG_NO_CAPTURE;

  /* Build the atoms */
  /* FIXME: i'm here */

  /* We have a valid device, get some info about it */
  /* Fill in inputs */
  info->inputs = NULL;
  info->cur_input = 0;
  error = tvengxv_get_inputs(info);
  if (error < 1)
    {
      if (error == 0) /* No inputs */
	{
	  info->tveng_errno = -1;
	  snprintf(info->error, 256, _("No inputs for this device"));
	  fprintf(stderr, "%s\n", info->error);
	}
      tvengxv_close_device(info);
      return -1;
    }

  /* Fill in standards */
  info->standards = NULL;
  info->cur_standard = 0;
  error = tvengxv_get_standards(info);
  if (error < 0)
    {
      tvengxv_close_device(info);
      return -1;
    }

  /* Query present controls */
  info->num_controls = 0;
  info->controls = NULL;
  error = p_tvengxv_build_controls(info);
  if (error == -1)
      return -1;

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
  int i;
  int j;
  struct private_tvengxv_device_info *p_info=
    (struct private_tvengxv_device_info*) info;
  t_assert(info != NULL);

  tveng_stop_everything(info);

  XvUngrabPort(info->private->display, p_info->port, CurrentTime);
  XvFreeEncodingInfo(p_info->ei);

  info -> fd = 0;
  info -> current_controller = TVENG_CONTROLLER_NONE;

  if (info -> file_name)
    free(info -> file_name);
  if (info -> inputs)
    free(info -> inputs);
  if (info -> standards)
    free(info -> standards);
  for (i=0; i<info->num_controls; i++)
    {
      if ((info->controls[i].type == TVENG_CONTROL_MENU) &&
	  (info->controls[i].data))
	{
	  j = 0;
	  while (info->controls[i].data[j])
	    {
	      free(info->controls[i].data[j]);
	      j++;
	    }
	  free(info->controls[i].data);
	}
    }
  if (info -> controls)
    free(info -> controls);

  /* fixme: Do the atoms need to be freed? from the man page it
     doesn't look like it */
  /* clear the atoms */
  p_info->encoding = p_info->color = p_info->hue = p_info->saturation
    = p_info->brightness = p_info->contrast = p_info->freq =
    p_info->mute = p_info->volume = p_info->colorkey = None;

  info -> num_controls = 0;
  info -> num_standards = 0;
  info -> num_inputs = 0;
  info -> controls = NULL;
  info -> inputs = NULL;
  info -> standards = NULL;
  info -> file_name = NULL;
}

/* Returns -11 if the input doesn't exist */
static int
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
  int i;

  t_assert(info != NULL);

  dpy = info->private->display;

  norm[63] = input[63] = 0;

  if (info->inputs)
    free(info->inputs);

  info->inputs = NULL;
  info->num_inputs = 0;
  info->cur_input = 0;

  for (i=0; i<p_info->encodings; i++)
    {
      if (2 != sscanf(p_info->ei[i].name, "%63[^-]-%63s", norm, input))
	continue; /* not parseable */
      if (-1 == tvengxv_find_input(input, info))
	continue;
      /* norm not present, add to the list */
      info->inputs = realloc(info->inputs, (info->num_inputs+1)*
			     sizeof(struct tveng_enum_input));
      info->inputs[info->num_inputs].id = i;
      /* The XVideo extension provides very few info about encodings,
	 we must just make something up */
      info->inputs[info->num_inputs].tuners = 1;
      snprintf(info->inputs[info->num_inputs].name, 32,
	       p_info->ei[i].name);
      info->inputs[info->num_inputs].flags = TVENG_INPUT_TUNER |
	TVENG_INPUT_AUDIO;
      info->inputs[info->num_inputs].type = TVENG_INPUT_TYPE_TV;
      info->num_inputs++;
    }
  /* fixme: get the encoding, create the encoding atom */
}

static int
tvengxv_set_input(struct tveng_enum_input * input,
		  tveng_device_info * info)
{
  
  t_assert(info != NULL);
  t_assert(input != NULL);
}

static struct tveng_module_info tvengxv_module_info = {
  tvengxv_attach_device,
  tvengxv_describe_controller,
  tvengxv_close_device,
  NULL, //  tvengxv_get_inputs,
  NULL, //  tvengxv_set_input,
  NULL, //  tvengxv_get_standards,
  NULL, //  tvengxv_set_standard,
  NULL, //  tvengxv_update_capture_format,
  NULL, //  tvengxv_set_capture_format,
  NULL, //  tvengxv_update_controls,
  NULL, //  tvengxv_set_control,
  NULL, //  tvengxv_get_mute,
  NULL, //  tvengxv_set_mute,
  NULL, //  tvengxv_tune_input,
  NULL, //  tvengxv_get_signal_strength,
  NULL, //  tvengxv_get_tune,
  NULL, //  tvengxv_get_tuner_bounds,
  NULL, //  tvengxv_start_capturing,
  NULL, //  tvengxv_stop_capturing,
  NULL, //  tvengxv_read_frame,
  NULL, //  tvengxv_get_timestamp,
  NULL, //  tvengxv_set_capture_size,
  NULL, //  tvengxv_get_capture_size,
  NULL, //  tvengxv_detect_preview,
  NULL, //  tvengxv_set_preview_window,
  NULL, //  tvengxv_get_preview_window,
  NULL, //  tvengxv_set_preview,
  NULL, //  tvengxv_start_previewing,
  NULL, //  tvengxv_stop_previewing,
  sizeof(struct private_tvengxv_device_info)
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
