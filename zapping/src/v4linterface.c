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
#include "mixer.h"

struct control_window;

struct control_window *ToolBox = NULL; /* Pointer to the last control box */
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

struct control
{
  struct control *		next;
  tveng_device_info *		info;
  tveng_control *		ctrl;
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

static struct control *
add_control			(struct control_window *cb,
				 tveng_device_info *	info,
				 tveng_control *	ctrl,
				 GtkWidget *		label,
				 GtkWidget *		crank)
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

  *cp = c;

  return c;
}

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

  return symbol;
}

static void
on_control_slider_changed	(GtkAdjustment *	adjust,
				 gpointer		user_data)
{
  struct control *c = user_data;

  tveng_set_control (c->ctrl, (int) adjust->value, c->info);
}

static void
create_slider			(struct control_window *cb,
				 tveng_device_info *	info,
				 tveng_control *	ctrl)
{ 
  GObject *adj; /* Adjustment object for the slider */
  GtkWidget *spinslider;

  adj = G_OBJECT (gtk_adjustment_new (ctrl->cur_value,
				      ctrl->min, ctrl->max,
				      1, 10, 10));

  spinslider = z_spinslider_new (GTK_ADJUSTMENT (adj), NULL,
				 NULL, ctrl->def_value, 0);

  z_spinslider_set_value (spinslider, ctrl->cur_value);

  g_signal_connect (adj, "value-changed",
		    G_CALLBACK (on_control_slider_changed),
		    add_control (cb, info, ctrl, control_symbol (ctrl),
				 spinslider));
}

static void
on_control_checkbutton_toggled	(GtkToggleButton *	tb,
				 gpointer		user_data)
{
  struct control *c = user_data;

  tveng_set_control (c->ctrl, gtk_toggle_button_get_active (tb), c->info);

  /* Update tool button XXX switch to callback */
  set_mute (3, /* controls */ FALSE, /* osd */ FALSE);
}

static void
create_checkbutton		(struct control_window *cb,
				 tveng_device_info *	info,
				 tveng_control *	ctrl)
{
  GtkWidget *check_button;

  check_button = gtk_check_button_new_with_label (_(ctrl->name));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
				ctrl->cur_value);

  g_signal_connect (G_OBJECT (check_button), "toggled",
		    G_CALLBACK (on_control_checkbutton_toggled),
		    add_control (cb, info, ctrl, NULL, check_button));
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
				 tveng_control *	ctrl)
{
  GtkWidget *label; /* This shows what the menu is for */
  GtkWidget *option_menu; /* The option menu */
  GtkWidget *menu; /* The menu displayed */
  GtkWidget *menu_item; /* Each of the menu items */
  struct control *c;
  guint i;

  label = gtk_label_new (_(ctrl->name));
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);

  option_menu = gtk_option_menu_new ();
  menu = gtk_menu_new ();
  gtk_widget_show (menu);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), menu);

  c = add_control (cb, info, ctrl, label, option_menu);

  /* Start querying menu_items and building the menu */
  for (i = 0; ctrl->data[i] != NULL; i++)
    {
      menu_item = gtk_menu_item_new_with_label (_(ctrl->data[i]));
      gtk_widget_show (menu_item);

      g_object_set_data (G_OBJECT (menu_item), "value", 
			 GINT_TO_POINTER (i));

      g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (on_control_menuitem_activate), c);

      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    }

  gtk_option_menu_set_history (GTK_OPTION_MENU (option_menu),
			       ctrl->cur_value);
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
				 tveng_control *	ctrl)
{
  GtkWidget *button;

  button = gtk_button_new_with_label (_(ctrl->name));

  g_signal_connect (G_OBJECT (button), "clicked",
		    G_CALLBACK (on_control_button_clicked),
		    add_control (cb, info, ctrl, NULL, button));
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
  guint color = ((arg1 >> 8) << 16) + ((arg2 >> 8) << 8) + (arg3 >> 8);

  tveng_set_control (c->ctrl, color, c->info);
}

static void
create_color_picker		(struct control_window *cb,
				 tveng_device_info *	info,
				 tveng_control *	ctrl)
{
  GtkWidget *label;
  GnomeColorPicker *color_picker;
  gchar *buffer;

  label = gtk_label_new (_(ctrl->name));
  gtk_misc_set_alignment (GTK_MISC (label), 1.0, 0.5);

  color_picker = GNOME_COLOR_PICKER (gnome_color_picker_new ());
  gnome_color_picker_set_use_alpha (color_picker, FALSE);
  gnome_color_picker_set_i8 (color_picker,
			     (ctrl->cur_value & 0xff0000) >> 16,
			     (ctrl->cur_value & 0xff00) >> 8,
			     (ctrl->cur_value & 0xff),
			     0);

  /* TRANSLATORS: In controls box, color picker, name of the property */
  buffer = g_strdup_printf (_("Adjust %s"), ctrl->name);
  gnome_color_picker_set_title (color_picker, buffer);
  g_free (buffer);

  g_signal_connect (G_OBJECT (color_picker), "color-set",
		    G_CALLBACK (on_color_set),
		    add_control (cb, info, ctrl, label, GTK_WIDGET(color_picker)));
}

static void
add_controls			(struct control_window *cb,
				 tveng_device_info *	info)
{
  tveng_control *ctrl;

  if (cb->hbox)
    {
      struct control *c;

      while ((c = cb->controls))
	{
	  cb->controls = c->next;
	  g_free (c);
	}
      
      gtk_container_remove (GTK_CONTAINER (cb->window), cb->hbox);
    }

  cb->hbox = gtk_hbox_new (FALSE, 0);
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

  for (; cb->index < info->num_controls; cb->index++)
    {
      if ((cb->index % 20) == 0)
	{
	  if (cb->table)
	    {
	      gtk_widget_show (cb->table);
	      gtk_box_pack_start_defaults (GTK_BOX (cb->hbox), cb->table);
	    }

	  cb->table = gtk_table_new (1, 2, FALSE);
	}

      ctrl = info->controls + cb->index;

      g_assert (ctrl != NULL);

      switch (ctrl->type)
	{
	case TVENG_CONTROL_SLIDER:
	  create_slider (cb, info, ctrl);
	  break;

	case TVENG_CONTROL_CHECKBOX:
	  create_checkbutton (cb, info, ctrl);
	  break;

	case TVENG_CONTROL_MENU:
	  create_menu (cb, info, ctrl);
	  break;

	case TVENG_CONTROL_BUTTON:
	  create_button (cb, info, ctrl);
	  break;

	case TVENG_CONTROL_COLOR:
	  create_color_picker (cb, info, ctrl);
	  break;

	default:
	  g_warning ("Type %d of control %s is not supported",
		     ctrl->type, ctrl->name);
	  continue;
	}
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
on_control_window_destroy	(GtkWidget *		widget,
				 gpointer		user_data)
{
  struct control_window *cb = user_data;
  struct control *c;

  ToolBox = NULL;

  while ((c = cb->controls))
    {
      cb->controls = c->next;
      g_free (c);
    }

  g_free (cb);

  /* See below.
     gtk_widget_set_sensitive (lookup_widget (main_window, "controls"), TRUE);
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
     gtk_widget_set_sensitive (lookup_widget (main_window, "controls"), FALSE);
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
  tveng_control *ctrl;
  guint i;

  if (!ToolBox)
    return;

  c = ToolBox->controls;

  for (i = 0; i < info->num_controls; i++)
    {
      ctrl = info->controls + i;

      /* XXX Is this safe? Unlikely. */
      if (!c || c->ctrl != ctrl)
	goto rebuild;

      switch (ctrl->type)
	{
	case TVENG_CONTROL_SLIDER:
	  z_spinslider_set_value (c->widget, ctrl->cur_value);
	  break;

	case TVENG_CONTROL_CHECKBOX:
	  gtk_toggle_button_set_active
	    (GTK_TOGGLE_BUTTON (c->widget), ctrl->cur_value);
	  break;

	case TVENG_CONTROL_MENU:
	  gtk_option_menu_set_history
	    (GTK_OPTION_MENU (c->widget), ctrl->cur_value);
	  break;

	case TVENG_CONTROL_BUTTON:
	  break;

	case TVENG_CONTROL_COLOR:
	  gnome_color_picker_set_i8
	    (GNOME_COLOR_PICKER (c->widget),
	     (ctrl->cur_value & 0xff0000) >> 16,
	     (ctrl->cur_value & 0xff00) >> 8,
	     (ctrl->cur_value & 0xff),
	     0);
	  break;

	default:
	  g_warning ("Type %d of control %s is not supported",
		     ctrl->type, ctrl->name);
	  continue;
	}

      c = c->next;
    }

  return;
  
 rebuild:
  add_controls (ToolBox, info);
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
  gint muted;
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

  muted = -1;

  if (zcg_bool (NULL, "avoid_noise"))
    {
      if (audio_get_mute (&muted))
	{
	  if (muted == FALSE)
	    set_mute (1, /* controls */ FALSE, /* osd */ FALSE);
	}
      else
	{
	  muted = -1;
	}
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

      if (muted == 0) /* was on (and no error) */
	set_mute (0, FALSE, FALSE);
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
    osd_render_markup (NULL,
	_("Channel: <span foreground=\"yellow\">%s</span>"), channel->name);
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
  gint txl = zconf_get_integer (NULL, "/zapping/options/main/channel_txl");

  if (timer
      && (txl >= 0 || kp_lirc) /* txl: -1 disable, 0 list entry, 1 RF channel */
      && kp_chsel_buf[0] != 0)
    kp_enter (txl);

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
	/* TRANSLATORS: Channel name being entered on numeric keypad */
	osd_render_markup (kp_timeout, _("<span foreground=\"green\">%s</span>"),
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
  tveng_tuned_channel *tc;
  z_key key;
  gint txl, i;

  txl = zconf_get_integer (NULL, "/zapping/options/main/channel_txl");

  if (txl >= 0) /* !disabled */
    if (kp_key_press (event, txl))
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
	  (*list)[i].name[31] = 0;
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
