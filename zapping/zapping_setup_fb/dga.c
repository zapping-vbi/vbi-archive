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

/* $Id: dga.c,v 1.1.2.4 2003-10-07 18:38:15 mschimek Exp $ */

/* XXX Verbatim copy from ../src/x11_stuff, keep in sync.
   One day this will be part of libtveng and we can just link. */

#include "../config.h"

#include "zapping_setup_fb.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xfuncs.h>
#include <X11/Xutil.h>

#define CLEAR(var) memset (&(var), 0, sizeof (var))

#define printv(template, args...) message (1, template , ##args)

#ifdef HAVE_DGA_EXTENSION

#ifndef X11STUFF_DGA_DEBUG
#define X11STUFF_DGA_DEBUG 0
#endif

/* man XF86DGA */
#include <X11/extensions/xf86dga.h>

gboolean
x11_dga_query			(x11_dga_parameters *	par,
				 const char *		display_name,
				 int			bpp_hint)
{
  x11_dga_parameters param;
  Display *display;
  int event_base, error_base;
  int major_version, minor_version;
  int screen;
  int flags;

  if (par)
    CLEAR (*par);
  else
    par = &param;

  if (display_name)
    {
      display = XOpenDisplay (display_name);

      if (NULL == display)
	{
	  printv ("Cannot open display '%s'\n", display_name);
	  return FALSE;
	}
    }
  else
    {
	  printv ("Cannot open display ''\n");
	  return FALSE;

      //      display = GDK_DISPLAY ();
    }

  if (!XF86DGAQueryExtension (display, &event_base, &error_base))
    {
      printv ("DGA extension not available\n");
      return FALSE;
    }

  if (!XF86DGAQueryVersion (display, &major_version, &minor_version))
    {
      printv ("DGA extension not usable\n");
      return FALSE;
    }

  printv ("DGA base %d, %d, version %d.%d\n",
	  event_base, error_base,
	  major_version, minor_version);

  if (major_version != 1
      && major_version != 2)
    {
      printv ("Unknown DGA version\n");
      return FALSE;
    }

  screen = XDefaultScreen (display);

  if (!XF86DGAQueryDirectVideo (display, screen, &flags))
    {
      printv ("DGA DirectVideo not available\n");
      return FALSE;
    }

  if (!(flags & XF86DGADirectPresent))
    {
      printv ("DGA DirectVideo not supported\n");
      return FALSE;
    }

  {
    int start;		/* physical address (?) */
    int width;		/* of root window in pixels */
    int banksize;	/* in bytes, usually video memory size */
    int memsize;	/* ? */

    if (!XF86DGAGetVideoLL (display, screen,
			    &start, &width, &banksize, &memsize))
      {
	printv ("XF86DGAGetVideoLL() failed\n");
	return FALSE;
      }

    if (X11STUFF_DGA_DEBUG)
      printv ("DGA start=%p width=%d banksize=%d "
	      "(0x%x) memsize=%d (0x%x)\n",
	      (void *) start, width, banksize, banksize,
	      memsize, memsize);

    par->base = (void *) start;
  }

  {
    Window root;
    XWindowAttributes wts;

    root = DefaultRootWindow (display);

    XGetWindowAttributes (display, root, &wts);

    if (X11STUFF_DGA_DEBUG)
      printv ("DGA root width=%u height=%u\n",
	      wts.width, wts.height);

    par->width  = wts.width;
    par->height = wts.height;
  }

  {
    XVisualInfo *info, templ;
    int i, nitems;

    templ.screen = screen;

    info = XGetVisualInfo (display, VisualScreenMask, &templ, &nitems);

    for (i = 0; i < nitems; i++)
      if (info[i].class == TrueColor && info[i].depth > 8)
	{
	  if (X11STUFF_DGA_DEBUG)
	    printv ("DGA vi[%u] depth=%u\n", i, info[i].depth);

	  par->depth = info[i].depth;
	  break;
	}

    XFree (info);

    if (i >= nitems)
      {
	printv ("DGA: No appropriate X visual available\n");
	CLEAR (*par);
	return FALSE;
      }

    switch (bpp_hint)
      {
      case 16:
      case 24:
      case 32:
	par->bits_per_pixel = bpp_hint;
	break;

      default:
	{
	  XPixmapFormatValues *pf;
	  int i, count;

	  /* BPP heuristic */

	  par->bits_per_pixel = 0;

	  pf = XListPixmapFormats (display, &count);

	  for (i = 0; i < count; i++)
	    {
	      if (X11STUFF_DGA_DEBUG)
		printv ("DGA pf[%u]: depth=%u bpp=%u\n",
			i, pf[i].depth, pf[i].bits_per_pixel);

	      if (pf[i].depth == par->depth)
		{
		  par->bits_per_pixel = pf[i].bits_per_pixel;
		  break;
		}
	    }

	  XFree (pf);

	  if (i >= count)
	    {
	      printv ("DGA: Unknown frame buffer bits per pixel\n");
	      CLEAR (*par);
	      return FALSE;
	    }
	}
      }

    /* XImageByteOrder() ? */
  }

  par->bytes_per_line = par->width * par->bits_per_pixel;

  if (par->bytes_per_line & 7)
    {
      printv ("DGA: Unknown frame buffer bits per pixel\n");
      CLEAR (*par);
      return FALSE;
    }

  par->bytes_per_line >>= 3;

  par->size = par->height * par->bytes_per_line;

  return TRUE;
}

#else /* !HAVE_DGA_EXTENSION */

gboolean
x11_dga_query			(x11_dga_parameters *	par,
				 const char *		display_name,
				 int			bpp_hint)
{
  printv ("DGA extension support not compiled in\n");

  if (par)
    CLEAR (*par);

  return FALSE;
}

#endif /* !HAVE_DGA_EXTENSION */
