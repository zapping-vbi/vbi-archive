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
  This is the library in charge of simplyfying Video Access API (I
  don't want to use thirteen lines of code with ioctl's every time I
  want to change tuning freq).
  the name is TV Engine, since it is intended mainly for TV viewing.
  This file is separated so zapping doesn't need to know about V4L[2]
*/

#include "site_def.h"

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <X11/Xlib.h> /* We use some X calls */
#include <X11/Xutil.h>
#include <ctype.h>
#include <assert.h>
#include <dirent.h>

/* This undef's are to avoid a couple of header warnings */
#undef WNOHANG
#undef WUNTRACED
#include "tveng.h"
#include "tveng1.h" /* V4L specific headers */
#include "tveng25.h" /* V4L2 2.5 specific headers */
#include "tvengxv.h" /* XVideo specific headers */
#include "tvengemu.h" /* Emulation device */
#include "tvengbktr.h" /* Emulation device */
#include "tveng_private.h" /* private definitions */

#include "globals.h" /* XXX for vidmodes, dga_param */

#include "zmisc.h"
#include "../common/device.h"

/* int rc = 0; */

/* XXX recursive callbacks not good, think again. */
#define TVLOCK								\
({									\
  if (0 == info->callback_recursion) {				\
    /* fprintf (stderr, "TVLOCK   %d %d in %s\n", (int) pthread_self(), rc++, __FUNCTION__);*/ \
    pthread_mutex_lock(&(info->mutex));				\
  }									\
})
#define UNTVLOCK							\
({									\
  if (0 == info->callback_recursion) {				\
    /* fprintf (stderr, "UNTVLOCK %d %d in %s\n", (int) pthread_self(), --rc, __FUNCTION__);*/ \
    pthread_mutex_unlock(&(info->mutex));				\
  }									\
})

#define RETURN_UNTVLOCK(X)						\
do {									\
  __typeof__(X) _unlocked_result = X;					\
  UNTVLOCK;								\
  return _unlocked_result;						\
} while (0)

#define TVUNSUPPORTED do { \
  /* function not supported by the module */ \
 info->tveng_errno = -1; \
 tv_error_msg (info, "Function not supported by the module"); \
} while (0)

/* for xv */
typedef struct {
	tv_control		pub;
	tv_control **		prev_next;
	tv_control *		source;
	tv_bool			override;
	tveng_device_info *	info;
	Atom			atom;
	tv_audio_line *		mixer_line;
	tv_callback *		mixer_line_cb;
} ccontrol;

#define C(l) PARENT (l, ccontrol, pub)

static tv_control *
control_by_id			(tveng_device_info *	info,
				 tv_control_id		id);

static void
destroy_ccontrol		(ccontrol *		cc)
{
	assert (NULL != cc);

	tv_callback_delete_all (cc->pub._callback,
				/* notify: any */ NULL,
				/* destroy: any */ NULL,
				/* user_data: any */ NULL,
				&cc->pub);

	assert (NULL != cc->prev_next);
	*cc->prev_next = cc->pub._next;

	if (cc->pub._next)
		C(cc->pub._next)->prev_next = cc->prev_next;

	tv_control_delete (&cc->pub);
}

static void
destroy_cloned_controls		(tveng_device_info *	info)
{
	while (info->cloned_controls) {
		ccontrol *c = C(info->cloned_controls);

		tv_callback_delete_all (c->pub._callback, 0, 0, 0, c);
		tv_callback_remove (c->mixer_line_cb);
		destroy_ccontrol (c);
	}
}

static void
ccontrol_notify_cb		(tv_control *		tc,
				 void *			user_data)
{
	ccontrol *cc = (ccontrol *) user_data;

	cc->pub.value = tc->value; 

	tv_callback_notify (NULL, &cc->pub, cc->pub._callback);
}

static void
ccontrol_destroy_cb		(tv_control *		tc _unused_,
				 void *			user_data)
{
	destroy_ccontrol ((ccontrol *) user_data);
}

static ccontrol *
ccontrol_copy			(tv_control *		tc)
{
	ccontrol *cc;

	if (!(cc = C (tv_control_dup (tc, sizeof (*cc))))) {
		return NULL;
	}

	cc->pub._callback = tc->_callback;
	tc->_callback = NULL;

	if (!tv_control_add_callback (&cc->pub,
				      ccontrol_notify_cb,
				      ccontrol_destroy_cb,
				      cc)) {
		tc->_callback = cc->pub._callback;
		cc->pub._callback = NULL;
		tv_control_delete (&cc->pub);
		return NULL;
	}

	cc->source = tc;

	return cc;
}

static void
insert_clone_control		(tv_control **		list,
				 ccontrol *		cc)
{
	while (NULL != *list)
		list = &(*list)->_next;

	cc->prev_next = list;
	cc->pub._next = *list;
	*list = &cc->pub;
}

static tv_bool
clone_controls			(tv_control **		list,
				 tv_control *		tc)
{
	tv_control **prev_next;

	assert (NULL != list);

	*list = NULL;
	prev_next = list;

	while (NULL != tc) {
		ccontrol *cc;

		if (!(cc = ccontrol_copy (tc))) {
			goto failure;
		}

		cc->prev_next = prev_next;
		*prev_next = &cc->pub;
		prev_next = &cc->pub._next;

		tc = tc->_next;
	}

	return TRUE;

 failure:
	free_control_list (list);

	return FALSE;
}




typedef void (*tveng_controller)(struct tveng_module_info *info);
static tveng_controller tveng_controllers[] = {
  tvengxv_init_module,
  tveng25_init_module,
  tveng1_init_module,
  tvengbktr_init_module,
};


/*
static void
deref_callback			(void *			object,
				 void *			user_data)
{
	*((void **) user_data) = NULL;
}
*/

void p_tveng_close_device(tveng_device_info * info);
int p_tveng_update_controls(tveng_device_info * info);
int p_tveng_get_display_depth(tveng_device_info * info);

tv_bool
p_tv_enable_overlay		(tveng_device_info *	info,
				 tv_bool		enable);


/* Initializes a tveng_device_info object */
tveng_device_info * tveng_device_info_new(Display * display, int bpp)
{
  size_t needed_mem = 0;
  tveng_device_info * new_object;
  struct tveng_module_info module_info;
  pthread_mutexattr_t attr;
  unsigned int i;

  /* Get the needed mem for the controllers */
  for (i = 0; i < N_ELEMENTS (tveng_controllers); ++i)
    {
      tveng_controllers[i](&module_info);
      needed_mem = MAX(needed_mem, (size_t) module_info.private_size);
    }

  assert (needed_mem > 0);

  if (!(new_object = calloc (1, needed_mem)))
    return NULL;

  if (!(new_object->error = malloc (256)))
    {
      free (new_object);
      perror ("malloc");
      return NULL;
    }

  new_object->display = display;
  new_object->bpp = bpp;

  new_object->zapping_setup_fb_verbosity = 0; /* No output by
							  default */

  x11_vidmode_clear_state (&new_object->old_mode);

  pthread_mutexattr_init(&attr);
  /*  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE); */
  pthread_mutex_init(&(new_object->mutex), &attr);
  pthread_mutexattr_destroy(&attr);

  new_object->current_controller = TVENG_CONTROLLER_NONE;
 
  if (io_debug_msg > 0)
    new_object->log_fp = stderr;

  /* return the allocated memory */
  return (new_object);
}

/* destroys a tveng_device_info object (and closes it if neccesary) */
void tveng_device_info_destroy(tveng_device_info * info)
{
  assert (NULL != info);

  destroy_cloned_controls (info);

  if (-1 != info -> fd)
    p_tveng_close_device(info);

  if (info -> error)
    free(info -> error);

  pthread_mutex_destroy(&(info->mutex));

  tv_callback_delete_all (info->panel.video_input_callback, 0, 0, 0, info);
  tv_callback_delete_all (info->panel.audio_input_callback, 0, 0, 0, info);
  tv_callback_delete_all (info->panel.video_standard_callback, 0, 0, 0, info);
  tv_callback_delete_all (info->panel.audio_callback, 0, 0, 0, info);

  tv_clip_vector_destroy (&info->overlay.clip_vector);

  free(info);
}

const char *
tv_get_errstr			(tveng_device_info *	info)
{
	return info->error;
}

int
tv_set_errstr			(tveng_device_info *	info,
				 const char *		template,
				 ...)
{
	va_list ap;
	int n;

	va_start (ap, template);

	n = vsnprintf (info->error, 256, template, ap);

	va_end (ap);

	return n;
}

int
tv_get_debug_level		(tveng_device_info *	info)
{
	return info->debug_level;
}

int
tv_get_errno			(tveng_device_info *	info)
{
	return info->tveng_errno;
}

capture_mode
tv_get_capture_mode		(tveng_device_info *	info)
{
	return info->capture_mode;
}

void
tv_set_capture_mode		(tveng_device_info *	info,
				 capture_mode		mode)
{
	info->capture_mode = mode;
}

extern enum tveng_controller
tv_get_controller		(tveng_device_info *	info)
{
	return info->current_controller;
}

const struct tveng_caps *
tv_get_caps			(tveng_device_info *	info)
{
	return &info->caps;
}

enum tveng_attach_mode
tv_get_attach_mode		(tveng_device_info *	info)
{
	return info->attach_mode;
}

int
tv_get_fd			(tveng_device_info *	info)
{
	return info->fd;
}

void
tv_clear_error			(tveng_device_info *	info)
{
	info->error[0] = 0;
}

void
tv_overlay_hack			(tveng_device_info *	info,
				 int x, int y, int w, int h)
{
	info->overlay.window.x = x;
	info->overlay.window.y = y;
	info->overlay.window.width = w;
	info->overlay.window.height = h;
}

void
tv_set_filename			(tveng_device_info *	info,
				 const char *		s)
{
	free (info->file_name);
	if (s)
		info->file_name = strdup (s);
	else
		info->file_name = NULL;
}

/*
  Associates the given tveng_device_info with the given video
  device. On error it returns -1 and sets info->tveng_errno, info->error to
  the correct values.
  device_file: The file used to access the video device (usually
  /dev/video)
  window: Find a device capable of rendering into this window
  (XVideo, Xinerama). Can be None. 
  attach_mode: Specifies the mode to open the device file
  depth: The color depth the capture will be in, -1 means let tveng
  decide based on the current private->display depth.
  info: The structure to be associated with the device
*/
int tveng_attach_device(const char* device_file,
			Window window,
			enum tveng_attach_mode attach_mode,
			tveng_device_info * info)
{
  const char *long_str, *short_str;
  char *sign = NULL;
  tv_control *tc;
  int num_controls;
  tv_video_line *tl;
  int num_inputs;

  assert (NULL != device_file);
  assert (NULL != info);

  TVLOCK;

  tv_clear_error (info);

  destroy_cloned_controls (info);

  if (-1 != info -> fd) /* If the device is already attached, detach it */
    p_tveng_close_device(info);

  info -> current_controller = TVENG_CONTROLLER_NONE;

  /*
    Check that the current private->display depth is one of the
    supported ones.

    XXX this is inaccurate. It depends on the
    formats supported by the video HW and our conversion
    capabilities.
  */
  info->current_bpp = p_tveng_get_display_depth(info);

  switch (info->current_bpp)
    {
    case 15:
    case 16:
    case 24:
    case 32:
      break;
    default:
      info -> tveng_errno = -1;
      tv_error_msg (info, "Display depth %u is not supported",
		    info->current_bpp);
      UNTVLOCK;
      return -1;
    }

  if (0 == strcmp (device_file, "emulator"))
    {
      info->fd = -1;

      tvengemu_init_module (&info->module);

      info->module.attach_device (device_file, window,
					attach_mode, info);

      goto success;
    }
  else
    {
      unsigned int i;

      for (i = 0; i < N_ELEMENTS (tveng_controllers); ++i)
	{
	  info->fd = -1;

	  /* XXX */
	  if (TVENG_ATTACH_VBI == attach_mode
	      && tvengxv_init_module == tveng_controllers[i])
	    continue;

	  tveng_controllers[i](&(info->module));

	  if (!info->module.attach_device)
	    continue;

	  if (-1 != info->module.attach_device
	      (device_file, window, attach_mode, info))
	    goto success;
	}
    }

  /* Error */
  info->tveng_errno = -1;
  tv_error_msg (info, "The device cannot be attached to any controller");
  CLEAR (info->module);
  UNTVLOCK;
  return -1;

 success:
  /* See p_tveng_set_capture_format() */
#ifdef ENABLE_BKTR
  /* FIXME bktr VBI capturing does not work if
     video capture format is YUV. */
#define YUVHACK TV_PIXFMT_SET_RGB
#else
#define YUVHACK (TV_PIXFMT_SET (TV_PIXFMT_YUV420) | \
       		 TV_PIXFMT_SET (TV_PIXFMT_YVU420) | \
		 TV_PIXFMT_SET (TV_PIXFMT_YUYV) | \
		 TV_PIXFMT_SET (TV_PIXFMT_UYVY))
#endif

#ifdef YUVHACK
  if (info->capture.supported_pixfmt_set & TV_PIXFMT_SET_YUV)
    info->capture.supported_pixfmt_set &= YUVHACK;
#endif

  info->freq_change_restart = TRUE;

  {
    info->cloned_controls = NULL;
    if (!clone_controls (&info->cloned_controls,
			 info->panel.controls))
      {
	p_tveng_close_device (info);
	CLEAR (info->module);
	UNTVLOCK;
	return -1;
      }
  }

  {
    info->control_mute = NULL;
    info->audio_mutable = 0;

    /* Add mixer controls */
    /* XXX the mixer_line should be property of a virtual
       device, but until we're there... */
    if (mixer && mixer_line)
      tveng_attach_mixer_line (info, mixer, mixer_line);

    if ((tc = control_by_id (info, TV_CONTROL_ID_MUTE)))
      {
	info->control_mute = tc;
	info->audio_mutable = 1; /* preliminary */
      }

    num_controls = 0;
    for_all (tc, info->cloned_controls)
      num_controls++;

    num_inputs = 0;
    for_all (tl, info->panel.video_inputs)
      num_inputs++;
  }

  if (_tv_asprintf (&sign, "%s - %d %d - %d %d %d",
		    info->caps.name,
		    num_inputs,
		    num_controls,
		    info->caps.flags,
		    info->caps.maxwidth,
		    info->caps.maxheight) < 0) {
	  t_error("asprintf", info);
	  destroy_cloned_controls (info);
	  p_tveng_close_device (info);
	  CLEAR (info->module);
	  UNTVLOCK;
	  return -1;
  }
  info->signature = tveng_build_hash (sign);
  free (sign);

  if (info->debug_level>0)
    {
      short_str = "?";
      long_str = "?";

      fprintf(stderr, "[TVeng] - Info about the video device\n");
      fprintf(stderr, "-------------------------------------\n");

      fprintf(stderr, "Device: %s [%s]\n", info->file_name,
	      info->module.interface_label);
      fprintf(stderr, "Device signature: %x\n", info->signature);
      fprintf(stderr, "Detected framebuffer depth: %d\n",
	      p_tveng_get_display_depth(info));
      fprintf (stderr, "Capture format:\n"
	       "  buffer size            %ux%u pixels, 0x%x bytes\n"
	       "  bytes per line         %u, %u bytes\n"
	       "  offset		 %u, %u, %u bytes\n"
	       "  pixfmt                 %s\n",
	       info->capture.format.width,
	       info->capture.format.height,
	       info->capture.format.size,
	       info->capture.format.bytes_per_line[0],
	       info->capture.format.bytes_per_line[1],
	       info->capture.format.offset[0],
	       info->capture.format.offset[1],
	       info->capture.format.offset[2],
	       info->capture.format.pixel_format ?
	       info->capture.format.pixel_format->name : "<none>");
      fprintf(stderr, "Current overlay window struct:\n");
      fprintf(stderr, "  Coords: %dx%d-%dx%d\n",
	      info->overlay.window.x,
	      info->overlay.window.y,
	      info->overlay.window.width,
	      info->overlay.window.height);

      {
	tv_video_standard *s;
	unsigned int i;

	fprintf (stderr, "Video standards:\n");

	for (s = info->panel.video_standards, i = 0; s; s = s->_next, ++i)
	  fprintf (stderr, "  %d) '%s'  0x%llx %dx%d  %.2f  %u  hash: %x\n",
		   i, s->label, s->videostd_set,
		   s->frame_width, s->frame_height,
		   s->frame_rate, s->frame_ticks, s->hash);

	fprintf (stderr, "  Current: %s\n",
		 info->panel.cur_video_standard ?
		 info->panel.cur_video_standard->label : "unset");
      }

      {
	tv_video_line *l;
	unsigned int i;

	fprintf (stderr, "Video inputs:\n");

	for (l = info->panel.video_inputs, i = 0; l; l = l->_next, ++i)
	  {
	    fprintf (stderr, "  %d) '%s'  id: %u  hash: %x  %s\n",
		     i, l->label, l->id, l->hash,
		     IS_TUNER_LINE (l) ? "tuner" : "composite");
	    if (IS_TUNER_LINE (l))
	      fprintf (stderr, "       freq.range: %u...%u %+d Hz, "
		       "current: %u Hz\n",
		       l->u.tuner.minimum, l->u.tuner.maximum,
		       l->u.tuner.step, l->u.tuner.frequency);
	  }

	fprintf (stderr, "  Current: %s\n",
		 info->panel.cur_video_input ?
		 info->panel.cur_video_input->label : "unset");
      }

      {
	tv_audio_line *l;
	unsigned int i;

	fprintf (stderr, "Audio inputs:\n");

	for (l = info->panel.audio_inputs, i = 0; l; l = l->_next, ++i)
	  {
	    fprintf (stderr, "  %d) '%s'  id: %u  hash: %x\n"
		     "       range: %d...%d step: %d reset: %d volume: %d,%d\n"
		     "       %srecordable %s %smuted\n",
		     i, l->label, l->id, l->hash,
		     l->minimum, l->maximum, l->step, l->reset,
		     l->volume[0], l->volume[1],
		     l->recordable ? "" : "not ",
		     l->stereo ? "stereo" : "mono/unknown",
		     l->muted ? "" : "un");
	  }

	fprintf (stderr, "  Current: %s\n",
		 info->panel.cur_audio_input ?
		 info->panel.cur_audio_input->label : "unset");
      }

      {
	tv_control *c;
	unsigned int i;

	fprintf (stderr, "Controls:\n");

	for (c = info->cloned_controls, i = 0; c; c = c->_next, ++i)
	  {
	    fprintf (stderr, "  %u) '%s'  id: %u  %d...%d"
		     " %+d  cur: %d  reset: %d ",
		     i, c->label, c->id,
		     c->minimum, c->maximum, c->step,
		     c->value, c->reset);

	    switch (c->type)
	      {
	      case TV_CONTROL_TYPE_INTEGER:
		fprintf (stderr, "integer\n");
		break;
	      case TV_CONTROL_TYPE_BOOLEAN:
		fprintf (stderr, "boolean\n");
		break;
	      case TV_CONTROL_TYPE_CHOICE:
		{
		  unsigned int i;

		  fprintf (stderr, "choice\n");

		  for (i = 0; c->menu[i]; ++i)
		    fprintf (stderr, "    %u) '%s'\n", i, c->menu[i]);

		  break;
		}
	      case TV_CONTROL_TYPE_ACTION:
		fprintf (stderr, "action\n");
		break;
	      case TV_CONTROL_TYPE_COLOR:
		fprintf(stderr, "color\n");
		break;
	      default:
		fprintf(stderr, "unknown type\n");
		break;
	      }
	  }
      }
    }

  UNTVLOCK;
  return info->fd; /* XXX not all devices have fd,
		      fake fd (tvengxv) is dangerous. */
}

/*
  Stores in short_str and long_str (if they are non-null) the
  description of the current controller. The enum value can be found in
  info->current_controller.
  For example, V4L2 controller would say:
  short_str: 'V4L2'
  long_str: 'Video4Linux 2'
  info->current_controller: TVENG_CONTROLLER_V4L2
  This function always succeeds.
*/
void
tveng_describe_controller(const char ** short_str, const char ** long_str,
			  tveng_device_info * info)
{
  assert (NULL != info);
  assert (TVENG_CONTROLLER_NONE != info->current_controller);

  TVLOCK;

  tv_clear_error (info);

  if (short_str)
    *short_str = "";
  if (long_str)
    *long_str = info->module.interface_label;

  UNTVLOCK;
}

/* Closes a device opened with tveng_init_device */
void p_tveng_close_device(tveng_device_info * info)
{
  gboolean dummy;

  if (info->current_controller == TVENG_CONTROLLER_NONE)
    return; /* nothing to be done */

  p_tveng_stop_everything(info, &dummy);

  /* remove mixer controls */
  tveng_attach_mixer_line (info, NULL, NULL);

  if (info->module.close_device)
    info->module.close_device(info);

  info->control_mute = NULL;
}

/* Closes a device opened with tveng_init_device */
void tveng_close_device(tveng_device_info * info)
{
  assert (NULL != info);

  if (info->current_controller == TVENG_CONTROLLER_NONE)
    return; /* nothing to be done */

  TVLOCK;

  tv_clear_error (info);

  p_tveng_close_device (info);

  UNTVLOCK;
}

/*
  Functions for controlling the video capture. All of them return -1
  in case of error, so any value != -1 should be considered valid
  (unless explicitly stated in the description of the function) 
*/

/* Returns a newly allocated copy of the string, normalized */
static char* normalize(const char *string)
{
  int i = 0;
  const char *strptr=string;
  char *result;

  assert (NULL != string);

  result = strdup(string);

  assert (NULL != result);

  while (*strptr != 0) {
    if (*strptr == '_' || *strptr == '-' || *strptr == ' ') {
      strptr++;
      continue;
    }
    result[i] = tolower(*strptr);

    strptr++;
    i++;
  }
  result[i] = 0;

  return result;
}

/* nomalize and compare */
static int tveng_normstrcmp (const char * in1, const char * in2)
{
  char *s1 = normalize(in1);
  char *s2 = normalize(in2);

  assert (NULL != in1);
  assert (NULL != in2);

  /* Compare the strings */
  if (!strcmp(s1, s2)) {
    free(s1);
    free(s2);
    return 1;
  } else {
    free(s1);
    free(s2);
    return 0;
  }
}

/* build hash for the given string, normalized */
int
tveng_build_hash(const char *string)
{
  char *norm = normalize(string);
  unsigned int i;
  int result=0;

  for (i = 0; i<strlen(norm); i++)
    result += ((result+171)*((int)norm[i]) & ~(norm[i]>>4));

  free(norm);

  return result;
}

void
ioctl_failure			(tveng_device_info *	info,
				 const char *		source_file_name,
				 const char *		function_name,
				 unsigned int		source_file_line,
				 const char *		ioctl_name)
{
	info->tveng_errno = errno;

	snprintf (info->error, 255,
		  "%s:%s:%u: ioctl %s failed: %d, %s",
		  source_file_name,
		  function_name,
		  source_file_line,
		  ioctl_name,
		  info->tveng_errno,
		  strerror (info->tveng_errno));

	if (info->debug_level > 0) {
		fputs (info->error, stderr);
		fputc ('\n', stderr);
	}

	errno = info->tveng_errno;
}

static void
panel_failure			(tveng_device_info *	info,
				 const char *		function_name)
{
	info->tveng_errno = -1; /* unknown */

	snprintf (info->error, 255,
		  "%s:%s: function not supported in control mode\n",
		  __FILE__, function_name);

	if (info->debug_level) {
		fputs (info->error, stderr);
		fputc ('\n', stderr);
	}
}

#define REQUIRE_IO_MODE(_result)					\
do {									\
  if (TVENG_ATTACH_CONTROL == info->attach_mode				\
      || TVENG_ATTACH_VBI == info->attach_mode) {			\
    panel_failure (info, __PRETTY_FUNCTION__);				\
    return _result;							\
  }									\
} while (0)

static void
support_failure			(tveng_device_info *	info,
				 const char *		function_name)
{
  info->tveng_errno = -1;

  snprintf (info->error, 255,
	    "%s not supported by driver\n",
	    function_name);

  if (info->debug_level)
    fprintf (stderr, "tveng:%s\n", info->error);
}

#define REQUIRE_SUPPORT(_func, _result)					\
do {									\
  if ((_func) == NULL) {						\
    support_failure (info, __PRETTY_FUNCTION__);			\
    return _result;							\
  }									\
} while (0)





#define CUR_NODE_FUNC(item, kind)					\
const tv_##kind *							\
tv_cur_##item			(const tveng_device_info *info)		\
{									\
	assert (NULL != info);						\
	return info->panel.cur_##item;					\
}

#define HEAD_NODE_FUNC(item, kind)					\
tv_##kind *							\
tv_##item##s			(const tveng_device_info *info)		\
{									\
	assert (NULL != info);						\
	return info->panel.item##s;					\
}

#define NEXT_NODE_FUNC(item, kind)					\
const tv_##kind *							\
tv_next_##item			(const tveng_device_info *info,		\
				 const tv_##kind *	p)		\
{									\
	if (p)								\
		return p->_next;					\
	if (info)							\
		return info->panel.item##s;				\
	return NULL;							\
}

#define NTH_NODE_FUNC(item, kind)					\
const tv_##kind *							\
tv_nth_##item			(tveng_device_info *	info,		\
				 unsigned int		index)		\
{									\
	tv_##kind *p;							\
									\
	assert (NULL != info);					        \
									\
	TVLOCK;								\
									\
	for_all (p, info->panel.item##s)				\
		if (index-- == 0)					\
			break;						\
									\
	RETURN_UNTVLOCK (p);						\
}

#define NODE_POSITION_FUNC(item, kind)					\
unsigned int								\
tv_##item##_position		(tveng_device_info *	info,		\
				 const tv_##kind *	p)		\
{									\
	tv_##kind *list;						\
	unsigned int index;						\
									\
	assert (NULL != info);			        		\
									\
	index = 0;							\
									\
	TVLOCK;								\
									\
	for_all (list, info->panel.item##s)				\
		if (p != list)						\
			++index;					\
		else							\
			break;						\
									\
	RETURN_UNTVLOCK (index);					\
}

#define NODE_BY_HASH_FUNC(item, kind)					\
tv_##kind *							        \
tv_##item##_by_hash		(tveng_device_info *	info,		\
				 unsigned int		hash)		\
{									\
	tv_##kind *p;							\
									\
	assert (NULL != info);					        \
									\
	TVLOCK;								\
									\
	for_all (p, info->panel.item##s)				\
		if (p->hash == hash)					\
			break;						\
									\
	RETURN_UNTVLOCK (p);						\
}

CUR_NODE_FUNC			(video_input, video_line);
HEAD_NODE_FUNC			(video_input, video_line);
NEXT_NODE_FUNC			(video_input, video_line);
NTH_NODE_FUNC			(video_input, video_line);
NODE_POSITION_FUNC		(video_input, video_line);
NODE_BY_HASH_FUNC		(video_input, video_line);

tv_bool
tv_get_tuner_frequency		(tveng_device_info *	info,
				 unsigned int *		frequency)
{
	tv_video_line *line;

	assert (NULL != info);
	assert (NULL != frequency);

	TVLOCK;

  tv_clear_error (info);

	line = info->panel.cur_video_input;

	if (!IS_TUNER_LINE (line))
		RETURN_UNTVLOCK (FALSE);

	if (info->panel.get_tuner_frequency) {
		if (!info->panel.get_tuner_frequency (info, line))
			RETURN_UNTVLOCK (FALSE);

		assert (line->u.tuner.frequency >= line->u.tuner.minimum);
		assert (line->u.tuner.frequency <= line->u.tuner.maximum);
	}

	*frequency = line->u.tuner.frequency;

	RETURN_UNTVLOCK (TRUE);
}

/* Note the tuner frequency is a property of the tuner. When you
   switch the tuner it will not assume the current frequency. Likewise
   you will get the frequency previously set for *this* tuner. Granted
   the function parameters are misleading, should change. */
tv_bool
tv_set_tuner_frequency		(tveng_device_info *	info,
				 unsigned int		frequency)
{
	tv_video_line *line;

	assert (NULL != info);

	REQUIRE_SUPPORT (info->panel.set_tuner_frequency, FALSE);

	TVLOCK;

  tv_clear_error (info);

	line = info->panel.cur_video_input;

	if (!IS_TUNER_LINE (line))
		RETURN_UNTVLOCK (FALSE);

	frequency = SATURATE (frequency,
			      line->u.tuner.minimum,
			      line->u.tuner.maximum);

	RETURN_UNTVLOCK (info->panel.set_tuner_frequency
			 (info, line, frequency));
}

const tv_video_line *
tv_get_video_input		(tveng_device_info *	info)
{
	const tv_video_line *tl;

	assert (NULL != info);

	TVLOCK;

  tv_clear_error (info);

	if (!info->panel.get_video_input)
		tl = info->panel.cur_video_input;
	else if (info->panel.get_video_input (info))
		tl = info->panel.cur_video_input;
	else
		tl = NULL;

	RETURN_UNTVLOCK (tl);
}

tv_bool
tv_set_video_input		(tveng_device_info *	info,
				 tv_video_line *	line)
{
	tv_video_line *list;

	assert (NULL != info);
	assert (NULL != line);

	REQUIRE_SUPPORT (info->panel.set_video_input, FALSE);

	TVLOCK;

  tv_clear_error (info);

	for_all (list, info->panel.video_inputs)
		if (list == line)
			break;

	if (list == NULL) {
		UNTVLOCK;
		return FALSE;
	}

	RETURN_UNTVLOCK (info->panel.set_video_input (info, line));
}

/*
  Sets the input named name as the active input. -1 on error.
*/
int
tveng_set_input_by_name(const char * input_name,
			tveng_device_info * info)
{
  tv_video_line *tl;

  assert (NULL != input_name);
  assert (NULL != info);

  TVLOCK;

  tv_clear_error (info);

  for_all (tl, info->panel.video_inputs)
    if (tveng_normstrcmp(tl->label, input_name))
      {
/*XXX*/
	UNTVLOCK;
	return tv_set_video_input (info, tl);
      }

  info->tveng_errno = -1;
  tv_error_msg (info, "Input %s doesn't appear to exist", input_name);

  UNTVLOCK;
  return -1; /* String not found */
}

/*
 *  Audio inputs
 */

CUR_NODE_FUNC			(audio_input, audio_line);
HEAD_NODE_FUNC			(audio_input, audio_line);
NEXT_NODE_FUNC			(audio_input, audio_line);
NTH_NODE_FUNC			(audio_input, audio_line);
NODE_POSITION_FUNC		(audio_input, audio_line);
NODE_BY_HASH_FUNC		(audio_input, audio_line);

const tv_audio_line *
tv_get_audio_input		(tveng_device_info *	info)
{
	const tv_audio_line *tl;

	assert (NULL != info);

	TVLOCK;

  tv_clear_error (info);

	if (!info->panel.get_audio_input)
		tl = info->panel.cur_audio_input;
	else if (info->panel.get_audio_input (info))
		tl = info->panel.cur_audio_input;
	else
		tl = NULL;

	RETURN_UNTVLOCK (tl);
}

tv_bool
tv_set_audio_input		(tveng_device_info *	info,
				 tv_audio_line *	line)
{
	tv_audio_line *list;

	assert (NULL != info);
	assert (NULL != line);

	REQUIRE_SUPPORT (info->panel.set_audio_input, FALSE);

	TVLOCK;

  tv_clear_error (info);

	for_all (list, info->panel.audio_inputs)
		if (list == line)
			break;

	if (list == NULL) {
		UNTVLOCK;
		return FALSE;
	}

	RETURN_UNTVLOCK (info->panel.set_audio_input (info, line));
}

/*
 *  Video standards
 */

CUR_NODE_FUNC			(video_standard, video_standard);
HEAD_NODE_FUNC			(video_standard, video_standard);
NEXT_NODE_FUNC			(video_standard, video_standard);
NTH_NODE_FUNC			(video_standard, video_standard);
NODE_POSITION_FUNC		(video_standard, video_standard);
NODE_BY_HASH_FUNC		(video_standard, video_standard);

const tv_video_standard *
tv_get_video_standard		(tveng_device_info *	info)
{
	const tv_video_standard *ts;

	assert (NULL != info);

	TVLOCK;

  tv_clear_error (info);

	if (!info->panel.get_video_standard)
		ts = info->panel.cur_video_standard;
	else if (info->panel.get_video_standard (info))
		ts = info->panel.cur_video_standard;
	else
		ts = NULL;

	RETURN_UNTVLOCK (ts);
}

tv_bool
tv_set_video_standard		(tveng_device_info *	info,
				 tv_video_standard *	standard)
{
	tv_video_standard *list;

	assert (NULL != info);
	assert (NULL != standard);

	REQUIRE_SUPPORT (info->panel.set_video_standard, FALSE);

	TVLOCK;

  tv_clear_error (info);

	for_all (list, info->panel.video_standards)
		if (list == standard)
			break;

	if (list == NULL) {
		UNTVLOCK;
		return FALSE;
	}

	RETURN_UNTVLOCK (info->panel.set_video_standard (info, standard));
}

tv_bool
tv_set_video_standard_by_id	(tveng_device_info *	info,
				 tv_videostd_set	videostd_set)
{
	tv_video_standard *ts, *list;

	assert (NULL != info);

	REQUIRE_SUPPORT (info->panel.set_video_standard, FALSE);

	TVLOCK;

  tv_clear_error (info);

	ts = NULL;

	for_all (list, info->panel.video_standards)
		if (ts->videostd_set & videostd_set) {
			if (ts) {
				info->tveng_errno = -1;
				tv_error_msg (info, "Ambiguous id %llx",
					      videostd_set);
				RETURN_UNTVLOCK (FALSE);
			}

			ts = list;
		}

	RETURN_UNTVLOCK (info->panel.set_video_standard (info, ts));
}

/*
  Sets the standard by name. -1 on error
*/
int
tveng_set_standard_by_name(const char * name, tveng_device_info * info)
{
  tv_video_standard *ts;

  assert (NULL != info);

  TVLOCK;

  tv_clear_error (info);

  for_all (ts, info->panel.video_standards)
    if (tveng_normstrcmp(name, ts->label))
      {
	if (info->panel.set_video_standard)
	  RETURN_UNTVLOCK(info->panel.set_video_standard(info, ts) ? 0 : -1);

	TVUNSUPPORTED;
	UNTVLOCK;
	return -1;
      }

  info->tveng_errno = -1;
  tv_error_msg (info, "Standard %s doesn't appear to exist", name);

  UNTVLOCK;
  return -1; /* String not found */  
}

/*
 *  Controls
 */

tv_control *
tv_next_control			(tveng_device_info *	info,
				 tv_control *		p)
{
	if (p)
		return p->_next;
	if (info)
		return info->cloned_controls;
	return NULL;
}

tv_control *
tv_nth_control			(tveng_device_info *	info,
				 unsigned int		index)
{
	tv_control *c;

	assert (NULL != info);

	TVLOCK;

  tv_clear_error (info);

	for (c = info->cloned_controls; c; c = c->_next) 
		if (0 == index--)
			break;

	RETURN_UNTVLOCK (c);
}

unsigned int
tv_control_position		(tveng_device_info *	info,
				 const tv_control *	control)
{
	tv_control *list;
	unsigned int index;

	assert (NULL != info);

	index = 0;

	TVLOCK;

  tv_clear_error (info);

	for (list = info->cloned_controls; list; list = list->_next)
		if (control != list)
			++index;
		else
			break;

	RETURN_UNTVLOCK (index);
}

tv_control *
tv_control_by_hash		(tveng_device_info *	info,
				 unsigned int		hash)
{
	tv_control *c;

	assert (NULL != info);

	TVLOCK;

  tv_clear_error (info);

	for (c = info->cloned_controls; c; c = c->_next)
		if (c->hash == hash)
			break;

	RETURN_UNTVLOCK (c);
}

static tv_control *
control_by_id			(tveng_device_info *	info,
				 tv_control_id		id)
{
	tv_control *c;

	assert (NULL != info);

	for (c = info->cloned_controls; c; c = c->_next)
		if (c->id == id)
			break;

	return c;
}

tv_control *
tv_control_by_id		(tveng_device_info *	info,
				 tv_control_id		id)
{
	tv_control *control;

	assert (NULL != info);

	TVLOCK;

  tv_clear_error (info);

  control = control_by_id (info, id);

	RETURN_UNTVLOCK (control);
}



static void
round_boundary_4		(unsigned int *		x1,
				 unsigned int *		width,
				 tv_pixfmt		pixfmt,			 
				 unsigned int		max_width)
{
	unsigned int x, w;

	if (!x1) {
		x = 0;
		x1 = &x;
	}

	switch (pixfmt) {
	case TV_PIXFMT_RGBA32_LE:
	case TV_PIXFMT_RGBA32_BE:
	case TV_PIXFMT_BGRA32_LE:
	case TV_PIXFMT_BGRA32_BE:
		x = (((*x1 << 2) + 2) & (unsigned int) -4) >> 2;
		w = (((*width << 2) + 2) & (unsigned int) -4) >> 2;
		break;

	case TV_PIXFMT_RGB24_LE:
	case TV_PIXFMT_BGR24_LE:
		/* Round to multiple of 12. */
		x = (*x1 + 2) & (unsigned int) -4;
		w = (*width + 2) & (unsigned int) -4;
		break;

	case TV_PIXFMT_RGB16_LE:
	case TV_PIXFMT_RGB16_BE:
	case TV_PIXFMT_BGR16_LE:
	case TV_PIXFMT_BGR16_BE:
	case TV_PIXFMT_RGBA16_LE:
	case TV_PIXFMT_RGBA16_BE:
	case TV_PIXFMT_BGRA16_LE:
	case TV_PIXFMT_BGRA16_BE:
	case TV_PIXFMT_ARGB16_LE:
	case TV_PIXFMT_ARGB16_BE:
	case TV_PIXFMT_ABGR16_LE:
	case TV_PIXFMT_ABGR16_BE:
	case TV_PIXFMT_RGBA12_LE:
	case TV_PIXFMT_RGBA12_BE:
	case TV_PIXFMT_BGRA12_LE:
	case TV_PIXFMT_BGRA12_BE:
	case TV_PIXFMT_ARGB12_LE:
	case TV_PIXFMT_ARGB12_BE:
	case TV_PIXFMT_ABGR12_LE:
	case TV_PIXFMT_ABGR12_BE:
		x = (((*x1 << 1) + 2) & (unsigned int) -4) >> 1;
		w = (((*width << 1) + 2) & (unsigned int) -4) >> 1;
		break;

	case TV_PIXFMT_RGB8:
	case TV_PIXFMT_BGR8:
	case TV_PIXFMT_RGBA8:
	case TV_PIXFMT_BGRA8:
	case TV_PIXFMT_ARGB8:
	case TV_PIXFMT_ABGR8:
		x = (*x1 + 2) & (unsigned int) -4;
		w = (*width + 2) & (unsigned int) -4;
		break;

	default:
		assert (!"reached");
	}

	w = MIN (w, max_width);

	if ((x + w) >= max_width)
		x = max_width - w;

	if (0)
		fprintf (stderr, "dword align: x=%u->%u w=%u->%u\n",
			 *x1, x, *width, w);
	*x1 = x;
	*width = w;
}

tv_pixfmt_set
tv_supported_pixfmts		(tveng_device_info *	info)
{
	assert (NULL != info);

	return info->capture.supported_pixfmt_set;
}

const tv_image_format *
tv_cur_capture_format		(tveng_device_info *	info)
{
	assert (NULL != info);
	return &info->capture.format;
}

const tv_image_format *
tv_get_capture_format		(tveng_device_info *	info)
{
	assert(NULL != info);

	if (TVENG_CONTROLLER_NONE == info->current_controller)
		return NULL;

	REQUIRE_IO_MODE (NULL);
	REQUIRE_SUPPORT (info->capture.get_format, NULL);

	TVLOCK;

	tv_clear_error (info);

	if (info->capture.get_format (info))
		RETURN_UNTVLOCK (&info->capture.format);
	else
		RETURN_UNTVLOCK (NULL);
}

const tv_image_format *
p_tv_set_capture_format		(tveng_device_info *	info,
				 const tv_image_format *fmt)
{
	tv_image_format format;
	tv_bool result;

	if (TVENG_CONTROLLER_NONE == info->current_controller)
		return NULL;

	if (CAPTURE_MODE_READ == info->capture_mode) {
#ifdef TVENG_FORCE_FORMAT
		if (0 == (TV_PIXFMT_SET (fmt->pixfmt)
			  & TV_PIXFMT_SET (TVENG_FORCE_FORMAT))) {
			return NULL;
		}
#else
		/* FIXME the format selection (capture.c) has problems
		   with RGB & YUV, so for now we permit only YUV. */
#ifdef YUVHACK
		if (info->capture.supported_pixfmt_set & TV_PIXFMT_SET_YUV)
		  if (0 == (TV_PIXFMT_SET (fmt->pixel_format->pixfmt) & YUVHACK)) {
		    return NULL;
		  }
#endif
#endif
	}

	REQUIRE_IO_MODE (NULL);
	REQUIRE_SUPPORT (info->capture.set_format, NULL);

	format = *fmt;

	format.width = SATURATE (format.width,
				 info->caps.minwidth,
				 info->caps.maxwidth);
	format.height = SATURATE (format.height,
				  info->caps.minheight,
				  info->caps.maxheight);

	if (info->dword_align)
		round_boundary_4 (NULL, &format.width,
				  format.pixel_format->pixfmt,
				  info->caps.maxwidth);

	result = info->capture.set_format (info, &format);

	if (result) {
		return &info->capture.format;
	}

	return NULL;
}

/*
   XXX this is called from scan_devices to determine supported
   formats, which is overkill. Maybe we should have a dedicated
   function to query formats a la try_fmt.

   FIXME request_capture_format_real needs a rewrite, see TODO.
   In the meantime we will allow only YUV formats a la Zapping
   0.0 - 0.6.
  */
const tv_image_format *
tv_set_capture_format		(tveng_device_info *	info,
				 const tv_image_format *fmt)
{
  assert (NULL != info);

  TVLOCK;

  tv_clear_error (info);

  RETURN_UNTVLOCK (p_tv_set_capture_format (info, fmt));
}

/*
 *  Controls
 */

static int
update_xv_control (tveng_device_info * info, ccontrol *c)
{
  int value;

  assert (c->override || !c->source);

  if (c->pub.id == TV_CONTROL_ID_VOLUME
      || c->pub.id == TV_CONTROL_ID_MUTE)
    {
      tv_audio_line *line;

      if (info->quiet)
	return 0;

      line = c->mixer_line;
      assert (NULL != line);

      if (!tv_mixer_line_update (line))
	return -1;

      if (c->pub.id == TV_CONTROL_ID_VOLUME)
	value = (line->volume[0] + line->volume[1] + 1) >> 1;
      else
	value = !!(line->muted);
    }
  else
    {
      return 0;
    }

  if (c->pub.value != value)
    {
      c->pub.value = value;
      tv_callback_notify (info, &c->pub, c->pub._callback);
    }

  return 0;
}

int
tveng_update_control(tv_control *control,
		     tveng_device_info * info)
{
  assert (NULL != info);

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return -1;

  assert (NULL != control);

  TVLOCK;

  tv_clear_error (info);

  if (C(control)->override || !C(control)->source)
    RETURN_UNTVLOCK (update_xv_control (info, C(control)));
  else
    control = C(control)->source;

  assert (NULL != control);

  if (!info->panel.get_control)
    RETURN_UNTVLOCK(0);

  RETURN_UNTVLOCK(info->panel.get_control (info, control) ? 0 : -1);
}

int
p_tveng_update_controls(tveng_device_info * info)
{
  tv_control *tc;

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return -1;

  /* Update the controls we maintain */
  for (tc = info->cloned_controls; tc; tc = tc->_next)
    if (C(tc)->override || !C(tc)->source)
      update_xv_control (info, C(tc));

  if (!info->panel.get_control)
    return 0;

  return (info->panel.get_control (info, NULL) ? 0 : -1);
}

int
tveng_update_controls(tveng_device_info * info)
{
  assert (NULL != info);

  TVLOCK;

  tv_clear_error (info);

  RETURN_UNTVLOCK (p_tveng_update_controls (info));
}

/* When setting the mixer volume we must ensure the
   video device is unmuted and volume at maximum. */
static void
reset_video_volume		(tveng_device_info *	info,
				 tv_bool		mute)
{
	tv_control *tc;

	if (!info->panel.set_control)
		return;

	for (tc = info->panel.controls; tc; tc = tc->_next) {
		switch (tc->id) {
		case TV_CONTROL_ID_VOLUME:
			/* Error ignored */
			info->panel.set_control (info, tc, tc->maximum);
			break;

		case TV_CONTROL_ID_MUTE:
			/* Error ignored */
			info->panel.set_control (info, tc, (int) mute);
			break;

		default:
			break;
		}
	}
}

static int
set_control_audio		(tveng_device_info *	info,
				 ccontrol *		c,
				 int			value,
				 tv_bool		quiet,
				 tv_bool		reset)
{
  tv_bool success = FALSE;

  if (c->override || !c->source)
    {
      tv_callback *cb;

      assert (NULL != c->mixer_line);

      cb = c->mixer_line->_callback;

      if (quiet)
	c->mixer_line->_callback = NULL;

      switch (c->pub.id)
	{
	case TV_CONTROL_ID_MUTE:
	  success = tv_mixer_line_set_mute (c->mixer_line, value);

	  value = c->mixer_line->muted;

	  if (value)
	    reset = FALSE;

	  break;

	case TV_CONTROL_ID_VOLUME:
	  success = tv_mixer_line_set_volume (c->mixer_line, value, value);

	  value = (c->mixer_line->volume[0]
		   + c->mixer_line->volume[1] + 1) >> 1;
	  break;

	default:
	  assert (0);
	}

      if (quiet)
	{
	  c->mixer_line->_callback = cb;
	}
      else if (success && c->pub.value != value)
	{
	  c->pub.value = value;
	  tv_callback_notify (info, &c->pub, c->pub._callback);
	}

      if (success && reset)
	reset_video_volume (info, /* mute */ FALSE);
    }
  else if (info->panel.set_control)
    {
      if (quiet)
	{
	  tv_callback *cb;
	  int old_value;

	  cb = c->pub._callback;
	  old_value = c->pub.value;

	  c->pub._callback = NULL;

	  success = info->panel.set_control (info, c->source, value);

	  c->pub.value = old_value;
	  c->pub._callback = cb;
	}
      else
	{
	  success = info->panel.set_control (info, c->source, value);
	}
    }

  return success ? 0 : -1;
}

static int
set_panel_control		(tveng_device_info *	info,
				 tv_control *		tc,
				 int			value)
{
  if (info->panel.set_control)
    return info->panel.set_control (info, tc, value) ? 0 : -1;

  TVUNSUPPORTED;
  return -1;
}

static int
set_control			(ccontrol * c, int value,
				 tv_bool reset,
				 tveng_device_info * info)
{
  int r = 0;

  value = SATURATE (value, c->pub.minimum, c->pub.maximum);

  /* If the quiet switch is set we remember the
     requested value but don't change driver state. */
  if (info->quiet)
    if (c->pub.id == TV_CONTROL_ID_VOLUME
	|| c->pub.id == TV_CONTROL_ID_MUTE)
      goto set_and_notify;

  if (c->override || !c->source)
    {
      if (c->pub.id == TV_CONTROL_ID_VOLUME
	  || c->pub.id == TV_CONTROL_ID_MUTE)
	{
	  return set_control_audio (info, c, value, /* quiet */ FALSE, reset);
	}
      else
	{
	  return 0;
	}

    set_and_notify:
      if (c->pub.value != value)
	{
	  c->pub.value = value;
	  tv_callback_notify (info, &c->pub, c->pub._callback);
	}

      return r;
    }
  else /* not override */
    {
      //      info = (tveng_device_info *) c->pub._parent; /* XXX */
      assert (NULL != c->source);
      return set_panel_control (info, c->source, value);
    }
}

/*
  Sets the value for a specific control. The given value will be
  clipped between min and max values. Returns -1 on error

  XXX another function tveng_set_controls would be nice.
  Would save v4l ioctls on channel switches.
*/
int
tveng_set_control(tv_control * control, int value,
		  tveng_device_info * info)
{
  assert (NULL != info);

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return -1;

  assert (NULL != control);

  TVLOCK;
  tv_clear_error (info);

  RETURN_UNTVLOCK (set_control (C(control), value, TRUE, info));
}

/*
  Gets the value of a control, given its name. Returns -1 on
  error. The comparison is performed disregarding the case. The value
  read is stored in cur_value.
*/
int
tveng_get_control_by_name(const char * control_name,
			  int * cur_value,
			  tveng_device_info * info)
{
  tv_control *tc;

  assert (NULL != info);
  assert (NULL != info->cloned_controls);
  assert (NULL != control_name);

  TVLOCK;

  tv_clear_error (info);

  /* Update the controls (their values) */
  if (info->quiet)
    {
      /* not now */
      /* FIXME volume and mute only */
    }
  else
    {
      if (tveng_update_controls(info) == -1)
	RETURN_UNTVLOCK(-1);
    }

  /* iterate through the info struct to find the control */
  for (tc = info->cloned_controls; tc; tc = tc->_next)
    if (!strcasecmp(tc->label,control_name))
      /* we found it */
      {
	int value;

	value = tc->value;
	if (cur_value)
	  *cur_value = value;
	UNTVLOCK;
	return 0; /* Success */
      }

  /* if we reach this, we haven't found the control */
  info->tveng_errno = -1;
  tv_error_msg(info,
	       "Cannot find control \"%s\" in the list of controls",
	       control_name);
  UNTVLOCK;
  return -1;
}

/*
  Sets the value of a control, given its name. Returns -1 on
  error. The comparison is performed disregarding the case.
  new_value holds the new value given to the control, and it is
  clipped as neccessary.
*/
int
tveng_set_control_by_name(const char * control_name,
			  int new_value,
			  tveng_device_info * info)
{
  tv_control *tc;

  assert (NULL != info);
  assert (NULL != info->cloned_controls);

  TVLOCK;

  tv_clear_error (info);

  for (tc = info->cloned_controls; tc; tc = tc->_next)
    if (0 == strcasecmp(tc->label,control_name))
      /* we found it */
      RETURN_UNTVLOCK (set_control (C(tc), new_value, TRUE, info));

  /* if we reach this, we haven't found the control */
  info->tveng_errno = -1;
  tv_error_msg(info,
	       "Cannot find control \"%s\" in the list of controls",
	       control_name);
  UNTVLOCK;
  return -1;
}


/*
 *  AUDIO ROUTINES
 */

#ifndef TVENG_MIXER_VOLUME_DEBUG
#define TVENG_MIXER_VOLUME_DEBUG 0
#endif

/*
 *  This is a mixer control. Notify when the underlying
 *  mixer line changes.
 */
static void
mixer_line_notify_cb		(tv_audio_line *	line,
				 void *			user_data)
{
  ccontrol *c = user_data;
  tv_control *tc;
  int value;

  assert (line == c->mixer_line);

  tc = &c->pub;

  if (tc->id == TV_CONTROL_ID_VOLUME)
    {
      tc->minimum = line->minimum;
      tc->maximum = line->maximum;
      tc->step = line->step;
      tc->reset = line->reset;

      value = (line->volume[0] + line->volume[1] + 1) >> 1;
    }
  else /* TV_CONTROL_ID_MUTE */
    {
      tc->reset = 0;
      tc->minimum = 0;
      tc->maximum = 1;
      tc->step = 1;

      value = line->muted;
    }

  if (c->info && !c->info->quiet)
    {
      tc->value = value;
      tv_callback_notify (NULL, tc, tc->_callback);
    }
}

/*
 *  This is a mixer control. When the underlying mixer line
 *  disappears, remove the control and make the corresponding
 *  video device control visible again.
 *
 *  XXX c->info->quiet?
 */
static void
mixer_line_destroy_cb		(tv_audio_line *	line,
				 void *			user_data)
{
  ccontrol *c = user_data;
  tv_control *tc;

  assert (line == c->mixer_line);

  if (c->override) {
	  /* Remove mixer line */

	  c->override = FALSE;

	  c->mixer_line = NULL;
	  c->mixer_line_cb = NULL;

	  c->pub.minimum = c->source->minimum;
	  c->pub.maximum = c->source->maximum;
	  c->pub.step = c->source->step;
	  c->pub.reset = c->source->reset;
			
	  c->pub.value = c->source->value;

	  tv_callback_notify (c->info, &c->pub, c->pub._callback);
  } else {
	  if (c->pub.id == TV_CONTROL_ID_MUTE)
	  {
		  c->info->control_mute = NULL;
		  c->info->audio_mutable = 0; /* preliminary */
	  }

	  tv_callback_delete_all (tc->_callback, 0, 0, 0, tc);

	  destroy_ccontrol (c);
  }
}

/*
 *  When line != NULL this adds a mixer control, hiding the
 *  corresponding video device (volume or mute) control.
 *  When line == NULL the old state is restored.
 */
static void
mixer_replace			(tveng_device_info *	info,
				 tv_audio_line *	line,
				 tv_control_id		id)
{
	tv_control *tc;
	ccontrol *c;

	tc = control_by_id (info, id);

	if (!tc) {
		if (!line)
			return;

		/* Add mixer control */

		c = calloc (1, sizeof (*c));
		assert (NULL != c); /* XXX */

		c->pub.id = id;

		if (id == TV_CONTROL_ID_VOLUME) {
			c->pub.type = TV_CONTROL_TYPE_INTEGER;

			if (TVENG_MIXER_VOLUME_DEBUG)
				c->pub.label = strdup ("Mixer volume");
			else
				c->pub.label = strdup (_("Volume"));
		} else {
			c->pub.type = TV_CONTROL_TYPE_BOOLEAN;

			if (TVENG_MIXER_VOLUME_DEBUG)
				c->pub.label = strdup ("Mixer mute");
			else
				c->pub.label = strdup (_("Mute"));
		}

		assert (NULL != c->pub.label); /* XXX */

		c->info = info;
		c->mixer_line = line;

		c->mixer_line_cb = tv_mixer_line_add_callback
			(line, mixer_line_notify_cb,
			 mixer_line_destroy_cb, c);
		assert (NULL != c->mixer_line_cb); /* XXX */

		mixer_line_notify_cb (line, c);

		insert_clone_control (&info->cloned_controls, c);

		return;
	}

	c = PARENT (tc, ccontrol, pub);

	if (C(tc)->override) {
		if (line) {
			/* Replace mixer line */

			tv_callback_remove (c->mixer_line_cb);

			c->mixer_line = line;

			c->mixer_line_cb = tv_mixer_line_add_callback
				(line, mixer_line_notify_cb,
				 mixer_line_destroy_cb, c);
			assert (NULL != c->mixer_line_cb); /* XXX */

			mixer_line_notify_cb (line, c);
		} else {
			/* Remove mixer line */

			c->override = FALSE;

			tv_callback_remove (c->mixer_line_cb);

			c->mixer_line = NULL;
			c->mixer_line_cb = NULL;

			c->pub.minimum = c->source->minimum;
			c->pub.maximum = c->source->maximum;
			c->pub.step = c->source->step;
			c->pub.reset = c->source->reset;
			
			c->pub.value = c->source->value;

			tv_callback_notify (info, &c->pub, c->pub._callback);
		}
	} else {
		if (line) {
			/* Override */

			c->override = TRUE;

			c->mixer_line = line;

			c->mixer_line_cb = tv_mixer_line_add_callback
				(line, mixer_line_notify_cb,
				 mixer_line_destroy_cb, c);
			assert (NULL != c->mixer_line_cb); /* XXX */

			mixer_line_notify_cb (line, c);
		} else {
			tv_callback_delete_all (tc->_callback, 0, 0, 0, tc);

			tv_callback_remove (c->mixer_line_cb);

			destroy_ccontrol (c);
		}
	}
}

/*
 *  This provisional function logically attaches a
 *  soundcard audio input to a video device.
 *
 *  Two configurations use this: TV cards with an audio
 *  loopback to a soundcard, and TV cards without audio,
 *  where the audio source (e.g. VCR, microphone) 
 *  connects directly to a soundcard.
 *
 *  The function adds an audio mixer volume and mute control,
 *  overriding any video device volume and
 *  mute controls. Tveng will also care to set the video device
 *  controls when mixer controls are changed.
 *
 *  To get rid of the mixer controls, call with line NULL or
 *  just delete the mixer struct.
 *
 *  NB control values are not copied when replacing controls.
 */
void
tveng_attach_mixer_line		(tveng_device_info *	info,
				 tv_mixer *		mixer,
				 tv_audio_line *	line)
{
  tv_control *tc;

  info->control_mute = NULL;
  info->audio_mutable = 0;

  if (mixer && line)
    printv ("Attaching mixer %s (%s %s), line %s\n",
	    mixer->node.device,
	    mixer->node.label,
	    mixer->node.driver,
	    line->label);
  else
    printv ("Removing mixer\n");

  mixer_replace (info, line, TV_CONTROL_ID_VOLUME);
  mixer_replace (info, line, TV_CONTROL_ID_MUTE);

  for (tc = info->cloned_controls; tc; tc = tc->_next)
    if (tc->id == TV_CONTROL_ID_MUTE)
      {
	info->control_mute = tc;
	info->audio_mutable = 1; /* preliminary */
	break;
      }
}

/*
 *  This function implements a second mute switch closer
 *  to the hardware. The 'quiet' state is not visible to the
 *  mute or volume controls or their callbacks, takes priority
 *  and can only be changed with this function.
 *
 *  Intention is that only the user changes audio controls, while
 *  this switch is flipped under program control, such as when
 *  switching channels.
 */
void
tv_quiet_set			(tveng_device_info *	info,
				 tv_bool		quiet)
{
  assert (NULL != info);

  TVLOCK;

  tv_clear_error (info);

  if (info->quiet == quiet)
    {
      UNTVLOCK;
      return;
    }

  info->quiet = quiet;

  if (quiet)
    {
      tv_control *tc;

      if ((tc = control_by_id (info, TV_CONTROL_ID_MUTE)))
	{
	  /* Error ignored */
	  set_control_audio (info, C (tc),
			     /* value */ TRUE,
			     /* quiet */ TRUE,
			     /* reset */ FALSE);
	}
      else if ((tc = control_by_id (info, TV_CONTROL_ID_VOLUME)))
	{
	  /* Error ignored */
	  set_control_audio (info, C (tc),
			     /* value */ 0,
			     /* quiet */ TRUE,
			     /* reset */ FALSE);
	}
    }
  else
    {
      tv_control *tc;
      tv_bool reset;

      reset = FALSE;

      if ((tc = control_by_id (info, TV_CONTROL_ID_MUTE))
	  || (tc = control_by_id (info, TV_CONTROL_ID_VOLUME)))
	{
	  /* Error ignored */
	  set_control_audio (info, C (tc),
			     tc->value,
			     /* quiet */ FALSE,
			     /* reset */ FALSE);

	  if (C(tc)->override) /* uses soundcard mixer */
	    reset_video_volume (info, /* mute */ FALSE);
	}
    }

  UNTVLOCK;
}

/*
 *  This is a shortcut to get the mute control state. When @update
 *  is TRUE the control is updated from the driver, otherwise
 *  it returns the last known state.
 *  1 = muted (no sound), 0 = unmuted, -1 = error
 */
int
tv_mute_get			(tveng_device_info *	info,
				 tv_bool		update)
{
  tv_control *tc;

  assert (NULL != info);

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return -1;

  /* XXX TVLOCK;*/

  tv_clear_error (info);

  if ((tc = info->control_mute))
    {
      if (!info->quiet)
	if (update)
	  if (-1 == tveng_update_control (tc, info))
	    return -1;

      return tc->value;
    }

  TVLOCK;
  TVUNSUPPORTED;
  RETURN_UNTVLOCK (-1);
}

/*
 *  This is a shortcut to set the mute control state.
 *  0 = ok, -1 = error.
 */
int
tv_mute_set			(tveng_device_info *	info,
				 tv_bool		mute)
{
  tv_control *tc;
  int r = -1;

  assert (NULL != info);

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return -1;

  TVLOCK;

  tv_clear_error (info);

  if ((tc = info->control_mute))
    r = set_control (C(tc), mute, TRUE, info);
  else
    TVUNSUPPORTED;

  RETURN_UNTVLOCK(r);
}

/*
 *  This is a shortcut to add a callback to the mute control.
 */
tv_callback *
tv_mute_add_callback		(tveng_device_info *	info,
				 void			(* notify)(tveng_device_info *, void *),
				 void			(* destroy)(tveng_device_info *, void *),
				 void *			user_data)
{
  tv_control *tc;

  assert (NULL != info);

  tv_clear_error (info);


  if (!(tc = info->control_mute))
    return NULL;

  return tv_callback_add (&tc->_callback,
			  (tv_callback_fn *) notify,
			  (tv_callback_fn *) destroy,
			  user_data);
}

/*
 *  Audio mode
 */

tv_bool
tv_set_audio_mode		(tveng_device_info *	info,
				 tv_audio_mode		mode)
{
	assert (NULL != info);

	REQUIRE_SUPPORT (info->panel.set_audio_mode, FALSE);

  tv_clear_error (info);


	/* XXX check mode within capabilities. */

/* XXX audio mode control
	TVLOCK;
	RETURN_UNTVLOCK (info->module.set_audio_mode (info, mode));
*/
	return info->panel.set_audio_mode (info, mode);
}

tv_bool
tv_audio_update			(tveng_device_info *	info _unused_)
{
	return FALSE; /* XXX todo*/
}

/* capability or reception changes */
tv_callback *
tv_add_audio_callback		(tveng_device_info *	info,
				 void			(* notify)(tveng_device_info *, void *),
				 void			(* destroy)(tveng_device_info *, void *),
				 void *			user_data)
{
  assert (NULL != info);

  tv_clear_error (info);


  return tv_callback_add (&info->panel.audio_callback,
			  (tv_callback_fn *) notify,
			  (tv_callback_fn *) destroy,
			  user_data);
}

/* ----------------------------------------------------------- */

/*
  Gets the signal strength and the afc code. The afc code indicates
  how to get a better signal, if negative, tune higher, if negative,
  tune lower. 0 means no idea or feature not present in the current
  controller (i.e. V4L1). Strength and/or afc can be NULL pointers,
  that would mean ignore that parameter.
*/
tv_bool
tv_get_signal_strength		(tveng_device_info *	info,
				 int *			strength,
				 int *			afc)
{
  assert (NULL != info);

  if (TVENG_CONTROLLER_NONE == info->current_controller)
	return FALSE;

	if (strength) {
		/* Default. */
		*strength = 0;
	} else if (!afc) {
		/* Neither requested, we're done. */
		return TRUE;
	}

	if (afc) {
		*afc = 0;
	}

  TVLOCK;

  tv_clear_error (info);

  if (!info->panel.cur_video_input)
    RETURN_UNTVLOCK(TRUE);

  /* Check that there are tuners in the current input */
  if (!IS_TUNER_LINE (info->panel.cur_video_input))
    RETURN_UNTVLOCK(FALSE);

  if (info->panel.get_signal_strength)
	  RETURN_UNTVLOCK(info->panel.get_signal_strength(info, strength, afc));
  TVUNSUPPORTED;
  UNTVLOCK;
  return FALSE;
}

/*
  Sets up the capture device so any read() call after this one
  succeeds. Returns -1 on error.
*/
int
tveng_start_capturing(tveng_device_info * info)
{
  assert (NULL != info);

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return -1;

  REQUIRE_IO_MODE (-1);

  TVLOCK;

  tv_clear_error (info);

  if (info->capture.enable)
    RETURN_UNTVLOCK(info->capture.enable (info, TRUE) ? 0 : -1);

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* Tries to stop capturing. -1 on error. */
int
tveng_stop_capturing(tveng_device_info * info)
{
  assert (NULL != info);

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return -1;

  REQUIRE_IO_MODE (-1);

  TVLOCK;

  if (info->capture_mode == CAPTURE_MODE_NONE)
    {
	    fprintf(stderr,
		    "Warning: trying to stop capture "
		    "with no capture active\n");
	    RETURN_UNTVLOCK(-1);
    }

  tv_clear_error (info);

  if (info->capture.enable)
    RETURN_UNTVLOCK(info->capture.enable (info, FALSE) ? 0 : -1);

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

tv_bool
tv_set_buffers			(tveng_device_info *	info,
				 unsigned int 		n_buffers)
{
  assert (NULL != info);

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return FALSE;

  REQUIRE_IO_MODE (FALSE);

  TVLOCK;

  if (info->capture.set_buffers)
    RETURN_UNTVLOCK(info->capture.set_buffers (info, NULL, n_buffers));

  TVUNSUPPORTED;
  UNTVLOCK;

  return FALSE;
}

tv_bool
tv_get_buffers			(tveng_device_info *	info,
				 unsigned int * 	n_buffers)
{
  *n_buffers = info->capture.n_buffers;
  return TRUE;
}

int
tv_read_frame			(tveng_device_info *	info,
				 tv_capture_buffer *	buffer,
				 const struct timeval *	timeout)
{
  assert (NULL != info);

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return -1;

  REQUIRE_IO_MODE (-1);

  /* XXX if buffer != NULL check buffer size. */

  TVLOCK;

  tv_clear_error (info);

  if (info->capture.read_frame)
    {
      int r;

      r = info->capture.read_frame (info, buffer, timeout);

      RETURN_UNTVLOCK(r);
    }

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

tv_bool
tv_queue_capture_buffer		(tveng_device_info *	info,
				 const tv_capture_buffer *buffer)
{
  tv_capture_buffer *b = /* const_cast */ buffer;

  assert (NULL != info);

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return FALSE;

  REQUIRE_IO_MODE (FALSE);

  TVLOCK;

  tv_clear_error (info);

  if (info->capture.queue_buffer)
    RETURN_UNTVLOCK(info->capture.queue_buffer (info, b));

  TVUNSUPPORTED;
  UNTVLOCK;
  return FALSE;
}

const tv_capture_buffer *
tv_dequeue_capture_buffer	(tveng_device_info *	info)
{
  tv_capture_buffer *buffer;
  struct timeval timeout;
  int r;

  assert (NULL != info);

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return NULL;

  REQUIRE_IO_MODE (NULL);

  TVLOCK;

  tv_clear_error (info);

  if (!info->capture.dequeue_buffer) {
    TVUNSUPPORTED;
    UNTVLOCK;
    return NULL;
  }

  timeout.tv_sec = 0;
  timeout.tv_usec = 0;

  r = info->capture.dequeue_buffer (info, &buffer, &timeout);

  RETURN_UNTVLOCK ((1 == r) ? buffer : NULL);
}

int
tv_dequeue_capture_buffer_with_timeout
				(tveng_device_info *	info,
				 const tv_capture_buffer **buffer,
				 struct timeval *	timeout)
{
  tv_capture_buffer **b = /* const_cast */ buffer;

  assert (NULL != info);

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return -1;

  REQUIRE_IO_MODE (-1);

  TVLOCK;

  tv_clear_error (info);

  if (info->capture.dequeue_buffer)
    RETURN_UNTVLOCK(info->capture.dequeue_buffer (info, b, timeout));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* Empties all filled but not yet dequeued buffers. */
tv_bool
tv_flush_capture_buffers	(tveng_device_info *	info)
{
  assert (NULL != info);

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return FALSE;

  REQUIRE_IO_MODE (FALSE);

  TVLOCK;

  tv_clear_error (info);

  if (info->capture.flush_buffers)
    RETURN_UNTVLOCK(info->capture.flush_buffers (info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return FALSE;
}

/* 
   Sets the capture buffer to an specific size. returns -1 on
   error. Remember to check the value of width and height since it can
   be different to the one requested. 
*/
int tveng_set_capture_size(unsigned int width,
			   unsigned int height,
			   tveng_device_info * info)
{
	tv_image_format format;

	format = info->capture.format;

	format.width = width;
	format.height = height;

	return tv_set_capture_format (info, &format) ? 0 : -1;
}

/* 
   Gets the actual size of the capture buffer in width and height.
   -1 on error
*/
int tveng_get_capture_size(int *width, int *height, tveng_device_info * info)
{
  assert (NULL != info);

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return -1;

  assert (NULL != width);
  assert (NULL != height);

  REQUIRE_IO_MODE (-1);

  TVLOCK;

  tv_clear_error (info);

  if (!info->capture.get_format)
    {
      TVUNSUPPORTED;
      UNTVLOCK;
      return -1;
    }

  if (-1 == info->capture.get_format(info))
    {
      UNTVLOCK;
      return -1;
    }

  if (width)
    *width = info->capture.format.width;
  if (height)
    *height = info->capture.format.height;

  UNTVLOCK;
  return 0;
}

/*
 *  Overlay
 */

/* DMA overlay is dangerous, it bypasses kernel memory protection.
   This function checks if the programmed (dma) and required (dga)
   overlay targets match. */
static tv_bool
validate_overlay_buffer		(const tv_overlay_buffer *dga,
				 const tv_overlay_buffer *dma)
{
	unsigned long dga_end;
	unsigned long dma_end;

	if (0 == dga->base
	    || dga->format.width < 32 || dga->format.height < 32
	    /* XXX width * bytes_per_pixel */
	    || dga->format.bytes_per_line[0] < dga->format.width
	    || dga->format.size <
	    (dga->format.bytes_per_line[0] * dga->format.height))
		return FALSE;

	if (0 == dma->base
	    || dma->format.width < 32 || dma->format.height < 32
	    /* XXX width * bytes_per_pixel */
	    || dma->format.bytes_per_line[0] < dma->format.width
	    || dma->format.size <
	    (dma->format.bytes_per_line[0] * dma->format.height))
		return FALSE;

	dga_end = dga->base + dga->format.size;
	dma_end = dma->base + dma->format.size;

	if (dma_end < dga->base
	    || dma->base >= dga_end)
		return FALSE;

	if (!dga->format.pixel_format
	    || TV_PIXFMT_NONE == dga->format.pixel_format->pixfmt)
		return FALSE;

	if (dga->format.bytes_per_line[0] != dma->format.bytes_per_line[0]
	    || dga->format.pixel_format != dma->format.pixel_format)
		return FALSE;

	/* Adjust? */
	if (dga->base != dma->base
	    || dga->format.width != dma->format.width
	    || dga->format.height != dma->format.height)
		return FALSE;

	return TRUE;
}

const tv_overlay_buffer *
tv_cur_overlay_buffer		(tveng_device_info *	info)
{
	assert (NULL != info);
	return &info->overlay.buffer;
}

const tv_overlay_buffer *
tv_get_overlay_buffer		(tveng_device_info *	info)
{
	assert (NULL != info);

	if (TVENG_CONTROLLER_NONE == info->current_controller)
		return FALSE;

	CLEAR (info->overlay.buffer); /* unusable */

	REQUIRE_IO_MODE (FALSE);
	REQUIRE_SUPPORT (info->overlay.get_buffer, FALSE);

	if (!(info->caps.flags & TVENG_CAPS_OVERLAY))
		return FALSE;

	TVLOCK;

	tv_clear_error (info);

	if (info->overlay.get_buffer (info))
		RETURN_UNTVLOCK (&info->overlay.buffer);
	else
		RETURN_UNTVLOCK (NULL);
}

/* If zapping_setup_fb must be called it will get display_name and
   screen_number as parameters. If display_name is NULL it will default
   to the DISPLAY env. screen_number is intended to choose a Xinerama
   screen, can be -1 to get the default. */
tv_bool
tv_set_overlay_buffer		(tveng_device_info *	info,
				 const char *		display_name,
				 int			screen_number,
				 const tv_overlay_buffer *target)
{
	const char *argv[20];
	char buf1[16];
	char buf2[16];
	pid_t pid;
	int status;
	int r;

	assert (NULL != info);

	if (TVENG_CONTROLLER_NONE == info->current_controller)
		return FALSE;

	assert (NULL != target);

	tv_clear_error (info);

	REQUIRE_IO_MODE (FALSE);
	REQUIRE_SUPPORT (info->overlay.get_buffer, FALSE);

	TVLOCK;

  tv_clear_error (info);

	/* We can save a lot work if the target is already
	   initialized. */

	if (info->overlay.get_buffer (info)) {
		if (validate_overlay_buffer (target, &info->overlay.buffer))
			goto success;

		/* We can save a lot work if the driver supports
		   set_overlay directly. TODO: Test if we actually have
		   access permission. */

		if (info->overlay.set_buffer)
			RETURN_UNTVLOCK (info->overlay.set_buffer 
					 (info, target));
	}

	/* Delegate to suid root zapping_setup_fb helper program. */

	{
		unsigned int argc;
		int i;

		argc = 0;

		argv[argc++] = "zapping_setup_fb";

		argv[argc++] = "-d";
		argv[argc++] = info->file_name;

		if (display_name) {
			argv[argc++] = "-D";
			argv[argc++] = display_name;
		}

		if (screen_number >= 0) {
			snprintf (buf1, sizeof (buf1), "%d", screen_number);
			argv[argc++] = "-S";
			argv[argc++] = buf1;
		}

		if (-1 != info->bpp) {
			snprintf (buf2, sizeof (buf2), "%d", info->bpp);
			argv[argc++] = "--bpp";
			argv[argc++] = buf2;
		}

		i = MIN (info->zapping_setup_fb_verbosity, 2);
		while (i-- > 0)
			argv[argc++] = "-v";

		argv[argc] = NULL;
	}

	{
		gboolean dummy;
		/* FIXME need a safer solution. */
		/* Could temporarily switch to control attach_mode. */
		assert (NULL != info->file_name);
		p_tveng_stop_everything (info, &dummy);
		device_close (info->log_fp, info->fd);
		info->fd = -1;
	}

	switch ((pid = fork ())) {
	case -1: /* error */
		info->tveng_errno = errno;
		t_error("fork()", info);
		goto failure;

	case 0: /* in child */
	  	/* Try in $PATH. Note this might be a consolehelper link. */
	  	r = execvp ("zapping_setup_fb", (char **) argv);

		if (-1 == r && ENOENT == errno) {
			/* Try the zapping_setup_fb install path.
			   Might fail due to missing SUID root, hence
			   second choice. */
		        r = execvp (PACKAGE_ZSFB_DIR "/zapping_setup_fb",
				    (char **) argv);

			if (-1 == r && ENOENT == errno)
				_exit (2);
		}

		_exit (3); /* zapping setup_fb on error returns 1 */

	default: /* in parent */
		while (-1 == (r = waitpid (pid, &status, 0))
		       && EINTR == errno)
			;
		break;
	}

	{
		/* FIXME need a safer solution. */
		info->fd = device_open (info->log_fp, info->file_name,
					O_RDWR, 0);
		assert (-1 != info->fd);
	}

	if (-1 == r) {
		info->tveng_errno = errno;
		t_error ("waitpid", info);
		goto failure;
	}

	if (!WIFEXITED (status)) {
		info->tveng_errno = errno;
		tv_error_msg (info, _("Cannot execute zapping_setup_fb."));
		goto failure;
	}

	switch (WEXITSTATUS (status)) {
	case 0: /* ok */
		if (!info->overlay.get_buffer (info))
			goto failure;

		if (!validate_overlay_buffer (target, &info->overlay.buffer)) {
			tv_error_msg (info, _("zapping_setup_fb failed."));
			goto failure;
		}

		break;

	case 1: /* zapping_setup_fb failure */
		info->tveng_errno = -1;
		tv_error_msg (info, _("zapping_setup_fb failed."));
		goto failure;

	case 2: /* zapping_setup_fb ENOENT */
		info->tveng_errno = ENOENT;
		tv_error_msg (info, _("zapping_setup_fb not found in \"%s\""
				      " or executable search path."),
			      PACKAGE_ZSFB_DIR);
		goto failure;

	default:
		info->tveng_errno = -1;
		tv_error_msg (info, _("Unknown error in zapping_setup_fb."));
	failure:
		RETURN_UNTVLOCK (FALSE);
	}

 success:
	RETURN_UNTVLOCK (TRUE);
}

static tv_bool
overlay_window_visible		(const tveng_device_info *info,
				 const tv_window *	w)
{
	if (w->x > (int)(info->overlay.buffer.x
			 + info->overlay.buffer.format.width))
		return FALSE;

	if ((w->x + w->width) <= info->overlay.buffer.x)
		return FALSE;

	if (w->y > (int)(info->overlay.buffer.y
			 + info->overlay.buffer.format.height))
		return FALSE;

	if ((w->y + w->height) <= info->overlay.buffer.y)
		return FALSE;

	return TRUE;
}

const tv_window *
tv_cur_overlay_window		(tveng_device_info *	info)
{
	assert (NULL != info);
	return &info->overlay.window;
}

/*
  Gets the current overlay window parameters.
  Returns -1 on error, and any other value on success.
  info   : The device to use
*/
const tv_window *
tv_get_overlay_window		(tveng_device_info *	info)
{
	assert (NULL != info);

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return NULL;

  REQUIRE_IO_MODE (NULL);

  TVLOCK;

  tv_clear_error (info);

  if (info->current_controller != TVENG_CONTROLLER_XV
      && !overlay_window_visible (info, &info->overlay.window))
    RETURN_UNTVLOCK (&info->overlay.window);

	if (info->overlay.get_window) {
		if (info->overlay.get_window (info))
			RETURN_UNTVLOCK (&info->overlay.window);
		else
			RETURN_UNTVLOCK (NULL);
	}

  TVUNSUPPORTED;
  UNTVLOCK;
  return NULL;
}

static void
init_overlay_window		(tveng_device_info *	info,
				 tv_window *		w)
{
	if (info->dword_align) {
		tv_pixfmt pixfmt;

		pixfmt = info->overlay.buffer.format.pixel_format->pixfmt;

		round_boundary_4 (&w->x,
				  &w->width,
				  pixfmt,
				  info->caps.maxwidth);
	}

	w->width = SATURATE (w->width,
			     info->caps.minwidth,
			     info->caps.maxwidth);

	w->height = SATURATE (w->height,
			      info->caps.minheight,
			      info->caps.maxheight);

	if (0)
		fprintf (stderr,
			 "buffer %u, %u - %u, %u\n"
			 "window %d, %d - %d, %d (%u x %u)\n",
			 info->overlay.buffer.x,
			 info->overlay.buffer.y,
			 info->overlay.buffer.x
			 + info->overlay.buffer.format.width,
			 info->overlay.buffer.y
			 + info->overlay.buffer.format.height,
			 w->x,
			 w->y,
			 w->x + w->width,
			 w->y + w->height,
			 w->width,
			 w->height);
}

static const tv_window *
p_tv_set_overlay_window		(tveng_device_info *	info,
				 const tv_window *	window,
				 const tv_clip_vector *	clip_vector)
{
	tv_window win;
	tv_clip_vector vec;
	int bx2;
	int by2;

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return NULL;

  REQUIRE_IO_MODE (NULL);
  REQUIRE_SUPPORT (info->overlay.set_window_clipvec, NULL);

	win = *window;

	init_overlay_window (info, &win);

	if (!overlay_window_visible (info, &win)) {
		info->overlay.window = win;
		return &info->overlay.window; /* nothing to do */
	}

	/* Make sure we clip against overlay buffer bounds. */

	if (clip_vector) {
		if (!tv_clip_vector_copy (&vec, clip_vector))
			goto failure;
	} else {
		tv_clip_vector_init (&vec);
	}

	if (win.x < (int) info->overlay.buffer.x)
		if (!tv_clip_vector_add_clip_xy
		    (&vec, 0, 0, info->overlay.buffer.x - win.x, win.height))
			goto failure;

	bx2 = info->overlay.buffer.x + info->overlay.buffer.format.width;

	if ((win.x + (int) win.width) > bx2)
		if (!tv_clip_vector_add_clip_xy
		    (&vec, (unsigned int)(bx2 - win.x), 0,
		     win.width, win.height))
			goto failure;

	if (win.y < (int) info->overlay.buffer.y)
		if (!tv_clip_vector_add_clip_xy
		    (&vec, 0, 0, win.width, info->overlay.buffer.y - win.y))
			goto failure;

	by2 = info->overlay.buffer.y + info->overlay.buffer.format.height;

	if ((win.y + (int) win.height) > by2)
		if (!tv_clip_vector_add_clip_xy
		    (&vec, 0, (unsigned int)(by2 - win.y),
		     win.width, win.height))
			goto failure;

	/* Make sure clips are within bounds and in proper order. */

	if (vec.size > 1) {
		const tv_clip *clip;
		const tv_clip *end;

		end = vec.vector + vec.size - 1;

		for (clip = vec.vector; clip < end; ++clip) {
			assert (clip->x1 < clip->x2);
			assert (clip->y1 < clip->y2);
			assert (clip->x2 <= win.width);
			assert (clip->y2 <= win.height);

			if (clip->y1 == clip[1].y1) {
				assert (clip->y2 == clip[1].y2);
				assert (clip->x2 <= clip[1].x1);
			} else {
				assert (clip->y2 <= clip[1].y2);
			}
		}

		assert (clip->x1 < clip->x2);
		assert (clip->y1 < clip->y2);
		assert (clip->x2 <= win.width);
		assert (clip->y2 <= win.height);
	}

	if (0) {
		const tv_clip *clip;
		const tv_clip *end;

		end = vec.vector + vec.size;

		for (clip = vec.vector; clip < end; ++clip) {
			fprintf (stderr, "clip %u: %u, %u - %u, %u\n",
				 clip - vec.vector,
				 clip->x1, clip->y1,
				 clip->x2, clip->y2);
		}
	}

	if (!tv_clip_vector_set (&info->overlay.clip_vector,
				 clip_vector))
		goto failure;

	if (info->overlay.active)
	  if (!p_tv_enable_overlay (info, FALSE))
	    goto failure;

	if (!info->overlay.set_window_clipvec (info, &win, &vec))
		goto failure;

	tv_clip_vector_destroy (&vec);

	return &info->overlay.window;

 failure:
	tv_clip_vector_destroy (&vec);

	return NULL;
}

/*
  Sets the preview window dimensions to the given window.
  Returns -1 on error, something else on success.
  Success doesn't mean that the requested dimensions are used, maybe
  they are different, check the returned fields to see if they are suitable
  info   : Device we are controlling

  clip_vector: Invisible regions of window, coordinates relative
  to window->x, y. can be NULL.

  The current chromakey value is used, the caller doesn't need to fill
  it in.
*/
const tv_window *
tv_set_overlay_window_clipvec	(tveng_device_info *	info,
				 const tv_window *	window,
				 const tv_clip_vector *	clip_vector)
{
  assert (NULL != info);

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return NULL;

  REQUIRE_IO_MODE (NULL);

  TVLOCK;
  tv_clear_error (info);

  RETURN_UNTVLOCK (p_tv_set_overlay_window (info, window, clip_vector));
}

tv_bool
tv_cur_overlay_chromakey	(tveng_device_info *	info,
				 unsigned int *		chromakey)
{
	assert (NULL != info);
	assert (NULL != chromakey);

	*chromakey = info->overlay.chromakey;

	return TRUE;
}

tv_bool
tv_get_overlay_chromakey	(tveng_device_info *	info,
				 unsigned int *		chromakey)
{
	assert (NULL != info);

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return FALSE;

  REQUIRE_IO_MODE (FALSE);
  REQUIRE_SUPPORT (info->overlay.get_chromakey, FALSE);

  TVLOCK;

  tv_clear_error (info);

	if (!info->overlay.get_chromakey (info))
		RETURN_UNTVLOCK (FALSE);

	if (chromakey)
		*chromakey = info->overlay.chromakey;

	RETURN_UNTVLOCK (TRUE);
}

const tv_window *
tv_set_overlay_window_chromakey	(tveng_device_info *	info,
				 const tv_window *	window,
				 unsigned int		chromakey)
{
	tv_window win;

  assert (NULL != info);

  if (TVENG_CONTROLLER_NONE == info->current_controller)
    return NULL;

  REQUIRE_IO_MODE (NULL);
  REQUIRE_SUPPORT (info->overlay.set_window_chromakey, NULL);

  TVLOCK;

  tv_clear_error (info);

	win = *window;

	init_overlay_window (info, &win);

	if (!overlay_window_visible (info, &win)) {
		info->overlay.window = win;
		return &info->overlay.window; /* nothing to do */
	}

	if (info->overlay.active)
	  if (!p_tv_enable_overlay (info, FALSE))
	    goto failure;

	fprintf (stderr, "set_window_chromakey incomplete\n");
	exit (1);
	/* XXX check if save to DMA without clips. */

	if (!info->overlay.set_window_chromakey (info, &win, chromakey))
		goto failure;

	info->overlay.chromakey = chromakey;

	RETURN_UNTVLOCK (&info->overlay.window);

 failure:
	UNTVLOCK;

	return NULL;
}

tv_bool
tv_set_overlay_xwindow		(tveng_device_info *	info,
				 Window			window,
				 GC			gc)
{
	assert (NULL != info);
	assert (0 != window);
	assert (0 != gc);

	REQUIRE_IO_MODE (FALSE);
	REQUIRE_SUPPORT (info->overlay.set_xwindow, FALSE);

	TVLOCK;

  tv_clear_error (info);

	RETURN_UNTVLOCK (info->overlay.set_xwindow (info, window, gc));
}

tv_bool
p_tv_enable_overlay		(tveng_device_info *	info,
				 tv_bool		enable)
{
	assert (NULL != info);

	if (TVENG_CONTROLLER_NONE == info->current_controller)
		return FALSE;

	REQUIRE_IO_MODE (FALSE);
	REQUIRE_SUPPORT (info->overlay.enable, FALSE);

	enable = !!enable;

	if (info->current_controller != TVENG_CONTROLLER_XV
	    && !overlay_window_visible (info, &info->overlay.window)) {
		info->overlay.active = enable;
		return TRUE;
	}

	if (enable && info->current_controller != TVENG_CONTROLLER_XV) {
		tv_screen *xs;

		if (!info->overlay.get_buffer) {
			support_failure (info, __PRETTY_FUNCTION__);
			return FALSE;
		}

		/* Safety: current target must match some screen. */

		if (!info->overlay.get_buffer (info))
			return FALSE;

		for (xs = screens; xs; xs = xs->next)
			if (validate_overlay_buffer (&xs->target,
						     &info->overlay.buffer))
				break;

		if (!xs) {
			fprintf (stderr, "** %s: Cannot start overlay, "
				 "DMA target is not properly initialized.",
				 __PRETTY_FUNCTION__);
			return FALSE;
		}

		/* XXX should also check if a previous 
		   tveng_set_preview_window () failed. */
	}

	if (!info->overlay.enable (info, enable))
	  return FALSE;

	info->overlay.active = enable;

	return TRUE;
}

tv_bool
tv_enable_overlay		(tveng_device_info *	info,
				 tv_bool		enable)
{
	assert (NULL != info);

	TVLOCK;
  tv_clear_error (info);

	RETURN_UNTVLOCK (p_tv_enable_overlay (info, enable));
}




int
p_tveng_get_display_depth(tveng_device_info * info)
{
  /* This routines are taken form xawtv, i don't understand them very
     well, but they seem to work OK */
  XVisualInfo * visual_info, template;
  XPixmapFormatValues * pf;
  Display * dpy = info->display;
  int found, v, i, n;
  int bpp = 0;

  if (info->bpp != -1)
    return info->bpp;

  /* Use the first screen, should give no problems assuming this */
  template.screen = 0;
  visual_info = XGetVisualInfo(dpy, VisualScreenMask, &template, &found);
  v = -1;
  for (i = 0; v == -1 && i < found; i++)
    if (visual_info[i].class == TrueColor && visual_info[i].depth >=
	15)
      v = i;

  if (v == -1) {
    info -> tveng_errno = -1;
    tv_error_msg(info,"Cannot find an appropiate visual");
    XFree(visual_info);
    return 0;
  }
  
  /* get depth + bpp (heuristic) */
  pf = XListPixmapFormats(dpy, &n);
  for (i = 0; i < n; i++) {
    if (pf[i].depth == visual_info[v].depth) {
      if (visual_info[v].depth == 15)
	bpp = 15; /* here bits_per_pixel is 16, but the depth is 15 */
      else
	bpp = pf[i].bits_per_pixel;
      break;
    }
  }

  if (bpp == 0) {
    info -> tveng_errno = -1;
    tv_error_msg(info, "Cannot figure out X depth");
    XFree(visual_info);
    XFree(pf);
    return 0;
  }

  XFree(visual_info);
  XFree(pf);

  return bpp;
}

/* 
   This is a convenience function, it returns the real screen depth in
   BPP (bits per pixel). This one is quite important for 24 and 32 bit
   modes, since the default X visual may be 24 bit and the real screen
   depth 32, thus an expensive RGB -> RGBA conversion must be
   performed for each frame.
   display: the display we want to know its real depth (can be
   accessed through gdk_private->display)
*/
int
tveng_get_display_depth(tveng_device_info * info)
{
  int bpp;

  TVLOCK;

  tv_clear_error (info);

  bpp = p_tveng_get_display_depth (info);

  UNTVLOCK;
  return bpp;
}

/* Adjusts the verbosity value passed to zapping_setup_fb, cannot fail
 */
void
tveng_set_zapping_setup_fb_verbosity(int level, tveng_device_info *
				     info)
{
  assert (NULL != info);

  if (level > 2)
    level = 2;
  else if (level < 0)
    level = 0;
  info->zapping_setup_fb_verbosity = level;
}


/* Sets the dword align flag */
void tveng_set_dword_align(int dword_align, tveng_device_info *info)
{
  info->dword_align = dword_align;
}

/* Returns the current verbosity value passed to zapping_setup_fb */
int
tveng_get_zapping_setup_fb_verbosity(tveng_device_info * info)
{
  return (info->zapping_setup_fb_verbosity);
}

capture_mode 
p_tveng_stop_everything (tveng_device_info *       info,
			 gboolean * overlay_was_active)
{
  capture_mode returned_mode;

  returned_mode = info->capture_mode;

  switch (info->capture_mode)
    {
    case CAPTURE_MODE_READ:
      *overlay_was_active = FALSE;
      if (info->capture.enable)
	info->capture.enable (info, FALSE);
      assert (CAPTURE_MODE_NONE == info->capture_mode);
      break;

    case CAPTURE_MODE_OVERLAY:
      *overlay_was_active = info->overlay.active;
      /* No error checking */
      if (info->overlay.active)
	p_tv_enable_overlay (info, FALSE);
      info -> capture_mode = CAPTURE_MODE_NONE;
      break;

    case CAPTURE_MODE_TELETEXT:
      /* nothing */
      break;

    default:
      assert (CAPTURE_MODE_NONE == info->capture_mode);
      break;
    };

  return returned_mode;
}

/*
  tveng INTERNAL function, stops the capture or the previewing. Returns the
  mode the device was before stopping.
  For stopping and restarting the device do:
  capture_mode cur_mode;
  cur_mode = tveng_stop_everything(info);
  ... do some stuff ...
  if (tveng_restart_everything(cur_mode, info) == -1)
     ... show error dialog ...
*/
capture_mode 
tveng_stop_everything (tveng_device_info *       info,
		       gboolean *overlay_was_active)
{
  capture_mode returned_mode;

  assert (NULL != info);

  TVLOCK;

  tv_clear_error (info);

  returned_mode = p_tveng_stop_everything (info, overlay_was_active);

  UNTVLOCK;

  return returned_mode;
}

/* FIXME overlay_was_active = info->overlay_active
   because mode can be _PREVIEW or _WINDOW without
  overlay_active due to delay timer. Don't reactivate prematurely. */
int p_tveng_restart_everything (capture_mode mode,
				gboolean overlay_was_active,
				tveng_device_info * info)
{
  switch (mode)
    {
    case CAPTURE_MODE_READ:
	    /* XXX REQUIRE_IO_MODE (-1); */
      if (info->capture.enable)
	if (!info->capture.enable (info, TRUE))
	  return -1;
      break;

    case CAPTURE_MODE_OVERLAY:
      if (info->capture_mode != mode)
	{
	  gboolean dummy;

	  p_tveng_stop_everything(info, &dummy);

	  if (overlay_was_active)
	    {
	      p_tv_set_overlay_window (info,
				       &info->overlay.window,
				       &info->overlay.clip_vector);

	      if (!p_tv_enable_overlay (info, TRUE))
		return (-1);
	    }

	  info->capture_mode = mode;
	}
      break;

    case CAPTURE_MODE_TELETEXT:
      info->capture_mode = mode;
      break;

    default:
      break;
    }

  overlay_was_active = FALSE;

  return 0; /* Success */
}

/*
  Restarts the given capture mode. See the comments on
  tveng_stop_everything. Returns -1 on error.
*/
int tveng_restart_everything (capture_mode mode,
			      gboolean overlay_was_active,
			      tveng_device_info * info)
{
  assert (NULL != info);

  TVLOCK;
  tv_clear_error (info);

  RETURN_UNTVLOCK (p_tveng_restart_everything
		   (mode, overlay_was_active, info));
}

int tveng_get_debug_level(tveng_device_info * info)
{
  assert (NULL != info);

  return (info->debug_level);
}

void tveng_set_debug_level(int level, tveng_device_info * info)
{
  assert (NULL != info);

  info->debug_level = level;
}

void tveng_set_xv_support(int disabled, tveng_device_info * info)
{
  assert (NULL != info);

  info->disable_xv_video = disabled;
}


int
tveng_ov511_get_button_state (tveng_device_info *info)
{
	if (!info) return -1;
/*  assert (NULL != info); */

  if (info->current_controller == TVENG_CONTROLLER_NONE)
    return -1;

  TVLOCK;

  tv_clear_error (info);

  if (info->module.ov511_get_button_state)
    RETURN_UNTVLOCK(info->module.ov511_get_button_state(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

void tveng_mutex_lock(tveng_device_info * info)
{
  TVLOCK;
}

void tveng_mutex_unlock(tveng_device_info * info)
{
  UNTVLOCK;
}



struct _tv_callback {
	tv_callback *		next;
	tv_callback **		prev_next;
	tv_callback_fn *	notify;
	tv_callback_fn *	destroy;
	void *			user_data;
	unsigned int		blocked;
};

/*
 *  Traverse a callback list and call all notify functions,
 *  usually on some event. Only tveng calls this.
 */
void
tv_callback_notify		(tveng_device_info *	info,
				 void *			object,
				 const tv_callback *	list)
{
	const tv_callback *cb;

	assert (NULL != object);

	if (info)
		++info->callback_recursion;

	for (cb = list; cb; cb = cb->next)
		if (cb->notify && cb->blocked == 0)
			/* XXX needs ref counter? */
			cb->notify (object, cb->user_data);

	if (info)
		--info->callback_recursion;
}

/*
 *  Device nodes
 */

tv_device_node *
tv_device_node_add		(tv_device_node **	list,
				 tv_device_node *	node)
{
        assert (NULL != list);

	if (!node)
		return NULL;

	while (*list)
		list = &(*list)->next;

	*list = node;
	node->next = NULL;

	return node;
}

tv_device_node *
tv_device_node_remove		(tv_device_node **	list,
				 tv_device_node *	node)
{
	if (node) {
		while (*list && *list != node)
			list = &(*list)->next;

		*list = node->next;
		node->next = NULL;
	}

	return node;
}

void
tv_device_node_delete		(tv_device_node **	list,
				 tv_device_node *	node,
				 tv_bool		restore)
{
	if (node) {
		if (list) {
			while (*list && *list != node)
				list = &(*list)->next;

			*list = node->next;
		}

		if (node->destroy)
			node->destroy (node, restore);
	}
}

static void
destroy_device_node		(tv_device_node *	node,
				 tv_bool		restore _unused_)
{
	if (!node)
		return;

	free ((char *) node->label);
	free ((char *) node->bus);
	free ((char *) node->driver);
	free ((char *) node->version);
	free ((char *) node->device);
	free (node);
}

tv_device_node *
tv_device_node_new		(const char *		label,
				 const char *		bus,
				 const char *		driver,
				 const char *		version,
				 const char *		device,
				 unsigned int		size)
{
	tv_device_node *node;

	if (!(node = calloc (1, MAX (size, sizeof (*node)))))
		return NULL;

	node->destroy = destroy_device_node;

#define STR_COPY(s) if (s && !(node->s = strdup (s))) goto error;

	STR_COPY (label);
	STR_COPY (bus);
	STR_COPY (driver);
	STR_COPY (version);
	STR_COPY (device);

	return node;

 error:
	destroy_device_node (node, FALSE);
	return NULL;
}

tv_device_node *
tv_device_node_find		(tv_device_node *	list,
				 const char *		name)
{
	struct stat st;

	if (-1 == stat (name, &st))
		return FALSE;

	for (; list; list = list->next) {
		struct stat lst;

		if (0 == strcmp (list->device, name))
			return list;

		if (-1 == stat (list->device, &lst))
			continue;

		if ((S_ISCHR (lst.st_mode) && S_ISCHR (st.st_mode))
		    || (S_ISBLK (lst.st_mode) && S_ISBLK (st.st_mode))) {
			/* Major and minor device number */
			if (lst.st_rdev == st.st_rdev)
				return list;
		}
	}

	return NULL;
}

#if 0
	tv_device_node *head = NULL;
	struct dirent dirent, *pdirent = &dirent;
	DIR *dir = NULL;

	t_assert (NULL != filter);

	if (!list)
		list = &head;

	for (; names && *names && names[0][0]; names++) {
		tv_device_node *node;

		if (find_node (*list, *names))
			continue;

		if ((node = filter (*names)))
			tv_device_node_append (list, node);
	}

	if (path && (dir = opendir (path))) {
		while (0 == readdir_r (dir, &dirent, &pdirent) && pdirent) {
			tv_device_node *node;
			char *s;

			if (!(s = malloc (strlen (path) + strlen (dirent.d_name) + 2)))
				continue;

			strcpy (s, path);
			if (s[0])
				strcat (s, "/");
			strcat (s, dirent.d_name);

			if (find_node (*list, s))
				continue;

			if ((node = filter (s)))
				tv_device_node_append (list, node);
		}

		closedir (dir);
	}

	return *list;
#endif

#define ADD_NODE_CALLBACK_FUNC(item)					\
tv_callback *								\
tv_add_##item##_callback	(tveng_device_info *	info,		\
				 void			(* notify)	\
				   (tveng_device_info *, void *),	\
				 void			(* destroy)	\
				   (tveng_device_info *, void *),	\
				 void *			user_data)	\
{									\
	assert (NULL != info);						\
									\
	return tv_callback_add (&info->panel.item##_callback,		\
				(tv_callback_fn *) notify,		\
				(tv_callback_fn *) destroy,		\
				user_data);				\
}

ADD_NODE_CALLBACK_FUNC (video_input);
ADD_NODE_CALLBACK_FUNC (audio_input);
ADD_NODE_CALLBACK_FUNC (video_standard);


/* Helper function for backends. Yes, it's inadequate, but
   that's how some drivers work. Note the depth 32 exception. */
tv_pixfmt
pig_depth_to_pixfmt		(unsigned int		depth)
{
	switch (Z_BYTE_ORDER) {
	case Z_LITTLE_ENDIAN:
		switch (depth) {
		case 15:	return TV_PIXFMT_BGRA16_LE;
		case 16:	return TV_PIXFMT_BGR16_LE;
		case 24:	return TV_PIXFMT_BGR24_LE;
		case 32:	return TV_PIXFMT_BGRA32_LE;
		}

		break;

	case Z_BIG_ENDIAN:
		switch (depth) {
		case 15:	return TV_PIXFMT_BGRA16_BE;
		case 16:	return TV_PIXFMT_BGR16_BE;
		case 24:	return TV_PIXFMT_BGR24_BE;
		case 32:	return TV_PIXFMT_BGRA32_BE;
		}

		break;
	}

	return TV_PIXFMT_UNKNOWN;
}

const char *
tv_videostd_name		(tv_videostd		videostd)
{
	switch (videostd) {
     	case TV_VIDEOSTD_PAL_B:		return "PAL_B";
	case TV_VIDEOSTD_PAL_B1:	return "PAL_B1";
	case TV_VIDEOSTD_PAL_G:		return "PAL_G";
	case TV_VIDEOSTD_PAL_H:		return "PAL_H";
	case TV_VIDEOSTD_PAL_I:		return "PAL_I";
	case TV_VIDEOSTD_PAL_D:		return "PAL_D";
	case TV_VIDEOSTD_PAL_D1:	return "PAL_D1";
	case TV_VIDEOSTD_PAL_K:		return "PAL_K";
	case TV_VIDEOSTD_PAL_M:		return "PAL_M";
	case TV_VIDEOSTD_PAL_N:		return "PAL_N";
	case TV_VIDEOSTD_PAL_NC:	return "PAL_NC";
	case TV_VIDEOSTD_NTSC_M:	return "NTSC_M";
	case TV_VIDEOSTD_NTSC_M_JP:	return "NTSC_M_JP";
	case TV_VIDEOSTD_SECAM_B:	return "SECAM_B";
	case TV_VIDEOSTD_SECAM_D:	return "SECAM_D";
	case TV_VIDEOSTD_SECAM_G:	return "SECAM_G";
	case TV_VIDEOSTD_SECAM_H:	return "SECAM_H";
	case TV_VIDEOSTD_SECAM_K:	return "SECAM_K";
	case TV_VIDEOSTD_SECAM_K1:	return "SECAM_K1";
	case TV_VIDEOSTD_SECAM_L:	return "SECAM_L";

	case TV_VIDEOSTD_CUSTOM_BEGIN:
	case TV_VIDEOSTD_CUSTOM_END:
		break;
		
		/* No default, gcc warns. */
	}

	return NULL;
}

/*
 *  Helper functions
 */

#define FREE_NODE(p)							\
do {									\
	if (p->label)							\
		free (p->label);					\
									\
	/* CLEAR (*p); */						\
									\
	free (p);							\
} while (0)

#define FREE_NODE_FUNC(kind)						\
void									\
free_##kind			(tv_##kind *		p)		\
{									\
	tv_callback_delete_all (p->_callback, 0, 0, 0, p);		\
									\
	FREE_NODE (p);							\
}

#define FREE_LIST(kind)							\
do {									\
	tv_##kind *p;							\
									\
	while ((p = *list)) {						\
		*list = p->_next;					\
		free_##kind (p);					\
	}								\
} while (0)

#define FREE_LIST_FUNC(kind)						\
void									\
free_##kind##_list		(tv_##kind **		list)		\
{									\
	FREE_LIST (kind);						\
}

#define STORE_CURRENT(item, kind, p)					\
do {									\
	if (info->panel.cur_##item != p) {				\
		info->panel.cur_##item = (tv_##kind *) p;		\
		tv_callback_notify (info, info,				\
				    info->panel.item##_callback);	\
	}								\
} while (0)

#define FREE_ITEM_FUNC(item, kind)					\
void									\
free_##item##s			(tveng_device_info *	info)		\
{									\
	STORE_CURRENT (item, kind, NULL); /* unknown */			\
									\
	free_##kind##_list (&info->panel.item##s);			\
}

#define ALLOC_NODE(p, size, _label, _hash)				\
do {									\
	assert (size >= sizeof (*p));					\
	if (!(p = calloc (1, size)))					\
		return NULL;						\
									\
	assert (_label != NULL);					\
	if (!(p->label = strdup (_label))) {				\
		free (p);						\
		return NULL;						\
	}								\
									\
	p->hash = _hash;						\
} while (0)

static void
hash_warning			(const char *		label1,
				 const char *		label2,
				 unsigned int		hash)
{
	fprintf (stderr,
		 "WARNING: TVENG: Hash collision between %s and %s (0x%x)\n"
		 "please send a bug report the maintainer!\n",
		 label1, label2, hash);
}

#define STORE_CURRENT_FUNC(item, kind)					\
void									\
store_cur_##item		(tveng_device_info *	info,		\
				 const tv_##kind *	p)		\
{									\
									\
	assert (NULL != info);			        		\
	assert (NULL != p);	  					\
									\
	STORE_CURRENT (item, kind, p);					\
}

#define APPEND_NODE(list, p)						\
do {									\
	while (*list) {							\
		if ((*list)->hash == p->hash)				\
			hash_warning ((*list)->label,			\
				      p->label,	p->hash);		\
		list = &(*list)->_next;					\
	}								\
									\
	*list = p;							\
} while (0)

void
free_control_list		(tv_control **		list)
{
	tv_control *p;

	while ((p = *list)) {
		*list = p->_next;
		tv_control_delete (p);
	}
}
void
free_panel_controls		(tveng_device_info *	info)
{
	free_control_list (&info->panel.controls);
}

tv_control *
append_panel_control		(tveng_device_info *	info,
				 tv_control *		tc,
				 unsigned int		size)
{
	tv_control **list;

	for (list = &info->panel.controls; *list; list = &(*list)->_next)
		;

	if (size > 0) {
		assert (size >= sizeof (**list));

		*list = malloc (size);

		if (!*list) {
			info->tveng_errno = errno;
			t_error ("malloc", info);
			return NULL;
		}

		memcpy (*list, tc, size);

		tc = *list;
	} else {
		*list = tc;
	}

	tc->_next = NULL;
	tc->_parent = info;

	return tc;
}



static tv_bool
add_menu_item			(tv_control *		tc,
				 const char *		item)
{
	char **new_menu;
	char *s;

	if (!(s = strdup (item)))
		return FALSE;

	new_menu = realloc (tc->menu, sizeof (*tc->menu) * (tc->maximum + 3));

	if (!new_menu) {
		free (s);
		return FALSE;
	}

	tc->menu = new_menu;

	new_menu[tc->maximum + 1] = s;
	new_menu[tc->maximum + 2] = NULL;

	++tc->maximum;

	return TRUE;
}

struct amtc {
	tv_control		pub;
	tv_audio_capability	cap;
};

/* Preliminary. */
tv_control *
append_audio_mode_control	(tveng_device_info *	info,
				 tv_audio_capability	cap)
{
	struct amtc *amtc;

	if (!(amtc = calloc (1, sizeof (*amtc))))
		return NULL;

	amtc->pub.type = TV_CONTROL_TYPE_CHOICE;
	amtc->pub.id = TV_CONTROL_ID_AUDIO_MODE;

	if (!(amtc->pub.label = strdup (_("Audio")))) {
		tv_control_delete (&amtc->pub);
		return NULL;
	}

	amtc->cap = cap;

	amtc->pub.maximum = -1;
	amtc->pub.step = 1;

	if (cap & TV_AUDIO_CAPABILITY_AUTO) {
		if (!add_menu_item (&amtc->pub, N_("Automatic"))) {
			tv_control_delete (&amtc->pub);
			return NULL;
		}
	}

	if (cap & TV_AUDIO_CAPABILITY_MONO) {
		if (!add_menu_item (&amtc->pub, N_("Mono"))) {
			tv_control_delete (&amtc->pub);
			return NULL;
		}
	}

	if (cap & TV_AUDIO_CAPABILITY_STEREO) {
		if (!add_menu_item (&amtc->pub, N_("Stereo"))) {
			tv_control_delete (&amtc->pub);
			return NULL;
		}
	}

	if (cap & (TV_AUDIO_CAPABILITY_SAP |
		   TV_AUDIO_CAPABILITY_BILINGUAL)) {
		if (!add_menu_item (&amtc->pub, N_("Language 2"))) {
			tv_control_delete (&amtc->pub);
			return NULL;
		}
	}

	append_panel_control (info, &amtc->pub, 0);

	return &amtc->pub;
}

tv_bool
set_audio_mode_control		(tveng_device_info *	info,
				 tv_control *		control,
				 int			value)
{
	struct amtc *amtc = PARENT (control, struct amtc, pub);

	assert (TV_CONTROL_TYPE_CHOICE == control->type
		&& TV_CONTROL_ID_AUDIO_MODE == control->id);

	control->value = value;

	if (amtc->cap & TV_AUDIO_CAPABILITY_AUTO) {
		if (value-- <= 0)
			return tv_set_audio_mode (info, TV_AUDIO_MODE_AUTO);
	}

	if (amtc->cap & TV_AUDIO_CAPABILITY_MONO) {
		if (value-- <= 0)
			return tv_set_audio_mode (info,
						  TV_AUDIO_MODE_LANG1_MONO);
	}

	if (amtc->cap & TV_AUDIO_CAPABILITY_STEREO) {
		if (value-- <= 0)
			return tv_set_audio_mode (info,
						  TV_AUDIO_MODE_LANG1_STEREO);
	}

	if (amtc->cap & (TV_AUDIO_CAPABILITY_SAP |
			 TV_AUDIO_CAPABILITY_BILINGUAL)) {
		if (value-- <= 0)
			return tv_set_audio_mode (info,
						  TV_AUDIO_MODE_LANG2_MONO);
	}

	return FALSE;
}


FREE_NODE_FUNC (video_standard);
FREE_LIST_FUNC (video_standard);

FREE_ITEM_FUNC (video_standard, video_standard);

void
store_cur_video_standard	(tveng_device_info *	info,
				 const tv_video_standard *p)
{
	assert (NULL != info);

	STORE_CURRENT (video_standard, video_standard, p);
}

tv_video_standard *
append_video_standard		(tv_video_standard **	list,
				 tv_videostd_set	videostd_set,
				 const char *		label,
				 const char *		hlabel,
				 unsigned int		size)
{
	tv_video_standard *ts;
	tv_video_standard *l;

	assert (TV_VIDEOSTD_SET_EMPTY != videostd_set);
	assert (NULL != hlabel);

	ALLOC_NODE (ts, size, label, tveng_build_hash (hlabel));

	ts->videostd_set = videostd_set;

	while ((l = *list)) {
		if (l->hash == ts->hash)
			hash_warning (l->label, ts->label, ts->hash);

		/* saa7134 driver: PAL 0xFF, PAL BG 0x7, PAL I 0x10,
		   PAL DK 0xe0, no bug.
		if (l->videostd_set & ts->videostd_set)
			fprintf (stderr, "WARNING: TVENG: Video standard "
				 "set collision between %s (0x%llx) "
				 "and %s (0x%llx)\n",
				 l->label, l->videostd_set,
				 ts->label, videostd_set);
		*/

		list = &l->_next;
	}

	*list = ts;

	if (videostd_set & TV_VIDEOSTD_SET_525_60) {
		ts->frame_width		= 640;
		ts->frame_height	= 480;
		ts->frame_rate		= 30000 / 1001.0;
		ts->frame_ticks		= 90000 * 1001 / 30000;
	} else {
		ts->frame_width		= 768;
		ts->frame_height	= 576;
		ts->frame_rate		= 25.0;
		ts->frame_ticks		= 90000 * 1 / 25;
	}

	return ts;
}

FREE_NODE_FUNC (audio_line);
FREE_LIST_FUNC (audio_line);

FREE_ITEM_FUNC (audio_input, audio_line);
STORE_CURRENT_FUNC (audio_input, audio_line);

tv_audio_line *
append_audio_line		(tv_audio_line **	list,
				 tv_audio_line_type	type,
				 const char *		label,
				 const char *		hlabel,
				 int			minimum,
				 int			maximum,
				 int			step,
				 int			reset,
				 unsigned int		size)
{
	tv_audio_line *tl;

/* err, there is no other type...
	assert (type != TV_AUDIO_LINE_TYPE_NONE);
*/
	assert (hlabel != NULL);
	assert (maximum >= minimum);
	assert (step >= 0);
	assert (reset >= minimum && reset <= maximum);

	ALLOC_NODE (tl, size, label, tveng_build_hash (hlabel));
	APPEND_NODE (list, tl);

	tl->type	= type;

	tl->minimum	= minimum;
	tl->maximum	= maximum;
	tl->step	= step;
	tl->reset	= reset;

	return tl;
}

FREE_NODE_FUNC (video_line);
FREE_LIST_FUNC (video_line);

FREE_ITEM_FUNC (video_input, video_line);

STORE_CURRENT_FUNC (video_input, video_line);

tv_video_line *
append_video_line		(tv_video_line **	list,
				 tv_video_line_type	type,
				 const char *		label,
				 const char *		hlabel,
				 unsigned int		size)
{
	tv_video_line *tl;

	assert (type != TV_VIDEO_LINE_TYPE_NONE);
	assert (hlabel != NULL);

	ALLOC_NODE (tl, size, label, tveng_build_hash (hlabel));
	APPEND_NODE (list, tl);



	tl->type = type;

	return tl;
}

tv_bool
tv_capture_buffer_clear		(tv_capture_buffer *	cb)
{
	assert (NULL != cb);

	return tv_clear_image (cb->data, cb->format);
}
