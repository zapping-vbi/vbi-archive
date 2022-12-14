/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2002 Michael H. Schimek
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

/* $Id: mpeg1.c,v 1.16 2005-02-25 18:30:56 mschimek Exp $ */

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

#define TPF 3600.0

static inline rte_bool
next_access_unit(stream *str, double *ppts,
		 unsigned char **pph, rte_bool *eof)
{
	extern int split_sequence;
	buffer *buf;
	unsigned char *ph;

	str->buf = buf = wait_full_buffer(&str->cons);

	str->ptr  = buf->data;
	str->left = buf->used;

	if (buf->used <= 0) {
		if (!split_sequence || buf->error == 0xE0F)
			*eof = TRUE;

		send_empty_buffer(&str->cons, buf);

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

	if ((ph = *pph)) {
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
				*ppts = str->dts_old + str->ticks_per_frame * (buf->offset + 1); /* reorder delay */
				time_stamp(ph +  6, MARKER_PTS, *ppts);
				time_stamp(ph + 11, MARKER_DTS, str->dts_old);
				*pph = NULL;
				break;

			case B_TYPE:
				*ppts = str->dts_old; /* delay always 0 */
				time_stamp(ph + 11, MARKER_PTS_ONLY, *ppts);
				*pph = NULL;
				break;

			default:
				; /* no time stamp */
			}
		} else /* audio */ {
			*ppts = str->dts_old + str->pts_offset;
			time_stamp(ph + 11, MARKER_PTS_ONLY, *ppts);
			*pph = NULL;
		}

		printv(3, "%02x %c %06x dts=%16.8f pts=%16.8f off=%+2d %s\n",
			str->stream_id, "?IPB?"[buf->type], buf->used,
			str->dts_old / TPF, *ppts / TPF, buf->offset,
			*pph ? "not coded" : "");
	} else {
		printv(3, "%02x %c %06x dts=%16.8f in same packet\n",
			str->stream_id, "?IPB?"[buf->type], buf->used,
			str->dts_old / TPF);
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

		if (s->buf && s->left >= 0)
			dtsi += (s->ptr - s->buf->data) * s->ticks_per_byte;

		if (dtsi < dtsi_min) {
			str = s;
			dtsi_min = dtsi;
		}
	}

	return str;
}

static void
prolog(multiplexer *mux, buffer **obufp,
       double *system_overhead, double *system_rate,
       double *system_rate_bound, double *scr,
       double *ticks_per_pack, rte_bool restarted)
{
	stream *str;
	buffer *obuf;
	double preload_delay;
	double video_frame_rate = DBL_MAX;
	double max_dts_end = 0.0;
	int nstreams = 0;
	int preload = 0;
	int bit_rate = 0;

	for_all_nodes (str, &mux->streams, fifo.node) {
		buffer *ibuf;

		str->buf = NULL;
		bit_rate += str->bit_rate;
		str->eff_bit_rate = str->bit_rate;
		str->ticks_per_frame = (double) SYSTEM_TICKS / str->frame_rate;

		if (IS_VIDEO_STREAM(str->stream_id)
		    && str->frame_rate < video_frame_rate)
			video_frame_rate = str->frame_rate;

		if (str->dts_end > max_dts_end)
			max_dts_end = str->dts_end;

		ibuf = wait_full_buffer(&str->cons);

		if (ibuf->used <= 0) // XXX
			FAIL("Premature end of file / error");

		str->cap_t0 = ibuf->time;
		preload += ibuf->used;

		unget_full_buffer(&str->cons, ibuf);

		nstreams++;
	}

	if (!(obuf = *obufp))
		*obufp = obuf = mux->mux_output(mux, NULL);

	assert(obuf && obuf->size >= 512
	       && obuf->size <= 32768);

	*system_overhead = mux->packet_size / (mux->packet_size
		- (SYSTEM_HEADER_SIZE(nstreams)
		+ PACK_HEADER_SIZE / PACKETS_PER_PACK));

	*system_rate = *system_rate_bound = bit_rate * *system_overhead / 8;

	/* Frames must arrive completely before decoding starts */
	preload_delay = (preload * *system_overhead + PACK_HEADER_SIZE)
		/ *system_rate_bound * SYSTEM_TICKS;

	*scr = SCR_offset / *system_rate_bound * SYSTEM_TICKS;
	*ticks_per_pack = (mux->packet_size * PACKETS_PER_PACK)
		/ *system_rate_bound * SYSTEM_TICKS;

	for_all_nodes (str, &mux->streams, fifo.node) {
		if (restarted) {
			double shift = max_dts_end - str->dts_end;

			// fprintf(stderr, "SH %02x %f\n", str->stream_id, shift);
			/* Carry over relative sampling instant shift */
			str->dts_old = preload_delay + shift;
		} else {
			/* Video PTS is delayed by one frame */
			if (!IS_VIDEO_STREAM(str->stream_id)) {
				str->pts_offset = (double) SYSTEM_TICKS
					/ (video_frame_rate * 1.0);

				/* + 0.00001 to schedule video frames first */
				str->dts_old = preload_delay + 0.00001;
			} else
				str->dts_old = preload_delay;
		}
	}
}

void *
mpeg1_system_mux(void *muxp)
{
	extern int split_sequence;
	extern long long part_length;
	extern void break_sequence(void);
	multiplexer *mux = muxp;
	unsigned char *p, *ph, *ps, *pl, *px;
	unsigned int pack_packet_count;
	unsigned int pack_count;
	double system_rate, system_rate_bound;
	double system_overhead;
	double ticks_per_pack;
	double scr, pts, front_pts = 0.0;
	rte_bool eof = FALSE;
	buffer *obuf = NULL;
	stream *str;

	pthread_cleanup_push((void (*)(void *)) pthread_rwlock_unlock,
			     (void *) &mux->streams.rwlock);

	assert(pthread_rwlock_rdlock(&mux->streams.rwlock) == 0);

	mux->status.frames_out = 0;
	mux->status.bytes_out = 0;
	mux->status.coded_time = 0.0;
	mux->status.valid = 0
		+ RTE_STATUS_FRAMES_OUT
		+ RTE_STATUS_BYTES_OUT
		+ RTE_STATUS_CODED_TIME;

#if CDLOG
	if ((cdlog = fopen("cdlog", "w"))) {
		fprintf(cdlog, "Clock drift\n\n");
	}
#endif

	prolog(mux, &obuf, &system_overhead,
	       &system_rate, &system_rate_bound,
	       &scr, &ticks_per_pack, FALSE);

	/* Packet loop */

restart:
	pack_packet_count = PACKETS_PER_PACK;
	pack_count = 0;

	for (;;) {
		p = obuf->data;
		px = p + obuf->size;

		/* Pack header, system header */

		if (pack_packet_count >= PACKETS_PER_PACK) {
			printv(3, "Pack #%d, scr=%f\n",	pack_count++, scr);

			p = pack_header(p, scr, system_rate);
			p = system_header(mux, p, system_rate_bound);

			if (0)
				scr += ticks_per_pack;
			else {
				int bit_rate = 0;

				for_all_nodes (str, &mux->streams, fifo.node)
					bit_rate += str->eff_bit_rate;

				system_rate = bit_rate * system_overhead / 8;
				scr += (mux->packet_size * PACKETS_PER_PACK)
					/ system_rate * SYSTEM_TICKS;
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
				if (!next_access_unit(str, &pts, &ph, &eof)) {
					str->dts_end = str->dts_old;
					/* stream ended, don't schedule */
					str->dts_old = LARGE_DTS * 2.0;

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

		printv(3, "Packet #%lld %s, pts=%f\n",
			mux->status.frames_out, mpeg_header_name(str->stream_id), pts);

		((unsigned short *) ps)[2] = swab16(p - ps - 6);

		obuf->used = p - obuf->data;

		if (part_length > 0
		    && mux->status.bytes_out > 0
		    && (mux->status.bytes_out + obuf->used) >= part_length) {
			break_sequence();
			mux->status.bytes_out = obuf->used;
		} else {
			mux->status.bytes_out += obuf->used;
		}

		obuf = mux->mux_output(mux, obuf);

		assert(obuf && obuf->size >= 512
			   && obuf->size <= 32768);

		if (pts > front_pts)
			front_pts = pts;

		/* XXX lock */
		mux->status.coded_time = front_pts * (1.0 / SYSTEM_TICKS);
		mux->status.frames_out++;

		pack_packet_count++;

		if (verbose > 0 && (mux->status.frames_out & 3) == 0) {
#ifdef VIDEO_FIFO_TEST
			extern double in_fifo_load, out_fifo_load;
#endif
			double system_load = 1.0 - get_idle();
			int min, sec;

			sec = front_pts / SYSTEM_TICKS;
			min = sec / 60;
			sec -= min * 60;

#ifdef VIDEO_FIFO_TEST
			printv(1, "%d:%02d (%.1f MB), system load %4.1f %%"
				" [V%2.1f:%2.1f]",
				min, sec,
				mux->status.bytes_out / (double)(1 << 20),
				100.0 * system_load,
				in_fifo_load, out_fifo_load);
#else
			printv(1, "%d:%02d (%.1f MB), system load %4.1f %%",
				min, sec,
				mux->status.bytes_out / (double)(1 << 20),
				100.0 * system_load);
#endif
			if (video_frames_dropped > 0)
				printv(1, ", %llu (%5.2f %%) dropped",
					video_frames_dropped,
					100.0 * video_frames_dropped / video_frame_count);

			printv(1, (verbose > 2) ? "\n" : "  \r");

			fflush(stderr);
		}
	}

	p = obuf->data;

	*((unsigned int *) p) = swab32(ISO_END_CODE);

	if (PAD_PACKETS) {
		memset(p + 4, 0, obuf->size - 4);
		obuf->used = obuf->size;
	} else
		obuf->used = 4;

	mux->mux_output(mux, obuf);

	if (split_sequence && !eof) {
		break_sequence();

		prolog(mux, &obuf, &system_overhead,
		       &system_rate, &system_rate_bound,
		       &scr, &ticks_per_pack, TRUE);

		goto restart;
	}

	obuf->used = 0; /* EOF */

	mux->mux_output(mux, obuf);

	pthread_cleanup_pop(1);

	return NULL;
}
