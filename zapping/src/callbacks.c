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
#include "plugins.h"
#include "zconf.h"
/* Manages config values for zconf (it saves me some typing) */
#define ZCONF_DOMAIN "/zapping/internal/callbacks/"
#include "zmisc.h"

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
  zcc_int(30, _("X coord of the Zapping window"), "x");
  zcc_int(30, _("Y coord of the Zapping window"), "y");
  zcc_int(640, _("Width of the Zapping window"), "w");
  zcc_int(480, _("Height of the Zapping window"), "h");
  zcc_int(0, _("Currently tuned channel"), "cur_tuned_channel");
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

  UpdateCoords(widget->window);

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
  gnome_help_display (NULL, &help_ref);
}

gboolean
on_zapping_delete_event                (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  flag_exit_program = TRUE;

  UpdateCoords(widget->window);

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

void
on_plugins1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget * plugin_properties = create_plugin_properties();
  GtkWidget * text1 = lookup_widget(plugin_properties, "text1");
  GList * p = g_list_first(plugin_list); /* Iterate through the
					    plugins */
  struct plugin_info * plug_info;
  gchar * clist2_entries[4]; /* Contains the entries for the CList */
  gchar buffer[256];
  GtkWidget * clist2 = lookup_widget(plugin_properties, "clist2");
  buffer[255] = 0;
  
  gtk_object_set_user_data(GTK_OBJECT(plugin_properties), menuitem);
  gtk_widget_set_sensitive(GTK_WIDGET(menuitem), FALSE);
  gtk_text_set_word_wrap(GTK_TEXT(text1), TRUE);

  /* Add the plugins to the CList */
  while (p)
    {
      plug_info = (struct plugin_info*) p->data;
      clist2_entries[0] = plugin_get_canonical_name(plug_info);
      clist2_entries[1] = plugin_get_name(plug_info);
      g_snprintf(buffer, 255, "%d.%d.%d", plug_info -> major, plug_info ->
		 minor, plug_info -> micro);
      clist2_entries[2] = buffer;
      clist2_entries[3] = plugin_running(plug_info) ? _("Yes") : _("No");
      gtk_clist_append(GTK_CLIST(clist2), clist2_entries);
      p = p->next;
    }

  gtk_widget_show(plugin_properties);
}


void
on_clist2_select_row                   (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  GdkColor blue;
  struct plugin_info * plug_info;
  GtkText * text1 = GTK_TEXT(lookup_widget(GTK_WIDGET(clist), "text1"));
  gchar buffer[256];

  /* lookup blue color */
  gdk_color_parse("blue", &blue);
  if (!gdk_colormap_alloc_color(gdk_rgb_get_cmap(), &blue,
				TRUE, TRUE))
    return;

  plug_info = (struct plugin_info*) g_list_nth_data(plugin_list, row);
  
  gtk_text_freeze(text1); /* We are going to do a number of
			     modifications */
  /* Delete all previous contents */
  gtk_editable_delete_text(GTK_EDITABLE(text1), 0, -1);

  gtk_text_insert(text1, NULL, &blue, NULL, _("Plugin Description: "),
		  -1);
  gtk_text_insert(text1, NULL, NULL, NULL, plugin_get_info(plug_info), -1);

  gtk_text_insert(text1, NULL, &blue, NULL, _("\nPlugin author: "), -1);
  gtk_text_insert(text1, NULL, NULL, NULL, plugin_author(plug_info), -1);

  g_snprintf(buffer, 255, "%d.%d.%d", plug_info -> zapping_major, plug_info ->
	     zapping_minor, plug_info -> zapping_micro);

  /* Adapt this and " required" to your own language structure freely */
  gtk_text_insert(text1, NULL, NULL, NULL, _("\nZapping "), -1);
  gtk_text_insert(text1, NULL, &blue, NULL, buffer, -1);
  gtk_text_insert(text1, NULL, NULL, NULL, _(" required"), -1);

  gtk_text_thaw(text1); /* Show the changes */
  gdk_colormap_free_colors(gdk_rgb_get_cmap(), &blue, 1);
}


void
on_button3_clicked                     (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget * plugin_properties = lookup_widget(GTK_WIDGET(button),
						"plugin_properties");

  /* Activate the menuitem and close the widget */
  gpointer menuitem = gtk_object_get_user_data(GTK_OBJECT(plugin_properties));
  gtk_widget_set_sensitive(GTK_WIDGET(menuitem), TRUE);
  gtk_widget_destroy(plugin_properties);
}


void
on_plugin_close_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
  /* We traverse the clist and call plugin_close on all the selected
     plugins */
  GtkWidget * clist2 = lookup_widget(GTK_WIDGET(button), "clist2");
  int i = 0;
  GList * ptr = GTK_CLIST(clist2) -> row_list;
  struct plugin_info * plug_info;
  
  while (ptr)
    {
      plug_info = (struct plugin_info*)g_list_nth(plugin_list, i)->data;
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	{
	  plugin_stop(plug_info);
	  gtk_clist_set_text(GTK_CLIST(clist2), i, 3,
			     plugin_running(plug_info) ? _("Yes") : _("No"));
	}
      i++;
      ptr = ptr -> next;
    }
}


void
on_plugin_apply_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
  /* We traverse the clist and call plugin_start on all the selected
     plugins, just a verbatim copy of the above */
  GtkWidget * clist2 = lookup_widget(GTK_WIDGET(button), "clist2");
  int i = 0;
  GList * ptr = GTK_CLIST(clist2) -> row_list;
  struct plugin_info * plug_info;
  
  while (ptr)
    {
      plug_info = (struct plugin_info*)g_list_nth(plugin_list, i)->data;
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	{
	  plugin_start(plug_info);
	  gtk_clist_set_text(GTK_CLIST(clist2), i, 3,
			     plugin_running(plug_info) ? _("Yes") : _("No"));
	}
      i++;
      ptr = ptr -> next;
    }
}

gboolean
on_plugin_properties_delete_event      (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  gpointer menuitem = gtk_object_get_user_data(GTK_OBJECT(widget));
  gtk_widget_set_sensitive(GTK_WIDGET(menuitem), TRUE);

  return FALSE;
}
