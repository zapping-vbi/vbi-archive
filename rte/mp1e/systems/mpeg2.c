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

/* $Id: mpeg2.c,v 1.8 2002-02-08 15:03:11 mschimek Exp $ */

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
	long long ts = llroundn(system_time); // 90 kHz

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

static inline unsigned char *
time_stamp_ext(unsigned char *p, int marker, double system_time)
{
	static const int ext = 0;
	long long ts = llroundn(system_time); // 90 kHz

	p[0] = (marker << 6) + ((ts >> 27) & 0x38) + 4 + (((long) ts >> 28) & 3);
	p[1] = ((long) ts >> 20);
	p[2] = (((long) ts >> 12) & 0xF8) + 4 + (((long) ts >> 13) & 3);
	p[3] = ((long) ts >> 5);
	p[4] = ((long) ts << 3) + 4 + (ext >> 7);
	p[5] = ext * 2 + 1;
	/*
	 *  marker [2], TS [32..30], marker_bit,
	 *  TS [29..15], marker_bit,
	 *  TS [14..0], marker_bit,
	 *  extension [9], marker_bit
	 */

	return p + 6;
}

#define PS_PACK_HEADER_SIZE 14
#define PS_SCR_offset 9

static inline unsigned char *
PS_pack_header(unsigned char *p, double scr, double system_rate)
{
	int mux_rate = ((unsigned int) ceil(system_rate + 49)) / 50;

	// assert(mux_rate > 0);

	put(p, PACK_START_CODE, 4);
	time_stamp_ext(p + 4, 1, scr);
	put(p + 10, mux_rate >> 6, 2);
	put(p + 12, (mux_rate << 2) | 3, 1);
	put(p + 13, (0x1F << 3) | 0 /* psl */, 1);
	/*
	 *  pack_start_code [32], system_clock_reference [48],
	 *  marker_bit, program_mux_rate [22], marker_bit,
	 *  reserved [5], pack_stuffing_length [3]
	 */

	return p + PS_PACK_HEADER_SIZE;
}

#define PS_SYSTEM_HEADER_SIZE(nstr) (12 + (nstr) * 3)

static inline unsigned char *
PS_system_header(multiplexer *mux, unsigned char *p, double system_rate_bound)
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
			/*
			 *  '11', P-STD_buffer_bound_scale, 
			 *  P-STD_buffer_size_bound [13]
			 */

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

#define PES_PACKET_HEADER_SIZE 19

static inline unsigned char *
PES_packet_header(unsigned char *p, stream *str)
{
	put(p, PACKET_START_CODE + str->stream_id, 4);
	put(p + 4, 0, 2); // |->

	if (str->stream_id == PRIVATE_STREAM_1) {
		put(p + 6, (2 << 6) + (0 << 4) + (0 << 3) + (1 << 2) + (0 << 1) + (0 << 0), 1);
		put(p + 7, (0 << 6) + (0 << 5) + (0 << 4) + (0 << 3) + (0 << 2) + (0 << 1) + (0 << 0), 1);
		put(p + 8, 0x00, 1);
		memset(p + 9, 0xFF, 36);
		put(p + 45, 0x10, 1); // data_identifier: EBU data

		return p + 46;
	} else {
		put(p + 6, (2 << 6) + (0 << 4) + (0 << 3) + (0 << 2) + (0 << 1) + (0 << 0), 1);
		put(p + 7, (0 << 6) + (0 << 5) + (0 << 4) + (0 << 3) + (0 << 2) + (0 << 1) + (0 << 0), 1);
		put(p + 8, 0x00FFFFFF, 4);
		put(p + 12, 0xFFFFFFFF, 4);
		put(p + 16, 0xFFFFFF, 3);

		return p + PES_PACKET_HEADER_SIZE;
	}
	/*
	 *  packet_start_code_prefix [24], stream_id [8];
	 *  PES_packet_length [16];
	 *  '10', PES_scrambling_control [2], PES_priority,
	 *  data_alignment_indicator, copyright, original_or_copy;
	 *  PTS_DTS_flags [2], ESCR_flag, ES_rate_flag, DSM_trick_mode_flag,
	 *  additional_copy_info_flag, PES_CRC_flag, PES_extension_flag;
	 *  PES_header_data_length [8]
	 */
}

#define Rvid (1.0 / 1024)
#define Raud (0.0)

static inline bool
next_access_unit(stream *str, double *ppts, unsigned char **pph)
{
	buffer *buf;
	unsigned char *ph;

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

	if ((ph = *pph)) {
		if (IS_VIDEO_STREAM(str->stream_id)) {
			switch (buf->type) {
			case I_TYPE:
			case P_TYPE:
				*ppts = str->dts_old + str->ticks_per_frame * (buf->offset + 1);
				ph[7] |= MARKER_PTS << 6;
				time_stamp(ph +  9, MARKER_PTS, *ppts);
				time_stamp(ph + 14, MARKER_DTS, str->dts_old);
				*pph = NULL;
				break;
			
			case B_TYPE:
				*ppts = str->dts_old;
				ph[7] |= MARKER_PTS << 6;
				time_stamp(ph +  9, MARKER_PTS_ONLY, *ppts);
				/* time_stamp(ph + 14, MARKER_DTS, str->dts); */
				*pph = NULL;
				break;

			default:
				; /* no time stamp */
			}			
		} else {
			*ppts = str->dts_old;
/* XXX ?		*ppts = str->dts + str->pts_offset;
*/
			ph[7] |= MARKER_PTS_ONLY << 6;
			time_stamp(ph + 9, MARKER_PTS_ONLY, *ppts);
			*pph = NULL;
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
		double dtsi = s->dts_old;

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
mpeg2_program_stream_mux(void *muxp)
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

			if (buf->used <= 0)
				FAIL("Premature end of file / error");

			str->cap_t0 = buf->time;
	    		preload += buf->used;

			unget_full_buffer(&str->cons, buf);

			nstreams++;
		}

		buf = mux_output(mux, NULL);

		assert(buf && buf->size >= 512
		           && buf->size <= 32768);

		system_overhead = mux->packet_size / (mux->packet_size
			- (PS_SYSTEM_HEADER_SIZE(nstreams) + PS_PACK_HEADER_SIZE / PACKETS_PER_PACK));

		system_rate = system_rate_bound = bit_rate * system_overhead / 8;

		preload_delay = (preload * system_overhead + PS_PACK_HEADER_SIZE)
			/ system_rate_bound * SYSTEM_TICKS;

		scr = PS_SCR_offset / system_rate_bound * SYSTEM_TICKS;
		ticks_per_pack = (mux->packet_size * PACKETS_PER_PACK) / system_rate_bound * SYSTEM_TICKS;

		for_all_nodes (str, &mux->streams, fifo.node) {
			if (!IS_VIDEO_STREAM(str->stream_id)) {
				str->pts_offset = (double) SYSTEM_TICKS / (video_frame_rate * 1.0);
				/* + 0.1 to schedule video frames first */
				str->dts_old = preload_delay + 0.1;
			} else
				str->dts_old = preload_delay;
		}
	}

	/* Packet loop */

	for (;;) {
		p = buf->data;
		px = p + buf->size;

		/* Packet header, system header */

		if (pack_packet_count >= PACKETS_PER_PACK) {
			printv(4, "Pack #%d, scr=%f\n",	pack_count++, scr);

			p = PS_pack_header(p, scr, system_rate);
			p = PS_system_header(mux, p, system_rate_bound);

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

		if (str->stream_id == PRIVATE_STREAM_1)
			px -= (px - p) % 184;

		pl = p = PES_packet_header(p, str);

		/* Packet fill loop */

		pts = -1;

		while (p < px) {
			int n;

			if (str->left == 0) {
				if (!next_access_unit(str, &pts, &ph)) {
					str->dts_old = LARGE_DTS * 2.0; // don't schedule stream

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
			}

			n = MIN(str->left, px - p);

			memcpy(p, str->ptr, n);

			str->left -= n;

			if (!str->left) {
				send_empty_buffer(&str->cons, str->buf);

				str->buf = NULL;
				str->dts_old += str->ticks_per_frame;
			}

			p += n;

			str->ptr += n;
		}

		printv(4, "Packet #%d %s, pts=%f\n",
			packet_count, mpeg_header_name(str->stream_id), pts);

		((unsigned short *) ps)[2] = swab16(p - ps - 6);

		bytes_out += buf->used = p - buf->data;

		buf = mux_output(mux, buf);

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

			if (video_frames_dropped > 0)
				printv(1, "%d:%02d (%.1f MB), %.2f %% dropped, system load %.1f %%  %c",
					min, sec, bytes_out / (double)(1 << 20),
					100.0 * video_frames_dropped / video_frame_count,
					100.0 * system_load, (verbose > 3) ? '\n' : '\r');
			else
				printv(1, "%d:%02d (%.1f MB), system load %.1f %%  %c",
					min, sec, bytes_out / (double)(1 << 20),
					100.0 * system_load, (verbose > 3) ? '\n' : '\r');

			fflush(stderr);
		}
	}

	p = buf->data;

	*((unsigned int *) p) = swab32(MPEG_PROGRAM_END_CODE);
	if (PAD_PACKETS) {
		memset(p + 4, 0, buf->size - 4);
		buf->used = buf->size;
	} else
		buf->used = 4;

	mux_output(mux, buf);

	pthread_cleanup_pop(1);

	return NULL;
}
