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
#include "tveng2.h"
#include "videodev2.h" /* the V4L2 definitions */

struct tveng2_vbuf
{
  void * vmem; /* Captured image in this buffer */
  struct v4l2_buffer vidbuf; /* Info about the buffer */
};

struct private_tveng2_device_info
{
  tveng_device_info info; /* Info field, inherited */
  int num_buffers; /* Number of mmaped buffers */
  struct tveng2_vbuf * buffers; /* Array of buffers */
  __s64 last_timestamp; /* The timestamp of the last captured buffer */
};

/* Private, builds the controls structure */
static int
p_tveng2_build_controls(tveng_device_info * info);

static int p_tveng2_open_device_file(int flags, tveng_device_info * info);
/*
  Return fd for the device file opened. Checks if the device is a
  valid video device. -1 on error.
  Flags will be used for open()'ing the file 
*/
static int p_tveng2_open_device_file(int flags, tveng_device_info * info)
{
  struct v4l2_capability caps;
  struct v4l2_framebuffer fb;

  t_assert(info != NULL);
  t_assert(info->file_name != NULL);

  info -> fd = open(info -> file_name, flags);
  if (info -> fd < 0)
    {
      info->tveng_errno = errno;
      t_error("open()", info);
      return -1;
    }

  /* We check the capabilities of this video device */
  memset(&caps, 0, sizeof(struct v4l2_capability));
  memset(&fb, 0, sizeof(struct v4l2_framebuffer));

  if (ioctl(info -> fd, VIDIOC_QUERYCAP, &caps))
    {
      info -> tveng_errno = errno;
      t_error("VIDIOC_QUERYCAP", info);
      close(info -> fd);
      return -1;
    }

  /* Check if this device is convenient for us */
  if (caps.type != V4L2_TYPE_CAPTURE)
    {
      info->tveng_errno = -1;
      snprintf(info->error, 256, 
	       _("%s doesn't look like a valid capture device"), info
	       -> file_name);
      close(info -> fd);
      return -1;
    }

  /* Check if we can select() and mmap() this device */
  if (!(caps.flags & V4L2_FLAG_STREAMING))
    {
      info -> tveng_errno = -1;
      snprintf(info->error, 256,
	       _("Sorry, but \"%s\" cannot do streaming"),
	       info -> file_name);
      close(info -> fd);
      return -1;
    }

  if (!(caps.flags & V4L2_FLAG_SELECT))
    {
      info->tveng_errno = -1;
      snprintf(info->error, 256, 
	       _("Sorry, but \"%s\" cannot do select() on file descriptors"),
	       info -> file_name);
      close(info -> fd);
      return -1;
    }

  /* Copy capability info */
  snprintf(info->caps.name, 32, caps.name);
  info->caps.channels = caps.inputs;
  info->caps.audios = caps.audios;
  info->caps.maxwidth = caps.maxwidth;
  info->caps.minwidth = caps.minwidth;
  info->caps.maxheight = caps.maxheight;
  info->caps.minheight = caps.minheight;
  info->caps.flags = 0;

  info->caps.flags |= TVENG_CAPS_CAPTURE; /* This has been tested before */

  if (caps.flags & V4L2_FLAG_TUNER)
    info->caps.flags |= TVENG_CAPS_TUNER;
  if (caps.flags & V4L2_FLAG_DATA_SERVICE)
    info->caps.flags |= TVENG_CAPS_TELETEXT;
  if (caps.flags & V4L2_FLAG_MONOCHROME)
    info->caps.flags |= TVENG_CAPS_MONOCHROME;

  if (caps.flags & V4L2_FLAG_PREVIEW)
    {
      info->caps.flags |= TVENG_CAPS_OVERLAY;
      /* Collect more info about the overlay mode */
      if (ioctl(info->fd, VIDIOC_G_FBUF, &fb))
	{
	  if (fb.flags & V4L2_FBUF_CAP_CHROMAKEY)
	    info->caps.flags |= TVENG_CAPS_CHROMAKEY;
	  if (fb.flags & V4L2_FBUF_CAP_CLIPPING)
	    info->caps.flags |= TVENG_CAPS_CLIPPING;
	  if (!(fb.flags & V4L2_FBUF_CAP_EXTERNOVERLAY))
	    info->caps.flags |= TVENG_CAPS_FRAMERAM;
	  if ((fb.flags & V4L2_FBUF_CAP_SCALEUP) ||
	      (fb.flags & V4L2_FBUF_CAP_SCALEDOWN))
	    info->caps.flags |= TVENG_CAPS_SCALES;
	}
    }

  info -> current_controller = TVENG_CONTROLLER_V4L2;
  
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
int tveng2_attach_device(const char* device_file,
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
      info -> tveng_errno = errno;
      t_error("strdup()", info);
      return -1;
    }

  switch (attach_mode)
    {
    case TVENG_ATTACH_CONTROL:
      info -> fd = p_tveng2_open_device_file(O_NOIO, info);
    case TVENG_ATTACH_READ:
      info -> fd = p_tveng2_open_device_file(O_RDWR, info);
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
  error = tveng2_get_inputs(info);
  if (error < 1)
    {
      if (error == 0) /* No inputs */
      {
	info->tveng_errno = -1;
	snprintf(info->error, 256, _("No inputs for this device"));
	fprintf(stderr, "%s\n", info->error);
      }
      tveng2_close_device(info);
      return -1;
    }

#ifndef TVENG_DISABLE_IOCTL_TESTS
  /* Make an ioctl test and switch to the first input */
  if (tveng2_set_input(&(info->inputs[0]), info) == -1)
    {
      tveng2_close_device(info);
      return -1;
    }
#endif

  /* Fill in standards */
  info->standards = NULL;
  info->cur_standard = 0;
  error = tveng2_get_standards(info);
  if (error < 1)
    {
      if (error == 0) /* No standards */
      {
	info->tveng_errno = -1;
	snprintf(info->error, 256, _("No standards for this device"));
	fprintf(stderr, "%s\n", info->error);
      }
      tveng2_close_device(info);
      return -1;
    }

#ifndef TVENG_DISABLE_IOCTL_TESTS
  /* make another ioctl test, switch to first standard */
  if (tveng2_set_standard(&(info->standards[0]), info) == -1)
    {
      tveng2_close_device(info);
      return -1;
    }
#endif

  /* Query present controls */
  info->num_controls = 0;
  info->controls = NULL;
  error = p_tveng2_build_controls(info);
  if (error == -1)
      return -1;

  /* Set up the palette according to the one present in the system */
  error = tveng_get_display_depth(info);

  if (error == -1)
    {
      tveng2_close_device(info);
      return -1;
    }

  switch(error)
    {
    case 15:
      info->format.pixformat = TVENG_PIX_RGB555;
      break;
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
      tveng2_close_device(info);
      return -1;
    }

  /* Get fb_info */
  tveng2_detect_preview(info);

  /* Set our desired size, make it halfway */
  info -> format.width = (info->caps.minwidth + info->caps.maxwidth)/2;
  info -> format.height = (info->caps.minheight +
			   info->caps.maxheight)/2;
  if (tveng2_set_capture_format(info) == -1)
    {
      tveng2_close_device(info);
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
tveng2_describe_controller(char ** short_str, char ** long_str,
			   tveng_device_info * info)
{
  t_assert(info != NULL);
  if (short_str)
    *short_str = "V4L2";
  if (long_str)
    *long_str = "Video4Linux 2";
}

/* Closes a device opened with tveng_init_device */
void tveng2_close_device(tveng_device_info * info)
{
  int i;
  int j;

  tveng_stop_everything(info);

  close(info -> fd);
  info -> fd = 0;
  info -> current_controller = TVENG_CONTROLLER_NONE;

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
    {
      free(info -> controls);
      info->controls = NULL;
    }
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
int tveng2_get_inputs(tveng_device_info * info)
{
  struct v4l2_input input;
  int i;

  t_assert(info != NULL);

  if (info->inputs)
    free(info->inputs);

  info->inputs = NULL;
  info->num_inputs = 0;
  info->cur_input = 0;

  for (i=0;;i++)
    {
      memset(&input, 0, sizeof(struct v4l2_input));
      input.index = i;
      if (ioctl(info->fd, VIDIOC_ENUMINPUT, &input))
	break;
      info->inputs = realloc(info->inputs, (i+1)*
			     sizeof(struct tveng_enum_input));
      info->inputs[i].id = i;
      snprintf(info->inputs[i].name, 32, input.name);
      info->inputs[i].flags = 0;
      if (input.type & V4L2_INPUT_TYPE_TUNER)
	{
	  info->inputs[i].flags |= TVENG_INPUT_TUNER;
	  info->inputs[i].tuners = 1;
	  info->inputs[i].type = TVENG_INPUT_TYPE_TV;
	}
      else
	{
	  info->inputs[i].tuners = 0;
	  info->inputs[i].type = TVENG_INPUT_TYPE_CAMERA;
	}
      if (input.capability & V4L2_INPUT_CAP_AUDIO)
	info->inputs[i].flags |= TVENG_INPUT_AUDIO;
    }

  if (i) /* If there is any input present, switch to the first one */
    {
      input.index = 0;
      if (ioctl(info -> fd, VIDIOC_S_INPUT, &input))
	{
	  info -> tveng_errno = errno;
	  t_error("VIDIOC_S_INPUT", info);
	  return -1;
	}
    }

  return (info->num_inputs = i);
}

/*
  Sets the current input for the capture
*/
int tveng2_set_input(struct tveng_enum_input * input,
		     tveng_device_info * info)
{
  enum tveng_capture_mode current_mode;
  struct v4l2_input new_input;

  t_assert(info != NULL);
  t_assert(input != NULL);

  current_mode = tveng_stop_everything(info);

  new_input.index = input->id;
  if (ioctl(info->fd, VIDIOC_S_INPUT, &new_input))
    {
      info -> tveng_errno = errno;
      t_error("VIDIOC_S_INPUT", info);
      return -1;
    }

  /* Start capturing again as if nothing had happened */
  if (tveng_restart_everything(current_mode, info) == -1)
    return -1;

  info->cur_input = input->id;

  /* Maybe there are some other standards, get'em again */
  tveng2_get_standards(info);

  return 0;
}

/*
  Sets the input named name as the active input. -1 on error.
*/
int
tveng2_set_input_by_name(const char * input_name,
			 tveng_device_info * info)
{
  int i;

  t_assert(input_name != NULL);
  t_assert(info != NULL);

  for (i = 0; i < info->num_inputs; i++)
    if (!strcasecmp(info->inputs[i].name, input_name))
      return tveng2_set_input(&(info->inputs[i]), info);

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
tveng2_set_input_by_id(int id, tveng_device_info * info)
{
  int i;

  t_assert(info != NULL);

  for (i = 0; i < info->num_inputs; i++)
    if (info->inputs[i].id == id)
      return tveng2_set_input(&(info->inputs[i]), info);

  info->tveng_errno = -1;
  snprintf(info->error, 256, 
	   _("Input number %d doesn't appear to exist"), id);

  return -1; /* String not found */
}

/*
  Sets the active input by its index in inputs. -1 on error
*/
int
tveng2_set_input_by_index(int index, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(index > -1);

  if (info->num_inputs)
    {
      t_assert(index < info -> num_inputs);
      return (tveng2_set_input(&(info -> inputs[index]), info));
    }
  return 0;
}

/*
  Returns the number of standards in the given device and fills in info
  with the correct info, allocating memory as needed
*/
int tveng2_get_standards(tveng_device_info * info)
{
  int count = 0; /* Number of available standards */
  struct v4l2_enumstd enumstd;
  struct v4l2_standard std;
  int i;

  /* free any previously allocated mem */
  if (info->standards)
    free(info->standards);

  info->standards = NULL;
  info->cur_standard = 0;
  info->num_standards = 0;
  info->tveng_errno = 0;

  /* Start querying for the supported standards */
  for (i = 0;; i++)
    {
      memset(&enumstd, 0, sizeof (struct v4l2_enumstd));
      enumstd.index = i;
      if (ioctl(info->fd, VIDIOC_ENUMSTD, &enumstd))
	break;

      /* Check that this standard is supported by the current input */
      if ((enumstd.inputs & (1 << info->cur_input)) == 0)
	continue; /* Unsupported by the current input */

      info->standards = realloc(info->standards,
				(count+1)*sizeof(struct tveng_enumstd));
      info->standards[count].index = count;
      info->standards[count].id = i;
      snprintf(info->standards[count].name, 24, enumstd.std.name);
      count++;
    }

  info -> num_standards = count;

  if (info->num_standards == 0)
    return 0; /* We are done, avoid the rest */

  /* Get the current standard */
  memset(&std, 0, sizeof(struct v4l2_standard));
  if (ioctl(info->fd, VIDIOC_G_STD, &std))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_G_STD", info);
      return -1;
    }

  for (i=0; i<info->num_standards; i++)
    if (!strcasecmp(std.name, info->standards[i].name))
      {
	info->cur_standard = i;
	break;
      }

  if (i == info->num_standards) /* Current standard not found */
    /* Set the first standard as active */
    tveng2_set_standard(&(info->standards[0]), info);
  
  return (info->num_standards);
}

/*
  Sets the current standard for the capture. standard is the name for
  the desired standard. updates cur_standard
*/
int tveng2_set_standard(struct tveng_enumstd * std, tveng_device_info * info)
{
  enum tveng_capture_mode current_mode;
  struct v4l2_enumstd enumstd;

  t_assert(info != NULL);
  t_assert(std != NULL);

  current_mode = tveng_stop_everything(info);

  /* Get info about the standard we are going to set */
  memset(&enumstd, 0, sizeof(struct v4l2_enumstd));
  enumstd.index = std -> id;
  if (ioctl(info->fd, VIDIOC_ENUMSTD, &enumstd))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_ENUMSTD", info);
      tveng_restart_everything(current_mode, info);
      return -1;
    }

  /* Now set it */
  if (ioctl(info->fd, VIDIOC_S_STD, &(enumstd.std)))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_S_STD", info);
      tveng_restart_everything(current_mode, info);
      return -1;
    }
  
  info->cur_standard = std->index;

  /* Start capturing again as if nothing had happened */
  return tveng_restart_everything(current_mode, info);
}

/*
  Sets the standard by name. -1 on error
*/
int
tveng2_set_standard_by_name(char * name, tveng_device_info * info)
{
  int i;
  for (i = 0; i < info->num_standards; i++)
    if (!strcmp(name, info->standards[i].name))
      return tveng2_set_standard(&(info->standards[i]), info);

  info->tveng_errno = -1;
  snprintf(info->error, 256, 
	   _("Standard %s doesn't appear to exist"), name);

  return -1; /* String not found */  
}

/*
  Sets the standard by id.
*/
int
tveng2_set_standard_by_id(int id, tveng_device_info * info)
{
  int i;
  for (i = 0; i < info->num_standards; i++)
    if (info->standards[i].id == id)
      return tveng2_set_standard(&(info->standards[i]), info);

  info->tveng_errno = -1;
  snprintf(info->error, 256, 
	   _("Standard %d doesn't appear to exist"), id);

  return -1; /* id not found */
}

/*
  Sets the standard by index. -1 on error
*/
int
tveng2_set_standard_by_index(int index, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(index > -1);

  if (info->num_standards)
    {
      t_assert(index < info->num_standards);
      return (tveng2_set_standard(&(info->standards[index]), info));
    }
  return 0;
}

int
tveng2_update_capture_format(tveng_device_info * info)
{
  struct v4l2_format format;
  struct v4l2_window window;

  t_assert(info != NULL);

  memset(&format, 0, sizeof(struct v4l2_format));
  memset(&window, 0, sizeof(struct v4l2_window));

  format.type = V4L2_BUF_TYPE_CAPTURE;
  if (ioctl(info->fd, VIDIOC_G_FMT, &format))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_G_FMT", info);
      return -1;
    }
  if (ioctl(info->fd, VIDIOC_G_WIN, &window))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_G_WIN", info);
      return -1;
    }
  info->format.bpp = ((double)format.fmt.pix.depth)/8;
  info->format.width = format.fmt.pix.width;
  info->format.height = format.fmt.pix.height;
  if (format.fmt.pix.flags & V4L2_FMT_FLAG_BYTESPERLINE)
    info->format.bytesperline = format.fmt.pix.bytesperline;
  else
    info->format.bytesperline = info->format.bpp * info->format.width;
  info->format.sizeimage = format.fmt.pix.sizeimage;
  switch (format.fmt.pix.pixelformat)
    {
    case V4L2_PIX_FMT_RGB555:
      info->format.depth = 15;
      info->format.pixformat = TVENG_PIX_RGB555;
      break;
    case V4L2_PIX_FMT_RGB565:
      info->format.depth = 16;
      info->format.pixformat = TVENG_PIX_RGB565;
      break;
    case V4L2_PIX_FMT_RGB24:
      info->format.depth = 24;
      info->format.pixformat = TVENG_PIX_RGB24;
      break;
    case V4L2_PIX_FMT_BGR24:
      info->format.depth = 24;
      info->format.pixformat = TVENG_PIX_BGR24;
      break;
    case V4L2_PIX_FMT_RGB32:
      info->format.depth = 32;
      info->format.pixformat = TVENG_PIX_RGB32;
      break;
    case V4L2_PIX_FMT_BGR32:
      info->format.depth = 32;
      info->format.pixformat = TVENG_PIX_BGR32;
      break;
    case V4L2_PIX_FMT_YVU420:
      info->format.depth = 12;
      info->format.pixformat = TVENG_PIX_YVU420;
      break;
#ifdef V4L2_PIX_FMT_YUV420 /* not in the spec, but very common */
    case V4L2_PIX_FMT_YUV420:
      info->format.depth = 12;
      info->format.pixformat = TVENG_PIX_YUV420;
      break;
#endif
    case V4L2_PIX_FMT_UYVY:
      info->format.depth = 16;
      info->format.pixformat = TVENG_PIX_UYVY;
      break;
    case V4L2_PIX_FMT_YUYV:
      info->format.depth = 16;
      info->format.pixformat = TVENG_PIX_YUYV;
      break;
    case V4L2_PIX_FMT_GREY:
      info->format.depth = 8;
      info->format.pixformat = TVENG_PIX_GREY;
      break;
    default:
      info->tveng_errno = -1; /* unknown */
      t_error_msg("switch()",
		  _("Cannot understand the actual palette"), info);

      return -1;    
    };
  info->window.x = window.x;
  info->window.y = window.y;
  info->window.width = window.width;
  info->window.height = window.height;
  info->window.chromakey = window.chromakey;
  /* These two are defined as read-only */
  info->window.clipcount = 0;
  info->window.clips = NULL;
  return 0;
}

/* -1 if failed. Sets the pixformat and fills in info -> pix_format
   with the correct values  */
int
tveng2_set_capture_format(tveng_device_info * info)
{
  struct v4l2_format format;

  t_assert(info != NULL);

  memset(&format, 0, sizeof(struct v4l2_format));

  format.type = V4L2_BUF_TYPE_CAPTURE;

  /* Transform the given palette value into a V4L value */
  switch(info->format.pixformat)
    {
    case TVENG_PIX_RGB555:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB555;
      format.fmt.pix.depth = 15;
      break;
    case TVENG_PIX_RGB565:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
      format.fmt.pix.depth = 16;
      break;
    case TVENG_PIX_RGB24:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
      format.fmt.pix.depth = 24;
      break;
    case TVENG_PIX_BGR24:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
      format.fmt.pix.depth = 24;
      break;
    case TVENG_PIX_RGB32:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB32;
      format.fmt.pix.depth = 32;
      break;
    case TVENG_PIX_BGR32:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR32;
      format.fmt.pix.depth = 32;
      break;
    case TVENG_PIX_YUV420:
#ifdef V4L2_PIX_FMT_YUV420 /* not in the spec, but very common */
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
      format.fmt.pix.depth = 12;
      break;
#endif
    case TVENG_PIX_YVU420:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_YVU420;
      format.fmt.pix.depth = 12;
      break;
    case TVENG_PIX_UYVY:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
      format.fmt.pix.depth = 16;
      break;
    case TVENG_PIX_YUYV:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
      format.fmt.pix.depth = 16;
      break;
    default:
      info->tveng_errno = -1; /* unknown */
      t_error_msg("switch()", _("Cannot understand the given palette"),
		  info);
      return -1;
    }

  /* Adjust the given dimensions */
  /* Make them 4-byte multiplus to avoid errors */
  info->format.width = (info->format.width+3) & ~3;
  info->format.height = (info->format.height+3) & ~3;
  if (info->format.height < info->caps.minheight)
    info->format.height = info->caps.minheight;
  if (info->format.height > info->caps.maxheight)
    info->format.height = info->caps.maxheight;
  if (info->format.width < info->caps.minwidth)
    info->format.width = info->caps.minwidth;
  if (info->format.width > info->caps.maxwidth)
    info->format.width = info->caps.maxwidth;
  
  format.fmt.pix.width = (info->format.width+3) & ~3;
  format.fmt.pix.height = (info->format.height+3) & ~3;
  format.fmt.pix.bytesperline = ((format.fmt.pix.depth+7)>>3) *
    format.fmt.pix.width;
  format.fmt.pix.sizeimage =
    format.fmt.pix.bytesperline * format.fmt.pix.height;
  format.fmt.pix.flags = V4L2_FMT_FLAG_INTERLACED;

  /* everything is set up */
  if (ioctl(info->fd, VIDIOC_S_FMT, &format))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_S_FMT", info);
      return -1;
    }

  /* Check fill in info with the current values (may not be the ones
     requested) */
  if (tveng2_update_capture_format(info) == -1)
    return -1; /* error */

  return 0; /* Success */
}

/* private, add a control to the control structure, -1 means ENOMEM */
static int
p_tveng2_append_control(struct tveng_control * new_control, 
			tveng_device_info * info);

static int
p_tveng2_append_control(struct tveng_control * new_control, 
			tveng_device_info * info)
{
  struct tveng_control * new_pointer = (struct tveng_control*)
    realloc(info->controls, (info->num_controls+1)*
	    sizeof(struct tveng_control));

  t_assert(info != NULL);

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

/* To aid i18n, possible label isn't actually used */
struct p_tveng2_control_with_i18n
{
  __u32 cid;
  char * possible_label;
};

/* Private, builds the controls structure */
static int
p_tveng2_build_controls(tveng_device_info * info)
{
  struct v4l2_queryctrl qc;
  struct v4l2_querymenu qm;
  struct tveng_control control;
  int i;
  int j;
  int p;

  /* This shouldn't be neccessary is control querying worked properly */
  /* FIXME: add audio subchannels selecting controls */
  struct p_tveng2_control_with_i18n cids[] =
  {
    {V4L2_CID_BRIGHTNESS, N_("Brightness")},
    {V4L2_CID_CONTRAST, N_("Contrast")},
    {V4L2_CID_SATURATION, N_("Saturation")},
    {V4L2_CID_HUE, N_("Hue")},
    {V4L2_CID_WHITENESS, N_("Whiteness")},
    {V4L2_CID_BLACK_LEVEL, N_("Black level")},
    {V4L2_CID_AUTO_WHITE_BALANCE, N_("White balance")},
    {V4L2_CID_DO_WHITE_BALANCE, N_("Do white balance")},
    {V4L2_CID_RED_BALANCE, N_("Red balance")},
    {V4L2_CID_BLUE_BALANCE, N_("Blue balance")},
    {V4L2_CID_GAMMA, N_("Gamma")},
    {V4L2_CID_EXPOSURE, N_("Exposure")},
    {V4L2_CID_AUTOGAIN, N_("Auto gain")},
    {V4L2_CID_GAIN, N_("Gain")},
    {V4L2_CID_HCENTER, N_("HCenter")},
    {V4L2_CID_VCENTER, N_("VCenter")},
    {V4L2_CID_HFLIP, N_("Horizontal flipping")},
    {V4L2_CID_VFLIP, N_("Vertical flipping")},
    {V4L2_CID_AUDIO_VOLUME, N_("Volume")},
    {V4L2_CID_AUDIO_MUTE, N_("Mute")},
    {V4L2_CID_AUDIO_MUTE, N_("Audio Mute")},
    {V4L2_CID_AUDIO_BALANCE, N_("Balance")},
    {V4L2_CID_AUDIO_BALANCE, N_("Audio Balance")},
    {V4L2_CID_AUDIO_TREBLE, N_("Treble")},
    {V4L2_CID_AUDIO_LOUDNESS, N_("Loudness")},
    {V4L2_CID_AUDIO_BASS, N_("Bass")}
  };

  memset(&qc, 0, sizeof(struct v4l2_queryctrl));
  for (p = V4L2_CID_BASE; p < V4L2_CID_LASTP1; p++)
    {
      qc.id = p;
      if ((ioctl(info->fd, VIDIOC_QUERYCTRL, &qc) == 0) &&
	  (!(qc.flags & V4L2_CTRL_FLAG_DISABLED)))
	{
	  snprintf(control.name, 32, qc.name);
	  /* search for possible translations */
	  for (i=0;
	       i<sizeof(cids)/sizeof(struct p_tveng2_control_with_i18n);
	       i++)
	    if ((strcasecmp(_(qc.name), qc.name)) &&
		(qc.id == cids[i].cid))
	      {
		snprintf(control.name, 32, _(qc.name));
		break; /* translation present for the given control */
	      }
	  control.name[31] = 0;
	  control.min = qc.minimum;
	  control.max = qc.maximum;
	  control.id = qc.id;
	  switch (qc.type)
	    {
	    case V4L2_CTRL_TYPE_INTEGER:
	      control.type = TVENG_CONTROL_SLIDER;
	      control.data = NULL;
	      break;
	    case V4L2_CTRL_TYPE_BOOLEAN:
	      control.type = TVENG_CONTROL_CHECKBOX;
	      control.data = NULL;
	      break;
	    case V4L2_CTRL_TYPE_BUTTON:
	      control.type = TVENG_CONTROL_BUTTON;
	      control.data = NULL;
	      break;
	    case V4L2_CTRL_TYPE_MENU:
	      control.type = TVENG_CONTROL_MENU;
	      control.data = NULL;
	      /* This needs a more complex management */
	      for (j=0; j<=control.max; j++)
		{
		  control.data = realloc(control.data,
					 (j+1)*sizeof(char*));
		  qm.id = qc.id;
		  qm.index = j;
		  if (ioctl(info->fd, VIDIOC_QUERYMENU, &qm))
		    control.data[j] =
			strdup(_("<Broken menu entry>"));
		  else
		    control.data[j] = strdup(_(qm.name));
		}
	      control.data = realloc(control.data,
				     (j+1)*sizeof(char*));
	      control.data[j] = NULL;
	      break;
	    default:
	      fprintf(stderr,
		      _("V4L2: Unknown control type 0x%x (%s)\n"),
		      qc.type, qc.name);
	      continue; /* Skip this one */
	    }
	  if (p_tveng2_append_control(&control, info) == -1)
	    return -1;
	}
    }
  return tveng2_update_controls(info);
}

/*
  Gets the current value of the controls, fills in info->controls
  appropiately. After this (and if it succeeds) you can look in
  info->controls to get the values for each control. -1 on error
*/
int
tveng2_update_controls(tveng_device_info * info)
{
  int i;
  struct v4l2_control c;

  t_assert(info != NULL);
  t_assert(info->num_controls>0);
  t_assert(info->controls != NULL);

  for (i=0; i<info->num_controls; i++)
    {
      c.id = info->controls[i].id;
      if (ioctl(info->fd, VIDIOC_G_CTRL, &c))
	{
	  info->tveng_errno = errno;
	  t_error("VIDIOC_G_CTRL", info);
	  c.value = 0; /* This shouldn't be critical */
	}
      info->controls[i].cur_value = c.value;
    }
  return 0;
}

/*
  Sets the value for an specific control. The given value will be
  clipped between min and max values. Returns -1 on error
*/
int
tveng2_set_control(struct tveng_control * control, int value,
		   tveng_device_info * info)
{
  struct v4l2_control c;

  t_assert(control != NULL);
  t_assert(info != NULL);

  t_assert(control != NULL);
  t_assert(info != NULL);

  /* Clip value to a valid one */
  if (value < control->min)
    value = control -> min;

  if (value > control->max)
    value = control -> max;

  c.id = control->id;
  c.value = value;

  if (ioctl(info->fd, VIDIOC_S_CTRL, &c))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_S_CTRL", info);
      return -1;
    }

  return (tveng2_update_controls(info));
}
/*
  Gets the value of a control, given its name. Returns -1 on
  error. The comparison is performed disregarding the case. The value
  read is stored in cur_value.
*/
int
tveng2_get_control_by_name(const char * control_name,
			   int * cur_value,
			   tveng_device_info * info)
{
  int i;
  int value;

  t_assert(info != NULL);
  t_assert(info -> num_controls > 0);
  t_assert(control_name != NULL);

  /* Update the controls (their values) */
  if (tveng2_update_controls(info) == -1)
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
tveng2_set_control_by_name(const char * control_name,
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
      return (tveng2_set_control(&(info->controls[i]), new_value, info));

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
tveng2_get_control_by_id(int cid, int * cur_value,
			 tveng_device_info * info)
{
  int i;
  int value;

  t_assert(info != NULL);
  t_assert(info -> num_controls > 0);

  /* Update the controls (their values) */
  if (tveng2_update_controls(info) == -1)
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
  return -1;
}

/*
  Sets a control by its id. Returns -1 on error
*/
int tveng2_set_control_by_id(int cid, int new_value,
			    tveng_device_info * info)
{
  int i;

  t_assert(info != NULL);
  t_assert(info -> num_controls > 0);

  /* iterate through the info struct to find the mute control */
  for (i = 0; i < info->num_controls; i++)
    if (info->controls[i].id == cid)
      /* we found it */
      return (tveng2_set_control(&(info->controls[i]), new_value,
				 info));

  /* if we reach this, we haven't found the control */
  info->tveng_errno = -1;
  snprintf(info->error, 256, 
	   _("Cannot find control %d in the list of controls"),
	   cid);
  return -1;
}

/*
  Gets the value of the mute property. 1 means mute (no sound) and 0
  unmute (sound). -1 on error
*/
int
tveng2_get_mute(tveng_device_info * info)
{
  int returned_value;
  if (tveng2_get_control_by_id(V4L2_CID_AUDIO_MUTE, &returned_value, info) ==
      -1)
    return -1;
  return returned_value;
}

/*
  Sets the value of the mute property. 0 means unmute (sound) and 1
  mute (no sound). -1 on error
*/
int
tveng2_set_mute(int value, tveng_device_info * info)
{
  return (tveng2_set_control_by_id(V4L2_CID_AUDIO_MUTE, value, info));
}

/*
  Tunes the current input to the given freq. Returns -1 on error.
*/
int
tveng2_tune_input(__u32 _freq, tveng_device_info * info)
{
  struct v4l2_tuner tuner_info;
  __u32 freq; /* real frequence passed to v4l2 */

  t_assert(info != NULL);
  t_assert(info->cur_input < info->num_inputs);
  t_assert(info->cur_input >= 0);

  /* Check that there are tuners in the current input */
  if (info->inputs[info->cur_input].tuners == 0)
    return 0; /* Success (we shouldn't be tuning, anyway) */

  /* Get more info about this tuner */
  tuner_info.input = info->inputs[info->cur_input].id;
  if (ioctl(info -> fd, VIDIOC_G_TUNER, &tuner_info) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_G_TUNER", info);
      return -1;
    }
  
  if (tuner_info.capability & V4L2_TUNER_CAP_LOW)
    freq = _freq / 0.0625;
  else
    freq = _freq / 62.5;

  if (freq > tuner_info.rangehigh)
    freq = tuner_info.rangehigh;
  if (freq < tuner_info.rangelow)
    freq = tuner_info.rangelow;
  
  /* OK, everything is set up, try to tune it */
  if (ioctl(info -> fd, VIDIOC_S_FREQ, &freq))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_S_FREQ", info);
      return -1;
    }

  return 0; /* Success */
}

/*
  Gets the signal strength and the afc code. The afc code indicates
  how to get a better signal, if negative, tune higher, if negative,
  tune lower. 0 means no idea or feature not present in the current
  controller (i.e. V4L1). Strength and/or afc can be NULL pointers,
  that would mean ignore that parameter.
*/
int
tveng2_get_signal_strength (int *strength, int * afc,
			   tveng_device_info * info)
{
  struct v4l2_tuner tuner;

  t_assert(info != NULL);
  t_assert(info->cur_input < info->num_inputs);
  t_assert(info->cur_input >= 0);

  /* Check that there are tuners in the current input */
  if (info->inputs[info->cur_input].tuners == 0)
    return -1;

  tuner.input = info->inputs[info->cur_input].id;
  if (ioctl(info -> fd, VIDIOC_G_TUNER, &tuner) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_G_TUNER", info);
      return -1;
    }

  if (strength)
    {
      /*
	Properly we should only return the signal field, but it doesn't
	always work :-/
	This has the advantage that it will find most stations (with a
	good reception) and the disadvantage that it will find too
	many stations... but better too many than too few :-)
      */
#if 0
      /* update: bttv2 does use the signal field, so lets use it
	 instead */
      if (tuner.signal)
	*strength = tuner.signal;
      else if (tuner.afc == 0)
	*strength = 65535;
      else
	*strength = 0;
#else
      /* This is the correct method, but it doesn't always work */
      *strength = tuner.signal;
#endif
    }

  if (afc)
    *afc = tuner.afc;

  return 0; /* Success */
}

/*
  Stores in freq the currently tuned freq. Returns -1 on error.
*/
int
tveng2_get_tune(__u32 * freq, tveng_device_info * info)
{
  __u32 real_freq;
  struct v4l2_tuner tuner;

  t_assert(info != NULL);
  t_assert(freq != NULL);
  t_assert(info->cur_input < info->num_inputs);
  t_assert(info->cur_input >= 0);

  /* Check that there are tuners in the current input */
  if (info->inputs[info->cur_input].tuners == 0)
    {
      if (freq)
	*freq = 0;
      info->tveng_errno = -1;
      t_error_msg("tuners check",
		  _("There are no tuners for the active input"),
		  info);
      return -1;
    }

  if (ioctl(info->fd, VIDIOC_G_FREQ, &real_freq) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_G_FREQ", info);
      return -1;
    }

  /* Get more info about this tuner */
  tuner.input = info->inputs[info->cur_input].id;
  if (ioctl(info -> fd, VIDIOC_G_TUNER, &tuner) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_G_TUNER", info);
      return -1;
    }
  
  if (tuner.capability & V4L2_TUNER_CAP_LOW)
    *freq = real_freq * 0.0625;
  else
    *freq = real_freq * 62.5;

  return 0;
}

/*
  Gets the minimum and maximum freq that the current input can
  tune. If there is no tuner in this input, -1 will be returned.
  If any of the pointers is NULL, its value will not be filled.
*/
int
tveng2_get_tuner_bounds(__u32 * min, __u32 * max, tveng_device_info *
			info)
{
  struct v4l2_tuner tuner;

  t_assert(info != NULL);
  t_assert(info->cur_input < info->num_inputs);
  t_assert(info->cur_input >= 0);

  /* Check that there are tuners in the current input */
  if (info->inputs[info->cur_input].tuners == 0)
    return -1;

  /* Get info about the current tuner */
  tuner.input = info->inputs[info->cur_input].id;
  if (ioctl(info -> fd, VIDIOC_G_TUNER, &tuner) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_G_TUNER", info);
      return -1;
    }

  if (min)
    *min = tuner.rangelow;
  if (max)
    *max = tuner.rangehigh;

  if (tuner.capability & V4L2_TUNER_CAP_LOW)
    {
      if (min)
	*min *= 0.0625;
      if (max)
	*max *= 0.0625;
    }
  else
    {
      if (min)
	*min *= 62.5;
      if (max)
	*max *= 62.5;
    }

  return 0; /* Success */
}

/* Some private functions */
static int p_tveng2_qbuf(int index, tveng_device_info * info);
static int p_tveng2_dqbuf(tveng_device_info * info);

/* Queues an specific buffer. -1 on error */
static int p_tveng2_qbuf(int index, tveng_device_info * info)
{
  struct v4l2_buffer tmp_buffer;
  struct private_tveng2_device_info * p_info =
    (struct private_tveng2_device_info*) info;

  t_assert(info != NULL);

  tmp_buffer.type = p_info -> buffers[0].vidbuf.type;
  tmp_buffer.index = index;

  if (ioctl(info->fd, VIDIOC_QBUF, &tmp_buffer))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_QBUF", info);
      return -1;
    }

  return 0;
}

/* dequeues next available buffer and returns it's id. -1 on error */
static int p_tveng2_dqbuf(tveng_device_info * info)
{
  struct v4l2_buffer tmp_buffer;
  struct private_tveng2_device_info * p_info =
    (struct private_tveng2_device_info*) info;
  __s64 init_timestamp;

  t_assert(info != NULL);

  tmp_buffer.type = p_info -> buffers[0].vidbuf.type;

  if (ioctl(info->fd, VIDIOC_DQBUF, &tmp_buffer))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_DQBUF", info);
      return -1;
    }

  init_timestamp = ((__s64)info->tv_init.tv_sec) * 1000000 +
    info->tv_init.tv_usec;
  init_timestamp *= 1000;

  p_info -> last_timestamp = tmp_buffer.timestamp;
  p_info -> last_timestamp -= init_timestamp;

  return (tmp_buffer.index);
}

/*
  Sets up the capture device so any read() call after this one
  succeeds. Returns -1 on error.
*/
int
tveng2_start_capturing(tveng_device_info * info)
{
  struct v4l2_requestbuffers rb;
  struct private_tveng2_device_info * p_info =
    (struct private_tveng2_device_info*) info;
  int i;

  t_assert(info != NULL);

  tveng_stop_everything(info);

  t_assert(info -> current_mode == TVENG_NO_CAPTURE);

  p_info -> buffers = NULL;
  p_info -> num_buffers = 0;

  rb.count = 8; /* This is a good number */
  rb.type = V4L2_BUF_TYPE_CAPTURE;
  if (ioctl(info->fd, VIDIOC_REQBUFS, &rb))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_REQBUFS", info);
      return -1;
    }

  if (rb.count == 0)
    {
      info->tveng_errno = -1;
      t_error_msg("check()", _("Not enough buffers"), info);
      return -1;
    }

  p_info -> buffers = (struct tveng2_vbuf*)
    malloc(rb.count*sizeof(struct tveng2_vbuf));
  p_info -> num_buffers = rb.count;

  for (i = 0; i < rb.count; i++)
    {
      p_info -> buffers[i].vidbuf.index = i;
      p_info -> buffers[i].vidbuf.type = V4L2_BUF_TYPE_CAPTURE;
      if (ioctl(info->fd, VIDIOC_QUERYBUF,
		&(p_info->buffers[i].vidbuf)))
	{
	  info->tveng_errno = errno;
	  t_error("VIDIOC_QUERYBUF", info);
	  return -1;
	}
      p_info->buffers[i].vmem =
	mmap(0, p_info->buffers[i].vidbuf.length,
	     PROT_READ,
	     MAP_SHARED, info->fd, 
	     p_info->buffers[i].vidbuf.offset);
      if ((int)p_info->buffers[i].vmem == -1)
	{
	  info->tveng_errno = errno;
	  t_error("mmap()", info);
	  return -1;
	}
      
	/* Queue the buffer */
      if (p_tveng2_qbuf(i, info) == -1)
	return -1;
    }

  /* Turn on streaming */
  i = V4L2_BUF_TYPE_CAPTURE;
  if (ioctl(info->fd, VIDIOC_STREAMON, &i) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_STREAMON", info);
      return -1;
    }

  p_info -> last_timestamp = -1;

  info->current_mode = TVENG_CAPTURE_READ;

  return 0;
}

/* Tries to stop capturing. -1 on error. */
int
tveng2_stop_capturing(tveng_device_info * info)
{
  struct private_tveng2_device_info * p_info =
    (struct private_tveng2_device_info*) info;
  int i;

  if (info -> current_mode == TVENG_NO_CAPTURE)
    {
      fprintf(stderr, 
	      _("Warning: trying to stop capture with no capture active\n"));
      return 0; /* Nothing to be done */
    }

  t_assert(info->current_mode == TVENG_CAPTURE_READ);

  /* Turn streaming off */
  i = V4L2_BUF_TYPE_CAPTURE;
  if (ioctl(info->fd, VIDIOC_STREAMOFF, &i))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_STREAMOFF", info);
      return -1;
    }

  for (i = 0; i < p_info -> num_buffers; i++)
    {
      if (munmap(p_info -> buffers[i].vmem,
		 p_info -> buffers[i].vidbuf.length))
	{
	  info->tveng_errno = errno;
	  t_error("munmap()", info);
	}
    }

  if (p_info -> buffers)
    free(p_info -> buffers);

  p_info -> last_timestamp = -1;

  info->current_mode = TVENG_NO_CAPTURE;

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
int tveng2_read_frame(void * where, unsigned int size, 
		      unsigned int time, tveng_device_info * info)
{
  struct private_tveng2_device_info * p_info =
    (struct private_tveng2_device_info*) info;
  int n; /* The dequeued buffer */
  fd_set rdset;
  struct timeval timeout;

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

  /* Fill in the rdset structure */
  FD_ZERO(&rdset);
  FD_SET(info->fd, &rdset);
  timeout.tv_sec = 0;
  timeout.tv_usec = time*1000;
  n = select(info->fd +1, &rdset, NULL, NULL, &timeout);
  if (n == -1)
    {
      info->tveng_errno = errno;
      t_error("select()", info);
      return -1;
    }
  else if (n == 0)
    return 0; /* This isn't properly an error, just a timeout */

  t_assert(FD_ISSET(info->fd, &rdset)); /* Some sanity check */

  n = p_tveng2_dqbuf(info);
  if (n == -1)
    return -1;

  /* Ignore frames we haven't been able to process */
  do{
    FD_ZERO(&rdset);
    FD_SET(info->fd, &rdset);
    timeout.tv_sec = timeout.tv_usec = 0;
    if (select(info->fd +1, &rdset, NULL, NULL, &timeout) < 1)
      break;
    p_tveng2_qbuf(n, info);
    n = p_tveng2_dqbuf(info);
  } while (1);

  /* Copy the data to the address given */
  memcpy(where, p_info->buffers[n].vmem,
	 size);

  /* Queue the buffer again for processing */
  if (p_tveng2_qbuf(n, info))
    return -1;

  /* Everything has been OK, return 0 (success) */
  return 0;
}

/*
  Gets the timestamp of the last read frame.
  Returns -1 on error, if the current mode isn't capture, or if we
  haven't captured any frame yet. The timestamp is relative to when we
  started streaming, and is calculated with the following formula:
  timestamp = (sec*1000000+usec)*1000
*/
__s64 tveng2_get_timestamp(tveng_device_info * info)
{
  struct private_tveng2_device_info * p_info =
    (struct private_tveng2_device_info *) info;

  t_assert(info != NULL);

  if (info->current_mode != TVENG_CAPTURE_READ)
    return -1;

  return (p_info -> last_timestamp);
}

/* 
   Sets the capture buffer to an specific size. returns -1 on
   error. Remember to check the value of width and height since it can
   be different to the one requested. 
*/
int tveng2_set_capture_size(int width, int height, tveng_device_info * info)
{
  enum tveng_capture_mode current_mode;
  t_assert(info != NULL);
  t_assert(width > 0);
  t_assert(height > 0);

  current_mode = tveng_stop_everything(info);

  if (width < info->caps.minwidth)
    width = info->caps.minwidth;
  else if (width > info->caps.maxwidth)
    width = info->caps.maxwidth;
  if (height < info->caps.minheight)
    height = info->caps.minheight;
  else if (height > info->caps.maxheight)
    height = info->caps.maxheight;
  
  info -> format.width = width;
  info -> format.height = height;
  if (tveng2_set_capture_format(info) == -1)
    return -1;

  /* Restart capture again */
  return tveng_restart_everything(current_mode, info);
}

/* 
   Gets the actual size of the capture buffer in width and height.
   -1 on error
*/
int tveng2_get_capture_size(int *width, int *height, tveng_device_info * info)
{
  t_assert(info != NULL);

  if (tveng2_update_capture_format(info))
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
tveng2_detect_preview (tveng_device_info * info)
{
  struct v4l2_framebuffer fb;

  t_assert(info != NULL);

  if ((info -> caps.flags & TVENG_CAPS_OVERLAY) == 0)
    {
      info -> tveng_errno = -1;
      t_error_msg("flags check",
       _("The capability field says that there is no overlay"), info);
      return 0;
    }

  /* Get the current framebuffer info */
  if (ioctl(info -> fd, VIDIOC_G_FBUF, &fb))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGFBUF", info);
      return 0;
    }

  info->fb_info.base = fb.base;
  info->fb_info.height = fb.fmt.height;
  info->fb_info.width = fb.fmt.width;
  info->fb_info.depth = fb.fmt.depth;
  info->fb_info.bytesperline = fb.fmt.bytesperline;

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
tveng2_set_preview_window(tveng_device_info * info)
{
  struct v4l2_window window;
  struct v4l2_clip * clip=NULL;
  int i;

  t_assert(info != NULL);
  t_assert(info-> window.clipcount >= 0);

  memset(&window, 0, sizeof(struct v4l2_window));

  /* We do not set the chromakey value */
  window.x = (info->window.x+3) & ~3;
  window.y = info->window.y;
  window.width = info->window.width & ~3;
  window.height = info->window.height;
  window.clipcount = info->window.clipcount;
  if (window.clipcount == 0)
    window.clips = NULL;
  else
    {
      clip = malloc(sizeof(struct v4l2_clip)*window.clipcount);
      window.clips = clip;
      for (i=0;i<window.clipcount;i++)
	{
	  clip[i].x = info->window.clips[i].x;
	  clip[i].y = info->window.clips[i].y;
	  clip[i].width = info->window.clips[i].width;
	  clip[i].height = info->window.clips[i].height;
	  clip[i].next = ((i+1) == window.clipcount) ? NULL : &(clip[i+1]);
	}
    }

  /* Set the new window */
  if (ioctl(info->fd, VIDIOC_S_WIN, &window))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_S_WIN", info);
      free(clip);
      return -1;
    }

  free(clip);

  /* Update the info struct */
  return (tveng2_get_preview_window(info));
}

/*
  Gets the current overlay window parameters.
  Returns -1 on error, and any other value on success.
  info   : The device to use
*/
int
tveng2_get_preview_window(tveng_device_info * info)
{
  /* Updates the entire capture format, since there is no
     difference */
  return (tveng2_update_capture_format(info));
}

/* 
   Sets the previewing on/off.
   on : if 1, set preview on, if 0 off, other values are silently ignored
   info  : device to use for previewing
   Returns -1 on error, anything else on success
*/
int
tveng2_set_preview (int on, tveng_device_info * info)
{
  int one = 1, zero = 0;

  t_assert(info != NULL);

  if ((on < 0) || (on > 1))
    return 0;

  if (ioctl(info->fd, VIDIOC_PREVIEW, on ? &one : &zero))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_PREVIEW", info);
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
tveng2_start_previewing (tveng_device_info * info)
{
#ifndef DISABLE_X_EXTENSIONS
  Display * display = info->display;
  int width, height;
  int dwidth, dheight; /* Width and height of the display */

  tveng_stop_everything(info);

  t_assert(info -> current_mode == TVENG_NO_CAPTURE);

  if (!tveng2_detect_preview(info))
    /* We shouldn't be reaching this if the app is well programmed */
    t_assert_not_reached();

  if (!tveng_detect_XF86DGA(info))
    return -1;

  /* calculate coordinates for the preview window. We compute this for
   the first display */
  XF86DGAGetViewPortSize(display, DefaultScreen(display),
			 &dwidth, &dheight);
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
  if (tveng2_set_preview_window(info) == -1)
    return -1;

  /* Center preview window (maybe the requested width and/or height)
     aren't valid */
  info->window.x = (dwidth - info->window.width)/2;
  info->window.y = (dheight - info->window.height)/2;
  info->window.clipcount = 0;
  info->window.clips = NULL;
  if (tveng2_set_preview_window(info) == -1)
    return -1;

  /* Start preview */
  if (tveng2_set_preview(1, info) == -1)
    return -1;

  info -> current_mode = TVENG_CAPTURE_PREVIEW;
  return 0; /* Success */
#else
  info -> tveng_errno = -1;
  t_error_msg("configure()",
	      "The X extensions have been disabled when configuring",
	      info);
  return -1;
#endif
}

/*
  Stops the fullscreen mode. Returns -1 on error
*/
int
tveng2_stop_previewing(tveng_device_info * info)
{
#ifndef DISABLE_X_EXTENSIONS
  if (info -> current_mode == TVENG_NO_CAPTURE)
    {
      fprintf(stderr, 
	      _("Warning: trying to stop preview with no capture active\n"));
      return 0; /* Nothing to be done */
    }
  t_assert(info->current_mode == TVENG_CAPTURE_PREVIEW);

  /* No error checking */
  tveng2_set_preview(0, info);

  info -> current_mode = TVENG_NO_CAPTURE;
  return 0; /* Success */
#else
  return 0;
#endif
}

int tveng2_get_private_size(void)
{
  return (sizeof(struct private_tveng2_device_info));
}
