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

/* $Id: ttxview.c,v 1.116.2.18 2003-11-16 10:45:48 mschimek Exp $ */

/*
 *  Teletext View
 */

/* XXX gtk+ 2.3 GtkOptionMenu, Gnome entry, toolbar changes */
/* gdk_pixbuf_render_to_drawable */
#undef GTK_DISABLE_DEPRECATED
#undef GNOME_DISABLE_DEPRECATED
#undef GDK_DISABLE_DEPRECATED

#include "config.h"

#ifdef HAVE_LIBZVBI

/* Libzvbi returns static strings which must be localized using
   the zvbi domain. Stupid. Fix planned for next version. */
#ifdef ENABLE_NLS
#  include <libintl.h>
#  define V_(String) dgettext("zvbi", String)
#else
#  define V_(String) (String)
#endif

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkx.h>
#include <ctype.h>

#include "interface.h"
#include "frequencies.h"
#include "ttxview.h"
#include "zvbi.h"
#include "v4linterface.h"
#define ZCONF_DOMAIN "/zapping/ttxview/"
#include "zconf.h"
#include "zmisc.h"
#include "zmodel.h"
#include "common/fifo.h"
#include "common/ucs-2.h"
#include "osd.h"
#include "remote.h"
#include "properties-handler.h"

extern gboolean			flag_exit_program;
extern tveng_tuned_channel *	global_channel_list;
extern gint			cur_tuned_channel;
extern tveng_device_info *	main_info;
extern GtkWidget *		main_window;

#define BLINK_CYCLE 300 /* ms */

typedef struct ttxview_data ttxview_data;
typedef struct export_dialog export_dialog;
typedef struct search_dialog search_dialog;
typedef struct bookmark_dialog bookmark_dialog;

struct export_dialog {
  GtkDialog *		dialog;
  GtkWidget *		entry;
  GtkWidget *		format_menu;
  GtkWidget *		option_box;
  vbi_export *		context;
  vbi_page		page;
  gboolean		reveal;
  gchar *		network;
  gchar *		filename;
};

struct bookmark {
  gchar *		channel;
  vbi_pgno		pgno;
  vbi_subno		subno;
  gchar *		description;
};

struct bookmark_dialog {
  GtkDialog *		dialog;
  GtkTreeView *		tree_view;
  GtkTreeSelection *	selection;
  GtkListStore *	store;
  GtkWidget *		remove;
};

static GList *		bookmarks;
bookmark_dialog 	bookmarks_dialog;
ZModel *		bookmarks_zmodel;

struct search_dialog {
  GtkDialog *		dialog;
  GtkLabel *		label;
  GtkWidget *		entry;
  GtkToggleButton *	regexp;
  GtkToggleButton *	casefold;
  GtkWidget *		back;
  GtkWidget *		forward;
  ttxview_data *	data;
  vbi_search *		context;
  gchar *		text;
  gint			direction;
  gboolean		searching;
};

static GtkWidget *	color_dialog;
static ZModel *		color_zmodel;

struct ttxview_data {
  GnomeApp *		app;

  GtkMenuItem *		bookmarks_menu;

  GtkToolbar *		toolbar;
  struct {
    GtkButton *		  prev;
    GtkButton *		  next;
    GtkBox *		  box1;
    GtkToggleButton *	  hold;
    GtkLabel *		  url;
    GtkBox *		  box2;
    GtkToggleButton *	  reveal;
  }			tool;			/* toolbar buttons */

  GtkWidget *		darea;			/* GtkDrawingArea */
  GdkGC	*		xor_gc;			/* gfx context for xor mask */

  GnomeAppBar *		appbar;

  GtkWidget *		parent;			/* toplevel window */

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
      vbi_pgno		    pgno;
      vbi_subno		    subno;
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

  search_dialog *	search_dialog;

  ZModel		*vbi_model; /* notifies when the vbi object is
				     destroyed */
};

ZModel *		ttxview_zmodel;		/* created / destroyed a view */
static ttxview_data *
ttxview_data_from_widget	(GtkWidget *		widget)
{
  ttxview_data *data;

  while (!(data = (ttxview_data *)
	   g_object_get_data (G_OBJECT (widget), "ttxview_data")))
    {
      if (!widget->parent)
	return NULL;

      widget = widget->parent;
    }

  return data;
}

static void
inc_ttxview_count		(void)
{
  if (!ttxview_zmodel)
    return;

  g_object_set_data (G_OBJECT (ttxview_zmodel), "count",
		     GINT_TO_POINTER (NUM_TTXVIEWS (ttxview_zmodel)) + 1);

  zmodel_changed (ttxview_zmodel);
}

static void
dec_ttxview_count		(void)
{
  if (!ttxview_zmodel)
    return;

  g_object_set_data (G_OBJECT (ttxview_zmodel), "count",
		     GINT_TO_POINTER (NUM_TTXVIEWS (ttxview_zmodel)) - 1);

  zmodel_changed (ttxview_zmodel);
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

static GdkAtom		GA_CLIPBOARD	= GDK_NONE;

static GdkCursor *	cursor_normal;
static GdkCursor *	cursor_link;
static GdkCursor *	cursor_select;
static GdkCursor *	cursor_busy;

static void
load_page			(ttxview_data *		data,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 vbi_page *		pg);

static void
ttxview_delete			(ttxview_data *		data);

static gint
decimal_subno			(vbi_subno		subno)
{
  if (subno == 0 || (guint) subno > 0x99)
    return -1; /* any */
  else
    return vbi_bcd2dec (subno);
}

static void
set_url				(ttxview_data *		data,
				 vbi_pgno		pgno,
				 vbi_subno		subno)
{
  gchar buffer[8];

  if ((guint) subno > 0x99)
    subno = 0; /* 0, VBI_ANY_SUBNO, 0x2359, bug */

  sprintf (buffer, "%3x.%02x", pgno & 0xFFF, subno);

  gtk_label_set_text (data->tool.url, buffer);
}

static void
set_hold			(ttxview_data *		data,
				 gboolean		hold)
{
  if (gtk_toggle_button_get_active (data->tool.hold) != hold)
    gtk_toggle_button_set_active (data->tool.hold, hold);

  if (hold != data->hold)
    {
      vbi_page *pg = data->fmt_page;

      data->hold = hold;

      if (hold)
	load_page (data, pg->pgno, pg->subno, NULL);
      else
	load_page (data, pg->pgno, VBI_ANY_SUBNO, NULL);
    }
}

static PyObject *
py_ttx_hold			(PyObject *		self,
				 PyObject *		args)
{
  ttxview_data *data;
  gint hold;

  if (!(data = ttxview_data_from_widget (python_command_widget ())))
    py_return_true;

  hold = -1;

  if (!PyArg_ParseTuple (args, "|i", &hold))
    g_error ("zapping.ttx_hold(|i)");

  if (hold < 0)
    hold = !data->hold;
  else
    hold = !!hold;

  set_hold (data, hold);

  py_return_true;
}

static PyObject *
py_ttx_reveal			(PyObject *		self,
				 PyObject *		args)
{
  ttxview_data *data;
  gint reveal;

  if (!(data = ttxview_data_from_widget (python_command_widget ())))
    py_return_true;

  reveal = -1;

  if (!PyArg_ParseTuple (args, "|i", &reveal))
    g_error ("zapping.ttx_reveal(|i)");

  if (reveal < 0)
    reveal = !data->reveal;
  else
    reveal = !!reveal;

  if (gtk_toggle_button_get_active (data->tool.reveal) != reveal)
    gtk_toggle_button_set_active (data->tool.reveal, reveal);

  set_ttx_parameters (data->zvbi_client_id, reveal);

  if (data->page >= 0x100)
    load_page (data, data->page, data->subpage, NULL);

  py_return_true;
}

static gboolean
deferred_load_timeout		(gpointer		user_data)
{
  ttxview_data *data = user_data;

  data->deferred.timeout_id = -1;

  if (data->deferred.pgno)
    monitor_ttx_page (data->zvbi_client_id,
		      data->deferred.pgno,
		      data->deferred.subno);
  else
    monitor_ttx_this (data->zvbi_client_id,
		      &data->deferred.pg);

  data->deferred_load = FALSE;

  return FALSE; /* don't call again */
}

static void
load_page			(ttxview_data *		data,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 vbi_page *		pg)
{
  gchar *buffer;

  set_hold (data, data->hold = (subno != VBI_ANY_SUBNO));

  data->subpage = subno;
  data->page = pgno;
  data->monitored_subpage = subno;

  set_url (data, pgno, subno);

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

  if (data->appbar)
    gnome_appbar_set_status (data->appbar, buffer);

  g_free (buffer);

  gtk_widget_grab_focus (data->darea);

  if (pg)
    {
      memcpy (&data->deferred.pg, pg, sizeof (data->deferred.pg));
      data->deferred.pgno = 0;
    }
  else
    {
      data->deferred.pgno = pgno;
      data->deferred.subno = subno;
    }

  if (data->deferred.timeout_id > 0)
    g_source_remove (data->deferred.timeout_id);

  if (data->deferred_load)
    {
      data->deferred.timeout_id =
	g_timeout_add (300, (GSourceFunc) deferred_load_timeout, data);
    }
  else
    {
      data->deferred.timeout_id = -1;

      if (pg)
	monitor_ttx_this (data->zvbi_client_id,
			  &data->deferred.pg);
      else
	monitor_ttx_page (data->zvbi_client_id,
			  pgno, subno);
    }

  z_update_gui ();
}

static void
vbi_link_from_pointer_position	(vbi_link *		ld,
				 ttxview_data *		data,
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

  gdk_window_get_geometry (data->darea->window, NULL, NULL,
			   &width, &height, NULL);

  if (width <= 0 || height <= 0)
    return;

  column = (x * data->fmt_page->columns) / width;
  row = (y * data->fmt_page->rows) / height;

  if (column >= data->fmt_page->columns
      || row >= data->fmt_page->rows)
    return;

  ac = data->fmt_page->text + row * data->fmt_page->columns + column;

  if (ac->link)
    vbi_resolve_link (data->fmt_page, column, row, ld);
}

static void
update_pointer			(ttxview_data *		data)
{
  GtkWidget *widget;
  GdkModifierType mask;
  gint x, y;
  vbi_link ld;
  gchar *buffer;

  widget = data->darea;

  gdk_window_get_pointer (widget->window, &x, &y, &mask);

  vbi_link_from_pointer_position (&ld, data, x, y);

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

    show:
      if (!data->cursor_over_link)
	{
	  data->cursor_over_link = TRUE;

	  if (data->appbar)
	    gnome_appbar_push (GNOME_APPBAR (data->appbar), buffer);

	  gdk_window_set_cursor (widget->window, cursor_link);
	}
      else
	{
	  if (data->appbar)
	    gnome_appbar_set_status (GNOME_APPBAR(data->appbar), buffer);
	}

      g_free(buffer);

      break;

    default:
      if (data->cursor_over_link)
	{
	  data->cursor_over_link = FALSE;

	  if (data->appbar)
	    gnome_appbar_pop (GNOME_APPBAR (data->appbar));

	  gdk_window_set_cursor(widget->window, cursor_normal);
	}

      break;
    }
}

static
void scale_image			(GtkWidget	*wid,
					 gint		w,
					 gint		h,
					 ttxview_data	*data)
{
  if (data->scale.width != w
      || data->scale.height != h)
    {
      if (data->scale.mask)
	g_object_unref (G_OBJECT (data->scale.mask));

      data->scale.mask = gdk_pixmap_new (data->darea->window, w, h, 1);
      g_assert (data->scale.mask != NULL);

      resize_ttx_page(data->zvbi_client_id, w, h);

      data->scale.width = w;
      data->scale.height = h;
    }
}

gboolean
get_ttxview_page		(GtkWidget *		view,
				 gint *			page,
				 gint *			subpage)
{
  ttxview_data *data = g_object_get_data (G_OBJECT (view), "ttxview_data");

  if (!data)
    return FALSE;

  if (page)
    *page = data->fmt_page->pgno;
  if (subpage)
    *subpage = data->monitored_subpage;

  return TRUE;
}

GdkPixbuf *
ttxview_get_scaled_ttx_page	(GtkWidget *		parent)
{
  ttxview_data *data = g_object_get_data
    (G_OBJECT (main_window), "ttxview_data");

  if (!data)
    return NULL;

  return get_scaled_ttx_page (data->zvbi_client_id);
}

/*
 *  Page commands
 */

static PyObject *
open_page			(PyObject *		self,
				 PyObject *		args,
				 gboolean		new_window)
{
  ttxview_data *data;
  int page;
  int subpage;
  vbi_pgno pgno;
  vbi_subno subno;

  data = ttxview_data_from_widget (python_command_widget ());

  if (new_window
      && data
      && data->fmt_page
      && data->fmt_page->pgno)
    {
      page = vbi_bcd2dec (data->fmt_page->pgno);
      subpage = vbi_bcd2dec (data->monitored_subpage & 0xFF);
    }
  else
    {
      page = 100;
      subpage = -1;
    }

  if (!PyArg_ParseTuple (args, "|ii", &page, &subpage))
    g_error ("zapping.ttx_open_new(|ii)");

  if (page >= 100 && page <= 899)
    pgno = vbi_dec2bcd (page);
  else
    py_return_false;

  if (subpage < 0)
    subno = VBI_ANY_SUBNO;
  else if ((guint) subpage <= 99)
    subno = vbi_dec2bcd (subpage);
  else
    py_return_false;

  if (!zvbi_get_object ())
    {
      ShowBox (_("VBI has been disabled"), GTK_MESSAGE_WARNING);
      py_return_false;
    }

  if (new_window)
    {
      GtkWidget *dolly;
      guint width;
      guint height;

      width = 300;
      height = 200;

      if (data)
	gdk_window_get_geometry (data->parent->window, NULL, NULL,
				 &width, &height, NULL);

      dolly = ttxview_new ();
      data = ttxview_data_from_widget (dolly);

      load_page (data, pgno, subno, NULL);

      gtk_widget_realize (dolly);

      z_update_gui ();

      gdk_window_resize (dolly->window, width, height);

      gtk_widget_show (dolly);
    }
  else if (data)
    {
      load_page (data, pgno, subno, NULL);
    }

  py_return_true;
}

static PyObject *
py_ttx_open			(PyObject *		self,
				 PyObject *		args)
{
  return open_page (self, args, FALSE);
}

static PyObject *
py_ttx_open_new			(PyObject *		self,
				 PyObject *		args)
{
  return open_page (self, args, TRUE);
}

static PyObject *
py_ttx_page_incr		(PyObject *		self,
				 PyObject *		args)
{
  ttxview_data *data;
  vbi_pgno pgno;
  gint value;

  if (!(data = ttxview_data_from_widget (python_command_widget ())))
    py_return_true;

  value = +1;

  if (!PyArg_ParseTuple (args, "|i", &value))
    g_error ("zapping.ttx_page_incr(|i)");

  if (abs (value) > 999)
    py_return_false;

  if (value < 0)
    value += 1000;

  pgno = vbi_add_bcd (data->page, vbi_dec2bcd (value)) & 0xFFF;

  if (pgno < 0x100)
    pgno = 0x800 + (pgno & 0xFF);
  else if (pgno > 0x899)
    pgno = 0x100 + (pgno & 0xFF);

  load_page (data, pgno, VBI_ANY_SUBNO, NULL);

  py_return_true;
}

static PyObject *
py_ttx_subpage_incr		(PyObject *		self,
				 PyObject *		args)
{
  ttxview_data *data;
  vbi_subno subno;
  gint value;

  if (!(data = ttxview_data_from_widget (python_command_widget ())))
    py_return_true;

  value = +1;

  if (!PyArg_ParseTuple (args, "|i", &value))
    g_error ("zapping.ttx_subpage_incr(|i)");

  if (abs (value) > 99)
    py_return_false;

  if (value < 0)
    value += 100; /* XXX should use actual or anounced number of subp */

  subno = vbi_add_bcd (data->subpage, vbi_dec2bcd (value)) & 0xFF;

  load_page (data, data->fmt_page->pgno, subno, NULL);

  py_return_true;
}

static PyObject *
py_ttx_home			(PyObject *		self,
				 PyObject *		args)
{
  ttxview_data *data;
  vbi_link ld;

  if (!(data = ttxview_data_from_widget (python_command_widget ())))
    py_return_true;

  vbi_resolve_home (data->fmt_page, &ld);

  if (ld.type == VBI_LINK_PAGE
      || ld.type == VBI_LINK_SUBPAGE)
    {
      if (ld.pgno)
	load_page (data, ld.pgno, ld.subno, NULL);
      else
	load_page (data, 0x100, VBI_ANY_SUBNO, NULL);
    }

  py_return_true;
}

/*
 *  Selection
 */

/* Called when another application claims the selection
   (i.e. sends new data to the clipboard). */
static gint
on_ttxview_selection_clear	(GtkWidget *		widget,
				 GdkEventSelection *	event,
				 ttxview_data *		data)
{
  if (event->selection == GDK_SELECTION_PRIMARY)
    data->select.in_selection = FALSE;
  else if (event->selection == GA_CLIPBOARD)
    data->select.in_clipboard = FALSE;

  return TRUE;
}

/* Called when another application requests our selected data. */
static void
on_ttxview_selection_get	(GtkWidget *		widget,
				 GtkSelectionData *	selection_data,
				 guint			info,
				 guint			time_stamp,
				 ttxview_data *		data)
{
  if ((selection_data->selection == GDK_SELECTION_PRIMARY
       && data->select.in_selection)
      || (selection_data->selection == GA_CLIPBOARD
	  && data->select.in_clipboard))
    {
      switch (info)
	{
	case TARGET_STRING:
	  {
	    int width;
	    int height;
	    int actual;
	    int size;
	    char *buf;

	    width = data->select.column2 - data->select.column1 + 1;
	    height = data->select.row2 - data->select.row1 + 1;

	    size = 25 * 48;
	    actual = 0;

	    if ((buf = malloc (size)))
	      {
		/* XXX According to ICCC Manual 2.0 the STRING target
		   uses encoding ISO Latin-1. How can we use UTF-8? */

#if VBI_VERSION_MAJOR >= 1
		actual = vbi_print_page_region (&data->select.page,
						buf, size, "ISO-8859-1",
						NULL, 0, /* std separator */
						data->select.table_mode,
						/* rtl */ FALSE,
						data->select.column1,
						data->select.row1,
						width,
						height);
#else
		actual = vbi_print_page_region (&data->select.page,
						buf, size, "ISO-8859-1",
						data->select.table_mode,
						/* ltr */ TRUE,
						data->select.column1,
						data->select.row1,
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
	    if (data->select.column2 < data->select.column1)
	      break;

	    width = data->select.column2 - data->select.column1 + 1;
	    height = data->select.row2 - data->select.row1 + 1;

	    pixmap = gdk_pixmap_new (data->darea->window,
				     width * CW,
				     height * CH,
				     -1);

	    canvas = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				     TRUE, 8,
				     width * CW,
				     height * CH);

	    vbi_draw_vt_page_region (&data->select.page,
				     VBI_PIXFMT_RGBA32_LE,
				     (uint32_t *) gdk_pixbuf_get_pixels (canvas),
				     data->select.column1,
				     data->select.row1,
				     width,
				     height,
				     gdk_pixbuf_get_rowstride (canvas),
				     data->select.reveal,
				     /* flash_on */ TRUE);

	    gdk_pixbuf_render_to_drawable (canvas,
					   pixmap,
					   data->darea->style->white_gc,
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
select_positions		(ttxview_data *		data,
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

  gdk_window_get_geometry (data->darea->window,
			   NULL, NULL,
			   &width, &height,
			   NULL);

  *pcols = columns = data->fmt_page->columns;
  *prows = rows = data->fmt_page->rows;

  *scol = SATURATE ((data->select.start_x * columns) / width, 0, columns - 1);
  *srow = SATURATE ((data->select.start_y * rows) / height, 0, rows - 1);

  *ccol = SATURATE ((x * columns) / width, 0, columns - 1);
  *crow = SATURATE ((y * rows) / height, 0, rows - 1);
}

static __inline__ gboolean
is_hidden_row			(ttxview_data *		data,
				 gint			row)
{
  if (row <= 0 || row >= 25)
    return FALSE;

  return !!(data->fmt_page->double_height_lower & (1 << row));
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
select_transform		(ttxview_data *		data,
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

  gdk_window_get_geometry (data->darea->window,
			   NULL, NULL,
			   &width, &height,
			   NULL);

  columns = data->fmt_page->columns;
  rows = data->fmt_page->rows;

  gdk_gc_set_clip_origin (data->xor_gc, 0, 0);

  {
    gboolean h1, h2, h3, h4;

    if (sy1 > sy2)
      {
	SWAP (sx1, sx2);
	SWAP (sy1, sy2);
      }

    h1 = is_hidden_row (data, sy1);
    h2 = is_hidden_row (data, sy1 + 1);
    h3 = is_hidden_row (data, sy2);
    h4 = is_hidden_row (data, sy2 + 1);

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

    h1 = is_hidden_row (data, dy1);
    h2 = is_hidden_row (data, dy1 + 1);
    h3 = is_hidden_row (data, dy2);
    h4 = is_hidden_row (data, dy2 + 1);

    if (dtable || dy1 == dy2 || ((dy2 - dy1) == 1 && h3))
      {
	if (dx1 > dx2)
	  SWAP(dx1, dx2);

	data->select.column1 = dx1;
	data->select.row1 = dy1 -= h1;
	data->select.column2 = dx2;
	data->select.row2 = dy2 += h4;

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

	data->select.column1 = dx1;
	data->select.row1 = dy1 + h2;
	data->select.column2 = dx2;
	data->select.row2 = dy2 - h3;

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

  gdk_gc_set_clip_region (data->xor_gc, src_region);

  gdk_region_destroy (src_region);

  gdk_draw_rectangle (data->darea->window,
		      data->xor_gc,
		      TRUE,
		      0, 0,
		      width - 1, height - 1);

  gdk_gc_set_clip_rectangle (data->xor_gc, NULL);
}

static void
select_stop			(ttxview_data *		data)
{
  if (data->appbar)
    gnome_appbar_pop (data->appbar);

  if (data->select.last_x != -1)
    {
      gint columns, rows; /* page */
      gint scol, srow; /* start */
      gint ccol, crow; /* current */
      gint tcol1, trow1;
      gint tcol2, trow2;

      select_positions (data,
			data->select.last_x,
			data->select.last_y,
			&columns, &rows,
			&scol, &srow,
			&ccol, &crow);

      /* Save because select_transform() will reset. */
      tcol1 = data->select.column1;
      trow1 = data->select.row1;
      tcol2 = data->select.column2;
      trow2 = data->select.row2;

      select_transform (data,
			scol, srow,	/* src1 */
			ccol, crow,	/* src2 */
			data->select.table_mode,
			columns, rows,	/* dst1 */
			columns, rows,	/* dst2 */
			data->select.table_mode,
			NULL);

      /* Copy selected text. */

      data->select.page    = *data->fmt_page;

      data->select.column1 = tcol1;
      data->select.row1    = trow1;
      data->select.column2 = tcol2;
      data->select.row2    = trow2;

      data->select.reveal  = data->reveal;

      if (!data->select.in_clipboard)
	if (gtk_selection_owner_set (data->darea,
				     GA_CLIPBOARD,
				     GDK_CURRENT_TIME))
	  data->select.in_clipboard = TRUE;

      if (!data->select.in_selection)
	if (gtk_selection_owner_set (data->darea,
				     GDK_SELECTION_PRIMARY,
				     GDK_CURRENT_TIME))
	  data->select.in_selection = TRUE;

      if (data->appbar)
	gnome_appbar_set_status (data->appbar,
				 _("Selection copied to clipboard"));
    }

  ttx_unfreeze (data->zvbi_client_id);

  update_pointer (data);

  data->selecting = FALSE;
}

static void
select_update			(ttxview_data *		data,
				 gint			x,
				 gint			y,
				 guint			state)
{
  gint columns, rows; /* page */
  gint scol, srow; /* start */
  gint ocol, orow; /* last */
  gint ccol, crow; /* current */
  gboolean table;

  select_positions (data,
		    x, y,
		    &columns, &rows,
		    &scol, &srow,
		    &ccol, &crow);

  table = !!(state & GDK_SHIFT_MASK);

  if (data->select.last_x == -1)
    {
      /* First motion. */
      select_transform (data,
			columns, rows,	/* src1 */
			columns, rows,	/* src2 */
			data->select.table_mode,
			scol, srow,	/* dst1 */
			ccol, crow,	/* dst2 */
			table,
			NULL);
    }
  else
    {
      gint width, height;

      gdk_window_get_geometry (data->darea->window,
			       NULL, NULL,
			       &width, &height,
			       NULL);

      ocol = (data->select.last_x * columns) / width;
      ocol = SATURATE (ocol, 0, columns - 1);
      orow = (data->select.last_y * rows) / height;
      orow = SATURATE (orow, 0, rows - 1);

      select_transform (data,
			scol, srow,	/* src1 */	
			ocol, orow,	/* src2 */
			data->select.table_mode,
			scol, srow,	/* dst1 */
			ccol, crow,	/* dst2 */
			table,
			NULL);
    }

  data->select.last_x = MAX (0, x);
  data->select.last_y = y;

  data->select.table_mode = table;
}

static void
select_start			(ttxview_data *		data,
				 gint			x,
				 gint			y,
				 guint			state)
{
  if (data->selecting)
    return;

  if (data->fmt_page->pgno < 0x100)
    {
      if (data->appbar)
  	gnome_appbar_set_status (data->appbar, _("No page loaded"));
      return;
    }

  if (data->cursor_over_link)
    {
      data->cursor_over_link = FALSE;
      if (data->appbar)
	gnome_appbar_pop (data->appbar);
    }

  if (data->appbar)
    gnome_appbar_push (data->appbar,
		       _("Selecting - press Shift key for table mode"));

  gdk_window_set_cursor (data->darea->window, cursor_select);

  data->select.start_x = x;
  data->select.start_y = y;

  data->select.last_x = -1; /* not yet, wait for move event */

  data->select.table_mode = !!(state & GDK_SHIFT_MASK);

  data->selecting = TRUE;

  ttx_freeze (data->zvbi_client_id);
}

/*
 *  Export dialog
 */

/**
 * xo_zconf_name:
 * @exp:
 * @oi: 
 * 
 * Return value: 
 * Zconf name for the given export option,
 * must be g_free()ed.
 **/
static __inline__ gchar *
xo_zconf_name			(vbi_export *		e,
				 vbi_option_info *	oi)
{
  vbi_export_info *xi = vbi_export_info_export (e);

  g_assert (xi != NULL);

  return g_strdup_printf ("/zapping/options/export/%s/%s",
			  xi->keyword, oi->keyword);
}

static void
export_dialog_set_tooltip	(GtkWidget *		widget,
				 vbi_option_info *	oi)
{
  gchar *buffer;

  if ((buffer = V_(oi->tooltip)))
    {
      buffer = g_locale_to_utf8 (buffer, strlen (buffer), NULL, NULL, NULL);
      z_tooltip_set (widget, buffer);
      g_free (buffer);
    }
}

static GtkWidget *
export_dialog_label_new		(vbi_option_info *	oi)
{
  GtkWidget *label;
  GtkMisc *misc;
  gchar *buffer1;
  gchar *buffer2;

  /* XXX */
  buffer1 = V_(oi->label);
  buffer1 = g_locale_to_utf8 (buffer1, strlen (buffer1), NULL, NULL, NULL);
  buffer2 = g_strconcat (buffer1, ":", NULL);
  g_free (buffer1);
  label = gtk_label_new (buffer2);
  g_free (buffer2);

  misc = GTK_MISC (label);
  gtk_misc_set_alignment (misc, 1, 0.5);
  gtk_misc_set_padding (misc, 3, 0);

  return label;
}

static void
on_export_dialog_control_changed
				(GtkWidget *		widget,
				 vbi_export *		e)
{
  gchar *keyword;
  vbi_option_info *oi;
  vbi_option_value val;
  gchar *zcname;

  g_assert (e != NULL);

  keyword = (gchar *) g_object_get_data (G_OBJECT (widget), "key");
  oi = vbi_export_option_info_keyword (e, keyword);

  g_assert (oi != NULL);

  zcname = xo_zconf_name (e, oi);

  if (oi->menu.str)
    {
      val.num = (gint) g_object_get_data (G_OBJECT (widget), "index");
      vbi_export_option_menu_set (e, keyword, val.num);
    }
  else
    {
      switch (oi->type)
	{
	case VBI_OPTION_BOOL:
	  val.num = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	  if (vbi_export_option_set (e, keyword, val))
	    zconf_set_boolean (val.num, zcname);
	  break;

	case VBI_OPTION_INT:
	  val.num = (int) GTK_ADJUSTMENT (widget)->value;
	  if (vbi_export_option_set (e, keyword, val))
	    zconf_set_integer (val.num, zcname);
	  break;

	case VBI_OPTION_REAL:
	  val.dbl = GTK_ADJUSTMENT (widget)->value;
	  if (vbi_export_option_set (e, keyword, val))
	    zconf_set_float (val.dbl, zcname);
	  break;

	case VBI_OPTION_STRING:
	  val.str = (gchar * ) gtk_entry_get_text (GTK_ENTRY (widget));
	  if (vbi_export_option_set (e, keyword, val))
	    zconf_set_string (val.str, zcname);
	  break;

	default:
	  g_warning ("Unknown export option type %d in %s",
		     oi->type, __PRETTY_FUNCTION__);
	  break;
	}
    }

  g_free(zcname);
}

static void
export_create_menu		(GtkWidget *		table,
				 vbi_option_info *	oi,
				 unsigned int		index,
				 vbi_export *		e)
{
  GtkWidget *option_menu;
  GtkWidget *menu;
  gchar *zcname;
  guint saved;
  unsigned int i;

  option_menu = gtk_option_menu_new ();
  menu = gtk_menu_new ();

  zcname = xo_zconf_name (e, oi);
  saved = zconf_get_integer (NULL, zcname);

  for (i = 0; i <= oi->max.num; ++i)
    {
      gchar buf[32];
      gchar *buffer;
      GtkWidget *menu_item;

      switch (oi->type)
	{
	case VBI_OPTION_BOOL:
	case VBI_OPTION_INT:
	  g_snprintf (buf, sizeof (buf), "%d", oi->menu.num[i]);
	  menu_item = gtk_menu_item_new_with_label (buf);
	  break;

	case VBI_OPTION_REAL:
	  g_snprintf (buf, sizeof (buf), "%f", oi->menu.dbl[i]);
	  menu_item = gtk_menu_item_new_with_label (buf);
	  break;

	case VBI_OPTION_STRING:
	  /* XXX encoding? */
	  menu_item = gtk_menu_item_new_with_label (oi->menu.str[i]);
	  break;

	case VBI_OPTION_MENU:
	  /* XXX */
	  buffer = V_(oi->menu.str[i]);
	  buffer = g_locale_to_utf8 (buffer, strlen (buffer), NULL, NULL, NULL);
	  menu_item = gtk_menu_item_new_with_label (buffer);
	  g_free (buffer);
	  break;

	default:
	  g_warning ("Unknown export option type %d in %s",
		     oi->type, __PRETTY_FUNCTION__);
	  continue;
	}

      g_object_set_data (G_OBJECT (menu_item), "key", oi->keyword);
      g_object_set_data (G_OBJECT (menu_item), "index", GINT_TO_POINTER (i));

      g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (on_export_dialog_control_changed), e);

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

      if (i == saved)
	on_export_dialog_control_changed (menu_item, e);
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
  zconf_create_integer (oi->def.num, oi->tooltip, zcname);
  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), saved);
  export_dialog_set_tooltip (option_menu, oi);

  g_free (zcname);

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (table), export_dialog_label_new (oi),
		    0, 1, index, index + 1,
                    (GtkAttachOptions)(GTK_FILL),
                    (GtkAttachOptions)(0), 3, 3);
  gtk_table_attach (GTK_TABLE (table), option_menu,
		    1, 2, index, index + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 3, 3);
}

static void
export_create_checkbutton	(GtkWidget *		table,
				 vbi_option_info *	oi,
				 unsigned int		index,
				 vbi_export *		e)
{
  GtkWidget *check_button;
  gchar *buffer;
  gchar *zcname;

  /* XXX */
  buffer = V_(oi->label);
  buffer = g_locale_to_utf8 (buffer, strlen (buffer), NULL, NULL, NULL);
  check_button = gtk_check_button_new_with_label (buffer);
  g_free (buffer);
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (check_button),
			      /* indicator */ FALSE);
  export_dialog_set_tooltip (check_button, oi);
  g_object_set_data (G_OBJECT (check_button), "key", oi->keyword);
  g_signal_connect (G_OBJECT (check_button), "toggled",
		     G_CALLBACK (on_export_dialog_control_changed), e);

  zcname = xo_zconf_name (e, oi);
  zconf_create_boolean (oi->def.num, oi->tooltip, zcname);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
				zconf_get_boolean (NULL, zcname));
  g_free (zcname);

  on_export_dialog_control_changed (check_button, e);

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (table), check_button,
		    1, 2, index, index + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 3, 3);
}

static void
export_create_slider		(GtkWidget *		table,
				 vbi_option_info *	oi,
				 unsigned int		index,
				 vbi_export *		e)
{ 
  GtkObject *adj;
  GtkWidget *hscale;
  gchar *zcname;

  zcname = xo_zconf_name (e, oi);

  if (oi->type == VBI_OPTION_INT)
    {
      adj = gtk_adjustment_new (oi->def.num, oi->min.num,
				oi->max.num, 1, 10, 10);
      zconf_create_integer (oi->def.num, oi->tooltip, zcname);
      gtk_adjustment_set_value (GTK_ADJUSTMENT (adj),
				zconf_get_integer (NULL, zcname));
    }
  else
    {
      adj = gtk_adjustment_new (oi->def.dbl, oi->min.dbl,
				oi->max.dbl, 1, 10, 10);
      zconf_create_float (oi->def.dbl, oi->tooltip, zcname);
      gtk_adjustment_set_value (GTK_ADJUSTMENT (adj),
				zconf_get_float (NULL, zcname));
    }

  g_free (zcname);

  g_object_set_data (G_OBJECT (adj), "key", oi->keyword);
  g_signal_connect (adj, "value-changed",
		    G_CALLBACK (on_export_dialog_control_changed), e);

  on_export_dialog_control_changed ((GtkWidget *) adj, e);

  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adj));
  gtk_scale_set_value_pos (GTK_SCALE (hscale), GTK_POS_LEFT);
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  export_dialog_set_tooltip (hscale, oi);

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (table), export_dialog_label_new (oi),
		    0, 1, index, index + 1,
                    (GtkAttachOptions)(GTK_FILL),
                    (GtkAttachOptions)(0), 3, 3);
  gtk_table_attach (GTK_TABLE (table), hscale,
		    1, 2, index, index + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 3, 3);
}

static void
export_create_entry		(GtkWidget *		table,
				 vbi_option_info *	oi,
				 unsigned int		index,
				 vbi_export *		e)
{ 
  GtkWidget *entry;
  gchar *zcname;

  entry = gtk_entry_new ();
  export_dialog_set_tooltip (entry, oi);

  g_object_set_data (G_OBJECT (entry), "key", oi->keyword);
  g_signal_connect (G_OBJECT (entry), "changed", 
		    G_CALLBACK (on_export_dialog_control_changed), e);

  on_export_dialog_control_changed (entry, e);

  zcname = xo_zconf_name (e, oi);
  zconf_create_string (oi->def.str, oi->tooltip, zcname);
  gtk_entry_set_text (GTK_ENTRY (entry),
		      zconf_get_string (NULL, zcname));
  g_free (zcname);

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (table), export_dialog_label_new (oi),
		    0, 1, index, index + 1,
                    (GtkAttachOptions)(GTK_FILL),
                    (GtkAttachOptions)(0), 3, 3);
  gtk_table_attach (GTK_TABLE (table), entry,
		    1, 2, index, index + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 3, 3);
}

static GtkWidget *
export_dialog_options_table_new	(vbi_export *		e)
{
  GtkWidget *table;
  vbi_option_info *oi;
  unsigned int i;

  table = gtk_table_new (1, 2, FALSE);

  for (i = 0; (oi = vbi_export_option_info_enum (e, i)); ++i)
    {
      if (!oi->label)
	continue; /* not intended for user */

      if (oi->menu.str)
	{
	  export_create_menu (table, oi, i, e);
	}
      else
	{
	  switch (oi->type)
	    {
	    case VBI_OPTION_BOOL:
	      export_create_checkbutton (table, oi, i, e);
	      break;

	    case VBI_OPTION_INT:
	    case VBI_OPTION_REAL:
	      export_create_slider (table, oi, i, e);
	      break;

	    case VBI_OPTION_STRING:
	      export_create_entry (table, oi, i, e);
	      break;

	    default:
	      g_warning ("Unknown export option type %d in %s",
			 oi->type, __PRETTY_FUNCTION__);
	      continue;
	    }
	}
    }

  return table;
}

static gchar *
export_dialog_default_filename	(export_dialog *	sp)
{
  vbi_export_info *xi;
  gchar **extensions;
  gchar *filename;
  vbi_subno subno;

  xi = vbi_export_info_export (sp->context);
  extensions = g_strsplit (xi->extension, ",", 2);

  subno = sp->page.subno;

  if (subno > 0 && subno <= 0x99)
    filename = g_strdup_printf ("%s-%x-%x.%s", sp->network,
				sp->page.pgno, subno,
				extensions[0]);
  else
    filename = g_strdup_printf ("%s-%x.%s", sp->network,
				sp->page.pgno,
				extensions[0]);

  g_strfreev (extensions);

  return filename;
}

static void
on_export_dialog_menu_activate	(GtkWidget *		menu_item,
				 export_dialog *	sp)
{
  gchar *keyword;
  GtkContainer *container;
  GList *glist;

  keyword = (gchar *) g_object_get_data (G_OBJECT (menu_item), "key");
  g_assert (keyword != NULL);
  zconf_set_string (keyword, "/zapping/options/export_format");

  if (sp->context)
    vbi_export_delete (sp->context);
  sp->context = vbi_export_new (keyword, NULL);
  g_assert (sp->context != NULL);

  /* Don't care if these fail */
  vbi_export_option_set (sp->context, "network", sp->network);
  vbi_export_option_set (sp->context, "creator", "Zapzilla " VERSION);
  vbi_export_option_set (sp->context, "reveal", sp->reveal);

  {
    vbi_export_info *xi;
    gchar **extensions;

    xi = vbi_export_info_export (sp->context);
    extensions = g_strsplit (xi->extension, ",", 2);
    z_electric_replace_extension (sp->entry, extensions[0]);
    g_strfreev (extensions);
  }

  container = GTK_CONTAINER (sp->option_box);
  while ((glist = gtk_container_get_children (container)))
    gtk_container_remove (container, GTK_WIDGET (glist->data));

  if (vbi_export_option_info_enum (sp->context, 0))
    {
      GtkWidget *frame;
      GtkWidget *table;

      frame = gtk_frame_new (_("Options"));
      table = export_dialog_options_table_new (sp->context);
      gtk_container_add (GTK_CONTAINER (frame), table);
      gtk_widget_show_all (frame);
      gtk_box_pack_start (GTK_BOX (sp->option_box), frame, TRUE, TRUE, 0);
    }
}

static void
on_export_dialog_destroy	(GObject *		object,
				 export_dialog *	sp)
{
  if (sp->context)
    vbi_export_delete (sp->context);

  g_free (sp->network);
  g_free (sp->filename);

  g_free (sp);
}

static void
on_export_dialog_cancel_clicked
				(GtkWidget *		widget,
				 gpointer 		user_data)
{
  while (widget->parent)
    widget = widget->parent;

  gtk_widget_destroy (widget);
}

static void
on_export_dialog_ok_clicked	(GtkWidget *		button,
				 export_dialog *	sp)
{
  gchar *name;

  /* XXX electric doesn't work
  name = sp->filename;
   */
  name = (gchar *) gtk_entry_get_text (GTK_ENTRY (sp->entry));

  if (!name || !*name)
    {
      gtk_window_present (GTK_WINDOW (sp->dialog));
      gtk_widget_grab_focus (sp->entry);
      return;
    }

  name = g_strdup (name);

  /* XXX check if file exists, prompt user */

  {
    gchar *dirname;

    g_strstrip (name);

    dirname = g_path_get_dirname (name);

    if (strcmp (dirname, ".") != 0 || name[0] == '.')
      {
	gchar *errstr;

	if (!z_build_path (dirname, &errstr))
	  {
	    ShowBox (_("Cannot create directory %s:\n%s"),
		     GTK_MESSAGE_WARNING, dirname, errstr);
	    g_free (errstr);
	    g_free (dirname);
	    goto finish;
	  }

	/* make absolute path? */
	zcs_char (dirname, "exportdir");
      }
    else
      {
	zcs_char ("", "exportdir");
      }

    g_free (dirname);
  }

  if (!vbi_export_file (sp->context, name, &sp->page))
    {
      char *msg = vbi_export_errstr (sp->context);
      gchar *t;

      t = g_locale_to_utf8 (msg, -1, NULL, NULL, NULL);
      g_assert (t != NULL);

      g_warning (msg);

      /*
	if (data->appbar)
      	  gnome_appbar_set_status (data->appbar, msg);
      */

      g_free (t);

      free (msg);
    }
  /*
  else if (data->appbar)
    {
      gchar *msg = g_strdup_printf (_("%s saved"), name);

      gnome_appbar_set_status (data->appbar, msg);
      g_free (msg);
    }
  */

 finish:
  g_free (name);

  on_export_dialog_cancel_clicked (button, sp);
}

static GtkWidget *
export_dialog_new		(ttxview_data *		data,
				 const gchar *		network)
{
  export_dialog *sp;
  GtkWindow *window;
  GtkBox *vbox;
  GtkBox *hbox;
  GtkWidget *file_entry;
  GtkWidget *widget;

  sp = g_malloc0 (sizeof (*sp));

  sp->page = *data->fmt_page;
  sp->reveal = data->reveal;
  sp->network = g_strdup (network);

  widget = gtk_dialog_new ();
  window = GTK_WINDOW (widget);
  sp->dialog = GTK_DIALOG (widget);
  gtk_window_set_title (window, _("Save as"));
  gtk_window_set_transient_for (window, GTK_WINDOW (data->parent));

  g_signal_connect (G_OBJECT (widget), "destroy",
		    G_CALLBACK (on_export_dialog_destroy), sp);

  widget = gtk_vbox_new (FALSE, 3);
  gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
  vbox = GTK_BOX (widget);
  gtk_box_pack_start (GTK_BOX (sp->dialog->vbox), widget, TRUE, TRUE, 0);

  file_entry = gnome_file_entry_new ("ttxview_export_id", NULL);
  gtk_widget_set_size_request (file_entry, 400, -1);
  sp->entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (file_entry));
  gtk_entry_set_activates_default (GTK_ENTRY (sp->entry), TRUE);
  gtk_box_pack_start (vbox, file_entry, FALSE, FALSE, 0);

  {
    widget = gtk_hbox_new (FALSE, 0);
    hbox = GTK_BOX (widget);
    gtk_box_pack_start (vbox, widget, FALSE, FALSE, 0);
    
    widget = gtk_label_new (_("Format:"));
    gtk_misc_set_padding (GTK_MISC (widget), 3, 0);
    gtk_box_pack_start (hbox, widget, FALSE, FALSE, 0);

    sp->format_menu = gtk_option_menu_new ();
    gtk_box_pack_start (hbox, sp->format_menu, TRUE, TRUE, 0);
  }

  sp->option_box = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (vbox, sp->option_box, TRUE, TRUE, 0);

  {
    GtkWidget *menu;
    gchar *format;
    vbi_export_info *xm;
    unsigned int i;

    menu = gtk_menu_new ();
    gtk_option_menu_set_menu (GTK_OPTION_MENU (sp->format_menu), menu);

    zconf_get_string (&format, "/zapping/options/export_format");

    for (i = 0; (xm = vbi_export_info_enum (i)); ++i)
      if (xm->label) /* user module */
	{
	  GtkWidget *menu_item;

	  menu_item = gtk_menu_item_new_with_label (xm->label);
	  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

	  if (xm->tooltip)
	    z_tooltip_set (menu_item, xm->tooltip);

	  g_object_set_data (G_OBJECT (menu_item), "key", xm->keyword);

	  if (i == 0 || (format && 0 == strcmp (xm->keyword, format)))
	    {
	      on_export_dialog_menu_activate (menu_item, sp);
	      gtk_option_menu_set_history (GTK_OPTION_MENU (sp->format_menu),
					   i);
	    }

	  g_signal_connect (G_OBJECT (menu_item), "activate",
			    G_CALLBACK (on_export_dialog_menu_activate), sp);
	}

    g_free (format);
  }

  {
    gchar *base;
    gchar *path;

    base = export_dialog_default_filename (sp);
    z_electric_set_basename (sp->entry, base);

    path = g_build_filename (zcg_char (NULL, "exportdir"), base, NULL);
    gtk_entry_set_text (GTK_ENTRY (sp->entry), path);

    sp->filename = path;

    /* XXX doesn't work
    g_signal_connect (G_OBJECT (sp->entry), "changed",
		      G_CALLBACK (z_on_electric_filename),
		      (gpointer) &sp->filename);
    */

    g_free (base);
  }

  widget = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
  gtk_dialog_add_action_widget (sp->dialog, widget, 1);
  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (widget), "clicked",
  		    G_CALLBACK (on_export_dialog_cancel_clicked), sp);

  widget = gtk_button_new_from_stock (GTK_STOCK_OK);
  gtk_dialog_add_action_widget (sp->dialog, widget, 2);
  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (widget), "clicked",
  		    G_CALLBACK (on_export_dialog_ok_clicked), sp);

  gtk_dialog_set_default_response (sp->dialog, 2);

  gtk_widget_grab_focus (sp->entry);

  return GTK_WIDGET (sp->dialog);
}

static void
on_export_dialog_activate	(GtkWidget *		widget,
				 ttxview_data *		data)
{
  GtkWidget *dialog;
  vbi_network network;

  if (data->fmt_page->pgno < 0x100)
    {
      if (data->appbar) /* XXX make save option insensitive instead */
	gnome_appbar_set_status (data->appbar, _("No page loaded"));
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

  if ((dialog = export_dialog_new (data, network.name)))
    gtk_widget_show_all (dialog);
}

/*
 *  Color dialog
 *
 *  Currently just text brightness and contrast, in the future possibly
 *  also Caption default colors, overriding standard white on blank.
 */

static void
on_color_dialog_control_changed	(GtkWidget *		adj,
				 gpointer		user_data)
{
  vbi_decoder *vbi = zvbi_get_object ();
  int value;

  switch (GPOINTER_TO_INT (user_data))
    {
    case 0:
      value = GTK_ADJUSTMENT (adj)->value;
      value = SATURATE (value, 0, 255);
      zconf_set_integer (value, "/zapping/options/text/brightness");
      vbi_set_brightness (vbi, value);
      break;

    case 1:
      value = GTK_ADJUSTMENT (adj)->value;
      value = SATURATE (value, -128, +127);
      zconf_set_integer (value, "/zapping/options/text/contrast");
      vbi_set_contrast (vbi, value);
      break;
    }

  zmodel_changed (color_zmodel);
}

static gboolean
on_color_dialog_key_press	(GtkWidget *		widget,
				 GdkEventKey *		event,
				 gpointer		data)
{
  switch (event->keyval)
    {
    case GDK_Escape:
      gtk_widget_destroy (widget);
      return TRUE; /* handled */

    case GDK_c:
    case GDK_C:
      if (event->state & GDK_CONTROL_MASK)
	{
	  gtk_widget_destroy (widget);
	  return TRUE; /* handled */
	}

    default:
      break;
    }

  return FALSE; /* don't know, pass on */
}

static GtkWidget *
color_dialog_new		(ttxview_data *		data)
{
  GtkWidget *window;
  GtkWidget *table;
  GtkWidget *widget;
  GtkObject *adj;
  gint value;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), _("Text Color"));

  g_signal_connect (G_OBJECT (window), "key-press-event",
		    G_CALLBACK (on_color_dialog_key_press), NULL);

  widget = gtk_vbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
  gtk_container_add (GTK_CONTAINER (window), widget);

  table = gtk_table_new (2, 2, FALSE);
  gtk_container_add (GTK_CONTAINER (widget), table);

  {
    widget = gtk_image_new_from_stock ("zapping-brightness",
				       GTK_ICON_SIZE_BUTTON);
    gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
    widget = z_tooltip_set_wrap (widget, _("Brightness"));

    gtk_table_attach_defaults (GTK_TABLE (table), widget, 0, 1, 0, 1);

    zconf_get_integer (&value, "/zapping/options/text/brightness");
    adj = gtk_adjustment_new (value, 0, 255, 1, 16, 16);
    widget = z_spinslider_new (GTK_ADJUSTMENT (adj), NULL, NULL, 128, 0);
    z_spinslider_set_value (widget, value);

    gtk_table_attach (GTK_TABLE (table), widget, 1, 2, 0, 1,
		      (GtkAttachOptions)(GTK_FILL | GTK_EXPAND),
		      (GtkAttachOptions)(0), 3, 3);

    g_signal_connect (G_OBJECT (adj), "value-changed",
		      G_CALLBACK (on_color_dialog_control_changed),
		      GINT_TO_POINTER (0));
  }

  {
    widget = gtk_image_new_from_stock ("zapping-contrast",
				       GTK_ICON_SIZE_BUTTON);
    gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
    widget = z_tooltip_set_wrap (widget, _("Contrast"));

    gtk_table_attach_defaults (GTK_TABLE (table), widget, 0, 1, 1, 2);

    zconf_get_integer (&value, "/zapping/options/text/contrast");
    adj = gtk_adjustment_new (value, -128, +127, 1, 16, 16);
    widget = z_spinslider_new (GTK_ADJUSTMENT (adj), NULL, NULL, 64, 0);
    z_spinslider_set_value (widget, value);

    gtk_table_attach (GTK_TABLE (table), widget, 1, 2, 1, 2,
		      (GtkAttachOptions)(GTK_FILL | GTK_EXPAND),
		      (GtkAttachOptions)(0), 3, 3);

    g_signal_connect (G_OBJECT (adj), "value-changed",
		      G_CALLBACK (on_color_dialog_control_changed),
		      GINT_TO_POINTER (1));
  }

  return window;
}

static void
on_color_dialog_activate	(GtkWidget *		menu_item,
				 ttxview_data *		data)
{
  if (color_dialog)
    gtk_window_present (GTK_WINDOW (color_dialog));
  else if ((color_dialog = color_dialog_new (data)))
    gtk_widget_show_all (color_dialog);
}

/*
 *  Bookmarks
 */

static __inline__ void
bookmark_delete			(struct bookmark *	bookmark)
{
  if (bookmark)
    {
      g_free (bookmark->channel);
      g_free (bookmark->description);
      g_free (bookmark);
    }
}

static void
bookmark_delete_all		(void)
{
  while (bookmarks)
    {
      bookmark_delete (bookmarks->data);
      bookmarks = g_list_delete_link (bookmarks, bookmarks);
    }
}

static struct bookmark *
bookmark_add			(const gchar *		channel,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 const gchar *		description)
{
  struct bookmark *bookmark;

  bookmark = g_malloc (sizeof (*bookmark));

  bookmark->channel =
    (channel && *channel) ? g_strdup (channel) : NULL;

  bookmark->pgno = pgno;
  bookmark->subno = subno;

  bookmark->description	=
    (description && *description) ? g_strdup (description) : NULL;

  bookmarks = g_list_append (bookmarks, bookmark);

  return bookmark;
}

static void
bookmark_save			(void)
{
  GList *glist;
  guint i;

  zconf_delete (ZCONF_DOMAIN "bookmarks");

  i = 0;

  for (glist = bookmarks; glist; glist = glist->next)
    {
      struct bookmark *bookmark;
      gchar buf[200];
      int n;

      bookmark = (struct bookmark *) glist->data;

      n = snprintf (buf, sizeof (buf) - 20, ZCONF_DOMAIN "bookmarks/%d/", i);
      g_assert (n < sizeof (buf) - 20);

      if (bookmark->channel)
	{
	  strcpy (buf + n, "channel");
	  zconf_create_string (bookmark->channel, "Channel", buf);
	}

      strcpy (buf + n, "page");
      zconf_create_integer (bookmark->pgno, "Page", buf);
      strcpy (buf + n, "subpage");
      zconf_create_integer (bookmark->subno, "Subpage", buf);

      if (bookmark->description)
	{
	  strcpy (buf + n, "description");
	  zconf_create_string (bookmark->description, "Description", buf);
	}

      ++i;
    }
}

static void
bookmark_load			(void)
{
  gchar *buffer;
  gchar *buffer2;
  gint pgno;
  gint subno;
  const gchar *channel;
  const gchar *descr;
  guint i;

  i = 0;

  while (zconf_get_nth (i, &buffer, ZCONF_DOMAIN "bookmarks"))
    {
      buffer2 = g_strconcat (buffer, "/channel", NULL);
      channel = zconf_get_string (NULL, buffer2);
      g_free (buffer2);

      buffer2 = g_strconcat (buffer, "/page", NULL);
      zconf_get_integer (&pgno, buffer2);
      g_free (buffer2);

      buffer2 = g_strconcat (buffer, "/subpage", NULL);
      zconf_get_integer (&subno, buffer2);
      g_free (buffer2);

      buffer2 = g_strconcat (buffer, "/description", NULL);
      descr = zconf_get_string (NULL, buffer2);
      g_free (buffer2);

      bookmark_add (channel, pgno, subno, descr);

      g_free (buffer);

      ++i;
    }
}

enum {
  BOOKMARK_COLUMN_CHANNEL,
  BOOKMARK_COLUMN_PGNO,
  BOOKMARK_COLUMN_SUBNO,
  BOOKMARK_COLUMN_DESCRIPTION,
  BOOKMARK_COLUMN_EDITABLE,
  BOOKMARK_NUM_COLUMNS
};

static void
bookmark_dialog_page_cell_data_func
				(GtkTreeViewColumn *	column,
				 GtkCellRenderer *	cell,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter,
				 gpointer		user_data)
{
  gchar buf[32];
  guint pgno;
  guint subno;

  gtk_tree_model_get (model, iter,
		      BOOKMARK_COLUMN_PGNO,	&pgno,
		      BOOKMARK_COLUMN_SUBNO,	&subno,
		      -1);

  if (subno && subno != VBI_ANY_SUBNO)
    g_snprintf (buf, sizeof (buf), "%x.%02x", pgno & 0xFFF, subno & 0xFF);
  else
    g_snprintf (buf, sizeof (buf), "%x", pgno & 0xFFF);

  g_object_set (GTK_CELL_RENDERER (cell), "text", buf, NULL);
}

static void
on_bookmark_dialog_descr_cell_edited
				(GtkCellRendererText *	cell,
				 const gchar *		path_string,
				 const gchar *		new_text,
				 bookmark_dialog *	sp)
{
  GtkTreePath *path;
  GtkTreeIter iter;

  path = gtk_tree_path_new_from_string (path_string);

  gtk_tree_model_get_iter (GTK_TREE_MODEL (sp->store), &iter, path);

  gtk_list_store_set (sp->store, &iter,
		      BOOKMARK_COLUMN_DESCRIPTION, new_text,
		      -1);

  gtk_tree_path_free (path);
}

static void
bookmark_dialog_append		(bookmark_dialog *	sp,
				 struct bookmark *	bookmark)
{
  GtkTreeIter iter;
  gchar *channel;
  gchar *description;

  channel = bookmark->channel ? bookmark->channel : "";
  description = bookmark->description ? bookmark->description : "";

  gtk_list_store_append (sp->store, &iter);
  gtk_list_store_set (sp->store, &iter,
		      BOOKMARK_COLUMN_CHANNEL,		channel,
		      BOOKMARK_COLUMN_PGNO,		bookmark->pgno,
		      BOOKMARK_COLUMN_SUBNO,		bookmark->subno,
		      BOOKMARK_COLUMN_DESCRIPTION,	description,
		      BOOKMARK_COLUMN_EDITABLE,	TRUE,
		      -1);
}

static void
on_bookmark_dialog_remove_clicked
				(GtkWidget *		widget,
				 bookmark_dialog *	sp)
{
  z_tree_view_remove_selected (sp->tree_view, sp->selection,
			       GTK_TREE_MODEL (sp->store));
}

static void
on_bookmark_dialog_selection_changed
				(GtkTreeSelection *	selection,
				 bookmark_dialog *	sp)
{
  GtkTreeIter iter;
  gboolean selected;

  selected = z_tree_selection_iter_first (selection,
					  GTK_TREE_MODEL (sp->store),
					  &iter);

  gtk_widget_set_sensitive (sp->remove, selected);
}

static void
on_bookmark_dialog_destroy	(GObject *		object,
				 bookmark_dialog *	sp)
{
  g_assert (sp == &bookmarks_dialog);

  g_object_unref (sp->store);

  CLEAR (*sp);
}

static void
on_bookmark_dialog_cancel_clicked
				(GtkWidget *		widget,
				 gpointer 		user_data)
{
  while (widget->parent)
    widget = widget->parent;

  gtk_widget_destroy (widget);
}

static gboolean
bookmark_dialog_foreach_add	(GtkTreeModel *		model,
				 GtkTreePath *		path,
				 GtkTreeIter *		iter,
				 gpointer		user_data)
{
  guint pgno;
  guint subno;
  gchar *channel;
  gchar *descr;

  gtk_tree_model_get (model, iter,
		      BOOKMARK_COLUMN_CHANNEL,		&channel,
		      BOOKMARK_COLUMN_PGNO,		&pgno,
		      BOOKMARK_COLUMN_SUBNO,		&subno,
		      BOOKMARK_COLUMN_DESCRIPTION,	&descr,
		      -1);

  bookmark_add (channel, pgno, subno, descr);

  return FALSE; /* continue */
}

static void
on_bookmark_dialog_ok_clicked	(GtkWidget *		button,
				 bookmark_dialog *	sp)
{
  bookmark_delete_all ();

  gtk_tree_model_foreach (GTK_TREE_MODEL (sp->store),
			  bookmark_dialog_foreach_add, sp);

  zmodel_changed (bookmarks_zmodel);

  on_bookmark_dialog_cancel_clicked (button, sp);
}

static GtkWidget *
bookmark_dialog_new		(ttxview_data *		data)
{
  bookmark_dialog *sp;
  GtkWindow *window;
  GtkBox *vbox;
  GtkWidget *scrolled_window;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkWidget *widget;
  GList *glist;

  sp = &bookmarks_dialog;
  g_assert (!sp->dialog);

  widget = gtk_dialog_new ();
  window = GTK_WINDOW (widget);
  sp->dialog = GTK_DIALOG (widget);
  gtk_window_set_title (window, _("Bookmarks"));
  gtk_window_set_transient_for (window, GTK_WINDOW (data->parent));

  g_signal_connect (G_OBJECT (widget), "destroy",
		    G_CALLBACK (on_bookmark_dialog_destroy), sp);

  widget = gtk_vbox_new (FALSE, 3);
  gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
  vbox = GTK_BOX (widget);
  gtk_box_pack_start (GTK_BOX (sp->dialog->vbox), widget, TRUE, TRUE, 0);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
				       GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (vbox, scrolled_window, TRUE, TRUE, 0);

  sp->store = gtk_list_store_new (BOOKMARK_NUM_COLUMNS,
				  G_TYPE_STRING,
				  G_TYPE_UINT,
				  G_TYPE_UINT,
				  G_TYPE_STRING,
				  G_TYPE_BOOLEAN);

  for (glist = bookmarks; glist; glist = glist->next)
    bookmark_dialog_append (sp, (struct bookmark *) glist->data);

  widget = gtk_tree_view_new_with_model (GTK_TREE_MODEL (sp->store));
  sp->tree_view = GTK_TREE_VIEW (widget);

  gtk_tree_view_set_rules_hint (sp->tree_view, TRUE);
  gtk_tree_view_set_reorderable (sp->tree_view, TRUE);
  gtk_tree_view_set_search_column (sp->tree_view,
				   BOOKMARK_COLUMN_DESCRIPTION);

  gtk_container_add (GTK_CONTAINER (scrolled_window), widget);

  sp->selection = gtk_tree_view_get_selection (sp->tree_view);
  gtk_tree_selection_set_mode (sp->selection, GTK_SELECTION_MULTIPLE);

  g_signal_connect (G_OBJECT (sp->selection), "changed",
  		    G_CALLBACK (on_bookmark_dialog_selection_changed), sp);

  column = gtk_tree_view_column_new_with_attributes
    (_("Channel"), gtk_cell_renderer_text_new (),
     "text", BOOKMARK_COLUMN_CHANNEL,
     NULL);
  gtk_tree_view_append_column (sp->tree_view, column);

  gtk_tree_view_insert_column_with_data_func
    (sp->tree_view, -1, _("Page"), gtk_cell_renderer_text_new (),
     bookmark_dialog_page_cell_data_func, NULL, NULL);

  renderer =  gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes
    (_("Description"), renderer,
     "text",	 BOOKMARK_COLUMN_DESCRIPTION,
     "editable", BOOKMARK_COLUMN_EDITABLE,
     NULL);
  gtk_tree_view_append_column (sp->tree_view, column);

  g_signal_connect (renderer, "edited",
		    G_CALLBACK (on_bookmark_dialog_descr_cell_edited), sp);

  widget = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
  sp->remove = widget;
  gtk_box_pack_start (vbox, widget, FALSE, FALSE, 0);
  gtk_widget_set_sensitive (widget, FALSE);

  g_signal_connect (G_OBJECT (widget), "clicked",
  		    G_CALLBACK (on_bookmark_dialog_remove_clicked), sp);

  widget = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
  gtk_dialog_add_action_widget (sp->dialog, widget, 1);
  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (widget), "clicked",
  		    G_CALLBACK (on_bookmark_dialog_cancel_clicked), sp);

  widget = gtk_button_new_from_stock (GTK_STOCK_OK);
  gtk_dialog_add_action_widget (sp->dialog, widget, 2);
  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (widget), "clicked",
  		    G_CALLBACK (on_bookmark_dialog_ok_clicked), sp);

  return GTK_WIDGET (sp->dialog);
}

static void
on_add_bookmark_activate	(GtkWidget *		menu_item,
				 ttxview_data *		data)
{
  struct bookmark *bookmark;
  tveng_tuned_channel *channel;
  vbi_pgno pgno;
  vbi_subno subno;
  gchar *description;
  vbi_decoder *vbi;
  gchar title[48];

  channel = tveng_tuned_channel_nth (global_channel_list, cur_tuned_channel);

  if (data->page >= 0x100)
    pgno = data->page;
  else
    pgno = data->fmt_page->pgno;

  subno = data->monitored_subpage;

  description = NULL;

  vbi = zvbi_get_object ();

  if (vbi && vbi_page_title (vbi, pgno, subno, title))
    {
      /* FIXME the title encoding should be UTF-8, but isn't
	 really defined. g_convert returns NULL on failure. */
      description = g_convert (title, strlen (title),
			       "UTF-8", "ISO-8859-1",
			       NULL, NULL, NULL);
    }

  bookmark = bookmark_add (channel ? channel->name : NULL,
			   pgno, subno, description);

  g_free (description);

  if (bookmarks_dialog.dialog)
    bookmark_dialog_append (&bookmarks_dialog, bookmark);

  zmodel_changed (bookmarks_zmodel);

  if (data->appbar)
    {
      gchar *buffer;

      if (subno && subno != VBI_ANY_SUBNO)
	buffer = g_strdup_printf (_("Added page %x.%02x to bookmarks"),
				  pgno, subno);
      else
	buffer = g_strdup_printf (_("Added page %x to bookmarks"), pgno);

      gnome_appbar_set_status (GNOME_APPBAR (data->appbar), buffer);

      g_free (buffer);
    }
}

static void
on_edit_bookmarks_activate	(GtkWidget *		menu_item,
				 ttxview_data *		data)
{
  GtkWidget *widget;

  if (bookmarks_dialog.dialog)
    gtk_window_present (GTK_WINDOW (bookmarks_dialog.dialog));
  else if ((widget = bookmark_dialog_new (data)))
    gtk_widget_show_all (widget);
}

static void
on_bookmark_menu_item_activate	(GtkWidget *		menu_item,
				 ttxview_data *		data)
{
  struct bookmark *bookmark;
  GList *glist;

  bookmark = g_object_get_data (G_OBJECT (menu_item), "bookmark");

  for (glist = bookmarks; glist; glist = glist->next)
    if (glist->data == bookmark)
      break;

  if (!glist)
    return;

  if (main_info
      && global_channel_list
      && bookmark->channel)
    {
      tveng_tuned_channel *channel;

      channel = tveng_tuned_channel_by_name (global_channel_list,
					     bookmark->channel);
      if (channel)
	z_switch_channel (channel, main_info);
    }

  load_page (data, bookmark->pgno, bookmark->subno, NULL);
}

static GnomeUIInfo
bookmarks_uiinfo [] = {
  {
    GNOME_APP_UI_ITEM, N_("_Add Bookmark"), NULL,
    G_CALLBACK (on_add_bookmark_activate), NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_ADD,
    GDK_D, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Edit Bookmarks..."), NULL,
    G_CALLBACK (on_edit_bookmarks_activate), NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    GDK_B, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  GNOMEUIINFO_END
};

static GtkWidget *
bookmarks_menu_new		(ttxview_data *		data)
{
  GtkMenuShell *menu;
  GtkWidget *widget;
  GList *glist;

  menu = GTK_MENU_SHELL (gtk_menu_new ());

  bookmarks_uiinfo[0].user_data = data;
  bookmarks_uiinfo[1].user_data = data;

  gnome_app_fill_menu (menu, bookmarks_uiinfo,
		       /* accel */ NULL,
		       /* mnemo */ TRUE,
		       /* position */ 0);

  if (!bookmarks)
    return GTK_WIDGET (menu);

  widget = gtk_separator_menu_item_new ();
  gtk_widget_show (widget);
  gtk_menu_shell_append (menu, widget);

  for (glist = bookmarks; glist; glist = glist->next)
    {
      struct bookmark *bookmark;
      GtkWidget *menu_item;
      gchar *buffer;
      gchar *channel;

      bookmark = (struct bookmark * ) glist->data;

      if (bookmark->subno != VBI_ANY_SUBNO)
	buffer = "%s%s%x.%x";
      else
	buffer = "%s%s%x";

      channel = bookmark->channel;

      if (channel && !*channel)
	channel = NULL;

      buffer = g_strdup_printf (buffer,
				channel ? channel : "",
				channel ? " " : "",
				bookmark->pgno,
				bookmark->subno);

      if (bookmark->description && *bookmark->description)
	{
	  menu_item = z_gtk_pixmap_menu_item_new (bookmark->description,
						  GTK_STOCK_JUMP_TO);
	  z_tooltip_set (menu_item, buffer);
	}
      else
	{
	  menu_item = z_gtk_pixmap_menu_item_new (buffer, GTK_STOCK_JUMP_TO);
	}

      gtk_widget_show (menu_item);

      g_object_set_data (G_OBJECT (menu_item), "bookmark", bookmark);
      g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (on_bookmark_menu_item_activate), data);

      gtk_menu_shell_append (menu, menu_item);

      g_free (buffer);
    }

  return GTK_WIDGET (menu);
}

GtkWidget *
ttxview_bookmarks_menu_new	(GtkWidget *		widget)
{
  ttxview_data *data;

  if ((data = ttxview_data_from_widget (widget)))
    return bookmarks_menu_new (data);
  else
    return NULL;
}

/*
 *  Search
 */

enum {
  SEARCH_RESPONSE_BACK = 1,
  SEARCH_RESPONSE_FORWARD,
};

/* Substitute keywords by regex, returns a newly allocated string,
   and g_free's the given string. */
static gchar *
search_dialog_subtitute		(gchar *		string)
{
  static const gchar *search_keys [][2] = {
    { "#email#", "([:alnum:]|[-~.])+@([:alnum:]|[-~.])+" },
    { "#url#", "(https?://([:alnum:]|[-~./?%_=+])+)|(www.([:alnum:]|[-~./?%_=+])+)" }
  };
  guint i;

  if (!string || !*string)
    {
      g_free (string);
      return g_strdup ("");
    }

  for (i = 0; i < 2; ++i)
    {
      gchar *found;

      while ((found = strstr (string, search_keys[i][0])))
	{
	  gchar *p;

	  *found = 0;

	  p = g_strconcat (string, search_keys[i][1],
			   found + strlen (search_keys[i][0]), NULL);
	  g_free (string);
	  string = p;
	}
    }

  return string;
}

static void
search_dialog_result		(search_dialog *	sp,
				 const gchar *		format,
				 ...)
{
  gchar *buffer;
  va_list args;

#if 1
  gdk_window_set_cursor (GTK_WIDGET (sp->dialog)->window, cursor_normal);
  gtk_widget_set_sensitive (GTK_WIDGET (sp->dialog), TRUE);
#else
  gtk_widget_set_sensitive (sp->entry, TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (sp->regexp), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (sp->casefold), TRUE);
  gtk_widget_set_sensitive (sp->back, TRUE);
  gtk_widget_set_sensitive (sp->forward, TRUE);

  if (sp->direction < 0)
    gtk_dialog_set_default_response (sp->dialog, SEARCH_RESPONSE_BACK);
  else
    gtk_dialog_set_default_response (sp->dialog, SEARCH_RESPONSE_FORWARD);
#endif

  va_start (args, format);
  buffer = g_strdup_vprintf (format, args);
  va_end (args);

  gtk_label_set_text (sp->label, buffer);

  g_free (buffer);
}

static gboolean
search_dialog_idle		(gpointer		user_data)
{
  search_dialog *sp = user_data;
  vbi_search_status status;
  vbi_page *pg;
  gboolean call_again;

#if 1
  gdk_window_set_cursor (GTK_WIDGET (sp->dialog)->window, cursor_busy);
  gtk_widget_set_sensitive (GTK_WIDGET (sp->dialog), FALSE);
#else
  gtk_widget_set_sensitive (sp->entry, FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (sp->regexp), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (sp->casefold), FALSE);
  gtk_widget_set_sensitive (sp->back, FALSE);
  gtk_widget_set_sensitive (sp->forward, FALSE);

  gtk_dialog_set_default_response (sp->dialog, GTK_RESPONSE_CANCEL);
#endif

  gtk_label_set_text (sp->label, _("Search text:"));

  status = vbi_search_next (sp->context, &pg, sp->direction);

  switch (status)
    {
    case VBI_SEARCH_SUCCESS:
      load_page (sp->data, pg->pgno, pg->subno, pg);
      search_dialog_result (sp, _("Found text on page %x.%02x:"),
			    pg->pgno, pg->subno);
      call_again = FALSE;
      sp->searching = FALSE;
      break;

    case VBI_SEARCH_NOT_FOUND:
      search_dialog_result (sp, _("Not found:"));
      call_again = FALSE;
      sp->searching = FALSE;
      break;

    case VBI_SEARCH_CANCELED:
      /* Events pending, handle them and continue. */
      call_again = TRUE;
      break;

    case VBI_SEARCH_CACHE_EMPTY:
      search_dialog_result (sp, _("Page memory is empty"));
      call_again = FALSE;
      sp->searching = FALSE;
      break;

    default:
      g_message ("Unknown search status %d in %s",
		 status, __PRETTY_FUNCTION__);

      /* fall through */

    case VBI_SEARCH_ERROR:
      call_again = FALSE;
      sp->searching = FALSE;
      break;
    }

  return call_again;
}

static void
on_search_dialog_destroy	(GObject *		object,
				 search_dialog *	sp)
{
  if (sp->searching)
    g_idle_remove_by_data (sp);

  if (sp->context)
    vbi_search_delete (sp->context);

  g_free (sp->text);

  sp->data->search_dialog = NULL;

  g_free (sp);
}

static void
search_dialog_continue		(search_dialog *	sp,
				 gint			direction)
{
  gchar *text;

  text = (gchar *) gtk_entry_get_text (GTK_ENTRY (sp->entry));

  if (!text || !*text)
    {
      /* Search for what? */
      gtk_window_present (GTK_WINDOW (sp->dialog));
      gtk_widget_grab_focus (sp->entry);
      return;
    }

  text = g_strdup (text);

  if (!sp->text || 0 != strcmp (sp->text, text))
    {
      uint16_t *pattern;
      const gchar *s;
      guint i;

      g_free (sp->text);
      sp->text = g_strdup (text);

      zcs_bool (gtk_toggle_button_get_active (sp->regexp), "ure_regexp");
      zcs_bool (gtk_toggle_button_get_active (sp->casefold), "ure_casefold");

      text = search_dialog_subtitute (text);

      /* I don't trust g_convert() to convert to the
	 machine endian UCS2 we need, hence g_utf8_foo. */

      pattern = g_malloc (strlen (text) * 2 + 2);

      i = 0;

      for (s = text; *s; s = g_utf8_next_char (s))
	pattern[i++] = g_utf8_get_char (s);

      pattern[i] = 0;

      vbi_search_delete (sp->context);

      /* Progress callback: Tried first with, to permit the user cancelling
	 a running search. But it seems there's a bug in libzvbi,
	 vbi_search_next does not properly resume after the progress
	 callback aborted to handle pending gtk events. Calling gtk main
         from callback is suicidal. Another bug: the callback lacks a
         user_data parameter. */
      sp->context = vbi_search_new (zvbi_get_object (),
				    0x100, VBI_ANY_SUBNO,
				    pattern,
				    zcg_bool (NULL, "ure_casefold"),
				    zcg_bool (NULL, "ure_regexp"),
				    /* progress */ NULL);
      g_free (pattern);
    }

  g_free (text);

  sp->direction = direction;

  g_idle_add (search_dialog_idle, sp);

  sp->searching = TRUE;
}

static void
on_search_dialog_next_clicked	(GtkButton *		button,
				 search_dialog *	sp)
{
  search_dialog_continue (sp, +1);
}

static void
on_search_dialog_prev_clicked	(GtkButton *		button,
				 search_dialog *	sp)
{
  search_dialog_continue (sp, -1);
}

static void
on_search_dialog_cancel_clicked	(GtkWidget *		button,
				 search_dialog *	sp)
{
  if (sp->dialog)
    {
      gtk_widget_destroy (GTK_WIDGET (sp->dialog));
    }
}

#if 0 /* to do */
static void
on_search_dialog_help_clicked	(GtkWidget *		button,
				 search_dialog *	sp)
{
  gnome_help_display ("ure.html", NULL, NULL); 
}
#endif

static GtkWidget *
search_dialog_new		(ttxview_data *		data)
{
  search_dialog *sp;
  GtkWidget *widget;
  GtkWindow *window;
  GtkBox *vbox;

  if (!zvbi_get_object())
    {
      ShowBox (_("VBI has been disabled"), GTK_MESSAGE_WARNING);
      return NULL;
    }

  if ((sp = data->search_dialog))
    {
      gtk_window_present (GTK_WINDOW (sp->dialog));
      return GTK_WIDGET (sp->dialog);
    }

  sp = g_malloc0 (sizeof (*sp));

  data->search_dialog = sp;
  sp->data = data;

  widget = gtk_dialog_new ();
  window = GTK_WINDOW (widget);
  sp->dialog = GTK_DIALOG (widget);

  gtk_window_set_title (window, _("Search page memory"));
  gtk_window_set_transient_for (window, GTK_WINDOW (data->parent));

  g_signal_connect (G_OBJECT (widget), "destroy",
		    G_CALLBACK (on_search_dialog_destroy), sp);

  widget = gtk_vbox_new (FALSE, 0);
  vbox = GTK_BOX (widget);
  gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
  gtk_box_pack_start (GTK_BOX (sp->dialog->vbox), widget, TRUE, TRUE, 0);

  widget = gtk_label_new (_("Search text:"));
  sp->label = GTK_LABEL (widget);
  gtk_box_pack_start (vbox, widget, FALSE, FALSE, 3);

  widget = gnome_entry_new ("ure_search_history");
  sp->entry = gnome_entry_gtk_entry (GNOME_ENTRY (widget));
  gtk_entry_set_activates_default (GTK_ENTRY (sp->entry), TRUE);
  gtk_box_pack_start (vbox, widget, FALSE, FALSE, 3);

  widget = gtk_check_button_new_with_mnemonic (_("_Regular expression"));
  sp->regexp = GTK_TOGGLE_BUTTON (widget);
  gtk_toggle_button_set_active (sp->regexp, zcg_bool (NULL, "ure_regexp"));
  gtk_box_pack_start (vbox, widget, FALSE, FALSE, 3);

  widget = gtk_check_button_new_with_mnemonic (_("Search case _insensitive"));
  sp->casefold = GTK_TOGGLE_BUTTON (widget);
  gtk_toggle_button_set_active (sp->casefold, zcg_bool (NULL, "ure_casefold"));
  gtk_box_pack_start (vbox, widget, FALSE, FALSE, 3);

#if 0 /* to do */
  widget = gtk_button_new_from_stock (GTK_STOCK_HELP);
  gtk_dialog_add_action_widget (sp->dialog, widget, GTK_RESPONSE_HELP);
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (on_search_dialog_help_clicked), sp);
#endif

  widget = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
  gtk_dialog_add_action_widget (sp->dialog, widget, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (on_search_dialog_cancel_clicked), sp);

  widget = gtk_button_new_from_stock (GTK_STOCK_GO_BACK);
  sp->back = widget;
  gtk_dialog_add_action_widget (sp->dialog, widget, SEARCH_RESPONSE_BACK);
  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (on_search_dialog_prev_clicked), sp);

  widget = gtk_button_new_from_stock (GTK_STOCK_GO_FORWARD);
  sp->forward = widget;
  gtk_dialog_add_action_widget (sp->dialog, widget, SEARCH_RESPONSE_FORWARD);
  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (on_search_dialog_next_clicked), sp);

  gtk_dialog_set_default_response (sp->dialog, SEARCH_RESPONSE_FORWARD);

  gtk_widget_grab_focus (sp->entry);

  return GTK_WIDGET (sp->dialog);
}

static PyObject *
py_ttx_search			(PyObject *		self,
				 PyObject *		args)
{
  ttxview_data *data;
  GtkWidget *widget;

  if (!(data = ttxview_data_from_widget (python_command_widget ())))
    py_return_true;

  if ((widget = search_dialog_new (data)))
    gtk_widget_show_all (widget);

  py_return_true;
}

/*
 *  Browser history
 */

/* Note history.stack[top - 1] is the current page. */

static void
history_dump			(ttxview_data *		data)
{
  guint i;

  fprintf (stderr, "top=%u size=%u ",
	   data->history.top,
	   data->history.size);

  for (i = 0; i < data->history.size; ++i)
    fprintf (stderr, "%03x ",
	     data->history.stack[i].pgno);

  fputc ('\n', stderr);
}

static void
history_update_gui		(ttxview_data *		data)
{
  gtk_widget_set_sensitive (GTK_WIDGET (data->tool.prev),
			    data->history.top >= 2);

  gtk_widget_set_sensitive (GTK_WIDGET (data->tool.next),
			    data->history.top < data->history.size);
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
history_push			(ttxview_data *		data,
				 vbi_pgno		pgno,
				 vbi_subno		subno)
{
  guint top;

  top = data->history.top;

  if (pgno < 0x100 || pgno > 0x899) 
    return;

  if (top > 0)
    {
      if (same_page (data->history.stack[top - 1].pgno,
		     data->history.stack[top - 1].subno,
		     pgno, subno))
	  return; /* already stacked */

      if (top >= N_ELEMENTS (data->history.stack))
	{
	  top = N_ELEMENTS (data->history.stack) - 1;

	  memmove (data->history.stack,
		   data->history.stack + 1,
		   top * sizeof (*data->history.stack));
	}
      else if (data->history.size > top)
	{
	  if (same_page (data->history.stack[top].pgno,
			 data->history.stack[top].subno,
			 pgno, subno))
	    {
	      /* Apparently we move forward in history, no new page. */
	      data->history.top = top + 1;
	      history_update_gui (data);
	      return;
	    }

	  /* Will discard future branch. */
	}
    }

  data->history.stack[top].pgno = pgno;
  data->history.stack[top].subno = subno;

  ++top;

  data->history.top = top;
  data->history.size = top;

  history_update_gui (data);

  if (0)
    history_dump (data);
}

static PyObject *
py_ttx_history_prev		(PyObject *		self,
				 PyObject *		args)
{
  ttxview_data *data;
  guint top;

  if (!(data = ttxview_data_from_widget (python_command_widget ())))
    py_return_true;

  top = data->history.top;

  if (top < 2)
    py_return_false;

  data->history.top = top - 1;
  history_update_gui (data);

  load_page (data,
	     data->history.stack[top - 2].pgno,
	     data->history.stack[top - 2].subno,
	     NULL);

  py_return_true;
}

static PyObject *
py_ttx_history_next		(PyObject *		self,
				 PyObject *		args)
{
  ttxview_data *data;
  guint top;

  if (!(data = ttxview_data_from_widget (python_command_widget ())))
    py_return_true;

  top = data->history.top;

  if (top >= data->history.size)
    py_return_false;

  data->history.top = top + 1;
  history_update_gui (data);

  load_page (data,
	     data->history.stack[top].pgno,
	     data->history.stack[top].subno,
	     NULL);

  py_return_true;
}

/*
 *  Miscellaneous menu stuff
 */

static void
on_subtitle_menu_activate	(GtkWidget *		menu_item,
				 gpointer		user_data)
{
  gint pgno = GPOINTER_TO_INT (user_data);
  vbi_decoder *vbi;
  vbi_page_type classf;

  if (!(vbi = zvbi_get_object ()))
    return;

  classf = vbi_classify_page (vbi, pgno, NULL, NULL);

  if (classf == VBI_SUBTITLE_PAGE ||
      (classf == VBI_NORMAL_PAGE && (pgno >= 5 && pgno <= 8)))
    zmisc_overlay_subtitles (pgno);
}

static GnomeUIInfo
subtitles_uiinfo [] = {
  GNOMEUIINFO_END
};

GtkWidget *
ttxview_subtitles_menu_new	(void)
{
  vbi_decoder *vbi;
  GtkMenuShell *menu;
  vbi_pgno pgno;

  if (!(vbi = zvbi_get_object ()))
    return NULL;

  menu = NULL;

  for (pgno = 1; pgno <= 0x899;
       pgno = (pgno == 8) ? 0x100 : vbi_add_bcd (pgno, 0x001))
    {
      vbi_page_type classf;
      gchar *language;
      gchar *buffer;
      GtkWidget *menu_item;

      classf = vbi_classify_page (vbi, pgno, NULL, &language);

      if (classf != VBI_SUBTITLE_PAGE
	  && (pgno > 8 || classf != VBI_NORMAL_PAGE))
	  continue;

      if (language)
	language = g_convert (language, strlen (language),
			      "UTF-8", "ISO-8859-1",
			      NULL, NULL, NULL);

      if (pgno <= 8)
	{
	  if (language)
	    {
	      if (classf == VBI_SUBTITLE_PAGE)
		buffer = g_strdup_printf (("Caption %x - %s"),
					  pgno, language);
	      else
		buffer = g_strdup_printf (("Text %x - %s"),
					  pgno - 4, language);
	    }
	  else
	    if (classf == VBI_SUBTITLE_PAGE)
	      buffer = g_strdup_printf (("Caption %x"), pgno);
	    else
	      buffer = g_strdup_printf (("Text %x"), pgno - 4);
	}
      else
	{
	  if (language)
	    buffer = g_strdup (language);
	  else
	    buffer = g_strdup_printf (_("Page %x"), pgno);
	}

      menu_item = gtk_menu_item_new_with_label (buffer);
      g_free (buffer);

      gtk_widget_show (menu_item);

      if (pgno >= 0x100 && language)
	{
	  gchar buffer [32];

	  g_snprintf (buffer, sizeof (buffer), "%x", pgno);
	  z_tooltip_set (menu_item, buffer);
	}

      g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (on_subtitle_menu_activate),
			GINT_TO_POINTER (pgno));

      if (!menu)
	{
	  menu = GTK_MENU_SHELL (gtk_menu_new ());

	  /* Let's pick up some desktop defaults. */
	  gnome_app_fill_menu (menu, subtitles_uiinfo,
			       /* accel */ NULL,
			       /* mnemo */ TRUE,
			       /* position */ 0);
	}

      gtk_menu_shell_append (menu, menu_item);

      g_free (language);
    }

  return menu ? GTK_WIDGET (menu) : NULL;
}

#define ONCE(b) if (b) continue; else b = TRUE;

guint
ttxview_hotlist_menu_append	(GtkMenuShell *		menu,
				 gboolean		separator)
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

  for (pgno = 0x100; pgno <= 0x899; pgno = vbi_add_bcd (pgno, 0x001))
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
	  gtk_menu_shell_append (menu, menu_item);

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

      gtk_menu_shell_append (menu, menu_item);

      ++count;
    }

  return count;
}

/*

static void
ttxview_resize			(ttxview_data *		data,
				 guint			width,
				 guint			height,
				 gboolean		aspect)
{
  gint main_width;
  gint main_height;
  gint darea_width;
  gint darea_height;

  gdk_window_get_geometry (GTK_WIDGET (data->app)->window, NULL, NULL,
			   &main_width, &main_height, NULL);

  gdk_window_get_geometry (data->darea->window, NULL, NULL,
			   &darea_width, &darea_height, NULL);

  if (aspect)
    {
      height = darea_width * height / width;
    }

  width += main_width - darea_width;
  height += main_height - darea_height;

  gdk_window_resize (data->darea->window, width, height);
}

*/

/*
 *  Popup menu
 */

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
    G_CALLBACK (on_export_dialog_activate), NULL, NULL,
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
    GNOME_APP_UI_ITEM, N_("_Color..."), NULL,
    G_CALLBACK (on_color_dialog_activate), NULL, NULL,
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

static GtkWidget *
ttxview_popup_menu_new		(ttxview_data *		data,
				 vbi_link *		ld,
				 gboolean		large)
{
  GtkWidget *menu;
  GtkWidget *widget;
  guint pos;

  menu = gtk_menu_new ();
  g_object_set_data (G_OBJECT (menu), "ttxview_data", data);

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

	  gnome_app_fill_menu (GTK_MENU_SHELL (menu), popup_open_page_uiinfo,
			       /* accel */ NULL, /* mnemo */ TRUE, pos);
	  return menu;

	case VBI_LINK_HTTP:
	case VBI_LINK_FTP:
	case VBI_LINK_EMAIL:
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

  popup_page_uiinfo[1].user_data = data; /* save as */
  popup_page_uiinfo[3].user_data = data; /* color */

  gnome_app_fill_menu (GTK_MENU_SHELL (menu), popup_page_uiinfo,
		       NULL, TRUE, pos);

  if (!vbi_export_info_enum (0))
    gtk_widget_set_sensitive (popup_page_uiinfo[1].widget, FALSE);

  if (large)
    {
      GtkWidget *subtitles_menu;

      widget = popup_page_uiinfo[4].widget; /* subtitles */

      if ((subtitles_menu = ttxview_subtitles_menu_new ()))
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (widget), subtitles_menu);
      else
	gtk_widget_set_sensitive (widget, FALSE);

      widget = popup_page_uiinfo[5].widget; /* bookmarks */

      gtk_menu_item_set_submenu (GTK_MENU_ITEM (widget),
				 bookmarks_menu_new (data));

      ttxview_hotlist_menu_append (GTK_MENU_SHELL (menu),
				   /* separator */ TRUE);
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

GtkWidget *
ttxview_popup			(GtkWidget *		widget,
				 GdkEventButton *	event)
{
  ttxview_data *data;

  if ((data = ttxview_data_from_widget (widget)))
    {
      vbi_link ld;

      vbi_link_from_pointer_position (&ld, data, event->x, event->y);

      return ttxview_popup_menu_new (data, &ld, FALSE);
    }
  else
    {
      return NULL;
    }
}

/*
 *  Main menu
 */

static void
on_view_toolbar_toggled		(GtkCheckMenuItem *	check_menu_item,
				 gpointer		user_data)
{
  zcs_bool (check_menu_item->active, "view/toolbar");
}

static void
on_view_statusbar_toggled	(GtkCheckMenuItem *	check_menu_item,
				 gpointer		user_data)
{
  zcs_bool (check_menu_item->active, "view/statusbar");
}

static void
on_edit_preferences_activate	(GtkMenuItem *		menu_item,
				 gpointer		user_data)
{
  python_command_printf (GTK_WIDGET (menu_item),
			 "zapping.properties('%s', '%s')",
			 _("VBI Options"), _("General"));
}

static void
on_ttxview_close		(GtkWidget *		widget,
				 ttxview_data *		data)
{
  if (data->app)
    ttxview_delete (data);
  else
    zmisc_restore_previous_mode (main_info);
}

/*
static void
on_ttxview_quit			(GtkWidget *		widget,
				 ttxview_data *		data)
{
  python_command (widget, "zapping.quit()");
}
*/

static GnomeUIInfo
main_file_uiinfo [] = {
  {
    GNOME_APP_UI_ITEM, N_("_New Window"), NULL, 
    G_CALLBACK (on_python_command1), "zapping.ttx_open_new()", NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_NEW,
    GDK_N, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_SAVE_AS_ITEM (on_export_dialog_activate, NULL),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_CLOSE_ITEM (on_ttxview_close, NULL),
  /* XXX crashes
     GNOMEUIINFO_MENU_QUIT_ITEM (on_ttxview_quit, NULL), */
  GNOMEUIINFO_END
};

static GnomeUIInfo
main_edit_uiinfo [] = {
  {
    GNOME_APP_UI_ITEM, N_("_Search"), NULL, 
    G_CALLBACK (on_python_command1), "zapping.ttx_search()", NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_FIND,
    GDK_F, (GdkModifierType) GDK_CONTROL_MASK, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_PREFERENCES_ITEM (on_edit_preferences_activate, NULL),
  GNOMEUIINFO_END
};

static GnomeUIInfo
main_view_uiinfo [] = {
  GNOMEUIINFO_TOGGLEITEM (N_("_Toolbar"), NULL,
			  on_view_toolbar_toggled, NULL),
  GNOMEUIINFO_TOGGLEITEM (N_("_Statusbar"), NULL,
			  on_view_statusbar_toggled, NULL),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_ITEM_DATA (N_("_Color"), NULL,
			 on_color_dialog_activate, NULL, NULL),
  GNOMEUIINFO_END
};

static GnomeUIInfo
main_go_uiinfo [] = {
  {
    GNOME_APP_UI_ITEM, N_("_Back"), NULL, 
    G_CALLBACK (on_python_command1), "zapping.ttx_history_prev()", NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    GDK_Left, (GdkModifierType) GDK_MOD1_MASK, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Forward"), NULL, 
    G_CALLBACK (on_python_command1), "zapping.ttx_history_next()", NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    GDK_Right, (GdkModifierType) GDK_MOD1_MASK, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  {
    GNOME_APP_UI_ITEM, N_("_Home"), NULL,
    G_CALLBACK (on_python_command1), "zapping.ttx_home()", NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    GDK_Home, (GdkModifierType) GDK_MOD1_MASK, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo
main_uiinfo [] = {
  GNOMEUIINFO_MENU_FILE_TREE (main_file_uiinfo),
  GNOMEUIINFO_MENU_EDIT_TREE (main_edit_uiinfo),
  /* XXX doesn't work when window is opened
     GNOMEUIINFO_MENU_VIEW_TREE (main_view_uiinfo), */
  GNOMEUIINFO_SUBTREE (N_("_Go"), main_go_uiinfo),
  {
    GNOME_APP_UI_ITEM, N_("_Bookmarks"), NULL,
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

static void
on_main_menu_bookmarks_changed	(ZModel *		zmodel,
				 ttxview_data *		data)
{
  gtk_menu_item_set_submenu (data->bookmarks_menu,
			     bookmarks_menu_new (data));
}

static void
on_main_menu_bookmarks_destroy	(GObject *		object,
				 ttxview_data *		data)
{
  if (bookmarks_zmodel)
    g_signal_handlers_disconnect_matched
      (G_OBJECT (bookmarks_zmodel), G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
       0, 0, NULL, G_CALLBACK (on_main_menu_bookmarks_changed), data);
}

static void
ttxview_create_main_menu	(ttxview_data *		data)
{
  main_file_uiinfo[2].user_data = data;
  main_file_uiinfo[4].user_data = data;
  main_file_uiinfo[5].user_data = data;

  main_view_uiinfo[3].user_data = data;

  gnome_app_create_menus (data->app, main_uiinfo);

  /* For ttxview_data_from_widget(). main_uiinfo[x].widget is
     no parent of the submenu. */
  g_object_set_data (G_OBJECT (main_file_uiinfo[0].widget->parent),
		     "ttxview_data", data);
  g_object_set_data (G_OBJECT (main_edit_uiinfo[0].widget->parent),
		     "ttxview_data", data);
  g_object_set_data (G_OBJECT (main_go_uiinfo[0].widget->parent),
		     "ttxview_data", data);

  if (!vbi_export_info_enum (0))
    gtk_widget_set_sensitive (main_file_uiinfo[2].widget, FALSE);

  /*
  gtk_check_menu_item_set_active
    (GTK_CHECK_MENU_ITEM (main_view_uiinfo[0].widget),
     zcg_bool (NULL, "view/toolbar"));

  gtk_check_menu_item_set_active
    (GTK_CHECK_MENU_ITEM (main_view_uiinfo[1].widget),
     zcg_bool (NULL, "view/statusbar"));
  */

  data->bookmarks_menu = GTK_MENU_ITEM (main_uiinfo[3].widget);

  gtk_menu_item_set_submenu (data->bookmarks_menu,
			     bookmarks_menu_new (data));

  /* This is plain stupid, but I see no way to create the bookmarks
     submenu on the fly, when the menu is opened by the user. Also
     for this reason there is no Go > (subtitles) or Go > (hotlist).
     There just is no signal to update when these change. */
  g_signal_connect (G_OBJECT (bookmarks_zmodel), "changed",
		    G_CALLBACK (on_main_menu_bookmarks_changed), data);
  g_signal_connect (G_OBJECT (data->bookmarks_menu), "destroy",
		    G_CALLBACK (on_main_menu_bookmarks_destroy), data);
}

/*****************************************************************************/

static void
on_vbi_model_changed		(ZModel *		zmodel,
				 ttxview_data *		data)
{
  if (data->app)
    ttxview_delete (data);
}

static void
on_color_zmodel_changed		(ZModel	*		zmodel,
				 ttxview_data *		data)
{
  load_page (data, data->page, data->subpage, NULL);
}

static gboolean
on_ttxview_delete_event		(GtkWidget *		widget,
				 GdkEvent *		event,
				 ttxview_data *		data)
{
  ttxview_delete (data);
  return FALSE;
}

static
void on_ttxview_size_allocate		(GtkWidget	*widget,
					 GtkAllocation	*allocation,
					 ttxview_data	*data)
{
  scale_image (widget, allocation->width, allocation->height, data);
}

static gboolean
zvbi_timeout			(gpointer		user_data)
{
  ttxview_data *data = user_data;
  ttx_message_data msg_data;
  enum ttx_message msg;

  while ((msg = peek_ttx_message (data->zvbi_client_id, &msg_data)))
    switch (msg)
      {
      case TTX_PAGE_RECEIVED:
	{
	  gint width;
	  gint height;

	  if (data->selecting)
	    continue;
/*
	  if (data->parent_toolbar &&
	      zconf_get_boolean(NULL,
				"/zapping/options/vbi/auto_overlay") &&
	      (data->fmt_page->screen_opacity == VBI_TRANSPARENT_SPACE ||
	       vbi_classify_page(zvbi_get_object(), data->fmt_page->pgno, NULL,
				 NULL) == VBI_SUBTITLE_PAGE))
	    {
	      zmisc_overlay_subtitles(data->fmt_page->pgno);
	      return TRUE;
	    }
*/
	  gdk_window_get_geometry (data->darea->window, NULL, NULL,
				   &width, &height, NULL);

	  gdk_window_clear_area_e (data->darea->window, 0, 0, width, height);

	  data->subpage = data->fmt_page->subno;

	  set_url (data, data->fmt_page->pgno, data->subpage);

	  history_push (data,
			data->fmt_page->pgno,
			data->monitored_subpage);

	  update_pointer (data);

	  break;
	}

      case TTX_NETWORK_CHANGE:
#if 0 /* temporarily disabled */
      case TTX_PROG_INFO_CHANGE:
      case TTX_TRIGGER:
#endif
	break;

      case TTX_CHANNEL_SWITCHED:
	/* Duh.
	gnome_appbar_set_status (data->appbar,
				 _("The cache has been cleared"));
	*/
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
  ttxview_data *data = user_data;

  refresh_ttx_page (data->zvbi_client_id, data->darea);

  return TRUE;
}

static gboolean
on_ttxview_expose_event		(GtkWidget *		widget,
				 GdkEventExpose *	event,
				 ttxview_data *		data)
{
  GdkRegion *region;

  /* XXX this prevents a segv, needs probably a fix elsewhere. */
  scale_image (widget,
	       widget->allocation.width,
	       widget->allocation.height,
	       data);

  render_ttx_page (data->zvbi_client_id,
		   widget->window,
		   widget->style->white_gc,
		   event->area.x, event->area.y,
		   event->area.x, event->area.y,
		   event->area.width, event->area.height);
  
  if (data->selecting && data->select.last_x != -1)
    {
      gint columns, rows; /* page */
      gint scol, srow; /* start */
      gint ccol, crow; /* current */

      select_positions (data,
			data->select.last_x,
			data->select.last_y,
			&columns, &rows,
			&scol, &srow,
			&ccol, &crow);

      region = gdk_region_rectangle (&event->area);

      select_transform (data,
			scol, srow,	/* src1 */
			ccol, crow,	/* src2 */
			data->select.table_mode,
			scol, srow,	/* dst2 */
			ccol, crow,	/* dst2 */
			data->select.table_mode,
			region);

      gdk_region_destroy (region);
    }

  return TRUE;
}

static gboolean
on_ttxview_motion_notify	(GtkWidget *		widget,
				 GdkEventMotion *	event,
				 ttxview_data *		data)
{
  if (data->selecting)
    select_update (data, event->x, event->y, event->state);
  else
    update_pointer (data);

  return FALSE;
}

static void
open_url			(vbi_link *		ld)
{
  GError *err = NULL;

  gnome_url_show (ld->url, &err);

  if (err)
    {
      /* TRANSLATORS: Cannot open URL (http, smtp). */
      ShowBox (_("Cannot open %s:\n%s"),
	       GTK_MESSAGE_ERROR,
	       ld->url, err->message);
      g_error_free (err);
    }
}

static gboolean
on_ttxview_button_press		(GtkWidget *		widget,
				 GdkEventButton *	event,
				 ttxview_data *		data)
{
  vbi_link ld;

  switch (event->button)
    {
    case 1: /* left button */
      if (event->state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK))
	{
	  select_start (data, event->x, event->y, event->state);
	}
      else
	{
	  vbi_link_from_pointer_position (&ld, data, event->x, event->y);

	  switch (ld.type)
	    {
	    case VBI_LINK_PAGE:
	    case VBI_LINK_SUBPAGE:
	      load_page (data, ld.pgno, ld.subno, NULL);
	      break;
	      
	    case VBI_LINK_HTTP:
	    case VBI_LINK_FTP:
	    case VBI_LINK_EMAIL:
	      open_url (&ld);
	      break;
	      
	    default:
	      select_start (data, event->x, event->y, event->state);
	      break;
	    }
	}

      return TRUE; /* handled */

    case 2: /* middle button, open link in new window */
      vbi_link_from_pointer_position (&ld, data, event->x, event->y);

      switch (ld.type)
        {
	case VBI_LINK_PAGE:
	case VBI_LINK_SUBPAGE:
	  python_command_printf (widget,
				 "zapping.ttx_open_new(%x, %d)",
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

    default: /* right button, context menu */
      if (data->app)
	{
	  GtkWidget *menu;

	  vbi_link_from_pointer_position (&ld, data, event->x, event->y);

	  if ((menu = ttxview_popup_menu_new (data, &ld, TRUE)))
	    gtk_menu_popup (GTK_MENU (menu),
			    NULL, NULL, NULL, NULL,
			    event->button, event->time);

	  return TRUE; /* handled */
	}

      break;
    }
  
  return FALSE; /* pass on */
}

static gboolean
on_ttxview_button_release	(GtkWidget *		widget,
				 GdkEventButton *	event,
				 ttxview_data *		data)
{
  if (data->selecting)
    select_stop (data);

  return FALSE;
}

static gboolean
on_ttxview_key_press		(GtkWidget *		widget,
				 GdkEventKey *		event,
				 ttxview_data *		data)
{
  guint digit;

  /* Loading a page takes time, possibly longer than the key
     repeat period, and repeated keys will stack up. This kludge
     effectively defers all loads until the key is released,
     and discards all but the last load request. */
  if (abs (data->last_key_press_event_time - event->time) < 100
      || event->length > 1)
    data->deferred_load = TRUE;

  data->last_key_press_event_time = event->time;

  digit = event->keyval - GDK_0;

  switch (event->keyval)
    {
    case GDK_KP_0 ... GDK_KP_9:
      digit = event->keyval - GDK_KP_0;

      /* fall through */

    case GDK_0 ... GDK_9:
      if (event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK))
	{
	  load_page (data, digit * 0x100, VBI_ANY_SUBNO, NULL);
	  return TRUE; /* handled, don't pass on */
	}

      if (data->page >= 0x100)
	data->page = 0;
      data->page = (data->page << 4) + digit;

      if (data->page >= 0x900)
        data->page = 0x900; /* TOP index page */

      if (data->page >= 0x100)
	{
	  load_page (data, data->page, VBI_ANY_SUBNO, NULL);
	}
      else
	{
	  ttx_freeze (data->zvbi_client_id);
	  set_url (data, data->page, 0);
	}

      return TRUE; /* handled */

    case GDK_S:
      /* Menu is "Save As", plain "Save" might be confusing. Now Save As
	 has accelerator Ctrl-Shift-S, omitting the Shift might be
	 confusing. But we accept Ctrl-S (of plain Save) here. */
      if (event->state & GDK_CONTROL_MASK)
	{
	  on_export_dialog_activate (NULL, data);
	  return TRUE; /* handled */
	}

      break;

    case GDK_Escape: /* XXX remove Esc */
      if (data->app) /* ignore if attached to main window */
	{
	  on_ttxview_close (NULL, data);
	  return TRUE; /* handled */
	}

      break;

    default:
      break;
    }

  /* Try user defined keys. */
  return on_user_key_press (widget, event, data)
    || on_picture_size_key_press (widget, event, data);
}

/*
 *  Toolbar
 */

#include "pixmaps/left.h"
#include "pixmaps/down.h"
#include "pixmaps/up.h"
#include "pixmaps/right.h"
#include "pixmaps/reveal.h"

static void
on_toolbar_hold_toggled		(GtkToggleButton *	button,
				 gpointer		user_data)
{
  python_command_printf (GTK_WIDGET (button),
			 "zapping.ttx_hold(%u)",
			 gtk_toggle_button_get_active (button));
}

static void
on_toolbar_reveal_toggled	(GtkToggleButton *	button,
				 gpointer		user_data)
{
  python_command_printf (GTK_WIDGET (button),
			 "zapping.ttx_reveal(%u)",
			 gtk_toggle_button_get_active (button));
}

static GtkWidget *
button_new_from_pixdata		(const GdkPixdata *	pixdata,
				 const gchar *		tooltip,
				 GtkReliefStyle		relief_style,
				 const gchar *		py_cmd)
{
  GtkWidget *button;
  GtkWidget *icon;

  icon = z_gtk_image_new_from_pixdata (pixdata);
  gtk_widget_show (icon);

  button = gtk_button_new ();
  gtk_widget_show (button);

  gtk_container_add (GTK_CONTAINER (button), icon);
  gtk_button_set_relief (GTK_BUTTON (button), relief_style);
  z_tooltip_set (button, tooltip);

  g_signal_connect (G_OBJECT (button), "clicked",
		    G_CALLBACK (on_python_command1),
		    (gpointer) py_cmd);

  return button;
}

static void
on_toolbar_orientation_changed	(GtkToolbar *		toolbar,
				 GtkOrientation		orientation,
				 ttxview_data *		data)
{
  GtkReliefStyle button_relief;
  GList *glist;
  GtkWidget *up;
  GtkWidget *down;
  GtkWidget *left;
  GtkWidget *right;

  while ((glist = data->tool.box1->children))
    gtk_container_remove (GTK_CONTAINER (data->tool.box1),
			  ((GtkBoxChild *) glist->data)->widget);

  while ((glist = data->tool.box2->children))
    gtk_container_remove (GTK_CONTAINER (data->tool.box2),
			  ((GtkBoxChild *) glist->data)->widget);

  button_relief = GTK_RELIEF_NORMAL;
  gtk_widget_style_get (GTK_WIDGET (toolbar), "button_relief",
			&button_relief, NULL);

  up = button_new_from_pixdata
    (&up_png, _("Next page"),
     button_relief, "zapping.ttx_page_incr(1)");
  down = button_new_from_pixdata
    (&down_png, _("Previous page"),
     button_relief, "zapping.ttx_page_incr(-1)");
  left = button_new_from_pixdata
    (&left_png, _("Previous subpage"),
     button_relief, "zapping.ttx_subpage_incr(-1)");
  right = button_new_from_pixdata
    (&right_png, _("Next subpage"),
     button_relief, "zapping.ttx_subpage_incr(1)");

  switch (orientation)
    {
    case GTK_ORIENTATION_HORIZONTAL:
      SWAP (up, left);

      /* fall through */

    case GTK_ORIENTATION_VERTICAL:
      gtk_box_pack_start (data->tool.box1, up,    FALSE, FALSE, 0);
      gtk_box_pack_start (data->tool.box1, down,  FALSE, FALSE, 0);
      gtk_box_pack_start (data->tool.box2, left,  FALSE, FALSE, 0);
      gtk_box_pack_start (data->tool.box2, right, FALSE, FALSE, 0);

      break;
    }
}

static void
ttxview_toolbar_detach		(ttxview_data *		data)
{
  guint i;

  for (i = 0; i < 8; ++i)
    {
      GList *glist;

      if ((glist = g_list_last (data->toolbar->children)))
	{
	  GtkToolbarChild *child = glist->data;

	  if (GTK_TOOLBAR_CHILD_SPACE == child->type)
	    {
	      gint position;

	      position = g_list_position (data->toolbar->children, glist);
	      gtk_toolbar_remove_space (data->toolbar, position);
	    }
	  else
	    {
	      gtk_container_remove (GTK_CONTAINER (data->toolbar),
				    child->widget);
	    }
	}
    }
}

static GtkWidget *
ttxview_toolbar_attach		(ttxview_data *		data,
				 GtkWidget *		widget,
				 gboolean		small)
{
  GtkToolbar *toolbar;
  GtkIconSize icon_size;
  GtkReliefStyle button_relief;

  toolbar = GTK_TOOLBAR (widget);

  icon_size = gtk_toolbar_get_icon_size (toolbar);

  button_relief = GTK_RELIEF_NORMAL;
  gtk_widget_ensure_style (widget);
  gtk_widget_style_get (widget, "button_relief", &button_relief, NULL);

  {
    widget = gtk_toolbar_insert_stock (toolbar, GTK_STOCK_GO_BACK,
				       _("Previous page in history"), NULL,
				       G_CALLBACK (on_python_command1),
				       "zapping.ttx_history_prev()", -1);
    data->tool.prev = GTK_BUTTON (widget);

    widget = gtk_toolbar_insert_stock (toolbar, GTK_STOCK_GO_FORWARD,
				       _("Next page in history"), NULL,
				       G_CALLBACK (on_python_command1),
				       "zapping.ttx_history_next()", -1);
    data->tool.next = GTK_BUTTON (widget);

    history_update_gui (data);
  }

  gtk_toolbar_append_item
    (toolbar, _("Index"), _("Go to the index page"), NULL,
     gtk_image_new_from_stock (GTK_STOCK_HOME, icon_size),
     G_CALLBACK (on_python_command1), "zapping.ttx_home()");

  if (!small)
    {
      gtk_toolbar_insert_stock (toolbar, GTK_STOCK_NEW,
				_("Open new Teletext window"), NULL,
				G_CALLBACK (on_python_command1),
				"zapping.ttx_open_new()", -1);

      gtk_toolbar_insert_stock (toolbar, GTK_STOCK_FIND,
				_("Search page memory"), NULL,
				G_CALLBACK (on_python_command1),
				"zapping.ttx_search()", -1);
    }

  widget = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (widget);
  data->tool.box1 = GTK_BOX (widget);
  gtk_toolbar_append_widget (toolbar, widget, NULL, NULL);

  {
    GtkWidget *frame;

    widget = gtk_toggle_button_new ();
    gtk_widget_show (widget);
    data->tool.hold = GTK_TOGGLE_BUTTON (widget);
    gtk_button_set_relief (GTK_BUTTON (widget), button_relief);
    gtk_toolbar_append_widget (toolbar, widget,
			       _("Hold the current subpage"), NULL);

    g_signal_connect (G_OBJECT (widget), "clicked",
		      G_CALLBACK (on_toolbar_hold_toggled), data);

    frame = gtk_frame_new (NULL);
    gtk_widget_show (frame);
    gtk_container_add (GTK_CONTAINER (widget), frame);

    widget = gtk_label_new (_("888.88"));
    gtk_widget_show (widget);
    data->tool.url = GTK_LABEL (widget);
    gtk_container_add (GTK_CONTAINER (frame), widget);
  }

  widget = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (widget);
  data->tool.box2 = GTK_BOX (widget);
  gtk_toolbar_append_widget (toolbar, widget, NULL, NULL);

  widget = gtk_toolbar_insert_element
    (toolbar, GTK_TOOLBAR_CHILD_TOGGLEBUTTON, NULL,
     _("Reveal"), _("Reveal concealed text"), NULL,
     z_gtk_image_new_from_pixdata (&reveal_png),
     G_CALLBACK (on_toolbar_reveal_toggled), data, -1);
  data->tool.reveal = GTK_TOGGLE_BUTTON (widget);
  gtk_toggle_button_set_active (data->tool.reveal, FALSE);

  g_signal_connect (G_OBJECT (toolbar), "orientation-changed",
		    G_CALLBACK (on_toolbar_orientation_changed), data);

  on_toolbar_orientation_changed
    (toolbar, gtk_toolbar_get_orientation (toolbar), data);

  return GTK_WIDGET (toolbar);
}

static GtkWidget *
ttxview_toolbar_new		(ttxview_data *		data)
{
  return ttxview_toolbar_attach (data, gtk_toolbar_new (), FALSE);
}

/*
 *  Drawing Area
 */

static void
ttxview_drawing_area_detach	(ttxview_data *		data,
				 GtkWidget *		widget)
{
  GObject *object;

  object = G_OBJECT (widget);

  g_signal_handlers_disconnect_matched
    (object, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
     0, 0, NULL, G_CALLBACK (on_ttxview_size_allocate), data);

  g_signal_handlers_disconnect_matched
    (object, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
     0, 0, NULL, G_CALLBACK (on_ttxview_expose_event), data);

  g_signal_handlers_disconnect_matched
    (object, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
     0, 0, NULL, G_CALLBACK (on_ttxview_motion_notify), data);

  g_signal_handlers_disconnect_matched
    (object, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
     0, 0, NULL, G_CALLBACK (on_ttxview_button_press), data);

  g_signal_handlers_disconnect_matched
    (object, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
     0, 0, NULL, G_CALLBACK (on_ttxview_button_release), data);

  g_signal_handlers_disconnect_matched
    (object, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
     0, 0, NULL, G_CALLBACK (on_ttxview_selection_get), data);

  g_signal_handlers_disconnect_matched
    (object, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
     0, 0, NULL, G_CALLBACK (on_ttxview_selection_clear), data);
}

static GtkWidget *
ttxview_drawing_area_attach	(ttxview_data *		data,
				 GtkWidget *		widget)
{
  GObject *object;

  gtk_widget_add_events (widget,
			 GDK_EXPOSURE_MASK |
			 GDK_POINTER_MOTION_MASK |
			 GDK_BUTTON_PRESS_MASK |
			 GDK_BUTTON_RELEASE_MASK |
			 GDK_KEY_PRESS_MASK |
			 GDK_KEY_RELEASE_MASK |
			 GDK_STRUCTURE_MASK |
			 0);

  object = G_OBJECT (widget);

  g_signal_connect (object, "size-allocate",
		    G_CALLBACK (on_ttxview_size_allocate), data);

  g_signal_connect (object, "expose-event",
		    G_CALLBACK (on_ttxview_expose_event), data);

  g_signal_connect (object, "motion-notify-event",
		    G_CALLBACK (on_ttxview_motion_notify), data);
  
  g_signal_connect (object, "button-press-event",
		    G_CALLBACK (on_ttxview_button_press), data);
  
  g_signal_connect (object, "button-release-event",
		    G_CALLBACK (on_ttxview_button_release), data);
  
  /* Selection */

  gtk_selection_add_targets (widget, GDK_SELECTION_PRIMARY,
			     clipboard_targets,
			     N_ELEMENTS (clipboard_targets));

  gtk_selection_add_targets (widget, GA_CLIPBOARD,
			     clipboard_targets,
			     N_ELEMENTS (clipboard_targets));

  g_signal_connect (object, "selection_clear_event",
		    G_CALLBACK (on_ttxview_selection_clear), data);

  g_signal_connect (object, "selection_get",
		    G_CALLBACK (on_ttxview_selection_get), data);

  return widget;
}

static GtkWidget *
ttxview_drawing_area_new	(ttxview_data *		data)
{
  return ttxview_drawing_area_attach (data, gtk_drawing_area_new ());
}

static void
ttxview_delete			(ttxview_data *		data)
{
  if (data->search_dialog)
    on_search_dialog_cancel_clicked (NULL, data->search_dialog);

  g_source_remove (data->zvbi_timeout_id);

  if (data->blink_timeout_id > 0)
    g_source_remove (data->blink_timeout_id);

  if (data->deferred.timeout_id > 0)
    g_source_remove(data->deferred.timeout_id);

  unregister_ttx_client (data->zvbi_client_id);

  if (data->scale.mask)
    g_object_unref (data->scale.mask);

  g_object_unref (data->xor_gc);

  if (data->select.in_clipboard)
    {
      if (gdk_selection_owner_get (GA_CLIPBOARD)
	  == data->darea->window)
	gtk_selection_owner_set (NULL, GA_CLIPBOARD,
				 GDK_CURRENT_TIME);
    }

  if (data->select.in_selection)
    {
      if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY)
	  == data->darea->window)
	gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY,
				 GDK_CURRENT_TIME);
    }

  g_signal_handlers_disconnect_matched
    (G_OBJECT (data->vbi_model), G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
     0, 0, NULL, G_CALLBACK (on_vbi_model_changed), data);

  if (color_zmodel)
    g_signal_handlers_disconnect_matched
      (G_OBJECT (color_zmodel), G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
       0, 0, NULL, G_CALLBACK (on_color_zmodel_changed), data);

  if (data->app)
    gtk_widget_destroy (GTK_WIDGET (data->app));

  g_free (data);

  dec_ttxview_count();
}

GtkWidget *
ttxview_new			(void)
{
  ttxview_data *data;
  GtkWidget *widget;
  GObject *object;

  if (!zvbi_get_object())
    {
      ShowBox("VBI couldn't be opened, Teletext won't work",
	      GTK_MESSAGE_ERROR);
      return NULL;
    }

  data = g_malloc0 (sizeof (*data));

  widget = gnome_app_new ("Zapping", "Zapzilla");
  data->app = GNOME_APP (widget);
  data->parent = widget;

  object = G_OBJECT (widget);

  g_object_set_data (object, "ttxview_data", data);

  gtk_widget_set_events (widget, GDK_KEY_PRESS_MASK);

  g_signal_connect (object, "delete-event",
		    G_CALLBACK (on_ttxview_delete_event), data);

  g_signal_connect (object, "key-press-event",
		     G_CALLBACK (on_ttxview_key_press), data);

  g_signal_connect (G_OBJECT (color_zmodel), "changed",
		    G_CALLBACK (on_color_zmodel_changed), data);

  ttxview_create_main_menu (data);

  {
    widget = ttxview_toolbar_new (data);
    gtk_widget_show (widget);

    data->toolbar = GTK_TOOLBAR (widget);

    gnome_app_set_toolbar (data->app, data->toolbar);

#if 0
    /* XXX Must hide the dock, not just the toolbar, hence widget->parent.
       Ugly hack? Maybe. But that's not the main problem. Hiding at
       this point doesn't work, the toolbar appears anyway. What is this
       BonoboDockItem gnome_app_add_toolbar() secretly allocates, and the
       (BonoboDockLayout *) GnomeApp.layout it is added to? They are
       implemented by libbonoboui, yet not documented. */
    /* tip: gnome_app_get_dock_item_by_name (app, GNOME_APP_TOOLBAR_NAME) */
    g_assert (BONOBO_IS_DOCK_ITEM (widget->parent));
    zconf_add_hook_while_alive (G_OBJECT (widget->parent),
				ZCONF_DOMAIN "view/toolbar",
				zconf_hook_widget_show, widget->parent,
				/* call now */ TRUE);
#endif
  }

  {
    widget = ttxview_drawing_area_new (data);
    data->darea = widget;
    gtk_widget_show (widget);

    gnome_app_set_contents (data->app, widget);

    gtk_widget_realize (widget);

    gdk_window_set_back_pixmap (widget->window, NULL, FALSE);

    data->xor_gc = gdk_gc_new (widget->window);
    gdk_gc_set_function (data->xor_gc, GDK_INVERT);
  }

  {
    widget = gnome_appbar_new (FALSE, TRUE, GNOME_PREFERENCES_NEVER);
    data->appbar = GNOME_APPBAR (widget);

    gnome_app_set_statusbar (data->app, widget);

#if 0
    /* XXX see toolbar, either both or none. */
    zconf_add_hook_while_alive (G_OBJECT (widget),
				ZCONF_DOMAIN "view/statusbar",
				zconf_hook_widget_show, widget,
				/* call now */ TRUE);
#endif
  }

  data->zvbi_client_id = register_ttx_client ();
  data->zvbi_timeout_id = g_timeout_add (100, (GSourceFunc)
					 zvbi_timeout, data);

  data->fmt_page = get_ttx_fmt_page (data->zvbi_client_id);

  data->vbi_model = zvbi_get_model();

  data->deferred_load = FALSE;
  data->deferred.timeout_id = -1;

  /* Callbacks */

  /* handle the destruction of the vbi object */
  g_signal_connect (G_OBJECT(data->vbi_model), "changed",
		    G_CALLBACK(on_vbi_model_changed),
		    data);

  data->blink_timeout_id =
    g_timeout_add (BLINK_CYCLE / 4, (GSourceFunc) blink_timeout, data);

  set_ttx_parameters(data->zvbi_client_id, data->reveal);

  gtk_widget_show (GTK_WIDGET (data->app));

  load_page(data, 0x100, VBI_ANY_SUBNO, NULL);

  inc_ttxview_count ();

  /* XXX Resizing the window to make sure it has a reasonable aspect doesn't
     work well because the window size isn't know at this point. Everything's
     in place except for the toolbar, which is added later, probably in the
     gtk mainloop. Some bonobo effect I suppose. */
  gtk_widget_set_size_request (GTK_WIDGET (data->darea), 260, 250);

  return GTK_WIDGET (data->app);
}

extern gboolean
on_zapping_key_press		(GtkWidget *		widget,
				 GdkEventKey *		event,
				 gpointer *		user_data);

/**
 * Detach the TTXView elements from the given window, does nothing
 * if the window isn't being used as a TTXView.
 */
void
ttxview_detach			(GtkWidget *		parent)
{
  ttxview_data *data;

  if (!(data = ttxview_data_from_widget (parent)))
    return;

  {
    /* Unredirect key-press-event */
    g_signal_handlers_disconnect_matched
      (G_OBJECT (data->parent), G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
       0, 0, NULL, G_CALLBACK (on_ttxview_key_press), data);

    g_signal_handlers_unblock_matched
      (G_OBJECT (data->parent), G_SIGNAL_MATCH_FUNC,
       0, 0, NULL, G_CALLBACK (on_zapping_key_press), NULL);
  }

  g_signal_handlers_disconnect_matched
    (G_OBJECT (data->parent), G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
     0, 0, NULL, G_CALLBACK (on_ttxview_delete_event), data);

  ttxview_toolbar_detach (data);

  ttxview_drawing_area_detach (data, data->darea);

  ttxview_delete (data);

  g_object_set_data (G_OBJECT (parent), "ttxview_data", NULL);
}

/**
 * Attach to a window making it a new Teletext view.
 * @parent: Toplevel window the teletext view will be in.
 * @dara: Drawing area we will be drawing to.
 * @toolbar: Toolbar to attach the TTXView controls to.
 * @appbar: Application bar to show messages, or NULL.
 */
void
ttxview_attach			(GtkWidget *		parent,
				 GtkWidget *		darea,
				 GtkWidget *		toolbar,
				 GtkWidget *		appbar)
{
  ttxview_data *data;

  if (!zvbi_get_object())
    {
      ShowBox("VBI couldn't be opened, Teletext won't work",
	      GTK_MESSAGE_ERROR);
      return;
    }

  if ((data = ttxview_data_from_widget (parent)))
    return; /* already used as TTXView */

  data = g_malloc0 (sizeof (*data));

  data->app = NULL;

  data->parent = parent;

  g_object_set_data (G_OBJECT (parent), "ttxview_data", data);

  gtk_widget_add_events (parent, GDK_KEY_PRESS_MASK);

  g_signal_connect (G_OBJECT (parent), "delete-event",
		    G_CALLBACK (on_ttxview_delete_event), data);

  {
    /* Redirect key-press-event */
    g_signal_handlers_block_matched
      (G_OBJECT (parent), G_SIGNAL_MATCH_FUNC,
       0, 0, NULL, G_CALLBACK (on_zapping_key_press), NULL);

    g_signal_connect (G_OBJECT (parent), "key-press-event",
		      G_CALLBACK (on_ttxview_key_press), data);
  }

  g_signal_connect (G_OBJECT (color_zmodel), "changed",
		    G_CALLBACK (on_color_zmodel_changed), data);

  data->toolbar = GTK_TOOLBAR (toolbar);
  gtk_toolbar_append_space (data->toolbar);
  ttxview_toolbar_attach (data, toolbar, TRUE);

  data->darea = ttxview_drawing_area_attach (data, darea);
  gdk_window_set_back_pixmap (data->darea->window, NULL, FALSE);
  data->xor_gc = gdk_gc_new (darea->window);
  gdk_gc_set_function (data->xor_gc, GDK_INVERT);

  data->appbar = GNOME_APPBAR (appbar);

  data->zvbi_client_id = register_ttx_client ();
  data->zvbi_timeout_id = g_timeout_add (100, (GSourceFunc)
					 zvbi_timeout, data);

  data->fmt_page = get_ttx_fmt_page (data->zvbi_client_id);

  data->vbi_model = zvbi_get_model();

  data->deferred_load = FALSE;
  data->deferred.timeout_id = -1;

  /* Callbacks */

  /* handle the destruction of the vbi object */
  g_signal_connect (G_OBJECT(data->vbi_model), "changed",
		    G_CALLBACK(on_vbi_model_changed),
		    data);

  data->blink_timeout_id =
    g_timeout_add (BLINK_CYCLE / 4, (GSourceFunc) blink_timeout, data);

  {
    gint width;
    gint height;

    gdk_window_get_geometry (data->darea->window, NULL, NULL,
			     &width, &height, NULL);

    if (width > 10 && height > 10)
      {
	resize_ttx_page (data->zvbi_client_id, width, height);
	render_ttx_page (data->zvbi_client_id, data->darea->window,
			 data->darea->style->white_gc,
			 0, 0, 0, 0,
			 width, height);
      }
  }

  set_ttx_parameters (data->zvbi_client_id, data->reveal);

  load_page(data, 0x100, VBI_ANY_SUBNO, NULL);

  inc_ttxview_count();
}

void
shutdown_ttxview		(void)
{
  /* XXX this should destroy all ttx child windows first. */

  if (bookmarks_dialog.dialog)
    {
      gtk_widget_destroy (GTK_WIDGET (bookmarks_dialog.dialog));
      bookmarks_dialog.dialog = NULL;
    }

  bookmark_save ();
  bookmark_delete_all ();

  g_object_unref (G_OBJECT (bookmarks_zmodel));
  bookmarks_zmodel = NULL;

  gdk_cursor_unref (cursor_select);
  gdk_cursor_unref (cursor_link);
  gdk_cursor_unref (cursor_normal);
  gdk_cursor_unref (cursor_busy);

  if (color_dialog)
    {
      gtk_widget_destroy (color_dialog);
      color_dialog = NULL;
    }

  g_object_unref (G_OBJECT (color_zmodel));
  color_zmodel = NULL;

  g_object_unref (G_OBJECT (ttxview_zmodel));
  ttxview_zmodel = NULL;
}

gboolean
startup_ttxview			(void)
{
  ttxview_zmodel = ZMODEL (zmodel_new ());
  g_object_set_data (G_OBJECT (ttxview_zmodel), "count", GINT_TO_POINTER (0));

  GA_CLIPBOARD = gdk_atom_intern ("CLIPBOARD", FALSE);

  color_zmodel = ZMODEL (zmodel_new ());

  cursor_normal	= gdk_cursor_new (GDK_LEFT_PTR);
  cursor_link	= gdk_cursor_new (GDK_HAND2);
  cursor_select	= gdk_cursor_new (GDK_XTERM);
  cursor_busy	= gdk_cursor_new (GDK_WATCH);

  zcc_bool (TRUE, "Show toolbar", "view/toolbar");
  zcc_bool (TRUE, "Show statusbar", "view/statusbar");

  zcc_char (g_get_home_dir(), "Export directory", "exportdir");

  zcc_bool (FALSE, "Search uses regular expression", "ure_regexp");
  zcc_bool (FALSE, "Search case insensitive", "ure_casefold");

  zcc_bool (FALSE, "Switch channel on bookmark selection", "bookmark_switch");

  zconf_create_integer (128, "Brightness", "/zapping/options/text/brightness");
  zconf_create_integer (64, "Contrast", "/zapping/options/text/contrast");

  bookmarks_zmodel = ZMODEL (zmodel_new ());

  bookmark_load ();

  cmd_register ("ttx_open", py_ttx_open, METH_VARARGS);
  cmd_register ("ttx_open_new", py_ttx_open_new, METH_VARARGS,
		("Open new Teletext window"), "zapping.ttx_open_new()");
  cmd_register ("ttx_page_incr", py_ttx_page_incr, METH_VARARGS,
		("Increment Teletext page number"), "zapping.ttx_page_incr(+1)",
		("Decrement Teletext page number"), "zapping.ttx_page_incr(-1)");
  cmd_register ("ttx_subpage_incr", py_ttx_subpage_incr, METH_VARARGS,
		("Increment Teletext subpage number"), "zapping.ttx_subpage_incr(+1)",
		("Decrement Teletext subpage number"), "zapping.ttx_subpage_incr(-1)");
  cmd_register ("ttx_home", py_ttx_home, METH_VARARGS,
		("Go to Teletext index page"), "zapping.ttx_home()");
  cmd_register ("ttx_hold", py_ttx_hold, METH_VARARGS,
		("Hold Teletext subpage"), "zapping.ttx_hold()");
  cmd_register ("ttx_reveal", py_ttx_reveal, METH_VARARGS,
		("Reveal concealed text"), "zapping.ttx_reveal()");
  cmd_register ("ttx_history_prev", py_ttx_history_prev, METH_VARARGS,
		("Previous Teletext page in history"), "zapping.ttx_history_prev()");
  cmd_register ("ttx_history_next", py_ttx_history_next, METH_VARARGS,
		("Next Teletext page in history"), "zapping.ttx_history_next()");
  cmd_register ("ttx_search", py_ttx_search, METH_VARARGS,
		("Teletext search"), "zapping.ttx_search()");

  return TRUE;
}

#endif /* HAVE_LIBZVBI */
