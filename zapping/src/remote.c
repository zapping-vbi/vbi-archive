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

#include <ctype.h>
#include <glade/glade.h>

#include "callbacks.h"
#include "zmisc.h"
#include "zvbi.h"
#include "remote.h"
#include "interface.h"
#include "ttxview.h"
#include "frequencies.h"
#include "v4linterface.h"

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
      cmd_execute (main_window, "quit");
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
      gchar *buf = g_strdup_printf("set_channel %u", GPOINTER_TO_INT(arg));

      cmd_execute (NULL, buf);
      g_free(buf);
    }
  else if (!strcasecmp(command, "channel_up"))
    {
      cmd_execute (NULL, "channel_up");
    }
  else if (!strcasecmp(command, "channel_down"))
    {
      cmd_execute (NULL, "channel_down");
    }
#ifdef HAVE_LIBZVBI
  else if (!strcasecmp(command, "set_vbi_mode"))
    {
      if (GPOINTER_TO_INT(arg))
	cmd_execute (main_window, "switch_mode teletext");
      else
	ttxview_detach (main_window);
    }
  else if (!strcasecmp(command, "load_page"))
    {
      gint page, subpage;

      zmisc_switch_mode(TVENG_NO_CAPTURE, main_info);

      page = GPOINTER_TO_INT(arg) >> 16;
      page = vbi_dec2bcd(page);
      subpage = GPOINTER_TO_INT(arg) & 0xFFFF;
      subpage = vbi_dec2bcd(subpage) & 0xFF;

      open_in_ttxview(main_window, page, subpage);
    }
#endif /* HAVE_LIBZVBI */

  return NULL;
}

/*
 *  Zapping commands Mk II
 */

#define CMD_LOG 0

typedef struct command {
  struct command *		next;
  gchar *			name;
  cmd_func *			func;
  gpointer			user_data;
} command;

static command *		command_list = NULL;

 /*
gboolean
example_cmd				(GtkWidget *widget,
					 gint argc,
					 gchar **argv,
					 gpointer user_data)
{
  gint arg1 = 123;
  gchar *arg2 = "foo";

  printf("This is command %s called for widget %p\n",
	 argv[0], widget);

  if (argc > 1)
    val1 = strtol(argv[1], NULL, 0);

  if (argc > 2)
    val2 = argv[2];

  cmd_return ("the result");

  return TRUE; // success
}
 */

void
cmd_return (const gchar *command_return_value)
{
  /* 2do */
}

/**
 * cmd_execute:
 * @widget: Passed to the command function.
 * @command_string: 
 * 
 * Execute a command string. Commands are separated by newline or
 * semicolon. Command name and arguments are separated by
 * whitespace except when "quoted". Backslash escapes the next
 * character, i. e. newline, semicolon, quote or backslash.
 * The command return value is --?
 * 
 * Return value: 
 * %TRUE on success.
 **/
gboolean
cmd_execute				(GtkWidget *	widget,
					 const gchar *	command_string)
{
  static gint recursion = 0;
  gchar buf[1024], *argv[10];
  const gchar *s = command_string;
  gboolean r = TRUE;
  command *cmd;
  gint argc;
  gint i;

  g_assert (s != NULL);

  if (!*s)
    return TRUE; /* nop */

  if (recursion > 20)
    return FALSE;

  if (CMD_LOG)
    fprintf (stderr, "cmd_execute '%s'\n", s);

  while (*s != 0)
    {
      buf[0] = 0;
      argv[0] = buf;

      argc = 0;
      i = 0;

      while (*s == ' ' || *s == '\t' || *s == '\n' || *s == ';')
	s++;

      while (*s != 0 && *s != '\n' && *s != ';')
	{
	  gboolean quote = FALSE;

	  if (argc >= 9)
	    return FALSE; /* too many arguments */

	  while (*s == ' ' || *s == '\t')
	    s++;

	  while (*s != 0 && *s != '\n'
		 && (quote || (*s != ' ' && *s != '\t' && *s != ';')))
	    {
	      if (*s == '\"') /* " stupid emacs c mode */
		{
		  quote = !quote;
		  continue;
		}
	      else if (*s == '\\')
		{
		  if (*++s == 0)
		    break;
		}

	      if (i >= 1023)
		return FALSE; /* command too long */

	      buf[i++] = *s++;
	    }

	  buf[i++] = 0;

	  argv[++argc] = buf + i;
	}

      if (!buf[0])
	continue; /* blank command */

      for (cmd = command_list; cmd; cmd = cmd->next)
	if (strcmp (cmd->name, buf) == 0)
	  break;

      if (!cmd)
	{
	  g_warning ("Unknown command '%s'\n", command_string);
	  return FALSE;
	}

      recursion++;

      r = cmd->func (widget, argc, argv, cmd->user_data);

      recursion--;

      if (!r)
	break;
    }

  return r;
}

gboolean
cmd_execute_printf			(GtkWidget *	widget,
					 const gchar *	template,
					 ...)
{
  gchar buf[1024];
  va_list ap;

  va_start (ap, template);

  vsnprintf (buf, sizeof (buf) - 1, template, ap);

  va_end (ap);

  return cmd_execute (widget, buf);
}

gboolean			on_remote_command_blocked = FALSE;

void
on_remote_command1			(GtkWidget *	widget,
					 gpointer 	user_data)
{
  gchar *command = (gchar *) user_data;

  if (on_remote_command_blocked)
    return;

  g_assert (command != NULL && command[0]);

  if (CMD_LOG)
    {
      gchar *long_name = (gchar *) glade_get_widget_long_name (widget);

      fprintf (stderr, "on_remote_command %p '%s' '%s'\n",
	       widget, long_name, command);
    }

  if (!cmd_execute (widget, command))
    if (CMD_LOG)
      fprintf (stderr, "command failed\n");
}

void
on_remote_command2			(GtkWidget *	widget,
					 gpointer	ignored,
					 gpointer 	user_data)
{
  gchar *command = (gchar *) user_data;

  if (on_remote_command_blocked)
    return;

  g_assert (command != NULL && command[0]);

  if (CMD_LOG)
    {
      gchar *long_name = (gchar *) glade_get_widget_long_name (widget);

      fprintf (stderr, "on_remote_command %p '%s' '%s'\n",
	       widget, long_name, command);
    }

  if (!cmd_execute (widget, command))
    if (CMD_LOG)
      fprintf (stderr, "command failed\n");
}

/* Command registry */

/**
 * cmd_list:
 * 
 * Creates a list with the name of all registered commands.
 * 
 * Return value: 
 * GList *. When done call g_list_free().
 **/
GList *
cmd_list (void)
{
  command *cmd;
  GList *list = NULL;

  for (cmd = command_list; cmd; cmd = cmd->next)
    list = g_list_append (list, cmd->name);

  return list;
}

static void
cmd_delete (command *cmd)
{
  g_free (cmd->name);
  g_free (cmd);
}

void
cmd_remove (const gchar *name)
{
  command *cmd, **cmdpp;

  for (cmdpp = &command_list; (cmd = *cmdpp); cmdpp = &cmd->next)
    if (strcmp (cmd->name, name) == 0)
      {
	*cmdpp = cmd->next;
	cmd_delete (cmd);
	break;
      }
}

void
cmd_register (const gchar *name, cmd_func *func, gpointer user_data)
{
  command *cmd;

  g_assert (name != NULL && name[0]);
  g_assert (func);

  for (cmd = command_list; cmd; cmd = cmd->next)
    if (strcmp (cmd->name, name) == 0)
      break;

  if (cmd)
    g_error ("Command %s registered twice.\n", name);

  cmd = g_malloc (sizeof (*cmd));
  cmd->next = command_list;
  command_list = cmd;

  cmd->name = g_strdup (name);
  cmd->func = func;
  cmd->user_data = user_data;
}

void
shutdown_remote (void)
{
  command *cmd;

  while ((cmd = command_list))
    {
      command_list = cmd->next;
      cmd_delete (cmd);
    }
}

void
startup_remote (void)
{
}
