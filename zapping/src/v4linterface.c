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

extern tveng_tuned_channel * global_channel_list;
extern tveng_device_info *main_info;
extern GtkWidget *main_window;
extern int cur_tuned_channel; /* currently tuned channel (in callbacks.c) */
GtkWidget * ToolBox = NULL; /* Pointer to the last control box */
ZModel *z_input_model = NULL;

/* Minimize updates */
static gboolean freeze = FALSE, needs_refresh = FALSE;

/* Activate an input */
static
void on_input_activate              (GtkMenuItem     *menuitem,
				     gpointer        user_data)
{
  z_switch_input(GPOINTER_TO_INT(user_data), main_info);
}

/* Activate an standard */
static
void on_standard_activate              (GtkMenuItem     *menuitem,
					gpointer        user_data)
{
  z_switch_standard(GPOINTER_TO_INT(user_data), main_info);
}

static void
update_bundle				(ZModel		*model,
					 tveng_device_info *info)
{
  GtkMenuItem *channels =
    GTK_MENU_ITEM(lookup_widget(main_window, "channels"));
  GtkMenu *menu, *menu2;
  GtkWidget *menu_item;
  gint i;
  tveng_tuned_channel *tc;
  gchar *tooltip;
  gboolean sth = FALSE;

  if (freeze)
    {
      needs_refresh = TRUE;
      return;
    }

  menu = GTK_MENU(gtk_menu_new());

  menu_item = gtk_tearoff_menu_item_new();
  gtk_widget_show(menu_item);
  gtk_menu_append(menu, menu_item);

  /* If no tuned channels show error not sensitive */
  if (tveng_tuned_channel_num(global_channel_list))
    for (i = 0;
	 (tc = tveng_retrieve_tuned_channel_by_index(i,
						     global_channel_list));
	 i++)
      {
	menu_item =
	  z_gtk_pixmap_menu_item_new(tc->name,
				     GNOME_STOCK_PIXMAP_PROPERTIES);
	gtk_signal_connect_object(GTK_OBJECT(menu_item), "activate",
				  GTK_SIGNAL_FUNC(z_select_channel),
				  GINT_TO_POINTER(i));
	tooltip = build_channel_tooltip(tc);
	if (tooltip)
	  {
	    set_tooltip(menu_item, tooltip);
	    g_free(tooltip);
	  }
	gtk_widget_show(menu_item);
	gtk_menu_append(menu, menu_item);
	sth = TRUE;
      }
  else
    {
      menu_item = gtk_menu_item_new_with_label(_("No tuned channels"));
      gtk_widget_show (menu_item);
      gtk_widget_set_sensitive(menu_item, FALSE);
      gtk_menu_append (GTK_MENU (menu), menu_item);
    }

  if (info->num_standards || info->num_inputs)
    {
      sth = TRUE;
      /* separator */
      menu_item = gtk_menu_item_new();
      gtk_widget_show(menu_item);
      gtk_menu_append (GTK_MENU (menu), menu_item);
    }

  if (info->num_inputs)
    {
      menu2 = GTK_MENU(gtk_menu_new());
      menu_item =
	    z_gtk_pixmap_menu_item_new(_("Inputs"),
				       GNOME_STOCK_PIXMAP_LINE_IN);
      gtk_widget_show(menu_item);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
				GTK_WIDGET(menu2));
      gtk_menu_append(menu, menu_item);
      for (i = 0; i<info->num_inputs; i++)
	{
	  menu_item =
	    z_gtk_pixmap_menu_item_new(info->inputs[i].name,
				       GNOME_STOCK_PIXMAP_LINE_IN);
	  gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
			     GTK_SIGNAL_FUNC(on_input_activate),
			     GINT_TO_POINTER(info->inputs[i].hash));
	  gtk_widget_show(menu_item);
	  gtk_menu_append(menu2, menu_item);
	}
    }

  if (info->num_standards)
    {
      menu2 = GTK_MENU(gtk_menu_new());
      menu_item =
	    z_gtk_pixmap_menu_item_new("Standards",
				       GNOME_STOCK_PIXMAP_COLORSELECTOR);
      gtk_widget_show(menu_item);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
				GTK_WIDGET(menu2));
      gtk_menu_append(menu, menu_item);
      for (i = 0; i<info->num_standards; i++)
	{
	  menu_item =
	    z_gtk_pixmap_menu_item_new(info->standards[i].name,
				       GNOME_STOCK_PIXMAP_COLORSELECTOR);
	  gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
			     GTK_SIGNAL_FUNC(on_standard_activate),
			     GINT_TO_POINTER(info->standards[i].hash));
	  gtk_widget_show(menu_item);
	  gtk_menu_append(menu2, menu_item);
	}
    }

  gtk_widget_show(GTK_WIDGET(menu));
  gtk_menu_item_remove_submenu(channels);
  gtk_menu_item_set_submenu(channels, GTK_WIDGET(menu));
  gtk_widget_set_sensitive(GTK_WIDGET(channels), sth);
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
						     itself */
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
build_control_box(GtkWidget * hbox, tveng_device_info * info)
{
  GtkWidget * control_added;
  int i = 0;
  struct tveng_control * control;
  GtkWidget *vbox = NULL;

  /* Update the values of all the controls */
  if (-1 == tveng_update_controls( info ))
    {
      ShowBox("Tveng critical error, Zapping will exit NOW.",
	      GNOME_MESSAGE_BOX_ERROR);
      g_error("tveng critical: %s", info->error);
    }

  for (i = 0; i < info->num_controls; i++)
    {
      if ((i%10) == 0)
	{
	  if (vbox)
	    {
	      gtk_widget_show(vbox);
	      gtk_box_pack_start_defaults(GTK_BOX(hbox), vbox);
	    }
	  vbox = gtk_vbox_new(FALSE, 10);
	}

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
	  g_warning("Type %d of control %s is not supported",
		    control->type, control->name);
	  continue;
	}

      if (control_added)
	{
	  gtk_widget_show(control_added);
	  gtk_box_pack_start(GTK_BOX(vbox), control_added, FALSE,
			     FALSE, 0);
	}
      else
	g_warning("Error adding %s", control->name);
    }

  if (vbox)
    {
      gtk_widget_show(vbox);
      gtk_box_pack_start_defaults(GTK_BOX(hbox), vbox);
    }
}

static
gboolean on_control_box_key_press	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	data)
{
  switch (event->keyval)
    {
    case GDK_Escape:
      gtk_widget_destroy(widget);
      break;
    case GDK_c:
    case GDK_C:
      if (event->state & GDK_CONTROL_MASK)
	{
	  gtk_widget_destroy(widget);
	  break;
	}
    default:
      return FALSE;
    }

  return TRUE;
}

static void
on_control_box_destroy		       (GtkWidget      *widget,
					gpointer        user_data)
{
  GtkWidget * related_button;

  related_button =
    GTK_WIDGET(gtk_object_get_user_data(GTK_OBJECT(widget)));

  gtk_widget_set_sensitive(related_button, TRUE);

  ToolBox = NULL;
}

GtkWidget * create_control_box(tveng_device_info * info)
{
  GtkWidget * control_box;
  GtkWidget * hbox;

  control_box = gtk_window_new(GTK_WINDOW_DIALOG);
  gtk_window_set_title(GTK_WINDOW(control_box), _("Available controls"));

  hbox = gtk_hbox_new(FALSE, 0);

  build_control_box(hbox, info);

  gtk_widget_show(hbox);
  gtk_container_add(GTK_CONTAINER (control_box), hbox);

  gtk_window_set_policy(GTK_WINDOW(control_box), FALSE, FALSE, FALSE);
  gtk_object_set_data(GTK_OBJECT(control_box), "hbox", hbox);
  gtk_signal_connect(GTK_OBJECT(control_box), "destroy",
		     GTK_SIGNAL_FUNC(on_control_box_destroy),
		     NULL);

  gtk_signal_connect(GTK_OBJECT(control_box), "key-press-event",
		     GTK_SIGNAL_FUNC(on_control_box_key_press),
		     NULL);

  return (control_box);
}

void
update_control_box(tveng_device_info * info)
{
  GtkWidget * hbox;
  GtkWidget * control_box = ToolBox;

  if ((!info) || (!control_box))
    return;

  hbox = GTK_WIDGET(gtk_object_get_data(GTK_OBJECT(control_box),
					"hbox"));
  gtk_container_remove(GTK_CONTAINER (control_box), hbox);

  hbox = gtk_hbox_new(FALSE, 0);

  build_control_box(hbox, info);

  gtk_widget_show(hbox);
  gtk_container_add(GTK_CONTAINER (control_box), hbox);
  gtk_object_set_data(GTK_OBJECT(control_box), "hbox", hbox);
}

void
z_switch_input			(int hash, tveng_device_info *info)
{
  struct tveng_enum_input *input =
    tveng_find_input_by_hash(hash, info);

  if (!input)
    {
      ShowBox("Couldn't find input with hash %x",
	      GNOME_MESSAGE_BOX_ERROR, hash);
      return;
    }

  if (input->index == info->cur_input)
    return;

  if (tveng_set_input(input, info) == -1)
    ShowBox("Couldn't switch to input %s\n%s",
	    GNOME_MESSAGE_BOX_ERROR,
	    input->name, info->error);
  else
    zmodel_changed(z_input_model);
}

void
z_switch_standard		(int hash, tveng_device_info *info)
{
  struct tveng_enumstd *standard =
    tveng_find_standard_by_hash(hash, info);

  if (!standard)
    {
      if (info->num_standards)
	ShowBox("Couldn't find standard with hash %x",
		GNOME_MESSAGE_BOX_ERROR, hash);
      return;
    }

  if (standard->index == info->cur_standard)
    return;

  if (tveng_set_standard(standard, info) == -1)
    ShowBox("Couldn't switch to standard %s\n%s",
	    GNOME_MESSAGE_BOX_ERROR,
	    standard->name, info->error);
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

static
void load_control_values(gint num_controls,
			 tveng_tc_control *list,
			 tveng_device_info *info)
{
  gint i, j, value;
  struct tveng_control *c;

  for (i = 0; i<num_controls; i++)
    for (j = 0; j<info->num_controls; j++)
      if (normstrcmp(info->controls[j].name,
		     list[i].name))
	{
	  c = &info->controls[j];
	  value = rint(c->min + (c->max - c->min)*list[i].value);
	  tveng_set_control(c, value, info);
	}
}

/*
  Substitute the special search keywords by the appropiate thing,
  returns a newly allocated string, and g_free's the given string.
  Valid search keywords:
  $(alias) -> tc->name
  $(index) -> tc->index
  $(id) -> tc->real_name
*/
static
gchar *substitute_keywords	(gchar		*string,
				 tveng_tuned_channel *tc,
				 gchar          *default_name)
{
  gint i;
  gchar *found, *buffer = NULL, *p;
  struct tveng_enum_input *ei;
  struct tveng_enumstd *es;
  gchar *search_keys[] =
  {
    "$(alias)",
    "$(index)",
    "$(id)",
    "$(input)",
    "$(standard)",
    "$(freq)"
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
	   if (tc->real_name)
	     buffer = g_strdup(tc->real_name);
	   else
	     buffer = g_strdup(_("No id"));
	   break;
	 case 3:
	   if ((ei = tveng_find_input_by_hash(tc->input, main_info)))
	     buffer = g_strdup(ei->name);
	   else
	     buffer = g_strdup(_("No input"));
	   break;
	 case 4:
	   if ((es = tveng_find_standard_by_hash(tc->standard, main_info)))
	     buffer = g_strdup(es->name);
	   else
	     buffer = g_strdup(_("No standard"));
	   break;
	 case 5:
	   buffer = g_strdup_printf("%d", tc->freq);
	   break;
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

  memset(&ch, 0, sizeof(ch));

  if (!channel)
    channel = &ch;

  if (channel != &ch
      || channel->name || default_name)
    buffer = substitute_keywords(g_strdup(zcg_char(NULL, "title_format")),
				 channel, default_name);
  if (buffer && *buffer)
    gtk_window_set_title(GTK_WINDOW(main_window), buffer);
  else
    gtk_window_set_title(GTK_WINDOW(main_window), "Zapping");

  g_free(buffer);

  if (channel != &ch)
    cur_tuned_channel = channel->index;
}

/* Do not save the control values in the first switch_channel */
static gboolean first_switch = TRUE;

void
z_switch_channel	(tveng_tuned_channel	*channel,
			 tveng_device_info	*info)
{
  int mute=0;
  gboolean was_first_switch = first_switch;
  tveng_tuned_channel *tc;
  gboolean in_global_list =
    tveng_tuned_channel_in_list(channel, global_channel_list);

  if (!channel)
    return;

  if (in_global_list &&
      (tc = tveng_retrieve_tuned_channel_by_index(cur_tuned_channel,
						  global_channel_list)))
    {
      if (!first_switch)
	{
	  g_free(tc->controls);
	  store_control_values(&tc->num_controls, &tc->controls, info);
	}
      else
	first_switch = FALSE;
    }

  if (zcg_bool(NULL, "avoid_noise"))
    {
      mute = tveng_get_mute(info);
      
      if (!mute)
	tveng_set_mute(1, info);
    }

  freeze_update();

  /* force rebuild on startup */
  if (was_first_switch)
    zmodel_changed(z_input_model);

  if (channel->input)
    z_switch_input(channel->input, info);

  if (channel->standard)
    z_switch_standard(channel->standard, info);

  if (info->num_inputs && info->inputs[info->cur_input].tuners)
    if (-1 == tveng_tune_input (channel->freq, info))
      ShowBox(info -> error, GNOME_MESSAGE_BOX_ERROR);

  if (zcg_bool(NULL, "avoid_noise"))
    {
      /* Sleep a little so the noise disappears */
      usleep(100000);
      
      if (!mute)
	tveng_set_mute(0, info);
    }

  if (in_global_list)
    z_set_main_title(channel, NULL);
  else
    gtk_window_set_title(GTK_WINDOW(main_window), "Zapping");

  thaw_update();

  if (channel->num_controls &&
      zcg_bool(NULL, "save_controls"))
    load_control_values(channel->num_controls, channel->controls,
			info);

  update_control_box(info);
}

void
z_select_channel			(gint num_channel)
{
  tveng_tuned_channel * channel =
    tveng_retrieve_tuned_channel_by_index(num_channel, global_channel_list);

  if (!channel)
    {
      g_warning("Cannot tune given channel %d (no such channel)",
		num_channel);
      return;
    }

  z_switch_channel(channel, main_info);
}

void
z_channel_up				(void)
{
  gint num_channels = tveng_tuned_channel_num(global_channel_list);
  gint new_channel;

  if (num_channels == 0) /* If there are no tuned channels stop
			    processing */
    return;

  new_channel = cur_tuned_channel - 1;
  if (new_channel < 0)
    new_channel = num_channels - 1;
  
  z_select_channel(new_channel);
}

void
z_channel_down				(void)
{
  gint num_channels = tveng_tuned_channel_num(global_channel_list);
  gint new_channel;

  if (num_channels == 0) /* If there are no tuned channels stop
			    processing */
    return;

  new_channel = cur_tuned_channel + 1;
  if (new_channel >= num_channels)
    new_channel = 0;

  z_select_channel(new_channel);
}

void store_control_values(gint *num_controls,
			  tveng_tc_control **list,
			  tveng_device_info *info)
{
  gint i;
  struct tveng_control *c;

  g_assert(info != NULL);
  g_assert(list != NULL);
  g_assert(num_controls != NULL);

  *num_controls = info->num_controls;

  if (*num_controls)
    {
      *list = g_malloc(sizeof(tveng_tc_control) * *num_controls);
      for (i = 0; i<*num_controls; i++)
	{
	  c = info->controls+i;
	  strncpy((*list)[i].name, c->name, 32);
	  if (c->max > c->min)
	    (*list)[i].value = (((gfloat)c->cur_value)-c->min)/
	      ((gfloat)c->max-c->min);
	  else
	    (*list)[i].value = 0;
	}
    }
  else
    *list = NULL;
}

void
startup_v4linterface(tveng_device_info *info)
{
  z_input_model = ZMODEL(zmodel_new());

  gtk_signal_connect(GTK_OBJECT(z_input_model), "changed",
		     GTK_SIGNAL_FUNC(update_bundle),
		     info);

  zcc_char("Zapping: $(alias)", "Title format Z will use", "title_format");
}

void
shutdown_v4linterface(void)
{
  gtk_object_destroy(GTK_OBJECT(z_input_model));
}













