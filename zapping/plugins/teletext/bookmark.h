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

/* $Id: bookmark.h,v 1.3 2005-09-01 01:40:53 mschimek Exp $ */

#ifndef TELETEXT_BOOKMARK_H
#define TELETEXT_BOOKMARK_H

#include <gtk/gtk.h>

#include "src/zmodel.h"
#include "page_num.h"
#include "view.h"

G_BEGIN_DECLS

typedef struct {
  gchar *		channel;
  gchar *		description;
  page_num		pn;
} bookmark;

typedef struct {
  GList *		bookmarks;
  ZModel *		zmodel;
} bookmark_list;

extern void
bookmark_delete			(bookmark *		b);
extern void
bookmark_list_destroy		(bookmark_list *	bl);
extern void
bookmark_list_init		(bookmark_list *	bl);
extern void
bookmark_list_remove_all	(bookmark_list *	bl);
extern bookmark *
bookmark_list_add		(bookmark_list *	bl,
				 const gchar *		channel,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 const gchar *		description);
extern void
bookmark_list_save		(const bookmark_list *	bl);
extern void
bookmark_list_load		(bookmark_list *	bl);

GtkWidget *
bookmarks_menu_new		(TeletextView *		view);

#define TYPE_BOOKMARK_EDITOR	(bookmark_editor_get_type ())
#define BOOKMARK_EDITOR(obj)						\
 (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_BOOKMARK_EDITOR, BookmarkEditor))
#define BOOKMARK_EDITOR_CLASS(klass)					\
 (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_BOOKMARK_EDITOR, BookmarkEditorClass))
#define IS_BOOKMARK_EDITOR(obj)						\
 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_BOOKMARK_EDITOR))
#define IS_BOOKMARK_EDITOR_CLASS(klass)					\
 (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_BOOKMARK_EDITOR))
#define BOOKMARK_EDITOR_GET_CLASS(obj)					\
 (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_BOOKMARK_EDITOR, BookmarkEditorClass))

typedef struct _BookmarkEditor BookmarkEditor;
typedef struct _BookmarkEditorClass BookmarkEditorClass;

struct _BookmarkEditor
{
  GtkDialog		dialog;

  /*< private >*/

  GtkTreeView *		tree_view;
  GtkTreeSelection *	selection;
  GtkListStore *	store;
  GtkWidget *		remove;

  bookmark_list *	bl;
};

struct _BookmarkEditorClass
{
  GtkDialogClass	parent_class;
};

extern GType
bookmark_editor_get_type	(void) G_GNUC_CONST;
extern GtkWidget *
bookmark_editor_new		(bookmark_list *	bl);

G_END_DECLS

#endif /* TELETEXT_BOOKMARK_H */
