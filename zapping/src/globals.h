#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#include <gconf/gconf-client.h>
#include "tveng.h"
#include "frequencies.h"
#include "x11stuff.h"
#include "mixer.h"
#include "zapping.h"

extern Zapping *	zapping;

display_mode		last_dmode;
capture_mode		last_cmode;
extern GtkWidget		*ChannelWindow;
extern volatile gboolean	flag_exit_program;
extern GList			*plugin_list;
/* Whether the device can be attached as XV */
extern gboolean			xv_present;
extern GConfClient *		gconf_client;

/* XVideo should be disabled */
extern gint			disable_xv;
extern gint			disable_xv_video;
extern gint			disable_xv_image;

extern gint			xv_video_port;
extern gint			xv_image_port;

extern gint			disable_overlay;

/* Will be TRUE if when quitting we were fullscreen */
extern gboolean			was_fullscreen;
extern tveng_tuned_channel *	global_channel_list;
extern tv_screen *		screens;
extern x11_vidmode_info *	vidmodes;
extern gboolean			have_wm_hints;
extern int			debug_msg;
extern int			io_debug_msg;
extern gint			cur_tuned_channel;
/* XXX Move this into virtual device context when ready. */
extern tv_mixer *		mixer;
extern tv_audio_line *		mixer_line;
#endif
