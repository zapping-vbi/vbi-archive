/* RTE (Real time encoder) front end for Zapping
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
#include "plugin_common.h"
#include <glade/glade.h>

/*
  This plugin was built from the template one. It does some thing
  that the template one doesn't, such as adding itself to the gui and
  adding help for the properties.
*/

/* This is the description of the plugin, change as appropiate */
static const gchar str_canonical_name[] = "mpeg";
static const gchar str_descriptive_name[] =
N_("MPEG encoder");
static const gchar str_description[] =
N_("This plugin encodes the image and audio stream into a MPEG file");
static const gchar str_short_description[] = 
N_("Encode the stream as MPEG.");
static const gchar str_author[] = "Iñaki García Etxebarria";
/* The format of the version string must be
   %d[[.%d[.%d]][other_things]], where the things between [] aren't
   needed, and if not present, 0 will be assumed */
static const gchar str_version[] = "0.1";

/* Set to TRUE when plugin_close is called */
static gboolean close_everything = FALSE;

static tveng_device_info * zapping_info = NULL; /* Info about the
						   video device */

gint plugin_get_protocol ( void )
{
  /* You don't need to modify this function */
  return PLUGIN_PROTOCOL;
}

/* Return FALSE if we aren't able to access a symbol, you should only
   need to edit the pointer table, not the code */
gboolean plugin_get_symbol(gchar * name, gint hash, gpointer * ptr)
{
  /* Usually this table is the only thing you will need to change */
  struct plugin_exported_symbol table_of_symbols[] =
  {
    SYMBOL(plugin_init, 0x1234),
    SYMBOL(plugin_get_info, 0x1234),
    SYMBOL(plugin_close, 0x1234),
    SYMBOL(plugin_start, 0x1234),
    SYMBOL(plugin_load_config, 0x1234),
    SYMBOL(plugin_save_config, 0x1234),
    SYMBOL(plugin_process_sample, 0x1234),
    SYMBOL(plugin_get_public_info, 0x1234),
    SYMBOL(plugin_add_properties, 0x1234),
    SYMBOL(plugin_activate_properties, 0x1234),
    SYMBOL(plugin_help_properties, 0x1234),
    SYMBOL(plugin_add_gui, 0x1234),
    SYMBOL(plugin_remove_gui, 0x1234),
    SYMBOL(plugin_get_misc_info, 0x1234)
  };
  gint num_exported_symbols =
    sizeof(table_of_symbols)/sizeof(struct plugin_exported_symbol);
  gint i;

  /* Try to find the given symbol in the table of exported symbols
   of the plugin */
  for (i=0; i<num_exported_symbols; i++)
    if (!strcmp(table_of_symbols[i].symbol, name))
      {
	if (table_of_symbols[i].hash != hash)
	  {
	    if (ptr)
	      *ptr = GINT_TO_POINTER(0x3); /* hash collision code */
	    /* Warn */
	    g_warning(_("Check error: \"%s\" in plugin %s"
		       " has hash 0x%x vs. 0x%x"), name,
		      str_canonical_name, 
		      table_of_symbols[i].hash,
		      hash);
	    return FALSE;
	  }
	if (ptr)
	  *ptr = table_of_symbols[i].ptr;
	return TRUE; /* Success */
      }

  if (ptr)
    *ptr = GINT_TO_POINTER(0x2); /* Symbol not found in the plugin */
  return FALSE;
}

static
void plugin_get_info (gchar ** canonical_name, gchar **
		  descriptive_name, gchar ** description, gchar **
		  short_description, gchar ** author, gchar **
		  version)
{
  /* Usually, this one doesn't need modification either */
  if (canonical_name)
    *canonical_name = _(str_canonical_name);
  if (descriptive_name)
    *descriptive_name = _(str_descriptive_name);
  if (description)
    *description = _(str_description);
  if (short_description)
    *short_description = _(str_short_description);
  if (author)
    *author = _(str_author);
  if (version)
    *version = _(str_version);
}

static
gboolean plugin_init ( PluginBridge bridge, tveng_device_info * info )
{
  zapping_info = info;

  return TRUE;
}

static
void plugin_close(void)
{
  close_everything = TRUE;
}

static
gboolean plugin_start (void)
{
  /* If everything has been ok, set the active flags and return TRUE
   */
  return TRUE;
}

static
void plugin_load_config (gchar * root_key)
{
}

static
void plugin_save_config (gchar * root_key)
{
}

static
void plugin_process_sample(plugin_sample * sample)
{
}

static
gboolean plugin_get_public_info (gint index, gpointer * ptr, gchar **
			     symbol, gchar ** description, gchar **
			     type, gint * hash)
{
  return FALSE; /* Nothing exported */
}

static
void plugin_add_properties ( GnomePropertyBox * gpb )
{
  GtkWidget *mpeg_properties =
    build_widget("notebook1", PACKAGE_DATA_DIR "/mpeg_properties.glade");
  GtkWidget * label = gtk_label_new(_("MPEG"));
  gint page;

  if (!mpeg_properties)
    {
      ShowBox("mpeg_properties.glade couldn't be found,\n"
	      "the mpeg plugin properties cannot be added.",
	      GNOME_MESSAGE_BOX_ERROR);
      return;
    }

  gtk_widget_show(mpeg_properties);
  gtk_widget_show(label);

  page = gnome_property_box_append_page(gpb, mpeg_properties, label);

  gtk_object_set_data(GTK_OBJECT(gpb), "mpeg_page", GINT_TO_POINTER (page));
}

static
gboolean plugin_activate_properties ( GnomePropertyBox * gpb, gint page )
{
  gpointer data = gtk_object_get_data(GTK_OBJECT(gpb), "mpeg_page");

  if (GPOINTER_TO_INT(data) == page)
    {
      g_message("FIXME: activate mpeg properties");
      return TRUE;
    }
  else
    return FALSE;
}

static
gboolean plugin_help_properties ( GnomePropertyBox * gpb, gint page )
{
  gpointer data = gtk_object_get_data(GTK_OBJECT(gpb), "mpeg_page");

  if (GPOINTER_TO_INT(data) == page)
    {
      g_message("FIXME: help about mpeg properties");
      return TRUE;
    }
  else
    return FALSE;
}

/* User defined functions */
static void
on_mpeg_button_clicked          (GtkButton       *button,
				 gpointer         user_data)
{
  ShowBox(_("Not done yet!"), GNOME_MESSAGE_BOX_ERROR);
}

static
void plugin_add_gui (GnomeApp * app)
{
  GtkWidget * toolbar1 = lookup_widget(GTK_WIDGET(app), "toolbar1");
  GtkWidget * button; /* The button to add */
  GtkWidget * tmp_toolbar_icon;

  tmp_toolbar_icon =
    gnome_stock_pixmap_widget (GTK_WIDGET(app),
			       GNOME_STOCK_PIXMAP_COLORSELECTOR);
  button = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar1),
				      GTK_TOOLBAR_CHILD_BUTTON, NULL,
				      _("MPEG"),
				      _("Store the data as MPEG"),
				      NULL, tmp_toolbar_icon,
				      on_mpeg_button_clicked,
				      NULL);
  gtk_widget_ref (button);

  /* Set up the widget so we can find it later */
  gtk_object_set_data_full (GTK_OBJECT(app), "mpeg_button",
			    button, (GtkDestroyNotify)
			    gtk_widget_unref);

  gtk_widget_show(button);
}

static
void plugin_remove_gui (GnomeApp * app)
{
  GtkWidget * button = 
    GTK_WIDGET(gtk_object_get_data(GTK_OBJECT(app),
				   "mpeg_button"));
  GtkWidget * toolbar1 = lookup_widget(GTK_WIDGET(app), "toolbar1");

  gtk_container_remove(GTK_CONTAINER(toolbar1), button);
}

static
struct plugin_misc_info * plugin_get_misc_info (void)
{
  static struct plugin_misc_info returned_struct =
  {
    sizeof(struct plugin_misc_info), /* size of this struct */
    -10, /* plugin priority, we must be executed with a fully
	    processed image */
    /* Cathegory */
    PLUGIN_CATHEGORY_VIDEO_OUT |
    PLUGIN_CATHEGORY_GUI
  };

  /*
    Tell that the template plugin should be run with a somewhat high
    priority (just to put an example)
  */
  return (&returned_struct);
}
