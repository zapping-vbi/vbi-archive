/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2004 Michael H. Schimek
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

/* $Id: zapping.c,v 1.1 2004-09-10 04:54:42 mschimek Exp $ */

#include "zmisc.h"
#include "zapping.h"

static GObjectClass *		parent_class;

static void
instance_finalize		(GObject *		object)
{
  /* Zapping *z = ZAPPING (object); */

  parent_class->finalize (object);
}

static void
instance_init			(GTypeInstance *	instance _unused_,
				 gpointer		g_class _unused_)
{
  /* Zapping *z = (Zapping *) instance; */
}

GtkWidget *
zapping_new			(void)
{
  return GTK_WIDGET (g_object_new (TYPE_ZAPPING, NULL));
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
zapping_get_type		(void)
{
  static GType type = 0;
  
  if (!type)
    {
      GTypeInfo info;

      CLEAR (info);

      info.class_size = sizeof (ZappingClass);
      info.class_init = class_init;
      info.instance_size = sizeof (Zapping);
      info.instance_init = instance_init;

      type = g_type_register_static (GNOME_TYPE_APP,
				     "Zapping",
				     &info, (GTypeFlags) 0);
    }

  return type;
}
