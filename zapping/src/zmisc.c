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

/* XXX gtk+ 2.3 GtkOptionMenu, GtkCombo, toolbar changes */
#undef GTK_DISABLE_DEPRECATED
#undef GDK_DISABLE_DEPRECATED

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "site_def.h"

#include <gdk/gdkx.h>

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>

#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zmisc.h"
#include "zconf.h"
#include "tveng.h"
#include "interface.h"
#include "x11stuff.h"
#include "overlay.h"
#include "capture.h"
#include "fullscreen.h"
#include "v4linterface.h"

#include "zvbi.h"
#include "osd.h"
#include "remote.h"
#include "keyboard.h"
#include "properties-handler.h"
#include "globals.h"
#include "audio.h"
#include "mixer.h"
#include "zvideo.h"

extern tveng_device_info * main_info;
extern volatile gboolean flag_exit_program;
extern gboolean xv_present;

gchar*
Prompt (GtkWidget *main_window, const gchar *title,
	const gchar *prompt,  const gchar *default_text)
{
  GtkWidget * dialog;
  GtkBox *vbox;
  GtkWidget *label, *entry;
  gchar *buffer = NULL;

  dialog = gtk_dialog_new_with_buttons
    (title, GTK_WINDOW (main_window),
     GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
     GTK_STOCK_OK, GTK_RESPONSE_OK,
     NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  vbox = GTK_BOX (GTK_DIALOG (dialog) -> vbox);

  if (prompt)
    {
      label = gtk_label_new (prompt);
      gtk_box_pack_start_defaults (vbox, label);
      gtk_widget_show(label);
    }
  entry = gtk_entry_new();
  gtk_box_pack_start_defaults(GTK_BOX(vbox), entry);
  gtk_widget_show(entry);
  gtk_widget_grab_focus(entry);
  if (default_text)
    {
      gtk_entry_set_text (GTK_ENTRY(entry), default_text);
      gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
    }

  z_entry_emits_response (entry, GTK_DIALOG (dialog),
			  GTK_RESPONSE_OK);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    buffer = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));

  gtk_widget_destroy(dialog);
  
  return buffer;
}

GtkWidget * z_gtk_pixmap_menu_item_new(const gchar * mnemonic,
				       const gchar * icon)
{
  GtkWidget * imi;
  GtkWidget * image;

  imi = gtk_image_menu_item_new_with_mnemonic (mnemonic);

  if (icon)
    {
      image = gtk_image_new_from_stock (icon, GTK_ICON_SIZE_MENU);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (imi),
				     image);
      /* not sure whether this is necessary, but won't harm */
      gtk_widget_show (image);
    }

  return (imi);
}

/*
 *  Zapping Global Tooltips
 */

static GList *			tooltips_list = NULL;
static GtkTooltips *		tooltips_default = NULL;
static gboolean			tooltips_enabled = TRUE;

static void
tooltips_destroy_notify		(gpointer	data,
				 GObject	*where_the_object_was _unused_)
{
  g_list_remove (tooltips_list, data);
}

GtkTooltips *
z_tooltips_add			(GtkTooltips *		tips)
{
  if (!tips)
    tips = gtk_tooltips_new (); /* XXX destroy at exit */

  tooltips_list = g_list_append (tooltips_list, (gpointer) tips);

  g_object_weak_ref (G_OBJECT (tips),
		     tooltips_destroy_notify,
		     (gpointer) tips);

  if (tooltips_enabled)
    gtk_tooltips_enable (tips);
  else
    gtk_tooltips_disable (tips);

  return tips;
}

void
z_tooltips_active		(gboolean		enable)
{
  GList *list;

  tooltips_enabled = enable;

  for (list = tooltips_list; list; list = list->next)
    {
      if (enable)
	gtk_tooltips_enable (GTK_TOOLTIPS (list->data));
      else
	gtk_tooltips_disable (GTK_TOOLTIPS (list->data));
    }
}

void
z_tooltip_set			(GtkWidget *		widget,
				 const gchar *		tip_text)
{
  if (!tooltips_default)
    tooltips_default = z_tooltips_add (NULL);

#ifndef ZMISC_TOOLTIP_WARNING
#define ZMISC_TOOLTIP_WARNING 0
#endif

  if (ZMISC_TOOLTIP_WARNING && GTK_WIDGET_NO_WINDOW(widget))
    fprintf(stderr, "Warning: tooltip <%s> for "
            "widget without window\n", tip_text);

  gtk_tooltips_set_tip (tooltips_default, widget, tip_text, "private tip");
}

GtkWidget *
z_tooltip_set_wrap		(GtkWidget *		widget,
				 const gchar *		tip_text)
{
  if (!tooltips_default)
    tooltips_default = z_tooltips_add (NULL);

  if (GTK_WIDGET_NO_WINDOW(widget))
    {
      GtkWidget *event_box = gtk_event_box_new ();

      gtk_widget_show (widget);
      gtk_container_add (GTK_CONTAINER (event_box), widget);
      widget = event_box;
    }

  gtk_tooltips_set_tip (tooltips_default, widget, tip_text, "private tip");

  return widget;
}

void
z_set_sensitive_with_tooltip	(GtkWidget *		widget,
				 gboolean		sensitive,
				 const gchar *		on_tip,
				 const gchar *		off_tip)
{
  const gchar *new_tip;

  if (!tooltips_default)
    tooltips_default = z_tooltips_add (NULL);

  gtk_widget_set_sensitive (widget, sensitive);

  new_tip = sensitive ? on_tip : off_tip; /* can be NULL */

  gtk_tooltips_set_tip (tooltips_default, widget, new_tip, NULL);
}

/**************************************************************************/

/* main.c */
extern gboolean
on_zapping_key_press            (GtkWidget *            widget,
                                 GdkEventKey *          event,
                                 gpointer *             user_data);

static gboolean
on_key_press                    (GtkWidget *            widget,
                                 GdkEventKey *          event,
                                 TeletextView *         view)
{
  return (_teletext_view_on_key_press (widget, event, view)
          || on_user_key_press (widget, event, NULL)
          || on_picture_size_key_press (widget, event, NULL));
}

static gboolean
on_button_press			(GtkWidget *		widget _unused_,
				 GdkEventButton *	event,
				 Zapping *		z)
{
  switch (event->button)
    {
    case 3: /* Right button */
      zapping_create_popup (z, event);
      return TRUE; /* handled */

    default:
      /* TeletextView handles. */
      break;
    }

  return FALSE; /* pass on */
}

static void
stop_teletext			(void)
{

#ifdef HAVE_LIBZVBI

  TeletextView *view;
  BonoboDockItem *dock_item;
	
  /* Teletext in main window */

  view = _teletext_view_from_widget (GTK_WIDGET (zapping));
 
  /* Unredirect key-press-event */
  g_signal_handlers_disconnect_matched
    (G_OBJECT (zapping), G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
     0, 0, NULL, G_CALLBACK (on_key_press), view);
	
  g_signal_handlers_unblock_matched
    (G_OBJECT (zapping), G_SIGNAL_MATCH_FUNC,
     0, 0, NULL, G_CALLBACK (on_zapping_key_press), NULL);

  dock_item = gnome_app_get_dock_item_by_name (&zapping->app,
					       "teletext-toolbar");
  gtk_widget_destroy (GTK_WIDGET (dock_item));
	
  gtk_widget_destroy (GTK_WIDGET (view));
  g_object_set_data (G_OBJECT (zapping), "TeletextView", NULL);
	
  gtk_widget_show (GTK_WIDGET (zapping->video));

#endif

}

static gboolean
start_teletext			(void)
{

#ifdef HAVE_LIBZVBI

  TeletextView *view;
  GtkWidget *widget;
  gint width;
  gint height;
  BonoboDockItemBehavior behaviour;

  if (!_teletext_view_new
      || !zvbi_get_object ())
    return FALSE;

  /* Bktr driver needs special programming for VBI-only mode. */
  if (zapping->info->current_controller != TVENG_CONTROLLER_NONE)
    tveng_close_device (zapping->info);
  if (-1 == tveng_attach_device (zcg_char (NULL, "video_device"),
				 GDK_WINDOW_XWINDOW
				 (GTK_WIDGET (zapping->video)->window),
				 TVENG_ATTACH_VBI, zapping->info))
    {
      ShowBox ("Teletext mode not available.",
	       GTK_MESSAGE_ERROR);
      return FALSE;
    }

  view = (TeletextView *) _teletext_view_new ();
  gtk_widget_show (GTK_WIDGET (view));
	  
  g_object_set_data (G_OBJECT (zapping), "TeletextView", view);
	  
  gtk_widget_add_events (GTK_WIDGET (zapping), GDK_KEY_PRESS_MASK);
	  
  /* Redirect key-press-event */
  g_signal_handlers_block_matched
    (G_OBJECT (zapping), G_SIGNAL_MATCH_FUNC,
     0, 0, NULL, G_CALLBACK (on_zapping_key_press), NULL);
	  
  g_signal_connect (G_OBJECT (zapping), "key_press_event",
		    G_CALLBACK (on_key_press), view);
  g_signal_connect (G_OBJECT (zapping), "button_press_event",
		    G_CALLBACK (on_button_press), zapping);

  widget = _teletext_toolbar_new (view->action_group);
  gtk_widget_show (widget);

  view->toolbar = (TeletextToolbar *) widget;

  behaviour = BONOBO_DOCK_ITEM_BEH_EXCLUSIVE;
  if (!gconf_client_get_bool
      (gconf_client, "/desktop/gnome/interface/toolbar_detachable", NULL))
    behaviour |= BONOBO_DOCK_ITEM_BEH_LOCKED;

  gnome_app_add_toolbar (&zapping->app,
			 GTK_TOOLBAR (widget),
			 "teletext-toolbar",
			 behaviour,
			 BONOBO_DOCK_TOP,
			 /* band_num */ 2,
			 /* band_position */ 0,
			 /* offset */ 0);

  zapping_view_appbar (zapping, TRUE);

  gtk_widget_hide (GTK_WIDGET (zapping->video));

  gtk_widget_queue_resize (GTK_WIDGET (zapping));
	  
  gtk_container_add (GTK_CONTAINER (zapping->contents),
		     GTK_WIDGET (view));
  
  gdk_window_get_geometry (GTK_WIDGET (view)->window,
			   /* x */ NULL,
			   /* y */ NULL,
			   &width,
			   &height,
			   /* depth */ NULL);
	  
  if (width > 10 && height > 10)
    {
      resize_ttx_page (view->zvbi_client_id, width, height);
      render_ttx_page (view->zvbi_client_id,
		       GTK_WIDGET (view)->window,
		       GTK_WIDGET (view)->style->white_gc,
		       0, 0, 0, 0, width, height);
    }

  return TRUE;

#else

  return FALSE;

#endif

}

void
z_set_window_bg			(GtkWidget *		widget,
				 GdkColor *		color)
{
  GdkRectangle rect;

  gtk_widget_modify_bg (widget, GTK_STATE_NORMAL, color);

  rect.x = 0;
  rect.y = 0;
  rect.width = widget->allocation.width;
  rect.height = widget->allocation.height;

  gdk_window_invalidate_rect (widget->window, &rect, /* children */ FALSE);
  gdk_window_process_updates (widget->window, /* children */ FALSE);
}

int
zmisc_restore_previous_mode(tveng_device_info * info)
{
  display_mode dmode;
  capture_mode cmode;

  from_old_tveng_capture_mode (&dmode, &cmode,
			       zcg_int(NULL, "previous_mode"));

  return zmisc_switch_mode(dmode, cmode, info);
}

void
zmisc_stop (tveng_device_info *info)
{
  if (CAPTURE_MODE_NONE == info->capture_mode)
    return;

  /* Stop current capture mode */
  switch (((int) zapping->display_mode) | (int) info->capture_mode)
    {
    case DISPLAY_MODE_FULLSCREEN | CAPTURE_MODE_READ:
    case DISPLAY_MODE_FULLSCREEN | CAPTURE_MODE_OVERLAY:
    case DISPLAY_MODE_FULLSCREEN | CAPTURE_MODE_TELETEXT:
    case DISPLAY_MODE_BACKGROUND | CAPTURE_MODE_READ:
    case DISPLAY_MODE_BACKGROUND | CAPTURE_MODE_OVERLAY:
    case DISPLAY_MODE_BACKGROUND | CAPTURE_MODE_TELETEXT:
      stop_fullscreen ();
      break;

    case DISPLAY_MODE_WINDOW | CAPTURE_MODE_READ:
    case CAPTURE_MODE_READ:
      capture_stop();
      video_uninit ();
      tveng_stop_capturing(info);
      break;

    case DISPLAY_MODE_WINDOW | CAPTURE_MODE_OVERLAY:
      stop_overlay ();
      break;

    case DISPLAY_MODE_WINDOW | CAPTURE_MODE_TELETEXT:
      stop_teletext ();
      zapping->info->capture_mode = CAPTURE_MODE_NONE; /* ugh */
      break;

    default:
      break;
    }

  {
    GdkColor color;

    CLEAR (color);

    z_set_window_bg (GTK_WIDGET (zapping->video), &color);
  }
}

/*
  does the mode switching. Since this requires more than just using
  tveng, a new routine is needed.
  Returns whatever tveng returns, but we print the message ourselves
  too, so no need to aknowledge it to the user.
*/
int
zmisc_switch_mode(display_mode new_dmode,
		  capture_mode new_cmode,
		  tveng_device_info * info)
{
  int return_value = 0;
  gint x, y, w, h;
  gchar * old_input = NULL;
  gchar * old_standard = NULL;
  display_mode old_dmode;
  capture_mode old_cmode;
  extern int disable_overlay;
  gint muted;
  GError *error;
  guint timeout;

  g_assert(info != NULL);
  g_assert(zapping->video != NULL);

  if (0)
    fprintf (stderr, "%s: %d %d -> %d %d\n",
	     __FUNCTION__,
	     zapping->display_mode, info->capture_mode,
	     new_dmode, new_cmode);

  error = NULL;

  timeout = (guint) gconf_client_get_int
    (gconf_client, "/apps/zapping/blank_cursor_timeout", &error);

  if (error)
    {
      timeout = 1500; /* ms */
      g_error_free (error);
    }

  if (zapping->display_mode == new_dmode
      && info->capture_mode == new_cmode)
    switch (new_cmode)
      {
      case CAPTURE_MODE_NONE:
      case CAPTURE_MODE_TELETEXT:
	break;
      default:
	x11_screensaver_set (X11_SCREENSAVER_DISPLAY_ACTIVE);
	z_video_blank_cursor (zapping->video, timeout);
	return 0; /* success */
      }

  /* save this input name for later retrieval */
  if (info->cur_video_input)
    old_input = g_strdup (info->cur_video_input->label);
  if (info->cur_video_standard)
    old_standard = g_strdup(info->cur_video_standard->label);

  {
    GdkWindow *window;

    window = GTK_WIDGET (zapping->video)->window;
    gdk_window_get_geometry(window, NULL, NULL, &w, &h, NULL);
    gdk_window_get_origin(window, &x, &y);
  }

#if 0 /* XXX should use quiet_set, but that can't handle yet
         how the controls are rebuilt when switching btw
         v4l <-> xv. */
  /* Always: if ((avoid_noise = zcg_bool (NULL, "avoid_noise"))) */
    tv_quiet_set (main_info, TRUE);
#else
  muted = tv_mute_get (zapping->info, FALSE);
#endif

  old_dmode = zapping->display_mode;
  old_cmode = info->capture_mode;

  zmisc_stop (info);

#ifdef HAVE_LIBZVBI
  if (!flag_exit_program)
    {
      GtkAction *action;

      if (new_dmode == DISPLAY_MODE_WINDOW
	  && new_cmode == CAPTURE_MODE_TELETEXT)
	{
	  action = gtk_action_group_get_action (zapping->vbi_action_group,
						"Teletext");
	  z_action_set_visible (action, FALSE);

	  action = gtk_action_group_get_action (zapping->vbi_action_group,
						"RestoreVideo");
	  z_action_set_visible (action, TRUE);
	}
      else
	{
	  action = gtk_action_group_get_action (zapping->vbi_action_group,
						"RestoreVideo");
	  z_action_set_visible (action, FALSE);

	  action = gtk_action_group_get_action (zapping->vbi_action_group,
						"Teletext");
	  z_action_set_visible (action, TRUE);

	  zapping_view_appbar (zapping, FALSE);
	}
    }
#endif /* HAVE_LIBZVBI */

  if (CAPTURE_MODE_NONE == new_cmode
      || CAPTURE_MODE_TELETEXT == new_cmode)
    {
      python_command_printf (GTK_WIDGET (zapping), "zapping.closed_caption(0)");
      osd_clear();
      osd_unset_window();
    }
  else if (DISPLAY_MODE_WINDOW == new_dmode)
    {
      osd_set_window(GTK_WIDGET (zapping->video));
    }

  switch (((int) new_dmode) | (int) new_cmode)
    {
    case DISPLAY_MODE_WINDOW | CAPTURE_MODE_READ:
      if (info->attach_mode == TVENG_ATTACH_VBI ||
	  info->attach_mode == TVENG_ATTACH_CONTROL ||
	  info->current_controller == TVENG_CONTROLLER_XV ||
	  info->current_controller == TVENG_CONTROLLER_NONE)
	{
	  if (info->current_controller != TVENG_CONTROLLER_NONE)
	    tveng_close_device(info);
	  if (-1 == tveng_attach_device
	      (zcg_char(NULL, "video_device"),
	       GDK_WINDOW_XID (GTK_WIDGET (zapping->video)->window),
	       TVENG_ATTACH_READ, info))
	    {
	      /* Try restoring as XVideo, error ignored. */
	      tveng_attach_device
		(zcg_char(NULL, "video_device"),
		 GDK_WINDOW_XID (GTK_WIDGET (zapping->video)->window),
		 TVENG_ATTACH_XV, info);

	      ShowBox("Capture mode not available:\n%s",
		      GTK_MESSAGE_ERROR, info->error);

	      goto failure;
	    }
	}

      /* XXX error? */
      tveng_set_capture_size((guint)w, (guint)h, info);
      return_value = capture_start(info, GTK_WIDGET (zapping->video));
      video_init (GTK_WIDGET (zapping->video),
		  GTK_WIDGET (zapping->video)->style->black_gc);
      video_suggest_format ();
      x11_screensaver_set (X11_SCREENSAVER_DISPLAY_ACTIVE
			   | X11_SCREENSAVER_CPU_ACTIVE);
      z_video_blank_cursor (zapping->video, timeout);
      break;

    case DISPLAY_MODE_WINDOW | CAPTURE_MODE_OVERLAY:
      if (disable_overlay) {
	ShowBox("preview has been disabled", GTK_MESSAGE_WARNING);
	goto failure;
      }

      if (start_overlay ())
	{
	  x11_screensaver_set (X11_SCREENSAVER_DISPLAY_ACTIVE);
          z_video_blank_cursor (zapping->video, timeout);
	}
      else
	{
	  ShowBox (_("Cannot start video overlay.\n%s"),
		   GTK_MESSAGE_ERROR, info->error);
	  zmisc_stop (info);
	  goto failure;
	}
      break;

    case DISPLAY_MODE_WINDOW | CAPTURE_MODE_TELETEXT:
      x11_screensaver_set (X11_SCREENSAVER_ON);
      z_video_blank_cursor (zapping->video, 0);

      if (!start_teletext ())
	{
	  ShowBox(_("VBI has been disabled, or it doesn't work."),
		  GTK_MESSAGE_INFO);
	  zmisc_stop (info);
	  goto failure;
	}

      zapping->info->capture_mode = CAPTURE_MODE_TELETEXT; /* ugh */

      break;

    case DISPLAY_MODE_FULLSCREEN | CAPTURE_MODE_READ:
    case DISPLAY_MODE_FULLSCREEN | CAPTURE_MODE_OVERLAY:
    case DISPLAY_MODE_FULLSCREEN | CAPTURE_MODE_TELETEXT:
    case DISPLAY_MODE_BACKGROUND | CAPTURE_MODE_READ:
    case DISPLAY_MODE_BACKGROUND | CAPTURE_MODE_OVERLAY:
    case DISPLAY_MODE_BACKGROUND | CAPTURE_MODE_TELETEXT:
      if (disable_overlay) {
	ShowBox ("Preview has been disabled", GTK_MESSAGE_WARNING);
	goto failure;
      }

      if (start_fullscreen (new_dmode, new_cmode))
	{
	  if (CAPTURE_MODE_TELETEXT != new_cmode)
	    {
	      if (CAPTURE_MODE_OVERLAY == new_cmode)
		x11_screensaver_set (X11_SCREENSAVER_DISPLAY_ACTIVE);
	      else
		x11_screensaver_set (X11_SCREENSAVER_DISPLAY_ACTIVE |
				     X11_SCREENSAVER_CPU_ACTIVE);
	    }
	}
      else
	{
	  ShowBox (_("Cannot start fullscreen overlay.\n%s"),
		   GTK_MESSAGE_ERROR, info->error);
	  /* XXX s/overlay//. */
	  zmisc_stop (info);
	  goto failure;
	}
      break;

    default: /* TVENG_NO_CAPTURE */
      x11_screensaver_set (X11_SCREENSAVER_ON);
      z_video_blank_cursor (zapping->video, 0);

      break;
    }

  /* Restore old input if we found it earlier */
  /*
  if (old_input != NULL)
    if (-1 == tveng_set_input_by_name(old_input, info))
      g_warning("couldn't restore old input");
  if (old_standard != NULL)
    if (-1 == tveng_set_standard_by_name(old_standard, info))
      g_warning("couldn't restore old standard");
  */
  if (old_input != NULL)
    tveng_set_input_by_name(old_input, info);

  if (old_standard != NULL)
    tveng_set_standard_by_name(old_standard, info);

  g_free (old_input);
  g_free (old_standard);

  if (old_cmode != new_cmode
      || old_dmode != new_dmode)
    {
      zapping->display_mode = new_dmode;

      zcs_int ((int) to_old_tveng_capture_mode (old_dmode, old_cmode),
	       "previous_mode");

      if (old_cmode != new_cmode)
	last_cmode = old_cmode;
    }

  /* Update the standards, channels, etc */
  zmodel_changed(z_input_model);
  /* Updating the properties is not so useful, and it isn't so easy,
     since there might be multiple properties dialogs open */

#if 0
  /* XXX don't reset when we're in shutdown, see cmd.c/py_quit(). */
  if (avoid_noise && !flag_exit_program)
    reset_quiet (zapping->info, /* delay ms */ 300);
#else
  if (muted != -1)
    tv_mute_set (zapping->info, muted);
#endif

  /* Update the controls window if it's open */
  update_control_box(info);

  /* Find optimum size for widgets */
  gtk_widget_queue_resize(GTK_WIDGET (zapping));

  return return_value;

 failure:
  g_free (old_input);
  g_free (old_standard);

  x11_screensaver_set (X11_SCREENSAVER_ON);

  z_video_blank_cursor (zapping->video, 0);

  return -1;
}

void set_stock_pixmap	(GtkWidget	*button,
			 const gchar	*new_pix)
{
  gtk_button_set_use_stock (GTK_BUTTON (button), TRUE);
  gtk_button_set_label (GTK_BUTTON (button), new_pix);
}

/**
 * Just like gdk_pixbuf_copy_area but does clipping.
 */
void
z_pixbuf_copy_area		(GdkPixbuf	*src_pixbuf,
				 gint		src_x,
				 gint		src_y,
				 gint		width,
				 gint		height,
				 GdkPixbuf	*dest_pixbuf,
				 gint		dest_x,
				 gint		dest_y)
{
  gint src_w = gdk_pixbuf_get_width(src_pixbuf);
  gint src_h = gdk_pixbuf_get_height(src_pixbuf);
  gint dest_w = gdk_pixbuf_get_width(dest_pixbuf);
  gint dest_h = gdk_pixbuf_get_height(dest_pixbuf);

  if (src_x < 0)
    {
      width += src_x;
      dest_x -= src_x;
      src_x = 0;
    }
  if (src_y < 0)
    {
      height += src_y;
      dest_y -= src_y;
      src_y = 0;
    }

  if (src_x + width > src_w)
    width = src_w - src_x;
  if (src_y + height > src_h)
    height = src_h - src_y;

  if (dest_x < 0)
    {
      src_x -= dest_x;
      width += dest_x;
      dest_x = 0;
    }
  if (dest_y < 0)
    {
      src_y -= dest_y;
      height += dest_y;
      dest_y = 0;
    }

  if (dest_x + width > dest_w)
    width = dest_w - dest_x;
  if (dest_y + height > dest_h)
    height = dest_h - dest_y;

  if ((width <= 0) || (height <= 0))
    return;

  gdk_pixbuf_copy_area(src_pixbuf, src_x, src_y, width, height,
		       dest_pixbuf, dest_x, dest_y);
}

void
z_pixbuf_render_to_drawable	(GdkPixbuf	*pixbuf,
				 GdkWindow	*window,
				 GdkGC		*gc,
				 gint		x,
				 gint		y,
				 gint		width,
				 gint		height)
{
  gint w, h;

  if (!pixbuf)
    return;

  w = gdk_pixbuf_get_width(pixbuf);
  h = gdk_pixbuf_get_height(pixbuf);

  if (x < 0)
    {
      width += x;
      x = 0;
    }
  if (y < 0)
    {
      height += 0;
      y = 0;
    }

  if (x + width > w)
    width = w - x;
  if (y + height > h)
    height = h - y;

  if (width < 0 || height < 0)
    return;

  gdk_draw_pixbuf (window, gc,
		   pixbuf,
		   x, y,
		   x, y,
		   width, height,
		   GDK_RGB_DITHER_NORMAL,
		   x, y);
}

gint
z_menu_get_index		(GtkWidget	*menu,
				 GtkWidget	*item)
{
  gint return_value =
    g_list_index(GTK_MENU_SHELL(menu)->children, item);

  return return_value ? return_value : -1;
}

GtkWidget *
z_menu_shell_nth_item		(GtkMenuShell *		menu_shell,
				 guint			n)
{
  GList *list;

  list = g_list_nth (menu_shell->children, n);
  assert (list != NULL);

  return GTK_WIDGET (list->data);
}


gint
z_option_menu_get_active	(GtkWidget	*option_menu)
{
  return gtk_option_menu_get_history (GTK_OPTION_MENU (option_menu));
}

void
z_option_menu_set_active	(GtkWidget	*option_menu,
				 guint		index)
{
  gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), index);
}

static GtkAccelGroup *accel_group = NULL;

static void
change_pixmenuitem_label		(GtkWidget	*menuitem,
					 const gchar	*new_label)
{
  GtkWidget *widget = GTK_BIN(menuitem)->child;

  gtk_label_set_text(GTK_LABEL(widget), new_label);
}

void
z_change_menuitem			 (GtkWidget	*widget,
					  const gchar	*new_pixmap,
					  const gchar	*new_label,
					  const gchar	*new_tooltip)
{
  GtkWidget *image;

  if (new_label)
    change_pixmenuitem_label(widget, new_label);
  if (new_tooltip)
    z_tooltip_set(widget, new_tooltip);
  if (new_pixmap)
    {
      image = gtk_image_new_from_stock (new_pixmap, GTK_ICON_SIZE_MENU);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (widget),
				     image);
      /* not sure whether this is necessary, but won't harm */
      gtk_widget_show (image);
     }
}

static void
appbar_hide(GtkWidget *appbar _unused_)
{
  zapping_view_appbar (zapping, FALSE);
}

static void
add_hide (void)
{
  GtkWidget *old =
    g_object_get_data(G_OBJECT(zapping->appbar), "hide_button");
  GtkWidget *widget;

  if (old)
    return;

  widget = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  z_tooltip_set(widget, _("Hide the statusbar"));

  if (widget)
    gtk_box_pack_end(GTK_BOX(zapping->appbar), widget, FALSE, FALSE, 0);

  gtk_widget_show(widget);
  g_signal_connect_swapped(G_OBJECT(widget), "clicked",
			   G_CALLBACK(appbar_hide), NULL);

  g_object_set_data(G_OBJECT(zapping->appbar), "hide_button", widget);
}

static guint 		status_hide_timeout_id 		= 0;
static gboolean		status_hide			= FALSE;

static gint
status_hide_timeout		(void *			ignored _unused_)
{
  if (!zapping->appbar)
    return FALSE; /* don't call again */

  if (status_hide)
    {
      zapping_view_appbar (zapping, FALSE);
    }
  else /* just clean */
    {
      GtkWidget *status;

      status = gnome_appbar_get_status (zapping->appbar);
      gtk_label_set_text (GTK_LABEL (status), "");
    }

  status_hide_timeout_id = NO_SOURCE_ID;

  return FALSE; /* don't call again */
}

void
z_status_print			(const gchar *		message,
				 gboolean		markup,
				 guint			timeout,
				 gboolean		hide)
{
  GtkWidget *status;

  zapping_view_appbar (zapping, TRUE);

  status = gnome_appbar_get_status (zapping->appbar);

  add_hide ();

  if (markup)
    gtk_label_set_markup (GTK_LABEL (status), message);
  else
    gtk_label_set_text (GTK_LABEL (status), message);

  if (status_hide_timeout_id > 0)
    g_source_remove (status_hide_timeout_id);

  status_hide = hide;

  if (timeout > 0)
    status_hide_timeout_id =
      g_timeout_add (timeout, (GSourceFunc) status_hide_timeout, NULL);
  else
    status_hide_timeout_id = NO_SOURCE_ID;
}

/* FIXME: [Hide] button */
void z_status_set_widget(GtkWidget * widget)
{
  GtkWidget *old;

  zapping_view_appbar (zapping, TRUE);

  old = g_object_get_data(G_OBJECT(zapping->appbar), "old_widget");
  if (old)
    gtk_container_remove(GTK_CONTAINER(zapping->appbar), old);

  if (widget)
    gtk_box_pack_end(GTK_BOX(zapping->appbar), widget, FALSE, FALSE, 0);

  add_hide();

  g_object_set_data(G_OBJECT(zapping->appbar), "old_widget", widget);
}

/* XXX should use GError */
gboolean
z_build_path(const gchar *path, gchar **error_description)
{
  struct stat sb;
  gchar *b;
  guint i;

  if (!path || *path != '/')
    {
      /* FIXME */
      if (error_description)
	*error_description =
	  g_strdup(_("The path must start with /"));
      return FALSE;
    }
    
  for (i=1; path[i]; i++)
    if (path[i] == '/' || !path[i+1])
      {
	b = g_strndup(path, i+1);

	if (stat(b, &sb) < 0)
	  {
	    if (mkdir(b, S_IRUSR | S_IWUSR | S_IXUSR) < 0)
	      {
		if (error_description)
		  *error_description =
		    g_strdup_printf(_("Cannot create %s: %s"), b,
				    strerror(errno));
		g_free(b);
		return FALSE;
	      }
	    else
	      g_assert(stat(b, &sb) >= 0);
	  }

	if (!S_ISDIR(sb.st_mode))
	  {
	    if (error_description)
	      *error_description =
		g_strdup_printf(_("%s is not a directory"), b);
	    g_free(b);
	    return FALSE;
	  }

	g_free(b);
      }

  return TRUE;
}

static gchar *
strnmerge			(const gchar *		s1,
				 guint			len1,
				 const gchar *		s2,
				 guint			len2)
{
  gchar *d;

  d = g_malloc (len1 + len2 + 1);

  memcpy (d, s1, len1);
  memcpy (d + len1, s2, len2);

  d[len1 + len2] = 0;

  return d;
}

gchar *
z_replace_filename_extension	(const gchar *		filename,
				 const gchar *		new_ext)
{
  const gchar *ext;
  guint len;

  if (!filename)
    return NULL;

  len = strlen (filename);
  /* Last '.' in basename. UTF-8 safe because we scan for ASCII only. */
  for (ext = filename + len - 1;
       ext > filename && *ext != '.' && *ext != '/'; ext--);

  if (len == 0 || *ext != '.')
    {
      if (!new_ext)
	return g_strdup (filename);
      else
	return g_strconcat (filename, ".", new_ext, NULL);
    }

  len = ext - filename;

  if (new_ext)
    return strnmerge (filename, len + 1, new_ext, strlen (new_ext));
  else
    return g_strndup (filename, len);
}

static void
append_text			(GtkEditable *		e,
				 const gchar *		text)
{
  const gint end_pos = -1;
  gint old_pos, new_pos;

  gtk_editable_set_position (e, end_pos);
  old_pos = gtk_editable_get_position (e);
  new_pos = old_pos;

  gtk_editable_insert_text (e, text,
			    /* bytes */ (gint) strlen (text), &new_pos);

  /* Move cursor before appended text */
  gtk_editable_set_position (e, old_pos);
}

void
z_electric_set_basename		(GtkWidget *		w,
				 const gchar *		basename)
{
  g_assert (NULL != w);
  g_assert (NULL != basename);

  g_object_set_data_full (G_OBJECT (w), "basename",
			  g_strdup (basename),
			  (GtkDestroyNotify) g_free);
}

/* See ttx export or screenshot for a demo */
void
z_on_electric_filename		(GtkWidget *		w,
				 gpointer		user_data)
{
  const gchar *name;	/* editable: "/foo/bar.baz" */
  const gchar *ext;	/* editable: "baz" */
  gchar *basename;	/* proposed: "far.faz" */
  gchar *baseext;	/* proposed: "faz" */
  gchar **bpp;		/* copy entered name here */
  gint len;
  gint baselen;
  gint baseextlen;

  name = gtk_entry_get_text (GTK_ENTRY (w));
  len = strlen (name);

  ext = name;

  if (len > 0)
    {
      /* Last '/' in name. */
      for (ext = name + len - 1; ext > name && *ext != '/'; ext--)
	;

      /* First '.' in last part of name. */
      for (; *ext && *ext != '.'; ext++)
	;
    }

  basename = (gchar *) g_object_get_data (G_OBJECT (w), "basename");
  g_assert (basename != NULL);

  baselen = strlen (basename);
  /* Last '.' in basename. UTF-8 safe because we scan for ASCII only. */
  for (baseext = basename + baselen - 1;
       baseext > basename && *baseext != '.'; baseext--);
  baseextlen = (*baseext == '.') ? baselen - (baseext - basename) : 0;

  bpp = (gchar **) user_data;

  /* This function is usually a callback handler for the "changed"
     signal in a GtkEditable. Since we will change the editable too,
     block the signal emission while we are editing */
  g_signal_handlers_block_by_func (G_OBJECT (w), 
				   z_on_electric_filename,
				   user_data);

  /* Tack basename on if no name or ends with '/' */
  if (len == 0 || name[len - 1] == '/')
    {
      append_text (GTK_EDITABLE (w), basename);
    }
  /* Cut off basename if not prepended by '/' */
  else if (len > baselen
	   && 0 == strcmp (&name[len - baselen], basename)
	   && name[len - baselen - 1] != '/')
    {
      const gint end_pos = -1;
      gchar *buf = g_strndup (name, (guint)(len - baselen));

      gtk_entry_set_text (GTK_ENTRY (w), buf);

      /* Attach baseext if none already */
      if (baseextlen > 0 && ext < (name + len - baselen))
	append_text (GTK_EDITABLE (w), baseext);
      else
	gtk_editable_set_position (GTK_EDITABLE (w), end_pos);

      g_free (buf);
    }
#if 0
  /* Tack baseext on if name ends with '.' */
  else if (baseextlen > 0 && name[len - 1] == '.')
    {
      append_text (GTK_EDITABLE (w), baseext + 1);
    }
#endif
  /* Cut off baseext when duplicate */
  else if (baseextlen > 0 && len > baseextlen
	   && 0 == strcmp (&name[len - baseextlen], baseext)
	   && ext < (name + len - baseextlen))
    {
      gchar *buf = g_strndup (name, (guint)(len - baseextlen));

      gtk_entry_set_text (GTK_ENTRY (w), buf);

      g_free (buf);
    }

  if (bpp)
    {
      g_free (*bpp);

      *bpp = g_strdup (gtk_entry_get_text (GTK_ENTRY (w)));
    }

  g_signal_handlers_unblock_by_func (G_OBJECT (w), 
				     z_on_electric_filename,
				     user_data);
}

void
z_electric_replace_extension	(GtkWidget *		w,
				 const gchar *		ext)
{
  const gchar *old_name;
  gchar *old_base;
  gchar *new_name;

  old_base = (gchar *) g_object_get_data (G_OBJECT (w), "basename");

  if (NULL == old_base)
    return; /* has no extension */

  new_name = z_replace_filename_extension (old_base, ext);
  z_electric_set_basename (w, new_name);

  old_name = gtk_entry_get_text (GTK_ENTRY (w));
  new_name = z_replace_filename_extension (old_name, ext);
  
  g_signal_handlers_block_matched (G_OBJECT (w), G_SIGNAL_MATCH_FUNC,
				   0, 0, 0, z_on_electric_filename, 0);

  gtk_entry_set_text (GTK_ENTRY (w), new_name);

  g_signal_handlers_unblock_matched (G_OBJECT (w), G_SIGNAL_MATCH_FUNC,
				     0, 0, 0, z_on_electric_filename, 0);

  g_free (new_name);
}

static void
set_orientation_recursive	(GtkToolbar	*toolbar,
				 GtkOrientation orientation)
{
  GList *p = toolbar->children;
  GtkToolbarChild *child;

  while (p)
    {
      child = (GtkToolbarChild*)p->data;
      
      if (child->type == GTK_TOOLBAR_CHILD_WIDGET &&
	  GTK_IS_TOOLBAR(child->widget))
	set_orientation_recursive(GTK_TOOLBAR(child->widget), orientation);
      p = p->next;
    }

  gtk_toolbar_set_orientation(toolbar, orientation);
}

#if 0

static void
on_orientation_changed		(GtkToolbar	*toolbar,
				 GtkOrientation	orientation,
				 gpointer	data)
{
  GList *p;
  GtkToolbarChild *child;

  if (!toolbar)
    return;

  p = toolbar->children;

  while (p)
    {
      child = (GtkToolbarChild*)p->data;

      if (child->type == GTK_TOOLBAR_CHILD_WIDGET &&
	  GTK_IS_TOOLBAR(child->widget))
	set_orientation_recursive(GTK_TOOLBAR(child->widget), orientation);
      p = p->next;
    }  
}

#endif

void
z_toolbar_set_style_recursive	(GtkToolbar *		toolbar,
				 GtkToolbarStyle	style)
{
  GList *p;

  for (p = toolbar->children; p; p = p->next)
    {
      GtkToolbarChild *child = (GtkToolbarChild *) p->data;
      
      if (child->type == GTK_TOOLBAR_CHILD_WIDGET
	  && GTK_IS_TOOLBAR (child->widget))
	z_toolbar_set_style_recursive (GTK_TOOLBAR (child->widget), style);
    }

  switch (style)
    {
    case GTK_TOOLBAR_ICONS:
    case GTK_TOOLBAR_TEXT:
    case GTK_TOOLBAR_BOTH:
    case GTK_TOOLBAR_BOTH_HORIZ:
      gtk_toolbar_set_style (toolbar, style);
      break;

    default:
      gtk_toolbar_unset_style (toolbar);
      break;
    }
}

/*
static void
on_style_changed		(GtkToolbar	*toolbar,
				 GtkToolbarStyle style,
				 gpointer	data)
{
  GList *p;
  GtkToolbarChild *child;

  if (!toolbar)
    return;

  p = toolbar->children;

  while (p)
    {
      child = (GtkToolbarChild*)p->data;

      if (child->type == GTK_TOOLBAR_CHILD_WIDGET &&
	  GTK_IS_TOOLBAR(child->widget))
	z_toolbar_set_style_recursive(GTK_TOOLBAR(child->widget), style);
      p = p->next;
    }
}
*/

GtkWidget *
z_load_pixmap			(const gchar *		name)
{
  GtkWidget *image;
  gchar *path;

  path = g_strconcat (PACKAGE_PIXMAPS_DIR "/", name, NULL);
  image = gtk_image_new_from_file (path);
  g_free (path);

  gtk_widget_show (image);

  return image;
}

GtkWindow *
z_main_window		(void)
{
  return GTK_WINDOW(zapping);
}

gchar *
find_unused_name		(const gchar *		dir,
				 const gchar *		file,
				 const gchar *		ext)
{
  gchar *buf = NULL;
  gchar *name;
  const gchar *slash = "";
  const gchar *dot = "";
  gint index = 0;
  const gchar *s;
  gint n;
  struct stat sb;

  if (!dir)
    dir = "";
  else if (dir[0] && dir[strlen (dir) - 1] != '/')
    slash = "/";

  if (!file || !file[0])
    return g_strconcat (dir, slash, NULL);

  n = strlen (file);

  /* cut off existing extension from @file */
  for (s = file + n; s > file;)
    if (*--s == '.')
      {
	if (s == file || s[-1] == '/')
	  return g_strconcat (dir, slash, NULL);
	else
	  break;
      }

  if (s == file) /* has no extension */
    s = file + n;
  else if (!ext) /* no new ext, keep old */
    ext = s;

  /* parse off existing numeral suffix */
  for (n = 1; s > file && n < 10000000; n *= 10)
    if (isdigit(s[-1]))
      index += (*--s & 15) * n;
    else
      break;

  name = g_strndup (file, (guint)(s - file));

  if (!ext)
    ext = "";
  else if (ext[0] && ext[0] != '.')
    dot = ".";

  if (index == 0 && n == 1) /* had no suffix */
    {
      /* Try first without numeral suffix */
      buf = g_strdup_printf ("%s%s%s%s%s",
			     dir, slash, name, dot, ext);
      index = 2; /* foo, foo2, foo3, ... */
    }
  /* else fooN, fooN+1, fooN+2 */

  for (n = 10000; n > 0; n--) /* eventually abort */
    {
      if (!buf)
	buf = g_strdup_printf("%s%s%s%d%s%s",
			      dir, slash, name, index++, dot, ext);

      /* Try to query file availability */
      /*
       * Note: This is easy to break, but since there's no good(tm)
       * way to predict an available file name, just do the simple thing.
       */
      if (stat (buf, &sb) == -1)
	{
	  switch (errno)
	    {
	    case ENOENT:
	    case ENOTDIR:
	      /* take this */
	      break;

	    default:
	      /* give up */
	      g_free (buf);
	      buf = NULL;
	      break;
	    }

	  break;
	}
      else
	{
	  /* exists, try other */
	  g_free (buf);
	  buf = NULL;
	}
    }

  g_free (name);

  return buf ? buf : g_strconcat (dir, slash, NULL);
}

/*
 *  "Spinslider"
 *
 *  [12345___|<>] unit [-<>--------][Reset]
 */

typedef struct _z_spinslider z_spinslider;

struct _z_spinslider
{
  GtkWidget *		hbox;
  GtkAdjustment *	spin_adj;
  GtkAdjustment *	hscale_adj;

  gfloat		history[3];
  guint			reset_state;
  gboolean		in_reset;
};

static inline z_spinslider *
get_spinslider			(GtkWidget *		hbox)
{
  z_spinslider *sp;

  sp = g_object_get_data (G_OBJECT (hbox), "z_spinslider");
  g_assert (sp != NULL);

  return sp;
}

GtkAdjustment *
z_spinslider_get_spin_adj	(GtkWidget *		hbox)
{
  return get_spinslider (hbox)->spin_adj;
}

GtkAdjustment *
z_spinslider_get_hscale_adj	(GtkWidget *		hbox)
{
  return get_spinslider (hbox)->hscale_adj;
}

gfloat
z_spinslider_get_value		(GtkWidget *		hbox)
{
  return get_spinslider (hbox)->spin_adj->value;
}

void
z_spinslider_set_value		(GtkWidget *		hbox,
				 gfloat			value)
{
  z_spinslider *sp = get_spinslider (hbox);

  gtk_adjustment_set_value (sp->spin_adj, value);
  gtk_adjustment_set_value (sp->hscale_adj, value);
}

void
z_spinslider_set_reset_value	(GtkWidget *		hbox,
				 gfloat			value)
{
  z_spinslider *sp = get_spinslider (hbox);

  sp->history[sp->reset_state] = value;

  gtk_adjustment_set_value (sp->spin_adj, value);
  gtk_adjustment_set_value (sp->hscale_adj, value);
}

void
z_spinslider_adjustment_changed	(GtkWidget *		hbox)
{
  z_spinslider *sp = get_spinslider (hbox);

  sp->hscale_adj->value = sp->spin_adj->value;
  sp->hscale_adj->lower = sp->spin_adj->lower;
  sp->hscale_adj->upper = sp->spin_adj->upper + sp->spin_adj->page_size;
  sp->hscale_adj->step_increment = sp->spin_adj->step_increment;
  sp->hscale_adj->page_increment = sp->spin_adj->page_increment;
  sp->hscale_adj->page_size = sp->spin_adj->page_size;
  gtk_adjustment_changed (sp->spin_adj);
  gtk_adjustment_changed (sp->hscale_adj);
}

static void
on_z_spinslider_hscale_changed	(GtkWidget *		widget _unused_,
				 z_spinslider *		sp)
{
  if (sp->spin_adj->value != sp->hscale_adj->value)
    gtk_adjustment_set_value (sp->spin_adj, sp->hscale_adj->value);
}

static void
on_z_spinslider_spinbutton_changed (GtkWidget *		widget _unused_,
				    z_spinslider *	sp)
{
  if (!sp->in_reset)
    {
      if (sp->reset_state != 0)
	{
	  sp->history[0] = sp->history[1];
	  sp->history[1] = sp->history[2];
	  sp->reset_state--;
	}
      sp->history[2] = sp->spin_adj->value;
    }

  if (sp->spin_adj->value != sp->hscale_adj->value)
    gtk_adjustment_set_value (sp->hscale_adj, sp->spin_adj->value);
}

static void
on_z_spinslider_reset		(GtkWidget *		widget _unused_,
				 z_spinslider *		sp)
{
  gfloat current_value;

  current_value = sp->history[2];
  sp->history[2] = sp->history[1];
  sp->history[1] = sp->history[0];
  sp->history[0] = current_value;

  sp->in_reset = TRUE;

  gtk_adjustment_set_value (sp->spin_adj, sp->history[2]);
  gtk_adjustment_set_value (sp->hscale_adj, sp->history[2]);

  sp->in_reset = FALSE;

  if (sp->reset_state == 0
      && fabs (sp->history[0] - sp->history[1]) < 1e-6)
    sp->reset_state = 2;
  else
    sp->reset_state = (sp->reset_state + 1) % 3;
}

#include "pixmaps/reset.h"

GtkWidget *
z_spinslider_new		(GtkAdjustment *	spin_adj,
				 GtkAdjustment *	hscale_adj,
				 const gchar *		unit,
				 gfloat			reset,
				 gint			digits)
{
  z_spinslider *sp;

  g_assert (spin_adj != NULL);

  sp = g_malloc (sizeof (*sp));

  sp->spin_adj = spin_adj;
  sp->hscale_adj = hscale_adj;

  sp->hbox = gtk_hbox_new (FALSE, 0);
  g_object_set_data_full (G_OBJECT (sp->hbox), "z_spinslider", sp,
			  (GDestroyNotify) g_free);

  if (0)
    fprintf (stderr, "zss_new %f %f...%f  %f %f  %f  %d\n",
	     spin_adj->value, spin_adj->lower, spin_adj->upper,
	     spin_adj->step_increment, spin_adj->page_increment,
	     spin_adj->page_size, digits);

  /* Spin button */

  {
    GtkWidget *spinbutton;

    spinbutton = gtk_spin_button_new (sp->spin_adj,
				      sp->spin_adj->step_increment,
				      (guint) digits);
    gtk_widget_show (spinbutton);
    /* I don't see how to set "as much as needed", so hacking this up */
    gtk_widget_set_size_request (spinbutton, 80, -1);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (spinbutton),
				       GTK_UPDATE_IF_VALID);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbutton), TRUE);
    gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinbutton), TRUE);
    gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (spinbutton), TRUE);
    gtk_box_pack_start (GTK_BOX (sp->hbox), spinbutton, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (sp->spin_adj), "value-changed",
		      G_CALLBACK (on_z_spinslider_spinbutton_changed), sp);
  }

  /* Unit name */

  if (unit)
    {
      GtkWidget *label;

      label = gtk_label_new (unit);
      gtk_widget_show (label);
      gtk_box_pack_start (GTK_BOX (sp->hbox), label, FALSE, FALSE, 3);
    }

  /* Slider */

  /* Necessary to reach spin_adj->upper with slider */
  if (!hscale_adj)
    sp->hscale_adj = GTK_ADJUSTMENT (gtk_adjustment_new
	(sp->spin_adj->value,
	 sp->spin_adj->lower,
	 sp->spin_adj->upper + sp->spin_adj->page_size,
	 sp->spin_adj->step_increment,
	 sp->spin_adj->page_increment,
	 sp->spin_adj->page_size));

  {
    GtkWidget *hscale;

    hscale = gtk_hscale_new (sp->hscale_adj);
    /* Another hack */
    gtk_widget_set_size_request (hscale, 80, -1);
    gtk_widget_show (hscale);
    gtk_scale_set_draw_value (GTK_SCALE (hscale), FALSE);
    gtk_scale_set_digits (GTK_SCALE (hscale), -digits);
    gtk_box_pack_start (GTK_BOX (sp->hbox), hscale, TRUE, TRUE, 3);
    g_signal_connect (G_OBJECT (sp->hscale_adj), "value-changed",
		      G_CALLBACK (on_z_spinslider_hscale_changed), sp);
  }

  /* Reset button */

  {
    static GdkPixbuf *pixbuf = NULL;
    GtkWidget *button;
    GtkWidget *image;

    sp->history[0] = reset;
    sp->history[1] = reset;
    sp->history[2] = reset;
    sp->reset_state = 0;
    sp->in_reset = FALSE;

    if (!pixbuf)
      pixbuf = gdk_pixbuf_from_pixdata (&reset_png, FALSE, NULL);

    if (pixbuf && (image = gtk_image_new_from_pixbuf (pixbuf)))
      {
	gtk_widget_show (image);
	button = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (button), image);
	z_tooltip_set (button, _("Reset"));
      }
    else
      {
	button = gtk_button_new_with_label (_("Reset"));
      }

    gtk_widget_show (button);
    gtk_box_pack_start (GTK_BOX (sp->hbox), button, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (button), "pressed",
		      G_CALLBACK (on_z_spinslider_reset), sp);
  }

  return sp->hbox;
}

/*
 *  "Device entry"
 *
 *  Device:	 Foo Inc. Mk II
 *  Driver:	 Foo 0.0.1
 *  Device file: [/dev/foo__________][v]
 */

typedef struct _z_device_entry z_device_entry;

struct _z_device_entry
{
  GtkWidget *		table;
  GtkWidget *		device;
  GtkWidget *		driver;
  GtkWidget *		combo;

  guint			timeout;

  tv_device_node *	list;
  tv_device_node *	selected;

  z_device_entry_open_fn *open_fn;
  z_device_entry_select_fn *select_fn;

  gpointer		user_data;
};

static void
z_device_entry_destroy		(gpointer		data)
{
  z_device_entry *de = data;

  if (de->timeout != NO_SOURCE_ID)
    gtk_timeout_remove (de->timeout);

  tv_device_node_delete_list (&de->list);

  g_free (de);
}

tv_device_node *
z_device_entry_selected		(GtkWidget *		table)
{
  z_device_entry *de = g_object_get_data (G_OBJECT (table), "z_device_entry");

  g_assert (de != NULL);
  return de->selected;
}

void
z_device_entry_grab_focus	(GtkWidget *		table)
{
  z_device_entry *de = g_object_get_data (G_OBJECT (table), "z_device_entry");

  g_assert (de != NULL);

  gtk_widget_grab_focus (de->combo);
}

static void
z_device_entry_select		(z_device_entry *	de,
				 tv_device_node *	n)
{
  const gchar *device;
  const gchar *driver;
  gchar *s = NULL;

  de->selected = n;

  if (de->select_fn)
    de->select_fn (de->table, n, de->user_data);

  device = _("Unknown");
  driver = device;

  if (n)
    {
      if (n->label)
	device = n->label;

      if (n->driver && n->version)
	driver = s = g_strdup_printf ("%s %s", n->driver, n->version);
      else if (n->driver)
	driver = n->driver;
      else if (n->version)
	driver = n->version;
    }

  gtk_label_set_text (GTK_LABEL (de->device), device);
  gtk_label_set_text (GTK_LABEL (de->driver), driver);

  g_free (s);
}

static GtkWidget *
z_device_entry_label_new	(const char *		text,
				 gint			padding)
{
  GtkWidget *label;

  label = gtk_label_new (text);
  gtk_widget_show (label);
  gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), padding, padding);

  return label;
}

static void
on_z_device_entry_changed	(GtkEntry *		entry,
				 gpointer		user_data);

static void
z_device_entry_relist		(z_device_entry *	de)
{
  tv_device_node *n;

  if (de->combo)
    gtk_widget_destroy (de->combo);
  de->combo = gtk_combo_new ();
  gtk_widget_show (de->combo);

  for (n = de->list; n; n = n->next)
    {
      GtkWidget *item;
      GtkWidget *label;
      gchar *s;

      /* XXX deprecated, although the GtkCombo description elaborates:
	 "The drop-down list is a GtkList widget and can be accessed
	 using the list member of the GtkCombo. List elements can contain
	 arbitrary widgets, [by appending GtkListItems to the list]" */
      extern GtkWidget *gtk_list_item_new (void);

      item = gtk_list_item_new ();
      gtk_widget_show (item);

      /* XXX Perhaps add an icon indicating if the device is
         present but busy (v4l...), or the user has no access permission. */

      if (n->driver && n->label)
	s = g_strdup_printf ("%s (%s %s)", n->device, n->driver, n->label);
      else if (n->driver || n->label)
	s = g_strdup_printf ("%s (%s)", n->device,
			     n->driver ? n->driver : n->label);
      else
	s = g_strdup (n->device);

      label = z_device_entry_label_new (s, 0);
      g_free (s);

      gtk_container_add (GTK_CONTAINER (item), label);
      gtk_combo_set_item_string (GTK_COMBO (de->combo), GTK_ITEM (item), n->device);
      gtk_container_add (GTK_CONTAINER (GTK_COMBO (de->combo)->list), item);
    }

    gtk_table_attach (GTK_TABLE (de->table), de->combo, 1, 2, 2, 2 + 1,
  	    GTK_FILL | GTK_EXPAND, 0, 0, 0);

    g_signal_connect (G_OBJECT (GTK_COMBO (de->combo)->entry),
  	    "changed", G_CALLBACK (on_z_device_entry_changed), de);
}

static gboolean
on_z_device_entry_timeout	(gpointer		user_data)
{
  z_device_entry *de = user_data;
  tv_device_node *n;
  const gchar *s;

  s = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (de->combo)->entry));

  if (s && s[0])
    if ((n = de->open_fn (de->table, de->list, s, de->user_data)))
      {
	tv_device_node_add (&de->list, n);
	z_device_entry_relist (de);
	z_device_entry_select (de, n);
	return FALSE;
      }

  z_device_entry_select (de, NULL);

  return FALSE; /* don't call again */
}

static void
on_z_device_entry_changed	(GtkEntry *		entry,
				 gpointer		user_data)
{
  z_device_entry *de = user_data;
  tv_device_node *n;
  const gchar *s;

  if (de->timeout != NO_SOURCE_ID)
    {
      gtk_timeout_remove (de->timeout);
      de->timeout = NO_SOURCE_ID;
    }

  s = gtk_entry_get_text (entry);

  if (s && s[0])
    {
      for (n = de->list; n; n = n->next)
	if (0 == strcmp (s, n->device))
	  {
	    z_device_entry_select (de, n);
	    return;
	  }

      z_device_entry_select (de, NULL);
      
      de->timeout = gtk_timeout_add (1000 /* ms */,
				     on_z_device_entry_timeout, de);
    }
  else
    {
      z_device_entry_select (de, NULL);
    }
}

static void
z_device_entry_table_pair	(z_device_entry *	de,
				 guint			row,
				 const char *		label,
				 GtkWidget *		crank)
{
  gtk_table_attach (GTK_TABLE (de->table), z_device_entry_label_new (label, 3),
		    0, 1, row, row + 1, GTK_FILL, 0, 0, 0);
  if (crank)
    {
      gtk_widget_show (crank);
      gtk_table_attach (GTK_TABLE (de->table), crank, 1, 2, row, row + 1,
			GTK_FILL | GTK_EXPAND, 0, 0, 0);
    }
}

GtkWidget *
z_device_entry_new		(const gchar *		prompt,
				 tv_device_node *	list,
				 const gchar *		current_device,
				 z_device_entry_open_fn *open_fn,
				 z_device_entry_select_fn *select_fn,
				 gpointer		user_data)
{
  z_device_entry *de;

  g_assert (open_fn != NULL);

  de = g_malloc0 (sizeof (*de));

  de->open_fn = open_fn;
  de->select_fn = select_fn;
  de->user_data = user_data;

  de->timeout = NO_SOURCE_ID;

  de->table = gtk_table_new (3, 2, FALSE);
  gtk_widget_show (de->table);
  g_object_set_data_full (G_OBJECT (de->table), "z_device_entry", de,
			  (GDestroyNotify) z_device_entry_destroy);

  de->device = z_device_entry_label_new ("", 3);
  de->driver = z_device_entry_label_new ("", 3);

  z_device_entry_table_pair (de, 0, _("Device:"), de->device);
  z_device_entry_table_pair (de, 1, _("Driver:"), de->driver);
  z_device_entry_table_pair (de, 2, prompt ? prompt : _("Device file:"), NULL);

  de->list = list;

  z_device_entry_relist (de);

  gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (de->combo)->entry), "");

  if (current_device && current_device[0])
    gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (de->combo)->entry),
			current_device);
  else if (list)
    gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (de->combo)->entry),
			list->device);

  return de->table;
}

/**
 * z_widget_add_accelerator:
 * @widget: 
 * @accel_signal: 
 * @accel_key: 
 * @accel_mods: 
 * 
 * Like gtk_widget_add_accelerator() but takes care of creating the
 * accel group.
 **/
void
z_widget_add_accelerator	(GtkWidget	*widget,
				 const gchar	*accel_signal,
				 guint		accel_key,
				 guint		accel_mods)
{
  if (!accel_group)
    {
      accel_group = gtk_accel_group_new();
      gtk_window_add_accel_group(GTK_WINDOW(zapping),
				 accel_group);
    }

  gtk_widget_add_accelerator(widget, accel_signal, accel_group,
			     accel_key, accel_mods, GTK_ACCEL_VISIBLE);
}

static void
on_entry_activate (GObject *entry, GtkDialog *dialog)
{
  gtk_dialog_response (dialog, GPOINTER_TO_INT
		       (g_object_get_data (entry, "zmisc-response")));
}

void
z_entry_emits_response		(GtkWidget	*entry,
				 GtkDialog	*dialog,
				 GtkResponseType response)
{
  g_signal_connect (G_OBJECT (entry), "activate",
		    G_CALLBACK (on_entry_activate),
		    dialog);

  g_object_set_data (G_OBJECT (entry), "zmisc-response",
		     GINT_TO_POINTER (response));
}

/*
 *  Application stock icons
 */

GtkWidget *
z_gtk_image_new_from_pixdata	(const GdkPixdata *	pixdata)
{
  GdkPixbuf *pixbuf;
  GtkWidget *image;

  pixbuf = gdk_pixbuf_from_pixdata (pixdata, FALSE, NULL);
  g_assert (pixbuf != NULL);

  image = gtk_image_new_from_pixbuf (pixbuf);
  g_object_unref (G_OBJECT (pixbuf));

  return image;
}

static GtkIconFactory *
icon_factory			(void)
{
  static GtkIconFactory *factory = NULL;

  if (!factory)
    {
      factory = gtk_icon_factory_new ();
      gtk_icon_factory_add_default (factory);
      /* g_object_unref (factory); */
    }

  return factory;
}

gboolean
z_icon_factory_add_file		(const gchar *		stock_id,
				 const gchar *		filename)
{
  GtkIconSet *icon_set;
  GdkPixbuf *pixbuf;
  GError *err;
  gchar *path;
 
  path = g_strconcat (PACKAGE_PIXMAPS_DIR "/", filename, NULL);

  err = NULL;

  pixbuf = gdk_pixbuf_new_from_file (path, &err);

  g_free (path);

  if (!pixbuf)
    {
      if (err)
	{
#ifdef ZMISC_DEBUG_STOCK /* FIXME */
	  fprintf (stderr, "Cannot read image file '%s':\n%s\n",
		   err->message);
#endif
	  g_error_free (err);
	}

      return FALSE;
    }

  icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);
  g_object_unref (G_OBJECT (pixbuf));

  gtk_icon_factory_add (icon_factory (), stock_id, icon_set);
  gtk_icon_set_unref (icon_set);

  return TRUE;
}

gboolean
z_icon_factory_add_pixdata	(const gchar *		stock_id,
				 const GdkPixdata *	pixdata)
{
  GtkIconSet *icon_set;
  GdkPixbuf *pixbuf;
  GError *err;

  err = NULL;

  pixbuf = gdk_pixbuf_from_pixdata (pixdata, /* copy_pixels */ FALSE, &err);

  if (!pixbuf)
    {
      if (err)
	{
#ifdef ZMISC_DEBUG_STOCK /* FIXME */
	  fprintf (stderr, "Cannot read pixdata:\n%s\n", err->message);
#endif
	  g_error_free (err);
	}

      return FALSE;
    }

  icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);
  g_object_unref (G_OBJECT (pixbuf));

  gtk_icon_factory_add (icon_factory (), stock_id, icon_set);
  gtk_icon_set_unref (icon_set);

  return TRUE;
}

/* This is for tveng, which will eventually spin off.
   Elsewhere use g_strlcpy. */
size_t
z_strlcpy			(char *			dst,
				 const char *		src,
				 size_t			size)
{
	register const char *s;
	register char c, *d, *e;

	if (size < 1)
		return strlen (src);

	s = src;
	d = dst;
	e = d + size - 1;

	while ((c = *s++) && d < e)
		*d++ = c;

	*d = 0;

	while (c)
		c = *s++;

	return s - src;
}

/* Debugging. */
const gchar *
z_gdk_event_name		(GdkEvent *		event)
{
  static const gchar *event_name[] =
    {
      "NOTHING", "DELETE", "DESTROY", "EXPOSE", "MOTION_NOTIFY",
      "BUTTON_PRESS", "2BUTTON_PRESS", "3BUTTON_PRESS", "BUTTON_RELEASE",
      "KEY_PRESS", "KEY_RELEASE", "ENTER_NOTIFY", "LEAVE_NOTIFY",
      "FOCUS_CHANGE", "CONFIGURE", "MAP", "UNMAP", "PROPERTY_NOTIFY",
      "SELECTION_CLEAR", "SELECTION_REQUEST", "SELECTION_NOTIFY",
      "PROXIMITY_IN", "PROXIMITY_OUT", "DRAG_ENTER", "DRAG_LEAVE",
      "DRAG_MOTION", "DRAG_STATUS", "DROP_START", "DROP_FINISHED",
      "CLIENT_EVENT", "VISIBILITY_NOTIFY", "NO_EXPOSE"
    };
  
  if (event->type >= GDK_NOTHING
      && event->type <= GDK_NO_EXPOSE)
    return event_name[event->type - GDK_NOTHING];
  else
    return "unknown";
}

void
z_label_set_text_printf		(GtkLabel *		label,
				 const gchar *		format,
				 ...)
{
  gchar *buffer;
  va_list args;

  va_start (args, format);
  buffer = g_strdup_vprintf (format, args);
  va_end (args);

  gtk_label_set_text (label, buffer);

  g_free (buffer);
}

#define VALID_ITER(iter, list_store)					\
  ((iter) != NULL							\
   && (iter)->user_data != NULL						\
   && ((GTK_LIST_STORE (list_store))->stamp == (iter)->stamp))

gboolean
z_tree_selection_iter_first	(GtkTreeSelection *	selection,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter)
{
  if (!gtk_tree_model_get_iter_first (model, iter))
    return FALSE; /* empty list */

  while (!gtk_tree_selection_iter_is_selected (selection, iter))
    if (!gtk_tree_model_iter_next (model, iter))
      return FALSE; /* nothing selected */

  return TRUE;
}

gboolean
z_tree_selection_iter_last	(GtkTreeSelection *	selection,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter)
{
  GtkTreeIter last;

  if (!z_tree_selection_iter_first (selection, model, iter))
    return FALSE; /* nothing */

  last = *iter;

  while (gtk_tree_model_iter_next (model, &last)
	 && gtk_tree_selection_iter_is_selected (selection, &last))
    *iter = last;

  return TRUE;
}

/* In a GTK_SELECTION_MULTIPLE tree view, remove all selected
   rows of the model. */
void
z_tree_view_remove_selected	(GtkTreeView *		tree_view,
				 GtkTreeSelection *	selection,
				 GtkTreeModel *		model)
{
  GtkTreeIter iter;

  if (!z_tree_selection_iter_first (selection, model, &iter))
    return; /* nothing */

  while (VALID_ITER (&iter, model))
    {
      if (!gtk_tree_selection_iter_is_selected (selection, &iter))
	break;

      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
    }

  if (VALID_ITER (&iter, model))
    {
      GtkTreePath *path;

      gtk_tree_selection_select_iter (selection, &iter);

      if ((path = gtk_tree_model_get_path (model, &iter)))
	{
	  gtk_tree_view_scroll_to_cell (tree_view, path, NULL,
					/* use_align */ TRUE, 0.5, 0.0);
	  gtk_tree_path_free (path);
	}
    }
}

gboolean
z_overwrite_file		(GtkWindow *		parent,
				 const gchar *		filename)
{
  struct stat st;
  GtkWidget *dialog;
  gchar *name;

  if (-1 == stat (filename, &st))
    {
      /* Also symlink to non-existing file. */
      return TRUE;
    }

  name = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
  if (!name)
    return FALSE; 

  if (S_ISREG (st.st_mode) || S_ISLNK (st.st_mode))
    {
      dialog = gtk_message_dialog_new (parent,
				       (GTK_DIALOG_MODAL |
					GTK_DIALOG_DESTROY_WITH_PARENT),
				       GTK_MESSAGE_QUESTION,
				       GTK_BUTTONS_NONE,
				       _("%s exists."),
				       name);

      gtk_dialog_add_buttons (GTK_DIALOG (dialog),
			      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
			      _("_Overwrite"), GTK_RESPONSE_ACCEPT,
			      NULL);
    }
  else if (S_ISDIR (st.st_mode))
    {
      ShowBox (_("%s is a directory."), GTK_MESSAGE_ERROR, name);
      g_free (name);
      return FALSE;
    }
  else
    {
      dialog = gtk_message_dialog_new (parent,
				       (GTK_DIALOG_MODAL |
					GTK_DIALOG_DESTROY_WITH_PARENT),
				       GTK_MESSAGE_QUESTION,
				       GTK_BUTTONS_NONE,
				       /* TRANSLATORS: device file, pipe,
					  socket etc. */
				       _("%s is a special file."),
				       name);

      gtk_dialog_add_buttons (GTK_DIALOG (dialog),
			      _("Continue"), GTK_RESPONSE_ACCEPT,
			      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
			      NULL);
    }

  g_free (name);

  return (GTK_RESPONSE_ACCEPT == gtk_dialog_run (GTK_DIALOG (dialog)));
}

void
from_old_tveng_capture_mode	(display_mode *		dmode,
				 capture_mode *		cmode,
				 enum old_tveng_capture_mode mode)
{
  switch (mode)
    {
    case OLD_TVENG_NO_CAPTURE:
      break;

    case OLD_TVENG_CAPTURE_READ:
      *dmode = DISPLAY_MODE_WINDOW;
      *cmode = CAPTURE_MODE_READ;
      return;

    case OLD_TVENG_CAPTURE_PREVIEW:
      *dmode = DISPLAY_MODE_FULLSCREEN;
      *cmode = CAPTURE_MODE_OVERLAY;
      return;

    case OLD_TVENG_CAPTURE_WINDOW:
      *dmode = DISPLAY_MODE_WINDOW;
      *cmode = CAPTURE_MODE_OVERLAY;
      return;

    case OLD_TVENG_TELETEXT:
      *dmode = DISPLAY_MODE_WINDOW;
      *cmode = CAPTURE_MODE_TELETEXT;
      return;

    case OLD_TVENG_FULLSCREEN_READ:
      *dmode = DISPLAY_MODE_FULLSCREEN;
      *cmode = CAPTURE_MODE_READ;
      return;

    case OLD_TVENG_FULLSCREEN_TELETEXT:
      *dmode = DISPLAY_MODE_FULLSCREEN;
      *cmode = CAPTURE_MODE_TELETEXT;
      return;

    case OLD_TVENG_BACKGROUND_READ:
      *dmode = DISPLAY_MODE_BACKGROUND;
      *cmode = CAPTURE_MODE_READ;
      return;

    case OLD_TVENG_BACKGROUND_PREVIEW:
      *dmode = DISPLAY_MODE_BACKGROUND;
      *cmode = CAPTURE_MODE_OVERLAY;
      return;

    case OLD_TVENG_BACKGROUND_TELETEXT:
      *dmode = DISPLAY_MODE_BACKGROUND;
      *cmode = CAPTURE_MODE_TELETEXT;
      return;
    }

  *dmode = DISPLAY_MODE_NONE;
  *cmode = CAPTURE_MODE_NONE;
}

enum old_tveng_capture_mode
to_old_tveng_capture_mode	(display_mode 		dmode,
				 capture_mode 		cmode)
{
  enum old_tveng_capture_mode mode;

  mode = OLD_TVENG_NO_CAPTURE;

  switch (dmode)
    {
    case DISPLAY_MODE_NONE:
      mode = OLD_TVENG_NO_CAPTURE; break;

    case DISPLAY_MODE_WINDOW:
      switch (cmode)
	{
	case CAPTURE_MODE_NONE:
	  mode = OLD_TVENG_NO_CAPTURE; break;
	case CAPTURE_MODE_READ:
	  mode = OLD_TVENG_CAPTURE_READ; break;
	case CAPTURE_MODE_OVERLAY:
	  mode = OLD_TVENG_CAPTURE_WINDOW; break;
	case CAPTURE_MODE_TELETEXT:
	  mode = OLD_TVENG_TELETEXT; break;
	}
      break;

    case DISPLAY_MODE_FULLSCREEN:
      switch (cmode)
	{
	case CAPTURE_MODE_NONE:
	  mode = OLD_TVENG_NO_CAPTURE; break;
	case CAPTURE_MODE_READ:
	  mode = OLD_TVENG_FULLSCREEN_READ; break;
	case CAPTURE_MODE_TELETEXT:
	  mode = OLD_TVENG_FULLSCREEN_TELETEXT; break;
	case CAPTURE_MODE_OVERLAY:
	  mode = OLD_TVENG_CAPTURE_PREVIEW; break;
	}
      break;

    case DISPLAY_MODE_BACKGROUND:
      switch (cmode)
	{
	case CAPTURE_MODE_NONE:
	  mode = OLD_TVENG_NO_CAPTURE; break;
	case CAPTURE_MODE_READ:
	  mode = OLD_TVENG_BACKGROUND_READ; break;
	case CAPTURE_MODE_TELETEXT:
	  mode = OLD_TVENG_BACKGROUND_TELETEXT; break;
	case CAPTURE_MODE_OVERLAY:
	  mode = OLD_TVENG_BACKGROUND_PREVIEW; break;
	}
      break;
    }

  return mode;
}

gboolean
z_set_overlay_buffer		(tveng_device_info *	info,
				 const tv_screen *	screen,
				 const GdkWindow *	window _unused_)
{
#if 0 /* Gtk 2.2 */
  GdkDisplay *display;
  const gchar *display_name;

  display = gdk_drawable_get_display (window);
  display_name = gdk_display_get_name (display);
#endif

  return tv_set_overlay_buffer (info,
				x11_display_name (),
				screen->screen_number,
				&screen->target);
}

typedef struct {
  gchar *		key;
  guint			cnxn;
} gc_notify;

static void
gc_notify_destroy		(gc_notify *		g)
{
  if (0 != g->cnxn)
    gconf_client_notify_remove (gconf_client, g->cnxn);

  g_free (g->key);

  g_free (g);
}

static void
gc_notify_add			(gc_notify *		g,
				 const char *		gconf_key,
				 GConfClientNotifyFunc	func)
{
  GError *error;

  error = NULL;

  g->key = g_strdup (gconf_key);

  g->cnxn = gconf_client_notify_add (gconf_client, gconf_key,
				     func, g, /* destroy */ NULL, &error);
  if (error)
    {
      g_message ("GConf notification '%s' error:\n%s\n",
		 gconf_key, error->message);
      g_error_free (error);
      exit (EXIT_FAILURE);
    }
}

typedef struct {
  gc_notify		gcn;
  GtkToggleAction *	toggle_action;
} gc_toggle_action;

static void
gc_toggle_action_notify		(GConfClient *		client _unused_,
				 guint			cnxn_id _unused_,
				 GConfEntry *		entry,
				 gc_toggle_action *	g)
{
  gboolean active;

  if (!entry->value)
    return; /* unset */

  active = gconf_value_get_bool (entry->value);

  if (active == gtk_toggle_action_get_active (g->toggle_action))
    return;

  gtk_toggle_action_set_active (g->toggle_action, active);
}

static void
gc_toggle_action_toggled	(GtkToggleAction *	toggle_action,
				 gc_toggle_action *	g)
{
  gboolean active;

  active = gtk_toggle_action_get_active (toggle_action);

  /* Error ignored. */
  gconf_client_set_bool (gconf_client,
			 g->gcn.key,
			 active,
			 /* err */ NULL);
}

void
z_toggle_action_connect_gconf_key
				(GtkToggleAction *	toggle_action,
				 const gchar *		gconf_key)
{
  gc_toggle_action *g;
  GConfValue *value;

  if ((value = gconf_client_get (gconf_client, gconf_key, /* err */ NULL)))
    {
      gboolean active;

      /* No error and value is set. Synchronize action with gconf. */

      active = gconf_value_get_bool (value);
      gconf_value_free (value);

      gtk_toggle_action_set_active (toggle_action, active);
    }

  g = g_malloc0 (sizeof (*g));

  g->toggle_action = toggle_action;

  gc_notify_add (&g->gcn, gconf_key,
		 (GConfClientNotifyFunc) gc_toggle_action_notify);

  g_signal_connect_data (G_OBJECT (toggle_action), "toggled",
			 G_CALLBACK (gc_toggle_action_toggled), g,
			 (GClosureNotify) gc_notify_destroy,
			 /* connect_flags */ 0);
}

typedef struct {
  gc_notify		gcn;
  GtkComboBox *		combo_box;
  const GConfEnumStringPair *lookup_table;
} gc_combo_box;

static void
gc_combo_box_notify		(GConfClient *		client _unused_,
				 guint			cnxn_id _unused_,
				 GConfEntry *		entry,
				 gc_combo_box *		g)
{
  if (entry->value)
    {
      const gchar *s;

      s = gconf_value_get_string (entry->value);
      if (s)
	{
	  guint i;

	  for (i = 0; g->lookup_table[i].str; ++i)
	    {
	      if (0 == strcmp (s, g->lookup_table[i].str))
		{
		  gint index;

		  index = gtk_combo_box_get_active (g->combo_box);
		  if ((gint) i != index)
		    gtk_combo_box_set_active (g->combo_box, (gint) i);

		  return;
		}
	    }
	}
    }

  gtk_combo_box_set_active (g->combo_box, -1 /* unset */);
}

static void
gc_combo_box_changed		(GtkComboBox *		combo_box,
				 gc_combo_box *		g)
{
  gint index;

  index = gtk_combo_box_get_active (combo_box);

  /* Error ignored. */
  gconf_client_set_string (gconf_client,
			   g->gcn.key,
			   g->lookup_table[index].str,
			   /* err */ NULL);
}

GtkWidget *
z_gconf_combo_box_new		(const gchar **		option_menu,
				 const gchar *		gconf_key,
				 const GConfEnumStringPair *lookup_table)
{
  gc_combo_box *g;
  GtkWidget *widget;
  guint i;
  GError *error;
  gchar *s;

  g = g_malloc0 (sizeof (*g));

  widget = gtk_combo_box_new_text ();
  g->combo_box = GTK_COMBO_BOX (widget);

  for (i = 0; option_menu[i]; ++i)
    gtk_combo_box_append_text (g->combo_box, _(option_menu[i]));

  error = NULL;

  if ((s = gconf_client_get_string (gconf_client, gconf_key, &error)))
    {
      for (i = 0; lookup_table[i].str; ++i)
	{
	  if (0 == strcmp (s, lookup_table[i].str))
	    {
	      gtk_combo_box_set_active (g->combo_box, (int) i);
	      break;
	    }
	}
    }
  else
    {
      g_message ("GConf get '%s' error:\n%s\n", gconf_key, error->message);
      g_error_free (error);
    }

  g->lookup_table = lookup_table;

  gc_notify_add (&g->gcn, gconf_key,
		 (GConfClientNotifyFunc) gc_combo_box_notify);

  g_signal_connect (G_OBJECT (widget), "changed",
		    G_CALLBACK (gc_combo_box_changed), g);

  g_signal_connect_swapped (G_OBJECT (widget), "destroy",
			    G_CALLBACK (gc_notify_destroy), g);

  return widget;
}

gboolean
z_gconf_get_string_enum		(gint *			enum_value,
				 const gchar *		gconf_key,
				 const GConfEnumStringPair *lookup_table)
{
  GError *error;
  gchar *s;
  gboolean r;

  error = NULL;

  if ((s = gconf_client_get_string (gconf_client, gconf_key, &error)))
    {
      r = gconf_string_to_enum (lookup_table, s, enum_value);
      g_free (s);
    }
  else
    {
      g_message ("GConf get '%s' error:\n%s\n", gconf_key, error->message);
      g_error_free (error);
      r = FALSE;
    }

  return r;
}

/* Not available until Gtk+ 2.6 */
void
z_action_set_sensitive		(GtkAction *		action,
				 gboolean		sensitive)
{
  g_object_set (G_OBJECT (action),
		"sensitive", sensitive,
		NULL);
}

/* Not available until Gtk+ 2.6 */
void
z_action_set_visible		(GtkAction *		action,
				 gboolean		visible)
{
  g_object_set (G_OBJECT (action),
		"visible", visible,
		NULL);
}
