/*
 *  Real Time Encoder
 *
 *  Copyright (C) 2000-2001 Iñaki García Etxebarria
 *  Modified 2001 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __RTE_TYPES_H__
#define __RTE_TYPES_H__

/* FIXME: This should be improved (requirements for off64_t) */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <sys/types.h>
#include <unistd.h>

/* use with option varargs to make sure the correct cast is done */
typedef int rte_bool;
typedef int rte_int;
typedef double rte_real;
typedef char* rte_string;
typedef int rte_menu;

typedef void* rte_pointer;

typedef struct {
  char *		keyword;	/* eg. "mp1e-mpeg1-ps" */

  char *		backend;	/* no NLS b/c proper name */

  char *		label;		/* gettext()ized _N() */
  char *		tooltip;	/* or NULL, gettext()ized _N() */

  /*
   *  Multiple strings allowed, separated by comma. The first
   *  string is preferred. Ex "video/x-mpeg", "mpg,mpeg".
   */
  char *		mime_type;	/* or NULL */
  char *		extension;	/* or NULL */

  /*
   *  Permitted number of elementary streams of each type, for example
   *  MPEG-1 PS: video 16, audio 32, sliced vbi 1, to select rte_codec_set
   *  substream number 0 ... n-1.
   */
  char			elementary[RTE_STREAM_MAX + 1];
} rte_context_info;

typedef struct _rte_context rte_context; /* opaque */

typedef struct {
  rte_stream_type	stream_type;
  char *		keyword;	/* eg. mpeg2_audio_layer_2 */
  char *		label;		/* gettext()ized _N() */
  char *		tooltip;	/* or NULL, gettext()ized _N() */
} rte_codec_info;

typedef struct _rte_codec rte_codec; /* opaque */

typedef union {
  rte_int		num;
  rte_menu		idx;
  rte_string		str;		/* gettext()ized _N() */
  rte_real		dbl;
} rte_option_value;

typedef struct {
  rte_option_type	type;
  char *		keyword;
  char *		label;		/* gettext()ized _N() */
  rte_option_value	def;		/* default (reset) */
  rte_option_value	min, max;
  rte_option_value	step;
  union {
    rte_bool *		val;
    rte_int *           num;
    rte_string *        str;
    rte_real *		dbl;
  }                     menu;
  rte_basic_type	menu_type;	/* type of data the menu contains */
  int			entries;
  char *		tooltip;	/* or NULL, gettext()ized _N() */
} rte_option_info;

typedef struct {
  rte_pixfmt	pixfmt;
  double	frame_rate;	    /* 24, 25, 30, 30000 / 1001, .. */
  double	pixel_aspect;
  int		width, height;    /* pixels, Y if YUV 4:2:0 */
  int		u_offset, v_offset; /* bytes rel. Y org or ignored */
  int		stride;	    /* bytes, Y if YUV 4:2:0 */
  int		uv_stride;	    /* bytes or ignored */
  /* scaling? */
} rte_video_stream_parameters;

typedef struct {
  rte_sndfmt	sndfmt;
  int		sampling_freq;	/* Hz */
  int		channels;	/* mono: 1, stereo: 2 */
  int		fragment_size;	/* bytes */
} rte_audio_stream_parameters;

typedef union {
  rte_video_stream_parameters	video;
  rte_audio_stream_parameters	audio;
  char				pad[128]; /* binary compat */
} rte_stream_parameters;

typedef struct {
  rte_pointer	data; /* Pointer to the data in the buffer */
  double	timestamp; /* timestamp for the buffer */
  rte_pointer	user_data; /* Whatever data the user wants to store */
} rte_buffer;

typedef struct {
  char *		keyword;
  char *		label;
  rte_basic_type	type;
  rte_option_value	val;
} rte_status_info;

typedef void (*rteDataCallback)(rte_context * context,
				rte_codec * codec,
				rte_pointer data,
				double * time);

typedef void (*rteBufferCallback)(rte_context * context,
				  rte_codec * codec,
				  rte_buffer * buffer);

typedef void (*rteWriteCallback)(rte_context * context,
				 rte_pointer data,
				 int bytes);

/**
 * rteSeekCallback:
 * @context: Context that requests the seek().
 * @offset: Position to seek to.
 * @whence: SEEK_SET..., see man lseek.
 *
 * The context requests to seek to the given resulting stream
 * position. @offset and @whence follow lseek semantics.
 **/
typedef void (*rteSeekCallback)(rte_context * context,
				off64_t offset,
				int whence);

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif

#endif /* rte-types.h */
