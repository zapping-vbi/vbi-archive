/*
 * Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
 * Copyright (C) 2003 Michael H. Schimek
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

#include "site_def.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <tveng.h>

#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zconf.h"
#include "x11stuff.h"
#include "zmisc.h"
//#include "zvbi.h"
#include "overlay.h"
#include "osd.h"
#include "globals.h"
#include "zvideo.h"

/* This code clips DMA overlay into video memory against X window
   boundaries. Which is really the job of the X server, but without
   XVideo extension we have little choice. If XVideo is available
   it merely adjusts the overlay to fill the video window. Actually
   all this should be integrated into zvideo.c. */

/* TODO:
   + Special mode for devices without clipping. Hint: think twice.
   + Matte option (clip out WSS, GCR).
   + Source rectangle, if supported by hardware (e.g. zoom function).
 */

#ifndef OVERLAY_LOG_FP
#define OVERLAY_LOG_FP 0
#endif

#ifndef OVERLAY_EVENT_LOG_FP
#define OVERLAY_EVENT_LOG_FP 0
#endif

#ifndef OVERLAY_DUMP_CLIPS
#define OVERLAY_DUMP_CLIPS 0
#endif

#ifndef OVERLAY_CHROMA_TEST
#define OVERLAY_CHROMA_TEST 0
#endif

#define CLEAR_TIMEOUT 50 /* ms for the clear window timeout */

static struct {
  GdkWindow *		root_window;

  GtkWidget *		main_window;	/* top level parent of video_window */
  GtkWidget *		video_window;	/* displays the overlay */

  const tv_screen *	screen;		/* screen containing video_window
					   (Xinerama) */

  gint			mw_x;		/* last known main_window position */
  gint			mw_y;		/* (root relative) */

  gint			vw_x;		/* last known video_window position */
  gint			vw_y;		/* (main_window relative) */

  guint			vw_width;	/* last known video_window size */
  guint			vw_height;

  gboolean		needs_cleaning; /* FALSE for XVideo and chromakey */

  tveng_device_info *	info;		/* V4L device */

  /* DMA overlay */

  GdkVisibilityState	visibility;	/* of video_window */

  tv_window		window;		/* overlay rectangle */

  tv_clip_vector	cur_vector;
  tv_clip_vector	tmp_vector;
  tv_clip_vector	set_vector;

  gboolean		clean_screen;
  gboolean		geometry_changed;

  guint			timeout_id;
} tv_info;

static __inline__ tv_bool
tv_window_equal			(const tv_window *	window1,
				 const tv_window *	window2)
{
  return (0 == ((window1->x ^ window2->x) |
		(window1->y ^ window2->y) |
		(window1->width ^ window2->width) |
		(window1->height ^ window2->height)));
}

static void
get_clips			(tv_clip_vector *	vector)
{
  if (!x11_window_clip_vector
      (vector,
       GDK_WINDOW_XDISPLAY (tv_info.video_window->window),
       GDK_WINDOW_XID (tv_info.video_window->window),
       tv_info.window.x,
       tv_info.window.y,
       tv_info.window.width,
       tv_info.window.height))
    g_assert_not_reached ();

  if (OVERLAY_DUMP_CLIPS)
    {
      tv_clip *clip;
      unsigned int i;

      clip = vector->vector;

      fprintf (stderr, "get_clips %u:\n", vector->size);

      for (i = 0; i < vector->size; ++i, ++clip)
	fprintf (stderr, "%3u: %3u, %3u - %3u, %3u\n",
		 i, clip->x1, clip->y1, clip->x2, clip->y2);
    }
}

static void
expose_window_clip_vector	(const tv_window *	window,
				 const tv_clip_vector *	clip_vector)
{
  tv_clip *clip;
  guint count;

  clip = clip_vector->vector;
  count = clip_vector->size;

  while (count-- > 0)
    {
      x11_force_expose (window->x + clip->x1,
			window->y + clip->y1,
			(guint)(clip->x2 - clip->x1),
			(guint)(clip->y2 - clip->y1));
      ++clip;
    }
}

static void
expose_screen			(void)
{
  tv_info.clean_screen = FALSE;

  x11_force_expose ((gint) tv_info.screen->x,
		    (gint) tv_info.screen->y,
		    tv_info.screen->width,
		    tv_info.screen->height);
}

static gboolean
set_window			(void)
{
  if (tv_set_overlay_window_clipvec (tv_info.info,
				     &tv_info.window,
				     &tv_info.cur_vector))
    {
      tv_clip_vector_set (&tv_info.set_vector, &tv_info.cur_vector);
      return TRUE;
    }

  return FALSE;
}

static gboolean
obscured_timeout		(gpointer		user_data _unused_)
{
  const tv_window *window;

  if (OVERLAY_LOG_FP)
    fprintf (OVERLAY_LOG_FP, "obscured_timeout\n");

  /* Changed from partially visible or unobscured to fully obscured. */

  window = tv_cur_overlay_window (tv_info.info);

  if (tv_info.clean_screen)
    expose_screen ();
  else
    x11_force_expose (window->x,
		      window->y,
		      window->width,
		      window->height);

  tv_clip_vector_clear (&tv_info.cur_vector);
  /* XXX error ignored */
  tv_clip_vector_add_clip_xy (&tv_info.cur_vector,
			      0, 0, window->width, window->height);

  tv_info.geometry_changed = TRUE;

  tv_info.timeout_id = NO_SOURCE_ID;
  return FALSE; /* remove timeout */
}

static gboolean
visible_timeout			(gpointer		user_data _unused_)
{
  if (OVERLAY_LOG_FP)
    fprintf (OVERLAY_LOG_FP, "visible_timeout\n");

  /* Changed from
     a) fully obscured or partially visible to unobscured
        (cleaning the clip vector is not enough, we must
	 still clip against the video_window bounds)
     b) fully obscured or unobscured to partially visible
     c) possible clip vector change while partially obscured
  */

  /* XXX error */
  get_clips (&tv_info.tmp_vector);

  if (!tv_clip_vector_equal (&tv_info.cur_vector,
			     &tv_info.tmp_vector))
    {
      /* Delay until the situation stabilizes. */

      if (OVERLAY_LOG_FP)
	fprintf (OVERLAY_LOG_FP, "visible_timeout: delay\n");

      SWAP (tv_info.cur_vector, tv_info.tmp_vector);

      tv_info.geometry_changed = TRUE;

      return TRUE; /* call again */
    }

  /* Resume overlay. */

  if (tv_info.geometry_changed)
    {
      const tv_screen *xs;
      guint retry_count;

      tv_info.geometry_changed = FALSE;

      if (OVERLAY_LOG_FP)
	fprintf (OVERLAY_LOG_FP, "visible_timeout: geometry change\n");

      xs = tv_screen_list_find (screens,
				    tv_info.mw_x + tv_info.vw_x,
				    tv_info.mw_y + tv_info.vw_y,
				    (guint) tv_info.vw_width,
				    (guint) tv_info.vw_height);
      if (!xs)
	xs = screens;

      if (tv_info.screen != xs)
	{
	  /* Moved to other screen (Xinerama). */

	  tv_info.screen = xs;
	  if (!z_set_overlay_buffer (tv_info.info,
				     tv_info.screen,
				     tv_info.video_window->window))
	    goto finish; /* XXX */
	}

      /* Other windows may have received expose events before we
	 were able to turn off the overlay. Resend expose
	 events for those regions which should be clean now. */
      if (tv_info.clean_screen)
	expose_screen ();
      else
	expose_window_clip_vector (tv_cur_overlay_window (tv_info.info),
				   &tv_info.set_vector);

      /* Desired overlay bounds */

      tv_info.window.x		= tv_info.mw_x + tv_info.vw_x;
      tv_info.window.y		= tv_info.mw_y + tv_info.vw_y;
      tv_info.window.width	= tv_info.vw_width;
      tv_info.window.height	= tv_info.vw_height;

      retry_count = 5;

      for (;;)
	{
	  const tv_window *w;

	  if (retry_count-- == 0)
	    goto finish; /* XXX */

	  /* XXX error */
	  set_window ();

	  w = tv_cur_overlay_window (tv_info.info);

	  if (tv_window_equal (&tv_info.window, w))
	    break;

	  /* The driver modified the overlay bounds (alignment, limits),
	     we must update the clips. */

	  tv_info.window.x      = w->x;
	  tv_info.window.y      = w->y;
	  tv_info.window.width  = w->width;
	  tv_info.window.height = w->height;

	  /* XXX error */
	  get_clips (&tv_info.tmp_vector);
	}
    }
  else if (tv_info.clean_screen)
    {
      expose_screen ();
    }

  if (!OVERLAY_CHROMA_TEST)
    {
      /* XXX error ignored */
      tv_enable_overlay (tv_info.info, TRUE);
    }

 finish:
  tv_info.timeout_id = NO_SOURCE_ID;
  return FALSE; /* remove timeout */
}

static void
stop_timeout			(void)
{
  if (tv_info.timeout_id > 0)
    g_source_remove (tv_info.timeout_id);

  tv_info.timeout_id = NO_SOURCE_ID;
}

static void
restart_timeout			(void)
{
  if (OVERLAY_LOG_FP)
    fprintf (OVERLAY_LOG_FP, "restart_timeout\n");

  tv_enable_overlay (tv_info.info, FALSE);

  stop_timeout ();

  if (tv_info.visibility == GDK_VISIBILITY_FULLY_OBSCURED)
    tv_info.timeout_id =
      g_timeout_add (CLEAR_TIMEOUT,
		     (GSourceFunc) obscured_timeout, &tv_info);
  else
    tv_info.timeout_id =
      g_timeout_add (CLEAR_TIMEOUT,
		     (GSourceFunc) visible_timeout, &tv_info);
}

static __inline__ void
reconfigure			(void)
{
  tv_info.geometry_changed = TRUE;
  tv_info.clean_screen = TRUE; /* see root_filter() below */

  if (tv_info.visibility != GDK_VISIBILITY_FULLY_OBSCURED)
    restart_timeout ();
}

static gboolean
on_video_window_event		(GtkWidget *		widget _unused_,
				 GdkEvent *		event,
				 gpointer		user_data _unused_)
{
  if (OVERLAY_EVENT_LOG_FP)
    fprintf (OVERLAY_EVENT_LOG_FP, "on_video_window_event: GDK_%s\n",
	     z_gdk_event_name (event));

  switch (event->type)
    {
    case GDK_CONFIGURE:
      /* Size, position, stacking order changed. Note position
         is relative to the parent window. */

      if (tv_info.needs_cleaning)
	{
	  if (tv_info.vw_width != (guint) event->configure.width
	      || tv_info.vw_height != (guint) event->configure.height
	      || tv_info.vw_x != (gint) event->configure.x
	      || tv_info.vw_y != (gint) event->configure.y)
	    {
	      tv_info.vw_x = event->configure.x; /* XXX really mw relative? */
	      tv_info.vw_y = event->configure.y;
	      
	      tv_info.vw_width = event->configure.width;
	      tv_info.vw_height = event->configure.height;

	      reconfigure ();
	    }
	}
      else
	{
	  /* XVideo overlay is automatically positioned relative to
	     the video_window origin, only have to adjust size. */

	  tv_enable_overlay (tv_info.info, FALSE);

	  /* XXX tveng_set_preview_window (currently default is
	     fill window size) */

	  if (!OVERLAY_CHROMA_TEST)
	    {
	      /* XXX error
	         XXX off/on is inefficient, XVideo does this automatically. */
	      tv_enable_overlay (tv_info.info, TRUE);
	    }
	}

      break;

    case GDK_VISIBILITY_NOTIFY:
      /* Visibility state changed: obscured, partially or fully visible. */

      if (!tv_info.needs_cleaning)
	break;

      tv_info.visibility = event->visibility.state;
      restart_timeout ();
      break;

    case GDK_EXPOSE:
      /* Parts of the video window have been exposed, e.g. menu window has
	 been closed. Remove those clips. */

      if (OVERLAY_EVENT_LOG_FP)
	fprintf (OVERLAY_EVENT_LOG_FP, "Expose rect %d,%d - %d,%d\n",
		 event->expose.area.x,
		 event->expose.area.y,
		 event->expose.area.x + event->expose.area.width - 1,
		 event->expose.area.y + event->expose.area.height - 1);

      if (tv_info.needs_cleaning)
	{
	  tv_info.geometry_changed = TRUE;
	  restart_timeout ();
	}

      break;

    default:
      break;
    }

  return FALSE; /* pass on */
}

static gboolean
on_main_window_event		(GtkWidget *		widget _unused_,
				 GdkEvent *		event,
				 gpointer		user_data _unused_)
{
  if (OVERLAY_EVENT_LOG_FP)
    fprintf (OVERLAY_EVENT_LOG_FP, "on_main_window_event: GDK_%s\n",
	     z_gdk_event_name (event));

  switch (event->type)
    {
    case GDK_CONFIGURE:
      /* Size, position, stacking order changed. Note position
         is relative to the parent window. */

      if (tv_info.mw_x != event->configure.x
	  || tv_info.mw_y != event->configure.y)
	{
	  tv_info.mw_x = event->configure.x; /* XXX really root relative? */
	  tv_info.mw_y = event->configure.y;

	  reconfigure ();
	}

      break;

    case GDK_UNMAP:
      /* Window was rolled up or minimized. No visibility events are
	 sent in this case. */

      tv_info.visibility = GDK_VISIBILITY_FULLY_OBSCURED;
      restart_timeout ();
      break;

    default:
      break;
    }

  return FALSE; /* pass on */
}

static GdkFilterReturn
root_filter			(GdkXEvent *		gdkxevent,
				 GdkEvent *		unused _unused_,
				 gpointer		data _unused_)
{
  XEvent *event = (XEvent *) gdkxevent;

  if (OVERLAY_EVENT_LOG_FP)
    {
      fprintf (OVERLAY_EVENT_LOG_FP, "root_filter: ");

      switch (event->type)
	{
	case MotionNotify:	fprintf (OVERLAY_EVENT_LOG_FP, "MotionNotify\n"); break;
	case Expose:		fprintf (OVERLAY_EVENT_LOG_FP, "Expose\n"); break;
	case VisibilityNotify:	fprintf (OVERLAY_EVENT_LOG_FP, "VisibilityNotify\n"); break;
	case CreateNotify:	fprintf (OVERLAY_EVENT_LOG_FP, "CreateNotify\n"); break;
	case DestroyNotify:	fprintf (OVERLAY_EVENT_LOG_FP, "DestroyNotify\n"); break;
	case UnmapNotify:	fprintf (OVERLAY_EVENT_LOG_FP, "UnmapNotify\n"); break;
	case MapNotify:		fprintf (OVERLAY_EVENT_LOG_FP, "MapNotify\n"); break;
	case ConfigureNotify:	fprintf (OVERLAY_EVENT_LOG_FP, "ConfigureNotify\n"); break;
	case GravityNotify:	fprintf (OVERLAY_EVENT_LOG_FP, "GravityNotify\n"); break;
	default:		fprintf (OVERLAY_EVENT_LOG_FP, "Unknown\n"); break;
	}
    }

  if (tv_info.visibility != GDK_VISIBILITY_PARTIAL)
    return GDK_FILTER_CONTINUE;

  switch (event->type)
    {
    case ConfigureNotify:
      {
	/* We could just refresh regions we previously DMAed, but unfortunately
	   it's possible to move a destroyed window away before we did. What's
	   worse, imperfect refresh or unconditionally refreshing the entire
	   screen? */

	if (1)
	  {
	    tv_info.clean_screen = TRUE;
	  }
	else
	  {
	    XConfigureEvent *ev = &event->xconfigure;
	    const tv_window *win = tv_cur_overlay_window (tv_info.info);

	    if ((int)(ev->x - ev->border_width) >= (int)(win->x + win->width)
		|| (int)(ev->x + ev->width + ev->border_width) <= (int) win->x
		|| (int)(ev->y - ev->border_width) >= (int)(win->y + win->height)
		|| (int)(ev->y + ev->height + ev->border_width) <= (int) win->y)
	      /* Windows do not overlap. */
	      return GDK_FILTER_CONTINUE;
	  }

	restart_timeout ();

	break;
      }

    default:
      break;
    }

  return GDK_FILTER_CONTINUE;
}

/* The osd pieces have changed, avoid flicker if not needed. */
static void
on_osd_model_changed		(ZModel *		osd_model _unused_,
				 gpointer		ignored _unused_)
{
  if (tv_info.visibility == GDK_VISIBILITY_FULLY_OBSCURED)
    return;

  tv_info.visibility = GDK_VISIBILITY_PARTIAL;
  restart_timeout ();
}

static void
terminate			(void)
{
  stop_timeout ();

  tv_enable_overlay (tv_info.info, FALSE);

  if (tv_info.needs_cleaning)
    {
      usleep (CLEAR_TIMEOUT * 1000);

      if (tv_info.clean_screen)
	expose_screen ();
      else
	expose_window_clip_vector (tv_cur_overlay_window (tv_info.info),
				   &tv_info.set_vector);
    }
}

static gboolean
on_main_window_delete_event	(GtkWidget *		widget _unused_,
				 GdkEvent *		event _unused_,
				 gpointer		user_data _unused_)
{
  terminate ();

  return FALSE; /* pass on */
}

void
stop_overlay			(void)
{
  g_assert (tv_info.main_window != NULL);

  /* XXX see below */
  z_video_set_max_size (Z_VIDEO (tv_info.video_window), 16384, 16384);

  g_signal_handlers_disconnect_matched
    (G_OBJECT (tv_info.main_window),
     G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
     0, 0, NULL, G_CALLBACK (on_main_window_delete_event), NULL);

  g_signal_handlers_disconnect_matched
    (G_OBJECT (tv_info.video_window),
     G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
     0, 0, NULL, G_CALLBACK (on_video_window_event), NULL);

  if (tv_info.needs_cleaning)
    {
      gdk_window_remove_filter (tv_info.root_window, root_filter, NULL);

      g_signal_handlers_disconnect_matched
	(G_OBJECT (tv_info.main_window),
	 G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
	 0, 0, NULL, G_CALLBACK (on_main_window_event), NULL);

      g_signal_handlers_disconnect_matched
	(G_OBJECT (osd_model),
	 G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
	 0, 0, NULL, G_CALLBACK (on_osd_model_changed), NULL);
    }

  terminate ();

  tv_clip_vector_destroy (&tv_info.set_vector);
  tv_clip_vector_destroy (&tv_info.tmp_vector);
  tv_clip_vector_destroy (&tv_info.cur_vector);

  tv_set_capture_mode (tv_info.info, CAPTURE_MODE_NONE);

  CLEAR (tv_info);
}

gboolean
start_overlay			(void)
{
  GdkEventMask mask;

  tv_info.main_window = GTK_WIDGET (zapping);
  tv_info.video_window = GTK_WIDGET (zapping->video);

  /* XXX no const limit please */
  z_video_set_max_size (zapping->video, 768, 576);

  tv_info.info = zapping->info;

  gdk_window_get_origin (GTK_WIDGET (zapping)->window,
			 &tv_info.mw_x, &tv_info.mw_y);

  gdk_window_get_geometry (GTK_WIDGET (zapping->video)->window,
			   &tv_info.vw_x, &tv_info.vw_y,
			   &tv_info.vw_width, &tv_info.vw_height,
			   /* depth */ NULL);

  tv_info.screen = tv_screen_list_find (screens,
					tv_info.mw_x + tv_info.vw_x,
					tv_info.mw_y + tv_info.vw_y,
					(guint) tv_info.vw_width,
					(guint) tv_info.vw_height);
  if (!tv_info.screen)
    tv_info.screen = screens;

  CLEAR (tv_info.window);

  tv_clip_vector_init (&tv_info.cur_vector);
  tv_clip_vector_init (&tv_info.tmp_vector);
  tv_clip_vector_init (&tv_info.set_vector);

  tv_info.visibility		= GDK_VISIBILITY_PARTIAL; /* assume worst */

  tv_info.clean_screen		= FALSE;
  tv_info.geometry_changed	= TRUE;

  tv_info.timeout_id		= NO_SOURCE_ID;

  /* Make sure we use an Xv adaptor which can render into da->window.
     (Won't help with X.org but it's the right thing to do.) */
  tveng_close_device(zapping->info);
  if (-1 == tveng_attach_device
      (zcg_char (NULL, "video_device"),
       GDK_WINDOW_XWINDOW (GTK_WIDGET (zapping->video)->window),
       TVENG_ATTACH_XV, zapping->info))
    {
      ShowBox("Overlay mode not available:\n%s",
	      GTK_MESSAGE_ERROR, tv_get_errstr (zapping->info));
      goto failure;
    }

  tv_info.needs_cleaning =
    (tv_get_controller (zapping->info) != TVENG_CONTROLLER_XV);

  if (tv_get_controller (zapping->info) != TVENG_CONTROLLER_XV
      && (OVERLAY_CHROMA_TEST
	  || (tv_get_caps (zapping->info)->flags & TVENG_CAPS_CHROMAKEY)))
    {
      GdkColor chroma;

      CLEAR (chroma);

      if (OVERLAY_CHROMA_TEST)
	chroma.red = 0xffff;
      else
	chroma.blue = 0xffff;

      if (gdk_colormap_alloc_color (gdk_colormap_get_system (),
				    &chroma, FALSE, TRUE))
	{
	  z_set_window_bg (GTK_WIDGET (zapping->video), &chroma);

	  /* XXX safe? (we run on 15/16/24/32 yet, but 8 bit later?) */
	  gdk_colormap_free_colors (gdk_colormap_get_system(), &chroma, 1);
	}
      else
	{
	  ShowBox ("Couldn't allocate chromakey, chroma won't work",
		   GTK_MESSAGE_WARNING);
	}
    }
  else if (tv_get_controller (zapping->info) == TVENG_CONTROLLER_XV)
    {
      GdkColor chroma;
      unsigned int chromakey;
      CLEAR (chroma);

      /* Error ignored */
      tv_get_overlay_chromakey (zapping->info, &chromakey);

      chroma.pixel = chromakey;

      z_set_window_bg (GTK_WIDGET (zapping->video), &chroma);
    }

  /* Disable double buffering just in case, will help when a
     XV driver doesn't provide XV_COLORKEY but requires the colorkey
     not to be overwritten */
  gtk_widget_set_double_buffered (GTK_WIDGET (zapping->video), FALSE);

  if (TVENG_CONTROLLER_XV == tv_get_controller (zapping->info))
    {
      if (!tv_set_overlay_xwindow
	  (tv_info.info,
	   GDK_WINDOW_XWINDOW (GTK_WIDGET (zapping->video)->window),
	   GDK_GC_XGC (GTK_WIDGET (zapping->video)->style->white_gc)))
	goto failure;

      /* Just update overlay on video_window size change. */

      g_signal_connect (G_OBJECT (zapping->video), "event",
			G_CALLBACK (on_video_window_event), NULL);

      mask = gdk_window_get_events (GTK_WIDGET (zapping->video)->window);
      mask |= GDK_CONFIGURE;
      gdk_window_set_events (GTK_WIDGET (zapping->video)->window, mask);
    }
  else
    {
      if (!z_set_overlay_buffer (zapping->info,
				 tv_info.screen,
				 GTK_WIDGET (zapping->video)->window))
	goto failure;

      g_signal_connect (G_OBJECT (osd_model), "changed",
			G_CALLBACK (on_osd_model_changed), NULL);

      g_signal_connect (G_OBJECT (zapping->video), "event",
			G_CALLBACK (on_video_window_event), NULL);

      mask = gdk_window_get_events (GTK_WIDGET (zapping->video)->window);
      mask |= GDK_VISIBILITY_NOTIFY_MASK | GDK_CONFIGURE | GDK_EXPOSE;
      gdk_window_set_events (GTK_WIDGET (zapping->video)->window, mask);

      /* We must connect to main_window because the video_window
	 GDK_CONFIGURE event notifies only about main_window relative,
	 not absolute i.e. root window relative position changes.
         GDK_UNMAP, but no visibility event, is sent after the window
	 was rolled up or minimized. */

      g_signal_connect (G_OBJECT (zapping), "event",
			G_CALLBACK (on_main_window_event), NULL);

      mask = gdk_window_get_events (GTK_WIDGET (zapping)->window);
      mask |= GDK_UNMAP | GDK_CONFIGURE;
      gdk_window_set_events (GTK_WIDGET (zapping)->window, mask);

      /* There is no GDK_OBSCURED event, we must monitor all child
	 windows of the root window. E.g. drop-down menus. */

      tv_info.root_window = gdk_get_default_root_window ();

      gdk_window_add_filter (tv_info.root_window, root_filter, NULL);

      mask = GDK_STRUCTURE_MASK | GDK_SUBSTRUCTURE_MASK;
      gdk_window_set_events (tv_info.root_window, mask);
    }

  g_signal_connect (G_OBJECT (zapping), "delete-event",
		    G_CALLBACK (on_main_window_delete_event), NULL);

  /* Start overlay */

  if (tv_info.needs_cleaning)
    {
      restart_timeout ();
    }
  else
    {
      /* XXX tveng_set_preview_window (currently default is
	 fill window size) */

      if (!OVERLAY_CHROMA_TEST)
	{
	  /* XXX error ignored */
	  tv_enable_overlay (tv_info.info, TRUE);
	}
    }

  zapping->display_mode = DISPLAY_MODE_WINDOW;
  tv_set_capture_mode (zapping->info, CAPTURE_MODE_OVERLAY);

  return TRUE;

 failure:
  return FALSE;
}
