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
 * Useful structures and definitions for property handlers.
 */
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
  /* Apply the current config to the dialog */
  void		(*setup)(GtkWidget *widget);
  /* Apply the dialog settings to the config */
  void		(*apply)(GtkWidget *widget);
  /* Help about this page (or NULL) */
  void		(*help)(GtkWidget *widget);
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
#endif

#endif /* properties.h */
