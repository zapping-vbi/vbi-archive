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

/*
 * This code is used to communicate with the VBI device (usually
 * /dev/vbi), so multiple plugins can access to it simultaneously.
 * The code uses libvbi, a nearly verbatim copy of alevt.
 */

#ifndef __ZVBI_H__
#define __ZVBI_H__

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <libvbi.h>
#include "tveng.h"

enum ttx_message {
  TTX_NONE=0, /* No messages */
  TTX_PAGE_RECEIVED, /* The monitored page has been received */
  TTX_BROKEN_PIPE /* No longer connected to the TTX decoder */
};

/*
 * Register a client as TTX receiver, and returns the id that the
 * client should use to identify itself.
 */
int register_ttx_client(void);

/*
 * Gets the next message in the queue, or TTX_NONE if nothing available.
 */
enum ttx_message peek_ttx_message(int id);

/*
 * Like peek, but waits until something is available
 */
enum ttx_message get_ttx_message(int id);

/*
 * Unregisters a client, telling that it won't continue porocessing
 * data
 */
void unregister_ttx_client(int id);

/*
 * Sets the given page as the page the client is interested in.
 * Use ANY_SUB in subpage for getting all subpages
 */
void monitor_ttx_page(int id/*client*/, int page, int subpage);

/*
 * Render the provided page to the client, and send the RECEIVED
 * event. Set it as the currently monitored page.
 */
void monitor_ttx_this(int id, struct fmt_page *pg);

/*
 * Freezes the current page, no more RECEIVED events are sent until
 * it's unfreezed.
 */
void ttx_freeze(int id);

/*
 * Allows sending RECEIVED events to the client.
 */
void ttx_unfreeze(int id);

/*
 * Returns a pointer to the formatted page the client is rendering
 */
struct fmt_page* get_ttx_fmt_page(int id);

/*
 * Resizes the rendered size of the page
 */
void resize_ttx_page(int id, int w, int h);

/*
 * Renders the currently monitored page into the given drawable.
 */
void render_ttx_page(int id, GdkDrawable *drawable, GdkGC *gc,
		     gint src_x, gint src_y,
		     gint dest_x, gint dest_y,
		     gint w, gint h);

/*
 * Renders into the given bitmap (that must have appropiate
 * dimensions) the transparency info of the current page.
 */
void render_ttx_mask(int id, GdkBitmap *bitmap);

/**
 * Updates the blinking items in the page.
 */
void refresh_ttx_page(int id, GtkWidget *drawable);

/* Open the configured VBI device, FALSE on error */
gboolean
zvbi_open_device(void);

/* Closes the VBI device */
void
zvbi_close_device(void);

/*
  Returns the global vbi object, or NULL if vbi isn't enabled or
  doesn't work. You can safely use this function to test if VBI works
  (it will return NULL if it doesn't).
*/
struct vbi *
zvbi_get_object(void);

/*
  Returns a pointer to the name of the Teletext provider, or NULL if
  this name is unknown. You must g_free the returned value.
*/
gchar*
zvbi_get_name(void);

/*
  Fills in the given pointers with the time as it appears in the
  header. The pointers can be NULL.
  If the time isn't known, -1 will be returned in all the fields
*/
void
zvbi_get_time(gint * hour, gint * min, gint * sec);

#endif /* zvbi.h */
