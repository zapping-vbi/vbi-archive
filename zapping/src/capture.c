/* Zapping (TV viewer for the Gnome Desktop)
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

/*
 * These routines handle the capture mode and the Xv stuff.
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zconf.h"

#include <pthread.h>

#include <tveng.h>
#include "zmisc.h"
#include "x11stuff.h"
#include "plugins.h"
#include "capture.h"
#include "common/fifo.h"

/* Uncomment for faster capture (only if XVideo backend scaler present) */
/* FIXME: Not production quality yet (properties, not hardcoded size) */
#define NO_INTERLACE 1
#define NO_INTERLACE_W 384
#define NO_INTERLACE_H 288

/* Some global stuff we need, see descriptions in main.c */
extern GList		*plugin_list;
extern gint		disable_xv; /* TRUE is XVideo should be
				       disabled */
extern gboolean		flag_exit_program;

static gboolean		have_xv = FALSE; /* Can we use the Xv extension? */

static guint		idle_id=0;

static gint		count=0; /* # of printed errors */

static pthread_t	capture_thread_id;
static fifo		capture_fifo; /* capture_thread <-> main
					 thread, uses bundles */
typedef struct {
  struct tveng_frame_format	format;

  union {
    xvzImage			*xvimage; /* if xv present */
    GdkImage			*gdkimage; /* otherwise */
    void			*yuv_data; /* raw data */
  } image;

#define CAPTURE_XV 1
#define CAPTURE_GDK 2
#define CAPTURE_DATA 3
  gint image_type;

  gint image_size;

  double timestamp;
} capture_bundle;
static struct tveng_frame_format current_format;

extern tveng_device_info	*main_info;
extern GtkWidget		*main_window;

static void print_info(GtkWidget *main_window);

/**
 * TRUE if the ioctl's worked and the pixformat is the same, don't
 * make assumptions about the granted size
*/
static gboolean
request_bundle_format(enum tveng_frame_pixformat pixformat, gint w, gint h)
{
  enum tveng_capture_mode cur_mode;

  if (w == current_format.width &&
      h == current_format.height &&
      pixformat == current_format.pixformat)
    return TRUE;

  tveng_update_capture_format(main_info);
  
  cur_mode = tveng_stop_everything(main_info);

  main_info->format.width = w;
  main_info->format.height = h;
  main_info->format.pixformat = pixformat;

  if (-1 != tveng_set_capture_format(main_info) &&
      -1 != tveng_restart_everything(cur_mode, main_info) &&
      main_info->format.pixformat == pixformat)
    memcpy(&current_format, &main_info->format,
	   sizeof(struct tveng_frame_format));
  else
    return FALSE;

  return TRUE;
}

static void
fill_bundle(capture_bundle *d, tveng_device_info *info)
{
  gpointer data;

  g_assert(d != NULL);

  tveng_mutex_lock(info);

  if (info->format.pixformat != d->format.pixformat ||
      info->format.sizeimage > d->image_size ||
      info->current_mode != TVENG_CAPTURE_READ)
    goto fill_bundle_failure;

  switch (d->image_type)
    {
    case CAPTURE_XV:
      data = d->image.xvimage->data;
      break;
    case CAPTURE_DATA:
      data = d->image.yuv_data;
      break;
    case CAPTURE_GDK:
      data = x11_get_data(d->image.gdkimage);
      break;
    default:
      goto fill_bundle_failure;
    }

  if (-1 == tveng_read_frame(data, d->image_size, 50, info))
    {
      if (!count++)
	fprintf(stderr, "cap: read(): %s\n", info->error);
      usleep(5000);
      goto fill_bundle_failure;
    }

  d->timestamp = tveng_get_timestamp(info);
  tveng_mutex_unlock(info);
  return;

 fill_bundle_failure:
  d->timestamp = 0; /* Flags error condition */
  tveng_mutex_unlock(info);
}

static volatile gboolean exit_capture_thread = FALSE;

static void *
capture_thread (gpointer data)
{
  buffer *b;
  capture_bundle *d;
  tveng_device_info *info = (tveng_device_info *)data;

  while (!exit_capture_thread) /* cancel is called on this thread */
    {
      if ((b = recv_empty_buffer(&capture_fifo)))
	{
	  d = (capture_bundle*)b->data;
	  
	  fill_bundle(d, info);
	  send_full_buffer(&capture_fifo, b);
	}
      usleep(1000);
      pthread_testcancel();
    }

  /* clear capture_fifo on exit */
  return NULL;
};

gboolean
startup_capture(GtkWidget * widget)
{
  return TRUE;
}

void
shutdown_capture(tveng_device_info * info)
{
}

static void
on_tv_screen_size_allocate             (GtkWidget       *widget,
                                        GtkAllocation   *allocation,
                                        tveng_device_info *info)
{
  gboolean success = FALSE;

  if (have_xv)
#ifndef NO_INTERLACE
    success = request_bundle_format(TVENG_PIX_YUYV, allocation->width,
    				    allocation->height);
#else
    success = request_bundle_format(TVENG_PIX_YUYV, NO_INTERLACE_W,
				    NO_INTERLACE_H);
#endif

  if (!success)
    request_bundle_format(zmisc_resolve_pixformat(x11_get_bpp(),
						  x11_get_byte_order()),
			  allocation->width, allocation->height);
}

/* Clear the bundle's contents, free mem, etc */
static void
clear_bundle(capture_bundle *d)
{
  switch (d->image_type)
    {
    case CAPTURE_XV:
      xvzImage_destroy(d->image.xvimage);
      break;
    case CAPTURE_GDK:
      gdk_image_destroy(d->image.gdkimage);
      break;
    case CAPTURE_DATA:
      g_free(d->image.yuv_data);
      break;
    default:
      break;
    }
  d->image_type = d->image_size = 0;
}

static void
build_bundle(capture_bundle *d, struct tveng_frame_format *format)
{
  g_assert(d != NULL);
  g_assert(format != NULL);

  if (d->image_type)
    clear_bundle(d);

  switch (format->pixformat)
    {
    case TVENG_PIX_YUYV:
      /* Try XV first */
      if (have_xv &&
	  (d->image.xvimage = xvzImage_new(format->width, format->height)))
	{
	  d->image_type = CAPTURE_XV;
	  d->format.width = d->image.xvimage->w;
	  d->format.height = d->image.xvimage->h;
	  d->image_size = d->image.xvimage->data_size;
	}
      else
	{
	  d->image_type = CAPTURE_DATA;
	  d->image.yuv_data = g_malloc(format->width * format->height *2);
	  d->format.width = format->width;
	  d->format.height = format->height;
	  d->image_size = d->format.width * d->format.height * 2;
	}
      d->format.pixformat = TVENG_PIX_YUYV;
      break;
    default: /* Anything else if assumed to be current visual RGB */
      d->image.gdkimage = gdk_image_new(GDK_IMAGE_FASTEST,
					gdk_visual_get_system(),
					format->width,
					format->height);
      if (d->image.gdkimage)
	{
	  d->image_type = CAPTURE_GDK;
	  d->format.width = d->image.gdkimage->width;
	  d->format.height = d->image.gdkimage->height;
	  d->format.pixformat =
	    zmisc_resolve_pixformat(d->image.gdkimage->bpp<<3,
				    x11_get_byte_order());
	  d->image_size = d->image.gdkimage->bpl * d->image.gdkimage->height;
	}
      break;
    }
}

static void
give_data_to_plugins(capture_bundle *d)
{
  plugin_sample sample;
  GList *p;

  memset(&sample, 0, sizeof(plugin_sample));

  switch (d->image_type)
    {
    case CAPTURE_XV:
      sample.video_data = d->image.xvimage->data;
      break;
    case CAPTURE_DATA:
      sample.video_data = d->image.yuv_data;
      break;
    case CAPTURE_GDK:
      sample.video_data = x11_get_data(d->image.gdkimage);
      break;
    default:
      g_assert_not_reached();
    }

  memcpy(&(sample.video_format), &(d->format),
	 sizeof(struct tveng_frame_format));
  sample.video_timestamp = d->timestamp;

  p = g_list_first(plugin_list);
  while (p)
    {
      /* FIXME: Nonfunctional right now, investigate */
      //      plugin_process_sample(&sample, (struct plugin_info*)p->data);
      p = p->next;
    }
}

static gint idle_handler(GtkWidget *tv_screen)
{
  GtkWidget *main_window;
  buffer *b;
  capture_bundle *d;
  gint w, h, iw, ih;

  if (flag_exit_program)
    return 0;

  main_window = lookup_widget(tv_screen, "zapping");

  print_info(main_window);

  if ((b = recv_full_buffer(&capture_fifo)))
    {
      d = (capture_bundle*)b->data;
      if (d->timestamp)
	{
	  switch (d->image_type)
	    {
	    case CAPTURE_XV:
	      xvzImage_put(d->image.xvimage, tv_screen->window,
			   tv_screen->style->white_gc);
	      break;
	    case CAPTURE_GDK:
	      gdk_window_get_size(tv_screen->window, &w, &h);
	      iw = d->image.gdkimage->width;
	      ih = d->image.gdkimage->height;
	      gdk_draw_image(tv_screen -> window,
			     tv_screen -> style -> white_gc,
			     d->image.gdkimage,
			     0, 0, (w-iw)/2, (h-ih)/2,
			     iw, ih);
	      break;
	    case CAPTURE_DATA:
	      g_warning("FIXME: TBD");
	      break;
	    case 0:
	      /* to be rebuilt, just ignore */
	      break;
	    default:
	      g_assert_not_reached();
	      break;
	    }

	  if (d->image_type)
	    give_data_to_plugins(d);
	}

      /* Rebuild if needed */
      if (d->format.width != current_format.width ||
	  d->format.height != current_format.height ||
	  d->format.pixformat != current_format.pixformat)
	{
	  clear_bundle(d);
	  build_bundle(d, &current_format);
	}
      send_empty_buffer(&capture_fifo, b);
    }
  else
    usleep(2000);

  return 1; /* Keep calling me */
}

gint
capture_start(GtkWidget * window, tveng_device_info *info)
{
  enum tveng_frame_pixformat pixformat;
  gint w, h;

  g_assert(window != NULL);
  g_assert(window->window != NULL);
  g_assert(info != NULL);

  memset(&current_format, 0, sizeof(current_format));

  g_assert(init_buffered_fifo(&capture_fifo, "zapping-capture", 8,
			      sizeof(capture_bundle)));

  gdk_window_set_back_pixmap(window->window, NULL, FALSE);

  gdk_window_get_size(window->window, &w, &h);

  have_xv = exit_capture_thread = FALSE;

  if (!disable_xv &&
      xvz_grab_port(info))
    have_xv = TRUE;
  
  if (have_xv)
    {
#ifdef NO_INTERLACE
      w = NO_INTERLACE_W;
      h = NO_INTERLACE_H;
#endif
      pixformat = TVENG_PIX_YUYV;
    }
  else
    pixformat =
      zmisc_resolve_pixformat(x11_get_bpp(), x11_get_byte_order());

  if (!request_bundle_format(pixformat, w, h))
    {
      ShowBox("Couldn't start capture: format request denied",
	      GNOME_MESSAGE_BOX_INFO);
      return -1;
    }

  if (-1 == tveng_start_capturing(info))
    {
      ShowBox("Couldn't start capturing: %s",
	      GNOME_MESSAGE_BOX_ERROR,
	      info->error);
      return -1;
    }

  g_assert(!pthread_create(&capture_thread_id, NULL, capture_thread,
			   info));

  idle_id = gtk_idle_add((GtkFunction)idle_handler, window);
  gtk_signal_connect(GTK_OBJECT(window), "size-allocate",
		     GTK_SIGNAL_FUNC(on_tv_screen_size_allocate), info);

  count = 0;

  /* Capture started correctly */
  return 0;
}

void
capture_stop(tveng_device_info *info)
{
  GtkWidget *tv_screen;
  buffer *b;
  gint i;

  exit_capture_thread = TRUE;
  while ((b = recv_full_buffer(&capture_fifo)))
    send_empty_buffer(&capture_fifo, b);
  pthread_join(capture_thread_id, NULL);

  gtk_idle_remove(idle_id);

  /* FIXME: ALL consumers of the fifo (i.e., plugins) must free it up
     reaching this point, we need some call. */

  for (i=0; i<capture_fifo.num_buffers; i++)
    clear_bundle((capture_bundle*)capture_fifo.buffers[i].data);

  xvz_ungrab_port(info);

  uninit_fifo(&capture_fifo);

  if (!flag_exit_program)
    {
      tv_screen = lookup_widget(main_window, "tv_screen");

      gtk_signal_disconnect_by_func(GTK_OBJECT(tv_screen),
		    GTK_SIGNAL_FUNC(on_tv_screen_size_allocate),
				    main_info);
    }
}

static void
print_visual_info(GdkVisual * visual, const char * name)
{
  fprintf(stderr,
	  "%s (%p):\n"
	  "	type:		%d\n"
	  "	depth:		%d\n"
	  "	byte_order:	%d\n"
	  "	cmap_size:	%d\n"
	  "	bprgb:		%d\n"
	  "	red_mask:	0x%x\n"
	  "	shift:		%d\n"
	  "	prec:		%d\n"
	  "	green_mask:	0x%x\n"
	  "	shift:		%d\n"
	  "	prec:		%d\n"
	  "	blue_mask:	0x%x\n"
	  "	shift:		%d\n"
	  "	prec:		%d\n",
	  name, visual, visual->type, visual->depth,
	  visual->byte_order, visual->colormap_size,
	  visual->bits_per_rgb,
	  visual->red_mask, visual->red_shift, visual->red_prec,
	  visual->green_mask, visual->green_shift, visual->green_prec,
	  visual->blue_mask, visual->blue_shift, visual->blue_prec);
}

static gboolean		print_info_inited = FALSE;

static void
print_info(GtkWidget *main_window)
{
  GdkWindow * tv_screen = lookup_widget(main_window, "tv_screen")->window;
  struct tveng_frame_format * format = &(main_info->format);

  if ((!debug_msg) || (print_info_inited))
    return;

  print_info_inited = TRUE;

  /* info about the used visuals (they should match exactly) */
  print_visual_info(gdk_visual_get_system(), "system visual");
  print_visual_info(gdk_window_get_visual(tv_screen), "tv screen visual");

  fprintf(stderr,
	  "tveng frame format:\n"
	  "	width:		%d\n"
	  "	height:		%d\n"
	  "	depth:		%d\n"
	  "	pixformat:	%d\n"
	  "	bpp:		%g\n"
	  "	sizeimage:	%d\n",
	  format->width, format->height, format->depth,
	  format->pixformat, format->bpp, format->sizeimage );

  fprintf(stderr, "detected x11 depth: %d\n", x11_get_bpp());
}

