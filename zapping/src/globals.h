#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <gnome.h>
#include "tveng.h"
#include "frequencies.h"

extern enum tveng_capture_mode	last_mode;
extern GtkWidget		*ChannelWindow;
extern tveng_device_info	*main_info;
extern volatile gboolean	flag_exit_program;
extern tveng_rf_table		*current_country;
extern GList			*plugin_list;
/* preview should be disabled */
extern gint			disable_preview;
/* XVideo should be disabled */
extern gint			disable_xv;
/* Whether the device can be attached as XV */
extern gboolean			xv_present;
extern GtkWidget		*main_window;
/* Will be TRUE if when quitting we were fullscreen */
extern gboolean			was_fullscreen;
extern tveng_tuned_channel	*global_channel_list;
extern gboolean			have_wmhooks;
extern int			debug_msg;
extern gint			cur_tuned_channel;
#endif
