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
#include "stream.h"
#include "../rtepriv.h"

buffer *		(* mux_output)(buffer *b);

static buffer		mux_buffer;

static buffer *
output(buffer *mbuf)
{
	if (!mbuf)
		return &mux_buffer;

	/* rte_global_context sanity checks */
	if ((!rte_global_context) || (!rte_global_context->private) ||
	    (!rte_global_context->private->encode_callback)) {
		rte_error(NULL, "sanity check failed");
		return mbuf;
	}

	rte_global_context->private->
		encode_callback(rte_global_context,
				mbuf->data,
				mbuf->used,
				rte_global_context->private->user_data);


	return mbuf; /* any previously entered */
}

int
output_init( void )
{
	if (!init_buffer(&mux_buffer, PACKET_SIZE))
		return FALSE;

	mux_output = output;

	return TRUE;
}

void
output_end ( void )
{
	uninit_buffer(&mux_buffer);
}
