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
#ifndef __REMOTE_H__
#define __REMOTE_H__

#include <glib.h>

/* The meaning of arg and the returned gpointer value depend on the
   function you call. The command checking isn't case sensitive */
gpointer remote_command(gchar *command, gpointer arg);

/**
 * Parses the given command and calls old_remote_command appropiately.
 * Command is a set of lines containing a single command each, or many
 * commands separated by ";"
 * Example > set_channel 5;
 */
void run_command(const gchar *command);

/*
  Implemented commands and description
  ------------------------------------
  - command="quit", arg=ignored, returns=NULL
  Quits the program, just like if the user had pressed the delete
  button in the window.

  - command="switch_mode", arg=GINT_TO_POINTER(enum tveng_capture_mode
  new_mode), returns=GINT_TO_POINTER(-1 on error, 0 on success)
  Switchs between different running modes. See tveng.h for valid
  capture mode values.

  - command="get_mode".
  Not implemented, use the current_mode field in
  your copy of the tveng_device_info * struct.

  - command="get_cur_channel", arg=ignored,
  returns=GINT_TO_POINTER(current_channel).
  Returns the currently selected station in Zapping.

  - command="get_channel_info", arg=index
  returns=tveng_tuned_channel* channel, or NULL if not found.
  Info about the given channel. Use tveng_clear_tuned_channel(result)
  when not longer needed.

  - command="get_num_channels", arg=ignored,
  returns=GINT_TO_POINTER(number_of_channels).
  Number of channels in the channel list.

  - command="set_channel", arg=GINT_TO_POINTER(channel_index),
  returns=NULL
  Sets a given channel in the Channel list. The given value is clipped
  to the nearest valid values.

  - commmand="channel_up", command="channel_down", arg=ignored,
  returns=NULL.
  Go to the previous/next channel in the channel list.

  - command="set_vbi_mode", arg=GINT_TO_POINTER(gboolean init_vbi),
  returns=NULL.
  Starts/stops Teletext, depending on the value of arg. If arg is
  TRUE, then teletext is started. Please note that if you stop
  Teletext, it doesn't restore the previous capture mode, you need to
  do that by hand.

  - command="load_page", arg=GINT_TO_POINTER((page<<16)+subpage)
  Loads the given page in the main window, tries to start TTX if it
  isn't already on (i.e., if info->current_mode != TVENG_CAPTURE_NONE).
  Loading all subpages of a page is done by giving the value
  ANY_SUB to subpage. Example: load_page((100<<16)+ANY_SUB)
*/

/*
 *  Zapping commands Mk II
 */

typedef gboolean (cmd_func)	(GtkWidget *	widget,
				 gint		argc,
				 gchar **	argv,
				 gpointer	user_data);

extern void			startup_remote		(void);
extern void			shutdown_remote		(void);

extern void			cmd_register		(const gchar *	name,
							 cmd_func *	func,
							 gpointer	user_data);
extern void			cmd_remove		(const gchar *	name);

extern GList *			cmd_list		(void);

extern gboolean			cmd_execute		(GtkWidget *	widget,
							 const gchar *	command_string);
extern gboolean			cmd_execute_printf	(GtkWidget *	widget,
							 const gchar *	templ,
							 ...);
extern void			on_remote_command1	(GtkWidget *	widget,
							 gpointer	user_data);
extern void			on_remote_command2	(GtkWidget *	widget,
							 gpointer	ignored,
							 gpointer	user_data);

extern gboolean			on_remote_command_blocked;

#define ORC_BLOCK(_func)						\
(on_remote_command_blocked = TRUE, _func, on_remote_command_blocked = FALSE)

/*
({
  GtkObject object = GTK_OBJECT (_object);
  GtkHandler *handler0 = gtk_object_get_data_by_id (object, gtk_handler_quark);
  GtkHandler *handler;
  typeof (_func) temp;

  for (handler = handler0; handler; handler = handler->next)
    if (handler->id > 0 && handler->func == on_remote_command)
      handler->blocked += 1;

  temp = _func;

  for (handler = handler0; handler; handler = handler->next)
    if (handler->id > 0 && handler->func == on_remote_command)
      handler->blocked -= 1;

  temp;
})
*/

extern void			cmd_return		(const gchar *	command_return_value);

#endif /* remote.h */
