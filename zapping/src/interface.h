#ifndef __INTERFACE_H__
#define __INTERFACE_H__

/*
 * Tries to find a widget, that is accesible though parent, named
 * name. IMHO this should be called glade_lookup_widget and go into
 * libglade, but anyway...
 */
GtkWidget*
lookup_widget(GtkWidget * parent, const char * name);

/*
 * Loads a GtkWidget from zapping.glade. All the memory is freed when
 * the object (widget) is destroyed. If name is NULL, all widgets are
 * loaded, but this is not recommended.
 */
GtkWidget*
build_widget(const char* name);
GtkWidget* create_zapping (void);
GtkWidget* create_channel_window (void);
GtkWidget* create_zapping_properties (void);
GtkWidget* create_about2 (void);
GtkWidget* create_plugin_properties (void);
GtkWidget* create_popup_menu1 (void);

#endif
