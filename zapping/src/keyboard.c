/*
 * Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
 * Copyright (C) 2002 Michael H. Schimek
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "keyboard.h"
#include "remote.h"
#include "properties.h"
#include "interface.h"
#include "zconf.h"

static gchar *
z_keyval_name				(guint keyval)
{
  switch (keyval)
    {
    /* These are synonyms, let's favour the versions more
       readily understandable by the user. */
    case GDK_Prior:
      return "Page_Up";

    case GDK_Next:
      return "Page_Down";

    case GDK_KP_Prior:
      return "KP_Page_Up";

    case GDK_KP_Next:
      return "KP_Page_Down";

    default:
      return gdk_keyval_name (keyval);
    }
}

/**
 * z_key_name:
 * @key: 
 * 
 * Similar to gdk_keyval_name(), including key modifiers.
 * 
 * Return value: 
 * %NULL if invalid key, otherwise a string you must
 * g_free(). 
 **/
gchar *
z_key_name				(z_key key)
{
  gchar *name;

  name = z_keyval_name (gdk_keyval_to_lower (key.key));

  if (!name)
    return NULL;

  return g_strdup_printf("%s%s%s%s",
			 (key.mask & GDK_CONTROL_MASK) ? _("Ctrl+") : "",
			 (key.mask & GDK_MOD1_MASK) ? _("Alt+") : "",
			 (key.mask & GDK_SHIFT_MASK) ? _("Shift+") : "",
			 name);
}

/**
 * z_key_from_name:
 * @name: 
 * 
 * Similar to gdk_keyval_from_name(), including key modifiers.
 *
 * Return value: 
 * A z_key, representing GDK_VoidSymbol if the name is invalid.
 **/
z_key
z_key_from_name				(const gchar *name)
{
  struct {
    gchar *			str;
    guint			mask;
  } modifiers[3] = {
    { N_("Ctrl+"),	GDK_CONTROL_MASK },
    { N_("Alt+"),	GDK_MOD1_MASK },
    { N_("Shift+"),	GDK_SHIFT_MASK }
  };
  const gint num_modifiers = sizeof(modifiers) / sizeof(*modifiers);
  z_key key;

  key.mask = 0;

  for (;;)
    {
      gint i, len;
      gchar *str;

      for (i = 0; i < num_modifiers; i++) {
	str = _(modifiers[i].str);
	len = strlen (str);

	if (g_ascii_strncasecmp(name, str, len) == 0)
	  break;
      }

      if (i >= num_modifiers)
	break;

      key.mask |= modifiers[i].mask;
      name += len;
    }

  key.key = gdk_keyval_to_lower (gdk_keyval_from_name (name));

  if (key.key == GDK_VoidSymbol)
    key.mask = 0;

  return key;
}

/* Configuration */

/**
 * zconf_create_z_key:
 * @key: 
 * @desc: 
 * @path: 
 * 
 * Similar to other zconf_create_.. functions for z_key types.
 * Give a unique path like ZCONF_DOMAIN "/foo/bar/accel".
 **/
void
zconf_create_z_key			(z_key		key,
					 const gchar * 	desc,
					 const gchar *	path)
{
  gchar *s;

  g_assert(path != NULL);

  s = g_strjoin (NULL, path, "_key", NULL);
  zconf_create_integer ((gint) key.key, desc, s);
  g_free (s);

  if (zconf_error ())
    return;

  s = g_strjoin (NULL, path, "_mask", NULL);
  zconf_create_integer ((gint) key.mask, NULL, s);
  g_free (s);
}

/**
 * zconf_set_z_key:
 * @key: 
 * @path: 
 **/
void
zconf_set_z_key				(z_key		key,
					 const gchar *	path)
{
  gchar *s;

  g_assert(path != NULL);

  s = g_strjoin (NULL, path, "_key", NULL);
  zconf_set_integer ((gint) key.key, s);
  g_free (s);

  if (zconf_error())
    return;

  s = g_strjoin (NULL, path, "_mask", NULL);
  zconf_set_integer ((gint) key.mask, s);
  g_free (s);
}

z_key
zconf_get_z_key				(z_key *	keyp,
					 const gchar *	path)
{
  z_key key;
  gchar *s;

  g_assert(path != NULL);

  s = g_strjoin (NULL, path, "_key", NULL);
  zconf_get_integer ((gint *) &key.key, s);
  g_free (s);

  if (!zconf_error())
    {
      s = g_strjoin (NULL, path, "_mask", NULL);
      zconf_get_integer ((gint *) &key.mask, s);
      g_free (s);
    }

  if (zconf_error())
    {
      key.key = GDK_VoidSymbol;
      key.mask = 0;
    }
  else if (keyp)
    *keyp = key;

  return key;
}

/* Key entry dialog */

struct z_key_entry {
  GtkWidget *			hbox;
  GtkWidget *			ctrl;
  GtkWidget *			alt;
  GtkWidget *			shift;
  GtkWidget *			entry;
};

/**
 * z_key_entry_set_key:
 * @hbox: 
 * @key: 
 * 
 * Set z_key_entry @hbox to display @key. When @key is
 * GDK_VoidSymbol the key name will be blank and all modifiers off.
 **/
void
z_key_entry_set_key			(GtkWidget	*hbox,
					 z_key		key)
{
  struct z_key_entry *ke;
  gchar *name;

  ke = g_object_get_data (G_OBJECT (hbox), "z_key_entry");

  g_assert(ke != NULL);

  name = z_keyval_name (gdk_keyval_to_lower (key.key));

  gtk_entry_set_text (GTK_ENTRY (ke->entry), name ? name : "");

  if (!name)
    key.mask = 0;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ke->ctrl),
			       !!(key.mask & GDK_CONTROL_MASK));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ke->alt),
			       !!(key.mask & GDK_MOD1_MASK));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ke->shift),
			       !!(key.mask & GDK_SHIFT_MASK));
}

/**
 * z_key_entry_get_key:
 * @hbox: 
 * 
 * Returns the key entered into the z_key_entry @hbox. When
 * the key is invalid (e. g. blank) the result will be
 * Z_KEY_VOID, that is key = GDK_VoidSymbol and mask = 0.
 * 
 * Return value: 
 * A z_key.
 **/
z_key
z_key_entry_get_key			(GtkWidget	*hbox)
{
  struct z_key_entry *ke;
  const gchar *name;
  z_key key;

  ke = g_object_get_data (G_OBJECT (hbox), "z_key_entry");

  g_assert(ke != NULL);

  name = gtk_entry_get_text (GTK_ENTRY (ke->entry));
  key.key = gdk_keyval_from_name (name);
  key.mask = 0;

  if (key.key != GDK_VoidSymbol)
    {
      key.mask += gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ke->ctrl)) ?
	GDK_CONTROL_MASK : 0;
      key.mask += gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ke->alt)) ?
	GDK_MOD1_MASK : 0;
      key.mask += gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ke->shift)) ?
	GDK_SHIFT_MASK : 0;
    }

  return key;
}

#include "keysyms.h"

static gboolean
on_key_press				(GtkWidget *	dialog,
					 GdkEventKey *	event,
					 gpointer	user_data)
{
  const guint mask = GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SHIFT_MASK;
  struct z_key_entry *ke = user_data;
  GtkWidget *widget;

  widget = lookup_widget (dialog, "togglebutton1");
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)))
    switch (event->keyval)
      {
	z_key key;

      case GDK_Shift_L:
      case GDK_Shift_R:
      case GDK_Control_L:
      case GDK_Control_R:
      case GDK_Caps_Lock:
      case GDK_Shift_Lock:
      case GDK_Meta_L:
      case GDK_Meta_R:
      case GDK_Alt_L:
      case GDK_Alt_R:
      case GDK_Super_L:
      case GDK_Super_R:
      case GDK_Hyper_L:
      case GDK_Hyper_R:
      case GDK_Mode_switch:
      case GDK_Multi_key:
	break;

      default:
	key.key = gdk_keyval_to_lower (event->keyval);
	key.mask = event->state & mask;

	z_key_entry_set_key (ke->hbox, key);

	/* OK means that we want to use the currently selected row,
	   so emit accept instead*/
	gtk_dialog_response (GTK_DIALOG (dialog),
			     GTK_RESPONSE_ACCEPT);      

	return TRUE;
      }

  return FALSE;
}

static void
on_key_table_clicked			(GtkWidget *	w,
					 gpointer	user_data)
{
  struct z_key_entry *ke = user_data;
  GtkWidget *dialog = build_widget("choose_key", NULL);
  GtkTreeView *key_view =
    GTK_TREE_VIEW(lookup_widget(dialog, "key_view"));
  const gchar *name;
  gint i;
  GtkListStore *store;
  GtkTreeIter iter;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkTreePath *path;
  GtkTreeSelection *sel;

  name = gtk_entry_get_text (GTK_ENTRY (ke->entry));

  store = gtk_list_store_new (1, G_TYPE_STRING);

  for (i = 0, path=NULL; i < num_keysyms; i++)
    {
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, keysyms[i],
			  -1);
      if (name && !strcasecmp (name, keysyms[i]))
	path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
    }

  gtk_tree_view_set_model (key_view, GTK_TREE_MODEL (store));

  /* Set browse mode for selection */
  /* Borrowed reference, no need to unref. The docs could mention
     this, of course ... but that would be too easy :-) */
  sel = gtk_tree_view_get_selection (key_view);
  gtk_tree_selection_set_mode (sel, GTK_SELECTION_BROWSE);

  /* Append our single column */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes
    (_("Key"), renderer,
     /* Render the 0th entry in the model */ "text", 0, NULL);
  gtk_tree_view_append_column (key_view, column);
  gtk_tree_view_set_search_column (key_view, 0);

  /* Select the previous option */
  if (path)
    {
      gtk_tree_view_set_cursor (key_view, path, NULL, FALSE);
      /* FIXME: doesn't work, prolly a gtk bug */
      gtk_tree_view_scroll_to_cell (key_view, path, NULL, TRUE, 0.0, 0.5);
      gtk_tree_path_free (path);
    }

  gtk_widget_grab_focus (GTK_WIDGET (key_view));

  g_signal_connect (G_OBJECT (dialog), "key_press_event",
		    G_CALLBACK (on_key_press), ke);

  while (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
      /* Returns the selected row in path */
     gtk_tree_view_get_cursor (key_view, &path, NULL);

      if (path)
	{
	  gchar *buf;
	  g_assert(gtk_tree_model_get_iter (GTK_TREE_MODEL (store),
					    &iter, path) == TRUE);
	  gtk_tree_model_get (GTK_TREE_MODEL (store), &iter,
			      0, &buf, -1);
	  gtk_entry_set_text (GTK_ENTRY (ke->entry), buf);
	  g_free(buf);
	  gtk_tree_path_free (path);
	}
    }

  /* We owned a reference */
  g_object_unref (G_OBJECT (store));

  gtk_widget_destroy(dialog);
}

/**
 * z_key_entry_new:
 * 
 * Creates a "key entry" widget. (Used as Glade custom
 * widget in zapping.glade.) The initial state is the same
 * as after z_key_entry_set_key (Z_KEY_VOID), i. e. blank.
 * 
 * Return value: 
 * GtkWidget pointer, gtk_destroy() as usual.
 **/
GtkWidget *
z_key_entry_new				(void)
{
  GtkWidget *button;
  struct z_key_entry *ke;

  ke = g_malloc (sizeof (*ke));

  ke->hbox = gtk_hbox_new (FALSE, 0);
  g_object_set_data_full (G_OBJECT (ke->hbox), "z_key_entry", ke,
			  (GDestroyNotify) g_free);
  gtk_widget_show (ke->hbox);

  ke->ctrl = gtk_check_button_new_with_label (_("Ctrl"));
  gtk_box_pack_start (GTK_BOX (ke->hbox), ke->ctrl, FALSE, FALSE, 3);
  gtk_widget_show (ke->ctrl);

  ke->alt = gtk_check_button_new_with_label (_("Alt"));
  gtk_box_pack_start (GTK_BOX (ke->hbox), ke->alt, FALSE, FALSE, 3);
  gtk_widget_show (ke->alt);

  ke->shift = gtk_check_button_new_with_label (_("Shift"));
  gtk_box_pack_start (GTK_BOX (ke->hbox), ke->shift, FALSE, FALSE, 3);
  gtk_widget_show (ke->shift);

  ke->entry = gtk_entry_new();
  gtk_box_pack_start (GTK_BOX (ke->hbox), ke->entry, TRUE, TRUE, 3);
  gtk_widget_show (ke->entry);

  button = gtk_button_new_with_label (_("Key table..."));
  gtk_box_pack_start (GTK_BOX (ke->hbox), button, FALSE, FALSE, 3);
  g_signal_connect (G_OBJECT (button), "clicked",
		    G_CALLBACK (on_key_table_clicked), ke);
  gtk_widget_show (button);

  return ke->hbox;
}

/*
 *  Keyboard command bindings
 */

typedef struct key_binding {
  struct key_binding *		next;
  z_key				key;
  gchar *			command;
} key_binding;

static key_binding *		kb_list = NULL;

static void
kb_delete				(key_binding *	kb)
{
  g_free (kb->command);
  g_free (kb);
}

static void
kb_flush				(void)
{
  key_binding *k;

  while ((k = kb_list))
    {
      kb_list = k->next;
      kb_delete (k);
    }
}

static void
kb_add					(z_key		key,
					 const gchar *	command)
{
  key_binding *kb, **kbpp;

  if (key.key == GDK_VoidSymbol || !command || !command[0])
    return;

  for (kbpp = &kb_list; (kb = *kbpp); kbpp = &kb->next)
    if (z_key_equal (kb->key, key))
      break;

  if (kb)
    {
      g_free (kb->command);
      kb->command = g_strdup (command);
    }
  else
    {
      kb = g_malloc (sizeof (*kb));
      kb->next = NULL;
      kb->key = key;
      kb->command = g_strdup (command);
      *kbpp = kb;
    }
}

gboolean
on_user_key_press			(GtkWidget *	widget,
					 GdkEventKey *	event,
					 gpointer	user_data)
{
  key_binding *kb;
  z_key key;

  key.key = gdk_keyval_to_lower (event->keyval);
  key.mask = event->state;

  /* fprintf(stderr, "key %x %x\n", key.key, key.mask); */

  for (kb = kb_list; kb; kb = kb->next)
    if (z_key_equal (kb->key, key))
      {
	cmd_run (kb->command);
	return TRUE;
      }

  return FALSE; /* not for us, pass on */
}

#define SHIFT GDK_SHIFT_MASK
#define ALT GDK_MOD1_MASK
#define CTRL GDK_CONTROL_MASK

static struct {
  guint				mask;
  guint				key;
  const gchar *			command;
} default_key_bindings[] = {
  /*
   *  Historic Zapping key bindings. Note Ctrl+ is reserved
   *  for Gnome shortcuts (exception Ctrl+S and +R), which are
   *  all defined in zapping.glade.
   */
  { 0,			GDK_a,		"zapping.mute()" }, /* XawTV */
  { CTRL + ALT,		GDK_c,		"zapping.toggle_mode('capture')" },
  { 0,			GDK_c,		"zapping.toggle_mode('capture')" }, /* new */
  { SHIFT,		GDK_c,		"zapping.ttx_open_new()" },
  { 0,			GDK_f,		"zapping.toggle_mode('fullscreen')" }, /* new */
  { 0,			GDK_g,		"zapping.quickshot('ppm')" }, /* XawTV */
  { 0,			GDK_h,		"zapping.ttx_hold()" },
  { SHIFT,		GDK_h,		"zapping.ttx_hold()" },
  { 0,			GDK_j,		"zapping.quickshot('jpeg')" }, /* XawTV */
  { CTRL + ALT,		GDK_n,		"zapping.ttx_open_new()" },
  { 0,			GDK_n,		"zapping.ttx_open_new()" }, /* new */
  { CTRL + ALT,		GDK_o,		"zapping.toggle_mode('preview')" },
  { 0,			GDK_o,		"zapping.toggle_mode('preview')" },
  { CTRL + ALT,		GDK_p,		"zapping.toggle_mode('preview')" },
  { CTRL,		GDK_p,		"zapping.toggle_mode('preview')" },
  { 0,			GDK_p,		"zapping.toggle_mode('preview')" }, /* new */
  { 0,			GDK_q,		"zapping.quit()" }, /* XawTV */
  { SHIFT,		GDK_r,		"zapping.ttx_reveal()" },
  { 0,			GDK_r,		"zapping.record()" }, /* XawTV */
  { CTRL,		GDK_r,		"zapping.quickrec()" },
  { 0,			GDK_s,		"zapping.screenshot()" },
  { CTRL,		GDK_s,		"zapping.quickshot()" },
  { CTRL + ALT,		GDK_t,		"zapping.switch_mode('teletext')" },
  { 0,			GDK_t,		"zapping.switch_mode('teletext')" }, /* new */
  { 0,			GDK_space,	"zapping.channel_up()" }, /* XawTV */
  { 0,			GDK_question,	"zapping.ttx_reveal()" },
  { 0,			GDK_plus,	"zapping.volume_incr(+1)" },
  { 0,			GDK_minus,	"zapping.volume_incr(-1)" },
  { 0,			GDK_Page_Up,	"zapping.channel_up()" },
  { 0,			GDK_KP_Page_Up,	"zapping.channel_up()" },
  { 0,			GDK_Page_Down,	"zapping.channel_down()" },
  { 0,			GDK_KP_Page_Down,"zapping.channel_down()" },
  { 0,			GDK_Home,	"zapping.ttx_home()" },
  { 0,			GDK_KP_Home,	"zapping.ttx_home()" },
  { 0,			GDK_Up,		"zapping.ttx_page_incr(+1)" },
  { 0,			GDK_KP_Up,	"zapping.ttx_page_incr(+1)" },
  { 0,			GDK_Down,	"zapping.ttx_page_incr(-1)" },
  { 0,			GDK_KP_Down,	"zapping.ttx_page_incr(-1)" },
  { SHIFT,		GDK_Up,		"zapping.ttx_page_incr(+10)" },
  { SHIFT,		GDK_KP_Up,	"zapping.ttx_page_incr(+10)" },
  { SHIFT,		GDK_Down,	"zapping.ttx_page_incr(-10)" },
  { SHIFT,		GDK_KP_Down,	"zapping.ttx_page_incr(-10)" },
  { 0,			GDK_Left,	"zapping.ttx_subpage_incr(-1)" },
  { 0,			GDK_KP_Left,	"zapping.ttx_subpage_incr(-1)" },
  { 0,			GDK_Right,	"zapping.ttx_subpage_incr(+1)" },
  { 0,			GDK_KP_Right,	"zapping.ttx_subpage_incr(+1)" },
  { 0,			GDK_KP_Add,	"zapping.ttx_subpage_incr(+1)" },
  { 0,			GDK_KP_Subtract,"zapping.ttx_subpage_incr(-1)" },
  { 0,			GDK_Escape,	"zapping.toggle_mode()" },
  { 0,			GDK_F11,	"zapping.toggle_mode('fullscreen')" },
};
static const gint num_default_key_bindings =
  sizeof (default_key_bindings) / sizeof (default_key_bindings[0]);

static void
load_default_key_bindings		(void)
{
  z_key key;
  gint i;

  for (i = 0; i < num_default_key_bindings; i++)
    {
      key.key = default_key_bindings[i].key;
      key.mask = default_key_bindings[i].mask;

      kb_add (key, default_key_bindings[i].command);
    }
}

static void
load_key_bindings			(void)
{
  gchar *buffer;
  gchar *command;
  z_key key;
  gint i;

  kb_flush ();

  for (i = 0;; i++)
    {
      buffer = g_strdup_printf ("/zapping/options/main/keys/%d_cmd", i);
      command = zconf_get_string (NULL, buffer);
      g_free (buffer);

      if (command == NULL)
        {
	  if (i == 0)
	    load_default_key_bindings ();

	  break;
	}

      buffer = g_strdup_printf ("/zapping/options/main/keys/%d", i);
      zconf_get_z_key (&key, buffer);
      g_free (buffer);

      kb_add (key, command);
    }
}

static void
save_key_bindings			(void)
{
  key_binding *kb;
  int i;

  zconf_delete("/zapping/options/main/keys");

  for (kb = kb_list, i = 0; kb; kb = kb->next, i++)
    {
      gchar *buffer;

      buffer = g_strdup_printf ("/zapping/options/main/keys/%d_cmd", i);
      zconf_create_string (kb->command, NULL, buffer);
      g_free (buffer);

      buffer = g_strdup_printf ("/zapping/options/main/keys/%d", i);
      zconf_create_z_key (kb->key, NULL, buffer);
      g_free (buffer);
    }
}

/*
 *  Preferences
 */
enum {
  KEY_NAME_COLUMN,
  KEY_ACTION_COLUMN,
  NUM_COLUMNS
};

static void
on_add_clicked				(GtkWidget *	button,
					 gpointer      	user_data)
{
  GtkWidget *key_entry = lookup_widget (button, "custom2");
  GtkWidget *combo = lookup_widget (button, "combo1");
  GtkTreeView *keyboard_commands = GTK_TREE_VIEW
    (lookup_widget (button, "keyboard_commands"));
  gchar *key_name;
  const gchar *cmd;
  GtkTreeIter iter;
  GtkTreeSelection *sel = gtk_tree_view_get_selection (keyboard_commands);
  GtkTreeModel *model;

  key_name = z_key_name (z_key_entry_get_key (key_entry));
  cmd = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (combo)->entry));

  if (!key_name || !cmd || *cmd == 0)
    goto finish;

  /* Only works in single or browse mode */
  if (gtk_tree_selection_get_selected (sel, &model, &iter))
    {
      gchar *buf;

      gtk_tree_model_get (model, &iter, KEY_NAME_COLUMN, &buf, -1);
      if (!strcmp (buf, key_name))
	{
	  /* assume we want to modify instead of add */
	  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			      KEY_ACTION_COLUMN, cmd, -1);
	  g_free(buf);
	  goto finish;
	}
      g_free (buf);
    }

  /* Assume we want to add things */
  gtk_list_store_append (GTK_LIST_STORE (model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      KEY_NAME_COLUMN, key_name,
		      KEY_ACTION_COLUMN, cmd,
		      -1);

 finish:
  g_free (key_name);
}

static void
on_delete_clicked			(GtkWidget *	button,
					 gpointer	user_data)
{
  GtkTreeView *keyboard_commands = GTK_TREE_VIEW
    (lookup_widget (button, "keyboard_commands"));
  GtkTreeModel *model;
  GtkTreeSelection *sel = gtk_tree_view_get_selection (keyboard_commands);
  GtkTreeIter iter;

  /* We are in single or browse mode, only one item selected at a time
   */
  if (gtk_tree_selection_get_selected (sel, &model, &iter))
    gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static void
on_keyboard_commands_cursor_changed	(GtkTreeView	*v,
					 gpointer	user_data)
{
  GtkWidget *key_entry = lookup_widget (GTK_WIDGET (v), "custom2");
  GtkWidget *combo = lookup_widget (GTK_WIDGET (v), "combo1");
  gchar *text;
  GtkTreeIter iter;
  GtkTreeModel *model = gtk_tree_view_get_model (v);
  GtkTreePath *path;

  gtk_tree_view_get_cursor (v, &path, NULL);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);

  gtk_tree_model_get (model, &iter, KEY_NAME_COLUMN, &text, -1);
  z_key_entry_set_key (key_entry, z_key_from_name (text));
  g_free (text);

  gtk_tree_model_get (model, &iter, KEY_ACTION_COLUMN, &text, -1);
  gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (combo)->entry), text);
  g_free (text);
}

static void
setup					(GtkWidget *	page)
{
  GtkTreeView *keyboard_commands = GTK_TREE_VIEW
    (lookup_widget (page, "keyboard_commands"));
  GtkWidget *combo = lookup_widget (page, "combo1");
  GtkWidget *add = lookup_widget (page, "button41");
  GtkWidget *delete = lookup_widget (page, "button43");
  key_binding *kb;
  GtkListStore *model;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkTreeSelection *sel = gtk_tree_view_get_selection (keyboard_commands);

  /* Create our model */
  model = gtk_list_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

  for (kb = kb_list; kb; kb = kb->next)
    {
      GtkTreeIter iter;
      gchar *buffer = z_key_name (kb->key);

      if (!buffer)
	continue;

      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter,
			  KEY_NAME_COLUMN, buffer,
			  KEY_ACTION_COLUMN, kb->command,
			  -1);

      g_free (buffer);
    }

  /* Set our model for the treeview and drop our reference */
  gtk_tree_view_set_model (keyboard_commands, GTK_TREE_MODEL (model));

  /* Set browse mode for the keys list */
  gtk_tree_selection_set_mode (sel, GTK_SELECTION_BROWSE);

  /* Define the view for the model. Two columns, first the key and
     then the action */
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes
    (_("Key"), renderer, "text", KEY_NAME_COLUMN, NULL);
  gtk_tree_view_append_column (keyboard_commands, column);  

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes
    (_("Action"), renderer, "text", KEY_ACTION_COLUMN, NULL);
  gtk_tree_view_append_column (keyboard_commands, column);

  gtk_combo_set_popdown_strings (GTK_COMBO (combo), cmd_list());

  g_signal_connect (G_OBJECT (add), "clicked",
		    G_CALLBACK (on_add_clicked),
		    NULL);

  g_signal_connect (G_OBJECT (delete), "clicked",
		    G_CALLBACK (on_delete_clicked),
		    NULL);

  g_signal_connect (G_OBJECT (keyboard_commands), "cursor-changed",
		    G_CALLBACK (on_keyboard_commands_cursor_changed),
		    NULL);
}

static void
apply					(GtkWidget *	page)
{
  GtkTreeView *keyboard_commands = GTK_TREE_VIEW
    (lookup_widget (page, "keyboard_commands"));
  GtkTreeModel *model = gtk_tree_view_get_model (keyboard_commands);
  GtkTreeIter iter;
  gboolean valid;
  gchar *key, *cmd;

  kb_flush ();

  valid = gtk_tree_model_get_iter_first (model, &iter);

  while (valid)
    {
      gtk_tree_model_get (model, &iter,
			  KEY_NAME_COLUMN, &key,
			  KEY_ACTION_COLUMN, &cmd,
			  -1);
      kb_add (z_key_from_name (key), cmd);
      g_free (key);
      g_free (cmd);

      valid = gtk_tree_model_iter_next (model, &iter);
    }
}

static void
add				(GtkDialog *		dialog)
{
  SidebarEntry general_options[] = {
    { N_("Keyboard"), "gnome-keyboard.png", "table75",
      setup, apply }
  };
  SidebarGroup groups[] = {
    { N_("General Options"), general_options, acount (general_options) }
  };

  standard_properties_add (dialog, groups, acount (groups),
			   "zapping.glade2");
}

void
shutdown_keyboard (void)
{
  save_key_bindings ();

  kb_flush ();
}

void
startup_keyboard (void)
{
  property_handler keyb_handler = {
    add: add
  };

  load_key_bindings ();

  prepend_property_handler (&keyb_handler);
}
