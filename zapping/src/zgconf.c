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

/* $Id: zgconf.c,v 1.7 2007-08-30 14:14:36 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <math.h>		/* fabs() */
#include "common/intl-priv.h"	/* _() */
#include "globals.h"		/* gconf_client */
#include "zspinslider.h"
#include "zgconf.h"

const gchar *
z_gconf_value_type_name		(GConfValueType		type)
{
  switch (type)
    {

#undef CASE
#define CASE(s) case GCONF_VALUE_##s : return #s

      CASE (INVALID);
      CASE (STRING);
      CASE (INT);
      CASE (FLOAT);
      CASE (BOOL);
      CASE (SCHEMA);
      CASE (LIST);
      CASE (PAIR);

    default:
      break;
    }

  return NULL;
}

static gboolean
z_gconf_value_get		(gpointer		result,
				 GConfValue *		value,
				 const gchar *		key,
				 GConfValueType		type)
{
  if (type == value->type)
    {
      switch (type)
	{
	case GCONF_VALUE_STRING:
	  *(const gchar **) result = gconf_value_get_string (value);
	  break;

	case GCONF_VALUE_INT:
	  *(gint *) result = gconf_value_get_int (value);
	  break;

	case GCONF_VALUE_FLOAT:
	  *(gdouble *) result = gconf_value_get_float (value);
	  break;

	case GCONF_VALUE_BOOL:
	  *(gboolean *) result = gconf_value_get_bool (value);
	  break;

	default:
	  g_assert_not_reached ();
	  break;
	}

      return TRUE;
    }
  else
    {
      g_warning ("GConf key '%s' has wrong type %s, expected %s.\n",
		 key ? key : "<unknown>",
		 z_gconf_value_type_name (value->type),
		 z_gconf_value_type_name (type));

      return FALSE;
    }
}

static void
z_gconf_handle_get_error	(const gchar *		key,
				 GError **		error)
{
  static gboolean warned = FALSE;

  if (*error)
    {
      g_warning ("GConf get '%s' error:\n%s\n", key, (*error)->message);
      g_error_free (*error);
      *error = NULL;
    }
  else if (!warned)
    {
      g_warning ("GConf key '%s' is unset and has no default. "
		 "Zapping schemas incomplete or not installed?\n",
		 key);

      warned = TRUE;
    }
}

/* Like gconf_client_get(), but uses our default GConfClient and handles
   the GConfValue stuff. On success the function returns TRUE and stores
   the value at *result. On failure it returns FALSE and prints various
   diagnostic messages on stdout.

   You shouldn't call this directly but z_gconf_get_bool, -int, etc for
   proper type checking. If you don't test for success (this function
   isn't really supposed to fail) set *result to some default before
   calling. (Schemes should provide defaults but may fail too.) */
gboolean
z_gconf_get			(gpointer		result,
				 const gchar *		key,
				 GConfValueType		type)
{
  GError *error = NULL;
  GConfValue *value;

  if ((value = gconf_client_get (gconf_client, key, &error)))
    {
      gboolean success;

      g_assert (!error);

      success = z_gconf_value_get (result, value, key, type);

      if (GCONF_VALUE_STRING == type)
	{
	  gchar **string = result;

	  *string = g_strdup (*string);
	}

      gconf_value_free (value);

      return success;
    }
  else
    {
      z_gconf_handle_get_error (key, &error);
      return FALSE;
    }
}

/* Convenient contraction of z_gconf_get_string()
   and gconf_string_to_enum(). */
gboolean
z_gconf_get_string_enum		(gint *			enum_value,
				 const gchar *		key,
				 const GConfEnumStringPair *lookup_table)
{
  gchar *s;
  gboolean r;

  r = FALSE;

  if (z_gconf_get_string (&s, key))
    {
      r = gconf_string_to_enum (lookup_table, s, enum_value);
      g_free (s);
    }

  return r;
}

static void
z_gconf_handle_set_error	(const gchar *		key,
				 GError **		error)
{
  if (*error)
    {
      g_warning ("GConf set '%s' error:\n%s\n", key, (*error)->message);
      g_error_free (*error);
      *error = NULL;
    }
}

/* Like gconf_client_set_<type>(), but uses our default GConfClient
   and prints diagnostic messages on failure (the function is not really
   supposed to fail). Returns success. */
#define Z_GCONF_SET(T1, T2)						\
gboolean								\
z_gconf_set_##T1		(const gchar *		key,		\
				 T2			value)		\
{									\
  GError *error = NULL;							\
  gboolean success;							\
									\
  success = gconf_client_set_##T1 (gconf_client, key, value, &error);	\
  z_gconf_handle_set_error (key, &error);				\
									\
  return success;							\
}

Z_GCONF_SET (bool, gboolean)
Z_GCONF_SET (int, gint)
Z_GCONF_SET (float, gdouble)
Z_GCONF_SET (string, const gchar *)

void
z_gconf_notify_remove		(guint			cnxn_id)
{
  gconf_client_notify_remove (gconf_client, cnxn_id);
}

/* Like gconf_client_notify_add(), but uses our default GConfClient
   and prints diagnostic messages on failure (the function is not really
   supposed to fail). On success it calls func once with the current
   value of key for initialization, and returns TRUE. */
gboolean
z_gconf_notify_add		(const gchar *		key,
				 GConfClientNotifyFunc	func,
				 gpointer		user_data)
{
  GError *error = NULL;
  GConfEntry entry;
  guint cnxn_id;

  cnxn_id = gconf_client_notify_add (gconf_client,
				     key,
				     func, user_data,
				     /* destroy */ NULL,
				     &error);

  if (error)
    {
      g_warning ("GConf notification '%s' error:\n%s\n", key, error->message);
      g_error_free (error);
      error = NULL;

      return FALSE;
    }

  /* Initial value. */

  entry.key = key;

  if ((entry.value = gconf_client_get (gconf_client, key, &error)))
    {
      func (gconf_client, cnxn_id, &entry, user_data);
      gconf_value_free (entry.value);
      return TRUE;
    }
  else
    {
      z_gconf_handle_get_error (key, &error);
      gconf_client_notify_remove (gconf_client, cnxn_id);
      return FALSE;
    }
}

#define Z_GCONF_AUTO(T1, type)						\
static void								\
auto_##T1			(GConfClient *		client _unused_,  \
				 guint			cnxn_id _unused_, \
				 GConfEntry *		entry,		\
				 gpointer		var)		\
{									\
  if (entry->value)							\
    z_gconf_value_get (var, entry->value, NULL, GCONF_VALUE_##type);	\
}

Z_GCONF_AUTO (bool, BOOL)
Z_GCONF_AUTO (int, INT)
Z_GCONF_AUTO (float, FLOAT)

/* This is a combination of z_gconf_get() and z_gconf_notify_add().
   The function stores the current value of the key in var, and updates
   var on all future changes of the key. There is no way to turn this
   off until program termination, so var must be global.

   You shouldn't call this directly but z_gconf_auto_update_bool, -int,
   -float etc for proper type checking. */
gboolean
z_gconf_auto_update		(gpointer		var,
				 const gchar *		key,
				 GConfValueType		type)
{
  GConfClientNotifyFunc func;

  switch (type)
    {
    case GCONF_VALUE_BOOL:
      func = auto_bool;
      break;

    case GCONF_VALUE_INT:
      func = auto_int;
      break;

    case GCONF_VALUE_FLOAT:
      func = auto_float;
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  return z_gconf_notify_add (key, func, var);
}

typedef struct {
  gchar *		key;		/* the key we monitor (copy) */
  GConfValueType	type;

  guint			cnxn;		/* GConf connection ID */

  gpointer		var;		/* optional auto-update */
  GObject *		object;		/* connected Widget */
} notify;

static void
notify_destroy			(notify *		n)
{
  if (0 != n->cnxn)
    gconf_client_notify_remove (gconf_client, n->cnxn);

  g_free (n->key);

  g_free (n);
}

static void
notify_add			(notify *		n,
				 const char *		key,
				 GConfClientNotifyFunc	func)
{
  GError *error = NULL;

  n->key = g_strdup (key);

  n->cnxn = gconf_client_notify_add (gconf_client, key,
				     func, n,
				     /* destroy */ NULL,
				     &error);
  if (error)
    {
      /* This is fatal, will not return. */
      g_error ("GConf notification '%s' error:\n%s\n",
	       key, error->message);
    }
}

static void
toggle_action_notify		(GConfClient *		client _unused_,
				 guint			cnxn_id _unused_,
				 GConfEntry *		entry,
				 notify *		n)
{
  GtkToggleAction *action;
  gboolean active;

  if (!entry->value)
    return; /* unset */

  active = gconf_value_get_bool (entry->value);
  action = GTK_TOGGLE_ACTION (n->object);

  /* Breaks recursion. */
  if (active == gtk_toggle_action_get_active (action))
    return;

  gtk_toggle_action_set_active (action, active);

  if (n->var)
    *((gboolean *) n->var) = active;
}

static void
toggle_action_toggled		(GtkToggleAction *	toggle_action,
				 notify *		n)
{
  gboolean active;

  active = gtk_toggle_action_get_active (toggle_action);

  /* Error ignored. */
  z_gconf_set_bool (n->key, active);

  if (n->var)
    *((gboolean *) n->var) = active;
}

/* Connects a GConf key to a GtkToggleAction. Sets the action state
   from the current value of the key, or keeps the current action state
   on failure. When the action state later changes, so will the key and
   vice versa (think gconf-editor). The connection remains until the
   action is destroyed. */
void
z_toggle_action_connect_gconf_key
				(GtkToggleAction *	toggle_action,
				 const gchar *		key)
{
  GError *error = NULL;
  notify *n;
  GConfValue *value;

  if ((value = gconf_client_get (gconf_client, key, &error)))
    {
      gboolean active;

      /* No error and value is set. Synchronize action with gconf. */

      active = gconf_value_get_bool (value);
      gconf_value_free (value);

      gtk_toggle_action_set_active (toggle_action, active);
    }
  else
    {
      /* Error ignored. */
      z_gconf_handle_get_error (key, &error);
    }

  n = g_malloc0 (sizeof (*n));

  n->var = NULL;
  n->object = G_OBJECT (toggle_action);

  notify_add (n, key, (GConfClientNotifyFunc) toggle_action_notify);

  g_signal_connect_data (G_OBJECT (toggle_action), "toggled",
			 G_CALLBACK (toggle_action_toggled), n,
			 (GClosureNotify) notify_destroy,
			 /* connect_flags */ 0);
}

static void
toggle_button_notify		(GConfClient *		client _unused_,
				 guint			cnxn_id _unused_,
				 GConfEntry *		entry,
				 notify *		n)
{
  GtkToggleButton *button;
  gboolean active;

  if (!entry->value)
    return; /* unset */

  active = gconf_value_get_bool (entry->value);
  button = GTK_TOGGLE_BUTTON (n->object);

  /* Breaks recursion. */
  if (active == gtk_toggle_button_get_active (button))
    return;

  gtk_toggle_button_set_active (button, active);

  if (n->var)
    *((gboolean *) n->var) = active;
}

static void
toggle_button_toggled		(GtkToggleButton *	toggle_button,
				 notify *		n)
{
  gboolean active;

  active = gtk_toggle_button_get_active (toggle_button);

  /* Error ignored. */
  z_gconf_set_bool (n->key, active);

  if (n->var)
    *((gboolean *) n->var) = active;
}

/* Creates a new GtkCheckButton connected to a GConf key. Sets
   the button state from the current value of the key, or if
   that fails, from active. When the button state later changes,
   so will the key and vice versa (think gconf-editor). Additionally
   when var is not NULL the value of the key will be stored here
   as with z_gconf_auto_update_bool(). */
GtkWidget *
z_gconf_check_button_new	(const gchar *		label,
				 const gchar *		key,
				 gboolean *		var,
				 gboolean		active)
{
  notify *n;

  n = g_malloc0 (sizeof (*n));

  n->var = var;
  n->object = G_OBJECT (gtk_check_button_new_with_mnemonic (label));

  if (!var)
    var = &active;

  /* Error ignored. */
  z_gconf_get_bool (var, key);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (n->object), *var);

  notify_add (n, key, (GConfClientNotifyFunc) toggle_button_notify);

  g_signal_connect_data (n->object, "toggled",
			 G_CALLBACK (toggle_button_toggled), n,
			 (GClosureNotify) notify_destroy,
			 /* connect_flags */ 0);

  return GTK_WIDGET (n->object);
}

static void
int_slider_notify		(GConfClient *		client _unused_,
				 guint			cnxn_id _unused_,
				 GConfEntry *		entry,
				 notify *		n)
{
  ZSpinSlider *spinslider;
  gint gvalue;
  gint svalue;

  if (!entry->value)
    return; /* unset */

  gvalue = gconf_value_get_int (entry->value);

  spinslider = Z_SPINSLIDER (n->object);
  svalue = z_spinslider_get_int_value (spinslider);

  /* Breaks recursion. */
  if (gvalue == svalue)
    return;

  z_spinslider_set_int_value (spinslider, gvalue);

  if (n->var)
    *((gint *) n->var) = gvalue;
}

static void
int_slider_changed		(GtkObject *		adj _unused_,
				 notify *		n)
{
  ZSpinSlider *spinslider;
  gint value;

  spinslider = Z_SPINSLIDER (n->object);
  value = z_spinslider_get_int_value (spinslider);

  /* Error ignored. */
  z_gconf_set_int (n->key, value);

  if (n->var)
    *((gint *) n->var) = value;
}

/* Creates a new integer SpinSlider connected to a GConf key. Sets
   the value from the current value of the key, or if that fails,
   from def_value. When the slider value later changes, so will the
   key and vice versa (think gconf-editor). Additionally
   when var is not NULL the value of the key will be stored here
   as with z_gconf_auto_update_int(). */
GtkWidget *
z_gconf_int_spinslider_new	(gint			def_value,
				 gint			min_value,
				 gint			max_value,
				 gint			step_incr,
				 gint			page_incr,
				 gint			page_size,
				 const gchar *		key,
				 gint *			var)
{
  notify *n;
  GtkObject *adj;
  GtkWidget *spinslider;

  n = g_malloc0 (sizeof (*n));

  n->var = var;

  if (!var)
    var = &def_value;

  /* Error ignored. */
  z_gconf_get_int (var, key);

  adj = gtk_adjustment_new ((gdouble) *var,
			    (gdouble) min_value,
			    (gdouble) max_value,
			    (gdouble) step_incr,
			    (gdouble) page_incr,
			    (gdouble) page_size);

  spinslider = z_spinslider_new (GTK_ADJUSTMENT (adj), NULL, NULL, *var, 0);

  n->object = G_OBJECT (spinslider);

  notify_add (n, key, (GConfClientNotifyFunc) int_slider_notify);

  g_signal_connect_data (G_OBJECT (adj), "value-changed",
			 G_CALLBACK (int_slider_changed), n,
			 (GClosureNotify) notify_destroy,
			 /* connect_flags */ 0);

  return spinslider;
}

static void
float_slider_notify		(GConfClient *		client _unused_,
				 guint			cnxn_id _unused_,
				 GConfEntry *		entry,
				 notify *		n)
{
  ZSpinSlider *spinslider;
  gdouble gvalue;
  gdouble svalue;

  if (!entry->value)
    return; /* unset */

  gvalue = gconf_value_get_float (entry->value);

  spinslider = Z_SPINSLIDER (n->object);
  svalue = z_spinslider_get_value (spinslider);

  /* Breaks recursion. */
  if (fabs (gvalue - svalue) < 0.0001)
    return;

  z_spinslider_set_value (spinslider, gvalue);

  if (n->var)
    *((gdouble *) n->var) = gvalue;
}

static void
float_slider_changed		(GtkObject *		adj _unused_,
				 notify *		n)
{
  ZSpinSlider *spinslider;
  gdouble value;

  spinslider = Z_SPINSLIDER (n->object);
  value = z_spinslider_get_value (spinslider);

  /* Error ignored. */
  z_gconf_set_float (n->key, value);

  if (n->var)
    *((gdouble *) n->var) = value;
}

GtkWidget *
z_gconf_float_spinslider_new	(gdouble		def_value,
				 gdouble		min_value,
				 gdouble		max_value,
				 gdouble		step_incr,
				 gdouble		page_incr,
				 gdouble		page_size,
				 gint			digits,
				 const gchar *		key,
				 gdouble *		var)
{
  notify *n;
  GtkObject *adj;
  GtkWidget *spinslider;

  n = g_malloc0 (sizeof (*n));

  n->var = var;

  if (!var)
    var = &def_value;

  /* Error ignored. */
  z_gconf_get_float (var, key);

  adj = gtk_adjustment_new (*var,
			    min_value,
			    max_value,
			    step_incr,
			    page_incr,
			    page_size);

  spinslider = z_spinslider_new (GTK_ADJUSTMENT (adj),
				 NULL, NULL, *var, digits);

  n->object = G_OBJECT (spinslider);

  notify_add (n, key, (GConfClientNotifyFunc) float_slider_notify);

  g_signal_connect_data (G_OBJECT (adj), "value-changed",
			 G_CALLBACK (float_slider_changed), n,
			 (GClosureNotify) notify_destroy,
			 /* connect_flags */ 0);

  return spinslider;
}

typedef struct {
  notify		n;
  const GConfEnumStringPair *lookup_table;
} notify_combo_box;

static void
combo_box_notify		(GConfClient *		client _unused_,
				 guint			cnxn_id _unused_,
				 GConfEntry *		entry,
				 notify_combo_box *	n)
{
  GtkComboBox *combo_box;

  combo_box = GTK_COMBO_BOX (n->n.object);

  if (entry->value)
    {
      const gchar *s;

      if ((s = gconf_value_get_string (entry->value)))
	{
	  guint i;

	  if (n->n.var)
	    *((const gchar **) n->n.var) = s;

	  for (i = 0; n->lookup_table[i].str; ++i)
	    {
	      if (0 == strcmp (s, n->lookup_table[i].str))
		{
		  gint index;

		  index = gtk_combo_box_get_active (combo_box);

		  if ((gint) i != index)
		    gtk_combo_box_set_active (combo_box, (gint) i);

		  return;
		}
	    }
	}
    }

  gtk_combo_box_set_active (combo_box, -1 /* unset */);
}

static void
combo_box_changed		(GtkComboBox *		combo_box,
				 notify_combo_box *	n)
{
  gint index;

  index = gtk_combo_box_get_active (combo_box);

  /* Error ignored. */
  z_gconf_set_string (n->n.key, n->lookup_table[index].str);

  if (n->n.var)
    *((const gchar **) n->n.var) = n->lookup_table[index].str;
}

/* Creates a new GtkComboBox connected to a GConf string key. The combo
   box is built from NULL terminated, _N localized string vector menu,
   the GConf lookup_table is used to convert item indices to GConf string
   values and vice versa. The function sets the active item from the
   current value of the key, or if that fails, to "unset". When the
   active item later changes, so will the key and vice versa. NOTE the
   lookup_table will be used until the combo box is destroyed, should
   be statically allocated. */
GtkWidget *
z_gconf_combo_box_new		(const gchar **		menu,
				 const gchar *		key,
				 const GConfEnumStringPair *lookup_table)
{
  GError *error = NULL;
  notify_combo_box *n;
  GtkComboBox *combo_box;
  guint i;
  gchar *s;

  n = g_malloc0 (sizeof (*n));

  n->n.var = NULL;
  n->n.object = G_OBJECT (gtk_combo_box_new_text ());

  combo_box = GTK_COMBO_BOX (n->n.object);

  for (i = 0; menu[i]; ++i)
    gtk_combo_box_append_text (combo_box, _(menu[i]));

  if ((s = gconf_client_get_string (gconf_client, key, &error)))
    {
      for (i = 0; lookup_table[i].str; ++i)
	{
	  if (0 == strcmp (s, lookup_table[i].str))
	    {
	      gtk_combo_box_set_active (combo_box, (int) i);
	      break;
	    }
	}
    }
  else
    {
      gtk_combo_box_set_active (combo_box, 0);
      z_gconf_handle_get_error (key, &error);
    }

  n->lookup_table = lookup_table; /* NOTE */

  notify_add (&n->n, key, (GConfClientNotifyFunc) combo_box_notify);

  g_signal_connect (n->n.object, "changed",
		    G_CALLBACK (combo_box_changed), n);

  g_signal_connect_swapped (n->n.object, "destroy",
			    G_CALLBACK (notify_destroy), n);

  return GTK_WIDGET (n->n.object);
}

gboolean
string_to_color			(GdkColor *		color,
				 const gchar *		string)
{
  gchar *s;
  gboolean r;
  guint value;
  guint i;

  g_return_val_if_fail (NULL != color, FALSE);
  g_return_val_if_fail (NULL != string, FALSE);

  s = string;
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

  if (0 != *s)
    goto failure;

  r = TRUE;

  color->pixel = 0;
  color->red = (value & 0xFF0000) >> 8;
  color->red |= color->red >> 8;
  color->green = value & 0xFF00;
  color->green |= color->green >> 8;
  color->blue = value & 0xFF;
  color->blue |= color->blue << 8;

 failure:
  return r;
}

extern gboolean
z_gconf_set_color		(const gchar *		key,
				 const GdkColor *	color)
{
  gchar *str;
  gboolean success;

  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (color != NULL, FALSE);

  str = g_strdup_printf ("#%02X%02X%02X",
			 color->red >> 8,
			 color->green >> 8,
			 color->blue >> 8);

  success = z_gconf_set_string (key, str);

  g_free (str);

  return success;
}

extern gboolean
z_gconf_get_color		(GdkColor *		color,
				 const gchar *		key)
{
  gchar *str;
  gboolean success;

  if (!z_gconf_get_string (&str, key))
    return FALSE;

  success = string_to_color (color, str);

  g_free (str);

  return success;
}

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
