/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __ZMODEL_H__
#define __ZMODEL_H__


#include <gdk/gdk.h>
#include <gtk/gtkdata.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_ZMODEL              (zmodel_get_type ())
#define ZMODEL(obj)                  (GTK_CHECK_CAST ((obj), GTK_TYPE_ZMODEL, ZModel))
#define ZMODEL_CLASS(klass)          (GTK_CHECK_CLASS_CAST ((klass), GTK_TYPE_ZMODEL, ZModelClass))
#define GTK_IS_ZMODEL(obj)               (GTK_CHECK_TYPE ((obj), GTK_TYPE_ZMODEL))
#define GTK_IS_ZMODEL_CLASS(klass)       (GTK_CHECK_CLASS_TYPE ((klass), GTK_TYPE_ZMODEL))


typedef struct _ZModel	    ZModel;
typedef struct _ZModelClass  ZModelClass;

struct _ZModel
{
  GtkData data;
};

struct _ZModelClass
{
  GtkDataClass parent_class;
  
  void (* changed)	 (ZModel *model);
};


GtkType	   zmodel_get_type			(void);
GtkObject* zmodel_new				(void);
void	   zmodel_changed			(ZModel		 *model);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __ZMODEL_H__ */

