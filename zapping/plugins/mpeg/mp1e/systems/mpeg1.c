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

/* $Id: mpeg1.c,v 1.1 2000-07-04 17:40:20 garetxe Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "../video/mpeg.h"
#include "../video/video.h"
#include "../audio/mpeg.h"
#include "../audio/audio.h"
#include "../options.h"
#include "../fifo.h"
#include "../log.h"
#include "mpeg.h"
#include "systems.h"

/*
 *  Simple MPEG-1 systems multiplexer for
 *  one video and one audio CBR stream.
 */

#define 			PACKET_SIZE		2048	// including any headers
#define 			PACKETS_PER_PACK	16

static unsigned char *		mux_buffer;

extern int			stereo;
extern volatile int		quit_please;

/*
 *  This routine must execute after capturing has started,
 *  but prior to compression and mux. Called only once to synchronize
 *  because the compression threads compensate frame dropping locally
 *  and we [will] use a PLL algorithm for clock drift compensation.
 */
void
mpeg1_system_run_in(void)
{
	int vindex;
	void *ap = NULL, *vp = NULL;
	double atime, vtime, d, max_d = 0.75 / frame_rate_value[frame_rate_code];

	ASSERT("allocate mux buffer, %d bytes",
		(mux_buffer = calloc_aligned(PACKET_SIZE, 32)) != NULL, PACKET_SIZE);

	for (;;) {
		if (!ap)
			ap = audio_read(&atime);
		if (!vp)
			vp = video_wait_frame(&vtime, &vindex);
		if (!ap || !vp)
			FAIL("Premature end of file");

		printv(3, "Sync vtime=%f, atime=%f\n", vtime, atime);

		d = vtime - atime;

		if (fabs(d) <= max_d) {
			break;
		}

		if (d < 0) {
			video_frame_done(vindex);
			vp = NULL;
		} else
			ap = NULL;
	}

	video_unget_frame(vindex);
	audio_unget(ap);
}

static inline void
putt(unsigned char *d, int mark, unsigned int time)
{
	d[0] = (mark << 4) | (time >> 29) | 1;
	d[1] = time >> 22;
	d[2] = (time >> 14) | 1;
	d[3] = time >> 7;
	d[4] = (time << 1) | 1;
}

static inline void
put4(unsigned char *d, unsigned int n)
{
	*((unsigned int *) d) = bswap(n);
}

static inline void
put3(unsigned char *d, unsigned int n)
{
	d[0] = n >> 16;
	d[1] = n >> 8;
	d[2] = n;
}

static inline void
put2(unsigned char *d, unsigned int n)
{
	d[0] = n >> 8;
	d[1] = n;
}

void *
mpeg1_system_mux(void *unused)
{
	unsigned int video_frame = 0, packet = 0, mux_rate_code;
	double system_rate = 0, scr, pack_tick;
	buffer *vbuf, *abuf;
	bool done = FALSE;

	// Get sizes of pending first frames

	pthread_mutex_lock(&mux_mutex);

	while (!(vbuf = (buffer *) vid.full.head))
		pthread_cond_wait(&mux_cond, &mux_mutex);

	while (!(abuf = (buffer *) aud.full.head))
		pthread_cond_wait(&mux_cond, &mux_mutex);

	pthread_mutex_unlock(&mux_mutex);

	if (!abuf->size || !vbuf->size)
		FAIL("Premature end of file");

	// Rate control

	{
		double overhead = PACKET_SIZE / (PACKET_SIZE - (16.0 + 30.0 / PACKETS_PER_PACK));
		double delay;

		system_rate = ((video_bit_rate + (audio_bit_rate << stereo)) / 8) * overhead;

		mux_rate_code = 0x800001 + ((unsigned int)((ceil(system_rate) + 49) / 50) << 1);

		scr = 8 /* SCR field offset */ / system_rate * SYSTEM_TICKS;
		pack_tick = (PACKET_SIZE * PACKETS_PER_PACK) / system_rate * SYSTEM_TICKS;

		vid.tick = (double) SYSTEM_TICKS / frame_rate_value[frame_rate_code];
		aud.tick = (double) SYSTEM_TICKS * 1152 / sampling_rate;

		vid.rtick = 1.0 / frame_rate_value[frame_rate_code];
		aud.rtick = 1152.0 / sampling_rate;
		vid.rtime = 0.0;
		aud.rtime = 0.0;

		delay = (vbuf->size + abuf->size) * overhead + 30.0;

		vid.dts = delay / system_rate * SYSTEM_TICKS;
		aud.dts = vid.dts + vid.tick * 1.0; // 1.0 .. 2.0

		vid.time = 0;
		aud.time = 0;

		printv(3,
			"system_rate=%.2f byte/s, mux_rate_code=0x%06x,\n"
			"scr=%.2f st, pack_tick=%.2f st,\n"
			"frame_tick=%.2f,%.2f st, dts=%.2f,%.2f st,\n",
			system_rate, mux_rate_code, scr, pack_tick,
			vid.tick, aud.tick, vid.dts, aud.dts);
	}

	// Packet loop

	while (!done) {
		fifo *f;
		int empty = PACKET_SIZE;
		unsigned char *ph, *p = mux_buffer;
		double pts;

		if ((packet % PACKETS_PER_PACK) == 0) {
			put4(p, PACK_START_CODE);
			putt(p + 4, MARKER_SCR, floor(scr + 0.5));
			put3(p + 9, mux_rate_code);

			put4(p + 12, SYSTEM_HEADER_CODE);
			put2(p + 16, 6 + 3 + 3);
			put3(p + 18, mux_rate_code);
			put3(p + 21, 0x07E1FF);
			put3(p + 24, (AUDIO_STREAM_0 << 16) | 0xC000 | 32);
			put3(p + 27, (VIDEO_STREAM_0 << 16) | 0xE000 | 40);

			printv(4, "Pack #%d, scr=%f\n",
				packet / PACKETS_PER_PACK, scr);

			scr += pack_tick;

			p += 30;
			empty -= 30;

			get_idle();
		}

		f = (vid.time <= aud.time) ? &vid : &aud;

		{
			ph = p;

			((unsigned int *) p)[0] = bswap(PACKET_START_CODE + ((f == &vid) ? VIDEO_STREAM_0 : AUDIO_STREAM_0));
			((unsigned int *) p)[1] = bswap(((empty - 6) << 16) + 0xFFFF);
			((unsigned int *) p)[2] = 0xFFFFFFFF;
			((unsigned int *) p)[3] = 0x0FFFFFFF;

			p += 16;
			empty -= 16;
		}

		// Packet fill loop

		pts = -1;

		while (empty > 0) {
			if (f->left == 0) {
				pthread_mutex_lock(&mux_mutex);

				for (;;) {
					if ((f->buf = (buffer *) rem_head(&f->full)))
						break;

					pthread_cond_wait(&mux_cond, &mux_mutex);
				}

				pthread_mutex_unlock(&mux_mutex);

				f->ptr  = f->buf->data;
				f->left = f->buf->size;
				f->rtime += f->rtick;

				if (!f->left)
					FAIL("Oops, not prepared for EOF.");

				if (f == &vid) {
					if (ph) {
						pts = vid.dts + vid.tick;

						putt(ph +  6, MARKER_PTS, floor(pts + 0.5));
						putt(ph + 11, MARKER_DTS, floor(vid.dts + 0.5));
						output(mux_buffer, p - mux_buffer);
						ph = NULL;
					}

					video_frame++;
				} else {
					if (ph)	{
						pts = aud.dts;

						putt(ph + 11, MARKER_PTS_ONLY, pts);
						output(mux_buffer, p - mux_buffer);
						ph = NULL;
					}
				}

				f->dts += f->tick;
				f->tpb  = f->tick / f->left;
			}

			if (f->left < empty) {
				if (ph) {
					memcpy(p, f->ptr, f->left);
					p += f->left;
				} else
					output(f->ptr, f->left);

				f->time += f->tpb * f->left;
				empty     -= f->left;
				f->left  = 0;
			} else {
				if (ph)	{
					output(mux_buffer, p - mux_buffer);
					ph = NULL;
				}

				output(f->ptr, empty);

				f->time += f->tpb * empty;
				f->ptr  += empty;
				f->left -= empty;
				empty      = 0;
			}

			if (f->left == 0) {
				empty_buffer(f, f->buf);

				if (f == &vid)
					if (video_frame >= video_num_frames || quit_please) {
						if (!ph)
							p = mux_buffer;

						if (empty >= 8) { // XXX
							((unsigned int *) p)[0] = bswap(SEQUENCE_END_CODE);
							((unsigned int *) p)[1] = bswap(ISO_END_CODE);

							p += 8;
							empty -= 8;
						}

						memset(p, 0, empty);

						output(mux_buffer, (p - mux_buffer) + empty);

						if (aud.left > 0) {
							// XXX does not stop audio in time

							ASSERT("(ugly hack)", aud.left < (PACKET_SIZE - 16));

							((unsigned int *) mux_buffer)[0] = bswap(PACKET_START_CODE + AUDIO_STREAM_0);
							((unsigned int *) mux_buffer)[1] = bswap(((PACKET_SIZE - 6) << 16) + 0xFFFF);
							((unsigned int *) mux_buffer)[2] = 0xFFFFFFFF;
							((unsigned int *) mux_buffer)[3] = 0x0FFFFFFF;

							output(mux_buffer, 16);
							output(aud.ptr, aud.left);
							memset(mux_buffer, 0, PACKET_SIZE - 16 - aud.left);
							output(mux_buffer, PACKET_SIZE - 16 - aud.left);
						}

						printv(1, "\n%s: %d video frames done.\n",
							my_name, video_frame);

						empty = 0;
						done = 1;
					}
			}
/*
			if (verbose > 3) {
				int al = aud.in, vl = vid.in;
				if (al < aud.out) al += aud.max;
				if (vl < vid.out) vl += vid.max;
				al = 100 * (al - aud.out) / aud.max;
				vl = 100 * (vl - vid.out) / vid.max;
				printv(4, "fifo load=%3d%%,%3d%%\n", vl, al);
			}
*/
		} // while (empty > 0)

		printv(4, "Packet #%d %s, pts=%f\n",
			packet, (f == &vid) ? "video" : "audio", pts);

		if (verbose > 0) {
			int min, sec;

			sec = bytes_out / system_rate;
			min = sec / 60;
			sec -= min * 60;

			if (video_frames_dropped > 0)
				printv(1, "%d:%02d (%.1f MB), %.2f %% dropped, system load %.1f %%  %c",
					min, sec, bytes_out / ((double)(1 << 20)),
					100.0 * video_frames_dropped / video_frame_count,
					100.0 * (1.0 - system_idle), (verbose > 3) ? '\n' : '\r');
			else
				printv(1, "%d:%02d (%.1f MB), system load %.1f %%  %c",
					min, sec, bytes_out / ((double)(1 << 20)),
					100.0 * (1.0 - system_idle), (verbose > 3) ? '\n' : '\r');

			fflush(stderr);
		}

		packet++;

	} // while (!done)

	return NULL;
}
