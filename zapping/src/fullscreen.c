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
 * $Id: fullscreen.c,v 1.26 2004-09-10 04:51:05 mschimek Exp $
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zconf.h"
#include "tveng_private.h"
#include "osd.h"
#include "x11stuff.h"
#include "zmisc.h"
#include "interface.h"
#include "v4linterface.h"
#include "fullscreen.h"
#include "audio.h"
#include "plugins.h"
#include "zvideo.h"
#include "globals.h"

static x11_vidmode_info *svidmodes;
static GtkWidget * black_window = NULL; /* The black window when you go
					   fullscreen */
static x11_vidmode_state old_vidmode;
static const tv_screen *screen;

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
	    zmisc_switch_mode(last_dmode, last_cmode, zapping->info);
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
      zmisc_switch_mode(last_dmode, last_cmode, zapping->info);
      return TRUE;

    default:
      break;
    }

  return FALSE; /* pass on */
}

/* Called when OSD changes the geometry of the pieces */
static void
osd_model_changed			(ZModel		*ignored1 _unused_,
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
on_cursor_blanked		(ZVideo *		video _unused_,
				 gpointer		user_data _unused_)
{
  /* Recenter */
  x11_vidmode_switch (svidmodes, screens, NULL, NULL);
}

static const x11_vidmode_info *
find_vidmode			(const x11_vidmode_info *vidmodes,
				 guint			width,
				 guint			height)
{
  const x11_vidmode_info *v;
  const x11_vidmode_info *vmin;
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

  if (zapping->info->debug_level > 0)
    fprintf (stderr, "Using mode %ux%u@%u for video %ux%u\n",
	     v->width, v->height, (guint)(v->vfreq + 0.5),
	     width, height);

  return v;
}

gboolean
start_fullscreen		(tveng_device_info *	info)
{
  const tv_screen *xs;
  GtkWindow *window;
  GtkWidget *da; /* drawing area */
  GdkColor chroma;
  const gchar *vm_name;
  unsigned int width;
  unsigned int height;
  const x11_vidmode_info *vm;

  /* Notes: The main window is not used in fullscreen mode but it
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

  {
    gint x;
    gint y;
    gint width;
    gint height;

    /* Root window relative position. */
    gdk_window_get_origin (GTK_WIDGET (zapping)->window, &x, &y);

    gdk_window_get_geometry (GTK_WIDGET (zapping)->window,
			     /* x */ NULL,
			     /* y */ NULL,
			     &width,
			     &height,
			     /* depth */ NULL);

    xs = tv_screen_list_find (screens, x, y,
				  (guint) width, (guint) height);
    if (!xs)
      xs = screens;

    screen = xs;
  }

  /* VidModes available on this screen. */
  svidmodes = x11_vidmode_list_new (NULL, xs->screen_number);

  black_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  window = GTK_WINDOW (black_window);

  gtk_widget_set_size_request (black_window,
			       (gint) xs->width,
			       (gint) xs->height);

  da = z_video_new ();
  gtk_widget_show (da);

  CLEAR (chroma);
  gtk_widget_modify_bg (black_window, GTK_STATE_NORMAL, &chroma);
  gtk_widget_modify_bg (da, GTK_STATE_NORMAL, &chroma);

  gtk_container_add (GTK_CONTAINER (black_window), da);

  gtk_widget_add_events (da, GDK_BUTTON_PRESS_MASK);

  gtk_widget_realize (black_window);
  gdk_window_set_decorations (black_window->window, 0);

  gtk_widget_show (black_window);

  /* Make sure we're on the right screen. */
  gtk_window_move (window, (gint) xs->x, (gint) xs->y);

  /* This should span only one screen, with or without Xinerama. */
  x11_window_fullscreen (window, TRUE);

  gtk_window_present (window);

  z_video_blank_cursor (Z_VIDEO (da), 1500 /* ms */);

  /* Make sure we use an Xv adaptor which can render into da->window.
     (Won't help with X.org but it's the right thing to do.) */
  tveng_close_device(info);
  if (-1 == tveng_attach_device (zcg_char (NULL, "video_device"),
				 GDK_WINDOW_XWINDOW (da->window),
				 TVENG_ATTACH_XV, info))
    {
      ShowBox("Overlay mode not available:\n%s",
	      GTK_MESSAGE_ERROR, info->error);
      goto failure;
    }

  if (info->current_controller != TVENG_CONTROLLER_XV &&
      (info->caps.flags & TVENG_CAPS_CHROMAKEY))
    {
      chroma.blue = 0xffff;
      
      if (gdk_colormap_alloc_color(gdk_colormap_get_system(), &chroma,
				   FALSE, TRUE))
	{
	  z_set_window_bg (da, &chroma);

	  gdk_colormap_free_colors (gdk_colormap_get_system(), &chroma, 1);
	}
      else
	{
	  ShowBox("Couldn't allocate chromakey, chroma won't work",
		  GTK_MESSAGE_WARNING);
	}
    }
  else if (info->current_controller == TVENG_CONTROLLER_XV)
    {
      GdkColor chroma;

      CLEAR (chroma);

      if (0 == tveng_get_chromakey (&chroma.pixel, info))
	z_set_window_bg (da, &chroma);
    }

  /* Disable double buffering just in case, will help when a
     XV driver doesn't provide XV_COLORKEY but requires the colorkey
     not to be overwritten */
  gtk_widget_set_double_buffered (da, FALSE);

#ifdef HAVE_VIDMODE_EXTENSION
  vm_name = zcg_char (NULL, "fullscreen/vidmode");
#else
  vm_name = NULL;
#endif

  /* XXX we must distinguish between (limited) Xv overlay and Xv scaling.
   * 1) determine natural size (videostd -> 480, 576 -> 640, 768),
   *    try Xv target size, get actual
   * 2) find closest vidmode
   * 3) try Xv target size from vidmode, get actual,
   *    if closer go back to 2) until sizes converge 
   */
  if (info->current_controller == TVENG_CONTROLLER_XV
      && (!vm_name || 0 == strcmp (vm_name, "auto")))
    {
      vm_name = NULL; /* not needed  XXX wrong */
    }
  else
    {
      /* XXX ditto, limited V4L/V4L2 overlay */
      /* XXX using xs */
    }

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
      width = MIN (info->caps.maxwidth, 768U);
      height = MIN (info->caps.maxheight, 576U);
    }

  width = MIN (width, xs->width);
  height = MIN (height, xs->height);

  if (!vm_name || 0 == vm_name[0])
    {
      /* Don't change. */
      vm = NULL;
    }
  else if ((vm = x11_vidmode_by_name (svidmodes, vm_name)))
    {
      /* User defined mode. */
    }
  else
    {
      /* Automatic. */
      vm = find_vidmode (svidmodes, width, height);
    }

  if (!x11_vidmode_switch (svidmodes, screens, vm, &old_vidmode))
    {
      /* Bad, but not fatal. */
      /* goto failure; */
      x11_vidmode_clear_state (&old_vidmode);
    }

  /* XXX */
  if (info->current_controller == TVENG_CONTROLLER_XV)
    {
      if (!tv_set_overlay_xwindow (info,
				   GDK_WINDOW_XWINDOW (da->window),
				   GDK_GC_XGC (da->style->white_gc)))
	goto failure;

      /* For OSD. */
      info->overlay_window.x = (xs->width - width) >> 1;
      info->overlay_window.y = (xs->height - height) >> 1;
      info->overlay_window.width = width;
      info->overlay_window.height = height;
    }
  else
    {
      if (!z_set_overlay_buffer (info, xs, da->window))
	goto failure;

      /* Center the window, dwidth is always >= width */
      info->overlay_window.x = (xs->width - width) >> 1;
      info->overlay_window.y = (xs->height - height) >> 1;
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
	    (xs->width - info->overlay_window.width) >> 1;
	  info->overlay_window.y =
	    (xs->height - info->overlay_window.height) >> 1;

	  if (-1 == tveng_set_preview_window (info))
	    goto failure;
	}
    }

  /* Start preview */
  if (-1 == tveng_set_preview (TRUE, info))
    goto failure;

  zapping->display_mode = DISPLAY_MODE_FULLSCREEN;
  info->capture_mode = CAPTURE_MODE_OVERLAY;

  g_signal_connect (G_OBJECT (da), "cursor-blanked",
		    G_CALLBACK (on_cursor_blanked), info);

  gtk_widget_grab_focus (black_window);

  g_signal_connect (G_OBJECT (black_window), "event",
		    G_CALLBACK (on_fullscreen_event),
		    GTK_WIDGET (zapping));

  if (info->current_controller != TVENG_CONTROLLER_XV)
    {
      osd_set_coords (da,
		      info->overlay_window.x,
		      info->overlay_window.y,
		      (gint) info->overlay_window.width,
		      (gint) info->overlay_window.height);
    }
  else
    {
      /* wrong because Xv may pad to this size (DMA hw limit) */
      osd_set_coords (da,
		      info->overlay_window.x,
		      info->overlay_window.y,
		      (gint) info->overlay_window.width,
		      (gint) info->overlay_window.height);
    }

  g_signal_connect (G_OBJECT (osd_model), "changed",
		    G_CALLBACK (osd_model_changed), info);

  return TRUE;

 failure:
  x11_vidmode_list_delete (svidmodes);

  gtk_widget_destroy (black_window);

  return FALSE;
}

void
stop_fullscreen			(tveng_device_info *	info)
{
  g_assert (DISPLAY_MODE_FULLSCREEN == zapping->display_mode);

  /* Error ignored */
  tveng_set_preview (FALSE, info);

  zapping->display_mode = DISPLAY_MODE_NONE;
  info->capture_mode = CAPTURE_MODE_NONE;

  x11_vidmode_restore (vidmodes, &old_vidmode);

  osd_unset_window ();

  /* Remove the black window */
  gtk_widget_destroy (black_window);
  black_window = NULL;

  x11_force_expose ((gint) screen->x,
		    (gint) screen->y,
		    screen->width,
		    screen->height);

  g_signal_handlers_disconnect_matched
    (G_OBJECT (osd_model), G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
     0, 0, NULL, G_CALLBACK (osd_model_changed), info);

  x11_vidmode_list_delete (svidmodes);
  svidmodes = NULL;
}
