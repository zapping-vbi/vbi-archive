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

/* $Id: codec.h,v 1.5 2002-08-22 22:07:24 mschimek Exp $ */

#ifndef CODEC_H
#define CODEC_H

#include "option.h"

/* Public */

/**
 * @ingroup Codec
 * Opaque rte_codec object. You can allocate an rte_codec with
 * rte_set_codec().
 */
typedef struct rte_codec rte_codec;

/**
 * @ingroup Codec
 * Basic type of the data processed by a codec.
 */
typedef enum {
	RTE_STREAM_VIDEO = 1,	/* XXX STREAM :-( need a better term */
	RTE_STREAM_AUDIO,	
	RTE_STREAM_RAW_VBI,
	RTE_STREAM_SLICED_VBI,
	RTE_STREAM_MAX = 15
} rte_stream_type;

/**
 * @ingroup Codec
 * Details about the codec.
 */
typedef struct {
	/**
	 * Whether this is a video, audio or other kind of codec.
	 */
	rte_stream_type         stream_type;

	/**
	 * Uniquely (within the parent context) identifies the codec,
	 * this may be stored in a config file.
	 */
	const char *		keyword;

	/**
	 * @p label is a name for the codec to be presented to the user,
	 *  can be localized with gettext(label).
	 */
	const char *		label;

	/**
	 * Gives additional info for the user, also localized.
	 *   This pointer can be @c NULL.
	 */
	const char *		tooltip;
} rte_codec_info;

/**
 * @ingroup Param
 * Image format used in a video input stream.
 *
 * @htmlonly
 * <table border=1>
 * <tr><th>Symbol</th><th>Byte&nbsp;0</th><th>Byte&nbsp;1</th><th>Byte&nbsp;2</th><th>Byte&nbsp;3</th></tr>
 * <tr><td colspan=5>Planar YUV 4:2:0 data.</td></tr>
 * <tr><td>RTE_PIXFMT_YUV420</td><td colspan=4>
 *  <table>
 *   <tr><th>Y plane</th><th>U plane</th><th>V plane</th></tr>
 *   <tr><td><table border=1>
 *    <tr><td>Y00</td><td>Y01</td><td>Y02</td><td>Y03</td></tr>
 *    <tr><td>Y10</td><td>Y11</td><td>Y12</td><td>Y13</td></tr>
 *    <tr><td>Y20</td><td>Y21</td><td>Y22</td><td>Y23</td></tr>
 *    <tr><td>Y30</td><td>Y31</td><td>Y32</td><td>Y33</td></tr>
 *   </table></td>
 *   <td><table border=1>
 *    <tr><td>Cb00</td><td>Cb01</td></tr>
 *    <tr><td>Cb10</td><td>Cb11</td></tr>
 *   </table></td>
 *   <td><table border=1>
 *    <tr><td>Cr00</td><td>Cr01</td></tr>
 *    <tr><td>Cr10</td><td>Cr11</td></tr>
 *   </table></td>
 *  </tr></table></td>
 * </tr>
 * <tr><td colspan=5>Packed YUV 4:2:2 data.</td></tr>
 * <tr><td>RTE_PIXFMT_YUYV</td><td>Y0</td><td>Cb</td><td>Y1</td><td>Cr</td></tr>
 * <tr><td>RTE_PIXFMT_YVYU</td><td>Y0</td><td>Cr</td><td>Y1</td><td>Cb</td></tr>
 * <tr><td>RTE_PIXFMT_UYVY</td><td>Cb</td><td>Y0</td><td>Cr</td><td>Y1</td></tr>
 * <tr><td>RTE_PIXFMT_VYUY</td><td>Cr</td><td>Y0</td><td>Cb</td><td>Y1</td></tr>
 * <tr><td colspan=5>Packed 32 bit RGB data.</td></tr>
 * <tr><td>RTE_PIXFMT_RGBA32_LE RTE_PIXFMT_ARGB32_BE</td>
 * <td>r7&nbsp;...&nbsp;r0</td><td>g7&nbsp;...&nbsp;g0</td>
 * <td>b7&nbsp;...&nbsp;b0</td><td>a7&nbsp;...&nbsp;a0</td></tr>
 * <tr><td>RTE_PIXFMT_BGRA32_LE RTE_PIXFMT_ARGB32_BE</td>
 * <td>b7&nbsp;...&nbsp;b0</td><td>g7&nbsp;...&nbsp;g0</td>
 * <td>r7&nbsp;...&nbsp;r0</td><td>a7&nbsp;...&nbsp;a0</td></tr>
 * <tr><td>RTE_PIXFMT_ARGB32_LE RTE_PIXFMT_BGRA32_BE</td>
 * <td>a7&nbsp;...&nbsp;a0</td><td>r7&nbsp;...&nbsp;r0</td>
 * <td>g7&nbsp;...&nbsp;g0</td><td>b7&nbsp;...&nbsp;b0</td></tr>
 * <tr><td>RTE_PIXFMT_ABGR32_LE RTE_PIXFMT_RGBA32_BE</td>
 * <td>a7&nbsp;...&nbsp;a0</td><td>b7&nbsp;...&nbsp;b0</td>
 * <td>g7&nbsp;...&nbsp;g0</td><td>r7&nbsp;...&nbsp;r0</td></tr>
 * <tr><td colspan=5>Packed 24 bit RGB data.</td></tr>
 * <tr><td>RTE_PIXFMT_RGBA24</td>
 * <td>r7&nbsp;...&nbsp;r0</td><td>g7&nbsp;...&nbsp;g0</td>
 * <td>b7&nbsp;...&nbsp;b0</td><td>&nbsp;</td></tr>
 * <tr><td>RTE_PIXFMT_BGRA24</td>
 * <td>b7&nbsp;...&nbsp;b0</td><td>g7&nbsp;...&nbsp;g0</td>
 * <td>r7&nbsp;...&nbsp;r0</td><td>&nbsp;</td></tr>
 * <tr><td colspan=5>Packed 16 bit RGB data.</td></tr>
 * <tr><td>RTE_PIXFMT_RGB16_LE</td>
 * <td>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</td>
 * <td>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g5&nbsp;g4&nbsp;g3</td>
 * <td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>RTE_PIXFMT_BGR16_LE</td>
 * <td>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</td>
 * <td>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g5&nbsp;g4&nbsp;g3</td>
 * <td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>RTE_PIXFMT_RGB16_BE</td>
 * <td>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g5&nbsp;g4&nbsp;g3</td>
 * <td>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</td>
 * <td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>RTE_PIXFMT_BGR16_BE</td>
 * <td>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g5&nbsp;g4&nbsp;g3</td>
 * <td>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</td>
 * <td>&nbsp;</td><td>&nbsp;</td></tr>
 * <tr><td colspan=5>Packed 15 bit RGB data.</td></tr>
 * <tr><td>RTE_PIXFMT_RGBA15_LE</td>
 * <td>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</td>
 * <td>a0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3</td>
 * <td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>RTE_PIXFMT_BGRA15_LE</td>
 * <td>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</td>
 * <td>a0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3</td>
 * <td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>RTE_PIXFMT_ARGB15_LE</td>
 * <td>g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;a0</td>
 * <td>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3&nbsp;g2</td>
 * <td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>RTE_PIXFMT_ABGR15_LE</td>
 * <td>g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;a0</td>
 * <td>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3&nbsp;g2</td>
 * <td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>RTE_PIXFMT_RGBA15_BE</td>
 * <td>a0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3</td>
 * <td>g2&nbsp;g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0</td>
 * <td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>RTE_PIXFMT_BGRA15_BE</td>
 * <td>a0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3</td>
 * <td>g2&nbsp;g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0</td>
 * <td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>RTE_PIXFMT_ARGB15_BE</td>
 * <td>b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;g4&nbsp;g3&nbsp;g2</td>
 * <td>g1&nbsp;g0&nbsp;r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;a0</td>
 * <td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>RTE_PIXFMT_ABGR15_BE</td>
 * <td>r4&nbsp;r3&nbsp;r2&nbsp;r1&nbsp;r0&nbsp;g4&nbsp;g3&nbsp;g2</td>
 * <td>g1&nbsp;g0&nbsp;b4&nbsp;b3&nbsp;b2&nbsp;b1&nbsp;b0&nbsp;a0</td>
 * <td>&nbsp;</td><td>&nbsp;</td></tr>
 * </table>
 */
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
	RTE_PIXFMT_ABGR32_BE = 32, /* synonyms */
	RTE_PIXFMT_ABGR32_LE,
	RTE_PIXFMT_ARGB32_BE,
	RTE_PIXFMT_ARGB32_LE,
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

/**
 * @ingroup Param
 * Video input stream parameters.
 */
typedef struct {
	rte_pixfmt		pixfmt;

	/**
	 * @p frame_rate (for example 24.0, 25.0, 30000 / 1001.0) is the
	 * nominal frame sampling rate, usually derived from the video
	 * standard. The codec may compare this value against frame
	 * timestamps to detect frame dropping.
	 */
	double			frame_rate;

	/**
	 * Sampling aspect ratio, y / x.
	 * Examples:
	 * - Square pixels: 1.0
	 * - ITU-R Rec. 601 PAL or SECAM: 54.0 / 59.0
	 * - ITU-R Rec. 601 NTSC: 11.0 / 10.0
	 *
-	 * The codec may store this information
	 * in the stream for correct playback.
	 */
	double			sample_aspect;

	/**
	 * The image size in pixels. The encoded
	 * image size can be different due to scaling options. When the
	 * format is RTE_PIXFMT_YUV420 this refers to the luminance plane, and
	 * both @p width and @p height must be multiples of two. When the format is
	 * RTE_PIXFMT_YUYV (or its variants) @p width must be a multiple of two.
	 * Still a codec may modify the proposed width and height if only
	 * descrete values or other multiples are possible.
	 */
	unsigned int		width, height;

	/**
	 * Distance in bytes from the start of the image
	 * to the top- and leftmost pixel to be encoded: x0 * bytes
	 * per pixel + y1 * @p stride. Usually this value is zero. When
	 * the format is RTE_PIXFMT_YUV420 this refers to the luminance
	 * plane only. Aligning at addresses which are a multiple of a
	 * power of two may speed up encoding.
	 */
	unsigned int		offset;

	/**
	 * @p u_offset and @p v_offset apply only when the format is RTE_PIXFMT_YUV420,
	 * expressing the offset of the U (Cb) and V (Cr) plane from the
	 * start of the image in bytes.
	 */
	unsigned int		u_offset, v_offset;

	/**
	 * The distance from one pixel to the adjacent pixel in the
	 * next line, in bytes. When the format is RTE_PIXFMT_YUV420 this refers
	 * to the luminance plane only. Aligning lines at addresses which are a
	 * multiple of a power of two may speed up encoding. Usually @p stride
	 * is @p width * bytes per pixel, but a codec may accept any larger or
	 * smaller, even negative values.
	 */
	unsigned int		stride;

	/**
	 * @p uv_stride applies only when the format is RTE_PIXFMT_YUV420, the
	 * distance from one pixel to the adjacent pixel in the next line, in
	 * bytes, of both the U (Cb) and V (Cr) plane. Usually this value
	 * is @p stride / 2.
	 */
	unsigned int		uv_stride;

	/**
	 * Indicates the size of the frame buffer relative to its
	 * start which can be safely accessed by the encoder. The image as
	 * described by @p width, @p height, the offsets and strides must lie
	 * completely within these bounds. When @p frame_size is zero only the
	 * actual image data can be accessed (which may be less efficient
	 * depending on alignment, and such a configuration may be rejected by
	 * a codec altogether).
	 */
	unsigned int		frame_size;
} rte_video_stream_params;

/**
 * @ingroup Param
 * Sample format used in a audio input stream.
 *
 * @htmlonly
 * <table border=1>
 * <tr><th>Symbol</th><th>Meaning</th><tr>
 * <tr><td>RTE_SNDFMT_S8</td><td>Signed 8 bit samples</td></tr> 
 * <tr><td>RTE_SNDFMT_U8</td><td>Unsigned 8 bit samples</td></tr>
 * <tr><td>RTE_SNDFMT_S16_LE</td><td>Signed 16 bit samples, little endian</td></tr>
 * <tr><td>RTE_SNDFMT_S16_BE</td><td>Signed 16 bit samples, big endian</td></tr>
 * <tr><td>RTE_SNDFMT_U16_LE</td><td>Unsigned 16 bit samples, little endian</td></tr>
 * <tr><td>RTE_SNDFMT_U16_BE</td><td>Unsigned 16 bit samples, big endian</td></tr>
 * </table>
 */
typedef enum {
	RTE_SNDFMT_S8 = 1,
	RTE_SNDFMT_U8,
	RTE_SNDFMT_S16_LE,
	RTE_SNDFMT_S16_BE,
	RTE_SNDFMT_U16_LE,
	RTE_SNDFMT_U16_BE
} rte_sndfmt;

/**
 * @ingroup Param
 * Audio input stream parameters.
 */
typedef struct {
	rte_sndfmt		sndfmt;

	/**
	 * Sampling frequency in Hz.
	 */
	unsigned int		sampling_freq;

	/**
	 * Number of audio channels: mono 1, stereo 2.
	 *
	 * In stereo mode the sample order is left0, right0, left1, right1, ...
	 * Order of surround channels to be defined.
	 */
	unsigned int		channels;

	/**
	 * Audio data is passed in blocks rather than
	 * single samples. The codec may require a certain number of
	 * samples at once, modifying this value accordingly. This
	 * size is given in bytes, but must be divisible
	 * by sample size in bytes times @p channels.
	 */
	unsigned int		fragment_size;
} rte_audio_stream_params;

/**
 * @ingroup Param
 * Stream parameters union used with
 * rte_parameters_set() and rte_parameters_get().
*/
typedef union {
	rte_video_stream_params	video;
	rte_audio_stream_params	audio;
#ifdef __LIBZVBI_H__
	/* Raw and sliced vbi params will go here */
#endif
	char			pad[128];
} rte_stream_parameters;

/**
 * @addtogroup Codec
 * @{
 */
extern rte_codec_info *		rte_codec_info_enum(rte_context *context, unsigned int index);
extern rte_codec_info *		rte_codec_info_by_keyword(rte_context *context, const char *keyword);
extern rte_codec_info *		rte_codec_info_by_codec(rte_codec *codec);

extern rte_codec *		rte_set_codec(rte_context *context, const char *keyword, unsigned int stream_index, void *user_data);
extern void			rte_codec_delete(rte_codec *codec);

extern void *			rte_codec_user_data(rte_codec *codec);
/** @} */

/**
 * @addtogroup Codec
 * @{
 */
extern rte_option_info *	rte_codec_option_info_enum(rte_codec *codec, unsigned int index);
extern rte_option_info *	rte_codec_option_info_by_keyword(rte_codec *codec, const char *keyword);
extern rte_bool			rte_codec_option_get(rte_codec *codec, const char *keyword, rte_option_value *value);
extern rte_bool			rte_codec_option_set(rte_codec *codec, const char *keyword, ...);
extern char *			rte_codec_option_print(rte_codec *codec, const char *keyword, ...);
extern rte_bool			rte_codec_option_menu_get(rte_codec *codec, const char *keyword, int *entry);
extern rte_bool			rte_codec_option_menu_set(rte_codec *codec, const char *keyword, int entry);
extern rte_bool			rte_codec_options_reset(rte_codec *codec);
/** @} */

/**
 * @addtogroup Param
 * @{
 */
extern rte_bool			rte_parameters_set(rte_codec *codec, rte_stream_parameters *params);
extern rte_bool			rte_parameters_get(rte_codec *codec, rte_stream_parameters *params);
/** @} */

/**
 * @addtogroup Status
 * @{
 */
static_inline void
rte_codec_status(rte_codec *codec, rte_status *status)
{
	extern void rte_status_query(rte_context *context, rte_codec *codec,
				     rte_status *status, unsigned int size);

	rte_status_query(0, codec, status, sizeof(rte_status));
}
/** @} */

/**
 * @addtogroup Context
 * @{
 */
extern rte_codec *		rte_get_codec(rte_context *context, rte_stream_type stream_type, int stream_index);
extern void			rte_remove_codec(rte_context *context, rte_stream_type stream_type, unsigned int stream_index);
/** @} */

/* Private */

#include "context.h"
#include "rte.h"

/**
 * @ingroup Backend
 * Part of the backend interface.
 */
typedef struct rte_codec_class rte_codec_class;

/**
 * @ingroup Backend
 * Codec instance.
 * Part of the backend interface.
 */
struct rte_codec {
	rte_codec *		next;		/**< Backend/context use, list of context's codec instances */

	rte_context *		context;	/**< Parent */
	rte_codec_class *	_class;		/**< Methods of this codec */

	void *			user_data;	/**< Frontend private */

	unsigned int		stream_index;	/**< From rte_set_codec(), maintained by backend/context */

	pthread_mutex_t		mutex;		/**< Codec use */

	/**
	 * The codec module shall set this value as described
	 * in the rte_state documentation. Elsewhere read only.
	 */
	rte_state		state;

	/**
	 * Maintained by codec, from rte_parameters_set(). A higher layer will
	 * access this data directly in absence of a get function,
	 * it is only valid at rte_state @c RTE_STATUS_PARAM or higher.
	 */
	rte_stream_parameters	params;

	rte_io_method		input_method;	/**< Frontend private */
	int			input_fd;	/**< Frontend private */
};

/**
 * @ingroup Backend
 * Methods of a codec. No fields may change while a codec->_class
 * is referencing this structure. Part of the backend interface.
 */
struct rte_codec_class {
	/**
	 * Backend/context/codec use, list of codec classes.
	 */
	rte_codec_class *	next;

	/**
	 * Codecs can use this to store rte_codec_info, the
	 * field is not directly accessed by the frontend.
	 */
	rte_codec_info		_public;

	/**
	 * Allocate new codec instance. Returns all fields zero except
	 * rte_codec->_class, ->state (@c RTE_STATE_NEW) and ->mutex (initialized).
	 * Options are reset by frontend when _new() succeeded.
	 *
	 * When the allocation fails, return @c NULL and set @p errstr to a
	 * description of the problem for the user. Subsequently (after the rte_codec
	 * has been attached to a rte_context) the codec shall call rte_error_printf()
	 * et al to pass error strings.
	 */
	rte_codec *		(* _new)(rte_codec_class *, char **errstr);
	/** Delete codec instance */
	void			(* _delete)(rte_codec *);

	/** Same as frontend version, optional. */
	rte_option_info *	(* option_enum)(rte_codec *, unsigned int);
	/** Same as frontend version, optional when @p option_enum is. */
	rte_bool		(* option_get)(rte_codec *, const char *, rte_option_value *);
	/** Same as frontend version, optional when @p option_enum is. */
	rte_bool		(* option_set)(rte_codec *, const char *, va_list);
	/** Same as frontend version, optional when @p option_enum is. */
	char *			(* option_print)(rte_codec *, const char *, va_list);

	/**
	 * Same as frontend versions. @p parameters_set is mandatory,
	 * @p parameters_get is optional. A higher layer will
	 * access this data directly in absence of a get function,
	 * it is only valid at rte_state @c RTE_STATUS_PARAM or higher.
	 */
	rte_bool		(* parameters_set)(rte_codec *, rte_stream_parameters *);
	rte_bool		(* parameters_get)(rte_codec *, rte_stream_parameters *);

	/**
	 * Select input method and put codec into @c RTE_STATUS_READY, mandatory
	 * function. Only certain parameter combinations are valid, depending
	 * on rte_io_method:
	 * <table>
	 * <tr><td>RTE_CALLBACK_MASTER</td><td>read_cb, optional (@c NULL) unref_cb</td></tr>
	 * <tr><td>RTE_CALLBACK_SLAVE</td><td>read_cb</td></tr>
	 * <tr><td>RTE_PUSH_MASTER</td><td>optional (@c NULL) unref_cb</td></tr>
	 * <tr><td>RTE_PUSH_SLAVE</td><td>nothing</td></tr>
	 * <tr><td>RTE_FIFO</td><td>not defined yet</td></tr>
	 * </table>
	 * A codec need not support all input methods. @p queue_length (input and
	 * output), the pointer always valid, applies as defined for the
	 * rte_set_input_callback_master() et al functions. Return @c FALSE on error.
	 */
	rte_bool		(* set_input)(rte_codec *, rte_io_method,
					      rte_buffer_callback read_cb,
					      rte_buffer_callback unref_cb,
					      int *queue_length);

	/**
	 * Same as the frontend version, optional if the push i/o
	 * method is not supported by the codec.
	 */
	rte_bool		(* push_buffer)(rte_codec *codec, rte_buffer *buffer, rte_bool blocking);
};

#endif /* CODEC_H */
