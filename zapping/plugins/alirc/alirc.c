/* Alirc: Another lirc plugin for Zapping
 * based on zappings template plugin
 * Copyright (C) 2001 Sjoerd Simons <sjoerd@luon.net>
 *
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

/* $Id: alirc.c,v 1.10 2004-10-09 02:52:24 mschimek Exp $ */

/* XXX gtk_input */
#undef GTK_DISABLE_DEPRECATED

#include "src/v4linterface.h" /* channel_key_press() */
#include "src/plugin_common.h"

#ifdef HAVE_LIRC
#include <lirc/lirc_client.h>
#include <time.h>
#include <ctype.h>

/* This is the description of the plugin, change as appropiate */
static const gchar str_canonical_name[] = "alirc";
static const gchar str_descriptive_name[] =
N_("Another lirc plugin");
static const gchar str_description[] =
N_("Another plugin to control zapping through lirc\n\n\
To enable this plugin you must edit your ~/.lircrc\n\
file. See the Zapping documentation for details.\n");

static const gchar str_short_description[] = 
N_("Lets you control zapping through lirc");
static const gchar str_author[] = "Sjoerd Simons";
/* The format of the version string must be
   %d[[.%d[.%d]][other_things]], where the things between [] aren't
   needed, and if not present, 0 will be assumed */
static const gchar str_version[] = "1.0";

/* Active status of the plugin */
static gboolean active = FALSE;

/* global lirc config struct */
static struct lirc_config *config = NULL;
/* lirc io_tag */
static guint lirc_iotag;
/* our link to the device struct */
static tveng_device_info *tveng_info;

static void
legacy_zoom			(const gchar *		args _unused_)
{
  python_command (NULL, "zapping.toggle_mode('fullscreen')");
}

static void
legacy_setchannel		(const gchar *		args)
{
  GdkEventKey event;
  gint n;

  if (args == NULL)
    return;

  n = atoi (args);

  if (n < 0)
    n = 0;

  if (n < 10)
    {
      /*
       *  This (preliminary) calls the routine used to
       *  enter channel numbers on the numeric keypad.
       *  Side effects: OSD, digits combine within timeout,
       *  numbers are interpreted as RF channel or channel name. 
       */
      event.keyval = GDK_KP_0 + n;
      
      channel_key_press (&event);
    }
  else
    {
      python_command_printf (NULL, "zapping.set_channel(%d)", n);
    }
}

struct legacy_command
{
  const gchar *		lirc_command;
  const gchar *		py_command;
  void			(* func)(const gchar *args);
};

static const struct legacy_command
legacy_command_txl_table [] =
{
  { "CHANUP",		"zapping.channel_up()",			NULL },
  { "CHANDOWN",		"zapping.channel_down()",		NULL },
  { "QUIT",		"zapping.quit()",			NULL },
  { "ZOOM",		NULL,				legacy_zoom },
  { "SETCHANNEL",	NULL,				legacy_setchannel },
  { "MUTE",		"zapping.mute()",			NULL },
  { "VOL_UP",		"zapping.control_incr('volume',+1)",	NULL },
  { "VOL_DOWN",		"zapping.control_incr('volume',-1)",	NULL },
};

static void 
run_command			(const gchar *		s)
{
  const struct legacy_command *lc;
  guint i;

  printv ("alirc: command string '%s'\n", s);

  while (*s && isspace(*s))
    s++;

  if (!*s)
    return;

  lc = legacy_command_txl_table;

  for (i = 0; i < G_N_ELEMENTS (legacy_command_txl_table); i++)
    {
      guint n = strlen (lc->lirc_command);

      if (0 == strncmp (s, lc->lirc_command, n)
	  && (s[n] == 0 || isspace(s[n])))
	{
	  printv ("alirc: command '%*s'\n", n, s);

	  s += n;

	  while (*s && isspace(*s))
	    s++;

	  if (lc->py_command)
	    {
	      printv ("alirc: command txl '%s'\n", lc->py_command);
	      python_command (NULL, lc->py_command);
	    }
	  else
	    {
	      printv ("alirc: command func w/args '%s'\n", s);
	      lc->func (s);
	    }

	  return;
	}

      lc++;
    }

  printv ("alirc: not a legacy command\n");

  python_command (NULL, s);
}

static void 
plugin_stop(void) {
  /* Most times we cannot be stopped while we are stopped */
  if (!active)
    return;
  /* stop it, we were active so */
  gtk_input_remove(lirc_iotag);
  lirc_freeconfig(config);
  printv("alirc: Freed config struct\n");
  lirc_deinit();
  printv("alirc: Lirc deinitialized\n");
  /* Stop anything the plugin is doing and set the flag */
  active = FALSE;
}

static void
lirc_receive(gpointer *data _unused_, int fd _unused_) {
  /* activity on the lirc socket, so let's check */
  char *string;
  char *command;
  if (lirc_nextcode(&string) != 0) {
    printv("alirc: Eeek somethings wrong with lirc\n");
    printv("alirc: Stopping plugin\n");
    plugin_stop();
  }
  printv("->Received from lirc:  %s",string);
  
  lirc_code2char(config,string,&command);
  while(command != NULL) {
    run_command(command);
    lirc_code2char(config,string,&command);
  }

}

/*
  Declaration of the static symbols of the plugin. Refer to the docs
  to know what does each of these functions do
*/
gint plugin_get_protocol ( void )
{
  /* You don't need to modify this function */
  return PLUGIN_PROTOCOL;
}

static
gboolean plugin_running (void)
{
  /* This will usually be like this too */
  return active;
}

static
void plugin_get_info (const gchar ** canonical_name,
		      const gchar ** descriptive_name,
		      const gchar ** description,
		      const gchar ** short_description,
		      const gchar ** author,
		      const gchar ** version)
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

static gboolean 
plugin_start (void) {
  /* In most plugins, you don't want to be started twice */
  int fd; 
  if (active)
    return TRUE;

  /* Do any neccessary work to start the plugin here */
  if ((fd = lirc_init("zapping_lirc",1)) < 0) {
    printv("alirc: Failed to initialize\n");
    return FALSE;
  }
  if (lirc_readconfig(NULL,&config,NULL) != 0) {
    printv("Couldn't read config file\n");
    return FALSE;
  }
  printv("alirc: Succesfully initialize\n");

  lirc_iotag = gtk_input_add_full (fd,
				   GDK_INPUT_READ,
				   (GdkInputFunction)
				   lirc_receive, NULL, NULL, NULL);
  /* If everything has been ok, set the active flags and return TRUE
   */
  active = TRUE;
  return TRUE;
}

static gboolean 
plugin_init (PluginBridge bridge _unused_, tveng_device_info * info) {
  /* Do any startup you need here, and return FALSE on error */
  printv("alirc plugin: init\n");
  tveng_info = info;

  /* If this is set, autostarting is on (we should start now) */
  if (active) {
    active = FALSE; /* autostarting, so we're not really active */
    return plugin_start();
  }

  return TRUE;
}

static void 
plugin_close(void) {
  /* If we were working, stop the work */
  if (active)
    plugin_stop();

  /* Any cleanups would go here (closing fd's and so on) */
}

static void 
plugin_load_config (gchar * root_key) {
  gchar * buffer;

  /* The autostart config value is compulsory, you shouldn't need to
     change the following */
     printv("alirc: loading configuration\n");
  buffer = g_strconcat(root_key, "autostart", NULL);
  /* Create sets a default value for a key, check src/zconf.h */
  zconf_create_boolean(FALSE,
		       "Whether the plugin should start"
		       " automatically when opening Zapping", buffer);
  active = zconf_get_boolean(NULL, buffer);
  g_free(buffer);

  /* Load here any other config key */
}

static void 
plugin_save_config (gchar * root_key) {
  gchar * buffer;

  /* This one is compulsory, you won't need to change it */
  buffer = g_strconcat(root_key, "autostart", NULL);
  zconf_set_boolean(active, buffer);
  g_free(buffer);

  /* Save here any other config keys you need to save */
}

static struct 
plugin_misc_info * plugin_get_misc_info (void) {
  static struct plugin_misc_info returned_struct =
  {
    sizeof(struct plugin_misc_info), /* size of this struct */
    6, /* plugin priority, this is just an example */
    0 /* Category */
  };

  /*
    Tell that the template plugin should be run with a somewhat high
    priority (just to put an example)
  */
  return (&returned_struct);
}

/* Return FALSE if we aren't able to access a symbol, you should only
   need to edit the pointer table, not the code */
gboolean plugin_get_symbol(gchar * name, gint hash, gpointer * ptr)
{
  /* Usually this table is the only thing you will need to change */
  const struct plugin_exported_symbol table_of_symbols[] =
  {
    SYMBOL(plugin_get_info, 0x1234),
    SYMBOL(plugin_init, 0x1234),
    SYMBOL(plugin_close, 0x1234),
    SYMBOL(plugin_start, 0x1234),
    SYMBOL(plugin_stop, 0x1234),
    SYMBOL(plugin_load_config, 0x1234),
    SYMBOL(plugin_save_config, 0x1234),
    SYMBOL(plugin_running, 0x1234),
    SYMBOL(plugin_get_misc_info, 0x1234)
  };
  gint num_exported_symbols =
    sizeof(table_of_symbols)/sizeof(struct plugin_exported_symbol);
  gint i;

  /* Try to find the given symbol in the table of exported symbols
   of the plugin */
  for (i=0; i<num_exported_symbols; i++)
    if (!strcmp(table_of_symbols[i].symbol, name))
      {
	if (table_of_symbols[i].hash != hash)
	  {
	    if (ptr)
	      *ptr = GINT_TO_POINTER(0x3); /* hash collision code */
	    /* Warn */
	    g_warning(_("Check error: \"%s\" in plugin %s "
		       "has hash 0x%x vs. 0x%x"), name,
		      str_canonical_name, 
		      table_of_symbols[i].hash,
		      hash);
	    return FALSE;
	  }
	if (ptr)
	  *ptr = table_of_symbols[i].ptr;
	return TRUE; /* Success */
      }

  if (ptr)
    *ptr = GINT_TO_POINTER(0x2); /* Symbol not found in the plugin */
  return FALSE;
}

#endif /* ifdef HAVE_LIRC */
