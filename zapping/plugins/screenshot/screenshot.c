/*
 * Screenshot saving plugin for Zapping
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
 * Copyright (C) 2001 Michael H. Schimek
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
#include "screenshot.h"
#include "yuv2rgb.h"
#include "ttxview.h"
#include "properties.h"
#include <gdk-pixbuf/gdk-pixbuf.h> /* previews */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h> /* flock */

/*
  This plugin was built from the template one. It does some thing
  that the template one doesn't, such as adding itself to the gui and
  adding help for the properties.
*/

/* This is the description of the plugin, change as appropiate */
static const gchar str_canonical_name[] = "screenshot";
static const gchar str_descriptive_name[] =
N_("Screenshot saver");
static const gchar str_description[] =
N_("With this plugin you can take screen shots of the program "
   "you are watching and save them in various formats.");
static const gchar str_short_description[] = 
N_("This plugin takes screenshots of the video capture.");
static const gchar str_author[] =
"Iñaki García Etxebarria & Michael H. Schimek";
/* The format of the version string must be
   %d[[.%d[.%d]][other_things]], where the things between [] aren't
   needed, and if not present, 0 will be assumed */
static const gchar str_version[] = "0.8";

/* Set to TRUE when plugin_close is called */
gboolean screenshot_close_everything = FALSE;

/* Number of threads currently running (so we can know when are all of
   them closed) */
static gint num_threads = 0;

/* Where should the screenshots be saved to (dir) */
static gchar * save_dir = NULL;

/* Command to run after saving the screenshot */
static gchar * command = NULL;

static gint quality; /* Quality of the compressed image */

/* Last format (screenshot_backend.keyword) */
static gchar *format = NULL;

static tveng_device_info * zapping_info = NULL; /* Info about the
						   video device */

/* Properties handling code */
static void
properties_add			(GnomeDialog	*dialog);

/* Callbacks */
static void
on_screenshot_button_clicked          (GtkButton       *button,
				       gpointer         user_data);
static void
on_screenshot_button_fast_clicked	(GtkButton       *button,
					 gpointer         user_data);

/*
 *  Conversions between different RGB pixel formats
 *
 *  These functions are exported through the symbol mechanism
 *  so other plugins can use them too.
 */
static gchar *Convert_RGB565_RGB24 (gint width, gchar* src, gchar* dest);
static gchar *Convert_BGR565_RGB24 (gint width, gchar* src, gchar* dest);
static gchar *Convert_RGB555_RGB24 (gint width, gchar* src, gchar* dest);
static gchar *Convert_BGR555_RGB24 (gint width, gchar* src, gchar* dest);
static gchar *Convert_RGBA_RGB24 (gint width, gchar* src, gchar* dest);
static gchar *Convert_BGRA_RGB24 (gint width, gchar* src, gchar* dest);
static gchar *Convert_BGR24_RGB24 (gint width, gchar* src, gchar* dest);
static gchar *Convert_YUYV_RGB24 (gint w, guchar *src, guchar *dest);
static gchar *Convert_Null (gint width, gchar *src, gchar *dest);

/*
 *  plugin_read_bundle interface
 */
static screenshot_data *grab_data;
static int grab_countdown = 0;

/*
 *  Encoding backends
 */
extern screenshot_backend screenshot_backend_jpeg;
extern screenshot_backend screenshot_backend_ppm;

static struct screenshot_backend *
backends[] =
{
#ifdef HAVE_LIBJPEG
  &screenshot_backend_jpeg,
#endif
  &screenshot_backend_ppm,
  NULL
};


/*
  Callback when the WM tries to destroy the progress window, cancel
  job.
*/
static gboolean
on_progress_delete_event               (GtkWidget       *widget,
					GdkEvent        *event,
                                        struct screenshot_data * data);

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
    SYMBOL(plugin_read_bundle, 0x1234),
    SYMBOL(plugin_get_public_info, 0x1234),
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
void plugin_get_info (const gchar ** canonical_name,
		      const gchar ** descriptive_name,
		      const gchar ** description,
		      const gchar ** short_description,
		      const gchar ** author,
		      const gchar ** version)
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

/*
 *  Creative WebCam (ov511) grab button
 *  Contributed by Sandino Flores Moreno <tigrux@avantel.net>
 */

static gint ogb_timeout_id = -1;

static gint
ov511_grab_button_timeout (gint *timeout_id)
{
  switch (tveng_ov511_get_button_state(zapping_info))
    {
    case 1:
      plugin_start();
      break;
    case 0:
      /* Not clicked but camera detected, continue */
      break;
    default:
      /* Some kind of error, destroy */
      *timeout_id = -1;
      return FALSE;
    }

  return TRUE;
}

static
gboolean plugin_init ( PluginBridge bridge, tveng_device_info * info )
{
  /* Register the plugin as interested in the properties dialog */
  property_handler screenshot_handler =
  {
    add: properties_add
  };

  append_property_handler(&screenshot_handler);

  ogb_timeout_id =
    gtk_timeout_add (250 /* ms */,
		     (GtkFunction) ov511_grab_button_timeout,
		     &ogb_timeout_id);

  zapping_info = info;

  return TRUE;
}

static
void plugin_close(void)
{
  screenshot_close_everything = TRUE;

  if (ogb_timeout_id >= 0)
    {
      gtk_timeout_remove (ogb_timeout_id);
      ogb_timeout_id = -1;
    }

  while (num_threads) /* Wait until all threads exit cleanly */
    {
      z_update_gui();

      usleep(5000);
    }
}

static gboolean
plugin_start (void)
{
  on_screenshot_button_fast_clicked (NULL, NULL);
  return TRUE;
}

static
void plugin_load_config (gchar * root_key)
{
  gchar * buffer;
  gchar * default_save_dir;

  default_save_dir = g_strconcat(getenv("HOME"), "/shots", NULL);

  buffer = g_strconcat(root_key, "quality", NULL);
  zconf_create_integer(75,
		       "Quality of the compressed image",
		       buffer);
  quality = zconf_get_integer(NULL, buffer);
  g_free(buffer);

  buffer = g_strconcat (root_key, "format", NULL);
  zconf_create_string ("jpeg", "File format", buffer);
  zconf_get_string (&format, buffer);
  g_free (buffer);

  buffer = g_strconcat(root_key, "save_dir", NULL);
  zconf_create_string(default_save_dir,
		      "The directory where screenshot will be"
			" written to", buffer);
  zconf_get_string(&save_dir, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "command", NULL);
  zconf_create_string("", "Command to run after taking the screenshot",
		      buffer);
  zconf_get_string(&command, buffer);
  if (!command)
    command = g_strdup("");
  g_free(buffer);

  g_free(default_save_dir);
}

static
void plugin_save_config (gchar * root_key)
{
  gchar * buffer;

  buffer = g_strconcat(root_key, "save_dir", NULL);
  zconf_set_string(save_dir, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "quality", NULL);
  zconf_set_integer(quality, buffer);
  g_free(buffer);

  buffer = g_strconcat (root_key, "format", NULL);
  zconf_set_string (format, buffer);
  g_free (buffer);

  buffer = g_strconcat(root_key, "command", NULL);
  zconf_set_string(command, buffer);
  g_free(buffer);

  g_free(save_dir);
  g_free(command);
}

static
gboolean plugin_get_public_info (gint index, gpointer * ptr, gchar **
			     symbol, gchar ** description, gchar **
			     type, gint * hash)
{
  /* Export the conversion functions */
  struct plugin_exported_symbol symbols[] =
  {
    {
      Convert_RGB565_RGB24, "Convert_RGB565_RGB24",
      N_("Converts a row in RGB565 format to RGB24. Returns the destination."),
      "gchar * Convert_RGB565_RGB24 ( gint width, gchar * src, gchar * dest);",
      0x1234
    },
    {
      Convert_RGB555_RGB24, "Convert_RGB555_RGB24",
      N_("Converts a row in RGB555 format to RGB24. Returns the destination."),
      "gchar * Convert_RGB555_RGB24 ( gint width, gchar * src, gchar * dest);",
      0x1234
    },
    {
      Convert_RGBA_RGB24, "Convert_RGBA_RGB24",
      N_("Converts a row in RGBA format to RGB24. Returns the destination."),
      "gchar * Convert_RGBA_RGB24 ( gint width, gchar * src, gchar * dest);",
      0x1234
    }
  };
  gint num_exported_symbols =
    sizeof(symbols)/sizeof(struct plugin_exported_symbol);

  if ((index >= num_exported_symbols) || (index < 0))
    return FALSE;

  if (ptr)
    *ptr = symbols[index].ptr;
  if (symbol)
    *symbol = symbols[index].symbol;
  if (description)
    *description = _(symbols[index].description);
  if (type)
    *type = symbols[index].type;
  if (hash)
    *hash = symbols[index].hash;

  return TRUE; /* Exported */
}

static void
screenshot_setup		(GtkWidget	*page)
{
  GtkWidget *screenshot_quality;
  GtkWidget *screenshot_dir;
  GtkWidget *screenshot_command;
  GtkWidget *combo_entry1;
  GtkAdjustment *adj;

  screenshot_quality =
    lookup_widget(page, "screenshot_quality");
  screenshot_dir =
    lookup_widget(page, "screenshot_dir");
  screenshot_command =
    lookup_widget(page, "screenshot_command");
  combo_entry1 = lookup_widget(page, "combo-entry1");

  gnome_file_entry_set_default_path(GNOME_FILE_ENTRY(screenshot_dir),
				    save_dir);

  gtk_entry_set_text(GTK_ENTRY(combo_entry1), save_dir);

  adj = gtk_range_get_adjustment(GTK_RANGE(screenshot_quality));
  gtk_adjustment_set_value(adj, quality);

  gtk_entry_set_text(GTK_ENTRY(screenshot_command), command);
}

static void
screenshot_apply		(GtkWidget	*page)
{
  GtkAdjustment *adj = gtk_range_get_adjustment
    (GTK_RANGE(lookup_widget(page, "screenshot_quality")));
  GnomeFileEntry * screenshot_dir = GNOME_FILE_ENTRY
    (lookup_widget(page, "screenshot_dir"));
  GtkWidget *screenshot_command =
    lookup_widget(page, "screenshot_command");

  g_free(save_dir);
  save_dir = gnome_file_entry_get_full_path(screenshot_dir, FALSE);
  gnome_entry_save_history(GNOME_ENTRY(gnome_file_entry_gnome_entry(
	 screenshot_dir)));
  quality = adj->value;

  g_free(command);
  command = g_strdup(gtk_entry_get_text(GTK_ENTRY(screenshot_command)));
}

static void
screenshot_help			(GtkWidget	*widget)
{
  gchar * help =
    N_("The first option, the screenshot dir, lets you specify where\n"
       "will the screenshots be saved. The file name will be:\n"
       "save_dir/shot[1,2,3,...].ext\n\n"
       "The quality option lets you choose how much info will be\n"
       "discarded when compressing the image.\n\n"
       "The command will be run after writing to disk the screenshot,\n"
       "with the environmental variables $SCREENSHOT_PATH, $CHANNEL_ALIAS,\n"
       "$CHANNEL_ID, $CURRENT_STANDARD, $CURRENT_INPUT set to their\n"
       "correct value."
       );

  ShowBox(_(help), GNOME_MESSAGE_BOX_INFO);
}

static void
properties_add			(GnomeDialog	*dialog)
{
  SidebarEntry plugin_options[] = {
    { N_("Screenshot"), ICON_ZAPPING, "gnome-digital-camera.png", "vbox1",
      screenshot_setup, screenshot_apply, screenshot_help }
  };
  SidebarGroup groups[] = {
    { N_("Plugins"), plugin_options, acount(plugin_options) }
  };

  standard_properties_add(dialog, groups, acount(groups),
			  PACKAGE_DATA_DIR "/screenshot.glade");
}

static
void plugin_add_gui (GnomeApp * app)
{
  GtkWidget * toolbar1 = lookup_widget(GTK_WIDGET(app), "toolbar1");
  GtkWidget * button;
  GtkWidget * tmp_toolbar_icon;

  tmp_toolbar_icon =
    gnome_stock_pixmap_widget (GTK_WIDGET(app),
			       GNOME_STOCK_PIXMAP_COLORSELECTOR);
  button = gtk_toolbar_append_element(GTK_TOOLBAR(toolbar1),
				      GTK_TOOLBAR_CHILD_BUTTON, NULL,
				      _("Screenshot"),
				      _("Take a screenshot [s, Ctrl+s]"),
				      NULL, tmp_toolbar_icon,
				      on_screenshot_button_clicked,
				      NULL);

  /* Ctrl variants, start recording without configuring */
  gtk_signal_connect (GTK_OBJECT(button), "fast-clicked",
		      GTK_SIGNAL_FUNC(on_screenshot_button_fast_clicked),
		      NULL);

  z_widget_add_accelerator (button, "fast-clicked", GDK_s, GDK_CONTROL_MASK);
  z_widget_add_accelerator (button, "clicked", GDK_s, 0);

  /* Set up the widget so we can find it later */
  gtk_object_set_data (GTK_OBJECT(app), "screenshot_button",
		       button);

  gtk_widget_show(button);
}

static
void plugin_remove_gui (GnomeApp * app)
{
  GtkWidget * button = 
    GTK_WIDGET(gtk_object_get_data(GTK_OBJECT(app),
				   "screenshot_button"));
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
    PLUGIN_CATHEGORY_FILTER |
    PLUGIN_CATHEGORY_GUI
  };

  /*
    Tell that the template plugin should be run with a somewhat high
    priority (just to put an example)
  */
  return (&returned_struct);
}

static void
screenshot_destroy (screenshot_data *data)
{
  if (!data)
    return;

  data->lines = 0;

  if (data->filename)
    {
      if (data->io_fp)
	unlink (data->filename);

      g_free (data->filename);
    }

  if (data->command)
    g_free (data->command);

  if (data->status_window)
    gtk_widget_destroy (data->status_window);

  if (data->io_buffer)
    g_free (data->io_buffer);

  data->io_buffer_size = 0;
  data->io_buffer_used = 0;

  data->io_flush = NULL;

  if (data->io_fp)
    fclose (data->io_fp);

  if (data->error)
    g_free (data->error);

  data->Converter = NULL;

  if (data->line_data)
    g_free (data->line_data);

  if (data->data)
    g_free (data->data);

  if (data->auto_filename)
    g_free (data->auto_filename);

  if (data->pixbuf)
    gdk_pixbuf_unref (data->pixbuf);

  if (data->dialog)
    gtk_widget_destroy (data->dialog);

  g_free (data);
}

static screenshot_data *
screenshot_new (void)
{
    gint i;
    gint private_max = 0;

    for (i = 0; backends[i]; i++)
      if (backends[i]->sizeof_private > private_max)
        private_max = backends[i]->sizeof_private;

    return (screenshot_data *)
      g_malloc0 (sizeof (screenshot_data) + private_max);
}

static gboolean
io_buffer_init (screenshot_data *data, gint size)
{
  data->io_buffer = g_malloc (size);

  if (!data->io_buffer)
    return FALSE;

  data->io_buffer_size = size;
  data->io_buffer_used = 0;

  return TRUE;
}

static gboolean
io_flush_stdio (screenshot_data *data, gint size)
{
  if (data->thread_abort)
    return FALSE;

  if (fwrite (data->io_buffer, 1, size, data->io_fp) != size)
    {
      data->error = g_strconcat(_("Error while writing screenshot\n"),
				data->filename, "\n",
				strerror (errno), NULL);
      data->thread_abort = TRUE;
      return FALSE;
    }

  data->io_buffer_used += size;

  return TRUE;
}

static gboolean
io_flush_memory (screenshot_data *data, gint size)
{
  if (data->io_buffer_used > 0)
    {
      data->thread_abort = TRUE;
      return FALSE; /* output exceeds data->io_buffer_size */
    }

  data->io_buffer_used = size;

  return TRUE;
}

static void
execute_command (screenshot_data *data)
{
  extern int cur_tuned_channel;
  extern tveng_tuned_channel *global_channel_list;
  tveng_tuned_channel *tc;
  char *argv[10];
  int argc = 0;
  char *env[10];
  int envc = 0;
  int i;

  /* Invoke through sh */
  argv[argc++] = "sh";
  argv[argc++] = "-c";
  argv[argc++] = data->command;

  /* FIXME */
  env[envc++] = g_strdup_printf ("SCREENSHOT_PATH=%s", data->filename);

  /* XXX thread safe? prolly not */
  tc = tveng_retrieve_tuned_channel_by_index
    (cur_tuned_channel, global_channel_list);

  if (tc)
    {
      env[envc++] = g_strdup_printf ("CHANNEL_ALIAS=%s", tc->name);
      env[envc++] = g_strdup_printf ("CHANNEL_ID=%s", tc->real_name);
      if (zapping_info->num_standards)
	env[envc++] =
	  g_strdup_printf ("CURRENT_STANDARD=%s",
	    zapping_info->standards[zapping_info->cur_standard].name);
      if (zapping_info->num_inputs)
	env[envc++] =
	  g_strdup_printf ("CURRENT_INPUT=%s",
	    zapping_info->inputs[zapping_info->cur_input].name);
    }

  gnome_execute_async_with_env (NULL, argc, argv, envc, env);

  for (i = 0; i < envc; i++)
    g_free (env[i]);
}

static void *
screenshot_saving_thread (void *_data)
{
  screenshot_data *data = (screenshot_data *) _data;

  data->backend->save (data);

  if (data->thread_abort || data->error)
    {
      unlink (data->filename);
      fclose (data->io_fp);
    }
  else if (fclose (data->io_fp) != 0)
    {
      data->error = g_strconcat(_("Error while writing screenshot\n"),
				data->filename, "\n",
				strerror(errno), NULL);
      unlink (data->filename);
      data->thread_abort = TRUE;
    }
  else if (data->command)
    execute_command (data);

  data->io_fp = NULL;

  data->status = 5; /* thread done */

  return NULL;
}

static gboolean
on_progress_delete_event               (GtkWidget       *widget,
					GdkEvent        *event,
					screenshot_data *data)
{
  data->thread_abort = TRUE; /* thread shall abort */
  data->status_window = NULL; /* will be destroyed */

  return FALSE; /* destroy window */
}

static GtkWidget *
create_status_window (screenshot_data *data,
		      gchar *filename)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *progressbar;

  label = gtk_label_new (filename);
  gtk_widget_show (label);

  progressbar =
    gtk_progress_bar_new_with_adjustment (
      GTK_ADJUSTMENT (gtk_adjustment_new (0, 0, 100, 1, 10, 10)));
  gtk_widget_show (progressbar);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start_defaults (GTK_BOX (vbox), label);
  gtk_box_pack_start_defaults (GTK_BOX (vbox), progressbar);
  gtk_widget_show (vbox);

  window = gtk_window_new (GTK_WINDOW_DIALOG);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_window_set_title (GTK_WINDOW (window), _("Saving..."));
  gtk_window_set_modal (GTK_WINDOW (window), FALSE);
  gtk_object_set_data (GTK_OBJECT (window), "progressbar", progressbar);
  gtk_signal_connect (GTK_OBJECT (window), "delete-event",
		      (GtkSignalFunc) on_progress_delete_event, data);
  gtk_widget_show (window);

  return window;
}

static gboolean
screenshot_save (screenshot_data *data,
		 gchar *filename)
{
  GtkWindow *window;
  gchar *b, *dir;

  dir = g_dirname (filename);

  if (!z_build_path (dir, &b))
    {
      ShowBox (_("Cannot create directory:\n%s\n%s"),
	       GNOME_MESSAGE_BOX_WARNING, dir, b);
      g_free (b);
      g_free (dir);
      return FALSE;
    }

  g_free (dir);

  if (!(data->io_fp = fopen (filename, "wb")))
    {
      gchar *window_title;

      window_title = g_strconcat (_("Sorry, but I cannot write\n"),
				  filename,
				  _("\nThe image won't be saved.\n"),
				  strerror (errno),
				  NULL);
      ShowBox (window_title, GNOME_MESSAGE_BOX_ERROR);
      g_free (window_title);
      return FALSE;
    }

  data->filename = g_strdup (filename);

  if (!data->io_buffer)
    if (!io_buffer_init (data, 1 << 10))
      return FALSE;

  data->io_flush = io_flush_stdio;

  if (!data->backend->init (data, /* write */ TRUE, quality))
    return FALSE;

  data->status_window = create_status_window (data, filename);

  if (command && command[0])
    data->command = g_strdup (command); /* may change until */

  data->thread_abort = FALSE;

  data->lines = 0;

  switch (pthread_create (&data->saving_thread, NULL,
			  screenshot_saving_thread, data))
    {
    case ENOMEM:
      ShowBox (_("Sorry, not enough resources to create a new thread"), 
	       GNOME_MESSAGE_BOX_ERROR);
      return FALSE;

    case EAGAIN:
      ShowBox (_("There are too many threads"),
	       GNOME_MESSAGE_BOX_ERROR);
      return FALSE;

    case 0:
      num_threads++;
      grab_data = NULL; /* permit new request */
      data->status = 4; /* monitoring */
      return TRUE;

    default:
      return FALSE;
    }
}

#define PREVIEW_WIDTH 192
#define PREVIEW_HEIGHT 144

static void
preview (screenshot_data *data)
{
  gpointer old_data;
  struct tveng_frame_format old_format;

  g_assert (data && data->drawingarea && data->pixbuf);

  old_data = data->data;
  old_format = data->format;

  // XXX asserts image size >= preview size

  data->data += (int)
    (((data->format.width - PREVIEW_WIDTH) >> 1)
     * data->format.bpp
     + ((data->format.height - PREVIEW_HEIGHT) >> 1)
     * data->format.bytesperline);

  data->format.width = PREVIEW_WIDTH;
  data->format.height = PREVIEW_HEIGHT;

  if (data->backend->load)
    {
      if (!data->io_buffer)
	if (!io_buffer_init (data, PREVIEW_WIDTH
			     * PREVIEW_HEIGHT * 4))
	  goto restore;

      data->io_flush = io_flush_memory;
      data->io_buffer_used = 0;

      if (!data->backend->init (data, /* write */ TRUE, quality))
	goto restore;

      data->backend->save (data);

      if (data->thread_abort)
	goto restore;

      data->size_est = data->io_buffer_used
	* (double)(old_format.width * old_format.height)
	/ (double)(PREVIEW_WIDTH * PREVIEW_HEIGHT);

      if (!data->backend->init (data, /* write */ FALSE, 0))
	goto restore;

      data->backend->load (data, gdk_pixbuf_get_pixels (data->pixbuf),
			   gdk_pixbuf_get_rowstride (data->pixbuf));
    }
  else /* backend doesn't support preview (lossless format?) */
    {
      gint line, rowstride;
      gchar *s, *d, *row;

      s = data->data;
      d = gdk_pixbuf_get_pixels (data->pixbuf);
      rowstride = gdk_pixbuf_get_rowstride (data->pixbuf);

      for (line = 0; line < data->format.height; line++)
	{
	  (data->Converter)(data->format.width, s, d);
	  s += data->format.bytesperline;
	  d += rowstride;
	}

      data->size_est = data->backend->bpp_est
	* (double)(old_format.width * old_format.height);
    }

 restore:
  data->format = old_format;
  data->data = old_data;
}

static gboolean
on_drawingarea_expose_event             (GtkWidget      *widget,
                                         GdkEventExpose *event,
                                         screenshot_data *data)
{
  gchar buf[80];

  gdk_pixbuf_render_to_drawable (data->pixbuf, data->drawingarea->window,
				 data->drawingarea->style->white_gc,
				 0, 0, 0, 0,
				 PREVIEW_WIDTH, PREVIEW_HEIGHT,
				 GDK_RGB_DITHER_NORMAL, 0, 0);

  if (data->size_est < (double)(1 << 20))
    snprintf (buf, sizeof(buf) - 1, _("appx %.2f kB"),
	      data->size_est / (double)(1 << 10));
  else
    snprintf (buf, sizeof(buf) - 1, _("appx %.2f MB"),
	      data->size_est / (double)(1 << 20));

  gtk_label_set_text (GTK_LABEL (data->size_label), buf);

  return FALSE;
}

static gboolean
on_quality_changed                      (GtkWidget      *widget,
                                         screenshot_data *data)
{
  gint new_quality = GTK_ADJUSTMENT (widget)->value;  

  if (quality == new_quality)
    return FALSE;

  quality = new_quality;

  preview (data);
  on_drawingarea_expose_event (widget, NULL, data);

  return FALSE;
}                                                

static screenshot_backend *
find_backend (gchar *keyword)
{
    gint i;

    if (keyword)
      for (i = 0; backends[i]; i++)
	if (strcmp(keyword, backends[i]->keyword) == 0)
	  return backends[i];

    g_assert (backends[0] != NULL);

    return backends[0];
}

static void
on_format_changed                     (GtkWidget *menu,
				       screenshot_data *data)
{
  GtkWidget *menu_item = gtk_menu_get_active (GTK_MENU (menu));
  gchar *keyword, *name;

  keyword = gtk_object_get_data (GTK_OBJECT (menu_item), "keyword");

  data->backend = find_backend (keyword);

  g_assert (data->backend);

  gtk_widget_set_sensitive(data->quality_slider,
			   data->backend->quality);

  name = gtk_entry_get_text (data->entry);
  name = z_replace_filename_extension (name, data->backend->extension);
  gtk_entry_set_text (data->entry, name);
  g_free (name);

  preview (data);
  on_drawingarea_expose_event (menu, NULL, data);
}

static void
build_dialog (screenshot_data *data)
{
  GtkWidget *widget;
  GtkWidget *menu, *menu_item;
  GtkWidget *quality_slider;
  GtkAdjustment *adj;
  gchar *filename;
  gint default_item = 0;
  gint i;

  data->dialog = build_widget ("dialog1",
			       PACKAGE_DATA_DIR "/screenshot.glade");
  /* Format menu */

  widget = lookup_widget (data->dialog, "optionmenu1");

  if ((menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (widget))))
    gtk_widget_destroy (menu);

  menu = gtk_menu_new ();

  g_assert (backends[0] != NULL);

  for (i = 0; backends[i]; i++)
    {
      menu_item = gtk_menu_item_new_with_label (_(backends[i]->label));
      gtk_object_set_data (GTK_OBJECT (menu_item), "keyword",
			   backends[i]->keyword);
      gtk_widget_show (menu_item);
      gtk_menu_append (GTK_MENU (menu), menu_item);

      if (strcmp (format, backends[i]->keyword) == 0)
	default_item = i;
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (widget), default_item);
  gtk_signal_connect (GTK_OBJECT (GTK_OPTION_MENU (widget)->menu),
  		      "deactivate", on_format_changed, data);

  data->backend = backends[default_item];

  /* File entry */

  data->entry = GTK_ENTRY (lookup_widget (data->dialog, "entry"));
/* mhs doesn't like this. mhs will make this optional.
  gnome_dialog_editable_enters (GNOME_DIALOG (data->dialog),
				GTK_EDITABLE (data->entry));
*/
  gnome_dialog_set_default (GNOME_DIALOG (data->dialog), 0);
  filename = find_unused_name (save_dir, "shot",
			       data->backend->extension);
  data->auto_filename = g_strdup (g_basename (filename));
  gtk_entry_set_text (data->entry, filename);
  g_free (filename);
  gtk_object_set_data (GTK_OBJECT (data->entry),
		       "basename", (gpointer) data->auto_filename);
  gtk_signal_connect (GTK_OBJECT (data->entry), "changed",
		      GTK_SIGNAL_FUNC (z_on_electric_filename),
		      (gpointer) NULL);
  gtk_entry_select_region (data->entry, 0, -1);

  /* Preview */

  data->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				 /* has_alpha */ FALSE,
				 /* bits_per_sample */ 8,
				 PREVIEW_WIDTH, PREVIEW_HEIGHT);

  data->drawingarea = lookup_widget (data->dialog, "drawingarea1");
  data->size_label = lookup_widget (data->dialog, "label7");
  gdk_window_set_back_pixmap (data->drawingarea->window, NULL, FALSE);
  preview (data);
  gtk_signal_connect (GTK_OBJECT (data->drawingarea), "expose-event",
		      GTK_SIGNAL_FUNC (on_drawingarea_expose_event),
		      data);

  /* Quality slider */

  data->quality_slider = lookup_widget (data->dialog, "hscale1");
  adj = gtk_range_get_adjustment (GTK_RANGE (data->quality_slider));
  gtk_adjustment_set_value (GTK_ADJUSTMENT (adj), quality);
  gtk_signal_connect (GTK_OBJECT (adj), "value-changed",
		      GTK_SIGNAL_FUNC (on_quality_changed), data);

  gtk_widget_set_sensitive(data->quality_slider,
			   data->backend->quality);

  gnome_dialog_set_parent (GNOME_DIALOG (data->dialog),
			   z_main_window ());

  gtk_widget_grab_focus (GTK_WIDGET (data->entry));
}

/*
 *  This timer callback controls the progress of image saving.
 *  I [mhs] promise to never ever write something ugly like this
 *  again in my whole life, but for now I see no easy way around.
 *
 *  XXX non-atomic reads
 */
static gboolean
screenshot_timeout (screenshot_data *data)
{
  gchar *filename;
  gpointer result;

  switch (data->status)
    {
    case 0:
    case 1: /* waiting for image */
      if (data->lines-- <= 0)
	{
	  grab_data = NULL;
	  screenshot_destroy (data);
	  return FALSE; /* remove */
	}

      break;

    case 2: /* quick start */
      data->backend = find_backend (format);
      filename = find_unused_name (save_dir, "shot",
				   data->backend->extension);
    
      if (!screenshot_save (data, filename))
	{
	  grab_data = NULL;
	  screenshot_destroy (data);
	  return FALSE; /* remove */
	}

      break;

    case 3: /* dialog */
      build_dialog (data);

      /*
       * -1 or 1 if cancelled.
       */
      switch (gnome_dialog_run_and_close (GNOME_DIALOG (data->dialog)))
	{
	case 0: /* OK */
	  filename = g_strdup (gtk_entry_get_text (data->entry));

	  gtk_widget_destroy (GTK_WIDGET (data->dialog));
	  data->dialog = NULL;

	  if (filename)
	    if (!screenshot_save (data, filename))
	      {
		grab_data = NULL;
		screenshot_destroy (data);
		g_free (filename);
		return FALSE; /* remove */
	      }

	  g_free (filename);

	  break;

	case 2: /* Configure */
	  {
	    GtkWidget *properties;

	    grab_data = NULL;
	    screenshot_destroy (data);

	    properties = build_properties_dialog ();

	    open_properties_page (properties,
				  _("Plugins"), _("Screenshot"));

	    gnome_dialog_run (GNOME_DIALOG (properties));

	    return FALSE; /* remove */
	  }

	default: /* Cancel */
	  grab_data = NULL;
	  screenshot_destroy (data);
	  return FALSE; /* remove */
	}

      break;

    case 4: /* monitoring */
      if (data->status_window)
	{
	  GtkWidget *progressbar;
	  gfloat progress = 100 * data->lines
	    / (gfloat) data->format.height;

	  progressbar = gtk_object_get_data
	    (GTK_OBJECT (data->status_window), "progressbar");
	  gtk_progress_set_value (GTK_PROGRESS (progressbar), progress);
	}

      break;

    case 5: /* thread done */
      pthread_join (data->saving_thread, &result);
      num_threads--;

      if (data->error)
	ShowBox(data->error, GNOME_MESSAGE_BOX_ERROR);

      /* fall through */

    default:
      screenshot_destroy (data);
      return FALSE; /* remove the timer */
    }

  return TRUE; /* resume */
}

/* XXX ENDIANESS */

/* Takes a RGB565 line and saves it as RGB24 */
static gchar *
Convert_RGB565_RGB24 (gint width, gchar* src, gchar* dest)
{
  gint16 * line = (gint16*) src;
  gchar * where_have_we_written = dest;
  gint i;

  for (i = 0; i < width; i++)
    {
      gint word, val;

      word = *line; /* gggrrrrr bbbbbggg */
      val = word & 0x001f;
      dest[0] = (val << 3) + (val >> 2);
      val = word & 0x07E0;
      dest[1] = (val >> 3) + (val >> 9);
      word &= 0xF800;
      dest[2] = (word >> 8) + (word >> 13);
      dest += 3;
      line ++;
    }
  return where_have_we_written;
}

/* Takes a RGB565 line and saves it as RGB24 */
static gchar *
Convert_BGR565_RGB24 (gint width, gchar* src, gchar* dest)
{
  gint16 * line = (gint16*) src;
  gchar * where_have_we_written = dest;
  int i;

  for (i = 0; i < width; i++)
    {
      gint word, val;

      word = *line; /* gggbbbbb rrrrrggg */
      val = word & 0x001f;
      dest[2] = (val << 3) + (val >> 2);
      val = word & 0x07E0;
      dest[1] = (val >> 3) + (val >> 9);
      word &= 0xF800;
      dest[0] = (word >> 8) + (word >> 13);
      dest += 3;
      line ++;
    }
  return where_have_we_written;
}

/* Takes a RGB555 line and saves it as RGB24 */
static gchar *
Convert_RGB555_RGB24 (gint width, gchar* src, gchar* dest)
{
  gint16 * line = (gint16*) src;
  gchar * where_have_we_written = dest;
  int i;

  for (i = 0; i < width; i++)
    {
      gint word, val;

      word = *line; /* gggrrrrr abbbbbgg */
      val = word & 0x001f;
      dest[0] = (val << 3) + (val >> 2);
      val = word & 0x03e0;
      dest[1] = (val >> 2) + (val >> 7);
      word &= 0x7C00;
      dest[2] = (word >> 7) + (word >> 12);
      dest += 3;
      line ++;
    }
  return where_have_we_written;
}

/* Takes a BGR555 line and saves it as RGB24 */
static gchar *
Convert_BGR555_RGB24 (gint width, gchar* src, gchar* dest)
{
  gint16 * line = (gint16*) src;
  gchar * where_have_we_written = dest;
  int i;

  for (i = 0; i < width; i++)
    {
      gint word, val;

      word = *line; /* gggbbbbb arrrrrgg */
      val = word & 0x001f;
      dest[2] = (val << 3) + (val >> 2);
      val = word & 0x03e0;
      dest[1] = (val >> 2) + (val >> 7);
      word &= 0x7C00;
      dest[0] = (word >> 7) + (word >> 12);
      dest += 3;
      line ++;
    }
  return where_have_we_written;
}

/* Removes the last byte (supposedly alpha channel info) from the
   image */
static gchar *
Convert_RGBA_RGB24 (gint width, gchar* src, gchar* dest)
{
  gchar * where_have_we_written = dest;
  int i;

  for (i = 0; i < width; i++)
    {
      dest[0] = src[0];
      dest[1] = src[1];
      dest[2] = src[2];
      dest += 3;
      src += 4;
    }
  return where_have_we_written;
}

/* Removes the last byte (suppoosedly alpha channel info) from the
   image */
static gchar *
Convert_BGRA_RGB24 (gint width, gchar* src, gchar* dest)
{
  gchar * where_have_we_written = dest;
  int i;

  for (i = 0; i < width; i++)
    {
      dest[0] = src[2];
      dest[1] = src[1];
      dest[2] = src[0];
      dest += 3;
      src += 4;
    }
  return where_have_we_written;
}

/* Swaps the R and the B components of a RGB24 image */
static gchar *
Convert_BGR24_RGB24 (gint width, gchar* src, gchar* dest)
{
  gchar * where_have_we_written = dest;
  gint i;

  for (i = 0; i < width; i++)
    {
      dest[0] = src[2];
      dest[1] = src[1];
      dest[2] = src[0];
      dest += 3;
      src += 3;
    }
  return where_have_we_written;
}

static inline int
clamp (double n)
{
  int r = n;

  if (r > 255)
    return 255;
  else if (r < 0)
    return 0;
  else
    return r;
}

/* Converts a YUYV line into a RGB24 one */
static gchar *
Convert_YUYV_RGB24 (gint w, guchar *src, guchar *dest)
{
  gchar *where_have_we_written = dest;
  gint i;
  double y1, y2, u, v, uv;

  for (i=0; i<w; i+=2)
    {
      y1 = ((src[0] - 16) * 255) * (1 / 219.0);
      u  =  (src[1] - 128) * 127;
      y2 = ((src[2] - 16) * 255) * (1 / 219.0);
      v  =  (src[3] - 128) * 127;

      uv = (- 0.344 / 112.0) * u - (0.714 / 112.0) * v;
      v *= 1.402 / 112.0;
      u *= 1.772 / 112.0;

      dest[0] = clamp(y1 + v);
      dest[1] = clamp(y1 + uv);
      dest[2] = clamp(y1 + u);

      dest[3] = clamp(y2 + v);
      dest[4] = clamp(y2 + uv);
      dest[5] = clamp(y2 + u);

      src += 4;
      dest += 6;
    }
  return where_have_we_written;
}

static gchar *
Convert_RGB24_RGB24 (gint width, gchar* src, gchar* dest)
{
  memcpy (dest, src, width * 3);
  return dest;
}

#define SWAP(a, b) \
  do { __typeof__ (a) _t = (b); (b) = (a); (a) = _t; } while (0)

static gboolean
copy_image (screenshot_data *data,
	    gpointer image, struct tveng_frame_format *format)
{
  if (format->pixformat == TVENG_PIX_YVU420 ||
      format->pixformat == TVENG_PIX_YUV420)
    {
      gint yuv_bpp = (x11_get_bpp () + 7) >> 3;
      gint yuv_bytesperline = format->width * yuv_bpp;
      uint8_t *y, *u, *v;

      data->data = g_malloc (yuv_bytesperline * format->height);

      y = (uint8_t *) image;
      u = y + (format->width * format->height);
      v = u + ((format->width * format->height) >> 2);

      if (format->pixformat == TVENG_PIX_YVU420)
	SWAP (u, v);

      yuv2rgb (data->data, y, u, v, format->width, format->height,
	       yuv_bytesperline, format->width, format->width * 0.5);

      memcpy (&data->format, format, sizeof(data->format));

      data->format.pixformat =
	zmisc_resolve_pixformat (x11_get_bpp (), x11_get_byte_order ());
      data->format.bytesperline = yuv_bytesperline;
      data->format.depth = x11_get_bpp();
      /* unfortunate election of names :-( */
      data->format.bpp = yuv_bpp;
      data->format.sizeimage = data->format.bytesperline * format->height;
    }
  else
    {
      data->data = g_malloc (format->bytesperline * format->height);

      memcpy (&data->format, format, sizeof(data->format));

      memcpy (data->data, image, format->bytesperline *
	      format->height);
    }

  data->line_data = g_malloc (data->format.width * 3);

  switch (data->format.pixformat)
    {
    case TVENG_PIX_RGB32:
      data->Converter = (LineConverter) Convert_RGBA_RGB24;
      break;
    case TVENG_PIX_RGB24:
      data->Converter = (LineConverter) Convert_RGB24_RGB24;
      break;
    case TVENG_PIX_BGR32:
      data->Converter = (LineConverter) Convert_BGRA_RGB24;
      break;
    case TVENG_PIX_BGR24:
      data->Converter = (LineConverter) Convert_BGR24_RGB24;
      break;
    case TVENG_PIX_RGB565:
      data->Converter = (LineConverter) Convert_BGR565_RGB24;
      break;
    case TVENG_PIX_RGB555:
      data->Converter = (LineConverter) Convert_BGR555_RGB24;
      break;
    case TVENG_PIX_YUYV:
      data->Converter = (LineConverter) Convert_YUYV_RGB24;
      break;
    default:
      ShowBox("The current pixformat isn't supported",
	      GNOME_MESSAGE_BOX_ERROR);
      return FALSE;
    }

  return TRUE;
}

static void
plugin_read_bundle (capture_bundle *bundle)
{
  if (grab_data && grab_countdown > 0)
    if (grab_countdown-- == 1)
      {
	if (copy_image (grab_data, bundle->data, &bundle->format))
	  grab_data->status += 2; /* data ready */
	else
	  grab_data->status = -1; /* timeout abort yourself */
      }
}

static gboolean
screenshot_grab (gint dialog)
{
  GdkPixbuf *pixbuf;
  screenshot_data *data;

  if (grab_data)
    return FALSE; /* request pending */

  data = screenshot_new ();

  grab_countdown = 0;
  grab_data = data;

  /*
   *  Switch to capture mode if we aren't viewing Teletext
   *  (associated with TVENG_NO_CAPTURE)
   */
  if (zapping_info->current_mode != TVENG_NO_CAPTURE)
    {
      zmisc_switch_mode (TVENG_CAPTURE_READ, zapping_info);

      if (zapping_info->current_mode != TVENG_CAPTURE_READ)
	{
	  screenshot_destroy (data);
	  return FALSE; /* unable to set the mode */
	}
    }
  /*
   *  Otherwise request TTX image 
   *  XXX Option: include subtitles
   */
  else if ((pixbuf =
	    ttxview_get_scaled_ttx_page (GTK_WIDGET (z_main_window ()))))
    {
      struct tveng_frame_format format;

      format.width = gdk_pixbuf_get_width (pixbuf);
      format.height = gdk_pixbuf_get_height (pixbuf);
      format.bpp = 4;
      format.depth = 32;
      format.pixformat = TVENG_PIX_RGB32;
      format.bytesperline = gdk_pixbuf_get_rowstride (pixbuf);

      if (!copy_image (data, gdk_pixbuf_get_pixels (pixbuf), &format))
	{
	  screenshot_destroy (data);
	  return FALSE;
	}

      data->status = dialog + 2; /* image data is ready */

      g_timeout_add (50, (GSourceFunc) screenshot_timeout, data);

      return TRUE;
    }
  else
    return FALSE;

  grab_countdown = 2; /* take 2nd image */

  data->status = dialog; /* wait for image data (event sub) */
  data->lines = 2000 / 50; /* abort after 2 sec */

  g_timeout_add (50, (GSourceFunc) screenshot_timeout, data);

  return TRUE;
}

static void
on_screenshot_button_clicked		(GtkButton       *button,
					 gpointer         user_data)
{
  screenshot_grab (1);
}

static void
on_screenshot_button_fast_clicked	(GtkButton       *button,
					 gpointer         user_data)
{
  screenshot_grab (0);
}
