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
#include "../common/fifo.h"
#include "../common/log.h"
#include "../rtepriv.h"
#include "systems.h"
#include "stream.h"

buffer *		(* mux_output)(buffer *b);

static buffer		mux_buffer;

static buffer *
output_stdout(buffer *b)
{
	unsigned char *s;
	ssize_t r, n;

	if (!b)
		return &mux_buffer;

	s = b->data;
	n = b->used;

	while (n > 0) {
		r = write(STDOUT_FILENO, s, n);

		if (r < 0 && errno == EINTR)
			continue;

		ASSERT("write", r >= 0);

		s += r;
		n -= r;
	}

	return b;
}

bool
init_output_stdout(void)
{
	ASSERT("allocate mux buffer, %d bytes",
		init_buffer(&mux_buffer, PACKET_SIZE), PACKET_SIZE);
	/*
	 *  Attn: mux_buffer.size determines the packet size, not PACKET_SIZE.
	 *  All buffers shall have the same size, returning used <= size.
	 */

	mux_output = output_stdout;

	return TRUE;
}

#if 0

static buffer *
output_buffered(buffer *b)
{
	if (b)
		send_full_buffer(output_fifo, b);

	return wait_empty_buffer(output_fifo);
}

#endif

static _fifo            out;
static int              out_buffers = 8;
static size_t           out_buffer_size;

extern int		stereo;
extern pthread_t        output_thread_id;

static buffer *
output(buffer *mbuf)
{
	_buffer *obuf;

	if (!mbuf)
		return &mux_buffer;

	obuf = _new_buffer(&out);

	assert(out_buffer_size >= mbuf->used);

	memcpy(obuf->data, mbuf->data, mbuf->used);
	obuf->size = mbuf->used;

	_send_out_buffer(&out, obuf);

	return mbuf; /* any previously entered */
}

/*
  This thread is started from main()
*/
void *
output_thread (void * unused)
{
	_buffer * buf;

	for (;;) {
		pthread_mutex_lock(&out.mutex);

		while (!(buf = (_buffer *) rem_head(&out.full)))
			pthread_cond_wait(&out.cond, &out.mutex);

		pthread_mutex_unlock(&out.mutex);

		ASSERT("global context is set", rte_global_context != NULL);

		rte_global_context->private->encode_callback(buf->data, buf->size,
			   rte_global_context, rte_global_context->private->user_data);

		_empty_buffer(&out, buf);
	}

	return NULL;
}

int
output_init( void )
{
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

	if (!init_buffer(&mux_buffer, PACKET_SIZE))
		return 0;

	out_buffers = _init_fifo(&out, "output", out_buffer_size,
				out_buffers);

	if (out_buffers < 5) {
		uninit_buffer(&mux_buffer);
		_free_fifo(&out);
		return 0;
	}

	mux_output = output;

	return 1;
}

void
output_end ( void )
{
	_buffer * buf;
	
	pthread_cancel(output_thread_id);
	pthread_join(output_thread_id, NULL);

	while ((buf = (_buffer *) rem_head(&out.full)))
	{
		if (!buf->size)
			break;
		rte_global_context->private->encode_callback(buf->data, buf->size,
			   rte_global_context, rte_global_context->private->user_data);
		_empty_buffer(&out, buf);
	}

	uninit_buffer(&mux_buffer);
	_free_fifo(&out);
}
