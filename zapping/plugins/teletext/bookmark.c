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

/* $Id: bookmark.c,v 1.1 2004-09-22 21:29:07 mschimek Exp $ */

#define ZCONF_DOMAIN "/zapping/ttxview/"
#include "zconf.h"
#include "zmisc.h"

#include "bookmark.h"

void
bookmark_delete			(bookmark *		b)
{
  if (!b)
    return;

  g_free (b->channel);
  g_free (b->description);
  g_free (b);
}

void
bookmark_list_destroy		(bookmark_list *	bl)
{
  g_assert (NULL != bl);

  bookmark_list_remove_all (bl);

  g_object_unref (G_OBJECT (bl->zmodel));

  CLEAR (bl);
}

void
bookmark_list_init		(bookmark_list *	bl)
{
  g_assert (NULL != bl);

  bl->bookmarks = NULL;

  bl->zmodel = ZMODEL (zmodel_new ());
}

void
bookmark_list_remove_all	(bookmark_list *	bl)
{
  g_assert (NULL != bl);

  while (bl->bookmarks)
    {
      bookmark_delete ((bookmark *) bl->bookmarks->data);
      bl->bookmarks = g_list_delete_link (bl->bookmarks, bl->bookmarks);
    }
}

bookmark *
bookmark_list_add		(bookmark_list *	bl,
				 const gchar *		channel,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 const gchar *		description)
{
  bookmark *b;

  g_assert (NULL != bl);

  b = (bookmark *) g_malloc (sizeof (*b));

  b->channel = (channel && *channel) ? g_strdup (channel) : NULL;

  b->pgno = pgno;
  b->subno = subno;

  b->description = (description && *description) ?
    g_strdup (description) : NULL;

  bl->bookmarks = g_list_append (bl->bookmarks, b);

  return b;
}

void
bookmark_list_save		(const bookmark_list *	bl)
{
  const GList *glist;
  guint i;

  g_assert (NULL != bl);

  zconf_delete (ZCONF_DOMAIN "bookmarks");

  i = 0;

  for (glist = bl->bookmarks; glist; glist = glist->next)
    {
      bookmark *b;
      gchar buf[200];
      gint n;

      b = (bookmark *) glist->data;

      n = snprintf (buf, sizeof (buf) - 20, ZCONF_DOMAIN "bookmarks/%u/", i);
      g_assert (n > 0 && n < (gint) sizeof (buf) - 20);

      if (b->channel)
	{
	  strcpy (buf + n, "channel");
	  zconf_create_string (b->channel, "Channel", buf);
	}

      strcpy (buf + n, "page");
      zconf_create_int (b->pgno, "Page", buf);
      strcpy (buf + n, "subpage");
      zconf_create_int (b->subno, "Subpage", buf);

      if (b->description)
	{
	  strcpy (buf + n, "description");
	  zconf_create_string (b->description, "Description", buf);
	}

      ++i;
    }
}

void
bookmark_list_load		(bookmark_list *	bl)
{
  gchar *buffer;
  gchar *buffer2;
  gint pgno;
  gint subno;
  const gchar *channel;
  const gchar *descr;
  guint i;

  g_assert (NULL != bl);

  bookmark_list_remove_all (bl);

  i = 0;

  while (zconf_get_nth (i, &buffer, ZCONF_DOMAIN "bookmarks"))
    {
      buffer2 = g_strconcat (buffer, "/channel", NULL);
      channel = zconf_get_string (NULL, buffer2);
      g_free (buffer2);

      buffer2 = g_strconcat (buffer, "/page", NULL);
      zconf_get_int (&pgno, buffer2);
      g_free (buffer2);

      buffer2 = g_strconcat (buffer, "/subpage", NULL);
      zconf_get_int (&subno, buffer2);
      g_free (buffer2);

      buffer2 = g_strconcat (buffer, "/description", NULL);
      descr = zconf_get_string (NULL, buffer2);
      g_free (buffer2);

      bookmark_list_add (bl, channel, pgno, subno, descr);

      g_free (buffer);

      ++i;
    }
}

enum {
  BOOKMARK_COLUMN_CHANNEL,
  BOOKMARK_COLUMN_PGNO,
  BOOKMARK_COLUMN_SUBNO,
  BOOKMARK_COLUMN_DESCRIPTION,
  BOOKMARK_COLUMN_EDITABLE,
  BOOKMARK_NUM_COLUMNS
};

static GObjectClass *parent_class;

static void
page_cell_data_func		(GtkTreeViewColumn *	column _unused_,
				 GtkCellRenderer *	cell,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter,
				 gpointer		user_data _unused_)
{
  gchar buf[32];
  guint pgno;
  guint subno;

  gtk_tree_model_get (model, iter,
		      BOOKMARK_COLUMN_PGNO,	&pgno,
		      BOOKMARK_COLUMN_SUBNO,	&subno,
		      -1);

  if (subno && subno != VBI_ANY_SUBNO)
    g_snprintf (buf, sizeof (buf), "%x.%02x", pgno & 0xFFF, subno & 0xFF);
  else
    g_snprintf (buf, sizeof (buf), "%x", pgno & 0xFFF);

  g_object_set (GTK_CELL_RENDERER (cell), "text", buf, NULL);
}

static void
on_descr_cell_edited		(GtkCellRendererText *	cell _unused_,
				 const gchar *		path_string,
				 const gchar *		new_text,
				 BookmarkEditor *	sp)
{
  GtkTreePath *path;
  GtkTreeIter iter;

  path = gtk_tree_path_new_from_string (path_string);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (sp->store), &iter, path);

  gtk_list_store_set (sp->store, &iter,
		      BOOKMARK_COLUMN_DESCRIPTION, new_text,
		      -1);

  gtk_tree_path_free (path);
}

static void
on_remove_clicked		(GtkWidget *		widget _unused_,
				 BookmarkEditor *	sp)
{
  z_tree_view_remove_selected (sp->tree_view, sp->selection,
			       GTK_TREE_MODEL (sp->store));
}

static void
on_selection_changed		(GtkTreeSelection *	selection,
				 BookmarkEditor *	sp)
{
  GtkTreeIter iter;
  gboolean selected;

  selected = z_tree_selection_iter_first (selection,
					  GTK_TREE_MODEL (sp->store),
					  &iter);

  gtk_widget_set_sensitive (sp->remove, selected);
}

static void
on_cancel_clicked		(GtkWidget *		widget,
				 gpointer 		user_data _unused_)
{
  while (widget->parent)
    widget = widget->parent;

  gtk_widget_destroy (widget);
}

static gboolean
foreach_add			(GtkTreeModel *		model,
				 GtkTreePath *		path _unused_,
				 GtkTreeIter *		iter,
				 gpointer 		user_data)
{
  BookmarkEditor *sp = (BookmarkEditor *) user_data;
  guint pgno;
  guint subno;
  gchar *channel;
  gchar *descr;

  gtk_tree_model_get (model, iter,
		      BOOKMARK_COLUMN_CHANNEL,		&channel,
		      BOOKMARK_COLUMN_PGNO,		&pgno,
		      BOOKMARK_COLUMN_SUBNO,		&subno,
		      BOOKMARK_COLUMN_DESCRIPTION,	&descr,
		      -1);

  bookmark_list_add (sp->bl, channel,
		     (vbi_pgno) pgno,
		     (vbi_subno) subno, descr);

  return FALSE; /* continue */
}

static void
on_ok_clicked			(GtkWidget *		button,
				 BookmarkEditor *	sp)
{
  bookmark_list_remove_all (sp->bl);

  gtk_tree_model_foreach (GTK_TREE_MODEL (sp->store),
			  foreach_add, sp);

  zmodel_changed (sp->bl->zmodel);

  on_cancel_clicked (button, sp);
}

static void
instance_finalize		(GObject *		object)
{
  BookmarkEditor *t = BOOKMARK_EDITOR (object);

  g_object_unref (t->store);

  parent_class->finalize (object);
}

static void
instance_init			(GTypeInstance *	instance,
				 gpointer		g_class _unused_)
{
  BookmarkEditor *sp = (BookmarkEditor *) instance;
  GtkWindow *window;
  GtkBox *vbox;
  GtkWidget *scrolled_window;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkWidget *widget;

  window = GTK_WINDOW (sp);
  gtk_window_set_title (window, _("Bookmarks"));

  widget = gtk_vbox_new (FALSE, 3);
  gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
  vbox = GTK_BOX (widget);
  gtk_box_pack_start (GTK_BOX (sp->dialog.vbox), widget, TRUE, TRUE, 0);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
				       GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (vbox, scrolled_window, TRUE, TRUE, 0);

  sp->store = gtk_list_store_new (BOOKMARK_NUM_COLUMNS,
				  G_TYPE_STRING,
				  G_TYPE_UINT,
				  G_TYPE_UINT,
				  G_TYPE_STRING,
				  G_TYPE_BOOLEAN);

  widget = gtk_tree_view_new_with_model (GTK_TREE_MODEL (sp->store));
  sp->tree_view = GTK_TREE_VIEW (widget);

  gtk_tree_view_set_rules_hint (sp->tree_view, TRUE);
  gtk_tree_view_set_reorderable (sp->tree_view, TRUE);
  gtk_tree_view_set_search_column (sp->tree_view,
				   BOOKMARK_COLUMN_DESCRIPTION);

  gtk_container_add (GTK_CONTAINER (scrolled_window), widget);

  sp->selection = gtk_tree_view_get_selection (sp->tree_view);
  gtk_tree_selection_set_mode (sp->selection, GTK_SELECTION_MULTIPLE);

  g_signal_connect (G_OBJECT (sp->selection), "changed",
  		    G_CALLBACK (on_selection_changed), sp);

  column = gtk_tree_view_column_new_with_attributes
    (_("Channel"), gtk_cell_renderer_text_new (),
     "text", BOOKMARK_COLUMN_CHANNEL,
     NULL);
  gtk_tree_view_append_column (sp->tree_view, column);

  gtk_tree_view_insert_column_with_data_func
    (sp->tree_view, -1, _("Page"), gtk_cell_renderer_text_new (),
     page_cell_data_func, NULL, NULL);

  renderer =  gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes
    (_("Description"), renderer,
     "text",	 BOOKMARK_COLUMN_DESCRIPTION,
     "editable", BOOKMARK_COLUMN_EDITABLE,
     NULL);
  gtk_tree_view_append_column (sp->tree_view, column);

  g_signal_connect (renderer, "edited",
		    G_CALLBACK (on_descr_cell_edited), sp);

  widget = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
  sp->remove = widget;
  gtk_box_pack_start (vbox, widget, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (widget, FALSE);

  g_signal_connect (G_OBJECT (widget), "clicked",
  		    G_CALLBACK (on_remove_clicked), sp);

  widget = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
  gtk_dialog_add_action_widget (&sp->dialog, widget, 1);
  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (widget), "clicked",
  		    G_CALLBACK (on_cancel_clicked), sp);

  widget = gtk_button_new_from_stock (GTK_STOCK_OK);
  gtk_dialog_add_action_widget (&sp->dialog, widget, 2);
  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (on_ok_clicked), sp);
}

static void
append				(BookmarkEditor *	sp,
				 const bookmark *	b)
{
  GtkTreeIter iter;
  const gchar *channel;
  const gchar *description;

  channel = b->channel ? b->channel : "";
  description = b->description ? b->description : "";

  gtk_list_store_append (sp->store, &iter);
  gtk_list_store_set (sp->store, &iter,
		      BOOKMARK_COLUMN_CHANNEL,		channel,
		      BOOKMARK_COLUMN_PGNO,		b->pgno,
		      BOOKMARK_COLUMN_SUBNO,		b->subno,
		      BOOKMARK_COLUMN_DESCRIPTION,	description,
		      BOOKMARK_COLUMN_EDITABLE,		TRUE,
		      -1);
}

GtkWidget *
bookmark_editor_new		(bookmark_list *	bl)
{
  BookmarkEditor *sp;
  GList *glist;

  sp = (BookmarkEditor *) g_object_new (TYPE_BOOKMARK_EDITOR, NULL);

  sp->bl = bl;

  for (glist = bl->bookmarks; glist; glist = glist->next)
    append (sp, (bookmark *) glist->data);

  return GTK_WIDGET (sp);
}

static void
class_init			(gpointer		g_class,
				 gpointer		class_data _unused_)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (g_class);
  parent_class = g_type_class_peek_parent (object_class);

  object_class->finalize = instance_finalize;
}

GType
bookmark_editor_get_type	(void)
{
  static GType type = 0;
  
  if (!type)
    {
      GTypeInfo info;

      CLEAR (info);

      info.class_size = sizeof (BookmarkEditorClass);
      info.class_init = class_init;
      info.instance_size = sizeof (BookmarkEditor);
      info.instance_init = instance_init;

      type = g_type_register_static (GTK_TYPE_DIALOG,
				     "BookmarkEditor",
				     &info, (GTypeFlags) 0);
    }

  return type;
}
