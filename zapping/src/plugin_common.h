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
  Structures shared by Zapping and the plugins
*/
#ifndef __PLUGINS_COMMON_H__
#define __PLUGINS_COMMON_H__

#ifdef HAVE_CONFIG_H
#  include <config.h> /* VERSION */
#endif

#include <gnome.h>

#ifdef HAVE_LIBZVBI
#include <libzvbi.h>
#endif

#include "tveng.h"
#include "zmisc.h"
#include "zconf.h"
#include "interface.h"
#include "remote.h"
#include "x11stuff.h"
#include "../common/fifo.h"
#include "capture.h"
#include "frequencies.h"

/* The plugin protocol we are able to understand */
#define PLUGIN_PROTOCOL 2

/* The definition of a PluginBrigde */
typedef gboolean (*PluginBridge) ( gpointer * ptr, gchar * plugin,
				   gchar * symbol, gchar * type,
				   gint hash );

/* This structure holds info about one of the exported symbols of the
   plugin */
struct plugin_exported_symbol
{
  gpointer ptr; /* The resolved symbol */
  gchar * symbol; /* The name of the symbol */
  gchar * description; /* A brief description for the symbol */
  gchar * type; /* Symbol type */
  gint hash; /* Symbol hash */
};

/*
  Use this flags to describe your plugin. OR any convenient flags.
*/
#define PLUGIN_CATEGORY_AUDIO_OUT	(1<<0) /* Sends audio to other device */
#define PLUGIN_CATEGORY_AUDIO_PROCESS	(1<<1) /* Processes audio */
#define PLUGIN_CATEGORY_VIDEO_OUT	(1<<2) /* Sends video to other device */
#define PLUGIN_CATEGORY_VIDEO_PROCESS	(1<<3) /* Processes the video stream */
#define PLUGIN_CATEGORY_DEVICE_CONTROL	(1<<4) /* Controls the video device */
#define PLUGIN_CATEGORY_FILTER		(1<<5) /* Provides filters */
#define PLUGIN_CATEGORY_GUI		(1<<6) /* Modifies the GUI */

/*
  This struct holds some misc info about the plugin. If it exists, all
  fields must be set correctly upon return.
*/
struct plugin_misc_info
{
  gint size; /* Size of this structure */
  gint plugin_priority; /* Priority the plugin requests */
  gint plugin_category; /* Categories the plugin falls under */
};

extern void plugin_add_key				(const gchar *	canonical_name,
							 z_key		key,
							 void		(* func)(void));

#ifndef ZAPPING /* If this is being included from a plugin, give them
		   the correct prototypes for public symbols ( so
		   compiling will give an error if defined differently) */

gint plugin_get_protocol (void);
gboolean plugin_get_symbol(gchar * name, gint hash, gpointer * ptr);

#define UNUSED __attribute__ ((unused))

static gboolean plugin_running ( void ) UNUSED;
static
void plugin_get_info ( const gchar ** canonical_name,
		       const gchar ** descriptive_name,
		       const gchar ** description,
		       const gchar ** short_description,
		       const gchar ** author,
		       const gchar ** version ) UNUSED ;
static
gboolean plugin_init ( PluginBridge bridge, tveng_device_info * info ) UNUSED ;
static void plugin_close( void ) UNUSED ;
static gboolean plugin_start ( void ) UNUSED ;
static void plugin_stop( void ) UNUSED ;
static void plugin_load_config ( gchar * root_key ) UNUSED ;
static void plugin_save_config ( gchar * root_key ) UNUSED ;
static void plugin_write_bundle( capture_bundle * bundle ) UNUSED ;
static void plugin_read_bundle( capture_bundle * bundle ) UNUSED ;
static void plugin_capture_stop( void ) UNUSED ;
static
gboolean plugin_get_public_info (gint index, gpointer * ptr, gchar **
				 symbol, gchar ** description, gchar **
				 type, gint * hash) UNUSED ;
static void plugin_add_gui ( GnomeApp * app ) UNUSED ;
static void plugin_remove_gui ( GnomeApp * app ) UNUSED ;
static struct plugin_misc_info * plugin_get_misc_info ( void ) UNUSED ;
static void plugin_process_popup_menu ( GtkWidget *window,
					GdkEventButton *button,
					GtkMenu *popup) UNUSED;

/* This macro if for your convenience, it symplifies adding symbols */
#define SYMBOL(symbol, hash) \
{symbol, #symbol, NULL, NULL, hash}
#endif

#endif /* PLUGINS_COMMON */
