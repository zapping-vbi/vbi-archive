#include "globals.h"

enum tveng_capture_mode	last_mode;
GtkWidget		*ChannelWindow;
tveng_device_info	*main_info = NULL;
volatile gboolean	flag_exit_program = FALSE;
GList			*plugin_list = NULL;
/* preview should be disabled */
gint			disable_preview = FALSE;
/* XVideo should be disabled */
gint			disable_xv = FALSE;
/* Whether the device can be attached as XV */
gboolean		xv_present = FALSE;
gint			xv_overlay_port = -1;
GtkWidget		*main_window = NULL;
/* Will be TRUE if when quitting we were fullscreen */
gboolean		was_fullscreen = FALSE;
tveng_tuned_channel *	global_channel_list = NULL;
x11_vidmode_info *	vidmodes;
/* TRUE if we can tell the WM to keep the video window on top */
gboolean		have_wm_hints = FALSE;
x11_dga_parameters	dga_param;
int			debug_msg = 0;
gint			cur_tuned_channel = -1;
