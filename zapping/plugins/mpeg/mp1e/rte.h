/*
 *  MPEG-1 Real Time Encoder lib wrapper api
 *
 *  Copyright (C) 2000 Iñaki García Etxebarria
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

/*
 * $Id: rte.h,v 1.17 2000-11-04 20:38:02 garetxe Exp $
 * Function prototypes for RTE
 */

/* FIXME: We need declaring things as const */
/* FIXME: global_context must disappear */
/* FIXME: All these huge comments should go into a ref manual */

#ifndef __RTELIB_H__
#define __RTELIB_H__

/*
 * What are we going to encode, audio only, video only or both
 * FIXME: subtitles?
 */
enum rte_mux_mode {
	RTE_VIDEO = 1,
	RTE_AUDIO = 2,
	RTE_AUDIO_AND_VIDEO = 3 /* AUDIO | VIDEO */
};

/*
 * Which interface to use for fetching data
 */
enum rte_interface {
	RTE_NO_INTERFACE = 0,
	RTE_PUSH = 1,
	RTE_CALLBACKS = 2
};

/*
  Supported pixformats. This is the pixformat the pushed video frames
  will be pushed in, the output (encoded) format is the one specified
  by the MPEG standard (YCbCr)
  The YUV formats are in the YCbCr colorspace
*/
enum rte_pixformat {
	RTE_YUV420, /* Planar Y:Cb:Cr 1.5 bytes per pixel */
	RTE_YUYV, /* YCbYCr linear, 2 bytes per pixel */
	/* RGB modes, get converted to YUV420 */
	/* fixme: the rgb modes are currently unsupported in buffered
	   mode, and very slow otherwise. Somebody has to fix this. */
	RTE_RGB555,
	RTE_RGB565,
	RTE_BGR24,
	RTE_RGB24,
	RTE_BGR32,
	RTE_RGB32,
	/* these are the additional formats mp1e supports */
	/* in decimation modes the height of the buffer you have to
	   fill is twice the height of the resulting image */
	RTE_YUYV_VERTICAL_DECIMATION,
	RTE_YUYV_TEMPORAL_INTERPOLATION,
	RTE_YUYV_VERTICAL_INTERPOLATION,
	RTE_YUYV_PROGRESSIVE,
	RTE_YUYV_PROGRESSIVE_TEMPORAL,
	RTE_YUYV_EXP,
	RTE_YUYV_EXP_VERTICAL_DECIMATION,
	RTE_YUYV_EXP2
};

/*
  Video frame rate. From the standard.
*/
enum rte_frame_rate {
	RTE_RATE_NORATE=0, /* frame rate not set, rte won't encode */
	RTE_RATE_1, /* 23.976 Hz (3-2 pulldown NTSC) */
	RTE_RATE_2, /* 24 Hz (film) */
	RTE_RATE_3, /* 25 Hz (PAL/SECAM or 625/60 video) */
	RTE_RATE_4, /* 29.97 (NTSC) */
	RTE_RATE_5, /* 30 Hz (drop-frame NTSC or component 525/60) */
	RTE_RATE_6, /* 50 Hz (double rate PAL) */
	RTE_RATE_7, /* 59.97 Hz (double rate NTSC) */
	RTE_RATE_8, /* 60 Hz (double rate drop-frame NTSC/component
		       525/60 video) */
};

/*
  Available audio modes
*/
enum rte_audio_mode {
	RTE_AUDIO_MODE_MONO,
	RTE_AUDIO_MODE_STEREO
	/* fixme: what does this mean? */
//	RTE_AUDIO_MODE_DUAL_CHANNEL
};

typedef struct _rte_context_private rte_context_private;

typedef struct {
	/* Filename used when creating this context, NULL if unknown */
	char * file_name;
	/* Whether to encode audio only, video only or both */
	enum rte_mux_mode mode;

	/******** video parameters **********/
	/* pixformat the passed video data is in */
	enum rte_pixformat video_format;
	/* frame size */
	int width, height;

	/* Video frame rate (25 fps (PAL) by default) */
	enum rte_frame_rate video_rate;
	/* output video bits per second, defaults to 2.3 Mbit/s */
	size_t output_video_bits;

	/* size in bytes of a complete frame */
	int video_bytes;
	
	/******* audio parameters **********/
	/* audio sampling rate in Hz, 44100 by default */
	int audio_rate; 
	/* Audio mode, defaults to Mono */
	enum rte_audio_mode audio_mode;
	/* output audio bits per second, defaults to 80Kbit/s */
	size_t output_audio_bits;
	/* size in bytes of an audio frame */
	int audio_bytes;

	/* last error */
	char * error;

	/* Stuff we don't want you to see ;-) */
	rte_context_private * private;
} rte_context;

/*
  "You have to save this data" callback. Defaults to a disk (stdout)
  write().
  context: Context that created this data.
  data: Pointer to the encoded data.
  size: Size in bytes of the data stored in data
  user_data: Pointer passed to rte_context_new
*/
typedef void (*rteEncodeCallback)(rte_context * context,
				  void * data,
				  size_t size,
				  void * user_data);

#define RTE_ENCODE_CALLBACK(function) ((rteEncodeCallback)function)

/*
  "I need more data" callback. The input thread will call this
  callback whenever it thinks it will need some fresh
  data to encode (usually it will go one or two samples ahead of the
  encoder thread). The callbacks and the push() interfaces shouldn't
  be used together (rte isn't designed with that in mind, but it could
  work?).
  context: The context that asks for the data.
  data: Where should you write the data. It's a memchunk of
  context->video_bytes or context->audio_bytes (depending whether rte
  asks for video or audio), and you should fill it (i.e., a video
  frame of audio_bytes of audio).
  time: Push here the timestamp for your frame.
  stream: What are we requesting (RTE_AUDIO or RTE_VIDEO)
  user_data: User data passed to rte_context_new
*/
typedef void (*rteDataCallback)(rte_context * context,
				void * data,
				double * time,
				enum rte_mux_mode stream,
				void * user_data);

#define RTE_DATA_CALLBACK(function) ((rteDataCallback)function)

/*
 * Struct used for buffered input. We could use mp1e's own buffers and
 * save some complexity ... or maybe add some more :-)
 */
typedef struct {
	void	*data; /* Pointer to the data in the buffer */
	double	time; /* timestamp for the buffer */
	void	*user_data; /* Whatever data the user wants to store */
} rte_buffer;

/*
 * "I need a buffer" callback. The usage is the same as a
 * data callback, but you don't need to do the memcpy.
 * buffer: Buffer the lib will be using for encoding. Set data, time
 *	   and optionally user_data to the correct values.
 * stream: What are we requesting
 */
typedef void (*rteBufferCallback)(rte_context * context,
				  rte_buffer * buffer,
				  enum rte_mux_mode stream);

#define RTE_BUFFER_CALLBACK(function) ((rteBufferCallback)function)

/*
 * Callback used when the encoding engine no longer needs a supplied
 * buffer.
 * The buffer is property of rte, you just need to free whatever data
 * you provided when pushing or giving it through a callback.
 * buffer: The buffer no longer needed by the encoding engine.
 */
typedef void (*rteUnrefCallback)(rte_context * contex,
				 rte_buffer * buffer);

#define RTE_UNREF_CALLBACK(function) ((rteUnrefCallback)function)

/* Interface functions */
/*
  Inits the lib, and it does some checks.
  Returns 1 if the lib can be used in this box, and 0 if not.
*/
int rte_init ( void );

/*
  Creates a rte encoding context. The compulsory parameters are here,
  if the rest aren't specified before pushing data, then the defaults
  are used.
  Returns: The new context on startup, NULL on error.
  width, height: Width and height of the pushed frames, must be 16-multiplus
  user_data: Some data you would like to pass to the callback
*/
rte_context * rte_context_new (int width, int height,
			       enum rte_pixformat frame_format,
			       void * user_data);

/*
  Destroys the encoding context and (if encoding) stops encoding. It
  frees the memory allocated by the rte_context.
  Returns: Always NULL
*/
void * rte_context_destroy ( rte_context * context );

/*
 * Sets the a/v input mode for the context.
 * stream: What are we setting (RTE_AUDIO or RTE_VIDEO)
 * interface: interface to use (push or callback)
 * buffered: TRUE if buffering is to be used.
 * *_callback: Supply the appropiate callbacks here (NULL if not needed).
 */
void rte_set_input (rte_context * context,
		    enum rte_mux_mode stream,
		    enum rte_interface interface, int buffered,
		    rteDataCallback data_callback,
		    rteBufferCallback buffer_callback,
		    rteUnrefCallback unref_callback);

/*
 * Sets the what should rte do with the encoded data.
 * encode_callback: Callback when an encoded packet is ready, can be NULL.
 * filename: if encode_callback is NULL, put the file where rte will write
 * the data here. No verification is done until the file is opened.
 */
void rte_set_output (rte_context * context,
		     rteEncodeCallback encode_callback,
		     const char * filename);

/*
  Setters and getters for the members in the struct, so no direct
  access is needed. Direct access to the struct fields is allowed when
  no getter is provided (this is to avoid API bloat), but you should
  NEVER change a field directly.
  fixme: We need some functions to get stats (frame drop rate, bytes
  written, etc)
*/
/*
  Sets the video parameters. If you want to leave output_video_bits
  unmodified (for example), use context->output_video_bits
  Returns 0 on error
*/
int rte_set_video_parameters (rte_context * context,
			      enum rte_pixformat frame_format,
			      int width, int height,
			      enum rte_frame_rate video_rate,
			      size_t output_video_bits);

/* Sets the audio parameters, 0 on error */
int rte_set_audio_parameters (rte_context * context,
			      int audio_rate,
			      enum rte_audio_mode audio_mode,
			      size_t output_audio_bits);

/* Specifies whether to encode audio only, video only or both */
void rte_set_mode (rte_context * context, enum rte_mux_mode mode);

/* [SG]ets the user data parameter. Can be done while encoding */
void rte_set_user_data(rte_context * context, void * user_data);
void * rte_get_user_data(rte_context * context);

/*
 * Prepares the context for encoding. Must be called before calling
 * start_encoding. Returns 1 on success.
 */
int rte_init_context ( rte_context * context );

/*
 * If necessary, syncs the audio and video streams and starts
 * encoding. The context must be sucessfully inited before calling
 * this.
 * Returns1 on success.
 */
int rte_start_encoding ( rte_context * context );

/*
  Stops encoding frames. Usually you won't call this, but
  rte_context_destroy. This has the advantage that the current setting
  are kept, and rte_start can be called again on the same context.
  It flushes output buffers too.
*/
void rte_stop ( rte_context * context );

/*
  Pushes a video frame into the given encoding context.
  data: Pointer to the data to encode, it must be a complete frame as
  described in the context.
  time: Timestamp given to the frame, in seconds
  Returns: A pointer to the buffer where you should write the next
  frame. This way a redundant memcpy() is avoided. You can call
  push_video_data with data = NULL to get the first buffer.
  The size allocated for the buffer is in context->video_bytes

  void * ptr = rte_push_video_data(context, NULL, 0);
  do {
  double timestamp;
  get_data_from_video_source(ptr, &timestamp);
  ptr = rte_push_video_data(context, ptr, timestamp);
  } while (data_available);
*/
void * rte_push_video_data ( rte_context * context, void * data,
			     double time );

/*
 * This is the same as push_video_data but it uses buffers, thus saving a
 * memcpy. You should fill in the fields of buffer as needed.
 */
void rte_push_video_buffer ( rte_context * context,
			     rte_buffer * buffer );

/*
  Pushes an audio sample into the given encoding context. The usage is
  similar to push_video_data.
  When you push one sample, it is assumed that it contains
  context->audio_bytes of audio data with the current context audio
  parameters.
  The expected audio format is signed 16-bit Little Endian.
  A sample is structured as follows:
  2 * (stereo ? 1:2) * 1632 bytes of audio, where the first 1152
  audio atoms (2* (stereo ? 1:2) bytes of audio are considered an
  atom) are data, and the remaining 480 atoms belong to the next frame
  (so the last atoms will be pushed in the beginning of the next frame).
  data: pointer to the data to encode.
  time: Timestamp given to the frame, in seconds.
  Returns: A pointer to the buffer where you should write the next sample
*/
void * rte_push_audio_data ( rte_context * context, void * data,
			     double time );

/*
 * This is the same as push_audio_data but it uses buffers, thus saving a
 * memcpy. You should fill in the fields of buffer as needed.
 */
void rte_push_audio_buffer ( rte_context * context,
			     rte_buffer * buffer );

/*
 * Sets the verbosity value.
 */
void rte_set_verbosity ( rte_context * context, int level );

/*
 * Returns the current verbosity value
 */
int rte_get_verbosity ( rte_context * context );

/*
 * Some useful stuff
 */
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif

#endif /* rtelib.h */
