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

/* $Id: window.c,v 1.7 2004-12-07 17:30:43 mschimek Exp $ */

#include "config.h"

#include "libvbi/link.h"
#include "src/zmisc.h"
#include "src/remote.h"
#include "src/i18n.h"
#include "src/zvbi.h"
#include "src/zgconf.h"
#include "view.h"
#include "main.h"
#include "window.h"

static GObjectClass *		parent_class;

static void
on_zvbi_model_changed		(ZModel *		zmodel _unused_,
				 TeletextWindow *	window)
{
  gtk_widget_destroy (GTK_WIDGET (window));
}

/*
	TOP menu
*/

typedef struct {
  TeletextWindow *	window;
  page_num		pn;
} top_menu;

static void
top_menu_destroy		(gpointer		user_data)
{
  top_menu *tm = (top_menu *) user_data;

  page_num_destroy (&tm->pn);
  CLEAR (*tm);
  g_free (tm);
}

static void
on_top_menu_activate		(GtkWidget *		menu_item _unused_,
				 top_menu *		tm)
{
  teletext_view_load_page (tm->window->view,
			   &tm->pn.network,
			   tm->pn.pgno,
			   tm->pn.subno);
}

static GtkWidget *
top_menu_item_new		(TeletextWindow *	window,
				 const vbi3_network *	nk,
				 const vbi3_top_title *	tt,
				 gboolean		connect)
{
  vbi3_ttx_page_stat ps;
  GtkWidget *menu_item;
  const gchar *stock_id;

  ps.page_type = VBI3_UNKNOWN_PAGE;

  /* Error ignored. */
  vbi3_teletext_decoder_get_ttx_page_stat (td, &ps, nk, tt->pgno);

  switch (ps.page_type)
    {
    case VBI3_SUBTITLE_PAGE:
      stock_id = "zapping-teletext";
      break;

    case VBI3_PROGR_SCHEDULE:
      stock_id = "gnome-stock-timer";
      break;

    default:
      stock_id = NULL;
      break;
    }

  if (stock_id)
    {
      GtkWidget *image;

      menu_item = gtk_image_menu_item_new_with_label (tt->title);
      image = gtk_image_new_from_stock (stock_id, GTK_ICON_SIZE_MENU);
      gtk_widget_show (image);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), image);
    }
  else
    {
      menu_item = gtk_menu_item_new_with_label (tt->title);
    }

  gtk_widget_show (menu_item);

  {
    gchar buffer [32];

    g_snprintf (buffer, sizeof (buffer), "%x", tt->pgno);
    z_tooltip_set (menu_item, buffer);
  }

  if (connect)
    {
      top_menu *tm;
      vbi3_bool success;

      tm = g_malloc (sizeof (*tm));

      tm->window = window;
      tm->pn.pgno = tt->pgno;
      tm->pn.subno = tt->subno;

      if (nk)
	success = vbi3_network_copy (&tm->pn.network, nk);
      else
	success = vbi3_teletext_decoder_get_network (td, &tm->pn.network);

      g_assert (success);

      g_object_set_data_full (G_OBJECT (menu_item),
			      "z-top-menu", tm,
			      top_menu_destroy);

      g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (on_top_menu_activate), tm);
    }

  return menu_item;
}

static GtkWidget *
append_top_menu			(TeletextWindow *	window,
				 GtkMenuShell *		menu,
				 const vbi3_network *	nk)
{
  GtkWidget *first_item;
  vbi3_top_title *tt;
  unsigned int n_elements;

  first_item = NULL;

  vbi3_network_set (&window->top_network, nk);

  if (vbi3_network_is_anonymous (nk))
    nk = NULL; /* use received network */

  tt = vbi3_teletext_decoder_get_top_titles (td, nk, &n_elements);

  if (tt && n_elements > 0)
    {
      GtkWidget *subitem;
      GtkMenuShell *submenu;
      guint i;

      first_item = gtk_separator_menu_item_new ();
      gtk_widget_show (first_item);
      gtk_menu_shell_append (menu, first_item);

      subitem = NULL;

      for (i = 0; i < n_elements; ++i)
	{
	  GtkWidget *item;

	  item = top_menu_item_new (window, nk, &tt[i], TRUE);

	  if (tt[i].group && subitem)
	    {
	      gtk_menu_shell_append (submenu, item);
	    }
	  else if ((i + 1) < n_elements && tt[i + 1].group)
	    {
	      GtkWidget *widget;

	      subitem = top_menu_item_new (window, nk, &tt[i], FALSE);
	      gtk_menu_shell_append (menu, subitem);

	      widget = gtk_menu_new ();
	      gtk_widget_show (widget);
	      submenu = GTK_MENU_SHELL (widget);
	      gtk_menu_item_set_submenu (GTK_MENU_ITEM (subitem), widget);
	      gtk_menu_shell_append (submenu, item);
	    }
	  else
	    {
	      gtk_menu_shell_append (menu, item);
	      subitem = NULL;
	    }
	}
    }

  vbi3_top_title_array_delete (tt, n_elements);

  return first_item;
}

static void
update_top_menu			(TeletextWindow *	window)
{
  GtkWidget *item;
  GtkMenuShell *shell;
  GtkWidget *first_item;

  item = gtk_ui_manager_get_widget (window->ui_manager, "/MainMenu/GoSubmenu");
  if (!item)
    return;

  shell = GTK_MENU_SHELL (gtk_menu_item_get_submenu GTK_MENU_ITEM (item));

  if (window->top_items)
    z_menu_shell_chop_off (shell, window->top_items);

  first_item = append_top_menu (window, shell, &window->view->req.network);
  window->top_items = GTK_MENU_ITEM (first_item);
}

static void
on_view_request_changed		(TeletextView *		view,
				 TeletextWindow *	window)
{
  /* Update the TOP menu when the user changes networks. */
  if (!vbi3_network_equal (&view->req.network, &window->top_network))
    update_top_menu (window);
}

/*
	Channels menu
*/

typedef struct {
  TeletextWindow *	window;
  vbi3_network		network;
} channel_menu;

static void
channel_menu_destroy		(gpointer		user_data)
{
  channel_menu *cm = (channel_menu *) user_data;

  vbi3_network_destroy (&cm->network);
  CLEAR (*cm);
  g_free (cm);
}

static void
on_channel_menu_received_toggled (GtkCheckMenuItem *	menu_item,
				 TeletextWindow *	window)
{
  if (menu_item->active)
    teletext_view_switch_network (window->view, &anonymous_network);
}

static void
on_channel_menu_toggled		(GtkCheckMenuItem *	menu_item,
				 channel_menu *		cm)
{
  if (menu_item->active)
    {
      if (0)
	{
	  _vbi3_network_dump (&cm->network, stderr);
	  fputc ('\n', stderr);
	}

      teletext_view_switch_network (cm->window->view, &cm->network);
    }
}

static GtkWidget *
append_channel_menu		(TeletextWindow *	window,
				 GtkMenuShell *		menu)
{
  GtkWidget *first_item;
  vbi3_cache *ca;
  vbi3_network *nk;
  unsigned int n_elements;
  vbi3_bool anon;
  GSList *group;

  /* TRANSLATORS: Choose from the Teletext page cache the currently
     received channel. */
  first_item = gtk_radio_menu_item_new_with_mnemonic (NULL, _("_Received"));
  gtk_widget_show (first_item);
  group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (first_item));

  if ((anon = vbi3_network_is_anonymous (&window->view->req.network)))
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (first_item), TRUE);

  g_signal_connect (G_OBJECT (first_item), "toggled",
		    G_CALLBACK (on_channel_menu_received_toggled), window);

  gtk_menu_shell_append (menu, first_item);

  ca = vbi3_teletext_decoder_get_cache (td);
  nk = vbi3_cache_get_networks (ca, &n_elements);
  vbi3_cache_unref (ca);

  if (nk && n_elements > 0)
    {
      guint i;

      for (i = 0; i < n_elements; ++i)
	{
	  GtkWidget *menu_item;
	  channel_menu *cm;
	  vbi3_bool success;

	  if (nk[i].name)
	    menu_item = gtk_radio_menu_item_new_with_label
	      (group, nk[i].name);
	  else
	    menu_item = gtk_radio_menu_item_new_with_mnemonic
	      (group, _("Unnamed"));

	  gtk_widget_show (menu_item);

	  group = gtk_radio_menu_item_get_group
	    (GTK_RADIO_MENU_ITEM (menu_item));

	  cm = g_malloc (sizeof (*cm));
	  cm->window = window;
	  success = vbi3_network_copy (&cm->network, &nk[i]);
	  g_assert (success);

	  g_object_set_data_full (G_OBJECT (menu_item),
				  "z-channel-menu", cm,
				  channel_menu_destroy);

	  if (!anon)
	    if (vbi3_network_equal (&window->view->req.network, &cm->network))
	      gtk_check_menu_item_set_active
		(GTK_CHECK_MENU_ITEM (menu_item), TRUE);

	  g_signal_connect (G_OBJECT (menu_item), "toggled",
			    G_CALLBACK (on_channel_menu_toggled), cm);

	  gtk_menu_shell_append (menu, menu_item);
	}
    }

  vbi3_network_array_delete (nk, n_elements);

  return first_item;
}

static void
update_channel_menu		(TeletextWindow *	window)
{
  GtkWidget *item;
  GtkMenuShell *shell;
  GtkWidget *first_item;

  item = gtk_ui_manager_get_widget (window->ui_manager,
				    "/MainMenu/ChannelsSubmenu");
  if (!item)
    return;

  shell = GTK_MENU_SHELL (gtk_menu_item_get_submenu GTK_MENU_ITEM (item));

  /* Tried to set_submenu, but when we do that while the menu is active
     (think new channel on composite input) the app freezes. Gtk bug?
     Removing and adding items works fine. */
  z_menu_shell_chop_off (shell, NULL /* all items */);

  first_item = append_channel_menu (window, shell);
  window->channel_items = GTK_MENU_ITEM (first_item);
}

/*
	Bookmarks menu
*/

static void
on_bookmarks_changed		(ZModel *		zmodel _unused_,
				 TeletextWindow *	window)
{
  GtkWidget *menu_item;

  menu_item = gtk_ui_manager_get_widget (window->ui_manager,
					 "/MainMenu/BookmarksSubmenu");
  if (!menu_item)
    return;

  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item),
			     bookmarks_menu_new (window->view));
}

static void
on_bookmarks_destroy		(GObject *		object _unused_,
				 TeletextWindow *	window)
{
  if (bookmarks.zmodel)
    g_signal_handlers_disconnect_matched
      (G_OBJECT (bookmarks.zmodel),
       G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
       0, 0, NULL, G_CALLBACK (on_bookmarks_changed), window);
}

/*
	Encoding menu
*/

typedef struct _encoding_menu encoding_menu;

struct _encoding_menu {
  encoding_menu *	next;
  TeletextWindow *	window;
  GtkCheckMenuItem *	item;
  gchar *		name;
  vbi3_charset_code	code;
};

static void
on_encoding_menu_auto_toggled	(GtkCheckMenuItem *	menu_item,
				 TeletextWindow *	window)
{
  if (menu_item->active)
    teletext_view_set_charset (window->view, (vbi3_charset_code) -1);
}

static void
on_encoding_menu_toggled	(GtkCheckMenuItem *	menu_item,
				 encoding_menu *	em)
{
  if (menu_item->active)
    teletext_view_set_charset (em->window->view, em->code);
}

static void
on_view_charset_changed		(TeletextView *		view,
				 TeletextWindow *	window)
{
  GtkWidget *item;
  encoding_menu *list;
  GtkCheckMenuItem *check;

  item = gtk_ui_manager_get_widget (window->ui_manager,
				    "/MainMenu/ViewSubmenu/EncodingSubmenu");
  if (!item)
    return;

  list = g_object_get_data (G_OBJECT (item), "z-encoding-list");
  g_assert (NULL != list);

  check = window->encoding_auto_item;

  for (; list; list = list->next)
    if (list->code == view->charset)
      {
	check = list->item;
	break;
      }

  if (!check->active)
    gtk_check_menu_item_set_active (check, TRUE);
}

static void
encoding_menu_list_delete	(gpointer		user_data)
{
  encoding_menu *em = (encoding_menu *) user_data;

  while (em)
    {
      encoding_menu *next;

      next = em->next;

      g_free (em->name);
      CLEAR (*em);
      g_free (em);

      em = next;
    }
}

static encoding_menu *
encoding_menu_list_new		(TeletextWindow *	window)
{
  encoding_menu *list;
  vbi3_charset_code code;

  list = NULL;

  for (code = 0; code < 88; ++code)
    {
      const vbi3_character_set *cs;
      vbi3_charset_code code2;
      gchar *item_name;
      encoding_menu *em;
      encoding_menu **emp;
      guint i;

      if (!(cs = vbi3_character_set_from_code (code)))
	continue;

      for (code2 = 0; code2 < code; ++code2)
	{
	  const vbi3_character_set *cs2;

	  if (!(cs2 = vbi3_character_set_from_code (code2)))
	    continue;

	  if (cs->g0 == cs2->g0
	      && cs->g2 == cs2->g2
	      && cs->subset == cs2->subset)
	    break;
	}

      if (code2 < code)
	continue; /* duplicate */

      item_name = NULL;

      for (i = 0; i < N_ELEMENTS (cs->language_code)
	     && cs->language_code[i]; ++i)
	{
	  const char *language_name;

	  language_name = iso639_to_language_name (cs->language_code[i]);
	  if (!language_name)
	    continue;

	  if (!item_name)
	    item_name = g_strdup (language_name);
	  else
	    item_name = z_strappend (item_name, " / ", language_name, NULL);
	}

      if (!item_name)
	continue;

      /* sr/hr/sl */
      if (29 == code)
	item_name = z_strappend (item_name, _(" (Latin)"), NULL);
      else if (32 == code)
	item_name = z_strappend (item_name, _(" (Cyrillic)"), NULL);

      em = g_malloc (sizeof (*em));

      em->window = window;
      em->name = item_name;
      em->code = code;

      for (emp = &list; *emp; emp = &(*emp)->next)
	if (g_utf8_collate ((*emp)->name, item_name) >= 0)
	  break;

      em->next = *emp;
      *emp = em;
    }

  return list;
}

static void
create_encoding_menu		(TeletextWindow *	window)
{
  GtkWidget *menu;
  GtkMenuShell *shell;
  GtkWidget *item;
  GSList *group;
  encoding_menu *list;
  encoding_menu *em;

  item = gtk_ui_manager_get_widget (window->ui_manager,
				    "/MainMenu/ViewSubmenu/EncodingSubmenu");
  if (!item)
    return;

  list = encoding_menu_list_new (window);
  g_object_set_data_full (G_OBJECT (item), "z-encoding-list", list,
			  encoding_menu_list_delete);

  menu = gtk_menu_new ();
  shell = GTK_MENU_SHELL (menu);

  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);

  item = gtk_radio_menu_item_new_with_mnemonic (NULL, _("_Automatic"));
  gtk_widget_show (item);

  window->encoding_auto_item = GTK_CHECK_MENU_ITEM (item);

  group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));

  gtk_check_menu_item_set_active (window->encoding_auto_item, TRUE);

  g_signal_connect (G_OBJECT (item), "toggled",
		    G_CALLBACK (on_encoding_menu_auto_toggled), window);

  gtk_menu_shell_append (shell, item);

  for (em = list; em; em = em->next)
    {
      item = gtk_radio_menu_item_new_with_label (group, em->name);
      gtk_widget_show (item);

      em->item = GTK_CHECK_MENU_ITEM (item);

      group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));

      g_signal_connect (G_OBJECT (item), "toggled",
			G_CALLBACK (on_encoding_menu_toggled), em);

      gtk_menu_shell_append (shell, item);
    }
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
"   <separator/>"
"   <menu action='EncodingSubmenu'>"
"   </menu>"
"  </menu>"
"  <menu action='GoSubmenu'>"
"   <menuitem action='HistoryBack'/>"
"   <menuitem action='HistoryForward'/>"
"   <separator/>"
"   <menuitem action='Home'/>"
"  </menu>"
"  <menu action='ChannelsSubmenu'>"
"  </menu>"
"  <menu action='BookmarksSubmenu'>"
"  </menu>"
" </menubar>"
"</ui>";

static void
create_main_menu		(TeletextWindow *	window)
{
  GError *error = NULL;
  GtkAccelGroup *accel_group;
  GtkWidget *widget;
  gboolean success;

  window->ui_manager = gtk_ui_manager_new ();
  gtk_ui_manager_insert_action_group
    (window->ui_manager, window->action_group, APPEND);
  gtk_ui_manager_insert_action_group
    (window->ui_manager, window->view->action_group, APPEND);
  gtk_ui_manager_insert_action_group
    (window->ui_manager, teletext_action_group, APPEND);
  gtk_ui_manager_insert_action_group
    (window->ui_manager, zapping->generic_action_group, APPEND);

  success = gtk_ui_manager_add_ui_from_string (window->ui_manager,
					       menu_description,
					       NUL_TERMINATED,
					       &error);
  if (!success || error)
    {
      if (error)
	{
	  g_message ("Cannot build Teletext window menu:\n%s", error->message);
	  g_error_free (error);
	  error = NULL;
	}

      exit (EXIT_FAILURE);
    }

  accel_group = gtk_ui_manager_get_accel_group (window->ui_manager);
  gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

  widget = gtk_ui_manager_get_widget (window->ui_manager, "/MainMenu");
  gnome_app_set_menus (GNOME_APP (window), GTK_MENU_BAR (widget));

  vbi3_network_init (&window->top_network);
  update_top_menu (window);

  update_channel_menu (window);

  on_bookmarks_changed (NULL, window);

  /* Update bookmarks menu on bookmark changes. */
  g_signal_connect (G_OBJECT (bookmarks.zmodel), "changed",
		    G_CALLBACK (on_bookmarks_changed), window);
  g_signal_connect (G_OBJECT (window), "destroy",
		    G_CALLBACK (on_bookmarks_destroy), window);

  create_encoding_menu (window);
}

static gboolean
key_press_event			(GtkWidget *		widget,
				 GdkEventKey *		event)
{
  TeletextWindow *window = TELETEXT_WINDOW (widget);

  return (window->view->key_press (window->view, event)
	  || on_user_key_press (widget, event, NULL));
}

static gboolean
on_button_press_event		(GtkWidget *		widget _unused_,
				 GdkEventButton *	event,
				 gpointer		user_data)
{
  TeletextWindow *window = TELETEXT_WINDOW (user_data);
  vbi3_link link;
  gboolean success;
  GtkWidget *menu;

  switch (event->button)
    {
    case 3: /* right button, context menu */
      success = teletext_view_vbi3_link_from_pointer_position
	(window->view, &link, (gint) event->x, (gint) event->y);

      menu = teletext_view_popup_menu_new (window->view,
					   success ? &link : NULL,
					   TRUE);
      if (menu)
	gtk_menu_popup (GTK_MENU (menu),
			NULL, NULL, NULL, NULL,
			event->button, event->time);

      if (success)
	vbi3_link_destroy (&link);

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
  { "EncodingSubmenu", NULL, N_("_Encoding"), NULL, NULL, NULL },
  { "ChannelsSubmenu", NULL, N_("_Channels"), NULL, NULL, NULL },
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
  { "ViewToolbar", NULL, N_("_Toolbar"), NULL,
    NULL, G_CALLBACK (view_toolbar_action), FALSE },
  { "ViewStatusbar", NULL, N_("_Statusbar"), NULL,
    NULL, G_CALLBACK (view_statusbar_action), FALSE },
};

static vbi3_bool
window_vbi3_event_handler	(const vbi3_event *	ev,
				 void *			user_data)
{
  TeletextWindow *window = TELETEXT_WINDOW (user_data);

  switch (ev->type)
    {
    case VBI3_EVENT_TOP_CHANGE:
      /* TOP data is distributed across several Teletext pages,
	 may change as we receive more pages. Also called on
         network changes, when TOP at first becomes empty. */
      if (vbi3_network_is_anonymous (&window->top_network)
	  || vbi3_network_equal (&window->top_network, ev->network))
	update_top_menu (window);
      break;

    case VBI3_EVENT_NETWORK:
    case VBI3_EVENT_REMOVE_NETWORK:
      /* Update the channel menu when cache contents change. */
      update_channel_menu (window);
      break;

    default:
      break;
    }

  return FALSE; /* pass on */
}

static void
instance_finalize		(GObject *		object)
{
  TeletextWindow *window = TELETEXT_WINDOW (object);

  vbi3_teletext_decoder_remove_event_handler
    (td, window_vbi3_event_handler, window);

  teletext_windows = g_list_remove (teletext_windows, window);

  g_signal_handlers_disconnect_matched
    (G_OBJECT (zvbi_get_model ()),
     G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
     0, 0, NULL, G_CALLBACK (on_zvbi_model_changed), window);

  vbi3_network_destroy (&window->top_network);

  g_object_unref (G_OBJECT (window->ui_manager));

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
  z_show_empty_submenu (window->action_group, "BookmarksSubmenu");
  z_show_empty_submenu (window->action_group, "ChannelsSubmenu");
  z_show_empty_submenu (window->action_group, "EncodingSubmenu");

  app = GNOME_APP (window);
  gnome_app_construct (app, "Zapping", "Zapzilla");

  widget = teletext_view_new ();
  gtk_widget_show (widget);
  window->view = TELETEXT_VIEW (widget);

  object = G_OBJECT (window);
  g_object_set_data (object, "TeletextView", window->view);

  /* NOTE view only, not the entire window. */
  g_signal_connect (G_OBJECT (window->view), "button-press-event",
		    G_CALLBACK (on_button_press_event), window);

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

  {
    vbi3_bool success;

    /* Update UI if new information becomes available. */
    success = vbi3_teletext_decoder_add_event_handler
      (td,
       (VBI3_EVENT_NETWORK |
	VBI3_EVENT_TOP_CHANGE |
	VBI3_EVENT_REMOVE_NETWORK),
       window_vbi3_event_handler, window);
  
    g_assert (success);
  }

  g_signal_connect (G_OBJECT (window->view), "z-charset-changed",
		    G_CALLBACK (on_view_charset_changed), window);

  g_signal_connect (G_OBJECT (window->view), "z-request-changed",
		    G_CALLBACK (on_view_request_changed), window);

  g_signal_connect (G_OBJECT (zvbi_get_model ()), "changed",
		    G_CALLBACK (on_zvbi_model_changed), window);

  teletext_windows = g_list_append (teletext_windows, window);
}

GtkWidget *
teletext_window_new		(void)
{
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
