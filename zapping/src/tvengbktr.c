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

#if 0 /*** TODO ***/

/*
  This is the library in charge of simplifying Video Access API (I
  don't want to use thirteen lines of code with ioctl's every time I
  want to change tuning freq).
  the name is TV Engine, since it is intended mainly for TV viewing.
  This file is separated so zapping doesn't need to know about V4L[2]
*/
#include <site_def.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef ENABLE_V4L
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <errno.h>
#include <linux/kernel.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifndef DISABLE_X_EXTENSIONS
#include <X11/extensions/xf86dga.h>
#endif

#include "tveng.h"
#define TVENGBKTR_PROTOTYPES 1
#include "tvengbktr.h"
#include "../common/videodev2.h" /* the V4L2 definitions */
#include "../common/types.h"

#define CLEAR(v) memset (&(v), 0, sizeof (v))

/* TFR repeats the ioctl when interrupted (EINTR) */
#define IOCTL(fd, cmd, data) (TEMP_FAILURE_RETRY(ioctl(fd, cmd, data)))

struct control {
	tv_dev_control		dev;
	unsigned int		id;	/* V4L2_CID_ */
};

#define C(l) PARENT (l, struct control, dev)
#define CC(l) PARENT (l, struct control, dev.pub)

struct tvengbktr_vbuf
{
  void * vmem; /* Captured image in this buffer */
  struct v4l2_buffer vidbuf; /* Info about the buffer */
};

struct private_tvengbktr_device_info
{
  tveng_device_info info; /* Info field, inherited */
  int num_buffers; /* Number of mmaped buffers */
  struct tvengbktr_vbuf * buffers; /* Array of buffers */
  double last_timestamp; /* The timestamp of the last captured buffer */
  uint32_t chroma;
  int audio_mode; /* 0 mono */
	tv_control *mute;
};

#define P_INFO(p) PARENT (p, struct private_tvengbktr_device_info, info)

/* Private, builds the controls structure */
static int
p_tvengbktr_build_controls(tveng_device_info * info);

static int p_tvengbktr_open_device_file(int flags, tveng_device_info * info);
/*
  Return fd for the device file opened. Checks if the device is a
  valid video device. -1 on error.
  Flags will be used for open()'ing the file 
*/
static int p_tvengbktr_open_device_file(int flags, tveng_device_info * info)
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

  if (IOCTL(info->fd, VIDIOC_QUERYCAP, &caps) != 0)
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
      if (IOCTL(info->fd, VIDIOC_G_FBUF, &fb) != 0)
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
static
int tvengbktr_attach_device(const char* device_file,
			 enum tveng_attach_mode attach_mode,
			 tveng_device_info * info)
{
  int error;
  struct private_tvengbktr_device_info * p_info =
    (struct private_tvengbktr_device_info*) info;

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

  if (attach_mode == TVENG_ATTACH_XV)
    attach_mode = TVENG_ATTACH_READ;

  switch (attach_mode)
    {
    case TVENG_ATTACH_CONTROL:
      info -> fd = p_tvengbktr_open_device_file(O_NOIO, info);
    case TVENG_ATTACH_READ:
      info -> fd = p_tvengbktr_open_device_file(O_RDWR, info);
      break;
    default:
      t_error_msg("switch()", "Unknown attach mode for the device",
		  info);
      free(info->file_name);
      info->file_name = NULL;
      return -1;
    };

  /*
    Errors (if any) are already aknowledged when we reach this point,
    so we don't show them again
  */
  if (info -> fd < 0)
    {
      free(info->file_name);
      info->file_name = NULL;
      return -1;
    }
  
  info -> attach_mode = attach_mode;
  /* Current capture mode is no capture at all */
  info -> current_mode = TVENG_NO_CAPTURE;

  /* We have a valid device, get some info about it */
  /* Fill in inputs */
  info->inputs = NULL;
  info->cur_input = 0;
  info->num_inputs = 0;
  error = tvengbktr_get_inputs(info);
  if (error < 0)
    {
      tvengbktr_close_device(info);
      return -1;
    }

  /* Fill in standards */
  info->standards = NULL;
  info->cur_standard = 0;
  info->num_standards = 0;
  error = tvengbktr_get_standards(info);
  if (error < 0)
    {
      tvengbktr_close_device(info);
      return -1;
    }

  /* Query present controls */
  info->controls = NULL;
  error = p_tvengbktr_build_controls(info);
  if (error == -1)
      return -1;

  /* Set up the palette according to the one present in the system */
  error = info->priv->current_bpp;

  if (error == -1)
    {
      tvengbktr_close_device(info);
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
		  "Cannot find appropiate palette for current display",
		  info);
      tvengbktr_close_device(info);
      return -1;
    }

  /* Get fb_info */
  tvengbktr_detect_preview(info);

  /* Pass some dummy values to the driver, so g_win doesn't fail */
  memset(&info->window, 0, sizeof(struct tveng_window));

  info->window.width = info->window.height = 16;

  tveng_set_preview_window(info);

  /* Set our desired size, make it halfway */
  info -> format.width = (info->caps.minwidth + info->caps.maxwidth)/2;
  info -> format.height = (info->caps.minheight +
			   info->caps.maxheight)/2;

  /* Set some capture format (not important) */
  tvengbktr_set_capture_format(info);

  /* Init the private info struct */
  p_info->num_buffers = 0;
  p_info->buffers = NULL;

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
static void
tvengbktr_describe_controller(char ** short_str, char ** long_str,
			   tveng_device_info * info)
{
  t_assert(info != NULL);
  if (short_str)
    *short_str = "V4L2";
  if (long_str)
    *long_str = "Video4Linux 2";
}

/* Closes a device opened with tveng_init_device */
static void tvengbktr_close_device(tveng_device_info * info)
{
  tv_control *tc;

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

	while ((tc = info->controls)) {
		info->controls = tc->next;
		free_control (tc);
	}

  info->num_standards = 0;
  info->num_inputs = 0;
  info->inputs = NULL;
  info->standards = NULL;
  info->file_name = NULL;
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
static
int tvengbktr_get_inputs(tveng_device_info * info)
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
      if (IOCTL(info->fd, VIDIOC_ENUMINPUT, &input) != 0)
	break;
      info->inputs = realloc(info->inputs, (i+1)*
			     sizeof(struct tveng_enum_input));
      info->inputs[i].id = i;
      info->inputs[i].index = i;
      snprintf(info->inputs[i].name, 32, input.name);
      info->inputs[i].name[31] = 0;
      info->inputs[i].hash = tveng_build_hash(info->inputs[i].name);
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

  input_collisions(info);

  if (i) /* If there is any input present, switch to the first one */
    {
      input.index = 0;
      if (IOCTL(info->fd, VIDIOC_S_INPUT, &input) != 0)
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
static
int tvengbktr_set_input(struct tveng_enum_input * input,
		     tveng_device_info * info)
{
  enum tveng_capture_mode current_mode;
  enum tveng_frame_pixformat pixformat;
  struct v4l2_input new_input;
  int retcode;

  t_assert(info != NULL);
  t_assert(input != NULL);

  pixformat = info->format.pixformat;

  current_mode = tveng_stop_everything(info);

      new_input.index = input->id;
      if ((retcode = IOCTL(info->fd, VIDIOC_S_INPUT, &new_input)) != 0)
	{
	  info -> tveng_errno = errno;
	  t_error("VIDIOC_S_INPUT", info);
	}

  info->format.pixformat = pixformat;
  tveng_set_capture_format(info);

  /* Start capturing again as if nothing had happened */
  tveng_restart_everything(current_mode, info);

  info->cur_input = input->index;

  /* Maybe there are some other standards, get'em again */
  tvengbktr_get_standards(info);

  return retcode;
}

// XXX compare against bttv for safety
static tv_videostd_id
video_standard_quiz		(struct v4l2_standard *	std)
{
  if (std->framelines == 625) {
    switch (std->colorstandard) {
    case V4L2_COLOR_STD_PAL:
      switch (std->colorstandard_data.pal.colorsubcarrier) {
      case V4L2_COLOR_SUBC_PAL:
	return TV_VIDEOSTD_PAL; /* BGDKHI */
      case V4L2_COLOR_SUBC_PAL_N:
	return TV_VIDEOSTD_PAL_N;
      default:
	return TV_VIDEOSTD_UNKNOWN;
      }
    case V4L2_COLOR_STD_NTSC:
      return TV_VIDEOSTD_UNKNOWN; /* ? */
    case V4L2_COLOR_STD_SECAM:
      return TV_VIDEOSTD_SECAM; /* BDGHKK1L, although probably L */
    }
  } else if (std->framelines == 525) {
    switch (std->colorstandard) {
    case V4L2_COLOR_STD_PAL:
      switch (std->colorstandard_data.pal.colorsubcarrier) {
      case V4L2_COLOR_SUBC_PAL_M:
	return TV_VIDEOSTD_PAL_M;
      default:
	break; /* "PAL-60" */
      }
    case V4L2_COLOR_STD_NTSC:
      return TV_VIDEOSTD_NTSC; /* M/NTSC USA or Japan */
    }
  }

  return TV_VIDEOSTD_UNKNOWN;
}

/*
  Returns the number of standards in the given device and fills in info
  with the correct info, allocating memory as needed
*/
static
int tvengbktr_get_standards(tveng_device_info * info)
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
      if (IOCTL(info->fd, VIDIOC_ENUMSTD, &enumstd) != 0)
	break;

      /* Check that this standard is supported by the current input */
      if ((enumstd.inputs & (1 << info->cur_input)) == 0)
	continue; /* Unsupported by the current input */

      info->standards = realloc(info->standards,
				(count+1)*sizeof(struct tveng_enumstd));

      info->standards[count].stdid = video_standard_quiz (&enumstd.std);
      info->standards[count].index = count;
      info->standards[count].id = i;
      snprintf(info->standards[count].name, 32, enumstd.std.name);
      info->standards[count].name[31] = 0; /* not needed, std.name < 24 */
      info->standards[count].hash =
	tveng_build_hash(info->standards[count].name);

      info -> standards[count].height = enumstd.std.framelines;
      /* unreliable, a driver may report a 16:9 etc standard (api miss?) */
      info -> standards[count].width = (enumstd.std.framelines * 4) / 3;
      /* eg. 30000 / 1001 */
      info -> standards[count].frame_rate =
	enumstd.std.framerate.denominator
	/ (double) enumstd.std.framerate.numerator;

/* Only a label, contents by definition unknown.
      if (strstr(enumstd.std.name, "ntsc") ||
	  strstr(enumstd.std.name, "NTSC") ||
	  strstr(enumstd.std.name, "Ntsc"))
	{
	  info -> standards[count].width = 640;
	  info -> standards[count].height = 480;
	}
      else
	{
	  info -> standards[count].width = 768;
	  info -> standards[count].height = 576;
	}
*/      
      count++;
    }

  standard_collisions(info);

  info -> num_standards = count;

  if (info->num_standards == 0)
    return 0; /* We are done, avoid the rest */

  /* Get the current standard */
  memset(&std, 0, sizeof(struct v4l2_standard));
  if (IOCTL(info->fd, VIDIOC_G_STD, &std) != 0)
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
    tvengbktr_set_standard(&(info->standards[0]), info);
  
  return (info->num_standards);
}

/*
  Sets the current standard for the capture. standard is the name for
  the desired standard. updates cur_standard
*/
static
int tvengbktr_set_standard(struct tveng_enumstd * std, tveng_device_info * info)
{
  enum tveng_capture_mode current_mode;
  enum tveng_frame_pixformat pixformat;
  struct v4l2_enumstd enumstd;
  int retcode;

  t_assert(info != NULL);
  t_assert(std != NULL);

  pixformat = info->format.pixformat;

  current_mode = tveng_stop_everything(info);

  /* Get info about the standard we are going to set */
  memset(&enumstd, 0, sizeof(struct v4l2_enumstd));
  enumstd.index = std -> id;
  if (IOCTL(info->fd, VIDIOC_ENUMSTD, &enumstd) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_ENUMSTD", info);
      tveng_restart_everything(current_mode, info);
      return -1;
    }

  /* Now set it */
  if ((retcode = IOCTL(info->fd, VIDIOC_S_STD, &(enumstd.std))) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_S_STD", info);
    }
  
  info->cur_standard = std->index;

  /* Start capturing again as if nothing had happened */
  info->format.pixformat = pixformat;
  tveng_set_capture_format(info);

  tveng_restart_everything(current_mode, info);

  return retcode;
}

static int
tvengbktr_update_capture_format(tveng_device_info * info)
{
  struct v4l2_format format;
  struct v4l2_window window;

  t_assert(info != NULL);

  memset(&format, 0, sizeof(struct v4l2_format));
  memset(&window, 0, sizeof(struct v4l2_window));

  format.type = V4L2_BUF_TYPE_CAPTURE;
  if (IOCTL(info->fd, VIDIOC_G_FMT, &format) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_G_FMT", info);
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
		  "Cannot understand the actual palette", info);

      return -1;    
    };
  /* mhs: moved down here because tvengbktr_read_frame blamed
     info -> format.sizeimage != size after G_WIN failed */
  if (IOCTL(info->fd, VIDIOC_G_WIN, &window) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_G_WIN", info);
      return -1;
    }
  info->window.x = window.x;
  info->window.y = window.y;
  info->window.width = window.width;
  info->window.height = window.height;
  /* These two are defined as read-only */
  info->window.clipcount = 0;
  info->window.clips = NULL;
  return 0;
}

/* -1 if failed. Sets the pixformat and fills in info -> pix_format
   with the correct values  */
static int
tvengbktr_set_capture_format(tveng_device_info * info)
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
      t_error_msg("switch()", "Cannot understand the given palette",
		  info);
      return -1;
    }

  /* Adjust the given dimensions */
  if (info->format.height < info->caps.minheight)
    info->format.height = info->caps.minheight;
  if (info->format.height > info->caps.maxheight)
    info->format.height = info->caps.maxheight;
  if (info->format.width < info->caps.minwidth)
    info->format.width = info->caps.minwidth;
  if (info->format.width > info->caps.maxwidth)
    info->format.width = info->caps.maxwidth;
  
  format.fmt.pix.width = info->format.width;
  format.fmt.pix.height = info->format.height;
  format.fmt.pix.bytesperline = ((format.fmt.pix.depth+7)>>3) *
    format.fmt.pix.width;
  format.fmt.pix.sizeimage =
    format.fmt.pix.bytesperline * format.fmt.pix.height;
  format.fmt.pix.flags = V4L2_FMT_FLAG_INTERLACED;

  /* everything is set up */
  if (IOCTL(info->fd, VIDIOC_S_FMT, &format) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_S_FMT", info);
      return -1;
    }

  /* Check fill in info with the current values (may not be the ones
     requested) */
  tvengbktr_update_capture_format(info);

  return 0; /* Success */
}

/*
 *  Controls
 */

/* This shouldn't be neccessary if control querying worked properly */
/* FIXME: add audio subchannels selecting controls */
static const struct {
	unsigned int		cid;
	const char *		label;
	tv_control_id		tcid;
} control_meta [] = {
	{ V4L2_CID_BRIGHTNESS,		N_("Brightness"),	TV_CONTROL_ID_BRIGHTNESS },
	{ V4L2_CID_CONTRAST,		N_("Contrast"),		TV_CONTROL_ID_CONTRAST },
	{ V4L2_CID_SATURATION,		N_("Saturation"),	TV_CONTROL_ID_SATURATION },
	{ V4L2_CID_HUE,			N_("Hue"),		TV_CONTROL_ID_HUE },
	{ V4L2_CID_WHITENESS,		N_("Whiteness"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_BLACK_LEVEL,		N_("Black level"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUTO_WHITE_BALANCE,	N_("White balance"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_DO_WHITE_BALANCE,	N_("White balance"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_RED_BALANCE,		N_("Red balance"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_BLUE_BALANCE,	N_("Blue balance"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_GAMMA,		N_("Gamma"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_EXPOSURE,		N_("Exposure"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUTOGAIN,		N_("Auto gain"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_GAIN,		N_("Gain"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_HCENTER,		N_("HCenter"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_VCENTER,		N_("VCenter"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_HFLIP,		N_("Hor. flipping"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_VFLIP,		N_("Vert. flipping"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUDIO_VOLUME,	N_("Volume"),		TV_CONTROL_ID_VOLUME },
	{ V4L2_CID_AUDIO_MUTE,		N_("Mute"),		TV_CONTROL_ID_MUTE },
	{ V4L2_CID_AUDIO_MUTE,		N_("Audio Mute"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUDIO_BALANCE,	N_("Balance"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUDIO_BALANCE,	N_("Audio Balance"),	TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUDIO_TREBLE,	N_("Treble"),		TV_CONTROL_ID_TREBLE },
	{ V4L2_CID_AUDIO_LOUDNESS, 	N_("Loudness"),		TV_CONTROL_ID_UNKNOWN },
	{ V4L2_CID_AUDIO_BASS,		N_("Bass"),		TV_CONTROL_ID_BASS },
};

static const unsigned int control_meta_size = sizeof (control_meta) / sizeof (*control_meta);

/* Preliminary */
#define TVENGBKTR_AUDIO_MAGIC 0x1234

static const char *
audio_decoding_modes [] = {
	N_("Automatic"),
	N_("Mono"),
	N_("Stereo"),
	N_("Alternate 1"),
	N_("Alternate 2"),
};

static tv_bool
update_control			(struct private_tvengbktr_device_info *p_info,
				 struct control *	c)
{
	struct v4l2_control ctrl;

	if (c->id == TVENGBKTR_AUDIO_MAGIC) {
		/* FIXME actual value */
		c->dev.pub.value = p_info->audio_mode;
		return TRUE;
	} else if (c->id == V4L2_CID_AUDIO_MUTE) {
		/*
		  Doesn't seem to work with bttv.
		  FIXME we should check at runtime.
		*/
		return TRUE;
	}

	ctrl.id = c->id;

	if (-1 == IOCTL (p_info->info.fd, VIDIOC_G_CTRL, &ctrl)) {
		p_info->info.tveng_errno = errno;
		t_error("VIDIOC_G_CTRL", &p_info->info);
		return FALSE;
	}

	if (c->dev.pub.value != ctrl.value) {
		c->dev.pub.value = ctrl.value;
		tv_callback_notify (&c->dev.pub, c->dev.callback);
	}

	return TRUE;
}

static tv_bool
tvengbktr_update_control		(tveng_device_info *	info,
				 tv_dev_control *	tdc)
{
	struct private_tvengbktr_device_info * p_info = P_INFO (info);
	tv_control *tc;

	if (tdc)
		return update_control (p_info, C(tdc));

	for (tc = p_info->info.controls; tc; tc = tc->next)
		if (CC(tc)->dev.device == info)
			if (!update_control (p_info, CC(tc)))
				return FALSE;

	return TRUE;
}

static tv_bool
tvengbktr_set_control		(tveng_device_info *	info,
				 tv_dev_control *	tdc,
				 int			value)
{
	struct private_tvengbktr_device_info * p_info = P_INFO (info);
	struct v4l2_control ctrl;

	if (C(tdc)->id == TVENGBKTR_AUDIO_MAGIC) {
		if (p_info->info.inputs[p_info->info.cur_input].tuners > 0) {
			struct v4l2_tuner tuner;

			memset(&tuner, 0, sizeof(tuner));
			tuner.input = p_info->info.inputs
				[p_info->info.cur_input].id;

			/* XXX */
			if (0 == IOCTL(info -> fd, VIDIOC_G_TUNER, &tuner)) {
				tuner.audmode = "\0\1\3\2"[value];

				if (0 == IOCTL(info -> fd, VIDIOC_S_TUNER, &tuner)) {
					p_info->audio_mode = value;
				}
			}
		} else {
			p_info->audio_mode = 0; /* mono */
		}

		return TRUE;
	}

	ctrl.id = C(tdc)->id;
	ctrl.value = value;

	if (-1 == IOCTL(p_info->info.fd, VIDIOC_S_CTRL, &ctrl)) {
		p_info->info.tveng_errno = errno;
		t_error("VIDIOC_S_CTRL", &p_info->info);
		return FALSE;
	}

	/*
	  Doesn't seem to work with bttv.
	  FIXME we should check at runtime.
	*/
	if (ctrl.id == V4L2_CID_AUDIO_MUTE) {
		if (tdc->pub.value != value) {
			tdc->pub.value = value;
			tv_callback_notify (&tdc->pub, tdc->callback);
		}
		return TRUE;
	}

	return update_control (p_info, C(tdc));
}

static tv_bool
add_control			(tveng_device_info *	info,
				 unsigned int		id)
{
	struct private_tvengbktr_device_info * p_info = P_INFO (info);
	struct v4l2_queryctrl qc;
	struct v4l2_querymenu qm;
	struct control *c;
	tv_control *tc;
	unsigned int i, j;

	CLEAR (qc);

	qc.id = id;

	/* XXX */
	if (-1 == IOCTL (info->fd, VIDIOC_QUERYCTRL, &qc))
		return FALSE;

	if (qc.flags & (V4L2_CTRL_FLAG_DISABLED | V4L2_CTRL_FLAG_GRABBED))
		return TRUE;

	if (!(c = calloc (1, sizeof (*c))))
		goto failure;

	for (i = 0; i < control_meta_size; i++)
		if (qc.id == control_meta[i].cid)
			break;

	if (i < control_meta_size) {
		c->dev.pub.label = strdup (_(control_meta[i].label));
		c->dev.pub.id = control_meta[i].tcid;
	} else {
		c->dev.pub.label = strndup (qc.name, 32);
		c->dev.pub.id = TV_CONTROL_ID_UNKNOWN;
	}

	if (!c->dev.pub.label)
		goto failure;

	c->dev.pub.minimum = qc.minimum;
	c->dev.pub.maximum = qc.maximum;
	c->dev.pub.reset = qc.default_value;
	c->id = qc.id;
	c->dev.device = info;
	c->dev.pub.menu = NULL;

	switch (qc.type) {
	case V4L2_CTRL_TYPE_INTEGER:	c->dev.pub.type = TV_CONTROL_TYPE_INTEGER; break;
	case V4L2_CTRL_TYPE_BOOLEAN:	c->dev.pub.type = TV_CONTROL_TYPE_BOOLEAN; break;
	case V4L2_CTRL_TYPE_BUTTON:	c->dev.pub.type = TV_CONTROL_TYPE_ACTION; break;

	case V4L2_CTRL_TYPE_MENU:
		c->dev.pub.type = TV_CONTROL_TYPE_CHOICE;

		if (!(c->dev.pub.menu = calloc (qc.maximum + 2, sizeof (char *))))
			goto failure;

		for (j = 0; j <= qc.maximum; j++) {
			qm.id = qc.id;
			qm.index = j;

			if (0 == IOCTL (info->fd, VIDIOC_QUERYMENU, &qm)) {
				if (!(c->dev.pub.menu[j] = strndup(_(qm.name), 32)))
					goto failure;
			} else {
				goto failure;
			}
		}

	      break;

	default:
		fprintf(stderr, "V4L2: Unknown control type 0x%x (%s)\n", qc.type, qc.name);
		goto failure;
	}

	if (!(tc = append_control (info, &c->dev.pub, 0))) {
	failure:
		free_control (&c->dev.pub);
		return TRUE; /* no control, but not end of enum */
	}

	update_control (p_info, CC(tc));

	if (qc.id == V4L2_CID_AUDIO_MUTE) {
		p_info->mute = tc;
	}

	return TRUE;
}

/* Private, builds the controls structure */
static int
p_tvengbktr_build_controls(tveng_device_info * info)
{
	unsigned int cid;

	for (cid = V4L2_CID_BASE;
	     cid < V4L2_CID_LASTP1; cid++) {
		/* EINVAL ignored */
		add_control (info, cid);
	}

	/* Artificial control (preliminary) */

	/* Check that there are tuners in the current input */
	if (info->inputs[info->cur_input].tuners > 0) {
		struct v4l2_tuner tuner;

		memset(&tuner, 0, sizeof(tuner));
		tuner.input = info->inputs[info->cur_input].id;

		if (IOCTL(info->fd, VIDIOC_G_TUNER, &tuner) == 0) {
			/* NB this is not implemented in bttv 0.8.45 */
			if (tuner.capability & (V4L2_TUNER_CAP_STEREO
						| V4L2_TUNER_CAP_LANG2)) {
				struct control *c;
				unsigned int i;

				/* XXX this needs refinement */

				if (!(c = calloc (1, sizeof (*c))))
					goto failure;

				if (!(c->dev.pub.label = strdup (_("Audio"))))
					goto failure;

				c->id = TVENGBKTR_AUDIO_MAGIC;
				c->dev.pub.type = TV_CONTROL_TYPE_CHOICE;
				c->dev.pub.maximum = 4;

				if (!(c->dev.pub.menu = calloc (6, sizeof (char *))))
					goto failure;

				for (i = 0; i < 5; i++) {
					if (!(c->dev.pub.menu[i] = strdup (_(audio_decoding_modes[i]))))
						goto failure;
				}

				c->dev.device = info;

				if (!append_control (info, &c->dev.pub, 0)) {
				failure:
					free_control (&c->dev.pub);
				}

				update_control (P_INFO(info), c);
			}
		}
	}

	for (cid = V4L2_CID_PRIVATE_BASE;
	     cid < V4L2_CID_PRIVATE_BASE + 100; cid++) {
		if (!add_control (info, cid))
			break; /* end of enumeration */
	}

	return 0;
}

/*
  Tunes the current input to the given freq. Returns -1 on error.
*/
static int
tvengbktr_tune_input(uint32_t _freq, tveng_device_info * info)
{
  struct v4l2_tuner tuner_info;
  uint32_t freq; /* real frequence passed to v4l2 */

  t_assert(info != NULL);
  t_assert(info->cur_input < info->num_inputs);
  t_assert(info->cur_input >= 0);

  /* Check that there are tuners in the current input */
  if (info->inputs[info->cur_input].tuners == 0)
    return 0; /* Success (we shouldn't be tuning, anyway) */

  /* Get more info about this tuner */
  tuner_info.input = info->inputs[info->cur_input].id;
  if (IOCTL(info -> fd, VIDIOC_G_TUNER, &tuner_info) != 0)
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
  if (IOCTL(info -> fd, VIDIOC_S_FREQ, &freq) != 0)
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
static int
tvengbktr_get_signal_strength (int *strength, int * afc,
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
  if (IOCTL(info -> fd, VIDIOC_G_TUNER, &tuner) != 0)
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
static int
tvengbktr_get_tune(uint32_t * freq, tveng_device_info * info)
{
  uint32_t real_freq;
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
		  "There are no tuners for the active input",
		  info);
      return -1;
    }

  if (IOCTL(info->fd, VIDIOC_G_FREQ, &real_freq) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_G_FREQ", info);
      return -1;
    }
  /* Get more info about this tuner */
  tuner.input = info->inputs[info->cur_input].id;
  if (IOCTL(info -> fd, VIDIOC_G_TUNER, &tuner) != 0)
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
static int
tvengbktr_get_tuner_bounds(uint32_t * min, uint32_t * max, tveng_device_info *
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
  if (IOCTL(info -> fd, VIDIOC_G_TUNER, &tuner) != 0)
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
/* Queues an specific buffer. -1 on error */
static int p_tvengbktr_qbuf(int index, tveng_device_info * info)
{
  struct v4l2_buffer tmp_buffer;
  struct private_tvengbktr_device_info * p_info =
    (struct private_tvengbktr_device_info*) info;

  t_assert(info != NULL);

  tmp_buffer.type = p_info -> buffers[0].vidbuf.type;
  tmp_buffer.index = index;

  if (IOCTL(info->fd, VIDIOC_QBUF, &tmp_buffer) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_QBUF", info);
      return -1;
    }

  return 0;
}

/* dequeues next available buffer and returns it's id. -1 on error */
static int p_tvengbktr_dqbuf(tveng_device_info * info)
{
  struct v4l2_buffer tmp_buffer;
  struct private_tvengbktr_device_info * p_info =
    (struct private_tvengbktr_device_info*) info;
  
  t_assert(info != NULL);

  tmp_buffer.type = p_info -> buffers[0].vidbuf.type;

  if (IOCTL(info->fd, VIDIOC_DQBUF, &tmp_buffer) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_DQBUF", info);
      return -1;
    }

  p_info -> last_timestamp = tmp_buffer.timestamp /1e9;

  return (tmp_buffer.index);
}

/*
  Sets up the capture device so any read() call after this one
  succeeds. Returns -1 on error.
*/
static int
tvengbktr_start_capturing(tveng_device_info * info)
{
  struct v4l2_requestbuffers rb;
  struct private_tvengbktr_device_info * p_info =
    (struct private_tvengbktr_device_info*) info;
  int i;

  t_assert(info != NULL);

  tveng_stop_everything(info);

  t_assert(info -> current_mode == TVENG_NO_CAPTURE);
  t_assert(p_info->num_buffers == 0);
  t_assert(p_info->buffers == NULL);

  p_info -> buffers = NULL;
  p_info -> num_buffers = 0;

  rb.count = 8; /* This is a good number(tm) */
  rb.type = V4L2_BUF_TYPE_CAPTURE;
  if (IOCTL(info->fd, VIDIOC_REQBUFS, &rb) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_REQBUFS", info);
      return -1;
    }

  if (rb.count <= 2)
    {
      info->tveng_errno = -1;
      t_error_msg("check()", "Not enough buffers", info);
      return -1;
    }

  p_info -> buffers = (struct tvengbktr_vbuf*)
    malloc(rb.count*sizeof(struct tvengbktr_vbuf));
  p_info -> num_buffers = rb.count;

  for (i = 0; i < rb.count; i++)
    {
      p_info -> buffers[i].vidbuf.index = i;
      p_info -> buffers[i].vidbuf.type = V4L2_BUF_TYPE_CAPTURE;
      if (IOCTL(info->fd, VIDIOC_QUERYBUF,
		&(p_info->buffers[i].vidbuf)) != 0)
	{
	  info->tveng_errno = errno;
	  t_error("VIDIOC_QUERYBUF", info);
	  return -1;
	}

      /* bttv 0.8.x wants PROT_WRITE although AFAIK we don't. */
      p_info->buffers[i].vmem =
	mmap (0, p_info->buffers[i].vidbuf.length,
	      PROT_READ | PROT_WRITE,
	      MAP_SHARED, info->fd,
	      p_info->buffers[i].vidbuf.offset);

      if (p_info->buffers[i].vmem == (void *) -1)
	p_info->buffers[i].vmem =
	  mmap(0, p_info->buffers[i].vidbuf.length,
	       PROT_READ, MAP_SHARED, info->fd, 
	       p_info->buffers[i].vidbuf.offset);

      if (p_info->buffers[i].vmem == (void *) -1)
	{
	  info->tveng_errno = errno;
	  t_error("mmap()", info);
	  return -1;
	}

	/* Queue the buffer */
      if (p_tvengbktr_qbuf(i, info) == -1)
	return -1;
    }

  /* Turn on streaming */
  i = V4L2_BUF_TYPE_CAPTURE;
  if (IOCTL(info->fd, VIDIOC_STREAMON, &i) != 0)
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
static int
tvengbktr_stop_capturing(tveng_device_info * info)
{
  struct private_tvengbktr_device_info * p_info =
    (struct private_tvengbktr_device_info*) info;
  int i;

  if (info -> current_mode == TVENG_NO_CAPTURE)
    {
      fprintf(stderr,
	      "Warning: trying to stop capture with no capture active\n");
      return 0; /* Nothing to be done */
    }

  t_assert(info->current_mode == TVENG_CAPTURE_READ);

  /* Turn streaming off */
  i = V4L2_BUF_TYPE_CAPTURE;
  if (IOCTL(info->fd, VIDIOC_STREAMOFF, &i) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_STREAMOFF", info);
      /* No critical error, go on munmapping */
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
    {
      free(p_info -> buffers);
      p_info->buffers = NULL;
    }
  p_info->num_buffers = 0;

  p_info -> last_timestamp = -1;

  info->current_mode = TVENG_NO_CAPTURE;

  return 0;
}

/* 
   Reads a frame from the video device, storing the read data in
   the location pointed to by where.
   time: time to wait using select() in miliseconds
   info: pointer to the video device info structure
   Returns -1 on error, anything else on success
*/
static
int tvengbktr_read_frame(tveng_image_data * where, 
		      unsigned int time, tveng_device_info * info)
{
  struct private_tvengbktr_device_info * p_info =
    (struct private_tvengbktr_device_info*) info;
  int n; /* The dequeued buffer */
  fd_set rdset;
  struct timeval timeout;

  if (info -> current_mode != TVENG_CAPTURE_READ)
    {
      info -> tveng_errno = -1;
      t_error_msg("check", "Current capture mode is not READ (%d)",
		  info, info->current_mode);
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

  n = p_tvengbktr_dqbuf(info);
  if (n == -1)
    return -1;

  /* Ignore frames we haven't been able to process */
  do{
    FD_ZERO(&rdset);
    FD_SET(info->fd, &rdset);
    timeout.tv_sec = timeout.tv_usec = 0;
    if (select(info->fd +1, &rdset, NULL, NULL, &timeout) < 1)
      break;
    p_tvengbktr_qbuf(n, info);
    n = p_tvengbktr_dqbuf(info);
  } while (1);

  /* Copy the data to the address given */
  if (where)
    tveng_copy_frame (p_info->buffers[n].vmem, where, info);

  /* Queue the buffer again for processing */
  if (p_tvengbktr_qbuf(n, info))
    return -1;

  /* Everything has been OK, return 0 (success) */
  return 0;
}

/*
  Gets the timestamp of the last read frame in seconds.
  Returns -1 on error, if the current mode isn't capture, or if we
  haven't captured any frame yet.
*/
static double tvengbktr_get_timestamp(tveng_device_info * info)
{
  struct private_tvengbktr_device_info * p_info =
    (struct private_tvengbktr_device_info *) info;

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
static
int tvengbktr_set_capture_size(int width, int height, tveng_device_info * info)
{
  enum tveng_capture_mode current_mode;
  int retcode;

  t_assert(info != NULL);
  t_assert(width > 0);
  t_assert(height > 0);

  tvengbktr_update_capture_format(info);

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
  retcode = tvengbktr_set_capture_format(info);

  /* Restart capture again */
  if (tveng_restart_everything(current_mode, info) == -1)
    retcode = -1;

  return retcode;
}

/* 
   Gets the actual size of the capture buffer in width and height.
   -1 on error
*/
static
int tvengbktr_get_capture_size(int *width, int *height, tveng_device_info * info)
{
  t_assert(info != NULL);

  if (tvengbktr_update_capture_format(info))
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
static int
tvengbktr_detect_preview (tveng_device_info * info)
{
  struct v4l2_framebuffer fb;

  t_assert(info != NULL);

  if ((info -> caps.flags & TVENG_CAPS_OVERLAY) == 0)
    {
      info -> tveng_errno = -1;
      t_error_msg("flags check",
       "The capability field says that there is no overlay", info);
      return 0;
    }

  /* Get the current framebuffer info */
  if (IOCTL(info -> fd, VIDIOC_G_FBUF, &fb) != 0)
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

  if (!tveng_detect_XF86DGA(info))
    {
      info->tveng_errno = -1;
      t_error_msg("tveng_detect_XF86DGA",
		  "No DGA present, make sure you enable it in"
		  " /etc/X11/XF86Config.",
		  info);
      return 0;
    }

  return 1;
}

/*
  Sets the preview window dimensions to the given window.
  Returns -1 on error, something else on success.
  Success doesn't mean that the requested dimensions are used, maybe
  they are different, check the returned fields to see if they are suitable
  info   : Device we are controlling
*/
static int
tvengbktr_set_preview_window(tveng_device_info * info)
{
  struct v4l2_window window;
  struct v4l2_clip * clip=NULL;
  struct private_tvengbktr_device_info * p_info =
    (struct private_tvengbktr_device_info*) info;
  int i;

  t_assert(info != NULL);
  t_assert(info-> window.clipcount >= 0);

  memset(&window, 0, sizeof(struct v4l2_window));

  window.x = info->window.x;
  window.y = info->window.y;
  window.width = info->window.width;
  window.height = info->window.height;
  window.clipcount = info->window.clipcount;
  window.chromakey = p_info->chroma;
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

  tveng_set_preview_off(info);

  /* Set the new window */
  if (IOCTL(info->fd, VIDIOC_S_WIN, &window) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_S_WIN", info);
      free(clip);
      return -1;
    }

  if (clip)
    free(clip);

  /* Update the info struct */
  return (tvengbktr_get_preview_window(info));
}

/*
  Gets the current overlay window parameters.
  Returns -1 on error, and any other value on success.
  info   : The device to use
*/
static int
tvengbktr_get_preview_window(tveng_device_info * info)
{
  /* Updates the entire capture format, since there is no
     difference */
  return (tvengbktr_update_capture_format(info));
}

/* 
   Sets the previewing on/off.
   on : if 1, set preview on, if 0 off, other values are silently ignored
   info  : device to use for previewing
   Returns -1 on error, anything else on success
*/
static int
tvengbktr_set_preview (int on, tveng_device_info * info)
{
  int one = 1, zero = 0;

  t_assert(info != NULL);

  if ((on < 0) || (on > 1))
    return 0;

  if (IOCTL(info->fd, VIDIOC_PREVIEW, on ? &one : &zero) != 0)
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
static int
tvengbktr_start_previewing (tveng_device_info * info)
{
#ifndef DISABLE_X_EXTENSIONS
  int width, height;
  unsigned int dwidth, dheight;

  tveng_stop_everything(info);

  t_assert(info -> current_mode == TVENG_NO_CAPTURE);

  if (!tvengbktr_detect_preview(info))
    /* We shouldn't be reaching this if the app is well programmed */
    t_assert_not_reached();

  if (!tveng_detect_XF86DGA(info))
    return -1;

  x11_root_geometry (&dwidth, &dheight);

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
  if (tvengbktr_set_preview_window(info) == -1)
    return -1;

  /* Center preview window (maybe the requested width and/or height)
     aren't valid */
  info->window.x = (dwidth - info->window.width)/2;
  info->window.y = (dheight - info->window.height)/2;
  info->window.clipcount = 0;
  info->window.clips = NULL;
  if (tvengbktr_set_preview_window(info) == -1)
    return -1;

  /* Start preview */
  if (tvengbktr_set_preview(1, info) == -1)
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
static int
tvengbktr_stop_previewing(tveng_device_info * info)
{
#ifndef DISABLE_X_EXTENSIONS
  if (info -> current_mode == TVENG_NO_CAPTURE)
    {
      fprintf(stderr, 
	      "Warning: trying to stop preview with no capture active\n");
      return 0; /* Nothing to be done */
    }
  t_assert(info->current_mode == TVENG_CAPTURE_PREVIEW);

  /* No error checking */
  tvengbktr_set_preview(0, info);

  info -> current_mode = TVENG_NO_CAPTURE;
  return 0; /* Success */
#else
  return 0;
#endif
}

static void
tvengbktr_set_chromakey		(uint32_t chroma, tveng_device_info *info)
{
  struct private_tvengbktr_device_info * p_info =
    (struct private_tvengbktr_device_info*) info;

  p_info->chroma = chroma;

  /* Will be set in the next set_window call */
}

static int
tvengbktr_get_chromakey		(uint32_t *chroma, tveng_device_info *info)
{
  struct private_tvengbktr_device_info * p_info =
    (struct private_tvengbktr_device_info*) info;

  *chroma = p_info->chroma;

  return 0;
}

static struct tveng_module_info tvengbktr_module_info = {
  .attach_device =		tvengbktr_attach_device,
  .describe_controller =	tvengbktr_describe_controller,
  .close_device =		tvengbktr_close_device,
  .get_inputs =			tvengbktr_get_inputs,
  .set_input =			tvengbktr_set_input,
  .get_standards =		tvengbktr_get_standards,
  .set_standard =		tvengbktr_set_standard,
  .update_capture_format =	tvengbktr_update_capture_format,
  .set_capture_format =		tvengbktr_set_capture_format,
  .update_control =		tvengbktr_update_control,
  .set_control =		tvengbktr_set_control,
  .tune_input =			tvengbktr_tune_input,
  .get_signal_strength =	tvengbktr_get_signal_strength,
  .get_tune =			tvengbktr_get_tune,
  .get_tuner_bounds =		tvengbktr_get_tuner_bounds,
  .start_capturing =		tvengbktr_start_capturing,
  .stop_capturing =		tvengbktr_stop_capturing,
  .read_frame =			tvengbktr_read_frame,
  .get_timestamp =		tvengbktr_get_timestamp,
  .set_capture_size =		tvengbktr_set_capture_size,
  .get_capture_size =		tvengbktr_get_capture_size,
  .detect_preview =		tvengbktr_detect_preview,
  .set_preview_window =		tvengbktr_set_preview_window,
  .get_preview_window =		tvengbktr_get_preview_window,
  .set_preview =		tvengbktr_set_preview,
  .start_previewing =		tvengbktr_start_previewing,
  .stop_previewing =		tvengbktr_stop_previewing,
  .get_chromakey =		tvengbktr_get_chromakey,
  .set_chromakey =		tvengbktr_set_chromakey,

  .private_size =		sizeof(struct private_tvengbktr_device_info)
};

/*
  Inits the V4L2 module, and fills in the given table.
*/
void tvengbktr_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

  memcpy(module_info, &tvengbktr_module_info,
	 sizeof(struct tveng_module_info)); 
}

#else /* !ENABLE_V4L */

#include "tvengbktr.h"

void tvengbktr_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

  memset(module_info, 0, sizeof(struct tveng_module_info)); 
}

#endif /* ENABLE_V4L */

#endif /*** TODO ***/
