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

/* $Id: v4l.c,v 1.1.2.1 2003-01-21 05:23:30 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef ENABLE_V4L

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "zapping_setup_fb.h"

#include "../common/videodev.h" /* V4L header file */

#define v4l_ioctl(fd, cmd, arg) dev_ioctl (fd, cmd, arg, fprintf_v4l_ioctl)

static void
fprintf_v4l_ioctl		(FILE *			fp,
				 int			cmd,
				 void *			arg)
{
  switch (cmd)
    {
      CASE (VIDIOCGCAP)
	{
	  struct video_capability *t = arg;
	
	  fprintf (fp, "name=\"%.*s\" type=",
		   32, t->name);
	
	  fprintf_symbolic (fp, 0, (unsigned long) t->type,
			    "CAPTURE", VID_TYPE_CAPTURE,
			    "TUNER", VID_TYPE_TUNER,
			    "TELETEXT", VID_TYPE_TELETEXT,
			    "OVERLAY", VID_TYPE_OVERLAY,
			    "CHROMAKEY", VID_TYPE_CHROMAKEY,
			    "CLIPPING", VID_TYPE_CLIPPING,
			    "FRAMERAM", VID_TYPE_FRAMERAM,
			    "SCALES", VID_TYPE_SCALES,
			    "MONOCHROME", VID_TYPE_MONOCHROME,
			    "SUBCAPTURE", VID_TYPE_SUBCAPTURE,
			    "MPEG_DECODER", VID_TYPE_MPEG_DECODER,
			    "MPEG_ENCODER", VID_TYPE_MPEG_ENCODER,
			    "MJPEG_DECODER", VID_TYPE_MJPEG_DECODER,
			    "MJPEG_ENCODER", VID_TYPE_MJPEG_ENCODER,
			    0);
	
	  fprintf (fp, " channels=%ld audios=%ld "
		   "maxwidth=%ld maxheight=%ld "
		   "minwidth=%ld minheight=%ld",
		   (long) t->channels, (long) t->audios, 
		   (long) t->maxwidth, (long) t->maxheight, 
		   (long) t->minwidth, (long) t->minheight);
	  break;
	}

      CASE (VIDIOCGFBUF)
      CASE (VIDIOCSFBUF)
	{
	  struct video_buffer *t = arg;
	
	  fprintf (fp, "base=%p height=%ld "
		   "width=%ld depth=%ld "
		   "bytesperline=%ld",
		   t->base, (long) t->height, 
		   (long) t->width, (long) t->depth, 
		   (long) t->bytesperline);
	  break;
	}

      default:
        fprintf (fp, "<unknown cmd 0x%x>", cmd);
    }
}

int
setup_v4l			(const char *		device_name)
{
  int fd;
  struct video_capability caps;
  struct video_buffer fb;

  message (2, "Opening video device.\n");

  if (-1 == (fd = dev_open (device_name, 81, O_RDWR)))
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

  fb.base		= (void *) addr;
  fb.width		= width;
  fb.height		= height;
  fb.depth		= bpp;
  fb.bytesperline	= bpl;

  message (2, "Setting new FB parameters.\n");

  /*
   *  This ioctl is privileged because it sets up
   *  DMA to a random (video memory) address. 
   */
  {
    int success;
    int saved_errno;

    if (!restore_root_privileges (uid, euid))
      goto failure;

    success = v4l_ioctl (fd, VIDIOCSFBUF, &fb);
    saved_errno = errno;

    if (!drop_root_privileges (uid, euid))
      ; /* ignore */

    if (-1 == success)
      {
	errno = saved_errno;

        errmsg ("VIDIOCSFBUF ioctl failed");

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
setup_v4l			(const char *		device_name)
{
  return -1;
}

#endif /* !ENABLE_V4L */
