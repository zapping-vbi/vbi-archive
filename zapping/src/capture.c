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
#include <tveng.h>
#include "zmisc.h"
#include "capture.h"

#ifndef DISABLE_X_EXTENSIONS
#ifdef HAVE_LIBXV
#define USE_XV 1
#endif
#endif

#ifdef USE_XV
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#endif /* USE_XV */

/* Some global stuff we need, see descriptions in main.c */
extern GList		*plugin_list;
static gboolean		have_xv = FALSE; /* Can we use the Xv extension? */

#ifdef USE_XV
static XvPortID		xvport; /* Xv port we will use */
static XvImage		*xvimage=NULL; /* Xv, shm image */
#endif

/*
 * Rescale xvimage to the given dimensions. What it actually does is
 * to delete the old xvimage and create a new one with the given
 * dimensions.
 */
static void
xv_rescale_image()
{
  //  if (xvimage)
  //    
}

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
		if (pImgFormats[k].id == 0x32315659) /* YVU420 */
		  break;

	      XvUngrabPort(dpy, xvport, CurrentTime);
	      xvport ++;
	    }
	}
	
      formats = pAdaptor->formats;
    }

  if (i == nAdaptors)
    goto error2;

  /* success */
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
  if (have_xv)
    XvUngrabPort(GDK_DISPLAY(), xvport, CurrentTime);
#endif
}

void
shutdown_capture(void)
{
  shutdown_xv();
}
