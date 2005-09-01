/*
 * Zapping (TV viewer for the Gnome Desktop)
 * ZStack Container Widget
 *
 * Copyright (C) 2005 Michael H. Schimek
 *
 * Based on GtkFixed widget from:
 * GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
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
 * GtkFixed -
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#ifndef __Z_STACK_H__
#define __Z_STACK_H__

#include <gtk/gtkcontainer.h>

G_BEGIN_DECLS

#define Z_TYPE_STACK		(z_stack_get_type ())
#define Z_STACK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), Z_TYPE_STACK, ZStack))
#define Z_STACK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), Z_TYPE_STACK, ZStackClass))
#define Z_IS_STACK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), Z_TYPE_STACK))
#define Z_IS_STACK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), Z_TYPE_STACK))
#define Z_STACK_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), Z_TYPE_STACK, ZStackClass))

typedef struct _ZStack ZStack;
typedef struct _ZStackClass ZStackClass;
typedef struct _ZStackChild ZStackChild;

struct _ZStack
{
  GtkContainer container;

  GList *children;
};

struct _ZStackClass
{
  GtkContainerClass parent_class;
};

struct _ZStackChild
{
  GtkWidget *widget;
  gint x;
  gint y;
  gint z;
};

extern GType
z_stack_get_type		(void) G_GNUC_CONST;
extern GtkWidget *
z_stack_new			(void);
extern void
z_stack_put			(ZStack *		stack,
				 GtkWidget *		widget,
				 gint			z);
extern void
z_stack_set_has_window		(ZStack *		stack,
				 gboolean		has_window);

#define ZSTACK_CANVAS -500
#define ZSTACK_VIDEO -400
#define ZSTACK_SUBTITLES -300
#define ZSTACK_OSD -200
#define ZSTACK_GUI -100

G_END_DECLS

#endif /* __Z_STACK_H__ */
