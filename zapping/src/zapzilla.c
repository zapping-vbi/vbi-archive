/* Standalone Zapzilla binary
 * Copyright (C) 2001 Iñaki García Etxebarria
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

#ifdef HAVE_LIBZVBI

#include <gnome.h>
#include <glade/glade.h>
#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zconf.h"
#include "zmisc.h"
#include "interface.h"
#include "zvbi.h"
#include "ttxview.h"
#include "remote.h"
#include "keyboard.h"
#include "globals.h"

static void shutdown_zapzilla(void);
static gboolean startup_zapzilla(void);

extern volatile gboolean	flag_exit_program;

static PyObject*
py_quit (PyObject *self, PyObject *args)
{
  flag_exit_program = TRUE;
  gtk_main_quit();

  py_return_true;
}

static void
on_ttxview_zmodel_changed	(ZModel *		zmodel,
				 gpointer		data)
{
  if (NUM_TTXVIEWS (zmodel) == 0)
    {
      flag_exit_program = TRUE;
      gtk_main_quit ();
    }
}

extern int zapzilla_main(int argc, char * argv[]);

int zapzilla_main(int argc, char * argv[])
{
  const char *vbi_device = NULL;
  const struct poptOption options[] = {
    {
      "debug",
      'd',
      POPT_ARG_NONE,
      &debug_msg,
      0,
      N_("Set debug messages on"),
      NULL
    },
    {
      "device",
      0,
      POPT_ARG_STRING,
      &vbi_device,
      0,
      N_("VBI device to use"),
      N_("DEVICE")
    },
    {
      NULL,
    } /* end the list */
  };

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  gnome_program_init ("zapzilla", VERSION, LIBGNOMEUI_MODULE,
		      argc, argv,
		      GNOME_PARAM_APP_DATADIR, PACKAGE_DATA_DIR,
		      GNOME_PARAM_POPT_TABLE, options,
		      NULL);

  printv("%s\n%s %s, build date: %s\n",
	 "$Id: zapzilla.c,v 1.4.2.5 2003-09-24 18:41:25 mschimek Exp $",
	 "Zapzilla", VERSION, __DATE__);
  D();
  /* FIXME: Find something better */
  gnome_window_icon_set_default_from_file
    (PACKAGE_PIXMAPS_DIR "/gnome-television.png");
  D();
  if (!startup_zapzilla())
    {
      RunBox(_("Zapzilla couldn't be started"), GTK_MESSAGE_ERROR);
      return 0;
    }
  D();
  startup_zvbi();
  if (!vbi_device)
    vbi_device = zconf_get_string(NULL,
				  "/zapping/options/vbi/vbi_device");
  if (!zvbi_open_device(vbi_device))
    {
      /* zvbi_open_device reports error 
      RunBox(_("Couldn't open %s, exitting"), GTK_MESSAGE_ERROR,
	     vbi_device);
      */
      return 0;
    }
  D();
  startup_ttxview();
  D();
  g_signal_connect(G_OBJECT(ttxview_zmodel), "changed",
		   G_CALLBACK(on_ttxview_zmodel_changed),
		   NULL);
  D();
  gtk_widget_show (ttxview_new ());
  D();
  startup_keyboard();
  D();
  gtk_main();
  /* Closes all fd's, writes the config to HD, and that kind of things
   */
  shutdown_zapzilla();
  return 0;
}

static void shutdown_zapzilla(void)
{
  printv("Shutting down the beast:\n");

  printv(" kbd");
  shutdown_keyboard();

  /*
   * Shuts down the teletext view
   */
  printv(" ttxview");
  shutdown_ttxview();

  /* Shut down vbi */
  printv(" vbi");
  shutdown_zvbi();

  if (!zconf_close())
    RunBox(_("ZConf could not be closed properly , your\n"
	     "configuration will be lost.\n"
	     "Possible causes for this are:\n"
	     "   - There is not enough free memory\n"
	     "   - You do not have permissions to write to $HOME/.zapping\n"
	     "   - libxml is non-functional (?)\n"
	     "   - or, more probably, you have found a bug in\n"
	     "     %s. Please contact the author.\n"
	     ), GTK_MESSAGE_ERROR, "Zapzilla");

  printv(" cmd");
  shutdown_remote();

  printv(".\nShutdown complete, goodbye.\n");
}

static gboolean startup_zapzilla()
{
  startup_remote ();

  cmd_register ("quit", py_quit, METH_VARARGS,
		("Quit"), "zapping.quit()");
  D();

  /* Starts the configuration engine */
  if (!zconf_init("zapping"))
    {
      g_error(_("Sorry, Zapzilla is unable to create the config tree"));
      return FALSE;
    }
  D();
  return TRUE;
}

#endif /* HAVE_LIBZVBI */
