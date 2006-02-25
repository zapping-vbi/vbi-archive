/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2000 Iñaki García Etxebarria
 * Copyright (C) 2003, 2005 Michael H. Schimek
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

/* $Id: v4l.c,v 1.13 2006-02-25 17:34:54 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "zapping_setup_fb.h"

#ifdef ENABLE_V4L

#include "common/intl-priv.h"

#include "common/videodev.h"
#include "common/_videodev.h"

#define xioctl(fd, cmd, arg)						\
  (IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
   device_ioctl (device_log_fp, fprint_v4l_ioctl_arg, fd, cmd, arg))

/* Attn: device_name may be NULL, device_fd may be -1. */
zsfb_status
setup_v4l			(const char *		device_name,
				 int			device_fd,
				 const tv_overlay_buffer *buffer)
{
  zsfb_status status;
  int saved_errno;
  int fd;
  struct video_capability caps;
  struct video_buffer fb;
  const tv_pixel_format *pf;

  status = device_open_safer (&fd, device_name,
			      device_fd, /* major */ 81, O_RDWR);
  if (ZSFB_SUCCESS != status)
    return status;

  message (/* verbosity */ 2,
	   "Querying device capabilities.\n");

  if (-1 == xioctl (fd, VIDIOCGCAP, &caps))
    {
      saved_errno = errno;

      if (EINVAL == saved_errno)
	{
	  message (/* verbosity */ 2,
		   "Not a V4L device.\n");
	  status = ZSFB_UNKNOWN_DEVICE;
	}
      else
	{
	  errmsg_ioctl ("VIDIOCGCAP", saved_errno);
	  status = ZSFB_IOCTL_ERROR;
	}

      goto failure;
    }

  message (/* verbosity */ 1,
	   "Using V4L interface.\n");

  message (/* verbosity */ 2,
	   "Checking overlay capability.\n");

  if (!(caps.type & VID_TYPE_OVERLAY))
    {
      errmsg (_("The device does not support video overlay."));
      saved_errno = EINVAL;
      status = ZSFB_OVERLAY_IMPOSSIBLE;
      goto failure;
    }

  message (/* verbosity */ 2,
	   "Getting current frame buffer parameters.\n");

  if (-1 == xioctl (fd, VIDIOCGFBUF, &fb))
    {
      saved_errno = errno;
      errmsg_ioctl ("VIDIOCGFBUF", saved_errno);
      status = ZSFB_IOCTL_ERROR;
      goto failure;
    }

  fb.base		= (void *) buffer->base;
  fb.width		= buffer->format.width;
  fb.height		= buffer->format.height;

  pf = buffer->format.pixel_format;

  if (32 == pf->bits_per_pixel)
    fb.depth		= 32; /* depth 24 bpp 32 */
  else
    fb.depth		= pf->color_depth; /* 15, 16, 24 */

  fb.bytesperline	= buffer->format.bytes_per_line[0];

  message (/* verbosity */ 2,
	   "Setting new frame buffer parameters.\n");

  message (/* verbosity */ 3,
	   "base=%p height=%u width=%u depth=%u bytesperline=%u\n",
	   fb.base, fb.height, fb.width, fb.depth, fb.bytesperline);

  /* This ioctl is privileged because it sets up
     DMA to a random (video memory) address. */
  {
    int result;

    status = restore_root_privileges ();
    if (ZSFB_SUCCESS != status)
      {
	saved_errno = errno;
	goto failure;
      }

    result = ioctl (fd, VIDIOCSFBUF, &fb);
    saved_errno = errno;

    /* Aborts on error. */
    drop_root_privileges ();

    if (-1 == result)
      {
	errmsg_ioctl ("VIDIOCSFBUF", saved_errno);
	status = ZSFB_IOCTL_ERROR;
        if (EPERM == saved_errno)
	  {
	    status = ZSFB_NO_PERMISSION;
	    if (ROOT_UID != euid)
	      privilege_hint ();
	  }
	goto failure;
      }
  }

  device_close (device_log_fp, fd);
  return ZSFB_SUCCESS;

 failure:
  device_close (device_log_fp, fd);
  errno = saved_errno;
  return status;
}

#else /* !ENABLE_V4L */

zsfb_status
setup_v4l			(const char *		device_name,
				 const tv_overlay_buffer *buffer)
{
  message (/* verbosity */ 2,
	   "No V4L support compiled in.\n");

  errno = EINVAL;
  return ZSFB_UNKNOWN_DEVICE;
}

#endif /* !ENABLE_V4L */
