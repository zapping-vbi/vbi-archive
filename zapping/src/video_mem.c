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
 * Mem backend. This doesn't provide blitting capabilities, just the
 * necessary memory allocation/deallocation. Used when it isn't
 * possible to display a given format but we need it anyway (recording
 * YUV data without Xv displays, for example).
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include "common/math.h"
#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zconf.h"
#include "globals.h"
#include "zimage.h"
#include "zmisc.h"
#include "capture.h"
#include "x11stuff.h"

struct _zimage_private {
  guchar	*data;
};

static zimage*
planar_image_new (tv_pixfmt pixfmt, gint w, gint h)
{
  guchar *data;
  zimage *image;
  zimage_private *pimage;

  g_assert (pixfmt == TV_PIXFMT_YUV420 ||
	    pixfmt == TV_PIXFMT_YVU420);

  if ((w | h) & 1)
    {
      g_warning ("YUV420 formats require even dimensions");
      return NULL;
    }

  data = g_malloc (w * h * 3 / 2);

  image = zimage_create_object ();
  pimage = image->priv = g_malloc0 (sizeof (*pimage));
  pimage->data = data;
  image->fmt.width = w;
  image->fmt.height = h;
  image->fmt.pixfmt = pixfmt;
  image->fmt.bytesperline = 8 * w;
  image->fmt.sizeimage = w * h * 1.5;
  image->data.planar.y = data;
  image->data.planar.y_stride = w;
  image->data.planar.u = data + (w * h);
  image->data.planar.v = data + ((int)((w * h) * 1.25));
  image->data.planar.uv_stride = w * 0.25;

  return image;
}

static zimage*
image_new (tv_pixfmt pixfmt, gint w, gint h)
{
  guchar *data;
  zimage *image;
  zimage_private *pimage;
  tv_pixel_format format;
  guint bpl, size;

  tv_pixfmt_to_pixel_format (&format, pixfmt, 0);

  if (format.planar)
    return planar_image_new (pixfmt, w, h);

  bpl = (w * format.bits_per_pixel) >> 3;
  size = h * bpl;

  data = g_malloc (size);
  image = zimage_create_object ();
  pimage = image->priv = g_malloc0 (sizeof (*pimage));
  pimage->data = data;
  image->fmt.width = w;
  image->fmt.height = h;
  image->fmt.pixfmt = pixfmt;
  image->fmt.bytesperline = bpl;
  image->fmt.sizeimage = size;

  image->data.linear.data = data;
  image->data.linear.stride = bpl;

  return image;
}

static void
image_destroy (zimage *image)
{
  zimage_private *pimage = image->priv;

  g_free (pimage->data);
  g_free (pimage);
}

static video_backend mem = {
  name:			"Memory backend",
  image_new:		image_new,
  image_destroy:	image_destroy
};

void add_backend_mem (void);
void add_backend_mem (void)
{
  tv_pixfmt pixfmt;

  for (pixfmt = 0; pixfmt < TV_MAX_PIXFMTS; ++pixfmt)
    if (TV_PIXFMT_SET_ALL & TV_PIXFMT_SET (pixfmt))
      register_video_backend (pixfmt, &mem);
}
