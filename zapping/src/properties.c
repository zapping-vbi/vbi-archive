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
extern GtkWidget * main_window;

/* Signals that the given page has been modified */
static void
page_modified			(GnomeDialog	*dialog,
				 gint		page_id)
{
  g_message("modify!!");
}

static void
modify_page			(GtkWidget	*widget,
				 gpointer	page_id_ptr)
{
  GnomeDialog *dialog = GNOME_DIALOG
    (gtk_object_get_data(GTK_OBJECT(widget), "modify_page_dialog"));
  gint page_id = GPOINTER_TO_INT(page_id_ptr);

  page_modified(dialog, page_id);
}

static void
font_set_bridge	(GtkWidget	*widget,
		 const gchar	*new_font,
		 gpointer	page_id_ptr)
{
  GnomeDialog *dialog = GNOME_DIALOG
    (gtk_object_get_data(GTK_OBJECT(widget), "modify_page_dialog"));
  gint page_id = GPOINTER_TO_INT(page_id_ptr);

  page_modified(dialog, page_id);
}

static void
color_set_bridge (GtkWidget	*widget,
		  guint		r,
		  guint		g,
		  guint		b,
		  guint		a,
		  gpointer	page_id_ptr)
{
  GnomeDialog *dialog = GNOME_DIALOG
    (gtk_object_get_data(GTK_OBJECT(widget), "modify_page_dialog"));
  gint page_id = GPOINTER_TO_INT(page_id_ptr);

  page_modified(dialog, page_id);
}

/**
 * Makes modifications on the widgets descending from widget trigger a
 * page_modifed call.
 */
static void
autoconnect_modify		(GnomeDialog	*dialog,
				 GtkWidget	*widget,
				 gint		page_id)
{
  GList *children;

  if (GTK_IS_CONTAINER(widget))
    {
      children = gtk_container_children(GTK_CONTAINER(widget));
      while (children)
	{
	  autoconnect_modify(dialog, GTK_WIDGET(children->data),
			     page_id);
	  children = children->next;
	}
    }

  if (GNOME_IS_FILE_ENTRY(widget))
    {
      widget = gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY(widget));
      autoconnect_modify(dialog, widget, page_id);
    }
  else if (GNOME_IS_ENTRY(widget))
    {
      widget = gnome_entry_gtk_entry(GNOME_ENTRY(widget));
      autoconnect_modify(dialog, widget, page_id);
    }
  else if (GTK_IS_TOGGLE_BUTTON(widget))
    {
      gtk_signal_connect(GTK_OBJECT(widget), "toggled",
			 GTK_SIGNAL_FUNC(modify_page),
			 GINT_TO_POINTER(page_id));
    }
  else if (GTK_IS_ENTRY(widget))
    {
      gtk_signal_connect(GTK_OBJECT(widget), "changed",
			 GTK_SIGNAL_FUNC(modify_page),
			 GINT_TO_POINTER(page_id));      
    }
  else if (GTK_IS_OPTION_MENU(widget))
    {
      widget = GTK_WIDGET(GTK_OPTION_MENU(widget)->menu);

      gtk_signal_connect(GTK_OBJECT(widget),
			 "deactivate",
			 GTK_SIGNAL_FUNC(modify_page),
			 GINT_TO_POINTER(page_id));
    }
  else if (GNOME_IS_FONT_PICKER(widget))
    {
      gtk_signal_connect(GTK_OBJECT(widget), "font-set",
			 GTK_SIGNAL_FUNC(font_set_bridge),
			 GINT_TO_POINTER(page_id));
    }
  else if (GNOME_IS_COLOR_PICKER(widget))
    {
      gtk_signal_connect(GTK_OBJECT(widget), "color-set",
			 GTK_SIGNAL_FUNC(color_set_bridge),
			 GINT_TO_POINTER(page_id));
    }

  gtk_object_set_data(GTK_OBJECT(widget), "modify_page_dialog", dialog);
}

static void
find_selected_group		(GtkWidget	*widget,
				 gint		*group)
{
  GtkContainer *group_container =
    GTK_CONTAINER(lookup_widget(widget, "group-container"));

  if (GTK_IS_BUTTON(widget))
    return; /* Nothing to be done for buttons */

  if (GTK_WIDGET_VISIBLE(widget))
    *group = g_list_index(gtk_container_children(group_container), widget)/2;
}

/* Gets the currently selected group and item, or sets to -1 both
   group and item if nothing is selected yet */
static void
get_cur_sel			(GtkWidget	*dialog,
				 gint		*group,
				 gint		*item)
{
  GtkContainer *group_container =
    GTK_CONTAINER(lookup_widget(GTK_WIDGET(dialog), "group-container"));
  GtkWidget *group_widget;
  GSList *group_list;
  gint count = 0;

  *group = *item = -1;

  gtk_container_foreach(group_container,
			GTK_SIGNAL_FUNC(find_selected_group),
			group);

  if (*group == -1)
    return; /* Nothing shown yet */
  
  group_widget = GTK_WIDGET
    (g_list_nth_data(gtk_container_children(group_container),
		     2*(*group) + 1));
  group_list = gtk_object_get_data(GTK_OBJECT(group_widget), "group_list");

  while (group_list)
    {
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(group_list->data)))
	*item = count;
      count ++;
      group_list = group_list->next;
    }

  /* For some weird reason the group list is reversed ?!?!?! */
  *item = count - (*item + 1);
}

static void
on_properties_ok_clicked	(GtkWidget	*button,
				 GnomeDialog	*dialog)
{
  gnome_dialog_close(dialog);
}

static void
on_properties_apply_clicked	(GtkWidget	*button,
				 GnomeDialog	*dialog)
{
  gtk_signal_emit_stop_by_name(GTK_OBJECT(button), "clicked");
}

static void
on_properties_cancel_clicked	(GtkWidget	*button,
				 GnomeDialog	*dialog)
{
  gnome_dialog_close(dialog);
}

static void
on_properties_help_clicked	(GtkWidget	*button,
				 GnomeDialog	*dialog)
{
}

static gint
on_properties_close		(GnomeDialog	*dialog,
				 gpointer	unused)
{
  return FALSE; /* done */
}

static void
show_hide_foreach		(GtkWidget	*widget,
				 GtkWidget	*show)
{
  if (GTK_IS_BUTTON(widget))
    return; /* Nothing to be done for buttons */

  if (widget == show)
    gtk_widget_show(widget);
  else
    gtk_widget_hide(widget);
}

static void
open_properties_group		(GtkWidget	*dialog,
				 const gchar	*group)
{
  gchar *buf = g_strdup_printf("group-contents-%s", group);
  GtkWidget *contents = lookup_widget(dialog, buf);
  GtkContainer *group_container = GTK_CONTAINER
    (lookup_widget(dialog, "group-container"));
  gint cur_group, cur_item;

  /* If the current selection is in a different group, switch and
     select the hidden toggle */
  get_cur_sel(dialog, &cur_group, &cur_item);

  if (cur_group == -1 ||
      GTK_WIDGET
      (g_list_nth_data(gtk_container_children(group_container),
		       2*cur_group + 1)) != contents)
  {
    gtk_button_clicked(GTK_BUTTON(gtk_object_get_data(GTK_OBJECT(contents),
						      "nsbutton")));
    gtk_container_foreach(GTK_CONTAINER(group_container),
			  GTK_SIGNAL_FUNC(show_hide_foreach),
			  contents);
  }

  g_free(buf);
}

/**
 * Makes sure that parent is at least as wide as widget.
 */
static void
ensure_width			(GtkWidget	*widget,
				 GtkWidget	*parent)
{
  GtkRequisition request;
  gint req_max_width = GPOINTER_TO_INT
    (gtk_object_get_data(GTK_OBJECT(parent), "req_max_width"));

  gtk_widget_size_request(widget, &request);
  if (request.width > req_max_width)
    {
      req_max_width = request.width;
      gtk_object_set_data(GTK_OBJECT(parent), "req_max_width",
			  GINT_TO_POINTER(req_max_width));
      gtk_widget_set_usize(parent, req_max_width, -2);
    }
}

static void
append_properties_group		(GnomeDialog	*dialog,
				 const gchar	*group)
{
  GtkWidget *button;
  GtkWidget *vbox = lookup_widget(GTK_WIDGET(dialog), "group-container");
  GtkWidget *contents;
  gchar *buf;
  GtkWidget *nsbutton;

  buf = g_strdup_printf("group-button-%s", group);
  if (find_widget(GTK_WIDGET(dialog), buf))
    g_error("A group named %s already exists in this dialog", buf);

  button = gtk_button_new_with_label(group);
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, TRUE, 0);
  gtk_widget_show(button);
  register_widget(button, buf);
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
		     GTK_SIGNAL_FUNC(open_properties_group),
		     (gpointer)group);
  ensure_width(button, vbox);

  g_free(buf);

  buf = g_strdup_printf("group-contents-%s", group);
  contents = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), contents);
  /* Note that we don't show() contents */
  register_widget(contents, buf);
  g_free(buf);

  /* Add the nothing selected button */
  nsbutton = gtk_radio_button_new(NULL);
  gtk_box_pack_start(GTK_BOX(contents), nsbutton, FALSE, TRUE, 0);
  gtk_object_set_data(GTK_OBJECT(contents), "group_list",
		      gtk_radio_button_group(GTK_RADIO_BUTTON(nsbutton)));
  gtk_object_set_data(GTK_OBJECT(contents), "nsbutton", nsbutton);
}

static void
on_radio_toggled		(GtkWidget	*selector,
				 gpointer	page_id_ptr)
{
  GtkNotebook * notebook =
    GTK_NOTEBOOK(lookup_widget(selector, "notebook"));
  gint page_id = GPOINTER_TO_INT(page_id_ptr);

  gtk_notebook_set_page(notebook, page_id);
}

static void
append_properties_page		(GnomeDialog	*dialog,
				 const gchar	*group,
				 const gchar	*label,
				 GtkWidget	*pixmap,
				 GtkWidget	*page)
{
  gchar *buf = g_strdup_printf("group-contents-%s", group);
  GtkWidget *contents = lookup_widget(GTK_WIDGET(dialog), buf);
  GtkWidget *radio;
  GSList *group_list = gtk_object_get_data(GTK_OBJECT(contents),
					   "group_list");
  GtkWidget *notebook = lookup_widget(GTK_WIDGET(dialog), "notebook");
  GtkWidget *container = lookup_widget(contents, "group-container");
  GtkWidget *vbox;
  GtkWidget *label_widget;
  guint page_id;

  radio = gtk_radio_button_new(group_list);
  gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(radio), FALSE);
  gtk_box_pack_start(GTK_BOX(contents), radio, FALSE, TRUE, 0);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page, gtk_label_new(""));
  page_id = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), page);
  autoconnect_modify(dialog, page, page_id);

  gtk_signal_connect(GTK_OBJECT(radio), "toggled",
		     GTK_SIGNAL_FUNC(on_radio_toggled),
		     GINT_TO_POINTER(page_id));

  group_list = gtk_radio_button_group(GTK_RADIO_BUTTON(radio));
  gtk_object_set_data(GTK_OBJECT(contents), "group_list", group_list);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(radio), vbox);
  if (pixmap)
    gtk_box_pack_start_defaults(GTK_BOX(vbox), pixmap);
  label_widget = gtk_label_new(label);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), label_widget);

  gtk_button_set_relief(GTK_BUTTON(radio), GTK_RELIEF_NONE);

  gtk_widget_show_all(radio);

  ensure_width(radio, container);
}

typedef struct {
  /* Label for the entry */
  const gchar	*label;
  /* Source of the icon (we use both GNOME and Z icons for the moment) */
  enum {
    ICON_ZAPPING,
    ICON_GNOME
  } icon_source;
  const gchar	*icon_name; /* relative to PACKAGE_PIXMAPS_DIR or
			       gnome_pixmap_file */
  const gchar	*widget; /* Notebook page to use (must be in zapping.glade) */
} SidebarEntry;

typedef struct {
  /* Label */
  const gchar	*label;
  /* Contents of the group */
  SidebarEntry	*items;
  /* Number of entries in the group */
  gint num_items;
} SidebarGroup;

#ifndef acount
#define acount(x) ((sizeof(x))/(sizeof(x[0])))
#else
#warning "Redefining acount"
#endif

static void
populate_sidebar		(GnomeDialog	*dialog)
{
  SidebarEntry device_info[] = {
    { N_("Device Info"), ICON_GNOME, "gnome-info.png", "vbox9" }
  };
  SidebarEntry general_options[] = {
    { N_("Main Window"), ICON_GNOME, "gnome-session.png", "vbox35" },
    { N_("Video"), ICON_ZAPPING, "gnome-television.png", "vbox36" },
    { N_("Audio"), ICON_GNOME, "gnome-grecord.png", "vbox39" },
    { N_("OSD"), ICON_GNOME, "gnome-oscilloscope.png", "vbox37" }
  };
  SidebarEntry vbi_options[] = {
    { N_("General"), ICON_GNOME, "gnome-monitor.png", "vbox17" },
    { N_("Interactive TV"), ICON_GNOME, "gnome-monitor.png", "vbox33" }
  };
  SidebarGroup groups[] = {
    { N_("Device Info"), device_info, acount(device_info) },
    { N_("General Options"), general_options, acount(general_options) },
    { N_("VBI Options"), vbi_options, acount(vbi_options) }
  };
  gint i, j;

  for (i = 0; i<acount(groups); i++)
    {
      append_properties_group(dialog, _(groups[i].label));

      for (j = 0; j<groups[i].num_items; j++)
	{
	  const gchar *icon_name = groups[i].items[j].icon_name;
	  gchar *pixmap_path = (groups[i].items[j].icon_source ==
				ICON_ZAPPING) ?
	    g_strdup_printf("%s/%s", PACKAGE_PIXMAPS_DIR, icon_name) :
	    g_strdup(gnome_pixmap_file(icon_name)); /* FIXME: leak?? */
	  GtkWidget *pixmap = z_pixmap_new_from_file(pixmap_path);
	  GtkWidget *page = build_widget(groups[i].items[j].widget,
					 PACKAGE_DATA_DIR "/zapping.glade");

	  append_properties_page(dialog, _(groups[i].label),
				 _(groups[i].items[j].label),
				 pixmap, page);

	  g_free(pixmap_path);
	}
    }

  open_properties_group(GTK_WIDGET(dialog), _("General Options"));
}

static void
build_properties_contents	(GnomeDialog	*dialog)
{
  GtkWidget *hbox;
  GtkWidget *frame;
  GtkNotebook *notebook;
  GtkWidget *vbox;
  GtkWidget *logo;

  hbox = gtk_hbox_new(FALSE, 3);
  gtk_box_pack_start_defaults(GTK_BOX(dialog->vbox), hbox);
  frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_box_pack_start(GTK_BOX(hbox), frame, FALSE, FALSE, 0);
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  register_widget(vbox, "group-container");

  /* Set the minimum height to something usable */
  /* FIXME: something in the line of ensure_width would be better */
  gtk_widget_set_usize(frame, -2, 400);

  /* Create a notebook for holding the pages. Note that we don't rely
     on any of the notebook's features, we select the active page
     programatically */
  notebook = GTK_NOTEBOOK(gtk_notebook_new());
  gtk_notebook_set_show_tabs(notebook, FALSE);
  gtk_notebook_set_show_border(notebook, FALSE);
  gtk_notebook_set_scrollable(notebook, FALSE);
  gtk_notebook_popup_disable(notebook);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), GTK_WIDGET(notebook));
  register_widget(GTK_WIDGET(notebook), "notebook");

  /* Put our logo when nothing is selected yet */
  logo = z_pixmap_new_from_file(PACKAGE_PIXMAPS_DIR "/logo.png");
  if (logo)
    {
      gtk_widget_show(logo);
      gtk_notebook_append_page(notebook, logo, gtk_label_new(""));
    }

  gtk_widget_show_all(hbox);

  populate_sidebar(dialog);
}

void
on_propiedades1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GnomeDialog *dialog;
  enum {
    OK_ID, APPLY_ID, CANCEL_ID, HELP_ID
  };

  dialog = GNOME_DIALOG(gnome_dialog_new(_("Zapping Properties"),
					 GNOME_STOCK_BUTTON_OK,
					 GNOME_STOCK_BUTTON_APPLY,
					 GNOME_STOCK_BUTTON_CANCEL,
					 GNOME_STOCK_BUTTON_HELP,
					 NULL));

  gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, TRUE, FALSE);

  build_properties_contents(dialog);

  gnome_dialog_set_default(dialog, OK_ID);
  gnome_dialog_set_parent(dialog, GTK_WINDOW(main_window));
  gnome_dialog_close_hides(dialog, FALSE); /* destroy on close */
  gnome_dialog_set_close(dialog, FALSE);

  /* callbacks */
  gtk_signal_connect(GTK_OBJECT(dialog), "close",
		     GTK_SIGNAL_FUNC(on_properties_close),
		     dialog);
  gnome_dialog_button_connect(dialog, OK_ID,
			      GTK_SIGNAL_FUNC(on_properties_ok_clicked),
			      dialog);
  gnome_dialog_button_connect(dialog, APPLY_ID,
			      GTK_SIGNAL_FUNC(on_properties_apply_clicked),
			      dialog);
  gnome_dialog_button_connect(dialog, CANCEL_ID,
			      GTK_SIGNAL_FUNC(on_properties_cancel_clicked),
			      dialog);
  gnome_dialog_button_connect(dialog, HELP_ID,
			      GTK_SIGNAL_FUNC(on_properties_help_clicked),
			      dialog);

  gnome_dialog_run_and_close(dialog);
}

static property_handler *handlers = NULL;
static gint num_handlers = 0;

void register_properties_handler (property_handler *p)
{
  handlers = g_realloc(handlers, (num_handlers+1)*sizeof(handlers[0]));
  memcpy(&handlers[num_handlers++], p, sizeof(*p));
}

