#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <glade/glade.h>

#include "interface.h"
#include "remote.h"
#include "zmisc.h"
#include "globals.h"
#include "zconf.h"
#include "properties-handler.h"
#include "zvideo.h"
#include "callbacks.h"
#include "v4linterface.h"
#include "plugins.h"
#include "ttxview.h"
#include "zvbi.h"

extern gboolean have_wm_hints;

/**
 * Finds in the tree the given widget, returns a pointer to it or NULL
 * if not found
 */
GtkWidget *
find_widget				(GtkWidget *	parent,
					 const gchar *	name)
{
  GtkWidget *widget = parent;
  GtkWidget *result = NULL;
  GladeXML *tree;
  gchar *buf;

  buf = g_strdup_printf("registered-widget-%s", name);

  while (widget)
    {
      if ((result = (GtkWidget*) g_object_get_data (G_OBJECT(widget), buf)))
	break; /* found registered widget with that name */

      if ((tree = glade_get_widget_tree (widget)))
	if ((result = glade_xml_get_widget (tree, name)))
	  break; /* found glade widget with that name */

      if (0)
	fprintf (stderr, "found '%s'\n",
		 glade_get_widget_name(widget));

      /* try to go to the parent widget */
      if (GTK_IS_MENU (widget))
	widget = gtk_menu_get_attach_widget (GTK_MENU (widget));
      else
	widget = widget->parent;
    }

  g_free(buf);

  return result;
}

/*
 * Tries to find a widget, that is accesible though parent, named
 * name. IMHO this should be called glade_lookup_widget and go into
 * libglade, but anyway...
 * If the widget isn't found, a message is printed and the program
 * quits, it always returns a valid widget.
 */
GtkWidget *
lookup_widget				(GtkWidget *	parent,
					 const gchar *	name)
{
  GtkWidget *widget = find_widget(parent, name);

  if (!widget)
    {
      RunBox(_("Widget %s not found, please contact the maintainer"),
	     GTK_MESSAGE_ERROR, name);
      exit(1);
    }

  return widget;
}

void
register_widget(GtkWidget *parent, GtkWidget * widget, const char * name)
{
  if (NULL == parent)
    parent = widget;

  for (;;)
    {
      if (!GTK_IS_MENU(parent) && !parent->parent)
	{
	  gchar *buf;

	  buf = g_strdup_printf("registered-widget-%s", name);
	  g_object_set_data(G_OBJECT(parent), buf, widget);
	  g_free(buf);
	  return;
	}

      /* try to go to the parent widget */
      if (GTK_IS_MENU(parent))
	parent = gtk_menu_get_attach_widget (GTK_MENU (parent));
      else
	parent = parent->parent;

      if (!parent)
	return; /* Toplevel not found */
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
      RunBox ("%s [%s] couldn't be found, please contact the maintainer",
	      GTK_MESSAGE_ERROR, path, name);
      exit (1);
    }

  g_free (path);

  widget = glade_xml_get_widget (xml, name);

  if (!widget)
    {
      RunBox ("%s [%s] couldn't be loaded, please contact the maintainer",
	      GTK_MESSAGE_ERROR, path, name);
      exit (1);
    }

  glade_xml_signal_autoconnect (xml);

  return widget;
}


static void
on_toolbar_toggled		(GtkCheckMenuItem *	check_menu_item,
				 gpointer		user_data)
{
  python_command_printf (GTK_WIDGET (check_menu_item),
			 "zapping.hide_controls(%u)",
			 !check_menu_item->active);
}

static void
zconf_hook_toolbar		(const gchar *		key,
				 gpointer		new_value_ptr,
				 gpointer		data)
{
  gboolean hide = * (gboolean *) new_value_ptr;
  GtkCheckMenuItem *item = data;
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
on_keep_window_on_top_toggled	(GtkCheckMenuItem *	check_menu_item,
				 gpointer		user_data)
{
  python_command_printf (GTK_WIDGET (check_menu_item),
			 "zapping.keep_on_top(%u)",
			 check_menu_item->active);
}

static void
zconf_hook_keep_window_on_top	(const gchar *		key,
				 gpointer		new_value_ptr,
				 gpointer		data)
{
  gboolean keep = * (gboolean *) new_value_ptr;
  GtkCheckMenuItem *item = data;

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


static GnomeUIInfo
main_file_uiinfo [] = {
  GNOMEUIINFO_MENU_QUIT_ITEM (on_python_command1, "zapping.quit()"),
  GNOMEUIINFO_END
};

static GnomeUIInfo
main_edit_uiinfo [] = {
  GNOMEUIINFO_MENU_PREFERENCES_ITEM (on_python_command1, "zapping.properties()"),
  {
    GNOME_APP_UI_ITEM, N_("P_lugins"), NULL,
    G_CALLBACK (on_python_command1), "zapping.plugin_properties()", NULL,
    GNOME_APP_PIXMAP_STOCK, "gnome-stock-attach",
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("_TV Channels"), NULL,
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
    G_CALLBACK (on_python_command1), "zapping.mute()", NULL,
    GNOME_APP_PIXMAP_STOCK, "zapping-mute",
    GDK_A, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_TOGGLEITEM, N_("_Subtitles"), NULL,
    G_CALLBACK (on_python_command1), "zapping.closed_caption()", NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_TOGGLEITEM, N_("Menu and toolbar"), NULL,
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    GDK_H, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_TOGGLEITEM, N_("Keep window on top"), NULL,
    NULL, NULL, NULL,
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
  GNOMEUIINFO_MENU_SETTINGS_TREE (main_edit_uiinfo),
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
  register_widget (zapping, main_view_uiinfo[8].widget, "closed_caption1");

  widget = main_view_uiinfo[10].widget; /* hide menu and toolbar */
  g_signal_connect (G_OBJECT (widget), "toggled",
		    (GCallback) on_toolbar_toggled, NULL);
  zconf_add_hook_while_alive (G_OBJECT (widget),
			      "/zapping/internal/callbacks/hide_controls",
			      zconf_hook_toolbar,
			      GTK_CHECK_MENU_ITEM (widget), FALSE);

  widget = main_view_uiinfo[11].widget; /* keep window on top */
  /* XXX tell why not */
  gtk_widget_set_sensitive (widget, !!have_wm_hints);
  g_signal_connect (G_OBJECT (widget), "toggled",
		    (GCallback) on_keep_window_on_top_toggled, NULL);
  zconf_add_hook_while_alive (G_OBJECT (widget),
			      "/zapping/options/main/keep_on_top",
			      zconf_hook_keep_window_on_top,
			      GTK_CHECK_MENU_ITEM (widget), FALSE);

  return menu;
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
     _("Mute"), _("Switch audio on and off"), NULL,
     icon, G_CALLBACK (on_python_command1), "zapping.mute()");
  register_widget (zapping, widget, "toolbar-mute");

  widget = gtk_toolbar_insert_stock (toolbar, "zapping-teletext",
				     _("Activate Teletext mode"), NULL,
				     G_CALLBACK (on_python_command1),
				     "zapping.switch_mode('teletext')", -1);
  register_widget (zapping, widget, "toolbar-teletext");

  gtk_container_foreach (GTK_CONTAINER (widget),
			 (GtkCallback) gtk_widget_show_all,
                         NULL);

  return GTK_WIDGET (toolbar);
}

GtkWidget *
create_zapping			(void)
{
  GdkColor col = { 0, 0, 0, 0 };
  GnomeApp *gnome_app;
  GtkWidget *zapping;
  GtkWidget *menubar;
  GtkWidget *toolbar;
  GtkWidget *box;
  GtkWidget *appbar;
  GtkWidget *tv_screen;

  zapping = gnome_app_new ("Zapping", _("Zapping"));
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

  gtk_widget_modify_bg (box, GTK_STATE_NORMAL, &col);

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

  gtk_widget_modify_bg (tv_screen, GTK_STATE_NORMAL, &col);

  z_video_set_min_size (Z_VIDEO (tv_screen), 64, 64 * 3 / 4);

  // XXX free, 4:3, 16:9
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
		    (GCallback) on_tv_screen_button_press_event,
		    (gpointer) NULL);

  if (0) {
    static const char *zc = "/zapping/options/main/toolbar_style";
    GtkWidget *w;

    w = lookup_widget (zapping, "toolbar1");
    //    propagate_toolbar_changes (w);
    zconf_add_hook_while_alive (G_OBJECT (w), zc,
				zconf_hook_toolbar_style,
				GTK_TOOLBAR (w), /* call now */ TRUE);
  }

  return zapping;
}


static GnomeUIInfo
popup_appearance_uiinfo [] = {
  {
    GNOME_APP_UI_TOGGLEITEM, N_("Menu and Toolbar"), NULL,
    G_CALLBACK (on_toolbar_toggled), NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    GDK_H, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_TOGGLEITEM, N_("Keep Window on Top"), NULL,
    G_CALLBACK (on_keep_window_on_top_toggled), NULL, NULL,
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
    GDK_F, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Overlay mode"), NULL,
    G_CALLBACK (on_python_command1), "zapping.switch_mode('preview')", NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_EXECUTE,
    GDK_O, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Capture mode"), NULL,
    G_CALLBACK (on_python_command1), "zapping.switch_mode('capture')", NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_EXECUTE,
    GDK_C, (GdkModifierType) GDK_CONTROL_MASK, NULL
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
    GNOME_APP_PIXMAP_NONE, NULL,
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

GtkWidget *
create_popup_menu1		(GdkEventButton *	event)
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

	  /*
	    widget = lookup_widget(mw, "videotext2");
	    z_change_menuitem(widget,
			      "gnome-stock-table-fill",
			      _("Overlay this page"),
			      _("Return to windowed mode and use the current "
				"page as subtitles"));
	  */

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
    zconf_add_hook_while_alive (G_OBJECT (widget),
				"/zapping/internal/callbacks/hide_controls",
				zconf_hook_toolbar,
				GTK_CHECK_MENU_ITEM (widget), FALSE);

    widget = popup_appearance_uiinfo[1].widget;
    /* XXX tell why not */
    gtk_widget_set_sensitive (widget, !!have_wm_hints);
    zconf_add_hook_while_alive (G_OBJECT (widget),
				"/zapping/options/main/keep_on_top",
				zconf_hook_keep_window_on_top,
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
