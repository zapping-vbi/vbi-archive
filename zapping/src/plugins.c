/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
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

#include "plugins.h"
#include "properties.h"

/* This is the main plugin list, it is only used in plugin_bridge */
extern GList * plugin_list;

/* This shouldn't be public */
static void plugin_foreach_free(struct plugin_info * info, void *
				user_data);

/* Loads a plugin, returns TRUE if the plugin seems usable and FALSE
   in case of error. Shows an error box describing the error in case
   of error and the given structure is filled in on success.
   file_name: name of the plugin to load "/lib/plugin.so", "libm.so", z.b.
   info: Structure that holds all the info about the plugin, and will
         be passed to the rest of functions in this library.
*/
static gboolean plugin_load(gchar * file_name, struct plugin_info * info)
{
  /* This variables are for querying the symbols */
  gint i = 0;
  gchar *symbol, *type, *description;
  gpointer ptr;
  gint hash;
  /* This is used to get the canonical name */
  gchar * canonical_name;
  gchar * version;
  gint (*plugin_get_protocol)(void) = NULL;
  gboolean (*plugin_get_symbol)(gchar * name, gint hash,
				gpointer * ptr);

  g_assert(info != NULL);
  g_assert(file_name != NULL);

  memset (info, 0, sizeof(struct plugin_info));

  info -> handle = g_module_open (file_name, 0);

  if (!info -> handle)
    {
      g_warning("Failed to load plugin: %s", g_module_error());
      return FALSE;
    }

  /* Check that the protocols we speak are the same */
  if (!g_module_symbol(info -> handle, "plugin_get_protocol",
		       (gpointer*)&(plugin_get_protocol)))
    {
      g_module_close(info->handle);
      return FALSE;
    }

  if (((*plugin_get_protocol)()) != PLUGIN_PROTOCOL)
    {
      g_warning("While loading %s\n"
		"The plugin uses the protocol %d, and the current"
		" one is %d, it cannot be loaded.",
		file_name,
		(*plugin_get_protocol)(), PLUGIN_PROTOCOL);
      g_module_close(info->handle);
      return FALSE;
    }

  /* Get the other symbol */
  if (!g_module_symbol(info->handle, "plugin_get_symbol",
		       (gpointer*) &(plugin_get_symbol) ))
    {
      g_warning(g_module_error());
      g_module_close(info->handle);
      return FALSE;
    }

  /* plugin_get_info is the only compulsory symbol that must be
     present in the plugin's table of symbols */
  if (!(*plugin_get_symbol)("plugin_get_info", 0x1234,
			    (gpointer*)&(info->plugin_get_info)))
    {
      g_warning("plugin_get_info was not found in %s",
		file_name);
      g_module_close(info->handle);
      return FALSE;
    }

  /* Get the remaining symbols */
  if (!(*plugin_get_symbol)("plugin_init", 0x1234,
		       (gpointer*)&(info->plugin_init)))
    info->plugin_init = NULL;

  if (!(*plugin_get_symbol)("plugin_close", 0x1234,
		       (gpointer*)&(info->plugin_close)))
    info->plugin_close = NULL;

  if (!(*plugin_get_symbol)("plugin_start", 0x1234,
		       (gpointer*)&(info->plugin_start)))
    info->plugin_start = NULL;

  if (!(*plugin_get_symbol)("plugin_stop", 0x1234,
		       (gpointer*)&(info->plugin_stop)))
    info->plugin_stop = NULL;

  if (!(*plugin_get_symbol)("plugin_load_config", 0x1234,
		       (gpointer*)&(info->plugin_load_config)))
    info->plugin_load_config = NULL;

  if (!(*plugin_get_symbol)("plugin_save_config", 0x1234,
		       (gpointer*)&(info->plugin_save_config)))
    info->plugin_save_config = NULL;

  if (!(*plugin_get_symbol)("plugin_running", 0x1234,
		       (gpointer*)&(info->plugin_running)))
    info->plugin_running = NULL;

  if (!(*plugin_get_symbol)("plugin_write_bundle", 0x1234,
		       (gpointer*)&(info->plugin_write_bundle)))
    info->plugin_write_bundle = NULL;

  if (!(*plugin_get_symbol)("plugin_read_bundle", 0x1234,
		       (gpointer*)&(info->plugin_read_bundle)))
    info->plugin_read_bundle = NULL;

  if (!(*plugin_get_symbol)("plugin_capture_stop", 0x1234,
		       (gpointer*)&(info->plugin_capture_stop)))
    info->plugin_capture_stop = NULL;

  if (!(*plugin_get_symbol)("plugin_get_public_info", 0x1234,
		       (gpointer*)&(info->plugin_get_public_info)))
    info->plugin_get_public_info = NULL;

  if (!(*plugin_get_symbol)("plugin_add_properties", 0x1234,
		       (gpointer*)&(info->plugin_add_properties)))
    info->plugin_add_properties = NULL;

  if (!(*plugin_get_symbol)("plugin_activate_properties", 0x1234,
		       (gpointer*)&(info->plugin_activate_properties)))
    info->plugin_activate_properties = NULL;

  if (!(*plugin_get_symbol)("plugin_help_properties", 0x1234,
		       (gpointer*)&(info->plugin_help_properties)))
    info->plugin_help_properties = NULL;

  /* Check that these three functions are present */
   if ((!info -> plugin_add_properties) ||
       (!info -> plugin_activate_properties) ||
       (!info -> plugin_help_properties))
     info -> plugin_add_properties =
       (gpointer) info -> plugin_activate_properties
       = (gpointer) info -> plugin_help_properties = NULL;

  if (!(*plugin_get_symbol)("plugin_add_gui", 0x1234,
		       (gpointer*)&(info->plugin_add_gui)))
    info->plugin_add_gui = NULL;

  if (!(*plugin_get_symbol)("plugin_remove_gui", 0x1234,
		       (gpointer*)&(info->plugin_remove_gui)))
    info->plugin_remove_gui = NULL;

  /* Check that the two functions are present */
  if ((!info->plugin_add_gui) ||
      (!info->plugin_remove_gui))
    info -> plugin_add_gui = info -> plugin_remove_gui = NULL;

  if (!(*plugin_get_symbol)("plugin_process_popup_menu", 0x1234,
	    (gpointer*)&(info->plugin_process_popup_menu)))
    info->plugin_process_popup_menu = NULL;

  if (!(*plugin_get_symbol)("plugin_get_misc_info", 0x1234,
	    (gpointer*)&(info->plugin_get_misc_info)))
    info->plugin_get_misc_info = NULL;

  memset(&(info->misc_info), 0, sizeof(struct plugin_misc_info));

  if (info -> plugin_get_misc_info)
    memcpy(&(info->misc_info), (*info->plugin_get_misc_info)(),
	   ((*info->plugin_get_misc_info)())->size);

  plugin_get_info(&canonical_name, NULL, NULL, NULL, NULL, NULL,
		   info);
  if ((!canonical_name) || (strlen(canonical_name) == 0))
    {
      g_module_close(info->handle);
      g_warning("\"%s\" seems to be a valid plugin, but it doesn't "
		"provide a canonical name.", file_name);
      return FALSE;
    }
  /* Get the version of the plugin */
  version = plugin_get_version(info);
  if (!version)
    {
      g_module_close(info->handle);
      g_warning(_("\"%s\" doesn't provide a version"),
		file_name);
      return FALSE;
    }

  info -> major = info->minor = info->micro = 0;
  if (sscanf(version, "%d.%d.%d", &(info->major), &(info->minor),
      &(info->micro)) == 0)
    {
      g_warning(
	"Sorry, the version of the plugin cannot be parsed.\n"
	"The version must be something like %%d[.%%d[%%d[other things]]]\n"
	"The given version is %s.\nError loading \"%s\" (%s)",
      version, file_name, canonical_name);
      g_module_close(info -> handle);
      return FALSE;
    }

  info -> canonical_name = g_strdup(canonical_name);
  info -> file_name = g_strdup(file_name);

  /* Get the exported symbols for the plugin */
  info -> exported_symbols = NULL;
  info -> num_exported_symbols = 0;
  if (info->plugin_get_public_info)
    while ((*info->plugin_get_public_info)(i, &ptr, &symbol, 
					   &description, &type, &hash))
      {
	if ((!symbol) || (!type) || (!ptr) || (hash == -1))
	  {
	    i++;
	    continue;
	  }
	 info->exported_symbols = (struct plugin_exported_symbol*)
	   realloc(info->exported_symbols,
		   sizeof(struct plugin_exported_symbol)*(i+1));
	 if (!info->exported_symbols)
	  g_error(_("There wasn't enough mem for allocating symbol %d in %s"),
		  i+1, info->file_name);
	
	info->exported_symbols[i].symbol = g_strdup(symbol);
 	info->exported_symbols[i].type = g_strdup(type);
	if (description)
	  info->exported_symbols[i].description =
	    g_strdup(description);
	else
	  info->exported_symbols[i].description =
	    g_strdup(_("[No description provided]"));
	info->exported_symbols[i].ptr = ptr;
	info->exported_symbols[i].hash = hash;
	info->num_exported_symbols++;
	i++;
      }

  return TRUE;
}

void plugin_unload(struct plugin_info * info)
{
  gint i;

  g_assert(info != NULL);

  /* Tell the plugin to close itself */
  plugin_close( info );

  /* Free the memory of the exported symbols */
  if (info -> num_exported_symbols > 0)
    {
      g_assert(info -> exported_symbols != NULL);
      for (i=0; i < info->num_exported_symbols; i++)
	{
	  g_free(info->exported_symbols[i].symbol);
	  g_free(info->exported_symbols[i].type);
	  g_free(info->exported_symbols[i].description);
	}
      g_free(info -> exported_symbols);
      info -> exported_symbols = NULL;
    }
  g_free(info -> file_name);
  g_free(info -> canonical_name);
  g_module_close (info -> handle);
}

/*
  This is the bridge given to the plugins.
*/
static gboolean plugin_bridge (gpointer * ptr, gchar * plugin, gchar *
			       symbol, gchar * type, gint hash)
{
  gint i;
  GList * list = g_list_first(plugin_list); /* From main.c */
  struct plugin_info * info;
  struct plugin_exported_symbol * es = NULL; /* A pointer to the table of
						exported symbols */
  gint num_exported_symbols=0; /* Number of exported symbols in the
				plugin */

  if (!plugin)
    {
      if (ptr)
	*ptr = GINT_TO_POINTER(0x2);
      return FALSE; /* Zapping exports no symbols */
    }
  else /* We have to query the list of plugins */
    while (list)
      {
	info = (struct plugin_info*) list->data;
	if (!strcasecmp(info->canonical_name, plugin))
	  {
	    es = info->exported_symbols;
	    num_exported_symbols = info->num_exported_symbols;
	    break;
	  }
	list = list->next;
      }

  if (!es)
    {
      if (ptr)
	*ptr = GINT_TO_POINTER(0x1); /* Plugin not found */
      return FALSE;
    }

  /* Now try to find the given symbol in the table of exported symbols
   of the plugin */
  for (i=0; i<num_exported_symbols; i++)
    if (!strcmp(es[i].symbol, symbol))
      {
	if (es[i].hash != hash)
	  {
	    if (ptr)
	      *ptr = GINT_TO_POINTER(0x3);
	    /* Warn */
	    g_warning(_("Check error: \"%s\" in plugin %s"
			" was supposed to be \"%s\" but it is:"
			"\"%s\". Hashes are 0x%x vs. 0x%x"), symbol,
		      plugin ? plugin : "Zapping", type,
		      es[i].type, hash, es[i].hash);
	    return FALSE;
	  }
	if (ptr)
	  *ptr = es[i].ptr;
	return TRUE; /* Success */
      }

  if (ptr)
    *ptr = GINT_TO_POINTER(0x2); /* Symbol not found in the plugin */
  return FALSE;
}

/* This are wrappers to avoid the use of the pointers in the
   plugin_info struct just in case somewhen the plugin system changes */
gint plugin_protocol(struct plugin_info * info)
{
  g_assert(info != NULL);

  return PLUGIN_PROTOCOL;
}

gboolean plugin_init ( tveng_device_info * device_info,
			      struct plugin_info * info )
{
  g_assert(info != NULL);

  if (!info->plugin_init)
    return TRUE;

  return (((*info->plugin_init)((PluginBridge)plugin_bridge,
			      device_info)));
}

void plugin_close(struct plugin_info * info)
{
  g_assert(info != NULL);

  if (!info->plugin_close)
    return;

  ((*info->plugin_close))();
}

gboolean plugin_start (struct plugin_info * info)
{
  g_assert(info != NULL);

  if (!info->plugin_start)
    return TRUE;

  return ((*info->plugin_start))();
}

void plugin_stop (struct plugin_info * info)
{
  g_assert(info != NULL);

  if (!info->plugin_stop)
    return;

  ((*info->plugin_stop))();
}

void plugin_load_config(struct plugin_info * info)
{
  gchar * key = NULL;
  gchar * buffer = NULL;
  g_assert(info != NULL);

  if (!info->plugin_load_config)
    return;

  plugin_get_info (&buffer, NULL, NULL, NULL, NULL, NULL, info);
  key = g_strconcat("/zapping/plugins/", buffer, "/", NULL);
  ((*info->plugin_load_config))(key);
  g_free ( key );
}

void plugin_save_config(struct plugin_info * info)
{
  gchar * key = NULL;
  gchar * buffer = NULL;
  g_assert(info != NULL);

  if (!info->plugin_save_config)
    return;

  plugin_get_info (&buffer, NULL, NULL, NULL, NULL, NULL, info);
  key = g_strconcat("/zapping/plugins/", buffer, "/", NULL);
  ((*info->plugin_save_config))(key);
  g_free ( key );
}

void plugin_get_info(gchar ** canonical_name, gchar **
		      descriptive_name, gchar ** description, gchar
		      ** short_description, gchar ** author, gchar **
		      version, struct plugin_info * info)
{
  g_assert(info != NULL);

  ((*info->plugin_get_info))(canonical_name, descriptive_name,
			     description, short_description, author,
			     version);
}

/* These functions are more convenient when accessing some individual
   fields of the plugin's info */
gchar * plugin_get_canonical_name (struct plugin_info * info)
{
  g_assert(info != NULL);

  return (info -> canonical_name);
}

gchar * plugin_get_name (struct plugin_info * info)
{
  gchar * buffer;
  g_assert(info != NULL);

  plugin_get_info(NULL, &buffer, NULL, NULL, NULL, NULL, info);
  return buffer;
}

gchar * plugin_get_description (struct plugin_info * info)
{
  gchar * buffer;
  g_assert(info != NULL);
  plugin_get_info(NULL, NULL, &buffer, NULL, NULL, NULL, info);
  return buffer;
}

gchar * plugin_get_short_description (struct plugin_info * info)
{
  gchar * buffer;
  g_assert(info != NULL);
  plugin_get_info(NULL, NULL, NULL, &buffer, NULL, NULL, info);
  return buffer;
}

gchar * plugin_get_author (struct plugin_info * info)
{
  gchar * buffer;
  g_assert(info != NULL);
  plugin_get_info(NULL, NULL, NULL, NULL, &buffer, NULL, info);
  return buffer;
}

gchar * plugin_get_version (struct plugin_info * info)
{
  gchar * buffer;
  g_assert(info != NULL);
  plugin_get_info(NULL, NULL, NULL, NULL, NULL, &buffer, info);
  return buffer;
}

gboolean plugin_running ( struct plugin_info * info)
{
  g_assert(info != NULL);

  if (!info->plugin_running)
    return FALSE; /* If the plugin doesn't care, we shouldn't either */

  return (*(info->plugin_running))();
}

void plugin_write_bundle (capture_bundle * bundle, struct plugin_info
			  * info)
{
  g_assert(info != NULL);
  g_return_if_fail(bundle != NULL);
  g_return_if_fail(bundle -> image_type != 0);

  if (info -> plugin_write_bundle)
    (*info->plugin_write_bundle)(bundle);
}

void plugin_read_bundle (capture_bundle * bundle, struct plugin_info
			 * info)
{
  g_assert(info != NULL);
  g_return_if_fail(bundle != NULL);
  g_return_if_fail(bundle -> image_type != 0);

  if (info -> plugin_read_bundle)
    (*info->plugin_read_bundle)(bundle);
}

void plugin_capture_stop (struct plugin_info * info)
{
  g_assert(info != NULL);

  if (info -> plugin_capture_stop)
    (*info->plugin_capture_stop)();
}

gboolean plugin_add_properties (GnomePropertyBox * gpb, struct plugin_info
				   * info)
{
  g_assert(info != NULL);

  if (info -> plugin_add_properties)
    return ((*info->plugin_add_properties)(gpb));
  else
    return FALSE;
}

gboolean plugin_activate_properties (GnomePropertyBox * gpb, gint
				      page, struct plugin_info * info)
{
  g_assert(info != NULL);
  g_assert(gpb != NULL);
  if (info -> plugin_activate_properties)
    return ((*info->plugin_activate_properties)(gpb, page));
  else
    return FALSE;
}

gboolean plugin_help_properties (GnomePropertyBox * gpb, gint page,
				  struct plugin_info * info)
{
  g_assert(info != NULL);
  g_assert(gpb != NULL);
  if (info -> plugin_help_properties)
    return ((*info->plugin_help_properties)(gpb, page));
  else
    return FALSE;
}

void plugin_add_gui (GnomeApp * app, struct plugin_info * info)
{
  g_assert(info != NULL);
  g_assert(app != NULL);

  if (info -> plugin_add_gui)
    ((*info->plugin_add_gui)(app));
}

void plugin_remove_gui (GnomeApp * app, struct plugin_info * info)
{
  g_assert(info != NULL);
  g_assert(app != NULL);

  if (info -> plugin_remove_gui)
    return ((*info->plugin_remove_gui)(app));
}

gint plugin_get_priority (struct plugin_info * info)
{
  g_assert(info != NULL);

  return info -> misc_info.plugin_priority;
}

void plugin_process_popup_menu (GtkWidget	*widget,
				GdkEventButton	*event,
				GtkMenu	*popup,
				struct plugin_info *info)
{
  g_assert(info != NULL);

  if (info->plugin_process_popup_menu)
    (*info->plugin_process_popup_menu)(widget, event, popup);
}

/* Loads all the valid plugins in the given directory, and appends them to
   the given GList. It returns the new GList. The plugins should
   contain exp in their filename (usually called with exp = .zapping.so) */
static GList * plugin_load_plugins_in_dir( gchar * directory, gchar * exp,
					   GList * old )
{
  struct dirent ** namelist;
  int n; /* Number of scanned items */
  struct plugin_info plug;
  struct plugin_info * new_plugin; /* If plugin is OK, a copy will be
				      allocated here */
  GList * p;

  int i;
  gchar * filename; /* Complete path to the plugin to load */

  g_assert(exp != NULL);
  g_assert(directory != NULL);
  g_assert(strlen(directory) > 0);

  printv("looking for plugins in %s\n", directory);

  n = scandir(directory, &namelist, 0, alphasort);
  if (n < 0)
    {
      /* Show error just when there is actually an error */
      if (errno != ENOENT)
	perror("scandir");
      return old;
    }

  for (i = 0; i < n; i++)
    {
      filename = strstr(namelist[i]->d_name, exp);

      if ((!filename) || (strlen(exp) != strlen(filename)))
	{
	  free(namelist[i]);
	  continue;
	}

      filename = g_strconcat(directory, directory[strlen(directory)-1] ==
			     '/' ? "" : "/", namelist[i] -> d_name,
			     NULL);
      free (namelist[i]);

      if (!plugin_load(filename, &plug))
	{
	plugin_load_error:
	  g_free(filename);
	  continue;
	}
      /* Check whether there is no other other plugin with the same
	 canonical name */
      p = g_list_first(old);
      while (p)
	{
	  new_plugin = (struct plugin_info*)p->data;
	  if (!strcasecmp(new_plugin->canonical_name,
			  plug.canonical_name))
	    {
	      /* Collision, load the highest version number */
	      if (new_plugin->major > plug.major)
		goto plugin_load_error;
	      else if (new_plugin->major == plug.major)
		{
		  if (new_plugin->minor > plug.minor)
		    goto plugin_load_error;
		  else if (new_plugin->minor == plug.minor)
		    {
		      if (new_plugin->micro >= plug.micro)
			goto plugin_load_error;
		    }
		}
	      /* Replace the old one with the new one, just delete the
	       old one */
	      old = g_list_remove(old, new_plugin);
	      plugin_unload(new_plugin);
	      free(new_plugin);
	      break; /* There is no need to continue querying */
	    }
	  p = p->next;
	}
      g_free(filename);

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

  if (namelist)
    free(namelist);

  return old;
}

/* This is the function used to sort the plugins by priority */
static gint plugin_sorter (struct plugin_info * a, struct plugin_info * b)
{
  g_assert(a != NULL);
  g_assert(b != NULL);

  return (b->misc_info.plugin_priority - a->misc_info.plugin_priority);
}

static void add_plugin_properties (GnomePropertyBox *gpb)
{
  GList *p = g_list_first(plugin_list);

  while (p)
    {
      plugin_add_properties(gpb, (struct plugin_info*)p->data);
      p = p->next;
    }
}

static gboolean apply_plugin_properties (GnomePropertyBox *gpb, gint page)
{
  GList *p = g_list_first(plugin_list);

  while (p)
    {
      if (plugin_activate_properties(gpb, page,
				     (struct plugin_info*)p->data))
	return TRUE;
      p = p->next;
    }

  return FALSE;
}

static gboolean help_plugin_properties (GnomePropertyBox *gpb, gint page)
{
  GList *p = g_list_first(plugin_list);

  while (p)
    {
      if (plugin_help_properties(gpb, page,
				 (struct plugin_info*)p->data))
	return TRUE;
      p = p->next;
    }

  return FALSE;
}

/* Loads all the plugins in the system */
GList * plugin_load_plugins ( void )
{
  gchar * plugin_path; /* Path to the plugins */
  gchar * buffer;
  GList * list = NULL;
  FILE * fd;
  gint i;

  /* First load plugins in the home dir */
  buffer = g_get_home_dir();
  if (buffer)
    {
      g_assert(strlen(buffer) > 0);
      if (buffer[strlen(buffer)-1] == '/')
	plugin_path = g_strconcat(buffer, ".zapping/plugins", NULL);
      else
	plugin_path = g_strconcat(buffer, "/.zapping/plugins", NULL);
      list = plugin_load_plugins_in_dir (plugin_path, PLUGIN_STRID,
					 list);
      g_free( plugin_path );

      /* Now load ~/.zapping/plugins_dirs */
      if (buffer[strlen(buffer)-1] == '/')
	plugin_path = g_strconcat(buffer, ".zapping/plugin_dirs", NULL);
      else
	plugin_path = g_strconcat(buffer, "/.zapping/plugin_dirs",
				  NULL);
      fd = fopen (plugin_path, "r");
      g_free(plugin_path);
      if (fd)
	{
	  buffer = g_malloc0(1024);
	  while (fgets(buffer, 1023, fd))
	    {
	      plugin_path = buffer;
	      /* Skip all spaces */
	      while (*plugin_path == ' ')
		plugin_path++;

	      if ((strlen(plugin_path) == 0) || 
		  (plugin_path[0] == '\n') ||
		  (plugin_path[0] == '#')) /* Comments and empty lines
					    */
		continue;
	      if (plugin_path[strlen(plugin_path)-1] == '\n')
		plugin_path[strlen(plugin_path)-1] = 0;
	      /* Remove all spaces at the end of the string */
	      i = strlen(plugin_path)-1;
	      while ((plugin_path[i] == ' ') && (i>=0))
		{
		  plugin_path[i--] = 0;
		}
	      if (strlen(plugin_path) == 0)
		continue;
	      list = plugin_load_plugins_in_dir (plugin_path,
						 PLUGIN_STRID, list);
	    }
	  g_free (buffer);
	}
    }

  /* Now load plugins in $(prefix)/lib/zapping/plugins */
  buffer = PACKAGE_LIB_DIR;

  g_assert(strlen(buffer) > 0);
  if (buffer[strlen(buffer)-1] == '/')
    plugin_path = g_strconcat(buffer, "plugins", NULL);
  else
    plugin_path = g_strconcat(buffer, "/plugins", NULL);
  /* Load all the plugins in this dir */
  list = plugin_load_plugins_in_dir (plugin_path, PLUGIN_STRID, list);
  g_free(plugin_path);

  /* Register the plugin handler as a property box handler */
  {
    property_handler handler =
    {
      add:	add_plugin_properties,
      apply:	apply_plugin_properties,
      help:	help_plugin_properties
    };
    register_properties_handler(&handler);
  }

  list = g_list_sort (list, (GCompareFunc) plugin_sorter);

  return list;
}

/* This function is called from g_list_foreach */
static void plugin_foreach_free(struct plugin_info * info, void * user_data)
{
  plugin_save_config(info);
  plugin_unload(info);
  free(info);
}

/* Unloads all the plugins loaded in the GList */
void plugin_unload_plugins(GList * list)
{
  g_list_foreach ( list, (GFunc) plugin_foreach_free, NULL );
  g_list_free ( list );
}
