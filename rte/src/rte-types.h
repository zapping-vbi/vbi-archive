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
 *   <row><entry>Menu:</><entry>%NULL or menu.num[min.num ... max.num],
 *     step.num (1)</></row>
 *   </tbody></tgroup></informaltable>
 * @RTE_OPTION_REAL:
 *   A real value, optional a vector of possible values.
 *   <informaltable frame=none><tgroup cols=2><tbody>
 *   <row><entry>Type:</><entry>double</></row>
 *   <row><entry>Default:</><entry>def.dbl or menu.dbl[def.num]</></row>
 *   <row><entry>Bounds:</><entry>min.dbl ... max.dbl,
 *      step.dbl or menu</></row>
 *   <row><entry>Menu:</><entry>%NULL or menu.dbl[min.num ... max.num],
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
 *   <row><entry>Menu:</><entry>%NULL or menu.str[min.num ... max.num],
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
 *   <row><entry>Menu:</><entry>menu.str[min.num ... max.num],
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
   *  MPEG-1 PS: video 0-16, audio 0-32, sliced vbi 0-1, to select rte_codec_set
   *  substream number 0 ... n-1.
   */
  char			min_elementary[RTE_STREAM_MAX + 1];
  char			max_elementary[RTE_STREAM_MAX + 1];

/* should we have flags like can pause, needs seek(), syncs, etc? */

} rte_context_info;

/**
 * rte_context:
 *
 * Opaque rte_context object. You can allocate an rte_context with
 * rte_context_new().
 **/
typedef struct rte_context rte_context;

typedef struct {
  rte_stream_type	stream_type;
  char *		keyword;	/* eg. mpeg2_audio_layer_2 */
  char *		label;		/* gettext()ized _N() */
  char *		tooltip;	/* or NULL, gettext()ized _N() */
} rte_codec_info;

/**
 * rte_context:
 *
 * Opaque rte_codec object. You can allocate an rte_codec with
 * rte_codec_set().
 **/
typedef struct rte_codec rte_codec;

/**
 * rte_video_stream_params:
 *
 * @frame_rate (for example 24.0, 25.0, 30000 / 1001.0) is the
 * nominal frame sampling rate, usually derived from the video
 * standard. The codec may compare this value against frame
 * timestamps to detect frame dropping.
 *
 * @pixel_aspect is the sampling aspect ratio, x / y.
 * Examples: Square pixels 1.0; ITU-R Rec. 601 sampled PAL
 * or SECAM 59 / 54.0, NTSC 10 / 11.0; times 9 / 16.0 for
 * anamorphic 16:9 video. The codec may store this information
 * in the stream for correct playback.
 *
 * @offset is the distance in bytes from the start of the image
 * to the top- and leftmost pixel to be encoded: x0 * bytes
 * per pixel + y1 * @stride. Usually this value is zero. When
 * the format is RTE_PIXFMT_YUV420 this refers to the luminance
 * plane only. Aligning at addresses which are a multiple of a
 * power of two may speed up encoding.
 *
 * @width, @height is the image size in pixels. The encoded
 * image size can be different due to scaling options. When the
 * format is RTE_PIXFMT_YUV420 this refers to the luminance plane, and
 * both @width and @height must be multiples of two. When the format is
 * RTE_PIXFMT_YUYV (or its variants) @width must be a multiple of two.
 * Still a codec may modify the proposed width and height if only
 * descrete values or other multiples are possible.
 *
 * @u_offset, @v_offset apply only when the format is RTE_PIXFMT_YUV420,
 * expressing the offset of the U (Cb) and V (Cr) plane from the
 * start of the image in bytes.
 *
 * @stride is the distance from one pixel to the adjacent pixel in the
 * next line, in bytes. When the format is RTE_PIXFMT_YUV420 this refers
 * to the luminance plane only. Aligning lines at addresses which are a
 * multiple of a power of two may speed up encoding. Usually @stride
 * is @width * bytes per pixel, but a codec may accept any larger or
 * smaller, even negative values.
 *
 * @uv_stride applies only when the format is RTE_PIXFMT_YUV420, the
 * distance from one pixel to the adjacent pixel in the next line, in
 * bytes, of both the U (Cb) and V (Cr) plane. Usually this value
 * is @stride / 2.
 **/
// XXX shall we permit abs(stride) * height access
// or add size field?
typedef struct {
	rte_pixfmt		pixfmt;
	double			frame_rate;
	double			pixel_aspect;
	unsigned int		width, height;
	unsigned int		offset, u_offset, v_offset;
	unsigned int		stride, uv_stride;
	/* scaling? */
} rte_video_stream_params;

/**
 * rte_audio_stream_params:
 *
 * @sampling_freq is the sampling frequency in Hz.
 *
 * @channels the number of audio channels, mono 1, stereo 2.
 * (May have to rethink this for 5.1.)
 *
 * @fragment_size: Audio data is passed in blocks rather than
 * single samples. The codec may require a certain number of
 * samples at once, modifying this value accordingly. This
 * block size is given in bytes, but must be divisible
 * by sample size in bytes times @channels.
 **/
typedef struct {
	rte_sndfmt		sndfmt;
	unsigned int		sampling_freq;
	unsigned int		channels;
	unsigned int		fragment_size;
} rte_audio_stream_params;

typedef union {
	rte_video_stream_params	video;
	rte_audio_stream_params	audio;
	char			pad[128];
} rte_stream_parameters;

/**
 * rte_buffer:
 *
 * This structure holds information about data packets exchanged
 * with the codec, for example one video frame or one block of audio
 * samples as defined with rte_parameters_set().
 *
 * Depending on data direction @data points to the data and @size
 * is the size of the data in bytes, or @data points to buffer space
 * to store data and @size is the space available.
 *
 * When data is passed, @timestamp is the capture instant (of the
 * first byte if you wish) in seconds and fractions since 1970-01-01
 * 00:00. A codec may use the timestamps for synchronization and to
 * detect frame dropping. Timestamps must increment with each buffer
 * passed, and should increment by 1 / nominal buffer rate. That is
 * #rte_video_stream_params.frame_rate or
 * #rte_audio_stream_params.sampling_freq * channels and samples per
 * buffer.
 *
 * The @user_data will be returned along @data with rte_push_buffer()
 * in passive push mode, and the unreference callback in passive
 * callback mode, is otherwise ignored.
 **/
typedef struct {
	void *			data;
	unsigned int		size;
	double			timestamp;
	void *			user_data;
} rte_buffer;

/**
 * rte_buffer_callback:
 * @context: #rte_context this operation refers to.
 * @codec: #rte_codec this operation refers to (if any).
 * @buffer: Pointer to #rte_buffer.
 *
 * When a function of this type is called to read more data
 * in active callback mode, the rte client must initialize the
 * @buffer fields .data, .size and .timestamp. In passive
 * callback mode .data points to the buffer space to store
 * the data and .size is the free space, so the client
 * must initialize .timestamp and may set .size and
 * .user_data.
 *
 * When a function of this type is called to unreference data,
 * the same @buffer contents will be passed as to the read
 * callback or rte_push_buffer() before.
 *
 * When a function of this type is called to write data,
 * @codec is NULL and the @buffer fields .data and .size
 * are initialized.
 *
 * Do <emphasis>not</> depend on the value of the @buffer
 * pointer, use buffer.user_data instead.
 *
 * Attention: A codec may read more than once before freeing
 * the data, and it may also free the data in a different order
 * than it has been read.
 *
 * Return value:
 * The callback can return %FALSE to terminate (or in case
 * of an i/o error abort) the encoding.
 **/
typedef rte_bool (* rte_buffer_callback)(rte_context *context,
					 rte_codec *codec,
					 rte_buffer *buffer);

/**
 * rte_seek_callback:
 * @context: #rte_context this operation refers to.
 * @codec: #rte_codec this operation refers to (if any).
 * @offset: Position to seek to.
 * @whence: SEEK_SET..., see man lseek.
 *
 * The context requests to seek to the given resulting stream
 * position. @offset and @whence follow lseek semantics.
 **/
typedef rte_bool (*rte_seek_callback)(rte_context *context,
				      off64_t offset,
				      int whence);

typedef struct {
  char *		keyword;
  char *		label;
  rte_basic_type	type;
  rte_option_value	val;
} rte_status_info;

#endif /* rte-types.h */



