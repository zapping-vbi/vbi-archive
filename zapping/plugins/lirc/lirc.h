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

void add_action(action_list_item *item);
void delete_action(gchar *button);
gchar *get_action(gchar *button);
void add_actions_to_list();
void dump_list();

static int fd;
static struct sockaddr_un addr;
static int lirc_page;
static int last_row = -1; /* last selected row in property dialog */

GtkWidget *lirc_actionlist;
GtkWidget *lirc_edit_button;
GtkWidget *lirc_combo_channel;
GtkWidget *lirc_combo_action;

GtkWidget* create_lirc_properties (GtkWidget *lirc_properties);

void on_lirc_actionlist_select_row(GtkCList *clist, gint row, gint column, 
				   GdkEvent *event, gpointer user_data);

void on_lirc_button_add_clicked(GtkButton *button, gpointer user_data);
void on_lirc_button_delete_clicked(GtkButton *button, gpointer user_data);


void set_channel(int c);

void *lirc_thread(void *dummy);

int init_socket();

#endif
