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
#include "config.h"

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
#include "tveng2.h" /* V4L2 specific headers */
#include "tveng25.h" /* V4L2 2.5 specific headers */
#include "tvengxv.h" /* XVideo specific headers */
#include "tvengemu.h" /* Emulation device */
#include "tvengbktr.h" /* Emulation device */
#include "tveng_private.h" /* private definitions */

#include "globals.h" /* XXX for vidmodes, dga_param */

#include "zmisc.h"
#include "../common/device.h"

/* XXX recursive callbacks not good, think again. */
#define TVLOCK								\
({									\
  if (0 == info->priv->callback_recursion) {				\
    /* fprintf (stderr, "TVLOCK   %d in %s\n", rc++, __FUNCTION__); */	\
    pthread_mutex_lock(&(info->priv->mutex));				\
  }									\
})
#define UNTVLOCK							\
({									\
  if (0 == info->priv->callback_recursion) {				\
    /* fprintf (stderr, "UNTVLOCK %d in %s\n", --rc, __FUNCTION__); */	\
    pthread_mutex_unlock(&(info->priv->mutex));				\
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
 t_error_msg("module", \
	     "function not supported by the module", info); \
} while (0)

/* for xv */
struct control {
  tv_control		pub;
  tveng_device_info *	info;
  Atom			atom;
  tv_audio_line *	mixer_line;
  tv_callback *		mixer_line_cb;
};

#define C(l) PARENT (l, struct control, pub)

typedef void (*tveng_controller)(struct tveng_module_info *info);
static tveng_controller tveng_controllers[] = {
  tvengxv_init_module,
  tveng25_init_module,
  tveng2_init_module,
  tveng1_init_module,
  tvengbktr_init_module,
  tvengemu_init_module
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




/* Initializes a tveng_device_info object */
tveng_device_info * tveng_device_info_new(Display * display, int bpp)
{
  size_t needed_mem = 0;
  tveng_device_info * new_object;
  struct tveng_module_info module_info;
  pthread_mutexattr_t attr;
  int i;

  /* Get the needed mem for the controllers */
  for (i = 0; i < N_ELEMENTS (tveng_controllers); ++i)
    {
      tveng_controllers[i](&module_info);
      needed_mem = MAX(needed_mem, (size_t) module_info.private_size);
    }

  t_assert(needed_mem > 0);

  if (!(new_object = calloc (1, needed_mem)))
    return NULL;

  if (!(new_object->priv = calloc (1, sizeof (*new_object->priv))))
    {
      free (new_object);
      return NULL;
    }

  if (!(new_object->error = malloc (256)))
    {
      free (new_object->priv);
      free (new_object);
      perror ("malloc");
      return NULL;
    }

  new_object->priv->display = display;
  new_object->priv->bpp = bpp;

  new_object->priv->zapping_setup_fb_verbosity = 0; /* No output by
							  default */
#ifdef USE_XV
  new_object->priv->port = None;
  new_object->priv->filter = None;
  new_object->priv->colorkey = None;
  new_object->priv->double_buffer = None;
#endif

  x11_vidmode_clear_state (&new_object->priv->old_mode);

  pthread_mutexattr_init(&attr);
  //  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&(new_object->priv->mutex), &attr);
  pthread_mutexattr_destroy(&attr);

  new_object->current_controller = TVENG_CONTROLLER_NONE;

  /* return the allocated memory */
  return (new_object);
}

/* destroys a tveng_device_info object (and closes it if neccesary) */
void tveng_device_info_destroy(tveng_device_info * info)
{
  t_assert(info != NULL);

  if (info -> fd > 0)
    p_tveng_close_device(info);

  if (info -> error)
    free(info -> error);

  pthread_mutex_destroy(&(info->priv->mutex));

  tv_callback_destroy (info, &info->priv->video_input_callback);
  tv_callback_destroy (info, &info->priv->audio_input_callback);
  tv_callback_destroy (info, &info->priv->video_standard_callback);

  free(info->priv);
  free(info);
}


/*
  Associates the given tveng_device_info with the given video
  device. On error it returns -1 and sets info->tveng_errno, info->error to
  the correct values.
  device_file: The file used to access the video device (usually
  /dev/video)
  attach_mode: Specifies the mode to open the device file
  depth: The color depth the capture will be in, -1 means let tveng
  decide based on the current private->display depth.
  info: The structure to be associated with the device
*/
int tveng_attach_device(const char* device_file,
			enum tveng_attach_mode attach_mode,
			tveng_device_info * info)
{
  int i;
  char *long_str, *short_str, *sign = NULL;
  tv_control *tc;
  int num_controls;
  tv_video_line *tl;
  int num_inputs;

  t_assert(device_file != NULL);
  t_assert(info != NULL);

  TVLOCK;

  if (info -> fd) /* If the device is already attached, detach it */
    p_tveng_close_device(info);

  info -> current_controller = TVENG_CONTROLLER_NONE;

  /*
    Check that the current private->display depth is one of the
    supported ones.

    XXX this is inaccurate. It depends on the
    formats supported by the video HW and our conversion
    capabilities.
  */
  info->priv->current_bpp = p_tveng_get_display_depth(info);

  switch (info->priv->current_bpp)
    {
    case 15:
    case 16:
    case 24:
    case 32:
      break;
    default:
      info -> tveng_errno = -1;
      t_error_msg("switch()",
		  "The current display depth isn't supported by TVeng",
		  info);
      UNTVLOCK;
      return -1;
    }

  for (i=0; i<(sizeof(tveng_controllers)/sizeof(tveng_controller));
       i++)
    {
      info -> fd = 0;
      tveng_controllers[i](&(info->priv->module));
      if (!info->priv->module.attach_device)
	continue;
      if (-1 != info->priv->module.attach_device(device_file, attach_mode,
						    info))
	goto success;
    }

  /* Error */
  info->tveng_errno = -1;
  t_error_msg("check()",
	      "The device cannot be attached to any controller",
	      info);
  CLEAR (info->priv->module);
  UNTVLOCK;
  return -1;

 success:
  {
    info->priv->control_mute = NULL;
    info->audio_mutable = 0;

    /* Add mixer controls */
    /* XXX the mixer_line should be property of a virtual
       device, but until we're there... */
    if (mixer && mixer_line)
      tveng_attach_mixer_line (info, mixer, mixer_line);

    num_controls = 0;

    for_all (tc, info->controls) {
      if (tc->id == TV_CONTROL_ID_MUTE
	  && (!tc->_ignore)
	  && info->priv->control_mute == NULL) {
	//	if (tv_control_callback_add (tc, NULL, (void *) deref_callback,
	//			     &info->priv->control_mute))
	  {
	    info->priv->control_mute = tc;
	    info->audio_mutable = 1; /* preliminary */
	  }
      }

      num_controls++;
    }

    num_inputs = 0;

    for_all (tl, info->video_inputs)
      num_inputs++;
  }

  if (asprintf (&sign, "%s - %d %d - %d %d %d", info->caps.name,
		num_inputs, num_controls,
		info->caps.flags, info->caps.maxwidth,
		info->caps.maxheight) == -1)
    {
      t_error ("asprintf", info);
      p_tveng_close_device (info);
      CLEAR (info->priv->module);
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

      if (info->priv->module.describe_controller)
	info->priv->module.describe_controller (&short_str, &long_str, info);

      fprintf(stderr, "Device: %s [%s - %s]\n", info->file_name,
	      short_str, long_str);
      fprintf(stderr, "Device signature: %x\n", info->signature);
      fprintf(stderr, "Detected framebuffer depth: %d\n",
	      p_tveng_get_display_depth(info));
      fprintf (stderr, "Capture format:\n"
	       "  buffer size            %ux%u pixels, 0x%x bytes\n"
	       "  bytes per line         %u, %u bytes\n"
	       "  offset		 %u, %u, %u bytes\n"
	       "  pixfmt                 %s\n",
	       info->format.width, info->format.height,
	       info->format.size,
	       info->format.bytes_per_line,
	       info->format.uv_bytes_per_line,
	       info->format.offset, info->format.u_offset,
	       info->format.v_offset,
	       tv_pixfmt_name (info->format.pixfmt));
      fprintf(stderr, "Current overlay window struct:\n");
      fprintf(stderr, "  Coords: %dx%d-%dx%d\n",
	      info->overlay_window.x,
	      info->overlay_window.y,
	      info->overlay_window.width,
	      info->overlay_window.height);

      {
	tv_video_standard *s;
	unsigned int i;

	fprintf (stderr, "Video standards:\n");

	for (s = info->video_standards, i = 0; s; s = s->_next, ++i)
	  fprintf (stderr, "  %d) '%s'  0x%llx %dx%d  %.2f  hash: %x\n",
		   i, s->label, s->videostd_set,
		   s->frame_width, s->frame_height,
		   s->frame_rate, s->hash);
      }

      {
	tv_video_line *l;
	unsigned int i;

	fprintf (stderr, "Video inputs:\n");

	for (l = info->video_inputs, i = 0; l; l = l->_next, ++i)
	  {
	    fprintf (stderr, "  %d) '%s'  id: %u  hash: %x  %s\n",
		     i, l->label, l->id, l->hash,
		     IS_TUNER_LINE (l) ? "tuner" : "composite");
	    if (IS_TUNER_LINE (l))
	      fprintf (stderr, "       freq.range: %u...%u %+d Hz\n",
		       l->u.tuner.minimum, l->u.tuner.maximum,
		       l->u.tuner.step);
	  }
      }

      {
	tv_control *c;
	unsigned int i;

	fprintf (stderr, "Controls:\n");

	for (c = info->controls, i = 0; c; c = c->_next, ++i)
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
  return info->fd;
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
tveng_describe_controller(char ** short_str, char ** long_str,
			  tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.describe_controller)
    info->priv->module.describe_controller(short_str, long_str,
					      info);
  else /* function not supported by the module */
    {
      if (short_str)
	*short_str = "UNKNOWN";
      if (long_str)
	*long_str = "No description provided";
    }

  UNTVLOCK;
}

/* Closes a device opened with tveng_init_device */
void p_tveng_close_device(tveng_device_info * info)
{
  if (info->current_controller == TVENG_CONTROLLER_NONE)
    return; /* nothing to be done */

  p_tveng_stop_everything(info);

  /* remove mixer controls */
  tveng_attach_mixer_line (info, NULL, NULL);

  if (info->priv->module.close_device)
    info->priv->module.close_device(info);

  info->priv->control_mute = NULL;
}

/* Closes a device opened with tveng_init_device */
void tveng_close_device(tveng_device_info * info)
{
  t_assert(info != NULL);

  if (info->current_controller == TVENG_CONTROLLER_NONE)
    return; /* nothing to be done */

  TVLOCK;

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

  t_assert(string != NULL);

  result = strdup(string);

  t_assert(result != NULL);

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

  t_assert(in1 != NULL);
  t_assert(in2 != NULL);

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
  int i;
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
  if (TVENG_ATTACH_CONTROL == info->attach_mode) {			\
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





#define NEXT_NODE_FUNC(item, kind)					\
const tv_##kind *							\
tv_next_##item			(const tveng_device_info *info,		\
				 const tv_##kind *	p)		\
{									\
	if (p)								\
		return p->_next;					\
	if (info)							\
		return info->item##s;					\
	return NULL;							\
}

#define NTH_NODE_FUNC(item, kind)					\
const tv_##kind *							\
tv_nth_##item			(tveng_device_info *	info,		\
				 unsigned int		index)		\
{									\
	tv_##kind *p;							\
									\
	t_assert (info != NULL);					\
									\
	TVLOCK;								\
									\
	for_all (p, info->item##s)					\
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
	t_assert (info != NULL);					\
									\
	index = 0;							\
									\
	TVLOCK;								\
									\
	for_all (list, info->item##s)					\
		if (p != list)						\
			++index;					\
		else							\
			break;						\
									\
	RETURN_UNTVLOCK (index);					\
}

#define NODE_BY_HASH_FUNC(item, kind)					\
const tv_##kind *							\
tv_##item##_by_hash		(tveng_device_info *	info,		\
				 unsigned int		hash)		\
{									\
	tv_##kind *p;							\
									\
	t_assert (info != NULL);					\
									\
	TVLOCK;								\
									\
	for_all (p, info->item##s)					\
		if (p->hash == hash)					\
			break;						\
									\
	RETURN_UNTVLOCK (p);						\
}

NEXT_NODE_FUNC			(video_input, video_line);
NTH_NODE_FUNC			(video_input, video_line);
NODE_POSITION_FUNC		(video_input, video_line);
NODE_BY_HASH_FUNC		(video_input, video_line);

tv_bool
tv_get_tuner_frequency		(tveng_device_info *	info,
				 unsigned int *		frequency)
{
	tv_video_line *line;

	t_assert (info != NULL);
	t_assert (frequency != NULL);

	TVLOCK;

	line = info->cur_video_input;

	if (!IS_TUNER_LINE (line))
		RETURN_UNTVLOCK (FALSE);

	if (info->priv->module.update_tuner_frequency) {
		if (!info->priv->module.update_tuner_frequency (info, line))
			RETURN_UNTVLOCK (FALSE);

		t_assert (line->u.tuner.frequency >= line->u.tuner.minimum);
		t_assert (line->u.tuner.frequency <= line->u.tuner.maximum);
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

	t_assert (info != NULL);

	REQUIRE_SUPPORT (info->priv->module.set_tuner_frequency, FALSE);

	TVLOCK;

	line = info->cur_video_input;

	if (!IS_TUNER_LINE (line))
		RETURN_UNTVLOCK (FALSE);

	frequency = SATURATE (frequency,
			      line->u.tuner.minimum,
			      line->u.tuner.maximum);

	RETURN_UNTVLOCK (info->priv->module.set_tuner_frequency
			 (info, line, frequency));
}

const tv_video_line *
tv_get_video_input		(tveng_device_info *	info)
{
	const tv_video_line *tl;

	t_assert (info != NULL);

	TVLOCK;

	if (!info->priv->module.update_video_input)
		tl = info->cur_video_input;
	else if (info->priv->module.update_video_input (info))
		tl = info->cur_video_input;
	else
		tl = NULL;

	RETURN_UNTVLOCK (tl);
}

tv_bool
tv_set_video_input		(tveng_device_info *	info,
				 const tv_video_line *	line)
{
	tv_video_line *list;

	t_assert (info != NULL);
	t_assert (line != NULL);

	REQUIRE_SUPPORT (info->priv->module.set_video_input, FALSE);

	TVLOCK;

	for_all (list, info->video_inputs)
		if (list == line)
			break;

	if (list == NULL) {
		UNTVLOCK;
		return FALSE;
	}

	RETURN_UNTVLOCK (info->priv->module.set_video_input (info, line));
}

/*
  Sets the input named name as the active input. -1 on error.
*/
int
tveng_set_input_by_name(const char * input_name,
			tveng_device_info * info)
{
  tv_video_line *tl;

  t_assert(input_name != NULL);
  t_assert(info != NULL);

  TVLOCK;

  for_all (tl, info->video_inputs)
    if (tveng_normstrcmp(tl->label, input_name))
      {
//XXX
	UNTVLOCK;
	return tv_set_video_input (info, tl);
      }

  info->tveng_errno = -1;
  t_error_msg("finding",
	      "Input %s doesn't appear to exist", info, input_name);

  UNTVLOCK;
  return -1; /* String not found */
}

/*
 *  Audio inputs
 */

NEXT_NODE_FUNC			(audio_input, audio_line);
NTH_NODE_FUNC			(audio_input, audio_line);
NODE_POSITION_FUNC		(audio_input, audio_line);
NODE_BY_HASH_FUNC		(audio_input, audio_line);

const tv_audio_line *
tv_get_audio_input		(tveng_device_info *	info)
{
	return NULL; /* to do */
}

tv_bool
tv_set_audio_input		(tveng_device_info *	info,
				 const tv_audio_line *	line)
{
	return FALSE; /* to do */
}

/*
 *  Video standards
 */

NEXT_NODE_FUNC			(video_standard, video_standard);
NTH_NODE_FUNC			(video_standard, video_standard);
NODE_POSITION_FUNC		(video_standard, video_standard);
NODE_BY_HASH_FUNC		(video_standard, video_standard);

const tv_video_standard *
tv_get_video_standard		(tveng_device_info *	info)
{
	const tv_video_standard *ts;

	t_assert (info != NULL);

	TVLOCK;

	if (!info->priv->module.update_standard)
		ts = info->cur_video_standard;
	else if (info->priv->module.update_standard (info))
		ts = info->cur_video_standard;
	else
		ts = NULL;

	RETURN_UNTVLOCK (ts);
}

tv_bool
tv_set_video_standard		(tveng_device_info *	info,
				 const tv_video_standard *standard)
{
	tv_video_standard *list;

	t_assert (info != NULL);
	t_assert (standard != NULL);

	REQUIRE_SUPPORT (info->priv->module.set_standard, FALSE);

	TVLOCK;

	for_all (list, info->video_standards)
		if (list == standard)
			break;

	if (list == NULL) {
		UNTVLOCK;
		return FALSE;
	}

	RETURN_UNTVLOCK (info->priv->module.set_standard (info, standard));
}

tv_bool
tv_set_video_standard_by_id	(tveng_device_info *	info,
				 tv_videostd_set	videostd_set)
{
	tv_video_standard *ts, *list;

	t_assert (info != NULL);

	REQUIRE_SUPPORT (info->priv->module.set_standard, FALSE);

	TVLOCK;

	ts = NULL;

	for_all (list, info->video_standards)
		if (ts->videostd_set & videostd_set) {
			if (ts) {
				info->tveng_errno = -1;
				t_error_msg("finding",
					    "Standard by ambiguous id %llx", info,
					    videostd_set);
				RETURN_UNTVLOCK (FALSE);
			}

			ts = list;
		}

	RETURN_UNTVLOCK (info->priv->module.set_standard (info, ts));
}

/*
  Sets the standard by name. -1 on error
*/
int
tveng_set_standard_by_name(const char * name, tveng_device_info * info)
{
  tv_video_standard *ts;

  t_assert(info != NULL);

  TVLOCK;

  for_all (ts, info->video_standards)
    if (tveng_normstrcmp(name, ts->label))
      {
	if (info->priv->module.set_standard)
	  RETURN_UNTVLOCK(info->priv->module.set_standard(info, ts));

	TVUNSUPPORTED;
	UNTVLOCK;
	return -1;
      }

  info->tveng_errno = -1;
  t_error_msg("finding",
	      "Standard %s doesn't appear to exist", info, name);

  UNTVLOCK;
  return -1; /* String not found */  
}

/*
 *  Controls
 */

tv_control *
tv_next_control			(const tveng_device_info *info,
				 const tv_control *	control)
{
	tv_control *next_control;

	if (!control) {
		if (!info || !(next_control = info->controls))
			return NULL;

		if (!next_control->_ignore)
			return next_control;
	}

	next_control = control->_next;

	while (next_control) {
		if (!next_control->_ignore)
			return next_control;

		next_control = next_control->_next;
	}

	return NULL;
}

tv_control *
tv_nth_control			(tveng_device_info *	info,
				 unsigned int		index)
{
	tv_control *control;

	t_assert (info != NULL);

	TVLOCK;

	for (control = tv_next_control (info, NULL);
	     control; control = tv_next_control (info, control))
		if (index-- == 0)
			break;

	RETURN_UNTVLOCK (control);
}

unsigned int
tv_control_position		(tveng_device_info *	info,
				 const tv_control *	control)
{
	tv_control *list;
	unsigned int index;

	t_assert (info != NULL);

	index = 0;

	TVLOCK;

	for (list = tv_next_control (info, NULL);
	     list; list = tv_next_control (info, list))
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
	tv_control *control;

	t_assert (info != NULL);

	TVLOCK;

	for (control = tv_next_control (info, NULL);
	     control; control = tv_next_control (info, control))
		if (control->hash == hash)
			break;

	RETURN_UNTVLOCK (control);
}

tv_control *
tv_control_by_id		(tveng_device_info *	info,
				 tv_control_id		id)
{
	tv_control *control;

	t_assert (info != NULL);

	TVLOCK;

	for (control = tv_next_control (info, NULL);
	     control; control = tv_next_control (info, control))
		if (control->id == id)
			break;

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
	case TV_PIXFMT_RGBA24_LE:
	case TV_PIXFMT_RGBA24_BE:
	case TV_PIXFMT_BGRA24_LE:
	case TV_PIXFMT_BGRA24_BE:
		x = (((*x1 << 2) + 2) & -4) >> 2;
		w = (((*width << 2) + 2) & -4) >> 2;
		break;

	case TV_PIXFMT_RGB24_LE:
	case TV_PIXFMT_BGR24_LE:
		/* Round to multiple of 12. */
		x = (*x1 + 2) & -4;
		w = (*width + 2) & -4;
		break;

	case TV_PIXFMT_RGB16_LE:
	case TV_PIXFMT_RGB16_BE:
	case TV_PIXFMT_BGR16_LE:
	case TV_PIXFMT_BGR16_BE:
	case TV_PIXFMT_RGBA15_LE:
	case TV_PIXFMT_RGBA15_BE:
	case TV_PIXFMT_BGRA15_LE:
	case TV_PIXFMT_BGRA15_BE:
	case TV_PIXFMT_ARGB15_LE:
	case TV_PIXFMT_ARGB15_BE:
	case TV_PIXFMT_ABGR15_LE:
	case TV_PIXFMT_ABGR15_BE:
	case TV_PIXFMT_RGBA12_LE:
	case TV_PIXFMT_RGBA12_BE:
	case TV_PIXFMT_BGRA12_LE:
	case TV_PIXFMT_BGRA12_BE:
	case TV_PIXFMT_ARGB12_LE:
	case TV_PIXFMT_ARGB12_BE:
	case TV_PIXFMT_ABGR12_LE:
	case TV_PIXFMT_ABGR12_BE:
		x = (((*x1 << 1) + 2) & -4) >> 1;
		w = (((*width << 1) + 2) & -4) >> 1;
		break;

	case TV_PIXFMT_RGB8:
	case TV_PIXFMT_BGR8:
	case TV_PIXFMT_RGBA7:
	case TV_PIXFMT_BGRA7:
	case TV_PIXFMT_ARGB7:
	case TV_PIXFMT_ABGR7:
		x = (*x1 + 2) & -4;
		w = (*width + 2) & -4;
		break;

	default:
		t_assert_not_reached ();
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

/* Updates the current capture format info. -1 if failed */
int
tveng_update_capture_format(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  REQUIRE_IO_MODE (-1);

  TVLOCK;

  if (info->priv->module.update_capture_format)
    RETURN_UNTVLOCK(info->priv->module.update_capture_format(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

int
p_tveng_set_capture_format(tveng_device_info * info)
{
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  /* FIXME */
  if (0 == (TV_PIXFMT_SET (info->format.pixfmt) &
	    (TV_PIXFMT_SET (TV_PIXFMT_YUV420) |
	     TV_PIXFMT_SET (TV_PIXFMT_YVU420) |
	     TV_PIXFMT_SET (TV_PIXFMT_YUYV) |
	     TV_PIXFMT_SET (TV_PIXFMT_UYVY))))
    {
      return -1;
    }

  REQUIRE_IO_MODE (-1);

  info->format.height = SATURATE (info->format.height,
				  info->caps.minheight, info->caps.maxheight);
  info->format.width  = SATURATE (info->format.width,
				  info->caps.minwidth, info->caps.maxwidth);

  if (info->priv->dword_align)
    round_boundary_4 (NULL, &info->format.width,
		      info->format.pixfmt, info->caps.maxwidth);

  if (info->priv->module.set_capture_format)
    return (info->priv->module.set_capture_format(info));

  TVUNSUPPORTED;

  return -1;
}


/* -1 if failed. Sets the pixformat and fills in info -> pix_format
   with the correct values

   XXX this is called from scan_devices to determine supported
   formats, which is overkill. Maybe we should have a dedicated
   function to query formats a la try_fmt.

   FIXME request_capture_format_real needs a rewrite, see TODO.
   In the meantime we will allow only YUV formats a la Zapping
   0.0 - 0.6.
  */
int
tveng_set_capture_format(tveng_device_info * info)
{
  t_assert(info != NULL);

  TVLOCK;
  RETURN_UNTVLOCK (p_tveng_set_capture_format (info));
}

/*
 *  Controls
 */

static int
update_xv_control (tveng_device_info * info, struct control *c)
{
  int value;

  if (c->pub.id == TV_CONTROL_ID_VOLUME
      || c->pub.id == TV_CONTROL_ID_MUTE)
    {
      tv_audio_line *line;

      if (info->priv->quiet)
	return 0;

      line = c->mixer_line;
      t_assert (line != NULL);

      if (!tv_mixer_line_update (line))
	return -1;

      if (c->pub.id == TV_CONTROL_ID_VOLUME)
	value = (line->volume[0] + line->volume[1] + 1) >> 1;
      else
	value = !!(line->muted);
    }

#ifdef USE_XV
  else if ((c->atom == info->priv->filter) &&
	   (info->priv->port != None))
    {
      XvGetPortAttribute(info->priv->display,
			 info->priv->port,
			 info->priv->filter,
			 &value);
    }
  else if ((c->atom == info->priv->double_buffer) &&
	   (info->priv->port != None))
    {
      XvGetPortAttribute(info->priv->display,
			 info->priv->port,
			 info->priv->double_buffer,
			 &value);
    }
  else if ((c->atom == info->priv->colorkey) &&
	   (info->priv->port != None))
    {
      int r,g,b, val;
      int rm=0xff, gm=0xff, bm=0xff, rs=16, gs=8, bs=0; /* masks, shifts */

      XvGetPortAttribute(info->priv->display,
			 info->priv->port,
			 info->priv->colorkey,
			 &(val));

      /* Adjust colorkey to the current pixformat */
      switch (info->priv->current_bpp)
	{
	case 15:
	  rm = gm = bm = 0xf8;
	  rs = 7; gs = 2; bs = -3;
	  break;
	case 16:
	  rm = bm = 0xf8; gm = 0xfc;
	  rs = 8; gs = 3; bs = -3;
	  break;
	default:
	  break;
	}
      if (rs > 0)
	r = val >> rs;
      else
	r = val << -rs;
      r &= rm;
      if (gs > 0)
	g = val >> gs;
      else
	g = val << -gs;
      g &= gm;
      if (bs > 0)
	b = val >> bs;
      else
	b = val << -bs;
      b &= bm;
      value = (r<<16)+(g<<8)+b;
    }
#endif
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
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);
  t_assert(control != NULL);

  TVLOCK;

  if (control->_parent == NULL /* TVENG_CONTROLLER_MOTHER */)
    RETURN_UNTVLOCK (update_xv_control (info, C(control)));

  if (!info->priv->module.update_control)
    RETURN_UNTVLOCK(0);

  RETURN_UNTVLOCK(info->priv->module.update_control (info, control) ? 0 : -1);
}

int
p_tveng_update_controls(tveng_device_info * info)
{
  tv_control *tc;

  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  /* Update the controls we maintain */
  for (tc = info->controls; tc; tc = tc->_next)
    if (tc->_parent == NULL /* TVENG_CONTROLLER_MOTHER */)
      update_xv_control (info, C(tc));

  if (!info->priv->module.update_control)
    return 0;

  return (info->priv->module.update_control (info, NULL) ? 0 : -1);
}

int
tveng_update_controls(tveng_device_info * info)
{
  t_assert(info != NULL);

  TVLOCK;

  RETURN_UNTVLOCK (p_tveng_update_controls (info));
}

/*
 *  When setting the mixer volume we must ensure the
 *  video device is unmuted and volume at maximum.
 */
static void
reset_video_volume		(tveng_device_info *	info,
				 guint			mute)
{
  tv_control *tc;

  for (tc = info->controls; tc; tc = tc->_next)
    {
      if (tc->_parent != NULL && tc->_ignore)
	switch (tc->id)
	  {
	  case TV_CONTROL_ID_VOLUME:
	    if (info->priv->module.set_control)
	      /* Error ignored */
	      info->priv->module.set_control (info, tc, tc->maximum);
	    break;

	  case TV_CONTROL_ID_MUTE:
	    if (info->priv->module.set_control)
	      /* Error ignored */
	      info->priv->module.set_control (info, tc, mute);
	    break;

	  default:
	    break;
	  }
    }
} 

static int
set_control_audio		(tveng_device_info *	info,
				 struct control *	c,
				 int			value,
				 tv_bool		quiet,
				 tv_bool		reset)
{
  tv_bool success;

  if (NULL == c->pub._parent)
    {
      tv_callback *cb;

      t_assert (NULL != c->mixer_line);

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
  else if (info->priv->module.set_control)
    {
      if (quiet)
	{
	  tv_callback *cb;
	  int old_value;

	  cb = c->pub._callback;
	  old_value = c->pub.value;

	  c->pub._callback = NULL;

	  success = info->priv->module.set_control (info, &c->pub, value);

	  c->pub.value = old_value;
	  c->pub._callback = cb;
	}
      else
	{
	  success = info->priv->module.set_control (info, &c->pub, value);
	}
    }

  return success ? 0 : -1;
}

static int
set_control			(struct control * c, int value,
				 tv_bool reset,
				 tveng_device_info * info)
{
  int r = 0;

  value = SATURATE (value, c->pub.minimum, c->pub.maximum);

  /* If the quiet switch is set we remember the
     requested value but don't change driver state. */
  if (info->priv->quiet)
    if (c->pub.id == TV_CONTROL_ID_VOLUME
	|| c->pub.id == TV_CONTROL_ID_MUTE)
      goto set_and_notify;

  if (c->pub._parent == NULL /* TVENG_CONTROLLER_MOTHER */)
    {
      if (c->pub.id == TV_CONTROL_ID_VOLUME
	  || c->pub.id == TV_CONTROL_ID_MUTE)
	{
	  return set_control_audio (info, c, value, /* quiet */ FALSE, reset);
	}
#ifdef USE_XV
      else if ((c->atom == info->priv->filter) &&
	  (info->priv->port != None))
	{
	  XvSetPortAttribute(info->priv->display,
			     info->priv->port,
			     info->priv->filter,
			     value);
	}
      else if ((c->atom == info->priv->double_buffer) &&
	       (info->priv->port != None))
	{
	  XvSetPortAttribute(info->priv->display,
			     info->priv->port,
			     info->priv->double_buffer,
			     value);
	}
      else if ((c->atom == info->priv->colorkey) &&
	       (info->priv->port != None))
	{
	  int r, g, b;
	  int rm=0xff, gm=0xff, bm=0xff, rs=16, gs=8, bs=0; /* masks, shifts */
	  /* Adjust colorkey to the current pixformat */
	  switch (info->priv->current_bpp)
	    {
	    case 15:
	      rm = gm = bm = 0xf8;
	      rs = 7; gs = 2; bs = -3;
	      break;
	    case 16:
	      rm = bm = 0xf8; gm = 0xfc;
	      rs = 8; gs = 3; bs = -3;
	      break;
	    default:
	      break;
	    }
	  r = (value>>16)&rm;
	  if (rs > 0)
	    r <<= rs;
	  else
	    r >>= -rs;
	  g = (value>>8)&gm;
	  if (gs > 0)
	    g <<= gs;
	  else
	    g >>= -gs;
	  b = value&bm;
	  if (bs > 0)
	    b <<= bs;
	  else
	    b >>= -bs;
	  value = r+g+b;
	  XvSetPortAttribute(info->priv->display,
			     info->priv->port,
			     info->priv->colorkey,
			     value);
	}
      else
#endif
	{
	  t_assert (0);
	  return -1;
	}

    set_and_notify:
      if (c->pub.value != value)
	{
	  c->pub.value = value;
	  tv_callback_notify (info, &c->pub, c->pub._callback);
	}

      return r;
    }

  info = (tveng_device_info *) c->pub._parent; /* XXX */

  if (info->priv->module.set_control)
    return info->priv->module.set_control (info, &c->pub, value) ? 0 : -1;

  TVUNSUPPORTED;

  return -1;
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
  t_assert(info != NULL);
  t_assert(control != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;
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
  int value;
  tv_control *tc;

  t_assert(info != NULL);
  t_assert(info->controls != NULL);
  t_assert(control_name != NULL);

  TVLOCK;

  /* Update the controls (their values) */
  if (info->priv->quiet
      && (tc->id == TV_CONTROL_ID_VOLUME
	  || tc->id == TV_CONTROL_ID_MUTE))
    {
      /* not now */
    }
  else
    {
      if (tveng_update_controls(info) == -1)
	RETURN_UNTVLOCK(-1);
    }

  /* iterate through the info struct to find the control */
  for (tc = info->controls; tc; tc = tc->_next)
    if (!strcasecmp(tc->label,control_name))
      /* we found it */
      {
	value = tc->value;
	if (cur_value)
	  *cur_value = value;
	UNTVLOCK;
	return 0; /* Success */
      }

  /* if we reach this, we haven't found the control */
  info->tveng_errno = -1;
  t_error_msg("finding",
	      "Cannot find control \"%s\" in the list of controls",
	      info, control_name);
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

  t_assert(info != NULL);
  t_assert(info->controls != NULL);

  TVLOCK;

  for (tc = info->controls; tc; tc = tc->_next)
    if (!tc->_ignore && 0 == strcasecmp(tc->label,control_name))
      /* we found it */
      RETURN_UNTVLOCK (set_control (C(tc), new_value, TRUE, info));

  /* if we reach this, we haven't found the control */
  info->tveng_errno = -1;
  t_error_msg("finding",
	   "Cannot find control \"%s\" in the list of controls",
	   info, control_name);
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
  struct control *c = user_data;
  tv_control *tc;
  int value;

  t_assert (line == c->mixer_line);

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

  if (!c->info->priv->quiet)
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
 *  XXX c->info->priv->quiet?
 */
static void
mixer_line_destroy_cb		(tv_audio_line *	line,
				 void *			user_data)
{
  struct control *c = user_data;
  tv_control *tc, **tcp;
  tv_control_id id;

  t_assert (line == c->mixer_line);

  id = c->pub.id;

  if (id == TV_CONTROL_ID_MUTE)
    {
      c->info->priv->control_mute = NULL;
      c->info->audio_mutable = 0; /* preliminary */
    }

  for (tcp = &c->info->controls; (tc = *tcp);)
    {
      if (tc == &c->pub)
	{
	  tv_callback_destroy (tc, &tc->_callback);
	  *tcp = tc->_next;
	  free (tc);
	  continue;
	}
      else if (tc->id == id)
	{
	  tc->_ignore = FALSE;

	  if (id == TV_CONTROL_ID_MUTE)
	    {
	      c->info->priv->control_mute = tc;
	      c->info->audio_mutable = 1; /* preliminary */
	    }
	}

      tcp = &tc->_next;
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
  tv_control *tc, **tcp, *orig;
  tv_bool done = FALSE;
  struct control *c;

  orig = NULL;

  for (tcp = &info->controls; (tc = *tcp);)
    {
      if (tc->id == id)
	{
	  if (tc->_parent == NULL)
	    {
	      struct control *c = PARENT (tc, struct control, pub);

	      if (line)
		{
		  /* Replace mixer line */

		  tv_callback_remove (c->mixer_line_cb);

		  c->mixer_line = line;

		  c->mixer_line_cb = tv_mixer_line_add_callback
		    (line, mixer_line_notify_cb,
		     mixer_line_destroy_cb, c);
		  t_assert (c->mixer_line_cb != NULL); /* XXX */

		  mixer_line_notify_cb (line, c);

		  done = TRUE;
		}
	      else
		{
		  /* Remove mixer line & control */

		  tv_callback_destroy (tc, &tc->_callback);
		  tv_callback_remove (c->mixer_line_cb);

		  *tcp = tc->_next;
		  free (tc);

		  done = TRUE;
		  continue;
		}
	    }
	  else /* video device control */
	    {
	      if (!TVENG_MIXER_VOLUME_DEBUG)
		tc->_ignore = (line != NULL);

	      orig = tc;
	    }
	}

      tcp = &tc->_next;
    }

  if (done || !line)
    return;

  /* Add mixer control */

  c = calloc (1, sizeof (*c));
  t_assert (c != NULL); /* XXX */

  c->pub.id = id;

  if (id == TV_CONTROL_ID_VOLUME)
    {
      c->pub.type = TV_CONTROL_TYPE_INTEGER;
      if (TVENG_MIXER_VOLUME_DEBUG)
	c->pub.label = "Mixer volume";
      else
	c->pub.label = _("Volume");
    }
  else
    {
      c->pub.type = TV_CONTROL_TYPE_BOOLEAN;
      if (TVENG_MIXER_VOLUME_DEBUG)
	c->pub.label = "Mixer mute";
      else
	c->pub.label = _("Mute");
    }

  c->info = info;
  c->mixer_line = line;

  c->mixer_line_cb = tv_mixer_line_add_callback
    (line, mixer_line_notify_cb,
     mixer_line_destroy_cb, c);
  t_assert (c->mixer_line_cb != NULL); /* XXX */

  mixer_line_notify_cb (line, c);

  if (orig)
    {
      c->pub._next = orig->_next;
      orig->_next = &c->pub;
    }
  else
    {
      *tcp = &c->pub;
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
 *  and sets tv_control->ignore on any video device volume and
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

  info->priv->control_mute = NULL;
  info->audio_mutable = 0;

  if (mixer && mixer_line)
    printv ("Attaching mixer %s (%s %s), line %s\n",
	    mixer->node.device,
	    mixer->node.label,
	    mixer->node.driver,
	    mixer_line->label);
  else
    printv ("Removing mixer\n");

  mixer_replace (info, line, TV_CONTROL_ID_VOLUME);
  mixer_replace (info, line, TV_CONTROL_ID_MUTE);

  for (tc = info->controls; tc; tc = tc->_next)
    if (tc->id == TV_CONTROL_ID_MUTE && !tc->_ignore)
      {
	info->priv->control_mute = tc;
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
  t_assert (info != NULL);

  TVLOCK;

  if (info->priv->quiet == quiet)
    {
      UNTVLOCK;
      return;
    }

  info->priv->quiet = quiet;

  if (quiet)
    {
      tv_control *tc;

      for (tc = info->controls; tc; tc = tc->_next)
	if (!tc->_ignore && TV_CONTROL_ID_MUTE == tc->id)
	  {
	    /* Error ignored */
	    set_control_audio (info, C (tc),
			       /* value */ TRUE,
			       /* quiet */ TRUE,
			       /* reset */ FALSE);
	    break;
	  }

      if (NULL == tc) /* no mute control found */
	for (tc = info->controls; tc; tc = tc->_next)
	  if (!tc->_ignore && TV_CONTROL_ID_VOLUME == tc->id)
	    {
	      /* Error ignored */
	      set_control_audio (info, C (tc),
				 /* value */ 0,
				 /* quiet */ TRUE,
				 /* reset */ FALSE);
	      break;
	    }
    }
  else
    {
      tv_control *tc;
      tv_bool reset;

      reset = FALSE;

      for (tc = info->controls; tc; tc = tc->_next)
	{
	  if (tc->_ignore)
	    continue;

	  switch (tc->id)
	    {
	    case TV_CONTROL_ID_MUTE:
	    case TV_CONTROL_ID_VOLUME:
	      set_control_audio (info, C (tc),
				 tc->value,
				 /* quiet */ FALSE,
				 /* reset */ FALSE);

	      if (NULL == tc->_parent) /* uses soundcard mixer */
		reset = TRUE;

	      break;

	    default:
	      break;
	    }
	}

      if (reset)
	reset_video_volume (info, /* mute */ FALSE);
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

  t_assert (info != NULL);
  t_assert (info->current_controller != TVENG_CONTROLLER_NONE);

  // XXX TVLOCK;

  if ((tc = info->priv->control_mute))
    {
      if (!info->priv->quiet)
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

  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if ((tc = info->priv->control_mute))
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

  t_assert (info != NULL);

  if (!(tc = info->priv->control_mute))
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
  return FALSE; // XXX todo
}

tv_bool
tv_audio_update			(tveng_device_info *	info)
{
  return FALSE; // XXX todo
}

/* capability or reception changes */
tv_callback *
tv_add_audio_callback		(tveng_device_info *	info,
				 void			(* notify)(tveng_device_info *, void *),
				 void			(* destroy)(tveng_device_info *, void *),
				 void *			user_data)
{
  t_assert (info != NULL);

  return tv_callback_add (&info->priv->audio_callback,
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
int
tveng_get_signal_strength (int *strength, int * afc,
			   tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->priv->module.get_signal_strength)
    RETURN_UNTVLOCK(info->priv->module.get_signal_strength(strength,
							    afc,
							    info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Sets up the capture device so any read() call after this one
  succeeds. Returns -1 on error.
*/
int
tveng_start_capturing(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  REQUIRE_IO_MODE (-1);

  TVLOCK;

  if (info->priv->module.start_capturing)
    RETURN_UNTVLOCK(info->priv->module.start_capturing(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* Tries to stop capturing. -1 on error. */
int
tveng_stop_capturing(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  REQUIRE_IO_MODE (-1);

  TVLOCK;

  if (info->priv->module.stop_capturing)
    RETURN_UNTVLOCK(info->priv->module.stop_capturing(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* 
   Reads a frame from the video device, storing the read data in
   the location pointed to by dest. dest can be NULL to discard.
   time: time to wait using select() in miliseconds
   info: pointer to the video device info structure
   Returns -1 on error, anything else on success
*/
int tveng_read_frame(tveng_image_data * dest, 
		     unsigned int time, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  REQUIRE_IO_MODE (-1);

  TVLOCK;

  if (info->priv->module.read_frame)
    RETURN_UNTVLOCK(info->priv->module.read_frame(dest, time,
						   info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Gets the timestamp of the last read frame in seconds.
*/
double tveng_get_timestamp(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  REQUIRE_IO_MODE (-1);

  TVLOCK;

  if (info->priv->module.get_timestamp)
    RETURN_UNTVLOCK(info->priv->module.get_timestamp(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* 
   Sets the capture buffer to an specific size. returns -1 on
   error. Remember to check the value of width and height since it can
   be different to the one requested. 
*/
int tveng_set_capture_size(int width, int height, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  REQUIRE_IO_MODE (-1);

  TVLOCK;

  if (info->priv->dword_align)
    round_boundary_4 (NULL, &info->format.width,
		      info->format.pixfmt, info->caps.maxwidth);

  height = SATURATE (height, info->caps.minheight, info->caps.maxheight);
  width  = SATURATE (width, info->caps.minwidth, info->caps.maxwidth);

  if (info->priv->module.set_capture_size)
    RETURN_UNTVLOCK(info->priv->module.set_capture_size(width,
							 height,
							 info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* 
   Gets the actual size of the capture buffer in width and height.
   -1 on error
*/
int tveng_get_capture_size(int *width, int *height, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(width != NULL);
  t_assert(height != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  REQUIRE_IO_MODE (-1);

  TVLOCK;

  if (info->priv->module.get_capture_size)
    RETURN_UNTVLOCK(info->priv->module.get_capture_size(width,
							 height,
							 info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
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
	    || dga->format.bytes_per_line < dga->format.width
	    || dga->format.size < (dga->format.bytes_per_line * dga->format.height))
		return FALSE;

	if (0 == dma->base
	    || dma->format.width < 32 || dma->format.height < 32
	    || dma->format.bytes_per_line < dma->format.width
	    || dma->format.size < (dma->format.bytes_per_line * dma->format.height))
		return FALSE;

	dga_end = dga->base + dga->format.size;
	dma_end = dma->base + dma->format.size;

	if (dma_end < dga->base
	    || dma->base >= dga_end)
		return FALSE;

	if (TV_PIXFMT_NONE == dga->format.pixfmt)
		return FALSE;

	if (dga->format.bytes_per_line != dma->format.bytes_per_line
	    || dga->format.pixfmt != dma->format.pixfmt)
		return FALSE;

	/* Adjust? */
	if (dga->base != dma->base
	    || dga->format.width != dma->format.width
	    || dga->format.height != dma->format.height)
		return FALSE;

	return TRUE;
}

tv_bool
tv_get_overlay_buffer		(tveng_device_info *	info,
				 tv_overlay_buffer *	target)
{
	t_assert (info != NULL);
	t_assert (info->current_controller != TVENG_CONTROLLER_NONE);
	t_assert (target != NULL);

	CLEAR (*target); /* unusable */

	REQUIRE_IO_MODE (FALSE);
	REQUIRE_SUPPORT (info->priv->module.get_overlay_buffer, FALSE);

	TVLOCK;

	RETURN_UNTVLOCK (info->priv->module.get_overlay_buffer (info, target));
}

tv_bool
tv_set_overlay_buffer		(tveng_device_info *	info,
				 tv_overlay_buffer *	target)
{
	tv_overlay_buffer dma;
	char *argv[20];
	char buf[16];
	pid_t pid;
	int status;
	int r;

	t_assert (info != NULL);
	t_assert (info->current_controller != TVENG_CONTROLLER_NONE);
	t_assert (target != NULL);

	REQUIRE_IO_MODE (FALSE);
	REQUIRE_SUPPORT (info->priv->module.get_overlay_buffer, FALSE);

	TVLOCK;

	/* We can save a lot work if the target is already
	   initialized. */

	if (info->priv->module.get_overlay_buffer (info, &dma)) {
		if (validate_overlay_buffer (target, &dma))
			goto success;

		/* We can save a lot work if the driver supports
		   set_overlay directly. TODO: Test if we actually have
		   access permission. */

		if (info->priv->module.set_overlay_buffer)
			RETURN_UNTVLOCK (info->priv->module.set_overlay_buffer
					 (info, target));
	}

	/* Delegate to suid root zapping_setup_fb helper program. */

	{
		unsigned int argc;

		argc = 0;

		argv[argc++] = "zapping_setup_fb";
		argv[argc++] = "--device";
		argv[argc++] = info->file_name;

		{
			int i = MIN (info->priv->zapping_setup_fb_verbosity, 2);

			while (i-- > 0)
				argv[argc++] = "--verbose";
		}

		if (info->priv->bpp != -1) {
			snprintf (buf, sizeof (buf), "%d", info->priv->bpp);
			argv[argc++] = "--bpp";
			argv[argc++] = buf;
		}

		argv[argc] = NULL;
	}

	{
		/* FIXME need a safer solution. */
		/* Could temporarily switch to control attach_mode. */
		t_assert (info->file_name != NULL);
		p_tveng_stop_everything (info);
		device_close (info->log_fp, info->fd);
		info->fd = 0;
	}

	switch ((pid = fork ())) {
	case -1: /* error */
		info->tveng_errno = errno;
		t_error("fork()", info);
		goto failure;

	case 0: /* in child */
		/* First try the zapping_setup_fb install path. */
		r = execvp (PACKAGE_ZSFB_DIR "/zapping_setup_fb", argv);

		if (-1 == r && ENOENT == errno) {
			/* Try in $PATH. */
			r = execvp ("zapping_setup_fb", argv);

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
		t_assert (-1 != info->fd);
	}

	if (-1 == r) {
		info->tveng_errno = errno;
		t_error ("waitpid", info);
		goto failure;
	}

	if (!WIFEXITED (status)) {
		info->tveng_errno = errno;
		tv_error_msg (info, _("zapping_setup_fb exited abnormally."));
		goto failure;
	}

	switch (WEXITSTATUS (status)) {
	case 0: /* ok */
		if (!info->priv->module.get_overlay_buffer (info, &dma))
			goto failure;

		if (!validate_overlay_buffer (target, &dma))
			goto failure;

		break;

	case 1: /* zapping_setup_fb failure */
		info->tveng_errno = -1; /* unknown */
		tv_error_msg (info, _("zapping_setup_fb failed."));
		goto failure;

	case 2: /* zapping_setup_fb ENOENT */
		info->tveng_errno = ENOENT;
		tv_error_msg (info, _("zapping_setup_fb not found in "
			PACKAGE_ZSFB_DIR " or executable search path."));
		goto failure;

	default:
		info->tveng_errno = -1; /* unknown */
		tv_error_msg (info, _("Unknown error in zapping_setup_fb."));
	failure:
		RETURN_UNTVLOCK (FALSE);
	}

 success:
	RETURN_UNTVLOCK (TRUE);
}

tv_bool
tv_set_overlay_xwindow		(tveng_device_info *	info,
				 Window			window,
				 GC			gc)
{
	t_assert (info != NULL);
	t_assert (window != 0);
	t_assert (gc != 0);

	REQUIRE_IO_MODE (FALSE);
	REQUIRE_SUPPORT (info->priv->module.set_overlay_xwindow, FALSE);

	TVLOCK;

	RETURN_UNTVLOCK (info->priv->module.set_overlay_xwindow
			 (info, window, gc));
}

int
p_tveng_get_display_depth(tveng_device_info * info)
{
  /* This routines are taken form xawtv, i don't understand them very
     well, but they seem to work OK */
  XVisualInfo * visual_info, template;
  XPixmapFormatValues * pf;
  Display * dpy = info->priv->display;
  int found, v, i, n;
  int bpp = 0;

  if (info->priv->bpp != -1)
    return info->priv->bpp;

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
    t_error_msg("XGetVisualInfo",
		"Cannot find an appropiate visual", info);
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
    t_error_msg("XListPixmapFormats",
		"Cannot figure out X depth", info);
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
   PRIV->BPP (bits per pixel). This one is quite important for 24 and 32 bit
   modes, since the default X visual may be 24 bit and the real screen
   depth 32, thus an expensive RGB -> RGBA conversion must be
   performed for each frame.
   priv->display: the priv->display we want to know its real depth (can be
   accessed through gdk_private->display)
*/
int
tveng_get_display_depth(tveng_device_info * info)
{
  int bpp;

  TVLOCK;

  bpp = p_tveng_get_display_depth (info);

  UNTVLOCK;
  return bpp;
}

int
p_tveng_set_preview_window(tveng_device_info * info)
{
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  REQUIRE_IO_MODE (-1);

  if (info->priv->dword_align)
    round_boundary_4 (&info->overlay_window.x,
		      &info->overlay_window.width,
		      info->overlay_buffer.format.pixfmt,
		      info->caps.maxwidth);

  if (info->overlay_window.height < info->caps.minheight)
    info->overlay_window.height = info->caps.minheight;
  if (info->overlay_window.height > info->caps.maxheight)
    info->overlay_window.height = info->caps.maxheight;
  if (info->overlay_window.width < info->caps.minwidth)
    info->overlay_window.width = info->caps.minwidth;
  if (info->overlay_window.width > info->caps.maxwidth)
    info->overlay_window.width = info->caps.maxwidth;

  if (info->priv->module.set_preview_window)
    return (info->priv->module.set_preview_window(info));

  TVUNSUPPORTED;

  return -1;
}

/*
  Sets the preview window dimensions to the given window.
  Returns -1 on error, something else on success.
  Success doesn't mean that the requested dimensions are used, maybe
  they are different, check the returned fields to see if they are suitable
  info   : Device we are controlling
  The current chromakey value is used, the caller doesn't need to fill
  it in.
*/
int
tveng_set_preview_window(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  REQUIRE_IO_MODE (-1);

  TVLOCK;
  RETURN_UNTVLOCK (p_tveng_set_preview_window (info));
}

/*
  Gets the current overlay window parameters.
  Returns -1 on error, and any other value on success.
  info   : The device to use
*/
int
tveng_get_preview_window(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  REQUIRE_IO_MODE (-1);

  TVLOCK;

  if (info->priv->module.get_preview_window)
    RETURN_UNTVLOCK(info->priv->module.get_preview_window(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

int
p_tveng_set_preview (int on, tveng_device_info * info)
{
	tv_overlay_buffer dma;

	t_assert (info != NULL);
	t_assert (info->current_controller != TVENG_CONTROLLER_NONE);

	REQUIRE_IO_MODE (-1);
	REQUIRE_SUPPORT (info->priv->module.set_overlay, -1);

	on = !!on;

	if (on && info->current_controller != TVENG_CONTROLLER_XV) {
		if (!info->priv->module.get_overlay_buffer) {
			support_failure (info, __PRETTY_FUNCTION__);
			return (-1);
		}

		if (!info->priv->module.get_overlay_buffer (info, &dma))
			return (-1);

		if (!validate_overlay_buffer (&dga_param, &dma)) {
			fprintf (stderr, "** %s: Cannot start overlay, "
				 "DMA target is not properly initialized.",
				 __PRETTY_FUNCTION__);
			return (-1);
		}

		// XXX should also check if a previous
		// tveng_set_preview_window () failed.
	}

	if (!info->priv->module.set_overlay (info, on))
	  return (-1);

	info->overlay_active = on;

	return (0);
}

/* 
   Sets the previewing on/off.
   on : if 1, set preview on, if 0 off, other values are silently ignored
   info  : device to use for previewing
   Returns -1 on error, anything else on success
*/
int
tveng_set_preview (int on, tveng_device_info * info)
{
	t_assert (info != NULL);

	TVLOCK;
	RETURN_UNTVLOCK (p_tveng_set_preview (on, info));
}

/* Adjusts the verbosity value passed to zapping_setup_fb, cannot fail
 */
void
tveng_set_zapping_setup_fb_verbosity(int level, tveng_device_info *
				     info)
{
  t_assert(info != NULL);

  if (level > 2)
    level = 2;
  else if (level < 0)
    level = 0;
  info->priv->zapping_setup_fb_verbosity = level;
}

/* Sets the chromakey */
void tveng_set_chromakey(uint32_t chroma, tveng_device_info *info)
{
  t_assert (info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  if (TVENG_ATTACH_CONTROL == info->attach_mode) {
    panel_failure (info, __PRETTY_FUNCTION__);
    return;
  }

  TVLOCK;

  if (info->priv->module.set_chromakey)
    info->priv->module.set_chromakey (chroma, info);

  UNTVLOCK;
  return;
}

int tveng_get_chromakey (uint32_t *chroma, tveng_device_info *info)
{
  t_assert (info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  REQUIRE_IO_MODE (-1);

  TVLOCK;

  if (info->priv->module.get_chromakey)
    RETURN_UNTVLOCK (info->priv->module.get_chromakey (chroma, info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* Sets the dword align flag */
void tveng_set_dword_align(int dword_align, tveng_device_info *info)
{
  info->priv->dword_align = dword_align;
}

/* Returns the current verbosity value passed to zapping_setup_fb */
int
tveng_get_zapping_setup_fb_verbosity(tveng_device_info * info)
{
  return (info->priv->zapping_setup_fb_verbosity);
}

// FIXME static
// This is used because mode can be _PREVIEW or _WINDOW without
// overlay_active due to delay timer. Don't reactivate prematurely.
static tv_bool overlay_was_active = FALSE;

enum tveng_capture_mode 
p_tveng_stop_everything (tveng_device_info *       info)
{
  enum tveng_capture_mode returned_mode;

  returned_mode = info->current_mode;
  switch (info->current_mode)
    {
    case TVENG_CAPTURE_READ:
      overlay_was_active = FALSE;
      if (info->priv->module.stop_capturing)
	(info->priv->module.stop_capturing(info));
      t_assert(info->current_mode == TVENG_NO_CAPTURE);
      break;

    case TVENG_CAPTURE_PREVIEW:
#if 0
      /* Eeek. The client must stop overlay, switch, check if the
         image geometry changed (new video standard), handle this
         appropriately and restart. Not our business to switch
	 the vidmode back and forth, and changing the overlay window.
	 That belongs at a higher layer, e.g. the zvideo widget. */
      overlay_was_active = info->overlay_active;
      p_tveng_stop_previewing(info);
      t_assert(info->current_mode == TVENG_NO_CAPTURE);
      break;
#else
      /* fall through */
#endif
    case TVENG_CAPTURE_WINDOW:
      overlay_was_active = info->overlay_active;
  /* No error checking */
      if (info->overlay_active)
	p_tveng_set_preview (FALSE, info);
      info -> current_mode = TVENG_NO_CAPTURE;
      break;

    case TVENG_TELETEXT:
      /* nothing */
      break;

    default:
      t_assert(info->current_mode == TVENG_NO_CAPTURE);
      break;
    };

  return returned_mode;
}

/*
  tveng INTERNAL function, stops the capture or the previewing. Returns the
  mode the device was before stopping.
  For stopping and restarting the device do:
  enum tveng_capture_mode cur_mode;
  cur_mode = tveng_stop_everything(info);
  ... do some stuff ...
  if (tveng_restart_everything(cur_mode, info) == -1)
     ... show error dialog ...
*/
enum tveng_capture_mode 
tveng_stop_everything (tveng_device_info *       info)
{
  enum tveng_capture_mode returned_mode;

  t_assert(info != NULL);

  TVLOCK;

  returned_mode = p_tveng_stop_everything (info);

  UNTVLOCK;

  return returned_mode;
}

int p_tveng_restart_everything (enum tveng_capture_mode mode,
			      tveng_device_info * info)
{
  switch (mode)
    {
    case TVENG_CAPTURE_READ:
      // XXX REQUIRE_IO_MODE (-1);
      if (info->priv->module.start_capturing)
	if (-1 == info->priv->module.start_capturing(info))
	  return -1;
      break;

    case TVENG_CAPTURE_PREVIEW:
#if 0 /* See above. */
      if (p_tveng_start_previewing(info, (const char *) -1) == -1)
	return (-1);
      break;
#else
      /* fall through */
#endif

    case TVENG_CAPTURE_WINDOW:
      if (info->current_mode != mode)
	{
	  p_tveng_stop_everything(info);

	  if (overlay_was_active)
	    {
	      p_tveng_set_preview_window(info);

	      if (p_tveng_set_preview (TRUE, info) == -1)
		return (-1);
	    }

	  info->current_mode = mode;
	}
      break;

    case TVENG_TELETEXT:
      info->current_mode = mode;
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
int tveng_restart_everything (enum tveng_capture_mode mode,
			      tveng_device_info * info)
{
  t_assert(info != NULL);

  TVLOCK;
  RETURN_UNTVLOCK (p_tveng_restart_everything (mode, info));
}

int tveng_get_debug_level(tveng_device_info * info)
{
  t_assert(info != NULL);

  return (info->debug_level);
}

void tveng_set_debug_level(int level, tveng_device_info * info)
{
  t_assert(info != NULL);

  info->debug_level = level;
}

void tveng_set_xv_support(int disabled, tveng_device_info * info)
{
  t_assert(info != NULL);

  info->priv->disable_xv_video = disabled;
}

#ifdef USE_XV

void tveng_set_xv_port(XvPortID port, tveng_device_info * info)
{
  XvAttribute *at;
  int attributes, i;
  Display *dpy;

  /* ? REQUIRE_IO_MODE (-1); */

  TVLOCK;

  info->priv->port = port;
  dpy = info->priv->display;
  info->priv->filter = info->priv->colorkey =
    info->priv->double_buffer = None;

  /* Add the controls in this port to the struct of controls */
  at = XvQueryPortAttributes(dpy, port, &attributes);

  for (i=0; i<attributes; i++)
    {
      struct control c;

      if (info->debug_level)
	fprintf(stderr, "  TVeng.c Xv atom: %s%s%s (%i -> %i)\n",
		at[i].name,
		(at[i].flags & XvGettable) ? " gettable" : "",
		(at[i].flags & XvSettable) ? " settable" : "",
		at[i].min_value, at[i].max_value);

      /* Any attribute not settable and Gettable is of little value */
      if ((!(at[i].flags & XvGettable)) ||
	  (!(at[i].flags & XvSettable)))
	continue;

      CLEAR (c);

      if (!strcmp("XV_FILTER", at[i].name))
	  {
	    info->priv->filter = XInternAtom(dpy, "XV_FILTER",
						False);
	    c.atom = info->priv->filter;
	    if (!(c.pub.label = strdup (_("Filter"))))
	      goto failure;
	    c.pub.minimum = at[i].min_value;
	    c.pub.maximum = at[i].max_value;
	    c.pub.type = TV_CONTROL_TYPE_BOOLEAN;
	    c.pub.menu = NULL;
	    c.pub._parent = NULL; /* TVENG_CONTROLLER_MOTHER; */
	    if (!append_control(info, &c.pub, sizeof (c)))
	      {
	      failure:
		XFree (at);
		UNTVLOCK;
		return;
	      }
	  }

      else if (!strcmp("XV_DOUBLE_BUFFER", at[i].name))
	  {
	    info->priv->double_buffer = XInternAtom(dpy, "XV_DOUBLE_BUFFER",
						       False);
	    c.atom = info->priv->double_buffer;
	    if (!(c.pub.label = strdup (_("Filter"))))
	      goto failure2;
	    c.pub.minimum = at[i].min_value;
	    c.pub.maximum = at[i].max_value;
	    c.pub.type = TV_CONTROL_TYPE_BOOLEAN;
	    c.pub.menu = NULL;
	    c.pub._parent = NULL; /* TVENG_CONTROLLER_MOTHER; */
	    if (!append_control(info, &c.pub, sizeof (c)))
	      {
	      failure2:
		XFree (at);
		UNTVLOCK;
		return;
	      }
	  }

      else if (!strcmp("XV_COLORKEY", at[i].name))
	  {
	    info->priv->colorkey = XInternAtom(dpy, "XV_COLORKEY",
						False);
	    c.atom = info->priv->colorkey;
	    if (!(c.pub.label = strdup (_("Colorkey"))))
	      goto failure3;
	    c.pub.minimum = at[i].min_value;
	    c.pub.maximum = at[i].max_value;
	    c.pub.type = TV_CONTROL_TYPE_COLOR;
	    c.pub.menu = NULL;
	    c.pub._parent = NULL; /* TVENG_CONTROLLER_MOTHER; */
	    if (!append_control(info, &c.pub, sizeof (c)))
	      {
	      failure3:
		XFree (at);
		UNTVLOCK;
		return;
	      }
	  }
    }

  XFree (at);

  p_tveng_update_controls(info);

  UNTVLOCK;
}

void tveng_unset_xv_port(tveng_device_info * info)
{
  info->priv->port = None;
  info->priv->filter = info->priv->colorkey = None;
}

#endif /* USE_XV */

int
tveng_ov511_get_button_state (tveng_device_info *info)
{
	if (!info) return -1;
//  t_assert(info != NULL);

  if (info->current_controller == TVENG_CONTROLLER_NONE)
    return -1;

  TVLOCK;

  if (info->priv->module.ov511_get_button_state)
    RETURN_UNTVLOCK(info->priv->module.ov511_get_button_state(info));

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

#ifndef HAVE_STRNDUP

char *
tv_strndup			(const char *		s,
				 size_t			size)
{
	char *d;
	size_t len;

	if (NULL == s || size <= 0)
		return NULL;

	len = strlen (s);
	len = MIN (len, size);

	if (!(d = malloc (len + 1)))
		return NULL;

	memcpy (d, s, len);
	d[len] = 0;

	return d;
}

#endif /* HAVE_STRNDUP */

/* XXX use asprintf when available */
char *
tv_strdup_printf (const char *templ, ...)
{
  char *s, buf[512];
  va_list ap;
  int temp;

  temp = errno;

  va_start(ap, templ);
  vsnprintf(buf, sizeof(buf) - 1, templ, ap);
  va_end(ap);

  s = strdup(buf);

  errno = temp;

  return s;
}

/*
 *  Simple callback mechanism 
 */

struct _tv_callback {
	tv_callback *		next;
	tv_callback **		prev_next;
	tv_callback_fn *	notify;
	tv_callback_fn *	destroy;
	void *			user_data;
	unsigned int		blocked;
};

/*
 *  Add a function to a callback list. The notify function is called
 *  on some event, the destroy function (if not NULL) before the list
 *  is deleted.
 *
 *  Not intended to be called directly by tveng clients.
 */
tv_callback *
tv_callback_add			(tv_callback **		list,
				 tv_callback_fn *	notify,
				 tv_callback_fn *	destroy,
				 void *			user_data)
{
	tv_callback *cb;

	t_assert (list != NULL);
	t_assert (notify != NULL || destroy != NULL);

	for (; (cb = *list); list = &cb->next)
		;

	if (!(cb = calloc (1, sizeof (*cb))))
		return NULL;

	*list = cb;

	cb->prev_next = list; 

	cb->notify = notify;
	cb->destroy = destroy;
	cb->user_data = user_data;

	return cb;
}

/*
 *  Remove a callback function from its list. Argument is the result
 *  of tv_callback_add().
 *
 *  Intended for tveng clients. Note the destroy handler is not called.
 */
void
tv_callback_remove		(tv_callback *		cb)
{
	if (cb) {
		if (cb->next)
			cb->next->prev_next = cb->prev_next;
		*(cb->prev_next) = cb->next;
		free (cb);
	}
}

/*
 *  Delete an entire callback list, usually before the object owning
 *  the list is destroyed.
 *  XXX we probably need ref counting.
 */
void
tv_callback_destroy		(void *			object,
				 tv_callback **		list)
{
	tv_callback *cb;

	t_assert (object != NULL);
	t_assert (list != NULL);

	while ((cb = *list)) {
		*list = cb->next;

		if (cb->next)
			cb->next->prev_next = list;

		if (cb->destroy)
			cb->destroy (object, cb->user_data);

		free (cb);
	}
}

/*
 *  Temporarily block calls to the notify handler.
 *  XXX user tv_callback * is fast but inconvenient because
 *  the user must prepare a destroy handler. Ideas? 
 */
void
tv_callback_block		(tv_callback *		cb)
{
  	if (cb)
		cb->blocked++;
}

/*
 *  Counterpart of tv_callback_block().
 */
void
tv_callback_unblock		(tv_callback *		cb)
{
	if (cb && cb->blocked > 0)
		cb->blocked--;
}

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

	t_assert (object != NULL);

	if (info)
		++info->priv->callback_recursion;

	for (cb = list; cb; cb = cb->next)
		if (cb->notify && cb->blocked == 0)
			cb->notify (object, cb->user_data);

	if (info)
		--info->priv->callback_recursion;
}

/*
 *  Device nodes
 */

tv_device_node *
tv_device_node_add		(tv_device_node **	list,
				 tv_device_node *	node)
{
	t_assert (list != NULL);

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
				 tv_bool		restore)
{
	if (!node)
		return;

	free (node->label);
	free (node->bus);
	free (node->driver);
	free (node->version);
	free (node->device);
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

	t_assert (filter != NULL);

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
	assert (info != NULL);						\
									\
	return tv_callback_add (&info->priv->item##_callback,		\
				(tv_callback_fn *) notify,		\
				(tv_callback_fn *) destroy,		\
				user_data);				\
}

ADD_NODE_CALLBACK_FUNC (video_input);
ADD_NODE_CALLBACK_FUNC (audio_input);
ADD_NODE_CALLBACK_FUNC (video_standard);

const char *
tv_pixfmt_name			(tv_pixfmt		pixfmt)
{
	switch (pixfmt) {
	case TV_PIXFMT_NONE:		return "NONE";
	case TV_PIXFMT_YUV444:		return "YUV444";
	case TV_PIXFMT_YVU444:		return "YVU444";
	case TV_PIXFMT_YUV422:		return "YUV422";
	case TV_PIXFMT_YVU422:		return "YVU422";
	case TV_PIXFMT_YUV411:		return "YUV411";
	case TV_PIXFMT_YVU411:		return "YVU411";
	case TV_PIXFMT_YUV420:		return "YUV420";
	case TV_PIXFMT_YVU420:		return "YVU420";
	case TV_PIXFMT_YUV410:		return "YUV410";
	case TV_PIXFMT_YVU410:		return "YVU410";
	case TV_PIXFMT_YUVA24_LE:	return "YUVA24_LE";
	case TV_PIXFMT_YUVA24_BE:	return "YUVA24_BE";
	case TV_PIXFMT_YVUA24_LE:	return "YVUA24_LE";
	case TV_PIXFMT_YVUA24_BE:	return "YVUA24_BE";
     /* case TV_PIXFMT_AVUY24_BE:	return "AVUY24_BE"; synonyms */
     /* case TV_PIXFMT_AVUY24_LE:	return "AVUY24_LE"; */
     /* case TV_PIXFMT_AUVY24_BE:	return "AUVY24_BE"; */
     /* case TV_PIXFMT_AUVY24_LE:	return "AUVY24_LE"; */
	case TV_PIXFMT_YUV24_LE:	return "YUV24_LE";
	case TV_PIXFMT_YUV24_BE:	return "YUV24_BE";
	case TV_PIXFMT_YVU24_LE:	return "YVU24_LE";
	case TV_PIXFMT_YVU24_BE:	return "YVU24_BE";
     /* case TV_PIXFMT_VUY24_BE:	return "VUY24_BE"; */
     /* case TV_PIXFMT_VUY24_LE:	return "VUY24_LE"; */
     /* case TV_PIXFMT_UVY24_BE:	return "UVY24_BE"; */
     /* case TV_PIXFMT_UVY24_LE:	return "UVY24_LE"; */
	case TV_PIXFMT_YUYV:		return "YUYV";
	case TV_PIXFMT_YVYU:		return "YVYU";
	case TV_PIXFMT_UYVY:		return "UYVY";
	case TV_PIXFMT_VYUY:		return "VYUY";
	case TV_PIXFMT_Y8:		return "Y8";
	case TV_PIXFMT_RGBA24_LE:	return "RGBA24_LE";
	case TV_PIXFMT_RGBA24_BE:	return "RGBA24_BE";
	case TV_PIXFMT_BGRA24_LE:	return "BGRA24_LE";
	case TV_PIXFMT_BGRA24_BE:	return "BGRA24_BE";
     /* case TV_PIXFMT_ABGR24_BE:	return "ABGR24_BE"; synonyms */
     /* case TV_PIXFMT_ABGR24_LE:	return "ABGR24_LE"; */
     /* case TV_PIXFMT_ARGB24_BE:	return "ARGB24_BE"; */
     /* case TV_PIXFMT_ARGB24_LE:	return "ARGB24_LE"; */
	case TV_PIXFMT_RGB24_LE:	return "RGB24_LE";
	case TV_PIXFMT_BGR24_LE:	return "BGR24_LE";
     /* case TV_PIXFMT_BGR24_BE:	return "BGR24_BE"; */
     /* case TV_PIXFMT_RGB24_BE:	return "RGB24_BE"; */
	case TV_PIXFMT_RGB16_LE:	return "RGB16_LE";
	case TV_PIXFMT_RGB16_BE:	return "RGB16_BE";
	case TV_PIXFMT_BGR16_LE:	return "BGR16_LE";
	case TV_PIXFMT_BGR16_BE:	return "BGR16_BE";
	case TV_PIXFMT_RGBA15_LE:	return "RGBA15_LE";
	case TV_PIXFMT_RGBA15_BE:	return "RGBA15_BE";
	case TV_PIXFMT_BGRA15_LE:	return "BGRA15_LE";
	case TV_PIXFMT_BGRA15_BE:	return "BGRA15_BE";
	case TV_PIXFMT_ARGB15_LE:	return "ARGB15_LE";
	case TV_PIXFMT_ARGB15_BE:	return "ARGB15_BE";
	case TV_PIXFMT_ABGR15_LE:	return "ABGR15_LE";
	case TV_PIXFMT_ABGR15_BE:	return "ABGR15_BE";
	case TV_PIXFMT_RGBA12_LE:	return "RGBA12_LE";
	case TV_PIXFMT_RGBA12_BE:	return "RGBA12_BE";
	case TV_PIXFMT_BGRA12_LE:	return "BGRA12_LE";
	case TV_PIXFMT_BGRA12_BE:	return "BGRA12_BE";
	case TV_PIXFMT_ARGB12_LE:	return "ARGB12_LE";
	case TV_PIXFMT_ARGB12_BE:	return "ARGB12_BE";
	case TV_PIXFMT_ABGR12_LE:	return "ABGR12_LE";
	case TV_PIXFMT_ABGR12_BE:	return "ABGR12_BE";
	case TV_PIXFMT_RGB8:		return "RGB8";
	case TV_PIXFMT_BGR8:		return "BGR8";
	case TV_PIXFMT_RGBA7:		return "RGBA7";
	case TV_PIXFMT_BGRA7:		return "BGRA7";
	case TV_PIXFMT_ARGB7:		return "ARGB7";
	case TV_PIXFMT_ABGR7:		return "ABGR7";

	case TV_PIXFMT_RESERVED0:
	case TV_PIXFMT_RESERVED1:
	case TV_PIXFMT_RESERVED2:
	case TV_PIXFMT_RESERVED3:
		break;

		/* No default, gcc warns if we missed any. */
	}

	return NULL;
}

unsigned int
tv_pixfmt_bytes_per_pixel	(tv_pixfmt		pixfmt)
{
	switch (pixfmt) {
	case TV_PIXFMT_YUV444:
	case TV_PIXFMT_YVU444:
	case TV_PIXFMT_YUV422:
	case TV_PIXFMT_YVU422:
	case TV_PIXFMT_YUV411:
	case TV_PIXFMT_YVU411:
	case TV_PIXFMT_YUV420:
	case TV_PIXFMT_YVU420:
	case TV_PIXFMT_YUV410:
	case TV_PIXFMT_YVU410:
		return 1;

	case TV_PIXFMT_YUVA24_LE:
	case TV_PIXFMT_YUVA24_BE:
	case TV_PIXFMT_YVUA24_LE:
	case TV_PIXFMT_YVUA24_BE:
		return 4;

	case TV_PIXFMT_YUV24_LE:
	case TV_PIXFMT_YUV24_BE:
	case TV_PIXFMT_YVU24_LE:
	case TV_PIXFMT_YVU24_BE:
		return 3;

	case TV_PIXFMT_YUYV:
	case TV_PIXFMT_YVYU:
	case TV_PIXFMT_UYVY:
	case TV_PIXFMT_VYUY:
		return 2;

	case TV_PIXFMT_Y8:
		return 1;

	case TV_PIXFMT_RGBA24_LE:
	case TV_PIXFMT_RGBA24_BE:
	case TV_PIXFMT_BGRA24_LE:
	case TV_PIXFMT_BGRA24_BE:
		return 4;

	case TV_PIXFMT_RGB24_LE:
	case TV_PIXFMT_BGR24_LE:
		return 3;

	case TV_PIXFMT_RGB16_LE:
	case TV_PIXFMT_RGB16_BE:
	case TV_PIXFMT_BGR16_LE:
	case TV_PIXFMT_BGR16_BE:
	case TV_PIXFMT_RGBA15_LE:
	case TV_PIXFMT_RGBA15_BE:
	case TV_PIXFMT_BGRA15_LE:
	case TV_PIXFMT_BGRA15_BE:
	case TV_PIXFMT_ARGB15_LE:
	case TV_PIXFMT_ARGB15_BE:
	case TV_PIXFMT_ABGR15_LE:
	case TV_PIXFMT_ABGR15_BE:
	case TV_PIXFMT_RGBA12_LE:
	case TV_PIXFMT_RGBA12_BE:
	case TV_PIXFMT_BGRA12_LE:
	case TV_PIXFMT_BGRA12_BE:
	case TV_PIXFMT_ARGB12_LE:
	case TV_PIXFMT_ARGB12_BE:
	case TV_PIXFMT_ABGR12_LE:
	case TV_PIXFMT_ABGR12_BE:
		return 2;

	case TV_PIXFMT_RGB8:
	case TV_PIXFMT_BGR8:
	case TV_PIXFMT_RGBA7:
	case TV_PIXFMT_BGRA7:
	case TV_PIXFMT_ARGB7:
	case TV_PIXFMT_ABGR7:
		return 1;

	case TV_PIXFMT_NONE:
	case TV_PIXFMT_RESERVED0:
	case TV_PIXFMT_RESERVED1:
	case TV_PIXFMT_RESERVED2:
	case TV_PIXFMT_RESERVED3:
		break;

		/* No default, gcc warns if we missed any. */
	}

	return 0;
}

/* XXX check this, esp LE/BE */
tv_bool
tv_pixfmt_to_pixel_format	(tv_pixel_format *	format,
				 tv_pixfmt		pixfmt,
				 unsigned int		reserved)
{
	/* unused, VU, msb A, msb B,
	   unused, unused, BE on BE, BE on LE machine */
	static const uint8_t attr_table [] = {
		[TV_PIXFMT_YUV444]	= 0x00,
		[TV_PIXFMT_YVU444]	= 0x40,
		[TV_PIXFMT_YUV422]	= 0x00,
		[TV_PIXFMT_YVU422]	= 0x40,
		[TV_PIXFMT_YUV411]	= 0x00,
		[TV_PIXFMT_YVU411]	= 0x40,
		[TV_PIXFMT_YUV420]	= 0x00,
		[TV_PIXFMT_YVU420]	= 0x40,
		[TV_PIXFMT_YUV410]	= 0x00,
		[TV_PIXFMT_YVU410]	= 0x40,

		[TV_PIXFMT_YUVA24_LE]	= 0x02,
		[TV_PIXFMT_YUVA24_BE]	= 0x01,
		[TV_PIXFMT_YVUA24_LE]	= 0x42,
		[TV_PIXFMT_YVUA24_BE]	= 0x41,

		[TV_PIXFMT_YUV24_LE]	= 0x02,
		[TV_PIXFMT_YUV24_BE]	= 0x01,
		[TV_PIXFMT_YVU24_LE]	= 0x42,
		[TV_PIXFMT_YVU24_BE]	= 0x41,

		[TV_PIXFMT_YUYV]	= 0x00,
		[TV_PIXFMT_YVYU]	= 0x40,
		[TV_PIXFMT_UYVY]	= 0x00,
		[TV_PIXFMT_VYUY]	= 0x40,

		[TV_PIXFMT_Y8]		= 0x00,

		[TV_PIXFMT_RGBA24_LE]	= 0x02,
		[TV_PIXFMT_RGBA24_BE]	= 0x01,
		[TV_PIXFMT_BGRA24_LE]	= 0x12,
		[TV_PIXFMT_BGRA24_BE]	= 0x11,

		[TV_PIXFMT_RGB24_LE]	= 0x02,
		[TV_PIXFMT_RGB24_BE]	= 0x01,

		[TV_PIXFMT_RGB16_LE]	= 0x00,
		[TV_PIXFMT_RGB16_BE]	= 0x03,
		[TV_PIXFMT_BGR16_LE]	= 0x10,
		[TV_PIXFMT_BGR16_BE]	= 0x13,

		[TV_PIXFMT_RGBA15_LE]	= 0x00,
		[TV_PIXFMT_RGBA15_BE]	= 0x03,
		[TV_PIXFMT_BGRA15_LE]	= 0x10,
		[TV_PIXFMT_BGRA15_BE]	= 0x13,
		[TV_PIXFMT_ARGB15_LE]	= 0x20,
		[TV_PIXFMT_ARGB15_BE]	= 0x23,
		[TV_PIXFMT_ABGR15_LE]	= 0x30,
		[TV_PIXFMT_ABGR15_BE]	= 0x33,

		[TV_PIXFMT_RGBA12_LE]	= 0x00,
		[TV_PIXFMT_RGBA12_BE]	= 0x03,
		[TV_PIXFMT_BGRA12_LE]	= 0x10,
		[TV_PIXFMT_BGRA12_BE]	= 0x13,
		[TV_PIXFMT_ARGB12_LE]	= 0x20,
		[TV_PIXFMT_ARGB12_BE]	= 0x23,
		[TV_PIXFMT_ABGR12_LE]	= 0x30,
		[TV_PIXFMT_ABGR12_BE]	= 0x33,

		[TV_PIXFMT_RGB8]	= 0x00,
		[TV_PIXFMT_BGR8]	= 0x00,

		[TV_PIXFMT_RGBA7]	= 0x00,
		[TV_PIXFMT_BGRA7]	= 0x10,
		[TV_PIXFMT_ARGB7]	= 0x20,
		[TV_PIXFMT_ABGR7]	= 0x30,
	};
	unsigned int attr;

	t_assert (format != NULL);

	if ((unsigned int) pixfmt >= N_ELEMENTS (attr_table))
		return FALSE;

	attr = attr_table [pixfmt];

	format->pixfmt			= pixfmt;
	format->_reserved1		= 0;

	format->uv_hscale		= 1;
	format->uv_vscale		= 1;

#if BYTE_ORDER == BIG_ENDIAN
	format->big_endian		= !!(attr & 3);
#else
	format->big_endian		= attr & 1;
#endif

	format->planar			= FALSE;

	format->mask.rgb.a		= 0;

	switch (pixfmt) {
	case TV_PIXFMT_YUV444:
	case TV_PIXFMT_YVU444:
		format->color_depth = 24;
		goto yuv_planar;

	case TV_PIXFMT_YUV422:
	case TV_PIXFMT_YVU422:
		format->color_depth = 16;
		format->uv_hscale = 2;
		goto yuv_planar;

	case TV_PIXFMT_YUV411:
	case TV_PIXFMT_YVU411:
		format->color_depth = 12;
		format->uv_hscale = 4;
		goto yuv_planar;

	case TV_PIXFMT_YUV420:
	case TV_PIXFMT_YVU420:
		format->color_depth = 12;
		format->uv_hscale = 2;
		format->uv_vscale = 2;
		goto yuv_planar;

	case TV_PIXFMT_YUV410:
	case TV_PIXFMT_YVU410:
		format->color_depth = 9;
		format->uv_hscale = 4;
		format->uv_vscale = 4;

	yuv_planar:
		format->bits_per_pixel = 8; /* Y plane */
		format->planar = TRUE;
		format->mask.yuv.y = 0xFF;
		format->mask.yuv.u = 0xFF;
		format->mask.yuv.v = 0xFF;
		break;

	case TV_PIXFMT_YUVA24_LE: /* LE 0xAAVVUUYY BE 0xYYUUVVAA */
	case TV_PIXFMT_YUVA24_BE: /* LE 0xYYUUVVAA BE 0xAAVVUUYY */
	case TV_PIXFMT_YVUA24_LE: /* LE 0xAAUUVVYY BE 0xYYVVUUAA */
	case TV_PIXFMT_YVUA24_BE: /* LE 0xYYVVUUAA BE 0xAAUUVVYY */
		format->bits_per_pixel = 32;
		format->color_depth = 24;

		if (format->big_endian) {
			format->mask.yuv.y = 0xFF << 24;
			format->mask.yuv.u = 0xFF << 16;
			format->mask.yuv.v = 0xFF << 8;
			format->mask.yuv.a = 0xFF;
		} else {
			format->mask.yuv.y = 0xFF;
			format->mask.yuv.u = 0xFF << 8;
			format->mask.yuv.v = 0xFF << 16;
			format->mask.yuv.a = 0xFF << 24;
		}
		break;

	case TV_PIXFMT_YUV24_LE: /* LE 0xVVUUYY BE 0xYYUUVV */
	case TV_PIXFMT_YUV24_BE: /* LE 0xYYUUVV BE 0xVVUUYY */
	case TV_PIXFMT_YVU24_LE: /* LE 0xUUVVYY BE 0xYYVVUU */
	case TV_PIXFMT_YVU24_BE: /* LE 0xYYVVUU BE 0xUUVVYY */
		format->bits_per_pixel = 24;
		format->color_depth = 24;

		if (format->big_endian) {
			format->mask.yuv.y = 0xFF << 16;
			format->mask.yuv.v = 0xFF;
		} else {
			format->mask.yuv.y = 0xFF;
			format->mask.yuv.v = 0xFF << 16;
		}

		format->mask.yuv.u = 0xFF << 8;

		break;

	case TV_PIXFMT_YUYV:
	case TV_PIXFMT_YVYU:
	case TV_PIXFMT_UYVY:
	case TV_PIXFMT_VYUY:
		format->bits_per_pixel = 16;
		format->color_depth = 16;
		format->uv_hscale = 2;
		format->mask.yuv.y = 0xFF; /* << 0, << 8 ? */
		format->mask.yuv.u = 0xFF;
		format->mask.yuv.v = 0xFF;
		break;

	case TV_PIXFMT_Y8:
		format->bits_per_pixel = 8;
		format->color_depth = 8;
		format->mask.yuv.y = 0xFF;
		format->mask.yuv.u = 0;
		format->mask.yuv.v = 0;
		break;

	case TV_PIXFMT_RGBA24_LE: /* LE 0xAABBGGRR BE 0xRRGGBBAA */
	case TV_PIXFMT_RGBA24_BE: /* LE 0xRRGGBBAA BE 0xAABBGGRR */
	case TV_PIXFMT_BGRA24_LE: /* LE 0xAARRGGBB BE 0xBBGGRRAA */
	case TV_PIXFMT_BGRA24_BE: /* LE 0xBBGGRRAA BE 0xAARRGGBB */
		format->bits_per_pixel = 32;
		format->color_depth = 24;

		if (format->big_endian) {
			format->mask.rgb.r = 0xFF << 24;
			format->mask.rgb.g = 0xFF << 16;
			format->mask.rgb.b = 0xFF << 8;
			format->mask.rgb.a = 0xFF;
		} else {
			format->mask.rgb.r = 0xFF;
			format->mask.rgb.g = 0xFF << 8;
			format->mask.rgb.b = 0xFF << 16;
			format->mask.rgb.a = 0xFF << 24;
		}
		break;

	case TV_PIXFMT_RGB24_LE: /* LE 0xBBGGRR BE 0xRRGGBB */
	case TV_PIXFMT_BGR24_LE: /* LE 0xRRGGBB BE 0xBBGGRR */
		format->bits_per_pixel = 24;
		format->color_depth = 24;

		if (format->big_endian) {
			format->mask.rgb.r = 0xFF << 16;
			format->mask.rgb.b = 0xFF;
		} else {
			format->mask.rgb.r = 0xFF;
			format->mask.rgb.b = 0xFF << 16;
		}

		format->mask.rgb.g = 0xFF << 8;

		break;

	case TV_PIXFMT_RGB16_LE:
	case TV_PIXFMT_RGB16_BE:
	case TV_PIXFMT_BGR16_LE:
	case TV_PIXFMT_BGR16_BE:
		format->bits_per_pixel = 16;
		format->color_depth = 16;
		format->mask.rgb.r = 0x1F;
		format->mask.rgb.g = 0x3F << 5;
		format->mask.rgb.b = 0x1F << 11;
		break;

	case TV_PIXFMT_RGBA15_LE:
	case TV_PIXFMT_RGBA15_BE:
	case TV_PIXFMT_BGRA15_LE:
	case TV_PIXFMT_BGRA15_BE:
		format->bits_per_pixel = 16;
		format->color_depth = 15;
		format->mask.rgb.r = 0x1F;
		format->mask.rgb.g = 0x1F << 5;
		format->mask.rgb.b = 0x1F << 10;
		format->mask.rgb.a = 0x01 << 15;
		break;

	case TV_PIXFMT_ARGB15_LE:
	case TV_PIXFMT_ARGB15_BE:
	case TV_PIXFMT_ABGR15_LE:
	case TV_PIXFMT_ABGR15_BE:
		format->bits_per_pixel = 16;
		format->color_depth = 15;
		format->mask.rgb.r = 0x1F << 1;
		format->mask.rgb.g = 0x1F << 6;
		format->mask.rgb.b = 0x1F << 11;
		format->mask.rgb.a = 0x01;
		break;

	case TV_PIXFMT_RGBA12_LE:
	case TV_PIXFMT_RGBA12_BE:
	case TV_PIXFMT_BGRA12_LE:
	case TV_PIXFMT_BGRA12_BE:
		format->bits_per_pixel = 16;
		format->color_depth = 12;
		format->mask.rgb.r = 0x0F;
		format->mask.rgb.g = 0x0F << 4;
		format->mask.rgb.b = 0x0F << 8;
		format->mask.rgb.a = 0x0F << 12;
		break;

	case TV_PIXFMT_ARGB12_LE:
	case TV_PIXFMT_ARGB12_BE:
	case TV_PIXFMT_ABGR12_LE:
	case TV_PIXFMT_ABGR12_BE:
		format->bits_per_pixel = 16;
		format->color_depth = 12;
		format->mask.rgb.r = 0x0F << 4;
		format->mask.rgb.g = 0x0F << 8;
		format->mask.rgb.b = 0x0F << 12;
		format->mask.rgb.a = 0x0F;
		break;

	case TV_PIXFMT_RGB8:
		format->bits_per_pixel = 8;
		format->color_depth = 8;
		format->mask.rgb.r = 0x07;
		format->mask.rgb.g = 0x07 << 3;
		format->mask.rgb.b = 0x03 << 6;
		break;

	case TV_PIXFMT_BGR8:
		format->bits_per_pixel = 8;
		format->color_depth = 8;
		format->mask.rgb.r = 0x03 << 6;
		format->mask.rgb.g = 0x07 << 3;
		format->mask.rgb.b = 0x07;
		break;

	case TV_PIXFMT_RGBA7:
	case TV_PIXFMT_BGRA7:
		format->bits_per_pixel = 8;
		format->color_depth = 7;
		format->mask.rgb.r = 0x03;
		format->mask.rgb.g = 0x07 << 2;
		format->mask.rgb.b = 0x03 << 5;
		format->mask.rgb.a = 0x01 << 7;
		break;

	case TV_PIXFMT_ARGB7:
	case TV_PIXFMT_ABGR7:
		format->bits_per_pixel = 8;
		format->color_depth = 7;
		format->mask.rgb.r = 0x03 << 1;
		format->mask.rgb.g = 0x07 << 3;
		format->mask.rgb.b = 0x03 << 6;
		format->mask.rgb.a = 0x01;
		break;

	default:
		return FALSE;
	}

	if (attr & 0x10)
		SWAP (format->mask.rgb.r, format->mask.rgb.b);

	if (attr & 0x40) {
		SWAP (format->mask.yuv.u, format->mask.yuv.v);
		format->vu_order = TRUE;
	} else {
		format->vu_order = FALSE;
	}

	return TRUE;
}

/* Note this works only for RGB formats. */
tv_bool
tv_pixel_format_to_pixfmt	(tv_pixel_format *	format)
{
	unsigned int mask;
	unsigned int r_msb;
	unsigned int a_lsb;

	mask = format->mask.rgb.r | format->mask.rgb.g | format->mask.rgb.b;

	if (0 == mask)
		return FALSE;

	if (0 == format->mask.rgb.a)
		format->mask.rgb.a = mask
			^ (0xFFFFFFFFUL >> (32 - format->bits_per_pixel));

	a_lsb = (format->mask.rgb.a <= mask);
	r_msb = (format->mask.rgb.r >= format->mask.rgb.b);

	if (0 == format->color_depth) {
		if (0 == format->bits_per_pixel)
			return FALSE;
		else
			format->color_depth = format->bits_per_pixel;
	}

	switch (format->color_depth) {
	case 32:
	case 24:
		if (24 == format->bits_per_pixel) {
			static tv_pixfmt mapping [] = {
				TV_PIXFMT_RGB24_LE, TV_PIXFMT_BGR24_LE
			};

			format->pixfmt = mapping [r_msb];
			break;
		} else {
			static tv_pixfmt mapping [] = {
				TV_PIXFMT_RGBA24_LE, TV_PIXFMT_RGBA24_BE,
				TV_PIXFMT_BGRA24_LE, TV_PIXFMT_BGRA24_BE,
				TV_PIXFMT_ARGB24_LE, TV_PIXFMT_ARGB24_BE,
				TV_PIXFMT_ABGR24_LE, TV_PIXFMT_ABGR24_BE,
			};

			format->pixfmt = mapping [a_lsb * 4 + r_msb * 2
						  + format->big_endian];
			break;
		}

	case 16:
	{
		static tv_pixfmt mapping [] = {
			TV_PIXFMT_RGB16_LE, TV_PIXFMT_RGB16_BE,
			TV_PIXFMT_BGR16_LE, TV_PIXFMT_BGR16_BE,
		};

		format->pixfmt = mapping [r_msb * 2 + format->big_endian];
		break;
	}

	case 15:
	{
		static tv_pixfmt mapping [] = {
			TV_PIXFMT_RGBA15_LE, TV_PIXFMT_RGBA15_BE,
			TV_PIXFMT_BGRA15_LE, TV_PIXFMT_BGRA15_BE,
			TV_PIXFMT_ARGB15_LE, TV_PIXFMT_ARGB15_BE,
			TV_PIXFMT_ABGR15_LE, TV_PIXFMT_ABGR15_BE,
		};

		format->pixfmt = mapping [a_lsb * 4 + r_msb * 2
					  + format->big_endian];
		break;
	}

	case 12:
	{
		static tv_pixfmt mapping [] = {
			TV_PIXFMT_RGBA12_LE, TV_PIXFMT_RGBA12_BE,
			TV_PIXFMT_BGRA12_LE, TV_PIXFMT_BGRA12_BE,
			TV_PIXFMT_ARGB12_LE, TV_PIXFMT_ARGB12_BE,
			TV_PIXFMT_ABGR12_LE, TV_PIXFMT_ABGR12_BE,
		};

		format->pixfmt = mapping [a_lsb * 4 + r_msb * 2
					  + format->big_endian];
		break;
	}

	case 8:
	{
		static tv_pixfmt mapping [] = {
			TV_PIXFMT_RGB8, TV_PIXFMT_BGR8
		};

		format->pixfmt = mapping [r_msb];
		break;
	}

	case 7:
	{
		static tv_pixfmt mapping [] = {
			TV_PIXFMT_RGBA7, TV_PIXFMT_BGRA7,
			TV_PIXFMT_ARGB7, TV_PIXFMT_ABGR7,
		};

		format->pixfmt = mapping [a_lsb * 2 + r_msb];
		break;
	}

	default:
		return FALSE;
	}

	format->_reserved1 = 0;

	format->uv_hscale = 1;
	format->uv_vscale = 1;

	format->planar = 0;
	format->vu_order = 0;

	return TRUE;
}

/* Helper function for backends. Yes, it's inadequate, but
   that's how some drivers work. Note the depth 32 exception. */
tv_pixfmt
pig_depth_to_pixfmt		(unsigned int		depth)
{
	switch (depth) {
#if BYTE_ORDER == BIG_ENDIAN
	case 15:	return TV_PIXFMT_BGRA15_BE;
	case 16:	return TV_PIXFMT_BGR16_BE;
	case 24:	return TV_PIXFMT_BGR24_BE;
	case 32:	return TV_PIXFMT_BGRA24_BE;
#else
	case 15:	return TV_PIXFMT_BGRA15_LE;
	case 16:	return TV_PIXFMT_BGR16_LE;
	case 24:	return TV_PIXFMT_BGR24_LE;
	case 32:	return TV_PIXFMT_BGRA24_LE;
#endif
	default:	return TV_PIXFMT_UNKNOWN;
	}
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
	tv_callback_destroy (p, &p->_callback);				\
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
	if (info->cur_##item != p) {					\
		info->cur_##item = (tv_##kind *) p;			\
		tv_callback_notify (info, info,				\
				    info->priv->item##_callback);	\
	}								\
} while (0)

#define FREE_ITEM_FUNC(item, kind)					\
void									\
free_##item##s			(tveng_device_info *	info)		\
{									\
	STORE_CURRENT (item, kind, NULL); /* unknown */			\
									\
	free_##kind##_list (&info->item##s);				\
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
	t_assert (info != NULL);					\
	t_assert (p != NULL);	  					\
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
free_control			(tv_control *		tc)
{
	tv_callback_destroy (tc, &tc->_callback);

	if (tc->menu) {
		unsigned int i;

		for (i = 0; tc->menu[i]; ++i)
			free (tc->menu[i]);

		free (tc->menu);
	}

	FREE_NODE (tc);
}

FREE_LIST_FUNC (control);

void
free_controls			(tveng_device_info *	info)
{
	free_control_list (&info->controls);
}

tv_control *
append_control			(tveng_device_info *	info,
				 tv_control *		tc,
				 unsigned int		size)
{
	tv_control **list;

	for (list = &info->controls; *list; list = &(*list)->_next)
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

FREE_NODE_FUNC (video_standard);
FREE_LIST_FUNC (video_standard);

FREE_ITEM_FUNC (video_standard, video_standard);
STORE_CURRENT_FUNC (video_standard, video_standard);

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
		if (l->videostd_set & ts->videostd_set)
			fprintf (stderr, "WARNING: TVENG: Video standard "
				 "set collision between %s (0x%llx) "
				 "and %s (0x%llx)\n",
				 l->label, l->videostd_set,
				 ts->label, videostd_set);
		list = &l->_next;
	}

	*list = ts;

	if (videostd_set & TV_VIDEOSTD_SET_525_60) {
		ts->frame_width		= 640;
		ts->frame_height	= 480;
		ts->frame_rate		= 30000 / 1001.0;
	} else {
		ts->frame_width		= 768;
		ts->frame_height	= 576;
		ts->frame_rate		= 25.0;
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

	assert (type != TV_AUDIO_LINE_TYPE_NONE);
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







static __inline__ void
tveng_copy_block		(uint8_t *		src,
				 uint8_t *		dst,
				 unsigned int		src_stride,
				 unsigned int		dst_stride,
				 unsigned int		lines)
{
  /* XXX dont copy padding if > 32 bytes;
     eventually use MMX/SSE. */

	if (src_stride != dst_stride) {
		unsigned int min_stride = MIN (src_stride, dst_stride);

		for (; lines-- > 0; src += src_stride, dst += dst_stride)
			memcpy (dst, src, min_stride);
	} else {
		memcpy (dst, src, src_stride * lines);
	}
}

/* XXX should take dst*, src*, tv_image_format* */
void
tveng_copy_frame		(unsigned char *	src,
				 tveng_image_data *	where,
				 tveng_device_info *	info)
{
	if (TV_PIXFMT_IS_PLANAR (info->format.pixfmt)) {
		unsigned int size_y;
		uint8_t *src_y;
		uint8_t *src_u;
		uint8_t *src_v;

		t_assert (TV_PIXFMT_YUV420 == info->format.pixfmt
			  || TV_PIXFMT_YVU420 == info->format.pixfmt);

		size_y = info->format.height * info->format.width;

		src_y = src;
		src_u = src_y + size_y;
		src_v = src_u + (size_y >> 2);

		/* XXX shortcut if no padding between planes. */

		tveng_copy_block (src_y, where->planar.y,
				  info->format.width,
				  where->planar.y_stride,
				  info->format.height);

		tveng_copy_block (src_u, where->planar.u,
				  info->format.width >> 1,
				  where->planar.uv_stride,
				  info->format.height >> 1);

		tveng_copy_block (src_v, where->planar.v,
				  info->format.width >> 1,
				  where->planar.uv_stride,
				  info->format.height >> 1);
	} else {
		tveng_copy_block (src, where->linear.data,
				  info->format.bytes_per_line,
				  where->linear.stride,
				  info->format.height);
	}
}

/*
 *  Clip vector
 */

tv_bool
tv_clip_vector_equal		(const tv_clip_vector *	vector1,
				 const tv_clip_vector *	vector2)
{
	unsigned int i;

	if (vector1->size != vector2->size)
		return FALSE;

	for (i = 0; i < vector1->size; ++i)
		if (!tv_clip_equal (vector1->vector + i,
				    vector2->vector + i))
			return FALSE;

	return TRUE;
}

tv_bool
tv_clip_vector_copy		(tv_clip_vector *	dst,
				 const tv_clip_vector *	src)
{
	tv_clip *clip_vector;

	if (src->size > dst->capacity) {
		if (!(clip_vector = realloc (dst->vector, src->capacity)))
			return FALSE;

		dst->vector = clip_vector;
		dst->capacity = src->capacity;
	}

	memcpy (dst->vector, src->vector, sizeof (*src->vector) * src->size);
	dst->size = src->size;

	return TRUE;
}

/* Note: width = x2 - x1, height = y2 - y1. */
tv_bool
tv_clip_vector_add_clip_xy	(tv_clip_vector *	vector,
				 unsigned int		x1,
				 unsigned int		y1,
				 unsigned int		x2,
				 unsigned int		y2)
{
	tv_clip *clip;
	tv_clip *end;
	unsigned int height;

	if (x1 >= x2 || y1 >= y2)
		return TRUE;

	clip = vector->vector;
	end = vector->vector + vector->size;

	/* Clips are sorted in ascending order, first by y1, second x1.
	   We split into horizontal bands, merging clips where possible.
	   Vertically adjacent clips or bands are not merged. */

	for (;;) {
		if (clip >= end)
			goto insert;

		if (y1 < clip->y) {
			if (y2 > clip->y) {
				unsigned int n = clip - vector->vector;

				/* Insert bottom half. */
				if (!tv_clip_vector_add_clip_xy
				    (vector, x1, clip->y, x2, y2))
					return FALSE;

				clip = vector->vector + n;
				end = vector->vector + vector->size;

				/* Top half. */
				y2 = clip->y;
			}

			goto insert;
		}

		if (y1 == clip->y)
			break;

		++clip;
	}

	height = y2 - y1;

	if (height < clip->height) {
		unsigned int n, i, h;

		/* Split band at y2. */

		h = clip->height - height;

		for (n = 1; clip + n < end; ++n)
			if (y1 != clip[n].y)
				break;

		if (vector->size + n >= vector->capacity) {
			tv_clip *clip_vector;

			/* n <= size <= capacity */
			i = vector->capacity * 2;

			if (!(clip_vector = realloc (vector->vector, i)))
				return FALSE;

			clip = clip_vector + (clip - vector->vector);
			end = clip_vector + vector->size;

			vector->vector = clip_vector;
			vector->capacity = i;
		}

		memmove (clip + n, clip, sizeof (*clip) * (end - clip));

		vector->size += n;
		end += n;

		for (i = 0; i < n; ++i) {
			clip[i].height = height;
			clip[i + n].y = y2;
			clip[i + n].height = h;
		}
	} else if (height > clip->height) {
		unsigned int n = clip - vector->vector;

		/* Insert bottom half. */
		if (!tv_clip_vector_add_clip_xy
		    (vector, x1, y1 + clip->height, x2, y2))
			return FALSE;

		clip = vector->vector + n;
		end = vector->vector + vector->size;

		/* Top half. */
		y2 = y1 + clip->height;
	}

	do {
		unsigned int cx2;

		if (x2 < clip->x)
			break; /* insert before */

		cx2 = clip->x + clip->width;

		if (x1 <= cx2) {
			tv_clip *c = clip;

			/* Merge clips. */

			for (;;) {
				x1 = MIN (x1, clip->x);
				x2 = MAX (x2, cx2);

				if (clip + 1 >= end ||
				    y1 != clip[1].y ||
				    x2 < clip[1].x)
					break;

				++clip;

				cx2 = clip->x + clip->width;
			}

			if (clip > c) {
				memmove (c, clip, sizeof (*clip)
					 * (end - clip));
				vector->size -= (clip - c);
				clip = c;
			}

			goto store;
		}

	} while (++clip < end && y1 == clip->y);

 insert:
	if (vector->size >= vector->capacity) {
		tv_clip *clip_vector;
		unsigned int c;

		c = (vector->capacity < 16) ? 16 : vector->capacity * 2;

		if (!(clip_vector = realloc (vector->vector, c)))
			return FALSE;

		clip = clip_vector + (clip - vector->vector);
		end = clip_vector + vector->size;

		vector->vector = clip_vector;
		vector->capacity = c;
	}

	if (clip < end)
		memmove (clip + 1, clip, sizeof (*clip) * vector->size);

	++vector->size;

 store:
	clip->x = x1;
	clip->y = y1;
	clip->width = x2 - x1;
	clip->height = y2 - y1;

	if (0) {
		unsigned int i;

		clip = vector->vector;

		for (i = 0; i < vector->size; ++i, ++clip)
			fprintf (stderr, "%3u: %3u, %3u - %3u, %3u\n",
				 i, clip->x, clip->y,
				 clip->x + clip->width,
				 clip->y + clip->height);
	}

	return TRUE;
}

/* XXX untested. Should create another mask_to_vector to test if
   the clips are equivalent. */
uint8_t *
tv_clip_vector_to_clip_mask	(tv_clip_vector *	vector,
				 unsigned int		width,
				 unsigned int		height)
{
	uint8_t *mask;
	unsigned int bpl;
	tv_clip *clip;
	unsigned int count;

	bpl = (width + 7) & -8; 

	if (!(mask = calloc (1, height * bpl)))
		return NULL;

	clip = vector->vector;

	for (count = vector->size; count > 0; --count) {
		unsigned int x7;
		unsigned int xw;
		uint8_t *start;
		uint8_t lmask;
		uint8_t rmask;
		unsigned int length;
		unsigned int count;

		if (clip->x + clip->width > width)
			goto failure;

		if (clip->y + clip->height > height)
			goto failure;

		start = mask + clip->y * bpl + (clip->x >> 3);

		x7 = clip->x & 7;
		xw = x7 + clip->width;

		lmask = ~0 << x7;
		rmask = ~(~0 << (xw & 7));

		length = (xw - 1) >> 3;

		if (0 == length) {
			lmask &= rmask;

			for (count = clip->height; count > 0; --count) {
				*start |= lmask;
				start += bpl;
			}
		} else {
			for (count = clip->height; count > 0; --count) {
				unsigned int i;

				start[0] |= lmask;

				for (i = 1; i < length; ++i)
					start[i] = 0xFF;

				start[length] |= rmask;

				start += bpl;
			}
		}

		++clip;
	}

	return mask;

 failure:
	free (mask);
	return NULL;
}

tv_bool
tv_image_format_init		(tv_image_format *	format,
				 unsigned int		width,
				 unsigned int		height,
				 tv_pixfmt		pixfmt,
				 unsigned int		reserved)
{
	tv_pixel_format pixel;

	assert (NULL != format);

	if (!tv_pixfmt_to_pixel_format (&pixel, pixfmt, 0))
		return FALSE;

	if (0 == width || 0 == height)
		return FALSE;

	if (pixel.planar) {
		/* Round size up to U and V scale factors. */

		width += pixel.uv_hscale - 1;
		width -= width % pixel.uv_hscale;

		height += pixel.uv_vscale - 1;
		height -= height % pixel.uv_vscale;
	}

	format->width = width;
	format->height = height;

	/* No padding. */
	format->bytes_per_line = (width * pixel.bits_per_pixel + 7) >> 3;

	format->offset = 0;

	if (pixel.planar) {
		unsigned int y_size;
		unsigned int uv_size;

		/* No padding. */
		format->uv_bytes_per_line = width / pixel.uv_hscale;

		y_size = format->bytes_per_line * height;
		uv_size = y_size / (pixel.uv_hscale * pixel.uv_vscale);

		if (pixel.vu_order) {
			format->v_offset = y_size; 
			format->u_offset = y_size + uv_size;
		} else {
			format->u_offset = y_size; 
			format->v_offset = y_size + uv_size;
		}

		format->size = y_size + uv_size * 2;
	} else {
		format->uv_bytes_per_line = 0;

		format->u_offset = 0;
		format->v_offset = 0;

		format->size = format->bytes_per_line * height;
	}

	format->pixfmt = pixfmt;
	format->_reserved = reserved;

	return TRUE;
}

tv_bool
tv_image_format_is_valid	(const tv_image_format *format)
{
	tv_pixel_format pixel;
	unsigned int min_bytes_per_line;
	unsigned int min_size;

	assert (NULL != format);

	if (!tv_pixfmt_to_pixel_format (&pixel, format->pixfmt, 0))
		return FALSE;

	if (0 == format->width
	    || 0 == format->height)
		return FALSE;

	min_bytes_per_line = (format->width * pixel.bits_per_pixel + 7) >> 3;

	if (format->bytes_per_line < min_bytes_per_line)
		return FALSE;

	/* We don't enforce bytes_per_line padding on the last
	   line, in case offset and bytes_per_line were adjusted
	   for cropping. */
	min_size = format->bytes_per_line * (format->height - 1)
		+ min_bytes_per_line;

	if (pixel.planar) {
		unsigned int min_uv_bytes_per_line;
		unsigned int min_uv_size;
		unsigned int p1_offset;
		unsigned int p2_offset;

		if (0 != format->width % pixel.uv_hscale
		    || 0 != format->height % pixel.uv_vscale)
			return FALSE;

		if (0 != format->bytes_per_line % pixel.uv_hscale)
			return FALSE;

		/* U and V bits_per_pixel assumed 8. */
		min_uv_bytes_per_line = format->width / pixel.uv_hscale;

		if (format->uv_bytes_per_line < min_uv_bytes_per_line)
			return FALSE;

		/* We don't enforce bytes_per_line padding on the last line. */
		min_uv_size = format->uv_bytes_per_line
			* (format->height / pixel.uv_vscale - 1)
			+ min_uv_bytes_per_line;

		if (pixel.vu_order != (format->v_offset > format->u_offset))
			return FALSE;

		if (format->u_offset > format->v_offset) {
			p1_offset = format->v_offset;
			p2_offset = format->u_offset;
		} else {
			p1_offset = format->u_offset;
			p2_offset = format->v_offset;
		}

		/* Y before U and V, planes must not overlap. */
		if (format->offset + min_size >= p1_offset)
			return FALSE;

		/* U and V planes must not overlap. */
		if (p1_offset + min_uv_size >= p2_offset)
			return FALSE;

		/* All planes must fit in buffer. */
		if (p2_offset + min_uv_size >= format->size)
			return FALSE;
	} else {
		if (format->offset + min_size >= format->size)
			return FALSE;
	}

	return TRUE;
}
