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

#ifndef __TVENG_H__
#define __TVENG_H__

#include <gnome.h> /* This file depends on Gnome and glib */

#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <errno.h>

/* We need a lot of header files so videodev doesn't give warnings */
#include "videodev.h"

/* We need video extensions (DGA) */
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xfuncs.h>
#include <X11/extensions/xf86dga.h>

/* i18n support */
#include "support.h"

/* Channel tuning by countries */
#include "frequencies.h"

/* FIXME: This isn't the correct place for this, should go in a
   standalone module (setup.c or likewise) */
struct ParseStruct
{
  gchar * name; /* The name to parse */
  gchar * format; /* The format to sscanf */
  gpointer * where; /* Where to store the value, make sure it has the
		       correct size for sscanf()'ing it */
  int max_length; /* If max_length > 0, "where" is supposed to be a
		     string and g_snprintf is used instead of sscanf */
};

typedef struct
{
  struct v4l2_buffer vidbuf; /* Info about the buffer */
  gpointer vmem; /* Captured image in this buffer */
}
tveng_vbuf;

/* The format of a pixel, similar to the V4L2 ones, but they aren't
   fourcc'ed */
enum tveng_frame_pixformat{
  TVENG_PIX_RGB555,
  TVENG_PIX_RGB565,
  TVENG_PIX_RGB24,
  TVENG_PIX_BGR24,
  TVENG_PIX_RGB32,
  TVENG_PIX_BGR32
};

/* This struct holds the structure of the captured frame */
struct tveng_frame_format
{
  gpointer data; /* A pointer to the captured data */
  int width, height; /* Dimensions of the capture */
  int bytesperline; /* Bytes per scan line */
  int depth; /* Bits per pixel */
  enum tveng_frame_pixformat pixformat; /* The pixformat entry */
  int bpp; /* Bytes per pixel */
  int sizeimage; /* Size in bytes of the image */
};

/* We need this for the callbacks */
typedef struct
{
  gpointer info; /* where is this stored */
  int id; /* id of this structure in the menu (changes) */
  struct v4l2_input input; /* The input structure itself */
}
tveng_input;

/* This is also for the callbacks, same meanings as above*/
typedef struct
{
  gpointer info;
  int id;
  struct v4l2_enumstd std;
} 
tveng_enumstd;

enum tveng_capture_mode
{
  TVENG_CAPTURE_MMAPED_BUFFERS, /* Capture is going to mmaped buffers */
  TVENG_CAPTURE_FULLSCREEN, /* Capture is fullscreen */
  TVENG_NO_CAPTURE /* There is not capture at the moment */
};

/* The structure used to hold info about a video_device */
typedef struct
{
  gchar * file_name; /* The name used to open() this fd */
  int fd; /* Video device file descriptor */
  int num_desired_buffers; /* Number of desired buffers */
  enum tveng_capture_mode current_mode; /* Current capture mode */
  struct v4l2_capability caps; /* Video system capabilities */
  int num_standards; /* 
			Number of standards supported by this device
		      */
  int cur_standard_index; /* Index of cur_standard in standards */
  struct v4l2_standard cur_standard; /* Currently selected standard */
  tveng_enumstd *standards; /* Standards supported */

  int num_inputs; /* Number of inputs in this device */
  int cur_input; /* Currently selected input */
  tveng_input * inputs; /* Video inputs in this device */
  int num_buffers; /* Capture buffers allocated for this device */
  tveng_vbuf * buffers; /* Array of mmap'ed capture buffers */
  struct v4l2_format pix_format; /* pixel format of this device */
  struct tveng_frame_format format; /* pixel format of this device */

  /* Framebuffer info */
  struct v4l2_framebuffer fb;

  gboolean interlaced; /* if TRUE the image will be interlaced */

  GdkImage * image; /* This image contains the image to be
		       drawn on screen */
  XImage * ximage; /* The XImage contained in image */
}
tveng_device_info;

/*
  Routine for opening a video device. Checks if the device is a video
  capture device (under v4l2 we can have other kinds of devices), if
  it has a tuner of any kind, if supports select() call, and if it
  supports mmap()'ing the device. If any of these fails, it shows a
  Gnome error box and returns with the code -1. Else it returns the
  file descriptor for the device and fills in the tveng_device_info
  structure.
  device_file usually is "/dev/video0",
  flags can be O_RDONLY (for capturing and controlling the device), or
  O_NONCAP for controlling only,
  info is the structure to be filled in.
*/
int tveng_init_device(const gchar* device_file, int flags,
		      tveng_device_info * info);

/*
  Used internally by tveng, should be used since its behaviour can
  change.
  Open the device that info points to and checks whether its a device
  file valid. Actually it does nearly all the job tveng_init_device
  performs. Anyway, you shouldn't use this (although it is useful
  sometimes, and thus it's here)
*/
int tveng_open_device_file(int flags, tveng_device_info * info);

/*
  Routine for closing a video device. Closes file descriptors and
  frees any memory malloc()'ed by tveng_init_device. Should be called for
  each device_opened.
*/
void tveng_close_device(tveng_device_info* info);

/*
  Functions for controlling the video capture. All of them return -1
  in case of error, so any value != -1 should be considered valid
  (unless explicitly stated in the description of the function) 
*/
/*
  Returns the number of inputs in the given device and fills in info
  with the correct info, allocating memory as needed
*/
int tveng_get_inputs(tveng_device_info * info);

/*
  Sets the current input for the capture
*/
int tveng_set_input(int input, tveng_device_info * info);

/*
  Sets the input named name as the active input. -1 on error.
*/
int
tveng_set_input_by_name(gchar * name, tveng_device_info * info);

/*
  Sets the current standard for the capture. standard is the index of
  the desired standard in standards.
*/
int tveng_set_standard(gchar * name, tveng_device_info * info);

/*
  Returns the number of standards in the given device and fills in info
  with the correct info, allocating memory as needed
*/
int tveng_get_standards(tveng_device_info * info);

/*
  Updates the value stored in cur_input. Returns -1 on error
*/
int tveng_update_input(tveng_device_info * info);

/*
  Updates the value stored in cur_standard. Returns -1 on error
*/
int tveng_update_standard(tveng_device_info * info);

/* -1 if failed */
int
tveng_get_capture_format(tveng_device_info * info);

/* -1 if failed. Sets the pixformat and fills in info -> pix_format
   with the correct values  */
int
tveng_set_capture_format(__u32 video_format, tveng_device_info * info);

/* Prints the name of the current pixformat and a newline */
void print_format(__u32 format);

/*
  Test if a given control is supported by the driver and fills in the
  structure qc with some info about this control. Returns EINVAL in
  case of failure. Return value of this function should be considered
  valid only if it is 0. Usually tveng_set_control will be used
  instead of this function directly.
*/
int tveng_test_control(int control_id, struct v4l2_queryctrl * qc,
		       tveng_device_info * info);

/*
  Set the value for specific controls. Some of this functions have
  wrappers, such as tveng_set_mute(). Returns 1 in case the value is
  lower than the allowed minimum, and 2 in case the value is higher
  than the allowed maximum. The value is set to the nearest valid
  value.
  Returns 0 on success
*/
int tveng_set_control(int control_id, int value,
		      tveng_device_info * info);

/*
  Gets the value of an specific control. Some of this functions have
  wrappers, such as tveng_get_mute(). The value is stored in the
  address value points to, if there is no error. In case of error, the
  function returns -1, and value is undefined
*/
int tveng_get_control(int control_id, int * value,
		      tveng_device_info * info);

/*
  Some useful macros about controls
*/
/*
  Sets and gets audio on/off. A value of TRUE means go mute, that is,
  no audio. Kind of weird, but v4l2 is designed this way.
*/
#define tveng_set_mute(value,info) \
 tveng_set_control(V4L2_CID_AUDIO_MUTE, value, info)
#define tveng_get_mute(value,info) \
 tveng_get_control(V4L2_CID_AUDIO_MUTE, value, info) 

/*
  Tunes a video input (if is hasn't a tuner this function fails with
  0) to the specified frequence (in kHz). If the given frequence is
  higher than rangehigh or lower than rangelow it is clipped.
  FIXME: What does afc in v4l2_tuner mean?
*/
int tveng_tune_input(int input, __u32 _freq, tveng_device_info * info);

/*
  Stores in freq the frequence the current input is tuned on. Returns
  -1 on error.
*/
int tveng_get_tune(__u32 * freq, tveng_device_info * info);

/*
  Start capturing frames to a memory buffer. Returns NULL on error,
  the address of the first mmap'ed buffer otherwise
  We should specify the number of different buffers to allocate using
  mmap() in info->num_desired_buffers.
*/
gpointer tveng_start_capturing(tveng_device_info * info);

/* Try to stop capturing. -1 on error */
int tveng_stop_capturing(tveng_device_info * info);

/* 
   Reads a frame from the video device, storing the read data in
   info->format.data
   info: pointer to the video device info structure
   Returns whatever read() returns
*/
int tveng_read_frame(tveng_device_info * info);

/* dequeues next available buffer and returns it's id. -1 on error */
int tveng_dqbuf(tveng_device_info * info);

/* Queues an specific buffer. -1 on error */
int tveng_qbuf(int buffer_id, tveng_device_info * info);

/* 
   Sets the capture buffer to an specific size. returns -1 on
   error. Remember to check the value of width and height since it can
   be different to the one requested. 
*/
int tveng_set_capture_size(int width, int height, tveng_device_info *
			   info);

/* 
   Gets the actual size of the capture buffer in width and height.
   -1 on error
*/
int tveng_get_capture_size(int *width, int *height, tveng_device_info * info);

/* XF86 Frame Buffer routines */
/* 
   Detects the presence of a suitable Frame Buffer.
   TRUE if the program should continue (Frame Buffer present,
   available and suitable)
   FALSE if the framebuffer shouldn't be used.
   FIXME: Many FB and Overlay routines require root access, make that
          routines into an external program with SUID root
	  privileges. Potential security flaw here.
   display: The display we are connected to (gdk_display)
   screen: The screen we will use
   info: Its fb member is filled in
*/
gboolean
tveng_detect_XF86DGA(Display * display, int screen, tveng_device_info
		     * info);

/*
  Checks if previewing is available for the desired device.
  Returns TRUE if success, FALSE on error
*/
gboolean
tveng_detect_preview (tveng_device_info * info);

/*
  Sets the preview window dimensions to the given window.
  Returns -1 on error, something else on success.
  Success doesn't mean that the requested dimensions are used, maybe
  they are different, check the returned fields to see if they are suitable
  window : Structure containing the window to use (including clipping)
  info   : Device we are controlling
  The current chromakey value is used, the caller doesn't need to fill
  it in.
*/
int
tveng_set_preview_window(struct v4l2_window * window,
			 tveng_device_info * info);

/*
  Gets the current overlay window parameters.
  Returns -1 on error, and any other value on success.
  window : Where to store the collected data
  info   : The device to use
*/
int
tveng_get_preview_window(struct v4l2_window * window,
			 tveng_device_info * info);

/* Some useful macros for the following function */
#define ON TRUE
#define OFF FALSE
#define tveng_set_preview_on(INFO) tveng_set_preview (ON, INFO)
#define tveng_set_preview_off(INFO) tveng_set_preview (OFF, INFO)

/* 
   Sets the previewing on/off.
   TRUE  : on
   FALSE : off
   info  : device to use for previewing
   Returns -1 on error, anything else on success
*/
int
tveng_set_preview (gboolean on, tveng_device_info * info);

/* 
   Sets up everything and starts previewing to Full Screen if possible.
   Just call this function to start fullscreen mode, it takes care of
   everything.
   There should be no capture active when calling this function
   Returns TRUE on success and FALSE on failure.
   Verbosity refers to the verbosity zapping_setup_fb should use
*/
gboolean
tveng_start_fullscreen_previewing (tveng_device_info * info, int
				   verbosity);

/*
  Stops the fullscreen mode. FALSE on error and TRUE on success
*/
gboolean
tveng_stop_fullscreen_previewing(tveng_device_info * info);

/*
  Shows a Gnome MessageBox. Shouldn't go here, but anyway... 
  message is the message that should be displayed,
  message_box_type is the kind of the message box (typically
  GNOME_MESSAGE_BOX_ERROR).
*/
int ShowBox(const gchar* message, const gchar* message_box_type);

#endif /* TVENG.H */
