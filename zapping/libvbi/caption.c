/*
 *  Zapzilla - Closed Caption decoder
 *
 *  gcc -g -ocaption caption.c -L/usr/X11R6/lib -lX11 -DTEST=1
 *
 *  Copyright (C) 2000-2001 Michael H. Schimek
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

/* $Id: caption.c,v 1.20 2001-03-17 07:44:29 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>

#include <unistd.h>
#include <string.h>
#include <assert.h>

#include "vbi.h"

#if TEST

#include "hamm.c"
#include "tables.c"
#include "lang.c"

#define XDS_DEBUG 1
#define ITV_DISABLE 0

#else

#include "hamm.h"
#include "tables.h"
#include "lang.h"

#define XDS_DEBUG 0
#define ITV_DEBUG 0

#endif

#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#include "cc.h"

/*
 *  XDS (Extended Data Service)
 */

#define XDS_CURRENT		0
#define XDS_FUTURE		1
#define XDS_CHANNEL		2
#define XDS_MISC		3
#define XDS_PUBLIC_SERVICE	4
#define XDS_RESERVED		5
#define XDS_UNDEFINED		6	/* proprietary format */

#define XDS_END			15

#if XDS_DEBUG

static char *mpaa_rating[8]	= { "n/a", "G", "PG", "PG-13", "R", "NC-17", "X", "not rated" };
static char *us_tv_rating[8]	= { "not rated", "TV-Y", "TV-Y7", "TV-G", "TV-PG", "TV-14", "TV-MA", "not rated" };
static char *cdn_en_rating[8]	= { "exempt", "C", "C8+", "G", "PG", "14+", "18+", "-" };
static char *cdn_fr_rating[8]	= { "exempt", "G", "8 ans +", "13 ans +", "16 ans +", "18 ans +", "-", "-" };
static char *map_type[8]	= { "unknown", "mono", "simulated stereo", "stereo", "stereo surround", "data service", "unknown", "none" };
static char *sap_type[8]	= { "unknown", "mono", "video descriptions", "non-program audio", "special effects", "data service", "unknown", "none" };
static char *language[8]	= { "unknown", "english", "spanish", "french", "german", "italian", "unknown", "none" };

static char *
cgmsa[4] = {
	"copying permitted",
	"-",
	"one generation copy allowed",
	"no copying permitted"
};

static char *
scrambling[4] = {
	"no pseudo-sync pulse",
	"pseudo-sync pulse on; color striping off",
	"pseudo-sync pulse on; 2-line color striping on",
	"pseudo-sync pulse on; 4-line color striping on"
};

static const char *
month_names[] = {
	"0?", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
	"Sep", "Oct", "Nov", "Dec", "13?", "14?", "15?"
};

static const char *
day_names[] = {
	"0?", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

#endif /* XDS_DEBUG */

static uint32_t hcrc[128];

static void init_hcrc(void) __attribute__ ((constructor));

static void
init_hcrc(void)
{
	unsigned int sum;
	int i, j;

	for (i = 0; i < 128; i++) {
		sum = 0;
		for (j = 7 - 1; j >= 0; j--)
			if (i & (1 << j))
				sum ^= 0x48000000L >> j;
		hcrc[i] = sum;
	}
}

static int
xds_strfu(char *d, char *s, int len)
{
	int c, neq = 0;

	for (; len > 0 && *s <= 0x20; s++, len--);

	for (; len > 0; s++, len--) {
		c = MAX(0x20, *s);
		neq |= *d ^ c;
		*d++ = c;
	}

	neq |= *d;
	*d = 0;

	return neq;
}

static inline void
xds_decoder(struct vbi *vbi, int class, int type, char *buffer, int length)
{
	int i __attribute__ ((unused));

	// assert(length > 0 && length < 32);

	switch (class) {
	case XDS_CURRENT:
	case XDS_FUTURE:
#if XDS_DEBUG
		if (class == XDS_CURRENT)
			printf("Current ");
		else
			printf("Next ");

		switch (type) {
		case 1:		/* program identification number */
			if (length != 4)
				return;
			printf("PIN: %d %s %02d:%02d UTC, D=%d L=%d Z=%d T(ape delayed)=%d\n",
				buffer[2] & 31, month_names[buffer[3] & 15],
				buffer[1] & 31, buffer[0] & 63,
				!!(buffer[1] & 0x20),
				!!(buffer[2] & 0x20),
				!!(buffer[3] & 0x20),
				!!(buffer[3] & 0x10));
			break;

		case 2:		/* program length */
			if (length > 5)
				return;
			printf("length: %02d:%02d",
				buffer[1] & 63, buffer[0] & 63);
			if (length >= 4)
				printf(", elapsed: %02d:%02d",
					buffer[3] & 63, buffer[2] & 63);
			if (length >= 5)
				printf(":%02d", buffer[4] & 63);
			printf("\n");
			break;

		case 3:		/* program name */
			printf("program title: '");
			for (i = 0; i < length; i++)
				putchar(printable(buffer[i]));
			printf("'\n");
			break;

		case 4:		/* program type */
			printf("program type: ");
			for (i = 0; i < length; i++)
				printf((i > 0) ? ", %s" : "%s",
					eia608_program_type[buffer[i] - 0x20]);
			printf("\n");
			break;

		case 5:		/* program rating */
			printf("program movie rating: %s, tv rating: ",
				mpaa_rating[buffer[0] & 7]);
			if (buffer[0] & 0x10) {
				if (buffer[0] & 0x20)
					puts(cdn_fr_rating[buffer[1] & 7]);
				else
					puts(cdn_en_rating[buffer[1] & 7]);
			} else {
				printf("%s; ", us_tv_rating[buffer[1] & 7]);
				if (buffer[1] & 0x20)
					printf("violence; ");
				if (buffer[1] & 0x10)
					printf("sexual situations; ");
				if (buffer[1] & 8)
					printf("adult language; ");
				if (buffer[0] & 0x20)
					printf("sexually suggestive dialog");
				putchar('\n');
			}
			break;

		case 6:		/* program audio services */
			if (length != 2)
				return;
			printf("main audio: %s, %s; second audio program: %s, %s\n",
				map_type[buffer[0] & 7], language[(buffer[0] >> 3) & 7],
				sap_type[buffer[1] & 7], language[(buffer[1] >> 3) & 7]);
			break;

		case 7:		/* program caption services */
			if (length > 8)
				return;
			printf("program caption services:\n");
			for (i = 0; i < length; i++)
				printf("Line %3d, channel %d, %s: %s\n",
					(buffer[i] & 4) ? 284 : 21,
					(buffer[i] & 2) ? 2 : 1,
					(buffer[i] & 1) ? "text      " : "captioning",
					language[(buffer[i] >> 3) & 7]);
			break;

		case 8:		/* copy generation management system */
			if (length != 1)
				return;
			printf("CGMS: %s", cgmsa[(buffer[0] >> 3) & 3]);
			if (buffer[0] & 0x18)
				printf("; %s", scrambling[(buffer[0] >> 1) & 3]);
			printf("; analog source: %d", buffer[0] & 1);
			break;

		case 9:		/* program aspect ratio */
			if (length > 3)
				return;
			printf("program aspect ratio info: active start %d, end %d%s\n",
				(buffer[0] & 63) + 22, 262 - (buffer[1] & 63),
				(length >= 3 && (buffer[2] & 1)) ? " (anamorphic)" : "");
			break;

		case 0x10 ... 0x17: /* program description */
			printf("program descr. line %d: >", type - 0x10 + 1);
			for (i = 0; i < length; i++)
				putchar(printable(buffer[i]));
			printf("<\n");
			break;

		default:
			printf("<unknown %d/%02x length %d>\n", class, type, length);
			break;
		}

#endif /* XDS_DEBUG */

		break;

	case XDS_CHANNEL:
		switch (type) {
		case 1:		/* network name */
			if (xds_strfu(vbi->network.name, buffer, length)) {
				vbi->network.cycle = 1;
			} else if (vbi->network.cycle == 1) {
				char *s = vbi->network.name;
				uint32_t sum;
				vbi_event ev;

				if (vbi->network.call[0])
					s = vbi->network.call;

				for (sum = 0; *s; s++)
					sum = (sum >> 7) ^ hcrc[(sum ^ *s) & 0x7F];

				vbi->network.id = sum & ((1UL << 31) - 1);

				ev.type = VBI_EVENT_NETWORK;
				ev.p1 = &vbi->network;
				vbi_send_event(vbi, &ev);

				vbi->network.cycle = 3;
			}
#if XDS_DEBUG
			printf("Network name: '");
			for (i = 0; i < length; i++)
				putchar(printable(buffer[i]));
			printf("'\n");
#endif
			break;

		case 2:		/* network call letters */
			if (xds_strfu(vbi->network.call, buffer, length)) {
				if (vbi->network.cycle != 1) {
					vbi->network.name[0] = 0;
					vbi->network.cycle = 0;
				}
			}
#if XDS_DEBUG
			printf("Network call letters: '");
			for (i = 0; i < length; i++)
				putchar(printable(buffer[i]));
			printf("'\n");
#endif
			break;

		case 3:		/* channel tape delay */
			if (length != 2)
				return;

			vbi->network.tape_delay =
				(buffer[1] & 31) * 60 + (buffer[0] & 63);
#if XDS_DEBUG
			printf("Channel tape delay: %02d:%02d",
				buffer[1] & 31, buffer[0] & 63);
#endif
			break;

		default:
			printf("<unknown %d/%02x length %d>\n", class, type, length);
			break;
		}

		break;

	case XDS_MISC:
#if XDS_DEBUG
		switch (type) {
		case 1:		/* time of day */
			if (length != 6)
				return;
			printf("Time of day (UTC): %s, %d %s %d ",
				day_names[buffer[4] & 7],
				buffer[2] & 31, month_names[buffer[3] & 15],
				1990 + (buffer[5] & 63));
			printf("%02d:%02d ", buffer[1] & 31, buffer[0] & 63);
			printf("D(ST)=%d, L(eap day)=%d, "
				"(Second )Z(ero)=%d, T(ape delayed)=%d\n",
				!!(buffer[1] & 0x20),
				!!(buffer[2] & 0x20),
				!!(buffer[3] & 0x20),
				!!(buffer[3] & 0x10));
			break;

		case 2:		/* impulse capture id */
			if (length != 6)
				return;
			printf("Impulse capture id: %d %s ",
				buffer[2] & 31, month_names[buffer[3] & 15]);
			printf("%02d:%02d ", buffer[1] & 31, buffer[0] & 63);
			printf("length %02d:%02d ",
				buffer[5] & 63, buffer[4] & 63);
			printf("D=%d, L=%d, Z=%d, T(ape delayed)=%d\n",
				!!(buffer[1] & 0x20),
				!!(buffer[2] & 0x20),
				!!(buffer[3] & 0x20),
				!!(buffer[3] & 0x10));
			break;

		case 3:		/* supplemental data location */
			for (i = 0; i < length; i++)
				printf("Supplemental data: field %d, line %d\n",
					!!(buffer[i] & 0x20), buffer[i] & 31);
			break;

		case 4:		/* local time zone */
			if (length != 1)
				return;
			printf("Local time zone: UTC + %d h; D(ST)=%d\n",
				buffer[0] & 31, !!(buffer[0] & 0x20));
			break;

		case 0x40:	/* out-of-band channel number */
			if (length != 2)
				return;
			i = (buffer[0] & 63) | ((buffer[1] & 63) << 6);
			printf("Out-of-band channel %d -- ?\n", i);
			break;

		default:
			printf("<unknown %d/%02x length %d>\n", class, type, length);
			break;
		}

#endif /* XDS_DEBUG */

		break;

	default:
#if XDS_DEBUG
		printf("<unknown %d/%02x length %d>\n", class, type, length);
#endif
		break;
	}
}

static void
xds_separator(struct vbi *vbi, unsigned char *buf)
{
	struct caption *cc = &vbi->cc;
	xds_sub_packet *sp = cc->curr_sp;
	int c1 = parity(buf[0]);
	int c2 = parity(buf[1]);
	int class, type;

//	printf("XDS %02x %02x\n", buf[0], buf[1]);

	if ((c1 | c2) < 0) {
//		printf("XDS tx error, discard current packet\n");

		if (sp) {
			sp->count = 0;
			sp->chksum = 0;
			sp = NULL;
		}

		return;
	}

	switch (c1) {
	case 1 ... 14:
		class = (c1 - 1) >> 1;

		if (class > sizeof(cc->sub_packet) / sizeof(cc->sub_packet[0])
		    || c2 > sizeof(cc->sub_packet[0]) / sizeof(cc->sub_packet[0][0]))
		{
//			printf("XDS ignore packet %d/0x%02x\n", class, c2);
			cc->curr_sp = NULL;
			return;
		}

		cc->curr_sp = sp = &cc->sub_packet[class][c2];

		if (c1 & 1) { /* start */
			sp->chksum = c1 + c2;
			sp->count = 2;
		} else {
			if (!sp->count) {
//				printf("XDS can't continue %d/0x%02x\n", class, c2);
				cc->curr_sp = NULL;
				return;
			}
		}

		return;

	case 15:
		if (!sp)
			return;

		sp->chksum += c1 + c2;

		class = (sp - cc->sub_packet[0]) / (sizeof(cc->sub_packet[0]) / sizeof(cc->sub_packet[0][0]));
		type = (sp - cc->sub_packet[0]) % (sizeof(cc->sub_packet[0]) / sizeof(cc->sub_packet[0][0]));

		if (sp->chksum & 0x7F) {
//			printf("XDS ignore packet %d/0x%02x, checksum error\n", class, type);
		} else if (sp->count <= 2) {
//			printf("XDS ignore empty packet %d/0x%02x\n", class, type);
		} else {
			xds_decoder(vbi, class, type, sp->buffer, sp->count - 2);
/*
	for (i = 0; i < sp->count - 2; i++)
		printf("%c", printable(sp->buffer[i]));
	printf(" %d/0x%02x\n", class, type);
*/
		}

		sp->count = 0;
		sp->chksum = 0;
		cc->curr_sp = NULL;

		return;

	case 0x20 ... 0x7F:
		if (!sp)
			return;

		if (sp->count >= 32 + 2) {
//			printf("XDS packet length overflow, discard %d/0x%02x\n",
//				(sp - cc->sub_packet[0]) / sizeof(cc->sub_packet[0]),
//				(sp - cc->sub_packet[0]) % sizeof(cc->sub_packet[0][0]));

			sp->count = 0;
			sp->chksum = 0;
			cc->curr_sp = NULL;
			return;
		}

		sp->buffer[sp->count - 2] = c1;
		sp->buffer[sp->count - 1] = c2;
		sp->chksum += c1 + c2;
		sp->count += 1 + !!c2;

		return;

	default:
		assert(0);
	}
}

/*
 *  ITV (Interactive TV Link aka. WebTV)
 *
 *  http://developer.webtv.net
 */

static char *
itv_key[] = { "program", "network", "station", "sponsor", "operator", NULL };

static inline int
itv_chksum(char *s, unsigned int sum)
{
	int i = (strlen(s) + 1) >> 1;

	for (; i > 0; i--) {
		sum += *s++ << 8;
		sum += *s++;
		if (sum >= 0x10000)
			sum += 1 - 0x10000;
	}

	return sum == 0xFFFF;
}

static void
itv_decoder(struct caption *cc, char *s1)
{
#if ITV_DEBUG
	char *s, *e;
	char *d, ripped[sizeof(cc->itv_buf)];
	int type = -1, view = 'w';
	char *url = NULL, *name = NULL;
	char *script = NULL, *expires = "29991231T235959";

	for (s = s1, d = ripped;; s++) {
		e = s;

		if (*s == '<') {
			for (url = d, ++s; *s != '>'; s++)
				if (*s)
					*d++ = *s;
				else
					return;
			*d++ = 0;
		} else if (*s == '[') {
			char *attr;
			int i, quote;

			for (attr = d, ++s; *s != ':' && *s != ']'; s++)
				if (*s)
					*d++ = *s;
				else
					return;
			*d++ = 0;

			if (*s == ']') {
				for (i = 0; itv_key[i]; i++)
					if (!strcmp(itv_key[i], attr))
						break;

				if (itv_key[i]) {
					type = i;
					continue;
				}

				*e = 0;

				if (s[1] || !itv_chksum(s1, strtoul(attr, NULL, 16)))
					return;

				break;
			}

			s++;

			switch (*attr) {
			case 't':
				for (i = 0; itv_key[i]; i++)
					if (*s == itv_key[i][0]
					    || !strcmp(itv_key[i], attr))
						break;

				if (!itv_key[i])
					return;

				type = i;

				break;

			case 'v':
				view = *s;
				break;

			case 'n':
				name = d;
				break;

			case 's':
				script = d;
				break;

			case 'e':
				expires = d;
				break;
			}

			for (quote = 0; quote || *s != ']'; s++) {
				if (!*s)
					return;
				if (*s == '"')
					quote ^= 1;
				*d++ = *s;
			}

			*d++ = 0;
		} else
			return;
	}

	printf("<i> %s\n URL: %s\n Type: ", name, url);
	switch (type) {
	case -1: printf("not given"); break;
	case 0:	 printf("program related"); break;
	case 1:	 printf("network related"); break;
	case 2:	 printf("station related"); break;
	case 3:	 printf("sponsor"); break;
	case 4:	 printf("operator"); break;
	default: printf("unknown"); break;
	}
	printf("; script: %s\n Cached page expires: %s; view: ",
		script ? script : "none", expires);
	switch (view) {
	case 'w': printf("conventional web page"); break;
	case 't': printf("tv related, WebTV style"); break;
	default:  printf("unknown");
	}
	putchar('\n');

#endif /* ITV_DEBUG */

}

static void
itv_separator(struct caption *cc, char c)
{
	if (c >= 0x20) {
		if (c == '<') // s4nbc omitted CR
			itv_separator(cc, 0);
		else if (cc->itv_count > sizeof(cc->itv_buf) - 2)
			cc->itv_count = 0;

		cc->itv_buf[cc->itv_count++] = c;

		return;
	}

	cc->itv_buf[cc->itv_count] = 0;
	cc->itv_count = 0;

	itv_decoder(cc,  cc->itv_buf);
}

/* Caption */

#include "format.h"

#define ROWS			15
#define COLUMNS			34

/* Mostly obsolete - now cc_event & pg.dirty */

/*
 *  Render <row> 0 ... 14 or -1 all rows, from <pg->text> if you're
 *  monitoring <pg->pgno>, which is CC_PAGE_BASE +
 *
 *  0:	"Caption 1" "Primary synchronous caption service [English]"
 *  1:  "Caption 2" "Special non-synchronous data that is intended to
 *                   augment information carried in the program"
 *  2:  "Caption 3" "Secondary synchronous caption service, usually
 *                   second language [Spanish, French]"
 *  3:  "Caption 4" "Special non-synchronous data similar to Caption 2"
 *  4:  "Text 1"    "First text service, data usually not program
 *                   related"
 *  5:  "Text 2"    "Second text service, additional data usually
 *                   not program related (ITV)"
 *  6:  "Text 3"    "Third and forth text services, to be used only
 *  7:  "Text 4"     if Text 1 and Text 2 are not sufficient"
 *
 *  Remember unscaled image or you'll have to redraw all if row != -1;
 *  Don't dereference <buffer> pointer after returning.
 *  (XXX may need a change if we want smooth scrolling in.)
 *
 *  The decoder doesn't filter channels, all 'pages' in cache are live
 *  cf. WST. Exception POP_ON mode because we only cache the hidden
 *  buffer we're writing while the unscaled image is our visible buffer.
 *  render() is intended to be a callback (function pointer), we can
 *  export a fetch() function calling render() in turn to update the
 *  screen immediately after a switch.
 */
static void render(struct fmt_page *pg, int row);

/*
 *  Another render() shortcut, set all cols and rows to
 *  TRANSPARENT_SPACE. Added for Erase Displayed Memory in POP_ON
 *  mode because we don't have the buffer to render() and erasing
 *  all data at once will be faster than scanning the buffer anyway.
 */
static void clear(struct fmt_page *pg);

/*
 *  Start soft scrolling, move <first_row> + 1 ... <last_row> (inclusive)
 *  up to <first_row>. Rows numbered 0 ... 14. Feel free to scroll a row
 *  at once, render from the buffer, move unscaled image data, window
 *  contents, windows, whatever. first_row is provided in buffer to render,
 *  the decoder will roll <pg->text> *after* returning and continue writing
 *  in last_row or another. Don't dereference <pg> pointer after
 *  returning. Soft scrolling finished or not, render() can be called
 *  any time later, any row, so prepare.
 */
static void roll_up(struct fmt_page *pg, int first_row, int last_row);





static inline void
update(channel *ch)
{
	attr_char *acp = ch->line - ch->pg[0].text + ch->pg[1].text;

	memcpy(acp, ch->line, sizeof(*acp) * COLUMNS);
}

static void
word_break(struct caption *cc, channel *ch, int upd)
{
	/*
	 *  Add a leading and trailing space.
	 */
	if (ch->col > ch->col1) {
		attr_char c = ch->line[ch->col1];

		if ((c.glyph & 0x7F) != 0x20
		    && ch->line[ch->col1 - 1].opacity == TRANSPARENT_SPACE) {
			c.glyph = 0x20;
			ch->line[ch->col1 - 1] = c;
		}

		c = ch->line[ch->col - 1];

		if ((c.glyph & 0x7F) != 0x20
		    && ch->line[ch->col].opacity == TRANSPARENT_SPACE) {
			c.glyph = 0x20;
			ch->line[ch->col] = c;
		}
	}

	if (!upd || ch->mode == MODE_POP_ON)
		return;

	/*
	 *  NB we render only at spaces (end of word) and
	 *  before cursor motions and mode switching, to keep the
	 *  drawing efforts (scaling etc) at a minimum. update()
	 *  for double buffering at word granularity.
	 *
	 *  XXX should not render if space follows space,
	 *  but force in long words. 
	 */

	update(ch);
	render(ch->pg + 1, ch->row);
}

static inline void
set_cursor(channel *ch, int col, int row)
{
	ch->col = ch->col1 = col;
	ch->row = row;

	ch->line = ch->pg[ch->hidden].text + row * COLUMNS;
}

static void
put_char(struct caption *cc, channel *ch, attr_char c)
{
	// c.foreground = rand() & 7;
	// c.background = rand() & 7;

	if (ch->col < COLUMNS - 1)
		ch->line[ch->col++] = c;
	else {
		/* line break here? */

		ch->line[COLUMNS - 2] = c;
	}

	if ((c.glyph & 0x7F) == 0x20)
		word_break(cc, ch, 1);
}

static inline channel *
switch_channel(struct caption *cc, channel *ch, int new_chan)
{
	word_break(cc, ch, 1); // we leave for a number of frames

	return &cc->channel[cc->curr_chan = new_chan];
}

static void
erase_memory(struct caption *cc, channel *ch, int page)
{
	struct fmt_page *pg = ch->pg + page;
	attr_char *acp = pg->text;
	attr_char c = cc->transp_space[ch >= &cc->channel[4]];
	int i;

	for (i = 0; i < COLUMNS * ROWS; acp++, i++)
		*acp = c;

	pg->dirty.y0 = 0;
	pg->dirty.y1 = ROWS - 1;
	pg->dirty.roll = ROWS;
}

static const attr_colours
palette_mapping[8] = {
	WHITE, GREEN, BLUE, CYAN, RED, YELLOW, MAGENTA, BLACK
};

static int
row_mapping[] = {
	10, -1,  0, 1, 2, 3,  11, 12, 13, 14,  4, 5, 6, 7, 8, 9
};

// not verified means I didn't encounter the code in a
// sample stream yet

static inline void
caption_command(struct caption *cc,
	unsigned char c1, unsigned char c2, bool field2)
{
	channel *ch;
	int chan, col, i;
	int last_row;

	chan = (cc->curr_chan & 4) + field2 * 2 + ((c1 >> 3) & 1);
	ch = &cc->channel[chan];

	c1 &= 7;

	if (c2 >= 0x40) {	/* Preamble Address Codes  001 crrr  1ri xxxu */
		int row = row_mapping[(c1 << 1) + ((c2 >> 5) & 1)];

		if (row < 0 || !ch->mode)
			return;

		ch->attr.underline = c2 & 1;
		ch->attr.background = BLACK;
		ch->attr.opacity = OPAQUE;
		ch->attr.flash = FALSE;

		word_break(cc, ch, 1);

		if (ch->mode == MODE_ROLL_UP) {
			int row1 = row - ch->roll + 1;

			if (row1 < 0)
				row1 = 0;

			if (row1 != ch->row1) {
				ch->row1 = row1;
				erase_memory(cc, ch, ch->hidden);
				erase_memory(cc, ch, ch->hidden ^ 1);
			}

			set_cursor(ch, 1, ch->row1 + ch->roll - 1);
		} else
			set_cursor(ch, 1, row);

		if (c2 & 0x10) {
			col = ch->col;

			for (i = (c2 & 14) * 2; i > 0 && col < COLUMNS - 1; i--)
				ch->line[col++] = cc->transp_space[chan >> 2];

			if (col > ch->col)
				ch->col = ch->col1 = col;

			ch->attr.italic = FALSE;
			ch->attr.foreground = WHITE;
		} else {
// not verified
			c2 = (c2 >> 1) & 7;

			if (c2 < 7) {
				ch->attr.italic = FALSE;
				ch->attr.foreground = palette_mapping[c2];
			} else {
				ch->attr.italic = TRUE;
				ch->attr.foreground = WHITE;
			}
		}

		return;
	}

	switch (c1) {
	case 0:		/* Optional Attributes		001 c000  010 xxxt */
// not verified
		ch->attr.opacity = (c2 & 1) ? SEMI_TRANSPARENT : OPAQUE;
		ch->attr.background = palette_mapping[(c2 >> 1) & 7];
		return;

	case 1:
		if (c2 & 0x10) {	/* Special Characters	001 c001  011 xxxx */
// not verified
			c2 &= 15;

			if (c2 == 9) { // "transparent space"
				if (ch->col < COLUMNS - 1) {
					ch->line[ch->col++] = cc->transp_space[chan >> 2];
					ch->col1 = ch->col;
				} else
					ch->line[COLUMNS - 2] = cc->transp_space[chan >> 2];
					// XXX boxed logic?
			} else {
				attr_char c = ch->attr;

				c.glyph = GL_CAPTION + 16 + (c.italic * 128) + (c2 & 15);

				put_char(cc, ch, c);
			}
		} else {		/* Midrow Codes		001 c001  010 xxxu */
// not verified
			ch->attr.flash = FALSE;
			ch->attr.underline = c2 & 1;

			c2 = (c2 >> 1) & 7;

			if (c2 < 7) {
				ch->attr.italic = FALSE;
				ch->attr.foreground = palette_mapping[c2];
			} else {
				ch->attr.italic = TRUE;
				ch->attr.foreground = WHITE;
			}
		}

		return;

	case 2:		/* Optional Extended Characters	001 c01f  01x xxxx */
	case 3:
		/* Send specs to the maintainer of this code */
		return;

	case 4:		/* Misc Control Codes		001 c10f  010 xxxx */
	case 5:		/* Misc Control Codes		001 c10f  010 xxxx */
		/* f ("field"): purpose? */

		switch (c2 & 15) {
		case 0:		/* Resume Caption Loading	001 c10f  010 0000 */
			ch = switch_channel(cc, ch, chan & 3);

			ch->mode = MODE_POP_ON;

// no?			erase_memory(cc, ch);

			return;

		/* case 4: reserved */

		case 5:		/* Roll-Up Captions		001 c10f  010 0xxx */
		case 6:
		case 7:
		{
			int roll = (c2 & 7) - 3;

			ch = switch_channel(cc, ch, chan & 3);

			if (ch->mode == MODE_ROLL_UP && ch->roll == roll)
				return;

			erase_memory(cc, ch, ch->hidden);
			erase_memory(cc, ch, ch->hidden ^ 1);

			ch->mode = MODE_ROLL_UP;
			ch->roll = roll;

			set_cursor(ch, 1, 14);

			ch->row1 = 14 - roll + 1;

			return;
		}

		case 9:		/* Resume Direct Captioning	001 c10f  010 1001 */
// not verified
			ch = switch_channel(cc, ch, chan & 3);
			ch->mode = MODE_PAINT_ON;
			return;

		case 10:	/* Text Restart			001 c10f  010 1010 */
// not verified
			ch = switch_channel(cc, ch, chan | 4);
			set_cursor(ch, 1, 0);
			return;

		case 11:	/* Resume Text Display		001 c10f  010 1011 */
			ch = switch_channel(cc, ch, chan | 4);
			return;

		case 15:	/* End Of Caption		001 c10f  010 1111 */
			ch = switch_channel(cc, ch, chan & 3);
			ch->mode = MODE_POP_ON;

			word_break(cc, ch, 1);

			ch->hidden ^= 1;

			render(ch->pg + (ch->hidden ^ 1), -1 /* ! */);

			erase_memory(cc, ch, ch->hidden); // yes?

			/*
			 *  A Preamble Address Code should follow,
			 *  reset to a known state to be safe.
			 *  Reset ch->line for new ch->hidden.
			 *  XXX row 0?
			 */
			set_cursor(ch, 1, ROWS - 1);

			return;

		case 8:		/* Flash On			001 c10f  010 1000 */
// not verified
			ch->attr.flash = TRUE;
			return;

		case 1:		/* Backspace			001 c10f  010 0001 */
// not verified
			if (ch->mode && ch->col > 1) {
				ch->line[--ch->col] = cc->transp_space[chan >> 2];

				if (ch->col < ch->col1)
					ch->col1 = ch->col;
			}

			return;

		case 13:	/* Carriage Return		001 c10f  010 1101 */
			if (ch == cc->channel + 5)
				itv_separator(cc, 0);

			if (!ch->mode)
				return;

			last_row = ch->row1 + ch->roll - 1;

			if (last_row > ROWS - 1)
				last_row = ROWS - 1;

			if (ch->row < last_row) {
				word_break(cc, ch, 1);
				set_cursor(ch, 1, ch->row + 1);
			} else {
				attr_char *acp = &ch->pg[ch->hidden ^ (ch->mode != MODE_POP_ON)]
					.text[ch->row1 * COLUMNS];

				word_break(cc, ch, 0);
				update(ch);

				memmove(acp, acp + COLUMNS, sizeof(*acp) * (ch->roll - 1) * COLUMNS);

				for (i = 0; i <= COLUMNS; i++)
					ch->line[i] = cc->transp_space[chan >> 2];

				if (ch->mode != MODE_POP_ON) {
					update(ch);
					roll_up(ch->pg + (ch->hidden ^ 1), ch->row1, last_row);
				}

				ch->col1 = ch->col = 1;
			}

			return;

		case 4:		/* Delete To End Of Row		001 c10f  010 0100 */
// not verified
			if (!ch->mode)
				return;

			for (i = ch->col; i <= COLUMNS - 1; i++)
				ch->line[i] = cc->transp_space[chan >> 2];

			word_break(cc, ch, 0);

			if (ch->mode != MODE_POP_ON) {
				update(ch);
				render(ch->pg + (ch->hidden ^ 1), ch->row);
			}

			return;

		case 12:	/* Erase Displayed Memory	001 c10f  010 1100 */
// s1, s4: EDM always before EOC
			if (ch->mode != MODE_POP_ON)
				erase_memory(cc, ch, ch->hidden);

			erase_memory(cc, ch, ch->hidden ^ 1);
			clear(ch->pg + (ch->hidden ^ 1));

			return;

		case 14:	/* Erase Non-Displayed Memory	001 c10f  010 1110 */
// not verified
			if (ch->mode == MODE_POP_ON)
				erase_memory(cc, ch, ch->hidden);

			return;
		}

		return;

	/* case 6: reserved */

	case 7:
		if (!ch->mode)
			return;

		switch (c2) {
		case 0x21 ... 0x23:	/* Misc Control Codes, Tabs	001 c111  010 00xx */
// not verified
			col = ch->col;

			for (i = c2 & 3; i > 0 && col < COLUMNS - 1; i--)
				ch->line[col++] = cc->transp_space[chan >> 2];

			if (col > ch->col)
				ch->col = ch->col1 = col;

			return;

		case 0x2D:		/* Optional Attributes		001 c111  010 11xx */
// not verified
			ch->attr.opacity = TRANSPARENT;
			break;

		case 0x2E:		/* Optional Attributes		001 c111  010 11xx */
		case 0x2F:
// not verified
			ch->attr.foreground = BLACK;
			ch->attr.underline = c2 & 1;
			break;

		default:
			return;
		}

		/* Optional Attributes, backspace magic */

		if (ch->col > 1 && (ch->line[ch->col - 1].glyph & 0x7F) == 0x20) {
			attr_char c = ch->attr;

			c.glyph = GL_CAPTION + 0x20 + (c.italic * 128);
			ch->line[ch->col - 1] = c;
		}
	}
}

void
vbi_caption_dispatcher(struct vbi *vbi, int line, unsigned char *buf)
{
	struct caption *cc = &vbi->cc;
	char c1 = buf[0] & 0x7F;
	int field2 = 1, i;

	switch (line) {
	case 21:	/* NTSC */
	case 22:	/* PAL */
		field2 = 0;
		break;

	case 335:	/* PAL, hardly XDS */
		break;

	case 284:	/* NTSC */
		if (parity(buf[0]) >= 0) {
			if (c1 == 0)
				return;
			else if (c1 <= 0x0F) {
				xds_separator(vbi, buf);
				cc->xds = (c1 != XDS_END);
				return;
			} else if (c1 <= 0x1F) {
				cc->xds = FALSE;
			} else if (cc->xds) {
				xds_separator(vbi, buf);
				return;
			}
		} else if (cc->xds) {
			xds_separator(vbi, buf);
			return;
		}
		
		break;

	default:
		return;
	}

	if (parity(buf[0]) < 0) {
		c1 = 127;
		buf[0] = c1; /* traditional 'bad' glyph, ccfont has */
		buf[1] = c1; /*  room, design a special glyph? */
	}

	pthread_mutex_lock(&cc->mutex);

	switch (c1) {
		channel *ch;
		attr_char c;

	case 0x01 ... 0x0F:
		if (!field2)
			cc->last[0] = 0;
		break; /* XDS field 1?? */

	case 0x10 ... 0x1F:
		if (parity(buf[1]) >= 0) {
			if (!field2
			    && buf[0] == cc->last[0]
			    && buf[1] == cc->last[1]) {
				/* cmd repetition F1: already executed */
				cc->last[0] = 0; /* one rep */
				break;
			}

			caption_command(cc, c1, buf[1] & 0x7F, field2);

			if (!field2) {
				cc->last[0] = buf[0];
				cc->last[1] = buf[1];
			}
		} else if (!field2)
			cc->last[0] = 0;

		break;

	default:
		ch = &cc->channel[(cc->curr_chan & 5) + field2 * 2];

		if (buf[0] == 0x80 && buf[1] == 0x80) {
			if (ch->mode) {
				if (ch->nul_ct == 2)
					word_break(cc, ch, 1);
				ch->nul_ct += 2;
			}

			break;
		}

		if (!field2)
			cc->last[0] = 0;

		ch->nul_ct = 0;

		if (!ch->mode)
			break;

		c = ch->attr;

		for (i = 0; i < 2; i++) {
			char ci = parity(buf[i]) & 0x7F; /* 127 if bad */

			if (ci == 0)
				continue;

			if (ch == cc->channel + 5) // 'T2'
				itv_separator(cc, ci);

			c.glyph = GL_CAPTION + (c.italic * 128) + ci;

			put_char(cc, ch, c);
		}
	}

	pthread_mutex_unlock(&cc->mutex);
}

static attr_rgba
default_colour_map[8] = {
	0xFF000000, 0xFF0000FF, 0xFF00FF00, 0xFF00FFFF,	
	0xFFFF0000, 0xFFFF00FF, 0xFFFFFF00, 0xFFFFFFFF
};

void
vbi_init_caption(struct vbi *vbi)
{
	struct caption *cc = &vbi->cc;
	channel *ch;
	int i;

	memset(cc, 0, sizeof(struct caption));

	pthread_mutex_init(&cc->mutex, NULL);

	for (i = 0; i < 2; i++) {
		cc->transp_space[i].foreground = WHITE;
		cc->transp_space[i].background = BLACK;
		cc->transp_space[i].glyph = GL_CAPTION + 0x20;
	}

	cc->transp_space[0].opacity = TRANSPARENT_SPACE;
	cc->transp_space[1].opacity = OPAQUE;

	for (i = 0; i < 8; i++) {
		ch = &cc->channel[i];

		if (i < 4) {
			ch->mode = MODE_NONE; // MODE_ROLL_UP;
			ch->row = ROWS - 1;
			ch->row1 = ROWS - 3;
			ch->roll = 3;
		} else {
			ch->mode = MODE_TEXT;
			ch->row1 = ch->row = 0;
			ch->roll = ROWS;
		}

		ch->attr.opacity = OPAQUE;
		ch->attr.foreground = WHITE;
		ch->attr.background = BLACK;

		set_cursor(ch, 1, ch->row);

		ch->hidden = 0;

		ch->pg[0].vbi = vbi;

		ch->pg[0].pgno = CC_PAGE_BASE + i;
		ch->pg[0].subno = ANY_SUB;

		ch->pg[0].rows = ROWS;
		ch->pg[0].columns = COLUMNS;

		ch->pg[0].screen_colour = 0;
		ch->pg[0].screen_opacity = (i < 4) ? TRANSPARENT_SPACE : OPAQUE;

		ch->pg[0].colour_map = default_colour_map;

		ch->pg[0].font[0] = font_descriptors; /* English */
		ch->pg[0].font[1] = font_descriptors;

		ch->pg[0].dirty.y0 = 0;
		ch->pg[0].dirty.y1 = ROWS - 1;
		ch->pg[0].dirty.roll = 0;

		erase_memory(cc, ch, 0);

		memcpy(&ch->pg[1], &ch->pg[0], sizeof(ch->pg[1]));
	}
}

#if !TEST

int
vbi_fetch_cc_page(struct vbi *vbi, struct fmt_page *pg, int page)
{
	channel *ch = vbi->cc.channel + ((page - CC_PAGE_BASE) & 7);
	struct fmt_page *spg;

	pthread_mutex_lock(&vbi->cc.mutex);

	spg = ch->pg + (ch->hidden ^ 1);

	memcpy(pg, spg, sizeof(*pg)); /* shortcut? */

	spg->dirty.y0 = ROWS;
	spg->dirty.y1 = -1;
	spg->dirty.roll = 0;

	pthread_mutex_unlock(&vbi->cc.mutex);

	return 1;
}

static void
render(struct fmt_page *pg, int row)
{
	vbi_event event;

	if (row < 0 || pg->dirty.roll) {
		/* no particular row or not fetched
		   since last roll/clear, redraw all */
		pg->dirty.y0 = 0;
		pg->dirty.y1 = ROWS - 1;
		pg->dirty.roll = 0;
	} else {
		pg->dirty.y0 = MIN(row, pg->dirty.y0);
		pg->dirty.y1 = MAX(row, pg->dirty.y1);
	}

	event.type = VBI_EVENT_CAPTION;
	event.pgno = pg->pgno;

	vbi_send_event(pg->vbi, &event);
}

static void
clear(struct fmt_page *pg)
{
	vbi_event event;

	pg->dirty.y0 = 0;
	pg->dirty.y1 = ROWS - 1;
	pg->dirty.roll = -ROWS;

	event.type = VBI_EVENT_CAPTION;
	event.pgno = pg->pgno;

	vbi_send_event(pg->vbi, &event);
}

static void
roll_up(struct fmt_page *pg, int first_row, int last_row)
{
	vbi_event event;

	if (pg->dirty.y0 <= pg->dirty.y1) {
		/* not fetched since last update, redraw all */
		pg->dirty.roll = 0;
		pg->dirty.y0 = MIN(first_row, pg->dirty.y0);
		pg->dirty.y1 = MAX(last_row, pg->dirty.y1);
	} else {
		pg->dirty.roll = -1;
		pg->dirty.y0 = first_row;
		pg->dirty.y1 = last_row;
	}

	event.type = VBI_EVENT_CAPTION;
	event.pgno = pg->pgno;

	vbi_send_event(pg->vbi, &event);
}

#else /* TEST */

/*
 *  Preliminary render code, for testing the decoder only
 *  ATTN: colour depth 5:6:5 only, code may be x86 specific
 */

#include "ccfont.xbm"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xutil.h>
#include <X11/xpm.h>

#define DISP_WIDTH	640
#define DISP_HEIGHT	480

#define CELL_WIDTH	16
#define CELL_HEIGHT	26

Display *		display;
int			screen;
Colormap		cmap;
Window			window;
GC			gc;
XEvent			event;
XImage *		ximage;
ushort *		ximgdata;

struct vbi		vbi;
int			draw_page = -1; /* page number, -1 all */
int			shift = 0, step = 3;
int			sh_first, sh_last;

#define RGB565(r, g, b)	\
	(((b & 0xF8) << 8) + ((g & 0xFC) << 3) + ((r & 0xF8) >> 3))

const ushort palette[8] = {
	RGB565(0x00, 0x00, 0x00),
	RGB565(0xFF, 0x00, 0x00),
	RGB565(0x00, 0xFF, 0x00),
	RGB565(0xFF, 0xFF, 0x00),
	RGB565(0x00, 0x00, 0xFF),
	RGB565(0xFF, 0x00, 0xFF),
	RGB565(0x00, 0xFF, 0xFF),
	RGB565(0xFF, 0xFF, 0xFF)
};

#define CHROMAKEY RGB565(0x80, 0xFF, 0x80)

static inline void
draw_char(ushort *canvas, unsigned int c, ushort *pen, int underline)
{
	ushort *s = ((unsigned short *) ccfont_bits)
		+ (c & 31) + (c >> 5) * 32 * CELL_HEIGHT;
	int x, y, b;

	for (y = 0; y < CELL_HEIGHT; y++) {
		b = *s;
		s += 32;

		if (underline && (y >= 24 && y <= 25))
			b = ~0;

		for (x = 0; x < CELL_WIDTH; x++) {
			canvas[x] = pen[b & 1];
			b >>= 1;
		}

		canvas += DISP_WIDTH;
	}
}

static void
draw_tspaces(ushort *canvas, int num_chars)
{
	int x, y;

	for (y = 0; y < CELL_HEIGHT; y++) {
		for (x = 0; x < CELL_WIDTH * num_chars; x++)
			canvas[x] = CHROMAKEY;

		canvas += DISP_WIDTH;
	}
}

static void
draw_row(ushort *canvas, attr_char *line)
{
	int i, num_tspaces = 0;
	ushort pen[2];

	for (i = 0; i < COLUMNS; i++) {
		if (line[i].opacity == TRANSPARENT_SPACE) {
			num_tspaces++;
			continue;
		}

		if (num_tspaces > 0) {
			draw_tspaces(canvas, num_tspaces);
			canvas += num_tspaces * CELL_WIDTH;
			num_tspaces = 0; 
		}

		switch (line[i].opacity) {
		case TRANSPARENT:
		case SEMI_TRANSPARENT:
			pen[0] = CHROMAKEY;
			pen[1] = palette[line[i].foreground];
			break;

		default:
			pen[0] = palette[line[i].background];
			pen[1] = palette[line[i].foreground];
			break;
		}

		draw_char(canvas, line[i].glyph & 0xFF, pen, line[i].underline);
		canvas += CELL_WIDTH;
	}

	if (num_tspaces > 0)
		draw_tspaces(canvas, num_tspaces);
}

void
bump(int n, bool draw)
{
	ushort *canvas = ximgdata + 45 * DISP_WIDTH;
	int i;

	if (shift < n)
		n = shift;

	if (shift <= 0 || n <= 0)
		return;

	memmove(canvas + (sh_first * CELL_HEIGHT) * DISP_WIDTH,
		canvas + (sh_first * CELL_HEIGHT + n) * DISP_WIDTH,
		((sh_last - sh_first + 1) * CELL_HEIGHT - n) * DISP_WIDTH * 2);

	if (draw)
		XPutImage(display, window, gc, ximage,
			0, 0, 0, 0, DISP_WIDTH, DISP_HEIGHT);

	shift -= n;
}

static void
render(struct fmt_page *pg, int row)
{
	ushort *canvas = ximgdata + 48 + 45 * DISP_WIDTH;
	int i;

	if (draw_page >= 0 && pg->pgno != draw_page)
		return;

	if (shift > 0) {
		bump(shift, FALSE);
		draw_tspaces(ximgdata + 48 + (45 + (sh_last * CELL_HEIGHT))
			* DISP_WIDTH, 34);
	}

	if (row < 0)
		for (i = 0; i < ROWS; i++)
			draw_row(ximgdata + 48 + (45 + i * CELL_HEIGHT)
				 * DISP_WIDTH, pg->text + i * COLUMNS);
	else
		draw_row(ximgdata + 48 + (45 + row * CELL_HEIGHT)
			 * DISP_WIDTH, pg->text + row * COLUMNS);

	XPutImage(display, window, gc, ximage,
		0, 0, 0, 0, DISP_WIDTH, DISP_HEIGHT);
}

static void
clear(struct fmt_page *pg)
{
	int i;

	if (draw_page >= 0 && pg->pgno != draw_page)
		return;

	for (i = 0; i < ROWS; i++)
		draw_tspaces(ximgdata + 48 + (45 + i * CELL_HEIGHT)
			 * DISP_WIDTH, COLUMNS);

	XPutImage(display, window, gc, ximage,
		0, 0, 0, 0, DISP_WIDTH, DISP_HEIGHT);
}

static void
roll_up(struct fmt_page *pg, int first_row, int last_row)
{
	ushort *canvas = ximgdata + 45 * DISP_WIDTH;
	int i;

	if (draw_page >= 0 && pg->pgno != draw_page)
		return;

#if 0
	sh_first = first_row;
	sh_last = last_row;
	shift = 26;
	bump(page, step, FALSE);

	canvas += ((last_row * CELL_HEIGHT) + CELL_HEIGHT - step) * DISP_WIDTH;

	for (i = 0; i < DISP_WIDTH * step; i++)
		canvas[i] = CHROMAKEY;
#else
	memmove(canvas + first_row * CELL_HEIGHT * DISP_WIDTH,
		canvas + (first_row + 1) * CELL_HEIGHT * DISP_WIDTH,
		(last_row - first_row) * CELL_HEIGHT * DISP_WIDTH * 2);

	draw_tspaces(ximgdata + 48 + (45 + (last_row * CELL_HEIGHT))
		* DISP_WIDTH, COLUMNS);
#endif
	XPutImage(display, window, gc, ximage,
		0, 0, 0, 0, DISP_WIDTH, DISP_HEIGHT);
}

/* Preliminary fetch function
 * XXX s1: something wrong with T2
 */

static void
fetch(int page)
{
	channel *ch = &vbi.cc.channel[page - CC_PAGE_BASE];

	if (shift > 0) {
		bump(shift, FALSE);
		draw_tspaces(ximgdata + 48 + (45 + (sh_last * CELL_HEIGHT))
			* DISP_WIDTH, 34);
	}

	draw_page = page;
}

static void
xevent(int nap_usec)
{
	while (XPending(display)) {
		XNextEvent(display, &event);

		switch (event.type) {
		case KeyPress:
		{
			int c = XLookupKeysym(&event.xkey, 0);

			switch (c) {
			case 'q':
			case 'c':
				exit(EXIT_SUCCESS);

			case '1' ... '8':
				fetch(c - '1' + CC_PAGE_BASE);
				return;

			case XK_F1 ... XK_F8:
				fetch(c - XK_F1 + CC_PAGE_BASE);
				return;
			}

			break;
		}

	        case ButtonPress:
			break;

		case FocusIn:
			break;

		case ConfigureNotify:
			break;

		case Expose:
			XPutImage(display, window, gc, ximage,
				0, 0, 0, 0, DISP_WIDTH, DISP_HEIGHT);
			break;

		case ClientMessage:
			exit(EXIT_SUCCESS);
		}
	}

	bump(step, TRUE);

	usleep(nap_usec / 4);
}

static bool
init_window(int ac, char **av)
{
	Atom delete_window_atom;
	XWindowAttributes wa;
	int i;

	if (!(display = XOpenDisplay(NULL))) {
		return FALSE;
	}

	screen = DefaultScreen(display);
	cmap = DefaultColormap(display, screen);
 
	window = XCreateSimpleWindow(display,
		RootWindow(display, screen),
		0, 0,		// x, y
		DISP_WIDTH, DISP_HEIGHT,
		2,		// borderwidth
		0xffffffff,	// fgd
		0x00000000);	// bgd 

	if (!window) {
		return FALSE;
	}

	XGetWindowAttributes(display, window, &wa);
			
	if (wa.depth != 16) {
		fprintf(stderr, "Can only run at colour depth 16 (5:6:5)\n");
		return FALSE;
	}

	if (!(ximgdata = malloc(DISP_WIDTH * DISP_HEIGHT * 2))) {
		return FALSE;
	}

	for (i = 0; i < DISP_WIDTH * DISP_HEIGHT; i++)
		ximgdata[i] = CHROMAKEY;

	ximage = XCreateImage(display,
		DefaultVisual(display, screen),
		DefaultDepth(display, screen),
		ZPixmap, 0, (char *) ximgdata,
		DISP_WIDTH, DISP_HEIGHT,
		8, 0);

	if (!ximage) {
		return FALSE;
	}

	delete_window_atom = XInternAtom(display, "WM_DELETE_WINDOW", False);

	XSelectInput(display, window, KeyPressMask | ExposureMask | StructureNotifyMask);
	XSetWMProtocols(display, window, &delete_window_atom, 1);
	XStoreName(display, window, "Caption Test - [Q], [F1]..[F8]");

	gc = XCreateGC(display, window, 0, NULL);

	XMapWindow(display, window);
	       
	XSync(display, False);

	XPutImage(display, window, gc, ximage,
		0, 0, 0, 0, DISP_WIDTH, DISP_HEIGHT);

	return TRUE;
}

static inline int
odd(int c)
{
	int n;
	
	n = c ^ (c >> 4);
	n = n ^ (n >> 2);
	n = n ^ (n >> 1);

	if (!(n & 1))
		c |= 0x80;

	return c;
}

static void
printc(char c)
{
	unsigned char buf[2];

	buf[0] = odd(c);
	buf[1] = 0x80;
	vbi_caption_dispatcher(&vbi, 21, buf);

	xevent(33333);
}

static void
prints(char *s)
{
	unsigned char buf[2];

	for (; s[0] && s[1]; s += 2) {
		buf[0] = odd(s[0]);
		buf[1] = odd(s[1]);
		vbi_caption_dispatcher(&vbi, 21, buf);
	}

	if (s[0]) {
		buf[0] = odd(s[0]);
		buf[1] = 0x80;
		vbi_caption_dispatcher(&vbi, 21, buf);
	}

	xevent(33333);
}

static void
cmd(unsigned int n)
{
	unsigned char buf[2];

	buf[0] = odd(n >> 8);
	buf[1] = odd(n & 0x7F);

	vbi_caption_dispatcher(&vbi, 21, buf);

	xevent(33333);
}

enum {
	white, green, red, yellow, blue, cyan, magenta, black
};

static int mapping_row[] = {
	2, 3, 4, 5,  10, 11, 12, 13, 14, 15,  0, 6, 7, 8, 9, -1
};


#define italic 7
#define underline 1
#define opaque 0
#define semi_transp 1

#define BACKG(bg, t)		(cmd(0x2000), cmd(0x1020 + ((ch & 1) << 11) + (bg << 1) + t))
#define PREAMBLE(r, fg, u)	cmd(0x1040 + ((ch & 1) << 11) + ((mapping_row[r] & 14) << 7) + ((mapping_row[r] & 1) << 5) + (fg << 1) + u)
#define INDENT(r, fg, u)	cmd(0x1050 + ((ch & 1) << 11) + ((mapping_row[r] & 14) << 7) + ((mapping_row[r] & 1) << 5) + ((fg / 4) << 1) + u)
#define MIDROW(fg, u)		cmd(0x1120 + ((ch & 1) << 11) + (fg << 1) + u)
#define SPECIAL_CHAR(n)		cmd(0x1130 + ((ch & 1) << 11) + n)
#define RESUME_CAPTION		cmd(0x1420 + ((ch & 1) << 11) + ((ch & 2) << 7))
#define BACKSPACE		cmd(0x1421 + ((ch & 1) << 11) + ((ch & 2) << 7))
#define DELETE_EOR		cmd(0x1424 + ((ch & 1) << 11) + ((ch & 2) << 7))
#define ROLL_UP(rows)		cmd(0x1425 + ((ch & 1) << 11) + ((ch & 2) << 7) + rows - 2)
#define FLASH_ON		cmd(0x1428 + ((ch & 1) << 11) + ((ch & 2) << 7))
#define RESUME_DIRECT		cmd(0x1429 + ((ch & 1) << 11) + ((ch & 2) << 7))
#define TEXT_RESTART		cmd(0x142A + ((ch & 1) << 11) + ((ch & 2) << 7))
#define RESUME_TEXT		cmd(0x142B + ((ch & 1) << 11) + ((ch & 2) << 7))
#define END_OF_CAPTION		cmd(0x142F + ((ch & 1) << 11) + ((ch & 2) << 7))
#define ERASE_DISPLAY		cmd(0x142C + ((ch & 1) << 11) + ((ch & 2) << 7))
#define CR			cmd(0x142D + ((ch & 1) << 11) + ((ch & 2) << 7))
#define ERASE_HIDDEN		cmd(0x142E + ((ch & 1) << 11) + ((ch & 2) << 7))
#define TAB(t)			cmd(0x1720 + ((ch & 1) << 11) + ((ch & 2) << 7) + t)
#define TRANSP			(cmd(0x2000), cmd(0x172D + ((ch & 1) << 11)))
#define BLACK(u)		(cmd(0x2000), cmd(0x172E + ((ch & 1) << 11) + u))

static void
PAUSE(int frames)
{
	while (frames--)
		xevent(33333);
}

void
sample_beta(void)
{
	unsigned char cc[2];
	char buf[256];
	double dt;
	int index, line;
	int items;
	int i, j;

	while (!feof(stdin)) {
		fgets(buf, 255, stdin);

		/* usually 0.033333 (1/30) */
		dt = strtod(buf, NULL);

		items = fgetc(stdin);

//		printf("%8.6f %d:\n", dt, items);

		for (i = 0; i < items; i++) {
			index = fgetc(stdin);
			line = fgetc(stdin);
			line += 256 * fgetc(stdin);

			cc[0] = fgetc(stdin);
			cc[1] = fgetc(stdin);

//			printf(" %3d %02x %02x ", line & 0xFFF, cc[0] & 0x7F, cc[1] & 0x7F);
//			printf(" %c%c\n", printable(cc[0]), printable(cc[1]));

			vbi_caption_dispatcher(&vbi, line & 0xFFF, cc);

			xevent(dt * 1e6);
		}
	}
}

static void
hello_world(void)
{
	int ch = 0;
	int i;

	draw_page = -1;

	prints(" HELLO WORLD! ");
	PAUSE(30);

	ch = 4;
	TEXT_RESTART;
	prints("Character set - Text 1");
	CR; CR;
	for (i = 32; i <= 127; i++) {
		printc(i);
		if ((i & 15) == 15)
			CR;
	}
	MIDROW(italic, 0);
	for (i = 32; i <= 127; i++) {
		printc(i);
		if ((i & 15) == 15)
			CR;
	}
	MIDROW(white, underline);
	for (i = 32; i <= 127; i++) {
		printc(i);
		if ((i & 15) == 15)
			CR;
	}
	MIDROW(white, 0);
	prints("Special: ");
	for (i = 0; i <= 15; i++) {
		SPECIAL_CHAR(i);
	}
	CR;
	prints("DONE - Text 1 ");
	PAUSE(50);

	ch = 5;
	TEXT_RESTART;
	prints("Styles - Text 2");
	CR; CR;
	MIDROW(white, 0); prints("WHITE"); CR;
	MIDROW(red, 0); prints("RED"); CR;
	MIDROW(green, 0); prints("GREEN"); CR;
	MIDROW(blue, 0); prints("BLUE"); CR;
	MIDROW(yellow, 0); prints("YELLOW"); CR;
	MIDROW(cyan, 0); prints("CYAN"); CR;
	MIDROW(magenta, 0); prints("MAGENTA"); BLACK(0); CR;
	BACKG(white, opaque); prints("WHITE"); BACKG(black, opaque); CR;
	BACKG(red, opaque); prints("RED"); BACKG(black, opaque); CR;
	BACKG(green, opaque); prints("GREEN"); BACKG(black, opaque); CR;
	BACKG(blue, opaque); prints("BLUE"); BACKG(black, opaque); CR;
	BACKG(yellow, opaque); prints("YELLOW"); BACKG(black, opaque); CR;
	BACKG(cyan, opaque); prints("CYAN"); BACKG(black, opaque); CR;
	BACKG(magenta, opaque); prints("MAGENTA"); BACKG(black, opaque); CR;
	TRANSP;
	prints(" TRANSPARENT BACKGROUND ");
	BACKG(black, opaque); CR;
	MIDROW(white, 0); FLASH_ON;
	prints(" Flashing Text (if implemented) "); CR;
	MIDROW(white, 0); prints("DONE - Text 2 ");
	PAUSE(50);

	ch = 0;
	ROLL_UP(2);
	ERASE_DISPLAY;
	prints(" ROLL-UP TEST "); PAUSE(20);
	prints(">> A young Jedi named Darth Vader,"); CR; PAUSE(20);
	prints("who was a pupil of mine until he"); CR; PAUSE(20);
	prints("turned to evil, helped the Empire"); CR; PAUSE(20);
	prints("hunt down and destroy the Jedi Knights."); CR; PAUSE(20);
	prints("He betrayed and murdered your father."); CR; PAUSE(20);
	prints("Now the Jedi are all but extinct."); CR; PAUSE(20);
	prints("Vader was seduced by the dark side of"); CR; PAUSE(20);
	prints("the Force."); CR; PAUSE(20);                        
	prints(">> The Force?"); CR; PAUSE(20);
	prints(">> Well, the Force is what gives a"); CR; PAUSE(20);
	prints("Jedi his power. It's an energy field"); CR; PAUSE(20);
	prints("created by all living things."); CR; PAUSE(20);
	prints("It surrounds us and penetrates us."); CR; PAUSE(20);
	prints("It binds the galaxy together."); CR; PAUSE(20);
	CR; PAUSE(30);
	prints(" DONE - Caption 1 ");
	PAUSE(30);

	ch = 1;
	RESUME_DIRECT;
	ERASE_DISPLAY;
	MIDROW(yellow, 0);
	INDENT(2, 10, 0); prints(" FOO "); CR;
	INDENT(3, 10, 0); prints(" MIKE WAS HERE "); CR; PAUSE(20);
	MIDROW(red, 0);
	INDENT(6, 13, 0); prints(" AND NOW... "); CR;
	INDENT(8, 13, 0); prints(" HE'S HERE "); CR; PAUSE(20);
	PREAMBLE(12, cyan, 0);
	prints("01234567890123456789012345678901234567890123456789"); CR;
	MIDROW(white, 0);
	prints(" DONE - Caption 2 "); CR;
	PAUSE(30);
}

int
main(int ac, char **av)
{
	if (!init_window(ac, av))
		exit(EXIT_FAILURE);

	vbi_init_caption(&vbi);

	if (isatty(STDIN_FILENO))
		hello_world();
	else {
		draw_page = 1;
		sample_beta();
	}

	printf("Done.\n");

	for (;;)
		xevent(33333);

	exit(EXIT_SUCCESS);
}

#endif
