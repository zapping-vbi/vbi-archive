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
#include "../common/math.h"
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
planar_image_new (enum tveng_frame_pixformat fmt, gint w, gint h)
{
  guchar *data;
  zimage *image;
  zimage_private *pimage;
  
  g_assert (fmt == TVENG_PIX_YUV420 ||
	    fmt == TVENG_PIX_YVU420);

  if ((w%2 != 0) || (h%2 != 0))
    {
      g_warning ("YUV420 formats require even dimensions");
      return NULL;
    }

  data = g_malloc (w * h * 1.5);

  image = zimage_create_object ();
  pimage = image->priv = g_malloc0 (sizeof (*pimage));
  pimage->data = data;
  image->fmt.width = w;
  image->fmt.height = h;
  image->fmt.pixformat = fmt;
  image->fmt.bpp = 1.5;
  image->fmt.depth = 12;
  image->fmt.bytesperline = image->fmt.bpp * w;
  image->fmt.sizeimage = w * h * 1.5;
  image->data.planar.y = data;
  image->data.planar.y_stride = w;
  image->data.planar.u = data + (w * h);
  image->data.planar.v = data + ((int)((w * h) * 1.25));
  image->data.planar.uv_stride = w * 0.25;

  return image;
}

static zimage*
image_new (enum tveng_frame_pixformat fmt, gint w, gint h)
{
  guchar *data;
  zimage *image;
  zimage_private *pimage;
  /* bpp's for the different modes */
  gint bpps[TVENG_PIX_LAST] =
    {
      2, 2, 3, 3, 4, 4, 0, 0, 2, 2
    };
  gint bpp = bpps[fmt];

  if (!bpp)
    return planar_image_new (fmt, w, h);

  data = g_malloc (w * h * bpp);
  image = zimage_create_object ();
  pimage = image->priv = g_malloc0 (sizeof (*pimage));
  pimage->data = data;
  image->fmt.width = w;
  image->fmt.height = h;
  image->fmt.pixformat = fmt;
  image->fmt.bpp = bpp;
  image->fmt.depth = 8*bpp;
  image->fmt.bytesperline = image->fmt.bpp * w;
  image->fmt.sizeimage = w * h * bpp;

  image->data.linear.data = data;
  image->data.linear.stride = w * bpp;

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
  int i;
  for (i=0; i<TVENG_PIX_LAST; i++)
    register_video_backend (i, &mem);
}
