/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2001 Iñaki García Etxebarria
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
/**
 * Properties handler for Z. Uses the shell code from properties.h,
 * you can think of this code as the "model" for the properties
 * structure, while properties.c is the "view". Buzzwords, ya know :-)
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "callbacks.h"
#include "interface.h"
#include "properties.h"
#include "properties-handler.h"
#include "zmisc.h"
#define ZCONF_DOMAIN "/zapping/internal/callbacks/"
#include "zconf.h"
#include "zvbi.h"
#include "osd.h"

/* Property handlers for the different pages */
/* Device info */
static void
di_setup		(GtkWidget	*page)
{
  extern tveng_device_info *main_info;
  GtkWidget *widget;
  gchar *buffer;
  gint i;
  GtkNotebook *nb;
  GtkWidget * nb_label;
  GtkWidget * nb_body;

  /* The device name */
  widget = lookup_widget(page, "label27");
  gtk_label_set_text(GTK_LABEL(widget), main_info->caps.name);

  /* Minimum capture dimensions */
  widget = lookup_widget(page, "label28");
  buffer = g_strdup_printf("%d x %d", main_info->caps.minwidth,
			   main_info->caps.minheight);
  gtk_label_set_text(GTK_LABEL(widget), buffer);
  g_free(buffer);

  /* Maximum capture dimensions */
  widget = lookup_widget(page, "label29");
  buffer = g_strdup_printf("%d x %d", main_info->caps.maxwidth,
			   main_info->caps.maxheight);
  gtk_label_set_text(GTK_LABEL(widget), buffer);
  g_free(buffer);

  /* Reported device capabilities */
  widget = lookup_widget(page, "label30");
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

  nb = GTK_NOTEBOOK (lookup_widget(page, "notebook2"));
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
  
  /* Selected video device */
  widget = lookup_widget(page, "fileentry1");
  widget = gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY(widget));
  gtk_entry_set_text(GTK_ENTRY(widget),
		     zconf_get_string(NULL,
				      "/zapping/options/main/video_device"));
  /* Current controller */
  widget = lookup_widget(page, "label31");
  tveng_describe_controller(NULL, &buffer, main_info);
  gtk_label_set_text(GTK_LABEL(widget), buffer);
}

static void
di_apply		(GtkWidget	*page)
{
  GtkWidget *widget;
  gchar *text;

  widget = lookup_widget(page, "fileentry1"); /* Video device entry
					       */
  text = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY(widget),
					 TRUE);
  if (text)
    zconf_set_string(text, "/zapping/options/main/video_device");
  
  g_free(text); /* In the docs it says this should be freed */  
}

/* Main window */
static void
mw_setup		(GtkWidget	*page)
{
  GtkWidget *widget;

  /* Save the geometry through sessions */
  widget = lookup_widget(page, "checkbutton2");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/keep_geometry"));

  /* Resize using fixed increments */
  widget = lookup_widget(page, "checkbutton4");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/fixed_increments"));  

  /* Swap Page Up/Down */
  widget = lookup_widget(page, "checkbutton13");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/swap_up_down"));

  /* Hide the mouse pointer */
  widget = lookup_widget(page, "checkbutton14");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/hide_pointer"));

  /* Title format Z will use */
  widget = lookup_widget(page, "title_format");
  widget = gnome_entry_gtk_entry(GNOME_ENTRY(widget));
  gtk_entry_set_text(GTK_ENTRY(widget),
		     zconf_get_string(NULL,
				      "/zapping/options/main/title_format"));

  /* ratio mode to use */
  widget = lookup_widget(page, "optionmenu1");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/main/ratio"));  
}

static void
mw_apply		(GtkWidget	*page)
{
  GtkWidget *widget;

  widget = lookup_widget(page, "checkbutton2"); /* keep geometry */
  zconf_set_boolean(gtk_toggle_button_get_active(
		 GTK_TOGGLE_BUTTON(widget)),
		    "/zapping/options/main/keep_geometry");

  widget = lookup_widget(page, "checkbutton4"); /* fixed increments */
  zconf_set_boolean(gtk_toggle_button_get_active(
		 GTK_TOGGLE_BUTTON(widget)),
		    "/zapping/options/main/fixed_increments");

  widget = lookup_widget(page, "checkbutton13"); /* swap chan up/down */
  zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/swap_up_down");  

  widget = lookup_widget(page, "checkbutton14"); /* mouse pointer */
  zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/hide_pointer");

  widget = lookup_widget(page, "title_format"); /* title format */
  widget = gnome_entry_gtk_entry(GNOME_ENTRY(widget));
  zconf_set_string(gtk_entry_get_text(GTK_ENTRY(widget)),
		   "/zapping/options/main/title_format");

  widget = lookup_widget(page, "optionmenu1"); /* ratio mode */
  zconf_set_integer(z_option_menu_get_active(widget),
		    "/zapping/options/main/ratio");
}

/* Video */
static void
video_setup		(GtkWidget	*page)
{
  GtkWidget *widget;

  /* Avoid some flicker in preview mode */
  widget = lookup_widget(page, "checkbutton5");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/avoid_flicker"));

  /* Save control info with the channel */
  widget = lookup_widget(page, "checkbutton11");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/save_controls"));

  /* Verbosity value passed to zapping_setup_fb */
  widget = lookup_widget(page, "spinbutton1");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),
     zconf_get_integer(NULL,
		       "/zapping/options/main/zapping_setup_fb_verbosity"));

  /* fullscreen video mode */
  widget = lookup_widget(page, "optionmenu2");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/main/change_mode"));

  /* capture size under XVideo */
  widget = lookup_widget(page, "optionmenu20");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/capture/xvsize"));
  
}

static void
video_apply		(GtkWidget	*page)
{
  GtkWidget *widget;


  widget = lookup_widget(page, "checkbutton5"); /* avoid flicker */
  zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/avoid_flicker");

  widget = lookup_widget(page, "checkbutton11"); /* save controls */
  zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/save_controls");

  widget = lookup_widget(page, "spinbutton1"); /* zapping_setup_fb
						  verbosity */
  zconf_set_integer(
	gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget)),
		"/zapping/options/main/zapping_setup_fb_verbosity");

  widget = lookup_widget(page, "optionmenu2"); /* change mode */
  zconf_set_integer(z_option_menu_get_active(widget),
		    "/zapping/options/main/change_mode");

  widget = lookup_widget(page, "optionmenu20"); /* xv capture size */
  zconf_set_integer(z_option_menu_get_active(widget),
		    "/zapping/options/capture/xvsize");
}

static void
on_osd_type_changed	(GtkWidget	*widget,
			 GtkWidget	*page)
{
  widget = lookup_widget(widget, "optionmenu22");
  gtk_widget_set_sensitive(lookup_widget(widget, "vbox38"),
			   !z_option_menu_get_active(widget));
}

/* OSD */
static void
osd_setup		(GtkWidget	*page)
{
  GtkWidget *widget;

  /* OSD type */
  widget = lookup_widget(page, "optionmenu22");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/osd/osd_type"));
  gtk_widget_set_sensitive(lookup_widget(widget, "vbox38"),
	   !zconf_get_integer(NULL, "/zapping/options/osd/osd_type"));
  gtk_signal_connect(GTK_OBJECT(GTK_OPTION_MENU(widget)->menu), "deactivate",
		     GTK_SIGNAL_FUNC(on_osd_type_changed),
		     page);

  /* OSD font */
  widget = lookup_widget(page, "fontpicker1");
  if (zconf_get_string(NULL, "/zapping/options/osd/font"))
    gnome_font_picker_set_font_name(GNOME_FONT_PICKER(widget),
    zconf_get_string(NULL, "/zapping/options/osd/font"));

  /* OSD foreground color */
  widget = lookup_widget(page, "colorpicker1");
  gnome_color_picker_set_d(GNOME_COLOR_PICKER(widget),
		   zconf_get_float(NULL, "/zapping/options/osd/fg_r"),
		   zconf_get_float(NULL, "/zapping/options/osd/fg_g"),
		   zconf_get_float(NULL, "/zapping/options/osd/fg_b"),
		   0);

  /* OSD background color */
  widget = lookup_widget(page, "colorpicker2");
  gnome_color_picker_set_d(GNOME_COLOR_PICKER(widget),
		   zconf_get_float(NULL, "/zapping/options/osd/bg_r"),
		   zconf_get_float(NULL, "/zapping/options/osd/bg_g"),
		   zconf_get_float(NULL, "/zapping/options/osd/bg_b"),
		   0);

  /* OSD timeout in seconds */
  widget = lookup_widget(page, "spinbutton2");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),
     zconf_get_float(NULL,
		       "/zapping/options/osd/timeout"));
}

static void
osd_apply		(GtkWidget	*page)
{
  GtkWidget *widget;
  gdouble r, g, b, a;

  widget = lookup_widget(page, "optionmenu22"); /* osd type */
  zconf_set_integer(z_option_menu_get_active(widget),
		    "/zapping/options/osd/osd_type");

  widget = lookup_widget(page, "fontpicker1");
  zconf_set_string(gnome_font_picker_get_font_name(GNOME_FONT_PICKER(widget)),
		   "/zapping/options/osd/font");

  widget = lookup_widget(page, "colorpicker1");
  gnome_color_picker_get_d(GNOME_COLOR_PICKER(widget), &r, &g, &b,
			   &a);
  zconf_set_float(r, "/zapping/options/osd/fg_r");
  zconf_set_float(g, "/zapping/options/osd/fg_g");
  zconf_set_float(b, "/zapping/options/osd/fg_b");

  widget = lookup_widget(page, "colorpicker2");
  gnome_color_picker_get_d(GNOME_COLOR_PICKER(widget), &r, &g, &b,
			   &a);
  zconf_set_float(r, "/zapping/options/osd/bg_r");
  zconf_set_float(g, "/zapping/options/osd/bg_g");
  zconf_set_float(b, "/zapping/options/osd/bg_b");

  widget = lookup_widget(page, "spinbutton2"); /* osd timeout */
  zconf_set_float(
	gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(widget)),
	"/zapping/options/osd/timeout");

}

static void
on_enable_vbi_toggled	(GtkWidget	*widget,
			 GtkWidget	*page)
{
  gboolean active =
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  GtkWidget *itv_props =
    get_properties_page(widget, _("VBI Options"),
			_("Interactive TV"));
  gtk_widget_set_sensitive(lookup_widget(widget,
					 "vbox19"), active);

  if (itv_props)
    gtk_widget_set_sensitive(itv_props, active);
}

/* VBI */
static void
vbi_general_setup	(GtkWidget	*page)
{
  GtkWidget *widget;

  /* Enable VBI decoding */
  widget = lookup_widget(page, "checkbutton6");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/vbi/enable_vbi"));
  on_enable_vbi_toggled(widget, page);
  gtk_signal_connect(GTK_OBJECT(widget), "toggled",
		     GTK_SIGNAL_FUNC(on_enable_vbi_toggled),
		     page);

  /* use VBI for getting station names */
  widget = lookup_widget(page, "checkbutton7");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/vbi/use_vbi"));

  /* overlay subtitle pages automagically */
  widget = lookup_widget(page, "checkbutton12");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/vbi/auto_overlay"));

  /* VBI device */
  widget = lookup_widget(page, "fileentry2");
  widget = gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY(widget));
  gtk_entry_set_text(GTK_ENTRY(widget),
		     zconf_get_string(NULL,
				      "/zapping/options/vbi/vbi_device"));

  /* Default region */
  widget = lookup_widget(page, "optionmenu3");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/default_region"));

  /* Teletext level */
  widget = lookup_widget(page, "optionmenu4");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/teletext_level"));

  /* Quality/speed tradeoff */
  widget = lookup_widget(page, "optionmenu21");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/qstradeoff"));

  /* Default subtitle page */
  widget = lookup_widget(page, "subtitle_page");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),
			    bcd2dec(zcg_int(NULL, "zvbi_page")));
}

static void
vbi_general_apply	(GtkWidget	*page)
{
  GtkWidget *widget;
  gchar *text;
  gint index;

  int region_mapping[8] = {
    0, /* WCE */
    8, /* EE */
    16, /* WET */
    24, /* CSE */
    32, /* C */
    48, /* GC */
    64, /* A */
    80 /* I */
  };

  widget = lookup_widget(page, "checkbutton6"); /* enable VBI
						   decoding */
  zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/vbi/enable_vbi");

  widget = lookup_widget(page, "checkbutton7"); /* Use VBI
						   station names */
  zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/vbi/use_vbi");

  widget = lookup_widget(page, "checkbutton12"); /* Overlay TTX
						    pages
						    automagically */
  zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/vbi/auto_overlay");

  widget = lookup_widget(page, "fileentry2"); /* VBI device entry */
  text = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY(widget),
					 TRUE);
  if (text)
    zconf_set_string(text, "/zapping/options/vbi/vbi_device");
  g_free(text); /* In the docs it says this should be freed */

  /* default_region */
  widget = lookup_widget(page, "optionmenu3");
  index = z_option_menu_get_active(widget);

  if (index < 0)
    index = 0;
  if (index > 7)
    index = 7;

  zconf_set_integer(index, "/zapping/options/vbi/default_region");
  if (zvbi_get_object())
    vbi_teletext_set_default_region(zvbi_get_object(), region_mapping[index]);

  /* teletext_level */
  widget = lookup_widget(page, "optionmenu4");
  index = z_option_menu_get_active(widget);
  if (index < 0)
    index = 0;
  if (index > 3)
    index = 3;
  zconf_set_integer(index, "/zapping/options/vbi/teletext_level");
  if (zvbi_get_object())
    vbi_teletext_set_level(zvbi_get_object(), index);

  /* Quality/speed tradeoff */
  widget = lookup_widget(page, "optionmenu21");
  index = z_option_menu_get_active(widget);
  if (index < 0)
    index = 0;
  if (index > 3)
    index = 3;
  zconf_set_integer(index, "/zapping/options/vbi/qstradeoff");

  widget = lookup_widget(page, "subtitle_page"); /* subtitle page */
  index =
    dec2bcd(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget)));
  if (index != zvbi_page)
    {
      zvbi_page = index;
      zcs_int(zvbi_page, "zvbi_page");
      osd_clear();
    }
}

/* Interactive TV */
static void
itv_setup		(GtkWidget	*page)
{
  GtkWidget *widget;

  /* The various itv filters */
  widget = lookup_widget(page, "optionmenu12");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/pr_trigger"));

  widget = lookup_widget(page, "optionmenu16");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/nw_trigger"));

  widget = lookup_widget(page, "optionmenu17");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/st_trigger"));

  widget = lookup_widget(page, "optionmenu18");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/sp_trigger"));

  widget = lookup_widget(page, "optionmenu19");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/op_trigger"));

  widget = lookup_widget(page, "optionmenu6");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/trigger_default"));

  /* Filter level */
  widget = lookup_widget(page, "optionmenu5");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
    zconf_get_integer(NULL,
		      "/zapping/options/vbi/filter_level"));

  /* Set sensitive/unsensitive according to enable_vbi state */
  gtk_widget_set_sensitive(page, zconf_get_boolean(NULL,
			   "/zapping/options/vbi/enable_vbi"));
}

static void
itv_apply		(GtkWidget	*page)
{
  GtkWidget *widget;
  gint index;

  /* The many itv filters */
  widget = lookup_widget(page, "optionmenu12");
  index = z_option_menu_get_active(widget);
  zconf_set_integer(index, "/zapping/options/vbi/pr_trigger");
  
  widget = lookup_widget(page, "optionmenu16");
  index = z_option_menu_get_active(widget);
  zconf_set_integer(index, "/zapping/options/vbi/nw_trigger");
  
  widget = lookup_widget(page, "optionmenu17");
  index = z_option_menu_get_active(widget);
  zconf_set_integer(index, "/zapping/options/vbi/st_trigger");
  
  widget = lookup_widget(page, "optionmenu18");
  index = z_option_menu_get_active(widget);
  zconf_set_integer(index, "/zapping/options/vbi/sp_trigger");
  
  widget = lookup_widget(page, "optionmenu19");
  index = z_option_menu_get_active(widget);
  zconf_set_integer(index, "/zapping/options/vbi/op_trigger");
  
  widget = lookup_widget(page, "optionmenu6");
  index = z_option_menu_get_active(widget);
  zconf_set_integer(index, "/zapping/options/vbi/trigger_default");
  
  /* Filter level */
  widget = lookup_widget(page, "optionmenu5");
  index = z_option_menu_get_active(widget);
  zconf_set_integer(index, "/zapping/options/vbi/filter_level");
}

static void
add				(GnomeDialog	*dialog)
{
  SidebarEntry device_info[] = {
    { N_("Device Info"), ICON_GNOME, "gnome-info.png", "vbox9",
      di_setup, di_apply }
  };
  SidebarEntry general_options[] = {
    { N_("Main Window"), ICON_GNOME, "gnome-session.png", "vbox35",
      mw_setup, mw_apply },
    { N_("OSD"), ICON_GNOME, "gnome-oscilloscope.png", "vbox37",
      osd_setup, osd_apply },
    { N_("Video"), ICON_ZAPPING, "gnome-television.png", "vbox36",
      video_setup, video_apply }
  };
  SidebarEntry vbi_options[] = {
    { N_("General"), ICON_GNOME, "gnome-monitor.png", "vbox17",
      vbi_general_setup, vbi_general_apply },
    { N_("Interactive TV"), ICON_GNOME, "gnome-monitor.png", "vbox33",
      itv_setup, itv_apply }
  };
  SidebarGroup groups[] = {
    { N_("Device Info"), device_info, acount(device_info) },
    { N_("General Options"), general_options, acount(general_options) },
    { N_("VBI Options"), vbi_options, acount(vbi_options) }
  };

  standard_properties_add(dialog, groups, acount(groups),
			  PACKAGE_DATA_DIR "/zapping.glade");

  open_properties_group(GTK_WIDGET(dialog), _("General Options"));
}

void startup_properties_handler(void)
{
  property_handler handler = {
    add:	add
  };
  prepend_property_handler(&handler);
}

void shutdown_properties_handler(void)
{
  /* Nothing */
}
