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
#ifndef __TTXVIEW_H__
#define __TTXVIEW_H__

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_LIBZVBI

#include <zmodel.h>

GtkWidget *build_ttxview(void);
gboolean startup_ttxview(void);
void shutdown_ttxview(void);

/**
 * Signals creation/destruction of a view, number of views after
 * creation/destruction is stored in
 * GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(ttxview_model)))
 */
extern ZModel *ttxview_model;

/**
 * Attach the necessary things to the given window to make it a
 * Teletext view.
 * @parent: Toplevel window the teletext view will be in.
 * @da: Drawing area we will be drawing to.
 * @toolbar: Toolbar to attach the TTXView controls to.
 * @appbar: Application bar to show messages into, or NULL.
 */
void
ttxview_attach			(GtkWidget	*parent,
				 GtkWidget	*da,
				 GtkWidget	*toolbar,
				 GtkWidget	*appbar);

/**
 * Detach the TTXView elements from the given window, or does nothing
 * is the window isn't being used as a TTXView.
 */
void
ttxview_detach			(GtkWidget	*parent);

/**
 * Returns the scaled pixbuf for the given widget acting as a
 * TTXView. The return value is volatile blah, blah, blah. See zvbi.h,
 * get_scaled_ttx_page. Can return NULL.
 */
GdkPixbuf *
ttxview_get_scaled_ttx_page	(GtkWidget	*parent);

/**
 * If the given widget is a window acting as a TTXView, add to the
 * given popup menu the TTXView entries
 */
void
process_ttxview_menu_popup	(GtkWidget	*widget,
				 GdkEventButton	*event,
				 GtkMenu	*popup);


/**
 * If the given window is a TTXView, the given page is opened on it.
 */
void
open_in_ttxview				(GtkWidget	*view,
					 gint		page,
					 gint		subpage);

/**
 * If the given window is a TTXView, stores in page, subpage the page
 * currently rendered. Returns FALSE on error.
 * page, subpage can be NULL.
 */
gboolean
get_ttxview_page			(GtkWidget	*view,
					 gint		*page,
					 gint		*subpage);

#endif /* HAVE_LIBZVBI */
#endif /* __TTXVIEW_H__ */
