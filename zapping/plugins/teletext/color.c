/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2001 Iñaki García Etxebarria
 * Copyright (C) 2003 Michael H. Schimek
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

/* $Id: color.c,v 1.1 2004-09-22 21:29:07 mschimek Exp $ */

#define ZCONF_DOMAIN "/zapping/ttxview/"
#include "zconf.h"
#include "zmisc.h"
#include "zvbi.h"

#include "main.h"
#include "color.h"

/*
 *  Color dialog
 *
 *  Currently just text brightness and contrast, in the future possibly
 *  also Caption default colors, overriding standard white on blank.
 */

static void
on_control_changed		(GtkWidget *		adj,
				 gpointer		user_data)
{
  vbi_decoder *vbi;
  int value;

  vbi = zvbi_get_object ();
  if (!vbi)
    return;

  switch (GPOINTER_TO_INT (user_data))
    {
    case 0:
      value = GTK_ADJUSTMENT (adj)->value;
      value = SATURATE (value, 0, 255);
      zconf_set_int (value, "/zapping/options/text/brightness");
      vbi_set_brightness (vbi, value);
      break;

    case 1:
      value = GTK_ADJUSTMENT (adj)->value;
      value = SATURATE (value, -128, +127);
      zconf_set_int (value, "/zapping/options/text/contrast");
      vbi_set_contrast (vbi, value);
      break;
    }

  zmodel_changed (color_zmodel);
}

static gboolean
on_key_press			(GtkWidget *		widget,
				 GdkEventKey *		event,
				 gpointer		data _unused_)
{
  switch (event->keyval)
    {
    case GDK_Escape:
      gtk_widget_destroy (widget);
      return TRUE; /* handled */

    case GDK_c:
    case GDK_C:
      if (event->state & GDK_CONTROL_MASK)
	{
	  gtk_widget_destroy (widget);
	  return TRUE; /* handled */
	}

    default:
      break;
    }

  return FALSE; /* don't know, pass on */
}

static void
instance_init			(GTypeInstance *	instance,
				 gpointer		g_class _unused_)
{
  ColorDialog *t = (ColorDialog *) instance;
  GtkWidget *widget;
  GtkContainer *container;
  GtkWidget *table;
  GtkObject *adj;
  gint value;

  gtk_window_set_title (&t->window, _("Text Color"));

  g_signal_connect (G_OBJECT (t), "key-press-event",
		    G_CALLBACK (on_key_press), NULL);

  widget = gtk_vbox_new (FALSE, 0);
  container = GTK_CONTAINER (widget);
  gtk_container_set_border_width (container, 6);
  gtk_container_add (GTK_CONTAINER (&t->window), widget);

  table = gtk_table_new (2, 2, FALSE);
  gtk_container_add (container, table);

  {
    widget = gtk_image_new_from_stock ("zapping-brightness",
				       GTK_ICON_SIZE_BUTTON);
    gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
    widget = z_tooltip_set_wrap (widget, _("Brightness"));

    gtk_table_attach_defaults (GTK_TABLE (table), widget, 0, 1, 0, 1);

    zconf_get_int (&value, "/zapping/options/text/brightness");
    adj = gtk_adjustment_new (value, 0.0, 255.0, 1.0, 16.0, 16.0);
    widget = z_spinslider_new (GTK_ADJUSTMENT (adj), NULL, NULL, 128.0, 0);
    z_spinslider_set_value (widget, value);

    gtk_table_attach (GTK_TABLE (table), widget, 1, 2, 0, 1,
		      (GtkAttachOptions)(GTK_FILL | GTK_EXPAND),
		      (GtkAttachOptions)(0), 3, 3);

    g_signal_connect (G_OBJECT (adj), "value-changed",
		      G_CALLBACK (on_control_changed),
		      GINT_TO_POINTER (0));
  }

  {
    widget = gtk_image_new_from_stock ("zapping-contrast",
				       GTK_ICON_SIZE_BUTTON);
    gtk_misc_set_alignment (GTK_MISC (widget), 1.0, 0.5);
    widget = z_tooltip_set_wrap (widget, _("Contrast"));

    gtk_table_attach_defaults (GTK_TABLE (table), widget, 0, 1, 1, 2);

    zconf_get_int (&value, "/zapping/options/text/contrast");
    adj = gtk_adjustment_new (value, -128.0, +127.0, 1.0, 16.0, 16.0);
    widget = z_spinslider_new (GTK_ADJUSTMENT (adj), NULL, NULL, 64.0, 0);
    z_spinslider_set_value (widget, value);

    gtk_table_attach (GTK_TABLE (table), widget, 1, 2, 1, 2,
		      (GtkAttachOptions)(GTK_FILL | GTK_EXPAND),
		      (GtkAttachOptions)(0), 3, 3);

    g_signal_connect (G_OBJECT (adj), "value-changed",
		      G_CALLBACK (on_control_changed),
		      GINT_TO_POINTER (1));
  }
}

GtkWidget *
color_dialog_new		(void)
{
  return GTK_WIDGET (g_object_new (TYPE_COLOR_DIALOG, NULL));
}

GType
color_dialog_get_type		(void)
{
  static GType type = 0;
  
  if (!type)
    {
      GTypeInfo info;

      CLEAR (info);

      info.class_size = sizeof (ColorDialogClass);
      info.instance_size = sizeof (ColorDialog);
      info.instance_init = instance_init;

      type = g_type_register_static (GTK_TYPE_WINDOW,
				     "ColorDialog",
				     &info, (GTypeFlags) 0);
    }

  return type;
}
