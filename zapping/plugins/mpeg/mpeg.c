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
static volatile gboolean active = FALSE;
/* The context we are encoding to */
static rte_context * context = NULL;
/* Info about the video device */
static tveng_device_info * zapping_info = NULL;
/* Pointer to the dialog that appears while saving */
static GtkWidget * saving_dialog = NULL;
/* Show the warning about sync with V4L */
static gboolean lip_sync_warning = TRUE;
/* The consumer for capture_fifo */
static consumer mpeg_consumer;
/* This updates the GUI */
static gint update_timeout_id;

/* Plugin options */
/* Compressor options */
static gdouble output_video_bits;
static gdouble output_audio_bits;
static gint engine_verbosity;
/* I/O */
static gchar* save_dir;
static gint output_mode; /* 0:file, 1:/dev/null */
static gint mux_mode; /* 0:audio, 1:video, 2:both */
static gint capture_w, capture_h;
static gboolean motion_comp; /* bool */

static gboolean on_delete_event(GtkWidget *widget, GdkEvent
				*event, gpointer user_data);
static void on_button_clicked(GtkButton *button, gpointer user_data);

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
    SYMBOL(plugin_get_misc_info, 0x1234),
    SYMBOL(plugin_running, 0x1234)
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
      /* FIXME: This could hang */
      if (context)
	context = rte_context_destroy(context);
      active = FALSE;
    }
}

/*
 * Audio capture
 */
static int		esd_recording_socket = -1;

static void
read_audio(void * data, double * time, rte_context * context)
{
	int stereo = (context->audio_mode == RTE_AUDIO_MODE_STEREO) ? 1
		: 0;
	struct timeval tv;
	int sampling_rate = context->audio_rate;
	ssize_t r, n = context->audio_bytes;

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
		
		r = read(esd_recording_socket, data, n);
		
		if (r < 0 && errno == EINTR)
			continue;
		
		if (r == 0) {
			memset(data, 0, n);
			break;
		}
		
		g_assert(r > 0);
		
		(char *) data += r;
		n -= r;
	}

	*time = current_time();
	*time -= (((context->audio_bytes - n)/sizeof(short))>>stereo)
		/ (double) sampling_rate;
}

/* Preliminary */

#if USE_OSS

#include <linux/soundcard.h>

static int		oss_fd = -1;
extern char *		oss_device_name;

/* TFR repeats the ioctl when interrupted (EINTR) */
#define IOCTL(fd, cmd, data) (TEMP_FAILURE_RETRY(ioctl(fd, cmd, data)))

static void
read_audio_oss(void * data, double * time, rte_context * context)
{
  int stereo = (context->audio_mode == RTE_AUDIO_MODE_STEREO);
  int sampling_rate = context->audio_rate;
  ssize_t r, n = context->audio_bytes;
  struct audio_buf_info info;

  while (n > 0)
    {
      r = read(oss_fd, data, n);
		
      if (r < 0 && errno == EINTR)
	continue;

      if (r == 0)
        {
	  memset(data, 0, n);
	  break;
	}
	
      g_assert(r > 0 && "OSS read failed");

      data = (char *) data + r;
      n -= r;
    }

  *time = current_time();

  if (IOCTL(oss_fd, SNDCTL_DSP_GETISPACE, &info) == -1)
    g_assert(!"SNDCTL_DSP_GETISPACE failed");

  *time -= (((context->audio_bytes -
	    (n+info.bytes))/sizeof(short))>>stereo) / (double)
		sampling_rate;
}

#endif /* USE_OSS */

static void
audio_data_callback(rte_context * context, void * data, double * time, enum
		    rte_mux_mode stream, void * user_data)
{
  struct timeval tv;

  g_assert (stream == RTE_AUDIO);

  if (esd_recording_socket != -1)
    read_audio(data, time, context);
#if USE_OSS
  else if (oss_fd != -1)
    read_audio_oss(data, time, context);
#endif
  else
    g_assert_not_reached();  
}

static gboolean
init_audio(gint rate, gboolean stereo)
{
#if USE_OSS
  if (oss_device_name)
    {
      int Format = AFMT_S16_LE;
      int Stereo = !!stereo;
      int Speed = 44100;

      if ((oss_fd = open(oss_device_name, O_RDONLY)) == -1)
        return FALSE;

      if ((IOCTL(oss_fd, SNDCTL_DSP_SETFMT, &Format) == -1))
        goto failed;
      if ((IOCTL(oss_fd, SNDCTL_DSP_STEREO, &Stereo) == -1))
        goto failed;
      if ((IOCTL(oss_fd, SNDCTL_DSP_SPEED, &Speed) == -1))
        goto failed;

      return TRUE;

 failed:
      close(oss_fd);
      oss_fd = -1;
      return FALSE;
    }
  else
#endif /* USE_OSS */
    {
      esd_format_t format = ESD_STREAM | ESD_RECORD | ESD_BITS16;

      format |= stereo ? ESD_STEREO : ESD_MONO;

      esd_recording_socket =
        esd_record_stream_fallback(format, rate, NULL, NULL);

      if (esd_recording_socket >= 0)
        return TRUE;

      esd_recording_socket = -1;
    }

  return FALSE;
}

static void
close_audio(void)
{
  if (esd_recording_socket != -1)
    close(esd_recording_socket);
  esd_recording_socket = -1;
#if USE_OSS
  if (oss_fd != -1)
    close(oss_fd);
  oss_fd = -1;
#endif
}

/* Called when compressed data is ready */
static void
encode_callback(rte_context * context, void * data, size_t size,
		void * user_data)
{
  /* currently /dev/null handler */
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

static gpointer data_dest;

static
gint update_timeout ( rte_context *context )
{
  struct rte_status_info status;
  GtkWidget *widget;
  gchar *buffer,*dropbuf,*procbuf;

  if (!active || !saving_dialog)
    {
      update_timeout_id = -1;
      return FALSE;
    }

  rte_get_status(context, &status);

  widget = lookup_widget(saving_dialog, "label12");
  dropbuf = g_strdup_printf(ngettext("%d frame dropped",
				     "%d frames dropped",
				     status.dropped_frames),
			    status.dropped_frames);
  procbuf = g_strdup_printf(ngettext("%d frame processed",
				     "%d frames processed",
				     status.processed_frames),
			    status.processed_frames);
  buffer = g_strdup_printf(_("%.1f MB : %s : %s"),
		    status.bytes_out / ((double)(1<<20)), dropbuf,
		    procbuf);
  gtk_label_set_text(GTK_LABEL(widget), buffer);
  g_free(buffer);
  g_free(dropbuf);
  g_free(procbuf);

  return TRUE;
}

static void
video_buffer_callback(rte_context *context,
		      rte_buffer *rb,
		      enum rte_mux_mode stream)
{
  buffer *b;
  struct tveng_frame_format *fmt;

  g_assert(stream == RTE_VIDEO);

  for (;;) {
    capture_buffer *cb = (capture_buffer *)
      (b = wait_full_buffer(&mpeg_consumer));

    fmt = &cb->d.format;

    if (cb->d.image_type &&
	fmt->height == context->height &&
	fmt->width == context->width &&
	fmt->sizeimage == context->video_bytes &&
	b->time)
      break;

    send_empty_buffer(&mpeg_consumer, b);
  }

  rb->time = b->time;
  rb->data = b->data;
  rb->user_data = b;
}

static void
video_unref_callback(rte_context *context, rte_buffer *buf)
{
  send_empty_buffer(&mpeg_consumer, (buffer*)buf->user_data);
}

static
gboolean plugin_start (void)
{
  enum rte_pixformat pixformat;
  enum tveng_frame_pixformat tveng_pixformat;
  gchar * file_name = NULL;
  GtkWidget * widget;
  FILE * file_fd;
  gchar * buffer, *b;
  GtkWidget *dialog, *label;

  tveng_pixformat =
    zconf_get_integer(NULL, "/zapping/options/main/yuv_format");

  /* it would be better to gray out the button and set insensitive */
  if (active)
    {
      ShowBox("The plugin is running!", GNOME_MESSAGE_BOX_WARNING);
      return FALSE;
    }

  if (!z_build_path(save_dir, &b))
    {
      ShowBox(_("Cannot create destination dir for clips:\n%s\n%s"),
	      GNOME_MESSAGE_BOX_WARNING, save_dir, b);
      g_free(b);
      return FALSE;
    }

#if 0
  if (lip_sync_warning &&
      zapping_info->current_controller == TVENG_CONTROLLER_V4L1)
    {
      dialog =
	gnome_dialog_new(_("Synchronization under V4L1"),
			 GNOME_STOCK_BUTTON_OK,
			 _("Do not show this again"),
			 _("V4L2 page"),
			 NULL);
      label =
	gtk_label_new(_("You are using a V4L1 driver, so synchronization\n"
			"between audio and video will not be very good.\n"
			"V4L2 drivers provide a much better sync,\n"
			"you might want to try those if you aren't\n"
			"satisfied with the results."));

      gtk_box_pack_start(GTK_BOX(GNOME_DIALOG(dialog)->vbox), label,
			 TRUE, TRUE, 0);

      gtk_widget_show(label);

      gnome_dialog_set_default(GNOME_DIALOG(dialog), 1);

      switch (gnome_dialog_run_and_close(GNOME_DIALOG(dialog)))
	{
	case 1:
	  lip_sync_warning = FALSE;
	  break;
	case 2:
	  gnome_url_show("http://www.thedirks.org/v4l2");
	  break;
	default:
	  break;
	}
    }
#endif

  if (zmisc_switch_mode(TVENG_CAPTURE_READ, zapping_info))
    {
      ShowBox("This plugin needs to run in Capture mode, but"
	      " couln't switch to that mode:\n%s",
	      GNOME_MESSAGE_BOX_INFO, zapping_info->error);
      return FALSE;
    }

  if (!request_bundle_format(tveng_pixformat, capture_w,
			     capture_h))
    {
      ShowBox("Cannot switch to %s capture format",
	      GNOME_MESSAGE_BOX_ERROR,
	      (tveng_pixformat == TVENG_PIX_YVU420) ? "YVU420" : "YUYV");
      return FALSE;
    }

  if (tveng_pixformat == TVENG_PIX_YVU420)
    pixformat = RTE_YVU420;
  else
    pixformat = RTE_YUYV;

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
		    zapping_info->format.height, "mp1e",
		    NULL);

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
		    video_buffer_callback, video_unref_callback);
      break;
    default:
      rte_set_mode(context, RTE_AUDIO | RTE_VIDEO);
      rte_set_input(context, RTE_AUDIO, RTE_CALLBACKS, FALSE,
		    audio_data_callback, NULL, NULL);
      rte_set_input(context, RTE_VIDEO, RTE_CALLBACKS, TRUE, NULL,
		    video_buffer_callback, video_unref_callback);
      break;
    }

  data_dest = NULL;

  if (motion_comp)
    rte_set_motion(context, 8, 24);

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
      rte_set_output(context, NULL, NULL, file_name);
      break;
    default:
      file_name = g_strdup("/dev/null");
      rte_set_output(context, encode_callback, NULL, NULL);
      break;
    }

  /* Set video and audio rates */
  rte_set_video_parameters(context, pixformat,
			   context->width, context->height,
			   context->video_rate, output_video_bits*1e6,
			   context->gop_sequence);
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

  active = TRUE;

  /* don't let anyone mess with our settings from now on */
  capture_lock();

  add_consumer(capture_fifo, &mpeg_consumer);

  if (!rte_start_encoding(context))
    {
      ShowBox("Cannot start encoding: %s", GNOME_MESSAGE_BOX_ERROR,
	      context->error);
      context = rte_context_destroy(context);
      if ((mux_mode+1) & 1)
	close_audio();
      active = FALSE;
      rem_consumer(&mpeg_consumer);
      capture_unlock();
      return FALSE;
    }

  if (saving_dialog)
    gtk_widget_destroy(saving_dialog);

  saving_dialog =
    build_widget("dialog1", PACKAGE_DATA_DIR
		 "/mpeg_properties.glade");

  gtk_signal_connect(GTK_OBJECT(saving_dialog), "delete-event",
		     GTK_SIGNAL_FUNC(on_delete_event),
		     NULL);
  widget = lookup_widget(saving_dialog, "button1");
  gtk_signal_connect(GTK_OBJECT(widget), "clicked",
		     GTK_SIGNAL_FUNC(on_button_clicked),
		     NULL);

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
  switch (mux_mode)
    {
    case 0:
      buffer =
	g_strdup_printf("%s %g",
			_("Output audio Kbits per second:"),
			context->output_audio_bits/1e3);
      break;
    case 1:
      buffer =
	g_strdup_printf("%s %g",
			_("Output video Mbits per second:"),
			context->output_video_bits/1e6);
      break;
    default:
      buffer =
	g_strdup_printf("%s %g\n%s %g",
			_("Output audio Kbits per second:"),
			context->output_audio_bits/1e3,
			_("Output video Mbits per second:"),
			context->output_video_bits/1e6);
      break;
    }
  gtk_label_set_text(GTK_LABEL(widget), buffer);
  g_free(buffer);

  widget = lookup_widget(saving_dialog, "label12");
  gtk_label_set_text(GTK_LABEL(widget), _("Waiting for frames..."));

  g_free(file_name);

  update_timeout_id =
    gtk_timeout_add(50, (GtkFunction)update_timeout, context);

  gtk_widget_show(saving_dialog);

  return TRUE;
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

  buffer = g_strconcat(root_key, "motion_comp", NULL);
  zconf_create_boolean(FALSE, "Enable motion compensation", buffer);
  motion_comp = !!zconf_get_boolean(NULL, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "capture_w", NULL);
  zconf_create_integer(384, "Capture Width", buffer);
  capture_w = zconf_get_integer(NULL, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "capture_h", NULL);
  zconf_create_integer(288, "Capture height", buffer);
  capture_h = zconf_get_integer(NULL, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "lip_sync_warning", NULL);
  zconf_create_boolean(TRUE, "Warning about lip sync with V4L", buffer);
  lip_sync_warning = zconf_get_boolean(NULL, buffer);
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

  buffer = g_strconcat(root_key, "motion_comp", NULL);
  zconf_set_boolean(!!motion_comp, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "capture_w", NULL);
  zconf_set_integer(capture_w, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "capture_h", NULL);
  zconf_set_integer(capture_h, buffer);
  g_free(buffer);

  buffer = g_strconcat(root_key, "lip_sync_warning", NULL);
  zconf_set_boolean(lip_sync_warning, buffer);
  g_free(buffer);
}

static void
do_stop(void)
{
  if (!active)
    return;

  if (saving_dialog)
    {
      gtk_widget_destroy(saving_dialog);

      saving_dialog = NULL;
    }

  active = FALSE;

  context = rte_context_destroy(context);

  rem_consumer(&mpeg_consumer);

  if (update_timeout_id >= 0)
    {
      gtk_timeout_remove(update_timeout_id);
      update_timeout_id = -1;
    }

  if ((mux_mode+1) & 1)
    close_audio();

  capture_unlock();
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
gboolean plugin_add_properties ( GnomePropertyBox * gpb )
{
  GtkWidget *mpeg_properties;
  GtkWidget *label;
  GtkWidget * widget;
  GtkAdjustment * adj;
  gint page;

  if (!gpb)
    return TRUE;

  mpeg_properties =
    build_widget("notebook1", PACKAGE_DATA_DIR "/mpeg_properties.glade");
  label = gtk_label_new(_("MPEG"));

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

  widget = lookup_widget(mpeg_properties, "checkmotion");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), motion_comp);
  gtk_signal_connect(GTK_OBJECT(widget),
		     "toggled", on_property_item_changed, gpb);

  widget = lookup_widget(mpeg_properties, "spinbutton3");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), capture_w);
  gtk_signal_connect(GTK_OBJECT(widget), "changed",
		     on_property_item_changed, gpb);

  widget = lookup_widget(mpeg_properties, "spinbutton4");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), capture_h);
  gtk_signal_connect(GTK_OBJECT(widget), "changed",
		     on_property_item_changed, gpb);

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

  return TRUE;
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

      widget = lookup_widget(mpeg_properties, "checkmotion");
      motion_comp = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

      widget = lookup_widget(mpeg_properties, "optionmenu1");
      widget = GTK_WIDGET(GTK_OPTION_MENU(widget)->menu);
      output_mode = g_list_index(GTK_MENU_SHELL(widget)->children,
				 gtk_menu_get_active(GTK_MENU(widget)));

      widget = lookup_widget(mpeg_properties, "spinbutton1");
      engine_verbosity =
	gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));

      widget = lookup_widget(mpeg_properties, "spinbutton3");
      capture_w =
	gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
      capture_w &= ~15;

      widget = lookup_widget(mpeg_properties, "spinbutton4");
      capture_h =
	gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
      capture_h &= ~15;
      
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
static gboolean
on_delete_event         		(GtkWidget	*widget,
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
static void
on_button_clicked			(GtkButton	*button,
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
