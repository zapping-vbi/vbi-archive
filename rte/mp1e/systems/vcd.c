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

/* $Id: vcd.c,v 1.13 2002-12-14 00:43:44 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <limits.h>

#include "../video/mpeg.h"
#include "../video/video.h"
#include "../audio/libaudio.h"
#include "../common/log.h"
#include "../common/fifo.h"
#include "../common/math.h"

#include "mpeg.h"
#include "systems.h"

#define TEST(x) x
#define PVER 4

const int sector_size		= 2324;			/* bytes */
const int system_rate		= 75 * 2324;		/* bytes */
const int ticks_per_packet	= SYSTEM_TICKS / 75;	/* SYSTEM_TICKS */
const int BS_audio		= 4096;			/* bytes */
const int BS_video		= 40960;		/* bytes */
/* video XXX? */

#define put(p, val, bytes)						\
do {									\
	uint32_t v = val;						\
									\
	switch (bytes) {						\
	case 4:								\
		/* Uses bswap or compile time swapping if const	*/	\
		*((uint32_t *)(p)) = swab32 (v);			\
		break;							\
	case 3:	(p)[bytes - 3] = v >> 16;				\
	case 2:	(p)[bytes - 2] = v >> 8;				\
	case 1:	(p)[bytes - 1] = v;					\
	}								\
} while (0)

static inline uint8_t *
timestamp			(uint8_t *		p,
				 tstamp_marker		marker,
				 tstamp			ts)
{
	p[0] = (marker << 4) + ((ts >> 29) & 0xE) + 1;
	p[1] = ((uint32_t) ts >> 22);
	p[2] = ((uint32_t) ts >> 14) | 1;
	p[3] = ((uint32_t) ts >> 7);
	p[4] = (uint32_t) ts * 2 + 1;
	/*
	 *  marker [4], TS [32..30], marker_bit,
	 *  TS [29..15], marker_bit,
	 *  TS [14..0], marker_bit
	 */

	return p + 5;
}

/*
 *  SCR indicates the intended time of arrival of the last
 *  byte of the system_clock_reference field at the input of the STD.
 */
#define SCR_offset 8

#define PACK_HEADER_SIZE 12

static inline uint8_t *
pack_header			(uint8_t *		p,
				 tstamp			scr,
				 int			system_rate)
{
	unsigned int mux_rate = ((unsigned int) ceil (system_rate + 49)) / 50;

	assert (mux_rate < (1 << 22));

	put (p, PACK_START_CODE, 4);			/* pack_start_code [32] */
	timestamp (p + 4, MARKER_SCR, scr);		/* system_clock_reference [40] */
	put (p + 9, (mux_rate >> 15) | 0x80, 1);	/* marker_bit, */
	put (p + 10, mux_rate >> 7, 1);			/* program_mux_rate [22], */
	put (p + 11, mux_rate * 2 + 1, 1);		/* marker_bit */

	return p + 12;
}

#define SYSTEM_HEADER_SIZE 15

static inline uint8_t *
system_header			(uint8_t *		p,
				 stream *		str,
				 int			system_rate_bound)
{
	unsigned int mux_rate_bound = (system_rate_bound + 49) / 50;
	unsigned int audio_bound = 0;
	unsigned int video_bound = 0;

	assert (mux_rate_bound < (1 << 22));

	put (p + 12, str->stream_id, 1);		/* stream_id */

	if (str->stream_id == VIDEO_STREAM + 0) {
		put(p + 13, (0x3 << 14)			/* '11' */
			    + (1 << 13)			/* STD_buffer_bound_scale */
			    + (BS_video >> 10), 2);	/* STD_buffer_size_bound */
		video_bound++;
	} else if (str->stream_id == AUDIO_STREAM + 0) {
		put(p + 13, (0x3 << 14)			/* '11' */
			    + (0 << 13)			/* STD_buffer_bound_scale */
			    + (BS_audio >> 7), 2);	/* STD_buffer_size_bound */
		audio_bound++;
	} else {
		FAIL("Stream id mismatch\n");
	}

	put (p, SYSTEM_HEADER_CODE, 4);			/* system_header_start_code [32] */
	put (p + 4, 9, 2);				/* header_length [16] */
	put (p + 6, (mux_rate_bound >> 15) | 0x80, 1);	/* marker_bit, */
	put (p + 7, mux_rate_bound >> 7, 1);		/* rate_bound [22], */
	put (p + 8, (mux_rate_bound << 1) | 1, 1);	/* marker_bit */
	put (p + 9, (audio_bound << 2)			/* audio_bound [6] */
		   + (0 << 1)				/* fixed_flag */
		   + (1 << 0), 1);			/* CSPS_flag */
	put (p + 10, (1 << 7)				/* system_audio_lock_flag */
		    + (1 << 6)				/* system_video_lock_flag */
		    + (1 << 5)				/* marker_bit */
		    + (video_bound << 0), 1);		/* video_bound [5] */
	put (p + 11, 0xFF, 1);				/* reserved_byte [8] */

	return p + 15;
}

#define PADDING_MIN 7

static inline uint8_t *
padding_packet			(uint8_t *		p,
				 int			size)
{
	if (size < PADDING_MIN)
		return p;

	put (p, PACKET_START_CODE			/* packet_start_code_prefix [23] */
		+ PADDING_STREAM, 4);			/* stream_id [8] */
	/* 2.4.4.3: bytes after packet_length field */
	put (p + 4, size - 6, 2);			/* packet_length [16] */
	put (p + 6, 0x0F, 1);				/* '0000 1111' */

	memset (p + 7, PADDING_BYTE, size - 7);		/* N * packet_data_byte [8] */

	return p + size;
}

#define PACKET_HEADER_SIZE(id) (((id) == (VIDEO_STREAM + 0)) ? 18 : 13)

static inline rte_bool
stamp_packet			(stream *		str,
				 buffer *		buf,
				 uint8_t *		ph,
				 tstamp *		ppts)
{
	if (str->stream_id == VIDEO_STREAM + 0) {
		/*
		 *  display order	IBBPBBIBBPBBP
		 *  coded order		IPBBIBBPBBPBB
		 *  DTS			0123456789ABC
		 *  PTS			1423756A89DBC
		 */
		switch (buf->type) {
		case I_TYPE:
			*ppts = str->dts + str->ticks_per_frame * (buf->offset + 1);
			/* STD_buffer is optional, this mimics a reference stream */
			put (ph + 6, (0x1 << 14)		/* '01' */
				     + (1 << 13)		/* STD_buffer_scale */
				     + (BS_video >> 10), 2);	/* STD_buffer_size [13] */
			timestamp (ph +  8, MARKER_PTS, *ppts);
			timestamp (ph + 13, MARKER_DTS, str->dts);
			return TRUE;

		case P_TYPE:
			*ppts = str->dts + str->ticks_per_frame * (buf->offset + 1);
			/* Caller stores 2 * stuffing_byte [8] = PADDING_BYTE at ph + 6 */
			timestamp (ph +  8, MARKER_PTS, *ppts);
			timestamp (ph + 13, MARKER_DTS, str->dts);
			return TRUE;

		case B_TYPE:
			*ppts = str->dts;
			/*
			 *  Caller stores 7 * stuffing_byte [8] = PADDING_BYTE at ph + 6.
			 *  No DTS here but we need a constant header length to predict
			 *  data rates.
			 */
			timestamp (ph + 13, MARKER_PTS_ONLY, *ppts);
			return TRUE;

		default:
			return FALSE; /* no time stamp */
		}
	} else {
		*ppts = str->dts + str->pts_offset;
		/* STD_buffer is optional, this mimics a reference stream */
		put (ph + 6, (0x1 << 14)		/* '01' */
			     + (0 << 13)		/* STD_buffer_scale */
			     + (BS_audio >> 7), 2);	/* STD_buffer_size [13] */
		timestamp (ph + 8, MARKER_PTS_ONLY, *ppts);
		return TRUE;
	}
}

static inline uint8_t *
fill_packet(uint8_t *p, uint8_t *ph, uint8_t *px,
	    stream *str, tstamp dend, tstamp *ppts)
{
	access_unit *au = str->au_w;
	rte_bool stamped = FALSE;

	while (p < px) {
		int n;

		if (str->left == 0) {
			buffer *buf;

			/* Frame buffer empty */

			if (str->dts >= dend) {
				/*
				 *  When we get many frames much shorter than expected
				 *  (eg. a series of skipped pictures) we must assure not
				 *  to exceed the encoder->mux fifo depth by attempting
				 *  to fill the current packet.
				 */
				if (0)
					fprintf(stderr, "str %x ->dts %lld >= dend %lld\n",
						str->stream_id, str->dts, dend);

				if ((px - p) >= PADDING_MIN) {
					break;
				}
			}

			str->buf = buf = wait_full_buffer(&str->cons);

			str->ptr = buf->data;

			/* XXX buf->used < 0 error -? */

			if (buf->used <= 0) {
				if (0)
					fprintf(stderr, "fill_packet end of stream %x\n",
						str->stream_id);
				str->left = -1;
				break;
			}

			str->left = buf->used;

			str->ticks_per_byte = str->ticks_per_frame / buf->used;

			/* Another AU enters Bn */

			assert(au != str->au_r && "str->au_ring[] too small");

			au->dts = str->dts;
			au->size = 0;

			if (!stamped) {
				/* Add DTS/PTS of first access unit commencing in packet. */
				stamped = stamp_packet(str, buf, ph, ppts);
			}
		}

		n = MIN(str->left, px - p);

		memcpy(p, str->ptr, n);

		au->size += n;
		str->inbuf_free -= n;

		TEST(assert(au != str->au_r));
		TEST(assert(str->inbuf_free >= 0));

		str->ptr += n;
		str->left -= n;

		if (str->left == 0) {
			if (++au >= &str->au_ring[elements(str->au_ring)])
				au = &str->au_ring[0];

			str->au_w = au;

			send_empty_buffer(&str->cons, str->buf);

			str->buf = NULL;
			str->dts += str->ticks_per_frame;
		}

		p += n;
	}

	return p;
}

static inline stream *
schedule(multiplexer *mux, tstamp scr)
{
	tstamp dts_min = TSTAMP_MAX / 2;
	stream *s, *str = NULL;

	for_all_nodes (s, &mux->streams, fifo.node) {
		tstamp dts = s->dts;

		if (s->buf)
			dts += (s->ptr - s->buf->data) * s->ticks_per_byte;

		if (dts >= dts_min) {
			if (0)
				fprintf(stderr, "schedule %x dts %lld >= dts_min %lld\n",
					s->stream_id, dts, dts_min);
			continue;
		}

		dts_min = dts;

		for (;;) {
			access_unit *au = s->au_r + 1;

			if (au >= &s->au_ring[elements(s->au_ring)])
				au = &s->au_ring[0];

			if (au != s->au_w) {
				if (au->dts < scr) {
					/* AU removed from Bn at au->dts */

					s->inbuf_free += au->size;
					s->au_r = au;

					TEST(assert(s->inbuf_free <= ((s->stream_id
					     == VIDEO_STREAM) ? BS_video : BS_audio)));

					continue;
				}
			}

			break;
		}

		if (s->inbuf_free < s->packet_payload) {
			if (0)
				fprintf(stderr, "schedule %x s->inbuf_free %d < pp %d\n",
					s->stream_id, s->inbuf_free, s->packet_payload);
			continue; /* not enough space in Bn */
		}

		str = s;
	}

	return str;
}

void *
vcd_system_mux(void *muxp)
{
	tstamp scr, pts, front_pts = 0;
	multiplexer *mux = muxp;
	int nstreams, fspace;
	stream *str, *video_stream = NULL;
	stream *audio_stream = NULL;
	rte_bool terminated = FALSE;
	buffer *buf;
	uint8_t *p;

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

	/* Initialization */

	{
		double video_frame_rate = 0;
		int preload_delay = INT_MAX;
		int packets = 0;

		nstreams = 0;
		fspace = 0;

		for_all_nodes (str, &mux->streams, fifo.node) {
			buffer *buf;
			int inbuft;

			str->ticks_per_frame = (double) SYSTEM_TICKS / str->frame_rate;
			str->buf = NULL;

			buf = wait_full_buffer(&str->cons);

			if (buf->used <= 0) /* XXX */
				FAIL("Premature end of file / error");

			if (IS_VIDEO_STREAM(str->stream_id)) {
				video_stream = str;
				video_frame_rate = str->frame_rate;

				str->inbuf_free = BS_video;
				str->packet_payload = sector_size
					- PACK_HEADER_SIZE - PACKET_HEADER_SIZE(VIDEO_STREAM);

				packets += (buf->used + str->packet_payload - 1) / str->packet_payload;
			} else {
				audio_stream = str;

				str->inbuf_free = BS_audio;
				str->packet_payload = sector_size
					- PACK_HEADER_SIZE - PACKET_HEADER_SIZE(AUDIO_STREAM);

				packets += (buf->used + str->packet_payload - 1) / str->packet_payload;
			}

			unget_full_buffer(&str->cons, buf);

			/* Bn size in system clock ticks */
			inbuft = str->inbuf_free * (tstamp)(SYSTEM_TICKS * 8) / str->bit_rate;

			fspace = MAX(fspace, inbuft); // XXX correct?
			preload_delay = MIN(preload_delay, inbuft * 4/5);

			nstreams++;
		}

		if (!video_stream || !audio_stream || nstreams != 2)
			FAIL("One video, one audio stream required");

		packets += 2; /* system headers */

		/*
		 *  First frames must arrive completely before decoding starts,
		 *  we aim at a start time when the input buffers are roughly
		 *  half filled.
		 */
		preload_delay = MAX(preload_delay, packets * ticks_per_packet);

		for_all_nodes (str, &mux->streams, fifo.node) {
			/* Video PTS is delayed by one frame */
			if (!IS_VIDEO_STREAM(str->stream_id)) {
				str->pts_offset = (double) SYSTEM_TICKS / video_frame_rate;
				str->dts = preload_delay;
			} else {
				str->dts = preload_delay;
			}
		}
	}

	scr = SCR_offset * SYSTEM_TICKS / system_rate;

	/* System header */

	buf = mux->mux_output(mux, NULL);
	assert(buf && buf->size >= sector_size);

	p = buf->data;
	p = pack_header(p, scr, system_rate);
	p = system_header(p, video_stream, system_rate);

	p = padding_packet(p, buf->data + buf->size - p);

	mux->status.bytes_out += buf->used = p - buf->data;
	buf = mux->mux_output(mux, buf);
	assert(buf && buf->size >= sector_size);

	scr += ticks_per_packet;

	p = buf->data;
	p = pack_header(p, scr, system_rate);
	p = system_header(p, audio_stream, system_rate);

	p = padding_packet(p, buf->data + buf->size - p);

	mux->status.bytes_out += buf->used = p - buf->data;
	buf = mux->mux_output(mux, buf);
	assert(buf && buf->size >= sector_size);

	mux->status.frames_out = 2;

	/* Packet loop */

	while (nstreams > 0) {
		uint8_t *px; /* packet buffer end + 1 */

		scr += ticks_per_packet;

		p = buf->data;
		px = p + buf->size;
		p = pack_header(p, scr, system_rate);

		pts = -1; /* for status report: no new frame in packet */
reschedule:
		str = schedule(mux, scr);

		if (!str) {
			p = padding_packet(p, px - p);

			printv(PVER, "Packet #%lld %s\n",
			       mux->status.frames_out, mpeg_header_name(PADDING_STREAM));
		} else {
			uint8_t *ph = p; /* packet header */
			int n, m; /* max payload, timestamp size */

			n = px - p - 7; /* max payload (id[4], length[2], 0F[1]) */
			m = PACKET_HEADER_SIZE(str->stream_id) - 7;

			put(p, PACKET_START_CODE + str->stream_id, 4);
			put(p + 4, 0xFFFFFFFF, 4);
			put(p + 8, 0xFFFFFFFF, 4);
			put(p + 12, 0xFFFFFFFF, 4);
			put(p + 16, 0xFF0F, 2);

			if (str->left >= (n - m)) {
				/* Continue frame */

				m = MIN(str->left, n);
				p += n - m + 7; /* padding */

				put(p - 1, 0x0F, 1); /* no ts */

				memcpy(p, str->ptr, m);

				str->ptr += m;
				str->left -= m;

				if (!str->left) {
					send_empty_buffer(&str->cons, str->buf);

					str->buf = NULL;
					str->dts += str->ticks_per_frame;
				}

				printv(PVER, "Packet #%lld %s\n",
				       mux->status.frames_out, mpeg_header_name(str->stream_id));

				put(ph + 4, px - ph - 6, 2);
			} else {
				uint8_t *pp = p + m + 7; /* start of payload */

				/* New frame commencing in packet */

				/* In case new frame is stream end, no ts */
				put(pp - 1, 0x0F, 1);

				p = fill_packet(pp, ph, px, str, scr + fspace, &pts);

				if (str->left == -1) {
					str->dts = TSTAMP_MAX; /* don't schedule anymore */
					nstreams--;

					if (p == pp) {
						/*
						 *  No payload, happens when end of stream
						 *  aligns with packet start. Instead of padding
						 *  we schedule another elementary stream.
						 */
						p = ph;
						goto reschedule;
					}

					if ((px - p) < PADDING_MIN) {
						n = px - p;
						if (nstreams == 1 && n > 4)
							n -= 4;
						memset(p, 0, n);
						p += n;
					}
				}

				printv(PVER, "Packet #%lld %s, pts=%lld\n",
				       mux->status.frames_out, mpeg_header_name(str->stream_id), pts);

				put(ph + 4, p - ph - 6, 2); /* packet size */


				if (p < px) {
					/* Pad to sector size (stream end only) */

					n = px - p;

					if (nstreams == 0 && n >= (PADDING_MIN + 4)) {
						p = padding_packet(p, n - 4);
						put(p, MPEG_PROGRAM_END_CODE, 4);
						terminated = TRUE;
					} else {
						assert(n >= PADDING_MIN);
						p = padding_packet(p, n);
					}
				}
			}
		}

		mux->status.bytes_out += buf->used = sector_size;

		buf = mux->mux_output(mux, buf);
		assert(buf && buf->size >= sector_size);

		if (pts > front_pts)
			front_pts = pts;

		mux->status.bytes_out = front_pts * (1.0 / SYSTEM_TICKS);
		mux->status.frames_out++;

		if (verbose > 0) {
			if ((mux->status.frames_out & 3) == 0) {
				double system_load = 1.0 - get_idle();
				int min, sec;

				sec = front_pts / SYSTEM_TICKS;
				min = sec / 60;
				sec -= min * 60;

				printv(1, "%d:%02d (%.1f MB), system load %4.1f %%",
				       min, sec,
				       mux->status.bytes_out / (double)(1 << 20),
				       100.0 * system_load);

				if (video_frames_dropped > 0)
					printv(1, ", %5.2f %% dropped",
					       100.0 * video_frames_dropped / video_frame_count);

				printv(1, (verbose > 3) ? "\n" : "  \r");

				fflush(stderr);
			}
		}
	}

	if (!terminated) {
		p = padding_packet(buf->data, sector_size - 4);
		put(p, MPEG_PROGRAM_END_CODE, 4);
		buf->used = sector_size;
		mux->mux_output(mux, buf);
	}

	buf->used = 0; /* EOF */

	mux->mux_output(mux, buf);

	pthread_cleanup_pop(1);

	return NULL;
}
