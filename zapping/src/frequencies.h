#ifndef __FREQUENCIES_H__
#define __FREQUENCIES_H__

#include "tveng.h"

typedef struct{
  gchar * name;
  __u32 freq;
}
tveng_channel;

typedef struct _tveng_tuned_channel tveng_tuned_channel;

struct _tveng_tuned_channel{
  gchar *name; /* Name given to the channel (RTL, Eurosport, whatever) */
  gchar *real_name; /* Channel we chose this one from ("35", for
		       example) */
  int index; /* Index in the tuned_channel list */
  __u32 freq;

  /* Don't use this to navigate through the tuned_channel list, use
     the API instead */
  tveng_tuned_channel *prev;
  tveng_tuned_channel *next;
};

typedef struct{
  gchar * name;
  tveng_channel * channel_list;
  int chan_count;
}
tveng_channels;

/* Returns the number of channel in an specific country */
#define CHAN_COUNT(X) (sizeof(X)/sizeof(tveng_channel))

/* 
   Returns a pointer to the channel struct for some specific
   country. NULL if the specified country is not found.
*/
tveng_channels*
tveng_get_country_tune_by_name (gchar * country);

/*
  Returns a pointer to the specified by id channel. Returns NULL on
  error.
  This is useful if you want to get all the countries we know about,
  you can start from id 0, and go up until you get an error.
*/
tveng_channels*
tveng_get_country_tune_by_id (int id);

/*
  Finds an especific channel in an especific country by name. NULL on
  error.
*/
tveng_channel*
tveng_get_channel_by_name (gchar* name, tveng_channels * country);

/*
  Finds an especific channel in an especific country by its id. NULL on
  error.
*/
tveng_channel*
tveng_get_channel_by_id (int id, tveng_channels * country);

/*
  This function inserts a channel in the list (the list will keep
  alphabetically ordered).
  It returns the index where the channel is inserted.
*/
int
tveng_insert_tuned_channel (tveng_tuned_channel * new_channel);

/*
  Returns the number of items in the tuned_channel list
*/
int
tveng_tuned_channel_num (void);

/*
  Removes an specific channel form the list. You must provide its
  "real" name, i.e. "64" instead of "Tele5", for example. Returns -1
  if the channel could not be found. If real_name is NULL, then id is
  interpreted as the index in the tuned_channel list. Then -1 means
  out of bounds. if real_name is not NULL, then the first matching
  item from id is deleted.
*/
int
tveng_remove_tuned_channel (gchar * real_name, int id);

/*
  Removes all the items in the channel list
*/
int
tveng_clear_tuned_channel (void);

/*
  Retrieves the specified channel form the list, searching by name
  ("VOX"), and starting from index. Returns NULL on error. It uses
  strcasecomp(), so "VoX" matches "vOx", "Vox", "voX", ...
*/
tveng_tuned_channel*
tveng_retrieve_tuned_channel_by_name (gchar * name, int index);

/*
  Retrieves the specified channel by real name ("S23"), and starting
  from index. Returns NULL on error. Again a strcasecmp
*/
tveng_tuned_channel*
tveng_retrieve_tuned_channel_by_real_name (gchar * real_name, int
					   index);

/*
  Retrieves the channel in position "index". NULL on error
*/
tveng_tuned_channel*
tveng_retrieve_tuned_channel_by_index (int index);

#endif
