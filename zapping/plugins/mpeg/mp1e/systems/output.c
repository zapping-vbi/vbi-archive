/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
 *
 *  Modified by Iñaki G.E.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include "../video/mpeg.h"
#include "../video/video.h"
#include "../audio/mpeg.h"
#include "../options.h"
#include "../fifo.h"
#include "../log.h"
#include "systems.h"

static int              output_file = -1;
extern pthread_t        output_thread_id;

static fifo             out;
static int              out_buffers = 8;
static size_t           out_buffer_size;

extern int		stereo;

int
output_init( const char * file )
{
	if (output_file >= 0)
		close(output_file);
	
	output_file = creat(file, 00755);

	/* we need this later for the size check */
	switch (mux_mode & 3) {
	case 1:
		out_buffer_size = mb_num * 384 * 4;
		break;
	case 2:
		out_buffer_size = 2048 << stereo;
		break;
	case 3:
		out_buffer_size = PACKET_SIZE;
		break;
	}

	out_buffers = init_fifo(&out, "output", out_buffer_size,
				out_buffers);

	return (output_file);
}


static inline void
do_real_output(char * s, size_t n)
{
	size_t r;

	while (n > 0) {
		r = write(output_file, s, n);
		
		if (r < 0 && errno == EINTR)
			continue;
		
		ASSERT("write", r >= 0);
		
		(char *) s += r;
		n -= r;
	}
}

void
output_end ( void )
{
	buffer * buf;
	
	pthread_cancel(output_thread_id);
	pthread_join(output_thread_id, NULL);

	while ((buf = (buffer *) rem_head(&out.full)))
	{
		do_real_output(buf->data, buf->size);
		empty_buffer(&out, buf);
	}

	if (output_file)
		close(output_file);

}

/*
  If the output callback is NULL, we should call do_real_output
  directly from here, and avoid the fifo thing.
*/
buffer *
output(buffer *mbuf)
{
	buffer *obuf;

	bytes_out += mbuf->size;

	obuf = new_buffer(&out);

	assert(out_buffer_size >= mbuf->size);

	memcpy(obuf->data, mbuf->data, mbuf->size);
	obuf->size = mbuf->size;

	send_out_buffer(&out, obuf);

	return mbuf; /* any previously entered */
}

/*
  This thread is started from main()
*/
void *
output_thread (void * unused)
{
	buffer * buf;

	for (;;) {
		pthread_mutex_lock(&out.mutex);

		while (!(buf = (buffer *) rem_head(&out.full)))
			pthread_cond_wait(&out.cond, &out.mutex);

		pthread_mutex_unlock(&out.mutex);

		do_real_output(buf->data, buf->size);

		empty_buffer(&out, buf);
	}

	return NULL;
}
