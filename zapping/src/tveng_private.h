/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 Iñaki García Etxebarria
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
 * Private stuff, we can play freely with this without losing binary
 * or source compatibility.
 */
#ifndef __TVENG_PRIVATE_H__
#define __TVENG_PRIVATE_H__
#include <tveng.h>
#include <pthread.h>

#include "common/intl-priv.h"
#include "libtv/misc.h"
#include "x11stuff.h"
#include "zmisc.h" /* preliminary */

/*
  Function prototypes for modules, NULL means not implemented or not
  pertinent.
  For the descriptions, see tveng.h
*/
struct tveng_module_info {
  int	(*attach_device)(const char* device_file,
			 Window window,
			 enum tveng_attach_mode  attach_mode,
			 tveng_device_info * info);
  void	(*describe_controller)(const char **short_str, const char **long_str,
			       tveng_device_info *info);
  void	(*close_device)(tveng_device_info *info);
	/*
	 */
	int		(* ioctl)		(tveng_device_info *,
						 unsigned int,
						 char *);


	/* Sets the current video input from info->video_inputs,
	   with all effects of get_video_input(). */
	tv_bool		(* set_video_input)	(tveng_device_info *,
						 const tv_video_line *);
	/* Reads the current video input from the device and stores
	   it in info->cur_video_input. May update info->video_standards
	   and info->cur_video_standards. */
	tv_bool		(* get_video_input)	(tveng_device_info *);
	/* Sets the frequency of a video input if a tuner, with all
	   effects of get_tuner_frequency(). */
	tv_bool		(* set_tuner_frequency)	(tveng_device_info *,
						 tv_video_line *,
						 unsigned int);
	/* Reads the current frequency of a video input and stores it
	   in the tv_video_line struct. */
	tv_bool		(* get_tuner_frequency)	(tveng_device_info *,
						 tv_video_line *);
	/* Sets the current video standard, with all effects of
	   get_standard(). */
	tv_bool		(* set_video_standard)	(tveng_device_info *,
						 const tv_video_standard *);
	/* Reads the current video standard from the device and updates
	   info->cur_video_standard. To update info->video_standards
	   call update_video_input. */
	tv_bool		(* get_video_standard)	(tveng_device_info *);
	/* Sets the current audio input from info->audio_inputs,
	   with all effects of get_audio_input(). */
	tv_bool		(* set_audio_input)	(tveng_device_info *,
						 const tv_audio_line *);
	/* Reads the current audio input from the device and stores
	   it in info->cur_audio_input. */
	tv_bool		(* get_audio_input)	(tveng_device_info *);
	/* Sets the value of a control, with all effects of get_control(). */
  	tv_bool		(* set_control)		(tveng_device_info *,
						 tv_control *,
						 int);
	/* Reads the current control value and stores it in tv_control.
	   If the control is NULL updates all controls. */
	tv_bool		(* get_control)		(tveng_device_info *,
						 tv_control *);

	tv_bool		(* set_audio_mode)	(tveng_device_info *,
						 tv_audio_mode);

  int	(*get_signal_strength)(int *strength, int *afc,
			       tveng_device_info *info);


  /* Device specific stuff */
  int	(*ov511_get_button_state)(tveng_device_info *info);

  /* size of the private data of the module */
  int	private_size;
};

struct capture_device {
	tv_image_format		format;

	/* Preliminary. If zero try set_format. */
	tv_pixfmt_set		supported_pixfmt_set;

	tv_bool			(* set_format)	(tveng_device_info *,
						 const tv_image_format *);
	tv_bool			(* get_format)	(tveng_device_info *);

  int	(*start_capturing)(tveng_device_info *info);
  int	(*stop_capturing)(tveng_device_info *info);
  int	(*read_frame)(tveng_image_data *where,
		      unsigned int time, tveng_device_info *info);
  double (*get_timestamp)(tveng_device_info *info);
};

struct overlay_device {
	tv_overlay_buffer	buffer;
	tv_window		window;

  	tv_clip_vector		clip_vector; /* 2 b removed */

	unsigned int		chromakey;

	tv_bool			active; /* XXX internal */

	tv_bool			(* set_buffer)	(tveng_device_info *,
						 const tv_overlay_buffer *);
	tv_bool			(* get_buffer)	(tveng_device_info *);
	tv_bool			(* set_window_clipvec)
       						(tveng_device_info *,
						 const tv_window *,
						 const tv_clip_vector *);
	tv_bool			(* set_window_chromakey)
						(tveng_device_info *,
						 const tv_window *,
						 unsigned int);
	tv_bool			(* set_xwindow)	(tveng_device_info *,
						 Window,
						 GC);
	tv_bool			(* get_window)	(tveng_device_info *);
	tv_bool			(* get_chromakey)(tveng_device_info *);
	tv_bool			(* enable)	(tveng_device_info *,
						 tv_bool);
};

/* The structure used to hold info about a video_device */
struct _tveng_device_info
{
  char * file_name; /* The name used to open() this fd */
  int fd; /* Video device file descriptor */
  capture_mode capture_mode; /* Current capture mode */
  enum tveng_attach_mode attach_mode; /* Mode this was attached with
				       */
  enum tveng_controller current_controller; /* Controller used */
  struct tveng_caps caps; /* Video system capabilities */


  struct capture_device capture;
  struct overlay_device overlay;

	/* All internal communication with the device is logged
	   through this fp when non-NULL. */
	FILE *			log_fp;

	/* Panel properties */

	/* Video inputs of the device, invariable. */
	tv_video_line *		video_inputs;
	/* Can be NULL only when no list exists. */
	tv_video_line *		cur_video_input;

	/* Audio inputs of the device, invariable. Not supported yet.
	   Need a function telling which video and audio inputs combine. */
	tv_audio_line *		audio_inputs;
	/* Can be NULL only when no list exists. */
	tv_audio_line *		cur_audio_input;

	/* Video standards supported by the current video input. Note
	   videostd_ids are bitwise mutually exclusive, i.e. no two listed
	   standards can have the same videostd_id bit set. */
	tv_video_standard *	video_standards;
	/* This can be NULL if we don't know. If it matters,
	   and video_standards is not NULL, clients should ask the user. */
	tv_video_standard *	cur_video_standard;

	/* Controls */
	tv_control *		controls;
	unsigned		audio_mutable : 1;

	/* Audio mode */
	tv_audio_capability	audio_capability;
	/* lang1/2: 0-none/unknown 1-mono 2-stereo */
	unsigned int		audio_reception[2];
	tv_audio_mode		audio_mode;


  /* Unique integer that indentifies this device */
  int signature;

  /* Debugging/error reporting stuff */
  int tveng_errno; /* Numerical id of the last error, 0 == success */
  char * error; /* points to the last error message */
  int debug_level; /* 0 for no errors, increase for greater verbosity */

  Display	*display;
  int		save_x, save_y;
  int		bpp;
  int		current_bpp;

  struct tveng_module_info module;

  x11_vidmode_state old_mode;

  int		zapping_setup_fb_verbosity;
  gchar *	mode; /* vidmode */

  int		disable_xv_video; /* 1 if XVideo should be disabled */
  int		dword_align; /* 1 if x and w should be dword aligned */

  pthread_mutex_t mutex; /* Thread safety */

  /* Controls managed directly by tveng.c */
#ifdef USE_XV
  XvPortID	port;
  Atom filter;
  Atom double_buffer;
  Atom colorkey; /* colorkey doesn't have min, max, it's defined by
		    RGB triplets */
#endif

  unsigned int		callback_recursion;

  tv_callback *		video_input_callback;
  tv_callback *		audio_input_callback;
  tv_callback *		video_standard_callback;

  tv_control *		control_mute;
  tv_bool		quiet;

  /* when audio capability or reception changes */
  tv_callback *		audio_callback;

};




/*
  Utility function, stops the capture or the previewing. Returns the
  mode the device was before stopping.
  For stopping and restarting the device do:
  enum tveng_capture_mode cur_mode;
  cur_mode = tveng_stop_everything(info);
  ... do some stuff ...
  if (tveng_restart_everything(cur_mode, info) == -1)
     ... show error dialog ...
*/
capture_mode tveng_stop_everything (tveng_device_info *info,
				    gboolean *overlay_was_active);
/*
  Restarts the given capture mode. See the comments on
  tveng_stop_everything. Returns -1 on error.
*/
int tveng_restart_everything (capture_mode mode,
			      gboolean overlay_was_active,
			      tveng_device_info * info);

int p_tv_set_preview_window(tveng_device_info * info, const tv_window *window);
tv_bool p_tv_enable_overlay (tveng_device_info * info, tv_bool enable);
capture_mode 
p_tveng_stop_everything (tveng_device_info * info,
			 gboolean * overlay_was_active);
int p_tveng_restart_everything (capture_mode mode,
				gboolean overlay_was_active,
				tveng_device_info * info);
const tv_image_format *
p_tv_set_capture_format(tveng_device_info *info,const tv_image_format *format);

extern void
tv_callback_notify		(tveng_device_info *	info,
				 void *			object,
				 const tv_callback *	list);


#define for_all(p, pslist) for (p = pslist; p; p = p->_next)

#define IS_TUNER_LINE(l) ((l) && (l)->type == TV_VIDEO_LINE_TYPE_TUNER)

#define NODE_HELPER_FUNCTIONS(item, kind)				\
extern void								\
free_##kind			(tv_##kind *		p);		\
extern void								\
free_##kind##_list		(tv_##kind **		list);		\
extern void								\
store_cur_##item		(tveng_device_info *	info,		\
				 const tv_##kind *	p);

NODE_HELPER_FUNCTIONS		(control, control);
extern void
free_controls			(tveng_device_info *	info);
extern tv_control *
append_control			(tveng_device_info *	info,
				 tv_control *		tc,
				 unsigned int		size);
extern tv_control *
append_audio_mode_control	(tveng_device_info *	info,
				 tv_audio_capability	cap);
extern tv_bool
set_audio_mode_control		(tveng_device_info *	info,
				 tv_control *		control,
				 int			value);
extern tv_audio_capability
select_audio_capability		(tv_audio_capability		cap,
				 const tv_video_standard *	std);
NODE_HELPER_FUNCTIONS		(video_standard, video_standard);
extern void
free_video_standards		(tveng_device_info *	info);
extern tv_video_standard *
append_video_standard		(tv_video_standard **	list,
				 tv_videostd_set	videostd_set,
				 const char *		label,
				 const char *		hlabel,
				 unsigned int		size);
NODE_HELPER_FUNCTIONS		(audio_input, audio_line);
extern void
free_audio_inputs		(tveng_device_info *	info);
extern tv_audio_line *
append_audio_line		(tv_audio_line **	list,
				 tv_audio_line_type	type,
				 const char *		label,
				 const char *		hlabel,
				 int			minimum,
				 int			maximum,
				 int			step,
				 int			reset,
				 unsigned int		size);
NODE_HELPER_FUNCTIONS		(video_input, video_line);
extern void
free_video_inputs		(tveng_device_info *	info);
extern tv_video_line *
append_video_line		(tv_video_line **	list,
				 tv_video_line_type	type,
				 const char *		label,
				 const char *		hlabel,
				 unsigned int		size);


extern void
tveng_copy_frame		(unsigned char *	src,
				 tveng_image_data *	where,
				 tveng_device_info *	info);

extern void
ioctl_failure			(tveng_device_info *	info,
				 const char *		source_file_name,
				 const char *		function_name,
				 unsigned int		source_file_line,
				 const char *		ioctl_name);

extern tv_pixfmt
pig_depth_to_pixfmt		(unsigned int		depth);

struct _tv_mixer_interface {
	const char *		name;

	/*
	 *  Open a soundcard mixer by its device file name.
	 */
	tv_mixer *		(* open)		(const tv_mixer_interface *,
							 FILE *log,
							 const char *device);
	/*
	 *  Scan for mixer devices present on the system.
	 */
	tv_mixer *		(* scan)		(const tv_mixer_interface *,
							 FILE *log);
	/*
	 *  Update tv_audio_line.muted and .volume, e.g. to notice when
	 *  other applications change mixer properties asynchronously.
	 *  Regular polling recommended, may call tv_dev_audio_line.changed.
	 */
	tv_bool			(* update_line)		(tv_audio_line *);
	/*
	 *  Set mixer volume and update tv_audio_line.volume accordingly.
	 *  On mono lines left volume will be set. May call
	 *  tv_dev_audio_line.changed. Does not unmute.
	 */
	tv_bool			(* set_volume)		(tv_audio_line *,
							 int left,
							 int right);
	/*
	 *  Mute (TRUE) or unmute (FALSE) mixer line and update
	 *  tv_audio_line.muted accordingly. May call
	 *  tv_dev_audio_line.changed.
	 */
	tv_bool			(* set_mute)		(tv_audio_line *,
							 tv_bool mute);
	/*
	 *  Select a recording line from tv_mixer.adc_lines. When
	 *  exclusive is TRUE disable all other recording sources (should
	 *  be the default, but we must not prohibit recording from
	 *  multiple sources if the user really insists). Line can be
	 *  NULL. May call tv_dev_mixer.changed. 
	 */
	tv_bool			(* set_rec_line)	(tv_mixer *,
							 tv_audio_line *,
							 tv_bool exclusive);
	/*
	 *  Update tv_mixer.rec_line, e.g. to notice when other applications
	 *  change mixer properties asynchronously. Regular polling recommended,
	 *  may call tv_dev_mixer.changed.
	 */
	tv_bool			(* update_mixer)	(tv_mixer *);
};

#endif /* tveng_private.h */
