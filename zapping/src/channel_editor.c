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
#include "zconf.h"
/* Manages config values for zconf (it saves me some typing) */
#define ZCONF_DOMAIN "/zapping/internal/callbacks/"
#include "zmisc.h"
#include "plugins.h"

GtkWidget * ChannelWindow = NULL; /* Here is stored the channel editor
				   widget (if any) */

extern tveng_channels * current_country; /* Currently selected contry */

extern tveng_device_info * main_info; /* About the device we are using */

extern int cur_tuned_channel; /* Currently tuned channel */

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

  gchar alias[256];
  gchar channel[256];
  gchar country[256];
  gchar freq[256];

  gchar *entry[] = {alias, channel, country, freq};

  if (ChannelWindow)
    {
      gdk_window_raise(ChannelWindow->window);
      return;
    }

  if (main_info->inputs[main_info->cur_input].tuners == 0)
    {
      ShowBox(_("Sorry, but the current input has no tuners"),
	      GNOME_MESSAGE_BOX_ERROR);
      return;
    }

  alias[255] = channel[255] = country[255] = freq[255] = 0;

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
  
  /* Change contry to the currently tuned one */
  if (menu_item)
    on_country_switch(menu_item, 
		      tveng_get_country_tune_by_id (currently_tuned_country));

  /* Setup the channel list */
  i = 0;

  while ((tuned_channel = tveng_retrieve_tuned_channel_by_index(i)))
    {
      i++;
      g_snprintf(entry[0], 255, tuned_channel->name);
      g_snprintf(entry[1], 255, tuned_channel->real_name);
      g_snprintf(entry[2], 255, _(tuned_channel->country));
      g_snprintf(entry[3], 255, "%u", tuned_channel->freq);
      gtk_clist_append(GTK_CLIST(channel_list), entry);
    }

  /* Save the disabled menuitem */
  gtk_object_set_user_data(GTK_OBJECT(channel_window), menuitem);

  gtk_widget_set_sensitive(GTK_WIDGET(menuitem), FALSE);

  gtk_widget_show(channel_window);

  ChannelWindow = channel_window; /* Set this, we are present */
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
  GtkWidget * menu_item; /* The menu item asocciated with this entry */


  GList * ptr; /* Pointer to the selected item(s) in clist1 */
  int index; /* The row we are reading now */
  gchar * dummy_ptr; /* We need this one for getting the freq */

  tveng_channels * country_id; /* The country some channel is in */

  tveng_tuned_channel tc;

  /* Clear tuned channel list */
  tveng_clear_tuned_channel();

  index = 0;

  ptr = GTK_CLIST(channel_list) -> row_list;

  /* Using a GUI element as a data storage structure is a
     BAD(tm) practice, but it works fine this way */
  while (ptr)
    {
      /* Add this selected channel to the channel list */
      gtk_clist_get_text(GTK_CLIST(channel_list), index, 0, &(tc.name));
      gtk_clist_get_text(GTK_CLIST(channel_list), index, 1,
			 &(tc.real_name));
      gtk_clist_get_text(GTK_CLIST(channel_list), index, 2,
			 &(dummy_ptr));
      g_assert(dummy_ptr != NULL);
      country_id = tveng_get_country_tune_by_i18ed_name(dummy_ptr);
      if (!country_id)
	tc.country = NULL;
      else
	tc.country = country_id -> name; /* This is not i18ed */

      gtk_clist_get_text(GTK_CLIST(channel_list), index, 3,
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
  menu_item =
    GTK_WIDGET(gtk_object_get_user_data(GTK_OBJECT(channel_window)));
  update_channels_menu(menu_item, main_info);

  gtk_widget_set_sensitive(menu_item, TRUE);

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

  gchar *entry[4];
  
  entry[0] = gtk_entry_get_text (GTK_ENTRY(channel_name));
  if (current_country)
    entry[2] = _(current_country -> name);
  else
    entry[2] = _("(Unknown country)");

  ptr = GTK_CLIST(clist1) -> row_list;

  /* Again, using a GUI element as a data storage struct is a
     HORRIBLE(tm) thing, but other things would be overcomplicated */
  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	{ 
	  /* Add this selected channel to the channel list */
	  gtk_clist_get_text(GTK_CLIST(clist1), index, 0,
			     &(entry[1]));
	  gtk_clist_get_text(GTK_CLIST(clist1), index, 1,
			     &(entry[3]));
	  gtk_clist_append(GTK_CLIST(channel_list), entry);
	}
      ptr = ptr -> next;
      index++;
    }

  gtk_entry_set_text(GTK_ENTRY(channel_name), "");
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
      /* If we reach this it means that we are trying to select a item
       in the channel list but it hasn't been filled yet (it is filled
       by a callback) */
      return;
    }

  tveng_tune_input (selected_channel->freq, main_info);
}

gboolean
on_channel_window_delete_event         (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  GtkWidget * related_menuitem =
    GTK_WIDGET(gtk_object_get_user_data(GTK_OBJECT(widget)));

  ChannelWindow = NULL; /* No more channel window */

  update_channels_menu(related_menuitem, main_info);

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


  /* We are done, acknowledge the update in the channel list */
  menu_item =
    GTK_WIDGET(gtk_object_get_user_data(GTK_OBJECT(channel_window)));

  update_channels_menu(menu_item, main_info);

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
  gchar * s_channel;
  gchar * s_country;
  gchar * s_freq;
  unsigned int freq;
  tveng_channels * country;
  tveng_channel * channel;
  int country_id;
  int channel_id;
  int mute = 0; /* If the device was previously muted */

  GtkWidget * country_options_menu = lookup_widget(GTK_WIDGET (clist),
						   "country_options_menu");
  GtkWidget * clist1 = lookup_widget(GTK_WIDGET(clist), "clist1");

  if (!gtk_clist_get_text(clist, row, 1, &s_channel))
    {
      g_warning(_("s_channel could not be retrieved, strange..."));
      return;
    }
  if (!gtk_clist_get_text(clist, row, 2, &s_country))
    {
      g_warning(_("s_country could not be retrieved, strange..."));
      return;
    }
  if (!gtk_clist_get_text(clist, row, 3, &s_freq))
    {
      g_warning(_("s_freq could not be retrieved, strange..."));
      return;
    }

  country = tveng_get_country_tune_by_i18ed_name (s_country);
  if (!sscanf(s_freq, "%u", &freq))
    {
      g_warning(_("The specified frequence cannot be parsed"));
      return;
    }

  /* If we could understand the country, select it */
  if (country)
    {
      country_id = tveng_get_id_of_country_tune (country);
      if (country_id < 0)
	{
	  g_warning(_("Returned country tune id is invalid"));
	  return;
	}

      channel = tveng_get_channel_by_name (s_channel, country);
      channel_id = tveng_get_id_of_channel (channel, country);
      if (!channel)
	{
	  g_warning(_("Channel %s cannot be found in current country: %s"), 
		    s_channel, _(country->name));
	  return;
	}
      if (channel_id < 0)
	{
	  g_warning (_("Returned channel id (%d) is not valid"),
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

  /* Tune to this channel's freq */
  if (zconf_get_boolean(NULL, "/zapping/options/main/avoid_noise"))
    {
      mute = tveng_get_mute(main_info);
      
      if (!mute)
	tveng_set_mute(1, main_info);
    }

  if (-1 == tveng_tune_input (freq, main_info))
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

gint do_search (GtkWidget * searching);

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
	  gtk_entry_set_text(GTK_ENTRY(channel_name), channel->name);
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

  return TRUE;
}

void
on_channel_search_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget * channel_window =
    lookup_widget(GTK_WIDGET(button), "channel_window");
  GtkWidget * channel_list =
    lookup_widget(channel_window, "channel_list");
  GtkWidget * searching = create_searching();
  GtkWidget * progress =
    lookup_widget(searching, "progressbar1");
  gint timeout;

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
