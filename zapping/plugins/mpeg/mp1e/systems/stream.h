/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
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

/* $Id: stream.h,v 1.3 2000-09-30 19:38:45 mschimek Exp $ */

#include "../common/log.h"
#include "../common/types.h"
#include "../common/fifo.h"

#define PACKET_SIZE		2048
#define PACKETS_PER_PACK	16
#define PAD_PACKETS		FALSE
#define CONST_BIT_RATE		FALSE
#define PAYLOAD_ALIGNMENT	1

extern buffer *			(* mux_output)(buffer *b);

typedef struct {
	node			node;
	fifo			fifo;

	int			stream_id;

	int			bit_rate;
	double			frame_rate;

	buffer *		buf;
	unsigned char *		ptr;
	int			left;

	double			dts;
	double			pts_offset;

	double			eff_bit_rate;

	double			ticks_per_frame;
	double			ticks_per_byte;

	double			cap_t0;
	int			frame_count;

	fifo *			cap_fifo;
} stream;
