#ifndef __CSCONVERT_H__
#define __CSCONVERT_H__

/**
 * Colorspace conversions.
 */

#include <tveng.h>

/**
 * Try to find an available converter, returns -1 on error or the
 * converter id on success.
 */
int lookup_csconvert(enum tveng_frame_pixformat src_fmt,
		     enum tveng_frame_pixformat dest_fmt);

/**
 * Converts from src to dest.
 */
void csconvert(int id, const char *src, char *dest,
	       int src_stride, int dest_stride, int width, int height);

/**
 * Builds the appropiate conversion tables.
 * The format of the fields is the same as in the visual info reported
 * by X.
 */
void build_csconvert_tables(int rmask, int rshift, int rprec,
			    int gmask, int gshift, int gprec,
			    int bmask, int bshift, int bprec);

/* startup and shutdown of the conversions */
void startup_csconvert(void);
void shutdown_csconvert(void);

#endif /* csconvert.h */
