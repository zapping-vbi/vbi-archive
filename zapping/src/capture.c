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

#define ZCONF_DOMAIN "/zapping/options/capture/"
#include "zconf.h"

#include <pthread.h>

#include <tveng.h>
#include "zmisc.h"
#include "x11stuff.h"
#include "plugins.h"
#include "capture.h"
#include "common/fifo.h"

/* Some global stuff we need, see descriptions in main.c */
extern GList		*plugin_list;
extern gint		disable_xv; /* TRUE is XVideo should be
				       disabled */
extern gboolean		flag_exit_program;

static gboolean		have_xv = FALSE; /* Can we use the Xv extension? */

static guint		idle_id=0;

static gint		count=0; /* # of printed errors */

static gboolean		capture_locked = FALSE;

static pthread_t	capture_thread_id;
static fifo		capture_fifo; /* capture_thread <-> main
					 thread, uses bundles */
static struct tveng_frame_format current_format;

extern tveng_device_info	*main_info;
extern GtkWidget		*main_window;

static GtkWidget		*capture_canvas = NULL;

static void print_info(GtkWidget *main_window);

static
gboolean request_default_format(gint w, gint h, tveng_device_info *info)
{
  gboolean success = FALSE;

  if (have_xv)
    {
      if ((!zcg_int(NULL, "xvsize")) && /* biggest noninterlaced */
	  (info->num_standards))
	success =
	  request_bundle_format(TVENG_PIX_YUYV,
				info->standards[info->cur_standard].width/2,
				info->standards[info->cur_standard].height/2);
      
      if ((zcg_int(NULL, "xvsize") == 1) || /* 320x240 */
	  ((!zcg_int(NULL, "xvsize")) && (!success)))
	success =
	  request_bundle_format(TVENG_PIX_YUYV, 320, 240);
      
      if (!success)
	success = request_bundle_format(TVENG_PIX_YUYV, w, h);
    }
  
  if (!success)
    success = request_bundle_format(zmisc_resolve_pixformat(x11_get_bpp(),
				    x11_get_byte_order()),
				    w, h);

  return success;
}

void
capture_lock(void)
{
  capture_locked = TRUE;
}

void
capture_unlock(void)
{
  gint w, h;

  if (capture_locked &&
      capture_canvas)
    {
      /* request */
      capture_locked = FALSE;

      gdk_window_get_size(capture_canvas->window, &w, &h);

      if (!request_default_format(w, h, main_info))
	ShowBox(_("Cannot set a default capture format"),
		GNOME_MESSAGE_BOX_WARNING);
    }
  capture_locked = FALSE;
}

/**
 * TRUE if the ioctl's worked and the pixformat is the same, don't
 * make assumptions about the granted size
*/
gboolean
request_bundle_format(enum tveng_frame_pixformat pixformat, gint w, gint h)
{
  enum tveng_capture_mode cur_mode;

  if (capture_locked)
    return FALSE;

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
  g_assert(d != NULL);

  tveng_mutex_lock(info);

  if (info->format.pixformat != d->format.pixformat ||
      info->format.sizeimage > d->image_size ||
      info->current_mode != TVENG_CAPTURE_READ ||
      d->data == NULL ||
      d->image_type < 1)
    goto fill_bundle_failure;

  if (-1 == tveng_read_frame(d->data, d->image_size, 50, info))
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
  zcc_int(0, "Capture size under XVideo", "xvsize");

  return TRUE;
}

void
shutdown_capture(tveng_device_info * info)
{
}

static void
on_capture_canvas_allocate             (GtkWidget       *widget,
                                        GtkAllocation   *allocation,
                                        tveng_device_info *info)
{
  gboolean success = FALSE;

  if (have_xv)
    {
      if ((!zcg_int(NULL, "xvsize")) && /* biggest noninterlaced */
	  (info->num_standards))
	success =
	  request_bundle_format(TVENG_PIX_YUYV,
				info->standards[info->cur_standard].width/2,
				info->standards[info->cur_standard].height/2);

      if ((zcg_int(NULL, "xvsize") == 1) || /* 320x240 */
	  ((!zcg_int(NULL, "xvsize")) && (!success)))
	success =
	  request_bundle_format(TVENG_PIX_YUYV, 320, 240);

      if (!success)
	success = request_bundle_format(TVENG_PIX_YUYV,
					allocation->width,
					allocation->height);
    }

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
    case CAPTURE_BUNDLE_XV:
      xvzImage_destroy(d->image.xvimage);
      break;
    case CAPTURE_BUNDLE_GDK:
      gdk_image_destroy(d->image.gdkimage);
      break;
    case CAPTURE_BUNDLE_DATA:
      g_free(d->image.yuv_data);
      break;
    default:
      break;
    }
  d->image_type = d->image_size = 0;
}

static void
build_bundle(capture_bundle *d, struct tveng_frame_format *format,
	     fifo *f)
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
	  d->image_type = CAPTURE_BUNDLE_XV;
	  d->format.width = d->image.xvimage->w;
	  d->format.height = d->image.xvimage->h;
	  d->image_size = d->image.xvimage->data_size;
	  d->format.bytesperline = d->format.width * 2;
	  d->data = d->image.xvimage->data;
	}
      else
	{
	  d->image_type = CAPTURE_BUNDLE_DATA;
	  d->image.yuv_data = g_malloc(format->width * format->height *2);
	  d->format.width = format->width;
	  d->format.height = format->height;
	  d->image_size = d->format.width * d->format.height * 2;
	  d->format.bytesperline = d->format.width * 2;
	  d->data = d->image.yuv_data;
	}
      d->format.pixformat = TVENG_PIX_YUYV;
      d->format.depth = 16;
      break;
    default: /* Anything else if assumed to be current visual RGB */
      d->image.gdkimage = gdk_image_new(GDK_IMAGE_FASTEST,
					gdk_visual_get_system(),
					format->width,
					format->height);
      if (d->image.gdkimage)
	{
	  d->image_type = CAPTURE_BUNDLE_GDK;
	  d->format.width = d->image.gdkimage->width;
	  d->format.height = d->image.gdkimage->height;
	  d->format.pixformat =
	    zmisc_resolve_pixformat(d->image.gdkimage->bpp<<3,
				    x11_get_byte_order());
	  d->format.depth = d->image.gdkimage->bpp;
	  d->image_size = d->image.gdkimage->bpl *
	    d->image.gdkimage->height;
	  d->format.bytesperline = d->image.gdkimage->bpl;
	  d->data = x11_get_data(d->image.gdkimage);
	}
      break;
    }
  d->format.sizeimage = d->image_size;
  d->format.bpp = (format->depth+7)>>3;
  d->f = f;
}

static void
give_data_to_plugins(capture_bundle *d)
{
  GList *p;

  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_process_bundle(d, (struct plugin_info*)p->data);
      p = p->next;
    }
}

static gint idle_handler(gpointer ignored)
{
  buffer *b;
  capture_bundle *d;
  gint w, h, iw, ih;

  if (flag_exit_program)
    return 0;

  print_info(main_window);

  if ((b = recv_full_buffer(&capture_fifo)))
    {
      d = (capture_bundle*)b->data;
      if (d->timestamp)
	{
	  if (d->image_type)
	    give_data_to_plugins(d);

	  switch (d->image_type)
	    {
	    case CAPTURE_BUNDLE_XV:
	      xvzImage_put(d->image.xvimage, capture_canvas->window,
			   capture_canvas->style->white_gc);
	      break;
	    case CAPTURE_BUNDLE_GDK:
	      gdk_window_get_size(capture_canvas->window, &w, &h);
	      iw = d->image.gdkimage->width;
	      ih = d->image.gdkimage->height;
	      gdk_draw_image(capture_canvas -> window,
			     capture_canvas -> style -> white_gc,
			     d->image.gdkimage,
			     0, 0, (w-iw)/2, (h-ih)/2,
			     iw, ih);
	      break;
	    case CAPTURE_BUNDLE_DATA:
	      //	      g_warning("FIXME: TBD");
	      break;
	    case 0:
	      /* to be rebuilt, just ignore */
	      break;
	    default:
	      g_assert_not_reached();
	      break;
	    }
	}

      /* Rebuild if needed */
      if (d->format.width != current_format.width ||
	  d->format.height != current_format.height ||
	  d->format.pixformat != current_format.pixformat)
	{
	  clear_bundle(d);
	  build_bundle(d, &current_format, &capture_fifo);
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
  gint w, h;

  g_assert(window != NULL);
  g_assert(window->window != NULL);
  g_assert(info != NULL);

  memset(&current_format, 0, sizeof(current_format));

  g_assert(init_buffered_fifo(&capture_fifo, "zapping-capture", 8,
			      sizeof(capture_bundle)));

  gdk_window_set_back_pixmap(window->window, NULL, FALSE);

  gdk_window_get_size(window->window, &w, &h);

  have_xv = exit_capture_thread = capture_locked = FALSE;

  if (!disable_xv &&
      xvz_grab_port(info))
    have_xv = TRUE;

  if (!request_default_format(w, h, info))
    {
      ShowBox("Couldn't start capture: no capture format available",
	      GNOME_MESSAGE_BOX_ERROR);
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
		     GTK_SIGNAL_FUNC(on_capture_canvas_allocate), info);

  capture_canvas = window;

  count = 0;

  /* Capture started correctly */
  return 0;
}

void
capture_stop(tveng_device_info *info)
{
  buffer *b;
  gint i;
  GList *p;

  exit_capture_thread = TRUE;
  while ((b = recv_full_buffer(&capture_fifo)))
    send_empty_buffer(&capture_fifo, b);
  pthread_join(capture_thread_id, NULL);

  gtk_idle_remove(idle_id);

  /* Tell the plugins that capture is stopped */
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_capture_stop((struct plugin_info*)p->data);
      p = p->next;
    }

  /* Free the memeory used by the bundles */
  for (i=0; i<capture_fifo.num_buffers; i++)
    clear_bundle((capture_bundle*)capture_fifo.buffers[i].data);

  xvz_ungrab_port(info);

  uninit_fifo(&capture_fifo);

  if (!flag_exit_program)
    {
      gtk_signal_disconnect_by_func(GTK_OBJECT(capture_canvas),
		   GTK_SIGNAL_FUNC(on_capture_canvas_allocate),
				    main_info);
    }

  capture_canvas = NULL;
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

