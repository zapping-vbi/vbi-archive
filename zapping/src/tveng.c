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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <X11/Xlib.h> /* We use some X calls */
#include <X11/Xutil.h>
#include <ctype.h>

/* This undef's are to avoid a couple of header warnings */
#undef WNOHANG
#undef WUNTRACED
#include "tveng.h"
#include "tveng1.h" /* V4L specific headers */
#include "tveng2.h" /* V4L2 specific headers */
#include "tvengxv.h" /* XVideo specific headers */
#include "tveng_private.h" /* private definitions */

#define TVLOCK		pthread_mutex_lock(&(info->private->mutex))
#define UNTVLOCK	pthread_mutex_unlock(&(info->private->mutex))

#define RETURN_UNTVLOCK(X) \
do { __typeof__(X) _unlocked_result = X; \
     UNTVLOCK; \
     return _unlocked_result; \
} while (0)

#define TVUNSUPPORTED do { \
  /* function not supported by the module */ \
 info->tveng_errno = -1; \
 t_error_msg("module", \
	     "function not supported by the module", info); \
} while (0)


typedef void (*tveng_controller)(struct tveng_module_info *info);
static tveng_controller tveng_controllers[] = {
  tvengxv_init_module,
  tveng2_init_module,
  tveng1_init_module
};

/* Initializes a tveng_device_info object */
tveng_device_info * tveng_device_info_new(Display * display, int bpp,
					  const char *default_standard)
{
  size_t needed_mem = 0;
  tveng_device_info * new_object;
  struct tveng_module_info module_info;
  pthread_mutexattr_t attr;
  int i;

  /* Get the needed mem for the controllers */
  for (i=0; i<(sizeof(tveng_controllers)/sizeof(tveng_controller));
       i++)
    {
      tveng_controllers[i](&module_info);
      needed_mem = MAX(needed_mem, module_info.private_size);
    }

  t_assert(needed_mem > 0);

  new_object = (tveng_device_info*) malloc(needed_mem);

  if (!new_object)
    return NULL;

  /* fill the struct with 0's */
  memset(new_object, 0, needed_mem);

  new_object -> private = malloc(sizeof(struct tveng_private));

  if (!new_object->private)
    {
      free(new_object);
      return NULL;
    }

  memset(new_object->private, 0, sizeof(struct tveng_private));

  /* Allocate some space for the error string */
  new_object -> error = (char*) malloc(256);

  if (!new_object->error)
    {
      free(new_object->private);
      free(new_object);
      perror("malloc");
      return NULL;
    }

  new_object->private->display = display;
  new_object->private->bpp = bpp;
  if (default_standard)
    new_object->private->default_standard=strdup(default_standard);
  else
    new_object->private->default_standard=NULL;

  new_object->private->zapping_setup_fb_verbosity = 0; /* No output by
							  default */
#ifdef USE_XV
  new_object->private->port = None;
  new_object->private->filter = None;
  new_object->private->colorkey = None;
  new_object->private->double_buffer = None;
#endif

  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&(new_object->private->mutex), &attr);
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
    tveng_close_device(info);

  if (info -> error)
    free(info -> error);

  if (info -> private->default_standard)
    free(info -> private->default_standard);

  pthread_mutex_destroy(&(info->private->mutex));

  free(info->private);
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
  int i, j;
  char *long_str, *short_str;

  t_assert(device_file != NULL);
  t_assert(info != NULL);

  TVLOCK;

  if (info -> fd) /* If the device is already attached, detach it */
    tveng_close_device(info);

  info -> current_controller = TVENG_CONTROLLER_NONE;

  /*
    Check that the current private->display depth is one of the supported ones
  */
  info->private->current_bpp = tveng_get_display_depth(info);
  switch (info->private->current_bpp)
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
      tveng_controllers[i](&(info->private->module));
      if (!info->private->module.attach_device)
	continue;
      if (-1 != info->private->module.attach_device(device_file, attach_mode,
						    info))
	goto success;
    }

  /* Error */
  info->tveng_errno = -1;
  t_error_msg("check()",
	      "The device cannot be attached to any controller",
	      info);
  memset(&(info->private->module), 0, sizeof(info->private->module));
  UNTVLOCK;
  return -1;

 success:

  if (info->debug_level>0)
    {
      fprintf(stderr, "[TVeng] - Info about the video device\n");
      fprintf(stderr, "-------------------------------------\n");
      tveng_describe_controller(&short_str, &long_str, info);
      fprintf(stderr, "Device: %s [%s - %s]\n", info->file_name,
	      short_str, long_str);
      fprintf(stderr, "Detected framebuffer depth: %d\n",
	      tveng_get_display_depth(info));
      if (info->private->default_standard)
	fprintf(stderr, "On tunerless inputs, the norm defaults to %s\n",
		info->private->default_standard);
      fprintf(stderr, "Current capture format:\n");
      fprintf(stderr, "  Dimensions: %dx%d  BytesPerLine: %d  Depth: %d "
	      "Size: %d K\n", info->format.width,
	      info->format.height, info->format.bytesperline,
	      info->format.depth, info->format.sizeimage/1024);
      fprintf(stderr, "Current overlay window struct:\n");
      fprintf(stderr, "  Coords: %dx%d-%dx%d\n",
	      info->window.x, info->window.y, info->window.width,
	      info->window.height);
      fprintf(stderr, "Detected standards:\n");
      for (i=0;i<info->num_standards;i++)
	fprintf(stderr, "  %d) [%s] ID: %d Size: %dx%d Hash: %x\n", i,
		info->standards[i].name, info->standards[i].id,
		info->standards[i].width, info->standards[i].height,
		info->standards[i].hash);
      fprintf(stderr, "Detected inputs:\n");
      for (i=0;i<info->num_inputs;i++)
	{
	  fprintf(stderr, "  %d) [%s] ID: %d Hash: %x\n", i,
		  info->inputs[i].name, info->inputs[i].id,
		  info->inputs[i].hash);
	  fprintf(stderr, "      Type: %s  Tuners: %d  Flags: 0x%x\n",
		  (info->inputs[i].type == TVENG_INPUT_TYPE_TV) ? _("TV")
		  : _("Camera"), info->inputs[i].tuners,
		  info->inputs[i].flags);
	}
      fprintf(stderr, "Available controls:\n");
      for (i=0;i<info->num_controls;i++)
	{
	  fprintf(stderr, "  %d) [%s] ID: %d  Range: (%d, %d)  Value: %d ",
		  i, info->controls[i].name, info->controls[i].id,
		  info->controls[i].min, info->controls[i].max,
		  info->controls[i].cur_value);
	  switch (info->controls[i].type)
	    {
	    case TVENG_CONTROL_SLIDER:
	      fprintf(stderr, " <Slider>\n");
	      break;
	    case TVENG_CONTROL_CHECKBOX:
	      fprintf(stderr, " <Checkbox>\n");
	      break;
	    case TVENG_CONTROL_MENU:
	      fprintf(stderr, " <Menu>\n");
	      for (j=0; info->controls[i].data[j]; j++)
		fprintf(stderr, "    %d.%d) [%s] <Menu entry>\n", i, j,
			info->controls[i].data[j]);
	      break;
	    case TVENG_CONTROL_BUTTON:
	      fprintf(stderr, " <Button>\n");
	      break;
	    default:
	      fprintf(stderr, " <Unknown type>\n");
	      break;
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

  if (info->private->module.describe_controller)
    info->private->module.describe_controller(short_str, long_str,
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
void tveng_close_device(tveng_device_info * info)
{
  t_assert(info != NULL);

  if (info->current_controller == TVENG_CONTROLLER_NONE)
    return; /* nothing to be done */

  TVLOCK;

  tveng_stop_everything(info);

  if (info->private->module.close_device)
    info->private->module.close_device(info);

  UNTVLOCK;
}

/*
  Functions for controlling the video capture. All of them return -1
  in case of error, so any value != -1 should be considered valid
  (unless explicitly stated in the description of the function) 
*/
/*
  Returns the number of inputs in the given device and fills in info
  with the correct info, allocating memory as needed
*/
int tveng_get_inputs(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->private->module.get_inputs)
    RETURN_UNTVLOCK(info->private->module.get_inputs(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

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

/*
  Sets the current input for the capture
*/
int tveng_set_input(struct tveng_enum_input * input,
		    tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(input != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->private->module.set_input)
    RETURN_UNTVLOCK(info->private->module.set_input(input, info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Sets the input named name as the active input. -1 on error.
*/
int
tveng_set_input_by_name(const char * input_name,
			tveng_device_info * info)
{
  int i;

  t_assert(input_name != NULL);
  t_assert(info != NULL);

  TVLOCK;

  for (i = 0; i < info->num_inputs; i++)
    if (tveng_normstrcmp(info->inputs[i].name, input_name))
      RETURN_UNTVLOCK(tveng_set_input(&(info->inputs[i]), info));

  info->tveng_errno = -1;
  t_error_msg("finding",
	      "Input %s doesn't appear to exist", info, input_name);

  UNTVLOCK;
  return -1; /* String not found */
}

/*
  Sets the active input by its id (may not be the same as its array
  index, but it should be). -1 on error
*/
int
tveng_set_input_by_id(int id, tveng_device_info * info)
{
  int i;

  t_assert(info != NULL);

  TVLOCK;

  for (i = 0; i < info->num_inputs; i++)
    if (info->inputs[i].id == id)
      RETURN_UNTVLOCK(tveng_set_input(&(info->inputs[i]), info));

  info->tveng_errno = -1;
  t_error_msg("finding",
	      "Input number %d doesn't appear to exist", info, id);

  UNTVLOCK;
  return -1; /* String not found */
}

/*
  Sets the active input by its index in inputs. -1 on error
*/
int
tveng_set_input_by_index(int index, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(index > -1);

  TVLOCK;

  if (info->num_inputs)
    {
      t_assert(index < info -> num_inputs);
      RETURN_UNTVLOCK((tveng_set_input(&(info -> inputs[index]), info)));
    }

  UNTVLOCK;
  return 0;
}

/**
 * Finds the input with the given hash, or NULL.
 * The hash is based on the input normalized name.
 */
struct tveng_enum_input *
tveng_find_input_by_hash(int hash, tveng_device_info *info)
{
  int i;

  t_assert(info != NULL);

  for (i=0; i<info->num_inputs; i++)
    if (info->inputs[i].hash == hash)
      return &(info->inputs[i]);

  return  NULL;
}

/*
  Queries the device about its standards. Fills in info as appropiate
  and returns the number of standards in the device. This is for the
  first tuner in the current input, should be enough since most (all)
  inputs have 1 or less tuners.
*/
int tveng_get_standards(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->private->module.get_standards)
    RETURN_UNTVLOCK(info->private->module.get_standards(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Sets the current standard for the capture. standard is the name for
  the desired standard. updates cur_standard
*/
int tveng_set_standard(struct tveng_enumstd * std, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(std != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->private->module.set_standard)
    RETURN_UNTVLOCK(info->private->module.set_standard(std, info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Sets the standard by name. -1 on error
*/
int
tveng_set_standard_by_name(const char * name, tveng_device_info * info)
{
  int i;

  t_assert(info != NULL);

  TVLOCK;

  for (i = 0; i < info->num_standards; i++)
    if (tveng_normstrcmp(name, info->standards[i].name))
      RETURN_UNTVLOCK(tveng_set_standard(&(info->standards[i]), info));

  info->tveng_errno = -1;
  t_error_msg("finding",
	      "Standard %s doesn't appear to exist", info, name);

  UNTVLOCK;
  return -1; /* String not found */  
}

/*
  Sets the standard by id.
*/
int
tveng_set_standard_by_id(int id, tveng_device_info * info)
{
  int i;

  t_assert(info != NULL);

  TVLOCK;

  for (i = 0; i < info->num_standards; i++)
    if (info->standards[i].id == id)
      RETURN_UNTVLOCK(tveng_set_standard(&(info->standards[i]), info));

  info->tveng_errno = -1;
  t_error_msg("finding",
	      "Standard number %d doesn't appear to exist", info, id);

  UNTVLOCK;
  return -1; /* id not found */
}

/*
  Sets the standard by index. -1 on error
*/
int
tveng_set_standard_by_index(int index, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(index > -1);

  TVLOCK;

  if (info->num_standards)
    {
      t_assert(index < info->num_standards);
      RETURN_UNTVLOCK(tveng_set_standard(&(info->standards[index]), info));
    }

  UNTVLOCK;
  return 0;
}

/**
 * Finds the standard with the given hash, or NULL.
 * The hash is based on the standard normalized name.
 */
struct tveng_enumstd *
tveng_find_standard_by_hash(int hash, tveng_device_info *info)
{
  int i;

  t_assert(info != NULL);

  for (i=0; i<info->num_standards; i++)
    if (info->standards[i].hash == hash)
      return &(info->standards[i]);

  return  NULL;
}

/* Updates the current capture format info. -1 if failed */
int
tveng_update_capture_format(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->private->module.update_capture_format)
    RETURN_UNTVLOCK(info->private->module.update_capture_format(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* -1 if failed. Sets the pixformat and fills in info -> pix_format
   with the correct values  */
int
tveng_set_capture_format(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->format.height < info->caps.minheight)
    info->format.height = info->caps.minheight;
  if (info->format.height > info->caps.maxheight)
    info->format.height = info->caps.maxheight;
  if (info->format.width < info->caps.minwidth)
    info->format.width = info->caps.minwidth;
  if (info->format.width > info->caps.maxwidth)
    info->format.width = info->caps.maxwidth;

  /* force dword aligning of width if requested */
  if (info->private->dword_align)
    info->format.width = (info->format.width+3) & ~3;

  /* force dword-aligning of data */
  switch (info->format.pixformat)
    {
    case TVENG_PIX_RGB565:
    case TVENG_PIX_RGB555:
    case TVENG_PIX_YUYV:
    case TVENG_PIX_UYVY:
      info->format.width = (info->format.width+1) & ~1;
      break;
    case TVENG_PIX_YUV420:
    case TVENG_PIX_YVU420:
    case TVENG_PIX_RGB24:
    case TVENG_PIX_BGR24:
      info->format.width = (info->format.width+3) & ~3;
      break;
    default:
      break;
    }

  if (info->private->module.set_capture_format)
    RETURN_UNTVLOCK(info->private->module.set_capture_format(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;

}

/*
  Gets the current value of the controls, fills in info->controls
  appropiately. After this (and if it succeeds) you can look in
  info->controls to get the values for each control. -1 on error
*/
int
tveng_update_controls(tveng_device_info * info)
{
  struct tveng_control * control;
  int i;
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  /* Update the controls we maintain */
  for (i=0; i<info->num_controls; i++)
    {
      control = &(info->controls[i]);
      if (control->controller != TVENG_CONTROLLER_MOTHER)
	continue; /* not created by us */
#ifdef USE_XV
      if ((control->id == (int)info->private->filter) &&
	  (info->private->port != None))
	{
	  XvGetPortAttribute(info->private->display,
			     info->private->port,
			     info->private->filter,
			     &(info->controls[i].cur_value));
	  UNTVLOCK;
	  return 0;
	}
      else if ((control->id == (int)info->private->double_buffer) &&
	       (info->private->port))
	{
	  XvGetPortAttribute(info->private->display,
			     info->private->port,
			     info->private->double_buffer,
			     &(info->controls[i].cur_value));
	  UNTVLOCK;
	  return 0;
	}
      else if ((control->id == (int)info->private->colorkey) &&
	       (info->private->port != None))
	{
	  int r,g,b, val;
	  int rm=0xff, gm=0xff, bm=0xff, rs=16, gs=8, bs=0; /* masks, shifts */
	  XvGetPortAttribute(info->private->display,
			     info->private->port,
			     info->private->colorkey,
			     &(val));
	  /* Adjust colorkey to the current pixformat */
	  switch (info->private->current_bpp)
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
	  info->controls[i].cur_value = (r<<16)+(g<<8)+b;
	  UNTVLOCK;
	  return 0;
	}
#endif
    }

  if (info->private->module.update_controls)
    RETURN_UNTVLOCK(info->private->module.update_controls(info));

  TVUNSUPPORTED;
  TVLOCK;
  return -1;
}

/*
  Sets the value for an specific control. The given value will be
  clipped between min and max values. Returns -1 on error
*/
int
tveng_set_control(struct tveng_control * control, int value,
		  tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(control != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (control->controller == TVENG_CONTROLLER_MOTHER)
    {
#ifdef USE_XV
      if ((control->id == (int)info->private->filter) &&
	  (info->private->port != None))
	{
	  XvSetPortAttribute(info->private->display,
			     info->private->port,
			     info->private->filter,
			     value);
	  UNTVLOCK;
	  return 0;
	}
      else if ((control->id == (int)info->private->double_buffer) &&
	       (info->private->port != None))
	{
	  XvSetPortAttribute(info->private->display,
			     info->private->port,
			     info->private->double_buffer,
			     value);
	  UNTVLOCK;
	  return 0;
	}
      else if ((control->id == (int)info->private->colorkey) &&
	       (info->private->port != None))
	{
	  int r, g, b;
	  int rm=0xff, gm=0xff, bm=0xff, rs=16, gs=8, bs=0; /* masks, shifts */
	  /* Adjust colorkey to the current pixformat */
	  switch (info->private->current_bpp)
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
	  XvSetPortAttribute(info->private->display,
			     info->private->port,
			     info->private->colorkey,
			     value);
	  UNTVLOCK;
	  return 0;
	}
      else
#endif
	{
	  info->tveng_errno = -1;
	  t_error_msg("check",
		      "Unknown control given: %d", info, control->id);
	  UNTVLOCK;
	  return -1;
	}
    }
  else if (info->private->module.set_control)
    RETURN_UNTVLOCK(info->private->module.set_control(control, value, info));
  
  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
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
  int i;
  int value;

  t_assert(info != NULL);
  t_assert(info -> num_controls > 0);
  t_assert(control_name != NULL);

  TVLOCK;

  /* Update the controls (their values) */
  if (tveng_update_controls(info) == -1)
    RETURN_UNTVLOCK(-1);

  /* iterate through the info struct to find the control */
  for (i = 0; i < info->num_controls; i++)
    if (!strcasecmp(info->controls[i].name,control_name))
      /* we found it */
      {
	value = info->controls[i].cur_value;
	t_assert(value <= info->controls[i].max);
	t_assert(value >= info->controls[i].min);
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
  int i;

  t_assert(info != NULL);
  t_assert(info -> num_controls > 0);

  TVLOCK;

  /* iterate through the info struct to find the mute control */
  for (i = 0; i < info->num_controls; i++)
    if (!strcasecmp(info->controls[i].name,control_name))
      /* we found it */
      RETURN_UNTVLOCK((tveng_set_control(&(info->controls[i]),
				       new_value, info)));

  /* if we reach this, we haven't found the control */
  info->tveng_errno = -1;
  t_error_msg("finding",
	   "Cannot find control \"%s\" in the list of controls",
	   info, control_name);
  UNTVLOCK;
  return -1;
}

/*
  Gets the value of a control, given its control id. -1 on error (or
  cid not found). The result is stored in cur_value.
*/
int
tveng_get_control_by_id(int cid, int * cur_value,
			tveng_device_info * info)
{
  int i;
  int value;

  t_assert(info != NULL);
  t_assert(info -> num_controls > 0);

  TVLOCK;

  /* Update the controls (their values) */
  if (tveng_update_controls(info) == -1)
    RETURN_UNTVLOCK(-1);

  /* iterate through the info struct to find the mute control */
  for (i = 0; i < info->num_controls; i++)
    if (info->controls[i].id == cid)
      /* we found it */
      {
	value = info->controls[i].cur_value;
	t_assert(value <= info->controls[i].max);
	t_assert(value >= info->controls[i].min);
	if (cur_value)
	  *cur_value = value;
	UNTVLOCK;
	return 0; /* Success */
      }

  /* if we reach this, we haven't found the control */
  info->tveng_errno = -1;
  t_error_msg("finding",
	      "Cannot find control %d in the list of controls",
	      info, cid);
  UNTVLOCK;
  return -1;
}

/*
  Sets a control by its id. Returns -1 on error
*/
int tveng_set_control_by_id(int cid, int new_value,
			    tveng_device_info * info)
{
  int i;

  t_assert(info != NULL);
  t_assert(info -> num_controls > 0);

  TVLOCK;

  /* iterate through the info struct to find the mute control */
  for (i = 0; i < info->num_controls; i++)
    if (info->controls[i].id == cid)
      /* we found it */
      RETURN_UNTVLOCK(tveng_set_control(&(info->controls[i]), new_value,
				      info));

  /* if we reach this, we haven't found the control */
  info->tveng_errno = -1;
  t_error_msg("finding",
	      "Cannot find control %d in the list of controls",
	      info, cid);
  UNTVLOCK;
  return -1;
}

/*
  Gets the value of the mute property. 1 means mute (no sound) and 0
  unmute (sound). -1 on error
*/
int
tveng_get_mute(tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->private->module.get_mute)
    RETURN_UNTVLOCK(info->private->module.get_mute(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Sets the value of the mute property. 0 means unmute (sound) and 1
  mute (no sound). -1 on error
*/
int
tveng_set_mute(int value, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->private->module.set_mute)
    RETURN_UNTVLOCK(info->private->module.set_mute(value, info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Tunes the current input to the given freq. Returns -1 on error.
*/
int
tveng_tune_input(uint32_t freq, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->private->module.tune_input)
    RETURN_UNTVLOCK(info->private->module.tune_input(freq, info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

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

  if (info->private->module.get_signal_strength)
    RETURN_UNTVLOCK(info->private->module.get_signal_strength(strength,
							    afc,
							    info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Stores in freq the currently tuned freq. Returns -1 on error.
*/
int
tveng_get_tune(uint32_t * freq, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(freq != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->private->module.get_tune)
    RETURN_UNTVLOCK(info->private->module.get_tune(freq, info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Gets the minimum and maximum freq that the current input can
  tune. If there is no tuner in this input, -1 will be returned.
  If any of the pointers is NULL, its value will not be filled.
*/
int
tveng_get_tuner_bounds(uint32_t * min, uint32_t * max, tveng_device_info *
		       info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->private->module.get_tuner_bounds)
    RETURN_UNTVLOCK(info->private->module.get_tuner_bounds(min, max,
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

  TVLOCK;

  if (info->private->module.start_capturing)
    RETURN_UNTVLOCK(info->private->module.start_capturing(info));

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

  TVLOCK;

  if (info->private->module.stop_capturing)
    RETURN_UNTVLOCK(info->private->module.stop_capturing(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* 
   Reads a frame from the video device, storing the read data in
   the location pointed to by dest.
   time: time to wait using select() in miliseconds
   info: pointer to the video device info structure
   Returns -1 on error, anything else on success
*/
int tveng_read_frame(void * dest, unsigned int bpl, 
		     unsigned int time, tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->private->module.read_frame)
    RETURN_UNTVLOCK(info->private->module.read_frame(dest, bpl, time,
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

  TVLOCK;

  if (info->private->module.get_timestamp)
    RETURN_UNTVLOCK(info->private->module.get_timestamp(info));

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

  TVLOCK;

  if (info->private->dword_align)
    width = (width & ~3);
  if (width < info->caps.minwidth)
    width = info->caps.minwidth;
  if (width > info->caps.maxwidth)
    width = info->caps.maxwidth;
  if (height < info->caps.minheight)
    height = info->caps.minheight;
  if (height > info->caps.maxheight)
    height = info->caps.maxheight;

  if (info->private->module.set_capture_size)
    RETURN_UNTVLOCK(info->private->module.set_capture_size(width,
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

  TVLOCK;

  if (info->private->module.get_capture_size)
    RETURN_UNTVLOCK(info->private->module.get_capture_size(width,
							 height,
							 info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/* XF86 Frame Buffer routines */
/* 
   Detects the presence of a suitable Frame Buffer.
   1 if the program should continue (Frame Buffer present,
   available and suitable)
   0 if the framebuffer shouldn't be used.
   private->display: The private->display we are connected to (gdk_private->display)
   info: Its fb member is filled in
*/
int
tveng_detect_XF86DGA(tveng_device_info * info)
{
#ifndef DISABLE_X_EXTENSIONS
  int event_base, error_base;
  int major_version, minor_version;
  int flags;
  static int info_printed = 0; /* Print the info just once */

  Display * dpy = info->private->display;

  TVLOCK;

  if (!XF86DGAQueryExtension(dpy, &event_base, &error_base))
    {
      perror("XF86DGAQueryExtension");
      UNTVLOCK;
      return 0;
    }

  if (!XF86DGAQueryVersion(dpy, &major_version, &minor_version))
    {
      perror("XF86DGAQueryVersion");
      UNTVLOCK;
      return 0;
    }

  if (!XF86DGAQueryDirectVideo(dpy, 0, &flags))
    {
      perror("XF86DGAQueryDirectVideo");
      UNTVLOCK;
      return 0;
    }

  /* Direct Video should be present (otherwise enabling all this would
     be pointless) */
  if (!(flags & XF86DGADirectPresent))
    {
      printf("flags & XF86DGADirectPresent\n");
      UNTVLOCK;
      return 0;
    }

  /* Print collected info if we are in debug mode */
  if ((info->debug_level > 0) && (!info_printed))
    {
      info_printed = 1;
      fprintf(stderr, "DGA info:\n");
      fprintf(stderr, "  - event and error base  : %d, %d\n", event_base,
	      error_base);
      fprintf(stderr, "  - DGA reported version  : %d.%d\n",
	      major_version, minor_version);
      fprintf(stderr, "  - Supported features    :%s\n",
	      (flags & XF86DGADirectPresent) ? " DirectVideo" : "");
    }

  UNTVLOCK;
  return 1; /* Everything correct */
#else

  return 0; /* disabled by configure */
#endif
}

/*
  Returns 1 if the device attached to info suports previewing, 0 otherwise
*/
int
tveng_detect_preview (tveng_device_info * info)
{
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->private->module.detect_preview)
    RETURN_UNTVLOCK(info->private->module.detect_preview(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return 0;
}

/* 
   Runs zapping_setup_fb with the actual verbosity value.
   Returns -1 in case of error, 0 otherwise.
   This calls (or tries to) the external program zapping_setup_fb,
   that should be installed as suid root.
*/
int
tveng_run_zapping_setup_fb(tveng_device_info * info)
{
  char * argv[10]; /* The command line passed to zapping_setup_fb */
  pid_t pid; /* New child's pid as returned by fork() */
  int status; /* zapping_setup_fb returned status */
  int i=0;
  int verbosity = info->private->zapping_setup_fb_verbosity;
  char buffer[256]; /* A temporary buffer */

  TVLOCK;

  /* Executes zapping_setup_fb with the given arguments */
  argv[i++] = "zapping_setup_fb";
  argv[i++] = "--device";
  argv[i++] = info -> file_name;

  /* Clip verbosity to valid values */
  if (verbosity < 0)
    verbosity = 0;
  else if (verbosity > 2)
    verbosity = 2;
  for (; verbosity > 0; verbosity --)
    argv[i++] = "--verbose";
  if (info->private->bpp != -1)
    {
      snprintf(buffer, 255, "%d", info->private->bpp);
      buffer[255] = 0;
      argv[i++] = "--bpp";
      argv[i++] = buffer;
    }
  argv[i] = NULL;
  
  pid = fork();

  if (pid == -1)
    {
      info->tveng_errno = errno;
      t_error("fork()", info);
      UNTVLOCK;
      return -1;
    }

  if (!pid) /* New child process */
    {
      execvp("zapping_setup_fb", argv);

      /* This shouldn't be reached if everything suceeds */
      perror("execvp(zapping_setup_fb)");

      _exit(2); /* zapping setup_fb on error returns 1 */
    }

  /* This statement is only reached by the parent */
  if (waitpid(pid, &status, 0) == -1)
    {
      info->tveng_errno = errno;
      t_error("waitpid", info);
      UNTVLOCK;
      return -1;
    }

  if (! WIFEXITED(status)) /* zapping_setup_fb exited abnormally */
    {
      info->tveng_errno = errno;
      t_error_msg("WIFEXITED(status)", 
		  "zapping_setup_fb exited abnormally, check stderr",
		  info);
      UNTVLOCK;
      return -1;
    }

  switch (WEXITSTATUS(status))
    {
    case 1:
      info -> tveng_errno = -1;
      t_error_msg("case 1",
		  "zapping_setup_fb failed to set up the video",
		  info);
      UNTVLOCK;
      return -1;
    case 2:
      info -> tveng_errno = -1;
      t_error_msg("case 2",
		  _("Couldn't locate zapping_setup_fb, check your install"),
		  info);
      UNTVLOCK;
      return -1;
    default:
      break; /* Exit code == 0, success setting up the framebuffer */
    }
  UNTVLOCK;
  return 0; /* Success */
}

/* 
   This is a convenience function, it returns the real screen depth in
   PRIVATE->BPP (bits per pixel). This one is quite important for 24 and 32 bit
   modes, since the default X visual may be 24 bit and the real screen
   depth 32, thus an expensive RGB -> RGBA conversion must be
   performed for each frame.
   private->display: the private->display we want to know its real depth (can be
   accessed through gdk_private->display)
*/
int
tveng_get_display_depth(tveng_device_info * info)
{
  /* This routines are taken form xawtv, i don't understand them very
     well, but they seem to work OK */
  XVisualInfo * visual_info, template;
  XPixmapFormatValues * pf;
  Display * dpy = info->private->display;
  int found, v, i, n;
  int bpp = 0;

  TVLOCK;

  if (info->private->bpp != -1)
    RETURN_UNTVLOCK(info->private->bpp);

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
    UNTVLOCK;
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
    UNTVLOCK;
    return 0;
  }

  XFree(visual_info);
  XFree(pf);

  UNTVLOCK;
  return bpp;
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

  TVLOCK;

  if (info->private->dword_align)
    {
      info->window.x = (info->window.x+3) & ~3;
      info->window.width = (info->window.width+3) & ~3;
    }
  if (info->window.height < info->caps.minheight)
    info->window.height = info->caps.minheight;
  if (info->window.height > info->caps.maxheight)
    info->window.height = info->caps.maxheight;
  if (info->window.width < info->caps.minwidth)
    info->window.width = info->caps.minwidth;
  if (info->window.width > info->caps.maxwidth)
    info->window.width = info->caps.maxwidth;

  if (info->private->module.set_preview_window)
    RETURN_UNTVLOCK(info->private->module.set_preview_window(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
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

  TVLOCK;

  if (info->private->module.get_preview_window)
    RETURN_UNTVLOCK(info->private->module.get_preview_window(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
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
  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->private->module.set_preview)
    RETURN_UNTVLOCK(info->private->module.set_preview(on, info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
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
  info->private->zapping_setup_fb_verbosity = level;
}

/* Sets the chromakey */
void tveng_set_chromakey(int r, int g, int b, tveng_device_info *info)
{
  info->private->chromakey = ((r&0xff)<<16) + ((g&0xff)<<8) + (b&0xff);
}

/* Sets the dword align flag */
void tveng_set_dword_align(int dword_align, tveng_device_info *info)
{
  info->private->dword_align = dword_align;
}

/* Returns the current verbosity value passed to zapping_setup_fb */
int
tveng_get_zapping_setup_fb_verbosity(tveng_device_info * info)
{
  return (info->private->zapping_setup_fb_verbosity);
}

/* 
   Sets up everything and starts previewing.
   Just call this function to start previewing, it takes care of
   (mostly) everything.
   Returns -1 on error.
*/
int
tveng_start_previewing (tveng_device_info * info, int change_mode)
{
#ifndef DISABLE_X_EXTENSIONS
  int event_base, error_base, major_version, minor_version;
  XF86VidModeModeInfo ** modesinfo;
  int modecount;
  int i;
  int chosen_mode=0; /* The video mode we will use for fullscreen */
  int distance=-1; /* Distance of the best video mode to a valid size */
  int temp; /* Temporal value */
  int bigger=0; /* Allow choosing bigger screen depths */
  static int info_printed = 0; /* do not print the modes every time */
#endif

  t_assert(info != NULL);
  t_assert(info->current_controller != TVENG_CONTROLLER_NONE);

  TVLOCK;

  if (info->current_controller == TVENG_CONTROLLER_XV)
    change_mode = 0; /* not needed */
  else
    if (!tveng_detect_XF86DGA(info))
      {
	info->tveng_errno = -1;
	t_error_msg("tveng_detect_XF86DGA",
		    "No DGA present, make sure you enable it in"
		    " /etc/X11/XF86Config.",
		    info);
	UNTVLOCK;
	return -1;
      }

  /* special code, used only inside tveng, means remember from last
     time */
  if (change_mode == -1)
    change_mode = info->private->change_mode;
  else
    info->private->change_mode = change_mode;

#ifndef DISABLE_X_EXTENSIONS
  info -> private->xf86vm_enabled = 1;

  if (!XF86VidModeQueryExtension(info->private->display, &event_base,
				 &error_base))
    {
      info->tveng_errno = -1;
      t_error_msg("XF86VidModeQueryExtension",
		  "No vidmode extension supported", info);
      info->private->xf86vm_enabled = 0;
    }

  if ((info->private->xf86vm_enabled) &&
      (!XF86VidModeQueryVersion(info->private->display, &major_version,
				&minor_version)))
    {
      info->tveng_errno = -1;
      t_error_msg("XF86VidModeQueryVersion",
		  "No vidmode extension supported", info);
      info->private->xf86vm_enabled = 0;      
    }

  if ((info->private->xf86vm_enabled) &&
      (!XF86VidModeGetAllModeLines(info->private->display, 
				   DefaultScreen(info->private->display),
				   &modecount, &modesinfo)))
    {
      info->tveng_errno = -1;
      t_error_msg("XF86VidModeGetAllModeLines",
		  "No vidmode extension supported", info);
      info->private->xf86vm_enabled = 0;
    }

  if (info -> private->xf86vm_enabled)
    {
      if ((info->debug_level > 0) && (!info_printed))
	{
	  fprintf(stderr, "XF86VidMode info:\n");
	  fprintf(stderr, "  - event and error base  : %d, %d\n", event_base,
		  error_base);
	  fprintf(stderr, "  - XF86VidMode version   : %d.%d\n",
		  major_version, minor_version);
	  fprintf(stderr, "  - Available video modes : %d\n",
		  modecount);
	}

    loop_point:
      for (i = 0; i<modecount; i++)
	{
	  if (info->debug_level > 0)
	    if ((!bigger) && (!info_printed)) /* print only once */
	      fprintf(stderr, "      %d) %dx%d @ %d Hz\n", i, (int)
		      modesinfo[i]->hdisplay, (int) modesinfo[i]->vdisplay,
		      (int) modesinfo[i]->dotclock);

	  /* Check whether this is a good value */
	  temp = ((int)modesinfo[i]->hdisplay) - info->caps.maxwidth;
	  temp *= temp;
	  temp += (((int)modesinfo[i]->vdisplay) - info->caps.maxheight) *
	    (((int)modesinfo[i]->vdisplay) - info->caps.maxheight);
	  if  (((modesinfo[i]->hdisplay < info->caps.maxwidth) &&
		(modesinfo[i]->vdisplay < info->caps.maxheight))
	       || (bigger))
	    {
	      if ((distance == -1) || (temp < distance))
		{
		  /* This heuristic is somewhat krusty, but it should mostly
		     work */
		chosen_mode = i;
		distance = temp;
		}
	    }
	}

      if (distance == -1) /* No adequate size smaller than the given
			     one, get the nearest bigger one */
	{
	  bigger = 1;
	  goto loop_point;
	}
      
      if ((info->debug_level > 0) && (!info_printed))
	{
	  info_printed = 1;
	  fprintf(stderr, "      Mode # %d chosen\n", chosen_mode);
	}
      
      /* If the chosen mode isn't the actual one, choose it, but
	 place the viewport correctly first */
      /* get the current viewport pos for restoring later */
      XF86VidModeGetViewPort(info->private->display,
			     DefaultScreen(info->private->display),
			     &(info->private->save_x), &(info->private->save_y));

      if (change_mode == 0)
	chosen_mode = 0;

      if (chosen_mode == 0)
	{
	  info->private->restore_mode = 0;
	  XF86VidModeSetViewPort(info->private->display,
				 DefaultScreen(info->private->display), 0, 0);
	}
      else
	{
	  info->private->restore_mode = 1;
	  info->private->modeinfo.privsize = 0;
	  XF86VidModeGetModeLine(info->private->display,
				 DefaultScreen(info->private->display),
				 &(info->private->modeinfo.dotclock),
				 /* this is kinda broken, but it
				    will probably always work */
				 (XF86VidModeModeLine*)
				 &(info->private->modeinfo.hdisplay));

	  if (!XF86VidModeSwitchToMode(info->private->display,
				       DefaultScreen(info->private->display),
				       modesinfo[chosen_mode]))
	    info -> private->xf86vm_enabled = 0;

	  /* Place the viewport again */
	  if (info -> private->xf86vm_enabled)
	    XF86VidModeSetViewPort(info->private->display,
				   DefaultScreen(info->private->display), 0, 0);
	}

      for (i=0; i<modecount; i++)
	if (modesinfo[i]->privsize > 0)
	  XFree(modesinfo[i]->private);
      
      XFree(modesinfo);
    }
  else /* info -> private->xf86vm_enabled */
    {
      fprintf(stderr, "XF86VidMode not enabled: %s\n", info -> error);
    }
#endif /* DISABLE_X_EXTENSIONS */

  if (info->private->module.start_previewing)
    RETURN_UNTVLOCK(info->private->module.start_previewing(info));

  TVUNSUPPORTED;
  UNTVLOCK;
  return -1;
}

/*
  Stops the fullscreen mode. Returns -1 on error
*/
int
tveng_stop_previewing(tveng_device_info * info)
{
  int return_code = 0;

  t_assert(info != NULL);

  TVLOCK;

  if (info->private->module.stop_previewing)
    return_code = info->private->module.stop_previewing(info);

#ifndef DISABLE_X_EXTENSIONS
  if (info->private->xf86vm_enabled)
    {
      if (info->private->restore_mode)
	{
	  XF86VidModeSwitchToMode(info->private->display,
				  DefaultScreen(info->private->display),
				  &(info->private->modeinfo));
	  if (info->private->modeinfo.privsize > 0)
	    XFree(info->private->modeinfo.private);
	}
      XF86VidModeSetViewPort(info->private->display,
			     DefaultScreen(info->private->display),
			     info->private->save_x, info->private->save_y);
    }
#endif

  UNTVLOCK;

  return return_code;
}

/*
  Sets up everything and starts previewing in a window. It doesn't do
  many of the things tveng_start_previewing does, it's mostly just a
  wrapper around tveng_set_preview_on. Returns -1 on error
  The window must be specified from before calling this function (with
  tveng_set_preview_window), and overlaying must be available.
*/
int
tveng_start_window (tveng_device_info * info)
{
  t_assert(info != NULL);

  TVLOCK;

  tveng_stop_everything(info);

  t_assert(info -> current_mode == TVENG_NO_CAPTURE);

  if (!tveng_detect_preview(info))
    /* We shouldn't be reaching this if the app is well programmed */
    t_assert_not_reached();

  tveng_set_preview_window(info);

  if (tveng_set_preview_on(info) == -1)
    RETURN_UNTVLOCK(-1);

  info->current_mode = TVENG_CAPTURE_WINDOW;

  UNTVLOCK;
  return 0;
}

/*
  Stops the window mode. Returns -1 on error
*/
int
tveng_stop_window (tveng_device_info * info)
{
  t_assert(info != NULL);

  TVLOCK;

  if (info -> current_mode == TVENG_NO_CAPTURE)
    {
      fprintf(stderr, 
	      "Warning: trying to stop window with no capture active\n");
      UNTVLOCK;
      return 0; /* Nothing to be done */
    }

  t_assert(info->current_mode == TVENG_CAPTURE_WINDOW);

  /* No error checking */
  tveng_set_preview_off(info);

  info -> current_mode = TVENG_NO_CAPTURE;

  UNTVLOCK;
  return 0; /* Success */
}

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
enum tveng_capture_mode tveng_stop_everything (tveng_device_info *
					       info)
{
  enum tveng_capture_mode returned_mode;

  t_assert(info != NULL);

  TVLOCK;

  returned_mode = info->current_mode;
  switch (info->current_mode)
    {
    case TVENG_CAPTURE_READ:
      tveng_stop_capturing(info);
      break;
    case TVENG_CAPTURE_PREVIEW:
      tveng_stop_previewing(info);
      break;
    case TVENG_CAPTURE_WINDOW:
      tveng_stop_window(info);
      break;
    default:
      break;
    };

  t_assert(info->current_mode == TVENG_NO_CAPTURE);

  UNTVLOCK;

  return returned_mode;
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

  switch (mode)
    {
    case TVENG_CAPTURE_READ:
      if (tveng_start_capturing(info) == -1)
	RETURN_UNTVLOCK(-1);
      break;
    case TVENG_CAPTURE_PREVIEW:
      if (tveng_start_previewing(info, -1) == -1)
	RETURN_UNTVLOCK(-1);
      break;
    case TVENG_CAPTURE_WINDOW:
      if (tveng_start_window(info) == -1)
	RETURN_UNTVLOCK(-1);
      break;
    default:
      break;
    }

  UNTVLOCK;
  return 0; /* Success */
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

  info->private->disable_xv = disabled;
}

void tveng_assume_yvu(int assume, tveng_device_info * info)
{
  t_assert(info != NULL);

  info->private->assume_yvu = assume;
}

#ifdef USE_XV
void tveng_set_xv_port(XvPortID port, tveng_device_info * info)
{
  XvAttribute *at;
  int attributes, i;
  Display *dpy;
  struct tveng_control control;

  TVLOCK;

  info->private->port = port;
  dpy = info->private->display;
  info->private->filter = info->private->colorkey =
    info->private->double_buffer = None;

  /* Add the controls in this port to the struct of controls */
  at = XvQueryPortAttributes(dpy, port, &attributes);
  for (i=0; i<attributes; i++)
    {
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

      if (!strcmp("XV_FILTER", at[i].name))
	  {
	    info->private->filter = XInternAtom(dpy, "XV_FILTER",
						False);
	    control.id = (int)info->private->filter;
	    snprintf(control.name, 32, _("Filter"));
	    control.min = at[i].min_value;
	    control.max = at[i].max_value;
	    control.type = TVENG_CONTROL_CHECKBOX;
	    control.data = NULL;
	    control.controller = TVENG_CONTROLLER_MOTHER;
	    if (p_tveng_append_control(&control, info) == -1)
	      {
		UNTVLOCK;
		return;
	      }
	  }

      else if (!strcmp("XV_DOUBLE_BUFFER", at[i].name))
	  {
	    info->private->double_buffer = XInternAtom(dpy, "XV_DOUBLE_BUFFER",
						       False);
	    control.id = (int)info->private->double_buffer;
	    snprintf(control.name, 32, _("Filter"));
	    control.min = at[i].min_value;
	    control.max = at[i].max_value;
	    control.type = TVENG_CONTROL_CHECKBOX;
	    control.data = NULL;
	    control.controller = TVENG_CONTROLLER_MOTHER;
	    if (p_tveng_append_control(&control, info) == -1)
	      {
		UNTVLOCK;
		return;
	      }
	  }

      else if (!strcmp("XV_COLORKEY", at[i].name))
	  {
	    info->private->colorkey = XInternAtom(dpy, "XV_COLORKEY",
						False);
	    control.id = (int)info->private->colorkey;
	    snprintf(control.name, 32, _("Colorkey"));
	    control.min = at[i].min_value;
	    control.max = at[i].max_value;
	    control.type = TVENG_CONTROL_COLOR;
	    control.data = NULL;
	    control.controller = TVENG_CONTROLLER_MOTHER;
	    if (p_tveng_append_control(&control, info) == -1)
	      {
		UNTVLOCK;
		return;
	      }
	  }
    }

  tveng_update_controls(info);

  UNTVLOCK;
}

void tveng_unset_xv_port(tveng_device_info * info)
{
  info->private->port = None;
  info->private->filter = info->private->colorkey = None;
}
#endif

int
tveng_ov511_get_button_state (tveng_device_info *info)
{
  t_assert(info != NULL);

  if (info->current_controller == TVENG_CONTROLLER_NONE)
    return -1;

  TVLOCK;

  if (info->private->module.ov511_get_button_state)
    RETURN_UNTVLOCK(info->private->module.ov511_get_button_state(info));

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
