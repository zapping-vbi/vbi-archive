#ifndef __PLUGINS_H__
#define __PLUGINS_H__

/* Mainly for glib.h */
#include <gnome.h>

/* Mainly for ShowBox definition */
#include "tveng.h"

/* This structure holds the info needed for identifying and using a
   plugin */
struct plugin_info{
  gpointer handle;
  gchar plugin_protocol[8]; /* Plugin protocol the plugin uses */
  int major_version, minor_version; /* Minimum zapping version for
				       using this plugin */
  gchar * error; /* In case of error, the error string is stored here */
  gchar* (*plugin_get_info)(); /* Returns a pointer to the plugin info
				(long) */
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
  void (*plugin_eat_frame)(gchar * frame, struct v4l2_format * format);
};

#endif
