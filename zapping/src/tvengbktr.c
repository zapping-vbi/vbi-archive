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

/* Note this code forked off from tveng1.c before a number of
   changes were made to the interface and wasn't updated. It's
   really just an ugly mix of v4l, bktr, meteor and old tveng
   code. A complete review will be necessary. */

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


#include "tveng.h"
#define TVENGBKTR_PROTOTYPES 1
#include "tvengbktr.h"
#include "../common/videodev2.h" /* the V4L2 definitions */
#include "../common/types.h"

#include "zmisc.h"

/* TFR repeats the ioctl when interrupted (EINTR) */
#define IOCTL(fd, cmd, data) (TEMP_FAILURE_RETRY(ioctl(fd, cmd, data)))



#define LOG_FP 0 /* stderr */

#define bktr_ioctl(fd, cmd, arg)					\
(IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
 device_ioctl (LOG_FP, fprintf_ioctl_arg, fd, cmd, arg))

struct standard {
	tv_video_standard	pub;
	unsigned long		fmt;
};

#define S(l) PARENT (l, struct standard, pub)

struct control {
	tv_control		pub;
	unsigned int		id;
};

#define C(l) PARENT (l, struct control, pub)

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
    failure:
      tvengbktr_close_device(info);
      return -1;
    }

	/* Video standards */

	info->video_standards = NULL;
	info->cur_video_standard = NULL;

	if (!update_standard_list (info)
	    || !update_current_standard (info))
		goto failure;


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
    *short_str = "BKTR";
  if (long_str)
    *long_str = "BKTR/Meteor";
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

	free_standards (info);

	free_controls (info);

  info->num_inputs = 0;
  info->inputs = NULL;
  info->file_name = NULL;
}

/*
  Functions for controlling the video capture. All of them return -1
  in case of error, so any value != -1 should be considered valid
  (unless explicitly stated in the description of the function) 
*/


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

struct control_bridge {
	unsigned int		id;
	const char *		label;
	tv_control_id		tcid;
	int			minimum;
	int			maximum;
	int			reset;
};

#define CONTROL_BRIDGE_END { 0, NULL, 0, 0, 0, 0 }

static const struct control_bridge
meteor_controls [] = {
	{ METEORGBRIG, N_("Brightness"), TV_CONTROL_ID_BRIGHTNESS,   0, 255, 128 },
	{ METEORGCONT, N_("Contrast"),   TV_CONTROL_ID_CONTRAST,     0, 255, 128 },
	{ METEORGCSAT, N_("Saturation"), TV_CONTROL_ID_SATURATION,   0, 255, 128 },
	{ METEORGHUE,  N_("Hue"),        TV_CONTROL_ID_HUE,       -128, 127,   0 },
#if 0
	{ METEORGCHCV, N_("U/V Gain"),   TV_CONTROL_ID_UNKNOWN,      0, 255, 128 },
#endif
	CONTROL_BRIDGE_END
};

static const struct control_bridge
bktr_controls [] = {
	{ BT848_GBRIG, N_("Brightness"),   TV_CONTROL_ID_BRIGHTNESS, -128, 127,   0 },
	{ BT848_GCONT, N_("Contrast"),     TV_CONTROL_ID_CONTRAST,      0, 511, 216 },
	{ BT848_GCSAT, N_("Saturation"),   TV_CONTROL_ID_SATURATION,    0, 511, 216 },
	{ BT848_GHUE,  N_("Hue"),          TV_CONTROL_ID_HUE,        -128, 127,   0 },
#if 0
	{ BT848_GUSAT, N_("U Saturation"), TV_CONTROL_ID_UNKNOWN,       0, 511, 256 },
	{ BT848_GVSAT, N_("V Saturation"), TV_CONTROL_ID_UNKNOWN,       0, 511, 180 },
#endif
	CONTROL_BRIDGE_END
};

static tv_bool
update_control			(struct private_tvengbktr_device_info * p_info,
				 struct control *	c)
{
	int value;
	int r;
	char c;
	unsigned char uc;

	switch (c->id) {
	case METEORGHUE:
		r = bktr_ioctl (p_info->info.fd, c->id, &c);
		value = c;
		break;

	case METEORGBRIG:
	case METEORGCSAT:
	case METEORGCONT:
	case METEORGCHCV:
		r = bktr_ioctl (p_info->info.fd, c->id, &uc);
		value = uc;
		break;

	case BT848_GHUE:
	case BT848_GBRIG:
	case BT848_GCSAT:
	case BT848_GCONT:
	case BT848_GVSAT:
	case BT848_GUSAT:
		r = bktr_ioctl (p_info->info.fd, c->id, &value);
		break;

	default:
		t_assert (!"reached");
		break;
	}

	if (-1 == r) {
		p_info->info.tveng_errno = errno;
		t_error("Get control", &p_info->info);
		return FALSE;
	}

	if (c->dev.pub.value != ctrl.value) {
		c->dev.pub.value = ctrl.value;
		tv_callback_notify (&c->dev.pub, c->dev.callback);
	}

	return TRUE;
}

static tv_bool
tvengbktr_update_control	(tveng_device_info *	info,
				 tv_control *		tc)
{
	struct private_tvengbktr_device_info * p_info = P_INFO (info);

	if (tc)
		return update_control (p_info, C(tc));

	for (tc = p_info->info.controls; tc; tc = tc->next)
		if (C(tc)->_device == info)
			if (!update_control (p_info, C(tc)))
				return FALSE;

	return TRUE;
}

static tv_bool
tvengbktr_set_control		(tveng_device_info *	info,
				 tv_control *		tc,
				 int			value)
{
	struct private_tvengbktr_device_info * p_info = P_INFO (info);
	int r;
	char c;
	unsigned char uc;

	switch (c->id) {
	case METEORGHUE:
		c = value;
		r = bktr_ioctl (p_info->info.fd, METEORSHUE, &c);
		break;

	case METEORGBRIG:
		uc = value;
		r = bktr_ioctl (p_info->info.fd, METEORSBRIG, &uc);
		break;

	case METEORGCSAT:
		uc = value;
		r = bktr_ioctl (p_info->info.fd, METEORSCSAT, &uc);
		break;

	case METEORGCONT:
		uc = value;
		r = bktr_ioctl (p_info->info.fd, METEORSCONT, &uc);
		break;

	case METEORGCHCV:
		uc = value;
		r = bktr_ioctl (p_info->info.fd, METEORSCHCV, &uc);
		break;

	case BT848_GHUE:
		r = bktr_ioctl (p_info->info.fd, BKTR_SHUE, &value);
		break;

	case BT848_GBRIG:
		r = bktr_ioctl (p_info->info.fd, BKTR_SBRIG, &value);
		break;

	case BT848_GCSAT:
		r = bktr_ioctl (p_info->info.fd, BKTR_SCSAT, &value);
		break;

	case BT848_GCONT:
		r = bktr_ioctl (p_info->info.fd, BKTR_SCONT, &value);
		break;

	case BT848_GVSAT:
		r = bktr_ioctl (p_info->info.fd, BKTR_SVSAT, &value);
		break;

	case BT848_GUSAT:
		r = bktr_ioctl (p_info->info.fd, BKTR_SUSAT, &value);
		break;

	default:
		t_assert (!"reached");
		break;
	}

	if (-1 == r) {
		p_info->info.tveng_errno = errno;
		t_error("Set control", &p_info->info);
		return FALSE;
	}

	if (c->dev.pub.value != ctrl.value) {
		c->dev.pub.value = ctrl.value;
		tv_callback_notify (&c->dev.pub, c->dev.callback);
	}

	return TRUE;
}

static int
p_tvengbktr_build_controls(tveng_device_info * info)
{
	struct private_tvengbktr_device_info * p_info = P_INFO (info);
	struct control_bridge *cm;
	struct control *c;
	tv_control *tc;

	cm = ;

	for (; cm->id; ++cm) {
		if (!(c = calloc (1, sizeof (*c))))
			return FALSE;

		c->id		= cm->id;

		c->pub.type	= TV_CONTROL_TYPE_INTEGER;
		c->pub.id	= cm->tcid;

		if (!(c->pub.label = strdup (_(cm->label))))
			goto failure;

		c->pub.minimum	= cm->minimum;
		c->pub.maximum	= cm->maximum;
		c->pub.step	= 1;
		c->pub.reset	= cm->reset;

		if (!(tc = append_control (info, &c->pub, 0))) {
		failure:
			free_control (&c->pub);
			return FALSE;
		}

		update_control (p_info, C(tc));
	}

	return TRUE;
}

/*
 *  Video standards
 */

struct standard_bridge {
	unsigned int		fmt;
	const char *		label;
	tv_video_standard_id	id;
};

static const struct standard_bridge
meteor_standards [] = {
	/* XXX should investigate what exactly these videostandards are. */
	{ METEOR_FMT_PAL,		"PAL",		TV_VIDEOSTD_PAL },
	{ METEOR_FMT_NTSC,		"NTSC",		TV_VIDEOSTD_NTSC },
	{ METEOR_FMT_SECAM,		"SECAM",	TV_VIDEOSTD_SECAM },
	{ 0,				NULL,		0 }
};

static const struct standard_bridge
bktr_standards [] = {
	{ BT848_IFORM_F_PALBDGHI,	"PAL",		TV_VIDEOSTD_PAL },
	{ BT848_IFORM_F_NTSCM,		"NTSC",		TV_VIDEOSTD_NTSC_M },
	{ BT848_IFORM_F_SECAM,		"SECAM",	TV_VIDEOSTD_SECAM },
	{ BT848_IFORM_F_PALM,		"PAL-M",	TV_VIDEOSTD_PAL_M },
	{ BT848_IFORM_F_PALN,		"PAL-N",	TV_VIDEOSTD_PAL_N },
	{ BT848_IFORM_F_NTSCJ,		"NTSC-JP",	TV_VIDEOSTD_NTSC_M_JP },
#if 0
	{ BT848_IFORM_F_AUTO,		"AUTO",		TV_VIDEOSTD_UNKNOWN },
	{ BT848_IFORM_F_RSVD,		"RSVD",		TV_VIDEOSTD_UNKNOWN },
#endif
	{ 0,				NULL,		0 }
};

static tv_bool
update_standard_list		(tveng_device_info *	info)
{
	struct standard_bridge *table;
	unsigned int i;

	if (info->videostds)
		return TRUE; /* invariable */

	if (P_INFO (info)->bktr_driver)
		table = bktr_standards;
	else
		table = meteor_standards;

	for (i = 0; table[i].label; ++i) {
		struct standard *s;

		if (!(s = S(append_video_standard (&info->video_standards,
						   table[i].id,
						   table[i].label,
						   table[i].label,
						   sizeof (*s))))) {
			free_video_standard_list (&info->video_standards);
			return FALSE;
		}

		s->fmt = table[i].fmt;
	}

	return TRUE;
}

static tv_bool
update_current_standard		(tveng_device_info *	info)
{
	tv_video_standard *ts;

	ts = NULL; /* unknown */

	if (info->video_standards) {
		struct tveng_enum_input *input;
		unsigned long fmt;

		input = info->inputs + info->cur_input;

		if (info->bktr_driver) {
			if (-1 == bktr_ioctl (info->fd, BT848GFMT, &fmt))
				return FALSE;
		} else {
			if (-1 == bktr_ioctl (info->fd, METEORGFMT, &fmt))
				return FALSE;
		}

		for (ts = info->video_standards; ts; ts = ts->_next)
			if (S(ts)->fmt == fmt)
				break;
	}

	set_cur_video_standard (info, ts);

	return TRUE;
}

static tv_bool
set_standard			(tveng_device_info *	info,
				 tv_videostd *		ts)
{
	enum tveng_capture_mode current_mode;
	struct tveng_enum_input *input;
	int r;

	input = info->inputs + info->cur_input;

	current_mode = tveng_stop_everything(info);

	if (info->bktr_driver) {
		r = bktr_ioctl (info->fd, BT848GFMT, &S(ts)->fmt);
	} else {
		r = bktr_ioctl (info->fd, METEORGFMT, &S(ts)->fmt);
	}

	/* Start capturing again as if nothing had happened */
	/* XXX stop yes, restarting is not our business (eg. frame geometry change). */
	tveng_restart_everything(current_mode, info);

	return (r == 0);
}

/*
 *  Video inputs
 */

#if 0

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

#endif /* 0 */



/*
  Tunes the current input to the given freq (khz). Returns -1 on error.
*/
static int
tvengbktr_tune_input(uint32_t _freq, tveng_device_info * info)
{
	unsigned int freq;

	t_assert(info != NULL);
	t_assert(info->cur_input < info->num_inputs);
	t_assert(info->cur_input >= 0);

	/* Check that there are tuners in the current input */
	if (info->inputs[info->cur_input].tuners == 0)
		return 0; /* Success (we shouldn't be tuning, anyway) */
	
	freq = _freq / 62.5;

	if (freq > tuner_info.rangehigh)
		freq = tuner_info.rangehigh;
	else if (freq < tuner_info.rangelow)
		freq = tuner_info.rangelow;

	if (-1 == bktr_ioctl (info->fd, TVTUNER_SETFREQ, &freq)) {
		info->tveng_errno = errno;
		t_error("TVTUNER_SETFREQ", info);
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
  Stores in freq the currently tuned freq (kHz). Returns -1 on error.
*/
static int
tvengbktr_get_tune		(uint32_t * _freq,
				 tveng_device_info * info)
{
	unsigned int freq;

	t_assert(info != NULL);
	t_assert(freq != NULL);
	t_assert(info->cur_input < info->num_inputs);
	t_assert(info->cur_input >= 0);

	/* Check that there are tuners in the current input */
	if (info->inputs[info->cur_input].tuners == 0) {
		if (freq)
			*freq = 0;
		info->tveng_errno = -1;
		t_error_msg("tuners check",
			    "There are no tuners for the active input",
			    info);
		return -1;
	}

	if (-1 == bktr_ioctl (info->fd, TVTUNER_GETFREQ, &freq)) {
		info->tveng_errno = errno;
		t_error("TVTUNER_GETFREQ", info);
		return -1;
	}

	*_freq = freq * 62.5;

	return 0;
}

/*
  Gets the minimum and maximum freq that the current input can
  tune. If there is no tuner in this input, -1 will be returned.
  If any of the pointers is NULL, its value will not be filled.
*/
static int
tvengbktr_get_tuner_bounds	(uint32_t * min,
				 uint32_t * max,
				 tveng_device_info *info)
{
	struct v4l2_tuner tuner;

	t_assert(info != NULL);
	t_assert(info->cur_input < info->num_inputs);
	t_assert(info->cur_input >= 0);

	/* Check that there are tuners in the current input */
	if (info->inputs[info->cur_input].tuners == 0)
		return -1;
	
	*min = 0;
	*max = 900000;	// XXX

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
static int
tvengbktr_get_capture_size	(int *width,
				 int *height,
				 tveng_device_info * info)
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

/*
 *  Overlay
 */

static tv_bool
get_overlay_buffer		(tveng_device_info *	info,
				 tv_overlay_buffer *	t)
{
}

/*
  Sets the preview window dimensions to the given window.
  Returns -1 on error, something else on success.
  Success doesn't mean that the requested dimensions are used, maybe
  they are different, check the returned fields to see if they are suitable
  info   : Device we are controlling
*/
static int
tvengbktr_set_preview_window	(tveng_device_info *	info,
				 x11_dga_parameters *	dga)
{
	struct private_tvengbktr_device_info * p_info =
		(struct private_tvengbktr_device_info*) info;
	struct meteor_video video;
	struct meteor_geom geom;

	struct _bktr_clip clip;
	const tv_clip *tclip;
	unsigned int i;

	t_assert(info != NULL);
	t_assert(info-> window.clipcount >= 0);

	video.addr = (void *)((char *) dga->base
			      + info->window.y * dga->bytes_per_line
			      + info->window.x * dga->bits_per_pixel * 8);
	video.width = dga->bytes_per_line;
	video.banksize = dga->size / 1024;
	video.ramsize = dga->size / 1024;

	geom.rows = info->window.width;
	geom.columns = info->window.height;
	geom.frames = 1;
	geom.oformat = ;


	if (info->overlay_window.clip_vector.size > (N_ELEMENTS (clip.x) - 1)) {
		info->tveng_errno = 0;
		t_error ("Too many clipping rectangles", info);
		return -1;
	}

	tclip = info->overlay_window.clip_vector.vector;

	for (i = 0; i < info->overlay_window.clip_vector.size; ++i) {
		clip.x[i].x_min = tclip->x;
		clip.x[i].y_min = tclip->y;
		clip.x[i].x_max = tclip->x + tclip->width;
		clip.x[i].y_max = tclip->y + tclip->height;
	}

	clip.x[i].y_min = 0;
	clip.x[i].y_max = 0;


  tveng_set_preview_off(info);

  /* Set the new window */
  if (IOCTL(info->fd, VIDIOC_S_WIN, &window) != 0)
    {
      info->tveng_errno = errno;
      t_error("VIDIOC_S_WIN", info);
      return -1;
    }


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

static tv_bool
set_overlay			(tveng_device_info *	info,
				 tv_bool		on)
{
}


/* 
   Sets up everything and starts previewing.
   Just call this function to start previewing, it takes care of
   (mostly) everything.
   Returns -1 on error.
*/
static int
tvengbktr_start_previewing (tveng_device_info * info,
			    x11_dga_parameters *dga)
{
	unsigned int width, height;

	tveng_stop_everything(info);

	t_assert(info -> current_mode == TVENG_NO_CAPTURE);

	if (!tvengbktr_detect_preview(info))
		/* We shouldn't be reaching this if the app is well programmed */
		t_assert_not_reached();
	
	if (!x11_dga_present (dga))
		return -1;

	width = MIN (info->standards[cur_standard].width, dga->width);
	height = MIN (info->standards[cur_standard].height, dga->height);

	info->window.x = (dga->width - width) >> 1;
	info->window.y = (dga->height - height) >> 1;
	info->window.width = width;
	info->window.height = height;
	info->window.clips = NULL;
	info->window.clipcount = 0;

	/* Set new capture dimensions */
	if (tvengbktr_set_preview_window(info) == -1)
		return -1;

	/* Center preview window (maybe the requested width and/or height)
	   aren't valid */
	info->window.x = (dga->width - info->window.width) >> 1;
	info->window.y = (dga->height - info->window.height) >> 1;
	info->window.clipcount = 0;
	info->window.clips = NULL;
	if (tvengbktr_set_preview_window(info) == -1)
		return -1;

	/* Start preview */
	if (tvengbktr_set_preview(1, info) == -1)
		return -1;

	info -> current_mode = TVENG_CAPTURE_PREVIEW;
	return 0; /* Success */
}

/*
  Stops the fullscreen mode. Returns -1 on error
*/
static int
tvengbktr_stop_previewing(tveng_device_info * info)
{
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
  .update_standards		= update_current_standard,
  .set_standard			= set_standard,
  .update_capture_format =	tvengbktr_update_capture_format,
  .set_capture_format =		tvengbktr_set_capture_format,
  .update_control		= update_control,
  .set_control			= set_control,
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

  .ilog =			fprintf_ioctl_arg,

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
