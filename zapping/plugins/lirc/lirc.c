/* LIRC plugin for Zapping
 * Copyright (C) 2001 Marco Pfattner
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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <errno.h>
#include <pthread.h>

#include "callbacks.h"
#include "plugin_common.h"
#include "tveng.h"
#include "remote.h"
#include "lirc.h"

/* This is the description of the plugin, change as appropiate */
static const gchar str_canonical_name[] = "lirc";
static const gchar str_descriptive_name[] = N_("LIRC plugin");
static const gchar str_description[] = N_("This plugin enables the usage of LIRC, the Linux Infrared Remote Control.");
static const gchar str_short_description[] = N_("This plugin enables the usage of LIRC.");
static const gchar str_author[] = "Marco Pfattner";
static const gchar str_version[] = "0.1";

/* Active status of the plugin */
static gboolean active = FALSE;
static gboolean first = TRUE;

static pthread_t lirc_thread_id;
static int thread_exit = 0;

static int num_channels;

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
    SYMBOL(plugin_process_sample, 0x1234),
    SYMBOL(plugin_add_properties, 0x1234),
    SYMBOL(plugin_activate_properties, 0x1234),
    SYMBOL(plugin_help_properties, 0x1234),
    SYMBOL(plugin_add_gui, 0x1234),
    SYMBOL(plugin_remove_gui, 0x1234),
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
  num_channels = (int)remote_command("get_num_channels", NULL);

  printf("lirc plugin: init\n");
  printf("lirc plugin: number of channels: %d\n", num_channels);

  /* If this is set, autostarting is on (we should start now) */
  if (active)
    return plugin_start();
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
  if (active && !first)
    return TRUE;

  first = FALSE;

  /* Do any neccessary work to start the plugin here */
  thread_exit = 0;
  if (init_socket() == INIT_SOCKET_FAILED) return FALSE;
  if (pthread_create(&lirc_thread_id, NULL, lirc_thread, NULL)) return FALSE;


  /* If everything has been ok, set the active flags and return TRUE */
  active = TRUE;
  return TRUE;
}

static
void plugin_stop(void)
{
  /* Most times we cannot be stopped while we are stopped */
  if (!active)
    return;

  thread_exit = 1;

  /* Stop anything the plugin is doing and set the flag */
  active = FALSE;
}

static
void plugin_load_config (gchar * root_key)
{
  gchar *buffer;
  gchar *button, *action;
  gchar number[15];
  int i=0;
  action_list_item *item;

  printf("lirc plugin: loading configuration\n");

  /* The autostart config value is compulsory, you shouldn't need to
     change the following */
  buffer = g_strconcat(root_key, "autostart", NULL);
  /* Create sets a default value for a key, check src/zconf.h */
  zconf_create_boolean(FALSE,"Whether the plugin should start"
		       " automatically when opening Zapping", buffer);
  active = zconf_get_boolean(NULL, buffer);
  g_free(buffer);

  while(1) {
    sprintf(number, "action_%d", ++i);
    buffer = g_strconcat(root_key, "actions/", number, NULL);
    button = zconf_get_string(NULL, buffer);
    action = zconf_get_description(NULL, buffer);
    g_free(buffer);
    if (button != NULL) {
      item = (action_list_item*)malloc(sizeof(action_list_item));
      strncpy(item->button, button, 20);
      strncpy(item->action, action, 30);
      add_action(item);
    }
    else
      return;
  }

  printf("\n");

  /* Load here any other config key */
}

static
void plugin_save_config (gchar * root_key)
{
  gchar *buffer;
  action_list_item *item;
  gchar number[15];
  int i=0;

  printf("lirc plugin: saving configuration\n");

  /* This one is compulsory, you won't need to change it */
  buffer = g_strconcat(root_key, "autostart", NULL);
  zconf_set_boolean(active, buffer);

  buffer = g_strconcat(root_key, "actions");
  zconf_delete(buffer);
  g_free(buffer);

  item = first_item;
  while (item != NULL) {
    sprintf(number, "action_%d", ++i);
    buffer = g_strconcat(root_key, "actions/", number, NULL);
    zconf_create_string(item->button, item->action, buffer);
    zconf_set_string(item->button, buffer);
    item = item->next;
    g_free(buffer);
  }

  /* Save here any other config keys you need to save */
}

static
void plugin_process_sample(plugin_sample * sample)
{
  /* If the plugin isn't active, it shouldn't do anything */
  if (!active)
    return;
}

static
void plugin_add_properties ( GnomePropertyBox * gpb )
{
  GtkWidget * label;
  GtkBox * vbox; /* the page added to the notebook */
  gint page;

  printf("lirc plugin: adding properties\n");

  vbox = GTK_BOX(gtk_vbox_new(FALSE, 15));

  create_lirc_properties(GTK_WIDGET(vbox));
  add_actions_to_list();

  gtk_widget_show(GTK_WIDGET(vbox));

  label = gtk_label_new(_("LIRC"));
  gtk_widget_show(label);
  lirc_page = gnome_property_box_append_page(gpb, GTK_WIDGET(vbox), label);

  gtk_object_set_data(GTK_OBJECT(gpb), "lirc_page", GINT_TO_POINTER(page));
}

static
gboolean plugin_activate_properties ( GnomePropertyBox * gpb, gint page )
{
  /* Return TRUE only if the given page have been builded by this
     plugin, and apply any config changes here */
  return FALSE;
}

static
gboolean plugin_help_properties ( GnomePropertyBox * gpb, gint page )
{
  /*
    Return TRUE only if the given page have been builded by this
    plugin, and show some help (or at least sth like ShowBox
    "Sorry, but the template plugin doesn't help you").
  */
  if (page == lirc_page) {
    ShowBox("LIRC plugin version 0.1 by Marco Pfattner\nmarco.p@bigfoot.com", GNOME_MESSAGE_BOX_INFO);
    return TRUE;
  }
  return FALSE;
}

static
void plugin_add_gui (GnomeApp * app)
{
}

static
void plugin_remove_gui (GnomeApp * app)
{
}

static
struct plugin_misc_info * plugin_get_misc_info (void)
{
  static struct plugin_misc_info returned_struct =
  {
    sizeof(struct plugin_misc_info),
    6,
    PLUGIN_CATHEGORY_DEVICE_CONTROL |
    PLUGIN_CATHEGORY_GUI
  };

  /*
    Tell that the template plugin should be run with a somewhat high
    priority (just to put an example)
  */
  return (&returned_struct);
}

static
void *lirc_thread(void *dummy)
{
  int i;
  char buf[128];
  char *action;

  char new_button[20];
  char old_button[20];
  char check_button[50];

  char *buf2;

  struct timeval time;

  long old_msec = 0;
  long new_msec = 0;
  long diff;

  old_button[0] = '\0';

  while(!thread_exit) {
    i = read(fd, buf, 128);
    if (i == -1) {
      perror("read");
    }

    buf2 = buf;
    while(buf2++[0] != ' ');
    while(buf2++[0] != ' ');
    
    i=0;
    while(buf2[i++] != ' ');
    buf2[i-1] = '\0';

    gettimeofday(&time, NULL);
    new_msec = time.tv_sec * 1000 + time.tv_usec / 1000;

    if (old_msec != new_msec) {
      diff = new_msec - old_msec;
      old_msec = new_msec;
    }

    strncpy(new_button, buf2, 20);
    strncpy(check_button, buf2, 20);

    printf("lirc plugin: button %s pressed\n", buf2);
    printf("lirc plugin: time: %ld\n", new_msec);
    printf("lirc plugin: diff: %ld\n", diff);

    printf("lirc plugin: old button: %s\n", old_button);

    if (diff <= 1500) {
      strncpy(check_button, old_button, 20);
      strcat(check_button, ":");
      strcat(check_button, new_button);
      action = get_action(check_button);
      printf("lirc plugin: action for button %s: %s\n", check_button, action);
      if (action == NULL) {
	strncpy(check_button, new_button, 20);
      }
    }

    action = get_action(check_button);
    printf("lirc plugin: action for button %s: %s\n", check_button, action);

    strncpy(old_button, new_button, 20);
    
    if (action == NULL) continue;

    if (strcmp(action, "power off") == 0) remote_command("quit", NULL);
    else if (strcmp(action, "channel up") == 0) remote_command("channel_up", NULL);
    else if (strcmp(action, "channel down") == 0) remote_command("channel_down", NULL);
    else if (strncmp(action, "set channel", 11) == 0) {
      /* extract channel number */
      action += 12;
      i = atoi(action);
      set_channel(i);
    }
  }

  return NULL;
}

static
int init_socket()
{
  addr.sun_family=AF_UNIX;
  strcpy(addr.sun_path, "/dev/lircd");
  fd=socket(AF_UNIX, SOCK_STREAM, 0);
  if(fd == -1)  {
    perror("socket");
    return INIT_SOCKET_FAILED;
  };
  
  if(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("connect");
    return INIT_SOCKET_FAILED;
  };
  
  return INIT_SOCKET_OK;
}

static
void set_channel(int c)
{
  c--;
  if (c < 0) c = 0;
  if (c >= num_channels) c = num_channels - 1;
  
  remote_command("set_channel", GINT_TO_POINTER(c));
}

static GtkWidget*
create_lirc_properties (GtkWidget *lirc_properties)
{
  GtkWidget *lirc_vbox;
  GtkWidget *lirc_scrolledwindow;
  GtkWidget *lirc_label1;
  GtkWidget *lirc_label2;
  GtkWidget *lirc_table;
  GtkWidget *lirc_label3;
  GtkWidget *lirc_label4;
  GList *lirc_combo_action_items = NULL;
  GtkWidget *lirc_combo_entry_action;
  GList *lirc_combo_channel_items = NULL;
  GtkWidget *lirc_combo_entry_channel;
  GtkWidget *lirc_fixed;
  GtkWidget *lirc_button_add;
  GtkWidget *lirc_button_delete;
  int i=0;
  char *number;

  lirc_vbox = gtk_vbox_new (FALSE, 5);
  gtk_widget_ref (lirc_vbox);
  gtk_object_set_data_full (GTK_OBJECT (lirc_properties), "lirc_vbox", lirc_vbox,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_vbox);
  gtk_container_add (GTK_CONTAINER (lirc_properties), lirc_vbox);
  gtk_container_set_border_width (GTK_CONTAINER (lirc_vbox), 5);

  lirc_scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (lirc_scrolledwindow);
  gtk_object_set_data_full (GTK_OBJECT (lirc_properties), "lirc_scrolledwindow", lirc_scrolledwindow,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_scrolledwindow);
  gtk_box_pack_start (GTK_BOX (lirc_vbox), lirc_scrolledwindow, TRUE, TRUE, 0);

  lirc_actionlist = gtk_clist_new (2);
  gtk_widget_ref (lirc_actionlist);
  gtk_object_set_data_full (GTK_OBJECT (lirc_properties), "lirc_actionlist", lirc_actionlist,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_actionlist);
  gtk_container_add (GTK_CONTAINER (lirc_scrolledwindow), lirc_actionlist);
  gtk_clist_set_column_width (GTK_CLIST (lirc_actionlist), 0, 80);
  gtk_clist_set_column_width (GTK_CLIST (lirc_actionlist), 1, 80);
  gtk_clist_column_titles_show (GTK_CLIST (lirc_actionlist));

  lirc_label1 = gtk_label_new (_("Button"));
  gtk_widget_ref (lirc_label1);
  gtk_object_set_data_full (GTK_OBJECT (lirc_properties), "lirc_label1", lirc_label1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_label1);
  gtk_clist_set_column_widget (GTK_CLIST (lirc_actionlist), 0, lirc_label1);

  lirc_label2 = gtk_label_new (_("Action"));
  gtk_widget_ref (lirc_label2);
  gtk_object_set_data_full (GTK_OBJECT (lirc_properties), "lirc_label2", lirc_label2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_label2);
  gtk_clist_set_column_widget (GTK_CLIST (lirc_actionlist), 1, lirc_label2);

  lirc_table = gtk_table_new (2, 3, FALSE);
  gtk_widget_ref (lirc_table);
  gtk_object_set_data_full (GTK_OBJECT (lirc_properties), "lirc_table", lirc_table,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_table);
  gtk_box_pack_start (GTK_BOX (lirc_vbox), lirc_table, FALSE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (lirc_table), 5);
  gtk_table_set_row_spacings (GTK_TABLE (lirc_table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (lirc_table), 5);

  lirc_label3 = gtk_label_new (_("Button:"));
  gtk_widget_ref (lirc_label3);
  gtk_object_set_data_full (GTK_OBJECT (lirc_properties), "lirc_label3", lirc_label3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_label3);
  gtk_table_attach (GTK_TABLE (lirc_table), lirc_label3, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (lirc_label3), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (lirc_label3), 0, 0.5);

  lirc_label4 = gtk_label_new (_("Action:"));
  gtk_widget_ref (lirc_label4);
  gtk_object_set_data_full (GTK_OBJECT (lirc_properties), "lirc_label4", lirc_label4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_label4);
  gtk_table_attach (GTK_TABLE (lirc_table), lirc_label4, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (lirc_label4), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (lirc_label4), 0, 0.5);

  lirc_edit_button = gtk_entry_new ();
  gtk_widget_ref (lirc_edit_button);
  gtk_object_set_data_full (GTK_OBJECT (lirc_properties), "lirc_edit_button", lirc_edit_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_edit_button);
  gtk_table_attach (GTK_TABLE (lirc_table), lirc_edit_button, 1, 3, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  lirc_combo_action = gtk_combo_new ();
  gtk_widget_ref (lirc_combo_action);
  gtk_object_set_data_full (GTK_OBJECT (lirc_properties), "lirc_combo_action", lirc_combo_action,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_combo_action);
  gtk_table_attach (GTK_TABLE (lirc_table), lirc_combo_action, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  lirc_combo_action_items = g_list_append (lirc_combo_action_items, _("channel up"));
  lirc_combo_action_items = g_list_append (lirc_combo_action_items, _("channel down"));
  lirc_combo_action_items = g_list_append (lirc_combo_action_items, _("set channel"));
  lirc_combo_action_items = g_list_append (lirc_combo_action_items, _("power off"));
  gtk_combo_set_popdown_strings (GTK_COMBO (lirc_combo_action), lirc_combo_action_items);
  g_list_free (lirc_combo_action_items);

  lirc_combo_entry_action = GTK_COMBO (lirc_combo_action)->entry;
  gtk_widget_ref (lirc_combo_entry_action);
  gtk_object_set_data_full (GTK_OBJECT (lirc_properties), "lirc_combo_entry_action", lirc_combo_entry_action,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_combo_entry_action);
  gtk_entry_set_text (GTK_ENTRY (lirc_combo_entry_action), _("channel up"));

  lirc_combo_channel = gtk_combo_new ();
  gtk_widget_ref (lirc_combo_channel);
  gtk_object_set_data_full (GTK_OBJECT (lirc_properties), "lirc_combo_channel", lirc_combo_channel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_combo_channel);
  gtk_table_attach (GTK_TABLE (lirc_table), lirc_combo_channel, 2, 3, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_usize (lirc_combo_channel, 60, -2);

  for (i=1; i<=99; i++) {
    number = (char*)malloc(3);
    sprintf(number, "%d", i);
    lirc_combo_channel_items = g_list_append (lirc_combo_channel_items, _(number));
  }
  /*lirc_combo_channel_items = g_list_append (lirc_combo_channel_items, _("1"));
  lirc_combo_channel_items = g_list_append (lirc_combo_channel_items, _("2"));
  lirc_combo_channel_items = g_list_append (lirc_combo_channel_items, _("3"));
  lirc_combo_channel_items = g_list_append (lirc_combo_channel_items, _("4"));
  lirc_combo_channel_items = g_list_append (lirc_combo_channel_items, _("5"));
  lirc_combo_channel_items = g_list_append (lirc_combo_channel_items, _("6"));
  lirc_combo_channel_items = g_list_append (lirc_combo_channel_items, _("7"));
  lirc_combo_channel_items = g_list_append (lirc_combo_channel_items, _("8"));
  lirc_combo_channel_items = g_list_append (lirc_combo_channel_items, _("9"));*/
  gtk_combo_set_popdown_strings (GTK_COMBO (lirc_combo_channel), lirc_combo_channel_items);
  g_list_free (lirc_combo_channel_items);

  lirc_combo_entry_channel = GTK_COMBO (lirc_combo_channel)->entry;
  gtk_widget_ref (lirc_combo_entry_channel);
  gtk_object_set_data_full (GTK_OBJECT (lirc_properties), "lirc_combo_entry_channel", lirc_combo_entry_channel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_combo_entry_channel);
  gtk_entry_set_text (GTK_ENTRY (lirc_combo_entry_channel), _("1"));

  lirc_fixed = gtk_fixed_new ();
  gtk_widget_ref (lirc_fixed);
  gtk_object_set_data_full (GTK_OBJECT (lirc_properties), "lirc_fixed", lirc_fixed,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_fixed);
  gtk_box_pack_start (GTK_BOX (lirc_vbox), lirc_fixed, FALSE, TRUE, 0);

  lirc_button_add = gtk_button_new_with_label (_("Add"));
  gtk_widget_ref (lirc_button_add);
  gtk_object_set_data_full (GTK_OBJECT (lirc_properties), "lirc_button_add", lirc_button_add,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_button_add);
  gtk_fixed_put (GTK_FIXED (lirc_fixed), lirc_button_add, 0, 0);
  gtk_widget_set_uposition (lirc_button_add, 0, 0);
  gtk_widget_set_usize (lirc_button_add, 72, 24);

  lirc_button_delete = gtk_button_new_with_label (_("Delete"));
  gtk_widget_ref (lirc_button_delete);
  gtk_object_set_data_full (GTK_OBJECT (lirc_properties), "lirc_button_delete", lirc_button_delete,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_button_delete);
  gtk_fixed_put (GTK_FIXED (lirc_fixed), lirc_button_delete, 80, 0);
  gtk_widget_set_uposition (lirc_button_delete, 80, 0);
  gtk_widget_set_usize (lirc_button_delete, 72, 24);

  gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(lirc_combo_action)->entry), FALSE);
  gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(lirc_combo_channel)->entry), FALSE);


  gtk_signal_connect (GTK_OBJECT (lirc_actionlist), "select_row",
                      GTK_SIGNAL_FUNC (on_lirc_actionlist_select_row),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (lirc_button_add), "clicked",
                      GTK_SIGNAL_FUNC (on_lirc_button_add_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (lirc_button_delete), "clicked",
                      GTK_SIGNAL_FUNC (on_lirc_button_delete_clicked),
                      NULL);

  return lirc_properties;
}

static
void on_lirc_actionlist_select_row (GtkCList *clist, gint row, gint column,
                                    GdkEvent *event, gpointer user_data)
{
  last_row = row;
}


static
void on_lirc_button_add_clicked (GtkButton *button, gpointer user_data)
{
  gchar buf[50];
  gchar *data[2];
  int i=0;
  action_list_item *item;
  gchar *buffer = (gchar *)malloc(50);

  data[0] = gtk_entry_get_text(GTK_ENTRY(lirc_edit_button));
  data[1] = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(lirc_combo_action)->entry));

  if (strlen(data[0]) == 0) return;

  item = (action_list_item*)malloc(sizeof(action_list_item));

  if (strcmp(data[1], "set channel") == 0) {
    strcpy(buf, data[1]);
    strcat(buf, " ");
    strcat(buf, gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(lirc_combo_channel)->entry)));
    data[1] = buf;
  }

  strncpy(item->button, data[0], 20);
  strncpy(item->action, data[1], 30);
  delete_action(item->button);
  add_action(item);

  while (1) {
    if (!gtk_clist_get_text(GTK_CLIST(lirc_actionlist), i, 0, &buffer)) break;
    if (strcmp(buffer, item->button) == 0) {
      gtk_clist_set_text(GTK_CLIST(lirc_actionlist), i, 1, item->action);
      return;
    }

    i++;
  }
  gtk_clist_append(GTK_CLIST(lirc_actionlist), data);
}

static
void on_lirc_button_delete_clicked (GtkButton *button, gpointer user_data)
{
  if (last_row != -1) {
    gchar *buffer = (gchar *)malloc(50);
    gtk_clist_get_text(GTK_CLIST(lirc_actionlist), last_row, 0, &buffer);
    delete_action(buffer);

    gtk_clist_remove(GTK_CLIST (lirc_actionlist), last_row);
    last_row = -1;
  }
}

static
void add_action(action_list_item *item)
{
  action_list_item *prev;

  item->next = NULL;
  item->prev = last_item;

  if (first_item == NULL) {
    /* list is empty */
    first_item = item;
    last_item = item;
  }
  else {
    prev = last_item;
    prev->next = item;
    last_item = item;
  }
}

static
void delete_action(gchar *button)
{
  action_list_item *item, *prev, *next;

  item = first_item;  

  while(item != NULL) {
    if(strcmp(item->button, button) == 0) {
      prev = item->prev;
      next = item->next;
      
      if (prev == NULL && next == NULL) {
	first_item = NULL;
	last_item = NULL;
      }
      else if (prev == NULL && next != NULL) { /* first item */
	first_item = next;
	next->prev = NULL;
      }
      else if (prev != NULL && next == NULL) { /* last item */
	last_item = prev;
	prev->next = NULL;
      }
      else {
	prev->next = next;
	next->prev = prev;
      }
      free(item);
      return;
    }
    item = item->next;
  }
}

static
gchar *get_action(gchar *button)
{
  action_list_item *item = first_item;
  while (item != NULL) {
    if(strcmp(item->button, button) == 0) {
      return item->action;
    }
    item = item->next;
  }
  return NULL;
}

static
void add_actions_to_list()
{
  gchar *data[2];
  action_list_item *item = first_item;

  while (item != NULL) {
    data[0] = item->button;
    data[1] = item->action;
    gtk_clist_append(GTK_CLIST(lirc_actionlist), data);
    item = item->next;
  }
}

static
void dump_list()
{
  action_list_item *item = first_item;
  while (item != NULL) {
    printf("%s: %s\n", item->button, item->action);
    item = item->next;
  }
}
