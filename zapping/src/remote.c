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
 * Generic command implementation. This is mostly useful for plugins,
 * that can use this routines for executing arbitrary functions in a
 * clean way.
 */

#include "callbacks.h"
#include "zmisc.h"
#include "zvbi.h"
#include "remote.h"
#include "interface.h"

#include <tveng.h>

/* Pointers to important structs */
extern GtkWidget * main_window;
extern tveng_device_info * main_info;

/* The meaning of arg and the returned gpointer value depend on the
   function you call. The command checking isn't case sensitive */
gpointer remote_command(gchar *command, gpointer arg)
{
  if (!strcasecmp(command, "quit"))
    {
      on_zapping_delete_event(main_window, NULL, NULL);
    }
  else if (!strcasecmp(command, "switch_mode"))
    {
      enum tveng_capture_mode capture_mode =
	(enum tveng_capture_mode) arg;
      return (GINT_TO_POINTER(zmisc_switch_mode(capture_mode,
						main_info)));
    }
  else if (!strcasecmp(command, "get_cur_channel"))
    {
      extern int cur_tuned_channel;
      return (GINT_TO_POINTER(cur_tuned_channel));
    }
  else if (!strcasecmp(command, "get_num_channels"))
    {
      return (GINT_TO_POINTER(tveng_tuned_channel_num()));
    }
  else if (!strcasecmp(command, "set_channel"))
    {
      int channel_num = GPOINTER_TO_INT(arg);
      GtkWidget * Channels = lookup_widget(main_window, "Channels");
      if (channel_num < 0)
	channel_num = 0;
      if (channel_num >= tveng_tuned_channel_num())
	channel_num = tveng_tuned_channel_num()-1;
      on_channel_activate(NULL,
			  GINT_TO_POINTER(channel_num));
      /* Update the option menu */
      gtk_option_menu_set_history(GTK_OPTION_MENU (Channels),
				  channel_num);
    }
  else if (!strcasecmp(command, "channel_up"))
    {
      on_channel_up1_activate(GTK_MENU_ITEM(lookup_widget(main_window,
					    "channel_up1")),
			      NULL);
    }
  else if (!strcasecmp(command, "channel_down"))
    {
      on_channel_down1_activate(GTK_MENU_ITEM(lookup_widget(main_window,
					      "channel_down1")),
				NULL);
    }
  else if (!strcasecmp(command, "set_vbi_mode"))
    {
      if (GPOINTER_TO_INT(arg))
	on_videotext1_activate(GTK_MENU_ITEM(lookup_widget(main_window,
					     "videotext1")),
			       NULL);
      else
	zvbi_set_mode(FALSE);
    }
  return NULL;
}
