/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2004 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: zgconf.h,v 1.3 2005-01-08 14:29:30 mschimek Exp $ */

#ifndef Z_GCONF_H
#define Z_GCONF_H

#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

#include "zmisc.h"		/* _unused_ */

extern const gchar *
z_gconf_value_type_name		(GConfValueType		type);
extern gboolean
z_gconf_get			(gpointer		result,
				 const gchar *		key,
				 GConfValueType		type);
extern gboolean
z_gconf_auto_update		(gpointer		var,
				 const gchar *		key,
				 GConfValueType		type);

#define Z_GCONF_SET_GET_NOTIFY(T1, T2, type)				\
extern gboolean								\
z_gconf_set_##T1 (const gchar *key, T2 value);				\
static __inline__ gboolean						\
z_gconf_get_##T1 (T2 *result, const gchar *key)				\
{ return z_gconf_get (result, key, GCONF_VALUE_##type); }		\
static __inline__ gboolean						\
z_gconf_auto_update_##T1 (T2 *var, const gchar *key)			\
{ return z_gconf_auto_update (var, key, GCONF_VALUE_##type); }

Z_GCONF_SET_GET_NOTIFY (bool, gboolean, BOOL)
Z_GCONF_SET_GET_NOTIFY (int, gint, INT)
Z_GCONF_SET_GET_NOTIFY (float, gdouble, FLOAT)

extern gboolean
z_gconf_set_string		(const gchar *		key,
				 const gchar *		string);
static __inline__ gboolean
z_gconf_get_string		(gchar **		result,
				 const gchar *		key)
{
  return z_gconf_get (result, key, GCONF_VALUE_STRING);
}

extern gboolean
z_gconf_get_string_enum		(gint *			enum_value,
				 const gchar *		gconf_key,
				 const GConfEnumStringPair *lookup_table);
extern gboolean
z_gconf_notify_add		(const gchar *		key,
				 GConfClientNotifyFunc	func,
				 gpointer		user_data);
extern void
z_toggle_action_connect_gconf_key
				(GtkToggleAction *	toggle_action,
				 const gchar *		key);
extern GtkWidget *
z_gconf_check_button_new	(const gchar *		label,
				 const gchar *		key,
				 gboolean *		var,
				 gboolean		active);
extern GtkWidget *
z_gconf_int_spinslider_new	(gint			def_value,
				 gint			min_value,
				 gint			max_value,
				 gint			step_incr,
				 gint			page_incr,
				 gint			page_size,
				 const gchar *		key,
				 gint *			var);
GtkWidget *
z_gconf_float_spinslider_new	(gdouble		def_value,
				 gdouble		min_value,
				 gdouble		max_value,
				 gdouble		step_incr,
				 gdouble		page_incr,
				 gdouble		page_size,
				 gint			digits,
				 const gchar *		key,
				 gdouble *		var);
extern GtkWidget *
z_gconf_combo_box_new		(const gchar **		menu,
				 const gchar *		key,
				 const GConfEnumStringPair *lookup_table);

#endif /* Z_GCONF_H */
