/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2001 Iñaki García Etxebarria
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
 */

#ifndef __ZVBI_H__
#define __ZVBI_H__

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_LIBZVBI

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libzvbi.h>

#include "tveng.h"
#include "zmodel.h"

enum ttx_message {
  TTX_NONE=0, /* No messages */
  TTX_PAGE_RECEIVED, /* The monitored page has been received */
  TTX_NETWORK_CHANGE, /* New network info feeded into the decoder */
#if 0 /* temporarily disabled */
  TTX_PROG_INFO_CHANGE, /* New program info feeded into the decoder */
  TTX_TRIGGER, /* Trigger event, ttx_message_data.link filled */
#endif
  TTX_CHANNEL_SWITCHED, /* zvbi_channel_switched was called, the cache
			   has been cleared */
  TTX_BROKEN_PIPE /* No longer connected to the TTX decoder */
};

typedef struct {
  enum ttx_message msg;
#if 0 /* temporarily disabled */
  union {
    vbi_link	link; /* A trigger link */
  } data;
#endif
} ttx_message_data;

extern vbi_pgno zvbi_caption_pgno; /* page for subtitles */

void startup_zvbi(void);

/* Shuts down the VBI engine */
void shutdown_zvbi(void);

/*
 * Register a client as TTX receiver, and returns the id that the
 * client should use to identify itself.
 */
int register_ttx_client(void);

/*
 * Gets the next message in the queue, or TTX_NONE if nothing
 * available.
 * Provide a valid pointer in data, it can be filled in.
 */
enum ttx_message peek_ttx_message(int id, ttx_message_data *data);

/*
 * Like peek, but waits until something is available
 * Provide a valid pointer in data, it can be filled in.
 */
enum ttx_message get_ttx_message(int id, ttx_message_data *data);

/*
 * Unregisters a client, telling that it won't continue porocessing
 * data
 */
void unregister_ttx_client(int id);

/*
 * Tells the renderer whether to reveal hidden chars.
 */
void set_ttx_parameters(int id, int reveal);

/*
 * Sets the given page as the page the client is interested in.
 * Use ANY_SUB in subpage for getting all subpages
 */
void monitor_ttx_page(int id/*client*/, int page, int subpage);

/*
 * Render the provided page to the client, and send the RECEIVED
 * event. Set it as the currently monitored page.
 */
void monitor_ttx_this(int id, vbi_page *pg);

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
vbi_page* get_ttx_fmt_page(int id);

/*
 * Returns a pointer to the scaled version of the page. Consider the
 * returned pixbuf as volatile, i.e., any other ttx_ call on the same
 * client can invalidate the returned pointer.
 * Can return NULL if the scaled version doesn't exist, etc...
 */
GdkPixbuf * get_scaled_ttx_page(int id);

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
zvbi_open_device(const char *device);

/* Closes the VBI device */
void
zvbi_close_device(void);

/*
  Returns the global vbi object, or NULL if vbi isn't enabled or
  doesn't work. You can safely use this function to test if VBI works
  (it will return NULL if it doesn't).
*/
vbi_decoder *
zvbi_get_object(void);

/*
  Returns the model for the vbi object. You can hook into this to know
  when the vbi object is created or destroyed.
*/
ZModel *
zvbi_get_model(void);

/*
  Returns the g_strdup'ed name of the current station, if known, or
  NULL. The returned value should be g_free'ed
*/
gchar *
zvbi_get_name(void);

/*
  Clears the station_name_known flag. Useful when you are changing
  freqs fast and you want to know the current station
*/
void
zvbi_name_unknown(void);

/*
  Called when changing channels, inputs, etc, tells the decoding engine
  to flush the cache, triggers, etc.
*/
void
zvbi_channel_switched(void);

/*
  Reset our knowledge about network and program,
  update main title and info dialogs.
*/
void
zvbi_reset_network_info(void);
void
zvbi_reset_program_info(void);

/*
  Returns the current program title ("Foo", "Bar", "Unknown"),
  the returned string must be g_free'ed
*/
gchar *
zvbi_current_title(void);

/*
  Returns the current program rating ("TV-MA", "Not rated"),
  don't free, just a pointer.
*/
const gchar *
zvbi_current_rating(void);

/*
  Returns the currently selected
  Teletext implementation level (for vbi_fetch_*)
*/
vbi_wst_level
zvbi_teletext_level (void);

#else /* !HAVE_LIBZVBI */

/* Startups the VBI engine (doesn't open devices) */
void startup_zvbi(void);

/* Shuts down the VBI engine */
void shutdown_zvbi(void);

typedef unsigned int vbi_pgno;

#endif /* !HAVE_LIBZVBI */

/*
  Modifies the gui according to VBI availability.
 */
void
vbi_gui_sensitive (gboolean on);

#endif /* zvbi.h */
