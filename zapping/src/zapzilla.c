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
#include <libgnomeui/gnome-window-icon.h> /* only gnome 1.2 and above */
#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zconf.h"
#include "zmisc.h"
#include "interface.h"
#include "zvbi.h"
#include "ttxview.h"

static void shutdown_zapzilla(void);
static gboolean startup_zapzilla(void);

extern volatile gboolean	flag_exit_program;
extern gint			console_errors;

static void
on_ttxview_model_changed		(ZModel		*model,
					 gpointer	data)
{
  if (!GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(model))))
    {
      flag_exit_program = TRUE;
      gtk_main_quit();
    }
}

extern int zapzilla_main(int argc, char * argv[]);

int zapzilla_main(int argc, char * argv[])
{
  char *vbi_device = NULL;
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
      "console-errors",
      0,
      POPT_ARG_NONE,
      &console_errors,
      0,
      N_("Redirect the error messages to the console"),
      NULL
    },
    {
      NULL,
    } /* end the list */
  };

#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
  textdomain (PACKAGE);
#endif

  /* Init gnome, libglade, modules and tveng */
  gnome_init_with_popt_table ("zapzilla", VERSION, argc, argv, options,
			      0, NULL);

  printv("%s\n%s %s, build date: %s\n",
	 "$Id: zapzilla.c,v 1.2 2002-01-13 09:52:22 mschimek Exp $",
	 "Zapzilla", VERSION, __DATE__);
  glade_gnome_init();
  D();
  /* FIXME: Find something better */
  gnome_window_icon_set_default_from_file(PACKAGE_PIXMAPS_DIR "/gnome-television.png");
  D();
  if (!startup_zapzilla())
    {
      RunBox(_("Zapzilla couldn't be started"), GNOME_MESSAGE_BOX_ERROR);
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
      RunBox(_("Couldn't open %s, exitting"), GNOME_MESSAGE_BOX_ERROR,
	     vbi_device);
      */
      return 0;
    }
  D();
  startup_ttxview();
  D();
  gtk_signal_connect(GTK_OBJECT(ttxview_model), "changed",
		     GTK_SIGNAL_FUNC(on_ttxview_model_changed),
		     NULL);
  D();
  gtk_widget_show(build_ttxview());
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
	     ), GNOME_MESSAGE_BOX_ERROR, "Zapzilla");

  printv(".\nShutdown complete, goodbye.\n");
}

static gboolean startup_zapzilla()
{
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
