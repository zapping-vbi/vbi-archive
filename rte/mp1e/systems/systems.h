/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2000 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

/* $Id: systems.h,v 1.10 2002-05-07 06:39:37 mschimek Exp $ */

#include "libsystems.h"

#include "../common/log.h"
#include "../common/types.h"
#include "../common/fifo.h"

#define PACKET_SIZE		2048
#define PACKETS_PER_PACK	16
#define PAD_PACKETS		FALSE
#define CONST_BIT_RATE		FALSE
#define PAYLOAD_ALIGNMENT	1

typedef struct stream stream;

typedef int64_t			tstamp; /* 90000/s */

#define TSTAMP_MIN ((tstamp)(((uint64_t) 1) << 63))
#define TSTAMP_MAX ((tstamp)((((uint64_t) 1) << 63) - 1))

struct au {
	tstamp			dts;
	int			size;
};

struct stream {
	fifo			fifo;
	consumer		cons;

	multiplexer *		mux;

	int			stream_id;

	int			bit_rate;
	double			frame_rate;

	buffer *		buf;
	unsigned char *		ptr;
	int			left;

	double			dts_old;
	double			dts_end;
	double			pts_offset;

	double			eff_bit_rate;

	double			ticks_per_frame;
	double			ticks_per_byte;

	double			cap_t0;
	long long		frame_count;



	tstamp			dts;

	int			inbuf_free;
	int			packet_payload;

	struct au *		au_r;
	struct au *		au_w;
	struct au		au_ring[64];

	int killme;
};

#define elements(array) (sizeof(array) / sizeof(array[0]))






