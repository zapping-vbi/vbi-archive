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
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#include "../video/mpeg.h"
#include "../video/video.h"
#include "../audio/mpeg.h"
#include "../options.h"
#include "../common/fifo.h"
#include "../common/log.h"
#include "systems.h"

static buffer		mux_buffer;

static buffer *
output_stdout(struct multiplexer *mux,
	      buffer *b)
{
	static long long count = 0;
	static int part = 1;
	extern char *outFileName;
	unsigned char *s;
	ssize_t r, n;

	if (!b)
		return &mux_buffer;

	if (!b->used) /* EOF */
		return b;

	if (cut_output) {
		if ((count + b->used) >= (1LL << 30)) {
			char buf[256];

			close (outFileFD);

			snprintf (buf, 255, "part-%03d.mpg", ++part);

#ifdef HAVE_LARGEFILE64
			outFileFD = open64 (buf, O_CREAT | O_WRONLY |
					    O_TRUNC | O_LARGEFILE,
					    S_IRUSR | S_IWUSR |
					    S_IRGRP | S_IWGRP |
					    S_IROTH | S_IWOTH);
#else
			outFileFD = open (buf, O_CREAT | O_WRONLY | O_TRUNC,
					  S_IRUSR | S_IWUSR |
					  S_IRGRP | S_IWGRP |
					  S_IROTH | S_IWOTH);
#endif
			count = 0;
		}

		count += b->used;
	}

	s = b->data;
	n = b->used;

	while (n > 0) {
		r = write(outFileFD, s, n);

		if (r < 0 && errno == EINTR)
			continue;

		ASSERT("write", r >= 0);

		s += r;
		n -= r;
	}

	return b;
}

rte_bool
init_output_stdout(multiplexer *mux)
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

	mux->mux_output = output_stdout;

	return TRUE;
}

#if 0

static buffer *
output_buffered(struct multiplexer *mux, buffer *b)
{
	if (b)
		send_full_buffer(output_fifo, b);

	return wait_empty_buffer(output_fifo);
}

#endif
