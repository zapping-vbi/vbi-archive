/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2002 Iñaki García Etxebarria
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
/**
 * Channel editor
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#define ZCONF_DOMAIN "/zapping/internal/properties/"
#include "zconf.h"
#include "interface.h"
#include "zmisc.h"
#include "remote.h"
#include "frequencies.h"
#include "globals.h"
#warning #include "channel_editor.h"
//#include "channel_editor.h"

/* Shorten a bit the usual stuff */
#define LOOKUP(X) GtkWidget *X = lookup_widget(channel_editor, #X)
#define CONNECT(X, Y) g_signal_connect (G_OBJECT (X), #Y, \
				        G_CALLBACK (on_ ## X ## _ ## Y), \
					channel_editor)

static void
on_country_options_menu_changed		(GtkOptionMenu	*menu,
					 GtkWidget	*channel_editor)
{
  tveng_rf_channel *tc;
  tveng_rf_table *country;
  gint i;
  LOOKUP (country_frequencies);
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW
						 (country_frequencies));

  country = tveng_get_country_tune_by_id
    (gtk_option_menu_get_history (menu));

  g_message ("new country: %s", country->name);
  for (i=0; (tc = tveng_get_channel_by_id (i, country)); i++)
    {
      
    }
}

static void
on_channel_search_clicked	(GtkButton	*channel_search,
				 GtkWidget	*channel_editor)
{
}

static void
on_add_all_channels_clicked	(GtkButton	*add_all_channels,
				 GtkWidget	*channel_editor)
{
}

static void
on_channel_name_changed		(GtkEditable	*channel_name,
				 GtkWidget	*channel_editor)
{
  gchar *chars = gtk_editable_get_chars (channel_name, 0, -1);
  g_message ("edited: %s", chars);
  g_free (chars);
}

static void
on_attached_input_changed	(GtkOptionMenu	*attached_input,
				 GtkWidget	*channel_editor)
{
}

static void
on_attached_standard_changed	(GtkOptionMenu	*attached_standard,
				 GtkWidget	*channel_editor)
{
}

static void
on_add_channel_clicked		(GtkButton	*add_channel,
				 GtkWidget	*channel_editor)
{
}

static void
on_ok_clicked			(GtkButton	*ok,
				 GtkWidget	*channel_editor)
{
  gtk_widget_destroy (channel_editor);
}

static void
on_cancel_clicked		(GtkButton	*cancel,
				 GtkWidget	*channel_editor)
{
  gtk_widget_destroy (channel_editor);
}

static void
on_help_clicked			(GtkButton	*help,
				 GtkWidget	*channel_editor)
{
}

static void
on_channel_editor_destroy	(GtkObject	*ce,
				 GtkWidget	*channel_editor)
{
  g_message ("destroy");
}

static GtkWidget *
build_channel_editor (void)
{
  GtkWidget *channel_editor = build_widget ("channel_editor", NULL);
  LOOKUP (country_options_menu);
  LOOKUP (channel_search);
  LOOKUP (add_all_channels);
  LOOKUP (channel_treeview);
  LOOKUP (channel_name);
  LOOKUP (attached_input);
  LOOKUP (fine_tuning);
  LOOKUP (attached_standard);
  LOOKUP (channel_accel);
  LOOKUP (add_channel);
  LOOKUP (ok);
  LOOKUP (cancel);
  LOOKUP (help);

  /* fill up the country list */
  {
    GtkMenuShell *menu = GTK_MENU_SHELL
      (gtk_option_menu_get_menu (GTK_OPTION_MENU (country_options_menu)));
    gint i;
    tveng_rf_table *rf;
    
    for (i=0; (rf = tveng_get_country_tune_by_id (i)); i++)
      {
	GtkWidget *item =
	  gtk_menu_item_new_with_label (_(rf->name));
	gtk_menu_shell_append (menu, item);
	gtk_widget_show (item);
      }
    
    gtk_option_menu_set_history (GTK_OPTION_MENU (country_options_menu),
				 tveng_get_id_of_country_tune
				 (current_country));
  }

  CONNECT (country_options_menu, changed);
  CONNECT (channel_search, clicked);
  CONNECT (add_all_channels, clicked);
  CONNECT (channel_name, changed);
  CONNECT (attached_input, changed);
  CONNECT (attached_standard, changed);
  CONNECT (add_channel, clicked);
  CONNECT (ok, clicked);
  CONNECT (cancel, clicked);
  CONNECT (help, clicked);
  CONNECT (channel_editor, destroy);

  return channel_editor;
}

static PyObject* py_channel_editor (PyObject *self, PyObject *args)
{
  gtk_widget_show (build_channel_editor ());

  py_return_true;
}

void startup_channel_editor (void)
{
  cmd_register ("channel_editor", py_channel_editor, METH_VARARGS,
		_("Opens the channel editor"), "zapping.channel_editor()");
}

void shutdown_channel_editor (void)
{
}
