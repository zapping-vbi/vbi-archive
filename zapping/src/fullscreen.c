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

/* $Id: fullscreen.c,v 1.38 2005-01-08 14:29:56 mschimek Exp $ */

/**
 * Fullscreen mode handling
 *
 * I wonder if we shouldn't simply switch the main window to
 * fullscreen...
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zconf.h"
#include "zgconf.h"
#include "osd.h"
#include "x11stuff.h"
#include "interface.h"
#include "v4linterface.h"
#include "fullscreen.h"
#include "audio.h"
#include "plugins.h"
#include "zvideo.h"
#include "globals.h"
#include "zvbi.h"

static x11_vidmode_info *	svidmodes;
static GtkWidget *		drawing_area;
static GtkWidget * 		black_window; /* The black window when you go
						 fullscreen */
static x11_vidmode_state	old_vidmode;
static const tv_screen *	screen;

static gint			old_mw_x;
static gint			old_mw_y;

static gboolean
on_key_press			(GtkWidget *		widget,
				 GdkEventKey *		event)
{
  if (GDK_q == event->keyval
      && (event->state & GDK_CONTROL_MASK))
    {
      extern gboolean was_fullscreen;

      was_fullscreen = TRUE;

      python_command (widget, "zapping.switch_mode('window'); zapping.quit()");

      return TRUE;
    }
  else if (GDK_Escape == event->keyval)
    {
      python_command (widget, "zapping.switch_mode('window')");

      return TRUE;
    }

#ifdef HAVE_LIBZVBI
  if (CAPTURE_MODE_TELETEXT == tv_get_capture_mode (zapping->info))
    {
      TeletextView *view;

      view = (TeletextView *) drawing_area;
      if (view->key_press (view, event))
	return TRUE;
    }
#endif

  return on_user_key_press (widget, event, NULL)
    || on_channel_key_press (widget, event, NULL);
}

static gboolean
on_button_press			(GtkWidget *		widget,
				 GdkEventButton *	event)
{
  switch (event->button)
    {
    case 2: /* Middle button */
      python_command (widget, "zapping.switch_mode('window')");
      return TRUE; /* handled */

    case 3: /* Right button */
      zapping_create_popup (zapping, event);
      return TRUE; /* handled */

    default:
      break;
    }

  return FALSE; /* pass on */
}

static gboolean
on_scroll_event			(GtkWidget *		widget,
				 GdkEventScroll *	event)
{
  switch (event->direction)
    {
    case GDK_SCROLL_UP:
    case GDK_SCROLL_LEFT:
      python_command (widget, "zapping.channel_up()");
      return TRUE; /* handled */

    case GDK_SCROLL_DOWN:
    case GDK_SCROLL_RIGHT:
      python_command (widget, "zapping.channel_down()");
      return TRUE; /* handled */

    default:
      break;
    }

  return FALSE; /* pass on */
}

/* Called when OSD changes the geometry of the pieces */
static void
osd_model_changed			(ZModel	*	ignored1 _unused_,
					 tveng_device_info *info)
{
  const tv_window *w;
  tv_clip_vector clip_vector;
  GdkWindow *gdk_window;

  if (tv_get_controller (info) == TVENG_CONTROLLER_XV ||
      !black_window || !black_window->window)
    return;

  tv_enable_overlay (info, FALSE);

  tv_clip_vector_init (&clip_vector);

  gdk_window = GTK_BIN (black_window)->child->window;

  w = tv_cur_overlay_window (info);

  x11_window_clip_vector (&clip_vector,
			  GDK_WINDOW_XDISPLAY (gdk_window),
			  GDK_WINDOW_XID (gdk_window),
			  w->x,
			  w->y,
			  w->width,
			  w->height);

  tv_set_overlay_window_clipvec (info, w, &clip_vector);

  tv_clip_vector_destroy (&clip_vector);

  tv_enable_overlay (info, TRUE);
}

static void
on_cursor_blanked		(ZVideo *		video _unused_,
				 gpointer		user_data _unused_)
{
  /* Recenter the viewport */
  x11_vidmode_switch (svidmodes, screens, NULL, NULL);
}

static void
set_blank_timeout		(ZVideo *		video)
{
  gint timeout;

  timeout = 0; /* disabled */
  z_gconf_get_int (&timeout, "/apps/zapping/blank_cursor_timeout");

  z_video_blank_cursor (video, timeout);
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

  if (0)
    fprintf (stderr, "Using mode %ux%u@%u for video %ux%u\n",
	     v->width, v->height, (guint)(v->vfreq + 0.5),
	     width, height);

  return v;
}

static const tv_screen *
find_screen			(void)
{
  gint x;
  gint y;
  gint width;
  gint height;
  GdkWindow *window;
  const tv_screen *xs;

  window = GTK_WIDGET (zapping)->window;

  /* Root window relative position. */
  gdk_window_get_origin (window, &x, &y);

  gdk_window_get_geometry (window,
			   /* x */ NULL,
			   /* y */ NULL,
			   &width,
			   &height,
			   /* depth */ NULL);

  xs = tv_screen_list_find (screens,
			    x, y,
			    (guint) width,
			    (guint) height);
  if (!xs)
    xs = screens;

  return xs;
}

gboolean
stop_fullscreen			(void)
{
  g_assert (DISPLAY_MODE_FULLSCREEN == zapping->display_mode
	    || DISPLAY_MODE_BACKGROUND == zapping->display_mode);

  switch (tv_get_capture_mode (zapping->info))
    {
    case CAPTURE_MODE_OVERLAY:
      /* Error ignored */
      tv_enable_overlay (zapping->info, FALSE);

      g_signal_handlers_disconnect_matched
	(G_OBJECT (osd_model), G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
	 0, 0, NULL, G_CALLBACK (osd_model_changed), zapping->info);

      break;

    case CAPTURE_MODE_READ:
      if (!capture_stop ())
	return FALSE;
      break;

    case CAPTURE_MODE_TELETEXT:
      /* Nuk nuk. */
      break;

    default:
      g_assert_not_reached ();
    }

  zapping->display_mode = DISPLAY_MODE_WINDOW;
  tv_set_capture_mode (zapping->info, CAPTURE_MODE_NONE);

  x11_vidmode_restore (svidmodes, &old_vidmode);

  osd_unset_window ();

  /* Remove the black window */
  gtk_widget_destroy (black_window);
  black_window = NULL;

  x11_force_expose ((gint) screen->x,
		    (gint) screen->y,
		    screen->width,
		    screen->height);

  x11_vidmode_list_delete (svidmodes);
  svidmodes = NULL;

  gtk_widget_show (GTK_WIDGET (zapping));

  /* Undo "smart" placement by the WM. */
  gtk_window_move (GTK_WINDOW (zapping),
		   old_mw_x,
		   old_mw_y);

  return TRUE;
}

gboolean
start_fullscreen		(display_mode		dmode,
				 capture_mode		cmode)
{
  const tv_screen *xs;
  GtkWindow *window;
  GdkColor chroma;
  const gchar *vm_name;
  const x11_vidmode_info *vm;
  gint vx;
  gint vy;
  guint vwidth;
  guint vheight;
  GtkWidget *fixed;

#ifdef HAVE_LIBZVBI
  if (CAPTURE_MODE_TELETEXT == cmode)
    if (!_teletext_view_new
	|| !zvbi_get_object ())
      return FALSE;
#else
  if (CAPTURE_MODE_TELETEXT == cmode)
    return FALSE;
#endif

  xs = find_screen ();
  screen = xs;

  /* VidModes available on this screen. */
  svidmodes = x11_vidmode_list_new (NULL, xs->screen_number);

  CLEAR (chroma);

  black_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  window = GTK_WINDOW (black_window);

  gtk_widget_realize (black_window);
  gdk_window_set_decorations (black_window->window, 0);

  gtk_widget_set_size_request (black_window,
  			       (gint) xs->width,
 			       (gint) xs->height);

  gtk_widget_modify_bg (black_window, GTK_STATE_NORMAL, &chroma);

  gtk_window_add_accel_group
    (window, gtk_ui_manager_get_accel_group (zapping->ui_manager));


  fixed = gtk_fixed_new ();
  gtk_widget_show (fixed);
  gtk_container_add (GTK_CONTAINER (black_window), fixed);


  if (CAPTURE_MODE_TELETEXT == cmode)
    {
#ifdef HAVE_LIBZVBI
      /* Welcome to the magic world of encapsulation. */ 
      drawing_area = _teletext_view_new ();
#endif

      /* Gdk scaling routines hang if we start with the default 1, 1. */
      gtk_widget_set_size_request (drawing_area, 400, 300);

      /* For Python bindings. */
      g_object_set_data (G_OBJECT (black_window),
			 "TeletextView", drawing_area);      
    }
  else
    {
      drawing_area = z_video_new ();
      Z_VIDEO (drawing_area)->size_magic = FALSE;

      set_blank_timeout (Z_VIDEO (drawing_area));

      g_signal_connect (G_OBJECT (drawing_area), "cursor-blanked",
			G_CALLBACK (on_cursor_blanked), zapping->info);
    }

  gtk_widget_show (drawing_area);
  gtk_fixed_put (GTK_FIXED (fixed), drawing_area, 0, 0);

  gtk_widget_add_events (drawing_area,
			 GDK_BUTTON_PRESS_MASK |
			 GDK_BUTTON_RELEASE_MASK |
			 GDK_SCROLL_MASK |
			 GDK_KEY_PRESS_MASK);

  gtk_widget_modify_bg (drawing_area, GTK_STATE_NORMAL, &chroma);

  gtk_widget_show (black_window);

  if (CAPTURE_MODE_TELETEXT == cmode)
    {
      /* XXX TeletextView should already do this on realize, doesn't work??? */
      gdk_window_set_back_pixmap (drawing_area->window, NULL, FALSE);
    }

  /* Make sure we're on the right screen. */
  if (1)
    gtk_window_move (window, (gint) xs->x, (gint) xs->y);
  else /* debug */
    gtk_window_move (window, 700, 600);

  if (DISPLAY_MODE_BACKGROUND == dmode)
    {
      x11_window_below (window, TRUE);
    }
  else
    {
      /* This should span only one screen, with or without Xinerama,
	 if the WM has proper Xinerama support. */
      x11_window_fullscreen (window, TRUE);
    }

  gtk_window_present (window);


  switch (cmode)
    {
    case CAPTURE_MODE_OVERLAY:
      /* Make sure we use an Xv adaptor which can render into da->window.
	 (Won't help with X.org but it's the right thing to do.) */
      tveng_close_device(zapping->info);

      if (-1 == tveng_attach_device (zcg_char (NULL, "video_device"),
				     GDK_WINDOW_XWINDOW (drawing_area->window),
				     TVENG_ATTACH_XV,
				     zapping->info))
	{
	  ShowBox("Overlay mode not available:\n%s",
		  GTK_MESSAGE_ERROR, tv_get_errstr (zapping->info));
	  goto failure;
	}

      if (tv_get_controller (zapping->info) != TVENG_CONTROLLER_XV &&
	  (tv_get_caps (zapping->info)->flags & TVENG_CAPS_CHROMAKEY))
	{
	  chroma.blue = 0xffff;
      
	  if (gdk_colormap_alloc_color(gdk_colormap_get_system(), &chroma,
				       FALSE, TRUE))
	    {
	      z_set_window_bg (drawing_area, &chroma);
	      
	      gdk_colormap_free_colors (gdk_colormap_get_system(), &chroma, 1);
	    }
	  else
	    {
	      ShowBox("Couldn't allocate chromakey, chroma won't work",
		      GTK_MESSAGE_WARNING);
	    }
	}
      else if (tv_get_controller (zapping->info) == TVENG_CONTROLLER_XV)
	{
	  GdkColor chroma;
	  unsigned int chromakey;
	  
	  CLEAR (chroma);
	  
	  if (tv_get_overlay_chromakey (zapping->info, &chromakey))
	    {
	      chroma.pixel = chromakey;
	      z_set_window_bg (drawing_area, &chroma);
	    }
	}

      /* Disable double buffering just in case, will help when a
	 XV driver doesn't provide XV_COLORKEY but requires the colorkey
	 not to be overwritten */
      gtk_widget_set_double_buffered (drawing_area, FALSE);

      break;

    case CAPTURE_MODE_READ:
      tveng_close_device(zapping->info);

      if (-1 == tveng_attach_device (zcg_char (NULL, "video_device"),
				     GDK_WINDOW_XWINDOW (drawing_area->window),
				     TVENG_ATTACH_READ,
				     zapping->info))
	{
	  ShowBox("Capture mode not available:\n%s",
		  GTK_MESSAGE_ERROR, tv_get_errstr (zapping->info));
	  goto failure;
	}

      break;

    case CAPTURE_MODE_TELETEXT:
      /* Bktr driver needs special programming for VBI-only mode. */
      tveng_close_device (zapping->info);

      if (-1 == tveng_attach_device (zcg_char (NULL, "video_device"),
				     GDK_WINDOW_XWINDOW (drawing_area->window),
				     TVENG_ATTACH_VBI,
				     zapping->info))
	{
	  ShowBox ("Teletext mode not available.",
		   GTK_MESSAGE_ERROR);
	  goto failure;
	}

      break;

    default:
      goto failure;
    }

  /* Determine video size. XXX improve me. */
  if (CAPTURE_MODE_TELETEXT == cmode)
    {
      if ((vm_name = zcg_char (NULL, "fullscreen/vidmode"))
	  && 0 != strcmp (vm_name, "auto"))
	{
	  vm = x11_vidmode_by_name (svidmodes, vm_name);
	}
      else
	{
	  vm = NULL;
	}

      if (vm && x11_vidmode_switch (svidmodes, screens, vm, &old_vidmode))
	{
	  /* xs->width, height >= vm->width, height */

	  vwidth = vm->width;
	  vheight = vm->height;
	}
      else
	{
	  x11_vidmode_clear_state (&old_vidmode);

	  vwidth = xs->width;
	  vheight = xs->height;
	}
    }
  else
    {
      const tv_video_standard *vs;
      const struct tveng_caps *caps;

      if ((vs = tv_cur_video_standard (zapping->info)))
	{
	  vwidth = vs->frame_width;
	  vheight = vs->frame_height;
	}
      else
	{
	  vwidth = 640;
	  vheight = 480;
	}

      /* Driver limit. */
      caps = tv_get_caps (zapping->info);
      vwidth = MIN ((guint) caps->maxwidth, vwidth);
      vheight = MIN ((guint) caps->maxheight, vheight);

      /* Screen limit. XXX should try to scale captured images down. */
      vwidth = MIN (xs->width, vwidth);
      vheight = MIN (xs->height, vheight);

      if ((vm_name = zcg_char (NULL, "fullscreen/vidmode")))
	{
	  if (0 == strcmp (vm_name, "auto"))
	    {
	      vm = find_vidmode (svidmodes, vwidth, vheight);
	    }
	  else
	    {
	      vm = x11_vidmode_by_name (svidmodes, vm_name);
	    }
	}
      else
	{
	  vm = NULL;
	}
      
      if (vm && x11_vidmode_switch (svidmodes, screens, vm, &old_vidmode))
	{
	  /* xs->width, height >= vm->width, height <=> vwidth, vheight */
	}
      else
	{
	  x11_vidmode_clear_state (&old_vidmode);

	  if (CAPTURE_MODE_READ == cmode)
	    {
	      /* XXX should check scaling abilities. */
	      vwidth = xs->width;
	      vheight = xs->height;
	    }
	  else
	    {
	      /* xs->width, height >= vwidth, vheight */
	    }
	}
    }

  vx = (xs->width - vwidth) >> 1;
  vy = (xs->height - vheight) >> 1;

  if (0)
    fprintf (stderr, "vx=%u,%u vwidth=%u,%u xswidth=%u,%u\n",
	     vx, vy, vwidth, vheight, xs->width, xs->height);

  gtk_widget_set_size_request (drawing_area, (gint) vwidth, (gint) vheight);
  gtk_fixed_move (GTK_FIXED (fixed), drawing_area, vx, vy);

  switch (cmode)
    {
    case CAPTURE_MODE_OVERLAY:
      /* XXX */
      if (tv_get_controller (zapping->info) == TVENG_CONTROLLER_XV)
	{
	  /* Wait until da has the desired size. */
	  while (gtk_events_pending ())
	    gtk_main_iteration ();

	  if (!tv_set_overlay_xwindow (zapping->info,
				       GDK_WINDOW_XWINDOW (drawing_area->window),
				       GDK_GC_XGC (drawing_area->style->white_gc)))
	    goto failure;

	  /* For OSD. */
	  tv_overlay_hack (zapping->info, vx, vy, (int) vwidth, (int) vheight);
	}
      else
	{
	  tv_window window;
	  const tv_window *w;

	  if (!z_set_overlay_buffer (zapping->info, xs, drawing_area->window))
	    goto failure;

	  CLEAR (w);

	  window.x = vx;
	  window.y = vy;
	  window.width = vwidth;
	  window.height = vheight;

	  /* Set new capture dimensions */
	  if (!tv_set_overlay_window_clipvec (zapping->info, &window, NULL))
	    goto failure;

	  w = tv_cur_overlay_window (zapping->info);
	  if (w->width != vwidth
	      || w->height != vheight)
	    {
	      window.x = (xs->width - w->width) >> 1;
	      window.y = (xs->height - w->height) >> 1;

	      if (!tv_set_overlay_window_clipvec
		  (zapping->info, &window, NULL))
		goto failure;
	    }
	}

      /* Start preview */
      if (!tv_enable_overlay (zapping->info, TRUE))
	goto failure;

      break;

    case CAPTURE_MODE_READ:
      /* XXX error? */
      tveng_set_capture_size (vwidth, vheight, zapping->info);

      if (-1 == capture_start (zapping->info, GTK_WIDGET (drawing_area)))
	goto failure;

      break;

    case CAPTURE_MODE_TELETEXT:
      /* Nuk nuk. */
      break;

    default:
      goto failure;
    }

  gdk_window_get_origin (GTK_WIDGET (zapping)->window,
			 &old_mw_x,
			 &old_mw_y);

  gtk_widget_hide (GTK_WIDGET (zapping));

  zapping->display_mode = DISPLAY_MODE_FULLSCREEN;
  tv_set_capture_mode (zapping->info, cmode);

  gtk_widget_grab_focus (black_window);

  g_signal_connect (G_OBJECT (black_window), "key_press_event",
		    G_CALLBACK (on_key_press),
		    GTK_WIDGET (zapping));

  g_signal_connect (G_OBJECT (black_window), "button_press_event",
		    G_CALLBACK (on_button_press),
		    GTK_WIDGET (zapping));

  g_signal_connect (G_OBJECT (drawing_area), "button_press_event",
		    G_CALLBACK (on_button_press),
		    GTK_WIDGET (zapping));

  g_signal_connect (G_OBJECT (drawing_area), "scroll_event",
		    G_CALLBACK (on_scroll_event),
		    GTK_WIDGET (zapping));

  switch (cmode)
    {
      const tv_window *w;

    case CAPTURE_MODE_OVERLAY:
      w = tv_cur_overlay_window (zapping->info);

      if (tv_get_controller (zapping->info) != TVENG_CONTROLLER_XV)
	{
	  osd_set_coords (drawing_area,
			  w->x, w->y, (gint) w->width, (gint) w->height);
	}
      else
	{
	  /* wrong because Xv may pad to this size (DMA hw limit) */
	  osd_set_coords (drawing_area,
			  w->x, w->y, (gint) w->width, (gint) w->height);
	}

      g_signal_connect (G_OBJECT (osd_model), "changed",
			G_CALLBACK (osd_model_changed), zapping->info);

      break;

    case CAPTURE_MODE_READ:
      osd_set_window (GTK_WIDGET (drawing_area));
      break;

    case CAPTURE_MODE_TELETEXT:
      /* Nuk nuk. */
      break;

    default:
      break;
    }

  return TRUE;

 failure:
  x11_vidmode_list_delete (svidmodes);

  gtk_widget_destroy (black_window);

  return FALSE;
}
