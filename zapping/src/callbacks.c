/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2002 Iñaki García Etxebarria
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

#define ZCONF_DOMAIN "/zapping/internal/callbacks/"
#include "zconf.h"
#include "callbacks.h"
#include "remote.h"
#include "plugins.h"
#include "globals.h"
#include "zmisc.h"
#include "interface.h"
#include "v4linterface.h"
#include "zvbi.h"
#include "ttxview.h"

void
on_zapping_delete_event			(GtkWidget	*widget,
					 gpointer	unused)
{
  cmd_run("zapping.quit()");
}

gboolean
on_tv_screen_button_press_event        (GtkWidget       *widget,
					GdkEvent        *event,
					gpointer        user_data)
{
  GdkEventButton * bevent = (GdkEventButton *) event;
  GList *p;

  if (event->type != GDK_BUTTON_PRESS)
    return TRUE;

  switch (bevent->button)
    {
    case 3:
      {
	GtkMenuShell * menu = GTK_MENU_SHELL(create_popup_menu1());
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
			      "gnome-stock-table-fill",
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
			    "gnome-stock-book-open",
			    _("Show controls"),
			    _("Show the menu and the toolbar"));
#ifdef HAVE_LIBZVBI
	process_ttxview_menu_popup(main_window, bevent, menu);
#endif

	/* Let plugins add their GUI to this context menu */
	p = g_list_first(plugin_list);
	while (p)
	  {
	    plugin_process_popup_menu(main_window, bevent,
				      GTK_MENU (menu),
			      (struct plugin_info*)p->data);
	    p = p->next;
	  }
	gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL,
		       NULL, bevent->button, bevent->time);
      }
      return TRUE;
    case 4:
      cmd_run ("zapping.channel_up()");
      return TRUE;
    case 5:
      cmd_run ("zapping.channel_down()");
      return TRUE;
    case 2:
      if (main_info->current_mode == TVENG_NO_CAPTURE)
	return FALSE;
      cmd_run ("zapping.switch_mode('fullscreen')");
      break;
    default:
      break;
    }

  return TRUE;
}
