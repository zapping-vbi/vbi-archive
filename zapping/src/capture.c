/* Zapping (TV viewer for the Gnome Desktop)
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#define ZCONF_DOMAIN "/zapping/options/capture/"
#include "zconf.h"

#include <pthread.h>

/* for vbi_push_video */
#include <libvbi.h>
#include "zvbi.h"

#include <tveng.h>
#include <common/fifo.h>
#include "zmisc.h"
#include "x11stuff.h"
#include "plugins.h"
#include "capture.h"
#include "yuv2rgb.h"
#include "csconvert.h"

/* FIXME: This is a pretty vital piece of code but it's a bit hard
   to follow. Needs some docs/cleanup. */
/* FIXME: Plain ugliness */

/* Some global stuff we need, see descriptions in main.c */
extern tveng_device_info	*main_info;
extern GtkWidget		*main_window;
extern gboolean			flag_exit_program;
extern GList			*plugin_list;

#define NUM_BUNDLES 6 /* in capture_fifo */
static fifo		_capture_fifo;
fifo			*capture_fifo = &_capture_fifo;
static pthread_t	capture_thread_id; /* the producer */
/* FIXME: there's something better needed */
static volatile gboolean exit_capture_thread; /* controls capture_thread */
static gint		count; /* errors printed */
static GdkImage		*yuv_image=NULL; /* colorspace conversion */
static gboolean		needs_conversion=FALSE; /* BGR24->whatever needed */
static struct tveng_frame_format req_format; /* requested format */
static pthread_mutex_t	req_format_mutex; /* protects the above */
static gboolean		have_xv = FALSE;
/* FIXME: This isn't elegant */
static gboolean		capture_locked;

/* Where does the capture go to */
static GtkWidget	*capture_canvas = NULL;
static guint		idle_id=0; /* idle timeout */


#define BUNDLE_FORMAT (zconf_get_integer(NULL, \
					 "/zapping/options/main/yuv_format"))

/* What we will fill capture_fifo with */
typedef struct {
  /* The buffer the plugins see */
  capture_buffer	d;
  /* our reference, this cannot they touch */
  capture_bundle	vanilla;
  /* TRUE if the buffer needs rebuilding */
  gboolean		dirty;
} producer_buffer;

/* check whether the two pixformats are compatible (not necessarily equal) */
static gboolean
pixformat_ok			(enum tveng_frame_pixformat a,
				 enum tveng_frame_pixformat b)
{
  if (a == b)
    return TRUE;

  /* be tolerant with this, since the API can be somewhat confusing */
  if (a == TVENG_PIX_YVU420 && b == TVENG_PIX_YUV420)
    return TRUE;

  if (a == TVENG_PIX_YUV420 && b == TVENG_PIX_YVU420)
    return TRUE;

  return FALSE; /* nope, this won't work */
}

/* Clear the bundle's contents, free mem, etc */
void
clear_bundle(capture_bundle *d)
{
  if (!d)
    return;

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

  if (d->converted)
    g_free(d->data_src);

  d->image_type = 0;
}

/**
 * TRUE if the ioctl's worked and the pixformat is the same, don't
 * make assumptions about the granted size
*/
gboolean
request_bundle_format(enum tveng_frame_pixformat pixformat, gint w, gint h)
{
  enum tveng_capture_mode cur_mode;

  if (w == req_format.width &&
      h == req_format.height &&
      pixformat_ok(pixformat, req_format.pixformat))
    return TRUE;

  if (capture_locked)
    return FALSE;

  tveng_update_capture_format(main_info);
  
  cur_mode = tveng_stop_everything(main_info);

  if (!cur_mode)
    cur_mode = TVENG_CAPTURE_READ;

  /* hmmm, there are v4l drivers out there that grant an odd width for
   the YVU pixformat !!! */
  switch (pixformat)
    {
    case TVENG_PIX_YVU420:
    case TVENG_PIX_YUV420:
      h &=~1;
    case TVENG_PIX_YUYV:
      w &=~1;
      break;
    default:
      break;
    }

  main_info->format.width = w;
  main_info->format.height = h;
  main_info->format.pixformat = pixformat;

  if (-1 == tveng_set_capture_format(main_info) ||
      -1 == tveng_restart_everything(cur_mode, main_info))
    {
      g_warning("Cannot set new capture format: %s",
		main_info->error);
      return FALSE;
    }

  if (pixformat_ok(pixformat, main_info->format.pixformat))
    {
      pthread_mutex_lock(&req_format_mutex);
      memcpy(&req_format, &main_info->format,
      	     sizeof(struct tveng_frame_format));
      pthread_mutex_unlock(&req_format_mutex);
    }
  else
    {
      g_warning("pixformat type mismatch: %d versus %d",
		main_info->format.pixformat, pixformat);
      return FALSE;
    }

  return TRUE;
}

static void
free_bundle(buffer *b)
{
  clear_bundle(&((producer_buffer*)b)->vanilla);
  g_free(b);
}

gboolean
bundle_equal(capture_bundle *a, capture_bundle *b)
{
  if (!a || !b || !a->image_type || !b->image_type ||
      !a->timestamp || !b->timestamp ||
      !pixformat_ok(a->format.pixformat, b->format.pixformat) ||
      a->format.width != b->format.width ||
      a->format.height != b->format.height)
    return FALSE;

  return TRUE;
}

static void
fill_bundle_tveng(capture_bundle *d, tveng_device_info *info)
{
  int bpl = d->converted ? (d->format.width*3) : d->format.bytesperline;

  tveng_mutex_lock(info);

  if (!d->converted)
    {
      if (!pixformat_ok(info->format.pixformat, d->format.pixformat) ||
	  info->format.width != d->format.width ||
	  info->format.height != d->format.height ||
	  !d->image_type)
	goto fill_bundle_failure;

      if (d->format.pixformat == TVENG_PIX_YVU420 &&
	  info->format.pixformat == TVENG_PIX_YUV420)
	tveng_assume_yvu(TRUE, info);
      else
	tveng_assume_yvu(FALSE, info); /* default settings */
    }

  if (-1 == tveng_read_frame(d->converted ? d->data_src : d->data,
			     bpl, 50, info))
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

void
build_bundle(capture_bundle *d, struct tveng_frame_format *format)
{
  gint size = 0;

  if (d->image_type)
    clear_bundle(d);

  d->converted = FALSE;

  switch (format->pixformat)
    {
    case TVENG_PIX_YUYV:
      d->format.pixformat = format->pixformat;
      d->format.depth = 16;
      d->format.bpp = 2;
      if (have_xv &&
	  (d->image.xvimage =
	   xvzImage_new(TVENG_PIX_YUYV, format->width, format->height)))
	{
	  d->image_type = CAPTURE_BUNDLE_XV;
	  d->format.width = d->image.xvimage->w;
	  d->format.height = d->image.xvimage->h;
	  size = d->image.xvimage->data_size;
	  d->format.bytesperline = size/d->format.height;
	  g_assert((d->format.bytesperline * d->format.height) == size);
	  d->data = d->image.xvimage->data;
	}
      else
	{
	  d->image_type = CAPTURE_BUNDLE_DATA;
	  d->format.width = format->width;
	  d->format.height = format->height;
	  size = d->format.width * d->format.height * d->format.bpp;
	  d->image.yuv_data = g_malloc( size );
	  d->format.bytesperline = d->format.width * d->format.bpp;
	  d->data = d->image.yuv_data;
	}
      break;
      
    case TVENG_PIX_YVU420:
    case TVENG_PIX_YUV420:
      d->format.depth = 12;
      d->format.bpp = 1.5;

      /* Try XV first */
      if (have_xv &&
	  (d->image.xvimage =
	   xvzImage_new(TVENG_PIX_YVU420, format->width, format->height)))
	{
	  d->image_type = CAPTURE_BUNDLE_XV;
	  d->format.width = d->image.xvimage->w;
	  d->format.height = d->image.xvimage->h;
	  size = d->image.xvimage->data_size;
	  d->format.bytesperline = d->format.width;
	  d->data = d->image.xvimage->data;
	  d->format.pixformat = TVENG_PIX_YVU420;
	}
      else
	{
	  d->image_type = CAPTURE_BUNDLE_DATA;
	  d->format.width = format->width;
	  d->format.height = format->height;
	  size = d->format.width * d->format.height * d->format.bpp;
	  d->image.yuv_data = g_malloc( size );
	  d->format.bytesperline = d->format.width;
	  d->data = d->image.yuv_data;
	  d->format.pixformat = format->pixformat;
	}
      break;
    default: /* Anything else is assumed to be current visual RGB */
      if (needs_conversion)
	{
	  d->converted = TRUE;
	  /* FIXME: BGR24 */
	  d->data_src = g_malloc(format->width*3*format->height);
	}

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
	  size = d->image.gdkimage->bpl * d->image.gdkimage->height;
	  d->format.bytesperline = d->image.gdkimage->bpl;
	  d->data = x11_get_data(d->image.gdkimage);
	  d->format.bpp = (format->depth+7)>>3;
	}
      break;
    }
  d->format.sizeimage = size;
}

static void *
capture_thread (void *data)
{
  tveng_device_info *info = (tveng_device_info*)data;
  producer prod;
  GList *plugin;

  add_producer(capture_fifo, &prod);

  while (!exit_capture_thread)
    {
      producer_buffer *p =
	(producer_buffer*)wait_empty_buffer(&prod);
      capture_bundle *d = &(p->d.d);

      /* resent til the rebuilder gets it */
      if (p->dirty)
	{
	  send_full_buffer(&prod, (buffer*)p);
	  continue;
	}

      /* needs rebuilding */
      pthread_mutex_lock(&req_format_mutex);
      if (!p->vanilla.image_type ||
	  p->vanilla.format.width != req_format.width ||
	  p->vanilla.format.height != req_format.height ||
	  !pixformat_ok(p->vanilla.format.pixformat, req_format.pixformat))
	{
	  /* schedule for rebuilding in the main thread */
	  memcpy(&(p->d.d.format), &req_format, sizeof(req_format));
	  p->d.d.image_type = 0;
	  p->d.b.used = 1; /* used==0 is eof */
	  p->dirty = TRUE;
	  p->vanilla.producer = &prod;
	  send_full_buffer(&prod, (buffer*)p);
	  pthread_mutex_unlock(&req_format_mutex);
	  continue;
	}
      pthread_mutex_unlock(&req_format_mutex);

      fill_bundle_tveng(&p->vanilla, info);

      if (p->vanilla.converted)
	{
	  int csconversion =
	    lookup_csconvert(TVENG_PIX_BGR24,
			     p->vanilla.format.pixformat);
	  g_assert(csconversion > -1);
	  csconvert(csconversion, p->vanilla.data_src,
		    p->vanilla.data, p->vanilla.format.width*3,
		    p->vanilla.format.bytesperline,
		    p->vanilla.format.width, p->vanilla.format.height);
	}

      p->d.b.time = p->vanilla.timestamp;

      /* Now pass the buffer to the modifying plugins */
      memcpy(d, &p->vanilla, sizeof(*d));

#if 0
      {
	struct vbi *vbi;
	
	if ((vbi = zvbi_get_object()))
	  {
	    /* XXX redundant if we're not in PAL/SECAM mode
	       or VBI captures line 23 already */
	    vbi_push_video(vbi, d->data, d->format.width,
			   d->format.pixformat, d->timestamp);
	  }
      }
#endif

      plugin = g_list_first(plugin_list);
      while (plugin)
	{
	  plugin_write_bundle(d, (struct plugin_info*)plugin->data);
	  plugin = plugin->next;
	}

      send_full_buffer(&prod, (buffer*)p);
    }

  rem_producer(&prod);

  return NULL;
}

static
gboolean request_default_format(gint w, gint h, tveng_device_info *info)
{
  gboolean success = FALSE;
  enum tveng_frame_pixformat bundle_format2;

  if (needs_conversion)
    {
      success = request_bundle_format(info->format.pixformat,
				      info->format.width,
				      info->format.height);
      req_format.pixformat = zmisc_resolve_pixformat(x11_get_bpp(),
			     x11_get_byte_order());
    }

#ifdef FORCE_DATA
  success = request_bundle_format(BUNDLE_FORMAT, w, h);
#endif

  if (have_xv && !success)
    {
      if (BUNDLE_FORMAT == TVENG_PIX_YVU420)
	bundle_format2 = TVENG_PIX_YUYV;
      else
	bundle_format2 = TVENG_PIX_YVU420;

      if ((!zcg_int(NULL, "xvsize")) && /* biggest noninterlaced */
	  (info->num_standards))
	{
	  success =
	    request_bundle_format(BUNDLE_FORMAT,
			  info->standards[info->cur_standard].width/2,
			  info->standards[info->cur_standard].height/2);

	  if (!success) /* try with the other YUV pixformat */
	    {
	      success =
		request_bundle_format(bundle_format2,
			      info->standards[info->cur_standard].width/2,
			      info->standards[info->cur_standard].height/2);
	      if (success)
		zconf_set_integer(bundle_format2,
				  "/zapping/options/main/yuv_format");
	    }
	}
      
      if ((zcg_int(NULL, "xvsize") == 1) || /* 320x240 */
	  ((!zcg_int(NULL, "xvsize")) && (!success)))
	{
	  success =
	    request_bundle_format(BUNDLE_FORMAT, 320, 240);

	  if (!success)
	    {
	      success =
		request_bundle_format(bundle_format2,
				      320, 240);
	      if (success)
		zconf_set_integer(bundle_format2,
				  "/zapping/options/main/yuv_format");
	    }
	}
      
      if (!success)
	{
	  success = request_bundle_format(BUNDLE_FORMAT, w, h);

	  if (!success)
	    {
	      success =
		request_bundle_format(bundle_format2, w, h);

	      if (success)
		zconf_set_integer(bundle_format2,
				  "/zapping/options/main/yuv_format");
	    }
	}
    }
  
  if (!success)
    success = request_bundle_format(zmisc_resolve_pixformat(x11_get_bpp(),
				    x11_get_byte_order()), w, h);

  /* Try conversions then (FIXME: More conversions) */
  if (!success)
    {
      int csconv = lookup_csconvert(TVENG_PIX_BGR24,
	    zmisc_resolve_pixformat(x11_get_bpp(), x11_get_byte_order()));

      if (csconv != -1)
	{
	  /* FIXME: The code assumes that bpl == width*3 */
	  success = request_bundle_format(TVENG_PIX_BGR24,
					  (info->caps.maxwidth+15)&~16,
					  info->caps.maxheight);
	  if (success)
	    {
	      GdkVisual * v = gdk_visual_get_system();
	      build_csconvert_tables(v->red_mask, v->red_shift, v->red_prec,
				     v->green_mask, v->green_shift,
				     v->green_prec, v->blue_mask,
				     v->blue_shift, v->blue_prec);
	      req_format.pixformat =
		zmisc_resolve_pixformat(x11_get_bpp(), x11_get_byte_order());
	      
	      needs_conversion = TRUE;
	    }
	}
    }

  return success;
}

void
capture_lock(void)
{
  capture_locked++;
}

void
capture_unlock(void)
{
  gint w, h;

  if (capture_locked)
    {
      capture_locked--;
      if (!capture_locked &&
	  capture_canvas)
	{
	  /* request */
	  gdk_window_get_size(capture_canvas->window, &w, &h);
	  
	  request_default_format(w, h, main_info);
	}
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
	  "	byterperline:	%d\n",
	  format->width, format->height, format->depth,
	  format->pixformat, format->bpp, format->bytesperline );

  fprintf(stderr, "detected x11 depth: %d\n", x11_get_bpp());
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

static consumer			cf_idle_consumer;
static gint idle_handler(gpointer ignored)
{
  buffer *b;
  capture_bundle *d;
  producer_buffer *p;
  capture_buffer *cb;
  gint w, h, iw, ih;
  GList *plugin;

  if (flag_exit_program)
    return 0;

  print_info(main_window);

  b = wait_full_buffer(&cf_idle_consumer);

  cb = (capture_buffer*)b;
  d = &(cb->d);
  p = (producer_buffer*)b;

  g_assert(b->used > 0);

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
      if (d->format.pixformat != TVENG_PIX_YUV420 &&
	  d->format.pixformat != TVENG_PIX_YVU420 &&
	  d->format.pixformat != TVENG_PIX_YUYV)
	break;
	
      if (!yuv_image ||
	  yuv_image->width != d->format.width ||
	  yuv_image->height != d->format.height)
	{
	  /* reallocate translation buffer */
	  if (yuv_image)
	    gdk_image_destroy(yuv_image);
	  yuv_image = NULL;
	  yuv_image = gdk_image_new(GDK_IMAGE_FASTEST,
				    gdk_visual_get_system(),
				    d->format.width,
				    d->format.height);
	}
	
      /* fixme: need a flag to turn drawing off, it's slow */
      if (d->format.pixformat == TVENG_PIX_YUYV)
	{
	  if (yuv_image && yuyv2rgb)
	    {
	      yuyv2rgb(x11_get_data(yuv_image),
		       (uint8_t*)d->image.yuv_data,
		       d->format.width, d->format.height,
		       yuv_image->bpl, d->format.width*2);
	      gdk_window_get_size(capture_canvas->window, &w, &h);
	      iw = yuv_image->width;
	      ih = yuv_image->height;
	      gdk_draw_image(capture_canvas -> window,
			     capture_canvas -> style -> white_gc,
			     yuv_image,
			     0, 0, (w-iw)/2, (h-ih)/2,
			     iw, ih);
	    }
	}
      else
	if (yuv_image)
	  {
	    uint8_t *y, *u, *v, *t;
	    y = (uint8_t*)d->image.yuv_data;
	    v = y + d->format.width * d->format.height;
	    u = v + ((d->format.width * d->format.height)>>2);
	    g_assert(d->format.pixformat == TVENG_PIX_YUV420 ||
		     d->format.pixformat == TVENG_PIX_YVU420);
	    if (d->format.pixformat == TVENG_PIX_YUV420)
	      { t = u; u = v; v = t; }
	    if (yuv2rgb)
	      {
		yuv2rgb(x11_get_data(yuv_image), y, u, v,
			d->format.width, d->format.height,
			yuv_image->bpl, d->format.width,
			d->format.width*0.5);
		gdk_window_get_size(capture_canvas->window, &w, &h);
		iw = yuv_image->width;
		ih = yuv_image->height;
		gdk_draw_image(capture_canvas -> window,
			       capture_canvas -> style -> white_gc,
			       yuv_image,
			       0, 0, (w-iw)/2, (h-ih)/2,
			       iw, ih);
	      }
	  }
      break;
    case 0:
      /* scheduled for rebuilding */
      clear_bundle(&p->vanilla);
      build_bundle(&p->vanilla, &p->d.d.format);
      p->d.b.data = p->vanilla.data;
      p->d.b.used = p->vanilla.format.sizeimage;
      p->dirty = FALSE;
      break;
    default:
      g_assert_not_reached();
      break;
    }

  if (d->image_type)
    {
      capture_bundle carbon_copy;
      /* plugins have read-only access to the struct now */
      memcpy(&carbon_copy, d, sizeof(carbon_copy));
      plugin = g_list_first(plugin_list);
      while (plugin)
	{
	  plugin_read_bundle(d, (struct plugin_info*)plugin->data);
	  memcpy(d, &carbon_copy, sizeof(carbon_copy));
	  plugin = plugin->next;
	}
    }

  send_empty_buffer(&cf_idle_consumer, b);

  return TRUE;
}

static void
on_capture_canvas_allocate             (GtkWidget       *widget,
                                        GtkAllocation   *allocation,
                                        tveng_device_info *info)
{
  request_default_format(allocation->width, allocation->height, info);
}

gint
capture_start(GtkWidget * window, tveng_device_info *info)
{
  gint w, h;

  g_assert(window != NULL);
  g_assert(window->window != NULL);
  g_assert(info != NULL);

  memset(&req_format, 0, sizeof(req_format));

  g_assert(add_consumer(capture_fifo, &cf_idle_consumer));

  gdk_window_set_back_pixmap(window->window, NULL, FALSE);

  gdk_window_get_size(window->window, &w, &h);

  have_xv = exit_capture_thread = FALSE;
  capture_locked = 0;

  if (xvz_grab_port(info))
    have_xv = TRUE;

  if (!request_default_format(w, h, info))
    {
      ShowBox("Couldn't start capture: no capture format available",
	      GNOME_MESSAGE_BOX_ERROR);
      if (have_xv)
	xvz_ungrab_port(info);
      return -1;
    }

  if (-1 == tveng_start_capturing(info))
    {
      ShowBox("Couldn't start capturing: %s",
	      GNOME_MESSAGE_BOX_ERROR,
	      info->error);
      if (have_xv)
	xvz_ungrab_port(info);
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
  GList *p;

  gtk_idle_remove(idle_id);

  /* Tell the plugins that capture is stopped */
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_capture_stop((struct plugin_info*)p->data);
      p = p->next;
    }

  exit_capture_thread = TRUE;

  while ((b = recv_full_buffer(&cf_idle_consumer)))
    send_empty_buffer(&cf_idle_consumer, b);

  pthread_join(capture_thread_id, NULL);

  rem_consumer(&cf_idle_consumer);

  xvz_ungrab_port(info);

  if (!flag_exit_program)
    gtk_signal_disconnect_by_func(GTK_OBJECT(capture_canvas),
				  GTK_SIGNAL_FUNC(on_capture_canvas_allocate),
				  main_info);
  capture_canvas = NULL;
}

gboolean
startup_capture(GtkWidget * widget)
{
  gint i;
  buffer *b;

  zcc_int(0, "Capture size under XVideo", "xvsize");

  init_buffered_fifo(capture_fifo, "zapping-capture", 0, 0);

  /* init the bundle-buffers */
  for (i=0; i<NUM_BUNDLES;i++)
    {
      g_assert((b = g_malloc0(sizeof(producer_buffer))));
      b->destroy = free_bundle;
      add_buffer(capture_fifo, b);
    }

  pthread_mutex_init(&req_format_mutex, NULL);
  memset(&req_format, 0, sizeof(req_format));

  return TRUE;
}

void
shutdown_capture(void)
{
  if (yuv_image)
    gdk_image_destroy(yuv_image);

  pthread_mutex_destroy(&req_format_mutex);

  destroy_fifo(capture_fifo);
}
