#ifndef __MIXER_H__
#define __MIXER_H__

/* Startup/shutdown, as usual */
void		startup_mixer(void);
void		shutdown_mixer(void);

/**
 * Returns the name of the nth line, the result must be free'd.
 * NULL on error (/dev/mixer not usable, nth entry nonexistant, ...).
 * Lines start from 0.
 */
char		*mixer_get_description	(int	line);

/**
 * Gets the min/max bounds for the given line.
 * Returns -1 on error, 0 on success.
 */
int		mixer_get_bounds	(int	line,
					 int	*min,
					 int	*max);

/**
 * Sets the volume for a given line.
 * Returns -1 on error, 0 on success.
 */
int		mixer_set_volume	(int	line,
					 int	volume);

/**
 * Selects the given line for recording.
 * -1 on error, 0 on success.
 */
int		mixer_set_recording_line(int	line);
#endif
