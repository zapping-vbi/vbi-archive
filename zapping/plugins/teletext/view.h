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

/* $Id: view.h,v 1.1 2004-09-22 21:29:07 mschimek Exp $ */

#ifndef VIEW_H
#define VIEW_H

#include <gnome.h>
#include <libzvbi.h>
#include "toolbar.h"

G_BEGIN_DECLS

#define TYPE_TELETEXT_VIEW (teletext_view_get_type ())
#define TELETEXT_VIEW(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_TELETEXT_VIEW, TeletextView))
#define TELETEXT_VIEW_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_TELETEXT_VIEW, TeletextViewClass))
#define IS_TELETEXT_VIEW(obj)						\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_TELETEXT_VIEW))
#define IS_TELETEXT_VIEW_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_TELETEXT_VIEW))
#define TELETEXT_VIEW_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_TELETEXT_VIEW, TeletextViewClass))

typedef struct _TeletextView TeletextView;
typedef struct _TeletextViewClass TeletextViewClass;

struct _TeletextView
{
  GtkDrawingArea	darea;

  TeletextToolbar *	toolbar;
  GnomeAppBar *		appbar;

  GtkActionGroup *	action_group; 

  /*< private >*/

  GdkGC	*		xor_gc;			/* gfx context for xor mask */

  int			zvbi_client_id;
  guint			zvbi_timeout_id;

  vbi_page *		fmt_page;		/* current page, formatted */

  gint			page;			/* page we are entering */
  gint			subpage;		/* current subpage */

  gint			monitored_subpage;

  struct {
    GdkBitmap *		  mask;
    gint		  width;
    gint		  height;
  }			scale;

  guint			blink_timeout_id;

  guint32		last_key_press_event_time; /* repeat key kludge */

  gboolean		deferred_load;
  struct {
    guint		  timeout_id;
    vbi_pgno		  pgno;
    vbi_subno		  subno;
    vbi_page		  pg;
  }			deferred;

  struct {
    struct {
      vbi_pgno	    	    pgno;
      vbi_subno	    	    subno;
    }			  stack [25];
    guint		  top;
    guint		  size;
  }			history;

  gboolean		hold;			/* hold the current subpage */
  gboolean		reveal;			/* reveal concealed text */

  gboolean		cursor_over_link;

  gboolean		selecting;
  struct {
    gint		  start_x;
    gint		  start_y;
    gint		  last_x;
    gint		  last_y;
    
    gboolean		  table_mode;

    vbi_page		  page;			/* selected text */

    gint		  column1;		/* selected text */
    gint		  row1;
    gint		  column2;
    gint		  row2;
    
    gboolean		  reveal;		/* at select time */

    						/* selected text "sent" to */
    gboolean		  in_clipboard;		/* X11 "CLIPBOARD" */
    gboolean		  in_selection;		/* GDK primary selection */
  }			select;

  GtkWidget *		search_dialog;
};

struct _TeletextViewClass
{
  GtkDrawingAreaClass	parent_class;
};

extern GType
teletext_view_get_type		(void) G_GNUC_CONST;
GtkWidget *
teletext_view_new		(void);

extern void
teletext_view_vbi_link_from_pointer_position
				(TeletextView *		view,
				 vbi_link *		ld,
				 gint			x,
				 gint			y);
extern GtkWidget *
teletext_view_popup_menu_new	(TeletextView *		view,
				 const vbi_link *	ld,
				 gboolean		large);
extern TeletextView *
teletext_view_from_widget	(GtkWidget *		widget);
extern void
teletext_view_load_page		(TeletextView *		view,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 vbi_page *		pg);
extern gboolean
teletext_view_on_key_press	(GtkWidget *		widget,
				 GdkEventKey *		event,
				 TeletextView *		view);
extern GtkWidget *
bookmarks_menu_new (TeletextView *	view);

G_END_DECLS

#endif /* VIEW_H */
