/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2001 Iñaki García Etxebarria
 * Copyright (C) 2003 Michael H. Schimek
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

/* $Id: window.c,v 1.2 2004-09-26 13:30:23 mschimek Exp $ */

#include "config.h"

#include "zmisc.h"
#include "zvbi.h"
#define ZCONF_DOMAIN "/zapping/ttxview/"
#include "zconf.h"
#include "keyboard.h"
#include "properties-handler.h"
#include "remote.h"

#include "main.h"
#include "window.h"

static GObjectClass *		parent_class;

static void
on_vbi_model_changed		(ZModel *		zmodel _unused_,
				 TeletextWindow *	window)
{
  gtk_widget_destroy (GTK_WIDGET (window));
}

static void
on_main_menu_bookmarks_changed	(ZModel *		zmodel _unused_,
				 TeletextWindow *	window)
{
  gtk_menu_item_set_submenu (window->bookmarks_menu,
			     bookmarks_menu_new (window->view));
}

static void
on_main_menu_bookmarks_destroy	(GObject *		object _unused_,
				 TeletextWindow *	window)
{
  if (bookmarks.zmodel)
    g_signal_handlers_disconnect_matched
      (G_OBJECT (bookmarks.zmodel),
       G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
       0, 0, NULL, G_CALLBACK (on_main_menu_bookmarks_changed), window);
}

static const char *
menu_description =
"<ui>"
" <menubar name='MainMenu'>"
"  <menu action='FileSubmenu'>"
"   <menuitem action='NewWindow'/>"
"   <separator/>"
"   <menuitem action='Export'/>"
"   <separator/>"
"   <menuitem action='Close'/>"
"   <menuitem action='Quit'/>"
"  </menu>"
"  <menu action='EditSubmenu'>"
"   <menuitem action='Search'/>"
"   <separator/>"
"   <menuitem action='Preferences'/>"
"  </menu>"
"  <menu action='ViewSubmenu'>"
"   <menuitem action='ViewToolbar'/>"
"   <menuitem action='ViewStatusbar'/>"
"   <separator/>"
"   <menuitem action='Colors'/>"
"  </menu>"
"  <menu action='GoSubmenu'>"
"   <menuitem action='HistoryBack'/>"
"   <menuitem action='HistoryForward'/>"
"   <separator/>"
"   <menuitem action='Home'/>"
"  </menu>"
"  <menu action='BookmarksSubmenu'>"
"  </menu>"
" </menubar>"
"</ui>";

static void
create_main_menu		(TeletextWindow *	window)
{
  GtkUIManager *ui_manager;
  GError *error;
  GtkWidget *widget;

  ui_manager = gtk_ui_manager_new ();
  gtk_ui_manager_insert_action_group
    (ui_manager, window->action_group, APPEND);
  gtk_ui_manager_insert_action_group
    (ui_manager, window->view->action_group, APPEND);
  gtk_ui_manager_insert_action_group
    (ui_manager, teletext_action_group, APPEND);
  gtk_ui_manager_insert_action_group
    (ui_manager, zapping->generic_action_group, APPEND);

  error = NULL;
  if (!gtk_ui_manager_add_ui_from_string
      (ui_manager, menu_description, NUL_TERMINATED, &error))
    {
      g_message ("Cannot build Teletext window menu:\n%s", error->message);
      g_error_free (error);
      exit (EXIT_FAILURE);
    }

  widget = gtk_ui_manager_get_widget (ui_manager, "/MainMenu");
  gnome_app_set_menus (GNOME_APP (window), GTK_MENU_BAR (widget));

  widget = gtk_ui_manager_get_widget (ui_manager,
				      "/MainMenu/BookmarksSubmenu");
  window->bookmarks_menu = GTK_MENU_ITEM (widget);

  gtk_menu_item_set_submenu (window->bookmarks_menu,
			     bookmarks_menu_new (window->view));

  /* This is plain stupid, but I see no way to create the bookmarks
     submenu on the fly, when the menu is opened by the user. Also
     for this reason there is no Go > (subtitles) or Go > (hotlist).
     There just is no signal to update when these change. */
  g_signal_connect (G_OBJECT (bookmarks.zmodel), "changed",
		    G_CALLBACK (on_main_menu_bookmarks_changed), window);
  g_signal_connect (G_OBJECT (window->bookmarks_menu), "destroy",
		    G_CALLBACK (on_main_menu_bookmarks_destroy), window);
}

static gboolean
key_press_event			(GtkWidget *		widget,
				 GdkEventKey *		event)
{
  TeletextWindow *window = TELETEXT_WINDOW (widget);

  return (teletext_view_on_key_press (widget, event, window->view)
	  || on_user_key_press (widget, event, NULL)
	  || on_picture_size_key_press (widget, event, NULL));
}

static gboolean
button_press_event		(GtkWidget *		widget,
				 GdkEventButton *	event)
{
  TeletextWindow *window = TELETEXT_WINDOW (widget);
  GtkWidget *menu;  
  vbi_link ld;

  switch (event->button)
    {
    case 3: /* right button, context menu */
      teletext_view_vbi_link_from_pointer_position
	(window->view, &ld, (gint) event->x, (gint) event->y);

      if ((menu = teletext_view_popup_menu_new (window->view, &ld, TRUE)))
	gtk_menu_popup (GTK_MENU (menu),
			NULL, NULL, NULL, NULL,
			event->button, event->time);

      return TRUE; /* handled */

    default:
      break;
    }

  return FALSE; /* pass on */
}

static void
close_action			(GtkAction *		action _unused_,
				 TeletextWindow *	window)
{
  gtk_widget_destroy (GTK_WIDGET (window));
}

/* preliminary */
static void
new_window_action		(GtkAction *		action _unused_,
				 TeletextWindow *	window)
{
  on_python_command1 (GTK_WIDGET (window), "zapping.ttx_open_new()");
}

static GtkActionEntry
actions [] = {
  { "FileSubmenu", NULL, N_("_File"), NULL, NULL, NULL },
  { "NewWindow", GTK_STOCK_NEW, N_("_New Window"), NULL,
    NULL, G_CALLBACK (new_window_action) },
  { "Close", GTK_STOCK_CLOSE, NULL, NULL, NULL, G_CALLBACK (close_action) },
  { "EditSubmenu", NULL, N_("_Edit"), NULL, NULL, NULL },
  { "ViewSubmenu", NULL, N_("_View"), NULL, NULL, NULL },
  { "BookmarksSubmenu", NULL, N_("_Bookmarks"), NULL, NULL, NULL },
};

static void
view_statusbar_action		(GtkToggleAction *	toggle_action,
				 TeletextWindow *	window)
{
  if (gtk_toggle_action_get_active (toggle_action))
    {
      /* Adding a hidden status bar is impossible, we have to add when the
	 status bar becomes first visible. */
      if (!window->statusbar_added)
	{
	  window->statusbar_added = TRUE;
	  gnome_app_set_statusbar (&window->app,
				   GTK_WIDGET (window->view->appbar));
	}

      gtk_widget_show (GTK_WIDGET (window->view->appbar));
    }
  else if (window->statusbar_added)
    {
      gtk_widget_hide (GTK_WIDGET (window->view->appbar));
    }
}

static void
view_toolbar_action		(GtkToggleAction *	toggle_action,
				 TeletextWindow *	window)
{
  BonoboDockItem *dock_item;

  if (gtk_toggle_action_get_active (toggle_action))
    {
      /* Adding a hidden toolbar is impossible, we have to add when the
	 toolbar becomes first visible. */
      if (!window->toolbar_added)
	{
	  window->toolbar_added = TRUE;
	  gnome_app_set_toolbar (&window->app,
				 GTK_TOOLBAR (window->view->toolbar));
	}

      dock_item = gnome_app_get_dock_item_by_name
	(&window->app, GNOME_APP_TOOLBAR_NAME);

      gtk_widget_show (GTK_WIDGET (dock_item));
    }
  else if (window->toolbar_added)
    {
      dock_item = gnome_app_get_dock_item_by_name
	(&window->app, GNOME_APP_TOOLBAR_NAME);

      gtk_widget_hide (GTK_WIDGET (dock_item));
    }
}

static GtkToggleActionEntry
toggle_actions [] = {
  { "ViewToolbar", NULL, N_("Toolbar"), NULL,
    NULL, G_CALLBACK (view_toolbar_action), FALSE },
  { "ViewStatusbar", NULL, N_("Statusbar"), NULL,
    NULL, G_CALLBACK (view_statusbar_action), FALSE },
};

static void
instance_finalize		(GObject *		object)
{
  TeletextWindow *window = TELETEXT_WINDOW (object);

  teletext_windows = g_list_remove (teletext_windows, window);

  g_signal_handlers_disconnect_matched
    (G_OBJECT (zvbi_get_model ()),
     G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
     0, 0, NULL, G_CALLBACK (on_vbi_model_changed), window);

  parent_class->finalize (object);
}

static void
instance_init			(GTypeInstance *	instance,
				 gpointer		g_class _unused_)
{
  TeletextWindow *window = (TeletextWindow *) instance;
  GnomeApp *app;
  GtkWidget *widget;
  GObject *object;
  GtkAction *action;
  GtkToggleAction *toggle_action;

  window->action_group = gtk_action_group_new ("TeletextWindowActions");
#ifdef ENABLE_NLS
  gtk_action_group_set_translation_domain (window->action_group,
					   GETTEXT_PACKAGE);
#endif					   
  gtk_action_group_add_actions (window->action_group,
				actions, G_N_ELEMENTS (actions),
				/* user_data */ window);
  gtk_action_group_add_toggle_actions (window->action_group,
				       toggle_actions,
				       G_N_ELEMENTS (toggle_actions),
				       /* user_data */ window);

  /* We add the submenu ourselves. Make sure the menu item is
     visible despite initially without menu. */
  action = gtk_action_group_get_action (window->action_group,
					"BookmarksSubmenu");
  g_object_set (G_OBJECT (action), "hide-if-empty", FALSE, NULL);

  app = GNOME_APP (window);
  gnome_app_construct (app, "Zapping", "Zapzilla");

  widget = teletext_view_new ();
  gtk_widget_show (widget);
  window->view = TELETEXT_VIEW (widget);

  object = G_OBJECT (window);
  g_object_set_data (object, "TeletextView", window->view);

  gtk_widget_set_size_request (widget, 260, 250);

  gnome_app_set_contents (app, widget);

  create_main_menu (window);

  {
    widget = teletext_toolbar_new (window->view->action_group);
    gtk_widget_show_all (widget);

    window->view->toolbar = TELETEXT_TOOLBAR (widget);

    toggle_action = GTK_TOGGLE_ACTION (gtk_action_group_get_action
				       (window->action_group, "ViewToolbar"));
    z_toggle_action_connect_gconf_key
      (toggle_action, "/apps/zapping/plugins/teletext/window/view_toolbar");
    /* Adds the toolbar if necessary. */
    view_toolbar_action (toggle_action, window);
  }

  {
    widget = gnome_appbar_new (/* has_progress */ FALSE,
			       /* has_status */ TRUE,
			       GNOME_PREFERENCES_NEVER);

    window->view->appbar = GNOME_APPBAR (widget);

    toggle_action = GTK_TOGGLE_ACTION (gtk_action_group_get_action
				       (window->action_group,
					"ViewStatusbar"));
    z_toggle_action_connect_gconf_key
      (toggle_action, "/apps/zapping/plugins/teletext/window/view_statusbar");
    /* Adds the status bar if necessary. */
    view_statusbar_action (toggle_action, window);
  }

  g_signal_connect (G_OBJECT (zvbi_get_model ()), "changed",
		    G_CALLBACK (on_vbi_model_changed), window);

  teletext_windows = g_list_append (teletext_windows, window);
}

GtkWidget *
teletext_window_new		(void)
{
  if (!zvbi_get_object ())
    return NULL;

  return GTK_WIDGET (g_object_new (TYPE_TELETEXT_WINDOW, NULL));
}

static void
class_init			(gpointer		g_class,
				 gpointer		class_data _unused_)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (g_class);
  widget_class = GTK_WIDGET_CLASS (g_class);
  parent_class = g_type_class_peek_parent (g_class);

  object_class->finalize = instance_finalize;

  widget_class->key_press_event = key_press_event;
  widget_class->button_press_event = button_press_event;
}

GType
teletext_window_get_type		(void)
{
  static GType type = 0;
  
  if (!type)
    {
      GTypeInfo info;

      CLEAR (info);

      info.class_size = sizeof (TeletextWindowClass);
      info.class_init = class_init;
      info.instance_size = sizeof (TeletextWindow);
      info.instance_init = instance_init;

      type = g_type_register_static (GNOME_TYPE_APP,
				     "TeletextWindow",
				     &info, (GTypeFlags) 0);
    }

  return type;
}
