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

/* $Id: v4l2.c,v 1.7 2004-11-03 06:37:31 mschimek Exp $ */

#include "../config.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "zapping_setup_fb.h"

#ifdef ENABLE_V4L

#include <assert.h>

#include "../common/videodev2.h" /* V4L2 header file */
#include "../common/_videodev2.h"

#define v4l2_ioctl(fd, cmd, arg)					\
  (IOCTL_ARG_TYPE_CHECK_ ## cmd (arg),					\
   device_ioctl (log_fp, fprint_ioctl_arg, fd, cmd, arg))

int
setup_v4l2			(const char *		device_name,
				 const tv_overlay_buffer *buffer)
{
  int fd;
  struct v4l2_capability cap;

  message (2, "Opening video device.\n");

  if (-1 == (fd = device_open_safer (device_name, 81, O_RDWR)))
    return -1;

  message (2, "Querying device capabilities.\n");

  if (0 == v4l2_ioctl (fd, VIDIOC_QUERYCAP, &cap))
    {
      errmsg ("VIDIOC_QUERYCAP ioctl failed,\n  probably not a V4L2 device");
      close (fd);
      return -1;
    }

  /* V4L2 0.20 is obsolete, superseded by V4L2 of Linux 2.6. */

  errmsg ("V4L2 0.20 API not supported");

  close (fd);

  return 0; /* failed */
}

#else /* !ENABLE_V4L */

int
setup_v4l2			(const char *		device_name,
				 const tv_overlay_buffer *buffer)
{
  return -1; /* try other */
}

#endif /* !ENABLE_V4L */
