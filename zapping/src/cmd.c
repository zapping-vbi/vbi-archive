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

/**
 * Provides the functionality in the Python interface of Zapping.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "remote.h"
#include "cmd.h"
#include "interface.h"
#include "plugins.h"
#include "zmisc.h"
#include "zconf.h"
#include "globals.h"
#include "audio.h"

static PyObject* py_quit (PyObject *self, PyObject *args)
{
  GList *p;
  int x, y, w, h;

  if (!main_window)
    py_return_false;

  if (zconf_get_boolean (NULL, "/zapping/options/audio/quit_muted"))
    {
      /* Error ignored */
      tv_quiet_set (main_info, TRUE);
    }

  /* Save the currently tuned channel */
  zconf_set_integer (cur_tuned_channel,
		     "/zapping/options/main/cur_tuned_channel");

  flag_exit_program = TRUE;

  gdk_window_get_origin(main_window->window, &x, &y);
  gdk_window_get_geometry(main_window->window, NULL, NULL, &w, &h,
			  NULL);
  
  zconf_set_integer(x, "/zapping/internal/callbacks/x");
  zconf_set_integer(y, "/zapping/internal/callbacks/y");
  zconf_set_integer(w, "/zapping/internal/callbacks/w");
  zconf_set_integer(h, "/zapping/internal/callbacks/h");

  zconf_set_integer(main_info->current_mode,
		    "/zapping/options/main/capture_mode");

  zmisc_switch_mode(TVENG_NO_CAPTURE, main_info);

  /* Tell the widget that the GUI is going to be closed */
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_remove_gui (GNOME_APP (main_window), 
			 (struct plugin_info *) p->data);
      p = p->next;
    }

  gtk_main_quit ();

  py_return_true;
}

static enum tveng_capture_mode
resolve_mode			(const gchar *		mode_str)
{
  if (0 == strcasecmp (mode_str, "preview"))
    return TVENG_CAPTURE_WINDOW;
  else if (0 == strcasecmp (mode_str, "fullscreen"))
    return TVENG_CAPTURE_PREVIEW;
  else if (0 == strcasecmp (mode_str, "capture"))
    return TVENG_CAPTURE_READ;
  else if (0 == strcasecmp (mode_str, "teletext"))
    return TVENG_TELETEXT;
  else
    ShowBox (_("Unknown display mode '%s', possible choices are:\n"
	       "preview, fullscreen, capture and teletext"),
	     GTK_MESSAGE_ERROR, mode_str);

  return TVENG_NO_CAPTURE;
}

static gboolean
switch_mode			(enum tveng_capture_mode mode)
{
  switch (mode)
    {
    case TVENG_CAPTURE_PREVIEW:
      if (-1 == zmisc_switch_mode (TVENG_CAPTURE_PREVIEW, main_info))
	return FALSE;
      break;

    case TVENG_CAPTURE_WINDOW:
      if (-1 == zmisc_switch_mode (TVENG_CAPTURE_WINDOW, main_info))
	{
	  ShowBox(main_info->error, GTK_MESSAGE_ERROR);
	  return FALSE;
	}
      break;

    case TVENG_CAPTURE_READ:
      if (-1 == zmisc_switch_mode(TVENG_CAPTURE_READ, main_info))
	{
	  ShowBox(main_info->error, GTK_MESSAGE_ERROR);
	  return FALSE;
	}
      break;

    case TVENG_TELETEXT:
      if (-1 == zmisc_switch_mode(TVENG_TELETEXT, main_info))
	return FALSE;
      break;

    case TVENG_NO_CAPTURE:
      if (-1 == zmisc_switch_mode(TVENG_NO_CAPTURE, main_info))
	return FALSE;
      break;

    default:
      return FALSE;
    }

  return TRUE;
}

static PyObject *
py_switch_mode			(PyObject *		self,
				 PyObject *		args)
{
  enum tveng_capture_mode old_mode;
  char *mode_str;

  if (!PyArg_ParseTuple (args, "s", &mode_str))
    g_error ("zapping.switch_mode(s)");

  old_mode = main_info->current_mode;

  if (!switch_mode (resolve_mode (mode_str)))
    py_return_false;

  if (main_info->current_mode != old_mode)
    last_mode = old_mode;

  py_return_none;
}

static PyObject *
py_toggle_mode			(PyObject *		self,
				 PyObject *		args)
{
  enum tveng_capture_mode mode;
  char *mode_str;

  mode_str = NULL;

  if (!PyArg_ParseTuple (args, "|s", &mode_str))
    g_error ("zapping.toggle_mode(|s)");

  if (mode_str)
    mode = resolve_mode (mode_str);
  else
    mode = main_info->current_mode;

  if (!switch_mode (last_mode))
    py_return_false;

  last_mode = mode;

  py_return_true;
}

static PyObject* py_about (PyObject *self, PyObject *args)
{
  GtkWidget *about = build_widget ("about", NULL);

  /* Do the setting up libglade can't */
  g_object_set (G_OBJECT (about), "name", PACKAGE,
		"version", VERSION, NULL);

  gtk_widget_show (about);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* py_resize_screen (PyObject *self, PyObject *args)
{
  GdkWindow *subwindow = lookup_widget (main_window, "tv-screen")->window;
  GdkWindow *mw = gdk_window_get_toplevel(subwindow);
  gint sw_w, sw_h, mw_w, mw_h;
  int w, h;
  int ok = PyArg_ParseTuple (args, "ii", &w, &h);

  if (!ok)
    g_error ("zapping.resize_screen(ii)");

  gdk_window_get_geometry (mw, NULL, NULL, &mw_w, &mw_h, NULL);
  gdk_window_get_geometry (subwindow, NULL, NULL, &sw_w, &sw_h, NULL);

  w += (mw_w - sw_w);
  h += (mw_h - sw_h);

  gdk_window_resize(mw, w, h);

  py_return_true;
}

static PyObject *
py_hide_controls		(PyObject *		self,
				 PyObject *		args)
{
  int hide;
  int ok;

  hide = 2; /* toggle */
  ok = PyArg_ParseTuple (args, "|i", &hide);

  if (!ok)
    g_error ("zapping.hide_controls(|i)");

  zconf_set_boolean (hide, "/zapping/internal/callbacks/hide_controls");

  py_return_none;
}

static PyObject *
py_keep_on_top			(PyObject *		self,
				 PyObject *		args)
{
  extern gboolean have_wm_hints;
  int keep;
  int ok;

  keep = 2; /* toggle */
  ok = PyArg_ParseTuple (args, "|i", &keep);

  if (!ok)
    g_error ("zapping.keep_on_top(|i)");

  if (have_wm_hints)
    {
      if (keep > 1)
        keep = !zconf_get_boolean (NULL, "/zapping/options/main/keep_on_top");
      zconf_set_boolean (keep, "/zapping/options/main/keep_on_top");

      window_on_top (GTK_WINDOW (main_window), keep);
    }

  py_return_none;
}

static PyObject *py_help (PyObject *self, PyObject *args)
{
#warning
  /* See gnome-docu what's appropriate in Gnome-2.
     gnome_help_display ("index.html", NULL, NULL); */

  py_return_none;
}

void
startup_cmd (void)
{
  cmd_register ("quit", py_quit, METH_VARARGS,
		("Quit"), "zapping.quit()");
  cmd_register ("switch_mode", py_switch_mode, METH_VARARGS,
		("Switch to Fullscreen mode"), "zapping.switch_mode('fullscreen')",
		("Switch to Capture mode"), "zapping.switch_mode('capture')",
		("Switch to Overlay mode"), "zapping.switch_mode('preview')",
		("Switch to Teletext mode"), "zapping.switch_mode('teletext')");
  /* FIXME: This isn't the place for this, create a mode.c containing
     the mode switching logic, it's getting a bit too  complex */
  cmd_register ("toggle_mode", py_toggle_mode, METH_VARARGS,
		("Switch to Fullscreen mode or previous mode"), "zapping.toggle_mode('fullscreen')",
		("Switch to Capture mode or previous mode"), "zapping.toggle_mode('capture')",
		("Switch to Overlay mode or previous mode"), "zapping.toggle_mode('preview')",
		("Switch to Teletext mode or previous mode"), "zapping.toggle_mode('teletext')");
  /* Compatibility (FIXME: Does it really make sense to keep this?) */
  cmd_register ("restore_mode", py_toggle_mode, METH_VARARGS);
  cmd_register ("about", py_about, METH_VARARGS,
		("About Zapping"), "zapping.about()");
  cmd_register ("resize_screen", py_resize_screen, METH_VARARGS);
  cmd_register ("hide_controls", py_hide_controls, METH_VARARGS,
		("Show/hide menu and toolbar"), "zapping.hide_controls()");
  cmd_register ("help", py_help, METH_VARARGS,
		("Zapping help"), "zapping.help()"); 
  cmd_register ("keep_on_top", py_keep_on_top, METH_VARARGS,
		("Keep window on top"), "zapping.keep_on_top()");
}

void
shutdown_cmd (void)
{
}
