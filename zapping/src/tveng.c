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
#include <inttypes.h>

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
#include "audio.h"

#include "zmisc.h"
#include "../common/device.h"

#ifndef PRId64
#  define PRId64 "lld"
#endif
#ifndef PRIu64
#  define PRIu64 "llu"
#endif
#ifndef PRIx64
#  define PRIx64 "llx"
#endif

#ifndef TVENG1_RIVATV_TEST
#  define TVENG1_RIVATV_TEST 0
#endif
#ifndef TVENG25_XV_TEST
#  define TVENG25_XV_TEST 0
#endif
#ifndef TVENG1_XV_TEST
#  define TVENG1_XV_TEST 0
#endif

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

typedef struct {
	tv_control		pub;
	tv_control **		prev_next;
	tveng_device_info *	info;
	/* TV card control. */
	tv_control *		video_control;
	/* Graphics card control (not used yet). */
	tv_control *		display_control;
	/* Soundcard control. */
	tv_audio_line *		mixer_line;
} virtual_control;

#define VC(l) PARENT (l, virtual_control, pub)

static tv_control *
control_by_id			(tveng_device_info *	info,
				 tv_control_id		id);

static void
virtual_control_notify_cb	(tv_control *		tc,
				 void *			user_data);
static void
virtual_control_destroy_cb	(tv_control *		tc,
				 void *			user_data);
static void
vc_mixer_line_notify_cb		(tv_audio_line *	line,
				 void *			user_data);
static void
vc_mixer_line_destroy_cb	(tv_audio_line *	line,
				 void *			user_data);

static void
destroy_virtual_control		(virtual_control *	vc)
{
	tv_control *next;

	assert (NULL != vc);

	tv_callback_delete_all (vc->pub._callback,
				/* notify: any */ NULL,
				/* destroy: any */ NULL,
				/* user_data: any */ NULL,
				/* object */ &vc->pub);

	if (vc->video_control) {
		tv_callback_remove_all
			(vc->video_control->_callback,
			 (tv_callback_fn *) virtual_control_notify_cb,
			 (tv_callback_fn *) virtual_control_destroy_cb,
			 /* user_data */ vc);
		vc->video_control = NULL;
	}

	if (vc->mixer_line) {
		tv_callback_remove_all
			(vc->mixer_line->_callback,
			 (tv_callback_fn *) vc_mixer_line_notify_cb,
			 (tv_callback_fn *) vc_mixer_line_destroy_cb,
			 /* user_data */ vc);
		vc->mixer_line = NULL;
	}

	next = vc->pub._next;

	assert (NULL != vc->prev_next);
	*vc->prev_next = next;

	if (next)
		VC(next)->prev_next = vc->prev_next;

	tv_control_delete (&vc->pub);
}

static void
destroy_cloned_controls		(tveng_device_info *	info)
{
	while (info->cloned_controls) {
		virtual_control *vc = VC (info->cloned_controls);

		destroy_virtual_control (vc);
	}
}

/* Called after vc->video changed. */
static void
virtual_control_notify_cb	(tv_control *		tc,
				 void *			user_data)
{
	virtual_control *vc = (virtual_control *) user_data;

	vc->pub.value = tc->value;

	tv_callback_notify (NULL, &vc->pub, vc->pub._callback);
}

/* Called before vc->video disappears. */
static void
virtual_control_destroy_cb	(tv_control *		tc _unused_,
				 void *			user_data)
{
	virtual_control *vc = (virtual_control *) user_data;

	assert (tc == vc->video_control);

	vc->video_control = NULL;

	if (vc->mixer_line) {
		/* Keep going as a mixer control. */
	} else {
		destroy_virtual_control (vc);
	}
}

static virtual_control *
virtual_control_copy			(tv_control *		tc)
{
	virtual_control *vc;

	if (!(vc = malloc (sizeof (*vc)))) {
		return NULL;
	}

	CLEAR (*vc);

	if (!tv_control_copy (&vc->pub, tc)) {
		free (vc);
		return NULL;
	}

	vc->video_control = tc;

	/* Redirect callbacks. */

	assert (NULL == tc->_callback);

	if (!tv_control_add_callback (tc,
				      virtual_control_notify_cb,
				      virtual_control_destroy_cb,
				      /* user_data */ vc)) {
		tv_control_delete (&vc->pub);
		return NULL;
	}

	return vc;
}

static void
insert_virtual_control		(tv_control **		list,
				 virtual_control *	vc)
{
	while (NULL != *list)
		list = &(*list)->_next;

	vc->prev_next = list;
	vc->pub._next = *list;
	*list = &vc->pub;
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
		virtual_control *vc;

		if (!(vc = virtual_control_copy (tc))) {
			goto failure;
		}

		vc->prev_next = prev_next;
		*prev_next = &vc->pub;
		prev_next = &vc->pub._next;

		tc = tc->_next;
	}

	return TRUE;

 failure:
	free_control_list (list);

	return FALSE;
}




typedef void (*tveng_controller)(struct tveng_module_info *info);
static tveng_controller tveng_controllers[] = {
#if TVENG1_RIVATV_TEST || TVENG1_XV_TEST
  tveng1_init_module,
#elif TVENG25_XV_TEST
  tveng25_init_module,
#else
  tvengxv_init_module,
  tveng25_init_module,
  tveng1_init_module,
  tvengbktr_init_module,
#endif
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

  if (0
      && (TVENG1_XV_TEST | TVENG25_XV_TEST)
      && -1 != info->fd
      && NULL != info->module.change_mode)
    {
      if (-1 != info->module.change_mode (info, window, attach_mode))
	goto done;

      info->tveng_errno = -1;
      tv_error_msg (info, "Cannot change to the requested capture mode");
      UNTVLOCK;
      return -1;
    }

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
  info->using_xvideo = FALSE;

  /* See p_tveng_set_capture_format() */
#ifdef ENABLE_BKTR
  /* FIXME bktr VBI capturing does not work if
     video capture format is YUV. */
#define YUVHACK TV_PIXFMT_SET_RGB
#else
#define YUVHACK (TV_PIXFMT_SET (TV_PIXFMT_YUV420) | \
       		 TV_PIXFMT_SET (TV_PIXFMT_YVU420) | \
		 TV_PIXFMT_SET (TV_PIXFMT_NV12) | \
		 TV_PIXFMT_SET (TV_PIXFMT_HM12) | \
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
    if (esd_output || ivtv_audio)
      tveng_attach_mixer_line (info,
			       &audio_loopback_mixer,
			       &audio_loopback_mixer_line);
    else if (mixer && mixer_line)
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
      static const struct {
	const gchar *name;
	guint value;
      } caps [] = {
#undef CAP
#define CAP(name) { #name, TVENG_CAPS_##name }
	CAP (CAPTURE),
	CAP (TUNER),
	CAP (TELETEXT),
	CAP (OVERLAY),
	CAP (CHROMAKEY),
	CAP (CLIPPING),
	CAP (FRAMERAM),
	CAP (SCALES),
	CAP (MONOCHROME),
	CAP (SUBCAPTURE),
	CAP (SUBCAPTURE),
	CAP (QUEUE),
	CAP (XVIDEO),
      };
      guint i;

      short_str = "?";
      long_str = "?";

      fprintf(stderr, "[TVeng] - Info about the video device\n");
      fprintf(stderr, "-------------------------------------\n");

      fprintf(stderr, "Device: %s [%s]\n", info->file_name,
	      info->module.interface_label);
      fprintf(stderr, "Device signature: %x\n", info->signature);
      fprintf(stderr, "Detected framebuffer depth: %d\n",
	      p_tveng_get_display_depth(info));
      fprintf (stderr, "Capabilities:\n  0x%x=",
	       (unsigned int) info->caps.flags);
      for (i = 0; i < N_ELEMENTS (caps); ++i)
	{
	  if (info->caps.flags & caps[i].value)
	    fprintf (stderr, "%s%s", caps[i].name,
		     (info->caps.flags
		      > (int) caps[i].value * 2 - 1) ? "|" : "");
	}
      fprintf (stderr,
	       "\n  channels=%u audios=%u\n"
	       "  min=%ux%u max=%ux%u\n",
	       info->caps.channels,
	       info->caps.audios,
	       info->caps.minwidth, info->caps.minheight,
	       info->caps.maxwidth, info->caps.maxheight);
      fprintf (stderr, "Capture format:\n"
	       "  buffer size            %ux%u pixels, 0x%lx bytes\n"
	       "  bytes per line         %lu, %lu bytes\n"
	       "  offset		 %lu, %lu, %lu bytes\n"
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
#ifdef HAVE_XV_EXTENSION
      fprintf(stderr, "Overlay Xv Port: %d\n",
	      (int) info->overlay.xv_port_id);
#else
      fprintf(stderr, "Overlay Xv Port: Xv not compiled.\n");
#endif
      {
	tv_video_standard *s;
	unsigned int i;

	fprintf (stderr, "Video standards:\n");

	for (s = info->panel.video_standards, i = 0; s; s = s->_next, ++i)
	  fprintf (stderr,
		   "  %d) '%s'  0x%" PRIx64 " %dx%d  %.2f  %u  hash: %x\n",
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

 done:
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
				 const char *		video_file_name,
				 const char *		function_name,
				 unsigned int		video_file_line,
				 const char *		ioctl_name)
{
	info->tveng_errno = errno;

	snprintf (info->error, 255,
		  "%s:%s:%u: ioctl %s failed: %d, %s",
		  video_file_name,
		  function_name,
		  video_file_line,
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
tv_##kind *								\
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
tv_##kind *								\
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
				tv_error_msg (info, "Ambiguous id %" PRIx64,
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
round_boundary_4		(int *			x1,
				 unsigned int *		width,
				 tv_pixfmt		pixfmt,			 
				 unsigned int		max_width)
{
	int x;
	unsigned int w;

	if (!x1) {
		x = 0;
		x1 = &x;
	}

	switch (pixfmt) {
	case TV_PIXFMT_RGBA32_LE:
	case TV_PIXFMT_RGBA32_BE:
	case TV_PIXFMT_BGRA32_LE:
	case TV_PIXFMT_BGRA32_BE:
		x = *x1;
		w = *width;
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
update_xv_control (tveng_device_info * info, virtual_control *vc)
{
  int value;

  switch (vc->pub.id)
    {
    case TV_CONTROL_ID_VOLUME:
    case TV_CONTROL_ID_MUTE:
      {
	tv_audio_line *line;

	if (info->quiet)
	  return 0;

	line = vc->mixer_line;
	assert (NULL != line);

	if (!tv_mixer_line_update (line))
	  return -1;

	if (vc->pub.id == TV_CONTROL_ID_VOLUME)
	  value = (line->volume[0] + line->volume[1] + 1) >> 1;
	else
	  value = !!(line->muted);
      }

      break;

    default:
      return 0;
    }

  if (vc->pub.value != value)
    {
      vc->pub.value = value;
      tv_callback_notify (info, &vc->pub, vc->pub._callback);
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

  if (VC(control)->mixer_line)
    RETURN_UNTVLOCK (update_xv_control (info, VC(control)));

  control = VC(control)->video_control;
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
    {
      if (VC(tc)->mixer_line)
	update_xv_control (info, VC(tc));
    }

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
			info->panel.set_control (info, tc,
						 tc->maximum * 9 / 10);
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
				 virtual_control *	vc,
				 int			value,
				 tv_bool		quiet,
				 tv_bool		reset)
{
  tv_bool success = FALSE;

  if (vc->mixer_line)
    {
      tv_callback *cb;

      cb = vc->mixer_line->_callback;

      if (quiet)
	vc->mixer_line->_callback = NULL;

      switch (vc->pub.id)
	{
	case TV_CONTROL_ID_MUTE:
	  success = tv_mixer_line_set_mute (vc->mixer_line, value);

	  value = vc->mixer_line->muted;

	  if (value)
	    reset = FALSE;

	  break;

	case TV_CONTROL_ID_VOLUME:
	  success = tv_mixer_line_set_volume (vc->mixer_line, value, value);

	  value = (vc->mixer_line->volume[0]
		   + vc->mixer_line->volume[1] + 1) >> 1;
	  break;

	default:
	  assert (0);
	}

      if (quiet)
	{
	  vc->mixer_line->_callback = cb;
	}
      else if (success && vc->pub.value != value)
	{
	  vc->pub.value = value;
	  tv_callback_notify (info, &vc->pub, vc->pub._callback);
	}

      if (success && reset)
	reset_video_volume (info, /* mute */ FALSE);
    }
  else if (info->panel.set_control)
    {
      assert (NULL != vc->video_control);

      if (quiet)
	{
	  tv_callback *cb;
	  int old_value;

	  cb = vc->pub._callback;
	  old_value = vc->pub.value;

	  vc->pub._callback = NULL;

	  success = info->panel.set_control (info, vc->video_control, value);

	  vc->pub.value = old_value;
	  vc->pub._callback = cb;
	}
      else
	{
	  success = info->panel.set_control (info, vc->video_control, value);
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
set_control			(virtual_control * vc,
				 int value,
				 tv_bool reset,
				 tveng_device_info * info)
{
  int r = 0;

  value = SATURATE (value, vc->pub.minimum, vc->pub.maximum);

  /* If the quiet switch is set we remember the
     requested value but don't change driver state. */
  if (info->quiet)
    if (vc->pub.id == TV_CONTROL_ID_VOLUME
	|| vc->pub.id == TV_CONTROL_ID_MUTE)
      goto set_and_notify;

  if (vc->mixer_line)
    {
      if (vc->pub.id == TV_CONTROL_ID_VOLUME
	  || vc->pub.id == TV_CONTROL_ID_MUTE)
	{
	  return set_control_audio (info, vc, value, /* quiet */ FALSE, reset);
	}
      else
	{
	  return 0;
	}

    set_and_notify:
      if (vc->pub.value != value)
	{
	  vc->pub.value = value;
	  tv_callback_notify (info, &vc->pub, vc->pub._callback);
	}

      return r;
    }
  else
    {
      assert (NULL != vc->video_control);
      return set_panel_control (info, vc->video_control, value);
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

  RETURN_UNTVLOCK (set_control (VC(control), value, TRUE, info));
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
      RETURN_UNTVLOCK (set_control (VC(tc), new_value, TRUE, info));

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

/* This is a virtual control. Notify when the underlying
   mixer line changes. */
static void
vc_mixer_line_notify_cb		(tv_audio_line *	line,
				 void *			user_data)
{
	virtual_control *vc = (virtual_control *) user_data;
	int value;

	assert (line == vc->mixer_line);

	switch (vc->pub.id) {
	case TV_CONTROL_ID_VOLUME:
		vc->pub.minimum = line->minimum;
		vc->pub.maximum = line->maximum;
		vc->pub.step = line->step;
		vc->pub.reset = line->reset;

		value = (line->volume[0] + line->volume[1] + 1) >> 1;

		break;

	case TV_CONTROL_ID_MUTE:
		vc->pub.reset = 0;
		vc->pub.minimum = 0;
		vc->pub.maximum = 1;
		vc->pub.step = 1;

		value = line->muted;

		break;

	default:
		assert (0);
	}

	if (vc->info && !vc->info->quiet) {
		vc->pub.value = value;
		tv_callback_notify (vc->info, &vc->pub, vc->pub._callback);
	}
}

/* This is a virtual control. When the underlying mixer line
   disappears, remove the control and make the corresponding
   video device control, if any, visible again. */
static void
vc_mixer_line_destroy_cb	(tv_audio_line *	line,
				 void *			user_data)
{
	virtual_control *vc = (virtual_control *) user_data;

	assert (line == vc->mixer_line);

	vc->mixer_line = NULL;

	if (vc->video_control) {
		/* Restore overridden video device control. */

		vc->pub.minimum = vc->video_control->minimum;
		vc->pub.maximum = vc->video_control->maximum;
		vc->pub.step	= vc->video_control->step;
		vc->pub.reset	= vc->video_control->reset;

		/* XXX which value when info->quiet? */
		vc->pub.value	= vc->video_control->value;

		/* Notify about value and limits change. */
		tv_callback_notify (vc->info, &vc->pub, vc->pub._callback);
	} else {
		if (TV_CONTROL_ID_MUTE == vc->pub.id) {
			vc->info->control_mute = NULL;
			vc->info->audio_mutable = 0; /* preliminary */
		}

		destroy_virtual_control (vc);
	}
}

static virtual_control *
new_virtual_mixer_control	(tveng_device_info *	info,
				 tv_audio_line *	line,
				 tv_control_id		id)
{
	virtual_control *vc;
	tv_callback *cb;
	int value;

	info = info;

	vc = calloc (1, sizeof (*vc));
	assert (NULL != vc); /* XXX */

	vc->pub.id = id;

	switch (id) {
	case TV_CONTROL_ID_VOLUME:
		vc->pub.type = TV_CONTROL_TYPE_INTEGER;

		/* XXX which value when info->quiet? */
		value = (line->volume[0] + line->volume[1] + 1) >> 1;

		if (TVENG_MIXER_VOLUME_DEBUG)
			vc->pub.label = strdup ("Mixer volume");
		else
			vc->pub.label = strdup (_("Volume"));

		break;

	case TV_CONTROL_ID_MUTE:
		vc->pub.type = TV_CONTROL_TYPE_BOOLEAN;

		/* XXX which value when info->quiet? */
		value = line->muted;

		if (TVENG_MIXER_VOLUME_DEBUG)
			vc->pub.label = strdup ("Mixer mute");
		else
			vc->pub.label = strdup (_("Mute"));
			
		break;

	default:
		assert (0);
	}

	assert (NULL != vc->pub.label); /* XXX */

	vc->pub.minimum	= line->minimum;
	vc->pub.maximum	= line->maximum;
	vc->pub.step	= line->step;
	vc->pub.reset	= line->reset;
	vc->pub.value	= value;

	vc->mixer_line = line;

	cb = tv_mixer_line_add_callback (line,
					 vc_mixer_line_notify_cb,
					 vc_mixer_line_destroy_cb, vc);
	assert (NULL != cb); /* XXX */

	return vc;
}

/* When line != NULL this adds a mixer control, hiding the
   corresponding video device (volume or mute) control if any.
   When line == NULL the old state is restored. */
static void
mixer_replace			(tveng_device_info *	info,
				 tv_audio_line *	line,
				 tv_control_id		id)
{
	tv_control *tc;
	virtual_control *vc;
	tv_callback *cb;

	tc = control_by_id (info, id);

	if (!tc) {
		if (line) {
			vc = new_virtual_mixer_control (info, line, id);

			vc->info = info;

			insert_virtual_control (&info->cloned_controls, vc);
		}

		return;
	}

	vc = PARENT (tc, virtual_control, pub);

	if (!line) {
		if (vc->mixer_line) {
			tv_callback_remove_all
			  (vc->mixer_line->_callback,
			   (tv_callback_fn *) vc_mixer_line_notify_cb,
			   (tv_callback_fn *) vc_mixer_line_destroy_cb,
			   /* user_data */ vc);

			/* Pretend the mixer_line will be deleted. */
			vc_mixer_line_destroy_cb (vc->mixer_line, vc);
		}

		return;
	}

	/* Set new mixer line. */

	if (vc->mixer_line)
		tv_callback_remove_all
		  (vc->mixer_line->_callback,
		   (tv_callback_fn *) vc_mixer_line_notify_cb,
		   (tv_callback_fn *) vc_mixer_line_destroy_cb,
		   /* user_data */ vc);

	vc->mixer_line = line;

	cb = tv_mixer_line_add_callback	(line,
					 vc_mixer_line_notify_cb,
					 vc_mixer_line_destroy_cb, vc);
	assert (NULL != cb); /* XXX */

	/* Notify about value and limits change. */
	vc_mixer_line_notify_cb (line, vc);
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

  info->control_mute = NULL;
  info->audio_mutable = FALSE; /* preliminary */

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
	  set_control_audio (info, VC (tc),
			     /* value */ TRUE,
			     /* quiet */ TRUE,
			     /* reset */ FALSE);
	}
      else if ((tc = control_by_id (info, TV_CONTROL_ID_VOLUME)))
	{
	  /* Error ignored */
	  set_control_audio (info, VC (tc),
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
	  set_control_audio (info, VC (tc),
			     tc->value,
			     /* quiet */ FALSE,
			     /* reset */ FALSE);

	  if (VC(tc)->mixer_line) /* uses soundcard mixer */
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
    r = set_control (VC(tc), mute, TRUE, info);
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

tv_bool
tv_enable_capturing		(tveng_device_info *	info,
				 tv_bool		enable)
{
	assert (NULL != info);

	if (TVENG_CONTROLLER_NONE == info->current_controller)
		return FALSE;

	REQUIRE_IO_MODE (FALSE);

	TVLOCK;

	if (!enable && info->capture_mode == CAPTURE_MODE_NONE) {
		fprintf(stderr,
			"Warning: trying to stop capture "
			"with no capture active\n");
		RETURN_UNTVLOCK(-1);
	}

	tv_clear_error (info);

	if (info->capture.enable)
		RETURN_UNTVLOCK(info->capture.enable (info, enable));

	TVUNSUPPORTED;
	UNTVLOCK;
	return FALSE;
}

tv_bool
tv_set_num_capture_buffers	(tveng_device_info *	info,
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
tv_get_num_capture_buffers	(tveng_device_info *	info,
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
 *  Overlay
 */

/* DMA overlay is dangerous, it bypasses kernel memory protection.
   This function checks if the programmed (dma) and required (dga)
   overlay targets match. */
static tv_bool
verify_overlay_buffer		(const tv_overlay_buffer *dga,
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

static int
read_file_from_fd		(char *			buffer,
				 ssize_t		size,
				 int			fd)
{
	ssize_t done, actual;

	assert (size > 1);

	--size; /* space for NUL */
	done = 0;

	/* Read until EOF. */
	/* XXX may block forever. */
	while (0 != (actual = read (fd, buffer + done, size - done))) {
		if (-1 == actual) {
			if (EINTR != errno)
				return -1; /* failed */
			/* XXX EAGAIN possible? */
		} else {
			done += actual;
		}
	}

	/* Make sure we have a NUL terminated string
	   if this is a text file. */
	buffer[done] = 0;

	return 0; /* success */
}

#define ZSFB_NAME "zapping_setup_fb"

/* Keep this in sync with zapping_setup_fb.h. */
typedef enum {
	ZSFB_SUCCESS = EXIT_SUCCESS,
	ZSFB_FAILURE = EXIT_FAILURE,

	/* zapping errors. */
	ZSFB_FORK_ERROR = 40,
	ZSFB_EXEC_ENOENT,
	ZSFB_EXEC_ERROR,
	ZSFB_IO_ERROR,
	ZSFB_BAD_BUFFER,

	/* zapping_setup_fb errors. */
	ZSFB_BUG = 60,
	ZSFB_NO_PERMISSION,
	ZSFB_NO_SCREEN,
	ZSFB_INVALID_BPP,
	ZSFB_BAD_DEVICE_NAME,
	ZSFB_BAD_DEVICE_FD,
	ZSFB_UNKNOWN_DEVICE,
	ZSFB_OPEN_ERROR,
	ZSFB_IOCTL_ERROR,
	ZSFB_OVERLAY_IMPOSSIBLE,
} zsfb_status;

static zsfb_status
exec_zapping_setup_fb_argv	(tveng_device_info *	info,
				 const char **		argv)
{
	int stderr_pipe[2];
	pid_t pid;
	char errmsg[256];
	int status;
	int r;

	stderr_pipe[0] = -1;
	stderr_pipe[1] = -1;

	if (-1 == pipe (stderr_pipe)) {
		info->tveng_errno = errno;
		printv ("pipe() error: %s\n",
			strerror (info->tveng_errno));
		goto fork_error;
	}

	fflush (stderr);

	pid = fork ();

	switch (pid) {
	case -1: /* error */
		info->tveng_errno = errno;

		printv ("fork() error: %s\n",
			strerror (info->tveng_errno));

		/* Error ignored. */
		close (stderr_pipe[1]);
		close (stderr_pipe[0]);

		goto fork_error;

	case 0: /* in child */
	{
		int saved_errno;

		printv ("Forked.\n");

		/* Pipe input is unused in child. Error ignored. */
		close (stderr_pipe[0]);
		stderr_pipe[0] = -1;

		/* Redirect error message to pipe. */
		if (-1 == dup2 (stderr_pipe[1], STDERR_FILENO)) {
			printv ("dup2() error: %s\n", strerror (errno));
			_exit (ZSFB_FORK_ERROR); /* tell parent */
		}

		/* Try in $PATH. Note this might be a consolehelper symlink.
		   Exit status 0 on success, something else on error. */
		execvp (argv[0], (char **) argv);

		/* When execvp returns it failed. */

		saved_errno = errno;

		fputs (strerror (saved_errno), stderr);

		switch (saved_errno) {
		case ENOENT:
			_exit (ZSFB_EXEC_ENOENT);

		default:
			_exit (ZSFB_EXEC_ERROR);
		}

		assert (0);
	}

	default: /* in parent */
		break;
	}

	/* Pipe output is unused in parent. Error ignored. */
	close (stderr_pipe[1]);
	stderr_pipe[1] = -1;

	r = read_file_from_fd (errmsg, sizeof (errmsg), stderr_pipe[0]);
	info->tveng_errno = errno;

	/* Error ignored. */
	close (stderr_pipe[0]);
	stderr_pipe[0] = -1;

	if (-1 == r) {
		printv ("Stderr pipe read error %s\n",
			strerror (info->tveng_errno));

		while (-1 == waitpid (pid, &status, 0)
		       && EINTR == errno)
			;

		goto fork_error;
	}

	while (-1 == (r = waitpid (pid, &status, 0))
	       && EINTR == errno)
		;

	info->tveng_errno = errno;

	if (-1 == r) {
		printv ("waitpid() error %s, errmsg %s\n",
			strerror (info->tveng_errno), errmsg);

		goto fork_error;
	}

	if (!WIFEXITED (status)) {
		printv ("WIFEXITED() error %s, errmsg %s\n",
			strerror (info->tveng_errno), errmsg);

		goto fork_error;
	}

	info->tveng_errno = 0;

	switch ((zsfb_status) WEXITSTATUS (status)) {
	case ZSFB_SUCCESS: /* zapping_setup_fb success */
		break;

	case ZSFB_FORK_ERROR:
		tv_error_msg (info, _("Cannot execute %s. Pipe error."),
			      argv[0]);

		return ZSFB_FORK_ERROR;

	case ZSFB_EXEC_ERROR:
	case ZSFB_EXEC_ENOENT:
		tv_error_msg (info, _("Cannot execute %s. %s."),
			      argv[0], errmsg);

		return (zsfb_status) WEXITSTATUS (status);

	case ZSFB_BUG ... ZSFB_OVERLAY_IMPOSSIBLE:
		printv ("zapping_setup_fb failed with exit status %d\n",
			(int) WEXITSTATUS (status));

		/* TRANSLATORS: Program name, error message. */
		tv_error_msg (info, _("%s failed.\n%s"),
			      argv[0], errmsg);

		return (zsfb_status) WEXITSTATUS (status);

	default:
		printv ("zapping_setup_fb failed with exit status %d\n",
			(int) WEXITSTATUS (status));

		/* Something executed, but it wasn't
		   zapping_setup_fb. Perhaps consolehelper
		   couldn't authenticate the user? */
		tv_error_msg (info, _("Cannot execute %s. "
				      "No permission?"),
			      argv[0]);

		return (zsfb_status) WEXITSTATUS (status);
	}

	return ZSFB_SUCCESS;

 fork_error:
	tv_error_msg (info, _("Cannot execute %s. %s."),
		      argv[0], strerror (info->tveng_errno));

	return ZSFB_FORK_ERROR;
}

static zsfb_status
exec_zapping_setup_fb		(tveng_device_info *	info,
				 const char *		executable_name,
				 const char *		display_name,
				 int			screen_number,
				 const tv_overlay_buffer *target)
{
	const char *argv[20];
	char buf[3][16];
	unsigned int argc;
	unsigned int i;
	int old_flags;
	zsfb_status status;

	argc = 0;

	argv[argc++] = executable_name;

	argv[argc++] = "-c"; /* enable UTF-8 error messages */

	/* Just in case. XXX error ignored. */
	old_flags = fcntl (info->fd, F_GETFD, 0);
	if (-1 != old_flags
	    && (old_flags & FD_CLOEXEC)) {
		old_flags &= ~FD_CLOEXEC;
		/* XXX error ignored. */
		fcntl (info->fd, F_SETFD, old_flags);
	}

	snprintf (buf[0], sizeof (buf[0]), "%d", info->fd);
	argv[argc++] = "-f";
	argv[argc++] = buf[0];

	if (NULL != display_name) {
		argv[argc++] = "-D";
		argv[argc++] = display_name;
	}

	if (screen_number >= 0) {
		snprintf (buf[1], sizeof (buf[1]), "%d", screen_number);
		argv[argc++] = "-S";
		argv[argc++] = buf[1];
	}

	if (-1 != info->bpp) {
		snprintf (buf[2], sizeof (buf[2]), "%d", info->bpp);
		argv[argc++] = "-b";
		argv[argc++] = buf[2];
	}

	argv[argc] = NULL;

	assert (argc <= N_ELEMENTS (argv));

	printv ("Running ");
	for (i = 0; i < argc; ++i)
		printv ("'%s' ", argv[i]);
	printv ("\n");

	status = exec_zapping_setup_fb_argv (info, argv);

	if (ZSFB_BAD_DEVICE_FD == status) {
		gboolean dummy;

                if (NULL == info->file_name)
			return status;

		argv[2] = "-d";
                argv[3] = info->file_name;

                /* XXX this is unnecessary with V4L2. We should just
		   temporarily switch to a panel mode. */

                p_tveng_stop_everything (info, &dummy);

                device_close (info->log_fp, info->fd);
                info->fd = -1;

		printv ("Running ");
		for (i = 0; i < argc; ++i)
			printv ("'%s' ", argv[i]);
		printv ("\n");

		status = exec_zapping_setup_fb_argv (info, argv);

		/* XXX what if this doesn't succeed? */
                info->fd = device_open (info->log_fp, info->file_name,
                                        O_RDWR, 0);
                assert (-1 != info->fd);
	}

	if (ZSFB_SUCCESS == status) {
		if (!info->overlay.get_buffer (info)) {
			tv_error_msg (info, _("Cannot determine current "
					      "overlay parameters. %s"),
				      strerror (info->tveng_errno));

			return ZSFB_IO_ERROR;
		}

		if (!verify_overlay_buffer (target, &info->overlay.buffer)) {
			/* TRANSLATORS: Program name. */
			tv_error_msg (info, _("%s did not work as expected."),
				      argv[0]);

			return ZSFB_BAD_BUFFER;
		}
	}

	return status;
}

/**
 * This function sets the overlay buffer, where images will be stored.
 * When this operation is privileged we run the zapping_setup_fb
 * helper application and pass display_name and screen_number as
 * parameters. If display_name is NULL it will default
 * to the DISPLAY environment variable. screen_number is intended to
 * choose a Xinerama screen, can be -1 to get the default.
 */
tv_bool
tv_set_overlay_buffer		(tveng_device_info *	info,
				 const char *		display_name,
				 int			screen_number,
				 const tv_overlay_buffer *target)
{
	zsfb_status status1;
	zsfb_status status2;

	assert (NULL != info);

	printv ("tv_set_overlay_buffer()\n");

	if (TVENG_CONTROLLER_NONE == info->current_controller)
		return FALSE;

	assert (NULL != target);

	tv_clear_error (info);

	REQUIRE_IO_MODE (FALSE);
	REQUIRE_SUPPORT (info->overlay.get_buffer, FALSE);

	TVLOCK;

	tv_clear_error (info);

	info->using_xvideo = FALSE;

	/* We can save a lot of work if the driver is already
	   initialized for this target. */
	if (info->overlay.get_buffer (info)
	    && verify_overlay_buffer (target, &info->overlay.buffer))
		RETURN_UNTVLOCK (TRUE);

	/* We can still save a lot of work if the driver supports
	   set_overlay directly and this app has the required
	   privileges. */
	if (info->overlay.set_buffer) {
		if (info->overlay.set_buffer (info, target)) {
			RETURN_UNTVLOCK (verify_overlay_buffer
					 (target, &info->overlay.buffer));
		}

		if (EPERM != errno) {
			RETURN_UNTVLOCK (FALSE);
		}
	}

	/* Delegate to suid root ZSFB_NAME helper program. */

	status1 = exec_zapping_setup_fb (info,
					 PACKAGE_ZSFB_DIR "/" ZSFB_NAME,
					 display_name,
					 screen_number,
					 target);
	switch (status1) {
	case ZSFB_SUCCESS:
		break;

	case ZSFB_FORK_ERROR:
	case ZSFB_BUG:
	case ZSFB_NO_SCREEN:
	case ZSFB_INVALID_BPP:
	case ZSFB_UNKNOWN_DEVICE:
	case ZSFB_OPEN_ERROR:
	case ZSFB_IOCTL_ERROR:
	case ZSFB_OVERLAY_IMPOSSIBLE:
		/* Hopeless. */
		goto failure;

	default:
		/* Let's try in $PATH. Note this may be a consolehelper
		   symlink. */

		status2 = exec_zapping_setup_fb (info,
						 ZSFB_NAME,
						 display_name,
						 screen_number,
						 target);
		switch (status2) {
		case ZSFB_SUCCESS:
			break;

		case ZSFB_EXEC_ENOENT:
			if (ZSFB_EXEC_ENOENT == status1) {
				tv_error_msg (info,
					      _("%s not found in %s or the "
						"executable search path."),
					      ZSFB_NAME, PACKAGE_ZSFB_DIR);
			}

			/* fall through */

		default:
			goto failure;
		}
	}

	printv ("tv_set_overlay_buffer() ok\n");

	RETURN_UNTVLOCK (TRUE);

 failure:
	printv ("tv_set_overlay_buffer() failed\n");

	RETURN_UNTVLOCK (FALSE);
}

static tv_bool
overlay_window_visible		(const tveng_device_info *info,
				 const tv_window *	w)
{
	if (unlikely (w->x > (int)(info->overlay.buffer.x
				   + info->overlay.buffer.format.width)))
		return FALSE;

	if (unlikely ((int)(w->x + w->width) <= (int) info->overlay.buffer.x))
		return FALSE;

	if (unlikely (w->y > (int)(info->overlay.buffer.y
				   + info->overlay.buffer.format.height)))
		return FALSE;

	if (unlikely ((int)(w->y + w->height) <= (int) info->overlay.buffer.y))
		return FALSE;

	return TRUE;
}

const tv_window *
tv_cur_overlay_window		(tveng_device_info *	info)
{
	assert (NULL != info);

	return &info->overlay.window;
}

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

const tv_clip_vector *
tv_cur_overlay_clipvec		(tveng_device_info *	info)
{
	assert (NULL != info);

	return &info->overlay.clip_vector;
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

	if (0) {
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
}

/* Be VERY careful here. The bktr driver lets any application which
   has access permission DMA video data to any memory address, and
   barely checks the parameters. */

static tv_bool
add_boundary_clips		(tveng_device_info *	info,
				 tv_clip_vector *	vec,
				 const tv_window *	win)
				 
{
	int bx2;
	int by2;

	if (unlikely (win->x < (int) info->overlay.buffer.x)) {
		unsigned int obscured_width = info->overlay.buffer.x - win->x;

		if (!tv_clip_vector_add_clip_xy
		    (vec, 0, 0, obscured_width, win->height))
			return FALSE;
	}

	bx2 = info->overlay.buffer.x + info->overlay.buffer.format.width;

	if (unlikely ((win->x + (int) win->width) > bx2)) {
		unsigned int visible_width = bx2 - win->x;

		if (!tv_clip_vector_add_clip_xy
		    (vec, visible_width, 0, win->width, win->height))
			return FALSE;
	}

	if (unlikely (win->y < (int) info->overlay.buffer.y)) {
		unsigned int obscured_height = info->overlay.buffer.y - win->y;

		if (!tv_clip_vector_add_clip_xy
		    (vec, 0, 0, win->width, obscured_height))
			return FALSE;
	}

	by2 = info->overlay.buffer.y + info->overlay.buffer.format.height;

	if (unlikely ((win->y + (int) win->height) > by2)) {
		unsigned int visible_height = by2 - win->y;

		if (!tv_clip_vector_add_clip_xy
		    (vec, 0, visible_height, win->width, win->height))
			return FALSE;
	}

	return TRUE;
}

static void
verify_clip_vector		(tv_clip_vector *	vec,
				 tv_window *		win)
{
	const tv_clip *clip;
	const tv_clip *end;

	/* Make sure clips are within bounds and in proper order. */

	if (0 == vec->size)
		return;

	clip = vec->vector;

	if (vec->size > 1) {
		end = vec->vector + vec->size - 1;

		for (; clip < end; ++clip) {
			assert (clip->x1 < clip->x2);
			assert (clip->y1 < clip->y2);
			assert (clip->x2 <= win->width);
			assert (clip->y2 <= win->height);

			if (clip->y1 == clip[1].y1) {
				assert (clip->y2 == clip[1].y2);
				assert (clip->x2 <= clip[1].x1);
			} else {
				assert (clip->y2 <= clip[1].y2);
			}
		}
	}

	assert (clip->x1 < clip->x2);
	assert (clip->y1 < clip->y2);
	assert (clip->x2 <= win->width);
	assert (clip->y2 <= win->height);
}

static const tv_window *
p_tv_set_overlay_window		(tveng_device_info *	info,
				 const tv_window *	window,
				 const tv_clip_vector *	clip_vector)
{
	tv_window win;
	tv_clip_vector new_vec;
	tv_clip_vector safe_vec;

	if (TVENG_CONTROLLER_NONE == info->current_controller)
		return NULL;

	REQUIRE_IO_MODE (NULL);
	REQUIRE_SUPPORT (info->overlay.set_window, NULL);

	info->using_xvideo = FALSE;

	win = *window;

	init_overlay_window (info, &win);

	if (!overlay_window_visible (info, &win)) {
		if (info->overlay.active)
			if (!p_tv_enable_overlay (info, FALSE))
				goto failure;

		info->overlay.window = win;

		return &info->overlay.window; /* nothing to do */
	}

	if (NULL != clip_vector) {
		if (!tv_clip_vector_copy (&new_vec, clip_vector))
			return NULL;

		if (!tv_clip_vector_copy (&safe_vec, clip_vector)) {
			tv_clip_vector_destroy (&new_vec);
			return NULL;
		}
	} else {
		tv_clip_vector_init (&new_vec);
		tv_clip_vector_init (&safe_vec);
	}

	/* Make sure we clip against overlay buffer bounds. */

	if (!add_boundary_clips (info, &safe_vec, &win))
		goto failure;

	verify_clip_vector (&safe_vec, &win);

	if (0)
		_tv_clip_vector_dump (&safe_vec, stderr);

	if (info->overlay.active)
		if (!p_tv_enable_overlay (info, FALSE))
			goto failure;

	if (!info->overlay.set_window (info, &win, &safe_vec,
				       info->overlay.chromakey))
		goto failure;

	tv_clip_vector_destroy (&safe_vec);

	tv_clip_vector_destroy (&info->overlay.clip_vector);
	info->overlay.clip_vector = new_vec;

	return &info->overlay.window;

 failure:
	tv_clip_vector_destroy (&safe_vec);
	tv_clip_vector_destroy (&new_vec);

	return NULL;
}

/**
 * info: Device we are controlling
 * window: The area you want to overlay, with coordinates relative to
 *   the current overlay buffer.
 * clip_vector: Invisible regions of the window (where it is obscured by
 *   other windows etc) with coordinates relative to window->x, y. No clips
 *   are required for regions outside the overlay buffer. clip_vector can
 *   be NULL.
 *
 * Sets the overlay window dimensions and clips.
 *
 * Returns NULL on error, a pointer to the actual overlay window on
 * success. It may differ from the requested dimensions due to hardware
 * limitations and other reasons. Should be verified before enabling
 * overlay.
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
	REQUIRE_SUPPORT (info->overlay.get_window, FALSE);

	TVLOCK;
	
	tv_clear_error (info);

	if (!info->overlay.get_window (info))
		RETURN_UNTVLOCK (FALSE);

	if (chromakey)
		*chromakey = info->overlay.chromakey;

	RETURN_UNTVLOCK (TRUE);
}

/**
 * info: Device we are controlling
 * window: The area you want to overlay, with coordinates relative to
 *   the current overlay buffer.
 * chromakey: The device shall display video where overlay buffer pixels
 *   have this color (0xRRGGBB).
 *
 * Sets the overlay window dimensions and chromakey.
 *
 * Returns NULL on error, a pointer to the actual overlay window on
 * success. It may differ from the requested dimensions due to hardware
 * limitations and other reasons. Should be verified before enabling
 * overlay.
 */
const tv_window *
tv_set_overlay_window_chromakey	(tveng_device_info *	info,
				 const tv_window *	window,
				 unsigned int		chromakey)
{
	tv_window win;
	tv_clip_vector safe_vec;

	assert (NULL != info);

	if (TVENG_CONTROLLER_NONE == info->current_controller)
		return NULL;

	REQUIRE_IO_MODE (NULL);
	REQUIRE_SUPPORT (info->overlay.set_window, NULL);

	TVLOCK;

	info->using_xvideo = FALSE;

	tv_clear_error (info);

	win = *window;

	init_overlay_window (info, &win);

	if (!overlay_window_visible (info, &win)) {
		if (info->overlay.active)
			if (!p_tv_enable_overlay (info, FALSE))
				goto failure;

		info->overlay.window = win;

		RETURN_UNTVLOCK (&info->overlay.window); /* nothing to do */
	}

	/* Calculate boundary clips in case the driver DMAs instead
	   of chromakeys the image. */

	tv_clip_vector_init (&safe_vec);

	if (!add_boundary_clips (info, &safe_vec, &win))
		goto failure2;

	if (0) {
		fprintf (stderr, "win.x=%d y=%d width=%u height=%u\n",
			 win.x, win.y,
			 win.width, win.height);
	}

	verify_clip_vector (&safe_vec, &win);

	if (0)
		_tv_clip_vector_dump (&safe_vec, stderr);

	if (info->overlay.active)
		if (!p_tv_enable_overlay (info, FALSE))
			goto failure2;

	if (!info->overlay.set_window (info, &win, &safe_vec, chromakey))
		goto failure2;

	tv_clip_vector_destroy (&safe_vec);

	RETURN_UNTVLOCK (&info->overlay.window);

 failure2:
	tv_clip_vector_destroy (&safe_vec);

 failure:
	RETURN_UNTVLOCK (NULL);
}

/**
 * info: Device we are controlling (XVideo only).
 * window: Place the video in this window.
 * gc: Use this graphics context.
 * chromakey: The device, if it works that way, shall display video where
 *   window pixels have this color (0xRRGGBB).
 *
 * Selects the window for XVideo PutVideo(). The video dimension will be
 * the same as the window dimensions.
 *
 * Returns FALSE on error. XXX hardware limitations? zoom?
 */
tv_bool
tv_set_overlay_xwindow		(tveng_device_info *	info,
				 Window			window,
				 GC			gc,
				 unsigned int		chromakey)
{
	tv_bool success;

	assert (NULL != info);
	assert (0 != window);
	assert (0 != gc);

	REQUIRE_IO_MODE (FALSE);
	REQUIRE_SUPPORT (info->overlay.set_xwindow, FALSE);

	TVLOCK;

	tv_clear_error (info);

	success = info->overlay.set_xwindow (info, window, gc, chromakey);

	info->using_xvideo = success;

	RETURN_UNTVLOCK (success);
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

	if (!info->using_xvideo
	    && !overlay_window_visible (info, &info->overlay.window)) {
		info->overlay.active = enable;
		return TRUE;
	}

	if (enable && !info->using_xvideo) {
		tv_screen *xs;

		if (!info->overlay.get_buffer) {
			support_failure (info, __PRETTY_FUNCTION__);
			return FALSE;
		}

		/* Safety: current target must match some screen. */

		if (!info->overlay.get_buffer (info))
			return FALSE;

		for (xs = screens; xs; xs = xs->next)
			if (verify_overlay_buffer (&xs->target,
						   &info->overlay.buffer))
				break;

		if (!xs) {
			fprintf (stderr, "** %s: Cannot start overlay, "
				 "DMA target is not properly initialized.\n",
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
				 tv_##kind *		p)		\
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
				 tv_video_standard *	p)
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
				 "set collision between %s (0x%" PRIx64 ") "
				 "and %s (0x%" PRIx64 ")\n",
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

void
store_cur_audio_input		(tveng_device_info *	info,
				 tv_audio_line *	p)
{
	assert (NULL != info);

	STORE_CURRENT (audio_input, audio_line, p);
}

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
