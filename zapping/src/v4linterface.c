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

#include "../site_def.h"

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
  GtkMenuShell *menu;
  GtkWidget *menu_item;

  if (freeze)
    {
      needs_refresh = TRUE;
      return;
    }

  menu = GTK_MENU_SHELL(gtk_menu_new());

  menu_item = gtk_tearoff_menu_item_new();
  gtk_widget_show(menu_item);
  gtk_menu_shell_append(menu, menu_item);

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

/*
 *  Control box
 */

static GtkWidget *
control_symbol			(struct tveng_control *	qc)
{
  static const struct {
    enum tveng_control_property	prop;
    const char *		image;
  } pixmaps[] = {
    { TVENG_CTRL_PROP_BRIGHTNESS,	"brightness.png" },
    { TVENG_CTRL_PROP_CONTRAST,		"contrast.png" },
    { TVENG_CTRL_PROP_SATURATION,	"saturation.png" },
    { TVENG_CTRL_PROP_HUE,		"hue.png" },
  };
  GtkWidget *symbol;
  guint i;

  symbol = NULL;

  for (i = 0; i < sizeof(pixmaps) / sizeof(*pixmaps); i++)
    if (qc->property == pixmaps[i].prop)
      {
	symbol = z_load_pixmap (pixmaps[i].image);
  gtk_misc_set_alignment (GTK_MISC (symbol), 1.0, 0.5);
	symbol = z_tooltip_set_wrap (symbol, _(qc->name));
	break;
      }

  if (!symbol)
    {
      symbol = gtk_label_new (_(qc->name));
      gtk_widget_show (symbol);
  gtk_misc_set_alignment (GTK_MISC (symbol), 1.0, 0.5);
    }

  if (!GTK_IS_MISC (symbol))
    {
      
    }


  return symbol;
}

static void
on_control_slider_changed	(GtkAdjustment *	adjust,
				 gpointer		user_data)
{
  /* Control id */
  gint cid = GPOINTER_TO_INT (user_data);
  tveng_device_info *info =
    (tveng_device_info *) g_object_get_data (G_OBJECT (adjust), "info");

  g_assert (info != NULL);
  g_assert (cid < info->num_controls);

  tveng_set_control (&info->controls[cid], (int) adjust->value, info);
}

static void
create_slider			(GtkWidget *		table,
				 struct tveng_control *	qc,
				 int			index,
				 tveng_device_info *	info)
{ 
  GtkWidget *spinslider;
  GObject *adj; /* Adjustment object for the slider */

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);

  gtk_table_attach (GTK_TABLE (table),
		    control_symbol (qc),
		    0, 1, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);

  adj = G_OBJECT (gtk_adjustment_new (qc->cur_value,
				      qc->min, qc->max,
				      1, 10, 10));
  g_object_set_data (adj, "info", (gpointer) info);
  g_signal_connect (adj, "value-changed", 
		    G_CALLBACK (on_control_slider_changed),
		    GINT_TO_POINTER (index));

  spinslider = z_spinslider_new (GTK_ADJUSTMENT (adj), NULL,
				 NULL, qc->def_value, 0);

  z_spinslider_set_value (spinslider, qc->cur_value);

  gtk_widget_show (spinslider);

  gtk_table_attach (GTK_TABLE (table), spinslider,
		    1, 3, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions) (0), 3, 3);
}

static void
on_control_checkbutton_toggled	(GtkToggleButton *	tb,
				 gpointer		user_data)
{
  gint cid = GPOINTER_TO_INT (user_data);
  tveng_device_info *info =
    (tveng_device_info *) g_object_get_data (G_OBJECT (tb), "info");

  g_assert (info != NULL);
  g_assert (cid < info->num_controls);

  tveng_set_control (&(info->controls[cid]),
		     gtk_toggle_button_get_active (tb),
		     info);

  set_mute1 (3, FALSE, FALSE); /* update */
}

static void
create_checkbutton		(GtkWidget *		table,
				 struct tveng_control *	qc,
				 int			index,
				 tveng_device_info *	info)
{
  GtkWidget *cb;

  cb = gtk_check_button_new_with_label (_(qc->name));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb), qc->cur_value);

  g_object_set_data (G_OBJECT (cb), "info", (gpointer) info);

  g_signal_connect (G_OBJECT (cb), "toggled",
		    G_CALLBACK (on_control_checkbutton_toggled),
		    GINT_TO_POINTER (index));

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (table), cb,
		    1, 3, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions) (0), 3, 3);
}

static void
on_control_menuitem_activate	(GtkMenuItem *		menuitem,
				 gpointer		user_data)
{
  gint cid = GPOINTER_TO_INT (user_data);
  tveng_device_info *info;
  int value;

  info = (tveng_device_info *) g_object_get_data (G_OBJECT (menuitem), "info");
  value = (int) g_object_get_data (G_OBJECT (menuitem), "value");

  g_assert (info != NULL);
  g_assert (cid < info->num_controls);

  tveng_set_control (&(info->controls[cid]), value, info);
}

static void
create_menu			(GtkWidget *		table,
				 struct tveng_control *	qc,
				 int			index,
				 tveng_device_info *	info)
{
  GtkWidget *option_menu; /* The option menu */
  GtkWidget *menu; /* The menu displayed */
  GtkWidget *menu_item; /* Each of the menu items */
  GtkWidget *label; /* This shows what the menu is for */
  guint i;

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);

  label = gtk_label_new (_(qc->name));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);

  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);

  option_menu = gtk_option_menu_new ();
  menu = gtk_menu_new ();
  
  /* Start querying menu_items and building the menu */
  for (i = 0; qc->data[i] != NULL; i++)
    {
      menu_item = gtk_menu_item_new_with_label (_(qc->data[i]));

      g_object_set_data (G_OBJECT (menu_item), "info", (gpointer) info);
      g_object_set_data (G_OBJECT (menu_item), "value", 
			 GINT_TO_POINTER (i));
      g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (on_control_menuitem_activate),
			GINT_TO_POINTER (index)); /* it should know about
						     itself */
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);
  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu),
			       qc->cur_value);
  gtk_widget_show (menu);
  gtk_widget_show (option_menu);

  gtk_table_attach (GTK_TABLE (table), option_menu,
		    1, 3, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions) (0), 3, 3);
}

static void
on_control_button_clicked	(GtkButton *		button,
				 gpointer		user_data)
{
  gint cid = GPOINTER_TO_INT (user_data);
  tveng_device_info *info =
    (tveng_device_info*) g_object_get_data (G_OBJECT (button), "info");

  g_assert (info != NULL);
  g_assert (cid < info->num_controls);

  tveng_set_control (&(info->controls[cid]), 1, info);
}

static void
create_button			(GtkWidget *		table,
				 struct tveng_control *	qc,
				 int			index,
				 tveng_device_info *	info)
{
  GtkWidget *button;

  button = gtk_button_new_with_label (_(qc->name));
  gtk_widget_show (button);

  g_object_set_data (G_OBJECT (button), "info", (gpointer) info);
  g_signal_connect (G_OBJECT (button), "clicked",
		    G_CALLBACK (on_control_button_clicked),
		    GINT_TO_POINTER (index));

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);
  gtk_table_attach (GTK_TABLE (table), button,
		    1, 3, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions) (0), 3, 3);
}

static void
on_color_set			(GnomeColorPicker *	colorpicker,
				 guint			arg1,
				 guint			arg2,
				 guint			arg3,
				 guint			arg4,
				 gpointer		user_data)
{
  gint cid = GPOINTER_TO_INT (user_data);
  gint color = ((arg1 >> 8) << 16) + ((arg2 >> 8) << 8) + (arg3 >> 8);
  tveng_device_info *info =
    (tveng_device_info *) g_object_get_data (G_OBJECT (colorpicker), "info");

  g_assert (info != NULL);
  g_assert (cid < info->num_controls);

  tveng_set_control (&(info->controls[cid]), color, info);
}

static void
create_color_picker		(GtkWidget *		table,
				 struct tveng_control *	qc,
				 int			index,
				 tveng_device_info *	info)
{
  GnomeColorPicker *color_picker;
  gchar *buffer;
  GtkWidget *label;

  gtk_table_resize (GTK_TABLE (table), index + 1, 2);

  label = gtk_label_new (_(qc->name));
  gtk_widget_show (label);
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);
  gtk_table_attach (GTK_TABLE (table), label,
		    0, 1, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 3, 3);

  color_picker = GNOME_COLOR_PICKER (gnome_color_picker_new ());
  gnome_color_picker_set_use_alpha (color_picker, FALSE);
  gnome_color_picker_set_i8 (color_picker,
			     (qc->cur_value & 0xff0000) >> 16,
			     (qc->cur_value & 0xff00) >> 8,
			     (qc->cur_value & 0xff),
			     0);
  /* TRANSLATORS: Video controls, color picker, name of property */
  buffer = g_strdup_printf (_("Adjust %s"), qc->name);
  gnome_color_picker_set_title (color_picker, buffer);
  g_object_set_data (G_OBJECT (color_picker), "info", (gpointer) info);
  gtk_widget_show (GTK_WIDGET (color_picker));
  g_free (buffer);

  g_signal_connect (G_OBJECT (color_picker), "color-set",
		    G_CALLBACK (on_color_set),
		    GINT_TO_POINTER (index));

  gtk_table_attach (GTK_TABLE (table), GTK_WIDGET(color_picker),
		    1, 3, index, index + 1,
                    (GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
                    (GtkAttachOptions) (0), 3, 3);
}

static void
build_control_box		(GtkWidget *		hbox,
				 tveng_device_info *	info)
{
  struct tveng_control *control;
  GtkWidget *table = NULL;
  guint i = 0;

  /* Update the values of all the controls */
  if (-1 == tveng_update_controls (info))
    {
      ShowBox ("Tveng critical error, Zapping will exit NOW.",
	       GTK_MESSAGE_ERROR);
      g_error ("tveng critical: %s", info->error);
    }

  for (i = 0; i < info->num_controls; i++)
    {
      if ((i % 20) == 0)
	{
	  if (table)
	    {
	      gtk_widget_show (table);
	      gtk_box_pack_start_defaults (GTK_BOX (hbox), table);
	    }

	  table = gtk_table_new (1, 3, FALSE);
	}

      control = &(info->controls[i]);

      g_assert (control != NULL);

      switch (control->type)
	{
	case TVENG_CONTROL_SLIDER:
	  create_slider (table, control, i, info);
	  break;

	case TVENG_CONTROL_CHECKBOX:
	  create_checkbutton (table, control, i, info);
	  break;

	case TVENG_CONTROL_MENU:
	  create_menu (table, control, i, info);
	  break;

	case TVENG_CONTROL_BUTTON:
	  create_button (table, control, i, info);
	  break;

	case TVENG_CONTROL_COLOR:
	  create_color_picker (table, control, i, info);
	  break;

	default:
	  g_warning ("Type %d of control %s is not supported",
		     control->type, control->name);
	  continue;
	}
    }

  if (table)
    {
      gtk_widget_show (table);
      gtk_box_pack_start_defaults (GTK_BOX (hbox), table);
    }
}

static gboolean
on_control_box_key_press	(GtkWidget *		widget,
				 GdkEventKey *		event,
				 gpointer		user_data)
{
  switch (event->keyval)
    {
    case GDK_Escape:
      gtk_widget_destroy (widget);
      return TRUE;

    case GDK_c:
    case GDK_C:
      if (event->state & GDK_CONTROL_MASK)
	{
	  gtk_widget_destroy (widget);
	  return TRUE;
	}

    default:
      break;
    }

  return on_user_key_press (widget, event, user_data)
    || on_channel_key_press (widget, event, user_data);
}

static void
on_control_box_destroy		(GtkWidget *		widget,
				 gpointer		user_data)
{
  GtkWidget * related_button;

  related_button =
    GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "user-data"));

  gtk_widget_set_sensitive(related_button, TRUE);

  ToolBox = NULL;
}

static PyObject *
py_control_box			(PyObject *		self,
				 PyObject *		args)
{
  GtkWidget * control_box;
  GtkWidget * hbox;
  GtkWidget * related_button = lookup_widget (main_window, "controls");

  g_assert (ToolBox == NULL);

  control_box = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(control_box), _("Controls"));

  hbox = gtk_hbox_new(FALSE, 0);

  build_control_box(hbox, main_info);

  gtk_widget_show(hbox);
  gtk_container_add(GTK_CONTAINER (control_box), hbox);

  g_object_set_data(G_OBJECT(control_box), "hbox", hbox);
  g_signal_connect(G_OBJECT(control_box), "destroy",
		     G_CALLBACK(on_control_box_destroy),
		     NULL);

  g_signal_connect(G_OBJECT(control_box), "key-press-event",
		     G_CALLBACK(on_control_box_key_press),
		     NULL);

  gtk_widget_set_sensitive (related_button, FALSE);
  g_object_set_data (G_OBJECT (control_box), "user-data",
		     related_button);

  ToolBox = control_box;

  gtk_widget_show (control_box);

  py_return_none;
}

void
update_control_box		(tveng_device_info *	info)
{
  GtkWidget * hbox;
  GtkWidget * control_box = ToolBox;

  if ((!info) || (!control_box))
    return;

  hbox = GTK_WIDGET(g_object_get_data(G_OBJECT(control_box),
					"hbox"));
  gtk_container_remove(GTK_CONTAINER (control_box), hbox);

  hbox = gtk_hbox_new(FALSE, 0);

  build_control_box(hbox, info);

  gtk_widget_show(hbox);
  gtk_container_add(GTK_CONTAINER (control_box), hbox);
  g_object_set_data(G_OBJECT(control_box), "hbox", hbox);
}



void
z_switch_input			(int hash, tveng_device_info *info)
{
  struct tveng_enum_input *input =
    tveng_find_input_by_hash(hash, info);

  if (!input)
    {
      ShowBox("Couldn't find input with hash %x",
	      GTK_MESSAGE_ERROR, hash);
      return;
    }

  if (input->index == info->cur_input)
    return;

  if (tveng_set_input(input, info) == -1)
    ShowBox("Couldn't switch to input %s\n%s",
	    GTK_MESSAGE_ERROR,
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
		GTK_MESSAGE_ERROR, hash);
      return;
    }

  if (standard->index == info->cur_standard)
    return;

  if (tveng_set_standard(standard, info) == -1)
    ShowBox("Couldn't switch to standard %s\n%s",
	    GTK_MESSAGE_ERROR,
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
      ShowBox(info -> error, GTK_MESSAGE_ERROR);

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

  /* IMHO it's wrong to start at 0, but compatibility rules. */
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

static gchar			kp_chsel_buf[5];
static gint			kp_chsel_prefix;
static gboolean			kp_clear;

static void
kp_timeout				(gboolean timer)
{
  gint txl = zconf_get_integer (NULL, "/zapping/options/main/channel_txl");

  if (timer && txl >= 0) /* -1 disable, 0 list entry, 1 RF channel */
    {
      if (!isdigit (kp_chsel_buf[0]) || txl >= 1)
	cmd_run_printf ("zapping.lookup_channel('%s')", kp_chsel_buf);
      else
	cmd_run_printf ("zapping.set_channel(%d)", kp_chsel_buf);
    }

  if (kp_clear)
    {
      kp_chsel_buf[0] = 0;
      kp_chsel_prefix = 0;
    }
}

gboolean
on_channel_key_press			(GtkWidget *	widget,
					 GdkEventKey *	event,
					 gpointer	user_data)
{
  extern tveng_rf_table *current_country; /* Currently selected contry */
  tveng_tuned_channel *tc;
  const gchar *prefix;
  z_key key;
  int txl, i;

  txl = zconf_get_integer (NULL, "/zapping/options/main/channel_txl");

  if (txl >= 0) /* !disabled */
    switch (event->keyval)
      {
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
	  cmd_run_printf ("zapping.lookup_channel('%s')", kp_chsel_buf);
	else
	  cmd_run_printf ("zapping.set_channel(%d)", kp_chsel_buf);

	kp_chsel_buf[0] = 0;
	kp_chsel_prefix = 0;

	return TRUE;

      default:
	break;
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
insert_one_channel		(GtkMenuShell *		menu,
				 guint			index,
				 guint			pos)
{
  tveng_tuned_channel *tc;
  GtkWidget *menu_item;
  gchar *tooltip;

  if (!(tc = tveng_tuned_channel_nth (global_channel_list, index)))
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
tuned_channel_nth_name		(tveng_tuned_channel *	list,
				 guint			index)
{
  tveng_tuned_channel *tc;

  tc = tveng_tuned_channel_nth (list, index);

  g_assert (tc != NULL);

  return tc->name ? tc->name : _("Unnamed");
}

/* Returns whether something (useful) was added */
gboolean
add_channel_entries			(GtkMenuShell *menu,
					 gint pos,
					 gint menu_max_entries,
					 tveng_device_info *info)
{
  gboolean sth = FALSE;
  guint num_channels;

  num_channels = tveng_tuned_channel_num (global_channel_list);

  if (info->num_standards)
    {
      GtkMenuShell *submenu;
      GtkWidget *menu_item;
      guint i;

      menu_item = z_gtk_pixmap_menu_item_new ("Standards",
					      GTK_STOCK_SELECT_COLOR);
      gtk_widget_show (menu_item);
      submenu = GTK_MENU_SHELL (gtk_menu_new());
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item),
				 GTK_WIDGET (submenu));
      gtk_menu_shell_insert (menu, menu_item, pos);

      menu_item = gtk_tearoff_menu_item_new ();
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (submenu, menu_item);

      for (i = 0; i < info->num_standards; i++)
	{
	  menu_item = z_gtk_pixmap_menu_item_new (info->standards[i].name,
						  GTK_STOCK_SELECT_COLOR);
	  gtk_widget_show (menu_item);
	  gtk_menu_shell_append (submenu, menu_item);
	  g_signal_connect (G_OBJECT (menu_item), "activate",
			    G_CALLBACK (on_standard_activate),
			    GINT_TO_POINTER (info->standards[i].hash));
	}
    }

  if (info->num_inputs)
    {
      GtkMenuShell *submenu;
      GtkWidget *menu_item;
      guint i;

      menu_item = z_gtk_pixmap_menu_item_new (_("Inputs"),
					      "gnome-stock-line-in");
      gtk_widget_show (menu_item);
      submenu = GTK_MENU_SHELL (gtk_menu_new ());
      gtk_menu_item_set_submenu (GTK_MENU_ITEM (menu_item),
				 GTK_WIDGET (submenu));
      gtk_menu_shell_insert (menu, menu_item, pos);

      menu_item = gtk_tearoff_menu_item_new ();
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (submenu, menu_item);

      for (i = 0; i < info->num_inputs; i++)
	{
	  menu_item = z_gtk_pixmap_menu_item_new (info->inputs[i].name,
						  "gnome-stock-line-in");
	  gtk_widget_show (menu_item);
	  gtk_menu_shell_append (submenu, menu_item);
	  g_signal_connect (G_OBJECT (menu_item), "activate",
			    G_CALLBACK (on_input_activate),
			    GINT_TO_POINTER (info->inputs[i].hash));
	}
    }

  if ((info->num_standards > 0 || info->num_inputs > 0) && num_channels > 0)
    {
      GtkWidget *menu_item;

      /* Separator */

      menu_item = gtk_menu_item_new ();
      gtk_widget_show (menu_item);
      gtk_menu_shell_insert (menu, menu_item, pos);
    }

#define ITEMS_PER_SUBMENU 20

  if (num_channels == 0)
    {
      GtkWidget *menu_item;

      /* This doesn't count as something added */

      menu_item = z_gtk_pixmap_menu_item_new (_("No tuned channels"),
					      GTK_STOCK_CLOSE);
      gtk_widget_set_sensitive (menu_item, FALSE);
      gtk_widget_show (menu_item);
      gtk_menu_shell_insert (menu, menu_item, pos);
    }
  else
    {
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

		  first_name = tuned_channel_nth_name
		    (global_channel_list, num_channels - remainder);
		  last_name = tuned_channel_nth_name
		    (global_channel_list, num_channels - 1);
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

void
startup_v4linterface(tveng_device_info *info)
{
  z_input_model = ZMODEL(zmodel_new());

  g_signal_connect(G_OBJECT(z_input_model), "changed",
		     G_CALLBACK(update_bundle),
		     info);

  cmd_register ("channel_up", py_channel_up, METH_VARARGS,
		_("Switches to the next channel"),
		"zapping.channel_up()");
  cmd_register ("channel_down", py_channel_down, METH_VARARGS,
		_("Switches to the previous channel"),
		"zapping.channel_down()");
  cmd_register ("set_channel", py_set_channel, METH_VARARGS,
		_("Switches to a channel given its index"),
		"zapping.set_channel(5)");
  cmd_register ("lookup_channel", py_lookup_channel, METH_VARARGS,
		_("Switches to a channel given its name"),
		"zapping.lookup_channel('Linux TV')");
  cmd_register ("control_box", py_control_box, METH_VARARGS,
		_("Opens the control box"), "zapping.control_box()");

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
