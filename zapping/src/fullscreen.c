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
 * $Id: fullscreen.c,v 1.6 2001-04-04 21:29:23 garetxe Exp $
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
#include "fullscreen.h"

static GtkWidget * black_window = NULL; /* The black window when you go
					   fullscreen */
static GdkCursor *fullscreen_cursor=NULL;

extern GtkWidget * main_window;

/* Comment out the next line if you don't want to mess with the
   XScreensaver */
#define MESS_WITH_XSS 1

static
gboolean on_fullscreen_event (GtkWidget * widget, GdkEvent * event,
			      gpointer user_data)
{
  GtkWidget * window = GTK_WIDGET(user_data);
  GtkMenuItem * channel_up1 =
    GTK_MENU_ITEM(lookup_widget(window, "channel_up1"));
  GtkMenuItem * channel_down1 =
    GTK_MENU_ITEM(lookup_widget(window, "channel_down1"));
  GtkMenuItem * go_windowed1 =
    GTK_MENU_ITEM(lookup_widget(window, "channel_down1"));
  GtkMenuItem * exit2 =
    GTK_MENU_ITEM(lookup_widget(window, "exit2"));

  if (event->type == GDK_KEY_PRESS)
    {
      GdkEventKey * kevent = (GdkEventKey*) event;
      switch (kevent->keyval)
	{
	case GDK_Page_Up:
	case GDK_KP_Page_Up:
	  on_channel_up1_activate(channel_up1, NULL);
	  break;
	case GDK_Page_Down:
	case GDK_KP_Page_Down:
	  on_channel_down1_activate(channel_down1, NULL);
	  break;
	case GDK_Escape:
	case GDK_F11: /* Might be common too */
	  on_go_windowed1_activate(go_windowed1, NULL);
	  break;
	  /* Let control-Q exit the app */
	case GDK_q:
	  if (kevent->state & GDK_CONTROL_MASK)
	    {
	      extern gboolean was_fullscreen;
	      was_fullscreen = TRUE;
	      on_go_windowed1_activate(go_windowed1, NULL);
	      on_exit2_activate(exit2, NULL);
	    }
	  break;
	}
      return TRUE; /* Event processing done */
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
    x11_get_clips(black_window->window,
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
  GdkPixmap *source, *mask;
  GdkColor fg = {0, 0, 0, 0};
  GdkColor bg = {0, 0, 0, 0};
  GdkColor chroma;

#define empty_cursor_width 16
#define empty_cursor_height 16
  unsigned char empty_cursor_bits[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  unsigned char empty_cursor_mask[] = {
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  source = gdk_bitmap_create_from_data(NULL, empty_cursor_bits,
				       empty_cursor_width,
				       empty_cursor_height);

  mask = gdk_bitmap_create_from_data(NULL, empty_cursor_mask,
				     empty_cursor_width,
				     empty_cursor_height);

  /* Add a black background */
  black_window = gtk_window_new( GTK_WINDOW_POPUP );
  da = gtk_drawing_area_new();

  gtk_widget_show(da);

  if (fullscreen_cursor)
    gdk_cursor_destroy(fullscreen_cursor);

  fullscreen_cursor =
    gdk_cursor_new_from_pixmap(source, mask, &fg, &bg, 8, 8);

  gdk_pixmap_unref(source);
  gdk_pixmap_unref(mask);

  gtk_container_add(GTK_CONTAINER(black_window), da);
  gtk_widget_set_usize(black_window, gdk_screen_width(),
		       gdk_screen_height());

  gtk_widget_realize(black_window);
  gtk_widget_realize(da);
  gtk_window_set_modal(GTK_WINDOW(black_window), TRUE);
  gdk_window_set_decorations(black_window->window, 0);

  /* hide the cursor in fullscreen mode */
  gdk_window_set_cursor(da->window, fullscreen_cursor);

  /* Draw on the drawing area */
  gdk_draw_rectangle(da -> window,
  		     da -> style -> black_gc,
		     TRUE,
		     0, 0, gdk_screen_width(), gdk_screen_height());

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
	      gdk_window_set_background(da->window, &chroma);
	      gdk_colormap_free_colors(gdk_colormap_get_system(), &chroma,
				       1);
	    }
	  else
	    {
	      ShowBox("Couldn't allocate chromakey, chroma won't work",
		      GNOME_MESSAGE_BOX_WARNING);
	      gdk_window_set_background(da->window, &bg);
	    }
	}
      else
	gdk_window_set_background(da->window, &bg);
    }

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
  /*
    If something doesn't work, everything will be blocked here, maybe
    this isn't a good idea... but it is apparently the less bad one.
  */
  gdk_keyboard_grab(black_window->window, TRUE, GDK_CURRENT_TIME);

  gdk_window_set_events(black_window->window, GDK_ALL_EVENTS_MASK);

  gtk_signal_connect(GTK_OBJECT(black_window), "key-press-event",
		     GTK_SIGNAL_FUNC(on_fullscreen_event),
  		     main_window);

  if (info->current_controller != TVENG_CONTROLLER_XV)
    osd_set_coords(info->window.x, info->window.y, info->window.width,
		   info->window.height);
  else
    osd_set_coords(0, 0, info->window.width, info->window.height);

  gtk_signal_connect(GTK_OBJECT(osd_model), "changed",
		     GTK_SIGNAL_FUNC(osd_model_changed), info);

  return 0;
}

void
fullscreen_stop(tveng_device_info * info)
{
  gdk_keyboard_ungrab(GDK_CURRENT_TIME);

#ifdef MESS_WITH_XSS
  /* Restore the normal screensaver */
  x11_set_screensaver(ON);
#endif

  /* Remove the black window */
  gtk_widget_destroy(black_window);
  x11_force_expose(0, 0, gdk_screen_width(), gdk_screen_height());

  if (fullscreen_cursor)
    gdk_cursor_destroy(fullscreen_cursor);

  fullscreen_cursor = NULL;
  gtk_signal_disconnect_by_func(GTK_OBJECT(osd_model),
				GTK_SIGNAL_FUNC(osd_model_changed),
				info);
}
