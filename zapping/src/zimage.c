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
 * Video output.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include "zimage.h"
#include "globals.h"
#include "capture.h"

static struct {
  video_backend		backend;
  enum tveng_frame_pixformat fmt;
} *backends = NULL;
static int num_backends = 0;

/* Bookkeeping stuff */
typedef struct {
  zimage	image;

  int		refcount; /* Image refcount */
  int		backend; /* Backend that created this image */
} private_zimage;

static GtkWidget *dest_window = NULL;

gboolean register_video_backend (enum tveng_frame_pixformat fmt,
				 video_backend *backend)
{
  /* We always let backends be registered, so zimage_new always
     succeeds for all pixformats (video_mem doesn't fail). */
  backends = g_realloc (backends, (num_backends+1)*sizeof(*backends));
  backends[num_backends].fmt = fmt;
  memcpy (&backends[num_backends].backend, backend, sizeof(*backend));
  num_backends ++;
  
  return TRUE;
}

zimage *zimage_create_object (void)
{
  return (g_malloc0 (sizeof (private_zimage)));
}

zimage *zimage_new (enum tveng_frame_pixformat fmt,
		    gint w, gint h)
{
  gint i;
  for (i=0; i<num_backends; i++)
    if (backends[i].fmt == fmt)
      {
	zimage *zimage = backends[i].backend.image_new (fmt, w, h);
	if (zimage)
	  {
	    private_zimage *pz = (private_zimage*)zimage;
	    pz->refcount = 1;
	    pz->backend = i;
	    return zimage;
	  }
      }

  /* The video_mem backend should always succeed. */
  g_assert_not_reached ();
  return NULL;
}

void zimage_ref (zimage *image)
{
  private_zimage *pz = (private_zimage*)image;
  pz->refcount ++;
}

void zimage_unref (zimage *image)
{
  private_zimage *pz = (private_zimage*)image;

  if (! (-- pz->refcount))
    {
      backends[pz->backend].backend.image_destroy (image);
      g_free (pz);
    }
}

void zimage_blit (zimage *image)
{
  private_zimage *pz = (private_zimage*)image;

  g_assert (dest_window != NULL);

  if (backends[pz->backend].backend.image_put)
    backends[pz->backend].backend.image_put
      (image, dest_window->allocation.width,
       dest_window->allocation.height);
}

void video_init (GtkWidget *window, GdkGC *gc)
{
  int i;
  g_assert (window->window);

  dest_window = window;

  for (i=0; i<num_backends; i++)
    if (backends[i].backend.set_destination)
      backends[i].backend.set_destination (window->window, gc, main_info);
}

void video_uninit (void)
{
  int i;
  for (i=0; i<num_backends; i++)
    if (backends[i].backend.unset_destination)
      backends[i].backend.unset_destination (main_info);

  dest_window = NULL;
}

void video_suggest_format (void)
{
  int i;
  for (i=0; i<num_backends; i++)
    if (backends[i].backend.suggest_format &&
	backends[i].backend.suggest_format ())
      break;
}

void video_blit_frame (capture_frame *frame)
{
  int i;
  zimage *img;

  for (i=0; i<num_backends; i++)
    if ((img = retrieve_frame (frame, backends[i].fmt)))
      {
	zimage_blit (img);
	return;
      }
}

void startup_zimage (void)
{
  extern void add_backend_xv (void);
  extern void add_backend_x11 (void);
  extern void add_backend_gdkrgb (void);
  extern void add_backend_mem (void);

  /* The order is important, fast backends should be added first */
  add_backend_xv ();
  add_backend_x11 ();
  add_backend_gdkrgb ();
  add_backend_mem ();
}

void shutdown_zimage (void)
{
  video_uninit ();

  g_free (backends);
  backends = NULL;
  num_backends = 0;
}
