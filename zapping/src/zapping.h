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

/* $Id: zapping.h,v 1.1 2004-09-10 04:54:51 mschimek Exp $ */

#ifndef ZAPPING_H
#define ZAPPING_H

#include <gnome.h>
#include "tveng.h"

G_BEGIN_DECLS

#define TYPE_ZAPPING (zapping_get_type ())
#define ZAPPING(obj)							\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_ZAPPING, Zapping))
#define ZAPPING_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass),		\
  TYPE_ZAPPING, ZappingClass))
#define IS_ZAPPING(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_ZAPPING))
#define IS_ZAPPING_CLASS(klass)						\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_ZAPPING))
#define ZAPPING_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj),	\
  TYPE_ZAPPING, ZappingClass))

typedef enum {
  DISPLAY_MODE_NONE		= 0x00,
  DISPLAY_MODE_WINDOW		= 0x10,
  DISPLAY_MODE_BACKGROUND	= 0x20,
  DISPLAY_MODE_FULLSCREEN	= 0x30,
} display_mode;

typedef struct _Zapping Zapping;
typedef struct _ZappingClass ZappingClass;

struct _Zapping
{
  GnomeApp		app;

  /*< private >*/

  tveng_device_info *	info;
  display_mode		display_mode;
};

struct _ZappingClass
{
  GnomeAppClass		parent_class;
};

extern GType
zapping_get_type		(void) G_GNUC_CONST;
GtkWidget *
zapping_new			(void);

G_END_DECLS

#endif /* ZAPPING_H */
