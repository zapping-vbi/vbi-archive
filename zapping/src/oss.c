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

#include "libtv/misc.h"

/** OSS backend **/
#ifdef HAVE_OSS

#include <gnome.h>
#include <math.h>
#include <unistd.h>

#include "common/fifo.h" /* current_time() */

#include "audio.h"
#define ZCONF_DOMAIN "/zapping/options/audio/"
#include "zconf.h"
#include "interface.h"
#include "globals.h"

#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include "tveng_private.h"
#include "common/device.h"


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
oss_open (gboolean stereo, guint rate, enum audio_format format)
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
oss_read (gpointer handle, gpointer dest, guint num_bytes,
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
      r = read(h->fd, data, (size_t) n);
		
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

/* Preliminary */

struct pcm {
	tv_device_node		node;
	int			fd;
	FILE *			_log;
};

static void
destroy_pcm			(tv_device_node *	n,
				 tv_bool		restore)
{
	struct pcm *p;
	int saved_errno;

	if (!n)
		return;

	p = PARENT (n, struct pcm, node);

	saved_errno = errno;

	if (restore) {
		/* Blah. */
	}

	free ((char *) p->node.device);
	free ((char *) p->node.version);
	free ((char *) p->node.driver);
	free ((char *) p->node.label);

	if (p->fd >= 0)
		device_close (p->_log, p->fd);

	CLEAR (*p);

	free (p);

	errno = saved_errno;
}

static struct pcm *
open_pcm			(void *			unused _unused_,
				 FILE *			log,
				 const char *		device)
{
	struct stat st;
	struct pcm *p;

	/* if (OSS_LOG_FP)
	   log = OSS_LOG_FP; */

	if (-1 == stat (device, &st))
		return NULL;

	/* Don't accidentally overwrite a regular file. */
	if (!S_ISCHR (st.st_mode)) {
		errno = EINVAL;
		return NULL;
	}

	/* Check minor number here? */

	if (!(p = calloc (1, sizeof (*p))))
		goto error;

	p->node.destroy = destroy_pcm;
	p->_log = log;

	p->fd = device_open (p->_log, device, O_RDWR, 0);

	if (-1 == p->fd)
		goto error;

	if (!(p->node.device = strdup (device)))
		goto error;

#ifdef OSS_GETVERSION
	{
		int version;

		/* Introduced in OSS 3.6, error ignored */
		/* XXX use ioctl wrapper */
		ioctl (p->fd, OSS_GETVERSION, &version);

		if (version > 0) {
			if (_tv_asprintf (&p->node.version,
					  "OSS %u.%u.%u",
					  (version >> 16) & 0xFF,
					  (version >> 8) & 0xFF,
					  (version >> 0) & 0xFF) < 0)
				goto error;
		}
	}
#endif

	/* That's it. No other information available.
	   Maybe if we could somehow link mixer and pcm device? */

	return p;

 error:
	destroy_pcm (&p->node, FALSE);
	return NULL;
}
	
tv_device_node *
oss_pcm_open			(void *			unused _unused_,
				 FILE *			log, 
				 const char *		dev_name)
{
	struct pcm *p;

	if ((p = open_pcm (NULL, log, dev_name)))
		return &p->node;

	return NULL;
}

tv_device_node *
oss_pcm_scan			(void *			unused _unused_,
				 FILE *			log)
{
	static const char *pcm_devices [] = {
		"/dev/dsp",
		"/dev/dsp0",
		"/dev/dsp1",
		"/dev/dsp2",
		"/dev/dsp3",
	};
	tv_device_node *list = NULL;
	const char **sp;

	for (sp = pcm_devices; *sp; ++sp) {
		if (!tv_device_node_find (list, *sp)) {
			struct pcm *p;

			if ((p = open_pcm (NULL, log, *sp))) {
				tv_device_node_add (&list, &p->node);
			}
		}
	}

	return list;
}













/* XXX check for freebsd quirks */

#ifndef OSS_LOG_FP
#define OSS_LOG_FP 0 /* stderr */
#endif

#define VOL_MIN 1	/* 0 = muted */
#define VOL_MAX 100

#ifndef SOUND_MASK_PHONEIN
#define SOUND_MASK_PHONEIN 0
#endif

#ifndef SOUND_MIXER_READ_OUTMASK
#define SOUND_MIXER_READ_OUTMASK MIXER_READ (SOUND_MIXER_OUTMASK)
#endif

#ifndef SOUND_MIXER_READ_OUTSRC
#define SOUND_MIXER_READ_OUTSRC MIXER_READ (SOUND_MIXER_OUTSRC)
#endif

#ifndef SOUND_MIXER_WRITE_OUTSRC
#define SOUND_MIXER_WRITE_OUTSRC MIXER_WRITE (SOUND_MIXER_OUTSRC)
#endif

static const char *dev_names [] = SOUND_DEVICE_LABELS;

static const unsigned int INPUTS =
	SOUND_MASK_LINE | SOUND_MASK_MIC |
	SOUND_MASK_CD | SOUND_MASK_PHONEIN |
	SOUND_MASK_VIDEO | SOUND_MASK_RADIO |
	/*
	 *  soundcard.h suggests inputs, but some drivers
	 *  use them for outputs. :-(
	 */
	SOUND_MASK_LINE1 | SOUND_MASK_LINE2 |
	SOUND_MASK_LINE3 | SOUND_MASK_DIGITAL1 |
	SOUND_MASK_DIGITAL2 | SOUND_MASK_DIGITAL3;

#define CASE(x) case x: if (!arg) { fputs (# x, fp); return; }

static void
fprintf_ioctl_arg		(FILE *			fp,
				 unsigned int		cmd,
				 int			rw _unused_,
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

		/*
		 *  Note: MIXER_WRITE() is really write-read, but it
		 *  just returns the argument, not the actual volume.
		 */
		if (IOCTL_READ_WRITE (cmd))
			rw = "WRITE";			
		else if (IOCTL_READ (cmd))
			rw = "READ";


		line = IOCTL_NUMBER (cmd);

		if (line >= SOUND_MIXER_NRDEVICES) {
			fprint_unknown_ioctl (fp, cmd, arg);
			break;
		}

		if (arg)
			fprintf (fp, "L%03u R%03u",
				 (* (int *) arg) & 0xFF,
				 ((* (int *) arg) >> 8) & 0xFF);
		else
			fprintf (fp, "SOUND_MIXER_%s<%u %s>",
				 rw, line, dev_names[line]);
	}
	}
}

#define mixer_ioctl(fd, cmd, arg)					\
  (0 == device_ioctl (m->pub._log, fprintf_ioctl_arg, fd, cmd, arg))

struct line {
	tv_audio_line		pub;
	int			id;
	int			old_volume;		/* restore */
};

#define L(l) PARENT (l, struct line, pub)

#define NOTIFY(l) tv_callback_notify (NULL, &l->pub, l->pub._callback)

struct mixer {
	tv_mixer		pub;

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

#define M(m) PARENT (m, struct mixer, pub)

#define HARD_MUTABLE(m, l) ((m)->has_outsrc && ((m)->out_mask & (1 << (l)->id)))

static tv_bool
update_line			(struct mixer *		m,
				 struct line *		l,
				 int *			volume)
{
	int left, right;
	tv_bool muted;

	*volume = 0;

	if (!mixer_ioctl (m->fd, MIXER_READ (l->id), volume))
		return FALSE;

#ifdef SOUND_MIXER_OUTSRC

	if (HARD_MUTABLE (m, l)) {
		int outsrc; /* sic */

		if (!mixer_ioctl (m->fd, SOUND_MIXER_READ_OUTSRC, &outsrc))
			return FALSE;

		muted = (0 == (outsrc & (1 << l->id)));
	} else
#endif
	{
		if ((muted = (*volume == 0))) {
			if (l->pub.muted != muted) {
				l->pub.muted = muted;
				NOTIFY(l);
			}

			return TRUE;
		}
	}

	left = *volume & 0xFF;
	/* left = left ? MIN (left + 1, (unsigned int) VOL_MAX) : 0; */

	if (!l->pub.stereo) {
		right = (*volume >> 8) & 0xFF; /* higher bits are undefined */
		/* right = right ? MIN (right + 1, (unsigned int) VOL_MAX) : 0; */
	} else {
		right = left;
	}

	if (l->pub.volume[0] != left
	    || l->pub.volume[1] != right
	    || l->pub.muted != muted) {
		l->pub.volume[0] = left;
		l->pub.volume[1] = right;
		l->pub.muted = muted;
		NOTIFY(l);
	}

	return TRUE;
}

static tv_bool
oss_mixer_update_line		(tv_audio_line *	line)
{
	int volume; /* auxiliary result ignored */

	return update_line (M ((tv_mixer *) line->_parent),
			    L (line), &volume);
}

static tv_bool
oss_mixer_set_volume		(tv_audio_line *	line,
				 int			left,
				 int			right)
{
	struct mixer *m = M ((tv_mixer *) line->_parent);
	struct line *l = L (line);

	if (!l->pub.muted || HARD_MUTABLE (m, l)) {
		int volume; /* sic */

		volume = left | (right << 8);

		/* We don't know the current volume, so let's switch anyway. */

		if (!mixer_ioctl (m->fd, MIXER_WRITE (l->id), &volume))
			return FALSE;

		/*
		 *  OSS scales volume to hardware resolution and back.
		 *  The actual volume returned by MIXER_WRITE is likely not
		 *  what was requested. It's desirable the UI reflects this.
		 */
		return update_line (m, l, &volume);
	} else {
		if (l->pub.volume[0] != left
		    || l->pub.volume[1] != right) {
			l->pub.volume[0] = left;
			l->pub.volume[1] = right;
			NOTIFY(l);
		}

		return TRUE;
	}
}

static tv_bool
oss_mixer_set_mute		(tv_audio_line *	line,
				 tv_bool		mute)
{
	struct line *l = L (line);
	struct mixer *m = M ((tv_mixer *) line->_parent);

	/*
	 *  Hopefully using the hardware mute switch will prevent an
	 *  ugly clicking noise, and really mute whereas volume zero
	 *  may be just very low.
	 */

#ifdef SOUND_MIXER_OUTSRC

	if (HARD_MUTABLE (m, l)) {
		int outsrc; /* sic */

		/* We don't know the current outsrc, so let's switch anyway. */

		if (!mixer_ioctl (m->fd, SOUND_MIXER_READ_OUTSRC, &outsrc))
			return FALSE;

		if (mute)
			outsrc &= ~(1 << l->id);
		else
			outsrc |= 1 << l->id;

		if (!mixer_ioctl (m->fd, SOUND_MIXER_WRITE_OUTSRC, &outsrc))
			return FALSE;

		if (l->pub.muted != mute) {
			l->pub.muted = mute;
			NOTIFY(l);
		}
	} else
#endif
	{
		int volume; /* sic */

		if (mute)
			volume = 0;
		else
			volume = l->pub.volume[0] | (l->pub.volume[1] << 8);

		/* We don't know the current volume, so let's switch anyway. */

		if (!mixer_ioctl (m->fd, MIXER_WRITE (l->id), &volume))
			return FALSE;

		if (l->pub.muted != mute) {
			l->pub.muted = mute;
			NOTIFY(l);
		}
	}

	return TRUE;
}

static tv_audio_line *
find_rec_line			(tv_mixer *		m,
				 int			set)
{
	struct line * l;

	for (l = L (m->inputs); l; l = L (l->pub._next)) {
		if (set & (1 << l->id))
			return &l->pub;
	}

	fprintf (stderr, "%s: Unknown recording source 0x%08x reported\n",
		 __FILE__, set);

	return NULL;
}

static tv_bool
rec_source_changed		(struct mixer *		m,
				 int			set)
{
	if (NULL != m->pub.rec_line) {
		int mask = 1 << L (m->pub.rec_line)->id;

		if (0 == set) {
			m->pub.rec_line = NULL;
			return TRUE;
		} else if (0 == (set & mask)) {
			m->pub.rec_line = find_rec_line (&m->pub, set);
			return TRUE;
		}
	} else {
		if (0 != set) {
			m->pub.rec_line = find_rec_line (&m->pub, set);
			return (NULL != m->pub.rec_line);
		}
	}

	return FALSE;
}

static tv_bool
oss_mixer_update_mixer		(tv_mixer *		mixer)
{
	struct mixer *m = M (mixer);
	int set; /* sic */

	if (m->has_recsrc) {
		if (!mixer_ioctl (m->fd, SOUND_MIXER_READ_RECSRC, &set))
			return FALSE;

		if (rec_source_changed (m, set))
			tv_callback_notify (NULL, &m->pub, m->pub._callback);
	}

	return TRUE;
}

static tv_bool
oss_mixer_set_rec_line		(tv_mixer *		mixer,
				 tv_audio_line *	line,
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

		if (m->pub.rec_line)
			set &= ~(1 << L (m->pub.rec_line)->id);
	}

	if (line)
		set |= 1 << L (line)->id;

	/* We don't know the current rec source, so let's switch anyway. */

	if (!mixer_ioctl (m->fd, SOUND_MIXER_WRITE_RECSRC, &set))
		return FALSE;

	/* NB the driver will never return set = 0, it defaults to Mic. */

	if (rec_source_changed (m, set))
		tv_callback_notify (NULL, &m->pub, m->pub._callback);

	return TRUE;
}

static void
free_mixer_lines		(struct mixer *		m,
				 tv_audio_line *	line,
				 tv_bool		restore)
{
	while (line) {
		struct line *l;
	
		l = L (line);
		line = l->pub._next;

		tv_callback_delete_all (l->pub._callback, 0, 0, 0, &l->pub);

		if (restore) {
			/* Error ignored */
			mixer_ioctl (m->fd, MIXER_WRITE (l->id),
				     &l->old_volume);
		}

		free ((char *) l->pub.label);
		free (l);
	}
}

static void
destroy_mixer			(tv_device_node *	n,
				 tv_bool		restore)
{
	struct mixer *m;
	int saved_errno;

	if (!n)
		return;

	m = PARENT (n, struct mixer, pub.node);

	saved_errno = errno;

	tv_callback_delete_all (m->pub._callback, 0, 0, 0, &m->pub);

	if (restore) {
		/* Error ignored */
		mixer_ioctl (m->fd, SOUND_MIXER_WRITE_RECSRC, &m->old_recsrc);

#ifdef SOUND_MIXER_OUTSRC
		/* Error ignored */
		mixer_ioctl (m->fd, SOUND_MIXER_WRITE_OUTSRC, &m->old_outsrc);
#endif
	}

	free ((char *) m->pub.node.device);
	free ((char *) m->pub.node.version);
	free ((char *) m->pub.node.driver);
	free ((char *) m->pub.node.label);

	free_mixer_lines (m, m->pub.inputs, restore);
	free_mixer_lines (m, m->pub.rec_gain, restore);
	free_mixer_lines (m, m->pub.play_gain, restore);

	if (m->fd >= 0)
		device_close (m->pub._log, m->fd);

	CLEAR (*m);

	free (m);

	errno = saved_errno;
}

static tv_bool
add_mixer_line			(struct mixer *		m,
				 tv_audio_line **	linepp,
				 unsigned int		oss_id)
{
	struct line *l;

	if (!(l = calloc (1, sizeof (*l))))
		return FALSE;

	while (*linepp)
		linepp = &(*linepp)->_next;

	*linepp = &l->pub;

	l->id = oss_id;

	l->pub._parent = &m->pub;

	l->pub.id = TV_AUDIO_LINE_ID_UNKNOWN;
	l->pub.hash = oss_id;

	l->pub.stereo = ((m->stereo_mask & (1 << oss_id)) != 0);
	l->pub.recordable = ((m->rec_mask & (1 << oss_id)) != 0);

	if (m->stereo_mask && !l->pub.stereo) {
		char buf[80];

		snprintf (buf, sizeof (buf) - 1,
			  /* TRANSLATORS: Name of mixer line, mono as
			     opposed to stereo. */
			  _("%s (Mono)"), dev_names[oss_id]);

		if (!(l->pub.label = strdup (buf)))
			return FALSE;
	} else {
		if (!(l->pub.label = strdup (dev_names[oss_id])))
			return FALSE;
	}

	l->pub.minimum	= VOL_MIN;
	l->pub.maximum	= VOL_MAX;
	l->pub.step	= 4;		/* or so, could be probed */
	l->pub.reset	= 80;		/* or so */

	update_line (m, l, &l->old_volume);

	return TRUE;
}

static struct mixer *
open_mixer			(const tv_mixer_interface *mi,
				 FILE *			log,
				 const char *		device)
{
	struct stat st;
	struct mixer *m;
	int capabilities; /* sic */
	unsigned int i;

	if (OSS_LOG_FP)
		log = OSS_LOG_FP;

	if (-1 == stat (device, &st))
		return NULL;

	/* Don't accidentally overwrite a regular file. */
	if (!S_ISCHR (st.st_mode)) {
		errno = EINVAL;
		return NULL;
	}

	/* Mixers have minor n*16, major on Linux 14, FreeBSD 30. Check? */

	if (!(m = calloc (1, sizeof (*m))))
		goto error;

	m->pub.node.destroy = destroy_mixer;
	m->pub._log = log;
	m->pub._interface = mi;

	m->fd = device_open (m->pub._log, device, O_RDWR, 0);

	if (-1 == m->fd)
		goto error;

	if (!(m->pub.node.device = strdup (device)))
		goto error;

#ifdef SOUND_MIXER_INFO
	if (mixer_ioctl (m->fd, SOUND_MIXER_INFO, &m->mixer_info)) {
		if (m->mixer_info.name[0]) {
			m->pub.node.label =
				_tv_strndup (m->mixer_info.name,
					     N_ELEMENTS (m->mixer_info.name));

			if (!m->pub.node.label)
				goto error;
		}

		if (m->mixer_info.id[0]) {
			m->pub.node.driver =
				_tv_strndup (m->mixer_info.id,
					     N_ELEMENTS (m->mixer_info.id));

			if (!m->pub.node.driver)
				goto error;
		}

		/* Will you please stick to C conventions. */
		m->mixer_info.id [N_ELEMENTS (m->mixer_info.id) - 1] = 0;
		m->mixer_info.name [N_ELEMENTS (m->mixer_info.name) - 1] = 0;
	} else {
		if (errno == EINVAL) {
			/* Probably not Open Sound System device, or something
			   ancient. DEVMASK below should be conclusive.*/
		} else if (errno == ENXIO) {
			/* OSS installed but no hardware. */
		} else {
			/* ? */
			goto error;
		}
	}
#endif

#ifdef OSS_GETVERSION
	/* Introduced in OSS 3.6, error ignored */
	mixer_ioctl (m->fd, OSS_GETVERSION, &m->version);

	if (m->version > 0) {
		if (_tv_asprintf (&m->pub.node.version,
				  "OSS %u.%u.%u",
				  (m->version >> 16) & 0xFF,
				  (m->version >> 8) & 0xFF,
				  (m->version >> 0) & 0xFF) < 0)
			goto error;
	}
#endif

	if (!mixer_ioctl (m->fd, SOUND_MIXER_READ_DEVMASK, &m->dev_mask))
		goto error;

	/* Error ignored */
	mixer_ioctl (m->fd, SOUND_MIXER_READ_STEREODEVS, &m->stereo_mask);

	if (!mixer_ioctl (m->fd, SOUND_MIXER_READ_CAPS, &capabilities)
	    || (capabilities & SOUND_CAP_EXCL_INPUT))
		m->rec_single = TRUE;

	if (mixer_ioctl (m->fd, SOUND_MIXER_READ_RECMASK, &m->rec_mask)
	    && mixer_ioctl (m->fd, SOUND_MIXER_READ_RECSRC, &m->old_recsrc))
		m->has_recsrc = TRUE;

#if defined (SOUND_MIXER_OUTMASK) && defined (SOUND_MIXER_OUTSRC)
#if 0 /* XXX test me with SB16. */
	if (mixer_ioctl (m->fd, SOUND_MIXER_READ_OUTMASK, &m->out_mask)
	    && mixer_ioctl (m->fd, SOUND_MIXER_READ_OUTSRC, &m->old_outsrc))
		m->has_outsrc = TRUE;
#endif
#endif

	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++) {
		if (0 == (m->dev_mask & (1 << i)))
			continue;

		if (SOUND_MIXER_IGAIN == i) {
			if (!add_mixer_line (m, &m->pub.rec_gain, i))
				goto error;
			continue;
		}

		if (SOUND_MIXER_PCM == i) {
			if (!add_mixer_line (m, &m->pub.play_gain, i))
				goto error;
			continue;
		}

		if (INPUTS & (1 << i)) {
			if (!add_mixer_line (m,	&m->pub.inputs, i))
				goto error;
		}
	}

	if (m->has_recsrc && 0 != m->old_recsrc)
		m->pub.rec_line = find_rec_line (&m->pub, m->old_recsrc);

	return m;

 error:
	if (m)
		destroy_mixer (&m->pub.node, FALSE);

	return NULL;
}

static tv_mixer *
oss_mixer_open			(const tv_mixer_interface *mi,
				 FILE *			log, 
				 const char *		dev_name)
{
	struct mixer *m;

	if ((m = open_mixer (mi, log, dev_name)))
		return &m->pub;

	return NULL;
}

static tv_mixer *
oss_mixer_scan			(const tv_mixer_interface *mi,
				 FILE *			log)
{
	static const char *mixer_devices [] = {
		"/dev/mixer",
		"/dev/mixer0",
		"/dev/mixer1",
		"/dev/mixer2",
		"/dev/mixer3",
	};
	tv_device_node *list = NULL;
	const char **sp;

	for (sp = mixer_devices; *sp; sp++) {
		if (!tv_device_node_find (list, *sp)) {
			struct mixer *m;

			if ((m = open_mixer (mi, log, *sp))) {
				tv_device_node_add (&list, &m->pub.node);
			}
		}
	}

	return list ? PARENT (list, tv_mixer, node) : NULL;
}

const tv_mixer_interface
oss_mixer_interface = {
	.name 		= "Open Sound System",

	.open		= oss_mixer_open,
	.scan		= oss_mixer_scan,

	.update_line	= oss_mixer_update_line,
	.set_volume	= oss_mixer_set_volume,
	.set_mute	= oss_mixer_set_mute,
	.set_rec_line	= oss_mixer_set_rec_line,

	.update_mixer	= oss_mixer_update_mixer,
};

#endif /* OSS backend */
