#ifndef __PROPERTIES_HANDLER_H__
#define __PROPERTIES_HANDLER_H__

guint
picture_sizes_append_menu	(GtkMenuShell *		menu);

gboolean
on_picture_size_key_press	(GtkWidget *		widget,
				 GdkEventKey *		event,
				 gpointer		user_data);

extern void startup_properties_handler(void);
extern void shutdown_properties_handler(void);

#endif
