/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2001 I�aki Garc�a Etxebarria
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

/* $Id: export.h,v 1.1 2004-09-22 21:29:07 mschimek Exp $ */

#ifndef EXPORT_H
#define EXPORT_H

#include <gtk/gtk.h>
#include <libzvbi.h>

G_BEGIN_DECLS

#define TYPE_EXPORT_DIALOG (export_dialog_get_type ())
#define EXPORT_DIALOG(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_EXPORT_DIALOG, ExportDialog))
#define EXPORT_DIALOG_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_EXPORT_DIALOG, ExportDialogClass))
#define IS_EXPORT_DIALOG(obj)						\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_EXPORT_DIALOG))
#define IS_EXPORT_DIALOG_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_EXPORT_DIALOG))
#define EXPORT_DIALOG_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_EXPORT_DIALOG, ExportDialogClass))

typedef struct _ExportDialog ExportDialog;
typedef struct _ExportDialogClass ExportDialogClass;

struct _ExportDialog
{
  GtkDialog		dialog;

  /*< private >*/

  GtkWidget *		entry;
  GtkWidget *		format_menu;
  GtkWidget *		option_box;

  vbi_export *		context;

  vbi_page		page;

  gboolean		reveal;

  gchar *		network;
  gchar *		filename;
};

struct _ExportDialogClass
{
  GtkDialogClass	parent_class;
};

extern GType
export_dialog_get_type		(void) G_GNUC_CONST;
extern GtkWidget *
export_dialog_new		(const vbi_page *	pg,
				 const gchar *		network,
				 gboolean		reveal);

G_END_DECLS

#endif /* EXPORT_H */
