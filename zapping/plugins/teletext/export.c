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

/* $Id: export.c,v 1.3 2004-12-07 17:30:43 mschimek Exp $ */

#include "config.h"

#define ZCONF_DOMAIN "/zapping/ttxview/"
#include "src/zconf.h"
#include "src/zmisc.h"
#include "export.h"

static GObjectClass *		parent_class;

/* Zconf name for the given export option, must be g_free()ed. */
static gchar *
xo_zconf_name			(const vbi3_export *	e,
				 const vbi3_option_info *oi)
{
  const vbi3_export_info *xi;

  xi = vbi3_export_info_from_export (e);
  g_assert (xi != NULL);

  return g_strdup_printf ("/zapping/options/export/%s/%s",
			  xi->keyword, oi->keyword);
}

static GtkWidget *
label_new			(const vbi3_option_info *oi)
{
  GtkWidget *label;
  GtkMisc *misc;
  gchar *buffer2;

  buffer2 = g_strconcat (oi->label, ":", NULL);
  label = gtk_label_new (buffer2);
  g_free (buffer2);

  misc = GTK_MISC (label);
  gtk_misc_set_alignment (misc, 1.0, 0.5);
  gtk_misc_set_padding (misc, 3, 0);

  return label;
}

static void
on_control_changed		(GtkWidget *		widget,
				 vbi3_export *		e)
{
  gchar *keyword;
  const vbi3_option_info *oi;
  vbi3_option_value val;
  gchar *zcname;

  g_assert (e != NULL);

  keyword = (gchar *) g_object_get_data (G_OBJECT (widget), "key");
  oi = vbi3_export_option_info_by_keyword (e, keyword);

  g_assert (oi != NULL);

  zcname = xo_zconf_name (e, oi);

  if (oi->menu.str)
    {
      val.num = (gint) g_object_get_data (G_OBJECT (widget), "index");
      vbi3_export_option_menu_set (e, keyword, val.num);
    }
  else
    {
      switch (oi->type)
	{
	case VBI3_OPTION_BOOL:
	  val.num = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	  if (vbi3_export_option_set (e, keyword, val))
	    zconf_set_boolean (val.num, zcname);
	  break;

	case VBI3_OPTION_INT:
	  val.num = (int) GTK_ADJUSTMENT (widget)->value;
	  if (vbi3_export_option_set (e, keyword, val))
	    zconf_set_int (val.num, zcname);
	  break;

	case VBI3_OPTION_REAL:
	  val.dbl = GTK_ADJUSTMENT (widget)->value;
	  if (vbi3_export_option_set (e, keyword, val))
	    zconf_set_float (val.dbl, zcname);
	  break;

	case VBI3_OPTION_STRING:
	  val.str = (gchar * ) gtk_entry_get_text (GTK_ENTRY (widget));
	  if (vbi3_export_option_set (e, keyword, val))
	    zconf_set_string (val.str, zcname);
	  break;

	default:
	  g_warning ("Unknown export option type %d in %s\n",
		     oi->type, __PRETTY_FUNCTION__);
	  break;
	}
    }

  g_free (zcname);
}

static void
create_menu			(GtkWidget *		table,
				 const vbi3_option_info *oi,
				 unsigned int		index,
				 vbi3_export *		e)
{
  GtkWidget *option_menu;
  GtkWidget *menu;
  gchar *zcname;
  guint saved;
  unsigned int i;

  option_menu = gtk_option_menu_new ();
  menu = gtk_menu_new ();

  zcname = xo_zconf_name (e, oi);
  saved = zconf_get_int (NULL, zcname);

  for (i = 0; i <= (unsigned int) oi->max.num; ++i)
    {
      gchar buf[32];
      GtkWidget *menu_item;

      switch (oi->type)
	{
	case VBI3_OPTION_BOOL:
	case VBI3_OPTION_INT:
	  g_snprintf (buf, sizeof (buf), "%d", oi->menu.num[i]);
	  menu_item = gtk_menu_item_new_with_label (buf);
	  break;

	case VBI3_OPTION_REAL:
	  g_snprintf (buf, sizeof (buf), "%f", oi->menu.dbl[i]);
	  menu_item = gtk_menu_item_new_with_label (buf);
	  break;

	case VBI3_OPTION_STRING:
	  menu_item = gtk_menu_item_new_with_label (oi->menu.str[i]);
	  break;

	case VBI3_OPTION_MENU:
	  menu_item = gtk_menu_item_new_with_label (oi->menu.str[i]);
	  break;

	default:
	  g_warning ("Unknown export option type %d in %s\n",
		     oi->type, __PRETTY_FUNCTION__);
	  continue;
	}

      z_object_set_const_data (G_OBJECT (menu_item), "key", oi->keyword);
      g_object_set_data (G_OBJECT (menu_item), "index", GINT_TO_POINTER (i));

      g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (on_control_changed), e);

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

      if (i == saved)
	on_control_changed (menu_item, e);
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
  zconf_create_int (oi->def.num, oi->tooltip, zcname);
  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), saved);
  z_tooltip_set (option_menu, oi->tooltip);

  g_free (zcname);

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (table), label_new (oi),
		    0, 1, index, index + 1,
                    (GtkAttachOptions)(GTK_FILL),
                    (GtkAttachOptions)(0), 3, 3);
  gtk_table_attach (GTK_TABLE (table), option_menu,
		    1, 2, index, index + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 3, 3);
}

static void
create_checkbutton		(GtkWidget *		table,
				 const vbi3_option_info *oi,
				 unsigned int		index,
				 vbi3_export *		e)
{
  GtkWidget *check_button;
  gchar *zcname;

  check_button = gtk_check_button_new_with_label (oi->label);
  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (check_button),
			      /* indicator */ FALSE);
  z_tooltip_set (check_button, oi->tooltip);
  z_object_set_const_data (G_OBJECT (check_button), "key", oi->keyword);
  g_signal_connect (G_OBJECT (check_button), "toggled",
		    G_CALLBACK (on_control_changed), e);

  zcname = xo_zconf_name (e, oi);
  zconf_create_boolean (oi->def.num, oi->tooltip, zcname);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
				zconf_get_boolean (NULL, zcname));
  g_free (zcname);

  on_control_changed (check_button, e);

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (table), check_button,
		    1, 2, index, index + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 3, 3);
}

static void
create_slider			(GtkWidget *		table,
				 const vbi3_option_info *oi,
				 unsigned int		index,
				 vbi3_export *		e)
{ 
  GtkObject *adj;
  GtkWidget *hscale;
  gchar *zcname;

  zcname = xo_zconf_name (e, oi);

  if (oi->type == VBI3_OPTION_INT)
    {
      adj = gtk_adjustment_new (oi->def.num, oi->min.num, oi->max.num,
				1, 10, 10);
      zconf_create_int (oi->def.num, oi->tooltip, zcname);
      gtk_adjustment_set_value (GTK_ADJUSTMENT (adj),
				zconf_get_int (NULL, zcname));
    }
  else
    {
      adj = gtk_adjustment_new (oi->def.dbl, oi->min.dbl, oi->max.dbl,
				1, 10, 10);
      zconf_create_float (oi->def.dbl, oi->tooltip, zcname);
      gtk_adjustment_set_value (GTK_ADJUSTMENT (adj),
				zconf_get_float (NULL, zcname));
    }

  g_free (zcname);

  z_object_set_const_data (G_OBJECT (adj), "key", oi->keyword);
  g_signal_connect (adj, "value-changed", G_CALLBACK (on_control_changed), e);

  on_control_changed ((GtkWidget *) adj, e);

  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adj));
  gtk_scale_set_value_pos (GTK_SCALE (hscale), GTK_POS_LEFT);
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  z_tooltip_set (hscale, oi->tooltip);

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (table), label_new (oi),
		    0, 1, index, index + 1,
                    (GtkAttachOptions)(GTK_FILL),
                    (GtkAttachOptions)(0), 3, 3);
  gtk_table_attach (GTK_TABLE (table), hscale,
		    1, 2, index, index + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 3, 3);
}

static void
create_entry			(GtkWidget *		table,
				 const vbi3_option_info *oi,
				 unsigned int		index,
				 vbi3_export *		e)
{ 
  GtkWidget *entry;
  gchar *zcname;

  entry = gtk_entry_new ();
  z_tooltip_set (entry, oi->tooltip);

  z_object_set_const_data (G_OBJECT (entry), "key", oi->keyword);
  g_signal_connect (G_OBJECT (entry), "changed", 
		    G_CALLBACK (on_control_changed), e);

  on_control_changed (entry, e);

  zcname = xo_zconf_name (e, oi);
  zconf_create_string (oi->def.str, oi->tooltip, zcname);
  gtk_entry_set_text (GTK_ENTRY (entry), zconf_get_string (NULL, zcname));
  g_free (zcname);

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (table), label_new (oi),
		    0, 1, index, index + 1,
                    (GtkAttachOptions)(GTK_FILL),
                    (GtkAttachOptions)(0), 3, 3);
  gtk_table_attach (GTK_TABLE (table), entry,
		    1, 2, index, index + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0), 3, 3);
}

static GtkWidget *
options_table_new		(vbi3_export *		e)
{
  GtkWidget *table;
  const vbi3_option_info *oi;
  unsigned int i;

  table = gtk_table_new (1, 2, FALSE);

  for (i = 0; (oi = vbi3_export_option_info_enum (e, (int) i)); ++i)
    {
      if (!oi->label)
	continue; /* not intended for user */

      if (oi->menu.str)
	{
	  create_menu (table, oi, i, e);
	}
      else
	{
	  switch (oi->type)
	    {
	    case VBI3_OPTION_BOOL:
	      create_checkbutton (table, oi, i, e);
	      break;

	    case VBI3_OPTION_INT:
	    case VBI3_OPTION_REAL:
	      create_slider (table, oi, i, e);
	      break;

	    case VBI3_OPTION_STRING:
	      create_entry (table, oi, i, e);
	      break;

	    default:
	      g_warning ("Unknown export option type %d in %s",
			 oi->type, __PRETTY_FUNCTION__);
	      continue;
	    }
	}
    }

  return table;
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

  if (vbi3_export_option_info_enum (sp->context, 0))
    {
      GtkWidget *frame;
      GtkWidget *table;

      frame = gtk_frame_new (_("Options"));
      table = options_table_new (sp->context);
      gtk_container_add (GTK_CONTAINER (frame), table);
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
    unsigned int i;

    menu = gtk_menu_new ();
    gtk_option_menu_set_menu (GTK_OPTION_MENU (sp->format_menu), menu);

    zconf_get_string (&format, "/zapping/options/export_format");

    for (i = 0; (xm = vbi3_export_info_enum ((int) i)); ++i)
      if (xm->label) /* user module */
	{
	  GtkWidget *menu_item;

	  menu_item = gtk_menu_item_new_with_label (xm->label);
	  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

	  if (xm->tooltip)
	    z_tooltip_set (menu_item, xm->tooltip);

	  z_object_set_const_data (G_OBJECT (menu_item), "key", xm->keyword);

	  if (i == 0 || (format && 0 == strcmp (xm->keyword, format)))
	    {
	      on_menu_activate (menu_item, sp);
	      gtk_option_menu_set_history (GTK_OPTION_MENU (sp->format_menu),
					   i);
	    }

	  g_signal_connect (G_OBJECT (menu_item), "activate",
			    G_CALLBACK (on_menu_activate), sp);
	}

    g_free (format);
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
