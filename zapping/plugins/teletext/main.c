/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2000, 2001, 2002 Iñaki García Etxebarria
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: main.c,v 1.6 2004-11-03 06:45:46 mschimek Exp $ */

#include "config.h"

#define ZCONF_DOMAIN "/zapping/ttxview/"
#include "src/zconf.h"
#include "src/plugin_common.h"
#include "src/properties.h"
#include "src/zmisc.h"
#include "src/remote.h"
#include "view.h"
#include "window.h"
#include "preferences.h"
#include "main.h"

vbi3_teletext_decoder *		td;

vbi3_network			anonymous_network;

GtkActionGroup *		teletext_action_group;

bookmark_list			bookmarks;
BookmarkEditor *		bookmarks_dialog;

GList *				teletext_windows;
GList *				teletext_views;

static GtkWidget *
ttxview_popup_menu_new		(GtkWidget *		widget,
				 GdkEventButton *	event)
{
  TeletextView *view;
  vbi3_link link;

  if (!(view = teletext_view_from_widget (widget)))
    return NULL;

  teletext_view_vbi3_link_from_pointer_position
    (view, &link, (int) event->x, (int) event->y);

  widget = teletext_view_popup_menu_new (view, &link, FALSE);

  vbi3_link_destroy (&link);

  return widget;
}

static GtkWidget *
ttxview_bookmarks_menu_new	(GtkWidget *		widget)
{
  TeletextView *view;

  if ((view = teletext_view_from_widget (widget)))
    return bookmarks_menu_new (view);

  return NULL;
}

static PyObject *
py_ttx_open_new			(PyObject *		self _unused_,
				 PyObject *		args)
{
  TeletextView *view;
  TeletextWindow *window;
  GtkWidget *widget;
  int page;
  int subpage;
  vbi3_pgno pgno;
  vbi3_subno subno;
  guint width;
  guint height;

  view = teletext_view_from_widget (python_command_widget ());

  if (view && view->pg)
    {
      page = vbi3_bcd2dec (view->pg->pgno);
      subpage = vbi3_bcd2dec (view->pg->subno & 0xFF);
    }
  else
    {
      page = 100;
      subpage = -1;
    }

  if (!ParseTuple (args, "|ii", &page, &subpage))
    g_error ("zapping.ttx_open_new(|ii)");

  if (page >= 100 && page <= 899)
    pgno = vbi3_dec2bcd (page);
  else
    py_return_false;

  if (subpage < 0)
    subno = VBI3_ANY_SUBNO;
  else if ((guint) subpage <= 99)
    subno = vbi3_dec2bcd (subpage);
  else
    py_return_false;

  width = 300;
  height = 200;

  if (view)
    gdk_window_get_geometry (GTK_WIDGET (view)->window,
			     /* x */ NULL,
			     /* y */ NULL,
			     &width,
			     &height,
			     /* depth */ NULL);

  widget = teletext_window_new ();
  window = TELETEXT_WINDOW (widget);

  teletext_view_load_page (window->view, &anonymous_network, pgno, subno);

  gtk_widget_realize (widget);

  z_update_gui ();

  gdk_window_resize (widget->window, (int) width, (int) height);

  gtk_widget_show (widget);

  py_return_true;
}

static void
preferences_action		(GtkAction *		action _unused_,
				 gpointer		user_data _unused_)
{
  python_command_printf (NULL, "zapping.properties('Plugins','Teletext')");
}

static PyObject *
py_ttx_color			(PyObject *		self _unused_,
				 PyObject *		args _unused_)
{
  preferences_action (NULL, NULL);
  py_return_true;
}

static GtkActionEntry
actions [] = {
  { "Preferences", NULL, N_("_Preferences"), NULL,
    NULL, G_CALLBACK (preferences_action) },
  { "Colors", NULL, N_("_Colors"), NULL,
    NULL, G_CALLBACK (preferences_action) /* sic */},
};

static void
plugin_close			(void)
{
  while (teletext_windows)
    gtk_widget_destroy (GTK_WIDGET (teletext_windows->data));

  if (bookmarks_dialog)
    {
      gtk_widget_destroy (GTK_WIDGET (bookmarks_dialog));
      bookmarks_dialog = NULL;
    }

  bookmark_list_save (&bookmarks);
  bookmark_list_destroy (&bookmarks);

  vbi3_network_destroy (&anonymous_network);

  stop_zvbi ();

  vbi3_teletext_decoder_delete (td);
  td = NULL;
}

static void
properties_add			(GtkDialog *		dialog)
{
  static SidebarEntry se = {
    .label		= N_("Teletext"),
    .icon_name		= "teletext48.png",
    .create		= teletext_prefs_new,
    .apply		= (void (*)(GtkWidget *)) teletext_prefs_apply,
    .cancel		= (void (*)(GtkWidget *)) teletext_prefs_cancel,
    .help_link_id	= "zapping-settings-vbi",
  };
  static const SidebarGroup sg = {
    N_("Plugins"), &se, 1
  };

  standard_properties_add (dialog, &sg, 1, /* glade */ NULL);
}

static gboolean
plugin_init			(PluginBridge		bridge _unused_,
				 tveng_device_info *	info _unused_)
{
  static const property_handler ph = {
    .add = properties_add,
  };

  /* Preliminary. */
  _ttxview_popup_menu_new = ttxview_popup_menu_new;
  _ttxview_bookmarks_menu_new = ttxview_bookmarks_menu_new;
  _ttxview_hotlist_menu_insert = ttxview_hotlist_menu_insert;

  append_property_handler (&ph);

  teletext_action_group = gtk_action_group_new ("TeletextActions");
#ifdef ENABLE_NLS
  gtk_action_group_set_translation_domain (teletext_action_group,
					   GETTEXT_PACKAGE);
#endif					   
  gtk_action_group_add_actions (teletext_action_group,
				actions, G_N_ELEMENTS (actions), NULL);

  vbi3_network_init (&anonymous_network);

  bookmark_list_init (&bookmarks);
  bookmark_list_load (&bookmarks);

  zcc_char (g_get_home_dir(), "Export directory", "exportdir");

  cmd_register ("ttx_open_new", py_ttx_open_new, METH_VARARGS,
		("Open new Teletext window"), "zapping.ttx_open_new()");
  cmd_register ("ttx_color", py_ttx_color, METH_VARARGS,
		("Open Teletext color dialog"), "zapping.ttx_color()");

  td = vbi3_teletext_decoder_new (NULL, NULL, VBI3_VIDEOSTD_SET_625_50);
  g_assert (NULL != td);

  start_zvbi ();

  return TRUE;
}

static void
plugin_get_info			(const gchar **		canonical_name,
				 const gchar **		descriptive_name,
				 const gchar **		description,
				 const gchar **		short_description,
				 const gchar **		author,
				 const gchar **		version)
{
  if (canonical_name)
    *canonical_name = "teletext";
  if (descriptive_name)
    *descriptive_name = N_("Teletext plugin");
  if (description)
    *description = "";
  if (short_description)
    *short_description = "";
  if (author)
    *author = "";
  if (version)
    *version = "2.0";
}

static struct plugin_misc_info *
plugin_get_misc_info		(void)
{
  static struct plugin_misc_info returned_struct = {
    sizeof (struct plugin_misc_info),
    6, /* plugin priority */
    0 /* category */
  };

  return &returned_struct;
}

gboolean
plugin_get_symbol		(const gchar *		name,
				 gint			hash,
				 gpointer *		ptr)
{
  static const struct plugin_exported_symbol symbols [] = {
    SYMBOL (plugin_close, 0x1234),
    SYMBOL (plugin_get_info, 0x1234),
    SYMBOL (plugin_get_misc_info, 0x1234),
    SYMBOL (plugin_init, 0x1234),
    SYMBOL2 (teletext, view_new),
    SYMBOL2 (teletext, view_from_widget),
    SYMBOL2 (teletext, toolbar_new),
  };
  guint i;

  for (i = 0; i < N_ELEMENTS (symbols); ++i)
    if (0 == strcmp (symbols[i].symbol, name))
      {
	if (symbols[i].hash != hash)
	  {
	    if (ptr)
	      *ptr = GINT_TO_POINTER(0x3); /* hash collision code */
	    g_warning("Check error: \"%s\" in plugin %s "
		      "has hash 0x%x vs. 0x%x",
		      name, "teletext", symbols[i].hash, hash);
	    return FALSE;
	  }
	if (ptr)
	  *ptr = symbols[i].ptr;
	return TRUE;
      }

  if (ptr)
    *ptr = GINT_TO_POINTER(0x2);
  return FALSE;
}

gint
plugin_get_protocol		(void)
{
  return PLUGIN_PROTOCOL;
}
