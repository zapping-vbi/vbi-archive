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

/* i18n */
#ifndef _
#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) gettext (String)
#ifndef N_
#define N_(String) (String)
#endif
#else /* ENABLE_NLS */
#define _(String) (String)
#ifndef N_
#define N_(String) (String)
#endif
#endif /* ENABLE_NLS */
#endif /* _ */

#include "x11stuff.h"
#include "zmisc.h"

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
enum tveng_capture_mode tveng_stop_everything (tveng_device_info *info,
					       gboolean *overlay_was_active);
/*
  Restarts the given capture mode. See the comments on
  tveng_stop_everything. Returns -1 on error.
*/
int tveng_restart_everything (enum tveng_capture_mode mode,
			      gboolean overlay_was_active,
			      tveng_device_info * info);

int p_tveng_set_preview_window(tveng_device_info * info);
int p_tveng_set_preview (int on, tveng_device_info * info);
enum tveng_capture_mode 
p_tveng_stop_everything (tveng_device_info * info,
			 gboolean * overlay_was_active);
int p_tveng_restart_everything (enum tveng_capture_mode mode,
				gboolean overlay_was_active,
				tveng_device_info * info);
int p_tveng_set_capture_format(tveng_device_info * info);

extern void
tv_callback_notify		(tveng_device_info *	info,
				 void *			object,
				 const tv_callback *	list);

/*
  Function prototypes for modules, NULL means not implemented or not
  pertinent.
  For the descriptions, see tveng.h
*/
struct tveng_module_info {
  int	(*attach_device)(const char* device_file,
			 enum tveng_attach_mode  attach_mode,
			 tveng_device_info * info);
  void	(*describe_controller)(char **short_str, char **long_str,
			       tveng_device_info *info);
  void	(*close_device)(tveng_device_info *info);
	/*
	 */
	int		(* ioctl)		(tveng_device_info *,
						 int,
						 char *);
	/*
	 *  Sets the current video input to one in the video
	 *  input list. This implies update_video_input with all
	 *  side effects mentioned.
	 */
	tv_bool		(* set_video_input)	(tveng_device_info *,
						 const tv_video_line *);
	/*
	 *  Updates info.cur_video_input to notice asynchronous changes
	 *  by other applications, may call the video_input_callback.
	 *  May update the video standard list and the current video
	 *  standard.
	 */
	tv_bool		(* update_video_input)	(tveng_device_info *);

	tv_bool		(* set_tuner_frequency)	(tveng_device_info *,
						 tv_video_line *,
						 unsigned int);
	tv_bool		(* update_tuner_frequency)
						(tveng_device_info *,
						 tv_video_line *);
	/*
	 *  Sets the current video standard to one in the video
	 *  standard list. This implies update_standard with all
	 *  side effects mentioned.
	 */
	tv_bool		(* set_standard)	(tveng_device_info *,
						 const tv_video_standard *);
	/*
	 *  Updates info.current_videostd to notice asynchronous changes
	 *  by other applications, may call the videostd_callback.
	 *  To update the list of supported standards update the
	 *  current input property instead.
	 */
	tv_bool		(* update_standard)	(tveng_device_info *);
	/*
	 *  Sets the value of a control, this implies update_control
	 *  with all side effects mentioned.
	 */
  	tv_bool		(* set_control)		(tveng_device_info *,
						 tv_control *,
						 int);
	/*
	 *  Updates tv_control.value to notice asynchronous changes
	 *  by other applications, may call tv_control.callback.
	 *  May also update other properties if we get that
	 *  information in the course. If the control is NULL update
	 *  all controls, this may be faster than individual updates.
	 */
	tv_bool		(* update_control)	(tveng_device_info *,
						 tv_control *);

  int	(*update_capture_format)(tveng_device_info *info);
  int	(*set_capture_format)(tveng_device_info *info);

  int	(*get_signal_strength)(int *strength, int *afc,
			       tveng_device_info *info);

  int	(*start_capturing)(tveng_device_info *info);
  int	(*stop_capturing)(tveng_device_info *info);
  int	(*read_frame)(tveng_image_data *where,
		      unsigned int time, tveng_device_info *info);
  double (*get_timestamp)(tveng_device_info *info);
  int	(*set_capture_size)(int width, int height,
			    tveng_device_info *info);
  int	(*get_capture_size)(int *width, int *height,
			    tveng_device_info *info);


	tv_bool		(* get_overlay_buffer)	(tveng_device_info *,
						 tv_overlay_buffer *);
	tv_bool		(* set_overlay_buffer)	(tveng_device_info *,
						 tv_overlay_buffer *);
	tv_bool		(* set_overlay_xwindow)	(tveng_device_info *,
						 Window,
						 GC);
  int	(*set_preview_window)(tveng_device_info *info);
  int	(*get_preview_window)(tveng_device_info *info);
	tv_bool		(* set_overlay)		(tveng_device_info *,
						 tv_bool);

  void	(*set_chromakey)(uint32_t pixel, tveng_device_info *info);
  int	(*get_chromakey)(uint32_t *pixel, tveng_device_info *info);

  /* Device specific stuff */
  int	(*ov511_get_button_state)(tveng_device_info *info);

  /* size of the private data of the module */
  int	private_size;
};

struct tveng_private {
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
#ifdef HAVE_STRNDUP
#define tv_strndup strndup
#else
extern char *
tv_strndup			(const char *		s,
				 size_t			size);
#endif

extern char *
tv_strdup_printf		(const char *		templ,
				 ...);

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
							 unsigned int left,
							 unsigned int right);
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
