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
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <X11/Xlib.h> /* We use some X calls */
#include <X11/Xutil.h>

/* This undef's are to avoid a couple of header warnings */
#undef WNOHANG
#undef WUNTRACED
#include "tveng.h"
#include "tveng1.h"
#include "videodev.h" /* the V4L definitions */

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

/* 
   This works around a bug bttv appears to have with the mute
   property. Comment out the line if your V4L driver isn't buggy.
*/
#define TVENG1_BTTV_MUTE_BUG_WORKAROUND 1
/*
  If this is enabled, some specific features of the bttv driver are
  enabled, but they are non-standard
*/
#define TVENG1_BTTV_PRESENT 1

/*
  If this is enabled, pal_n mode is enabled ( it can crash my system )
*/
/* #define TVENG1_PAL_N 1 */

/* The following audio decoding modes are defined */
char * tveng1_audio_decoding_entries[] =
{
  N_("Mono"),
  N_("Stereo"),
  N_("European alternate 1"),
  N_("European alternate 2"),
  NULL
};

/* Private, builds the controls structure */
int
p_tveng1_build_controls(tveng_device_info * info);

/* Internal function declaration */
int tveng1_open_device_file(int flags, tveng_device_info * info);

/*
  Return fd for the device file opened. Checks if the device is a
  valid video device. -1 on error.
  Flags will be used for open()'ing the file 
*/
int tveng1_open_device_file(int flags, tveng_device_info * info)
{
  struct video_capability caps;

  t_assert(info != NULL);
  t_assert(info -> file_name != NULL);

  info -> fd = open(info -> file_name, flags);
  if (info -> fd < 0)
    {
      info->tveng_errno = errno; /* Just to put something other than 0 */
      t_error("open()", info);
      return -1;
    }

  /* We check the capabilities of this video device */
  if (ioctl(info -> fd, VIDIOCGCAP, &caps))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGCAP", info);
      close(info -> fd);
      return -1;
    }

  /* Check if this device is convenient for capturing */
  if ( !(caps.type & VID_TYPE_CAPTURE) )
    {
      info->tveng_errno = -1;
      snprintf(info->error, 256, 
	       _("%s doesn't look like a valid capture device"), info
	       -> file_name);
      close(info -> fd);
      return -1;
    }

  /* Copy capability info*/
  snprintf(info->caps.name, 32, caps.name);
  info->caps.channels = caps.channels;
  info->caps.audios = caps.audios;
  info->caps.maxwidth = caps.maxwidth;
  info->caps.minwidth = caps.minwidth;
  info->caps.maxheight = caps.maxheight;
  info->caps.minheight = caps.minheight;
  info->caps.flags = 0;

  /* Sets up the capability flags */
  if (caps.type & VID_TYPE_CAPTURE)
    info ->caps.flags |= TVENG_CAPS_CAPTURE;
  if (caps.type & VID_TYPE_TUNER)
    info ->caps.flags |= TVENG_CAPS_TUNER;
  if (caps.type & VID_TYPE_TELETEXT)
    info ->caps.flags |= TVENG_CAPS_TELETEXT;
  if (caps.type & VID_TYPE_OVERLAY)
    info ->caps.flags |= TVENG_CAPS_OVERLAY;
  if (caps.type & VID_TYPE_CHROMAKEY)
    info ->caps.flags |= TVENG_CAPS_CHROMAKEY;
  if (caps.type & VID_TYPE_CLIPPING)
    info ->caps.flags |= TVENG_CAPS_CLIPPING;
  if (caps.type & VID_TYPE_FRAMERAM)
    info ->caps.flags |= TVENG_CAPS_FRAMERAM;
  if (caps.type & VID_TYPE_SCALES)
    info ->caps.flags |= TVENG_CAPS_SCALES;
  if (caps.type & VID_TYPE_MONOCHROME)
    info ->caps.flags |= TVENG_CAPS_MONOCHROME;
  if (caps.type & VID_TYPE_SUBCAPTURE)
    info ->caps.flags |= TVENG_CAPS_SUBCAPTURE;

  /* Set some flags for this device */
  fcntl( info -> fd, F_SETFD, FD_CLOEXEC );

  /* Ignore the alarm signal */
  signal(SIGALRM, SIG_IGN);

  /* Set the controller */
  info -> current_controller = TVENG_CONTROLLER_V4L1;

  /* Everything seems to be OK with this device */
  return (info -> fd);
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
int tveng1_attach_device(const char* device_file,
			enum tveng_attach_mode attach_mode,
			tveng_device_info * info)
{
  int error;
  t_assert(device_file != NULL);
  t_assert(info != NULL);

  if (info -> fd) /* If the device is already attached, detach it */
    tveng_close_device(info);

  info -> file_name = strdup(device_file);
  if (!(info -> file_name))
    {
      perror("strdup");
      info->tveng_errno = errno;
      snprintf(info->error, 256, _("Cannot duplicate device name"));
      return -1;
    }

  switch (attach_mode)
    {
      /* In V4L there is no control-only mode */
    case TVENG_ATTACH_CONTROL:
    case TVENG_ATTACH_READ:
      info -> fd = tveng1_open_device_file(O_RDWR, info);
      break;
    default:
      t_error_msg("switch()", _("Unknown attach mode for the device"),
		  info);
      return -1;
    };

  /*
    Errors (if any) are already aknowledged when we reach this point,
    so we don't show them again
  */
  if (info -> fd < 0)
    return -1;
  
  info -> attach_mode = attach_mode;
  /* Current capture mode is no capture at all */
  info -> current_mode = TVENG_NO_CAPTURE;

  /* We have a valid device, get some info about it */
  /* Fill in inputs */
  info->inputs = NULL;
  info->cur_input = 0;
  error = tveng1_get_inputs(info);
  if (error < 1)
    {
      if (error == 0) /* No inputs */
      {
	info->tveng_errno = -1;
	snprintf(info->error, 256, _("No inputs for this device"));
	fprintf(stderr, "%s\n", info->error);
      }
      tveng1_close_device(info);
      return -1;
    }

  /* Make an ioctl test and switch to the first input */
  if (tveng1_set_input(&(info->inputs[0]), info) == -1)
    {
      tveng1_close_device(info);
      return -1;
    }

  /* Fill in standards */
  info->standards = NULL;
  info->cur_standard = 0;
  error = tveng1_get_standards(info);
  if (error < 1)
    {
      if (error == 0) /* No standards */
      {
	info->tveng_errno = -1;
	snprintf(info->error, 256, _("No standards for this device"));
	fprintf(stderr, "%s\n", info->error);
      }
      tveng1_close_device(info);
      return -1;
    }

  /* make another ioctl test, switch to first standard */
  if (tveng1_set_standard(&(info->standards[0]), info) == -1)
    {
      tveng1_close_device(info);
      return -1;
    }

  /* Query present controls */
  info->num_controls = 0;
  info->controls = NULL;
  error = p_tveng1_build_controls(info);
  if (error == -1)
      return -1;

#ifdef TVENG1_BTTV_MUTE_BUG_WORKAROUND
  /* Mute the device, so we know for sure which is the mute value on
     startup */
  if (tveng1_set_mute(1, info) == -1)
    {
      tveng1_close_device(info);
      return -1;
    }
#endif

  /* Set up the palette according to the one present in the system */
  error = tveng_get_display_depth(info);

  if (error == -1)
    {
      tveng1_close_device(info);
      return -1;
    }

  switch(error)
    {
    case 16:
      info->format.pixformat = TVENG_PIX_RGB565;
      break;
    case 24:
      info->format.pixformat = TVENG_PIX_BGR24;
      break;
    case 32:
      info->format.pixformat = TVENG_PIX_BGR32;
      break;
    default:
      info -> tveng_errno = -1;
      t_error_msg("switch()", 
		  _("Cannot find appropiate palette for current display"),
		  info);
      tveng1_close_device(info);
      return 0;
    }

  /* Set our desired size, make it halfway */
  info -> format.width = (info->caps.minwidth + info->caps.maxwidth)/2;
  info -> format.height = (info->caps.minheight +
			   info->caps.maxheight)/2;
  if (tveng1_set_capture_format(info) == -1)
    {
      tveng1_close_device(info);
      return -1;
    }

  return info -> fd;
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
tveng1_describe_controller(char ** short_str, char ** long_str,
			   tveng_device_info * info)
{
  t_assert(info != NULL);
  if (short_str)
    *short_str = "V4L1";
  if (long_str)
    *long_str = "Video4Linux 1";
}

/* Closes a device opened with tveng_init_device */
void tveng1_close_device(tveng_device_info * info)
{
  switch (info->current_mode)
    {
    case TVENG_CAPTURE_READ:
      tveng1_stop_capturing(info);
      break;
    case TVENG_CAPTURE_PREVIEW:
      tveng1_stop_previewing(info);
      break;
    default:
      break;
    };

  close(info -> fd);
  info -> fd = 0;
  info -> current_controller = TVENG_CONTROLLER_NONE;

  if (info -> file_name)
    free(info -> file_name);
  if (info -> inputs)
    free(info -> inputs);
  if (info -> standards)
    free(info -> standards);
  if (info -> controls)
    free(info -> controls);
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
int tveng1_get_inputs(tveng_device_info * info)
{
  /* In v4l, inputs are called channels */
  struct video_channel channel;
  int i;

  t_assert(info != NULL);

  if (info->inputs)
    free(info->inputs);

  info->inputs = NULL;
  info->num_inputs = 0;
  info->cur_input = 0;

  for (i=0;;i++)
    {
      channel.channel = i;
      if (ioctl(info->fd, VIDIOCGCHAN, &channel))
	break;
      info->inputs = realloc(info->inputs, (i+1)*
			     sizeof(struct tveng_enum_input));
      info->inputs[i].id = i;
      snprintf(info->inputs[i].name, 32, channel.name);
      info->inputs[i].tuners = channel.tuners;
      info->inputs[i].flags = 0;
      if (channel.flags & VIDEO_VC_TUNER)
	info->inputs[i].flags |= TVENG_INPUT_TUNER;
      if (channel.flags & VIDEO_VC_AUDIO)
	info->inputs[i].flags |= TVENG_INPUT_AUDIO;
      /* get the correct input type */
      switch(channel.type)
	{
	case VIDEO_TYPE_TV:
	  info->inputs[i].type = TVENG_INPUT_TYPE_TV;
	  break;
	case VIDEO_TYPE_CAMERA:
	  info->inputs[i].type = TVENG_INPUT_TYPE_CAMERA;
	  break;
	default:
	  break;
	}
    }
  if (i) /* If there is any channel, switch to the first one */
    {
      channel.channel = 0;
      if (ioctl(info->fd, VIDIOCGCHAN, &channel))
	{
	  info -> tveng_errno = errno;
	  t_error("VIDIOCGCHAN", info);
	  return -1;
	}
      if (ioctl(info->fd, VIDIOCSCHAN, &channel))
	{
	  info -> tveng_errno = errno;
	  t_error("VIDIOCSCHAN", info);
	  return -1;
	}
    }

  return (info->num_inputs = i);
}

/*
  Sets the current input for the capture
*/
int tveng1_set_input(struct tveng_enum_input * input,
		     tveng_device_info * info)
{
  enum tveng_capture_mode current_mode;
  struct video_channel channel;

  t_assert(info != NULL);
  t_assert(input != NULL);

  current_mode = info -> current_mode;

  /* Stop any current capture */
  switch (info -> current_mode)
    {
    case TVENG_CAPTURE_READ:
      if (tveng1_stop_capturing(info) == -1)
	return -1;
      break;
    case TVENG_CAPTURE_PREVIEW:
      if (tveng1_stop_previewing(info) == -1)
	return -1;
      break;
    default:
      break;
    }

  /* Fill in the channel with the appropiate info */
  channel.channel = input->id;
  if (ioctl(info->fd, VIDIOCGCHAN, &channel))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGCHAN", info);
      return -1;
    }    

  /* Now set the channel */
  if (ioctl(info->fd, VIDIOCSCHAN, &channel))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCSCHAN", info);
      return -1;
    }

  /* Start capturing again as if nothing had happened */
  switch (current_mode)
    {
    case TVENG_CAPTURE_READ:
      if (tveng1_start_capturing(info) == -1)
	return -1;
      break;
    case TVENG_CAPTURE_PREVIEW:
      if (tveng1_start_previewing(info) == -1)
	return -1;
      break;
    default:
      break;
    }

  info->cur_input = input->id;

  /* Maybe there are some other standards, get'em */
  tveng1_get_standards(info);

  return 0;
}

/*
  Sets the input named name as the active input. -1 on error.
*/
int
tveng1_set_input_by_name(const char * input_name,
			 tveng_device_info * info)
{
  int i;

  t_assert(input_name != NULL);
  t_assert(info != NULL);

  for (i = 0; i < info->num_inputs; i++)
    if (!strcasecmp(info->inputs[i].name, input_name))
      return tveng1_set_input(&(info->inputs[i]), info);

  info->tveng_errno = -1;
  snprintf(info->error, 256,
	   _("Input %s doesn't appear to exist"), input_name);

  return -1; /* String not found */
}

/*
  Sets the active input by its id (may not be the same as its array
  index, but it should be). -1 on error
*/
int
tveng1_set_input_by_id(int id, tveng_device_info * info)
{
  int i;

  t_assert(info != NULL);

  for (i = 0; i < info->num_inputs; i++)
    if (info->inputs[i].id == id)
      return tveng1_set_input(&(info->inputs[i]), info);

  info->tveng_errno = -1;
  snprintf(info->error, 256, 
	   _("Input number %d doesn't appear to exist"), id);

  return -1; /* String not found */
}

/*
  Sets the active input by its index in inputs. -1 on error
*/
int
tveng1_set_input_by_index(int index, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(index > -1);
  t_assert(index < info -> num_inputs);
  return (tveng1_set_input(&(info -> inputs[index]), info));
}

/* For declaring the possible standards */
struct dummy_standard_struct{
  int id;
  char * name;
};

/*
  Queries the device about its standards. Fills in info as appropiate
  and returns the number of standards in the device. This is for the
  first tuner in the current input, should be enough since most (all)
  inputs have 1 or less tuners.
*/
int tveng1_get_standards(tveng_device_info * info)
{
  int count = 0; /* Number of standards */
  struct video_channel channel;
  int i;

  /* The table with the possible standards */
  struct dummy_standard_struct std_t[] =
  {
    {  0, "PAL" },
    {  1, "NTSC" },
    {  2, "SECAM" },
#ifndef TVENG1_BTTV_PRESENT
    {  3, "AUTO" },
#else
    {  3, "PAL-NC" },
    {  4, "PAL-M" },
#ifdef TVENG1_PAL_N
    {  5, "PAL-N" }, /* This one hangs zapping */
#endif
    {  6, "NTSC-JP" },
#endif
    { -1, NULL}
  };

  /* Free any previously allocated mem */
  if (info->standards)
    free(info->standards);

  info->standards = NULL;
  info->cur_standard = 0;
  info->num_standards = 0;
  info->tveng_errno = 0; /* Set errno flag */

  /* If it has no tuners, we are done */
  if (info->inputs[info->cur_input].tuners == 0)
    return 0;

  while (std_t[count].name)
    {
      /* Valid norm, add it to the list */
      info -> standards = realloc(info->standards,
				  sizeof(struct tveng_enumstd)*(count+1));
      info -> standards[count].id = std_t[count].id;
      info -> standards[count].index = count;
      snprintf(info -> standards[count].name, 32, std_t[count].name);
      count++;
    }

  /* Get the current standard */
  /* Fill in the channel with the appropiate info */
  channel.channel = info->inputs[info->cur_input].id;
  if (ioctl(info->fd, VIDIOCGCHAN, &channel))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGCHAN", info);
      return -1;
    }

  for (i = 0; i<count; i++)
    if (info->standards[i].id == channel.norm)
      break; /* Found */

  if (i != count)
    info->cur_standard = i;
    
  return (info->num_standards = count);
}

/*
  Sets the current standard for the capture. standard is the name for
  the desired standard. updates cur_standard
*/
int tveng1_set_standard(struct tveng_enumstd * std, tveng_device_info * info)
{
  enum tveng_capture_mode current_mode;
  /* in v4l the standard is known as the norm of the channel */
  struct video_channel channel;

  t_assert(info != NULL);
  t_assert(std != NULL);

  current_mode = info -> current_mode;

  /* Stop any current capture (this is a big deal) */
  switch (info -> current_mode)
    {
    case TVENG_CAPTURE_READ:
      if (tveng1_stop_capturing(info) == -1)
	return -1;
      break;
    case TVENG_CAPTURE_PREVIEW:
      if (tveng1_stop_previewing(info) == -1)
	return -1;
      break;
    default:
      break;
    }

  /* Fill in the channel with the appropiate info */
  channel.channel = info->inputs[info->cur_input].id;
  if (ioctl(info->fd, VIDIOCGCHAN, &channel))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGCHAN", info);
      return -1;
    }    

  /* Now set the channel and the norm */
  channel.norm = std->id;
  if (ioctl(info->fd, VIDIOCSCHAN, &channel))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCSCHAN", info);
      return -1;
    }

  info->cur_standard = std->index;

  /* Start capturing again as if nothing had happened */
  switch (current_mode)
    {
    case TVENG_CAPTURE_READ:
      if (tveng1_start_capturing(info) == -1)
	return -1;
      break;
    case TVENG_CAPTURE_PREVIEW:
      if (tveng1_start_previewing(info) == -1)
	return -1;
      break;
    default:
      break;
    }

  return 0;
}

/*
  Sets the standard by name. -1 on error
*/
int
tveng1_set_standard_by_name(char * name, tveng_device_info * info)
{
  int i;
  for (i = 0; i < info->num_standards; i++)
    if (!strcmp(name, info->standards[i].name))
      return tveng1_set_standard(&(info->standards[i]), info);

  info->tveng_errno = -1;
  snprintf(info->error, 256, 
	   _("Standard %s doesn't appear to exist"), name);

  return -1; /* String not found */  
}

/*
  Sets the standard by id.
*/
int
tveng1_set_standard_by_id(int id, tveng_device_info * info)
{
  int i;
  for (i = 0; i < info->num_standards; i++)
    if (info->standards[i].id == id)
      return tveng1_set_standard(&(info->standards[i]), info);

  info->tveng_errno = -1;
  snprintf(info->error, 256, 
	   _("Standard %d doesn't appear to exist"), id);

  return -1; /* id not found */
}

/*
  Sets the standard by index. -1 on error
*/
int
tveng1_set_standard_by_index(int index, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(index < info->num_standards);
  t_assert(index > -1);

  return (tveng1_set_standard(&(info->standards[index]), info));
}

/* Updates the current capture format info. -1 if failed */
int
tveng1_update_capture_format(tveng_device_info * info)
{
  struct video_picture pict;
  struct video_window window;

  t_assert(info != NULL);

  memset(&window, 0, sizeof(struct video_window));

  if (ioctl(info->fd, VIDIOCGPICT, &pict))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGPICT", info);
      return -1;
    }
  
  /* Transform the palette value into a tveng value */
  switch(pict.palette)
    {
    case VIDEO_PALETTE_RGB565:
      info->format.depth = 16;
      info->format.pixformat = TVENG_PIX_RGB565;
      break;
    case VIDEO_PALETTE_RGB24:
      info->format.depth = 24;
      info->format.pixformat = TVENG_PIX_BGR24;
      break;
    case VIDEO_PALETTE_RGB32:
      info->format.depth = 32;
      info->format.pixformat = TVENG_PIX_BGR32;
      break;
    default:
      info->tveng_errno = -1; /* unknown */
      t_error_msg("switch()",
		  _("Cannot understand the actual palette"), info);
      return -1;
    }
  /* Ok, now get the video window dimensions */
  if (ioctl(info->fd, VIDIOCGWIN, &window))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGWIN", info);
      return -1;
    }
  /* Fill in the format structure (except for the data field) */
  info->format.bpp = (info->format.depth + 7)>>3;
  info->format.width = window.width;
  info->format.height = window.height;
  info->format.bytesperline = window.width * info->format.bpp;
  info->format.sizeimage = info->format.height* info->format.bytesperline;
  info->window.x = window.x;
  info->window.y = window.y;
  info->window.width = window.width;
  info->window.height = window.height;
  info->window.chromakey = window.chromakey;
  /* These two are write-only */
  info->window.clipcount = 0;
  info->window.clips = NULL;
  return 0;
}

/* -1 if failed. Sets the pixformat and fills in info -> pix_format
   with the correct values  */
int
tveng1_set_capture_format(tveng_device_info * info)
{
  struct video_picture pict;
  struct video_window window;

  memset(&pict, 0, sizeof(struct video_picture));
  memset(&window, 0, sizeof(struct video_window));

  t_assert(info != NULL);

  if (ioctl(info->fd, VIDIOCGPICT, &pict))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGPICT", info);
      return -1;
    }
  
  /* Transform the given palette value into a V4L value */
  switch(info->format.pixformat)
    {
    case TVENG_PIX_RGB565:
      pict.palette = VIDEO_PALETTE_RGB565;
      pict.depth = 16;
      break;
    case TVENG_PIX_RGB24:
    case TVENG_PIX_BGR24: /* No way to distinguish these two in V4L */
      pict.palette = VIDEO_PALETTE_RGB24;
      pict.depth = 24;
      break;
    case TVENG_PIX_RGB32:
    case TVENG_PIX_BGR32:
      pict.palette = VIDEO_PALETTE_RGB32;
      pict.depth = 32;
      break;
    default:
      info->tveng_errno = -1; /* unknown */
      t_error_msg("switch()", _("Cannot understand the given palette"),
		  info);
      return -1;
    }

  /* Set this values for the picture properties */
  if (ioctl(info->fd, VIDIOCSPICT, &pict))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCSPICT", info);
      return -1;
    }

  /* Fill in the new width and height parameters */
  /* Make them 4-byte multiplus to avoid errors */
  if (info->format.height < info->caps.minheight)
    info->format.height = info->caps.minheight;
  if (info->format.height > info->caps.maxheight)
    info->format.height = info->caps.maxheight;
  if (info->format.width < info->caps.minwidth)
    info->format.width = info->caps.minwidth;
  if (info->format.width > info->caps.maxwidth)
    info->format.width = info->caps.maxwidth;
  
  window.width = (info->format.width+3) & ~3;
  window.height = (info->format.height+3) & ~3;
  window.clips = NULL;
  window.clipcount = 0;

  /* Ok, now set the video window dimensions */
  if (ioctl(info->fd, VIDIOCSWIN, &window))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCSWIN", info);
      return -1;
    }
  /* Check fill in info with the current values (may not be the ones
     asked for) */
  if (tveng1_update_capture_format(info) == -1)
    return -1; /* error */

  return 0; /* Success */
}

/* Private definition for the control id's. In v4l the "control"
   concept doesn't exist */
#define P_TVENG1_C_AUDIO_MUTE 0
#define P_TVENG1_C_AUDIO_VOLUME 1
#define P_TVENG1_C_AUDIO_BASS 2
#define P_TVENG1_C_AUDIO_TREBLE 3
#define P_TVENG1_C_AUDIO_BALANCE 4
#define P_TVENG1_C_AUDIO_DECODING 5 /* The audio decoding mode */
#define P_TVENG1_C_AUDIO_MIN P_TVENG1_C_AUDIO_MUTE /* the first audio control */
#define P_TVENG1_C_AUDIO_MAX P_TVENG1_C_AUDIO_DECODING /* the last audio
						 control */
#define P_TVENG1_C_VIDEO_BRIGHTNESS 100 /* leave free room */
#define P_TVENG1_C_VIDEO_HUE 101
#define P_TVENG1_C_VIDEO_COLOUR 102
#define P_TVENG1_C_VIDEO_CONTRAST 103
#define P_TVENG1_C_VIDEO_MIN P_TVENG1_C_VIDEO_BRIGHTNESS /* the first video
						    control */
#define P_TVENG1_C_VIDEO_MAX P_TVENG1_C_VIDEO_CONTRAST /* The last video
						  control */


/* private, add a control to the control structure, -1 means ENOMEM */
int
p_tveng1_append_control(struct tveng_control * new_control, 
		       tveng_device_info * info);

int
p_tveng1_append_control(struct tveng_control * new_control, 
		       tveng_device_info * info)
{
  struct tveng_control * new_pointer = (struct tveng_control*)
    realloc(info->controls, (info->num_controls+1)*
	    sizeof(struct tveng_control));

  if (!new_pointer)
    {
      info->tveng_errno = errno;
      t_error("realloc", info);
      return -1;
    }
  info->controls = new_pointer;

  memcpy(&info->controls[info->num_controls], new_control, sizeof(struct
							   tveng_control));
  info->num_controls++;
  return 0;
}

/* Private, builds the controls structure */
int
p_tveng1_build_controls(tveng_device_info * info)
{
  /* Info about the video device */
  struct video_picture pict;
  struct video_audio audio;
  struct tveng_control control;

  memset(&pict, 0, sizeof(struct video_picture));
  memset(&audio, 0, sizeof(struct video_audio));

  t_assert(info != NULL);

  /* Fill in the two structs */
  if (ioctl(info->fd, VIDIOCGAUDIO, &audio))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGAUDIO", info);
      return -1;
    }
  if (ioctl(info->fd, VIDIOCGPICT, &pict))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGPICT", info);
      return -1;
    }

  /* Build the audio controls if they are available */
  if (audio.flags & VIDEO_AUDIO_MUTABLE)
    {
      control.id = P_TVENG1_C_AUDIO_MUTE;
      snprintf(control.name, 32, _("Mute"));
      control.min = 0;
      control.max = 1;
      control.type = TVENG_CONTROL_CHECKBOX;
      control.data = NULL;
      if (p_tveng1_append_control(&control, info) == -1)
	return -1;
    }
  if (audio.flags & VIDEO_AUDIO_VOLUME)
    {
      control.id = P_TVENG1_C_AUDIO_VOLUME;
      snprintf(control.name, 32, _("Volume"));
      control.min = 0;
      control.max = 65535;
      control.type = TVENG_CONTROL_SLIDER;
      control.data = NULL;
      if (p_tveng1_append_control(&control, info) == -1)
	return -1;
    }
  if (audio.flags & VIDEO_AUDIO_BASS)
    {
      control.id = P_TVENG1_C_AUDIO_BASS;
      snprintf(control.name, 32, _("Bass"));
      control.min = 0;
      control.max = 65535;
      control.type = TVENG_CONTROL_SLIDER;
      control.data = NULL;
      if (p_tveng1_append_control(&control, info) == -1)
	return -1;
    }
  if (audio.flags & VIDEO_AUDIO_TREBLE)
    {
      control.id = P_TVENG1_C_AUDIO_TREBLE;
      snprintf(control.name, 32, _("Treble"));
      control.min = 0;
      control.max = 65535;
      control.type = TVENG_CONTROL_SLIDER;
      control.data = NULL;
      if (p_tveng1_append_control(&control, info) == -1)
	return -1;
    }
  /* Add the balance control */
  control.id = P_TVENG1_C_AUDIO_BALANCE;
  snprintf(control.name, 32, _("Balance"));
  control.min = 0;
  control.max = 65535;
  control.type = TVENG_CONTROL_SLIDER;
  control.data = NULL;
  if (p_tveng1_append_control(&control, info) == -1)
    return -1;

  control.id = P_TVENG1_C_AUDIO_DECODING;
  snprintf(control.name, 32, _("Audio Decoding"));
  control.min = 0;
  control.max = 3;
  control.type = TVENG_CONTROL_MENU;
  control.data = tveng1_audio_decoding_entries;
  if (p_tveng1_append_control(&control, info) == -1)
    return -1;

  /* Build the video controls now */
  control.id = P_TVENG1_C_VIDEO_BRIGHTNESS;
  snprintf(control.name, 32, _("Brightness"));
  control.min = 0;
  control.max = 65535;
  control.type = TVENG_CONTROL_SLIDER;
  control.data = NULL;
  if (p_tveng1_append_control(&control, info) == -1)
    return -1;

  control.id = P_TVENG1_C_VIDEO_HUE;
  snprintf(control.name, 32, _("Hue"));
  control.min = 0;
  control.max = 65535;
  control.type = TVENG_CONTROL_SLIDER;
  control.data = NULL;
  if (p_tveng1_append_control(&control, info) == -1)
    return -1;

  control.id = P_TVENG1_C_VIDEO_COLOUR;
  snprintf(control.name, 32, _("Colour"));
  control.min = 0;
  control.max = 65535;
  control.type = TVENG_CONTROL_SLIDER;
  control.data = NULL;
  if (p_tveng1_append_control(&control, info) == -1)
    return -1;

  control.id = P_TVENG1_C_VIDEO_CONTRAST;
  snprintf(control.name, 32, _("Contrast"));
  control.min = 0;
  control.max = 65535;
  control.type = TVENG_CONTROL_SLIDER;
  control.data = NULL;
  if (p_tveng1_append_control(&control, info) == -1)
    return -1;

  return (tveng1_update_controls(info)); /* Fill in with the valid
					   values */
}

/*
  Gets the current value of the controls, fills in info->controls
  appropiately. After this (and if it succeeds) you can look in
  info->controls to get the values for each control. -1 on error
*/
int
tveng1_update_controls(tveng_device_info * info)
{
  /* Info about the video device */
  struct video_picture pict;
  struct video_audio audio;
  int i;
  struct tveng_control * control;
#ifdef TVENG1_BTTV_MUTE_BUG_WORKAROUND
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;
#endif

  memset(&pict, 0, sizeof(struct video_picture));
  memset(&audio, 0, sizeof(struct video_audio));

  t_assert(info != NULL);
  t_assert(info->num_controls > 0);

  /* Fill in the two structs */
  if (ioctl(info->fd, VIDIOCGAUDIO, &audio))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGAUDIO", info);
      return -1;
    }
  if (ioctl(info->fd, VIDIOCGPICT, &pict))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGPICT", info);
      return -1;
    }

  /* Iterate for all the controls */
  for (i = 0; i < info-> num_controls; i++)
    {
      control = &(info->controls[i]);
      switch (control -> id)
	{
	  /* Audio controls */
	case P_TVENG1_C_AUDIO_MUTE:
	  if ((audio.flags & VIDEO_AUDIO_MUTE) != 0)
	    control -> cur_value = 1;
	  else
	    control -> cur_value = 0;
#ifdef TVENG1_BTTV_MUTE_BUG_WORKAROUND
	  control -> cur_value = p_info -> muted;
#endif
	  break;
	case P_TVENG1_C_AUDIO_VOLUME:
	  control -> cur_value = audio.volume;
	  break;
	case P_TVENG1_C_AUDIO_BASS:
	  control -> cur_value = audio.bass;
	  break;
	case P_TVENG1_C_AUDIO_TREBLE:
	  control -> cur_value = audio.treble;
	  break;
	case P_TVENG1_C_AUDIO_BALANCE:
	  control -> cur_value = audio.balance;
	  break;
	case P_TVENG1_C_AUDIO_DECODING:
	  switch (audio.mode)
	    {
	    case VIDEO_SOUND_MONO:
	      control -> cur_value = 0;
	      break;
	    case VIDEO_SOUND_STEREO:
	      control -> cur_value = 1;
	      break;
	    case VIDEO_SOUND_LANG1:
	      control -> cur_value = 2;
	      break;
	    case VIDEO_SOUND_LANG2:
	      control -> cur_value = 3;
	      break;
	    default:
	      t_error_msg("switch()", _("Unknown decoding mode"),
			  info);
	      break;
	    }
	  break;

	/* Video controls */
	case P_TVENG1_C_VIDEO_BRIGHTNESS:
	  control -> cur_value = pict.brightness;
	  break;
	case P_TVENG1_C_VIDEO_HUE:
	  control -> cur_value = pict.hue;
	  break;
	case P_TVENG1_C_VIDEO_COLOUR:
	  control -> cur_value = pict.colour;
	  break;
	case P_TVENG1_C_VIDEO_CONTRAST:
	  control -> cur_value = pict.contrast;
	  break;
	default:
	  info->tveng_errno = -1;
	  snprintf(info->error, 256,
		   _("Unknown control: %s (%d)"),
		   control->name, control->id);
	  fprintf(stderr, "%s\n", info->error);
	  return -1;
	}
    }
  return 0;
}

/*
  Sets the value for an specific control. The given value will be
  clipped between min and max values. Returns -1 on error
*/
int
tveng1_set_control(struct tveng_control * control, int value,
		   tveng_device_info * info)
{
  struct video_picture pict; /* For setting a picture control */
  struct video_audio audio; /* For setting up audio controls */
#ifdef TVENG1_BTTV_MUTE_BUG_WORKAROUND
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;
#endif
  /* The possible audio decoding modes */
  int decoding_mode[] =
  {
    VIDEO_SOUND_MONO,
    VIDEO_SOUND_STEREO,
    VIDEO_SOUND_LANG1,
    VIDEO_SOUND_LANG2
  };

  memset(&pict, 0, sizeof(struct video_picture));
  memset(&audio, 0, sizeof(struct video_audio));
  
  t_assert(control != NULL);
  t_assert(info != NULL);

  /* check that the control is in a valid range */
  /* we are supposing that P_TVENG1_C_AUDIO_MIN < P_TVENG1_C_VIDEO_MAX
     and that P_TVENG1_C_VIDEO_MIN > P_TVENG1_C_AUDIO_MAX */
  t_assert(control->id >= P_TVENG1_C_AUDIO_MIN);
  t_assert(control->id <= P_TVENG1_C_VIDEO_MAX);
  t_assert((control->id >= P_TVENG1_C_VIDEO_MIN) || (control->id <=
	   P_TVENG1_C_AUDIO_MAX));

  /* Clip value to a valid one */
  if (value < control->min)
    value = control -> min;

  if (value > control->max)
    value = control -> max;

  if (control -> id <= P_TVENG1_C_AUDIO_MAX) /* Set an audio control */
    {
      if (ioctl(info->fd, VIDIOCGAUDIO, &audio))
	{
	  info->tveng_errno = errno;
	  t_error("VIDIOCGAUDIO", info);
	  return -1;
	}
      audio.flags=0;
      switch (control->id)
	{
	case P_TVENG1_C_AUDIO_MUTE:
	  if (value)
	    audio.flags |= VIDEO_AUDIO_MUTE;
	  else
	    audio.flags &= ~VIDEO_AUDIO_MUTE;
#ifdef TVENG1_BTTV_MUTE_BUG_WORKAROUND
	  p_info->muted = value;
#endif
	  break;
	case P_TVENG1_C_AUDIO_VOLUME:
	  audio.volume = value;
	  break;
	case P_TVENG1_C_AUDIO_BASS:
	  audio.bass = value;
	  break;
	case P_TVENG1_C_AUDIO_TREBLE:
	  audio.treble = value;
	  break;
	case P_TVENG1_C_AUDIO_BALANCE:
	  audio.balance = value;
	  break;
	case P_TVENG1_C_AUDIO_DECODING:
	  audio.mode = decoding_mode[value];
	  break;

	default:
	  info->tveng_errno = -1;
	  snprintf(info->error, 256, _("Unknown audio control: %s (%d)"),
			    control->name, control->id);
	  fprintf(stderr, "%s\n", info->error);
	  return -1;
	}
      /* Set the control */
      if (ioctl(info->fd, VIDIOCSAUDIO, &audio))
	{
	  info->tveng_errno = errno;
	  t_error("VIDIOCSAUDIO", info);
	  return -1;
	}
    }
  else /* a video control */
    {
      /* get current settings */
      if (ioctl(info->fd, VIDIOCGPICT, &pict))
	{
	  info->tveng_errno = errno;
	  t_error("VIDIOCGPICT", info);
	  return -1;
	}
      switch(control->id)
	{
	case P_TVENG1_C_VIDEO_BRIGHTNESS:
	  pict.brightness = value;
	  break;
	case P_TVENG1_C_VIDEO_HUE:
	  pict.hue = value;
	  break;
	case P_TVENG1_C_VIDEO_COLOUR:
	  pict.colour = value;
	  break;
	case P_TVENG1_C_VIDEO_CONTRAST:
	  pict.contrast = value;
	  break;
	default:
	  info->tveng_errno = -1;
	  snprintf(info->error, 256, _("Unknown video control: %s (%d)"),
			    control->name, control->id);
	  fprintf(stderr, "%s\n", info->error);
	  return -1;
	}
      /* Set new values */
      if (ioctl(info->fd, VIDIOCSPICT, &pict))
	{
	  info->tveng_errno = errno;
	  t_error("VIDIOCSPICT", info);
	  return -1;
	}
    }
  /* updates the value of all the controls */
  return (tveng1_update_controls(info));
}

/*
  Gets the value of a control, given its name. Returns -1 on
  error. The comparison is performed disregarding the case. The value
  read is stored in cur_value.
*/
int
tveng1_get_control_by_name(const char * control_name,
			   int * cur_value,
			   tveng_device_info * info)
{
  int i;
  int value;

  t_assert(info != NULL);
  t_assert(info -> num_controls > 0);
  t_assert(control_name != NULL);

  /* Update the controls (their values) */
  if (tveng1_update_controls(info) == -1)
    return -1;

  /* iterate through the info struct to find the mute control */
  for (i = 0; i < info->num_controls; i++)
    if (!strcasecmp(info->controls[i].name,control_name))
      /* we found it */
      {
	value = info->controls[i].cur_value;
	t_assert(value <= info->controls[i].max);
	t_assert(value >= info->controls[i].min);
	if (cur_value)
	  *cur_value = value;
	return 0; /* Success */
      }

  /* if we reach this, we haven't found the control */
  info->tveng_errno = -1;
  snprintf(info->error, 256, 
	   _("Cannot find control \"%s\" in the list of controls"),
	   control_name);
  fprintf(stderr, "%s\n", info->error);
  return -1;
}

/*
  Sets the value of a control, given its name. Returns -1 on
  error. The comparison is performed disregarding the case.
  new_value holds the new value given to the control, and it is
  clipped as neccessary.
*/
int
tveng1_set_control_by_name(const char * control_name,
			   int new_value,
			   tveng_device_info * info)
{
  int i;

  t_assert(info != NULL);
  t_assert(info -> num_controls > 0);

  /* iterate through the info struct to find the mute control */
  for (i = 0; i < info->num_controls; i++)
    if (!strcasecmp(info->controls[i].name,control_name))
      /* we found it */
      return (tveng1_set_control(&(info->controls[i]), new_value, info));

  /* if we reach this, we haven't found the control */
  info->tveng_errno = -1;
  snprintf(info->error, 256, 
	   _("Cannot find control \"%s\" in the list of controls"),
	   control_name);
  fprintf(stderr, "%s\n", info->error);
  return -1;
}

/*
  Gets the value of a control, given its control id. -1 on error (or
  cid not found). The result is stored in cur_value.
*/
int
tveng1_get_control_by_id(int cid, int * cur_value,
			 tveng_device_info * info)
{
  int i;
  int value;

  t_assert(info != NULL);
  t_assert(info -> num_controls > 0);

  /* Update the controls (their values) */
  if (tveng1_update_controls(info) == -1)
    return -1;

  /* iterate through the info struct to find the mute control */
  for (i = 0; i < info->num_controls; i++)
    if (info->controls[i].id == cid)
      /* we found it */
      {
	value = info->controls[i].cur_value;
	t_assert(value <= info->controls[i].max);
	t_assert(value >= info->controls[i].min);
	if (cur_value)
	  *cur_value = value;
	return 0; /* Success */
      }

  /* if we reach this, we haven't found the control */
  info->tveng_errno = -1;
  snprintf(info->error, 256, 
	   _("Cannot find control %d in the list of controls"),
	   cid);
  fprintf(stderr, "%s\n", info->error);
  return -1;
}

/*
  Sets a control by its id. Returns -1 on error
*/
int tveng1_set_control_by_id(int cid, int new_value,
			    tveng_device_info * info)
{
  int i;

  t_assert(info != NULL);
  t_assert(info -> num_controls > 0);

  /* iterate through the info struct to find the mute control */
  for (i = 0; i < info->num_controls; i++)
    if (info->controls[i].id == cid)
      /* we found it */
      return (tveng1_set_control(&(info->controls[i]), new_value, info));

  /* if we reach this, we haven't found the control */
  info->tveng_errno = -1;
  snprintf(info->error, 256, 
	   _("Cannot find control %d in the list of controls"),
	   cid);
  fprintf(stderr, "%s\n", info->error);
  return -1;
}

/*
  Gets the value of the mute property. 1 means mute (no sound) and 0
  unmute (sound). -1 on error
*/
int
tveng1_get_mute(tveng_device_info * info)
{
  int returned_value;
  if (tveng1_get_control_by_name(_("Mute"), &returned_value, info) ==
      -1)
    return -1;
  return returned_value;
}

/*
  Sets the value of the mute property. 0 means unmute (sound) and 1
  mute (no sound). -1 on error
*/
int
tveng1_set_mute(int value, tveng_device_info * info)
{
  return (tveng1_set_control_by_name(_("Mute"), value, info));
}

/*
  Tunes the current input to the given freq. Returns -1 on error.
*/
int
tveng1_tune_input(__u32 freq, tveng_device_info * info)
{
  __u32 new_freq;
  struct video_tuner tuner;
  int muted; /* To 'fix' the current behaviour of bttv, i don't like
		it too much (it mutes the input if the signal strength
		is too low) */

  memset(&tuner, 0, sizeof(struct video_tuner));

  t_assert(info != NULL);
  t_assert(info->cur_input < info->num_inputs);
  t_assert(info->cur_input >= 0);

  /* Check that there are tuners in the current input */
  if (info->inputs[info->cur_input].tuners == 0)
    return 0; /* Success (we shouldn't be tuning, anyway) */

  /* Get info about the current tuner (usually the 0 tuner) */
  tuner.tuner = 0;
  if (ioctl(info->fd, VIDIOCGTUNER, &tuner))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGTUNER", info);
      return -1;
    }
  if (tuner.flags & VIDEO_TUNER_LOW)
    new_freq = (freq * 16);
  else
    new_freq = (freq * 0.016);

  /* Clip to the valid freq range */
  if (new_freq < tuner.rangelow)
    new_freq = tuner.rangelow;
  if (new_freq > tuner.rangehigh)
    new_freq = tuner.rangehigh;

  muted = tveng1_get_mute(info);

  /* Ok, tune the current input */
  if (ioctl(info->fd, VIDIOCSFREQ, &new_freq))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCSFREQ", info);
      return -1;
    }

  /* Restore the mute status. This makes bttv behave like i want */
  if (!muted)
    tveng1_set_mute(FALSE, info);

  return 0; /* Success */
}

/*
  Gets the signal strength and the afc code. The afc code indicates
  how to get a better signal, if negative, tune higher, if negative,
  tune lower. 0 means no idea of feature not present in the current
  controller (i.e. V4L1). Strength and/or afc can be NULL pointers,
  that would mean ignore that parameter.
*/
int
tveng1_get_signal_strength (int *strength, int * afc,
			   tveng_device_info * info)
{
  struct video_tuner tuner;

  memset(&tuner, 0, sizeof(struct video_tuner));

  t_assert(info != NULL);
  t_assert(info->cur_input < info->num_inputs);
  t_assert(info->cur_input >= 0);

  /* Check that there are tuners in the current input */
  if (info->inputs[info->cur_input].tuners == 0)
    return -1;

  /* Get info about the current tuner (usually the 0 tuner) */
  tuner.tuner = 0;
  if (ioctl(info->fd, VIDIOCGTUNER, &tuner))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGTUNER", info);
      return -1;
    }

  if (strength)
    *strength = tuner.signal;

  if (afc)
    *afc = 0; /* No such thing in the V4L1 spec */

  return 0; /* Success */
}

/*
  Stores in freq the currently tuned freq. Returns -1 on error.
*/
int
tveng1_get_tune(__u32 * freq, tveng_device_info * info)
{
  __u32 real_freq;
  struct video_tuner tuner;

  t_assert(info != NULL);
  t_assert(freq != NULL);
  t_assert(info->cur_input < info->num_inputs);
  t_assert(info->cur_input >= 0);

  /* Check that there are tuners in the current input */
  if (info->inputs[info->cur_input].tuners == 0)
    {
      *freq = 0;
      info->tveng_errno = -1;
      t_error_msg("tuners check",
		  _("There are no tuners for the active input"),
		  info);
      return -1;
    }

  /* get the current tune value */
  if (ioctl(info->fd, VIDIOCGFREQ, &real_freq))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGFREQ", info);
      return -1;
    }

  /* Now we must convert this to a valid kHz value */
  /* Get info about the current tuner (usually the 0 tuner) */
  tuner.tuner = 0;
  if (ioctl(info->fd, VIDIOCGTUNER, &tuner))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGTUNER", info);
      return -1;
    }

  if (tuner.flags & VIDEO_TUNER_LOW)
    *freq = (real_freq / 16);
  else
    *freq = (real_freq / 0.016);

  return 0;
}

/*
  Gets the minimum and maximum freq that the current input can
  tune. If there is no tuner in this input, -1 will be returned.
  If any of the pointers is NULL, its value will not be filled.
*/
int
tveng1_get_tuner_bounds(__u32 * min, __u32 * max, tveng_device_info *
			info)
{
  struct video_tuner tuner;

  memset(&tuner, 0, sizeof(struct video_tuner));

  t_assert(info != NULL);
  t_assert(info->cur_input < info->num_inputs);
  t_assert(info->cur_input >= 0);

  /* Check that there are tuners in the current input */
  if (info->inputs[info->cur_input].tuners == 0)
    return -1;

  /* Get info about the current tuner (usually the 0 tuner) */
  tuner.tuner = 0;
  if (ioctl(info->fd, VIDIOCGTUNER, &tuner))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGTUNER", info);
      return -1;
    }

  if (min)
    *min = tuner.rangelow;
  if (max)
    *max = tuner.rangehigh;

  if (tuner.flags & VIDEO_TUNER_LOW)
    {
      if (min)
	*min /= 16;
      if (max)
	*max /= 16;
    }
  else
    {
      if (min)
	*min /= 0.016;
      if (max)
	*max /= 0.016;
    }

  return 0; /* Success */
}

/* Two internal functions, both return -1 on error */
int p_tveng1_queue(tveng_device_info * info);
int p_tveng1_dequeue(void * where, tveng_device_info * info);

/*
  Sets up the capture device so any read() call after this one
  succeeds. Returns -1 on error.
*/
int
tveng1_start_capturing(tveng_device_info * info)
{
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;
 
  /* Check which is the current mode and switch it off if neccesary */
  switch (info -> current_mode)
    {
    case TVENG_CAPTURE_READ:
      /* Doing this is a bit dumb, but maybe we want to restart */
      tveng1_stop_capturing(info);
      break;
    case TVENG_CAPTURE_PREVIEW:
      tveng1_stop_previewing(info);
      break;
    default:
      break;
    }
  t_assert(info -> current_mode == TVENG_NO_CAPTURE);

  /* Make the pointer a invalid pointer */
  p_info -> mmaped_data = (char*) -1;

  /* 
     When this function is called, the desired capture format should
     have been set.
  */
  if (ioctl(info->fd, VIDIOCGMBUF, &(p_info->mmbuf)))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGMBUF", info);
      return -1;
    }

  t_assert (p_info->mmbuf.frames > 0);

  p_info -> mmaped_data = (char*)mmap(0, p_info->mmbuf.size,
				      PROT_READ, MAP_SHARED,
				      info->fd, 0);

  if (p_info->mmaped_data == ((char*)-1))
    {
      info->tveng_errno = errno;
      t_error("mmap()", info);
      return -1;
    }

  p_info -> queued = p_info -> dequeued = 0;

  info->current_mode = TVENG_CAPTURE_READ;

  /* Queue first buffer */
  if (p_tveng1_queue(info) == -1)
    return -1;

  return 0;
}

/* Tries to stop capturing. -1 on error. */
int
tveng1_stop_capturing(tveng_device_info * info)
{
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;

  if (info -> current_mode == TVENG_NO_CAPTURE)
    {
      fprintf(stderr, 
	      _("Warning: trying to stop capture with no capture active\n"));
      return 0; /* Nothing to be done */
    }
  t_assert(info->current_mode == TVENG_CAPTURE_READ);

  /* Dequeue last buffer */
  if (p_tveng1_dequeue(NULL, info) == -1)
    return -1;

  if (p_info -> mmaped_data != ((char*)-1))
    if (munmap(p_info->mmaped_data, p_info->mmbuf.size) == -1)
      {
	info -> tveng_errno = errno;
	t_error("munmap()", info);
	return -1;
      }

  info->current_mode = TVENG_NO_CAPTURE;

  return 0;
}

int p_tveng1_queue(tveng_device_info * info)
{
  struct video_mmap bm;
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;

  t_assert(info != NULL);
  t_assert(info -> current_mode == TVENG_CAPTURE_READ);

  /* Fill in the mmaped_buffer struct */
  switch(info->format.pixformat)
    {
    case TVENG_PIX_RGB565:
      bm.format = VIDEO_PALETTE_RGB565;
      break;
    case TVENG_PIX_RGB24:
    case TVENG_PIX_BGR24:
      bm.format = VIDEO_PALETTE_RGB24;
      break;
    case TVENG_PIX_BGR32:
    case TVENG_PIX_RGB32:
      bm.format = VIDEO_PALETTE_RGB32;
      break;
    default:
      info -> tveng_errno = -1;
      t_error_msg("switch()", _("Cannot understand actual palette"),
		  info);
      return -1;
    }
  bm.frame = (p_info -> queued) % p_info->mmbuf.frames;
  bm.width = info -> format.width;
  bm.height = info -> format.height;

  if (ioctl(info -> fd, VIDIOCMCAPTURE, &bm) == -1)
    {
      /* This comes from xawtv, it isn't in the V4L API */
      if (errno == EAGAIN)
	t_error_msg("VIDIOCMCAPTURE", 
		    _("Grabber chip can't sync (no station tuned in?)"),
		    info);
      else
	{
	  info -> tveng_errno = errno;
	  t_error("VIDIOCMCAPTURE", info);
	}
      return -1;
    }

  /* increase the queued index */
  p_info -> queued = p_info -> queued ++;

  return 0; /* Success */
}

int p_tveng1_dequeue(void * where, tveng_device_info * info)
{
  struct video_mmap bm;
  struct private_tveng1_device_info * p_info =
    (struct private_tveng1_device_info*) info;

  t_assert(info != NULL);
  t_assert(info -> current_mode == TVENG_CAPTURE_READ);

  if (p_info -> dequeued == p_info -> queued)
    return 0; /* All queued frames have been dequeued */

  /* Fill in the mmaped_buffer struct */
  switch(info->format.pixformat)
    {
    case TVENG_PIX_RGB565:
      bm.format = VIDEO_PALETTE_RGB565;
      break;
    case TVENG_PIX_RGB24:
    case TVENG_PIX_BGR24:
      bm.format = VIDEO_PALETTE_RGB24;
      break;
    case TVENG_PIX_BGR32:
    case TVENG_PIX_RGB32:
      bm.format = VIDEO_PALETTE_RGB32;
      break;
    default:
      info -> tveng_errno = -1;
      t_error_msg("switch()", _("Cannot understand actual palette"),
		  info);
      return -1;
    }
  bm.frame = (p_info -> dequeued) % (p_info->mmbuf.frames);
  bm.width = info -> format.width;
  bm.height = info -> format.height;

  if (ioctl(info -> fd, VIDIOCSYNC, &(bm.frame)) == -1)
    {
      info -> tveng_errno = errno;
      t_error("VIDIOCSYNC", info);
      return -1;
    }

  /* Copy the mmaped data to the data struct, if it is not null */
  if (where)
    memcpy(where, p_info -> mmaped_data + p_info->
	   mmbuf.offsets[bm.frame],
	   info->format.sizeimage);

  /* increase the dequeued index */
  p_info -> dequeued ++;

  return 0;
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
int tveng1_read_frame(void * where, unsigned int size, 
		      unsigned int time, tveng_device_info * info)
{
  struct itimerval iv;

  t_assert(info != NULL);

  if (info -> current_mode != TVENG_CAPTURE_READ)
    {
      info -> tveng_errno = -1;
      t_error_msg("check", _("Current capture mode is not READ"),
		  info);
      return -1;
    }

  if (info -> format.sizeimage > size)
    {
      info -> tveng_errno = ENOMEM;
      t_error_msg("check()", 
	      _("Size check failed, quitting to avoid segfault"), info);
      return -1;
    }

  /* Queue a new frame (for the next time) */
  /* This should be inmediate */
  if (p_tveng1_queue(info) == -1)
    return -1;

  /* Dequeue previously queued frame */
  /* Sets the timer to expire (SIGALARM) in the given time */
  iv.it_interval.tv_sec = iv.it_interval.tv_usec = iv.it_value.tv_sec
    = 0;
  iv.it_value.tv_usec = time;
  if (setitimer(ITIMER_REAL, &iv, NULL) == -1)
    {
      info->tveng_errno = errno;
      t_error("setitimer()", info);
      return -1;
    }

  if (p_tveng1_dequeue(where, info) == -1)
    return -1;
  
  /* Everything has been OK, return 0 (success) */
  return 0;
}

/* 
   Sets the capture buffer to an specific size. returns -1 on
   error. Remember to check the value of width and height since it can
   be different to the one requested. 
*/
int tveng1_set_capture_size(int width, int height, tveng_device_info * info)
{
  enum tveng_capture_mode current_mode;
  t_assert(info != NULL);
  t_assert(width > 0);
  t_assert(height > 0);

  current_mode = info -> current_mode;

  /* Stop any current capture (this is a big deal) */
  switch (info -> current_mode)
    {
    case TVENG_CAPTURE_READ:
      if (tveng1_stop_capturing(info) == -1)
	return -1;
      break;
    case TVENG_CAPTURE_PREVIEW:
      if (tveng1_stop_previewing(info) == -1)
	return -1;
      break;
    default:
      break;
    }

  info -> format.width = width;
  info -> format.height = height;
  if (tveng1_set_capture_format(info) == -1)
    return -1;

  /* Restart capture again */
  switch (current_mode)
    {
    case TVENG_CAPTURE_READ:
      if (tveng1_start_capturing(info) == -1)
	return -1;
      break;
    case TVENG_CAPTURE_PREVIEW:
      if (tveng1_start_previewing(info) == -1)
	return -1;
      break;
    default:
      break;
    }

  return 0;
}

/* 
   Gets the actual size of the capture buffer in width and height.
   -1 on error
*/
int tveng1_get_capture_size(int *width, int *height, tveng_device_info * info)
{
  t_assert(info != NULL);

  if (tveng1_update_capture_format(info))
    return -1;

  if (width)
    *width = info->format.width;
  if (height)
    *height = info->format.height;

  return 0; /* Success */
}

/* XF86 Frame Buffer routines */
/*
  Returns 1 if the device attached to info suports previewing, 0 otherwise
*/
int
tveng1_detect_preview (tveng_device_info * info)
{
  struct video_buffer buffer;

  t_assert(info != NULL);

  if ((info -> caps.flags & TVENG_CAPS_OVERLAY) == 0)
    {
      info -> tveng_errno = -1;
      t_error_msg("flags check",
       _("The capability field says that there is no overlay"), info);
      return 0;
    }

  /* Get the current framebuffer info */
  if (ioctl(info -> fd, VIDIOCGFBUF, &buffer))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGFBUF", info);
      return 0;
    }

  info->fb_info.base = buffer.base;
  info->fb_info.height = buffer.height;
  info->fb_info.width = buffer.width;
  info->fb_info.depth = buffer.depth;
  info->fb_info.bytesperline = buffer.bytesperline;

  return 1;
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
tveng1_set_preview_window(tveng_device_info * info)
{
  struct video_window v4l_window;
  struct video_clip * clips=NULL;
  int i;

  t_assert(info != NULL);
  t_assert(info-> window.clipcount >= 0);

  memset(&v4l_window, 0, sizeof(struct video_window));

  /* We do not set the chromakey value */
  v4l_window.x = info->window.x;
  v4l_window.y = info->window.y;
  v4l_window.width = (info->window.width+3) & ~3;
  v4l_window.height = (info->window.height+3) & ~3;
  v4l_window.clipcount = info->window.clipcount;
  v4l_window.clips = NULL;
  if (v4l_window.clipcount)
    {
      clips = (struct video_clip*)malloc(v4l_window.clipcount* 
					 sizeof(struct video_clip));
      memset(clips, 0, v4l_window.clipcount*
	     sizeof(struct video_clip));
      if (!clips)
	{
	  info->tveng_errno = errno;
	  t_error("malloc", info);
	  return -1;
	}
      v4l_window.clips = clips;
      for (i=0;i<v4l_window.clipcount;i++)
	{
	  v4l_window.clips[i].x = info->window.clips[i].x;
	  v4l_window.clips[i].y = info->window.clips[i].y;
	  v4l_window.clips[i].width = info->window.clips[i].width;
	  v4l_window.clips[i].height = info->window.clips[i].height;
	}
    }

  /* Set the new window */
  if (ioctl(info->fd, VIDIOCSWIN, &v4l_window))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCSWIN", info);
      if (clips)
	free(clips);
      return -1;
    }

  /* free allocated mem */
  if (clips)
    free(clips);

  /* Update the info struct */
  return (tveng1_get_preview_window(info));
}

/*
  Gets the current overlay window parameters.
  Returns -1 on error, and any other value on success.
  info   : The device to use
*/
int
tveng1_get_preview_window(tveng_device_info * info)
{
  /* Updates the entire capture format, since in V4L there is no
     difference */
  return (tveng1_update_capture_format(info));
}

/* 
   Sets the previewing on/off.
   on : if 1, set preview on, if 0 off, other values are silently ignored
   info  : device to use for previewing
   Returns -1 on error, anything else on success
*/
int
tveng1_set_preview (int on, tveng_device_info * info)
{
  int one = 1, zero = 0;
  t_assert(info != NULL);

  if ((on < 0) || (on > 1))
    return 0;

  if (ioctl(info->fd, VIDIOCCAPTURE, on ? &one : &zero))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCCAPTURE", info);
      return -1;
    }
  return 0;
}

/* 
   Sets up everything and starts previewing.
   Just call this function to start previewing, it takes care of
   (mostly) everything.
   Returns -1 on error.
*/
int
tveng1_start_previewing (tveng_device_info * info)
{
  Display * display = info->display;
  int width, height;
  int dwidth, dheight; /* Width and height of the display */

  /* Check which is the current mode and switch it off if neccesary */
  switch (info -> current_mode)
    {
    case TVENG_CAPTURE_READ:
      /* Doing this is a bit dumb, but maybe we want to restart */
      tveng1_stop_capturing(info);
      break;
    case TVENG_CAPTURE_PREVIEW:
      tveng1_stop_previewing(info);
      break;
    default:
      break;
    }
  t_assert(info -> current_mode == TVENG_NO_CAPTURE);

  if (!tveng1_detect_preview(info))
    /* We shouldn't be reaching this if the app is well programmed */
    t_assert_not_reached();

  if (!tveng_detect_XF86DGA(info))
    return -1;

  /* Enable Direct Graphics (just the DirectVideo thing) */
  if (!XF86DGADirectVideo(display, 0, XF86DGADirectGraphics))
    {
      t_error("XF86DGADirectVideo", info);
      return -1;
    }

  /* calculate coordinates for the preview window. We compute this for
   the first display */
  dwidth = DisplayWidth(display, 0);
  dheight = DisplayHeight(display, 0); 
  width = info->caps.maxwidth;

  if (width > dwidth)
    width = dwidth;

  height = info->caps.maxheight;
  if (height > dheight)
    height = dheight;

  /* Center the window, dwidth is always >= width */
  info->window.x = (dwidth - width)/2;
  info->window.y = (dheight - height)/2;
  info->window.width = width;
  info->window.height = height;
  info->window.clips = NULL;
  info->window.clipcount = 0;

  /* Set new capture dimensions */
  if (tveng1_set_preview_window(info) == -1)
    {
      /* Switch DirectVideo off */
      if (!XF86DGADirectVideo(display, 0, 0))
	t_error("XF86DGADirectVideo", info);
      return -1;
    }

  /* Center preview window (maybe the requested width and/or height)
     aren't valid */
  info->window.x = (dwidth - info->window.width)/2;
  info->window.y = (dheight - info->window.height)/2;
  info->window.clipcount = 0;
  info->window.clips = NULL;
  if (tveng1_set_preview_window(info) == -1)
    {
      /* Switch DirectVideo off */
      if (!XF86DGADirectVideo(display, 0, 0))
	t_error("XF86DGADirectVideo", info);
      return -1;
    }

  /* Start preview */
  if (tveng1_set_preview(1, info) == -1)
    {
      /* Switch DirectVideo off */
      if (!XF86DGADirectVideo(display, 0, 0))
	t_error("XF86DGADirectVideo", info);
      return -1;
    }

  info -> current_mode = TVENG_CAPTURE_PREVIEW;
  return 0; /* Success */
}

/*
  Stops the fullscreen mode. Returns -1 on error
*/
int
tveng1_stop_previewing(tveng_device_info * info)
{
  Display * display = info->display;

  if (info -> current_mode == TVENG_NO_CAPTURE)
    {
      fprintf(stderr, 
	      _("Warning: trying to stop preview with no capture active\n"));
      return 0; /* Nothing to be done */
    }
  t_assert(info->current_mode == TVENG_CAPTURE_PREVIEW);

  /* No error checking */
  tveng1_set_preview(0, info);

  if (!XF86DGADirectVideo(display, 0, 0))
    {
      t_error("XF86DGADirectVideo", info);
      return -1;
    }

  info -> current_mode = TVENG_NO_CAPTURE;
  return 0; /* Success */
}

