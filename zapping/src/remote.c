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

#include <assert.h>
#include "remote.h"

PyObject *		dict;

static GList *		c_list;
static GtkWidget *	c_widget;

/* Callback glue for Gtk signals. */
void
on_python_command1		(GtkWidget *		widget,
				 const gchar *		cmd)
{
  char *buf;
  unsigned int len;

  c_widget = widget;

  len = strlen (cmd);

  buf = malloc (len + 2);
  assert (buf != NULL);

  memcpy (buf, cmd, len);

  buf[len + 0] = '\n';
  buf[len + 1] = 0;

  PyRun_SimpleString (buf);

  free (buf);
}

void
on_python_command2		(GtkWidget *		widget,
				 gpointer 		unused,
				 const gchar *		cmd)
{
  on_python_command1 (widget, cmd);
}

void
on_python_command3		(GtkWidget *		widget,
				 gpointer 		unused1,
				 gpointer 		unused2,
				 const gchar *		cmd)
{
  on_python_command1 (widget, cmd);
}

void
python_command_printf		(GtkWidget *		widget,
				 const gchar *		fmt,
				 ...)
{
  char *buf;
  va_list ap;
  int result;

  va_start (ap, fmt);
  result = vasprintf (&buf, fmt, ap);
  va_end (ap);

  if (-1 == result)
    {
      perror ("vsprintf");
      return;
    }

  python_command (widget, buf);

  free (buf);
}

GList *
cmd_list			(void)
{
  return g_list_copy (c_list);
}

/* Widget sending the last command, or NULL. */
GtkWidget *
python_command_widget		(void)
{
  return c_widget;
}

#define OPTIONAL	(1 << 0)
#define INTEGER		(1 << 1)
#define STRING		(1 << 2)
#define TOGGLE		(1 << 3) /* 0 = off, 1 = on, <nothing> = toggle */

struct cmd_txl {
  const gchar *		name;
  guint			num_args;
  guint			flags;
};

static const struct cmd_txl
cmd_txl_table [] = {
  { "mute",		1,	OPTIONAL | TOGGLE },
  { "volume_incr",	1,	OPTIONAL | INTEGER }, /* incr (+1) */
  { "ttx_open_new",	2,	OPTIONAL | INTEGER }, /* page (100), sub (any) */
  { "ttx_history_next",	0,	0 },
  { "ttx_history_prev",	0,	0 },
  { "ttx_page_incr",	1,	OPTIONAL | INTEGER }, /* incr (+1) */
  { "ttx_subpage_incr",	1,	OPTIONAL | INTEGER }, /* incr (+1) */
  { "ttx_reveal",	1,	OPTIONAL | TOGGLE },
  { "ttx_home",		0,	0 },
  { "ttx_hold",		1,	OPTIONAL | TOGGLE },
  { "stoprec",		0,	0 },
/*  { "pauserec",	0,	0 }, */ /* never implemented */
  { "quickrec",		1,	OPTIONAL | STRING }, /* (last) */
  { "record",		1,	OPTIONAL | STRING }, /* (last) */
  { "quickshot",	1,	OPTIONAL | STRING }, /* (last) */
  { "screenshot",	1,	OPTIONAL | STRING }, /* (last) */
  { "lookup_channel",	1,	STRING },
  { "set_channel",	1,	STRING },
  { "channel_down",	0,	0 },
  { "channel_up",	0,	0 },
  { "quit",		0,	0 },
  { "subtitle_overlay",	1,	OPTIONAL | TOGGLE },
  { "restore_mode",	1,	OPTIONAL | STRING }, /* (toggle) */
  { "toggle_mode",	1,	OPTIONAL | STRING }, /* (toggle) */
  { "switch_mode",	1,	STRING },
};

/* Translate pre-0.7 command to new Python command. You must
   g_free() the returned string. */
gchar *
cmd_compatibility		(const gchar *		cmd)
{
  const gchar *s = cmd;
  gchar *d = NULL, *d1;
  guint i, j, args = 0;

  if (!s || *s == 0)
    return g_strdup ("");

  while (g_unichar_isspace (g_utf8_get_char_validated (s, -1)))
    s = g_utf8_next_char (s);

  if (0 == strncmp (s, "zapping.", 8))
    return g_strdup (cmd);

  for (i = 0; i < G_N_ELEMENTS (cmd_txl_table); i++)
    {
      guint n = strlen (cmd_txl_table[i].name);

      if (0 == strncmp (s, cmd_txl_table[i].name, n))
	{
	  s += n;
	  break;
	}
    }

  if (i >= G_N_ELEMENTS (cmd_txl_table))
    {
      if (0 == strncmp (s, "zapping.volume_incr", 19))
	goto volume_incr;
      else
	goto bad_cmd;
    }

  if (0 == strcmp (cmd_txl_table[i].name, "volume_incr"))
    {
    volume_incr:
      d = g_strdup ("zapping.control_incr('volume'");
      args = 1;
    }
  else
    {
      d = g_strconcat ("zapping.", cmd_txl_table[i].name, "(", NULL);
    }

  if (*s)
    {
      if (!g_unichar_isspace (g_utf8_get_char_validated (s, -1)))
	goto bad_cmd;

      s = g_utf8_next_char (s);
    }

  for (j = 0; j < cmd_txl_table[i].num_args; j++)
    {
      const gchar *s1 = s;
      gchar *arg;

      if (*s == 0)
	{
	  if (cmd_txl_table[i].flags & OPTIONAL)
	    break;
	  else
	    goto bad_cmd;
	}

      while (*s != 0 && !g_unichar_isspace (g_utf8_get_char_validated (s, -1)))
	s = g_utf8_next_char (s);

      arg = g_strndup (s1, s - s1);

      if (cmd_txl_table[i].flags & STRING)
	d1 = g_strconcat (d, (args > 0) ? ", " : "", "'", arg, "'", NULL);
      else
	d1 = g_strconcat (d, (args > 0) ? ", " : "", arg, NULL);

      args++;

      g_free (d);
      d = d1;

      g_free (arg);

      while (*s && g_unichar_isspace (g_utf8_get_char_validated (s, -1)))
	s = g_utf8_next_char (s);
    }

  if (*s)
    goto bad_cmd;

  d1 = g_strconcat (d, ")", NULL);
  g_free (d);

  return d1;

 bad_cmd:
  g_free (d);
  return g_strconcat ("/* ", cmd, " */", NULL);  
}

/* We leak happily here. Not a bug really because Python requires
   the passed method def to be around all of its lifetime, and we
   will be registering every command just once during the
   program's lifetime. */
void
cmd_register			(const gchar *		name,
				 PyCFunction		cfunc,
				 int			flags,
				 const gchar *		doc,
				 const gchar *		usage)
{
  PyMethodDef *def;
  PyObject *func;

  def = (PyMethodDef *) malloc (sizeof (*def));
  assert (def != NULL);

  def->ml_name = strdup(name);
  def->ml_meth = cfunc;
  def->ml_flags = flags;
  def->ml_doc = strdup(doc);
  
  func = PyCFunction_New (def, NULL);
  PyDict_SetItemString (dict, (char *) name, func);
  Py_DECREF (func);

  /* Append this cmd to the list of known methods. */
  c_list = g_list_append (c_list, g_strdup (usage));
}

void
shutdown_remote			(void)
{
  /* Unload the Python interpreter */
  Py_Finalize ();
}

void
startup_remote			(void)
{
  PyMethodDef empty [] = {
    { NULL, NULL }
  };
  PyObject *module;

  /* Initialize the Python interpreter. */
  Py_SetProgramName (PACKAGE);
  Py_Initialize ();

  /* Create the zapping class. */
  module = Py_InitModule ("zapping", empty);
  dict = PyModule_GetDict (module);

  /* Load the zapping module. */
  PyRun_SimpleString ("import zapping\n");
}
