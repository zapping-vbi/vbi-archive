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
 * $Id: fullscreen.c,v 1.21.2.5 2003-01-30 02:39:49 mschimek Exp $
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
#include "audio.h"
#include "plugins.h"

static GtkWidget * black_window = NULL; /* The black window when you go
					   fullscreen */
extern GtkWidget * main_window;

extern tveng_tuned_channel *global_channel_list;

extern enum tveng_capture_mode last_mode;

extern tveng_device_info *main_info;

/* Comment out the next line if you don't want to mess with the
   XScreensaver */
#define MESS_WITH_XSS 1

static gboolean
on_fullscreen_event			(GtkWidget *	widget,
					 GdkEvent *	event,
					 gpointer	user_data)
{
  if (event->type == GDK_KEY_PRESS)
    {
      GdkEventKey *kevent = (GdkEventKey *) event;

      if (kevent->keyval == GDK_q && (kevent->state & GDK_CONTROL_MASK))
	{
	  extern gboolean was_fullscreen;

	  was_fullscreen = TRUE;
	  zmisc_switch_mode(last_mode, main_info);
	  cmd_run ("zapping.quit()");

	  return TRUE;
	}
      else
	{
          return on_user_key_press (widget, kevent, user_data)
	    || on_channel_key_press (widget, kevent, user_data);
	}
    }
  else if (event->type == GDK_BUTTON_PRESS)
    {
      zmisc_switch_mode(last_mode, main_info);
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
  da = gtk_drawing_area_new();

  gtk_widget_show(da);

  gtk_container_add(GTK_CONTAINER(black_window), da);
  gtk_widget_set_size_request(black_window, gdk_screen_width(),
		       gdk_screen_height());

  gtk_widget_add_events(da, GDK_BUTTON_PRESS_MASK);
  gtk_window_set_modal(GTK_WINDOW(black_window), TRUE);
  gtk_widget_realize (black_window);
  gdk_window_set_decorations (black_window->window, 0);
  gtk_widget_show (black_window);

  /* hide the cursor in fullscreen mode */
  z_set_cursor(da->window, 0);

  if (info->current_controller != TVENG_CONTROLLER_XV &&
      (info->caps.flags & TVENG_CAPS_CHROMAKEY))
    {
      chroma.red = chroma.green = 0;
      chroma.blue = 0xffff;
      
      if (gdk_colormap_alloc_color(gdk_colormap_get_system(), &chroma,
				   FALSE, TRUE))
	{
	  tveng_set_chromakey(chroma.pixel, info);
	  gdk_window_set_background(da->window, &chroma);
	  gdk_colormap_free_colors(gdk_colormap_get_system(), &chroma,
				   1);
	}
      else
	ShowBox("Couldn't allocate chromakey, chroma won't work",
		GTK_MESSAGE_WARNING);
    }
  else if (info->current_controller == TVENG_CONTROLLER_XV &&
	   tveng_get_chromakey (&chroma.pixel, info) == 0)
    gdk_window_set_background(da->window, &chroma);

  /* Disable double buffering just in case, will help in case a
     XV driver doesn't provide XV_COLORKEY but requires the colorkey
     not to be overwritten */
  gtk_widget_set_double_buffered (da, FALSE);

  /* Needed for XV fullscreen */
  info->window.win = GDK_WINDOW_XWINDOW(da->window);
  info->window.gc = GDK_GC_XGC(da->style->white_gc);

  if (tveng_start_previewing(info, zcg_char(NULL, "fullscreen/vidmode")) == -1)
    {
      ShowBox(_("Sorry, but cannot go fullscreen:\n%s"),
	      GTK_MESSAGE_ERROR, info->error);
      gtk_widget_destroy(black_window);
      zmisc_switch_mode(TVENG_CAPTURE_READ, info);
      return -1;
    }

  if (info -> current_mode != TVENG_CAPTURE_PREVIEW)
    g_warning("Setting preview succeeded, but the mode is not set");

  gtk_widget_grab_focus(black_window);

  g_signal_connect(G_OBJECT(black_window), "event",
		     G_CALLBACK(on_fullscreen_event),
  		     main_window);

  if (info->current_controller != TVENG_CONTROLLER_XV)
    osd_set_coords(da,
		   info->window.x, info->window.y, info->window.width,
		   info->window.height);
  else
    osd_set_coords(da, 0, 0, info->window.width,
		   info->window.height);

  g_signal_connect(G_OBJECT(osd_model), "changed",
		     G_CALLBACK(osd_model_changed), info);

  return 0;
}

void
fullscreen_stop(tveng_device_info * info)
{
  osd_unset_window();

  /* Remove the black window */
  gtk_widget_destroy(black_window);
  x11_force_expose(0, 0, gdk_screen_width(), gdk_screen_height());

  g_signal_handlers_disconnect_matched(G_OBJECT(osd_model),
				       G_SIGNAL_MATCH_FUNC |
				       G_SIGNAL_MATCH_DATA,
				       0, 0, NULL,
				       G_CALLBACK(osd_model_changed),
				       info);
}
