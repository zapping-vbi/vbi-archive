#include "globals.h"

enum tveng_capture_mode	last_mode;
GtkWidget		*ChannelWindow;
tveng_device_info	*main_info = NULL;
volatile gboolean	flag_exit_program = FALSE;
GList			*plugin_list = NULL;
/* preview should be disabled */
gint			disable_preview = FALSE;
/* Whether the device can be attached as XV */
gboolean		xv_present = FALSE;

/* Disable XVideo support */
gint			disable_xv		= FALSE;
gint			disable_xv_video	= FALSE;
gint			disable_xv_image	= FALSE;

/* XXX actually XvPortID xv_port = None; */
gint			xv_video_port		= -1;
gint			xv_image_port		= -1;

/* Disable Xv and V4L overlay */
gint			disable_overlay		= FALSE;

GtkWidget		*main_window = NULL;
/* Will be TRUE if when quitting we were fullscreen */
gboolean		was_fullscreen = FALSE;
tveng_tuned_channel *	global_channel_list = NULL;
x11_vidmode_info *	vidmodes;
/* TRUE if we can tell the WM to keep the video window on top */
gboolean		have_wm_hints = FALSE;
tv_overlay_buffer	dga_param;
int			debug_msg = 0;
int			io_debug_msg = 0;
gint			cur_tuned_channel = -1;
/* XXX Move this into virtual device context when ready. */
tv_mixer *		mixer = NULL;
tv_audio_line *		mixer_line = NULL;
