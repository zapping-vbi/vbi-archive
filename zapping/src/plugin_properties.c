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
/*
  This modules takes care of the plugins properties dialog
*/
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "callbacks.h"
#include "interface.h"
#include "zmisc.h"
#include "plugins.h"

GtkWidget * PluginProperties = NULL; /* Pointer to the plugin
					properties dialog */
extern GList * plugin_list; /* Loaded plugins */

void
on_plugins1_activate                   (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget * plugin_properties;
  gchar * clist_buffer[1];
  GtkCList * plugin_clist;

  GList * p = g_list_first(plugin_list);
  int i=0;
  struct plugin_info * info_plugin;

  if (PluginProperties)
    {
      gdk_window_raise(PluginProperties -> window);
      return;
    }

  if (!plugin_list)
    {
      ShowBox(_("Sorry, but no plugins have been loaded"),
	      GNOME_MESSAGE_BOX_INFO);
      return;
    }

  gtk_widget_set_sensitive(GTK_WIDGET(menuitem), FALSE);

  plugin_properties = create_plugin_properties();

  /* So the menuitem can be set sensitive again */
  gtk_object_set_user_data(GTK_OBJECT(plugin_properties),
			   menuitem);

  PluginProperties = plugin_properties;

  /* Lookup all widgets in the box */
  plugin_clist = GTK_CLIST(lookup_widget(plugin_properties,
					"plugin_list"));
  while (p)
    {
      info_plugin = (struct plugin_info*) p->data;
      clist_buffer[0] = info_plugin->canonical_name;
      gtk_clist_append(GTK_CLIST(plugin_clist), clist_buffer);
      gtk_clist_set_row_data(GTK_CLIST(plugin_clist), i++,
			     info_plugin);

      p = p->next;
    }

  /* Select the first row (there will always be at least one) */
  gtk_clist_select_row(GTK_CLIST(plugin_clist), 0, 0);

  gtk_widget_show(plugin_properties);
}


void
on_plugin_list_select_row                   (GtkCList        *clist,
					     gint             row,
					     gint             column,
					     GdkEvent        *event,
					     gpointer         user_data)
{
  GtkWidget * plugin_properties = lookup_widget(GTK_WIDGET(clist),
						"plugin_properties");
  GtkCList * symbol_list;
  GtkLabel * label67; /* Plugin name */
  GtkLabel * label68; /* Plugin short description */
  GtkLabel * label69; /* Plugin canonical name */
  GtkText  * text2; /* File location */
  GtkLabel * label71; /* Author */
  GtkLabel * label72; /* Plugin version */
  GtkLabel * label73; /* Plugin priority */
  GtkText * text1; /* Plugin description */
  GtkLabel * label74; /* Symbol prototype */
  GtkLabel * label75; /* Symbol description */
  GtkLabel * label76; /* "Symbols exported by the plugin" label */
  GtkWidget * vbox13; /* The vbox where the exported symbols are */
  GtkWidget * plugin_start_w;
  GtkWidget * plugin_stop_w;
  gchar * buffer;
  gchar * clist_buffer[2];
  struct plugin_info * info;
  gint i;
  gint priority; /* The priority of the plugin */

  info = gtk_clist_get_row_data(clist, row);
  if (!info)
    {
      /* This is not an error, it is just that the clist emits a
	 select-row when appending the first item in the row, and
	 the row data is not set yet */
      return;
    }

  symbol_list = GTK_CLIST(lookup_widget(plugin_properties,
					"symbol_list"));
  /* Plugin description */
  text1 = GTK_TEXT(lookup_widget(plugin_properties, "text1"));
  label67 = GTK_LABEL(lookup_widget(plugin_properties, "label67"));
  label68 = GTK_LABEL(lookup_widget(plugin_properties, "label68"));
  label69 = GTK_LABEL(lookup_widget(plugin_properties, "label69"));
  text2 = GTK_TEXT(lookup_widget(plugin_properties, "text2"));
  label71 = GTK_LABEL(lookup_widget(plugin_properties, "label71"));
  label72 = GTK_LABEL(lookup_widget(plugin_properties, "label72"));
  label73 = GTK_LABEL(lookup_widget(plugin_properties, "label73"));
  label74 = GTK_LABEL(lookup_widget(plugin_properties, "label74"));
  label75 = GTK_LABEL(lookup_widget(plugin_properties, "label75"));
  label76 = GTK_LABEL(lookup_widget(plugin_properties, "label76"));
  plugin_start_w = lookup_widget(plugin_properties, "plugin_start");
  plugin_stop_w = lookup_widget(plugin_properties, "plugin_stop");
  vbox13 = lookup_widget(plugin_properties, "vbox13");

  gtk_label_set_text(label67, plugin_get_name(info));
  gtk_label_set_text(label68, plugin_get_short_description(info));
  gtk_label_set_text(label69, plugin_get_canonical_name(info));
  gtk_label_set_text(label71, plugin_get_author(info));
  gtk_label_set_text(label72, plugin_get_version(info));
  priority = plugin_get_priority(info);
  if (priority < -9)
    buffer = _("Very low");
  else if (priority < -4)
    buffer = _("Low");
  else if (priority < 0)
    buffer = _("Below normal");
  else if (priority == 0)
    buffer = _("Normal");
  else if (priority > 9)
    buffer = _("Very high");
  else if (priority > 4)
    buffer = _("High");
  else
    buffer = _("Above normal");

  buffer = g_strdup_printf("%d (%s)", priority, buffer);
  gtk_label_set_text(label73, buffer);
  g_free(buffer);
  gtk_editable_delete_text(GTK_EDITABLE(text1), 0, -1);
  gtk_text_set_word_wrap(text1, TRUE);
  gtk_text_insert(text1, NULL, NULL, NULL,
		  plugin_get_description(info), -1);
  gtk_editable_delete_text(GTK_EDITABLE(text2), 0, -1);
  gtk_text_insert(text2, NULL, NULL, NULL,
		  info->file_name, -1);

  if (plugin_running(info))
    {
      gtk_widget_set_sensitive(plugin_start_w, FALSE);
      gtk_widget_set_sensitive(plugin_stop_w, TRUE);
      gtk_object_set_user_data(GTK_OBJECT(plugin_stop_w), info);
    }
  else
    {
      gtk_widget_set_sensitive(plugin_stop_w, FALSE);
      gtk_widget_set_sensitive(plugin_start_w, TRUE);
      gtk_object_set_user_data(GTK_OBJECT(plugin_start_w), info);
    }
  /* Add the items to the public symbols list if neccesary */
  if (info->num_exported_symbols == 0)
    {
      gtk_widget_set_sensitive(vbox13, FALSE);
      gtk_clist_clear(symbol_list);
      gtk_label_set_text(label74, "");
      gtk_label_set_text(label75, "");
      gtk_label_set_text(label76,
			 _("This plugin has no public symbols"));
    }
  else
    {
      gtk_clist_freeze(symbol_list);
      gtk_clist_clear(symbol_list);
      for (i = 0; i<info->num_exported_symbols;i++)
	{
	  clist_buffer[0] = info->exported_symbols[i].symbol;
	  buffer = g_strdup_printf("0x%X",
				   info->exported_symbols[i].hash);
	  clist_buffer[1] = buffer;
	  gtk_clist_append(symbol_list, clist_buffer);
	  gtk_clist_set_row_data(symbol_list, i,
				 &(info->exported_symbols[i]));
	  g_free(buffer);
	}

      gtk_clist_select_row(symbol_list, 0, 0);
      gtk_clist_thaw(symbol_list);
      gtk_widget_set_sensitive(vbox13, TRUE);
      gtk_label_set_text(label76,
			 _("List of symbols exported by the plugin"));
    }
}

void
on_symbol_list_select_row                   (GtkCList        *clist,
					     gint             row,
					     gint             column,
					     GdkEvent        *event,
					     gpointer         user_data)
{
  struct plugin_exported_symbol * symbol = 
    gtk_clist_get_row_data(clist, row);
  GtkWidget * plugin_properties =
    lookup_widget(GTK_WIDGET(clist), "plugin_properties");
  GtkLabel * label74; /* Symbol prototype */
  GtkLabel * label75; /* Symbol description */

  if (!symbol)
    return; /* It is too soon to display anything */

  label74 = GTK_LABEL(lookup_widget(plugin_properties, "label74"));
  label75 = GTK_LABEL(lookup_widget(plugin_properties, "label75"));

  gtk_label_set_text(label74, symbol->type);
  gtk_label_set_text(label75, symbol->description);
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
  PluginProperties = NULL;
  gtk_widget_destroy(plugin_properties);
}


void
on_plugin_start_clicked                (GtkButton       *button,
                                        gpointer         user_data)
{
  struct plugin_info * info =
    gtk_object_get_user_data(GTK_OBJECT(button));
  GtkWidget * plugin_stop_w = 
    lookup_widget(GTK_WIDGET(button), "plugin_stop");
  GtkWidget * plugin_start_w =
    lookup_widget(GTK_WIDGET(button), "plugin_start");

  if (!plugin_start(info))
    ShowBox(_("Sorry, but the plugin couldn't be started"),
	    GNOME_MESSAGE_BOX_INFO);

  if (plugin_running(info))
    {
      gtk_widget_set_sensitive(plugin_start_w, FALSE);
      gtk_widget_set_sensitive(plugin_stop_w, TRUE);
      gtk_object_set_user_data(GTK_OBJECT(plugin_stop_w), info);
    }  
  else
    {
      gtk_widget_set_sensitive(plugin_start_w, TRUE);
      gtk_widget_set_sensitive(plugin_stop_w, FALSE);
      gtk_object_set_user_data(GTK_OBJECT(plugin_start_w), info);
    }  
}

void
on_plugin_stop_clicked                (GtkButton       *button,
				       gpointer         user_data)
{
  struct plugin_info * info =
    gtk_object_get_user_data(GTK_OBJECT(button));
  GtkWidget * plugin_stop_w = 
    lookup_widget(GTK_WIDGET(button), "plugin_stop");
  GtkWidget * plugin_start_w =
    lookup_widget(GTK_WIDGET(button), "plugin_start");

  plugin_stop(info);
  
  if (plugin_running(info))
    {
      gtk_widget_set_sensitive(plugin_start_w, FALSE);
      gtk_widget_set_sensitive(plugin_stop_w, TRUE);
      gtk_object_set_user_data(GTK_OBJECT(plugin_stop_w), info);
    }  
  else
    {
      gtk_widget_set_sensitive(plugin_start_w, TRUE);
      gtk_widget_set_sensitive(plugin_stop_w, FALSE);
      gtk_object_set_user_data(GTK_OBJECT(plugin_start_w), info);
    }  
}

gboolean
on_plugin_properties_delete_event      (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  gpointer menuitem = gtk_object_get_user_data(GTK_OBJECT(widget));
  gtk_widget_set_sensitive(GTK_WIDGET(menuitem), TRUE);
  PluginProperties = NULL;

  return FALSE;
}
