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

/* FIXME: We need declaring things as const */
/* FIXME: This is only a draft, i don't like the input code */

#ifndef __RTELIB_H__
#define __RTELIB_H__

/*
  What are we going to encode, audio only, video only or both
*/
enum rte_mux_mode {
	RTE_MUX_VIDEO_ONLY = 1,
	RTE_MUX_AUDIO_ONLY,
	RTE_MUX_VIDEO_AND_AUDIO
};

/*
  Supported pixformats. This is the pixformat the pushed video frames
  will be pushed in, the output (encoded) format is the one specified
  by the MPEG standard (YCbCr)
*/
enum rte_pixformat {
	RTE_YCbCr /* The obvious one :-) */
/*	RTE_RGB555,
	RTE_RGB565,
	RTE_BGR24,
	RTE_RGB24,
	RTE_BGR32,
	RTE_RGB32 */
	/* more formats to come ... */
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
	RTE_AUDIO_MODE_STEREO,
	RTE_AUDIO_MODE_DUAL_CHANNEL
};

typedef struct _rte_context_private rte_context_private;

typedef struct {
	/* Filename used when creating this context, NULL if unknown */
	char * filename;
	/* Whether to encode audio only, video only or both */
	enum rte_mux_mode mode;

	/******** video parameters **********/
	/* pixformat the passed video data is in, RTE_YCbCr by default */
	enum rte_pixformat video_format;
	/* frame size */
	int width, height;

	/* Video frame rate */
	enum rte_frame_rate video_rate;
	/* output video bits per second, defaults to 2000000 */
	size_t output_video_bits;

	/* size in bytes of a complete frame */
	int video_bytes;
	
	/******* audio parameters **********/
	/* audio sampling rate in kHz, 44100 by default */
	int audio_rate; 
	/* audio bits per sample, defaults to 16 */
	int bits;
	/* Audio mode, defaults to Mono */
	enum rte_audio_mode audio_mode;
	/* output audio bits per second, defaults to 80000 */
	size_t output_audio_bits;
	/* size in bytes of an audio frame */
	int audio_bytes;

	/* Number of encoded bytes written */
	size_t bytes_written;

	/* Pointer to the private data of this struct */
	rte_context_private * private;
} rte_context;

/*
  "You have to save this data" callback. Defaults to a disk (stdout)
  write().
  Return 0 if encoding should be stopped, anything else to continue
  working.
  data: Pointer to the encoded data.
  size: Size in bytes of the data stored in data
  context: Context that created this data.
  user_data: Pointer passed to rte_context_new
*/
typedef int (*rteEncodeCallback)(void * data,
				 size_t size,
				 rte_context * context,
				 void * user_data);

#define RTE_ENCODE_CALLBACK(function) ((rteEncodeCallback)function)

/*
  "I need more data" callback. The input thread will call this
  callback whenever it thinks it will need some fresh
  data to encode (usually it will go one or two samples ahead of the
  encoder thread). The callbacks and the push() interfaces can be used
  together.
  data: Where should you write the data. It's a memchunk of
  context->video_bytes or context->audio_bytes (depending whether rte
  asks for video or audio), and you should fill it (i.e., a video
  frame of audio_bytes of audio).
  time: Push here the timestamp for your frame.
  video: 1 if rte requests video, 0 if audio.
  context: The context that asks for the data.
  user_data: User data passed to rte_context_new
*/
typedef int (*rteDataCallback)(void * data,
			       double * time,
			       int video,
			       rte_context * context,
			       void * user_data);

#define RTE_DATA_CALLBACK(function) ((rteDataCallback)function)

/* Interface functions */
/*
  Inits the lib.
  Returns 1 if the lib can be used in this box, and 0 if not.
*/
int rte_init ( void );

/*
  Creates a rte encoding context. The compulsory parameters are here,
  if the rest aren't specified before pushing data, then the defaults
  are used.
  Returns: The new context on startup, NULL on error.
  file: If you don't specify a encode_callback, and file is not NULL, this
        file will be opened (created) and data will be stored here. If
	it is NULL, and the encode callback too, NULL will be returned.
  width, height: Width and height of the pushed frames, must be 16-multiplus
  rate: Video frame rate
  encode_callback: Function to be called when encoded data is ready.
  data_callback: Function to be called when data to be encoded is needed.
  user_data: Some data you would like to pass to the callback
*/
rte_context * rte_context_new (char * file,
			       int width, int height,
			       enum rte_frame_rate rate,
			       rteEncodeCallback encode_callback,
			       rteDataCallback data_callback,
			       void * user_data);

/*
  Destroys the encoding context and (if encoding) stops encoding. It
  frees the memory allocated by the rte_context.
  Returns: Always NULL
*/
void * rte_context_destroy ( rte_context * context );

/*
  Sets the video parameters.
*/
void rte_set_video_parameters (rte_context * context,
			       enum rte_pixformat frame_format,
			       int width, int height,
			       enum rte_frame_rate video_rate,
			       size_t output_video_bits);
/*
  Setters and getters for the members in the struct, so no direct
  access is needed.
  FIXME: This needs to be added, remeber to add setter and getter for
  the callbacks and the filename
*/

/*
  Sets up everything to start coding. Call this after setting the
  desired parameters, after calling this they will be read-only. If we
  are currently encoding, the function will fail. (librte isn't
  reentrant yet)
  Returns: 1 on success, 0 on error.
  FIXME: Some error reporting is needed here.
*/
int rte_start ( rte_context * context );

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
  double time = get_data_from_video_source(ptr);
  ptr = rte_push_video_data(context, ptr, time);
  } while (data_available);
*/
void * rte_push_video_data ( rte_context * context, void * data,
			     double time );

/*
  Pushes an audio sample into the given encoding context. The usage is
  similar to push_video_data.
  When you push one sample, it is assumed that it contains
  context->audio_bytes of audio data with the current context audio
  parameters.
  data: pointer to the data to encode.
  time: Timestamp given to the frame, in seconds.
  Returns: A pointer to the buffer where you should write the next sample
*/
void * rte_push_audio_data ( rte_context * context, void * data,
			     double time );

/*
  Returns: a pointer to the last error. The returned string is
  statically allocated (and it can be NULL), you don't need to free it.
*/
char * rte_last_error ( rte_context * context );

#endif /* rtelib.h */
