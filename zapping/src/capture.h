/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 Iñaki García Etxebarria
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

#ifndef __CAPTURE_H__
#define __CAPTURE_H__

/*
 * These routines handle the capture mode.
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <tveng.h>
#include "common/fifo.h"

/*
  This struct holds all the info about a video sample pased to a
  plugin. The plugin can write to all fields, but it should keep all
  the data valid, since the same struct will be passed to the
  remaining plugins.
*/
typedef struct {
  struct tveng_frame_format	format;

  union {
    xvzImage			*xvimage; /* if xv present */
    GdkImage			*gdkimage; /* otherwise */
    gpointer			yuv_data; /* raw data */
  } image;

#define CAPTURE_BUNDLE_XV 1		/* XvImage */
#define CAPTURE_BUNDLE_GDK 2		/* GdkImage */
#define CAPTURE_BUNDLE_DATA 3		/* raw YUYV data */
  gint		image_type;		/* type of data the bundle
					   contains */

  gpointer	data;			/* pointer to the data
					   (writable) */

  gint		image_size;		/* size of data, in bytes */

  double	timestamp;		/* time when the bundle was
					   captured */

  /* mhs: try to avoid; redundant when capture_bundle
     becomes parent of buffer. */
  fifo		*f;			/* fifo this bundle belongs to */
  buffer	*b;			/* buffer this bundle belongs to */
} capture_bundle;

/*
 * Inits the capture, setting the given widget as a destination.
 * Note that this only sets up the structs.
 * Returns FALSE on error
 */
gboolean
startup_capture(GtkWidget * widget);

/*
 * Releases the capture structs.
 */
void
shutdown_capture(tveng_device_info * info);

/*
 * Starts capturing to the given widget, returns -1 on error
 */
gint
capture_start(GtkWidget *widget, tveng_device_info * info);

/*
 * Stops capturing
 */
void
capture_stop(tveng_device_info * info);

/*
 * Requests that the bundles produced from now on have the given
 * format. Returns TRUE on success, FALSE on error.
 */
gboolean
request_bundle_format(enum tveng_frame_pixformat pixformat, gint w, gint h);

/*
 * Builds the bundle with the given parameters.
 */
void
build_bundle(capture_bundle *d, struct tveng_frame_format *format,
	     fifo *f, buffer *b);

/*
 * Frees the memory used by the bundle.
 */
void clear_bundle(capture_bundle *d);

/*
 * Returns TRUE if the two bundles have the same image properties
 * (width, height, pixformat, image_size...). If this function returns
 * TRUE, then a memcpy(b->data, a->data, a->image_size) is safe.
 */
gboolean bundle_equal(capture_bundle *a, capture_bundle *b);

/**
 * Locks the current capture format, so any call to
 * request_bundle_format will fail.
 * If it's already locked it does nothing.
 */
void
capture_lock(void);

/**
 * If the capture format is locked, this unlocks it.
 */
void
capture_unlock(void);

/**
 * Bundle filler. Allows plugins to provide frames from a variety of
 * sources.
 */
typedef void (*BundleFiller)(capture_bundle *bundle,
			     tveng_device_info *info);

/**
 * Sets the new bundle filler and returns the old one. If fill
 * bundle is NULL, then the default filler is restored.
 */
BundleFiller set_bundle_filler(BundleFiller fill_bundle);

fifo *
get_capture_fifo(void);

#endif /* capture.h */





