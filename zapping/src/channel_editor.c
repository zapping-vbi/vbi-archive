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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "tveng.h"
#include "callbacks.h"
#include "interface.h"
#include "v4linterface.h"
#include "zconf.h"
#include "keysyms.h"
/* Manages config values for zconf (it saves me some typing) */
#define ZCONF_DOMAIN "/zapping/internal/callbacks/"
#include "zmisc.h"
#include "plugins.h"
#include "zvbi.h"

GtkWidget * ChannelWindow = NULL; /* Here is stored the channel editor
				   widget (if any) */

extern tveng_channels * current_country; /* Currently selected contry */
extern tveng_tuned_channel * global_channel_list;
extern tveng_device_info * main_info; /* About the device we are using */

extern int cur_tuned_channel; /* Currently tuned channel */

static void
build_channel_list(GtkCList *clist, tveng_tuned_channel * list)
{
  gint i=0;
  tveng_tuned_channel * tuned_channel;
  gchar alias[256];
  gchar channel[256];
  gchar country[256];
  gchar freq[256];
  gchar accel[256];
  gchar * buffer;

  gchar *entry[] = {alias, channel, country, freq, accel};

  alias[255] = channel[255] = country[255] = freq[255] = accel[255] = 0;

  gtk_clist_clear(clist);

  gtk_clist_freeze(clist);

  /* Setup the channel list */
  while ((tuned_channel = tveng_retrieve_tuned_channel_by_index(i++, list)))
    {
      g_snprintf(entry[0], 255, tuned_channel->name);
      g_snprintf(entry[1], 255, tuned_channel->real_name);
      g_snprintf(entry[2], 255, _(tuned_channel->country));
      g_snprintf(entry[3], 255, "%u", tuned_channel->freq);
      if (tuned_channel->accel_key)
	{
	  buffer = gdk_keyval_name(tuned_channel->accel_key);
	  if (buffer)
	    g_snprintf(entry[4], 255, "%s%s%s%s",
		       (tuned_channel->accel_mask&GDK_CONTROL_MASK)?"Ctl+":"",
		       (tuned_channel->accel_mask&GDK_MOD1_MASK)?"Alt+":"",
		       (tuned_channel->accel_mask&GDK_SHIFT_MASK)?"Shift+":"",
		       buffer);
	  else
	    entry[4][0] = 0;
	}
      else
	entry[4][0] = 0;
      gtk_clist_append(clist, entry);
    }

  gtk_clist_thaw(clist);
}

/* Tunes the current input and updates hscale1 */
static int channel_editor_tune_input(__u32 freq, tveng_device_info * info)
{
  GtkAdjustment * adj;
  GtkWidget * hscale1 = lookup_widget(ChannelWindow, "hscale1");

  adj = GTK_ADJUSTMENT(gtk_adjustment_new(freq -1, freq - 1e4, freq + 1e4,
					 2e2, 1e3, 0));

  gtk_range_set_adjustment(GTK_RANGE(hscale1), adj);
  gtk_adjustment_set_value(adj, freq); /* ugly evil dirty hack */
  gtk_signal_connect(GTK_OBJECT(adj), "value-changed",
		     on_hscale1_value_changed, NULL);

  return (tveng_tune_input(freq, info));
}

/* Called when the current country selection has been changed */
void
on_country_switch                      (GtkWidget       *menu_item,
					tveng_channels  *country)
{
  GtkWidget * clist1 = lookup_widget(menu_item, "clist1");

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

static void rebuild_inputs_standards(gpointer ignored,
				     GtkWidget *widget)
{
  GtkWidget * input = lookup_widget(widget, "attached_input");
  GtkWidget * standard = lookup_widget(widget, "attached_standard");
  GtkWidget * NewMenu; /* New menu */
  GtkWidget * menu_item;
  int i;
  
  /* remove old menu */
  gtk_widget_destroy(gtk_option_menu_get_menu (GTK_OPTION_MENU (input)));
  gtk_widget_destroy(gtk_option_menu_get_menu (GTK_OPTION_MENU (standard)));

  NewMenu = gtk_menu_new ();

  menu_item = gtk_menu_item_new_with_label(_("Do not change standard"));
  gtk_widget_show (menu_item);
  gtk_menu_append(GTK_MENU (NewMenu), menu_item);

  for (i = 0; i < main_info->num_standards; i++)
  {
    menu_item =
      gtk_menu_item_new_with_label(main_info->standards[i].name);
    gtk_widget_show (menu_item);
    gtk_menu_append(GTK_MENU (NewMenu), menu_item);
  }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (standard), NewMenu);

  NewMenu = gtk_menu_new ();

  menu_item = gtk_menu_item_new_with_label(_("Do not change input"));
  gtk_widget_show (menu_item);
  gtk_menu_append(GTK_MENU (NewMenu), menu_item);

  for (i = 0; i < main_info->num_inputs; i++)
  {
    menu_item =
      gtk_menu_item_new_with_label(main_info->inputs[i].name);
    gtk_widget_show (menu_item);
    gtk_menu_append(GTK_MENU (NewMenu), menu_item);
  }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (input), NewMenu);
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

  GtkWidget * combo1;

  int i = 0;
  int currently_tuned_country = 0;

  tveng_channels * tune;
  tveng_tuned_channel * tuned_channel;
  tveng_tuned_channel * list = NULL;

  GList * keylist = NULL;

  if (ChannelWindow)
    {
      gdk_window_raise(ChannelWindow->window);
      return;
    }

  if ((main_info->num_inputs == 0) ||
      (main_info->inputs[main_info->cur_input].tuners == 0))
    {
      ShowBox(_("Sorry, but the current input has no tuners"),
	      GNOME_MESSAGE_BOX_ERROR);
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

  /* select the first item if there's no current country */
  if (current_country == NULL)
    currently_tuned_country = 0;

  gtk_widget_show(new_menu);

  gtk_option_menu_set_menu( GTK_OPTION_MENU(country_options_menu),
			    new_menu);

  gtk_option_menu_set_history ( GTK_OPTION_MENU(country_options_menu),
				currently_tuned_country);

  i = 0;

  while ((tuned_channel =
	  tveng_retrieve_tuned_channel_by_index(i++, global_channel_list)))
    list = tveng_insert_tuned_channel(tuned_channel, list);

  build_channel_list(GTK_CLIST(channel_list), list);
  
  /* Change contry to the currently tuned one */
  if (menu_item)
    on_country_switch(menu_item, 
		      tveng_get_country_tune_by_id (currently_tuned_country));

  /* Save the disabled menuitem */
  gtk_object_set_user_data(GTK_OBJECT(channel_window), menuitem);
  gtk_object_set_data(GTK_OBJECT(channel_window), "list", list);

  gtk_widget_set_sensitive(GTK_WIDGET(menuitem), FALSE);

  combo1 = lookup_widget(channel_window, "combo1");

  for (i=0; i<num_keysyms; i++)
    keylist = g_list_append(keylist, keysyms[i]);

  gtk_combo_set_popdown_strings(GTK_COMBO(combo1), keylist);

  g_list_free(keylist);

  rebuild_inputs_standards(NULL, channel_window);
  gtk_signal_connect(GTK_OBJECT(z_input_model), "changed",
		     GTK_SIGNAL_FUNC(rebuild_inputs_standards),
		     channel_window);

  gtk_widget_show(channel_window);

  ChannelWindow = channel_window; /* Set this, we are present */
}

/**
 * This is called when we are done processing the channels, to update
 *  the GUI
 */
void
on_channels_done_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget * channel_window = lookup_widget(GTK_WIDGET (button),
					     "channel_window"); /* The
					     channel editor window */
  GtkWidget * menu_item; /* The menu item asocciated with this entry */
  tveng_tuned_channel * list =
    gtk_object_get_data(GTK_OBJECT(channel_window), "list");

  int index; /* The row we are reading now */

  tveng_tuned_channel * tc;

  /* Clear tuned channel list */
  global_channel_list =
    tveng_clear_tuned_channel(global_channel_list);

  index = 0;

  while ((tc = tveng_retrieve_tuned_channel_by_index(index++, list)))
    global_channel_list =
      tveng_insert_tuned_channel(tc, global_channel_list);

  /* We are done, acknowledge the update in the model  */
  menu_item =
    GTK_WIDGET(gtk_object_get_user_data(GTK_OBJECT(channel_window)));

  gtk_signal_disconnect_by_func(GTK_OBJECT(z_input_model),
				GTK_SIGNAL_FUNC(rebuild_inputs_standards),
				ChannelWindow);
  zmodel_changed(z_input_model);

  gtk_widget_set_sensitive(menu_item, TRUE);

  tveng_clear_tuned_channel(list);

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
  GtkWidget * channel_window = lookup_widget(GTK_WIDGET(button),
					     "channel_window");
  GtkWidget * channel_name = lookup_widget(GTK_WIDGET(button),
					   "channel_name");
  GtkWidget * channel_accel = lookup_widget(GTK_WIDGET(button),
					    "channel_accel");
  GtkWidget * widget;

  tveng_tuned_channel * list =
    gtk_object_get_data(GTK_OBJECT(channel_window), "list");

  GList * ptr; /* Pointer to the selected item(s) in clist1 */
  int index = 0; /* The row we are reading now */
  tveng_tuned_channel tc;
  gchar * buffer;
  gint selected;
  
  memset(&tc, 0, sizeof(tveng_tuned_channel));
  tc.name = gtk_entry_get_text (GTK_ENTRY(channel_name));
  if (current_country)
    tc.country = current_country -> name;
  else
    tc.country = NULL;
  tveng_get_tune(&tc.freq, main_info);
  tc.accel_key = 0;
  buffer = gtk_entry_get_text(GTK_ENTRY(channel_accel));
  if (buffer)
      tc.accel_key = gdk_keyval_from_name(buffer);
  if (tc.accel_key == GDK_VoidSymbol)
    tc.accel_key = 0;

  tc.accel_mask = 0;
  widget = lookup_widget(clist1, "channel_accel_ctrl");
  tc.accel_mask |=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ?
    GDK_CONTROL_MASK : 0;
  widget = lookup_widget(clist1, "channel_accel_shift");
  tc.accel_mask |=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ?
    GDK_SHIFT_MASK : 0;
  widget = lookup_widget(clist1, "channel_accel_alt");
  tc.accel_mask |=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ?
    GDK_MOD1_MASK : 0;

  selected =
    z_option_menu_get_active(lookup_widget(clist1, "attached_input"));
  if (selected)
    tc.input = main_info->inputs[selected-1].hash;
  else
    tc.input = 0;

  selected =
    z_option_menu_get_active(lookup_widget(clist1, "attached_standard"));
  if (selected)
    tc.standard = main_info->standards[selected-1].hash;
  else
    tc.standard = 0;

  ptr = GTK_CLIST(clist1) -> row_list;

  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	{ 
	  /* Add this selected channel to the channel list */
	  gtk_clist_get_text(GTK_CLIST(clist1), index, 0,
			     &(tc.real_name));
	  break; /* found */
	}
      ptr = ptr -> next;
      index++;
    }

  list = tveng_insert_tuned_channel(&tc, list);

  gtk_object_set_data(GTK_OBJECT(channel_window), "list", list);

  build_channel_list(GTK_CLIST(channel_list), list);

  gtk_entry_set_text(GTK_ENTRY(channel_name), "");
}

void
on_modify_channel_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget * clist1 = lookup_widget(GTK_WIDGET(button), "clist1");
  GtkWidget * channel_list = lookup_widget(GTK_WIDGET(button),
					   "channel_list");
  GtkWidget * channel_window = lookup_widget(GTK_WIDGET(button),
					     "channel_window");
  tveng_tuned_channel * list =
    gtk_object_get_data(GTK_OBJECT(channel_window), "list");
  GtkWidget * channel_name = lookup_widget(GTK_WIDGET(button),
					   "channel_name");
  GtkWidget * channel_accel = lookup_widget(GTK_WIDGET(button),
					    "channel_accel");
  GtkWidget * widget;

  GList * ptr; /* Pointer to the selected item(s) in clist1 */
  int index=0; /* The row we are reading now */
  tveng_tuned_channel tc, *p;
  gchar * buffer;
  gint selected;

  tc.name = gtk_entry_get_text (GTK_ENTRY(channel_name));
  if (current_country)
    tc.country = current_country -> name;
  else
    tc.country = NULL;

  tc.accel_key = 0;
  buffer = gtk_entry_get_text(GTK_ENTRY(channel_accel));
  if (buffer)
      tc.accel_key = gdk_keyval_from_name(buffer);
  if (tc.accel_key == GDK_VoidSymbol)
    tc.accel_key = 0;
  tveng_get_tune(&tc.freq, main_info);

  tc.accel_mask = 0;
  widget = lookup_widget(clist1, "channel_accel_ctrl");
  tc.accel_mask |=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ?
    GDK_CONTROL_MASK : 0;
  widget = lookup_widget(clist1, "channel_accel_shift");
  tc.accel_mask |=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ?
    GDK_SHIFT_MASK : 0;
  widget = lookup_widget(clist1, "channel_accel_alt");
  tc.accel_mask |=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ?
    GDK_MOD1_MASK : 0;

  selected =
    z_option_menu_get_active(lookup_widget(clist1, "attached_input"));
  if (selected)
    tc.input = main_info->inputs[selected-1].hash;
  else
    tc.input = 0;

  selected =
    z_option_menu_get_active(lookup_widget(clist1, "attached_standard"));
  if (selected)
    tc.standard = main_info->standards[selected-1].hash;
  else
    tc.standard = 0;

  ptr = GTK_CLIST(clist1) -> row_list;

  /* Again, using a GUI element as a data storage struct is a
     HORRIBLE(tm) thing, but other things would be overcomplicated */
  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	{ 
	  gtk_clist_get_text(GTK_CLIST(clist1), index, 0,
			     &(tc.real_name));
	  break;
	}
      index ++;
      ptr = ptr -> next;
    }

  if (!ptr)
    return;

  index = 0;

  ptr = GTK_CLIST(channel_list) -> row_list;

  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	if ((p = tveng_retrieve_tuned_channel_by_index(index,
						       list)))
	  tveng_copy_tuned_channel(p, &tc);

      ptr = ptr -> next;
      index++;
    }

  build_channel_list(GTK_CLIST(channel_list), list);
}

void
on_remove_channel_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget * channel_list = lookup_widget(GTK_WIDGET(button),
					   "channel_list");
  GtkWidget * channel_window = lookup_widget(GTK_WIDGET(button),
					     "channel_window");
  tveng_tuned_channel * list =
    gtk_object_get_data(GTK_OBJECT(channel_window), "list");

  GList * ptr; /* Pointer to the selected item(s) in clist1 */
  int index; /* The row we are reading now */
  int deleted = 0;

  index = 0;

  ptr = GTK_CLIST(channel_list) -> row_list;

  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	{
	  /* Add this selected channel to the channel list */
	  list = tveng_remove_tuned_channel(NULL, index-deleted, list);
	  deleted++;
	}

      ptr = ptr -> next;
      index++;
    }

  gtk_object_set_data(GTK_OBJECT(channel_window), "list", list);

  build_channel_list(GTK_CLIST(channel_list), list);
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
  tveng_channel * selected_channel =
    tveng_get_channel_by_id (row, country);

  if ((!selected_channel) || (!country))
    {
      /* If we reach this it means that we are trying to select a item
       in the channel list but it hasn't been filled yet (it is filled
       by a callback) */
      return;
    }

  channel_editor_tune_input (selected_channel->freq, main_info);
}

gboolean
on_channel_window_delete_event         (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  GtkWidget * related_menuitem =
    GTK_WIDGET(gtk_object_get_user_data(GTK_OBJECT(widget)));
  tveng_tuned_channel * list =
    gtk_object_get_data(GTK_OBJECT(widget), "list");

  ChannelWindow = NULL; /* No more channel window */

  tveng_clear_tuned_channel(list);

  gtk_signal_disconnect_by_func(GTK_OBJECT(z_input_model),
				GTK_SIGNAL_FUNC(rebuild_inputs_standards),
				ChannelWindow);

  zmodel_changed(z_input_model);

  /* Set the menuentry sensitive again */
  gtk_widget_set_sensitive(related_menuitem, TRUE);

  return FALSE;
}

void
on_cancel_channels_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget * channel_window = lookup_widget(GTK_WIDGET (button),
					     "channel_window"); /* The
					     channel editor window */
  GtkWidget * menu_item; /* The menu item asocciated with this entry */

  tveng_tuned_channel * list =
    gtk_object_get_data(GTK_OBJECT(channel_window), "list");

  /* We are done, acknowledge the update in the channel list */
  menu_item =
    GTK_WIDGET(gtk_object_get_user_data(GTK_OBJECT(channel_window)));

  tveng_clear_tuned_channel(list);

  gtk_signal_disconnect_by_func(GTK_OBJECT(z_input_model),
				GTK_SIGNAL_FUNC(rebuild_inputs_standards),
				ChannelWindow);

  zmodel_changed(z_input_model);

  gtk_widget_set_sensitive(menu_item, TRUE);

  gtk_widget_destroy(channel_window);

  ChannelWindow = NULL;
}

void
on_channel_name_activate               (GtkEditable     *editable,
                                        gpointer         user_data)
{
  GtkWidget * widget = lookup_widget(GTK_WIDGET(editable),
				     "add_channel");

  /* Add a channel to the list, just like hitting the apply button */
  on_add_channel_clicked(GTK_BUTTON(widget), user_data);
}

void
on_help_channels_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
  GnomeHelpMenuEntry help_ref = { NULL,
				  "channel_editor.html" };
  enum tveng_capture_mode cur_mode;

  cur_mode = tveng_stop_everything(main_info);

  help_ref.name = gnome_app_id;
  gnome_help_display (NULL, &help_ref);

  if (tveng_restart_everything(cur_mode, main_info) == -1)
    ShowBox(main_info->error, GNOME_MESSAGE_BOX_ERROR);
}

void
on_channel_list_select_row             (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  tveng_channels * country;
  tveng_channel * channel;
  int country_id;
  int channel_id;
  int mute = 0; /* Whether the device was previously muted */
  GtkWidget * country_options_menu = lookup_widget(GTK_WIDGET (clist),
						   "country_options_menu");
  GtkWidget * clist1 = lookup_widget(GTK_WIDGET(clist), "clist1");
  GtkWidget * channel_name = lookup_widget(GTK_WIDGET(clist),
					   "channel_name");
  GtkWidget * channel_window =
    lookup_widget(GTK_WIDGET(clist), "channel_window");
  tveng_tuned_channel * list =
    gtk_object_get_data(GTK_OBJECT(channel_window), "list");
  GtkWidget * channel_accel = lookup_widget(GTK_WIDGET(clist),
					    "channel_accel");
  GtkWidget * widget;
  struct tveng_enumstd *std;
  struct tveng_enum_input *input;

  list = tveng_retrieve_tuned_channel_by_index(row, list);

  g_assert(list != NULL);

  country = tveng_get_country_tune_by_name (list->country);

  /* If we could understand the country, select it */
  if (country)
    {
      country_id = tveng_get_id_of_country_tune (country);
      if (country_id < 0)
	{
	  g_warning("Returned country tune id is invalid");
	  return;
	}

      channel = tveng_get_channel_by_name (list->real_name, country);
      if (!channel)
	{
	  g_warning("Channel %s cannot be found in current country: %s", 
		    list->real_name, country->name);
	  return;
	}

      channel_id = tveng_get_id_of_channel (channel, country);
      if (channel_id < 0)
	{
	  g_warning ("Returned channel id (%d) is not valid",
		     channel_id);
	  return;
	}

      gtk_option_menu_set_history ( GTK_OPTION_MENU(country_options_menu),
				    country_id);
      on_country_switch (clist1, country);

      gtk_clist_select_row(GTK_CLIST (clist1), channel_id, 0);
      /* make the row visible */
      gtk_clist_moveto(GTK_CLIST(clist1), channel_id, 0,
		       0.5, 0);
    }

  gtk_entry_set_text(GTK_ENTRY(channel_name), list->name);
  if (list->accel_key)
    {
      if (gdk_keyval_name(list->accel_key))
	gtk_entry_set_text(GTK_ENTRY(channel_accel),
			   gdk_keyval_name(list->accel_key));
      else
	gtk_entry_set_text(GTK_ENTRY(channel_accel), "");
      widget = lookup_widget(channel_accel, "channel_accel_ctrl");
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
				   list->accel_mask & GDK_CONTROL_MASK);
      widget = lookup_widget(channel_accel, "channel_accel_alt");
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
				   list->accel_mask & GDK_MOD1_MASK);
      widget = lookup_widget(channel_accel, "channel_accel_shift");
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
				   list->accel_mask & GDK_SHIFT_MASK);
    }
  else
    {
      gtk_entry_set_text(GTK_ENTRY(channel_accel), "");
      widget = lookup_widget(channel_accel, "channel_accel_ctrl");
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), FALSE);
      widget = lookup_widget(channel_accel, "channel_accel_alt");
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), FALSE);
      widget = lookup_widget(channel_accel, "channel_accel_shift");
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), FALSE);
    }

  widget = lookup_widget(channel_accel, "attached_standard");
  if (list->standard)
    {
      std = tveng_find_standard_by_hash(list->standard, main_info);
      if (std)
	z_option_menu_set_active(widget, std->index+1);
      else
	z_option_menu_set_active(widget, 0);
    }
  else
    z_option_menu_set_active(widget, 0);

  widget = lookup_widget(channel_accel, "attached_input");
  if (list->input)
    {
      input = tveng_find_input_by_hash(list->input, main_info);
      if (input)
	z_option_menu_set_active(widget, input->index+1);
      else
	z_option_menu_set_active(widget, 0);
    }
  else
    z_option_menu_set_active(widget, 0);

  /* Tune to this channel's freq */
  if (zconf_get_boolean(NULL, "/zapping/options/main/avoid_noise"))
    {
      mute = tveng_get_mute(main_info);
      
      if (!mute)
	tveng_set_mute(1, main_info);
    }

  if (-1 == channel_editor_tune_input (list->freq, main_info))
    ShowBox(main_info -> error, GNOME_MESSAGE_BOX_ERROR);

  if (zconf_get_boolean(NULL, "/zapping/options/main/avoid_noise"))
    {
      /* Sleep a little so the noise dissappears */
      usleep(100000);
      
      if (!mute)
	tveng_set_mute(0, main_info);
    }
}

/*
  Called when a key is pressed in the channel list. Should call remove
  if the pressed key is Del
*/
gboolean
on_channel_list_key_press_event        (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data)
{
  GtkWidget * remove_channel = lookup_widget(widget, "remove_channel");
  if ((event->keyval == GDK_Delete) || (event->keyval == GDK_KP_Delete))
    {
      if (remove_channel)
	on_remove_channel_clicked(GTK_BUTTON(remove_channel), NULL);
      return TRUE; /* Processed */
    }

  return FALSE;
}

void
on_hscale1_value_changed               (GtkAdjustment   *adj,
                                        gpointer         user_data)
{
  tveng_tune_input(adj->value, main_info);
}

static
gint do_search (GtkWidget * searching)
{
  GtkWidget * progress = lookup_widget(searching, "progressbar1");
  GtkWidget * label80 = lookup_widget(searching, "label80");
  gint scanning_channel =
    GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(searching),
					"scanning_channel"));
  GtkWidget * channel_list =
    gtk_object_get_user_data(GTK_OBJECT(searching));
  GtkWidget * clist1 =
    lookup_widget(channel_list, "clist1");
  gint strength;
  gchar * tuned_name=NULL;
  tveng_channel * channel;

  if (scanning_channel >= 0)
    {
      channel = tveng_get_channel_by_id(scanning_channel,
					current_country);
      g_assert(channel != NULL);

      if ((-1 != tveng_get_signal_strength(&strength, NULL, main_info)) &&
	  (strength > 0))
	{
	  GtkWidget * channel_name =
	    lookup_widget(channel_list, "channel_name");

	  //	  if (zconf_get_boolean(NULL, "/zapping/options/vbi/use_vbi"))
	  //	    tuned_name = zvbi_get_name();
	  // FIXME: Obsolete
	  if ((!zconf_get_boolean(NULL,
				  "/zapping/options/vbi/use_vbi")) ||
	      (!tuned_name))
	    tuned_name = g_strdup(channel->name);
	  gtk_entry_set_text(GTK_ENTRY(channel_name), tuned_name);
	  g_free(tuned_name);

	  on_channel_name_activate(GTK_EDITABLE(channel_name), NULL);
	}
    }

  scanning_channel++;

  /* Check if we have reached the end */
  if (current_country->chan_count <= scanning_channel)
    {
      gtk_widget_destroy(searching);
      return FALSE;
    }

  gtk_progress_set_percentage(GTK_PROGRESS(progress),
     ((gfloat)scanning_channel)/current_country->chan_count);

  channel = tveng_get_channel_by_id(scanning_channel,
				    current_country);
  g_assert(channel != NULL);
  gtk_label_set_text(GTK_LABEL(label80), channel->name);

  gtk_clist_select_row(GTK_CLIST (clist1), scanning_channel, 0);

  /* make the row visible */
  gtk_clist_moveto(GTK_CLIST(clist1), scanning_channel, 0,
		   0.5, 0);

  /* Tune to this channel's freq */
  if (-1 == tveng_tune_input (channel->freq, main_info))
    g_warning("While tuning: %s", main_info -> error);

  gtk_object_set_data(GTK_OBJECT(searching), "scanning_channel",
		      GINT_TO_POINTER(scanning_channel));

  /* The timeout has to be big enough to let the tuner estabilize */
  gtk_object_set_data(GTK_OBJECT(searching), "timeout",
		      GINT_TO_POINTER((gtk_timeout_add(150,
			    (GtkFunction)do_search, searching))));

  return FALSE;
  /*
    rationale: returning TRUE might look like it does the same thing as
    above, but it doesn't. We want 150ms from now, not from when this
    callback was called. This is because some vbi functions here might
    take some time to complete.
  */
}

void
on_channel_search_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget * channel_window =
    lookup_widget(GTK_WIDGET(button), "channel_window");
  GtkWidget * channel_list =
    lookup_widget(channel_window, "channel_list");
  GtkWidget * searching;
  GtkWidget * progress;
  gint timeout;

  /* Channel auto-searching won't work with XVideo */
  if (main_info->current_controller == TVENG_CONTROLLER_XV)
    {
      ShowBox(_("Channel autosearching won't work with XVideo.\n"
		"Please switch to another controller by starting\n"
		"Capture mode (\"View/Go Capturing\" menu entry)."),
	      GNOME_MESSAGE_BOX_INFO);
      return;
    }

  searching = create_searching();
  progress = lookup_widget(searching, "progressbar1");

  gtk_progress_set_percentage(GTK_PROGRESS(progress), 0.0);

  /* The timeout has to be big enough to let the tuner estabilize */
  timeout = gtk_timeout_add(150, (GtkFunction)do_search, searching);

  gtk_object_set_user_data(GTK_OBJECT(searching), channel_list);
  gtk_object_set_data(GTK_OBJECT(searching), "timeout",
		      GINT_TO_POINTER(timeout));
  gtk_object_set_data(GTK_OBJECT(searching), "scanning_channel",
		      GINT_TO_POINTER(-1));

  gtk_widget_show(searching);
}

void
on_cancel_search_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget * searching =
    lookup_widget(GTK_WIDGET(button), "searching");
  gint timeout =
    GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(searching), "timeout"));

  gtk_timeout_remove(timeout);

  gtk_widget_destroy(searching);
}

gboolean
on_searching_delete_event              (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  gint timeout =
    GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(widget), "timeout"));

  gtk_timeout_remove(timeout);

  return FALSE;
}
