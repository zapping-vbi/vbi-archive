/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
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
#include <common/fifo.h> // current_time
#include <math.h>
#include <unistd.h>

#include "audio.h"
#define ZCONF_DOMAIN "/zapping/options/audio/"
#include "zconf.h"
#include "properties.h"
#include "interface.h"
#include "zmisc.h"

#include <sys/ioctl.h>
#include <sys/soundcard.h>

typedef struct {
  int		fd;
  int		stereo;
  int		sampling_rate;
} oss_handle;

/* TFR repeats the ioctl when interrupted (EINTR) */
#define IOCTL(fd, cmd, data) (TEMP_FAILURE_RETRY(ioctl(fd, cmd, data)))

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

  h = g_malloc0(sizeof(*h));
  h->fd = oss_fd;
  h->stereo = stereo;
  h->sampling_rate = rate;
  
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
  oss_handle *h = (oss_handle*)h;
  int stereo = h->stereo;
  int sampling_rate = h->sampling_rate;
  ssize_t r, n = num_bytes;
  struct audio_buf_info info;
  char *data = dest;
  int oss_fd = h->fd;

  while (n > 0)
    {
      r = read(oss_fd, data, n);
		
      if (r < 0 && errno == EINTR)
	continue;

      if (r == 0)
        {
	  memset(data, 0, n);
	  break;
	}
	
      g_assert(r > 0 && "OSS read failed");

      data = (char *) data + r;
      n -= r;
    }

  *timestamp = current_time();

  if (IOCTL(oss_fd, SNDCTL_DSP_GETISPACE, &info) == -1)
    g_assert(!"SNDCTL_DSP_GETISPACE failed");

  *timestamp -= (((num_bytes - (n+info.bytes))/sizeof(short))>>stereo)
    / (double) sampling_rate;
}

static void
oss_add_props (GtkBox *vbox, GnomePropertyBox *gpb)
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
  gtk_object_set_data(GTK_OBJECT(vbox), "fentry", fentry);
  gtk_signal_connect_object(GTK_OBJECT(entry), "changed",
		     GTK_SIGNAL_FUNC(gnome_property_box_changed),
		     (GtkObject*)gpb);

  gtk_widget_show_all(hbox);
  gtk_box_pack_start_defaults(vbox, hbox);
}

static void
oss_apply_props (GtkBox *vbox)
{
  GnomeFileEntry *fentry =
    GNOME_FILE_ENTRY(gtk_object_get_data(GTK_OBJECT(vbox), "fentry"));
  gchar *result = gnome_file_entry_get_full_path(fentry, TRUE);

  if (!result)
    {
      gchar *real_text =
	gtk_entry_get_text(GTK_ENTRY(gnome_file_entry_gtk_entry(fentry)));

      ShowBox(_("The given audio device \"%s\" doesn't exist"),
	      GNOME_MESSAGE_BOX_WARNING, real_text);
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
  apply_props:	oss_apply_props,
};

#endif /* OSS backend */
