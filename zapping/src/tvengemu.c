/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2002 Iñaki García Etxebarria
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
  This module is somewhat different, this time we are generating a
  virtual device without actual hw underneath.
  The module is disabled by default, to enable it define
  ENABLE_TVENGEMU in site_def.h:
  #define ENABLE_TVENGEMU
  You can also this module as a template when you want to support
  other apis in tveng.
*/
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <stdio.h>
#include <errno.h>
#include <site_def.h>
#include <common/fifo.h>
#include <tveng.h>

#define TVENGEMU_PROTOTYPES
#include "tvengemu.h"

struct private_tvengemu_device_info
{
  tveng_device_info	info; /* Inherited */
  uint32_t		freq; /* Current freq */
  uint32_t		freq_min, freq_max; /* tuner bounds */
  uint32_t		chromakey; /* overlay chroma */
};

/* The following definitions define the properties of our device */
/* Supported standards */
static struct tveng_enumstd standards[] = {
  {
    name:	"PAL",
    width:	384*2,
    height:	288*2,
    frame_rate:	25.0
  },
  {
    name:	"NTSC",
    width:	320*2,
    height:	240*2,
    frame_rate:	30000.0 / 1001.0
  }
};
#define nstandards (sizeof(standards)/sizeof(standards[0]))

/* Supported inputs */
static struct tveng_enum_input inputs[] = {
  {
    name:	"Television",
    tuners:	1,
    flags:	TVENG_INPUT_TUNER | TVENG_INPUT_AUDIO,
    type:	TVENG_INPUT_TYPE_TV
  },
  {
    name:	"Camera",
    tuners:	0,
    flags:	TVENG_INPUT_AUDIO,
    type:	TVENG_INPUT_TYPE_CAMERA
  }
};
#define ninputs (sizeof(inputs)/sizeof(inputs[0]))

/* Available controls */
static char *meaning_of_life_options[] =
  {"42", "0xdeadbeef", "Monthy Python knows", NULL};
static struct tveng_control controls[] = {
  {
    name:	"Foobarity",
    min:	-20,
    max:	72,
    def_value:	13,
    type:	TVENG_CONTROL_SLIDER
  },
  {
    name:	"Meaning of life",
    min:	0,
    max:	(sizeof(meaning_of_life_options)/sizeof(char*)) - 2,
    def_value:	0,
    type:	TVENG_CONTROL_MENU,
    data:	meaning_of_life_options
  },
  {
    name:	"Do magical things",
    type:	TVENG_CONTROL_BUTTON
  },
  {
    name:	"Mute",
    def_value:	0,
    type:	TVENG_CONTROL_CHECKBOX
  }
};
#define ncontrols (sizeof(controls)/sizeof(controls[0]))

static struct tveng_caps caps = {
  name:		"Emulation device",
  flags:	TVENG_CAPS_CAPTURE | TVENG_CAPS_TUNER |
  TVENG_CAPS_TELETEXT | TVENG_CAPS_OVERLAY | TVENG_CAPS_CLIPPING,
  channels:	ninputs,
  audios:	1,
  maxwidth:	768,
  maxheight:	576,
  minwidth:	32,
  minheight:	32
};

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
int tvengemu_attach_device(const char* device_file,
			   enum tveng_attach_mode attach_mode,
			   tveng_device_info * info)
{
  int i;
  struct private_tvengemu_device_info * p_info =
    (struct private_tvengemu_device_info*) info;

  t_assert (device_file != NULL);
  t_assert (info != NULL);

  if (info->fd)
    tveng_close_device (info);

  info -> file_name = strdup (device_file);

  memcpy (&info->caps, &caps, sizeof (caps));

  info -> attach_mode = attach_mode;
  info -> current_mode = TVENG_NO_CAPTURE;
  info -> fd = 0xdeadbeef;

  info->inputs = malloc (sizeof(inputs[0])*ninputs);
  for (i=0; i<ninputs; i++)
    {
      memcpy (&info->inputs[i], &inputs[i], sizeof(inputs[0]));
      info->inputs[i].id = i;
      info->inputs[i].index = i;
      info->inputs[i].hash = tveng_build_hash (info->inputs[i].name);
    }
  info->num_inputs = ninputs;
  info->cur_input = 0;

  info->standards = malloc (sizeof(standards[0])*nstandards);
  for (i=0; i<nstandards; i++)
    {
      memcpy (&info->standards[i], &standards[i], sizeof(standards[0]));
      info->standards[i].id = i;
      info->standards[i].index = i;
      info->standards[i].hash = tveng_build_hash (info->standards[i].name);
    }
  info->num_standards = nstandards;
  info->cur_standard = 0;

  info->controls = malloc (sizeof(controls[0])*ncontrols);
  for (i=0; i<ncontrols; i++)
    {
      if (controls[i].type == TVENG_CONTROL_BUTTON ||
	  controls[i].type == TVENG_CONTROL_CHECKBOX)
	{
	  controls[i].min = 0;
	  controls[i].max = 1;
	}
      memcpy (&info->controls[i], &controls[i], sizeof (controls[0]));
      info->controls[i].controller = TVENG_CONTROLLER_EMU;
      info->controls[i].cur_value = info->controls[i].def_value;
      info->controls[i].id = i;
    }
  info->num_controls = ncontrols;

  /* Set up some capture parameters */
  info->format.width = info->standards[info->cur_standard].width/2;
  info->format.height = info->standards[info->cur_standard].height/2;
  info->format.pixformat = TVENG_PIX_YVU420;
  tvengemu_update_capture_format (info);

  /* Overlay window setup */
  info->window.x = 0;
  info->window.y = 0;
  info->window.width = info->format.width;
  info->window.height = info->format.height;
  info->window.clipcount = 0;
  info->window.clips = NULL;

  /* Framebuffer */
  info->fb_info.base = NULL;
  info->fb_info.width = info->caps.maxwidth;
  info->fb_info.height = info->caps.maxheight;
  info->fb_info.depth = 17;
  info->fb_info.bytesperline = (info->fb_info.depth+7)/8 *
    info->fb_info.width;

  /* Tuner bounds */
  p_info -> freq_min = 1000;
  p_info -> freq_max = 1000000;
  p_info -> freq = 815250;

  info -> current_controller = TVENG_CONTROLLER_EMU;

  return 0;
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
tvengemu_describe_controller(char ** short_str, char ** long_str,
			     tveng_device_info * info)
{
  t_assert(info != NULL);
  if (short_str)
    *short_str = "EMU";
  if (long_str)
    *long_str = "Emulation driver";
}

/* Closes a device opened with tveng_init_device */
static void tvengemu_close_device(tveng_device_info * info)
{
  tveng_stop_everything (info);
  info->fd = 0;
  info->current_controller = TVENG_CONTROLLER_NONE;

  if (info -> file_name)
    {
      free(info -> file_name);
      info->file_name = NULL;
    }

  if (info -> inputs)
    {
      free(info -> inputs);
      info->inputs = NULL;
    }

  if (info -> standards)
    {
      free(info -> standards);
      info->standards = NULL;
    }

  if (info -> controls)
    {
      free(info -> controls);
      info->controls = NULL;
    }

  info->num_controls = 0;
  info->num_standards = 0;
  info->num_inputs = 0;
}

static
int tvengemu_get_inputs(tveng_device_info * info)
{
  t_assert (info != NULL);
  return 0;
}

static int
tvengemu_set_input (struct tveng_enum_input *input,
		    tveng_device_info *info)
{
  t_assert (info != NULL);

  info->cur_input = input->index;

  return 0;
}

static int
tvengemu_get_standards (tveng_device_info *info)
{
  t_assert (info != NULL);

  return 0;
}

static int
tvengemu_set_standard (struct tveng_enumstd *std,
		       tveng_device_info *info)
{
  t_assert (info != NULL);

  info->cur_standard = std->index;

  return 0;
}

static int
tvengemu_update_capture_format (tveng_device_info *info)
{
  t_assert (info != NULL);

  switch (info->format.pixformat)
    {
    case TVENG_PIX_RGB555:
      info->format.bpp = 2;
      info->format.depth = 15;
      break;
    case TVENG_PIX_RGB565:
    case TVENG_PIX_YUYV:
    case TVENG_PIX_UYVY:
      info->format.bpp = 2;
      info->format.depth = 16;
      break;
    case TVENG_PIX_YVU420:
    case TVENG_PIX_YUV420:
      info->format.bpp = 1.5;
      info->format.depth = 12;
      break;
    case TVENG_PIX_GREY:
      info->format.bpp = 1;
      info->format.depth = 8;
      break;
    case TVENG_PIX_RGB24:
    case TVENG_PIX_BGR24:
      info->format.bpp = 3;
      info->format.depth = 24;
      break;
    case TVENG_PIX_RGB32:
    case TVENG_PIX_BGR32:
      info->format.bpp = 4;
      info->format.depth = 32;
      break;
    default:
      t_assert_not_reached ();
      break;
    }

  info->format.bytesperline = info->format.width * info->format.bpp;
  info->format.sizeimage = info->format.height * info->format.bytesperline;

  return 0;
}

static int
tvengemu_set_capture_format (tveng_device_info *info)
{
  t_assert (info != NULL);

  if (info->format.height < info->caps.minheight)
    info->format.height = info->caps.minheight;
  if (info->format.height > info->caps.maxheight)
    info->format.height = info->caps.maxheight;
  if (info->format.width < info->caps.minwidth)
    info->format.width = info->caps.minwidth;
  if (info->format.width > info->caps.maxwidth)
    info->format.width = info->caps.maxwidth;

  tveng_update_capture_format (info);

  return 0;
}

static int
tvengemu_update_controls (tveng_device_info *info)
{
  t_assert (info != NULL);

  return 0;
}

static int
tvengemu_set_control (struct tveng_control *control, int value,
		      tveng_device_info *info)
{
  t_assert(control != NULL);
  t_assert(info != NULL);

  /* Clip value to a valid one */
  if (value < control->min)
    value = control -> min;
      
  if (value > control->max)
    value = control -> max;

  control->cur_value = value;

  return 0;
}

static int
tvengemu_get_mute (tveng_device_info * info)
{
  int returned_value;
  if (tveng_get_control_by_name("Mute", &returned_value, info) ==
      -1)
    return -1;

  return !!returned_value;
}

static int
tvengemu_set_mute (int value, tveng_device_info * info)
{
  return (tveng_set_control_by_name("Mute", !!value, info));
}

static int
tvengemu_tune_input (uint32_t freq, tveng_device_info *info)
{
  struct private_tvengemu_device_info * p_info =
    (struct private_tvengemu_device_info*) info;

  t_assert (info != NULL);

  if (info->inputs[info->cur_input].tuners == 0)
    return -1;

  p_info->freq = freq;

  return 0;
}

static int
tvengemu_get_signal_strength (int *strength, int *afc,
			      tveng_device_info *info)
{
  t_assert (info != NULL);

  if (info->inputs[info->cur_input].tuners == 0)
    return -1;

  if (strength)
    *strength = 0;

  if (afc)
    *afc = 0;

  return 0;
}

static int
tvengemu_get_tune (uint32_t *freq, tveng_device_info *info)
{
  struct private_tvengemu_device_info * p_info =
    (struct private_tvengemu_device_info*) info;

  t_assert (info != NULL);

  if (info->inputs[info->cur_input].tuners == 0)
    return -1;

  if (freq)
    *freq = p_info->freq;

  return 0;
}

static int
tvengemu_get_tuner_bounds (uint32_t *min, uint32_t *max,
			   tveng_device_info *info)
{
  struct private_tvengemu_device_info * p_info =
    (struct private_tvengemu_device_info*) info;

  t_assert (info != NULL);

  if (info->inputs[info->cur_input].tuners == 0)
    return -1;

  if (min)
    *min = p_info->freq_min;
  if (max)
    *max = p_info->freq_max;

  return 0;
}

static int
tvengemu_start_capturing (tveng_device_info *info)
{
  t_assert (info != NULL);

  tveng_stop_everything (info);

  info->current_mode = TVENG_CAPTURE_READ;

  return 0;
}

static int
tvengemu_stop_capturing (tveng_device_info *info)
{
  t_assert (info != NULL);

  info->current_mode = TVENG_NO_CAPTURE;

  return 0;
}

static int
tvengemu_read_frame (tveng_image_data *where,
		     unsigned int time, tveng_device_info *info)
{
  t_assert (info != NULL);

  if (info -> current_mode != TVENG_CAPTURE_READ)
    {
      info -> tveng_errno = -1;
      t_error_msg("check", "Current capture mode is not READ (%d)",
		  info, info->current_mode);
      return -1;
    }

  usleep (1e6 / 40);
 
  return 0;
}

static double
tvengemu_get_timestamp (tveng_device_info *info)
{
  /* Don't kill me, Michael ;-) */
  return current_time ();
}

static int
tvengemu_set_capture_size (int width, int height,
			   tveng_device_info * info)
{
  t_assert (info != NULL);

  info->format.width = width;
  info->format.height = height;

  return tveng_set_capture_format (info);
}

static int
tvengemu_get_capture_size (int *width, int *height,
			   tveng_device_info *info)
{
  t_assert (info != NULL);

  if (width)
    *width = info->format.width;

  if (height)
    *height = info->format.height;

  return 0;
}

static int
tvengemu_detect_preview (tveng_device_info *info)
{
  return 1;
}

static int
tvengemu_set_preview_window (tveng_device_info *info)
{
  return 0;
}

static int
tvengemu_get_preview_window (tveng_device_info *info)
{
  return 0;
}

static int
tvengemu_set_preview (int on, tveng_device_info *info)
{
  return 0;
}

static int
tvengemu_start_previewing (tveng_device_info *info)
{
  t_assert (info != NULL);

  info->current_mode = TVENG_CAPTURE_PREVIEW;

  return 0;
}

static int
tvengemu_stop_previewing (tveng_device_info *info)
{
  t_assert (info != NULL);

  info -> current_mode = TVENG_NO_CAPTURE;

  return 0;
}

static void
tvengemu_set_chromakey (uint32_t chroma, tveng_device_info *info)
{
  struct private_tvengemu_device_info * p_info =
    (struct private_tvengemu_device_info*) info;

  t_assert (info != NULL);

  p_info -> chromakey = chroma;
}

static int
tvengemu_get_chromakey (uint32_t *chroma, tveng_device_info *info)
{
  struct private_tvengemu_device_info * p_info =
    (struct private_tvengemu_device_info*) info;

  t_assert (info != NULL);

  if (chroma)
    *chroma = p_info -> chromakey;

  return 0;
}

static struct tveng_module_info tvengemu_module_info = {
  attach_device:		tvengemu_attach_device,
  describe_controller:		tvengemu_describe_controller,
  close_device:			tvengemu_close_device,
  get_inputs:			tvengemu_get_inputs,
  set_input:			tvengemu_set_input,
  get_standards:		tvengemu_get_standards,
  set_standard:			tvengemu_set_standard,
  update_capture_format:	tvengemu_update_capture_format,
  set_capture_format:		tvengemu_set_capture_format,
  update_controls:		tvengemu_update_controls,
  set_control:			tvengemu_set_control,
  get_mute:			tvengemu_get_mute,
  set_mute:			tvengemu_set_mute,
  tune_input:			tvengemu_tune_input,
  get_signal_strength:		tvengemu_get_signal_strength,
  get_tune:			tvengemu_get_tune,
  get_tuner_bounds:		tvengemu_get_tuner_bounds,
  start_capturing:		tvengemu_start_capturing,
  stop_capturing:		tvengemu_stop_capturing,
  read_frame:			tvengemu_read_frame,
  get_timestamp:		tvengemu_get_timestamp,
  set_capture_size:		tvengemu_set_capture_size,
  get_capture_size:		tvengemu_get_capture_size,
  detect_preview:		tvengemu_detect_preview,
  set_preview_window:		tvengemu_set_preview_window,
  get_preview_window:		tvengemu_get_preview_window,
  set_preview:			tvengemu_set_preview,
  start_previewing:		tvengemu_start_previewing,
  stop_previewing:		tvengemu_stop_previewing,
  get_chromakey:		tvengemu_get_chromakey,
  set_chromakey:		tvengemu_set_chromakey,

  private_size:			sizeof(struct private_tvengemu_device_info)
};


void tvengemu_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

#ifdef ENABLE_TVENGEMU
  memcpy(module_info, &tvengemu_module_info,
	 sizeof(struct tveng_module_info));
#endif 
}
