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

#include "../config.h"

#ifdef HAVE_LIBZVBI

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libzvbi.h>

#include "tveng.h"
#include "zmodel.h"

typedef void
vbi_sliced_fn			(const vbi_sliced *	sliced,
				 unsigned int		n_lines,
				 double			timestamp);
extern GList *sliced_list;

extern void
startup_zvbi			(void);
extern void
shutdown_zvbi			(void);

/* Open the configured VBI device, FALSE on error */
gboolean
zvbi_open_device		(const char *		dev_name);
/* Closes the VBI device */
void
zvbi_close_device		(void);
/* Returns the global vbi object, or NULL if vbi isn't enabled or
   doesn't work. You can safely use this function to test if VBI works
   (it will return NULL if it doesn't). */
vbi_decoder *
zvbi_get_object			(void);
/* Returns the model for the vbi object. You can hook into this to know
   when the vbi object is created or destroyed. */
ZModel *
zvbi_get_model			(void);
/* Returns the g_strdup'ed name of the current station, if known, or
   NULL. The returned value should be g_free'ed. */
gchar *
zvbi_get_name			(void);
gchar *
zvbi_get_current_network_name	(void);
/* Clears the station_name_known flag. Useful when you are changing
   freqs fast and you want to know the current station */
void
zvbi_name_unknown		(void);
/* Called when changing channels, inputs, etc, tells the decoding engine
   to flush the cache, triggers, etc. */
void
zvbi_channel_switched		(void);
/* Reset our knowledge about network and program,
   update main title and info dialogs. */
void
zvbi_reset_network_info		(void);
void
zvbi_reset_program_info		(void);
/* Returns the current program title ("Foo", "Bar", "Unknown"),
   the returned string must be g_free'ed */
gchar *
zvbi_current_title		(void);
/* Returns the current program rating ("TV-MA", "Not rated"),
   don't free, just a pointer. */
const gchar *
zvbi_current_rating		(void);
/* Returns the currently selected
   Teletext implementation level (for vbi_fetch_*) */
vbi_wst_level
zvbi_teletext_level		(void);

#else /* !HAVE_LIBZVBI */

/* Startups the VBI engine (doesn't open devices) */
void
startup_zvbi			(void);
/* Shuts down the VBI engine */
void
shutdown_zvbi			(void);

typedef int vbi_pgno;

#endif /* !HAVE_LIBZVBI */

/* Modifies the gui according to VBI availability. */
void
vbi_gui_sensitive		(gboolean		on);

#endif /* zvbi.h */
