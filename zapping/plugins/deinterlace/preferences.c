/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2004 Michael H. Schimek
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

/* $Id: preferences.c,v 1.2.2.1 2005-05-20 05:45:13 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "common/intl-priv.h"
#include "src/zgconf.h"
#include "main.h"
#include "preferences.h"

#define INDENT_COL 0
#define N_COLUMNS 2

#define GCONF_DIR "/apps/zapping/plugins/deinterlace"

/* n/4 */
GConfEnumStringPair
resolution_enum [] = {
  { 2, "low" },
  { 3, "medium" },
  { 4, "high" },
  { 0, NULL }
};

static const gchar *
resolution_menu [] = {
  N_("Low"),
  N_("Medium"),
  N_("High"),
  NULL,
};

static GObjectClass *		parent_class;

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
		    INDENT_COL + 1, N_COLUMNS,
		    row, row + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0),
		    /* padding */ 0, 0);
}

static void
attach_field_balance		(GtkTable *		table,
				 guint			row,
				 const gchar *		gconf_key,
				 const gchar *		tooltip)
{
  GtkWidget *widget;

  widget = z_gconf_float_spinslider_new (/* default */ 0.45,
					 /* min */ 0.0,
					 /* max */ 1.0,
					 /* step */ 0.05,
					 /* page_incr */ 0.1,
					 /* page_size */ 0.1,
					 /* digits */ 3,
					 gconf_key,
					 /* var */ NULL);
  gtk_widget_show (widget);

  if (tooltip)
    z_tooltip_set (widget, tooltip);

  gtk_table_attach (table, widget,
		    INDENT_COL + 1, N_COLUMNS,
		    row, row + 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(0),
		    /* padding */ 0, 0);
}



static gchar *
key_from_setting		(const SETTING *	setting)
{
  gchar *key;

  g_return_val_if_fail (NULL != setting, NULL);
  g_return_val_if_fail (NULL != setting->szIniEntry, NULL);
  g_return_val_if_fail (NULL != setting->szIniSection, NULL);

  key = g_strconcat (GCONF_DIR "/options/",
		     setting->szIniSection,
		     "/",
		     setting->szIniEntry,
		     NULL);

  return key;
}

static void
on_option_changed		(GtkWidget *		widget,
				 gpointer		user_data)
{
  const SETTING *setting = user_data;
  gchar *key;

  if (!(key = key_from_setting (setting)))
    return;

  switch (setting->Type)
    {
    case ITEMFROMLIST:
      {
	gint index;

	index = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

	if (index >= 0 && NULL != setting->pszList[index])
	  {
	    /* Error ignored. */
	    z_gconf_set_string (key, setting->pszList[index]);
	    
	    *setting->pValue = index;
	  }

	break;
      }

    default:
      g_assert_not_reached ();
      break;
    }

  g_free (key);
  key = NULL;
}

static void
create_menu			(GtkTable *		table,
				 guint			row,
				 const SETTING *	setting,
				 const gchar *		key)
{
  GtkWidget *widget;
  GtkComboBox *combo_box;
  GObject *object;

  widget = gtk_combo_box_new_text ();
  gtk_widget_show (widget);

  combo_box = GTK_COMBO_BOX (widget);

  {
    gchar *item_name;
    guint value;
    guint i;

    item_name = NULL;

    /* Error ignored. */
    z_gconf_get_string (&item_name, key);

    if (!item_name)
      if (NULL != setting->pszList[0])
	item_name = g_strdup (setting->pszList[0]);

    value = 0;

    for (i = setting->MinValue; i <= (guint) setting->MaxValue; ++i)
      {
	const gchar *name;

	if (!(name = setting->pszList[i]))
	  break;

	if (item_name)
	  if (0 == g_ascii_strcasecmp (item_name, name))
	    value = i;

	gtk_combo_box_append_text (combo_box, name);
      }

    g_free (item_name);

    gtk_combo_box_set_active (combo_box, value);
  }

  object = G_OBJECT (combo_box);
  z_signal_connect_const (object, "changed",
			  G_CALLBACK (on_option_changed), setting);

  gtk_table_resize (table, row + 1, N_COLUMNS);
  attach_label (table, row, _(setting->szDisplayName));
  gtk_table_attach (table, widget,
		    INDENT_COL + 1, N_COLUMNS,
		    row, row + 1,
                    (GtkAttachOptions)(GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions)(0),
		    /* padding */ 0, 0);
}

static void
create_slider			(GtkTable *		table,
				 guint			row,
				 const SETTING *	setting,
				 const gchar *		key)
{
  GtkWidget *spinslider;

  spinslider = z_gconf_int_spinslider_new (setting->Default,
					   setting->MinValue,
					   setting->MaxValue,
					   /* step_incr */ setting->StepValue,
					   /* page_incr */ setting->StepValue,
					   /* page_size */ setting->StepValue,
					   key,
					   (gint *) setting->pValue);
  gtk_widget_show (spinslider);

  gtk_table_resize (table, row + 1, N_COLUMNS);
  attach_label (table, row, _(setting->szDisplayName));
  gtk_table_attach (table, spinslider,
		    INDENT_COL + 1, N_COLUMNS,
		    row, row + 1,
                    (GtkAttachOptions)(GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions)(0),
		    /* padding */ 0, 0);
}

static void
create_checkbutton		(GtkTable *		table,
				 guint			row,
				 const SETTING *	setting,
				 const gchar *		key)
{
  GtkWidget *widget;

  widget = z_gconf_check_button_new (_(setting->szDisplayName),
				     key,
				     (gboolean *) setting->pValue,
				     !!setting->Default);
  gtk_widget_show (widget);

  gtk_table_resize (table, row + 1, N_COLUMNS);
  gtk_table_attach (table, widget,
		    INDENT_COL, N_COLUMNS,
		    row, row + 1,
                    (GtkAttachOptions)(GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions)(0),
		    /* padding */ 0, 0);
}

static GtkWidget *
create_option_table		(const DEINTERLACE_METHOD *method)
{
  GtkTable *table;
  GtkWidget *widget;
  guint row;
  guint i;

  widget = gtk_table_new (/* rows */ 1, N_COLUMNS, /* homogeneous */ FALSE);
  table = GTK_TABLE (widget);

  gtk_table_set_row_spacings (table, 3);
  gtk_table_set_col_spacings (table, 12);

  row = 0;

  attach_label (table, row, _("_Resolution:"));
  attach_combo_box (table, row++, resolution_menu,
		    GCONF_DIR "/resolution",
		    resolution_enum,
		    /* tooltip */ NULL);

#if 0
  /* TRANSLATORS: Relative display duration of the first and
     second field in deinterlace mode. */
  attach_label (table, row, _("_Field Balance:"));
  attach_field_balance (table, row++,
			GCONF_DIR "/field_balance",
			/* tooltip */ NULL);
#endif

  if (!method)
    return widget;

  for (i = 0; i < (guint) method->nSettings; ++i)
    {
      const SETTING *setting;
      gchar *key;

      if (!(setting = method->pSettings + i))
	continue;

      key = NULL;

      if (!setting->szDisplayName)
	{
	  /* Hidden option. */
	  continue;
	}

      switch (setting->Type)
	  {
	  case ONOFF:
	  case YESNO:
	    if (!(key = key_from_setting (setting)))
	      break;

	    create_checkbutton (table, row++, setting, key);

	    break;

	  case ITEMFROMLIST:
	    if (!(key = key_from_setting (setting)))
	      break;

	    create_menu (table, row++, setting, key);

	    break;
 
	  case SLIDER:
	    if (!(key = key_from_setting (setting)))
	      break;

	    create_slider (table, row++, setting, key);

	    break;

	  default:
	    /* Ignored. */
	    break;
	  }

      g_free (key);
      key = NULL;
    }

  return widget;
}

static gboolean
load_options			(const DEINTERLACE_METHOD *method)
{
  guint i;

  g_return_val_if_fail (NULL != method, FALSE);

  for (i = 0; i < (guint) method->nSettings; ++i)
    {
      const SETTING *setting;
      gchar *key;

      if (!(setting = method->pSettings + i))
	continue;

      key = NULL;

      switch (setting->Type)
	  {
	  case ONOFF:
	  case YESNO:
	    {
	      gboolean active;

	      if (!(key = key_from_setting (setting)))
		break;

	      active = setting->Default;
	      
	      /* Error ignored. */
	      z_gconf_get_bool (&active, key);
	      
	      *setting->pValue = active;
	      
	      break;
	    }

	  case ITEMFROMLIST:
	    {
	      gchar *item;
	      guint index;
	      guint i;

	      if (!(key = key_from_setting (setting)))
		break;

	      item = NULL;

	      /* Error ignored. */
	      z_gconf_get_string (&item, key);

	      if (!item)
		if (setting->pszList[0])
		  item = g_strdup (setting->pszList[0]);

	      index = 0;

	      for (i = setting->MinValue; i <= (guint) setting->MaxValue; ++i)
		{
		  const gchar *name;

		  if (!(name = setting->pszList[i]))
		    break;

		  if (item)
		    if (0 == g_ascii_strcasecmp (item, name))
		      index = i;
		}

	      g_free (item);

	      *setting->pValue = index;

	      break;
	    }

	  case SLIDER:
	    {
	      gint value;

	      if (!(key = key_from_setting (setting)))
		break;

	      value = setting->Default;
	    
	      /* Error ignored. */
	      z_gconf_get_bool (&value, key);
	    
	      *setting->pValue = value;
	    
	      break;
	    }

	  default:
	    /* Ignored. */
	    break;
	  }

      g_free (key);
      key = NULL;
    }

  return TRUE;
}

void
deinterlace_prefs_cancel		(DeinterlacePrefs *	prefs)
{
  GError *error = NULL;
  gboolean success;
  gchar *item;
  DEINTERLACE_METHOD *method;

  g_return_if_fail (IS_DEINTERLACE_PREFS (prefs));

  if (!prefs->change_set)
    return;

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
	  printv ("Cannot revert deinterlace prefs: %s\n", error->message);
	  g_error_free (error);
	  error = NULL;
	}
    }

  item = NULL;

  /* Error ignored. */
  z_gconf_get_string (&item, GCONF_DIR "/method");

  /* Apply reverted gconf values. */
  if ((method = deinterlace_find_method (item)))
    load_options (method);

  g_free (item);

  gtk_widget_destroy (GTK_WIDGET (prefs));
}

static void
on_method_changed		(GtkComboBox *		combo_box,
				 gpointer		user_data)
{
  DeinterlacePrefs *prefs = DEINTERLACE_PREFS (user_data);
  gint active_item;

  g_return_if_fail (IS_DEINTERLACE_PREFS (prefs));

  if (prefs->option_table)
    {
      gtk_widget_destroy (prefs->option_table);
      prefs->option_table = NULL;
    }

  active_item = gtk_combo_box_get_active (combo_box);

  if (active_item <= 0)
    {
      /* Error ignored. */
      z_gconf_set_string (GCONF_DIR "/method", "disabled");
    }
  else
    {
      const DEINTERLACE_METHOD *method;
      guint i;

      --active_item;

      for (i = 0; i < N_ELEMENTS (deinterlace_methods); ++i)
	{
	  if (!(method = deinterlace_methods[i]))
	    continue;

	  if (0 == active_item)
	    break;

	  --active_item;
	}

      if (i < N_ELEMENTS (deinterlace_methods)
	  && NULL != method
	  && NULL != method->szName)
	{
	  /* Error ignored. */
	  z_gconf_set_string (GCONF_DIR "/method", method->szName);

	  prefs->option_table = create_option_table (method);
	  gtk_widget_show (prefs->option_table);

	  gtk_table_attach (&prefs->table, prefs->option_table,
			    /* column */ 0, 0 + 1,
			    /* row */ 1, 1 + 1,
			    (GtkAttachOptions)(GTK_FILL | GTK_EXPAND),
			    (GtkAttachOptions)(0),
			    /* padding */ 0, 0);
	}
      else
	{
	  /* Error ignored. */
	  z_gconf_set_string (GCONF_DIR "/method", "disabled");
	}
    }
}

static void
instance_finalize		(GObject *		object)
{
  DeinterlacePrefs *prefs = DEINTERLACE_PREFS (object);

  if (prefs->change_set)
    {
      gconf_change_set_unref (prefs->change_set);
      prefs->change_set = NULL;
    }

  parent_class->finalize (object);
}

static GConfChangeSet *
create_change_set		(void)
{
  GError *error = NULL;
  GConfChangeSet *change_set;
  gchar **keys;
  guint keys_capacity;
  guint keys_size;
  guint i;

  keys_capacity = 16;
  keys_size = 0;

  keys = g_new (char *, keys_capacity);
  keys[0] = NULL;

  for (i = 0; i < N_ELEMENTS (deinterlace_methods); ++i)
    {
      const DEINTERLACE_METHOD *method;
      guint j;

      if (!(method = deinterlace_methods[i]))
	continue;

      for (j = 0; j < (guint) method->nSettings; ++j)
	{
	  const SETTING *setting;

	  if (!(setting = method->pSettings + j))
	    continue;

	  switch (setting->Type)
	    {
	    case ONOFF:
	    case YESNO:
	    case ITEMFROMLIST:
	    case SLIDER:
	      {
		gchar *key;

		if (!(key = key_from_setting (setting)))
		  break;

		if (0)
		  fprintf (stderr,
			   "  <schema>\n"
			   "   <key>/schemas%s</key>\n"
			   "   <applyto>%s</applyto>\n"
			   "   <owner>Zapping</owner>\n"
			   "   <type>%u</type>\n"
			   "   <default>%ld</default>\n"
			   "  </schema>\n",
			   key, key, setting->Type, setting->Default);

		if (keys_size + 1 >= keys_capacity)
		  {
		    keys_capacity *= 2;
		    keys = g_renew (char *, keys, keys_capacity);
		  }

		keys[keys_size++] = key;
		key = NULL;

		keys[keys_size] = NULL;

		break;
	      }

	    default:
	      /* Ignored. */
	      break;
	    }
	}
    }

  change_set = gconf_client_change_set_from_currentv (gconf_client,
						      keys,
						      &error);
  if (!change_set || error)
    {
      g_assert (NULL == change_set);

      if (error)
	{
	  g_warning ("Cannot create deinterlace prefs change set:\n%s",
		     error->message);
	  g_error_free (error);
	  error = NULL;
	}
    }

  g_strfreev (keys);

  return change_set;
}

static GtkWidget *
attach_method_combo		(DeinterlacePrefs *	prefs,
				 guint			row)
{
  GtkWidget *widget;
  GtkComboBox *combo_box;
  GObject *object;

  widget = gtk_combo_box_new_text ();
  gtk_widget_show (widget);

  combo_box = GTK_COMBO_BOX (widget);

  {
    gchar *item_name;
    guint item_counter;
    guint active_item;
    guint i;

    item_name = NULL;

    /* Error ignored. */
    z_gconf_get_string (&item_name, GCONF_DIR "/method");

    gtk_combo_box_append_text (combo_box, _("Disabled"));

    active_item = 0; /* disabled */
    item_counter = 1;

    for (i = 0; i < N_ELEMENTS (deinterlace_methods); ++i)
      {
	const DEINTERLACE_METHOD *method;

	if (!(method = deinterlace_methods[i]))
	  continue;

	if (item_name && 0 == active_item)
	  if (0 == g_ascii_strcasecmp (item_name, method->szName))
	    active_item = item_counter;

	gtk_combo_box_append_text (combo_box, _(method->szName));

	++item_counter;
      }

    g_free (item_name);

    gtk_combo_box_set_active (combo_box, active_item);
  }

  object = G_OBJECT (combo_box);
  g_signal_connect (object, "changed",
		    G_CALLBACK (on_method_changed), prefs);

  gtk_table_attach (&prefs->table, widget,
		    0, 0 + 1,
		    row, row + 1,
                    (GtkAttachOptions)(GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions)(0),
		    /* padding */ 0, 0);

  return widget;
}

static void
instance_init			(GTypeInstance *	instance,
				 gpointer		g_class _unused_)
{
  DeinterlacePrefs *prefs = (DeinterlacePrefs *) instance;
  GtkWidget *method_combo;

  gtk_table_resize (&prefs->table, /* rows */ 2, /* columns */ 1);
  gtk_table_set_homogeneous (&prefs->table, FALSE);

  gtk_container_set_border_width (GTK_CONTAINER (&prefs->table), 12);
  gtk_table_set_row_spacings (&prefs->table, 3);

  method_combo = attach_method_combo (prefs, /* row */ 0);

  /* Add options of current method. */
  on_method_changed (GTK_COMBO_BOX (method_combo), prefs);

  prefs->change_set = create_change_set ();
}

GtkWidget *
deinterlace_prefs_new		(void)
{
  return GTK_WIDGET (g_object_new (TYPE_DEINTERLACE_PREFS, NULL));
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
deinterlace_prefs_get_type	(void)
{
  static GType type = 0;
  
  if (!type)
    {
      GTypeInfo info;

      CLEAR (info);

      info.class_size = sizeof (DeinterlacePrefsClass);
      info.class_init = class_init;
      info.instance_size = sizeof (DeinterlacePrefs);
      info.instance_init = instance_init;

      type = g_type_register_static (GTK_TYPE_TABLE,
				     "DeinterlacePrefs",
				     &info, (GTypeFlags) 0);
    }

  return type;
}
