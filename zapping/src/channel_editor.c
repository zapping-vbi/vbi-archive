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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "tveng.h"
#include "callbacks.h"
#include "interface.h"
#include "v4linterface.h"
#include "zconf.h"
#include "keysyms.h"
/* Manages config values for zconf (it saves me some typing) */
#define ZCONF_DOMAIN "/zapping/internal/callbacks/"
#include "zmisc.h"
#include "plugins.h"
#include "zvbi.h"

GtkWidget * ChannelWindow = NULL; /* Here is stored the channel editor
				   widget (if any) */

extern tveng_channels * current_country; /* Currently selected contry */
extern tveng_tuned_channel * global_channel_list;
extern tveng_device_info * main_info; /* About the device we are using */

extern int cur_tuned_channel; /* Currently tuned channel */

GtkStyle *istyle; /* Insensitive CList style */

static void
update_edit_buttons_sensitivity		(GtkWidget	*channel_editor)
{
  GtkWidget *up = lookup_widget(channel_editor, "move_channel_up");
  GtkWidget *down = lookup_widget(channel_editor,
				  "move_channel_down");
  GtkWidget *remove = lookup_widget(channel_editor, "remove_channel");
  GtkWidget *modify = lookup_widget(channel_editor, "modify_channel");
  GtkWidget *channel_list = lookup_widget(channel_editor, "channel_list");
  GList *ptr;
  gboolean sensitive = FALSE;
  gboolean sensitive_up = FALSE;
  gboolean sensitive_down = FALSE;

  ptr = GTK_CLIST(channel_list) -> row_list;

  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	{
	  sensitive = TRUE;

	  ptr = g_list_first(GTK_CLIST(channel_list) -> row_list);
	  sensitive_up =
	    (ptr && GTK_CLIST_ROW(ptr) -> state != GTK_STATE_SELECTED);

	  ptr = g_list_last(GTK_CLIST(channel_list) -> row_list);
	  sensitive_down =
	    (ptr && GTK_CLIST_ROW(ptr) -> state != GTK_STATE_SELECTED);

	  break;
	}

      ptr = ptr -> next;
    }

  gtk_widget_set_sensitive(up, sensitive_up);
  gtk_widget_set_sensitive(down, sensitive_down);
  gtk_widget_set_sensitive(remove, sensitive);
  gtk_widget_set_sensitive(modify, sensitive);
}

static void
build_channel_list(GtkCList *clist, tveng_tuned_channel * list)
{
  gint i=0;
  tveng_tuned_channel *tuned_channel;
  struct tveng_enumstd *std;
  struct tveng_enum_input *input;
  gchar index[256];
  gchar alias[256];
  gchar country[256];
  gchar channel[256];
  gchar freq[256];
  gchar standard[256];
  gchar accel[256];
  gchar * buffer;
  gfloat value;
  gchar *entry[] = { index, alias, country, channel, freq, standard, accel };

  for (i = 0; i < sizeof(entry) / sizeof(entry[0]); i++)
    memset(entry[i], 0, 256);

  value = gtk_clist_get_vadjustment(clist)->value;

  gtk_clist_freeze(clist);

  gtk_clist_clear(clist);

  /* Setup the channel list */
  for (i = 0; (tuned_channel =
	       tveng_retrieve_tuned_channel_by_index(i, list)); i++)
    {
      /* clist has no optional built-in row number? */
      g_snprintf(entry[0], 255, "%3u", i + 1);
      strncpy(entry[1], tuned_channel->name, 255);

      entry[2][0] = 0;
      entry[3][0] = 0;
      entry[4][0] = 0;

      input = tveng_find_input_by_hash(tuned_channel->input, main_info);

      if (!input || input->tuners > 0)
	{
	  strncpy(entry[2], _(tuned_channel->country), 255);
	  strncpy(entry[3], tuned_channel->real_name, 255);
	  g_snprintf(entry[4], 255, "%u", tuned_channel->freq);
	  buffer = gdk_keyval_name(tuned_channel->accel_key);
	  if (buffer)
	    g_snprintf(accel, sizeof(accel)-1, "%s%s%s%s",
		       (tuned_channel->accel_mask&GDK_CONTROL_MASK)?"Ctl+":"",
		       (tuned_channel->accel_mask&GDK_MOD1_MASK)?"Alt+":"",
		       (tuned_channel->accel_mask&GDK_SHIFT_MASK)?"Shift+":"",
		       buffer);
	  else
	    accel[0] = 0;
	}
      else
	{
	  strncpy(entry[3], input->name, 255);
	  /* too bad there's no span-columns parameter */
	}

      std = tveng_find_standard_by_hash(tuned_channel->standard, main_info);
      strncpy(entry[5], std ? std->name : "", 255);

      entry[6][0] = 0;

      if (tuned_channel->accel_key)
	{
	  gchar *buffer = gdk_keyval_name(tuned_channel->accel_key);

	  if (buffer)
	    g_snprintf(entry[6], 255, "%s%s%s%s",
	      (tuned_channel->accel_mask & GDK_CONTROL_MASK) ? _("Ctrl+") : "",
	      (tuned_channel->accel_mask & GDK_MOD1_MASK) ? _("Alt+") : "",
	      (tuned_channel->accel_mask & GDK_SHIFT_MASK) ? _("Shift+") : "",
	      buffer);
	}

      gtk_clist_append(clist, entry);

      if (tuned_channel->input == 0)
	{
	  gtk_clist_set_cell_style (clist, i, 2, istyle);
	  gtk_clist_set_cell_style (clist, i, 3, istyle);
	  gtk_clist_set_cell_style (clist, i, 4, istyle);
	}
    }

  gtk_clist_thaw(clist);

  gtk_adjustment_set_value(gtk_clist_get_vadjustment(clist), value);

  update_edit_buttons_sensitivity(GTK_WIDGET(clist));
}

static void
real_add_channel			(GtkWidget	*some_widget,
					 gpointer	user_data)
{
  GtkWidget * clist1 = lookup_widget(GTK_WIDGET(some_widget), "clist1");
  GtkWidget * channel_list = lookup_widget(GTK_WIDGET(some_widget),
					   "channel_list");
  GtkWidget * channel_editor = lookup_widget(GTK_WIDGET(some_widget),
					     "channel_editor");
  GtkWidget * channel_name = lookup_widget(GTK_WIDGET(some_widget),
					   "channel_name");
  GtkWidget * channel_accel = lookup_widget(GTK_WIDGET(some_widget),
					    "channel_accel");
  GtkWidget * widget;

  tveng_tuned_channel * list =
    gtk_object_get_data(GTK_OBJECT(channel_editor), "list");

  GList * ptr; /* Pointer to the selected item(s) in clist1 */
  int index = 0; /* The row we are reading now */
  tveng_tuned_channel tc;
  gchar * buffer;
  gint selected;
  
  memset(&tc, 0, sizeof(tveng_tuned_channel));
  tc.name = gtk_entry_get_text (GTK_ENTRY(channel_name));
  if (current_country)
    tc.country = current_country -> name;
  else
    tc.country = NULL;
  /*
    See same sequence in on_modify_channel_clicked.
  if (main_info->inputs &&
      main_info->inputs[main_info->cur_input].tuners)
  */
    {
      GtkWidget *spinslider =
	gtk_object_get_data (GTK_OBJECT (channel_editor),
			     "spinslider");

      tc.freq = z_spinslider_get_value (spinslider);
    }
  tc.accel_key = 0;
  buffer = gtk_entry_get_text(GTK_ENTRY(channel_accel));
  if (buffer)
      tc.accel_key = gdk_keyval_from_name(buffer);
  if (tc.accel_key == GDK_VoidSymbol)
    tc.accel_key = 0;

  tc.accel_mask = 0;
  widget = lookup_widget(clist1, "channel_accel_ctrl");
  tc.accel_mask |=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ?
    GDK_CONTROL_MASK : 0;
  widget = lookup_widget(clist1, "channel_accel_shift");
  tc.accel_mask |=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ?
    GDK_SHIFT_MASK : 0;
  widget = lookup_widget(clist1, "channel_accel_alt");
  tc.accel_mask |=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ?
    GDK_MOD1_MASK : 0;

  selected =
    z_option_menu_get_active(lookup_widget(clist1, "attached_input"));
  if (selected && main_info->inputs)
    tc.input = main_info->inputs[selected-1].hash;
  else
    tc.input = 0;

  selected =
    z_option_menu_get_active(lookup_widget(clist1, "attached_standard"));
  if (selected)
    tc.standard = main_info->standards[selected-1].hash;
  else
    tc.standard = 0;

  ptr = GTK_CLIST(clist1) -> row_list;

  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	{ 
	  /* Add this selected channel to the channel list */
	  gtk_clist_get_text(GTK_CLIST(clist1), index, 0,
			     &(tc.real_name));
	  break; /* found */
	}
      ptr = ptr -> next;
      index++;
    }

  store_control_values(&tc.num_controls, &tc.controls, main_info);

  list = tveng_insert_tuned_channel_sorted(&tc, list);

  gtk_object_set_data(GTK_OBJECT(channel_editor), "list", list);

  g_free(tc.controls);

  build_channel_list(GTK_CLIST(channel_list), list);

  gtk_entry_set_text(GTK_ENTRY(channel_name), "");
}

static void
on_fine_tune_value_changed               (GtkAdjustment		*adj,
					  tveng_device_info	*info)
{
  if (info->inputs &&
      info->inputs[info->cur_input].tuners)
    tveng_tune_input(adj->value, main_info);
}

static void
set_slider(uint32_t freq, GtkWidget *channel_editor,
	   tveng_device_info *info)
{
  GtkWidget *spinslider =
    gtk_object_get_data (GTK_OBJECT (channel_editor), "spinslider");
  GtkAdjustment *spin_adj = z_spinslider_get_spin_adj (spinslider);
  GtkAdjustment *hscale_adj = z_spinslider_get_hscale_adj (spinslider);

  if (freq)
    {
      freq += 12;
      freq -= freq % 25;

      spin_adj -> value = freq;
      spin_adj -> lower = 5000;
      spin_adj -> upper = 900000;
      spin_adj -> step_increment = 25;
      spin_adj -> page_increment = 1e3;
      spin_adj -> page_size = 0;

      hscale_adj -> value = freq;
      hscale_adj -> lower = freq - 1e4;
      hscale_adj -> upper = freq + 1e4;
      hscale_adj -> step_increment = 25;
      hscale_adj -> page_increment = 1e3;
      hscale_adj -> page_size = 0;

      gtk_adjustment_changed (spin_adj);
      gtk_adjustment_value_changed (spin_adj);
      gtk_adjustment_changed (hscale_adj);
      gtk_adjustment_value_changed (hscale_adj);
      z_spinslider_set_reset_value (spinslider, freq);
    }

  if (!freq || !info->inputs ||
      !info->inputs[info->cur_input].tuners)
    {
      gtk_widget_set_sensitive (spinslider, FALSE);
      return;
    }

  gtk_widget_set_sensitive (spinslider, TRUE);
}

static void
on_input_changed		       (GtkWidget       *menu_item,
					gpointer	*user_data)
{
  GtkWidget * channel_editor = lookup_widget (menu_item, "channel_editor");
  gint selected = (gint) user_data;
  tveng_device_info *info = main_info;

  if (selected == 0) /* "don't change" */
    return;

  if (!info->inputs || (selected - 1) >= info->num_inputs)
    return;

  if (tveng_set_input (info->inputs + selected - 1, info) == -1)
    return;

  /* XXX zmodel_changed (z_input_model);
     destroys this menu */

  if (info->inputs[info->cur_input].tuners > 0)
    {
      GtkWidget *clist1 =
	lookup_widget (GTK_WIDGET (channel_editor), "clist1");
      GtkWidget *spinslider =
	gtk_object_get_data (GTK_OBJECT (channel_editor), "spinslider");
      GList *ptr;
      uint32_t freq;

      if (clist1)
	for (ptr = GTK_CLIST(clist1)->row_list; ptr; ptr = ptr->next)
	  if (GTK_CLIST_ROW(ptr)->state == GTK_STATE_SELECTED)
	    {
	      freq = z_spinslider_get_value (spinslider);
	      break;
	    }

      if (clist1 && ptr && freq != 0)
	{
	  /* A channel is selected which has set the slider already,
	   * we keep that freq (or whatever the user entered in the
	   * meantime). Happens for baseband input and "don't change"
	   * channels as well, so one can switch back and forth without
	   * erasing the slider setting.
	   */
	  set_slider (freq, channel_editor, info);
	  return;
	}
      else if (tveng_get_tune(&freq, info) != -1)
	{
	  set_slider (freq, channel_editor, info);
	  return;
	}
      /* else: */
    }

  /* insensitive */
  set_slider (0, channel_editor, info);
}

/* Called when the current country selection has been changed */
static void
on_country_switch                      (GtkWidget       *menu_item,
					tveng_channels  *country)
{
  GtkWidget * clist1 = lookup_widget(menu_item, "clist1");

  tveng_channel * channel;
  int id=0;

  gchar new_entry_0[128];
  gchar new_entry_1[128];
  gchar *new_entry[] = {new_entry_0, new_entry_1}; /* Allocate room
						      for new entries */
  new_entry[0][127] = new_entry[1][127] = 0;

  /* Set the current country */
  current_country = country;

  gtk_clist_freeze( GTK_CLIST(clist1)); /* We are going to do a number
					   of changes */

  gtk_clist_clear( GTK_CLIST(clist1));
  
  /* Get all available channels for this country */
  while ((channel = tveng_get_channel_by_id(id, country)))
    {
      g_snprintf(new_entry[0], 127, "%s", channel->name);
      g_snprintf(new_entry[1], 127, "%u", channel->freq);
      gtk_clist_append(GTK_CLIST(clist1), new_entry);
      id++;
    }

  gtk_clist_thaw( GTK_CLIST(clist1));

  /* Set the current country as the user data of the clist */
  gtk_object_set_user_data ( GTK_OBJECT(clist1), country);
}

static void
rebuild_inputs_and_standards (gpointer ignored, GtkWidget *widget)
{
  GtkWidget * input = lookup_widget(widget, "attached_input");
  GtkWidget * standard = lookup_widget(widget, "attached_standard");
  GtkWidget * NewMenu; /* New menu */
  GtkWidget * menu_item;
  int i;
  
  /* remove old menu */
  gtk_widget_destroy(gtk_option_menu_get_menu (GTK_OPTION_MENU (input)));
  gtk_widget_destroy(gtk_option_menu_get_menu (GTK_OPTION_MENU (standard)));

  NewMenu = gtk_menu_new ();

  menu_item = gtk_menu_item_new_with_label(_("Do not change standard"));
  gtk_widget_show (menu_item);
  gtk_menu_append(GTK_MENU (NewMenu), menu_item);

  for (i = 0; i < main_info->num_standards; i++)
  {
    menu_item =
      gtk_menu_item_new_with_label(main_info->standards[i].name);
    gtk_widget_show (menu_item);
    gtk_menu_append(GTK_MENU (NewMenu), menu_item);
  }

  if (main_info->num_standards)
    z_option_menu_set_active(standard, main_info->cur_standard+1);
  else
    z_option_menu_set_active(standard, 0);

  gtk_option_menu_set_menu (GTK_OPTION_MENU (standard), NewMenu);

  NewMenu = gtk_menu_new ();

  menu_item = gtk_menu_item_new_with_label(_("Do not change input"));
  gtk_widget_show (menu_item);
  gtk_menu_append(GTK_MENU (NewMenu), menu_item);

  for (i = 0; i < main_info->num_inputs; i++)
  {
    menu_item = gtk_menu_item_new_with_label(main_info->inputs[i].name);
    gtk_signal_connect (GTK_OBJECT (menu_item), "activate",
			GTK_SIGNAL_FUNC (on_input_changed),
			(gpointer)(i + 1));
    gtk_widget_show (menu_item);
    gtk_menu_append(GTK_MENU (NewMenu), menu_item);
  }

  if (main_info->num_inputs)
    z_option_menu_set_active(input, main_info->cur_input+1);
  else
    z_option_menu_set_active(input, 0);

  gtk_option_menu_set_menu (GTK_OPTION_MENU (input), NewMenu);
}

static void
on_known_keys_clicked			(GtkWidget	*button,
					 GtkWidget	*channel_editor)
{
  GtkWidget *dialog = create_widget("choose_key");
  GtkWidget *key_clist = lookup_widget(dialog, "key_clist");
  GtkWidget *channel_accel= lookup_widget(channel_editor, "channel_accel");
  gint i;
  gchar *tmp[1];
  gchar *buffer;
  gint selected = -1;
  GList *ptr;

  buffer = gtk_entry_get_text(GTK_ENTRY(channel_accel));

  for (i=0; i<num_keysyms; i++)
    {
      tmp[0] = keysyms[i];
      gtk_clist_append(GTK_CLIST(key_clist), tmp);
      if (buffer && !strcasecmp(buffer, keysyms[i]))
	selected = i;
    }

  if (selected >= 0)
    {
      gtk_clist_moveto(GTK_CLIST(key_clist), selected, 0, 0.5, 0.5);
      gtk_clist_select_row(GTK_CLIST(key_clist), selected, 0);
    }

  if (!gnome_dialog_run_and_close(GNOME_DIALOG(dialog)))
    {
      ptr = GTK_CLIST(key_clist) -> row_list;
      i = 0;

      /* get first selected row */
      while (ptr)
	{
	  if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	    break;

	  ptr = ptr -> next;
	  i++;
	}

      if (ptr)
	gtk_entry_set_text(GTK_ENTRY(channel_accel), keysyms[i]);
    }

  gtk_widget_destroy(dialog);
}

static void
on_move_channel_down_clicked		(GtkWidget	*button,
					 GtkWidget	*channel_editor)
{
  GtkWidget * channel_list = lookup_widget(GTK_WIDGET(button),
					   "channel_list");
  tveng_tuned_channel * list =
    gtk_object_get_data(GTK_OBJECT(channel_editor), "list");
  tveng_tuned_channel * tc;
  GList *ptr;
  gint pos, last_pos;
  gboolean selected[tveng_tuned_channel_num(list)];
  gboolean moved = FALSE;

  memset(selected, FALSE, sizeof(selected));

  ptr = g_list_last(GTK_CLIST(channel_list) -> row_list);

  /* look for first unselected entry */
  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state != GTK_STATE_SELECTED)
	break;

      selected[g_list_position(GTK_CLIST(channel_list) -> row_list,
			       ptr)] = TRUE;

      ptr = ptr -> prev;
    }

  /* swap this and next */
  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	{
	  moved = TRUE;
	  pos = g_list_position(GTK_CLIST(channel_list) -> row_list, ptr);
	  g_assert(pos >= 0);
	  tc = tveng_retrieve_tuned_channel_by_index(pos, list);
	  tveng_tuned_channel_down(tc);
	  selected[pos+1] = TRUE;
	}

      ptr = ptr -> prev;
    }

  if (!moved)
    return;

  /* redraw list */
  build_channel_list(GTK_CLIST(channel_list), list);

  /* select channels again */
  gtk_signal_handler_block_by_func(GTK_OBJECT(channel_list),
			   GTK_SIGNAL_FUNC(on_channel_list_select_row),
			   NULL);

  for (pos = last_pos = 0; pos < tveng_tuned_channel_num(list); pos++)
    if (selected[pos])
      {
	gtk_clist_select_row(GTK_CLIST(channel_list), pos, 0);
	if (pos > last_pos)
	  last_pos = pos;
      }

  /* bring the row following the selection back in sight */
  if (last_pos + 1 < pos)
    last_pos++;
  if (gtk_clist_row_is_visible(GTK_CLIST(channel_list), last_pos)
      != GTK_VISIBILITY_FULL)
    gtk_clist_moveto(GTK_CLIST(channel_list), last_pos, 0, 1.0, 0.0);

  gtk_signal_handler_unblock_by_func(GTK_OBJECT(channel_list),
			     GTK_SIGNAL_FUNC(on_channel_list_select_row),
			     NULL);

  update_edit_buttons_sensitivity(GTK_WIDGET(channel_list));
}

static void
on_move_channel_up_clicked		(GtkWidget	*button,
					 GtkWidget	*channel_editor)
{
  GtkWidget * channel_list = lookup_widget(GTK_WIDGET(button),
					   "channel_list");
  tveng_tuned_channel * list =
    gtk_object_get_data(GTK_OBJECT(channel_editor), "list");
  tveng_tuned_channel * tc;
  GList *ptr;
  gint pos, first_pos;
  gboolean selected[tveng_tuned_channel_num(list)];
  gboolean moved = FALSE;

  memset(selected, FALSE, sizeof(selected));

  ptr = g_list_first(GTK_CLIST(channel_list) -> row_list);

  /* look for first unselected entry */
  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state != GTK_STATE_SELECTED)
	break;

      selected[g_list_position(GTK_CLIST(channel_list) -> row_list,
			       ptr)] = TRUE;
      ptr = ptr -> next;
    }

  /* swap this and next */
  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	{
	  moved = TRUE;
	  pos = g_list_position(GTK_CLIST(channel_list) -> row_list, ptr);
	  g_assert(pos > 0);
	  tc = tveng_retrieve_tuned_channel_by_index(pos, list);
	  tveng_tuned_channel_up(tc);
	  selected[pos-1] = TRUE;
	}

      ptr = ptr -> next;
    }

  if (!moved)
    return;

  /* redraw list */
  build_channel_list(GTK_CLIST(channel_list), list);

  /* select channels again */
  gtk_signal_handler_block_by_func(GTK_OBJECT(channel_list),
			   GTK_SIGNAL_FUNC(on_channel_list_select_row),
			   NULL);

  for (pos = 0, first_pos = tveng_tuned_channel_num(list);
       pos < tveng_tuned_channel_num(list); pos++)
    if (selected[pos])
      {
	gtk_clist_select_row(GTK_CLIST(channel_list), pos, 0);
	if (pos < first_pos)
	  first_pos = pos;
      }

  /* bring the row preceding the selection back in sight */
  if (first_pos > 0)
    first_pos--;
  if (gtk_clist_row_is_visible(GTK_CLIST(channel_list), first_pos)
      != GTK_VISIBILITY_FULL)
    gtk_clist_moveto(GTK_CLIST(channel_list), first_pos, 0, 0.0, 0.0);

  gtk_signal_handler_unblock_by_func(GTK_OBJECT(channel_list),
			     GTK_SIGNAL_FUNC(on_channel_list_select_row),
			     NULL);

  update_edit_buttons_sensitivity(GTK_WIDGET(channel_list));
}

static void
on_channel_list_unselect_row		(GtkCList	*channel_list,
					 gint		 row,
					 gint		 column,
					 GdkEvent       *event,
					 GtkWidget	*channel_editor)
{
  update_edit_buttons_sensitivity(channel_editor);
}

static gboolean
on_channel_editor_delete_event         (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  GtkWidget * related_menuitem =
    GTK_WIDGET(gtk_object_get_user_data(GTK_OBJECT(widget)));
  tveng_tuned_channel * list =
    gtk_object_get_data(GTK_OBJECT(widget), "list");

  tveng_clear_tuned_channel(list);

  gtk_signal_disconnect_by_func(GTK_OBJECT(z_input_model),
				GTK_SIGNAL_FUNC(rebuild_inputs_and_standards),
				ChannelWindow);

  zmodel_changed(z_input_model);

  /* Set the menuentry sensitive again */
  gtk_widget_set_sensitive(related_menuitem, TRUE);

  gtk_style_unref (istyle);

  ChannelWindow = NULL; /* No more channel window */

  return FALSE;
}

void
on_channels1_activate                  (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget * channel_editor;
  GtkWidget * country_options_menu;

  GtkWidget * channel_list;

  GtkWidget * new_menu;
  GtkWidget * menu_item = NULL;

  GtkWidget * move_channel_up;
  GtkWidget * move_channel_down;

  GtkWidget * spinslider;
  GtkAdjustment * spinslider_adj;

  int i = 0;
  int currently_tuned_country = 0;

  tveng_channels * tune;
  tveng_tuned_channel * tuned_channel;
  tveng_tuned_channel * list = NULL;

  if (ChannelWindow)
    {
      gdk_window_raise(ChannelWindow->window);
      return;
    }

  channel_editor = create_widget("channel_editor");
  country_options_menu = lookup_widget(channel_editor,
				       "country_options_menu");

  move_channel_up = lookup_widget(channel_editor, "move_channel_up");
  move_channel_down = lookup_widget(channel_editor, "move_channel_down");

  channel_list = lookup_widget(channel_editor, "channel_list");
  new_menu = gtk_menu_new();

  /* Let's setup the window */
  gtk_widget_destroy(gtk_option_menu_get_menu (GTK_OPTION_MENU
					       (country_options_menu)));

  while ((tune = tveng_get_country_tune_by_id(i)))
    {
      i++;
      if (tune == current_country)
	currently_tuned_country = i-1;
      menu_item = gtk_menu_item_new_with_label(_(tune->name));
      gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
			 GTK_SIGNAL_FUNC(on_country_switch),
			 tune);
      gtk_widget_show(menu_item);
      gtk_menu_append( GTK_MENU(new_menu), menu_item);
    }

  /* select the first item if there's no current country */
  if (current_country == NULL)
    currently_tuned_country = 0;

  gtk_widget_show(new_menu);

  gtk_option_menu_set_menu( GTK_OPTION_MENU(country_options_menu),
			    new_menu);

  gtk_option_menu_set_history ( GTK_OPTION_MENU(country_options_menu),
				currently_tuned_country);

  i = 0;

  while ((tuned_channel =
	  tveng_retrieve_tuned_channel_by_index(i++, global_channel_list)))
    list = tveng_append_tuned_channel(tuned_channel, list);

  istyle = gtk_style_copy (channel_editor->style);

  /* Cough. */
  istyle->fg[GTK_STATE_NORMAL] =
    istyle->fg[GTK_STATE_INSENSITIVE];

  build_channel_list(GTK_CLIST(channel_list), list);
  
  /* Change contry to the currently tuned one */
  if (menu_item)
    on_country_switch(menu_item, 
		      tveng_get_country_tune_by_id (currently_tuned_country));

  /* Save the disabled menuitem */
  gtk_object_set_user_data(GTK_OBJECT(channel_editor), menuitem);
  gtk_object_set_data(GTK_OBJECT(channel_editor), "list", list);

  gtk_widget_set_sensitive(GTK_WIDGET(menuitem), FALSE);

  /* Add fine tuning widget */
  spinslider_adj = GTK_ADJUSTMENT
    (gtk_adjustment_new (0, 0, 0, 0, 0, 0));
  spinslider = z_spinslider_new (spinslider_adj, NULL, "kHz", 0);
  gtk_widget_set_sensitive (spinslider, FALSE);
  gtk_widget_show (spinslider);
  gtk_table_attach_defaults(GTK_TABLE(lookup_widget(channel_editor,
						    "table72")),
			    spinslider, 1, 2, 2, 3);
  gtk_object_set_data (GTK_OBJECT (channel_editor),
		       "spinslider", spinslider);

  rebuild_inputs_and_standards(NULL, channel_editor);
  gtk_signal_connect(GTK_OBJECT(z_input_model), "changed",
		     GTK_SIGNAL_FUNC(rebuild_inputs_and_standards),
		     channel_editor);

  gtk_signal_connect(GTK_OBJECT(lookup_widget(channel_editor, "known_keys")),
		     "clicked",
		     GTK_SIGNAL_FUNC(on_known_keys_clicked),
		     channel_editor);

  gtk_signal_connect(GTK_OBJECT(move_channel_up), "clicked",
		     GTK_SIGNAL_FUNC(on_move_channel_up_clicked),
		     channel_editor);
  gtk_signal_connect(GTK_OBJECT(move_channel_down), "clicked",
		     GTK_SIGNAL_FUNC(on_move_channel_down_clicked),
		     channel_editor);
  gtk_signal_connect(GTK_OBJECT(channel_list), "unselect-row",
		     GTK_SIGNAL_FUNC(on_channel_list_unselect_row),
		     channel_editor);
  gtk_signal_connect(GTK_OBJECT(channel_editor), "delete-event",
		     GTK_SIGNAL_FUNC(on_channel_editor_delete_event),
		     channel_editor);
  gtk_signal_connect(GTK_OBJECT(spinslider_adj), "value-changed",
		     GTK_SIGNAL_FUNC(on_fine_tune_value_changed),
		     main_info);

  update_edit_buttons_sensitivity(channel_editor);

  gtk_widget_show(channel_editor);

  ChannelWindow = channel_editor; /* Set this, we are present */
}

/**
 * This is called when we are done processing the channels, to update
 *  the GUI
 */
void
on_channels_done_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget * channel_editor = lookup_widget(GTK_WIDGET (button),
					     "channel_editor"); /* The
					     channel editor window */
  GtkWidget * menu_item; /* The menu item asocciated with this entry */
  tveng_tuned_channel * list =
    gtk_object_get_data(GTK_OBJECT(channel_editor), "list");

  int index; /* The row we are reading now */

  tveng_tuned_channel * tc;

  /* Clear tuned channel list */
  global_channel_list =
    tveng_clear_tuned_channel(global_channel_list);

  index = 0;

  while ((tc = tveng_retrieve_tuned_channel_by_index(index++, list)))
    global_channel_list =
      tveng_append_tuned_channel(tc, global_channel_list);

  /* We are done, acknowledge the update in the model  */
  menu_item =
    GTK_WIDGET(gtk_object_get_user_data(GTK_OBJECT(channel_editor)));

  gtk_signal_disconnect_by_func(GTK_OBJECT(z_input_model),
				GTK_SIGNAL_FUNC(rebuild_inputs_and_standards),
				ChannelWindow);
  zmodel_changed(z_input_model);

  gtk_widget_set_sensitive(menu_item, TRUE);

  tveng_clear_tuned_channel(list);

  gtk_widget_destroy(channel_editor);

  ChannelWindow = NULL;
}

void
on_add_channel_clicked                 (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget *channel_name = lookup_widget(GTK_WIDGET(button),
					  "channel_name");
  GtkWidget *clist1 = lookup_widget(GTK_WIDGET(button), "clist1");
  gchar *buf =
    gtk_entry_get_text(GTK_ENTRY(channel_name));
  GList *ptr;
  gchar *buf2;
  gint index;

  if (!buf || !*buf)
    {
      ptr = GTK_CLIST(clist1) -> row_list;
      index = 0;
      while (ptr)
	{
	  if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	    { 
	      /* Add this selected channel to the channel list */
	      gtk_clist_get_text(GTK_CLIST(clist1), index, 0,
				 &(buf2));
	      break; /* found */
	    }
	  ptr = ptr -> next;
	  index++;
	}

      if (!ptr)
	buf2 = "";

      buf = Prompt(lookup_widget(GTK_WIDGET(button),
				 "channel_editor"),
		   _("Add Channel"), _("New channel name:"), buf2);
      
      if (buf)
	{
	  gtk_entry_set_text(GTK_ENTRY(channel_name), buf);
	  g_free(buf);
	}
      else
	return; /* cancelled */
    }

  real_add_channel(channel_name, user_data);
}

void
on_add_all_channels_clicked            (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget * channel_list = lookup_widget(GTK_WIDGET(button),
					   "channel_list");
  GtkWidget * channel_editor = lookup_widget(GTK_WIDGET(button),
					     "channel_editor");
  tveng_tuned_channel * list =
    gtk_object_get_data(GTK_OBJECT(channel_editor), "list");
  tveng_tuned_channel tc;
  tveng_channel *chan;
  int i = 0;

  while ((chan = tveng_get_channel_by_id(i++, current_country)))
    {
      memset(&tc, 0, sizeof(tveng_tuned_channel));
      tc.real_name = tc.name = chan->name;
      tc.country = current_country -> name;
      tc.freq = chan->freq;
      store_control_values(&tc.num_controls, &tc.controls, main_info);
      list = tveng_insert_tuned_channel_sorted(&tc, list);
      g_free(tc.controls);
    }

  gtk_object_set_data(GTK_OBJECT(channel_editor), "list", list);

  build_channel_list(GTK_CLIST(channel_list), list);
}

void
on_modify_channel_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget * clist1 = lookup_widget(GTK_WIDGET(button), "clist1");
  GtkWidget * channel_list = lookup_widget(GTK_WIDGET(button),
					   "channel_list");
  GtkWidget * channel_editor = lookup_widget(GTK_WIDGET(button),
					     "channel_editor");
  tveng_tuned_channel * list =
    gtk_object_get_data(GTK_OBJECT(channel_editor), "list");
  GtkWidget * channel_name = lookup_widget(GTK_WIDGET(button),
					   "channel_name");
  GtkWidget * channel_accel = lookup_widget(GTK_WIDGET(button),
					    "channel_accel");
  GtkWidget * widget;

  GList * ptr; /* Pointer to the selected item(s) in clist1 */
  int index=0; /* The row we are reading now */
  tveng_tuned_channel tc, *p;
  gchar * buffer;
  gint selected;
  gboolean selected_rows[tveng_tuned_channel_num(list)];

  memset(selected_rows, 0, sizeof(selected_rows));

  tc.name = gtk_entry_get_text (GTK_ENTRY(channel_name));
  if (current_country)
    tc.country = current_country -> name;
  else
    tc.country = NULL;

  tc.accel_key = 0;
  buffer = gtk_entry_get_text(GTK_ENTRY(channel_accel));
  if (buffer)
      tc.accel_key = gdk_keyval_from_name(buffer);
  if (tc.accel_key == GDK_VoidSymbol)
    tc.accel_key = 0;
  /*
    When this is a baseband input, tc.freq will be random.
    You'll get it when switching back to tuner, so it's
    even better to store the spinslider value (default 0)
    instead of initializing to zero or some.
  if (main_info->inputs &&
      main_info->inputs[main_info->cur_input].tuners)
  */
    {
      GtkWidget *spinslider =
	gtk_object_get_data (GTK_OBJECT (channel_editor),
			     "spinslider");

      tc.freq = z_spinslider_get_value (spinslider);
    }
  tc.accel_mask = 0;
  widget = lookup_widget(clist1, "channel_accel_ctrl");
  tc.accel_mask |=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ?
    GDK_CONTROL_MASK : 0;
  widget = lookup_widget(clist1, "channel_accel_shift");
  tc.accel_mask |=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ?
    GDK_SHIFT_MASK : 0;
  widget = lookup_widget(clist1, "channel_accel_alt");
  tc.accel_mask |=
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)) ?
    GDK_MOD1_MASK : 0;

  selected =
    z_option_menu_get_active(lookup_widget(clist1, "attached_input"));
  if (selected && main_info->inputs)
    tc.input = main_info->inputs[selected-1].hash;
  else
    tc.input = 0;

  selected =
    z_option_menu_get_active(lookup_widget(clist1, "attached_standard"));
  if (selected)
    tc.standard = main_info->standards[selected-1].hash;
  else
    tc.standard = 0;

  ptr = GTK_CLIST(clist1) -> row_list;

  /* Again, using a GUI element as a data storage struct is a
     HORRIBLE(tm) thing, but other things would be overcomplicated */
  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	{ 
	  gtk_clist_get_text(GTK_CLIST(clist1), index, 0,
			     &(tc.real_name));
	  break;
	}
      index ++;
      ptr = ptr -> next;
    }

  if (!ptr)
    return;

  store_control_values(&tc.num_controls, &tc.controls, main_info);

  index = 0;

  ptr = GTK_CLIST(channel_list) -> row_list;

  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	{
	  if ((p = tveng_retrieve_tuned_channel_by_index(index,
							 list)))
	    tveng_copy_tuned_channel(p, &tc);
	  selected_rows[index] = TRUE;
	}

      ptr = ptr -> next;
      index++;
    }

  g_free(tc.controls);
  build_channel_list(GTK_CLIST(channel_list), list);

  /* select channels again */
  gtk_signal_handler_block_by_func(GTK_OBJECT(channel_list),
			   GTK_SIGNAL_FUNC(on_channel_list_select_row),
			   NULL);

  for (index=0; index<tveng_tuned_channel_num(list); index++)
    if (selected_rows[index])
      gtk_clist_select_row(GTK_CLIST(channel_list), index, 0);
  gtk_signal_handler_unblock_by_func(GTK_OBJECT(channel_list),
			     GTK_SIGNAL_FUNC(on_channel_list_select_row),
			     NULL);

  update_edit_buttons_sensitivity(GTK_WIDGET(channel_list));
}

void
on_remove_channel_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget * channel_list = lookup_widget(GTK_WIDGET(button),
					   "channel_list");
  GtkWidget * channel_editor = lookup_widget(GTK_WIDGET(button),
					     "channel_editor");
  tveng_tuned_channel * list =
    gtk_object_get_data(GTK_OBJECT(channel_editor), "list");

  GList * ptr; /* Pointer to the selected item(s) in clist1 */
  int index; /* The row we are reading now */
  int deleted = 0;

  index = 0;

  ptr = GTK_CLIST(channel_list) -> row_list;

  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	{
	  /* Add this selected channel to the channel list */
	  list = tveng_remove_tuned_channel(NULL, index-deleted, list);
	  deleted++;
	}

      ptr = ptr -> next;
      index++;
    }

  gtk_object_set_data(GTK_OBJECT(channel_editor), "list", list);

  build_channel_list(GTK_CLIST(channel_list), list);
}

void
on_clist1_select_row                   (GtkWidget       *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  tveng_channels * country = (tveng_channels*)
    gtk_object_get_user_data( GTK_OBJECT(clist));
  tveng_channel * selected_channel =
    tveng_get_channel_by_id (row, country);

  if ((!selected_channel) || (!country) || (!main_info->inputs))
    {
      /* If we reach this it means that we are trying to select a item
       in the channel list but it hasn't been filled yet (it is filled
       by a callback) */
      return;
    }

  if (!main_info->inputs[main_info->cur_input].tuners)
    return;

  if (-1 == tveng_tune_input(selected_channel->freq, main_info))
    g_warning("Cannot tune input at %d: %s", selected_channel->freq,
	      main_info->error);
  else
    set_slider(selected_channel->freq,
	       lookup_widget(clist, "channel_editor"), main_info);
}

void
on_cancel_channels_clicked             (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget * channel_editor = lookup_widget(GTK_WIDGET (button),
					     "channel_editor"); /* The
					     channel editor window */
  GtkWidget * menu_item; /* The menu item asocciated with this entry */

  tveng_tuned_channel * list =
    gtk_object_get_data(GTK_OBJECT(channel_editor), "list");

  /* We are done, acknowledge the update in the channel list */
  menu_item =
    GTK_WIDGET(gtk_object_get_user_data(GTK_OBJECT(channel_editor)));

  tveng_clear_tuned_channel(list);

  gtk_signal_disconnect_by_func(GTK_OBJECT(z_input_model),
				GTK_SIGNAL_FUNC(rebuild_inputs_and_standards),
				ChannelWindow);

  zmodel_changed(z_input_model);

  gtk_widget_set_sensitive(menu_item, TRUE);

  gtk_widget_destroy(channel_editor);

  ChannelWindow = NULL;
}

void
on_channel_name_activate               (GtkWidget       *editable,
                                        gpointer         user_data)
{
  GtkWidget	*channel_list =
    lookup_widget(editable, "channel_list");
  GList *ptr = GTK_CLIST(channel_list) -> row_list;
  gint n_select = 0;

  while (ptr)
    {
      if (GTK_CLIST_ROW(ptr) -> state == GTK_STATE_SELECTED)
	n_select ++;

      ptr = ptr -> next;
    }

  if (n_select != 1)
    real_add_channel(editable, user_data);
  else
    on_modify_channel_clicked(GTK_BUTTON(lookup_widget(editable,
				"modify_channel")), user_data);
}

void
on_help_channels_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
  GnomeHelpMenuEntry help_ref = { NULL,
				  "channel_editor.html" };
  enum tveng_capture_mode cur_mode;

  cur_mode = tveng_stop_everything(main_info);

  help_ref.name = gnome_app_id;
  gnome_help_display (NULL, &help_ref);

  if (z_restart_everything(cur_mode, main_info) == -1)
    ShowBox(main_info->error, GNOME_MESSAGE_BOX_ERROR);
}

static gint control_timeout_id = -1;

static gint
control_timeout				(gpointer	data)
{
  control_timeout_id = -1;
  return FALSE;
}

void
on_channel_list_select_row             (GtkCList        *clist,
                                        gint             row,
                                        gint             column,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  tveng_channels * country;
  tveng_channel * channel;
  int country_id;
  int channel_id;
  GtkWidget * country_options_menu = lookup_widget(GTK_WIDGET (clist),
						   "country_options_menu");
  GtkWidget * clist1 = lookup_widget(GTK_WIDGET(clist), "clist1");
  GtkWidget * channel_name = lookup_widget(GTK_WIDGET(clist),
					   "channel_name");
  GtkWidget * channel_editor =
    lookup_widget(GTK_WIDGET(clist), "channel_editor");
  tveng_tuned_channel * list =
    gtk_object_get_data(GTK_OBJECT(channel_editor), "list");
  GtkWidget * channel_accel = lookup_widget(GTK_WIDGET(clist),
					    "channel_accel");
  GtkWidget * widget;
  struct tveng_enumstd *std;
  struct tveng_enum_input *input;

  if (control_timeout_id != -1)
    {
      /* This code only triggers when many channels have been selected
       in a short amount of time, avoid switching to all of them
       pointlessly */
      gtk_timeout_remove(control_timeout_id);
      control_timeout_id =
	gtk_timeout_add(50, (GtkFunction)control_timeout, NULL);
      return;
    }

  list = tveng_retrieve_tuned_channel_by_index(row, list);

  g_assert(list != NULL);

  country = tveng_get_country_tune_by_name (list->country);

  /* If we could understand the country, select it */
  if (country)
    {
      country_id = tveng_get_id_of_country_tune (country);
      if (country_id < 0)
	{
	  g_warning("Returned country tune id is invalid");
	  return;
	}

      channel = tveng_get_channel_by_name (list->real_name, country);
      if (!channel)
	{
	  g_warning("Channel %s cannot be found in current country: %s", 
		    list->real_name, country->name);
	  return;
	}

      channel_id = tveng_get_id_of_channel (channel, country);
      if (channel_id < 0)
	{
	  g_warning ("Returned channel id (%d) is not valid",
		     channel_id);
	  return;
	}

      gtk_option_menu_set_history ( GTK_OPTION_MENU(country_options_menu),
				    country_id);
      on_country_switch (clist1, country);

      gtk_clist_select_row(GTK_CLIST (clist1), channel_id, 0);
      /* make the row visible */
      gtk_clist_moveto(GTK_CLIST(clist1), channel_id, 0,
		       0.5, 0);
    }

  gtk_entry_set_text(GTK_ENTRY(channel_name), list->name);
  if (list->accel_key)
    {
      if (gdk_keyval_name(list->accel_key))
	gtk_entry_set_text(GTK_ENTRY(channel_accel),
			   gdk_keyval_name(list->accel_key));
      else
	gtk_entry_set_text(GTK_ENTRY(channel_accel), "");
      widget = lookup_widget(channel_accel, "channel_accel_ctrl");
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
				   list->accel_mask & GDK_CONTROL_MASK);
      widget = lookup_widget(channel_accel, "channel_accel_alt");
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
				   list->accel_mask & GDK_MOD1_MASK);
      widget = lookup_widget(channel_accel, "channel_accel_shift");
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
				   list->accel_mask & GDK_SHIFT_MASK);
    }
  else
    {
      gtk_entry_set_text(GTK_ENTRY(channel_accel), "");
      widget = lookup_widget(channel_accel, "channel_accel_ctrl");
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), FALSE);
      widget = lookup_widget(channel_accel, "channel_accel_alt");
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), FALSE);
      widget = lookup_widget(channel_accel, "channel_accel_shift");
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), FALSE);
    }

  /* Tune to this channel's freq */
  z_switch_channel(list, main_info);

  if (main_info->inputs &&
      main_info->inputs[main_info->cur_input].tuners)
    {
      set_slider(list->freq, channel_editor, main_info);
    }
  else if (list->input == 0)
    {
      /* Problematic. This is a "don't change input" and
       * despite displaying a RF the user can't change it
       * because we're not switched to a tuner, and we can't
       * switch to a tuner either. (Which one? Making this
       * silently a "tuner input channel"? The whole "don't
       * change" channel idea is kind of odd.)
       *
       * Anyway we set the slider to this freq for
       * on_input_changed while keeping it disabled. Same
       * below, see on_modify_channel_clicked and real_add_channel.
       */
      set_slider(list->freq, channel_editor, main_info);
    }
  else /* decidedly baseband input */
    {
      set_slider(list->freq, channel_editor, main_info);
    }

  widget = lookup_widget(channel_accel, "attached_standard");
  if (list->standard)
    {
      std = tveng_find_standard_by_hash(list->standard, main_info);
      if (std)
	z_option_menu_set_active(widget, std->index+1);
      else
	z_option_menu_set_active(widget, 0);
    }
  else
    z_option_menu_set_active(widget, 0);

  widget = lookup_widget(channel_accel, "attached_input");

  if (list->input)
    {
      input = tveng_find_input_by_hash(list->input, main_info);
      if (input)
	z_option_menu_set_active(widget, input->index+1);
      else
	z_option_menu_set_active(widget, 0);
    }
  else
    z_option_menu_set_active(widget, 0);

  update_edit_buttons_sensitivity(channel_editor);

  /* block this call a bit longer */
  control_timeout_id =
    gtk_timeout_add(50, (GtkFunction)control_timeout, NULL);
}

/*
  Called when a key is pressed in the channel list. Should call remove
  if the pressed key is Del
*/
gboolean
on_channel_list_key_press_event        (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data)
{
  GtkWidget * remove_channel = lookup_widget(widget, "remove_channel");
  if ((event->keyval == GDK_Delete) || (event->keyval == GDK_KP_Delete))
    {
      if (remove_channel)
	on_remove_channel_clicked(GTK_BUTTON(remove_channel), NULL);
      return TRUE; /* Processed */
    }

  return FALSE;
}

static
gint do_search (GtkWidget * searching)
{
  GtkWidget * progress = lookup_widget(searching, "progressbar1");
  GtkWidget * label80 = lookup_widget(searching, "label80");
  gint scanning_channel =
    GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(searching),
					"scanning_channel"));
  GtkWidget * channel_list =
    gtk_object_get_user_data(GTK_OBJECT(searching));
  GtkWidget * clist1 =
    lookup_widget(channel_list, "clist1");
  gint strength, afc;
  gchar * tuned_name=NULL;
  tveng_channel * channel;

  if (scanning_channel >= 0)
    {
      channel = tveng_get_channel_by_id(scanning_channel,
					current_country);
      g_assert(channel != NULL);

      if ((-1 != tveng_get_signal_strength(&strength, &afc, main_info)) &&
	  (strength > 0))
	{
	  GtkWidget * channel_name =
	    lookup_widget(channel_list, "channel_name");
	  guint32 last_freq = channel->freq, last_afc = afc;

	  /* zvbi should store the station name if known from now */
	  zvbi_name_unknown();
	  /* wait afc code, receive some VBI data to get the station,
	     etc. XXX should wake up earlier when the data is ready */
	  usleep(3e5);

	  /* Fine search (untested) */
	  while (afc && (afc != -last_afc))
	    {
	      /* Should be afc*50, but won't harm */
	      if (-1 == tveng_tune_input(last_freq + afc*25, main_info))
		break;
	      usleep(2e5);
	      if (-1 == tveng_get_signal_strength(&strength, &afc,
						  main_info) ||
		  !strength)
		break;
	      last_freq += afc*25;
	    }

	  if (zconf_get_boolean(NULL, "/zapping/options/vbi/use_vbi"))
	    tuned_name = zvbi_get_name();

	  if ((!zconf_get_boolean(NULL,
				  "/zapping/options/vbi/use_vbi")) ||
	      (!tuned_name))
	    tuned_name = g_strdup(channel->name);
	  gtk_entry_set_text(GTK_ENTRY(channel_name), tuned_name);
	  g_free(tuned_name);

	  real_add_channel(channel_name, NULL);
	}
    }

  scanning_channel++;

  /* Check if we have reached the end */
  if (current_country->chan_count <= scanning_channel)
    {
      gtk_widget_destroy(searching);
      return FALSE;
    }

  gtk_progress_set_percentage(GTK_PROGRESS(progress),
     ((gfloat)scanning_channel)/current_country->chan_count);

  channel = tveng_get_channel_by_id(scanning_channel,
				    current_country);
  g_assert(channel != NULL);
  gtk_label_set_text(GTK_LABEL(label80), channel->name);

  gtk_clist_select_row(GTK_CLIST (clist1), scanning_channel, 0);

  /* make the row visible */
  gtk_clist_moveto(GTK_CLIST(clist1), scanning_channel, 0,
		   0.5, 0);

  /* Tune to this channel's freq */
  if (-1 == tveng_tune_input (channel->freq, main_info))
    g_warning("While tuning: %s", main_info -> error);

  gtk_object_set_data(GTK_OBJECT(searching), "scanning_channel",
		      GINT_TO_POINTER(scanning_channel));

  /* The timeout has to be big enough to let the tuner estabilize */
  gtk_object_set_data(GTK_OBJECT(searching), "timeout",
		      GINT_TO_POINTER((gtk_timeout_add(150,
			    (GtkFunction)do_search, searching))));

  return FALSE;
  /*
    rationale: returning TRUE might look like it does the same thing as
    above, but it doesn't. We want 150ms from now, not from when this
    callback was called. This is because some vbi functions here might
    take some time to complete.
  */
}

void
on_channel_search_clicked              (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget * channel_editor =
    lookup_widget(GTK_WIDGET(button), "channel_editor");
  GtkWidget * channel_list =
    lookup_widget(channel_editor, "channel_list");
  GtkWidget * searching;
  GtkWidget * progress;
  gint timeout;

  /* Make a prove to see whether it's possible to get the signal
     strength */
  if (-1 == tveng_get_signal_strength(NULL, NULL, main_info))
    {
      /* Channel auto-searching won't work with XVideo */
      if (main_info->current_controller == TVENG_CONTROLLER_XV)
	ShowBox(_("Channel autosearching won't work with XVideo.\n"
		  "Please switch to another controller by starting\n"
		  "Capture mode (\"View/Go Capturing\" menu "
		  "entry).\nReported error:\n%s"),
		GNOME_MESSAGE_BOX_WARNING, main_info->error);
      else
	ShowBox(_("Your current V4L/V4L2 driver cannot do "
		  "channel autosearching, sorry"),
		GNOME_MESSAGE_BOX_INFO);
      return;
    }

  searching = create_searching();
  progress = lookup_widget(searching, "progressbar1");

  gtk_progress_set_percentage(GTK_PROGRESS(progress), 0.0);

  /* The timeout has to be big enough to let the tuner estabilize */
  timeout = gtk_timeout_add(150, (GtkFunction)do_search, searching);

  gtk_object_set_user_data(GTK_OBJECT(searching), channel_list);
  gtk_object_set_data(GTK_OBJECT(searching), "timeout",
		      GINT_TO_POINTER(timeout));
  gtk_object_set_data(GTK_OBJECT(searching), "scanning_channel",
		      GINT_TO_POINTER(-1));

  gtk_widget_show(searching);
}

void
on_cancel_search_clicked               (GtkButton       *button,
                                        gpointer         user_data)
{
  GtkWidget * searching =
    lookup_widget(GTK_WIDGET(button), "searching");
  gint timeout =
    GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(searching), "timeout"));

  gtk_timeout_remove(timeout);

  gtk_widget_destroy(searching);
}

gboolean
on_searching_delete_event              (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
  gint timeout =
    GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(widget), "timeout"));

  gtk_timeout_remove(timeout);

  return FALSE;
}
