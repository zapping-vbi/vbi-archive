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

/* $Id: vbi.c,v 1.5 2000-11-01 08:59:18 mschimek Exp $ */

#include "../common/fifo.h"
#include "../systems/mpeg.h"
#include "../systems/systems.h"
#include "../common/alloc.h"
#include "../common/log.h"
#include "../common/remote.h"
#include "../options.h"
#include "vbi.h"

static int		lines;
static int		vps_offset;
static bool		do_pdc, do_subtitles;
static struct decode_rec vpsd, ttxd;
static unsigned char	buf[64];
static fifo *		vbi_output_fifo;

extern int		video_num_frames;

/*
 *  ETS 300 706 -- Enhanced Teletext specification
 */

static int
decode_ttx(unsigned char *p, unsigned char *buf, int line)
{
	int r = 0;
	int packet_address;
	int magazine, packet;
	int designation;

	if ((packet_address = unham84(buf + 0)) < 0)
		return 0; /* hamming error */

	magazine = packet_address & 7;
	packet = packet_address >> 3;

	if (do_subtitles)
		r = dvb_packet_filter(p, buf, line, magazine, packet);

	if (do_pdc) {
		if (magazine != (8 & 7) || packet != 30)
			return r;

		designation = hamming84[buf[2]]; 

		if (designation <= 1)
			return r; /* hamming error or packet 8/30/1 */

		if (designation <= 3) /* packet 8/30/2 */
			decode_pdc(buf);
	}

	return r;
}

void *
vbi_thread(void *f) // XXX
{
	struct vbi_context *vbi = ((fifo *) f)->user_data;
	buffer *ibuf, *obuf;
	unsigned char *p, *p1;
	int vbi_frame_count = 0;

	if (do_subtitles)
		remote_sync(&vbi->fifo, MOD_SUBTITLES, 1 / 25.0);

	while (vbi_frame_count < video_num_frames) { // XXX video XXX pdc
		if (!(ibuf = wait_full_buffer(&vbi->fifo)))
			break; // EOF

		if (do_subtitles && remote_break(ibuf->time, 1 / 25.0)) {
			send_empty_buffer(&vbi->fifo, ibuf);
			break;
		}

		vbi_frame_count++;

		if (do_pdc && decode_nrz(&vpsd, ibuf->data + vps_offset, buf))
			decode_vps(buf);

		if (do_subtitles) {
			int i;

			obuf = wait_empty_buffer(vbi_output_fifo);
			p1 = p = obuf->data;

			if (vbi->interlaced) {
				for (i = 0; i < (vbi->count[0] * 2 + 0); i += 2)
					if (decode_nrz(&ttxd, ibuf->data + i * vbi->samples_per_line, buf))
						p += decode_ttx(p, buf, i);

				if (p == p1) {
					memcpy(p, stuffing_packet[0], 46);
					p1 = p += 46;
				}

				for (i = 1; i < (vbi->count[0] * 2 + 1); i += 2)
					if (decode_nrz(&ttxd, ibuf->data + i * vbi->samples_per_line, buf))
						p += decode_ttx(p, buf, i);
			} else {
				// Top field
				for (i = 0; i < vbi->count[0]; i++)
					if (decode_nrz(&ttxd, ibuf->data + i * vbi->samples_per_line, buf))
						p += decode_ttx(p, buf, i);

				if (p == p1) {
					memcpy(p, stuffing_packet[0], 46);
					p1 = p += 46;
				}

				// Bottom field
				for (; i < lines; i++)
					if (decode_nrz(&ttxd, ibuf->data + i * vbi->samples_per_line, buf))
						p += decode_ttx(p, buf, i);
			}

			if (p == p1) {
				memcpy(p, stuffing_packet[1], 46);
				p += 46;
			}

			obuf->time = ibuf->time;
			obuf->used = p - obuf->data;

			send_full_buffer(vbi_output_fifo, obuf);
		}
		else if (do_pdc)
		{
			int i;

			if (vbi->interlaced) {
				for (i = 0; i < (vbi->count[0] * 2 + 0); i += 2)
					if (decode_nrz(&ttxd, ibuf->data + i * vbi->samples_per_line, buf))
						decode_ttx(NULL, buf, i);

				for (i = 1; i < (vbi->count[0] * 2 + 1); i += 2)
					if (decode_nrz(&ttxd, ibuf->data + i * vbi->samples_per_line, buf))
						decode_ttx(NULL, buf, i);
			} else
				for (i = 0; i < lines; i++)
					if (decode_nrz(&ttxd, ibuf->data + i * vbi->samples_per_line, buf))
						decode_ttx(NULL, buf, i);
		}

		send_empty_buffer(&vbi->fifo, ibuf);
	}

	printv(2, "VBI: End of file\n");

	if (do_subtitles)
		for (;;) {
			obuf = wait_empty_buffer(vbi_output_fifo);
			obuf->used = 0;
			send_full_buffer(vbi_output_fifo, obuf);
		}

	return NULL;
}

void
vbi_init(fifo *f)
{
	struct vbi_context *vbi = f->user_data;

	lines = vbi->count[0] + vbi->count[1];

	vps_offset = ((16 - (vbi->start[0] + 1)) << vbi->interlaced) * vbi->samples_per_line;

	init_decoder(&vpsd, vbi->samples_per_line, vbi->sampling_rate,
		5000000, 0x99515555, 0xFFFFFF00, 26);

	init_decoder(&ttxd, vbi->samples_per_line, vbi->sampling_rate,
		6937500, 0x27555500, 0xFFFF0000, 42);

	do_pdc = FALSE;
//	do_pdc = TRUE;

	do_subtitles = FALSE;

	if (subtitle_pages != NULL) {
		do_subtitles = TRUE;

		init_dvb_packet_filter(vbi, subtitle_pages);

		vbi_output_fifo = mux_add_input_stream(PRIVATE_STREAM_1,
			32 * 46, 5, 25.0, 294400 /* peak */, &vbi->fifo);
	}

	if (!do_pdc && !do_subtitles)
		FAIL("Bug: redundant vbi_init");
}
