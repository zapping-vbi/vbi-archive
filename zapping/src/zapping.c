/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2004 Michael H. Schimek
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

/* $Id: zapping.c,v 1.4 2004-09-23 00:59:59 mschimek Exp $ */

#include "config.h"

#include "audio.h"
#include "interface.h"
#include "v4linterface.h"
#include "plugins.h"
#include "properties-handler.h"
#include "subtitle.h"
#include "zconf.h"
#include "zmisc.h"
#include "zvideo.h"
#include "zapping.h"
#include "remote.h"

static GObjectClass *		parent_class;

static void
quit_action			(GtkAction *		action _unused_,
				 Zapping *		z)
{
  on_python_command1 (GTK_WIDGET (z), "zapping.quit()");
}

static void
preferences_action		(GtkAction *		action _unused_,
				 Zapping *		z)

{
  on_python_command1 (GTK_WIDGET (z), "zapping.properties()");
}

static void
plugins_action			(GtkAction *		action _unused_,
				 Zapping *		z)
{
  on_python_command1 (GTK_WIDGET (z), "zapping.plugin_properties()");
}

static void
channel_editor_action		(GtkAction *		action _unused_,
				 Zapping *		z)
{
  on_python_command1 (GTK_WIDGET (z), "zapping.channel_editor()");
}

static void
fullscreen_action		(GtkAction *		action _unused_,
				 Zapping *		z)
{
  on_python_command1 (GTK_WIDGET (z), "zapping.switch_mode('fullscreen')");
}

static void
overlay_action			(GtkAction *		action _unused_,
				 Zapping *		z)
{
  on_python_command1 (GTK_WIDGET (z), "zapping.switch_mode('preview')");
}

static void
capture_action			(GtkAction *		action _unused_,
				 Zapping *		z)
{
  on_python_command1 (GTK_WIDGET (z), "zapping.switch_mode('capture')");
}

static void
teletext_action			(GtkAction *		action _unused_,
				 Zapping *		z)
{
  on_python_command1 (GTK_WIDGET (z), "zapping.switch_mode('teletext')");
}

static void
restore_video_action		(GtkAction *		action _unused_,
				 Zapping *		z)
{
  on_python_command1 (GTK_WIDGET (z), "zapping.toggle_mode()");
}

static void
new_teletext_action		(GtkAction *		action _unused_,
				 Zapping *		z)
{
  on_python_command1 (GTK_WIDGET (z), "zapping.ttx_open_new()");
}

static void
mute_action			(GtkToggleAction *	toggle_action,
				 Zapping *		z _unused_)
{
  gboolean mute;

  mute = gtk_toggle_action_get_active (toggle_action);
  set_mute (mute, TRUE, TRUE);
}

static void
subtitles_action		(GtkToggleAction *	toggle_action,
				 Zapping *		z _unused_)
{
  python_command_printf (NULL, "zapping.closed_caption(%u)",
			 gtk_toggle_action_get_active (toggle_action));
}

static void
zconf_hook_subtitles		(const gchar *		key _unused_,
				 gpointer		new_value_ptr,
				 gpointer		user_data)
{
  gboolean active = * (gboolean *) new_value_ptr;
  GtkToggleAction *toggle_action = user_data;

  if (active != gtk_toggle_action_get_active (toggle_action))
    gtk_toggle_action_set_active (toggle_action, active);
}

static void
view_menu_action		(GtkToggleAction *	toggle_action,
				 Zapping *		z)
{
  BonoboDockItem *dock_item;

  if (gtk_toggle_action_get_active (toggle_action))
    {
      /* Adding a hidden menu is impossible, we have to add when the
	 menu becomes first visible. */
      if (!z->menubar_added)
	{
	  z->menubar_added = TRUE;
	  gnome_app_set_menus (&z->app, z->menubar);
	}

      dock_item = gnome_app_get_dock_item_by_name
	(&z->app, GNOME_APP_MENUBAR_NAME);

      gtk_widget_show (GTK_WIDGET (dock_item));
    }
  else if (z->menubar_added)
    {
      dock_item = gnome_app_get_dock_item_by_name
	(&z->app, GNOME_APP_MENUBAR_NAME);

      gtk_widget_hide (GTK_WIDGET (dock_item));
    }

  gtk_widget_queue_resize (GTK_WIDGET (z));
}

static void
view_toolbar_action		(GtkToggleAction *	toggle_action,
				 Zapping *		z)
{
  BonoboDockItem *dock_item;

  if (gtk_toggle_action_get_active (toggle_action))
    {
      /* Adding a hidden toolbar is impossible, we have to add when the
	 toolbar becomes first visible. */
      if (!z->toolbar_added)
	{
	  z->toolbar_added = TRUE;
	  gnome_app_set_toolbar (&z->app, z->toolbar);
	}

      dock_item = gnome_app_get_dock_item_by_name
	(&z->app, GNOME_APP_TOOLBAR_NAME);

      gtk_widget_show (GTK_WIDGET (dock_item));
    }
  else if (z->toolbar_added)
    {
      dock_item = gnome_app_get_dock_item_by_name
	(&z->app, GNOME_APP_TOOLBAR_NAME);

      gtk_widget_hide (GTK_WIDGET (dock_item));
    }

  gtk_widget_queue_resize (GTK_WIDGET (z));
}

static PyObject *
py_hide_controls		(PyObject *		self _unused_,
				 PyObject *		args)
{
  int hide;
  GtkToggleAction *toggle_action;

  if (!zapping)
    py_return_false;

  hide = 2; /* toggle */

  if (!ParseTuple (args, "|i", &hide))
    g_error ("zapping.hide_controls(|i)");

  toggle_action = GTK_TOGGLE_ACTION
    (gtk_action_group_get_action (zapping->generic_action_group, "ViewMenu"));

  if (hide > 1)
    hide = !!gtk_toggle_action_get_active (toggle_action);

  gtk_toggle_action_set_active (toggle_action, !hide);

  toggle_action = GTK_TOGGLE_ACTION
    (gtk_action_group_get_action (zapping->generic_action_group,
				  "ViewToolbar"));

  gtk_toggle_action_set_active (toggle_action, !hide);

  py_return_none;
}

static void
keep_window_on_top_action	(GtkToggleAction *	toggle_action,
				 Zapping *		z)
{
  gboolean keep;

  keep = gtk_toggle_action_get_active (toggle_action);
  x11_window_on_top (GTK_WINDOW (z), keep);
}

static PyObject *
py_keep_on_top			(PyObject *		self _unused_,
				 PyObject *		args)
{
  int keep;

  if (!zapping)
    py_return_false;

  keep = 2; /* toggle */

  if (!ParseTuple (args, "|i", &keep))
    g_error ("zapping.keep_on_top(|i)");

  if (have_wm_hints)
    {
      GtkToggleAction *action;

      action = GTK_TOGGLE_ACTION (gtk_action_group_get_action
				  (zapping->generic_action_group,
				   "KeepWindowOnTop"));
      if (keep > 1)
        keep = !gtk_toggle_action_get_active (action);

      gtk_toggle_action_set_active (action, keep);
    }

  py_return_none;
}

static void
help_contents_action		(GtkAction *		action _unused_,
				 Zapping *		z _unused_)
{
  gnome_help_display ("zapping", NULL, /* GError */ NULL);
}

static void
about_action			(GtkAction *		action _unused_,
				 Zapping *		z)
{
  on_python_command1 (GTK_WIDGET (z), "zapping.about()");
}

static void
channel_up_action		(GtkAction *		action _unused_,
				 Zapping *		z)
{
  on_python_command1 (GTK_WIDGET (z), "zapping.channel_up()");
}

static void
channel_down_action		(GtkAction *		action _unused_,
				 Zapping *		z)
{
  on_python_command1 (GTK_WIDGET (z), "zapping.channel_down()");
}

static void
controls_action			(GtkAction *		action _unused_,
				 Zapping *		z)
{
  on_python_command1 (GTK_WIDGET (z), "zapping.control_box()");
}

static GtkActionEntry
generic_actions [] = {
  { "FileSubmenu", NULL, N_("_File"), NULL, NULL, NULL },
  { "Quit", GTK_STOCK_QUIT, NULL, NULL, NULL, G_CALLBACK (quit_action) },
  { "EditSubmenu", NULL, N_("_Edit"), NULL, NULL, NULL },
  { "Preferences", GTK_STOCK_PREFERENCES, NULL, NULL,
    NULL, G_CALLBACK (preferences_action) },
  { "Plugins", NULL, N_("P_lugins"), NULL,
    NULL, G_CALLBACK (plugins_action) },
  { "ChannelEditor", NULL /* 2.6 GTK_STOCK_EDIT */, N_("_Channels"), NULL,
    NULL, G_CALLBACK (channel_editor_action) },
  { "ViewSubmenu", NULL, N_("_View"), NULL, NULL, NULL },
  { "Fullscreen", GTK_STOCK_EXECUTE, N_("_Fullscreen"), "<Control>f",
    NULL, G_CALLBACK (fullscreen_action) },
  { "Overlay", GTK_STOCK_EXECUTE, N_("_Overlay mode"), "<Control>o",
    NULL, G_CALLBACK (overlay_action) },
  { "Capture", GTK_STOCK_EXECUTE, N_("_Capture mode"), "<Control>c",
    NULL, G_CALLBACK (capture_action) },
  { "ChannelsSubmenu", NULL, N_("_Channels"), NULL, NULL, NULL },
  { "HelpSubmenu", NULL, N_("_Help"), NULL, NULL, NULL },
  { "HelpContents", GTK_STOCK_HELP, N_("_Contents"), "F1",
    NULL, G_CALLBACK (help_contents_action) },
  { "About", GNOME_STOCK_ABOUT, N_("_About"), NULL,
    NULL, G_CALLBACK (about_action) },
  { "PopupSubmenu", NULL, "Dummy", NULL, NULL, NULL },
  { "Appearance", NULL, N_("Appearance"), NULL, NULL, NULL },
  { "ChannelUp", GTK_STOCK_GO_UP, N_("Ch. Up"), NULL,
    N_("Switch to higher channel"), G_CALLBACK (channel_up_action) },
  { "ChannelDown", GTK_STOCK_GO_DOWN, N_("Ch. Down"), NULL,
    N_("Switch to lower channel"), G_CALLBACK (channel_down_action) },
  { "Controls", GTK_STOCK_EXECUTE, N_("Controls"), NULL,
    N_("Change picture controls"), G_CALLBACK (controls_action) },
};

static GtkToggleActionEntry
generic_toggle_actions [] = {
  { "Mute", "zapping-mute", N_("_Mute"), "<Control>a",
    N_("Switch audio on or off"), G_CALLBACK (mute_action), FALSE },
  { "ViewMenu", NULL, N_("Menu"), NULL,
    NULL, G_CALLBACK (view_menu_action), TRUE },
  { "ViewToolbar", NULL, N_("Toolbar"), NULL,
    NULL, G_CALLBACK (view_toolbar_action), TRUE },
  { "KeepWindowOnTop", NULL, N_("Keep window on top"), NULL,
    NULL, G_CALLBACK (keep_window_on_top_action), FALSE },
};

static GtkActionEntry
vbi_actions [] = {
  { "TeletextSubmenu", NULL, N_("_Teletext"), NULL, NULL, NULL },
  { "SubtitlesSubmenu", NULL, N_("_Subtitles"), NULL, NULL, NULL },
  { "BookmarksSubmenu", NULL, N_("_Bookmarks"), NULL, NULL, NULL },
  { "Teletext", "zapping-teletext", N_("_Teletext"), "<Control>t",
    N_("Activate Teletext mode"), G_CALLBACK (teletext_action) },
  { "RestoreVideo", "zapping-video", N_("_Video"), "<Control>v",
    N_("Return to video mode"), G_CALLBACK (restore_video_action) },
  { "NewTeletext", NULL, N_("_New Teletext View"), "<Control>n",
    NULL, G_CALLBACK (new_teletext_action) },
};

static GtkToggleActionEntry
vbi_toggle_actions [] = {
  { "Subtitles", "zapping-subtitle", N_("_Subtitles"), "<Control>u",
    N_("Switch subtitles on or off"), G_CALLBACK (subtitles_action), FALSE },
};

static const char *
ui_description =
"<ui>"
" <menubar name='MainMenu'>"
"  <menu action='FileSubmenu'>"
"   <menuitem action='Quit'/>"
"  </menu>"
"  <menu action='EditSubmenu'>"
"   <menuitem action='Preferences'/>"
"   <menuitem action='Plugins'/>"
"   <menuitem action='ChannelEditor'/>"
"  </menu>"
"  <menu action='ViewSubmenu'>"
"   <menuitem action='Fullscreen'/>"
"   <menuitem action='Overlay'/>"
"   <menuitem action='Capture'/>"
"   <separator/>"
"   <menuitem action='Teletext'/>"
"   <menuitem action='RestoreVideo'/>"
"   <menuitem action='NewTeletext'/>"
"   <separator/>"
"   <menuitem action='Mute'/>"
"   <menuitem action='Subtitles'/>"
"   <separator/>"
"   <menuitem action='ViewMenu'/>"
"   <menuitem action='ViewToolbar'/>"
"   <menuitem action='KeepWindowOnTop'/>"
"  </menu>"
"  <menu action='ChannelsSubmenu'>"
"  </menu>"
"  <menu action='HelpSubmenu'>"
"   <menuitem action='HelpContents'/>"
"   <separator/>"
"   <menuitem action='About'/>"
"  </menu>"
" </menubar>"
" <toolbar name='Toolbar'>"
"  <toolitem action='ChannelUp'/>"
"  <toolitem action='ChannelDown'/>"
"  <separator/>"
"  <toolitem action='Controls'/>"
"  <toolitem action='Mute'/>"
"  <toolitem action='Teletext'/>"
"  <toolitem action='RestoreVideo'/>"
"  <toolitem action='Subtitles'/>"
" </toolbar>"  
"</ui>";

static const char *
popup_menu_description =
"<ui>"
" <menubar name='Popup'>"
"  <menu action='PopupSubmenu'>"
"   <menuitem action='Fullscreen'/>"
"   <menuitem action='Overlay'/>"
"   <menuitem action='Capture'/>"
"   <separator/>"
"   <menuitem action='Teletext'/>"
"   <menu action='TeletextSubmenu'>"
"   </menu>"
"   <menu action='SubtitlesSubmenu'>"
"   </menu>"
"   <menu action='BookmarksSubmenu'>"
"   </menu>"
"   <separator/>"
"   <menu action='Appearance'>"
"    <menuitem action='ViewMenu'/>"
"    <menuitem action='ViewToolbar'/>"
"    <menuitem action='KeepWindowOnTop'/>"
"   </menu>"
"   <separator/>"
"  </menu>"  
" </menubar>"
"</ui>";

void
zapping_create_popup		(Zapping *		z,
				 GdkEventButton *	event)
{
  GtkUIManager *ui_manager;
  GError *error;
  GtkWidget *widget;
  GtkWidget *popup_menu;
  GtkWidget *menu;

  ui_manager = gtk_ui_manager_new ();
  gtk_ui_manager_insert_action_group (ui_manager,
				      z->generic_action_group, APPEND);
  gtk_ui_manager_insert_action_group (ui_manager,
				      z->vbi_action_group, APPEND);

  error = NULL;
  if (!gtk_ui_manager_add_ui_from_string (ui_manager, popup_menu_description,
					  NUL_TERMINATED, &error))
    {
      g_message ("Cannot build popup menu:\n%s", error->message);
      g_error_free (error);
      exit (EXIT_FAILURE);
    }

  widget = gtk_ui_manager_get_widget (ui_manager, "/Popup/PopupSubmenu");
  popup_menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));

  widget = gtk_separator_menu_item_new ();
  gtk_widget_show (widget);
  gtk_menu_shell_prepend (GTK_MENU_SHELL (popup_menu), widget);

  add_channel_entries (GTK_MENU_SHELL (popup_menu), 0, 10, z->info);

#ifdef HAVE_LIBZVBI
  if (zvbi_get_object ())
    {
      if (CAPTURE_MODE_TELETEXT == z->info->capture_mode)
	{
	  widget = gtk_ui_manager_get_widget
	    (ui_manager, "/Popup/PopupSubmenu/TeletextSubmenu");

	  if (widget)
	    {
	      menu = NULL;
	      if (_ttxview_popup)
                menu = _ttxview_popup (GTK_WIDGET (zapping), event);  
	      if (menu)
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (widget), menu);
	      else
		gtk_widget_hide (widget);
	    }

	  widget = gtk_ui_manager_get_widget
	    (ui_manager, "/Popup/PopupSubmenu/BookmarksSubmenu");
	  if (widget)
	    {
	      if (_ttxview_bookmarks_menu_new)
		{
		  menu = _ttxview_bookmarks_menu_new (GTK_WIDGET (z));
		  gtk_menu_item_set_submenu (GTK_MENU_ITEM (widget), menu);
		}
	      else
		{
		  gtk_widget_hide (widget);
		}
	    }
	}
      else
	{
	  widget = gtk_ui_manager_get_widget
	    (ui_manager, "/Popup/PopupSubmenu/TeletextSubmenu");
	  if (widget)
	    gtk_widget_hide (widget);

	  widget = gtk_ui_manager_get_widget
	    (ui_manager, "/Popup/PopupSubmenu/BookmarksSubmenu");
	  if (widget)
	    gtk_widget_hide (widget);
	}

      widget = gtk_ui_manager_get_widget
	(ui_manager, "/Popup/PopupSubmenu/SubtitlesSubmenu");
      if (widget)
	{
	  if ((menu = subtitles_menu_new ()))
	    gtk_menu_item_set_submenu (GTK_MENU_ITEM (widget), menu);
	  else
	    gtk_widget_hide (widget);
	}

      widget = gtk_ui_manager_get_widget
	(ui_manager, "/Popup/PopupSubmenu/BookmarksSubmenu");
      if (widget)
	{
	  gint index;

	  index = g_list_index (GTK_MENU_SHELL (popup_menu)->children, widget);
	  if (index >= 0)
	    {
	      if (_ttxview_hotlist_menu_insert)
		_ttxview_hotlist_menu_insert (GTK_MENU_SHELL (popup_menu),
					      /* separator */ FALSE,
					      index + 1);
	    }
	}
    }
  else
#endif
    {
      /* separator */
      /* teletext */
      /* subtitles */
      /* bookmarks */
    }

  widget = gtk_ui_manager_get_widget (ui_manager,
				      "/Popup/PopupSubmenu/Appearance");
  menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));

  picture_sizes_append_menu (GTK_MENU_SHELL (menu));

  {
    GList *glist;

    /* Let plugins add their GUI to this context menu */
    for (glist = plugin_list; glist; glist = glist->next)
      plugin_process_popup_menu (GTK_WIDGET (z), event,
				 GTK_MENU (popup_menu),
				 (struct plugin_info *) glist->data);
  }

  gtk_menu_popup (GTK_MENU (popup_menu),
		  /* parent_menu_shell */ NULL,
		  /* parent_menu_item */ NULL,
		  /* menu position func */ NULL,
		  /* menu position data */ NULL,
		  event->button,
		  event->time);
}

void
zapping_rebuild_channel_menu	(Zapping *		z)
{
  if (z->info && z->channels_menu)
    {
      GtkMenuShell *menu;
      GtkWidget *menu_item;
      GtkWidget *widget;

      widget = gtk_menu_new ();
      gtk_widget_show (widget);
      menu = GTK_MENU_SHELL (widget);

      menu_item = gtk_tearoff_menu_item_new();
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (menu, menu_item);

      gtk_menu_item_remove_submenu (z->channels_menu);
      gtk_menu_item_set_submenu (z->channels_menu, widget);

      add_channel_entries (menu, 1, 16, z->info);
    }
}

void
zapping_enable_appbar		(Zapping *		z,
				 gboolean		show)
{
  if (show)
    {
      if (!z->appbar)
	{
	  GtkWidget *appbar;

	  /* Adding a hidden appbar is impossible, we have to add when
	     it becomes first visible. */

	  appbar = gnome_appbar_new (/* progress */ FALSE,
				     /* status */ TRUE,
				     /* interactive */
				     GNOME_PREFERENCES_NEVER);

	  z->appbar = GNOME_APPBAR (appbar);

	  gnome_app_set_statusbar (&z->app, appbar);
	}
      else
	{
	  gtk_widget_show (GTK_WIDGET (z->appbar));
	}
    }
  else if (z->appbar)
    {
      gtk_widget_hide (GTK_WIDGET (z->appbar));
    }

  gtk_widget_queue_resize (GTK_WIDGET (z));
}

static gboolean
scroll_event			(GtkWidget *		widget,
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
    
static gboolean
button_press_event		(GtkWidget *		widget,
				 GdkEventButton *	event)
{
  Zapping *z = ZAPPING (widget);

  switch (event->button)
    {
    case 2: /* Middle button */
      if (CAPTURE_MODE_TELETEXT == z->info->capture_mode)
	break;

      python_command (widget, "zapping.switch_mode('fullscreen')");

      return TRUE; /* handled */

    case 3: /* Right button */
      zapping_create_popup (z, event);
      return TRUE; /* handled */

    default:
      break;
    }

  return FALSE; /* pass on */
}

static gboolean
delete_event			(GtkWidget *		widget,
				 GdkEventAny *		event _unused_)
{
  python_command (widget, "zapping.quit()");
  return TRUE; /* handled */
}

static void
realize				(GtkWidget *		widget)
{
  Zapping *z = ZAPPING (widget);
  GtkAction *action;

  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  action = gtk_action_group_get_action (z->generic_action_group,
					"KeepWindowOnTop");
  if (have_wm_hints)
    {
      GtkToggleAction *toggle_action;

      toggle_action = GTK_TOGGLE_ACTION (action);
      z_toggle_action_connect_gconf_key (toggle_action,
					 "/apps/zapping/window/keep_on_top");
      /* Window is realized now, set the initial state. */
      keep_window_on_top_action (toggle_action, z);
    }
  else
    {
      z_action_set_sensitive (action, FALSE);
    }
}

static void
instance_finalize		(GObject *		object)
{
  extern void shutdown_zapping(void); /* main.c */
  /* Zapping *z = ZAPPING (object); */

  /* preliminary */
  shutdown_zapping ();

  parent_class->finalize (object);
}

static void
instance_init			(GTypeInstance *	instance,
				 gpointer		g_class _unused_)
{
  Zapping *z = (Zapping *) instance;
  GError *error;
  GtkAction *action;
  GtkToggleAction *toggle_action;
  GtkWidget *box;
  GtkWidget *widget;

  z->generic_action_group = gtk_action_group_new ("ZappingGenericActions");
#ifdef ENABLE_NLS
  gtk_action_group_set_translation_domain (z->generic_action_group,
					   GETTEXT_PACKAGE);
#endif					   
  gtk_action_group_add_actions (z->generic_action_group,
				generic_actions,
				G_N_ELEMENTS (generic_actions),
				/* user_data */ z);
  gtk_action_group_add_toggle_actions (z->generic_action_group,
				       generic_toggle_actions,
				       G_N_ELEMENTS (generic_toggle_actions),
				       /* user_data */ z);

  /* We add the submenu ourselves. Make sure the menu item is
     visible despite initially without menu. */
  action = gtk_action_group_get_action (z->generic_action_group,
					"ChannelsSubmenu");
  g_object_set (G_OBJECT (action), "hide-if-empty", FALSE, NULL);

  z->vbi_action_group = gtk_action_group_new ("ZappingVBIActions");
#ifdef ENABLE_NLS
  gtk_action_group_set_translation_domain (z->vbi_action_group,
					   GETTEXT_PACKAGE);
#endif					   
  gtk_action_group_add_actions (z->vbi_action_group,
				vbi_actions, G_N_ELEMENTS (vbi_actions),
				/* user_data */ z);
  gtk_action_group_add_toggle_actions (z->vbi_action_group,
				       vbi_toggle_actions,
				       G_N_ELEMENTS (vbi_toggle_actions),
				       /* user_data */ z);

  /* Mutual exclusive with "Teletext". */
  action = gtk_action_group_get_action (z->vbi_action_group, "RestoreVideo");
  z_action_set_visible (action, FALSE);

  action = gtk_action_group_get_action (z->vbi_action_group, "Subtitles");
  zconf_add_hook ("/zapping/internal/callbacks/closed_caption",
		  (ZConfHook) zconf_hook_subtitles,
		  GTK_TOGGLE_ACTION (action));

  /* We add the submenu ourselves. Make sure the menu item is
     visible despite initially without menu. */
  action = gtk_action_group_get_action (z->vbi_action_group,
					"TeletextSubmenu");
  g_object_set (G_OBJECT (action), "hide-if-empty", FALSE, NULL);
  action = gtk_action_group_get_action (z->vbi_action_group,
					"SubtitlesSubmenu");
  g_object_set (G_OBJECT (action), "hide-if-empty", FALSE, NULL);
  action = gtk_action_group_get_action (z->vbi_action_group,
					"BookmarksSubmenu");
  g_object_set (G_OBJECT (action), "hide-if-empty", FALSE, NULL);

  gnome_app_construct (&z->app, "Zapping", "Zapping");

  z->ui_manager = gtk_ui_manager_new ();
  gtk_ui_manager_insert_action_group (z->ui_manager,
				      z->generic_action_group, APPEND);
  gtk_ui_manager_insert_action_group (z->ui_manager,
				      z->vbi_action_group, APPEND);

  error = NULL;
  if (!gtk_ui_manager_add_ui_from_string (z->ui_manager, ui_description,
					  NUL_TERMINATED, &error))
    {
      g_message ("Cannot build main menu:\n%s", error->message);
      g_error_free (error);
      exit (EXIT_FAILURE);
    }

  widget = gtk_ui_manager_get_widget (z->ui_manager, "/MainMenu");
  z->menubar = GTK_MENU_BAR (widget);
  widget = gtk_ui_manager_get_widget (z->ui_manager,
				      "/MainMenu/ChannelsSubmenu");
  z->channels_menu = GTK_MENU_ITEM (widget);
  zapping_rebuild_channel_menu (z);

  toggle_action = GTK_TOGGLE_ACTION (gtk_action_group_get_action
				     (z->generic_action_group,
				      "ViewMenu"));
  z_toggle_action_connect_gconf_key (toggle_action,
				     "/apps/zapping/window/view_menu");
  /* Adds the menu if necessary. */
  view_menu_action (toggle_action, z);

  widget = gtk_ui_manager_get_widget (z->ui_manager, "/Toolbar");
  z->toolbar = GTK_TOOLBAR (widget);

  toggle_action = GTK_TOGGLE_ACTION (gtk_action_group_get_action
				     (z->generic_action_group,
				      "ViewToolbar"));
  z_toggle_action_connect_gconf_key (toggle_action,
				     "/apps/zapping/window/view_toolbar");
  /* Adds the toolbar if necessary. */
  view_toolbar_action (toggle_action, z);

  /* Will add appbar on demand, see zapping_enable_appbar(). */

  box = gtk_hbox_new (FALSE, 0);
  z->contents = GTK_BOX (box);
  gtk_widget_show (box);
  gnome_app_set_contents (&z->app, box);

  widget = z_video_new ();
  z->video = Z_VIDEO (widget);
  gtk_widget_show (widget);
  gtk_container_add (GTK_CONTAINER (box), widget);

  {
    GdkColor black = { 0, 0, 0, 0 };
    gtk_widget_modify_bg (widget, GTK_STATE_NORMAL, &black);
  }

  z_video_set_min_size (z->video, 64, 64 * 3 / 4);

  /* XXX free, 4:3, 16:9 */
  if (zconf_get_boolean (NULL, "/zapping/options/main/fixed_increments"))
    z_video_set_size_inc (z->video, 64, 64 * 3 / 4);

  gtk_widget_add_events (widget,
			 GDK_BUTTON_PRESS_MASK |
			 GDK_BUTTON_RELEASE_MASK |
			 GDK_SCROLL_MASK |
			 GDK_EXPOSURE_MASK |
			 GDK_POINTER_MOTION_MASK |
			 GDK_VISIBILITY_NOTIFY_MASK |
			 GDK_KEY_PRESS_MASK);
}

GtkWidget *
zapping_new			(void)
{
  return GTK_WIDGET (g_object_new (TYPE_ZAPPING, NULL));
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

  widget_class->realize	= realize;
  widget_class->delete_event = delete_event;
  widget_class->button_press_event = button_press_event;
  widget_class->scroll_event = scroll_event;

  cmd_register ("hide_controls", py_hide_controls, METH_VARARGS,
		N_("Show menu and toolbar"), "zapping.hide_controls()");
  cmd_register ("keep_on_top", py_keep_on_top, METH_VARARGS,
		N_("Keep window on top"), "zapping.keep_on_top()");
}

GType
zapping_get_type		(void)
{
  static GType type = 0;
  
  if (!type)
    {
      GTypeInfo info;

      CLEAR (info);

      info.class_size = sizeof (ZappingClass);
      info.class_init = class_init;
      info.instance_size = sizeof (Zapping);
      info.instance_init = instance_init;

      type = g_type_register_static (GNOME_TYPE_APP,
				     "Zapping",
				     &info, (GTypeFlags) 0);
    }

  return type;
}
