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
 * X11 backend. Actually this should be named video_gdkimage, but since
 * video_gdkimage is a bit too long and this kind of blitter is
 * typically dubbed x11,  just go with that name. No X specific code
 * in here, though.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zconf.h"
#include "globals.h"
#include "zimage.h"
#include "zmisc.h"
#include "capture.h"
#include "x11stuff.h"

static GdkWindow	*window = NULL;
static GdkGC		*gc = NULL, *black_gc = NULL;
static tv_pixfmt	x11_pixfmt;

struct _zimage_private {
  GdkImage		*image;
};

static zimage*
image_new (tv_pixfmt pixfmt, guint w, guint h)
{
  zimage *new_image;
  zimage_private *pimage;
  GdkImage *image;

  g_assert (pixfmt == x11_pixfmt);

  image = gdk_image_new (GDK_IMAGE_FASTEST, gdk_visual_get_system (),
			 (gint) w, (gint) h);

  if (!image)
    return NULL;

  if ((guint) image->width != w ||
      (guint) image->height != h)
    {
      g_object_unref (G_OBJECT (image));
      return NULL;
    }

  pimage = g_malloc0 (sizeof (*pimage));
  new_image = zimage_create_object ();
  new_image->priv = pimage;
  pimage->image = image;

  new_image->fmt.width = w;
  new_image->fmt.height = h;
  new_image->fmt.pixel_format = tv_pixel_format_from_pixfmt (pixfmt);
  new_image->fmt.offset[0] = 0;
  new_image->fmt.bytes_per_line[0] = image->bpl;
  new_image->fmt.size = image->bpl * image->height;
  new_image->img = image->mem;

  return new_image;
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
image_put (zimage *image, guint w, guint h)
{
  zimage_private *pimage = image->priv;
  gint iw = pimage->image->width, ih = pimage->image->height;

  g_assert (window != NULL);

  clear_canvas (window, w, h, iw, ih);
  gdk_draw_image (window, gc, pimage->image,
		  0, 0, (w - iw)/2, (h - ih)/2, iw, ih);

  gdk_display_flush (gdk_display_get_default ());
}

static void
image_destroy (zimage *image)
{
  zimage_private *pimage = image->priv;

  g_object_unref (G_OBJECT (pimage->image));

  g_free (pimage);
}

static void
set_destination (GdkWindow *_w, GdkGC *_gc,
		 tveng_device_info *info _unused_)
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
unset_destination(tveng_device_info *info _unused_)
{
  /* see comment in set_destination */
  if ((!window) && (!gc))
    return;

  window = NULL;
  gc = NULL;

  g_object_unref (G_OBJECT (black_gc));
  black_gc = NULL;
}

static tv_pixfmt_set
supported_formats		(void)
{
  return TV_PIXFMT_SET (x11_pixfmt);
}

static video_backend x11 = {
  .name			= "X11 PutImage",
  .set_destination	= set_destination,
  .unset_destination	= unset_destination,
  .image_new		= image_new,
  .image_destroy	= image_destroy,
  .image_put		= image_put,
  .supported_formats	= supported_formats,
};

void add_backend_x11 (void);
void add_backend_x11 (void)
{
  /* Same for all screens. */
  x11_pixfmt = screens->target.format.pixel_format->pixfmt;

  g_assert (TV_PIXFMT_UNKNOWN != x11_pixfmt);
  g_assert (TV_PIXFMT_IS_PACKED (x11_pixfmt));

  register_video_backend (x11_pixfmt, &x11);
}

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
