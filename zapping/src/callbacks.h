#ifndef __CALLBACKS_H__
#define __CALLBACKS_H__

void on_zapping_delete_event (GtkWidget *, gpointer);

gboolean
on_tv_screen_button_press_event        (GtkWidget       *widget,
					GdkEvent        *event,
					gpointer        user_data);

#endif
