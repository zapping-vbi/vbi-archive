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
#include "common/device.h"
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

  struct timeval	sample_time;
  int64_t		stream_time;
};

#define P_INFO(p) PARENT (p, struct private_tvengemu_device_info, info)

static tv_bool
set_control			(tveng_device_info *	info _unused_,
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
				 tv_video_standard *	ts)
{
	fprintf (stderr, "emu::set_standard '%s'\n", ts->label);

	store_cur_video_standard (info, ts);

	return TRUE;
}

static void
add_standards			(tveng_device_info *	info)
{
	append_video_standard (&info->panel.video_standards, TV_VIDEOSTD_SET_PAL,
			       "PAL", "PAL", sizeof (tv_video_standard));

	append_video_standard (&info->panel.video_standards, TV_VIDEOSTD_SET_NTSC,
			       "NTSC", "NTSC", sizeof (tv_video_standard));

	info->panel.cur_video_standard = info->panel.video_standards;
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
get_signal_strength		(tveng_device_info *	info _unused_,
				 int *			strength _unused_,
				 int *			afc _unused_)
{
	return TRUE;
}

static tv_bool
set_video_input			(tveng_device_info *	info,
				 tv_video_line *	tl)
{
	fprintf (stderr, "emu::set_video_input '%s'\n", tl->label);

	store_cur_video_input (info, tl);

	return TRUE;
}

static void
add_video_inputs		(tveng_device_info *	info)
{
	tv_video_line *l;

	l = append_video_line (&info->panel.video_inputs, TV_VIDEO_LINE_TYPE_TUNER,
			       "Tuner", "Tuner", sizeof (tv_video_line));

	l->_parent = info;

	l = append_video_line (&info->panel.video_inputs, TV_VIDEO_LINE_TYPE_BASEBAND,
			       "Composite", "Composite", sizeof (tv_video_line));

	l->_parent = info;

	info->panel.cur_video_input = info->panel.video_inputs;
}



/* Closes a device opened with tveng_init_device */
static void tvengemu_close_device(tveng_device_info * info)
{
  gboolean dummy;

  p_tveng_stop_everything (info, &dummy);
  info->fd = -1;
  info->current_controller = TVENG_CONTROLLER_NONE;

  if (info -> file_name)
    {
      free(info -> file_name);
      info->file_name = NULL;
    }

	free_controls (info);
	free_video_standards (info);
	free_video_inputs (info);

	free (info->node.device);
	info->node.device = NULL;

	/* Don't free other info->node strings, these are static. */

	CLEAR (info->node);
}

static tv_bool
get_capture_format		(tveng_device_info *	info _unused_)
{
	return TRUE;
}

static tv_bool
set_capture_format		(tveng_device_info *	info,
				 const tv_image_format *fmt)
{
	info->capture.format = *fmt;

	return TRUE;
}

static tv_bool
capture_enable			(tveng_device_info *	info,
				 tv_bool		enable)
{
	struct private_tvengemu_device_info *p_info = P_INFO (info);

	if (enable) {
		gboolean dummy;

		p_tveng_stop_everything (info, &dummy);

		info->capture_mode = CAPTURE_MODE_READ;

		gettimeofday (&p_info->sample_time, /* tz */ NULL);

		p_info->stream_time = 0;
	} else {
		info->capture_mode = CAPTURE_MODE_NONE;
	}

	return TRUE;
}

static int
read_frame			(tveng_device_info *	info,
				 tv_capture_buffer *	buffer _unused_,
				 const struct timeval *	timeout _unused_)
{
  struct private_tvengemu_device_info *p_info = P_INFO (info);
  struct timeval adv;

  t_assert (info != NULL);

  if (info -> capture_mode != CAPTURE_MODE_READ)
    {
      info -> tveng_errno = -1;
      t_error_msg("check", "Current capture mode is not READ (%d)",
		  info, info->capture_mode);
      return -1;
    }

  usleep ((unsigned int)(1e6 / 40));

  if (buffer) {
    buffer->sample_time = p_info->sample_time;
    buffer->stream_time = p_info->stream_time;
  }

  adv.tv_sec = 0;
  adv.tv_usec = info->panel.cur_video_standard->frame_ticks * 1000 / 90;
  timeval_add (&p_info->sample_time, &p_info->sample_time, &adv);

  p_info->stream_time += info->panel.cur_video_standard->frame_ticks;
 
  return 1; /* success */
}

static tv_bool
set_overlay_buffer		(tveng_device_info *	info,
				 const tv_overlay_buffer *t)
{
	P_INFO (info)->overlay_buffer = *t;

	return TRUE;
}

static tv_bool
get_overlay_buffer		(tveng_device_info *	info _unused_)
{
	return TRUE;
}

static tv_bool
set_overlay_window_clipvec	(tveng_device_info *	info _unused_,
				 const tv_window *	w _unused_,
				 const tv_clip_vector *	v _unused_)
{
	return TRUE;
}

static tv_bool
get_overlay_window		(tveng_device_info *	info _unused_)
{
	return TRUE;
}

static tv_bool
set_overlay_window_chromakey	(tveng_device_info *	info,
				 const tv_window *	w _unused_,
				 unsigned int		chromakey)
{
  struct private_tvengemu_device_info *p_info = P_INFO (info);

  p_info -> chromakey = chromakey;
  return TRUE;
}

static tv_bool
get_overlay_chromakey		(tveng_device_info *info)
{
  struct private_tvengemu_device_info * p_info =
    (struct private_tvengemu_device_info*) info;

  info->overlay.chromakey = p_info -> chromakey;
  return TRUE;
}

static tv_bool
enable_overlay			(tveng_device_info *	info _unused_,
				 tv_bool		on _unused_)
{
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
int tvengemu_attach_device(const char* device_file,
			   Window window _unused_,
			   enum tveng_attach_mode attach_mode,
			   tveng_device_info * info)
{
  static const tv_device_node node = {
    .label	= "Emulated device",
    .bus	= NULL,
    .driver	= "Emulator",
    .version	= "0.1",
  };
  static const struct tveng_caps caps = {
    .name	= "Emulated device",
    .flags	= (TVENG_CAPS_CAPTURE |
		   TVENG_CAPS_TUNER |
		   TVENG_CAPS_TELETEXT |
		   TVENG_CAPS_OVERLAY |
		   TVENG_CAPS_CLIPPING),
    .channels	= 2,
    .audios	= 1,
    .maxwidth	= 768,
    .maxheight	= 576,
    .minwidth	= 32,
    .minheight	= 32
  };

  struct private_tvengemu_device_info * p_info = P_INFO (info);

  t_assert (device_file != NULL);
  t_assert (info != NULL);

  if (-1 != info->fd)
    tveng_close_device (info);

  info->node = node;

  /* XXX error */
  info->node.device = strdup (device_file);

  info -> file_name = strdup (device_file);

  info->caps = caps;

  info -> attach_mode = attach_mode;
  info -> capture_mode = CAPTURE_MODE_NONE;
  info -> fd = 0xdeadbeef;

  info->panel.set_video_input = set_video_input;
  info->panel.set_tuner_frequency = set_tuner_frequency;
  info->panel.get_signal_strength = get_signal_strength;
  info->panel.set_video_standard = set_video_standard;
  info->panel.set_control = set_control;

  add_video_inputs (info);
  add_standards (info);
  add_controls (info);

  CLEAR (p_info->overlay_buffer);

  CLEAR (info->overlay);

  info->overlay.set_buffer = set_overlay_buffer;
  info->overlay.get_buffer = get_overlay_buffer;
  info->overlay.set_window_clipvec = set_overlay_window_clipvec;
  info->overlay.get_window = get_overlay_window;
  info->overlay.set_window_chromakey = set_overlay_window_chromakey;
  info->overlay.get_chromakey = get_overlay_chromakey;
  info->overlay.enable = enable_overlay;

  CLEAR (info->capture);

  info->capture.get_format = get_capture_format;
  info->capture.set_format = set_capture_format;
  info->capture.enable = capture_enable;
  info->capture.read_frame = read_frame;

  /* Set up some capture parameters */
  info->capture.supported_pixfmt_set = TV_PIXFMT_SET_ALL;
  info->capture.format.width = info->panel.cur_video_standard->frame_width / 2;
  info->capture.format.height = info->panel.cur_video_standard->frame_height / 2;
  info->capture.format.pixel_format =
    tv_pixel_format_from_pixfmt (TV_PIXFMT_YVU420);
  get_capture_format (info);

  /* Overlay window setup */
  info->overlay.window.x = 0;
  info->overlay.window.y = 0;
  info->overlay.window.width = info->capture.format.width;
  info->overlay.window.height = info->capture.format.height;

  /* Framebuffer */
  info->overlay.buffer.base = 0;
  info->overlay.buffer.format.width = info->caps.maxwidth;
  info->overlay.buffer.format.height = info->caps.maxheight;
  /*  info->overlay.buffer.depth = 17;
    info->overlay.buffer.bytes_per_line = (info->overlay.buffer.depth+7)/8 *
      info->overlay.buffer.width;
  */
  /* Tuner bounds */
  p_info -> freq_min = 1000;
  p_info -> freq_max = 1000000;
  p_info -> freq = 815250;

  info -> current_controller = TVENG_CONTROLLER_EMU;

  return 0;
}

static struct tveng_module_info tvengemu_module_info = {
  attach_device:		tvengemu_attach_device,
  close_device:			tvengemu_close_device,

  .interface_label		= "Emulation",

  private_size:			sizeof(struct private_tvengemu_device_info)
};


void tvengemu_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

  memcpy (module_info, &tvengemu_module_info,
	  sizeof (struct tveng_module_info));
}
