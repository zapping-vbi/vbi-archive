/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2000 Iñaki García Etxebarria
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

#ifndef TTXVIEW_H
#define TTXVIEW_H

#include "config.h"

#ifdef HAVE_LIBZVBI

#include "zmodel.h"

/* Signals creation or destruction of a view. */
extern ZModel *		ttxview_zmodel;

#define NUM_TTXVIEWS(zmodel)						\
  GPOINTER_TO_INT (g_object_get_data (G_OBJECT (zmodel), "count"))

/**
 * Returns the scaled pixbuf for the given widget acting as a
 * TTXView. The return value is volatile blah, blah, blah. See zvbi.h,
 * get_scaled_ttx_page. Can return NULL.
 */
GdkPixbuf *
ttxview_get_scaled_ttx_page	(GtkWidget	*parent);

/**
 * If the given window is a TTXView, stores in page, subpage the page
 * currently rendered. Returns FALSE on error.
 * page, subpage can be NULL.
 */
gboolean
get_ttxview_page			(GtkWidget	*view,
					 gint		*page,
					 gint		*subpage);

extern GtkWidget *
ttxview_popup			(GtkWidget *		widget,
				 GdkEventButton *	event);
extern GtkWidget *
ttxview_subtitles_menu_new	(void);
GtkWidget *
ttxview_bookmarks_menu_new	(GtkWidget *		widget);
extern guint
ttxview_hotlist_menu_append	(GtkMenuShell *		menu,
				 gboolean		separator);

extern gboolean
startup_ttxview			(void);
extern void
shutdown_ttxview		(void);
extern GtkWidget *
ttxview_new			(void);
extern void
ttxview_attach			(GtkWidget *		parent,
				 GtkWidget *		da,
				 GtkWidget *		toolbar,
				 GtkWidget *		appbar);
extern void
ttxview_detach			(GtkWidget *		parent);

#endif /* HAVE_LIBZVBI */
#endif /* TTXVIEW_H */
