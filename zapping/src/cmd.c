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

#include "cmd.h"
#include "interface.h"
#include "plugins.h"
#include "zmisc.h"
#include "zconf.h"
#include "globals.h"
#include "audio.h"
#include "remote.h"

static PyObject* py_quit (PyObject *self _unused_,
			  PyObject *args _unused_)
{
  GList *p;
  int x, y, w, h;
  gboolean quit_muted;

  if (!zapping)
    py_return_false;

  quit_muted = TRUE;

  /* Error ignored */
  zconf_get_boolean (&quit_muted, "/zapping/options/main/quit_muted");

  if (quit_muted)
    {
      /* Error ignored */
      tv_quiet_set (zapping->info, TRUE);
    }

  /* Save the currently tuned channel */
  zconf_set_int (cur_tuned_channel,
		 "/zapping/options/main/cur_tuned_channel");

  flag_exit_program = TRUE;

  gdk_window_get_origin(GTK_WIDGET (zapping)->window, &x, &y);
  gdk_window_get_geometry(GTK_WIDGET (zapping)->window, NULL, NULL, &w, &h,
			  NULL);
  
  zconf_set_int (x, "/zapping/internal/callbacks/x");
  zconf_set_int (y, "/zapping/internal/callbacks/y");
  zconf_set_int (w, "/zapping/internal/callbacks/w");
  zconf_set_int (h, "/zapping/internal/callbacks/h");

  zconf_set_int ((int) to_old_tveng_capture_mode (zapping->display_mode,
						  zapping->info->capture_mode),
		 "/zapping/options/main/capture_mode");

  zmisc_switch_mode (DISPLAY_MODE_WINDOW,
		     CAPTURE_MODE_NONE,
		     zapping->info);

  /* Tell the widget that the GUI is going to be closed */
  p = g_list_first(plugin_list);
  while (p)
    {
      plugin_remove_gui (&zapping->app, (struct plugin_info *) p->data);
      p = p->next;
    }

  gtk_object_destroy (GTK_OBJECT (zapping));

  gtk_main_quit ();

  py_return_true;
}

static gboolean
switch_mode			(display_mode dmode,
				 capture_mode cmode)
{
  if (0)
    fprintf (stderr, "switch_mode: %d %d\n", dmode, cmode);

  if (-1 == zmisc_switch_mode (dmode, cmode, zapping->info))
    {
      ShowBox(zapping->info->error, GTK_MESSAGE_ERROR);
      return FALSE;
    }

  return TRUE;
}

static PyObject *
py_switch_mode			(PyObject *		self _unused_,
				 PyObject *		args)
{
  display_mode old_dmode;
  capture_mode old_cmode;
  display_mode new_dmode;
  capture_mode new_cmode;
  char *mode_str;

  if (!zapping)
    py_return_false;

  if (!ParseTuple (args, "s", &mode_str))
    g_error ("zapping.switch_mode(s)");

  old_dmode = zapping->display_mode;
  old_cmode = zapping->info->capture_mode;

  new_dmode = old_dmode;
  new_cmode = old_cmode;

  if (0 == g_ascii_strcasecmp (mode_str, "preview"))
    {
      new_cmode = CAPTURE_MODE_OVERLAY;
    }
  else if (0 == g_ascii_strcasecmp (mode_str, "window"))
    {
      new_dmode = DISPLAY_MODE_WINDOW;
    }
  else if (0 == g_ascii_strcasecmp (mode_str, "fullscreen"))
    {
      new_dmode = DISPLAY_MODE_FULLSCREEN;
    }
  else if (0 == g_ascii_strcasecmp (mode_str, "background"))
    {
      new_dmode = DISPLAY_MODE_BACKGROUND;
    }
  else if (0 == g_ascii_strcasecmp (mode_str, "capture"))
    {
      new_cmode = CAPTURE_MODE_READ;
    }
  else if (0 == g_ascii_strcasecmp (mode_str, "teletext"))
    {
      new_cmode = CAPTURE_MODE_TELETEXT;
    }
  else
    {
      /* XXX */
      ShowBox ("Unknown display mode \"%s\", possible choices are:\n"
	       "preview, fullscreen, capture and teletext",
	       GTK_MESSAGE_ERROR, mode_str);
      new_dmode = old_dmode;
      new_cmode = CAPTURE_MODE_NONE;
    }

  if (0)
    fprintf (stderr, "switch: old=%d,%d new=%d,%d last=%d,%d\n",
	     old_dmode, old_cmode,
	     new_dmode, new_cmode,
	     last_dmode, last_cmode);

  if (!switch_mode (new_dmode, new_cmode))
    py_return_false;

  py_return_none;
}

static PyObject *
py_toggle_mode			(PyObject *		self _unused_,
				 PyObject *		args)
{
  capture_mode old_cmode;
  capture_mode new_cmode;
  display_mode old_dmode;
  display_mode new_dmode;
  char *mode_str;

  if (!zapping)
    py_return_false;

  mode_str = NULL;

  if (!ParseTuple (args, "|s", &mode_str))
    g_error ("zapping.toggle_mode(|s)");

  old_dmode = zapping->display_mode;
  old_cmode = zapping->info->capture_mode;

  new_dmode = old_dmode;
  new_cmode = old_cmode;

  if (mode_str)
    {
      if (0 == g_ascii_strcasecmp (mode_str, "preview"))
	{
	  new_cmode = CAPTURE_MODE_OVERLAY;
	}
      else if (0 == g_ascii_strcasecmp (mode_str, "window"))
	{
	  new_dmode = DISPLAY_MODE_WINDOW;
	}
      else if (0 == g_ascii_strcasecmp (mode_str, "fullscreen"))
	{
	  new_dmode = DISPLAY_MODE_FULLSCREEN;
	}
      else if (0 == g_ascii_strcasecmp (mode_str, "background"))
	{
	  new_dmode = DISPLAY_MODE_BACKGROUND;
	}
      else if (0 == g_ascii_strcasecmp (mode_str, "capture"))
	{
	  new_cmode = CAPTURE_MODE_READ;
	}
      else if (0 == g_ascii_strcasecmp (mode_str, "teletext"))
	{
	  new_cmode = CAPTURE_MODE_TELETEXT;
	}
      else
	{
	  /* XXX */
	  ShowBox ("Unknown display mode \"%s\", possible choices are:\n"
		   "preview, fullscreen, capture and teletext",
		   GTK_MESSAGE_ERROR, mode_str);
	  py_return_false;
	}
    }

  if (0)
    fprintf (stderr, "toggle: old=%d,%d new=%d,%d last=%d,%d\n",
	     old_dmode, old_cmode,
	     new_dmode, new_cmode,
	     last_dmode, last_cmode);

  if (new_cmode == old_cmode)
    {
      if (new_cmode == last_cmode)
	py_return_true;

      if (!switch_mode (new_dmode, last_cmode))
	py_return_false;
    }
  else
    {
      if (!switch_mode (new_dmode, new_cmode))
	py_return_false;
    }

  py_return_true;
}

static PyObject* py_about (PyObject *self _unused_, PyObject *args _unused_)
{
  GtkWidget *about = build_widget ("about", NULL);

  /* Do the setting up libglade can't */
  g_object_set (G_OBJECT (about), "name", PACKAGE,
		"version", VERSION, NULL);

  gtk_widget_show (about);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* py_resize_screen (PyObject *self _unused_, PyObject *args)
{
  GdkWindow *subwindow;
  GdkWindow *mw;
  gint sw_w, sw_h, mw_w, mw_h;
  int w, h;
  int ok;

  if (!zapping)
    py_return_false;

  subwindow = GTK_WIDGET (zapping->video)->window;
  mw = gdk_window_get_toplevel(subwindow);
  ok = ParseTuple (args, "ii", &w, &h);

  if (!ok)
    g_error ("zapping.resize_screen(ii)");

  gdk_window_get_geometry (mw, NULL, NULL, &mw_w, &mw_h, NULL);
  gdk_window_get_geometry (subwindow, NULL, NULL, &sw_w, &sw_h, NULL);

  w += (mw_w - sw_w);
  h += (mw_h - sw_h);

  gdk_window_resize(mw, w, h);

  py_return_true;
}


static PyObject *py_help (PyObject *self _unused_, PyObject *args _unused_)
{
  /* XXX handle error, maybe use link_id */
  gnome_help_display ("zapping", NULL, NULL);

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
  cmd_register ("help", py_help, METH_VARARGS,
		("Zapping help"), "zapping.help()"); 
}

void
shutdown_cmd (void)
{
}
