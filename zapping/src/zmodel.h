/* Adapted from gdkkeys.[ch] in the gdk sources. Original copyright
   follows */

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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#ifndef __Z_MODEL_H__
#define __Z_MODEL_H__

#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct _ZModel      ZModel;
typedef struct _ZModelClass ZModelClass;

#define ZMODEL_TYPE              (zmodel_get_type ())
#define ZMODEL(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), ZMODEL_TYPE, ZModel))
#define ZMODEL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), ZMODEL_TYPE, ZModelClass))
#define Z_IS_MODEL(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), ZMODEL_TYPE))
#define Z_IS_MODEL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), ZMODEL_TYPE))
#define ZMODEL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), ZMODEL_TYPE, ZModelClass))


struct _ZModel
{
  GObject parent_instance;
};

struct _ZModelClass
{
  GObjectClass parent_class;

  void (*changed) (ZModel *zmodel);
};

GType zmodel_get_type (void) G_GNUC_CONST;

ZModel* zmodel_new (void);
void zmodel_changed (ZModel *zmodel);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __Z_MODEL_H__ */
