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
planar_image_new (const tv_pixel_format *pf, guint w, guint h)
{
  guchar *data;
  zimage *image;
  zimage_private *pimage;
  unsigned int y_size;
  unsigned int uv_size;
  unsigned int hres;
  unsigned int vres;

  hres = (1 << pf->uv_hshift) - 1;
  vres = (1 << pf->uv_vshift) - 1;

  if ((w & hres) || (h & vres))
    {
      g_warning ("YUV420 formats require even dimensions");
      return NULL;
    }

  y_size = w * h;
  uv_size = y_size >> (pf->uv_hshift + pf->uv_vshift);
  data = g_malloc (y_size + uv_size * 2);

  image = zimage_create_object ();
  pimage = image->priv = g_malloc0 (sizeof (*pimage));
  pimage->data = data;

  image->fmt.width = w;
  image->fmt.height = h;
  image->fmt.pixel_format = tv_pixel_format_from_pixfmt (pf->pixfmt);
  image->fmt.bytes_per_line[0] = w;
  image->fmt.size = y_size + uv_size * 2;

  image->img = data;
  image->fmt.offset[1] = y_size + pf->vu_order * uv_size;
  image->fmt.offset[2] = y_size + (pf->vu_order ^ 1) * uv_size;
  image->fmt.bytes_per_line[1] = w >> pf->uv_hshift;
  image->fmt.bytes_per_line[2] = w >> pf->uv_hshift;

  return image;
}

static zimage*
image_new (tv_pixfmt pixfmt, guint w, guint h)
{
  guchar *data;
  zimage *image;
  zimage_private *pimage;
  const tv_pixel_format *pf;
  guint bpl, size;

  pf = tv_pixel_format_from_pixfmt (pixfmt);
  assert (NULL != pf);

  if (pf->planar)
    return planar_image_new (pf, w, h);

  bpl = (w * pf->bits_per_pixel) >> 3;
  size = h * bpl;

  data = g_malloc (size);
  image = zimage_create_object ();
  pimage = image->priv = g_malloc0 (sizeof (*pimage));
  pimage->data = data;
  image->fmt.width = w;
  image->fmt.height = h;
  image->fmt.pixel_format = tv_pixel_format_from_pixfmt (pixfmt);
  image->fmt.bytes_per_line[0] = bpl;
  image->fmt.size = size;

  image->img = data;

  return image;
}

static void
image_destroy (zimage *image)
{
  zimage_private *pimage = image->priv;

  g_free (pimage->data);
  g_free (pimage);
}

static tv_pixfmt_set
supported_formats		(void)
{
  /* No formats displayable. */
  return 0;
}

static video_backend mem = {
  .name			= "Memory backend",
  .image_new		= image_new,
  .image_destroy	= image_destroy,
  .supported_formats	= supported_formats,
};

void add_backend_mem (void);
void add_backend_mem (void)
{
  tv_pixfmt pixfmt;

  for (pixfmt = 0; pixfmt < TV_MAX_PIXFMTS; ++pixfmt)
    if (TV_PIXFMT_SET_ALL & TV_PIXFMT_SET (pixfmt))
      register_video_backend (pixfmt, &mem);
}
