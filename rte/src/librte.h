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

/* Generated file, do not edit! */

#ifndef __LIBRTE_H__
#define __LIBRTE_H__

#define RTE_VERSION_MAJOR 0
#define RTE_VERSION_MINOR 5
#define RTE_VERSION_MICRO 4

#ifdef __cplusplus
extern "C" {
#endif

/* option.h */

#include <inttypes.h>

/* doxygen sees static and ignores it... */
#define static_inline static inline

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

typedef unsigned int rte_bool;


typedef enum {
	RTE_OPTION_BOOL = 1,

	RTE_OPTION_INT,

	RTE_OPTION_REAL,

	RTE_OPTION_STRING,

	RTE_OPTION_MENU
} rte_option_type;

typedef union {
	int			num;
	double			dbl;
	char *			str;
} rte_option_value;

typedef union {
	int *			num;
	double *		dbl;
	char **			str;
} rte_option_value_ptr;

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

#define RTE_STATUS_FRAMES_IN		(1 << 3)
#define RTE_STATUS_FRAMES_OUT		(1 << 4)
#define RTE_STATUS_FRAMES_DROPPED	(1 << 5)
#define RTE_STATUS_BYTES_IN		(1 << 6)
#define RTE_STATUS_BYTES_OUT		(1 << 7)
#define RTE_STATUS_CAPTURED_TIME	(1 << 8)
#define RTE_STATUS_CODED_TIME		(1 << 9)


typedef struct {
	unsigned int			valid;

	unsigned int			bytes_per_frame_out;
	double				time_per_frame_out;	

	uint64_t			frames_in;
	uint64_t			frames_out;
	uint64_t			frames_dropped;		

	uint64_t			bytes_in;
	uint64_t			bytes_out;

	double				captured_time;

	double				coded_time;		

	/* future extensions */

} rte_status;

/* context.h */

#include <inttypes.h>

typedef struct rte_context rte_context;

typedef struct {
	const char *		keyword;

	const char *		backend;

	const char *		label;

	const char *		tooltip;

	const char *		mime_type;

	const char *		extension;

	uint8_t			min_elementary[16];
	uint8_t			max_elementary[16];

	unsigned int		flags;
} rte_context_info;

#define RTE_FLAG_SEEKS		(1 << 0)


extern rte_context_info *	rte_context_info_enum(unsigned int index);
extern rte_context_info *	rte_context_info_by_keyword(const char *keyword);
extern rte_context_info *	rte_context_info_by_context(rte_context *context);

extern rte_context *		rte_context_new(const char *keyword, void *user_data, char **errstr);
extern void			rte_context_delete(rte_context *context);

extern void *			rte_context_user_data(rte_context *context);


extern rte_option_info *	rte_context_option_info_enum(rte_context *context, unsigned int index);
extern rte_option_info *	rte_context_option_info_by_keyword(rte_context *context, const char *keyword);
extern rte_bool			rte_context_option_get(rte_context *context, const char *keyword, rte_option_value *value);
extern rte_bool			rte_context_option_set(rte_context *context, const char *keyword, ...);
extern char *			rte_context_option_print(rte_context *context, const char *keyword, ...);
extern rte_bool			rte_context_option_menu_get(rte_context *context, const char *keyword, unsigned int *entry);
extern rte_bool			rte_context_option_menu_set(rte_context *context, const char *keyword, unsigned int entry);
extern rte_bool			rte_context_options_reset(rte_context *context);


#ifndef DOXYGEN_SHOULD_SKIP_THIS
struct rte_codec; /* forward */
extern void			rte_status_query(rte_context *context, struct rte_codec *codec, rte_status *status, unsigned int size);
#endif

static_inline void
rte_context_status(rte_context *context, rte_status *status)
{
	rte_status_query(context, 0, status, sizeof(rte_status));
}


extern void
rte_error_printf		(rte_context *		context,
				 const char *		templ,
				 ...)
  __attribute__ ((format (printf, 2, 3)));
extern char *			rte_errstr(rte_context *context);


/* codec.h */

typedef struct rte_codec rte_codec;

typedef enum {
	RTE_STREAM_VIDEO = 1,	/* XXX STREAM :-( need a better term */
	RTE_STREAM_AUDIO,	
	RTE_STREAM_RAW_VBI,
	RTE_STREAM_SLICED_VBI,
	RTE_STREAM_MAX = 15
} rte_stream_type;

typedef struct {
	rte_stream_type         stream_type;

	const char *		keyword;

	const char *		label;

	const char *		tooltip;
} rte_codec_info;

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

typedef enum {
	RTE_FRAMEFMT_PROGRESSIVE = 1,

	RTE_FRAMEFMT_INTERLACED,

	RTE_FRAMEFMT_ALTERNATING,
} rte_framefmt;

typedef struct {
	rte_framefmt		framefmt;
	rte_pixfmt		pixfmt;

	rte_bool		spatial_order;

	rte_bool		temporal_order;

	double			frame_rate;

	double			sample_aspect;

	unsigned int		width, height;

	unsigned int		offset;

	unsigned int		u_offset, v_offset;

	unsigned int		stride;

	unsigned int		uv_stride;

	unsigned int		frame_size;
} rte_video_stream_params;

typedef enum {
	RTE_SNDFMT_S8 = 1,
	RTE_SNDFMT_U8,
	RTE_SNDFMT_S16_LE,
	RTE_SNDFMT_S16_BE,
	RTE_SNDFMT_U16_LE,
	RTE_SNDFMT_U16_BE
} rte_sndfmt;

typedef struct {
	rte_sndfmt		sndfmt;

	unsigned int		sampling_freq;

	unsigned int		channels;

	unsigned int		fragment_size;
} rte_audio_stream_params;

typedef union {
	rte_video_stream_params	video;
	rte_audio_stream_params	audio;
#ifdef __LIBZVBI_H__
	/* Raw and sliced vbi params will go here */
#endif
	char			pad[128];
} rte_stream_parameters;

extern rte_codec_info *		rte_codec_info_enum(rte_context *context, unsigned int index);
extern rte_codec_info *		rte_codec_info_by_keyword(rte_context *context, const char *keyword);
extern rte_codec_info *		rte_codec_info_by_codec(rte_codec *codec);

extern rte_codec *		rte_set_codec(rte_context *context, const char *keyword, unsigned int stream_index, void *user_data);
extern void			rte_codec_delete(rte_codec *codec);

extern void *			rte_codec_user_data(rte_codec *codec);


extern rte_option_info *	rte_codec_option_info_enum(rte_codec *codec, unsigned int index);
extern rte_option_info *	rte_codec_option_info_by_keyword(rte_codec *codec, const char *keyword);
extern rte_bool			rte_codec_option_get(rte_codec *codec, const char *keyword, rte_option_value *value);
extern rte_bool			rte_codec_option_set(rte_codec *codec, const char *keyword, ...);
extern char *			rte_codec_option_print(rte_codec *codec, const char *keyword, ...);
extern rte_bool			rte_codec_option_menu_get(rte_codec *codec, const char *keyword, int *entry);
extern rte_bool			rte_codec_option_menu_set(rte_codec *codec, const char *keyword, int entry);
extern rte_bool			rte_codec_options_reset(rte_codec *codec);


extern rte_bool			rte_parameters_set(rte_codec *codec, rte_stream_parameters *params);
extern rte_bool			rte_parameters_get(rte_codec *codec, rte_stream_parameters *params);


extern rte_bool			rte_codec_parameters_set(rte_codec *codec, rte_stream_parameters *params);
extern rte_bool			rte_codec_parameters_get(rte_codec *codec, rte_stream_parameters *params);


static_inline void
rte_codec_status(rte_codec *codec, rte_status *status)
{
	rte_status_query(0, codec, status, sizeof(rte_status));
}


extern rte_codec *		rte_get_codec(rte_context *context, rte_stream_type stream_type, int stream_index);
extern void			rte_remove_codec(rte_context *context, rte_stream_type stream_type, unsigned int stream_index);


/* rte.h */

typedef struct {
	void *			data;
	unsigned int		size;
	double			timestamp;
	void *			user_data;
} rte_buffer;

typedef rte_bool (* rte_buffer_callback)(rte_context *context,
					 rte_codec *codec,
					 rte_buffer *buffer);

typedef rte_bool (*rte_seek_callback)(rte_context *context,
				      long long offset,
				      int whence);

extern rte_bool			rte_set_input_callback_master(rte_codec *codec, rte_buffer_callback read_cb, rte_buffer_callback unref_cb, unsigned int *queue_length);
extern rte_bool			rte_set_input_callback_slave(rte_codec *codec, rte_buffer_callback read_cb);
extern rte_bool			rte_set_input_push_master(rte_codec *codec, rte_buffer_callback unref_cb, unsigned int queue_request, unsigned int *queue_length);
extern rte_bool			rte_set_input_push_slave(rte_codec *codec, unsigned int queue_request, unsigned int *queue_length);

extern rte_bool			rte_push_buffer(rte_codec *codec, rte_buffer *buffer, rte_bool blocking);


extern rte_bool			rte_set_output_callback_slave(rte_context *context, rte_buffer_callback write_cb, rte_seek_callback seek_cb);
extern rte_bool			rte_set_output_stdio(rte_context *context, int fd);
extern rte_bool			rte_set_output_file(rte_context *context, const char *filename);
extern rte_bool			rte_set_output_discard(rte_context *context);


extern rte_bool			rte_start(rte_context *context, double timestamp, rte_codec *sync_ref, rte_bool async);
extern rte_bool			rte_stop(rte_context *context, double timestamp);



#ifdef __cplusplus
}
#endif

#endif /* __LIBRTE_H__ */
