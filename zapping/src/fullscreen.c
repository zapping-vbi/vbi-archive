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

/* $Id: fullscreen.c,v 1.53 2006-03-11 13:11:29 mschimek Exp $ */

/**
 * Fullscreen mode handling
 *
 * I wonder if we shouldn't simply switch the main window to
 * fullscreen...
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
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
#include "zstack.h"
#include "plugins/subtitle/view.h"
#include "subtitle.h"
#include "overlay.h"

extern gboolean was_fullscreen;

static x11_vidmode_info *	svidmodes;
static GtkWidget *		drawing_area;
static GtkWidget * 		black_window; /* The black window when you go
						 fullscreen */
static GtkWidget *		full_stack;
static SubtitleView *		subtitles;
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

  return on_channel_enter (widget, event, NULL)
    || on_user_key_press (widget, event, NULL)
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

static void
on_cursor_blanked		(ZVideo *		video _unused_,
				 gpointer		user_data _unused_)
{
  GtkWindow *window;

  if (0)
    {
      gint x;
      gint y;
      gint width;
      gint height;
      const tv_screen *s;

      /* Root window relative position. */
      gdk_window_get_origin (black_window->window, &x, &y);

      gdk_window_get_geometry (black_window->window,
			       /* x */ NULL,
			       /* y */ NULL,
			       &width,
			       &height,
			       /* depth */ NULL);

      s = tv_screen_list_find (screens,
			       x, y,
			       (guint) width,
			       (guint) height);

      if (NULL != s && screen != s)
	{
	  /* Moved to a different screen. */
	  /* TO DO:
	     - restore vidmode on old screen.
	     - find suitable vidmode on new screen.
	  */
	}
    }

  /* Recenter the window. */
  window = GTK_WINDOW (black_window);
  gtk_window_move (window, (gint) screen->x, (gint) screen->y);

  /* Recenter the viewport. */
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

GdkPixbuf *
fullscreen_get_subtitle_image	(GdkRectangle *		expose,
				 guint			width,
				 guint			height)
{
  if (subtitles)
    return subtitles->get_image (subtitles, expose, width, height);
  else
    return NULL;
}

static const gchar *
subfull_key = "/zapping/internal/subtitle/fullscreen";

gboolean
fullscreen_activate_subtitles	(gboolean		active)
{
  g_assert (NULL != full_stack);

  if (active)
    {
#ifdef HAVE_LIBZVBI
      if (_subtitle_view_new)
	{
	  if (!subtitles)
	    {
	      subtitles = (SubtitleView *) _subtitle_view_new ();

	      subt_set_position_from_config (subtitles, subfull_key);

	      gtk_widget_show (GTK_WIDGET (subtitles));

	      z_signal_connect_const
		(G_OBJECT (subtitles),
		 "z-position-changed",
		 G_CALLBACK (subt_store_position_in_config),
		 subfull_key);

	      g_signal_connect (G_OBJECT (subtitles), "destroy",
				G_CALLBACK (gtk_widget_destroyed),
				&subtitles);

	      z_stack_put (Z_STACK (full_stack),
			   GTK_WIDGET (subtitles),
			   ZSTACK_SUBTITLES);
	    }

	  subtitles->monitor_page (subtitles,
				   zvbi_caption_pgno);

	  if (CAPTURE_MODE_OVERLAY == tv_get_capture_mode (zapping->info))
	    {
	      const tv_window *w;

	      w = tv_cur_overlay_window (zapping->info);

	      subtitles->set_rolling (subtitles, FALSE);

	      subtitles->set_video_bounds (subtitles,
					   w->x, w->y, w->width, w->height);
	    }
	}
#endif /* HAVE_LIBZVBI */
    }
  else
    {
      if (subtitles)
	{
	  gtk_widget_destroy (GTK_WIDGET (subtitles));
	  subtitles = NULL;
	}
    }

  return TRUE;
}

gboolean
stop_fullscreen			(void)
{
  g_assert (DISPLAY_MODE_FULLSCREEN == zapping->display_mode
	    || DISPLAY_MODE_BACKGROUND == zapping->display_mode);

  if (subtitles)
    {
      gtk_widget_destroy (GTK_WIDGET (subtitles));
      subtitles = NULL;
    }

  switch (tv_get_capture_mode (zapping->info))
    {
    case CAPTURE_MODE_OVERLAY:
      stop_overlay ();
      break;

    case CAPTURE_MODE_READ:
      if (!capture_stop ())
	return FALSE;
      break;

    case CAPTURE_MODE_TELETEXT:
      /* Nothing to do. */
      break;

    default:
      g_assert_not_reached ();
    }

  zapping->display_mode = DISPLAY_MODE_WINDOW;
  zapping->display_window = GTK_WIDGET (zapping->video);
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


  full_stack = z_stack_new ();
  gtk_widget_show (full_stack);
  gtk_container_add (GTK_CONTAINER (black_window), full_stack);

  fixed = gtk_fixed_new ();
  gtk_widget_show (fixed);
  z_stack_put (Z_STACK (full_stack), fixed, ZSTACK_VIDEO);

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
      /* Nothing to do. */
      break;

    case CAPTURE_MODE_READ:
      if (-1 == tveng_attach_device (zcg_char (NULL, "video_device"),
				     GDK_WINDOW_XWINDOW (drawing_area->window),
				     TVENG_ATTACH_READ,
				     zapping->info))
	{
	  ShowBox("Capture mode not available:\n%s",
		  GTK_MESSAGE_ERROR, tv_get_errstr (zapping->info));
	  goto failure;
	}

      zconf_get_sources (zapping->info, /* mute */ FALSE);

      break;

    case CAPTURE_MODE_TELETEXT:
      /* Bktr driver needs special programming for VBI-only mode. */
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
      /* FIXME the capture (after ATTACH_READ) and overlay limits (after
         ATTACH_XV or tv_set_overlay_buffer()) may be different. */
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
      if (!start_overlay (black_window, drawing_area))
	goto failure;

      break;

    case CAPTURE_MODE_READ:
      {
	tv_image_format format;
	guint awidth;

	format = *tv_cur_capture_format (zapping->info);

	format.width = vwidth;
	format.height = vheight;

	/* XXX error? */
	tv_set_capture_format (zapping->info, &format);

	/* We asked for screen size images, which is likely
	   impossible. In case the driver has strange limits and
	   XvImage is unavailable to scale images let's try to
	   get closer to the desired aspect ratio. */

	format = *tv_cur_capture_format (zapping->info);

	awidth = format.height * 4 / 3;
	if (format.width != awidth)
	  {
	    format.width = awidth;
	    /* Error ignored. */
	    tv_set_capture_format (zapping->info, &format);
	  }

	if (!capture_start (zapping->info, GTK_WIDGET (drawing_area)))
	  goto failure;

	break;
      }

    case CAPTURE_MODE_TELETEXT:
      /* Nothing to do. */
      break;

    default:
      goto failure;
    }

  gdk_window_get_root_origin (GTK_WIDGET (zapping)->window,
			      &old_mw_x,
			      &old_mw_y);

  gtk_widget_hide (GTK_WIDGET (zapping));

  zapping->display_mode = DISPLAY_MODE_FULLSCREEN;
  zapping->display_window = GTK_WIDGET (drawing_area);
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
    case CAPTURE_MODE_OVERLAY:
      /* XXX wrong because XvPutImage may pad to this size (DMA hw limit) */
      osd_set_coords (drawing_area,
		      /* x, y */ 0, 0,
		      vwidth, vheight);
      break;

    case CAPTURE_MODE_READ:
      osd_set_window (GTK_WIDGET (drawing_area));
      break;

    case CAPTURE_MODE_TELETEXT:
      /* Nothing to do. */
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
