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

/* $Id: interface.c,v 1.24.2.15 2003-11-28 18:35:49 mschimek Exp $ */

/* XXX gtk+ 2.3 toolbar changes */
#undef GTK_DISABLE_DEPRECATED

#include "config.h"

#include <gnome.h>
#include <glade/glade.h>

#include "interface.h"
#include "remote.h"
#include "zmisc.h"
#include "globals.h"
#include "zconf.h"
#include "properties-handler.h"
#include "zvideo.h"
#include "v4linterface.h"
#include "plugins.h"
#include "ttxview.h"
#include "zvbi.h"

extern gboolean have_wm_hints;

/* Finds the named widget in the tree parent belongs to.
   Returns a pointer to it or NULL if not found. */
GtkWidget *
find_widget			(GtkWidget *		parent,
				 const gchar *		name)
{
  GtkWidget *widget;
  GtkWidget *result;
  GladeXML *tree;
  gchar *buf;

  widget = parent;
  result = NULL;

  buf = g_strconcat ("registered-widget-", name, NULL);

  while (widget)
    {
      if ((result = (GtkWidget*) g_object_get_data (G_OBJECT (widget), buf)))
	break; /* found registered widget with that name */

      if ((tree = glade_get_widget_tree (widget)))
	if ((result = glade_xml_get_widget (tree, name)))
	  break; /* found glade widget with that name */

      if (0)
	fprintf (stderr, "found '%s'\n",
		 glade_get_widget_name (widget));

      if (GTK_IS_MENU (widget))
	widget = gtk_menu_get_attach_widget (GTK_MENU (widget));
      else
	widget = widget->parent;
    }

  g_free (buf);

  return result;
}

/* Tries to find a widget that is accesible though parent, named
   name. IMHO this should be called glade_lookup_widget and go into
   libglade, but anyway... If the widget isn't found, a message is
   printed and the program quits, it always returns a valid widget. */
GtkWidget *
lookup_widget			(GtkWidget *		parent,
				 const gchar *		name)
{
  GtkWidget *widget = find_widget (parent, name);

  if (!widget)
    {
      RunBox ("Widget %s not found, please contact the maintainer",
	      GTK_MESSAGE_ERROR, name);
      exit (EXIT_FAILURE);
    }

  return widget;
}

/* Register a widget to be found with find_widget() or lookup_widget().
   The information is attached to the toplevel ancestor of parent,
   or when NULL of the widget. */
void
register_widget			(GtkWidget *		parent,
				 GtkWidget *		widget,
				 const char *		name)
{
  if (!parent)
    parent = widget;

  for (;;)
    {
      if (!GTK_IS_MENU (parent) && !parent->parent)
	{
	  gchar * buf;

	  buf = g_strconcat ("registered-widget-", name, NULL);
	  g_object_set_data (G_OBJECT (parent), buf, widget);
	  g_free (buf);
	  return;
	}

      if (GTK_IS_MENU (parent))
	parent = gtk_menu_get_attach_widget (GTK_MENU (parent));
      else
	parent = parent->parent;

      if (!parent)
	return; /* toplevel not found */
    }
}

/**
 * build_widget:
 * @name: Name of the widget.
 * @file: Name of the Glade file. 
 * 
 * Loads a GtkWidget from a Glade file, when @file is %NULL from
 * zapping.glade2. All the memory is freed when the object (widget)
 * is destroyed. If name is %NULL, all widgets are loaded, but this
 * is not recommended.
 * 
 * Return value: 
 * Widget pointer, cannot fail.
 **/
GtkWidget *
build_widget			(const gchar *		name,
				 const gchar *		file)
{
  GladeXML *xml;
  GtkWidget *widget;
  gchar *path;

  if (!file)
    file = "zapping.glade2";

  path = g_strconcat (PACKAGE_DATA_DIR "/" PACKAGE "/", file, NULL);
  xml = glade_xml_new (path, name, NULL);

  if (!xml)
    {
      RunBox ("File %s [%s] not found, please contact the maintainer",
	      GTK_MESSAGE_ERROR, path, name);
      exit (EXIT_FAILURE);
    }

  g_free (path);

  widget = glade_xml_get_widget (xml, name);

  if (!widget)
    {
      RunBox ("Widget %s not found in %s, please contact the maintainer",
	      GTK_MESSAGE_ERROR, name, path);
      exit (EXIT_FAILURE);
    }

  glade_xml_signal_autoconnect (xml);

  return widget;
}

/* Menu callbacks */

static void
on_python_check_menu_toggled	(GtkCheckMenuItem *	check_menu_item,
				 gpointer		user_data)
{
  python_command_printf (GTK_WIDGET (check_menu_item),
			 "zapping.%s(%u)", (const char *) user_data,
			 check_menu_item->active);
}

static void
on_python_check_menu_toggled_reverse
				(GtkCheckMenuItem *	check_menu_item,
				 gpointer		user_data)
{
  python_command_printf (GTK_WIDGET (check_menu_item),
			 "zapping.%s(%u)", (const char *) user_data,
			 !check_menu_item->active);
}

static void
zconf_hook_show_toolbar		(const gchar *		key,
				 gpointer		new_value_ptr,
				 GtkCheckMenuItem *	item)
{
  gboolean hide = * (gboolean *) new_value_ptr;
  GtkWidget *menu;
  GtkWidget *toolbar;

  if (hide == item->active)
    gtk_check_menu_item_set_active (item, !hide);

  menu = GTK_WIDGET (gnome_app_get_dock_item_by_name
		     (GNOME_APP (main_window), GNOME_APP_MENUBAR_NAME));
  toolbar = GTK_WIDGET (gnome_app_get_dock_item_by_name
			(GNOME_APP (main_window), GNOME_APP_TOOLBAR_NAME));
  if (hide)
    {
      gtk_widget_hide (menu);
      gtk_widget_hide (toolbar);
    }
  else
    {
      gtk_widget_show (menu);
      gtk_widget_show (toolbar);
    }

  gtk_widget_queue_resize (main_window);
}

static void
zconf_hook_keep_window_on_top	(const gchar *		key,
				 gpointer		new_value_ptr,
				 GtkCheckMenuItem *	item)
{
  gboolean keep = * (gboolean *) new_value_ptr;

  keep &= have_wm_hints;

  if (keep != item->active)
    gtk_check_menu_item_set_active (item, keep);
}

static void
zconf_hook_toolbar_style	(const gchar *		key,
				 gpointer		new_value_ptr,
				 gpointer		data)
{
#if 0
  gint style = * (gint *) new_value_ptr;
  GtkToolbar *toolbar = data;

  z_toolbar_set_style_recursive (toolbar, (GtkToolbarStyle) style);
#endif
}

static void
zconf_hook_toolbar_tooltips	(const gchar *		key,
				 gpointer		new_value_ptr,
				 gpointer		data)
{
  gboolean enable = * (gboolean *) new_value_ptr;
  GtkToolbar *toolbar = data;

  gtk_toolbar_set_tooltips (toolbar, enable);
}

static GnomeUIInfo
popup_appearance_uiinfo [] = {
  {
    GNOME_APP_UI_TOGGLEITEM, N_("Show Menu and Toolbar"), NULL,
    G_CALLBACK (on_python_check_menu_toggled_reverse), "hide_controls", NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    GDK_H, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_TOGGLEITEM, N_("Keep Window on Top"), NULL,
    G_CALLBACK (on_python_check_menu_toggled), "keep_on_top", NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    GDK_K, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo
popup_uiinfo [] = {
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("_Fullscreen"), NULL,
    G_CALLBACK (on_python_command1), "zapping.switch_mode('fullscreen')", NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_EXECUTE,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Overlay mode"), NULL,
    G_CALLBACK (on_python_command1), "zapping.switch_mode('preview')", NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_EXECUTE,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Capture mode"), NULL,
    G_CALLBACK (on_python_command1), "zapping.switch_mode('capture')", NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_EXECUTE,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("_Teletext"), NULL,
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, "zapping-teletext",
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Subtitles"), NULL,
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, "zapping-subtitle",
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Bookmarks"), NULL,
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

static GtkWidget *
zapping_popup_menu_new		(GdkEventButton *	event)
{
  GtkWidget *menu;
  GtkWidget *widget;

  menu = gtk_menu_new ();

  gnome_app_fill_menu (GTK_MENU_SHELL (menu), popup_uiinfo,
		       /* accel */ NULL,
		       /* mnemo */ FALSE,
		       /* position */ 0);

  add_channel_entries (GTK_MENU_SHELL (menu), 0, 10, main_info);

  if (disable_preview)
    {
      widget = popup_uiinfo[1].widget; /* fullscreen */
      gtk_widget_set_sensitive (widget, FALSE);

      widget = popup_uiinfo[2].widget; /* overlay mode */
      gtk_widget_set_sensitive (widget, FALSE);
    }

#ifdef HAVE_LIBZVBI
  if (zvbi_get_object ())
    {
      GtkWidget *teletext;

      teletext = popup_uiinfo[5].widget; /* teletext */

      if (TVENG_TELETEXT == main_info->current_mode)
	{
	  GtkWidget *ttxview_menu;

	  if (event)
	    ttxview_menu = ttxview_popup (main_window, event);

	  if (event && ttxview_menu)
	    {
	      gtk_menu_item_set_submenu (GTK_MENU_ITEM (teletext),
					 ttxview_menu);
	    }
	  else
	    {
	      gtk_widget_set_sensitive (teletext, FALSE);
	      gtk_widget_hide (teletext);
	    }

	  widget = popup_uiinfo[7].widget; /* bookmarks */
	  gtk_menu_item_set_submenu (GTK_MENU_ITEM (widget),
				     ttxview_bookmarks_menu_new (main_window));
	}
      else
	{
	  g_signal_connect (G_OBJECT (teletext), "activate",
			    G_CALLBACK (on_python_command1),
			    "zapping.switch_mode('teletext')");

	  widget = popup_uiinfo[7].widget; /* bookmarks */
	  gtk_widget_set_sensitive (widget, FALSE);
	  gtk_widget_hide (widget);
	}

      {
	GtkWidget *subtitles_menu;

	widget = popup_uiinfo[6].widget; /* subtitles */

	if ((subtitles_menu = ttxview_subtitles_menu_new ()))
	  {
	    gtk_menu_item_set_submenu (GTK_MENU_ITEM (widget),
				       subtitles_menu);
	  }
	else
	  {
	    gtk_widget_set_sensitive (widget, FALSE);
	    gtk_widget_hide (widget);
	  }
      }

      ttxview_hotlist_menu_append (GTK_MENU_SHELL (menu),
				   /* separator */ FALSE);
    }
  else
#endif
    {
      widget = popup_uiinfo[4].widget; /* separator */
      gtk_widget_set_sensitive (widget, FALSE);
      gtk_widget_hide (widget);

      widget = popup_uiinfo[5].widget; /* teletext */
      gtk_widget_set_sensitive (widget, FALSE);
      gtk_widget_hide (widget);

      widget = popup_uiinfo[6].widget; /* subtitles */
      gtk_widget_set_sensitive (widget, FALSE);
      gtk_widget_hide (widget);

      widget = popup_uiinfo[7].widget; /* bookmarks */
      gtk_widget_set_sensitive (widget, FALSE);
      gtk_widget_hide (widget);
    }

  {
    GtkWidget *app_menu;
    const gchar *key;
    gboolean active;

    widget = gtk_separator_menu_item_new ();
    gtk_widget_show (widget);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), widget);

    widget = z_gtk_pixmap_menu_item_new (_("Appearance"), GTK_STOCK_CONVERT);
    gtk_widget_show (widget);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), widget);

    app_menu = gtk_menu_new ();

    gnome_app_fill_menu (GTK_MENU_SHELL (app_menu), popup_appearance_uiinfo,
			 /* accel */ NULL,
			 /* mnemo */ FALSE,
			 /* position */ 0);

    gtk_menu_item_set_submenu (GTK_MENU_ITEM (widget), app_menu);

    widget = popup_appearance_uiinfo[0].widget;
    key = "/zapping/internal/callbacks/hide_controls";
    active = zconf_get_boolean (NULL, key);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), !active);
    zconf_add_hook_while_alive (G_OBJECT (widget), key,
				(ZConfHook) zconf_hook_show_toolbar,
				GTK_CHECK_MENU_ITEM (widget), FALSE);

    widget = popup_appearance_uiinfo[1].widget;
    /* XXX tell why not */
    gtk_widget_set_sensitive (widget, !!have_wm_hints);
    key = "/zapping/options/main/keep_on_top";
    active = zconf_get_boolean (NULL, key);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (widget), active);
    zconf_add_hook_while_alive (G_OBJECT (widget), key,
				(ZConfHook) zconf_hook_keep_window_on_top,
				GTK_CHECK_MENU_ITEM (widget), FALSE);

    picture_sizes_append_menu (GTK_MENU_SHELL (app_menu));
  }

  {
    GList *glist;

    /* Let plugins add their GUI to this context menu */
    for (glist = plugin_list; glist; glist = glist->next)
      plugin_process_popup_menu (main_window, event,
				 GTK_MENU (menu),
				 (struct plugin_info *) glist->data);
  }

  return menu;
}

static GnomeUIInfo
main_file_uiinfo [] = {
  GNOMEUIINFO_MENU_QUIT_ITEM (on_python_command1, "zapping.quit()"),
  GNOMEUIINFO_END
};

static GnomeUIInfo
main_edit_uiinfo [] = {
  GNOMEUIINFO_MENU_PREFERENCES_ITEM
    (on_python_command1, "zapping.properties()"),
  {
    GNOME_APP_UI_ITEM, N_("P_lugins"), NULL,
    G_CALLBACK (on_python_command1), "zapping.plugin_properties()", NULL,
    GNOME_APP_PIXMAP_STOCK, "gnome-stock-attach",
    0, (GdkModifierType) 0, NULL
  },
  /* GNOMEUIINFO_SEPARATOR, */
  {
    GNOME_APP_UI_ITEM, N_("_Channels"), NULL,
    G_CALLBACK (on_python_command1), "zapping.channel_editor()", NULL,
    GNOME_APP_PIXMAP_STOCK, "gtk-index",
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo
main_view_uiinfo [] = {
  {
    GNOME_APP_UI_ITEM, N_("_Fullscreen"), NULL,
    G_CALLBACK (on_python_command1), "zapping.switch_mode('fullscreen')", NULL,
    GNOME_APP_PIXMAP_STOCK, "gtk-execute",
    GDK_F, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Overlay mode"), NULL,
    G_CALLBACK (on_python_command1), "zapping.switch_mode('preview')", NULL,
    GNOME_APP_PIXMAP_STOCK, "gtk-execute",
    GDK_O, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Capture mode"), NULL,
    G_CALLBACK (on_python_command1), "zapping.switch_mode('capture')", NULL,
    GNOME_APP_PIXMAP_STOCK, "gtk-execute",
    GDK_C, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("_Teletext"), NULL,
    G_CALLBACK (on_python_command1), "zapping.switch_mode('teletext')", NULL,
    GNOME_APP_PIXMAP_STOCK, "zapping-teletext",
    GDK_T, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_New Teletext View"), NULL,
    G_CALLBACK (on_python_command1), "zapping.ttx_open_new()", NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    GDK_N, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_TOGGLEITEM, N_("_Mute"), NULL,
    G_CALLBACK (on_python_check_menu_toggled), "mute", NULL,
    GNOME_APP_PIXMAP_STOCK, "zapping-mute",
    GDK_A, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_TOGGLEITEM, N_("_Subtitles"), NULL,
    G_CALLBACK (on_python_check_menu_toggled), "closed_caption", NULL,
    GNOME_APP_PIXMAP_STOCK, "zapping-subtitle",
    GDK_U, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_TOGGLEITEM, N_("Show menu and toolbar"), NULL,
    G_CALLBACK (on_python_check_menu_toggled_reverse), "hide_controls", NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    GDK_H, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_TOGGLEITEM, N_("Keep window on top"), NULL,
    G_CALLBACK (on_python_check_menu_toggled), "keep_on_top", NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    GDK_K, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo
main_help_uiinfo [] = {
  GNOMEUIINFO_HELP ("Zapping"),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_ABOUT_ITEM (on_python_command1, "zapping.about()"),
  GNOMEUIINFO_END
};

static GnomeUIInfo
main_uiinfo [] = {
  GNOMEUIINFO_MENU_FILE_TREE (main_file_uiinfo),
  GNOMEUIINFO_MENU_EDIT_TREE (main_edit_uiinfo),
  GNOMEUIINFO_MENU_VIEW_TREE (main_view_uiinfo),
  {
    GNOME_APP_UI_ITEM, N_("_Channels"), NULL,
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_MENU_HELP_TREE (main_help_uiinfo),
  GNOMEUIINFO_END
};

static GtkWidget *
zapping_menu_new		(GtkWidget *		zapping)
{
  GtkWidget *menu;
  GtkWidget *widget;

  menu = gtk_menu_bar_new ();

  gnome_app_ui_configure_configurable (main_uiinfo);

  gnome_app_fill_menu (GTK_MENU_SHELL (menu), main_uiinfo,
		       GNOME_APP (zapping)->accel_group,
		       /* mnemo */ TRUE,
		       /* position */ 0);

  register_widget (zapping, main_uiinfo[3].widget, "channels");

  register_widget (zapping, main_edit_uiinfo[0].widget, "propiedades1");
  register_widget (zapping, main_edit_uiinfo[1].widget, "plugins1");

  register_widget (zapping, main_view_uiinfo[0].widget, "go_fullscreen1");
  register_widget (zapping, main_view_uiinfo[1].widget, "go_previewing2");
  register_widget (zapping, main_view_uiinfo[4].widget, "videotext1");
  register_widget (zapping, main_view_uiinfo[5].widget, "new_ttxview");
  register_widget (zapping, main_view_uiinfo[6].widget, "separador5");
  register_widget (zapping, main_view_uiinfo[7].widget, "mute2");

  widget = main_view_uiinfo[8].widget; /* subtitles */
  register_widget (zapping, widget, "menu-subtitle");
  zconf_add_hook_while_alive (G_OBJECT (widget),
			      "/zapping/internal/callbacks/closed_caption",
			      (ZConfHook) zconf_hook_check_menu,
			      GTK_CHECK_MENU_ITEM (widget), TRUE);

  widget = main_view_uiinfo[10].widget; /* hide menu and toolbar */
  zconf_add_hook_while_alive (G_OBJECT (widget),
			      "/zapping/internal/callbacks/hide_controls",
			      (ZConfHook) zconf_hook_show_toolbar,
			      GTK_CHECK_MENU_ITEM (widget), FALSE);

  widget = main_view_uiinfo[11].widget; /* keep window on top */
  /* XXX tell why not */
  gtk_widget_set_sensitive (widget, !!have_wm_hints);
  zconf_add_hook_while_alive (G_OBJECT (widget),
			      "/zapping/options/main/keep_on_top",
			      (ZConfHook) zconf_hook_keep_window_on_top,
			      GTK_CHECK_MENU_ITEM (widget), FALSE);

  return menu;
}

static void
on_python_toggle_button_toggled	(GtkToggleButton *	toggle_button,
				 gpointer		user_data)
{
  python_command_printf (GTK_WIDGET (toggle_button),
			 "zapping.%s(%u)", (const char *) user_data,
			 gtk_toggle_button_get_active (toggle_button));
}

static GtkWidget *
zapping_toolbar_new		(GtkWidget *		zapping)
{
  GtkToolbar *toolbar;
  GtkIconSize icon_size;
  GtkWidget *widget;
  GtkWidget *icon;

  widget = gtk_toolbar_new ();
  toolbar = GTK_TOOLBAR (widget);

  register_widget (zapping, widget, "toolbar1");

  zconf_add_hook_while_alive (G_OBJECT (widget),
			      "/zapping/options/main/show_tooltips",
				zconf_hook_toolbar_tooltips,
				toolbar, /* call now */ TRUE);

  icon_size = gtk_toolbar_get_icon_size (toolbar);

  icon = gtk_image_new_from_stock (GTK_STOCK_GO_UP, icon_size);
  widget = gtk_toolbar_append_item
    (toolbar, _("Ch. Up"),  _("Switch to higher channel"),
     NULL, icon, G_CALLBACK (on_python_command1), "zapping.channel_up()");

  icon = gtk_image_new_from_stock (GTK_STOCK_GO_DOWN, icon_size);
  widget = gtk_toolbar_append_item
    (toolbar, _("Ch. Down"),  _("Switch to lower channel"),
     NULL, icon, G_CALLBACK (on_python_command1), "zapping.channel_down()");

  gtk_toolbar_append_space (toolbar);

  icon = gtk_image_new_from_stock (GTK_STOCK_EXECUTE, icon_size);
  widget = gtk_toolbar_append_item
    (toolbar, _("Controls"),  _("Change picture controls"),
     NULL, icon, G_CALLBACK (on_python_command1), "zapping.control_box()");
  register_widget (zapping, widget, "toolbar-controls");

  icon = gtk_image_new_from_stock ("zapping-mute", icon_size);
  widget = gtk_toolbar_append_element
    (toolbar, GTK_TOOLBAR_CHILD_TOGGLEBUTTON, NULL,
     _("Mute"), _("Switch audio on or off"), NULL, icon,
     G_CALLBACK (on_python_toggle_button_toggled), "mute");
  register_widget (zapping, widget, "toolbar-mute");

  widget = gtk_toolbar_insert_stock (toolbar, "zapping-teletext",
				     _("Activate Teletext mode"), NULL,
				     G_CALLBACK (on_python_command1),
				     "zapping.switch_mode('teletext')", -1);
  register_widget (zapping, widget, "toolbar-teletext");

  icon = gtk_image_new_from_stock ("zapping-subtitle", icon_size);
  widget = gtk_toolbar_append_element
    (toolbar, GTK_TOOLBAR_CHILD_TOGGLEBUTTON, NULL,
     _("Subtitles"), _("Switch subtitles on or off"), NULL, icon,
     G_CALLBACK (on_python_toggle_button_toggled), "closed_caption");
  zconf_add_hook_while_alive (G_OBJECT (widget),
			      "/zapping/internal/callbacks/closed_caption",
			      (ZConfHook) zconf_hook_toggle_button,
			      GTK_TOGGLE_BUTTON (widget), TRUE);
  register_widget (zapping, widget, "toolbar-subtitle");

  gtk_container_foreach (GTK_CONTAINER (widget),
			 (GtkCallback) gtk_widget_show_all,
                         NULL);

  return GTK_WIDGET (toolbar);
}

static void
on_zapping_delete_event		(GtkWidget *		widget,
				 gpointer		user_data)
{
  python_command (widget, "zapping.quit()");
}

static gboolean
on_tv_screen_button_press_event	(GtkWidget *		widget,
				 GdkEventButton *	event,
				 gpointer		user_data)
{
  switch (event->button)
    {
    case 2: /* Middle button */
      if (main_info->current_mode == TVENG_TELETEXT)
	break;

      python_command (widget, "zapping.switch_mode('fullscreen')");
      return TRUE; /* handled */

    case 3: /* Right button */
      {
	GtkWidget *menu;

	menu = zapping_popup_menu_new (event);

	gtk_menu_popup (GTK_MENU (menu),
			/* parent_menu_shell */ NULL,
			/* parent_menu_item */ NULL,
			/* menu position func */ NULL,
			/* menu position data */ NULL,
			event->button,
			event->time);

	return TRUE; /* handled */
      }

    case 4: /* Another button (XXX wheel?) */
      python_command (widget, "zapping.channel_up()");
      return TRUE; /* handled */

    case 5: /* Yet another button */
      python_command (widget, "zapping.channel_down()");
      return TRUE; /* handled */

    default:
      break;
    }

  return FALSE; /* pass on */
}

GtkWidget *
create_zapping			(void)
{
  GdkColor black = { 0, 0, 0, 0 };
  GnomeApp *gnome_app;
  GtkWidget *zapping;
  GtkWidget *menubar;
  GtkWidget *toolbar;
  GtkWidget *box;
  GtkWidget *appbar;
  GtkWidget *tv_screen;

  zapping = gnome_app_new ("Zapping", "Zapping");
  gnome_app = GNOME_APP (zapping);

  menubar = zapping_menu_new (zapping);
  gtk_widget_show (menubar);
  gnome_app_set_menus (gnome_app, GTK_MENU_BAR (menubar));

  toolbar = zapping_toolbar_new (zapping);
  gtk_widget_show (toolbar);
  gnome_app_set_toolbar (gnome_app, GTK_TOOLBAR (toolbar));

  box = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (box);
  gnome_app_set_contents (gnome_app, box);

  /* gtk_widget_modify_bg (box, GTK_STATE_NORMAL, &black); */

  appbar = gnome_appbar_new (/* progress */ FALSE,
			     /* status */ TRUE,
			     /* interactive */ GNOME_PREFERENCES_NEVER);
  /* Status bar is only used in Teletext mode. */
  gtk_widget_hide (appbar);
  gnome_app_set_statusbar (gnome_app, appbar);
  register_widget (zapping, appbar, "appbar2");

  tv_screen = z_video_new ();
  gtk_widget_show (tv_screen);
  gtk_container_add (GTK_CONTAINER (box), tv_screen);
  register_widget (zapping, tv_screen, "tv-screen");

  gtk_widget_modify_bg (tv_screen, GTK_STATE_NORMAL, &black);

  z_video_set_min_size (Z_VIDEO (tv_screen), 64, 64 * 3 / 4);

  /* XXX free, 4:3, 16:9 */
  if (zconf_get_boolean (NULL, "/zapping/options/main/fixed_increments"))
    z_video_set_size_inc (Z_VIDEO (tv_screen), 64, 64 * 3 / 4);

  gtk_widget_add_events (tv_screen,
			 GDK_BUTTON_PRESS_MASK |
			 GDK_BUTTON_RELEASE_MASK |
			 GDK_EXPOSURE_MASK |
			 GDK_POINTER_MOTION_MASK |
			 GDK_VISIBILITY_NOTIFY_MASK |
			 GDK_KEY_PRESS_MASK);

  g_signal_connect (G_OBJECT (tv_screen), "button_press_event",
		    G_CALLBACK (on_tv_screen_button_press_event), NULL);

  g_signal_connect (G_OBJECT (zapping), "delete_event",
                    G_CALLBACK (on_zapping_delete_event), NULL);

  if (0) {
    GtkWidget *w;

    w = lookup_widget (zapping, "toolbar1");
    zconf_add_hook_while_alive (G_OBJECT (w),
				"/zapping/options/main/toolbar_style",
				zconf_hook_toolbar_style,
				GTK_TOOLBAR (w), /* call now */ TRUE);
  }

  return zapping;
}
