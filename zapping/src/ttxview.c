/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 Iñaki García Etxebarria
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

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "interface.h"
#include "ttxview.h"
#include "zvbi.h"
#include "zmisc.h"
#include "../common/fifo.h"

/*
  TODO:
    Better [Hold] handling, advanced controls, bring history back.
*/

typedef struct {
  GdkPixmap		*scaled;
  gint			w, h;
  GtkWidget		*da;
  GdkCursor		*hand; /* global? */
  GdkCursor		*arrow; /* global? */
  int			id; /* TTX client id */
  guint			timeout; /* id */
  gboolean		needs_redraw; /* isn't rendered yet */
  struct fmt_page	*fmt_page; /* current page, formatted */
  gint			page; /* page we are entering */
  gint			subpage; /* current subpage */
  gboolean		extra_controls; /* TRUE: Show extra controls */
  GList			*history; /* Visited pages */
  GList			*history_stack; /* for back, etc... */
  gint			history_stack_size; /* items in history_stack */
  gint			history_sp; /* pointer in the stack */
  gint			monitored_subpage;
  gboolean		no_history; /* don't send to history next page */
} ttxview_data;

static
void scale_image			(GtkWidget	*wid,
					 gint		w,
					 gint		h,
					 ttxview_data	*data)
{
  GtkWidget *widget = lookup_widget(wid, "ttxview");

  if ((data->w != w) ||
      (data->h != h))
    {
      if (data->scaled)
	gdk_pixmap_unref(data->scaled);
      data->scaled = gdk_pixmap_new(widget->window, w, h, -1);
      data->w = w;
      data->h = h;
    }

  if (data->scaled)
    render_ttx_page(data->id, data->scaled,
		    widget->style->white_gc, w, h);
}

static
void setup_history_gui	(ttxview_data *data)
{
  GtkWidget *prev, *next;

  prev = lookup_widget(data->da, "ttxview_history_prev");
  next = lookup_widget(data->da, "ttxview_history_next");

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
  GtkCList *history_clist;
  gchar * clist_entry[1];
  GList *p;

  pc_subpage = (subpage == ANY_SUB) ? 0xffff : (subpage & 0xffff);
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

  if (g_list_find(data->history, GINT_TO_POINTER(page)))
    data->history = g_list_remove(data->history, GINT_TO_POINTER(page));

  data->history = g_list_prepend(data->history, GINT_TO_POINTER(page));

  history_clist = GTK_CLIST(lookup_widget(data->da, "ttxview_history"));
  gtk_clist_freeze(history_clist);
  gtk_clist_clear(history_clist);
  p = g_list_first(data->history);
  while (p)
    {
      clist_entry[0]=g_strdup_printf("%x", GPOINTER_TO_INT(p->data));
      gtk_clist_append(history_clist, clist_entry);
      g_free(clist_entry[0]);
      p = p->next;
    }
  gtk_clist_thaw(history_clist);
}

/* API flaw */
static
void set_stock_pixmap	(GtkWidget	*button,
			 const gchar	*new_pix)
{
  GtkWidget *widget = GTK_BIN(button)->child;
  GList *node = g_list_first(GTK_BOX(widget)->children)->next;

  widget = GTK_WIDGET(((GtkBoxChild*)(node->data))->widget);

  gnome_stock_set_icon(GNOME_STOCK(widget), new_pix);
}

static
void set_tooltip	(GtkWidget	*widget,
			 const gchar	*new_tip)
{
  GtkTooltipsData *td = gtk_tooltips_data_get(widget);

  gtk_tooltips_set_tip(td->tooltips, widget, new_tip,
		       "private tip, or, er, just babbling, you know");
}

static gboolean
on_ttxview_delete_event			(GtkWidget	*widget,
					 GdkEvent	*event,
					 ttxview_data	*data)
{
  if (data->scaled)
    gdk_pixmap_unref(data->scaled);

  gdk_cursor_destroy(data->hand);
  gdk_cursor_destroy(data->arrow);

  unregister_ttx_client(data->id);
  gtk_timeout_remove(data->timeout);

  g_free(data);

  return FALSE;
}

static gint
event_timeout				(ttxview_data	*data)
{
  enum ttx_message msg;
  gint w, h;
  GtkWidget *widget;
  gchar *buffer;

  while ((msg = peek_ttx_message(data->id)))
    {
      switch (msg)
	{
	case TTX_PAGE_RECEIVED:
	  gdk_window_get_size(data->da->window, &w, &h);
	  scale_image(data->da, w, h, data);
	  gdk_window_clear_area_e(data->da->window, 0, 0, w, h);
	  data->subpage = data->fmt_page->vtp->subno;
	  widget = lookup_widget(data->da, "ttxview_subpage");
	  buffer = g_strdup_printf("S%d", data->subpage);
	  gtk_label_set_text(GTK_LABEL(widget), buffer);
	  if (!data->no_history)
	    append_history(data->fmt_page->vtp->pgno,
			   data->monitored_subpage, data);
	  data->no_history = FALSE;
	  g_free(buffer);
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

static void
load_page (int page, int subpage, ttxview_data *data)
{
  GtkWidget *appbar1 = lookup_widget(data->da, "appbar1");
  GtkWidget *ttxview_url = lookup_widget(data->da, "ttxview_url");
  GtkWidget *widget;
  gchar *buffer;

  buffer = g_strdup_printf("%d", hex2dec(page));
  gtk_label_set_text(GTK_LABEL(ttxview_url), buffer);
  g_free(buffer);

  data->subpage = subpage;
  data->monitored_subpage = subpage;
  widget = lookup_widget(data->da, "ttxview_subpage");
  if (subpage != ANY_SUB)
    buffer = g_strdup_printf("S%d", hex2dec(data->subpage));
  else
    buffer = g_strdup(_(""));
  gtk_label_set_text(GTK_LABEL(widget), buffer);
  g_free(buffer);

  if (subpage == ANY_SUB)
    buffer = g_strdup_printf(_("Loading page %d..."), hex2dec(page));
  else
    buffer = g_strdup_printf(_("Loading subpage %d..."),
			     hex2dec(subpage));
  gnome_appbar_set_status(GNOME_APPBAR(appbar1), buffer);
  g_free(buffer);

  gtk_widget_grab_focus(data->da);

  monitor_ttx_page(data->id, page, subpage);
}

static
void on_ttxview_home_clicked		(GtkButton	*button,
					 ttxview_data	*data)
{
  int page, subpage;

  get_ttx_index(data->id, &page, &subpage);
  load_page(page, subpage, data);
}

static
void on_ttxview_hold_toggled		(GtkToggleButton *button,
					 ttxview_data	*data)
{
  if (gtk_toggle_button_get_active(button))
    load_page(data->fmt_page->vtp->pgno, data->fmt_page->vtp->subno,
	      data);
  else
    load_page(data->fmt_page->vtp->pgno, ANY_SUB, data);
}

static
void on_ttxview_prev_subpage_clicked	(GtkButton	*button,
					 ttxview_data	*data)
{
  int subpage = dec2hex(hex2dec(data->subpage) - 1);
  if (subpage < 0)
    subpage = 0x99;
  load_page(data->fmt_page->vtp->pgno, subpage, data);
}

static
void on_ttxview_next_subpage_clicked	(GtkButton	*button,
					 ttxview_data	*data)
{
  int subpage = dec2hex(hex2dec(data->subpage) + 1);
  if (subpage >= 0x100)
    subpage = 1;
  load_page(data->fmt_page->vtp->pgno, subpage, data);
}

static
void on_ttxview_search_clicked		(GtkButton	*button,
					 ttxview_data	*data)
{
  ShowBox("Not done yet", GNOME_MESSAGE_BOX_INFO);
}

static
void on_ttxview_history_prev_clicked	(GtkButton	*button,
					 ttxview_data	*data)
{
  gint page, page_code, pc_subpage;

  if (data->history_sp == 0)
    return;

  data->history_sp--;

  page_code = GPOINTER_TO_INT(g_list_nth(data->history_stack,
					 data->history_sp)->data);
  page = page_code >> 16;
  pc_subpage = (page_code & 0xffff);
  pc_subpage = (pc_subpage == 0xffff) ? ANY_SUB : pc_subpage;

  data->no_history = TRUE;
  load_page(page, pc_subpage, data);
  setup_history_gui(data);
}

static
void on_ttxview_history_next_clicked	(GtkButton	*button,
					 ttxview_data	*data)
{
  gint page, page_code, pc_subpage;

  if (data->history_stack_size == (data->history_sp+1))
    return;
  data->history_sp++;

  page_code = GPOINTER_TO_INT(g_list_nth(data->history_stack,
					 data->history_sp)->data);
  page = page_code >> 16;
  pc_subpage = (page_code & 0xffff);
  pc_subpage = (pc_subpage == 0xffff) ? ANY_SUB : pc_subpage;

  data->no_history = TRUE;
  load_page(page, pc_subpage, data);
  setup_history_gui(data);
}

static void
on_ttxview_history_select_row          (GtkCList        *history_clist,
					gint             row,
					gint             column,
					GdkEventButton  *event,
					ttxview_data	*data)
{
  load_page(GPOINTER_TO_INT(g_list_nth(data->history, row)->data),
	    ANY_SUB, data);
}

static
void on_ttxview_clone_clicked		(GtkButton	*button,
					 ttxview_data	*data)
{
  GtkWidget *dolly = build_ttxview();
  load_page(data->fmt_page->vtp->pgno, data->fmt_page->vtp->subno,
	    (ttxview_data*)gtk_object_get_user_data(GTK_OBJECT(dolly)));
  gtk_widget_show(dolly);
}

static
void on_ttxview_extra_clicked		(GtkButton	*button,
					 ttxview_data	*data)
{
  GtkWidget *handlebox1 = lookup_widget(GTK_WIDGET(button), "handlebox1");

  data->extra_controls = !data->extra_controls;

  if (data->extra_controls)
    {
      set_stock_pixmap(lookup_widget(handlebox1, "ttxview_extra"),
		       GNOME_STOCK_PIXMAP_BOOK_BLUE);
      set_tooltip(lookup_widget(handlebox1, "ttxview_extra"),
		  _("Hide the extra controls"));
      gtk_widget_show(handlebox1);
    }
  else
    {
      set_stock_pixmap(lookup_widget(handlebox1, "ttxview_extra"),
		       GNOME_STOCK_PIXMAP_BOOK_OPEN);
      set_tooltip(lookup_widget(handlebox1, "ttxview_extra"),
		  _("Show the extra controls"));
      gtk_widget_hide(handlebox1);
    }
}

static
void on_ttxview_size_allocate		(GtkWidget	*widget,
					 GtkAllocation	*allocation,
					 ttxview_data	*data)
{
  data->needs_redraw = TRUE;

  gdk_window_clear_area_e(widget->window, 0, 0, allocation->width,
			  allocation->height);
}

static
gboolean on_ttxview_expose_event	(GtkWidget	*widget,
					 GdkEventExpose	*event,
					 ttxview_data	*data)
{
  gint w, h;

  if (data->needs_redraw)
    {
      gdk_window_get_size(widget->window, &w, &h);
      scale_image(widget, w, h, data);
      data->needs_redraw = FALSE;
    }

  if (!data->scaled)
    return TRUE;

  gdk_draw_pixmap(widget->window, widget->style->white_gc,
		  data->scaled, event->area.x, event->area.y,
		  event->area.x, event->area.y,
		  event->area.width, event->area.height);

  return TRUE;
}

static gboolean
on_ttxview_motion_notify	(GtkWidget	*widget,
				 GdkEventMotion	*event,
				 ttxview_data	*data)
{
  gint w, h, col, row;
  gint page, subpage;
  gchar *buffer;
  GtkWidget *appbar1 = lookup_widget(widget, "appbar1");

  gdk_window_get_size(widget->window, &w, &h);
  /* convert to fmt_page space */
  col = (event->x*40)/w;
  row = (event->y*25)/h;
  page = data->fmt_page->data[row][col].link_page;
  subpage = data->fmt_page->data[row][col].link_subpage;

  if (page)
    {
      if (subpage == (guchar)ANY_SUB)
	buffer = g_strdup_printf(_("Page %d"), hex2dec(page));
      else
	buffer = g_strdup_printf(_("Subpage %d"), hex2dec(subpage));
      gnome_appbar_set_status(GNOME_APPBAR(appbar1), buffer);
      g_free(buffer);
      gdk_window_set_cursor(widget->window, data->hand);
    }
  else
    {
      gnome_appbar_set_status(GNOME_APPBAR(appbar1), "");
      gdk_window_set_cursor(widget->window, data->arrow);
    }

  return FALSE;
}

static gboolean
on_ttxview_button_press			(GtkWidget	*widget,
					 GdkEventButton	*event,
					 ttxview_data	*data)
{
  gint w, h, col, row, page, subpage;
  GtkWidget *dolly;

  gdk_window_get_size(widget->window, &w, &h);
  /* convert to fmt_page space */
  col = (event->x*40)/w;
  row = (event->y*25)/h;
  page = data->fmt_page->data[row][col].link_page;
  subpage = data->fmt_page->data[row][col].link_subpage;
  if (subpage == (guchar)ANY_SUB)
    subpage = ANY_SUB;

  if (page)
    switch (event->button)
      {
      case 1:
	load_page(page, subpage, data);
	break;
      default:
	dolly = build_ttxview();
	load_page(page, subpage,
		  (ttxview_data*)gtk_object_get_user_data(GTK_OBJECT(dolly)));
	gtk_widget_show(dolly);
	break;
      }

  return FALSE;
}

static
gboolean on_ttxview_key_press		(GtkWidget	*widget,
					 GdkEventKey	*event,
					 ttxview_data	*data)
{
  gchar *buffer;

  switch (event->keyval)
    {
    case GDK_0...GDK_9:
      if (data->page >= 0x100)
	data->page = 0;
      data->page = (data->page<<4)+event->keyval-GDK_0;
      if (data->page > 0x899)
	data->page = 0x899;
      if (data->page >= 0x100)
	load_page(data->page, ANY_SUB, data);
      else
	{
	  buffer = g_strdup_printf("%d", hex2dec(data->page));
	  gtk_label_set_text(GTK_LABEL(lookup_widget(widget,
			     "ttxview_url")), buffer);
	  g_free(buffer);
	}
      break;
    case GDK_KP_0...GDK_KP_9:
      if (data->page >= 0x100)
	data->page = 0;
      data->page = (data->page<<4)+event->keyval-GDK_KP_0;
      if (data->page > 0x899)
	data->page = 0x899;
      if (data->page >= 0x100)
	load_page(data->page, ANY_SUB, data);
      else
	{
	  buffer = g_strdup_printf("%d", hex2dec(data->page));
	  gtk_label_set_text(GTK_LABEL(lookup_widget(widget,
			     "ttxview_url")), buffer);
	  g_free(buffer);
	}
      break;
    case GDK_Page_Down:
    case GDK_KP_Page_Down:
      if (data->page < 0x100)
	data->page = data->fmt_page->vtp->pgno + 0x10;
      else
	data->page += 0x10;
      if (data->page > 0x899)
	data->page = 0x100;
      load_page(data->page, ANY_SUB, data);
      break;
    case GDK_Page_Up:
    case GDK_KP_Page_Up:
      if (data->page < 0x100)
	data->page = data->fmt_page->vtp->pgno - 0x10;
      else
	data->page = data->page - 0x10;
      if (data->page < 0x100)
	data->page = 0x899;
      load_page(data->page, ANY_SUB, data);
      break;
    case GDK_KP_Up:
    case GDK_Up:
    case GDK_KP_Left:
    case GDK_Left:
      if (data->page < 0x100)
	data->page = dec2hex(hex2dec(data->fmt_page->vtp->pgno) - 1);
      else
	data->page = dec2hex(hex2dec(data->page) - 1);
      if (data->page < 0x100)
	data->page = 0x899;
      load_page(data->page, ANY_SUB, data);
      break;
    case GDK_KP_Down:
    case GDK_Down:
    case GDK_KP_Right:
    case GDK_Right:
      if (data->page < 0x100)
	data->page = dec2hex(hex2dec(data->fmt_page->vtp->pgno) + 1);
      else
	data->page = dec2hex(hex2dec(data->page) + 1);
      if (data->page > 0x899)
	data->page = 0x100;
      load_page(data->page, ANY_SUB, data);
      break;
    default:
      break;
    }

  return TRUE;
}

GtkWidget*
build_ttxview(void)
{
  GtkWidget *ttxview = create_ttxview();
  ttxview_data *data;

  if (!zvbi_get_object())
    {
      ShowBox("VBI couldn't be opened, Teletext won't work",
	      GNOME_MESSAGE_BOX_ERROR);
      return ttxview;
    }

  data = g_malloc(sizeof(ttxview_data));
  memset(data, 0, sizeof(ttxview_data));

  data->needs_redraw = TRUE;
  data->da = lookup_widget(ttxview, "drawingarea1");
  data->id = register_ttx_client();
  data->timeout =
    gtk_timeout_add(50, (GtkFunction)event_timeout, data);
  data->fmt_page = get_ttx_fmt_page(data->id);
  gtk_object_set_user_data(GTK_OBJECT(ttxview), data);

  /* Callbacks */
  gtk_signal_connect(GTK_OBJECT(ttxview), "delete-event",
		     GTK_SIGNAL_FUNC(on_ttxview_delete_event), data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview,
		      "ttxview_prev_subpage")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_prev_subpage_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview,
		      "ttxview_next_subpage")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_next_subpage_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview, "ttxview_home")),
		     "clicked", GTK_SIGNAL_FUNC(on_ttxview_home_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview, "ttxview_extra")),
		     "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_extra_clicked), data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview, "ttxview_hold")),
		     "toggled", GTK_SIGNAL_FUNC(on_ttxview_hold_toggled),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview, "ttxview_clone")),
		     "clicked", GTK_SIGNAL_FUNC(on_ttxview_clone_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview, "ttxview_search")),
		     "clicked", GTK_SIGNAL_FUNC(on_ttxview_search_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview,
		     "ttxview_history_prev")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_history_prev_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview,
		     "ttxview_history_next")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_history_next_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview,
		     "ttxview_history")), "select-row",
		     GTK_SIGNAL_FUNC(on_ttxview_history_select_row),
		     data);
  gtk_signal_connect(GTK_OBJECT(data->da),
		     "size-allocate",
		     GTK_SIGNAL_FUNC(on_ttxview_size_allocate), data);
  gtk_signal_connect(GTK_OBJECT(data->da),
		     "expose-event",
		     GTK_SIGNAL_FUNC(on_ttxview_expose_event), data);
  gtk_signal_connect(GTK_OBJECT(data->da),
		     "motion-notify-event",
		     GTK_SIGNAL_FUNC(on_ttxview_motion_notify), data);
  gtk_signal_connect(GTK_OBJECT(data->da),
		     "button-press-event",
		     GTK_SIGNAL_FUNC(on_ttxview_button_press), data);
  gtk_signal_connect(GTK_OBJECT(ttxview),
		     "key-press-event",
		     GTK_SIGNAL_FUNC(on_ttxview_key_press), data);

  gtk_toolbar_set_style(GTK_TOOLBAR(lookup_widget(ttxview,
			  "toolbar2")),	GTK_TOOLBAR_ICONS);
  gtk_toolbar_set_style(GTK_TOOLBAR(lookup_widget(ttxview,
			  "toolbar3")),	GTK_TOOLBAR_ICONS);
  gtk_toolbar_set_style(GTK_TOOLBAR(lookup_widget(ttxview,
			  "toolbar4")),	GTK_TOOLBAR_ICONS);
  gtk_handle_box_set_handle_position(GTK_HANDLE_BOX(lookup_widget(ttxview,
			    "handlebox1")), GTK_POS_TOP);
  gtk_handle_box_set_snap_edge(GTK_HANDLE_BOX(lookup_widget(ttxview,
			    "handlebox1")), GTK_POS_TOP);
  gtk_widget_set_usize(ttxview, 360, 400);
  gtk_widget_realize(ttxview);
  gdk_window_set_back_pixmap(data->da->window, NULL, FALSE);

  data->hand = gdk_cursor_new (GDK_HAND2);
  data->arrow = gdk_cursor_new(GDK_LEFT_PTR);

  load_page(0x100, ANY_SUB, data);

  return (ttxview);
}
