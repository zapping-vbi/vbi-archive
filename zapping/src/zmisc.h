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
  Misc stuff for zapping that didn't fit properly in any other place,
  but was commonly used.
*/

#ifndef __ZMISC_H__
#define __ZMISC_H__

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#ifdef HAVE_GDKPIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif

#include "tveng.h"

/* With precompiler aid we can print much more useful info */
/* This shows a non-modal, non-blocking message box */
#define ShowBox(MSG, MSGTYPE, args...) \
do { \
  gchar * tmp_str = g_strdup_printf(MSG,##args); \
  ShowBoxReal(__FILE__, __LINE__, G_GNUC_PRETTY_FUNCTION, \
	      tmp_str, MSGTYPE, FALSE, FALSE); \
  g_free(tmp_str); \
} while (FALSE)

/* This one shows a modal, non-blocking message box */
#define ShowBoxModal(MSG, MSGTYPE) \
ShowBoxReal(__FILE__, __LINE__, G_GNUC_PRETTY_FUNCTION, \
MSG, MSGTYPE, FALSE, TRUE)

/* This one shows a modal, blocking message box */
#define RunBox(MSG, MSGTYPE) \
ShowBoxReal(__FILE__, __LINE__, G_GNUC_PRETTY_FUNCTION, \
MSG, MSGTYPE, TRUE, TRUE)

/* Some debug messages to track the startup */
extern gboolean debug_msg;

#define D() \
do { \
  if (debug_msg) \
    g_message("Line %d, routine %s", __LINE__, __PRETTY_FUNCTION__); \
} while (FALSE)

#define printv(format, args...) \
do { \
  if (debug_msg) \
    g_message(format ,##args); \
} while (FALSE)

/*
  Prints a message box showing an error, with the location of the code
  that called the function.
*/
GtkWidget * ShowBoxReal(const gchar * sourcefile,
			const gint line,
			const gchar * func,
			const gchar * message,
			const gchar * message_box_type,
			gboolean blocking, gboolean modal);

/*
  Converts 0x123 into 123d
*/
gint hex2dec(gint hex);

/*
  Converts 123d into 0x123
*/
gint dec2hex(gint dec);

/*
  Resizes the image to a new size. If this is the same as the old
  size, nothing happens. Returns the newly allocated image on exit.
*/
GdkImage*
zimage_reallocate(int new_width, int new_height);

/*
  Returns a pointer to the zimage
*/
GdkImage*
zimage_get(void);

/*
  Returns a pointer to the zimage data.
*/
gpointer
zimage_get_data ( GdkImage * image);

/*
  Destroys the image that holds the capture
*/
void
zimage_destroy(void);

/*
  Creates a GtkPixmapMenuEntry with the desired pixmap and the
  desired label. The pixmap is a stock GNOME pixmap.
*/
GtkWidget * z_gtk_pixmap_menu_item_new(const gchar * label,
				       const gchar * icon);

/* Announces that the tv_screen has moved. This routine refreshes the
   old placement of the window if neccesary */
void zmisc_refresh_tv_screen(gint x, gint y, gint w, gint h, gboolean
			     obscured);

/* Clears any timers zmisc could use (the Zapping window is to be closed) */
void zmisc_clear_timers(void);

/*
  does the mode switching. Since this requires more than just using
  tveng, a new routine is needed.
  Returns whatever tveng returns, but we print the message ourselves
  too, so no need to aknowledge it to the user.
  Side efects: Stops whatever mode was being used before.
*/
int
zmisc_switch_mode(enum tveng_capture_mode new_mode,
		  tveng_device_info * info);

#endif /* ZMISC.H */

#ifdef ZCONF_DOMAIN
#define zcs_int(var, key) \
zconf_set_integer(var,  ZCONF_DOMAIN key)
#define zcg_int(where, key) \
zconf_get_integer(where, ZCONF_DOMAIN key)
#define zcc_int(value, desc, key) \
zconf_create_integer(value, desc, ZCONF_DOMAIN key)
#define zcs_float(var, key) \
zconf_set_float(var,  ZCONF_DOMAIN key)
#define zcg_float(where, key) \
zconf_get_float(where, ZCONF_DOMAIN key)
#define zcc_float(value, desc, key) \
zconf_create_float(value, desc, ZCONF_DOMAIN key)
#define zcs_char(var, key) \
zconf_set_string(var,  ZCONF_DOMAIN key)
#define zcg_char(where, key) \
zconf_get_string(where, ZCONF_DOMAIN key)
#define zcc_char(value, desc, key) \
zconf_create_string(value, desc, ZCONF_DOMAIN key)
#define zcs_bool(var, key) \
zconf_set_boolean(var,  ZCONF_DOMAIN key)
#define zcg_bool(where, key) \
zconf_get_boolean(where, ZCONF_DOMAIN key)
#define zcc_bool(value, desc, key) \
zconf_create_boolean(value, desc, ZCONF_DOMAIN key)
#endif /* ZCONF_DOMAIN */
