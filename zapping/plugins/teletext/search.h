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

/* $Id: search.h,v 1.2 2004-11-03 06:46:56 mschimek Exp $ */

#ifndef SEARCH_H
#define SEARCH_H

#include <gtk/gtk.h>
#include "libvbi/bcd.h"		/* vbi3_pgno, vbi3_subno */
#include "libvbi/search.h"	/* vbi3_search */
#include "view.h"

G_BEGIN_DECLS

#define TYPE_SEARCH_DIALOG (search_dialog_get_type ())
#define SEARCH_DIALOG(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_SEARCH_DIALOG, SearchDialog))
#define SEARCH_DIALOG_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_SEARCH_DIALOG, SearchDialogClass))
#define IS_SEARCH_DIALOG(obj)						\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_SEARCH_DIALOG))
#define IS_SEARCH_DIALOG_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_SEARCH_DIALOG))
#define SEARCH_DIALOG_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_SEARCH_DIALOG, SearchDialogClass))

typedef struct _SearchDialog SearchDialog;
typedef struct _SearchDialogClass SearchDialogClass;

struct _SearchDialog
{
  GtkDialog		dialog;

  /*< private >*/

  GtkLabel *		label;
  GtkWidget *		entry;
  GtkWidget *		back;
  GtkWidget *		forward;

  TeletextView *	view;

  vbi3_search *		context;
  gchar *		text;
  gint			direction;
  gboolean		searching;
  vbi3_pgno		start_pgno;
  vbi3_subno		start_subno;
  gboolean		regexp;
  gboolean		casefold;
  gboolean		all_channels;
};

struct _SearchDialogClass
{
  GtkDialogClass	parent_class;
};

extern GType
search_dialog_get_type		(void) G_GNUC_CONST;
extern GtkWidget *
search_dialog_new		(TeletextView *		view);

G_END_DECLS

#endif /* SEARCH_H */
