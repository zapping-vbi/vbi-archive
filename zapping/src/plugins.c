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
  This library is in charge of managing the plugins, and providing
  them with a consistent API for communicating with the program
  The plugins will be shared libraries open with dlopen, and with some
  functions to make them appear as any other component
*/

#include "plugins.h"

/* Loads a plugin, returns TRUE if the plugin seems usable and FALSE
   in case of error. Shows an error box describing the error in case
   of error and the given structure is filled in on success.
   file_name: name of the plugin to load "/lib/plugin.so", "libm.so", z.b.
   info: Structure that holds all the info about the plugin, and will
         be passed to the rest of functions in this library.
*/
gboolean plugin_load(gchar * file_name, struct plugin_info * info)
{
  info -> error = NULL;

  /* Open the file resolving all undefined symbols now */
  info -> handle = dlopen(file_name, RTLD_NOW);
  if (!(info -> handle)) /* Failed */
    {
      info -> error = dlerror();
      return FALSE;
    }

  dlclose(info -> handle); /* Do nothing for the moment */
  return TRUE;
}
