/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2001 Iñaki García Etxebarria
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

/*
 * GUI view for the Teletext data
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define V_(String) dgettext("zvbi", String)
#else
#  define V_(String) (String)
#endif

#ifdef HAVE_LIBZVBI

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
#include "../common/fifo.h"
#include "../common/ucs-2.h"
#include "osd.h"
#include "callbacks.h"
#include "remote.h"

/* graphics */
#include "../pixmaps/con.xpm" /* contrast */
#include "../pixmaps/brig.xpm" /* brightness */

/* Useful vars from Zapping */
extern gboolean flag_exit_program;
extern int cur_tuned_channel;
extern tveng_tuned_channel *global_channel_list;
extern tveng_device_info *main_info;
extern GtkWidget *main_window;
extern int zvbi_page;

/* Exported, notification of when do we create/destroy a view */
ZModel *ttxview_model = NULL;

#define ORC_BLOCK(X) (X)

static void inc_model_count(void)
{
  gint num_views =
    GPOINTER_TO_INT(g_object_get_data(G_OBJECT(ttxview_model), "user-data"));

  num_views++;

  g_object_set_data(G_OBJECT(ttxview_model), "user-data",
			   GINT_TO_POINTER(num_views));

  zmodel_changed(ttxview_model);
}

static void dec_model_count(void)
{
  gint num_views =
    GPOINTER_TO_INT(g_object_get_data(G_OBJECT(ttxview_model), "user-data"));

  num_views--;

  g_object_set_data(G_OBJECT(ttxview_model), "user-data",
	       GINT_TO_POINTER(num_views));

  zmodel_changed(ttxview_model);
}

#define BLINK_CYCLE 300 /* ms */

#define TXCOLOR_DOMAIN "/zapping/options/text/"

#define TOP_INDEX_PAGE 0x900

/* targets for the clipboard */
enum
{
  TARGET_STRING,
  TARGET_PIXMAP
};

static const GtkTargetEntry clip_targets[] =
{
  { "STRING", 0, TARGET_STRING },
  { "PIXMAP", 0, TARGET_PIXMAP }
};

static const gint n_clip_targets = sizeof(clip_targets) /
sizeof(clip_targets[0]);

/*
 * BUGS: No alpha yet.
 *       This is getting too big, maybe we should split it up.
 */
static GdkCursor	*hand=NULL;
static GdkCursor	*arrow=NULL;
static GdkCursor	*xterm=NULL;
static GtkWidget	*search_progress=NULL;

typedef struct {
  GdkBitmap		*mask;
  gint			w, h;
  GtkWidget		*da;
  int			id; /* TTX client id */
  guint			timeout; /* id */
  vbi_page		*fmt_page; /* current page, formatted */
  gint			page; /* page we are entering */
  gint			subpage; /* current subpage */
  GList			*history_stack; /* for back, etc... */
  gint			history_stack_size; /* items in history_stack */
  gint			history_sp; /* pointer in the stack */
  gint			monitored_subpage;
  gboolean		no_history; /* don't send to history next page */
  gboolean		hold; /* hold the current subpage */
  gint			pop_pgno, pop_subno; /* for popup */
  gboolean		in_link; /* whether the cursor is over a link */
  GtkWidget		*parent; /* toplevel window */
  GtkWidget		*appbar; /* appbar, or NULL */
  GtkWidget		*toolbar; /* toolbar */
  GtkWidget		*parent_toolbar; /* parent toolbar toolbar is in */
  gboolean		popup_menu; /* whether right-click shows a popup */
  GdkGC			*xor_gc; /* graphic context for the xor mask */
  gboolean		selecting; /* TRUE if we are selecting text */
  gint		        ssx, ssy; /* starting positions for the
				     selection */
  gint			osx, osy; /* old positions for the selection */
  gboolean		in_clipboard; /* in the "CLIPBOARD" clipboard */
  gboolean		in_selection; /* in the primary selection */
  vbi_page		clipboard_fmt_page; /* page that contains the
					   selection */
  gboolean		sel_table;
  gint			sel_col1, sel_row1, sel_col2, sel_row2;
  gint			trn_col1, trn_row1, trn_col2, trn_row2;
  gint			blink_timeout; /* timeout for refreshing the page */
  ZModel		*vbi_model; /* notifies when the vbi object is
				     destroyed */
  guint32		last_time; /* time of the last key event */
  gint			wait_timeout_id; /* GUI too fast, slowing down */
  guint			wait_page; /* last page requested */
  vbi_page		wait_pg; /* page we wait for */
  gboolean		wait_mode; /* waiting */
  
  GtkToolbarStyle	toolbar_style; /* previous style of the
					  toolbar (restored on detach) */
} ttxview_data;

struct bookmark {
  gint page;
  gint subpage;
  gchar *description;
  gchar *channel;
};

#define nullcheck() \
do { \
  if (!zvbi_get_object()) \
    { \
      ShowBox(_("VBI has been disabled"), GTK_MESSAGE_WARNING); \
      return; \
    } \
} while (0)

static GList	*bookmarks=NULL;
static ZModel	*model=NULL; /* for bookmarks */
static ZModel	*refresh=NULL; /* tell all the views to refresh */
static GdkAtom	clipboard_atom = GDK_NONE;



static void on_ttxview_search_clicked(GtkButton *, ttxview_data *);

static void
add_bookmark(gint page, gint subpage, const gchar *description,
	     const gchar *channel)
{
  struct bookmark *entry = (struct bookmark *)
    g_malloc(sizeof(struct bookmark));

  entry->page = page;
  entry->subpage = subpage;
  entry->description = g_strdup(description);
  if (channel)
    entry->channel = g_strdup(channel);
  else
    entry->channel = NULL;

  bookmarks = g_list_append(bookmarks, entry);
  zmodel_changed(model);
}

static void
remove_bookmark(gint index, gboolean changed)
{
  GList *node = g_list_nth(bookmarks, index);

  if (!node)
    return;

  g_free(((struct bookmark*)(node->data))->description);
  g_free(((struct bookmark*)(node->data))->channel);
  g_free(node->data);
  bookmarks = g_list_remove(bookmarks, node->data);

  if (changed)
    zmodel_changed(model);
}

static ttxview_data *
ttxview_data_from_widget		(GtkWidget *	widget)
{
  GtkWidget *parent;

  if ((parent = find_widget (widget, "ttxview"))
      || (parent = find_widget (widget, "zapping")))
    return (ttxview_data *)
      g_object_get_data (G_OBJECT (parent), "ttxview_data");

  return NULL;
}

static
void scale_image			(GtkWidget	*wid,
					 gint		w,
					 gint		h,
					 ttxview_data	*data)
{
  if ((data->w != w) ||
      (data->h != h))
    {
      if (data->mask)
	g_object_unref (G_OBJECT (data->mask));
      data->mask = gdk_pixmap_new(data->da->window, w, h, 1);
      g_assert(data->mask != NULL);
      resize_ttx_page(data->id, w, h);
      data->w = w;
      data->h = h;
    }
}

static
void setup_history_gui	(ttxview_data *data)
{
  GtkWidget *prev, *next, *prev_subpage, *next_subpage;

  prev = lookup_widget(data->toolbar, "ttxview_history_prev");
  next = lookup_widget(data->toolbar, "ttxview_history_next");
  prev_subpage = lookup_widget(data->toolbar, "ttxview_prev_subpage");
  next_subpage = lookup_widget(data->toolbar, "ttxview_next_subpage");

  if (data->history_stack_size > (data->history_sp+1))
    gtk_widget_set_sensitive(next, TRUE);
  else
    gtk_widget_set_sensitive(next, FALSE);
  
  if (data->history_sp > 0)
    gtk_widget_set_sensitive(prev, TRUE);
  else
    gtk_widget_set_sensitive(prev, FALSE);
}

static
void append_history	(int page, int subpage, ttxview_data *data)
{
  gint page_code, pc_subpage;

  pc_subpage = (subpage == VBI_ANY_SUBNO) ? 0xffff : (subpage & 0xffff);
  page_code = (page<<16)+pc_subpage;

  if ((!data->history_stack) ||
      (GPOINTER_TO_INT(g_list_nth(data->history_stack,
				  data->history_sp)->data) != page_code))
    {
      data->history_stack = g_list_append(data->history_stack,
					  GINT_TO_POINTER(page_code));
      data->history_stack_size++;
      data->history_sp = data->history_stack_size-1;
      setup_history_gui(data);
    }
}

static void
remove_ttxview_instance			(ttxview_data	*data);

static void
on_vbi_model_changed			(ZModel		*model,
					 ttxview_data	*data)
{
  GtkWidget *ttxview;

  if (!data->parent_toolbar) /* not attached, standalone window */
    {
      ttxview = lookup_widget(data->toolbar, "ttxview");
      remove_ttxview_instance(data);
      gtk_widget_destroy(ttxview);
    }
}

static gint wait_timeout (ttxview_data *data)
{
  int page, subpage;

  page = data->wait_page >> 8;
  subpage = data->wait_page & 0xff;
  if (subpage == (VBI_ANY_SUBNO & 0xff))
    subpage = VBI_ANY_SUBNO;

  data->wait_timeout_id = -1;

  if (data->wait_pg.pgno<=0)
    monitor_ttx_page(data->id, page, subpage);
  else
    monitor_ttx_this(data->id, &data->wait_pg);

  data->wait_mode = FALSE;

  return 0; /* don't call me again */
}

static void retarded_load(gint page, gint subpage,
			  ttxview_data *data, vbi_page *pg)
{

  data->wait_page = (page << 8) + (subpage & 0xff);
  if (pg)
    memcpy(&data->wait_pg, pg, sizeof(data->wait_pg));
  else
    data->wait_pg.pgno = -1;

  if (data->wait_timeout_id >= 0)
    {
      gtk_timeout_remove(data->wait_timeout_id);
      data->wait_timeout_id =
	gtk_timeout_add(300, (GtkFunction)wait_timeout, data);
      return;
    }

  if (data->wait_mode)
    {
      data->wait_timeout_id =
	gtk_timeout_add(300, (GtkFunction)wait_timeout, data); 
      return;
    }

  if (data->wait_pg.pgno<=0)
    monitor_ttx_page(data->id, page, subpage);
  else
    monitor_ttx_this(data->id, &data->wait_pg);
}

static void
set_hold				(ttxview_data *	data,
					 gboolean	hold);

static void
load_page (vbi_pgno page, vbi_subno subpage,
	   ttxview_data *data, vbi_page *pg)
{
  GtkWidget *ttxview_url = lookup_widget(data->toolbar, "ttxview_url");
  gchar *buffer;

  set_hold (data, data->hold = (subpage != VBI_ANY_SUBNO));

  data->subpage = subpage;
  data->page = page;
  data->monitored_subpage = subpage;
  if (subpage != VBI_ANY_SUBNO && subpage)
    buffer = g_strdup_printf("%d.%d", vbi_bcd2dec(page), vbi_bcd2dec(subpage));
  else
    buffer = g_strdup_printf("%d", vbi_bcd2dec(page));
  gtk_label_set_text(GTK_LABEL(ttxview_url), buffer);
  g_free(buffer);

  if ((page >= 0x100) && (page <= 0x900 /* 0x900 == top index */))
    {
      if (subpage == VBI_ANY_SUBNO)
	buffer = g_strdup_printf(_("Loading page %x..."), page);
      else
	buffer = g_strdup_printf(_("Loading subpage %x..."),
				 subpage);
    }
  else
    buffer = g_strdup_printf(_("Warning: Page not valid"));

  if (data->appbar)
    gnome_appbar_set_status(GNOME_APPBAR(data->appbar), buffer);
  g_free(buffer);

  gtk_widget_grab_focus(data->da);

  retarded_load(page, subpage, data, pg);

  z_update_gui();
}

static void
on_ttxview_refresh			(ZModel		*model,
					 ttxview_data	*data)
{
  load_page(data->page, data->subpage, data, NULL);
}

static void
remove_ttxview_instance			(ttxview_data	*data)
{
  if (data->mask)
    g_object_unref(data->mask);

  g_object_unref(data->xor_gc);

  if (data->in_clipboard)
    {
      if (gdk_selection_owner_get (clipboard_atom) ==
	  data->da->window)
	gtk_selection_owner_set (NULL, clipboard_atom,
				 GDK_CURRENT_TIME);
    }
  if (data->in_selection)
    {
      if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) ==
	  data->da->window)
	gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY,
				 GDK_CURRENT_TIME);
    }

  gtk_timeout_remove(data->blink_timeout);
  gtk_timeout_remove(data->timeout);
  if (data->wait_timeout_id > -1)
    gtk_timeout_remove(data->wait_timeout_id);

  unregister_ttx_client(data->id);

  g_signal_handlers_disconnect_matched(G_OBJECT(data->vbi_model),
				       G_SIGNAL_MATCH_FUNC |
				       G_SIGNAL_MATCH_DATA,
				       0, 0, NULL,
				       G_CALLBACK(on_vbi_model_changed),
				       data);
  if (refresh)
    g_signal_handlers_disconnect_matched(G_OBJECT(refresh),
					 G_SIGNAL_MATCH_FUNC |
					 G_SIGNAL_MATCH_DATA,
					 0, 0, NULL,
					 G_CALLBACK(on_ttxview_refresh),
					 data);
  g_free(data);

  dec_model_count();
}

static gboolean
on_ttxview_delete_event			(GtkWidget	*widget,
					 GdkEvent	*event,
					 ttxview_data	*data)
{
  remove_ttxview_instance(data);

  return FALSE;
}

/* Called when another application claims the selection */
static gint selection_clear		(GtkWidget	*widget,
					 GdkEventSelection *event,
					 ttxview_data	*data)
{
  if (event->selection == GDK_SELECTION_PRIMARY)
    data->in_selection = FALSE;
  else if (event->selection == clipboard_atom)
    data->in_clipboard = FALSE;

  return TRUE;
}

static void selection_handle		(GtkWidget	*widget,
					 GtkSelectionData *selection_data,
					 guint		info,
					 guint		time_stamp,
					 ttxview_data	*data)
{
  gint CW, CH;

  vbi_get_vt_cell_size(&CW, &CH);

  if (((selection_data->selection == GDK_SELECTION_PRIMARY) &&
       (data->in_selection)) ||
      ((selection_data->selection == clipboard_atom) &&
       (data->in_clipboard)))
    {
      if (info == TARGET_STRING)
	{
	  int width = data->sel_col2 - data->sel_col1 + 1;
	  int height = data->sel_row2 - data->sel_row1 + 1;
	  int actual = 0, size = width * height * 8;
	  char *buf = (char *) malloc (size);

	  if (buf) {
	    actual = vbi_print_page_region (&data->clipboard_fmt_page,
					    buf, size, "ISO-8859-1" /* OK? */,
					    data->sel_table, /* ltr */ TRUE,
					    data->sel_col1, data->sel_row1,
					    width, height);
	    if (actual > 0)
	      gtk_selection_data_set (selection_data,
				      GDK_SELECTION_TYPE_STRING, 8,
				      buf, actual);
	    free (buf);
	  }

	  if (actual <= 0)
	    g_warning (_("Text export failed"));
	}
      else if (info == TARGET_PIXMAP)
	{
	  gint width, height;
	  GdkPixmap *pixmap;
	  GdkPixbuf *canvas;
	  gint id[2];

	  /* Selection is open (eg. 25,5 - 15,6),
	     ok to simply bail out? */
	  if (data->sel_col2 < data->sel_col1)
	    return;

	  width = data->sel_col2 - data->sel_col1 + 1;
	  height = data->sel_row2 - data->sel_row1 + 1;
	  pixmap =
	    gdk_pixmap_new(data->da->window, width*CW, height*CH, -1);
	  canvas = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE,
					     8, width*CW, height*CH);

	  vbi_draw_vt_page_region(&data->clipboard_fmt_page,
				  VBI_PIXFMT_RGBA32_LE,
				  (uint32_t *) gdk_pixbuf_get_pixels(canvas),
				  data->sel_col1, data->sel_row1,
				  width, height,
				  gdk_pixbuf_get_rowstride(canvas),
				  zcg_bool(NULL, "reveal"), 1 /* flash_on */);
	  gdk_pixbuf_render_to_drawable(canvas, pixmap,
					data->da->style->white_gc, 0,
					0, 0, 0, width*CW, height*CH,
					GDK_RGB_DITHER_NORMAL, 0, 0);
	  id[0] = GDK_WINDOW_XWINDOW(pixmap);
	  gtk_selection_data_set (selection_data,
				  GDK_SELECTION_TYPE_PIXMAP, 32,
				  (char*)&id[0], 4);
	  g_object_unref(canvas);
	}
    }
}

static void
update_pointer (ttxview_data *data)
{
  gint x, y;
  GdkModifierType mask;
  gint w, h, col, row;
  gchar *buffer;
  GtkWidget *widget = data->da;

  gdk_window_get_pointer(widget->window, &x, &y, &mask);

  gdk_window_get_geometry(widget->window, NULL, NULL, &w, &h, NULL);

  if ((w <= 0) || (h <= 0))
    return;

  /* convert to fmt_page space */
  col = (x * data->fmt_page->columns) / w;
  row = (y * data->fmt_page->rows) / h;

  if ((col < 0) || (col >= data->fmt_page->columns)
      || (row < 0) || (row >= data->fmt_page->rows))
    return;

  if (data->fmt_page->text[row * data->fmt_page->columns + col].link)
    {
      vbi_link ld;

      vbi_resolve_link(data->fmt_page, col, row, &ld);

      switch (ld.type)
        {
	case VBI_LINK_PAGE:
	  buffer = g_strdup_printf(_(" Page %d"), vbi_bcd2dec(ld.pgno));
	  break;

	case VBI_LINK_SUBPAGE:
	  buffer = g_strdup_printf(_(" Subpage %d"),
				   vbi_bcd2dec(ld.subno & 0xFF));
	  break;

	case VBI_LINK_HTTP:
	case VBI_LINK_FTP:
	case VBI_LINK_EMAIL:
	  buffer = g_strdup_printf(" %s", ld.url);
	  break;

        default:
	  goto no_link;
	}
      if (!data->in_link)
	{
	  if (data->appbar)
	    gnome_appbar_push(GNOME_APPBAR(data->appbar), buffer);
	  data->in_link = TRUE;
	}
      else if (data->appbar)
	gnome_appbar_set_status(GNOME_APPBAR(data->appbar), buffer);
      g_free(buffer);
      gdk_window_set_cursor(widget->window, hand);
    }
  else
    {
no_link:
      if (data->in_link)
	{
	  if (data->appbar)
	    gnome_appbar_pop(GNOME_APPBAR(data->appbar));
	  data->in_link = FALSE;
	}
      gdk_window_set_cursor(widget->window, arrow);
    }
}

static gint
event_timeout				(ttxview_data	*data)
{
  enum ttx_message msg;
  gint w, h;
  GtkWidget *widget;
  gchar *buffer;
  ttx_message_data msg_data;

  while ((msg = peek_ttx_message(data->id, &msg_data)))
    {
      if (data->selecting && msg == TTX_PAGE_RECEIVED)
	continue;

      /* discard page received messages while selecting */
      switch (msg)
	{
	case TTX_PAGE_RECEIVED:
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

	  gdk_window_get_geometry(data->da->window, NULL, NULL, &w,
				  &h, NULL);
	  gdk_window_clear_area_e(data->da->window, 0, 0, w, h);
	  data->subpage = data->fmt_page->subno;
	  widget = lookup_widget(data->toolbar, "ttxview_url");
	  if (data->subpage)
	    buffer = g_strdup_printf("%03x.%02x", data->fmt_page->pgno,
				     data->subpage);
	  else
	    buffer = g_strdup_printf("%03x.00", data->fmt_page->pgno);
	  gtk_label_set_text(GTK_LABEL(widget), buffer);
	  if (!data->no_history)
	    append_history(data->fmt_page->pgno,
			   data->monitored_subpage, data);
	  data->no_history = FALSE;
	  g_free(buffer);
	  if (data->appbar)
	    gnome_appbar_set_status(GNOME_APPBAR(data->appbar), "");
	  if (data->in_link)
	    {
	      if (data->appbar)
		gnome_appbar_pop(GNOME_APPBAR(data->appbar));
	      data->in_link = FALSE;
	    }
	  if ((!data->fmt_page->pgno) &&
	      (data->appbar))
	    {
	      gnome_appbar_set_status(GNOME_APPBAR(data->appbar),
				      _("Warning: Page not valid"));
	      g_warning("BAD PAGE: 0x%x", data->fmt_page->pgno);
	    }
	  update_pointer(data);
	  break;
	case TTX_NETWORK_CHANGE:
	case TTX_PROG_INFO_CHANGE:
	case TTX_TRIGGER:
	  break;
	case TTX_CHANNEL_SWITCHED:
	  gnome_appbar_set_status(GNOME_APPBAR(data->appbar),
				  _("The cache has been cleared"));
	  break;
	case TTX_BROKEN_PIPE:
	  g_warning("Broken TTX pipe");
	  return FALSE;
	default:
	  g_warning("Unknown message: %d", msg);
	  break;
	}
    }

  return TRUE;
}

/*
 *  Commands
 */

static PyObject* py_ttx_open_new (PyObject *self, PyObject *args)
{
  ttxview_data *data = ttxview_data_from_widget
    (GTK_WIDGET (remote_last_caller ()));
  GtkWidget *dolly;
  vbi_pgno page = 100;
  vbi_subno subpage = VBI_ANY_SUBNO;
  gint w = 300, h = 320;
  int ok = PyArg_ParseTuple (args, "|ii", &page, &subpage);

  if (!ok)
    g_error ("zapping.ttx_open_new(|ii)");

  page = vbi_dec2bcd (page);
  if (subpage != VBI_ANY_SUBNO)
    subpage = vbi_dec2bcd (subpage);

  if (page < 0x100 || page > 0x899
      || (subpage != VBI_ANY_SUBNO && (subpage < 0x00 || subpage > 0x99)))
    py_return_false;

  if (data)
    {
      if (data->fmt_page && data->fmt_page->pgno)
	{
	  page = data->fmt_page->pgno;
	  subpage = data->monitored_subpage & 0xFF;
	}

      gdk_window_get_geometry (data->parent->window, NULL, NULL, &w,
			       &h, NULL);
    }

  if (!zvbi_get_object ())
    {
      ShowBox (_("VBI has been disabled"), GTK_MESSAGE_WARNING);
      py_return_false;
    }

  dolly = build_ttxview ();

  load_page (page, subpage,
	     (ttxview_data *) g_object_get_data
	     (G_OBJECT (dolly), "ttxview_data"), NULL);

  gtk_widget_realize (dolly);
  z_update_gui ();
  gdk_window_resize (dolly->window, w, h);
  gtk_widget_show (dolly);

  py_return_true;
}

static PyObject* py_ttx_home (PyObject *self, PyObject *args)
{
  ttxview_data *data = ttxview_data_from_widget
    (GTK_WIDGET (remote_last_caller ()));
  vbi_link ld;

  if (!data)
    py_return_true;

  vbi_resolve_home (data->fmt_page, &ld);

  if (ld.type == VBI_LINK_PAGE || ld.type == VBI_LINK_SUBPAGE)
    {
      if (ld.pgno)
	load_page (ld.pgno, ld.subno, data, NULL);
      else
	load_page (0x100, VBI_ANY_SUBNO, data, NULL);
    }
  /* else VBI_LINK_HTTP, "http://zapping.sourceforge.net" :-) */

  py_return_true;
}

static PyObject* py_ttx_page_incr (PyObject *self, PyObject *args)
{
  ttxview_data *data = ttxview_data_from_widget
    (GTK_WIDGET (remote_last_caller ()));
  vbi_pgno new_page;
  gint value = +1;
  int ok = PyArg_ParseTuple (args, "|i", &value);

  if (!ok)
    g_error ("zapping.ttx_page_incr(|i)");

  if (!data)
    py_return_true;

  if (abs (value) > 999)
    py_return_false;

  if (value < 0)
    value += 1000;

  new_page = vbi_add_bcd (data->page, vbi_dec2bcd (value)) & 0xFFF;

  if (new_page < 0x100)
    new_page = 0x800 + (new_page & 0xFF);
  else if (new_page > 0x899)
    new_page = 0x100 + (new_page & 0xFF);

  load_page (new_page, VBI_ANY_SUBNO, data, NULL);

  py_return_true;
}

static PyObject* py_ttx_subpage_incr (PyObject *self, PyObject *args)
{
  ttxview_data *data = ttxview_data_from_widget
    (GTK_WIDGET (remote_last_caller ()));
  vbi_pgno new_subpage;
  gint value = +1;
  int ok = PyArg_ParseTuple (args, "|i", &value);

  if (!ok)
    g_error ("zapping.ttx_subpage_incr(|i)");

  if (!data)
    py_return_none;

  if (abs (value) > 99)
    py_return_false;

  if (value < 0)
    value += 100; /* XXX should use actual or anounced number of subp */

  new_subpage = vbi_add_bcd (data->subpage, vbi_dec2bcd (value)) & 0xFF;

  load_page (data->fmt_page->pgno, new_subpage, data, NULL);

  py_return_true;
}

static void
set_hold				(ttxview_data *	data,
					 gboolean	hold)
{
  GtkWidget *button;

  button = lookup_widget (data->toolbar, "ttxview_hold");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)) != hold)
    ORC_BLOCK (gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), hold));

  if (hold != data->hold)
    {
      vbi_page *pg = data->fmt_page;

      data->hold = hold;

      if (hold)
	load_page (pg->pgno, pg->subno, data, NULL);
      else
	load_page (pg->pgno, VBI_ANY_SUBNO, data, NULL);
    }
}

static PyObject* py_ttx_hold (PyObject *self, PyObject *args)
{
  ttxview_data *data = ttxview_data_from_widget
    (GTK_WIDGET (remote_last_caller ()));
  gint hold = -1;
  int ok = PyArg_ParseTuple (args, "|i", &hold);

  if (!ok)
    g_error ("zapping.ttx_hold(|i)");

  if (!data)
    py_return_true;

  if (hold < 0)
    hold = !data->hold;
  else
    hold = !!hold;

  set_hold (data, hold);

  py_return_true;
}

static PyObject* py_ttx_reveal (PyObject *self, PyObject *args)
{
  ttxview_data *data = ttxview_data_from_widget
    (GTK_WIDGET (remote_last_caller ()));
  GtkWidget *button;
  gint reveal = -1;
  int ok = PyArg_ParseTuple (args, "|i", &reveal);

  if (!ok)
    g_error ("zapping.ttx_reveal(|i)");

  if (!data)
    py_return_true;

  if (reveal < 0)
    reveal = !zcg_bool (NULL, "reveal");
  else
    reveal = !!reveal;

  button = lookup_widget (data->toolbar, "ttxview_reveal");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)) != reveal)
    ORC_BLOCK (gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), reveal));

  zcs_bool (reveal, "reveal");
  set_ttx_parameters (data->id, reveal);

  if (data->page >= 0x100)
    load_page (data->page, data->subpage, data, NULL);

  py_return_true;
}

static PyObject* py_ttx_history_next (PyObject *self, PyObject *args)
{
  ttxview_data *data = ttxview_data_from_widget
    (GTK_WIDGET (remote_last_caller ()));
  gint page, page_code, pc_subpage;

  py_return_true;

  if (data->history_stack_size == (data->history_sp + 1))
    py_return_false;

  data->history_sp++;

  page_code = GPOINTER_TO_INT (g_list_nth (data->history_stack,
					   data->history_sp)->data);
  page = page_code >> 16;
  pc_subpage = (page_code & 0xffff);
  pc_subpage = (pc_subpage == 0xffff) ? VBI_ANY_SUBNO : pc_subpage;

  data->no_history = TRUE;
  load_page (page, pc_subpage, data, NULL);
  setup_history_gui (data);

  py_return_true;
}

static PyObject* py_ttx_history_prev (PyObject *self, PyObject *args)
{
  ttxview_data *data = ttxview_data_from_widget
    (GTK_WIDGET (remote_last_caller ()));
  gint page, page_code, pc_subpage;

  if (!data)
    py_return_true;

  if (data->history_sp == 0)
    py_return_false;
  data->history_sp--;

  page_code = GPOINTER_TO_INT (g_list_nth (data->history_stack,
					   data->history_sp)->data);
  page = page_code >> 16;
  pc_subpage = (page_code & 0xffff);
  pc_subpage = (pc_subpage == 0xffff) ? VBI_ANY_SUBNO : pc_subpage;

  data->no_history = TRUE;
  load_page (page, pc_subpage, data, NULL);
  setup_history_gui (data);

  py_return_true;
}

#if 0 /* Was bound to [left]/[right], restore? */

static int
find_prev_subpage (ttxview_data	*data, int subpage)
{
  vbi_decoder *vbi = zvbi_get_object();
  int start_subpage = subpage;

  if (!vbi)
    return -1;

  if (!vbi_cache_hi_subno(vbi, data->fmt_page->pgno))
    return -1;

  do {
    subpage = vbi_add_bcd(subpage, 0x999) & 0xFFF;

    if (subpage == start_subpage)
      return -1;
    
    if (subpage < 0)
      subpage = vbi_cache_hi_subno(vbi, data->fmt_page->pgno) - 1;
  } while (!vbi_is_cached(vbi, data->fmt_page->pgno, subpage));

  return subpage;
}

static int
find_next_subpage (ttxview_data	*data, int subpage)
{
  vbi_decoder *vbi = zvbi_get_object();
  int start_subpage = subpage;

  if (!vbi)
    return -1;

  if (!vbi_cache_hi_subno(vbi, data->fmt_page->pgno))
    return -1;

  do {
    subpage = vbi_add_bcd(subpage, 0x001) & 0xFFF;

    if (subpage == start_subpage)
      return -1;
    
    if (subpage >= vbi_cache_hi_subno(vbi, data->fmt_page->pgno))
      subpage = 0;
  } while (!vbi_is_cached(vbi, data->fmt_page->pgno, subpage));

  return subpage;
}

static void
on_ttxview_prev_sp_cache_clicked	(GtkWidget	*widget,
					 ttxview_data	*data)
{
  nullcheck();

  if (data->fmt_page->pgno == TOP_INDEX_PAGE)
    load_page(data->fmt_page->pgno,
	      vbi_add_bcd(data->subpage, 0x99) & 0xFF, data, NULL);
  else
    {
      int subpage = find_prev_subpage(data, data->subpage);

      if (subpage >= 0 && subpage != data->subpage)
	load_page(data->fmt_page->pgno, subpage, data, NULL);
      else if (data->appbar)
	gnome_appbar_set_status(GNOME_APPBAR(data->appbar),
				_("No other subpage in the cache"));
    }
}

static void
on_ttxview_next_sp_cache_clicked	(GtkWidget	*widget,
					 ttxview_data	*data)
{
  nullcheck();

  if (data->fmt_page->pgno == TOP_INDEX_PAGE)
    load_page(data->fmt_page->pgno,
	      vbi_add_bcd(data->subpage, 0x01) & 0xFF, data, NULL);
  else
    {
      int subpage = find_next_subpage(data, data->subpage);

      if (subpage >= 0 && subpage != data->subpage)
	load_page(data->fmt_page->pgno, subpage, data, NULL);
      else if (data->appbar)
	gnome_appbar_set_status(GNOME_APPBAR(data->appbar),
				_("No other subpage in the cache"));
    }
}

#endif /* 0 */

/*
 *  Search
 */

static
void on_search_progress_destroy		(GObject	*widget,
					 gpointer	context)
{
  gpointer running = g_object_get_data(widget, "user-data");

  search_progress = NULL;

  if (!running)
    vbi_search_delete((vbi_search *) context);
}

/* These should match the definitions in the glade file */
enum {
  SEARCH_RESPONSE_BACK = 1,
  SEARCH_RESPONSE_FORWARD,
  SEARCH_RESPONSE_NEXT
};

static
void run_next				(GtkButton	*button,
					 gpointer	context,
					 gint           dir)
{
  gint return_code;
  vbi_page *pg;
  ttxview_data *data =
    (ttxview_data *)g_object_get_data(G_OBJECT(button),
					"ttxview_data");
  GtkWidget *search_next = lookup_widget(GTK_WIDGET(button),
					   "button21");
  GtkWidget *search_prev = lookup_widget(GTK_WIDGET(button),
					   "button22");

  gtk_widget_set_sensitive(search_next, FALSE);
  gtk_widget_set_sensitive(search_prev, FALSE);
  gtk_widget_set_sensitive(lookup_widget(search_next,
					 "progressbar2"), TRUE);
  gtk_label_set_text(GTK_LABEL(lookup_widget(search_next, "label97")),
		     "");
  gtk_dialog_set_default_response(GTK_DIALOG(search_progress),
				  GTK_RESPONSE_CANCEL);
  g_object_set_data(G_OBJECT(search_progress), "user-data",
			   (gpointer)0xdeadbeef);

  switch ((return_code = vbi_search_next(context, &pg, dir)))
    {
    case 1: /* found, show the page, enable next */
      load_page(pg->pgno, pg->subno, data, pg);
      if (search_progress)
	{
	  gtk_widget_set_sensitive(search_next, TRUE);
	  gtk_widget_set_sensitive(search_prev, TRUE);
	  if (zcg_bool(NULL, "ure_backwards"))
	    gtk_dialog_set_default_response(GTK_DIALOG(search_progress),
					    SEARCH_RESPONSE_BACK);
	  else
	    gtk_dialog_set_default_response(GTK_DIALOG(search_progress),
					    SEARCH_RESPONSE_FORWARD);
	  gtk_label_set_text(GTK_LABEL(lookup_widget(search_next,
						     "label97")),
			     _("Found"));
	  gtk_widget_set_sensitive(lookup_widget(search_next,
						 "progressbar2"), FALSE);
	}
      break;
    case 0: /* not found */
      if (search_progress)
	{
	  gtk_widget_set_sensitive(search_next, TRUE);
	  gtk_widget_set_sensitive(search_prev, TRUE);
	  if (zcg_bool(NULL, "ure_backwards"))
	    gtk_dialog_set_default_response(GTK_DIALOG(search_progress),
					    SEARCH_RESPONSE_BACK);
	  else
	    gtk_dialog_set_default_response(GTK_DIALOG(search_progress),
					    SEARCH_RESPONSE_FORWARD);
	  gtk_label_set_text(GTK_LABEL(lookup_widget(search_next,
						     "label97")),
			     _("Not found"));
	  gtk_widget_set_sensitive(lookup_widget(search_next,
						 "progressbar2"), FALSE);
	}
      break;      
    case -1: /* cancelled */
      break;
    case -2: /* no pages in the cache */
      if (search_progress)
	{
	  gtk_widget_set_sensitive(search_next, TRUE);
	  gtk_widget_set_sensitive(search_prev, TRUE);
	  if (zcg_bool(NULL, "ure_backwards"))
	    gtk_dialog_set_default_response(GTK_DIALOG(search_progress),
					    SEARCH_RESPONSE_BACK);
	  else
	    gtk_dialog_set_default_response(GTK_DIALOG(search_progress),
					    SEARCH_RESPONSE_FORWARD);
	  gtk_label_set_text(GTK_LABEL(lookup_widget(search_next,
						     "label97")),
			     _("Empty cache"));
	  gtk_widget_set_sensitive(lookup_widget(search_next,
						 "progressbar2"), FALSE);
	}
      break;
    case -3: /* unclear error, forget */
      break;
    default:
      g_message("Unknown search return code: %d",
		return_code);
      break;
    }

  if (search_progress)
    g_object_set_data(G_OBJECT(search_progress), "user-data", NULL);
  
  if (return_code < 0 &&
      return_code != -2)
    {
      if (search_progress)
	gtk_widget_destroy(search_progress);
      else
	vbi_search_delete((vbi_search *) context);
    }
}

static
void on_new_search_clicked		(GtkWidget	*button,
					 ttxview_data	*data)
{
  g_signal_stop_emission_by_name(G_OBJECT(button), "clicked");

  gtk_widget_destroy(lookup_widget(button, "search_progress"));

  on_ttxview_search_clicked(GTK_BUTTON(lookup_widget(data->toolbar,
				       "ttxview_search")), data);
}

static
void on_search_progress_next		(GtkButton	*button,
					 gpointer	context)
{
  g_signal_stop_emission_by_name(G_OBJECT(button), "clicked");
  
  zcs_bool(FALSE, "ure_backwards");

  run_next(button, context, +1);
}

static
void on_search_progress_prev		(GtkButton	*button,
					 gpointer	context)
{
  g_signal_stop_emission_by_name(G_OBJECT(button), "clicked");

  zcs_bool(TRUE, "ure_backwards");

  run_next(button, context, -1);
}

/*
  Substitute the special search keywords by the appropiate regex,
  returns a newly allocated string, and g_free's the given string.
  Valid search keywords:
  #url# -> Expands to "https?://([:alnum:]|[-~./?%_=+])+" or
			www.*
  #email# -> Expands to "([:alnum:]|[-~.])+@([:alnum:]|[-~.])+"
*/
static
gchar *subtitute_search_keywords	(gchar		*string)
{
  gint i;
  gchar *found;
  gchar *search_keys[][2] = {
    {"#email#", "([:alnum:]|[-~.])+@([:alnum:]|[-~.])+"},
    {"#url#",
     "(https?://([:alnum:]|[-~./?%_=+])+)|(www.([:alnum:]|[-~./?%_=+])+)"}
  };

  if ((!string) || (!*string))
    {
      g_free(string);
      return g_strdup("");
    }

  for (i=0; i<2; i++)
     while ((found = strstr(string, search_keys[i][0])))
     {
       gchar *p;

       *found = 0;
       
       p = g_strconcat(string, search_keys[i][1],
		       found+strlen(search_keys[i][0]), NULL);
       g_free(string);
       string = p;
     }

  return string;
}

static
int progress_update			(vbi_page *pg)
{
  gchar *buffer;
  GtkProgressBar *progress;

  if (search_progress)
    {
      buffer = g_strdup_printf(_("Scanning %x.%02x"), pg->pgno, pg->subno);
      gtk_label_set_text(GTK_LABEL(lookup_widget(search_progress, "label97")),
			 buffer);
      g_free(buffer);
      progress =
	GTK_PROGRESS_BAR(lookup_widget(search_progress, "progressbar2"));
      gtk_progress_bar_pulse(progress);
    }
  else
    return FALSE;

  z_update_gui();
    
  if (flag_exit_program)
    return FALSE;
  else
    return TRUE;
}

static
void on_ttxview_search_clicked		(GtkButton	*button,
					 ttxview_data	*data)
{
  GtkDialog *ure_search = GTK_DIALOG(build_widget("ure_search", NULL));
  GtkWidget *entry2 = gnome_entry_gtk_entry(GNOME_ENTRY(
    lookup_widget(GTK_WIDGET(ure_search), "entry2")));
  GtkWidget * button23;
  GtkToggleButton *checkbutton9 =
    GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(ure_search),
				    "checkbutton9"));
  GtkToggleButton *checkbutton9a =
    GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(ure_search),
				    "checkbutton9a"));
  GtkToggleButton *checkbutton10 =
    GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(ure_search),
				    "checkbutton10"));
  GtkWidget *search_progress_next;
  GtkWidget *search_progress_prev;
  gboolean result;
  gchar *needle;
  void *search_context;
  uint16_t *pattern;

  if (!zvbi_get_object())
    {
      ShowBox(_("VBI has been disabled"), GTK_MESSAGE_WARNING);
      gtk_widget_destroy(GTK_WIDGET(ure_search));
      return;
    }

  gtk_window_set_transient_for (GTK_WINDOW (ure_search),
				GTK_WINDOW (data->parent));
  gtk_dialog_set_default_response(ure_search, GTK_RESPONSE_OK);

  gtk_toggle_button_set_active(checkbutton9,
			       zcg_bool(NULL, "ure_regexp"));
  gtk_toggle_button_set_active(checkbutton9a,
			       zcg_bool(NULL, "ure_casefold"));
  gtk_toggle_button_set_active(checkbutton10,
			       zcg_bool(NULL, "ure_backwards"));

 no_needle:
  gtk_widget_grab_focus(entry2);

  while ((result = gtk_dialog_run(ure_search)) == GTK_RESPONSE_HELP)
    gnome_help_display ("ure.html", NULL, NULL); 
 
  needle = (gchar*)gtk_entry_get_text(GTK_ENTRY(entry2));
  if (needle && *needle)
    needle = g_strdup(needle);
  else if (result == GTK_RESPONSE_OK)
    goto no_needle;

  zcs_bool(gtk_toggle_button_get_active(checkbutton9), "ure_regexp");
  zcs_bool(gtk_toggle_button_get_active(checkbutton9a), "ure_casefold");
  zcs_bool(gtk_toggle_button_get_active(checkbutton10), "ure_backwards");
  gtk_widget_destroy (GTK_WIDGET (ure_search));

  if ((result == GTK_RESPONSE_OK) && (needle))
    {
      needle = subtitute_search_keywords(needle);
      pattern = local2ucs2(needle);
      g_free(needle);
      if (pattern)
	{
	  search_context =
	    vbi_search_new(zvbi_get_object(),
			   0x100, VBI_ANY_SUBNO, pattern,
			   zcg_bool(NULL, "ure_casefold"),
			   zcg_bool(NULL, "ure_regexp"),
			   progress_update);
	  free(pattern);
	  if (search_context)
	    {
	      if (search_progress)
		gtk_widget_destroy(search_progress);
	      search_progress = build_widget("search_progress", NULL);
	      gtk_window_set_modal(GTK_WINDOW(search_progress), TRUE);
	      gtk_window_set_transient_for (GTK_WINDOW(search_progress),
					    GTK_WINDOW(data->parent));
	      g_signal_connect(G_OBJECT(search_progress), "destroy",
			       G_CALLBACK(on_search_progress_destroy),
			       search_context);
	      search_progress_next = lookup_widget(search_progress,
						   "button21");
	      g_signal_connect(G_OBJECT(search_progress_next), "clicked",
			       G_CALLBACK(on_search_progress_next),
			       search_context);
	      g_object_set_data(G_OBJECT(search_progress_next),
				"ttxview_data", data);
	      gtk_widget_set_sensitive(search_progress_next, FALSE);
	      search_progress_prev = lookup_widget(search_progress,
						   "button22");
	      g_signal_connect(G_OBJECT(search_progress_prev), "clicked",
			       G_CALLBACK(on_search_progress_prev),
			       search_context);
	      g_object_set_data(G_OBJECT(search_progress_prev),
				  "ttxview_data", data);
	      gtk_widget_set_sensitive(search_progress_prev, FALSE);
	      button23 =
		lookup_widget(GTK_WIDGET(search_progress), "button23");
	      g_signal_connect(G_OBJECT(button23), "clicked",
				 G_CALLBACK(on_new_search_clicked),
				 data);
	      gtk_widget_show(search_progress);
	      run_next(GTK_BUTTON(search_progress_next), search_context,
		zcg_bool(NULL, "ure_backwards") ? -1 : +1);
	    }
	}
    }
}



static
void on_ttxview_size_allocate		(GtkWidget	*widget,
					 GtkAllocation	*allocation,
					 ttxview_data	*data)
{
  scale_image(widget, allocation->width, allocation->height, data);
}

void
open_in_ttxview				(GtkWidget	*view,
					 gint		page,
					 gint		subpage)
{
  ttxview_data *data = (ttxview_data*)
    g_object_get_data(G_OBJECT(view), "ttxview_data");

  if (!data)
    return;

  load_page(page, subpage, data, NULL);
}

gboolean
get_ttxview_page			(GtkWidget	*view,
					 gint		*page,
					 gint		*subpage)
{
  ttxview_data *data = (ttxview_data*)
    g_object_get_data(G_OBJECT(view), "ttxview_data");

  if (!data)
    return FALSE;

  if (page)
    *page = data->fmt_page->pgno;
  if (subpage)
    *subpage = data->monitored_subpage;

  return TRUE;
}

static
void popup_new_win			(GtkWidget	*widget,
					 ttxview_data	*data)
{
  cmd_run_printf ("zapping.ttx_open_new(%x, %x)",
		  data->pop_pgno, data->pop_subno);
}

static
void new_bookmark			(GtkWidget	*widget,
					 ttxview_data	*data)
{
  vbi_decoder *vbi = zvbi_get_object();
  gchar *default_description=NULL;
  gchar title[41];
  const gchar *buffer;
  gint page, subpage;
  gint active;
  GtkWidget *dialog = build_widget("new_bookmark", NULL);
  GtkWidget *bookmark_name = lookup_widget(dialog, "bookmark_name");
  GtkWidget *bookmark_switch = lookup_widget(dialog, "bookmark_switch");
  tveng_tuned_channel *channel;

  nullcheck();

  if (data->page >= 0x100)
    page = data->page;
  else
    page = data->fmt_page->pgno;
  subpage = data->monitored_subpage;

  if (vbi_page_title(vbi, page, subpage, title))
    {
      if (subpage != VBI_ANY_SUBNO)
        default_description =
          g_strdup_printf("%x.%x %s", page, subpage, title);
      else
        default_description = g_strdup_printf("%x %s", page, title);
    }
  else
    {
      if (subpage != VBI_ANY_SUBNO)
        default_description =
          g_strdup_printf("%x.%x", page, subpage);
      else
        default_description = g_strdup_printf("%x", page);
    }

  gtk_widget_grab_focus(bookmark_name);
  gtk_entry_set_text(GTK_ENTRY(bookmark_name), default_description);
  g_free(default_description);
  gtk_editable_select_region(GTK_EDITABLE(bookmark_name), 0, -1);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(bookmark_switch),
			       zcg_bool(NULL, "bookmark_switch"));
  gtk_dialog_set_default_response(GTK_DIALOG (dialog),
				  GTK_RESPONSE_OK);
  
  if ((gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) &&
      (buffer = gtk_entry_get_text(GTK_ENTRY(bookmark_name))))
    {
      active =
	gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(bookmark_switch));
      zcs_bool(active, "bookmark_switch");
      if (!active)
	add_bookmark(page, subpage, buffer, NULL);
      else
	{
	  channel =
	    tveng_retrieve_tuned_channel_by_index(cur_tuned_channel,
						  global_channel_list);

	  if (channel)
	    add_bookmark(page, subpage, buffer, channel->name);
	  else
	    add_bookmark(page, subpage, buffer, NULL);
	}
      default_description =
	g_strdup_printf(_("<%s> added to the bookmarks"), buffer);
      if (data->appbar)
	gnome_appbar_set_status(GNOME_APPBAR(data->appbar),
				default_description);
      g_free(default_description);
    }

  gtk_widget_destroy(dialog);
}

static
void on_be_close			(GtkWidget	*widget,
					 ttxview_data	*data)
{
  gtk_widget_destroy(lookup_widget(widget, "bookmarks_editor"));
}

static
void on_be_delete			(GtkWidget	*widget,
					 ttxview_data	*data)
{
  GtkTreeView *treeview = GTK_TREE_VIEW
    (lookup_widget(widget, "bookmarks_treeview"));
  gint *deleted=NULL;
  gint i=0, j=0, n = g_list_length(bookmarks);
  GtkTreeSelection *sel = gtk_tree_view_get_selection (treeview);
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);
  gboolean valid;
  GtkTreeIter iter;

  deleted = (gint *) g_malloc(n * sizeof(gint));

  valid = gtk_tree_model_get_iter_first (model, &iter);

  while (valid)
    {
      if (gtk_tree_selection_iter_is_selected (sel, &iter))
	deleted[j++] = i++;

      valid = gtk_tree_model_iter_next (model, &iter);
    }
  
  for (i=0; i<j; i++)
    remove_bookmark(deleted[i]-i, i == (j-1));
  
  g_free(deleted);
}

enum {
  BE_PAGE,
  BE_SUBPAGE,
  BE_DESCRIPTION
};

static
void on_be_model_changed		(gpointer	unused,
					 GtkTreeView	*treeview)
{
  GtkListStore *model =
    GTK_LIST_STORE (gtk_tree_view_get_model (treeview));
  GList *p = g_list_first(bookmarks);
  struct bookmark *bookmark;
  gchar *buffer[3];
  GtkTreeIter iter;

  gtk_list_store_clear (model);

  while (p)
    {
      bookmark = (struct bookmark*)p->data;
      buffer[0] = g_strdup_printf("%x", bookmark->page);
      if (bookmark->subpage == VBI_ANY_SUBNO)
	buffer[1] = g_strdup(_("Any subpage"));
      else
	buffer[1] = g_strdup_printf("%x", bookmark->subpage);
      buffer[2] = bookmark->description;
      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter,
			  BE_PAGE, buffer[0],
			  BE_SUBPAGE, buffer[1],
			  BE_DESCRIPTION, buffer[2],
			  -1);
      g_free(buffer[0]);
      g_free(buffer[1]);

      p = p->next;
    }
}

static
void on_be_destroy			(GtkWidget	*widget,
					 ttxview_data	*data)
{
  GtkTreeView *treeview = GTK_TREE_VIEW 
    (lookup_widget (widget, "bookmarks_treeview"));

  g_signal_handlers_disconnect_matched(G_OBJECT(model),
				       G_SIGNAL_MATCH_FUNC |
				       G_SIGNAL_MATCH_DATA,
				       0, 0, NULL,
				       G_CALLBACK(on_be_model_changed),
				       treeview);
}

static
void on_edit_bookmarks_activated	(GtkWidget	*widget,
					 ttxview_data	*data)
{
  GtkWidget *be = build_widget("bookmarks_editor", NULL);
  GtkTreeView *treeview = GTK_TREE_VIEW
    (lookup_widget(widget, "bookmarks_treeview"));
  GtkListStore *store = gtk_list_store_new (3, G_TYPE_STRING,
					    G_TYPE_STRING, G_TYPE_STRING);
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;

  g_signal_connect(G_OBJECT(lookup_widget(be, "bookmarks_close")),
		     "clicked",
		     G_CALLBACK(on_be_close), data);
  g_signal_connect(G_OBJECT(lookup_widget(be, "bookmarks_remove")),
		     "clicked",
		     G_CALLBACK(on_be_delete), data);
  g_signal_connect(G_OBJECT(model), "changed",
		   G_CALLBACK(on_be_model_changed), treeview);
  g_signal_connect(G_OBJECT(be), "destroy",
		     G_CALLBACK(on_be_destroy), data);

  /* Set the layout of the model */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes
    (_("Page"), renderer,
     "text", BE_PAGE, NULL);
  gtk_tree_view_append_column (treeview, column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes
    (_("Subpage"), renderer,
     "text", BE_SUBPAGE, NULL);
  gtk_tree_view_append_column (treeview, column);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes
    (_("Description"), renderer,
     "text", BE_DESCRIPTION, NULL);
  gtk_tree_view_append_column (treeview, column);

  gtk_tree_view_set_search_column (treeview, BE_DESCRIPTION);

  gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (store));

  on_be_model_changed(G_OBJECT(model), treeview);
  gtk_widget_show(be);
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
static inline gchar *
xo_zconf_name (vbi_export *exp, vbi_option_info *oi)
{
  vbi_export_info *xi = vbi_export_info_export(exp);

  g_assert (xi != NULL);

  return g_strdup_printf("/zapping/options/export/%s/%s",
			 xi->keyword, oi->keyword);
}

static void
on_export_control			(GtkWidget *w,
					 gpointer user_data)
{
  vbi_export *exp = user_data;
  gchar *keyword = (gchar *) g_object_get_data (G_OBJECT (w), "key");
  vbi_option_info *oi = vbi_export_option_info_keyword (exp, keyword);
  vbi_option_value val;
  gchar *zcname;

  g_assert (exp != NULL && oi != NULL);

  zcname = xo_zconf_name (exp, oi);

  if (oi->menu.str)
    {
      val.num = (gint) g_object_get_data (G_OBJECT (w), "idx");
      vbi_export_option_menu_set (exp, keyword, val.num);
    }
  else
    switch (oi->type)
      {
      case VBI_OPTION_BOOL:
	val.num = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w));
        if (vbi_export_option_set (exp, keyword, val))
	  zconf_set_boolean (val.num, zcname);
	break;
      case VBI_OPTION_INT:
	val.num = (int) GTK_ADJUSTMENT (w)->value;
        if (vbi_export_option_set (exp, keyword, val))
	  zconf_set_integer (val.num, zcname);
	break;
      case VBI_OPTION_REAL:
	val.dbl = GTK_ADJUSTMENT (w)->value;
        if (vbi_export_option_set (exp, keyword, val))
	  zconf_set_float (val.dbl, zcname);
	break;
      case VBI_OPTION_STRING:
	val.str = (gchar*)gtk_entry_get_text (GTK_ENTRY (w));
        if (vbi_export_option_set (exp, keyword, val))
	  zconf_set_string (val.str, zcname);
	break;
      default:
	g_warning ("Unknown export option type %d "
		   "in on_export_control", oi->type);
      }

  g_free(zcname);
}

static void
create_export_entry (GtkWidget *table, vbi_option_info *oi,
		     int index, vbi_export *exp)
{ 
  gchar *zcname = xo_zconf_name (exp, oi);
  GtkWidget *label;
  GtkWidget *entry;

  label = gtk_label_new (V_(oi->label));
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  z_tooltip_set (entry, V_(oi->tooltip));
  gtk_widget_show (entry);
  zconf_create_string (oi->def.str, oi->tooltip, zcname);
  gtk_entry_set_text (GTK_ENTRY (entry),
		      _(zconf_get_string(NULL, zcname)));
  g_free (zcname);

  g_object_set_data (G_OBJECT (entry), "key", oi->keyword);
  g_signal_connect (G_OBJECT (entry), "changed", 
		     G_CALLBACK (on_export_control), exp);
  on_export_control (entry, exp);

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
}

static void
create_export_menu (GtkWidget *table, vbi_option_info *oi,
		    int index, vbi_export *exp)
{
  GtkWidget *label; /* This shows what the menu is for */
  GtkWidget *option_menu; /* The option menu */
  GtkWidget *menu; /* The menu displayed */
  GtkWidget *menu_item; /* Each of the menu items */
  gchar *zcname = xo_zconf_name (exp, oi); /* Path to the config key */
  gchar buf[256];
  gint i, saved;

  label = gtk_label_new (V_(oi->label));
  gtk_widget_show (label);

  option_menu = gtk_option_menu_new ();
  menu = gtk_menu_new ();

  saved = zconf_get_integer (NULL, zcname);

  for (i = 0; i <= oi->max.num; i++)
    {
      switch (oi->type)
	{
	case VBI_OPTION_BOOL:
	case VBI_OPTION_INT:
	  snprintf (buf, sizeof(buf) - 1, "%d", oi->menu.num[i]);
	  break;
	case VBI_OPTION_REAL:
	  snprintf (buf, sizeof(buf) - 1, "%f", oi->menu.dbl[i]);
	  break;
	case VBI_OPTION_STRING:
	  strncpy (buf, oi->menu.str[i], sizeof(buf) - 1);
	  break;
	case VBI_OPTION_MENU:
	  strncpy (buf, V_(oi->menu.str[i]), sizeof(buf) - 1);
	  break;
	default:
	  g_warning ("Unknown export option type %d "
		     "in create_export_menu", oi->type);
	  buf[0] = 0;
	}

      menu_item = gtk_menu_item_new_with_label (buf);

      g_object_set_data (G_OBJECT (menu_item), "key", oi->keyword);
      g_object_set_data (G_OBJECT (menu_item), "idx", GINT_TO_POINTER (i));
      g_signal_connect (G_OBJECT (menu_item), "activate",
			 G_CALLBACK (on_export_control), exp);

      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

      if (i == saved)
	on_export_control(menu_item, exp);
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
  zconf_create_integer (oi->def.num, oi->tooltip, zcname);
  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), saved);
  g_free (zcname);
  gtk_widget_show (menu);
  z_tooltip_set (option_menu, V_(oi->tooltip));
  gtk_widget_show (option_menu);

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
  gtk_table_attach (GTK_TABLE (table), option_menu, 1, 2,
                    index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
}

static void
create_export_slider (GtkWidget *table, vbi_option_info *oi,
		      int index, vbi_export *exp)
{ 
  GtkWidget *label;
  GtkWidget *hscale;
  GtkObject *adj; /* Adjustment object for the slider */
  gchar *zcname = xo_zconf_name(exp, oi);

  label = gtk_label_new (V_(oi->label));
  gtk_widget_show (label);

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
		      G_CALLBACK (on_export_control), exp);
  on_export_control ((GtkWidget *) adj, exp);

  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adj));
  gtk_scale_set_value_pos (GTK_SCALE (hscale), GTK_POS_LEFT);
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  z_tooltip_set (hscale, V_(oi->tooltip));
  gtk_widget_show (hscale);

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
  gtk_table_attach (GTK_TABLE (table), hscale, 1, 2,
                    index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
}

static void
create_export_checkbutton (GtkWidget *table, vbi_option_info *oi,
			   int index, vbi_export *exp)
{
  GtkWidget *cb;
  gchar *zcname = xo_zconf_name(exp, oi);

  cb = gtk_check_button_new_with_label (V_(oi->label));
  zconf_create_boolean (oi->def.num, oi->tooltip, zcname);

  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (cb), FALSE);
  z_tooltip_set (cb, V_(oi->tooltip));
  gtk_widget_show (cb);

  g_object_set_data (G_OBJECT (cb), "key", oi->keyword);
  g_signal_connect (G_OBJECT (cb), "toggled",
		     G_CALLBACK (on_export_control), exp);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb),
				zconf_get_boolean (NULL, zcname));
  g_free (zcname);
  on_export_control (cb, exp);

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (table), cb, 1, 2, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
}

static void
create_export_options (GtkWidget *table, vbi_export *exp)
{
  vbi_option_info *oi;
  int i;

  for (i = 0; (oi = vbi_export_option_info_enum (exp, i)); i++)
    {
      if (!oi->label)
	continue;

      if (oi->menu.str)
	create_export_menu (table, oi, i, exp);
      else
	switch (oi->type)
	  {
	  case VBI_OPTION_BOOL:
	    create_export_checkbutton (table, oi, i, exp);
	    break;
	  case VBI_OPTION_INT:
	  case VBI_OPTION_REAL:
	    create_export_slider (table, oi, i, exp);
	    break;
	  case VBI_OPTION_STRING:
	    create_export_entry (table, oi, i, exp);
	    break;
	  default:
	    g_warning ("Unknown export option type %d "
		       "in create_export_options", oi->type);
	    continue;
	  }
    }
}

static GtkWidget *
create_export_dialog (gchar **bpp, gchar *basename,
		      ttxview_data *data, vbi_export *exp)
{
  vbi_export_info *xm;
  gchar *buffer;
  GtkWidget *dialog;
  GtkWidget *table;
  GtkWidget *vbox;
  GtkWidget *w;

  xm = vbi_export_info_export (exp);
  g_assert (xm != NULL);
  buffer = g_strdup_printf (_("Export %s"), xm->label);

  dialog = gtk_dialog_new_with_buttons
    (buffer,
     GTK_WINDOW (data->parent),
     GTK_DIALOG_DESTROY_WITH_PARENT,
     GTK_STOCK_OK, GTK_RESPONSE_OK,
     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
     NULL);

  g_free (buffer);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  vbox = gtk_vbox_new (FALSE, 3);
  gtk_widget_show (vbox);

  w = gtk_label_new (_("Save as:"));
  gtk_widget_show (w);
  gtk_box_pack_start_defaults (GTK_BOX (vbox), w);

  w = gnome_file_entry_new ("ttxview_export_id",
			    _("Select file you'll be exporting to"));
  gnome_file_entry_set_default_path (GNOME_FILE_ENTRY (w), *bpp);
  gtk_widget_set_size_request (w, 400, -1);
  gtk_widget_show (w);
  gtk_box_pack_start_defaults (GTK_BOX (vbox), w);

  w = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY(w));
  g_object_set_data (G_OBJECT (w), "basename", (gpointer) basename);
  gtk_entry_set_text (GTK_ENTRY(w), *bpp);
  g_signal_connect (G_OBJECT (w), "changed",
		      G_CALLBACK (z_on_electric_filename),
		      (gpointer) bpp);

  if (vbi_export_option_info_enum(exp, 0))
    {
      w = gtk_frame_new (_("Options"));
      gtk_widget_show (w);
      gtk_box_pack_start_defaults (GTK_BOX (vbox), w);

      table = gtk_table_new (1, 2, FALSE);
      gtk_widget_show (table);

      create_export_options (table, exp);

      gtk_widget_show (table);
      gtk_container_add (GTK_CONTAINER (w), table);
    }

  gtk_box_pack_start_defaults (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox);

  return dialog;
}

static
void export_ttx_page			(GtkWidget	*widget,
					 ttxview_data	*data,
					 char		*fmt)
{
  vbi_export *exp;
  char *error;

  if (data->fmt_page->pgno < 0x100)
    {
      if (data->appbar)
	gnome_appbar_set_status (GNOME_APPBAR (data->appbar),
				_("No page loaded"));
      return;
    }

  if ((exp = vbi_export_new (fmt, &error)))
    {
      extern vbi_network current_network; /* FIXME */
      vbi_network network;
      GtkWidget *dialog;
      vbi_export_info *xi;
      gchar *name;
      gchar *dirname;
      gchar *filename;
      gchar **extensions;
      gint i, result;

      network = current_network;
      if (!network.name[0])
	strncpy (network.name, "Zapzilla", sizeof (network.name) - 1);

      /* Don't care if these fail */
      vbi_export_option_set (exp, "network", network.name);
      vbi_export_option_set (exp, "creator", "Zapzilla " VERSION);
      vbi_export_option_set (exp, "reveal", !!zcg_bool(NULL, "reveal"));

      xi = vbi_export_info_export (exp);
      extensions = g_strsplit (xi->extension, ",", 1);

      for (i = 0; i < (int) strlen (network.name); i++)
	if (!isalnum (network.name[i]))
	  network.name[i] = '_';

      if (data->fmt_page->subno > 0
	  && data->fmt_page->subno != VBI_ANY_SUBNO)
	filename = g_strdup_printf ("%s-%x-%x.%s", network.name,
				    data->fmt_page->pgno,
				    data->fmt_page->subno & 0xFF,
				    extensions[0]);
      else
	filename = g_strdup_printf ("%s-%x.%s", network.name,
				    data->fmt_page->pgno,
				    extensions[0]);
      g_strfreev (extensions);

      name = z_build_filename (zcg_char (NULL, "exportdir"), filename);

      dialog = create_export_dialog (&name, filename, data, exp);
      result = gtk_dialog_run (GTK_DIALOG(dialog));
      if (result != GTK_RESPONSE_OK)
	goto failure;

      g_strstrip(name);

      dirname = g_path_get_dirname (name);
      if (strcmp (dirname, ".") != 0 || name[0] == '.')
	{
	  gchar *_errstr;

	  if (!z_build_path (dirname, &_errstr))
	    {
	      ShowBox (_("Cannot create destination dir for Zapzilla "
			 "export:\n%s\n%s"),
		       GTK_MESSAGE_WARNING, dirname, _errstr);
	      g_free (_errstr);
	      g_free (dirname);
	      goto failure;
	    }

	  /* make absolute path? */
	  zcs_char (dirname, "exportdir");
	}
      else
	{
	  zcs_char ("", "exportdir");
	}
      g_free (dirname);

      if (!vbi_export_file (exp, name, data->fmt_page))
	{
	  gchar *msg = g_strdup_printf (_("Export to %s failed: %s"),
					name, vbi_export_errstr(exp));
	  g_warning (msg);
	  if (data->appbar)
	    gnome_appbar_set_status (GNOME_APPBAR(data->appbar), msg);
	  g_free (msg);
	}
      else if (data->appbar)
	{
	  gchar *msg = g_strdup_printf (_("%s saved"), name);
 	  gnome_appbar_set_status (GNOME_APPBAR(data->appbar), msg);
	  g_free (msg);
	}

    failure:
      g_free (name);
      free (filename);
      vbi_export_delete (exp);
    }
  else
    {
      gchar *msg = g_strdup_printf (_("Export failed: %s"), error);

      free (error);
      g_warning (msg);
      if (data->appbar)
	gnome_appbar_set_status (GNOME_APPBAR(data->appbar), msg);
      g_free (msg);
    }
}

static
void on_export_menu			(GtkWidget	*widget,
					 ttxview_data	*data)
{
  gchar *keyword = (gchar *)
    g_object_get_data (G_OBJECT (widget), "user-data");

  export_ttx_page (widget, data, keyword);
}

static void
on_color_control			(GtkWidget *w,
					 gpointer user_data)
{
  vbi_decoder *vbi = zvbi_get_object();
  gint id = GPOINTER_TO_INT (user_data);
  gint brig, cont;

  switch (id) {
  case 0:
    brig = (gint) GTK_ADJUSTMENT(w)->value;
    zconf_set_integer(brig, TXCOLOR_DOMAIN "brightness");
    zconf_get_integer(&cont, TXCOLOR_DOMAIN "contrast");
    break;

  case 1:
    cont = (gint) GTK_ADJUSTMENT(w)->value;
    zconf_set_integer(cont, TXCOLOR_DOMAIN "contrast");
    zconf_get_integer(&brig, TXCOLOR_DOMAIN "brightness");
    break;
  }

  if (brig < 0) brig = 0;
  if (brig > 255) brig = 255;
  if (cont < -128) cont = -128;
  if (cont > 127) cont = 127;

  vbi_set_brightness(vbi, brig);
  vbi_set_contrast(vbi, cont);

  zmodel_changed(refresh);
}

static void
on_brig_con_reset			(GtkWidget	*widget,
					 gpointer	user_data)
{
  gint control = GPOINTER_TO_INT(user_data);
  GtkWidget *w;
  GtkAdjustment *adj;
  gint value;

  switch (control)
    {
    case 0: /* brightness */
      value = 128;
      w = lookup_widget(widget, "hscale71");
      break;
    default: /* contrast */
      value = 64;
      w = lookup_widget(widget, "hscale72");
      break;
    }

  adj = GTK_ADJUSTMENT (gtk_range_get_adjustment (GTK_RANGE(w)));
  gtk_adjustment_set_value (adj, value);
}

static
gboolean on_color_box_key_press		(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	data)
{
  switch (event->keyval)
    {
    case GDK_Escape:
      gtk_widget_destroy(widget);
      break;
    case GDK_c:
    case GDK_C:
      if (event->state & GDK_CONTROL_MASK)
	{
	  gtk_widget_destroy(widget);
	  break;
	}
    default:
      return FALSE;
    }

  return TRUE;
}

/*
 *  Teletext text brightness/contrast (in the future possibly
 *  Caption default colors (overriding std wht on blk))
 */
static GtkWidget *
create_color_dialog			(GtkWidget	*widget,
					 ttxview_data	*data)
{
  GtkWidget *dialog = build_widget("ttxview_color", NULL);
  GtkAdjustment *adj;
  GtkWidget *w;
  gint value;
  GdkPixbuf *pb;
  GtkWidget *pix;
  GtkTable *table71 =
    GTK_TABLE(lookup_widget(dialog, "table71"));

  /* Add brightness, contrast icons */
  pb = gdk_pixbuf_new_from_xpm_data(brig_xpm);
  pix = gtk_image_new_from_pixbuf (pb);
  gtk_widget_show(pix);
  gtk_table_attach_defaults(table71, pix, 0, 1, 0, 1);
  g_object_unref(pb);

  pb = gdk_pixbuf_new_from_xpm_data(con_xpm);
  pix = gtk_image_new_from_pixbuf (pb);
  gtk_widget_show(pix);
  gtk_table_attach_defaults(table71, pix, 0, 1, 1, 2);
  g_object_unref(pb);

  w = lookup_widget(dialog, "hscale71");
  zconf_get_integer(&value, TXCOLOR_DOMAIN "brightness");
  adj = GTK_ADJUSTMENT(gtk_range_get_adjustment(GTK_RANGE(w)));
  gtk_adjustment_set_value(adj, value);
  g_signal_connect(G_OBJECT(adj), "value-changed",
		     G_CALLBACK (on_color_control),
		     GINT_TO_POINTER (0));

  w = lookup_widget(dialog, "hscale72");
  zconf_get_integer(&value, TXCOLOR_DOMAIN "contrast");
  adj = GTK_ADJUSTMENT(gtk_range_get_adjustment(GTK_RANGE(w)));
  gtk_adjustment_set_value(adj, value);
  g_signal_connect(G_OBJECT(adj), "value-changed",
		     G_CALLBACK (on_color_control),
		     GINT_TO_POINTER (1));

  w = lookup_widget(dialog, "brig_reset");
  g_signal_connect(G_OBJECT(w), "clicked",
		     G_CALLBACK (on_brig_con_reset),
		     GINT_TO_POINTER (0));

  w = lookup_widget(dialog, "con_reset");
  g_signal_connect(G_OBJECT(w), "clicked",
		     G_CALLBACK (on_brig_con_reset),
		     GINT_TO_POINTER (1));

  g_signal_connect(G_OBJECT(dialog), "key-press-event",
		     G_CALLBACK(on_color_box_key_press),
		     NULL);

  return dialog;
}

static
void on_bookmark_activated		(GtkWidget	*widget,
					 ttxview_data	*data)
{
  struct bookmark *bookmark = (struct bookmark*)
    g_object_get_data(G_OBJECT(widget), "user-data");
  tveng_tuned_channel *channel;

  if (main_info)
    if (bookmark->channel &&
	(channel =
	 tveng_retrieve_tuned_channel_by_name(bookmark->channel, 0,
					      global_channel_list)))
      z_switch_channel(channel, main_info);

  load_page(bookmark->page, bookmark->subpage, data, NULL);
}

static
void on_subtitle_select			(GtkWidget	*widget,
					 gint		page)
{
  vbi_page_type classf;

  if (!zvbi_get_object())
    return;

  classf = vbi_classify_page(zvbi_get_object(), page, NULL, NULL);

  if (classf == VBI_SUBTITLE_PAGE ||
      (classf == VBI_NORMAL_PAGE && (page >= 5 && page <= 8)))
    zmisc_overlay_subtitles(page);
}

/* Open the subtitle page in a new TTXView */
static void
on_subtitle_page_ttxview		(GtkWidget	*widget,
					 gint		val)
{
  gint page = val >> 16;
  gint subpage = val & 0xffff;

  if (!zvbi_get_object() || page < 0x100)
    return;

  cmd_run_printf ("zapping.ttx_open_new(%x, %x)",
		  page, subpage);
}

/* Open the subtitle page in the main window */
static void
on_subtitle_page_main			(GtkWidget	*menuitem,
					 gint		val)
{
  gint page = val >> 16;
  gint subpage = val & 0xffff;
  ttxview_data *data;
  GtkWidget *widget =
    GTK_WIDGET(g_object_get_data(G_OBJECT(menuitem), "user-data"));

  if (!zvbi_get_object() || page < 0x100)
    return;

  /* Z acting as a TTXView */
  if (find_widget(widget, "zapping"))
    {
      /* Switch to TXT mode in the main window */
      zmisc_switch_mode(TVENG_NO_CAPTURE, main_info);
      
      data = (ttxview_data*)g_object_get_data(G_OBJECT(main_window),
						"ttxview_data");

      if (data) /* Z is now a TTXView */
	load_page(page, subpage, data, NULL);
    }
  else /* Zapzilla window */
    {
      widget = lookup_widget(widget, "ttxview");
      data = (ttxview_data*)g_object_get_data(G_OBJECT(widget),
						"ttxview_data");
      g_assert(data != NULL);
      /* should raise the window if the page is already open */
      load_page(page, subpage, data, NULL);
    }
}

/**
 * Widget: A widget in the tree the window the popup menu belongs to
 * belongs to (you get the point :-))
 */
static void
build_subtitles_submenu(GtkWidget *widget,
			GtkMenuShell *zmenu, gboolean build_subtitles)
{
  GtkMenuShell *menu;
  GtkWidget *menu_item;
  gint count;
  gboolean empty = TRUE, something = FALSE, index = FALSE;
  vbi_page_type classf;
  vbi_subno subpage = VBI_ANY_SUBNO;
  gchar *language;
  gchar *buffer;
  gint insert_index; /* after New TTX view */

  if (!zvbi_get_object())
    return;

  menu = GTK_MENU_SHELL(gtk_menu_new());

  menu_item = gtk_tearoff_menu_item_new();
  gtk_widget_show(menu_item);
  gtk_menu_shell_append(menu, menu_item);

  if (find_widget(GTK_WIDGET(zmenu), "separador7"))
    insert_index =
      z_menu_get_index(GTK_WIDGET(zmenu),
		       lookup_widget(GTK_WIDGET(zmenu), "separador7"));
  else
    insert_index = 1;

  g_assert(insert_index > 0);

  insert_index++; /* insert after the separator */

  /* CC */
  for (count = 1; count <= 8; count++)
    {
      classf = vbi_classify_page(zvbi_get_object(), count, NULL,
				 &language);
      if ((classf == VBI_SUBTITLE_PAGE || classf == VBI_NORMAL_PAGE)
	  && build_subtitles)
	{
	  if (language)
	    {
	      if (classf == VBI_SUBTITLE_PAGE)
	        buffer = g_strdup_printf(_("Caption %x - %s"), count, language);
	      else
	        buffer = g_strdup_printf(_("Text %x - %s"), count - 4, language);
	    }
	  else
	    if (classf == VBI_SUBTITLE_PAGE)
	      buffer = g_strdup_printf(_("Caption %x"), count);
	    else
	      buffer = g_strdup_printf(_("Text %x"), count - 4);

	  menu_item = gtk_menu_item_new_with_label(buffer);
	  g_signal_connect(G_OBJECT(menu_item), "activate",
			     G_CALLBACK(on_subtitle_select),
			     GINT_TO_POINTER(count));
	  gtk_menu_shell_append(menu, menu_item);
	  gtk_widget_show(menu_item);

	  g_free(buffer);

	  empty = FALSE;
	}
    }

  /* TTX */
  for (count = 0x100; count <= 0x899; count = vbi_add_bcd(count, 1) & 0xFFF)
    {
      classf = vbi_classify_page(zvbi_get_object(), count, NULL,
				 &language);
      if (classf == VBI_SUBTITLE_PAGE && build_subtitles)
	{
	  if (language)
	    buffer = g_strdup(language);
	  else
	    buffer = g_strdup_printf(_("%x: Unknown language"), count);

	  menu_item = gtk_menu_item_new_with_label(buffer);
	  g_signal_connect(G_OBJECT(menu_item), "activate",
			     G_CALLBACK(on_subtitle_select),
			     GINT_TO_POINTER(count));
	  gtk_menu_shell_append(menu, menu_item);
	  gtk_widget_show(menu_item);

	  g_free(buffer);
	  if (language)
	    {
	      buffer = g_strdup_printf(_("Page %x"), count);
	      z_tooltip_set(menu_item, buffer);
	      g_free(buffer);
	    }
	  empty = FALSE;
	}
      else if (classf == VBI_SUBTITLE_INDEX)
	{
	  menu_item =
	    z_gtk_pixmap_menu_item_new(_("Index"),
				       GTK_STOCK_INDEX);
	  g_object_set_data(G_OBJECT(menu_item), "user-data", widget);
	  g_signal_connect(G_OBJECT(menu_item), "activate",
			     G_CALLBACK(on_subtitle_page_ttxview),
			     GINT_TO_POINTER((count << 16) + VBI_ANY_SUBNO));
	  buffer = g_strdup_printf(_("Page %x"), count);
	  z_tooltip_set(menu_item, buffer);
	  g_free(buffer);
	  gtk_menu_shell_insert(menu, menu_item, 1);
	  gtk_widget_show(menu_item);
	  if (!index)
	    {
	      menu_item = gtk_menu_item_new();
	      gtk_widget_show(menu_item);
	      gtk_menu_shell_insert(menu, menu_item, 2);
	    }
	  index = TRUE;
	  empty = FALSE;
	}
      /* should be sorted: index, schedule, current, n&n, warning */
      else if (classf == VBI_NOW_AND_NEXT)
	{
	  menu_item =
	    z_gtk_pixmap_menu_item_new(_("Now and Next"),
				       GTK_STOCK_JUSTIFY_FILL);
	  g_object_set_data(G_OBJECT(menu_item), "user-data", widget);
	  g_signal_connect(G_OBJECT(menu_item), "activate",
			     G_CALLBACK(on_subtitle_page_main),
			     GINT_TO_POINTER((count << 16) + VBI_ANY_SUBNO));
	  buffer = g_strdup_printf(_("Page %x"), count);
	  z_tooltip_set(menu_item, buffer);
	  g_free(buffer);
	  gtk_menu_shell_insert(zmenu, menu_item, insert_index++);
	  gtk_widget_show(menu_item);
	  something = TRUE;
	}
      else if (classf == VBI_CURRENT_PROGR)
	{
	  menu_item =
	    z_gtk_pixmap_menu_item_new(_("Current program"),
				       GTK_STOCK_JUSTIFY_FILL);
	  g_object_set_data(G_OBJECT(menu_item), "user-data", widget);
	  g_signal_connect(G_OBJECT(menu_item), "activate",
			     G_CALLBACK(on_subtitle_page_ttxview),
			     GINT_TO_POINTER((count<<16) + subpage));
	  buffer = g_strdup_printf(_("Page %x"), count);
	  z_tooltip_set(menu_item, buffer);
	  g_free(buffer);
	  gtk_menu_shell_insert(zmenu, menu_item, insert_index++);
	  gtk_widget_show(menu_item);
	  something = TRUE;	  
	}
      else if (classf == VBI_PROGR_INDEX)
	{
	  menu_item =
	    z_gtk_pixmap_menu_item_new(_("Program Index"),
				       GTK_STOCK_INDEX);
	  g_object_set_data(G_OBJECT(menu_item), "user-data", widget);
	  g_signal_connect(G_OBJECT(menu_item), "activate",
			     G_CALLBACK(on_subtitle_page_ttxview),
			     GINT_TO_POINTER((count << 16) + VBI_ANY_SUBNO));
	  buffer = g_strdup_printf(_("Page %x"), count);
	  z_tooltip_set(menu_item, buffer);
	  g_free(buffer);
	  gtk_menu_shell_insert(zmenu, menu_item, insert_index++);
	  gtk_widget_show(menu_item);
	  something = TRUE;
	}
      else if (classf == VBI_PROGR_SCHEDULE)
	{
	  menu_item =
	    z_gtk_pixmap_menu_item_new(_("Program Schedule"),
				       "gnome-stock-timer");
	  g_object_set_data(G_OBJECT(menu_item), "user-data", widget);
	  g_signal_connect(G_OBJECT(menu_item), "activate",
			     G_CALLBACK(on_subtitle_page_ttxview),
			     GINT_TO_POINTER((count<<16) + subpage));
	  buffer = g_strdup_printf(_("Page %x"), count);
	  z_tooltip_set(menu_item, buffer);
	  g_free(buffer);
	  gtk_menu_shell_insert(zmenu, menu_item, insert_index++);
	  gtk_widget_show(menu_item);
	  something = TRUE;
	}
      else if (classf == VBI_PROGR_WARNING)
	{
	  menu_item =
	    z_gtk_pixmap_menu_item_new(_("Program Warning"),
				       "gnome-stock-mail");
	  g_object_set_data(G_OBJECT(menu_item), "user-data", widget);
	  g_signal_connect(G_OBJECT(menu_item), "activate",
			     G_CALLBACK(on_subtitle_page_main),
			     GINT_TO_POINTER((count << 16) + VBI_ANY_SUBNO));
	  buffer = g_strdup_printf(_("Page %x"), count);
	  z_tooltip_set(menu_item, buffer);
	  g_free(buffer);
	  gtk_menu_shell_insert(zmenu, menu_item, insert_index++);
	  gtk_widget_show(menu_item);
	  something = TRUE;
	}
    }

  if (empty)
    gtk_widget_destroy(GTK_WIDGET(menu));
  else
    {
      menu_item =
	z_gtk_pixmap_menu_item_new(_("Subtitles"),
				   GTK_STOCK_JUSTIFY_FILL);
      z_tooltip_set(menu_item, _("Select subtitles page"));
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
				GTK_WIDGET(menu));
      gtk_widget_show(menu_item);
      gtk_widget_show(GTK_WIDGET(menu));
      gtk_menu_shell_insert(GTK_MENU_SHELL(zmenu), menu_item, insert_index++);
    }

  if (!empty || something)
    {
      menu_item = gtk_menu_item_new();
      gtk_widget_show(menu_item);
      gtk_menu_shell_insert(GTK_MENU_SHELL(zmenu), menu_item, insert_index++);
    }
}

static
GtkWidget *build_ttxview_popup (ttxview_data *data, gint page, gint subpage)
{
  GtkWidget *popup = build_widget("ttxview_popup", NULL);
  GtkWidget *exp;
  GList *p = g_list_first(bookmarks);
  struct bookmark *bookmark;
  GtkWidget *menuitem;
  GtkWidget *menu;
  gchar *buffer, *buffer2;

  /* convert to fmt_page space */
  data->pop_pgno = page;
  data->pop_subno = subpage;

  gtk_widget_realize(popup);

  if (!page)
    {
      gtk_widget_hide(lookup_widget(popup, "open_in_new_window1"));
      gtk_widget_hide(lookup_widget(popup, "separator8"));
    }
  else
    g_signal_connect(G_OBJECT(lookup_widget(popup,
						"open_in_new_window1")),
		       "activate", G_CALLBACK(popup_new_win), data);

  g_signal_connect(G_OBJECT(lookup_widget(popup, "search1")),
		     "activate",
		     G_CALLBACK(on_ttxview_search_clicked),
		     data);
  g_signal_connect(G_OBJECT(lookup_widget(popup, "add_bookmark")),
		     "activate",
		     G_CALLBACK(new_bookmark), data);
  g_signal_connect(G_OBJECT(lookup_widget(popup, "edit_bookmarks")),
		     "activate",
		     G_CALLBACK(on_edit_bookmarks_activated),
		     data);

  g_signal_connect(G_OBJECT(lookup_widget(popup,
					      "ttx_color_dialog1")),
		       "activate", G_CALLBACK(create_color_dialog), data);

  /* Bookmark entries */
  if (!p)
    gtk_widget_hide(lookup_widget(popup, "separator9"));
  else
    while (p)
      {
	bookmark = (struct bookmark*)p->data;
	menuitem = z_gtk_pixmap_menu_item_new(bookmark->description,
					      GTK_STOCK_JUMP_TO);
	if (bookmark->subpage != VBI_ANY_SUBNO)
	  buffer = g_strdup_printf("%x.%x", bookmark->page, bookmark->subpage);
	else
	  buffer = g_strdup_printf("%x", bookmark->page);
	if (bookmark->channel)
	  buffer2 = g_strdup_printf(_("%s in %s"), buffer, bookmark->channel);
	else
	  buffer2 = g_strdup(buffer);
	z_tooltip_set(menuitem, buffer2);
	g_free(buffer2);
	g_free(buffer);
	g_object_set_data(G_OBJECT(menuitem), "user-data", bookmark);
	gtk_widget_show(menuitem);
	gtk_menu_shell_append(GTK_MENU_SHELL(popup), menuitem);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			   G_CALLBACK(on_bookmark_activated),
			   data);
	p = p->next;
      }

  /* Export modules */
  exp = lookup_widget(popup, "export1");

  if (!vbi_export_info_enum(0))
    gtk_widget_hide(exp);
  else
    {
      vbi_export_info *xm;
      gint i;

      menu = gtk_menu_new();
      gtk_widget_show(menu);
      menuitem = gtk_tearoff_menu_item_new();
      gtk_widget_show(menuitem);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

      for (i = 0; (xm = vbi_export_info_enum(i)); i++)
        if (xm->label) /* for public use */
          {
	    menuitem = gtk_menu_item_new_with_label(xm->label);
	    if (xm->tooltip)
	      z_tooltip_set(menuitem, xm->tooltip);
	    g_object_set_data(G_OBJECT(menuitem), "user-data", xm->keyword);
	    gtk_widget_show(menuitem);
	    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	    g_signal_connect(G_OBJECT(menuitem), "activate",
			       G_CALLBACK(on_export_menu),
			       data);
	  }
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(exp), menu);
    }

  build_subtitles_submenu(data->parent, GTK_MENU_SHELL(popup), FALSE);

  return popup;
}

void
process_ttxview_menu_popup		(GtkWidget	*widget,
					 GdkEventButton	*event,
					 GtkMenuShell	*popup)
{
  gint w, h, col, row;
  vbi_link ld;
  GtkMenuShell *menu;
  ttxview_data *data = (ttxview_data *)
    g_object_get_data(G_OBJECT(widget), "ttxview_data");

  GtkWidget *menu_item;
  gint count = 1;

  if (data)
    {
      gdk_window_get_geometry(widget->window, NULL, NULL, &w, &h, NULL);
      /* convert to fmt_page space */
      col = (event->x*data->fmt_page->columns)/w;
      row = (event->y*data->fmt_page->rows)/h;
      
      ld.pgno = 0;
      ld.subno = 0;

      if (data->fmt_page->text[row * data->fmt_page->columns + col].link)
	{
	  vbi_resolve_link(data->fmt_page, col, row, &ld);
	  
	  if (ld.type != VBI_LINK_PAGE &&
	      ld.type != VBI_LINK_SUBPAGE)
	    {
	      ld.pgno = 0;
	      ld.subno = 0;
	    }
	}
      
      menu = GTK_MENU_SHELL(build_ttxview_popup(data, ld.pgno, ld.subno));
      
      menu_item =
	z_gtk_pixmap_menu_item_new("Zapzilla", GTK_STOCK_JUSTIFY_FILL);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), GTK_WIDGET(menu));
      gtk_widget_show(GTK_WIDGET(menu));
      gtk_widget_show(menu_item);
      gtk_menu_shell_insert(GTK_MENU_SHELL(popup), menu_item, 1);
      menu_item = gtk_menu_item_new();
      gtk_widget_show(menu_item);
      gtk_menu_shell_insert(GTK_MENU_SHELL(popup), menu_item, 2);
      count += 2;
    }
  else
    build_subtitles_submenu(widget, GTK_MENU_SHELL(popup), !data);
}

GdkPixbuf*
ttxview_get_scaled_ttx_page		(GtkWidget	*parent)
{
  ttxview_data	*data = (ttxview_data*)
    g_object_get_data(G_OBJECT(main_window), "ttxview_data");

  if (!data)
    return NULL;

  return get_scaled_ttx_page(data->id);
}

/**
 * Calculates a rectangle, scaling from text coordinates
 * to window dimensions.
 */
static void scale_rect(GdkRectangle *rect,
		       gint x1, gint y1, gint x2, gint y2,
		       gint w, gint h, gint rows, gint columns)
{
  rect->x = (x1 * w) / columns;
  rect->y = (y1 * h) / rows;
  rect->width = ((x2 + 1) * w) / columns - rect->x;
  rect->height = ((y2 + 1) * h) / rows - rect->y;
}

#define SWAP(a, b)			\
do {					\
  gint temp = b;			\
  b = a;				\
  a = temp;				\
} while (0)

#define SATURATE(n, min, max)		\
  (((n) < (min)) ? (min) :		\
    (((n) > (max)) ? (max) : (n)))

#define hidden_row(X) real_hidden_row(X, data)
static int real_hidden_row(int row, ttxview_data * data)
{
  if ((row <= 0) || (row >= 25))
    return FALSE;

  return !!(data->fmt_page->double_height_lower & (1<<row));
}

/**
 * transform the selected region from the first rectangle to the
 * second one. Coordinates are in 40x25 space. No pixel is drawn more
 * than once.
 *
 * {mhs} moved hidden_row stuff and saturation here because it's
 * easier, calculates data->trn_* from dest rect on the fly.
 */
static void transform_region(gint sx1, gint sy1, gint sx2, gint sy2,
			     gint dx1, gint dy1, gint dx2, gint dy2,
			     gboolean stable, gboolean dtable,
			     GdkRegion *exposed, ttxview_data * data)
{
  gint w, h;
  gint h1, h2, h3, h4;
  gint rows, columns;
  GdkRectangle rect;
  GdkRegion *src_region, *dst_region;

  rows = data->fmt_page->rows;
  columns = data->fmt_page->columns;

  gdk_window_get_geometry(data->da->window, NULL, NULL, &w, &h, NULL);
  gdk_gc_set_clip_origin(data->xor_gc, 0, 0);

  data->sel_table = dtable;

  if (sy1 > sy2)
    {
      SWAP(sx1, sx2);
      SWAP(sy1, sy2);
    }

  h1 = hidden_row(sy1);
  h2 = hidden_row(sy1 + 1);
  h3 = hidden_row(sy2);
  h4 = hidden_row(sy2 + 1);

  if (stable || sy1 == sy2 || ((sy2 - sy1) == 1 && h3))
    {
      if (sx1 > sx2)
        SWAP(sx1, sx2);

      scale_rect(&rect, sx1, sy1 - h1, sx2, sy2 + h4, w, h, rows, columns);
      src_region = gdk_region_rectangle(&rect);
    }
  else
    {
      scale_rect(&rect, sx1, sy1 - h1, columns - 1, sy1 + h2, w, h, rows, columns);
      src_region = gdk_region_rectangle(&rect);
      scale_rect(&rect, 0, sy2 - h3, sx2, sy2 + h4, w, h, rows, columns);
      gdk_region_union_with_rect(src_region, &rect);

      sy1 += h2 + 1;
      sy2 -= h3 + 1;

      if (sy2 >= sy1)
        {
          scale_rect(&rect, 0, sy1, columns - 1, sy2, w, h, rows, columns);
	  gdk_region_union_with_rect(src_region, &rect);
	}
    }

  if (dy1 > dy2)
    {
      SWAP(dx1, dx2);
      SWAP(dy1, dy2);
    }

  h1 = hidden_row(dy1);
  h2 = hidden_row(dy1 + 1);
  h3 = hidden_row(dy2);
  h4 = hidden_row(dy2 + 1);

  if (dtable || dy1 == dy2 || ((dy2 - dy1) == 1 && h3))
    {
      if (dx1 > dx2)
        SWAP(dx1, dx2);

      data->trn_col1 = dx1;
      data->trn_row1 = dy1 -= h1;
      data->trn_col2 = dx2;
      data->trn_row2 = dy2 += h4;

      scale_rect(&rect, dx1, dy1, dx2, dy2, w, h, rows, columns);
      dst_region = gdk_region_rectangle(&rect);
    }
  else
    {
      scale_rect(&rect, dx1, dy1 - h1, columns - 1, dy1 + h2, w, h, rows, columns);
      dst_region = gdk_region_rectangle(&rect);
      scale_rect(&rect, 0, dy2 - h3, dx2, dy2 + h4, w, h, rows, columns);
      gdk_region_union_with_rect(dst_region, &rect);

      data->trn_col1 = dx1;
      data->trn_row1 = dy1 + h2;
      data->trn_col2 = dx2;
      data->trn_row2 = dy2 - h3;

      dy1 += h2 + 1;
      dy2 -= h3 + 1;

      if (dy2 >= dy1)
        {
          scale_rect(&rect, 0, dy1, columns - 1, dy2, w, h, rows, columns);
	  gdk_region_union_with_rect(dst_region, &rect);
	}
    }

  if (exposed)
    gdk_region_subtract(src_region, exposed);

  gdk_region_xor(src_region, dst_region);

  gdk_region_destroy(dst_region);

  gdk_gc_set_clip_region(data->xor_gc, src_region);

  gdk_region_destroy(src_region);

  gdk_draw_rectangle(data->da->window, data->xor_gc, TRUE,
		     0, 0, w - 1, h - 1);

  gdk_gc_set_clip_rectangle(data->xor_gc, NULL);
}

static void select_start(gint x, gint y, guint state,
			 ttxview_data * data)
{
  if (data->fmt_page->pgno < 0x100)
    {
      if (data->appbar)
  	gnome_appbar_set_status(GNOME_APPBAR(data->appbar),
  				_("No page loaded"));
      return;
    }

  if (data->selecting)
    return;

  if (data->appbar)
    gnome_appbar_push(GNOME_APPBAR(data->appbar),
		      _("Selecting - hold Shift for table mode"));

  gdk_window_set_cursor(data->da->window, xterm);

  data->ssx = x;
  data->ssy = y;
  data->osx = -1; /* Selection not started yet, wait to move event */
  data->selecting = TRUE;
  data->sel_table = !!(state & GDK_SHIFT_MASK);
  ttx_freeze(data->id);
}

static void select_update(gint x, gint y, guint state,
			  ttxview_data * data)
{
  gint w, h;
  gint ocol, orow, col, row, scol, srow, rows, columns;
  gboolean table = !!(state & GDK_SHIFT_MASK);

  if (!data->selecting)
    return;

  rows = data->fmt_page->rows;
  columns = data->fmt_page->columns;

  gdk_window_get_geometry(data->da->window, NULL, NULL, &w, &h, NULL);

  col = (x * columns) / w;
  row = (y * rows) / h;
  scol = (data->ssx * columns) / w;
  srow = (data->ssy * rows) / h;

  if (data->osx == -1)
    {
      ocol = (data->ssx * columns) / w;
      orow = (data->ssy * rows) / h;
    }
  else
    {
      ocol = (data->osx * columns) / w;
      orow = (data->osy * rows) / h;
    }

  col = SATURATE(col, 0, columns - 1);
  row = SATURATE(row, 0, rows - 1);
  scol = SATURATE(scol, 0, columns - 1);
  srow = SATURATE(srow, 0, rows - 1);
  ocol = SATURATE(ocol, 0, columns - 1);
  orow = SATURATE(orow, 0, rows - 1);

  /* first movement */
  if (data->osx == -1)
    {
      transform_region(columns, rows, columns, rows, scol, srow, col, row,
                       data->sel_table, table, NULL, data);
      data->osx = (x < 0) ? 0 : x;
      data->osy = y;
      return;
    }

  transform_region(scol, srow, ocol, orow, scol, srow, col, row,
                   data->sel_table, table, NULL, data);

  data->osx = (x < 0) ? 0 : x;
  data->osy = y;
}

static void select_stop(ttxview_data * data)
{
  gint w, h;
  gint scol, srow, col, row, rows, columns;

  if (!data->selecting)
    return;

  rows = data->fmt_page->rows;
  columns = data->fmt_page->columns;

  if (data->appbar)
    gnome_appbar_pop(GNOME_APPBAR(data->appbar));

  if (data->osx != -1)
    {
      gdk_window_get_geometry(data->da->window, NULL, NULL, &w, &h, NULL);

      scol = (data->ssx * columns) / w;
      srow = (data->ssy * rows) / h;
      col = (data->osx * columns) / w;
      row = (data->osy * rows) / h;

      col = SATURATE(col, 0, columns - 1);
      row = SATURATE(row, 0, rows - 1);
      scol = SATURATE(scol, 0, columns - 1);
      srow = SATURATE(srow, 0, rows - 1);

      data->sel_col1 = data->trn_col1;
      data->sel_row1 = data->trn_row1;
      data->sel_col2 = data->trn_col2;
      data->sel_row2 = data->trn_row2;

      transform_region(scol, srow, col, row, columns, rows, columns, rows,
                       data->sel_table, data->sel_table, NULL, data);

      memcpy(&data->clipboard_fmt_page, data->fmt_page,
	     sizeof(data->clipboard_fmt_page));

      if (!data->in_clipboard)
	if (gtk_selection_owner_set(data->da,
				    clipboard_atom,
				    GDK_CURRENT_TIME))
	  data->in_clipboard = TRUE;

      if (!data->in_selection)
	if (gtk_selection_owner_set(data->da,
				    GDK_SELECTION_PRIMARY,
				    GDK_CURRENT_TIME))
	  data->in_selection = TRUE;

      if (data->appbar)
	gnome_appbar_set_status(GNOME_APPBAR(data->appbar),
				_("Selection copied to clipboard"));
    }

  data->selecting = FALSE;
  ttx_unfreeze(data->id);

  update_pointer(data);
}

static gboolean
on_ttxview_motion_notify		(GtkWidget	*widget,
					 GdkEventMotion	*event,
					 ttxview_data	*data)
{
  if (!data->selecting)
    update_pointer(data);
  else
    select_update(event->x, event->y, event->state, data);

  return FALSE;
}

static
gboolean on_ttxview_expose_event	(GtkWidget	*widget,
					 GdkEventExpose	*event,
					 ttxview_data	*data)
{
  gint w, h;
  gint scol, srow, col, row, rows, columns;
  GdkRegion *region;

  rows = data->fmt_page->rows;
  columns = data->fmt_page->columns;

  render_ttx_page(data->id, widget->window, widget->style->white_gc,
		  event->area.x, event->area.y,
		  event->area.x, event->area.y,
		  event->area.width, event->area.height);
  
  if (data->selecting && data->osx != -1)
    {
      gdk_window_get_geometry(data->da->window, NULL, NULL, &w, &h, NULL);

      scol = (data->ssx * columns) / w;
      srow = (data->ssy * rows) / h;
      col = (data->osx * columns) / w;
      row = (data->osy * rows) / h;

      col = SATURATE(col, 0, columns - 1);
      row = SATURATE(row, 0, rows - 1);
      scol = SATURATE(scol, 0, columns - 1);
      srow = SATURATE(srow, 0, rows - 1);

      region = gdk_region_rectangle(&event->area);

      transform_region(scol, srow, col, row, scol, srow, col, row,
                       data->sel_table, data->sel_table, region, data);

      gdk_region_destroy(region);
    }

  return TRUE;
}

static gboolean
on_ttxview_button_press			(GtkWidget	*widget,
					 GdkEventButton	*event,
					 ttxview_data	*data)
{
  gint w, h, col, row;
  vbi_link ld;
  GtkMenuShell *menu;
  GError *err;

  gdk_window_get_geometry(widget->window, NULL, NULL, &w, &h, NULL);
  /* convert to fmt_page space */
  col = (event->x*data->fmt_page->columns)/w;
  row = (event->y*data->fmt_page->rows)/h;

  ld.type = VBI_LINK_NONE;
  ld.pgno = 0;
  ld.subno = 0;

  /* Any modifier enters select mode */
  if ((data->fmt_page->text[row * data->fmt_page->columns + col].link) && (!
      (event->state & GDK_SHIFT_MASK ||
       event->state & GDK_CONTROL_MASK ||
       event->state & GDK_MOD1_MASK)))
    vbi_resolve_link(data->fmt_page, col, row, &ld);

  switch (event->button)
    {
    case 1:
      switch (ld.type)
        {
	case VBI_LINK_PAGE:
	case VBI_LINK_SUBPAGE:
	  load_page(ld.pgno, ld.subno, data, NULL);
	  break;

	case VBI_LINK_HTTP:
	case VBI_LINK_FTP:
	case VBI_LINK_EMAIL:
	  gnome_url_show(ld.url, &err);
	  if (err)
	    {
	      ShowBox (_("Cannot show %s:\n%s"), GTK_MESSAGE_ERROR,
		       ld.url, err->message);
	      g_error_free (err);
	    }
	  break;

	default:
	  /* start selecting */
	  select_start(event->x, event->y, event->state, data);
	  break;
	}
      break;
    case 2: /* middle button, open link in new window */
      switch (ld.type)
        {
	case VBI_LINK_PAGE:
	case VBI_LINK_SUBPAGE:
	  cmd_run_printf ("zapping.ttx_open_new(%x, %x)",
			  ld.pgno, ld.subno);
	  break;

	case VBI_LINK_HTTP:
	case VBI_LINK_FTP:
	case VBI_LINK_EMAIL:
	  gnome_url_show(ld.url, &err);
	  if (err)
	    {
	      ShowBox (_("Cannot show %s:\n%s"), GTK_MESSAGE_ERROR,
		       ld.url, err->message);
	      g_error_free (err);
	    }
	  break;

	default:
	  break;
	}
      break;
    default: /* context menu */
      if (data->popup_menu)
	{
	  if (ld.type != VBI_LINK_PAGE &&
	      ld.type != VBI_LINK_SUBPAGE)
	    {
	      ld.pgno = 0;
	      ld.subno = 0;
	    }
	  menu = GTK_MENU_SHELL(build_ttxview_popup(data, ld.pgno, ld.subno));
	  gtk_menu_popup(GTK_MENU (menu), NULL, NULL, NULL,
			 NULL, event->button, event->time);
	}
      break;
    }
  
  return FALSE;
}

static gboolean
on_ttxview_button_release		(GtkWidget	*widget,
					 GdkEventButton	*event,
					 ttxview_data	*data)
{
  select_stop(data);

  return FALSE;
}

static void
on_ttxview_exit				(GtkWidget	*widget,
					 ttxview_data	*data)
{
  GtkWidget *ttxview;

  if (!data->parent_toolbar) /* not attached, standalone window */
    {
      ttxview = lookup_widget(data->toolbar, "ttxview");
      remove_ttxview_instance(data);
      gtk_widget_destroy(ttxview);
    }
  else
    zmisc_restore_previous_mode(main_info);
}

static gboolean
on_ttxview_key_press			(GtkWidget *	widget,
					 GdkEventKey *	event,
					 ttxview_data * data)
{
  gchar *buffer;
  gint digit;

  if ((abs(data->last_time - event->time) < 100) ||
      (event->length > 1))
    data->wait_mode = TRUE; /* load_page will take care */

  data->last_time = event->time;

  digit = event->keyval - GDK_0;

  switch (event->keyval)
    {
    case GDK_KP_0 ... GDK_KP_9:
      digit = event->keyval - GDK_KP_0;

    case GDK_0 ... GDK_9:
      if (event->state & GDK_SHIFT_MASK)
	{	
	  if (event->keyval - GDK_0)
	    load_page (digit * 0x100, VBI_ANY_SUBNO, data, NULL);
	  return TRUE;
	}

      if (data->page >= 0x100)
	data->page = 0;

      data->page = (data->page << 4) + digit;

      if (data->page == 0x999) /* faster typing than "900" */
        data->page = TOP_INDEX_PAGE;

      if (data->page > 0x900)
	data->page = TOP_INDEX_PAGE; /* 0x900 */

      if (data->page >= 0x100)
	load_page (data->page, VBI_ANY_SUBNO, data, NULL);
      else
	{
	  ttx_freeze (data->id);

	  buffer = g_strdup_printf ("%d", vbi_bcd2dec (data->page));

	  gtk_label_set_text (GTK_LABEL (lookup_widget (data->toolbar,
			      "ttxview_url")), buffer);
	  g_free (buffer);
	}

      return TRUE;

    case GDK_q:
    case GDK_Q: /* Quit */
    case GDK_w: /* Close window */
      if (!(event->state & GDK_CONTROL_MASK))
	break;

      /* fall through */

    case GDK_Escape: /* XXX remove Esc */
      /* Zapping-specific: Ignore Ctrl-q key presses when we are
	 attached to the main window */
      /* XXX change to Ctrl-Q/Ctrl-W */
      if (!data->parent_toolbar)
	on_ttxview_exit(NULL, data);

      return TRUE;

    default:
      break;
    }

  return on_user_key_press (widget, event, data);
}

static gint
ttxview_blink			(gpointer	p)
{
  ttxview_data *data = (ttxview_data*)p;

  refresh_ttx_page(data->id, data->da);

  return TRUE;
}

#define BUTTON_CMD(_name, _signal, _cmd)				\
  g_signal_connect (G_OBJECT (lookup_widget (toolbar, #_name)),	\
		      #_signal, G_CALLBACK (on_remote_command1),	\
 		      (gpointer)((const gchar *) _cmd))

static void
connect_toolbar				(ttxview_data *	data)
{
  GtkWidget *toolbar = data->toolbar;

  BUTTON_CMD (ttxview_prev_page,	clicked,
	      "zapping.ttx_page_incr(-1)");
  BUTTON_CMD (ttxview_next_page,	clicked,
	      "zapping.ttx_page_incr(+1)");
  BUTTON_CMD (ttxview_prev_subpage,	clicked,
	      "zapping.ttx_subpage_incr(-1)");
  BUTTON_CMD (ttxview_next_subpage,	clicked,
	      "zapping.ttx_subpage_incr(+1)");
  BUTTON_CMD (ttxview_home,		clicked,
	      "zapping.ttx_home()");
  BUTTON_CMD (ttxview_hold,		toggled,
	      "zapping.ttx_hold()");
  BUTTON_CMD (ttxview_reveal,		toggled,
	      "zapping.ttx_reveal()");
  BUTTON_CMD (ttxview_history_prev,	clicked,
	      "zapping.ttx_history_prev()");
  BUTTON_CMD (ttxview_history_next,	clicked,
	      "zapping.ttx_history_next()");
  BUTTON_CMD (ttxview_clone,		clicked,
	      "zapping.ttx_open_new()");

  /* FIXME: Convert these too */
  g_signal_connect(G_OBJECT(lookup_widget(toolbar, "ttxview_search")),
		     "clicked", G_CALLBACK(on_ttxview_search_clicked),
		     data);

  g_signal_connect(G_OBJECT(lookup_widget(toolbar, "ttxview_exit")), "clicked",
		     G_CALLBACK(on_ttxview_exit),
		     data);
}

static void
ttxview_toolbar_init		(GtkWidget *		toolbar)
{
  struct {
    const gchar *	pixmap;
    const gchar *	replacement;
    const gchar *	widget;
  } buttons[] = {
    { "left.png",	"<", "ttxview_prev_subpage" },
    { "down.png",	"v", "ttxview_prev_page" },
    { "up.png",		"^", "ttxview_next_page" },
    { "right.png",	">", "ttxview_next_subpage" },
    { "reveal.png",	"?", "ttxview_reveal" }
  };
  GtkWidget *widget;
  GtkWidget *button;
  guint i;

  for (i = 0; i < sizeof (buttons) / sizeof (buttons[0]); i++)
    {
      if (!(widget = z_load_pixmap (buttons[i].pixmap)))
	{
	  widget = gtk_label_new (buttons[i].replacement);
	  gtk_widget_show (widget);
	}

      button = lookup_widget (toolbar, buttons[i].widget);
      gtk_container_add (GTK_CONTAINER (button), widget);
    }

  button = lookup_widget (toolbar, "ttxview_reveal");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
				zcg_bool(NULL, "reveal"));
}


GtkWidget*
build_ttxview(void)
{
  GtkWidget *ttxview = build_widget("ttxview", NULL);
  ttxview_data *data;

  if (!zvbi_get_object())
    {
      ShowBox("VBI couldn't be opened, Teletext won't work",
	      GTK_MESSAGE_ERROR);
      return ttxview;
    }

  data = (ttxview_data *) g_malloc(sizeof(ttxview_data));
  memset(data, 0, sizeof(ttxview_data));

  data->da = lookup_widget(ttxview, "drawingarea1");
  data->appbar = lookup_widget(ttxview, "appbar1");
  data->toolbar = lookup_widget(ttxview, "toolbar2");
  data->id = register_ttx_client();
  data->parent = ttxview;
  data->timeout =
    gtk_timeout_add(100, (GtkFunction)event_timeout, data);
  data->fmt_page = get_ttx_fmt_page(data->id);
  data->popup_menu = TRUE;
  g_object_set_data(G_OBJECT(ttxview), "ttxview_data", data);
  data->xor_gc = gdk_gc_new(data->da->window);
  data->vbi_model = zvbi_get_model();
  data->wait_timeout_id = -1;
  gdk_gc_set_function(data->xor_gc, GDK_INVERT);

  ttxview_toolbar_init (data->toolbar);

  gtk_widget_add_events (data->da, GDK_BUTTON_PRESS_MASK |
			 GDK_BUTTON_RELEASE_MASK |
			 GDK_EXPOSURE_MASK |
			 GDK_POINTER_MOTION_MASK |
			 GDK_STRUCTURE_MASK |
			 GDK_VISIBILITY_NOTIFY_MASK |
			 GDK_KEY_PRESS_MASK);

  /* Callbacks */
  g_signal_connect(G_OBJECT(ttxview), "delete-event",
		     G_CALLBACK(on_ttxview_delete_event), data);

  connect_toolbar (data);

  g_signal_connect(G_OBJECT(data->da),
		     "size-allocate",
		     G_CALLBACK(on_ttxview_size_allocate), data);
  g_signal_connect(G_OBJECT(data->da),
		     "expose-event",
		     G_CALLBACK(on_ttxview_expose_event), data);
  g_signal_connect(G_OBJECT(data->da),
		     "motion-notify-event",
		     G_CALLBACK(on_ttxview_motion_notify), data);
  g_signal_connect(G_OBJECT(data->da),
		     "button-press-event",
		     G_CALLBACK(on_ttxview_button_press), data);
  g_signal_connect(G_OBJECT(data->da),
		     "button-release-event",
		     G_CALLBACK(on_ttxview_button_release), data);
  g_signal_connect(G_OBJECT(data->parent),
		     "key-press-event",
		     G_CALLBACK(on_ttxview_key_press), data);
  /* selection (aka clipboard) handling */
  g_signal_connect (G_OBJECT(data->da), "selection_clear_event",
		      G_CALLBACK (selection_clear), data);
  gtk_selection_add_targets(data->da, GDK_SELECTION_PRIMARY,
			    clip_targets, n_clip_targets);
  gtk_selection_add_targets(data->da, clipboard_atom,
			    clip_targets, n_clip_targets);
  g_signal_connect (G_OBJECT(data->da), "selection_get",
		      G_CALLBACK (selection_handle), data);
  /* handle the destruction of the vbi object */
  g_signal_connect (G_OBJECT(data->vbi_model), "changed",
		      G_CALLBACK(on_vbi_model_changed),
		      data);
  /* handle refreshing */
  g_signal_connect (G_OBJECT(refresh), "changed",
		      G_CALLBACK(on_ttxview_refresh),
		      data);

  data->blink_timeout = gtk_timeout_add(BLINK_CYCLE / 4, ttxview_blink, data);

  propagate_toolbar_changes(data->toolbar);
  gtk_toolbar_set_style(GTK_TOOLBAR(data->toolbar),
			GTK_TOOLBAR_ICONS);

  gtk_widget_set_size_request(ttxview, 360, 400);
  gtk_widget_realize(ttxview);
  gdk_window_set_back_pixmap(data->da->window, NULL, FALSE);

  set_ttx_parameters(data->id, zcg_bool(NULL, "reveal"));

  load_page(0x100, VBI_ANY_SUBNO, data, NULL);

  setup_history_gui(data);

  inc_model_count();

  return (ttxview);
}

extern gboolean
on_zapping_key_press			(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	*user_data);

void
ttxview_attach				(GtkWidget	*parent,
					 GtkWidget	*da,
					 GtkWidget	*toolbar,
					 GtkWidget	*appbar)
{
  ttxview_data *data = (ttxview_data *)
    g_object_get_data(G_OBJECT(parent), "ttxview_data");
  gint w, h;

  if (!zvbi_get_object())
    {
      ShowBox("VBI couldn't be opened, Teletext won't work",
	      GTK_MESSAGE_ERROR);
      return;
    }

  /* already being used as TTXView */
  if (data)
    return;

  data = (ttxview_data *) g_malloc(sizeof(ttxview_data));
  memset(data, 0, sizeof(ttxview_data));

  g_object_set_data(G_OBJECT(parent), "ttxview_data", data);

  data->da = da;
  data->appbar = appbar;
  data->toolbar = build_widget("toolbar2", NULL);
  data->id = register_ttx_client();
  data->parent = parent;
  data->parent_toolbar = toolbar;
  data->timeout =
    gtk_timeout_add(50, (GtkFunction)event_timeout, data);
  data->fmt_page = get_ttx_fmt_page(data->id);
  data->popup_menu = FALSE;
  data->xor_gc = gdk_gc_new(data->da->window);
  data->vbi_model = zvbi_get_model();
  data->wait_timeout_id = -1;
  gdk_gc_set_function(data->xor_gc, GDK_INVERT);

  ttxview_toolbar_init (data->toolbar);

  /* Callbacks */
  g_signal_connect(G_OBJECT(data->parent), "delete-event",
		     G_CALLBACK(on_ttxview_delete_event), data);

  /* Redirect key-press-event */
  g_signal_handlers_block_matched (G_OBJECT (data->parent),
				   G_SIGNAL_MATCH_FUNC,
				   0, 0, NULL,
				   G_CALLBACK (on_zapping_key_press),
				   NULL);

  g_signal_connect (G_OBJECT (data->parent), "key-press-event",
		      G_CALLBACK (on_ttxview_key_press), data);

  connect_toolbar (data);

  g_signal_connect(G_OBJECT(data->da),
		     "size-allocate",
		     G_CALLBACK(on_ttxview_size_allocate), data);
  g_signal_connect(G_OBJECT(data->da),
		     "expose-event",
		     G_CALLBACK(on_ttxview_expose_event), data);
  g_signal_connect(G_OBJECT(data->da),
		     "motion-notify-event",
		     G_CALLBACK(on_ttxview_motion_notify), data);
  g_signal_connect(G_OBJECT(data->da),
		     "button-press-event",
		     G_CALLBACK(on_ttxview_button_press), data);
  g_signal_connect(G_OBJECT(data->da),
		     "button-release-event",
		     G_CALLBACK(on_ttxview_button_release), data);
  g_signal_connect (G_OBJECT(data->da), "selection_clear_event",
		      G_CALLBACK (selection_clear), data);
  gtk_selection_add_targets(data->da, GDK_SELECTION_PRIMARY,
			    clip_targets, n_clip_targets);
  gtk_selection_add_targets(data->da, clipboard_atom,
			    clip_targets, n_clip_targets);
  g_signal_connect (G_OBJECT(data->da), "selection_get",
		      G_CALLBACK (selection_handle), data);
  /* handle the destruction of the vbi object */
  g_signal_connect (G_OBJECT(data->vbi_model), "changed",
		      G_CALLBACK(on_vbi_model_changed),
		      data);
  /* handle refreshing */
  g_signal_connect (G_OBJECT(refresh), "changed",
		      G_CALLBACK(on_ttxview_refresh),
		      data);

  gdk_window_set_back_pixmap(data->da->window, NULL, FALSE);

  gdk_window_get_geometry(data->da->window, NULL, NULL, &w, &h, NULL);
  if (w > 10 && h > 10)
    {
      resize_ttx_page(data->id, w, h);
      render_ttx_page(data->id, data->da->window,
		      data->da->style->white_gc,
		      0, 0, 0, 0,
		      w, h);
    }

  data->blink_timeout = gtk_timeout_add(BLINK_CYCLE / 4, ttxview_blink, data);

  gtk_toolbar_set_style(GTK_TOOLBAR(data->toolbar), GTK_TOOLBAR_ICONS);

  gtk_widget_show(data->toolbar);

  data->toolbar_style = GTK_TOOLBAR(data->parent_toolbar)->style;

  gtk_toolbar_set_style(GTK_TOOLBAR(data->parent_toolbar),
			GTK_TOOLBAR_ICONS);

  gtk_toolbar_prepend_space(GTK_TOOLBAR(data->toolbar));
  gtk_toolbar_append_widget(GTK_TOOLBAR(data->parent_toolbar),
			    data->toolbar, "", "");

  set_ttx_parameters(data->id, zcg_bool(NULL, "reveal"));

  load_page(0x100, VBI_ANY_SUBNO, data, NULL);

  setup_history_gui(data);

  inc_model_count();
}

void
ttxview_detach			(GtkWidget	*parent)
{
  ttxview_data *data = (ttxview_data *)
    g_object_get_data(G_OBJECT(parent), "ttxview_data");

  if (!data)
    return;

  gtk_container_remove(GTK_CONTAINER(data->parent_toolbar),
		       data->toolbar);

  g_signal_handlers_disconnect_matched(G_OBJECT(data->parent),
				       G_SIGNAL_MATCH_FUNC |
				       G_SIGNAL_MATCH_DATA,
				       0, 0, NULL,
				       G_CALLBACK(on_ttxview_delete_event),
				       data);

  g_signal_handlers_disconnect_matched(G_OBJECT(data->parent),
				       G_SIGNAL_MATCH_FUNC |
				       G_SIGNAL_MATCH_DATA,
				       0, 0, NULL,
				       G_CALLBACK(on_ttxview_key_press),
				       data);
  g_signal_handlers_unblock_matched (G_OBJECT (data->parent),
				     G_SIGNAL_MATCH_FUNC,
				     0, 0, NULL,
				     G_CALLBACK (on_zapping_key_press),
				     NULL);

  g_signal_handlers_disconnect_matched(G_OBJECT(data->da),
				       G_SIGNAL_MATCH_FUNC |
				       G_SIGNAL_MATCH_DATA,
				       0, 0, NULL,
				       G_CALLBACK(on_ttxview_size_allocate),
				       data);
  g_signal_handlers_disconnect_matched(G_OBJECT(data->da),
				       G_SIGNAL_MATCH_FUNC |
				       G_SIGNAL_MATCH_DATA,
				       0, 0, NULL,
				       G_CALLBACK(on_ttxview_expose_event),
				       data);
  g_signal_handlers_disconnect_matched(G_OBJECT(data->da),
				       G_SIGNAL_MATCH_FUNC |
				       G_SIGNAL_MATCH_DATA,
				       0, 0, NULL,
				       G_CALLBACK(on_ttxview_motion_notify),
				       data);
  g_signal_handlers_disconnect_matched(G_OBJECT(data->da),
				       G_SIGNAL_MATCH_FUNC |
				       G_SIGNAL_MATCH_DATA,
				       0, 0, NULL,
				       G_CALLBACK(on_ttxview_button_press),
				       data);
  g_signal_handlers_disconnect_matched(G_OBJECT(data->da),
				       G_SIGNAL_MATCH_FUNC |
				       G_SIGNAL_MATCH_DATA,
				       0, 0, NULL,
				       G_CALLBACK(on_ttxview_button_release),
				       data);
  g_signal_handlers_disconnect_matched(G_OBJECT(data->da),
				       G_SIGNAL_MATCH_FUNC |
				       G_SIGNAL_MATCH_DATA,
				       0, 0, NULL,
				       G_CALLBACK(selection_handle),
				       data);
  g_signal_handlers_disconnect_matched(G_OBJECT(data->da),
				       G_SIGNAL_MATCH_FUNC |
				       G_SIGNAL_MATCH_DATA,
				       0, 0, NULL,
				       G_CALLBACK(selection_clear),
				       data);

  gtk_toolbar_set_style(GTK_TOOLBAR(data->parent_toolbar),
			data->toolbar_style);

  remove_ttxview_instance(data);

  g_object_set_data(G_OBJECT(parent), "ttxview_data", NULL);
}

gboolean
startup_ttxview (void)
{
  gint i=0;
  gchar *buffer, *buffer2, *buffer3;
  gint page, subpage;

  ttxview_model = ZMODEL(zmodel_new());
  g_object_set_data(G_OBJECT(ttxview_model), "user-data",
		    GINT_TO_POINTER(0));

  hand = gdk_cursor_new (GDK_HAND2);
  arrow = gdk_cursor_new (GDK_LEFT_PTR);
  xterm = gdk_cursor_new (GDK_XTERM);
  model = ZMODEL(zmodel_new());
  refresh = ZMODEL(zmodel_new());
  clipboard_atom = gdk_atom_intern("CLIPBOARD", FALSE);

  zcc_char(g_get_home_dir(), "Directory to export pages to",
	   "exportdir");
  zcc_bool(FALSE, "URE regular expression", "ure_regexp");
  zcc_bool(FALSE, "URE matches disregarding case", "ure_casefold");
  zcc_bool(FALSE, "URE search backwards", "ure_backwards");
  zcc_bool(TRUE, "Reveal hidden characters", "reveal");
  zcc_bool(FALSE, "Selecting a bookmark switchs the current channel",
	   "bookmark_switch");
  zconf_create_integer(128, "Brightness",
		       TXCOLOR_DOMAIN "brightness");
  zconf_create_integer(64, "Contrast", TXCOLOR_DOMAIN "contrast");

  while (zconf_get_nth(i, &buffer, ZCONF_DOMAIN "bookmarks"))
    {
      buffer2 = g_strconcat(buffer, "/page", NULL);
      zconf_get_integer(&page, buffer2);
      g_free(buffer2);
      buffer2 = g_strconcat(buffer, "/subpage", NULL);
      zconf_get_integer(&subpage, buffer2);
      g_free(buffer2);
      buffer2 = g_strconcat(buffer, "/channel", NULL);
      buffer3 = zconf_get_string(NULL, buffer2);
      g_free(buffer2);
      buffer2 = g_strconcat(buffer, "/description", NULL);
      add_bookmark(page, subpage, zconf_get_string(NULL, buffer2), buffer3);
      g_free(buffer2);

      g_free(buffer);
      i++;
    }

  cmd_register ("ttx_page_incr", py_ttx_page_incr, METH_VARARGS,
		_("Advances the given number of pages, 1 by default"),
		"zapping.ttx_page_incr(-4)");
  cmd_register ("ttx_subpage_incr", py_ttx_subpage_incr, METH_VARARGS,
		_("Advances the given number of subpages, 1 by default"),
		"zapping.ttx_subpage_incr(-4)");
  cmd_register ("ttx_home", py_ttx_home, METH_VARARGS,
		_("Opens the home page, usually numbered 100"),
		"zapping.home()");
  cmd_register ("ttx_hold", py_ttx_hold, METH_VARARGS,
		_("Set the hold state, toggles by default"),
		"zapping.hold(1)");
  cmd_register ("ttx_reveal", py_ttx_reveal, METH_VARARGS,
		_("Whether to reveal hidden characters, toggles "
		  "by default"), "zapping.reveal(0)");
  cmd_register ("ttx_history_prev", py_ttx_history_prev, METH_VARARGS,
		_("Loads the previous visited page, if available"),
		"zapping.ttx_history_prev()");
  cmd_register ("ttx_history_next", py_ttx_history_next, METH_VARARGS,
		_("Loads the next visited page, if available"),
		"zapping.ttx_history_next()");
  cmd_register ("ttx_open_new", py_ttx_open_new, METH_VARARGS,
		_("Opens a new TTX view. You can also specify "
		  "the page and subpage to open"),
		"zapping.ttx_open_new(300)");

  return TRUE;
}

void
shutdown_ttxview (void)
{
  gchar *buffer;
  gint i=0;
  GList *p = g_list_first(bookmarks);
  struct bookmark* bookmark;

  gdk_cursor_unref(hand);
  gdk_cursor_unref(arrow);
  gdk_cursor_unref(xterm);

  /* Store the bookmarks in the config */
  zconf_delete(ZCONF_DOMAIN "bookmarks");
  while (p)
    {
      bookmark = (struct bookmark*)p->data;
      buffer = g_strdup_printf(ZCONF_DOMAIN "bookmarks/%d/page", i);
      zconf_create_integer(bookmark->page, "Page", buffer);
      g_free(buffer);
      buffer = g_strdup_printf(ZCONF_DOMAIN "bookmarks/%d/subpage", i);
      zconf_create_integer(bookmark->subpage, "Subpage", buffer);
      g_free(buffer);
      buffer = g_strdup_printf(ZCONF_DOMAIN "bookmarks/%d/description", i);
      zconf_create_string(bookmark->description, "Description", buffer);
      g_free(buffer);
      if (bookmark->channel)
	{
	  buffer = g_strdup_printf(ZCONF_DOMAIN "bookmarks/%d/channel", i);
	  zconf_create_string(bookmark->channel, "Channel", buffer);
	  g_free(buffer);
	}
      p=p->next;
      i++;
    }

  while (bookmarks)
    remove_bookmark(0, FALSE);

  g_object_unref(G_OBJECT(ttxview_model));
  g_object_unref(G_OBJECT(model));
  g_object_unref(G_OBJECT(refresh));
  refresh = NULL;
}

#endif /* HAVE_LIBZVBI */
