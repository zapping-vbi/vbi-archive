/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 Iñaki García Etxebarria
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gdk/gdkx.h>

#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zmisc.h"
#include "zconf.h"
#include "tveng.h"
#include "interface.h"
#include "callbacks.h"
#include "zvbi.h"
#include "x11stuff.h"
#include "overlay.h"
#include "capture.h"
#include "v4linterface.h"

extern tveng_device_info * main_info;
extern GtkWidget * main_window;
extern gboolean disable_preview; /* TRUE if preview won't work */
gboolean debug_msg=FALSE; /* Debugging messages on or off */
static GtkWidget * black_window = NULL; /* The black window when you go
					   preview */

/* Comment the next line if you don't want to mess with the
   XScreensaver */
#define MESS_WITH_XSS 1

/*
  Prints a message box showing an error, with the location of the code
  that called the function. If run is TRUE, gnome_dialog_run will be
  called instead of just showing the dialog. Keep in mind that this
  will block the capture.
*/
GtkWidget * ShowBoxReal(const gchar * sourcefile,
			const gint line,
			const gchar * func,
			const gchar * message,
			const gchar * message_box_type,
			gboolean blocking, gboolean modal)
{
  GtkWidget * dialog;
  gchar * str;
  gchar buffer[256];

  buffer[255] = 0;

  g_snprintf(buffer, 255, " (%d)", line);

  str = g_strconcat(sourcefile, buffer, ": [", func, "]", NULL);

  if (!str)
    return NULL;

  dialog = gnome_message_box_new(message, message_box_type,
				 GNOME_STOCK_BUTTON_OK,
				 NULL);


  gtk_window_set_title(GTK_WINDOW (dialog), str);

  /*
    I know this isn't usual for a dialog box, but this way we can see
    all the title bar if we want to
  */
  gtk_window_set_policy(GTK_WINDOW (dialog), FALSE, TRUE, TRUE);

  g_free(str);

  if (blocking)
    {
      gnome_dialog_run_and_close(GNOME_DIALOG(dialog));
      return NULL;
    }

  gtk_window_set_modal(GTK_WINDOW (dialog), modal);
  gtk_widget_show(dialog);

  return dialog;
}

gchar*
Prompt (GtkWidget *main_window, const gchar *title,
	const gchar *prompt,  const gchar *default_text)
{
  GtkWidget * dialog;
  GtkVBox * vbox;
  GtkWidget *label, *entry;
  gchar *buffer = NULL;

  dialog = gnome_dialog_new(title,
			    GNOME_STOCK_BUTTON_OK,
			    GNOME_STOCK_BUTTON_CANCEL,
			    NULL);
  if (main_window)
    gnome_dialog_set_parent(GNOME_DIALOG (dialog), GTK_WINDOW(main_window));
  gnome_dialog_close_hides(GNOME_DIALOG (dialog), TRUE);
  gnome_dialog_set_default(GNOME_DIALOG (dialog), 0);

  gtk_window_set_title(GTK_WINDOW (dialog), title);
  gtk_window_set_modal(GTK_WINDOW (dialog), TRUE);
  vbox = GTK_VBOX(GNOME_DIALOG(dialog)->vbox);
  if (prompt)
    {
      label = gtk_label_new(prompt);
      gtk_box_pack_start_defaults(GTK_BOX(vbox), label);
      gtk_widget_show(label);
    }
  entry = gtk_entry_new();
  gtk_box_pack_start_defaults(GTK_BOX(vbox), entry);
  gtk_widget_show(entry);
  gnome_dialog_editable_enters(GNOME_DIALOG(dialog), GTK_EDITABLE (entry));
  gtk_widget_grab_focus(entry);
  if (default_text)
    {
      gtk_entry_set_text(GTK_ENTRY(entry), default_text);
      gtk_entry_select_region(GTK_ENTRY(entry), 0, -1);
    }

  if (!gnome_dialog_run_and_close(GNOME_DIALOG(dialog)))
    buffer = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));

  gtk_widget_destroy(dialog);
  
  return buffer;
}

/*
  Creates a GtkPixmapMenuEntry with the desired pixmap and the
  desired label.
*/
GtkWidget * z_gtk_pixmap_menu_item_new(const gchar * label,
				       const gchar * icon)
{
  GtkWidget * pixmap_menu_item;
  GtkWidget * accel_label;
  GtkWidget * pixmap;

  g_assert(label != NULL);

  pixmap_menu_item = gtk_pixmap_menu_item_new();
  accel_label = gtk_accel_label_new (label);
  gtk_misc_set_alignment(GTK_MISC(accel_label), 0.0, 0.5);
  gtk_container_add(GTK_CONTAINER (pixmap_menu_item), accel_label);
  gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label),
				    pixmap_menu_item);
  gtk_widget_show (accel_label);

  if (icon)
    {
      /* if i don't specify the size, the pixmap is too big, but the
	 one libgnomeui creates isn't... why? */
      pixmap = gnome_stock_pixmap_widget_at_size (pixmap_menu_item,
						  icon, 16, 16);
      
      gtk_pixmap_menu_item_set_pixmap(GTK_PIXMAP_MENU_ITEM(pixmap_menu_item),
				      pixmap);
      gtk_widget_show(pixmap);
    }

  return (pixmap_menu_item);
}

static gint
fullscreen_start(tveng_device_info * info)
{
  GtkWidget * da; /* Drawing area */

  /* Add a black background */
  black_window = gtk_window_new( GTK_WINDOW_POPUP );
  da = gtk_drawing_area_new();

  gtk_widget_show(da);

  gtk_container_add(GTK_CONTAINER(black_window), da);
  gtk_widget_set_usize(black_window, gdk_screen_width(),
		       gdk_screen_height());

  gtk_widget_show(black_window);
  gtk_window_set_modal(GTK_WINDOW(black_window), TRUE);
  gdk_window_set_decorations(black_window->window, 0);

  /* Draw on the drawing area */
  gdk_draw_rectangle(da -> window,
  		     da -> style -> black_gc,
		     TRUE,
		     0, 0, gdk_screen_width(), gdk_screen_height());

  if (tveng_start_previewing(info, 1-zcg_int(NULL, "change_mode")) == -1)
    {
      ShowBox(_("Sorry, but cannot go fullscreen"),
	      GNOME_MESSAGE_BOX_ERROR);
      gtk_widget_destroy(black_window);
      zmisc_switch_mode(TVENG_CAPTURE_READ, info);
      return -1;
    }

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

  gtk_signal_connect(GTK_OBJECT(black_window), "event",
		     GTK_SIGNAL_FUNC(on_fullscreen_event),
  		     main_window);

  return 0;
}

static void
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
}

/*
  does the mode switching. Since this requires more than just using
  tveng, a new routine is needed.
  Returns whatever tveng returns, but we print the message ourselves
  too, so no need to aknowledge it to the user.
  Side efects: Stops whatever mode was being used before.
*/
int
zmisc_switch_mode(enum tveng_capture_mode new_mode,
		  tveng_device_info * info)
{
  GtkWidget * tv_screen;
  int return_value = 0;
  gint x, y, w, h;
  enum tveng_frame_pixformat format;
  gboolean muted;

  g_assert(info != NULL);
  g_assert(main_window != NULL);
  tv_screen = lookup_widget(main_window, "tv_screen");
  g_assert(tv_screen != NULL);

  if (info->current_mode == new_mode)
    return 0; /* success */

  gdk_window_get_size(tv_screen->window, &w, &h);
  gdk_window_get_origin(tv_screen->window, &x, &y);

  muted = tveng_get_mute(info);

  /* Stop current capture mode */
  switch (info->current_mode)
    {
    case TVENG_CAPTURE_PREVIEW:
      fullscreen_stop(info);
      break;
    case TVENG_CAPTURE_READ:
      capture_stop(info);
      break;
    case TVENG_CAPTURE_WINDOW:
      overlay_stop(info, main_window);
      break;
    default:
      break;
    }

  if (new_mode != TVENG_NO_CAPTURE)
    {
      zvbi_set_mode(FALSE);
      tveng_close_device(info);
    }
  else
    {
      zvbi_set_mode(TRUE);
      tveng_stop_everything(info);
    }

  switch (new_mode)
    {
    case TVENG_CAPTURE_READ:
      tveng_attach_device(zconf_get_string(NULL,
					   "/zapping/options/main/video_device"), TVENG_ATTACH_READ, info);
      tveng_set_capture_size(w, h, info);
      return_value = capture_start(tv_screen, info);
      break;
    case TVENG_CAPTURE_WINDOW:
      if (disable_preview) {
	g_warning("preview has been disabled");
	tveng_attach_device(zconf_get_string(NULL,
	 "/zapping/options/main/video_device"), TVENG_ATTACH_READ, info);
	return -1;
      }

      tveng_attach_device(zconf_get_string(NULL,
	 "/zapping/options/main/video_device"), TVENG_ATTACH_XV, info);

      format = zmisc_resolve_pixformat(tveng_get_display_depth(info),
				       x11_get_byte_order());

      if ((format != -1) &&
	  (info->current_controller != TVENG_CONTROLLER_XV))
	{
	  info->format.pixformat = format;
	  if ((tveng_set_capture_format(info) == -1) ||
	      (info->format.pixformat != format))
	    g_warning("Preview format invalid: %s (%d, %d)", info->error,
		      info->format.pixformat, format);
	  printv("prev: setting %d, got %d\n", format,
		 info->format.pixformat);
	}

      info->window.x = x;
      info->window.y = y;
      info->window.width = w;
      info->window.height = h;
      info->window.clipcount = 0;
      info->window.win = GDK_WINDOW_XWINDOW(tv_screen->window);
      info->window.gc = GDK_GC_XGC(tv_screen->style->white_gc);
      tveng_set_preview_window(info);
      return_value = tveng_start_window(info);
      if (return_value != -1)
	overlay_sync(TRUE);
      else
	g_warning(info->error);
      break;
    case TVENG_CAPTURE_PREVIEW:
      tveng_attach_device(zconf_get_string(NULL,
	"/zapping/options/main/video_device"), TVENG_ATTACH_READ, info);
      if (disable_preview) {
	g_warning("preview has been disabled");
	return -1;
      }

      format = zmisc_resolve_pixformat(tveng_get_display_depth(info),
				       x11_get_byte_order());

      if ((format != -1) &&
	  (info->current_controller != TVENG_CONTROLLER_XV))
	{
	  info->format.pixformat = format;
	  if ((tveng_set_capture_format(info) == -1) ||
	      (info->format.pixformat != format))
	    g_warning("Fullscreen format invalid: %s (%d, %d)", info->error,
		      info->format.pixformat, format);
	  printv("fulls: setting %d, got %d\n", format,
		 info->format.pixformat);
	}

      return_value = fullscreen_start(info);
      if (return_value == -1)
	g_warning("couldn't start fullscreen mode");
      break;
    default:
      break; /* TVENG_NO_CAPTURE */
    }

  /* Update the controls window if it's open */
  update_control_box(info);
  /* Update the standards, channels, etc */
  update_standards_menu(tv_screen, info);
  /* Updating the properties is not so useful, and it isn't so easy,
     since there might be multiple properties dialog open */
  tveng_set_mute(muted, info);

  return return_value;
}
