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

/*
 * These routines handle the Windowed overlay mode.
 * FIXME: This is WAY too HACKISH!
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <tveng.h>

#ifndef DISABLE_X_EXTENSIONS
#ifdef HAVE_LIBXV
#define USE_XV 1
#endif
#endif

#ifdef USE_XV
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#endif /* USE_XV */

#include "x11stuff.h"
#include "zmisc.h"
#include "zvbi.h"
#include "overlay.h"
#include "osd.h"

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
  GtkWidget * window; /* The window we will be overlaying to */
  GtkWidget * main_window; /* The toplevel window .window is in */
  tveng_device_info * info; /* The overlaying V4L device */
  struct tveng_clip * clips; /* Clip rectangles */
  gint clipcount; /* Number of clips in clips */
  gboolean clean_screen; /* TRUE if the whole screen will be cleared */
  gint clear_timeout_id; /* timeout for redrawing */
  gint check_timeout_id; /* timout for checking */
  gboolean needs_cleaning; /* FALSE for XVideo and chromakey */
} tv_info;

static int malloc_count = 0;

/*
 * Just like x11_get_clips, but fixes the dword-align ugliness
 */
static struct tveng_clip *
overlay_get_clips(GdkWindow * window, gint * clipcount)
{
  struct tveng_clip * clips;

  g_assert(clipcount != NULL);

  if (!window || !tv_info.needs_cleaning)
    {
      *clipcount = 0;
      return NULL;
    }

  clips = x11_get_clips(window,
			tv_info.info->window.x,
			tv_info.info->window.y,
			tv_info.info->window.width,
			tv_info.info->window.height,
			clipcount);

  if (clips)
    malloc_count ++;

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
	{
	  g_free(tv_info.clips);
	  malloc_count --;
	}

      tv_info.clips = clips;
      tv_info.clipcount = clipcount;
      tv_info.clean_screen = TRUE;
	
      return TRUE; /* call me again */
    }

  if (clips) {
    malloc_count --;
    g_free(clips);
  }

  if ((tv_info.info->current_mode == TVENG_CAPTURE_WINDOW) &&
      (tv_info.needs_cleaning) && (tv_info.visible))
    {
      if (tv_info.clean_screen)
	x11_force_expose(0, 0, gdk_screen_width(), gdk_screen_height());
      else
	x11_force_expose(tv_info.info->window.x-40,
			 tv_info.info->window.y-40,
			 tv_info.info->window.width+80,
			 tv_info.info->window.height+80);

      tv_info.clean_screen = FALSE;
    }

  /* Setup the overlay and start it again if needed */
  if (tv_info.info->current_mode == TVENG_CAPTURE_WINDOW)
    {
      overlay_sync(FALSE);
      if (tv_info.needs_cleaning)
	{
	  if (tv_info.visible)
	    tveng_set_preview_on(tv_info.info);
	  else
	    tveng_set_preview_off(tv_info.info);
	}
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

  if ((tv_info.info->current_mode == TVENG_CAPTURE_WINDOW) &&
      (tv_info.needs_cleaning))
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

  if (tv_info.clips) {
    malloc_count --;
    g_free(tv_info.clips);
  }

  tv_info.clips = clips;
  tv_info.clipcount = clipcount;

  /* Re-schedule a cleaning CLEAR_TIMEOUT from now */
  if (do_clean)
    overlay_status_changed(FALSE);

  return TRUE; /* keep calling me */
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
      overlay_status_changed(TRUE);
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
 * Called when the tv_screen geometry changes
 */
static void
on_tv_screen_size_allocate             (GtkWidget       *widget,
                                        GtkAllocation   *allocation,
					gpointer	ignored)
{
  static gint old_w=-1, old_h;

  /**
   * GtkFixed sends allocation events when removing
   * children from it, even when no resize is involved. Ignore these.
   */
  if (old_w == allocation->width &&
      old_h == allocation->height)
    return;

  old_w = allocation->width;
  old_h = allocation->height;

  overlay_status_changed(FALSE);
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
  if (tv_info.clear_timeout_id >= 0)
    {
      gtk_timeout_remove(tv_info.clear_timeout_id);
      tv_info.clear_timeout_id = -1;
    }

  if (tv_info.check_timeout_id >= 0)
    {
      gtk_timeout_remove(tv_info.check_timeout_id);
      tv_info.check_timeout_id = -1;
    }

  return FALSE; /* go on with the callbacks */
}

/*
 * The osd pieces have changed, avoid flicker if not needed.
 */
static void
on_osd_model_changed			(ZModel		*osd_model,
					 gpointer	ignored)
{
  struct tveng_window window;

  if (tv_info.clear_timeout_id >= 0 || !tv_info.needs_cleaning ||
      !tv_info.visible)
    return; /* no need for this */

  if (tv_info.clips) {
    malloc_count --;
    g_free(tv_info.clips);
  }

  tv_info.clips =
    overlay_get_clips(tv_info.window->window, &tv_info.clipcount);

  memcpy(&window, &tv_info.info->window, sizeof(struct tveng_window));
  tveng_set_preview_off(tv_info.info);
  memcpy(&tv_info.info->window, &window, sizeof(struct tveng_window));
  tv_info.info->window.clips = tv_info.clips;
  tv_info.info->window.clipcount = tv_info.clipcount;
  tveng_set_preview_window(tv_info.info);
  tveng_set_preview_on(tv_info.info);
}

/*
 * Inits the overlay engine.
 * window: The window we will be overlaying to.
 * main_window: The toplevel window window belongs to. They can be the
 * same.
 */
void
startup_overlay(GtkWidget * window, GtkWidget * main_window,
		tveng_device_info * info)
{
  GdkEventMask mask; /* The GDK events we want to see */
  GdkColor chroma;

  gtk_signal_connect(GTK_OBJECT(main_window), "event",
		     GTK_SIGNAL_FUNC(on_main_overlay_event),
		     NULL);

  gtk_signal_connect(GTK_OBJECT(main_window), "delete-event",
		     GTK_SIGNAL_FUNC(on_main_overlay_delete_event),
		     NULL);

  gtk_signal_connect(GTK_OBJECT(window), "size-allocate",
		     GTK_SIGNAL_FUNC(on_tv_screen_size_allocate),
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
  tv_info.info = info;
  tv_info.visible = x11_window_viewable(window->window);

  tv_info.clear_timeout_id = -1;
  tv_info.clean_screen = FALSE;
  tv_info.clips = NULL;
  tv_info.clipcount = 0;
  if (info->current_controller == TVENG_CONTROLLER_XV)
    tv_info.needs_cleaning = FALSE;
  else
    tv_info.needs_cleaning = TRUE;
  gdk_window_get_size(window->window, &tv_info.w, &tv_info.h);
  gdk_window_get_origin(window->window, &tv_info.x, &tv_info.y);

  if (info->current_controller != TVENG_CONTROLLER_XV)
    {
      if (info->caps.flags & TVENG_CAPS_CHROMAKEY)
	{
	  chroma.red = chroma.green = 0;
	  chroma.blue = 65535;

	  if (gdk_colormap_alloc_color(gdk_colormap_get_system(), &chroma,
				       FALSE, TRUE))
	    {
	      tveng_set_chromakey(chroma.red >> 8, chroma.green >> 8,
				  chroma.blue >> 8, info);
	      gdk_window_set_background(window->window, &chroma);
	      gdk_colormap_free_colors(gdk_colormap_get_system(), &chroma,
				       1);
	      gdk_window_clear_area_e(window->window, 0, 0, tv_info.w,
				      tv_info.h);
	      tv_info.needs_cleaning = FALSE;
	    }
	  else
	    ShowBox("Couldn't allocate chromakey, chroma won't work",
		    GNOME_MESSAGE_BOX_WARNING);
	}
      else
	gdk_window_set_back_pixmap(window->window, NULL, FALSE);
    }

  if (tv_info.needs_cleaning)
    {
      tv_info.check_timeout_id =
	gtk_timeout_add(CHECK_TIMEOUT, overlay_periodic_timeout,
			&(tv_info.check_timeout_id));
      gtk_signal_connect(GTK_OBJECT(osd_model), "changed",
			 GTK_SIGNAL_FUNC(on_osd_model_changed),
			 NULL);
    }
  else
    tv_info.check_timeout_id = -1;
}

/*
 * Stops the overlay engine.
 */
void
overlay_stop(tveng_device_info *info)
{
  gtk_signal_disconnect_by_func(GTK_OBJECT(tv_info.main_window),
				GTK_SIGNAL_FUNC(on_main_overlay_event),
				NULL);

  gtk_signal_disconnect_by_func(GTK_OBJECT(tv_info.main_window),
				GTK_SIGNAL_FUNC(on_main_overlay_delete_event),
				NULL);

  gtk_signal_disconnect_by_func(GTK_OBJECT(tv_info.window),
				GTK_SIGNAL_FUNC(on_tv_screen_size_allocate),
				NULL);

  if (tv_info.needs_cleaning)
    gtk_signal_disconnect_by_func(GTK_OBJECT(osd_model),
				  GTK_SIGNAL_FUNC(on_osd_model_changed),
				  NULL);

  if (tv_info.clear_timeout_id >= 0)
    {
      gtk_timeout_remove(tv_info.clear_timeout_id);
      tv_info.clear_timeout_id = -1;
    }

  if (tv_info.check_timeout_id >= 0)
    {
      gtk_timeout_remove(tv_info.check_timeout_id);
      tv_info.check_timeout_id = -1;
    }

  if (tv_info.needs_cleaning)
    x11_force_expose(0, 0, gdk_screen_width(), gdk_screen_height());
}

/*
 * Shuts down the overlay engine
 */
void
shutdown_overlay(void )
{
  /* Nothing to be done */
}

/*
 * Tells the overlay engine to sync the overlay with the window.
 * clean_screen: TRUE means that we should refresh the whole screeen.
 */
void
overlay_sync(gboolean clean_screen)
{
  extern tveng_device_info *main_info;

  if (main_info->current_mode != TVENG_CAPTURE_WINDOW)
    return;

  gdk_window_get_size(tv_info.window->window, &tv_info.w, &tv_info.h);
  gdk_window_get_origin(tv_info.window->window, &tv_info.x,
			&tv_info.y);

  if (tv_info.clips) {
    malloc_count --;
    g_free(tv_info.clips);
  }

  tv_info.info->window.x = tv_info.x;
  tv_info.info->window.y = tv_info.y;
  tv_info.info->window.width = tv_info.w;
  tv_info.info->window.height = tv_info.h;
  tv_info.info->window.clips = tv_info.clips =
    overlay_get_clips(tv_info.window->window,
		      &(tv_info.info->window.clipcount));
  tv_info.clipcount = tv_info.info->window.clipcount;
  tv_info.info->window.win = GDK_WINDOW_XWINDOW(tv_info.window->window);
  tv_info.info->window.gc = GDK_GC_XGC(tv_info.window->style->white_gc);

  tveng_set_preview_window(tv_info.info);

  if (tv_info.clips) {
    g_free(tv_info.clips);
    tv_info.clips = NULL;
    malloc_count --;
  }

  /* The requested overlay coords might not be the definitive ones,
     adapt the clips */
  if (tv_info.needs_cleaning)
    {
      tv_info.clips = tv_info.info->window.clips =
	overlay_get_clips(tv_info.window->window, &(tv_info.clipcount));
      tv_info.info->window.clipcount = tv_info.clipcount;
      
      tveng_set_preview_window(tv_info.info);
      
      if (clean_screen)
	x11_force_expose(0, 0, gdk_screen_width(), gdk_screen_height());
    }
}
