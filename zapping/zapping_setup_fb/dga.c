/*
 * Zapping (TV viewer for the Gnome Desktop)
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

/* $Id: dga.c,v 1.1 2003-01-21 05:18:39 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "zapping_setup_fb.h"

#ifdef DISABLE_X_EXTENSIONS

int
query_dga			(Display *		display,
				 int			screen,
				 int			bpp_arg)
{
  message (1, "Not compiled with DGA support.\n");

  return FALSE;
}

#else /* !DISABLE_X_EXTENSIONS */

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xfuncs.h>
#include <X11/extensions/xf86dga.h>
#include <X11/Xutil.h>

int
query_dga			(const char *		display_name,
				 int			bpp_arg)
{
  Display *display;
  int screen;
  int event_base, error_base;
  int major_version, minor_version;
  int flags;

  message (2, "Opening X display.\n");

  if (NULL == (display = XOpenDisplay (display_name)))
    {
      message (1, "Cannot open display '%s'.", display_name);
      return -1;
    }

  message (2, "Getting default screen for the given display.\n");

  screen = XDefaultScreen (display);

  if (!XF86DGAQueryExtension (display, &event_base, &error_base))
    {
      message (1, "DGA extension not available.\n");
      goto failure;
    }

  if (!XF86DGAQueryVersion (display, &major_version, &minor_version))
    {
      message (1, "XF86DGAQueryVersion() failed.\n");
      goto failure;
    }

  if (!XF86DGAQueryDirectVideo (display, screen, &flags))
    {
      message (1, "XF86DGAQueryDirectVideo() failed.\n");
      goto failure;
    }

  if (!(flags & XF86DGADirectPresent))
    {
      message (1, "DirectVideo not available.\n");
      goto failure;
    }

  message (1, "Querying frame buffer parameters from DGA.\n");

  {
    int start, width, banksize, memsize;

    if (!XF86DGAGetVideoLL (display, screen,
			    &start, &width, &banksize, &memsize))
      {
	message (1, "XF86DGAGetVideoLL() failed.\n");
	goto failure;
      }

    addr = start;
  }

#if 0

  if (!XF86DGAGetViewPortSize (display, screen, &vp_width, &vp_height))
    {
      message (1, "XF86DGAGetViewPortSize() failed.\n");
      goto failure;
    }

#endif

  {
    Window root;
    XWindowAttributes wts;

    root = DefaultRootWindow(display);

    XGetWindowAttributes(display, root, &wts);

    width  = wts.width;
    height = wts.height;
  }

  message (2, "Heuristic bpp search.\n");

  if (bpp_arg == -1)
    {
      XVisualInfo *info, templ;
      XPixmapFormatValues *pf;
      int found, v, i, n;

      bpp = 0;

      templ.screen = screen;

      info = XGetVisualInfo (display, VisualScreenMask, &templ, &found);

      for (i = 0, v = -1; v == -1 && i < found; i++)
        if (info[i].class == TrueColor && info[i].depth >= 15)
	  v = i;

      if (-1 == v)
        {
          message (1, "No appropriate X visual available.\n");
	  goto failure;
        }

      /* get depth + bpp (heuristic) */
      pf = XListPixmapFormats (display, &n);

      for (i = 0; i < n; i++)
        if (pf[i].depth == info[v].depth)
	  {
	    bpp = pf[i].bits_per_pixel;
	    break;
          }

      if (0 == bpp)
        {
          message (1, "Cannot figure out framebuffer depth,\n"
		   "please use option --bpp\n");
	  goto failure;
        }
    }
  else
    {
      bpp = bpp_arg;
    }

  depth = bpp;

  bpl = (width * bpp + 7) >> 3;

  message (2, "DGA info:\n");
  message (2, " - Version      : %d.%d\n", major_version, minor_version);
  /* message (2, " - Viewport     : %d x %d\n", vp_width, vp_height); */
  message (2, " - Frame buffer : %p, bpl %u\n", (void *) addr, bpl);
  message (2, " - FB size      : %u x %u\n", width, height);
  message (2, " - Screen bpp   : depth %u, bpp %u\n", depth, bpp);

  return TRUE;

 failure:
  if (display != 0)
    XCloseDisplay (display);

  return FALSE;
}

#endif /* !DISABLE_X_EXTENSIONS */
