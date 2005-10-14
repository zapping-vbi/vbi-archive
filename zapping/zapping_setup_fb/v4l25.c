/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2000 Iñaki García Etxebarria
 * Copyright (C) 2003-2005 Michael H. Schimek
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

/* $Id: v4l25.c,v 1.13 2005-10-14 23:40:14 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "zapping_setup_fb.h"

#ifdef ENABLE_V4L

#include "common/intl-priv.h"

#include <linux/types.h>		/* __u32 etc */
#include <sys/time.h>			/* struct timeval */
#include "common/videodev25.h"		/* V4L2 header file */
#include "common/_videodev25.h"

#define xioctl(fd, cmd, arg)						\
  (IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
   device_ioctl (device_log_fp, fprint_v4l25_ioctl_arg, fd, cmd, arg))

/* Attn: device_name may be NULL, device_fd may be -1. */
int
setup_v4l25			(const char *		device_name,
				 int			device_fd,
				 const tv_overlay_buffer *buffer)
{
  int fd;
  struct v4l2_capability cap;
  struct v4l2_framebuffer fb;
  const tv_pixel_format *pf;
  int saved_errno;

  fd = device_open_safer (device_name, device_fd, /* major */ 81, O_RDWR);
  if (-1 == fd)
    return -1; /* failed */

  message (/* verbosity */ 2,
	   "Querying device capabilities.\n");

  if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap))
    {
      saved_errno = errno;

      if (EINVAL != saved_errno)
	goto failure;

      device_close (device_log_fp, fd);

      message (/* verbosity */ 2,
	       "Not a V4L2 2.5 device.\n");

      errno = EINVAL;
      return -2; /* unknown API */
    }

  message (/* verbosity */ 1,
	   "Using V4L2 2.5 interface.\n");

  message (/* verbosity */ 2,
	   "Checking overlay capability.\n");

  if (!(cap.capabilities & V4L2_CAP_VIDEO_OVERLAY))
    {
      errmsg (_("The device does not support video overlay."));
      saved_errno = EINVAL;
      goto failure;
    }

  message (/* verbosity */ 2,
	   "Getting current frame buffer parameters.");
  
  if (-1 == xioctl (fd, VIDIOC_G_FBUF, &fb))
    {
      saved_errno = errno;
      goto failure;
    }

  if (fb.capability & V4L2_FBUF_CAP_EXTERNOVERLAY)
    {
      message (/* verbosity */ 2,
	       "Genlock device, no setup necessary.\n");
      goto success;
    }

  memset (&fb, 0, sizeof (fb));

  fb.base		= (void *) buffer->base;

  fb.fmt.width		= buffer->format.width;
  fb.fmt.height		= buffer->format.height;

  pf = buffer->format.pixel_format;

  switch (Z_BYTE_ORDER)
    {
    case Z_LITTLE_ENDIAN:
      switch (pf->color_depth)
	{
	case  8:
	  fb.fmt.pixelformat = V4L2_PIX_FMT_HI240; /* XXX bttv only */
	  break;

	  /* Note defines and spec (0.4) are wrong: r <-> b,
	     RGB32 == A,R,G,B in bttv 0.9 unlike description in spec. */

	case 15:
	  fb.fmt.pixelformat = V4L2_PIX_FMT_RGB555;
	  break;

	case 16:
	  fb.fmt.pixelformat = V4L2_PIX_FMT_RGB565;
	  break;

	case 24:
	case 32:
	  if (24 == pf->bits_per_pixel)
	    fb.fmt.pixelformat = V4L2_PIX_FMT_BGR24;
	  else
	    fb.fmt.pixelformat = V4L2_PIX_FMT_BGR32;
	  break;
	}

      break;

    case Z_BIG_ENDIAN:
      switch (pf->color_depth)
	{
	case  8:
	  fb.fmt.pixelformat = V4L2_PIX_FMT_HI240;
	  break;

	case 15:
	  fb.fmt.pixelformat = V4L2_PIX_FMT_RGB555X;
	  break;

	case 16:
	  fb.fmt.pixelformat = V4L2_PIX_FMT_RGB565X;
	  break;

	case 24:
	case 32:
	  if (24 == pf->bits_per_pixel)
	    fb.fmt.pixelformat = V4L2_PIX_FMT_RGB24;
	  else
	    fb.fmt.pixelformat = V4L2_PIX_FMT_RGB32;
	  break;
	}

      break;
    }

  fb.fmt.bytesperline	= buffer->format.bytes_per_line[0];
  fb.fmt.sizeimage	= buffer->format.height * fb.fmt.bytesperline;

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

    result = ioctl (fd, VIDIOC_S_FBUF, &fb);
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

 success:
  device_close (device_log_fp, fd);
  return 0; /* success */

 failure:
  device_close (device_log_fp, fd);
  errno = saved_errno;
  return -1; /* failed */
}

#else /* !ENABLE_V4L */

int
setup_v4l25			(const char *		device_name,
				 const tv_overlay_buffer *buffer)
{
  message (/* verbosity */ 2,
	   "No V4L2 2.5 support compiled in.\n");

  errno = EINVAL;
  return -2; /* unknown API */
}

#endif /* !ENABLE_V4L */
