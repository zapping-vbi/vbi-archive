/* RTE (Real time encoder) front end for Zapping
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
#include "plugin_common.h"
#ifdef HAVE_LIBRTE
#include <glade/glade.h>
#include <esd.h>
#include <rte.h>

/*
  TODO:
    . support for SECAM/NTSC/etc standards
    . Dynamic stats.
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
/* TRUE if we are running */
static gboolean active = FALSE;
/* The context we are encoding to */
static rte_context * context = NULL;
/* Info about the video device */
static tveng_device_info * zapping_info = NULL;
/* Pointer to the dialog that appears while saving */
static GtkWidget * saving_dialog = NULL;

/* Plugin options */
/* Compressor options */
static gdouble output_video_bits;
static gdouble output_audio_bits;
static gint engine_verbosity;
/* I/O */
static gchar* save_dir;
static gint output_mode; /* 0:file, 1:/dev/null */
static gint mux_mode; /* 0:audio, 1:video, 2:both */

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
    SYMBOL(plugin_capture_stop, 0x1234),
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

static
gboolean plugin_init ( PluginBridge bridge, tveng_device_info * info )
{
  zapping_info = info;

  if (!rte_init())
    {
      ShowBox("RTE cannot be inited in this box (no MMX?)\n",
	      GNOME_MESSAGE_BOX_ERROR);
      return FALSE;
    }

  return TRUE;
}

static
void plugin_close(void)
{
  if (active)
    {
      if (context)
	context = rte_context_destroy(context);
      active = FALSE;
    }
}

/*
 * Audio capture
 */
#define BUFFER_SIZE (ESD_BUF_SIZE)
static int		esd_recording_socket;
static short *		abuffer;
static int		scan_range;
static int		look_ahead;
static int		buffer_size;
static int		samples_per_frame;

static void
read_audio(void * data, double * time, rte_context * context)
{
	static double rtime, utime;
	static int left = 0;
	static short *p;
	int stereo = (context->audio_mode == RTE_AUDIO_MODE_STEREO) ? 1
		: 0;
	struct timeval tv;
	int sampling_rate = context->audio_rate;

	if (left <= 0)
	{
		ssize_t r;
		int n;

		memcpy(abuffer, abuffer + scan_range, look_ahead *
		       sizeof(abuffer[0]));

		p = abuffer + look_ahead;
		n = scan_range * sizeof(abuffer[0]);

		while (n > 0) {
			fd_set rdset;
			int err;

			FD_ZERO(&rdset);
			FD_SET(esd_recording_socket, &rdset);
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			err = select(esd_recording_socket+1, &rdset,
				   NULL, NULL, &tv);

			if ((err == -1) || (err == 0))
				continue;

			r = read(esd_recording_socket, p, n);
			
			if (r < 0 && errno == EINTR)
				continue;

			if (r == 0) {
				memset(p, 0, n);
				break;
			}

			(char *) p += r;
			n -= r;
		}

		gettimeofday(&tv, NULL);

		rtime = tv.tv_sec + tv.tv_usec / 1e6;
		rtime -= (scan_range - n) / (double) sampling_rate;

		left = scan_range - samples_per_frame;
		p = abuffer;

		*time = rtime;
		memcpy(data, p, context->audio_bytes);
		return;
	}

	utime = rtime + ((p - abuffer) >> stereo) / (double) sampling_rate;
	left -= samples_per_frame;

	p += samples_per_frame;

	*time = utime;
	memcpy(data, p, context->audio_bytes);
}

static void
audio_data_callback(rte_context * context, void * data, double * time, enum
		    rte_mux_mode stream, void * user_data)
{
  struct timeval tv;

  g_assert(stream == RTE_AUDIO);

  read_audio(data, time, context);
}

static gboolean
init_audio(gint rate, gboolean stereo)
{
  esd_format_t format = ESD_STREAM | ESD_RECORD | ESD_BITS16;

  format |= stereo ? ESD_STEREO : ESD_MONO;

  esd_recording_socket =
    esd_record_stream_fallback(format, rate, NULL, NULL);

  if (esd_recording_socket <= 0)
    return FALSE;

  samples_per_frame = 1152 << stereo;
  
  scan_range = MAX(BUFFER_SIZE / sizeof(short) /
		   samples_per_frame, 1) * samples_per_frame;
  
  look_ahead = (512 - 32) << stereo;
  
  buffer_size = (scan_range + look_ahead) * sizeof(abuffer[0]);

  abuffer = malloc(buffer_size);

  if (!abuffer)
    return FALSE;

  memset(abuffer, 0, buffer_size);

  return TRUE;
}

static void
close_audio(void)
{
  close(esd_recording_socket);
  esd_recording_socket = -1;

  if (abuffer)
    free(abuffer);
}

/* Called when compressed data is ready */
static void
encode_callback(rte_context * context, void * data, size_t size,
		void * user_data)
{
  /* currently /dev/null handler */
  g_assert(0xbeefdead == (int)user_data);
}

/*
 * Returns a pointer to a file available for writing and stores in
 * name the name of the opened file.
 * On error, NULL is returned, and name is undefined.
 * You need to g_free name on success.
 */
static FILE*
resolve_filename(const gchar * dir, const gchar * prefix,
		 const gchar * suffix, gchar ** name)
{
  gint clip_index = 1;
  gchar * buffer = NULL;
  FILE * returned_file = NULL;

  do {
    if (returned_file)
      fclose(returned_file);
    
    g_free(buffer);

    if ((!*save_dir) || (save_dir[strlen(save_dir)-1] != '/'))
      buffer = g_strdup_printf("%s/%s%d%s", save_dir, prefix,
			       clip_index++, suffix);
    else
      buffer = g_strdup_printf("%s%s%d%s", save_dir, prefix,
			       clip_index++, suffix);

    /* Just check for the existance for now */
    returned_file = fopen(buffer, "rb");
  } while (returned_file);

  if (!(returned_file = fopen(buffer, "wb")))
    {
      ShowBox("%s couldn't be opened for writing.\n"
	      "Check your permissions.", GNOME_MESSAGE_BOX_ERROR, buffer);
      g_free(buffer);
      return NULL;
    }

  if (name)
    *name = buffer;

  return returned_file;
}

static void
video_get_buffer(rte_context *context, rte_buffer *buf,
		 enum rte_mux_mode stream)
{
  fifo *capture_fifo = rte_get_user_data(context);
  buffer *b = NULL;
  capture_bundle *bundle;

  g_assert(capture_fifo != NULL);

  while (!b)
    {
      fprintf(stderr, "waiting full mpeg\n");
      b = wait_full_buffer(capture_fifo);
      fprintf(stderr, "waited full mpeg\n");
      bundle = (capture_bundle*)b->data;

      /* empty bundles are allowed (resizing, etc.) */
      if (!bundle->image_type ||
	  !bundle->data)
	{
	  fprintf(stderr, "sending empty mpeg\n");
	  send_empty_buffer(capture_fifo, b);
	  fprintf(stderr, "sent empty mpeg\n");
	  b = NULL;
	}
    }

  buf->data = bundle->data;
  buf->time = bundle->timestamp;
  buf->user_data = b;
}

static void
video_unref_buffer(rte_context *context, rte_buffer *buf)
{
  fifo *capture_fifo = rte_get_user_data(context);
  g_assert(capture_fifo != NULL);

  fprintf(stderr, "sending empty unref mpeg\n");
  send_empty_buffer(capture_fifo, (buffer*)buf->user_data);
  fprintf(stderr, "sent empty unref mpeg\n");
}

static
gboolean plugin_start (void)
{
  enum rte_pixformat pixformat = RTE_YUV420;
  gchar * file_name = NULL;
  GtkWidget * widget;
  FILE * file_fd;
  gchar * buffer;

  /* it would be better to gray out the button and set insensitive */
  if (active)
    {
      ShowBox("The plugin is running!", GNOME_MESSAGE_BOX_WARNING);
      return FALSE;
    }

  if (zmisc_switch_mode(TVENG_CAPTURE_READ, zapping_info))
    {
      ShowBox("This plugin needs to run in Capture mode, but"
	      " couln't switch to that mode:\n%s",
	      GNOME_MESSAGE_BOX_INFO, zapping_info->error);
      return FALSE;
    }

  /* FIXME: Size should be configurable */
  if (!request_bundle_format(TVENG_PIX_YUYV, 384, 288))
    {
      ShowBox("Cannot switch to YUYV capture format",
	      GNOME_MESSAGE_BOX_ERROR);
      return FALSE;
    }

  switch (zapping_info->format.pixformat)
    {
    case TVENG_PIX_YUYV:
      pixformat = RTE_YUYV;
      break;
    case TVENG_PIX_YUV420:
      pixformat = RTE_YUV420;
      break;
    default:
      ShowBox(_("The only supported pixformats are YUYV and YUV420"),
	      GNOME_MESSAGE_BOX_ERROR);
      return FALSE;
    }

  g_assert(context == NULL);

  if ((mux_mode+1) & 1)
    if (!init_audio(44100, FALSE))
      {
	ShowBox("Couldn't open the ESD device for capturing audio",
		GNOME_MESSAGE_BOX_ERROR);
	return FALSE;
      }

  context =
    rte_context_new(zapping_info->format.width,
		    zapping_info->format.height, pixformat,
		    get_capture_fifo());

  if (!context)
    {
      ShowBox("The encoding context cannot be created",
	      GNOME_MESSAGE_BOX_ERROR);
      close_audio();
      return FALSE;
    }

  rte_set_verbosity(context, engine_verbosity);

  /* Set up the context for encoding */
  switch (mux_mode)
    {
    case 0:
      rte_set_mode(context, RTE_AUDIO);
      rte_set_input(context, RTE_AUDIO, RTE_CALLBACKS, FALSE,
		    audio_data_callback, NULL, NULL);
      break;
    case 1:
      rte_set_mode(context, RTE_VIDEO);
      rte_set_input(context, RTE_VIDEO, RTE_CALLBACKS, TRUE, NULL,
		    video_get_buffer, video_unref_buffer);
      break;
    default:
      rte_set_mode(context, RTE_AUDIO | RTE_VIDEO);
      rte_set_input(context, RTE_AUDIO, RTE_CALLBACKS, FALSE,
		    audio_data_callback, NULL, NULL);
      rte_set_input(context, RTE_VIDEO, RTE_CALLBACKS, TRUE, NULL,
		    video_get_buffer, video_unref_buffer);
      break;
    }

  switch (output_mode)
    {
    case 0:
      /* we are just interested in the file name */
      file_fd = resolve_filename(save_dir, "clip", ".mpeg",
				 &file_name);
      if (!file_fd)
	{
	  context = rte_context_destroy(context);
	  if ((mux_mode+1) & 1)
	    close_audio();
	  return FALSE;
	}
      fclose(file_fd);
      rte_set_output(context, NULL, file_name);
      break;
    default:
      file_name = g_strdup("/dev/null");
      rte_set_output(context, encode_callback, NULL);
      break;
    }

  /* Set video and audio rates */
  rte_set_video_parameters(context, context->video_format,
			   context->width, context->height,
			   context->video_rate, output_video_bits*1e6);
  rte_set_audio_parameters(context, context->audio_rate,
			   context->audio_mode, output_audio_bits*1e3);

  /* Set everything up for encoding */
  if (!rte_init_context(context))
    {
      ShowBox("The encoding context cannot be inited: %s",
	      GNOME_MESSAGE_BOX_ERROR, context->error);
      context = rte_context_destroy(context);
      if ((mux_mode+1) & 1)
	close_audio();
      return FALSE;
    }

  if (!rte_start_encoding(context))
    {
      ShowBox("Cannot start encoding: %s", GNOME_MESSAGE_BOX_ERROR,
	      context->error);
      context = rte_context_destroy(context);
      if ((mux_mode+1) & 1)
	close_audio();
      return FALSE;
    }

  if (saving_dialog)
    gtk_widget_destroy(saving_dialog);

  saving_dialog =
    build_widget("dialog1", PACKAGE_DATA_DIR
		 "/mpeg_properties.glade");

  /* Set the fields on the control window */
  widget = lookup_widget(saving_dialog, "label13");
  buffer = g_strdup_printf(_("RTE ID: %s"), RTE_ID);
  gtk_label_set_text(GTK_LABEL(widget), buffer);
  g_free(buffer);

  widget = lookup_widget(saving_dialog, "label9");
  buffer = g_strdup_printf(_("Destination: %s"), file_name);
  gtk_label_set_text(GTK_LABEL(widget), buffer);
  g_free(buffer);

  widget = lookup_widget(saving_dialog, "label10");
  switch (mux_mode)
    {
    case 0:
      buffer = _("Audio only");
      break;
    case 1:
      buffer = _("Video only");
      break;
    default:
      buffer = _("Audio and Video");
      break;
    }
  gtk_label_set_text(GTK_LABEL(widget), buffer);

  widget = lookup_widget(saving_dialog, "label11");
  buffer =
    g_strdup_printf("%s %g\n%s %g",
		    _("Output video Mbits per second:"),
		    context->output_video_bits/1e6,
		    _("Output audio Kbits per second:"),
		    context->output_audio_bits/1e3);
  gtk_label_set_text(GTK_LABEL(widget), buffer);
  g_free(buffer);

  /* fixme: label12 : dinamic stats */

  g_free(file_name);

  gtk_widget_show(saving_dialog);

  /* don't let anyone mess with our settings from now on */
  capture_lock();

  active = TRUE;

  return TRUE;
}

static
void plugin_load_config (gchar * root_key)
{
  gchar *buffer;
  gchar *default_save_dir;

  default_save_dir = g_strconcat(getenv("HOME"), "/clips", NULL);

  buffer = g_strconcat(root_key, "output_video_bits", NULL);
  zconf_create_float(2.3, "Output video Mbits", buffer);
  output_video_bits = zconf_get_float(NULL, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "output_audio_bits", NULL);
  zconf_create_float(80, "Output audio Kbits", buffer);
  output_audio_bits = zconf_get_float(NULL, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "engine_verbosity", NULL);
  zconf_create_integer(0, "Engine verbosity", buffer);
  engine_verbosity = zconf_get_integer(NULL, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "save_dir", NULL);
  zconf_create_string(default_save_dir,
		      "Where will we write the clips to", buffer);
  zconf_get_string(&save_dir, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "output_mode", NULL);
  zconf_create_integer(0, "Where will we send the encoded stream",
		       buffer);
  output_mode = zconf_get_integer(NULL, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "mux_mode", NULL);
  zconf_create_integer(2, "Which kind of stream are we going to produce",
		       buffer);
  mux_mode = zconf_get_integer(NULL, buffer);
  g_free(buffer);

  g_free(default_save_dir);
}

static
void plugin_save_config (gchar * root_key)
{
  gchar *buffer;

  buffer = g_strconcat(root_key, "output_video_bits", NULL);
  zconf_set_float(output_video_bits, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "output_audio_bits", NULL);
  zconf_set_float(output_audio_bits, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "engine_verbosity", NULL);
  zconf_set_integer(engine_verbosity, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "save_dir", NULL);
  zconf_set_string(save_dir, buffer);
  g_free(save_dir);
  g_free(buffer);

  buffer = g_strconcat(root_key, "output_mode", NULL);
  zconf_set_integer(output_mode, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "mux_mode", NULL);
  zconf_set_integer(mux_mode, buffer);
  g_free(buffer);
}

static void
do_stop(void)
{
  capture_unlock();

  if (!active)
    return;

  context = rte_context_destroy(context);
  if ((mux_mode+1) & 1)
    close_audio();

  if (saving_dialog)
    gtk_widget_destroy(saving_dialog);

  saving_dialog = NULL;

  active = FALSE;
}

static
void plugin_capture_stop ( void )
{
  do_stop();
}

static
gboolean plugin_get_public_info (gint index, gpointer * ptr, gchar **
			     symbol, gchar ** description, gchar **
			     type, gint * hash)
{
  return FALSE; /* Nothing exported */
}

static void
on_property_item_changed              (GtkWidget * changed_widget,
				       GnomePropertyBox *propertybox)
{
  gnome_property_box_changed (propertybox);
}

static
void plugin_add_properties ( GnomePropertyBox * gpb )
{
  GtkWidget *mpeg_properties =
    build_widget("notebook1", PACKAGE_DATA_DIR "/mpeg_properties.glade");
  GtkWidget * label = gtk_label_new(_("MPEG"));
  GtkWidget * widget;
  GtkAdjustment * adj;
  gint page;

  /* Set current values and connect callbacks */
  widget = lookup_widget(mpeg_properties, "fileentry1");
  widget = gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY(widget));
  gtk_entry_set_text(GTK_ENTRY(widget), save_dir);
  gtk_signal_connect(GTK_OBJECT(widget), "changed",
		     on_property_item_changed, gpb);

  widget = lookup_widget(mpeg_properties, "optionmenu2");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget), mux_mode);
  gtk_signal_connect(GTK_OBJECT(GTK_OPTION_MENU(widget)->menu),
		     "deactivate", on_property_item_changed, gpb);

  widget = lookup_widget(mpeg_properties, "optionmenu1");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget), output_mode);
  gtk_signal_connect(GTK_OBJECT(GTK_OPTION_MENU(widget)->menu),
		     "deactivate", on_property_item_changed, gpb);

  widget = lookup_widget(mpeg_properties, "spinbutton1");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), engine_verbosity);
  gtk_signal_connect(GTK_OBJECT(widget), "changed",
		     on_property_item_changed, gpb);

  widget = lookup_widget(mpeg_properties, "hscale1");
  adj = gtk_range_get_adjustment(GTK_RANGE(widget));
  gtk_adjustment_set_value(adj, output_video_bits);
  gtk_signal_connect(GTK_OBJECT(adj), "value-changed",
		     on_property_item_changed, gpb);

  widget = lookup_widget(mpeg_properties, "hscale2");
  adj = gtk_range_get_adjustment(GTK_RANGE(widget));
  gtk_adjustment_set_value(adj, output_audio_bits);
  gtk_signal_connect(GTK_OBJECT(adj), "value-changed",
		     on_property_item_changed, gpb);

  gtk_widget_show(mpeg_properties);
  gtk_widget_show(label);

  page = gnome_property_box_append_page(gpb, mpeg_properties, label);

  gtk_object_set_data(GTK_OBJECT(gpb), "mpeg_page", GINT_TO_POINTER (page));
}

static
gboolean plugin_activate_properties ( GnomePropertyBox * gpb, gint page )
{
  gpointer data = gtk_object_get_data(GTK_OBJECT(gpb), "mpeg_page");
  GtkWidget * widget;
  GtkWidget * mpeg_properties;
  GtkAdjustment * adj;

  if (GPOINTER_TO_INT(data) == page)
    {
      mpeg_properties =
	gtk_notebook_get_nth_page(GTK_NOTEBOOK(gpb->notebook), page);

      widget = lookup_widget(mpeg_properties, "fileentry1");
      g_free(save_dir);
      save_dir =
	gnome_file_entry_get_full_path(GNOME_FILE_ENTRY(widget),
				       FALSE);

      widget = lookup_widget(mpeg_properties, "optionmenu2");
      widget = GTK_WIDGET(GTK_OPTION_MENU(widget)->menu);
      mux_mode = g_list_index(GTK_MENU_SHELL(widget)->children,
			      gtk_menu_get_active(GTK_MENU(widget)));

      widget = lookup_widget(mpeg_properties, "optionmenu1");
      widget = GTK_WIDGET(GTK_OPTION_MENU(widget)->menu);
      output_mode = g_list_index(GTK_MENU_SHELL(widget)->children,
				 gtk_menu_get_active(GTK_MENU(widget)));

      widget = lookup_widget(mpeg_properties, "spinbutton1");
      engine_verbosity =
	gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
      
      widget = lookup_widget(mpeg_properties, "hscale1");
      adj = gtk_range_get_adjustment(GTK_RANGE(widget));
      output_video_bits = adj->value;;

      widget = lookup_widget(mpeg_properties, "hscale2");
      adj = gtk_range_get_adjustment(GTK_RANGE(widget));
      output_audio_bits = adj->value;;

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
      ShowBox("FIXME: The help for this plugin hasn't been written yet",
	      GNOME_MESSAGE_BOX_INFO);
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
  plugin_start();
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

/*
 * Callback telling that the X in the dialog has been closed, we
 * should stop capturing.
*/
gboolean
on_mpeg_dialog1_delete_event		(GtkWidget	*widget,
					 GdkEvent	*event,
					 gpointer	user_data)
{
  saving_dialog = NULL;

  do_stop();
  
  return FALSE;
}

/*
 * Stop capturing pressed.
 */
void
on_mpeg_button1_clicked			(GtkButton	*button,
					 gpointer	user_data)
{
  g_assert(saving_dialog != NULL);

  gtk_widget_destroy(saving_dialog);
  saving_dialog = NULL;

  do_stop();
}
#else
/**
 * Load the plugin saying that it has been disabled due to RTE
 * missing, and tell the place to get it, with a handy "Goto.." button.
 */
#endif
