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
#  include "config.h"
#endif

#ifdef HAVE_LIBZVBI

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libzvbi.h>

#include "tveng.h"
#include "zmodel.h"
#include "frequencies.h"

#include "libvbi/vbi_decoder.h"
#include "libvbi/export.h"

extern void
startup_zvbi			(void);
extern void
shutdown_zvbi			(void);

extern void
zvbi_stop			(gboolean		destroy_decoder);
extern gboolean
zvbi_start			(void);

/* Returns the global vbi object, or NULL if vbi isn't enabled or
   doesn't work. You can safely use this function to test if VBI works
   (it will return NULL if it doesn't). */
vbi3_decoder *
zvbi_get_object			(void);
/* Returns the model for the vbi object. You can hook into this to know
   when the vbi object is created or destroyed. */
ZModel *
zvbi_get_model			(void);
/* Returns the g_strdup'ed name of the current station, if known, or
   NULL. The returned value should be g_free'ed. */
gchar *
zvbi_get_name			(void);
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

extern gchar *
zvbi_language_name		(const vbi3_ttx_charset *cs);
extern gboolean
zvbi_cur_channel_get_ttx_encoding
				(vbi3_ttx_charset_code *charset_code,
				 vbi3_pgno		pgno);
extern gboolean
zvbi_cur_channel_set_ttx_encoding
				(vbi3_pgno		pgno,
				 vbi3_ttx_charset_code	charset_code);

typedef struct _zvbi_encoding_menu zvbi_encoding_menu;

struct _zvbi_encoding_menu {
  zvbi_encoding_menu *		next;
  GtkCheckMenuItem *		item;
  gchar *			name;
  vbi3_ttx_charset_code		code;
  gpointer			user_data;
};

typedef void
zvbi_encoding_menu_toggled_cb	(GtkCheckMenuItem *	menu_item,
				 zvbi_encoding_menu *	em);
extern void
zvbi_encoding_menu_set_active	(GtkMenu *		menu,
				 vbi3_ttx_charset_code	code);
extern GtkMenu *
zvbi_create_encoding_menu	(zvbi_encoding_menu_toggled_cb *callback,
				 gpointer		user_data);

typedef gchar *
zvbi_xot_zcname_fn		(const vbi3_export *	e,
				 const vbi3_option_info *oi,
				 gpointer		user_data);
extern GtkWidget *
zvbi_export_option_table_new	(vbi3_export *		context,
				 zvbi_xot_zcname_fn *	zconf_name,
				 gpointer		user_data);
extern gboolean
zvbi_export_load_zconf		(vbi3_export *		context,
				 zvbi_xot_zcname_fn *	zconf_name,
				 gpointer		user_data);

vbi3_pgno
zvbi_find_subtitle_page		(tveng_device_info *	info);

#else /* !HAVE_LIBZVBI */

/* Startups the VBI engine (doesn't open devices) */
void
startup_zvbi			(void);
/* Shuts down the VBI engine */
void
shutdown_zvbi			(void);

typedef int vbi_pgno;

#endif /* !HAVE_LIBZVBI */

#endif /* zvbi.h */

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
