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
  /* The plugin should start if it isn't started yet. This should be
     called from plugin_init automagically if the config said so (the
     plugin itself should take care of that) */
  gboolean (*plugin_start)();
  /* Stop the plugin if it is running */
  void (*plugin_stop)();
  /* Returns TRUE if the plugin is running */
  gboolean (*plugin_running)();
  /* The name(s) of the plugin author(s) */
  gchar* (*plugin_author)();
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
  void (*plugin_eat_frame)(struct tveng_frame_format * format);
  /* Add the plugin entries to the property box, we should connect
     callbacks from the plugin appropiately */
  void (*plugin_add_properties)(GnomePropertyBox * gpb);
  /* This function is called when apply'ing the changes of the
     property box. The plugin should ignore (returning FALSE) this
     call if n is not the property page it has created for itself */
  gboolean (*plugin_apply_properties)(GnomePropertyBox * gpb, int n);
  /* This one is similar to the above, but it is called when pressing
     help on the Property Box */
  gboolean (*plugin_help_properties)(GnomePropertyBox * gpb, int n);
  /* The structure will be written to the config file just before
     calling plugin_close and read just before calling plugin_init. */
  struct ParseStruct * parse_struct;
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

/* Returns the long descriptive strings identifying the plugin and
   descibing its use */
gchar * plugin_get_info(struct plugin_info * info);

/* Inits the plugin for the given device and returns TRUE if the
   plugin coold be inited succesfully */
gboolean plugin_init(tveng_device_info * info, struct plugin_info *
		     plug_info);

/* Closes the plugin, tells it to close all fds and so on */
void plugin_close(struct plugin_info * info);

/* Starts the execution of the plugin, TRUE on success */
gboolean plugin_start(struct plugin_info * info);

/* stops (pauses) the execution of the plugin */
void plugin_stop(struct plugin_info * info);

/* Returns TRUE if the plugin is running now */
gboolean plugin_running(struct plugin_info * info);

/* The name of the plugin author */
gchar * plugin_author(struct plugin_info * info);

/* Add the plugin to the GUI */
void plugin_add_gui(GtkWidget * main_window, struct plugin_info * info);

/* Remove the plugin from the GUI */
void plugin_remove_gui(GtkWidget * main_window, struct plugin_info * info);

/* Let the plugin add a page to the property box */
void plugin_add_properties(GnomePropertyBox * gpb, struct plugin_info
			   * info);

/* This function is called when apply'ing the changes of the
   property box. The plugin should ignore (returning FALSE) this
   call if n is not the property page it has created for itself */
gboolean plugin_apply_properties(GnomePropertyBox * gpb, int n, struct
				 plugin_info * info);

/* Give a frame to the plugin so it can process it */
void plugin_eat_frame(struct tveng_frame_format * frame, struct
		      plugin_info * info);

/* This one is similar to the above, but it is called when pressing
   help on the Property Box */
gboolean plugin_help_properties(GnomePropertyBox * gpb, int n,
				struct plugin_info * info);

/* Loads all the valid plugins in the given directory, and appends them to
   the given GList. It returns the new GList. The plugins should
   contain exp in their filename (usually called with exp = .zapping.so) */
GList * plugin_load_plugins(gchar * directory, gchar * exp, GList * old);

/* Unloads all the plugins loaded in the GList */
void plugin_unload_plugins(GList * list);

#endif
