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
#include "zvbi.h"
#include "osd.h"
#include "remote.h"

extern tveng_tuned_channel * global_channel_list;
extern tveng_device_info *main_info;
extern GtkWidget *main_window;
extern int cur_tuned_channel; /* currently tuned channel (in callbacks.c) */
GtkWidget * ToolBox = NULL; /* Pointer to the last control box */
ZModel *z_input_model = NULL;

/* Minimize updates */
static gboolean freeze = FALSE, needs_refresh = FALSE;

static void
update_bundle				(ZModel		*model,
					 tveng_device_info *info)
{
  GtkMenuItem *channels =
    GTK_MENU_ITEM(lookup_widget(main_window, "channels"));
  GtkMenu *menu;
  GtkWidget *menu_item;

  if (freeze)
    {
      needs_refresh = TRUE;
      return;
    }

  menu = GTK_MENU(gtk_menu_new());

  menu_item = gtk_tearoff_menu_item_new();
  gtk_widget_show(menu_item);
  gtk_menu_append(menu, menu_item);

  gtk_widget_show(GTK_WIDGET(menu));
  gtk_menu_item_remove_submenu(channels);
  gtk_menu_item_set_submenu(channels, GTK_WIDGET(menu));
  gtk_widget_set_sensitive(GTK_WIDGET(channels),
			   add_channel_entries(menu, 1, 16, info));
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

/* Creates a spinbutton/slider pair */
static
GtkWidget * create_slider(struct tveng_control * qc,
			  int index,
			  tveng_device_info * info)
{ 
  GtkWidget * vbox; /* We have a slider and a label */
  GtkWidget * spinslider;
  GtkWidget * label;
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

  spinslider = z_spinslider_new (GTK_ADJUSTMENT (adj), NULL,
				 NULL, qc -> def_value);
  gtk_widget_show (spinslider);
  gtk_box_pack_start_defaults (GTK_BOX (vbox), spinslider);

  z_spinslider_set_value (spinslider, cur_value);
  
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

  set_mute1(3, FALSE, FALSE); /* update */
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

  gtk_window_set_policy(GTK_WINDOW(control_box), FALSE, TRUE, FALSE);
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
  $(id) -> tc->rf_name
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
#ifdef HAVE_LIBZVBI
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

  memset(&ch, 0, sizeof(ch));

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
	  if (zcg_bool(NULL, "save_controls"))
	    store_control_values(&tc->num_controls, &tc->controls,
				 info);
	  else
	    {
	      tc->num_controls = 0;
	      tc->controls = NULL;
	    }
	}
      else
	first_switch = FALSE;
    }

  if (zcg_bool(NULL, "avoid_noise"))
    {
      mute = tveng_get_mute(info);
      set_mute1(TRUE, TRUE, FALSE);
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
	set_mute1(FALSE, TRUE, FALSE);
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

#ifdef HAVE_LIBZVBI
  zvbi_channel_switched();

  if (info->current_mode == TVENG_CAPTURE_PREVIEW)
    osd_render_sgml(NULL, _("Channel: <yellow>%s</yellow>"), channel->name);
#endif
}

static void
select_channel (gint num_channel)
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

gboolean
channel_up_cmd				(GtkWidget *	widget,
					 gint		argc,
					 gchar **	argv,
					 gpointer	user_data)
{
  gint num_channels = tveng_tuned_channel_num(global_channel_list);
  gint new_channel;

  if (num_channels == 0) /* If there are no tuned channels stop
			    processing */
    return TRUE;

  new_channel = cur_tuned_channel + 1;
  if (new_channel >= num_channels)
    new_channel = 0;

  select_channel(new_channel);

  return TRUE;
}

gboolean
channel_down_cmd			(GtkWidget *	widget,
					 gint		argc,
					 gchar **	argv,
					 gpointer	user_data)
{
  gint num_channels = tveng_tuned_channel_num(global_channel_list);
  gint new_channel;

  if (num_channels == 0) /* If there are no tuned channels stop
			    processing */
    return TRUE;

  new_channel = cur_tuned_channel - 1;
  if (new_channel < 0)
    new_channel = num_channels - 1;
  
  select_channel(new_channel);

  return TRUE;
}

/*
 *  Select a channel by index into the the channel list.
 */
static gboolean
set_channel_cmd				(GtkWidget *	widget,
					 gint		argc,
					 gchar **	argv,
					 gpointer	user_data)
{
  gint num_channels;
  gint i;

  if (argc < 2 || !argv[1][0])
    return FALSE;

  i = strtol(argv[1], NULL, 0);

  num_channels = tveng_tuned_channel_num(global_channel_list);

  /* IMHO it's wrong to start at 0, but compatibility rules. */
  if (i >= 0 && i < num_channels)
    {
      select_channel(i);
      return TRUE;
    }

  return FALSE;
}

/*
 *  Select a channel by station name ("MSNBCBS", "Linux TV", ...),
 *  when not found by channel name ("5", "S7", ...)
 */
static gboolean
lookup_channel_cmd			(GtkWidget *	widget,
					 gint		argc,
					 gchar **	argv,
					 gpointer	user_data)
{
  tveng_tuned_channel *tc;
  gint i;

  if (argc < 2 || !argv[1][0])
    return FALSE;

  for (i = 0; (tc = tveng_retrieve_tuned_channel_by_index
	       (i, global_channel_list)); i++)
    if (strcasecmp(argv[1], tc->name) == 0)
      {
	z_switch_channel(tc, main_info);
	return TRUE;
      }

  for (i = 0; (tc = tveng_retrieve_tuned_channel_by_index
	       (i, global_channel_list)); i++)
    if (strcasecmp(argv[1], tc->rf_name) == 0)
      {
	z_switch_channel(tc, main_info);
	return TRUE;
      }

  return FALSE;
}

static gchar			kp_chsel_buf[5];
static gint			kp_chsel_prefix;
static gboolean			kp_clear;
static gboolean			kp_lirc;

static void
kp_timeout			(gboolean timer)
{
  gchar *vec[2] = { 0, kp_chsel_buf };
  gint txl = zconf_get_integer (NULL, "/zapping/options/main/channel_txl");

  if (timer && (txl >= 0 || kp_lirc)) /* -1 disable, 0 list entry, 1 RF channel */
    {
      if (!isdigit(kp_chsel_buf[0]) || txl >= 1)
	lookup_channel_cmd (NULL, 2, vec, NULL);
      else
	set_channel_cmd (NULL, 2, vec, NULL);
    }

  if (kp_clear)
    {
      kp_chsel_buf[0] = 0;
      kp_chsel_prefix = 0;
    }
}

static gboolean
kp_key_press			(GdkEventKey *	event,
				 gint txl)
{
  extern tveng_rf_table *current_country; /* Currently selected contry */
  gchar *vec[2] = { 0, kp_chsel_buf };
  tveng_tuned_channel *tc;
  const gchar *prefix;
  z_key key;
  int i;

  switch (event->keyval)
      {
#ifdef HAVE_LIBZVBI
      case GDK_KP_0 ... GDK_KP_9:
	i = strlen (kp_chsel_buf);

	if (i >= sizeof (kp_chsel_buf) - 1)
	  memcpy (kp_chsel_buf, kp_chsel_buf + 1, i--);

	kp_chsel_buf[i] = event->keyval - GDK_KP_0 + '0';
	kp_chsel_buf[i + 1] = 0;

      show:
	kp_clear = FALSE;
	/* NLS: Channel name being entered on numeric keypad */
	osd_render_sgml (kp_timeout, _("<green>%s</green>"), kp_chsel_buf);
	kp_clear = TRUE;

	return TRUE;

      case GDK_KP_Decimal:
	/* Run through all RF channel prefixes incl. nil (== clear) */

	prefix = current_country->prefixes[kp_chsel_prefix];

	if (prefix)
	  {
	    strncpy (kp_chsel_buf, prefix, sizeof (kp_chsel_buf) - 1);
	    kp_chsel_prefix++;
	    goto show;
	  }
	else
	  {
	    kp_clear = TRUE;
	    osd_render_sgml (kp_timeout, "<black>/</black>");
	  }

	return TRUE;

      case GDK_KP_Enter:
	if (!isdigit (kp_chsel_buf[0]) || txl >= 1)
	  lookup_channel_cmd (NULL, 2, vec, NULL);
	else
	  set_channel_cmd (NULL, 2, vec, NULL);

	kp_chsel_buf[0] = 0;
	kp_chsel_prefix = 0;

	return TRUE;

#endif /* HAVE_LIBZVBI */

      default:
	break;
      }

  return FALSE; /* not for us, pass it on */
}

/*
 * Called from alirc.c, preliminary.
 */
gboolean
channel_key_press		(GdkEventKey *		event)
{
  gint txl;

  txl = zconf_get_integer (NULL, "/zapping/options/main/channel_txl");

  kp_lirc = TRUE;

  return kp_key_press (event, txl);
}

gboolean
on_channel_key_press			(GtkWidget *	widget,
					 GdkEventKey *	event,
					 gpointer	user_data)
{
  extern tveng_rf_table *current_country; /* Currently selected contry */
  gchar *vec[2] = { 0, kp_chsel_buf };
  tveng_tuned_channel *tc;
  const gchar *prefix;
  z_key key;
  int txl, i;

  txl = zconf_get_integer (NULL, "/zapping/options/main/channel_txl");

  if (txl >= 0) /* !disabled */
    if (kp_key_press (event, txl))
      return TRUE;

  /* Channel accelerators */

  key.key = gdk_keyval_to_lower (event->keyval);
  key.mask = event->state;

  for (i = 0; (tc = tveng_retrieve_tuned_channel_by_index
	       (i, global_channel_list)); i++)
    if (z_key_equal (tc->accel, key))
      {
	select_channel (tc->index);
	return TRUE;
      }

  return FALSE; /* not for us, pass it on */
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

/* Activate an input */
static
void on_input_activate              (GtkMenuItem     *menuitem,
				     gpointer        user_data)
{
  z_switch_input(GPOINTER_TO_INT(user_data), main_info);
#ifdef HAVE_LIBZVBI
  zvbi_channel_switched();
#endif
}

/* Activate an standard */
static
void on_standard_activate              (GtkMenuItem     *menuitem,
					gpointer        user_data)
{
  z_switch_standard(GPOINTER_TO_INT(user_data), main_info);
#ifdef HAVE_LIBZVBI
  zvbi_channel_switched();
#endif
}

static inline void
insert_one_channel			(GtkMenu *menu,
					 gint index,
					 gint pos)
{
  tveng_tuned_channel *tuned =
    tveng_retrieve_tuned_channel_by_index(index, global_channel_list);
  gchar *tooltip;
  GtkWidget *menu_item =
    z_gtk_pixmap_menu_item_new(tuned->name,
			       GNOME_STOCK_PIXMAP_PROPERTIES);
  gtk_signal_connect_object(GTK_OBJECT(menu_item), "activate",
			    GTK_SIGNAL_FUNC(select_channel),
			    (GtkObject*)GINT_TO_POINTER(index));

  if ((tooltip = z_key_name (tuned->accel)))
    {
      z_tooltip_set (menu_item, tooltip);
      g_free (tooltip);
    }

  gtk_widget_show(menu_item);
  gtk_menu_insert(menu, menu_item, pos);
}

/* Returns whether something (useful) was added */
gboolean
add_channel_entries			(GtkMenu *menu,
					 gint pos,
					 gint menu_max_entries,
					 tveng_device_info *info)
{
  GtkWidget *menu_item = NULL;
  GtkMenu *menu2 = NULL;
  gchar *buf = NULL;
  gint i;
  gboolean sth = FALSE;

  if (info->num_standards)
    {
      menu2 = GTK_MENU(gtk_menu_new());
      menu_item =
	    z_gtk_pixmap_menu_item_new("Standards",
				       GNOME_STOCK_PIXMAP_COLORSELECTOR);
      gtk_widget_show(menu_item);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
				GTK_WIDGET(menu2));
      gtk_menu_insert(menu, menu_item, pos);
      menu_item = gtk_tearoff_menu_item_new();
      gtk_widget_show(menu_item);
      gtk_menu_append(menu2, menu_item);
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

  if (info->num_inputs)
    {
      menu2 = GTK_MENU(gtk_menu_new());
      menu_item =
	    z_gtk_pixmap_menu_item_new(_("Inputs"),
				       GNOME_STOCK_PIXMAP_LINE_IN);
      gtk_widget_show(menu_item);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
				GTK_WIDGET(menu2));
      gtk_menu_insert(menu, menu_item, pos);
      menu_item = gtk_tearoff_menu_item_new();
      gtk_widget_show(menu_item);
      gtk_menu_append(menu2, menu_item);
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

  if ((info->num_standards || info->num_inputs) &&
      tveng_tuned_channel_num(global_channel_list))
    {
      /* separator */
      menu_item = gtk_menu_item_new();
      gtk_widget_show(menu_item);
      gtk_menu_insert (menu, menu_item, pos);
      sth = TRUE;
    }

#define ITEMS_PER_SUBMENU 20

  if (tveng_tuned_channel_num(global_channel_list) == 0)
    {
      menu_item = z_gtk_pixmap_menu_item_new(_("No tuned channels"),
					     GNOME_STOCK_PIXMAP_CLOSE);
      gtk_widget_set_sensitive(menu_item, FALSE);
      gtk_widget_show(menu_item);
      gtk_menu_insert(menu, menu_item, pos);
      /* This doesn't count as something added */
    }
  else
    {
      sth = TRUE;
      i = tveng_tuned_channel_num(global_channel_list);
      if (i <= ITEMS_PER_SUBMENU && i <= menu_max_entries)
	for (--i;i>=0;i--)
	  insert_one_channel(menu, i, pos);
      else {
	if ((((--i)+1) % ITEMS_PER_SUBMENU) == 1)
	    insert_one_channel(menu, i--, pos);
	menu2 = NULL;
	for (;i>=0;i--) {
	  if (!menu2)
	    {
	      menu2 = GTK_MENU(gtk_menu_new());
	      menu_item = gtk_tearoff_menu_item_new();
	      gtk_widget_show(menu_item);
	      gtk_menu_append(menu2, menu_item);
	      gtk_widget_show(GTK_WIDGET(menu2));
	      menu_item =
		z_gtk_pixmap_menu_item_new("foobar",
					   GNOME_STOCK_PIXMAP_LINE_IN);
	      gtk_widget_show(menu_item);
	      gtk_menu_insert(menu, menu_item, pos);
	      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item),
					GTK_WIDGET(menu2));
	      buf =
		tveng_retrieve_tuned_channel_by_index(i,
				      global_channel_list)->name;
	    }
	  insert_one_channel(menu2, i, 1);
	  if (!(i%ITEMS_PER_SUBMENU))
	    {
	      buf = g_strdup_printf("%s/%s",
			    tveng_retrieve_tuned_channel_by_index(i,
			    global_channel_list)->name, buf);
	      z_change_menuitem(menu_item, NULL, buf, NULL);
	      g_free(buf);
	      menu2 = NULL;
	    }
	}
      }
    }

  return sth;
}

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

  gnome_dialog_set_default (GNOME_DIALOG (dialog), 0 /* ok */);
  
  if (0 /* ok */ != gnome_dialog_run_and_close (GNOME_DIALOG (dialog)))
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

#define CMD_REG(_name) cmd_register (#_name, _name##_cmd, NULL)

void
startup_v4linterface(tveng_device_info *info)
{
  z_input_model = ZMODEL(zmodel_new());

  gtk_signal_connect(GTK_OBJECT(z_input_model), "changed",
		     GTK_SIGNAL_FUNC(update_bundle),
		     info);

  CMD_REG (channel_up);
  CMD_REG (channel_down);
  CMD_REG (set_channel);
  CMD_REG (lookup_channel);

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
  gtk_object_destroy(GTK_OBJECT(z_input_model));
}
