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

/* Routines for building GUI elements dependant on the v4l2 device
   (such as the number of inputs and so on) */
#include "v4l2interface.h"
#include "callbacks.h"

extern int cur_tuned_channel; /* currently tuned channel (in callbacks.c) */

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

  for (i = 0; i < info->num_standards; i++)
  {
    menu_item =
      gtk_menu_item_new_with_label(info->standards[i].std.std.name);
    info->standards[i].id = i;
    gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
		       GTK_SIGNAL_FUNC(on_standard_activate),
		       &(info->standards[i])); /* it should know about
						  itself*/
    gtk_widget_show (menu_item);
    gtk_menu_append(GTK_MENU (NewMenu), menu_item);
  }

  tveng_update_standard(info);

  gtk_option_menu_set_menu (GTK_OPTION_MENU (Standards), NewMenu);

  gtk_option_menu_set_history (GTK_OPTION_MENU (Standards), 
			       info -> cur_standard_index);

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

  /* Get current standard */
  tveng_update_standard(info);

  /* remove old menu */
  gtk_widget_destroy(gtk_option_menu_get_menu (GTK_OPTION_MENU (Inputs)));

  NewMenu = gtk_menu_new ();

  for (i = 0; i < info->num_inputs; i++)
  {
    /* Show only supported inputs */
    if (info -> cur_standard_index > -1) /* Check only if valid */
      if (!(info->standards[info->cur_standard_index].std.inputs & (1 << i)))
	continue;

    menu_item =
      gtk_menu_item_new_with_label(info->inputs[i].input.name);
    info -> inputs[i].id = i;
    gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
		       GTK_SIGNAL_FUNC(on_input_activate),
		       &(info->inputs[i])); /* it should know about
						 itself*/
    gtk_widget_show (menu_item);
    gtk_menu_append(GTK_MENU (NewMenu), menu_item);
  }

  tveng_update_input(info);

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
  GtkWidget * menu_item;
  tveng_tuned_channel * tuned;
  gboolean tunes;

  /* remove old menu */
  gtk_widget_destroy(gtk_option_menu_get_menu (GTK_OPTION_MENU (Channels)));

  NewMenu = gtk_menu_new ();
  
  /* Check whether the current input has a tuner attached */
  tunes = info->inputs[info->cur_input].input.type &
    V4L2_INPUT_TYPE_TUNER;

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
			   (gpointer)i); /* it should know about
					    itself */
	gtk_widget_show (menu_item);
	gtk_menu_append (GTK_MENU (NewMenu), menu_item);
      }
  else
    {
      if (tveng_tuned_channel_num() > 0)
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
      if (tveng_tune_input(info->cur_input, tuned -> freq, info) == -1)
	ShowBox(_("Cannot tune the device"), GNOME_MESSAGE_BOX_ERROR);
    }
}

/* Prototype this functions to avoid compiler warnings, but they
   shouldn't be externally accesible */
GtkWidget * create_slider(struct v4l2_queryctrl * qc,
			  tveng_device_info * info);
GtkWidget * create_checkbutton(struct v4l2_queryctrl * qc,
			    tveng_device_info * info);
GtkWidget * create_menu(struct v4l2_queryctrl * qc,
			tveng_device_info * info);
GtkWidget * create_button(struct v4l2_queryctrl * qc,
			  tveng_device_info * info);

/* helper function for create_control_box */
GtkWidget * create_slider(struct v4l2_queryctrl * qc, 
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

  /* get current control value */
  if (tveng_get_control(qc->id, &cur_value, info) == -1)
    return NULL;

#ifndef NDEBUG
  printf("Current control value is %d\n", cur_value);
#endif
  
  adj = gtk_adjustment_new(cur_value, qc->minimum, qc->maximum, 1, 10,
			   10);

  /* This will only work in architectures where sizeof(gpointer) >= 4 */
  gtk_object_set_data(adj, "info", (gpointer)info);

  gtk_signal_connect(adj, "value-changed", 
		     GTK_SIGNAL_FUNC(on_control_slider_changed),
		     (gpointer) qc->id);

  hscale = gtk_hscale_new (GTK_ADJUSTMENT (adj));

  gtk_widget_show (hscale);
  gtk_box_pack_end_defaults(GTK_BOX (vbox), hscale);
  gtk_scale_set_value_pos (GTK_SCALE(hscale), GTK_POS_RIGHT);
  gtk_scale_set_digits (GTK_SCALE (hscale), 0);
  gtk_adjustment_set_value( GTK_ADJUSTMENT (adj), cur_value);
  
  return (vbox);
}

/* helper function for create_control_box */
GtkWidget * create_checkbutton(struct v4l2_queryctrl * qc,
			       tveng_device_info * info)
{
  GtkWidget * cb;
  int cur_value;

  if (tveng_get_control(qc->id, &cur_value, info) == -1)
    return NULL;  

  cb = gtk_check_button_new_with_label(_(qc->name));
  gtk_object_set_data(GTK_OBJECT(cb), "info", info);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(cb), cur_value);

  gtk_signal_connect(GTK_OBJECT(cb), "toggled",
		     GTK_SIGNAL_FUNC(on_control_checkbutton_toggled),
		     (gpointer) qc->id);
  return cb;
}

/* helper function for create_control_box */
GtkWidget * create_menu(struct v4l2_queryctrl * qc,
			tveng_device_info * info)
{
  GtkWidget * option_menu; /* The option menu */
  GtkWidget * menu; /* The menu displayed */
  GtkWidget * menu_item; /* Each of the menu items */

  int i=0;
  struct v4l2_querymenu qm; /* Info about each of the items in the
			       menu */
  int cur_value;

  /* get the current value and select it */
  if (tveng_get_control(qc->id, &cur_value, info) == -1)
    return NULL;
  
  option_menu = gtk_option_menu_new();
  menu = gtk_menu_new();

  /* Start querying menu_items and building the menu */
  qm.id = i;
  while (ioctl(info->fd, VIDIOC_QUERYMENU, &qm) == 0)
    {
      menu_item = gtk_menu_item_new_with_label(_(qm.name));

      gtk_object_set_data(GTK_OBJECT(menu_item), "info", (gpointer) info);
      gtk_object_set_data(GTK_OBJECT(menu_item), "value", (gpointer)
			  i);
      gtk_widget_show(menu_item);
      gtk_menu_append(GTK_MENU(menu), menu_item);
    }

  gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);

  gtk_widget_show(menu);
  return option_menu;
}

/* helper function for create_control_box */
GtkWidget * create_button(struct v4l2_queryctrl * qc,
			  tveng_device_info * info)
{
  GtkWidget * button;

  button = gtk_button_new_with_label(_(qc->name));

  gtk_object_set_data(GTK_OBJECT(button), "info", (gpointer) info);

  gtk_signal_connect(GTK_OBJECT(button), "clicked",
		     GTK_SIGNAL_FUNC(on_control_button_clicked),
		     (gpointer) qc->id);

  return button;
}

/* 
   Creates a control box suited for setting up all the controls this
   device can have.
   FIXME: V4L2 BUGS: videodev.h, V4L2_CID_VCENTER gives error
   (V4"l"2_CID_BASE)
   QUERY'ing the controls doesn't work properly (ioctl ()
   always succeeds)
   Changing the input turns sound off
   In the docs it says item[32] is a member of struct
   v4l2_querymenu, it should say name[32]
*/
GtkWidget * create_control_box(tveng_device_info * info)
{
  GtkWidget * control_box;
  GtkWidget * vbox;

  __u32 available_controls[] =
  {
    V4L2_CID_BRIGHTNESS,
    V4L2_CID_CONTRAST,
    V4L2_CID_SATURATION,
    V4L2_CID_HUE,
    V4L2_CID_WHITENESS,
    V4L2_CID_BLACK_LEVEL,
    /*    V4L2_CID_AUTO_WHITE_BALANCE,*/
    V4L2_CID_DO_WHITE_BALANCE,
    V4L2_CID_RED_BALANCE,
    V4L2_CID_BLUE_BALANCE,
    V4L2_CID_GAMMA,
    V4L2_CID_EXPOSURE,
    V4L2_CID_AUTOGAIN,
    V4L2_CID_GAIN,
    V4L2_CID_HCENTER,
    V4L2_CID_VCENTER,
    V4L2_CID_HFLIP,
    V4L2_CID_VFLIP,
    
    V4L2_CID_AUDIO_VOLUME,
    V4L2_CID_AUDIO_MUTE,
    V4L2_CID_AUDIO_BALANCE,
    V4L2_CID_AUDIO_BASS,
    V4L2_CID_AUDIO_TREBLE,
    V4L2_CID_AUDIO_LOUDNESS
  };

  GtkWidget * control_added;
  struct v4l2_queryctrl qc;
  int i, num_controls = sizeof(available_controls)/sizeof(__u32);

  control_box = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_window_set_title(GTK_WINDOW(control_box), _("Available controls"));

  vbox = gtk_vbox_new(FALSE, 10); /* Leave 10 pixels of space between
				     controls */

  for (i=0; i < num_controls; i++)
    {
      if (tveng_test_control(available_controls[i], &qc, info) != 0)
	continue;
      g_assert(available_controls[i] == qc.id);
      if (qc.flags & V4L2_CTRL_FLAG_DISABLED)
	continue;

#ifndef NDEBUG
      printf("Adding control %s (%d)\n", qc.name, i);
#endif

      switch (qc.type)
	{
	case V4L2_CTRL_TYPE_INTEGER:
	  control_added = create_slider(&qc, info);
	  break;
	case V4L2_CTRL_TYPE_BOOLEAN:
	  control_added = create_checkbutton(&qc, info);
	  break;
	case V4L2_CTRL_TYPE_MENU:
	  control_added = create_menu(&qc, info);
	  break;
	case V4L2_CTRL_TYPE_BUTTON:
	  control_added = create_button(&qc, info);
	  break;
	default:
	  control_added = NULL; /* for sanity purpouses */
	  printf("Type %d of control %s is not supported\n", qc.type, 
		 _(qc.name));
	  continue;
	}
      if (control_added)
	{
	  gtk_widget_set_sensitive(control_added,
				   !(qc.flags & V4L2_CTRL_FLAG_GRABBED));
	  gtk_widget_show(control_added);
	  gtk_box_pack_start_defaults(GTK_BOX(vbox), control_added);
	}
      else
	printf(_("Error adding %s\n"), _(qc.name));
    }

  gtk_widget_show(vbox);
  gtk_container_add(GTK_CONTAINER (control_box), vbox);

  gtk_window_set_policy(GTK_WINDOW(control_box), FALSE, FALSE, FALSE);
  gtk_signal_connect(GTK_OBJECT(control_box), "delete_event",
		     GTK_SIGNAL_FUNC(on_control_box_delete_event),
		     NULL);

  return (control_box);
}
