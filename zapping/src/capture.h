/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
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

#include "common/fifo.h"
#include "tveng.h"

#include "x11stuff.h"
#include "zimage.h"

/* Startup/shutdown of the modules */
void startup_capture (void);
void shutdown_capture (void);

/* Description of a possible capture format */
typedef struct {
  gboolean	locked; /* TRUE if we need a fixed size */
  gint		width, height; /* Frame size, only applies if locked */
  enum tveng_frame_pixformat fmt; /* The actual format */
} capture_fmt;

/*
 * Capture routines proper.
 */
/*
 * Start capturing, returns -1 on error.
 */
gint capture_start (tveng_device_info *info);

/*
 * Stop the current capture.
 */
void capture_stop (void);

/*
 * Requests the given format. Upon success a request id will be
 * returned, failure is signalled by returning -1. Once a format is
 * granted, you are assured to get all data read from the input device
 * in this format.
 */
gint request_capture_format (capture_fmt *fmt);

/*
 * Like request_capture_format, but when a request that isn't
 * compatible with this one arrives, the format will be temporarily
 * dropped. Useful when getting frames in this format would be nice
 * but it isn't absolutely essential.
 */
gint suggest_capture_format (capture_fmt *fmt);

/*
 * Releases the given requests, call this when you won't longer need
 * the format. The id returned by request_capture_format must be passed.
 */
void release_capture_format (gint id);

/*
 * Gets a list of the current granted requests.
 * @formats: Pointer to a place where to allocate the list, you must
 * g_free it when no longer needed.
 * @num_formats: Pointer to a place where to store the size of the list.
 */
void get_request_formats (capture_fmt **fmts, gint *num_fmts);

/*
 * Capture events:
 *	CAPTURE_STOP: No more data will be produced (for example, this
 *	will be called when capture is stopped through the
 *	GUI). Assume that notify hooks will be disconnected and no
 *	more data will arrive. The request list is also cleared, so
 *	free any data associated with it.
 *	CAPTURE_CHANGED: The internal capture format has
 *	changed. Typical usage is renegotiating the current capture
 *	format when this is received, perhaps a better format can be
 *	granted now.
 */
typedef enum {
  CAPTURE_STOP,
  CAPTURE_CHANGE
} capture_event;

typedef void (*CaptureNotify) (capture_event	event,
			       gpointer		user_data);

/*
 * Connect to this if you want to get notified of capture events. You
 * can use the returned id when releasing the handler. Note that
 * handlers are automatically released when the capture is stopped,
 * but it's good practise to release them asap.
 * -1 is returned upon error.
 */
gint add_capture_handler (CaptureNotify notify, gpointer user_data);

/* Removes a handler, use the id returned by add_capture_handler */
void remove_capture_handler (gint id);

/*
 * The capture fifo.
 */
extern fifo *capture_fifo;

/*
 * Returns the zimage with the given pixformat contained in the
 * frame. NULL if it ain't possible to produce the zimage.
 * The returned zimage, if any, will only be valid until you
 * send_empty the buffer. Also, you shouldn't free the image, it's a
 * reference to an static object.
 */
zimage *retrieve_frame (capture_frame *frame,
			enum tveng_frame_pixformat fmt);

#endif
