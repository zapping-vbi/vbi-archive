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

/* XXX gtk+ 2.3 GtkOptionMenu, Gnome entry, font picker, color picker */
#undef GTK_DISABLE_DEPRECATED
#undef GNOME_DISABLE_DEPRECATED

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#define ZCONF_DOMAIN "/zapping/internal/properties/"
#include "zconf.h"
#include "interface.h"
#include "properties.h"
#include "zmisc.h"
#include "remote.h"

extern GtkWidget * main_window;

static GtkDialog *PropertiesDialog = NULL; /* Only you.. */
static property_handler *handlers = NULL;
static gint num_handlers = 0;

/**
 * Signals that the given page has been modified, mark the page as
 * dirty and set Apply sensitive.
 */
static void
page_modified			(GtkDialog	*dialog,
				 gint		page_id)
{
  GtkNotebook *notebook = GTK_NOTEBOOK
    (lookup_widget(GTK_WIDGET(dialog), "properties-notebook"));
  GtkWidget *page = gtk_notebook_get_nth_page(notebook, page_id);

  g_object_set_data(G_OBJECT(page), "properties-dirty",
		    GINT_TO_POINTER(TRUE));

  gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_APPLY, TRUE);
}

static void
modify_page			(GtkWidget	*widget,
				 gpointer	page_id_ptr)
{
  GtkDialog *dialog = GTK_DIALOG
    (g_object_get_data(G_OBJECT(widget), "modify_page_dialog"));
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

  page_modified(GTK_DIALOG(gtk_widget_get_toplevel(notebook)),
		gtk_notebook_page_num(GTK_NOTEBOOK(notebook), widget));
}

static void
font_set_bridge	(GtkWidget	*widget,
		 const gchar	*new_font,
		 gpointer	page_id_ptr)
{
  GtkDialog *dialog = GTK_DIALOG
    (g_object_get_data(G_OBJECT(widget), "modify_page_dialog"));
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
  GtkDialog *dialog = GTK_DIALOG
    (g_object_get_data(G_OBJECT(widget), "modify_page_dialog"));
  gint page_id = GPOINTER_TO_INT(page_id_ptr);

  page_modified(dialog, page_id);
}

/**
 * Makes modifications on the widgets descending from widget trigger a
 * page_modifed call.
 */
static void
autoconnect_modify		(GtkDialog	*dialog,
				 GtkWidget	*widget,
				 gint		page_id)
{
  if (GTK_IS_CONTAINER(widget))
    {
      GList *children = gtk_container_get_children(GTK_CONTAINER(widget));
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
      g_signal_connect(G_OBJECT(widget), "toggled",
		       G_CALLBACK(modify_page),
		       GINT_TO_POINTER(page_id));
    }
  else if (GTK_IS_ENTRY(widget))
    {
      g_signal_connect(G_OBJECT(widget), "changed",
		       G_CALLBACK(modify_page),
		       GINT_TO_POINTER(page_id));      
    }
  else if (GTK_IS_OPTION_MENU(widget))
    {
      g_signal_connect(G_OBJECT(widget), "changed",
		       G_CALLBACK(modify_page),
		       GINT_TO_POINTER(page_id));      
    }
  else if (GNOME_IS_FONT_PICKER(widget))
    {
      g_signal_connect(G_OBJECT(widget), "font-set",
		       G_CALLBACK(font_set_bridge),
		       GINT_TO_POINTER(page_id));
    }
  else if (GNOME_IS_COLOR_PICKER(widget))
    {
      g_signal_connect(G_OBJECT(widget), "color-set",
		       G_CALLBACK(color_set_bridge),
		       GINT_TO_POINTER(page_id));
    }
  else if (GTK_IS_RANGE(widget))
    {
      /* Weird cast to spare some useless code */
      widget = (GtkWidget*)gtk_range_get_adjustment(GTK_RANGE(widget));
      g_signal_connect(G_OBJECT(widget),
		       "value-changed",
		       G_CALLBACK(modify_page),
		       GINT_TO_POINTER(page_id));
    }

  g_object_set_data(G_OBJECT(widget), "modify_page_dialog", dialog);
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
      (g_list_index(gtk_container_get_children(group_container),
		    widget)-1)/2;
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

  return GTK_WIDGET((g_list_nth_data
		     (gtk_container_get_children(group_container),
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
			(GtkCallback)find_selected_group,
			group);

  if (*group == -1)
    return; /* Nothing shown yet */
  
  group_widget = nth_group_contents(dialog, *group);
  group_list = g_object_get_data(G_OBJECT(group_widget), "group_list");

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
generic_apply			(GtkDialog	*dialog)
{
  gint i = 0;
  GtkWidget *page;
  GtkNotebook *notebook =
    GTK_NOTEBOOK(lookup_widget(GTK_WIDGET(dialog), "properties-notebook"));

  while ((page = gtk_notebook_get_nth_page(notebook, i++)))
    if (g_object_get_data(G_OBJECT(page), "properties-dirty"))
      {
	property_handler *handler = (property_handler*)
	  g_object_get_data(G_OBJECT(page), "property-handler");

	handler->apply(dialog, page);

	g_object_set_data(G_OBJECT(page), "properties-dirty", NULL);
      }
}

static void
generic_cancel			(GtkDialog	*dialog)
{
  gint i = 0;
  GtkWidget *page;
  GtkNotebook *notebook =
    GTK_NOTEBOOK(lookup_widget(GTK_WIDGET(dialog), "properties-notebook"));

  while ((page = gtk_notebook_get_nth_page(notebook, i++)))
    {
      property_handler *handler = (property_handler*)
	g_object_get_data(G_OBJECT(page), "property-handler");

      if (handler && handler->cancel)
	handler->cancel(dialog, page);

      g_object_set_data(G_OBJECT(page), "properties-dirty", NULL);
    }
}

static void
generic_help			(GtkDialog	*dialog)
{
  GtkNotebook *notebook =
    GTK_NOTEBOOK(lookup_widget(GTK_WIDGET (dialog), "properties-notebook"));
  gint cur_page = gtk_notebook_get_current_page(notebook);
  GtkWidget *page = gtk_notebook_get_nth_page(notebook, cur_page);
  property_handler *handler = (property_handler*)
    g_object_get_data(G_OBJECT(page), "property-handler");

  if (!handler)
    {
      /* All pages must have a handler except the first (Z logo) */
      g_assert(cur_page == 0);
      return;
    }

  handler->help(dialog, page);
}

static void
generic_close			(GtkWidget	*dialog)
{
  gint cur_group, cur_item;
  gchar *group_name = NULL;

  /* Remember last open group */
  get_cur_sel(dialog, &cur_group, &cur_item);

  if (cur_group != -1 &&
      (group_name = g_object_get_data(G_OBJECT
				      (nth_group_contents(dialog, cur_group)), "group-name")))
    zcs_char(group_name, "last_group");

  PropertiesDialog = NULL;
}

static void
on_properties_response		(GtkDialog	*dialog,
				 gint		response,
				 gpointer	unused)
{
  switch (response)
    {
    case GTK_RESPONSE_APPLY:
      generic_apply (dialog);
      gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_APPLY, FALSE);
      return; /* Do not destroy */
    case GTK_RESPONSE_HELP:
      generic_help (dialog);
      return; /* Do not destroy */
    case GTK_RESPONSE_OK:
      generic_apply (dialog);
      break;
    case GTK_RESPONSE_CANCEL:
      generic_cancel (dialog);
      break;
    default:
      /* Some sort of delete event or similar */
      break;
    }

  /* Remember settings and close */
  generic_close (GTK_WIDGET (dialog));
  gtk_widget_destroy (GTK_WIDGET (dialog));
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
    (g_object_get_data(G_OBJECT(parent), "req_max_width"));

  gtk_widget_size_request(widget, &request);
  if (request.width > req_max_width)
    {
      req_max_width = request.width;
      g_object_set_data(G_OBJECT(parent), "req_max_width",
			GINT_TO_POINTER(req_max_width));
      gtk_widget_set_size_request(parent, req_max_width, -1);
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
    (g_object_get_data(G_OBJECT(sidebar), "req_button_height"));
  /* Biggest subgroup allocated */
  gint req_max_height = GPOINTER_TO_INT
    (g_object_get_data(G_OBJECT(sidebar), "req_max_height"));

  /* Adding a new group */
  if (container == sidebar && GTK_IS_BUTTON(widget))
    {
      gtk_widget_size_request(widget, &request);

      req_button_height += request.height;
      g_object_set_data(G_OBJECT(sidebar), "req_button_height",
			GINT_TO_POINTER(req_button_height));
      gtk_widget_set_size_request(sidebar, -1,
				  req_button_height + req_max_height);
    }
  /* Adding a new item to a group */
  else if (GTK_IS_BUTTON(widget))
    {
      /* Get size request for the parent container */
      gtk_widget_size_request(container, &request);

      if (req_max_height < request.height)
	{
	  req_max_height = request.height;
	  g_object_set_data(G_OBJECT(sidebar), "req_max_height",
			    GINT_TO_POINTER(req_max_height));
	  gtk_widget_set_size_request(sidebar, -1, req_button_height +
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

  gtk_notebook_set_current_page(notebook, page_id);
}

static void
build_properties_contents	(GtkDialog	*dialog)
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
  register_widget(NULL, vbox, "group-container");

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
  register_widget(NULL, GTK_WIDGET(notebook), "properties-notebook");

  /* Put our logo when nothing is selected yet */
  if ((logo = z_load_pixmap ("logo.png")))
    {
      GtkWidget *box;
      /* GdkColor color; */

      box = gtk_frame_new (NULL);
      /* all i wanted was white bg and all i got was this lousy comment */
      /* gdk_color_parse ("#FFFFFF", &color); */
	 /* gtk_widget_modify_bg (box, GTK_STATE_NORMAL, &color); */
      gtk_container_add (GTK_CONTAINER (box), logo);
      gtk_misc_set_alignment (GTK_MISC (logo), 1, 1);

      gtk_notebook_append_page(notebook, box, gtk_label_new(""));
      page_count ++; /* No handler for this page */
    }

  gtk_widget_show_all(hbox);

  /* Add the nothing selected button */
  nsbutton = gtk_radio_button_new(NULL);
  gtk_box_pack_start(GTK_BOX(vbox), nsbutton, FALSE, TRUE, 0);
  g_object_set_data(G_OBJECT(vbox), "group_list",
		    gtk_radio_button_get_group(GTK_RADIO_BUTTON(nsbutton)));

  /* Make property handlers build their pages */
  for (i = 0; i<num_handlers; i++)
    {
      GtkWidget *page;
      handlers[i].add(dialog);
      while ((page = gtk_notebook_get_nth_page(notebook, page_count++)))
	{
	  g_object_set_data(G_OBJECT(page), "property-handler",
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
  GtkDialog *dialog;
  GtkWidget *menuitem;

  if (PropertiesDialog)
    {
      gtk_window_present (GTK_WINDOW (PropertiesDialog));
      return GTK_WIDGET(PropertiesDialog);
    }

  dialog = GTK_DIALOG(gtk_dialog_new_with_buttons
		      (_("Zapping Properties"),
		       GTK_WINDOW (main_window),
		       GTK_DIALOG_DESTROY_WITH_PARENT,
		       GTK_STOCK_OK, GTK_RESPONSE_OK,
		       GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
		       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		       GTK_STOCK_HELP, GTK_RESPONSE_HELP,
		       NULL));

  if (main_window)
    {
      menuitem = lookup_widget (main_window, "propiedades1");
      gtk_widget_set_sensitive (menuitem, FALSE);
      g_signal_connect_swapped (G_OBJECT (dialog), "destroy",
				G_CALLBACK (gtk_widget_set_sensitive),
				menuitem);
    }

  gtk_dialog_set_response_sensitive(dialog, GTK_RESPONSE_APPLY, FALSE);

  gtk_dialog_set_default_response (dialog, GTK_RESPONSE_OK);

  /* Connect the appropiate callbacks */
  g_signal_connect (G_OBJECT (dialog), "response",
		    G_CALLBACK (on_properties_response),
		    NULL);

  PropertiesDialog = dialog;

  /* Build the rest of the dialog */
  build_properties_contents(dialog);

  /* Open the last selected group */
  open_properties_group(GTK_WIDGET(dialog), zcg_char(NULL, "last_group"));

  return GTK_WIDGET(dialog);
}

void
append_properties_group		(GtkDialog	*dialog,
				 const gchar	*group,
				 const gchar *	 group_i18n)
{
  GtkWidget *button;
  GtkWidget *vbox = lookup_widget(GTK_WIDGET(dialog), "group-container");
  GtkWidget *contents;
  gchar *buf;

  buf = g_strdup_printf("group-button-%s", group);
  if (find_widget(GTK_WIDGET(dialog), buf))
    return; /* The given group already exists */

  button = gtk_button_new_with_label(group_i18n);
  gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, TRUE, 0);
  on_container_add(vbox, button, vbox);
  gtk_widget_show(button);
  register_widget(NULL, button, buf);
  g_signal_connect(G_OBJECT(button), "clicked",
		   G_CALLBACK(open_properties_group),
		   (gpointer)group);
  g_free(buf);

  buf = g_strdup_printf("group-contents-%s", group);
  contents = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), contents);
  g_object_set_data_full(G_OBJECT(contents), "group-name",
			 g_strdup(group),
			 g_free);
  /* Note that we don't show() contents */
  register_widget(NULL, contents, buf);
  g_free(buf);
}

void
append_properties_page		(GtkDialog	*dialog,
				 const gchar	*group,
				 const gchar	*label,
				 GtkWidget	*pixmap,
				 GtkWidget	*page)
{
  gchar *buf = g_strdup_printf("group-contents-%s", group);
  GtkWidget *contents = lookup_widget(GTK_WIDGET(dialog), buf);
  GtkWidget *radio;
  GtkWidget *container = lookup_widget(contents, "group-container");
  GSList *group_list = g_object_get_data(G_OBJECT(container),
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

  g_signal_connect(G_OBJECT(radio), "toggled",
		   G_CALLBACK(on_radio_toggled),
		   GINT_TO_POINTER(page_id));

  group_list = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio));
  g_object_set_data(G_OBJECT(container), "group_list", group_list);

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
  register_widget(NULL, page, buf);
  g_free(buf);

  buf = g_strdup_printf("group-%s-item-%s-radio", group, label);
  register_widget(NULL, radio, buf);
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
			  (GtkCallback)show_hide_foreach,
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
standard_properties_add		(GtkDialog	*dialog,
				 SidebarGroup	*groups,
				 gint		num_groups,
				 const gchar	*glade_file)
{
  gint i, j;

  for (i = 0; i<num_groups; i++)
    {
      append_properties_group(dialog, groups[i].label, _(groups[i].label));

      for (j = 0; j < groups[i].num_items; j++)
	{
	  GtkWidget *pixmap;
	  GtkWidget *page;

	  pixmap = z_load_pixmap (groups[i].items[j].icon_name);

	  page = build_widget(groups[i].items[j].widget, glade_file);

	  g_object_set_data(G_OBJECT(page), "apply",
			    groups[i].items[j].apply);
	  g_object_set_data(G_OBJECT(page), "help",
			    groups[i].items[j].help);
	  g_object_set_data(G_OBJECT(page), "help_link_id",
			    (gpointer) groups[i].items[j].help_link_id);
	  g_object_set_data(G_OBJECT(page), "cancel",
			    groups[i].items[j].cancel);

	  append_properties_page(dialog, /* no i18n */ groups[i].label,
				 _(groups[i].items[j].label),
				 pixmap, page);

	  groups[i].items[j].setup(page);
	}
    }
}

static void
apply				(GtkDialog	*dialog,
				 GtkWidget	*page)
{
  void (*page_apply)(GtkWidget *page) =
    g_object_get_data(G_OBJECT(page), "apply");

  g_assert(page_apply != NULL);

  page_apply(page);
}

static void
help		(GtkDialog	*dialog,
		 GtkWidget	*page)
{
  const gchar *link_id =
    g_object_get_data (G_OBJECT (page), "help_link_id");
  void (*page_help)(GtkWidget *page) =
    g_object_get_data (G_OBJECT (page), "help");

  if (link_id)
    /* XXX handle error */
    gnome_help_display ("zapping", link_id, NULL);
  else if (page_help)
    page_help (page);
  else
    ShowBox ("No help available", GTK_MESSAGE_WARNING);
}

static void
cancel		(GtkDialog	*dialog,
		 GtkWidget	*page)
{
  void (*page_cancel)(GtkWidget *page) =
    g_object_get_data(G_OBJECT(page), "cancel");

  if (page_cancel)
    page_cancel(page);
}

void prepend_property_handler (const property_handler *p)
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

void append_property_handler (const property_handler *p)
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

static PyObject *
py_properties(PyObject *self, PyObject *args, PyObject *keywds)
{
  char *group = NULL;
  char *item = NULL;
  char *kwlist[] = {"group", "item", NULL};
  GtkWidget *dialog;
  
  if (!PyArg_ParseTupleAndKeywords(args, keywds, "|ss", kwlist, 
				   &group, &item))
    g_error ("zapping.properties(|ss)");

  if (item && !group)
    g_error ("Cannot open an item without knowing the group");

  dialog = build_properties_dialog ();

  if (group)
    {
      if (item)
	open_properties_page (dialog, group, item);
      else
	open_properties_group (dialog, group);
    }

  gtk_widget_show (dialog);

  Py_INCREF(Py_None);

  return Py_None;
}

void
startup_properties(void)
{
  zcc_char(_("General Options"), "Selected properties group", "last_group");
  cmd_register ("properties", (PyCFunction) py_properties,
		METH_VARARGS | METH_KEYWORDS);
}

void shutdown_properties(void)
{
  g_free(handlers);
  handlers = NULL;
  num_handlers = 0;
}
