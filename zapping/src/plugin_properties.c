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

#include "interface.h"
#include "zmisc.h"
#include "plugins.h"
#include "globals.h"
#include "remote.h"
#include "plugin_properties.h"

static GtkWidget * PluginProperties = NULL; /* Pointer to the plugin
					       properties dialog */

enum {
  CANONICAL_NAME,
  INFO_STRUCT /* Not visible */
};

enum {
  SYMBOL,
  SYMBOL_HASH,
  SYMBOL_INFO /* Not visible */
};

static void
z_put_cursor_on_first_row (GtkTreeView *view)
{
  GtkTreePath *path = gtk_tree_path_new_from_string ("0");

  gtk_tree_view_set_cursor (view, path, NULL, FALSE);

  gtk_tree_path_free (path);
}

static void
on_plugin_treeview_cursor_changed	(GtkTreeView	*v,
					 gpointer	user_data _unused_)
{
  GtkWidget * plugin_properties = lookup_widget(GTK_WIDGET(v),
						"plugin_properties");
  GtkTreeView * symbol_treeview;
  GtkLabel * label67; /* Plugin name */
  GtkLabel * label68; /* Plugin short description */
  GtkLabel * label69; /* Plugin canonical name */
  GtkLabel * label946; /* File location */
  GtkLabel * label71; /* Author */
  GtkLabel * label72; /* Plugin version */
  GtkLabel * label73; /* Plugin priority */
  GtkLabel * label945; /* Plugin description */
  GtkLabel * label947; /* Symbol type and description */
  GtkLabel * label76; /* "Symbols exported by the plugin" label */
  GtkWidget * vbox13; /* The vbox where the exported symbols are */
  GtkWidget * plugin_start_w; /* Start it button */
  GtkWidget * plugin_stop_w; /* Stop it button */
  gchar * buffer;
  struct plugin_info * info;
  gint i;
  gint priority; /* The priority of the plugin */
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model (v);
  GtkTreePath *path;
  gboolean running;

  gtk_tree_view_get_cursor (v, &path, NULL);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);

  gtk_tree_model_get (model, &iter, INFO_STRUCT, &info, -1);

  g_assert (info != NULL);

  symbol_treeview = GTK_TREE_VIEW (lookup_widget (plugin_properties,
						  "symbol_treeview"));
  label945 = GTK_LABEL (lookup_widget (plugin_properties, "label945"));
  label67 = GTK_LABEL (lookup_widget (plugin_properties, "label67"));
  label68 = GTK_LABEL (lookup_widget (plugin_properties, "label68"));
  label69 = GTK_LABEL (lookup_widget (plugin_properties, "label69"));
  label946 = GTK_LABEL (lookup_widget (plugin_properties, "label946"));
  label71 = GTK_LABEL (lookup_widget (plugin_properties, "label71"));
  label72 = GTK_LABEL (lookup_widget (plugin_properties, "label72"));
  label73 = GTK_LABEL (lookup_widget (plugin_properties, "label73"));
  label947 = GTK_LABEL (lookup_widget (plugin_properties, "label947"));
  label76 = GTK_LABEL (lookup_widget (plugin_properties, "label76"));
  plugin_start_w = lookup_widget (plugin_properties, "plugin_start");
  plugin_stop_w = lookup_widget (plugin_properties, "plugin_stop");
  vbox13 = lookup_widget (plugin_properties, "vbox13");

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

  z_label_set_text_printf (label73, "%d (%s)", priority, buffer);

  gtk_label_set_text (label945, plugin_get_description (info));
  gtk_label_set_text (label946, info->file_name);

  running = plugin_running (info);
  gtk_widget_set_sensitive (plugin_start_w, !running);
  gtk_widget_set_sensitive (plugin_stop_w, running);

  g_object_set_data (G_OBJECT (plugin_start_w), "plugin_info", info);
  g_object_set_data (G_OBJECT (plugin_stop_w), "plugin_info", info);

  /* Add the items to the public symbols list if neccesary */
  model = gtk_tree_view_get_model (symbol_treeview);
  gtk_list_store_clear (GTK_LIST_STORE (model));
  if (info->num_exported_symbols == 0)
    {
      gtk_widget_set_sensitive(vbox13, FALSE);
      gtk_label_set_text (label947, "");
      gtk_label_set_text(label76,
			 _("This plugin has no public symbols"));
    }
  else
    {
      for (i = 0; i<info->num_exported_symbols;i++)
	{
	  buffer = g_strdup_printf("0x%X",
				   info->exported_symbols[i].hash);
	  gtk_list_store_append (GTK_LIST_STORE (model), &iter);	  
	  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			      SYMBOL, info->exported_symbols[i].symbol,
			      SYMBOL_HASH, buffer,
			      SYMBOL_INFO, &(info->exported_symbols[i]),
			      -1);
	  g_free(buffer);
	}
      z_put_cursor_on_first_row (symbol_treeview);
      gtk_widget_set_sensitive(vbox13, TRUE);
      gtk_label_set_text(label76,
			 _("List of symbols exported by the plugin"));
    }
}

static void
on_symbol_treeview_cursor_changed	(GtkTreeView	*v,
					 gpointer	user_data _unused_)
{
  struct plugin_exported_symbol * symbol;
  GtkWidget * label947; /* Symbol description and properties */
  gchar * buffer;
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model (v);
  GtkTreePath *path;

  gtk_tree_view_get_cursor (v, &path, NULL);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);

  gtk_tree_model_get (model, &iter, SYMBOL_INFO, &symbol, -1);

  g_assert (symbol != NULL);

  buffer = g_strconcat(symbol->description, "\n\n", symbol->type, NULL);

  label947 = lookup_widget (GTK_WIDGET (v), "label947");
  gtk_label_set_text (GTK_LABEL (label947), buffer);

  g_free(buffer);
}

static void
on_plugin_start_clicked                (GtkWidget	*button,
                                        gpointer         user_data _unused_)
{
  struct plugin_info * info =
    g_object_get_data(G_OBJECT(button), "plugin_info");
  GtkWidget * plugin_stop_w = lookup_widget(button, "plugin_stop");
  GtkWidget * plugin_start_w = button;
  gboolean running;

  if (!plugin_start(info))
    ShowBox(_("Sorry, the plugin couldn't be started"),
	    GTK_MESSAGE_INFO);

  running = plugin_running (info);
  gtk_widget_set_sensitive (plugin_start_w, !running);
  gtk_widget_set_sensitive (plugin_stop_w, running);
}

static void
on_plugin_stop_clicked                (GtkWidget	*button,
				       gpointer         user_data _unused_)
{
  struct plugin_info * info =
    g_object_get_data (G_OBJECT (button), "plugin_info");
  GtkWidget * plugin_stop_w = button;
  GtkWidget * plugin_start_w = lookup_widget(button, "plugin_start");
  gboolean running;

  plugin_stop(info);

  running = plugin_running (info);
  gtk_widget_set_sensitive (plugin_start_w, !running);
  gtk_widget_set_sensitive (plugin_stop_w, running);
}

static PyObject *
py_plugin_properties (PyObject *self _unused_, PyObject *args _unused_)
{
  GtkWidget * plugin_properties;
  GtkTreeView * plugin_treeview, *symbol_treeview;
  GtkWidget *menuitem =
    lookup_widget (GTK_WIDGET (zapping), "plugins1");
  GtkListStore *model;
  GtkTreeIter iter;
  GtkTreeSelection *sel;
  GList * p = g_list_first(plugin_list);
  struct plugin_info * info_plugin;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkWidget *w;

  if (PluginProperties)
    {
      gtk_window_present (GTK_WINDOW (PluginProperties));
      py_return_true;
    }

  if (!plugin_list)
    {
      ShowBox(_("Sorry, but no plugins have been loaded"),
	      GTK_MESSAGE_INFO);
      py_return_true;
    }

  plugin_properties = build_widget("plugin_properties", NULL);

  /* Set the menuitem sensitive again when we are destroyed */
  gtk_widget_set_sensitive(menuitem, FALSE);
  g_signal_connect_swapped (G_OBJECT (plugin_properties), "destroy",
			    G_CALLBACK (gtk_widget_set_sensitive),
			    menuitem);
  /* Nullify PluginProperties on destroy */
  g_signal_connect_swapped (G_OBJECT (plugin_properties), "destroy",
			    G_CALLBACK (g_nullify_pointer),
			    &PluginProperties);

  PluginProperties = plugin_properties;

  /* Lookup all widgets in the box */
  plugin_treeview = GTK_TREE_VIEW(lookup_widget(plugin_properties,
						"plugin_treeview"));
  model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);

  g_signal_connect (G_OBJECT (plugin_treeview), "cursor-changed",
		    G_CALLBACK (on_plugin_treeview_cursor_changed),
		    NULL);

  gtk_tree_view_set_model (plugin_treeview, GTK_TREE_MODEL (model));
  sel = gtk_tree_view_get_selection (plugin_treeview);
  gtk_tree_selection_set_mode (sel, GTK_SELECTION_BROWSE);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes
    (_("Plugin name"), renderer, "text", CANONICAL_NAME, NULL);
  gtk_tree_view_append_column (plugin_treeview, column);

  while (p)
    {
      info_plugin = (struct plugin_info*) p->data;
      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter,
			  CANONICAL_NAME, info_plugin->canonical_name,
			  INFO_STRUCT, info_plugin,
			  -1);

      p = p->next;
    }

  /* Setup the plugin symbols treeview too */
  symbol_treeview = GTK_TREE_VIEW (lookup_widget (plugin_properties,
						  "symbol_treeview"));
  model = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING,
			      G_TYPE_POINTER);

  gtk_tree_view_set_model (symbol_treeview, GTK_TREE_MODEL (model));
  sel = gtk_tree_view_get_selection (symbol_treeview);
  gtk_tree_selection_set_mode (sel, GTK_SELECTION_BROWSE);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes
    (_("Symbol"), renderer, "text", SYMBOL, NULL);
  gtk_tree_view_append_column (symbol_treeview, column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes
    (_("Hash"), renderer, "text", SYMBOL_HASH, NULL);
  gtk_tree_view_append_column (symbol_treeview, column);

  g_signal_connect (G_OBJECT (symbol_treeview), "cursor-changed",
		    G_CALLBACK (on_symbol_treeview_cursor_changed),
		    NULL);

  /* And lastly, connect the buttons */
  w = lookup_widget (plugin_properties, "plugin_properties_done");
  g_signal_connect_swapped (G_OBJECT (w), "clicked",
			    G_CALLBACK (gtk_widget_destroy),
			    plugin_properties);

  w = lookup_widget (plugin_properties, "plugin_start");
  g_signal_connect (G_OBJECT (w), "clicked",
		    G_CALLBACK (on_plugin_start_clicked),
		    NULL);

  w = lookup_widget (plugin_properties, "plugin_stop");
  g_signal_connect (G_OBJECT (w), "clicked",
		    G_CALLBACK (on_plugin_stop_clicked),
		    NULL);

  z_put_cursor_on_first_row (plugin_treeview);

  gtk_widget_show (plugin_properties);

  py_return_true;
}

void startup_plugin_properties (void)
{
  cmd_register ("plugin_properties", py_plugin_properties, METH_VARARGS,
		("Plugin properties"), "zapping.plugin_properties()");
}

void shutdown_plugin_properties (void)
{
}
