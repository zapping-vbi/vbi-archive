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
  virtual device without actual hw underneath. To enable it
  use the device name "emulator".
  You can also this module as a template when you want to support
  other apis in tveng.
*/
#include "site_def.h"
#include "config.h"

#include <stdio.h>
#include <errno.h>
#include "common/fifo.h"
#include <tveng.h>

#define TVENGEMU_PROTOTYPES
#include "tvengemu.h"

struct private_tvengemu_device_info
{
  tveng_device_info	info; /* Inherited */
  tv_overlay_buffer	overlay_buffer;
  uint32_t		freq; /* Current freq */
  uint32_t		freq_min, freq_max; /* tuner bounds */
  uint32_t		chromakey; /* overlay chroma */
};

#define P_INFO(p) PARENT (p, struct private_tvengemu_device_info, info)

static tv_bool
set_control			(tveng_device_info *	info,
				 tv_control *		tc,
				 int			value)
{
	fprintf (stderr, "emu::set_control '%s' value=%d=0x%x\n",
		 tc->label, value, value);

	tc->value = value;

	return TRUE;
}

static tv_control *
add_control			(tveng_device_info *	info,
				 const char *		label,
				 tv_control_id		id,
				 tv_control_type	type,
				 int			current,
				 int			reset,
				 int			minimum,
				 int			maximum,
				 int			step)
{
	tv_control c;
	tv_control *tc;

	CLEAR (c);

	c.type		= type;
	c.id		= id;

	if (!(c.label = strdup (_(label))))
		return NULL;

	c.minimum	= minimum;
	c.maximum	= maximum;
	c.step		= step;
	c.reset		= reset;

	c.value		= current;

	tc = append_control (info, &c, sizeof (c));

	return tc;
}

static void
add_controls			(tveng_device_info *	info)
{
	tv_control *tc;

	add_control (info, "Integer",
		     TV_CONTROL_ID_UNKNOWN,
		     TV_CONTROL_TYPE_INTEGER,
		     0, 13, -20, 72, 3);

	tc = add_control (info, "Meaning of Life",
			  TV_CONTROL_ID_UNKNOWN,
			  TV_CONTROL_TYPE_CHOICE,
			  0, 0, 0, 2, 1);

	tc->menu = calloc (3 + 1, sizeof (const char *));

	tc->menu[0] = strdup ("42");
	tc->menu[1] = strdup ("9:4:1");
	tc->menu[2] = strdup ("Monty Python");

	add_control (info, "Self Destruct",
		     TV_CONTROL_ID_UNKNOWN,
		     TV_CONTROL_TYPE_ACTION,
		     0, 0, INT_MIN, INT_MAX, 0);

	add_control (info, "Mute",
		     TV_CONTROL_ID_MUTE,
		     TV_CONTROL_TYPE_BOOLEAN,
		     0, 1, 0, 1, 1);

	add_control (info, "Red Alert Color",
		     TV_CONTROL_ID_UNKNOWN,
		     TV_CONTROL_TYPE_COLOR,
		     0x00FF00, 0xFF0000,
		     0x000000, 0xFFFFFF, 0);
}

static tv_bool
set_video_standard		(tveng_device_info *	info,
				 const tv_video_standard *ts)
{
	fprintf (stderr, "emu::set_standard '%s'\n", ts->label);

	store_cur_video_standard (info, ts);

	return TRUE;
}

static void
add_standards			(tveng_device_info *	info)
{
	append_video_standard (&info->video_standards, TV_VIDEOSTD_SET_PAL,
			       "PAL", "PAL", sizeof (tv_video_standard));

	append_video_standard (&info->video_standards, TV_VIDEOSTD_SET_NTSC,
			       "NTSC", "NTSC", sizeof (tv_video_standard));

	info->cur_video_standard = info->video_standards;
}

static tv_bool
set_tuner_frequency		(tveng_device_info *	info,
				 tv_video_line *	l,
				 unsigned int		frequency)
{
	if (l->u.tuner.frequency != frequency) {
		l->u.tuner.frequency = frequency;
		tv_callback_notify (info, l, l->_callback);
	}

	return TRUE;
}

static tv_bool
set_video_input			(tveng_device_info *	info,
				 const tv_video_line *	tl)
{
	fprintf (stderr, "emu::set_video_input '%s'\n", tl->label);

	store_cur_video_input (info, tl);

	return TRUE;
}

static void
add_video_inputs		(tveng_device_info *	info)
{
	tv_video_line *l;

	l = append_video_line (&info->video_inputs, TV_VIDEO_LINE_TYPE_TUNER,
			       "Tuner", "Tuner", sizeof (tv_video_line));

	l->_parent = info;

	l = append_video_line (&info->video_inputs, TV_VIDEO_LINE_TYPE_BASEBAND,
			       "Composite", "Composite", sizeof (tv_video_line));

	l->_parent = info;

	info->cur_video_input = info->video_inputs;
}


static struct tveng_caps caps = {
  name:		"Emulation device",
  flags:	TVENG_CAPS_CAPTURE | TVENG_CAPS_TUNER |
  TVENG_CAPS_TELETEXT | TVENG_CAPS_OVERLAY | TVENG_CAPS_CLIPPING,
  channels:	2,
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

  add_video_inputs (info);
  add_standards (info);
  add_controls (info);

  CLEAR (p_info->overlay_buffer);

  /* Set up some capture parameters */
  info->format.width = info->cur_video_standard->frame_width / 2;
  info->format.height = info->cur_video_standard->frame_height / 2;
  info->format.pixfmt = TV_PIXFMT_YVU420;
  tvengemu_update_capture_format (info);

  /* Overlay window setup */
  info->overlay_window.x = 0;
  info->overlay_window.y = 0;
  info->overlay_window.width = info->format.width;
  info->overlay_window.height = info->format.height;
  info->overlay_window.clip_vector.vector = NULL;
  info->overlay_window.clip_vector.size = 0;
  info->overlay_window.clip_vector.capacity = 0;

  /* Framebuffer */
  info->overlay_buffer.base = 0;
  info->overlay_buffer.format.width = info->caps.maxwidth;
  info->overlay_buffer.format.height = info->caps.maxheight;
  /*  info->overlay_buffer.depth = 17;
    info->overlay_buffer.bytes_per_line = (info->overlay_buffer.depth+7)/8 *
      info->overlay_buffer.width;
  */
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
  gboolean dummy;

  p_tveng_stop_everything (info, &dummy);
  info->fd = 0;
  info->current_controller = TVENG_CONTROLLER_NONE;

  if (info -> file_name)
    {
      free(info -> file_name);
      info->file_name = NULL;
    }

	free_controls (info);
	free_video_standards (info);
	free_video_inputs (info);
}

static int
tvengemu_update_capture_format (tveng_device_info *info)
{
  tv_image_format_init (&info->format,
			info->format.width,
			info->format.height,
			/* bytes_per_line */ 0,
			info->format.pixfmt, 0);

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

  tvengemu_update_capture_format (info);

  return 0;
}


static int
tvengemu_get_signal_strength (int *strength, int *afc,
			      tveng_device_info *info)
{
  t_assert (info != NULL);

  if (!info->cur_video_input
      || (info->cur_video_input->type
	  != TV_VIDEO_LINE_TYPE_TUNER))
    return -1;

  if (strength)
    *strength = 0;

  if (afc)
    *afc = 0;

  return 0;
}




static int
tvengemu_start_capturing (tveng_device_info *info)
{
  gboolean dummy;

  t_assert (info != NULL);

  p_tveng_stop_everything (info, &dummy);

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
  return zf_current_time ();
}


static tv_bool
get_overlay_buffer		(tveng_device_info *	info,
				 tv_overlay_buffer *	t)
{
	*t = P_INFO (info)->overlay_buffer;
	return TRUE;
}

static tv_bool
set_overlay_buffer		(tveng_device_info *	info,
				 const tv_overlay_buffer *t)
{
	P_INFO (info)->overlay_buffer = *t;
	return TRUE;
}

static tv_bool
set_overlay_window		(tveng_device_info *	info,
				 const tv_window *	w)
{
	info->overlay_window.x		= w->x;
	info->overlay_window.y		= w->y;
	info->overlay_window.width	= w->width;
	info->overlay_window.height	= w->height;

	tv_clip_vector_copy (&info->overlay_window.clip_vector,
			     &w->clip_vector);

	return TRUE;
}

static tv_bool
get_overlay_window		(tveng_device_info *	info)
{
	return TRUE;
}

static tv_bool
enable_overlay			(tveng_device_info *	info,
				 tv_bool		on)
{
	return TRUE;
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

  .set_video_input		= set_video_input,
  .set_tuner_frequency		= set_tuner_frequency,
  .set_video_standard		= set_video_standard,
  .set_control			= set_control,

  update_capture_format:	tvengemu_update_capture_format,
  set_capture_format:		tvengemu_set_capture_format,
  get_signal_strength:		tvengemu_get_signal_strength,
  start_capturing:		tvengemu_start_capturing,
  stop_capturing:		tvengemu_stop_capturing,
  read_frame:			tvengemu_read_frame,
  get_timestamp:		tvengemu_get_timestamp,

  .set_overlay_buffer		= set_overlay_buffer,
  .get_overlay_buffer		= get_overlay_buffer,
  .set_overlay_window		= set_overlay_window,
  .get_overlay_window		= get_overlay_window,
  .enable_overlay		= enable_overlay,

  get_chromakey:		tvengemu_get_chromakey,
  set_chromakey:		tvengemu_set_chromakey,

  private_size:			sizeof(struct private_tvengemu_device_info)
};


void tvengemu_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

  memcpy (module_info, &tvengemu_module_info,
	  sizeof (struct tveng_module_info));
}
