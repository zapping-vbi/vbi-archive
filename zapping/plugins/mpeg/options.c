/*
 * RTE (Real time encoder) front end for Zapping
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
 *
 * Export options cloned by Michael H. Schimek
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

/* $Id: options.c,v 1.2 2001-09-21 20:04:00 garetxe Exp $ */

#include "plugin_common.h"

#ifdef HAVE_LIBRTE

#include <glade/glade.h>
#include <rte.h>

#include "mpeg.h"

typedef struct grte_options {
  rte_context *		context;
  rte_codec *		codec;

  GtkWidget *		table;
  gchar *		zc_domain;

} grte_options;

static void
grte_options_destroy (grte_options *opts)
{
  g_free (opts->zc_domain);
  g_free (opts);
}

/* returns the zconf name for the given option, needs to be
   g_free'd by the caller */
static inline gchar *
ro_zconf_name (grte_options *opts, rte_option *ro)
{
  return g_strdup_printf ("%s/%s", opts->zc_domain, ro->keyword);
}

static void
on_option_control (GtkWidget *w, gpointer user_data)
{
  grte_options *opts =
    (grte_options *) gtk_object_get_data (GTK_OBJECT (w), "opts");
  gint id = GPOINTER_TO_INT (user_data);
  rte_option *ro;
  gchar *zcname, *str;
  int num;

  g_assert (opts != NULL);

  if (!opts->context || !opts->codec)
    return;

  ro = rte_enum_option (opts->context, opts->codec, id);

  g_assert (ro != NULL);

  zcname = ro_zconf_name (opts, ro);

  switch (ro->type)
    {
      case RTE_OPTION_BOOL:
	num = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w));
        if (rte_set_option (opts->context, opts->codec, ro->keyword, (int) num))
	  zconf_set_boolean (num, zcname);
	break;
      case VBI_EXPORT_INT:
	num = GTK_ADJUSTMENT (w)->value;
        if (rte_set_option (opts->context, opts->codec, ro->keyword, (int) num))
	  zconf_set_integer (num, zcname);
	break;
      case VBI_EXPORT_MENU:
        num = (gint) gtk_object_get_data (GTK_OBJECT (w), "value");
	if (rte_set_option (opts->context, opts->codec, ro->keyword, (int) num))
	  zconf_set_integer (num, zcname);
	break;
      case VBI_EXPORT_STRING:
	str = gtk_entry_get_text (GTK_ENTRY (w));
        if (rte_set_option (opts->context, opts->codec, ro->keyword, (char *) str))
	  zconf_set_string (str, zcname);
	break;
      default:
	g_warning ("Miracle of type %d in on_option_control", ro->type);
    }

  g_free (zcname);
}

static void
create_entry (grte_options *opts, rte_option *ro, int index)
{ 
  GtkWidget *label;
  GtkWidget *entry;
  gchar *zcname = ro_zconf_name (opts, ro);

  label = gtk_label_new (_(ro->label));
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  set_tooltip (entry, _(ro->tooltip));
  gtk_widget_show (entry);
  zconf_create_string (ro->def.str, ro->tooltip, zcname);
  gtk_entry_set_text (GTK_ENTRY (entry),
		      zconf_get_string (NULL, zcname));
  g_free (zcname);

  gtk_object_set_data (GTK_OBJECT (entry), "opts", opts);
  gtk_signal_connect (GTK_OBJECT (entry), "changed", 
		     GTK_SIGNAL_FUNC (on_option_control),
		     GINT_TO_POINTER (index));
  on_option_control (entry, GINT_TO_POINTER (index));

  gtk_table_resize (GTK_TABLE (opts->table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (opts->table), label, 0, 1, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
  gtk_table_attach (GTK_TABLE (opts->table), entry, 1, 2, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
}

static void
create_menu (grte_options *opts, rte_option *ro, int index)
{
  GtkWidget *label; /* This shows what the menu is for */
  GtkWidget *option_menu; /* The option menu */
  GtkWidget *menu; /* The menu displayed */
  GtkWidget *menu_item; /* Each of the menu items */
  gchar *zcname = ro_zconf_name (opts, ro); /* Path to the config key */
  gint i, saved;

  label = gtk_label_new (_(ro->label));
  gtk_widget_show (label);

  option_menu = gtk_option_menu_new ();
  menu = gtk_menu_new ();

  zconf_create_integer (ro->def.num, ro->tooltip, zcname);
  saved = zconf_get_integer (NULL, zcname);
  g_free (zcname);

  for (i = ro->min; i <= ro->max; i++)
    {
      menu_item = gtk_menu_item_new_with_label (_(ro->menu.label[i - ro->min]));

      gtk_object_set_data (GTK_OBJECT (menu_item), "opts", opts);
      gtk_object_set_data (GTK_OBJECT (menu_item), "value", 
			  GINT_TO_POINTER (i));
      gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
			 GTK_SIGNAL_FUNC (on_option_control),
			 GINT_TO_POINTER (index));

      gtk_widget_show (menu_item);
      gtk_menu_append (GTK_MENU (menu), menu_item);

      if (i == saved)
	on_option_control (menu_item, GINT_TO_POINTER (index));
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), saved);
  gtk_widget_show (menu);
  set_tooltip (option_menu, _(ro->tooltip));
  gtk_widget_show (option_menu);

  gtk_table_resize (GTK_TABLE (opts->table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (opts->table), label, 0, 1, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
  gtk_table_attach (GTK_TABLE (opts->table), option_menu, 1, 2,
                    index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
}

static void
create_slider (grte_options *opts, rte_option *ro, int index)
{ 
  GtkWidget *label;
  GtkWidget *hscale;
  GtkObject *adj; /* Adjustment object for the slider */
  gchar *zcname = ro_zconf_name (opts, ro);

  label = gtk_label_new (_(ro->label));
  gtk_widget_show (label);

  adj = gtk_adjustment_new (ro->def.num, ro->min, ro->max, 1, 10, 10);
  zconf_create_integer (ro->def.num, ro->tooltip, zcname);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (adj),
			    zconf_get_integer (NULL, zcname));
  g_free (zcname);

  gtk_object_set_data (GTK_OBJECT (adj), "opts", opts);
  gtk_signal_connect (adj, "value-changed", 
		     GTK_SIGNAL_FUNC (on_option_control),
		     GINT_TO_POINTER (index));
  on_option_control (GTK_WIDGET (adj), GINT_TO_POINTER (index));

  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adj));
  gtk_scale_set_value_pos (GTK_SCALE (hscale), GTK_POS_LEFT);
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  set_tooltip (hscale, _(ro->tooltip));
  gtk_widget_show (hscale);

  gtk_table_resize (GTK_TABLE (opts->table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (opts->table), label, 0, 1, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
  gtk_table_attach (GTK_TABLE (opts->table), hscale, 1, 2,
                    index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
}

static void
create_checkbutton (grte_options *opts, rte_option *ro, int index)
{
  GtkWidget *cb;
  gchar *zcname = ro_zconf_name (opts, ro);

  cb = gtk_check_button_new_with_label (_(ro->label));
  zconf_create_boolean (ro->def.num, ro->tooltip, zcname);

  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (cb), FALSE);
  set_tooltip (cb, _(ro->tooltip));
  gtk_widget_show (cb);

  gtk_object_set_data (GTK_OBJECT (cb), "opts", opts);
  gtk_signal_connect (GTK_OBJECT (cb), "toggled",
		     GTK_SIGNAL_FUNC (on_option_control),
		     GINT_TO_POINTER (index));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb),
				zconf_get_boolean (NULL, zcname));
  g_free (zcname);
  /* mhs: gtk_tbsa not enough? */
  on_option_control (cb, GINT_TO_POINTER (index));

  gtk_table_resize (GTK_TABLE (opts->table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (opts->table), cb, 1, 2, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
}

/*
 *  Create options for the rte_context/codec, returns NULL
 *  when the codec has no options.
 *
 *  + frame "Options"
 *    + table
 *      + option
 *      + option
 *      + option
 */
GtkWidget *
grte_options_create (rte_context *context, rte_codec *codec, gchar *zc_domain)
{
  GtkWidget *frame;
  grte_options *opts;
  rte_option *ro;
  int i;

  if (!rte_enum_option (context, codec, 0))
    return NULL; /* no options */

  opts = g_malloc (sizeof (*opts));

  opts->context = context;
  opts->codec = codec;
  opts->zc_domain = g_strdup(zc_domain);

  frame = gtk_frame_new ("Options");
  gtk_widget_show (frame);

  gtk_object_set_data_full (GTK_OBJECT (frame), "opts", opts,
    (GtkDestroyNotify) grte_options_destroy);

  opts->table = gtk_table_new (1, 2, FALSE);
  gtk_widget_show (opts->table);

  for (i = 0; (ro = rte_enum_option (context, codec, i)); i++)
    {
      switch (ro->type)
	{
	case RTE_OPTION_BOOL:
	  create_checkbutton (opts, ro, i);
	  break;
	case RTE_OPTION_INT:
	  create_slider (opts, ro, i);
	  break;
	case RTE_OPTION_MENU:
	  create_menu (opts, ro, i);
	  break;
	case RTE_OPTION_STRING:
	  create_entry (opts, ro, i);
	  break;

	default:
	  g_warning ("Type %d of RTE option %s is not supported",
		     ro->type, ro->keyword);
	  continue;
	}
    }

  gtk_container_add (GTK_CONTAINER (frame), opts->table);

  return frame;
}

#endif /* HAVE_LIBRTE */
