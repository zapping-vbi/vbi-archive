/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
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

/* XXX gtk+ 2.3 GtkOptionMenu, GnomeColorPicker */
#undef GTK_DISABLE_DEPRECATED
#undef GNOME_DISABLE_DEPRECATED

#include "site_def.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <math.h>
#include <ctype.h>

/* Routines for building GUI elements dependant on the v4l device
   (such as the number of inputs and so on) */
#include "tveng.h"
#include "v4linterface.h"
#include "callbacks.h"
#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zconf.h"
#include "zmisc.h"
#include "interface.h"
#include "zvbi.h"
#include "osd.h"
#include "remote.h"
#include "globals.h"
#include "audio.h"
#include "mixer.h"
#include "properties-handler.h"

struct control_window;

struct control_window *ToolBox = NULL; /* Pointer to the last control box */
ZModel *z_input_model = NULL;

/* Minimize updates */
static gboolean freeze = FALSE, needs_refresh = FALSE;
static gboolean rebuild_channel_menu = TRUE;

static void
update_bundle				(ZModel		*model,
					 tveng_device_info *info)
{
  if (freeze)
    {
      needs_refresh = TRUE;
      return;
    }

  if (rebuild_channel_menu)
    {
      GtkMenuItem *channels;
      GtkMenuShell *menu;
      GtkWidget *menu_item;
      gint has_rf;

      channels = GTK_MENU_ITEM (lookup_widget (main_window, "channels"));
      menu = GTK_MENU_SHELL(gtk_menu_new());

      menu_item = gtk_tearoff_menu_item_new();
      gtk_widget_show(menu_item);
      gtk_menu_shell_append(menu, menu_item);

      gtk_widget_show(GTK_WIDGET(menu));
      gtk_menu_item_remove_submenu(channels);
      gtk_menu_item_set_submenu(channels, GTK_WIDGET(menu));

      has_rf = add_channel_entries(menu, 1, 16, info);

      if (0)
	gtk_widget_set_sensitive(GTK_WIDGET(channels), has_rf);
    }
}

static void
freeze_update (void)
{
  freeze = TRUE;
  needs_refresh = FALSE;
}

static void
thaw_update (void)
{
  freeze = FALSE;

  if (needs_refresh)
    update_bundle(z_input_model, main_info);

  needs_refresh = FALSE;
}

/*
 *  Control box
 */

struct control {
  struct control *		next;

  tveng_device_info *		info;

  tv_control *			ctrl;
  tv_callback *			tvcb;

  GtkWidget *			widget;
};

struct control_window
{
  GtkWidget *			window;
  GtkWidget *			hbox;

  struct control *		controls;

  /* This is only used for building */
  GtkWidget *			table;
  guint				index;
};

static void
on_tv_control_destroy		(tv_control *		ctrl,
				 void *			user_data)
{
  struct control *c = user_data;

  c->ctrl = NULL;
  c->tvcb = NULL;
}

static void
free_control			(struct control *	c)
{
  tv_callback_remove (c->tvcb);
  g_free (c);
}

static struct control *
add_control			(struct control_window *cb,
				 tveng_device_info *	info,
				 tv_control *		ctrl,
				 GtkWidget *		label,
				 GtkWidget *		crank,
				 GObject *		object,
				 const gchar *		signal,
				 void *			g_callback,
				 void *			tv_callback)
{
  struct control *c, **cp;

  g_assert (cb != NULL);
  g_assert (info != NULL);
  g_assert (ctrl != NULL);
  g_assert (crank != NULL);

  for (cp = &cb->controls; (c = *cp); cp = &c->next)
    ;

  c = g_malloc0 (sizeof (*c));
  
  c->info = info;
  c->ctrl = ctrl;

  c->widget = crank;

  gtk_table_resize (GTK_TABLE (cb->table), cb->index + 1, 2);

  if (label)
    {
      gtk_widget_show (label);

      gtk_table_attach (GTK_TABLE (cb->table), label,
			0, 1, cb->index, cb->index + 1,
			(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 3, 3);
    }

  gtk_widget_show (crank);

  gtk_table_attach (GTK_TABLE (cb->table), crank,
		    1, 2, cb->index, cb->index + 1,
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions) (0), 3, 3);

  if (object && signal && g_callback)
    g_signal_connect (object, signal, G_CALLBACK (g_callback), c);

  if (tv_callback)
    {
      c->tvcb = tv_control_add_callback (ctrl, tv_callback,
					 on_tv_control_destroy, c);
    }

  *cp = c;

  return c;
}

static GtkWidget *
control_symbol			(tv_control *		ctrl)
{
  static const struct {
    tv_control_id		id;
    const char *		stock_id;
  } pixmaps [] = {
    { TV_CONTROL_ID_BRIGHTNESS,	"zapping-brightness" },
    { TV_CONTROL_ID_CONTRAST,	"zapping-contrast" },
    { TV_CONTROL_ID_SATURATION,	"zapping-saturation" },
    { TV_CONTROL_ID_HUE,	"zapping-hue" },
  };
  GtkWidget *symbol;
  guint i;

  symbol = NULL;

  for (i = 0; i < G_N_ELEMENTS (pixmaps); ++i)
    if (0 && ctrl->id == pixmaps[i].id)
      {
	symbol = gtk_image_new_from_stock (pixmaps[i].stock_id,
					   GTK_ICON_SIZE_BUTTON);
	gtk_misc_set_alignment (GTK_MISC (symbol), 1.0, 0.5);
	symbol = z_tooltip_set_wrap (symbol, ctrl->label);
	break;
      }

  if (!symbol)
    {
      symbol = gtk_label_new (ctrl->label);
      gtk_widget_show (symbol);
      gtk_misc_set_alignment (GTK_MISC (symbol), 1.0, 0.5);
    }

  return symbol;
}

static void
on_control_slider_changed	(GtkAdjustment *	adjust,
				 gpointer		user_data)
{
  struct control *c = user_data;

  TV_CALLBACK_BLOCK (c->tvcb, tveng_set_control
		     (c->ctrl, (int) adjust->value, c->info));
}

static void
on_tv_control_integer_changed	(tv_control *		ctrl,
				 void *			user_data)
{
  struct control *c = user_data;

  SIGNAL_HANDLER_BLOCK (z_spinslider_get_spin_adj (c->widget),
			on_control_slider_changed,
			z_spinslider_set_value (c->widget, ctrl->value));
}

static void
create_slider			(struct control_window *cb,
				 tveng_device_info *	info,
				 tv_control *	ctrl)
{ 
  GObject *adj; /* Adjustment object for the slider */
  GtkWidget *spinslider;

  /* XXX use tv_control.step */

  adj = G_OBJECT (gtk_adjustment_new (ctrl->value,
				      ctrl->minimum, ctrl->maximum,
				      1, 10, 10));

  spinslider = z_spinslider_new (GTK_ADJUSTMENT (adj), NULL,
				 NULL, ctrl->reset, 0);

  z_spinslider_set_value (spinslider, ctrl->value);

  add_control (cb, info, ctrl, control_symbol (ctrl), spinslider,
	       adj, "value-changed", on_control_slider_changed,
	       on_tv_control_integer_changed);
}

static void
on_control_checkbutton_toggled	(GtkToggleButton *	tb,
				 gpointer		user_data)
{
  struct control *c = user_data;

  if (c->ctrl->id == TV_CONTROL_ID_MUTE)
    {
      TV_CALLBACK_BLOCK (c->tvcb, tv_mute_set
			 (c->info, gtk_toggle_button_get_active (tb)));
      /* Update tool button & menu XXX switch to callback */
      set_mute (3, /* controls */ FALSE, /* osd */ FALSE);
    }
  else
    {
      TV_CALLBACK_BLOCK (c->tvcb, tveng_set_control
			 (c->ctrl, gtk_toggle_button_get_active (tb), c->info));
    }
}

static void
on_tv_control_boolean_changed	(tv_control *		ctrl,
				 void *			user_data)
{
  struct control *c = user_data;

  SIGNAL_HANDLER_BLOCK (c->widget, on_control_checkbutton_toggled,
			gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON (c->widget), ctrl->value));
}

static void
create_checkbutton		(struct control_window *cb,
				 tveng_device_info *	info,
				 tv_control *		ctrl)
{
  GtkWidget *check_button;

  check_button = gtk_check_button_new_with_label (ctrl->label);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
				ctrl->value);

  add_control (cb, info, ctrl, /* label */ NULL, check_button,
	       G_OBJECT (check_button), "toggled",
	       on_control_checkbutton_toggled,
	       on_tv_control_boolean_changed);
}

static void
on_control_menuitem_activate	(GtkMenuItem *		menuitem,
				 gpointer		user_data)
{
  struct control *c = user_data;
  gint value;

  value = (gint) g_object_get_data (G_OBJECT (menuitem), "value");

  tveng_set_control (c->ctrl, value, c->info);
}

static void
create_menu			(struct control_window *cb,
				 tveng_device_info *	info,
				 tv_control *		ctrl)
{
  GtkWidget *label; /* This shows what the menu is for */
  GtkWidget *option_menu; /* The option menu */
  GtkWidget *menu; /* The menu displayed */
  GtkWidget *menu_item; /* Each of the menu items */
  struct control *c;
  guint i;

  label = gtk_label_new (ctrl->label);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);

  option_menu = gtk_option_menu_new ();
  menu = gtk_menu_new ();
  gtk_widget_show (menu);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);

  c = add_control (cb, info, ctrl, label, option_menu,
		   NULL, NULL, NULL, NULL);

  /* Start querying menu_items and building the menu */
  for (i = 0; ctrl->menu[i] != NULL; i++)
    {
      menu_item = gtk_menu_item_new_with_label (_(ctrl->menu[i]));
      gtk_widget_show (menu_item);

      g_object_set_data (G_OBJECT (menu_item), "value", 
			 GINT_TO_POINTER (i));

      g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (on_control_menuitem_activate), c);

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    }

  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu),
			       ctrl->value);
}

static void
on_control_button_clicked	(GtkButton *		button,
				 gpointer		user_data)
{
  struct control *c = user_data;

  tveng_set_control (c->ctrl, 1, c->info);
}

static void
create_button			(struct control_window *cb,
				 tveng_device_info *	info,
				 tv_control *		ctrl)
{
  GtkWidget *button;

  button = gtk_button_new_with_label (ctrl->label);

  add_control (cb, info, ctrl, /* label */ NULL, button,
	       G_OBJECT (button), "clicked",
	       on_control_button_clicked, NULL);
}

static void
on_color_set			(GnomeColorPicker *	colorpicker,
				 guint			arg1,
				 guint			arg2,
				 guint			arg3,
				 guint			arg4,
				 gpointer		user_data)
{
  struct control *c = user_data;
  guint color;

  color  = (arg1 >> 8) << 16;	/* red */
  color += (arg2 >> 8) << 8;	/* green */
  color += (arg3 >> 8);		/* blue */
  /* arg4 alpha ignored */

  tveng_set_control (c->ctrl, color, c->info);
}

static void
create_color_picker		(struct control_window *cb,
				 tveng_device_info *	info,
				 tv_control *		ctrl)
{
  GtkWidget *label;
  GnomeColorPicker *color_picker;
  gchar *buffer;

  label = gtk_label_new (ctrl->label);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);

  color_picker = GNOME_COLOR_PICKER (gnome_color_picker_new ());
  gnome_color_picker_set_use_alpha (color_picker, FALSE);
  gnome_color_picker_set_i8 (color_picker,
			     (ctrl->value & 0xff0000) >> 16,
			     (ctrl->value & 0xff00) >> 8,
			     (ctrl->value & 0xff),
			     0);

  /* TRANSLATORS: In controls box, color picker control,
     something like "Adjust Chroma-Key" for overlay. */
  buffer = g_strdup_printf (_("Adjust %s"), ctrl->label);
  gnome_color_picker_set_title (color_picker, buffer);
  g_free (buffer);

  add_control (cb, info, ctrl, label, GTK_WIDGET(color_picker),
	       G_OBJECT (color_picker), "color-set",
	       on_color_set, NULL);
}

static void
add_controls			(struct control_window *cb,
				 tveng_device_info *	info)
{
  tv_control *ctrl;

  if (cb->hbox)
    {
      struct control *c;

      while ((c = cb->controls))
	{
	  cb->controls = c->next;
	  free_control (c);
	}
      
      gtk_container_remove (GTK_CONTAINER (cb->window), cb->hbox);
    }

  cb->hbox = gtk_hbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (cb->hbox), 6);
  gtk_container_add (GTK_CONTAINER (cb->window), cb->hbox);

  /* Update the values of all the controls */
  if (-1 == tveng_update_controls (info))
    {
      ShowBox ("Tveng critical error, Zapping will exit NOW.",
	       GTK_MESSAGE_ERROR);
      g_error ("tveng critical: %s", info->error);
    }

  cb->table = NULL;
  cb->index = 0;

  ctrl = NULL;

  while ((ctrl = tv_next_control (info, ctrl)))
    {
      if (ctrl->id == TV_CONTROL_ID_HUE)
	if (info->cur_video_standard)
	  if (!(info->cur_video_standard->videostd_set & TV_VIDEOSTD_SET_NTSC))
	    {
	      /* Useless. XXX connect to standard change callback. */
	      tveng_set_control (ctrl, ctrl->reset, info);
	      continue;
	    }

      if ((cb->index % 20) == 0)
	{
	  if (cb->table)
	    {
	      gtk_widget_show (cb->table);
	      gtk_box_pack_start_defaults (GTK_BOX (cb->hbox), cb->table);
	    }

	  cb->table = gtk_table_new (1, 2, FALSE);
	}

      switch (ctrl->type)
	{
	case TV_CONTROL_TYPE_INTEGER:
	  create_slider (cb, info, ctrl);
	  break;

	case TV_CONTROL_TYPE_BOOLEAN:
	  create_checkbutton (cb, info, ctrl);
	  break;

	case TV_CONTROL_TYPE_CHOICE:
	  create_menu (cb, info, ctrl);
	  break;

	case TV_CONTROL_TYPE_ACTION:
	  create_button (cb, info, ctrl);
	  break;

	case TV_CONTROL_TYPE_COLOR:
	  create_color_picker (cb, info, ctrl);
	  break;

	default:
	  g_warning ("Type %d of control %s is not supported",
		     ctrl->type, ctrl->label);
	  continue;
	}

      cb->index++;
    }

  if (cb->table)
    {
      gtk_widget_show (cb->table);
      gtk_box_pack_start_defaults (GTK_BOX (cb->hbox), cb->table);
    }

  gtk_widget_show (cb->hbox);
}

static gboolean
on_control_window_key_press	(GtkWidget *		widget,
				 GdkEventKey *		event,
				 gpointer		user_data)
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

  return on_user_key_press (widget, event, user_data)
    || on_picture_size_key_press (widget, event, user_data)
    || on_channel_key_press (widget, event, user_data);
}

static void
on_control_window_destroy	(GtkWidget *		widget,
				 gpointer		user_data)
{
  struct control_window *cb = user_data;
  struct control *c;

  ToolBox = NULL;

  while ((c = cb->controls))
    {
      cb->controls = c->next;
      free_control (c);
    }

  g_free (cb);

  /* See below.
     gtk_widget_set_sensitive (lookup_widget (main_window, "toolbar-controls"), TRUE);
  */
}

static void
create_control_window		(void)
{
  struct control_window *cb;

  cb = g_malloc0 (sizeof (*cb));

  cb->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (cb->window), _("Controls"));
  g_signal_connect (G_OBJECT (cb->window), "destroy",
		    G_CALLBACK (on_control_window_destroy), cb);
  g_signal_connect (G_OBJECT(cb->window), "key-press-event",
		    G_CALLBACK (on_control_window_key_press), cb);

  add_controls (cb, main_info);

  gtk_widget_show (cb->window);

  /* Not good because it may just raise a hidden control window.
     gtk_widget_set_sensitive (lookup_widget (main_window, "toolbar-controls"), FALSE);
   */

  ToolBox = cb;
}

static PyObject *
py_control_box			(PyObject *		self,
				 PyObject *		args)
{
  if (ToolBox == NULL)
    create_control_window ();
  else
    gtk_window_present (GTK_WINDOW (ToolBox->window));

  py_return_none;
}

void
update_control_box		(tveng_device_info *	info)
{
  struct control *c;
  tv_control *ctrl;

  if (!ToolBox)
    return;

  c = ToolBox->controls;

  ctrl = NULL;

  while ((ctrl = tv_next_control (info, ctrl)))
    {
      /* XXX Is this safe? Unlikely. */
      if (!c || c->ctrl != ctrl)
	goto rebuild;

      switch (ctrl->type)
	{
	case TV_CONTROL_TYPE_INTEGER:
	  on_tv_control_integer_changed (ctrl, c);
	  break;

	case TV_CONTROL_TYPE_BOOLEAN:
	  on_tv_control_boolean_changed (ctrl, c);
	  break;

	case TV_CONTROL_TYPE_CHOICE:
	  gtk_option_menu_set_history
	    (GTK_OPTION_MENU (c->widget), ctrl->value);
	  break;

	case TV_CONTROL_TYPE_ACTION:
	  break;

	case TV_CONTROL_TYPE_COLOR:
	  gnome_color_picker_set_i8
	    (GNOME_COLOR_PICKER (c->widget),
	     (ctrl->value & 0xff0000) >> 16,
	     (ctrl->value & 0xff00) >> 8,
	     (ctrl->value & 0xff),
	     0);
	  break;

	default:
	  g_warning ("Type %d of control %s is not supported",
		     ctrl->type, ctrl->label);
	  continue;
	}

      c = c->next;
    }

  return;
  
 rebuild:
  add_controls (ToolBox, info);
}



/* XXX these functions change an a/v source property but
   do not switch away from the current tveng_tuned_channel.
   Maybe there should be a -1 tuned channel intended to
   change on the fly, and a channel history to properly
   switch back (channel up, down etc). We also need a
   function to find a tuned channel already matching the
   new configuration. */

gboolean
z_switch_input			(int hash, tveng_device_info *info)
{
  const tv_video_line *l;

  if (!(l = tv_video_input_by_hash (info, hash)))
    {
#if 0 /* annoying */
      ShowBox("Couldn't find input with hash %x",
	      GTK_MESSAGE_ERROR, hash);
#endif
      return FALSE;
    }

  if (!tv_set_video_input (info, l))
    {
      ShowBox("Couldn't switch to input %s\n%s",
	      GTK_MESSAGE_ERROR,
	      l->label, info->error);
      return FALSE;
    }

  zmodel_changed(z_input_model);

  return TRUE;
}

gboolean
z_switch_standard		(int hash, tveng_device_info *info)
{
  const tv_video_standard *s;

  for (s = tv_next_video_standard (info, NULL);
       s; s = tv_next_video_standard (info, s))
    if (s->hash == hash)
      break;

  if (!s)
    {
#if 0 /* annoying */
      if (info->video_standards)
	ShowBox("Couldn't find standard with hash %x",
		GTK_MESSAGE_ERROR, hash);
#endif
      return FALSE;
    }

  if (!tv_set_video_standard (info, s))
    {
      ShowBox("Couldn't switch to standard %s\n%s",
	      GTK_MESSAGE_ERROR,
	      s->label, info->error);
      return FALSE;
    }

  return TRUE;
}

/* Returns a newly allocated copy of the string, normalized */
static char* normalize(const char *string)
{
  int i = 0;
  const char *strptr=string;
  char *result;

  t_assert(string != NULL);

  result = strdup(string);

  t_assert(result != NULL);

  while (*strptr != 0) {
    if (*strptr == '_' || *strptr == '-' || *strptr == ' ') {
      strptr++;
      continue;
    }
    result[i] = tolower(*strptr);

    strptr++;
    i++;
  }
  result[i] = 0;

  return result;
}

/* nomalize and compare */
static int normstrcmp (const char * in1, const char * in2)
{
  char *s1 = normalize(in1);
  char *s2 = normalize(in2);

  t_assert(in1 != NULL);
  t_assert(in2 != NULL);

  /* Compare the strings */
  if (!strcmp(s1, s2)) {
    free(s1);
    free(s2);
    return 1;
  } else {
    free(s1);
    free(s2);
    return 0;
  }
}

tveng_tc_control *
zconf_get_controls		(guint			num_controls,
				 const gchar *		path)
{
  tveng_tc_control *tcc;
  gchar *array;
  guint i;

  if (num_controls == 0)
    return NULL;

  tcc = g_malloc0 (sizeof (*tcc) * num_controls);
  array = g_strconcat (path, "/controls", NULL);

  for (i = 0; i < num_controls; i++)
    {
      gchar *control;
      gchar *name;
      const gchar *s;

      if (!zconf_get_nth (i, &control, array))
	{
	  g_warning ("Saved control %u is malformed, skipping", i);
	  continue;
	}
  
      name = g_strconcat (control, "/name", NULL);
      s = zconf_get_string (NULL, name);
      g_free (name);

      if (!s)
	{
	  g_free (control);
	  continue;
	}

      strncpy (tcc[i].name, s, 32);

      name = g_strconcat (control, "/value", NULL);
      zconf_get_float (&tcc[i].value, name);
      g_free (name);

      g_free (control);
    }

  g_free (array);

  return tcc;
}

void
zconf_create_controls		(tveng_tc_control *	tcc,
				 guint			num_controls,
				 const gchar *		path)
{
  guint i;

  for (i = 0; i < num_controls; i++)
    {
      gchar *name;

      name = g_strdup_printf ("%s/controls/%d/name", path, i);
      zconf_create_string (tcc[i].name, "Control name", name);
      g_free (name);

      name = g_strdup_printf ("%s/controls/%d/value", path, i);
      zconf_create_float (tcc[i].value, "Control value", name);
      g_free (name);
    }
}

tveng_tc_control *
tveng_tc_control_by_id		(const tveng_device_info *info,
				 tveng_tc_control *	tcc,
				 guint			num_controls,
				 tv_control_id		id)
{
  const tv_control *c;
  guint i;

  for (c = NULL; (c = tv_next_control (info, c));)
    if (c->id == id)
      break;

  if (NULL != c)
    for (i = 0; i < num_controls; ++i)
      if (normstrcmp (c->label, tcc[i].name))
	return tcc + i;

  return NULL;
}

gint
load_control_values		(tveng_device_info *	info,
				 tveng_tc_control *	tcc,
				 guint			num_controls)
{
  tv_control *ctrl;
  guint i;
  gint mute = 0;

  g_assert (info != NULL);

  if (!tcc || num_controls == 0)
    return 0;

  ctrl = NULL;

  while ((ctrl = tv_next_control (info, ctrl)))
    for (i = 0; i < num_controls; i++)
      if (normstrcmp (ctrl->label, tcc[i].name))
	{
	  gint value;

	  value = (ctrl->minimum
		   + (ctrl->maximum - ctrl->minimum)
		   * tcc[i].value);
#if 0
	  if (ctrl->id == TV_CONTROL_ID_MUTE)
	    {
	      if (skip_mute)
		mute = value;
	      else
		set_mute (value, /* controls */ TRUE, /* osd */ FALSE);
	    }
	  else
#endif
	    {
	      tveng_set_control (ctrl, value, info);
	    }

	  break;
	}

  return mute;
}

void
store_control_values		(tveng_device_info *	info,
				 tveng_tc_control **	tccp,
				 guint *		num_controls_p)
{
  tveng_tc_control *tcc;
  guint num_controls;
  tv_control *ctrl;
  guint i;

  g_assert (info != NULL);
  g_assert (tccp != NULL);
  g_assert (num_controls_p != NULL);

  tcc = NULL;
  num_controls = 0;

  ctrl = NULL;

  while ((ctrl = tv_next_control (info, ctrl)))
    num_controls++;

  if (num_controls > 0)
    {
      tcc = g_malloc (sizeof (*tcc) * num_controls);

      ctrl = NULL;
      i = 0;

      while (i < num_controls && (ctrl = tv_next_control (info, ctrl)))
	{
	  int value;

	  value = ctrl->value;

	  g_strlcpy (tcc[i].name, ctrl->label, 32);
	  tcc[i].name[31] = 0;

	  if (ctrl->maximum > ctrl->minimum)
	    tcc[i].value = (((gfloat) value) - ctrl->minimum)
	      / ((gfloat) ctrl->maximum - ctrl->minimum);
	  else
	    tcc[i].value = 0;

	  i++;
	}
    }

  *tccp = tcc;
  *num_controls_p = num_controls;
}

/*
  Substitute the special search keywords by the appropiate thing,
  returns a newly allocated string, and g_free's the given string.
  Valid search keywords:
  $(alias) -> tc->name
  $(index) -> tc->index
  $(id) -> tc->rf_name
*/
static
gchar *substitute_keywords	(gchar		*string,
				 tveng_tuned_channel *tc,
				 gchar          *default_name)
{
  gint i;
  gchar *found, *buffer = NULL, *p;
  gchar *search_keys[] =
  {
    "$(alias)",
    "$(index)",
    "$(id)",
    "$(input)",
    "$(standard)",
    "$(freq)",
    "$(title)",
    "$(rating)"
  };
  gint num_keys = sizeof(search_keys)/sizeof(*search_keys);

  if ((!string) || (!*string) || (!tc))
    {
      g_free(string);
      return g_strdup("");
    }

  for (i=0; i<num_keys; i++)
     while ((found = strstr(string, search_keys[i])))
     {
       switch (i)
	 {
	 case 0:
	   if (tc->name)
	     buffer = g_strdup(tc->name);
	   else if (default_name)
	     buffer = g_strdup(default_name);
	   else
	     buffer = g_strdup(_("Unknown"));
	   break;
	 case 1:
	   buffer = g_strdup_printf("%d", tc->index+1);
	   break;
	 case 2:
	   if (tc->rf_name)
	     buffer = g_strdup(tc->rf_name);
	   else
	     buffer = g_strdup(_("Unnamed"));
	   break;
	 case 3:
	   {
	     const tv_video_line *l;

	     if ((l = tv_video_input_by_hash (main_info, tc->input)))
	       buffer = g_strdup (l->label);
	     else
	       buffer = g_strdup (_("No input"));
	   
	     break;
	   }
	 case 4:
	   {
	     const tv_video_standard *s;

	     if ((s = tv_video_standard_by_hash (main_info, tc->standard)))
	       buffer = g_strdup (s->label);
	     else
	       buffer = g_strdup (_("No standard"));

	     break;
	   }
	 case 5:
	   buffer = g_strdup_printf("%d", tc->frequ / 1000);
	   break;
#if 0 /* Temporarily removed. ifdef HAVE_LIBZVBI */
	 case 6: /* title */
	   buffer = zvbi_current_title();
	   break;
	 case 7: /* rating */
	   buffer = g_strdup(zvbi_current_rating());
	   break;
#else
	 case 6: /* title */
	 case 7: /* rating */
	   buffer = g_strdup("");
	   break;
#endif
	 default:
	   g_assert_not_reached();
	   break;
	 }

       *found = 0;
       
       p = g_strconcat(string, buffer,
		       found+strlen(search_keys[i]), NULL);
       g_free(string);
       g_free(buffer);
       string = p;
     }

  return string;
}

void
z_set_main_title	(tveng_tuned_channel	*channel,
			 gchar *default_name)
{
  tveng_tuned_channel ch;
  gchar *buffer = NULL;

  CLEAR (ch);

  if (!channel)
    channel = &ch;

  if (channel != &ch
      || channel->name || default_name)
    buffer = substitute_keywords(g_strdup(zcg_char(NULL, "title_format")),
				 channel, default_name);
  if (buffer && *buffer && main_window)
    gtk_window_set_title(GTK_WINDOW(main_window), buffer);
  else if (main_window)
    gtk_window_set_title(GTK_WINDOW(main_window), "Zapping");

  g_free(buffer);

  if (channel != &ch)
    cur_tuned_channel = channel->index;
}

/* Do not save the control values in the first switch_channel */
static gboolean first_switch = TRUE;

void
z_switch_channel		(tveng_tuned_channel *	channel,
				 tveng_device_info *	info)
{
  gboolean was_first_switch = first_switch;
  tveng_tuned_channel *tc;
  gboolean in_global_list;

  if (!channel)
    return;

  in_global_list = tveng_tuned_channel_in_list (global_channel_list, channel);

  if (in_global_list &&
      (tc = tveng_tuned_channel_nth (global_channel_list, cur_tuned_channel)))
    {
      if (!first_switch)
	{
	  g_free(tc->controls);
	  if (zcg_bool(NULL, "save_controls"))
	    store_control_values(info, &tc->controls, &tc->num_controls);
	  else
	    {
	      tc->num_controls = 0;
	      tc->controls = NULL;
	    }
	}
      else
	{
	  first_switch = FALSE;
	}

#ifdef HAVE_LIBZVBI
      tc->caption_pgno = zvbi_caption_pgno;
#endif
    }

  /* Always: if ((avoid_noise = zcg_bool (NULL, "avoid_noise"))) */
    tv_quiet_set (main_info, TRUE);

  freeze_update();

  /* force rebuild on startup */
  if (was_first_switch)
    zmodel_changed(z_input_model);

  if (channel->input)
    z_switch_input(channel->input, info);

  if (channel->standard)
    z_switch_standard(channel->standard, info);

  /* Always: if (avoid_noise) */
    reset_quiet (main_info, /* delay ms */ 500);

  if (info->cur_video_input
      && info->cur_video_input->type == TV_VIDEO_LINE_TYPE_TUNER)
    if (!tv_set_tuner_frequency (info, channel->frequ))
      ShowBox(info -> error, GTK_MESSAGE_ERROR);

  if (in_global_list)
    z_set_main_title(channel, NULL);
  else
    gtk_window_set_title(GTK_WINDOW(main_window), "Zapping");

  thaw_update();

  if (channel->num_controls && zcg_bool(NULL, "save_controls"))
    /* XXX should we save mute state per-channel? */
    load_control_values(info, channel->controls, channel->num_controls);

  update_control_box(info);

#ifdef HAVE_LIBZVBI
  zvbi_caption_pgno = channel->caption_pgno;

  if (zvbi_caption_pgno <= 0)
    python_command_printf (NULL, "zapping.closed_caption(0)");

  zvbi_channel_switched();

  if (info->current_mode == TVENG_CAPTURE_PREVIEW)
    osd_render_markup (NULL,
	("<span foreground=\"yellow\">%s</span>"), channel->name);
#endif
}

static void
select_channel (gint num_channel)
{
  tveng_tuned_channel * channel =
    tveng_tuned_channel_nth (global_channel_list, num_channel);

  if (!channel)
    {
      g_warning("Cannot tune given channel %d (no such channel)",
		num_channel);
      return;
    }

  z_switch_channel(channel, main_info);
}

static PyObject*
py_channel_up			(PyObject *self, PyObject *args)
{
  gint num_channels = tveng_tuned_channel_num(global_channel_list);
  gint new_channel;

  if (num_channels == 0) /* If there are no tuned channels stop
			    processing */
    py_return_none;

  new_channel = cur_tuned_channel + 1;
  if (new_channel >= num_channels)
    new_channel = 0;

  select_channel(new_channel);

  py_return_none;
}

static PyObject *
py_channel_down			(PyObject *self, PyObject *args)
{
  gint num_channels = tveng_tuned_channel_num(global_channel_list);
  gint new_channel;

  if (num_channels == 0) /* If there are no tuned channels stop
			    processing */
    py_return_none;

  new_channel = cur_tuned_channel - 1;
  if (new_channel < 0)
    new_channel = num_channels - 1;
  
  select_channel(new_channel);

  py_return_none;
}

/*
 *  Select a channel by index into the the channel list.
 */
static PyObject*
py_set_channel				(PyObject *self, PyObject *args)
{
  gint num_channels;
  gint i;
  int ok = PyArg_ParseTuple (args, "i", &i);

  if (!ok)
    py_return_false;

  num_channels = tveng_tuned_channel_num(global_channel_list);

  if (i >= 0 && i < num_channels)
    {
      select_channel(i);
      py_return_true;
    }

  py_return_false;
}

/*
 *  Select a channel by station name ("MSNBCBS", "Linux TV", ...),
 *  when not found by channel name ("5", "S7", ...)
 */
static PyObject*
py_lookup_channel			(PyObject *self, PyObject *args)
{
  tveng_tuned_channel *tc;
  char *name;
  int ok = PyArg_ParseTuple (args, "s", &name);

  if (!ok)
    py_return_false;

  if ((tc = tveng_tuned_channel_by_name (global_channel_list, name)))
    {
      z_switch_channel(tc, main_info);
      py_return_true;
    }

  if ((tc = tveng_tuned_channel_by_rf_name (global_channel_list, name)))
    {
      z_switch_channel(tc, main_info);
      py_return_true;
    }

  py_return_false;
}

static gchar			kp_chsel_buf[8];
static gint			kp_chsel_prefix;
static gboolean			kp_clear;
static gboolean			kp_lirc; /* XXX */

static gint
channel_txl			(void)
{
  gint txl;

  /* 0 = channel list number, 1 = RF channel number */
  txl = zconf_get_integer (NULL, "/zapping/options/main/channel_txl");

  if (txl < 0)
    txl = 0; /* historical: -1 disabled keypad channel number entering */

  return txl;
}

static void
kp_enter			(gint			txl)
{
  tveng_tuned_channel *tc;

  if (!isdigit (kp_chsel_buf[0]) || txl >= 1)
    tc = tveng_tuned_channel_by_rf_name (global_channel_list, kp_chsel_buf);
  else
    tc = tveng_tuned_channel_nth (global_channel_list, atoi (kp_chsel_buf));

  if (tc)
    z_switch_channel (tc, main_info);
}

static void
kp_timeout			(gboolean		timer)
{
  if (timer && kp_chsel_buf[0] != 0)
    kp_enter (channel_txl ());

  if (kp_clear)
    {
      kp_chsel_buf[0] = 0;
      kp_chsel_prefix = 0;
    }
}

static gboolean
kp_key_press			(GdkEventKey *		event,
				 gint			txl)
{
  switch (event->keyval)
    {
#ifdef HAVE_LIBZVBI /* FIXME */
    case GDK_KP_0 ... GDK_KP_9:
      {
	tveng_tuned_channel *tc;
	gint len;

	len = strlen (kp_chsel_buf);

	if (len >= sizeof (kp_chsel_buf) - 1)
	  memcpy (kp_chsel_buf, kp_chsel_buf + 1, len--);

	kp_chsel_buf[len] = event->keyval - GDK_KP_0 + '0';
	kp_chsel_buf[len + 1] = 0;

      show:
	tc = NULL;

	if (txl == 1)
	  {
	    guint match = 0;

	    /* RF channel name completion */

	    len = strlen (kp_chsel_buf);

	    for (tc = tveng_tuned_channel_first (global_channel_list);
		 tc; tc = tc->next)
	      if (!tc->rf_name || tc->rf_name[0] == 0)
		{
		  continue;
		}
	      else if (0 == strncmp (tc->rf_name, kp_chsel_buf, len))
		{
		  if (strlen (tc->rf_name) == len)
		    break; /* exact match */

		  if (match++ > 0)
		    {
		      tc = NULL;
		      break; /* ambiguous */
		    }
		}

	    if (tc)
	      strncpy (kp_chsel_buf, tc->rf_name, sizeof (kp_chsel_buf) - 1);
	  }

	kp_clear = FALSE;
	osd_render_markup (kp_timeout, ("<span foreground=\"green\">%s</span>"),
			   kp_chsel_buf);
	kp_clear = TRUE;

	if (txl == 0)
	  {
	    guint num = atoi (kp_chsel_buf);

	    /* Switch to channel if the number is unambiguous */

	    if (num == 0 || (num * 10) >= tveng_tuned_channel_num (global_channel_list))
	      tc = tveng_tuned_channel_nth (global_channel_list, num);
	  }

	if (!tc)
	  return TRUE; /* unknown channel */

	z_switch_channel (tc, main_info);

	kp_chsel_buf[0] = 0;
	kp_chsel_prefix = 0;

	return TRUE;
      }

    case GDK_KP_Decimal:
      if (txl >= 1)
	{
	  const tveng_tuned_channel *tc;
	  const gchar *rf_table;
	  tv_rf_channel ch;
	  const char *prefix;

	  /* Run through all RF channel prefixes incl. nil (== clear) */

	  if (!(rf_table = zconf_get_string (NULL, "/zapping/options/main/current_country")))
	    {
	      tc = tveng_tuned_channel_nth (global_channel_list, cur_tuned_channel);

	      if (!tc || !(rf_table = tc->rf_table) || rf_table[0] == 0)
		return TRUE; /* dead key */
	    }

	  if (!tv_rf_channel_table_by_name (&ch, rf_table))
	    return TRUE; /* dead key */

	  if ((prefix = tv_rf_channel_table_prefix (&ch, kp_chsel_prefix)))
	    {
	      strncpy (kp_chsel_buf, prefix, sizeof (kp_chsel_buf) - 1);
	      kp_chsel_buf[sizeof (kp_chsel_buf) - 1] = 0;
	      kp_chsel_prefix++;
	      goto show;
	    }
	}

      kp_clear = TRUE;
      osd_render_markup (kp_timeout,
			 "<span foreground=\"black\">/</span>");
      return TRUE;

    case GDK_KP_Enter:
      kp_enter (txl);

      kp_chsel_buf[0] = 0;
      kp_chsel_prefix = 0;

      return TRUE;
#endif

    default:
      break;
    }

  return FALSE; /* don't know, pass it on */
}

/*
 * Called from alirc.c, preliminary.
 */
gboolean
channel_key_press		(GdkEventKey *		event)
{
  kp_lirc = TRUE;

  return kp_key_press (event, channel_txl ());
}

gboolean
on_channel_key_press			(GtkWidget *	widget,
					 GdkEventKey *	event,
					 gpointer	user_data)
{
  tveng_tuned_channel *tc;
  z_key key;
  gint i;

  if (1) /* XXX !disabled */
    if (kp_key_press (event, channel_txl ()))
      {
	kp_lirc = FALSE;
	return TRUE;
      }

  /* Channel accelerators */

  key.key = gdk_keyval_to_lower (event->keyval);
  key.mask = event->state;

  for (i = 0; (tc = tveng_tuned_channel_nth(global_channel_list, i)); i++)
    if (z_key_equal (tc->accel, key))
      {
	select_channel (tc->index);
	return TRUE;
      }

  return FALSE; /* don't know, pass it on */
}

/* ------------------------------------------------------------------------- */

typedef struct {
  tveng_device_info *	info;
  GtkMenuItem *		menu_item;
  tv_callback *		callback;
} source_menu;

static void
on_menu_item_destroy		(gpointer		user_data)
{
  source_menu *sm = user_data;

  tv_callback_remove (sm->callback);
  g_free (sm);
}

static void
append_radio_menu_item		(GtkMenuShell **	menu_shell,
				 GSList **		group,
				 const gchar *		label,
				 gboolean		active,
				 GCallback		handler,
				 const source_menu *	sm)
{
  GtkWidget *menu_item;

  menu_item = gtk_radio_menu_item_new_with_label (*group, label);
  gtk_widget_show (menu_item);

  *group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menu_item));

  gtk_menu_shell_append (*menu_shell, menu_item);

  g_signal_connect (G_OBJECT (menu_item), "activate", handler, (gpointer) sm);
}

/* Video standards */

static void
on_video_standard_activate	(GtkMenuItem *		menu_item,
				 gpointer		user_data);
static GtkWidget *
video_standard_menu		(source_menu *		sm);

static void
select_cur_video_standard_item	(GtkMenuShell *		menu_shell,
				 tveng_device_info *	info)
{
  GtkWidget *menu_item;
  guint index;

  g_assert (info->cur_video_standard != NULL);

  index = tv_video_standard_position (info, info->cur_video_standard);
  menu_item = z_menu_shell_nth_item (menu_shell, index + 1 /* tear-off */);
  g_assert (menu_item != NULL);

  SIGNAL_HANDLER_BLOCK (menu_item, on_video_standard_activate,
			gtk_menu_shell_select_item (menu_shell, menu_item));
}

static void
on_tv_video_standard_change	(tveng_device_info *	info,
				 void *			user_data)
{
  source_menu *sm = user_data;

  if (!sm->info->cur_video_standard)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (sm->menu_item), FALSE);
      gtk_menu_item_remove_submenu (sm->menu_item);
    }
  else
    {
      GtkWidget *w;

      if (!(w = gtk_menu_item_get_submenu (sm->menu_item)))
	{
	  gtk_widget_set_sensitive (GTK_WIDGET (sm->menu_item), TRUE);
	  gtk_menu_item_set_submenu (sm->menu_item, video_standard_menu (sm));
	}
      else
	{
	  GtkMenuShell *menu_shell;

	  menu_shell = GTK_MENU_SHELL (w);
	  select_cur_video_standard_item (menu_shell, sm->info);
	}
    }
}

static void
on_video_standard_activate	(GtkMenuItem *		menu_item,
				 gpointer		user_data)
{
  const source_menu *sm = user_data;
  GtkMenuShell *menu_shell;
  const tv_video_standard *s;
  gboolean success;
  gint index;

  menu_shell = GTK_MENU_SHELL (gtk_menu_item_get_submenu (sm->menu_item));
  index = g_list_index (menu_shell->children, menu_item);

  success = FALSE;

  rebuild_channel_menu = FALSE; /* old stuff */

  if (index >= 1 && (s = tv_nth_video_standard (sm->info, index - 1 /* tear-off */)))
    TV_CALLBACK_BLOCK (sm->callback, (success = z_switch_standard (s->hash, main_info)));

  rebuild_channel_menu = TRUE;

  if (success)
    {
#ifdef HAVE_LIBZVBI
      zvbi_channel_switched ();
#endif
    }
  else
    {
      select_cur_video_standard_item (menu_shell, sm->info);
    }
}

static GtkWidget *
video_standard_menu		(source_menu *		sm)
{
  const tv_video_standard *s;
  GtkMenuShell *menu_shell;
  GtkWidget *menu_item;
  GSList *group;

  if (!(s = tv_next_video_standard (sm->info, NULL)))
    return NULL;

  menu_shell = GTK_MENU_SHELL (gtk_menu_new ());

  menu_item = gtk_tearoff_menu_item_new ();
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (menu_shell, menu_item);

  group = NULL;

  for (; s; s = tv_next_video_standard (sm->info, s))
    append_radio_menu_item (&menu_shell, &group, s->label,
			    /* active */ s == sm->info->cur_video_standard,
			    G_CALLBACK (on_video_standard_activate), sm);

  select_cur_video_standard_item (menu_shell, sm->info);

  if (!sm->callback)
    sm->callback = tv_add_video_standard_callback
      (sm->info, on_tv_video_standard_change, NULL, sm);

  g_assert (sm->callback != NULL);

  return GTK_WIDGET (menu_shell);
}

/* Audio inputs */

  /* to do */

/* Video inputs */

static void
on_video_input_activate		(GtkMenuItem *		menu_item,
				 gpointer		user_data);
static GtkWidget *
video_input_menu		(source_menu *		sm);

static void
select_cur_video_input_item	(GtkMenuShell *		menu_shell,
				 tveng_device_info *	info)
{
  GtkWidget *menu_item;
  guint index;

  g_assert (info->cur_video_input != NULL);

  index = tv_video_input_position (info, info->cur_video_input);
  menu_item = z_menu_shell_nth_item (menu_shell, index + 1 /* tear-off */);
  g_assert (menu_item != NULL);

  SIGNAL_HANDLER_BLOCK (menu_item, on_video_input_activate,
			gtk_menu_shell_select_item (menu_shell, menu_item));
}

static void
on_tv_video_input_change	(tveng_device_info *	info,
				 void *			user_data)
{
  source_menu *sm = user_data;

  if (!sm->info->cur_video_input)
    {
      gtk_widget_set_sensitive (GTK_WIDGET (sm->menu_item), FALSE);
      gtk_menu_item_remove_submenu (sm->menu_item);
    }
  else
    {
      GtkWidget *w;

      if (!(w = gtk_menu_item_get_submenu (sm->menu_item)))
	{
	  gtk_widget_set_sensitive (GTK_WIDGET (sm->menu_item), TRUE);
	  gtk_menu_item_set_submenu (sm->menu_item, video_input_menu (sm));
	}
      else
	{
	  GtkMenuShell *menu_shell;

	  menu_shell = GTK_MENU_SHELL (w);
	  select_cur_video_input_item (menu_shell, sm->info);

	  /* Usually standards depend on video input, but we don't rebuild the
	     menu here. That happens automatically above in the video standard
	     callbacks. */
	}
    }
}

static void
on_video_input_activate		(GtkMenuItem *		menu_item,
				 gpointer		user_data)
{
  const source_menu *sm = user_data;
  GtkMenuShell *menu_shell;
  const tv_video_line *l;
  gboolean success;
  gint index;

  menu_shell = GTK_MENU_SHELL (gtk_menu_item_get_submenu (sm->menu_item));
  index = g_list_index (menu_shell->children, menu_item);

  success = FALSE;

  rebuild_channel_menu = FALSE; /* old stuff */

  if (index >= 1 && (l = tv_nth_video_input (sm->info, index - 1 /* tear-off */)))
    TV_CALLBACK_BLOCK (sm->callback, (success = z_switch_input (l->hash, main_info)));

  rebuild_channel_menu = TRUE;

  if (success)
    {
#ifdef HAVE_LIBZVBI
      zvbi_channel_switched ();
#endif
    }
  else
    {
      select_cur_video_input_item (menu_shell, sm->info);
    }
}

static GtkWidget *
video_input_menu		(source_menu *		sm)
{
  const tv_video_line *l;
  GtkMenuShell *menu_shell;
  GtkWidget *menu_item;
  GSList *group;

  if (!(l = tv_next_video_input (sm->info, NULL)))
    return NULL;

  menu_shell = GTK_MENU_SHELL (gtk_menu_new ());

  menu_item = gtk_tearoff_menu_item_new ();
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (menu_shell, menu_item);

  group = NULL;

  for (; l; l = tv_next_video_input (sm->info, l))
    append_radio_menu_item (&menu_shell, &group, l->label,
			    /* active */ l == sm->info->cur_video_input,
			    G_CALLBACK (on_video_input_activate), sm);

  select_cur_video_input_item (menu_shell, sm->info);

  if (!sm->callback)
    sm->callback = tv_add_video_input_callback
      (sm->info, on_tv_video_input_change, NULL, sm);

  g_assert (sm->callback != NULL);

  return GTK_WIDGET (menu_shell);
}

static void
add_source_items		(GtkMenuShell *		menu,
				 gint			pos,
				 tveng_device_info *	info)
{
  source_menu *sm;
  GtkWidget *item;

  {
    sm = g_malloc0 (sizeof (*sm));
    sm->info = info;

    item = z_gtk_pixmap_menu_item_new (_("Video standards"),
				     GTK_STOCK_SELECT_COLOR);
    gtk_widget_show (item);

    sm->menu_item = GTK_MENU_ITEM (item);
    g_object_set_data_full (G_OBJECT (item), "sm", sm,
			    (GtkDestroyNotify) on_menu_item_destroy);

    gtk_menu_shell_insert (menu, item, pos);

    if ((item = video_standard_menu (sm)))
      gtk_menu_item_set_submenu (sm->menu_item, item);
    else
      gtk_widget_set_sensitive (GTK_WIDGET (sm->menu_item), FALSE);
  }

  {
    item = z_gtk_pixmap_menu_item_new (_("Audio inputs"), "gnome-stock-line-in");
    gtk_widget_show (item);

    gtk_menu_shell_insert (menu, item, pos);

    /* Not used yet. */
    gtk_widget_set_sensitive (item, FALSE);
  }

  {
    sm = g_malloc0 (sizeof (*sm));
    sm->info = info;

    item = z_gtk_pixmap_menu_item_new (_("Video inputs"), "gnome-stock-line-in");
    gtk_widget_show (item);

    sm->menu_item = GTK_MENU_ITEM (item);
    g_object_set_data_full (G_OBJECT (item), "sm", sm,
			    (GtkDestroyNotify) on_menu_item_destroy);

    gtk_menu_shell_insert (menu, item, pos);

    if ((item = video_input_menu (sm)))
      gtk_menu_item_set_submenu (sm->menu_item, item);
    else
      gtk_widget_set_sensitive (GTK_WIDGET (sm->menu_item), FALSE);
  }
}

/* ------------------------------------------------------------------------- */

static inline tveng_tuned_channel *
nth_channel			(guint			index)
{
  tveng_tuned_channel *tc;
  guint i;

  for (tc = global_channel_list, i = 0; tc; tc = tc->next)
    if (tc->name && tc->name[0])
      if (i++ == index)
	break;

  return tc;
}

static inline void
insert_one_channel		(GtkMenuShell *		menu,
				 guint			index,
				 guint			pos)
{
  tveng_tuned_channel *tc;
  GtkWidget *menu_item;
  gchar *tooltip;

  if (!(tc = nth_channel (index)))
    return;

  menu_item = z_gtk_pixmap_menu_item_new (tc->name, GTK_STOCK_PROPERTIES);

  g_signal_connect_swapped (G_OBJECT (menu_item), "activate",
			    G_CALLBACK (select_channel),
			    GINT_TO_POINTER (index));

  if ((tooltip = z_key_name (tc->accel)))
    {
      z_tooltip_set (menu_item, tooltip);
      g_free (tooltip);
    }

  gtk_widget_show (menu_item);
  gtk_menu_shell_insert (menu, menu_item, pos);
}

static inline const gchar *
tuned_channel_nth_name		(guint			index)
{
  tveng_tuned_channel *tc;

  tc = nth_channel (index);

  /* huh? */
  if (!tc || !tc->name)
    return _("Unnamed");
  else
    return tc->name;
}

/* Returns whether something (useful) was added */
gboolean
add_channel_entries			(GtkMenuShell *menu,
					 gint pos,
					 gint menu_max_entries,
					 tveng_device_info *info)
{
  const guint ITEMS_PER_SUBMENU = 20;
  gboolean sth = FALSE;
  guint num_channels;

  add_source_items (menu, pos, info);

  num_channels = tveng_tuned_channel_num (global_channel_list);

  if (num_channels == 0)
    {
      /* This doesn't count as something added */

      GtkWidget *menu_item;

      /* TRANSLATORS: This is displayed in the channel menu when
         the channel list is empty. */
      menu_item = z_gtk_pixmap_menu_item_new (_("No channels"),
					      GTK_STOCK_CLOSE);
      gtk_widget_set_sensitive (menu_item, FALSE);
      gtk_widget_show (menu_item);
      gtk_menu_shell_insert (menu, menu_item, pos);
    }
  else
    {
      {
	GtkWidget *menu_item;

	/* Separator */

	menu_item = gtk_menu_item_new ();
	gtk_widget_show (menu_item);
	gtk_menu_shell_insert (menu, menu_item, pos);
      }

      sth = TRUE;

      if (num_channels <= ITEMS_PER_SUBMENU
	  && num_channels <= menu_max_entries)
	{
	  while (num_channels > 0)
	    insert_one_channel (menu, --num_channels, pos);
	}
      else
	{
	  while (num_channels > 0)
	    {
	      guint remainder = num_channels % ITEMS_PER_SUBMENU;

	      if (remainder == 1)
		{
		  insert_one_channel (menu, --num_channels, pos);
		}
	      else
		{
		  const gchar *first_name;
		  const gchar *last_name;
		  gchar *buf;
		  GtkMenuShell *submenu;
		  GtkWidget *menu_item;

		  if (remainder == 0)
		    remainder = ITEMS_PER_SUBMENU;

		  first_name = tuned_channel_nth_name (num_channels - remainder);
		  last_name = tuned_channel_nth_name (num_channels - 1);
		  buf = g_strdup_printf ("%s/%s", first_name, last_name);
		  menu_item = z_gtk_pixmap_menu_item_new (buf, "gnome-stock-line-in");
		  g_free (buf);
		  gtk_widget_show (menu_item);
		  gtk_menu_shell_insert (menu, menu_item, pos);

		  submenu = GTK_MENU_SHELL (gtk_menu_new());
		  gtk_widget_show (GTK_WIDGET (submenu));
		  gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item),
					     GTK_WIDGET (submenu));

		  menu_item = gtk_tearoff_menu_item_new ();
		  gtk_widget_show (menu_item);
		  gtk_menu_shell_append (submenu, menu_item);

		  while (remainder-- > 0)
		    insert_one_channel (submenu, --num_channels, 1);
		}
	    }
	}
    }

  return sth;
}

/* ------------------------------------------------------------------------- */

gdouble
videostd_inquiry(void)
{
  GtkWidget *dialog, *option, *check;
  gint std_hint;

  std_hint = zconf_get_integer (NULL, "/zapping/options/main/std_hint");

  if (std_hint >= 1)
    goto ok;

  dialog = build_widget ("videostd_inquiry", NULL);
  option = lookup_widget (dialog, "optionmenu24");
  check = lookup_widget (dialog, "checkbutton15");

  if (GTK_RESPONSE_ACCEPT != gtk_dialog_run (GTK_DIALOG (dialog)))
    {
      gtk_widget_destroy(dialog);
      return -1;
    }

  std_hint = 1 + z_option_menu_get_active (option);

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)))
    zconf_set_integer (std_hint, "/zapping/options/main/std_hint");

 ok:
  if (std_hint == 1)
    return 30000.0 / 1001;
  else if (std_hint >= 2)
    return 25.0;
  else
    return -1;
}

/*
 *  Preliminary. This should create an object such that we can write:
 *  zapping.control('volume').value += 1;
 *  But for now I just copied py_volume_incr().
 */
static PyObject*
py_control_incr			(PyObject *self, PyObject *args)
{
  static const struct {
    tv_control_id	id;
    const gchar *	name;
  } controls[] = {
    { TV_CONTROL_ID_BRIGHTNESS,	"brightness" },
    { TV_CONTROL_ID_CONTRAST,	"contrast" },
    { TV_CONTROL_ID_SATURATION,	"saturation" },
    { TV_CONTROL_ID_HUE,	"hue" },
    { TV_CONTROL_ID_MUTE,	"mute" },
    { TV_CONTROL_ID_VOLUME,	"volume" },
    { TV_CONTROL_ID_BASS,	"bass" },
    { TV_CONTROL_ID_TREBLE,	"treble" },
  };
  char *control_name;
  int increment, ok;
  const tv_control *tc;
  guint i;

  increment = +1;

  ok = PyArg_ParseTuple (args, "s|i", &control_name, &increment);

  if (!ok)
    g_error ("zapping.control_incr(s|i)");

  for (i = 0; i < N_ELEMENTS (controls); i++)
    if (0 == strcmp (controls[i].name, control_name))
      break;

  if (i >= N_ELEMENTS (controls))
    goto done;

  tc = NULL;

  while ((tc = tv_next_control (main_info, tc)))
    if (tc->id == controls[i].id)
      break;

  if (!tc)
    goto done;

  switch (tc->type)
    {
    case TV_CONTROL_TYPE_INTEGER:
    case TV_CONTROL_TYPE_BOOLEAN:
    case TV_CONTROL_TYPE_CHOICE:
      break;

    default:
      goto done;
    }

  if (tc->id == TV_CONTROL_ID_MUTE)
    {
      set_mute ((increment > 0) ? TRUE : FALSE, TRUE, TRUE);
    }
  else
    {
      if (-1 == tveng_update_control ((tv_control *) tc, main_info))
	goto done;

      tveng_set_control ((tv_control *) tc, tc->value + increment * tc->step, main_info);

#ifdef HAVE_LIBZVBI
      osd_render_markup (NULL, ("<span foreground=\"blue\">%s %d %%</span>"),
			 tc->label, (tc->value - tc->minimum)
			 * 100 / (tc->maximum - tc->minimum));
#endif
    }

 done:
  Py_INCREF(Py_None);

  return Py_None;
}

void
startup_v4linterface(tveng_device_info *info)
{
  z_input_model = ZMODEL(zmodel_new());

  g_signal_connect(G_OBJECT(z_input_model), "changed",
		     G_CALLBACK(update_bundle),
		     info);

  cmd_register ("channel_up", py_channel_up, METH_VARARGS,
		("Switch to higher channel"), "zapping.channel_up()");
  cmd_register ("channel_down", py_channel_down, METH_VARARGS,
		("Switch to lower channel"), "zapping.channel_down()");
  cmd_register ("set_channel", py_set_channel, METH_VARARGS);
  cmd_register ("lookup_channel", py_lookup_channel, METH_VARARGS);
  cmd_register ("control_box", py_control_box, METH_VARARGS,
		("Control window"), "zapping.control_box()");
  cmd_register ("control_incr", py_control_incr, METH_VARARGS,
		("Increase brightness"), "zapping.control_incr('brightness',+1)",
		("Decrease brightness"), "zapping.control_incr('brightness',-1)",
		("Increase hue"), "zapping.control_incr('hue',+1)",
		("Decrease hue"), "zapping.control_incr('hue',-1)",
		("Increase contrast"), "zapping.control_incr('contrast',+1)",
		("Decrease contrast"), "zapping.control_incr('contrast',-1)",
		("Increase saturation"), "zapping.control_incr('saturation',+1)",
		("Decrease saturation"), "zapping.control_incr('saturation',-1)",
		("Increase volume"), "zapping.control_incr('volume',+1)",
		("Decrease volume"), "zapping.control_incr('volume',-1)");

  zcc_char("Zapping: $(alias)", "Title format Z will use", "title_format");
  zcc_bool(FALSE, "Swap the page Up/Down bindings", "swap_up_down");
/*
  zcc_zkey (zkey_from_name ("Page_Up"), "Channel up key", "acc_channel_up");
  zcc_zkey (zkey_from_name ("Page_Down"), "Channel down key", "acc_channel_down");
*/
}

void
shutdown_v4linterface(void)
{
  g_object_unref(G_OBJECT(z_input_model));
}
