#ifndef __PROPERTIES_H__
#define __PROPERTIES_H__

/* A module handling a toplevel property page */
typedef struct {
  /* Add a property page to the properties dialog */
  void (*add) ( GnomePropertyBox * gpb );
  /* Called when the OK or Apply buttons are pressed */
  gboolean (*apply) ( GnomePropertyBox * gpb, gint page );
  /* Called when the help button is pressed */
  gboolean (*help) ( GnomePropertyBox * gpb, gint page );
} property_handler;

/* A module handling a toplevel property page (new version) */
typedef struct {
  /* Add a property page to the properties dialog */
  void (*add) ( GnomeDialog *dialog );
  /* Called when the OK or Apply buttons are pressed for a modified
     page. @page is the modified page */
  void (*apply) ( GnomeDialog *dialog, GtkWidget *page );
  /* Called when the help button is pressed */
  void (*help) ( GnomeDialog *dialog, GtkWidget *page );
} property_handler2;

/* Register a set of callbacks to manage the properties */
void register_properties_handler (property_handler *p);
void register_property_handler2 (property_handler2 *p);

/**
 * Create a group with the given name.
 * @group: Name for the group.
 */
void
append_properties_group		(GnomeDialog	*dialog,
				 const gchar	*group);

/**
 * Opens a created group.
 * @group: Name you gave the group when creating it.
 */
void
open_properties_group		(GtkWidget	*dialog,
				 const gchar	*group);

/**
 * Appends to the dialog group the given page.
 * @group: Name of the group this page belongs to.
 * @label: Name you wish to give to this group.
 * @pixmap: Pixmap to show with the label, can be %NULL.
 * @page: The page contents.
 */
void
append_properties_page		(GnomeDialog	*dialog,
				 const gchar	*group,
				 const gchar	*label,
				 GtkWidget	*pixmap,
				 GtkWidget	*page);

#endif /* properties.h */
