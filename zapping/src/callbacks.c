/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2002 Iñaki García Etxebarria
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

#define ZCONF_DOMAIN "/zapping/internal/callbacks/"
#include "zconf.h"
#include "callbacks.h"
#include "remote.h"
#include "plugins.h"
#include "globals.h"
#include "zmisc.h"
#include "interface.h"
#include "v4linterface.h"
#include "zvbi.h"
#include "ttxview.h"

void
on_zapping_delete_event		(GtkWidget *		widget,
				 gpointer		user_data)
{
  python_command (widget, "zapping.quit()");
}

gboolean
on_tv_screen_button_press_event	(GtkWidget *		widget,
				 GdkEventButton *	event,
				 gpointer		user_data)
{
  switch (event->button)
    {
    case 2:
      if (main_info->current_mode == TVENG_TELETEXT)
	return FALSE; /* pass_on */

      python_command (widget, "zapping.switch_mode('fullscreen')");

      return TRUE; /* handled */

    case 3:
      {
	GtkWidget *menu;

	menu = create_popup_menu1 (event);

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL,
			NULL, event->button, event->time);

	return TRUE; /* handled */
      }

    case 4:
      python_command (widget, "zapping.channel_up()");
      return TRUE; /* handled */

    case 5:
      python_command (widget, "zapping.channel_down()");
      return TRUE; /* handled */

    default:
      break;
    }

  return FALSE; /* pass on */
}
