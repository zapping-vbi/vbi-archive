/*
 * RTE (Real time encoder) front end for Zapping
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
 *
 * Export options cloned & extended by Michael H. Schimek
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

/* $Id: options.c,v 1.6 2001-10-19 06:57:09 mschimek Exp $ */

#include "plugin_common.h"

#ifdef HAVE_LIBRTE

#include <glade/glade.h>
#include <rte.h>

#include "mpeg.h"

typedef struct grte_options {
  rte_context *         context;
  rte_codec *		codec;

  GtkWidget *		table;
  GnomePropertyBox *	propertybox;
} grte_options;

static void
grte_options_destroy (grte_options *opts)
{
  g_free (opts);
}

static GtkWidget *
ro_label_new (rte_option *ro)
{
  GtkWidget *label;
  gchar *s;

  s = g_strdup_printf ("%s:", _(ro->label));
  label = gtk_label_new (s);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label), 3, 3);
  gtk_widget_show (label);
  g_free (s);

  return label;
}

static void
do_option_control (GtkWidget *w, gpointer user_data)
{
  grte_options *opts = (grte_options *) user_data;
  char *keyword = (char *) gtk_object_get_data (GTK_OBJECT (w), "key");
  GtkLabel *label;
  rte_option *ro;
  rte_option_value val;
  gchar *zcname, *str;
  gint num;
  char *s;

  g_assert (opts && keyword);

  if (!opts->context || !opts->codec
      || !(ro = rte_option_by_keyword (opts->codec, keyword)))
    return;

  /* XXX rte_option_set errors ignored */

  if (ro->entries > 0)
    {
      val.num = (gint) gtk_object_get_data (GTK_OBJECT (w), "idx");
      rte_option_set_menu (opts->codec, ro->keyword, val.num);
    }
  else
    {
      switch (ro->type)
	{
          case RTE_OPTION_BOOL:
	    val.num = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w));
	    break;
          case RTE_OPTION_INT:
          case RTE_OPTION_REAL:
	    g_assert ((label = (GtkLabel *)
		       gtk_object_get_data (GTK_OBJECT (w), "label")));
	    if (ro->type == RTE_OPTION_INT)
	      val.num = GTK_ADJUSTMENT (w)->value;
	    else
	      val.dbl = GTK_ADJUSTMENT (w)->value;
	    s = rte_option_print(opts->codec, ro->keyword, val);
	    gtk_label_set_text (label, s);
	    free (s);
	    break;
          case RTE_OPTION_MENU:
	    g_assert_not_reached();
	    break;
          case RTE_OPTION_STRING:
	    val.str = gtk_entry_get_text (GTK_ENTRY (w));
	    break;
          default:
	    g_warning ("Type %d of RTE option %s is not supported",
		       ro->type, ro->keyword);
	}

      rte_option_set (opts->codec, ro->keyword, val);
    }
}

static void
on_option_control (GtkWidget *w, gpointer user_data)
{
  grte_options *opts = (grte_options *) user_data;

  do_option_control (w, user_data);

  if (opts->propertybox)
    gnome_property_box_changed (opts->propertybox);
}

static void
on_reset_slider (GtkWidget *w, gpointer user_data)
{
  grte_options *opts = (grte_options *) user_data;
  GtkWidget *adj = (GtkWidget *) gtk_object_get_data (GTK_OBJECT (w), "adj");
  char *keyword = (char *) gtk_object_get_data (GTK_OBJECT (w), "key");
  rte_option *ro;

  g_assert (opts && adj && keyword);

  if (!(ro = rte_option_by_keyword (opts->codec, keyword)))
    return;

  gtk_adjustment_set_value (GTK_ADJUSTMENT (adj),
			    (ro->type == RTE_OPTION_INT) ?
			     ro->def.num : ro->def.dbl);

  on_option_control (adj, opts);
}

static void
create_entry (grte_options *opts, rte_option *ro, int index)
{ 
  GtkWidget *label;
  GtkWidget *entry;
  rte_option_value val;

  label = ro_label_new (ro);

  entry = gtk_entry_new ();
  set_tooltip (entry, _(ro->tooltip));
  gtk_widget_show (entry);

  g_assert (rte_option_get (opts->codec, ro->keyword, &val));
  gtk_entry_set_text (GTK_ENTRY (entry), val.str);
  free (val.str);

  gtk_object_set_data (GTK_OBJECT (entry), "key", ro->keyword);
  gtk_signal_connect (GTK_OBJECT (entry), "changed",
		      GTK_SIGNAL_FUNC (on_option_control), opts);

  do_option_control (entry, opts);

  gtk_table_resize (GTK_TABLE (opts->table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (opts->table), label, 0, 1, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
  gtk_table_attach (GTK_TABLE (opts->table), entry, 1, 3, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions) (0), 3, 3);
}

static void
create_menu (grte_options *opts, rte_option *ro, int index)
{
  GtkWidget *label; /* This shows what the menu is for */
  GtkWidget *option_menu; /* The option menu */
  GtkWidget *menu; /* The menu displayed */
  GtkWidget *menu_item; /* Each of the menu items */
  int current;
  gint i;

  label = ro_label_new (ro);

  option_menu = gtk_option_menu_new ();
  menu = gtk_menu_new ();

  g_assert (ro->entries > 0);

  if (!rte_option_get_menu (opts->codec, ro->keyword, &current))
    current = 0;

  for (i = 0; i < ro->entries; i++)
    {
      char *str;

      switch (ro->type) 
	{
	  case RTE_OPTION_BOOL:
	  case RTE_OPTION_INT:
	    str = rte_option_print (opts->codec, ro->keyword, ro->menu.num[i]);
	    break;
	  case RTE_OPTION_REAL:
	    str = rte_option_print (opts->codec, ro->keyword, ro->menu.dbl[i]);
	    break;
	  case RTE_OPTION_STRING:
	    str = rte_option_print (opts->codec, ro->keyword, ro->menu.str[i]);
	    break;
	  case RTE_OPTION_MENU:
	    str = rte_option_print (opts->codec, ro->keyword, i);
	    break;
	  default:
	    g_warning ("Type %d of RTE option %s is not supported",
		       ro->type, ro->keyword);
	    abort();
	}

      g_assert(str != NULL);
      menu_item = gtk_menu_item_new_with_label (str);
      free(str);

      gtk_object_set_data (GTK_OBJECT (menu_item), "key", ro->keyword);
      gtk_object_set_data (GTK_OBJECT (menu_item), "idx", GINT_TO_POINTER (i));
      gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
			  GTK_SIGNAL_FUNC (on_option_control), opts);

      gtk_widget_show (menu_item);
      gtk_menu_append (GTK_MENU (menu), menu_item);

      if (current == i)
	do_option_control (menu_item, opts);
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), current);
  gtk_widget_show (menu);
  set_tooltip (option_menu, _(ro->tooltip));
  gtk_widget_show (option_menu);

  gtk_table_resize (GTK_TABLE (opts->table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (opts->table), label, 0, 1, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
  gtk_table_attach (GTK_TABLE (opts->table), option_menu, 1, 3,
                    index, index + 1,
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions) (0), 3, 3);
}

static void
create_slider (grte_options *opts, rte_option *ro, int index)
{ 
  GtkWidget *label1; /* This shows what the menu is for */
  GtkWidget *label2; /* Displays the current value */
  GtkWidget *hbox; /* Slider/button pair */
  GtkWidget *hscale;
  GtkWidget *button; /* Reset button */
  GtkObject *adj; /* Adjustment object for the slider */
  gfloat def, min, max, step, foo;
  rte_option_value val;
  char *s;

  label1 = ro_label_new (ro);

  s = rte_option_print (opts->codec, ro->keyword, ro->max);
  label2 = gtk_label_new (s);
  free (s);
  gtk_misc_set_alignment (GTK_MISC (label2), 1.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label2), 3, 0);
  gtk_widget_show (label2);

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_widget_show (hbox);

  g_assert (rte_option_get (opts->codec, ro->keyword, &val));

  if (ro->type == RTE_OPTION_INT)
    {
      def = ro->def.num; step = ro->step.num;
      min = ro->min.num; max = ro->max.num;
      val.dbl = val.num;
    }
  else
    {
      def = ro->def.dbl; step = ro->step.dbl;
      min = ro->min.dbl; max = ro->max.dbl;
    }

  foo = (max - min + step) / 10;
  if (0)
    fprintf(stderr, "slider %s: def=%f min=%f max=%f step=%f foo=%f cur=%f\n",
	    ro->keyword, (double) def, (double) min,
	    (double) max, (double) step, (double) foo, val.dbl);
  adj = gtk_adjustment_new (def, min, max + foo, step, step, foo);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (adj), val.dbl);
  gtk_object_set_data (GTK_OBJECT (adj), "key", ro->keyword);
  gtk_object_set_data (GTK_OBJECT (adj), "label", label2);
  gtk_signal_connect (GTK_OBJECT (adj), "value-changed",
		      GTK_SIGNAL_FUNC (on_option_control), opts);

  do_option_control (GTK_OBJECT (adj), opts);

  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adj));
  gtk_scale_set_draw_value (GTK_SCALE (hscale), FALSE);
  set_tooltip (hscale, _(ro->tooltip));
  gtk_widget_show (hscale);
  gtk_box_pack_start (GTK_BOX (hbox), hscale, TRUE, TRUE, 0);

  button = gtk_button_new_with_label (_("Reset"));
  gtk_object_set_data (GTK_OBJECT (button), "key", ro->keyword);
  gtk_object_set_data (GTK_OBJECT (button), "adj", adj);
  gtk_signal_connect (GTK_OBJECT (button), "pressed",
		      GTK_SIGNAL_FUNC (on_reset_slider), opts);
  gtk_widget_show (button);
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  gtk_table_resize (GTK_TABLE (opts->table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (opts->table), label1, 0, 1, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
  gtk_table_attach (GTK_TABLE (opts->table), label2, 1, 2,
                    index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
  gtk_table_attach (GTK_TABLE (opts->table), hbox, 2, 3,
                    index, index + 1,
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions) (0), 3, 3);
}

static void
create_checkbutton (grte_options *opts, rte_option *ro, int index)
{
  GtkWidget *cb;
  rte_option_value val;

  cb = gtk_check_button_new_with_label (_(ro->label));

  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (cb), FALSE);
  set_tooltip (cb, _(ro->tooltip));
  gtk_widget_show (cb);

  g_assert (rte_option_get (opts->codec, ro->keyword, &val));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb), val.num);

  gtk_object_set_data (GTK_OBJECT (cb), "key", ro->keyword);
  gtk_signal_connect (GTK_OBJECT (cb), "toggled",
		      GTK_SIGNAL_FUNC (on_option_control), opts);

  do_option_control (cb, opts);

  gtk_table_resize (GTK_TABLE (opts->table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (opts->table), cb, 1, 3, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
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
grte_options_create (rte_context *context, rte_codec *codec,
		     GnomePropertyBox *propertybox)
{
  GtkWidget *frame;
  grte_options *opts;
  rte_option *ro;
  gint index;
  int i;

  if (!rte_option_enum (codec, 0))
    return NULL; /* no options */

  opts = g_malloc (sizeof (*opts));

  opts->context = context;
  opts->codec = codec;
  opts->propertybox = propertybox;

  frame = gtk_frame_new (_("Options"));
  gtk_widget_show (frame);

  gtk_object_set_data_full (GTK_OBJECT (frame), "opts", opts,
    (GtkDestroyNotify) grte_options_destroy);

  opts->table = gtk_table_new (1, 3, FALSE);
  gtk_widget_show (opts->table);

  for (i = 0, index = 0; (ro = rte_option_enum (codec, i)); i++)
    {
      if (strcmp(ro->keyword, "coded_frame_rate") == 0)
	/* we'll override this */
#warning dont forget
	continue;
      else if (ro->entries > 0)
	create_menu (opts, ro, index++);
      else
	switch (ro->type)
	  {
	    case RTE_OPTION_BOOL:
	      create_checkbutton (opts, ro, index++);
	      break;
	    case RTE_OPTION_INT:
	    case RTE_OPTION_REAL:
	      create_slider (opts, ro, index++);
	      break;
	    case RTE_OPTION_MENU:
	      g_assert_not_reached ();
	      break;
	    case RTE_OPTION_STRING:
	      create_entry (opts, ro, index++);
	      break;
	    default:
	      g_warning ("Type %d of RTE option %s is not supported",
			 ro->type, ro->keyword);
	      continue;
	  }

      if (strcmp(ro->keyword, "audio_mode") == 0)
	{
          /* insert auto option */
	}
    }

  gtk_container_add (GTK_CONTAINER (frame), opts->table);

  return frame;
}

/*
 *  Set all codec options from the named zconf domain, keeps the previous
 *  value [default] if no conf data exists for the keyword. Returns boolean
 *  success, on failure options are only partially loaded.
 *
 *  XXX the zconf error handling is unsatisfactory. Keeping the default
 *  if nothing else is known seems ok, but I don't want to record crap
 *  because of some unreported problem (permissions, read error etc)
 *  (should display the error cause too)
 */
gboolean
grte_options_load (rte_codec *codec, gchar *zc_domain)
{
  rte_option *ro;
  int i;

  g_assert (codec && zc_domain);

  for (i = 0; (ro = rte_option_enum (codec, i)); i++)
    {
      gchar *zcname = g_strconcat (zc_domain, "/", ro->keyword, NULL);
      rte_option_value val;

      switch (ro->type)
	{
	  case RTE_OPTION_BOOL:
	    val.num = zconf_get_boolean (NULL, zcname);
	    break;
	  case RTE_OPTION_INT:
	  case RTE_OPTION_MENU:
	    val.num = zconf_get_integer (NULL, zcname);
	    break;
	  case RTE_OPTION_REAL:
	    val.dbl = zconf_get_float (NULL, zcname);
	    break;
          case RTE_OPTION_STRING:
	    val.str = zconf_get_string (NULL, zcname);
	    break;
          default:
	    g_warning ("Unknown option keyword %d in grte_load_options", ro->type);
	    break;
	}

      g_free (zcname);

      if (zconf_error ())
	continue;

      if (!rte_option_set (codec, ro->keyword, val))
	return FALSE;
    }

  return TRUE;
}

/*
 *  Inverse of grte_options_load.
 */
gboolean
grte_options_save (rte_codec *codec, gchar *zc_domain)
{
  rte_option *ro;
  int i;

  g_assert (codec && zc_domain);

  for (i = 0; (ro = rte_option_enum (codec, i)); i++)
    {
      gchar *zcname = g_strconcat (zc_domain, "/", ro->keyword, NULL);
      rte_option_value val;

      if (!rte_option_get (codec, ro->keyword, &val))
	return FALSE;

      switch (ro->type)
	{
	  case RTE_OPTION_BOOL:
	    zconf_create_boolean (val.num, _(ro->tooltip), zcname);
	    break;
	  case RTE_OPTION_INT:
	  case RTE_OPTION_MENU:
	    zconf_create_integer (val.num, _(ro->tooltip), zcname);
	    break;
	  case RTE_OPTION_REAL:
	    zconf_create_float (val.dbl, _(ro->tooltip), zcname);
	    break;
	  case RTE_OPTION_STRING:
	    zconf_create_string (val.str, _(ro->tooltip), zcname);
	    free (val.str);
	    break;
	  default:
	    g_warning ("Type %d of RTE option %s is not supported",
		       ro->type, ro->keyword);
	}

      g_free (zcname);
    }

  return TRUE;
}

#endif /* HAVE_LIBRTE */
