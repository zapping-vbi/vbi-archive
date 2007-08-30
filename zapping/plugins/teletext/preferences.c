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

/* $Id: preferences.c,v 1.5 2007-08-30 14:14:33 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common/intl-priv.h"
#include "libvbi/cache.h"
#include "src/zgconf.h"
#include "src/zspinslider.h"
#include "src/zvbi.h"
#include "main.h"
#include "preferences.h"

#define GCONF_DIR "/apps/zapping/plugins/teletext"

#define INDENT_COL 2

static GObjectClass *		parent_class;

GConfEnumStringPair
teletext_charset_enum [] = {
  { 0, "western_and_central_europe" },
  { 8, "eastern_europe" },
  { 16, "western_europe_and_turkey" },
  { 24, "central_and_southeast_europe" },
  { 32, "cyrillic" },
  { 48, "greek_and_turkish" },
  { 64, "arabic" },
  { 80, "hebrew_and_arabic" },
  { 0, NULL }
};

static const gchar *
charset_menu [] = {
  N_("Western and Central Europe"),
  N_("Eastern Europe"),
  N_("Western Europe and Turkey"),
  N_("Central and Southeast Europe"),
  N_("Cyrillic"),
  N_("Greek and Turkish"),
  N_("Arabic"),
  N_("Hebrew and Arabic"),
  NULL,
};

GConfEnumStringPair
teletext_level_enum [] = {
  { VBI3_WST_LEVEL_1, "1" },
  { VBI3_WST_LEVEL_1p5, "1.5" },
  { VBI3_WST_LEVEL_2p5, "2.5" },
  { VBI3_WST_LEVEL_3p5, "3.5" },
  { 0, NULL }
};

static const gchar *
level_menu [] = {
  N_("Level 1"),
  N_("Level 1.5 (additional national characters)"),
  N_("Level 2.5 (more colors, font styles and graphics)"),
  N_("Level 3.5 (proportional spacing, multicolor graphics)"),
  NULL,
};

GConfEnumStringPair
teletext_interp_enum [] = {
  { GDK_INTERP_NEAREST, "nearest" },
  { GDK_INTERP_TILES, "tiles" },
  { GDK_INTERP_BILINEAR, "bilinear" },	
  { GDK_INTERP_HYPER, "hyper" },
  { 0, NULL }
};

static const gchar *
interp_menu [] = {
  N_("Nearest (fast, low quality)"),
  N_("Tiles"),
  N_("Bilinear"),
  N_("Hyper (slow, high quality)"),
  NULL,
};

static void
attach_header			(GtkTable *		table,
				 guint			row,
				 const gchar *		text)
{
  GtkWidget *widget;

  widget = gtk_label_new (text);
  gtk_widget_show (widget);

  gtk_label_set_use_markup (GTK_LABEL (widget), TRUE);
  gtk_misc_set_alignment (GTK_MISC (widget), 0, 0.5);

  gtk_table_attach (table, widget,
		    /* column */ 0, 4,
		    row, row + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0),
		    /* padding */ 0, 3);
}

static void
attach_label			(GtkTable *		table,
				 guint			row,
				 const gchar *		text)
{
  GtkWidget *widget;

  widget = gtk_label_new_with_mnemonic (text);
  gtk_widget_show (widget);

  gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);

  gtk_table_attach (table, widget,
		    INDENT_COL, INDENT_COL + 1,
		    row, row + 1,
                    (GtkAttachOptions)(GTK_FILL),
                    (GtkAttachOptions)(0),
		    /* padding */ 0, 0);
}

static void
attach_combo_box		(GtkTable *		table,
				 guint			row,
				 const gchar **		option_menu,
				 const gchar *		gconf_key,
				 const GConfEnumStringPair *lookup_table,
				 const gchar *		tooltip)
{
  GtkWidget *widget;

  widget = z_gconf_combo_box_new (option_menu, gconf_key, lookup_table);
  gtk_widget_show (widget);

  if (tooltip)
    z_tooltip_set (widget, tooltip);

  gtk_table_attach (table, widget,
		    INDENT_COL + 1, INDENT_COL + 2,
		    row, row + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0),
		    /* padding */ 0, 0);
}

void
teletext_prefs_apply		(TeletextPrefs *	prefs)
{
  vbi3_decoder *vbi;
  vbi3_cache *ca;
  gint value;

  g_return_if_fail (IS_TELETEXT_PREFS (prefs));

  /* These do not auto apply because we cannot undo. */

  ca = NULL;
  if ((vbi = zvbi_get_object ()))
    {
      vbi3_teletext_decoder *td;

      td = vbi3_decoder_cast_to_teletext_decoder (vbi);
      ca = vbi3_teletext_decoder_get_cache (td);
    }

  value = gtk_adjustment_get_value (prefs->cache_size);
  value *= 1 << 10;
  z_gconf_set_int (GCONF_DIR "/cache_size", value);

  if (ca)
    vbi3_cache_set_memory_limit (ca, (unsigned int) value);
  
  value = gtk_adjustment_get_value (prefs->cache_networks);
  z_gconf_set_int (GCONF_DIR "/cache_networks", value);

  if (ca)
    {
      vbi3_cache_set_network_limit (ca, (unsigned int) value);
      vbi3_cache_unref (ca);
    }
}

void
teletext_prefs_cancel		(TeletextPrefs *	prefs)
{
  GError *error = NULL;
  gboolean success;

  g_return_if_fail (IS_TELETEXT_PREFS (prefs));

  if (prefs->change_set)
    {

  /* Revert to old values. */
  success = gconf_client_commit_change_set (gconf_client,
					    prefs->change_set,
					    /* remove_committed */ FALSE,
					    &error);
  if (!success || error)
    {
      /* Error ignored. */

      if (error)
	{
	  printv ("Cannot revert Teletext prefs: %s\n", error->message);
	  g_error_free (error);
	  error = NULL;
	}
    }
    }

  gtk_widget_destroy (GTK_WIDGET (prefs));
}

static void
on_control_changed		(GtkWidget *		adj,
				 gpointer		user_data)
{
  gint value;

  switch (GPOINTER_TO_INT (user_data))
    {
    case 0:
      value = GTK_ADJUSTMENT (adj)->value;
      /* Error ignored. */
      z_gconf_set_int (GCONF_DIR "/view/brightness", SATURATE (value, 0, 255));
      break;

    case 1:
      value = GTK_ADJUSTMENT (adj)->value;
      /* Error ignored. */
      z_gconf_set_int (GCONF_DIR "/view/contrast",
		       SATURATE (value, -128, +127));
      break;
    }
}

static void
instance_finalize		(GObject *		object)
{
  TeletextPrefs *prefs = TELETEXT_PREFS (object);

  if (prefs->change_set)
    {
      gconf_change_set_unref (prefs->change_set);
      prefs->change_set = NULL;
    }

  parent_class->finalize (object);
}

static void
instance_init			(GTypeInstance *	instance,
				 gpointer		g_class _unused_)
{
  TeletextPrefs *prefs = (TeletextPrefs *) instance;
  GError *error = NULL;
  GtkWidget *hbox;
  GtkWidget *widget;
  GtkObject *adj;
  guint row;
  gint value;

  gtk_table_resize (&prefs->table, /* rows */ 6, /* columns */ 4);
  gtk_table_set_homogeneous (&prefs->table, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (&prefs->table), 12);
  gtk_table_set_row_spacings (&prefs->table, 3);
  gtk_table_set_col_spacings (&prefs->table, 12);

  row = 0;

  attach_header (&prefs->table, row++, _("<b>General</b>"));

  attach_label (&prefs->table, row, _("_Teletext implementation:"));
  attach_combo_box (&prefs->table, row++, level_menu,
		    GCONF_DIR "/level",
		    teletext_level_enum,
		    NULL);

  attach_label (&prefs->table, row, _("_Default encoding:"));
  attach_combo_box (&prefs->table, row++, charset_menu,
		    GCONF_DIR "/default_charset",
		    teletext_charset_enum,
		    _("Some stations fail to transmit a complete language "
		      "identifier, so the Teletext viewer may not display "
		      "the correct font or national characters. You can "
		      "select your geographical region here as an "
		      "additional hint."));

  attach_header (&prefs->table, row++, _("<b>Page memory</b>"));

  attach_label (&prefs->table, row, _("_Size:"));

  hbox = gtk_hbox_new (/* homogeneous */ FALSE, /* spacing */ 0);
  gtk_widget_show (hbox);
  gtk_box_set_spacing (GTK_BOX (hbox), 6);
  gtk_table_attach (&prefs->table, hbox,
		    INDENT_COL + 1, INDENT_COL + 2,
		    row, row + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0),
		    /* padding */ 0, 0);

  value = 1000 << 10; /* bytes */
  z_gconf_get_int (&value, GCONF_DIR "/cache_size");
  adj = gtk_adjustment_new ((value + 1023) >> 10, /* kbytes */
			    /* min, max */ 1, 1 << 20,
			    /* step */ 10,
			    /* page incr, size */ 1000, 1000);
  prefs->cache_size = GTK_ADJUSTMENT (adj);
  widget = gtk_spin_button_new (prefs->cache_size,
				/* climb */ 10, /* decimals */ 0);
  gtk_widget_show (widget);
  gtk_box_pack_start (GTK_BOX (hbox), widget,
		      /* expand */ TRUE, /* fill */ TRUE, /* pad */ 0);

  /* TRANSLATORS: Kilobyte (2^10) */
  widget = gtk_label_new (_("KiB"));
  gtk_widget_show (widget);
  gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);   
  gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);

  ++row;

  attach_label (&prefs->table, 5, _("_Channels:"));

  value = 1;
  z_gconf_get_int (&value, GCONF_DIR "/cache_networks");
  adj = gtk_adjustment_new (value,
			    /* min, max */ 1, 300,
			    /* step */ 1,
			    /* page incr, size */ 10, 10);
  prefs->cache_networks = GTK_ADJUSTMENT (adj);
  widget = gtk_spin_button_new (prefs->cache_networks,
				/* climb */ 1, /* decimals */ 0);
  gtk_widget_show (widget);
  gtk_table_attach (&prefs->table, widget,
		    INDENT_COL + 1, INDENT_COL + 2,
		    row, row + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0),
		    /* padding */ 0, 0);
  ++row;

  attach_header (&prefs->table, row++, _("<b>Display</b>"));

  attach_label (&prefs->table, row, _("_Brightness:"));
  value = 128;
  z_gconf_get_int (&value, GCONF_DIR "/view/brightness");
  adj = gtk_adjustment_new (value,
			    /* min, max */ 0.0, 255.0,
			    /* step */ 1,
			    /* page incr, size */ 16, 16);
  widget = z_spinslider_new (GTK_ADJUSTMENT (adj), NULL, NULL, 128, 0);
  z_spinslider_set_int_value (Z_SPINSLIDER (widget), value);
  gtk_widget_show (widget);
  gtk_table_attach (&prefs->table, widget,
		    INDENT_COL + 1, INDENT_COL + 2,
		    row, row + 1,
		    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions)(0),
		    /* padding */ 0, 0);
  g_signal_connect (G_OBJECT (adj), "value-changed",
		    G_CALLBACK (on_control_changed),
		    GINT_TO_POINTER (0));

  ++row;

  attach_label (&prefs->table, row, _("_Contrast:"));
  value = 64;
  z_gconf_get_int (&value, GCONF_DIR "/view/contrast");
  adj = gtk_adjustment_new (value,
			    /* min, max */ -128, +127,
			    /* step */ 1,
			    /* page incr, size */ 16, 16);
  widget = z_spinslider_new (GTK_ADJUSTMENT (adj), NULL, NULL, 64, 0);
  z_spinslider_set_int_value (Z_SPINSLIDER (widget), value);
  gtk_widget_show (widget);
  gtk_table_attach (&prefs->table, widget,
		    INDENT_COL + 1, INDENT_COL + 2,
		    row, row + 1,
		    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
		    (GtkAttachOptions)(0),
		    /* padding */ 0, 0);
  g_signal_connect (G_OBJECT (adj), "value-changed",
		    G_CALLBACK (on_control_changed),
		    GINT_TO_POINTER (0));

  ++row;

  attach_label (&prefs->table, row, _("S_caling:"));
  attach_combo_box (&prefs->table, row++, interp_menu,
		    GCONF_DIR "/view/interp_type",
		    teletext_interp_enum,
		    _("Quality/speed trade-off when scaling and "
		      "anti-aliasing the page."));

  prefs->change_set =
    gconf_client_change_set_from_current (gconf_client,
					  &error,
					  GCONF_DIR "/default_charset",
					  GCONF_DIR "/level",
					  GCONF_DIR "/view/interp_type",
					  NULL);
  if (!prefs->change_set || error)
    {
      g_assert (!prefs->change_set);

      if (error)
	{
	  g_warning ("Cannot create Teletext prefs change set:\n%s",
		     error->message);
	  g_error_free (error);
	  error = NULL;
	}
    }
}

GtkWidget *
teletext_prefs_new		(void)
{
  return GTK_WIDGET (g_object_new (TYPE_TELETEXT_PREFS, NULL));
}

static void
class_init			(gpointer		g_class,
				 gpointer		class_data _unused_)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (g_class);
  parent_class = g_type_class_peek_parent (g_class);

  object_class->finalize = instance_finalize;
}

GType
teletext_prefs_get_type		(void)
{
  static GType type = 0;
  
  if (!type)
    {
      GTypeInfo info;

      CLEAR (info);

      info.class_size = sizeof (TeletextPrefsClass);
      info.class_init = class_init;
      info.instance_size = sizeof (TeletextPrefs);
      info.instance_init = instance_init;

      type = g_type_register_static (GTK_TYPE_TABLE,
				     "TeletextPrefs",
				     &info, (GTypeFlags) 0);
    }

  return type;
}

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
