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

/* $Id: mpeg.c,v 1.31 2002-03-06 00:53:49 mschimek Exp $ */

#include "plugin_common.h"

#ifdef HAVE_LIBRTE

#include <glade/glade.h>

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
static gint update_timeout_id;
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
	rte_context_destroy (context_enc);
      context_enc = NULL;

      if (context_prop)
	rte_context_destroy (context_prop);
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

  context = grte_context_load (MPEG_CONFIG, NULL, &audio_codec, &video_codec);

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

      g_assert (rte_option_set (video_codec, "coded_frame_rate",
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

  saving_dialog =
    build_widget ("dialog1", PACKAGE_DATA_DIR
		 "/mpeg_properties.glade");

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

  rte_context_destroy (context_enc);
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
    grte_context_save (context_prop, MPEG_CONFIG);
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
      codec = grte_codec_load (context_prop, MPEG_CONFIG, stream_type, keyword);
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

  menu = grte_codec_create_menu (context_prop, MPEG_CONFIG, stream_type, &default_item);

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
    rte_context_destroy (context_prop);

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

  menu = grte_context_create_menu (MPEG_CONFIG, &default_item);

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
    grte_context_save (context_prop, MPEG_CONFIG);

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
			  PACKAGE_DATA_DIR "/mpeg_properties.glade");
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

  if (!mpeg_configured())
    return FALSE;

  if (output_mode != OUTPUT_FILE)
    {
      plugin_start();
      return FALSE;
    }

  dialog = build_widget("dialog2",
			PACKAGE_DATA_DIR "/mpeg_properties.glade");
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
				       _(tooltip),
				       NULL, tmp_toolbar_icon,
				       on_remote_command1,
				       (gpointer)((const gchar *) "record"));

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
  set_tooltip (menuitem, _(tooltip));

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
#else
/**
 * Load the plugin saying that it has been disabled due to RTE
 * missing, and tell the place to get it, with a handy "Goto.." button.
 */
#endif
