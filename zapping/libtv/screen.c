/*
 *  Copyright (C) 2001-2004 Michael H. Schimek
 *  Copyright (C) 2000-2003 Iñaki García Etxebarria
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: screen.c,v 1.1 2004-09-10 04:56:36 mschimek Exp $ */

#include <stdlib.h>		/* calloc(), free() */
#include <X11/Xlib.h>
#include <X11/Xutil.h>		/* XVisualInfo */	
#include "config.h"
#include "misc.h"
#include "screen.h"

/**
 * Finds the screen containing a point (width, height are 1), or most
 * of the points in a rectangle. If screens overlap and more than one
 * matches, returns the first matching in the list. If no screens
 * match, returns NULL.
 */
const tv_screen *
tv_screen_list_find		(const tv_screen *	list,
				 int			x,
				 int			y,
				 unsigned int		width,
				 unsigned int		height)
{
	const tv_screen *xs;
	unsigned int max;
	int x2;
	int y2;

	xs = NULL;

	max = 0;

	x2 = x + width;
	y2 = y + height;

	for (; list; list = list->next) {
		int lx2;
		int ly2;
		unsigned int n;

		lx2 = list->x + list->width;
		ly2 = list->y + list->height;

		if (x2 <= (int) list->x || x >= lx2)
			continue;

		if (y2 <= (int) list->y || y >= ly2)
			continue;

		n = (MIN (x2, lx2) - MAX (x, (int) list->x))
			* (MIN (y2, ly2) - MAX (y, (int) list->y));

		if (n > max) {
			xs = list;
			max = n;
		}
	}

	return xs;
}

void
tv_screen_list_delete		(tv_screen *		list)
{
	while (list) {
		tv_screen *xs;

		xs = list;
		list = xs->next;
		free (xs);
	}
}

#ifdef HAVE_XINERAMA_EXTENSION

/* Where is this documented? */
#include <X11/extensions/Xinerama.h>

static tv_bool
xinerama_query			(tv_screen **		list,
				 Display *		display)
{
	int event_base;
	int error_base;
	int major_version;
	int minor_version;
	XineramaScreenInfo *screen_info;
	int n_screens;
	tv_screen **next;
	int i;

	assert (NULL != list);
	assert (NULL != display);

	*list = NULL;

	if (!XineramaQueryExtension (display, &event_base, &error_base)) {
		printv ("Xinerama extension not available\n");
		return TRUE;
	}

	if (!XineramaQueryVersion (display, &major_version, &minor_version)) {
		printv ("Xinerama extension not usable\n");
		return FALSE;
	}

	printv ("Xinerama base %d, %d, version %d.%d\n",
		event_base, error_base,
		major_version, minor_version);

	if (1 != major_version)	{
		printv ("Unknown Xinerama version\n");
		return FALSE;
	}

	if (!XineramaIsActive (display)) {
		printv ("Xinerama inactive\n");
		return TRUE;
	}

	screen_info = XineramaQueryScreens (display, &n_screens);
	if (!screen_info) {
		printv ("XineramaQueryScreens() failed\n");
		return FALSE;
	}

	next = list;

	for (i = 0; i < n_screens; ++i) {
		tv_screen *xs;

		if (!(xs = calloc (1, sizeof (*xs)))) {
			tv_screen_list_delete (*list);
			*list = NULL;

			XFree (screen_info);

			return FALSE;
		}

		xs->screen_number = screen_info[i].screen_number;

		xs->x      = screen_info[i].x_org;
		xs->y      = screen_info[i].y_org;
		xs->width  = screen_info[i].width;
		xs->height = screen_info[i].height;

		*next = xs;
		next = &xs->next;
	}

	XFree (screen_info);

	return TRUE;
}

#else /* !HAVE_XINERAMA_EXTENSION */

static gboolean
xinerama_query			(tv_screen **	list,
				 Display *		display)
{
	assert (NULL != list);
	assert (NULL != display);

	printv ("Xinerama extension support not compiled in\n");

	*list = NULL;

	return TRUE;
}

#endif /* !HAVE_XINERAMA_EXTENSION */

#ifdef HAVE_DGA_EXTENSION

/* man XF86DGA */
#include <X11/extensions/xf86dga.h>

static tv_bool
dga_query			(tv_screen *		list,
				 Display *		display,
				 int			bpp_hint)
{
	int event_base;
	int error_base;
	int major_version;
	int minor_version;
	int flags;
	tv_pixel_format format;
	XVisualInfo templ;
	XVisualInfo *vi;
	int n_items;
	int i;
	XPixmapFormatValues *pf;

	assert (NULL != list);
	assert (NULL != display);

	if (!XF86DGAQueryExtension (display, &event_base, &error_base)) {
		printv ("DGA extension not available\n");
		return TRUE;
	}

	if (!XF86DGAQueryVersion (display, &major_version, &minor_version)) {
		printv ("DGA extension not usable\n");
		return FALSE;
	}

	printv ("DGA base %d, %d, version %d.%d\n",
		event_base, error_base,
		major_version, minor_version);

	if (1 != major_version
	    && 2 != major_version) {
		printv ("Unknown DGA version\n");
		return FALSE;
	}

	CLEAR (format);

	/* Screen from XOpenDisplay() name. */
	templ.screen = XDefaultScreen (display);

	vi = XGetVisualInfo (display, VisualScreenMask, &templ, &n_items);

	for (i = 0; i < n_items; ++i) {
		/* XXX We might support depth 7-8 after requesting a
		   TrueColor visual for the video window. */
		if (vi[i].class == TrueColor && vi[i].depth > 8) {
			if (0)
				printv ("DGA vi[%u] depth=%u\n",
					i, vi[i].depth);

			/* Note vi[i].bits_per_rgb is not bits_per_pixel, but
			   the number of significant bits per color component.
			   Usually 8 (as in 8 bit DAC for each of R, G, B). */

			/* vi[i].depth counts R, G and B bits, not alpha. */
			format.color_depth = vi[i].depth;

			format.mask.rgb.r = vi[i].red_mask;
			format.mask.rgb.g = vi[i].green_mask;
			format.mask.rgb.b = vi[i].blue_mask;

			break;
		}
	}

	XFree (vi);

	if (i >= n_items) {
		printv ("DGA: No appropriate X visual available\n");
		return TRUE;
	}

	switch (bpp_hint) {
	case 16:
	case 24:
	case 32:
		format.bits_per_pixel = bpp_hint;
		break;

	default: /* BPP heuristic */
		format.bits_per_pixel = 0;

		pf = XListPixmapFormats (display, &n_items);

		for (i = 0; i < n_items; ++i) {
			if (0)
				printv ("DGA pf[%u]: depth=%u bpp=%u\n",
					i,
					pf[i].depth,
					pf[i].bits_per_pixel);

			if ((unsigned int) pf[i].depth == format.color_depth) {
				format.bits_per_pixel = pf[i].bits_per_pixel;
				break;
			}
		}

		XFree (pf);

		if (i >= n_items) {
			printv ("DGA: Unknown frame buffer bits per pixel\n");
			return FALSE;
		}
	}

	format.big_endian = (MSBFirst == XImageByteOrder (display));

	tv_pixel_format_to_pixfmt (&format);

	if (TV_PIXFMT_UNKNOWN == format.pixfmt) {
		printv ("DGA: Unknown frame buffer format\n");
		return TRUE;
	}

	for (; list; list = list->next)	{
		int start;	/* physical address (?) */
		int width;	/* of root window in pixels */
		int banksize;	/* in bytes, usually video memory size */
		int memsize;	/* ? */
		unsigned int bits_per_line;

		/* Fortunately these functions refer to physical screens,
		   not the virtual Xinerama screen. */
		if (!XF86DGAQueryDirectVideo (display, list->screen_number,
					      &flags)) {
			printv ("DGA DirectVideo not available on screen %d\n",
				list->screen_number);
			continue;
		}

		if (!(flags & XF86DGADirectPresent)) {
			printv ("DGA DirectVideo not supported on screen %d\n",
				list->screen_number);
			continue;
		}

		if (!XF86DGAGetVideoLL (display, list->screen_number,
					&start, &width, &banksize, &memsize)) {
			printv ("XF86DGAGetVideoLL() failed on screen %d\n",
				list->screen_number);
			continue;
		}

		if (0)
			printv ("DGA screen %d: "
				"start=%p width=%d "
				"banksize=%d (0x%x) "
				"memsize=%d (0x%x)\n",
				list->screen_number,
				(void *) start, width,
				banksize, banksize,
				memsize, memsize);

		bits_per_line = list->width * format.bits_per_pixel;
		if (bits_per_line & 7) {
			printv ("DGA: Unknown frame buffer "
				"bits per pixel %u on screen %d\n",
				format.bits_per_pixel, list->screen_number);
			continue;
		}

		list->target.base = start;

		list->target.x = list->x;
		list->target.y = list->y;

		list->target.format.pixfmt = format.pixfmt;

		list->target.format.width = list->width;
		list->target.format.height = list->height;

		list->target.format.bytes_per_line = bits_per_line >>= 3;

		list->target.format.size =
			list->height * list->target.format.bytes_per_line;
	}

	return TRUE;
}

#else /* !HAVE_DGA_EXTENSION */

static gboolean
dga_query			(tv_screen *		list,
				 Display *		display,
				 int			bpp_hint)
{
	assert (NULL != list);
	assert (NULL != display);

	printv ("DGA extension support not compiled in\n");

	return FALSE;
}

#endif /* !HAVE_DGA_EXTENSION */

tv_screen *
tv_screen_list_new		(const char *		display_name,
				 int			bpp_hint)
{
	Display *display;
	tv_screen *list;

	assert (NULL != display_name);

	display = XOpenDisplay (display_name);

	if (NULL == display) {
		printv ("%s: Cannot open display '%s'\n",
			__FUNCTION__, display_name);
		return NULL;
	}

	list = NULL;

	if (!xinerama_query (&list, display)) {
		goto failure;
	}

	if (!list) {
		Window root;
		XWindowAttributes wa;

		/* Have no Xinerama. */

		if (!(list = calloc (1, sizeof (*list)))) {
			goto failure;
		}

		/* Root window of XDefaultScreen (display). */
		root = XDefaultRootWindow (display);

		XGetWindowAttributes (display, root, &wa);

		if (0)
			printv ("DGA root width=%u height=%u\n",
				wa.width, wa.height);

		/* Screen from XOpenDisplay() name. */
		list->screen_number = XDefaultScreen (display);

		list->x = 0;
		list->y = 0;
		list->width = wa.width;
		list->height = wa.height;
	}

	if (!dga_query (list, display, bpp_hint)) {
		goto failure;
	}

	XCloseDisplay (display);

	return list;

 failure:
	tv_screen_list_delete (list);

	XCloseDisplay (display);

	return NULL;
}
