/*
 *  MPEG-1 Real Time Encoder
 *
 *  Copyright (C) 1999-2001 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 2.
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

/* $Id: vbi.c,v 1.3 2001-08-15 23:16:16 mschimek Exp $ */

#include "site_def.h"

#include "../common/fifo.h"
#include "../systems/mpeg.h"
#include "../systems/systems.h"
#include "../common/alloc.h"
#include "../common/log.h"
#include "../common/sync.h"
#include "../options.h"
#include "vbi.h"
#include "tables.h"
#include "decoder.h"
#include "hamm.h"

#ifndef DO_PDC
#define DO_PDC 0
#endif

static bool		do_pdc, do_subtitles;
static fifo *		vbi_output_fifo;
static producer         vbi_prod;

extern int		video_num_frames;

/*
 *  ETS 300 706 -- Enhanced Teletext specification
 */
static inline int
teletext_packet(unsigned char *p, unsigned char *buf, int line)
{
	int r = 0;
	int mag0, packet;
	int designation;

	if ((packet = hamm16(buf)) < 0)
		return 0; /* hamming error */

	mag0 = packet & 7;
	packet >>= 3;

	if (do_subtitles)
		r = dvb_teletext_packet_filter(p, buf, line, mag0, packet);

	if (do_pdc) {
		if (mag0 != 0 /* 8 */ || packet != 30)
			return r;

		designation = hamm8(buf[2]);

		if (designation <= 1)
			return r; /* hamming error or packet 8/30/1 */

		if (designation <= 3) /* packet 8/30/2 */
			decode_pdc(buf);
	}

	return r;
}

#define SLICED_TELETEXT_B	(SLICED_TELETEXT_B_L10_625 | SLICED_TELETEXT_B_L25_625)
#define SLICED_CAPTION		(SLICED_CAPTION_625_F1 | SLICED_CAPTION_625 \
				 | SLICED_CAPTION_525_F1 | SLICED_CAPTION_525)

void *
vbi_thread(void *F)
{
	consumer cons;
	buffer *obuf = NULL, *ibuf;
	unsigned char *p = NULL, *p1 = NULL;
	int vbi_frame_count = 0;
	int items, parity = -1;
	vbi_sliced *s;

	ASSERT("add vbi cons", add_consumer((fifo *) F, &cons));

	if (do_subtitles)
		sync_sync(&cons, MOD_SUBTITLES, 1 / 25.0);

	while (vbi_frame_count < video_num_frames) { // XXX video XXX pdc
		if (!(ibuf = wait_full_buffer(&cons)) || ibuf->used <= 0)
			break; // EOF or error

		if (do_subtitles) {
			if (sync_break(MOD_SUBTITLES, ibuf->time, 1 / 25.0)) {
				send_empty_buffer(&cons, ibuf);
				break;
			}

			obuf = wait_empty_buffer(&vbi_prod);
			p1 = p = obuf->data;

			parity = 0;
		}

		vbi_frame_count++;
// XXX frame dropping not handled

		s = (vbi_sliced *) ibuf->data;
		items = ibuf->used / sizeof(vbi_sliced);

		while (items) {
			if ((do_pdc | do_subtitles) && (s->id & SLICED_TELETEXT_B)) {
				p += teletext_packet(p, s->data, s->line);

				if (s->line > 32 && parity == 0) {
					parity = 1;

					if (p == p1) {
						memcpy(p, stuffing_packet[0], 46);
						p1 = p += 46;
					}
				}
			} else if (do_pdc && (s->id & SLICED_VPS))
				decode_vps(s->data);

			s++;
			items--;
		}

		if (do_subtitles) {
			if (p == p1) {
				memcpy(p, stuffing_packet[1], 46);
				p += 46;
			}

			obuf->time = ibuf->time;
			obuf->used = p - obuf->data;

			send_full_buffer(&vbi_prod, obuf);
		}

		send_empty_buffer(&cons, ibuf);
	}

	printv(2, "VBI: End of file\n");

	if (do_subtitles)
		for (;;) {
			obuf = wait_empty_buffer(&vbi_prod);
			obuf->used = 0;
			send_full_buffer(&vbi_prod, obuf);
		}

	rem_consumer(&cons);

	return NULL;
}

void
vbi_init(fifo *f, multiplexer *mux)
{
	do_pdc = DO_PDC;

	do_subtitles = FALSE;

	if (subtitle_pages != NULL) {
		do_subtitles = TRUE;

		init_dvb_packet_filter(subtitle_pages);

		vbi_output_fifo = mux_add_input_stream(mux,
			PRIVATE_STREAM_1, "vbi-ps1",
			32 * 46, 5, 25.0, 294400 /* peak */);

		add_producer(vbi_output_fifo, &vbi_prod);
	}

	if (!do_pdc && !do_subtitles)
		FAIL("Bug: redundant vbi_init");
}
