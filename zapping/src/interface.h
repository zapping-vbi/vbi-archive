#ifndef __INTERFACE_H__
#define __INTERFACE_H__

/**
 * Finds in the tree the given widget, returns a pointer to it or NULL
 * if not found
 */
GtkWidget *
find_widget (GtkWidget * parent, const gchar * name);

/*
 * Tries to find a widget, that is accesible though parent, named
 * name. IMHO this should be called glade_lookup_widget and go into
 * libglade, but anyway...
 * If the widget isn't found, a message is printed and the program
 * quits, it always returns a valid widget.
 */
GtkWidget *
lookup_widget (GtkWidget * parent, const gchar * name);

/**
 * Registers a widget created by the app so lookup_widget finds it.
 */
void
register_widget (GtkWidget *parent, GtkWidget * widget, const char * name);

/**
 * Builds the given widget from the given glade file.
 */
GtkWidget *
build_widget (const gchar* name, const gchar* file);

/**
 * Builds the main window.
 */
GtkWidget *
create_zapping (void);

/**
 * Builds the context menu for the main window.
 */
GtkWidget*
create_popup_menu1 (GdkEventButton *	event);

#endif
