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

/* $Id: toolbar.h,v 1.1 2004-09-22 21:29:07 mschimek Exp $ */

#ifndef TOOLBAR_H
#define TOOLBAR_H

#include <gtk/gtk.h>
#include <libzvbi.h>

G_BEGIN_DECLS

#define TYPE_TELETEXT_TOOLBAR (teletext_toolbar_get_type ())
#define TELETEXT_TOOLBAR(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_TELETEXT_TOOLBAR, TeletextToolbar))
#define TELETEXT_TOOLBAR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TYPE_TELETEXT_TOOLBAR, TeletextToolbarClass))
#define IS_TELETEXT_TOOLBAR(obj)					\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_TELETEXT_TOOLBAR))
#define IS_TELETEXT_TOOLBAR_CLASS(klass)				\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_TELETEXT_TOOLBAR))
#define TELETEXT_TOOLBAR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TYPE_TELETEXT_TOOLBAR, TeletextToolbarClass))

typedef struct _TeletextToolbar TeletextToolbar;
typedef struct _TeletextToolbarClass TeletextToolbarClass;

struct _TeletextToolbar
{
  GtkToolbar		toolbar;

  /*< private >*/

  GtkBox *		box1;
  GtkToggleButton *	hold;
  GtkLabel *		url;
  GtkBox *		box2;
  GtkToggleToolButton *	reveal;
};

struct _TeletextToolbarClass
{
  GtkToolbarClass	parent_class;
};

static __inline__ void
teletext_toolbar_set_reveal	(TeletextToolbar *	toolbar,
				 gboolean		reveal)
{
  if (reveal != gtk_toggle_tool_button_get_active (toolbar->reveal))
    gtk_toggle_tool_button_set_active (toolbar->reveal, reveal);
}

static __inline__ void
teletext_toolbar_set_hold	(TeletextToolbar *	toolbar,
				 gboolean		hold)
{
  if (hold != gtk_toggle_button_get_active (toolbar->hold))
    gtk_toggle_button_set_active (toolbar->hold, hold);
}

extern void
teletext_toolbar_set_url	(TeletextToolbar *	toolbar,
				 vbi_pgno		pgno,
				 vbi_subno		subno);
extern GType
teletext_toolbar_get_type		(void) G_GNUC_CONST;
extern GtkWidget *
teletext_toolbar_new			(GtkActionGroup *action_group);

G_END_DECLS

#endif /* TOOLBAR_H */
