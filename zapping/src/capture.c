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

/*
 * These routines handle the capture mode and the Xv stuff.
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#ifndef DISABLE_X_EXTENSIONS
#ifdef HAVE_LIBXV
#define USE_XV 1
#endif
#endif

#ifdef USE_XV
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#endif /* USE_XV */

#include <tveng.h>
#include "zmisc.h"
#include "x11stuff.h"
#include "capture.h"

/* Some global stuff we need, see descriptions in main.c */
extern GList		*plugin_list;
static gboolean		have_xv = FALSE; /* Can we use the Xv extension? */

#ifdef USE_XV
static XvPortID		xvport; /* Xv port we will use */
static XvImage		*xvimage=NULL; /* Xv, shm image */
static XShmSegmentInfo	shminfo; /* shared mem info for the xvimage */
#endif

static unsigned int
xv_mode_id(char * fourcc)
{
  return ((((__u32)(fourcc[0])<<0)|
	   ((__u32)(fourcc[1])<<8)|
	   ((__u32)(fourcc[2])<<16)|
	   ((__u32)(fourcc[3])<<24)));
}

#define YV12 xv_mode_id("YV12") /* YVU420 (planar, 12 bits) */
#define UYVY xv_mode_id("UYVY") /* UYUV (packed, 16 bits) */

#define XV_MODE UYVY /* preferred mode in Xv */

#ifdef USE_XV
static void
xv_image_delete(void)
{
  if (!xvimage)
    return;

  XShmDetach(GDK_DISPLAY(), &shminfo);
  XFree(xvimage);
  shmdt(shminfo.shmaddr);

  xvimage = NULL;
}

/*
 * Rescale xvimage to the given dimensions. What it actually does is
 * to delete the old xvimage and create a new one with the given
 * dimensions.
 */
static void
xv_image_rescale(gint w, gint h)
{
  if ((xvimage) && (xvimage->width == w) && (xvimage->height == h))
    return; /* nothing to be done */

  if (xvimage)
    xv_image_delete();

  memset(&shminfo, 0, sizeof(XShmSegmentInfo));
  xvimage = XvShmCreateImage(GDK_DISPLAY(), xvport, XV_MODE, NULL,
			     w, h, &shminfo);
  if (!xvimage)
    return;
  
  shminfo.shmid = shmget(IPC_PRIVATE, xvimage->data_size, IPC_CREAT | 0777);
  shminfo.shmaddr = xvimage->data = shmat(shminfo.shmid, 0, 0);
  shmctl(shminfo.shmid, IPC_RMID, 0); /* remove when we terminate */

  shminfo.readOnly = False;

  XShmAttach(GDK_DISPLAY(), &shminfo);
  XSync(GDK_DISPLAY(), False);
}
#endif /* USE_XV */

/* Checks for the Xv extension, and sets have_xv accordingly */
static void
startup_xv(void)
{
#ifdef USE_XV
  Display *dpy=GDK_DISPLAY();
  Window root_window = GDK_ROOT_WINDOW();
  unsigned int version, revision, major_opcode, event_base,
    error_base;
  int i, j=0, k=0;
  int nAdaptors;
  XvAdaptorInfo *pAdaptors, *pAdaptor;
  XvFormat *formats;
  XvImageFormatValues *pImgFormats=NULL;
  int nImgFormats;

  if (Success != XvQueryExtension(dpy, &version, &revision,
				  &major_opcode, &event_base,
				  &error_base))
    goto error1;

  if (version < 2 || revision < 2)
    goto error1;

  XvQueryAdaptors(dpy, root_window, &nAdaptors, &pAdaptors);

  for (i=0; i<nAdaptors; i++)
    {
      pAdaptor = pAdaptors + i;
      /* print some info about this adaptor */
      printv("%d) Adaptor info:\n"
	     "	- Base port id:		0x%x\n"
	     "	- Number of ports:	%d\n"
	     "	- Type:			%d\n"
	     "	- Name:			%s\n"
	     "	- Number of formats:	%d\n",
	     i, (int)pAdaptor->base_id, (int) pAdaptor->num_ports,
	     (int)pAdaptor->type, pAdaptor->name,
	     (int)pAdaptor->num_formats);

      if ((pAdaptor->type & XvInputMask) &&
	  (pAdaptor->type & XvImageMask))
	{ /* Image adaptor, check if some port fits our needs */
	  xvport = pAdaptor->base_id;
	  for (j=0; j<pAdaptor->num_ports;j++)
	    {
	      pImgFormats = XvListImageFormats(dpy, xvport,
					       &nImgFormats);
	      if (!pImgFormats)
		continue;

	      if (Success != XvGrabPort(dpy, xvport, CurrentTime))
		continue;

	      for (k=0; k<nImgFormats; k++)
		if (pImgFormats[k].id == XV_MODE)
		  goto adaptor_found;

	      XvUngrabPort(dpy, xvport, CurrentTime);
	      xvport ++;
	    }
	}
	
      formats = pAdaptor->formats;
    }

  if (i == nAdaptors)
    goto error2;

  /* success */
 adaptor_found:
  printv("Adaptor #%d, image format #%d (0x%x), port #%d chosen\n",
	 i, k, pImgFormats[k].id, j);
  have_xv = TRUE;
  XvFreeAdaptorInfo(pAdaptors);
  return;

 error2:
  XvFreeAdaptorInfo(pAdaptors);

 error1:
  have_xv = FALSE;

#else
  have_xv = FALSE;
#endif
}

gboolean
startup_capture(GtkWidget * widget)
{
  startup_xv();

  return TRUE;
}

static void
shutdown_xv(void)
{
#ifdef USE_XV
  if (xvimage)
    xv_image_delete();

  if (have_xv)
    XvUngrabPort(GDK_DISPLAY(), xvport, CurrentTime);
#endif
}

void
shutdown_capture(void)
{
  shutdown_xv();
}

void
capture_process_frame(GtkWidget * widget, tveng_device_info * info)
{
#ifdef USE_XV
  gint w, h;

  gdk_window_get_size(widget->window, &w, &h);

  if (have_xv)
    {
      xv_image_rescale(info->format.width, info->format.height);

      if (-1 == tveng_read_frame(xvimage->data, xvimage->data_size,
				 50, info))
	{
	  g_warning("cap: read(): %s\n", info->error);
	  usleep(50);
	  return;
	}

      XvShmPutImage(GDK_DISPLAY(), xvport,
		    GDK_WINDOW_XWINDOW(widget->window),
		    GDK_GC_XGC(widget->style->white_gc), xvimage,
		    0, 0, xvimage->width, xvimage->height, /* source */
		    0, 0, w, h, /* dest */
		    False /* wait for completition */);
    }
#endif
}

gint
capture_start(GtkWidget * window, tveng_device_info *info)
{
  enum tveng_frame_pixformat pixformat;

  if (have_xv)
    pixformat = TVENG_PIX_UYVY;
  else
    pixformat =
	zmisc_resolve_pixformat(x11_get_bpp(), x11_get_byte_order());

  printv("cap: setting format %d\n", pixformat);

  info->format.pixformat = pixformat;
  if (tveng_set_capture_format(info) == -1)
    {
      g_warning("setting cap: %s", info->error);
      return -1;
    }
  if (info->format.pixformat != pixformat)
    {
      g_warning("Failed to set valid pixformat: got %d",
		info->format.pixformat);
      return -1;
    }

  /* OK, startup done, try to start capturing */
  if (-1 == tveng_start_capturing(info))
    {
      g_warning("Couldn't start capturing: %s", info->error);
      return -1;
    }

  /* Capture started correctly */
  return 0;
}

void
capture_stop(tveng_device_info *info)
{
  /* nothing to be done in here */
}
