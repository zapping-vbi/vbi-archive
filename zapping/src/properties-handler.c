/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2001 Iñaki García Etxebarria
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
/**
 * Properties handler for Z. Uses the shell code from properties.h,
 * you can think of this code as the "model" for the properties
 * structure, while properties.c is the "view". Buzzwords, ya know :-)
 */

/* XXX gtk+ 2.3 GtkOptionMenu, Gnome entry */
#undef GTK_DISABLE_DEPRECATED
#undef GNOME_DISABLE_DEPRECATED

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "interface.h"
#include "properties.h"
#include "properties-handler.h"
#include "zmisc.h"
#define ZCONF_DOMAIN "/zapping/internal/callbacks/"
#include "zconf.h"
#include "zvbi.h"
#include "osd.h"
#include "x11stuff.h"
#include "keyboard.h"
#include "remote.h"
#include "globals.h"
#include "zvideo.h"
#include "eggcellrendererkeys.h"

/* Property handlers for the different pages */
/* Device info */
static void
di_setup		(GtkWidget	*page)
{
  extern tveng_device_info *main_info;
  GtkWidget *widget;
  gchar *buffer;
  GtkNotebook *nb;
  GtkWidget * nb_label;
  GtkWidget * nb_body;
  const tv_video_line *l;

  /* The device name */
  widget = lookup_widget(page, "label27");
  gtk_label_set_text(GTK_LABEL(widget), main_info->caps.name);

  /* Minimum capture dimensions */
  widget = lookup_widget(page, "label28");
  z_label_set_text_printf (GTK_LABEL(widget),
			   "%d x %d",
			   main_info->caps.minwidth,
			   main_info->caps.minheight);

  /* Maximum capture dimensions */
  widget = lookup_widget(page, "label29");
  z_label_set_text_printf (GTK_LABEL(widget),
			   "%d x %d",
			   main_info->caps.maxwidth,
			   main_info->caps.maxheight);

  /* Reported device capabilities */
  widget = lookup_widget(page, "label30");
  buffer = g_strdup_printf("%s%s%s%s%s%s%s%s%s%s",
			   main_info->caps.flags & TVENG_CAPS_CAPTURE
			   ? _("Can capture to memory.\n") : "",
			   main_info->caps.flags & TVENG_CAPS_TUNER
			   ? _("Has some tuner.\n") : "",
			   main_info->caps.flags & TVENG_CAPS_TELETEXT
			   ? _("Supports the teletext service.\n") : "",
			   main_info->caps.flags & TVENG_CAPS_OVERLAY
			   ? _("Can overlay the image.\n") : "",
			   main_info->caps.flags & TVENG_CAPS_CHROMAKEY
			   ? _("Can chromakey the image.\n") : "",
			   main_info->caps.flags & TVENG_CAPS_CLIPPING
			   ? _("Clipping rectangles are supported.\n") : "",
			   main_info->caps.flags & TVENG_CAPS_FRAMERAM
			   ? _("Framebuffer memory is overwritten.\n") : "",
			   main_info->caps.flags & TVENG_CAPS_SCALES
			   ? _("The capture can be scaled.\n") : "",
			   main_info->caps.flags & TVENG_CAPS_MONOCHROME
			   ? _("Only monochrome is available\n") : "",
			   main_info->caps.flags & TVENG_CAPS_SUBCAPTURE
			   ? _("The capture can be zoomed\n") : "");
  /* Delete the last '\n' to save some space */
  if ((strlen(buffer) > 0) && (buffer[strlen(buffer)-1] == '\n'))
    buffer[strlen(buffer)-1] = 0;

  gtk_label_set_text(GTK_LABEL(widget), buffer);
  g_free(buffer);

  nb = GTK_NOTEBOOK (lookup_widget(page, "notebook2"));
  if (!main_info->video_inputs)
    {
      nb_label = gtk_label_new(_("No available inputs"));
      gtk_widget_show (nb_label);
      nb_body = gtk_label_new(_("Your video device has no inputs"));
      gtk_widget_show(nb_body);
      gtk_notebook_append_page(nb, nb_body, nb_label);
      gtk_widget_set_sensitive(GTK_WIDGET(nb), FALSE);
   }
  else
    for (l = tv_next_video_input (main_info, NULL);
	 l; l = tv_next_video_input (main_info, l))
      {
	char *type_str;

	nb_label = gtk_label_new(l->label);
	gtk_widget_show (nb_label);

	if (l->type == TV_VIDEO_LINE_TYPE_TUNER)
	  type_str = _("TV input");
	else
	  type_str = _("Camera");

          buffer = g_strdup_printf ("%s", type_str);

	nb_body = gtk_label_new (buffer);
	g_free (buffer);
	gtk_widget_show (nb_body);
	gtk_notebook_append_page(nb, nb_body, nb_label);
      }
  
  /* Selected video device */
  widget = lookup_widget(page, "fileentry1");
  widget = gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY(widget));
  gtk_entry_set_text(GTK_ENTRY(widget),
		     zconf_get_string(NULL,
				      "/zapping/options/main/video_device"));
  /* Current controller */
  widget = lookup_widget(page, "label31");
  tveng_describe_controller(NULL, &buffer, main_info);
  gtk_label_set_text(GTK_LABEL(widget), buffer);
}

static void
di_apply		(GtkWidget	*page)
{
  GtkWidget *widget;
  gchar *text;

  widget = lookup_widget(page, "fileentry1"); /* Video device entry
					       */
  text = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY(widget),
					 TRUE);
  if (text)
    zconf_set_string(text, "/zapping/options/main/video_device");
  
  g_free(text); /* In the docs it says this should be freed */  
}

/*
 *  Favorite picture sizes
 */

#define ZCONF_PICTURE_SIZES "/zapping/options/main/picture_sizes"

typedef struct picture_size {
  struct picture_size *		next;
  guint				width;
  guint				height;
  z_key				key;
} picture_size;

static picture_size *		favorite_picture_sizes;

static inline void
picture_size_delete		(picture_size *		ps)
{
  g_free (ps);
}

static inline picture_size *
picture_size_new		(guint			width,
				 guint			height,
				 z_key			key)
{
  picture_size *ps;

  ps = g_malloc0 (sizeof (*ps));

  ps->width = width;
  ps->height = height;
  ps->key = key;

  return ps;
}

static void
picture_sizes_delete		(void)
{
  picture_size *ps;

  while ((ps = favorite_picture_sizes))
    {
      favorite_picture_sizes = ps->next;
      picture_size_delete (ps);
    }
}

gboolean
on_picture_size_key_press	(GtkWidget *		widget,
				 GdkEventKey *		event,
				 gpointer		user_data)
{
  picture_size *ps;
  z_key key;

  key.key = gdk_keyval_to_lower (event->keyval);
  key.mask = event->state;

  /* fprintf(stderr, "key %x %x\n", key.key, key.mask); */

  for (ps = favorite_picture_sizes; ps; ps = ps->next)
    if (z_key_equal (ps->key, key))
      {
	python_command_printf (widget,
			       "zapping.resize_screen(%u, %u)",
			       ps->width, ps->height);
	return TRUE; /* handled */
      }

  return FALSE; /* not for us, pass on */
}

static void
picture_sizes_load_default	(void)
{
  static const guint sizes[][2] =
    {
      { 1024, 576 },	/* PAL/SECAM 16:9 */
      { 768, 576 },	/* PAL/SECAM 4:3 */
      { 720, 576 },	/* ITU-R 601 */
      { 864, 480 },	/* NTSC 16:9 (rounded) */
      { 640, 480 },	/* NTSC 4:3 */
      { 384, 288 },	/* PAL/SECAM 1/4 */
      { 352, 288 },	/* CIF */
      { 320, 240 }	/* NTSC 1/4 */
    };
  picture_size **pps;
  guint i;

  picture_sizes_delete ();
  pps = &favorite_picture_sizes;

  for (i = 0; i < G_N_ELEMENTS (sizes); ++i)
    {
      *pps = picture_size_new (sizes[i][0], sizes[i][1], Z_KEY_NONE);
      pps = &(*pps)->next;
    }
}

#define ZCONF_PICTURE_SIZES "/zapping/options/main/picture_sizes"

static inline void
picture_sizes_reset_index	(void)
{
  zconf_create_integer (0, "", ZCONF_PICTURE_SIZES "/index");
}

static gboolean
picture_sizes_load		(void)
{
  picture_size **pps;
  guint i;

  picture_sizes_delete ();
  pps = &favorite_picture_sizes;

  for (i = 0;; ++i)
    {
      gchar buffer[256], *s;
      guint width, height;
      z_key key = Z_KEY_NONE;

      s = buffer + snprintf (buffer, sizeof (buffer) - 10,
			     ZCONF_PICTURE_SIZES "/%u/", i);

      strcpy (s, "width");
      width = zconf_get_integer (NULL, buffer);

      if (zconf_error ())
	{
	  if (0 == i)
	    return FALSE;
	  else
	    break;
	}

      strcpy (s, "height");
      height = zconf_get_integer (NULL, buffer);

      if (zconf_error ())
	break;

      *s = 0;
      zconf_get_z_key (&key, buffer);

      *pps = picture_size_new (width, height, key);
      pps = &(*pps)->next;
    }

  return TRUE;
}

static void
picture_sizes_save		(void)
{
  picture_size *ps;
  guint i = 0;

  zconf_delete (ZCONF_PICTURE_SIZES);

  for (ps = favorite_picture_sizes; ps; ps = ps->next)
    {
      gchar buffer[256], *s;

      s = buffer + snprintf (buffer, sizeof (buffer) - 10,
			     ZCONF_PICTURE_SIZES "/%u/", i);

      strcpy (s, "width");
      zconf_create_integer (ps->width, "", buffer);

      strcpy (s, "height");
      zconf_create_integer (ps->height, "", buffer);

      *s = 0;
      zconf_create_z_key (ps->key, "", buffer);

      ++i;
    }
}

/* Popup menu */

static GtkAccelGroup *		accel_group;

static void
picture_sizes_on_menu_activate	(GtkMenuItem *		menu_item,
				 gpointer		user_data)
{
  picture_size *ps;
  guint count = GPOINTER_TO_INT (user_data);

  g_object_unref (accel_group);
  accel_group = NULL;

  for (ps = favorite_picture_sizes; ps; ps = ps->next)
    if (0 == count--)
      {
	zconf_create_integer (GPOINTER_TO_INT (user_data),
			      "", ZCONF_PICTURE_SIZES "/index");
	python_command_printf (GTK_WIDGET (menu_item),
			       "zapping.resize_screen(%u, %u)",
			       ps->width, ps->height);
	return;
      }
}

guint
picture_sizes_append_menu	(GtkMenuShell *		menu)
{
  picture_size *ps;
  GtkWidget *menu_item;
  guint count = 0;

  if (!favorite_picture_sizes)
    {
      if (!picture_sizes_load ())
	picture_sizes_load_default ();

      picture_sizes_reset_index ();
    }

  if (favorite_picture_sizes)
    {
      if (!accel_group)
	accel_group = gtk_accel_group_new ();

      menu_item = gtk_separator_menu_item_new ();
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (menu, menu_item);
    }

  for (ps = favorite_picture_sizes; ps; ps = ps->next)
    {
      gchar *buffer;

      buffer = g_strdup_printf (_("Resize to %ux%u"), ps->width, ps->height);
      menu_item = gtk_menu_item_new_with_label (buffer);
      g_free (buffer);

      if (ps->key.key)
	gtk_widget_add_accelerator (menu_item, "activate",
				    accel_group,
				    ps->key.key, ps->key.mask,
				    GTK_ACCEL_VISIBLE);

      g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (picture_sizes_on_menu_activate),
			GINT_TO_POINTER (count));

      gtk_widget_show (menu_item);
      gtk_menu_shell_append (menu, menu_item);

      ++count;
    }

  return count;
}

static PyObject *
py_picture_size_cycle		(PyObject *		self,
				 PyObject *		args)
{
  picture_size *ps;
  int value;
  gint index;
  gint count;

  value = +1;

  count = 0;
  for (ps = favorite_picture_sizes; ps; ps = ps->next)
    ++count;

  if (!PyArg_ParseTuple (args, "|i", &value))
    g_error ("zapping.picture_size_cycle(|i)");

  zconf_get_integer (&index, ZCONF_PICTURE_SIZES "/index");

  index += value;

  if (index < 0)
    index = count - 1;
  else if (index >= count)
    index = 0;

  zconf_set_integer (index, ZCONF_PICTURE_SIZES "/index");

  for (ps = favorite_picture_sizes; ps; ps = ps->next)
    if (--index == 0)
      {
	python_command_printf (python_command_widget (),
			       "zapping.resize_screen(%u, %u)",
			       ps->width, ps->height);
	break;
      }

  py_return_true;
}

/* Picture sizes editing dialog */

enum 
{
  C_WIDTH,
  C_HEIGHT,
  C_KEY,
  C_KEY_MASK,
  C_EDITABLE,
  C_NUM
};

static GtkListStore *
picture_sizes_create_model	(void)
{
  GtkListStore *model;
  picture_size *ps;

  if (!favorite_picture_sizes)
    {
      if (!picture_sizes_load ())
	picture_sizes_load_default ();

      picture_sizes_reset_index ();
    }

  model = gtk_list_store_new (C_NUM,
			      G_TYPE_UINT,
			      G_TYPE_UINT,
			      G_TYPE_UINT,
			      G_TYPE_UINT,
			      G_TYPE_BOOLEAN);

  for (ps = favorite_picture_sizes; ps; ps = ps->next)
    {
      GtkTreeIter iter;

      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter,
			  C_WIDTH, ps->width,
			  C_HEIGHT, ps->height,
			  C_KEY, ps->key.key,
			  C_KEY_MASK, ps->key.mask,
			  C_EDITABLE, TRUE,
			  -1);
    }

  return model;
}

static void
picture_sizes_model_iter	(GtkTreeView *		tree_view,
				 const gchar *		path_string,
				 GtkTreeModel **	model,
				 GtkTreeIter *		iter)
{
  GtkTreePath *path;

  *model = gtk_tree_view_get_model (tree_view);

  path = gtk_tree_path_new_from_string (path_string);
  gtk_tree_model_get_iter (*model, iter, path);
  gtk_tree_path_free (path);
}

static void
picture_sizes_on_cell_edited	(GtkCellRendererText *	cell,
				 const gchar *		path_string,
				 const gchar *		new_text,
				 GtkTreeView *		tree_view)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  guint column;
  guint value;

  picture_sizes_model_iter (tree_view, path_string, &model, &iter);
  column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column"));
  value = strtoul (new_text, NULL, 0);

  if (value > 0)
    value = SATURATE (value, 64, 16383);

  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      column, value,
		      -1);

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
picture_sizes_on_accel_edited	(GtkCellRendererText *	cell,
				 const char *		path_string,
				 guint			keyval,
				 EggVirtualModifierType	mask,
				 guint			keycode,
				 GtkTreeView *		tree_view)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  z_key key;

  key.key = keyval;
  key.mask = mask;

  picture_sizes_model_iter (tree_view, path_string, &model, &iter);

  if (keyval != 0)
    gtk_tree_model_foreach (model, unique, &key);

  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      C_KEY, key.key,
		      C_KEY_MASK, key.mask,
		      -1);

  z_property_item_modified (GTK_WIDGET (tree_view));
}

static void
picture_sizes_accel_set_func	(GtkTreeViewColumn *	tree_column,
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
picture_sizes_on_selection_changed
				(GtkTreeSelection *	selection,
				 GtkTreeView *		tree_view)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkWidget *remove;
  gboolean selected;

  model = gtk_tree_view_get_model (tree_view);

  remove = lookup_widget (GTK_WIDGET (tree_view), "picture-sizes-remove");

  selected = z_tree_selection_iter_first (selection, model, &iter);

  gtk_widget_set_sensitive (remove, selected);
}

static void
picture_sizes_on_add_clicked	(GtkButton *		add,
				 GtkTreeView *		tree_view)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreePath *path;

  selection = gtk_tree_view_get_selection (tree_view);
  model = gtk_tree_view_get_model (tree_view);

  if (z_tree_selection_iter_first (selection, model, &iter))
    gtk_list_store_insert_before (GTK_LIST_STORE (model), &iter, &iter);
  else
    gtk_list_store_append (GTK_LIST_STORE (model), &iter);

  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      C_WIDTH, 0,
		      C_HEIGHT, 0,
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
picture_sizes_on_remove_clicked	(GtkButton *		remove,
				 GtkTreeView *		tree_view)
{
  z_tree_view_remove_selected (tree_view,
			       gtk_tree_view_get_selection (tree_view),
			       gtk_tree_view_get_model (tree_view));
}

static void
picture_sizes_apply		(GtkTreeView *		tree_view)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  picture_size **pps;

  model = gtk_tree_view_get_model (tree_view);

  picture_sizes_delete ();
  pps = &favorite_picture_sizes;

  if (!gtk_tree_model_get_iter_first (model, &iter))
    return;

  do
    {
      guint width;
      guint height;
      z_key key;
      
      gtk_tree_model_get (model, &iter,
			  C_WIDTH, &width,
			  C_HEIGHT, &height,
			  C_KEY, &key.key,
			  C_KEY_MASK, &key.mask,
			  -1);

      if (width > 0 && height > 0)
	{
	  *pps = picture_size_new (width, height, key);
	  pps = &(*pps)->next;
	}
    }
  while (gtk_tree_model_iter_next (model, &iter));

  picture_sizes_save ();
}

static void
picture_sizes_setup		(GtkWidget *		page)
{
  GtkTreeView *tree_view;
  GtkTreeSelection *selection;
  GtkListStore *list_store;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkWidget *widget;
  guint col;

  widget = lookup_widget (page, "picture-sizes-treeview");
  tree_view = GTK_TREE_VIEW (widget);
  gtk_tree_view_set_rules_hint (tree_view, TRUE);
  gtk_tree_view_set_reorderable (tree_view, TRUE);

  selection = gtk_tree_view_get_selection (tree_view);
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

  g_signal_connect (G_OBJECT (selection), "changed",
  		    G_CALLBACK (picture_sizes_on_selection_changed),
		    tree_view);

  list_store = picture_sizes_create_model ();
  gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (list_store));

  for (col = C_WIDTH; col <= C_HEIGHT; ++col)
    {
      renderer = gtk_cell_renderer_text_new ();

      gtk_tree_view_insert_column_with_attributes
	(tree_view, -1 /* append */,
	 (col == C_WIDTH) ? _("Width") : _("Height"),
	 renderer,
	 "text", col,
	 "editable", C_EDITABLE,
	 NULL);

      g_signal_connect (G_OBJECT (renderer), "edited",
			G_CALLBACK (picture_sizes_on_cell_edited),
			tree_view);

      g_object_set_data (G_OBJECT (renderer), "column",
			 (gpointer) col);
    }

  renderer = (GtkCellRenderer *) g_object_new
    (EGG_TYPE_CELL_RENDERER_KEYS,
     "editable", TRUE,
     "accel_mode", EGG_CELL_RENDERER_KEYS_MODE_X,
     NULL);

  g_signal_connect (G_OBJECT (renderer), "keys_edited",
		    G_CALLBACK (picture_sizes_on_accel_edited), tree_view);

  column = gtk_tree_view_column_new_with_attributes (_("Shortcut"),
						     renderer, NULL);
  gtk_tree_view_column_set_cell_data_func
    (column, renderer,
     (GtkTreeCellDataFunc) picture_sizes_accel_set_func,
     NULL, NULL);
  gtk_tree_view_append_column (tree_view, column);

  widget = lookup_widget (page, "picture-sizes-add");
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (picture_sizes_on_add_clicked),
		    tree_view);

  widget = lookup_widget (page, "picture-sizes-remove");
  /* First select, then remove. */
  gtk_widget_set_sensitive (widget, FALSE);
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (picture_sizes_on_remove_clicked),
		    tree_view);
}

/* Toolbar style option menu mostly copied from libgnomeui,
   for compatibility with libgnomeui's toolbar context menu. */

#include <gconf/gconf-client.h>

static GConfEnumStringPair toolbar_styles [] = {
  { GTK_TOOLBAR_TEXT, "text" },
  { GTK_TOOLBAR_ICONS, "icons" },
  { GTK_TOOLBAR_BOTH, "both" },
  { GTK_TOOLBAR_BOTH_HORIZ, "both_horiz" },
  { -1,	NULL }
};
   
static void
style_menu_item_activated	(GtkWidget *		item,
				 GtkToolbarStyle	style)
{
  GConfClient *conf;
  char *key;
  int i;

  key = gnome_gconf_get_gnome_libs_settings_relative ("toolbar_style");
  conf = gconf_client_get_default ();

  /* Set our per-app toolbar setting */
  for (i = 0; i < G_N_ELEMENTS (toolbar_styles); i++)
    {
      if (toolbar_styles[i].enum_value == style)
	{
	  gconf_client_set_string (conf, key, toolbar_styles[i].str, NULL);
	  break;
	}
    }

  g_free (key);
}

static void
global_menu_item_activated	(GtkWidget *		item)
{
  GConfClient *conf;
  char *key;

  key = gnome_gconf_get_gnome_libs_settings_relative ("toolbar_style");
  conf = gconf_client_get_default ();

  /* Unset the per-app toolbar setting */
  gconf_client_unset (conf, key, NULL);
  g_free (key);
}

static GtkWidget *
create_toolbar_style_menu	(void)
{
  GtkWidget *menu;
  GtkWidget *item;
  GtkWidget *both_item, *both_horiz_item;
  GtkWidget *icons_item, *text_item, *global_item;
  GSList *group;
  char *both, *both_horiz, *icons, *text, *global;
  char *str, *key;
  GtkToolbarStyle toolbar_style;
  GConfClient *conf;

  group = NULL;
  toolbar_style = GTK_TOOLBAR_BOTH;
  menu = gtk_menu_new ();

  both = _("Text Below Icons");
  both_horiz = _("Priority Text Beside Icons");
  icons = _("Icons Only");
  text = _("Text Only");

  both_item = gtk_radio_menu_item_new_with_label (group, both);
  g_signal_connect (both_item, "activate",
		    G_CALLBACK (style_menu_item_activated),
		    GINT_TO_POINTER (GTK_TOOLBAR_BOTH));
  group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (both_item));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), both_item);

  both_horiz_item = gtk_radio_menu_item_new_with_label (group, both_horiz);
  g_signal_connect (both_horiz_item, "activate",
		    G_CALLBACK (style_menu_item_activated),
		    GINT_TO_POINTER (GTK_TOOLBAR_BOTH_HORIZ));
  group = gtk_radio_menu_item_get_group
    (GTK_RADIO_MENU_ITEM (both_horiz_item));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), both_horiz_item);

  icons_item = gtk_radio_menu_item_new_with_label (group, icons);
  g_signal_connect (icons_item, "activate",
		    G_CALLBACK (style_menu_item_activated),
		    GINT_TO_POINTER (GTK_TOOLBAR_ICONS));
  group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (icons_item));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), icons_item);

  text_item = gtk_radio_menu_item_new_with_label (group, text);
  g_signal_connect (text_item, "activate",
		    G_CALLBACK (style_menu_item_activated),
		    GINT_TO_POINTER (GTK_TOOLBAR_TEXT));

  group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (text_item));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), text_item);

  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  /* Get global setting */
  conf = gconf_client_get_default ();
  str = gconf_client_get_string
    (conf, "/desktop/gnome/interface/toolbar_style", NULL);

  if (str != NULL)
    {
      if (!gconf_string_to_enum (toolbar_styles, str, (gint *) &toolbar_style))
	toolbar_style = GTK_TOOLBAR_BOTH;

      g_free (str);
    }

  switch (toolbar_style)
    {
    case GTK_TOOLBAR_BOTH:
      str = both;
      break;
    case GTK_TOOLBAR_BOTH_HORIZ:
      str = both_horiz;
      break;
    case GTK_TOOLBAR_ICONS:
      str = icons;
      break;
    case GTK_TOOLBAR_TEXT:
      str = text;
      break;
    default:
      g_assert_not_reached ();
    }

  global = g_strdup_printf (_("Use desktop default (%s)"), str);
  global_item = gtk_radio_menu_item_new_with_label (group, global);
  g_signal_connect (global_item, "activate",
		    G_CALLBACK (global_menu_item_activated), NULL);
  group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (global_item));
  g_free (global);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), global_item);

  gtk_widget_show_all (menu);

  /* Now select the correct menu according to our preferences */
  key = gnome_gconf_get_gnome_libs_settings_relative ("toolbar_style");
  str = gconf_client_get_string (conf, key, NULL);

  if (str == NULL)
    {
      /* We have no per-app setting, so the global one must be right. */
      gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (global_item), TRUE);
    }
  else
    {
      if (!gconf_string_to_enum (toolbar_styles, str, (gint *) &toolbar_style))
	toolbar_style = GTK_TOOLBAR_BOTH;

      /* We have a per-app setting, find out which one it is */
      switch (toolbar_style)
	{
	case GTK_TOOLBAR_BOTH:
	  gtk_check_menu_item_set_active
	    (GTK_CHECK_MENU_ITEM (both_item), TRUE);
	  break;
	case GTK_TOOLBAR_BOTH_HORIZ:
	  gtk_check_menu_item_set_active
	    (GTK_CHECK_MENU_ITEM (both_horiz_item), TRUE);
	  break;
	case GTK_TOOLBAR_ICONS:
	  gtk_check_menu_item_set_active
	    (GTK_CHECK_MENU_ITEM (icons_item), TRUE);
	  break;
	case GTK_TOOLBAR_TEXT:
	  gtk_check_menu_item_set_active
	    (GTK_CHECK_MENU_ITEM (text_item), TRUE);
	  break;
	default:
	  g_assert_not_reached ();
	}

      g_free (str);
    }

  g_free (key);

  return menu;
}

/* Main window */
static void
mw_setup		(GtkWidget	*page)
{
  GtkWidget *widget;
  GtkWidget *w;
  gboolean active;

  /* Save the geometry through sessions */
  w = lookup_widget(page, "checkbutton2");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),
    zconf_get_boolean(NULL, "/zapping/options/main/keep_geometry"));

  /* Show tooltips */
  w = lookup_widget (page, "checkbutton14");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(w),
    zconf_get_boolean (NULL, "/zapping/options/main/show_tooltips"));

  /* Disable screensaver */
  w = lookup_widget (page, "disable_screensaver");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(w),
    zconf_get_boolean (NULL, "/zapping/options/main/disable_screensaver"));

  /* Toolbar style */
  w = lookup_widget(page, "toolbar_style");
  gtk_option_menu_set_menu (GTK_OPTION_MENU (w), create_toolbar_style_menu ());

  /* Swap Page Up/Down */
/*
  w = lookup_widget(page, "checkbutton13");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w),
    zconf_get_boolean(NULL, "/zapping/options/main/swap_up_down"));
*/

  /* Title format Z will use */
  w = lookup_widget(page, "title_format");
  w = gnome_entry_gtk_entry(GNOME_ENTRY(w));
  gtk_entry_set_text(GTK_ENTRY(w),
		     zconf_get_string(NULL,
				      "/zapping/options/main/title_format"));

#if 0 /* FIXME combine with size inc, move into menu */
  /* ratio mode to use */
  w = lookup_widget(page, "optionmenu1");
  gtk_option_menu_set_history(GTK_OPTION_MENU(w),
    zconf_get_integer(NULL,
		      "/zapping/options/main/ratio"));
#endif

  active = zconf_get_boolean (NULL, "/zapping/options/main/save_controls");
  widget = lookup_widget (page, "general-main-controls-per-channel");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), active);



  {
    gint n;

    /* entered channel numbers refer to */
    w = lookup_widget (page, "channel_number_translation");
    n = zconf_get_integer (NULL, "/zapping/options/main/channel_txl");

    if (n < 0)
      n = 0; /* historical: -1 disabled keypad channel number entering */

    gtk_option_menu_set_history (GTK_OPTION_MENU (w), n);
  }

}

static void
mw_apply		(GtkWidget	*page)
{
  GtkWidget *widget;
  gboolean top;
  gboolean active;

  widget = lookup_widget(page, "checkbutton2"); /* keep geometry */
  zconf_set_boolean(gtk_toggle_button_get_active(
		 GTK_TOGGLE_BUTTON(widget)),
		    "/zapping/options/main/keep_geometry");

  widget = lookup_widget(page, "checkbutton14"); /* show tooltips */
  top = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  zconf_set_boolean (top, "/zapping/options/main/show_tooltips");
  z_tooltips_active (top);

  widget = lookup_widget(page, "disable_screensaver");
  top = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  zconf_set_boolean (top, "/zapping/options/main/disable_screensaver");
  x11_screensaver_control (top);


  widget = lookup_widget(page, "title_format"); /* title format */
  widget = gnome_entry_gtk_entry(GNOME_ENTRY(widget));
  zconf_set_string(gtk_entry_get_text(GTK_ENTRY(widget)),
		   "/zapping/options/main/title_format");

#if 0 /* FIXME */
  widget = lookup_widget(page, "optionmenu1"); /* ratio mode */
  zconf_set_integer(z_option_menu_get_active(widget),
		    "/zapping/options/main/ratio");
#endif

  widget = lookup_widget (page, "general-main-controls-per-channel");
  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  zconf_set_boolean (active, "/zapping/options/main/save_controls");

  /* entered channel numbers refer to */
  widget = lookup_widget (page, "channel_number_translation");
  zconf_set_integer (z_option_menu_get_active (widget),
		     "/zapping/options/main/channel_txl");
}









/* Video */
static void
video_setup		(GtkWidget	*page)
{
  GtkWidget *widget;
  gboolean active;
#if 0
  /* Verbosity value passed to zapping_setup_fb */
  widget = lookup_widget(page, "spinbutton1");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),
     zconf_get_integer(NULL,
		       "/zapping/options/main/zapping_setup_fb_verbosity"));
#endif

#ifdef HAVE_VIDMODE_EXTENSION

  {
    GtkWidget *menu;
    GtkWidget *menuitem;
    x11_vidmode_info *info, *hist;
    const gchar *mode;
    guint i, h;

    /* fullscreen video mode */

    mode = zconf_get_string (NULL, "/zapping/options/main/fullscreen/vidmode");

    menu = gtk_menu_new ();

    /* TRANSLATORS: Fullscreen video mode */
    menuitem = gtk_menu_item_new_with_label (_("Do not change"));
    gtk_widget_show (menuitem);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

    menuitem = gtk_menu_item_new_with_label (_("Automatic"));
    gtk_widget_show (menuitem);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

    h = (mode && 0 == strcmp (mode, "auto"));
    hist = x11_vidmode_by_name (vidmodes, mode);

    for (info = vidmodes, i = 2; info; info = info->next, i++)
      {
        gchar *s;

        if (info == hist)
          h = i;
      
        /* TRANSLATORS: Fullscreen video mode */
        s = g_strdup_printf (_("%u x %u @ %u Hz"),
		         info->width, info->height,
		         (unsigned int)(info->vfreq + 0.5));

        menuitem = gtk_menu_item_new_with_label (s);
        gtk_widget_show (menuitem);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menuitem);

        g_free (s);
      }
      
    widget = lookup_widget(page, "optionmenu2");
    gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), menu);
    gtk_option_menu_set_history (GTK_OPTION_MENU(widget), h);
  }

#else /* !HAVE_VIDMODE_EXTENSION */

  widget = lookup_widget (page, "label90");
  gtk_widget_set_sensitive (widget, FALSE);
  widget = lookup_widget (page, "optionmenu2");
  gtk_widget_set_sensitive (widget, FALSE);

#endif

#ifdef HAVE_XV_EXTENSION

  /* capture size under XVideo */
  widget = lookup_widget(page, "optionmenu20");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/capture/xvsize"));

#else

  widget = lookup_widget (page, "label220");
  gtk_widget_set_sensitive (widget, FALSE);
  widget = lookup_widget (page, "optionmenu20");
  gtk_widget_set_sensitive (widget, FALSE);

#endif

  widget = lookup_widget (page, "general-video-fixed-inc");
  active = zconf_get_boolean (NULL, "/zapping/options/main/fixed_increments");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), active);

  picture_sizes_setup (page);
}

static void
video_apply		(GtkWidget	*page)
{
  GtkWidget *tv_screen;
  GtkWidget *widget;
  gboolean active;

#if 0
  widget = lookup_widget(page, "spinbutton1"); /* zapping_setup_fb
						  verbosity */
  zconf_set_integer(
	gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget)),
		"/zapping/options/main/zapping_setup_fb_verbosity");
#endif

#ifdef HAVE_VIDMODE_EXTENSION

  {
    const gchar *opt = "/zapping/options/main/fullscreen/vidmode";
    guint i;

    widget = lookup_widget(page, "optionmenu2"); /* change mode */
    i = z_option_menu_get_active (widget);

    if (i == 1)
      {
        zconf_set_string ("auto", opt);
      }
    else
      {
        x11_vidmode_info *info;

        info = NULL;
      
        if (i >= 2)
          for (info = vidmodes; info; info = info->next)
            if (i-- == 2)
	      break;

	if (info)
	  {
            gchar *s = g_strdup_printf ("%ux%u@%u",
					info->width, info->height,
					(unsigned int)(info->vfreq + 0.5));
            zconf_set_string (s, opt);
	    g_free (s);
	  }
	else
	  {
            zconf_set_string ("", opt);
          }
      }
  }

#endif /* HAVE_VIDMODE_EXTENSION */

#ifdef HAVE_XV_EXTENSION

  widget = lookup_widget(page, "optionmenu20"); /* xv capture size */
  zconf_set_integer(z_option_menu_get_active(widget),
		    "/zapping/options/capture/xvsize");

#endif

  widget = lookup_widget (page, "general-video-fixed-inc");
  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  zconf_set_boolean (active, "/zapping/options/main/fixed_increments");

  tv_screen = lookup_widget (main_window, "tv-screen");

  if (active) /* XXX free, 4:3, 16:9 */
    z_video_set_size_inc (Z_VIDEO (tv_screen), 64, 64 * 3 / 4);
  else
    z_video_set_size_inc (Z_VIDEO (tv_screen), 1, 1);

  widget = lookup_widget (page, "picture-sizes-treeview");
  picture_sizes_apply (GTK_TREE_VIEW (widget));
}

/* VBI */

static void
on_enable_vbi_toggled	(GtkWidget	*widget,
			 GtkWidget	*page)
{
  gboolean active =
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  GtkWidget *itv_props =
    get_properties_page(widget, _("VBI Options"),
			_("Interactive TV"));
  gtk_widget_set_sensitive(lookup_widget(widget,
					 "vbox19"), active);

  if (itv_props)
    gtk_widget_set_sensitive(itv_props, active);
}

static void
vbi_general_setup	(GtkWidget	*page)
{

#ifdef HAVE_LIBZVBI

  GtkWidget *widget;

  /* Enable VBI decoding */
  widget = lookup_widget(page, "checkbutton6");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/vbi/enable_vbi"));
  on_enable_vbi_toggled(widget, page);
  g_signal_connect(G_OBJECT(widget), "toggled",
		   G_CALLBACK(on_enable_vbi_toggled),
		   page);

  /* VBI device */
  widget = lookup_widget(page, "fileentry2");
  widget = gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY(widget));
  gtk_entry_set_text(GTK_ENTRY(widget),
		     zconf_get_string(NULL,
				      "/zapping/options/vbi/vbi_device"));

  /* Default region */
  widget = lookup_widget(page, "optionmenu3");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/default_region"));

  /* Teletext level */
  widget = lookup_widget(page, "optionmenu4");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/teletext_level"));

  /* Quality/speed tradeoff */
  widget = lookup_widget(page, "optionmenu21");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/qstradeoff"));

#else /* !HAVE_LIBZVBI */

  gtk_widget_set_sensitive (page, FALSE);

#endif

}

typedef enum {
  TOGGLE_FALSE,
  TOGGLE_TRUE,
  TOGGLE_TO_FALSE,
  TOGGLE_TO_TRUE,
} togglean;

#define TOGGLE_CURRENT(t) ((t) & 1)
#define TOGGLE_CHANGED(t) ((t) & 2)

static togglean
set_toggle		(GtkWidget *page,
			 gchar *widget_name,
			 gchar *zconf_name)
{
  gboolean new_state, old_state;
  GtkWidget *widget;

  widget = lookup_widget (page, widget_name);
  new_state = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  old_state = zconf_get_boolean (NULL, zconf_name);
  zconf_set_boolean (new_state, zconf_name);

  return (old_state ^ new_state) * 2 + new_state;
}

static void
vbi_general_apply	(GtkWidget	*page)
{

#ifdef HAVE_LIBZVBI

  togglean enable_vbi;
  GtkWidget *widget;
  gchar *text;
  gint index;

  int region_mapping[8] = {
    0, /* WCE */
    8, /* EE */
    16, /* WET */
    24, /* CSE */
    32, /* C */
    48, /* GC */
    64, /* A */
    80 /* I */
  };

  /* enable VBI decoding */
  enable_vbi = set_toggle (page, "checkbutton6",
			   "/zapping/options/vbi/enable_vbi");
  if (enable_vbi == TOGGLE_TO_FALSE)
    {
      /* XXX bad design */
#if 0 /* Temporarily removed */
      zvbi_reset_program_info ();
      zvbi_reset_network_info ();
#endif
    }
#if 0 /* always */
  else if (use_vbi == TOGGLE_TO_FALSE)
    {
#if 0 /* Temporarily removed */
      zvbi_reset_network_info ();
#endif
    }
#endif

  widget = lookup_widget(page, "fileentry2"); /* VBI device entry */
  text = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY(widget),
					 TRUE);
  if (text)
    zconf_set_string(text, "/zapping/options/vbi/vbi_device");
  g_free(text); /* In the docs it says this should be freed */

  /* default_region */
  widget = lookup_widget(page, "optionmenu3");
  index = z_option_menu_get_active(widget);

  if (index < 0)
    index = 0;
  if (index > 7)
    index = 7;

  zconf_set_integer(index, "/zapping/options/vbi/default_region");
  if (zvbi_get_object())
    vbi_teletext_set_default_region(zvbi_get_object(), region_mapping[index]);

  /* teletext_level */
  widget = lookup_widget(page, "optionmenu4");
  index = z_option_menu_get_active(widget);
  if (index < 0)
    index = 0;
  if (index > 3)
    index = 3;
  zconf_set_integer(index, "/zapping/options/vbi/teletext_level");
  if (zvbi_get_object())
    vbi_teletext_set_level(zvbi_get_object(), index);

  /* Quality/speed tradeoff */
  widget = lookup_widget(page, "optionmenu21");
  index = z_option_menu_get_active(widget);
  if (index < 0)
    index = 0;
  if (index > 3)
    index = 3;
  zconf_set_integer(index, "/zapping/options/vbi/qstradeoff");

#endif /* HAVE_LIBZVBI */

}

#if 0

/* Interactive TV */
static void
itv_setup		(GtkWidget	*page)
{
  GtkWidget *widget;

  /* The various itv filters */
  widget = lookup_widget(page, "optionmenu12");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/pr_trigger"));

  widget = lookup_widget(page, "optionmenu16");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/nw_trigger"));

  widget = lookup_widget(page, "optionmenu17");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/st_trigger"));

  widget = lookup_widget(page, "optionmenu18");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/sp_trigger"));

  widget = lookup_widget(page, "optionmenu19");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/op_trigger"));

  widget = lookup_widget(page, "optionmenu6");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/trigger_default"));

  /* Filter level */
  widget = lookup_widget(page, "optionmenu5");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/filter_level"));

  /* Set sensitive/unsensitive according to enable_vbi state */
  gtk_widget_set_sensitive(page, zconf_get_boolean(NULL,
			   "/zapping/options/vbi/enable_vbi"));
}

static void
itv_apply		(GtkWidget	*page)
{
  GtkWidget *widget;
  gint index;

  /* The many itv filters */
  widget = lookup_widget(page, "optionmenu12");
  index = z_option_menu_get_active(widget);
  zconf_set_integer(index, "/zapping/options/vbi/pr_trigger");
  
  widget = lookup_widget(page, "optionmenu16");
  index = z_option_menu_get_active(widget);
  zconf_set_integer(index, "/zapping/options/vbi/nw_trigger");
  
  widget = lookup_widget(page, "optionmenu17");
  index = z_option_menu_get_active(widget);
  zconf_set_integer(index, "/zapping/options/vbi/st_trigger");
  
  widget = lookup_widget(page, "optionmenu18");
  index = z_option_menu_get_active(widget);
  zconf_set_integer(index, "/zapping/options/vbi/sp_trigger");
  
  widget = lookup_widget(page, "optionmenu19");
  index = z_option_menu_get_active(widget);
  zconf_set_integer(index, "/zapping/options/vbi/op_trigger");
  
  widget = lookup_widget(page, "optionmenu6");
  index = z_option_menu_get_active(widget);
  zconf_set_integer(index, "/zapping/options/vbi/trigger_default");
  
  /* Filter level */
  widget = lookup_widget(page, "optionmenu5");
  index = z_option_menu_get_active(widget);
  zconf_set_integer(index, "/zapping/options/vbi/filter_level");
}

#endif

static void
add				(GtkDialog	*dialog)
{
  SidebarEntry devices [] = {
    { N_("Video"), "gnome-info.png", "vbox9",
      di_setup, di_apply,
      .help_link_id = "zapping-settings-video-device" }
  };
  SidebarEntry general [] = {
    { N_("Main Window"), "gnome-session.png", "vbox35",
      mw_setup, mw_apply,
      .help_link_id = "zapping-settings-main" },
    { N_("Video"), "gnome-television.png",
      "general-video-table", video_setup, video_apply,
      .help_link_id = "zapping-settings-video-options" }
  };
  SidebarEntry vbi [] = {
    { N_("General"), "gnome-monitor.png", "vbox17",
      vbi_general_setup, vbi_general_apply,
      .help_link_id = "zapping-settings-vbi" },
#if 0 /* temporarily disabled */
    { N_("Interactive TV"), "gnome-monitor.png", "vbox33",
      itv_setup, itv_apply }
#endif
  };
  SidebarGroup groups [] = {
    { N_("Devices"),	     devices, G_N_ELEMENTS (devices) },
    { N_("General Options"), general, G_N_ELEMENTS (general) },
    { N_("VBI Options"),     vbi,     G_N_ELEMENTS (vbi) }
    /* XXX "VBI Options" is also a constant in ttxview.c */
  };

  standard_properties_add (dialog, groups, G_N_ELEMENTS (groups),
			   "zapping.glade2");
}

void startup_properties_handler(void)
{
  property_handler handler = {
    add:	add
  };
  prepend_property_handler(&handler);

  if (!favorite_picture_sizes)
    {
      if (!picture_sizes_load ())
	picture_sizes_load_default ();

      picture_sizes_reset_index ();
    }

  cmd_register ("picture_size_cycle", py_picture_size_cycle, METH_VARARGS,
		("Next favorite picture size"), "zapping.picture_size_cycle(+1)",
		("Previous favorite picture size"), "zapping.picture_size_cycle(-1)");
}

void shutdown_properties_handler(void)
{
  /* Nothing */
}

