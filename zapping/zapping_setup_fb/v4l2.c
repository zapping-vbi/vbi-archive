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

/* $Id: v4l2.c,v 1.2 2003-01-25 23:39:58 mschimek Exp $ */

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

#include "../common/videodev2.h" /* V4L2 header file */

#define v4l2_ioctl(fd, cmd, arg) dev_ioctl (fd, cmd, arg, fprintf_v4l2_ioctl)

static void
fprintf_v4l2_ioctl		(FILE *			fp,
				 int			cmd,
				 void *			arg)
{
  switch (cmd)
    {
      CASE (VIDIOC_G_FBUF)
      CASE (VIDIOC_S_FBUF)
	{
	  struct v4l2_framebuffer *t = arg;

	  fprintf (fp, "capability=");

	  fprintf_symbolic (fp, 0, (unsigned long) t->capability,
			    "EXTERNOVERLAY", V4L2_FBUF_CAP_EXTERNOVERLAY,
			    "CHROMAKEY", V4L2_FBUF_CAP_CHROMAKEY,
			    "CLIPPING", V4L2_FBUF_CAP_CLIPPING,
			    "SCALEUP", V4L2_FBUF_CAP_SCALEUP,
			    "SCALEDOWN", V4L2_FBUF_CAP_SCALEDOWN,
			    "BITMAP_CLIPPING", V4L2_FBUF_CAP_BITMAP_CLIPPING,
			    0);
	  
	  fprintf (fp, " flags=");
	  
	  fprintf_symbolic (fp, 2, (unsigned long) t->flags,
			    "PRIMARY", V4L2_FBUF_FLAG_PRIMARY,
			    "OVERLAY", V4L2_FBUF_FLAG_OVERLAY,
			    "CHROMAKEY", V4L2_FBUF_FLAG_CHROMAKEY,
			  0);
	  
	  fprintf (fp, " base[]={%p,%p,%p} fmt.width=%lu "
		   "fmt.height=%lu fmt.depth=%lu "
		   "fmt.pixelformat=0x%lx fmt.flags=",
		   t->base[0], t->base[1],
		   t->base[2], (unsigned long) t->fmt.width,
		   (unsigned long) t->fmt.height, (unsigned long) t->fmt.depth,
		   (unsigned long) t->fmt.pixelformat);
	  
	  fprintf_symbolic (fp, 2, (unsigned long) t->fmt.flags,
			    "COMPRESSED", V4L2_FMT_FLAG_COMPRESSED,
			    "BYTESPERLINE", V4L2_FMT_FLAG_BYTESPERLINE,
			    "INTERLACED", V4L2_FMT_FLAG_INTERLACED,
			    "TOPFIELD", V4L2_FMT_FLAG_TOPFIELD,
			    "BOTFIELD", V4L2_FMT_FLAG_BOTFIELD,
			    0);

	  fprintf (fp, " fmt.bytesperline=%lu fmt.sizeimage=%lu fmt.priv=%lu",
		   (unsigned long) t->fmt.bytesperline,
		   (unsigned long) t->fmt.sizeimage,
		   (unsigned long) t->fmt.priv);
	  break;
	}

      CASE (VIDIOC_QUERYCAP)
	{
	  struct v4l2_capability *t = arg;

	  fprintf (fp, "name=\"%.*s\" type=",
		   32, t->name);

	  fprintf_symbolic (fp, 1, (unsigned long) t->type,
			    "CAPTURE", V4L2_TYPE_CAPTURE,
			    "CODEC", V4L2_TYPE_CODEC,
			    "OUTPUT", V4L2_TYPE_OUTPUT,
			    "FX", V4L2_TYPE_FX,
			    "VBI", V4L2_TYPE_VBI,
			    "VTR", V4L2_TYPE_VTR,
			    "VTX", V4L2_TYPE_VTX,
			    "RADIO", V4L2_TYPE_RADIO,
			    "VBI_INPUT", V4L2_TYPE_VBI_INPUT,
			    "VBI_OUTPUT", V4L2_TYPE_VBI_OUTPUT,
			    "PRIVATE", V4L2_TYPE_PRIVATE,
			    0);
	
	  fprintf (fp, " inputs=%ld outputs=%ld "
		   "audios=%ld maxwidth=%ld "
		   "maxheight=%ld minwidth=%ld "
		   "minheight=%ld maxframerate=%ld "
		   "flags=",
		   (long) t->inputs, (long) t->outputs, 
		   (long) t->audios, (long) t->maxwidth, 
		   (long) t->maxheight, (long) t->minwidth, 
		   (long) t->minheight, (long) t->maxframerate);
	  
	  fprintf_symbolic (fp, 2, (unsigned long) t->flags,
			    "READ", V4L2_FLAG_READ,
			    "WRITE", V4L2_FLAG_WRITE,
			    "STREAMING", V4L2_FLAG_STREAMING,
			    "PREVIEW", V4L2_FLAG_PREVIEW,
			    "SELECT", V4L2_FLAG_SELECT,
			    "TUNER", V4L2_FLAG_TUNER,
			    "MONOCHROME", V4L2_FLAG_MONOCHROME,
			    "DATA_SERVICE", V4L2_FLAG_DATA_SERVICE,
			    0);
	  break;
	}

      default:
	fprintf (fp, "<unknown cmd 0x%x>", cmd);
    }
}

int
setup_v4l2			(const char *		device_name)
{
  int fd;
  struct v4l2_capability cap;
  struct v4l2_framebuffer fb;

  message (2, "Opening video device.\n");

  if (-1 == (fd = dev_open (device_name, 81, O_RDWR)))
    return -1;

  message (2, "Querying device capabilities.\n");

  if (-1 == v4l2_ioctl (fd, VIDIOC_QUERYCAP, &cap))
    {
      errmsg ("VIDIOC_QUERYCAP ioctl failed,\n  probably not a V4L2 device");
      close (fd);
      return -1;
    }

  message (1, "Using V4L2 interface.\n");

  message (2, "Checking overlay capability.\n");

  if (V4L2_TYPE_CAPTURE != cap.type
      || !(V4L2_FLAG_PREVIEW & cap.flags))
    {
      message (1, "Device '%s' does not support video overlay.\n", device_name);
      goto failure;
    }

  message (2, "Getting current frame buffer parameters.\n");

  if (-1 == v4l2_ioctl (fd, VIDIOC_G_FBUF, &fb))
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

  fb.base[0]		= (void *) addr;
  fb.base[1]		= (void *) addr;
  fb.base[2]		= (void *) addr;
  fb.fmt.width		= width;
  fb.fmt.height		= height;
  fb.fmt.depth		= bpp;

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

  fb.fmt.flags		= V4L2_FMT_FLAG_BYTESPERLINE; 
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

    success = v4l2_ioctl (fd, VIDIOC_S_FBUF, &fb);
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
setup_v4l2			(const char *		device_name)
{
  return -1;
}

#endif /* !ENABLE_V4L */
