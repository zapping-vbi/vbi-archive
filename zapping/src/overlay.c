/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 Iñaki García Etxebarria
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
 * These routines handle the Windowed overlay mode.
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <tveng.h>
#include "x11stuff.h"
#include "zmisc.h"
#include "zvbi.h"
#include "overlay.h"

#define CHECK_TIMEOUT 100 /* ms for the overlay check timeout */
#define CLEAR_TIMEOUT 50 /* ms for the clear window timeout */

/* event names for debugging. event_str = events[event->type+1] */
#if 0 /* just for debugging */
static gchar * events[] =
{
  "GDK_NOTHING",
  "GDK_DELETE",
  "GDK_DESTROY",
  "GDK_EXPOSE",
  "GDK_MOTION_NOTIFY",
  "GDK_BUTTON_PRESS",
  "GDK_2BUTTON_PRESS",
  "GDK_3BUTTON_PRESS",
  "GDK_BUTTON_RELEASE",
  "GDK_KEY_PRESS",
  "GDK_KEY_RELEASE",
  "GDK_ENTER_NOTIFY",
  "GDK_LEAVE_NOTIFY",
  "GDK_FOCUS_CHANGE",
  "GDK_CONFIGURE",
  "GDK_MAP",
  "GDK_UNMAP",
  "GDK_PROPERTY_NOTIFY",
  "GDK_SELECTION_CLEAR",
  "GDK_SELECTION_REQUEST",
  "GDK_SELECTION_NOTIFY",
  "GDK_PROXIMITY_IN",
  "GDK_PROXIMITY_OUT",
  "GDK_DRAG_ENTER",
  "GDK_DRAG_LEAVE",
  "GDK_DRAG_MOTION",
  "GDK_DRAG_STATUS",
  "GDK_DROP_START",
  "GDK_DROP_FINISHED",
  "GDK_CLIENT_EVENT",
  "GDK_VISIBILITY_NOTIFY",
  "GDK_NO_EXPOSE"
};
#endif

/* Saves some info about the tv_screen */
static struct {
  gint x, y, w, h; /* geometry */
  gboolean visible; /* If it is visible */
  gboolean use_xv; /* TRUE if we should use Xv instead of V4L */
  gboolean ignore_expose; /* whether we should ignore the next expose
			     event */
  GtkWidget * window; /* The window we will be overlaying to */
  GtkWidget * main_window; /* The toplevel window .window is in */
  tveng_device_info * info; /* The overlaying V4L device */
  struct tveng_clip * clips; /* Clip rectangles */
  gint clipcount; /* Number of clips in clips */
  gboolean clean_screen; /* TRUE if the whole screen will be cleared */
  gint clear_timeout_id; /* timeout for redrawing */
  gint check_timeout_id; /* timout for checking */
} tv_info;

/*
 * Just like x11_get_clips, but fixes the dword-align ugliness
 */
static struct tveng_clip *
overlay_get_clips(GdkWindow * window, gint * clipcount)
{
  struct tveng_clip * clips;

  g_assert(window != NULL);
  g_assert(clipcount != NULL);

  clips = x11_get_clips(window,
			tv_info.info->window.x,
			tv_info.info->window.y,
			tv_info.info->window.width,
			tv_info.info->window.height,
			clipcount);

  return clips;
}

/*
 * Checks whether two clipping structs look the same.
 */
static gboolean
compare_clips(struct tveng_clip *a, gint na, struct tveng_clip* b,
	      gint nb)
{
  gint i;

  if (na != nb)
    return FALSE;

  for (i = 0; i < na; i++)
    if ((a[i].x != b[i].x) || (a[i].y != b[i].y) ||
	(a[i].width != b[i].width) || (a[i].height != b[i].height))
      return FALSE;

  return TRUE;
}

/*
 * Clearing needed callback
 */
static gint
overlay_clearing_timeout(gpointer data)
{
  struct tveng_clip * clips;
  gint clipcount;

  clips = overlay_get_clips(tv_info.window->window, &clipcount);

  if (!compare_clips(clips, clipcount, tv_info.clips,
		     tv_info.clipcount))
    { /* delay the update till the situation stabilizes */
      if (tv_info.clips)
	free(tv_info.clips);

      tv_info.clips = clips;
      tv_info.clipcount = clipcount;
      tv_info.clean_screen = TRUE;
	
      return TRUE; /* call me again */
    }

  if (clips)
    free(clips);

  if ((tv_info.info->current_mode == TVENG_CAPTURE_WINDOW) &&
      (tv_info.visible))
    {
      if (tv_info.clean_screen)
	x11_force_expose(0, 0, gdk_screen_width(), gdk_screen_height());
      else
	x11_force_expose(tv_info.info->window.x-40,
			 tv_info.info->window.y-40,
			 tv_info.info->window.width+80,
			 tv_info.info->window.height+80);

      tv_info.ignore_expose = TRUE;
      tv_info.clean_screen = FALSE;
    }

  /* Setup the overlay and start it again if needed */
  if (tv_info.info->current_mode == TVENG_CAPTURE_WINDOW)
    {
      overlay_sync(FALSE);
      if (tv_info.visible)
	tveng_set_preview_on(tv_info.info);
      else
	tveng_set_preview_off(tv_info.info);
    }

  *((gint*)data) = -1; /* set timeout_id to destroyed */

  return FALSE; /* the timeout will be destroyed */
}

/*
 * Something has changed, schedule a redraw in TIMEOUT ms
 * clean_screen: the whole screen needs updating
 */
static void
overlay_status_changed(gboolean clean_screen)
{
  if (tv_info.clear_timeout_id >= 0)
    gtk_timeout_remove(tv_info.clear_timeout_id);

  tv_info.clear_timeout_id =
    gtk_timeout_add(CLEAR_TIMEOUT, overlay_clearing_timeout,
		    &(tv_info.clear_timeout_id));

  if (tv_info.info->current_mode == TVENG_CAPTURE_WINDOW)
    tveng_set_preview_off(tv_info.info);

  if (!tv_info.clean_screen)
    tv_info.clean_screen = clean_screen;
}

/*
 * Timeout callback. Check for changes in the window stacking and
 * schedules overlay_clearing_timeout if needed. The rest of the
 * changes that could affect the overlay window are event-handled.
 */
static gint
overlay_periodic_timeout(gpointer data)
{
  struct tveng_clip * clips;
  gint clipcount;
  gboolean do_clean = FALSE;

  clips = overlay_get_clips(tv_info.window->window, &clipcount);

  do_clean = !compare_clips(clips, clipcount, tv_info.clips,
			    tv_info.clipcount);

  if (tv_info.clips)
    free(tv_info.clips);

  tv_info.clips = clips;
  tv_info.clipcount = clipcount;

  /* Re-schedule a cleaning CLEAR_TIMEOUT from now */
  if (do_clean)
    overlay_status_changed(FALSE);

  return TRUE; /* keep calling me */
}

/*
 * event handler for the client overlay window
 */
static gboolean
on_overlay_event             (GtkWidget       *widget,
			      GdkEvent        *event,
			      gpointer         user_data)
{
  switch (event->type)
    {
    case GDK_EXPOSE:
      /* fixme: The VBI engine needn't be in the same widget as the
       overlay */
      zvbi_exposed(widget, event->expose.area.x, event->expose.area.y,
		   event->expose.area.width,
		   event->expose.area.height);
      break;
    default:
      break;
    }

  return FALSE;
}

/*
 * event handler for the toplevel overlay window
 */
static gboolean
on_main_overlay_event        (GtkWidget       *widget,
			      GdkEvent        *event,
			      gpointer         user_data)
{
  switch (event->type)
    {
    case GDK_UNMAP:
      tv_info.visible = FALSE;
      overlay_sync(TRUE);
      overlay_status_changed(FALSE);
      break;
    case GDK_MAP:
      tv_info.visible = TRUE;
      overlay_status_changed(FALSE);
      break;
    case GDK_CONFIGURE:
      overlay_status_changed(TRUE);
      break;
    default:
      break;
    }

  return FALSE;
}

/*
 * Called when the main overlay window is destroyed (shuts down the
 * timers)
 */
static gboolean
on_main_overlay_delete_event           (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  if (tv_info.clear_timeout_id)
    {
      gtk_timeout_remove(tv_info.clear_timeout_id);
      tv_info.clear_timeout_id = -1;
    }

  if (tv_info.check_timeout_id)
    {
      gtk_timeout_remove(tv_info.check_timeout_id);
      tv_info.check_timeout_id = -1;
    }

  return FALSE; /* go on with the callbacks */
}

/*
 * Inits the overlay engine. Inits some stuff, and returns FALSE on
 * error
 * use_xv: TRUE if overlay mode should use the Xv extension instead of
 * the V4L interface.
 * window: The window we will be overlaying to.
 * main_window: The toplevel window window belongs to. They can be the
 * same.
 */
gboolean
startup_overlay(gboolean use_xv, GtkWidget * window, GtkWidget *
		main_window, tveng_device_info * info)
{
  GdkEventMask mask; /* The GDK events we want to see */

  gtk_signal_connect(GTK_OBJECT(window), "event",
		     GTK_SIGNAL_FUNC(on_overlay_event),
		     NULL);

  gtk_signal_connect(GTK_OBJECT(main_window), "event",
		     GTK_SIGNAL_FUNC(on_main_overlay_event),
		     NULL);

  gtk_signal_connect(GTK_OBJECT(main_window), "delete-event",
		     GTK_SIGNAL_FUNC(on_main_overlay_delete_event),
		     NULL);

  /*
    gdk has no [Sub]structureNotify wrapper, but a timer will
    do nicely.
  */
  mask = gdk_window_get_events(window->window);
  mask |= (GDK_EXPOSURE_MASK | GDK_VISIBILITY_NOTIFY_MASK);
  gdk_window_set_events(window->window, mask);

  mask = gdk_window_get_events(main_window->window);
  mask |= (GDK_EXPOSURE_MASK | GDK_VISIBILITY_NOTIFY_MASK);
  gdk_window_set_events(main_window->window, mask);

  tv_info.window = window;
  tv_info.main_window = main_window;
  tv_info.use_xv = use_xv;
  tv_info.info = info;
  tv_info.visible = x11_window_viewable(window->window);

  tv_info.ignore_expose = FALSE;
  tv_info.clear_timeout_id = -1;
  tv_info.check_timeout_id =
    gtk_timeout_add(CHECK_TIMEOUT, overlay_periodic_timeout, NULL);
  tv_info.clean_screen = FALSE;
  tv_info.clips = NULL;
  tv_info.clipcount = 0;
  
  gdk_window_get_size(window->window, &tv_info.w, &tv_info.h);
  gdk_window_get_origin(window->window, &tv_info.x, &tv_info.y);

  return TRUE;
}

/*
 * Shuts down the overlay engine
 * do_cleanup: TRUE if the screen is possibly corrupted
 */
void
shutdown_overlay(gboolean do_cleanup)
{
  if (do_cleanup)
    x11_force_expose(0, 0, gdk_screen_width(), gdk_screen_height());
}

/*
 * Tells the overlay engine to sync the overlay with the window.
 * clean_screen: TRUE means that we should refresh the whole screeen.
 */
void
overlay_sync(gboolean clean_screen)
{
  gdk_window_get_size(tv_info.window->window, &tv_info.w, &tv_info.h);
  gdk_window_get_origin(tv_info.window->window, &tv_info.x,
			&tv_info.y);

  if (tv_info.clips)
    free(tv_info.clips);

  tv_info.info->window.x = tv_info.x;
  tv_info.info->window.y = tv_info.y;
  tv_info.info->window.width = tv_info.w;
  tv_info.info->window.height = tv_info.h;
  tv_info.info->window.clips = tv_info.clips =
    overlay_get_clips(tv_info.window->window,
		      &(tv_info.info->window.clipcount));
  tv_info.clipcount = tv_info.info->window.clipcount;

  tveng_set_preview_window(tv_info.info);

  if (tv_info.clips)
    free(tv_info.clips);

  /* The requested overlay coords might not be the definitive ones,
     adapt the clips */
  tv_info.clips = tv_info.info->window.clips =
    overlay_get_clips(tv_info.window->window, &(tv_info.clipcount));
  tv_info.info->window.clipcount = tv_info.clipcount;

  tveng_set_preview_window(tv_info.info);

  if (clean_screen)
    {
      x11_force_expose(0, 0, gdk_screen_width(), gdk_screen_height());
      tv_info.ignore_expose = TRUE;
    }
}
