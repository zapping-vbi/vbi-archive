/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
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

/* $Id: v4l25.c,v 1.1 2003-02-16 17:56:40 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef ENABLE_V4L

#include <fcntl.h>
#include <sys/ioctl.h>

#include "zapping_setup_fb.h"

#include <linux/types.h>		/* __u32 etc */
#include <sys/time.h>			/* struct timeval */
#include "../common/videodev25.h"	/* V4L2 header file */

#define v4l25_ioctl(fd, cmd, arg) dev_ioctl (fd, cmd, arg, NULL)

int
setup_v4l25			(const char *		device_name)
{
  int fd;
  struct v4l2_capability cap;
  struct v4l2_framebuffer fb;

  message (2, "Opening video device.\n");

  if (-1 == (fd = dev_open (device_name, 81, O_RDWR)))
    return -1;

  message (2, "Querying device capabilities.\n");

  if (-1 == v4l25_ioctl (fd, VIDIOC_QUERYCAP, &cap))
    {
      errmsg ("VIDIOC_QUERYCAP ioctl failed,\n  probably not a V4L2 device");
      close (fd);
      return -1;
    }

  message (1, "Using V4L2 2.5 interface.\n");

  message (2, "Checking overlay capability.\n");

  if (!(cap.capabilities & V4L2_CAP_VIDEO_OVERLAY))
    {
      message (1, "Device '%s' does not support video overlay.\n", device_name);
      goto failure;
    }

  message (2, "Getting current frame buffer parameters.\n");

  if (-1 == v4l25_ioctl (fd, VIDIOC_G_FBUF, &fb))
    {
      errmsg ("VIDIOC_G_FBUF ioctl failed");
      goto failure;
    }

  if (fb.capability & V4L2_FBUF_CAP_EXTERNOVERLAY)
    {
      message (2, "Genlock device, mission accomplished.\n");
      close (fd);
      return 1;
    }

  memset (&fb, 0, sizeof (fb));

  fb.base		= (void *) addr;
  fb.fmt.width		= width;
  fb.fmt.height		= height;

  switch (depth)
    {
    case  8:
      fb.fmt.pixelformat = V4L2_PIX_FMT_HI240; /* XXX bttv only */
      break;

#if BYTE_ORDER == BIG_ENDIAN /* safe? */
    case 15: fb.fmt.pixelformat = V4L2_PIX_FMT_RGB555X; break;
    case 16: fb.fmt.pixelformat = V4L2_PIX_FMT_RGB565X; break;
    case 24: fb.fmt.pixelformat = V4L2_PIX_FMT_RGB24;   break;
    case 32: fb.fmt.pixelformat = V4L2_PIX_FMT_RGB32;   break;
#else
    case 15: fb.fmt.pixelformat = V4L2_PIX_FMT_RGB555;  break;
    case 16: fb.fmt.pixelformat = V4L2_PIX_FMT_RGB565;  break;
    case 24: fb.fmt.pixelformat = V4L2_PIX_FMT_BGR24;   break;
    case 32: fb.fmt.pixelformat = V4L2_PIX_FMT_BGR32;   break;
#endif
    }

  fb.fmt.bytesperline	= bpl;
  fb.fmt.sizeimage	= height * fb.fmt.bytesperline;

  message (2, "Setting new frame buffer parameters.\n");

  /*
   *  This ioctl is privileged because it sets up
   *  DMA to a random (video memory) address. 
   */
  {
    int success;
    int saved_errno;

    if (!restore_root_privileges (uid, euid))
      goto failure;

    success = v4l25_ioctl (fd, VIDIOC_S_FBUF, &fb);
    saved_errno = errno;

    if (!drop_root_privileges (uid, euid))
      ; /* ignore */

    if (success == -1)
      {
	errno = saved_errno;

        errmsg ("VIDIOC_S_FBUF ioctl failed");

        if (EPERM == saved_errno && ROOT_UID != euid)
	  message (1, "%s must be run as root, or marked as SUID root.\n",
		   program_invocation_short_name);
      failure:
	close (fd);
        return 0;
      }
  }

  close (fd);

  return 1;
}

#else /* !ENABLE_V4L */

int
setup_v4l25			(const char *		device_name)
{
  return -1;
}

#endif /* !ENABLE_V4L */
