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

/* Register a set of callbacks to manage the properties */
void register_properties_handler (property_handler *p);

#endif /* properties.h */
