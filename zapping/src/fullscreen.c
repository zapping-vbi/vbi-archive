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

/* $Id: fullscreen.c,v 1.31 2004-10-04 02:44:08 mschimek Exp $ */

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
#include "tveng_private.h"
#include "osd.h"
#include "x11stuff.h"
#include "interface.h"
#include "v4linterface.h"
#include "fullscreen.h"
#include "audio.h"
#include "plugins.h"
#include "zvideo.h"
#include "globals.h"

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

  if (CAPTURE_MODE_TELETEXT == zapping->info->capture_mode)
    if (_teletext_view_on_key_press (widget, event,
				     (TeletextView *)(drawing_area)))
      return TRUE;

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
  /* Recenter the viewport */
  x11_vidmode_switch (svidmodes, screens, NULL, NULL);
}

static void
set_blank_timeout		(ZVideo *		video)
{
  GError *error;
  guint timeout;

  error = NULL;

  timeout = (guint) gconf_client_get_int
    (gconf_client, "/apps/zapping/blank_cursor_timeout", &error);

  if (error)
    {
      timeout = 1500; /* ms */
      g_error_free (error);
    }

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

  if (zapping->info->debug_level > 0)
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

void
stop_fullscreen			(void)
{
  g_assert (DISPLAY_MODE_FULLSCREEN == zapping->display_mode
	    || DISPLAY_MODE_BACKGROUND == zapping->display_mode);

  switch (zapping->info->capture_mode)
    {
    case CAPTURE_MODE_OVERLAY:
      /* Error ignored */
      tveng_set_preview (FALSE, zapping->info);

      g_signal_handlers_disconnect_matched
	(G_OBJECT (osd_model), G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
	 0, 0, NULL, G_CALLBACK (osd_model_changed), zapping->info);

      break;

    case CAPTURE_MODE_READ:
      capture_stop();
      video_uninit ();
      tveng_stop_capturing(zapping->info);
      break;

    case CAPTURE_MODE_TELETEXT:
      /* Nuk nuk. */
      break;

    default:
      g_assert_not_reached ();
    }

  zapping->display_mode = DISPLAY_MODE_NONE;
  zapping->info->capture_mode = CAPTURE_MODE_NONE;

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

  if (CAPTURE_MODE_TELETEXT == cmode)
    if (!_teletext_view_new
	|| !zvbi_get_object ())
      return FALSE;

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


  fixed = gtk_fixed_new ();
  gtk_widget_show (fixed);
  gtk_container_add (GTK_CONTAINER (black_window), fixed);


  if (CAPTURE_MODE_TELETEXT == cmode)
    {
      /* Welcome to the magic world of encapsulation. */ 
      drawing_area = _teletext_view_new ();

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
		  GTK_MESSAGE_ERROR, zapping->info->error);
	  goto failure;
	}

      if (zapping->info->current_controller != TVENG_CONTROLLER_XV &&
	  (zapping->info->caps.flags & TVENG_CAPS_CHROMAKEY))
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
      else if (zapping->info->current_controller == TVENG_CONTROLLER_XV)
	{
	  GdkColor chroma;
	  
	  CLEAR (chroma);
	  
	  if (0 == tveng_get_chromakey (&chroma.pixel, zapping->info))
	    z_set_window_bg (drawing_area, &chroma);
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
		  GTK_MESSAGE_ERROR, zapping->info->error);
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
      if (zapping->info->cur_video_standard)
	{
	  vwidth = zapping->info->cur_video_standard->frame_width;
	  vheight = zapping->info->cur_video_standard->frame_height;
	}
      else
	{
	  vwidth = 640;
	  vheight = 480;
	}

      /* Driver limit. */
      vwidth = MIN ((guint) zapping->info->caps.maxwidth, vwidth);
      vheight = MIN ((guint) zapping->info->caps.maxheight, vheight);

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
      if (zapping->info->current_controller == TVENG_CONTROLLER_XV)
	{
	  /* Wait until da has the desired size. */
	  while (gtk_events_pending ())
	    gtk_main_iteration ();

	  if (!tv_set_overlay_xwindow (zapping->info,
				       GDK_WINDOW_XWINDOW (drawing_area->window),
				       GDK_GC_XGC (drawing_area->style->white_gc)))
	    goto failure;

	  /* For OSD. */
	  zapping->info->overlay_window.x = vx;
	  zapping->info->overlay_window.y = vy;
	  zapping->info->overlay_window.width = vwidth;
	  zapping->info->overlay_window.height = vheight;
	}
      else
	{
	  if (!z_set_overlay_buffer (zapping->info, xs, drawing_area->window))
	    goto failure;

	  zapping->info->overlay_window.x = vx;
	  zapping->info->overlay_window.y = vy;
	  zapping->info->overlay_window.width = vwidth;
	  zapping->info->overlay_window.height = vheight;

	  tv_clip_vector_clear (&zapping->info->overlay_window.clip_vector);

	  /* Set new capture dimensions */
	  if (-1 == tveng_set_preview_window (zapping->info))
	    goto failure;

	  if (zapping->info->overlay_window.width != vwidth
	      || zapping->info->overlay_window.height != vheight)
	    {
	      zapping->info->overlay_window.x =
		(xs->width - zapping->info->overlay_window.width) >> 1;
	      zapping->info->overlay_window.y =
		(xs->height - zapping->info->overlay_window.height) >> 1;

	      if (-1 == tveng_set_preview_window (zapping->info))
		goto failure;
	    }
	}

      /* Start preview */
      if (-1 == tveng_set_preview (TRUE, zapping->info))
	goto failure;

      break;

    case CAPTURE_MODE_READ:
      /* XXX error? */
      tveng_set_capture_size (vwidth, vheight, zapping->info);

      if (-1 == capture_start (zapping->info, GTK_WIDGET (drawing_area)))
	goto failure;

      video_init (GTK_WIDGET (drawing_area),
		  GTK_WIDGET (drawing_area)->style->black_gc);

      video_suggest_format ();

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
  zapping->info->capture_mode = cmode;

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
    case CAPTURE_MODE_OVERLAY:
      if (zapping->info->current_controller != TVENG_CONTROLLER_XV)
	{
	  osd_set_coords (drawing_area,
			  zapping->info->overlay_window.x,
			  zapping->info->overlay_window.y,
			  (gint) zapping->info->overlay_window.width,
			  (gint) zapping->info->overlay_window.height);
	}
      else
	{
	  /* wrong because Xv may pad to this size (DMA hw limit) */
	  osd_set_coords (drawing_area,
			  zapping->info->overlay_window.x,
			  zapping->info->overlay_window.y,
			  (gint) zapping->info->overlay_window.width,
			  (gint) zapping->info->overlay_window.height);
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
