/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "zmisc.h"
#include "zmodel.h"
#include "zmarshalers.h"

enum {
  CHANGED,
  LAST_SIGNAL
};

static void zmodel_init       (ZModel      *zmodel);
static void zmodel_class_init (ZModelClass *klass);

static gpointer parent_class = NULL;

static guint signals[LAST_SIGNAL] = { 0 };

GType
zmodel_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (ZModelClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) zmodel_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (ZModel),
        0,              /* n_preallocs */
        (GInstanceInitFunc) zmodel_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "ZModel",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
zmodel_init (ZModel *zmodel _unused_)
{

}

static void
zmodel_class_init (ZModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  signals[CHANGED] =
    g_signal_new ("changed",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (ZModelClass, changed),
		  NULL, NULL,
		  z_marshal_VOID__VOID,
		  G_TYPE_NONE,
		  0);
}

ZModel *
zmodel_new (void)
{
  return (ZMODEL(g_object_new (ZMODEL_TYPE, NULL)));
}

void
zmodel_changed (ZModel *zmodel)
{
  g_signal_emit (zmodel, signals[CHANGED], 0);
}
