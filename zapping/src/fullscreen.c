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

/**
 * Fullscreen mode handling
 * $Id: fullscreen.c,v 1.16 2002-01-13 09:52:22 mschimek Exp $
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "tveng.h"
#include "osd.h"
#include "x11stuff.h"
#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zmisc.h"
#include "zconf.h"
#include "interface.h"
#include "callbacks.h"
#include "v4linterface.h"
#include "fullscreen.h"

static GtkWidget * black_window = NULL; /* The black window when you go
					   fullscreen */
extern GtkWidget * main_window;

extern tveng_tuned_channel *global_channel_list;

extern enum tveng_capture_mode restore_mode;

extern tveng_device_info *main_info;

/* Comment out the next line if you don't want to mess with the
   XScreensaver */
#define MESS_WITH_XSS 1

static
gboolean on_fullscreen_event (GtkWidget * widget, GdkEvent * event,
			      gpointer user_data)
{
  GtkWidget * window = GTK_WIDGET(user_data);
  GtkMenuItem * exit2 =
    GTK_MENU_ITEM(lookup_widget(window, "exit2"));
  tveng_tuned_channel *tc;
  gint i = 0;

  if (event->type == GDK_KEY_PRESS)
    {
      GdkEventKey * kevent = (GdkEventKey*) event;
      switch (kevent->keyval)
	{
	case GDK_Page_Up:
	case GDK_KP_Page_Up:
	  z_channel_up();
	  break;
	case GDK_Page_Down:
	case GDK_KP_Page_Down:
	  z_channel_down();
	  break;
	case GDK_Escape:
	case GDK_F11: /* Might be common too */
	  zmisc_switch_mode(restore_mode, main_info);
	  break;
	  /* Let control-Q exit the app */
	case GDK_q:
	  if (kevent->state & GDK_CONTROL_MASK)
	    {
	      extern gboolean was_fullscreen;
	      was_fullscreen = TRUE;
	      zmisc_switch_mode(restore_mode, main_info);
	      on_exit2_activate(exit2, NULL);
	    }
	  break;
	  /* mute/unmute */
	case GDK_a:
	  if (kevent->state & GDK_CONTROL_MASK)
	    on_toggle_muted1_activate(NULL, NULL);
	  break;
	default:
	  while ((tc =
		  tveng_retrieve_tuned_channel_by_index(i++,
							global_channel_list)))
	    {
	      if ((kevent->keyval == tc->accel_key) &&
		  ((tc->accel_mask & kevent->state) == tc->accel_mask))
		{
		  z_select_channel(tc->index);
		  return TRUE;
		}
	    }
	  break;
	}
      return TRUE; /* Event processing done */
    }
  else if (event->type == GDK_BUTTON_PRESS)
    {
      zmisc_switch_mode(restore_mode, main_info);
      return TRUE;
    }

  return FALSE; /* We aren't interested in this event, pass it on */
}

/* Called when OSD changes the geometry of the pieces */
static void
osd_model_changed			(ZModel		*ignored1,
					 tveng_device_info *info)
{
  struct tveng_window window;
  struct tveng_clip *clips;

  if (info->current_controller == TVENG_CONTROLLER_XV ||
      !black_window || !black_window->window)
    return;

  /* save for later use */
  memcpy(&window, &info->window, sizeof(struct tveng_window));

  tveng_set_preview_off(info);
  memcpy(&info->window, &window, sizeof(struct tveng_window));
  clips = info->window.clips =
    x11_get_clips(GTK_BIN(black_window)->child->window,
		  window.x, window.y,
		  window.width,
		  window.height,
		  &window.clipcount);
  info->window.clipcount = window.clipcount;
  tveng_set_preview_window(info);
  tveng_set_preview_on(info);

  g_free(clips);
}

gint
fullscreen_start(tveng_device_info * info)
{
  GtkWidget * da; /* Drawing area */
  GdkColor chroma = {0, 0, 0, 0};

  /* Add a black background */
  black_window = gtk_window_new( GTK_WINDOW_POPUP );
  da = gtk_fixed_new();

  gtk_widget_show(da);

  gtk_container_add(GTK_CONTAINER(black_window), da);
  gtk_widget_set_usize(black_window, gdk_screen_width(),
		       gdk_screen_height());

  gtk_widget_realize(black_window);
  gtk_widget_realize(da);
  gdk_window_set_events(black_window->window, GDK_ALL_EVENTS_MASK);
  gdk_window_set_events(da->window, GDK_ALL_EVENTS_MASK);
  gtk_window_set_modal(GTK_WINDOW(black_window), TRUE);
  gdk_window_set_decorations(black_window->window, 0);

  /* hide the cursor in fullscreen mode */
  z_set_cursor(da->window, 0);

  if (info->current_controller != TVENG_CONTROLLER_XV &&
      (info->caps.flags & TVENG_CAPS_CHROMAKEY))
    {
      chroma.red = chroma.green = 0;
      chroma.blue = 0xffff;
      
      if (gdk_colormap_alloc_color(gdk_colormap_get_system(), &chroma,
				   FALSE, TRUE))
	tveng_set_chromakey(chroma.red >> 8, chroma.green >> 8,
			    chroma.blue >> 8, info);
      else
	ShowBox("Couldn't allocate chromakey, chroma won't work",
		GNOME_MESSAGE_BOX_WARNING);
    }

  gdk_window_set_background(da->window, &chroma);

  if (chroma.pixel != 0)
    gdk_colormap_free_colors(gdk_colormap_get_system(), &chroma,
			     1);

  /* Needed for XV fullscreen */
  info->window.win = GDK_WINDOW_XWINDOW(da->window);
  info->window.gc = GDK_GC_XGC(da->style->white_gc);

  if (tveng_start_previewing(info, 1-zcg_int(NULL, "change_mode")) == -1)
    {
      ShowBox(_("Sorry, but cannot go fullscreen:\n%s"),
	      GNOME_MESSAGE_BOX_ERROR, info->error);
      gtk_widget_destroy(black_window);
      zmisc_switch_mode(TVENG_CAPTURE_READ, info);
      return -1;
    }

  gtk_widget_show(black_window);

  if (info -> current_mode != TVENG_CAPTURE_PREVIEW)
    g_warning("Setting preview succeeded, but the mode is not set");

#ifdef MESS_WITH_XSS
  /* Set the blank screensaver */
  x11_set_screensaver(OFF);
#endif

  gtk_widget_grab_focus(black_window);

  gtk_signal_connect(GTK_OBJECT(black_window), "event",
		     GTK_SIGNAL_FUNC(on_fullscreen_event),
  		     main_window);
#ifdef HAVE_LIBZVBI
  if (info->current_controller != TVENG_CONTROLLER_XV)
    osd_set_coords(da,
		   info->window.x, info->window.y, info->window.width,
		   info->window.height);
  else
    osd_set_coords(da, 0, 0, info->window.width,
		   info->window.height);

  gtk_signal_connect(GTK_OBJECT(osd_model), "changed",
		     GTK_SIGNAL_FUNC(osd_model_changed), info);
#endif

  return 0;
}

void
fullscreen_stop(tveng_device_info * info)
{
#ifdef MESS_WITH_XSS
  /* Restore the normal screensaver */
  x11_set_screensaver(ON);
#endif

#ifdef HAVE_LIBZVBI
  osd_unset_window();
#endif

  /* Remove the black window */
  gtk_widget_destroy(black_window);
  x11_force_expose(0, 0, gdk_screen_width(), gdk_screen_height());

#ifdef HAVE_LIBZVBI
  gtk_signal_disconnect_by_func(GTK_OBJECT(osd_model),
				GTK_SIGNAL_FUNC(osd_model_changed),
				info);
#endif
}
