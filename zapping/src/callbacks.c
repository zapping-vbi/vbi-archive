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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "tveng.h"
#include "callbacks.h"
#include "interface.h"
#include "v4linterface.h"
#include "x11stuff.h"
/* Manages config values for zconf (it saves me some typing) */
#define ZCONF_DOMAIN "/zapping/internal/callbacks/"
#include "zconf.h"
#include "zmisc.h"
#include "plugins.h"
#include "zconf.h"
#include "zvbi.h"
#include "ttxview.h"
#include "osd.h"

/* set this flag to TRUE to exit the program */
extern volatile gboolean flag_exit_program;
extern GtkWidget *ToolBox; /* Control box, if any */
/* the mode set when we went fullscreen (used by main.c too) */
enum tveng_capture_mode restore_mode;

extern tveng_tuned_channel * global_channel_list;
extern tveng_device_info * main_info; /* About the device we are using */
extern gint disable_preview; /* TRUE if preview (fullscreen)
				    doesn't work */
extern gint zvbi_page;

int cur_tuned_channel = 0; /* Currently tuned channel */

extern GtkWidget * main_window; /* main Zapping window */

extern GList * plugin_list; /* The plugins we have */

/* Starts and stops callbacks */
gboolean startup_callbacks(void)
{
  /* Init values to their defaults */
  zcc_int(30, "X coord of the Zapping window", "x");
  zcc_int(30, "Y coord of the Zapping window", "y");
  zcc_int(640, "Width of the Zapping window", "w");
  zcc_int(480, "Height of the Zapping window", "h");
  zcc_int(0, "Currently tuned channel", "cur_tuned_channel");
  zcc_bool(TRUE, "Show the Closed Caption", "closed_caption");
  cur_tuned_channel = zcg_int(NULL, "cur_tuned_channel");
  zcc_int(0x1, "Subtitles page", "zvbi_page");

  return TRUE;
}

void shutdown_callbacks(void)
{
  zcs_int(cur_tuned_channel, "cur_tuned_channel");
}

/* Gets the geometry of the main window */
static void UpdateCoords(GdkWindow * window)
{
  int x, y, w, h;
  gdk_window_get_origin(window, &x, &y);
  gdk_window_get_size(window, &w, &h);
  
  zcs_int(x, "x");
  zcs_int(y, "y");
  zcs_int(w, "w");
  zcs_int(h, "h");
  zconf_set_integer(main_info->current_mode,
		    "/zapping/options/main/capture_mode");
  zmisc_switch_mode(TVENG_NO_CAPTURE, main_info);
}

void
on_exit2_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget * widget = lookup_widget(GTK_WIDGET(menuitem), "zapping");
  GList * p;

  flag_exit_program = TRUE;

  UpdateCoords(widget->window);

  /* Tell the widget that the GUI is going to be closed */
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_remove_gui(GNOME_APP(widget), 
			(struct plugin_info*)p->data);
      p = p->next;
    }

  gtk_main_quit();
}

void
on_toggle_muted1_activate		(GtkMenuItem	*menuitem,
					 gpointer	user_data)
{
  tveng_set_mute(1-tveng_get_mute(main_info), main_info);

  osd_render_sgml(tveng_get_mute(main_info) ?
		  _("<blue>audio off</blue>") :
		  _("<yellow>AUDIO ON</yellow>"));
}

void
on_about1_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  gtk_widget_show(create_about2());
}

void
on_plugin_writing1_activate            (GtkMenuItem     *menuitem,
					gpointer         user_data)
{
  GnomeHelpMenuEntry help_ref = { NULL,
				  "plugin_devel.html" };
  enum tveng_capture_mode cur_mode;

  cur_mode = tveng_stop_everything(main_info);

  help_ref.name = gnome_app_id;
  gnome_help_display (NULL, &help_ref);

  if (z_restart_everything(cur_mode, main_info) == -1)
    ShowBox(main_info->error, GNOME_MESSAGE_BOX_ERROR);
}

void
on_main_help1_activate                 (GtkMenuItem     *menuitem,
					gpointer         user_data)
{
  static GnomeHelpMenuEntry help_ref = { NULL,
					 "index.html" };
  enum tveng_capture_mode cur_mode;

  cur_mode = tveng_stop_everything(main_info);

  help_ref.name = gnome_app_id;
  gnome_help_display (NULL, &help_ref);

  if (z_restart_everything(cur_mode, main_info) == -1)
    ShowBox(main_info->error, GNOME_MESSAGE_BOX_ERROR);
}

void
on_hide_controls1_activate             (GtkMenuItem     *menuitem,
					gpointer         user_data)
{
  if (zcg_bool(NULL, "hide_controls"))
    {
      zcs_bool(FALSE, "hide_controls");
      gtk_widget_show(lookup_widget(main_window, "dockitem1"));
      gtk_widget_show(lookup_widget(main_window, "dockitem2")); 
      gtk_widget_queue_resize(main_window);

      z_change_menuitem(lookup_widget(GTK_WIDGET(main_window),
				      "hide_controls2"),
			GNOME_STOCK_PIXMAP_BOOK_YELLOW,
			_("Hide controls"),
			_("Hide the menu and the toolbar"));
    }
  else
    {
      zcs_bool(TRUE, "hide_controls");
      gtk_widget_hide(lookup_widget(main_window, "dockitem1"));
      gtk_widget_hide(lookup_widget(main_window, "dockitem2"));
      gtk_widget_queue_resize(main_window);

      z_change_menuitem(lookup_widget(GTK_WIDGET(main_window),
				      "hide_controls2"),
			GNOME_STOCK_PIXMAP_BOOK_OPEN,
			_("Show controls"),
			_("Show the menu and the toolbar"));
    }
}

gboolean
on_zapping_delete_event                (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  GList * p;

  flag_exit_program = TRUE;

  UpdateCoords(widget->window);

  /* Tell the widget that the GUI is going to be closed */
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_remove_gui(GNOME_APP(widget), 
			 (struct plugin_info*)p->data);
      p = p->next;
    }

  gtk_main_quit();

  return FALSE;
}

void
on_controls_clicked                    (GtkButton       *button,
                                        gpointer         user_data)
{
  if (ToolBox)
    {
      gdk_window_raise(ToolBox -> window);
      return;
    }

  ToolBox = create_control_box(main_info);

  gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);

  gtk_object_set_user_data(GTK_OBJECT(ToolBox), button);

  gtk_widget_show(ToolBox);
}

void
on_go_fullscreen1_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  restore_mode = main_info->current_mode;

  zmisc_switch_mode(TVENG_CAPTURE_PREVIEW, main_info);
}

void
on_closed_caption1_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkCheckMenuItem *button = GTK_CHECK_MENU_ITEM(menuitem);
  gboolean status = button->active;

  zcs_bool(status, "closed_caption");

  if (!status)
    osd_clear();
}

void
on_videotext1_activate                 (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  /* Switch from TTX to Subtitles overlay, and viceversa */
  if (main_info->current_mode == TVENG_NO_CAPTURE)
    {
      if (get_ttxview_page(main_window, &zvbi_page, NULL))
	zmisc_overlay_subtitles(zvbi_page);
    }
  else
    zmisc_switch_mode(TVENG_NO_CAPTURE, main_info);
}

void
on_vbi_info1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  gtk_widget_show(build_vbi_info());
}

void
on_new_ttxview_activate		       (GtkMenuItem	*menuitem,
					gpointer	user_data)
{
  gtk_widget_show(build_ttxview());
}

void
on_go_capturing2_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (zmisc_switch_mode(TVENG_CAPTURE_READ, main_info) == -1)
    ShowBox(main_info->error, GNOME_MESSAGE_BOX_ERROR);
}

void
on_go_previewing2_activate             (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  if (zmisc_switch_mode(TVENG_CAPTURE_WINDOW, main_info) == -1)
    ShowBox(_("%s:\n"
	      "Try running as root \"zapping_fix_overlay\" in a console"),
	    GNOME_MESSAGE_BOX_ERROR, main_info->error);
}

gboolean
on_tv_screen_button_press_event        (GtkWidget       *widget,
					GdkEvent        *event,
					gpointer        user_data)
{
  GtkWidget * zapping = lookup_widget(widget, "zapping");
  GdkEventButton * bevent = (GdkEventButton *) event;
  GList *p;

  if (event->type != GDK_BUTTON_PRESS)
    return FALSE;

  switch (bevent->button)
    {
    case 3:
      {
	GtkMenu * menu = GTK_MENU(create_popup_menu1());
	/* it needs to be realized before operating on it */
	gtk_widget_realize(GTK_WIDGET(menu));
	add_channel_entries(menu, 1, 10, main_info);
	if (disable_preview)
	  {
	    widget = lookup_widget(GTK_WIDGET(menu), "go_fullscreen2");
	    gtk_widget_set_sensitive(widget, FALSE);
	    gtk_widget_hide(widget);
	    widget = lookup_widget(GTK_WIDGET(menu), "go_previewing2");
	    gtk_widget_set_sensitive(widget, FALSE);
	    gtk_widget_hide(widget);
	  }
	if (!zvbi_get_object())
	  {
	    widget = lookup_widget(GTK_WIDGET(menu), "separador6");
	    gtk_widget_set_sensitive(widget, FALSE);
	    gtk_widget_hide(widget);
	    widget = lookup_widget(GTK_WIDGET(menu), "videotext2");
	    gtk_widget_set_sensitive(widget, FALSE);
	    gtk_widget_hide(widget);
	    widget = lookup_widget(GTK_WIDGET(menu), "new_ttxview2");
	    gtk_widget_set_sensitive(widget, FALSE);
	    gtk_widget_hide(widget);
	  }
	else if (main_info->current_mode == TVENG_NO_CAPTURE)
	  {
	    widget = lookup_widget(GTK_WIDGET(menu), "videotext2");
	    z_change_menuitem(widget,
			      GNOME_STOCK_PIXMAP_TABLE_FILL,
			      _("Overlay this page"),
			      _("Return to windowed mode and use the current "
				"page as subtitles"));
	  }
	/* Remove capturing item if it's redundant */
	if ((!zvbi_get_object()) && (disable_preview))
	  {
	    gtk_widget_hide(lookup_widget(GTK_WIDGET(menu),
					  "separador3"));
	    widget = lookup_widget(GTK_WIDGET(menu), "go_capturing2");
	    gtk_widget_set_sensitive(widget, FALSE);
	    gtk_widget_hide(widget);
	  }

	if (zcg_bool(NULL, "hide_controls"))
	  z_change_menuitem(lookup_widget(GTK_WIDGET(menu),
					  "hide_controls1"),
			    GNOME_STOCK_PIXMAP_BOOK_OPEN,
			    _("Show controls"),
			    _("Show the menu and the toolbar"));

	process_ttxview_menu_popup(main_window, bevent, menu);
	/* Let plugins add their GUI to this context menu */
	p = g_list_first(plugin_list);
	while (p)
	  {
	    plugin_process_popup_menu(main_window, bevent, menu, 
			      (struct plugin_info*)p->data);
	    p = p->next;
	  }
	gtk_menu_popup(menu, NULL, NULL, NULL,
		       NULL, bevent->button, bevent->time);
	gtk_object_set_user_data(GTK_OBJECT(menu), zapping);
      }
      return TRUE;
    case 4:
      z_channel_up();
      return TRUE;
    case 5:
      z_channel_down();
      return TRUE;
    case 2:
      if (main_info->current_mode == TVENG_NO_CAPTURE)
	return FALSE;
      on_go_fullscreen1_activate(GTK_MENU_ITEM(lookup_widget(widget,
					     "go_fullscreen1")),
				NULL);
      break;
    default:
      break;
    }
  return FALSE;
}

/* Resize subwindow to wxh */
static void
resize_subwindow			(GdkWindow	*subwindow,
					 gint		w,
					 gint		h)
{
  gint sw_w, sw_h, mw_w, mw_h;
  GdkWindow *mw = gdk_window_get_toplevel(subwindow);

  gdk_window_get_size(mw, &mw_w, &mw_h);
  gdk_window_get_size(subwindow, &sw_w, &sw_h);

  w += (mw_w - sw_w);
  h += (mw_h - sw_h);

  gdk_window_resize(mw, w, h);
}

void
on_pal_big_activate		       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget *tv_screen = lookup_widget(main_window, "tv_screen");

  resize_subwindow(tv_screen->window, 768, 576);
}

void
on_rec601_pal_big_activate	       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget *tv_screen = lookup_widget(main_window, "tv_screen");

  resize_subwindow(tv_screen->window, 720, 576);
}

void
on_ntsc_big_activate		       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget *tv_screen = lookup_widget(main_window, "tv_screen");

  resize_subwindow(tv_screen->window, 640, 480);
}

void
on_pal_small_activate		       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget *tv_screen = lookup_widget(main_window, "tv_screen");

  resize_subwindow(tv_screen->window, 768/2, 576/2);
}

void
on_rec601_pal_small_activate	       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget *tv_screen = lookup_widget(main_window, "tv_screen");

  resize_subwindow(tv_screen->window, 704/2, 576/2);
}

void
on_ntsc_small_activate		       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget *tv_screen = lookup_widget(main_window, "tv_screen");

  resize_subwindow(tv_screen->window, 640/2, 480/2);
}
