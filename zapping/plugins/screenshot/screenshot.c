/*
 * Screenshot saving plugin for Zapping
 * Copyright (C) 2000, 2001 Iñaki García Etxebarria
 * Copyright (C) 2001, 2002 Michael H. Schimek
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

/* XXX gtk+ 2.3 GtkTimeout -> ?; GtkOptionMenu -> ? */
/* gdk_pixbuf_render_to_drawable */
#undef GTK_DISABLE_DEPRECATED
#undef GDK_DISABLE_DEPRECATED

#include "screenshot.h"
#include "src/zgconf.h"
#include "src/yuv2rgb.h"
#include "src/properties.h"
#include <gdk-pixbuf/gdk-pixbuf.h> /* previews */
#include <sys/stat.h>
#include <sys/types.h>


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

static tveng_device_info * zapping_info = NULL; /* Info about the
						   video device */

/*
 *  Options
 */
/* save_dir is no longer an option but still in conf,
   save_base remembers the file basename too. */
static gchar *screenshot_option_save_dir = NULL;
static gchar *screenshot_option_save_base = NULL;
static gchar *screenshot_option_command = NULL;
static gboolean screenshot_option_grab_on_ok;
static gint screenshot_option_skip;
static gboolean screenshot_option_toolbutton;
/* Dialog options */
static gchar *screenshot_option_format = NULL;
static gint screenshot_option_quality;
static gint screenshot_option_deint;


/* Properties handling code */
static void
properties_add			(GtkDialog	*dialog);


/*
 *  plugin_read_frame interface
 */
static screenshot_data *grab_data;
static int grab_countdown = 0;

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

static void
plugin_get_info (const gchar ** canonical_name,
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
 *
 *  tveng_ov511_get_button_state apparently can take some time, but
 *  plugin_start must be called from the main thread. Thus we have a
 *  thread reading the /proc entry and a 250 ms timeout that get notified
 *  (nothing fancy, just a gboolean) and calls plugin_start.
 */

static guint ogb_timeout_id = NO_SOURCE_ID;
static volatile gboolean ov511_clicked = FALSE;
static volatile gboolean ov511_poll_quit = FALSE;
static gboolean have_ov511_poll_thread = FALSE;
static pthread_t ov511_poll_thread_id;

static void *
ov511_poll_thread (void *unused _unused_)
{
  /* I know this while (!quit) isn't the best thing since sliced
     bread, but it's easy to do and will work just fine */
  while (!ov511_poll_quit)
    switch (tveng_ov511_get_button_state(zapping_info))
      {
      case 1:
	ov511_clicked = TRUE; /* clicked */
	break;
      case 0: /* not clicked since last get_button_state */
	break;
      default: /* sth has gone wrong, exit the thread */
	return NULL;
      }

  return NULL;
}

static gboolean plugin_start (void);

static gint
ov511_grab_button_timeout (guint *timeout_id)
{
  static gboolean first_run = TRUE;
  /* Startup. This has two uses. First we check that the ov511
     controller is loaded and if loaded we clear any previous
     "clicked" flag */
  if (first_run)
    {
      if (tveng_ov511_get_button_state(zapping_info) < 0)
	{
	  *timeout_id = NO_SOURCE_ID;
	  return FALSE;
	}
      have_ov511_poll_thread =
        (0 == pthread_create(&ov511_poll_thread_id, NULL,
	   ov511_poll_thread, NULL));
      first_run = FALSE;
    }

  /* only done afterwards */
  if (ov511_clicked)
    {
      ov511_clicked = FALSE;
      plugin_start();
    }

  return TRUE; /* Continue */
}

static gboolean screenshot_grab (gint dialog);

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

static PyObject*
py_screenshot	(PyObject *self _unused_, PyObject *args _unused_)
{
  char *_format = NULL;
  int ok = ParseTuple (args, "|s", &_format);

  if (!ok)
    g_error ("py_screenshot(|s)");

  if (_format)
    {
      struct screenshot_backend *backend = find_backend (_format);

      if (!backend)
	py_return_false;

      g_free (screenshot_option_format);
      screenshot_option_format = g_strdup (backend->keyword);
    }

  /* 1 = screenshot, 0 = quickshot */
  screenshot_grab (1);
  py_return_true;
}

static PyObject*
py_quickshot	(PyObject *self _unused_, PyObject *args _unused_)
{
  char *_format = NULL;
  int ok = ParseTuple (args, "|s", &_format);

  if (!ok)
    g_error ("py_quickshot(|s)");

  if (_format)
    {
      struct screenshot_backend *backend = find_backend (_format);

      if (!backend)
	py_return_false;

      g_free (screenshot_option_format);
      screenshot_option_format = g_strdup (backend->keyword);
    }

  /* 1 = screenshot, 0 = quickshot */
  screenshot_grab (0);
  py_return_true;
}

static gboolean
plugin_init ( PluginBridge bridge _unused_, tveng_device_info * info )
{
  /* Register the plugin as interested in the properties dialog */
  property_handler screenshot_handler =
  {
    add: properties_add
  };

  D();

  append_property_handler(&screenshot_handler);

  ogb_timeout_id =
    g_timeout_add (100 /* ms */,
		   (GSourceFunc) ov511_grab_button_timeout,
		   &ogb_timeout_id);

  zapping_info = info;

  D();

  cmd_register ("screenshot", py_screenshot, METH_VARARGS,
		("Screenshot dialog"),	"zapping.screenshot()");
  cmd_register ("quickshot", py_quickshot, METH_VARARGS,
		("Screenshot"), "zapping.quickshot()",
		("PPM Screenshot"), "zapping.quickshot('ppm')",
		("JPEG Screenshot"), "zapping.quickshot('jpeg')");

  return TRUE;
}

static void
plugin_close(void)
{
  screenshot_close_everything = TRUE;

  if (ogb_timeout_id != NO_SOURCE_ID)
    {
      g_source_remove (ogb_timeout_id);
      ogb_timeout_id = NO_SOURCE_ID;
    }

  if (have_ov511_poll_thread)
    {
      ov511_poll_quit = TRUE;
      pthread_join(ov511_poll_thread_id, NULL);
      have_ov511_poll_thread = FALSE;
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
  screenshot_grab (0);
  return TRUE;
}

#define LOAD_CONFIG(_type, _def, _name, _descr)				\
  buffer = g_strconcat (root_key, #_name, NULL);			\
  zconf_create_##_type (_def, _descr, buffer);				\
  zconf_get_##_type (&screenshot_option_##_name, buffer);		\
  g_free (buffer);

static void
plugin_load_config (gchar *root_key)
{
  gchar *buffer;
  gchar *default_save_dir;

  D();

  default_save_dir = g_strconcat (g_get_home_dir (), "/shots", NULL);
  LOAD_CONFIG (string, default_save_dir, save_dir, 
	       "The directory where screenshot will be written to");
  g_free (default_save_dir);

  LOAD_CONFIG (string, "shot", save_base, 
	       "Default filename of screenshots");

  D();

  LOAD_CONFIG (string, "", command, "Command to run after taking the screenshot");
  if (!screenshot_option_command)
    screenshot_option_command = g_strdup ("");

  LOAD_CONFIG (boolean, FALSE, grab_on_ok, "Grab on clicking OK");
  LOAD_CONFIG (int, 0, skip, "Skip pictures before grabbing");

  LOAD_CONFIG (string, "jpeg", format, "File format");

  D();

  LOAD_CONFIG (int, 75, quality, "Quality of the compressed image");
  LOAD_CONFIG (int, 0, deint, "Deinterlace mode");

  LOAD_CONFIG (boolean, TRUE, toolbutton, "Add toolbar button");
}

#define SAVE_CONFIG(_type, _name)					\
  buffer = g_strconcat (root_key, #_name, NULL);			\
  zconf_set_##_type (screenshot_option_##_name, buffer);		\
  g_free (buffer);

static void
plugin_save_config (gchar * root_key)
{
  gchar *buffer;

  SAVE_CONFIG (string, save_dir);
  g_free(screenshot_option_save_dir);
  screenshot_option_save_dir = NULL;

  SAVE_CONFIG (string, save_base);
  g_free(screenshot_option_save_base);
  screenshot_option_save_base = NULL;

  SAVE_CONFIG (string, command);
  g_free (screenshot_option_command);
  screenshot_option_command = NULL;

  SAVE_CONFIG (boolean, grab_on_ok);
  SAVE_CONFIG (int, skip);

  SAVE_CONFIG (string, format);
  g_free (screenshot_option_format);
  screenshot_option_format = NULL;

  SAVE_CONFIG (int, quality);
  SAVE_CONFIG (int, deint);

  SAVE_CONFIG (boolean, toolbutton);
}

static gboolean
plugin_get_public_info (gint index, gpointer * ptr, const gchar **
			symbol, const gchar ** description, const gchar **
			type, gint * hash)
{
  /* Export the conversion functions */
  struct plugin_exported_symbol symbols[] =
  {
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

/* Preferences */

#define SET_BOOL(_name)							\
  w = lookup_widget (page, "screenshot_" #_name);			\
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w),			\
				screenshot_option_##_name);

static void
screenshot_setup		(GtkWidget	*page)
{
  GtkWidget *w;
  gboolean full_size;

  w = lookup_widget (page, "screenshot_command");
  gtk_entry_set_text (GTK_ENTRY (w), screenshot_option_command);

  full_size = FALSE;
  z_gconf_get_bool (&full_size, "/apps/zapping/plugins/screenshot/full_size");
  w = lookup_widget (page, "screenshot_full_size");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), full_size);

  SET_BOOL (grab_on_ok);

  w = lookup_widget (page, "screenshot_skip");
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (w), screenshot_option_skip);

  SET_BOOL (toolbutton);
}

#define GET_BOOL(_name)							\
  w = lookup_widget (page, "screenshot_" #_name);			\
  screenshot_option_##_name =						\
    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w));

static void plugin_add_gui (GnomeApp *);

static void
screenshot_apply		(GtkWidget	*page)
{
  GtkWidget *w;

  w = lookup_widget (page, "screenshot_command");
  g_free (screenshot_option_command);
  screenshot_option_command =
    g_strdup (gtk_entry_get_text (GTK_ENTRY (w)));

  w = lookup_widget (page, "screenshot_full_size");
  z_gconf_set_bool ("/apps/zapping/plugins/screenshot/full_size",
		    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)));

  GET_BOOL (grab_on_ok);

  w = lookup_widget (page, "screenshot_skip");
  screenshot_option_skip =
    gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (w));

  GET_BOOL (toolbutton);

  plugin_add_gui (&zapping->app);
}

static void
properties_add			(GtkDialog	*dialog)
{
  SidebarEntry plugin_options[] = {
    { N_("Screenshot"), "gnome-digital-camera.png", "screenshot_prefs",
      screenshot_setup, screenshot_apply,
      .help_link_id = "zapping-settings-screenshot" }
  };
  SidebarGroup groups[] = {
    { N_("Plugins"), plugin_options, G_N_ELEMENTS (plugin_options) }
  };

  standard_properties_add(dialog, groups, G_N_ELEMENTS (groups),
			  "screenshot.glade2");
}

static
void plugin_add_gui (GnomeApp * app _unused_)
{
  GtkToolItem *tool_item;
  gpointer p;

  p = g_object_get_data (G_OBJECT (zapping), "screenshot_button");
  if (p)
    {
      tool_item = GTK_TOOL_ITEM (p);
    }
  else
    {
      tool_item = gtk_tool_button_new (NULL, _("Screenshot"));

      gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON (tool_item),
				    "zapping-screenshot");

      z_tooltip_set (GTK_WIDGET (tool_item), _("Take a screenshot"));

      z_signal_connect_python (G_OBJECT (tool_item), "clicked",
			       "zapping.screenshot()");

      gtk_toolbar_insert (zapping->toolbar, tool_item, APPEND);

      /* Set up the widget so we can find it later */
      g_object_set_data (G_OBJECT (zapping), "screenshot_button", tool_item);
    }

  if (screenshot_option_toolbutton)
    gtk_widget_show (GTK_WIDGET (tool_item));
  else
    gtk_widget_hide (GTK_WIDGET (tool_item));
}

static void
plugin_remove_gui (GnomeApp * app _unused_)
{
  GtkWidget *button;

  button = GTK_WIDGET (g_object_get_data (G_OBJECT (zapping),
					  "screenshot_button"));
  gtk_container_remove (GTK_CONTAINER (zapping->toolbar), button);
}

static struct plugin_misc_info *
plugin_get_misc_info (void)
{
  static struct plugin_misc_info returned_struct =
  {
    sizeof(struct plugin_misc_info), /* size of this struct */
    -10, /* plugin priority, we must be executed with a fully
	    processed image */
    /* Category */
    PLUGIN_CATEGORY_VIDEO_OUT |
    PLUGIN_CATEGORY_FILTER |
    PLUGIN_CATEGORY_GUI
  };

  /*
    Tell that the template plugin should be run with a somewhat high
    priority (just to put an example)
  */
  return (&returned_struct);
}

/*
 *  And here starts the real work... :-)
 */

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

  g_free (data->command);

  if (data->status_window)
    gtk_widget_destroy (data->status_window);

  g_free (data->io_buffer);

  data->io_buffer_size = 0;
  data->io_buffer_used = 0;

  data->io_flush = NULL;

  if (data->io_fp)
    fclose (data->io_fp);

  g_free (data->error);
  g_free (data->deint_data);

  free (data->data);
  g_free (data->auto_filename);

  if (data->pixbuf)
    g_object_unref (G_OBJECT (data->pixbuf));

  if (data->dialog)
    gtk_widget_destroy (data->dialog);

  g_free (data);
}

static screenshot_data *
screenshot_new (void)
{
    gint i;
    guint private_max = 0;

    for (i = 0; backends[i]; i++)
      if (backends[i]->sizeof_private > private_max)
        private_max = backends[i]->sizeof_private;

    return (screenshot_data *)
      g_malloc0 (sizeof (screenshot_data) + private_max);
}

static gboolean
io_buffer_init (screenshot_data *data, guint size)
{
  data->io_buffer = g_malloc (size);

  if (!data->io_buffer)
    return FALSE;

  data->io_buffer_size = size;
  data->io_buffer_used = 0;

  return TRUE;
}

static gboolean
io_flush_stdio (screenshot_data *data, guint size)
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
io_flush_memory (screenshot_data *data, guint size)
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
  tveng_tuned_channel *tc;
  char *argv[10];
  int argc = 0;
  char *env[10];
  int envc = 0;

  /* Invoke through sh */
  argv[argc++] = g_strdup ("sh");
  argv[argc++] = g_strdup ("-c");
  argv[argc++] = g_strdup (data->command);

  /* FIXME */
  env[envc++] = g_strdup_printf ("SCREENSHOT_PATH=%s", data->filename);

  /* XXX thread safe? prolly not */
  tc = tveng_tuned_channel_nth (global_channel_list,
				(guint) cur_tuned_channel);

  if (tc)
    {
      env[envc++] = g_strdup_printf ("CHANNEL_ALIAS=%s", tc->name);
      env[envc++] = g_strdup_printf ("CHANNEL_ID=%s", tc->rf_name);
      if (tv_cur_video_standard (zapping_info))
	env[envc++] =
	  g_strdup_printf ("CURRENT_STANDARD=%s",
	    tv_cur_video_standard (zapping_info)->label);
      if (tv_cur_video_input (zapping_info))
	env[envc++] =
	  g_strdup_printf ("CURRENT_INPUT=%s",
	    tv_cur_video_input (zapping_info)->label);
    }

  gnome_execute_async_with_env (NULL, argc, argv, envc, env);

  while (envc-- > 0)
    g_free (env[envc]);

  while (argc-- > 0)
    g_free (argv[argc]);
}

static void *
screenshot_saving_thread (void *_data)
{
  screenshot_data *data = (screenshot_data *) _data;
  gchar *new_data;

  g_free (data->deint_data);
  data->deint_data = NULL;

  if (data->format.height == 480 || data->format.height == 576)
    if (screenshot_option_deint)
      if ((new_data = screenshot_deinterlace (data,
		      screenshot_option_deint - 1)))
	{
	  free (data->data);
	  data->data = new_data;
	}

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
    {
      execute_command (data);
    }

  data->io_fp = NULL;

  data->status = 8; /* thread done */

  return NULL;
}

static gboolean
on_progress_delete_event               (GtkWidget       *widget _unused_,
					GdkEvent        *event _unused_,
					screenshot_data *data)
{
  data->thread_abort = TRUE; /* thread shall abort */
  data->status_window = NULL; /* will be destroyed */

  return FALSE; /* destroy window */
}

static GtkWidget *
create_status_window (screenshot_data *data)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *progressbar;

  label = gtk_label_new (data->filename);
  gtk_widget_show (label);

  progressbar = gtk_progress_bar_new ();
  gtk_widget_show (progressbar);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start_defaults (GTK_BOX (vbox), label);
  gtk_box_pack_start_defaults (GTK_BOX (vbox), progressbar);
  gtk_widget_show (vbox);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_add (GTK_CONTAINER (window), vbox);
  gtk_window_set_title (GTK_WINDOW (window), _("Saving..."));
  gtk_window_set_modal (GTK_WINDOW (window), FALSE);
  g_object_set_data (G_OBJECT (window), "progressbar", progressbar);
  g_signal_connect (G_OBJECT (window), "delete-event",
		      (GtkSignalFunc) on_progress_delete_event, data);
  gtk_widget_show (window);

  return window;
}

static gboolean
screenshot_save			(screenshot_data *	data)
{
  gchar *dir_name;
  gchar *base_name;

  dir_name = g_path_get_dirname (data->filename);
  base_name = g_path_get_basename (data->filename);

  /* XXX zapping real parent? */
  if (!z_build_path_with_alert (GTK_WINDOW (zapping), dir_name))
    goto failure;

  if (!(data->io_fp = fopen (data->filename, "wb")))
    {
      gchar *window_title;

      window_title = g_strdup_printf (_("Sorry, but I cannot write %s\n%s"),
				      data->filename, strerror (errno));
      ShowBox ("%s", GTK_MESSAGE_ERROR, window_title);
      g_free (window_title);

      goto failure;
    }

  if (!data->io_buffer)
    if (!io_buffer_init (data, 1 << 16))
      goto failure;

  data->io_flush = io_flush_stdio;

  if (!data->backend->init (data, /* write */ TRUE,
			    screenshot_option_quality))
    goto failure;

  data->status_window = create_status_window (data);

  if (screenshot_option_command && screenshot_option_command[0])
    {
      /* may change while saving */
      data->command = g_strdup (screenshot_option_command);
    }

  data->thread_abort = FALSE;

  data->lines = 0;

  switch (pthread_create (&data->saving_thread, NULL,
			  screenshot_saving_thread, data))
    {
    case ENOMEM:
      ShowBox (_("Sorry, not enough resources to create a new thread"), 
	       GTK_MESSAGE_ERROR);
      goto failure;

    case EAGAIN:
      ShowBox (_("There are too many threads"),
	       GTK_MESSAGE_ERROR);
      goto failure;

    case 0:
      num_threads++;
      grab_data = NULL; /* permit new request */

      /* Funny bug: With small images encoding might finish before
	 pthread_create returns, thus we must check that we aren't
	 overwriting the finished status */
      if (data->status != 8)
	data->status = 7; /* monitoring */

      break;

    default:
      goto failure;
    }

  g_free (screenshot_option_save_dir);
  screenshot_option_save_dir = dir_name;

  g_free (screenshot_option_save_base);
  screenshot_option_save_base = base_name;

  return TRUE;

 failure:
  g_free (base_name);
  g_free (dir_name);

  return FALSE;
}

/*
 *  Screenshot dialog
 */

/* Hardcoded */
#define PREVIEW_WIDTH 192
#define PREVIEW_HEIGHT 144

static void
preview (screenshot_data *data)
{
  void *old_data;
  tv_image_format old_format;
  const tv_pixel_format *pf;
  unsigned int h_offset;
  unsigned int v_offset;

  if (!data || !data->drawingarea || !data->pixbuf)
    return;

  old_data = data->data;
  old_format = data->format;

  pf = data->format.pixel_format;

  h_offset = (((data->format.width - PREVIEW_WIDTH) >> 1)
	      * pf->bits_per_pixel) >> 3;
  v_offset = (((data->format.height - PREVIEW_HEIGHT) >> 1)
	      & (unsigned int) -1); /* top field first */

  data->data = (char *) data->data
    + h_offset + v_offset * old_format.bytes_per_line[0];

  data->format.width = PREVIEW_WIDTH;
  data->format.height = PREVIEW_HEIGHT;

  if ((!!screenshot_option_deint) != (!!data->deint_data))
    {
      if (data->deint_data)
	g_free (data->deint_data);

      if (screenshot_option_deint)
	data->deint_data = screenshot_deinterlace (data,
			     screenshot_option_deint - 1);
      else
	data->deint_data = NULL;
    }

  if (screenshot_option_deint && data->deint_data)
    {
      data->data = data->deint_data;
      data->format.bytes_per_line[0] = data->format.width * 3;
    }

  if (data->backend->load)
    {
      if (!data->io_buffer)
	if (!io_buffer_init (data, PREVIEW_WIDTH * PREVIEW_HEIGHT * 4))
	  {
	    //	    printf ("a\n");
	    goto restore;
	  }

      data->io_flush = io_flush_memory;
      data->io_buffer_used = 0;

      if (!data->backend->init (data, /* write */ TRUE,
				screenshot_option_quality))
	{
	  //	  printf ("b\n");
	  goto restore;
	}

      data->backend->save (data);

      if (data->thread_abort)
	{
	  //	  printf ("c\n");
	  goto restore;
	}

      data->size_est = data->io_buffer_used
	* (double)(old_format.width * old_format.height)
	/ (double)(PREVIEW_WIDTH * PREVIEW_HEIGHT);

      if (!data->backend->init (data, /* write */ FALSE, 0))
	{
	  //	  printf ("d\n");
	  goto restore;
	}

      data->backend->load (data,
			   gdk_pixbuf_get_pixels (data->pixbuf),
			   gdk_pixbuf_get_rowstride (data->pixbuf));
    }
  else /* backend doesn't support preview (lossless format?) */
    {
      guint line, rowstride;
      gchar *s, *d;

      s = data->data;
      d = gdk_pixbuf_get_pixels (data->pixbuf);
      rowstride = gdk_pixbuf_get_rowstride (data->pixbuf);

      for (line = 0; line < data->format.height; line++)
	{
	  memcpy (d, s, data->format.width * 3);
	  s += data->format.bytes_per_line[0];
	  d += rowstride;
	}

      data->size_est = data->backend->bpp_est
	* (double)(old_format.width * old_format.height);
    }

  gtk_widget_set_size_request (data->drawingarea, PREVIEW_WIDTH,
			       PREVIEW_HEIGHT);

 restore:
  data->format = old_format;
  data->data = old_data;
}

static gboolean
on_drawingarea_expose_event             (GtkWidget      *widget _unused_,
                                         GdkEventExpose *event _unused_,
                                         screenshot_data *data)
{
  gchar buf[80];

  if (data->drawingarea && data->pixbuf)
    gdk_pixbuf_render_to_drawable (data->pixbuf,
				   data->drawingarea->window,
				   data->drawingarea->style->white_gc,
				   0, 0, 0, 0,
				   PREVIEW_WIDTH, PREVIEW_HEIGHT,
				   GDK_RGB_DITHER_NORMAL, 0, 0);

  if (data->size_label)
    {
      if (data->size_est < (double)(1 << 20))
	snprintf (buf, sizeof(buf) - 1, _("appx %u KB"),
		  (unsigned int)(data->size_est / (1 << 10)));
      else
	snprintf (buf, sizeof(buf) - 1, _("appx %.2f MB"),
		  data->size_est / (double)(1 << 20));

      gtk_label_set_text (GTK_LABEL (data->size_label), buf);
    }

  return FALSE;
}

static gboolean
on_deint_changed                      (GtkWidget *widget,
				       screenshot_data *data)
{
  gint new_deint = z_object_get_int_data (G_OBJECT (widget), "deint");

  if (screenshot_option_deint == new_deint)
    return FALSE;

  screenshot_option_deint = new_deint;

  g_free (data->deint_data);
  data->deint_data = NULL;

  preview (data);
  on_drawingarea_expose_event (NULL, NULL, data);

  return FALSE;
}

static gboolean
on_quality_changed                      (GtkWidget      *widget,
                                         screenshot_data *data)
{
  gint new_quality = GTK_ADJUSTMENT (widget)->value;  

  if (screenshot_option_quality == new_quality)
    return FALSE;

  screenshot_option_quality = new_quality;

  preview (data);
  on_drawingarea_expose_event (NULL, NULL, data);

  return FALSE;
}                                                

static void
on_format_changed                     (GtkWidget *menu,
				       screenshot_data *data)
{
  GtkWidget *menu_item = gtk_menu_get_active (GTK_MENU (menu));
  gchar *keyword;

  keyword = g_object_get_data (G_OBJECT (menu_item), "keyword");

  data->backend = find_backend (keyword);

  g_assert (data->backend);

  g_free (screenshot_option_format);
  screenshot_option_format = g_strdup (data->backend->keyword);

  z_set_sensitive_with_tooltip (data->quality_slider,
				data->backend->quality,
				NULL,
				_("This format has no quality option"));

  z_electric_replace_extension (GTK_WIDGET (data->entry),
				data->backend->extension);

  preview (data);
  on_drawingarea_expose_event (NULL, NULL, data);
}

enum {
  SCREENSHOT_CONFIGURE = 666 /* Must match the value in the .glade2
				file */
};

static gchar *
default_filename		(const screenshot_data *data)
{
  if (NULL == screenshot_option_save_base
      || 0 == screenshot_option_save_base[0])
    {
      g_free (screenshot_option_save_base);
      screenshot_option_save_base = g_strdup ("shot");
    }

  return find_unused_name (screenshot_option_save_dir,
			   screenshot_option_save_base,
			   data->backend->extension);
}

static void
build_dialog (screenshot_data *data)
{
  GtkWidget *widget;
  GtkWidget *menu, *menu_item;
  GtkAdjustment *adj;
  guint default_item = 0;
  guint i;

  data->dialog = build_widget ("dialog1", "screenshot.glade2");
  /* Format menu */

  widget = lookup_widget (data->dialog, "optionmenu1");

  if ((menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (widget))))
    gtk_widget_destroy (menu);

  menu = gtk_menu_new ();

  g_assert (backends[0] != NULL);

  for (i = 0; backends[i]; i++)
    {
      menu_item = gtk_menu_item_new_with_label (_(backends[i]->label));
      z_object_set_const_data (G_OBJECT (menu_item), "keyword",
			       backends[i]->keyword);
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

      if (strcmp (screenshot_option_format, backends[i]->keyword) == 0)
	default_item = i;
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (widget), default_item);
  g_signal_connect (G_OBJECT (GTK_OPTION_MENU (widget)->menu),
		    "deactivate", G_CALLBACK (on_format_changed), data);

  data->backend = backends[default_item];

  {
    gchar *filename;

    /* File entry */

    data->entry = GTK_ENTRY (lookup_widget (data->dialog, "entry"));

    z_entry_emits_response (GTK_WIDGET (data->entry),
			    GTK_DIALOG (data->dialog),
			    GTK_RESPONSE_OK);
    gtk_dialog_set_default_response (GTK_DIALOG (data->dialog),
				     GTK_RESPONSE_OK);

    filename = default_filename (data);

    data->auto_filename = g_path_get_basename (filename);
    gtk_entry_set_text (data->entry, filename);

    g_free (filename);

    g_object_set_data (G_OBJECT (data->entry),
		       "basename", (gpointer) data->auto_filename);
    g_signal_connect (G_OBJECT (data->entry), "changed",
		      G_CALLBACK (z_on_electric_filename),
		      (gpointer) NULL);
    gtk_editable_select_region (GTK_EDITABLE (data->entry), 0, -1);
  }

  /* Preview */

  if (1
      && data->format.width >= PREVIEW_WIDTH
      && data->format.height >= PREVIEW_HEIGHT)
    {
      data->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				     /* has_alpha */ FALSE,
				     /* bits_per_sample */ 8,
				     PREVIEW_WIDTH, PREVIEW_HEIGHT);

      data->drawingarea = lookup_widget (data->dialog,
					 "drawingarea1");
      data->size_label = lookup_widget (data->dialog, "label7");
      gdk_window_set_back_pixmap (data->drawingarea->window,
				  NULL, FALSE);
      preview (data);
      g_signal_connect (G_OBJECT (data->drawingarea), "expose-event",
			  G_CALLBACK (on_drawingarea_expose_event),
			  data);
    }
  else
    {
      GtkWidget *drawingarea = lookup_widget (data->dialog,
					      "drawingarea1");
      GtkWidget *size_label = lookup_widget (data->dialog, "label7");

      gtk_widget_destroy (drawingarea);
      gtk_widget_destroy (size_label);

      data->pixbuf = NULL;
      data->drawingarea = NULL;
      data->size_label = NULL;
    }

  /* Quality slider */

  data->quality_slider = lookup_widget (data->dialog, "hscale1");
  adj = gtk_range_get_adjustment (GTK_RANGE (data->quality_slider));
  gtk_adjustment_set_value (GTK_ADJUSTMENT (adj),
			    screenshot_option_quality);
  g_signal_connect (G_OBJECT (adj), "value-changed",
		      G_CALLBACK (on_quality_changed), data);

  z_set_sensitive_with_tooltip (data->quality_slider,
				data->backend->quality,
				NULL,
				_("This format has no quality option"));

  gtk_window_set_transient_for (GTK_WINDOW (data->dialog),
				GTK_WINDOW (zapping));

  gtk_widget_grab_focus (GTK_WIDGET (data->entry));

  /* Deinterlace */

  if (data->format.height == 480 || data->format.height == 576)
    {
      widget = lookup_widget (data->dialog, "radiobutton4");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
				    (screenshot_option_deint == 0));
      g_signal_connect (G_OBJECT (widget), "pressed",
			  G_CALLBACK (on_deint_changed), data);

      widget = lookup_widget (data->dialog, "radiobutton2");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
				    (screenshot_option_deint == 1));
      g_object_set_data (G_OBJECT (widget), "deint", (gpointer) 1);
      g_signal_connect (G_OBJECT (widget), "pressed",
			  G_CALLBACK (on_deint_changed), data);

      widget = lookup_widget (data->dialog, "radiobutton3");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
				    (screenshot_option_deint == 2));
      g_object_set_data (G_OBJECT (widget), "deint", (gpointer) 2);
      g_signal_connect (G_OBJECT (widget), "pressed",
			  G_CALLBACK (on_deint_changed), data);
    }
  else
    {
      widget = lookup_widget (data->dialog, "hbox2");

      z_set_sensitive_with_tooltip (widget, FALSE, NULL,
        _("Only useful with full size, unscaled picture (480 or 576 lines)"));
    }
}

static gint format_request = -1;

static void unrequest (void)
{
  if (-1 != format_request)
    {
      release_capture_format (format_request);
      format_request = -1;
    }
}

/*
 *  This timer callback controls the progress of image saving.
 *  I [mhs] promise to never ever write something ugly like this
 *  again in my whole life, but for now I see no easy way around.
 */
static gboolean
screenshot_timeout (screenshot_data *data)
{
  const gchar *filename;
  gpointer result;

  switch (data->status)
    {
    case 0:
    case 1:
    case 4: /* waiting for image */
      if (data->lines-- <= 0)
	{
	  unrequest ();
	  grab_data = NULL;
	  screenshot_destroy (data);
	  return FALSE; /* remove */
	}

      break;

    case 2: /* quick start */
      data->backend = find_backend (screenshot_option_format);
      data->filename = default_filename (data);

      if (!screenshot_save (data))
	{
	  unrequest ();
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
      switch (gtk_dialog_run (GTK_DIALOG (data->dialog)))
	{
	case GTK_RESPONSE_OK: /* OK */
	  filename = gtk_entry_get_text (data->entry);

	  if (filename)
	    {
	      data->filename = g_strdup (filename);
	      gtk_widget_destroy (GTK_WIDGET (data->dialog));
	      data->dialog = NULL;

	      if (screenshot_option_grab_on_ok)
		{
		  data->status = 4;
		  data->lines = 2000 / 50; /* abort after 2 sec */
		  grab_countdown = screenshot_option_skip + 1;
		}
	      else if (!screenshot_save (data))
		{
		  unrequest ();
		  grab_data = NULL;
		  screenshot_destroy (data);
		  return FALSE; /* remove */
		}

	      break;
	    }

	  /* fall through */

	default: /* Cancel */
	  unrequest ();
	  grab_data = NULL;
	  screenshot_destroy (data);
	  return FALSE; /* remove */

	case SCREENSHOT_CONFIGURE: /* Configure */
	  {
	    unrequest ();
	    grab_data = NULL;
	    screenshot_destroy (data);

	    python_command_printf (/* GtkWidget */ NULL,
				   "zapping.properties('%s', '%s')",
				   _("Plugins"), _("Screenshot"));

	    return FALSE; /* remove */
	  }
	}

      break;

    case 6: /* post-dialog data ready */
      if (!screenshot_save (data))
	{
	  unrequest ();
	  grab_data = NULL;
	  screenshot_destroy (data);
	  return FALSE; /* remove */
	}

      break;

    case 7: /* monitoring */
      if (data->status_window)
	{
	  GtkWidget *progressbar;
	  gfloat progress = data->lines / (gfloat) data->format.height;

	  progressbar = g_object_get_data
	    (G_OBJECT (data->status_window), "progressbar");
	  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progressbar),
					 progress);
	}

      break;

    case 8: /* thread done */
      pthread_join (data->saving_thread, &result);
      num_threads--;

      if (data->error)
	ShowBox("%s", GTK_MESSAGE_ERROR, data->error);

      /* fall through */

    default:
      unrequest ();
  
      if (grab_data == data)
	grab_data = NULL;
      screenshot_destroy (data);
      return FALSE; /* remove the timer */
    }

  return TRUE; /* resume */
}

static gboolean
copy_image (screenshot_data *data, capture_frame *frame)
{
  zimage *image = retrieve_frame (frame, TV_PIXFMT_RGB24_LE, /* copy */ FALSE);

  if (!image)
    return FALSE;

  data->format = image->fmt;
  data->data = tv_new_image (image->img, &image->fmt);

  return (NULL != data->data);
}

static void
plugin_read_frame (capture_frame *frame)
{
  if (grab_data && grab_countdown > 0)
    if (grab_countdown-- == 1)
      {
	if (copy_image (grab_data, frame))
	  {
	    unrequest ();
	    grab_data->status += 2; /* data ready */
	  }
	else
	  {
	    grab_data->status = -1; /* timeout abort yourself */
	  }
      }
}

static gboolean
screenshot_grab (gint dialog)
{
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
  if (CAPTURE_MODE_NONE != tv_get_capture_mode (zapping_info))
    {
      guint width;
      guint height;
      gboolean full_size;

      if (CAPTURE_MODE_READ != tv_get_capture_mode (zapping_info))
	zmisc_switch_mode (DISPLAY_MODE_WINDOW,
			   CAPTURE_MODE_READ, zapping_info);

      if (tv_get_capture_mode (zapping_info) != CAPTURE_MODE_READ)
	{
	  screenshot_destroy (data);
	  return FALSE; /* unable to set the mode */
	}

      width = 0; /* any */
      height = 0;

      full_size = FALSE;
      z_gconf_get_bool (&full_size,
			"/apps/zapping/plugins/screenshot/full_size");
      if (full_size)
	{
	  const tv_video_standard *std;

	  if ((std = tv_cur_video_standard (zapping->info)))
	    {
	      width = std->frame_width;
	      height = std->frame_height;
	    }
	}

      format_request = request_capture_format
	(zapping_info, width, height,
	 TV_PIXFMT_SET (TV_PIXFMT_RGB24_LE),
	 /* flags */ 0);

      if (-1 == format_request && 0 != width)
	{
	  format_request = request_capture_format
	    (zapping_info, 0, 0,
	     TV_PIXFMT_SET (TV_PIXFMT_RGB24_LE),
	     /* flags */ 0);
	}

      if (-1 == format_request)
	{
	  /* FIXME: This and above we should restore whatever mode was
	     present before */
	  return FALSE;
	}
    }

#ifdef GNOME2_CONVERSION_COMPLETE
#ifdef HAVE_LIBZVBI
  /*
   *  Otherwise request TTX image 
   *  XXX Option: include subtitles
   */
  else if (teletext mode &&
	   (pixbuf =
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
#endif /* HAVE_LIBZVBI */
#endif /* conversion complete */
  else
    return FALSE;

  grab_countdown = screenshot_option_skip + 1;

  data->status = dialog; /* wait for image data (event sub) */
  data->lines = 2000 / 50; /* abort after 2 sec */

  g_timeout_add (50, (GSourceFunc) screenshot_timeout, data);

  return TRUE;
}

/* Return FALSE if we aren't able to access a symbol, you should only
   need to edit the pointer table, not the code */
gboolean
plugin_get_symbol(const gchar * name, gint hash, gpointer * ptr)
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
    SYMBOL(plugin_read_frame, 0x1234),
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

