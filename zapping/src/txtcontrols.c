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
  The code in this file controls the Teletext navigation dialog. No
  real code here, all the work is done in zvbi.c
*/
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include "zvbi.h"
#include "callbacks.h"
#include "zmisc.h"
#include "interface.h"

gboolean
on_txtcontrols_delete_event            (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  GtkWidget **p;
  p = (GtkWidget**)gtk_object_get_user_data(GTK_OBJECT(widget));
  g_assert(p != NULL);
  *p = NULL;
  return FALSE;
}

void
on_history_select_row                  (GtkCList        *history_clist,
					gint             row,
					gint             column,
					GdkEventButton  *event,
					gpointer         user_data)
{
  gboolean ignored =
    GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(history_clist)));

  if (!ignored)
    zvbi_select_visited_page(row);

  /* Set the ignore select_row flag to FALSE */
  gtk_object_set_user_data(GTK_OBJECT(history_clist),
			   GINT_TO_POINTER(FALSE));
}

void
on_vtx_previous_page_clicked           (GtkButton       *button,
					gpointer         user_data)
{
  gint cur_page;
  zvbi_get_current_page(&cur_page, NULL);
  cur_page = hex2dec(cur_page)-1;
  if (cur_page < 100)
    cur_page = 899;
  zvbi_set_current_page(dec2hex(cur_page), ANY_SUB);
}

void
on_vtx_next_page_clicked               (GtkButton       *button,
					gpointer         user_data)
{
  gint cur_page;
  zvbi_get_current_page(&cur_page, NULL);
  cur_page = hex2dec(cur_page)+1;
  if (cur_page > 899)
    cur_page = 100;
  zvbi_set_current_page(dec2hex(cur_page), ANY_SUB);
}

void
on_vtx_history_previous_clicked        (GtkButton       *button,
					gpointer         user_data)
{
  zvbi_history_previous();
}

void
on_vtx_history_next_clicked            (GtkButton       *button,
					gpointer         user_data)
{
  zvbi_history_next();
}

void
on_manual_page_value_changed          (GtkAdjustment    *adj,
				       GtkSpinButton    *spin)
{
  GtkSpinButton * manual_subpage =
    GTK_SPIN_BUTTON(lookup_widget(GTK_WIDGET(spin),
				  "manual_subpage"));
  /* This is enough, the next callback will be activated, and it will
     do the real work */
  gtk_spin_button_set_value(manual_subpage, -1);
  /* raise the signal manually... GtkAdjustment checks before calling
     value_changed */
  gtk_adjustment_value_changed(manual_subpage->adjustment);
}

void
on_manual_subpage_value_changed       (GtkAdjustment    *adj,
				       GtkSpinButton    *spin)
{
  GtkSpinButton * manual_page =
    GTK_SPIN_BUTTON(lookup_widget(GTK_WIDGET(spin), "manual_page"));
  gint page = gtk_spin_button_get_value_as_int(manual_page);
  gint subpage = gtk_spin_button_get_value_as_int(spin);
  gint cur_page, cur_subpage;

  /* check if we should ignore this call, this is used when ZVBI is
     modifying this fields, we shouldn't load the page */
  if (gtk_object_get_user_data(GTK_OBJECT(spin)) == (gpointer)0xdeadbeef)
    return;

  zvbi_get_current_page(&cur_page, &cur_subpage);

  page = dec2hex(page);
  if (subpage == -1)
    subpage = ANY_SUB;
  else
    subpage = dec2hex(subpage);

  if ((cur_page == page) && (cur_subpage == subpage))
    return; /* Do nothing if there's no need for it */

  zvbi_set_current_page(page, subpage);
}
