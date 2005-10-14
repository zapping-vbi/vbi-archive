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

/* $Id: v4l.c,v 1.12 2005-10-14 23:38:01 mschimek Exp $ */

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
int
setup_v4l			(const char *		device_name,
				 int			device_fd,
				 const tv_overlay_buffer *buffer)
{
  int fd;
  struct video_capability caps;
  struct video_buffer fb;
  const tv_pixel_format *pf;
  int saved_errno;

  fd = device_open_safer (device_name, device_fd, /* major */ 81, O_RDWR);
  if (-1 == fd)
    return -1; /* failed */

  message (/* verbosity */ 2,
	   "Querying device capabilities.\n");

  if (-1 == xioctl (fd, VIDIOCGCAP, &caps))
    {
      saved_errno = errno;

      if (EINVAL != saved_errno)
	goto failure;

      device_close (device_log_fp, fd);

      message (/* verbosity */ 2,
	       "Not a V4L device.\n");

      errno = EINVAL;
      return -2; /* unknown API */
    }

  message (/* verbosity */ 1,
	   "Using V4L interface.\n");

  message (/* verbosity */ 2,
	   "Checking overlay capability.\n");

  if (!(caps.type & VID_TYPE_OVERLAY))
    {
      errmsg (_("The device does not support video overlay."));
      saved_errno = EINVAL;
      goto failure;
    }

  message (/* verbosity */ 2,
	   "Getting current frame buffer parameters.\n");

  if (-1 == xioctl (fd, VIDIOCGFBUF, &fb))
    {
      saved_errno = errno;
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

  /* This ioctl is privileged because it sets up
     DMA to a random (video memory) address. */
  {
    int result;

    if (-1 == restore_root_privileges ())
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
        if (EPERM == saved_errno
	    && ROOT_UID != euid)
	  privilege_hint ();

	goto failure;
      }
  }

  device_close (device_log_fp, fd);
  return 0; /* success */

 failure:
  device_close (device_log_fp, fd);
  errno = saved_errno;
  return -1; /* failed */
}

#else /* !ENABLE_V4L */

int
setup_v4l			(const char *		device_name,
				 const tv_overlay_buffer *buffer)
{
  message (/* verbosity */ 2,
	   "No V4L support compiled in.\n");

  errno = EINVAL;
  return -2; /* unknown API */
}

#endif /* !ENABLE_V4L */
