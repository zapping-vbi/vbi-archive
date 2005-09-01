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

/* $Id: export.h,v 1.3 2005-09-01 01:40:53 mschimek Exp $ */

#ifndef TELETEXT_EXPORT_H
#define TELETEXT_EXPORT_H

#include <gtk/gtk.h>
#include "libvbi/export.h"

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

  vbi3_export *		context;

  vbi3_page *		pg;

  gboolean		reveal;

  gchar *		network;
};

struct _ExportDialogClass
{
  GtkDialogClass	parent_class;
};

extern GType
export_dialog_get_type		(void) G_GNUC_CONST;
extern GtkWidget *
export_dialog_new		(const vbi3_page *	pg,
				 const gchar *		network,
				 gboolean		reveal);

G_END_DECLS

#endif /* TELETEXT_EXPORT_H */
