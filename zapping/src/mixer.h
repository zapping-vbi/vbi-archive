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
 * [Un]Mute a given line.
 * Returns -1 on error, 0 on success.
 */
int		mixer_set_mute		(int	line,
					 int	mute);
/**
 * Returns -1 on error, 0|1 on success.
 */
int		mixer_get_mute		(int	line);

/**
 * Selects the given line for recording.
 * -1 on error, 0 on success.
 */
int		mixer_set_recording_line(int	line);







/* Future */

#include "tveng.h"

/* Client interface */

/*
 *  Machine readable purpose of the mixer line.
 *  We'll see what's needed.
 */
typedef enum {
	TV_MIXER_LINE_ID_NONE,
	TV_MIXER_LINE_ID_UNKNOWN = TV_MIXER_LINE_ID_NONE,
} tv_mixer_line_id;

typedef struct _tv_mixer tv_mixer;
typedef struct _tv_mixer_line tv_mixer_line;

struct _tv_mixer_line {
	tv_mixer_line *		next;

	tv_mixer_line_id	id;

	/*
	 *  For the user, localized.
	 */
	const char *		label;

	/*
	 *  Volume bounds.
	 */
	unsigned int		minimum;
	unsigned int		maximum;
	unsigned int		step;
	unsigned int		reset;

	/*
	 *  A mixer input which can be routed to the ADC.
	 *  Only these are valid args for set_rec_line().
	 */
	unsigned recordable	: 1;

	/*
	 *  This is a stereo line.
	 */
	unsigned stereo		: 1;

	/*
	 *  Last known left (0) and right (1) volume and
	 *  mute state. Last known in the sense that rogue users
	 *  equipped with evil mixer applications change these
	 *  whenever we don't expect. Use of callback recommended.
	 */
	unsigned int		volume[2];

	tv_bool			muted;
};

/*
 *  Assumptions, for now:
 *
 *  A mixer has zero or more analog inputs listed in
 *  tv_mixer.inputs. All inputs are summed and routed to one or
 *  more outputs. The volume and mute control affect each input line
 *  individually before summation and have no effect on recording.
 *
 *  Sound devices can have one ADC. If so, of a subset of inputs one
 *  (tv_mixer.rec_line) can be routed to the only ADC. Optional a
 *  gain control (tv_mixer.rec_gain) exists on the internal line
 *  between the input multiplexer and ADC.
 *
 *  Sound devices can have one DAC, somehow routed to one or more
 *  outputs. Optional a gain control (tv_mixer.play_gain) exists
 *  on the internal line between the DAC and summation or a
 *  multiplexer connecting to the outputs.
 *
 *  Outputs may have volume controls associated with them, but we
 *  leave them alone, to be changed with a mixer application.
 *  Likewise we need not care about the routing of inputs and DAC to
 *  outputs, at least until we have to deal with multichannel sound
 *  or user complaints. :-)
 *
 *  Some mixers can select more than one input for recording.
 *  We don't need that, but won't interfere if the user insists.
 *  Except to reset when muted at startup the playback gain should
 *  be left alone, because hardware or software may sum PCM audio
 *  before converted by the DAC. Output volume can be easily
 *  implemented in our codecs.
 *
 *  Major flaw remains the assumption of a single ADC and DAC. Some
 *  devices have more, have digital inputs and outputs, and not all
 *  of them routed through the mixer.
 */
struct _tv_mixer {
	/*
	 *  For the user, ASCII.
	 */
	const char *		name;

	/*
	 *  For the config, ASCII.
	 *  XXX rethink
	 */
	const char *		device;

	/*
	 *  Routes from inputs to output sum/mux. Mute/volume
	 *  does not affect recording.
	 */
	tv_mixer_line *		inputs;

	/*
	 *  Last known recording source, this points to one of the
	 *  'inputs' or NULL.
	 *
	 *  Note the user can select multiple sources with a mixer
	 *  application, then this is only one of them, usually the
	 *  one requested with set_rec_line(). Point is this may
	 *  change asynchronously, use of callback recommended.
	 */
	tv_mixer_line *		rec_line;

	/*
	 *  Route from rec mux to ADC or NULL.
	 */
	tv_mixer_line *		rec_gain;

	/*
	 *  Route from DAC to output sum/mux or NULL.
	 */
	tv_mixer_line *		play_gain;
};

extern tv_bool
tv_mixer_line_update		(tv_mixer_line *	line);

extern tv_bool
tv_mixer_line_get_volume	(tv_mixer_line *	line,
				 unsigned int *		left,
				 unsigned int *		right);
extern tv_bool
tv_mixer_line_set_volume	(tv_mixer_line *	line,
				 unsigned int		left,
				 unsigned int		right);
extern tv_bool
tv_mixer_line_get_mute		(tv_mixer_line *	line,
				 tv_bool *		mute);
extern tv_bool
tv_mixer_line_set_mute		(tv_mixer_line *	line,
				 tv_bool		mute);
extern tv_bool
tv_mixer_line_record		(tv_mixer_line *	line,
				 tv_bool		exclusive);
extern tv_callback_node *
tv_mixer_line_callback_add	(tv_mixer_line *	line,
				 tv_bool		(* notify)(tv_mixer_line *, void *user_data),
				 void			(* destroy)(tv_mixer_line *, void *user_data),
				 void *			user_data);
extern tv_bool
tv_mixer_update			(tv_mixer *		mixer);
extern tv_callback_node *
tv_mixer_callback_add		(tv_mixer *		mixer,
				 tv_bool		(* notify)(tv_mixer_line *, void *user_data),
				 void			(* destroy)(tv_mixer_line *, void *user_data),
				 void *			user_data);

/* open, scan? */

/* Device interface */

typedef struct _tv_dev_mixer tv_dev_mixer;
typedef struct _tv_dev_mixer_line tv_dev_mixer_line;
typedef struct _tv_dev_mixer_interface tv_dev_mixer_interface;

struct _tv_dev_mixer_line {
	tv_mixer_line		pub;
	tv_dev_mixer *		mixer;

	/*
	 *  Called by interface when tv_mixer_line.volume
	 *  or .muted changed.
	 */
	tv_callback_node *	callback;
};

struct _tv_dev_mixer {
	tv_dev_mixer *		next;
	tv_dev_mixer_interface *interface;

	FILE *			log;		/* if non-zero log all driver i/o */

	tv_mixer		pub;

	/*
	 *  Called by interface when tv_mixer.rec_line changed.
	 */
	tv_callback_node *	callback;

	/*
	 *  Update tv_mixer_line.muted and .volume, e.g. to notice when
	 *  other applications change mixer properties asynchronously.
	 *  Regular polling recommended, may call tv_dev_mixer_line.changed.
	 */
	tv_bool			(* update_line)		(tv_dev_mixer_line *);

	/*
	 *  Set mixer volume and update tv_mixer_line.volume accordingly.
	 *  On mono lines left volume will be set. May call
	 *  tv_dev_mixer_line.changed. Does not unmute.
	 */
	tv_bool			(* set_volume)		(tv_dev_mixer_line *,
							 unsigned int left,
							 unsigned int right);
	
	/*
	 *  Mute (TRUE) or unmute (FALSE) mixer line and update
	 *  tv_mixer_line.muted accordingly. May call
	 *  tv_dev_mixer_line.changed.
	 */
	tv_bool			(* set_mute)		(tv_dev_mixer_line *,
							 tv_bool mute);

	/*
	 *  Select a recording line from tv_mixer.adc_lines. When
	 *  exclusive is TRUE disable all other recording sources (should
	 *  be the default, but we must not prohibit recording from
	 *  multiple sources if the user really insists). Line can be
	 *  NULL. May call tv_dev_mixer.changed. 
	 */
	tv_bool			(* set_rec_line)	(tv_dev_mixer *,
							 tv_dev_mixer_line *,
							 tv_bool exclusive);

	/*
	 *  Update tv_mixer.rec_line, e.g. to notice when other applications
	 *  change mixer properties asynchronously. Regular polling recommended,
	 *  may call tv_dev_mixer.changed.
	 */
	tv_bool			(* update_mixer)	(tv_dev_mixer *);

	/*
	 *  Called by the frontend to free the structure and all
	 *  associated data, close the device.
	 */
	void			(* destroy)		(tv_dev_mixer *,
							 tv_bool restore);
};

struct _tv_dev_mixer_interface {
	const char *		name;

	tv_dev_mixer *		(* open)		(tv_dev_mixer_interface *,
							 const char *dev_name,
							 FILE *log);

	tv_dev_mixer *		(* scan)		(tv_dev_mixer_interface *);
};

#endif
