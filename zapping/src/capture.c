/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2002 Iñaki García Etxebarria
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

/**
 * Capture management code. The logic is somewhat complex since we
 * have to support many combinations of request, available and
 * displayable formats, and try to find an optimum combination.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#define ZCONF_DOMAIN "/zapping/options/capture/"
#include "zconf.h"

#include <pthread.h>

#include "zvbi.h" /* for vbi_push_video */

#include <tveng.h>
#include "../common/fifo.h"
#include "zmisc.h"
#include "plugins.h"
#include "capture.h"
#include "csconvert.h"
#include "globals.h"

/* The capture fifo */
#define NUM_BUNDLES 6 /* in capture_fifo */
static fifo				_capture_fifo;
fifo					*capture_fifo = &_capture_fifo;
/* The frame producer */
static pthread_t			capture_thread_id;
static volatile gboolean		exit_capture_thread;
static pthread_rwlock_t size_rwlock;
/* List of requested capture formats */
static struct {
  gint			id;
  capture_fmt		fmt;
  gboolean		required;
}		*formats = NULL;
static int	num_formats = 0;
static pthread_rwlock_t fmt_rwlock;
static volatile gint	request_id = 0;
/* Capture handlers */
static struct {
  CaptureNotify		notify;
  gpointer		user_data;
  gint			id;
}					*handlers = NULL;
static int				num_handlers = 0;
/* The capture buffers */
typedef struct {
  /* What everyone else sees */
  capture_frame		frame;
  /* When this tag is different from request_id the buffer needs
     rebuilding */
  gint			tag;
  /* Images we can build for this buffer */
  gint			num_images;
  zimage		*images[TVENG_PIX_LAST];
  gboolean		converted[TVENG_PIX_LAST];
  /* The source image and its index in images */
  zimage		*src_image;
  gint			src_index;
} producer_buffer;
/* Available video formats in the video device (bitmask) */
static int		available_formats = 0;

/* For debugging */
static const char *mode2str (enum tveng_frame_pixformat fmt)
     __attribute__ ((unused));

static void broadcast (capture_event event)
{
  gint i;

  for (i=0; i<num_handlers; i++)
    handlers[i].notify (event, handlers[i].user_data);
}

static void
free_bundle (buffer *b)
{
  producer_buffer *pb = (producer_buffer*)b;
  gint i;

  for (i=0; i<pb->num_images; i++)
    zimage_unref (pb->images[i]);

  g_free (b);
}

static void
fill_bundle_tveng (producer_buffer *p, tveng_device_info *info)
{
  if (p->src_image)
    tveng_read_frame (&p->src_image->data, 50, info);
  else
    tveng_read_frame (NULL, 50, info);
  p->frame.timestamp = tveng_get_timestamp (info);

  memset (&p->converted, 0, sizeof (p->converted));
  p->converted[p->src_index] = TRUE;
}

static gint build_mask (gboolean allow_suggested)
{
  gint mask = 0;
  gint i;
  for (i=0; i<num_formats; i++)
    if (allow_suggested || formats[i].required)
      mask |= 1<<formats[i].fmt.fmt;

  return mask;
}

/* Check whether the given buffer can hold the current request */
static gboolean
compatible (producer_buffer *p, tveng_device_info *info)
{
  gint i, avail_mask = 0, retvalue;

  /* No images, it's always failure */
  if (!p->num_images)
    return FALSE;

  pthread_rwlock_rdlock (&fmt_rwlock);
  /* first check whether the size is right */
  if (info->format.width != p->images[0]->fmt.width ||
      info->format.height != p->images[0]->fmt.height)
    retvalue = FALSE;
  else /* size is ok, check pixformat */
    {
      for (i=0; i<p->num_images; i++)
	avail_mask |= 1<<p->images[i]->fmt.pixformat;

      retvalue = !!((avail_mask | build_mask (FALSE)) == avail_mask);
    }
  pthread_rwlock_unlock (&fmt_rwlock);

  return retvalue;
}

static void *
capture_thread (void *data)
{
  tveng_device_info *info = (tveng_device_info*)data;
  producer prod;

  add_producer(capture_fifo, &prod);

  while (!exit_capture_thread)
    {
      producer_buffer *p =
	(producer_buffer*)wait_empty_buffer(&prod);

      /*
	check whether this buffer needs rebuilding. We don't do it
	here but defer it to the main thread, since rebuilding buffers
	typically requires X calls, and those don't work across
	multiple threads (actually they do, but relaying on that is
	just asking for trouble, it's too complex to get right).
	Just seeing that the buffer is old doesn't mean that it's
	unusable, if compatible is TRUE then we fill it normally.
      */

      pthread_rwlock_rdlock (&size_rwlock);

      if (p->tag != request_id && !compatible (p, info))
	{
	  /* schedule for rebuilding in the main thread */
	  p->frame.b.used = 1; /* used==0 indicates eof */
	  send_full_buffer(&prod, &p->frame.b);
	  pthread_rwlock_unlock (&size_rwlock);
	  continue;
	}

      fill_bundle_tveng(p, info);

      pthread_rwlock_unlock (&size_rwlock);

      p->frame.b.time = p->frame.timestamp;
      if (p->src_image)
	p->frame.b.used = p->src_image->fmt.sizeimage;
      else
	p->frame.b.used = 1;

      send_full_buffer(&prod, &p->frame.b);
    }

  rem_producer(&prod);

  return NULL;
}

/*
  If the device has never been scanned for available modes before
  this routine checks for that. Otherwise the appropiate values are
  loaded from the configuration.
  Returns a bitmask indicating which modes are available.
*/
static uint32_t
scan_device		(tveng_device_info	*info)
{
  gchar *key;
  int values = 0, fmt;
  enum tveng_frame_pixformat old_fmt;

  key = g_strdup_printf (ZCONF_DOMAIN "%x/scanned", info->signature);
  if (zconf_get_boolean (NULL, key))
    {
      g_free (key);
      key = g_strdup_printf (ZCONF_DOMAIN "%x/available_formats",
			     info->signature);
      zconf_get_integer (&values, key);
      g_free (key);
      return values;
    }

  old_fmt = info->format.pixformat;

  /* Things are simpler this way. */
  g_assert (TVENG_PIX_FIRST == 0);
  g_assert (TVENG_PIX_LAST < (sizeof (int)*8));
  for (fmt = TVENG_PIX_FIRST; fmt <= TVENG_PIX_LAST; fmt++)
    {
      if (fmt == TVENG_PIX_GREY)
	continue;
      info->format.pixformat = fmt;
      if (!tveng_set_capture_format (info))
	values += 1<<fmt;
    }

  info->format.pixformat = old_fmt;
  tveng_set_capture_format (info);

  zconf_create_boolean (TRUE,
			"Whether this device has been already scanned",
			key);
  g_free (key);

  key = g_strdup_printf (ZCONF_DOMAIN "%x/available_formats",
			 info->signature);
  zconf_create_integer (values, "Bitmaps of available pixformats", key);
  g_free (key);

  return values;
}

/*
 * This is an special consumer. It could have been splitted into these
 * three different consumers, each one with a very specific job:
 * a) Rebuild:
 *	The producers and the main (GTK) loop are in different
 *	threads. Producers defer the job of rebuilding the bundles to
 *	this consumer.
 * b) Display:
 *	The consumer that blits the data into the tvscreen.
 * c) Plugins:
 *	Passes the data to the serial_read plugins.
 */

static consumer			__ctc, *cf_timeout_consumer = &__ctc;
static gint			idle_id;
static gint idle_handler(gpointer _info)
{
  buffer *b;
  struct timespec t;
  struct timeval now;
  producer_buffer *pb;
  tveng_device_info *info = (tveng_device_info*)_info;

  gettimeofday (&now, NULL);

  t.tv_sec = now.tv_sec;
  t.tv_nsec = (now.tv_usec + 50000)* 1000;

  if (t.tv_nsec >= 1e9)
    {
      t.tv_nsec -= 1e9;
      t.tv_sec ++;
    }

  b = wait_full_buffer_timeout (cf_timeout_consumer, &t);
  if (!b)
    return TRUE; /* keep calling me */

  pb = (producer_buffer*)b;

  if (pb->tag == request_id)
    {
      capture_frame *cf = (capture_frame*)pb;
      GList *p = plugin_list;
      /* Draw the image */
      video_blit_frame (cf);
      
      /* Pass the data to the plugins */
      while (p)
	{
	  plugin_read_frame (cf, (struct plugin_info*)p->data);
	  p = p->next;
	}
    }
  else /* Rebuild time */
    {
      int mask = build_mask (TRUE);
      int i;

      /* Remove items in the current array */
      for (i=0; i<pb->num_images; i++)
	zimage_unref (pb->images[i]);

      pb->src_image = NULL;

      /* Get number of requested modes */
      for (i=0, pb->num_images=0; i<TVENG_PIX_LAST; i++)
	if ((1<<i) & (mask | (1<<info->format.pixformat)))
	  {
	    pb->images[pb->num_images] =
	      zimage_new (i, info->format.width,
			  info->format.height);

	    if (info->format.pixformat == i)
	      {
		pb->src_index = pb->num_images;
		pb->src_image = pb->images[pb->num_images];
	      }
	    pb->num_images++;
	  }
      g_assert (pb->src_image != NULL);
      pb->tag = request_id; /* Done */
    }

  send_empty_buffer (cf_timeout_consumer, b);

  return TRUE; /* keep calling me */
}

static void
on_capture_canvas_allocate             (GtkWidget       *widget,
                                        GtkAllocation   *allocation,
                                        tveng_device_info *info)
{
  capture_fmt fmt;

  memset (&fmt, 0, sizeof (fmt));

  fmt.fmt = info->format.pixformat;
  fmt.width = allocation->width;
  fmt.height = allocation->height;

  request_capture_format (&fmt);
}

gint capture_start (tveng_device_info *info)
{
  int i;
  buffer *b;

  if (tveng_start_capturing (info) == -1)
    {
      ShowBox (_("Cannot start capturing: %s"),
	       GTK_MESSAGE_ERROR, info->error);
      return FALSE;
    }

  init_buffered_fifo (capture_fifo, "zapping-capture", 0, 0);

  for (i=0; i<NUM_BUNDLES; i++)
    {
      g_assert ((b = g_malloc0(sizeof(producer_buffer))));
      b->destroy = free_bundle;
      add_buffer (capture_fifo, b);
    }

  add_consumer (capture_fifo, cf_timeout_consumer);

  exit_capture_thread = FALSE;
  g_assert (!pthread_create (&capture_thread_id, NULL, capture_thread,
			     main_info));

  available_formats = scan_device (main_info);

  idle_id = gtk_idle_add (idle_handler, info);

  /* XXX */
  g_signal_connect (G_OBJECT (lookup_widget (main_window, "tv-screen")),
		    "size-allocate",
		    GTK_SIGNAL_FUNC (on_capture_canvas_allocate),
		    main_info);

  return TRUE;
}

void capture_stop (void)
{
  GList *p;
  buffer *b;

  /* XXX */
  g_signal_handlers_disconnect_by_func
    (G_OBJECT (lookup_widget (main_window, "tv-screen")),
     GTK_SIGNAL_FUNC (on_capture_canvas_allocate),
     main_info);

  /* First tell all well-behaved consumers to stop */
  broadcast (CAPTURE_STOP);
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_capture_stop((struct plugin_info*)p->data);
      p = p->next;
    }  

  /* Stop our marvellous consumer */
  gtk_idle_remove (idle_id);
  idle_id = -1;

  /* Let the capture thread go to a better place */
  exit_capture_thread = TRUE;
  /* empty full queue and remove timeout consumer */
  while ((b = recv_full_buffer (cf_timeout_consumer)))
    send_empty_buffer (cf_timeout_consumer, b);
  rem_consumer (cf_timeout_consumer);
  pthread_join (capture_thread_id, NULL);

  /* Free handlers and formats */
  g_free (handlers);
  handlers = NULL;
  num_handlers = 0;

  g_free (formats);
  formats = NULL;
  num_formats = 0;

  destroy_fifo (capture_fifo);
}

/*
  Finds the first request in (formats, fmt) that gives a size, and
  stores it in width, height. If nothing gives a size, then uses some
  defaults.
*/
static gboolean
find_request_size (capture_fmt *fmt, gint *width, gint *height)
{
  gint i;
  for (i=0; i<num_formats; i++)
    if (formats[i].fmt.locked)
      {
	*width = formats[i].fmt.width;
	*height = formats[i].fmt.height;
	return TRUE;
      }

  if (fmt && fmt->locked)
    {
      *width = fmt->width;
      *height = fmt->height;
      return TRUE;
    }

  /* Not specified, use config defaults */
  /* FIXME: Query zconf and use the values in there */

  /* mhs FIXME: The intention isn't clear to me. */

#if 0
  *width = 320;
  *height = 240;
#else
  {
    GtkWidget *widget;

    widget = lookup_widget (main_window, "tv-screen");

    *width = MAX (64, widget->allocation.width);
    *height = MAX (64 * 3/4, widget->allocation.height);
  }
#endif

  return FALSE;
}

/*
 * The following routine tries to find the optimum tveng mode for the
 * current requested formats. Optionally, we can also pass an extra
 * capture_fmt, it will be treated just like it was a member of formats.
 * Upon success, the request is stored
 * and its id is returned. This routine isn't a complete algorithm, in
 * the sense that we don't try all possible combinations of
 * suggested/required modes when allow_suggested == FALSE. In fact,
 * since we will only have one suggested format it provides the
 * correct answer.
 * This routine could suffer some additional optimization, but
 * obfuscating it even more isn't a good thing.
 */
static gint
request_capture_format_real (capture_fmt *fmt, gboolean required,
			     gboolean allow_suggested,
			     tveng_device_info *info)
{
  gint mask;
  gint req_mask = fmt ? 1<<fmt->fmt : 0;
  gint i, id=0, k;
  static gint counter = 0;
  gint conversions = 999;
  struct tveng_frame_format prev_fmt;
  gint req_w, req_h;

  if (fmt)
    pthread_rwlock_wrlock (&fmt_rwlock);
  else
    pthread_rwlock_rdlock (&fmt_rwlock);

  mask = build_mask (allow_suggested);

  /* First do the easy check, whether the req size is available */
  /* Note that we assume that formats is valid [should be, since it's
     constructed by adding/removing] */
  if (fmt && fmt->locked)
    for (i=0; i<num_formats; i++)
      if (formats[i].fmt.locked &&
	  ((!formats[i].required) && allow_suggested) &&
	  (formats[i].fmt.width != fmt->width ||
	   formats[i].fmt.height != fmt->height))
	{
	  pthread_rwlock_unlock (&fmt_rwlock);
	  return -1;
	}

  /* Size is OK, try cs matching. First check whether the pixmode is
     available in the already requested mode. */
  if (mask & req_mask)
    {
      /* Keep the current format */
      id = info->format.pixformat;
      goto req_ok;
    }

  /* Not so simple :-) The following is a bit slow, but at least we
     won't give false negatives. */
  for (i=0; i<TVENG_PIX_LAST; i++)
    if (available_formats & (1<<i))
      {
	gint num_conversions = 0;
	/* See what would happen if we set mode i, which is supported
	   by the video hw */
	for (k=0; k<TVENG_PIX_LAST; k++)
	  if ((req_mask|mask) & (1<<k))
	    {
	      /* Mode k has been requested */
	      if ((i != k) && (!(lookup_csconvert (i, k))))
		/* For simplicity we don't allow converting a
		   converted image. Ain't that difficult to support,
		   but i think it's better just to fail than doing too
		   many cs conversions. */
		break; /* breaks the for (k=0; ...) */
	      else if (i != k)
		/* Available through a cs conversion */
		num_conversions ++;
	    }

	/* Candidate found, record it if it requires less conversions
	   than our last candidate */
	if ((num_conversions < conversions) &&
	    (k == TVENG_PIX_LAST))
	  {
	    conversions = num_conversions;
	    id = i;
	  }
      }

  /* Combo not possible, try disabling the suggested modes [all of
     them, otherwise it's a bit too complex] */
  if (conversions == 999)
    {
      if (!allow_suggested)
	{
	  pthread_rwlock_unlock (&fmt_rwlock);
	  return -1; /* No way, we were already checking without
			suggested modes */
	}
      pthread_rwlock_unlock (&fmt_rwlock);
      return request_capture_format_real (fmt, required, FALSE, info);
    }

 req_ok:
  /* We cannot change w/h while the thread runs: race. */
  pthread_rwlock_wrlock (&size_rwlock);

  /* Request the new format to TVeng (should succeed) [id] */
  memcpy (&prev_fmt, &info->format, sizeof (prev_fmt));
  info->format.pixformat = id;
#warning

  find_request_size (fmt, &req_w, &req_h);
    {
      info->format.width = req_w;
      info->format.height = req_h;
    }
  printv ("Setting TVeng mode %s [%d x %d]\n", mode2str(id), req_w, req_h);

  if (tveng_set_capture_format (info) == -1 ||
      info->format.width != req_w || info->format.height != req_h)
    {
      if (info->tveng_errno)
	g_warning ("Cannot set new mode: %s", info->error);
      /* Try to restore previous setup so we can keep working */
      memcpy (&info->format, &prev_fmt, sizeof (prev_fmt));

      if (tveng_set_capture_format (info) != -1)
	if (info->current_mode == TVENG_NO_CAPTURE
	    || info->current_mode == TVENG_TELETEXT)
	  tveng_start_capturing (info);
      pthread_rwlock_unlock (&size_rwlock);
      pthread_rwlock_unlock (&fmt_rwlock);
      return -1;
    }

  pthread_rwlock_unlock (&size_rwlock);

  /* Flags that the request list *might* have changed */
  request_id ++;
  broadcast (CAPTURE_CHANGE);

  if (fmt)
    {
      formats = g_realloc (formats, sizeof(*formats)*(num_formats+1));
      formats[num_formats].id = counter++;
      formats[num_formats].required = required;
      memcpy (&formats[num_formats].fmt, fmt, sizeof(*fmt));
      num_formats++;
      pthread_rwlock_unlock (&fmt_rwlock);
      printv ("Format %s accepted [%s]\n", mode2str(fmt->fmt),
	      mode2str(info->format.pixformat));
      /* Safe because we only modify in one thread */
      return formats[num_formats-1].id;
    }

  pthread_rwlock_unlock (&fmt_rwlock);
  return 0;
}

gint request_capture_format (capture_fmt *fmt)
{
  g_assert (fmt != NULL);

  printv ("%s requested\n", mode2str (fmt->fmt));

  return request_capture_format_real (fmt, TRUE, TRUE, main_info);
}

gint suggest_capture_format (capture_fmt *fmt)
{
  g_assert (fmt != NULL);

  printv ("%s suggested\n", mode2str (fmt->fmt));

  return request_capture_format_real (fmt, FALSE, TRUE, main_info);
}

void release_capture_format (gint id)
{
  gint index;

  pthread_rwlock_wrlock (&fmt_rwlock);

  for (index=0; index<num_formats; index++)
    if (formats[index].id == id)
      break;

  g_assert (index != num_formats);

  num_formats--;
  if (index != num_formats)
    memcpy (&formats[index], &formats[index+1],
	    (num_formats - index) * sizeof (formats[0]));

  formats = g_realloc (formats, num_formats*sizeof(formats[0]));

  pthread_rwlock_unlock (&fmt_rwlock);

  request_capture_format_real (NULL, FALSE, TRUE, main_info);
}

void
get_request_formats (capture_fmt **fmt, gint *num_fmts)
{
  gint i;

  pthread_rwlock_rdlock (&fmt_rwlock);
  *fmt = g_malloc0 (num_formats * sizeof (capture_fmt));

  for (i=0; i<num_formats; i++)
    memcpy (&fmt[i], &formats[i].fmt, sizeof (capture_fmt));

  *num_fmts = num_formats;
  pthread_rwlock_unlock (&fmt_rwlock);
}

gint
add_capture_handler (CaptureNotify notify, gpointer user_data)
{
  static int id = 0;

  handlers = g_realloc (handlers,
			sizeof (*handlers) * num_handlers);
  handlers[num_handlers].notify = notify;
  handlers[num_handlers].user_data = user_data;
  handlers[num_handlers].id = id++;
  num_handlers ++;

  return handlers[num_handlers-1].id;
}

void
remove_capture_handler (gint id)
{
  gint i;

  for (i=0; i<num_handlers; i++)
    if (handlers[i].id == id)
      break;

  g_assert (i != num_handlers);

  num_handlers--;
  if (i != num_handlers)
    memcpy (&handlers[i], &handlers[i+1],
	    (num_handlers-i)*sizeof(*handlers));
}

zimage*
retrieve_frame (capture_frame *frame,
		enum tveng_frame_pixformat fmt)
{
  gint i;
  producer_buffer *pb = (producer_buffer*)frame;

  for (i=0; i<pb->num_images; i++)
    if (pb->images[i]->fmt.pixformat == fmt)
      {
	if (!pb->converted[i])
	  {
	    zimage *src = pb->src_image;
	    zimage *dest = pb->images[i];
	    int id = lookup_csconvert (src->fmt.pixformat, fmt);
	    if (id == -1)
	      return NULL;

	    csconvert (id, &src->data, &dest->data, src->fmt.width,
		       src->fmt.height);
	    pb->converted[i] = TRUE;
	  }
	return pb->images[i];
      }

  return NULL;
}

void
startup_capture (void)
{
  pthread_rwlock_init (&fmt_rwlock, NULL);
  pthread_rwlock_init (&size_rwlock, NULL);
}

void
shutdown_capture (void)
{
  pthread_rwlock_destroy (&size_rwlock);
  pthread_rwlock_destroy (&fmt_rwlock);
}

static const char *mode2str (enum tveng_frame_pixformat fmt)
{
  char *modes[] = {
    "RGB555", "RGB565", "RGB24", "BGR24",
    "RGB32", "BGR32", "YVU420", "YUV420", "YUYV", "UYVY",
    "GREY"
  };

  return modes[fmt];
}
