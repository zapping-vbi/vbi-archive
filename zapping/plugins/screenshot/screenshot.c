/* Screenshot saving plugin for Zapping
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
#include "plugin_common.h"
/*
  You can use this code as the template for your own plugins. If you
  have any doubts after reading this and the docs, please contact the
  author. Declaring things as static isn't needed, it is just to make
  clear what is to be exported and what isn't.
*/

/* This is the description of the plugin, change as appropiate */
static const gchar str_canonical_name[] = "screenshot";
static const gchar str_descriptive_name[] =
N_("Screenshot saver");
static const gchar str_description[] =
N_("This plugin takes the current capture and saves it as a "
"PNG screenshot.\nYou should configure it before using through the "
"Properties dialog.");
static const gchar str_short_description[] = 
N_("Takes a screenshot of the capture");
static const gchar str_author[] = "Iñaki García Etxebarria";
/* The format of the version string must be
   %d[[.%d[.%d]][other_things]], where the things between [] aren't
   needed, and if not present, 0 will be assumed */
static const gchar str_version[] = "0.7";

/* Active status of the plugin */
static gboolean active = FALSE;

gint zp_protocol ( void )
{
  /* You don't need to modify this function */
  return PLUGIN_PROTOCOL;
}

gboolean zp_running (void)
{
  /* This will usually be like this too */
  return active;
}

void zp_get_info (gchar ** canonical_name, gchar **
		  descriptive_name, gchar ** description, gchar **
		  short_description, gchar ** author, gchar **
		  version)
{
  /* Usually, this one doesn't need modification either */
  if (canonical_name)
    *canonical_name = _(str_canonical_name);
  if (descriptive_name)
    *descriptive_name = _(str_descriptive_name);
  if (description)
    *description = _(str_description);
  if (short_description)
    *short_description = _(str_short_description);
  if (author)
    *author = _(str_author);
  if (version)
    *version = _(str_version);
}

gboolean zp_init (PluginBridge bridge, tveng_device_info * info)
{
  /* Do any startup you need here, and return FALSE on error */

  /* If this is set, autostarting is on (we should start now) */
  if (active)
    return zp_start();

  return TRUE;
}

void zp_close(void)
{
  /* If we were working, stop the work */
  if (active)
    zp_stop();

  /* Any cleanups would go here (closing fd's and so on) */
}

gboolean zp_start (void)
{
  /* In most plugins, you don't want to be started twice */
  if (active)
    return TRUE;

  /* Do any neccessary work to start the plugin here */

  /* If everything has been ok, set the active flags and return TRUE
   */
  active = TRUE;
  return TRUE;
}

void zp_stop(void)
{
  /* Most times we cannot be stopped while we are stopped */
  if (!active)
    return;

  /* Stop anything the plugin is doing and set the flag */
  active = FALSE;
}

void zp_load_config (gchar * root_key)
{
  gchar * buffer;
  /* The autostart config value is compulsory, you shouldn't need to
     change the following */
  buffer = g_strconcat(root_key, "autostart", NULL);
  /* Create sets a default value for a key, check src/zconf.h */
  zconf_create_boolean(FALSE,
		       _("Whether the plugin should start"
			 " automatically when opening Zapping"), buffer);
  active = zconf_get_boolean(NULL, buffer);
  g_free(buffer);

  /* Load here any other config key */
}

void zp_save_config (gchar * root_key)
{
  gchar * buffer;

  /* This one is compulsory, you won't need to change it */
  buffer = g_strconcat(root_key, "autostart", NULL);
  zconf_set_boolean(active, buffer);
  g_free(buffer);

  /* Save here any other config keys you need to save*/
}
