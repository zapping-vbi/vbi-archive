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

typedef struct {
  GdkPixbuf		*unscaled;
  GdkPixmap		*scaled;
  gboolean		needs_redraw; /* isn't rendered yet */
} ttxview_data;

static gboolean
on_ttxview_delete_event			(GtkWidget	*widget,
					 GdkEvent	*event,
					 ttxview_data	*data)
{
  if (data->unscaled)
    gdk_pixbuf_unref(data->unscaled);
  if (data->scaled)
    gdk_pixmap_unref(data->scaled);
  g_free(data);

  return FALSE;
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
}

static
void on_ttxview_home_clicked		(GtkButton	*button,
					 ttxview_data	*data)
{
  GtkWidget *ttxview_url = lookup_widget(GTK_WIDGET(button),
					 "ttxview_url");

  gtk_spin_button_set_value(GTK_SPIN_BUTTON(ttxview_url), 100);
}

static
void on_ttxview_clone_clicked		(GtkButton	*button,
					 ttxview_data	*data)
{
  gtk_widget_show(build_ttxview());
}

static
void scale_image			(GtkWidget	*wid,
					 gint		w,
					 gint		h,
					 ttxview_data	*data)
{
  GtkWidget *widget = lookup_widget(wid, "drawingarea1");
  GdkPixbuf *scaled_temp;

  if ((!data->unscaled) ||
      (w < 0) ||
      (h < 0))
    return;
  
  if (data->scaled)
    gdk_pixmap_unref(data->scaled);
  data->scaled = gdk_pixmap_new(widget->window, w, h, -1);

  if (data->scaled)
    {
      scaled_temp = gdk_pixbuf_scale_simple(data->unscaled,
					    w, h,
					    GDK_INTERP_BILINEAR);
      if (scaled_temp)
	{
	  gdk_pixbuf_render_to_drawable(scaled_temp,
					data->scaled,
					widget->style->white_gc,
					0, 0, 0, 0, w, h,
					GDK_RGB_DITHER_NONE, 0, 0);
	  gdk_pixbuf_unref(scaled_temp);
	}
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
void on_ttxview_expose_event		(GtkWidget	*widget,
					 GdkEvent	*event,
					 ttxview_data	*data)
{
  gint w, h;

  if (data->needs_redraw)
    {
      gdk_window_get_size(widget->window, &w, &h);
      scale_image(widget, w, h, data);
      gdk_window_set_back_pixmap(widget->window, NULL, FALSE);
    }

  if (!data->scaled)
    return;

  gdk_draw_pixmap(widget->window, widget->style->white_gc,
		  data->scaled, 0, 0, 0, 0, -1, -1);
}

GtkWidget*
build_ttxview(void)
{
  GtkWidget *ttxview = create_ttxview();
  GtkAdjustment *adj;
  ttxview_data *data;
  gchar *filename;

  data = g_malloc(sizeof(ttxview_data));
  memset(data, 0, sizeof(ttxview_data));
  filename = g_strdup_printf("%s/%s%d.jpeg", PACKAGE_DATA_DIR,
			     "../pixmaps/zapping/vt_loading",
			     (rand()%2)+1);
  data->unscaled = gdk_pixbuf_new_from_file(filename);
  if (data->unscaled)
    data->needs_redraw = TRUE;
  g_free(filename);

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
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview, "drawingarea1")),
		     "size-allocate",
		     GTK_SIGNAL_FUNC(on_ttxview_size_allocate), data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview, "drawingarea1")),
		     "expose-event",
		     GTK_SIGNAL_FUNC(on_ttxview_expose_event), data);

  return (ttxview);
}
