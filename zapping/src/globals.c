#include "globals.h"

enum tveng_capture_mode	last_mode;
GtkWidget		*ChannelWindow;
tveng_device_info	*main_info = NULL;
volatile gboolean	flag_exit_program = FALSE;
tveng_rf_table		*current_country = NULL;
GList			*plugin_list = NULL;
/* preview should be disabled */
gint			disable_preview = FALSE;
/* XVideo should be disabled */
gint			disable_xv = FALSE;
/* Whether the device can be attached as XV */
gboolean		xv_present = FALSE;
GtkWidget		*main_window = NULL;
/* Will be TRUE if when quitting we were fullscreen */
gboolean		was_fullscreen = FALSE;
tveng_tuned_channel	*global_channel_list = NULL;
gboolean		have_wmhooks = FALSE;
int			debug_msg = 0;
gint			cur_tuned_channel = -1;
