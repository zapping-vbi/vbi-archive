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

#ifndef __V4LINTERFACE_H__
#define __V4LINTERFACE_H__

#include "frequencies.h"
#include "zmodel.h"
#include "zmisc.h"
#include "tveng.h"

/*
  Rebuilds the control box if it's open. Call whenever the device
  controls change.
*/
void
update_control_box(tveng_device_info * info);

/* Notification of standard/input changes */
extern ZModel *z_input_model;

/**
 * Sets the given input, based on its hash.
 */
gboolean
z_switch_video_input		(guint hash, tveng_device_info *info);

gboolean
z_switch_audio_input		(guint hash, tveng_device_info *info);

/**
 * Sets the given standard, based on its hash.
 */
gboolean
z_switch_standard		(guint hash, tveng_device_info *info);

/**
 * Sets the given channel.
 */
void
z_switch_channel		(tveng_tuned_channel	*channel,
				 tveng_device_info	*info);


gboolean
channel_key_press		(GdkEventKey *		event);

gboolean
on_channel_key_press		(GtkWidget *	widget,
				 GdkEventKey *	event,
				 gpointer	user_data);

/**
 * Sets the given channel.
 */
void
z_set_main_title		(tveng_tuned_channel	*channel,
				 gchar *default_name);

extern tveng_tc_control *
zconf_get_controls		(guint			num_controls,
				 const gchar *		path);
extern void
zconf_create_controls		(tveng_tc_control *	tcc,
				 guint			num_controls,
				 const gchar *		path);
tveng_tc_control *
tveng_tc_control_by_id		(const tveng_device_info *info,
				 tveng_tc_control *tcc,
				 guint			num_controls,
				 tv_control_id		id);
extern gint
load_control_values		(tveng_device_info *	info,
				 tveng_tc_control *	tcc,
				 guint			num_controls);
void
store_control_values		(tveng_device_info *	info,
				 tveng_tc_control **	tcc,
				 guint *		num_controls);

/* Returns whether something (useful) was added */
gboolean
add_channel_entries		(GtkMenuShell *menu,
				 gint pos,
				 guint menu_max_entries,
				 tveng_device_info *info);

/* Do the startup/shutdown */
void startup_v4linterface	(tveng_device_info *info);
void shutdown_v4linterface	(void);

extern gdouble videostd_inquiry(void);

#endif
