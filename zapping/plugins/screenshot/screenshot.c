/* Screenshot saving plugin for Zapping
 * Copyright (C) 2000-2001 I�aki Garc�a Etxebarria
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
#include "yuv2rgb.h"
#include "ttxview.h"
#include "properties.h"
#ifdef HAVE_LIBJPEG
#include <pthread.h>
#include <gdk-pixbuf/gdk-pixbuf.h> /* previews */
/* avoid redefinition warning */
#undef HAVE_STDLIB_H
#undef HAVE_STDDEF_H
#include <jpeglib.h> /* jpeg compression */
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
N_("You can use this plugin to take screenshots of what you are"
" actually watching in TV.\nIt will save the screenshots in JPEG"
" format.");
static const gchar str_short_description[] = 
N_("This plugin takes screenshots of the capture.");
static const gchar str_author[] = "I�aki Garc�a Etxebarria";
/* The format of the version string must be
   %d[[.%d[.%d]][other_things]], where the things between [] aren't
   needed, and if not present, 0 will be assumed */
static const gchar str_version[] = "0.7";

/* Set to TRUE when plugin_close is called */
static gboolean close_everything = FALSE;

/* Number of threads currently running (so we can know when are all of
   them closed) */
static gint num_threads = 0;

/* Where should the screenshots be saved to (dir) */
static gchar * save_dir = NULL;

/* Command to run after saving the screenshot */
static gchar * command = NULL;

static gint quality; /* Quality of the compressed image */

static tveng_device_info * zapping_info = NULL; /* Info about the
						   video device */
static gchar *save_screenshot_file_name = NULL; /* file name to save
						   (tmp. loc.) */

/* Properties handling code */
static void
properties_add			(GnomeDialog	*dialog);

/*
  1 when the plugin should save the next frame
*/
static gint save_screenshot = 0; 

/* Callbacks */
static void
on_screenshot_button_clicked          (GtkButton       *button,
				       gpointer         user_data);
static void
on_screenshot_button_fast_clicked	(GtkButton       *button,
					 gpointer         user_data);

/* This one starts a new thread that saves the current screenshot */
static void
start_saving_screenshot (gpointer data_to_save,
			 const gchar *path,
			 struct tveng_frame_format * format);

/* This struct is shared between the main and the saving thread */
struct screenshot_data
{
  gpointer data; /* Pointer to the allocated data */
  gpointer line_data; /* A line of the image */
  struct tveng_frame_format format; /* The format of the data to save */
  gint lines; /* Lines saved by the thread */
  gboolean done; /* TRUE when done saving */
  gboolean close; /* TRUE if the plugin should close itself */
  pthread_t thread; /* The thread responsible for this data */
  GtkWidget * window; /* The window that contains the progressbar */
  struct jpeg_compress_struct cinfo; /* Compression parameters */
  struct jpeg_error_mgr jerr; /* Error handler */
  FILE * handle; /* Handle to the file we are saving */
  gchar *path; /* path to the file we are saving */
  gboolean set_bgr; /* TRUE if we got BGR data instead of RGB */
};

/*
  This routine is the one that takes care of saving the image. It runs
  in another thread.
*/
static void * saver_thread(void * _data);

/*
  This callback is used to update the progressbar and removing the
  finished threads.
*/
static gboolean thread_manager (struct screenshot_data * data);

/*
  Callback when the WM tries to destroy the progress window, cancel
  job.
*/
static gboolean
on_progress_delete_event               (GtkWidget       *widget,
					GdkEvent        *event,
                                        struct screenshot_data * data);

/* Conversions between different RGB pixel formats */
/*
  This Convert... functions are exported through the symbol mechanism
  so other plugins can use them too.
*/
static gchar *
Convert_RGB565_RGB24 (gint width, gchar* src, gchar* dest);

/* Conversions between different pixel formats */
static gchar *
Convert_RGB555_RGB24 (gint width, gchar* src, gchar* dest);

/* Removes the last byte (supposedly alpha channel info) from the
   image */
static gchar *
Convert_RGBA_RGB24 (gint width, gchar* src, gchar* dest);

/* Converts the given YUYV line to RGB */
static gchar *
Convert_YUYV_RGB24 (gint width, guchar* src, guchar* dest);

/* The definition of a line converter function */
/* The purpose of this kind of functions is to convert one line of src
 into one line of dest */
typedef gchar* (*LineConverter) (gint width, gchar* src, gchar* dest);

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
static gint ogb_startup_delay;
static int ogb_fd;

static gint
ov511_grab_button_timeout (void *unused)
{
  char button_flag = '0';

  if (ogb_fd < 0)
    {
      if (ogb_startup_delay-- > 0)
        return TRUE; /* repeat */
    
      ogb_fd = open ("/proc/video/ov511/0/button", O_RDONLY);

      if (ogb_fd == -1)
        {
	  /* no warning if file doesn't exist */
//          fprintf (stderr, "ov511_grab_button_timeout cannot open button flag file: %d, %s\n",
//		     errno, strerror(errno));
	  ogb_timeout_id = -1;
	  return FALSE; /* destroy */
        }

//      fprintf (stderr, "ov511_grab_button_timeout monitoring ov511 button flag file\n");
    }

  if (read (ogb_fd, &button_flag, 1) == 1)
    if (lseek (ogb_fd, 0, SEEK_SET) != (off_t) -1)
      {
        if (button_flag == '1')
	  plugin_start();

	return TRUE; /* repeat */
      }

//  fprintf (stderr, "ov511_grab_button_timeout read or seek error: %d, %s\n",
//           errno, strerror(errno));

  ogb_timeout_id = -1;
  return FALSE; /* destroy */
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

  ogb_fd = -1;
  ogb_startup_delay = 5 * 1000 / 250;
  ogb_timeout_id =
    gtk_timeout_add (250 /* ms */, (GtkFunction) ov511_grab_button_timeout, NULL);

  zapping_info = info;

  return TRUE;
}

static
void plugin_close(void)
{
  close_everything = TRUE;

  if (ogb_timeout_id >= 0)
    {
      if (ogb_fd >= 0)
        close (ogb_fd);

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
real_plugin_start	(const gchar	*dest_path)
{
  struct tveng_frame_format format;
  GdkPixbuf	*pixbuf;

  /*
   * Switch to capture mode if we aren't viewing Teletext (associated
   * with TVENG_NO_CAPTURE)
   */
  if (zapping_info->current_mode != TVENG_NO_CAPTURE)
    {
      zmisc_switch_mode(TVENG_CAPTURE_READ, zapping_info);
      
      if (zapping_info->current_mode != TVENG_CAPTURE_READ)
	return FALSE; /* unable to set the mode */
    }
  /* request TTX render */
  else if ((pixbuf =
	    ttxview_get_scaled_ttx_page(GTK_WIDGET(z_main_window()))))
    {
      format.width = gdk_pixbuf_get_width(pixbuf);
      format.height = gdk_pixbuf_get_height(pixbuf);
      format.bpp = 4;
      format.depth = 32;
      format.pixformat = TVENG_PIX_RGB32;
      format.bytesperline = gdk_pixbuf_get_rowstride(pixbuf);
      start_saving_screenshot(gdk_pixbuf_get_pixels(pixbuf),
			      dest_path,
			      &format);
      return TRUE;
    }
  else
    return FALSE;

  save_screenshot = 2;

  save_screenshot_file_name = g_strdup(dest_path);

  /* If everything has been ok, set the active flags and return TRUE
   */
  return TRUE;
}

static
gboolean plugin_start (void)
{
  gchar *filename = find_unused_name(save_dir, "shot", ".jpeg");
  gint result = real_plugin_start(filename);
  g_free(filename);

  return result;
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

  buffer = g_strconcat(root_key, "command", NULL);
  zconf_set_string(command, buffer);
  g_free(buffer);

  g_free(save_dir);
  g_free(command);
}

static
void plugin_read_bundle ( capture_bundle * bundle )
{
  if (!save_screenshot)
    return;
  else if (save_screenshot == 1)
    {
      if (save_screenshot_file_name)
	start_saving_screenshot(bundle->data,
				save_screenshot_file_name,
				&(bundle->format));
      g_free(save_screenshot_file_name);
      save_screenshot_file_name = NULL;
      
      save_screenshot = 0;
    }
  else
    save_screenshot --;
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
       "save_dir/shot[1,2,3,...].jpeg\n\n"
       "The quality option lets you choose how much info will be\n"
       "discarded when compressing the JPEG.\n\n"
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
				      _("Take a JPEG screenshot [s, Ctrl+s]"),
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

/* User defined functions */
static void
on_screenshot_button_clicked		(GtkButton       *button,
					 gpointer         user_data)
{
  /* Normal invocation, configure and start */
  GtkWidget *dialog;
  GtkEntry *entry;
  gchar *filename;
  GtkWidget *properties;

  dialog = build_widget("dialog1", PACKAGE_DATA_DIR "/screenshot.glade");
  entry = GTK_ENTRY(lookup_widget(dialog, "entry"));
  gnome_dialog_editable_enters(GNOME_DIALOG(dialog), GTK_EDITABLE (entry));
  gnome_dialog_set_default(GNOME_DIALOG(dialog), 0);
  filename = find_unused_name(save_dir, "shot", ".jpeg");
  gtk_entry_set_text(entry, filename);
  g_free(filename);
  gtk_entry_select_region(entry, 0, -1);

  gnome_dialog_set_parent(GNOME_DIALOG(dialog), z_main_window());
  gtk_widget_grab_focus(GTK_WIDGET(entry));

  /*
   * -1 or 1 if cancelled.
   */
  switch (gnome_dialog_run_and_close (GNOME_DIALOG (dialog)))
    {
    case 0: /* OK */
      filename = g_strdup(gtk_entry_get_text(entry));
      if (filename)
	real_plugin_start (filename);
      g_free(filename);
      break;
    case 2: /* Configure */
      properties = build_properties_dialog();
      open_properties_page(properties,
			   _("Plugins"), _("Screenshot"));
      gnome_dialog_run(GNOME_DIALOG(properties));
      break;
    default: /* Cancel */
      break;
    }

  gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void
on_screenshot_button_fast_clicked	(GtkButton       *button,
					 gpointer         user_data)
{
  plugin_start ();
}

/* This one starts a new thread that saves the current screenshot */
static void
start_saving_screenshot (gpointer data_to_save,
			 const gchar *path,
			 struct tveng_frame_format * format)
{
  struct screenshot_data * data = (struct screenshot_data*)
    g_malloc0(sizeof(struct screenshot_data));

  GtkWidget * vbox;
  GtkWidget * label;
  GtkWidget * progressbar;
  uint8_t *y, *u, *v, *t;
  gchar *b, *dir = g_dirname(path);

  if (!z_build_path(dir, &b))
    {
      ShowBox(_("Cannot create destination dir for screenshots:\n%s\n%s"),
	      GNOME_MESSAGE_BOX_WARNING, dir, b);
      g_free(b);
      g_free(dir);
      return;
    }
  g_free(dir);

  if (format->pixformat == TVENG_PIX_YVU420 ||
      format->pixformat == TVENG_PIX_YUV420)
    data -> data = g_malloc(format->width * format->height *
			    ((x11_get_bpp()+7)>>3));
  else
    data -> data = g_malloc(format->bytesperline * format->height);

  data->line_data = g_malloc(format->width*3);

  if (format->pixformat == TVENG_PIX_YVU420 ||
      format->pixformat == TVENG_PIX_YUV420)
    {
      y = (uint8_t*) data_to_save;
      u = y + (format->width*format->height);
      v = u + (format->width*format->height)/4;
      if (format->pixformat == TVENG_PIX_YVU420)
	{ t = u; u = v; v = t; }
      
      yuv2rgb(data->data, y, u, v, format->width, format->height,
	      format->width * ((x11_get_bpp()+7)>>3),
	      format->width, format->width*0.5);

      memcpy(&data->format, format, sizeof(struct tveng_frame_format));
      data->format.pixformat =
	zmisc_resolve_pixformat(x11_get_bpp(), x11_get_byte_order());
      data->format.bytesperline = format->width *
	((x11_get_bpp()+7)>>3);
      data->format.depth = x11_get_bpp();
      /* unfortunate election of names :-( */
      data->format.bpp = (x11_get_bpp()+7)>>3;
      data->format.sizeimage = data->format.bytesperline * format->height;
    }
  else
    {
      memcpy(&data->format, format, sizeof(struct tveng_frame_format));
      memcpy(data->data, data_to_save, format->bytesperline *
	     format->height);
    }

  /* Create this file */
  if (!(data->handle = fopen(path, "wb")))
    {
      gchar * window_title;

      window_title = g_strconcat(_("Sorry, but I cannot write\n"),
				 path,
				 _("\nThe image won't be saved.\n"),
				 strerror(errno),
				 NULL);

      ShowBox(window_title,
	      GNOME_MESSAGE_BOX_ERROR);

      g_free(window_title);
      g_free(data->line_data);
      g_free(data->data);
      g_free(data);

      return;
    }  

  data -> set_bgr = FALSE;

  /* Check if BGR must be used */
  switch (data->format.pixformat)
    {
    case TVENG_PIX_RGB32:
      data->set_bgr = FALSE;
      break;
    case TVENG_PIX_RGB24:
      data->set_bgr = FALSE;
      break;
    case TVENG_PIX_BGR32:
      data->set_bgr = TRUE;
      break;
    case TVENG_PIX_BGR24:
      data->set_bgr = TRUE;
      break;
    case TVENG_PIX_RGB565:
      data->set_bgr = TRUE;
      break;
    case TVENG_PIX_RGB555:
      data->set_bgr = TRUE;
      break;
    case TVENG_PIX_YUYV:
      data->set_bgr = FALSE;
      break;
    default:
      ShowBox("The current pixformat isn't supported",
	      GNOME_MESSAGE_BOX_ERROR);

      fclose(data->handle);
      g_free(data->line_data);
      g_free(data->data);
      g_free(data);
      
      return;
    }

  data->cinfo.err = jpeg_std_error(&(data->jerr));
  jpeg_create_compress(&(data->cinfo));
  jpeg_stdio_dest(&(data->cinfo), data->handle);
  data->cinfo.image_width = data->format.width;
  data->cinfo.image_height = data->format.height;
  data->cinfo.input_components = 3;
  data->cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&(data->cinfo));
  jpeg_set_quality(&(data->cinfo), quality, TRUE);

  jpeg_start_compress(&(data->cinfo), TRUE);

  data -> window = gtk_window_new(GTK_WINDOW_DIALOG);
  progressbar =
    gtk_progress_bar_new_with_adjustment(
      GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 100, 1,
					10, 10)));
  gtk_widget_show(progressbar);
  vbox = gtk_vbox_new (FALSE, 0);
  label = gtk_label_new(path);
  data->path = g_strdup(path);
  gtk_widget_show(label);
  gtk_box_pack_start_defaults(GTK_BOX(vbox),label);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), progressbar);
  gtk_widget_show(vbox);
  gtk_container_add (GTK_CONTAINER(data->window), vbox);
  gtk_window_set_title(GTK_WINDOW(data->window), _("Saving..."));
  gtk_window_set_modal(GTK_WINDOW(data->window), FALSE);
  gtk_object_set_data(GTK_OBJECT(data->window), "progressbar", progressbar);
  gtk_signal_connect(GTK_OBJECT(data->window), "delete-event",
		     (GtkSignalFunc)on_progress_delete_event, data);
  gtk_widget_show (data->window);

  switch (pthread_create(&(data->thread), NULL,
			 saver_thread, data))
    {
    case ENOMEM:
      ShowBox(_("Sorry, not enough resources for creating a new thread"), 
	      GNOME_MESSAGE_BOX_ERROR);
      break;
    case EAGAIN:
      ShowBox(_("There are too many threads"), GNOME_MESSAGE_BOX_ERROR);
      break;
    case 0:
      num_threads++;
      g_timeout_add(50, (GSourceFunc)thread_manager, data);
      break;
    }
}

/* Swaps the R and the B components of a RGB (BGR) image */
static void bgr2rgb(gchar * data, gint npixels)
{
  gchar c;
  gint i;

  for (i=0; i<npixels; i++)
    {
      c = data[0]; data[0] = data[2]; data[2] = c;
      data += 3;
    }
}

/*
  This routine is the one that takes care of saving the image. It runs
  in another thread.
*/
static void * saver_thread(void * _data)
{
  struct screenshot_data * data = (struct screenshot_data *) _data;
  LineConverter Converter = NULL; /* The line converter, could be NULL (no
				      conversion) */
  gint rowstride;
  gchar * pixels, *row_pointer = NULL;
  gboolean done_writing = FALSE;
  char *argv[10];
  int argc = 0;
  char *env[10];
  int envc = 0;
  int i;

  data -> lines = 0;
  data -> done = FALSE;
  data -> close = FALSE;

  /* Set up the necessary converter */
  switch (data->format.pixformat)
    {
    case TVENG_PIX_RGB32:
      Converter = (LineConverter) Convert_RGBA_RGB24;
      break;
    case TVENG_PIX_RGB24:
      Converter = NULL;
      break;
    case TVENG_PIX_BGR32:
      Converter = (LineConverter) Convert_RGBA_RGB24;
      break;
    case TVENG_PIX_BGR24:
      Converter = NULL; /* No conversion needed */
      break;
    case TVENG_PIX_RGB565:
      Converter = (LineConverter) Convert_RGB565_RGB24;
      break;
    case TVENG_PIX_RGB555:
      Converter = (LineConverter) Convert_RGB555_RGB24;
      break;
    case TVENG_PIX_YUYV:
      Converter = (LineConverter) Convert_YUYV_RGB24;
      break;
    default:
      g_assert_not_reached();
    }

  pixels = (gchar*) data->data;
  rowstride = data->format.bytesperline;

  while ((!done_writing) && (!close_everything) &&
	 (!data -> close))
    {
      if (Converter)
	row_pointer = (*Converter)(data->format.width, pixels,
				   (gchar*)data->line_data);
      else
	row_pointer = pixels;

      if (data->set_bgr)
	bgr2rgb(row_pointer, data->format.width);

      jpeg_write_scanlines(&(data->cinfo), (JSAMPROW*)&row_pointer, 1);

      pixels += rowstride;

      data->lines++;
      if (data->lines == data->format.height)
	done_writing = TRUE;
    }

  jpeg_finish_compress(&(data->cinfo));
  jpeg_destroy_compress(&(data->cinfo));
  fclose(data->handle);

  data->done = TRUE;

  /* Attempt to run command if it isn't empty */
  if (command && *command)
    {
      extern int cur_tuned_channel;
      extern tveng_tuned_channel *global_channel_list;
      tveng_tuned_channel *tc =
	tveng_retrieve_tuned_channel_by_index(cur_tuned_channel,
					      global_channel_list);
      /* Invoque through sh */
      argv[argc++] = "sh";
      argv[argc++] = "-c";
      argv[argc++] = command;
      /* FIXME */
      env[envc++] = g_strdup_printf("SCREENSHOT_PATH=%s", data->path);
      if (tc)
	{
	  env[envc++] = g_strdup_printf("CHANNEL_ALIAS=%s", tc->name);
	  env[envc++] = g_strdup_printf("CHANNEL_ID=%s",
					tc->real_name);
	  if (zapping_info->num_standards)
	    env[envc++] =
	      g_strdup_printf("CURRENT_STANDARD=%s",
		zapping_info->standards[zapping_info->cur_standard].name);
	  if (zapping_info->num_inputs)
	    env[envc++] =
	      g_strdup_printf("CURRENT_INPUT=%s",
		zapping_info->inputs[zapping_info->cur_input].name);
	}
      gnome_execute_async_with_env(NULL, argc, argv, envc, env);
      for (i=0; i<envc; i++)
	g_free(env[i]);
    }

  return NULL;
}

/*
  This callback is used to update the progressbar and removing the
  finished threads.
*/
static gboolean thread_manager (struct screenshot_data * data)
{
  gfloat done = ((gfloat)data->lines)/data->format.height;
  GtkWidget * progressbar;
  gpointer result;

  if (data->window)
    {
      progressbar = 
	gtk_object_get_data(GTK_OBJECT(data->window), "progressbar");
      gtk_progress_set_value(GTK_PROGRESS(progressbar), done*100);
    }

  if (data -> done)
    {
      if (data->window)
	gtk_widget_destroy(data->window);
      pthread_join(data->thread, &result);
      g_free(data -> path);
      g_free(data -> data);
      g_free(data -> line_data);
      g_free(data);
      num_threads--;
      return FALSE; /* Remove the timeout */
    }

  return TRUE; /* Keep calling */
}

static gboolean
on_progress_delete_event               (GtkWidget       *widget,
					GdkEvent        *event,
					struct screenshot_data * data)
{
  data -> close = TRUE; /* Tell the plugin to stop working */
  data -> window = NULL; /* It is being destroyed */

  return FALSE; /* destroy it, yes */
}

/* Takes a RGB565 line and saves it as RGB24 */
static gchar *
Convert_RGB565_RGB24 (gint width, gchar* src, gchar* dest)
{
  gint16 * line = (gint16*) src;
  gchar * where_have_we_written = dest;
  int i;
  gint16 word_value;

  for (i = 0; i < width; i++)
    {
      word_value = *line;
      *(dest++) = (word_value & 0x1f) << 3;
      word_value >>= 5;
      *(dest++) = (word_value & 0x3f) << 2;
      word_value >>= 6;
      *(dest++) = (word_value) << 3;
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
  gint16 word_value;

  for (i = 0; i < width; i++)
    {
      word_value = *line;
      *(dest++) = (word_value & 0x1f) << 3;
      word_value >>= 5;
      *(dest++) = (word_value & 0x1f) << 3;
      word_value >>= 5;
      *(dest++) = (word_value & 0x1f) << 3;
      line ++;
    }
  return where_have_we_written;
}

/* Removes the last byte (suppoosedly alpha channel info) from the
   image */
static gchar *
Convert_RGBA_RGB24 (gint width, gchar* src, gchar* dest)
{
  gchar * where_have_we_written = dest;
  int i;

  for (i = 0; i < width; i++)
    {
      *(dest++) = *(src++);
      *(dest++) = *(src++);
      *(dest++) = *(src++);
      src++;
    }
  return where_have_we_written;
}

/* Converts a YUYV line into a RGB24 one */
static gchar *
Convert_YUYV_RGB24 (gint w, guchar *src, guchar *dest )
{
  gchar *where_have_we_written = dest;
  gint i;
  double y,u,v;
  double r, g, b;

  for (i=0; i<w; i+=2)
    {
      y = src[0];
      u = src[1];
      v = src[3];

      y = ((y-16)*255)/219;
      u = ((u-128)*127)/112;
      v = ((v-128)*127)/112;

      r = y + 1.402*v;
      g = y - 0.344*u - 0.714*v;
      b = y + 1.772*u;

      /* clamping is needed */
      if (r > 255)
	r = 255;
      else if (r < 0)
	r = 0;
      if (g > 255)
	g = 255;
      else if (g < 0)
	g = 0;
      if (b > 255)
	b = 255;
      else if (b < 0)
	b = 0;

      dest[0] = r;
      dest[1] = g;
      dest[2] = b;

      dest += 3;

      y = src[2];
      u = src[1];
      v = src[3];

      y = ((y-16)*255)/219;
      u = ((u-128)*127)/112;
      v = ((v-128)*127)/112;

      r = y + 1.402*v;
      g = y - 0.344*u - 0.714*v;
      b = y + 1.772*u;

      /* clamping is needed */
      if (r > 255)
	r = 255;
      else if (r < 0)
	r = 0;
      if (g > 255)
	g = 255;
      else if (g < 0)
	g = 0;
      if (b > 255)
	b = 255;
      else if (b < 0)
	b = 0;

      dest[0] = r;
      dest[1] = g;
      dest[2] = b;

      src += 4;
      dest += 3;
    }

  return where_have_we_written;
}
#else // !HAVE_LIBJPEG
#endif
