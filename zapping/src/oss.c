/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
 *
 * New OSS mixer Copyright (C) 2003 Michael H. Schimek
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

/** OSS backend **/
#if USE_OSS

#include <gnome.h>
#include <math.h>
#include <unistd.h>

#include "../common/fifo.h" // current_time()

#include "audio.h"
#define ZCONF_DOMAIN "/zapping/options/audio/"
#include "zconf.h"
#include "interface.h"
#include "zmisc.h"
#include "globals.h"

#include <sys/ioctl.h>
#include <sys/soundcard.h>

typedef struct {
  int		fd;
  int		stereo;
  int		sampling_rate;
  double	time, buffer_period;
} oss_handle;

#define IOCTL(fd, cmd, data)						\
({ int __result; do __result = ioctl(fd, cmd, data);			\
   while (__result == -1L && errno == EINTR); __result; })

static gpointer
oss_open (gboolean stereo, gint rate, enum audio_format format)
{
  int Format = AFMT_S16_LE;
  int Stereo = !!stereo;
  int Speed = rate;
  int oss_fd;
  oss_handle *h;

  if (format != AUDIO_FORMAT_S16_LE)
    {
      g_warning("Requested audio format won't work");
      return NULL;
    }

  if ((oss_fd = open(zcg_char(NULL, "oss/device"), O_RDONLY)) == -1)
    return NULL;

  if ((IOCTL(oss_fd, SNDCTL_DSP_SETFMT, &Format) == -1))
    goto failed;
  if ((IOCTL(oss_fd, SNDCTL_DSP_STEREO, &Stereo) == -1))
    goto failed;
  if ((IOCTL(oss_fd, SNDCTL_DSP_SPEED, &Speed) == -1))
    goto failed;

  h = (oss_handle *) g_malloc0(sizeof(*h));
  h->fd = oss_fd;
  h->stereo = stereo;
  h->sampling_rate = rate;
  h->time = 0.0;

  return h;
  
 failed:
  close(oss_fd);
  return NULL;
}

static void
oss_close (gpointer handle)
{
  oss_handle *h = (oss_handle*)handle;

  close(h->fd);
  
  g_free(h);
}

static void
oss_read (gpointer handle, gpointer dest, gint num_bytes,
	  double *timestamp)
{
  oss_handle *h = (oss_handle *) handle;
  ssize_t r, n = num_bytes;
  struct audio_buf_info info;
  char *data = (char *) dest;
  struct timeval tv1, tv2;
  double now;

  while (n > 0)
    {
      r = read(h->fd, data, n);
		
      if (r == 0 || (r < 0 && errno == EINTR))
	continue;

      g_assert(r > 0 && "OSS read failed");

      data = (char *) data + r;
      n -= r;
    }

  /* XXX asks for improvements */

  r = 5;

  do
    {
      gettimeofday(&tv1, NULL);

      if (ioctl(h->fd, SNDCTL_DSP_GETISPACE, &info) != 0)
	{
	  g_assert(errno != EINTR && !"SNDCTL_DSP_GETISPACE failed");
	  continue;
	}

      gettimeofday(&tv2, NULL);
      
      tv2.tv_sec -= tv1.tv_sec;
      tv2.tv_usec -= tv1.tv_usec + (tv2.tv_sec ? 1000000 : 0);
    }
  while ((tv2.tv_sec > 1 || tv2.tv_usec > 100) && r--);

  now = tv1.tv_sec + tv1.tv_usec * (1 / 1e6);

  if ((n -= info.bytes) == 0) /* usually */
    now -= h->buffer_period;
  else
    now -= (num_bytes - n) * h->buffer_period / (double) num_bytes;

  if (h->time > 0)
    {
      double dt = now - h->time;
      double ddt = h->buffer_period - dt;
      double q = 128 * fabs(ddt) / h->buffer_period;

      h->buffer_period = ddt * MIN(q, 0.9999) + dt;
      *timestamp = h->time;
      h->time += h->buffer_period;
    } 
  else
    {
      *timestamp = h->time = now;

      /* XXX assuming num_bytes won't change */
      h->buffer_period =
          num_bytes / (double)(h->sampling_rate * 2 << h->stereo);
    }
}

static void
oss_add_props (GtkBox *vbox)
{
  GtkWidget *label = gtk_label_new(_("Audio device:"));
  GtkWidget *hbox = gtk_hbox_new(TRUE, 3);
  GtkWidget *fentry = gnome_file_entry_new("audio_device_history",
				_("Select the audio device"));
  GtkEntry *entry;

  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), label);

  entry = GTK_ENTRY(gnome_file_entry_gtk_entry(GNOME_FILE_ENTRY(fentry)));
  gtk_entry_set_text(entry, zcg_char(NULL, "oss/device"));
  gtk_box_pack_start_defaults(GTK_BOX(hbox), fentry);
  g_object_set_data(G_OBJECT(vbox), "fentry", fentry);

  gtk_widget_show_all(hbox);
  gtk_box_pack_start_defaults(vbox, hbox);
}

static void
oss_apply_props (GtkBox *vbox)
{
  GnomeFileEntry *fentry =
    GNOME_FILE_ENTRY(g_object_get_data(G_OBJECT(vbox), "fentry"));
  gchar *result = gnome_file_entry_get_full_path(fentry, TRUE);

  if (!result)
    {
      const gchar *real_text =
	gtk_entry_get_text(GTK_ENTRY(gnome_file_entry_gtk_entry(fentry)));

      ShowBox(_("The given audio device \"%s\" doesn't exist"),
	      GTK_MESSAGE_WARNING, real_text);
    }
  else
    {
      zcs_char(result, "oss/device");
      g_free(result);
    }
}

static void
oss_init (void)
{
  zcc_char("/dev/dsp", "OSS audio device", "oss/device");
}

const audio_backend_info oss_backend =
{
  name:		"Open Sound System",
  open:		oss_open,
  close:	oss_close,
  read:		oss_read,
  init:		oss_init,
  
  add_props:	oss_add_props,
  apply_props:	oss_apply_props
};
















/* Future */

/* XXX check for freebsd quirks */

#include "mixer.h"

#include "../common/device.h"

#define VOL_MIN 0
#define VOL_MAX 100

#ifndef SOUND_MIXER_READ_OUTMASK
#define SOUND_MIXER_READ_OUTMASK MIXER_READ (SOUND_MIXER_OUTMASK)
#endif

#ifndef SOUND_MIXER_READ_OUTSRC
#define SOUND_MIXER_READ_OUTSRC MIXER_READ (SOUND_MIXER_OUTSRC)
#endif

#ifndef SOUND_MIXER_WRITE_OUTSRC
#define SOUND_MIXER_WRITE_OUTSRC MIXER_WRITE (SOUND_MIXER_OUTSRC)
#endif

#define CASE(x) case x: if (!arg) { fputs (# x, fp); return; }

static const char *dev_names [] = SOUND_DEVICE_LABELS;

static void
fprintf_ioctl_arg		(FILE *			fp,
				 unsigned int		cmd,
				 void *			arg)
{
	switch (cmd) {
#ifdef SOUND_MIXER_INFO
	CASE (SOUND_MIXER_INFO) {
		struct mixer_info *info = arg;

		fprintf (fp, "id=\"%.*s\" name=\"%.*s\" modify_counter=%d",
			 N_ELEMENTS (info->id), info->id,
			 N_ELEMENTS (info->name), info->name,
			 info->modify_counter);
		break;
	}
#endif
#ifdef OSS_GETVERSION
	CASE (OSS_GETVERSION)
#endif
#ifdef SOUND_MIXER_OUTSRC
	CASE (SOUND_MIXER_WRITE_OUTSRC)
	CASE (SOUND_MIXER_READ_OUTSRC)
#endif
#ifdef SOUND_MIXER_OUTMASK
	CASE (SOUND_MIXER_READ_OUTMASK)
#endif
	CASE (SOUND_MIXER_WRITE_RECSRC)
	CASE (SOUND_MIXER_READ_RECSRC)
	CASE (SOUND_MIXER_READ_DEVMASK)
	CASE (SOUND_MIXER_READ_RECMASK)
	CASE (SOUND_MIXER_READ_CAPS)
	CASE (SOUND_MIXER_READ_STEREODEVS)
		fprintf (fp, "0x%08x", * (unsigned int *) arg);
		break;

	default:
	{
		const char *rw = "?";
		unsigned int line;

		if (IOCTL_READ (cmd))
			rw = "READ";
		else if (IOCTL_READ_WRITE (cmd))
			rw = "WRITE";			

		line = IOCTL_NUMBER (cmd);

		if (line >= SOUND_MIXER_NRDEVICES) {
			fprintf_unknown_cmd (fp, cmd, arg);
			break;
		}

		if (arg)
			fprintf (fp, "L%03u R%03u",
				 (* (int *) arg) & 0xFF,
				 ((* (int *) arg) >> 8) & 0xFF);
		else
			fprintf (fp, "SOUND_MIXER_%s <%u %s>",
				 rw, line, dev_names[line]);
	}
	}
}

#define mixer_ioctl(fd, cmd, arg)					\
  (0 == device_ioctl (fd, cmd, arg, m->dev.log, fprintf_ioctl_arg))

struct line {
	tv_dev_mixer_line	dev;

	int			id;

	int			old_volume;		/* restore */
};

#define L(l) PARENT (l, struct line, dev)
#define LL(l) PARENT (l, struct line, dev.pub)

#define NOTIFY(l) tv_callback_notify (&l->dev.pub, l->dev.callback)

struct mixer {
	tv_dev_mixer		dev;

	int			fd;

	/*
	 *  .id, .name = mixer name, driver name, device brand
	 *  name or any combination, in any order. :-(
	 *  .modify_count = incremented on each write ioctl.
	 *  version = driver of OSS version 3.6.0 (0x030600)
	 *  or higher, 0 older version.
	 */
#ifdef SOUND_MIXER_INFO
	struct mixer_info	mixer_info;
#endif
	int			version;

	int			dev_mask;
	int			rec_mask;
	int			out_mask;
	int			stereo_mask;

	unsigned		has_recsrc	: 1;
	unsigned		has_outsrc	: 1;
	unsigned		rec_single	: 1;

	/*
	 *  recsrc: inputs to ADC (rec_single: one line, else mask)
	 *  outsrc: inputs to sum & output (mute function)
	 */
	int			old_recsrc;		/* restore */
	int			old_outsrc;
};

#define M(m) PARENT (m, struct mixer, dev)
#define MM(m) PARENT (m, struct mixer, dev.pub)

static tv_bool
volume_changed			(struct line *		l,
				 int			volume)
{
	unsigned int left, right;

	left = volume & 0xFF;
	right = (volume >> 8) & 0xFF; /* higher bits are undefined */

	/*
	 *  Scaling applies integer division rounding to zero
	 *  ret = (int)((int)(req * scale / 100) * 100 / scale),
	 *  this must be corrected to prevent error accumulation.
	 */
	left = MIN (left + 1, (unsigned int) VOL_MAX);

	if (!l->dev.pub.stereo)
		right = left;
	else
		right = MIN (right + 1, (unsigned int) VOL_MAX);

	if (l->dev.pub.volume[0] != left
	    || l->dev.pub.volume[1] != right) {
		l->dev.pub.volume[0] = left;
		l->dev.pub.volume[1] = right;
		return TRUE;
	}

	return FALSE;
}

static tv_bool
update_line			(struct mixer *		m,
				 struct line *		l,
				 int *			volume)
{
	*volume = 0;

	if (!mixer_ioctl (m->fd, MIXER_READ (l->id), volume))
		return FALSE;

	if (*volume == 0) {
		if (!l->dev.pub.muted) {
			l->dev.pub.muted = TRUE;
			NOTIFY(l);
		}
	} else {
		tv_bool muted = FALSE;

#ifdef SOUND_MIXER_OUTSRC

		if (m->has_outsrc && (m->out_mask & (1 << l->id))) {
			int outsrc; /* sic */

			if (!mixer_ioctl (m->fd, SOUND_MIXER_READ_OUTSRC, &outsrc))
				return FALSE;

			if (0 == (outsrc & (1 << l->id)))
				muted = TRUE;
		}
#endif
		if (volume_changed (l, *volume)
		    || l->dev.pub.muted != muted) {
			l->dev.pub.muted = muted;
			NOTIFY(l);
		}
	}

	return TRUE;
}

static tv_bool
oss_mixer_update_line		(tv_dev_mixer_line *	line)
{
	int volume; /* auxiliary result ignored */

	return update_line (M (line->mixer), L (line), &volume);
}

static tv_bool
oss_mixer_set_volume		(tv_dev_mixer_line *	line,
				 unsigned int		left,
				 unsigned int		right)
{
	struct mixer *m = M (line->mixer);
	struct line *l = L (line);
	int volume; /* sic */

	volume = left | (right << 8);

	if (!l->dev.pub.muted) {
		/* We don't know the current volume, so let's switch anyway. */

		if (!mixer_ioctl (m->fd, MIXER_WRITE (l->id), &volume))
			return FALSE;
	}

	/*
	 *  OSS scales volume to hardware resolution and back.
	 *  The actual volume returned by MIXER_WRITE is likely not
	 *  what was requested, and it's desirable the UI reflects this.
	 */
	if (volume_changed (l, volume))
		NOTIFY(l);

	return TRUE;
}

static tv_bool
oss_mixer_set_mute		(tv_dev_mixer_line *	line,
				 tv_bool		mute)
{
	struct line *l = L (line);
	struct mixer *m = M (l->dev.mixer);

	/*
	 *  Hopefully using the hardware mute switch will prevent an
	 *  ugly clicking noise, and really mute whereas volume zero
	 *  may be just very low.
	 */

#ifdef SOUND_MIXER_OUTSRC

	if (0 && m->has_outsrc && (m->out_mask & (1 << l->id))) {
		int outsrc; /* sic */

#warning test me with SB16.

		/* We don't know the current outsrc, so let's switch anyway. */

		if (!mixer_ioctl (m->fd, SOUND_MIXER_READ_OUTSRC, &outsrc))
			return FALSE;

		if (mute)
			outsrc &= ~(1 << l->id);
		else
			outsrc |= 1 << l->id;

		if (!mixer_ioctl (m->fd, SOUND_MIXER_WRITE_OUTSRC, &outsrc))
			return FALSE;

		if (l->dev.pub.muted != mute) {
			l->dev.pub.muted = mute;
			NOTIFY(l);
		}
	} else
#endif
	{
		int volume; /* sic */

		if (mute)
			volume = 0;
		else
			volume = l->dev.pub.volume[0] | (l->dev.pub.volume[1] << 8);

		/* We don't know the current volume, so let's switch anyway. */

		if (!mixer_ioctl (m->fd, MIXER_WRITE (l->id), &volume))
			return FALSE;

		if ((!mute && volume_changed (l, volume))
		    || l->dev.pub.muted != mute) {
			l->dev.pub.muted = mute;
			NOTIFY(l);
		}
	}

	return TRUE;
}

static tv_mixer_line *
find_rec_line			(tv_mixer *		m,
				 unsigned int		set)
{
	struct line *l;

	for (l = LL (m->inputs); l; l = LL (l->dev.pub.next)) {
		if (set & (1 << l->id))
			return &l->dev.pub;
	}

	fprintf (stderr, "%s: Unknown recording source 0x%08x reported\n",
		 __FILE__, set);

	return NULL;
}

static tv_bool
rec_source_changed		(struct mixer *		m,
				 unsigned int		set)
{
	if (NULL != m->dev.pub.rec_line) {
		unsigned int mask = 1 << LL (m->dev.pub.rec_line)->id;

		if (0 == set) {
			m->dev.pub.rec_line = NULL;
			return TRUE;
		} else if (0 == (set & mask)) {
			m->dev.pub.rec_line = find_rec_line (&m->dev.pub, set);
			return TRUE;
		}
	} else {
		if (0 != set) {
			m->dev.pub.rec_line = find_rec_line (&m->dev.pub, set);
			return (NULL != m->dev.pub.rec_line);
		}
	}

	return FALSE;
}

static tv_bool
oss_mixer_update_mixer		(tv_dev_mixer *		mixer)
{
	struct mixer *m = M (mixer);
	int set; /* sic */

	if (m->has_recsrc) {
		if (!mixer_ioctl (m->fd, SOUND_MIXER_READ_RECSRC, &set))
			return FALSE;

		if (rec_source_changed (m, set))
			tv_callback_notify (&m->dev.pub, m->dev.callback);
	}

	return TRUE;
}

static tv_bool
oss_mixer_set_rec_line		(tv_dev_mixer *		mixer,
				 tv_dev_mixer_line *	line,
				 tv_bool		exclusive)
{
	struct mixer *m = M (mixer);
	int set; /* sic */

	if (!m->has_recsrc)
		return FALSE;

	set = 0;

	if (!(exclusive | m->rec_single)) {
		if (!mixer_ioctl (m->fd, SOUND_MIXER_READ_RECSRC, &set))
			return FALSE;

		if (m->dev.pub.rec_line)
			set &= ~ LL (m->dev.pub.rec_line)->id;
	}

	if (line)
		set |= L (line)->id;

	/* We don't know the current rec source, so let's switch anyway. */

	if (!mixer_ioctl (m->fd, SOUND_MIXER_WRITE_RECSRC, &set))
		return FALSE;

	/* NB the driver will never return set = 0, it defaults to Mic. */

	if (rec_source_changed (m, set))
		tv_callback_notify (&m->dev.pub, m->dev.callback);

	return TRUE;
}

static void
free_mixer_lines		(struct mixer *		m,
				 tv_mixer_line *	line,
				 tv_bool		restore)
{
	while (line) {
		struct line *l;
	
		l = LL (line);
		line = l->dev.pub.next;

		tv_callback_destroy (&l->dev.pub, &l->dev.callback);

		if (restore) {
			/* Error ignored */
			mixer_ioctl (m->fd, MIXER_WRITE (l->id), &l->old_volume);
		}

		free ((char *) l->dev.pub.label);
		free (l);
	}
}

static void
oss_mixer_destroy		(tv_dev_mixer *		mixer,
				 tv_bool		restore)
{
	struct mixer *m;
	int saved_errno;

	if (!mixer)
		return;

	m = M (mixer);

	saved_errno = errno;

	tv_callback_destroy (&m->dev.pub, &m->dev.callback);

	if (restore) {
		/* Error ignored */
		mixer_ioctl (m->fd, SOUND_MIXER_WRITE_RECSRC, &m->old_recsrc);

#ifdef SOUND_MIXER_OUTSRC
		/* Error ignored */
		mixer_ioctl (m->fd, SOUND_MIXER_WRITE_OUTSRC, &m->old_outsrc);
#endif
	}

	free ((char *) m->dev.pub.name);
	free ((char *) m->dev.pub.device);

	free_mixer_lines (m, m->dev.pub.inputs, restore);
	free_mixer_lines (m, m->dev.pub.rec_gain, restore);
	free_mixer_lines (m, m->dev.pub.play_gain, restore);

	if (m->fd >= 0)
		close (m->fd);

	CLEAR (*m);

	free (m);

	errno = saved_errno;
}

static tv_bool
add_mixer_line			(struct mixer *		m,
				 tv_mixer_line **	linepp,
				 unsigned int		oss_id)
{
	struct line *l;

	if (!(l = calloc (1, sizeof (*l))))
		return FALSE;

	while (*linepp)
		linepp = &(*linepp)->next;

	*linepp = &l->dev.pub;

	l->id = oss_id;

	l->dev.mixer = &m->dev;

	l->dev.pub.id = TV_MIXER_LINE_ID_UNKNOWN;

	l->dev.pub.stereo = ((m->stereo_mask & (1 << oss_id)) != 0);
	l->dev.pub.recordable = ((m->rec_mask & (1 << oss_id)) != 0);

	if (m->stereo_mask && !l->dev.pub.stereo) {
		char buf[80];

		snprintf (buf, sizeof (buf) - 1,
			  /* TRANSLATORS: Mixer line, mono as opposed to stereo. */
			  _("%s (Mono)"), dev_names[oss_id]);

		if (!(l->dev.pub.label = strdup (buf)))
			return FALSE;
	} else {
		if (!(l->dev.pub.label = strdup (dev_names[oss_id])));
			return FALSE;
	}

	l->dev.pub.minimum = VOL_MIN;
	l->dev.pub.maximum = VOL_MAX;
	l->dev.pub.step = 1; /* or so */
	l->dev.pub.reset = 67; /* ok for most */

	update_line (m, l, &l->old_volume);

	return TRUE;
}

tv_dev_mixer *
oss_mixer_open			(tv_dev_mixer_interface *mi,
				 const char *		dev_name,
				 FILE *			log)
{
	struct mixer *m;
	int saved_errno;
	int capabilities; /* sic */
	unsigned int inputs;
	unsigned int i;

	if (!(m = calloc (1, sizeof (*m))))
		goto failure;

	m->fd = open (dev_name, O_RDWR, 0);

	if (log) {
		saved_errno = errno;

		m->dev.log = log;

		fprintf (m->dev.log, "%s: %d = open (\"%s\", O_RDWR, 0)",
			 __FILE__, m->fd, dev_name);

		if (m->fd == -1)
			fprintf (m->dev.log, " errno=%d %s",
				 saved_errno, strerror (saved_errno));

		fputc ('\n', m->dev.log);

		errno = saved_errno;
	}

	if (m->fd < 0)
		goto failure;

#ifdef SOUND_MIXER_INFO

	if (!mixer_ioctl (m->fd, SOUND_MIXER_INFO, &m->mixer_info)) {
		if (errno == EINVAL) {
			/* Not Open Sound System device */
		} else if (errno == ENXIO) {
			/* OSS installed but no hardware. */
		}

		goto failure;
	}

	/* Will you please stick to C conventions. */
	m->mixer_info.id [N_ELEMENTS (m->mixer_info.id) - 1] = 0;
	m->mixer_info.name [N_ELEMENTS (m->mixer_info.name) - 1] = 0;

#endif

#ifdef OSS_GETVERSION
	/* Introduced in OSS 3.6, error ignored */
	mixer_ioctl (m->fd, OSS_GETVERSION, &m->version);
#endif

	if (!mixer_ioctl (m->fd, SOUND_MIXER_READ_DEVMASK, &m->dev_mask))
		goto failure;

	/* Error ignored */
	mixer_ioctl (m->fd, SOUND_MIXER_READ_STEREODEVS, &m->stereo_mask);

	if (!mixer_ioctl (m->fd, SOUND_MIXER_READ_CAPS, &capabilities)
	    || (capabilities & SOUND_CAP_EXCL_INPUT))
		m->rec_single = TRUE;

	if (mixer_ioctl (m->fd, SOUND_MIXER_READ_RECMASK, &m->rec_mask)
	    && mixer_ioctl (m->fd, SOUND_MIXER_READ_RECSRC, &m->old_recsrc))
		m->has_recsrc = TRUE;

#if defined (SOUND_MIXER_OUTMASK) && defined (SOUND_MIXER_OUTSRC)

	if (mixer_ioctl (m->fd, SOUND_MIXER_READ_OUTMASK, &m->out_mask)
	    && mixer_ioctl (m->fd, SOUND_MIXER_READ_OUTSRC, &m->old_outsrc))
		m->has_outsrc = TRUE;
#endif

	if (m->has_recsrc && m->has_outsrc) {
		/* Our basic assumption. */
		inputs = m->rec_mask & m->out_mask;
	} else if (m->has_recsrc) {
		/* Guess. */
		inputs = m->rec_mask;
	} else {
		inputs = SOUND_MASK_LINE | SOUND_MASK_MIC |
			SOUND_MASK_CD | SOUND_MASK_PHONEIN |
			SOUND_MASK_VIDEO | SOUND_MASK_RADIO;
		/*
		 *  soundcard.h suggests inputs, but some drivers
		 *  use them for outputs. :-(
		 */
		inputs |= SOUND_MASK_LINE1 | SOUND_MASK_LINE2 |
			SOUND_MASK_LINE3 | SOUND_MASK_DIGITAL1 |
			SOUND_MASK_DIGITAL2 | SOUND_MASK_DIGITAL3;
	}

#ifdef SOUND_MIXER_INFO

	if (m->mixer_info.name[0]) {
		if (!(m->dev.pub.name = strdup (m->mixer_info.name)))
			goto failure;
	} else
#endif
	{
		if (!(m->dev.pub.name = strdup (dev_name)))
			goto failure;
	}

	/* XXX */
	if (!(m->dev.pub.device = strdup (dev_name)))
		goto failure;

	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
		if (0 == (m->dev_mask & (1 << i)))
			continue;

		if (SOUND_MIXER_IGAIN == i) {
			if (!add_mixer_line (m, &m->dev.pub.rec_gain, i))
				goto failure;
			continue;
		}

		if (SOUND_MIXER_PCM == i) {
			if (!add_mixer_line (m, &m->dev.pub.play_gain, i))
				goto failure;
			continue;
		}

		if (inputs & (1 << i)) {
			if (!add_mixer_line (m,	&m->dev.pub.inputs, i))
				goto failure;
		}
	}

	if (m->has_recsrc)
		m->dev.pub.rec_line = find_rec_line (&m->dev.pub, m->old_recsrc);

	m->dev.update_line	= oss_mixer_update_line;
	m->dev.set_volume	= oss_mixer_set_volume;
	m->dev.set_mute		= oss_mixer_set_mute;
	m->dev.set_rec_line	= oss_mixer_set_rec_line;

	m->dev.update_mixer	= oss_mixer_update_mixer;
	m->dev.destroy		= oss_mixer_destroy;

	return &m->dev;

 failure:
	if (m)
		oss_mixer_destroy (&m->dev, FALSE);

	return NULL;
}

static tv_dev_mixer *
oss_mixer_scan			(tv_dev_mixer_interface *mi)
{
	return NULL;
/*
	find & open all oss mixer devices in /dev,
	take care to eliminate duplicates (symlink, different names)
 */
}

const tv_dev_mixer_interface
oss_mixer_interface = {
	.name 		= "Open Sound System",

	.open		= oss_mixer_open,
	.scan		= oss_mixer_scan,
};

#endif /* OSS backend */
