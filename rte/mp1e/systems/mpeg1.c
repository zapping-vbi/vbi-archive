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

/* $Id: mpeg1.c,v 1.3 2001-08-19 10:58:35 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include "../video/mpeg.h"
#include "../video/video.h"
#include "../audio/libaudio.h"
#include "../common/log.h"
#include "../common/fifo.h"
#include "../common/math.h"
#include "../options.h"
#include "mpeg.h"
#include "systems.h"

#define put(p, val, bytes)						\
do {									\
	unsigned int v = val;						\
									\
	switch (bytes) {						\
	case 4:								\
		*((unsigned int *)(p)) = swab32(v);			\
		break;							\
	case 3:	(p)[bytes - 3] = v >> 16;				\
	case 2:	(p)[bytes - 2] = v >> 8;				\
	case 1:	(p)[bytes - 1] = v;					\
	}								\
} while (0)

static inline unsigned char *
time_stamp(unsigned char *p, int marker, double system_time)
{
	long long ts = llroundn(system_time);

	p[0] = (marker << 4) + ((ts >> 29) & 0xE) + 1;
	p[1] = ((long) ts >> 22);
	p[2] = ((long) ts >> 14) | 1;
	p[3] = ((long) ts >> 7);
	p[4] = (long) ts * 2 + 1;
	/*
	 *  marker [4], TS [32..30], marker_bit,
	 *  TS [29..15], marker_bit,
	 *  TS [14..0], marker_bit
	 */

	return p + 5;
}

#define PACK_HEADER_SIZE 12
#define SCR_offset 8

static inline unsigned char *
pack_header(unsigned char *p, double scr, double system_rate)
{
	int mux_rate = ((unsigned int) ceil(system_rate + 49)) / 50;

	// assert(mux_rate > 0);

	put(p, PACK_START_CODE, 4);
	time_stamp(p + 4, MARKER_SCR, scr);
	put(p + 9, (mux_rate >> 15) | 0x80, 1);
	put(p + 10, mux_rate >> 7, 1);
	put(p + 11, (mux_rate << 1) | 1, 1);
	/*
	 *  pack_start_code [32], system_clock_reference [40],
	 *  marker_bit, program_mux_rate, marker_bit
	 */

	return p + 12;
}

#define SYSTEM_HEADER_SIZE(nstr) (12 + (nstr) * 3)

static inline unsigned char *
system_header(multiplexer *mux, unsigned char *p, double system_rate_bound)
{
	int mux_rate_bound = ((unsigned int) ceil(system_rate_bound + 49)) / 50;
	int audio_bound = 0;
	int video_bound = 0;
	unsigned char *ph;
	stream *str;

	// assert(mux_rate_bound > 0);

	ph = p;
	p += 12;

	for_all_nodes (str, &mux->streams, fifo.node) {
		put(p, str->stream_id, 1);

		if (IS_VIDEO_STREAM(str->stream_id)) {
			put(p + 1, (0x3 << 14) + (1 << 13) + ((40960 + 1023) >> 10), 2);
			video_bound++;
		} else if (IS_AUDIO_STREAM(str->stream_id)) {
			put(p + 1, (0x3 << 14) + (0 << 13) + ((4096 + 127) >> 7), 2);
			audio_bound++;
		} else if (str->stream_id == PRIVATE_STREAM_1) {
			put(p + 1, (0x3 << 14) + (0 << 13) + ((1504 + 127) >> 7), 2);
		} else if (0 < (1 << (13 + 7))) {
			put(p + 1, (0x3 << 14) + (0 << 13) + ((0 + 127) >> 7), 2);
		} else
			put(p + 1, (0x3 << 14) + (1 << 13) + ((0 + 1023) >> 10), 2);
			/* '11', buffer_bound_scale, buffer_size_bound [13] */

		p += 3;
	}

	put(ph, SYSTEM_HEADER_CODE, 4);
	put(ph + 4, p - ph - 6, 2);
	put(ph + 6, (mux_rate_bound >> 15) | 0x80, 1);
	put(ph + 7, mux_rate_bound >> 7, 1);
	put(ph + 8, (mux_rate_bound << 1) | 1, 1);
	put(ph + 9, (audio_bound << 2) + (0 << 1) + (0 << 0), 1);
	put(ph + 10, (0 << 7) + (0 << 6) + (0x1 << 5) + (video_bound << 0), 1);
	put(ph + 11, 0xFF, 1);
	/*
	 *  system_header_start_code [32], header_length [16],
	 *  marker_bit, rate_bound [22], marker_bit,
	 *  audio_bound [6], fixed_flag, CSPS_flag,
	 *  system_audio_lock_flag, system_video_lock_flag,
	 *  marker_bit, video_bound [5], reserved_byte [8]
	 */

	return p;
}

#define PACKET_HEADER_SIZE 16

static inline unsigned char *
packet_header(unsigned char *p, stream *str)
{
	put(p, PACKET_START_CODE + str->stream_id, 4);
	put(p + 4, 0xFFFFFFFF, 4);
	put(p + 8, 0xFFFFFFFF, 4);
	put(p + 12, 0xFFFFFF0F, 4);

	if (str->stream_id != PRIVATE_STREAM_1)
		return p + PACKET_HEADER_SIZE;
	else {
		/* TS alignment not applicable (how?) */
		put(p + 16, 0x10, 1); // data_identifier: EBU data

		return p + PACKET_HEADER_SIZE + 1;
	}
}

#define CDLOG 0

#if CDLOG
static FILE *cdlog;
#endif

#define Rvid (1.0 / 1024)
#define Raud (0.0)

static inline bool
next_access_unit(stream *str, double *ppts, unsigned char *ph)
{
	buffer *buf;

	str->buf = buf = wait_full_buffer(&str->cons);

	str->ptr  = buf->data;
	str->left = buf->used;

	if (buf->used <= 0) {
		str->left = 0; /* XXX */
		return FALSE;
	}

	if (!IS_AUDIO_STREAM(str->stream_id))
		str->eff_bit_rate +=
			((buf->used * 8 * str->frame_rate)
			 - str->eff_bit_rate) * Rvid;

#if CDLOG
	if (IS_VIDEO_STREAM(str->stream_id)) {
		double stime = str->cap_t0 +
			(str->frame_count + buf->offset - 1) / str->frame_rate;

		fprintf(cdlog, "%02x: I%15.10f S%15.10f %+15.10f\n",
			str->stream_id, buf->time, stime, buf->time - stime);

		str->frame_count++;
	} else if (IS_AUDIO_STREAM(str->stream_id)) {
		double stime = str->cap_t0 + str->frame_count / str->frame_rate;

		fprintf(cdlog, "%02x: I%15.10f S%15.10f %+15.10f\n",
			str->stream_id, buf->time, stime, buf->time - stime);

		str->frame_count++;
	}
#endif

	if (ph) {
		/*
		 *  Add DTS/PTS of first access unit
		 *  commencing in packet.
		 */
		if (IS_VIDEO_STREAM(str->stream_id)) {
			/*
			 *  d/o  IBBPBBIBBPBBP
			 *  c/o  IPBBIBBPBBPBB
			 *  DTS  0123456789ABC
			 *  PTS  1423756A89DBC
			 */
			switch (buf->type) {
			case I_TYPE:
			case P_TYPE:
				*ppts = str->dts + str->ticks_per_frame * buf->offset; // reorder delay
				time_stamp(ph +  6, MARKER_PTS, *ppts);
				time_stamp(ph + 11, MARKER_DTS, str->dts);
				break;
			
			case B_TYPE:
				*ppts = str->dts; // delay always 0
				time_stamp(ph +  6, MARKER_PTS, *ppts);
				time_stamp(ph + 11, MARKER_DTS, str->dts);
				break;

			default:
				; /* no time stamp */
			}
		} else {
			*ppts = str->dts + str->pts_offset;
			time_stamp(ph + 11, MARKER_PTS_ONLY, *ppts);
		}
	}


	str->ticks_per_byte = str->ticks_per_frame / str->left;

	return TRUE;
}

#define LARGE_DTS 1E30

static inline stream *
schedule(multiplexer *mux)
{
	double dtsi_min = LARGE_DTS;
	stream *s, *str;

	str = NULL;

	for_all_nodes (s, &mux->streams, fifo.node) {
		double dtsi = s->dts;

		if (s->buf)
			dtsi += (s->ptr - s->buf->data) * s->ticks_per_byte;

		if (dtsi < dtsi_min) {
			str = s;
			dtsi_min = dtsi;
		}
	}

	return str;
}

void *
mpeg1_system_mux(void *muxp)
{
	multiplexer *mux = muxp;
	unsigned char *p, *ph, *ps, *pl, *px;
	unsigned long bytes_out = 0;
	unsigned int pack_packet_count = PACKETS_PER_PACK;
	unsigned int packet_count = 0;
	unsigned int pack_count = 0;
	double system_rate, system_rate_bound;
	double system_overhead;
	double ticks_per_pack;
	double scr, pts, front_pts = 0.0;
	buffer *buf;
	stream *str;

	pthread_cleanup_push((void (*)(void *)) pthread_rwlock_unlock, (void *) &mux->streams.rwlock);
	assert(pthread_rwlock_rdlock(&mux->streams.rwlock) == 0);

#if CDLOG
	if ((cdlog = fopen("cdlog", "w"))) {
		fprintf(cdlog, "Clock drift\n\n");
	}
#endif

	{
		double preload_delay;
		double video_frame_rate = DBL_MAX;
		int nstreams = 0;
		int preload = 0;
		int bit_rate = 0;

		for_all_nodes (str, &mux->streams, fifo.node) {
			str->buf = NULL;
			bit_rate += str->bit_rate;
			str->eff_bit_rate = str->bit_rate;
			str->ticks_per_frame = (double) SYSTEM_TICKS / str->frame_rate;

			if (IS_VIDEO_STREAM(str->stream_id) && str->frame_rate < video_frame_rate)
				video_frame_rate = frame_rate;

			buf = wait_full_buffer(&str->cons);

			if (buf->used <= 0) // XXX
				FAIL("Premature end of file / error");

			str->cap_t0 = buf->time;
	    		preload += buf->used;

			unget_full_buffer(&str->cons, buf);

			nstreams++;
		}

		buf = mux_output(NULL);

		assert(buf && buf->size >= 512
		           && buf->size <= 32768);

		system_overhead = mux->packet_size / (mux->packet_size
			- (SYSTEM_HEADER_SIZE(nstreams) + PACK_HEADER_SIZE / PACKETS_PER_PACK));

		system_rate = system_rate_bound = bit_rate * system_overhead / 8;

		/* Frames must arrive completely before decoding starts */
		preload_delay = (preload * system_overhead + PACK_HEADER_SIZE)
			/ system_rate_bound * SYSTEM_TICKS;

		scr = SCR_offset / system_rate_bound * SYSTEM_TICKS;
		ticks_per_pack = (mux->packet_size * PACKETS_PER_PACK) / system_rate_bound * SYSTEM_TICKS;

		for_all_nodes (str, &mux->streams, fifo.node) {
			/* Video PTS is delayed by one frame */
			if (!IS_VIDEO_STREAM(str->stream_id)) {
				str->pts_offset = (double) SYSTEM_TICKS / (video_frame_rate * 1.0);
				/* + 0.1 to schedule video frames first */
				str->dts = preload_delay + 0.1;
			} else
				str->dts = preload_delay;
		}
	}

	/* Packet loop */

	for (;;) {
		p = buf->data;
		px = p + buf->size;

		/* Pack header, system header */

		if (pack_packet_count >= PACKETS_PER_PACK) {
			printv(4, "Pack #%d, scr=%f\n",	pack_count++, scr);

			p = pack_header(p, scr, system_rate);
			p = system_header(mux, p, system_rate_bound);

			if (0)
				scr += ticks_per_pack;
			else {
				int bit_rate = 0;

				for_all_nodes (str, &mux->streams, fifo.node)
					bit_rate += str->eff_bit_rate;

				system_rate = bit_rate * system_overhead / 8;
				scr += (mux->packet_size * PACKETS_PER_PACK) / system_rate * SYSTEM_TICKS;
			}

			pack_packet_count = 0;
		}

reschedule:
		if (!(str = schedule(mux)))
			break;

		ph = p;
		ps = p;

		pl = p = packet_header(p, str);

		if (str->stream_id == PRIVATE_STREAM_1)
			px -= (px - p) % 184;

		/* Packet fill loop */

		pts = -1;

		while (p < px) {
			int n;

			if (str->left == 0) {
				if (!next_access_unit(str, &pts, ph)) {
					str->dts = LARGE_DTS * 2.0; // don't schedule stream

					if (pl == p) {
						/* no payload */
						p = ps;
						goto reschedule;
					}

					if (PAD_PACKETS || str->stream_id == PRIVATE_STREAM_1) {
						memset(p, 0, px - p);
						p = px;
					}

					break;
				}

				ph = NULL;
			}

			n = MIN(str->left, px - p);

			memcpy(p, str->ptr, n);

			str->left -= n;

			if (!str->left) {
				send_empty_buffer(&str->cons, str->buf);

				str->buf = NULL;
				str->dts += str->ticks_per_frame;
			}

			p += n;

			str->ptr += n;
		}

		printv(4, "Packet #%d %s, pts=%f\n",
			packet_count, mpeg_header_name(str->stream_id), pts);

		((unsigned short *) ps)[2] = swab16(p - ps - 6);

		bytes_out += buf->used = p - buf->data;

		buf = mux_output(buf);

		assert(buf && buf->size >= 512
			   && buf->size <= 32768);

		packet_count++;
		pack_packet_count++;

		if (pts > front_pts)
			front_pts = pts;

		if (verbose > 0 && (packet_count & 3) == 0) {
			double system_load = 1.0 - get_idle();
			int min, sec;

			sec = front_pts / SYSTEM_TICKS;
			min = sec / 60;
			sec -= min * 60;

			printv(1, "%d:%02d (%.1f MB), system load %4.1f %%",
				min, sec, bytes_out / (double)(1 << 20),
				100.0 * system_load);

			if (video_frames_dropped > 0)
				printv(1, ", %5.2f %% dropped",
					100.0 * video_frames_dropped / video_frame_count);


#if 0 /* garetxe: num_buffers_queued doesn't exist any longer */
			printv(1, ", fifo v=%5.2f%% a=%5.2f%%",
			       100.0 * num_buffers_queued(video_fifo) / video_fifo->num_buffers,
			       100.0 * num_buffers_queued(audio_fifo) / audio_fifo->num_buffers);
#endif
			printv(1, (verbose > 3) ? "\n" : "  \r");

			fflush(stderr);
		}
	}

	p = buf->data;

	*((unsigned int *) p) = swab32(ISO_END_CODE);

	if (PAD_PACKETS) {
		memset(p + 4, 0, buf->size - 4);
		buf->used = buf->size;
	} else
		buf->used = 4;

	mux_output(buf);

	pthread_cleanup_pop(1);

	return NULL;
}
