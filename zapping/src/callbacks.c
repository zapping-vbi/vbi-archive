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

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "v4l2interface.h"
#include "plugins.h"

gboolean flag_exit_program; /* set this flag to TRUE to exit the program */
GtkWidget * ChannelWindow = NULL; /* Here is stored the channel editor
				   widget (if any) */
GtkWidget * ToolBox = NULL; /* Here is stored the Toolbox (if any) */

extern tveng_channels * current_country; /* Currently selected contry */

gboolean channels_updated = FALSE; /* TRUE if there are pending channel
				      updates */
extern tveng_device_info info; /* About the main device*/
extern int zapping_window_x, zapping_window_y; /* Main Window
						  coordinates */
extern int zapping_window_width, zapping_window_height;

int cur_tuned_channel = 0; /* Currently tuned channel */

gboolean take_screenshot = FALSE; /* Set to TRUE if you want a
				     screenshot */

GtkWidget * black_window = NULL; /* The black window when you go
				    preview */

extern GList * plugin_list; /* The plugins we have */

extern struct config_struct config;

/* Gets the geometry of the main window */
void UpdateCoords(GdkWindow * window);

void UpdateCoords(GdkWindow * window)
{
  gdk_window_get_origin(window, &zapping_window_x,
			&zapping_window_y);

  gdk_window_get_size(window, &zapping_window_width,
		      &zapping_window_height);
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
on_country_switch                      (GtkMenuItem     *menu_item,
					tveng_channels  *country)
{
  GtkWidget * clist1 = lookup_widget(GTK_WIDGET(menu_item), "clist1");

  tveng_channel * channel;
  int id=0;

  gchar new_entry_0[128];
  gchar new_entry_1[128];
  gchar *new_entry[] = {new_entry_0, new_entry_1}; /* Allocate room
						      for new entries */
  new_entry[0][127] = new_entry[1][127] = 0;

  /* Set the current country */
  current_country = country;

  gtk_clist_freeze( GTK_CLIST(clist1)); /* We are going to do a number
					   of changes */

  gtk_clist_clear( GTK_CLIST(clist1));
  
  /* Get all available channels for this country */
  while ((channel = tveng_get_channel_by_id(id, country)))
    {
      g_snprintf(new_entry[0], 127, "%s", channel->name);
      g_snprintf(new_entry[1], 127, "%u", channel->freq);
      gtk_clist_append(GTK_CLIST(clist1), new_entry);
      id++;
    }

  gtk_clist_thaw( GTK_CLIST(clist1));

  /* Set the current country as the user data of the clist */
  gtk_object_set_user_data ( GTK_OBJECT(clist1), country);
}

void
on_channels1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget * channel_window;
  GtkWidget * country_options_menu;

  GtkWidget * channel_list;

  GtkWidget * new_menu;
  GtkWidget * menu_item = NULL;

  int i = 0;
  int currently_tuned_country = 0;

  tveng_channels * tune;
  tveng_tuned_channel * tuned_channel;

  gchar name[256];
  gchar real[256];
  gchar freq[256];

  gchar *entry[] = {name, real, freq};

  name[255] = real[255] = freq[255] = 0;

  if (ChannelWindow)
    {
      gtk_widget_grab_focus(ChannelWindow);
      return;
    }

  channel_window = create_channel_window();
  country_options_menu = lookup_widget(channel_window,
				       "country_options_menu");

  channel_list = lookup_widget(channel_window, "channel_list");
  new_menu = gtk_menu_new();

  /* Let's setup the window */
  gtk_widget_destroy(gtk_option_menu_get_menu (GTK_OPTION_MENU
					       (country_options_menu)));

  while ((tune = tveng_get_country_tune_by_id(i)))
    {
      i++;
      if (tune == current_country)
	currently_tuned_country = i-1;
      menu_item = gtk_menu_item_new_with_label(_(tune->name));
      gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
			 GTK_SIGNAL_FUNC(on_country_switch),
			 tune);
      gtk_widget_show(menu_item);
      gtk_menu_append( GTK_MENU(new_menu), menu_item);
    }

  gtk_widget_show(new_menu);

  gtk_option_menu_set_menu( GTK_OPTION_MENU(country_options_menu),
			    new_menu);

  gtk_option_menu_set_history ( GTK_OPTION_MENU(country_options_menu),
				currently_tuned_country);
  
  /* Change contry to the currently tuned one */
  if (menu_item)
    on_country_switch(GTK_MENU_ITEM(menu_item), 
		      current_country);

  /* Setup the channel list */
  i = 0;

  while ((tuned_channel = tveng_retrieve_tuned_channel_by_index(i)))
    {
      i++;
      g_snprintf(entry[0], 255, tuned_channel->name);
      g_snprintf(entry[1], 255, tuned_channel->real_name);
      g_snprintf(entry[2], 255, "%u", tuned_channel->freq);
      gtk_clist_append(GTK_CLIST(channel_list), entry);
    }

  /* Save the disabled menuitem */
  gtk_object_set_user_data(GTK_OBJECT(channel_window), menuitem);

  gtk_widget_set_sensitive(GTK_WIDGET(menuitem), FALSE);

  gtk_widget_show(channel_window);

  ChannelWindow = channel_window; /* Set this, we are present */
}

void
on_about1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  gtk_widget_show(create_about2());
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
  int w2,h2;

  /* Set to nearest 4-multiplus */
  /* This way some line-padding problems I got dissapear */
  int w = (allocation->width+3) & (~3);
  int h = (allocation->height+3) & (~3);

  if (GTK_WIDGET_REALIZED(widget))
    {
      gdk_window_get_size(widget -> window, &w2, &h2);

      /* Don't call reallocate on this one */
      if ((w2 == w) && (h2 == h))
	return;
    }

  if (tveng_set_capture_size(w, h,
			     &info) == -1)
    {
      ShowBox(_("Sorry, but I cannot set the capture size correctly, exiting"),
	      GNOME_MESSAGE_BOX_ERROR);
      flag_exit_program = TRUE;
    }

  if (tveng_get_capture_size(&w, &h, &info) == -1)
    {
      ShowBox(_("Sorry, but I cannot get the capture size correctly, exiting"),
	      GNOME_MESSAGE_BOX_ERROR);
      flag_exit_program = TRUE;
    }

  /* This function shouldn't be called before the widget is shown
     (realized) */
  if (GTK_WIDGET_REALIZED(widget))
    gdk_window_resize(widget -> window, w, h);
}

/* Activate an standard */
void on_standard_activate              (GtkMenuItem     *menuitem,
					gpointer        user_data)
{
  tveng_enumstd * std = (tveng_enumstd *) user_data; 
  tveng_device_info * info = std -> info;

  if (!strcasecmp(info->cur_standard.name, std->std.std.name))
    return; /* Do nothing if this standard is already active */

  if (tveng_set_standard(std->std.std.name, info) == -1) /* Set the
							 standard */
    {
      ShowBox(_("Cannot set standard"), GNOME_MESSAGE_BOX_ERROR);
      return;
    }

  /* redesign menus */
  update_inputs_menu(GTK_WIDGET(menuitem), info);
}

/* Activate an input */
void on_input_activate              (GtkMenuItem     *menuitem,
				     gpointer        user_data)
{
  tveng_input * input = (tveng_input *) user_data; 
  tveng_device_info * info = input -> info;

  if (tveng_set_input(input->input.index, info) == -1) /* Set the
							 input */
    {
      ShowBox(_("Cannot set input"), GNOME_MESSAGE_BOX_ERROR);
      return;
    }
  update_channels_menu(GTK_WIDGET(menuitem), info);
}

/* Activate a TV channel */
void on_channel_activate              (GtkMenuItem     *menuitem,
				       gpointer        user_data)
{
  int num_channel = (int) user_data;
  int mute;

  tveng_tuned_channel * channel =
    tveng_retrieve_tuned_channel_by_index(num_channel); 

  if (!channel)
    {
      printf(_("Cannot tune given channel %d (no such channel)\n"), 
	     num_channel);
      return;
    }

  if (config.avoid_noise)
    {
      tveng_get_mute(&mute, &info);
      
      if (!mute)
	tveng_set_mute(1, &info);
    }

  if (tveng_tune_input(info.cur_input,
		       channel->freq, &info) == -1) /* Set the
						       input freq*/
    ShowBox(_("Cannot tune input"), GNOME_MESSAGE_BOX_ERROR);

  if (config.avoid_noise)
    {
      /* Sleep a little so the noise dissappears */
      usleep(100000);
      
      if (!mute)
	tveng_set_mute(0, &info);
    }

  cur_tuned_channel = num_channel; /* Set the current channel to this */
}

void
on_controls_clicked                    (GtkButton       *button,
                                        gpointer         user_data)
{
  if (ToolBox)
    return;

  ToolBox = create_control_box(&info);

  gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);

  gtk_object_set_user_data(GTK_OBJECT(ToolBox), button);

  gtk_widget_show(ToolBox);
}

void
on_control_slider_changed              (GtkAdjustment *adjust,
					gpointer user_data)
{
  __u32 cid = (__u32) user_data;
  tveng_device_info * info = gtk_object_get_data(GTK_OBJECT(adjust),
						 "info");
  tveng_set_control(cid, (int)adjust->value, info);
}

void
on_control_checkbutton_toggled         (GtkToggleButton *tb,
					gpointer user_data)
{
  __u32 cid = (__u32) user_data;
  tveng_device_info * info = gtk_object_get_data(GTK_OBJECT(tb),
						 "info");
  tveng_set_control(cid, gtk_toggle_button_get_active(tb), info);
}

void
on_control_menuitem_activate           (GtkMenuItem *menuitem,
					gpointer user_data)
{
  __u32 cid = (__u32) user_data;
  tveng_device_info * info = gtk_object_get_data(GTK_OBJECT(menuitem),
						 "info");
  int value = (int) gtk_object_get_data(GTK_OBJECT(menuitem),
					"value");

  tveng_set_control(cid, value, info);
}

void
on_control_button_clicked              (GtkButton *button,
					gpointer user_data)
{
  __u32 cid = (__u32) user_data;
  tveng_device_info * info = gtk_object_get_data (GTK_OBJECT(button),
						  "info");
  tveng_set_control(cid, 1, info);
}

/* 
   This is called when we are done processing the channels, to update
   the GUI
*/
void
on_channels_done_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget * channel_list = lookup_widget(GTK_WIDGET(button),
					   "channel_list");
  GtkWidget * channel_window = lookup_widget(GTK_WIDGET (button),
					     "channel_window"); /* The
					     channel editor window */

  GList * ptr; /* Pointer to the selected item(s) in clist1 */
  int index; /* The row we are reading now */
  gchar * dummy_ptr; /* We need this one for getting the freq */

  tveng_tuned_channel tc;

  /* Clear tuned channel list */
  tveng_clear_tuned_channel();

  index = 0;

  ptr = GTK_CLIST(channel_list) -> row_list;

  while (ptr)
    {
      /* Add this selected channel to the channel list */
      gtk_clist_get_text(GTK_CLIST(channel_list), index, 0, &(tc.name));
      gtk_clist_get_text(GTK_CLIST(channel_list), index, 1,
			 &(tc.real_name));

      gtk_clist_get_text(GTK_CLIST(channel_list), index, 2,
			 &(dummy_ptr));

      g_assert(dummy_ptr != NULL);

      if (!sscanf(dummy_ptr, "%u", &(tc.freq)))
	  g_warning(_("Cannot sscanf() unsigned integer from %s"),
		    dummy_ptr);

      tveng_insert_tuned_channel(&tc);

      ptr = ptr -> next;
      index++;
    }

  /* We are done, acknowledge the update in the channel list */
  channels_updated = TRUE;

  gtk_widget_set_sensitive(GTK_WIDGET(
		  gtk_object_get_user_data(GTK_OBJECT(channel_window))),
			   TRUE);

  gtk_widget_destroy(channel_window);

  ChannelWindow = NULL;
}

void
on_add_channel_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget * clist1 = lookup_widget(GTK_WIDGET(button), "clist1");
  GtkWidget * channel_list = lookup_widget(GTK_WIDGET(button),
					   "channel_list");
  GtkWidget * channel_name = lookup_widget(GTK_WIDGET(button),
					   "channel_name");

  GList * ptr; /* Pointer to the selected item(s) in clist1 */
  int index = 0; /* The row we are reading now */

  gchar *entry[3];
  
  entry[0] = gtk_entry_get_text (GTK_ENTRY(channel_name));

  ptr = GTK_CLIST(clist1) -> row_list;

  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	{ /* Add this selected channel to the channel list */
	  gtk_clist_get_text(GTK_CLIST(clist1), index, 0,
			     &(entry[1]));
	  gtk_clist_get_text(GTK_CLIST(clist1), index, 1,
			     &(entry[2]));
	  gtk_clist_append(GTK_CLIST(channel_list), entry);
	}
      ptr = ptr -> next;
      index++;
    }
}

void
on_remove_channel_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget * channel_list = lookup_widget(GTK_WIDGET(button),
					   "channel_list");

  GList * ptr; /* Pointer to the selected item(s) in clist1 */
  int index; /* The row we are reading now */

 reinit_loop:; /* Could be programmed better, but this way it works */

  index = 0;

  ptr = GTK_CLIST(channel_list) -> row_list;

  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	{
	  /* Add this selected channel to the channel list */
	  gtk_clist_remove(GTK_CLIST(channel_list), index);
	  goto reinit_loop;
	}

      ptr = ptr -> next;
      index++;
    }
}

void
on_clist1_select_row                   (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  tveng_channels * country = (tveng_channels*)
    gtk_object_get_user_data( GTK_OBJECT(clist));
  tveng_channel * selected_channel = tveng_get_channel_by_id (row,
							      country);
  if ((!selected_channel) || (!country))
    {
      printf("Interface error: trying to switch to void channel\n");
      return;
    }

  tveng_tune_input (info.cur_input, selected_channel->freq, &info);
}

gboolean
on_channel_window_delete_event         (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  GtkWidget * related_menuitem =
    GTK_WIDGET(gtk_object_get_user_data(GTK_OBJECT(widget)));

  ChannelWindow = NULL; /* No more channel window */

  /* Set the menuentry sensitive again */
  gtk_widget_set_sensitive(related_menuitem, TRUE);

  return FALSE;
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
on_screenshot_clicked                  (GtkButton       *button,
                                        gpointer         user_data)
{
  take_screenshot = TRUE;
}


void
on_propiedades1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget * zapping_properties = create_zapping_properties();
  GList * p = g_list_first(plugin_list); /* For traversing the plugins
					  */

  /* Widget for assigning the callbacks (generic) */
  GtkWidget * widget;

  /* Connect the widgets to the apropiate callbacks, so the Apply
     button works correctly. Set the correct values too */
  widget = lookup_widget(zapping_properties, "checkbutton1");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
			       config.png_show_progress);
  gtk_signal_connect(GTK_OBJECT(widget), "toggled",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  widget = lookup_widget(zapping_properties, "entry1");
  gtk_entry_set_text(GTK_ENTRY(widget), config.png_prefix);
  gtk_signal_connect(GTK_OBJECT(widget), "changed",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  widget = lookup_widget(zapping_properties, "fileentry1");
  widget = gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY(widget));

  gtk_entry_set_text(GTK_ENTRY(widget),
		     config.png_src_dir);

  gtk_signal_connect(GTK_OBJECT(widget), "changed",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  widget = lookup_widget(zapping_properties, "checkbutton2");
  config.capture_interlaced = info.interlaced;
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
			       config.capture_interlaced);

  gtk_signal_connect(GTK_OBJECT(widget), "toggled",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  widget = lookup_widget(zapping_properties, "spinbutton1");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),
			    info.num_desired_buffers);

  gtk_signal_connect(GTK_OBJECT(widget), "changed",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  widget = lookup_widget(zapping_properties, "fileentry2"); /* Video
							       device */
  widget = gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY(widget));

  gtk_entry_set_text(GTK_ENTRY(widget),
		     config.video_device);

  gtk_signal_connect(GTK_OBJECT(widget), "changed",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* zapping_setup_fb verbosity */
  widget = lookup_widget(zapping_properties, "spinbutton2"); 
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),
			    config.zapping_setup_fb_verbosity);

  gtk_signal_connect(GTK_OBJECT(widget), "changed",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  widget = lookup_widget(zapping_properties, "checkbutton3");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
			       config.avoid_noise);

  gtk_signal_connect(GTK_OBJECT(widget), "toggled",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* Let the plugins add their properties */
  while (p)
    {
      plugin_add_properties(GNOME_PROPERTY_BOX(zapping_properties),
			    (struct plugin_info * ) p->data);
      p = p->next;
    }

  gtk_widget_show(zapping_properties);
}


void
on_zapping_properties_apply            (GnomePropertyBox *gnomepropertybox,
                                        gint             arg1,
                                        gpointer         user_data)
{
  GtkWidget * widget; /* Generic widget */
  GtkWidget * pbox = GTK_WIDGET(gnomepropertybox); /* Very long name */
  gchar * text; /* Pointer to returned text */
  GList * p; /* For traversing the plugins */
  
  /* Apply just the given page */
  switch (arg1)
    {
    case 0:
      widget = lookup_widget(pbox, "entry1"); /* Prefix entry */
      config.png_prefix[31] = 0;
      g_snprintf(config.png_prefix, 31,
		 gtk_entry_get_text(GTK_ENTRY(widget)));
      widget = lookup_widget(pbox, "fileentry1"); /* Source dir entry
						    */
      text = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY(widget),
					     TRUE);
      config.png_src_dir[PATH_MAX-1] = 0;
      g_snprintf(config.png_src_dir, PATH_MAX-1, text);
      g_free(text); /* In the docs it says this should be freed */

      widget = lookup_widget(pbox, "checkbutton1"); /* Wheter to show
						       a progress bar
						       or not */
      config.png_show_progress = gtk_toggle_button_get_active
	(GTK_TOGGLE_BUTTON(widget));
      break;
    case 1:
      widget = lookup_widget(pbox, "checkbutton2"); /* Capture
						       interlaced */
      config.capture_interlaced = gtk_toggle_button_get_active
	(GTK_TOGGLE_BUTTON(widget));
      info.interlaced = config.capture_interlaced;

      widget = lookup_widget(pbox, "spinbutton1"); /* Number of
						      desired buffers */
      info.num_desired_buffers =
	gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

      widget = lookup_widget(pbox, "fileentry2"); /* Video device entry
						    */
      text = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY(widget),
					     TRUE);
      config.video_device[FILENAME_MAX-1] = 0;
      g_snprintf(config.video_device, FILENAME_MAX-1, text);
      g_free(text); /* In the docs it says this should be freed */
      break;
    case 2: /* Misc options */
      widget = lookup_widget(pbox, "spinbutton2"); /* zapping_setup_fb
						    verbosity */
      config.zapping_setup_fb_verbosity =
	gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

      widget = lookup_widget(pbox, "checkbutton3"); /* avoid noises */
      config.avoid_noise = gtk_toggle_button_get_active
	(GTK_TOGGLE_BUTTON(widget));
      break;
    default:
      p = g_list_first(plugin_list);
      while (p) /* Try with all the plugins until one of them accepts
		   the call */
	{
	  if (plugin_apply_properties(gnomepropertybox, arg1,
				      (struct plugin_info*) p->data))
	    break; /* returned TRUE: stop */
	  p = p->next;
	}
      /* This shouldn't ideally be reached, but a g_assert is too
	 strong */
      if ((p == NULL) && (arg1 != -1))
	printf(_("%s (%d): This shouldn't have been reached\n"), 
	       __FILE__, __LINE__);
      break;
    }
}


void
on_zapping_properties_help             (GnomePropertyBox *gnomepropertybox,
                                        gint             arg1,
                                        gpointer         user_data)
{
  /* FIXME: Find out why we have to start and stop capturing to avoid
     errors */
  /* FIXME: Add the help */

  /*  gboolean flag = info.current_mode == TVENG_CAPTURE_MMAPED_BUFFERS;
  static GnomeHelpMenuEntry help_ref = { "gnumeric",
					 "formatting.html" };

  if (flag)
    tveng_stop_capturing(&info);

  gnome_help_display(NULL, & help_ref);

  if (flag)
    tveng_start_capturing(&info);
  */
}

/* This function is called when some item in the property box changes */
void
on_property_item_changed              (GtkWidget * changed_widget,
				       GnomePropertyBox *propertybox)
{
  gnome_property_box_changed (propertybox);
}

void
on_go_fullscreen1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget * da; /* Drawing area */
  /* Return if we are in fullscreen mode now */
  if (info.current_mode == TVENG_CAPTURE_FULLSCREEN)
    return;

  /* Stop capturing if we are capturing */
  if (info.current_mode == TVENG_CAPTURE_MMAPED_BUFFERS)
    tveng_stop_capturing(&info);

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

  if (!tveng_start_fullscreen_previewing(&info,
					 config.zapping_setup_fb_verbosity))
    {
      ShowBox(_("Sorry, but cannot go fullscreen"),
	      GNOME_MESSAGE_BOX_ERROR);
      tveng_start_capturing(&info);
      return;
    }

  /* Grab the keyboard to the main zapping window */
  gdk_keyboard_grab(lookup_widget(GTK_WIDGET(menuitem), "zapping")->window,
		    TRUE,
		    GDK_CURRENT_TIME);
}


void
on_go_windowed1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (info.current_mode != TVENG_CAPTURE_FULLSCREEN)
    return;

  tveng_stop_fullscreen_previewing(&info);
  tveng_start_capturing(&info);

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
  on_channel_activate(NULL, (gpointer) new_channel);
  
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
  on_channel_activate(NULL, (gpointer) new_channel);
  
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
  gchar buffer[256];
  gchar * clist2_entries[4];
  
  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	plugin_stop((struct plugin_info*)
		    g_list_nth(plugin_list, i)->data);
      i++;
      ptr = ptr -> next;
    }
  /* And now update the clist */
  gtk_clist_freeze(GTK_CLIST(clist2));
  gtk_clist_clear(GTK_CLIST(clist2));

  ptr = g_list_first(plugin_list);

  /* Add the plugins to the CList again */
  while (ptr)
    {
      plug_info = (struct plugin_info*) ptr->data;
      clist2_entries[0] = plugin_get_canonical_name(plug_info);
      clist2_entries[1] = plugin_get_name(plug_info);
      g_snprintf(buffer, 255, "%d.%d.%d", plug_info -> major, plug_info ->
		 minor, plug_info -> micro);
      clist2_entries[2] = buffer;
      clist2_entries[3] = plugin_running(plug_info) ? _("Yes") : _("No");
      gtk_clist_append(GTK_CLIST(clist2), clist2_entries);
      ptr = ptr->next;
    }

  /* Show the changes */
  gtk_clist_thaw(GTK_CLIST(clist2));
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
  gchar buffer[256];
  gchar * clist2_entries[4];
  
  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	plugin_start((struct plugin_info*)
		    g_list_nth(plugin_list, i)->data);
      i++;
      ptr = ptr -> next;
    }
  /* And now update the clist */
  gtk_clist_freeze(GTK_CLIST(clist2));
  gtk_clist_clear(GTK_CLIST(clist2));

  ptr = g_list_first(plugin_list);

  /* Add the plugins to the CList again */
  while (ptr)
    {
      plug_info = (struct plugin_info*) ptr->data;
      clist2_entries[0] = plugin_get_canonical_name(plug_info);
      clist2_entries[1] = plugin_get_name(plug_info);
      g_snprintf(buffer, 255, "%d.%d.%d", plug_info -> major, plug_info ->
		 minor, plug_info -> micro);
      clist2_entries[2] = buffer;
      clist2_entries[3] = plugin_running(plug_info) ? _("Yes") : _("No");
      gtk_clist_append(GTK_CLIST(clist2), clist2_entries);
      ptr = ptr->next;
    }

  /* Show the changes */
  gtk_clist_thaw(GTK_CLIST(clist2));
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
