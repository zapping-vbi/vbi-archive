#ifndef __CSCONVERT_H__
#define __CSCONVERT_H__

/**
 * Colorspace conversions.
 */

#include <tveng.h>

typedef void (*CSConverter) (tveng_image_data *src, tveng_image_data *dest,
			     int width, int height,
			     gpointer user_data);

typedef struct {
  enum tveng_frame_pixformat	src;
  enum tveng_frame_pixformat	dest;
  CSConverter	convert;
  gpointer	user_data;
} CSFilter;

/**
 * Try to find an available converter, returns -1 on error or the
 * converter id on success.
 */
int lookup_csconvert(enum tveng_frame_pixformat src_fmt,
		     enum tveng_frame_pixformat dest_fmt);

/**
 * Converts from src to dest.
 */
void csconvert(int id, tveng_image_data *src,
	       tveng_image_data *dest,
	       int width, int height);

/**
 * Registers a converter. Returns -1 and does nothing when there
 * already a converter for the given pair, something else on success.
 * User data will be passed to the converter each time it's called.
 */
int register_converter (enum tveng_frame_pixformat src,
			enum tveng_frame_pixformat dest,
			CSConverter	converter,
			gpointer	user_data);

/*
 * Registers a bunch of converters at once. Does the same thing as
 * registering them one by one, it's just convenience. Returns
 * the number of successfully registered converters.
 */
int register_converters (CSFilter	*converters,
			 int		num_converters);

/* startup and shutdown of the conversions */
void startup_csconvert(void);
void shutdown_csconvert(void);

#endif /* csconvert.h */
