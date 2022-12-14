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

/* XXX gtk+ 2.3 GtkCombo -> GtkComboBox */
#undef GTK_DISABLE_DEPRECATED

#include "src/plugin_common.h"

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

#include "src/callbacks.h"
#include "src/tveng.h"
#include "src/remote.h"
#include "lirc.h"
#include "src/properties.h"

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

enum {
  BUTTON_COLUMN,
  ACTION_COLUMN
};

/* Properties handling code */
static void
properties_add			(GtkDialog	*dialog);

static
int init_socket(void)
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
void add_actions_to_list(void)
{
  action_list_item *item = first_item;
  GtkTreeIter iter;
  GtkListStore *model = GTK_LIST_STORE (gtk_tree_view_get_model
					(GTK_TREE_VIEW (lirc_actionlist)));

  while (item != NULL) {
    gtk_list_store_append (model, &iter);
    gtk_list_store_set (model, &iter,
			BUTTON_COLUMN, item->button,
			ACTION_COLUMN, item->action,
			-1);

    item = item->next;
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
  /* Register the plugin as interested in the properties dialog */
  property_handler lirc_handler =
  {
    add: properties_add
  };

  append_property_handler(&lirc_handler);

  printv("lirc plugin: init\n");

  /* If this is set, autostarting is on (we should start now) */
  if (active)
    plugin_start();

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
  const gchar *button;
  gchar *action;
  int i=0;
  action_list_item *item;

  printv("lirc plugin: loading configuration\n");

  /* The autostart config value is compulsory, you shouldn't need to
     change the following */
  buffer = g_strconcat(root_key, "autostart", NULL);
  /* Create sets a default value for a key, check src/zconf.h */
  zconf_create_boolean(FALSE,"Whether the plugin should start"
		       " automatically when opening Zapping", buffer);
  active = zconf_get_boolean(NULL, buffer);
  g_free(buffer);

  while(1) {
    buffer = g_strdup_printf("%sactions/action_%d", root_key, ++i);
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

  printv("\n");

  /* Load here any other config key */
}

static
void plugin_save_config (gchar * root_key)
{
  gchar *buffer;
  action_list_item *item;
  int i=0;

  /* This one is compulsory, you won't need to change it */
  buffer = g_strconcat(root_key, "autostart", NULL);
  zconf_set_boolean(active, buffer);

  buffer = g_strconcat(root_key, "actions", NULL);
  zconf_delete(buffer);
  g_free(buffer);

  item = first_item;
  while (item != NULL) {
    buffer = g_strdup_printf("%sactions/action_%d", root_key, ++i);
    zconf_create_string(item->button, item->action, buffer);
    item = item->next;
    g_free(buffer);
  }
}

/* mostly cut'n'paste from properties.c */
static void
custom_properties_add		(GtkDialog	*dialog,
				 SidebarGroup	*groups,
				 gint		num_groups)
{
  gint i, j;
  GtkWidget *vbox;

  for (i = 0; i<num_groups; i++)
    {
      append_properties_group(dialog, groups[i].label, _(groups[i].label));

      for (j = 0; j<groups[i].num_items; j++)
	{
	  GtkWidget *pixmap;
	  GtkWidget *page = gtk_vbox_new(FALSE, 15);

	  pixmap = z_load_pixmap (groups[i].items[j].icon_name);

	  g_object_set_data(G_OBJECT(page), "apply",
			      groups[i].items[j].apply);
	  g_object_set_data(G_OBJECT(page), "help",
			      groups[i].items[j].help);

	  append_properties_page(dialog, /* no i18n */ groups[i].label,
				 _(groups[i].items[j].label),
				 pixmap, page);

	  create_lirc_properties(page);
	  add_actions_to_list();
	}
    }
}

static void
on_lirc_apply			(GtkWidget	*widget)
{
  /*
    Nothing, bro, but there's an assert in properties.c.
    The right thing to do is to switch to glade files, and use
    apply/help/etc correctly, but i'm not for it right now :-)
   */
}

static void
on_lirc_help			(GtkWidget	*widget)
{
  ShowBox("LIRC plugin version 0.1 by Marco Pfattner\nmarco.p@bigfoot.com",
	  GTK_MESSAGE_INFO);
}

static void
properties_add			(GtkDialog	*dialog)
{
  SidebarEntry plugin_options[] = {
    { N_("LIRC"), "gnome-shutdown.png" , NULL, NULL,
      on_lirc_apply, on_lirc_help }
  };
  SidebarGroup groups[] = {
    { N_("Plugins"), plugin_options, acount(plugin_options) }
  };

  custom_properties_add(dialog, groups, acount(groups));
}


static
struct plugin_misc_info * plugin_get_misc_info (void)
{
  static struct plugin_misc_info returned_struct =
  {
    sizeof(struct plugin_misc_info),
    6,
    PLUGIN_CATEGORY_DEVICE_CONTROL |
    PLUGIN_CATEGORY_GUI
  };

  /*
    Tell that the template plugin should be run with a somewhat high
    priority (just to put an example)
  */
  return (&returned_struct);
}

#warning security issues

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
    } else {
      diff = 0;
    }

    strncpy(new_button, buf2, 20);
    strncpy(check_button, buf2, 20);

    printv("lirc plugin: button %s pressed\n", buf2);
    printv("lirc plugin: time: %ld\n", new_msec);
    printv("lirc plugin: diff: %ld\n", diff);

    printv("lirc plugin: old button: %s\n", old_button);

    if (diff <= 1500) {
      strncpy(check_button, old_button, 20);
      strcat(check_button, ":");
      strcat(check_button, new_button);
      action = get_action(check_button);
      printv("lirc plugin: action for button %s: %s\n", check_button, action);
      if (action == NULL) {
	strncpy(check_button, new_button, 20);
      }
    }

    action = get_action(check_button);
    printv("lirc plugin: action for button %s: %s\n", check_button, action);

    strncpy(old_button, new_button, 20);
    
    if (action == NULL) continue;

    if (strcmp(action, "power off") == 0)
      python_command (NULL, "zapping.quit()");
    else if (strcmp(action, "channel up") == 0)
      python_command (NULL, "zapping.channel_up()");
    else if (strcmp(action, "channel down") == 0)
      python_command (NULL, "zapping.channel_down()");
    else if (strncmp(action, "set channel", strlen("set_channel")) == 0) {
      /* extract channel number */
      action += strlen ("set channel");
      python_command_printf (NULL, "zapping.set_channel(%s - 1)", action);
    }
  }

  return NULL;
}

static GtkWidget*
create_lirc_properties (GtkWidget *lirc_properties)
{
  GtkWidget *lirc_vbox;
  GtkWidget *lirc_scrolledwindow;
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
  GtkListStore *model;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  int i=0;
  char *number;

  lirc_vbox = gtk_vbox_new (FALSE, 5);
  gtk_widget_ref (lirc_vbox);
  g_object_set_data_full (G_OBJECT (lirc_properties), "lirc_vbox", lirc_vbox,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_vbox);
  gtk_container_add (GTK_CONTAINER (lirc_properties), lirc_vbox);
  gtk_container_set_border_width (GTK_CONTAINER (lirc_vbox), 5);

  lirc_scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (lirc_scrolledwindow);
  g_object_set_data_full (G_OBJECT (lirc_properties), "lirc_scrolledwindow", lirc_scrolledwindow,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_scrolledwindow);
  gtk_box_pack_start (GTK_BOX (lirc_vbox), lirc_scrolledwindow, TRUE, TRUE, 0);

  model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

  lirc_actionlist = gtk_tree_view_new_with_model (GTK_TREE_MODEL (model));
  gtk_widget_ref (lirc_actionlist);
  g_object_set_data_full (G_OBJECT (lirc_properties), "lirc_actionlist", lirc_actionlist,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_actionlist);
  gtk_container_add (GTK_CONTAINER (lirc_scrolledwindow), lirc_actionlist);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes
    (_("Button"), renderer, "text", BUTTON_COLUMN, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (lirc_actionlist), column);  

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes
    (_("Action"), renderer, "text", ACTION_COLUMN, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (lirc_actionlist), column);

  lirc_table = gtk_table_new (2, 3, FALSE);
  gtk_widget_ref (lirc_table);
  g_object_set_data_full (G_OBJECT (lirc_properties), "lirc_table", lirc_table,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_table);
  gtk_box_pack_start (GTK_BOX (lirc_vbox), lirc_table, FALSE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (lirc_table), 5);
  gtk_table_set_row_spacings (GTK_TABLE (lirc_table), 5);
  gtk_table_set_col_spacings (GTK_TABLE (lirc_table), 5);

  lirc_label3 = gtk_label_new (_("Button:"));
  gtk_widget_ref (lirc_label3);
  g_object_set_data_full (G_OBJECT (lirc_properties), "lirc_label3", lirc_label3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_label3);
  gtk_table_attach (GTK_TABLE (lirc_table), lirc_label3, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (lirc_label3), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (lirc_label3), 0, 0.5);

  lirc_label4 = gtk_label_new (_("Action:"));
  gtk_widget_ref (lirc_label4);
  g_object_set_data_full (G_OBJECT (lirc_properties), "lirc_label4", lirc_label4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_label4);
  gtk_table_attach (GTK_TABLE (lirc_table), lirc_label4, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_label_set_justify (GTK_LABEL (lirc_label4), GTK_JUSTIFY_LEFT);
  gtk_misc_set_alignment (GTK_MISC (lirc_label4), 0, 0.5);

  lirc_edit_button = gtk_entry_new ();
  gtk_widget_ref (lirc_edit_button);
  g_object_set_data_full (G_OBJECT (lirc_properties), "lirc_edit_button", lirc_edit_button,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_edit_button);
  gtk_table_attach (GTK_TABLE (lirc_table), lirc_edit_button, 1, 3, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  lirc_combo_action = gtk_combo_new ();
  gtk_widget_ref (lirc_combo_action);
  g_object_set_data_full (G_OBJECT (lirc_properties), "lirc_combo_action", lirc_combo_action,
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
  g_object_set_data_full (G_OBJECT (lirc_properties), "lirc_combo_entry_action", lirc_combo_entry_action,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_combo_entry_action);
  gtk_entry_set_text (GTK_ENTRY (lirc_combo_entry_action), _("channel up"));

  lirc_combo_channel = gtk_combo_new ();
  gtk_widget_ref (lirc_combo_channel);
  g_object_set_data_full (G_OBJECT (lirc_properties), "lirc_combo_channel", lirc_combo_channel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_combo_channel);
  gtk_table_attach (GTK_TABLE (lirc_table), lirc_combo_channel, 2, 3, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_widget_set_size_request (lirc_combo_channel, 60, -1);

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
  g_object_set_data_full (G_OBJECT (lirc_properties), "lirc_combo_entry_channel", lirc_combo_entry_channel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_combo_entry_channel);
  gtk_entry_set_text (GTK_ENTRY (lirc_combo_entry_channel), _("1"));

  lirc_fixed = gtk_fixed_new ();
  gtk_widget_ref (lirc_fixed);
  g_object_set_data_full (G_OBJECT (lirc_properties), "lirc_fixed", lirc_fixed,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_fixed);
  gtk_box_pack_start (GTK_BOX (lirc_vbox), lirc_fixed, FALSE, TRUE, 0);

  lirc_button_add = gtk_button_new_with_label (_("Add"));
  gtk_widget_ref (lirc_button_add);
  g_object_set_data_full (G_OBJECT (lirc_properties), "lirc_button_add", lirc_button_add,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_button_add);
  gtk_fixed_put (GTK_FIXED (lirc_fixed), lirc_button_add, 0, 0);
  gtk_widget_set_size_request (lirc_button_add, 72, 24);

  lirc_button_delete = gtk_button_new_with_label (_("Delete"));
  gtk_widget_ref (lirc_button_delete);
  g_object_set_data_full (G_OBJECT (lirc_properties), "lirc_button_delete", lirc_button_delete,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (lirc_button_delete);
  gtk_fixed_put (GTK_FIXED (lirc_fixed), lirc_button_delete, 80, 0);
  gtk_widget_set_size_request (lirc_button_delete, 72, 24);

  gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(lirc_combo_action)->entry), FALSE);
  gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(lirc_combo_channel)->entry), FALSE);


  g_signal_connect (G_OBJECT (lirc_actionlist), "cursor-changed",
		    G_CALLBACK (on_lirc_actionlist_cursor_changed),
		    NULL);
  g_signal_connect (G_OBJECT (lirc_button_add), "clicked",
		    G_CALLBACK (on_lirc_button_add_clicked),
		    NULL);
  g_signal_connect (G_OBJECT (lirc_button_delete), "clicked",
		    G_CALLBACK (on_lirc_button_delete_clicked),
		    NULL);

  return lirc_properties;
}

static
void on_lirc_actionlist_cursor_changed (GtkTreeView *view,
					gpointer user_data)
{
  if (last_row)
    gtk_tree_path_free (last_row);

  gtk_tree_view_get_cursor (view, &last_row, NULL);
}

static
void on_lirc_button_add_clicked (GtkButton *button, gpointer user_data)
{
  gchar buf[50];
  const gchar *data[2];
  int i=0;
  action_list_item *item;
  gchar *buffer = (gchar *)malloc(50);
  gboolean valid;
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model
    (GTK_TREE_VIEW (lirc_actionlist));

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

  valid = gtk_tree_model_get_iter_first (model, &iter);
  while (valid) {
    gtk_tree_model_get (model, &iter, BUTTON_COLUMN, &buffer, -1);

    if (strcmp(buffer, item->button) == 0) {
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			  ACTION_COLUMN, item->action, -1);
      g_free (buffer);
      return;
    }

    g_free (buffer);

    valid = gtk_tree_model_iter_next (model, &iter);
  }

  /* Not found, add */
  gtk_list_store_append (GTK_LIST_STORE (model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      ACTION_COLUMN, data[1],
		      BUTTON_COLUMN, data[0],
		      -1);
}

static
void on_lirc_button_delete_clicked (GtkButton *button, gpointer user_data)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gchar *buffer;

  if (!last_row)
    return;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (lirc_actionlist));
  gtk_tree_model_get_iter (model, &iter, last_row);
  gtk_tree_model_get (model, &iter, BUTTON_COLUMN, &buffer, -1);

  delete_action(buffer);
  g_free (buffer);

  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

  gtk_tree_path_free (last_row);
  last_row = NULL;
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
void dump_list(void)
{
  action_list_item *item = first_item;
  while (item != NULL) {
    printv("%s: %s\n", item->button, item->action);
    item = item->next;
  }
}
