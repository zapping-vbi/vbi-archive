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
#ifndef __PLUGINS_H__
#define __PLUGINS_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"		/* VERSION */
#endif

/* Mainly for glib.h */
#include <gnome.h>

/* for scanning directories */
#include <dirent.h>

/* for dlopen() and friends */
#include <gmodule.h>

/* Mainly for ShowBox definition */
#include "zmisc.h"

/* For lookup_widget */
#include "interface.h"

/* Configuration saving and restoring */
#include "zconf.h"


/* For the tveng structures */
#include "tveng.h"

/* Some definitions common to the plugins and Zapping */
#define ZAPPING_SRC /* Tell this header that we are Zapping */
#include "plugin_common.h"

/* Any plugin should have its filename ending in this string */
#define PLUGIN_STRID ".zapping.so"

/* This structure holds the info needed for identifying and using a
   plugin */
struct plugin_info{
  GModule * handle; /* The handle to the plugin */
  /* This returns the protocol the plugin understands */
  gint (*plugin_protocol) (void);

  gboolean (*get_symbol)(const gchar *name, gint hash, gpointer *ptr);

  /******* OPTATIVE FUNCTIONS *******/
  /* Init the plugin using the current video device, FALSE on error */
  gboolean (*plugin_init)(PluginBridge bridge, tveng_device_info *
			  info);
  /* Close the plugin */
  void (*plugin_close) (void);
  /* The plugin should start to work when this is called */
  gboolean (*plugin_start)( void );
  /* Stop the plugin if it is running */
  void (*plugin_stop) ( void );
  /* Tells the plugin that it can now load its config */
  void (*plugin_load_config) ( gchar * root_key );
  /* Tells the plugin to save its config */
  void (*plugin_save_config) ( gchar * root_key);
  /* Gets some info about the plugin */
  void (*plugin_get_info) ( gchar ** canonical_name, gchar **
			    descriptive_name, gchar ** description,
			    gchar ** short_description, gchar **
			    author, gchar ** version);
  /* Returns TRUE if the plugin is working */
  gboolean (*plugin_running) ( void );

  /* Read only processing of the frame (gtk+ thread) */
  void (*plugin_read_frame) ( capture_frame * frame );
  void (*plugin_capture_start)	( void );
  /* If the plugin is capturing using the provided fifo, it must stop
   when this call returns */
  void (*plugin_capture_stop)	( void );
  /* Used to query the public symbols from the plugin */
  gboolean (*plugin_get_public_info) ( gint index, gpointer * ptr,
				       gchar ** symbol, 
				       gchar ** description,
				       gchar ** type, gint * hash );
  /* Add the plugin to the GUI */
  void (*plugin_add_gui) ( GnomeApp * app );
  /* Remove the plugin from the GUI */
  void (*plugin_remove_gui) ( GnomeApp * app );
  /* Get some misc info about the plugin */
  struct plugin_misc_info * (*plugin_get_misc_info) ( void );
  /* Add the plugin actions to the context menu */
  void (*plugin_process_popup_menu) ( GtkWidget	*widget,
				      GdkEventButton	*event,
				      GtkMenu	*popup );

  /******* Variables *********/
  struct plugin_misc_info misc_info; /* Info about the plugin */
  gchar * file_name; /* The name of the file that holds the plugin */
  gchar * canonical_name; /* The canonical name of the plugin */
  gint major, minor, micro; /* Plugin version number, used only
			       internally */
  /* The symbols that the plugin shares with other plugins */
  gint num_exported_symbols;
  struct plugin_exported_symbol * exported_symbols;
};


/*
  Wrappers to avoid having to access the struct's fields directly
*/
/* This are wrappers to avoid the use of the pointers in the
   plugin_info struct just in case somewhen the plugin system changes */
gint plugin_protocol(struct plugin_info * info);

gboolean plugin_init ( tveng_device_info * device_info,
		       struct plugin_info * info );

void plugin_close(struct plugin_info * info);

void plugin_unload(struct plugin_info * info);

gboolean plugin_start (struct plugin_info * info);

void plugin_stop (struct plugin_info * info);

void plugin_load_config(struct plugin_info * info);

void plugin_save_config(struct plugin_info * info);

void plugin_get_info(gchar ** canonical_name, gchar **
		     descriptive_name, gchar ** description, gchar
		     ** short_description, gchar ** author, gchar **
		     version, struct plugin_info * info);

/* These functions are more convenient when accessing some individual
   fields of the plugin's info */
gchar * plugin_get_canonical_name (struct plugin_info * info);

gchar * plugin_get_name (struct plugin_info * info);

gchar * plugin_get_description (struct plugin_info * info);

gchar * plugin_get_short_description (struct plugin_info * info);

gchar * plugin_get_author (struct plugin_info * info);

gchar * plugin_get_version (struct plugin_info * info);

gboolean plugin_running ( struct plugin_info * info);

void plugin_read_frame (capture_frame * frame,
			struct plugin_info * info);

void plugin_capture_start ( struct plugin_info * info);

void plugin_capture_stop ( struct plugin_info * info);

void plugin_add_gui (GnomeApp * app, struct plugin_info * info);

void plugin_remove_gui (GnomeApp * app, struct plugin_info * info);

gint plugin_get_priority (struct plugin_info * info);

void plugin_process_popup_menu (GtkWidget	*widget,
				GdkEventButton	*event,
				GtkMenu	*popup,
				struct plugin_info *info);

extern gboolean			plugin_key_press(GdkEventKey *event);

/*
  Loads the plugins, returning a GList. The data item of each element
  in the GList points to a plugin_info structure.
*/
GList * plugin_load_plugins ( void );

/* Unloads all the plugins loaded in the GList */
void plugin_unload_plugins(GList * list);

extern struct plugin_info *
plugin_by_name			(const gchar *		name);
extern gpointer
plugin_symbol			(const struct plugin_info *info,
				 const gchar *		name);

#endif
