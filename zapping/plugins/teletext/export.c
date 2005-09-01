/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2000, 2001, 2002 Iñaki García Etxebarria
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: export.c,v 1.6 2005-09-01 01:37:57 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define ZCONF_DOMAIN "/zapping/ttxview/"
#include "src/zconf.h"
#include "src/zmisc.h"
#include "src/zvbi.h"
#include "export.h"

static GObjectClass *		parent_class;

/* Zconf name for the given export option, must be g_free()ed. */
static gchar *
xo_zconf_name			(const vbi3_export *	e,
				 const vbi3_option_info *oi,
				 gpointer		user_data)
{
  const vbi3_export_info *xi;

  user_data = user_data;

  xi = vbi3_export_info_from_export (e);
  g_assert (xi != NULL);

  return g_strdup_printf ("/zapping/options/export/%s/%s",
			  xi->keyword, oi->keyword);
}

static gchar *
default_filename		(ExportDialog *		sp)
{
  const vbi3_export_info *xi;
  gchar **extensions;
  gchar *filename;
  vbi3_subno subno;

  xi = vbi3_export_info_from_export (sp->context);
  extensions = g_strsplit (xi->extension, ",", 2);

  subno = sp->pg->subno;

  if (subno > 0 && subno <= 0x99)
    filename = g_strdup_printf ("%s-%x-%x.%s", sp->network,
				sp->pg->pgno, subno,
				extensions[0]);
  else
    filename = g_strdup_printf ("%s-%x.%s", sp->network,
				sp->pg->pgno,
				extensions[0]);

  g_strfreev (extensions);

  return filename;
}

static void
on_menu_activate		(GtkWidget *		menu_item,
				 ExportDialog *		sp)
{
  gchar *keyword;
  GtkContainer *container;
  GList *glist;
  GtkWidget *table;

  keyword = (gchar *) g_object_get_data (G_OBJECT (menu_item), "key");
  g_assert (keyword != NULL);
  zconf_set_string (keyword, "/zapping/options/export_format");

  if (sp->context)
    vbi3_export_delete (sp->context);
  sp->context = vbi3_export_new (keyword, NULL);
  g_assert (sp->context != NULL);

  /* Don't care if these fail */
  vbi3_export_option_set (sp->context, "network", sp->network);
  vbi3_export_option_set (sp->context, "creator", "Zapzilla " VERSION);
  vbi3_export_option_set (sp->context, "reveal", sp->reveal);

  {
    const vbi3_export_info *xi;
    gchar **extensions;

    xi = vbi3_export_info_from_export (sp->context);
    extensions = g_strsplit (xi->extension, ",", 2);
    z_electric_replace_extension (sp->entry, extensions[0]);
    g_strfreev (extensions);
  }

  container = GTK_CONTAINER (sp->option_box);
  while ((glist = gtk_container_get_children (container)))
    gtk_container_remove (container, GTK_WIDGET (glist->data));

  table = zvbi_export_option_table_new (sp->context,
					xo_zconf_name,
					/* user_data */ NULL);
  if (NULL != table)
    {
      GtkWidget *box;
      GtkWidget *frame;

      box = gtk_hbox_new (/* homogeneous */ FALSE, /* spacing */ 0);
      gtk_container_add (GTK_CONTAINER (box), table);
      gtk_container_set_border_width (GTK_CONTAINER (box), 6);
      frame = gtk_frame_new (_("Options"));
      gtk_container_add (GTK_CONTAINER (frame), box);
      gtk_widget_show_all (frame);
      gtk_box_pack_start (GTK_BOX (sp->option_box), frame, TRUE, TRUE, 0);
    }
}

static void
on_cancel_clicked		(GtkWidget *		widget,
				 gpointer 		user_data _unused_)
{
  while (widget->parent)
    widget = widget->parent;

  gtk_widget_destroy (widget);
}

static void
on_ok_clicked			(GtkWidget *		button,
				 ExportDialog *		sp)
{
  const gchar *cname;
  gchar *name;
  gchar *dirname;

  name = NULL;
  dirname = NULL;

  cname = gtk_entry_get_text (GTK_ENTRY (sp->entry));

  if (!cname || !*cname)
    {
      gtk_window_present (GTK_WINDOW (sp));
      gtk_widget_grab_focus (sp->entry);
      return;
    }

  name = g_strdup (cname);

  if (!z_overwrite_file_dialog (GTK_WINDOW (sp),
				_("Could not save page"),
				name))
    goto failure;

  g_strstrip (name);

  dirname = g_path_get_dirname (name);

  if (0 != strcmp (dirname, ".") || '.' == name[0])
    {
      if (!z_build_path_with_alert (GTK_WINDOW (sp), dirname))
	goto failure;

      /* XXX make absolute path? */
      zcs_char (dirname, "exportdir");
    }
  else
    {
      zcs_char ("", "exportdir");
    }
  
  if (!vbi3_export_file (sp->context, name, sp->pg))
    {
      z_show_non_modal_message_dialog
	(GTK_WINDOW (sp),
	 GTK_MESSAGE_ERROR,
	 _("Could not save page"), "%s",
	 vbi3_export_errstr (sp->context));

      goto failure;
    }

  g_free (name);
  g_free (dirname);

  on_cancel_clicked (button, sp);

  return;

 failure:
  g_free (name);
  g_free (dirname);
}

static void
instance_finalize		(GObject *		object)
{
  ExportDialog *t = EXPORT_DIALOG (object);

  if (t->context)
    vbi3_export_delete (t->context);

  g_free (t->network);

  vbi3_page_delete (t->pg);

  parent_class->finalize (object);
}

static void
instance_init			(GTypeInstance *	instance,
				 gpointer		g_class _unused_)
{
  ExportDialog *sp = (ExportDialog *) instance;
  GtkWindow *window;
  GtkBox *vbox;
  GtkBox *hbox;
  GtkWidget *file_entry;
  GtkWidget *widget;

  window = GTK_WINDOW (sp);
  gtk_window_set_title (window, _("Save as"));

  widget = gtk_vbox_new (FALSE, 3);
  gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
  vbox = GTK_BOX (widget);
  gtk_box_pack_start (GTK_BOX (sp->dialog.vbox), widget, TRUE, TRUE, 0);

  file_entry = gnome_file_entry_new ("ttxview_export_id", NULL);
  gtk_widget_set_size_request (file_entry, 400, -1);
  sp->entry = gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (file_entry));
  gtk_entry_set_activates_default (GTK_ENTRY (sp->entry), TRUE);
  gtk_box_pack_start (vbox, file_entry, FALSE, FALSE, 0);

  {
    widget = gtk_hbox_new (FALSE, 0);
    hbox = GTK_BOX (widget);
    gtk_box_pack_start (vbox, widget, FALSE, FALSE, 0);

    widget = gtk_label_new (_("Format:"));
    gtk_misc_set_padding (GTK_MISC (widget), 3, 0);
    gtk_box_pack_start (hbox, widget, FALSE, FALSE, 0);

    sp->format_menu = gtk_option_menu_new ();
    gtk_box_pack_start (hbox, sp->format_menu, TRUE, TRUE, 0);
  }

  sp->option_box = gtk_hbox_new (FALSE, 0);
  gtk_box_pack_start (vbox, sp->option_box, TRUE, TRUE, 0);

  {
    GtkWidget *menu;
    gchar *format;
    const vbi3_export_info *xm;
    guint count;
    guint i;

    menu = gtk_menu_new ();
    gtk_option_menu_set_menu (GTK_OPTION_MENU (sp->format_menu), menu);

    zconf_get_string (&format, "/zapping/options/export_format");

    count = 0;

    for (i = 0; (xm = vbi3_export_info_enum ((int) i)); ++i)
      {
	if (xm->label && !xm->open_format) /* user module, not subtitles */
	  {
	    GtkWidget *menu_item;

	    menu_item = gtk_menu_item_new_with_label (xm->label);
	    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

	    if (xm->tooltip)
	      z_tooltip_set (menu_item, xm->tooltip);

	    z_object_set_const_data (G_OBJECT (menu_item), "key", xm->keyword);

	    if (0 == count || (format && 0 == strcmp (xm->keyword, format)))
	      {
		on_menu_activate (menu_item, sp);
		gtk_option_menu_set_history (GTK_OPTION_MENU (sp->format_menu),
					     count);
	      }

	    g_signal_connect (G_OBJECT (menu_item), "activate",
			      G_CALLBACK (on_menu_activate), sp);

	    ++count;
	  }
      }

    g_free (format);
    format = NULL;
  }

  widget = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
  gtk_dialog_add_action_widget (&sp->dialog, widget, 1);
  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (widget), "clicked",
  		    G_CALLBACK (on_cancel_clicked), sp);

  widget = gtk_button_new_from_stock (GTK_STOCK_OK);
  gtk_dialog_add_action_widget (&sp->dialog, widget, 2);
  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);

  g_signal_connect (G_OBJECT (widget), "clicked",
  		    G_CALLBACK (on_ok_clicked), sp);

  gtk_dialog_set_default_response (&sp->dialog, 2);

  gtk_widget_grab_focus (sp->entry);
}

GtkWidget *
export_dialog_new		(const vbi3_page *	pg,
				 const gchar *		network,
				 gboolean		reveal)
{
  ExportDialog *sp;

  sp = (ExportDialog *) g_object_new (TYPE_EXPORT_DIALOG, NULL);

  sp->pg = vbi3_page_dup (pg);
  g_assert (NULL != sp->pg);

  sp->reveal = reveal;
  sp->network = g_strdup (network);

  {
    gchar *base;
    gchar *path;

    base = default_filename (sp);
    z_electric_set_basename (sp->entry, base);

    path = g_build_filename (zcg_char (NULL, "exportdir"), base, NULL);
    gtk_entry_set_text (GTK_ENTRY (sp->entry), path);

    g_free (base);
  }

  return GTK_WIDGET (sp);
}

static void
class_init			(gpointer		g_class,
				 gpointer		class_data _unused_)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (g_class);
  parent_class = g_type_class_peek_parent (object_class);

  object_class->finalize = instance_finalize;
}

GType
export_dialog_get_type		(void)
{
  static GType type = 0;
  
  if (!type)
    {
      GTypeInfo info;

      CLEAR (info);

      info.class_size = sizeof (ExportDialogClass);
      info.class_init = class_init;
      info.instance_size = sizeof (ExportDialog);
      info.instance_init = instance_init;

      type = g_type_register_static (GTK_TYPE_DIALOG,
				     "ExportDialog",
				     &info, (GTypeFlags) 0);
    }

  return type;
}
