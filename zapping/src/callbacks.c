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

/* previously selected mode (vs. current mode) */
enum tveng_capture_mode last_mode = -1;

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

  if (!window)
    return;

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

/* NB this isn't used by zapzilla frontend */
gboolean
quit_cmd				(GtkWidget *	widget,
					 gint		argc,
					 gchar **	argv,
					 gpointer	user_data)
{
  GList *p;

  if (!main_window)
    return FALSE;

  flag_exit_program = TRUE;

  UpdateCoords (main_window->window);

  /* Tell the widget that the GUI is going to be closed */
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_remove_gui (GNOME_APP (main_window), 
			 (struct plugin_info *) p->data);
      p = p->next;
    }

  gtk_main_quit();

  return TRUE;
}

static gboolean
switch_mode				(enum tveng_capture_mode mode)
{
  switch (mode)
    {
    case TVENG_CAPTURE_PREVIEW:
      zmisc_switch_mode (TVENG_CAPTURE_PREVIEW, main_info);
      break;

    case TVENG_CAPTURE_WINDOW:
      if (zmisc_switch_mode (TVENG_CAPTURE_WINDOW, main_info) == -1)
	ShowBox(_("%s:\n"
		  "Try running as root \"zapping_fix_overlay\" in a console"),
		GNOME_MESSAGE_BOX_ERROR, main_info->error);
      break;

    case TVENG_CAPTURE_READ:
      if (zmisc_switch_mode(TVENG_CAPTURE_READ, main_info) == -1)
	ShowBox(main_info->error, GNOME_MESSAGE_BOX_ERROR);
      break;

#ifdef HAVE_LIBZVBI
    case TVENG_NO_CAPTURE:
      /* Switch from TTX to Subtitles overlay, and vice versa */
#if 0 /* needs some other solution */
      if (main_info->current_mode == TVENG_NO_CAPTURE)
	{
	  /* implicit switch to previous_mode */
	  if (get_ttxview_page(main_window, &zvbi_page, NULL))
	    zmisc_overlay_subtitles(zvbi_page);
	}
      else
#endif
	zmisc_switch_mode(TVENG_NO_CAPTURE, main_info);

      break;
#endif

    default:
      return FALSE;
    }

  return TRUE;
}

static enum tveng_capture_mode
str2tcm					(gchar *	s)
{
  if (!s || !*s)
    return -1;
  else if (strcmp (s, "fullscreen") == 0)
    return TVENG_CAPTURE_PREVIEW;
  else if (strcmp (s, "preview") == 0)
    return TVENG_CAPTURE_WINDOW;
  else if (strcmp (s, "capture") == 0)
    return TVENG_CAPTURE_READ;
  else if (strcmp (s, "teletext") == 0)
    return TVENG_NO_CAPTURE;
  else
    return -1;
}

gboolean
switch_mode_cmd				(GtkWidget *	widget,
					 gint		argc,
					 gchar **	argv,
					 gpointer	user_data)
{
  enum tveng_capture_mode old_mode = main_info->current_mode;

  if (argc < 2)
    return FALSE;

  if (!switch_mode (str2tcm (argv[1])))
    return FALSE;

  if (old_mode != main_info->current_mode)
    last_mode = old_mode;

  return TRUE;
}

gboolean
toggle_mode_cmd				(GtkWidget *	widget,
					 gint		argc,
					 gchar **	argv,
					 gpointer	user_data)
{
  enum tveng_capture_mode mode, curr_mode = main_info->current_mode;

  if (argc >= 2)
    mode = str2tcm (argv[1]);
  else
    mode = curr_mode;

  if (curr_mode == mode && mode != TVENG_NO_CAPTURE)
    {
      /* swap requested (current) mode and last mode */

      if (!switch_mode (last_mode))
        return FALSE;

      last_mode = mode;
    }
  else
    {
      /* switch to requested mode */

      if (!switch_mode (mode))
        return FALSE;

      last_mode = curr_mode;
    }

  return TRUE;
}

/* XXX improve me */
gboolean
subtitle_overlay_cmd			(GtkWidget *	widget,
					 gint		argc,
					 gchar **	argv,
					 gpointer	user_data)
{

#ifdef HAVE_LIBZVBI

  if (argc > 1)
    return FALSE; /* page number todo */

  if (main_info->current_mode == TVENG_NO_CAPTURE)
    {
      if (!switch_mode (last_mode))
        return FALSE;

      last_mode = TVENG_NO_CAPTURE;
    }

  if (get_ttxview_page(main_window, &zvbi_page, NULL))
    {
      zmisc_overlay_subtitles(zvbi_page);
      return TRUE;
    }

#endif

  return FALSE;
}

void
on_videotext3_pressed			(GtkWidget	*w,
					 gpointer	user_data)
{
  /* Can't swap commands here. D'oh! */
  if (main_info->current_mode == TVENG_NO_CAPTURE)
    cmd_execute (w, "subtitle_overlay");
  else
    cmd_execute (w, "switch_mode teletext");
}

static gboolean mute_controls = TRUE;
static gboolean mute_osd = TRUE;

/**
 * set_mute1:
 * @mode: 0 - off, 1 - on, 2 - toggle, 3 - query driver.
 * @controls: Update the controls box, if open.
 * @osd: OSD mute state, if in fullscreen mode.
 * 
 * Switch audio on or off.
 * 
 * Return value: 
 * %TRUE on success.
 **/
gboolean
set_mute1				(gint	        mode,
					 gboolean	controls,
					 gboolean	osd)
{
  static gboolean recursion;
  GtkCheckMenuItem *check;
  GtkWidget *button;
  gint mute;

  if (recursion)
    return TRUE;

  recursion = TRUE;

  if (mode >= 2)
    {
      if ((mute = tveng_get_mute(main_info)) < 0)
	{
	  printv("tveng_get_mute failed\n");
	  recursion = FALSE;
	  return FALSE;
	}

      if (mode == 2)
	mute = !mute;
    }
  else
    mute = !!mode;

  if (mode <= 2)
    {
      if (tveng_set_mute(mute, main_info) < 0)
	{
	  printv("tveng_set_mute failed\n");
	  recursion = FALSE;
	  return FALSE;
	}
    }

  /* Reflect change in GUI */

  mute_controls = controls;
  mute_osd = osd;

  /* The if's here break the signal -> change -> signal recursion */

  button = lookup_widget (main_window, "mute1");

  check = GTK_CHECK_MENU_ITEM (lookup_widget (main_window, "mute2"));

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)) != mute)
    ORC_BLOCK (gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), mute));
  else if (check->active == mute)
    mute_controls = FALSE;

  if (check->active != mute)
    ORC_BLOCK (gtk_check_menu_item_set_active (check, mute));

  if (mute_controls)
    update_control_box (main_info);

#ifdef HAVE_LIBZVBI
  if (mute_osd)
    if (main_info->current_mode == TVENG_CAPTURE_PREVIEW)
      // XXX or toolbar hidden
      osd_render_sgml(NULL, mute ?
		      _("<blue>audio off</blue>") :
		      _("<yellow>AUDIO ON</yellow>"));
#endif

  mute_controls = TRUE;
  mute_osd = TRUE;

  recursion = FALSE;

  return TRUE;
}

void
on_mute1_toggled			(GtkWidget	*w,
					 gpointer	user_data)
{
  GtkWidget *widget = lookup_widget (main_window, "mute1");
  gboolean state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));

  set_mute1 (!!state, mute_controls, mute_osd);
}

void
on_mute2_activate            	       (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkCheckMenuItem *button = GTK_CHECK_MENU_ITEM(menuitem);
  gboolean state = button->active;

  set_mute1 (!!state, mute_controls, mute_osd);
}

/**
 * mute_cmd:
 * @widget: Ignored.
 * @argc: 1 or 2.
 * @argv: Non-zero number mutes, zero or no number unmutes,
 *   without arguments toggles mute.
 * 
 * Switch audio on or off.
 *
 * Return value: 
 * %TRUE on success.
 **/
gboolean
mute_cmd				(GtkWidget *	widget,
					 gint		argc,
					 gchar **	argv,
					 gpointer	user_data)
{
  gint value = 2; /* toggle */

  if (argc > 1)
    value = !!strtol (argv[1], NULL, 0);

  set_mute1 (value, mute_controls, mute_osd);

  return TRUE;
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
on_closed_caption1_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
#ifdef HAVE_LIBZVBI
  GtkCheckMenuItem *button = GTK_CHECK_MENU_ITEM(menuitem);
  gboolean status = button->active;

  zcs_bool(status, "closed_caption");

  if (!status)
    osd_clear();
#endif
}

void
on_vbi_info1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
#ifdef HAVE_LIBZVBI
  gtk_widget_show(zvbi_build_network_info());
#endif
}

void
on_program_info1_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
#ifdef HAVE_LIBZVBI
  gtk_widget_show(zvbi_build_program_info());
#endif
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

#ifdef HAVE_LIBZVBI
	if (!zvbi_get_object())
#endif
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
#ifdef HAVE_LIBZVBI
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
#else
	if (disable_preview)
#endif
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
#ifdef HAVE_LIBZVBI
	process_ttxview_menu_popup(main_window, bevent, menu);
#endif

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
      cmd_execute (widget, "channel_up");
      return TRUE;
    case 5:
      cmd_execute (widget, "channel_down");
      return TRUE;
    case 2:
      if (main_info->current_mode == TVENG_NO_CAPTURE)
	return FALSE;
      cmd_execute (widget, "switch_mode fullscreen");
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

void
vbi_gui_sensitive (gboolean on)
{
  if (!on)
    {
      printv("VBI disabled, removing GUI items\n");
      gtk_widget_set_sensitive(lookup_widget(main_window, "separador5"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "separador5"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "videotext1"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "videotext1"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "vbi_info1"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "vbi_info1"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "program_info1"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "program_info1"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "videotext3"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "videotext3"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "new_ttxview"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "new_ttxview"));
      gtk_widget_set_sensitive(lookup_widget(main_window,
					     "closed_caption1"),
			       FALSE);
      gtk_widget_hide(lookup_widget(main_window, "closed_caption1"));
      gtk_widget_hide(lookup_widget(main_window, "separator8"));
      /* Set the capture mode to a default value and disable VBI */
      if (zcg_int(NULL, "capture_mode") == TVENG_NO_CAPTURE)
	zcs_int(TVENG_CAPTURE_READ, "capture_mode");
    }
  else
    {
      printv("VBI enabled, showing GUI items\n");
      gtk_widget_set_sensitive(lookup_widget(main_window, "separador5"),
			       TRUE);
      gtk_widget_show(lookup_widget(main_window, "separador5"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "videotext1"),
			       TRUE);
      gtk_widget_show(lookup_widget(main_window, "videotext1"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "vbi_info1"),
			       TRUE);
      gtk_widget_show(lookup_widget(main_window, "vbi_info1"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "program_info1"),
			       TRUE);
      gtk_widget_show(lookup_widget(main_window, "program_info1"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "videotext3"),
			       TRUE);
      gtk_widget_show(lookup_widget(main_window, "videotext3"));
      gtk_widget_set_sensitive(lookup_widget(main_window, "new_ttxview"),
			       TRUE);
      gtk_widget_show(lookup_widget(main_window, "new_ttxview"));
      gtk_widget_set_sensitive(lookup_widget(main_window,
					     "closed_caption1"),
			       TRUE);
      gtk_widget_show(lookup_widget(main_window, "closed_caption1"));
      gtk_widget_show(lookup_widget(main_window, "separator8"));
    }
}
