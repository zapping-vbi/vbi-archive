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
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "callbacks.h"
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

/* Property handlers for the different pages */
/* Device info */
static void
di_setup		(GtkWidget	*page)
{
  extern tveng_device_info *main_info;
  GtkWidget *widget;
  gchar *buffer;
  gint i;
  GtkNotebook *nb;
  GtkWidget * nb_label;
  GtkWidget * nb_body;

  /* The device name */
  widget = lookup_widget(page, "label27");
  gtk_label_set_text(GTK_LABEL(widget), main_info->caps.name);

  /* Minimum capture dimensions */
  widget = lookup_widget(page, "label28");
  buffer = g_strdup_printf("%d x %d", main_info->caps.minwidth,
			   main_info->caps.minheight);
  gtk_label_set_text(GTK_LABEL(widget), buffer);
  g_free(buffer);

  /* Maximum capture dimensions */
  widget = lookup_widget(page, "label29");
  buffer = g_strdup_printf("%d x %d", main_info->caps.maxwidth,
			   main_info->caps.maxheight);
  gtk_label_set_text(GTK_LABEL(widget), buffer);
  g_free(buffer);

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
  if (main_info -> num_inputs == 0)
    {
      nb_label = gtk_label_new(_("No available inputs"));
      gtk_widget_show (nb_label);
      nb_body = gtk_label_new(_("Your video device has no inputs"));
      gtk_widget_show(nb_body);
      gtk_notebook_append_page(nb, nb_body, nb_label);
      gtk_widget_set_sensitive(GTK_WIDGET(nb), FALSE);
   }
  else
    for (i = 0; i < main_info->num_inputs; i++)
      {
	char *type_str;

	nb_label = gtk_label_new(main_info->inputs[i].name);
	gtk_widget_show (nb_label);

	if (main_info->inputs[i].type == TVENG_INPUT_TYPE_TV)
	  type_str = _("TV input");
	else
	  type_str = _("Camera");

	if (main_info->inputs[i].tuners)
	  buffer = g_strdup_printf (ngettext ("%s with %d tuner",
	                                      "%s with %d tuners",
			                      main_info->inputs[i].tuners),
				    type_str, main_info->inputs[i].tuners);
	else
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
 *  Favorite picture sizes (popup menu)
 */

#define ZCONF_PICTURE_SIZES "/zapping/options/main/picture_sizes"

typedef struct picture_size {
  struct picture_size *		next;
  guint				width;
  guint				height;
  z_key				key;
} picture_size;

static picture_size *		favorite_picture_sizes = NULL;

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
picture_sizes_flush		(void)
{
  picture_size *ps;

  while ((ps = favorite_picture_sizes))
    {
      favorite_picture_sizes = ps->next;
      picture_size_delete (ps);
    }
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

  picture_sizes_flush ();
  pps = &favorite_picture_sizes;

  for (i = 0; i < G_N_ELEMENTS (sizes); i++)
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

  picture_sizes_flush ();
  pps = &favorite_picture_sizes;

  for (i = 0;; i++)
    {
      gchar *buffer;
      guint width, height;
      z_key key = Z_KEY_NONE;

      buffer = g_strdup_printf (ZCONF_PICTURE_SIZES "/%u/width", i);
      width = zconf_get_integer (NULL, buffer);
      g_free (buffer);

      if (zconf_error ())
	{
	  if (i == 0)
	    return FALSE;
	  else
	    break;
	}

      buffer = g_strdup_printf (ZCONF_PICTURE_SIZES "/%u/height", i);
      height = zconf_get_integer (NULL, buffer);
      g_free (buffer);

      if (zconf_error ())
	break;

      buffer = g_strdup_printf (ZCONF_PICTURE_SIZES "/%u/", i);
      zconf_get_z_key (&key, buffer);
      g_free (buffer);

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
      gchar *buffer;

      buffer = g_strdup_printf (ZCONF_PICTURE_SIZES "/%u/width", i);
      zconf_create_integer (ps->width, "", buffer);
      g_free (buffer);

      buffer = g_strdup_printf (ZCONF_PICTURE_SIZES "/%u/height", i);
      zconf_create_integer (ps->height, "", buffer);
      g_free (buffer);

      buffer = g_strdup_printf (ZCONF_PICTURE_SIZES "/%u/", i);
      zconf_create_z_key (ps->key, "", buffer);
      g_free (buffer);

      i++;
    }
}

/* Popup menu */

static GtkAccelGroup *		accel_group;

static void
picture_sizes_on_menu_activate	(GtkMenuItem *		menuitem,
				 gpointer		user_data)
{
  picture_size *ps;
  guint count = GPOINTER_TO_INT (user_data);

  g_object_unref (accel_group);
  accel_group = NULL;

  for (ps = favorite_picture_sizes; ps; ps = ps->next)
    if (count-- == 0)
      {
	zconf_create_integer (GPOINTER_TO_INT (user_data),
			      "", ZCONF_PICTURE_SIZES "/index");

	cmd_run_printf ("zapping.resize_screen(%u, %u)",
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

      buffer = g_strdup_printf (_("Resize to %ux%u"),
				ps->width, ps->height);
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

      count++;
    }

  return count;
}

static PyObject *
py_picture_size_cycle		(PyObject *		self,
				 PyObject *		args)
{
  picture_size *ps;
  gint value = +1;
  gint index, count;

  count = 0;

  for (ps = favorite_picture_sizes; ps; ps = ps->next)
    count++;

  if (!PyArg_ParseTuple (args, "|i", &value))
    g_error ("zapping.ttx_page_incr(|i)");

  zconf_get_integer (&index, ZCONF_PICTURE_SIZES "/index");

  index += value;

  if (index < 0)
    index = count - 1;
  else if (index >= count)
    index = 0;

  zconf_set_integer (index, ZCONF_PICTURE_SIZES "/index");

  for (ps = favorite_picture_sizes; ps; ps = ps->next)
    if (index-- == 0)
      {
	cmd_run_printf ("zapping.resize_screen(%u, %u)",
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
			      G_TYPE_UINT, G_TYPE_UINT,
			      G_TYPE_UINT, G_TYPE_UINT,
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
picture_sizes_on_cell_edited	(GtkCellRendererText *	cell,
				 const gchar *		path_string,
				 const gchar *		new_text,
				 GtkTreeView *		tree_view)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  guint column, value;

  model = gtk_tree_view_get_model (tree_view);

  {
    GtkTreePath *path;

    path = gtk_tree_path_new_from_string (path_string);
    gtk_tree_model_get_iter (model, &iter, path);
    gtk_tree_path_free (path);
  }

  column = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (cell), "column"));

  value = strtoul (new_text, NULL, 0);

  if (value > 16383)
    value = 16383;

  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      column, value,
		      -1);
}

static void
picture_sizes_set_func_key	(GtkTreeViewColumn *	column,
				 GtkCellRenderer *	cell,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter,
				 gpointer		data)
{
  z_key key;
  gchar *key_name;

  gtk_tree_model_get (model, iter,
		      C_KEY, &key.key,
		      C_KEY_MASK, &key.mask,
		      -1);

  key_name = z_key_name (key);

  g_object_set (cell, "text", key_name, NULL);

  g_free (key_name);
}

static void
picture_sizes_on_key_entry_changed (GtkEditable *	editable,
				    GtkTreeView *	tree_view)
{
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkWidget *key_entry;

  selection = gtk_tree_view_get_selection (tree_view);
  model = gtk_tree_view_get_model (tree_view);
  key_entry = lookup_widget (GTK_WIDGET (tree_view), "custom3");

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      z_key key;

      key = z_key_entry_get_key (key_entry);

      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			  C_KEY, key.key,
			  C_KEY_MASK, key.mask,
			  -1);
    }
}

static void
picture_sizes_selection_changed	(GtkTreeSelection *	selection,
				 GtkTreeView *		tree_view)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkWidget *key_entry;

  model = gtk_tree_view_get_model (tree_view);
  key_entry = lookup_widget (GTK_WIDGET (tree_view), "custom3");

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      z_key key;

      g_signal_handlers_block_by_func (G_OBJECT (key_entry),
				       (gpointer) picture_sizes_on_key_entry_changed,
				       (gpointer) tree_view);

      gtk_tree_model_get (model, &iter,
			  C_KEY, &key.key,
			  C_KEY_MASK, &key.mask,
			  -1);

      z_key_entry_set_key (key_entry, key);

      g_signal_handlers_unblock_by_func (G_OBJECT (key_entry),
					 (gpointer) picture_sizes_on_key_entry_changed,
					 (gpointer) tree_view);
    }
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

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
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
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;

  selection = gtk_tree_view_get_selection (tree_view);
  model = gtk_tree_view_get_model (tree_view);

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
}

static void
picture_sizes_apply		(GtkTreeView *		tree_view)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  picture_size **pps;

  model = gtk_tree_view_get_model (tree_view);

  picture_sizes_flush ();
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

/* Main window */
static void
mw_setup		(GtkWidget	*page)
{
  GtkWidget *widget;

  /* Save the geometry through sessions */
  widget = lookup_widget(page, "checkbutton2");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/keep_geometry"));

  /* Show tooltips */
  widget = lookup_widget (page, "checkbutton14");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean (NULL, "/zapping/options/main/show_tooltips"));

  /* Disable screensaver */
  widget = lookup_widget (page, "disable_screensaver");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean (NULL, "/zapping/options/main/disable_screensaver"));

  /* Resize using fixed increments */
  widget = lookup_widget(page, "checkbutton4");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/fixed_increments"));  

  /* Swap Page Up/Down */
/*
  widget = lookup_widget(page, "checkbutton13");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/swap_up_down"));
*/

  /* Title format Z will use */
  widget = lookup_widget(page, "title_format");
  widget = gnome_entry_gtk_entry(GNOME_ENTRY(widget));
  gtk_entry_set_text(GTK_ENTRY(widget),
		     zconf_get_string(NULL,
				      "/zapping/options/main/title_format"));

#if 0 /* FIXME combine with size inc, move into menu */
  /* ratio mode to use */
  widget = lookup_widget(page, "optionmenu1");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/main/ratio"));
#endif

  {
    gint n;

    /* entered channel numbers refer to */
    widget = lookup_widget (page, "channel_number_translation");
    n = zconf_get_integer (NULL, "/zapping/options/main/channel_txl");

    if (n < 0)
      n = 0; /* historical: -1 disabled keypad channel number entering */

    gtk_option_menu_set_history (GTK_OPTION_MENU (widget), n);
  }

  /* Picture size list */
  {
    GtkWidget *tree_view;
    guint column;

    tree_view = lookup_widget (page, "treeview1");
    gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view),
			     GTK_TREE_MODEL (picture_sizes_create_model ()));

    for (column = C_WIDTH; column <= C_HEIGHT; column++)
      {
	GtkCellRenderer *renderer;

	renderer = gtk_cell_renderer_text_new ();

	gtk_tree_view_insert_column_with_attributes
	  (GTK_TREE_VIEW (tree_view), -1 /* append */,
	   (column == C_WIDTH) ? _("Width") : _("Height"),
	   renderer,
	   "text", column,
	   "editable", C_EDITABLE,
	   NULL);

	g_signal_connect (G_OBJECT (renderer), "edited",
			  G_CALLBACK (picture_sizes_on_cell_edited),
			  tree_view);

	g_object_set_data (G_OBJECT (renderer), "column",
			   (gpointer) column);
      }

    gtk_tree_view_insert_column_with_data_func
      (GTK_TREE_VIEW (tree_view), -1 /* append */, _("Key"),
       gtk_cell_renderer_text_new (),
       picture_sizes_set_func_key, NULL, NULL);

    widget = lookup_widget (page, "button44");
    g_signal_connect (G_OBJECT (widget), "clicked",
		      G_CALLBACK (picture_sizes_on_add_clicked),
		      tree_view);

    widget = lookup_widget (page, "button45");
    g_signal_connect (G_OBJECT (widget), "clicked",
		      G_CALLBACK (picture_sizes_on_remove_clicked),
		      tree_view);

    g_signal_connect (G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view))),
		      "changed",
		      G_CALLBACK (picture_sizes_selection_changed),
		      tree_view);

    widget = lookup_widget (page, "custom3");
    g_signal_connect (G_OBJECT (z_key_entry_entry (widget)), "changed",
		      G_CALLBACK (picture_sizes_on_key_entry_changed),
		      tree_view);
  }
}

static void
mw_apply		(GtkWidget	*page)
{
  GtkWidget *widget;
  GtkWidget *tv_screen;
  gboolean top, inc;

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

  widget = lookup_widget(page, "checkbutton4"); /* fixed increments */
  inc = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
  zconf_set_boolean (inc, "/zapping/options/main/fixed_increments");
  tv_screen = lookup_widget (main_window, "tv-screen");
  if (inc)
    z_video_set_size_inc (Z_VIDEO (tv_screen), 64, 64 * 3 / 4); // XXX free, 4:3, 16:9
  else
    z_video_set_size_inc (Z_VIDEO (tv_screen), 1, 1);

/*
  widget = lookup_widget(page, "checkbutton13"); // swap chan up/down
  zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/swap_up_down");  
*/

  widget = lookup_widget(page, "title_format"); /* title format */
  widget = gnome_entry_gtk_entry(GNOME_ENTRY(widget));
  zconf_set_string(gtk_entry_get_text(GTK_ENTRY(widget)),
		   "/zapping/options/main/title_format");

#if 0 /* FIXME */
  widget = lookup_widget(page, "optionmenu1"); /* ratio mode */
  zconf_set_integer(z_option_menu_get_active(widget),
		    "/zapping/options/main/ratio");
#endif

  /* entered channel numbers refer to */
  widget = lookup_widget (page, "channel_number_translation");
  zconf_set_integer (z_option_menu_get_active (widget),
		     "/zapping/options/main/channel_txl");

  widget = lookup_widget (page, "treeview1");
  picture_sizes_apply (GTK_TREE_VIEW (widget));
}

/* Video */
static void
video_setup		(GtkWidget	*page)
{
  GtkWidget *widget;

  /* Avoid some flicker in preview mode */
  widget = lookup_widget(page, "checkbutton5");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/avoid_flicker"));

  /* Save control info with the channel */
  widget = lookup_widget(page, "checkbutton11");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/save_controls"));

  /* Verbosity value passed to zapping_setup_fb */
  widget = lookup_widget(page, "spinbutton1");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),
     zconf_get_integer(NULL,
		       "/zapping/options/main/zapping_setup_fb_verbosity"));

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

  /* capture size under XVideo */
  widget = lookup_widget(page, "optionmenu20");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/capture/xvsize"));
  
}

static void
video_apply		(GtkWidget	*page)
{
  GtkWidget *widget;


  widget = lookup_widget(page, "checkbutton5"); /* avoid flicker */
  zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/avoid_flicker");

  widget = lookup_widget(page, "checkbutton11"); /* save controls */
  zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/save_controls");

  widget = lookup_widget(page, "spinbutton1"); /* zapping_setup_fb
						  verbosity */
  zconf_set_integer(
	gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget)),
		"/zapping/options/main/zapping_setup_fb_verbosity");

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

  widget = lookup_widget(page, "optionmenu20"); /* xv capture size */
  zconf_set_integer(z_option_menu_get_active(widget),
		    "/zapping/options/capture/xvsize");
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
  GtkWidget *widget;

  /* Enable VBI decoding */
  widget = lookup_widget(page, "checkbutton6");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/vbi/enable_vbi"));
  on_enable_vbi_toggled(widget, page);
  g_signal_connect(G_OBJECT(widget), "toggled",
		   G_CALLBACK(on_enable_vbi_toggled),
		   page);

  /* use VBI for getting station names */
  widget = lookup_widget(page, "checkbutton7");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/vbi/use_vbi"));

  /* overlay subtitle pages automagically */
  widget = lookup_widget(page, "checkbutton12");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/vbi/auto_overlay"));

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
#ifdef HAVE_LIBZVBI
  /* Default subtitle page */
  widget = lookup_widget(page, "subtitle_page");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),
			    vbi_bcd2dec(zcg_int(NULL, "zvbi_page")));
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
  togglean enable_vbi, use_vbi;
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
  /* Use VBI station names */
  use_vbi = set_toggle (page, "checkbutton7",
			"/zapping/options/vbi/use_vbi");

  if (enable_vbi == TOGGLE_TO_FALSE)
    {
      /* XXX bad design */
      zvbi_reset_program_info ();
      zvbi_reset_network_info ();
    }
  else if (use_vbi == TOGGLE_TO_FALSE)
    {
      zvbi_reset_network_info ();
    }

  /* Overlay TTX pages automagically */
  set_toggle (page, "checkbutton12",
	      "/zapping/options/vbi/auto_overlay");

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

  widget = lookup_widget(page, "subtitle_page"); /* subtitle page */
  index =
    vbi_dec2bcd(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget)));
  if (index != zvbi_page)
    {
      zvbi_page = index;
      zcs_int(zvbi_page, "zvbi_page");
      osd_clear();
    }
#endif /* HAVE_LIBZVBI */
}

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

static void
add				(GtkDialog	*dialog)
{
  SidebarEntry device_info[] = {
    { N_("Device Info"), "gnome-info.png", "vbox9",
      di_setup, di_apply }
  };
  SidebarEntry general_options[] = {
    { N_("Main Window"), "gnome-session.png", "vbox35",
      mw_setup, mw_apply },
    { N_("Video"), "gnome-television.png", "vbox36",
      video_setup, video_apply }
  };
  SidebarEntry vbi_options[] = {
    { N_("General"), "gnome-monitor.png", "vbox17",
      vbi_general_setup, vbi_general_apply },
    { N_("Interactive TV"), "gnome-monitor.png", "vbox33",
      itv_setup, itv_apply }
  };
  SidebarGroup groups[] = {
    { N_("Device Info"), device_info, acount(device_info) },
    { N_("General Options"), general_options, acount(general_options) },
    { N_("VBI Options"), vbi_options, acount(vbi_options) }
  };

  standard_properties_add(dialog, groups, acount(groups),
			  "zapping.glade2");
}

void startup_properties_handler(void)
{
  property_handler handler = {
    add:	add
  };
  prepend_property_handler(&handler);

  cmd_register ("picture_size_cycle",
		py_picture_size_cycle, METH_VARARGS,
	       _("Cycle through the favoritve picture sizes"),
		"zapping.picture_size_cycle(+1)");
}

void shutdown_properties_handler(void)
{
  /* Nothing */
}

