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
#include "systems.h"

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
	n = b->used; // XXX eof 

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
	int bsize = (mux_syn == 4) ? 2324 /* VCD */ : PACKET_SIZE;

	ASSERT("allocate mux buffer, %d bytes",
		init_buffer(&mux_buffer, bsize), bsize);
	/*
	 *  Attn: mux_buffer.size determines the packet size, not PACKET_SIZE.
	 *  All buffers shall have the same size,
	 *  will send full buffers used <= size.
	 */

	mux_buffer.data = mux_buffer.allocated; /* XXX */

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
