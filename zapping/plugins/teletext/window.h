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

/* $Id: window.h,v 1.4 2007-08-30 14:14:33 mschimek Exp $ */

#ifndef TELETEXT_WINDOW_H
#define TELETEXT_WINDOW_H

#include <gnome.h>
#include "view.h"

G_BEGIN_DECLS

#define TYPE_TELETEXT_WINDOW (teletext_window_get_type ())
#define TELETEXT_WINDOW(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_TELETEXT_WINDOW, TeletextWindow))
#define TELETEXT_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass),	\
  TYPE_TELETEXT_WINDOW, TeletextWindowClass))
#define IS_TELETEXT_WINDOW(obj)						\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_TELETEXT_WINDOW))
#define IS_TELETEXT_WINDOW_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_TELETEXT_WINDOW))
#define TELETEXT_WINDOW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TYPE_TELETEXT_WINDOW, TeletextWindowClass))

typedef struct _TeletextWindow TeletextWindow;
typedef struct _TeletextWindowClass TeletextWindowClass;

struct _TeletextWindow
{
  GnomeApp		app;

  GtkActionGroup *	action_group;

  /*< private >*/

  vbi3_decoder *	vbi;

  GtkUIManager *	ui_manager;

  TeletextView *	view;

  GtkMenuItem *		top_items;
  vbi3_network		top_network;

  GtkMenuItem *		channel_items;

  GtkMenuItem *		bookmarks_menu;

  GtkCheckMenuItem *	encoding_auto_item;

  gboolean		toolbar_added;
  gboolean		statusbar_added;
};

struct _TeletextWindowClass
{
  GnomeAppClass		parent_class;
};

extern GType
teletext_window_get_type	(void) G_GNUC_CONST;
GtkWidget *
teletext_window_new		(void);

G_END_DECLS

#endif /* TELETEXT_WINDOW_H */

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
