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

/* $Id: options.c,v 1.4 2001-10-16 11:17:09 mschimek Exp $ */

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
on_option_control (GtkWidget *w, gpointer user_data)
{
  grte_options *opts =
    (grte_options *) gtk_object_get_data (GTK_OBJECT (w), "opts");
  gint id = GPOINTER_TO_INT (user_data);
  GtkLabel *label;
  rte_option *ro;
  rte_option_value val;
  gchar *zcname, *str;
  gint num;
  char *s;

  g_assert (opts != NULL);

  if (!opts->context || !opts->codec)
    return;

  ro = rte_option_enum (opts->codec, id);

  g_assert (ro != NULL);

  zcname = ro_zconf_name (opts, ro);

  if (ro->entries > 0)
    {
      val.num = (gint) gtk_object_get_data (GTK_OBJECT (w), "value");

      if (!rte_option_set_menu (opts->codec, ro->keyword, val.num)
	  || !rte_option_get (opts->codec, ro->keyword, &val))
	goto failure;
    }
  else
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
	  /* not g_strdup */
	  val.str = strdup (gtk_entry_get_text (GTK_ENTRY (w)));
	  break;
        default:
	  g_warning ("Miracle of type %d in on_option_control", ro->type);
      }

  switch (ro->type)
    {
      case RTE_OPTION_BOOL:
	if (rte_option_set (opts->codec, ro->keyword, val.num))
	  zconf_set_boolean (val.num, zcname);
	break;
      case RTE_OPTION_INT:
      case RTE_OPTION_MENU:
	if (rte_option_set (opts->codec, ro->keyword, val.num))
	  zconf_set_integer (val.num, zcname);
	break;
      case RTE_OPTION_REAL:
	if (rte_option_set (opts->codec, ro->keyword, val.dbl))
	  zconf_set_float (val.dbl, zcname);
	break;
      case RTE_OPTION_STRING:
	if (rte_option_set (opts->codec, ro->keyword, val.str))
	  zconf_set_string (val.str, zcname);
	free (val.str);
	break;
      default:
	g_warning ("Type %d of RTE option %s is not supported",
		   ro->type, ro->keyword);
    }

 failure:

  g_free (zcname);
}

static void
create_entry (grte_options *opts, rte_option *ro, int index)
{ 
  GtkWidget *label;
  GtkWidget *entry;
  gchar *zcname = ro_zconf_name (opts, ro);

  label = ro_label_new (ro);

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
  gchar *zcname = ro_zconf_name (opts, ro); /* Path to the config key */
  gint i, saved;

  label = ro_label_new (ro);

  option_menu = gtk_option_menu_new ();
  menu = gtk_menu_new ();

  zconf_create_integer (ro->def.num, ro->tooltip, zcname);
  saved = zconf_get_integer (NULL, zcname);
  g_free (zcname);

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
  gtk_table_attach (GTK_TABLE (opts->table), option_menu, 1, 3,
                    index, index + 1,
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions) (0), 3, 3);
}

static void
create_slider (grte_options *opts, rte_option *ro, int index)
{ 
  GtkWidget *label1;
  //  GtkWidget *hbox;
  GtkWidget *label2;
  GtkWidget *hscale;
  GtkObject *adj; /* Adjustment object for the slider */
  gchar *zcname = ro_zconf_name (opts, ro);
  gfloat def, min, max, step;
  char *s;

  label1 = ro_label_new (ro);

  //  hbox = gtk_hbox_new (FALSE, 0);
  //  gtk_widget_show (hbox);

  s = rte_option_print (opts->codec, ro->keyword, ro->max);
  label2 = gtk_label_new (s);
  free (s);
  gtk_misc_set_alignment (GTK_MISC (label2), 1.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (label2), 3, 0);
  gtk_widget_show (label2);
  //  gtk_box_pack_start (GTK_BOX (hbox), label2, FALSE, TRUE, 0);

  if (ro->type == RTE_OPTION_INT)
    {
      def = ro->def.num; step = ro->step.num;
      min = ro->min.num; max = ro->max.num;
      zconf_create_integer (ro->def.num, ro->tooltip, zcname);
    }
  else
    {
      def = ro->def.dbl; step = ro->step.dbl;
      min = ro->min.dbl; max = ro->max.dbl;
      zconf_create_float (ro->def.dbl, ro->tooltip, zcname);
    }

  g_free (zcname);

  adj = gtk_adjustment_new (def, min, max, step, step * 10,
			    (max - min + step) / 10);
  gtk_adjustment_set_value (GTK_ADJUSTMENT (adj), ro->def.dbl);

  gtk_object_set_data (GTK_OBJECT (adj), "opts", opts);
  gtk_object_set_data (GTK_OBJECT (adj), "label", label2);
  gtk_signal_connect (adj, "value-changed",
		      GTK_SIGNAL_FUNC (on_option_control),
		      GINT_TO_POINTER (index));
  on_option_control (GTK_WIDGET (adj), GINT_TO_POINTER (index));

  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adj));
  gtk_scale_set_draw_value (GTK_SCALE (hscale), FALSE);
  set_tooltip (hscale, _(ro->tooltip));
  gtk_widget_show (hscale);
  //  gtk_box_pack_end (GTK_BOX (hbox), hscale, TRUE, TRUE, 0);

  gtk_table_resize (GTK_TABLE (opts->table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (opts->table), label1, 0, 1, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
  gtk_table_attach (GTK_TABLE (opts->table), label2, 1, 2,
                    index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
  gtk_table_attach (GTK_TABLE (opts->table), hscale, 2, 3,
                    index, index + 1,
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
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
grte_options_create (rte_context *context, rte_codec *codec, gchar *zc_domain)
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
  opts->zc_domain = g_strdup(zc_domain);

  frame = gtk_frame_new ("Options");
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
      gchar *zcname = g_strdup_printf ("%s/%s", zc_domain, ro->keyword);
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

gboolean
grte_options_save (rte_codec *codec, gchar *zc_domain)
{
  /* future grte_options_create will maintain codec config in rte_codec only */

  return FALSE;
}

#endif /* HAVE_LIBRTE */
