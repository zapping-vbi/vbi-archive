#ifndef __REMOTE_H__
#define __REMOTE_H__

/**
 * This provides a Python interface to the internal Zapping routines.
 */

#ifdef _POSIX_C_SOURCE
  /* python 2.3 redefines. ugh. */
#  undef _POSIX_C_SOURCE
#  include <Python.h>
#  ifndef _POSIX_C_SOURCE
#    define _POSIX_C_SOURCE 199506L
#  endif
#else
#  include <Python.h>
#endif

#include <gtk/gtk.h>

/* The zapping dictionary in case you want to add things
   manually. To register functions use cmd_register() instead. */
extern PyObject *	dict;

extern void
on_python_command1		(GtkWidget *		widget,
				 const gchar *		cmd);
extern void
on_python_command2		(GtkWidget *		widget,
				 gpointer 		unused,
				 const gchar *		cmd);
extern void
on_python_command3		(GtkWidget *		widget,
				 gpointer 		unused1,
				 gpointer 		unused2,
				 const gchar *		cmd);

#define python_command(widget, cmd) on_python_command1 (widget, cmd)

extern void
python_command_printf		(GtkWidget *		widget,
				 const gchar *		fmt,
				 ...);
extern GtkWidget *
python_command_widget		(void);

extern GList *
cmd_list			(void);
extern const gchar *
cmd_action_from_cmd		(const gchar *		cmd);
extern const gchar *
cmd_action_to_cmd		(const gchar *		action);
extern GtkMenu *
cmd_action_menu			(void);
extern gchar *
cmd_compatibility		(const gchar *		cmd);
extern void
_cmd_register			(const gchar *		name,
				 PyCFunction		cfunc,
				 int			flags,
				 ...);
#define cmd_register(name, cfunc, flags, args...)			\
  _cmd_register (name, cfunc, flags ,##args , 0)
extern void
shutdown_remote			(void);
extern void
startup_remote			(void);

/* The following macros simplify writing the python wrappers. */
#define py_return_none							\
do {									\
  Py_INCREF(Py_None);							\
  return Py_None;							\
} while (0)

#define py_return_true return PyInt_FromLong (TRUE)
#define py_return_false return PyInt_FromLong (FALSE)

#endif /* __REMOTE_H__ */
