/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2001 Michael H. Schimek
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

/* $Id: vcd.c,v 1.1 2001-01-24 22:48:52 mschimek Exp $ */

/*
 *  This code creates a stream suitable for mkvcdfs as vcdmplex
 *  does, both part of vcdtools by Rainer Johanni.
 *
 *  Disclaimer: I don't have the standard documents (ISO 11172-1,
 *  VCD "White Book") and, quote: "[vcdtools] where created mainly
 *  by reverse engineering the content of a VCD", so nobody can
 *  guarantee the files will work.
 *
 *  (IMHO there are better choices for archiving than VCD and mp1e
 *  at 1.152 or 2.3 Mbit/s, you will get what you asked for.)
 */

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
#include "stream.h"

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

	assert(mux_rate > 0);

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

#define SYSTEM_HEADER_SIZE 15

static inline unsigned char *
system_header(unsigned char *p, stream *str, double system_rate_bound)
{
	int mux_rate_bound = ((unsigned int) ceil(system_rate_bound + 49)) / 50;
	int audio_bound = 0;
	int video_bound = 0;

	assert(mux_rate_bound > 0);

	put(p + 12, str->stream_id, 1);

	if (str->stream_id == VIDEO_STREAM + 0) {
		put(p + 13, (0x3 << 14) + (1 << 13) + (47104 >> 10), 2);
		video_bound++;
	} else if (str->stream_id == AUDIO_STREAM + 0) {
		put(p + 13, (0x3 << 14) + (0 << 13) + (4096 >> 7), 2);
		audio_bound++;
	} else {
		FAIL("Stream id mismatch\n");
	}

	put(p, SYSTEM_HEADER_CODE, 4);
	put(p + 4, 9, 2);
	put(p + 6, (mux_rate_bound >> 15) | 0x80, 1);
	put(p + 7, mux_rate_bound >> 7, 1);
	put(p + 8, (mux_rate_bound << 1) | 1, 1);
	put(p + 9, (audio_bound << 2) + (0 << 1) + (0 << 0), 1);
	put(p + 10, (0 << 7) + (0 << 6) + (0x1 << 5) + (video_bound << 0), 1);
	put(p + 11, 0xFF, 1);
	/*
	 *  system_header_start_code [32], header_length [16],
	 *  marker_bit, rate_bound [22], marker_bit,
	 *  audio_bound [6], fixed_flag, CSPS_flag,
	 *  system_audio_lock_flag, system_video_lock_flag,
	 *  marker_bit, video_bound [5], reserved_byte [8]
	 */

	return p + 15;
}

static inline unsigned char *
padding_packet(unsigned char *p, int size)
{
	if (size < 8)
		return p;

	put(p, PACKET_START_CODE + PADDING_STREAM, 4);
	put(p + 4, size - 6, 2);
	put(p + 6, 0x0F, 1);

	memset(p + 7, 0xFF, size - 7);

	return p + size;
}

static inline bool
next_access_unit(stream *str, double *ppts, unsigned char *ph)
{
	buffer *buf;

	str->buf = buf = wait_full_buffer(&str->fifo);

	str->ptr  = buf->data;
	str->left = buf->used;

	if (!str->left)
		return FALSE;

	if (ph) {
		/*
		 *  Add DTS/PTS of first access unit
		 *  commencing in packet.
		 */
		if (str->stream_id == VIDEO_STREAM + 0) {
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
				put(ph + 6, (0x1 << 14) + (1 << 13) + (47104 >> 10), 2);
				time_stamp(ph +  8, MARKER_PTS, *ppts);
				time_stamp(ph + 13, MARKER_DTS, str->dts);
				break;

			case B_TYPE:
				*ppts = str->dts; // delay always 0
				time_stamp(ph +  8, MARKER_PTS, *ppts);
				time_stamp(ph + 13, MARKER_DTS, str->dts);
				break;

			default:
				/* no time stamp */
			}
		} else {
			*ppts = str->dts + str->pts_offset;
			put(ph + 6, (0x1 << 14) + (0 << 13) + (4096 >> 7), 2);
			time_stamp(ph + 8, MARKER_PTS_ONLY, *ppts);
		}
	}

	str->ticks_per_byte = str->ticks_per_frame / str->left;

	return TRUE;
}

#define LARGE_DTS 1E30

static inline stream *
schedule(void)
{
	double dtsi_min = LARGE_DTS;
	stream *s, *str;

	s = (stream *) mux_input_streams.head;
	str = NULL;

	while (s) {
		double dtsi = s->dts;

		if (s->buf)
			dtsi += (s->ptr - s->buf->data) * s->ticks_per_byte;

		if (dtsi < dtsi_min) {
			str = s;
			dtsi_min = dtsi;
		}

		s = (stream *) s->node.next;
	}

	return str;
}

void *
vcd_system_mux(void *unused)
{
	unsigned char *p, *ph, *ps, *pl, *px;
	unsigned long bytes_out = 0;
	unsigned int packet_count;
	unsigned int packet_size;
	double system_rate, system_rate_bound;
	double system_overhead;
	double ticks_per_pack, tscr;
	double scr, pts, front_pts = 0.0;
	stream *str, *video_stream = NULL;
	stream *audio_stream = NULL;
	bool do_pad = TRUE;
	buffer *buf;

	{
		double preload_delay;
		double video_frame_rate = DBL_MAX;
		int nstreams = 0;
		int preload = 0;
		int bit_rate = 0;

		for (str = (stream *) mux_input_streams.head; str; str = (stream *) str->node.next) {
			str->buf = NULL;
			bit_rate += str->bit_rate;
			str->eff_bit_rate = str->bit_rate;
			str->ticks_per_frame = (double) SYSTEM_TICKS / str->frame_rate;

			if (IS_VIDEO_STREAM(str->stream_id) && str->frame_rate < video_frame_rate) {
				video_stream = str;
				video_frame_rate = frame_rate;
				if (abs(str->bit_rate - 1152000) > 20000)
					do_pad = FALSE;
			} else {
				audio_stream = str;
				if (abs(str->bit_rate - 224000) > 5000)
					do_pad = FALSE;
			}

			buf = wait_full_buffer(&str->fifo);

			if (!buf->used)
				FAIL("Premature end of file");

	    		preload += buf->used;

			unget_full_buffer(&str->fifo, buf);

			nstreams++;
		}

		if (!video_stream || !audio_stream || nstreams != 2)
			FAIL("One video, one audio stream required");

		buf = mux_output(NULL);

		assert(buf
		       && buf->size >= 512
		       && buf->size <= 32768);

		packet_size = buf->size;

		system_overhead = packet_size / (packet_size - SYSTEM_HEADER_SIZE - PACK_HEADER_SIZE);
		system_rate = system_rate_bound = bit_rate * system_overhead / 8;

		/* Frames must arrive completely before decoding starts */
		preload_delay = (preload * system_overhead + PACK_HEADER_SIZE)
			/ system_rate_bound * SYSTEM_TICKS;

		scr = 36000;
		ticks_per_pack = packet_size / system_rate_bound * SYSTEM_TICKS;

		for (str = (stream *) mux_input_streams.head; str; str = (stream *) str->node.next) {
			/* Video PTS is delayed by one frame */
			if (!IS_VIDEO_STREAM(str->stream_id)) {
				str->pts_offset = (double) SYSTEM_TICKS / (video_frame_rate * 1.0);
				/* + 0.1 to schedule video frames first */
				str->dts = preload_delay + 0.1;
			} else
				str->dts = preload_delay;
		}
	}

	/* Appears suspicious to me, but this is what vcdmplex does. */

	p = buf->data;
	p = pack_header(p, scr, system_rate);
	p = system_header(p, audio_stream, system_rate_bound);

	if (do_pad)
		p = padding_packet(p, buf->data + buf->size - p);

	bytes_out += buf->used = p - buf->data;
	buf = mux_output(buf);

	scr += ticks_per_pack;

	p = buf->data;
	p = pack_header(p, scr, system_rate);
	p = system_header(p, video_stream, system_rate_bound);

	if (do_pad)
		p = padding_packet(p, buf->data + buf->size - p);

	bytes_out += buf->used = p - buf->data;
	buf = mux_output(buf);

	tscr = scr;

	packet_count = 2;

	/* Packet loop */

	for (;;) {
		int n, m;

		p = buf->data;
		px = p + buf->size;
		p = pack_header(p, scr, system_rate);

		scr += ticks_per_pack;

		/* XXX target system rate 75 * 2324 B/s */
reschedule:
		if (!(str = schedule()))
			break;

		pts = -1;

		ph = p;
		ps = p;

		n = px - p - 7;
		m = (str->stream_id == VIDEO_STREAM + 0) ? 11 : 6;

		put(p, PACKET_START_CODE + str->stream_id, 4);
		put(p + 4, 0xFFFFFFFF, 4);
		put(p + 8, 0xFFFFFFFF, 4);
		put(p + 12, 0xFFFFFFFF, 4);
		put(p + 16, 0xFFFF, 2);

		if (str->left >= (n - m)) {
			m = MIN(str->left, n);
			p += n - m + 6;

			put(p, 0x0F, 1);
			memcpy(p + 1, str->ptr, m);

			p = px;
			str->ptr += m;
			str->left -= m;
		} else {
			put(p + m + 6, 0x0F, 1);
			pl = p += m + 7;

			while (p < px) {
				if (str->left <= 0) {
					if (!next_access_unit(str, &pts, ph)) {
						str->dts = LARGE_DTS * 2.0; // don't schedule stream

						if (pl == p) {
							/* no payload */
							p = ps;
							goto reschedule;
						}

						if (do_pad) {
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
					send_empty_buffer(&str->fifo, str->buf);

					str->buf = NULL;
					str->dts += str->ticks_per_frame;
				}

				p += n;

				str->ptr += n;
			}
		}

		printv(4, "Packet #%d %s, pts=%f\n",
			packet_count, mpeg_header_name(str->stream_id), pts);

		((unsigned short *) ps)[2] = swab16(p - ps - 6);

		bytes_out += buf->used = p - buf->data;

		buf = mux_output(buf);

		assert(buf
			&& buf->size >= 512
			&& buf->size <= 32768);

		packet_count++;

		if (pts > front_pts)
			front_pts = pts;

		if (verbose > 0 && (packet_count & 3) == 0) {
			double system_load = 1.0 - get_idle();
			int min, sec;

			sec = front_pts / SYSTEM_TICKS;
			min = sec / 60;
			sec -= min * 60;

			printv(1, "%d:%02d (%.1f MB), system load %4.1f %%",
				min, sec, bytes_out / (double)(1 << 20), 100.0 * system_load);

			if (video_frames_dropped > 0)
				printv(1, ", %5.2f %% dropped",
					100.0 * video_frames_dropped / video_frame_count);

			printv(1, (verbose > 3) ? "\n" : "  \r");

			fflush(stderr);
		}
	}

	p = buf->data;

	*((unsigned int *) p) = swab32(ISO_END_CODE);
	buf->used = 4;

	mux_output(buf);

	return NULL;
}
