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
 * $Id: fullscreen.c,v 1.21.2.10 2003-09-29 07:06:52 mschimek Exp $
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "tveng_private.h"
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
#include "zvideo.h"
#include "globals.h"

static GtkWidget * black_window = NULL; /* The black window when you go
					   fullscreen */
static x11_vidmode_state old_vidmode;

static gboolean
on_fullscreen_event		(GtkWidget *		widget,
				 GdkEvent *		event,
				 gpointer		user_data)
{
  switch (event->type)
    {
    case GDK_KEY_PRESS:
      {
	GdkEventKey *kevent = (GdkEventKey *) event;

	if (kevent->keyval == GDK_q
	    && (kevent->state & GDK_CONTROL_MASK))
	  {
	    extern gboolean was_fullscreen;

	    was_fullscreen = TRUE;
	    zmisc_switch_mode(last_mode, main_info);
	    python_command (widget, "zapping.quit()");

	    return TRUE;
	  }
	else
	  {
	    return on_user_key_press (widget, kevent, user_data)
	      || on_channel_key_press (widget, kevent, user_data);
	  }
      }

    case GDK_BUTTON_PRESS:
      zmisc_switch_mode(last_mode, main_info);
      return TRUE;
    }

  return FALSE; /* pass on */
}

/* Called when OSD changes the geometry of the pieces */
static void
osd_model_changed			(ZModel		*ignored1,
					 tveng_device_info *info)
{
  tv_window window;
  GdkWindow *gdk_window;

  if (info->current_controller == TVENG_CONTROLLER_XV ||
      !black_window || !black_window->window)
    return;

  window = info->overlay_window;
  tveng_set_preview_off (info);
  info->overlay_window = window;

  gdk_window = GTK_BIN (black_window)->child->window;

  x11_window_clip_vector (&info->overlay_window.clip_vector,
			  GDK_WINDOW_XDISPLAY (gdk_window),
			  GDK_WINDOW_XID (gdk_window),
			  window.x,
			  window.y,
			  window.width,
			  window.height);

  tveng_set_preview_window (info);
  tveng_set_preview_on (info);
}

static void
on_cursor_blanked		(ZVideo *		video,
				 gpointer		user_data)
{
  /* Recenter */
  x11_vidmode_switch (vidmodes, NULL, NULL);
}

static x11_vidmode_info *
find_vidmode			(guint			width,
				 guint			height)
{
  x11_vidmode_info *v;
  x11_vidmode_info *vmin;
  gint64 amin;

  vmin = vidmodes;
  amin = ((gint64) 1) << 62;

  for (v = vidmodes; v; v = v->next)
    {
      gint64 a;
      gint64 dw, dh;

      if (0)
	fprintf (stderr, "width=%d height=%d cur %ux%u@%u best %ux%u@%u\n",
		 width, height,
		 v->width, v->height, (guint)(v->vfreq + 0.5),
		 vmin->width, vmin->height, (guint)(vmin->vfreq + 0.5));

      /* XXX ok? */
      dw = (gint64) v->width - width;
      dh = (gint64) v->height - height;
      a = dw * dw + dh * dh;

      if (a < amin
	  || (a == amin && v->vfreq > vmin->vfreq))
	{
	  vmin = v;
	  amin = a;
	}
    }

  v = vmin;

  if (main_info->debug_level > 0)
    fprintf (stderr, "Using mode %ux%u@%u for video %ux%u\n",
	     v->width, v->height, (guint)(v->vfreq + 0.5),
	     width, height);

  return v;
}

gboolean
start_fullscreen		(tveng_device_info *	info)
{
  GtkWidget *da; /* drawing area */
  GdkColor chroma = {0, 0, 0, 0};
  const gchar *vidmode;
  x11_vidmode_info *v;
  unsigned int width, height;

  /* Notes: The main window is not used in fullscreen mode but
     remains open all the time. black_window is an unmanaged,
     fullscreen window without decorations.
     da is a drawing area of same size as the window.
     start_previewing enables Xv or v4l overlay into da->window.
     The video size is limited by the DMA hardware
     (eg. 768x576x4) and the window size, thus usually drawing only
     part of the window, centered. Independent of all this
     start_previewing tries to figure out the actual video size
     and select a similar vidmode (eg. 800x600).

     Plan: Determine a nominal size 480 or 576 times 4/3 or 16/9.
     Find hardware limit and choose capture or overlay mode,
     da->window size == overlay size and vidmode.
   */

  black_window = gtk_window_new (GTK_WINDOW_POPUP);
  gtk_widget_set_size_request (black_window,
			       gdk_screen_width(),
			       gdk_screen_height());

  da = z_video_new ();
  gtk_widget_show (da);
  gtk_widget_modify_bg (da, GTK_STATE_NORMAL, &chroma);
  gtk_container_add (GTK_CONTAINER (black_window), da);

  gtk_widget_add_events (da, GDK_BUTTON_PRESS_MASK);
  gtk_window_set_modal (GTK_WINDOW(black_window), TRUE);
  gtk_widget_realize (black_window);
  gdk_window_set_decorations (black_window->window, 0);
  gtk_widget_show (black_window);

  z_video_blank_cursor (Z_VIDEO (da), 1500 /* ms */);

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
	{
	  ShowBox("Couldn't allocate chromakey, chroma won't work",
		  GTK_MESSAGE_WARNING);
	}
    }
  else if (info->current_controller == TVENG_CONTROLLER_XV
	   && 0 == tveng_get_chromakey (&chroma.pixel, info))
    {
      gdk_window_set_background(da->window, &chroma);
    }

  /* Disable double buffering just in case, will help in case a
     XV driver doesn't provide XV_COLORKEY but requires the colorkey
     not to be overwritten */
  gtk_widget_set_double_buffered (da, FALSE);

  /* Needed for XV fullscreen */
  info->overlay_window.win = GDK_WINDOW_XWINDOW(da->window);
  info->overlay_window.gc = GDK_GC_XGC(da->style->white_gc);

  if (!tv_set_overlay_buffer (info, &dga_param))
    goto failure;

  vidmode = zcg_char (NULL, "fullscreen/vidmode");

  /* XXX we must distinguish between (limited) Xv overlay and Xv scaling.
   * 1) determine natural size (videostd -> 480, 576 -> 640, 768),
   *    try Xv target size, get actual
   * 2) find closest vidmode
   * 3) try Xv target size from vidmode, get actual,
   *    if closer go back to 2) until sizes converge 
   */
  if (info->current_controller == TVENG_CONTROLLER_XV
      && (vidmode == NULL || 0 == strcmp (vidmode, "auto")))
    {
      vidmode = NULL; /* not needed  XXX wrong */
    }
  else
    {
      /* XXX ditto, limited V4L/V4L2 overlay */
      if (!x11_dga_present (&dga_param))
	{
	  info->tveng_errno = -1;
	  tv_error_msg (info, _("No XFree86 DGA extension.\n"));
	  goto failure;
	}
    }

  g_assert (vidmode != (const char *) -1);

  /* XXX wrong */
  if (info->cur_video_standard)
    {
      /* Note e.g. v4l2(old) bttv reports 48,32-922,576 */
      width = MIN ((guint) info->caps.maxwidth,
		   info->cur_video_standard->frame_width);
      height = MIN ((guint) info->caps.maxheight,
		    info->cur_video_standard->frame_height);
    }
  else
    {
      width = MIN (info->caps.maxwidth, 768);
      height = MIN (info->caps.maxheight, 576);
    }

  width = MIN (width, dga_param.width);
  height = MIN (height, dga_param.height);

  v = NULL;

  if (vidmode == NULL || vidmode[0] == 0)
    {
      /* Don't change, v = NULL. */
    }
  else if (vidmodes)
    {
      if ((v = x11_vidmode_by_name (vidmodes, vidmode)))
	{
	  /* User defined mode. */
	}
      else
	{
	  /* Automatic. */
	  v = find_vidmode (width, height);
	}
    }

  if (!x11_vidmode_switch (vidmodes, v, &old_vidmode))
    {
      /* Bad, but not fatal. */
      /* goto failure; */
      x11_vidmode_clear_state (&old_vidmode);
    }

  // XXX
  if (info->current_controller == TVENG_CONTROLLER_XV)
    {
      if (!tv_set_overlay_xwindow (info, info->overlay_window.win,
				   info->overlay_window.gc))
	goto failure;
    }
  else
    {
      /* Center the window, dwidth is always >= width */
      info->overlay_window.x = (dga_param.width - width) >> 1;
      info->overlay_window.y = (dga_param.height - height) >> 1;
      info->overlay_window.width = width;
      info->overlay_window.height = height;

      tv_clip_vector_clear (&info->overlay_window.clip_vector);

      /* Set new capture dimensions */
      if (-1 == tveng_set_preview_window (info))
	goto failure;

      if (info->overlay_window.width != width
	  || info->overlay_window.height != height)
	{
	  info->overlay_window.x =
	    (dga_param.width - info->overlay_window.width) >> 1;
	  info->overlay_window.y =
	    (dga_param.height - info->overlay_window.height) >> 1;

	  if (-1 == tveng_set_preview_window (info))
	    goto failure;
	}
    }

  /* Start preview */
  if (-1 == tveng_set_preview (TRUE, info))
    goto failure;

  info->current_mode = TVENG_CAPTURE_PREVIEW; /* "fullscreen overlay" */

  g_signal_connect (G_OBJECT (da), "cursor-blanked",
		    G_CALLBACK (on_cursor_blanked), info);

  gtk_widget_grab_focus (black_window);

  g_signal_connect (G_OBJECT (black_window), "event",
		    G_CALLBACK (on_fullscreen_event),
		    main_window);

  if (info->current_controller != TVENG_CONTROLLER_XV)
    {
      osd_set_coords (da,
		      info->overlay_window.x,
		      info->overlay_window.y,
		      info->overlay_window.width,
		      info->overlay_window.height);
    }
  else
    {
      /* wrong because Xv may pad to this size (DMA hw limit) */
      osd_set_coords (da, 0, 0, info->overlay_window.width,
		      info->overlay_window.height);
    }

  g_signal_connect (G_OBJECT (osd_model), "changed",
		    G_CALLBACK (osd_model_changed), info);

  return TRUE;

 failure:
  gtk_widget_destroy (black_window);
  return FALSE;
}

void
stop_fullscreen			(tveng_device_info *	info)
{
  g_assert (info->current_mode == TVENG_CAPTURE_PREVIEW);

  /* Error ignored */
  tveng_set_preview (FALSE, info);

  info->current_mode = TVENG_NO_CAPTURE;

  x11_vidmode_restore (vidmodes, &old_vidmode);

  osd_unset_window ();

  /* Remove the black window */
  gtk_widget_destroy (black_window);
  black_window = NULL;

  x11_force_expose (0, 0, gdk_screen_width(), gdk_screen_height());

  g_signal_handlers_disconnect_matched
    (G_OBJECT (osd_model), G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
     0, 0, NULL, G_CALLBACK (osd_model_changed), info);
}
