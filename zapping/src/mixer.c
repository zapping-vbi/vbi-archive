/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2001 Iñaki García Etxebarria
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
/**
 * Audio mixer handling stuff.
 *
 * FIXME: This is linux specific and doesn't support
 * things such as alsa, i've tried to make supporting new systems
 * as painless as possible.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include "mixer.h"

typedef struct _device_entry	device_entry;
typedef struct _line_entry	line_entry;

/**
 * A device line, i.e., a interface to the Micro volume for example. 
 */
struct _line_entry {
  char		*description;
  int		volume_max;
  int		volume_min;

  union {
    /* /dev/mixer fields */
    struct {
      int		recmask; /* Devices usable for recording */
      int		index; /* Line index in the device */
    }	dev_mixer;
    /* alsa, etc would go in here */
  } data;

  /* Callbacks the handler provides */
  int		(*set_volume)		(device_entry	*dev,
					 line_entry	*line,
					 int volume);
  int		(*set_recording_line)	(device_entry	*dev,
					 line_entry	*line);
  void		(*destroy)		(device_entry	*dev,
					 line_entry	*line);
};

/**
 * Definition of a device entry (that is, a set of lines associated
 * with a given device, such as /dev/mixer0).
 */
struct _device_entry {
  int		num_lines;
  line_entry	*lines;

  union {
    /* /dev/mixer fields */
    struct {
      int		fd;
    }	dev_mixer;
    /* alsa, etc would go in here */
  } data;

  /* Callbacks the handler must implement */
  void		(*destroy)(device_entry *entry);
};

static int		num_devices = 0;
static device_entry	*devices = NULL; /* mixers we have detected */

char		*mixer_get_description	(int	line)
{
  int linecount = 0;
  int i, j;

  for (i=0; i<num_devices; i++)
    {
      for (j=0; j<devices[i].num_lines; j++, linecount++)
	if (linecount == line)
	  return strdup(devices[i].lines[j].description);
    }

  return NULL;
}

int		mixer_get_bounds	(int	line,
					 int	*min,
					 int	*max)
{
  int linecount = 0;
  int i, j;

  for (i=0; i<num_devices; i++)
    {
      for (j=0; j<devices[i].num_lines; j++, linecount++)
	if (linecount == line)
	  {
	    if (min)
	      *min = devices[i].lines[j].volume_min;
	    if (max)
	      *max = devices[i].lines[j].volume_max;
	    return 0;
	  }
    }
  
  return -1; /* not found */
}

int		mixer_set_volume	(int	line,
					 int	volume)
{
  int linecount = 0;
  int i, j;

  for (i=0; i<num_devices; i++)
    {
      for (j=0; j<devices[i].num_lines; j++, linecount++)
	if (linecount == line)
	  {
	    if (volume < devices[i].lines[j].volume_min)
	      volume = devices[i].lines[j].volume_min;
	    if (volume > devices[i].lines[j].volume_max)
	      volume = devices[i].lines[j].volume_max;
	    return devices[i].lines[j].set_volume(&devices[i],
						  &devices[i].lines[j],
						  volume);
	  }
    }
  
  return -1; /* not found */
}

int		mixer_set_recording_line(int	line)
{
  int linecount = 0;
  int i, j;

  for (i=0; i<num_devices; i++)
    {
      for (j=0; j<devices[i].num_lines; j++, linecount++)
	if (linecount == line)
	  return
	    devices[i].lines[j].set_recording_line(&devices[i],
						   &devices[i].lines[j]);
    }
  
  return -1; /* not found */
}

static void add_dev_mixer_devices	(void);

void		startup_mixer	(void)
{
  add_dev_mixer_devices();
  /* alsa, etc */
}

void		shutdown_mixer(void)
{
  int i;

  for (i=0; i<num_devices; i++)
    devices[i].destroy(&devices[i]);

  if (devices)
    free(devices);
  devices = NULL;

  num_devices = 0;
}

/**
 * Some convenience stuff.
 */
/**
 * Add a line to the device, returns a pointer to the object (valid
 * only till the next call to add_line_entry).
 */
static line_entry*
add_line			(device_entry	*d)
{
  d->lines = realloc(d->lines, sizeof(line_entry)*(++d->num_lines));

  memset(&d->lines[d->num_lines-1], 0, sizeof(line_entry));

  return (&d->lines[d->num_lines-1]);
}

/**
 * Add a new device to the list.
 * Returns a pointer to the device, valid only till the next call to
 * add_device_entry.
 */
static device_entry*
add_device_entry		(void)
{
  devices = realloc(devices, sizeof(device_entry)*(++num_devices));

  memset(&devices[num_devices-1], 0, sizeof(*devices));

  return (&devices[num_devices-1]);
}

/**
 * Runs the "destroy" callback in all the lines belonging to a give
 * device and frees dev->lines.
 */
static void
destroy_lines			(device_entry	*dev)
{
  int i;

  for (i=0; i<dev->num_lines; i++)
    dev->lines[i].destroy(dev, &dev->lines[i]);

  free(dev->lines);
  dev->lines = NULL;
}

/**
 * Platform specific code follows.
 */
/************************ /dev/mixer interface **************************/
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/soundcard.h>
#include <stdio.h>
#include <unistd.h>

static int
dev_mixer_set_volume		(device_entry	*dev,
				 line_entry	*line,
				 int		volume)
{
  volume = volume | (volume << 8);

  return ioctl(dev->data.dev_mixer.fd,
	       MIXER_WRITE(line->data.dev_mixer.index), &volume);
}

static int
dev_mixer_set_recording_line	(device_entry	*dev,
				 line_entry	*line)
{
  int recsrc = 1<<line->data.dev_mixer.index;

  return ioctl(dev->data.dev_mixer.fd,
	       MIXER_WRITE(SOUND_MIXER_RECSRC), &recsrc);
}

static void
dev_mixer_line_destroy		(device_entry	*dev,
				 line_entry	*line)
{
  free(line->description);
}

static void
dev_mixer_destroy		(device_entry	*dev)
{
  destroy_lines(dev);

  close(dev->data.dev_mixer.fd);
}

static void
add_dev_mixer_devices		(void)
{
  int i, j;
  device_entry *d = NULL;

  for (i=0; ;i++)
    {
      char buffer[256];
      int fd;
      int r;
      int devmask, recmask;

      buffer[sizeof(buffer)-1] = 0;
      snprintf(buffer, sizeof(buffer)-1, "/dev/mixer%d", i);

      fd = open(buffer, O_RDWR, 0);
      if (fd < 1)
	break;

      r = ioctl(fd, SOUND_MIXER_READ_DEVMASK, &devmask);
      r |= ioctl(fd, SOUND_MIXER_READ_RECMASK, &recmask);

      if (r)
	goto noncritical;

      if (!(recmask & devmask))
	goto noncritical; /* nothing useful */

      d = NULL;

      for (j = 0; j<SOUND_MIXER_NRDEVICES /* soundcard.h */; j++)
	if ((recmask) & (1<<j))
	  {
	    line_entry *line;
	    char *labels[] = SOUND_DEVICE_LABELS; /* soundcard.h */

	    if (!d)
	      d = add_device_entry();

	    line = add_line(d);
	    line->description = strdup(labels[j]); /* FIXME: i18n */
	    line->volume_min = 0;
	    line->volume_max = 100;

	    line->set_volume = dev_mixer_set_volume;
	    line->set_recording_line = dev_mixer_set_recording_line;
	    line->destroy = dev_mixer_line_destroy;

	    line->data.dev_mixer.recmask = recmask;
	    line->data.dev_mixer.index = j;
	  }

      if (d)
	{
	  d->data.dev_mixer.fd = fd;
	  d->destroy = dev_mixer_destroy;
	}
      continue;

      /* The device is unusable, but we can keep scanning */
    noncritical:
      close(fd);
    }
}
