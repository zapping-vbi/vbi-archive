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

#include "remote.h"

PyObject 	*dict;

static GList *list = NULL;

void startup_remote (void)
{
  PyMethodDef empty [] = {
    {NULL,		NULL}
  };
  PyObject *module;
  
  /* Initialize the Python interpreter */
  Py_SetProgramName (PACKAGE);
  Py_Initialize ();

  /* Create the zapping class */
  module = Py_InitModule ("zapping", empty);
  dict = PyModule_GetDict (module);

  /* Load the zapping module */
  PyRun_SimpleString("import zapping\n");
}

void shutdown_remote (void)
{
  /* Unload the Python interpreter */
  Py_Finalize ();
}

/**
 * We are leaking happily here. Not a bug really because Python
 * requires the passed method def to be around all of its lifetime,
 * and we will be registering every command just once during the
 * program's lifetime.
 */
void cmd_register (const char *name, PyCFunction cfunc,
		   int flags,
		   const char *doc, const char *usage)
{
  PyMethodDef *def = (PyMethodDef*)malloc(sizeof(PyMethodDef));
  PyObject *func;

  def->ml_name = strdup(name);
  def->ml_meth = cfunc;
  def->ml_flags = flags;
  def->ml_doc = strdup(doc);
  
  func = PyCFunction_New(def, NULL);
  PyDict_SetItemString(dict, (char*)name, func);
  Py_DECREF(func);

  /* Append this cmd to the list of known methods */
  list = g_list_append (list, strdup(usage));
}

void cmd_run (const char *cmd)
{
  char *buf = (char*)malloc(strlen(cmd)+5);
  sprintf(buf, "%s\n", cmd);
  PyRun_SimpleString (buf);
  free (buf);
}

void cmd_run_printf (const char *fmt, ...)
{
  char *buf;
  va_list ap;
  int result;

  va_start (ap, fmt);

  result = vasprintf (&buf, fmt, ap);

  va_end (ap);

  if (result == -1)
    {
      perror ("vsprintf");
      return;
    }

  cmd_run (buf);
  free (buf);
}

GList *cmd_list (void)
{
  return g_list_copy (list);
}

static gpointer last_caller = NULL;

void
on_remote_command1 (void *lc, const char *cmd)
{
  last_caller = lc;
  cmd_run (cmd);
}

void
on_remote_command2 (void *lc, void *unused, const char *cmd)
{
  last_caller = lc;
  cmd_run (cmd);
}

gpointer
remote_last_caller (void)
{
  return last_caller;
}
