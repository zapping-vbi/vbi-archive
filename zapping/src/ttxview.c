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
#include "../common/fifo.h"

typedef struct {
  GdkPixmap		*scaled;
  gint			w, h;
  GtkWidget		*da; /* fixme: make sure it's realized */
  GdkCursor		*hand; /* global? */
  GdkCursor		*arrow; /* global? */
  int			id; /* TTX client id */
  guint			timeout; /* id */
  gboolean		needs_redraw; /* isn't rendered yet */
  struct fmt_page	*fmt_page; /* current page, formatted */
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

  while ((msg = peek_ttx_message(data->id)))
    {
      switch (msg)
	{
	case TTX_PAGE_RECEIVED:
	  gdk_window_get_size(data->da->window, &w, &h);
	  scale_image(data->da, w, h, data);
	  gdk_window_clear_area_e(data->da->window, 0, 0, w, h);
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

static
void on_ttxview_back_clicked		(GtkButton	*button,
					 ttxview_data	*data)
{
}

static
void on_ttxview_forward_clicked		(GtkButton	*button,
					 ttxview_data	*data)
{
}

static
void on_ttxview_url_changed		(GtkAdjustment	*adj,
					 ttxview_data	*data)
{
  GtkWidget *ttxview =
    GTK_WIDGET(gtk_object_get_user_data(GTK_OBJECT(adj)));
  GtkWidget *appbar1 = lookup_widget(ttxview, "appbar1");
  GtkWidget *ttxview_url = lookup_widget(ttxview, "ttxview_url");
  gchar *buffer;

  gtk_entry_set_text(GTK_ENTRY(ttxview_url), "");
  buffer = g_strdup_printf(_("Loading %d..."), (gint)adj->value);
  gnome_appbar_set_status(GNOME_APPBAR(appbar1), buffer);
  g_free(buffer);

  monitor_ttx_page(data->id, dec2hex(((gint)adj->value)), ANY_SUB);
}

static
void on_ttxview_home_clicked		(GtkButton	*button,
					 ttxview_data	*data)
{
  GtkWidget *ttxview_url = lookup_widget(GTK_WIDGET(button),
					 "ttxview_url");
  int page, subpage;

  get_ttx_index(data->id, &page, &subpage);
  page = hex2dec(page);

  /* FIXME: No support for subpages yet */
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(ttxview_url), page);
}

static
void on_ttxview_clone_clicked		(GtkButton	*button,
					 ttxview_data	*data)
{
  gtk_widget_show(build_ttxview());
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
  GtkWidget *ttxview_url = lookup_widget(widget, "ttxview_url");

  gdk_window_get_size(widget->window, &w, &h);
  /* convert to fmt_page space */
  col = (event->x*40)/w;
  row = (event->y*25)/h;
  page = data->fmt_page->data[row][col].link_page;
  subpage = data->fmt_page->data[row][col].link_subpage;

  /* FIXME: Subpage support */
  if (page)
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(ttxview_url),
			      hex2dec(page));

  return FALSE;
}

GtkWidget*
build_ttxview(void)
{
  GtkWidget *ttxview = create_ttxview();
  GtkAdjustment *adj;
  ttxview_data *data;

  data = g_malloc(sizeof(ttxview_data));
  memset(data, 0, sizeof(ttxview_data));

  data->needs_redraw = TRUE;
  data->da = lookup_widget(ttxview, "drawingarea1");
  data->id = register_ttx_client();
  monitor_ttx_page(data->id, 0x100, ANY_SUB);
  data->timeout =
    gtk_timeout_add(50, (GtkFunction)event_timeout, data);
  data->fmt_page = get_ttx_fmt_page(data->id);

  /* Callbacks */
  gtk_signal_connect(GTK_OBJECT(ttxview), "delete-event",
		     GTK_SIGNAL_FUNC(on_ttxview_delete_event), data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview, "ttxview_back")),
		     "clicked", GTK_SIGNAL_FUNC(on_ttxview_back_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview, "ttxview_forward")),
		     "clicked", GTK_SIGNAL_FUNC(on_ttxview_forward_clicked),
		     data);
  adj = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(
	  lookup_widget(ttxview, "ttxview_url")));
  gtk_object_set_user_data(GTK_OBJECT(adj), ttxview);
  gtk_signal_connect(GTK_OBJECT(adj), "value-changed",
		     GTK_SIGNAL_FUNC(on_ttxview_url_changed), data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview, "ttxview_home")),
		     "clicked", GTK_SIGNAL_FUNC(on_ttxview_home_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview, "ttxview_clone")),
		     "clicked", GTK_SIGNAL_FUNC(on_ttxview_clone_clicked),
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

  gtk_widget_realize(data->da);
  gdk_window_set_back_pixmap(data->da->window, NULL, FALSE);

  data->hand = gdk_cursor_new (GDK_HAND2);
  data->arrow = gdk_cursor_new(GDK_LEFT_PTR);

  return (ttxview);
}
