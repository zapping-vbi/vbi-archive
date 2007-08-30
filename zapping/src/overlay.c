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
#include "zgconf.h"

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
   + Special mode for devices without clipping.
      Attn: disable OSD and subtitles.
   + Matte option (clip out WSS, GCR).
   + Source rectangle, if supported by hardware (e.g. zoom function).
   + Perhaps integrate this code into zvideo.c.
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
#ifndef OVERLAY_METHOD_FAILURE_TEST
#  define OVERLAY_METHOD_FAILURE_TEST 0
#endif
#ifndef OVERLAY_COLORMAP_FAILURE_TEST
#  define OVERLAY_COLORMAP_FAILURE_TEST 0
#endif

#define CLEAR_TIMEOUT 50 /* ms for the clear window timeout */

enum overlay_mode {
  XVIDEO_OVERLAY = 1,
  CHROMA_KEY_OVERLAY,
  CLIP_OVERLAY,
};

struct context {
  /** XVideo or kernel device. */
  tveng_device_info *	info;

  /** The screen containing video_window (Xinerama). */
  const tv_screen *	screen;

  /** The root window on the current display. */
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
  guint			chroma_key_color_cnxn_id;

  GdkEventMask		old_root_events;
  GdkEventMask		old_main_events;
  GdkEventMask		old_video_events;

  /** See above. */
  enum overlay_mode	overlay_mode;

  /** Additional data for overlay_mode CLIP_OVERLAY. */

  /** Last known visibility of the video_window. */ 
  GdkVisibilityState	visibility;

  /**
   * Regions obscuring the overlay_rect,
   * relative to overlay_rect position.
   */
  tv_clip_vector	cur_vector;
  tv_clip_vector	tmp_vector;

  gboolean		clean_screen;
  gboolean		geometry_changed;

  guint			timeout_id;
};

static struct context tv_info;

#define DEVICE_USES_XVIDEO(c)						\
  (!OVERLAY_METHOD_FAILURE_TEST						\
   && (0 != (tv_get_caps ((c)->info)->flags & TVENG_CAPS_XVIDEO)))

#define DEVICE_SUPPORTS_CHROMA_KEYING(c)				\
  (!OVERLAY_METHOD_FAILURE_TEST						\
   && (OVERLAY_CHROMA_TEST						\
       || (0 != (tv_get_caps ((c)->info)->flags & TVENG_CAPS_CHROMAKEY))))

#define DEVICE_SUPPORTS_CLIPPING(c)					\
  (!OVERLAY_METHOD_FAILURE_TEST						\
   && (0 != (tv_get_caps ((c)->info)->flags & TVENG_CAPS_CLIPPING)))

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
get_clips			(const struct context *	c,
				 tv_clip_vector *	vector)
{
  if (!x11_window_clip_vector
      (vector,
       GDK_WINDOW_XDISPLAY (c->video_window->window),
       GDK_WINDOW_XID (c->video_window->window),
       c->overlay_rect.x,
       c->overlay_rect.y,
       c->overlay_rect.width,
       c->overlay_rect.height))
    g_assert_not_reached (); /* XXX */

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
expose_screen			(struct context *	c)
{
  const tv_screen *s;

  c->clean_screen = FALSE;

  s = c->screen;

  x11_force_expose ((gint) s->x,
		    (gint) s->y,
		    s->width,
		    s->height);
}

static gboolean
select_screen			(struct context *	c)
{
  const tv_screen *s;

  s = tv_screen_list_find (screens,
			   c->mw_x + c->vw_x,
			   c->mw_y + c->vw_y,
			   (guint) c->vw_width,
			   (guint) c->vw_height);
  if (NULL == s)
    {
      /* No pixel of the video_window on any screen. */

      c->screen = NULL;
      c->visibility = GDK_VISIBILITY_FULLY_OBSCURED;

      return TRUE;
    }

  if (c->screen == s)
    return TRUE;

  /* Moved to another Xinerama screen. */

  c->screen = s;

  return tv_set_overlay_buffer (c->info,
				x11_display_name (),
				s->screen_number,
				&s->target);
}

static gboolean
obscured_timeout		(gpointer		user_data)
{
  struct context *c = user_data;
  const tv_window *window;

  /* The window changed from fully or partially visible
     to fully obscured. */

  g_assert (CLIP_OVERLAY == c->overlay_mode);

  if (OVERLAY_LOG_FP)
    fprintf (OVERLAY_LOG_FP, "obscured_timeout\n");

  window = tv_cur_overlay_window (c->info);

  if (c->clean_screen)
    {
      expose_screen (c);
    }
  else
    {
      x11_force_expose (window->x,
			window->y,
			window->width,
			window->height);
    }

  tv_clip_vector_clear (&c->cur_vector);

  /* XXX error ignored */
  tv_clip_vector_add_clip_xy (&c->cur_vector,
			      /* x, y */ 0, 0,
			      window->width, window->height);

  c->geometry_changed = TRUE;

  c->timeout_id = NO_SOURCE_ID;

  return FALSE; /* remove timeout */
}

static gboolean
visible_timeout			(gpointer		user_data)
{
  struct context *c = user_data;

  /* The Window changed from fully or partially obscured to
     unobscured (cleaning the clip vector is not enough, we must
     still clip against the video_window bounds).

     Or from fully obscured or unobscured to partially obscured.

     Or the clip vector may have changed while the window is
     partially obscured. */

  g_assert (CLIP_OVERLAY == c->overlay_mode);

  if (OVERLAY_LOG_FP)
    fprintf (OVERLAY_LOG_FP, "visible_timeout\n");

  /* XXX error ignored */
  get_clips (c, &c->tmp_vector);

  if (!tv_clip_vector_equal (&c->cur_vector, &c->tmp_vector))
    {
      /* Delay until the situation stabilizes. */

      if (OVERLAY_LOG_FP)
	fprintf (OVERLAY_LOG_FP, "visible_timeout: delay\n");

      SWAP (c->cur_vector, c->tmp_vector);

      c->geometry_changed = TRUE;

      return TRUE; /* call again */
    }

  SWAP (c->cur_vector, c->tmp_vector);

  /* Resume overlay. */

  if (c->geometry_changed)
    {
      guint retry_count;

      c->geometry_changed = FALSE;

      if (OVERLAY_LOG_FP)
	fprintf (OVERLAY_LOG_FP, "visible_timeout: geometry change\n");

      /* Other windows may have received expose events before we
	 were able to turn off the overlay. Resend expose
	 events for those regions which should be clean now. */
      if (c->clean_screen)
	expose_screen (c);
      else
	expose_window_clip_vector (tv_cur_overlay_window (c->info),
				   tv_cur_overlay_clipvec (c->info));

      if (!select_screen (c))
	goto finish; /* XXX */

      /* Desired overlay bounds */

      c->overlay_rect.x	= c->mw_x + c->vw_x;
      c->overlay_rect.y	= c->mw_y + c->vw_y;
      c->overlay_rect.width = c->vw_width;
      c->overlay_rect.height = c->vw_height;

      if (NULL == c->screen)
	{
	  /* video_window is outside all screens. */
	  goto finish;
	}

      retry_count = 5;

      for (;;)
	{
	  tv_window swin;
	  const tv_screen *s;
	  const tv_window *w;
	  unsigned int old_size;

	  if (0 == retry_count--)
	    goto finish; /* XXX */

	  s = c->screen;

	  swin = c->overlay_rect;
	  swin.x -= s->x;
	  swin.y -= s->y;

	  w = tv_set_overlay_window_clipvec (c->info, &swin, &c->cur_vector);
	  if (NULL == w)
	    goto finish; /* XXX */

	  if (tv_window_equal (&swin, w))
	    break;

	  /* The driver modified the overlay bounds (alignment, limits),
	     we must update the clips. */

	  old_size = c->cur_vector.size;

	  c->overlay_rect = *w;
	  c->overlay_rect.x += s->x;
	  c->overlay_rect.y += s->y;

	  /* XXX error ignored */
	  get_clips (c, &c->cur_vector);

	  if (0 == (old_size | c->cur_vector.size))
	    break;
	}
    }
  else if (c->clean_screen)
    {
      expose_screen (c);
    }

  /* XXX error ignored */
  tv_enable_overlay (c->info, TRUE);

 finish:
  c->timeout_id = NO_SOURCE_ID;

  return FALSE; /* remove timeout */
}

static void
stop_timeout			(struct context *	c)
{
  if (c->timeout_id > 0)
    g_source_remove (c->timeout_id);

  c->timeout_id = NO_SOURCE_ID;
}

static void
restart_timeout			(struct context *	c)
{
  g_assert (CLIP_OVERLAY == c->overlay_mode);

  if (OVERLAY_LOG_FP)
    fprintf (OVERLAY_LOG_FP, "restart_timeout\n");

  tv_enable_overlay (c->info, FALSE);

  stop_timeout (c);

  if (GDK_VISIBILITY_FULLY_OBSCURED == c->visibility)
    {
      c->timeout_id = g_timeout_add (CLEAR_TIMEOUT,
				     (GSourceFunc) obscured_timeout,
				     c);
    }
  else
    {
      c->timeout_id = g_timeout_add (CLEAR_TIMEOUT,
				     (GSourceFunc) visible_timeout,
				     c);
    }
}

static unsigned int
chroma_key_rgb			(GdkColor *		color)
{
  return ((color->red >> 8) |
	  (color->green & 0xFF00) |
	  ((color->blue & 0xFF00) << 8));
}

static gboolean
reconfigure			(struct context *	c)
{
  switch (c->overlay_mode)
    {
      unsigned int rgb;
      tv_window swin;

    case CLIP_OVERLAY:
      c->geometry_changed = TRUE;
      c->clean_screen = TRUE; /* see root_filter() */

      if (GDK_VISIBILITY_FULLY_OBSCURED != c->visibility)
	restart_timeout (c);

      break;

    case CHROMA_KEY_OVERLAY:
      /* Implied by tv_set_overlay_window_chromakey():
	 tv_enable_overlay (c->info, FALSE); */

      /* Restore background color (chroma key). XXX the X server
	 should do this automatically, did we we disabled that
	 somewhere to avoid flicker in capture overlay_mode? */
      gdk_window_clear_area (c->video_window->window,
			     /* x, y */ 0, 0,
			     c->vw_width,
			     c->vw_height);

      if (!select_screen (c))
	return FALSE;

      c->overlay_rect.x = c->mw_x + c->vw_x;
      c->overlay_rect.y = c->mw_y + c->vw_y;
      c->overlay_rect.width = c->vw_width;
      c->overlay_rect.height = c->vw_height;

      if (0)
	fprintf (stderr, "reconfigure overlay_rect: %ux%u%+d%+d\n",
		 c->overlay_rect.width,
		 c->overlay_rect.height,
		 c->overlay_rect.x,
		 c->overlay_rect.y);

      if (NULL == c->screen)
	{
	  /* video_window is outside all screens. */

	  tv_enable_overlay (c->info, FALSE);

	  return TRUE;
	}

      c->visibility = GDK_VISIBILITY_PARTIAL; /* or full */

      if (OVERLAY_CHROMA_TEST)
	{
	  /* Show me the chroma key color. */
	  c->overlay_rect.width /= 2;
	}

      swin = c->overlay_rect;
      swin.x -= c->screen->x;
      swin.y -= c->screen->y;

      rgb = chroma_key_rgb (&c->chroma_key_color);

      if (NULL == tv_set_overlay_window_chromakey (c->info, &swin, rgb))
	return FALSE;

      if (!tv_enable_overlay (c->info, TRUE))
	return FALSE;

      break;

    case XVIDEO_OVERLAY:
      /* XVideo overlay is automatically positioned relative to
	 the video_window origin, we only have to adjust the
	 overlay size. */

      tv_enable_overlay (c->info, FALSE);

      if (c->colormap)
	{
	  /* Restore background color (chroma key). */
	  gdk_window_clear_area (c->video_window->window,
				 /* x, y */ 0, 0,
				 c->vw_width,
				 c->vw_height);
	}

      /* XXX set overlay rectangle here, currently the XVideo
	 interface fills the entire window when overlay is enabled. */

      /* XXX off/on is inefficient, XVideo drivers
	 do that automatically. */
      if (!tv_enable_overlay (c->info, TRUE))
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
  struct context *c = user_data;

  g_assert (widget == c->video_window);

  if (OVERLAY_EVENT_LOG_FP)
    fprintf (OVERLAY_EVENT_LOG_FP,
	     "on_video_window_event: GDK_%s\n",
	     z_gdk_event_name (event));

  switch (event->type)
    {
    case GDK_CONFIGURE:
      /* Size, position, stacking order changed. Note position
         is relative to the parent window. */

      if (c->vw_width != (guint) event->configure.width
	  || c->vw_height != (guint) event->configure.height
	  || c->vw_x != (gint) event->configure.x
	  || c->vw_y != (gint) event->configure.y)
	{
	  /* XXX really mw relative? */
	  c->vw_x = event->configure.x; 
	  c->vw_y = event->configure.y;
	      
	  c->vw_width = event->configure.width;
	  c->vw_height = event->configure.height;

	  /* XXX error ignored. */
	  reconfigure (c);
	}

      break;

    case GDK_VISIBILITY_NOTIFY:
      /* Visibility state changed: obscured, partially or fully visible. */

      if (CLIP_OVERLAY == c->overlay_mode)
	{
	  c->visibility = event->visibility.state;
	  restart_timeout (c);
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

      if (CLIP_OVERLAY == c->overlay_mode)
	{
	  c->geometry_changed = TRUE;
	  restart_timeout (c);
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
  struct context *c = user_data;

  g_assert (widget == c->main_window);

  if (OVERLAY_EVENT_LOG_FP)
    fprintf (OVERLAY_EVENT_LOG_FP,
	     "on_main_window_event: GDK_%s\n",
	     z_gdk_event_name (event));

  switch (event->type)
    {
    case GDK_CONFIGURE:
      /* Size, position, stacking order changed. Note position
         is relative to the parent window. */

      switch (c->overlay_mode)
	{
	case CLIP_OVERLAY:
	case CHROMA_KEY_OVERLAY:
	  if (c->mw_x != event->configure.x
	      || c->mw_y != event->configure.y)
	    {
	      /* XXX really root relative? */
	      c->mw_x = event->configure.x;
	      c->mw_y = event->configure.y;

	      /* XXX error ignored. */
	      reconfigure (c);
	    }

	  break;

	case XVIDEO_OVERLAY:
	  g_assert_not_reached ();
	}

      break;

    case GDK_UNMAP:
      /* Window was rolled up or minimized. No
	 visibility events are sent in this case. */

      if (CLIP_OVERLAY == c->overlay_mode)
	{
	  c->visibility = GDK_VISIBILITY_FULLY_OBSCURED;
	  restart_timeout (c);
	}

      break;

    default:
      break;
    }

  return FALSE; /* pass on */
}

static GdkFilterReturn
root_filter			(GdkXEvent *		gdkxevent,
				 GdkEvent *		gdkevent,
				 gpointer		user_data)
{
  struct context *c = user_data;
  XEvent *event = (XEvent *) gdkxevent;

  gdkevent = gdkevent;

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

  if (c->visibility != GDK_VISIBILITY_PARTIAL)
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
	    c->clean_screen = TRUE;
	  }
	else
	  {
	    XConfigureEvent *ev;
	    const tv_window *win;
	    int evx2, evy2;
	    int wx2, wy2;

	    ev = &event->xconfigure;

	    win = tv_cur_overlay_window (c->info);

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

	restart_timeout (c);

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
				 gpointer		user_data)
{
  struct context *c = user_data;

  osd_model = osd_model;

  if (CLIP_OVERLAY == c->overlay_mode
      && GDK_VISIBILITY_FULLY_OBSCURED != c->visibility)
    {
      c->visibility = GDK_VISIBILITY_PARTIAL;
      restart_timeout (c);
    }
}

static void
terminate			(struct context *	c)
{
  stop_timeout (c);

  tv_enable_overlay (c->info, FALSE);

  if (CLIP_OVERLAY == c->overlay_mode)
    {
      usleep (CLEAR_TIMEOUT * 1000);

      if (c->clean_screen)
	expose_screen (c);
      else
	expose_window_clip_vector (tv_cur_overlay_window (c->info),
				   tv_cur_overlay_clipvec (c->info));
    }
}

static gboolean
on_main_window_delete_event	(GtkWidget *		widget,
				 GdkEvent *		event,
				 gpointer		user_data)
{
  struct context *c = user_data;

  widget = widget;
  event = event;

  terminate (c);

  return FALSE; /* pass on */
}

typedef gboolean
event_function			(GtkWidget *		widget,
				 GdkEvent  *		event,
				 gpointer		user_data);

static void
event_signal_disconnect		(struct context *	c,
				 GtkWidget *		widget,
				 event_function *	callback,
				 GdkEventMask		old_events)
{
  if (NULL != callback)
    g_signal_handlers_disconnect_matched (G_OBJECT (widget),
					  (G_SIGNAL_MATCH_FUNC |
					   G_SIGNAL_MATCH_DATA),
					  /* signal_id */ 0,
					  /* detail */ 0,
					  /* closure */ NULL,
					  G_CALLBACK (callback),
					  /* user_data */ c);

  gdk_window_set_events (widget->window, old_events);
}

void
stop_overlay			(void)
{
  struct context *c = &tv_info;

  g_assert (c->main_window != NULL);

  g_signal_handlers_disconnect_matched
	  (G_OBJECT (zapping),
	   (G_SIGNAL_MATCH_FUNC |
	    G_SIGNAL_MATCH_DATA),
	   /* signal_id */ 0,
	   /* detail */ 0,
	   /* closure */ NULL,
	   G_CALLBACK (on_main_window_delete_event),
	   /* user_data */ c);

  switch (c->overlay_mode)
    {
    case CLIP_OVERLAY:
      gdk_window_set_events (c->root_window,
			     c->old_root_events);

      gdk_window_remove_filter (c->root_window,
				root_filter,
				/* user_data */ c);

      g_signal_handlers_disconnect_matched
	(G_OBJECT (osd_model),
	 (G_SIGNAL_MATCH_FUNC |
	  G_SIGNAL_MATCH_DATA),
	 /* signal_id */ 0,
	 /* detail */ 0,
	 /* closure */ NULL,
	 G_CALLBACK (on_osd_model_changed),
	 /* user_data */ c);

      event_signal_disconnect (c, c->main_window,
			       on_main_window_event,
			       c->old_main_events);

      event_signal_disconnect (c, c->video_window,
			       on_video_window_event,
			       c->old_video_events);

      break;

    case CHROMA_KEY_OVERLAY:
      event_signal_disconnect (c, c->main_window,
			       on_main_window_event,
			       c->old_main_events);

      /* fall through */

    case XVIDEO_OVERLAY:
      event_signal_disconnect (c, c->video_window,
			       on_video_window_event,
			       c->old_video_events);

      if (0 != c->chroma_key_color_cnxn_id)
	z_gconf_notify_remove (c->chroma_key_color_cnxn_id);

      break;
    }

  terminate (c);

  if (c->colormap)
    {
      gdk_colormap_free_colors (c->colormap,
				&c->chroma_key_color, 1);

      g_object_unref (G_OBJECT (c->colormap));
      c->colormap = NULL;

      z_set_window_bg_black (c->video_window);
    }

  tv_clip_vector_destroy (&c->tmp_vector);
  tv_clip_vector_destroy (&c->cur_vector);

  tv_set_capture_mode (c->info, CAPTURE_MODE_NONE);

  /* XXX no const limit please */
  z_video_set_max_size (Z_VIDEO (c->video_window), 16384, 16384);

  CLEAR (*c);
}

static gboolean
chroma_key_color_from_config	(GdkColor *		color)
{
  /* Factory default if the config is inaccessible
     or the string is malformed. */
  color->pixel = 0;
  color->red = 0xFFFF;
  color->green = 0xCCCC;
  color->blue = 0xCCCC;

  /* XXX error message please. */
  return z_gconf_get_color (color, "/apps/zapping/window/chroma_key_color");
}

static GdkColor *
set_window_bg_black		(struct context *	c)
{
  z_set_window_bg_black (c->video_window);

  CLEAR (c->chroma_key_color);

  return &c->chroma_key_color;
}

static GdkColor *
set_window_bg_from_config	(struct context *	c)
{
  GdkColor color;

  if (!DEVICE_SUPPORTS_CHROMA_KEYING (c))
    return set_window_bg_black (c);

  if (c->colormap)
    {
      gdk_colormap_free_colors (c->colormap,
				&c->chroma_key_color, 1);
    }
  else
    {
      c->colormap = gdk_colormap_get_system ();
    }

  /* Error ignored, will continue with default. */
  chroma_key_color_from_config (&color);
  c->chroma_key_color = color;

  if (!OVERLAY_COLORMAP_FAILURE_TEST
      && gdk_colormap_alloc_color (c->colormap,
				   &c->chroma_key_color,
				   /* writable */ FALSE,
				   /* or_best_match */ TRUE))
    {
      /* Note gdk_colormap_alloc_color() may change c->chroma_key_color. */

      z_set_window_bg (c->video_window, &c->chroma_key_color);

      return &c->chroma_key_color;
    }
  else
    {
      g_object_unref (G_OBJECT (c->colormap));
      c->colormap = NULL;

      z_show_non_modal_message_dialog
	(GTK_WINDOW (zapping), GTK_MESSAGE_ERROR,
	 _("No chroma-key color"),
	 _("Color #%2X%2X%2X is not available for chroma-keying. "
	   "Try another color.\n"),
	 color.red >> 8,
	 color.green >> 8,
	 color.blue >> 8);

      return set_window_bg_black (c);
    }
}

static void
chroma_key_color_changed	(GConfClient *		client,
				 guint			cnxn_id,
				 GConfEntry *		entry,
				 gpointer		user_data)
{
  struct context *c = user_data;

  client = client;
  entry = entry;

  c->chroma_key_color_cnxn_id = cnxn_id;

  set_window_bg_from_config (c);
}

static GdkColor *
watch_config_chroma_key_color	(struct context *	c)
{
  if (!DEVICE_SUPPORTS_CHROMA_KEYING (c))
    return set_window_bg_black (c);

  /* Calls chroma_key_color_notify on success. */
  if (z_gconf_notify_add ("/apps/zapping/window/chroma_key_color",
			  chroma_key_color_changed,
			  /* user_data */ c))
    return &c->chroma_key_color;

  /* Too bad. Let's the window background once and continue. */
  return set_window_bg_from_config (c);
}

static GdkEventMask
event_signal_connect		(struct context *	c,
				 GtkWidget *		widget,
				 event_function *	callback,
				 GdkEventMask		new_events)
{
  GdkEventMask old_events;

  if (NULL != callback)
    g_signal_connect (G_OBJECT (widget), "event",
		      G_CALLBACK (callback),
		      /* user_data */ c);

  old_events = gdk_window_get_events (widget->window);
  gdk_window_set_events (widget->window, old_events | new_events);

  return old_events;
}

gboolean
start_overlay			(GtkWidget *		main_window,
				 GtkWidget *		video_window)
{
  struct context *c = &tv_info;
  const struct tveng_caps *caps;
  Window xwindow;
  gint width;
  gint height;

  CLEAR (*c);

  c->info = zapping->info;

  c->root_window = gdk_get_default_root_window ();

  c->main_window = main_window;
  c->video_window = video_window;

  gdk_window_get_origin (c->main_window->window,
			 &c->mw_x, &c->mw_y);

  gdk_window_get_geometry (c->video_window->window,
			   &c->vw_x, &c->vw_y,
			   &width, &height,
			   /* depth */ NULL);
  c->vw_width = width;
  c->vw_height = height;

  c->visibility = GDK_VISIBILITY_PARTIAL; /* assume worst */

  c->geometry_changed = TRUE; /* restart overlay */

  c->timeout_id = NO_SOURCE_ID;

  /* Make sure we use an XVideo adaptor which can render into
     da->window. (Doesn't matter with X.org but it's the right thing
     to do.) */
  xwindow = GDK_WINDOW_XWINDOW (c->video_window->window);

  /* Switch to overlay mode, XVideo or other. */
  if (-1 == tveng_attach_device (zcg_char (NULL, "video_device"),
				 xwindow, TVENG_ATTACH_XV, c->info))
    {
      z_show_non_modal_message_dialog
	(GTK_WINDOW (zapping), GTK_MESSAGE_ERROR,
	 _("Cannot switch to overlay mode"),
	 "%s", tv_get_errstr (c->info));

      goto failure;
    }

  /* Switch to selected video input, RF channel on new device. */
  zconf_get_sources (c->info, /* mute */ FALSE);

  if (OVERLAY_CHROMA_TEST)
    {
      c->overlay_mode = CHROMA_KEY_OVERLAY;
    }
  else if (DEVICE_USES_XVIDEO (c))
    {
      c->overlay_mode = XVIDEO_OVERLAY;
    }
  else if (DEVICE_SUPPORTS_CHROMA_KEYING (c))
    {
      c->overlay_mode = CHROMA_KEY_OVERLAY;
    }
  else if (DEVICE_SUPPORTS_CLIPPING (c))
    {
      c->overlay_mode = CLIP_OVERLAY;
    }
  else
    {
      z_show_non_modal_message_dialog
	(GTK_WINDOW (zapping), GTK_MESSAGE_ERROR,
	 _("Cannot overlay with this device"),
	 "Device does not support clipping or chroma-keying.");

      /* XXX Perhaps the driver supports another kind of overlay? */

      goto failure;
    }

  switch (c->overlay_mode)
    {
      GC xgc;
      GdkColor *color;
      unsigned int rgb;

    case XVIDEO_OVERLAY:
      /* XXX no const limit please, ask the driver instead. */
      z_video_set_max_size (Z_VIDEO (video_window), 768, 576);

      xwindow = GDK_WINDOW_XWINDOW (c->video_window->window);
      xgc = GDK_GC_XGC (c->video_window->style->white_gc);

      color = watch_config_chroma_key_color (c);

      rgb = chroma_key_rgb (color);

      if (!tv_set_overlay_xwindow (c->info, xwindow, xgc, rgb))
	goto failure;

      /* Disable double buffering just in case, will help when a
	 XV driver doesn't provide XV_COLORKEY but requires the colorkey
	 not to be overwritten */
      gtk_widget_set_double_buffered (c->video_window, FALSE);

      /* Update on video_window size change. */
      c->old_video_events =
	event_signal_connect (c, c->video_window,
			      on_video_window_event,
			      GDK_CONFIGURE);

      /* Start overlay, XXX error ignored. */
      reconfigure (c);
    
      break;

    case CHROMA_KEY_OVERLAY:
      if (!select_screen (c))
	goto failure;

      /* After select_screen() because the limits may depend on
	 the overlay buffer size. FIXME they may also depend on
	 the (then) current video standard. */
      caps = tv_get_caps (c->info);
      z_video_set_max_size (Z_VIDEO (video_window),
			    caps->maxwidth,
			    caps->maxheight);

      watch_config_chroma_key_color (c);

      /* Update overlay on video_window size and position change. We
	 must connect to main_window as well because the video_window
	 GDK_CONFIGURE event notifies only about main_window, not
	 root_window relative position changes. */

      c->old_video_events =
	event_signal_connect (c, c->video_window,
			      on_video_window_event,
			      GDK_CONFIGURE);

      c->old_main_events =
	event_signal_connect (c, c->main_window,
			      on_main_window_event,
			      GDK_CONFIGURE);

      /* Start overlay, XXX error ignored. */
      reconfigure (c);

      break;

    case CLIP_OVERLAY:
      if (!select_screen (c))
	goto failure;

      caps = tv_get_caps (c->info);
      z_video_set_max_size (Z_VIDEO (video_window),
			    caps->maxwidth,
			    caps->maxheight);

      g_signal_connect (G_OBJECT (osd_model), "changed",
			G_CALLBACK (on_osd_model_changed),
			/* user_data */ c);

      c->old_video_events =
	event_signal_connect (c, c->video_window,
			      on_video_window_event,
			      (GDK_VISIBILITY_NOTIFY_MASK |
			       GDK_CONFIGURE |
			       GDK_EXPOSE));

      /* We must connect to main_window as well because the
	 video_window GDK_CONFIGURE event notifies only about
	 main_window, not root_window relative position
	 changes. GDK_UNMAP, but no visibility event, is sent
	 after the window was rolled up or minimized. */

      c->old_main_events =
	event_signal_connect (c, c->main_window,
			      on_main_window_event,
			      (GDK_UNMAP |
			       GDK_CONFIGURE));

      /* There is no GDK_OBSCURED event, we must monitor all child
	 windows of the root window. E.g. drop-down menus. */

      gdk_window_add_filter (c->root_window,
			     root_filter,
			     /* user_data */ c);

      c->old_root_events = gdk_window_get_events (c->root_window);
      gdk_window_set_events (c->root_window,
			     (c->old_root_events |
			      GDK_STRUCTURE_MASK |
			      GDK_SUBSTRUCTURE_MASK));

      /* Start overlay. */

      restart_timeout (c);

      break;
    }

  g_signal_connect (G_OBJECT (zapping), "delete-event",
		    G_CALLBACK (on_main_window_delete_event),
		    /* user_data */ c);

  zapping->display_mode = DISPLAY_MODE_WINDOW;
  zapping->display_window = video_window;
  tv_set_capture_mode (c->info, CAPTURE_MODE_OVERLAY);

  return TRUE;

 failure:
  return FALSE;
}

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
