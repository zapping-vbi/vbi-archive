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
  plugin.
*/
typedef struct {
  struct tveng_frame_format	format; /* described pixformat, size,
					   etc, see tveng.h */

  /* type of data the bundle contains */
  enum {
    CAPTURE_BUNDLE_XV=1,	/* XvImage */
    CAPTURE_BUNDLE_GDK,		/* GdkImage */
    CAPTURE_BUNDLE_DATA		/* raw YUV data */
  } image_type;

  union {
    xvzImage			*xvimage; /* _XV */
    GdkImage			*gdkimage; /* _GDK */
    gpointer			yuv_data; /* _DATA */
  } image;

  /* data contained in the image; size etc is described by format */
  gpointer			data;
  /* time this bundle was created */
  double			timestamp;

  /* Who produced this bundle (read/only) */
  producer			*producer;
} capture_bundle;

/*
 * capture buffer. consumer plugins can add them to the consumer list
 * of the capture fifo and wait_full buffers.
 */
typedef struct {
  buffer	b; /* this is read-only */

  capture_bundle d;
} capture_buffer;

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
shutdown_capture(void);

/*
 * Starts capturing to the given widget, returns -1 on error
 */
gint
capture_start(GtkWidget *widget, tveng_device_info * info);

/*
 * Stops capturing
 */
void
capture_stop(tveng_device_info *info);

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
build_bundle(capture_bundle *d, struct tveng_frame_format *format);

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

extern fifo	*capture_fifo;

#endif /* capture.h */





