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

#ifndef __RTEPRIV_H__
#define __RTEPRIV_H__
#include "rte.h"

/* fixme: this symbol will make namespace collisions, change it */
extern rte_context * rte_global_context;

/*
  Private things we don't want people to see, we can play with this
  without breaking any compatibility.
  Eventually all the global data will move here, except for the
  tables.
*/
struct _rte_context_private {
	int encoding; /* 1 if working, 0 if not */
	rteEncodeCallback encode_callback; /* save-data Callback */
	rteDataCallback data_callback; /* need-data Callback */
	int fd; /* file descriptor of the file we are saving */
	void * user_data; /* user data given to the callback */
	_fifo aud, vid; /* fifos for pushing */
	int v_ubuffer; /* for unget() */
	_buffer * a_ubuffer; /* for unget() */
	int a_again; /* if 1, return a_ubuffer again */
	int depth; /* video bit depth (bytes per pixel, includes
		      packing) */
	_buffer * last_video_buffer; /* video buffer the app should be
				       encoding to */
	_buffer * last_audio_buffer; /* audio buffer */
	/* video fetcher (callbacks) */
	int video_pending; /* Pending video frames */
	pthread_t video_fetcher_id; /* id of the video fetcher thread */
	pthread_mutex_t video_mutex; /* mutex for the fetcher */
	pthread_cond_t video_cond; /* cond for the fetcher */
	/* audio fetcher (callbacks) */
	int audio_pending; /* Pending video frames */
	pthread_t audio_fetcher_id; /* id of the video fetcher thread */
	pthread_mutex_t audio_mutex; /* mutex for the fetcher */
	pthread_cond_t audio_cond; /* cond for the fetcher */
};

#endif /* rtepriv.h */
