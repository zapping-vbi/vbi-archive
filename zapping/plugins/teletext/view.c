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

/* $Id: view.c,v 1.2 2004-09-26 13:30:37 mschimek Exp $ */

#include "config.h"

#include <gdk/gdkx.h>

#include "view.h"
#include "export.h"
#include "search.h"
#include "main.h"

#include "subtitle.h"

#include "zvbi.h"
#include "zmisc.h"
#include "frequencies.h"
#include "globals.h"
#include "remote.h"

#define BLINK_CYCLE 300 /* ms */

static GObjectClass *		parent_class;

static GdkCursor *		cursor_normal;
static GdkCursor *		cursor_link;
static GdkCursor *		cursor_select;

static GdkAtom			GA_CLIPBOARD = GDK_NONE;

static __inline__ int
vbi_bcd2dec2			(int			n)
{
  return vbi_bcd2dec ((unsigned int) n);
}

static __inline__ int
vbi_dec2bcd2			(int			n)
{
  return vbi_dec2bcd ((unsigned int) n);
}

static __inline__ int
vbi_add_bcd2			(int			a,
				 int			b)
{
  return vbi_add_bcd ((unsigned int) a, (unsigned int) b);
}


TeletextView *
teletext_view_from_widget	(GtkWidget *		widget)
{
  TeletextView *view;

  while (!(view = (TeletextView *)
	   g_object_get_data (G_OBJECT (widget), "TeletextView")))
    {
      if (!(widget = widget->parent))
	return NULL;
    }

  return view;
}

static void
set_transient_for		(GtkWindow *		window,
				 TeletextView *		view)
{
  GtkWidget *parent;

  parent = GTK_WIDGET (view);

  while (parent)
    {
      if (GTK_IS_WINDOW (parent))
	{
	  gtk_window_set_transient_for (window,	GTK_WINDOW (parent));
	  break;
	}

      parent = parent->parent;
    }
}

/* Why did they omit this? */
static void
action_set_sensitive		(GtkAction *		action,
				 gboolean		sensitive)
{
  g_object_set (G_OBJECT (action), "sensitive", sensitive, NULL);
}

enum {
  TARGET_STRING,
  TARGET_PIXMAP
};

static const GtkTargetEntry
clipboard_targets [] = {
  { "STRING", 0, TARGET_STRING },
  { "PIXMAP", 0, TARGET_PIXMAP },
};

static gint
decimal_subno			(vbi_subno		subno)
{
  if (subno == 0 || (guint) subno > 0x99)
    return -1; /* any */
  else
    return vbi_bcd2dec2 (subno);
}

static void
set_hold			(TeletextView *		view,
				 gboolean		hold)
{
  if (view->toolbar)
    teletext_toolbar_set_hold (view->toolbar, hold);

  if (hold != view->hold)
    {
      vbi_page *pg = view->fmt_page;

      view->hold = hold;

      if (hold)
	teletext_view_load_page (view, pg->pgno, pg->subno, NULL);
      else
	teletext_view_load_page (view, pg->pgno, VBI_ANY_SUBNO, NULL);
    }
}

static PyObject *
py_ttx_hold			(PyObject *		self _unused_,
				 PyObject *		args)
{
  TeletextView *view;
  gint hold;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  hold = -1;

  if (!ParseTuple (args, "|i", &hold))
    g_error ("zapping.ttx_hold(|i)");

  if (hold < 0)
    hold = !view->hold;
  else
    hold = !!hold;

  set_hold (view, hold);

  py_return_true;
}

static PyObject *
py_ttx_reveal			(PyObject *		self _unused_,
				 PyObject *		args)
{
  TeletextView *view;
  gint reveal;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  reveal = -1;

  if (!ParseTuple (args, "|i", &reveal))
    g_error ("zapping.ttx_reveal(|i)");

  if (reveal < 0)
    reveal = !view->reveal;
  else
    reveal = !!reveal;

  if (view->toolbar)
    teletext_toolbar_set_reveal (view->toolbar, reveal);

  set_ttx_parameters (view->zvbi_client_id, reveal);

  if (view->page >= 0x100)
    teletext_view_load_page (view, view->page, view->subpage, NULL);

  py_return_true;
}

static gboolean
deferred_load_timeout		(gpointer		user_data)
{
  TeletextView *view = user_data;

  view->deferred.timeout_id = NO_SOURCE_ID;

  if (view->deferred.pgno)
    monitor_ttx_page (view->zvbi_client_id,
		      view->deferred.pgno,
		      view->deferred.subno);
  else
    monitor_ttx_this (view->zvbi_client_id,
		      &view->deferred.pg);

  view->deferred_load = FALSE;

  return FALSE; /* don't call again */
}

void
teletext_view_load_page		(TeletextView *		view,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 vbi_page *		pg)
{
  gchar *buffer;

  set_hold (view, view->hold = (subno != VBI_ANY_SUBNO));

  view->subpage = subno;
  view->page = pgno;
  view->monitored_subpage = subno;

  if (view->toolbar)
    teletext_toolbar_set_url (view->toolbar, pgno, subno);

  if (pgno >= 0x100 && pgno <= 0x999 /* 0x9nn == top index */)
    {
      if (0 == subno || VBI_ANY_SUBNO == subno)
	buffer = g_strdup_printf (_("Loading page %x..."), pgno);
      else
	buffer = g_strdup_printf (_("Loading page %x.%02x..."),
				  pgno, subno & 0x7F);
    }
  else
    {
      buffer = g_strdup_printf ("Invalid page %x.%x", pgno, subno);
    }

  if (view->appbar)
    gnome_appbar_set_status (view->appbar, buffer);

  g_free (buffer);

  gtk_widget_grab_focus (GTK_WIDGET (view));

  if (pg)
    {
      memcpy (&view->deferred.pg, pg, sizeof (view->deferred.pg));
      view->deferred.pgno = 0;
    }
  else
    {
      view->deferred.pgno = pgno;
      view->deferred.subno = subno;
    }

  if (view->deferred.timeout_id > 0)
    g_source_remove (view->deferred.timeout_id);

  if (view->deferred_load)
    {
      view->deferred.timeout_id =
	g_timeout_add (300, (GSourceFunc) deferred_load_timeout, view);
    }
  else
    {
      view->deferred.timeout_id = NO_SOURCE_ID;

      if (pg)
	monitor_ttx_this (view->zvbi_client_id,
			  &view->deferred.pg);
      else
	monitor_ttx_page (view->zvbi_client_id,
			  pgno, subno);
    }

  z_update_gui ();
}

void
teletext_view_vbi_link_from_pointer_position
				(TeletextView *		view,
				 vbi_link *		ld,
				 gint			x,
				 gint			y)
{
  gint width;
  gint height;
  gint row;
  gint column;
  vbi_char *ac;

  ld->type = VBI_LINK_NONE;

  if (x < 0 || y < 0)
    return;

  gdk_window_get_geometry (GTK_WIDGET (view)->window,
			   /* x */ NULL,
			   /* y */ NULL,
			   &width,
			   &height,
			   /* depth */ NULL);

  if (width <= 0 || height <= 0)
    return;

  column = (x * view->fmt_page->columns) / width;
  row = (y * view->fmt_page->rows) / height;

  if (column >= view->fmt_page->columns
      || row >= view->fmt_page->rows)
    return;

  ac = view->fmt_page->text
    + row * view->fmt_page->columns
    + column;

  if (ac->link)
    vbi_resolve_link (view->fmt_page, column, row, ld);
}

static void
update_pointer			(TeletextView *		view)
{
  gint x;
  gint y;
  GdkModifierType mask;
  vbi_link ld;
  gchar *buffer;

  gdk_window_get_pointer (GTK_WIDGET (view)->window, &x, &y, &mask);

  teletext_view_vbi_link_from_pointer_position (view, &ld, x, y);

  switch (ld.type)
    {
    case VBI_LINK_PAGE:
      buffer = g_strdup_printf (_(" Page %x"), ld.pgno);
      goto show;

    case VBI_LINK_SUBPAGE:
      buffer = g_strdup_printf (_(" Subpage %x"), ld.subno & 0xFF);
      goto show;

    case VBI_LINK_HTTP:
    case VBI_LINK_FTP:
    case VBI_LINK_EMAIL:
      buffer = g_strconcat (" ", ld.url, NULL);
      goto show;

    show:
      if (!view->cursor_over_link)
	{
	  view->cursor_over_link = TRUE;

	  if (view->appbar)
	    gnome_appbar_push (GNOME_APPBAR (view->appbar), buffer);

	  gdk_window_set_cursor (GTK_WIDGET (view)->window, cursor_link);
	}
      else
	{
	  if (view->appbar)
	    gnome_appbar_set_status (GNOME_APPBAR (view->appbar), buffer);
	}

      g_free (buffer);

      break;

    default:
      if (view->cursor_over_link)
	{
	  view->cursor_over_link = FALSE;

	  if (view->appbar)
	    gnome_appbar_pop (GNOME_APPBAR (view->appbar));

	  gdk_window_set_cursor (GTK_WIDGET (view)->window, cursor_normal);
	}

      break;
    }
}

static
void scale_image			(GtkWidget	*wid _unused_,
					 gint		w,
					 gint		h,
					 TeletextView	*view)
{
  if (view->scale.width != w
      || view->scale.height != h)
    {
      if (view->scale.mask)
	g_object_unref (G_OBJECT (view->scale.mask));

      view->scale.mask =
	gdk_pixmap_new (GTK_WIDGET (view)->window, w, h, 1);

      g_assert (view->scale.mask != NULL);

      resize_ttx_page(view->zvbi_client_id, w, h);

      view->scale.width = w;
      view->scale.height = h;
    }
}


/*
 *  Page commands
 */

static PyObject *
py_ttx_open			(PyObject *		self _unused_,
				 PyObject *		args)
{
  TeletextView *view;
  int page;
  int subpage;
  vbi_pgno pgno;
  vbi_subno subno;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  page = 100;
  subpage = -1;

  if (!ParseTuple (args, "|ii", &page, &subpage))
    g_error ("zapping.ttx_open_new(|ii)");

  if (page >= 100 && page <= 899)
    pgno = vbi_dec2bcd2 (page);
  else
    py_return_false;

  if (subpage < 0)
    subno = VBI_ANY_SUBNO;
  else if ((guint) subpage <= 99)
    subno = vbi_dec2bcd2 (subpage);
  else
    py_return_false;

  teletext_view_load_page (view, pgno, subno, NULL);

  py_return_true;
}

static PyObject *
py_ttx_page_incr		(PyObject *		self _unused_,
				 PyObject *		args)
{
  TeletextView *view;
  vbi_pgno pgno;
  gint value;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  value = +1;

  if (!ParseTuple (args, "|i", &value))
    g_error ("zapping.ttx_page_incr(|i)");

  if (abs (value) > 999)
    py_return_false;

  if (value < 0)
    value += 1000;

  pgno = vbi_add_bcd2 (view->page, vbi_dec2bcd2 (value)) & 0xFFF;

  if (pgno < 0x100)
    pgno = 0x800 + (pgno & 0xFF);
  else if (pgno > 0x899)
    pgno = 0x100 + (pgno & 0xFF);

  teletext_view_load_page (view, pgno, VBI_ANY_SUBNO, NULL);

  py_return_true;
}

static PyObject *
py_ttx_subpage_incr		(PyObject *		self _unused_,
				 PyObject *		args)
{
  TeletextView *view;
  vbi_subno subno;
  gint value;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  value = +1;

  if (!ParseTuple (args, "|i", &value))
    g_error ("zapping.ttx_subpage_incr(|i)");

  if (abs (value) > 99)
    py_return_false;

  if (value < 0)
    value += 100; /* XXX should use actual or anounced number of subp */

  subno = vbi_add_bcd2 (view->subpage, vbi_dec2bcd2 (value)) & 0xFF;

  teletext_view_load_page (view, view->fmt_page->pgno, subno, NULL);

  py_return_true;
}

static vbi_pgno
default_home_pgno		(void)
{
  gint value;

  value = gconf_client_get_int (gconf_client,
				"/apps/zapping/plugins/teletext/home_page",
				/* err */ NULL);

  if (value < 100 || value > 899)
    {
      /* Error, unset or invalid. */
      value = 100;
    }

  return vbi_dec2bcd2 (value);
}

static void
home_action			(GtkAction *		action _unused_,
				 TeletextView *		view)
{
  vbi_link ld;

  vbi_resolve_home (view->fmt_page, &ld);

  switch (ld.type)
    {
    case VBI_LINK_PAGE:
    case VBI_LINK_SUBPAGE:
      if (ld.pgno)
	{
	  teletext_view_load_page (view, ld.pgno, ld.subno, NULL);
	}
      else
	{
	  teletext_view_load_page (view,
				   default_home_pgno (),
				   VBI_ANY_SUBNO,
				   NULL);
	}

      break;

    default:
      break;
    }
}

static PyObject *
py_ttx_home			(PyObject *		self _unused_,
				 PyObject *		args _unused_)
{
  TeletextView *view;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  home_action (NULL, view);

  py_return_true;
}

/*
	Selection
*/

/* Called when another application claims the selection
   (i.e. sends new data to the clipboard). */
static gint
on_selection_clear		(GtkWidget *		widget _unused_,
				 GdkEventSelection *	event,
				 TeletextView *		view)
{
  if (event->selection == GDK_SELECTION_PRIMARY)
    view->select.in_selection = FALSE;
  else if (event->selection == GA_CLIPBOARD)
    view->select.in_clipboard = FALSE;

  return TRUE;
}

/* Called when another application requests our selected data. */
static void
on_selection_get		(GtkWidget *		widget _unused_,
				 GtkSelectionData *	selection_data,
				 guint			info,
				 guint			time_stamp _unused_,
				 TeletextView *		view)
{
  if ((selection_data->selection == GDK_SELECTION_PRIMARY
       && view->select.in_selection)
      || (selection_data->selection == GA_CLIPBOARD
	  && view->select.in_clipboard))
    {
      switch (info)
	{
	case TARGET_STRING:
	  {
	    int width;
	    int height;
	    int actual;
	    unsigned int size;
	    char *buf;

	    width = view->select.column2 - view->select.column1 + 1;
	    height = view->select.row2 - view->select.row1 + 1;

	    size = 25 * 48;
	    actual = 0;

	    if ((buf = malloc (size)))
	      {
		/* XXX According to ICCC Manual 2.0 the STRING target
		   uses encoding ISO Latin-1. How can we use UTF-8? */

#if VBI_VERSION_MAJOR >= 1
		actual = vbi_print_page_region (&view->select.page,
						buf, size, "ISO-8859-1",
						NULL, 0, /* std separator */
						view->select.table_mode,
						/* rtl */ FALSE,
						view->select.column1,
						view->select.row1,
						width,
						height);
#else
		actual = vbi_print_page_region (&view->select.page,
						buf, (int) size, "ISO-8859-1",
						view->select.table_mode,
						/* ltr */ TRUE,
						view->select.column1,
						view->select.row1,
						width,
						height);
#endif
		if (actual > 0)
		  gtk_selection_data_set (selection_data,
					  GDK_SELECTION_TYPE_STRING, 8,
					  buf, actual);

		free (buf);
	      }

	    if (actual <= 0)
	      g_warning (_("Text export failed"));

	    break;
	  }

	case TARGET_PIXMAP:
	  {
	    const gint CW = 12; /* Teletext character cell size */
	    const gint CH = 10;
	    gint width;
	    gint height;
	    GdkPixmap *pixmap;
	    GdkPixbuf *canvas;
	    gint id[2];

	    /* XXX Selection is open (eg. 25,5 - 15,6),
	       ok to simply bail out? */
	    if (view->select.column2 < view->select.column1)
	      break;

	    width = view->select.column2 - view->select.column1 + 1;
	    height = view->select.row2 - view->select.row1 + 1;

	    pixmap = gdk_pixmap_new (GTK_WIDGET (view)->window,
				     width * CW,
				     height * CH,
				     -1 /* same depth as window */);

	    canvas = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				     TRUE, 8,
				     width * CW,
				     height * CH);

	    vbi_draw_vt_page_region (&view->select.page,
				     VBI_PIXFMT_RGBA32_LE,
				     (uint32_t *) gdk_pixbuf_get_pixels (canvas),
				     view->select.column1,
				     view->select.row1,
				     width,
				     height,
				     gdk_pixbuf_get_rowstride (canvas),
				     view->select.reveal,
				     /* flash_on */ TRUE);

	    gdk_pixbuf_render_to_drawable (canvas,
					   pixmap,
					   GTK_WIDGET (view)->style->white_gc,
					   0, 0, 0, 0,
					   width * CW,
					   height * CH,
					   GDK_RGB_DITHER_NORMAL,
					   0, 0);

	    id[0] = GDK_WINDOW_XWINDOW (pixmap);

	    gtk_selection_data_set (selection_data,
				    GDK_SELECTION_TYPE_PIXMAP, 32,
				    (char * ) id, 4);
	
	    g_object_unref(canvas);

	    break;
	  }

	default:
	  break;
	}
    }
}

static __inline__ void
select_positions		(TeletextView *		view,
				 gint			x,
				 gint			y,
				 gint *			pcols,
				 gint *			prows,
				 gint *			scol,
				 gint *			srow,
				 gint *			ccol,
				 gint *			crow)
{
  gint width, height;	/* window */
  gint columns, rows;	/* page */

  gdk_window_get_geometry (GTK_WIDGET (view)->window,
			   NULL, NULL,
			   &width, &height,
			   NULL);

  *pcols = columns = view->fmt_page->columns;
  *prows = rows = view->fmt_page->rows;

  *scol = SATURATE ((view->select.start_x * columns) / width, 0, columns - 1);
  *srow = SATURATE ((view->select.start_y * rows) / height, 0, rows - 1);

  *ccol = SATURATE ((x * columns) / width, 0, columns - 1);
  *crow = SATURATE ((y * rows) / height, 0, rows - 1);
}

static __inline__ gboolean
is_hidden_row			(TeletextView *		view,
				 gint			row)
{
  if (row <= 0 || row >= 25)
    return FALSE;

  return !!(view->fmt_page->double_height_lower & (1 << row));
}

/* Calculates a rectangle, scaling from text coordinates
   to window coordinates. */
static void
scale_rect			(GdkRectangle *		rect,
				 gint			x1,
				 gint			y1,
				 gint			x2,
				 gint			y2,
				 gint			width,
				 gint			height,
				 gint			rows,
				 gint			columns)
{
  gint ch = columns >> 1;
  gint rh = rows >> 1;

  rect->x = (x1 * width + ch) / columns;
  rect->y = (y1 * height + rh) / rows;
  rect->width = ((x2 + 1) * width + ch) / columns - rect->x;
  rect->height = ((y2 + 1) * height + rh) / rows - rect->y;
}

/* Transforms the selected region from the first rectangle to the
   second one, given in text coordinates. */
static void
select_transform		(TeletextView *		view,
				 gint			sx1,
				 gint			sy1,
				 gint			sx2,
				 gint			sy2,
				 gboolean		stable,
				 gint			dx1,
				 gint			dy1,
				 gint			dx2,
				 gint			dy2,
				 gboolean		dtable,
				 GdkRegion *		exposed)
{
  gint width, height; /* window */
  gint columns, rows; /* page */
  GdkRectangle rect;
  GdkRegion *src_region;
  GdkRegion *dst_region;

  gdk_window_get_geometry (GTK_WIDGET (view)->window,
			   NULL, NULL,
			   &width, &height,
			   NULL);

  columns = view->fmt_page->columns;
  rows = view->fmt_page->rows;

  gdk_gc_set_clip_origin (view->xor_gc, 0, 0);

  {
    gboolean h1, h2, h3, h4;

    if (sy1 > sy2)
      {
	SWAP (sx1, sx2);
	SWAP (sy1, sy2);
      }

    h1 = is_hidden_row (view, sy1);
    h2 = is_hidden_row (view, sy1 + 1);
    h3 = is_hidden_row (view, sy2);
    h4 = is_hidden_row (view, sy2 + 1);

    if (stable || sy1 == sy2 || ((sy2 - sy1) == 1 && h3))
      {
	if (sx1 > sx2)
	  SWAP (sx1, sx2);

	scale_rect (&rect,
		    sx1, sy1 - h1,
		    sx2, sy2 + h4,
		    width, height,
		    rows, columns);
	src_region = gdk_region_rectangle (&rect);
      }
    else
      {
	scale_rect (&rect,
		    sx1, sy1 - h1,
		    columns - 1, sy1 + h2,
		    width, height,
		    rows, columns);
	src_region = gdk_region_rectangle (&rect);

	scale_rect (&rect,
		    0, sy2 - h3,
		    sx2, sy2 + h4,
		    width, height,
		    rows, columns);
	gdk_region_union_with_rect (src_region, &rect);

	sy1 += h2 + 1;
	sy2 -= h3 + 1;

	if (sy2 >= sy1)
	  {
	    scale_rect (&rect,
			0, sy1,
			columns - 1, sy2,
			width, height,
			rows, columns);
	    gdk_region_union_with_rect (src_region, &rect);
	  }
      }
  }

  {
    gboolean h1, h2, h3, h4;

    if (dy1 > dy2)
      {
	SWAP(dx1, dx2);
	SWAP(dy1, dy2);
      }

    h1 = is_hidden_row (view, dy1);
    h2 = is_hidden_row (view, dy1 + 1);
    h3 = is_hidden_row (view, dy2);
    h4 = is_hidden_row (view, dy2 + 1);

    if (dtable || dy1 == dy2 || ((dy2 - dy1) == 1 && h3))
      {
	if (dx1 > dx2)
	  SWAP(dx1, dx2);

	view->select.column1 = dx1;
	view->select.row1 = dy1 -= h1;
	view->select.column2 = dx2;
	view->select.row2 = dy2 += h4;

	scale_rect (&rect,
		    dx1, dy1,
		    dx2, dy2,
		    width, height,
		    rows, columns);
	dst_region = gdk_region_rectangle (&rect);
      }
    else
      {
	scale_rect (&rect,
		    dx1, dy1 - h1,
		    columns - 1, dy1 + h2,
		    width, height,
		    rows, columns);
	dst_region = gdk_region_rectangle (&rect);

	scale_rect (&rect,
		    0, dy2 - h3,
		    dx2, dy2 + h4,
		    width, height,
		    rows, columns);
	gdk_region_union_with_rect (dst_region, &rect);

	view->select.column1 = dx1;
	view->select.row1 = dy1 + h2;
	view->select.column2 = dx2;
	view->select.row2 = dy2 - h3;

	dy1 += h2 + 1;
	dy2 -= h3 + 1;

	if (dy2 >= dy1)
	  {
	    scale_rect (&rect,
			0, dy1,
			columns - 1, dy2,
			width, height,
			rows, columns);
	    gdk_region_union_with_rect (dst_region, &rect);
	  }
      }
  }

  if (exposed)
    gdk_region_subtract (src_region, exposed);

  gdk_region_xor (src_region, dst_region);

  gdk_region_destroy (dst_region);

  gdk_gc_set_clip_region (view->xor_gc, src_region);

  gdk_region_destroy (src_region);

  gdk_draw_rectangle (GTK_WIDGET (view)->window,
		      view->xor_gc,
		      TRUE,
		      0, 0,
		      width - 1, height - 1);

  gdk_gc_set_clip_rectangle (view->xor_gc, NULL);
}

static void
select_stop			(TeletextView *		view)
{
  if (view->appbar)
    gnome_appbar_pop (view->appbar);

  if (view->select.last_x != -1)
    {
      gint columns, rows; /* page */
      gint scol, srow; /* start */
      gint ccol, crow; /* current */
      gint tcol1, trow1;
      gint tcol2, trow2;

      select_positions (view,
			view->select.last_x,
			view->select.last_y,
			&columns, &rows,
			&scol, &srow,
			&ccol, &crow);

      /* Save because select_transform() will reset. */
      tcol1 = view->select.column1;
      trow1 = view->select.row1;
      tcol2 = view->select.column2;
      trow2 = view->select.row2;

      select_transform (view,
			scol, srow,	/* src1 */
			ccol, crow,	/* src2 */
			view->select.table_mode,
			columns, rows,	/* dst1 */
			columns, rows,	/* dst2 */
			view->select.table_mode,
			NULL);

      /* Copy selected text. */

      view->select.page    = *view->fmt_page;

      view->select.column1 = tcol1;
      view->select.row1    = trow1;
      view->select.column2 = tcol2;
      view->select.row2    = trow2;

      view->select.reveal  = view->reveal;

      if (!view->select.in_clipboard)
	if (gtk_selection_owner_set (GTK_WIDGET (view),
				     GA_CLIPBOARD,
				     GDK_CURRENT_TIME))
	  view->select.in_clipboard = TRUE;

      if (!view->select.in_selection)
	if (gtk_selection_owner_set (GTK_WIDGET (view),
				     GDK_SELECTION_PRIMARY,
				     GDK_CURRENT_TIME))
	  view->select.in_selection = TRUE;

      if (view->appbar)
	gnome_appbar_set_status (view->appbar,
				 _("Selection copied to clipboard"));
    }

  ttx_unfreeze (view->zvbi_client_id);

  update_pointer (view);

  view->selecting = FALSE;
}

static void
select_update			(TeletextView *		view,
				 gint			x,
				 gint			y,
				 guint			state)
{
  gint columns, rows; /* page */
  gint scol, srow; /* start */
  gint ocol, orow; /* last */
  gint ccol, crow; /* current */
  gboolean table;

  select_positions (view,
		    x, y,
		    &columns, &rows,
		    &scol, &srow,
		    &ccol, &crow);

  table = !!(state & GDK_SHIFT_MASK);

  if (view->select.last_x == -1)
    {
      /* First motion. */
      select_transform (view,
			columns, rows,	/* src1 */
			columns, rows,	/* src2 */
			view->select.table_mode,
			scol, srow,	/* dst1 */
			ccol, crow,	/* dst2 */
			table,
			NULL);
    }
  else
    {
      gint width, height;

      gdk_window_get_geometry (GTK_WIDGET (view)->window,
			       NULL, NULL,
			       &width, &height,
			       NULL);

      ocol = (view->select.last_x * columns) / width;
      ocol = SATURATE (ocol, 0, columns - 1);
      orow = (view->select.last_y * rows) / height;
      orow = SATURATE (orow, 0, rows - 1);

      select_transform (view,
			scol, srow,	/* src1 */	
			ocol, orow,	/* src2 */
			view->select.table_mode,
			scol, srow,	/* dst1 */
			ccol, crow,	/* dst2 */
			table,
			NULL);
    }

  view->select.last_x = MAX (0, x);
  view->select.last_y = y;

  view->select.table_mode = table;
}

static void
select_start			(TeletextView *		view,
				 gint			x,
				 gint			y,
				 guint			state)
{
  if (view->selecting)
    return;

  if (view->fmt_page->pgno < 0x100)
    {
      if (view->appbar)
  	gnome_appbar_set_status (view->appbar, _("No page loaded"));
      return;
    }

  if (view->cursor_over_link)
    {
      view->cursor_over_link = FALSE;
      if (view->appbar)
	gnome_appbar_pop (view->appbar);
    }

  if (view->appbar)
    gnome_appbar_push (view->appbar,
		       _("Selecting - press Shift key for table mode"));

  gdk_window_set_cursor (GTK_WIDGET (view)->window, cursor_select);

  view->select.start_x = x;
  view->select.start_y = y;

  view->select.last_x = -1; /* not yet, wait for move event */

  view->select.table_mode = !!(state & GDK_SHIFT_MASK);

  view->selecting = TRUE;

  ttx_freeze (view->zvbi_client_id);
}

static void
export_action			(GtkAction *		action _unused_,
				 TeletextView *		view)
{
  GtkWidget *dialog;
  vbi_network network;

  if (!view->fmt_page
      || view->fmt_page->pgno < 0x100)
    {
      /* XXX make save option insensitive instead */
      if (view->appbar)
	gnome_appbar_set_status (view->appbar, _("No page loaded"));
      return;
    }

  {
    extern vbi_network current_network; /* FIXME */
    guint i;

    network = current_network;

    if (!network.name[0])
      g_strlcpy (network.name, "Zapzilla", sizeof (network.name));

    for (i = 0; i < strlen (network.name); ++i)
      if (!g_ascii_isalnum (network.name[i]))
	network.name[i] = '_';
  }

  if ((dialog = export_dialog_new (view->fmt_page,
				   network.name, view->reveal)))
    {
      set_transient_for (GTK_WINDOW (dialog), view);
      gtk_widget_show_all (dialog);
    }
}

static PyObject *
py_ttx_export			(PyObject *		self _unused_,
				 PyObject *		args _unused_)
{
  TeletextView *view;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  export_action (NULL, view);

  py_return_true;
}

static void
search_action			(GtkAction *		action _unused_,
				 TeletextView *		view)
{
  GtkWidget *widget;

  if (view->search_dialog)
    {
      gtk_window_present (GTK_WINDOW (view->search_dialog));
    }
  else if ((widget = search_dialog_new (view)))
    {
      view->search_dialog = widget;
      g_signal_connect (G_OBJECT (widget), "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&view->search_dialog);

      set_transient_for (GTK_WINDOW (widget), view);

      gtk_widget_show_all (widget);
    }
}

static PyObject *
py_ttx_search			(PyObject *		self _unused_,
				 PyObject *		args _unused_)
{
  TeletextView *view;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  search_action (NULL, view);

  py_return_true;
}

/*
	Browser history
*/

/* Note history.stack[top - 1] is the current page. */

static void
history_dump			(TeletextView *		view)
{
  guint i;

  fprintf (stderr, "top=%u size=%u ",
	   view->history.top,
	   view->history.size);

  for (i = 0; i < view->history.size; ++i)
    fprintf (stderr, "%03x ",
	     view->history.stack[i].pgno);

  fputc ('\n', stderr);
}

static void
history_update_gui		(TeletextView *		view)
{
  GtkAction *action;

  action = gtk_action_group_get_action (view->action_group, "HistoryBack");
  action_set_sensitive (action, view->history.top >= 2);

  action = gtk_action_group_get_action (view->action_group, "HistoryForward");
  action_set_sensitive (action, view->history.top < view->history.size);
}

static __inline__ gboolean
same_page			(vbi_pgno		pgno1,
				 vbi_subno		subno1,
				 vbi_pgno		pgno2,
				 vbi_subno		subno2)
{
  return (pgno1 == pgno2
	  && (subno1 == subno2
	      || VBI_ANY_SUBNO == subno1
	      || VBI_ANY_SUBNO == subno2));
}

/* Push *current page* onto history stack. Actually one should push
   the page being replaced, but things are simpler this way. */
static void
history_push			(TeletextView *		view,
				 vbi_pgno		pgno,
				 vbi_subno		subno)
{
  guint top;

  top = view->history.top;

  if (pgno < 0x100 || pgno > 0x899) 
    return;

  if (top > 0)
    {
      if (same_page (view->history.stack[top - 1].pgno,
		     view->history.stack[top - 1].subno,
		     pgno, subno))
	  return; /* already stacked */

      if (top >= N_ELEMENTS (view->history.stack))
	{
	  top = N_ELEMENTS (view->history.stack) - 1;

	  memmove (view->history.stack,
		   view->history.stack + 1,
		   top * sizeof (*view->history.stack));
	}
      else if (view->history.size > top)
	{
	  if (same_page (view->history.stack[top].pgno,
			 view->history.stack[top].subno,
			 pgno, subno))
	    {
	      /* Apparently we move forward in history, no new page. */
	      view->history.top = top + 1;
	      history_update_gui (view);
	      return;
	    }

	  /* Will discard future branch. */
	}
    }

  view->history.stack[top].pgno = pgno;
  view->history.stack[top].subno = subno;

  ++top;

  view->history.top = top;
  view->history.size = top;

  history_update_gui (view);

  if (0)
    history_dump (view);
}

static void
history_back_action		(GtkAction *		action _unused_,
				 TeletextView *		view)
{
  guint top;

  top = view->history.top;

  if (top >= 2)
    {
      view->history.top = top - 1;
      history_update_gui (view);

      teletext_view_load_page (view,
			       view->history.stack[top - 2].pgno,
			       view->history.stack[top - 2].subno,
			       NULL);
    }
}

static PyObject *
py_ttx_history_prev		(PyObject *		self _unused_,
				 PyObject *		args _unused_)
{
  TeletextView *view;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  history_back_action (NULL, view);

  py_return_true;
}

static void
history_forward_action		(GtkAction *		action _unused_,
				 TeletextView *		view)
{
  guint top;

  top = view->history.top;

  if (top < view->history.size)
    {
      view->history.top = top + 1;
      history_update_gui (view);

      teletext_view_load_page (view,
			       view->history.stack[top].pgno,
			       view->history.stack[top].subno,
			       NULL);
    }
}

static PyObject *
py_ttx_history_next		(PyObject *		self _unused_,
				 PyObject *		args _unused_)
{
  TeletextView *view;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  history_forward_action (NULL, view);

  py_return_true;
}



/*
	Popup Menu
*/

#define ONCE(b) if (b) continue; else b = TRUE;

guint
ttxview_hotlist_menu_insert	(GtkMenuShell *		menu,
				 gboolean		separator,
				 gint			position)
{
  vbi_decoder *vbi = zvbi_get_object ();
  vbi_pgno pgno;
  guint count;
  gboolean have_subtitle_index = FALSE;
  gboolean have_now_and_next   = FALSE;
  gboolean have_current_progr  = FALSE;
  gboolean have_progr_index    = FALSE;
  gboolean have_progr_schedule = FALSE;
  gboolean have_progr_warning  = FALSE;

  count = 0;

  for (pgno = 0x100; pgno <= 0x899;
       pgno = vbi_add_bcd2 (pgno, 0x001))
    {
      vbi_page_type classf;
      gchar *buffer;
      GtkWidget *menu_item;
      gboolean new_window;

      classf = vbi_classify_page (vbi, pgno, NULL, NULL);

      /* XXX should be sorted: index, schedule, current, n&n, warning. */

      switch (classf)
	{
	case VBI_SUBTITLE_INDEX:
	  ONCE (have_subtitle_index);
	  menu_item = z_gtk_pixmap_menu_item_new
	    (_("Subtitle index"), GTK_STOCK_INDEX);
	  new_window = TRUE;
	  break;

	case VBI_NOW_AND_NEXT:
	  ONCE (have_now_and_next);
	  menu_item = z_gtk_pixmap_menu_item_new
	    (_("Now and Next"), GTK_STOCK_JUSTIFY_FILL);
	  new_window = FALSE;
	  break;

	case VBI_CURRENT_PROGR:
	  ONCE (have_current_progr);
	  menu_item = z_gtk_pixmap_menu_item_new
	    (_("Current program"), GTK_STOCK_JUSTIFY_FILL);
	  new_window = TRUE;
	  break;

	case VBI_PROGR_INDEX:
	  ONCE (have_progr_index);
	  menu_item = z_gtk_pixmap_menu_item_new
	    (_("Program Index"), GTK_STOCK_INDEX);
	  new_window = TRUE;
	  break;

	case VBI_PROGR_SCHEDULE:
	  ONCE (have_progr_schedule);
	  menu_item = z_gtk_pixmap_menu_item_new
	    (_("Program Schedule"), "gnome-stock-timer");
	  new_window = TRUE;
	  break;

	case VBI_PROGR_WARNING:
	  ONCE (have_progr_warning);
	  /* TRANSLATORS: Schedule changes and the like. */
	  menu_item = z_gtk_pixmap_menu_item_new
	    (_("Program Warning"), "gnome-stock-mail");
	  new_window = FALSE;
	  break;

	default:
	  continue;
	}

      if (separator)
	{
	  GtkWidget *menu_item;

	  menu_item = gtk_separator_menu_item_new ();
	  gtk_widget_show (menu_item);
	  gtk_menu_shell_insert (menu, menu_item, position);
	  if (position > 0)
	    ++position;

	  separator = FALSE;
	}

      gtk_widget_show (menu_item);

      {
	gchar buffer [32];
	
	g_snprintf (buffer, sizeof (buffer), "%x", pgno);
	z_tooltip_set (menu_item, buffer);
      }

      if (new_window)
	buffer = g_strdup_printf ("zapping.ttx_open_new(%x, -1)", pgno);
      else
	buffer = g_strdup_printf ("zapping.ttx_open(%x, -1)", pgno);

      g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (on_python_command1), buffer);
      g_signal_connect_swapped (G_OBJECT (menu_item), "destroy",
				G_CALLBACK (g_free), buffer);

      gtk_menu_shell_insert (menu, menu_item, position);
      if (position > 0)
	++position;

      ++count;
    }

  return count;
}


static GnomeUIInfo
popup_open_page_uiinfo [] = {
  {
    GNOME_APP_UI_ITEM, N_("_Open"), NULL,
    G_CALLBACK (on_python_command1), NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Open in _New Window"), NULL, 
    G_CALLBACK (on_python_command1), NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo
popup_open_url_uiinfo [] = {
  {
    GNOME_APP_UI_ITEM, N_("_Open Link"), NULL,
    G_CALLBACK (on_python_command1), NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo
popup_page_uiinfo [] = {
  {
    GNOME_APP_UI_ITEM, N_("_New Window"), NULL, 
    G_CALLBACK (on_python_command1), "zapping.ttx_open_new()", NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_NEW,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Save as..."), NULL,
    G_CALLBACK (on_python_command1), "zapping.ttx_export()", NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_SAVE_AS,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("S_earch..."), NULL, 
    G_CALLBACK (on_python_command1), "zapping.ttx_search()", NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_FIND,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Colors..."), NULL,
    G_CALLBACK (on_python_command1), "zapping.ttx_color()", NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("S_ubtitles"), NULL,
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_INDEX,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Bookmarks"), NULL,
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_INDEX,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

GtkWidget *
teletext_view_popup_menu_new	(TeletextView *		view,
				 const vbi_link *	ld,
				 gboolean		large)
{
  GtkWidget *menu;
  GtkWidget *widget;
  gint pos;

  menu = gtk_menu_new ();
  g_object_set_data (G_OBJECT (menu), "TeletextView", view);

  pos = 0;

  if (ld)
    {
      switch (ld->type)
	{
	  gint subpage;

	case VBI_LINK_PAGE:
	case VBI_LINK_SUBPAGE:
	  subpage = decimal_subno (ld->subno);

	  popup_open_page_uiinfo[0].user_data =
	    g_strdup_printf ("zapping.ttx_open(%x, %d)",
			     ld->pgno, subpage);
	  g_signal_connect_swapped (G_OBJECT (menu), "destroy",
				    G_CALLBACK (g_free),
				    popup_open_page_uiinfo[0].user_data);

	  popup_open_page_uiinfo[1].user_data =
	    g_strdup_printf ("zapping.ttx_open_new(%x, %d)",
			     ld->pgno, subpage);
	  g_signal_connect_swapped (G_OBJECT (menu), "destroy",
				    G_CALLBACK (g_free),
				    popup_open_page_uiinfo[1].user_data);

	  gnome_app_fill_menu (GTK_MENU_SHELL (menu),
			       popup_open_page_uiinfo,
			       /* accel */ NULL,
			       /* mnemo */ TRUE,
			       pos);
	  return menu;

	case VBI_LINK_HTTP:
	case VBI_LINK_FTP:
	case VBI_LINK_EMAIL:
	  break; /* FIXME BUG */
	  popup_open_url_uiinfo[0].user_data = g_strdup (ld->url);
	  g_signal_connect_swapped (G_OBJECT (menu), "destroy",
				    G_CALLBACK (g_free),
				    popup_open_url_uiinfo[0].user_data);

	  gnome_app_fill_menu (GTK_MENU_SHELL (menu), popup_open_url_uiinfo,
			       /* accel */ NULL, /* mnemo */ TRUE, pos);
	  return menu;

	default:
	  break;
	}
    }

  popup_page_uiinfo[1].user_data = view; /* save as */
  popup_page_uiinfo[3].user_data = view; /* color */

  gnome_app_fill_menu (GTK_MENU_SHELL (menu), popup_page_uiinfo,
		       NULL, TRUE, pos);

  if (!vbi_export_info_enum (0))
    gtk_widget_set_sensitive (popup_page_uiinfo[1].widget, FALSE);

  if (large)
    {
      GtkWidget *subtitles_menu;

      widget = popup_page_uiinfo[4].widget; /* subtitles */

      if ((subtitles_menu = subtitles_menu_new ()))
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (widget), subtitles_menu);
      else
	gtk_widget_set_sensitive (widget, FALSE);

      widget = popup_page_uiinfo[5].widget; /* bookmarks */

      gtk_menu_item_set_submenu (GTK_MENU_ITEM (widget),
				 bookmarks_menu_new (view));

      ttxview_hotlist_menu_insert (GTK_MENU_SHELL (menu),
				   /* separator */ TRUE, APPEND);
    }
  else
    {
      widget = popup_page_uiinfo[4].widget; /* subtitles */
      gtk_widget_set_sensitive (widget, FALSE);
      gtk_widget_hide (widget);

      widget = popup_page_uiinfo[5].widget; /* bookmarks */
      gtk_widget_set_sensitive (widget, FALSE);
      gtk_widget_hide (widget);
    }

  return menu;
}

static void
on_color_zmodel_changed		(ZModel	*		zmodel _unused_,
				 TeletextView *		view)
{
  teletext_view_load_page (view, view->page, view->subpage, NULL);
}

static gboolean
zvbi_timeout			(gpointer		user_data)
{
  TeletextView *view = user_data;
  ttx_message_data msg_data;
  enum ttx_message msg;

  while ((msg = peek_ttx_message (view->zvbi_client_id, &msg_data)))
    switch (msg)
      {
      case TTX_PAGE_RECEIVED:
	{
	  GdkWindow *window;
	  gint width;
	  gint height;

	  if (view->selecting)
	    continue;

	  window = GTK_WIDGET (view)->window;
	  if (!window)
	    continue;

	  gdk_window_get_geometry (window,
				   /* x */ NULL,
				   /* y */ NULL,
				   &width,
				   &height,
				   /* depth */ NULL);

	  gdk_window_clear_area_e (window, 0, 0, width, height);

	  view->subpage = view->fmt_page->subno;

	  if (view->toolbar)
	    teletext_toolbar_set_url (view->toolbar,
				      view->fmt_page->pgno,
				      view->subpage);

	  history_push (view,
			view->fmt_page->pgno,
			view->monitored_subpage);

	  update_pointer (view);

	  /* XXX belongs elsewhere? */
	  if (vbi_export_info_enum (0))
	    {
	      GtkAction *action;

	      action = gtk_action_group_get_action
		(view->action_group, "Export");
	      action_set_sensitive (action, TRUE);
	    }

	  break;
	}

      case TTX_NETWORK_CHANGE:
      case TTX_CHANNEL_SWITCHED:
#if 0 /* temporarily disabled */
      case TTX_PROG_INFO_CHANGE:
      case TTX_TRIGGER:
#endif
	break;

      case TTX_BROKEN_PIPE:
	g_warning ("Broken TTX pipe");
	return FALSE;

      default:
	g_warning ("Unknown message %d in %s",
		   msg, __PRETTY_FUNCTION__);
	break;
      }

  return TRUE;
}

static gboolean
blink_timeout			(gpointer		user_data)
{
  TeletextView *view = user_data;

  refresh_ttx_page (view->zvbi_client_id, GTK_WIDGET (view));

  return TRUE;
}

static void
size_allocate			(GtkWidget *		widget,
				 GtkAllocation *	allocation)
{
  TeletextView *view = TELETEXT_VIEW (widget);

  GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

  scale_image (widget, allocation->width, allocation->height, view);
}

static gboolean
expose_event			(GtkWidget *		widget,
				 GdkEventExpose *	event)
{
  TeletextView *view = TELETEXT_VIEW (widget);
  GdkRegion *region;

  /* XXX this prevents a segv, needs probably a fix elsewhere. */
  scale_image (widget,
	       widget->allocation.width,
	       widget->allocation.height,
	       view);

  render_ttx_page (view->zvbi_client_id,
		   widget->window,
		   widget->style->white_gc,
		   event->area.x, event->area.y,
		   event->area.x, event->area.y,
		   event->area.width, event->area.height);
  
  if (view->selecting
      && view->select.last_x != -1)
    {
      gint columns, rows; /* page */
      gint scol, srow; /* start */
      gint ccol, crow; /* current */

      select_positions (view,
			view->select.last_x,
			view->select.last_y,
			&columns, &rows,
			&scol, &srow,
			&ccol, &crow);

      region = gdk_region_rectangle (&event->area);

      select_transform (view,
			scol, srow,	/* src1 */
			ccol, crow,	/* src2 */
			view->select.table_mode,
			scol, srow,	/* dst2 */
			ccol, crow,	/* dst2 */
			view->select.table_mode,
			region);

      gdk_region_destroy (region);
    }

  return TRUE;
}

static gboolean
on_motion_notify		(GtkWidget *		widget _unused_,
				 GdkEventMotion *	event,
				 TeletextView *		view)
{
  if (view->selecting)
    select_update (view, (int) event->x, (int) event->y, event->state);
  else
    update_pointer (view);

  return FALSE;
}

static void
open_url			(vbi_link *		ld)
{
  GError *err = NULL;

  /* XXX switch out of fullscreen first */

  gnome_url_show (ld->url, &err);

  if (err)
    {

      /* TRANSLATORS: "Cannot open <URL>" (http or smtp). */
      ShowBox (_("Cannot open %s:\n%s"),
	       GTK_MESSAGE_ERROR,
	       ld->url, err->message);

      g_error_free (err);
    }
}

static gboolean
on_button_release		(GtkWidget *		widget _unused_,
				 GdkEventButton *	event _unused_,
				 TeletextView *		view)
{
  if (view->selecting)
    select_stop (view);

  return FALSE; /* pass on */
}

static gboolean
on_button_press			(GtkWidget *		widget,
				 GdkEventButton *	event,
				 TeletextView *		view)
{
  vbi_link ld;

  switch (event->button)
    {
    case 1: /* left button */
      if (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK))
	{
	  select_start (view, (int) event->x, (int) event->y, event->state);
	}
      else
	{
	  teletext_view_vbi_link_from_pointer_position
	    (view, &ld, (int) event->x, (int) event->y);

	  switch (ld.type)
	    {
	    case VBI_LINK_PAGE:
	    case VBI_LINK_SUBPAGE:
	      teletext_view_load_page (view, ld.pgno, ld.subno, NULL);
	      break;

	    case VBI_LINK_HTTP:
	    case VBI_LINK_FTP:
	    case VBI_LINK_EMAIL:
	      open_url (&ld);
	      break;
	      
	    default:
	      select_start (view, (int) event->x,
			    (int) event->y, event->state);
	      break;
	    }
	}

      return TRUE; /* handled */

    case 2: /* middle button, open link in new window */
      teletext_view_vbi_link_from_pointer_position
	(view, &ld, (int) event->x, (int) event->y);

      switch (ld.type)
        {
	case VBI_LINK_PAGE:
	case VBI_LINK_SUBPAGE:
	  python_command_printf (widget,
				 "zapping.ttx_open_new(%x,%d)",
				 ld.pgno, decimal_subno (ld.subno));
	  return TRUE; /* handled */

	case VBI_LINK_HTTP:
	case VBI_LINK_FTP:
	case VBI_LINK_EMAIL:
	  open_url (&ld);
	  return TRUE; /* handled */

	default:
	  break;
	}

      break;

    default:
      break;
    }

  return FALSE; /* pass on */
}

/* Drawing area cannot focus, must be called by parent. */
gboolean
teletext_view_on_key_press	(GtkWidget *		widget _unused_,
				 GdkEventKey *		event,
				 TeletextView *		view)
{
  guint digit;

  /* Loading a page takes time, possibly longer than the key
     repeat period, and repeated keys will stack up. This kludge
     effectively defers all loads until the key is released,
     and discards all but the last load request. */
  if (abs ((int) view->last_key_press_event_time - (int) event->time) < 100
      || event->length > 1)
    view->deferred_load = TRUE;

  view->last_key_press_event_time = event->time;

  digit = event->keyval - GDK_0;

  switch (event->keyval)
    {
    case GDK_KP_0 ... GDK_KP_9:
      digit = event->keyval - GDK_KP_0;

      /* fall through */

    case GDK_0 ... GDK_9:
      if (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK))
	{
	  teletext_view_load_page (view, (vbi_pgno) digit * 0x100,
				   VBI_ANY_SUBNO, NULL);
	  return TRUE; /* handled, don't pass on */
	}

      if (view->page >= 0x100)
	view->page = 0;
      view->page = (view->page << 4) + digit;

      if (view->page >= 0x900)
        view->page = 0x900; /* TOP index page */

      if (view->page >= 0x100)
	{
	  teletext_view_load_page (view, view->page, VBI_ANY_SUBNO, NULL);
	}
      else
	{
	  ttx_freeze (view->zvbi_client_id);

	  if (view->toolbar)
	    teletext_toolbar_set_url (view->toolbar, view->page, 0);
	}

      return TRUE; /* handled */

    case GDK_S:
      /* Menu is "Save As", plain "Save" might be confusing. Now Save As
	 has accelerator Ctrl-Shift-S, omitting the Shift might be
	 confusing. But we accept Ctrl-S (of plain Save) here. */
      if (event->state & GDK_CONTROL_MASK)
	{
	  python_command_printf (GTK_WIDGET (view), "zapping.ttx_export()");
	  return TRUE; /* handled */
	}

      break;

    default:
      break;
    }

  return FALSE; /* pass on */
}

static void
instance_finalize		(GObject *		object)
{
  TeletextView *view = TELETEXT_VIEW (object);
  GdkWindow *window;

  teletext_views = g_list_remove (teletext_views, view);

  if (view->search_dialog)
    gtk_widget_destroy (view->search_dialog);

  g_source_remove (view->zvbi_timeout_id);

  if (view->blink_timeout_id > 0)
    g_source_remove (view->blink_timeout_id);

  if (view->deferred.timeout_id > 0)
    g_source_remove (view->deferred.timeout_id);

  unregister_ttx_client (view->zvbi_client_id);

  if (view->scale.mask)
    g_object_unref (view->scale.mask);

  g_object_unref (view->xor_gc);

  window = GTK_WIDGET (view)->window;

  if (view->select.in_clipboard)
    {
      if (gdk_selection_owner_get (GA_CLIPBOARD) == window)
	gtk_selection_owner_set (NULL, GA_CLIPBOARD, GDK_CURRENT_TIME);
    }

  if (view->select.in_selection)
    {
      if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == window)
	gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY,
				 GDK_CURRENT_TIME);
    }

  if (color_zmodel)
    g_signal_handlers_disconnect_matched
      (G_OBJECT (color_zmodel), G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
       0, 0, NULL, G_CALLBACK (on_color_zmodel_changed), view);

  parent_class->finalize (object);
}

static GtkActionEntry
actions [] = {
  { "GoSubmenu", NULL, N_("_Go"), NULL, NULL, NULL },
  { "HistoryBack", GTK_STOCK_GO_BACK, NULL, NULL,
    N_("Previous page in history"), G_CALLBACK (history_back_action) },
  { "HistoryForward", GTK_STOCK_GO_FORWARD, NULL, NULL,
    N_("Next page in history"), G_CALLBACK (history_forward_action) },
  { "Home", GTK_STOCK_HOME, N_("Index"), NULL,
    N_("Go to the index page"), G_CALLBACK (home_action) },
  { "Search", GTK_STOCK_FIND, NULL, NULL,
    N_("Search page memory"), G_CALLBACK (search_action) },
  { "Export", GTK_STOCK_SAVE_AS, NULL, NULL,
    N_("Save this page to a file"), G_CALLBACK (export_action) },
};

/* We cannot initialize this until the widget (and its
   parent GtkWindow) has a window. */
static void
realize				(GtkWidget *		widget)
{
  TeletextView *view = TELETEXT_VIEW (widget);

  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  /* No background, prevents flicker. */
  gdk_window_set_back_pixmap (widget->window, NULL, FALSE);

  view->xor_gc = gdk_gc_new (widget->window);
  gdk_gc_set_function (view->xor_gc, GDK_INVERT);
}

static void
instance_init			(GTypeInstance *	instance,
				 gpointer		g_class _unused_)
{
  TeletextView *view = (TeletextView *) instance;
  GtkAction *action;
  GtkWidget *widget;
  GObject *object;

  view->action_group = gtk_action_group_new ("TeletextViewActions");
#ifdef ENABLE_NLS
  gtk_action_group_set_translation_domain (view->action_group,
					   GETTEXT_PACKAGE);
#endif					   
  gtk_action_group_add_actions (view->action_group,
				actions, G_N_ELEMENTS (actions), view);

  action = gtk_action_group_get_action (view->action_group, "Export");
  action_set_sensitive (action, FALSE);

  history_update_gui (view);

  widget = GTK_WIDGET (view);

  gtk_widget_add_events (widget,
			 GDK_EXPOSURE_MASK |
			 GDK_POINTER_MOTION_MASK |
			 GDK_BUTTON_PRESS_MASK |
			 GDK_BUTTON_RELEASE_MASK |
			 GDK_KEY_PRESS_MASK |
			 GDK_KEY_RELEASE_MASK |
			 GDK_STRUCTURE_MASK |
			 0);

  object = G_OBJECT (instance);

  g_signal_connect (object, "motion-notify-event",
		    G_CALLBACK (on_motion_notify), view);
  
  g_signal_connect (object, "button-press-event",
		    G_CALLBACK (on_button_press), view);
  
  g_signal_connect (object, "button-release-event",
		    G_CALLBACK (on_button_release), view);

  /* Selection */

  gtk_selection_add_targets (widget, GDK_SELECTION_PRIMARY,
			     clipboard_targets,
			     N_ELEMENTS (clipboard_targets));

  gtk_selection_add_targets (widget, GA_CLIPBOARD,
			     clipboard_targets,
			     N_ELEMENTS (clipboard_targets));

  g_signal_connect (object, "selection_clear_event",
		    G_CALLBACK (on_selection_clear), view);

  g_signal_connect (object, "selection_get",
		    G_CALLBACK (on_selection_get), view);

  /* Update view on color changes. */
  g_signal_connect (G_OBJECT (color_zmodel), "changed",
		    G_CALLBACK (on_color_zmodel_changed), view);

  view->zvbi_client_id = register_ttx_client ();
  view->zvbi_timeout_id = g_timeout_add (100, (GSourceFunc)
					 zvbi_timeout, view);

  view->fmt_page = get_ttx_fmt_page (view->zvbi_client_id);

  view->deferred_load = FALSE;
  view->deferred.timeout_id = NO_SOURCE_ID;

  view->blink_timeout_id =
    g_timeout_add (BLINK_CYCLE / 4, (GSourceFunc) blink_timeout, view);

  set_ttx_parameters (view->zvbi_client_id, view->reveal);

  teletext_view_load_page (view, default_home_pgno (), VBI_ANY_SUBNO, NULL);

  teletext_views = g_list_append (teletext_views, view);
}

GtkWidget *
teletext_view_new		(void)
{
  return GTK_WIDGET (g_object_new (TYPE_TELETEXT_VIEW, NULL));
}

static void
class_init			(gpointer		g_class,
				 gpointer		class_data _unused_)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = G_OBJECT_CLASS (g_class);
  widget_class = GTK_WIDGET_CLASS (g_class);
  parent_class = g_type_class_peek_parent (g_class);

  object_class->finalize = instance_finalize;

  widget_class->realize	= realize;
  widget_class->size_allocate = size_allocate;
  widget_class->expose_event = expose_event;

  cursor_normal	= gdk_cursor_new (GDK_LEFT_PTR);
  cursor_link	= gdk_cursor_new (GDK_HAND2);
  cursor_select	= gdk_cursor_new (GDK_XTERM);

  GA_CLIPBOARD = gdk_atom_intern ("CLIPBOARD", FALSE);

  cmd_register ("ttx_open", py_ttx_open, METH_VARARGS);
  cmd_register ("ttx_page_incr", py_ttx_page_incr, METH_VARARGS,
		("Increment Teletext page number"),
		"zapping.ttx_page_incr(+1)",
		("Decrement Teletext page number"),
		"zapping.ttx_page_incr(-1)");
  cmd_register ("ttx_subpage_incr", py_ttx_subpage_incr, METH_VARARGS,
		("Increment Teletext subpage number"),
		"zapping.ttx_subpage_incr(+1)",
		("Decrement Teletext subpage number"),
		"zapping.ttx_subpage_incr(-1)");
  cmd_register ("ttx_home", py_ttx_home, METH_VARARGS,
		("Go to Teletext index page"), "zapping.ttx_home()");
  cmd_register ("ttx_hold", py_ttx_hold, METH_VARARGS,
		("Hold Teletext subpage"), "zapping.ttx_hold()");
  cmd_register ("ttx_reveal", py_ttx_reveal, METH_VARARGS,
		("Reveal concealed text"), "zapping.ttx_reveal()");
  cmd_register ("ttx_history_prev", py_ttx_history_prev, METH_VARARGS,
		("Previous Teletext page in history"),
		"zapping.ttx_history_prev()");
  cmd_register ("ttx_history_next", py_ttx_history_next, METH_VARARGS,
		("Next Teletext page in history"),
		"zapping.ttx_history_next()");
  cmd_register ("ttx_search", py_ttx_search, METH_VARARGS,
		("Teletext search"), "zapping.ttx_search()");
  cmd_register ("ttx_export", py_ttx_export, METH_VARARGS,
		("Teletext export"), "zapping.ttx_export()");
}

GType
teletext_view_get_type		(void)
{
  static GType type = 0;
  
  if (!type)
    {
      GTypeInfo info;

      CLEAR (info);

      info.class_size = sizeof (TeletextViewClass);
      info.class_init = class_init;
      info.instance_size = sizeof (TeletextView);
      info.instance_init = instance_init;

      type = g_type_register_static (GTK_TYPE_DRAWING_AREA,
				     "TeletextView",
				     &info, (GTypeFlags) 0);
    }

  return type;
}
