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
 * Manages the error console
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gnome.h>
#include "zmisc.h" /* ec_add_message declaration */
#include "interface.h"

static GtkWidget *ec = NULL;

extern gint console_errors; /* main.c */

static
void on_clean_console_clicked		(GtkWidget	*button,
					 gpointer	data)
{
  GtkWidget *text4 = lookup_widget(button, "text4");

  gtk_editable_delete_text(GTK_EDITABLE(text4), 0, -1);
}

static GtkWidget *create_console	(void)
{
  gchar *buffer;
  time_t t = time(NULL);
  GdkColor red;

  if (ec)
    return ec;

  ec = build_widget("error_console", NULL);

  gnome_dialog_close_hides(GNOME_DIALOG(ec), TRUE);
  gnome_dialog_set_default(GNOME_DIALOG(ec), 0);
  gnome_dialog_button_connect(GNOME_DIALOG(ec), 1,
			      GTK_SIGNAL_FUNC(on_clean_console_clicked),
			      NULL);
  gnome_dialog_set_close(GNOME_DIALOG(ec), TRUE);

  buffer =
    g_strdup_printf("Console started, stardate %s"
		     "Please tell the maintainer about any bugs you find.",
		    ctime(&t));

  gdk_color_parse("red", &red);
  if (!gdk_colormap_alloc_color(gdk_colormap_get_system(), &red, FALSE,
				TRUE))
    ec_add_message(buffer, FALSE, NULL);
  else
    {
      ec_add_message(buffer, FALSE, &red);
      gdk_colormap_free_colors(gdk_colormap_get_system(), &red, 1);
    }
  
  g_free(buffer);

  return ec;
}

void ec_add_message			(const gchar	*text,
					 gboolean	show,
					 GdkColor	*color)
{
  GtkWidget *console;
  GtkWidget *text4;
  gchar * new_text;

  if (console_errors)
    {
      fprintf(stderr, "\n%s\n", text);
      return;
    }

  console = create_console();
  text4 = lookup_widget(console, "text4");
  new_text = g_strdup_printf("\n%s\n", text);

  gtk_text_insert(GTK_TEXT(text4), NULL, color, NULL, new_text, -1);

  gtk_text_set_point(GTK_TEXT(text4), gtk_text_get_length(GTK_TEXT(text4)));

  g_free(new_text);

  if (show)
    gtk_widget_show(console);
}
