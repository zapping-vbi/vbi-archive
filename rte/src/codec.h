/*
 *  Real Time Encoding Library
 *
 *  Copyright (C) 2000, 2001 Iñaki García Etxebarria
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
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

/* $Id: codec.h,v 1.1 2002-03-16 16:35:37 mschimek Exp $ */

#ifndef CODEC_H
#define CODEC_H

#include "option.h"

/* Public */

/**
 * rte_codec:
 *
 * Opaque rte_codec object. You can allocate an rte_codec with
 * rte_codec_set().
 **/
typedef struct rte_codec rte_codec;

typedef enum {
  RTE_STREAM_VIDEO = 0,  /* XXX STREAM :-( need a better term */
  RTE_STREAM_AUDIO,	 /* input/output distinction? */
  RTE_STREAM_RAW_VBI,
  RTE_STREAM_SLICED_VBI,
  RTE_STREAM_MAX = 15
} rte_stream_type;

typedef struct {
	rte_stream_type		stream_type;
	char *			keyword;	/* eg. mpeg2_audio_layer_2 */
	char *			label;		/* gettext()ized _N() */
	char *			tooltip;	/* or NULL, gettext()ized _N() */
} rte_codec_info;

/**
 * rte_pixfmt:
 * 
 * <table frame=all><title>Sample formats</title><tgroup cols=5 align=center>
 * <colspec colname=c1><colspec colname=c2><colspec colname=c3><colspec colname=c4>
 * <colspec colname=c5>
 * <spanspec spanname=desc1 namest=c1 nameend=c5 align=left>
 * <spanspec spanname=desc2 namest=c2 nameend=c5 align=left>
 * <thead>
 * <row><entry>Symbol</><entry>Byte&nbsp;0</><entry>Byte&nbsp;1</>
 * <entry>Byte&nbsp;2</><entry>Byte&nbsp;3</></row>
 * </thead><tbody>
 * <row><entry spanname=desc1>Planar YUV 4:2:0 data.</></row>
 * <row><entry>@RTE_PIXFMT_YUV420</><entry spanname=desc2>
 *  <informaltable frame=none><tgroup cols=3><thead>
 *   <row><entry>Y plane</><entry>U plane</><entry>V plane</></row>
 *   </thead><tbody><row>
 *   <entry><informaltable frame=1><tgroup cols=4><tbody>
 *    <row><entry>Y00</><entry>Y01</><entry>Y02</><entry>Y03</></row>
 *    <row><entry>Y10</><entry>Y11</><entry>Y12</><entry>Y13</></row>
 *    <row><entry>Y20</><entry>Y21</><entry>Y22</><entry>Y23</></row>
 *    <row><entry>Y30</><entry>Y31</><entry>Y32</><entry>Y33</></row>
 *   </tbody></tgroup></informaltable></entry>
 *   <entry><informaltable frame=1><tgroup cols=2><tbody>
 *    <row><entry>Cb00</><entry>Cb01</></row>
 *    <row><entry>Cb10</><entry>Cb11</></row>
 *   </tbody></tgroup></informaltable></entry>
 *   <entry><informaltable frame=1><tgroup cols=2><tbody>
 *    <row><entry>Cr00</><entry>Cr01</></row>
 *    <row><entry>Cr10</><entry>Cr11</></row>
 *   </tbody></tgroup></informaltable></entry>
 *  </row></tbody></tgroup></informaltable></entry>
 * </row>
 * <row><entry spanname=desc1>Packed YUV 4:2:2 data.</></row>
 * <row><entry>@RTE_PIXFMT_YUYV</><entry>Y0</><entry>Cb</><entry>Y1</><entry>Cr</></row>
 * <row><entry>@RTE_PIXFMT_YVYU</><entry>Y0</><entry>Cr</><entry>Y1</><entry>Cb</></row>
 * <row><entry>@RTE_PIXFMT_UYVY</><entry>Cb</><entry>Y0</><entry>Cr</><entry>Y1</></row>
 * <row><entry>@RTE_PIXFMT_VYUY</><entry>Cr</><entry>Y0</><entry>Cb</><entry>Y1</></row>
 * <row><entry spanname=desc1>Packed 32 bit RGB data.</></row>
 * <row><entry>@RTE_PIXFMT_RGBA32_LE @RTE_PIXFMT_ARGB32_BE</>
 * <entry>r7&nbsp;...&nbsp;r0</><entry>g7&nbsp;...&nbsp;g0</>
 * <entry>b7&nbsp;...&nbsp;b0</><entry>a7&nbsp;...&nbsp;a0</></row>
 * <row><entry>@RTE_PIXFMT_BGRA32_LE @RTE_PIXFMT_ARGB32_BE</>
 * <entry>b7&nbsp;...&nbsp;b0</><entry>g7&nbsp;...&nbsp;g0</>
 * <entry>r7&nbsp;...&nbsp;r0</><entry>a7&nbsp;...&nbsp;a0</></row>
 * <row><entry>@RTE_PIXFMT_ARGB32_LE @RTE_PIXFMT_BGRA32_BE</>
 * <entry>a7&nbsp;...&nbsp;a0</><entry>r7&nbsp;...&nbsp;r0</>
 * <entry>g7&nbsp;...&nbsp;g0</><entry>b7&nbsp;...&nbsp;b0</></row>
 * <row><entry>@RTE_PIXFMT_ABGR32_LE @RTE_PIXFMT_RGBA32_BE</>
 * <entry>a7&nbsp;...&nbsp;a0</><entry>b7&nbsp;...&nbsp;b0</>
 * <entry>g7&nbsp;...&nbsp;g0</><entry>r7&nbsp;...&nbsp;r0</></row>
 * <row><entry spanname=desc1>Packed 24 bit RGB data.</></row>
 * <row><entry>@RTE_PIXFMT_RGBA24</>
 * <entry>r7&nbsp;...&nbsp;r0</><entry>g7&nbsp;...&nbsp;g0</>
 * <entry>b7&nbsp;...&nbsp;b0</><entry></></row>
 * <row><entry>@RTE_PIXFMT_BGRA24</>
 * <entry>b7&nbsp;...&nbsp;b0</><entry>g7&nbsp;...&nbsp;g0</>
 * <entry>r7&nbsp;...&nbsp;r0</><entry></></row>
 * <row><entry spanname=desc1>Packed 16 bit RGB data.</></row>
 * <row><entry>@RTE_PIXFMT_RGB16_LE</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</>
 * <entry>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g5&nbsp;g4&nbsp;g3</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_BGR16_LE</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</>
 * <entry>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g5&nbsp;g4&nbsp;g3</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_RGB16_BE</>
 * <entry>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g5&nbsp;g4&nbsp;g3</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_BGR16_BE</>
 * <entry>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g5&nbsp;g4&nbsp;g3</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</>
 * <entry></><entry></></row>
 * <row><entry spanname=desc1>Packed 15 bit RGB data.</></row>
 * <row><entry>@RTE_PIXFMT_RGBA15_LE</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</>
 * <entry>a0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_BGRA15_LE</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</>
 * <entry>a0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_ARGB15_LE</>
 * <entry>g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;a0</>
 * <entry>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3&nbsp;g2</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_ABGR15_LE</>
 * <entry>g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;a0</>
 * <entry>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3&nbsp;g2</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_RGBA15_BE</>
 * <entry>a0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_BGRA15_BE</>
 * <entry>a0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3</>
 * <entry>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_ARGB15_BE</>
 * <entry>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3&nbsp;g2</>
 * <entry>g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;a0</>
 * <entry></><entry></></row><row><entry>@RTE_PIXFMT_ABGR15_BE</>
 * <entry>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3&nbsp;g2</>
 * <entry>g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;a0</>
 * <entry></><entry></></row>
 * </tbody></tgroup></table>
 **/
/* Attn: keep this in sync with zvbi, don't change order */
typedef enum {
  RTE_PIXFMT_YUV420 = 1,
  RTE_PIXFMT_YUYV,
  RTE_PIXFMT_YVYU,
  RTE_PIXFMT_UYVY,
  RTE_PIXFMT_VYUY,
  RTE_PIXFMT_RGBA32_LE = 32,
  RTE_PIXFMT_RGBA32_BE,
  RTE_PIXFMT_BGRA32_LE,
  RTE_PIXFMT_BGRA32_BE,
  RTE_PIXFMT_RGB24,
  RTE_PIXFMT_BGR24,
  RTE_PIXFMT_RGB16_LE,
  RTE_PIXFMT_RGB16_BE,
  RTE_PIXFMT_BGR16_LE,
  RTE_PIXFMT_BGR16_BE,
  RTE_PIXFMT_RGBA15_LE,
  RTE_PIXFMT_RGBA15_BE,
  RTE_PIXFMT_BGRA15_LE,
  RTE_PIXFMT_BGRA15_BE,
  RTE_PIXFMT_ARGB15_LE,
  RTE_PIXFMT_ARGB15_BE,
  RTE_PIXFMT_ABGR15_LE,
  RTE_PIXFMT_ABGR15_BE
} rte_pixfmt;

#define RTE_PIXFMT_ABGR32_BE RTE_PIXFMT_RGBA32_LE
#define RTE_PIXFMT_ARGB32_BE RTE_PIXFMT_BGRA32_LE
#define RTE_PIXFMT_ABGR32_LE RTE_PIXFMT_RGBA32_BE
#define RTE_PIXFMT_ARGB32_LE RTE_PIXFMT_BGRA32_BE


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
typedef struct rte_video_stream_params rte_video_stream_params;

struct rte_video_stream_params {
	rte_pixfmt		pixfmt;
	double			frame_rate;
	double			pixel_aspect;
	unsigned int		width, height;
	unsigned int		offset, u_offset, v_offset;
	unsigned int		stride, uv_stride;
};

/* Private */
// XXX shall we permit abs(stride) * height access
// or add size field? What about scaling?
/* Public */

/**
 * rte_sndfmt:
 * @RTE_SNDFMT_S8: Signed 8 bit samples. 
 * @RTE_SNDFMT_U8: Unsigned 8 bit samples.
 * @RTE_SNDFMT_S16_LE: Signed 16 bit samples, little endian.
 * @RTE_SNDFMT_S16_BE: Signed 16 bit samples, big endian.
 * @RTE_SNDFMT_U16_LE: Unsigned 16 bit samples, little endian.
 * @RTE_SNDFMT_U16_BE: Unsigned 16 bit samples, big endian.
 *
 * RTE PCM audio formats.
 **/
typedef enum {
  RTE_SNDFMT_S8 = 1,
  RTE_SNDFMT_U8,
  RTE_SNDFMT_S16_LE,
  RTE_SNDFMT_S16_BE,
  RTE_SNDFMT_U16_LE,
  RTE_SNDFMT_U16_BE
} rte_sndfmt;

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
typedef struct rte_audio_stream_params rte_audio_stream_params;

struct rte_audio_stream_params {
	rte_sndfmt		sndfmt;
	unsigned int		sampling_freq;
	unsigned int		channels;
	unsigned int		fragment_size;
};

/* VBI parameters defined in libzvbi.h */

typedef union {
	rte_video_stream_params	video;
	rte_audio_stream_params	audio;
	char			pad[128];
} rte_stream_parameters;

extern rte_codec_info *		rte_codec_info_enum(rte_context *context, int index);
extern rte_codec_info *		rte_codec_info_keyword(rte_context *context, const char *keyword);
extern rte_codec_info *		rte_codec_info_codec(rte_codec *codec);

extern rte_codec *		rte_codec_set(rte_context *context, const char *keyword, int stream_index, void *user_data);
extern void			rte_codec_remove(rte_context *context, rte_stream_type stream_type, int stream_index);
extern rte_codec *		rte_codec_get(rte_context *context, rte_stream_type stream_type, int stream_index);

extern void *			rte_codec_user_data(rte_codec *codec);

extern rte_option_info *	rte_codec_option_info_enum(rte_codec *codec, int index);
extern rte_option_info *	rte_codec_option_info_keyword(rte_codec *codec, const char *keyword);
extern rte_bool			rte_codec_option_get(rte_codec *codec, const char *keyword, rte_option_value *value);
extern rte_bool			rte_codec_option_set(rte_codec *codec, const char *keyword, ...);
extern char *			rte_codec_option_print(rte_codec *codec, const char *keyword, ...);
extern rte_bool			rte_codec_option_menu_get(rte_codec *codec, const char *keyword, int *entry);
extern rte_bool			rte_codec_option_menu_set(rte_codec *codec, const char *keyword, int entry);
extern rte_bool			rte_codec_options_reset(rte_codec *codec);

extern rte_bool			rte_codec_parameters_set(rte_codec *codec, rte_stream_parameters *params);
extern rte_bool			rte_codec_parameters_get(rte_codec *codec, rte_stream_parameters *params);

extern rte_status_info *	rte_codec_status_enum(rte_codec *codec, int n);
extern rte_status_info *	rte_codec_status_keyword(rte_codec *codec, const char *keyword);

/* Private */

#include "context.h"
#include "rte.h"

typedef struct rte_codec_class rte_codec_class;

/*
 *  Codec instance.
 */
struct rte_codec {
	rte_codec *		next;		/* backend use, list of context's codec instances */

	rte_context *		context;	/* parent */
	rte_codec_class *	class;

	void *			user_data;
	int			stream_index;	/* from rte_codec_set() */

	pthread_mutex_t		mutex;

	/*
	 *  Maintained by codec, elsewhere read only. Stream
	 *  parameters are only valid at status RTE_STATUS_PARAM
	 *  or higher.
	 */
	rte_status		status;
	rte_stream_parameters	params;

	/*
	 *  Frontend private
	 */
	rte_io_method		input_method;
	int			input_fd;

// <<
	/* Valid when RTE_STATUS_RUNNING; mutex protected, read only. */
	int64_t			frame_input_count;
	int64_t			frame_input_missed;	/* excl. intent. skipped */
	int64_t			frame_output_count;
	int64_t			byte_output_count;
	double			coded_time_elapsed;
	// XXX?
	double			frame_output_rate;	/* invariable, 1/s */
};

/*
 *  Methods of a codec. No fields may change while a codec->class
 *  is referencing this structure.
 */
struct rte_codec_class {
	rte_codec_class *	next;		/* backend use, list of codec classes */
	rte_codec_info		public;

	/*
	 *  Allocate new codec instance. Returns all fields zero except
	 *  rte_codec.class, .status (RTE_STATUS_NEW) and .mutex (initialized).
	 *  Options to be reset by caller, i. e. backend.
	 */
	rte_codec *		(* new)(rte_codec_class *, char **errstr);
	void			(* delete)(rte_codec *);

	/* Same as frontend version, collectively optional. */
	rte_option_info *	(* option_enum)(rte_codec *, int);
	int			(* option_get)(rte_codec *, const char *, rte_option_value *);
	int			(* option_set)(rte_codec *, const char *, va_list);
	char *			(* option_print)(rte_codec *, const char *, va_list);

	/*
	 *  Same as frontend versions. parameters_set() is mandatory,
	 *  parameters_get() is optional. Then the frontend or backend reads
	 *  the codec.params directly.
	 */
	rte_bool		(* parameters_set)(rte_codec *, rte_stream_parameters *);
	rte_bool		(* parameters_get)(rte_codec *, rte_stream_parameters *);

	/*
	 *  Select input method and put codec into RTE_STATUS_READY, mandatory
	 *  function. Only certain parameter combinations are valid, depending
	 *  on rte_io_method:
	 *  RTE_CALLBACK_ACTIVE   - read_cb, optional (NULL) unref_cb
	 *  RTE_CALLBACK_PASSIVE  - read_cb
	 *  RTE_PUSH_PULL_ACTIVE  - optional (NULL) unref_cb
	 *  RTE_PUSH_PULL_PASSIVE - nothing
	 *  RTE_FIFO              - not defined yet
	 *  A codec need not support all input methods. queue_length (input and
	 *  output), the pointer always valid, applies as defined for the
	 *  rte_set_input_*() functions. Return FALSE on error.
	 */
	rte_bool		(* set_input)(rte_codec *, rte_io_method,
					      rte_buffer_callback read_cb,
					      rte_buffer_callback unref_cb,
					      int *queue_length);

	/* Same as the frontend version, optional. */
	rte_bool		(* push_buffer)(rte_codec *codec, rte_buffer *buffer, rte_bool blocking);

//<<

	rte_status_info *	(* status_enum)(rte_codec *, int);
};

#endif /* CODEC_H */
