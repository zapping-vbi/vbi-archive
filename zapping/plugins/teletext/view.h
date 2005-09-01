/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2000, 2001, 2002 Iñaki García Etxebarria
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: view.h,v 1.3 2005-09-01 01:40:53 mschimek Exp $ */

#ifndef TELETEXT_VIEW_H
#define TELETEXT_VIEW_H

#include <gnome.h>
#include "libvbi/page.h"	/* vbi3_page, vbi3_pgno, vbi3_subno */
#include "libvbi/link.h"	/* vbi3_link */
#include "libvbi/vbi_decoder.h"
#include "page_num.h"
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

/**
 * The blink of items in the page is done by applying the patch once
 * every second (whenever the client wishes) to the apropriate places
 * on the screen.
 */
struct ttx_patch {
  guint			column;
  guint			row;
  gint			width, height; /* geometry of the patch */
  gint			sx, sy;
  gint			sw, sh;
  gint			dx, dy;
  GdkPixbuf *		unscaled_on;	/* unscaled image, flash on */
  GdkPixbuf *		unscaled_off;	/* unscaled image, flash off or NULL */
  GdkPixbuf *		scaled_on;	/* scaled image, flash on */
  GdkPixbuf *		scaled_off;	/* scaled image, flash off or NULL */
  guint			columns;	/* text columns covered */
  gint			phase;		/* flash phase */
  gboolean		flash;		/* flashing patch */
  gboolean		dirty;		/* image changed */
};

struct _TeletextView
{
  GtkDrawingArea	darea;

  TeletextToolbar *	toolbar;
  GnomeAppBar *		appbar;

  GtkActionGroup *	action_group; 

  void
  (* show_page)		(TeletextView *		view,
			 vbi3_page *		pg);
  gboolean
  (* load_page)		(TeletextView *		view,
			 const vbi3_network *	nk,
			 vbi3_pgno		pgno,
			 vbi3_subno		subno);
  gboolean
  (* switch_network)	(TeletextView *		view,
			 const vbi3_network *	nk);
  GtkWidget *
  (* popup_menu)	(TeletextView *		view,
			 const vbi3_link *	ld,
			 gboolean		large);
  gboolean
  (* link_from_pointer_position)
			(TeletextView *		view,
			 vbi3_link *		ld,
			 gint			x,
			 gint			y);
  gboolean
  (* set_charset)	(TeletextView *		view,
			 vbi3_charset_code	charset_code);


  /* ugly hack */
  void			(* client_redraw)(TeletextView *	view,
					  unsigned int		width,
					  unsigned int		height);
  gboolean		(* key_press)(TeletextView *	view,
				      GdkEventKey *	event);
  int			(* cur_pgno)(TeletextView *	view);

  /*< private >*/

  vbi3_decoder *	vbi;

  vbi3_pgno		entered_pgno;	/* page number being entered */

  page_num		req;		/* requested page */
  vbi3_charset_code	override_charset;

  vbi3_page *		pg;		/* displayed page (shared, r/o) */

  gboolean		freezed;	/* no refresh (header / subpages) */

  GdkPixbuf *		unscaled_on;	/* unscaled image of pg, flash on */
  GdkPixbuf *		unscaled_off;	/* unscaled image of pg, flash off */
  GdkPixbuf *		scaled_on;	/* scaled image of pg, flash on */

  struct ttx_patch *	patches;	/* patches to be applied */
  guint			n_patches;


  guint			blink_timeout_id;

  guint32		last_key_press_event_time; /* repeat key kludge */

  gboolean		deferred_load;
  struct {
    guint		  timeout_id;
    vbi3_network	  network;
    vbi3_pgno		  pgno;
    vbi3_subno		  subno;
  }			deferred;

  struct {
    page_num		  stack [25];
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
    gboolean		  rtl_mode;

    gboolean		  reveal;		/* at select time */

    vbi3_page *		  pg;			/* selected text */

    gint		  column1;		/* selected text */
    gint		  row1;
    gint		  column2;
    gint		  row2;

    GdkGC *		  xor_gc;		/* gfx context for xor mask */

    						/* selected text "sent" to */
    gboolean		  in_clipboard;		/* X11 "CLIPBOARD" */
    gboolean		  in_selection;		/* GDK primary selection */
  }			select;

  GtkWidget *		search_dialog;
};

struct _TeletextViewClass
{
  GtkDrawingAreaClass	parent_class;

  /* Signals. */

  void (*request_changed)(TeletextView *view);
  void (*charset_changed)(TeletextView *view);
};

extern GType
teletext_view_get_type		(void) G_GNUC_CONST;
GtkWidget *
teletext_view_new		(void);

extern TeletextView *
teletext_view_from_widget	(GtkWidget *		widget);

extern guint
ttxview_hotlist_menu_insert	(GtkMenuShell *		menu,
				 gboolean		separator,
				 gint position);

G_END_DECLS

#endif /* TELETEXT_VIEW_H */
