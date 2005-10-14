/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2000-2002 Iñaki García Etxebarria
 *  Copyright (C) 2000-2005 Michael H. Schimek
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

/* $Id: preferences.c,v 1.2 2005-10-14 23:40:13 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common/intl-priv.h"
#include "src/zgconf.h"
#include "src/zspinslider.h"
#include "main.h"
#include "preferences.h"

#define GCONF_DIR "/apps/zapping/plugins/subtitle"

#define INDENT_COL 2

static GObjectClass *		parent_class;

GConfEnumStringPair
subtitle_charset_enum [] = {
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
subtitle_interp_enum [] = {
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
attach_check_button		(GtkTable *		table,
				 guint			row,
				 const gchar *		label,
				 const gchar *		gconf_key,
				 gboolean		def_value,
				 const gchar *		tooltip)
{
  GtkWidget *widget;

  widget = z_gconf_check_button_new (label, gconf_key, NULL, def_value);
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

static gboolean
get_color			(vbi3_rgba *		rgba,
				 const gchar *		key)
{
  gchar *str;
  gchar *s;
  gboolean r;
  guint value;
  guint i;

  if (!z_gconf_get_string (&str, key))
    return FALSE;

  s = str;
  r = FALSE;

  while (g_ascii_isspace (*s))
    ++s;

  if ('#' != *s++)
    goto failure;

  while (g_ascii_isspace (*s))
    ++s;

  value = 0;

  for (i = 0; i < 6; ++i)
    {
      if (g_ascii_isdigit (*s))
	value = value * 16 + (*s - '0');
      else if (g_ascii_isxdigit (*s))
	value = value * 16 + ((*s - ('A' - 0xA)) & 0xF);
      else
	goto failure;

      ++s;
    }

  while (g_ascii_isspace (*s))
    ++s;

  if (0 == *s)
    {
      r = TRUE;

      *rgba = (((value & 0xFF) << 16) |
	       ((value & 0xFF00) << 0) |
	       ((value & 0xFF0000) >> 16));
    }

 failure:
  g_free (str);

  return r;
}

static void
on_color_set			(GnomeColorPicker *	colorpicker,
				 guint			arg1,
				 guint			arg2,
				 guint			arg3,
				 guint			arg4,
				 gpointer		user_data)
{
  char buffer[40];
  const gchar *gconf_key;

  colorpicker = colorpicker;
  arg4 = arg4;

  gconf_key = user_data;

  snprintf (buffer, sizeof (buffer), "#%02X%02X%02X",
	    arg1 >> 8, arg2 >> 8, arg3 >> 8);

  /* Error ignored. */
  z_gconf_set_string (gconf_key, buffer);
}

static void
attach_color_picker		(GtkTable *		table,
				 guint			row,
				 const gchar *		title,
				 const gchar *		gconf_key,
				 vbi3_rgba		def_value,
				 const gchar *		tooltip)
{
  GtkWidget *widget;
  GnomeColorPicker *color_picker;

  /* Error ignored. */
  get_color (&def_value, gconf_key);

  widget = gnome_color_picker_new ();
  gtk_widget_show (widget);

  color_picker = GNOME_COLOR_PICKER (widget);
  gnome_color_picker_set_use_alpha (color_picker, FALSE);
  gnome_color_picker_set_i8 (color_picker,
			     (def_value & 0xff),
			     (def_value & 0xff00) >> 8,
			     (def_value & 0xff0000) >> 16,
			     0);
  gnome_color_picker_set_title (color_picker, title);

  z_signal_connect_const (G_OBJECT (color_picker), "color-set",
			  G_CALLBACK (on_color_set), gconf_key);

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
subtitle_prefs_apply		(SubtitlePrefs *	prefs)
{
  prefs = prefs;
}

void
subtitle_prefs_cancel		(SubtitlePrefs *	prefs)
{
  GError *error = NULL;
  gboolean success;

  g_return_if_fail (IS_SUBTITLE_PREFS (prefs));

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
	      printv ("Cannot revert Subtitle prefs: %s\n", error->message);
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
      z_gconf_set_int (GCONF_DIR "/brightness", SATURATE (value, 0, 255));
      break;

    case 1:
      value = GTK_ADJUSTMENT (adj)->value;
      /* Error ignored. */
      z_gconf_set_int (GCONF_DIR "/contrast",
		       SATURATE (value, -128, +127));
      break;
    }
}

static void
instance_finalize		(GObject *		object)
{
  SubtitlePrefs *prefs = SUBTITLE_PREFS (object);

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
  SubtitlePrefs *prefs = (SubtitlePrefs *) instance;
  GError *error = NULL;
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

  attach_header (&prefs->table, row++, _("<b>Teletext</b>"));

  attach_label (&prefs->table, row, _("_Default encoding:"));
  attach_combo_box (&prefs->table, row++, charset_menu,
		    GCONF_DIR "/default_charset",
		    subtitle_charset_enum,
		    _("Some stations fail to transmit a complete language "
		      "identifier, so the Subtitle viewer may not display "
		      "the correct font or national characters. You can "
		      "select your geographical region here as an "
		      "additional hint."));

  attach_check_button (&prefs->table, row++,
		       _("_Shrink double height characters"),
		       GCONF_DIR "/shrink_double", FALSE, NULL);

  /* TRANSLATORS:
     Closed Caption is a captioning system used mainly in the USA,
     and on VHS/LD/DVD in other countries. Europe uses Teletext for
     caption and subtitle transmissions. */
  attach_header (&prefs->table, row++, _("<b>Closed Caption</b>"));

  attach_label (&prefs->table, row, _("_Foreground:"));
  attach_color_picker (&prefs->table, row++,
		       _("Closed Caption foreground color"),
		       GCONF_DIR "/foreground", 0xFFFFFF, NULL);

  attach_label (&prefs->table, row, _("_Background:"));
  attach_color_picker (&prefs->table, row++,
		       _("Closed Caption background color"),
		       GCONF_DIR "/background", 0x000000, NULL);

#if 0
  attach_check_button (&prefs->table, row++,
		       ("_Pad"),
		       GCONF_DIR "/pad", FALSE, NULL);
#endif
  attach_check_button (&prefs->table, row++,
		       _("_Roll live caption"),
		       GCONF_DIR "/roll", FALSE, NULL);

  attach_header (&prefs->table, row++, _("<b>Display</b>"));

  attach_label (&prefs->table, row, _("_Brightness:"));
  value = 128;
  z_gconf_get_int (&value, GCONF_DIR "/brightness");
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
  z_gconf_get_int (&value, GCONF_DIR "/contrast");
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
		    GCONF_DIR "/interp_type",
		    subtitle_interp_enum,
		    _("Quality/speed trade-off when scaling and "
		      "anti-aliasing the page."));

  prefs->change_set =
    gconf_client_change_set_from_current (gconf_client,
					  &error,
					  GCONF_DIR "/default_charset",
					  GCONF_DIR "/interp_type",
					  GCONF_DIR "/brightness",
					  GCONF_DIR "/contrast",
					  GCONF_DIR "/foreground",
					  GCONF_DIR "/background",
					  GCONF_DIR "/pad",
					  GCONF_DIR "/roll",
					  GCONF_DIR "/shrink_double",
					  NULL);
  if (!prefs->change_set || error)
    {
      g_assert (!prefs->change_set);

      if (error)
	{
	  g_warning ("Cannot create Subtitle prefs change set:\n%s",
		     error->message);
	  g_error_free (error);
	  error = NULL;
	}
    }
}

GtkWidget *
subtitle_prefs_new		(void)
{
  return GTK_WIDGET (g_object_new (TYPE_SUBTITLE_PREFS, NULL));
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
subtitle_prefs_get_type		(void)
{
  static GType type = 0;
  
  if (!type)
    {
      GTypeInfo info;

      CLEAR (info);

      info.class_size = sizeof (SubtitlePrefsClass);
      info.class_init = class_init;
      info.instance_size = sizeof (SubtitlePrefs);
      info.instance_init = instance_init;

      type = g_type_register_static (GTK_TYPE_TABLE,
				     "SubtitlePrefs",
				     &info, (GTypeFlags) 0);
    }

  return type;
}

