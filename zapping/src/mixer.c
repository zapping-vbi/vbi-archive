/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2001 I�aki Garc�a Etxebarria
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

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
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

  int           muted;
  int           muted_volume;

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
  int		(*get_volume)		(device_entry	*dev,
					 line_entry	*line,
					 int *volume);
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
            devices[i].lines[j].muted = 0;
	    return devices[i].lines[j].set_volume(&devices[i],
						  &devices[i].lines[j],
						  volume);
	  }
    }
  
  return -1; /* not found */
}

int		mixer_set_mute		(int	line,
					 int	mute)
{
  int linecount = 0;
  int i, j;

  for (i=0; i<num_devices; i++)
    {
      for (j=0; j<devices[i].num_lines; j++, linecount++)
	if (linecount == line)
	  {
	    if (devices[i].lines[j].muted == !!mute)
	      return 0;
	    if (mute)
	      {
	        devices[i].lines[j].get_volume (&devices[i],
						&devices[i].lines[j],
						&devices[i].lines[j].muted_volume);
	        devices[i].lines[j].set_volume (&devices[i],
						&devices[i].lines[j],
						devices[i].lines[j].volume_min);
		devices[i].lines[j].muted = 1;
	      }
	    else
	      {
	        devices[i].lines[j].set_volume (&devices[i],
						&devices[i].lines[j],
						devices[i].lines[j].muted_volume);
		devices[i].lines[j].muted = 0;
	      }

	    return 0;
	  }
    }
  
  return -1; /* not found */
}

int		mixer_get_mute		(int	line)
{
  int linecount = 0;
  int i, j;

  for (i=0; i<num_devices; i++)
    {
      for (j=0; j<devices[i].num_lines; j++, linecount++)
	if (linecount == line)
	  {
	    return devices[i].lines[j].muted;
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

#ifdef USE_OSS
static void add_dev_mixer_devices	(void);
#endif

void		startup_mixer	(void)
{
#ifdef USE_OSS
  add_dev_mixer_devices();
#endif
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
#ifdef USE_OSS
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef HAVE_SYS_SOUNDCARD_H
#include <sys/soundcard.h>
#elif defined(HAVE_MACHINE_SOUNDCARD_H)
#include <machine/soundcard.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

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
dev_mixer_get_volume		(device_entry	*dev,
				 line_entry	*line,
				 int *		volume)
{
  int r;

  r = ioctl (dev->data.dev_mixer.fd,
	       MIXER_READ (line->data.dev_mixer.index), volume);

  *volume = ((*volume & 0xFF) + (*volume >> 8)) / 2;

  return r;
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

  for (i=-1; ;i++)
    {
      char buffer[256];
      int fd;
      int r;
      int devmask, recmask;

      buffer[sizeof(buffer)-1] = 0;
      if (i != -1)
	snprintf(buffer, sizeof(buffer)-1, "/dev/mixer%d", i);
      else /* try /dev/mixer first */
	snprintf(buffer, sizeof(buffer)-1, "/dev/mixer");

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
	    const char *labels[] = SOUND_DEVICE_LABELS; /* soundcard.h */

	    if (!d)
	      d = add_device_entry();

	    line = add_line(d);
	    line->description = g_strdup_printf("%s %u: %s", buffer, j, labels[j]);
	    line->volume_min = 0;
	    line->volume_max = 100;
	    line->muted = 0;

	    line->set_volume = dev_mixer_set_volume;
	    line->get_volume = dev_mixer_get_volume;
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
#endif

/* New mixer interface */

#include "../common/types.h"

#undef SATURATE
#ifdef __i686__
#define SATURATE(n, min, max) ({					\
	__typeof__ (n) _n = n;						\
	__typeof__ (n) _min = min;					\
	__typeof__ (n) _max = max;					\
	if (_n < _min)							\
		_n = _min;						\
	if (_n > _max)							\
		_n = _max;						\
	_n;								\
})
#else
#define SATURATE(n, min, max) ({					\
	__typeof__ (n) _n = n;						\
	__typeof__ (n) _min = min;					\
	__typeof__ (n) _max = max;					\
	if (_n < _min)							\
		_n = _min;						\
	else if (_n > _max)						\
		_n = _max;						\
	_n;								\
})
#endif

#define PROLOG								\
	tv_dev_mixer_line *l;						\
	tv_dev_mixer *m;						\
									\
	assert (line != NULL);						\
									\
	l = PARENT (line, tv_dev_mixer_line, pub);			\
	m = l->mixer;

tv_bool
tv_mixer_line_update		(tv_mixer_line *	line)
{
	PROLOG

	return m->update_line (l);
}

tv_bool
tv_mixer_line_get_volume	(tv_mixer_line *	line,
				 unsigned int *		left,
				 unsigned int *		right)
{
	PROLOG

	if (!m->update_line (l))
		return FALSE;

	*left = l->pub.volume[0];
	*right = l->pub.volume[1];

	return TRUE;
}

tv_bool
tv_mixer_line_set_volume	(tv_mixer_line *	line,
				 unsigned int		left,
				 unsigned int		right)
{
	PROLOG

	left = SATURATE (left, l->pub.minimum, l->pub.maximum);
	right = SATURATE (right, l->pub.minimum, l->pub.maximum);

	return m->set_volume (l, left, right);
}

tv_bool
tv_mixer_line_get_mute		(tv_mixer_line *	line,
				 tv_bool *		mute)
{
	PROLOG

	if (!m->update_line (l))
		return FALSE;

	*mute = l->pub.muted;

	return TRUE;
}

extern tv_bool
tv_mixer_line_set_mute		(tv_mixer_line *	line,
				 tv_bool		mute)
{
	PROLOG

	return m->set_mute (l, !!mute);
}

tv_bool
tv_mixer_line_record		(tv_mixer_line *	line,
				 tv_bool		exclusive)
{
	tv_dev_mixer_line *l;
	tv_dev_mixer *m;

	assert (line != NULL);

	l = PARENT (line, tv_dev_mixer_line, pub);
	m = l->mixer;

	for (line = m->pub.inputs; line; line = line->next)
		if (line == &l->pub)
			break;

	if (!line || !l->pub.recordable) {
		fprintf (stderr, "%s: Invalid recording line requested\n", __FILE__);
		return FALSE;
	}

	return m->set_rec_line (m, l, !!exclusive);
}

tv_callback_node *
tv_mixer_line_callback_add	(tv_mixer_line *	line,
				 tv_bool		(* notify)(tv_mixer_line *, void *user_data),
				 void			(* destroy)(tv_mixer_line *, void *user_data),
				 void *			user_data)
{
	tv_dev_mixer_line *l;

	assert (line != NULL);
	assert (notify != NULL);

	l = PARENT (line, tv_dev_mixer_line, pub);

	return tv_callback_add (&l->callback,
				(void *) notify,
				(void *) destroy,
				user_data);
}

tv_bool
tv_mixer_update			(tv_mixer *		mixer)
{
	tv_dev_mixer *m;

	assert (mixer != NULL);

	m = PARENT (mixer, tv_dev_mixer, pub);

	return m->update_mixer (m);
}

tv_callback_node *
tv_mixer_callback_add		(tv_mixer *		mixer,
				 tv_bool		(* notify)(tv_mixer_line *, void *user_data),
				 void			(* destroy)(tv_mixer_line *, void *user_data),
				 void *			user_data)
{
	tv_dev_mixer *m;

	assert (mixer != NULL);

	m = PARENT (mixer, tv_dev_mixer, pub);

	return tv_callback_add (&m->callback,
				(void *) notify,
				(void *) destroy,
				user_data);
}
