#ifndef __CSCONVERT_H__
#define __CSCONVERT_H__

/**
 * Colorspace conversions.
 */

#include "tveng.h"

typedef void
CSConverter_fn			(void *			dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format,
				 const void *		user_data);

typedef struct {
  tv_pixfmt		src_pixfmt;
  tv_pixfmt		dst_pixfmt;
  CSConverter_fn *	convert;
  const void *		user_data;
} CSFilter;

typedef struct {
  tv_pixfmt_set		src_pixfmt_set;
  tv_pixfmt_set		dst_pixfmt_set;
  CSConverter_fn *	convert;
  const void *		user_data;
} CSFilters;

/**
 * Try to find an available converter, returns -1 on error or the
 * converter id on success.
 */
int lookup_csconvert(tv_pixfmt src_pixfmt,
		     tv_pixfmt dst_pixfmt);

/**
 * Converts from src to dest.
 */
gboolean csconvert(void *	dst_image,
				 const tv_image_format *dst_format,
				 const void *		src_image,
				 const tv_image_format *src_format);

/**
 * Registers a converter. Returns -1 and does nothing when there
 * already a converter for the given pair, something else on success.
 * User data will be passed to the converter each time it's called.
 */
int register_converter (const char *name,
			tv_pixfmt src_pixfmt,
			tv_pixfmt dst_pixfmt,
			CSConverter_fn *converter,
			const void *user_data);

/*
 * Registers a bunch of converters at once. Does the same thing as
 * registering them one by one, it's just convenience. Returns
 * the number of successfully registered converters.
 */
int register_converters (const char *name,
			 CSFilter	*converters,
			 int		num_converters);

/* startup and shutdown of the conversions */
void startup_csconvert(void);
void shutdown_csconvert(void);

#endif /* csconvert.h */
