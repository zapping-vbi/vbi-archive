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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "callbacks.h"
#include "zmisc.h"
#include "zvbi.h"
#include "remote.h"
#include "interface.h"
#include "ttxview.h"
#include "frequencies.h"

#include <tveng.h>

/* Pointers to important structs */
extern GtkWidget * main_window;
extern tveng_device_info * main_info;
extern tveng_tuned_channel * global_channel_list;

static void parse_single_command(const gchar *command)
{
  gint t;
  gchar *p = g_strdup(command);
  gchar *b;
  tveng_tuned_channel *tc;

  *p = 0;

  if (strstr(command, "set_channel"))
    {
      if ((sscanf(command, "set_channel %d", &t) != 1) &&
	  (sscanf(command, "set_channel %s", p) != 1))
	g_warning("Wrong number of parameters to set_channel,"
		  " syntax is\n\tset_channel [channel_num:integer |"
		  " channel_name:string]");
      else
	{
	  if (! *p)
	    {
	      g_message("Command: set_channel by index: %d", t);
	      remote_command("set_channel", GINT_TO_POINTER(t));
	    }
	  else
	    {
	      g_assert((b = strstr(command, p)) != NULL);
	      g_message("Command: set_channel by name: %s", b);
	      tc =
		tveng_retrieve_tuned_channel_by_name(b, 0,
						     global_channel_list);
	      if (tc)
		remote_command("set_channel",
			       GINT_TO_POINTER(tc->index));
	      else
		g_warning("Channel not found: \"%s\"", b);
	    }
	}
    }
  else /* a bit sparse right now, will get bigger */
    {
      g_message("Unknown command \"%s\", ignored", command);
    }

  g_free(p);
}

void run_command(const gchar *command)
{
  gchar *buffer = g_strdup(command);
  gint i = 0;

  while (*command)
    {
      if (!i && *command == ' ')
	{
	  command++;
	  continue;
	}

      if (*command == '\n' || *command == ';')
	{
	  buffer[i] = 0;
	  parse_single_command(buffer);
	  i = 0;
	}
      else
	buffer[i++] = *command;
      command++;
    }

  if (i)
    {
      buffer[i] = 0;
      parse_single_command(buffer);
    }

  g_free(buffer);
}

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
  else if (!strcasecmp(command, "get_channel_info"))
    {
      tveng_tuned_channel *channel =
	tveng_retrieve_tuned_channel_by_index(GPOINTER_TO_INT(arg),
					      global_channel_list);
      if (channel)
	{
	  /* tveng_clear_tuned_channel(result) when not longer needed */
	  return tveng_append_tuned_channel(channel, NULL);
	}
      return NULL;
    }
  else if (!strcasecmp(command, "get_num_channels"))
    {
      return (GINT_TO_POINTER(tveng_tuned_channel_num(global_channel_list)));
    }
  else if (!strcasecmp(command, "set_channel"))
    {
      int channel_num = GPOINTER_TO_INT(arg);
      GtkWidget * Channels = lookup_widget(main_window, "Channels");
      if (channel_num < 0)
	channel_num = 0;
      if (channel_num >= tveng_tuned_channel_num(global_channel_list))
	channel_num = tveng_tuned_channel_num(global_channel_list)-1;
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
	ttxview_detach(main_window);
    }
  else if (!strcasecmp(command, "load_page"))
    {
      gint page, subpage;

      zmisc_switch_mode(TVENG_NO_CAPTURE, main_info);

      page = GPOINTER_TO_INT(arg)>>16;
      page = dec2bcd(page);
      subpage = GPOINTER_TO_INT(arg)&0xfff;
      subpage = dec2bcd(subpage);
      
      open_in_ttxview(main_window, page, subpage);
    }

  return NULL;
}
