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

/* $Id: subtitles.c,v 1.1 2000-09-29 17:54:33 mschimek Exp $ */

#include <ctype.h>
#include "vbi.h"

/*
 *  ETS 300 472 -- Specification for conveying
 *  ITU-R System B Teletext in DVB bitstreams
 */

#define DATA_UNIT_EBU_TELETEXT_NON_SUBTITLE	0x02
#define DATA_UNIT_EBU_TELETEXT_SUBTITLE		0x03
#define DATA_UNIT_USER_DEFINED			0x80 // 0x80 .. 0xFE
#define DATA_UNIT_STUFFING			0xFF

static struct {
	short	mask;
	short	page;
} *page_list;

static int magazine_set;
static int next_packet[8];
static unsigned char fplo_lut[32];

unsigned char stuffing_packet[2][46];

int
init_dvb_packet_filter(char *s)
{
	int i, j, max;
	int page, mask;

	memset(next_packet, sizeof(next_packet), 0);

	/* 7 (6) ... 22, 320 (318) ... 335 */

	if ((vbi_para.start[0] >= 0 && vbi_para.start[0] + vbi_para.count[0] > 22) ||
	    (vbi_para.start[1] >= 0 && vbi_para.start[1] + vbi_para.count[0] > 335) ||
	    (vbi_para.count[0] + vbi_para.count[1]) > 32)
		return 0; // XXX relax

	for (i = 0; i < vbi_para.count[0]; i++)
		fplo_lut[i] = (3 << 6) + (1 << 5) + ((vbi_para.start[0] < 0) ? 0 : vbi_para.start[0] + 1 + i);

	for (i = 0; i < vbi_para.count[1]; i++)
		fplo_lut[i + vbi_para.count[0]] = (3 << 6) + (0 << 5)
			+ ((vbi_para.start[1] < 0) ? 0 : vbi_para.start[1] + 1 + i - 313);

	memset(stuffing_packet, 0, sizeof(stuffing_packet));

	for (i = 0; i < 2; i++) {
		/* Update field counter between PES packet PTS updates */

		stuffing_packet[i][0] = DATA_UNIT_EBU_TELETEXT_NON_SUBTITLE;
		stuffing_packet[i][1] = 44;
		stuffing_packet[i][2] = (3 << 6) + ((i ^ 1) << 5) + 0;
		stuffing_packet[i][3] = 0xE4; // bit_reverse[0x27]
		stuffing_packet[i][4] = 0xE2; // packet 7/31 (private)
		stuffing_packet[i][5] = 0xE2;

		// strcpy(&stuffing_packet[i][6], PACKAGE " " VERSION);
	}

	magazine_set = 0;

	if (page_list)
		free(page_list);
	if (!(page_list = malloc((max = 8) * sizeof(page_list[0]))))
		return 0;

	for (i = 0;; i++) {
		page_list[i].mask = 0x000;

		while (isspace(*s))
			s++;
	    	if (!*s)
			break;

		for (page = mask = j = 0; j < 3; s++, j++) {
			char c = *s;

			page <<= 4;
			mask <<= 4;

			if (isxdigit(c)) {
				page += (c & 0xF);
				if (c > '9')
					page += 9;
				if (j == 0 && (page < 1 || page > 8))
					return 0;
				mask += 0xF;
			} else if (c != '?')
				return 0;
		}

		if (i >= max - 1) {
			void *p = realloc(page_list, (max += 8) * sizeof(page_list[0]));

			if (!p) {
				free(page_list);
				return 0;
			}

			page_list = p;
		}

		page_list[i].mask = mask & 0x7FF;
		page_list[i].page = page &= 0x7FF;

		if ((mask >> 8) != 0xF)
			magazine_set = 0xFF;
		else
			magazine_set |= 1 << (page >> 8);
	}

	return 1;
}

int
dvb_packet_filter(unsigned char *p, unsigned char *buf,
	int line, int magazine, int packet)
{
	int page, i;

	if (page_list[0].mask == 0x000)
		goto encode_packet;

	if (packet == 0) {
		if ((page = unham84(buf + 2)) < 0)
			return 0;

		next_packet[magazine] = 0;

		if (page == 0xFF && (magazine_set & (1 << magazine)))
			goto encode_packet;

		page += magazine << 8;

		for (i = 0; page_list[i].mask; i++)
			if ((page & page_list[i].mask) == page_list[i].page) {
				next_packet[magazine] = 1;
				goto encode_packet;
			}
	} else if (packet <= 23 && next_packet[magazine] > 0) {
		if (packet < next_packet[magazine])
			next_packet[magazine] = 0;
		else {
			next_packet[magazine]++;
			goto encode_packet;
		}
	} else if (packet <= 28 && next_packet[magazine] > 0)
		goto encode_packet;
	else if (packet == 29 && (magazine_set & (1 << magazine)))
		goto encode_packet;

	return 0;

encode_packet:
#if 0
	for (i = 0; i < 42; i++) {
		char c = buf[i] & 0x7F;
		if (c < 0x20 || c == 0x7F) c = '.';
			fputc(c, stderr);
	}

	fputc('\n', stderr);
#endif
#if 1
	p[0] = DATA_UNIT_EBU_TELETEXT_NON_SUBTITLE; // ?
	p[1] = 44;
	p[2] = fplo_lut[line];
	p[3] = 0xE4; // bit_reverse[0x27]
	for (i = 0; i < 42; i++)
		p[4 + i] = bit_reverse[buf[i]];
	/*
	 *  data_unit_id [8], data_unit_length [8],
	 *  reserved[2], field_parity, line_offset [5],
	 *  framing_code [8], magazine_and_packet_address [16],
	 *  data_block [320]
	 */
#endif

	return 46;
}
