#ifndef __AUDIO_H__
#define __AUDIO_H__

#include <gtk/gtk.h>
#include "tveng.h"

enum audio_format {
  /* More to come when we need more */
  AUDIO_FORMAT_S16_LE
};

typedef struct _mhandle mhandle;

/**
 * Opens the configured audio device and returns an opaque handler to
 * it, or %NULL on error.
 * @stereo: %TRUE if the device should be opened in stereo mode.
 * @rate: Samples per second (typically 44100)
 * @format: Audio format to use.
 */
mhandle *
open_audio_device (gboolean stereo, guint rate, enum audio_format format);

/**
 * Closes @handle.
 */
void
close_audio_device (mhandle *handle);

/**
 * Reads @num_bytes bytes from the audio device.
 * @handle: The audio device.
 * @dest: Where to store the read data, should have enough room.
 * @num_bytes: Number of bytes to read.
 * @timestamp: A place to store the sample timestamp, or NULL.
 */
gboolean
read_audio_data (mhandle *handle, gpointer dest, guint num_bytes,
		 double *timestamp);
gboolean
write_audio_data (mhandle *handle, gpointer src, guint num_bytes,
		  double timestamp);

/* XXX eeew hack */
extern tv_audio_line audio_loopback_mixer_line;
extern tv_mixer audio_loopback_mixer;

void
reset_quiet			(tveng_device_info *	info,
				 guint			delay);
gboolean
set_mute			(gint		        mode,
				 gboolean		controls,
				 gboolean		osd);

extern void mixer_setup ( void );

/**
 * Initialization/shutdown
 */
void startup_audio (void);
void shutdown_audio (void);

typedef struct {
  const char *	name;

  /* Implementations */
  gpointer	(* open)	(gboolean		stereo,
				 guint			sampling_rate,
				 enum audio_format	format,
				 gboolean		write);
  void		(*close)(gpointer handle);
  gboolean		(*read)(gpointer handle, gpointer dest,
			guint num_bytes, double *timestamp);
  gboolean	(*write)(gpointer handle, gpointer src,
			 guint num_bytes, double timestamp);

  /* startup/shutdown, called just once */
  void		(*init)(void);
  void		(*shutdown)(void);

  /* Backend properties handling */
  /* Connect to "destroy" to free resources */
  void		(*add_props)(GtkBox *vbox);
  void		(*apply_props)(GtkBox *vbox);
} audio_backend_info;

tv_device_node *
oss_pcm_open			(void *			unused,
				 FILE *			log_fp, 
				 const char *		dev_name);
tv_device_node *
oss_pcm_scan			(void *			unused,
				 FILE *			log_fp);

#endif /* audio.h */

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
