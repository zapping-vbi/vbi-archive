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

/* $Id: mpeg.c,v 1.63 2007-08-30 12:22:31 mschimek Exp $ */

/* XXX gtk+ 2.3 GtkOptionMenu -> ? */
#undef GTK_DISABLE_DEPRECATED

#include "src/plugin_common.h"

#ifdef HAVE_LIBRTE

#include <glade/glade.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <ctype.h>

#include "src/audio.h"
#include "mpeg.h"
#include "src/properties.h"
#include "pixmaps.h"
#include "src/zvbi.h"
#include "src/subtitle.h"

#include "src/v4linterface.h" /* videostd_inquiry; preliminary */

#include "libvbi/export.h"

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
static vbi3_export *		export_prop;
static GtkWidget *		audio_options;
static GtkWidget *		video_options;
static gint			capture_w = 384;
static gint			capture_h = 288;

/* Encoding */

static GtkWidget *		saving_dialog;

static volatile gboolean	active;
static gint			capture_format_id = -1;
static tv_pixfmt		capture_pixfmt;

/* XXX */
static volatile gint		stopped;

static rte_context *		context_enc;
static rte_codec *		audio_codec;
static rte_codec *		video_codec;

static rte_stream_parameters	audio_params;
static rte_stream_parameters	video_params;

static guint			update_timeout_id = NO_SOURCE_ID;

static gpointer			audio_handle;
static void *			audio_buf;	/* preliminary */
static unsigned int		audio_size;

/* An array of files for multilingual subtitle recording
   in multiple monolingual formats. Not implemented yet. */
static struct {
  vbi3_export *		export;
  FILE *		fp;
}				subt_file[30];
static struct {
  vbi3_pgno		first;
  vbi3_pgno		last;
  guint			file_num; /* index into subt_file[] */
}				subt_page[60];
static enum {
  SUBT_SEL_DISPLAYED,
  SUBT_SEL_ALL,
  SUBT_SEL_PAGES,
}				subt_selection;
static double			subt_start_timestamp;
static gboolean			subt_row_update;

static tveng_device_info *	zapping_info;
static zf_consumer		mpeg_consumer;

static void
saving_dialog_attach_formats	(void);

/*
 *  Input/output
 */

static rte_bool
audio_callback			(rte_context *		context _unused_,
				 rte_codec *		codec _unused_,
				 rte_buffer *		rb)
{
  rb->data = audio_buf;
  rb->size = audio_size;

  read_audio_data (audio_handle, rb->data, rb->size, &rb->timestamp);

  return TRUE;
}

static rte_bool
video_callback			(rte_context *		context _unused_,
				 rte_codec *		codec _unused_,
				 rte_buffer *		rb)
{
  zf_buffer *b;
  zimage *zi;

  for (;;) {
    capture_frame *cf;

    /* Abort if a bug prevents normal termination. */
    if (0 == stopped)
      return FALSE;
    else if (stopped > 0)
      --stopped;

    b = zf_wait_full_buffer (&mpeg_consumer);
    cf = PARENT (b, capture_frame, b);

    /* XXX copy? */
    zi = retrieve_frame (cf, capture_pixfmt, /* copy */ TRUE);

    if (NULL != zi)
      break;

    zf_send_empty_buffer (&mpeg_consumer, b);
  }

  rb->timestamp = b->time;
  rb->data = zi->img;
  rb->size = 1; /* XXX don't care 4 now, should be zi->fmt.size */
  rb->user_data = b;

  return TRUE;
}

static rte_bool
video_unref			(rte_context *		context _unused_,
				 rte_codec *		codec _unused_,
				 rte_buffer *		rb)
{
  zf_buffer *b;

  if (0 == rb->size) {
    /* Bug in librte < 0.5.6: If video_callback() returned FALSE
       it passes an "EOF" buffer to the codec and later thinks
       it must return data to this function. */
    return TRUE;
  }

  b = (zf_buffer *) rb->user_data;
  zf_send_empty_buffer (&mpeg_consumer, b);
  return TRUE;
}

static vbi3_bool
subt_handler			(const vbi3_event *	ev,
				 void *			user_data)
{
  vbi3_page *pg;
  vbi3_decoder *vbi;
  vbi3_pgno pgno;
  guint file_num;

  user_data = user_data;

  switch (ev->type) {
  case VBI3_EVENT_TTX_PAGE:
    pgno = ev->ev.ttx_page.pgno;
    break;

  case VBI3_EVENT_CC_PAGE:
    if (subt_row_update && !(ev->ev.caption.flags & VBI3_ROW_UPDATE))
      return FALSE; /* pass on */
    pgno = ev->ev.caption.channel;
    break;

  default:
    g_assert_not_reached ();
  }

  file_num = 0;

  switch (subt_selection)
    {
      guint i;

    default:
      for (i = 0; i < 1 /* G_N_ELEMENTS (subt_page) */; ++i)
	{
	  if (pgno >= subt_page[i].first
	      && pgno <= subt_page[i].last)
	    break;
	}

      if (i >= 1 /* G_N_ELEMENTS (subt_page) */)
	return FALSE; /* pass on */

      file_num = subt_page[i].file_num;

      break;
    }

  vbi = zvbi_get_object ();
  g_assert (NULL != vbi);

  if (pgno >= 0x100)
    {
      vbi3_ttx_charset_code charset_code;

      if (zvbi_cur_channel_get_ttx_encoding (&charset_code, pgno))
	{
	  pg = vbi3_decoder_get_page
	    (vbi, NULL /* current network */,
	     pgno, VBI3_ANY_SUBNO,
	     VBI3_OVERRIDE_CHARSET_0, charset_code,
	     VBI3_WST_LEVEL, VBI3_WST_LEVEL_1p5,
	     VBI3_PADDING, FALSE,
	     VBI3_END);
	}
      else
	{
	  pg = vbi3_decoder_get_page
	    (vbi, NULL /* current network */,
	     pgno, VBI3_ANY_SUBNO,
#if 0
	     VBI3_DEFAULT_CHARSET_0, option_default_cs,
#endif
	     VBI3_WST_LEVEL, VBI3_WST_LEVEL_1p5,
	     VBI3_PADDING, FALSE,
	     VBI3_END);
	}
    }
  else
    {
      pg = vbi3_decoder_get_page
	(vbi, NULL /* current network */,
	 pgno, /* subno */ 0,
#if 0
	 VBI3_DEFAULT_FOREGROUND, option_default_fg,
	 VBI3_DEFAULT_BACKGROUND, option_default_bg,
#endif
	 VBI3_PADDING, FALSE,
	 VBI3_ROW_CHANGE, (vbi3_bool) subt_row_update,
	 VBI3_END);
    }

  g_assert (NULL != pg);

  g_assert (NULL != subt_file[file_num].export);

  vbi3_export_set_timestamp (subt_file[file_num].export, ev->timestamp);

  if (!vbi3_export_stdio (subt_file[file_num].export,
			  subt_file[file_num].fp, pg))
    {
      /* TODO, error ignored for now. */
    }

  vbi3_page_delete (pg);

  return FALSE; /* pass on */
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
				 gpointer		data _unused_)
{
  gint max[2] = { 0, 0 };
  char *p, *e;
  gint w, h;

  /* ATTENTION S16LE assumed */
  for (p = ((char *) audio_buf) + 1,
	 e = ((char *) audio_buf + audio_size) - 2;
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

  if (!active || !saving_dialog)
    {
      update_timeout_id = NO_SOURCE_ID;
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
  if (update_timeout_id != NO_SOURCE_ID)
    {
      g_source_remove (update_timeout_id);
      update_timeout_id = NO_SOURCE_ID;
    }
}

static void
saving_dialog_status_enable	(rte_context *		context)
{
  g_assert (saving_dialog != NULL);

  if (audio_codec)
    {
      g_signal_connect (G_OBJECT (lookup_widget (saving_dialog, "volume")),
			  "expose-event",
			  G_CALLBACK (volume_expose),
			  NULL);

      update_timeout_id =
	g_timeout_add (1000 / 8, (GSourceFunc)
		       saving_dialog_status_update, context);
    }
  else
    {
      update_timeout_id =
	g_timeout_add (1000 / 2, (GSourceFunc)
		       saving_dialog_status_update, context);
    }
}

/*
 *  Start/stop recording
 */

static void
stop_subtitle_encoding		(void)
{
  vbi3_decoder *vbi;
  guint i;

  vbi = NULL;

  if (NULL != subt_file[0].export)
    {
      vbi = zvbi_get_object ();
      g_assert (NULL != vbi);

      vbi3_decoder_remove_event_handler (vbi, subt_handler, NULL);
    }

  for (i = 0; i < G_N_ELEMENTS (subt_file); ++i)
    {
      if (NULL != subt_file[i].export)
	{
	  const vbi3_export_info *xi;

	  g_assert (NULL != subt_file[i].fp);

	  xi = vbi3_export_info_from_export (subt_file[i].export);
	  g_assert (xi != NULL);

	  if (xi->open_format)
	    {
	      /* Finalize. */
	      /* XXX error check */
	      vbi3_export_stdio (subt_file[i].export,
				 subt_file[i].fp, NULL);
	    }

	  fclose (subt_file[i].fp);
	  subt_file[i].fp = NULL;

	  vbi3_export_delete (subt_file[i].export);
	  subt_file[i].export = NULL;
	}
    }
}

static void
do_stop				(void)
{
  if (!active)
    return;

  stopped = 20;

  saving_dialog_status_disable ();

  rte_context_delete (context_enc);
  context_enc = NULL;

  stop_subtitle_encoding ();

  zf_rem_consumer (&mpeg_consumer);

  if (audio_handle)
    close_audio_device (audio_handle);
  audio_handle = NULL;

  if (audio_buf)
    free (audio_buf);
  audio_buf = NULL;

  if (-1 != capture_format_id)
    release_capture_format (capture_format_id);
  capture_format_id = -1;

  active = FALSE;
}

static const gchar *
rec_conf_get_string		(const gchar *		name)
{
  gchar *zcname;
  const gchar *value;

  zcname = g_strconcat (zconf_root, "/configs/",
			record_config_name, name, NULL);
  value = zconf_get_string (NULL, zcname);
  g_free (zcname);

  return value;
}

static gchar *
xo_zconf_name			(const vbi3_export *	e,
				 const vbi3_option_info *oi,
				 gpointer		user_data)
{
  const vbi3_export_info *xi;

  user_data = user_data;

  xi = vbi3_export_info_from_export (e);
  g_assert (xi != NULL);

  return g_strconcat (zconf_root, "/configs/",
		      record_config_name, "/vbi_file_options/",
		      xi->keyword, "/", oi->keyword, NULL);
}

static gboolean
open_subtitle_file		(const gchar *		format,
				 const gchar *		file_name)
{
  const vbi3_export_info *xi;
  gchar **extensions;
  gchar *subt_file_name;
  guint i;

  subt_file_name = NULL;

  for (i = 0; i < G_N_ELEMENTS (subt_file); ++i)
    if (NULL == subt_file[i].export)
      break;

  if (i >= G_N_ELEMENTS (subt_file))
    return FALSE;

  subt_file[i].export = vbi3_export_new (format, NULL);
  if (NULL == subt_file[i].export)
    goto failure;

  if (!zvbi_export_load_zconf (subt_file[i].export, xo_zconf_name, NULL))
    goto failure;

  vbi3_export_set_timestamp (subt_file[i].export, subt_start_timestamp);

  xi = vbi3_export_info_from_export (subt_file[i].export);
  g_assert (xi != NULL);

  extensions = g_strsplit (xi->extension, ",", 2);
  subt_file_name = z_replace_filename_extension (file_name, extensions[0]);
  g_strfreev (extensions);

  subt_file[i].fp = fopen (subt_file_name, "w");
  if (NULL == subt_file[i].fp)
    goto failure;

  g_free (subt_file_name);
  subt_file_name = NULL;

  return TRUE;

 failure:
  g_free (subt_file_name);
  subt_file_name = NULL;

  vbi3_export_delete (subt_file[i].export);
  subt_file[i].export = NULL;

  return FALSE;
}

static gboolean
is_valid_pgno			(vbi3_pgno		pgno)
{
  if (!vbi3_is_bcd (pgno))
    return FALSE;

  if (pgno >= 1 && pgno <= 8)
    return TRUE;

  if (pgno >= 0x100 && pgno <= 0x899)
    return TRUE;

  return FALSE;
}

static void
parse_subtitle_page_numbers	(void)
{
  gchar *zcname;
  const gchar *s;
  gchar *end;
  vbi3_pgno pgno;
  guint n_pages;

  CLEAR (subt_page);

  zcname = g_strconcat (zconf_root, "/configs/",
			record_config_name, "/vbi_pages", NULL);
  s = zconf_get_string (NULL, zcname);
  g_free (zcname);
  zcname = NULL;

  if (NULL == s)
    return;

  n_pages = 0;

  for (;;)
    {
      if (n_pages >= G_N_ELEMENTS (subt_page))
	break;

      if (0 == *s)
	break;

      if (!isdigit (*s))
	{
	  ++s;
	  continue;
	}

      pgno = strtoul (s, &end, 16);
      s = end;

      if (!is_valid_pgno (pgno))
	continue;

      subt_page[n_pages].first = pgno;

      while (*s && isspace (*s))
	++s;

      if (0 && '-' == *s)
	{
	  ++s;

	  while (*s && isspace (*s))
	    ++s;

	  if (isdigit (*s))
	    {
	      pgno = strtoul (s, &end, 16);
	      s = end;

	      if (!is_valid_pgno (pgno))
		continue;
	    }
	}

      subt_page[n_pages++].last = pgno;
    }
}

static void
init_subtitle_encoding		(const gchar *		file_name)
{
  const gchar *selection;
  const gchar *mode;

  selection = rec_conf_get_string ("/vbi_selection");
  if (NULL == selection)
    return;

  if (0 == strcmp (selection, "displayed"))
    {
      subt_selection = SUBT_SEL_DISPLAYED;

      CLEAR (subt_page);

      subt_page[0].first = zvbi_caption_pgno;
      subt_page[0].last = subt_page[0].first;
    }
  else if (0 == strcmp (selection, "all"))
    {
      subt_selection = SUBT_SEL_ALL;

      CLEAR (subt_page);

      /* 0 if none. */
      subt_page[0].first = zvbi_find_subtitle_page (zapping_info);
      subt_page[0].last = subt_page[0].first;
    }
  else if (0 == strcmp (selection, "pages"))
    {
      subt_selection = SUBT_SEL_PAGES;

      parse_subtitle_page_numbers ();
    }

  if (0 == subt_page[0].first)
    return;

  mode = rec_conf_get_string ("/vbi_mode");
  if (NULL == mode)
    return;

  if (0 == strcmp (mode, "file"))
    {
      vbi3_decoder *vbi;
      const gchar *format;
      gchar *zcname;

      vbi = zvbi_get_object ();
      if (NULL == vbi)
	goto failure;

      format = rec_conf_get_string ("/vbi_file_format");
      if (NULL == format)
	goto failure;

      /* XXX this isn't terribly accurate, should use the
	 start time calculated by/for the rte_context. */
      subt_start_timestamp = zf_current_time ();

      /* Error ignored. */
      open_subtitle_file (format, file_name);

      zcname = g_strconcat (zconf_root, "/configs/",
			    record_config_name, "/vbi_row_update", NULL);
      subt_row_update = zconf_get_boolean (NULL, zcname);
      g_free (zcname);

    failure:
      ;
    }
}

static gboolean
do_start			(const gchar *		file_name)
{
  rte_context *context;
  gdouble captured_frame_rate;
  gint width, height;
  gint retry;

  if (active)
    return FALSE;

  stopped = -1;

  /* sync_warning (void); */

  g_assert (context_enc == NULL);

  context = grte_context_load (zconf_root, record_config_name,
			       NULL,
			       &audio_codec, &video_codec,
			       &width, &height);

  if (!context)
    {
      ShowBox ("Couldn't create the encoding context",
	       GTK_MESSAGE_ERROR);
      return FALSE;
    }

  if (!audio_codec && !video_codec)
    {
      ShowBox (_("Format %s is incomplete, please configure the video or audio codec."),
	       GTK_MESSAGE_ERROR,
	       record_config_name);
      rte_context_delete (context);
      return FALSE;
    }

  context_enc = context;

  if (0)
    fprintf (stderr, "codecs: a%p v%p\n", audio_codec, video_codec);

  captured_frame_rate = 0.0;

  if (video_codec)
    {
      display_mode old_dmode;
      capture_mode old_cmode;
      tv_pixfmt_set pixfmt_set;
      rte_video_stream_params *par;

      par = &video_params.video;

      old_dmode = zapping->display_mode;
      old_cmode = tv_get_capture_mode (zapping_info);

      /* Stop and restart to make sure we use a capture thread,
	 not a queue because in do_stop the (main) thread copying
	 buffers from the queue to the fifo and the video codec
	 thread reading frames until stop time is reached may
	 deadlock. That means for now recording and deinterlacing
	 do not combine. :-( */

      if (-1 == zmisc_stop (zapping_info))
	{
	  rte_context_delete (context);
	  context_enc = NULL;

	  ShowBox ("This plugin needs to run in Capture mode, but"
		   " couldn't switch to that mode:\n%s",
		   GTK_MESSAGE_INFO, tv_get_errstr (zapping_info));

	  return FALSE;
	}

      retry = 0;

      pixfmt_set = (TV_PIXFMT_SET (TV_PIXFMT_YUV420) |
		    TV_PIXFMT_SET (TV_PIXFMT_YVU420) |
		    TV_PIXFMT_SET (TV_PIXFMT_YUYV));

      for (;;)
	{
	  const tv_image_format *fmt;
	  guint i;

	  if (-1 != capture_format_id)
	    {
	      release_capture_format (capture_format_id);
	      capture_format_id = -1;
	    }

	  if (6 == retry++
	      || TV_PIXFMT_SET_EMPTY == pixfmt_set)
	    {
	      rte_context_delete (context);
	      context_enc = NULL;

	      ShowBox ("Cannot switch to requested capture format",
		       GTK_MESSAGE_ERROR);

	      zmisc_switch_mode (old_dmode, old_cmode, zapping_info,
				 /* warnings */ FALSE);

	      return FALSE;
	    }

	  capture_format_id = request_capture_format
	    (zapping_info, width, height,
	     pixfmt_set, REQ_SIZE | REQ_PIXFMT | REQ_CONTINUOUS);

	  if (-1 == capture_format_id)
	    {
	      rte_context_delete (context);
	      context_enc = NULL;

	      ShowBox ("Cannot switch capture format",
		       GTK_MESSAGE_ERROR);

	      zmisc_switch_mode (old_dmode, old_cmode, zapping_info,
				 /* warnings */ FALSE);

	      return FALSE;
	    }

	  get_capture_format (capture_format_id,
			      /* width */ NULL,
			      /* height */ NULL,
			      &capture_pixfmt);

	  fmt = tv_cur_capture_format (zapping_info);

	  CLEAR (*par);

	  par->width = fmt->width;
	  par->height = fmt->height;

	  switch (capture_pixfmt)
	    {
	    case TV_PIXFMT_YUV420:
	      par->pixfmt = RTE_PIXFMT_YUV420;
	      par->stride = par->width;
	      par->uv_stride = par->stride >> 1;
	      par->v_offset = par->stride * par->height;
	      par->u_offset = par->v_offset * 5 / 4;
	      break;

	    case TV_PIXFMT_YVU420:
	      par->pixfmt = RTE_PIXFMT_YUV420;
	      par->stride = par->width;
	      par->uv_stride = par->stride >> 1;
	      par->u_offset = par->stride * par->height;
	      par->v_offset = par->u_offset * 5 / 4;
	      break;

	    case TV_PIXFMT_YUYV:
	      par->pixfmt = RTE_PIXFMT_YUYV;
	      /* defaults */
	      break;

	    default:
	      g_assert_not_reached ();
	    }

	  if (tv_cur_video_standard (zapping_info))
	    {
	      captured_frame_rate =
		tv_cur_video_standard (zapping_info)->frame_rate;
	    }
	  else
	    {
	      captured_frame_rate = videostd_inquiry ();
	      
	      if (captured_frame_rate < 0.0)
		{
		  rte_context_delete (context);
		  context_enc = NULL;

		  if (-1 != capture_format_id)
		    {
		      release_capture_format (capture_format_id);
		      capture_format_id = -1;
		    }

		  zmisc_switch_mode (old_dmode, old_cmode, zapping_info,
				     /* warnings */ FALSE);

		  return FALSE;
		}
	    }

	  /* not supported by all codecs */
	  /* g_assert */ (rte_codec_option_set (video_codec,
						"coded_frame_rate",
						(double) captured_frame_rate));

	  par->frame_rate = captured_frame_rate;

	  if (!rte_parameters_set (video_codec, &video_params))
	    {
	      rte_context_delete (context);
	      context_enc = NULL;

	      if (-1 != capture_format_id)
		{
		  release_capture_format (capture_format_id);
		  capture_format_id = -1;
		}

	      /* FIXME */

	      ShowBox ("Oops, catched a bug.",
		       GTK_MESSAGE_ERROR);

	      zmisc_switch_mode (old_dmode, old_cmode, zapping_info,
				 /* warnings */ FALSE);

	      return FALSE; 
	    }

	  fmt = tv_cur_capture_format (zapping_info);

	  /* Width, height may change due to codec limits. */
	  if (par->width != fmt->width
	      || par->height != fmt->height)
	    {
	      /* Try to capture this size. */
	      width = par->width;
	      height = par->height;
	      continue;
	    }
	  else if (par->pixfmt == RTE_PIXFMT_YUYV)
	    {
	      if (capture_pixfmt == TV_PIXFMT_YUYV)
		break;
	    }
	  else if (par->pixfmt == RTE_PIXFMT_YUV420)
	    {
	      /* NB RTE_PIXFMT_YUV420 is backwards. */
	      if (par->v_offset < par->u_offset)
		{
		  if (capture_pixfmt == TV_PIXFMT_YUV420)
		    break;
		}
	      else
		{
		  if (capture_pixfmt == TV_PIXFMT_YVU420)
		    break;
		}
	    }

	  /* Try other format. */

	  for (i = 0; i < TV_MAX_PIXFMTS; ++i)
	    {
	      if (pixfmt_set & TV_PIXFMT_SET (i))
		{
		  pixfmt_set &= ~TV_PIXFMT_SET (i);
		  break;
		}
	    }
	} /* retry loop */



      if (-1 == zmisc_switch_mode (old_dmode, CAPTURE_MODE_READ, zapping_info,
				   /* warnings */ FALSE))
	{
	  rte_context_delete (context);
	  context_enc = NULL;

	  if (-1 != capture_format_id)
	    {
	      release_capture_format (capture_format_id);
	      capture_format_id = -1;
	    }

	  ShowBox ("This plugin needs to run in Capture mode, but"
		   " couldn't switch to that mode:\n%s",
		   GTK_MESSAGE_INFO, tv_get_errstr (zapping_info));

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
		   GTK_MESSAGE_ERROR);
	  goto failed;
	}

      if (!(audio_buf = malloc(audio_size = par->fragment_size)))
	{
	  ShowBox ("Couldn't open the audio device",
		   GTK_MESSAGE_ERROR);
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
      gchar *dir = g_path_get_dirname(file_name);

      /* XXX zapping real parent? */
      if (!z_build_path_with_alert (GTK_WINDOW (zapping), dir))
	{
	  g_free (dir);
	  goto failed;
	}

      g_free(dir);

      if (!rte_set_output_file (context, file_name))
	  /* XXX more info please */
        {
	  ShowBox (_("Cannot create file %s: %s\n"),
		   GTK_MESSAGE_WARNING,
		   file_name, rte_errstr (context));
	  goto failed;
	}
    }
  else
    {
      g_assert (rte_set_output_discard (context));
    }

  init_subtitle_encoding (file_name);

  active = TRUE;

  /* don't let anyone mess with our settings from now on */
  //  capture_lock ();

  if (video_codec)
    zf_add_consumer (&capture_fifo, &mpeg_consumer);

  if (subt_file[0].export)
    {
      vbi3_decoder *vbi;
      vbi3_bool success;

      vbi = zvbi_get_object ();
      g_assert (NULL != vbi);

      success = vbi3_decoder_add_event_handler (vbi,
						VBI3_EVENT_TTX_PAGE |
						VBI3_EVENT_CC_PAGE,
						subt_handler, NULL);
      g_assert (success);
    }

  if (!rte_start (context, 0.0, NULL, TRUE))
    {
      ShowBox ("Cannot start encoding: %s", GTK_MESSAGE_ERROR,
	       rte_errstr (context));
      zf_rem_consumer (&mpeg_consumer);
      //      capture_unlock ();
      active = FALSE;
      goto failed;
    }

  saving_dialog_status_enable (context);

  return TRUE;

 failed:
  stop_subtitle_encoding ();

  if (context_enc)
    rte_context_delete (context_enc);
  context_enc = NULL;

  if (audio_buf)
    free (audio_buf);
  audio_buf = NULL;

  if (audio_handle)
    close_audio_device (audio_handle);
  audio_handle = NULL;

  if (-1 != capture_format_id)
    release_capture_format (capture_format_id);
  capture_format_id = -1;

  return FALSE;
}

/*
 *  Record config menu
 */

/**
 * record_config_menu_get_active:
 * @option_menu: 
 * 
 * Returns currently selected configuration. 
 * 
 * Return value: 
 * Static keyword (escaped config name).
 **/
static const gchar *
record_config_menu_get_active	(GtkWidget *		option_menu)
{
  GtkWidget *widget;

  widget = gtk_option_menu_get_menu (GTK_OPTION_MENU (option_menu));
  widget = gtk_menu_get_active (GTK_MENU (widget));

  if (!widget)
    return NULL;

  return g_object_get_data (G_OBJECT (widget), "keyword");
}

static gboolean
record_config_menu_set_active	(GtkWidget *		option_menu,
				 const gchar *		keyword)
{
  GtkWidget *widget;
  GtkMenuShell *menu_shell;
  guint i;
  GList *glist;

  widget = gtk_option_menu_get_menu (GTK_OPTION_MENU (option_menu));
  menu_shell = GTK_MENU_SHELL (widget);

  i = 0;

  for (glist = menu_shell->children; glist; glist = glist->next)
    {
      const gchar *key;

      key = g_object_get_data (G_OBJECT (glist->data), "keyword");
      if (key && 0 == strcmp (key, keyword))
	{
	  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), i);
	  return TRUE;
	}

      ++i;
    }

  return FALSE;
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
  const gchar *label;
  gint i;
  guint def, count;

  if ((menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (option_menu))))
    gtk_widget_destroy (menu);

  menu = gtk_menu_new ();

  def = 0;
  count = 0;

  for (i = 0; (label = zconf_get_nth ((guint) i, NULL, zcname)); i++)
    {
      gchar *base = g_path_get_basename (label);
      GtkWidget *menu_item;

      menu_item = gtk_menu_item_new_with_label (base);

      gtk_widget_show (menu_item);
      g_object_set_data_full (G_OBJECT (menu_item), "keyword",
			      base,
			      (GtkDestroyNotify) g_free);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

      if (default_item)
	if (strcmp (base, default_item) == 0)
	  def = count;

      count++;
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), def);

  g_free (zcname);

  return count;
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
  const gchar *label;
  gint i;

  for (i = 0; (label = zconf_get_nth ((guint) i, NULL, zcname)); i++)
    {
      gchar *base = g_path_get_basename (label);

      if (strcmp (base, name) == 0)
	{
	  g_free (zcname);
	  g_free (base);
	  return i;
	}

      g_free (base);
    }

  g_free (zcname);
  return -1;
}

/*
 *  Format configuration
 */

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
	  g_signal_connect_swapped (G_OBJECT (*optionspp), "destroy",
				    G_CALLBACK (g_nullify_pointer),
				    optionspp);
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
  keyword = g_object_get_data (G_OBJECT (menu_item), "keyword");
  select_codec (mpeg_properties, record_config_name, keyword, RTE_STREAM_AUDIO);
}

static void
on_video_codec_changed		(GtkWidget *		menu,
				 GtkWidget *		mpeg_properties)
{
  GtkWidget *menu_item = gtk_menu_get_active (GTK_MENU (menu));
  char *keyword;

  g_assert (menu_item);
  keyword = g_object_get_data (G_OBJECT (menu_item), "keyword");
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
  guint default_item;
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
  g_signal_connect (G_OBJECT (menu), "selection-done",
		    G_CALLBACK (on_changed), mpeg_properties);

  menu_item = gtk_menu_get_active (GTK_MENU (menu));
  keyword = g_object_get_data (G_OBJECT (menu_item), "keyword");

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

  /* preliminary */
  {
    rte_context_info *ci = rte_context_info_by_context (context);
    GtkWidget *widget;

    if (ci && 0 == strcmp (ci->keyword, "mp1e_mpeg1_vcd"))
      ci = NULL;

    widget = lookup_widget (mpeg_properties, "spinbutton9");
    gtk_widget_set_sensitive (widget, !!ci);
    widget = lookup_widget (mpeg_properties, "spinbutton10");
    gtk_widget_set_sensitive (widget, !!ci);
  }
}

static void
on_file_format_changed		(GtkWidget *		menu,
				 GtkWidget *		mpeg_properties)
{
  GtkWidget *menu_item = gtk_menu_get_active (GTK_MENU (menu));
  char *keyword = g_object_get_data (G_OBJECT (menu_item), "keyword");

  select_file_format (mpeg_properties, record_config_name, keyword);
}

static const gchar *
subtitle_modes [] = {
  "none",
  /* "open", */
  /* "closed", */
  "file"
};

static guint
find_subtitle_mode		(const gchar *		mode)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (subtitle_modes); ++i)
    if (0 == strcmp (subtitle_modes[i], mode))
      return i;

  return 0;
}

static void
activate_subtitle_mode_button	(GtkWidget *		mpeg_properties,
				 const gchar *		conf_name)
{
  gchar *zcname;
  const gchar *mode;
  gchar *wname;
  GtkWidget *widget;

  zcname = g_strconcat (zconf_root_temp, "/configs/",
			conf_name, "/vbi_mode", NULL);

  zconf_create_string (subtitle_modes[0], "VBI recording mode", zcname);
  mode = zconf_get_string (NULL, zcname);

  if (NULL != mode)
    mode = subtitle_modes[find_subtitle_mode (mode)];
  else
    mode = subtitle_modes[0];

  g_free (zcname);

  wname = g_strconcat ("subt-", mode, NULL);
  widget = lookup_widget (mpeg_properties, wname);
  g_free (wname);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
}

static void
on_subtitle_file_toggled	(GtkToggleButton *	toggle_button,
				 GtkWidget *		mpeg_properties)
{
  gboolean active;
  GtkWidget *box;

  active = gtk_toggle_button_get_active (toggle_button);

  box = lookup_widget (mpeg_properties, "alignment5");
  gtk_widget_set_sensitive (box, active);
}

static gchar *
xo_temp_zconf_name		(const vbi3_export *	e,
				 const vbi3_option_info *oi,
				 gpointer		user_data)
{
  const vbi3_export_info *xi;

  user_data = user_data;

  xi = vbi3_export_info_from_export (e);
  g_assert (xi != NULL);

  return g_strconcat (zconf_root_temp, "/configs/",
		      record_config_name, "/vbi_file_options/",
		      xi->keyword, "/", oi->keyword, NULL);
}

static void
on_vbi_format_menu_activate	(GtkWidget *		menu_item,
				 GtkWidget *		mpeg_properties)
{
  GtkWidget *format_menu;
  gchar *zcname;
  gchar *keyword;
  GtkContainer *container;
  GList *glist;
  GtkWidget *table;

  format_menu = lookup_widget (mpeg_properties, "optionmenu17");
  zcname = (gchar *) g_object_get_data (G_OBJECT (format_menu), "zcname");

  keyword = (gchar *) g_object_get_data (G_OBJECT (menu_item), "key");
  g_assert (keyword != NULL);

  zconf_set_string (keyword, zcname);

  vbi3_export_delete (export_prop);
  export_prop = vbi3_export_new (keyword, NULL);
  g_assert (NULL != export_prop);

  container = GTK_CONTAINER (lookup_widget (mpeg_properties,
					    "subt-file-option-box"));
  while ((glist = gtk_container_get_children (container)))
    gtk_container_remove (container, GTK_WIDGET (glist->data));

  table = zvbi_export_option_table_new (export_prop, xo_temp_zconf_name,
					/* user_data */ NULL);
  if (NULL != table)
    {
#if 0
      GtkWidget *frame;

      frame = gtk_frame_new (_("Options"));
      gtk_container_add (GTK_CONTAINER (frame), table);
      gtk_widget_show_all (frame);
      gtk_box_pack_start (GTK_BOX (container), frame, TRUE, TRUE, 0);
#else
      gtk_widget_show_all (table);
      gtk_box_pack_start (GTK_BOX (container), table, TRUE, TRUE, 0);
#endif
    }
}

static const gchar *
subtitle_selections [] = {
  "displayed",
  /* "all", */
  "pages",
};

static guint
find_subtitle_selection		(const gchar *		selection)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (subtitle_selections); ++i)
    if (0 == strcmp (subtitle_selections[i], selection))
      return i;

  return 0;
}

static void
activate_subtitle_selection	(GtkWidget *		mpeg_properties,
				 const gchar *		conf_name)
{
  gchar *zcname;
  const gchar *selection;
  gchar *wname;
  GtkWidget *widget;

  zcname = g_strconcat (zconf_root_temp, "/configs/",
			conf_name, "/vbi_selection", NULL);

  zconf_create_string (subtitle_selections[0],
		       "VBI subtitle page selection", zcname);
  selection = zconf_get_string (NULL, zcname);

  if (NULL != selection)
    selection = subtitle_selections[find_subtitle_selection (selection)];
  else
    selection = subtitle_selections[0];

  g_free (zcname);

  wname = g_strconcat ("subt-", selection, NULL);
  widget = lookup_widget (mpeg_properties, wname);
  g_free (wname);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
}

static void
init_subtitle_file_options	(GtkWidget *		mpeg_properties,
				 const gchar *		conf_name)
{
    GtkWidget *format_menu;
    GtkWidget *menu;
    gchar *zcname;
    gchar *format;
    const vbi3_export_info *xm;
    guint count;
    guint i;

    format_menu = lookup_widget (mpeg_properties, "optionmenu17");

    menu = gtk_menu_new ();
    gtk_widget_show (menu);

    gtk_option_menu_set_menu (GTK_OPTION_MENU (format_menu), menu);

    zcname = g_strconcat (zconf_root_temp, "/configs/",
			  conf_name, "/vbi_file_format", NULL);

    g_object_set_data_full (G_OBJECT (format_menu), "zcname",
			    zcname, g_free);

    zconf_get_string (&format, zcname);

    count = 0;

    for (i = 0; (xm = vbi3_export_info_enum ((int) i)); ++i)
      {
	if (xm->label && xm->open_format)
	  {
	    GtkWidget *menu_item;

	    menu_item = gtk_menu_item_new_with_label (xm->label);
	    gtk_widget_show (menu_item);
	    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

	    if (xm->tooltip)
	      z_tooltip_set (menu_item, xm->tooltip);

	    z_object_set_const_data (G_OBJECT (menu_item), "key", xm->keyword);

	    if (0 == count || (format && 0 == strcmp (xm->keyword, format)))
	      {
		on_vbi_format_menu_activate (menu_item, mpeg_properties);
		gtk_option_menu_set_history (GTK_OPTION_MENU (format_menu),
					     count);
	      }

	    g_signal_connect (G_OBJECT (menu_item), "activate",
			      G_CALLBACK (on_vbi_format_menu_activate),
			      mpeg_properties);

	    ++count;
	  }
      }

    g_free (format);
    format = NULL;
}

static void
save_subtitle_config		(GtkWidget *		mpeg_properties)
{
  gchar *zcname;
  GtkWidget *widget;
  gboolean active;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (subtitle_modes); ++i)
    {
      gchar *wname;

      wname = g_strconcat ("subt-", subtitle_modes[i], NULL);
      widget = lookup_widget (mpeg_properties, wname);
      g_free (wname);

      active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
      if (active)
	{
	  zcname = g_strconcat (zconf_root_temp, "/configs/",
				record_config_name, "/vbi_mode", NULL);

	  zconf_set_string (subtitle_modes[i], zcname);

	  g_free (zcname);
	  zcname = NULL;

	  break;
	}
    }

  /* vbi_file_format and its options take care of themselves,
     except for row_update. */

  {
    widget = lookup_widget (mpeg_properties, "subt-row-update");
    active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

    zcname = g_strconcat (zconf_root_temp, "/configs/",
			  record_config_name, "/vbi_row_update", NULL);

    zconf_set_boolean (active, zcname);

    g_free (zcname);
    zcname = NULL;
  }

  for (i = 0; i < G_N_ELEMENTS (subtitle_selections); ++i)
    {
      gchar *wname;

      wname = g_strconcat ("subt-", subtitle_selections[i], NULL);
      widget = lookup_widget (mpeg_properties, wname);
      g_free (wname);

      active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
      if (active)
	{
	  zcname = g_strconcat (zconf_root_temp, "/configs/",
				record_config_name, "/vbi_selection", NULL);

	  zconf_set_string (subtitle_selections[i], zcname);

	  g_free (zcname);
	  zcname = NULL;

	  break;
	}
    }

  {
    const gchar *buffer;

    widget = lookup_widget (mpeg_properties, "subt-page-entry");
    buffer = gtk_entry_get_text (GTK_ENTRY (widget));

    zcname = g_strconcat (zconf_root_temp, "/configs/",
			  record_config_name, "/vbi_pages", NULL);

    zconf_set_string (buffer, zcname);

    g_free (zcname);
    zcname = NULL;
  }
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
  GtkWidget *menu;
  GtkWidget *widget;
  guint default_item;

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
  g_signal_connect (G_OBJECT (menu), "selection-done",
		    G_CALLBACK (on_file_format_changed), mpeg_properties);

  widget = gtk_menu_get_active (GTK_MENU (GTK_OPTION_MENU (widget)->menu));

  if (widget)
    {
      char *keyword;

      keyword = g_object_get_data (G_OBJECT (widget), "keyword");
  
      select_file_format (mpeg_properties, conf_name, keyword);
    }

  /* preliminary */
  {
    gchar *zcname;

    zcname = g_strconcat (zconf_root_temp, "/configs/",
			  conf_name, "/capture_width", NULL);
    zconf_create_int (384, "Capture width", zcname);
    zconf_get_int (&capture_w, zcname);
    g_free (zcname);
    widget = lookup_widget (mpeg_properties, "spinbutton9");
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), (gdouble) capture_w);

    zcname = g_strconcat (zconf_root_temp, "/configs/",
			  conf_name, "/capture_height", NULL);
    zconf_create_int (288, "Capture height", zcname);
    zconf_get_int (&capture_h, zcname);
    g_free (zcname);
    widget = lookup_widget (mpeg_properties, "spinbutton10");
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), (gdouble) capture_h);
  }

  {
    widget = lookup_widget (mpeg_properties, "subt-open");
    gtk_widget_set_sensitive (widget, FALSE);
    widget = lookup_widget (mpeg_properties, "subt-closed");
    gtk_widget_set_sensitive (widget, FALSE);
    widget = lookup_widget (mpeg_properties, "alignment1");
    gtk_widget_set_sensitive (widget, FALSE);
    widget = lookup_widget (mpeg_properties, "optionmenu16");
    gtk_widget_set_sensitive (widget, FALSE);
    widget = lookup_widget (mpeg_properties, "subt-all");
    gtk_widget_set_sensitive (widget, FALSE);

    widget = lookup_widget (mpeg_properties, "subt-file");
    on_subtitle_file_toggled (GTK_TOGGLE_BUTTON (widget), mpeg_properties);
    g_signal_connect (G_OBJECT (widget), "toggled",
		      G_CALLBACK (on_subtitle_file_toggled), mpeg_properties);

    activate_subtitle_mode_button (mpeg_properties, conf_name);

    activate_subtitle_selection	(mpeg_properties, conf_name);

    init_subtitle_file_options (mpeg_properties, conf_name);

    {
      gchar *zcname;

      zcname = g_strconcat (zconf_root, "/configs/",
			    record_config_name, "/vbi_row_update", NULL);
      zconf_create_boolean
	(FALSE, "Update roll-up caption at row granularity", zcname);

      widget = lookup_widget (mpeg_properties, "subt-row-update");
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget),
				    zconf_get_boolean (NULL, zcname));

      g_free (zcname);
      zcname = NULL;
    }

    {
      gchar *zcname;

      zcname = g_strconcat (zconf_root, "/configs/",
			    record_config_name, "/vbi_pages", NULL);
      zconf_create_string ("", "Subtitle pages to record", zcname);

      widget = lookup_widget (mpeg_properties, "subt-page-entry");
      gtk_entry_set_text (GTK_ENTRY (widget),
			  zconf_get_string (NULL, zcname));

      g_free (zcname);
      zcname = NULL;
    }
  }
}

static void on_pref_config_changed (GtkWidget *, GtkWidget *);

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
  GtkWidget *configs;
  gchar *why;
  gint nformats;

  configs = lookup_widget (page, "optionmenu15");
  nformats = record_config_menu_attach (zconf_root_temp, configs,
					default_item);

  g_signal_connect (G_OBJECT (GTK_OPTION_MENU (configs)->menu),
		      "selection-done",
		      G_CALLBACK (on_pref_config_changed), page);

  why = NULL;
  z_set_sensitive_with_tooltip (configs, nformats > 0, NULL, why);
  z_set_sensitive_with_tooltip (lookup_widget (page, "delete"), nformats > 0, NULL, why);
  z_set_sensitive_with_tooltip (lookup_widget (page, "notebook2"), nformats > 0, NULL, why);

  if (nformats > 0)
    on_pref_config_changed (NULL, page);
}

static void
pref_cancel			(GtkWidget *		page _unused_)
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

      save_subtitle_config (page);
    }
}

static void
pref_apply			(GtkWidget *		page)
{
  gchar *buffer;

  save_current_config (page);

  /* Replace old configs by changed ones and delete temp */

  buffer = g_strconcat (zconf_root_temp, "/configs", NULL);
  zconf_copy (zconf_root, buffer);
  g_free (buffer);

  zconf_delete (zconf_root_temp);

  saving_dialog_attach_formats ();
}

static void
on_config_new_clicked			(GtkButton *	       button _unused_,
					 GtkWidget *		page)
{
  gchar *name;

  name = Prompt (GTK_WIDGET (zapping),
		 _("New format"),
		 _("Format name:"),
		 NULL);

  if (!name || !*name)
    {
      g_free (name);
      return;
    }

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
}

static void
on_config_delete_clicked		(GtkButton *	       button _unused_,
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
on_pref_config_changed		(GtkWidget *		menu _unused_,
				 GtkWidget *		page)
{
  GtkWidget *configs;
  const gchar *conf;

  configs = lookup_widget (page, "optionmenu15");
  conf = record_config_menu_get_active (configs);

  if (!conf || strcmp(conf, record_config_name) == 0)
    return;

  save_current_config (page);

  g_free (record_config_name);
  record_config_name = g_strdup (conf);

  rebuild_config_dialog (page, record_config_name);

  if (saving_dialog)
    {
      configs = lookup_widget (saving_dialog, "optionmenu14");
      record_config_menu_set_active (configs, record_config_name);
    }
}

static void
pref_setup			(GtkWidget *		page)
{
  GtkWidget *new = lookup_widget (page, "new");
  GtkWidget *delete = lookup_widget (page, "delete");
  gchar *buffer;

  /* All changes preliminary until ok/apply */

  buffer = g_strconcat (zconf_root, "/configs", NULL);
  zconf_copy (zconf_root_temp, buffer);
  g_free (buffer);

  pref_rebuild_configs (page, record_config_name);

  /* Load context_prop even if nothing configured yet */
  rebuild_config_dialog (page, record_config_name);

  g_signal_connect (G_OBJECT (new), "clicked",
		      G_CALLBACK (on_config_new_clicked), page);
  g_signal_connect (G_OBJECT (delete), "clicked",
		      G_CALLBACK (on_config_delete_clicked), page);
}

/*
 *  Saving dialog
 */

static gboolean
on_saving_delete_event		(GtkWidget *		widget _unused_,
				 GdkEvent *		event _unused_,
				 gpointer		user_data _unused_)
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

  return g_strndup (info->extension, (guint)(s - info->extension));
}

static void
on_saving_format_changed   	(GtkWidget *		menu _unused_,
				 gpointer		user_data _unused_)
{
  GtkWidget *configs;
  GtkWidget *entry;
  gchar *ext;

  g_assert (saving_dialog != NULL);

  /* Replace filename extension in entry according to selected format */

  configs = lookup_widget (saving_dialog, "optionmenu14");
  entry = lookup_widget (saving_dialog, "entry1");

  ext = file_format_ext (record_config_menu_get_active (configs));
  z_electric_replace_extension (entry, ext);
  g_free (ext);
}

static void
on_saving_configure_clicked	(GtkButton *		button,
				 gpointer		user_data _unused_)
{
  g_assert (saving_dialog != NULL);

  gtk_widget_set_sensitive (saving_dialog, FALSE);

  python_command_printf (GTK_WIDGET (button),
			 "zapping.properties('%s', '%s')",
			 _("Plugins"), _("Record"));

  gtk_widget_set_sensitive (saving_dialog, TRUE);

  saving_dialog_attach_formats ();
}

static void
on_saving_filename_changed   	(GtkWidget *		widget,
				 gpointer		user_data _unused_)
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
on_saving_stop_clicked		(GtkButton *		button _unused_,
				 gpointer		user_data _unused_)
{
  g_assert (saving_dialog != NULL);

  gtk_widget_destroy (saving_dialog);
  saving_dialog = NULL;

  do_stop ();
}

static void
on_saving_record_clicked	(GtkButton *		button _unused_,
				 gpointer		user_data _unused_)
{
  GtkToggleButton *record;
  GtkWidget *widget;
  const gchar *buffer;

  g_assert (saving_dialog != NULL);

  if (active)
    return;

  record = GTK_TOGGLE_BUTTON (lookup_widget (saving_dialog, "record"));

  /* Do not start recording if case we are _unset_ */
  if (!gtk_toggle_button_get_active (record))
    return;

  widget = lookup_widget (saving_dialog, "optionmenu14");
  buffer = record_config_menu_get_active (widget);

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
      active = TRUE;
      gtk_toggle_button_set_active (record, FALSE);
      active = FALSE;
    }

  return;

 reject:
  gtk_toggle_button_set_active (record, FALSE);
  return;
}

static void
saving_dialog_attach_formats	(void)
{
  GtkWidget *configs;
  GtkWidget *entry;
  gint nformats;
  gchar *ext;
  gchar *name;
  gchar *base_name;

  if (!saving_dialog)
    return;

  configs = lookup_widget (saving_dialog, "optionmenu14");
  entry = lookup_widget (saving_dialog, "entry1");

  nformats = record_config_menu_attach (zconf_root, configs,
					record_config_name);
  z_set_sensitive_with_tooltip (configs, nformats > 0, NULL, NULL);

  /*
   *  Proposed filename is last one, with appropriate extension
   *  and numeral suffix++ if file exists.
   */

  ext = file_format_ext (record_config_menu_get_active (configs));
  name = find_unused_name (NULL, record_option_filename, ext);

  gtk_entry_set_text (GTK_ENTRY (entry), name);
  base_name = g_path_get_basename (name);
  z_electric_set_basename (entry, base_name);

  g_free (base_name);
  g_free (name);
  g_free (ext);

  g_signal_connect (G_OBJECT (entry), "changed",
		      G_CALLBACK (z_on_electric_filename),
		      (gpointer) NULL);

  gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);

  if (nformats > 0)
    {
      GtkWidget *optionmenu;

      if (!active)
	gtk_widget_set_sensitive (lookup_widget (saving_dialog, "record"), TRUE);

      optionmenu = lookup_widget (saving_dialog, "optionmenu14");

      g_signal_connect (G_OBJECT (GTK_OPTION_MENU (optionmenu)->menu),
			"selection-done",
			G_CALLBACK (on_saving_format_changed), NULL);
    }
  else
    {
      gtk_widget_set_sensitive (lookup_widget (saving_dialog, "record"), FALSE);
    }
}

#if 0
static GtkWidget *
saving_popup_new		(void)
{
  static gint times[] = { 15, 30, 45, 60, 90, 120, 180 };
  GtkWidget *menu;
  guint i;

  menu = build_widget ("menu1", "mpeg_properties.glade2");

  gtk_widget_set_sensitive (lookup_widget (menu, "record_this"), FALSE);
  gtk_widget_set_sensitive (lookup_widget (menu, "record_next"), FALSE);

  for (i = 0; i < sizeof(times) / sizeof (times[0]); ++i)
    {
      GtkWidget *menu_item;
      gchar *buffer = g_strdup_printf("Record %d min", times[i]);

      menu_item = gtk_menu_item_new_with_label (buffer);
      g_free (buffer);

      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    }

  return menu;
}
#endif

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
saving_dialog_new_pixmap_table	(const GdkPixdata *	pixdata,
				 const gchar *		table_name)
{
  GtkWidget *pixmap;

  pixmap = z_gtk_image_new_from_pixdata (pixdata);

  if (pixmap)
    {
      GtkWidget *table;

      table = lookup_widget (saving_dialog, table_name);
      gtk_widget_show (pixmap);
      gtk_table_attach (GTK_TABLE (table), pixmap, 0, 1, 0, 1, 0, 0, 3, 0);
    }
}

static void
saving_dialog_new_pixmap_box	(const GdkPixdata *	pixdata,
				 const gchar *		box_name)
{
  GtkWidget *pixmap;

  pixmap = z_gtk_image_new_from_pixdata (pixdata);

  if (pixmap)
    {
      GtkWidget *box;

      box = lookup_widget (saving_dialog, box_name);
      gtk_widget_show (pixmap);
      gtk_box_pack_start (GTK_BOX (box), pixmap, FALSE, FALSE, 0);
    }
}

static void
saving_dialog_new		(gboolean		recording)
{
  GtkWidget *widget;
  GtkWidget *record, *stop;

  if (saving_dialog)
    gtk_widget_destroy (saving_dialog);

  saving_dialog = build_widget ("window3", "mpeg_properties.glade2");

  saving_dialog_new_pixmap_table (&time_png, "table4");
  saving_dialog_new_pixmap_table (&drop_png, "table5");
  saving_dialog_new_pixmap_table (&disk_empty_png, "table7");
  saving_dialog_new_pixmap_table (&volume_png, "table8");

  saving_dialog_new_pixmap_box (&record_png, "hbox20");
  saving_dialog_new_pixmap_box (&pause_png, "hbox22");
  saving_dialog_new_pixmap_box (&stop_png, "hbox24");

  saving_dialog_attach_formats ();
  /*
  gnome_popup_menu_attach (saving_popup = saving_popup_new (),
			   saving_dialog, NULL);
  */
  g_signal_connect (G_OBJECT (saving_dialog),
		    "delete-event",
		    G_CALLBACK (on_saving_delete_event), NULL);

  g_signal_connect (G_OBJECT (lookup_widget (saving_dialog, "configure_format")),
		    "clicked",
		    G_CALLBACK (on_saving_configure_clicked), NULL);
  g_signal_connect (G_OBJECT (lookup_widget (saving_dialog, "entry1")),
		    "changed",
		    G_CALLBACK (on_saving_filename_changed), NULL);

  record = lookup_widget (saving_dialog, "record");
  if (recording) {
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (record), TRUE);
    gtk_widget_set_sensitive (record, FALSE);
  }

  g_signal_connect (G_OBJECT (record), "clicked",
		    G_CALLBACK (on_saving_record_clicked), NULL);

  stop = lookup_widget (saving_dialog, "stop");
  gtk_widget_set_sensitive (stop, recording);

  g_signal_connect (G_OBJECT (stop), "clicked",
		    G_CALLBACK (on_saving_stop_clicked), NULL);

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

static PyObject*
py_stoprec (PyObject *self _unused_, PyObject *args _unused_)
{
  saving_dialog_delete ();

  do_stop ();

  py_return_true;
}

static PyObject*
py_pauserec (PyObject *self _unused_, PyObject *args _unused_)
{
  py_return_false;
}

static PyObject*
py_quickrec (PyObject *self _unused_, PyObject *args _unused_)
{
  gchar *ext;
  gchar *name;
  gboolean success;

  if (saving_dialog || active)
    py_return_false;

  if (!record_config_name[0])
    py_return_false;

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

  return PyInt_FromLong (success);
}

static PyObject*
py_record (PyObject *self _unused_, PyObject *args _unused_)
{
  if (saving_dialog || active)
    py_return_false;

  saving_dialog_new (FALSE);

  py_return_true;
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
  zconf_delete ("/zapping/plugins/mpeg");
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
  guint n = strlen (root_key);

  D();

  g_assert (n > 0 && root_key[n - 1] == '/');
  zconf_root = g_strndup (root_key, n - 1);
  zconf_root_temp = g_strconcat (zconf_root, "/temp", NULL);

  buffer = g_strconcat (root_key, "config", NULL);
  zconf_create_string ("", "Last config", buffer);
  zconf_get_string (&record_config_name, buffer);
  g_free (buffer);

  D();

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
  /* Not good, and we stop capturing ourselves in do_start(). */
  /* saving_dialog_delete (); */

  do_stop ();
}

static gboolean
plugin_start			(void)
{
  saving_dialog_new (FALSE);

  return TRUE;
}

static void
plugin_remove_gui		(GnomeApp *		app _unused_)
{
  GtkWidget *button;

  button = GTK_WIDGET (g_object_get_data (G_OBJECT (zapping), "mpeg_button"));
  gtk_container_remove (GTK_CONTAINER (zapping->toolbar), button);
}

static const gchar *tooltip = N_("Record a video stream");

static void
plugin_add_gui			(GnomeApp *		app _unused_)
{
  GtkToolItem *tool_item;

  tool_item = gtk_tool_button_new (NULL, _("Record"));
  gtk_widget_show (GTK_WIDGET (tool_item));

  gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON (tool_item),
				"zapping-recordtb");

  z_tooltip_set (GTK_WIDGET (tool_item), _(tooltip));

  z_signal_connect_python (G_OBJECT (tool_item), "clicked",
			   "zapping.record()");

  gtk_toolbar_insert (zapping->toolbar, tool_item, APPEND);

  /* Set up the widget so we can find it later */
  g_object_set_data (G_OBJECT (zapping), "mpeg_button", tool_item);
}

static void
plugin_process_popup_menu	(GtkWidget *		widget _unused_,
				 GdkEventButton	*	event _unused_,
				 GtkMenu *		popup)
{
  GtkWidget *menuitem;

  menuitem = gtk_menu_item_new ();
  gtk_widget_show (menuitem);
  gtk_menu_shell_append (GTK_MENU_SHELL (popup), menuitem);

  menuitem = z_gtk_pixmap_menu_item_new (_("Record"),
					 GTK_STOCK_SELECT_COLOR);
  z_tooltip_set (menuitem, _(tooltip));

  z_signal_connect_python (G_OBJECT (menuitem), "activate",
			   "zapping.record()");

  gtk_widget_show (menuitem);
  gtk_menu_shell_append (GTK_MENU_SHELL (popup), menuitem);
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
plugin_get_public_info		(gint			i _unused_,
				 gpointer *		ptr _unused_,
				 gchar **		symbol _unused_,
				 gchar **		description _unused_,
				 gchar **		type _unused_,
				 gint *			hash _unused_)
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

  saving_dialog_delete ();

  if (export_prop)
    vbi3_export_delete (export_prop);
  export_prop = NULL;

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
properties_add			(GtkDialog *		dialog)
{
  SidebarEntry plugin_options[] = {
    {
      N_("Record"),
      "gnome-media-player.png",
      "vbox9",
      .setup = pref_setup,
      .apply = pref_apply,
      .cancel = pref_cancel,
      .help_link_id = "zapping-settings-recording"
    }
  };
  SidebarGroup groups[] = {
    {
      N_("Plugins"),
      plugin_options,
      1
    }
  };

  standard_properties_add (dialog, groups, G_N_ELEMENTS (groups),
			   "mpeg_properties.glade2");
}

static gboolean
plugin_init			(PluginBridge		bridge _unused_,
				 tveng_device_info *	info)
{
  property_handler mpeg_handler = {
    add: properties_add
  };

  D();

  zapping_info = info;

  append_property_handler (&mpeg_handler);

  D();

  cmd_register ("record", py_record, METH_VARARGS,
		("Record dialog"), "zapping.record()");
  cmd_register ("quickrec", py_quickrec, METH_VARARGS,
		("Start recording"), "zapping.quickrec()");
  cmd_register ("pauserec", py_pauserec, METH_VARARGS,
		("Pause recording"), "zapping.pauserec()");
  cmd_register ("stoprec", py_stoprec, METH_VARARGS,
		("Stop recording"), "zapping.stoprec()");

  return TRUE;
}

gboolean
plugin_get_symbol		(const gchar *		name,
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

#endif /* HAVE_LIBRTE */

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
