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
#  include "config.h"
#endif

#include <gtk/gtk.h>

#define ZCONF_DOMAIN "/zapping/options/capture/"
#include "zconf.h"
#include "zgconf.h"
#include <pthread.h>
#include <tveng.h>
#include "common/fifo.h"
#include "zmisc.h"
#include "plugins.h"
#include "capture.h"
#include "csconvert.h"
#include "globals.h"

#include <sched.h>

#define _pthread_rwlock_rdlock(l) pthread_rwlock_rdlock (l)
#define _pthread_rwlock_wrlock(l) pthread_rwlock_wrlock (l)
#define _pthread_rwlock_unlock(l) pthread_rwlock_unlock (l)

/* The capture fifo FIXME must not exceed number of buffers in tveng queue. */
#define N_BUNDLES 8 /* in capture_fifo */

typedef struct {
  /* What everyone else sees */
  capture_frame			frame;

  /* Source image in driver buffer. Pointer will change with
     each iteration, can be NULL too. */
  const tv_capture_buffer *	src_buffer;

  /* Pixfmt converted copies of the source image. */
  zimage *			images[TV_MAX_PIXFMTS];

  /* Copied source image in images[] (if we have no src_buffer
     or a copy from mmapped to shared memory is necessary). */
  zimage *			src_image;

  /* See retrieve_frame(). */
  zimage			src_direct;

  /* We convert images on demand. */
  tv_pixfmt_set			converted;

  /* When this tag is different from request_id the buffer
     needs rebuilding (i.e. images must be reallocated). */
  gint				tag;
} producer_buffer;

static volatile gint		request_id;

typedef struct {
  gint				id;

  /* Requested image size. */
  guint				width;
  guint				height;

  /* Requested pixel formats. */
  tv_pixfmt_set			pixfmt_set;

  /* Granted pixel format. */
  tv_pixfmt			pixfmt;

  req_flags			flags;
} req_format;

static pthread_rwlock_t		fmt_rwlock;

/* List of requested capture formats. */
static req_format *		formats;
static guint			n_formats;


zf_fifo				capture_fifo;

static GtkWidget *		dest_window;


/* Called when a full capture buffer goes back to the empty queue.
   Note this may be executed by a different thread and the fifo is
   locked. */
static void
buffer_done                     (zf_fifo *              f _unused_,
                                 zf_buffer *            b)
{
  producer_buffer *pb = PARENT (b, producer_buffer, frame.b);

  if (pb->src_buffer)
    {
      if (0)
	fprintf (stderr, "/%p\n", pb->src_buffer);

      tv_queue_capture_buffer (zapping->info, pb->src_buffer);
      pb->src_buffer = NULL;
    }
}

static void
free_bundle			(zf_buffer *		b)
{
  producer_buffer *pb = PARENT (b, producer_buffer, frame.b);
  guint i;

  for (i = 0; i < G_N_ELEMENTS (pb->images); ++i)
    if (pb->images[i])
      zimage_unref (pb->images[i]);

  CLEAR (*b);

  g_free (b);
}

/* Note fmt_rwlock must be wlocked. */
static void
flush_buffers			(tveng_device_info *	info)
{
  node *n;

  if (tv_get_caps (info)->flags & TVENG_CAPS_QUEUE)
    {
      for (n = capture_fifo.buffers.head; n->succ; n = n->succ)
	{
	  producer_buffer *pb;

	  pb = PARENT (n, producer_buffer, frame.b.added);

	  if (pb->src_buffer)
	    {
	      pb->src_buffer = NULL;
	    }
	}
    }
}

/* returns: -1 error, 0 timeout, 1 ok */
static int
fill_bundle_tveng		(producer_buffer *	pb,
				 tveng_device_info *	info,
				 guint			time)
{
  struct timeval timeout;
  int r;

  timeout.tv_sec = 0;
  timeout.tv_usec = time * 1000;

  if (tv_get_caps (info)->flags & TVENG_CAPS_QUEUE)
    {
      if (pb->src_buffer)
	{
	  tv_queue_capture_buffer (zapping->info, pb->src_buffer);
	  pb->src_buffer = NULL;
	}

      pb->converted = 0;

      r = tv_dequeue_capture_buffer_with_timeout (info,
						  &pb->src_buffer,
						  &timeout);
      if (r <= 0)
	return r;

      pb->frame.timestamp = pb->src_buffer->sample_time.tv_sec
	+ pb->src_buffer->sample_time.tv_usec * (1 / 1e6);

      if (pb->src_image)
	{
	  pb->frame.b.data = pb->src_buffer->data;
	  pb->frame.b.used = pb->src_buffer->size;
	  pb->frame.b.time = pb->frame.timestamp;
	}
      else
	{
	  /* Discard. */

	  tv_queue_capture_buffer (zapping->info, pb->src_buffer);
	  pb->src_buffer = NULL;

	  pb->frame.b.data = NULL;
	  pb->frame.b.used = 1;
	}
    }
  else
    {
      if (pb->src_image)
	{
	  tv_capture_buffer buffer;

	  buffer.data = pb->src_image->img;
	  buffer.size = 0; /* XXX */
	  buffer.format = &pb->src_image->fmt;

	  r = tv_read_frame (info, &buffer, &timeout);
	  if (r <= 0)
	    return r;

	  pb->frame.timestamp = (buffer.sample_time.tv_sec
				 + buffer.sample_time.tv_usec * (1 / 1e6));

	  if (pb->frame.timestamp > 0)
	    {
	      pb->frame.b.data = pb->src_image->img;
	      pb->frame.b.used = pb->src_image->fmt.size;
	      pb->frame.b.time = pb->frame.timestamp;
	    }
	  else
	    {
	      pb->frame.b.data = NULL;
	      pb->frame.b.used = 1;
	    }

	  pb->converted =
	    TV_PIXFMT_SET (pb->src_image->fmt.pixel_format->pixfmt);
	}
      else
	{
	  /* Read & discard. */
	  r = tv_read_frame (info, NULL, &timeout);
	  if (r <= 0)
	    return r;

	  pb->frame.timestamp = 0;

	  pb->frame.b.data = NULL;
	  pb->frame.b.used = 1;

	  pb->converted = 0;
	}
    }

  return 1;
}

/* Check whether the given buffer can hold the current request. */
static gboolean
compatible			(producer_buffer *	pb,
				 tveng_device_info *	info)
{
  const tv_image_format *fmt;
  guint i;
  guint j;

  for (i = 0; i < G_N_ELEMENTS (pb->images); ++i)
    if (pb->images[i])
      break;

  if (i >= G_N_ELEMENTS (pb->images))
    return FALSE;

  /* First check whether the size is right. */

  fmt = tv_cur_capture_format (info);

  for (i = 0; i < G_N_ELEMENTS (pb->images); ++i)
    if (pb->images[i])
      if (fmt->width != pb->images[i]->fmt.width
	  || fmt->height != pb->images[i]->fmt.height)
	return FALSE;

  /* Size is ok, check pixformat. */

  for (i = 0; i < G_N_ELEMENTS (pb->images); ++i)
    if (pb->images[i])
      {
	tv_pixfmt pixfmt;

	pixfmt = pb->images[i]->fmt.pixel_format->pixfmt;

	for (j = 0; j < n_formats; ++j)
	  if (pixfmt == formats[j].pixfmt)
	    break;

	if (j >= n_formats)
	  return FALSE;
      }

  return TRUE;
}

static void
rebuild_buffer			(producer_buffer *	pb,
				 tveng_device_info *	info)
{
  const tv_image_format *fmt;
  guint i;

  fmt = tv_cur_capture_format (info);
  if (!fmt || !fmt->pixel_format)
    return;

  pb->src_image = NULL;

  for (i = 1; i < G_N_ELEMENTS (pb->images); ++i)
    {
      guint j;

      for (j = 0; j < n_formats; ++j)
	if (formats[j].pixfmt == (tv_pixfmt) i)
	  break;

      if (j >= n_formats)
	{
	  if (pb->images[i])
	    {
	      /* This pixfmt is no longer needed. */
	      zimage_unref (pb->images[i]);
	      pb->images[i] = NULL;
	    }
	}
      else
	{
	  if (pb->images[i])
	    {
	      if (pb->images[i]->fmt.width == fmt->width
		  && pb->images[i]->fmt.height == fmt->height)
		goto done;

	      /* Has right pixfmt but wrong size. */
	      zimage_unref (pb->images[i]);
	      pb->images[i] = NULL;
	    }

	  pb->images[i] = zimage_new ((tv_pixfmt) i, fmt->width, fmt->height);

	  /* At least the memory backend should "display" this. */
	  g_assert (NULL != pb->images[i]);

	done:
	  if (0)
	    fprintf (stderr, "rebuilt %p %s, capture %s\n",
		     pb, tv_pixfmt_name ((tv_pixfmt) i),
		     fmt->pixel_format->name);

	  if (fmt->pixel_format->pixfmt == (tv_pixfmt) i)
	    pb->src_image = pb->images[i];
	}
    }

  /* Possible if none of the capture formats supported by the driver are
     directly displayable or have been requested for other purposes. */
  if (NULL == pb->src_image)
    {
      i = (guint) fmt->pixel_format->pixfmt;

      pb->images[i] = zimage_new ((tv_pixfmt) i, fmt->width, fmt->height);
      g_assert (NULL != pb->images[i]);

      pb->src_image = pb->images[i];
    }

  pb->tag = request_id; /* Done */
}

static void
pass_frame_to_plugins		(producer_buffer *	pb)
{
  GList *p;

  for (p = plugin_list; p != NULL; p = p->next)
    {
      struct plugin_info *pi;

      pi = (struct plugin_info *) p->data;
      plugin_read_frame (&pb->frame, pi);
    }
}

/* Capture source. */

typedef struct {
  GSource			source; 
  GPollFD			poll_fd;
  tveng_device_info *		info;
  zf_producer			prod;
} capture_source;

static GSource *		source;
static gboolean			field2;
static gdouble			filter_time;
static gdouble			f2time;
static gdouble			field_balance = .45;
static producer_buffer		display_buffers[2];
static display_filter_fn *	display_filter;

static gboolean
capture_source_prepare		(GSource *		source _unused_,
				 gint *			timeout)
{
  if (field2)
    {
      double now;

      now = zf_current_time ();
      if (now > f2time)
	{
	  /* Too late. */
	  return TRUE;
	}
      else
	{
	  *timeout = (f2time - now) * 1e3;
	}
    }
  else
    {
      *timeout = -1; /* infinite */
    }

  return FALSE; /* go poll */
}

static gboolean
capture_source_check		(GSource *		source)
{
  capture_source *cs = PARENT (source, capture_source, source);

  return !!(cs->poll_fd.revents & G_IO_IN);
}

static gboolean
capture_source_dispatch		(GSource *		source,
				 GSourceFunc		callback _unused_,
				 gpointer		user_data _unused_)
{
  capture_source *cs = PARENT (source, capture_source, source);
  struct timeval tv;
  struct timespec ts;
  zf_buffer *b2;
  producer_buffer *pb;
  double start;

  if (field2)
    {
      video_blit_frame (&display_buffers[1].frame);
      field2 = FALSE;

      return TRUE;
    }

  gettimeofday (&tv, /* tz */ NULL);

  ts.tv_sec = tv.tv_sec;
  ts.tv_nsec = (tv.tv_usec + 50000) * 1000;

  if (ts.tv_nsec >= 1000000000)
    {
      ts.tv_nsec -= 1000000000;
      ++ts.tv_sec;
    }

  b2 = zf_wait_empty_buffer_timeout (&cs->prod, &ts);
  if (!b2)
    {
      assert (tv_get_caps (cs->info)->flags & TVENG_CAPS_QUEUE);

      /* Clear cs->poll_fd.revents, we cannot dequeue any buffers now. */
      tv_flush_capture_buffers (cs->info);

      return TRUE;
    }

  pb = PARENT (b2, producer_buffer, frame.b);

  if (pb->tag != request_id)
    {
      rebuild_buffer (pb, cs->info);
    }

  if (1 != fill_bundle_tveng (pb, cs->info, /* timeout */ 0))
    {
      zf_unget_empty_buffer (&cs->prod, b2);
      return TRUE;
    }

  start = tv.tv_sec + tv.tv_usec * (1 / 1e6);

  if (0)
    fputc ("-+"[start >= filter_time], stderr); 

  if (start >= filter_time)
    {
      double end;

      if (display_filter)
	{
	  guint n_frames;

	  n_frames = display_filter (display_buffers[0].src_image,
				     display_buffers[1].src_image);
	  if (n_frames > 0)
	    {
	      video_blit_frame (&display_buffers[0].frame);

	      pass_frame_to_plugins (pb);

	      end = zf_current_time ();

	      if (n_frames > 1)
		{
		  const tv_video_standard *s;

		  field2 = TRUE;

		  if ((s = tv_cur_video_standard (cs->info)))
		    f2time = end + 1 / s->frame_rate * field_balance;
		  else
		    f2time = end + 1 / 25.0 * field_balance;
		}
	    }
	  else
	    {
	      pass_frame_to_plugins (pb);

	      end = zf_current_time ();
	    }
	}
      else
	{
	  video_blit_frame (&pb->frame);

	  pass_frame_to_plugins (pb);

	  end = zf_current_time ();
	}

      /* The average time spent in this function must not exceed one frame
	 period or unprocessed frames accumulate and the main context
	 calls us over and over again while the GUI starves to death. */
      filter_time = end + end - start - 1/25.0 * .7;
    }

  zf_send_full_buffer (&cs->prod, b2);

  return TRUE;
}

static GSourceFuncs
capture_source_funcs = {
  capture_source_prepare,
  capture_source_check,
  capture_source_dispatch,
  /* finalize */ NULL,
  /* closure_callback */ NULL,
  /* closure_marshal */ NULL,
};

/* Capture thread / idle handler. */

static pthread_t		capture_thread_id;
static volatile gboolean	exit_capture_thread;
static volatile gboolean	capture_quit_ack;

static void *
capture_thread			(void *			data)
{
  tveng_device_info *info = (tveng_device_info *) data;
  zf_producer prod;

  zf_add_producer (&capture_fifo, &prod);

  while (!exit_capture_thread)
    {
      producer_buffer *p =
	(producer_buffer *) zf_wait_empty_buffer (&prod);

      /*
	check whether this buffer needs rebuilding. We don't do it
	here but defer it to the main thread, since rebuilding buffers
	typically requires X calls, and those don't work across
	multiple threads (actually they do, but relaying on that is
	just asking for trouble, it's too complex to get right).
	Just seeing that the buffer is old doesn't mean that it's
	unusable, if compatible is TRUE then we fill it normally.
      */

      /* No size change now. */
      _pthread_rwlock_rdlock (&fmt_rwlock); 

      {
      retry:
	if (exit_capture_thread)
	  {
	    _pthread_rwlock_unlock (&fmt_rwlock);
	    zf_unget_empty_buffer (&prod, &p->frame.b);
	    break;
	  }

	if (p->tag != request_id && !compatible (p, info))
	  {
	    /* schedule for rebuilding in the main thread */
	    p->frame.b.used = 1; /* note used == 0 indicates eof */
	    zf_send_full_buffer (&prod, &p->frame.b);
	    _pthread_rwlock_unlock (&fmt_rwlock);
	    continue;
	  }

	/* We cannot handle timeouts or errors. Note timeouts
	   are frequent when capturing an empty */
	if (1 != fill_bundle_tveng(p, info, 50))
	  {
	    /* Avoid busy loop. FIXME there must be a better way. */
	    _pthread_rwlock_unlock (&fmt_rwlock);
	    usleep (10000);
	    _pthread_rwlock_rdlock (&fmt_rwlock);
	    goto retry;
	  }
      }

      _pthread_rwlock_unlock (&fmt_rwlock);

      zf_send_full_buffer(&prod, &p->frame.b);
    }

  zf_rem_producer(&prod);

  capture_quit_ack = TRUE;

  return NULL;
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

static zf_consumer		idle_consumer;
static guint			idle_id;

static gint
idle_handler			(gpointer		data)
{
  tveng_device_info *info = (tveng_device_info *) data;
  zf_buffer *b;
  struct timespec t;
  struct timeval now;
  producer_buffer *pb;

  gettimeofday (&now, NULL);

  t.tv_sec = now.tv_sec;
  t.tv_nsec = (now.tv_usec + 50000)* 1000;

  if (t.tv_nsec >= 1e9)
    {
      t.tv_nsec -= 1e9;
      t.tv_sec ++;
    }

  b = zf_wait_full_buffer_timeout (&idle_consumer, &t);
  if (!b)
    return TRUE; /* keep calling me */

  pb = (producer_buffer*)b;

  if (pb->tag == request_id)
    {
      video_blit_frame (&pb->frame);
      pass_frame_to_plugins (pb);
    }
  else
    {
      rebuild_buffer (pb, info);
    }

  zf_send_empty_buffer (&idle_consumer, b);

  return TRUE; /* keep calling me */
}

zimage *
retrieve_frame			(capture_frame *	frame,
				 tv_pixfmt		pixfmt,
				 gboolean		copy)
{
  producer_buffer *pb = PARENT (frame, producer_buffer, frame);
  void *dst;
  const void *src;
  const tv_image_format *dst_format;
  const tv_image_format *src_format;
  guint i;

  if (0)
    fprintf (stderr, "%s: %p %s\n", __FUNCTION__, frame,
	     tv_pixfmt_name (pixfmt));

  i = (guint) pixfmt;

  assert (i < G_N_ELEMENTS (pb->images));

  if (NULL == pb->images[i])
    return NULL;

  assert (pb->images[i]->fmt.pixel_format->pixfmt == pixfmt);

  if (pb->converted & TV_PIXFMT_SET (pixfmt))
    return pb->images[i];

  if (1
      && !copy
      && pb->src_buffer
      && pb->src_buffer->format->pixel_format->pixfmt == pixfmt)
    {
      /* Just return a pointer to src_buffer. */

      pb->src_direct = *pb->images[i];
      pb->src_direct.img = pb->src_buffer->data;

      return &pb->src_direct;
    }

  if (pb->src_buffer)
    {
      /* Convert from driver buffer. */
      src = pb->src_buffer->data;
      src_format = pb->src_buffer->format;
    }
  else
    {
      /* Convert from first copy in pb->images[]. */
      src = pb->src_image->img;
      src_format = &pb->src_image->fmt;
    }

  dst = pb->images[i]->img;
  dst_format = &pb->images[i]->fmt;

  assert (src != dst);

  if (!csconvert (dst, dst_format, src, src_format))
    return NULL;

  pb->converted |= TV_PIXFMT_SET (pixfmt);

  return pb->images[i];
}

static void
on_capture_canvas_allocate             (GtkWidget       *widget _unused_,
                                        GtkAllocation   *allocation,
                                        tveng_device_info *info)
{
  /* Suggest widget size as capture size. */

  request_capture_format (info,
			  allocation->width,
			  allocation->height,
			  TV_PIXFMT_SET_EMPTY,
			  /* flags */ 0);
}

gboolean
remove_display_filter		(display_filter_fn *	filter)
{
  guint i;

  if (display_filter != filter)
    return FALSE;

  display_filter = NULL;

  for (i = 0; i < N_ELEMENTS (display_buffers); ++i)
    {
      producer_buffer *pb = &display_buffers[i];
      guint j;

      for (j = 0; j < G_N_ELEMENTS (pb->images); ++j)
	if (pb->images[j])
	  {
	    zimage_unref (pb->images[j]);
	    pb->images[j] = NULL;
	  }

      pb->src_image = NULL;
    }

  return TRUE;
}

gboolean
add_display_filter		(display_filter_fn *	filter,
				 tv_pixfmt		pixfmt,
				 guint			width,
				 guint			height)
{
  guint i;

  g_assert (pixfmt < TV_MAX_PIXFMTS);

  if (display_filter)
    return FALSE;

  display_filter = filter;

  for (i = 0; i < N_ELEMENTS (display_buffers); ++i)
    {
      producer_buffer *pb = &display_buffers[i];

      CLEAR (*pb);

      pb->images[pixfmt] = zimage_new (pixfmt, width, height);
      g_assert (NULL != pb->images[pixfmt]);

      pb->converted = TV_PIXFMT_SET (pixfmt);

      pb->src_image = pb->images[pixfmt];

      tv_clear_image (pb->src_image->img, &pb->src_image->fmt);
    }

  return TRUE;
}

static gint
join				(const char *		who,
				 pthread_t		id,
				 volatile gboolean *	ack,
				 gint			timeout)
{
  /* Dirty. Where is pthread_try_join()? */
  for (; (!*ack) && timeout > 0; timeout--) {
    usleep (100000);
  }

  /* Ok, you asked for it */
  if (timeout == 0) {
    int r;

    printv("Unfriendly video capture termination\n");
    r = pthread_cancel (id);
    if (r != 0)
      {
	printv("Cancellation of %s failed: %d\n", who, r);
	return 0;
      }
  }

  pthread_join (id, NULL);

  return timeout;
}

gboolean
capture_stop			(void)
{
  GList *p;
  zf_buffer *b;
  guint i;

  /* Don't stop when recording. */
  for (i = 0; i < n_formats; ++i)
    if (formats[i].flags & REQ_SIZE)
      return FALSE;

  tveng_stop_capturing (zapping->info);

  /* XXX */
  g_signal_handlers_disconnect_by_func
    (G_OBJECT (dest_window),
     GTK_SIGNAL_FUNC (on_capture_canvas_allocate),
     zapping->info);

  dest_window = NULL;

  /* First tell all well-behaved consumers to stop. */
  for (p = g_list_first (plugin_list); p; p = p->next)
    plugin_capture_stop ((struct plugin_info *) p->data);

  if (1 && tv_get_caps (zapping->info)->flags & TVENG_CAPS_QUEUE)
    {
      capture_source *cs;

      cs = PARENT (source, capture_source, source);
      zf_rem_producer (&cs->prod);

      g_source_destroy (source);
      g_source_unref (source);
      source = NULL;
    }
  else
    {
      /* Stop our marvellous consumer */
      if (NO_SOURCE_ID != idle_id)
	g_source_remove (idle_id);
      idle_id = NO_SOURCE_ID;

      /* Let the capture thread go to a better place. */
      exit_capture_thread = TRUE;

      /* empty full queue and remove timeout consumer. */
      while ((b = zf_recv_full_buffer (&idle_consumer)))
	zf_send_empty_buffer (&idle_consumer, b);

      zf_rem_consumer (&idle_consumer);

      join ("videocap", capture_thread_id, &capture_quit_ack, 15);
    }

  g_free (formats);
  formats = NULL;
  n_formats = 0;

  zf_destroy_fifo (&capture_fifo);

  video_uninit ();

  return TRUE;
}

gboolean
capture_start			(tveng_device_info *	info,
				 GtkWidget *		window)
{
  guint i;
  GList *p;

  if (-1 == tveng_start_capturing (info))
    {
      ShowBox (_("Cannot start capturing: %s"),
	       GTK_MESSAGE_ERROR, tv_get_errstr (info));
      return FALSE;
    }

  /* XXX */
  dest_window = window;

  zf_init_buffered_fifo (&capture_fifo, "zapping-capture", 0, 0);

  /* Immediately requeue capture buffers when all consumers are done. */
  capture_fifo.buffer_done = buffer_done;

  for (i = 0; i < N_BUNDLES; ++i)
    {
      producer_buffer *pb;

      pb = g_malloc0 (sizeof (*pb));

      pb->frame.b.destroy = free_bundle;

      zf_add_buffer (&capture_fifo, &pb->frame.b);
    }

  if (1 && tv_get_caps (info)->flags & TVENG_CAPS_QUEUE)
    {
      capture_source *cs;

      /* Attn: source_funcs must be static. */
      source = g_source_new (&capture_source_funcs, sizeof (*cs));

      cs = PARENT (source, capture_source, source);

      cs->poll_fd.fd = tv_get_fd (info);
      cs->poll_fd.events = G_IO_IN;
      cs->poll_fd.revents = 0;

      cs->info = info;

      zf_add_producer (&capture_fifo, &cs->prod);

      g_source_add_poll (source, &cs->poll_fd);

      g_source_attach (source, /* context: default */ NULL);

      /* Needs higher priority than VBI or video stumbles. */
      g_source_set_priority (source, -10);

      filter_time = 0.0;
    }
  else
    {
      gint r;

      zf_add_consumer (&capture_fifo, &idle_consumer);

      exit_capture_thread = FALSE;
      capture_quit_ack = FALSE;

      r = pthread_create (&capture_thread_id, NULL,
			  capture_thread, zapping->info);
      g_assert (0 == r);

      idle_id = g_idle_add ((GSourceFunc) idle_handler, info);
    }

  /* XXX */
  g_signal_connect (G_OBJECT (window),
		    "size-allocate",
		    GTK_SIGNAL_FUNC (on_capture_canvas_allocate),
		    zapping->info);

  for (p = g_list_first (plugin_list); p; p = p->next)
    plugin_capture_start ((struct plugin_info *) p->data);

  video_init (window, window->style->black_gc);

  return TRUE;
}

static tv_pixfmt
convertible_to_pixfmt_set	(tv_pixfmt_set		dst_pixfmt_set,
				 tv_pixfmt		src_pixfmt)
{
  tv_pixfmt dst_pixfmt;
  
  if (dst_pixfmt_set & TV_PIXFMT_SET (src_pixfmt))
    return src_pixfmt;

  for (dst_pixfmt = 0; dst_pixfmt < TV_MAX_PIXFMTS; ++dst_pixfmt)
    {
      if (!(dst_pixfmt_set & TV_PIXFMT_SET (dst_pixfmt)))
	continue;

      if (-1 != lookup_csconvert (src_pixfmt, dst_pixfmt))
	return dst_pixfmt;
    }

  return TV_PIXFMT_NONE;
}

static tv_pixfmt
convertible_to_format		(req_format *		format,
				 tv_pixfmt		src_pixfmt)
{
  if (format->flags & REQ_PIXFMT)
    {
      /* We're stuck with format->pixfmt. */

      if (format->pixfmt == src_pixfmt)
	return format->pixfmt;

      if (-1 != lookup_csconvert (src_pixfmt, format->pixfmt))
	return format->pixfmt;
    }
  else
    {
      return convertible_to_pixfmt_set (format->pixfmt_set, src_pixfmt);
    }

  return TV_PIXFMT_NONE;
}

/* This searches for a capture pixfmt which can be converted to
   all requested pixfmts. If more than one qualifies, we pick
   that which needs the fewest conversions.
   FIXME this is too simple. See TODO. */
static gboolean
find_capture_pixfmt		(tveng_device_info *	info,
				 tv_pixfmt *		capture_pixfmt,	 
				 tv_pixfmt *		target_pixfmt,
				 tv_pixfmt_set		pixfmt_set)
{
  tv_pixfmt_set supported_pixfmts;
  tv_pixfmt src_pixfmt;
  guint min_conversions;

  supported_pixfmts = tv_supported_pixfmts (info);
  if (TV_PIXFMT_SET_EMPTY == supported_pixfmts)
    return FALSE;

  *capture_pixfmt = TV_PIXFMT_NONE;
  *target_pixfmt = TV_PIXFMT_NONE;
  min_conversions = 999;

  for (src_pixfmt = 0; src_pixfmt < TV_MAX_PIXFMTS; ++src_pixfmt)
    {
      tv_pixfmt dst_pixfmt;
      guint n_conversions;
      guint i;

      /* What if we would capture using src_pixfmt? */

      if (!(supported_pixfmts & TV_PIXFMT_SET (src_pixfmt)))
	continue;

      n_conversions = 0;

      dst_pixfmt = convertible_to_pixfmt_set (pixfmt_set, src_pixfmt);
      if (TV_PIXFMT_NONE == dst_pixfmt)
	{
	  /* Cannot convert to any of the requested pixfmts. */
	  continue;
	}
      else if (src_pixfmt != dst_pixfmt)
	{
	  n_conversions = 1;
	}

      for (i = 0; i < n_formats; ++i)
	{
	  tv_pixfmt dst_pixfmt_n;

	  dst_pixfmt_n = convertible_to_format (&formats[i], src_pixfmt);
	  if (TV_PIXFMT_NONE == dst_pixfmt_n)
	    {
	      n_conversions = 999;
	      break;
	    }
	  else if (src_pixfmt != dst_pixfmt_n)
	    {
	      ++n_conversions;
	    }
	}

      if (n_conversions < min_conversions)
	{
	  *capture_pixfmt = src_pixfmt;
	  *target_pixfmt = dst_pixfmt;
	  min_conversions = n_conversions;
	}
    }

  return (min_conversions < 999);
}

static const tv_image_format *
change_capture_format		(tveng_device_info *	info,
				 guint			width,
				 guint			height,
				 tv_pixfmt		pixfmt,
				 req_flags		flags)
{
  const tv_image_format *fmt;
  tv_image_format new_format;
  tv_image_format old_format;
  capture_mode old_mode;

  fmt = tv_cur_capture_format (info);
  if (!fmt)
    return NULL;

  if (0 == (width | height))
    flags &= ~REQ_SIZE;

  if (0 == width)
    width = fmt->width;

  if (0 == height)
    height = fmt->height;

  if (TV_PIXFMT_NONE == pixfmt)
    pixfmt = fmt->pixel_format->pixfmt;

  if (fmt->pixel_format->pixfmt == pixfmt
      && fmt->width == width
      && fmt->height == height)
    return fmt;

  if (!tv_image_format_init (&new_format,
			     width, height,
			     /* bytes_per_line: minimum */ 0,
			     pixfmt, TV_COLSPC_UNKNOWN))
    {
      /* Huh? */
      return NULL;
    }

  old_format = *fmt;

  printv ("Setting capture format %s %ux%u\n",
	  new_format.pixel_format->name,
	  new_format.width,
	  new_format.height);

  old_mode = tv_get_capture_mode (info);
  if (CAPTURE_MODE_READ == old_mode)
    tveng_stop_capturing (info);

  flush_buffers (info);

  /* XXX TRY_FMT would be nice. */
  fmt = tv_set_capture_format (info, &new_format);

  /* Size may change due to driver limits. */
  if ((flags & REQ_SIZE)
      && fmt->width != width
      && fmt->height != height)
    fmt = NULL; /* failed */

  if (!fmt)
    {
      fmt = tv_set_capture_format (info, &old_format);
      if (!fmt)
	{
	  /* XXX Cannot restore old format. What now? */
	  return NULL;
	}

      fmt = NULL; /* failed */
    }

  if (CAPTURE_MODE_READ == old_mode)
    tveng_start_capturing (info);

  return fmt;
}

/* Releases the given requests, call this when you won't longer need
   the format. The id returned by request_capture_format must be passed. */
void
release_capture_format		(gint			id)
{
  guint i;

  _pthread_rwlock_wrlock (&fmt_rwlock);

  {
    for (i = 0; i < n_formats; ++i)
      if (formats[i].id == id)
	break;

    if (i >= n_formats)
      {
	_pthread_rwlock_unlock (&fmt_rwlock);
	return;
      }

    --n_formats;

    if (i != n_formats)
      memmove (&formats[i],
	       &formats[i + 1],
	       (n_formats - i) * sizeof (formats[0]));
  }

  /* XXX maybe we can save a conversion now? But I don't really want
     to interrupt capturing without good reason. */

  /* Flag that buffers may need rebuilding. */
  ++request_id;

  _pthread_rwlock_unlock (&fmt_rwlock);
}

gboolean
get_capture_format		(gint			id,
				 guint *		width,
				 guint *		height,
				 tv_pixfmt *		pixfmt)
{
  guint i;

  for (i = 0; i < n_formats; ++i)
    if (formats[i].id == id)
      break;

  if (i >= n_formats)
    return FALSE;

  if (width)
    *width = formats[i].width;

  if (height)
    *height = formats[i].height;

  if (pixfmt)
    *pixfmt = formats[i].pixfmt;

  return TRUE;
}

/* Basically this is a tv_set_capture_format() wrapper. If modules need
   different image sizes and pixel formats it will find a good capture
   format and determine if conversion will be possible.

   width and height - the desired image size, can be zero to get the
   current capture size. pixfmt_set - set of acceptable pixel formats.
   flags - REQ_SIZE if the image size must not change until
   release_capture_format(). REQ_PIXFMT ditto for the pixel format.

   Returns a capture_format ID on success, -1 otherwise. */
gint
request_capture_format		(tveng_device_info *	info,
				 guint			width,
				 guint			height,
				 tv_pixfmt_set		pixfmt_set,
				 req_flags		flags)
{
  static gint format_id = 0;
  tv_pixfmt capture_pixfmt;
  tv_pixfmt target_pixfmt;
  guint i;
  gint id;

  g_assert (NULL != info);

  if (debug_msg)
    {
      tv_pixfmt pixfmt;
      int c = ' ';

      printv ("Format");

      for (pixfmt = 0; pixfmt < TV_MAX_PIXFMTS; ++pixfmt)
	if (pixfmt_set & TV_PIXFMT_SET (pixfmt))
	  {
	    printv ("%c%s", c, tv_pixfmt_name (pixfmt));
	    c = '|';
	  }

      printv (" %ux%u%s requested\n",
	      width, height,
	      (flags & REQ_SIZE) ? " (locked)" : "");
    }

  _pthread_rwlock_wrlock (&fmt_rwlock);

  {
    if (0 == (width | height))
      {
	flags &= ~REQ_SIZE;
      }
    else
      {
	/* Check if the requested size is available. */
	
	for (i = 0; i < n_formats; ++i)
	  {
	    if ((formats[i].flags & REQ_SIZE)
		&& (formats[i].width != width
		    || formats[i].height != height))
	      {
		if (flags & REQ_SIZE)
		  {
		    printv ("Format rejected, size locked at %ux%u\n",
			    formats[i].width, formats[i].height);
		    goto failure;
		  }
		
		/* Keep current size. */
		width = 0;
		height = 0;
		
		break;
	      }
	  }
      }
    
    capture_pixfmt = TV_PIXFMT_NONE;
    target_pixfmt = TV_PIXFMT_NONE;
  
    if (TV_PIXFMT_SET_EMPTY != pixfmt_set)
      {
	/* Check if we already convert to one of the requested pixfmts. */
	for (i = 0; i < n_formats; ++i)
	  if (pixfmt_set & TV_PIXFMT_SET (formats[i].pixfmt))
	    {
	      /* Keep the current capture pixfmt. */
	      target_pixfmt = formats[i].pixfmt;
	      break;
	    }
	
	/* Or find a capture pixfmt that can be
	   converted to all requested pixfmts. */
	if (i >= n_formats)
	  {
	    if (!find_capture_pixfmt (info,
				      &capture_pixfmt,
				      &target_pixfmt,
				      pixfmt_set))
	      {
		printv ("Format rejected, not convertible\n");
		goto failure;
	      }
	  }
      }
    
    if (TV_PIXFMT_NONE != capture_pixfmt
	|| 0 != (width | height))
      {
	const tv_image_format *fmt;
	
	fmt = change_capture_format (info, width, height,
				     capture_pixfmt, flags);
	if (!fmt)
	  {
	    printv ("Format rejected, cannot capture %s %ux%u\n",
		    tv_pixfmt_name (capture_pixfmt), width, height);
	    /* XXX maybe another pixfmt works? */
	    goto failure;
	  }
	
	/* Another target pixfmt may be more efficient now. */
	if (TV_PIXFMT_NONE != capture_pixfmt)
	  for (i = 0; i < n_formats; ++i)
	    if (!(formats[i].flags & REQ_PIXFMT))
	      formats[i].pixfmt =
		convertible_to_pixfmt_set (formats[i].pixfmt_set,
					   capture_pixfmt);
      }
    
    if (TV_PIXFMT_SET_EMPTY != pixfmt_set)
      {
	/* Add new format. */
	
	formats = g_realloc (formats, sizeof (*formats) * (n_formats + 1));
	
	id = ++format_id;
	
	formats[n_formats].id = id;
	formats[n_formats].width = width;
	formats[n_formats].height = height;
	formats[n_formats].pixfmt_set = pixfmt_set;
	formats[n_formats].pixfmt = target_pixfmt;
	formats[n_formats].flags = flags;
	
	++n_formats;
      }

    /* Flag that buffers may need rebuilding. */
    ++request_id;
  }

  _pthread_rwlock_unlock (&fmt_rwlock);

  printv ("Format accepted, converting %s to %s %ux%u\n",
	  tv_pixfmt_name (capture_pixfmt),
	  tv_pixfmt_name (target_pixfmt),
	  width, height);

  return id;

 failure:
  _pthread_rwlock_unlock (&fmt_rwlock);
  return -1;
}

void
shutdown_capture		(void)
{
  pthread_rwlock_destroy (&fmt_rwlock);
}

void
startup_capture			(void)
{
  pthread_rwlock_init (&fmt_rwlock, NULL);

  z_gconf_auto_update_float
    (&field_balance, "/apps/zapping/plugins/deinterlace/field_balance");
}
