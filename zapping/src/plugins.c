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

/* This shouldn't be public */
void plugin_foreach_free(struct plugin_info * info, void * user_data);

/* Loads a plugin, returns TRUE if the plugin seems usable and FALSE
   in case of error. Shows an error box describing the error in case
   of error and the given structure is filled in on success.
   file_name: name of the plugin to load "/lib/plugin.so", "libm.so", z.b.
   info: Structure that holds all the info about the plugin, and will
         be passed to the rest of functions in this library.
*/

gboolean plugin_load(gchar * file_name, struct plugin_info * info)
{
  gboolean (*plugin_validate_protocol)(gchar * protocol);
  void (*plugin_get_version)(int * zapping_major, int * zapping_minor,
			     int * zapping_micro, int * plugin_major,
			     int * plugin_minor, int * plugin_micro);
  struct ParseStruct * (*plugin_get_parse_struct)();


  /* Open the file resolving all undefined symbols now */
  info -> handle = dlopen(file_name, RTLD_NOW);
  if (!(info -> handle)) /* Failed */
    {
      info -> error = dlerror();
      return FALSE;
    }

  plugin_validate_protocol = dlsym(info -> handle, 
				   "plugin_validate_protocol");

  if ((info -> error = dlerror()) != NULL) /* Error */
    {
      dlclose(info->handle);
      return FALSE;
    }

  /* The plugin should return TRUE in case it supports our protocol */
  if (!(*plugin_validate_protocol)(PLUGIN_PROTOCOL))
    {
      info -> error = _("The plugin doesn't understand our protocol");
      dlclose(info->handle);
      return FALSE;
    }

  /* OK, the protocol is valid, resolve all symbols */
  info -> plugin_get_info = dlsym(info -> handle, "plugin_get_info");
  if ((info -> error = dlerror()) != NULL)
    {
      dlclose(info -> handle);
      return FALSE;
    }

  info -> plugin_get_name = dlsym(info -> handle, "plugin_get_name");
  if ((info -> error = dlerror()) != NULL)
    {
      dlclose(info -> handle);
      return FALSE;
    }

  info -> plugin_get_canonical_name = dlsym(info -> handle, 
					    "plugin_get_canonical_name");
  if ((info -> error = dlerror()) != NULL)
    {
      dlclose(info -> handle);
      return FALSE;
    }

  plugin_get_version = dlsym(info -> handle, "plugin_get_version");
  if ((info -> error = dlerror()) != NULL)
    {
      dlclose(info -> handle);
      return FALSE;
    }

  info -> plugin_running = dlsym(info -> handle, "plugin_running");
  if ((info -> error = dlerror()) != NULL)
    {
      dlclose(info -> handle);
      return FALSE;
    }

  info -> plugin_author = dlsym(info -> handle, "plugin_author");
  if ((info -> error = dlerror()) != NULL)
    {
      dlclose(info -> handle);
      return FALSE;
    }

  info -> plugin_start = dlsym(info -> handle, "plugin_start");
  if ((info -> error = dlerror()) != NULL)
    {
      dlclose(info -> handle);
      return FALSE;
    }

  info -> plugin_stop = dlsym(info -> handle, "plugin_stop");
  if ((info -> error = dlerror()) != NULL)
    {
      dlclose(info -> handle);
      return FALSE;
    }

  plugin_get_parse_struct = dlsym(info -> handle, 
				  "plugin_get_parse_struct");
  if ((info -> error = dlerror()) != NULL)
    {
      dlclose(info -> handle);
      return FALSE;
    }

  info -> plugin_add_properties = dlsym(info -> handle,
					"plugin_add_properties");
  if ((info -> error = dlerror()) != NULL)
    {
      dlclose(info -> handle);
      return FALSE;
    }

  info -> plugin_apply_properties = dlsym(info -> handle, 
					  "plugin_apply_properties");
  if ((info -> error = dlerror()) != NULL)
    {
      dlclose(info -> handle);
      return FALSE;
    }

  info -> plugin_add_gui = dlsym(info -> handle, "plugin_add_gui");
  if ((info -> error = dlerror()) != NULL)
    {
      dlclose(info -> handle);
      return FALSE;
    }

  info -> plugin_init = dlsym(info -> handle, "plugin_init");
  if ((info -> error = dlerror()) != NULL)
    {
      dlclose(info -> handle);
      return FALSE;
    }

  info -> plugin_close = dlsym(info -> handle, "plugin_close");
  if ((info -> error = dlerror()) != NULL)
    {
      dlclose(info -> handle);
      return FALSE;
    }

  info -> plugin_remove_gui = dlsym(info -> handle, "plugin_remove_gui");
  if ((info -> error = dlerror()) != NULL)
    {
      dlclose(info -> handle);
      return FALSE;
    }

  /* Get the plugin version */
  (*plugin_get_version)(&info->zapping_major, & info -> zapping_minor,
			&info->zapping_micro, & info -> major,
			&info->minor, &info -> micro);

  /* if the canonical name is null, then there is an error */
  if (!(*info->plugin_get_canonical_name)())
    {
      info -> error = _("The plugin doesn't provide a canonical name");
      dlclose(info -> handle);
      return FALSE;
    }

  info -> parse_struct = (*plugin_get_parse_struct)();
  if (!(info -> parse_struct))
    {
      info -> error = _("The plugin doesn't provide a parse struct");
      dlclose(info -> handle);
      return FALSE;
    }

  return TRUE;
}

void plugin_unload(struct plugin_info * info)
{
  g_assert(info != NULL);

  dlclose(info -> handle);
}

/* This are wrappers to avoid the use of the pointers in the
   plugin_info struct just in case somewhen the plugin system changes */
/* If the plugin returns NULL, then avoid SEGFAULT */
gchar * plugin_get_name(struct plugin_info * info)
{
  gchar * returned_string = (*info->plugin_get_name)();
  if (returned_string)
    return returned_string;
  else
    return _("(Void name)");
}

/* This function should never return NULL, since plugin_load should
   fail if the canonical name is NULL */
gchar * plugin_get_canonical_name(struct plugin_info * info)
{
  return ((*info -> plugin_get_canonical_name)());
}

/* Gets the long descriptive string for the plugin */
gchar * plugin_get_info(struct plugin_info * info)
{
  gchar * returned_string = (*info->plugin_get_info)();
  if (!returned_string)
    return _("(No description provided for this plugin)");
  else
    return returned_string;
}

/* Inits the plugin for the given device and returns TRUE if the
   plugin coold be inited succesfully */
gboolean plugin_init(tveng_device_info * info, struct plugin_info *
		     plug_info)
{
  return ((*plug_info -> plugin_init)(info));
}

/* Closes the plugin, tells it to close all fds and so on */
void plugin_close(struct plugin_info * info)
{
  (*info->plugin_close)();
}

/* Starts the execution of the plugin, TRUE on success */
gboolean plugin_start(struct plugin_info * info)
{
  return ((*info->plugin_start)());
}

/* stops (pauses) the execution of the plugin */
void plugin_stop(struct plugin_info * info)
{
  (*info->plugin_stop)();
}

/* TRUE if the plugin says it's active */
gboolean plugin_running(struct plugin_info * info)
{
  return ((*info->plugin_running)());
}

/* Add the plugin to the GUI */
void plugin_add_gui(GtkWidget * main_window, struct plugin_info * info)
{
  (*info->plugin_add_gui)(main_window);
}

/* Remove the plugin from the GUI */
void plugin_remove_gui(GtkWidget * main_window, struct plugin_info * info)
{
  (*info->plugin_remove_gui)(main_window);
}

/* Let the plugin add a page to the property box */
void plugin_add_properties(GnomePropertyBox * gpb, struct plugin_info
			   * info)
{
  (*info->plugin_add_properties)(gpb);
}

/* This function is called when apply'ing the changes of the
   property box. The plugin should ignore (returning FALSE) this
   call if n is not the property page it has created for itself */
gboolean plugin_apply_properties(GnomePropertyBox * gpb, int n, struct
				 plugin_info * info)
{
  return ((*info->plugin_apply_properties)(gpb, n));
}

/* The name of the plugin author */
gchar * plugin_author(struct plugin_info * info)
{
  gchar * returned_string = (*info->plugin_author)();
  if (!returned_string)
    return _("(Unknown author)");
  else
    return returned_string;
}

/* Loads all the valid plugins in the given directory, and appends them to
   the given GList. It returns the new GList. The plugins should
   contain exp in their filename (usually called with exp = .zapping.so) */
GList * plugin_load_plugins(gchar * directory, gchar * exp, GList * old)
{
  struct dirent ** namelist;
  int n; /* Number of scanned items */
  int major, minor, micro; /* Zapping version numbers */
  struct plugin_info plug;
  struct plugin_info * new_plugin; /* If plugin is OK, a copy will be
				      allocated here */
  int i;
  gchar * filename; /* Complete path to the plugin to load */
  int zapping_version, plugin_version; /* Numeric version values */

  g_assert(exp != NULL);
  g_assert(directory != NULL);
  g_assert(strlen(directory) > 0);
  
  major = minor = micro = 0; /* In case we cannot sscanf */

  /* Get current zapping version */
  sscanf(VERSION, "%d.%d.%d", &major, &minor, &micro);

  zapping_version = major * 256 * 256 + minor * 256 + micro;

  n = scandir(directory, &namelist, 0, alphasort);
  if (n < 0)
    {
      perror("scandir");
      return old;
    }

  for (i = 0; i < n; i++)
    {
      if (!strstr(namelist[i] -> d_name, exp))
	continue;
      
      filename = g_strconcat(directory, directory[strlen(directory)-1] ==
			     '/' ? "" : "/", namelist[i] -> d_name, NULL);

      if (!plugin_load(filename, &plug))
	{
	  g_free(filename);
	  continue;
	}

      g_free(filename);

      /* Check that zapping is mature enough for this plugin */
      plugin_version = plug.zapping_major * 256 * 256 +
	plug.zapping_minor * 256 + plug.zapping_micro;

      if (zapping_version < plugin_version)
	{
	  plugin_unload(&plug);
	  continue;
	}

      /* This plugin is valid, copy it and add it to the GList */
      new_plugin = (struct plugin_info *) malloc(sizeof(struct plugin_info));
      if (!new_plugin)
	{
	  perror("malloc");
	  plugin_unload(&plug);
	  continue;
	}

      memcpy(new_plugin, &plug, sizeof(struct plugin_info));
      
      old = g_list_append(old, new_plugin);
    }

  return old;
}

/* This function is called from g_list_foreach */
void plugin_foreach_free(struct plugin_info * info, void * user_data)
{
  plugin_unload(info);
  free(info);
}

/* Unloads all the plugins loaded in the GList */
void plugin_unload_plugins(GList * list)
{
  g_list_foreach(list, (GFunc) plugin_foreach_free, NULL);
  g_list_free(list);
}
