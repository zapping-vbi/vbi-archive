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
planar_image_new (tv_pixel_format *pf, guint w, guint h)
{
  guchar *data;
  zimage *image;
  zimage_private *pimage;
  unsigned int y_size;
  unsigned int uv_size;

  if (0 != (w % pf->uv_hscale) || 0 != (h % pf->uv_vscale))
    {
      g_warning ("YUV420 formats require even dimensions");
      return NULL;
    }

  y_size = w * h;
  uv_size = y_size / (pf->uv_hscale * pf->uv_vscale);
  data = g_malloc (y_size + uv_size * 2);

  image = zimage_create_object ();
  pimage = image->priv = g_malloc0 (sizeof (*pimage));
  pimage->data = data;

  image->fmt.width = w;
  image->fmt.height = h;
  image->fmt.pixfmt = pf->pixfmt;
  image->fmt.bytes_per_line = w;
  image->fmt.size = y_size + uv_size * 2;

  image->data.planar.y = data;
  image->data.planar.y_stride = w;
  image->data.planar.u = data + y_size + pf->vu_order * uv_size;
  image->data.planar.v = data + y_size + (pf->vu_order ^ 1) * uv_size;
  image->data.planar.uv_stride = w / pf->uv_hscale;

  return image;
}

static zimage*
image_new (tv_pixfmt pixfmt, guint w, guint h)
{
  guchar *data;
  zimage *image;
  zimage_private *pimage;
  tv_pixel_format format;
  guint bpl, size;

  tv_pixel_format_from_pixfmt (&format, pixfmt, 0);

  if (format.planar)
    return planar_image_new (&format, w, h);

  bpl = (w * format.bits_per_pixel) >> 3;
  size = h * bpl;

  data = g_malloc (size);
  image = zimage_create_object ();
  pimage = image->priv = g_malloc0 (sizeof (*pimage));
  pimage->data = data;
  image->fmt.width = w;
  image->fmt.height = h;
  image->fmt.pixfmt = pixfmt;
  image->fmt.bytes_per_line = bpl;
  image->fmt.size = size;

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
