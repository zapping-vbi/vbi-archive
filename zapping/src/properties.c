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
/*
  Handles the property dialog. This was previously in callbacks.c, but
  it was getting too big, so I moved the code here.
*/
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

/* Manages config values for zconf (it saves me some typing) */
#define ZCONF_DOMAIN "/zapping/internal/callbacks/"

#include "tveng.h"
#include "callbacks.h"
#include "interface.h"
#include "v4linterface.h"
#include "properties.h"
#include "zconf.h"
#include "zvbi.h"
#include "zmisc.h"
#include "osd.h"

extern tveng_device_info * main_info; /* About the device we are using */

static property_handler *handlers = NULL;
static gint num_handlers = 0;

static void
update_sensitivity(GtkWidget *widget)
{
  if (lookup_widget(widget, "checkbutton6") == widget)
    {
      gboolean active =
	gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
      gtk_widget_set_sensitive(lookup_widget(widget,
					     "vbox19"), active);      
      gtk_widget_set_sensitive(lookup_widget(widget,
					     "vbox33"), active);      
    }
  else if (gtk_option_menu_get_menu(GTK_OPTION_MENU(lookup_widget(widget,
			    "optionmenu22"))) == widget)
    {
      widget = lookup_widget(widget, "optionmenu22");
      gtk_widget_set_sensitive(lookup_widget(widget, "vbox38"),
			       !z_option_menu_get_active(widget));
    }
}

static void
font_set_bridge	(GtkWidget	*widget,
		 const gchar	*new_font,
		 GnomePropertyBox *box)
{
  on_property_item_changed(widget, box);
}

static void
color_set_bridge (GtkWidget	*widget,
		  guint		r,
		  guint		g,
		  guint		b,
		  guint		a,
		  GnomePropertyBox *box)
{
  on_property_item_changed(widget, box);
}

void
on_propiedades1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget * zapping_properties = create_zapping_properties();

  GtkNotebook * nb;
  GtkWidget * nb_label;
  GtkWidget * nb_body;
  guint32 i=0;

  gchar * buffer; /* Some temporary buffer */

  /* Widget for assigning the callbacks (generic) */
  GtkWidget * widget;

  /* Connect the widgets to the apropiate callbacks, so the Apply
     button works correctly. Set the correct values too */

  /* The device name */
  widget = lookup_widget(zapping_properties, "label27");
  gtk_label_set_text(GTK_LABEL(widget), main_info->caps.name);

  /* Minimum capture dimensions */
  widget = lookup_widget(zapping_properties, "label28");
  buffer = g_strdup_printf("%d x %d", main_info->caps.minwidth,
			   main_info->caps.minheight);
  gtk_label_set_text(GTK_LABEL(widget), buffer);
  g_free(buffer);

  /* Maximum capture dimensions */
  widget = lookup_widget(zapping_properties, "label29");
  buffer = g_strdup_printf("%d x %d", main_info->caps.maxwidth,
			   main_info->caps.maxheight);
  gtk_label_set_text(GTK_LABEL(widget), buffer);
  g_free(buffer);

  /* Reported device capabilities */
  widget = lookup_widget(zapping_properties, "label30");
  buffer = g_strdup_printf("%s%s%s%s%s%s%s%s%s%s",
			   main_info->caps.flags & TVENG_CAPS_CAPTURE
			   ? _("Can capture to memory.\n") : "",
			   main_info->caps.flags & TVENG_CAPS_TUNER
			   ? _("Has some tuner.\n") : "",
			   main_info->caps.flags & TVENG_CAPS_TELETEXT
			   ? _("Supports the teletext service.\n") : "",
			   main_info->caps.flags & TVENG_CAPS_OVERLAY
			   ? _("Can overlay the image.\n") : "",
			   main_info->caps.flags & TVENG_CAPS_CHROMAKEY
			   ? _("Can chromakey the image.\n") : "",
			   main_info->caps.flags & TVENG_CAPS_CLIPPING
			   ? _("Clipping rectangles are supported.\n") : "",
			   main_info->caps.flags & TVENG_CAPS_FRAMERAM
			   ? _("Framebuffer memory is overwritten.\n") : "",
			   main_info->caps.flags & TVENG_CAPS_SCALES
			   ? _("The capture can be scaled.\n") : "",
			   main_info->caps.flags & TVENG_CAPS_MONOCHROME
			   ? _("Only monochrome is available\n") : "",
			   main_info->caps.flags & TVENG_CAPS_SUBCAPTURE
			   ? _("The capture can be zoomed\n") : "");
  /* Delete the last '\n' to save some space */
  if ((strlen(buffer) > 0) && (buffer[strlen(buffer)-1] == '\n'))
    buffer[strlen(buffer)-1] = 0;

  gtk_label_set_text(GTK_LABEL(widget), buffer);
  g_free(buffer);

  nb = GTK_NOTEBOOK (lookup_widget(zapping_properties, "notebook2"));
  if (main_info -> num_inputs == 0)
    {
      nb_label = gtk_label_new(_("No available inputs"));
      gtk_widget_show (nb_label);
      nb_body = gtk_label_new(_("Your video device has no inputs"));
      gtk_widget_show(nb_body);
      gtk_notebook_append_page(nb, nb_body, nb_label);
      gtk_widget_set_sensitive(GTK_WIDGET(nb), FALSE);
   }
  else
    for (i = 0; i < main_info->num_inputs; i++)
      {
	nb_label = gtk_label_new(main_info->inputs[i].name);
	gtk_widget_show (nb_label);
	switch (main_info->inputs[i].tuners)
	  {
	  case 0:
	    buffer = 
	      g_strdup_printf(_("%s"),
			      main_info->inputs[i].type ==
			      TVENG_INPUT_TYPE_TV ? _("TV input") :
			      _("Camera"));
	      break;
	  case 1:
	    buffer =
	      g_strdup_printf(_("%s with a tuner"),
			      main_info->inputs[i].type ==
			      TVENG_INPUT_TYPE_TV ? _("TV input") :
			      _("Camera"));
	    break;
	  default:
	    buffer =
	      g_strdup_printf(_("%s with %d tuners"),
			      main_info->inputs[i].type ==
			      TVENG_INPUT_TYPE_TV ? _("TV input") :
			      _("Camera"),
			      main_info->inputs[i].tuners);
	    break;
	  }
	nb_body = gtk_label_new (buffer);
	g_free (buffer);
	gtk_widget_show (nb_body);
	gtk_notebook_append_page(nb, nb_body, nb_label);
      }
  
  widget = lookup_widget(zapping_properties, "fileentry1");
  widget = gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY(widget));
  gtk_entry_set_text(GTK_ENTRY(widget),
		     zconf_get_string(NULL,
				      "/zapping/options/main/video_device"));

  gtk_signal_connect(GTK_OBJECT(widget), "changed",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* Current controller */
  widget = lookup_widget(zapping_properties, "label31");
  tveng_describe_controller(NULL, &buffer, main_info);
  gtk_label_set_text(GTK_LABEL(widget), buffer);

  /* Avoid noise while changing channels */
  widget = lookup_widget(zapping_properties, "checkbutton1");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/avoid_noise"));

  gtk_signal_connect(GTK_OBJECT(widget), "toggled",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* Save the geometry through sessions */
  widget = lookup_widget(zapping_properties, "checkbutton2");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/keep_geometry"));

  gtk_signal_connect(GTK_OBJECT(widget), "toggled",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* Resize using fixed increments */
  widget = lookup_widget(zapping_properties, "checkbutton4");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/fixed_increments"));

  gtk_signal_connect(GTK_OBJECT(widget), "toggled",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* Swap Page Up/Down */
  widget = lookup_widget(zapping_properties, "checkbutton13");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/swap_up_down"));

  gtk_signal_connect(GTK_OBJECT(widget), "toggled",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* Start zapping muted */
  widget = lookup_widget(zapping_properties, "checkbutton3");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/start_muted"));

  gtk_signal_connect(GTK_OBJECT(widget), "toggled",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* Hide the mouse pointer */
  widget = lookup_widget(zapping_properties, "checkbutton14");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/hide_pointer"));

  gtk_signal_connect(GTK_OBJECT(widget), "toggled",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* Avoid some flicker in preview mode */
  widget = lookup_widget(zapping_properties, "checkbutton5");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/avoid_flicker"));

  gtk_signal_connect(GTK_OBJECT(widget), "toggled",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* Save control info with the channel */
  widget = lookup_widget(zapping_properties, "checkbutton11");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/save_controls"));

  gtk_signal_connect(GTK_OBJECT(widget), "toggled",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* Title format Z will use */
  widget = lookup_widget(zapping_properties, "title_format");
  widget = gnome_entry_gtk_entry(GNOME_ENTRY(widget));
  gtk_entry_set_text(GTK_ENTRY(widget),
		     zconf_get_string(NULL,
				      "/zapping/options/main/title_format"));

  gtk_signal_connect(GTK_OBJECT(widget), "changed",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* Verbosity value passed to zapping_setup_fb */
  widget = lookup_widget(zapping_properties, "spinbutton1");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),
     zconf_get_integer(NULL,
		       "/zapping/options/main/zapping_setup_fb_verbosity"));

  gtk_signal_connect(GTK_OBJECT(widget), "changed",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* ratio mode to use */
  widget = lookup_widget(zapping_properties, "optionmenu1");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/main/ratio"));

  gtk_signal_connect(GTK_OBJECT(GTK_OPTION_MENU(widget)->menu), "deactivate",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* fullscreen video mode */
  widget = lookup_widget(zapping_properties, "optionmenu2");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/main/change_mode"));

  gtk_signal_connect(GTK_OBJECT(GTK_OPTION_MENU(widget)->menu), "deactivate",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* capture size under XVideo */
  widget = lookup_widget(zapping_properties, "optionmenu20");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/capture/xvsize"));

  gtk_signal_connect(GTK_OBJECT(GTK_OPTION_MENU(widget)->menu), "deactivate",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* OSD type */
  widget = lookup_widget(zapping_properties, "optionmenu22");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/osd/osd_type"));

  gtk_widget_set_sensitive(lookup_widget(widget, "vbox38"),
	   !zconf_get_integer(NULL, "/zapping/options/osd/osd_type"));

  gtk_signal_connect(GTK_OBJECT(GTK_OPTION_MENU(widget)->menu), "deactivate",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* OSD font */
  widget = lookup_widget(zapping_properties, "fontpicker1");
  if (zconf_get_string(NULL, "/zapping/options/osd/font"))
    gnome_font_picker_set_font_name(GNOME_FONT_PICKER(widget),
    zconf_get_string(NULL, "/zapping/options/osd/font"));

  gtk_signal_connect(GTK_OBJECT(widget), "font-set",
		     GTK_SIGNAL_FUNC(font_set_bridge),
		     zapping_properties);

  /* OSD foreground color */
  widget = lookup_widget(zapping_properties, "colorpicker1");
  gnome_color_picker_set_d(GNOME_COLOR_PICKER(widget),
		   zconf_get_float(NULL, "/zapping/options/osd/fg_r"),
		   zconf_get_float(NULL, "/zapping/options/osd/fg_g"),
		   zconf_get_float(NULL, "/zapping/options/osd/fg_b"),
		   0);

  gtk_signal_connect(GTK_OBJECT(widget), "color-set",
		     GTK_SIGNAL_FUNC(color_set_bridge),
		     zapping_properties);

  /* OSD background color */
  widget = lookup_widget(zapping_properties, "colorpicker2");
  gnome_color_picker_set_d(GNOME_COLOR_PICKER(widget),
		   zconf_get_float(NULL, "/zapping/options/osd/bg_r"),
		   zconf_get_float(NULL, "/zapping/options/osd/bg_g"),
		   zconf_get_float(NULL, "/zapping/options/osd/bg_b"),
		   0);

  gtk_signal_connect(GTK_OBJECT(widget), "color-set",
		     GTK_SIGNAL_FUNC(color_set_bridge),
		     zapping_properties);

  /* OSD timeout in seconds */
  widget = lookup_widget(zapping_properties, "spinbutton2");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),
     zconf_get_float(NULL,
		       "/zapping/options/osd/timeout"));

  gtk_signal_connect(GTK_OBJECT(widget), "changed",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* Enable VBI decoding */
  widget = lookup_widget(zapping_properties, "checkbutton6");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/vbi/enable_vbi"));

  update_sensitivity(widget);

  gtk_signal_connect(GTK_OBJECT(widget), "toggled",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* use VBI for getting station names */
  widget = lookup_widget(zapping_properties, "checkbutton7");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/vbi/use_vbi"));

  gtk_signal_connect(GTK_OBJECT(widget), "toggled",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* overlay subtitle pages automagically */
  widget = lookup_widget(zapping_properties, "checkbutton12");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/vbi/auto_overlay"));

  gtk_signal_connect(GTK_OBJECT(widget), "toggled",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);  

  /* VBI device */
  widget = lookup_widget(zapping_properties, "fileentry2");
  widget = gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY(widget));
  gtk_entry_set_text(GTK_ENTRY(widget),
		     zconf_get_string(NULL,
				      "/zapping/options/vbi/vbi_device"));

  gtk_signal_connect(GTK_OBJECT(widget), "changed",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* Default region */
  widget = lookup_widget(zapping_properties, "optionmenu3");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/default_region"));

  gtk_signal_connect(GTK_OBJECT(GTK_OPTION_MENU(widget)->menu), "deactivate",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* Teletext level */
  widget = lookup_widget(zapping_properties, "optionmenu4");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/teletext_level"));

  gtk_signal_connect(GTK_OBJECT(GTK_OPTION_MENU(widget)->menu), "deactivate",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* Quality/speed tradeoff */
  widget = lookup_widget(zapping_properties, "optionmenu21");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/qstradeoff"));

  gtk_signal_connect(GTK_OBJECT(GTK_OPTION_MENU(widget)->menu), "deactivate",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* Default subtitle page */
  widget = lookup_widget(zapping_properties, "subtitle_page");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),
			    bcd2dec(zcg_int(NULL, "zvbi_page")));

  gtk_signal_connect(GTK_OBJECT(widget), "changed",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* The various itv filters */
  widget = lookup_widget(zapping_properties, "optionmenu12");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/pr_trigger"));
  gtk_signal_connect(GTK_OBJECT(GTK_OPTION_MENU(widget)->menu), "deactivate",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  widget = lookup_widget(zapping_properties, "optionmenu16");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/nw_trigger"));
  gtk_signal_connect(GTK_OBJECT(GTK_OPTION_MENU(widget)->menu), "deactivate",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  widget = lookup_widget(zapping_properties, "optionmenu17");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/st_trigger"));
  gtk_signal_connect(GTK_OBJECT(GTK_OPTION_MENU(widget)->menu), "deactivate",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  widget = lookup_widget(zapping_properties, "optionmenu18");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/sp_trigger"));
  gtk_signal_connect(GTK_OBJECT(GTK_OPTION_MENU(widget)->menu), "deactivate",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  widget = lookup_widget(zapping_properties, "optionmenu19");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/op_trigger"));
  gtk_signal_connect(GTK_OBJECT(GTK_OPTION_MENU(widget)->menu), "deactivate",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  widget = lookup_widget(zapping_properties, "optionmenu6");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/trigger_default"));
  gtk_signal_connect(GTK_OBJECT(GTK_OPTION_MENU(widget)->menu), "deactivate",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* Filter level */
  widget = lookup_widget(zapping_properties, "optionmenu5");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/filter_level"));
  gtk_signal_connect(GTK_OBJECT(GTK_OPTION_MENU(widget)->menu), "deactivate",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* Let other modules build their config */
  for (i=0; i<num_handlers; i++)
    handlers[i].add(GNOME_PROPERTY_BOX(zapping_properties));

  /* Make sure there can be just one properties dialog open */
  gtk_widget_set_sensitive(GTK_WIDGET(menuitem), FALSE);
  gtk_signal_connect_object(GTK_OBJECT(zapping_properties), "destroy",
			    GTK_SIGNAL_FUNC(gtk_widget_set_sensitive),
			    GTK_OBJECT(menuitem));

  gtk_widget_show(zapping_properties);
}


void
on_zapping_properties_apply            (GnomePropertyBox *gnomepropertybox,
                                        gint             arg1,
                                        gpointer         user_data)
{
  GtkWidget * widget; /* Generic widget */
  GtkWidget * pbox = GTK_WIDGET(gnomepropertybox); /* Very long name */
  gchar * text; /* Pointer to returned text */
  gint index, i;
  gdouble r, g, b, a;

  static int region_mapping[8] = {
    0, /* WCE */
    8, /* EE */
    16, /* WET */
    24, /* CSE */
    32, /* C */
    48, /* GC */
    64, /* A */
    80 /* I */
  };

  /* Apply just the given page */
  switch (arg1)
    {
    case -1:
      break; /* End of the calls */
    case 0:
      widget = lookup_widget(pbox, "fileentry1"); /* Video device entry
						    */
      text = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY(widget),
					     TRUE);
      if (text)
	zconf_set_string(text, "/zapping/options/main/video_device");

      g_free(text); /* In the docs it says this should be freed */
      break;
    case 1:
      widget = lookup_widget(pbox, "checkbutton1"); /* avoid noise */
      zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/avoid_noise");

      widget = lookup_widget(pbox, "checkbutton2"); /* keep geometry */
      zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)),
			"/zapping/options/main/keep_geometry");

      widget = lookup_widget(pbox, "checkbutton4"); /* fixed increments */
      zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/fixed_increments");

      widget = lookup_widget(pbox, "checkbutton13"); /* swap chan up/down */
      zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/swap_up_down");

      widget = lookup_widget(pbox, "checkbutton14"); /* mouse pointer */
      zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/hide_pointer");

      widget = lookup_widget(pbox, "checkbutton3"); /* start muted */
      zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/start_muted");

      widget = lookup_widget(pbox, "checkbutton5"); /* avoid flicker */
      zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/avoid_flicker");

      widget = lookup_widget(pbox, "checkbutton11"); /* save controls */
      zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/save_controls");

      widget = lookup_widget(pbox, "title_format"); /* title format */
      widget = gnome_entry_gtk_entry(GNOME_ENTRY(widget));
      zconf_set_string(gtk_entry_get_text(GTK_ENTRY(widget)),
			"/zapping/options/main/title_format");

      widget = lookup_widget(pbox, "spinbutton1"); /* zapping_setup_fb
						    verbosity */
      zconf_set_integer(
	gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget)),
			"/zapping/options/main/zapping_setup_fb_verbosity");

      widget = lookup_widget(pbox, "optionmenu1"); /* ratio mode */

      zconf_set_integer(z_option_menu_get_active(widget),
			"/zapping/options/main/ratio");

      widget = lookup_widget(pbox, "optionmenu2"); /* change mode */

      zconf_set_integer(z_option_menu_get_active(widget),
			"/zapping/options/main/change_mode");

      widget = lookup_widget(pbox, "optionmenu20"); /* xv capture size */

      zconf_set_integer(z_option_menu_get_active(widget),
			"/zapping/options/capture/xvsize");

      widget = lookup_widget(pbox, "optionmenu22"); /* osd type */

      zconf_set_integer(z_option_menu_get_active(widget),
			"/zapping/options/osd/osd_type");

      widget = lookup_widget(pbox, "fontpicker1");
      zconf_set_string(gnome_font_picker_get_font_name(GNOME_FONT_PICKER(widget)),
		       "/zapping/options/osd/font");

      widget = lookup_widget(pbox, "colorpicker1");

      gnome_color_picker_get_d(GNOME_COLOR_PICKER(widget), &r, &g, &b,
			       &a);
      zconf_set_float(r, "/zapping/options/osd/fg_r");
      zconf_set_float(g, "/zapping/options/osd/fg_g");
      zconf_set_float(b, "/zapping/options/osd/fg_b");

      widget = lookup_widget(pbox, "colorpicker2");

      gnome_color_picker_get_d(GNOME_COLOR_PICKER(widget), &r, &g, &b,
			       &a);
      zconf_set_float(r, "/zapping/options/osd/bg_r");
      zconf_set_float(g, "/zapping/options/osd/bg_g");
      zconf_set_float(b, "/zapping/options/osd/bg_b");

      widget = lookup_widget(pbox, "spinbutton2"); /* osd timeout */
      zconf_set_float(
	gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(widget)),
			"/zapping/options/osd/timeout");

      break;
    case 2:
      widget = lookup_widget(pbox, "checkbutton6"); /* enable VBI
						       decoding */
      zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/vbi/enable_vbi");

      widget = lookup_widget(pbox, "checkbutton7"); /* Use VBI
						       station names */
      zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/vbi/use_vbi");

      widget = lookup_widget(pbox, "checkbutton12"); /* Overlay TTX
						       pages
						       automagically */
      zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/vbi/auto_overlay");

      widget = lookup_widget(pbox, "fileentry2"); /* VBI device entry */
      text = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY(widget),
					     TRUE);
      if (text)
	zconf_set_string(text, "/zapping/options/vbi/vbi_device");

      g_free(text); /* In the docs it says this should be freed */

      /* default_region */
      widget = lookup_widget(pbox, "optionmenu3");
      index = z_option_menu_get_active(widget);

      if (index < 0)
	index = 0;
      if (index > 7)
	index = 7;

      zconf_set_integer(index, "/zapping/options/vbi/default_region");
      if (zvbi_get_object())
	vbi_teletext_set_default_region(zvbi_get_object(), region_mapping[index]);

      /* teletext_level */
      widget = lookup_widget(pbox, "optionmenu4");
      index = z_option_menu_get_active(widget);

      if (index < 0)
	index = 0;
      if (index > 3)
	index = 3;

      zconf_set_integer(index, "/zapping/options/vbi/teletext_level");
      if (zvbi_get_object())
	vbi_teletext_set_level(zvbi_get_object(), index);

      /* Quality/speed tradeoff */
      widget = lookup_widget(pbox, "optionmenu21");
      index = z_option_menu_get_active(widget);

      if (index < 0)
	index = 0;
      if (index > 3)
	index = 3;

      zconf_set_integer(index, "/zapping/options/vbi/qstradeoff");

      widget = lookup_widget(pbox, "subtitle_page"); /* subtitle page */
      zvbi_page =
	dec2bcd(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget)));
      zcs_int(zvbi_page, "zvbi_page");
      osd_clear();

      /* The many itv filters */
      widget = lookup_widget(pbox, "optionmenu12");
      index = z_option_menu_get_active(widget);
      zconf_set_integer(index, "/zapping/options/vbi/pr_trigger");

      widget = lookup_widget(pbox, "optionmenu16");
      index = z_option_menu_get_active(widget);
      zconf_set_integer(index, "/zapping/options/vbi/nw_trigger");

      widget = lookup_widget(pbox, "optionmenu17");
      index = z_option_menu_get_active(widget);
      zconf_set_integer(index, "/zapping/options/vbi/st_trigger");

      widget = lookup_widget(pbox, "optionmenu18");
      index = z_option_menu_get_active(widget);
      zconf_set_integer(index, "/zapping/options/vbi/sp_trigger");

      widget = lookup_widget(pbox, "optionmenu19");
      index = z_option_menu_get_active(widget);
      zconf_set_integer(index, "/zapping/options/vbi/op_trigger");

      widget = lookup_widget(pbox, "optionmenu6");
      index = z_option_menu_get_active(widget);
      zconf_set_integer(index, "/zapping/options/vbi/trigger_default");

      /* Filter level */
      widget = lookup_widget(pbox, "optionmenu5");
      index = z_option_menu_get_active(widget);
      zconf_set_integer(index, "/zapping/options/vbi/filter_level");

      break;
    default:
      for (i=0; i<num_handlers; i++)
	if (handlers[i].apply(gnomepropertybox, arg1))
	  break;

      if (i == num_handlers)
	ShowBox("Nothing accepts this page!!\nPlease contact the maintainer",
		GNOME_MESSAGE_BOX_WARNING);
      break;
    }
}


void
on_zapping_properties_help             (GnomePropertyBox *gnomepropertybox,
                                        gint             arg1,
                                        gpointer         user_data)
{
  GnomeHelpMenuEntry entry = {"zapping", "properties.html"};
  gint i;

  enum tveng_capture_mode cur_mode;

  cur_mode = tveng_stop_everything(main_info);

  switch (arg1)
    {
    case -1:
      break; /* end of the calls */
    case 0 ... 2:
      gnome_help_display(NULL, &entry);
      break;
    default:
      break;
    }

  for (i=0; i<num_handlers; i++)
    if (handlers[i].help(gnomepropertybox, arg1))
      break;
  
  if (i == num_handlers)
    ShowBox("The code that created the active page doesn't"
	    " provide help for it!!!\nPlease contact the maintainer",
	    GNOME_MESSAGE_BOX_WARNING);

  if (tveng_restart_everything(cur_mode, main_info) == -1)
    ShowBox(main_info->error, GNOME_MESSAGE_BOX_ERROR);
}

/* This function is called when some item in the property box changes */
void
on_property_item_changed              (GtkWidget * changed_widget,
				       GnomePropertyBox *propertybox)
{
  update_sensitivity(changed_widget);

  gnome_property_box_changed (propertybox);
}

void register_properties_handler (property_handler *p)
{
  handlers = g_realloc(handlers, (num_handlers+1)*sizeof(handlers[0]));
  memcpy(&handlers[num_handlers++], p, sizeof(*p));
}
