#ifndef __PROPERTIES_H__
#define __PROPERTIES_H__

#include <gtk/gtk.h>

/* A module handling a toplevel property page (new version) */
typedef struct {
  /* Add a property page to the properties dialog */
  void (*add) ( GtkDialog *dialog );
  /* Called when the OK or Apply buttons are pressed for a modified
     page. @page is the modified page */
  void (*apply) ( GtkDialog *dialog, GtkWidget *page );
  /* Called when the help button is pressed */
  void (*help) ( GtkDialog *dialog, GtkWidget *page );
  /* Called when the Cancel button is pressed for a modified
     page. @page is the modified page */
  void (*cancel) ( GtkDialog *dialog, GtkWidget *page );
} property_handler;

/* Register a set of callbacks to manage the properties */
/* prepend/append refer to the order in which the handlers are called
   to build the properties */
void prepend_property_handler (const property_handler *p);
void append_property_handler (const property_handler *p);

/**
 * Create a group with the given name.
 * @group: Name for the group.
 */
void
append_properties_group		(GtkDialog	*dialog,
				 const gchar	*group,
				 const gchar * group_i18n);

/**
 * Sets up a properties dialog.
 */
GtkWidget *
build_properties_dialog		(void);

/**
 * Opens a created group.
 * @group: Name you gave the group when creating it (not localized).
 */
void
open_properties_group		(GtkWidget	*dialog,
				 const gchar	*group);

/**
 * Opens the given property page.
 */
void
open_properties_page		(GtkWidget	*dialog,
				 const gchar	*group,
				 const gchar	*item);

/**
 * Appends to the dialog group the given page.
 * @group: Name of the group this page belongs to (not localized).
 * @label: Name you wish to give to this group.
 * @pixmap: Pixmap to show with the label, can be %NULL.
 * @page: The page contents.
 */
void
append_properties_page		(GtkDialog	*dialog,
				 const gchar	*group,
				 const gchar	*label,
				 GtkWidget	*pixmap,
				 GtkWidget	*page);

/**
 * @widget: Any widget in the dialog that contains the requested page.
 * @group: Group containing the desired item.
 * @item: name of the item to look for.
 * Returns the page associated with the given @group and @item, or %NULL
 * on error.
 */
GtkWidget *
get_properties_page		(GtkWidget	*widget,
				 const gchar	*group,
				 const gchar	*item);

/**
 * Call this when some widget in the property box is changed.
 * Usually you don't need to do this, since widgets are autoconnected
 * when you add the page. This is only needed when you add widgets to your
 * property page after adding it to the dialog (dynamic dialogs, for
 * example).
 * @widget: The modified widget.
 */
void
z_property_item_modified	(GtkWidget	*widget);

/**
 * Startup/shutdown
 */
void startup_properties(void);
void shutdown_properties(void);

/**
 * Useful structures and definitions for property handlers.
 * This isn't required (you can always add pages by hand), but using
 * these routines/structs helps a big deal.
 */
typedef struct {
  /* Label for the entry */
  const gchar	*label;
  const gchar	*icon_name; /* relative to the pixmaps installation dir */
  const gchar	*widget; /* Notebook page to use (must be in zapping.glade) */
  /* Apply the current config to the dialog */
  void		(*setup)(GtkWidget *widget);
  /* Apply the dialog settings to the config, if NULL, a default
     handler will be used (only valid when created with
     standard_properties_add) */
  void		(*apply)(GtkWidget *widget);
  /* Help about this page (or NULL) */
  void		(*help)(GtkWidget *widget);
  /* See apply */
  void		(*cancel)(GtkWidget *widget);
  /* Link id in zapping.xml, instead of help handler. */
  const gchar *help_link_id;
  /* Function to create preferences instead of glade file name
     in standard_properties_add(), .widget and .setup above. */
  GtkWidget *	(*create)(void);
} SidebarEntry;

typedef struct {
  /* Label */
  const gchar	*label;
  /* Contents of the group */
  SidebarEntry	*items;
  /* Number of entries in the group */
  gint num_items;
} SidebarGroup;

/**
 * Add the groups/items that groups contains.
 * @dialog: The properties dialog.
 * @groups: List of groups/items to add.
 * @num_groups: Groups to add from group, typically acount(groups)
 * @glade_file: glade file containing the widgets.
 */
void
standard_properties_add		(GtkDialog	*dialog,
				 const SidebarGroup *	groups,
				 gint		num_groups,
				 const gchar	*glade_file);

#endif /* properties.h */
