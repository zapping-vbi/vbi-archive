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

/* options */

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

typedef int rte_bool;

/**
 * rte_option_type:
 * @RTE_OPTION_BOOL:
 *   A boolean value, either %TRUE (1) or %FALSE (0).
 *   <informaltable frame=none><tgroup cols=2><tbody>
 *   <row><entry>Type:</><entry>int</></row>
 *   <row><entry>Default:</><entry>def.num</></row>
 *   <row><entry>Bounds:</><entry>min.num (0) ... max.num (1),
 *     step.num (1)</></row>
 *   <row><entry>Menu:</><entry>%NULL</></row>
 *   </tbody></tgroup></informaltable>
 * @RTE_OPTION_INT:
 *   A signed integer value. When only a few discrete values rather than
 *   a range are permitted @menu points to a vector of integers. Note the
 *   option is still set by value, not by menu index, which may be rejected
 *   or replaced by the closest possible.
 *   <informaltable frame=none><tgroup cols=2><tbody>
 *   <row><entry>Type:</><entry>int</></row>
 *   <row><entry>Default:</><entry>def.num or menu.num[def.num]</></row>
 *   <row><entry>Bounds:</><entry>min.num ... max.num, step.num or menu</></row>
 *   <row><entry>Menu:</><entry>%NULL or menu.num[min.num (0) ... max.num],
 *     step.num (1)</></row>
 *   </tbody></tgroup></informaltable>
 * @RTE_OPTION_REAL:
 *   A real value, optional a vector of possible values.
 *   <informaltable frame=none><tgroup cols=2><tbody>
 *   <row><entry>Type:</><entry>double</></row>
 *   <row><entry>Default:</><entry>def.dbl or menu.dbl[def.num]</></row>
 *   <row><entry>Bounds:</><entry>min.dbl ... max.dbl,
 *      step.dbl or menu</></row>
 *   <row><entry>Menu:</><entry>%NULL or menu.dbl[min.num (0) ... max.num],
 *      step.num (1)</></row>
 *   </tbody></tgroup></informaltable>
 * @RTE_OPTION_STRING:
 *   A null terminated string. Note the menu version differs from
 *   RTE_OPTION_MENU in its argument, which is the string itself. For example:
 *   <programlisting>
 *   menu.str[0] = "red"
 *   menu.str[1] = "blue"
 *   ... and perhaps other colors not explicitely listed
 *   </programlisting>
 *   <informaltable frame=none><tgroup cols=2><tbody>
 *   <row><entry>Type:</><entry>char *</></row>
 *   <row><entry>Default:</><entry>def.str or menu.str[def.num]</></row>
 *   <row><entry>Bounds:</><entry>not applicable</></row>
 *   <row><entry>Menu:</><entry>%NULL or menu.str[min.num (0) ... max.num],
 *     step.num (1)</></row>
 *   </tbody></tgroup></informaltable>
 * @RTE_OPTION_MENU:
 *   Choice between a number of named options. For example:
 *   <programlisting>
 *   menu.str[0] = "up"
 *   menu.str[1] = "down"
 *   menu.str[2] = "strange"
 *   </programlisting>
 *   <informaltable frame=none><tgroup cols=2><tbody>
 *   <row><entry>Type:</><entry>int</></row>
 *   <row><entry>Default:</><entry>def.num</></row>
 *   <row><entry>Bounds:</><entry>min.num (0) ... max.num, 
 *      step.num (1)</></row>
 *   <row><entry>Menu:</><entry>menu.str[min.num (0) ... max.num],
 *      step.num (1).
 *      These strings are gettext'ized N_(), see the gettext() manuals
 *      for details.</></row>
 *   </tbody></tgroup></informaltable>
 **/
typedef enum {
	RTE_OPTION_BOOL = 1,
	RTE_OPTION_INT,
	RTE_OPTION_REAL,
	RTE_OPTION_STRING,
	RTE_OPTION_MENU,
} rte_option_type;

typedef union rte_option_value {
	int			num;
	double			dbl;
	char *			str;
} rte_option_value;

typedef union rte_option_value_ptr {
	int *			num;
	double *		dbl;
	char **			str;
} rte_option_value_ptr;

/**
 * rte_option_info:
 * 
 * Although export options can be accessed by a static keyword they are
 * by definition opaque: the client can present them to the user and
 * manipulate them without knowing about their presence or purpose.
 * To do so, some amount of information about the option is necessary,
 * given in this structure.
 * 
 * You can obtain this information with rte_context_option_info_enum()
 * or rte_codec_option_info_enum().
 * 
 * @type: Type of the option, see #rte_option_type for details.
 *
 * @keyword: Unique (within this context or codec) keyword to identify
 *   this option. Can be stored in configuration files.
 *
 * @label: Name of the option to be shown to the user.
 *   This can be %NULL to indicate this option shall not be listed.
 *   gettext()ized N_(), see the gettext manual.
 *
 * @def, @min, @max, @step, @menu: See #rte_option_type for details.
 *
 * @tooltip: A brief description (or %NULL) for the user.
 *   gettext()ized N_(), see the gettext manual.
 **/
typedef struct {
	rte_option_type		type;
	char *			keyword;
	char *			label;
	rte_option_value	def;
	rte_option_value	min;
	rte_option_value	max;
	rte_option_value	step;
	rte_option_value_ptr	menu;
	char *			tooltip;
} rte_option_info;


/* use with option varargs to make sure the correct cast is done */
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

#endif /* rte-types.h */
