/*
 * RTE (Real time encoder) front end for Zapping
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
 *
 * Export options cloned & extended 2001 by Michael H. Schimek
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

/* $Id: options.c,v 1.26 2005-01-08 14:54:24 mschimek Exp $ */

/* XXX gtk+ 2.3 GtkOptionMenu -> ? */
#undef GTK_DISABLE_DEPRECATED

#include "src/plugin_common.h"

#ifdef HAVE_LIBRTE

#include <math.h>

#include <glade/glade.h>

#include "mpeg.h"
#include "src/properties.h"
#include "src/zspinslider.h"

typedef struct grte_options {
  rte_context *         context;
  rte_codec *		codec;

  GtkWidget *		table;
} grte_options;

static void
grte_options_destroy (grte_options *opts)
{
  g_free (opts);
}

static GtkWidget *
ro_label_new (rte_option_info *ro)
{
  GtkWidget *label;
  gchar *s;
  gchar *t;

  t = g_locale_to_utf8 (R_(ro->label), -1, NULL, NULL, NULL);
  g_assert (t != NULL);
  s = g_strdup_printf ("%s:", t);
  g_free (t);
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
  char *keyword = (char *) g_object_get_data (G_OBJECT (w), "key");
  rte_option_info *ro;
  rte_option_value val;

  g_assert (opts && keyword);

  if (!opts->context || !opts->codec
      || !(ro = rte_codec_option_info_by_keyword (opts->codec, keyword)))
    return;

  /* rte_option_set errors ignored */

  if (ro->menu.num)
    {
      val.num = z_object_get_int_data (G_OBJECT (w), "idx");
      rte_codec_option_menu_set (opts->codec, ro->keyword, val.num);
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
	    if (ro->type == RTE_OPTION_INT)
	      {
		val.num = rint(GTK_ADJUSTMENT (w)->value
			       / GTK_ADJUSTMENT (w)->step_increment);
		val.num *= ro->step.num;
	      }
	    else
	      {
		val.dbl = GTK_ADJUSTMENT (w)->value
		  * ro->step.dbl / GTK_ADJUSTMENT (w)->step_increment;
		val.dbl = rint(val.dbl / ro->step.dbl) * ro->step.dbl;
	      }
	    break;
          case RTE_OPTION_MENU:
	    g_assert_not_reached();
	    break;
          case RTE_OPTION_STRING:
	    val.str = (gchar *) gtk_entry_get_text (GTK_ENTRY (w));
	    break;
          default:
	    g_warning ("Type %d of RTE option %s is not supported",
		       ro->type, ro->keyword);
	}

      rte_codec_option_set (opts->codec, ro->keyword, val);
    }
}

static void
on_option_control (GtkWidget *w, gpointer user_data)
{
  do_option_control (w, user_data);

  /* Make sure we call z_property_item_modified for *widgets* */
  if (GTK_IS_WIDGET (w))
    z_property_item_modified (w);
  else if (GTK_IS_ADJUSTMENT (w))
    z_property_item_modified ((GtkWidget *) g_object_get_data
			      (G_OBJECT (w), "spinslider"));
}

static void
create_entry (grte_options *opts, rte_option_info *ro, guint index)
{ 
  GtkWidget *label;
  GtkWidget *entry;
  rte_option_value val;
  gchar *t;

  label = ro_label_new (ro);

  entry = gtk_entry_new ();
  if (ro->tooltip)
    {
      t = g_locale_to_utf8 (R_(ro->tooltip), -1, NULL, NULL, NULL);
      g_assert (t != NULL);
      z_tooltip_set (entry, t);
      g_free (t);
    }
  gtk_widget_show (entry);

  g_assert (rte_codec_option_get (opts->codec, ro->keyword, &val));
  gtk_entry_set_text (GTK_ENTRY (entry), val.str);
  free (val.str);

  g_object_set_data (G_OBJECT (entry), "key", ro->keyword);
  g_signal_connect (G_OBJECT (entry), "changed",
		      G_CALLBACK (on_option_control), opts);

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
create_menu (grte_options *opts, rte_option_info *ro, guint index)
{
  GtkWidget *label; /* This shows what the menu is for */
  GtkWidget *option_menu; /* The option menu */
  GtkWidget *menu; /* The menu displayed */
  GtkWidget *menu_item; /* Each of the menu items */
  int current;
  gint i;
  gchar *t;

  label = ro_label_new (ro);

  option_menu = gtk_option_menu_new ();
  menu = gtk_menu_new ();

  g_assert (ro->menu.num != NULL);

  if (!rte_codec_option_menu_get (opts->codec, ro->keyword, &current))
    current = 0;

  for (i = ro->min.num; i <= ro->max.num; i++)
    {
      char *str;

      switch (ro->type) 
	{
	  case RTE_OPTION_BOOL:
	  case RTE_OPTION_INT:
	    str = rte_codec_option_print (opts->codec, ro->keyword, ro->menu.num[i]);
	    break;
	  case RTE_OPTION_REAL:
	    str = rte_codec_option_print (opts->codec, ro->keyword, ro->menu.dbl[i]);
	    break;
	  case RTE_OPTION_STRING:
	    str = R_(ro->menu.str[i]);
	    break;
	  case RTE_OPTION_MENU:
	    str = rte_codec_option_print (opts->codec, ro->keyword, i);
	    break;
	  default:
	    g_warning ("Type %d of RTE option %s is not supported",
		       ro->type, ro->keyword);
	    abort();
	}

      g_assert(str != NULL);
      t = g_locale_to_utf8 (str, -1, NULL, NULL, NULL);
      g_assert (t != NULL);
      menu_item = gtk_menu_item_new_with_label (t);
      g_free (t);
      free(str);

      g_object_set_data (G_OBJECT (menu_item), "key", ro->keyword);
      g_object_set_data (G_OBJECT (menu_item), "idx", GINT_TO_POINTER (i));
      g_signal_connect (G_OBJECT (menu_item), "activate",
			  G_CALLBACK (on_option_control), opts);

      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

      if (current == i)
	do_option_control (menu_item, opts);
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu), (guint) current);
  gtk_widget_show (menu);
  if (ro->tooltip)
    {
      t = g_locale_to_utf8 (R_(ro->tooltip), -1, NULL, NULL, NULL);
      g_assert (t != NULL);
      z_tooltip_set (option_menu, t);
      g_free (t);
    }
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
create_slider (grte_options *opts, rte_option_info *ro, guint index)
{ 
  GtkWidget *label; /* This shows what the menu is for */
  GtkWidget *spinslider;
  GtkObject *adj; /* Adjustment object for the slider */
  gdouble def, min, max, step, big_step, div, maxp;
  rte_option_value val;
  char *s;
  gint digits = 0;

  label = ro_label_new (ro);
  s = rte_codec_option_print (opts->codec, ro->keyword, ro->max);
  maxp = strtod (s, &s);
  while (*s == ' ') s++;
  g_assert (rte_codec_option_get (opts->codec, ro->keyword, &val));
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
  div = maxp / max;
  if (div > 1.0)
    div = 1.0;
  big_step = (max - min + step) / 10;
  if (0)
    fprintf(stderr, "slider %s: def=%f min=%f max=%f step=%f foo=%f cur=%f\n",
	    ro->keyword, (double) def, (double) min,
	    (double) max, (double) step, (double) big_step, val.dbl);
  adj = gtk_adjustment_new (val.dbl * div, min * div, max * div,
			    step * div, big_step * div, big_step * div);
  /* Set decimal digits so that step increments < 1.0 become visible */
  if (GTK_ADJUSTMENT (adj)->step_increment == 0.0
      || (digits = floor (log10 (GTK_ADJUSTMENT (adj)->step_increment))) > 0)
    digits = 0;
  spinslider = z_spinslider_new (GTK_ADJUSTMENT (adj), NULL,
				 s, def * div, -digits);
  g_object_set_data (G_OBJECT (adj), "key", ro->keyword);
  g_object_set_data (G_OBJECT (adj), "spinslider", spinslider);
  g_signal_connect (G_OBJECT (adj), "value-changed",
		      G_CALLBACK (on_option_control), opts);
  gtk_widget_show (spinslider);
  gtk_table_resize (GTK_TABLE (opts->table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (opts->table), label, 0, 1, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);
  gtk_table_attach (GTK_TABLE (opts->table), spinslider, 1, 3, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions) (0), 3, 3);
}

static void
create_checkbutton (grte_options *opts, rte_option_info *ro, guint index)
{
  GtkWidget *cb;
  rte_option_value val;
  gchar *t;

  t = g_locale_to_utf8 (R_(ro->label), -1, NULL, NULL, NULL);
  g_assert (t != NULL);
  cb = gtk_check_button_new_with_label (t);
  g_free (t);

  gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (cb), FALSE);
  if (ro->tooltip)
    {
      t = g_locale_to_utf8 (R_(ro->tooltip), -1, NULL, NULL, NULL);
      g_assert (t != NULL);
      z_tooltip_set (cb, t);
      g_free (t);
    }
  gtk_widget_show (cb);

  g_assert (rte_codec_option_get (opts->codec, ro->keyword, &val));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb), val.num);

  g_object_set_data (G_OBJECT (cb), "key", ro->keyword);
  g_signal_connect (G_OBJECT (cb), "toggled",
		      G_CALLBACK (on_option_control), opts);

  do_option_control (cb, opts);

  gtk_table_resize (GTK_TABLE (opts->table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (opts->table), cb, 1, 3, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions) (0), 3, 3);
}

/**
 * grte_options_create:
 * @context: 
 * @codec: 
 * 
 * Create a tree of GtkWidgets containing all options of &rte_codec @codec
 * of &rte_context @context. Initially the option widgets will display the
 * respective CURRENT value of the &rte_codec option, you may need to reset
 * the options by other means first. DO NOT delete the @context or @codec
 * while Gtk may access this tree.
 *
 *  + frame "Options"
 *    + table (3 * n)
 *      + option (label || menu)
 *      + option (label | value | [ slider | reset ])
 *      + option (label || entry)
 *      + option (|| button)
 *
 * On any widget change, the respective &rte_codec option will be set.
 *
 * Return value: 
 * &GtkWidget pointer, %NULL when the @codec has no options. 
 *
 * BUGS:
 * No verification of user input, all rte errors will be ignored.
 * Undocumented filtering of options.
 **/
GtkWidget *
grte_options_create (rte_context *context, rte_codec *codec)
{
  GtkWidget *frame;
  grte_options *opts;
  rte_option_info *ro;
  guint index;
  unsigned int i;

  if (!rte_codec_option_info_enum (codec, 0))
    return NULL; /* no options */

  opts = g_malloc (sizeof (*opts));

  opts->context = context;
  opts->codec = codec;

  frame = gtk_frame_new (_("Options"));
  gtk_widget_show (frame);

  g_object_set_data_full (G_OBJECT (frame), "opts", opts,
    (GtkDestroyNotify) grte_options_destroy);

  opts->table = gtk_table_new (1, 3, FALSE);
  gtk_widget_show (opts->table);

  for (i = 0, index = 0; (ro = rte_codec_option_info_enum (codec, i)); i++)
    {
#ifdef LIBRTE4
      if (strcmp(ro->keyword, "coded_frame_rate") == 0)
	continue; /* we'll override this */
      else if (ro->entries > 0)
#else
      if (!ro->label)
	continue; /* experts only */
      else if (ro->menu.num)
#endif
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
 *  Config tree, rte 0.5+				rte 0.4
 *
 *  /zapping/plugins/record		<zc_root>	ZCONF_DOMAIN
 *  + ...
 *  + /configs						nil
 *    + / "user config name"		<zc_conf>	/ "default"
 *      + format = ...
 *      + audio_codec = ...
 *      + video_codec = ...
 *      + ...
 *      + / context	  } to do			nil
 *        + option = ...  }
 *      + / codecs					nil
 *        + / "codec name"				/ "codec name"
 *          + option = ...
 *        :
 *    :
 *
 *  Note this assumes a stream can consist of exactly one video
 *  and/or audio stream, which is a limitation of Zapping, not rte.
 *
 *  All functions accessing this tree below.
 */

#define CONFIGS "/configs/"
#define CODECS "/codecs/"

/**
 * grte_options_load:
 * @codec: 
 * @zc_domain:
 * 
 * Set the value of all options of #rte_codec @codec which are configured
 * in the Zapping config file under @zc_domain. All other options
 * remain unchanged (usually at their respective defaults).
 *
 * XXX the zconf error handling is unsatisfactory. Keeping the default
 * if nothing else is known seems ok, but I don't want to record crap
 * because of some unreported problem (permissions, read error etc)
 * (should display the error cause too)
 *
 * Return value: 
 * FALSE if an error ocurred, in this case options may be only
 * partially loaded.
 **/
static gboolean
grte_options_load		(rte_codec *		codec,
				 const gchar *		zc_domain)
{
  rte_option_info *ro;
  unsigned int i;

  g_assert (codec && zc_domain);

  for (i = 0; (ro = rte_codec_option_info_enum (codec, i)); i++)
    {
      gchar *zcname;
      rte_option_value val;

      if (!ro->label)
	continue; /* experts only */

      zcname = g_strconcat (zc_domain, "/", ro->keyword, NULL);

      switch (ro->type)
	{
	  case RTE_OPTION_BOOL:
	    val.num = zconf_get_boolean (NULL, zcname);
	    break;
	  case RTE_OPTION_INT:
	  case RTE_OPTION_MENU:
	    val.num = zconf_get_int (NULL, zcname);
	    break;
	  case RTE_OPTION_REAL:
	    val.dbl = zconf_get_float (NULL, zcname);
	    break;
          case RTE_OPTION_STRING:
	    val.str = (char *) zconf_get_string (NULL, zcname);
	    break;
          default:
	    g_warning ("Unknown option keyword %d in grte_load_options", ro->type);
	    break;
	}

      g_free (zcname);

      if (zconf_error ())
	continue;

      if (!rte_codec_option_set (codec, ro->keyword, val))
	return FALSE;
    }

  return TRUE;
}

/**
 * grte_options_save:
 * @codec: 
 * @zc_domain:
 * 
 * Save the current value of all options of #rte_codec @codec in the Zapping
 * config file under @zc_domain.
 *
 * Return value: 
 * FALSE if an error ocurred, in this case options may be only
 * partially saved.
 **/
static gboolean
grte_options_save		(rte_codec *		codec,
				 const gchar *		zc_domain)
{
  rte_option_info *ro;
  unsigned int i;
  gchar *t;

  g_assert (codec && zc_domain);

  for (i = 0; (ro = rte_codec_option_info_enum (codec, i)); i++)
    {
      gchar *zcname = g_strconcat (zc_domain, "/", ro->keyword, NULL);
      rte_option_value val;

      if (!rte_codec_option_get (codec, ro->keyword, &val))
	{
	  g_free(zcname);
	  return FALSE;
	}

      switch (ro->type)
	{
	  case RTE_OPTION_BOOL:
	    /* Create won't set an already existing variable,
	     * Set won't create one with description.
	     */
	    if (ro->tooltip)
	      {
		t = g_locale_to_utf8 (R_(ro->tooltip), -1, NULL, NULL, NULL);
		g_assert (t != NULL);
	      }
	    else
	      {
		t = NULL;
	      }
	    zconf_create_boolean (val.num, t, zcname);
	    g_free (t);
	    zconf_set_boolean (val.num, zcname);
	    break;
	  case RTE_OPTION_INT:
	  case RTE_OPTION_MENU:
	    if (ro->tooltip)
	      {
		t = g_locale_to_utf8 (R_(ro->tooltip), -1, NULL, NULL, NULL);
		g_assert (t != NULL);
	      }
	    else
	      {
		t = NULL;
	      }
	    zconf_create_int (val.num, t, zcname);
	    g_free (t);
	    zconf_set_int (val.num, zcname);
	    break;
	  case RTE_OPTION_REAL:
	    if (ro->tooltip)
	      {
		t = g_locale_to_utf8 (R_(ro->tooltip), -1, NULL, NULL, NULL);
		g_assert (t != NULL);
	      }
	    else
	      {
		t = NULL;
	      }
	    zconf_create_float (val.dbl, t, zcname);
	    g_free (t);
	    zconf_set_float (val.dbl, zcname);
	    break;
	  case RTE_OPTION_STRING:
	    if (ro->tooltip)
	      {
		t = g_locale_to_utf8 (R_(ro->tooltip), -1, NULL, NULL, NULL);
		g_assert (t != NULL);
	      }
	    else
	      {
		t = NULL;
	      }
	    zconf_create_string (val.str, t, zcname);
	    g_free (t);
	    zconf_set_string (val.str, zcname);
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

static const gchar *
codec_type_string[RTE_STREAM_MAX + 1] = {
  NULL,
  "video_codec",
  "audio_codec",
  "vbi_codec",
};

/**
 * grte_codec_create_menu:
 * @context: 
 * @stream_type: 
 * @zc_root: 
 * @zc_conf: 
 * @default_item: 
 * 
 * Create a #GtkMenu consisting of all codecs available for
 * #rte_context @context, of @stream_type (%RTE_STREAM_VIDEO or
 * %RTE_STREAM_AUDIO). The first menu item (number 0) will be "None".
 *
 * For all items except "None", a #GtkObject value "keyword" is allocated,
 * pointing to the respective #rte_codec_info keyword.
 *
 * When @default_item is given, this function queries the codec configured
 * for this @stream_type from the Zapping config file under @zc_root //
 * @zc_conf. Then @default_item is set to the item number listing said
 * codec, or item 0.
 * 
 * Return value: 
 * #GtkWidget pointer, %NULL if no codecs are available.
 *
 * BUGS:
 * The menu will always contain "None", even if the context offers
 * only one codec, or no suitable codec is available.
 **/
GtkWidget *
grte_codec_create_menu		(rte_context *		context,
				 const gchar *		zc_root,
				 const gchar *		zc_conf,
				 rte_stream_type	stream_type,
				 gint *			default_item)
{
  GtkWidget *menu, *menu_item;
  rte_context_info *cxinfo;
  rte_codec_info *cdinfo;
  gchar *zcname;
  const gchar *keyword = 0;
  gint base = 1, items = 0;
  int i;

  if (default_item)
    {
      zcname = g_strconcat (zc_root, CONFIGS, zc_conf, "/",
			    codec_type_string[stream_type], NULL);
      keyword = (char *) zconf_get_string (NULL, zcname);
      g_free (zcname);

      if (!keyword || !keyword[0])
	{
	  keyword = "";
	  *default_item = 0; /* "None" */
	}
      else
	{
	  *default_item = 1; /* first valid */
	}
    }

  menu = gtk_menu_new ();

  g_assert ((cxinfo = rte_context_info_by_context (context)));

  if (cxinfo->min_elementary[stream_type] != 1) /* "None" permitted? */
    {
      /* TRANSLATORS: Which codec: None, A, B, C, ... */
      menu_item = gtk_menu_item_new_with_label (_("No codec"));
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    }
  else
    {
      /* gtk_widget_set_sensitive (menu_item, FALSE); */
      if (default_item)
	*default_item = 0;
      base = 0;
    }

  // XXX it makes no sense to display a menu when there's
  // really no choice
  for (i = 0; (cdinfo = rte_codec_info_enum (context, (guint) i)); i++)
    {
      gchar *t;

      if (cdinfo->stream_type != stream_type)
        continue;

      t = g_locale_to_utf8 (R_(cdinfo->label), -1, NULL, NULL, NULL);
      g_assert (t != NULL);
      menu_item = gtk_menu_item_new_with_label (t);
      g_free (t);
      z_object_set_const_data (G_OBJECT (menu_item), "keyword",
			       cdinfo->keyword);
      if (cdinfo->tooltip)
	{
	  t = g_locale_to_utf8 (R_(cdinfo->tooltip), -1, NULL, NULL, NULL);
	  g_assert (t != NULL);
	  z_tooltip_set (menu_item, t);
	  g_free (t);
	}
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

      if (default_item)
	if (strcmp (keyword, cdinfo->keyword) == 0)
	  *default_item = base + items;

      items++;
    }

  return menu;
}

/**
 * grte_num_codecs:
 * @context: 
 * @stream_type: 
 * @info_p: 
 * 
 * Return value: 
 * Number of codecs of @stream_type available for @context. When
 * @info_p is not %NULL and number of codecs is 1, a pointer to
 * its #rte_codec_info is stored here.
 **/
gint
grte_num_codecs			(rte_context *		context,
				 rte_stream_type	stream_type,
				 rte_codec_info **	info_p)
{
  rte_codec_info *info;
  gint count, i;

  if (!info_p)
    info_p = &info;

  count = 0;

  for (i = 0; (*info_p = rte_codec_info_enum (context, (guint) i)); i++)
    if ((*info_p)->stream_type == stream_type)
      count++;

  return count;
}

/**
 * grte_codec_load:
 * @context: 
 * @zc_root: 
 * @zc_conf: 
 * @keyword: 
 * @stream_type: 
 * 
 * Set the #rte_codec for @stream_type (%RTE_STREAM_AUDIO or %RTE_STREAM_VIDEO)
 * of #rte_context @context named by @keyword and configure according to the
 * Zapping config file under @zc_root // @zc_conf. When @keyword is
 * %NULL, the #rte_codec configured in the Zapping config file will be set.
 *
 * Return value: 
 * #rte_codec pointer (as returned by rte_codec_set()) or %NULL if no codec
 * has been configured for this %stream_type or another error ocurred.
 **/
rte_codec *
grte_codec_load			(rte_context *		context,
				 const gchar *		zc_root,
				 const gchar *		zc_conf,
				 rte_stream_type	stream_type,
				 const gchar *		keyword)
{
  rte_codec *codec = NULL;
  gchar *zcname;

  if (!keyword)
    {
      zcname = g_strconcat (zc_root, CONFIGS, zc_conf, "/",
			    codec_type_string[stream_type], NULL);
      keyword = (const gchar *) zconf_get_string (NULL, zcname);
      g_free (zcname);
    }

  if (keyword && keyword[0])
    {
      codec = rte_set_codec (context, keyword, 0, NULL);

      if (codec)
	{
	  zcname = g_strconcat (zc_root, CONFIGS, zc_conf, CODECS, keyword, NULL);
	  grte_options_load (codec, zcname);
	  g_free (zcname);
	}
    }

  return codec;
}

/**
 * grte_codec_save:
 * @context: 
 * @zc_root: 
 * @zc_conf: 
 * @stream_type: 
 * 
 * Save the configuration of the codec set for @stream_type (%RTE_STREAM_AUDIO
 * or %RTE_STREAM_VIDEO) of #rte_context @context in the Zapping config file
 * under @zc_root // @zc_conf.
 **/
void
grte_codec_save			(rte_context *		context, 
				 const gchar *		zc_root,
				 const gchar *		zc_conf,
				 rte_stream_type	stream_type)
{
  rte_codec *codec;
  rte_codec_info *info;
  gchar *zcname;

  g_assert (zc_root && zc_root[0]);
  g_assert (zc_conf && zc_conf[0]);

  zcname = g_strconcat (zc_root, CONFIGS, zc_conf, "/",
			codec_type_string[stream_type], NULL);
  codec = rte_get_codec (context, stream_type, 0);

  if (codec)
    {
      g_assert ((info = rte_codec_info_by_codec (codec)));
      zconf_set_string (info->keyword, zcname);
      g_free (zcname);
      zcname = g_strconcat (zc_root, CONFIGS, zc_conf, CODECS,
			    info->keyword, NULL);
      grte_options_save (codec, zcname);
    }
  else
    {
      zconf_set_string ("", zcname);
    }

  g_free (zcname);
}

/**
 * grte_context_create_menu:
 * @zc_root: 
 * @zc_conf: 
 * @default_item: 
 * 
 * Create a #GtkMenu consisting of all rte formats / backends / multiplexers
 * available. For all items except "None", a #GtkObject value "keyword" is
 * allocated, pointing to the respective #rte_context_info keyword.
 *
 * When @default_item is given, this function queries the format configured
 * for this @stream_type from the Zapping config file under @zc_root //
 * @zc_conf. Then @default_item is set to the item number listing said
 * format, or item 0.
 * 
 * Return value: 
 * #GtkWidget pointer, %NULL if no formats are available.
 *
 * BUGS:
 * Assumes the format list is never empty.
 **/
GtkWidget *
grte_context_create_menu	(const gchar *		zc_root,
				 const gchar *		zc_conf,
				 gint *			default_item)
{
  GtkWidget *menu, *menu_item;
  rte_context_info *info;
  gchar *zcname;
  const gchar *keyword = 0;
  gint items = 0;
  int i;

  if (default_item)
    {
      zcname = g_strconcat (zc_root, CONFIGS, zc_conf, "/format", NULL);
      keyword = (char *) zconf_get_string (NULL, zcname);
      g_free (zcname);

      if (!keyword || !keyword[0])
	keyword = "";

      *default_item = 0;
    }

  menu = gtk_menu_new ();

  // XXX prepare for empty menu
  for (i = 0; (info = rte_context_info_enum ((guint) i)); i++)
    {
      gchar *label;
      gchar *t;

      t = g_locale_to_utf8 (R_(info->label), -1, NULL, NULL, NULL);
      g_assert (t != NULL);
      label = g_strconcat (info->backend, "  |  ", t, NULL);
      g_free (t);

      menu_item = gtk_menu_item_new_with_label (label);
      g_free(label);

      z_object_set_const_data (G_OBJECT (menu_item), "keyword", info->keyword);

      if (info->tooltip)
	{
	  t = g_locale_to_utf8 (R_(info->tooltip), -1, NULL, NULL, NULL);
	  g_assert (t != NULL);
	  z_tooltip_set (menu_item, t);
	  g_free (t);
	}
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

      if (default_item)
	if (strcmp (keyword, info->keyword) == 0)
	  *default_item = items;

      items++;
    }

  return menu; 
}

/**
 * grte_context_load:
 * @zc_root: 
 * @zc_conf: 
 * @audio_codec_p: 
 * @video_codec_p: 
 * 
 * Allocate a new #rte_context @context encoding the format @keyword,
 * and configure according to the Zapping config file entries under
 * @zc_root // @zc_conf. When @keyword is %NULL, the format
 * configured in the Zapping config file will be used.
 *
 * If given, pointers to the respective audio and video #rte_codec are
 * stored in @audio_codec_p and @video_codec_p. They will be %NULL when
 * the respective stream (audio or video) is not applicable or the codec
 * has been configured as "None".
 *
 * Return value:
 * #rte_context pointer, %NULL if the Zapping config file is empty
 * or another error occured.
 **/
rte_context *
grte_context_load		(const gchar *		zc_root,
				 const gchar *		zc_conf,
				 const gchar *		keyword,
				 rte_codec **		audio_codec_p, 
				 rte_codec **		video_codec_p,
				 gint *			capture_w,
				 gint *			capture_h)
{
  gchar *zcname;
  rte_context *context;
  rte_codec *dummy;

  if (!keyword)
    {
      zcname = g_strconcat (zc_root, CONFIGS, zc_conf, "/format", NULL);
      keyword = (const gchar *) zconf_get_string (NULL, zcname);
      g_free (zcname);

      if (!keyword || !keyword[0])
	return NULL;
    }

  /* preliminary */
  if (capture_w)
    {
      zcname = g_strconcat (zc_root, CONFIGS, zc_conf, "/capture_width", NULL);
      zconf_create_int (384, "Capture width", zcname);
      zconf_get_int (capture_w, zcname);
      g_free (zcname);
    }
  if (capture_h)
    {
      zcname = g_strconcat (zc_root, CONFIGS, zc_conf, "/capture_height", NULL);
      zconf_create_int (288, "Capture height", zcname);
      zconf_get_int (capture_h, zcname);
      g_free (zcname);
    }

  context = rte_context_new (keyword, NULL, NULL);

  if (!context)
    return NULL;

  if (!audio_codec_p) audio_codec_p = &dummy;
  if (!video_codec_p) video_codec_p = &dummy;

  *audio_codec_p = grte_codec_load (context, zc_root, zc_conf,
				    RTE_STREAM_AUDIO, NULL);
  *video_codec_p = grte_codec_load (context, zc_root, zc_conf,
				    RTE_STREAM_VIDEO, NULL);

  return context;
}

/**
 * grte_context_save:
 * @context: 
 * @zc_root: 
 * @zc_conf: 
 * 
 * Save the configuration of #rte_context @context in the Zapping
 * config file under @zc_root // @zc_conf.
 **/
void
grte_context_save		(rte_context *		context,
				 const gchar *		zc_root,
				 const gchar *		zc_conf,
				 gint			capture_w,
				 gint			capture_h)
{
  rte_context_info *info;
  gchar *zcname;

  g_assert (zc_root && zc_root[0]);
  g_assert (zc_conf && zc_conf[0]);
  g_assert ((info = rte_context_info_by_context (context)));

  zcname = g_strconcat (zc_root, CONFIGS, zc_conf, "/format", NULL);
  zconf_set_string (info->keyword, zcname);
  g_free (zcname);

  {
    /* preliminary */

    zcname = g_strconcat (zc_root, CONFIGS, zc_conf, "/capture_width", NULL);
    zconf_set_int (capture_w, zcname);
    g_free (zcname);

    zcname = g_strconcat (zc_root, CONFIGS, zc_conf, "/capture_height", NULL);
    zconf_set_int (capture_h, zcname);
    g_free (zcname);
  }

  grte_codec_save (context, zc_root, zc_conf, RTE_STREAM_AUDIO);
  grte_codec_save (context, zc_root, zc_conf, RTE_STREAM_VIDEO);
}

/**
 * grte_config_delete:
 * @zc_root: 
 * @zc_conf: 
 * 
 * Delete the entry @zc_root // @zc_conf in the Zapping
 * config file.
 **/
void
grte_config_delete		(const gchar *		zc_root,
				 const gchar *		zc_conf)
{
  gchar *zcname;

  zcname = g_strconcat (zc_root, CONFIGS, zc_conf, NULL);
  zconf_delete (zcname);
  g_free (zcname);
}

#endif /* HAVE_LIBRTE */
