#ifndef __ZIMAGE_H__
#define __ZIMAGE_H__

#include <gtk/gtk.h>
#include "tveng.h"
#include "common/fifo.h"
#include "csconvert.h"

/* Startup/shutdown */
void startup_zimage (void);
void shutdown_zimage (void);

typedef struct _zimage_private zimage_private;

typedef struct {
  struct tveng_frame_format fmt;
  tveng_image_data	data;

  /* Video backend dependant */
  zimage_private *priv;
} zimage;

/*
 * The buffers in the capture fifo are these kind of objects.
 */
typedef struct {
  /* the parent struct */
  zf_buffer b;
  /* time this frame was captured (same as b.time) */
  double timestamp;
} capture_frame;

/**
 * Struct for video backends
 */
typedef struct {
  /* A descriptive name for the backend */
  char		*name;
  /* Set blit destination. Grabbing, if necessary, should only happen
     when creating new images */
  void	(*set_destination)(GdkWindow *window, GdkGC *gc,
			   tveng_device_info *info);
  /* Unsets a previously set blit destination */
  void		(*unset_destination)(tveng_device_info *info);
  /* Create a suitable image, will always be called with the port
     grabbed */
  zimage*	(*image_new)(tv_pixfmt pixfmt,
			     gint width, gint height);
  /* Destroy any data associated with the image, do _not_ call
     g_free (image) */
  void		(*image_destroy)(zimage *image);
  /* Put the image in the drawable, do scaling as necessary. Width and
     height are the current dimensions of the destination, can be
     ignored if not appropiate. */
  void		(*image_put)(zimage *image, gint width, gint height);
  /* Suggest a blittable format, return TRUE if granted */
  gboolean	(*suggest_format)(void);
} video_backend;

/* Registers a zimage backend. Returns FALSE if already registered. */
gboolean register_video_backend (tv_pixfmt pixfmt,
				 video_backend *backend);

/* Creates a zimage with the given pixformat. The resulting image
   might not be blittable. NULL will be returned if the requested
   image cannot be allocated. The refcount of the resulting image will
   be 1. */
zimage *zimage_new (tv_pixfmt pixfmt,
		    gint w, gint h);

/* Increments the refcount of the image. */
void zimage_ref (zimage *image);

/* Decrements the refcount of the given image. In case it reaches 0 it
   will be destroyed. */
void zimage_unref (zimage *image);

/* Blits the image into the configured drawable for its video format.
   video_init must be called before calling this function, otherwise
   it won't work */
void zimage_blit (zimage *image);

/* Sets the given widget as the blit destination */
void video_init (GtkWidget *widget, GdkGC *gc);

/* Unsets any previously set destination window */
void video_uninit (void);

/* Suggests a blittable format */
void video_suggest_format (void);

/* Tries to blit some image contained in the frame */
void video_blit_frame (capture_frame* f);

/*
  Creates a zimage object, only used by backends.
 */
zimage *zimage_create_object (void);

#endif
