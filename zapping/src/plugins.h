#ifndef __PLUGINS_H__
#define __PLUGINS_H__

#ifdef HAVE_CONFIG_H
#  include <config.h> /* VERSION */
#endif

/* Mainly for glib.h */
#include <gnome.h>

/* dlopen() and so on */
#include <dlfcn.h>

/* for scanning directories */
#include <dirent.h>

/* Mainly for ShowBox definition */
#include "tveng.h"

/* The plugin protocol we are able to understand */
#define PLUGIN_PROTOCOL "Zapping_Plugin_Protocol_1"

/* This structure holds the info needed for identifying and using a
   plugin */
struct plugin_info{
  gpointer handle;
  int major, minor, micro; /* plugin version */
  int zapping_major, zapping_minor, zapping_micro; /* Minimum zapping
						      version */
  gchar * error; /* In case of error, the error string is stored here */
  gchar* (*plugin_get_info)(); /* Returns a pointer to the plugin info
				(should be a long descriptive string) */
  gchar* (*plugin_get_canonical_name)(); /* Pointer to the canonical
					    name */
  gchar* (*plugin_get_name)(); /* The name of the plugin */
  /* Init the plugin using the current video device, FALSE on error */
  gboolean (*plugin_init)(tveng_device_info * info);
  /* Close plugin (for freeing memory, closing handles, etc) */
  void (*plugin_close)();
  /* Add the plugin to the GUI (update menus and toolbar), the
     parameter is the main window */
  void (*plugin_add_gui)(GtkWidget * main_window);
  /* Remove the plugin from the GUI */
  void (*plugin_remove_gui)(GtkWidget * main_window);
  /* Give a frame to the plugin in the current capture format. The
     given frame is writable, so converter plugins don't segfault. The
     format parameter holds the current frame characteristics, and can
     also be modified (it's just a copy of the real v4l2_format
     structure) */
  void (*plugin_eat_frame)(gchar * frame, struct v4l2_format *
			   format);
  /* TODO: Add properties and config */
};

/* Loads a plugin */
gboolean plugin_load(gchar * file_name, struct plugin_info * info);

/* Unloads a plugin */
void plugin_unload(struct plugin_info * info);

/* This are wrappers to avoid the use of the pointers in the
   plugin_info struct just in case somewhen the plugin system changes */
/* If the plugin returns NULL, then avoid SEGFAULT */
gchar * plugin_get_name(struct plugin_info * info);

/* This function should never return NULL, since plugin_load should
   fail if the canonical name is NULL */
gchar * plugin_get_canonical_name(struct plugin_info * info);

gchar * plugin_get_info(struct plugin_info * info);

/* Loads all the valid plugins in the given directory, and appends them to
   the given GList. It returns the new GList. The plugins should
   contain exp in their filename (usually called with exp = .zapping.so) */
GList * plugin_load_plugins(gchar * directory, gchar * exp, GList * old);

/* Unloads all the plugins loaded in the GList */
void plugin_unload_plugins(GList * list);

#endif
