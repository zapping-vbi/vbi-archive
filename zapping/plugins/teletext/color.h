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

/* $Id: color.h,v 1.1 2004-09-22 21:29:07 mschimek Exp $ */

#ifndef COLOR_H
#define COLOR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TYPE_COLOR_DIALOG (color_dialog_get_type ())
#define COLOR_DIALOG(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_COLOR_DIALOG, ColorDialog))
#define COLOR_DIALOG_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_COLOR_DIALOG, ColorDialogClass))
#define IS_COLOR_DIALOG(obj)						\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_COLOR_DIALOG))
#define IS_COLOR_DIALOG_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_COLOR_DIALOG))
#define COLOR_DIALOG_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_COLOR_DIALOG, ColorDialogClass))

typedef struct _ColorDialog ColorDialog;
typedef struct _ColorDialogClass ColorDialogClass;

struct _ColorDialog
{
  GtkWindow		window;
};

struct _ColorDialogClass
{
  GtkWindowClass	parent_class;
};

extern GType
color_dialog_get_type		(void) G_GNUC_CONST;
extern GtkWidget *
color_dialog_new		(void);

G_END_DECLS

#endif /* COLOR_H */
