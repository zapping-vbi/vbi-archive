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
#include "overlay.h"
#include "osd.h"
#include "globals.h"
#include "zvideo.h"
#include "v4linterface.h"

/* This code implements video overlay. It supports three methods:

   We can ask a kernel device to continuously store video images in
   some arbitrary memory area, i.e. video memory, with clipping
   rectangles. Of course the device won't know about windows
   "obscuring" the overlay, so we can either disable X11 completely,
   or ask the X server to send events before opening or moving
   windows, and after closing windows. Alas, there are no "before"
   events, we have to disable the overlay after the fact, redraw the
   areas we shouldn't have overlaid, and restart the overlay at the
   new coordinates and with new clipping rectangles.

   Second we can ask a kernel device which can somehow overlay a VGA
   signal to replace all pixels of a specific color by video image
   pixels. Here we only have to reprogram the device when the size or
   position of the target window which shall display the video
   changes.

   An XVideo driver handles clipping or chromakeying automatically,
   we only have to pick a target window and adjust the overlay size
   and position relative to the window.
*/

/* TODO:
   + Special mode for devices without clipping. Hint: think twice.
   + Matte option (clip out WSS, GCR).
   + Source rectangle, if supported by hardware (e.g. zoom function).
 */

#ifndef OVERLAY_LOG_FP
#  define OVERLAY_LOG_FP 0
#endif
#ifndef OVERLAY_EVENT_LOG_FP
#  define OVERLAY_EVENT_LOG_FP 0
#endif
#ifndef OVERLAY_DUMP_CLIPS
#  define OVERLAY_DUMP_CLIPS 0
#endif
#ifndef OVERLAY_CHROMA_TEST
#  define OVERLAY_CHROMA_TEST 0
#endif
#ifndef OVERLAY_COLORMAP_FAILURE_TEST
#  define OVERLAY_COLORMAP_FAILURE_TEST 0
#endif

#define CLEAR_TIMEOUT 50 /* ms for the clear window timeout */

/* Preliminary. */
#define CHROMA_KEY 0xCCCCFF /* 0xBBGGRR */

enum mode {
  XVIDEO_OVERLAY = 1,
  CHROMA_KEY_OVERLAY,
  CLIP_OVERLAY,
};

static struct {
  /** The screen containing video_window (Xinerama). */
  const tv_screen *	screen;

  /** The root window on said screen. */
  GdkWindow *		root_window;

  /** The Zapping main window, top level parent of video_window. */
  GtkWidget *		main_window;

  /** The child window which actually displays the overlay. */
  GtkWidget *		video_window;

  /** Last known position of the main_window, relative to root_window. */ 
  gint			mw_x;
  gint			mw_y;

  /** Last known position of the video_window, relative to main_window. */ 
  gint			vw_x;
  gint			vw_y;

  /** Last known size of the video_window. */
  guint			vw_width;
  guint			vw_height;

  /** The overlay rectangle, relative to root_window. */
  tv_window		overlay_rect;

  GdkColormap *		colormap;
  GdkColor		chroma_key_color;

  GdkEventMask		old_root_events;
  GdkEventMask		old_main_events;
  GdkEventMask		old_video_events;

  enum mode		mode;

  /** XVideo or kernel device. */
  tveng_device_info *	info;

  /** Additional data for CLIP_OVERLAY. */

  /** Last known visibility of the video_window. */ 
  GdkVisibilityState	visibility;

  /**
   * Regions obscuring the overlay_rect,
   * relative to overlay_rect position.
   */
  tv_clip_vector	cur_vector;
  tv_clip_vector	tmp_vector;

  /* XXX is same as zapping->info.overlay.clip_vector? */ 
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
       tv_info.overlay_rect.x,
       tv_info.overlay_rect.y,
       tv_info.overlay_rect.width,
       tv_info.overlay_rect.height))
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
				     &tv_info.overlay_rect,
				     &tv_info.cur_vector))
    {
      tv_clip_vector_set (&tv_info.set_vector, &tv_info.cur_vector);
      return TRUE;
    }

  return FALSE;
}

static gboolean
obscured_timeout		(gpointer		user_data)
{
  const tv_window *window;

  user_data = user_data;

  if (OVERLAY_LOG_FP)
    fprintf (OVERLAY_LOG_FP, "obscured_timeout\n");

  /* The window changed from fully or partially visible
     to fully obscured. */

  window = tv_cur_overlay_window (tv_info.info);

  if (tv_info.clean_screen)
    {
      expose_screen ();
    }
  else
    {
      x11_force_expose (window->x,
			window->y,
			window->width,
			window->height);
    }

  tv_clip_vector_clear (&tv_info.cur_vector);

  /* XXX error ignored */
  tv_clip_vector_add_clip_xy (&tv_info.cur_vector,
			      0, 0, window->width, window->height);

  tv_info.geometry_changed = TRUE;

  tv_info.timeout_id = NO_SOURCE_ID;

  return FALSE; /* remove timeout */
}

static gboolean
visible_timeout			(gpointer		user_data)
{
  user_data = user_data;

  if (OVERLAY_LOG_FP)
    fprintf (OVERLAY_LOG_FP, "visible_timeout\n");

  /* The Window changed from fully or partially obscured to
     unobscured (cleaning the clip vector is not enough, we must
     still clip against the video_window bounds).

     Or from fully obscured or unobscured to partially obscured.

     Or the clip vector may have changed while the window is
     partially obscured. */

  /* XXX error ignored */
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

      tv_info.overlay_rect.x		= tv_info.mw_x + tv_info.vw_x;
      tv_info.overlay_rect.y		= tv_info.mw_y + tv_info.vw_y;
      tv_info.overlay_rect.width	= tv_info.vw_width;
      tv_info.overlay_rect.height	= tv_info.vw_height;

      retry_count = 5;

      for (;;)
	{
	  const tv_window *w;

	  if (retry_count-- == 0)
	    goto finish; /* XXX */

	  /* XXX error ignored */
	  set_window ();

	  w = tv_cur_overlay_window (tv_info.info);

	  if (tv_window_equal (&tv_info.overlay_rect, w))
	    break;

	  /* The driver modified the overlay bounds (alignment, limits),
	     we must update the clips. */

	  tv_info.overlay_rect.x      = w->x;
	  tv_info.overlay_rect.y      = w->y;
	  tv_info.overlay_rect.width  = w->width;
	  tv_info.overlay_rect.height = w->height;

	  /* XXX error ignored */
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

  if (GDK_VISIBILITY_FULLY_OBSCURED == tv_info.visibility)
    {
      tv_info.timeout_id =
	g_timeout_add (CLEAR_TIMEOUT,
		       (GSourceFunc) obscured_timeout, &tv_info);
    }
  else
    {
      tv_info.timeout_id =
	g_timeout_add (CLEAR_TIMEOUT,
		       (GSourceFunc) visible_timeout, &tv_info);
    }
}

static gboolean
reconfigure			(void)
{
  switch (tv_info.mode)
    {
    case CLIP_OVERLAY:
      tv_info.geometry_changed = TRUE;
      tv_info.clean_screen = TRUE; /* see root_filter() */

      if (GDK_VISIBILITY_FULLY_OBSCURED != tv_info.visibility)
	restart_timeout ();

      break;

    case CHROMA_KEY_OVERLAY:
      /* Implied: tv_enable_overlay (tv_info.info, FALSE); */

      if (tv_info.colormap)
	{
	  /* Restore background color (chroma key). XXX the X server
	     should do this automatically, did we we disabled that
	     somewhere to avoid flicker in capture mode? */
	  gdk_window_clear_area (tv_info.video_window->window,
				 /* x, y */ 0, 0,
				 tv_info.vw_width,
				 tv_info.vw_height);
	}

      tv_info.overlay_rect.x = tv_info.mw_x + tv_info.vw_x;
      tv_info.overlay_rect.y = tv_info.mw_y + tv_info.vw_y;
      tv_info.overlay_rect.width = tv_info.vw_width;
      tv_info.overlay_rect.height = tv_info.vw_height;

      if (0)
	fprintf (stderr, "reconfigure overlay_rect: %ux%u+%d+%d\n",
		 tv_info.overlay_rect.width,
		 tv_info.overlay_rect.height,
		 tv_info.overlay_rect.x,
		 tv_info.overlay_rect.y);

      if (!tv_set_overlay_window_chromakey (tv_info.info,
					    &tv_info.overlay_rect,
					    CHROMA_KEY))
	return FALSE;

      if (!tv_enable_overlay (tv_info.info, TRUE))
	return FALSE;

      break;

    case XVIDEO_OVERLAY:
      /* XVideo overlay is automatically positioned relative to
	 the video_window origin, we only have to adjust the
	 overlay size. */

      tv_enable_overlay (tv_info.info, FALSE);

      if (tv_info.colormap)
	{
	  /* Restore background color (chroma key). */
	  gdk_window_clear_area (tv_info.video_window->window,
				 /* x, y */ 0, 0,
				 tv_info.vw_width,
				 tv_info.vw_height);
	}

      /* XXX set overlay rectangle here, currently the XVideo
	 interface fills the entire window when overlay is enabled. */

      /* XXX off/on is inefficient, XVideo drivers
	 do that automatically. */
      if (!tv_enable_overlay (tv_info.info, TRUE))
	return FALSE;

      break;
    }

  return TRUE;
}

static gboolean
on_video_window_event		(GtkWidget *		widget,
				 GdkEvent *		event,
				 gpointer		user_data)
{
  user_data = user_data;

  g_assert (widget == tv_info.video_window);

  if (OVERLAY_EVENT_LOG_FP)
    fprintf (OVERLAY_EVENT_LOG_FP,
	     "on_video_window_event: GDK_%s\n",
	     z_gdk_event_name (event));

  switch (event->type)
    {
    case GDK_CONFIGURE:
      /* Size, position, stacking order changed. Note position
         is relative to the parent window. */

      if (tv_info.vw_width != (guint) event->configure.width
	  || tv_info.vw_height != (guint) event->configure.height
	  || tv_info.vw_x != (gint) event->configure.x
	  || tv_info.vw_y != (gint) event->configure.y)
	{
	  /* XXX really mw relative? */
	  tv_info.vw_x = event->configure.x; 
	  tv_info.vw_y = event->configure.y;
	      
	  tv_info.vw_width = event->configure.width;
	  tv_info.vw_height = event->configure.height;

	  /* XXX error ignored. */
	  reconfigure ();
	}

      break;

    case GDK_VISIBILITY_NOTIFY:
      /* Visibility state changed: obscured, partially or fully visible. */

      if (CLIP_OVERLAY == tv_info.mode)
	{
	  tv_info.visibility = event->visibility.state;
	  restart_timeout ();
	}

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

      if (CLIP_OVERLAY == tv_info.mode)
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
  user_data = user_data;

  g_assert (widget == tv_info.main_window);

  if (OVERLAY_EVENT_LOG_FP)
    fprintf (OVERLAY_EVENT_LOG_FP,
	     "on_main_window_event: GDK_%s\n",
	     z_gdk_event_name (event));

  switch (event->type)
    {
    case GDK_CONFIGURE:
      /* Size, position, stacking order changed. Note position
         is relative to the parent window. */

      switch (tv_info.mode)
	{
	case CLIP_OVERLAY:
	case CHROMA_KEY_OVERLAY:
	  if (tv_info.mw_x != event->configure.x
	      || tv_info.mw_y != event->configure.y)
	    {
	      /* XXX really root relative? */
	      tv_info.mw_x = event->configure.x;
	      tv_info.mw_y = event->configure.y;

	      /* XXX error ignored. */
	      reconfigure ();
	    }

	  break;

	case XVIDEO_OVERLAY:
	  g_assert_not_reached ();
	}

      break;

    case GDK_UNMAP:
      /* Window was rolled up or minimized. For some reason no
	 visibility events are sent in this case. */

      if (CLIP_OVERLAY == tv_info.mode)
	{
	  tv_info.visibility = GDK_VISIBILITY_FULLY_OBSCURED;
	  restart_timeout ();
	}

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
      const gchar *s;

      switch (event->type)
	{
#define CASE(event) case event: s = #event; break;
	CASE (KeyPress)
	CASE (KeyRelease)
	CASE (ButtonPress)
	CASE (ButtonRelease)
	CASE (MotionNotify)
	CASE (EnterNotify)
	CASE (LeaveNotify)
	CASE (FocusIn)
	CASE (FocusOut)
	CASE (KeymapNotify)
	CASE (Expose)
	CASE (GraphicsExpose)
	CASE (NoExpose)
	CASE (VisibilityNotify)
	CASE (CreateNotify)
	CASE (DestroyNotify)
	CASE (UnmapNotify)
	CASE (MapNotify)
	CASE (MapRequest)
	CASE (ReparentNotify)
	CASE (ConfigureNotify)
	CASE (ConfigureRequest)
	CASE (GravityNotify)
	CASE (ResizeRequest)
	CASE (CirculateNotify)
	CASE (CirculateRequest)
	CASE (PropertyNotify)
	CASE (SelectionClear)
	CASE (SelectionRequest)
	CASE (SelectionNotify)
	CASE (ColormapNotify)
	CASE (ClientMessage)
	CASE (MappingNotify)
#undef CASE
	default: s = "Unknown"; break;
	}

      fprintf (OVERLAY_EVENT_LOG_FP, "root_filter: %s\n", s);
    }

  if (tv_info.visibility != GDK_VISIBILITY_PARTIAL)
    return GDK_FILTER_CONTINUE;

  switch (event->type)
    {
    case ConfigureNotify:
      {
	/* We could just refresh regions we previously DMAed, but
	   unfortunately it's possible to move a destroyed window
	   away before we did. What's worse, imperfect refresh or
	   unconditionally refreshing the entire screen? */

	if (TRUE)
	  {
	    tv_info.clean_screen = TRUE;
	  }
	else
	  {
	    XConfigureEvent *ev;
	    const tv_window *win;
	    int evx2, evy2;
	    int wx2, wy2;

	    ev = &event->xconfigure;

	    win = tv_cur_overlay_window (tv_info.info);

	    evx2 = ev->x + ev->width;
	    evy2 = ev->y + ev->height;

	    wx2 = win->x + win->width;
	    wy2 = win->y + win->height;

	    if ((int)(ev->x - ev->border_width) >= wx2
		|| (int)(evx2 + ev->border_width) <= (int) win->x
		|| (int)(ev->y - ev->border_width) >= wy2
		|| (int)(evy2 + ev->border_width) <= (int) win->y)
	      {
		/* Windows do not overlap. */
		break;
	      }
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
  if (CLIP_OVERLAY == tv_info.mode
      && GDK_VISIBILITY_FULLY_OBSCURED != tv_info.visibility)
    {
      tv_info.visibility = GDK_VISIBILITY_PARTIAL;
      restart_timeout ();
    }
}

static void
terminate			(void)
{
  stop_timeout ();

  tv_enable_overlay (tv_info.info, FALSE);

  if (CLIP_OVERLAY == tv_info.mode)
    {
      usleep (CLEAR_TIMEOUT * 10);

      if (tv_info.clean_screen)
	expose_screen ();
      else
	expose_window_clip_vector (tv_cur_overlay_window (tv_info.info),
				   &tv_info.set_vector);
    }
}

static gboolean
on_main_window_delete_event	(GtkWidget *		widget,
				 GdkEvent *		event,
				 gpointer		user_data)
{
  widget = widget;
  event = event;
  user_data = user_data;

  terminate ();

  return FALSE; /* pass on */
}

typedef gboolean
event_function			(GtkWidget *		widget,
				 GdkEvent  *		event,
				 gpointer		user_data);

static GdkEventMask
event_signal_disconnect		(GtkWidget *		widget,
				 event_function *	callback,
				 GdkEventMask		old_events)
{
  g_signal_handlers_disconnect_matched (G_OBJECT (widget),
					(G_SIGNAL_MATCH_FUNC |
					 G_SIGNAL_MATCH_DATA),
					/* signal_id */ 0,
					/* detail */ 0,
					/* closure */ NULL,
					G_CALLBACK (callback),
					/* data */ NULL);

  gdk_window_set_events (widget->window, old_events);
}

void
stop_overlay			(void)
{
  g_assert (tv_info.main_window != NULL);

  /* XXX no const limit please */
  z_video_set_max_size (Z_VIDEO (tv_info.video_window), 16384, 16384);

  switch (tv_info.mode)
    {
    case CLIP_OVERLAY:
      gdk_window_set_events (tv_info.root_window,
			     tv_info.old_root_events);

      gdk_window_remove_filter (tv_info.root_window, root_filter, NULL);

      g_signal_handlers_disconnect_matched
	(G_OBJECT (osd_model),
	 G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
	 0, 0, NULL, G_CALLBACK (on_osd_model_changed), NULL);

      /* fall through */

    case CHROMA_KEY_OVERLAY:
      event_signal_disconnect (tv_info.main_window,
			       on_main_window_event,
			       tv_info.old_main_events);

      /* fall through */

    case XVIDEO_OVERLAY:
      event_signal_disconnect (tv_info.video_window,
			       on_video_window_event,
			       tv_info.old_video_events);
    }

  terminate ();

  if (tv_info.colormap)
    {
      gdk_colormap_free_colors (tv_info.colormap,
				&tv_info.chroma_key_color, 1);

      g_object_unref (G_OBJECT (tv_info.colormap));
      tv_info.colormap = NULL;

      z_set_window_bg_black (tv_info.video_window);
    }

  tv_clip_vector_destroy (&tv_info.set_vector);
  tv_clip_vector_destroy (&tv_info.tmp_vector);
  tv_clip_vector_destroy (&tv_info.cur_vector);

  tv_set_capture_mode (tv_info.info, CAPTURE_MODE_NONE);

  CLEAR (tv_info);
}

static gboolean
set_window_bg			(void)
{
  if (!(OVERLAY_CHROMA_TEST
	|| (tv_get_caps (zapping->info)->flags & TVENG_CAPS_CHROMAKEY)))
    {
      z_set_window_bg_black (tv_info.video_window);

      return TRUE;
    }

  if (tv_info.colormap)
    {
      gdk_colormap_free_colors (tv_info.colormap,
				&tv_info.chroma_key_color, 1);
    }
  else
    {
      tv_info.colormap = gdk_colormap_get_system ();
    }

  CLEAR (tv_info.chroma_key_color);

  tv_info.chroma_key_color.red = CHROMA_KEY & 0xFF;
  tv_info.chroma_key_color.red |= tv_info.chroma_key_color.red << 8;
  tv_info.chroma_key_color.green = CHROMA_KEY & 0xFF00;
  tv_info.chroma_key_color.green |= tv_info.chroma_key_color.green >> 8;
  tv_info.chroma_key_color.blue = (CHROMA_KEY & 0xFF0000) >> 16;
  tv_info.chroma_key_color.blue |= tv_info.chroma_key_color.blue << 8;

  if (!OVERLAY_COLORMAP_FAILURE_TEST
      && gdk_colormap_alloc_color (tv_info.colormap,
				   &tv_info.chroma_key_color,
				   /* writable */ FALSE,
				   /* or_best_match */ TRUE))
    {
      z_set_window_bg (tv_info.video_window, &tv_info.chroma_key_color);

      return TRUE;
    }
  else
    {
      g_object_unref (G_OBJECT (tv_info.colormap));
      tv_info.colormap = NULL;

      z_set_window_bg_black (tv_info.video_window);

      z_show_non_modal_message_dialog
	(GTK_WINDOW (zapping), GTK_MESSAGE_ERROR,
	 _("No chroma-key color"),
	 _("Could not allocate the color #%2X%2X%2X for chroma-keying. "
	   "Try to select another color.\n"),
	 CHROMA_KEY & 0xFF,
	 (CHROMA_KEY >> 8) & 0xFF,
	 (CHROMA_KEY >> 16) & 0xFF);

      return FALSE;
    }
}

static GdkEventMask
event_signal_connect		(GtkWidget *		widget,
				 event_function *	callback,
				 GdkEventMask		new_events)
{
  GdkEventMask old_events;

  if (NULL != callback)
    g_signal_connect (G_OBJECT (widget), "event",
		      G_CALLBACK (callback), /* user_data */ NULL);

  old_events = gdk_window_get_events (widget->window);
  gdk_window_set_events (widget->window, old_events | new_events);

  return old_events;
}

gboolean
start_overlay			(void)
{
  Window xwindow;
  gint width;
  gint height;

  tv_info.main_window = GTK_WIDGET (zapping);
  tv_info.video_window = GTK_WIDGET (zapping->video);

  /* XXX no const limit please */
  z_video_set_max_size (zapping->video, 768, 576);

  tv_info.info = zapping->info;

  gdk_window_get_origin (GTK_WIDGET (zapping)->window,
			 &tv_info.mw_x, &tv_info.mw_y);

  gdk_window_get_geometry (GTK_WIDGET (zapping->video)->window,
			   &tv_info.vw_x, &tv_info.vw_y,
			   &width, &height,
			   /* depth */ NULL);

  tv_info.vw_width = width;
  tv_info.vw_height = height;

  tv_info.screen = tv_screen_list_find (screens,
					tv_info.mw_x + tv_info.vw_x,
					tv_info.mw_y + tv_info.vw_y,
					(guint) tv_info.vw_width,
					(guint) tv_info.vw_height);
  if (!tv_info.screen)
    tv_info.screen = screens;

  CLEAR (tv_info.overlay_rect);

  tv_clip_vector_init (&tv_info.cur_vector);
  tv_clip_vector_init (&tv_info.tmp_vector);
  tv_clip_vector_init (&tv_info.set_vector);

  tv_info.visibility		= GDK_VISIBILITY_PARTIAL; /* assume worst */

  tv_info.clean_screen		= FALSE;
  tv_info.geometry_changed	= TRUE;

  tv_info.timeout_id		= NO_SOURCE_ID;

  /* Make sure we use an XVideo adaptor which can render into
     da->window. (Doesn't matter with X.org but it's the right thing
     to do.) */
  xwindow = GDK_WINDOW_XWINDOW (tv_info.video_window->window);

  tveng_close_device (zapping->info);

  /* Switch to overlay mode, XVideo or other. */
  if (-1 == tveng_attach_device (zcg_char (NULL, "video_device"),
				 xwindow, TVENG_ATTACH_XV, zapping->info))
    {
      z_show_non_modal_message_dialog
	(GTK_WINDOW (zapping), GTK_MESSAGE_ERROR,
	 _("Cannot switch to overlay mode"),
	 "%s", tv_get_errstr (zapping->info));

      goto failure;
    }

  /* Switch to selected video input, RF channel on new device. */
  zconf_get_sources (zapping->info, /* mute */ FALSE);

  if (OVERLAY_CHROMA_TEST)
    {
      tv_info.mode = CHROMA_KEY_OVERLAY;
    }
  else if (TVENG_CONTROLLER_XV == tv_get_controller (zapping->info))
    {
      tv_info.mode = XVIDEO_OVERLAY;
    }
  else
    {
      if (tv_get_caps (zapping->info)->flags & TVENG_CAPS_CHROMAKEY)
	{
	  tv_info.mode = CHROMA_KEY_OVERLAY;
	}
      else
	{
	  tv_info.mode = CLIP_OVERLAY;
	}
    }

  switch (tv_info.mode)
    {
      GdkEventMask events;
      GC xgc;

    case XVIDEO_OVERLAY:
      xwindow = GDK_WINDOW_XWINDOW (tv_info.video_window->window);
      xgc = GDK_GC_XGC (tv_info.video_window->style->white_gc);

      if (!tv_set_overlay_xwindow (tv_info.info, xwindow, xgc, CHROMA_KEY))
	goto failure;

      /* Error ignored so the user can try another chroma key. */
      set_window_bg ();

      /* Disable double buffering just in case, will help when a
	 XV driver doesn't provide XV_COLORKEY but requires the colorkey
	 not to be overwritten */
      gtk_widget_set_double_buffered (tv_info.video_window, FALSE);

      /* Update on video_window size change. */
      tv_info.old_video_events =
	event_signal_connect (tv_info.video_window,
			      on_video_window_event,
			      GDK_CONFIGURE);

      /* Start overlay, XXX error ignored. */
      reconfigure ();
    
      break;

    case CHROMA_KEY_OVERLAY:
      if (!z_set_overlay_buffer (zapping->info,
				 tv_info.screen,
				 tv_info.video_window->window))
	goto failure;

      /* Error ignored so the user can try another chroma key. */
      set_window_bg ();

      /* Update overlay on video_window size and position change. We
	 must connect to main_window as well because the video_window
	 GDK_CONFIGURE event notifies only about main_window, not
	 root_window relative position changes. */

      tv_info.old_video_events =
	event_signal_connect (tv_info.video_window,
			      on_video_window_event,
			      GDK_CONFIGURE);

      tv_info.old_main_events =
	event_signal_connect (tv_info.main_window,
			      on_main_window_event,
			      GDK_CONFIGURE);

      /* Start overlay, XXX error ignored. */
      reconfigure ();

      break;

    case CLIP_OVERLAY:
      if (!z_set_overlay_buffer (zapping->info,
				 tv_info.screen,
				 GTK_WIDGET (zapping->video)->window))
	goto failure;

      g_signal_connect (G_OBJECT (osd_model), "changed",
			G_CALLBACK (on_osd_model_changed), NULL);

      tv_info.old_video_events =
	event_signal_connect (tv_info.video_window,
			      on_video_window_event,
			      (GDK_VISIBILITY_NOTIFY_MASK |
			       GDK_CONFIGURE |
			       GDK_EXPOSE));

      /* We must connect to main_window as well because the
	 video_window GDK_CONFIGURE event notifies only about
	 main_window, not root_window relative position
	 changes. GDK_UNMAP, but no visibility event, is sent
	 after the window was rolled up or minimized. */

      tv_info.old_main_events =
	event_signal_connect (tv_info.main_window,
			      on_main_window_event,
			      (GDK_UNMAP |
			       GDK_CONFIGURE));

      /* There is no GDK_OBSCURED event, we must monitor all child
	 windows of the root window. E.g. drop-down menus. */

      tv_info.root_window = gdk_get_default_root_window ();

      gdk_window_add_filter (tv_info.root_window,
			     root_filter, /* user_data */ NULL);

      tv_info.old_root_events = gdk_window_get_events (tv_info.root_window);
      gdk_window_set_events (tv_info.root_window,
			     (tv_info.old_root_events |
			      GDK_STRUCTURE_MASK |
			      GDK_SUBSTRUCTURE_MASK));

      /* Start overlay. */

      restart_timeout ();

      break;
    }

  g_signal_connect (G_OBJECT (zapping), "delete-event",
		    G_CALLBACK (on_main_window_delete_event), NULL);

  zapping->display_mode = DISPLAY_MODE_WINDOW;
  zapping->display_window = GTK_WIDGET (zapping->video);
  tv_set_capture_mode (zapping->info, CAPTURE_MODE_OVERLAY);

  return TRUE;

 failure:
  return FALSE;
}
