/*
 * Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
 * Copyright (C) 2002-2003 Michael H. Schimek
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "keyboard.h"
#include "remote.h"
#include "properties.h"
#include "interface.h"
#include "zconf.h"
#include "eggcellrendererkeys.h"
#include "zmisc.h"

static const gchar *
z_keyval_name			(guint			keyval)
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
z_key_name			(z_key			key)
{
  const gchar *name;

  name = z_keyval_name (gdk_keyval_to_lower (key.key));

  if (!name)
    return NULL;

  return g_strconcat ((key.mask & GDK_CONTROL_MASK) ? _("Ctrl+")  : "",
		      (key.mask & GDK_MOD1_MASK)    ? _("Alt+")	  : "",
		      (key.mask & GDK_SHIFT_MASK)   ? _("Shift+") : "",
		      name,
		      NULL);
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
z_key_from_name			(const gchar *		name)
{
  struct {
    gchar *			str;
    guint			mask;
  } modifiers[3] = {
    { N_("Ctrl+"),  GDK_CONTROL_MASK },
    { N_("Alt+"),   GDK_MOD1_MASK },
    { N_("Shift+"), GDK_SHIFT_MASK }
  };
  const gint num_modifiers = G_N_ELEMENTS (modifiers);
  z_key key;

  key.mask = 0;

  for (;;)
    {
      gint i, len;
      gchar *str;

      for (i = 0; i < num_modifiers; i++) {
	str = _(modifiers[i].str);
	len = strlen (str);

	/* XXX utf8 ? */
	if (g_ascii_strncasecmp (name, str, len) == 0)
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
 * Give an absolute path like ZCONF_DOMAIN "/foo/bar/accel_".
 **/
void
zconf_create_z_key		(z_key			key,
				 const gchar *	 	desc,
				 const gchar *		path)
{
  gchar *s;

  g_assert(path != NULL);

  s = g_strjoin (NULL, path, "key", NULL);
  zconf_create_integer ((gint) key.key, desc, s);
  g_free (s);

  if (zconf_error ())
    return;

  s = g_strconcat (path, "mask", NULL);
  zconf_create_integer ((gint) key.mask, NULL, s);
  g_free (s);
}

/**
 * zconf_set_z_key:
 * @key: 
 * @path: 
 **/
void
zconf_set_z_key			(z_key			key,
				 const gchar *		path)
{
  gchar *s;

  g_assert(path != NULL);

  s = g_strconcat (path, "key", NULL);
  zconf_set_integer ((gint) key.key, s);
  g_free (s);

  if (zconf_error())
    return;

  s = g_strconcat (path, "mask", NULL);
  zconf_set_integer ((gint) key.mask, s);
  g_free (s);
}

z_key
zconf_get_z_key			(z_key *		keyp,
				 const gchar *		path)
{
  z_key key;
  gchar *s;

  g_assert(path != NULL);

  s = g_strconcat (path, "key", NULL);
  zconf_get_integer ((gint *) &key.key, s);
  g_free (s);

  if (!zconf_error())
    {
      s = g_strconcat (path, "mask", NULL);
      zconf_get_integer ((gint *) &key.mask, s);
      g_free (s);
    }

  if (zconf_error())
    key = Z_KEY_NONE;
  else if (keyp)
    *keyp = key;

  return key;
}

/*
 *  Generic key entry dialog
 */

typedef struct {
  GtkWidget *		hbox;
  GtkWidget *		ctrl;
  GtkWidget *		alt;
  GtkWidget *		shift;
  GtkWidget *		entry;
} z_key_entry;

static inline z_key_entry *
get_z_key_entry			(GtkWidget *		hbox)
{
  z_key_entry *ke = g_object_get_data (G_OBJECT (hbox), "z_key_entry");

  g_assert (ke != NULL);

  return ke;
}

/**
 * z_key_entry_entry:
 * @hbox:
 * 
 * Returns a pointer to the text entry of the z_key_entry, to
 * connect to the "changed" signal. (We can't emit "changed" on
 * a hbox, or can we?)
 **/
GtkWidget *
z_key_entry_entry		(GtkWidget *		hbox)
{
  z_key_entry *ke = get_z_key_entry (hbox);

  return ke->entry;
}

static void
on_modifier_toggled		(GtkToggleButton *	togglebutton,
				 gpointer		user_data)
{
  z_key_entry *ke = user_data;
  gint pos = 0;

  /* Emit "changed" signal */
  gtk_editable_insert_text (GTK_EDITABLE (ke->entry), "", 0, &pos);
}

/**
 * z_key_entry_set_key:
 * @hbox: 
 * @key: 
 * 
 * Set z_key_entry @hbox to display @key. When @key is
 * GDK_VoidSymbol the key name will be blank and all modifiers off.
 **/
void
z_key_entry_set_key		(GtkWidget *		hbox,
				 z_key			key)
{
  z_key_entry *ke = get_z_key_entry (hbox);
  const gchar *name;

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
z_key_entry_get_key		(GtkWidget *		hbox)
{
  z_key_entry *ke = get_z_key_entry (hbox);
  const gchar *name;
  z_key key;

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
on_key_press			(GtkWidget *		dialog,
				 GdkEventKey *		event,
				 gpointer		user_data)
{
  const guint mask = GDK_CONTROL_MASK | GDK_MOD1_MASK | GDK_SHIFT_MASK;
  z_key_entry *ke = user_data;
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
	   so emit accept instead */
	gtk_dialog_response (GTK_DIALOG (dialog),
			     GTK_RESPONSE_ACCEPT);      

	return TRUE; /* handled */
      }

  return FALSE; /* pass on */
}

static void
on_key_table_clicked		(GtkWidget *		w,
				 gpointer		user_data)
{
  z_key_entry *ke = user_data;
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
    (_("Accelerator"), renderer,
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

#define MOD_TOGGLE(object, label)					\
  ke->object = gtk_check_button_new_with_label (_(label));		\
  gtk_box_pack_start (GTK_BOX (ke->hbox), ke->object, FALSE, FALSE, 3);	\
  gtk_widget_show (ke->object);						\
  g_signal_connect (G_OBJECT (ke->object), "toggled",			\
		    G_CALLBACK (on_modifier_toggled), ke);

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
z_key_entry_new			(void)
{
  GtkWidget *button;
  z_key_entry *ke;

  ke = g_malloc (sizeof (*ke));

  ke->hbox = gtk_hbox_new (FALSE, 0);
  g_object_set_data_full (G_OBJECT (ke->hbox), "z_key_entry", ke,
			  (GDestroyNotify) g_free);
  gtk_widget_show (ke->hbox);

  MOD_TOGGLE (ctrl,  "Ctrl");
  MOD_TOGGLE (alt,   "Alt");
  MOD_TOGGLE (shift, "Shift");

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
  gchar *			old_cmd;
} key_binding;

static key_binding *		kb_list = NULL;

static void
kb_delete			(key_binding *		kb)
{
  g_free (kb->old_cmd);
  g_free (kb->command);
  g_free (kb);
}

static void
kb_flush			(void)
{
  key_binding *k;

  while ((k = kb_list))
    {
      kb_list = k->next;
      kb_delete (k);
    }
}

static void
kb_add				(z_key			key,
				 const gchar *		command,
				 const gchar *		old_cmd)
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
      kb->old_cmd = old_cmd ? g_strdup (old_cmd) : NULL;
    }
  else
    {
      kb = g_malloc (sizeof (*kb));
      kb->next = NULL;
      kb->key = key;
      kb->command = g_strdup (command);
      kb->old_cmd = old_cmd ? g_strdup (old_cmd) : NULL;
      *kbpp = kb;
    }
}

gboolean
on_user_key_press		(GtkWidget *		widget,
				 GdkEventKey *		event,
				 gpointer		user_data)
{
  key_binding *kb;
  z_key key;

  key.key = gdk_keyval_to_lower (event->keyval);
  key.mask = event->state;

  if (0)
    fprintf (stderr, "key %x %x\n", key.key, key.mask);

  for (kb = kb_list; kb; kb = kb->next)
    if (z_key_equal (kb->key, key))
      {
	python_command (widget, kb->command);
	return TRUE; /* handled */
      }

  return FALSE; /* not for us, pass on */
}

#undef SHIFT
#undef ALT
#undef CTRL
#define SHIFT GDK_SHIFT_MASK
#define ALT GDK_MOD1_MASK
#define CTRL GDK_CONTROL_MASK

static struct {
  guint				mask;
  guint				key;
  const gchar *			command;
} default_key_bindings[] = {
  /*
   *  Zapping default key bindings.
   *
   *  'Historic' entries were used in older versions. Eventually we
   *  found Ctrl+Alt+ annoying, so new versions without qualifiers were
   *  added. 'XawTV' entries are used by the ubiquitious XawTV viewer,
   *  added for people switching to Zapping.
   *
   *  Note Ctrl+, Alt+ are reserved for Gnome shortcuts (exception
   *  Ctrl+S and +R), which are all defined in zapping.glade.
   */
  { 0,			GDK_a,		"zapping.mute()" },				/* XawTV */
  { CTRL + ALT,		GDK_c,		"zapping.toggle_mode('capture')" },		/* historic */
  { 0,			GDK_c,		"zapping.toggle_mode('capture')" },
  { SHIFT,		GDK_c,		"zapping.ttx_open_new()" },
  { 0,			GDK_e,		"zapping.channel_editor()" },			/* XawTV */
  { 0,			GDK_f,		"zapping.toggle_mode('fullscreen')" },
  { 0,			GDK_g,		"zapping.quickshot('ppm')" },			/* XawTV */
  { 0,			GDK_h,		"zapping.ttx_hold()" },
  { SHIFT,		GDK_h,		"zapping.ttx_hold()" },
  { 0,			GDK_j,		"zapping.quickshot('jpeg')" },			/* XawTV */
  { CTRL + ALT,		GDK_n,		"zapping.ttx_open_new()" },			/* historic */
  { 0,			GDK_n,		"zapping.ttx_open_new()" },
  { CTRL + ALT,		GDK_o,		"zapping.toggle_mode('preview')" },		/* historic */
  { 0,			GDK_o,		"zapping.toggle_mode('preview')" },
  { CTRL + ALT,		GDK_p,		"zapping.toggle_mode('preview')" },		/* historic */
  { CTRL,		GDK_p,		"zapping.toggle_mode('preview')" },		/* historic */
  { 0,			GDK_p,		"zapping.toggle_mode('preview')" },
  { 0,			GDK_q,		"zapping.quit()" },				/* XawTV */
  { SHIFT,		GDK_r,		"zapping.ttx_reveal()" },
  { 0,			GDK_r,		"zapping.record()" },				/* XawTV */
  { CTRL,		GDK_r,		"zapping.quickrec()" },
  { 0,			GDK_s,		"zapping.screenshot()" },
  { CTRL,		GDK_s,		"zapping.quickshot()" },
  { CTRL + ALT,		GDK_t,		"zapping.switch_mode('teletext')" },		/* historic */
  { 0,			GDK_t,		"zapping.switch_mode('teletext')" },
  { 0,			GDK_space,	"zapping.channel_up()" },			/* XawTV */
  { 0,			GDK_question,	"zapping.ttx_reveal()" },
  { 0,			GDK_plus,	"zapping.control_incr('volume',+1)" },
  { 0,			GDK_minus,	"zapping.control_incr('volume',-1)" },
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
  { 0,			GDK_KP_Add,	"zapping.control_incr('volume',+1)" },		/* XawTV */
  { SHIFT,		GDK_KP_Add,	"zapping.picture_size_cycle(+1)" },
  { 0,			GDK_KP_Subtract,"zapping.control_incr('volume',-1)" },		/* XawTV */
  { SHIFT,		GDK_KP_Subtract,"zapping.picture_size_cycle(-1)" },
  { 0,			GDK_KP_Enter,	"zapping.mute()" },				/* XawTV */
  /* conflict with keypad channel number entering
  { 0,			GDK_Escape,	"zapping.toggle_mode()" }, */
  { 0,			GDK_F5,		"zapping.control_incr('brightness',-1)" },	/* XawTV */
  { 0,			GDK_F6,		"zapping.control_incr('brightness',+1)" },	/* XawTV */
  { 0,			GDK_F7,		"zapping.control_incr('hue',-1)" },		/* XawTV */
  { 0,			GDK_F8,		"zapping.control_incr('hue',+1)" },		/* XawTV */
  { 0,			GDK_F9,		"zapping.control_incr('contrast',-1)" },	/* XawTV */
  { 0,			GDK_F10,	"zapping.control_incr('contrast',+1)" },	/* XawTV */
  { 0,			GDK_F11,	"zapping.toggle_mode('fullscreen')" },		/* historic */
  /* conflict
  { 0,			GDK_F11,	"zapping.control_incr('saturation',-1)" },*/	/* XawTV */
  { 0,			GDK_F12,	"zapping.control_incr('saturation',+1)" },	/* XawTV */
};

static const gint num_default_key_bindings = G_N_ELEMENTS (default_key_bindings);

static void
load_default_key_bindings		(void)
{
  z_key key;
  gint i;

  for (i = 0; i < num_default_key_bindings; i++)
    {
      key.key = default_key_bindings[i].key;
      key.mask = default_key_bindings[i].mask;

      kb_add (key, default_key_bindings[i].command, NULL);
    }
}

static void
load_key_bindings			(void)
{
  gchar *buffer;
  gchar *cmd_txl;
  const gchar *command;
  z_key key;
  gint i;

  kb_flush ();

  for (i = 0;; i++)
    {
      gboolean translated;

      buffer = g_strdup_printf ("/zapping/options/main/keys/%d_cmd", i);      
      command = zconf_get_string (NULL, buffer);
      g_free (buffer);

      if (command == NULL)
        {
	  if (i == 0)
	    load_default_key_bindings ();

	  break;
	}

      cmd_txl = cmd_compatibility (command);
      translated = (0 != strcmp (command, cmd_txl));

      buffer = g_strdup_printf ("/zapping/options/main/keys/%d_", i);
      zconf_get_z_key (&key, buffer);
      g_free (buffer);

      kb_add (key, cmd_txl, translated ? command : NULL);

      g_free (cmd_txl);
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
      /* Save old style command for easier switch back to pre-0.7 versions. */
      if (kb->old_cmd)
	zconf_create_string (kb->old_cmd, NULL, buffer);
      else
	zconf_create_string (kb->command, NULL, buffer);
      g_free (buffer);

      buffer = g_strdup_printf ("/zapping/options/main/keys/%d_", i);
      zconf_create_z_key (kb->key, NULL, buffer);
      g_free (buffer);
    }
}

/*
 *  Preferences
 */

enum
{
  C_COMMAND,
  C_KEY,
  C_KEY_MASK,
  C_EDITABLE,
  C_NUM
};

static GtkListStore *
create_model			(void)
{
  GtkListStore *model;
  key_binding *kb;

  model = gtk_list_store_new (C_NUM,
			      G_TYPE_STRING,
			      G_TYPE_UINT,
			      G_TYPE_UINT,
			      G_TYPE_BOOLEAN);

  for (kb = kb_list; kb; kb = kb->next)
    {
      GtkTreeIter iter;

      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter,
			  C_COMMAND, kb->command,
			  C_KEY, kb->key.key,
			  C_KEY_MASK, kb->key.mask,
			  C_EDITABLE, TRUE,
			  -1);
    }

  return model;
}

static void
on_command_edited		(GtkCellRendererText *	cell,
				 const gchar *		path_string,
				 const gchar *		new_text,
				 GtkTreeView *		tree_view)
{
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  GtkWidget *combo;

  model = gtk_tree_view_get_model (tree_view);

  path = gtk_tree_path_new_from_string (path_string);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);

  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      C_COMMAND, new_text, -1);

  combo = lookup_widget (GTK_WIDGET (tree_view), "combo1");
  gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (combo)->entry), new_text);

  z_property_item_modified (GTK_WIDGET (tree_view));
}

static gboolean
unique				(GtkTreeModel *		model,
				 GtkTreePath *		path,
				 GtkTreeIter *		iter,
				 gpointer		user_data)
{
  z_key *new_key = user_data;
  z_key key;

  gtk_tree_model_get (model, iter,
		      C_KEY, &key.key,
		      C_KEY_MASK, &key.mask,
		      -1);

  if (key.key == new_key->key
      && key.mask == new_key->mask)
    gtk_list_store_set (GTK_LIST_STORE (model), iter,
			C_KEY, 0,
			C_KEY_MASK, 0,
			-1);

  return FALSE; /* continue */
}

static void
on_accel_edited			(GtkCellRendererText *	cell,
				 const char *		path_string,
				 guint			keyval,
				 EggVirtualModifierType	mask,
				 guint			keycode,
				 GtkTreeView *		tree_view)
{
  GtkTreePath *path;
  GtkTreeModel *model;
  GtkTreeIter iter;
  z_key key;

  key.key = keyval;
  key.mask = mask;

  model = gtk_tree_view_get_model (tree_view);

  path = gtk_tree_path_new_from_string (path_string);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);

  if (keyval != 0)
    gtk_tree_model_foreach (model, unique, &key);

  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      C_KEY, key.key,
		      C_KEY_MASK, key.mask,
		      -1);

  z_property_item_modified (GTK_WIDGET (tree_view));
}

static void
accel_set_func			(GtkTreeViewColumn *	tree_column,
				 GtkCellRenderer *	cell,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter,
				 GtkTreeView *		tree_view)
{
  z_key key;

  gtk_tree_model_get (model, iter,
		      C_KEY, &key.key,
		      C_KEY_MASK, &key.mask,
		      -1);

  g_object_set (G_OBJECT (cell),
		"visible", TRUE,
		"editable", TRUE,
		"accel_key", key.key,
		"accel_mask", key.mask,
		"style", PANGO_STYLE_NORMAL,
		NULL);
}

static void
on_selection_changed		(GtkTreeSelection *	selection,
				 GtkTreeView *		tree_view)
{
  GtkTreeModel *model;
  GtkWidget *remove;
  GtkWidget *combo;
  GtkTreeIter iter;
  gboolean selected;
  gchar *action;

  model = gtk_tree_view_get_model (tree_view);
  selected = z_tree_selection_iter_first (selection, model, &iter);

  remove = lookup_widget (GTK_WIDGET (tree_view), "general-keyboard-remove");
  gtk_widget_set_sensitive (remove, selected);

  combo = lookup_widget (GTK_WIDGET (tree_view), "combo1");
  gtk_tree_model_get (model, &iter, C_COMMAND, &action, -1);
  gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (combo)->entry), action);
  g_free (action);
}

static void
on_combo_entry_changed		(GtkEditable *		editable,
				 GtkTreeView *		tree_view)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *text;

  selection = gtk_tree_view_get_selection (tree_view);
  model = gtk_tree_view_get_model (tree_view);

  if (!z_tree_selection_iter_first (selection, model, &iter))
    return;

  text = gtk_editable_get_chars (editable, 0, -1);

  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      C_COMMAND, text, -1);
  g_free (text);
}

static void
on_add_clicked			(GtkWidget *		button,
				 GtkTreeView *		tree_view)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkWidget *combo;
  const gchar *cmd;

  selection = gtk_tree_view_get_selection (tree_view);
  model = gtk_tree_view_get_model (tree_view);

  if (z_tree_selection_iter_first (selection, model, &iter))
    gtk_list_store_insert_before (GTK_LIST_STORE (model), &iter, &iter);
  else
    gtk_list_store_append (GTK_LIST_STORE (model), &iter);

  combo = lookup_widget (GTK_WIDGET (tree_view), "combo1");
  cmd = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (combo)->entry));
  if (NULL == cmd)
    cmd = "";

  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      C_COMMAND, cmd,
		      C_KEY, 0,
		      C_KEY_MASK, 0,
		      C_EDITABLE, TRUE,
		      -1);

  gtk_tree_selection_unselect_all (selection);
  gtk_tree_selection_select_iter (selection, &iter);

  if ((path = gtk_tree_model_get_path (model, &iter)))
    {
      gtk_tree_view_set_cursor (tree_view, path,
				gtk_tree_view_get_column (tree_view, 0),
				/* start_editing */ TRUE);

      gtk_widget_grab_focus (GTK_WIDGET (tree_view));

      gtk_tree_path_free (path);
    }
}

static void
on_remove_clicked		(GtkWidget *		button,
				 GtkTreeView *		tree_view)
{
  z_tree_view_remove_selected (tree_view,
			       gtk_tree_view_get_selection (tree_view),
			       gtk_tree_view_get_model (tree_view));
}

static void
apply				(GtkWidget *		page)
{
  GtkTreeView *tree_view;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean valid;

  tree_view = GTK_TREE_VIEW (lookup_widget (page, "general-keyboard-treeview"));
  model = gtk_tree_view_get_model (tree_view);

  kb_flush ();

  valid = gtk_tree_model_get_iter_first (model, &iter);

  while (valid)
    {
      gchar *cmd;
      z_key key;

      gtk_tree_model_get (model, &iter,
			  C_COMMAND, &cmd,
			  C_KEY, &key.key,
			  C_KEY_MASK, &key.mask,
			  -1);

      kb_add (key, cmd, NULL);

      g_free (cmd);

      valid = gtk_tree_model_iter_next (model, &iter);
    }
}

static void
setup				(GtkWidget *		page)
{
  GtkTreeView *tree_view;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkWidget *widget;

  widget = lookup_widget (page, "general-keyboard-treeview");
  tree_view = GTK_TREE_VIEW (widget);
  gtk_tree_view_set_rules_hint (tree_view, TRUE);
  gtk_tree_view_set_reorderable (tree_view, TRUE);

  {
    GtkTreeSelection *selection;

    selection = gtk_tree_view_get_selection (tree_view);
    gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

    g_signal_connect (G_OBJECT (selection), "changed",
		      G_CALLBACK (on_selection_changed), tree_view);
  }

  {
    GtkListStore *model;

    model = create_model ();
    gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (model));
  }

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes
    (_("Command"), renderer,
     "text", C_COMMAND,
     "editable", C_EDITABLE,
     NULL);
  gtk_tree_view_append_column (tree_view, column);
  g_signal_connect (G_OBJECT (renderer), "edited",
		    G_CALLBACK (on_command_edited), tree_view);

  renderer = (GtkCellRenderer *) g_object_new
    (EGG_TYPE_CELL_RENDERER_KEYS,
     "editable", TRUE,
     "accel_mode", EGG_CELL_RENDERER_KEYS_MODE_X,
     NULL);
  column = gtk_tree_view_column_new_with_attributes
    (_("Shortcut"), renderer, NULL);
  gtk_tree_view_column_set_cell_data_func
    (column, renderer, (GtkTreeCellDataFunc) accel_set_func, NULL, NULL);
  gtk_tree_view_append_column (tree_view, column);
  g_signal_connect (G_OBJECT (renderer), "keys_edited",
		    G_CALLBACK (on_accel_edited), tree_view);

  widget = lookup_widget (GTK_WIDGET (tree_view), "combo1");
  gtk_combo_set_popdown_strings (GTK_COMBO (widget), cmd_list ());
  g_signal_connect (G_OBJECT (GTK_COMBO (widget)->entry), "changed",
		    G_CALLBACK (on_combo_entry_changed), tree_view);

  widget = lookup_widget (page, "general-keyboard-add");
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (on_add_clicked), tree_view);

  widget = lookup_widget (page, "general-keyboard-remove");
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (on_remove_clicked), tree_view);
}

static void
add				(GtkDialog *		dialog)
{
  SidebarEntry general_options[] = {
    { N_("Keyboard"), "gnome-keyboard.png",
      "general-keyboard-table", setup, apply,
      .help_link_id = "zapping-settings-keyboard" }
  };
  SidebarGroup groups[] = {
    { N_("General Options"), general_options, G_N_ELEMENTS (general_options) }
  };

  standard_properties_add (dialog, groups, G_N_ELEMENTS (groups),
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
