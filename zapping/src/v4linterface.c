/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 Iñaki García Etxebarria
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

/* Routines for building GUI elements dependant on the v4l device
   (such as the number of inputs and so on) */
#include "v4linterface.h"
#include "callbacks.h"
#include "zconf.h"
#include "zmisc.h"
#include "interface.h"

extern int cur_tuned_channel; /* currently tuned channel (in callbacks.c) */
GtkWidget * ToolBox = NULL; /* Pointer to the last control box */

/* 
   Update the menu from where we can choose the standard. Widget is
   any widget in the same window as the standard menu (we lookup() it)
*/
void update_standards_menu(GtkWidget * widget, tveng_device_info *
			   info)
{
  GtkWidget * Standards = lookup_widget(widget, "Standards");
  GtkWidget * NewMenu; /* New menu */
  GtkWidget * menu_item;
  int i;
  
  /* remove old (dummy) menu */
  gtk_widget_destroy(gtk_option_menu_get_menu (GTK_OPTION_MENU (Standards)));

  NewMenu = gtk_menu_new ();

  if (info -> num_standards == 0)
    gtk_widget_set_sensitive(Standards, FALSE);
  else
    gtk_widget_set_sensitive(Standards, TRUE);

  for (i = 0; i < info->num_standards; i++)
  {
    menu_item =
      gtk_menu_item_new_with_label(info->standards[i].name);
    gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
		       GTK_SIGNAL_FUNC(on_standard_activate),
		       GINT_TO_POINTER(i)); /* it should know about
					       itself */
    gtk_widget_show (menu_item);
    gtk_menu_append(GTK_MENU (NewMenu), menu_item);
  }

  if (info -> num_standards == 0)
    {
      menu_item =
	gtk_menu_item_new_with_label(_("No available standards"));
      gtk_widget_set_sensitive(menu_item, FALSE);
      gtk_widget_show (menu_item);
      gtk_menu_append (GTK_MENU (NewMenu), menu_item);
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (Standards), NewMenu);

  gtk_option_menu_set_history (GTK_OPTION_MENU (Standards), 
			       info -> cur_standard);

  /* Call this so the remaining menus get updated */
  update_inputs_menu(widget, info);
}

/* 
   Update the menu from where we can choose the input. Widget is
   any widget in the same window as the standard menu (we lookup() it)
 */
void update_inputs_menu(GtkWidget * widget, tveng_device_info *
			info)
{
  GtkWidget * Inputs = lookup_widget(widget, "Inputs");
  GtkWidget * NewMenu; /* New menu */
  GtkWidget * menu_item;
  int i;

  /* remove old menu */
  gtk_widget_destroy(gtk_option_menu_get_menu (GTK_OPTION_MENU (Inputs)));

  NewMenu = gtk_menu_new ();

  for (i = 0; i < info->num_inputs; i++)
  {
    menu_item =
      gtk_menu_item_new_with_label(info->inputs[i].name);

    gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
		       GTK_SIGNAL_FUNC(on_input_activate),
		       GINT_TO_POINTER( i )); /* it should know about
						 itself*/
    gtk_widget_show (menu_item);
    gtk_menu_append(GTK_MENU (NewMenu), menu_item);
  }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (Inputs), NewMenu);

  gtk_option_menu_set_history (GTK_OPTION_MENU (Inputs), info->cur_input);

  update_channels_menu(widget, info);
}

/*
  Update the menu from where we can choose the TV channel. Widget is
  any widget in the same window as the standard menu (we lookup() it)
*/
void update_channels_menu(GtkWidget* widget, tveng_device_info * info)
{
  GtkWidget * Channels = lookup_widget(widget, "Channels");
  GtkWidget * NewMenu; /* New menu */
  int i = 0;
  tveng_tuned_channel * tuned;
  GtkWidget * menu_item;
  gboolean tunes;

  /* remove old menu */
  gtk_widget_destroy(gtk_option_menu_get_menu (GTK_OPTION_MENU (Channels)));

  NewMenu = gtk_menu_new ();
  
  /* Check whether the current input has a tuner attached */
  if (info->num_inputs == 0)
    tunes = FALSE;
  else
    tunes = info->inputs[info->cur_input].flags & TVENG_INPUT_TUNER;

  /* If no tuned channels show error not sensitive */
  if (tveng_tuned_channel_num() == 0)
    tunes = FALSE;

  gtk_widget_set_sensitive(Channels, tunes);

  /* Different menus depending on the input */
  if (tunes)
    for (i = 0; (tuned = tveng_retrieve_tuned_channel_by_index(i)); i++)
      {
	menu_item =
	  gtk_menu_item_new_with_label(tuned -> name);
	gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
			   GTK_SIGNAL_FUNC(on_channel_activate),
			   GINT_TO_POINTER ( i )); /* it should know about
					    itself */
	gtk_widget_show (menu_item);
	gtk_menu_append (GTK_MENU (NewMenu), menu_item);
      }
  else
    {
      if ((info->num_inputs == 0) ||
	  (! (info->inputs[info->cur_input].flags & TVENG_INPUT_TUNER)))
	menu_item = gtk_menu_item_new_with_label(_("No tuner"));
      else
	menu_item = gtk_menu_item_new_with_label(_("No tuned channels"));
      gtk_widget_show (menu_item);
      gtk_menu_append (GTK_MENU (NewMenu), menu_item);
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (Channels), NewMenu);

  gtk_option_menu_set_history (GTK_OPTION_MENU (Channels), cur_tuned_channel);

  if (tunes)
    {
      if (cur_tuned_channel >= tveng_tuned_channel_num())
	cur_tuned_channel = tveng_tuned_channel_num() - 1;

      tuned =
	tveng_retrieve_tuned_channel_by_index(cur_tuned_channel);

      g_assert (tuned != NULL); /* This cannot happen, just for
				   checking */
      if (tveng_tune_input(tuned -> freq, info) == -1)
	ShowBox(_("Cannot tune the device"), GNOME_MESSAGE_BOX_ERROR);
    }
}

static void
on_control_slider_changed              (GtkAdjustment *adjust,
					gpointer user_data)
{
  /* Control id */
  gint cid = GPOINTER_TO_INT (user_data);
  tveng_device_info * info =
    (tveng_device_info*)gtk_object_get_data(GTK_OBJECT(adjust), "info");

  g_assert(info != NULL);
  g_assert(cid < info -> num_controls);

  tveng_set_control(&(info->controls[cid]), (int)adjust->value,
		    info);
}

static
GtkWidget * create_slider(struct tveng_control * qc,
			  int index,
			  tveng_device_info * info)
{ 
  GtkWidget * vbox; /* We have a slider and a label */
  GtkWidget * label;
  GtkWidget * hscale;
  GtkObject * adj; /* Adjustment object for the slider */
  int cur_value;
  
  vbox = gtk_vbox_new (FALSE, 0);
  label = gtk_label_new(_(qc->name));
  gtk_widget_show(label);
  gtk_box_pack_start_defaults(GTK_BOX (vbox), label);

  cur_value = qc -> cur_value;

  adj = gtk_adjustment_new(cur_value, qc->min, qc->max, 1, 10,
			   10);

  gtk_object_set_data(GTK_OBJECT(adj), "info", (gpointer) info);

  gtk_signal_connect(adj, "value-changed", 
		     GTK_SIGNAL_FUNC(on_control_slider_changed),
		     GINT_TO_POINTER (index));

  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adj));

  gtk_widget_show (hscale);
  gtk_box_pack_end_defaults(GTK_BOX (vbox), hscale);
  gtk_scale_set_value_pos (GTK_SCALE(hscale), GTK_POS_RIGHT);
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  gtk_adjustment_set_value( GTK_ADJUSTMENT (adj), cur_value);
  
  return (vbox);
}

static void
on_control_checkbutton_toggled         (GtkToggleButton *tb,
					gpointer user_data)
{
  gint cid = GPOINTER_TO_INT (user_data);
  tveng_device_info * info =
    (tveng_device_info*)gtk_object_get_data(GTK_OBJECT(tb), "info");

  g_assert(info != NULL);
  g_assert(cid < info -> num_controls);

  tveng_set_control(&(info-> controls[cid]),
		    gtk_toggle_button_get_active(tb),
		    info);
}

/* helper function for create_control_box */
static
GtkWidget * create_checkbutton(struct tveng_control * qc,
			       int index,
			       tveng_device_info * info)
{
  GtkWidget * cb;
  int cur_value;

  cur_value = qc->cur_value;

  cb = gtk_check_button_new_with_label(_(qc->name));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb), cur_value);

  gtk_object_set_data(GTK_OBJECT(cb), "info", (gpointer) info);
  gtk_signal_connect(GTK_OBJECT(cb), "toggled",
		     GTK_SIGNAL_FUNC(on_control_checkbutton_toggled),
		     GINT_TO_POINTER(index));
  return cb;
}

static void
on_control_menuitem_activate           (GtkMenuItem *menuitem,
					gpointer user_data)
{
  gint cid = GPOINTER_TO_INT (user_data);

  int value = (int) gtk_object_get_data(GTK_OBJECT(menuitem),
					"value");
  tveng_device_info * info =
    (tveng_device_info*)gtk_object_get_data(GTK_OBJECT(menuitem), "info");

  g_assert(info != NULL);
  g_assert(cid < info -> num_controls);

  tveng_set_control(&(info -> controls[cid]), value, info);
}

/* helper function for create_control_box */
static
GtkWidget * create_menu(struct tveng_control * qc,
			int index,
			tveng_device_info * info)
{
  GtkWidget * option_menu; /* The option menu */
  GtkWidget * menu; /* The menu displayed */
  GtkWidget * menu_item; /* Each of the menu items */
  GtkWidget * vbox; /* The container */
  GtkWidget * label; /* This shows what the menu is for */

  int i=0;

  option_menu = gtk_option_menu_new();
  menu = gtk_menu_new();

  vbox = gtk_vbox_new (FALSE, 0);
  label = gtk_label_new(_(qc->name));
  gtk_widget_show(label);
  gtk_box_pack_start_defaults(GTK_BOX (vbox), label);

  /* Start querying menu_items and building the menu */
  while (qc->data[i] != NULL)
    {
      menu_item = gtk_menu_item_new_with_label(_(qc->data[i]));

      gtk_object_set_data(GTK_OBJECT(menu_item), "info", (gpointer) info);
      gtk_object_set_data(GTK_OBJECT(menu_item), "value", 
			  GINT_TO_POINTER (i));
      gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
			 GTK_SIGNAL_FUNC(on_control_menuitem_activate),
			 GINT_TO_POINTER(index)); /* it should know about
						     itself*/
      gtk_widget_show(menu_item);
      gtk_menu_append(GTK_MENU(menu), menu_item);
      i++;
    }

  gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu),
			      qc->cur_value);

  gtk_widget_show(menu);
  gtk_widget_show(option_menu);

  gtk_box_pack_end_defaults(GTK_BOX (vbox), option_menu);

  return vbox;
}

static void
on_control_button_clicked              (GtkButton *button,
					gpointer user_data)
{
  gint cid = GPOINTER_TO_INT (user_data);
  tveng_device_info * info =
    (tveng_device_info*)gtk_object_get_data(GTK_OBJECT(button), "info");

  g_assert(info != NULL);
  g_assert(cid < info -> num_controls);

  tveng_set_control(&(info->controls[cid]), 1, info);
}

/* helper function for create_control_box */
static
GtkWidget * create_button(struct tveng_control * qc,
			  int index,
			  tveng_device_info * info)
{
  GtkWidget * button;

  button = gtk_button_new_with_label(_(qc->name));

  gtk_object_set_data(GTK_OBJECT(button), "info", (gpointer) info);
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
		     GTK_SIGNAL_FUNC(on_control_button_clicked),
		     GINT_TO_POINTER(index));

  return button;
}

static void
on_color_set		       (GnomeColorPicker *colorpicker,
				guint arg1,
				guint arg2,
				guint arg3,
				guint arg4,
				gpointer user_data)
{
  gint cid = GPOINTER_TO_INT (user_data);
  gint color = ((arg1>>8)<<16)+((arg2>>8)<<8)+(arg3>>8);
  tveng_device_info * info =
    (tveng_device_info*)gtk_object_get_data(GTK_OBJECT(colorpicker), "info");

  g_assert(info != NULL);
  g_assert(cid < info -> num_controls);

  tveng_set_control(&(info->controls[cid]), color, info);
}

static
GtkWidget * create_color_picker(struct tveng_control * qc,
				int index,
				tveng_device_info * info)
{
  GnomeColorPicker * color_picker =
    GNOME_COLOR_PICKER(gnome_color_picker_new());
  gchar * buffer = g_strdup_printf(_("Adjust %s"), qc->name);
  GtkWidget * label = gtk_label_new(_(qc->name));
  GtkWidget * hbox = gtk_hbox_new(TRUE, 10);

  gnome_color_picker_set_use_alpha(color_picker, FALSE);
  gnome_color_picker_set_i8(color_picker,
			    (qc->cur_value&0xff0000)>>16,
			    (qc->cur_value&0xff00)>>8,
			    (qc->cur_value&0xff), 0);
  gnome_color_picker_set_title(color_picker, buffer);
  gtk_object_set_data(GTK_OBJECT(color_picker), "info", (gpointer) info);
  g_free(buffer);

  gtk_widget_show(label);
  gtk_widget_show(GTK_WIDGET(color_picker));
  gtk_box_pack_start_defaults(GTK_BOX (hbox), label);
  gtk_box_pack_end_defaults(GTK_BOX (hbox), GTK_WIDGET(color_picker));

  gtk_signal_connect(GTK_OBJECT(color_picker), "color-set",
		     GTK_SIGNAL_FUNC(on_color_set),
		     GINT_TO_POINTER(index));

  return hbox;
}

static void
build_control_box(GtkWidget * vbox, tveng_device_info * info)
{
  GtkWidget * control_added;
  int i = 0;
  struct tveng_control * control;

  /* Update the values of all the controls */
  if (-1 == tveng_update_controls( info ))
    {
      ShowBox(_("Tveng critical error, zapping will exit NOW."),
	      GNOME_MESSAGE_BOX_ERROR);
      g_error("tveng critical: %s", info->error);
    }

  for (i = 0; i < info->num_controls; i++)
    {
      control = &(info->controls[i]);

      g_assert(control != NULL);

      switch (control->type)
	{
	case TVENG_CONTROL_SLIDER:
	  control_added = create_slider(control, i, info);
	  break;
	case TVENG_CONTROL_CHECKBOX:
	  control_added = create_checkbutton(control, i, info);
	  break;
	case TVENG_CONTROL_MENU:
	  control_added = create_menu(control, i, info);
	  break;
	case TVENG_CONTROL_BUTTON:
	  control_added = create_button(control, i, info);
	  break;
	case TVENG_CONTROL_COLOR:
	  control_added = create_color_picker(control, i, info);
	  break;
	default:
	  control_added = NULL; /* for sanity purpouses */
	  g_warning(_("Type %d of control %s is not supported"),
		    control->type, control->name);
	  continue;
	}

      if (control_added)
	{
	  gtk_widget_show(control_added);
	  gtk_box_pack_start_defaults(GTK_BOX(vbox), control_added);
	}
      else
	g_warning(_("Error adding %s"), control->name);
    }
}

GtkWidget * create_control_box(tveng_device_info * info)
{
  GtkWidget * control_box;
  GtkWidget * vbox;

  control_box = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_window_set_title(GTK_WINDOW(control_box), _("Available controls"));

  vbox = gtk_vbox_new(FALSE, 10); /* Leave 10 pixels of space between
				     controls */

  build_control_box(vbox, info);

  gtk_widget_show(vbox);
  gtk_container_add(GTK_CONTAINER (control_box), vbox);

  gtk_window_set_policy(GTK_WINDOW(control_box), FALSE, FALSE, FALSE);
  gtk_object_set_data(GTK_OBJECT(control_box), "vbox", vbox);
  gtk_signal_connect(GTK_OBJECT(control_box), "delete_event",
		     GTK_SIGNAL_FUNC(on_control_box_delete_event),
		     NULL);

  return (control_box);
}

void
update_control_box(tveng_device_info * info)
{
  GtkWidget * vbox;
  GtkWidget * control_box = ToolBox;

  if ((!info) || (!control_box))
    return;

  vbox = GTK_WIDGET(gtk_object_get_data(GTK_OBJECT(control_box),
					"vbox"));
  gtk_container_remove(GTK_CONTAINER (control_box), vbox);

  vbox = gtk_vbox_new(FALSE, 10); /* Leave 10 pixels of space between
				     controls */

  build_control_box(vbox, info);

  gtk_widget_show(vbox);
  gtk_container_add(GTK_CONTAINER (control_box), vbox);
  gtk_object_set_data(GTK_OBJECT(control_box), "vbox", vbox);
}
