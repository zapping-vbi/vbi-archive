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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "tveng.h"
#include "callbacks.h"
#include "interface.h"
#include "v4linterface.h"
/* Manages config values for zconf (it saves me some typing) */
#define ZCONF_DOMAIN "/zapping/internal/callbacks/"
#include "zmisc.h"
#include "plugins.h"
#include "zconf.h"

gboolean flag_exit_program; /* set this flag to TRUE to exit the program */
GtkWidget * ToolBox = NULL; /* Here is stored the Toolbox (if any) */

extern tveng_device_info * main_info; /* About the device we are using */

int cur_tuned_channel = 0; /* Currently tuned channel */

GtkWidget * black_window = NULL; /* The black window when you go
				    preview */

extern GList * plugin_list; /* The plugins we have */

/* Starts and stops callbacks */
gboolean startup_callbacks(void)
{
  /* Init values to their defaults */
  zcc_int(30, "X coord of the Zapping window", "x");
  zcc_int(30, "Y coord of the Zapping window", "y");
  zcc_int(640, "Width of the Zapping window", "w");
  zcc_int(480, "Height of the Zapping window", "h");
  zcc_int(0, "Currently tuned channel", "cur_tuned_channel");
  cur_tuned_channel = zcg_int(NULL, "cur_tuned_channel");

  return TRUE;
}

void shutdown_callbacks(void)
{
  zcs_int(cur_tuned_channel, "cur_tuned_channel");
}

/* Gets the geometry of the main window */
void UpdateCoords(GdkWindow * window);

void UpdateCoords(GdkWindow * window)
{
  int x, y, w, h;
  gdk_window_get_origin(window, &x, &y);
  gdk_window_get_size(window, &w, &h);
  
  zcs_int(x, "x");
  zcs_int(y, "y");
  zcs_int(w, "w");
  zcs_int(h, "h");
}

void
on_exit2_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget * widget = lookup_widget(GTK_WIDGET(menuitem), "zapping");
  GList * p;
  UpdateCoords(widget->window);

  /* Tell the widget that the GUI is going to be closed */
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_remove_gui(GNOME_APP(widget), 
			(struct plugin_info*)p->data);
      p = p->next;
    }

  flag_exit_program = TRUE;
}

void
on_about1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  gtk_widget_show(create_about2());
}

void
on_plugin_writing1_activate            (GtkMenuItem     *menuitem,
					gpointer         user_data)
{
  static GnomeHelpMenuEntry help_ref = { "zapping",
					 "plugin_devel.html" };
  enum tveng_capture_mode cur_mode;

  cur_mode = tveng_stop_everything(main_info);

  gnome_help_display (NULL, &help_ref);

  if (tveng_restart_everything(cur_mode, main_info) == -1)
    ShowBox(main_info->error, GNOME_MESSAGE_BOX_ERROR);
}

void
on_main_help1_activate                 (GtkMenuItem     *menuitem,
					gpointer         user_data)
{
  static GnomeHelpMenuEntry help_ref = { "zapping",
					 "index.html" };
  enum tveng_capture_mode cur_mode;

  cur_mode = tveng_stop_everything(main_info);

  gnome_help_display (NULL, &help_ref);

  if (tveng_restart_everything(cur_mode, main_info) == -1)
    ShowBox(main_info->error, GNOME_MESSAGE_BOX_ERROR);
}

gboolean
on_zapping_delete_event                (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  GList * p;
  flag_exit_program = TRUE;
  UpdateCoords(widget->window);

  /* Tell the widget that the GUI is going to be closed */
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_remove_gui(GNOME_APP(widget), 
			 (struct plugin_info*)p->data);
      p = p->next;
    }

  return FALSE;
}

void
on_tv_screen_size_allocate             (GtkWidget       *widget,
                                        GtkAllocation   *allocation,
                                        gpointer         user_data)
{
  /* Delete the old image */
  zimage_destroy();

  if (tveng_set_capture_size(allocation->width, allocation->height, 
			     main_info) == -1)
    {
      fprintf(stderr, "%s\n", main_info->error);
      return;
    }

  /* Reallocate a new image */
  zimage_reallocate(main_info->format.width, main_info->format.height);
}

/* Activate an standard */
void on_standard_activate              (GtkMenuItem     *menuitem,
					gpointer        user_data)
{
  gint std = GPOINTER_TO_INT(user_data);
  GtkWidget * main_window = lookup_widget(GTK_WIDGET(menuitem), "zapping");

  if (std == main_info->cur_standard)
    return; /* Do nothing if this standard is already active */

  if (tveng_set_standard_by_index(std, main_info) == -1) /* Set the
							 standard */
    {
      ShowBox(main_info -> error, GNOME_MESSAGE_BOX_ERROR);
      return;
    }

  /* redesign menus */
  update_standards_menu(main_window, main_info);
}

/* Activate an input */
void on_input_activate              (GtkMenuItem     *menuitem,
				     gpointer        user_data)
{
  gint input = GPOINTER_TO_INT (user_data);
  GtkWidget * main_window = lookup_widget(GTK_WIDGET(menuitem), "zapping");

  if (main_info -> cur_input == input)
    return; /* Nothing if no change is needed */

  if (tveng_set_input_by_index(input, main_info) == -1) /* Set the
							 input */
    {
      ShowBox(_("Cannot set input"), GNOME_MESSAGE_BOX_ERROR);
      return;
    }

  update_standards_menu(main_window, main_info);
}

/* Activate a TV channel */
void on_channel_activate              (GtkMenuItem     *menuitem,
				       gpointer        user_data)
{
  gint num_channel = GPOINTER_TO_INT(user_data);
  int mute = 0;

  tveng_tuned_channel * channel =
    tveng_retrieve_tuned_channel_by_index(num_channel); 

  if (!channel)
    {
      printf(_("Cannot tune given channel %d (no such channel)\n"), 
	     num_channel);
      return;
    }

  if (zconf_get_boolean(NULL, "/zapping/options/main/avoid_noise"))
    {
      mute = tveng_get_mute(main_info);
      
      if (!mute)
	tveng_set_mute(1, main_info);
    }

  if (tveng_tune_input(channel->freq, main_info) == -1) /* Set the
						       input freq*/
    ShowBox(_("Cannot tune input"), GNOME_MESSAGE_BOX_ERROR);

  if (zconf_get_boolean(NULL, "/zapping/options/main/avoid_noise"))
    {
      /* Sleep a little so the noise dissappears */
      usleep(100000);
      
      if (!mute)
	tveng_set_mute(0, main_info);
    }

  cur_tuned_channel = num_channel; /* Set the current channel to this */
}

void
on_controls_clicked                    (GtkButton       *button,
                                        gpointer         user_data)
{
  if (ToolBox)
    {
      gdk_window_raise(ToolBox -> window);
      return;
    }

  ToolBox = create_control_box(main_info);

  gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);

  gtk_object_set_user_data(GTK_OBJECT(ToolBox), button);

  gtk_widget_show(ToolBox);
}

void
on_control_slider_changed              (GtkAdjustment *adjust,
					gpointer user_data)
{
  /* Control id */
  gint cid = GPOINTER_TO_INT (user_data);

  g_assert(cid < main_info -> num_controls);

  tveng_set_control(&(main_info->controls[cid]), (int)adjust->value,
		    main_info);
}

void
on_control_checkbutton_toggled         (GtkToggleButton *tb,
					gpointer user_data)
{
  gint cid = GPOINTER_TO_INT (user_data);

  g_assert(cid < main_info -> num_controls);

  tveng_set_control(&(main_info-> controls[cid]),
		    gtk_toggle_button_get_active(tb),
		    main_info);
}

void
on_control_menuitem_activate           (GtkMenuItem *menuitem,
					gpointer user_data)
{
  gint cid = GPOINTER_TO_INT (user_data);

  int value = (int) gtk_object_get_data(GTK_OBJECT(menuitem),
					"value");

  g_assert(cid < main_info -> num_controls);

  tveng_set_control(&(main_info -> controls[cid]), value, main_info);
}

void
on_control_button_clicked              (GtkButton *button,
					gpointer user_data)
{
  gint cid = GPOINTER_TO_INT (user_data);

  g_assert(cid < main_info -> num_controls);

  tveng_set_control(&(main_info->controls[cid]), 1, main_info);
}

gboolean
on_control_box_delete_event            (GtkWidget      *widget,
					GdkEvent       *event,
					gpointer        user_data)
{
  GtkWidget * related_button;

  related_button =
    GTK_WIDGET(gtk_object_get_user_data(GTK_OBJECT(widget)));

  gtk_widget_set_sensitive(related_button, TRUE);

  ToolBox = NULL;

  return FALSE;
}

void
on_go_fullscreen1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget * da; /* Drawing area */

  /* Return if we are in fullscreen mode now */
  if (main_info->current_mode == TVENG_CAPTURE_PREVIEW)
    return;

  /* Add a black background */
  black_window = gtk_window_new( GTK_WINDOW_POPUP );
  da = gtk_drawing_area_new();

  gtk_container_add(GTK_CONTAINER(black_window), da);
  gtk_widget_set_usize(black_window, gdk_screen_width(), gdk_screen_height());
  gtk_widget_show(da);

  gtk_widget_show(black_window);

  /* Draw on the drawing area */
  gdk_draw_rectangle(da -> window,
		     da -> style -> black_gc,
		     TRUE,
		     0, 0, gdk_screen_width(), gdk_screen_height());

  if (tveng_start_previewing(main_info) == -1)
    {
      ShowBox(_("Sorry, but cannot go fullscreen"),
	      GNOME_MESSAGE_BOX_ERROR);
      tveng_start_capturing(main_info);
      return;
    }

  if (main_info -> current_mode != TVENG_CAPTURE_PREVIEW)
    g_warning("Setting preview succeeded, but the mode is not set");

  /* Grab the keyboard to the main zapping window */
  gdk_keyboard_grab(lookup_widget(GTK_WIDGET(menuitem), "zapping")->window,
		    TRUE,
		    GDK_CURRENT_TIME);
}


void
on_go_windowed1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  int w,h;
  GtkWidget * widget;
  GtkAllocation dummy_alloc;

  if (main_info->current_mode != TVENG_CAPTURE_PREVIEW)
    return;

  if (-1 == tveng_start_capturing(main_info))
    {
      ShowBox(main_info -> error, GNOME_MESSAGE_BOX_ERROR);
      return;
    }

  widget = lookup_widget(GTK_WIDGET(menuitem), "tv_screen");
  if (!widget)
    {
      ShowBox(_("I cannot find the main zapping window, weird..."),
	      GNOME_MESSAGE_BOX_ERROR);
      return;      
    }

  /* Fake a resize (to the actual size), this will update all capture
     structs */
  gdk_window_get_size(widget -> window, &w, &h);

  dummy_alloc.width = w;
  dummy_alloc.height = h;
  on_tv_screen_size_allocate(widget, &dummy_alloc, NULL);

  /* Ungrab the previously grabbed keyboard */
  gdk_keyboard_ungrab(GDK_CURRENT_TIME);

  /* Remove the black window */
  gtk_widget_destroy(black_window);

}

void
on_channel_up1_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  int num_channels = tveng_tuned_channel_num();
  GtkWidget * Channels = lookup_widget(GTK_WIDGET(menuitem),
					     "Channels");

  int new_channel;

  if (num_channels == 0) /* If there are no tuned channels stop
			    processing */
    return;

  new_channel = cur_tuned_channel - 1;
  if (new_channel < 0)
    new_channel = num_channels - 1;

  /* Simulate a callback */
  on_channel_activate(NULL, GINT_TO_POINTER(new_channel));
  
  /* Update the option menu */
  gtk_option_menu_set_history(GTK_OPTION_MENU (Channels),
			      new_channel);
}


void
on_channel_down1_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  int num_channels = tveng_tuned_channel_num();
  GtkWidget * Channels = lookup_widget(GTK_WIDGET(menuitem),
					     "Channels");

  int new_channel;

  if (num_channels == 0) /* If there are no tuned channels stop
			    processing */
    return;

  new_channel = cur_tuned_channel + 1;
  if (new_channel >= num_channels)
    new_channel = 0;

  /* Simulate a callback */
  on_channel_activate(NULL, GINT_TO_POINTER(new_channel));
  
  /* Update the option menu */
  gtk_option_menu_set_history(GTK_OPTION_MENU (Channels),
			      new_channel);
}
