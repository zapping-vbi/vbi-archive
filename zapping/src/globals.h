#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <gnome.h>
#include "tveng.h"
#include "frequencies.h"
#include "x11stuff.h"
#include "mixer.h"

extern enum tveng_capture_mode	last_mode;
extern GtkWidget		*ChannelWindow;
extern tveng_device_info	*main_info;
extern volatile gboolean	flag_exit_program;
extern GList			*plugin_list;
/* preview should be disabled */
extern gint			disable_preview;
/* XVideo should be disabled */
extern gint			disable_xv;
/* Whether the device can be attached as XV */
extern gboolean			xv_present;
extern gint			xv_overlay_port;
extern GtkWidget		*main_window;
/* Will be TRUE if when quitting we were fullscreen */
extern gboolean			was_fullscreen;
extern tveng_tuned_channel *	global_channel_list;
extern x11_vidmode_info *	vidmodes;
extern gboolean			have_wm_hints;
extern tv_overlay_buffer	dga_param;
extern int			debug_msg;
extern gint			cur_tuned_channel;
/* XXX Move this into virtual device context when ready. */
extern tv_mixer *		mixer;
extern tv_audio_line *		mixer_line;
#endif
