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
#include "plugin_common.h"

#ifdef HAVE_LIRC
#include <lirc/lirc_client.h>

/* This is the description of the plugin, change as appropiate */
static const gchar str_canonical_name[] = "alirc";
static const gchar str_descriptive_name[] =
N_("Another lirc plugin");
static const gchar str_description[] =
N_("Another plugin to control zapping through lirc");
static const gchar str_short_description[] = 
N_("Let you control zapping through lirc");
static const gchar str_author[] = "Sjoerd Simons";
/* The format of the version string must be
   %d[[.%d[.%d]][other_things]], where the things between [] aren't
   needed, and if not present, 0 will be assumed */
static const gchar str_version[] = "1.0gamma";

/* Active status of the plugin */
static gboolean active = FALSE;

/* global lirc config struct */
static struct lirc_config *config = NULL;
/* lirc io_tag */
static int lirc_iotag;
/* our link to the device struct */
static tveng_device_info *tveng_info;

static void
lirc_channel_up(char *args) {
  remote_command("channel_up",NULL);
}

static void
lirc_channel_down(char *args) {
  remote_command("channel_down",NULL);
}

static void
lirc_quit(char *args) {
  remote_command("quit",NULL);
}

static void
lirc_zoom(char *args) {
  if (tveng_info->current_mode == TVENG_CAPTURE_PREVIEW) {
    remote_command("switch_mode",GINT_TO_POINTER(TVENG_CAPTURE_WINDOW));
  } else {
    remote_command("switch_mode",GINT_TO_POINTER(TVENG_CAPTURE_PREVIEW));
  }
}

static void
lirc_setchannel(char *args) {
  int channel,nchannels;

  GINT_TO_POINTER(nchannels) = remote_command("get_num_channels",NULL);
  if (args == NULL) return;
  channel = atoi(args);
  if (channel < 0 && channel >= nchannels ) channel = 0;
  remote_command("set_channel",GINT_TO_POINTER(channel));
}

struct lirc_key {
  char *command; /* lirc command */
  void (*func)(char *args); /* function to call */
};

/* list of lirc commands, and which function to call when that command is 
 |   given 
 */
static struct lirc_key lirc_keys[] = {    
  {"CHANUP",lirc_channel_up},	   
  {"CHANDOWN",lirc_channel_down},    
  {"QUIT",lirc_quit},                
  {"ZOOM",lirc_zoom},                
  {"SETCHANNEL",lirc_setchannel}
};

static void 
lirc_do_command(char *string) {
  char *backup,*command,*args;
  int num,i;

  backup = strdup(string);
  command = strtok(backup," ");
  if (command == NULL) {
    free(backup);
    return;
  }
  args = strtok(NULL,"");
  printv("alirc: Command->%s... Args->%s\n",command,args);

  num = sizeof(lirc_keys)/sizeof(struct lirc_key);

  for (i=0; i < num; i++) {
    if (!strcmp(command,lirc_keys[i].command)) {
      (lirc_keys[i].func)(args);
      return;
    }
  }
}

static void
lirc_receive(gpointer *data,int fd) {
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
    lirc_do_command(command);
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

/* Return FALSE if we aren't able to access a symbol, you should only
   need to edit the pointer table, not the code */
gboolean plugin_get_symbol(gchar * name, gint hash, gpointer * ptr)
{
  /* Usually this table is the only thing you will need to change */
  struct plugin_exported_symbol table_of_symbols[] =
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
	    g_warning(_("Check error: \"%s\" in plugin %s"
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

static
gboolean plugin_init (PluginBridge bridge, tveng_device_info * info)
{
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

static
void plugin_close(void)
{
  /* If we were working, stop the work */
  if (active)
    plugin_stop();

  /* Any cleanups would go here (closing fd's and so on) */
}

static
gboolean plugin_start (void)
{
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

  lirc_iotag = gdk_input_add(fd,GDK_INPUT_READ,(GdkInputFunction)
                             lirc_receive, NULL);
  /* If everything has been ok, set the active flags and return TRUE
   */
  active = TRUE;
  return TRUE;
}

static
void plugin_stop(void)
{
  /* Most times we cannot be stopped while we are stopped */
  if (!active)
    return;
  /* stop it, we were active so */
  gdk_input_remove(lirc_iotag);
  lirc_freeconfig(config);
  printv("alirc: Freed config struct\n");
  lirc_deinit();
  printv("alirc: Lirc deinitialized\n");
  /* Stop anything the plugin is doing and set the flag */
  active = FALSE;
}

static
void plugin_load_config (gchar * root_key)
{
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

static
void plugin_save_config (gchar * root_key)
{
  gchar * buffer;

  /* This one is compulsory, you won't need to change it */
  buffer = g_strconcat(root_key, "autostart", NULL);
  zconf_set_boolean(active, buffer);
  g_free(buffer);

  /* Save here any other config keys you need to save */
}

static
struct plugin_misc_info * plugin_get_misc_info (void)
{
  static struct plugin_misc_info returned_struct =
  {
    sizeof(struct plugin_misc_info), /* size of this struct */
    6, /* plugin priority, this is just an example */
    0 /* Cathegory */
  };

  /*
    Tell that the template plugin should be run with a somewhat high
    priority (just to put an example)
  */
  return (&returned_struct);
}
#endif
