#ifndef _LIRC_H
#define _LIRC_H

#define INIT_SOCKET_FAILED 0
#define INIT_SOCKET_OK     1


typedef struct _action_list_item {
  gchar button[20];
  gchar action[30];
  void *next;
  void *prev;
} action_list_item;

static action_list_item *first_item=NULL;
static action_list_item *last_item=NULL;

static void add_action(action_list_item *item);
static void delete_action(gchar *button);
static gchar *get_action(gchar *button);
static void add_actions_to_list();
static void dump_list();

static int fd;
static struct sockaddr_un addr;
static int lirc_page;
static GtkTreePath *last_row = NULL; /* last selected row in
					property dialog */

static GtkWidget *lirc_actionlist;
static GtkWidget *lirc_edit_button;
static GtkWidget *lirc_combo_channel;
static GtkWidget *lirc_combo_action;

static GtkWidget* create_lirc_properties (GtkWidget *lirc_properties);

static void on_lirc_actionlist_cursor_changed (GtkTreeView *v,
					       gpointer user_data);

static void on_lirc_button_add_clicked(GtkButton *button, gpointer user_data);
static void on_lirc_button_delete_clicked(GtkButton *button, gpointer
					  user_data);


static void *lirc_thread(void *dummy);

static int init_socket();

#endif
