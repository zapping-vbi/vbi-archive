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

#ifndef __TVENG2_H__
#define __TVENG2_H__

#include "tveng_private.h"

/*
  Inits the V4L2 module, and fills in the given table.
*/
void tveng2_init_module(struct tveng_module_info *module_info);

/*
  Prototypes for forward declaration, used only in tveng2.c
*/
#ifdef TVENG2_PROTOTYPES
/*
  Associates the given tveng_device_info with the given video
  device. On error it returns -1 and sets info->errno, info->error to
  the correct values.
  device_file: The file used to access the video device (usually
  /dev/video)
  attach_mode: Specifies the mode to open the device file
  depth: The color depth the capture will be in, -1 means let tveng
  decide based on the current display depth.
  info: The structure to be associated with the device
*/
static
int tveng2_attach_device(const char* device_file,
			 enum tveng_attach_mode attach_mode,
			 tveng_device_info * info);

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
tveng2_describe_controller(char ** short_str, char ** long_str,
			   tveng_device_info * info);

/*
  Closes the video device asocciated to the device info object. Should
  be called before reattaching a video device to the same object, but
  there is no need to call this before calling tveng_device_info_destroy.
*/
static void tveng2_close_device(tveng_device_info* info);

/*
  Functions for controlling the video capture. All of them return -1
  in case of error, so any value != -1 should be considered valid
  (unless explicitly stated in the description of the function) 
*/
/*
  Returns the number of inputs in the given device and fills in info,
  allocating memory as needed
*/
static int tveng2_get_inputs(tveng_device_info * info);

/*
  Sets the current input for the capture
*/
static
int tveng2_set_input(struct tveng_enum_input * input, tveng_device_info
		     * info);

/*
  Queries the device about its standards. Fills in info as appropiate
  and returns the number of standards in the device.
*/
static int tveng2_get_standards(tveng_device_info * info);

/*
  Sets the given standard as the current standard
*/
static int 
tveng2_set_standard(struct tveng_enumstd * std, tveng_device_info * info);

/* Updates the current capture format info. -1 if failed */
static int
tveng2_update_capture_format(tveng_device_info * info);

/* -1 if failed. Sets the format and fills in info -> format
   with the correct values  */
static int
tveng2_set_capture_format(tveng_device_info * info);


/*
  Sets the value for an specific control. The given value will be
  clipped between min and max values. Returns -1 on error
*/
static int
tveng2_set_control(tveng_device_info * info,
		   tv_control *tc, int value);
		   


/*
  Tunes the current input to the given freq. Returns -1 on error.
*/
static int
tveng2_tune_input(uint32_t freq, tveng_device_info * info);

/*
  Gets the signal strength and the afc code. The afc code indicates
  how to get a better signal, if negative, tune higher, if negative,
  tune lower. 0 means no idea of feature not present in the current
  controller (i.e. V4L1). Strength and/or afc can be NULL pointers,
  that would mean ignore that parameter.
*/
static int
tveng2_get_signal_strength (int *strength, int * afc,
			    tveng_device_info * info);

/*
  Stores in freq the currently tuned freq. Returns -1 on error.
*/
static int
tveng2_get_tune(uint32_t * freq, tveng_device_info * info);

/*
  Gets the minimum and maximum freq that the current input can
  tune. If there is no tuner in this input, -1 will be returned.
  If any of the pointers is NULL, its value will not be filled.
*/
static int
tveng2_get_tuner_bounds(uint32_t * min, uint32_t * max, tveng_device_info *
			info);

/*
  Sets up the capture device so any read() call after this one
  succeeds. Returns -1 on error.
*/
static int
tveng2_start_capturing(tveng_device_info * info);

/* Tries to stop capturing. -1 on error. */
static int
tveng2_stop_capturing(tveng_device_info * info);

/* 
   Reads a frame from the video device, storing the read data in
   info->format.data
   time: time to wait using select() in miliseconds
   info: pointer to the video device info structure
   Returns -1 on error, anything else on success.
   Note: if you want this call to be non-blocking, call it with time=0
*/
static
int tveng2_read_frame(tveng_image_data * where,
		      unsigned int time, tveng_device_info * info);

/*
  Gets the timestamp of the last read frame in seconds.
*/
static
double tveng2_get_timestamp(tveng_device_info * info);

/* 
   Sets the capture buffer to an specific size. returns -1 on
   error. Remember to check the value of width and height in the
   format struct since it can be different to the one requested. 
*/
static
int tveng2_set_capture_size(int width, int height, tveng_device_info *
			    info);

/* 
   Gets the actual size of the capture buffer in width and height.
   -1 on error
*/
static
int tveng2_get_capture_size(int *width, int *height, tveng_device_info * info);

/* XF86 Frame Buffer routines */
/*
  Returns 1 if the device attached to info suports previewing, 0 otherwise
*/
static int
tveng2_detect_preview (tveng_device_info * info);

/*
  Sets the preview window dimensions to the given window.
  Returns -1 on error, something else on success.
  Success doesn't mean that the requested dimensions are used, maybe
  they are different, check the returned fields to see if they are suitable
  info   : Device we are controlling
  The current chromakey value is used, the caller doesn't need to fill
  it in.
*/
static int
tveng2_set_preview_window(tveng_device_info * info);

/*
  Gets the current overlay window parameters.
  Returns -1 on error, and any other value on success.
  info   : The device to use
*/
static int
tveng2_get_preview_window(tveng_device_info * info);

/*
   Sets the previewing on/off.
   on : if 1, set preview on, if 0 off, other values are silently ignored
   info  : device to use for previewing
   Returns -1 on error, anything else on success
*/
static int
tveng2_set_preview (int on, tveng_device_info * info);

/* 
   Sets up everything and starts previewing.
   Just call this function to start previewing, it takes care of
   (mostly) everything.
   Returns -1 on error.
*/
static int
tveng2_start_previewing (tveng_device_info * info,
			 x11_dga_parameters *dga);

/*
  Stops the fullscreen mode. Returns -1 on error
*/
static int
tveng2_stop_previewing(tveng_device_info * info);

#endif /* TVENG2_PROTOTYPES */
#endif /* TVENG2.H */
