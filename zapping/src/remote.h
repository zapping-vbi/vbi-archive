#ifndef __REMOTE_H__
#define __REMOTE_H__

/**
 * This provides a Python interface to the internal Zapping routines.
 */

#include <Python.h>
#include <glib.h>

/* the zapping dictionary in case you want to add things
   manually. But in case you are registering functions use
   cmd_register instead */
extern PyObject		*dict;

/* startup/shutdown */
void startup_remote (void);
void shutdown_remote (void);

/* register a python varargs routine in the zapping class */
void cmd_register (const char *name, PyCFunction func,
		   int flags,
		   const char *doc, const char *usage);

/* runs the given python code */
void cmd_run (const char *command);

/* runs the given python code, but now the command can be specified in
   a printf() fashion */
void cmd_run_printf (const char *cmd, ...);

/* Returns a list of all registered commands */
GList *cmd_list (void);

/* callbacks glue */
void
on_remote_command1 (void *, const char *cmd);
void
on_remote_command2 (void *, void *, const char *cmd);
/* Returns the last widget that invoqued cmd_run through
   on_remote_command[12] */
gpointer remote_last_caller (void);

/* The following macros simplify writing the python wrappers */
#define py_return_none do { Py_INCREF(Py_None); return Py_None; } while (0)
#define py_return_true return PyInt_FromLong (TRUE)
#define py_return_false return PyInt_FromLong (FALSE)

gchar *
cmd_compatibility		(const gchar *		cmd);

#endif /* remote.h */
