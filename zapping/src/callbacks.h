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
#ifndef __CALLBACKS_H__
#define __CALLBACKS_H__

#include <gnome.h>

#include "tveng.h"

extern gboolean channels_updated;

/*
  This structure holds the configuration.
*/
struct config_struct
{
  gchar country[32]; /* Selected country */
  gchar tuned_channel[32]; /* Selected channel */
  gchar input[32]; /* Currently selected input */
  gchar standard[32]; /* Selected standard */
  gchar video_device[FILENAME_MAX]; /* Usually /dev/video */

  __u32 freq;        /* Currently tuned freq (maybe different from the
			selected channel) */

  /* PNG saving specific stuff */
  gchar png_prefix[32];
  gchar png_src_dir[PATH_MAX];
  int   png_show_progress; /* Should be boolean, but this way parsing
			      is somewhat easier "%d" */

  int capture_interlaced; /* TRUE if interlaced capture is on */

  int   x,y;         /* Last window pos */
  int   width,height; /* Last capture dimensions */
  int zapping_setup_fb_verbosity; /* Verbosity used with
				     zapping_setup_fb */
  int avoid_noise; /* TRUE if we should sleep() when tuning channels */
};

void
on_country_switch                      (GtkMenuItem     *menu_item,
					tveng_channels  *country);

void
on_exit2_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_channels1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_about1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

gboolean
on_zapping_delete_event                (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_tv_screen_size_allocate             (GtkWidget       *widget,
                                        GtkAllocation   *allocation,
                                        gpointer         user_data);


void on_standard_activate              (GtkMenuItem     *menuitem,
					gpointer        user_data);

void on_input_activate                 (GtkMenuItem     *menuitem,
					gpointer        user_data);

/* Activate a TV channel */
void on_channel_activate              (GtkMenuItem     *menuitem,
				       gpointer        user_data);

void
on_controls_clicked                    (GtkButton       *button,
                                        gpointer         user_data);

void
on_control_slider_changed              (GtkAdjustment *adjust,
					gpointer user_data);

void
on_control_checkbutton_toggled         (GtkToggleButton *tb,
					gpointer user_data);

void
on_control_menuitem_activate           (GtkMenuItem *menuitem,
					gpointer user_data);

void
on_control_button_clicked              (GtkButton *button,
					gpointer user_data);

void
on_channels_done_clicked               (GtkButton       *button,
                                        gpointer         user_data);

void
on_add_channel_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

void
on_remove_channel_clicked              (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_channel_window_delete_event         (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_control_box_delete_event            (GtkWidget      *widget,
					GdkEvent       *event,
					gpointer        user_data);

void
on_clist1_select_row                   (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data);

gboolean
on_channel_window_delete_event         (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_screenshot_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_propiedades1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_zapping_properties_apply            (GnomePropertyBox *gnomepropertybox,
                                        gint             arg1,
                                        gpointer         user_data);

void
on_zapping_properties_help             (GnomePropertyBox *gnomepropertybox,
                                        gint             arg1,
                                        gpointer         user_data);

/* This function is called when some item in the property box changes */
void
on_property_item_changed               (GtkWidget * changed_widget,
				        GnomePropertyBox *propertybox);

#endif /* __CALLBACKS_H__ */

void
on_go_fullscreen1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_go_windowed1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_channel_up1_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_channel_down1_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);
