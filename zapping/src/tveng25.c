/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 Iñaki García Etxebarria
 * Copyright (C) 2003 Michael H. Schimek
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
#define TVENG25_PROTOTYPES 1
#include "tveng25.h"

#include <linux/types.h> /* __u32 etc */
#include "../common/videodev25.h" /* the V4L2 definitions */
#define D() \
do { \
    fprintf(stderr, "Line %d, routine %s\n", __LINE__, __PRETTY_FUNCTION__); \
} while (0)

struct tveng25_vbuf
{
  void * vmem; /* Captured image in this buffer */
  struct v4l2_buffer vidbuf; /* Info about the buffer */
};

struct private_tveng25_device_info
{
  tveng_device_info info; /* Info field, inherited */
  int num_buffers; /* Number of mmaped buffers */
  struct tveng25_vbuf * buffers; /* Array of buffers */
  double last_timestamp; /* The timestamp of the last captured buffer */
  int muted;
};

#define CLEAR(v) memset (&(v), 0, sizeof (v))
#define IOCTL_ARG_SIZE(cmd) _IOC_SIZE (cmd)

/* to do */
static int
_v4l25_ioctl			(int			fd,
				 unsigned int		cmd,
				 void *			arg)
{
  int err;

  do err = ioctl (fd, cmd, arg);
  while (-1 == err && EINTR == errno);

  return err;
}

/* pathetic attempt of type checking */
#define v4l25_ioctl(fd, cmd, arg)					\
({									\
  extern void v4l25_ioctl_arg_mismatch (void);				\
  if (sizeof (*(arg)) != IOCTL_ARG_SIZE(cmd))				\
    v4l25_ioctl_arg_mismatch (); /* link error */			\
  _v4l25_ioctl(fd, cmd, arg);						\
})

/* Private, builds the controls structure */
static int
p_tveng25_build_controls(tveng_device_info * info);

static int p_tveng25_open_device_file(int flags, tveng_device_info * info);
/*
  Return fd for the device file opened. Checks if the device is a
  valid video device. -1 on error.
  Flags will be used for open()'ing the file 
*/
static int p_tveng25_open_device_file(int flags, tveng_device_info * info)
{
  struct v4l2_capability caps;
  struct v4l2_framebuffer fb;
  extern int disable_overlay;

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
  CLEAR (caps);
  CLEAR (fb);

  if (-1 == v4l25_ioctl (info->fd, VIDIOC_QUERYCAP, &caps))
    {
      info -> tveng_errno = errno;
      t_error("VIDIOC_QUERYCAP", info);
      close(info -> fd);
      return -1;
    }

  /* Check if this device is convenient for us */
  if (!(caps.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
      info->tveng_errno = -1;
      snprintf(info->error, 256, 
	       _("%s doesn't look like a valid capture device"), info
	       -> file_name);
      close(info -> fd);
      return -1;
    }

  /* Check if we can mmap() this device */
  /* XXX REQBUFS must be checked too */
  if (!(caps.capabilities & V4L2_CAP_STREAMING))
    {
      info -> tveng_errno = -1;
      snprintf(info->error, 256,
	       _("Sorry, but \"%s\" cannot do streaming"),
	       info -> file_name);
      close(info -> fd);
      return -1;
    }

  /* Copy capability info */
  snprintf(info->caps.name, 32, caps.card);
  /* XXX get these elsewhere */
  info->caps.channels = 0; /* video inputs */
  info->caps.audios = 0;
  info->caps.maxwidth = 768;
  info->caps.minwidth = 320;
  info->caps.maxheight = 576;
  info->caps.minheight = 240;
  info->caps.flags = 0;

  info->caps.flags |= TVENG_CAPS_CAPTURE; /* This has been tested before */

  if (caps.capabilities & V4L2_CAP_TUNER)
    info->caps.flags |= TVENG_CAPS_TUNER;
  if (caps.capabilities & V4L2_CAP_VBI_CAPTURE)
    info->caps.flags |= TVENG_CAPS_TELETEXT;
  /* XXX get elsewhere */
  if (caps.capabilities & 0)
    info->caps.flags |= TVENG_CAPS_MONOCHROME;

  if (!disable_overlay && (caps.capabilities & V4L2_CAP_VIDEO_OVERLAY))
    {
      info->caps.flags |= TVENG_CAPS_OVERLAY;

      /* Collect more info about the overlay mode */
      if (v4l25_ioctl (info->fd, VIDIOC_G_FBUF, &fb) != 0)
	{
	  if (fb.capability & V4L2_FBUF_CAP_CHROMAKEY)
	    info->caps.flags |= TVENG_CAPS_CHROMAKEY;

	  if (fb.capability & (V4L2_FBUF_CAP_LIST_CLIPPING
			       | V4L2_FBUF_CAP_BITMAP_CLIPPING))
	    info->caps.flags |= TVENG_CAPS_CLIPPING;

	  if (!(fb.capability & V4L2_FBUF_CAP_EXTERNOVERLAY))
	    info->caps.flags |= TVENG_CAPS_FRAMERAM;

	  /* XXX get elsewhere
	  if ((fb.flags & V4L2_FBUF_CAP_SCALEUP) ||
	      (fb.flags & V4L2_FBUF_CAP_SCALEDOWN))
	      info->caps.flags |= TVENG_CAPS_SCALES; */
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
int tveng25_attach_device(const char* device_file,
			 enum tveng_attach_mode attach_mode,
			 tveng_device_info * info)
{
  int error;
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;

  t_assert(device_file != NULL);
  t_assert(info != NULL);

  if (info -> fd) /* If the device is already attached, detach it */
    tveng_close_device(info);

  info->audio_mutable = 0;

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
      info -> fd = p_tveng25_open_device_file(0, info);
      break;

    case TVENG_ATTACH_READ:
      info -> fd = p_tveng25_open_device_file(O_RDWR, info);
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
  error = tveng25_get_inputs(info);
  if (error < 0)
    {
      tveng25_close_device(info);
      return -1;
    }

  /* Fill in standards */
  info->standards = NULL;
  info->cur_standard = 0;
  info->num_standards = 0;
  error = tveng25_get_standards(info);
  if (error < 0)
    {
      tveng25_close_device(info);
      return -1;
    }

  /* Query present controls */
  info->num_controls = 0;
  info->controls = NULL;
  error = p_tveng25_build_controls(info);
  if (error == -1)
      return -1;

  /* Set up the palette according to the one present in the system */
  error = info->priv->current_bpp;

  if (error == -1)
    {
      tveng25_close_device(info);
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
      tveng25_close_device(info);
      return -1;
    }

  /* Get fb_info */
  tveng25_detect_preview(info);

  /* Pass some dummy values to the driver, so g_win doesn't fail */
  CLEAR (info->window);

  info->window.width = info->window.height = 16;

  tveng_set_preview_window(info);

  /* Set our desired size, make it halfway */
  info -> format.width = (info->caps.minwidth + info->caps.maxwidth)/2;
  info -> format.height = (info->caps.minheight +
			   info->caps.maxheight)/2;

  /* Set some capture format (not important) */
  tveng25_set_capture_format(info);

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
tveng25_describe_controller(char ** short_str, char ** long_str,
			   tveng_device_info * info)
{
  t_assert(info != NULL);
  if (short_str)
    *short_str = "V4L25";
  if (long_str)
    *long_str = "Video4Linux 2.5";
}

/* Closes a device opened with tveng_init_device */
static void tveng25_close_device(tveng_device_info * info)
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
  info->num_controls = 0;
  info->num_standards = 0;
  info->num_inputs = 0;
  info->inputs = NULL;
  info->standards = NULL;
  info->controls = NULL;
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
int tveng25_get_inputs(tveng_device_info * info)
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
      CLEAR (input);
      input.index = i;

      if (-1 == v4l25_ioctl (info->fd, VIDIOC_ENUMINPUT, &input))
	break;

      info->inputs = realloc(info->inputs, (i+1)*
			     sizeof(struct tveng_enum_input));
      info->inputs[i].id = i;
      info->inputs[i].tuner_id = input.tuner;
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

      if (input.audioset != 0)
	info->inputs[i].flags |= TVENG_INPUT_AUDIO;
    }

  input_collisions(info);

  if (i) /* If there is any input present, switch to the first one */
    {
      int index = 0;

      if (v4l25_ioctl (info->fd, VIDIOC_S_INPUT, &index) != 0)
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
int tveng25_set_input(struct tveng_enum_input * input,
		     tveng_device_info * info)
{
  enum tveng_capture_mode current_mode;
  enum tveng_frame_pixformat pixformat;
  int index;
  int retcode;

  t_assert(info != NULL);
  t_assert(input != NULL);

  pixformat = info->format.pixformat;

  current_mode = tveng_stop_everything(info);

  index = input->id;
  retcode = v4l25_ioctl (info->fd, VIDIOC_S_INPUT, &index);

  if (-1 == retcode)
      {
	info -> tveng_errno = errno;
	t_error("VIDIOC_S_INPUT", info);
      }

  info->format.pixformat = pixformat;
  tveng_set_capture_format(info);

  /* Start capturing again as if nothing had happened */
  tveng_restart_everything(current_mode, info);

  info->cur_input = index;

  /* Maybe there are some other standards, get'em again */
  tveng25_get_standards(info);

  return retcode;
}

/*
  Returns the number of standards in the given device and fills in info
  with the correct info, allocating memory as needed
*/
static
int tveng25_get_standards(tveng_device_info * info)
{
  int count = 0; /* Number of available standards */
  struct v4l2_standard std;
  v4l2_std_id std_id;
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
      CLEAR (std);

      std.index = i;

      if (-1 == v4l25_ioctl (info->fd, VIDIOC_ENUMSTD, &std))
	break;

      /* Check that this standard is supported by the current input */
      /* FIXME
      if ((std.inputs & (1 << info->cur_input)) == 0)
      continue; */ /* Unsupported by the current input */

      info->standards = realloc(info->standards,
				(count+1)*sizeof(struct tveng_enumstd));
      info->standards[count].index = count;
      info->standards[count].id = i;
      info->standards[count].std_id = std.id;
      snprintf(info->standards[count].name, 32, std.name);
      info->standards[count].name[31] = 0; /* not needed, std.name < 24 */
      info->standards[count].hash =
	tveng_build_hash(info->standards[count].name);

      info -> standards[count].height = std.framelines;
      /* unreliable, a driver may report a 16:9 etc standard (api miss?) */
      /* XXX check v4l2_std_id */
      info -> standards[count].width = (std.framelines * 4) / 3;
      /* eg. 30000 / 1001 */
      info -> standards[count].frame_rate =
	std.frameperiod.denominator
	/ (double) std.frameperiod.numerator;

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
  std_id = 0;
  if (-1 == v4l25_ioctl (info->fd, VIDIOC_G_STD, &std_id))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_G_STD", info);
      return -1;
    }

  for (i=0; i<info->num_standards; i++)
    if (info->standards[i].std_id & std_id)
      {
	info->cur_standard = i;
	break;
      }

  if (i == info->num_standards) /* Current standard not found */
    /* Set the first standard as active */
    tveng25_set_standard(&(info->standards[0]), info);
  
  return (info->num_standards);
}

/*
  Sets the current standard for the capture. standard is the name for
  the desired standard. updates cur_standard
*/
static
int tveng25_set_standard(struct tveng_enumstd * std, tveng_device_info * info)
{
  enum tveng_capture_mode current_mode;
  enum tveng_frame_pixformat pixformat;
  struct v4l2_standard vstd;
  int retcode;

  t_assert(info != NULL);
  t_assert(std != NULL);

  pixformat = info->format.pixformat;

  current_mode = tveng_stop_everything(info);

  /* Get info about the standard we are going to set */
  CLEAR (vstd);
  vstd.index = std -> id;
  if (-1 == v4l25_ioctl (info->fd, VIDIOC_ENUMSTD, &vstd))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_ENUMSTD", info);
      tveng_restart_everything(current_mode, info);
      return -1;
    }

  /* Now set it */
  retcode = v4l25_ioctl (info->fd, VIDIOC_S_STD, &vstd.id);

  if (-1 == retcode)
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
tveng25_update_capture_format(tveng_device_info * info)
{
  struct v4l2_format format;

  t_assert(info != NULL);

  CLEAR (format);

  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (-1 == v4l25_ioctl (info->fd, VIDIOC_G_FMT, &format))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_G_FMT", info);
      return -1;
    }

  info->format.width = format.fmt.pix.width;
  info->format.height = format.fmt.pix.height;
  info->format.bytesperline = format.fmt.pix.bytesperline;
  info->format.sizeimage = format.fmt.pix.sizeimage;

  switch (format.fmt.pix.pixelformat)
    {
    case V4L2_PIX_FMT_RGB555:
      info->format.depth = 15;
      info->format.bpp = 2;
      info->format.pixformat = TVENG_PIX_RGB555;
      break;
    case V4L2_PIX_FMT_RGB565:
      info->format.depth = 16;
      info->format.bpp = 2;
      info->format.pixformat = TVENG_PIX_RGB565;
      break;
    case V4L2_PIX_FMT_RGB24:
      info->format.depth = 24;
      info->format.bpp = 3;
      info->format.pixformat = TVENG_PIX_RGB24;
      break;
    case V4L2_PIX_FMT_BGR24:
      info->format.depth = 24;
      info->format.bpp = 3;
      info->format.pixformat = TVENG_PIX_BGR24;
      break;
    case V4L2_PIX_FMT_RGB32:
      info->format.depth = 32;
      info->format.bpp = 4;
      info->format.pixformat = TVENG_PIX_RGB32;
      break;
    case V4L2_PIX_FMT_BGR32:
      info->format.depth = 32;
      info->format.bpp = 4;
      info->format.pixformat = TVENG_PIX_BGR32;
      break;
    case V4L2_PIX_FMT_YVU420:
      info->format.depth = 12;
      info->format.bpp = 1.5;
      info->format.pixformat = TVENG_PIX_YVU420;
      break;
#ifdef V4L2_PIX_FMT_YUV420 /* not in the spec, but very common */
    case V4L2_PIX_FMT_YUV420:
      info->format.depth = 12;
      info->format.bpp = 1.5;
      info->format.pixformat = TVENG_PIX_YUV420;
      break;
#endif
    case V4L2_PIX_FMT_UYVY:
      info->format.depth = 16;
      info->format.bpp = 2;
      info->format.pixformat = TVENG_PIX_UYVY;
      break;
    case V4L2_PIX_FMT_YUYV:
      info->format.depth = 16;
      info->format.bpp = 2;
      info->format.pixformat = TVENG_PIX_YUYV;
      break;
    case V4L2_PIX_FMT_GREY:
      info->format.depth = 8;
      info->format.bpp = 1;
      info->format.pixformat = TVENG_PIX_GREY;
      break;
    default:
      info->tveng_errno = -1; /* unknown */
      t_error_msg("switch()",
		  "Cannot understand the actual palette", info);
      return -1;    
    };

  info->format.width = format.fmt.pix.width;
  info->format.height = format.fmt.pix.height;

  /* bttv */
  format.fmt.pix.bytesperline =
    MAX (format.fmt.pix.width * info->format.bpp,
	 format.fmt.pix.bytesperline);

  info->format.bytesperline = format.fmt.pix.bytesperline;

  info->format.sizeimage = format.fmt.pix.sizeimage;

  CLEAR (format);

  format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;

  /* mhs: moved down here because tveng25_read_frame blamed
     info -> format.sizeimage != size after G_WIN failed */
  if (-1 == v4l25_ioctl (info->fd, VIDIOC_G_FMT, &format))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_G_WIN", info);
      return -1;
    }

  info->window.x	= format.fmt.win.w.left;
  info->window.y	= format.fmt.win.w.top;
  info->window.width	= format.fmt.win.w.width;
  info->window.height	= format.fmt.win.w.height;
  /* These two are defined as write-only */
  info->window.clipcount = 0;
  info->window.clips = NULL;
  return 0;
}

/* -1 if failed. Sets the pixformat and fills in info -> pix_format
   with the correct values  */
static int
tveng25_set_capture_format(tveng_device_info * info)
{
  struct v4l2_format format;

  t_assert(info != NULL);

  CLEAR (format);

  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  /* Transform the given palette value into a V4L value */
  switch(info->format.pixformat)
    {
    case TVENG_PIX_RGB555:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB555;
      break;
    case TVENG_PIX_RGB565:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;
      break;
    case TVENG_PIX_RGB24:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
      break;
    case TVENG_PIX_BGR24:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
      break;
    case TVENG_PIX_RGB32:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB32;
      break;
    case TVENG_PIX_BGR32:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR32;
      break;
    case TVENG_PIX_YUV420:
#ifdef V4L2_PIX_FMT_YUV420 /* not in the spec, but very common */
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
      break;
#endif
    case TVENG_PIX_YVU420:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_YVU420;
      break;
    case TVENG_PIX_UYVY:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
      break;
    case TVENG_PIX_YUYV:
      format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
      break;
    default:
      info->tveng_errno = -1; /* unknown */
      t_error_msg("switch()", "Cannot understand the given palette",
		  info);
      return -1;
    }

  /* Adjust the given dimensions */
  /* FIXME
  if (info->format.height < info->caps.minheight)
    info->format.height = info->caps.minheight;

  if (info->format.height > info->caps.maxheight)
    info->format.height = info->caps.maxheight;

  if (info->format.width < info->caps.minwidth)
    info->format.width = info->caps.minwidth;

  if (info->format.width > info->caps.maxwidth)
    info->format.width = info->caps.maxwidth;
  */
  
  format.fmt.pix.width = info->format.width;
  format.fmt.pix.height = info->format.height;

  format.fmt.pix.bytesperline = 0; /* minimum please */
  format.fmt.pix.sizeimage = 0; /* ditto */

  /* XXX */
  if (format.fmt.pix.height <= 288)
    format.fmt.pix.field = V4L2_FIELD_TOP;
  else
    format.fmt.pix.field = V4L2_FIELD_INTERLACED;

  if (-1 == v4l25_ioctl (info->fd, VIDIOC_S_FMT, &format))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_S_FMT", info);
      return -1;
    }

  /* Check fill in info with the current values (may not be the ones
     requested) */
  tveng25_update_capture_format(info);

  return 0; /* Success */
}

/* To aid i18n, possible label isn't actually used */
struct p_tveng25_control_with_i18n
{
  uint32_t cid;
  char * possible_label;
};

/* This shouldn't be neccessary is control querying worked properly */
/* FIXME: add audio subchannels selecting controls */
static struct p_tveng25_control_with_i18n cids[] =
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

/* Private, builds the controls structure */
static int
p_tveng25_build_controls(tveng_device_info * info)
{
  struct v4l2_queryctrl qc;
  struct v4l2_querymenu qm;
  struct tveng_control control;
  int i;
  int j;
  int p;
  int start=V4L2_CID_BASE, end=V4L2_CID_LASTP1;

  CLEAR (qc);
  
 build_controls:
  for (p = start; p < end; p++)
    {
      qc.id = p;

      if (0 == v4l25_ioctl (info->fd, VIDIOC_QUERYCTRL, &qc)
	  && !(qc.flags & (V4L2_CTRL_FLAG_DISABLED | V4L2_CTRL_FLAG_GRABBED)))
	{
	  snprintf(control.name, 32, qc.name);
	  /* search for possible translations */
	  for (i=0;
	       i<sizeof(cids)/sizeof(struct p_tveng25_control_with_i18n);
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
	  control.def_value = qc.default_value;
	  control.id = qc.id;
	  control.controller = TVENG_CONTROLLER_V4L2;

	  if (qc.id == V4L2_CID_AUDIO_MUTE)
	    info->audio_mutable = 1;

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

		  if (-1 == v4l25_ioctl (info->fd, VIDIOC_QUERYMENU, &qm))
		    {
		      control.data[j] =
			strdup("<Broken menu entry>");
		    }
		  else
		    control.data[j] = strdup(_(qm.name));
		}
	      control.data = realloc(control.data,
				     (j+1)*sizeof(char*));
	      control.data[j] = NULL;
	      break;
	    default:
	      fprintf(stderr,
		      "V4L2: Unknown control type 0x%x (%s)\n",
		      qc.type, qc.name);
	      continue; /* Skip this one */
	    }
	  if (p_tveng_append_control(&control, info) == -1)
	    return -1;
	}
    }

  /* Build now the private controls */
  if (start == V4L2_CID_BASE)
    {
      start = V4L2_CID_PRIVATE_BASE;
      end = start+100;
      goto build_controls;
    }

  return tveng25_update_controls(info);
}

/*
  Gets the current value of the controls, fills in info->controls
  appropiately. After this (and if it succeeds) you can look in
  info->controls to get the values for each control. -1 on error
*/
static int
tveng25_update_controls(tveng_device_info * info)
{
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;
  int i;
  struct v4l2_control c;

  t_assert(info != NULL);
  t_assert(info->num_controls>0);
  t_assert(info->controls != NULL);

  for (i=0; i<info->num_controls; i++)
    {
      c.id = info->controls[i].id;

      if (info->controls[i].controller != TVENG_CONTROLLER_V4L2)
	continue; /* somebody else created this control */

      if (-1 == v4l25_ioctl (info->fd, VIDIOC_G_CTRL, &c))
	{
	  info->tveng_errno = errno;
	  t_error("VIDIOC_G_CTRL", info);
	  c.value = 0; /* This shouldn't be critical */
	}

/*
  Doesn't seem to work with bttv.
  FIXME we should check at runtime.
*/
      if (info->controls[i].id == V4L2_CID_AUDIO_MUTE)
	info->controls[i].cur_value = p_info->muted;
      else
	info->controls[i].cur_value = c.value;
    }
  return 0;
}

/*
  Sets the value for an specific control. The given value will be
  clipped between min and max values. Returns -1 on error
*/
static int
tveng25_set_control(struct tveng_control * control, int value,
		   tveng_device_info * info)
{
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;
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

  if (-1 == v4l25_ioctl (info->fd, VIDIOC_S_CTRL, &c))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_S_CTRL", info);
      return -1;
    }
/*
  Doesn't seem to work with bttv.
  FIXME we should check at runtime.
*/
  if (control->id == V4L2_CID_AUDIO_MUTE)
    p_info->muted = value;

  return (tveng25_update_controls(info));
}

/*
  Gets the value of the mute property. 1 means mute (no sound) and 0
  unmute (sound). -1 on error
*/
static int
tveng25_get_mute(tveng_device_info * info)
{
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;

  return p_info->muted;

/*
  Doesn't seem to work with bttv.
  FIXME we should check at runtime.

  int returned_value;
  if (tveng_get_control_by_id(V4L2_CID_AUDIO_MUTE, &returned_value, info) ==
      -1)
    return -1;
  return !!returned_value;
*/
}

/*
  Sets the value of the mute property. 0 means unmute (sound) and 1
  mute (no sound). -1 on error
*/
static int
tveng25_set_mute(int value, tveng_device_info * info)
{
  if (tveng_set_control_by_id(V4L2_CID_AUDIO_MUTE, !!value, info) < 0)
      return -1;

  return 0;
}

/*
  Tunes the current input to the given freq. Returns -1 on error.
*/
static int
tveng25_tune_input(uint32_t _freq, tveng_device_info * info)
{
  struct v4l2_tuner tuner;
  struct v4l2_frequency freq;

  t_assert(info != NULL);
  t_assert(info->cur_input < info->num_inputs);
  t_assert(info->cur_input >= 0);

  /* Check that there are tuners in the current input */
  if (info->inputs[info->cur_input].tuners == 0)
    return 0; /* Success (we shouldn't be tuning, anyway) */

  /* Get more info about this tuner */

  CLEAR (tuner);

  tuner.index = info->inputs[info->cur_input].tuner_id;

  if (-1 == v4l25_ioctl (info->fd, VIDIOC_G_TUNER, &tuner))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_G_TUNER", info);
      return -1;
    }

  CLEAR (freq);

  freq.tuner = tuner.index;
  freq.type = V4L2_TUNER_ANALOG_TV;
  
  if (tuner.capability & V4L2_TUNER_CAP_LOW)
    freq.frequency = _freq / 0.0625; /* kHz -> 62.5 Hz */
  else
    freq.frequency = _freq / 62.5; /* kHz -> 62.5 kHz */

  if (freq.frequency > tuner.rangehigh)
    freq.frequency = tuner.rangehigh;
  if (freq.frequency < tuner.rangelow)
    freq.frequency = tuner.rangelow;
  
  /* OK, everything is set up, try to tune it */
  if (-1 == v4l25_ioctl (info->fd, VIDIOC_S_FREQUENCY, &freq))
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
tveng25_get_signal_strength (int *strength, int * afc,
			   tveng_device_info * info)
{
  struct v4l2_tuner tuner;

  t_assert(info != NULL);
  t_assert(info->cur_input < info->num_inputs);
  t_assert(info->cur_input >= 0);

  /* Check that there are tuners in the current input */
  if (info->inputs[info->cur_input].tuners == 0)
    return -1;

  tuner.index = info->inputs[info->cur_input].tuner_id;

  if (-1 == v4l25_ioctl (info->fd, VIDIOC_G_TUNER, &tuner))
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
tveng25_get_tune(uint32_t * freq, tveng_device_info * info)
{
  struct v4l2_tuner tuner;
  struct v4l2_frequency vfreq;

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

  CLEAR (vfreq);

  vfreq.tuner = info->inputs[info->cur_input].tuner_id;
  vfreq.type = V4L2_TUNER_ANALOG_TV;

  if (-1 == v4l25_ioctl (info->fd, VIDIOC_G_FREQUENCY, &vfreq))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_G_FREQ", info);
      return -1;
    }

  /* Get more info about this tuner */
  CLEAR (tuner);

  tuner.index = info->inputs[info->cur_input].tuner_id;

  if (-1 == v4l25_ioctl (info -> fd, VIDIOC_G_TUNER, &tuner))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_G_TUNER", info);
      return -1;
    }

  if (tuner.capability & V4L2_TUNER_CAP_LOW)
    *freq = vfreq.frequency * 0.0625;
  else
    *freq = vfreq.frequency * 62.5;

  return 0;
}

/*
  Gets the minimum and maximum freq that the current input can
  tune. If there is no tuner in this input, -1 will be returned.
  If any of the pointers is NULL, its value will not be filled.
*/
static int
tveng25_get_tuner_bounds(uint32_t * min, uint32_t * max, tveng_device_info *
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
  CLEAR (tuner);
  tuner.index = info->inputs[info->cur_input].id;
  if (-1 == v4l25_ioctl (info->fd, VIDIOC_G_TUNER, &tuner))
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
static int p_tveng25_qbuf(int index, tveng_device_info * info)
{
  struct v4l2_buffer tmp_buffer;
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;

  t_assert(info != NULL);

  tmp_buffer.type = p_info -> buffers[0].vidbuf.type;
  tmp_buffer.index = index;

  if (-1 == v4l25_ioctl (info->fd, VIDIOC_QBUF, &tmp_buffer))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_QBUF", info);
      return -1;
    }

  return 0;
}

/* dequeues next available buffer and returns it's id. -1 on error */
static int p_tveng25_dqbuf(tveng_device_info * info)
{
  struct v4l2_buffer tmp_buffer;
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;
  
  t_assert(info != NULL);

  tmp_buffer.type = p_info -> buffers[0].vidbuf.type;

  /* NB this blocks */
  if (-1 == v4l25_ioctl (info->fd, VIDIOC_DQBUF, &tmp_buffer))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_DQBUF", info);
      return -1;
    }

  p_info -> last_timestamp =
    tmp_buffer.timestamp.tv_sec
    + tmp_buffer.timestamp.tv_usec * (1 / 1e6);

  return (tmp_buffer.index);
}

/*
  Sets up the capture device so any read() call after this one
  succeeds. Returns -1 on error.
*/
static int
tveng25_start_capturing(tveng_device_info * info)
{
  struct v4l2_requestbuffers rb;
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;
  int i;

  t_assert(info != NULL);

  tveng_stop_everything(info);

  t_assert(info -> current_mode == TVENG_NO_CAPTURE);
  t_assert(p_info->num_buffers == 0);
  t_assert(p_info->buffers == NULL);

  p_info -> buffers = NULL;
  p_info -> num_buffers = 0;

  rb.count = 8; /* This is a good number(tm) */
  rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  rb.memory = V4L2_MEMORY_MMAP;

  if (-1 == v4l25_ioctl (info->fd, VIDIOC_REQBUFS, &rb))
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

  p_info -> buffers = (struct tveng25_vbuf*)
    malloc(rb.count*sizeof(struct tveng25_vbuf));
  p_info -> num_buffers = rb.count;

  for (i = 0; i < rb.count; i++)
    {
      p_info -> buffers[i].vidbuf.index = i;
      p_info -> buffers[i].vidbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

      if (-1 == v4l25_ioctl (info->fd, VIDIOC_QUERYBUF,
			     &p_info->buffers[i].vidbuf))
	{
	  info->tveng_errno = errno;
	  t_error("VIDIOC_QUERYBUF", info);
	  return -1;
	}

      p_info->buffers[i].vmem =
	mmap (0, p_info->buffers[i].vidbuf.length,
	      PROT_READ | PROT_WRITE,
	      MAP_SHARED, info->fd,
	      p_info->buffers[i].vidbuf.m.offset);

      if (p_info->buffers[i].vmem == (void *) -1)
	p_info->buffers[i].vmem =
	  mmap(0, p_info->buffers[i].vidbuf.length,
	       PROT_READ, MAP_SHARED, info->fd, 
	       p_info->buffers[i].vidbuf.m.offset);

      if (p_info->buffers[i].vmem == (void *) -1)
	{
	  info->tveng_errno = errno;
	  t_error("mmap()", info);
	  return -1;
	}

	/* Queue the buffer */
      if (p_tveng25_qbuf(i, info) == -1)
	return -1;
    }

  /* Turn on streaming */
  i = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == v4l25_ioctl (info->fd, VIDIOC_STREAMON, &i))
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
tveng25_stop_capturing(tveng_device_info * info)
{
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;
  int i;

  if (info -> current_mode == TVENG_NO_CAPTURE)
    {
      fprintf(stderr,
	      "Warning: trying to stop capture with no capture active\n");
      return 0; /* Nothing to be done */
    }

  t_assert(info->current_mode == TVENG_CAPTURE_READ);

  /* Turn streaming off */
  i = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == v4l25_ioctl (info->fd, VIDIOC_STREAMOFF, &i))
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
int tveng25_read_frame(void * where, unsigned int bpl, 
		      unsigned int time, tveng_device_info * info)
{
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info*) info;
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

  if (info->format.pixformat != TVENG_PIX_YVU420 &&
      info->format.pixformat != TVENG_PIX_YVU420 &&
      info -> format.width * info->format.bpp > bpl)
    {
      info -> tveng_errno = ENOMEM;
      t_error_msg("check()", 
		  "Bpl check failed, quitting to avoid segfault (%d, %d)",
		  info, bpl, (int) (info->format.width * info->format.bpp));
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

  n = p_tveng25_dqbuf(info);
  if (n == -1)
    return -1;

  /* Ignore frames we haven't been able to process */
  do{
    FD_ZERO(&rdset);
    FD_SET(info->fd, &rdset);
    timeout.tv_sec = timeout.tv_usec = 0;
    if (select(info->fd +1, &rdset, NULL, NULL, &timeout) < 1)
      break;
    p_tveng25_qbuf(n, info);
    n = p_tveng25_dqbuf(info);
  } while (1);

  /* Copy the data to the address given */
  if (where)
    {
      if (bpl == info->format.bytesperline ||
	  info->format.pixformat == TVENG_PIX_YUV420 ||
	  info->format.pixformat == TVENG_PIX_YVU420)
	memcpy(where, p_info->buffers[n].vmem,
	       info->format.sizeimage);
      else
	{
	  unsigned char *p = p_info->buffers[n].vmem;
	  unsigned int line;
	  for (line = 0; line < info->format.height; line++)
	    {
	      memcpy(where, p, bpl);
	      where += bpl;
	      p += info->format.bytesperline;
	    }
	}
    }

  /* Queue the buffer again for processing */
  if (p_tveng25_qbuf(n, info))
    return -1;

  /* Everything has been OK, return 0 (success) */
  return 0;
}

/*
  Gets the timestamp of the last read frame in seconds.
  Returns -1 on error, if the current mode isn't capture, or if we
  haven't captured any frame yet.
*/
static double tveng25_get_timestamp(tveng_device_info * info)
{
  struct private_tveng25_device_info * p_info =
    (struct private_tveng25_device_info *) info;

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
int tveng25_set_capture_size(int width, int height, tveng_device_info * info)
{
  enum tveng_capture_mode current_mode;
  int retcode;

  t_assert(info != NULL);
  t_assert(width > 0);
  t_assert(height > 0);

  tveng25_update_capture_format(info);

  current_mode = tveng_stop_everything(info);

  info -> format.width = width;
  info -> format.height = height;
  retcode = tveng25_set_capture_format(info);

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
int tveng25_get_capture_size(int *width, int *height, tveng_device_info * info)
{
  t_assert(info != NULL);

  if (tveng25_update_capture_format(info))
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
tveng25_detect_preview (tveng_device_info * info)
{
  struct v4l2_framebuffer fb;
  extern int disable_overlay;

  t_assert(info != NULL);

  if (disable_overlay ||
      (info -> caps.flags & TVENG_CAPS_OVERLAY) == 0)
    {
      info -> tveng_errno = -1;
      t_error_msg("flags check",
       "The capability field says that there is no overlay", info);
      return 0;
    }

  /* Get the current framebuffer info */
  if (-1 == v4l25_ioctl (info -> fd, VIDIOC_G_FBUF, &fb))
    {
      info->tveng_errno = errno;
      t_error("VIDIOCGFBUF", info);
      return 0;
    }

  info->fb_info.base = fb.base;
  info->fb_info.height = fb.fmt.height;
  info->fb_info.width = fb.fmt.width;
  info->fb_info.depth = 1; /* XXX */
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
tveng25_set_preview_window(tveng_device_info * info)
{
  struct v4l2_format format;
  struct v4l2_clip * clip=NULL;
  int i;

  t_assert(info != NULL);
  t_assert(info-> window.clipcount >= 0);

  CLEAR (format);

  format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;

  format.fmt.win.w.left = info->window.x;
  format.fmt.win.w.top = info->window.y;
  format.fmt.win.w.width = info->window.width;
  format.fmt.win.w.height = info->window.height;
  format.fmt.win.clipcount = info->window.clipcount;
  format.fmt.win.chromakey = info->priv->chromakey;

  if (format.fmt.win.clipcount == 0)
   format.fmt.win.clips = NULL;
  else
    {
      clip = malloc(sizeof(struct v4l2_clip)*format.fmt.win.clipcount);
      format.fmt.win.clips = clip;
      for (i=0;i<format.fmt.win.clipcount;i++)
	{
	  clip[i].c.left = info->window.clips[i].x;
	  clip[i].c.top = info->window.clips[i].y;
	  clip[i].c.width = info->window.clips[i].width;
	  clip[i].c.height = info->window.clips[i].height;
	  clip[i].next = ((i+1) == format.fmt.win.clipcount) ?
	    NULL : &(clip[i+1]);
	}
    }

  tveng_set_preview_off(info);

  /* Set the new window */
  if (-1 == v4l25_ioctl (info->fd, VIDIOC_S_FMT, &format))
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_S_WIN", info);
      free(clip);
      return -1;
    }

  if (clip)
    free(clip);

  /* Update the info struct */
  return (tveng25_get_preview_window(info));
}

/*
  Gets the current overlay window parameters.
  Returns -1 on error, and any other value on success.
  info   : The device to use
*/
static int
tveng25_get_preview_window(tveng_device_info * info)
{
  /* Updates the entire capture format, since there is no
     difference */
  return (tveng25_update_capture_format(info));
}

/* 
   Sets the previewing on/off.
   on : if 1, set preview on, if 0 off, other values are silently ignored
   info  : device to use for previewing
   Returns -1 on error, anything else on success
*/
static int
tveng25_set_preview (int on, tveng_device_info * info)
{
  int i = !!on;

  t_assert(info != NULL);

  if (-1 == v4l25_ioctl (info->fd, VIDIOC_OVERLAY, &i))
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
tveng25_start_previewing (tveng_device_info * info)
{
#ifndef DISABLE_X_EXTENSIONS
  Display * dpy = info->priv->display;
  int width, height;
  int dwidth, dheight; /* Width and height of the display */

  tveng_stop_everything(info);

  t_assert(info -> current_mode == TVENG_NO_CAPTURE);

  if (!tveng25_detect_preview(info))
    /* We shouldn't be reaching this if the app is well programmed */
    t_assert_not_reached();

  if (!tveng_detect_XF86DGA(info))
    return -1;

  /* calculate coordinates for the preview window. We compute this for
   the first display */
  XF86DGAGetViewPortSize(dpy, DefaultScreen(dpy),
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
  if (tveng25_set_preview_window(info) == -1)
    return -1;

  /* Center preview window (maybe the requested width and/or height)
     aren't valid */
  info->window.x = (dwidth - info->window.width)/2;
  info->window.y = (dheight - info->window.height)/2;
  info->window.clipcount = 0;
  info->window.clips = NULL;
  if (tveng25_set_preview_window(info) == -1)
    return -1;

  /* Start preview */
  if (tveng25_set_preview(1, info) == -1)
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
tveng25_stop_previewing(tveng_device_info * info)
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
  tveng25_set_preview(0, info);

  info -> current_mode = TVENG_NO_CAPTURE;
  return 0; /* Success */
#else
  return 0;
#endif
}

static struct tveng_module_info tveng25_module_info = {
  attach_device:		tveng25_attach_device,
  describe_controller:		tveng25_describe_controller,
  close_device:			tveng25_close_device,
  get_inputs:			tveng25_get_inputs,
  set_input:			tveng25_set_input,
  get_standards:		tveng25_get_standards,
  set_standard:			tveng25_set_standard,
  update_capture_format:	tveng25_update_capture_format,
  set_capture_format:		tveng25_set_capture_format,
  update_controls:		tveng25_update_controls,
  set_control:			tveng25_set_control,
  get_mute:			tveng25_get_mute,
  set_mute:			tveng25_set_mute,
  tune_input:			tveng25_tune_input,
  get_signal_strength:		tveng25_get_signal_strength,
  get_tune:			tveng25_get_tune,
  get_tuner_bounds:		tveng25_get_tuner_bounds,
  start_capturing:		tveng25_start_capturing,
  stop_capturing:		tveng25_stop_capturing,
  read_frame:			tveng25_read_frame,
  get_timestamp:		tveng25_get_timestamp,
  set_capture_size:		tveng25_set_capture_size,
  get_capture_size:		tveng25_get_capture_size,
  detect_preview:		tveng25_detect_preview,
  set_preview_window:		tveng25_set_preview_window,
  get_preview_window:		tveng25_get_preview_window,
  set_preview:			tveng25_set_preview,
  start_previewing:		tveng25_start_previewing,
  stop_previewing:		tveng25_stop_previewing,

  private_size:			sizeof(struct private_tveng25_device_info)
};

/*
  Inits the V4L2 module, and fills in the given table.
*/
void tveng25_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

  memcpy(module_info, &tveng25_module_info,
	 sizeof(struct tveng_module_info)); 
}

#else /* !ENABLE_V4L */

#include "tveng25.h"

void tveng25_init_module(struct tveng_module_info *module_info)
{
  t_assert(module_info != NULL);

  CLEAR (module_info);
}

#endif /* ENABLE_V4L */
