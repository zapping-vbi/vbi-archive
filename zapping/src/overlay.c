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
#include "config.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <tveng.h>

#include "x11stuff.h"
#include "zmisc.h"
#include "zvbi.h"
#include "overlay.h"
#include "osd.h"
#include "globals.h"

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

#ifndef OVERLAY_LOG
#define OVERLAY_LOG 0
#endif

#define CLEAR_TIMEOUT 50 /* ms for the clear window timeout */

static struct {
  GdkWindow *		root_window;

  GtkWidget *		main_window;	/* top level parent of video_window */
  GtkWidget *		video_window;	/* displays the overlay */

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

  tv_clip_vector	tmp_vector;

  gboolean		clean_screen;
  gboolean		geometry_changed;

  gint			timeout_id;

  /* XVideo overlay */

  Window		xwindow;
  GC			xgc;
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

static __inline__ void
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
}

static void
expose_window_clip_vector	(tv_window *		window)
{
  tv_clip *clip;
  guint count;

  clip = window->clip_vector.vector;
  count = window->clip_vector.size;

  while (count-- > 0)
    {
      x11_force_expose (window->x + clip->x,
			window->y + clip->y,
			clip->width,
			clip->height);
      ++clip;
    }
}

static gboolean
set_window			(void)
{
  tv_info.info->overlay_window.x	= tv_info.window.x;
  tv_info.info->overlay_window.y	= tv_info.window.y;
  tv_info.info->overlay_window.width	= tv_info.window.width;
  tv_info.info->overlay_window.height	= tv_info.window.height;

  if (!tv_clip_vector_copy (&tv_info.info->overlay_window.clip_vector,
			    &tv_info.window.clip_vector))
    return FALSE;

  return (-1 != tveng_set_preview_window (tv_info.info));
}

static gboolean
obscured_timeout		(gpointer		user_data)
{
  tv_window *window;

  if (OVERLAY_LOG)
    fprintf (stderr, "obscured_timeout\n");

  /* Changed from partially visible or unobscured to fully obscured. */

  window = &tv_info.info->overlay_window;

  if (tv_info.clean_screen)
    {
      tv_info.clean_screen = FALSE;
      x11_force_expose (0, 0, gdk_screen_width (), gdk_screen_height ());
    }
  else
    {
      x11_force_expose (window->x, window->y, window->width, window->height);
    }

  tv_clip_vector_clear (&tv_info.window.clip_vector);
  // XXX error ignored
  tv_clip_vector_add_clip_xy (&tv_info.window.clip_vector,
			      0, 0, window->width, window->height);

  tv_info.geometry_changed = TRUE;

  tv_info.timeout_id = -1;
  return FALSE; /* remove timeout */
}

static gboolean
visible_timeout			(gpointer		user_data)
{
  if (OVERLAY_LOG)
    fprintf (stderr, "visible_timeout\n");

  /* Changed from
     a) fully obscured or partially visible to unobscured
        (cleaning the clip vector is not enough, we must
	 still clip against the video_window bounds)
     b) fully obscured or unobscured to partially visible
     c) possible clip vector change while partially obscured
  */

  // XXX error
  get_clips (&tv_info.tmp_vector);

  if (!tv_clip_vector_equal (&tv_info.window.clip_vector,
			     &tv_info.tmp_vector))
    {
      /* Delay until the situation stabilizes. */

      if (OVERLAY_LOG)
	fprintf (stderr, "visible_timeout: delay\n");

      SWAP (tv_info.window.clip_vector, tv_info.tmp_vector);

      tv_info.geometry_changed = TRUE;

      return TRUE; /* call again */
    }

  /* Resume overlay. */

  if (tv_info.geometry_changed)
    {
      guint retry_count;

      tv_info.geometry_changed = FALSE;

      if (OVERLAY_LOG)
	fprintf (stderr, "visible_timeout: geometry change\n");

      /* Other windows may have received expose events before we
	 were able to turn off the overlay. Resend expose
	 events for those regions which should be clean now. */
      if (tv_info.clean_screen)
	{
	  tv_info.clean_screen = FALSE;
	  x11_force_expose (0, 0, gdk_screen_width (), gdk_screen_height ());
	}
      else
	{
	  expose_window_clip_vector (&tv_info.info->overlay_window);
	}

      /* Desired overlay bounds */

      tv_info.window.x		= tv_info.mw_x + tv_info.vw_x;
      tv_info.window.y		= tv_info.mw_y + tv_info.vw_y;
      tv_info.window.width	= tv_info.vw_width;
      tv_info.window.height	= tv_info.vw_height;

      retry_count = 5;

      for (;;)
	{
	  if (retry_count-- == 0)
	    goto finish; // XXX

	  // XXX error
	  set_window ();

	  if (tv_window_equal (&tv_info.window, &tv_info.info->overlay_window))
	    break;

	  /* The driver modified the overlay bounds (alignment, limits),
	     we must update the clips. */

	  tv_info.window.x      = tv_info.info->overlay_window.x;
	  tv_info.window.y      = tv_info.info->overlay_window.y;
	  tv_info.window.width  = tv_info.info->overlay_window.width;
	  tv_info.window.height = tv_info.info->overlay_window.height;

	  // XXX error
	  get_clips (&tv_info.tmp_vector);
	}
    }
  else if (tv_info.clean_screen)
    {
      x11_force_expose (0, 0, gdk_screen_width (), gdk_screen_height ());
      tv_info.clean_screen = FALSE;
    }

  // XXX error ignored
  tveng_set_preview_on (tv_info.info);

 finish:
  tv_info.timeout_id = -1;
  return FALSE; /* remove timeout */
}

static void
stop_timeout			(void)
{
  if (tv_info.timeout_id >= 0)
    gtk_timeout_remove (tv_info.timeout_id);

  tv_info.timeout_id = -1;
}

static void
restart_timeout			(void)
{
  if (OVERLAY_LOG)
    fprintf (stderr, "restart_timeout\n");

  tveng_set_preview_off (tv_info.info);

  stop_timeout ();

  if (tv_info.visibility == GDK_VISIBILITY_FULLY_OBSCURED)
    tv_info.timeout_id =
      gtk_timeout_add (CLEAR_TIMEOUT, obscured_timeout, &tv_info);
  else
    tv_info.timeout_id =
      gtk_timeout_add (CLEAR_TIMEOUT, visible_timeout, &tv_info);
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
on_video_window_event		(GtkWidget *		widget,
				 GdkEvent *		event,
				 gpointer		user_data)
{
  if (0 && OVERLAY_LOG)
    fprintf (stderr, "on_video_window_event: GDK_%s\n",
	     z_gdk_event_name (event));

  switch (event->type)
    {
    case GDK_CONFIGURE:
      /* Size, position, stacking order changed. Note position
         is relative to the parent window. */

      if (tv_info.needs_cleaning)
	{
	  if (tv_info.vw_width != event->configure.width
	      || tv_info.vw_height != event->configure.height
	      || tv_info.vw_x != event->configure.x
	      || tv_info.vw_y != event->configure.y)
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

	  tveng_set_preview_off (tv_info.info);

	  /* XXX tveng_set_preview_window (currently default is
	     fill window size) */

	  // XXX error
	  // XXX off/on is inefficient, XVideo does this automatically.
	  tveng_set_preview_on (tv_info.info);
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
on_main_window_event		(GtkWidget *		widget,
				 GdkEvent *		event,
				 gpointer		user_data)
{
  if (0 && OVERLAY_LOG)
    fprintf (stderr, "on_main_window_event: GDK_%s\n",
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
				 GdkEvent *		unused,
				 gpointer		data)
{
  XEvent *event = (XEvent *) gdkxevent;

  if (0 && OVERLAY_LOG)
    {
      fprintf (stderr, "root_filter: ");

      switch (event->type)
	{
	case MotionNotify:	fprintf (stderr, "MotionNotify\n"); break;
	case Expose:		fprintf (stderr, "Expose\n"); break;
	case VisibilityNotify:	fprintf (stderr, "VisibilityNotify\n"); break;
	case CreateNotify:	fprintf (stderr, "CreateNotify\n"); break;
	case DestroyNotify:	fprintf (stderr, "DestroyNotify\n"); break;
	case UnmapNotify:	fprintf (stderr, "UnmapNotify\n"); break;
	case MapNotify:		fprintf (stderr, "MapNotify\n"); break;
	case ConfigureNotify:	fprintf (stderr, "ConfigureNotify\n"); break;
	case GravityNotify:	fprintf (stderr, "GravityNotify\n"); break;
	default:		fprintf (stderr, "Unknown\n"); break;
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
	    tv_window *win = &tv_info.info->overlay_window;

	    if ((ev->x - ev->border_width) >= (win->x + win->width)
		|| (ev->x + ev->width + ev->border_width) <= win->x
		|| (ev->y - ev->border_width) >= (win->y + win->height)
		|| (ev->y + ev->height + ev->border_width) <= win->y)
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
on_osd_model_changed		(ZModel *		osd_model,
				 gpointer		ignored)
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

  tveng_set_preview_off (tv_info.info);

  if (tv_info.needs_cleaning)
    {
      usleep (CLEAR_TIMEOUT * 1000);

      if (tv_info.clean_screen)
	x11_force_expose (0, 0, gdk_screen_width (), gdk_screen_height ());
      else
	expose_window_clip_vector (&tv_info.info->overlay_window);
    }
}

static gboolean
on_main_window_delete_event	(GtkWidget *		widget,
				 GdkEvent *		event,
				 gpointer		user_data)
{
  terminate ();

  return FALSE; /* pass on */
}

void
stop_overlay			(void)
{
  g_assert (tv_info.main_window != NULL);

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

  tv_window_destroy (&tv_info.window);

  tv_clip_vector_destroy (&tv_info.tmp_vector);

  tv_info.info->current_mode = TVENG_NO_CAPTURE;

  CLEAR (tv_info);
}

gboolean
start_overlay			(GtkWidget *		main_window,
				 GtkWidget *		video_window,
				 tveng_device_info *	info)
{
  GdkColor chroma;
  GdkEventMask mask;

  tv_info.main_window		= main_window;
  tv_info.video_window		= video_window;

  tv_info.needs_cleaning	=
    (info->current_controller != TVENG_CONTROLLER_XV);

  tv_info.info			= info;

  gdk_window_get_origin (main_window->window,
			 &tv_info.mw_x,
			 &tv_info.mw_y);

  gdk_window_get_geometry (video_window->window,
			   &tv_info.vw_x,
			   &tv_info.vw_y,
			   &tv_info.vw_width,
			   &tv_info.vw_height,
			   NULL);

  tv_window_init (&tv_info.window);

  tv_clip_vector_init (&tv_info.tmp_vector);

  tv_info.xwindow		= GDK_WINDOW_XWINDOW (video_window->window);
  tv_info.xgc			= GDK_GC_XGC (video_window->style->white_gc);

  tv_info.visibility		= GDK_VISIBILITY_PARTIAL; /* assume worst */

  tv_info.clean_screen		= FALSE;
  tv_info.geometry_changed	= TRUE;

  tv_info.timeout_id		= -1;

  if (info->current_controller != TVENG_CONTROLLER_XV &&
      info->caps.flags & TVENG_CAPS_CHROMAKEY)
    {
      CLEAR (chroma);
      chroma.blue = 0xffff;

      if (gdk_colormap_alloc_color (gdk_colormap_get_system (),
				    &chroma, FALSE, TRUE))
	{
	  tveng_set_chromakey (chroma.pixel, info);
	  gdk_window_set_background (video_window->window, &chroma);
	  gdk_colormap_free_colors (gdk_colormap_get_system(), &chroma, 1);
	}
      else
	{
	  ShowBox ("Couldn't allocate chromakey, chroma won't work",
		   GTK_MESSAGE_WARNING);
	}
    }
  else if (info->current_controller == TVENG_CONTROLLER_XV &&
	   tveng_get_chromakey (&chroma.pixel, info) == 0)
    {
      gdk_window_set_background (video_window->window, &chroma);      
    }

  /* Disable double buffering just in case, will help when a
     XV driver doesn't provide XV_COLORKEY but requires the colorkey
     not to be overwritten */
  gtk_widget_set_double_buffered (video_window, FALSE);

  if (tv_info.needs_cleaning)
    {
      if (!tv_set_overlay_buffer (info, &dga_param))
	return FALSE;

      g_signal_connect (G_OBJECT (osd_model), "changed",
			G_CALLBACK (on_osd_model_changed), NULL);

      g_signal_connect (G_OBJECT (video_window), "event",
			G_CALLBACK (on_video_window_event), NULL);

      mask = gdk_window_get_events (video_window->window);
      mask |= GDK_VISIBILITY_NOTIFY_MASK | GDK_CONFIGURE | GDK_EXPOSE;
      gdk_window_set_events (video_window->window, mask);

      /* We must connect to main_window because the video_window
	 GDK_CONFIGURE event notifies only about main_window relative,
	 not absolute i.e. root window relative position changes.
         GDK_UNMAP, but no visibility event, is sent after the window
	 was rolled up or minimized. */

      g_signal_connect (G_OBJECT (main_window), "event",
			G_CALLBACK (on_main_window_event), NULL);

      mask = gdk_window_get_events (main_window->window);
      mask |= GDK_UNMAP | GDK_CONFIGURE;
      gdk_window_set_events (main_window->window, mask);

      /* There is no GDK_OBSCURED event, we must monitor all child
	 windows of the root window. E.g. drop-down menus. */

      tv_info.root_window = gdk_get_default_root_window ();

      gdk_window_add_filter (tv_info.root_window, root_filter, NULL);

      mask = GDK_STRUCTURE_MASK | GDK_SUBSTRUCTURE_MASK;
      gdk_window_set_events (tv_info.root_window, mask);
    }
  else
    {
      if (!tv_set_overlay_xwindow (tv_info.info,
				   tv_info.xwindow,
				   tv_info.xgc))
	return FALSE;

      /* Just update overlay on video_window size change. */

      g_signal_connect (G_OBJECT (video_window), "event",
			G_CALLBACK (on_video_window_event), NULL);

      mask = gdk_window_get_events (video_window->window);
      mask |= GDK_CONFIGURE;
      gdk_window_set_events (video_window->window, mask);
    }

  g_signal_connect (G_OBJECT (main_window), "delete-event",
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

      // XXX error ignored
      tveng_set_preview_on (tv_info.info);
    }

  info->current_mode = TVENG_CAPTURE_WINDOW;

  return TRUE;
}
