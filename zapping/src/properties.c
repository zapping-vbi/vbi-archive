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
/*
  Handles the property dialog. This was previously in callbacks.c, but
  it was getting too big, so I moved the code here.
*/
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#include "tveng.h"
#include "callbacks.h"
#include "interface.h"
#include "v4linterface.h"
#include "plugins.h"
#include "zconf.h"
#include "zvbi.h"
/* Manages config values for zconf (it saves me some typing) */
#define ZCONF_DOMAIN "/zapping/internal/callbacks/"
#include "zmisc.h"

extern tveng_device_info * main_info; /* About the device we are using */

extern GList * plugin_list; /* The plugins we have */

void
on_propiedades1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget * zapping_properties = create_zapping_properties();
  GList * p = g_list_first(plugin_list); /* For traversing the plugins
					  */
  GtkNotebook * nb;
  GtkWidget * nb_label;
  GtkWidget * nb_body;
  int i=0;

  gchar * buffer; /* Some temporary buffer */

  /* Widget for assigning the callbacks (generic) */
  GtkWidget * widget;

  if (NULL == zapping_properties)
    {
      ShowBox(_("The properties dialog could not be opened\n"
		"Check the location of zapping.glade"),
	      GNOME_MESSAGE_BOX_ERROR);
      return;
    }

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

  /* Start zapping muted */
  widget = lookup_widget(zapping_properties, "checkbutton3");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/main/start_muted"));

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

  /* Enable VBI decoding */
  widget = lookup_widget(zapping_properties, "checkbutton6");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/vbi/enable_vbi"));

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

  /* VBI device */
  widget = lookup_widget(zapping_properties, "fileentry2");
  widget = gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY(widget));
  gtk_entry_set_text(GTK_ENTRY(widget),
		     zconf_get_string(NULL,
				      "/zapping/options/vbi/vbi_device"));

  gtk_signal_connect(GTK_OBJECT(widget), "changed",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);

  /* erc (Error correction) */
  widget = lookup_widget(zapping_properties, "checkbutton8");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
    zconf_get_boolean(NULL, "/zapping/options/vbi/erc"));

  gtk_signal_connect(GTK_OBJECT(widget), "toggled",
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

  /* Destination for Zapzilla exports */
  widget = lookup_widget(zapping_properties, "fileentry3");
  widget = gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY(widget));
  gtk_entry_set_text(GTK_ENTRY(widget),
		     zconf_get_string(NULL,
				      "/zapping/ttxview/exportdir"));

  gtk_signal_connect(GTK_OBJECT(widget), "changed",
		     GTK_SIGNAL_FUNC(on_property_item_changed),
		     zapping_properties);  

  /* Disable/enable the VBI options */
  widget = lookup_widget(zapping_properties, "vbox19");
  gtk_widget_set_sensitive(widget,
	 zconf_get_boolean(NULL, "/zapping/options/vbi/enable_vbi"));

  /* Let the plugins add their properties */
  while (p)
    {
      plugin_add_properties(GNOME_PROPERTY_BOX(zapping_properties),
			    (struct plugin_info *) p->data);
      p = p->next;
    }

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
  GList * p; /* For traversing the plugins */
  gint index;
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

      widget = lookup_widget(pbox, "checkbutton3"); /* start muted */
      zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/start_muted");

      widget = lookup_widget(pbox, "checkbutton5"); /* avoid flicker */
      zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/main/avoid_flicker");

      widget = lookup_widget(pbox, "spinbutton1"); /* zapping_setup_fb
						    verbosity */
      zconf_set_integer(
	gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget)),
			"/zapping/options/main/zapping_setup_fb_verbosity");

      widget = lookup_widget(pbox, "optionmenu1"); /* ratio mode */
      widget = GTK_WIDGET(GTK_OPTION_MENU(widget)->menu);

      zconf_set_integer(
	g_list_index(GTK_MENU_SHELL(widget)->children,
		     gtk_menu_get_active(GTK_MENU(widget))),
	"/zapping/options/main/ratio");

      widget = lookup_widget(pbox, "optionmenu2");
      widget = GTK_WIDGET(GTK_OPTION_MENU(widget)->menu);

      zconf_set_integer(
	g_list_index(GTK_MENU_SHELL(widget)->children,
		     gtk_menu_get_active(GTK_MENU(widget))),
	"/zapping/options/main/change_mode");

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

      widget = lookup_widget(pbox, "fileentry2"); /* VBI device entry */
      text = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY(widget),
					     TRUE);
      zconf_set_string(text, "/zapping/options/vbi/vbi_device");

      g_free(text); /* In the docs it says this should be freed */

      /* default_region */
      widget = lookup_widget(pbox, "optionmenu3");
      widget = GTK_WIDGET(GTK_OPTION_MENU(widget)->menu);

      index = g_list_index(GTK_MENU_SHELL(widget)->children,
			   gtk_menu_get_active(GTK_MENU(widget)));
      if (index < 0)
	index = 0;
      if (index > 7)
	index = 7;

      zconf_set_integer(index, "/zapping/options/vbi/default_region");
      if (zvbi_get_object())
	vbi_set_default_region(zvbi_get_object(), region_mapping[index]);

      widget = lookup_widget(pbox, "checkbutton8"); /* erc */
      zconf_set_boolean(gtk_toggle_button_get_active(
	GTK_TOGGLE_BUTTON(widget)), "/zapping/options/vbi/erc");

      /* Directory for exporting */
      widget = lookup_widget(pbox, "fileentry3");
      text = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY(widget),
					     TRUE);
      zconf_set_string(text, "/zapping/ttxview/exportdir");
      g_free(text);

      break;
    default:
      p = g_list_first(plugin_list);
      while (p) /* Try with all the plugins until one of them accepts
		   the call */
	{
	  if (plugin_activate_properties(gnomepropertybox, arg1,
					 (struct plugin_info*) p->data))
	    break; /* returned TRUE: stop */
	  p = p->next;
	}
      /* This shouldn't ideally be reached, but a g_assert is too
	 strong */
      if ((p == NULL) && (arg1 != -1))
	ShowBox(_("No plugin accepts this page."),
		GNOME_MESSAGE_BOX_INFO);
    }
}


void
on_zapping_properties_help             (GnomePropertyBox *gnomepropertybox,
                                        gint             arg1,
                                        gpointer         user_data)
{
  GnomeHelpMenuEntry entry = {"zapping", "properties.html"};

  GList * p = g_list_first(plugin_list); /* Traverse all the plugins */

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
      while (p)
	{
	  if (plugin_help_properties(gnomepropertybox, arg1,
				     (struct plugin_info*) p->data))
				     break;
	  p = p->next;
	}
      if (p == NULL)
	ShowBox(_("The plugin that created the active page doesn't"
		  " provide help for it. Sorry."),
		GNOME_MESSAGE_BOX_INFO);
    }

  if (tveng_restart_everything(cur_mode, main_info) == -1)
    ShowBox(main_info->error, GNOME_MESSAGE_BOX_ERROR);
}

/* This function is called when some item in the property box changes */
void
on_property_item_changed              (GtkWidget * changed_widget,
				       GnomePropertyBox *propertybox)
{
  gboolean active;

  if (lookup_widget(changed_widget, "checkbutton6") == changed_widget)
    {
      active =
	gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(changed_widget));
      gtk_widget_set_sensitive(lookup_widget(changed_widget,
					     "vbox19"), active);      
    }

  gnome_property_box_changed (propertybox);
}
