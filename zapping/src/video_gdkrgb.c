/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2002 Iñaki García Etxebarria
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

/**
 * GdkRGB backend. GdkRGB is a nice set of routines inside Gdk that
 * provide very fast blitters for rgb data. It's wrapped by GdkPixbuf,
 * which makes it behave similar to a GdkImage, but we will use it
 * directly in here.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <common/math.h>
#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zconf.h"
#include "globals.h"
#include "zimage.h"
#include "zmisc.h"
#include "capture.h"
#include "x11stuff.h"

static GdkWindow	*window = NULL;
static GdkGC		*gc = NULL, *black_gc = NULL;

struct _zimage_private {
  guchar	*data;
};


static zimage*
image_new (enum tveng_frame_pixformat fmt, gint w, gint h)
{
  zimage *image = zimage_create_object ();
  zimage_private *pimage = image->priv =
    g_malloc0 (sizeof (zimage_private));
  gint bpp = 0xdeadbeef;

  switch (fmt)
    {
    case TVENG_PIX_RGB24:
      bpp = 3;
      break;
    case TVENG_PIX_RGB32:
      bpp = 4;
      break;
    default:
      g_assert_not_reached ();
      break;
    }

  pimage->data = g_malloc (bpp * w * h);

  image->fmt.width = w;
  image->fmt.height = h;
  image->fmt.pixformat = fmt;
  image->fmt.bytesperline = w * bpp;
  image->fmt.bpp = bpp;
  image->fmt.depth = bpp*8;
  image->fmt.sizeimage = w * h * bpp;

  image->data.linear.data = pimage->data;
  image->data.linear.stride = w * bpp;

  return image;
}

/* Clear canvas minus the image */
static void
clear_canvas (GdkWindow *canvas, gint w, gint h, gint iw, int ih)
{
  gint y  = (h - ih) >> 1;
  gint h2 = (h + ih) >> 1;
  gint x  = (w - iw) >> 1;
  gint w2 = (w + iw) >> 1;

  if (y > 0)
    gdk_draw_rectangle (canvas, black_gc, TRUE,
			0, 0, w, y);
  if (h2 > 0)
    gdk_draw_rectangle (canvas, black_gc, TRUE,
			0, y + ih, w, h2);
  if (x > 0)
    gdk_draw_rectangle (canvas, black_gc, TRUE,
			0, y, x, ih);
  if (w2 > 0)
    gdk_draw_rectangle (canvas, black_gc, TRUE,
			x + iw, y, w2, ih);
}

static void
image_put (zimage *image, gint w, gint h)
{
  zimage_private *pimage = image->priv;
  gint iw = image->fmt.width, ih = image->fmt.height;

  g_assert (window != NULL);

  clear_canvas (window, w, h, iw, ih);
  switch (image->fmt.pixformat)
    {
    case TVENG_PIX_RGB24:
      gdk_draw_rgb_image (window, gc,
			  (w - iw)/2, (h - ih)/2, iw, ih,
			  GDK_RGB_DITHER_NORMAL, pimage->data,
			  image->data.linear.stride);
      break;
    case TVENG_PIX_RGB32:
      gdk_draw_rgb_32_image (window, gc,
			     (w - iw)/2, (h - ih)/2, iw, ih,
			     GDK_RGB_DITHER_NORMAL, pimage->data,
			     image->data.linear.stride);
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
image_destroy (zimage *image)
{
  zimage_private *pimage = image->priv;

  g_free (pimage->data);
  g_free (pimage);
}

static void
set_destination (GdkWindow *_w, GdkGC *_gc,
		 tveng_device_info *info)
{
  GdkColor black = {0, 0, 0, 0};

  /* set_ and _unset will be called multiple times */
  if ((window == _w) && (gc == _gc))
    return;

  window = _w;
  gc = _gc;

  if (black_gc)
    g_object_unref (G_OBJECT (black_gc));

  black_gc = gdk_gc_new (window);
  gdk_gc_copy (black_gc, gc);

  gdk_gc_set_fill (black_gc, GDK_SOLID);
  gdk_gc_set_rgb_fg_color (gc, &black);
  gdk_gc_set_rgb_bg_color (gc, &black);
}

static void
unset_destination(tveng_device_info *info)
{
  /* see comment in set_destination */
  if ((!window) && (!gc))
    return;

  window = NULL;
  gc = NULL;

  g_object_unref (G_OBJECT (black_gc));
  black_gc = NULL;
}

static enum tveng_frame_pixformat fmts[] = {
  TVENG_PIX_RGB24,
  TVENG_PIX_RGB32
};

static gboolean
suggest_format (void)
{
  capture_fmt fmt;
  int i;

  for (i=0; i<acount (fmts); i++)
    {
      fmt.fmt = fmts[i];
      fmt.locked = FALSE;
      if (suggest_capture_format (&fmt) != -1)
	return TRUE;
    }

  return FALSE;
}

static video_backend gdkrgb = {
  name:			"GdkRGB",
  set_destination:	set_destination,
  unset_destination:	unset_destination,
  image_new:		image_new,
  image_destroy:	image_destroy,
  image_put:		image_put,
  suggest_format:	suggest_format
};

void add_backend_gdkrgb (void);
void add_backend_gdkrgb (void)
{
  int i;
  for (i=0; i<acount (fmts); i++)
    register_video_backend (fmts[i], &gdkrgb);
}
