#ifndef __AUDIO_H__
#define __AUDIO_H__

enum audio_format {
  /* More to come when we need more */
  AUDIO_FORMAT_S16_LE
};

/**
 * Opens the configured audio device and returns an opaque handler to
 * it, or %NULL on error.
 * @stereo: %TRUE if the device should be opened in stereo mode.
 * @rate: Samples per second (typically 44100)
 * @format: Audio format to use.
 */
gpointer
open_audio_device (gboolean stereo, gint rate, enum audio_format format);

/**
 * Closes @handle.
 */
void
close_audio_device (gpointer handle);

/**
 * Reads @num_bytes bytes from the audio device.
 * @handle: The audio device.
 * @dest: Where to store the read data, should have enough room.
 * @num_bytes: Number of bytes to read.
 * @timestamp: A place to store the sample timestamp, or NULL.
 */
void
read_audio_data (gpointer handle, gpointer dest, gint num_bytes,
		 double *timestamp);

/**
 * Initialization/shutdown
 */
void startup_audio (void);
void shutdown_audio (void);


typedef struct {
  char		*name;

  /* Implementations */
  gpointer	(*open)(gboolean stereo, gint rate, enum audio_format format);
  void		(*close)(gpointer handle);
  void		(*read)(gpointer handle, gpointer dest,
			gint num_bytes, double *timestamp);

  /* startup/shutdown, called just once */
  void		(*init)(void);
  void		(*shutdown)(void);

  /* Backend properties handling */
  /* Connect to "destroy" to free resources */
  void		(*add_props)(GtkBox *vbox, GnomePropertyBox *gpb);
  void		(*apply_props)(GtkBox *vbox);
} audio_backend_info;

#endif /* audio.h */
