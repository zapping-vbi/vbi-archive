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

/* $Id: v4l.c,v 1.3 2004-04-19 15:24:16 mschimek Exp $ */

#include "../config.h"

#include "zapping_setup_fb.h"

#ifdef ENABLE_V4L

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "../common/videodev.h"
#include "../common/_videodev.h"

#define v4l_ioctl(fd, cmd, arg)						\
  (IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
   device_ioctl (log_fp, fprintf_ioctl_arg, fd, cmd, arg))

int
setup_v4l			(const char *		device_name,
				 x11_dga_parameters *	dga)
{
  int fd;
  struct video_capability caps;
  struct video_buffer fb;

  message (2, "Opening video device.\n");

  if (-1 == (fd = device_open_safer (device_name, 81, O_RDWR)))
    return -1;

  message (2, "Querying device capabilities.\n");

  if (-1 == v4l_ioctl (fd, VIDIOCGCAP, &caps))
    {
      errmsg ("VIDIOCGCAP ioctl failed,\n  probably not a V4L device");
      close (fd);
      return -1;
    }

  message (1, "Using V4L interface.\n");

  message (2, "Checking overlay capability.\n");

  if (!(caps.type & VID_TYPE_OVERLAY))
    {
      message (1, "Device '%s' does not support video overlay.\n", device_name);
      goto failure;
    }

  message (2, "Getting current FB parameters.\n");

  if (-1 == v4l_ioctl (fd, VIDIOCGFBUF, &fb))
    {
      errmsg ("VIDIOCGFBUF ioctl failed");
      goto failure;
    }

  fb.base		= dga->base;
  fb.width		= dga->width;
  fb.height		= dga->height;

  if (dga->bits_per_pixel == 32)
    fb.depth		= 32; /* depth 24 bpp 32 */
  else
    fb.depth		= dga->depth; /* 15, 16, 24 */

  fb.bytesperline	= dga->bytes_per_line;

  message (2, "Setting new FB parameters.\n");

  /*
   *  This ioctl is privileged because it sets up
   *  DMA to a random (video memory) address. 
   */
  {
    int success;
    int saved_errno;

    if (!restore_root_privileges ())
      goto failure;

    success = ioctl (fd, VIDIOCSFBUF, &fb);
    saved_errno = errno;

    drop_root_privileges ();

    if (-1 == success)
      {
	errno = saved_errno;

        errmsg ("VIDIOCSFBUF ioctl failed");

        if (EPERM == saved_errno && ROOT_UID != euid)
	  privilege_hint ();

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
setup_v4l			(const char *		device_name,
				 x11_dga_parameters *	dga)
{
  return -1;
}

#endif /* !ENABLE_V4L */
