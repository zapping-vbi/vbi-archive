/*
 * RTE (Real time encoder) front end for Zapping
 *
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
 * Copyright (C) 2000-2002 Michael H. Schimek
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

/* $Id: mpeg.c,v 1.38 2002-09-27 23:55:29 mschimek Exp $ */

#include "plugin_common.h"

#ifdef HAVE_LIBRTE4

#include <glade/glade.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "audio.h"
#include "mpeg.h"
#include "properties.h"

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
static const gchar str_version[] = "0.2";
/* TRUE if we are running */
static volatile gboolean active = FALSE;

/* The context we are encoding to */
static rte_context * context_enc = NULL;

/* Info about the video device */
static tveng_device_info * zapping_info = NULL;
static gdouble captured_frame_rate = 0.0;
/* Audio device */
static gpointer audio_handle;
/* Pointer to the dialog that appears while saving */
static GtkWidget * saving_dialog = NULL;
/* Show the warning about sync with V4L */
static gboolean lip_sync_warning = TRUE;
/* The consumer for capture_fifo */
static consumer mpeg_consumer;
/* This updates the GUI */
static gint update_timeout_id = -1;
static gint volume_update_timeout_id = -1;
/* Whether the plugin has been configured */
static gboolean configured = FALSE;

/* Plugin options */
/* Compressor options */
static gint engine_verbosity;
/* I/O */
static gchar* save_dir;
static enum {
  OUTPUT_FILE,
  OUTPUT_DEV_NULL
} output_mode; /* 0:file, 1:/dev/null */
static gint capture_w, capture_h;

/* Properties handling code */
static void
properties_add			(GnomeDialog	*dialog);

/*
 *  This context instance is the properties dialog sandbox.
 *  The GUI displays options as reported by this context and
 *  all changes the user makes are stored here. For encoding
 *  we'll have a separate active instance the user can't mess
 *  with and the configuration is copied via zconf. 
 */
static rte_context *context_prop;
static GtkWidget *audio_options;
static GtkWidget *video_options;

static gboolean on_delete_event (GtkWidget *widget, GdkEvent
				*event, gpointer user_data);
static void on_button_clicked (GtkButton *button, gpointer user_data);

gint plugin_get_protocol ( void )
{
  /* You don't need to modify this function */
  return PLUGIN_PROTOCOL;
}

/* Return FALSE if we aren't able to access a symbol, you should only
   need to edit the pointer table, not the code */
gboolean plugin_get_symbol (gchar * name, gint hash, gpointer * ptr)
{
  /* Usually this table is the only thing you will need to change */
  struct plugin_exported_symbol table_of_symbols[] =
  {
    SYMBOL (plugin_init, 0x1234),
    SYMBOL (plugin_get_info, 0x1234),
    SYMBOL (plugin_close, 0x1234),
    SYMBOL (plugin_start, 0x1234),
    SYMBOL (plugin_load_config, 0x1234),
    SYMBOL (plugin_save_config, 0x1234),
    SYMBOL (plugin_capture_stop, 0x1234),
    SYMBOL (plugin_get_public_info, 0x1234),
    SYMBOL (plugin_add_gui, 0x1234),
    SYMBOL (plugin_remove_gui, 0x1234),
    SYMBOL (plugin_get_misc_info, 0x1234),
    SYMBOL (plugin_running, 0x1234),
    SYMBOL (plugin_process_popup_menu, 0x1234)
  };
  gint num_exported_symbols =
    sizeof (table_of_symbols)/sizeof (struct plugin_exported_symbol);
  gint i;

  /* Try to find the given symbol in the table of exported symbols
   of the plugin */
  for (i=0; i<num_exported_symbols; i++)
    if (!strcmp (table_of_symbols[i].symbol, name))
      {
	if (table_of_symbols[i].hash != hash)
	  {
	    if (ptr)
	      *ptr = GINT_TO_POINTER (0x3); /* hash collision code */
	    /* Warn */
	    g_warning (_("Check error: \"%s\" in plugin %s"
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
    *ptr = GINT_TO_POINTER (0x2); /* Symbol not found in the plugin */
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

static gboolean
record_cmd				(GtkWidget *	widget,
					 gint		argc,
					 gchar **	argv,
					 gpointer	user_data);
static gboolean
quickrec_cmd				(GtkWidget *	widget,
					 gint		argc,
					 gchar **	argv,
					 gpointer	user_data);

static
gboolean plugin_init ( PluginBridge bridge, tveng_device_info * info )
{
  /* Register the plugin as interested in the properties dialog */
  property_handler mpeg_handler =
  {
    add: properties_add
  };

  zapping_info = info;

  if (!rte_init ())
    {
      ShowBox ("RTE cannot be inited in this box (no MMX?)\n",
	      GNOME_MESSAGE_BOX_ERROR);
      return FALSE;
    }

  append_property_handler(&mpeg_handler);

  cmd_register ("record", record_cmd, NULL);
  cmd_register ("quickrec", quickrec_cmd, NULL);

  return TRUE;
}

static
void plugin_close (void)
{
  if (active)
    {
      if (context_enc)
	rte_context_delete (context_enc);
      context_enc = NULL;

      if (context_prop)
	rte_context_delete (context_prop);
      context_prop = NULL;

      if (audio_handle)
	close_audio_device (audio_handle);
      audio_handle = NULL;

      active = FALSE;
    }
}

static gint
update_timeout (rte_context *context)
{
  struct rte_status_info status;
  GtkWidget *widget;
  gchar *buffer,*dropbuf,*procbuf;
  static gchar buf[64];
  gint h, m, s;
  gdouble t, rate;

  if (!active || !saving_dialog)
    {
      update_timeout_id = -1;
      return FALSE;
    }

  rte_get_status (context, &status);

  widget = lookup_widget (saving_dialog, "label31");

  s  = t = status.processed_frames / captured_frame_rate;
  m  = s / 60;
  s -= m * 60;
  h  = m / 60;
  m -= h * 60;

  /* XXX not really what i want */
  rate = status.bytes_out * (8 / 1e6) / t;

  snprintf(buf, sizeof(buf) - 1,
	   "%3u:%02u:%02u  %7.3f MB  %6.3f Mbit/s",
	   h, m, s, status.bytes_out / (double)(1 << 20),
	   rate);

  gtk_label_set_text (GTK_LABEL (widget), buf);



  widget = lookup_widget (saving_dialog, "label12");
  /* NB *_frames display will wrap after two years. */
  dropbuf = g_strdup_printf ((char *) ngettext ("%d frame dropped",
				     "%d frames dropped",
				     status.dropped_frames),
			    status.dropped_frames);
  procbuf = g_strdup_printf ((char *) ngettext ("%d frame processed",
				     "%d frames processed",
				     status.processed_frames),
			    status.processed_frames);
  buffer = g_strdup_printf (_("%s : %s"),
		    dropbuf, procbuf);
  gtk_label_set_text (GTK_LABEL (widget), buffer);
  g_free (buffer);
  g_free (dropbuf);
  g_free (procbuf);

  return TRUE;
}

/*
 *  Input/output
 */

static void
audio_data_callback (rte_context *context, void *data, double *time,
		     enum rte_mux_mode stream, void *user_data)
{
  g_assert (stream == RTE_AUDIO);

  read_audio_data (audio_handle, data, context->audio_bytes, time);
}

/* Called when compressed data is ready */
static void
encode_callback (rte_context *context, void *data, ssize_t size,
		 void *user_data)
{
  /* currently /dev/null handler */
}

static void
video_buffer_callback (rte_context *context, rte_buffer *rb,
		       enum rte_mux_mode stream)
{
  buffer *b;
  struct tveng_frame_format *fmt;

  g_assert (stream == RTE_VIDEO);

  for (;;) {
    capture_buffer *cb = (capture_buffer *)
      (b = wait_full_buffer (&mpeg_consumer));

    fmt = &cb->d.format;

    if (cb->d.image_type &&
	fmt->height == context->height &&
	fmt->width == context->width &&
	fmt->sizeimage == context->video_bytes &&
	b->time)
      break;

    send_empty_buffer (&mpeg_consumer, b);
  }

  rb->time = b->time;
  rb->data = b->data;
  rb->user_data = b;
}

static void
video_unref_callback (rte_context *context, rte_buffer *buf)
{
  send_empty_buffer (&mpeg_consumer, (buffer*)buf->user_data);
}

/*
 *  Initialization
 */

static gboolean
real_plugin_start (const gchar *file_name)
{
  enum rte_pixformat pixformat = 0;
  enum tveng_frame_pixformat tveng_pixformat;
  rte_stream_parameters params;
  GtkWidget * widget;
  gchar * buffer;
  GtkWidget *dialog, *label;
  rte_context *context;
  rte_codec *audio_codec, *video_codec;

  tveng_pixformat =
    zconf_get_integer (NULL, "/zapping/options/main/yuv_format");

  /* it would be better to gray out the button and set insensitive */
  if (active)
    {
      ShowBox ("The plugin is running!", GNOME_MESSAGE_BOX_WARNING);
      return FALSE;
    }

#if 0
  if (lip_sync_warning &&
      zapping_info->current_controller == TVENG_CONTROLLER_V4L1)
    {
      dialog =
	gnome_dialog_new (_("Synchronization under V4L1"),
			 GNOME_STOCK_BUTTON_OK,
			 _("Do not show this again"),
			 _("V4L2 page"),
			 NULL);
      label =
	gtk_label_new (_("You are using a V4L1 driver, so synchronization\n"
			"between audio and video will not be very good.\n"
			"V4L2 drivers provide a much better sync,\n"
			"you might want to try those if you aren't\n"
			"satisfied with the results."));

      gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label,
			 TRUE, TRUE, 0);

      gtk_widget_show (label);

      gnome_dialog_set_default (GNOME_DIALOG (dialog), 1);

      switch (gnome_dialog_run_and_close (GNOME_DIALOG (dialog)))
	{
	case 1:
	  lip_sync_warning = FALSE;
	  break;
	case 2:
	  gnome_url_show ("http://www.thedirks.org/v4l2");
	  break;
	default:
	  break;
	}
    }
#endif

  g_assert (context_enc == NULL);

  context = grte_context_load (ZCONF_DOMAIN, MPEG_CONFIG,
			       NULL, &audio_codec, &video_codec,
			       NULL, NULL);
  if (!context)
    {
      ShowBox ("Couldn't create the mpeg encoding context",
	       GNOME_MESSAGE_BOX_ERROR);
      return FALSE;
    }

  context_enc = context;

  if (0)
    fprintf (stderr, "codecs: a%p v%p\n", audio_codec, video_codec);

  captured_frame_rate = 0.0;

  if (video_codec)
    {
      if (zmisc_switch_mode (TVENG_CAPTURE_READ, zapping_info))
	{
	  rte_context_destroy (context);
	  context_enc = NULL;

	  ShowBox ("This plugin needs to run in Capture mode, but"
		   " couldn't switch to that mode:\n%s",
		   GNOME_MESSAGE_BOX_INFO, zapping_info->error);
	  return FALSE;
	}

      if (!request_bundle_format (tveng_pixformat,
				  capture_w, capture_h))
	{
	  rte_context_destroy (context);
	  context_enc = NULL;

	  ShowBox ("Cannot switch to %s capture format",
		   GNOME_MESSAGE_BOX_ERROR,
		   (tveng_pixformat == TVENG_PIX_YVU420) ?
		   "YVU420" : "YUYV");
	  return FALSE;
	}

      if (tveng_pixformat == TVENG_PIX_YVU420)
	pixformat = RTE_YVU420;
      else
	pixformat = RTE_YUYV;

      if (!zapping_info->num_standards)
	{
	  rte_context_destroy (context);
	  context_enc = NULL;

	  ShowBox ("Unable to determine current video standard",
		   GNOME_MESSAGE_BOX_ERROR);
	  return FALSE;
	}
  
      captured_frame_rate = zapping_info->standards[
	      zapping_info->cur_standard].frame_rate;

      /* g_assert */ (rte_option_set (video_codec, "coded_frame_rate",
				(double) captured_frame_rate));
    }

  if (audio_codec)
    {
      /* XXX improve me */

      memset (&params, 0, sizeof (params));
      g_assert (rte_set_parameters (audio_codec, &params));

      audio_handle = open_audio_device (params.audio.channels > 1,
					params.audio.sampling_freq,
					AUDIO_FORMAT_S16_LE);
      if (!audio_handle)
	{
	  ShowBox ("Couldn't open the audio device",
		  GNOME_MESSAGE_BOX_ERROR);
	  goto failed;
	}

      if (!video_codec) /* XXX mp2 only */
	captured_frame_rate = params.audio.sampling_freq / 1152.0;
    }

  rte_set_verbosity (context, engine_verbosity);

  /* Set up the context for encoding */
  switch ((!!video_codec) * 2 + (!!audio_codec))
    {
      case 1: /* audio only */
	rte_set_mode (context, RTE_AUDIO);
	rte_set_input (context, RTE_AUDIO, RTE_CALLBACKS, FALSE,
		      audio_data_callback, NULL, NULL);
	break;
      case 2: /* video only */
	rte_set_mode (context, RTE_VIDEO);
	rte_set_input (context, RTE_VIDEO, RTE_CALLBACKS, TRUE, NULL,
		      video_buffer_callback, video_unref_callback);
	break;
      case 3: /* audio & video */
	rte_set_mode (context, RTE_AUDIO | RTE_VIDEO);
	rte_set_input (context, RTE_AUDIO, RTE_CALLBACKS, FALSE,
		      audio_data_callback, NULL, NULL);
	rte_set_input (context, RTE_VIDEO, RTE_CALLBACKS, TRUE, NULL,
		      video_buffer_callback, video_unref_callback);
	break;
      default:
	ShowBox ("Nothing to record.",
		GNOME_MESSAGE_BOX_ERROR);
	goto failed;
    }

  if (output_mode == OUTPUT_FILE)
    {
      gchar *dir = g_dirname(file_name);
      gchar *error_msg;

      if (!z_build_path (dir, &error_msg))
	{
	  ShowBox (_("Cannot create destination dir for clips:\n%s\n%s"),
		   GNOME_MESSAGE_BOX_WARNING, dir, error_msg);
	  g_free (error_msg);
	  g_free (dir);
	  goto failed;
	}
      g_free(dir);

      rte_set_output (context, NULL, NULL, file_name);
    }
  else
    rte_set_output (context, encode_callback, NULL, NULL);

  /* Set video and audio rates */

  if (video_codec)
    {
      rte_option_value val;

      g_assert (rte_option_get (video_codec, "bit_rate", &val));

      rte_set_video_parameters (context, pixformat,
			       zapping_info->format.width,
			       zapping_info->format.height,
			       context->video_rate,
			       val.num /* output_video_bits */,
			       context->gop_sequence);
    }

  /* Set everything up for encoding */
  if (!rte_init_context (context))
    {
      ShowBox ("The encoding context cannot be inited: %s",
	      GNOME_MESSAGE_BOX_ERROR, context->error);
      goto failed;
    }

  active = TRUE;

  /* don't let anyone mess with our settings from now on */
  capture_lock ();

  add_consumer (capture_fifo, &mpeg_consumer);

  if (!rte_start_encoding (context))
    {
      ShowBox ("Cannot start encoding: %s", GNOME_MESSAGE_BOX_ERROR,
	      context->error);
      rem_consumer (&mpeg_consumer);
      capture_unlock ();
      active = FALSE;
      goto failed;
    }

  if (saving_dialog)
    gtk_widget_destroy (saving_dialog);

  saving_dialog = build_widget ("dialog1", "mpeg_properties.glade");

  gtk_signal_connect (GTK_OBJECT (saving_dialog), "delete-event",
		     GTK_SIGNAL_FUNC (on_delete_event),
		     NULL);
  widget = lookup_widget (saving_dialog, "button1");
  gtk_signal_connect (GTK_OBJECT (widget), "clicked",
		     GTK_SIGNAL_FUNC (on_button_clicked),
		     NULL);

  /* Set the fields on the control window */
  widget = lookup_widget (saving_dialog, "label13");
  buffer = g_strdup_printf (_("RTE ID: %s"), RTE_ID);
  gtk_label_set_text (GTK_LABEL (widget), buffer);
  g_free (buffer);

  widget = lookup_widget (saving_dialog, "label9");
  if (output_mode == OUTPUT_FILE)
    buffer = g_strdup_printf (_("Destination: %s"), file_name);
  else
    buffer = g_strdup_printf (_("Destination: %s"), "/dev/null");
  gtk_label_set_text (GTK_LABEL (widget), buffer);
  g_free (buffer);

  widget = lookup_widget (saving_dialog, "label10");
  switch ((!!video_codec) * 2 + (!!audio_codec))
    {
      case 1:
	buffer = _("Audio only");
	break;
      case 2:
	buffer = _("Video only");
	break;
      default:
	buffer = _("Audio and Video");
	break;
    }
  gtk_label_set_text (GTK_LABEL (widget), buffer);

  widget = lookup_widget (saving_dialog, "label11");
  {
    rte_option_value val1, val2;

    if (audio_codec)
      rte_option_get (audio_codec, "bit_rate", &val1);

    if (video_codec)
      rte_option_get (video_codec, "bit_rate", &val2);

    if (!video_codec)
      buffer = g_strdup_printf ("%s %g",
			       _("Output audio kbits per second:"),
			       val1.num / 1e3);
    else if (!audio_codec)
      buffer = g_strdup_printf ("%s %g",
			       _("Output video Mbits per second:"),
			       val2.num / 1e6);
    else
      buffer = g_strdup_printf ("%s %g\n%s %g",
			       _("Output audio kbits per second:"),
			       val1.num / 1e3,
			       _("Output video Mbits per second:"),
			       val2.num / 1e6);
  }
  gtk_label_set_text (GTK_LABEL (widget), buffer);
  g_free (buffer);

  widget = lookup_widget (saving_dialog, "label12");
  gtk_label_set_text (GTK_LABEL (widget), _("Waiting for frames..."));

  update_timeout_id =
    gtk_timeout_add (250, (GtkFunction)update_timeout, context);

  gtk_widget_show (saving_dialog);

  return TRUE;

 failed:
  if (context_enc)
    rte_context_destroy (context_enc);
  context_enc = NULL;

  if (audio_handle)
    close_audio_device (audio_handle);
  audio_handle = NULL;

  return FALSE;
}


static
gboolean plugin_start (void)
{
  gchar *filename = find_unused_name(save_dir, "clip", ".mpeg");
  gint result = real_plugin_start(filename);
  g_free(filename);

  return result;
}

static void
do_stop (void)
{
  if (!active)
    return;

  rte_context_delete (context_enc);
  context_enc = NULL;

  if (saving_dialog)
    {
      gtk_widget_destroy (saving_dialog);

      saving_dialog = NULL;
    }

  active = FALSE;

  rem_consumer (&mpeg_consumer);

  if (update_timeout_id >= 0)
    {
      gtk_timeout_remove (update_timeout_id);
      update_timeout_id = -1;
    }

  if (volume_update_timeout_id >= 0)
    {
      gtk_timeout_remove (volume_update_timeout_id);
      volume_update_timeout_id = -1;
    }

  if (audio_handle)
    close_audio_device (audio_handle);
  audio_handle = NULL;

  capture_unlock ();
}

static gboolean
plugin_running (void)
{
  return active;
}

static
void plugin_load_config (gchar * root_key)
{
  gchar *buffer;
  gchar *default_save_dir;

  buffer = g_strconcat (root_key, "configured", NULL);
  zconf_create_boolean (FALSE,
			"Whether the plugin has been configured", buffer);
  configured = zconf_get_boolean (NULL, buffer);
  g_free (buffer);

  default_save_dir = g_strconcat (getenv ("HOME"), "/clips", NULL);

  buffer = g_strconcat (root_key, "engine_verbosity", NULL);
  zconf_create_integer (0, "Engine verbosity", buffer);
  engine_verbosity = zconf_get_integer (NULL, buffer);
  g_free (buffer);

  buffer = g_strconcat (root_key, "save_dir", NULL);
  zconf_create_string (default_save_dir,
		      "Where will we write the clips to", buffer);
  zconf_get_string (&save_dir, buffer);
  g_free (buffer);

  buffer = g_strconcat (root_key, "output_mode", NULL);
  zconf_create_integer (0, "Where will we send the encoded stream",
		       buffer);
  output_mode = zconf_get_integer (NULL, buffer);
  g_free (buffer);

  buffer = g_strconcat (root_key, "capture_w", NULL);
  zconf_create_integer (384, "Capture Width", buffer);
  capture_w = zconf_get_integer (NULL, buffer);
  g_free (buffer);

  buffer = g_strconcat (root_key, "capture_h", NULL);
  zconf_create_integer (288, "Capture height", buffer);
  capture_h = zconf_get_integer (NULL, buffer);
  g_free (buffer);

  buffer = g_strconcat (root_key, "lip_sync_warning", NULL);
  zconf_create_boolean (TRUE, "Warning about lip sync with V4L", buffer);
  lip_sync_warning = zconf_get_boolean (NULL, buffer);
  g_free (buffer);

  g_free (default_save_dir);
}

static
void plugin_save_config (gchar * root_key)
{
  gchar *buffer;

  buffer = g_strconcat (root_key, "configured", NULL);
  zconf_set_boolean (configured, buffer);
  g_free (buffer);

  buffer = g_strconcat (root_key, "engine_verbosity", NULL);
  zconf_set_integer (engine_verbosity, buffer);
  g_free (buffer);

  buffer = g_strconcat (root_key, "save_dir", NULL);
  zconf_set_string (save_dir, buffer);
  g_free (save_dir);
  g_free (buffer);

  buffer = g_strconcat (root_key, "output_mode", NULL);
  zconf_set_integer (output_mode, buffer);
  g_free (buffer);

  buffer = g_strconcat (root_key, "capture_w", NULL);
  zconf_set_integer (capture_w, buffer);
  g_free (buffer);

  buffer = g_strconcat (root_key, "capture_h", NULL);
  zconf_set_integer (capture_h, buffer);
  g_free (buffer);

  buffer = g_strconcat (root_key, "lip_sync_warning", NULL);
  zconf_set_boolean (lip_sync_warning, buffer);
  g_free (buffer);

  if (context_prop)
    grte_context_save (context_prop, ZCONF_DOMAIN, MPEG_CONFIG, 0, 0);
}

static
void plugin_capture_stop ( void )
{
  do_stop ();
}

static
gboolean plugin_get_public_info (gint index, gpointer * ptr, gchar **
			     symbol, gchar ** description, gchar **
			     type, gint * hash)
{
  return FALSE; /* Nothing exported */
}

/*
 *  Properties
 */

static void nullify (void **p)
{
  *p = NULL;
}

static void
select_codec (GtkWidget *mpeg_properties, GtkWidget *menu,
	      rte_stream_type stream_type)
{
  GtkWidget *menu_item = gtk_menu_get_active (GTK_MENU (menu));
  GtkWidget *vbox = 0;
  GtkWidget **optionspp = 0;
  rte_codec *codec;
  char *keyword;

  switch (stream_type)
    {
      case RTE_STREAM_AUDIO:
	vbox = lookup_widget (mpeg_properties, "vbox5");
	optionspp = &audio_options;
	break;
      case RTE_STREAM_VIDEO:
	vbox = lookup_widget (mpeg_properties, "vbox4");
	optionspp = &video_options;
	break;
      default:
	g_assert_not_reached ();
    }

  g_assert (vbox && menu_item);

  if (*optionspp)
    gtk_widget_destroy (*optionspp);
  *optionspp = NULL;

  keyword = gtk_object_get_data (GTK_OBJECT (menu_item), "keyword");

  if (keyword)
    {
      codec = grte_codec_load (context_prop,
			       ZCONF_DOMAIN, MPEG_CONFIG,
			       stream_type, keyword);
      g_assert (codec);

      *optionspp = grte_options_create (context_prop, codec);

      if (*optionspp)
	{
	  gtk_widget_show (*optionspp);
	  gtk_box_pack_end (GTK_BOX (vbox), *optionspp, TRUE, TRUE, 3);
	  gtk_signal_connect_object (GTK_OBJECT (*optionspp), "destroy",
				     GTK_SIGNAL_FUNC (nullify),
				     (GtkObject *) optionspp);
	}
    }
  else
    {
      rte_codec_set (context_prop, stream_type, 0, NULL);
    }
}

static void
on_audio_codec_changed                (GtkWidget * changed_widget,
				       GtkWidget * mpeg_properties)
{
  select_codec (mpeg_properties, changed_widget, RTE_STREAM_AUDIO);
}

static void
on_video_codec_changed                (GtkWidget * changed_widget,
				       GtkWidget * mpeg_properties)
{
  select_codec (mpeg_properties, changed_widget, RTE_STREAM_VIDEO);
}


static void
attach_codec_menu (GtkWidget *mpeg_properties, gchar *widget_name,
		   rte_stream_type stream_type)
{
  GtkWidget *menu, *menu_item;
  GtkWidget *widget;
  gint default_item;
  void (* on_changed) (GtkWidget *, GtkWidget *) = 0;

  switch (stream_type)
    {
      case RTE_STREAM_AUDIO:
	on_changed = on_audio_codec_changed;
	break;
      case RTE_STREAM_VIDEO:
	on_changed = on_video_codec_changed;
	break;
      default:
	g_assert_not_reached ();
    }

  widget = lookup_widget (mpeg_properties, widget_name);

  if ((menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (widget))))
    gtk_widget_destroy (menu);

  menu = grte_codec_create_menu (context_prop,
				 ZCONF_DOMAIN, MPEG_CONFIG,
				 stream_type, &default_item);
  g_assert (menu);

  gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (widget), default_item);
  gtk_signal_connect (GTK_OBJECT (GTK_OPTION_MENU (widget)->menu),
		      "deactivate", on_changed, mpeg_properties);

  select_codec (mpeg_properties, menu, stream_type);
}

static void
select_format (GtkWidget *mpeg_properties, GtkWidget *menu)
{
  GtkWidget *menu_item = gtk_menu_get_active (GTK_MENU (menu));
  char *keyword;

  keyword = gtk_object_get_data (GTK_OBJECT (menu_item), "keyword");

  if (context_prop)
    rte_context_delete (context_prop);

  context_prop = rte_context_new (352, 288, keyword, NULL);

  attach_codec_menu (mpeg_properties, "optionmenu5", RTE_STREAM_AUDIO);
  attach_codec_menu (mpeg_properties, "optionmenu6", RTE_STREAM_VIDEO);
}

static void
on_format_changed                     (GtkWidget * changed_widget,
				       GtkWidget * mpeg_properties)
{
  select_format (mpeg_properties, changed_widget);
}

static void
attach_format_menu (GtkWidget *mpeg_properties)
{
  GtkWidget *menu, *menu_item;
  GtkWidget *widget;
  gint default_item;

  widget = lookup_widget (mpeg_properties, "optionmenu7");

  if ((menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (widget))))
    gtk_widget_destroy (menu);

  menu = grte_context_create_menu (ZCONF_DOMAIN, MPEG_CONFIG, &default_item);

  g_assert (menu);

  gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (widget), default_item);
  gtk_signal_connect (GTK_OBJECT (GTK_OPTION_MENU (widget)->menu),
  		      "deactivate", on_format_changed, mpeg_properties);

  select_format (mpeg_properties, menu);
}

static void
on_filename_changed                (GtkWidget * changed_widget,
				    gpointer	unused)
{
  // z_on_electric_filename (changed_widget, NULL);
}

static void
mpeg_setup			(GtkWidget	*page)
{
  GtkWidget * widget;
  GtkWidget *menu, *menu_item;
  GtkAdjustment * adj;

  /* Set current values and connect callbacks */
  widget = lookup_widget (page, "fileentry1");
  widget = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (widget));
  gtk_entry_set_text (GTK_ENTRY (widget), save_dir);
  // XXX basen.ame and auto-increment, no clips dir.
  gtk_object_set_data (GTK_OBJECT (widget), "basename", (gpointer) ".mpg");
  gtk_signal_connect (GTK_OBJECT (widget), "changed",
		      on_filename_changed, NULL);
  widget = lookup_widget (page, "spinbutton5");
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), capture_w);

  widget = lookup_widget (page, "spinbutton6");
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), capture_h);

  widget = lookup_widget (page, "optionmenu1");
  gtk_option_menu_set_history (GTK_OPTION_MENU (widget), output_mode);

  widget = lookup_widget (page, "spinbutton1");
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), engine_verbosity);

  attach_format_menu (page);
}

static void
mpeg_apply			(GtkWidget	*page)
{
  GtkWidget * widget;
  GtkWidget * mpeg_properties = page;
  GtkAdjustment * adj;

  widget = lookup_widget (mpeg_properties, "fileentry1");
  g_free (save_dir);
  save_dir =
    gnome_file_entry_get_full_path (GNOME_FILE_ENTRY (widget),
				    FALSE);
  widget = lookup_widget (mpeg_properties, "optionmenu1");
  output_mode = z_option_menu_get_active(widget);

  widget = lookup_widget (mpeg_properties, "spinbutton1");
  engine_verbosity =
    gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));

  widget = lookup_widget (mpeg_properties, "spinbutton5");
  capture_w =
    gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));
  capture_w &= ~15;

  widget = lookup_widget (mpeg_properties, "spinbutton6");
  capture_h =
    gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (widget));
  capture_h &= ~15;

  if (context_prop)
    grte_context_save (context_prop, ZCONF_DOMAIN, MPEG_CONFIG, 0, 0);

  configured = TRUE;
}

static void
properties_add			(GnomeDialog	*dialog)
{
  /* FIXME: Perhaps having a MPEG group with 3 items would be better? */
  SidebarEntry plugin_options[] = {
    { N_("MPEG"), ICON_ZAPPING, "gnome-media-player.png", "notebook1",
      mpeg_setup, mpeg_apply }
  };
  SidebarGroup groups[] = {
    { N_("Plugins"), plugin_options, acount(plugin_options) }
  };

  standard_properties_add(dialog, groups, acount(groups),
			  "mpeg_properties.glade");
}

/**
 * Makes sure that the plugin has been configured at least once.
 */
static gboolean
mpeg_configured			(void)
{
  if (!configured)
    {
      GtkWidget *properties;
      GtkWidget *configure_message = gnome_message_box_new
	(_("This is the first time you run the plugin,\n"
	   "please take a moment to configure it to your tastes.\n"),
	 GNOME_MESSAGE_BOX_GENERIC,
	 _("Open preferences"), GNOME_STOCK_BUTTON_CANCEL, NULL);
      gint ret = gnome_dialog_run (GNOME_DIALOG (configure_message));

      switch (ret)
	{
	case 0:
	  properties = build_properties_dialog ();
	  open_properties_page (properties, _("Plugins"), _("MPEG"));
	  gnome_dialog_run (GNOME_DIALOG (properties));
	  configured = TRUE;
	  break;

	case 1:
	default:
	  /* User closed dialog with window manager,
	   * assume "cancel"
	   */
	  return FALSE;
	}
    }

  return TRUE;
}

static gboolean
record_cmd				(GtkWidget *	widget,
					 gint		argc,
					 gchar **	argv,
					 gpointer	user_data)
{
  /* Normal invocation, configure and start */
  GtkWidget *dialog;
  GtkEntry *entry;
  gchar *filename;
  GtkWidget *properties;

fprintf(stderr, "###4\n");

  if (!mpeg_configured())
    return FALSE;

  if (output_mode != OUTPUT_FILE)
    {
      plugin_start();
      return FALSE;
    }

  dialog = build_widget("dialog2", "mpeg_properties.glade");
  entry = GTK_ENTRY(lookup_widget(dialog, "entry"));
  gnome_dialog_editable_enters(GNOME_DIALOG(dialog), GTK_EDITABLE (entry));
  gnome_dialog_set_default(GNOME_DIALOG(dialog), 0);
  filename = find_unused_name(save_dir, "clip", ".mpeg");
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
			   _("Plugins"), _("MPEG"));
      gnome_dialog_run(GNOME_DIALOG(properties));
      break;
    default: /* Cancel */
      break;
    }

  gtk_widget_destroy(GTK_WIDGET(dialog));

  return TRUE;
}

static gboolean
quickrec_cmd				(GtkWidget *	widget,
					 gint		argc,
					 gchar **	argv,
					 gpointer	user_data)
{
  if (!mpeg_configured())
    return FALSE;

  plugin_start ();

  return TRUE;
}

static const gchar *tooltip = N_("Start recording");

static
void plugin_add_gui (GnomeApp * app)
{
  GtkWidget * toolbar1 = lookup_widget (GTK_WIDGET (app), "toolbar1");
  GtkWidget * button; /* The button to add */
  GtkWidget * tmp_toolbar_icon;
  gint sig_id;

  tmp_toolbar_icon =
    gnome_stock_pixmap_widget (GTK_WIDGET (app),
			       GNOME_STOCK_PIXMAP_COLORSELECTOR);
  button = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar1),
				       GTK_TOOLBAR_CHILD_BUTTON, NULL,
				       _("MPEG"),
				       NULL, NULL, tmp_toolbar_icon,
				       on_remote_command1,
				       (gpointer)((const gchar *) "record"));

  z_tooltip_set (button, _(tooltip));

  /* Set up the widget so we can find it later */
  gtk_object_set_data (GTK_OBJECT (app), "mpeg_button",
		       button);

  gtk_widget_show (button);
}

static
void plugin_remove_gui (GnomeApp * app)
{
  GtkWidget * button = 
    GTK_WIDGET (gtk_object_get_data (GTK_OBJECT (app),
				   "mpeg_button"));
  GtkWidget * toolbar1 = lookup_widget (GTK_WIDGET (app), "toolbar1");

  gtk_container_remove (GTK_CONTAINER (toolbar1), button);
}

static void
plugin_process_popup_menu		 (GtkWidget	*widget,
					 GdkEventButton	*event,
					 GtkMenu	*popup)
{
  GtkWidget *menuitem;

  menuitem = gtk_menu_item_new ();
  gtk_widget_show (menuitem);
  gtk_menu_append (popup, menuitem);

  menuitem = z_gtk_pixmap_menu_item_new (_("Record as MPEG"),
					GNOME_STOCK_PIXMAP_COLORSELECTOR);
  z_tooltip_set (menuitem, _(tooltip));

  gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
		      (GtkSignalFunc) on_remote_command1,
		      (gpointer)((const gchar *) "record"));

  gtk_widget_show (menuitem);
  gtk_menu_append (popup, menuitem);
}

static
struct plugin_misc_info * plugin_get_misc_info (void)
{
  static struct plugin_misc_info returned_struct =
  {
    sizeof (struct plugin_misc_info), /* size of this struct */
    -10, /* plugin priority, we must be executed with a fully
	    processed image */
    /* Category */
    PLUGIN_CATEGORY_VIDEO_OUT |
    PLUGIN_CATEGORY_GUI
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
static gboolean
on_delete_event         		 (GtkWidget	*widget,
					 GdkEvent	*event,
					 gpointer	user_data)
{
  saving_dialog = NULL;

  do_stop ();
  
  return FALSE;
}

/*
 * Stop capturing pressed.
 */
static void
on_button_clicked			 (GtkButton	*button,
					 gpointer	user_data)
{
  g_assert (saving_dialog != NULL);

  gtk_widget_destroy (saving_dialog);
  saving_dialog = NULL;

  do_stop ();
}

#elif defined(HAVE_LIBRTE5)

#include <glade/glade.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <ctype.h>

#include "audio.h"
#include "mpeg.h"
#include "properties.h"

/*
  TODO:
    . options aren't checked at input time. problem eg.
      gop_seq, why bother the user while he's still typing.
      otoh we use the codec as temp storage and it wont
      accept any invalid options. 

    . Clipping/scaling init sequence:
      * user selects capture size (video window).
      * user selects coded width and height as options
      * x0 = display_width - width / 2, y0 accordingly.
      * repeat
        * zapping overlays border onto video.
	* user changes capture size (video window)
	  * continue
	*  user drags border, grabs line or corner.
	  * translate coordinates
	  * tentative rte_parameter_set, when accepted
            the border coordinates change, otherwise
	    only the pointer moves. Remind the parameters
	    snap to the closest possible.
      Border is nifty, we can re-use it elsewhere.
        Propose 1-pixel windows with X11 accelerated
        bg texture (eg. 16x16 with black/yellow 8x8 pattern)
        to keep it simple and fast. Add to osd.c.
      When we have a preview, one could use resizing that
        window instead of showing width and height options.
	Ha! cool. :-)
*/

static gchar *			zconf_root;
static gchar *			zconf_root_temp;

static gchar *			record_config_name;
static gchar *			record_option_filename;

/* Configuration */

static rte_context *		context_prop;
static GtkWidget *		audio_options;
static GtkWidget *		video_options;
static gint			capture_w = 384;
static gint			capture_h = 288;

/* Encoding */

static GtkWidget *		saving_dialog;
static GtkWidget *		saving_popup;

static volatile gboolean	active;

static rte_context *		context_enc;
static rte_codec *		audio_codec;
static rte_codec *		video_codec;

static rte_stream_parameters	audio_params;
static rte_stream_parameters	video_params;

static gint			update_timeout_id = -1;

static gpointer			audio_handle;
static void *			audio_buf;	/* preliminary */
static int			audio_size;

static tveng_device_info *	zapping_info;
static consumer			mpeg_consumer;

/*
 *  Input/output
 */

static rte_bool
audio_callback			(rte_context *		context,
				 rte_codec *		codec,
				 rte_buffer *		rb)
{
  rb->data = audio_buf;
  rb->size = audio_size;

  read_audio_data (audio_handle, rb->data, rb->size, &rb->timestamp);

  return TRUE;
}

static rte_bool
video_callback			(rte_context *		context,
				 rte_codec *		codec,
				 rte_buffer *		rb)
{
  buffer *b;
  struct tveng_frame_format *fmt;

  for (;;) {
    capture_buffer *cb = (capture_buffer *)
      (b = wait_full_buffer (&mpeg_consumer));

    fmt = &cb->d.format;

    if (cb->d.image_type &&
	fmt->height == video_params.video.height &&
	fmt->width == video_params.video.width &&
	/* fmt->sizeimage == context->video_bytes && */
	b->time)
      break;

    send_empty_buffer (&mpeg_consumer, b);
  }

  rb->timestamp = b->time;
  rb->data = b->data;
  rb->size = 1; /* XXX don't care 4 now */
  rb->user_data = b;

  return TRUE;
}

static rte_bool
video_unref			(rte_context *		context,
				 rte_codec *		codec,
				 rte_buffer *		rb)
{
  send_empty_buffer (&mpeg_consumer, (buffer *) rb->user_data);
  return TRUE;
}

/*
 *  Saving dialog status
 */

static inline unsigned int
qabs				(register int		n)
{
	register int t = n;

        t >>= 31;
	n ^= t;
	n -= t;

	return n;
}

/*
 *  Redraws the audio volume level bars when the widget
 *  is exposed (or needs an update).
 */
static gboolean
volume_expose			(GtkWidget *		widget,
				 GdkEventExpose *	event,
				 gpointer		data)
{
  gint max[2] = { 0, 0 };
  char *p, *e;
  gint w, h;

  /* ATTENTION S16LE assumed */
  for (p = ((char *) audio_buf) + 1,
	 e = (char *)(audio_buf + audio_size) - 2;
       p < e; p += 32)
    {
      gint n;

      n = qabs (p[0]);
      if (n > max[0])
	max[0] = n;

      n = qabs (p[2]);
      if (n > max[1])
	max[1] = n;
    }

  gdk_window_clear_area (widget->window,
			 event->area.x, event->area.y,
			 event->area.width, event->area.height);

  gdk_gc_set_clip_rectangle (widget->style->fg_gc[widget->state], &event->area);

  h = (widget->allocation.height - 1) >> 1;
  w = widget->allocation.width * max[0] / 128;

  if (audio_params.audio.channels == 1)
    {
      gdk_draw_rectangle (widget->window,
			  widget->style->fg_gc[widget->state],
			  TRUE, 0, h >> 1, MAX(1, w), h);
    }
  else 
    {
      gdk_draw_rectangle (widget->window,
			  widget->style->fg_gc[widget->state],
			  TRUE, 0, 0, MAX(1, w), h);

      w = widget->allocation.width * max[1] / 128;

      gdk_draw_rectangle (widget->window,
			  widget->style->fg_gc[widget->state],
			  TRUE, 0, h + 1, MAX(1, w), h);
    }

  gdk_gc_set_clip_rectangle (widget->style->fg_gc[widget->state], NULL);

  return TRUE;
}

/*
 *  Called by GTK timer to update saving window (time elapsed etc).
 */
static gint
saving_dialog_status_update		(rte_context *		context)
{
  static gchar buf[64];
  rte_status status;
  GtkWidget *widget;
  gint h, m, s;
  GtkRequisition req;

  if (!active || !saving_dialog)
    {
      update_timeout_id = -1;
      return FALSE;
    }

  if (audio_codec) {
    static gint cd;

    gtk_widget_queue_draw_area (lookup_widget (saving_dialog, "volume"),
				0, 0, 0x7FFF, 0x7FFF);
    if (cd-- <= 0)
      cd = 8 / 2;
    else
      return TRUE;
  }

  rte_context_status (context, &status);

  if (status.valid & RTE_STATUS_CODED_TIME)
    {
      s  = status.coded_time;
      m  = s / 60;
      s -= m * 60;
      h  = m / 60;
      m -= h * 60;
      h %= 99;

      snprintf(buf, sizeof(buf) - 1, "%02u:%02u:%02u", h, m, s);

      widget = lookup_widget (saving_dialog, "elapsed");
      gtk_label_set_text (GTK_LABEL (widget), buf);
    }

  if (status.valid & RTE_STATUS_BYTES_OUT)
    {
      snprintf(buf, sizeof(buf) - 1, "%.1f MB",
	       (status.bytes_out + ((1 << 20) / 10 - 1))
	       * (1.0 / (1 << 20)));

      widget = lookup_widget (saving_dialog, "bytes");
      gtk_label_set_text (GTK_LABEL (widget), buf);
    }

  /* more... */

  return TRUE;
}

static void
saving_dialog_status_disable		(void)
{
  if (update_timeout_id >= 0)
    {
      gtk_timeout_remove (update_timeout_id);
      update_timeout_id = -1;
    }
}

static void
saving_dialog_status_enable	(rte_context *		context)
{
  g_assert (saving_dialog != NULL);

  if (audio_codec)
    {
      gtk_signal_connect (GTK_OBJECT (lookup_widget (saving_dialog, "volume")),
			  "expose-event",
			  GTK_SIGNAL_FUNC (volume_expose),
			  NULL);

      update_timeout_id =
	gtk_timeout_add (1000 / 8, (GtkFunction) saving_dialog_status_update, context);
    }
  else
    {
      update_timeout_id =
	gtk_timeout_add (1000 / 2, (GtkFunction) saving_dialog_status_update, context);
    }
}

/*
 *  Start/stop recording
 */

static void
do_stop				(void)
{
  if (!active)
    return;

  saving_dialog_status_disable ();

  rte_context_delete (context_enc);
  context_enc = NULL;

  active = FALSE;

  rem_consumer (&mpeg_consumer);

  if (audio_handle)
    close_audio_device (audio_handle);
  audio_handle = NULL;

  if (audio_buf)
    free (audio_buf);
  audio_buf = NULL;

  capture_unlock ();
}

static gboolean
do_start			(const gchar *		file_name)
{
  rte_context *context;
  gdouble captured_frame_rate;
  gint width, height;

  if (active)
    return FALSE;

  /* sync_warning (void); */

  g_assert (context_enc == NULL);

  context = grte_context_load (zconf_root, record_config_name,
			       NULL,
			       &audio_codec, &video_codec,
			       &width, &height);

  if (!context)
    {
      ShowBox ("Couldn't create the encoding context",
	       GNOME_MESSAGE_BOX_ERROR);
fprintf(stderr, "##1\n");
      return FALSE;
    }

  if (!audio_codec && !video_codec)
    {
      ShowBox (_("Format %s is incomplete, please configure the video or audio codec."),
	       GNOME_MESSAGE_BOX_ERROR,
	       record_config_name);
      rte_context_delete (context);
fprintf(stderr, "##2\n");
      return FALSE;
    }

  context_enc = context;

  if (0)
    fprintf (stderr, "codecs: a%p v%p\n", audio_codec, video_codec);

  captured_frame_rate = 0.0;

  if (video_codec)
    {
      enum tveng_frame_pixformat tveng_pixformat;
      rte_video_stream_params *par = &video_params.video;

      memset (par, 0, sizeof (*par));

      if (zmisc_switch_mode (TVENG_CAPTURE_READ, zapping_info))
	{
	  rte_context_delete (context);
	  context_enc = NULL;

	  ShowBox ("This plugin needs to run in Capture mode, but"
		   " couldn't switch to that mode:\n%s",
		   GNOME_MESSAGE_BOX_INFO, zapping_info->error);
fprintf(stderr, "##3\n");
	  return FALSE;
	}

      tveng_pixformat =
	zconf_get_integer (NULL, "/zapping/options/main/yuv_format");

      if (!request_bundle_format (tveng_pixformat, width, height))
	{
	  rte_context_delete (context);
	  context_enc = NULL;

	  ShowBox ("Cannot switch to %s capture format",
		   GNOME_MESSAGE_BOX_ERROR,
		   (tveng_pixformat == TVENG_PIX_YVU420) ?
		   "YUV 4:2:0" : "YUV 4:2:2");
fprintf(stderr, "##4\n");
	  return FALSE;
	}

      par->width = zapping_info->format.width;
      par->height = zapping_info->format.height;

      if (tveng_pixformat == TVENG_PIX_YVU420)
	{
	  par->pixfmt = RTE_PIXFMT_YUV420;
	  par->stride = par->width;
	  par->uv_stride = par->stride >> 1;
	  par->v_offset = par->stride * par->height;
	  par->u_offset = par->v_offset * 5 / 4;
	}
      else
	{
	  par->pixfmt = RTE_PIXFMT_YUYV;
	  /* defaults */
	}

      if (!zapping_info->num_standards)
	{
	  rte_context_delete (context);
	  context_enc = NULL;

	  /* FIXME */

	  ShowBox ("Unable to determine current video standard",
		   GNOME_MESSAGE_BOX_ERROR);
fprintf(stderr, "##5\n");
	  return FALSE; 
	}

      captured_frame_rate = zapping_info->standards[
	zapping_info->cur_standard].frame_rate;

      /* g_assert */ (rte_codec_option_set (video_codec, "coded_frame_rate",
				      (double) captured_frame_rate));

      par->frame_rate = captured_frame_rate;

      if (!rte_parameters_set (video_codec, &video_params))
	{
	  rte_context_delete (context);
	  context_enc = NULL;

	  /* FIXME */

	  ShowBox ("Oops, catched a bug.",
		   GNOME_MESSAGE_BOX_ERROR);
fprintf(stderr, "##6\n");
	  return FALSE; 
	}
    }

  if (audio_codec)
    {
      rte_audio_stream_params *par = &audio_params.audio;

      memset (par, 0, sizeof (*par));

      /* XXX improve */

      g_assert (rte_parameters_set (audio_codec, &audio_params));

      audio_handle = open_audio_device (par->channels > 1,
					par->sampling_freq,
					AUDIO_FORMAT_S16_LE);
      if (!audio_handle)
	{
	  ShowBox ("Couldn't open the audio device",
		   GNOME_MESSAGE_BOX_ERROR);
fprintf(stderr, "##7\n");
	  goto failed;
	}

      if (!(audio_buf = malloc(audio_size = par->fragment_size)))
	{
	  ShowBox ("Couldn't open the audio device",
		   GNOME_MESSAGE_BOX_ERROR);
fprintf(stderr, "##8\n");
	  goto failed;
	}
    }

  if (audio_codec)
    { // XXX
      rte_set_input_callback_master (audio_codec, audio_callback, NULL, NULL);
    }

  if (video_codec)
    { // XXX
      rte_set_input_callback_master (video_codec, video_callback,
				     video_unref, NULL);
    }

  if (1)
    {
      gchar *dir = g_dirname(file_name);
      gchar *error_msg;

      if (!z_build_path (dir, &error_msg))
	{
	  ShowBox (_("Cannot create destination directory:\n%s\n%s"),
		   GNOME_MESSAGE_BOX_WARNING, dir, error_msg);
	  g_free (error_msg);
	  g_free (dir);
fprintf(stderr, "##9\n");
	  goto failed;
	}

      g_free(dir);

      if (!rte_set_output_file (context, file_name))
        {
	  // XXX more info please
	  ShowBox (_("Cannot create file %s: %s\n"),
		   GNOME_MESSAGE_BOX_WARNING,
		   file_name, rte_errstr (context));
fprintf(stderr, "##10\n");
	  goto failed;
	}
    }
  else
    {
      g_assert (rte_set_output_discard (context));
    }

  active = TRUE;

  /* don't let anyone mess with our settings from now on */
  capture_lock ();

  add_consumer (capture_fifo, &mpeg_consumer);

  if (!rte_start (context, 0.0, NULL, TRUE))
    {
      ShowBox ("Cannot start encoding: %s", GNOME_MESSAGE_BOX_ERROR,
	       rte_errstr (context));
      rem_consumer (&mpeg_consumer);
      capture_unlock ();
      active = FALSE;
fprintf(stderr, "##11\n");
      goto failed;
    }

  saving_dialog_status_enable (context);

  return TRUE;

 failed:
  if (context_enc)
    rte_context_delete (context_enc);
  context_enc = NULL;

  if (audio_buf)
    free (audio_buf);
  audio_buf = NULL;

  if (audio_handle)
    close_audio_device (audio_handle);
  audio_handle = NULL;
fprintf(stderr, "##12\n");

  return FALSE;
}

/*
 *  Record config menu
 */

static gchar *
escape_string			(gchar *		s)
{
  gchar *t;
  gint i, j, n;

  n = strlen (s);
  t = g_malloc (n * 3 + 1);

  for (i = j = 0; i < n; i++)
    if (isalnum (s[i]))
      t[j++] = s[i];
    else
      j += sprintf (t + j, "%%%02x", s[i] & 0xFF);

  t[j] = 0;

  return t;
}

static gchar *
restore_string			(gchar *		s)
{
  gchar *t;
  gint i, j, n;

  n = strlen (s);
  t = g_malloc (n + 1);

  for (i = j = 0; i < n; j++)
    if (s[i] == '%' && (n - i) >= 3)
      {
	t[j] = (s[i + 1] & 15) * 16 + (s[i + 2] & 15);

	if (s[i + 1] > '9') t[j] += 9 * 16;
	if (s[i + 2] > '9') t[j] += 9;

	i += 3;
      }
    else
      {
	t[j] = s[i++];
      }

  t[j] = 0;

  return t;
}

/**
 * record_config_menu_active:
 * @option_menu: 
 * 
 * Returns currently selected configuration. 
 * 
 * Return value: 
 * Static keyword (escaped config name).
 **/
static const gchar *
record_config_menu_active	(GtkWidget *		option_menu)
{
  GtkWidget *widget;

  widget = gtk_option_menu_get_menu (GTK_OPTION_MENU (option_menu));
  widget = gtk_menu_get_active (GTK_MENU (widget));

  if (!widget)
    return NULL;

  return gtk_object_get_data (GTK_OBJECT (widget), "keyword");
}

/**
 * record_config_menu_attach:
 * @source: 
 * @option_menu: 
 * @default_item: 
 * 
 * Rebuilds an @option_menu of configurations from zconf
 * <@source>/configs. Selects @default_item (escaped config name)
 * when it exists.
 * 
 * Return value: 
 * Number of configurations.
 **/
static gint
record_config_menu_attach	(const gchar *		source,
				 GtkWidget *		option_menu,
				 const gchar *		default_item)
{
  gchar *zcname = g_strconcat (source, "/configs", NULL);
  GtkWidget *menu;
  gchar *label;
  gint i, def, count;

  if ((menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (option_menu))))
    gtk_widget_destroy (menu);

  menu = gtk_menu_new ();

  def = 0;
  count = 0;

  for (i = 0; (label = zconf_get_nth (i, NULL, zcname)); i++)
    {
      gchar *base = g_basename (label);
      gchar *clear = restore_string (base);
      GtkWidget *menu_item;

      menu_item = gtk_menu_item_new_with_label (clear);

      gtk_widget_show (menu_item);
      gtk_object_set_data_full (GTK_OBJECT (menu_item), "keyword",
				g_strdup (base),
				(GtkDestroyNotify) g_free);
      gtk_menu_append (GTK_MENU (menu), menu_item);

      if (default_item)
	if (strcmp (base, default_item) == 0)
	  def = count;

      g_free (clear);

      count++;
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), def);

  g_free (zcname);

  return count;
}

/*
 *  Copy record configs (see below).
 */
static void
record_config_zconf_copy	(const gchar *		source,
				 const gchar *		dest)
{
  gchar *zcname = g_strconcat (source, "/configs", NULL);
  gchar *label;
  gint i;

  for (i = 0; (label = zconf_get_nth (i, NULL, zcname)); i++)
    {
      gchar *base = g_basename (label);
      rte_context *context;
      gint w, h;

      /* :-P */
      context = grte_context_load (source, base, NULL, NULL, NULL, &w, &h);
      grte_context_save (context, dest, base, w, h);
      rte_context_delete (context);
    }
}

/**
 * record_config_zconf_find:
 * @source: 
 * @name: 
 * 
 * Checks if configuration @name exists in zconf under
 * <@source>/configs.
 * 
 * Return value: 
 * Number of entry or -1 if not found.
 **/
static gint
record_config_zconf_find	(const gchar *		source,
				 const gchar *		name)
{
  gchar *zcname = g_strconcat (source, "/configs", NULL);
  gchar *label;
  gint i;

  for (i = 0; (label = zconf_get_nth (i, NULL, zcname)); i++)
    {
      gchar *base = g_basename (label);

      if (strcmp (base, name) == 0)
	{
	  g_free (zcname);
	  return i;
	}
    }

  g_free (zcname);
  return -1;
}

/*
 *  Format configuration
 */

static void
nullify				(void **		p)
{
  *p = NULL;
}

/**
 * select_codec:
 * @mpeg_properties: 
 * @conf_name: 
 * @keyword: 
 * @stream_type: 
 * 
 * Loads new #rte_codec @keyword of @stream_type into #context_prop,
 * with settings from <#zconf_root_temp>/configs/<@conf_name>, and
 * rebuilds the respective dialog options.
 **/
static void
select_codec			(GtkWidget *		mpeg_properties,
				 const gchar *		conf_name,
				 const char *		keyword,
				 rte_stream_type	stream_type)
{
  GtkWidget *vbox = 0;
  GtkWidget **optionspp = 0;
  rte_codec *codec;

  g_assert (mpeg_properties != NULL);
  g_assert (conf_name && conf_name[0]);

  switch (stream_type)
    {
      case RTE_STREAM_AUDIO:
	vbox = lookup_widget (mpeg_properties, "vbox13");
	optionspp = &audio_options;
	break;

      case RTE_STREAM_VIDEO:
	vbox = lookup_widget (mpeg_properties, "vbox12");
	optionspp = &video_options;
	break;

      default:
	g_assert_not_reached ();
    }

  g_assert (vbox);

  if (*optionspp)
    gtk_widget_destroy (*optionspp);
  *optionspp = NULL;

  if (keyword)
    {
      codec = grte_codec_load (context_prop, zconf_root_temp, conf_name,
			       stream_type, keyword);
      g_assert (codec);

      *optionspp = grte_options_create (context_prop, codec);

      if (*optionspp)
	{
	  gtk_widget_show (*optionspp);
	  gtk_box_pack_end (GTK_BOX (vbox), *optionspp, TRUE, TRUE, 3);
	  gtk_signal_connect_object (GTK_OBJECT (*optionspp), "destroy",
				     GTK_SIGNAL_FUNC (nullify),
				     (GtkObject *) optionspp);
	}
    }
  else
    {
      rte_remove_codec (context_prop, stream_type, 0);
    }
}

static void
on_audio_codec_changed		(GtkWidget *		menu,
				 GtkWidget *		mpeg_properties)
{
  GtkWidget *menu_item = gtk_menu_get_active (GTK_MENU (menu));
  char *keyword;

  g_assert (menu_item);
  keyword = gtk_object_get_data (GTK_OBJECT (menu_item), "keyword");
  select_codec (mpeg_properties, record_config_name, keyword, RTE_STREAM_AUDIO);
}

static void
on_video_codec_changed		(GtkWidget *		menu,
				 GtkWidget *		mpeg_properties)
{
  GtkWidget *menu_item = gtk_menu_get_active (GTK_MENU (menu));
  char *keyword;

  g_assert (menu_item);
  keyword = gtk_object_get_data (GTK_OBJECT (menu_item), "keyword");
  select_codec (mpeg_properties, record_config_name, keyword, RTE_STREAM_VIDEO);
}

static void
attach_codec_menu		(GtkWidget *		mpeg_properties,
				 gint			page,
				 const gchar *		widget_name,
				 const gchar *		conf_name,
				 rte_stream_type	stream_type)
{
  GtkWidget *menu, *menu_item;
  GtkWidget *widget, *notebook;
  gint default_item;
  void (* on_changed) (GtkWidget *, GtkWidget *) = 0;
  char *keyword;

  g_assert (mpeg_properties != NULL);

  if (!conf_name || !conf_name[0])
    return;

  switch (stream_type)
    {
      case RTE_STREAM_AUDIO:
	on_changed = on_audio_codec_changed;
	break;

      case RTE_STREAM_VIDEO:
	on_changed = on_video_codec_changed;
	break;

      default:
	g_assert_not_reached ();
    }

  notebook = lookup_widget (GTK_WIDGET (mpeg_properties), "notebook2");
  widget = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), page);

  switch (grte_num_codecs (context_prop, stream_type, NULL))
    {
    case 0:
      gtk_widget_set_sensitive (gtk_notebook_get_tab_label
				(GTK_NOTEBOOK (notebook), widget), FALSE);
      gtk_widget_set_sensitive (widget, FALSE);
      break;

    case 1:
    default:
      gtk_widget_set_sensitive (gtk_notebook_get_tab_label
				(GTK_NOTEBOOK (notebook), widget), TRUE);
      gtk_widget_set_sensitive (widget, TRUE);
      break;
    }

  widget = lookup_widget (mpeg_properties, widget_name);

  if ((menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (widget))))
    gtk_widget_destroy (menu);

  menu = grte_codec_create_menu (context_prop, zconf_root_temp, conf_name,
				 stream_type, &default_item);
  g_assert (menu);

  gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (widget), default_item);
  gtk_signal_connect (GTK_OBJECT (menu), "selection-done",
		      on_changed, mpeg_properties);

  menu_item = gtk_menu_get_active (GTK_MENU (menu));
  keyword = gtk_object_get_data (GTK_OBJECT (menu_item), "keyword");

  select_codec (mpeg_properties, conf_name, keyword, stream_type);
}

/**
 * select_file_format:
 * @mpeg_properties: 
 * @conf_name: 
 * @keyword: 
 * 
 * Loads new #rte_context @keyword into #context_prop, settings
 * from <#zconf_root_temp>/configs/<@conf_name>, and rebuilds the
 * config dialog codec pages.
 **/
static void
select_file_format		(GtkWidget *		mpeg_properties,
				 const gchar *		conf_name,
				 const char *		keyword)
{
  rte_context *context;

  g_assert (mpeg_properties != NULL);
  g_assert (conf_name && conf_name[0]);

  if (!keyword)
    return;

  context = rte_context_new (keyword, NULL, NULL);

  if (!context)
    return;

  if (context_prop)
    rte_context_delete (context_prop);

  context_prop = context;

  attach_codec_menu (mpeg_properties, 2, "optionmenu12", conf_name, RTE_STREAM_AUDIO);
  attach_codec_menu (mpeg_properties, 1, "optionmenu11", conf_name, RTE_STREAM_VIDEO);
}

static void
on_file_format_changed		(GtkWidget *		menu,
				 GtkWidget *		mpeg_properties)
{
  GtkWidget *menu_item = gtk_menu_get_active (GTK_MENU (menu));
  char *keyword = gtk_object_get_data (GTK_OBJECT (menu_item), "keyword");

  select_file_format (mpeg_properties, record_config_name, keyword);
}

/**
 * rebuild_config_dialog:
 * @mpeg_properties: 
 * @conf_name: 
 * 
 * Rebuilds configuration dialog with all settings loaded from
 * <#zconf_root_temp>/configs/<@conf_name>.
 **/
static void
rebuild_config_dialog		(GtkWidget *		mpeg_properties,
				 const gchar *		conf_name)
{
  GtkWidget *menu, *menu_item;
  GtkWidget *widget;
  gint default_item;

  g_assert (mpeg_properties != NULL);

  if (!conf_name || !conf_name[0])
    return;

  widget = lookup_widget (mpeg_properties, "context");

  if ((menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (widget))))
    gtk_widget_destroy (menu);

  menu = grte_context_create_menu (zconf_root_temp, conf_name, &default_item);

  g_assert (menu);

  gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (widget), default_item);
  gtk_signal_connect (GTK_OBJECT (menu), "selection-done",
		      on_file_format_changed, mpeg_properties);

  widget = gtk_menu_get_active (GTK_MENU (GTK_OPTION_MENU (widget)->menu));

  if (widget)
    {
      char *keyword;

      keyword = gtk_object_get_data (GTK_OBJECT (widget), "keyword");
  
      select_file_format (mpeg_properties, conf_name, keyword);
    }

  /* preliminary */
  {
    gchar *zcname;

    zcname = g_strconcat (zconf_root_temp, "/configs/", conf_name, "/capture_width", NULL);
    zconf_create_integer (384, "Capture width", zcname);
    zconf_get_integer (&capture_w, zcname);
    g_free (zcname);
    widget = lookup_widget (mpeg_properties, "spinbutton9");
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), capture_w);

    zcname = g_strconcat (zconf_root_temp, "/configs/", conf_name, "/capture_height", NULL);
    zconf_create_integer (288, "Capture height", zcname);
    zconf_get_integer (&capture_h, zcname);
    g_free (zcname);
    widget = lookup_widget (mpeg_properties, "spinbutton10");
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), capture_h);
  }
}

/**
 * pref_rebuild_configs:
 * @page: 
 * @default_item: 
 * 
 * Rebuilds preferences configs menu, selects @default_item (escaped
 * config name) if exists and rebuilds config dialog loading the
 * selected configuration from <#zconf_root_temp>/configs.
 **/
static void
pref_rebuild_configs		(GtkWidget *		page,
				 const gchar *		default_item)
{
  static void on_pref_config_changed (GtkWidget *, GtkWidget *);
  GtkWidget *configs = lookup_widget (page, "optionmenu15");
  gchar *why;
  gint nformats;

  nformats = record_config_menu_attach (zconf_root_temp, configs, default_item);

  gtk_signal_connect (GTK_OBJECT (GTK_OPTION_MENU (configs)->menu),
		      "selection-done",
		      GTK_SIGNAL_FUNC (on_pref_config_changed), page);

  why = NULL;
  z_set_sensitive_with_tooltip (configs, nformats > 0, NULL, why);
  z_set_sensitive_with_tooltip (lookup_widget (page, "delete"), nformats > 0, NULL, why);
  z_set_sensitive_with_tooltip (lookup_widget (page, "notebook2"), nformats > 0, NULL, why);

  if (nformats > 0)
    on_pref_config_changed (NULL, page);
}

static void
pref_cancel			(GtkWidget *		page)
{
  /* Delete preliminary changes */

  zconf_delete (zconf_root_temp);
}

static void
save_current_config		(GtkWidget *		page)
{
  if (context_prop && record_config_name[0])
    {
      GtkWidget *widget;

      widget = lookup_widget (page, "spinbutton9");
      capture_w = ~15 & gtk_spin_button_get_value_as_int
	(GTK_SPIN_BUTTON (widget));

      widget = lookup_widget (page, "spinbutton10");
      capture_h = ~15 & gtk_spin_button_get_value_as_int
	(GTK_SPIN_BUTTON (widget));

      grte_context_save (context_prop,
			 zconf_root_temp, record_config_name,
			 capture_w, capture_h);
    }
}

static void
pref_apply			(GtkWidget *		page)
{
  gchar *buffer;

  save_current_config (page);

  /* Replace old configs by changed ones and delete temp */

  buffer = g_strconcat (zconf_root, "/configs", NULL);
  zconf_delete (buffer);
  g_free (buffer);

  record_config_zconf_copy (zconf_root_temp, zconf_root);

  zconf_delete (zconf_root_temp);
}

static void
on_config_new_clicked			(GtkButton *		button,
					 GtkWidget *		page)
{
  GtkWidget *dialog;
  GtkEntry *entry;
  gchar *name;
  gint n;

  dialog = build_widget ("dialog3", "mpeg_properties.glade");

  entry = GTK_ENTRY (lookup_widget (dialog, "entry2"));

  gtk_widget_grab_focus (GTK_WIDGET (entry));

  gnome_dialog_editable_enters (GNOME_DIALOG (dialog), GTK_EDITABLE (entry));
  gnome_dialog_set_default (GNOME_DIALOG (dialog), 0);
  gnome_dialog_set_parent(GNOME_DIALOG(dialog), z_main_window());

  switch (gnome_dialog_run (GNOME_DIALOG (dialog)))
    {
    case 0: /* OK */
      name = gtk_entry_get_text (entry);

      if (!name || !name[0])
	break;

      name = escape_string (name);

      if (record_config_zconf_find (zconf_root_temp, name) >= 0)
	{
	  if (strcmp (name, record_config_name) != 0)
	    {
	      /* Rebuild configs menu, select new and save current */
	      pref_rebuild_configs (page, name);
	    }
	}
      else
	{
	  /*
	   *  Create new config (clone current), rebuild configs
	   *  menu, select new and save current config.
	   */
	  if (!context_prop)
	    rebuild_config_dialog (page, name);

	  if (context_prop)
	    grte_context_save (context_prop, zconf_root_temp, name,
			       capture_w, capture_h);

	  pref_rebuild_configs (page, name);

	  z_property_item_modified (page);
	}

      g_free (name);

      break;

    default: /* Cancel */
      break;
    }

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
on_config_delete_clicked		(GtkButton *		button,
					 GtkWidget *		page)
{
  /*
   *  If config selected delete it, rebuild configs menu and
   *  config dialog, and select some other config.
   */

  if (!record_config_name[0])
    return;

  grte_config_delete (zconf_root_temp, record_config_name);
  g_free (record_config_name);
  record_config_name = g_strdup ("");

  pref_rebuild_configs (page, NULL);

  z_property_item_modified (page);
}

static void
on_pref_config_changed		(GtkWidget *		menu,
				 GtkWidget *		page)
{
  GtkWidget *configs = lookup_widget (page, "optionmenu15");
  const gchar *conf = record_config_menu_active (configs);

  if (!conf || strcmp(conf, record_config_name) == 0)
    return;

  save_current_config (page);

  g_free (record_config_name);
  record_config_name = g_strdup (conf);

  rebuild_config_dialog (page, record_config_name);
}

static void
pref_setup			(GtkWidget *		page)
{
  GtkWidget *new = lookup_widget (page, "new");
  GtkWidget *delete = lookup_widget (page, "delete");
  GtkWidget *configs = lookup_widget (page, "optionmenu15");

  /* All changes preliminary until ok/apply */
  record_config_zconf_copy (zconf_root, zconf_root_temp);

  pref_rebuild_configs (page, record_config_name);

  /* Load context_prop even if nothing configured yet */
  rebuild_config_dialog (page, record_config_name);

  gtk_signal_connect (GTK_OBJECT (new), "clicked",
		      GTK_SIGNAL_FUNC (on_config_new_clicked), page);
  gtk_signal_connect (GTK_OBJECT (delete), "clicked",
		      GTK_SIGNAL_FUNC (on_config_delete_clicked), page);
}

/*
 *  Saving dialog
 */

static gboolean
on_saving_delete_event		(GtkWidget *		widget,
				 GdkEvent *		event,
				 gpointer		user_data)
{
  saving_dialog = NULL;

  do_stop ();
  
  return FALSE;
}

static gchar *
file_format_ext			(const gchar *		conf_name)
{
  rte_context *context;
  rte_context_info *info;
  const gchar *s;

  if (!conf_name || !conf_name[0])
    return NULL;

  context = grte_context_load (zconf_root, conf_name, NULL, NULL, NULL, NULL, NULL);

  if (!context)
    return NULL;

  info = rte_context_info_by_context (context);

  if (!info->extension)
    {
      rte_context_delete (context);
      return NULL;
    }

  for (s = info->extension; *s != 0 && *s != ','; s++)
    ;

  return g_strndup (info->extension, s - info->extension);
}

static void
on_saving_format_changed   	(GtkWidget *		menu,
				 gpointer		user_data)
{
  GtkWidget *configs;
  GtkWidget *entry;
  gchar *ext;
  gchar *name;

  g_assert (saving_dialog != NULL);

  /* Replace filename extension in entry according to selected format */

  configs = lookup_widget (saving_dialog, "optionmenu14");
  entry = lookup_widget (saving_dialog, "entry1");

  ext = file_format_ext (record_config_menu_active (configs));

  name = gtk_object_get_data (GTK_OBJECT (entry), "basename");
  name = z_replace_filename_extension (name, ext);
  gtk_object_set_data_full (GTK_OBJECT (entry), "basename",
			    name, (GtkDestroyNotify) g_free);

  name = gtk_entry_get_text (GTK_ENTRY (entry));
  name = z_replace_filename_extension (name , ext);
  gtk_entry_set_text (GTK_ENTRY (entry), name);
  g_free (name);

  g_free (ext);
}

static void
on_saving_configure_clicked	(GtkButton *		button,
				 gpointer		user_data)
{
  static void saving_dialog_attach_formats (void);
  GtkWidget *properties;

  g_assert (saving_dialog != NULL);

  gtk_widget_set_sensitive (saving_dialog, FALSE);

  properties = build_properties_dialog ();
  open_properties_page (properties, _("Plugins"), _("Record"));
  gnome_dialog_run (GNOME_DIALOG (properties));

  gtk_widget_set_sensitive (saving_dialog, TRUE);

  saving_dialog_attach_formats ();

  /* The former disconnects */
  gtk_signal_connect (GTK_OBJECT (GTK_OPTION_MENU
				  (lookup_widget (saving_dialog,
						  "optionmenu14"))->menu),
		      "selection-done",
		      GTK_SIGNAL_FUNC (on_saving_format_changed), NULL);
}

static void
on_saving_filename_changed   	(GtkWidget *		widget,
				 gpointer		user_data)
{
  gchar *buffer;

  g_assert (saving_dialog != NULL);

  /* Inhibit record button when filename is blank */

  buffer = gtk_editable_get_chars (GTK_EDITABLE (widget), 0, -1);
  gtk_widget_set_sensitive (lookup_widget (saving_dialog, "record"),
			    buffer && buffer[0]);
  g_free (buffer);
}

static void
on_saving_stop_clicked		(GtkButton *		button,
				 gpointer		user_data)
{
  g_assert (saving_dialog != NULL);

  gtk_widget_destroy (saving_dialog);
  saving_dialog = NULL;

  do_stop ();
}

static void
on_saving_record_clicked	(GtkButton *		button,
				 gpointer		user_data)
{
  GtkToggleButton *record;
  GtkWidget *widget;
  const gchar *buffer;
fprintf(stderr, "GGGGG\n");

  g_assert (saving_dialog != NULL);

  if (active)
    return;

  record = GTK_TOGGLE_BUTTON (lookup_widget (saving_dialog, "record"));

  widget = lookup_widget (saving_dialog, "optionmenu14");
  buffer = record_config_menu_active (widget);

  if (!buffer || !buffer[0])
    goto reject;

  g_free (record_config_name);
  record_config_name = g_strdup (buffer);

  widget = lookup_widget (saving_dialog, "entry1");
  buffer = gtk_entry_get_text (GTK_ENTRY (widget));

  if (!buffer || !buffer[0])
    goto reject;

  g_free (record_option_filename);
  record_option_filename = g_strdup (buffer);

  if (0)
    fprintf(stderr, "Record <%s> as <%s>\n",
	    record_option_filename,
	    record_config_name);

  if (do_start (record_option_filename))
    {
      z_set_sensitive_with_tooltip (lookup_widget (saving_dialog, "optionmenu14"),
				    FALSE, NULL, NULL);

      gtk_widget_set_sensitive (lookup_widget (saving_dialog, "fileentry3"), FALSE);
      gtk_widget_set_sensitive (GTK_WIDGET (record), FALSE);
      gtk_widget_set_sensitive (lookup_widget (saving_dialog, "stop"), TRUE);
    }
  else
    {
      gtk_toggle_button_set_active (record, FALSE);
    }

  return;

 reject:
  gtk_toggle_button_set_active (record, FALSE);
  return;
}

static void
saving_dialog_attach_formats	(void)
{
  GtkWidget *configs = lookup_widget (saving_dialog, "optionmenu14");
  GtkWidget *entry = lookup_widget (saving_dialog, "entry1");
  gint nformats;
  gchar *ext;
  gchar *name;

  nformats = record_config_menu_attach (zconf_root, configs, NULL);
  z_set_sensitive_with_tooltip (configs, nformats > 0, NULL, NULL);

  /*
   *  Proposed filename is last one, with appropriate extension
   *  and numeral suffix++ if file exists.
   */

  ext = file_format_ext (record_config_menu_active (configs));
  name = find_unused_name (NULL, record_option_filename, ext);

  gtk_entry_set_text (GTK_ENTRY (entry), name);
  gtk_object_set_data_full (GTK_OBJECT (entry), "basename",
			    g_strdup (g_basename (name)),
			    (GtkDestroyNotify) g_free);
  g_free (name);
  g_free (ext);

  gtk_signal_connect (GTK_OBJECT (entry), "changed",
		      GTK_SIGNAL_FUNC (z_on_electric_filename),
		      (gpointer) NULL);

  gtk_entry_select_region (GTK_ENTRY (entry), 0, -1);

  if (nformats > 0)
    {
      if (!active)
	gtk_widget_set_sensitive (lookup_widget (saving_dialog, "record"), TRUE);
    }
  else
    {
      gtk_widget_set_sensitive (lookup_widget (saving_dialog, "record"), FALSE);
    }
}

static GtkWidget *
saving_popup_new		(void)
{
  static gint times[] = { 15, 30, 45, 60, 90, 120, 180 };
  GtkWidget *menu;
  gint i;

  menu = build_widget ("menu1", "mpeg_properties.glade");

  gtk_widget_set_sensitive (lookup_widget (menu, "record_this"), FALSE);
  gtk_widget_set_sensitive (lookup_widget (menu, "record_next"), FALSE);

  for (i = 0; i < sizeof(times) / sizeof (times[0]); i++)
    {
      GtkWidget *menu_item;
      gchar *buffer = g_strdup_printf("Record %d min", times[i]);

      menu_item = gtk_menu_item_new_with_label (buffer);
      g_free (buffer);

      gtk_widget_show (menu_item);
      gtk_menu_append (GTK_MENU (menu), menu_item);
    }

  return menu;
}

static void
saving_dialog_delete		(void)
{
  if (saving_dialog)
    {
      gtk_widget_destroy (saving_dialog);
      saving_dialog = NULL;
    }
}

static void
saving_dialog_new		(gboolean		recording)
{
  GtkWidget *widget, *pixmap;
  GtkWidget *dialog, *label;
  GtkWidget *record, *stop;
  gchar *buffer, *filename;
  gint nformats;

  if (saving_dialog)
    gtk_widget_destroy (saving_dialog);

  saving_dialog = build_widget ("window3", "mpeg_properties.glade");

  if ((pixmap = z_load_pixmap ("time.png")))
    gtk_table_attach (GTK_TABLE (lookup_widget (saving_dialog, "table4")),
		      pixmap, 0, 1, 0, 1, 0, 0, 3, 0);
  if ((pixmap = z_load_pixmap ("drop.png")))
    gtk_table_attach (GTK_TABLE (lookup_widget (saving_dialog, "table5")),
		      pixmap, 0, 1, 0, 1, 0, 0, 3, 0);
  if ((pixmap = z_load_pixmap ("disk_empty.png")))
    gtk_table_attach (GTK_TABLE (lookup_widget (saving_dialog, "table7")),
		      pixmap, 0, 1, 0, 1, 0, 0, 3, 0);
  if ((pixmap = z_load_pixmap ("volume.png")))
    gtk_table_attach (GTK_TABLE (lookup_widget (saving_dialog, "table8")),
		      pixmap, 0, 1, 0, 1, 0, 0, 3, 0);

  if ((pixmap = z_load_pixmap ("record.png")))
   gtk_box_pack_start (GTK_BOX (lookup_widget (saving_dialog, "hbox20")),
		       pixmap, FALSE, FALSE, 0);
  if ((pixmap = z_load_pixmap ("pause.png")))
   gtk_box_pack_start (GTK_BOX (lookup_widget (saving_dialog, "hbox22")),
		       pixmap, FALSE, FALSE, 0);
  if ((pixmap = z_load_pixmap ("stop.png")))
   gtk_box_pack_start (GTK_BOX (lookup_widget (saving_dialog, "hbox24")),
		       pixmap, FALSE, FALSE, 0);

  saving_dialog_attach_formats ();
  /*
  gnome_popup_menu_attach (saving_popup = saving_popup_new (),
			   saving_dialog, NULL);
  */
  gtk_signal_connect (GTK_OBJECT (saving_dialog),
		      "delete-event",
		      GTK_SIGNAL_FUNC (on_saving_delete_event), NULL);
  gtk_signal_connect (GTK_OBJECT (GTK_OPTION_MENU
				  (lookup_widget (saving_dialog,
						  "optionmenu14"))->menu),
		      "selection-done",
		      GTK_SIGNAL_FUNC (on_saving_format_changed), NULL);
  gtk_signal_connect (GTK_OBJECT (lookup_widget (saving_dialog, "configure_format")),
		      "clicked",
		      GTK_SIGNAL_FUNC (on_saving_configure_clicked), NULL);
  gtk_signal_connect (GTK_OBJECT (lookup_widget (saving_dialog, "entry1")),
		      "changed",
		      GTK_SIGNAL_FUNC (on_saving_filename_changed), NULL);

  record = lookup_widget (saving_dialog, "record");
  if (recording) {
    gtk_toggle_button_set_active (record, TRUE);
    gtk_widget_set_sensitive (record, FALSE);
  }
  gtk_signal_connect (GTK_OBJECT (record), "clicked",
		      GTK_SIGNAL_FUNC (on_saving_record_clicked), NULL);

  stop = lookup_widget (saving_dialog, "stop");
  gtk_signal_connect (GTK_OBJECT (stop), "clicked",
		      GTK_SIGNAL_FUNC (on_saving_stop_clicked), NULL);
  gtk_widget_set_sensitive (stop, recording);

  widget = lookup_widget (saving_dialog, "pause");
  gtk_widget_set_sensitive (widget, FALSE);
  z_tooltip_set (widget, _("Not implemented yet"));

  if (recording) {
    z_set_sensitive_with_tooltip (lookup_widget (saving_dialog, "optionmenu14"),
				  FALSE, NULL, NULL);
    gtk_widget_set_sensitive (lookup_widget (saving_dialog, "fileentry3"), FALSE);
  }

  gtk_widget_show (saving_dialog);
}

/*
 *  Command interface
 */

static gboolean
stoprec_cmd			(GtkWidget *		widget,
				 gint			argc,
				 gchar **		argv,
				 gpointer		user_data)
{
  saving_dialog_delete ();

  do_stop ();

  return TRUE;
}

static gboolean
pauserec_cmd			(GtkWidget *		widget,
				 gint			argc,
				 gchar **		argv,
				 gpointer		user_data)
{
  return FALSE;
}

static gboolean
quickrec_cmd			(GtkWidget *		widget,
				 gint			argc,
				 gchar **		argv,
				 gpointer		user_data)
{
  gchar *ext;
  gchar *name;
  gchar *path;
  gboolean success;

  if (saving_dialog || active)
    return FALSE;

  if (!record_config_name[0])
    return FALSE;

  if (!record_option_filename[0])
    {
      g_free (record_option_filename);
      record_option_filename = g_strconcat (getenv ("HOME"), "/clips/clip1", NULL);
    }

  ext = file_format_ext (record_config_name);
  name = find_unused_name (NULL, record_option_filename, ext);

  saving_dialog_new (TRUE);

  success = do_start (name);

  if (success) {
    GtkToggleButton *record;

    record = GTK_TOGGLE_BUTTON (lookup_widget (saving_dialog, "record"));

  } else {
    saving_dialog_delete ();
  }

  g_free (name);
  g_free (ext);

  return success;
}

static gboolean
record_cmd			(GtkWidget *		widget,
				 gint			argc,
				 gchar **		argv,
				 gpointer		user_data)
{
fprintf(stderr, "EEEE\n");

  saving_dialog_new (FALSE);

  return TRUE;
}

/*
 *  Save config
 */

#define SAVE_CONFIG(_type, _name)					\
  buffer = g_strconcat (root_key, #_name, NULL);			\
  zconf_set_##_type (record_option_##_name, buffer);			\
  g_free (buffer);

static void
plugin_save_config		(gchar *		root_key)
{
  gchar *buffer;

  buffer = g_strconcat (root_key, "config", NULL);
  zconf_set_string (record_config_name, buffer);
  g_free (buffer);
  g_free (record_config_name);
  record_config_name = NULL;

  SAVE_CONFIG (string, filename);
  g_free (record_option_filename);
  record_option_filename = NULL;

  /* Obsolete pre-RTE 0.5 configuration */
  /* zconf_delete ("/zapping/plugins/mpeg"); */
}

#define LOAD_CONFIG(_type, _def, _name, _descr)				\
  buffer = g_strconcat (root_key, #_name, NULL);			\
  zconf_create_##_type (_def, _descr, buffer);				\
  zconf_get_##_type (&record_option_##_name, buffer);			\
  g_free (buffer);

static void
plugin_load_config		(gchar *		root_key)
{
  gchar *buffer;
  gchar *default_filename;
  gint n = strlen (root_key);

  g_assert (n > 0 && root_key[n - 1] == '/');
  zconf_root = strndup (root_key, n - 1);
  zconf_root_temp = g_strconcat (zconf_root, "/temp", NULL);

  buffer = g_strconcat (root_key, "config", NULL);
  zconf_create_string ("", "Last config", buffer);
  zconf_get_string (&record_config_name, buffer);
  g_free (buffer);

  default_filename = g_strconcat (getenv ("HOME"), "/clips/clip1", NULL);
  LOAD_CONFIG (string, default_filename, filename, "Last filename");
  g_free (default_filename);
}

/*
 *  Plugin interface
 */

static gboolean
plugin_running			(void)
{
  return active;
}

static void
plugin_capture_stop		(void)
{
  saving_dialog_delete ();

  do_stop ();
}

static gboolean
plugin_start			(void)
{
  saving_dialog_new (FALSE);

  return TRUE;
}

static void
plugin_remove_gui		(GnomeApp *		app)
{
  GtkWidget *button = 
    GTK_WIDGET (gtk_object_get_data (GTK_OBJECT (app), "mpeg_button"));
  GtkWidget *toolbar1 = lookup_widget (GTK_WIDGET (app), "toolbar1");

  gtk_container_remove (GTK_CONTAINER (toolbar1), button);
}

static const gchar *tooltip = N_("Recording dialog");

static void
plugin_add_gui			(GnomeApp *		app)
{
  GtkWidget *toolbar1 = lookup_widget (GTK_WIDGET (app), "toolbar1");
  GtkWidget *button; /* The button to add */
  GtkWidget *tmp_toolbar_icon;
  gint sig_id;

  tmp_toolbar_icon =
    gnome_stock_pixmap_widget (GTK_WIDGET (app),
			       GNOME_STOCK_PIXMAP_COLORSELECTOR);

  button = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar1),
				       GTK_TOOLBAR_CHILD_BUTTON, NULL,
				       _("Record"),
				       NULL, NULL, tmp_toolbar_icon,
				       on_remote_command1,
				       (gpointer)((const gchar *) "record"));

  z_tooltip_set (button, _(tooltip));

  /* Set up the widget so we can find it later */
  gtk_object_set_data (GTK_OBJECT (app), "mpeg_button", button);

  gtk_widget_show (button);
}

static void
plugin_process_popup_menu	(GtkWidget *		widget,
				 GdkEventButton	*	event,
				 GtkMenu *		popup)
{
  GtkWidget *menuitem;

  menuitem = gtk_menu_item_new ();
  gtk_widget_show (menuitem);
  gtk_menu_append (popup, menuitem);

  menuitem = z_gtk_pixmap_menu_item_new (_("Record"),
					 GNOME_STOCK_PIXMAP_COLORSELECTOR);
  z_tooltip_set (menuitem, _(tooltip));

  gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
		      (GtkSignalFunc) on_remote_command1,
		      (gpointer)((const gchar *) "record"));

  gtk_widget_show (menuitem);
  gtk_menu_append (popup, menuitem);
}


static struct plugin_misc_info *
plugin_get_misc_info		(void)
{
  static struct plugin_misc_info returned_struct = {
    sizeof (struct plugin_misc_info), /* size of this struct */
    -10, /* plugin priority, we must be executed with a fully
	    processed image */
    /* Category */
    PLUGIN_CATEGORY_VIDEO_OUT |
    PLUGIN_CATEGORY_GUI
  };

  return (&returned_struct);
}

static gboolean
plugin_get_public_info		(gint			index,
				 gpointer *		ptr,
				 gchar **		symbol,
				 gchar **		description,
				 gchar **		type,
				 gint *			hash)
{
  return FALSE; /* Nothing exported */
}

static void
plugin_get_info			(const gchar **		canonical_name,
				 const gchar **		descriptive_name,
				 const gchar **		description,
				 const gchar **		short_description,
				 const gchar **		author,
				 const gchar **		version)
{
  if (canonical_name)
    *canonical_name = "record";

  if (descriptive_name)
    *descriptive_name = _("Audio/Video Recorder");

  if (description)
    *description = _("This plugin records video and audio into a file");

  if (short_description)
    *short_description = _("Record video and audio.");

  if (author)
    *author = "Iñaki García Etxebarria, Michael H. Schimek";

  if (version)
    *version = "0.3";
}

static void
plugin_close			(void)
{
  // XXX order?

  cmd_remove ("record");
  cmd_remove ("quickrec");
  cmd_remove ("pauserec");
  cmd_remove ("stoprec");

  saving_dialog_delete ();

  if (context_prop)
    rte_context_delete (context_prop);
  context_prop = NULL;

  if (active)
    {
      if (context_enc)
	rte_context_delete (context_enc);
      context_enc = NULL;

      if (audio_buf)
	free (audio_buf);
      audio_buf = NULL;

      if (audio_handle)
	close_audio_device (audio_handle);
      audio_handle = NULL;

      active = FALSE;
    }

  g_free (zconf_root);
  g_free (zconf_root_temp);
  zconf_root = NULL;
  zconf_root_temp = NULL;
}

static void
properties_add			(GnomeDialog *		dialog)
{
  SidebarEntry plugin_options[] = {
    {
      N_("Record"),
      ICON_ZAPPING,
      "gnome-media-player.png",
      "vbox9",
      pref_setup,
      pref_apply,
      NULL, /* help */
      pref_cancel
    }
  };
  SidebarGroup groups[] = {
    {
      N_("Plugins"),
      plugin_options,
      1
    }
  };

  standard_properties_add (dialog, groups, acount(groups),
			   "mpeg_properties.glade");
}

static gboolean
plugin_init			(PluginBridge		bridge,
				 tveng_device_info *	info)
{
  property_handler mpeg_handler = {
    add: properties_add
  };

  zapping_info = info;

  append_property_handler (&mpeg_handler);

  cmd_register ("record", record_cmd, NULL);
  cmd_register ("quickrec", quickrec_cmd, NULL);
  cmd_register ("pauserec", pauserec_cmd, NULL);
  cmd_register ("stoprec", stoprec_cmd, NULL);

  return TRUE;
}

gboolean
plugin_get_symbol		(gchar *		name,
				 gint			hash,
				 gpointer *		ptr)
{
  struct plugin_exported_symbol table_of_symbols[] = {
    SYMBOL (plugin_init, 0x1234),
    SYMBOL (plugin_get_info, 0x1234),
    SYMBOL (plugin_close, 0x1234),
    SYMBOL (plugin_start, 0x1234),
    SYMBOL (plugin_load_config, 0x1234),
    SYMBOL (plugin_save_config, 0x1234),
    SYMBOL (plugin_capture_stop, 0x1234),
    SYMBOL (plugin_get_public_info, 0x1234),
    SYMBOL (plugin_add_gui, 0x1234),
    SYMBOL (plugin_remove_gui, 0x1234),
    SYMBOL (plugin_get_misc_info, 0x1234),
    SYMBOL (plugin_running, 0x1234),
    SYMBOL (plugin_process_popup_menu, 0x1234)
  };
  gint num_exported_symbols =
    sizeof (table_of_symbols) / sizeof (struct plugin_exported_symbol);
  gint i;

  for (i = 0; i < num_exported_symbols; i++)
    if (!strcmp (table_of_symbols[i].symbol, name))
      {
	if (table_of_symbols[i].hash != hash)
	  {
	    if (ptr)
	      *ptr = GINT_TO_POINTER (0x3); /* hash collision code */

	    g_warning (_("Check error: \"%s\" in plugin %s"
			 " has hash 0x%x vs. 0x%x"),
		       name, "record", 
		       table_of_symbols[i].hash, hash);

	    return FALSE;
	  }

	if (ptr)
	  *ptr = table_of_symbols[i].ptr;

	return TRUE; /* Success */
      }

  if (ptr)
    *ptr = GINT_TO_POINTER (0x2); /* Symbol not found in the plugin */

  return FALSE;
}

gint
plugin_get_protocol		(void)
{
  return PLUGIN_PROTOCOL;
}

#else

/**
 * Load the plugin saying that it has been disabled due to RTE
 * missing, and tell the place to get it, with a handy "Goto.." button.
 */

#endif /* HAVE_LIBRTE4 || HAVE_LIBRTE5 */
