/* Zapping
 * Adapted from GtkAdjustment in the GTK+ distro, generic simple model object
 * (C) Iñaki García Etxebarria 200[01]
 * Original copyright of GtkAdjustment
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtksignal.h>
#include "zmodel.h"

enum {
  CHANGED,
  LAST_SIGNAL
};

static void zmodel_class_init (ZModelClass *klass);
static void zmodel_init       (ZModel      *adjustment);

static guint zmodel_signals[LAST_SIGNAL] = { 0 };

GtkType
zmodel_get_type (void)
{
  static GtkType zmodel_type = 0;

  if (!zmodel_type)
    {
      static const GtkTypeInfo zmodel_info =
      {
	"ZModel",
	sizeof (ZModel),
	sizeof (ZModelClass),
	(GtkClassInitFunc) zmodel_class_init,
	(GtkObjectInitFunc) zmodel_init,
	/* reserved_1 */ NULL,
	/* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      zmodel_type = gtk_type_unique (GTK_TYPE_DATA, &zmodel_info);
    }

  return zmodel_type;
}

static void
zmodel_class_init (ZModelClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;

  zmodel_signals[CHANGED] =
    gtk_signal_new ("changed",
                    GTK_RUN_FIRST | GTK_RUN_NO_RECURSE,
                    object_class->type,
                    GTK_SIGNAL_OFFSET (ZModelClass, changed),
                    gtk_marshal_NONE__NONE,
		    GTK_TYPE_NONE, 0);

  gtk_object_class_add_signals (object_class, zmodel_signals, LAST_SIGNAL);

  class->changed = NULL;
}

static void
zmodel_init (ZModel *adjustment)
{
}

GtkObject*
zmodel_new (void)
{
  ZModel *model;

  model = gtk_type_new (zmodel_get_type ());

  return GTK_OBJECT (model);
}

void
zmodel_changed (ZModel        *model)
{
  g_return_if_fail (model != NULL);
  g_return_if_fail (GTK_IS_ZMODEL (model));

  gtk_signal_emit (GTK_OBJECT (model), zmodel_signals[CHANGED]);
}




