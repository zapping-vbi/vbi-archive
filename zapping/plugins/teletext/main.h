#ifndef MAIN_H
#define MAIN_H

#include "config.h"

#ifdef HAVE_LIBZVBI

#include "zmodel.h"
#include "zvbi.h"

#include "bookmark.h"

extern ZModel *			color_zmodel;
extern bookmark_list		bookmarks;
extern GtkActionGroup *		teletext_action_group;
extern GList *			teletext_windows;
extern GList *			teletext_views;

extern GtkWidget *
ttxview_popup			(GtkWidget *		widget,
				 GdkEventButton *	event);
GtkWidget *
ttxview_bookmarks_menu_new	(GtkWidget *		widget);
extern guint
ttxview_hotlist_menu_insert	(GtkMenuShell *		menu,
				 gboolean		separator,
				 gint position);

#endif /* HAVE_LIBZVBI */
#endif /* MAIN_H */
