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
#include "zmisc.h"

static struct {
  video_backend		backend;
  tv_pixfmt		pixfmt;
} *				backends;
static guint			n_backends;

/* Bookkeeping stuff */
typedef struct {
  zimage	image;

  int		refcount; /* Image refcount */
  int		backend; /* Backend that created this image */
} private_zimage;

static GtkWidget *dest_window = NULL;

gint				capture_format_id = -1;

gboolean register_video_backend (tv_pixfmt pixfmt,
				 video_backend *backend)
{
  /* We always let backends be registered, so zimage_new always
     succeeds for all pixformats (video_mem doesn't fail). */
  backends = g_realloc (backends, (n_backends+1)*sizeof(*backends));
  backends[n_backends].pixfmt = pixfmt;
  memcpy (&backends[n_backends].backend, backend, sizeof(*backend));
  n_backends ++;
  
  return TRUE;
}

zimage *zimage_create_object (void)
{
  return (g_malloc0 (sizeof (private_zimage)));
}

zimage *zimage_new (tv_pixfmt pixfmt,
		    guint w, guint h)
{
  guint i;
  for (i=0; i<n_backends; i++)
    if (backends[i].pixfmt == pixfmt)
      {
	zimage *zimage = backends[i].backend.image_new (pixfmt, w, h);
	if (zimage)
	  {
	    private_zimage *pz = (private_zimage*)zimage;

	    printv ("zimage_new %p using video backend %s\n",
		    zimage, backends[i].backend.name);

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
      printv ("zimage_destroy %p\n", image);

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
      (image,
       (guint) dest_window->allocation.width,
       (guint) dest_window->allocation.height);
}

void
video_init			(GtkWidget *		window,
				 GdkGC *		gc)
{
  tv_pixfmt_set pixfmt_set;
  guint i;

  g_assert (window->window);

  dest_window = window;

  pixfmt_set = TV_PIXFMT_SET_EMPTY;

  for (i = 0; i < n_backends; ++i)
    {
      pixfmt_set |= backends[i].backend.supported_formats ();

      if (backends[i].backend.set_destination)
	backends[i].backend.set_destination (window->window,
					     gc, zapping->info);
    }

  capture_format_id = request_capture_format (zapping->info,
					      /* width: any */ 0,
					      /* height: any */ 0,
					      pixfmt_set,
					      /* flags */ 0);
}

void
video_uninit			(void)
{
  guint i;

  release_capture_format (capture_format_id);
  capture_format_id = -1;

  for (i = 0; i < n_backends; ++i)
    if (backends[i].backend.unset_destination)
      backends[i].backend.unset_destination (zapping->info);

  dest_window = NULL;
}

void video_blit_frame (capture_frame *frame)
{
  guint i;
  zimage *img;

 for (i=0; i<n_backends; i++)
   {
     /* NOTE copy: we want the original zimage allocated by the backend. */
     if ((img = retrieve_frame (frame, backends[i].pixfmt, /* copy */ TRUE)))
       {
	 zimage_blit (img);
	 return;
       }
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
  n_backends = 0;
}
