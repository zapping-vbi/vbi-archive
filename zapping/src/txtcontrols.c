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
  g_message("deleted");
  return FALSE;
}

void
on_history_selection_changed           (GtkList         *list,
					gpointer         user_data)
{
  g_message("selection_changed");
}

void
on_vtx_previous_clicked                (GtkButton       *button,
					gpointer         user_data)
{
  g_message("previous");
}

void
on_vtx_next_clicked                    (GtkButton       *button,
					gpointer         user_data)
{
  g_message("next");
}
