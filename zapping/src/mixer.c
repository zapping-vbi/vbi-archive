/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *  Soundcard mixer interface
 *
 *  Copyright (C) 2002, 2003 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: mixer.c,v 1.18 2007-08-30 14:14:35 mschimek Exp $ */

/*
 *  These functions encapsulate the OS and driver specific
 *  soundcard mixer functions.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/*
 * Preliminary until the mixer becomes part of the
 * virtual tv device.
 */

#include <assert.h>

#include "tveng_private.h"
#include "mixer.h"
#include "zconf.h"
#include "globals.h"
#include "audio.h"

/* preliminary */
void		shutdown_mixer(tveng_device_info *info)
{
  if (mixer) {
    mixer_line = NULL;
    tveng_attach_mixer_line (info, NULL, NULL);
    tv_mixer_close (mixer);
    mixer = NULL;
  }
}

/* preliminary */
void		startup_mixer(tveng_device_info *info)
{
  const gchar *dev_name;

  shutdown_mixer(info);

  if (zconf_get_boolean (NULL, "/zapping/options/audio/force_mixer"))
    {
      if ((dev_name = zconf_get_string (NULL, "/zapping/options/audio/mixer_device")))
	{
	  /* FIXME report errors */
	  if ((mixer = tv_mixer_open ((io_debug_msg > 0) ? stderr : NULL,
				      dev_name)))
	    {
	      tv_audio_line *line;
	      guint hash;

	      if (!mixer->inputs)
		{
		  shutdown_mixer (info);
		  return;
		}
	      
	      hash = zconf_get_int (NULL,
				    "/zapping/options/audio/mixer_input");

	      mixer_line = mixer->inputs;

	      for (line = mixer->inputs; line; line = line->_next)
		if (line->hash == hash)
		  {
		    mixer_line = line;
		    break;
		  }

	      if (esd_output || ivtv_audio)
		tveng_attach_mixer_line (info,
					 &audio_loopback_mixer,
					 &audio_loopback_mixer_line);
	      else
		tveng_attach_mixer_line (info, mixer, mixer_line);
	    }
	}
      else
	{
	  ShowBox (_("The soundcard mixer is not configured."),
		   GTK_MESSAGE_WARNING);
	}
    }
}

/*
 *  Mixer client interface
 */

/* XXX document me */

tv_bool
tv_mixer_line_update		(tv_audio_line *	line)
{
	assert (NULL != line);

	return ((tv_mixer *) line->_parent)->_interface->update_line (line);
}

tv_bool
tv_mixer_line_get_volume	(tv_audio_line *	line,
				 int *			left,
				 int *			right)
{
	assert (NULL != line);

	if (!((tv_mixer *) line->_parent)->_interface->update_line (line))
		return FALSE;

	*left = line->volume[0];
	*right = line->volume[1];

	return TRUE;
}

tv_bool
tv_mixer_line_set_volume	(tv_audio_line *	line,
				 int			left,
				 int			right)
{
	assert (NULL != line);

	left = SATURATE (left, line->minimum, line->maximum);
	right = SATURATE (right, line->minimum, line->maximum);

	return ((tv_mixer *) line->_parent)
	  ->_interface->set_volume (line, left, right);
}

tv_bool
tv_mixer_line_get_mute		(tv_audio_line *	line,
				 tv_bool *		mute)
{
	assert (NULL != line);

	if (!((tv_mixer *) line->_parent)->_interface->update_line (line))
		return FALSE;

	*mute = line->muted;

	return TRUE;
}

extern tv_bool
tv_mixer_line_set_mute		(tv_audio_line *	line,
				 tv_bool		mute)
{
	assert (NULL != line);
	assert (NULL != line->_parent);
	assert (NULL != ((tv_mixer *) line->_parent)->_interface);
	assert (NULL != ((tv_mixer *) line->_parent)->_interface->set_mute);

	return ((tv_mixer *) line->_parent)->_interface->set_mute (line, !!mute);
}

tv_bool
tv_mixer_line_record		(tv_audio_line *	line,
				 tv_bool		exclusive)
{
	tv_audio_line *l;

	assert (NULL != line);
	assert (NULL != line->_parent);

	for (l = ((tv_mixer *) line->_parent)->inputs; l; l = l->_next)
		if (line == l)
			break;

	if (!l || !l->recordable) {
		fprintf (stderr, "%s: Invalid recording line requested\n", __FILE__);
		abort ();
	}

	return ((tv_mixer *) line->_parent)->_interface->set_rec_line
	  (((tv_mixer *) line->_parent), line, !!exclusive);
}

tv_bool
tv_mixer_update			(tv_mixer *		mixer)
{
	assert (NULL != mixer);

	return mixer->_interface->update_mixer (mixer);
}

extern const tv_mixer_interface oss_mixer_interface;

static const tv_mixer_interface *
mixer_interfaces [] = {
#ifdef HAVE_OSS
	&oss_mixer_interface,
#endif
	NULL
};

/* XXX document me */
tv_mixer *
tv_mixer_open			(FILE *			log,
				 const char *		dev_name)
{
	const tv_mixer_interface **mip;
	tv_mixer *m;

	if (!dev_name || !dev_name[0])
		return NULL;

	for (mip = mixer_interfaces; *mip; mip++)
		if ((m = (*mip)->open (*mip, log, dev_name)))
			return m;

	return NULL;
}

tv_mixer *
tv_mixer_scan			(FILE *			log)
{
	const tv_mixer_interface **mip;
	tv_mixer *m;

	for (mip = mixer_interfaces; *mip; mip++)
		if ((m = (*mip)->scan (*mip, log)))
			return m;

	return NULL;
}

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
