/* Screenshot saving plugin for Zapping
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
/*
  This plugin was builded from the template one. It does some thing
  that the template one doesn't, such as adding itself to the gui and
  adding help for the properties.
*/

/* This is the description of the plugin, change as appropiate */
static const gchar str_canonical_name[] = "screenshot";
static const gchar str_descriptive_name[] =
N_("Screenshot saver");
static const gchar str_description[] =
N_("You can use this plugin to take screenshots of what you are"
" actually watching in TV.\nIt will save the screenshots in PNG"
" format.");
static const gchar str_short_description[] = 
N_("This plugin takes screenshots of the capture.");
static const gchar str_author[] = "Iñaki García Etxebarria";
/* The format of the version string must be
   %d[[.%d[.%d]][other_things]], where the things between [] aren't
   needed, and if not present, 0 will be assumed */
static const gchar str_version[] = "0.6";

/* Active status of the plugin */
static gboolean active = FALSE;

/* Where should the screenshots be saved to (dir) */
static gchar * save_dir = NULL;

static gboolean interlaced; /* Whether the image should be saved
			       interlaced or not */

/*
  TRUE if plugin_start has been called and plugin_process_frame
  hasn't
*/
static gboolean save_screenshot = FALSE; 

/* Callbacks */
static void
on_screenshot_button_clicked          (GtkButton       *button,
				       gpointer         user_data);

/* This function is called when some item in the property box changes */
static void
on_property_item_changed              (GtkWidget * changed_widget,
				       GnomePropertyBox *propertybox);

/* This one starts a new thread that saves the current screenshot */
static void
start_saving_screenshot (gpointer data,
			 struct tveng_frame_format * format);

gint zp_protocol ( void )
{
  /* You don't need to modify this function */
  return PLUGIN_PROTOCOL;
}

gboolean zp_running (void)
{
  /* This will usually be like this too */
  return active;
}

void zp_get_info (gchar ** canonical_name, gchar **
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

gboolean zp_init (PluginBridge bridge, tveng_device_info * info)
{
  /* Do any startup you need here, and return FALSE on error */

  /* If this is set, autostarting is on (we should start now) */
  if (active)
    return zp_start();

  return TRUE;
}

void zp_close(void)
{
  /* If we were working, stop the work */
  if (active)
    zp_stop();

  /* Any cleanups would go here (closing fd's and so on) */
}

gboolean zp_start (void)
{
  /* In most plugins, you don't want to be started twice */
  if (active)
    return TRUE;

  save_screenshot = TRUE;

  /* If everything has been ok, set the active flags and return TRUE
   */
  active = TRUE;
  return TRUE;
}

void zp_stop(void)
{
  /* Most times we cannot be stopped while we are stopped */
  if (!active)
    return;

  /* Stop anything the plugin is doing and set the flag */
  active = FALSE;
}

void zp_load_config (gchar * root_key)
{
  gchar * buffer;

  /* The autostart config value is compulsory, you shouldn't need to
     change the following */
  buffer = g_strconcat(root_key, "autostart", NULL);
  /* Create sets a default value for a key, check src/zconf.h */
  zconf_create_boolean(FALSE,
		       _("Whether the plugin should start"
			 " automatically when opening Zapping"), buffer);
  active = zconf_get_boolean(NULL, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "interlaced", NULL);
  zconf_create_boolean(TRUE,
		       _("Whether interlacing should be used"),
		       buffer);
  interlaced = zconf_get_boolean(NULL, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "save_dir", NULL);
  zconf_create_string(getenv("HOME"),
		      _("The directory where screenshot will be"
			" written to"), buffer);
  zconf_get_string(&save_dir, buffer);
  g_free(buffer);
}

void zp_save_config (gchar * root_key)
{
  gchar * buffer;

  /* This one is compulsory, you won't need to change it */
  buffer = g_strconcat(root_key, "autostart", NULL);
  zconf_set_boolean(active, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "save_dir", NULL);
  zconf_set_string(save_dir, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "interlaced", NULL);
  zconf_set_boolean(interlaced, buffer);
  g_free(buffer);

  g_free(save_dir);
}

gpointer zp_process_frame(gpointer data, struct tveng_frame_format *
			  format)
{
  /* If the plugin isn't active, it shouldn't do anything */
  if (!active)
    return data;

  if (save_screenshot)
    {
      start_saving_screenshot(data, format);
      save_screenshot = FALSE;
    }

  /*
    Return the modified data (the same as the supplied on, in this
    case).
  */
  return data;
}

void zp_add_properties ( GnomePropertyBox * gpb )
{
  GtkWidget * label;
  GtkBox * vbox; /* the page added to the notebook */
  GtkWidget * widget;
  gint page;

  label = gtk_label_new(_("PNG saver"));
  vbox = GTK_BOX(gtk_vbox_new(FALSE, 15));

  widget =
    gtk_label_new(_("Select here the directory where screenshots will"
		    " be saved"));
  gtk_widget_show(widget);
  gtk_box_pack_start(vbox, widget, FALSE, TRUE, 0);

  widget = gnome_file_entry_new("screenshot_save_dir_history",
   _("Select directory to save screenshots"));
  gnome_file_entry_set_directory(GNOME_FILE_ENTRY(widget), TRUE);
  gnome_entry_load_history(GNOME_ENTRY(gnome_file_entry_gnome_entry
	(GNOME_FILE_ENTRY(widget))));
  gnome_file_entry_set_default_path(GNOME_FILE_ENTRY(widget),
				    save_dir);
  gnome_file_entry_set_modal(GNOME_FILE_ENTRY(widget), TRUE);
  /* Store a pointer to the widget so we can find it later */
  gtk_object_set_data(GTK_OBJECT(gpb), "screenshot_save_dir", widget);
  gtk_widget_show(widget);
  gtk_box_pack_start(vbox, widget, FALSE, TRUE, 0);
  widget = gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY(widget));
  gtk_entry_set_text(GTK_ENTRY(widget), save_dir);
  gtk_signal_connect(GTK_OBJECT(widget), "changed",
		     on_property_item_changed, gpb);

  /* The interlaced checkbutton */
  widget = gtk_check_button_new_with_label(_("Save interlaced PNG"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), interlaced);
  gtk_object_set_data (GTK_OBJECT(gpb), "screenshot_interlaced",
		       widget);
  gtk_signal_connect(GTK_OBJECT(widget), "toggled",
		     on_property_item_changed, gpb);
  gtk_box_pack_start(vbox, widget, FALSE, TRUE, 0);
  gtk_widget_show(widget);

  gtk_widget_show(label);
  gtk_widget_show(GTK_WIDGET(vbox));

  page = gnome_property_box_append_page(gpb, GTK_WIDGET(vbox), label);

  gtk_object_set_data(GTK_OBJECT(gpb), "screenshot_page",
		      GINT_TO_POINTER( page ));
}

gboolean zp_activate_properties ( GnomePropertyBox * gpb, gint page )
{
  /* Return TRUE only if the given page have been builded by this
     plugin, and apply any config changes here */
  gpointer data = gtk_object_get_data(GTK_OBJECT(gpb), "screenshot_page");
  GnomeFileEntry * save_dir_widget
    = GNOME_FILE_ENTRY(gtk_object_get_data(GTK_OBJECT(gpb),
					   "screenshot_save_dir"));
  GtkToggleButton * interlaced_widget =
    GTK_TOGGLE_BUTTON(gtk_object_get_data(GTK_OBJECT(gpb),
					  "screenshot_interlaced"));

  if (GPOINTER_TO_INT(data) == page)
    {
      /* It is our page, process */
      g_free(save_dir);
      save_dir = gnome_file_entry_get_full_path(save_dir_widget,
						FALSE);
      gnome_entry_save_history(GNOME_ENTRY(gnome_file_entry_gnome_entry(
	 save_dir_widget)));
      interlaced = gtk_toggle_button_get_active(interlaced_widget);
      return TRUE;
    }

  return FALSE;
}

gboolean zp_help_properties ( GnomePropertyBox * gpb, gint page )
{
  /*
    Return TRUE only if the given page have been builded by this
    plugin, and show some help (or at least sth like ShowBox
    "Sorry, but the template plugin doesn't help you").
  */
  gpointer data = gtk_object_get_data(GTK_OBJECT(gpb), "screenshot_page");
  gchar * help =
N_("The first option, the screenshot dir, lets you specify where\n"
   "will the screenshots be saved. The file name will be:\n"
   "save_dir/shot[1,2,3,...].png\n\n"
   "The interlacing lets you specify if the saved image will be\n"
   "interlaced. This allows image viewers load it progressively."
);

  if (GPOINTER_TO_INT(data) == page)
    {
      ShowBox(_(help), GNOME_MESSAGE_BOX_INFO);
      return TRUE;
    }

  return FALSE;
}

void zp_add_gui (GnomeApp * app)
{
  GtkWidget * toolbar1 = lookup_widget(GTK_WIDGET(app), "toolbar1");
  GtkWidget * button; /* The button to add */
  GtkWidget * tmp_toolbar_icon;

  tmp_toolbar_icon =
    gnome_stock_pixmap_widget (GTK_WIDGET(app),
			       GNOME_STOCK_PIXMAP_COLORSELECTOR);
  button = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar1),
				      GTK_TOOLBAR_CHILD_BUTTON, NULL,
				      _("Screenshot"),
				      _("Take a PNG screenshot"),
				      NULL, tmp_toolbar_icon,
				      on_screenshot_button_clicked,
				      NULL);
  gtk_widget_ref (button);

  /* Set up the widget so we can find it later */
  gtk_object_set_data_full (GTK_OBJECT(app), "screenshot_button",
			    button, (GtkDestroyNotify)
			    gtk_widget_unref);

  gtk_widget_show(button);
}

void zp_remove_gui (GnomeApp * app)
{
  GtkWidget * button = 
    GTK_WIDGET(gtk_object_get_data(GTK_OBJECT(app),
				   "screenshot_button"));
  GtkWidget * toolbar1 = lookup_widget(GTK_WIDGET(app), "toolbar1");

  gtk_container_remove(GTK_CONTAINER(toolbar1), button);
}

gint zp_get_priority (void)
{
  /*
    This plugin must be run after all the other plugins, because we
    want to save the image fully processed.
  */
  return -10;
}

/* User defined functions */
static void
on_screenshot_button_clicked          (GtkButton       *button,
				       gpointer         user_data)
{
  zp_start ();
}

static void
on_property_item_changed              (GtkWidget * changed_widget,
				       GnomePropertyBox *propertybox)
{
  gnome_property_box_changed (propertybox);
}

/* This struct is shared between the main and the saving thread */
struct screenshot_data
{
  gpointer data; /* Pointer to the allocated data */
};

/* This one starts a new thread that saves the current screenshot */
static void
start_saving_screenshot (gpointer data,
			 struct tveng_frame_format * format)
{
  ShowBox(_("We should start saving here"), GNOME_MESSAGE_BOX_INFO);
}
