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

/* This undef's are to avoid a couple of header warnings */
#undef WNOHANG
#undef WUNTRACED
#include "tveng.h"
#include "tveng2.h"
#include "videodev2.h" /* the V4L2 definitions */

/*
  Return fd for the device file opened. Checks if the device is a
  valid video device. -1 on error.
  Flags will be used for open()'ing the file 
*/
int tveng2_open_device_file(int flags, tveng_device_info * info)
{
  struct v4l2_capability caps;
  struct v4l2_framebuffer fb;

  t_assert(info != NULL);
  t_assert(info->file_name != NULL);

  info -> fd = open(info -> file_name, flags);
  if (info -> fd < 0)
    {
      info->tveng_errno = errno;
      t_error("open()", info);
      return -1;
    }

  /* We check the capabilities of this video device */
  memset(&caps, 0, sizeof(struct v4l2_capability));
  memset(&fb, 0, sizeof(struct v4l2_framebuffer));

  if (ioctl(info -> fd, VIDIOC_QUERYCAP, &caps))
    {
      info -> tveng_errno = errno;
      t_error("VIDIOC_QUERYCAP", info);
      close(info -> fd);
      return -1;
    }

  /* Check if this device is convenient for us */
  if (caps.type != V4L2_TYPE_CAPTURE)
    {
      info->tveng_errno = -1;
      snprintf(info->error, 256, 
	       _("%s doesn't look like a valid capture device"), info
	       -> file_name);
      close(info -> fd);
      return -1;
    }

  /* Check if we can select() and mmap() this device */
  if (!(caps.flags & V4L2_FLAG_STREAMING))
    {
      info -> tveng_errno = -1;
      snprintf(info->error, 256,
	       _("Sorry, but \"%s\" cannot do streaming"),
	       info -> file_name);
      close(info -> fd);
      return -1;
    }

  if (!(info->caps.flags & V4L2_FLAG_SELECT))
    {
      info->tveng_errno = -1;
      snprintf(info->error, 256, 
	       _("Sorry, but \"%s\" cannot do select() on file descriptors"),
	       info -> file_name);
      close(info -> fd);
      return -1;
    }

  if (!(info->caps.flags & V4L2_FLAG_TUNER))
    {
      info -> tveng_errno = -1;
      snprintf(info->error, 255, 
	       _("Sorry, but \"%s\" has no tuner"),
	       info -> file_name);
      close(info -> fd);
      return -1;
    }

  /* Copy capability info */
  snprintf(info->caps.name, 32, caps.name);
  info->caps.channels = caps.inputs;
  info->caps.audios = caps.audios;
  info->caps.maxwidth = caps.maxwidth;
  info->caps.minwidth = caps.minwidth;
  info->caps.maxheight = caps.maxheight;
  info->caps.minheight = caps.minheight;
  info->caps.flags = 0;

  info->caps.flags |= TVENG_CAPTURE; /* This has been tested before */

  if (caps.flags & V4L2_FLAG_TUNER)
    info->caps.flags |= TVENG_CAPS_TUNER;
  if (caps.flags & V4L2_FLAG_DATA_SERVICE)
    info->caps.flags |= TVENG_CAPS_TELETEXT;
  if (caps.flags & V4L2_FLAG_MONOCHROME)
    info->caps.flags |= TVENG_CAPS_MONOCHROME;

  if (caps.flags & V4L2_FLAG_PREVIEW)
    {
      info->caps.flags |= TVENG_CAPS_OVERLAY;
      /* Collect more info about the overlay mode */
      if (ioctl(info->fd, VIDIOC_G_FBUF, &fb))
	{
	  if (fb.flags & V4L2_FBUF_CAP_CHROMAKEY)
	    info->caps.flags |= TVENG_CAPS_CHROMAKEY;
	  if (fb.flags & V4L2_FBUF_CAP_CLIPPING)
	    info->caps.flags |= TVENG_CAPS_CLIPPING;
	  if (!(fb.flags & V4L2_FBUF_CAP_EXTERNOVERLAY))
	    info->caps.flags |= TVENG_CAPS_FRAMERAM;
	  if ((fb.flags & V4L2_FBUF_CAP_SCALEUP) ||
	      (fb.flags & V4L2_FBUF_CAP_SCALEDOWN))
	    info->caps.flags |= TVENG_CAPS_SCALES;
	}
    }

  info -> current_controller = TVENG_CONTROLLER_V4L2;
  
  /* Everything seems to be OK with this device */
  return (info -> fd);
}

/*
  Associates the given tveng_device_info with the given video
  device. On error it returns -1 and sets info->tveng_errno, info->error to
  the correct values.
  device_file: The file used to access the video device (usually
  /dev/video)
  attach_mode: Specifies the mode to open the device file
  depth: The color depth the capture will be in, -1 means let tveng
  decide based on the current display depth.
  info: The structure to be associated with the device
*/
int tveng2_attach_device(const char* device_file,
			 enum tveng_attach_mode attach_mode,
			 tveng_device_info * info)
{
  int error;
  t_assert(device_file != NULL);
  t_assert(info != NULL);

  if (info -> fd) /* If the device is already attached, detach it */
    tveng_close_device(info);

  info -> file_name = strdup(device_file);
  if (!(info -> file_name))
    {
      info -> tveng_errno = errno;
      t_error("strdup()", info);
      return -1;
    }

  switch (attach_mode)
    {
    case TVENG_ATTACH_CONTROL:
      info -> fd = tveng2_open_device_file(O_NOIO, info);
    case TVENG_ATTACH_READ:
      info -> fd = tveng2_open_device_file(O_RDWR, info);
      break;
    default:
      t_error_msg("switch()", _("Unknown attach mode for the device"),
		  info);
      return -1;
    };

  /*
    Errors (if any) are already aknowledged when we reach this point,
    so we don't show them again
  */
  if (info -> fd < 0)
    return -1;
  
  info -> attach_mode = attach_mode;
  /* Current capture mode is no capture at all */
  info -> current_mode = TVENG_NO_CAPTURE;

  /* We have a valid device, get some info about it */
  /* Fill in inputs */
  info->inputs = NULL;
  info->cur_input = 0;
  error = tveng2_get_inputs(info);
  if (error < 1)
    {
      if (error == 0) /* No inputs */
      {
	info->tveng_errno = -1;
	snprintf(info->error, 256, _("No inputs for this device"));
	fprintf(stderr, "%s\n", info->error);
      }
      tveng2_close_device(info);
      return -1;
    }

  /* Make an ioctl test and switch to the first input */
  if (tveng2_set_input(&(info->inputs[0]), info) == -1)
    {
      tveng2_close_device(info);
      return -1;
    }

  /* Fill in standards */
  info->standards = NULL;
  info->cur_standard = 0;
  error = tveng2_get_standards(info);
  if (error < 1)
    {
      if (error == 0) /* No standards */
      {
	info->tveng_errno = -1;
	snprintf(info->error, 256, _("No standards for this device"));
	fprintf(stderr, "%s\n", info->error);
      }
      tveng2_close_device(info);
      return -1;
    }

  /* make another ioctl test, switch to first standard */
  if (tveng2_set_standard(&(info->standards[0]), info) == -1)
    {
      tveng2_close_device(info);
      return -1;
    }

  /* Query present controls */
  info->num_controls = 0;
  info->controls = NULL;
  error = tveng2_get_controls(info);
  if (error == -1)
      return -1;

  /* Set up the palette according to the one present in the system */
  error = tveng_get_display_depth(info);

  if (error == -1)
    {
      tveng2_close_device(info);
      return -1;
    }

  switch(error)
    {
    case 16:
      info->format.pixformat = TVENG_PIX_RGB565;
      break;
    case 24:
      info->format.pixformat = TVENG_PIX_BGR24;
      break;
    case 32:
      info->format.pixformat = TVENG_PIX_BGR32;
      break;
    default:
      info -> tveng_errno = -1;
      t_error_msg("switch()", 
		  _("Cannot find appropiate palette for current display"),
		  info);
      tveng2_close_device(info);
      return -1;
    }

  /* Set our desired size, make it halfway */
  info -> format.width = (info->caps.minwidth + info->caps.maxwidth)/2;
  info -> format.height = (info->caps.minheight +
			   info->caps.maxheight)/2;
  if (tveng2_set_capture_format(info) == -1)
    {
      tveng2_close_device(info);
      return -1;
    }

  return info -> fd;
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
tveng1_describe_controller(char ** short_str, char ** long_str,
			   tveng_device_info * info)
{
  t_assert(info != NULL);
  if (short_str)
    *short_str = "V4L2";
  if (long_str)
    *long_str = "Video4Linux 2";
}

/* Closes a device opened with tveng_init_device */
void tveng1_close_device(tveng_device_info * info)
{
  switch (info->current_mode)
    {
    case TVENG_CAPTURE_READ:
      tveng2_stop_capturing(info);
      break;
    case TVENG_CAPTURE_PREVIEW:
      tveng2_stop_previewing(info);
      break;
    default:
      break;
    };

  close(info -> fd);
  info -> fd = 0;
  info -> current_controller = TVENG_CONTROLLER_NONE;

  if (info -> file_name)
    free(info -> file_name);
  if (info -> inputs)
    free(info -> inputs);
  if (info -> standards)
    free(info -> standards);
  if (info -> controls)
    free(info -> controls);
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
int tveng2_get_inputs(tveng2_device_info * info)
{
  int err = 0;
  int i;

  for (i = 0; err == 0; i++)
    {
      info->inputs = realloc(info->inputs, (i+1)*sizeof(tveng2_input));
      info->inputs[i].input.index = i;
      info->inputs[i].info = info;
      err = ioctl(info -> fd, VIDIOC_ENUMINPUT, &(info->inputs[i].input));
    }
  i--; /* Get rid of last (invalid) entry */
  info->inputs = realloc(info->inputs, (i)*sizeof(tveng2_input));
  return (info->num_inputs = i);
}

/*
  Sets the current input for the capture
*/
int tveng2_set_input(int input, tveng2_device_info * info)
{
  int was_mute = -1;

  g_assert(input < info->num_inputs);

  if (tveng2_get_mute(&was_mute, info))
    was_mute = -1;
  
  if (tveng2_stop_capturing(info) == -1)
    return -1;

  if (was_mute == 0)
    tveng2_set_mute(1, info);

  if (ioctl(info->fd, VIDIOC_S_INPUT, &(info->inputs[input].input)))
    {printf("Setting stardard failed\n"); return -1;}
  
  if (tveng2_start_capturing(info) == NULL)
    return -1;

  if (was_mute == 0)
    tveng2_set_mute(0, info);

  info->cur_input = input;

  return 0;
}

/*
  Sets the input named name as the active input. -1 on error.
*/
int
tveng2_set_input_by_name(gchar * name, tveng2_device_info * info)
{
  int i;
  for (i = 0; i < info->num_inputs; i++)
    if (!strcasecmp(info->inputs[i].input.name, name))
      return tveng2_set_input(i, info);

  return -1; /* String not found */
}

/*
  Returns the number of standards in the given device and fills in info
  with the correct info, allocating memory as needed
*/
int tveng2_get_standards(tveng2_device_info * info)
{
  int err = 0;
  int i;

  for (i = 0; err == 0; i++)
    {
      info->standards = realloc(info->standards, 
				(i+1)*sizeof(tveng2_enumstd));
      info->standards[i].std.index = i;
      info->standards[i].info = info;
      err = ioctl(info -> fd, VIDIOC_ENUMSTD, &(info->standards[i].std));
    }

  i--; /* Get rid of last (invalid) entry */
  info->standards = realloc(info->standards, (i)*sizeof(tveng2_enumstd));
  return (info->num_standards = i);
}

/*
  Sets the current standard for the capture. standard is the name for
  the desired standard. updates cur_standard
*/
int tveng2_set_standard(gchar * name, tveng2_device_info * info)
{
  int i = 0;

  /* Query the list of standards to get the standard we are to set */
  for (; i < info->num_standards; i++)
    if (!strcasecmp(info->standards[i].std.std.name, name))
      break; /* Stop the loop */

  if (i == info->num_standards)
    return -1; /* The given name doesn't seem to be valid */

  if (tveng2_stop_capturing(info) == -1)
    return -1;

  if (ioctl(info->fd, VIDIOC_S_STD, &(info->standards[i].std.std)))
    {
      perror("Setting stardard failed");
      return -1;
    }
  
  if (tveng2_start_capturing(info) == NULL)
    {
      perror("Cannot restart capture\n");
      return -1;
    }

  /* Update the current standard */
  tveng2_update_standard(info);

  if (!strcasecmp(info->cur_standard.name, name))
    return 0;

  printf("Standard re-checking failed\n");
  return -1;
}

/*
  Updates the value stored in cur_input. Returns -1 on error
*/
int tveng2_update_input(tveng2_device_info * info)
{
  if (ioctl(info->fd, VIDIOC_G_INPUT, &(info->cur_input)) != 0)
    return -1;
  return 0;
}

/*
  Updates the value stored in cur_standard. Returns -1 on error
*/
int tveng2_update_standard(tveng2_device_info * info)
{
  info -> cur_standard_index = -1;

  if (ioctl(info->fd, VIDIOC_G_STD, &(info->cur_standard)) != 0)
    return -1;

  /* Fill cur_standard_index with its current value */
  for (info -> cur_standard_index = 0; 
       info -> cur_standard_index < info -> num_standards;)
    {
      if (!strcasecmp(info -> cur_standard.name, 
		      info -> standards[info -> 
				       cur_standard_index].std.std.name))
	return 0; /* We have found the standard, exit */
      info -> cur_standard_index ++; /* This isn't the standard */
    }

  g_assert_not_reached(); /* This shouldn't be reached if
			     VIDIOC_ENUMSTD works OK*/
  
  info -> cur_standard_index = -1;

  printf(_("Error: ENUMSTD doesn't completely work\n"));

  return -1; /* We have set an standard that didn't appear when we
		ENUMSTD ? Really weird*/
}

/*
  Test if a given control is supported by the driver and fills in the
  structure qc with some info about this control. Returns EINVAL in
  case of failure. Return value of this function should be considered
  valid only if it is 0. Usually tveng2_set_control will be used
  instead of this function directly.
*/
int tveng2_test_control(int control_id, struct v4l2_queryctrl * qc,
		       tveng2_device_info * info)
{
  qc -> id = control_id;
  return (ioctl(info->fd, VIDIOC_QUERYCTRL, qc));
}

/*
  Set the value for specific controls. Some of this functions have
  wrappers, such as tveng2_set_mute(). Returns 1 in case the value is
  lower than the allowed minimum, and 2 in case the value is higher
  than the allowed maximum. The value is set to ne nearest valid value
  and we set it. returns 0 on success
*/
int tveng2_set_control(int control_id, int value,
		      tveng2_device_info * info)
{
  struct v4l2_queryctrl qc;
  struct v4l2_control c;
  int bounds_flag = 0;
  int err;

  /* Check if the control is available */
  if (tveng2_test_control(control_id, &qc, info) != 0)
    return -1;

  if (value > qc.maximum)
    {
      bounds_flag = 2;
      value = qc.maximum;
    }
  if (value < qc.minimum)
    {
      bounds_flag = 1;
      value = qc.minimum;
    }

  c.id = control_id;
  c.value = value;

  err = ioctl(info -> fd, VIDIOC_S_CTRL, &c);
  if (err == 0)
    return (bounds_flag);
  else
    return err;
}

/*
  Gets the value of an specific control. Some of this functions have
  wrappers, such as tveng2_get_mute(). The value is stored in the
  address value points to, if there is no error. In case of error, the
  function returns -1, and value is undefined
*/
int tveng2_get_control(int control_id, int * value,
		      tveng2_device_info * info)
{
  struct v4l2_queryctrl qc;
  struct v4l2_control c;

  /* Check if the control is available */
  if (tveng2_test_control(control_id, &qc, info) != 0)
    return -1;

  c.id = control_id;

  if (ioctl(info -> fd, VIDIOC_G_CTRL, &c) != 0)
    return -1;
  
  *value = c.value;

  return 0;
}

/*
  Tunes a video input (if is hasn't a tuner this function fails with
  0) to the specified frequence (in kHz). If the given frequence is
  higher than rangehigh or lower than rangelow it is clipped.
  FIXME: What does afc in v4l2_tuner mean?
*/
int tveng2_tune_input(int input, __u32 _freq, tveng2_device_info * info)
{
  struct v4l2_tuner tuner_info;
  unsigned int freq; /* real frequence passed to v4l2 */

  g_assert(input < info -> num_inputs);
  
  /* Check if there was a tuner in this input */
  if (!(info -> inputs[input].input.type & V4L2_INPUT_TYPE_TUNER))
    return 0;
  
  /* Get more info about this tuner */
  tuner_info.input = input;
  if (ioctl(info -> fd, VIDIOC_G_TUNER, &tuner_info) != 0)
    return -1;
  
  if (tuner_info.capability & V4L2_TUNER_CAP_LOW)
    freq = _freq / 0.0625;
  else
    freq = _freq / 62.5;

  if (freq > tuner_info.rangehigh)
    freq = tuner_info.rangehigh;
  if (freq < tuner_info.rangelow)
    freq = tuner_info.rangelow;
  
  /* OK, everything is set up, try to tune it */
  if (ioctl(info -> fd, VIDIOC_S_FREQ, &freq) != 0)
      return -1;

  /* It is tuned */

  return 1; /* Success */
}

/*
  Stores in freq the frequence the current input is tuned on. Returns
  -1 on error.
*/
int tveng2_get_tune(__u32 * freq, tveng2_device_info * info)
{
  if (ioctl(info->fd, VIDIOC_G_FREQ, freq) != 0)
    return -1;

  return 0;
}

/* Prints the name of the current pixformat and a newline */
void print_format(__u32 format)
{
  printf("%c%c%c%c\n",
	 (format & 0xff),
	 (format & 0xff00) >> 8,
	 (format & 0xff0000) >> 16,
	 (format & 0xff000000) >> 24);
}

/* -1 if failed */
int
tveng2_get_capture_format(tveng2_device_info * info)
{
  /* Get current settings */
  info -> pix_format.type = V4L2_BUF_TYPE_CAPTURE;

  if (ioctl(info -> fd, VIDIOC_G_FMT, &(info->pix_format)))
    {
      perror("Cannot get format");
      return -1;
    }
  return 0;
}

/* -1 if failed. Sets the pixformat and fills in info -> pix_format
   with the correct values  */
int
tveng2_set_capture_format(__u32 video_format, tveng2_device_info * info)
{
  if (tveng2_get_capture_format(info) == -1)
    return -1;

  info -> pix_format.type = V4L2_BUF_TYPE_CAPTURE;
  info -> pix_format.fmt.pix.pixelformat = video_format;
  if (info -> interlaced)
    info -> pix_format.fmt.pix.flags = V4L2_FMT_FLAG_INTERLACED;
  else
    info -> pix_format.fmt.pix.flags = 0;

  if (ioctl(info->fd, VIDIOC_S_FMT, &(info -> pix_format)) != 0)
    {
      perror("Cannot set format\n");
      return -1;
    }

  if (tveng2_get_capture_format(info) == -1)
    return -1;

  if (info -> pix_format.fmt.pix.pixelformat != video_format)
    return -1;

  return 0; /* Success setting the format */
}

/*
  Start capturing frames to a memory buffer. Returns NULL on error,
  the address of the first mmap'ed buffer otherwise
  We should specify the number of different buffers to allocate using
  mmap().
*/
gpointer tveng2_start_capturing(tveng2_device_info * info)
{
  struct v4l2_requestbuffers rb;
  int error;
  int i;
  int tmp_var = 0;
  gboolean video_format_set = FALSE;

  /* We are drawing on a RGB visual (even if we don't use GdkRgb
     functions directly) */
  GdkVisual * rgb_visual = gdk_rgb_get_visual();

  /* Sorry to use this, but we need to access private fields */
  GdkImagePrivate * image_private;

  g_assert(info -> current_mode == TVENG2_NO_CAPTURE);

  i = 0;
  error = TRUE;

  if (rgb_visual -> depth == 15)
    {
      if (tveng2_set_capture_format(V4L2_PIX_FMT_RGB555, info) != -1)
	video_format_set = TRUE;
    }
  else if (rgb_visual->depth == 16)
    {
      if (tveng2_set_capture_format(V4L2_PIX_FMT_RGB565, info) != 1)
	video_format_set = TRUE;
    }
  else if (rgb_visual->depth == 24)
    {
      if (tveng2_set_capture_format(V4L2_PIX_FMT_RGB24, info) != -1)
	video_format_set = TRUE;

      if (!video_format_set)
	if (tveng2_set_capture_format(V4L2_PIX_FMT_BGR32, info) != -1)
	  video_format_set = TRUE;
    }
  else if (rgb_visual->depth == 32)
    {
      if (tveng2_set_capture_format(V4L2_PIX_FMT_RGB32, info) != -1)
	video_format_set = TRUE;

      if (!video_format_set)
	if (tveng2_set_capture_format(V4L2_PIX_FMT_BGR32, info) != -1)
	  video_format_set = TRUE;
    }

  if (!video_format_set)
    {
      ShowBox(_("Sorry, but cannot start capturing in an adequate format\n"),
	      GNOME_MESSAGE_BOX_ERROR);
      return NULL;
    }

  /* Get current settings */
  if (tveng2_get_capture_format(info))
    return NULL;

  rb.count = info -> num_desired_buffers;
  rb.type = V4L2_BUF_TYPE_CAPTURE;
  error = ioctl(info->fd, VIDIOC_REQBUFS, &rb);

  if ((error < 0) || (rb.count < 1))
    {
#ifndef NDEBUG
      printf("Cannot get buffers\n");
#endif
      return NULL;
    }

  info -> buffers = (tveng2_vbuf*) malloc(rb.count*sizeof(tveng2_vbuf));
  info -> num_buffers = rb.count;

  for (i = 0; i < rb.count; i++)
    {
      info -> buffers[i].vidbuf.index = i;
      info -> buffers[i].vidbuf.type = V4L2_BUF_TYPE_CAPTURE;
      error = ioctl(info->fd, VIDIOC_QUERYBUF, &(info->buffers[i].vidbuf));
      if (error < 0)
	{
#ifndef NDEBUG
	  printf("Cannot query buffers\n");
#endif
	  return NULL;
	}
      info->buffers[i].vmem = mmap(0, info->buffers[i].vidbuf.length,
				   PROT_READ,
				   MAP_SHARED, info->fd, 
				   info->buffers[i].vidbuf.offset);
      if ((int)info->buffers[i].vmem == -1)
	{
#ifndef NDEBUG
	  printf("Cannot mmap() capture devices\n");
#endif
	  return NULL;
	}
      
	/* Queue the buffer */
      tveng2_qbuf(i, info);
    }

  switch (info -> pix_format.fmt.pix.pixelformat)
    {
    case V4L2_PIX_FMT_RGB555:
      info -> format.pixformat = TVENG2_PIX_RGB555;
      info -> format.depth = 15;
      info -> format.bpp = 2;
      break;
    case V4L2_PIX_FMT_RGB565:
      info -> format.pixformat = TVENG2_PIX_RGB565;
      info -> format.depth = 16;
      info -> format.bpp = 2;
      break;
    case V4L2_PIX_FMT_RGB24:
      info -> format.pixformat = TVENG2_PIX_RGB24;
      info -> format.depth = 24;
      info -> format.bpp = 3;
      break;
    case V4L2_PIX_FMT_BGR24:
      info -> format.pixformat = TVENG2_PIX_BGR24;
      info -> format.depth = 24;
      info -> format.bpp = 3;
      break;
    case V4L2_PIX_FMT_RGB32:
      info -> format.pixformat = TVENG2_PIX_RGB32;
      info -> format.depth = 32;
      info -> format.bpp = 4;
      break;
    case V4L2_PIX_FMT_BGR32:
      info -> format.pixformat = TVENG2_PIX_BGR32;
      info -> format.depth = 32;
      info -> format.bpp = 4;
      break;
    default:
      fprintf(stderr, "%s (%d): Cannot init this struct correctly\n",
	      __FILE__, __LINE__);
      break;
    }
  info -> format.width = info -> pix_format.fmt.pix.width;
  info -> format.height = info -> pix_format.fmt.pix.height;
  /* Set Bytes per line */
  if (info -> pix_format.fmt.pix.flags & V4L2_FMT_FLAG_BYTESPERLINE)
    {
#ifndef NDEBUG
      printf("Bytes per line set\n");
#endif
      info -> format.bytesperline = info->pix_format.fmt.pix.bytesperline;
    }
  else /* try to estimate this value */
    info -> format.bytesperline = (info -> format.width) * (info ->
							    format.bpp);
  info -> format.sizeimage = info->format.bytesperline *
    info->format.height;
  info -> format.data = (gpointer) malloc(info->format.sizeimage);
  if (!(info -> format.data))
    printf("(%s) %d: Cannot allocate memory for info->format.data\n",
	   __FILE__, __LINE__);

  /* Turn on streaming */
  tmp_var = V4L2_BUF_TYPE_CAPTURE;
  if (ioctl(info->fd, VIDIOC_STREAMON, &tmp_var) != 0)
    {
#ifndef NDEBUG
      perror("Cannot turn streaming on");
#endif
      return NULL;
    }

  /* Create the XImage to hold the capture */
  info -> image = gdk_image_new(GDK_IMAGE_NORMAL,
			       rgb_visual,
			       info -> format.width,
			       info -> format.height);

  image_private = (GdkImagePrivate*) info -> image;
  info -> ximage = image_private -> ximage;

  free(info -> ximage -> data);

  /* Set the mode flag */
  info -> current_mode = TVENG2_CAPTURE_MMAPED_BUFFERS;

  return (info->format.data); /* Success */
}

/* Try to stop capturing. -1 on error */
int tveng2_stop_capturing(tveng2_device_info * info)
{
  int i;
  int tmp_var;

  if (info -> current_mode == TVENG2_NO_CAPTURE)
    {
      printf("ERROR: This shouldn't happen (Bad design somewhere)!!! (1)\n");
      return 0; 
    }

  g_assert(info -> current_mode == TVENG2_CAPTURE_MMAPED_BUFFERS);

  /* Turn streaming off */
  tmp_var = V4L2_BUF_TYPE_CAPTURE;
  if (ioctl(info->fd, VIDIOC_STREAMOFF, &tmp_var))
    {
#ifndef NDEBUG
      perror("VIDIOC_STREAMOFF");
#endif
      return -1;
    }

  for (i = 0; i < info -> num_buffers; i++)
    {
      if (munmap(info -> buffers[i].vmem, info ->
		 buffers[i].vidbuf.length))
	{
#ifndef NDEBUG
	  perror("munmap");
#endif
	}
    }

  info -> ximage -> data = (char*) malloc(2); /* Allocate something */
  gdk_image_destroy(info -> image);

  //g_free(info -> buffers);

  /* Free the memory we allocated on start */
  if (info -> format.data)
    free(info -> format.data);

  /* We have switched off the mode */
  info -> current_mode = TVENG2_NO_CAPTURE;

  return 0; /* Success */
}

/* 
   Reads a frame from the video device, storing the read data in
   info->format.data
   time: time to wait using select() in miliseconds
   info: pointer to the video device info structure
   This call was originally intended to wrap a single read() call, but
   since i cannot get it to work, now encapsulates the dqbuf/qbuf
   logic.
   Returns -1 on error, anything else on success
*/
int tveng2_read_frame(unsigned int time, tveng2_device_info * info)
{
  int n; /* The dequeued buffer */
  fd_set rdset;
  struct timeval timeout;

  /* Fill in the rdset structure */
  FD_ZERO(&rdset);
  FD_SET(info->fd, &rdset);
  timeout.tv_sec = 0;
  timeout.tv_usec = time;
  n = select(info->fd +1, &rdset, NULL, NULL, &timeout);
  if (n == -1)
    {
#ifndef NDEBUG
      perror("select()");
#endif
      return -1;
    }
  else if (n == 0)
    return -1; /* This isn't properly an error */

  g_assert(FD_ISSET(info->fd, &rdset)); /* Some sanity check */
  n = tveng2_dqbuf(info);
  if (n == -1)
    {
#ifndef NDEBUG
      perror("tveng2_dqbuf()");
#endif
      return -1;
    }
  /* Ignore frames we haven't been able to process */
  do{
    FD_ZERO(&rdset);
    FD_SET(info->fd, &rdset);
    timeout.tv_sec = timeout.tv_usec = 0;
    if (select(info->fd +1, &rdset, NULL, NULL, &timeout) < 1)
      break;
    tveng2_qbuf(n, info);
    n = tveng2_dqbuf(info);
  } while (TRUE);
  /* Copy the data to the address info->format.data points to */
  memcpy(info->format.data, info->buffers[n].vmem,
	 info->format.sizeimage);
  /* Queue the buffer again for processing */
  if (tveng2_qbuf(n, info))
    {
#ifndef NDEBUG
      perror("tveng2_qbuf()");
#endif
      return -1;
    }
  /* Everything has been OK, return 0 (success) */
  return 0;
}

/* Queues an specific buffer. -1 on error */
int tveng2_qbuf(int buffer_id, tveng2_device_info * info)
{
  struct v4l2_buffer tmp_buffer;

  tmp_buffer.type = info -> buffers[0].vidbuf.type;
  tmp_buffer.index = buffer_id;

  return (ioctl(info->fd, VIDIOC_QBUF, &tmp_buffer));
}

/* dequeues next available buffer and returns it's id. -1 on error */
int tveng2_dqbuf(tveng2_device_info * info)
{
  struct v4l2_buffer tmp_buffer;

  tmp_buffer.type = info -> buffers[0].vidbuf.type;

  if (ioctl(info->fd, VIDIOC_DQBUF, &tmp_buffer))
    return -1;
  
  return (tmp_buffer.index);
}

/* 
   Sets the capture buffer to an specific size. returns -1 on
   error. Remember to check the value of width and height since it can
   be different to the one requested. 
*/
int tveng2_set_capture_size(int width, int height, tveng2_device_info * info)
{
  g_assert(width > 0);
  g_assert(height > 0);

  if (tveng2_stop_capturing(info) == -1) /* Stop all current captures
					 */
    {printf("stop_capturing_failed()\n"); return -1;}
  
  /* We set the new format , clipping if neccessary */
  
  if (width < info->caps.minwidth)
     width = info->caps.minwidth;
   else if (width > info->caps.maxwidth)
     width = info->caps.maxwidth;
   if (height < info->caps.minheight)
     height = info->caps.minheight;
   else if (height > info->caps.maxheight)
     height = info->caps.maxheight;

  info -> pix_format.type = V4L2_BUF_TYPE_CAPTURE;

  if (ioctl(info->fd, VIDIOC_G_FMT, &(info->pix_format)))
    {
      printf("get format in set_capture_size failed\n");
      return -1;
    }

  info -> pix_format.fmt.pix.width = width;
  info -> pix_format.fmt.pix.height = height;
  info -> pix_format.type = V4L2_BUF_TYPE_CAPTURE;
  if (info -> interlaced)
    info -> pix_format.fmt.pix.flags = V4L2_FMT_FLAG_INTERLACED;
  else
    info -> pix_format.fmt.pix.flags = 0;

  /* Call the ioctl */
  if (ioctl(info -> fd, VIDIOC_S_FMT, &(info -> pix_format)))
    {printf("Set format failed\n"); return -1;}

  if (tveng2_start_capturing(info) == NULL) /* Starts capturing again */
    {printf("start_capturing failed()\n"); return -1;}

  return 0; /* Success */
}

/* 
   Gets the actual size of the capture buffer in width and height.
   -1 on error
*/
int tveng2_get_capture_size(int *width, int *height, tveng2_device_info * info)
{
  struct v4l2_format format;

  format.type = V4L2_BUF_TYPE_CAPTURE;

  /* Call the ioctl */
  if (ioctl(info->fd, VIDIOC_G_FMT, &format))
    return -1;

  *width = format.fmt.pix.width;
  *height = format.fmt.pix.height;

  return 0; /* Success */
}

/* XF86 Frame Buffer routines */
/* 
   Detects the presence of a suitable Frame Buffer.
   TRUE if the program should continue (Frame Buffer present,
   available and suitable)
   FALSE if the framebuffer shouldn't be used.
   FIXME: Many FB and Overlay routines require root access, make that
          routines into an external program with SUID root
	  privileges. Potential security flaw here.
   display: The display we are connected to (gdk_display)
   screen: The screen we will use
   info: Its fb member is filled in
*/

/* Most of the functionality here has moved to zapping_setup_fb */
gboolean
tveng2_detect_XF86DGA(Display * display, int screen, tveng2_device_info
		     * info)
{
  int event_base, error_base;
  int major_version, minor_version;
  int flags;

  if (!XF86DGAQueryExtension(display, &event_base, &error_base))
    {
#ifndef NDEBUG
      perror("XF86DGAQueryExtension");
#endif
      return FALSE;
    }

  if (!XF86DGAQueryVersion(display, &major_version, &minor_version))
    {
#ifndef NDEBUG
      perror("XF86DGAQueryVersion");
#endif
      return FALSE;
    }

  if (!XF86DGAQueryDirectVideo(display, screen, &flags))
    {
#ifndef NDEBUG
      perror("XF86DGAQueryDirectVideo");
#endif
      return FALSE;
    }

  /* Direct Video should be present (otherwise enabling all this would
     be pointless) */
  if (!(flags & XF86DGADirectPresent))
    {
#ifndef NDEBUG
      printf("flags & XF86DGADirectPresent\n");
#endif
      return FALSE;
    }

/* Print collected info if we are in debug mode */
#ifndef NDEBUG
  printf("DGA info (using screen %d and current gdk display)\n", screen);
  printf("  - event and error base  : %d, %d\n", event_base, error_base);
  printf("  - DGA reported version  : %d.%d\n", major_version, minor_version);
  printf("  - Supported features    :%s\n",
	 (flags & XF86DGADirectPresent) ? " DirectVideo" : "");
#endif

  return TRUE; /* Everything correct */
}

/*
  Checks if previewing is available for the desired device.
  Returns TRUE if success, FALSE on error
*/
gboolean
tveng2_detect_preview (tveng2_device_info * info)
{
  if (!(info -> caps.flags & V4L2_FLAG_PREVIEW))
    {
#ifndef NDEBUG
      printf("info -> caps.flags & V4L2_FLAG_PREVIEW\n");
#endif
      return FALSE;
    }

  /* The current framebuffer info */
  if (ioctl(info -> fd, VIDIOC_G_FBUF, &(info -> fb)))
    {
#ifndef NDEBUG
      perror("VIDIOC_G_FBUF");
#endif
      return FALSE;
    }

/* Print some info about the Overlay device */
#ifndef NDEBUG
  printf("Capabilities of the FB device (according to V4L2):\n");
  if (info -> fb.capability & V4L2_FBUF_CAP_EXTERNOVERLAY)
    printf("   - V4L2_FBUF_CAP_EXTERNOVERLAY\n");
  if (info -> fb.capability & V4L2_FBUF_CAP_CHROMAKEY)
    printf("   - V4L2_FBUF_CAP_CHROMAKEY\n");
  if (info -> fb.capability & V4L2_FBUF_CAP_CLIPPING)
    printf("   - V4L2_FBUF_CAP_CLIPPING\n");
  if (info -> fb.capability & V4L2_FBUF_CAP_BITMAP_CLIPPING)
    printf("   - V4L2_FBUF_CAP_BITMAP_CLIPPING\n");
  if (info -> fb.capability & V4L2_FBUF_CAP_SCALEUP)
    printf("   - V4L2_FBUF_CAP_SCALEUP\n");
  if (info -> fb.capability & V4L2_FBUF_CAP_SCALEDOWN)
    printf("   - V4L2_FBUF_CAP_SCALEDOWN\n");
#endif

  return TRUE;
}

/*
  Sets the preview window dimensions to the given window.
  Returns -1 on error, something else on success.
  Success doesn't mean that the requested dimensions are used, maybe
  they are different, check the returned fields to see if they are suitable
  window : Structure containing the window to use (including clipping)
  info   : Device we are controlling
  The current chromakey value is used, the caller doesn't need to fill
  it in.
*/
int
tveng2_set_preview_window(struct v4l2_window * window,
			 tveng2_device_info * info)
{
  struct v4l2_window check_window;

  g_assert(window != NULL);
  g_assert(window->clipcount >= 0);
  g_assert((window->clips != NULL) || (window -> clipcount == 0));

  /* Get the current dimensions */
  if (tveng2_get_preview_window(&check_window, info) == -1)
    return -1;

  /* Fill in chromakey value and set window */
  window -> chromakey = check_window.chromakey;
  if (ioctl(info -> fd, VIDIOC_S_WIN, window))
    {
#ifndef NDEBUG
      perror("VIDIOC_S_WIN");
#endif
      return -1;
    }

  /* Check returned */
  if (tveng2_get_preview_window(window, info) == -1)
    return -1;

  /* Success */
  return 0;
}

/*
  Gets the current overlay window parameters.
  Returns -1 on error, and any other value on success.
  window : Where to store the collected data
  info   : The device to use
*/
int
tveng2_get_preview_window(struct v4l2_window * window,
			 tveng2_device_info * info)
{
  if (ioctl(info -> fd, VIDIOC_G_WIN, window))
    {
#ifndef NDEBUG
      perror("VIDIOC_G_WIN");
#endif
      return -1;
    }

  return 0; /* Success */
}

/* 
   Sets the previewing on/off.
   TRUE  : on
   FALSE : off
   info  : device to use for previewing
   Returns -1 on error, anything else on success
*/
int
tveng2_set_preview (gboolean on, tveng2_device_info * info)
{
  int state_on=1, state_off=0;
  if (ioctl(info -> fd, VIDIOC_PREVIEW, on ? &state_on : &state_off))
    {
#ifndef NDEBUG
      perror("VIDIOC_PREVIEW");
#endif
      return -1;
    }

  return 0;
}

/* 
   Sets up everything and starts previewing to Full Screen if possible.
   Just call this function to start fullscreen mode, it takes care of
   everything.
   There should be no capture active when calling this function
   Returns TRUE on success and FALSE on failure.
*/
gboolean
tveng2_start_fullscreen_previewing (tveng2_device_info * info, int
				   verbosity)
{
  Display * display = gdk_display;
  int screen = 0; /* Capture to screen number 0 by default */
  struct v4l2_window window;
  char * argv[8]; /* The command line passed to zapping_setup_fb */
  pid_t pid; /* New child's pid as returned by fork() */
  int status; /* zapping_setup_fb returned status */
  int i=0;

  if (!tveng2_detect_preview(info))
    {
      ShowBox(_("V4L2 doesn't report any previewing capabilities"),
	      GNOME_MESSAGE_BOX_ERROR);
      return FALSE;
    }

  if (!tveng2_detect_XF86DGA(display, screen, info))
    {
      ShowBox(_("XFree86 - DGA extension isn't valid or it is missing"),
	      GNOME_MESSAGE_BOX_ERROR);
      return FALSE;
    }

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
  argv[i] = NULL;
  
  pid = fork();

  if (pid == -1)
    {
      perror("fork");
      return FALSE;
    }

  if (!pid) /* New child process */
    {
      execvp("zapping_setup_fb", argv);
      /* This shouldn't be reached if everything suceeds */
      perror("execvp");
      _exit(2); /* zapping setup_fb on error returns 1 */
    }

  /* This statement is only reached by the parent */
  if (waitpid(pid, &status, 0) == -1)
    {
      perror("waitpid");
      return FALSE;
    }

  if (! WIFEXITED(status)) /* zapping_setup_fb exited abnormally */
    {
      ShowBox(_("zapping_setup_fb exited abnormally (check stderr)\n"),
	      GNOME_MESSAGE_BOX_ERROR);
      return FALSE;
    }

  switch (WEXITSTATUS(status))
    {
    case 1: /* zapping_setup_fb couldn't do its job */
      ShowBox(_("zapping_setup_fb returned error (check stderr)\n"),
	      GNOME_MESSAGE_BOX_ERROR);
      return FALSE;
    case 2:
      ShowBox(_("Cannot start zapping_setup_fb, please check\n"
		"your installation. Try running:\n"
		"zapping_setup_fb --verbose --verbose\n"),
	      GNOME_MESSAGE_BOX_ERROR);
      return FALSE;
    default:
      break; /* Exit code == 0, success setting up the framebuffer */
    }

  /* Enable Direct Graphics (just the DirectVideo thing) */
  if (!XF86DGADirectVideo(display, screen, XF86DGADirectGraphics))
    {
#ifndef NDEBUG
      perror("XF86DGADirectVideo\n");
#endif
      return FALSE;
    }

  /* Set coordinates */
  window.x = window.y = 0;
  window.width = gdk_screen_width();
  window.height = gdk_screen_height();
  window.clips = NULL;
  window.clipcount = 0;
  window.bitmap = NULL;

  /* Set new capture dimensions */
  if (tveng2_set_preview_window(&window, info) == -1)
    {
      /* Switch DirectVideo off */
      if (!XF86DGADirectVideo(display, screen, 0))
#ifndef NDEBUG
	perror("XF86DGADirectVideo");
#endif
      return FALSE;
    }

  /* Center preview window (maybe the requested width and/or height)
     aren't valid */
  window.x = (gdk_screen_width()/2) - (window.width/2);
  window.y = (gdk_screen_height()/2) - (window.height/2);
  window.clipcount = 0;
  window.clips = NULL;
  if (tveng2_set_preview_window(&window, info) == -1)
    {
      /* Switch DirectVideo off */
      if (!XF86DGADirectVideo(display, screen, 0))
#ifndef NDEBUG
	perror("XF86DGADirectVideo");
#endif
      return FALSE;
    }

  /* Start preview */
  if (tveng2_set_preview_on(info) == -1)
    {
      /* Switch DirectVideo off */
      if (!XF86DGADirectVideo(display, screen, 0))
#ifndef NDEBUG
	perror("XF86DGADirectVideo");
#endif
      return FALSE;
    }

  /* FIXME: Clear screen */

  info -> current_mode = TVENG2_CAPTURE_FULLSCREEN;
  return TRUE; /* Success */
}

/*
  Stops the fullscreen mode. FALSE on error and TRUE on success
*/
gboolean
tveng2_stop_fullscreen_previewing(tveng2_device_info * info)
{
  Display * display = gdk_display;
  int screen = 0; /* Capture to screen number 0 by default */

  if (info -> current_mode == TVENG2_NO_CAPTURE)
    {
      printf("ERROR: This shouldn't happen (Bad design somewhere)!!! (2)\n");
      return 0; 
    }

  g_assert(info -> current_mode == TVENG2_CAPTURE_FULLSCREEN);

  /* No error checking */
  tveng2_set_preview_off(info);

  if (!XF86DGADirectVideo(display, screen, 0))
    {
#ifndef NDEBUG
      perror("XF86DGADirectVideo\n");
#endif
      return FALSE;
    }

  info -> current_mode = TVENG2_NO_CAPTURE;
  return TRUE; /* Success */
}
