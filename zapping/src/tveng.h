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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

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
#include <linux/kernel.h>
#include <linux/types.h>
#include <errno.h>

/* We need video extensions (DGA) */
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xfuncs.h>
#ifndef DISABLE_X_EXTENSIONS
#include <X11/extensions/xf86dga.h>
#include <X11/extensions/xf86vmode.h>
#endif

/* i18n support if it hasn't been defined previously */
#ifndef gettext
#include <libintl.h>

#ifndef _
#define _(String) gettext (String)
#endif
#ifndef N_
#define N_(String) (String)
#endif
#endif

/* The video device capabilities flags */
#define TVENG_CAPS_CAPTURE 1 /* Can capture to memory */
#define TVENG_CAPS_TUNER (1 << 1) /* Has some tuner */
#define TVENG_CAPS_TELETEXT (1 << 2) /* Has teletext */
#define TVENG_CAPS_OVERLAY (1 << 3) /* Can overlay to the framebuffer */
#define TVENG_CAPS_CHROMAKEY (1 << 4) /* Overlay chromakeyed */
#define TVENG_CAPS_CLIPPING (1 << 5) /* Overlay clipping supported */
#define TVENG_CAPS_FRAMERAM (1 << 6) /* Overlay overwrites framebuffer
				      mem */
#define TVENG_CAPS_SCALES (1 << 7) /* HW image scaling supported */
#define TVENG_CAPS_MONOCHROME (1 << 8) /* greyscale only */
#define TVENG_CAPS_SUBCAPTURE (1 << 9) /* Can capture only part of the
					image */

/* The valid modes for opening the video device */
enum tveng_attach_mode
{
  /*
    Attachs the device so you can only control it, not read
    frames. This way you can do more than one opens per device. This
    will only work if V4L2 is present, otherwise this call means the
    same as TVENG_ATTACH_READ
  */
  TVENG_ATTACH_CONTROL,
  /*
    Attachs the device so you can read data from it and control
    it. You cannot attach the same device twice with this type of
    attachment.
  */
  TVENG_ATTACH_READ
};

/* The capture structure */
struct tveng_caps{
  char name[32]; /* canonical name for this interface */
  int flags; /* OR'ed flags, see the above #defines */
  int channels; /* Number of radio/tv channels */
  int audios; /* Number of audio devices */
  int maxwidth, maxheight; /* Maximum capture dimensions */
  int minwidth, minheight; /* minimum capture dimensions */
};

/* frame buffer info */
struct tveng_fb_info{
  void * base; /* Physical address for the FB */
  int height, width; /* Width and height (physical, not window dimensions) */
  int depth; /* FB depth in bits */
  int bytesperline; /* Bytesperline in the image */
};

/* Description of a clip rectangle */
struct tveng_clip{
  int x,y; /* Origin (X coordinates) */
  int width, height; /* Dimensions */
};

/* a capture window (for the fb) */
struct tveng_window{
  int x,y; /* Origin in X coordinates */
  int width, height; /* Dimensions */
  unsigned int chromakey; /* The chromakey (RGB32) */
  int clipcount; /* Number of clipping rectangles */
  struct tveng_clip * clips; /* pointer to the clip rectangle array */
};

enum tveng_field
{
  TVENG_FIELD_ODD, /* Only odd field */
  TVENG_FIELD_EVEN, /* Only even field */
  TVENG_FIELD_BOTH /* Capture both field */
};

/* The format of a pixel, similar to the V4L2 ones, but they aren't
   fourcc'ed */
enum tveng_frame_pixformat{
  TVENG_PIX_RGB555, /* This one isn't supported yet */
  TVENG_PIX_RGB565,
  TVENG_PIX_RGB24,
  TVENG_PIX_BGR24,
  TVENG_PIX_RGB32,
  TVENG_PIX_BGR32
};

/* This struct holds the structure of the captured frame */
struct tveng_frame_format
{
  int width, height; /* Dimensions of the capture */
  int bytesperline; /* Bytes per scan line */
  int depth; /* Bits per pixel */
  enum tveng_frame_pixformat pixformat; /* The pixformat entry */
  int bpp; /* Bytes per pixel */
  int sizeimage; /* Size in bytes of the image */
};

/* Info about a standard */
struct tveng_enumstd{
  int id; /* Standard id */
  int index; /* Index in info->standards */
  char name[32]; /* Canonical name for the standard */
};

/* Flags for the input */
#define TVENG_INPUT_TUNER 1      /* has tuner(s) attached */
#define TVENG_INPUT_AUDIO (1<<1) /* has audio */

enum tveng_input_type{
  TVENG_INPUT_TYPE_TV,
  TVENG_INPUT_TYPE_CAMERA
};

/* info about an input */
struct tveng_enum_input{
  int id; /* Id of the input */
  char name[32]; /* Canonical name for the input */
  int tuners; /* Number of tuners for this input */
  int flags; /* Flags for this channel */
  enum tveng_input_type type; /* The type for this input */
};

/* Possible control types */
enum tveng_control_type{
  TVENG_CONTROL_SLIDER, /* It can take any value between min and max */ 
  TVENG_CONTROL_CHECKBOX, /* Can only take boolean values */
  TVENG_CONTROL_MENU, /* The control is a menu with max options and
			 labels listed in the data struct */
  TVENG_CONTROL_BUTTON /* The control is a button (when assigned a value does
		  something, regarless the value given to it) */
};

/* info about a video control (could be image, sound, whatever) */
struct tveng_control{
  char name[32]; /* Canonical name */
  int id; /* control id */
  int min, max; /* Control ranges */
  int cur_value; /* The current control value */
  enum tveng_control_type type; /* The control type */
  char ** data; /* If this is a menu entry, pointer to a array of
		   pointers to the labels, ended by a NULL pointer */
};

enum tveng_capture_mode
{
  TVENG_CAPTURE_READ, /* Capture is through a read() call */
  TVENG_CAPTURE_PREVIEW, /* Capture is through previewing */
  TVENG_NO_CAPTURE /* Capture isn't active */
};

/* The controller we are using for this device */
enum tveng_controller
{
  TVENG_CONTROLLER_NONE, /* No controller set */
  TVENG_CONTROLLER_V4L1, /* V4L1 controller (old V4l spec) */
  TVENG_CONTROLLER_V4L2  /* V4L2 controller (new v4l spec) */
};

/* The structure used to hold info about a video_device */
typedef struct
{
  char * file_name; /* The name used to open() this fd */
  int fd; /* Video device file descriptor */
  enum tveng_capture_mode current_mode; /* Current capture mode */
  enum tveng_attach_mode attach_mode; /* Mode this was attached with
				       */
  enum tveng_controller current_controller; /* Controller used */
  struct tveng_caps caps; /* Video system capabilities */
  int num_standards; /*
			Number of standards supported by this device
		      */
  int cur_standard; /* Index of cur_standard in standards */
  struct tveng_enumstd *standards; /* Standards supported */

  int num_inputs; /* Number of inputs in this device */
  int cur_input; /* Currently selected input */
  struct tveng_enum_input * inputs; /* Video inputs in this device */

  /* the format about this capture */
  struct tveng_frame_format format; /* pixel format of this device */

  /* Framebuffer info */
  struct tveng_fb_info fb_info;

  /* Overlay window */
  struct tveng_window window;

  /* Number of items in controls */
  int num_controls;
  /* The supported controls */
  struct tveng_control * controls;

  int tveng_errno; /* Numerical id of the last error, 0 == success */
  char * error; /* points to the last error message */

  /* Consider the following as private data, you don't need to access
     this */
  Display * display;

#ifndef DISABLE_X_EXTENSIONS
  XF86VidModeModeInfo modeinfo;
  int restore_mode;
  int xf86vm_enabled;
#endif

  int zapping_setup_fb_verbosity;
}
tveng_device_info;

/* Starts a tveng_device_info object, returns a pointer to the object
   or NULL on error. Display is the display we are connected to */
tveng_device_info * tveng_device_info_new(Display * display);

/* Destroys a tveng_device_info object */
void tveng_device_info_destroy(tveng_device_info * info);

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
int tveng_attach_device(const char* device_file,
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
  The returned pointer are statically allocated, i.e., they don't need
  to be freed.
*/
void
tveng_describe_controller(char ** short_str, char ** long_str,
			  tveng_device_info * info);

/*
  Closes the video device asocciated to the device info object. Should
  be called before reattaching a video device to the same object, but
  there is no need to call this before calling tveng_device_info_destroy.
*/
void tveng_close_device(tveng_device_info* info);

/*
  Functions for controlling the video capture. All of them return -1
  in case of error, so any value != -1 should be considered valid
  (unless explicitly stated in the description of the function) 
*/
/*
  Returns the number of inputs in the given device and fills in info,
  allocating memory as needed
*/
int tveng_get_inputs(tveng_device_info * info);

/*
  Sets the current input for the capture
*/
int tveng_set_input(struct tveng_enum_input * input, tveng_device_info
		    * info);

/*
  Sets the input named name as the active input. -1 on error
  (info->error states the exact error)
*/
int
tveng_set_input_by_name(const char * name, tveng_device_info * info);

/*
  Sets the active input by its id. -1 on error
*/
int
tveng_set_input_by_id(int id, tveng_device_info * info);

/*
  Sets the active input by its index in inputs. -1 on error
*/
int
tveng_set_input_by_index(int index, tveng_device_info * info);

/*
  Queries the device about its standards. Fills in info as appropiate
  and returns the number of standards in the device.
*/
int tveng_get_standards(tveng_device_info * info);

/*
  Sets the given standard as the current standard
*/
int 
tveng_set_standard(struct tveng_enumstd * std, tveng_device_info * info);

/*
  Sets the standard by name. -1 on error
*/
int
tveng_set_standard_by_name(char * name, tveng_device_info * info);

/*
  Sets the standard by id. -1 on error
*/
int
tveng_set_standard_by_id(int id, tveng_device_info * info);

/*
  Sets the standard by index. -1 on error
*/
int
tveng_set_standard_by_index(int index, tveng_device_info * info);

/* Updates the current capture format info. -1 if failed */
int
tveng_update_capture_format(tveng_device_info * info);

/* -1 if failed. Sets the format and fills in info -> format
   with the correct values  */
int
tveng_set_capture_format(tveng_device_info * info);

/*
  Gets the current value of the controls, fills in info->controls
  appropiately. After this (and if it succeeds) you can look in
  info->controls to get the values for each control. -1 on error
*/
int
tveng_update_controls(tveng_device_info * info);

/*
  Sets the value for an specific control. The given value will be
  clipped between min and max values. Returns -1 on error
*/
int
tveng_set_control(struct tveng_control * control, int value,
		  tveng_device_info * info);

/*
  Gets the value of a control, given its name. Returns -1 on
  error. The comparison is performed disregarding the case. The value
  read is stored in cur_value.
*/
int
tveng_get_control_by_name(const char * control_name,
			  int * cur_value,
			  tveng_device_info * info);

/*
  Sets the value of a control, given its name. Returns -1 on
  error. The comparison is performed disregarding the case.
  new_value holds the new value given to the control, and it is
  clipped as neccessary.
*/
int
tveng_set_control_by_name(const char * control_name,
			  int new_value,
			  tveng_device_info * info);

/*
  Gets the value of a control, given its control id. -1 on error (or
  cid not found). The result is stored in cur_value.
*/
int
tveng_get_control_by_id(int cid, int * cur_value,
			tveng_device_info * info);

/*
  Sets a control by its id. Returns -1 on error
*/
int tveng_set_control_by_id(int cid, int new_value,
			    tveng_device_info * info);

/*
  Gets the value of the mute property. 1 means mute (no sound) and 0
  unmute (sound). -1 on error
*/
int
tveng_get_mute(tveng_device_info * info);

/*
  Sets the value of the mute property. 0 means unmute (sound) and 1
  mute (no sound). -1 on error
*/
int
tveng_set_mute(int value, tveng_device_info * info);

/*
  Tunes the current input to the given freq. Returns -1 on error.
*/
int
tveng_tune_input(__u32 freq, tveng_device_info * info);

/*
  Gets the signal strength and the afc code. The afc code indicates
  how to get a better signal, if negative, tune higher, if negative,
  tune lower. 0 means no idea of feature not present in the current
  controller (i.e. V4L1). Strength and/or afc can be NULL pointers,
  that would mean ignore that parameter.
*/
int
tveng_get_signal_strength (int *strength, int * afc,
			   tveng_device_info * info);

/*
  Stores in freq the currently tuned freq. Returns -1 on error.
*/
int
tveng_get_tune(__u32 * freq, tveng_device_info * info);

/*
  Gets the minimum and maximum freq that the current input can
  tune. If there is no tuner in this input, -1 will be returned.
  If any of the pointers is NULL, its value will not be filled.
*/
int
tveng_get_tuner_bounds(__u32 * min, __u32 * max, tveng_device_info *
		       info);

/*
  Sets up the capture device so any read() call after this one
  succeeds. Returns -1 on error.
*/
int
tveng_start_capturing(tveng_device_info * info);

/* Tries to stop capturing. -1 on error. */
int
tveng_stop_capturing(tveng_device_info * info);

/* 
   Reads a frame from the video device, storing the read data in
   info->format.data
   time: time to wait using select() in miliseconds
   info: pointer to the video device info structure
   Returns -1 on error, anything else on success.
   Note: if you want this call to be non-blocking, call it with time=0
*/
int tveng_read_frame(void * where, unsigned int size,
		     unsigned int time, tveng_device_info * info);

/*
  Gets the timestamp of the last read frame.
  Returns -1 on error, if the current mode isn't capture, or if we
  haven't captured any frame yet. The timestamp is relative to when we
  started streaming, and is calculated with the following formula:
  timestamp = (sec*1000000+usec)*1000
*/
__s64 tveng_get_timestamp(tveng_device_info * info);

/* 
   Sets the capture buffer to an specific size. returns -1 on
   error. Remember to check the value of width and height in the
   format struct since it can be different to the one requested. 
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
   1 if the program should continue (Frame Buffer present,
   available and suitable)
   0 if the framebuffer shouldn't be used.
   display: The display we are connected to (gdk_display)
   info: Its fb member is filled in
*/
int
tveng_detect_XF86DGA(tveng_device_info * info);

/*
  Returns 1 if the device attached to info suports previewing, 0 otherwise
*/
int
tveng_detect_preview (tveng_device_info * info);

/* 
   Runs zapping_setup_fb with the actual verbosity value.
   Returns -1 in case of error, 0 otherwise.
   This calls (or tries to) the external program zapping_setup_fb,
   that should be installed as suid root.
*/
int
tveng_run_zapping_setup_fb(tveng_device_info * info);

/* 
   This is a convenience function, it returns the real screen depth in
   BPP (bits per pixel). This one is quite important for 24 and 32 bit
   modes, since the default X visual may be 24 bit and the real screen
   depth 32, thus an expensive RGB -> RGBA conversion must be
   performed for each frame.
*/
int
tveng_get_display_depth(tveng_device_info * info);

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
tveng_set_preview_window(tveng_device_info * info);

/*
  Gets the current overlay window parameters.
  Returns -1 on error, and any other value on success.
  info   : The device to use
*/
int
tveng_get_preview_window(tveng_device_info * info);

/* Some useful macros for the following function */
#define ON 1
#define OFF 0
#define tveng_set_preview_on(INFO) tveng_set_preview (ON, INFO)
#define tveng_set_preview_off(INFO) tveng_set_preview (OFF, INFO)

/* 
   Sets the previewing on/off.
   on : if 1, set preview on, if 0 off, other values are silently ignored
   info  : device to use for previewing
   Returns -1 on error, anything else on success
*/
int
tveng_set_preview (int on, tveng_device_info * info);

/* Adjusts the verbosity value passed to zapping_setup_fb, cannot fail
 */
void
tveng_set_zapping_setup_fb_verbosity(int level, tveng_device_info * info);

/* Returns the current verbosity value passed to zapping_setup_fb */
int
tveng_get_zapping_setup_fb_verbosity(tveng_device_info * info);

/* 
   Sets up everything and starts previewing.
   Just call this function to start previewing, it takes care of
   (mostly) everything.
   Returns -1 on error.
*/
int
tveng_start_previewing (tveng_device_info * info);

/*
  Stops the fullscreen mode. Returns -1 on error
*/
int
tveng_stop_previewing(tveng_device_info * info);

/* Some utility functions a la glib */
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
					       info);

/*
  Restarts the given capture mode. See the comments on
  tveng_stop_everything. Returns -1 on error.
*/
int tveng_restart_everything (enum tveng_capture_mode mode,
			      tveng_device_info * info);

/* Sanity checks should use this */
#define t_assert(condition) if (!(condition)) { \
fprintf(stderr, _("%s (%d): %s: assertion (%s) failed\n"), __FILE__, \
__LINE__, __PRETTY_FUNCTION__, #condition); \
exit(1);}

/* Builds an error message that lets me debug much better */
#define t_error(str_error, info) \
t_error_msg(str_error, strerror(info->tveng_errno), info)

/* Builds a custom error message, doesn't use errno */
#define t_error_msg(str_error, msg_error, info) \
snprintf(info->error, 256, _("[%s] %s (line %d)\n%s failed: %s"), \
__FILE__, __PRETTY_FUNCTION__, __LINE__, str_error, msg_error)

/* Defines a point that should never be reached */
#define t_assert_not_reached() \
fprintf(stderr, \
_("[%s: %d: %s] This should have never been reached\n" ), __FILE__, \
__LINE__, __PRETTY_FUNCTION__)

#endif /* TVENG.H */
