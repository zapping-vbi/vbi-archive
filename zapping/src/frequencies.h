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

  gchar *			rf_table;
  gchar *			rf_name;	/* RF channel ("35", for example) */

  int input, standard; /* Attached input, standard or 0 */

  z_key				accel;		/* key to select this channel */

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
tveng_get_channel_by_id (int id, const tveng_rf_table *country);

/*
  Returns the id of the given channel, that can be used later with
  tveng_get_channel_by_id. Returns -1 on error.
*/
int
tveng_get_id_of_channel (tveng_rf_channel *channel, tveng_rf_table *country);



typedef int tv_bool;

/*
 *  tv_rf_channel represents one element in a three dimensional
 *  frequency table array. The country_code, table_name and
 *  channel_name uniquely identify a channel, the country_code
 *  and table_name a frequency table. The tv_rf_channel functions
 *  move through the array. All strings are static, feel free
 *  to copy.
 */
typedef struct {
	char		country_code[4];	/* ASCII ISO 3166, e.g. "US" */
	const char *	table_name;		/* ASCII identifier, e.g. "ccir" */
	const char *	domain;			/* UTF8 localized, e.g. "cable" */
	char		channel_name[8];	/* UTF8 prefix & channel number, e.g. "S21" */
	unsigned int	frequency;		/* Hz */
	unsigned int	bandwidth;		/* Hz */
	unsigned int	video_standards;	/* future stuff */
} tv_rf_channel;

#define tv_rf_channel_first_table(ch) tv_rf_channel_nth_table (ch, 0)
extern tv_bool
tv_rf_channel_next_table	(tv_rf_channel *	ch);
extern tv_bool
tv_rf_channel_nth_table		(tv_rf_channel *	ch,
				 unsigned int		index);
extern unsigned int
tv_rf_channel_table_size	(tv_rf_channel *	ch);
extern tv_bool
tv_rf_channel_table_by_name	(tv_rf_channel *	ch,
				 const char *		name);
#define tv_rf_channel_first_table_by_country(ch, country_code) \
	tv_rf_channel_table_by_name (ch, country_code)
extern tv_bool
tv_rf_channel_next_table_by_country
				(tv_rf_channel *	ch,
				 const char *		country_code);
extern const char *
tv_rf_channel_table_prefix	(tv_rf_channel *	ch,
				 unsigned int		index);
#define tv_rf_channel_first(ch) tv_rf_channel_nth (ch, 0)
extern tv_bool
tv_rf_channel_next		(tv_rf_channel *	ch);
extern tv_bool
tv_rf_channel_nth		(tv_rf_channel *	ch,
				 unsigned int		index);
extern tv_bool
tv_rf_channel_by_name		(tv_rf_channel *	ch,
				 const char *		name);
extern tv_bool
tv_rf_channel_by_frequency	(tv_rf_channel *	ch,
				 unsigned int		frequency);
extern tv_bool
tv_rf_channel_next_country	(tv_rf_channel *	ch);

/* ---- */

tveng_tuned_channel *
tveng_tuned_channel_first	(const tveng_tuned_channel *list);
tveng_tuned_channel *
tveng_tuned_channel_nth		(const tveng_tuned_channel *list,
				 guint			index);
tveng_tuned_channel *
tveng_tuned_channel_by_name	(tveng_tuned_channel *	list,
				 const gchar *		name);
tveng_tuned_channel *
tveng_tuned_channel_by_rf_name	(tveng_tuned_channel *	list,
				 const gchar *		rf_name);
void
tveng_tuned_channel_insert	(tveng_tuned_channel **	list,
				 tveng_tuned_channel *	tc,
				 guint			index);
void
tveng_tuned_channel_move	(tveng_tuned_channel **	list,
				 tveng_tuned_channel *	tc,
				 guint			new_index);
void
tveng_tuned_channel_remove	(tveng_tuned_channel **	list,
				 tveng_tuned_channel *	tc);
void
tveng_tuned_channel_copy	(tveng_tuned_channel *	dst,
				 const tveng_tuned_channel *src);
tveng_tuned_channel *
tveng_tuned_channel_new		(const tveng_tuned_channel *tc);
void
tveng_tuned_channel_delete	(tveng_tuned_channel *	tc);

tveng_tuned_channel *
tveng_tuned_channel_list_new	(tveng_tuned_channel *	list);
void
tveng_tuned_channel_list_delete (tveng_tuned_channel **	list);
gboolean
tveng_tuned_channel_in_list	(tveng_tuned_channel *	list,
				 tveng_tuned_channel *	tc);

/* old stuff */
int
tveng_tuned_channel_num (const tveng_tuned_channel * list);
tveng_tuned_channel *
tveng_remove_tuned_channel (gchar * rf_name, int id,
			    tveng_tuned_channel * list);
#endif
