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
/**
 * This is just the properties "shell" code, that is, the code that
 * manages the dialog.
 * To see the code that manages the properties themselves, look at
 * properties-handler.c
 *
 * The overall scheme of things is as follows:
 *	- Each module interested in adding something to the properties
 *	dialog registers itself as a property_handler.
 *	- When the dialog is being built, the 'add' method of the
 *	handler is called, the handler should add groups and items as
 *	necessary. The shell code takes care of detecting changes and
 *	modifying the 'Apply' status as necessary, so
 *	gnome_property_box_changed is gone to a better place.
 *	- When apply is hit, the apply method of the handlers owning
 *	the 'dirty' pages is called, passing the dirty page.
 *	- Similarly for help and cancel.
 *	- The rest of the housekeeping is left to the handlers.
 */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#define ZCONF_DOMAIN "/zapping/internal/properties/"
#include "zconf.h"
#include "callbacks.h"
#include "interface.h"
#include "properties.h"
#include "zmisc.h"

extern GtkWidget * main_window;

static GnomeDialog *PropertiesDialog = NULL; /* Only you.. */
static property_handler *handlers = NULL;
static gint num_handlers = 0;

/**
 * Signals that the given page has been modified, mark the page as
 * dirty and set Apply sensitive.
 */
static void
page_modified			(GnomeDialog	*dialog,
				 gint		page_id)
{
  GtkNotebook *notebook = GTK_NOTEBOOK
    (lookup_widget(GTK_WIDGET(dialog), "properties-notebook"));
  GtkWidget *page = gtk_notebook_get_nth_page(notebook, page_id);

  gtk_object_set_data(GTK_OBJECT(page), "properties-dirty",
		      GINT_TO_POINTER(TRUE));

  gtk_widget_set_sensitive(lookup_widget(GTK_WIDGET(dialog), "apply"),
			   TRUE);
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

void
z_property_item_modified	(GtkWidget	*widget)
{
  GtkWidget *notebook = lookup_widget(widget, "properties-notebook");

  /* Walk till our parent is the notebook */
  while (widget && widget->parent != notebook && widget->parent)
    {
      if (GTK_IS_MENU(widget))
	widget = gtk_menu_get_attach_widget (GTK_MENU (widget) );
      else
	widget = widget -> parent;
    }
  
  if (!widget || !widget->parent)
    {
      g_warning("Property item ancestor not found!!");
      return;
    }

  page_modified(GNOME_DIALOG(gtk_widget_get_toplevel(notebook)),
		gtk_notebook_page_num(GTK_NOTEBOOK(notebook), widget));
}

static void
on_option_menu_modify		(GtkWidget	*menu,
				 gpointer	page_id_ptr)
{
  GtkWidget *omenu = GTK_WIDGET
    (gtk_object_get_data(GTK_OBJECT(menu), "properties-menu-parent"));
  gint last_status = GPOINTER_TO_INT
    (gtk_object_get_data(GTK_OBJECT(menu), "properties-last-status"));
  GnomeDialog *dialog = GNOME_DIALOG
    (gtk_object_get_data(GTK_OBJECT(menu), "modify_page_dialog"));
  gint page_id = GPOINTER_TO_INT(page_id_ptr);
  gint status = z_option_menu_get_active(omenu);

  if (status != last_status)
    {
      gtk_signal_emit_by_name(GTK_OBJECT(omenu), "changed", status);
      gtk_object_set_data(GTK_OBJECT(menu), "properties-last-status",
			  GINT_TO_POINTER(status));
      page_modified(dialog, page_id);
    }
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
  if (GTK_IS_CONTAINER(widget))
    {
      GList *children = gtk_container_children(GTK_CONTAINER(widget));
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
      GtkWidget *omenu = widget; /* option menu */
      widget = GTK_WIDGET(GTK_OPTION_MENU(widget)->menu);

      /* Do not mark as dirty when we don't modify the option menu
	 status */
      gtk_object_set_data(GTK_OBJECT(widget), "properties-last-status",
			  GINT_TO_POINTER
			  (z_option_menu_get_active(omenu)));

      /* Let the menu know it's parent */
      gtk_object_set_data(GTK_OBJECT(widget), "properties-menu-parent",
			  omenu);

      gtk_signal_connect(GTK_OBJECT(widget),
			 "deactivate",
			 GTK_SIGNAL_FUNC(on_option_menu_modify),
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
  else if (GTK_IS_RANGE(widget))
    {
      /* Weird cast to spare some useless code */
      widget = (GtkWidget*)gtk_range_get_adjustment(GTK_RANGE(widget));
      gtk_signal_connect(GTK_OBJECT(widget),
			 "value-changed",
			 GTK_SIGNAL_FUNC(modify_page),
			 GINT_TO_POINTER(page_id));
    }

  gtk_object_set_data(GTK_OBJECT(widget), "modify_page_dialog", dialog);
}

/**
 * Note that the structure is
 * + nsbutton (type = gtk_radio_button, descendant of gtk_button)
 * + group button (type gtk_button)
 *   + group contents (packed in a vbox)
 * This explains the -1, +2 etc magic.
 */
static void
find_selected_group		(GtkWidget	*widget,
				 gint		*group)
{
  GtkContainer *group_container =
    GTK_CONTAINER(lookup_widget(widget, "group-container"));

  if (GTK_IS_BUTTON(widget))
    return; /* Nothing to be done for buttons */

  if (GTK_WIDGET_VISIBLE(widget))
    *group =
      (g_list_index(gtk_container_children(group_container), widget)-1)/2;
}

/**
 * Returns the vbox associated with the nth group.
 */
static GtkWidget*
nth_group_contents		(gpointer	dialog,
				 gint		n)
{
  GtkContainer *group_container = GTK_CONTAINER
    (lookup_widget(GTK_WIDGET(dialog), "group-container"));

  return GTK_WIDGET((g_list_nth_data(gtk_container_children(group_container),
				     2*n + 2)));
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
  
  group_widget = nth_group_contents(dialog, *group);
  group_list = gtk_object_get_data(GTK_OBJECT(group_widget), "group_list");

  while (group_list)
    {
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(group_list->data)))
	*item = count;
      count ++;
      group_list = group_list->next;
    }

  /* For some weird reason the group list is reversed ?!?!?! */
  *item = count - *item;
}

GtkWidget *
get_properties_page		(GtkWidget	*sth,
				 const gchar	*group,
				 const gchar	*item)
{
  gchar *buf = g_strdup_printf("group-%s-item-%s", group, item);
  GtkWidget *widget = find_widget(sth, buf);

  g_free(buf);

  return widget;
}

static void
generic_apply			(GnomeDialog	*dialog)
{
  gint i = 0;
  GtkWidget *page;
  GtkNotebook *notebook =
    GTK_NOTEBOOK(lookup_widget(GTK_WIDGET(dialog), "properties-notebook"));

  while ((page = gtk_notebook_get_nth_page(notebook, i++)))
    if (gtk_object_get_data(GTK_OBJECT(page), "properties-dirty"))
      {
	property_handler *handler = (property_handler*)
	  gtk_object_get_data(GTK_OBJECT(page), "property-handler");

	handler->apply(dialog, page);

	gtk_object_set_data(GTK_OBJECT(page), "properties-dirty", NULL);
      }
}

static void
generic_cancel			(GnomeDialog	*dialog)
{
  gint i = 0;
  GtkWidget *page;
  GtkNotebook *notebook =
    GTK_NOTEBOOK(lookup_widget(GTK_WIDGET(dialog), "properties-notebook"));

  while ((page = gtk_notebook_get_nth_page(notebook, i++)))
    {
      property_handler *handler = (property_handler*)
	gtk_object_get_data(GTK_OBJECT(page), "property-handler");

      if (handler && handler->cancel)
	handler->cancel(dialog, page);

      gtk_object_set_data(GTK_OBJECT(page), "properties-dirty", NULL);
    }
}

static void
on_properties_ok_clicked	(GtkWidget	*button,
				 GnomeDialog	*dialog)
{
  generic_apply(dialog);

  gnome_dialog_close(dialog);
}

static void
on_properties_apply_clicked	(GtkWidget	*button,
				 GnomeDialog	*dialog)
{
  gtk_widget_set_sensitive(button, FALSE);

  generic_apply(dialog);
}

static void
on_properties_cancel_clicked	(GtkWidget	*button,
				 GnomeDialog	*dialog)
{
  generic_cancel(dialog);

  gnome_dialog_close(dialog);
}

static void
on_properties_help_clicked	(GtkWidget	*button,
				 GnomeDialog	*dialog)
{
  GtkNotebook *notebook =
    GTK_NOTEBOOK(lookup_widget(button, "properties-notebook"));
  gint cur_page = gtk_notebook_current_page(notebook);
  GtkWidget *page = gtk_notebook_get_nth_page(notebook, cur_page);
  property_handler *handler = (property_handler*)
    gtk_object_get_data(GTK_OBJECT(page), "property-handler");

  if (!handler)
    {
      /* All pages must have a handler except the first (Z logo) */
      g_assert(cur_page == 0);
      return;
    }

  handler->help(dialog, page);
}

static gint
on_properties_close		(GtkWidget	*dialog,
				 gpointer	unused)
{
  gint cur_group, cur_item;
  gchar *group_name = NULL;

  /* Remember last open group */
  get_cur_sel(dialog, &cur_group, &cur_item);

  if (cur_group != -1 &&
      (group_name = gtk_object_get_data(GTK_OBJECT
		(nth_group_contents(dialog, cur_group)), "group-name")))
    zcs_char(group_name, "last_group");

  PropertiesDialog = NULL;

  return FALSE; /* done */
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

/**
 * Connected to containers in the sidebar. Makes sure that there's
 * enough room for displaying everything without resizes.
 * @container: The container being populated.
 * @widget: The widget being added.
 * @sidebar: Pointer to the sidebar we are managing.
 */
static void
on_container_add		(GtkWidget	*container,
				 GtkWidget	*widget,
				 GtkWidget	*sidebar)
{
  GtkRequisition request;
  /* Sum of all the buttons height up to now */
  gint req_button_height = GPOINTER_TO_INT
    (gtk_object_get_data(GTK_OBJECT(sidebar), "req_button_height"));
  /* Biggest subgroup allocated */
  gint req_max_height = GPOINTER_TO_INT
    (gtk_object_get_data(GTK_OBJECT(sidebar), "req_max_height"));

  /* Adding a new group */
  if (container == sidebar && GTK_IS_BUTTON(widget))
    {
      gtk_widget_size_request(widget, &request);

      req_button_height += request.height;
      gtk_object_set_data(GTK_OBJECT(sidebar), "req_button_height",
			  GINT_TO_POINTER(req_button_height));
      gtk_widget_set_usize(sidebar, -2, req_button_height + req_max_height);
    }
  /* Adding a new item to a group */
  else if (GTK_IS_BUTTON(widget))
    {
      /* Get size request for the parent container */
      gtk_widget_size_request(container, &request);

      if (req_max_height < request.height)
	{
	  req_max_height = request.height;
	  gtk_object_set_data(GTK_OBJECT(sidebar), "req_max_height",
			      GINT_TO_POINTER(req_max_height));
	  gtk_widget_set_usize(sidebar, -2, req_button_height +
			       req_max_height);
	}
    }

  ensure_width(widget, sidebar);  
}

static void
on_radio_toggled		(GtkWidget	*selector,
				 gpointer	page_id_ptr)
{
  GtkNotebook * notebook =
    GTK_NOTEBOOK(lookup_widget(selector, "properties-notebook"));
  gint page_id = GPOINTER_TO_INT(page_id_ptr);

  gtk_notebook_set_page(notebook, page_id);
}

static void
build_properties_contents	(GnomeDialog	*dialog)
{
  GtkWidget *hbox;
  GtkWidget *frame;
  GtkNotebook *notebook;
  GtkWidget *vbox;
  GtkWidget *logo;
  GtkWidget *nsbutton;
  gint i, page_count = 0;

  hbox = gtk_hbox_new(FALSE, 3);
  gtk_box_pack_start_defaults(GTK_BOX(dialog->vbox), hbox);
  frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_box_pack_start(GTK_BOX(hbox), frame, FALSE, FALSE, 0);
  vbox = gtk_vbox_new(FALSE, 0);
  gtk_widget_show(vbox);
  gtk_container_add(GTK_CONTAINER(frame), vbox);
  register_widget(vbox, "group-container");

  /* Create a notebook for holding the pages. Note that we don't rely
     on any of the notebook's features, we select the active page
     programatically */
  /* Some eye candy first */
  frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), frame);
  /* Notebook */
  notebook = GTK_NOTEBOOK(gtk_notebook_new());
  gtk_notebook_set_show_tabs(notebook, FALSE);
  gtk_notebook_set_show_border(notebook, FALSE);
  gtk_notebook_set_scrollable(notebook, FALSE);
  gtk_notebook_popup_disable(notebook);
  gtk_container_add(GTK_CONTAINER(frame), GTK_WIDGET(notebook));
  register_widget(GTK_WIDGET(notebook), "properties-notebook");

  /* Put our logo when nothing is selected yet */
  if ((logo = z_load_pixmap ("logo.png")))
    {
      gtk_notebook_append_page(notebook, logo, gtk_label_new(""));
      page_count ++; /* No handler for this page */
    }

  gtk_widget_show_all(hbox);

  /* Add the nothing selected button */
  nsbutton = gtk_radio_button_new(NULL);
  gtk_box_pack_start(GTK_BOX(vbox), nsbutton, FALSE, TRUE, 0);
  gtk_object_set_data(GTK_OBJECT(vbox), "group_list",
		      gtk_radio_button_group(GTK_RADIO_BUTTON(nsbutton)));

  /* Make property handlers build their pages */
  for (i = 0; i<num_handlers; i++)
    {
      GtkWidget *page;
      handlers[i].add(dialog);
      while ((page = gtk_notebook_get_nth_page(notebook, page_count++)))
	{
	  gtk_object_set_data(GTK_OBJECT(page), "property-handler",
			      handlers + i);
	  /* Connect widgets in these pages to modify events */
	  autoconnect_modify(dialog, page, page_count-1);
	}
      page_count--;
    }
}

GtkWidget*
build_properties_dialog			(void)
{
  GnomeDialog *dialog;
  GtkWidget *apply_button;
  enum {
    OK_ID, APPLY_ID, CANCEL_ID, HELP_ID
  };

  if (PropertiesDialog)
    {
      gdk_window_raise(GTK_WIDGET(PropertiesDialog)->window);
      return GTK_WIDGET(PropertiesDialog);
    }

  dialog = GNOME_DIALOG(gnome_dialog_new(_("Zapping Properties"),
					 GNOME_STOCK_BUTTON_OK,
					 GNOME_STOCK_BUTTON_APPLY,
					 GNOME_STOCK_BUTTON_CANCEL,
					 GNOME_STOCK_BUTTON_HELP,
					 NULL));

  /* We need this later on */
  apply_button = GTK_WIDGET(g_list_nth_data(dialog->buttons, APPLY_ID));
  register_widget(apply_button, "apply");

  gtk_widget_set_sensitive(apply_button, FALSE);

  gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, TRUE, FALSE);

  gnome_dialog_set_default(dialog, OK_ID);
  gnome_dialog_set_parent(dialog, GTK_WINDOW(main_window));
  gnome_dialog_close_hides(dialog, FALSE); /* destroy on close */
  gnome_dialog_set_close(dialog, FALSE);

  /* Connect the appropiate callbacks */
  gtk_signal_connect(GTK_OBJECT(dialog), "close",
		     GTK_SIGNAL_FUNC(on_properties_close),
		     NULL);
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

  PropertiesDialog = dialog;

  /* Build the rest of the dialog */
  build_properties_contents(dialog);

  /* Open the last selected group */
  open_properties_group(GTK_WIDGET(dialog), zcg_char(NULL, "last_group"));

  return GTK_WIDGET(dialog);
}

void
append_properties_group		(GnomeDialog	*dialog,
				 const gchar	*group)
{
  GtkWidget *button;
  GtkWidget *vbox = lookup_widget(GTK_WIDGET(dialog), "group-container");
  GtkWidget *contents;
  gchar *buf;

  buf = g_strdup_printf("group-button-%s", group);
  if (find_widget(GTK_WIDGET(dialog), buf))
    return; /* The given group already exists */

  button = gtk_button_new_with_label(group);
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, TRUE, 0);
  on_container_add(vbox, button, vbox);
  gtk_widget_show(button);
  register_widget(button, buf);
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
		     GTK_SIGNAL_FUNC(open_properties_group),
		     (gpointer)group);
  g_free(buf);

  buf = g_strdup_printf("group-contents-%s", group);
  contents = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), contents);
  gtk_object_set_data_full(GTK_OBJECT(contents), "group-name",
			   g_strdup(group),
			   g_free);
  /* Note that we don't show() contents */
  register_widget(contents, buf);
  g_free(buf);
}

void
append_properties_page		(GnomeDialog	*dialog,
				 const gchar	*group,
				 const gchar	*label,
				 GtkWidget	*pixmap,
				 GtkWidget	*page)
{
  gchar *buf = g_strdup_printf("group-contents-%s", group);
  GtkWidget *contents = lookup_widget(GTK_WIDGET(dialog), buf);
  GtkWidget *radio;
  GtkWidget *container = lookup_widget(contents, "group-container");
  GSList *group_list = gtk_object_get_data(GTK_OBJECT(container),
					   "group_list");
  GtkWidget *notebook = lookup_widget(GTK_WIDGET(dialog),
				      "properties-notebook");
  GtkWidget *vbox;
  GtkWidget *label_widget;
  guint page_id;

  radio = gtk_radio_button_new(group_list);
  gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(radio), FALSE);
  gtk_box_pack_start(GTK_BOX(contents), radio, FALSE, TRUE, 0);
  gtk_widget_show(page);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page, gtk_label_new(""));
  page_id = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), page);

  gtk_signal_connect(GTK_OBJECT(radio), "toggled",
		     GTK_SIGNAL_FUNC(on_radio_toggled),
		     GINT_TO_POINTER(page_id));

  group_list = gtk_radio_button_group(GTK_RADIO_BUTTON(radio));
  gtk_object_set_data(GTK_OBJECT(container), "group_list", group_list);

  vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(radio), vbox);
  if (pixmap)
    gtk_box_pack_start_defaults(GTK_BOX(vbox), pixmap);
  label_widget = gtk_label_new(label);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), label_widget);

  gtk_button_set_relief(GTK_BUTTON(radio), GTK_RELIEF_NONE);

  gtk_widget_show_all(radio);
  on_container_add(contents, radio, container);

  g_free(buf);

  buf = g_strdup_printf("group-%s-item-%s", group, label);
  register_widget(page, buf);
  g_free(buf);

  buf = g_strdup_printf("group-%s-item-%s-radio", group, label);
  register_widget(radio, buf);
  g_free(buf);
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

void
open_properties_group		(GtkWidget	*dialog,
				 const gchar	*group)
{
  gchar *buf = g_strdup_printf("group-contents-%s", group);
  GtkWidget *contents = find_widget(dialog, buf);
  GtkContainer *group_container = GTK_CONTAINER
    (lookup_widget(dialog, "group-container"));
  gint cur_group, cur_item;

  if (!contents)
    {
      g_warning("Group %s not found in the properties", group);
      return; /* Not found */
    }

  /* If the current selection is in a different group, switch to it */
  get_cur_sel(dialog, &cur_group, &cur_item);

  if (cur_group == -1 ||
      nth_group_contents(dialog, cur_group) != contents)
    gtk_container_foreach(GTK_CONTAINER(group_container),
			  GTK_SIGNAL_FUNC(show_hide_foreach),
			  contents);

  g_free(buf);
}

void
open_properties_page		(GtkWidget	*dialog,
				 const gchar	*group,
				 const gchar	*item)
{
  gchar *buf = g_strdup_printf("group-%s-item-%s-radio", group, item);
  GtkWidget *toggle = lookup_widget(GTK_WIDGET(dialog), buf);

  g_free(buf);

  if (!toggle)
    {
      g_warning("{%s, %s} not found in the properties", group, item);
      return;
    }

  open_properties_group(dialog, group);

  gtk_button_clicked(GTK_BUTTON(toggle));
}

void
standard_properties_add		(GnomeDialog	*dialog,
				 SidebarGroup	*groups,
				 gint		num_groups,
				 const gchar	*glade_file)
{
  gint i, j;

  for (i = 0; i<num_groups; i++)
    {
      append_properties_group(dialog, _(groups[i].label));

      for (j = 0; j < groups[i].num_items; j++)
	{
	  GtkWidget *pixmap;
	  GtkWidget *page;

	  if (groups[i].items[j].icon_source ==	ICON_ZAPPING)
	    {
	      pixmap = z_load_pixmap (groups[i].items[j].icon_name);
	    }
	  else
	    {
	      gchar *pixmap_path = g_strdup (gnome_pixmap_file
	      	(groups[i].items[j].icon_name)); /* FIXME: leak?? */

	      pixmap = z_pixmap_new_from_file (pixmap_path);

	      g_free(pixmap_path);
	    }

	  page = build_widget(groups[i].items[j].widget, glade_file);

	  gtk_object_set_data(GTK_OBJECT(page), "apply",
			      groups[i].items[j].apply);
	  gtk_object_set_data(GTK_OBJECT(page), "help",
			      groups[i].items[j].help);
	  gtk_object_set_data(GTK_OBJECT(page), "cancel",
			      groups[i].items[j].cancel);

	  append_properties_page(dialog, _(groups[i].label),
				 _(groups[i].items[j].label),
				 pixmap, page);

	  groups[i].items[j].setup(page);
	}
    }
}

static void
apply				(GnomeDialog	*dialog,
				 GtkWidget	*page)
{
  void (*page_apply)(GtkWidget *page) =
    gtk_object_get_data(GTK_OBJECT(page), "apply");

  g_assert(page_apply != NULL);

  page_apply(page);
}

static void
help		(GnomeDialog	*dialog,
		 GtkWidget	*page)
{
  void (*page_help)(GtkWidget *page) =
    gtk_object_get_data(GTK_OBJECT(page), "help");

  if (page_help)
    page_help(page);
  else
    ShowBox("No help written yet",
	    GNOME_MESSAGE_BOX_WARNING);
}

static void
cancel		(GnomeDialog	*dialog,
		 GtkWidget	*page)
{
  void (*page_cancel)(GtkWidget *page) =
    gtk_object_get_data(GTK_OBJECT(page), "cancel");

  if (page_cancel)
    page_cancel(page);
}

void prepend_property_handler (property_handler *p)
{
  gint i;

  if (!p->add)
    g_error("broken handler");
    
  handlers = g_realloc(handlers,
		       (num_handlers+1)*sizeof(handlers[0]));
  /* Move the existing handlers forward */
  for (i=num_handlers; i>0; i--)
    memcpy(&handlers[i], &handlers[i-1], sizeof(*p));
  memcpy(&handlers[0], p, sizeof(*p));

  /* When not given, the default handlers are used (valid only when
     the page is built with standard_properties_add) */

  if (!handlers[0].apply)
    handlers[0].apply = apply;

  if (!handlers[0].help)
    handlers[0].help = help;

  if (!handlers[0].cancel)
    handlers[0].cancel = cancel;

  num_handlers++;
}

void append_property_handler (property_handler *p)
{
  if (!p->add)
    g_error("passing in a broken handler");
    
  handlers = g_realloc(handlers, (num_handlers+1)*sizeof(handlers[0]));
  memcpy(&handlers[num_handlers], p, sizeof(*p));

  /* When not given, the default handlers are used (valid only when
     the page is built with standard_properties_add) */

  if (!handlers[num_handlers].apply)
    handlers[num_handlers].apply = apply;

  if (!handlers[num_handlers].help)
    handlers[num_handlers].help = help;

  if (!handlers[num_handlers].cancel)
    handlers[num_handlers].cancel = cancel;

  num_handlers++;
}

void
on_propiedades1_activate               (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
  GtkWidget *properties = build_properties_dialog();

  gtk_widget_set_sensitive(GTK_WIDGET(menuitem), FALSE);

  gtk_signal_connect_object(GTK_OBJECT(properties), "close",
			    GTK_SIGNAL_FUNC(gtk_widget_set_sensitive),
			    GTK_OBJECT(menuitem));

  gnome_dialog_run(GNOME_DIALOG(properties));
}

void
startup_properties(void)
{
  zcc_char(_("General Options"), "Selected properties group", "last_group");
}

void shutdown_properties(void)
{
  g_free(handlers);
  handlers = NULL;
  num_handlers = 0;
}
