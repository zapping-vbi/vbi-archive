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

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <X11/Xlib.h> /* We use some X calls */
#include <X11/Xutil.h>

/* This undef's are to avoid a couple of header warnings */
#undef WNOHANG
#undef WUNTRACED
#include "tveng.h"
#include "tveng1.h" /* V4L specific headers */
#include "tveng2.h" /* V4L2 specific headers */

/*
  This is the structure we will be actually allocating. Note that we
  must leave space for all the private structures, since we do not
  know the actual size of the struct before attaching it to a
  device. We are never going to access the fields of this struct, it
  is just for allocating.
*/
struct private_tveng_device_info
{
  union{
    struct private_tveng1_device_info do_not_access_this_1;
    struct private_tveng2_device_info do_not_access_this_2;
  } do_not_touch_this;
};

/* Initializes a tveng_device_info object */
tveng_device_info * tveng_device_info_new(Display * display)
{
  tveng_device_info * new_object = (tveng_device_info*)
    malloc(sizeof(struct private_tveng_device_info));

  if (!new_object)
    return NULL;

  /* fill the struct with 0's */
  memset(new_object, 0, sizeof(struct private_tveng_device_info));

  /* Allocate some space for the error string */
  new_object -> error = (char*) malloc(256);

  if (!new_object->error)
    {
      free(new_object);
      perror("malloc");
      return NULL;
    }

  new_object->display = display;

  new_object->zapping_setup_fb_verbosity = 0; /* No output by default */

  new_object->current_controller = TVENG_CONTROLLER_NONE;

  /* return the allocated memory */
  return (new_object);
}

/* destroys a tveng_device_info object (and closes it if neccesary) */
void tveng_device_info_destroy(tveng_device_info * info)
{
  t_assert(info != NULL);

  if (info -> fd > 0)
    tveng_close_device(info);

  if (info -> error)
    free(info -> error);

  free(info);
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
int tveng_attach_device(const char* device_file,
			enum tveng_attach_mode attach_mode,
			tveng_device_info * info)
{
  t_assert(device_file != NULL);
  t_assert(info != NULL);

  if (info -> fd) /* If the device is already attached, detach it */
    tveng_close_device(info);

  info -> current_controller = TVENG_CONTROLLER_NONE;

  /*
    Check that the current display depth is one of the supported ones
  */
  switch (tveng_get_display_depth(info))
    {
    case 15:
    case 16:
    case 24:
    case 32:
      break;
    default:
      info -> tveng_errno = -1;
      t_error_msg("switch()",
		  _("The current display depth isn't supported by TVeng"),
		  info);
      return -1;
    }

  info -> fd = 0;
  /* Try first to attach it as a V4L2 device */
  if (-1 != tveng2_attach_device(device_file, attach_mode, info))
      return info -> fd;

  info -> fd = 0;
  /* Now try it as a V4L device */
  if (-1 != tveng1_attach_device(device_file, attach_mode, info))
    return info->fd;

  /* Error */
  info->tveng_errno = -1;
  t_error_msg("check()",
	      _("The device cannot be attached to any controller"),
	      info);
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
void
tveng_describe_controller(char ** short_str, char ** long_str,
			  tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      tveng1_describe_controller(short_str, long_str, info);
      break;
    case TVENG_CONTROLLER_V4L2:
      tveng2_describe_controller(short_str, long_str, info);
      break;
    default:
      t_assert_not_reached();
    }

  return;
}

/* Closes a device opened with tveng_init_device */
void tveng_close_device(tveng_device_info * info)
{
  t_assert(info != NULL);

  tveng_stop_everything(info);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      tveng1_close_device(info);
      break; 
    case TVENG_CONTROLLER_V4L2:
      tveng2_close_device(info);
      break;
    default:
      t_assert_not_reached();
    }

  return;
}

/*
  Functions for controlling the video capture. All of them return -1
  in case of error, so any value != -1 should be considered valid
  (unless explicitly stated in the description of the function) 
*/
/*
  Returns the number of inputs in the given device and fills in info
  with the correct info, allocating memory as needed
*/
int tveng_get_inputs(tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_get_inputs(info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_get_inputs(info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Sets the current input for the capture
*/
int tveng_set_input(struct tveng_enum_input * input,
		    tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(input != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_set_input(input, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_set_input(input, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Sets the input named name as the active input. -1 on error.
*/
int
tveng_set_input_by_name(const char * input_name, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(input_name != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_set_input_by_name(input_name, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_set_input_by_name(input_name, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Sets the active input by its id (may not be the same as its array
  index, but it should be). -1 on error
*/
int
tveng_set_input_by_id(int id, tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_set_input_by_id(id, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_set_input_by_id(id, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Sets the active input by its index in inputs. -1 on error
*/
int
tveng_set_input_by_index(int index, tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_set_input_by_index(index, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_set_input_by_index(index, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Queries the device about its standards. Fills in info as appropiate
  and returns the number of standards in the device. This is for the
  first tuner in the current input, should be enough since most (all)
  inputs have 1 or less tuners.
*/
int tveng_get_standards(tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_get_standards(info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_get_standards(info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Sets the current standard for the capture. standard is the name for
  the desired standard. updates cur_standard
*/
int tveng_set_standard(struct tveng_enumstd * std, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(std != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_set_standard(std, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_set_standard(std, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Sets the standard by name. -1 on error
*/
int
tveng_set_standard_by_name(char * name, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(name != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_set_standard_by_name(name, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_set_standard_by_name(name, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Sets the standard by id.
*/
int
tveng_set_standard_by_id(int id, tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_set_standard_by_id(id, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_set_standard_by_id(id, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Sets the standard by index. -1 on error
*/
int
tveng_set_standard_by_index(int index, tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_set_standard_by_index(index, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_set_standard_by_index(index, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/* Updates the current capture format info. -1 if failed */
int
tveng_update_capture_format(tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_update_capture_format(info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_update_capture_format(info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/* -1 if failed. Sets the pixformat and fills in info -> pix_format
   with the correct values  */
int
tveng_set_capture_format(tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_set_capture_format(info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_set_capture_format(info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Gets the current value of the controls, fills in info->controls
  appropiately. After this (and if it succeeds) you can look in
  info->controls to get the values for each control. -1 on error
*/
int
tveng_update_controls(tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_update_controls(info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_update_controls(info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Sets the value for an specific control. The given value will be
  clipped between min and max values. Returns -1 on error
*/
int
tveng_set_control(struct tveng_control * control, int value,
		  tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(control != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_set_control(control, value, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_set_control(control, value, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Gets the value of a control, given its name. Returns -1 on
  error. The comparison is performed disregarding the case. The value
  read is stored in cur_value.
*/
int
tveng_get_control_by_name(const char * control_name,
			  int * cur_value,
			  tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(control_name != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_get_control_by_name(control_name,
					cur_value, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_get_control_by_name(control_name,
					cur_value, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Sets the value of a control, given its name. Returns -1 on
  error. The comparison is performed disregarding the case.
  new_value holds the new value given to the control, and it is
  clipped as neccessary.
*/
int
tveng_set_control_by_name(const char * control_name,
			  int new_value,
			  tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(control_name != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_set_control_by_name(control_name,
					new_value, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_set_control_by_name(control_name,
					new_value, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Gets the value of a control, given its control id. -1 on error (or
  cid not found). The result is stored in cur_value.
*/
int
tveng_get_control_by_id(int cid, int * cur_value,
			tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_get_control_by_id(cid, cur_value, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_get_control_by_id(cid, cur_value, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Sets a control by its id. Returns -1 on error
*/
int tveng_set_control_by_id(int cid, int new_value,
			    tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_set_control_by_id(cid, new_value, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_set_control_by_id(cid, new_value, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Gets the value of the mute property. 1 means mute (no sound) and 0
  unmute (sound). -1 on error
*/
int
tveng_get_mute(tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_get_mute(info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_get_mute(info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Sets the value of the mute property. 0 means unmute (sound) and 1
  mute (no sound). -1 on error
*/
int
tveng_set_mute(int value, tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_set_mute(value, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_set_mute(value, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Tunes the current input to the given freq. Returns -1 on error.
*/
int
tveng_tune_input(__u32 freq, tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_tune_input(freq, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_tune_input(freq, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Gets the signal strength and the afc code. The afc code indicates
  how to get a better signal, if negative, tune higher, if negative,
  tune lower. 0 means no idea of feature not present in the current
  controller (i.e. V4L1). Strength and/or afc can be NULL pointers,
  that would mean ignore that parameter.
*/
int
tveng_get_signal_strength (int *strength, int * afc,
			   tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_get_signal_strength(strength, afc, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_get_signal_strength(strength, afc, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Stores in freq the currently tuned freq. Returns -1 on error.
*/
int
tveng_get_tune(__u32 * freq, tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_get_tune(freq, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_get_tune(freq, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Gets the minimum and maximum freq that the current input can
  tune. If there is no tuner in this input, -1 will be returned.
  If any of the pointers is NULL, its value will not be filled.
*/
int
tveng_get_tuner_bounds(__u32 * min, __u32 * max, tveng_device_info *
		       info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_get_tuner_bounds(min, max, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_get_tuner_bounds(min, max, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Sets up the capture device so any read() call after this one
  succeeds. Returns -1 on error.
*/
int
tveng_start_capturing(tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_start_capturing(info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_start_capturing(info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/* Tries to stop capturing. -1 on error. */
int
tveng_stop_capturing(tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_stop_capturing(info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_stop_capturing(info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/* 
   Reads a frame from the video device, storing the read data in
   the location pointed to by where. size indicates the destination
   buffer size (that must equal or greater than format.sizeimage)
   time: time to wait using select() in miliseconds
   info: pointer to the video device info structure
   This call was originally intended to wrap a single read() call, but
   since i cannot get it to work, now encapsulates the dqbuf/qbuf
   logic.
   Returns -1 on error, anything else on success
*/
int tveng_read_frame(void * where, unsigned int size, 
		     unsigned int time, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(size >= 0);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_read_frame(where, size, time, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_read_frame(where, size, time, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/*
  Gets the timestamp of the last read frame.
  Returns -1 on error, if the current mode isn't capture, or if we
  haven't captured any frame yet. The timestamp is relative to when we
  started streaming, and is calculated with the following formula:
  timestamp = (sec*1000000+usec)*1000
*/
__s64 tveng_get_timestamp(tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_get_timestamp(info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_get_timestamp(info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/* 
   Sets the capture buffer to an specific size. returns -1 on
   error. Remember to check the value of width and height since it can
   be different to the one requested. 
*/
int tveng_set_capture_size(int width, int height, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(width > 0);
  t_assert(height > 0);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_set_capture_size(width, height, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_set_capture_size(width, height, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/* 
   Gets the actual size of the capture buffer in width and height.
   -1 on error
*/
int tveng_get_capture_size(int *width, int *height, tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_get_capture_size(width, height, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_get_capture_size(width, height, info);
    default:
      t_assert_not_reached();
    }

  return -1;
}

/* XF86 Frame Buffer routines */
/* 
   Detects the presence of a suitable Frame Buffer.
   1 if the program should continue (Frame Buffer present,
   available and suitable)
   0 if the framebuffer shouldn't be used.
   display: The display we are connected to (gdk_display)
   info: Its fb member is filled in
*/
int
tveng_detect_XF86DGA(tveng_device_info * info)
{
#ifndef DISABLE_X_EXTENSIONS
  int event_base, error_base;
  int major_version, minor_version;
  int flags;

  Display * display = info->display;

  if (!XF86DGAQueryExtension(display, &event_base, &error_base))
    {
      perror("XF86DGAQueryExtension");
      return 0;
    }

  if (!XF86DGAQueryVersion(display, &major_version, &minor_version))
    {
      perror("XF86DGAQueryVersion");
      return 0;
    }

  if (!XF86DGAQueryDirectVideo(display, 0, &flags))
    {
      perror("XF86DGAQueryDirectVideo");
      return 0;
    }

  /* Direct Video should be present (otherwise enabling all this would
     be pointless) */
  if (!(flags & XF86DGADirectPresent))
    {
      printf("flags & XF86DGADirectPresent\n");
      return 0;
    }

/* Print collected info if we are in debug mode */
  printf("DGA info:\n");
  printf("  - event and error base  : %d, %d\n", event_base, error_base);
  printf("  - DGA reported version  : %d.%d\n", major_version, minor_version);
  printf("  - Supported features    :%s\n",
	 (flags & XF86DGADirectPresent) ? " DirectVideo" : "");

  return 1; /* Everything correct */
#else
  return 0; /* disabled by configure */
#endif
}

/*
  Returns 1 if the device attached to info suports previewing, 0 otherwise
*/
int
tveng_detect_preview (tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_detect_preview(info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_detect_preview(info);
    default:
      t_assert_not_reached();
    }

  return 0;
}

/* 
   Runs zapping_setup_fb with the actual verbosity value.
   Returns -1 in case of error, 0 otherwise.
   This calls (or tries to) the external program zapping_setup_fb,
   that should be installed as suid root.
*/
int
tveng_run_zapping_setup_fb(tveng_device_info * info)
{
  char * argv[8]; /* The command line passed to zapping_setup_fb */
  pid_t pid; /* New child's pid as returned by fork() */
  int status; /* zapping_setup_fb returned status */
  int i=0;
  int verbosity = info->zapping_setup_fb_verbosity;

  /* Executes zapping_setup_fb with the given arguments */
  argv[i++] = "zapping_setup_fb";
  argv[i++] = "--device";
  argv[i++] = info -> file_name;

  /* Clip verbosity to valid values */
  if (verbosity < 0)
    verbosity = 0;
  else if (verbosity > 2)
    verbosity = 2;
  for (; verbosity > 0; verbosity --)
    argv[i++] = "--verbose";
  argv[i] = NULL;
  
  pid = fork();

  if (pid == -1)
    {
      info->tveng_errno = errno;
      t_error("fork()", info);
      return -1;
    }

  if (!pid) /* New child process */
    {
      execvp("zapping_setup_fb", argv);

      /* This shouldn't be reached if everything suceeds */
      perror("execvp");

      _exit(2); /* zapping setup_fb on error returns 1 */
    }

  /* This statement is only reached by the parent */
  if (waitpid(pid, &status, 0) == -1)
    {
      info->tveng_errno = errno;
      t_error("waitpid", info);
      return -1;
    }

  if (! WIFEXITED(status)) /* zapping_setup_fb exited abnormally */
    {
      info->tveng_errno = errno;
      t_error_msg("WIFEXITED(status)", 
		  _("zapping_setup_fb exited abnormally, check stderr"),
		  info);
      return -1;
    }

  switch (WEXITSTATUS(status))
    {
    case 1:
      info -> tveng_errno = -1;
      t_error_msg("case 1",
		  _("zapping_setup_fb failed to set up the video"), info);
      return -1;
    case 2:
      info -> tveng_errno = -1;
      t_error_msg("case 2",
		  _("Couldn't locate zapping_setup_fb, check your install"),
		  info);
      return -1;
    default:
      break; /* Exit code == 0, success setting up the framebuffer */
    }
  return 0; /* Success */
}

/* 
   This is a convenience function, it returns the real screen depth in
   BPP (bits per pixel). This one is quite important for 24 and 32 bit
   modes, since the default X visual may be 24 bit and the real screen
   depth 32, thus an expensive RGB -> RGBA conversion must be
   performed for each frame.
   display: the display we want to know its real depth (can be
   accessed thorugh gdk_display)
*/
int
tveng_get_display_depth(tveng_device_info * info)
{
  /* This routines are taken form xawtv, i don't understand them very
     well, but they seem to work OK */
  XVisualInfo * visual_info, template;
  XPixmapFormatValues * pf;
  Display * display = info->display;
  int found, v, i, n;
  int bpp = 0;

  /* Use the first screen, should give no problems assuming this */
  template.screen = 0;
  visual_info = XGetVisualInfo(display, VisualScreenMask, &template, &found);
  v = -1;
  for (i = 0; v == -1 && i < found; i++)
    if (visual_info[i].class == TrueColor && visual_info[i].depth >=
	15)
      v = i;

  if (v == -1) {
    info -> tveng_errno = -1;
    t_error_msg("XGetVisualInfo",
		_("Cannot find an appropiate visual"), info);
    XFree(visual_info);
    return 0;
  }
  
  /* get depth + bpp (heuristic) */
  pf = XListPixmapFormats(display,&n);
  for (i = 0; i < n; i++) {
    if (pf[i].depth == visual_info[v].depth) {
      if (visual_info[v].depth == 15)
	bpp = 15; /* here bits_per_pixel is 16, but the depth is 15 */
      else
	bpp   = pf[i].bits_per_pixel;
      break;
    }
  }

  if (bpp == 0) {
    info -> tveng_errno = -1;
    t_error_msg("XListPixmapFormats",
		_("Cannot figure out X depth"), info);
    XFree(visual_info);
    XFree(pf);
    return 0;
  }

  XFree(visual_info);
  XFree(pf);
  return bpp;
}

/*
  Sets the preview window dimensions to the given window.
  Returns -1 on error, something else on success.
  Success doesn't mean that the requested dimensions are used, maybe
  they are different, check the returned fields to see if they are suitable
  info   : Device we are controlling
  The current chromakey value is used, the caller doesn't need to fill
  it in.
*/
int
tveng_set_preview_window(tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_set_preview_window(info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_set_preview_window(info);
    default:
      t_assert_not_reached();
    }

  return 0;
}

/*
  Gets the current overlay window parameters.
  Returns -1 on error, and any other value on success.
  info   : The device to use
*/
int
tveng_get_preview_window(tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_get_preview_window(info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_get_preview_window(info);
    default:
      t_assert_not_reached();
    }

  return 0;
}

/* 
   Sets the previewing on/off.
   on : if 1, set preview on, if 0 off, other values are silently ignored
   info  : device to use for previewing
   Returns -1 on error, anything else on success
*/
int
tveng_set_preview (int on, tveng_device_info * info)
{
  t_assert(info != NULL);

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_set_preview(on, info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_set_preview(on, info);
    default:
      t_assert_not_reached();
    }

  return 0;
}

/* Adjusts the verbosity value passed to zapping_setup_fb, cannot fail
 */
void
tveng_set_zapping_setup_fb_verbosity(int level, tveng_device_info *
				     info)
{
  t_assert(info != NULL);
  if (level > 2)
    level = 2;
  else if (level < 0)
    level = 0;
  info->zapping_setup_fb_verbosity = level;
}

/* Returns the current verbosity value passed to zapping_setup_fb */
int
tveng_get_zapping_setup_fb_verbosity(tveng_device_info * info)
{
  return (info->zapping_setup_fb_verbosity);
}

/* 
   Sets up everything and starts previewing.
   Just call this function to start previewing, it takes care of
   (mostly) everything.
   Returns -1 on error.
*/
int
tveng_start_previewing (tveng_device_info * info)
{
#ifndef DISABLE_X_EXTENSIONS
  int event_base, error_base, major_version, minor_version;
  XF86VidModeModeInfo ** modesinfo;
  int modecount;
  int i;
  int chosen_mode=0; /* The video mode we will use for fullscreen */
  int distance=-1; /* Distance of the best video mode to a valid size */
  int temp; /* Temporal value */
  int bigger=0; /* Allow choosing bigger screen depths */
#endif

  t_assert(info != NULL);

#ifndef DISABLE_X_EXTENSIONS
  info -> xf86vm_enabled = 1;

  if (!XF86VidModeQueryExtension(info->display, &event_base,
				 &error_base))
    {
      info->tveng_errno = -1;
      t_error_msg("XF86VidModeQueryExtension",
		  "No vidmode extension supported", info);
      info->xf86vm_enabled = 0;
    }

  if ((info->xf86vm_enabled) &&
      (!XF86VidModeQueryVersion(info->display, &major_version,
				&minor_version)))
    {
      info->tveng_errno = -1;
      t_error_msg("XF86VidModeQueryVersion",
		  "No vidmode extension supported", info);
      info->xf86vm_enabled = 0;      
    }

  if ((info->xf86vm_enabled) &&
      (!XF86VidModeGetAllModeLines(info->display, 
				   DefaultScreen(info->display),
				   &modecount, &modesinfo)))
    {
      info->tveng_errno = -1;
      t_error_msg("XF86VidModeGetAllModeLines",
		  "No vidmode extension supported", info);
      info->xf86vm_enabled = 0;
    }

  if (info -> xf86vm_enabled)
    {
      fprintf(stderr, "XF86VidMode info:\n");
      fprintf(stderr, "  - event and error base  : %d, %d\n", event_base,
	      error_base);
      fprintf(stderr, "  - XF86VidMode version   : %d.%d\n",
	      major_version, minor_version);
      fprintf(stderr, "  - Available video modes : %d\n", modecount);

    loop_point:
      for (i = 0; i<modecount; i++)
	{
	  if (!bigger) /* print only once */
	    fprintf(stderr, "      %d) %dx%d @ %d Hz\n", i, (int)
		    modesinfo[i]->hdisplay, (int) modesinfo[i]->vdisplay,
		    (int) modesinfo[i]->dotclock);
	  /* Check whether this is a good value */
	  temp = ((int)modesinfo[i]->hdisplay) - info->caps.maxwidth;
	  temp *= temp;
	  temp += (((int)modesinfo[i]->vdisplay) - info->caps.maxheight) *
	    (((int)modesinfo[i]->vdisplay) - info->caps.maxheight);
	  if  (((modesinfo[i]->hdisplay < info->caps.maxwidth) &&
		(modesinfo[i]->vdisplay < info->caps.maxheight))
	       || (bigger))
	    {
	      if ((distance == -1) || (temp < distance))
		{
		  /* This heuristic is somewhat krusty, but it should mostly
		     work */
		chosen_mode = i;
		distance = temp;
		}
	    }
	}

      if (distance == -1) /* No adequate size smaller than the given
			     one, get the nearest bigger one */
	{
	  bigger = 1;
	  goto loop_point;
	}
      
      fprintf(stderr, "      Mode # %d chosen\n", chosen_mode);
      
      /* If the chosen mode isn't the actual one, choose it, but
	 place the viewport correctly first */
      if (chosen_mode == 0)
	{
	  info->restore_mode = 0;
	  XF86DGASetViewPort(info->display,
			     DefaultScreen(info->display), 0, 0);
	}
      else
	{
	  info->restore_mode = 1;
	  memcpy(&(info->modeinfo), modesinfo[0],
		 sizeof(XF86VidModeModeInfo));
	  if (!XF86VidModeSwitchToMode(info->display,
				       DefaultScreen(info->display),
				       modesinfo[chosen_mode]))
	    info -> xf86vm_enabled = 0;

	  /* Place the viewport again */
	  if (info -> xf86vm_enabled)
	    XF86DGASetViewPort(info->display,
			       DefaultScreen(info->display), 0, 0);
	}
      
      XFree(modesinfo);
    }
  else /* info -> xf86vm_enabled*/
    {
      fprintf(stderr, "XF86VidMode not enabled: %s\n", info -> error);
    }
#endif /* DISABLE_X_EXTENSIONS */

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_start_previewing(info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_start_previewing(info);
    default:
      t_assert_not_reached();
    }

  return 0;
}

/*
  Stops the fullscreen mode. Returns -1 on error
*/
int
tveng_stop_previewing(tveng_device_info * info)
{
  t_assert(info != NULL);

#ifndef DISABLE_X_EXTENSIONS
  if ((info->restore_mode) && (info->xf86vm_enabled))
    XF86VidModeSwitchToMode(info->display,
			    DefaultScreen(info->display),
			    &(info->modeinfo));
#endif

  switch (info -> current_controller)
    {
    case TVENG_CONTROLLER_NONE:
      fprintf(stderr, _("%s called on a non-controlled device: %s\n"),
	      __PRETTY_FUNCTION__, info -> file_name);
      break;
    case TVENG_CONTROLLER_V4L1:
      return tveng1_stop_previewing(info);
    case TVENG_CONTROLLER_V4L2:
      return tveng2_stop_previewing(info);
    default:
      t_assert_not_reached();
    }

  return 0;
}

/*
  Sets up everything and starts previewing in a window. It doesn't do
  many of the things tveng_start_previewing does, it's mostly just a
  wrapper around tveng_set_preview_on. Returns -1 on error
  The window must be specified from before calling this function (with
  tveng_set_preview_window), and overlaying must be available.
*/
int
tveng_start_window (tveng_device_info * info)
{
  tveng_stop_everything(info);

  t_assert(info -> current_mode == TVENG_NO_CAPTURE);

  if (!tveng_detect_preview(info))
    /* We shouldn't be reaching this if the app is well programmed */
    t_assert_not_reached();

  if (tveng_set_preview_on(info) == -1)
    return -1;

  info->current_mode = TVENG_CAPTURE_WINDOW;
  return 0;
}

/*
  Stops the window mode. Returns -1 on error
*/
int
tveng_stop_window (tveng_device_info * info)
{
  if (info -> current_mode == TVENG_NO_CAPTURE)
    {
      fprintf(stderr, 
	      _("Warning: trying to stop window with no capture active\n"));
      return 0; /* Nothing to be done */
    }

  t_assert(info->current_mode == TVENG_CAPTURE_WINDOW);

  /* No error checking */
  tveng_set_preview_off(info);

  info -> current_mode = TVENG_NO_CAPTURE;
  return 0; /* Success */
}

/*
  Utility function, stops the capture or the previewing. Returns the
  mode the device was before stopping.
  For stopping and restarting the device do:
  enum tveng_capture_mode cur_mode;
  cur_mode = tveng_stop_everything(info);
  ... do some stuff ...
  if (tveng_restart_everything(cur_mode, info) == -1)
     ... show error dialog ...
*/
enum tveng_capture_mode tveng_stop_everything (tveng_device_info *
					       info)
{
  enum tveng_capture_mode returned_mode;

  returned_mode = info->current_mode;

  switch (info->current_mode)
    {
    case TVENG_CAPTURE_READ:
      tveng_stop_capturing(info);
      break;
    case TVENG_CAPTURE_PREVIEW:
      tveng_stop_previewing(info);
      break;
    case TVENG_CAPTURE_WINDOW:
      tveng_stop_window(info);
      break;
    default:
      break;
    };

  return returned_mode;
}

/*
  Restarts the given capture mode. See the comments on
  tveng_stop_everything. Returns -1 on error.
*/
int tveng_restart_everything (enum tveng_capture_mode mode,
			      tveng_device_info * info)
{
  switch (mode)
    {
    case TVENG_CAPTURE_READ:
      if (tveng_start_capturing(info) == -1)
	return -1;
      break;
    case TVENG_CAPTURE_PREVIEW:
      if (tveng_start_previewing(info) == -1)
	return -1;
      break;
    case TVENG_CAPTURE_WINDOW:
      if (tveng_start_window(info) == -1)
	return -1;
      break;
    default:
      break;
    }
  return 0; /* Success */
}

void tveng_start_timer(tveng_device_info * info)
{
  gettimeofday(&(info->tv_init), NULL);
}
