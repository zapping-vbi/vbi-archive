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

static PyObject* py_quit (PyObject *self, PyObject *args)
{
  GList *p;
  int x, y, w, h;

  if (!main_window)
    py_return_false;

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
resolve_mode		(const gchar *_mode)
{
  if (!strcasecmp (_mode, "preview"))
    return TVENG_CAPTURE_WINDOW;
  else if (!strcasecmp (_mode, "fullscreen"))
    return TVENG_CAPTURE_PREVIEW;
  else if (!strcasecmp (_mode, "capture"))
    return TVENG_CAPTURE_READ;
  else if (!strcasecmp (_mode, "teletext"))
    return TVENG_NO_CAPTURE;
  else
    ShowBox (_("Unknown mode <%s>, possible choices are:\n"
	       "preview, fullscreen, capture and teletext"),
	     GTK_MESSAGE_ERROR, _mode);

  return -1;
}

static gboolean
switch_mode				(enum tveng_capture_mode mode)
{
  switch (mode)
    {
    case TVENG_CAPTURE_PREVIEW:
      zmisc_switch_mode (TVENG_CAPTURE_PREVIEW, main_info);
      break;

    case TVENG_CAPTURE_WINDOW:
      if (zmisc_switch_mode (TVENG_CAPTURE_WINDOW, main_info) == -1)
	ShowBox(_("%s:\n"
		  "Try running as root \"zapping_fix_overlay\" in a console"),
		GTK_MESSAGE_ERROR, main_info->error);
      break;

    case TVENG_CAPTURE_READ:
      if (zmisc_switch_mode(TVENG_CAPTURE_READ, main_info) == -1)
	ShowBox(main_info->error, GTK_MESSAGE_ERROR);
      break;

#ifdef HAVE_LIBZVBI
    case TVENG_NO_CAPTURE:
      /* Switch from TTX to Subtitles overlay, and vice versa */
#if 0 /* needs some other solution */
      if (main_info->current_mode == TVENG_NO_CAPTURE)
	{
	  /* implicit switch to previous_mode */
	  if (get_ttxview_page(main_window, &zvbi_page, NULL))
	    zmisc_overlay_subtitles(zvbi_page);
	}
      else
#endif
	zmisc_switch_mode(TVENG_NO_CAPTURE, main_info);

      break;
#endif

    default:
      return FALSE;
    }

  return TRUE;
}

static PyObject* py_switch_mode (PyObject *self, PyObject *args)
{
  char *_mode;
  int ok = PyArg_ParseTuple (args, "s", &_mode);
  enum tveng_capture_mode mode;

  if (!ok)
    g_error ("zapping.switch_mode(s)");

  mode = main_info->current_mode;

  if (!switch_mode (resolve_mode (_mode)))
    py_return_false;

  if (mode != main_info->current_mode)
    last_mode = mode;

  py_return_none;
}

static PyObject* py_toggle_mode (PyObject *self, PyObject *args)
{
  enum tveng_capture_mode mode, curr_mode = main_info->current_mode;
  char *_mode = NULL;
  int ok = PyArg_ParseTuple (args, "|s", &_mode);

  if (!ok)
    g_error ("zapping.toggle_mode(|s)");

  if (_mode)
    {
      mode = resolve_mode (_mode);
      if (mode < 0)
	py_return_false;
    }
  else
    mode = curr_mode;

  if (curr_mode == mode && mode != TVENG_NO_CAPTURE)
    {
      /* swap requested (current) mode and last mode */

      if (!switch_mode (last_mode))
	py_return_false;

      last_mode = mode;
    }
  else
    {
      /* switch to requested mode */

      if (!switch_mode (mode))
	py_return_false;

      last_mode = curr_mode;
    }

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
  GdkWindow *subwindow = lookup_widget (main_window, "tv_screen")->window;
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

static PyObject* py_hide_controls (PyObject *self, PyObject *args)
{
  int hide = 2; /* toggle */
  int ok = PyArg_ParseTuple (args, "|i", &hide);
  GtkWidget *toolbar, *menu, *menuitem;

  if (!ok)
    g_error ("zapping.hide_controls(|i)");

  if (hide > 1)
    hide = !zconf_get_boolean (NULL,
			       "/zapping/internal/callbacks/hide_controls");

  menu = lookup_widget (main_window, "bonobodockitem1");
  toolbar = lookup_widget (main_window, "bonobodockitem2");
  menuitem = lookup_widget (main_window, "hide_controls2"); 
  if (hide)
    {
      gtk_widget_hide (menu);
      gtk_widget_hide (toolbar);
      z_change_menuitem(menuitem,
			"gnome-stock-book-open",
			_("Show controls"),
			_("Show the menu and the toolbar"));
    }
  else
    {
      gtk_widget_show (menu);
      gtk_widget_show (toolbar);
      z_change_menuitem(menuitem,
			"gnome-stock-book-yellow",
			_("Hide controls"),
			_("Hide the menu and the toolbar"));
    }

  gtk_widget_queue_resize (main_window);
  zconf_set_boolean (hide,
		     "/zapping/internal/callbacks/hide_controls");

  py_return_none;
}

void
startup_cmd (void)
{
  cmd_register ("quit", py_quit, METH_VARARGS,
		_("Quits the program"), "zapping.quit()");
  cmd_register ("switch_mode", py_switch_mode, METH_VARARGS,
		_("Switches Zapping to the "
		  "given mode"), "zapping.switch_mode('preview')");
  /* FIXME: This isn't the place for this, create a mode.c containing
     the mode switching logic, it's getting a bit too  complex */
  cmd_register ("toggle_mode", py_toggle_mode, METH_VARARGS,
		_("Toggles between the previous and the given "
		  "(or current by default) mode"),
		"zapping.toggle_mode('fullscreen')");
  /* Compatibility (FIXME: Does it really make sense to keep this?) */
  cmd_register ("restore_mode", py_toggle_mode, METH_VARARGS,
		_("Toggles between the previous and the given "
		  "(or current by default) mode"),
		"zapping.restore_mode('fullscreen')");
  cmd_register ("about", py_about, METH_VARARGS,
		_("Shows the About box"), "zapping.about()");
  cmd_register ("resize_screen", py_resize_screen, METH_VARARGS,
		_("Resizes the screen to the given dimensions"),
		"zapping.resize_screen(640, 480)");
  cmd_register ("hide_controls", py_hide_controls, METH_VARARGS,
		_("Hides the menu and the toolbar"),
		"zapping.hide_controls([1])");
}

void
shutdown_cmd (void)
{
}
