/*
 * Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
 * Copyright (C) 2002 Michael H. Schimek
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

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <gdk/gdk.h>
#include <gtk/gtk.h>

extern void
startup_keyboard		(void);
extern void
shutdown_keyboard		(void);

typedef struct z_key {
  guint			key;
  guint			mask;
} z_key;

extern gchar *
z_key_name			(z_key			key);
extern z_key
z_key_from_name			(const gchar *		name);

#define Z_KEY_NONE ({ z_key none = { 0, 0 }; none; })

/*
 *  Note: keyvals are supposed to be lower case.
 */
static inline gboolean
z_key_equal			(z_key			key1,
				 z_key			key2)
{
  const guint mask = GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SHIFT_MASK;

  key1.key ^= key2.key;
  key1.mask ^= key2.mask;

  return ((key1.key | (key1.mask & mask)) == 0);
}

extern void
zconf_create_z_key		(z_key			key,
				 const gchar *	 	desc,
				 const gchar *		path);
extern void
zconf_set_z_key			(z_key			key,
				 const gchar *		path);
extern z_key
zconf_get_z_key			(z_key *		keyp,
				 const gchar *		path);

extern GtkWidget *
z_key_entry_new			(void);
extern GtkWidget *
z_key_entry_entry		(GtkWidget *		hbox);
extern void
z_key_entry_set_key		(GtkWidget *		hbox,
				 z_key			key);
extern z_key
z_key_entry_get_key		(GtkWidget *		hbox);

extern void
z_widget_add_accelerator	(GtkWidget *		widget,
				 const gchar *		accel_signal,
				 guint			accel_key,
				 guint			accel_mods);
extern gboolean
on_user_key_press		(GtkWidget *		widget,
				 GdkEventKey *		event,
				 gpointer		user_data);

#endif /* KEYBOARD_H */
