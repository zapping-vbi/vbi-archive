#ifndef __FREQUENCIES_H__
#define __FREQUENCIES_H__

#include "tveng.h"
#include "keyboard.h"

typedef struct tveng_rf_channel {
  const gchar *			name;
  uint32_t			freq; /* kHz */
} tveng_rf_channel;

typedef struct tveng_rf_table {
  const gchar *			name;
  tveng_rf_channel *		channel_list;
  int				channel_count;
  const gchar *			prefixes[4];
} tveng_rf_table;


typedef struct {
  gchar		name[32];
  gfloat	value; /* from 0 to 1 */
} tveng_tc_control;

typedef struct _tveng_tuned_channel tveng_tuned_channel;

struct _tveng_tuned_channel {
  gchar *			name;		/* Station (RTL, Eurosport, whatever) */
  gchar *			rf_name;	/* RF channel ("35", for example) */
  int input, standard; /* Attached input, standard or 0 */

  z_key				accel;		/* key to select this channel */

  gchar *country; /* The country this channel is in */
  int index; /* Index in the tuned_channel list */
  uint32_t			freq;		/* Frequency of this RF channel in kHz
						   (may differ from RF table due to fine tuning) */
  gint num_controls; /* number of saved controls for this channel */
  tveng_tc_control *controls; /* saved controls for this
				 channel pointer */

  /* Don't use this to navigate through the tuned_channel list, use
     the API instead */
  tveng_tuned_channel *prev;
  tveng_tuned_channel *next;
};

/* 
   Returns a pointer to the channel struct for some specific
   country. NULL if the specified country is not found.
*/
tveng_rf_table *
tveng_get_country_tune_by_name (gchar * country);

/* 
   Returns a pointer to the channel struct for some specific
   country. NULL if the specified country is not found.
   The given name can be i18ed this time.
*/
tveng_rf_table *
tveng_get_country_tune_by_i18ed_name (gchar * i18ed_country);

/*
  Returns a pointer to the specified by id channel. Returns NULL on
  error.
  This is useful if you want to get all the countries we know about,
  you can start from id 0, and go up until you get an error.
*/
tveng_rf_table *
tveng_get_country_tune_by_id (int id);

/*
  Returns the id of the given country tune, that could be used later
  on with tveng_get_country_tune_by_id. Returns -1 on error.
*/
int
tveng_get_id_of_country_tune (tveng_rf_table *country);

/*
  Finds a specific channel in a specific country by name. NULL on
  error.
*/
tveng_rf_channel *
tveng_get_channel_by_name (gchar *name, tveng_rf_table *country);

/*
  Finds a specific channel in a specific country by its id. NULL on
  error.
*/
tveng_rf_channel *
tveng_get_channel_by_id (int id, tveng_rf_table *country);

/*
  Returns the id of the given channel, that can be used later with
  tveng_get_channel_by_id. Returns -1 on error.
*/
int
tveng_get_id_of_channel (tveng_rf_channel *channel, tveng_rf_table *country);

/**
 * This function inserts a channel in the list (the list will keep
 * alphabetically ordered).
 * new_channel: The channel to be inserted, all fields must be filled
 * in, except prev and next.
 * list: List where we should insert the channel, or NULL for starting
 * a new list.
 * Returns the new pointer to the list (it might be different from list)
*/
tveng_tuned_channel *
tveng_insert_tuned_channel_sorted (tveng_tuned_channel * new_channel,
				   tveng_tuned_channel * list);

/**
 * Append the given channel to the list
 */
tveng_tuned_channel *
tveng_append_tuned_channel (tveng_tuned_channel * new_channel,
			    tveng_tuned_channel * list);

/**
 * Swaps the given channel and the previous one.
 */
void
tveng_tuned_channel_up (tveng_tuned_channel * channel);

/**
 * Swaps the given channel and the next one.
 */
void
tveng_tuned_channel_down (tveng_tuned_channel * channel);

/**
 * Swaps the two given channels.
 */
void
tveng_tuned_channels_swap (tveng_tuned_channel * a,
			   tveng_tuned_channel * b);

/**
 *  Returns the number of items in the tuned_channel list
 */
int
tveng_tuned_channel_num (tveng_tuned_channel * list);

/**
 * Removes an specific channel form the list. You must provide its
 * "real" name, i.e. "64" instead of "Tele5", for example. Returns -1
 * if the channel could not be found. If rf_name is NULL, then id is
 * interpreted as the index in the tuned_channel list. Then -1 means
 * out of bounds. if rf_name is not NULL, then the first matching
 * item from id is deleted.
 * Returns a pointer to the list (can return NULL, that's OK)
*/
tveng_tuned_channel *
tveng_remove_tuned_channel (gchar * rf_name, int id,
			    tveng_tuned_channel * list);

/**
 * replaces the contents of dest with the contents of src
 */
void
tveng_copy_tuned_channel (tveng_tuned_channel * dest,
			  tveng_tuned_channel * src);

/**
 *  Removes all the items in the channel list, returns NULL.
 */
tveng_tuned_channel *
tveng_clear_tuned_channel (tveng_tuned_channel * list);

/*
  Retrieves the specified channel form the list, searching by name
  ("VOX"), and starting from index. Returns NULL on error. It uses
  strcasecomp(), so "VoX" matches "vOx", "Vox", "voX", ...
*/
tveng_tuned_channel*
tveng_retrieve_tuned_channel_by_name (gchar * name, int index,
				      tveng_tuned_channel * list);

/*
  Retrieves the specified channel by RF channel name ("S23"), and starting
  from index. Returns NULL on error. Again a strcasecmp
*/
tveng_tuned_channel*
tveng_retrieve_tuned_channel_by_rf_name (gchar * rf_name, int
					 index,
					 tveng_tuned_channel * list);

/*
  Retrieves the channel in position "index". NULL on error
*/
tveng_tuned_channel*
tveng_retrieve_tuned_channel_by_index (int index,
				       tveng_tuned_channel * list);

/**
 * Returns TRUE if the given channel is in the given channel list.
 */
gboolean
tveng_tuned_channel_in_list (tveng_tuned_channel * channel,
			     tveng_tuned_channel * list);

#endif
