/*
 *  Closed Caption Decoder  DRAFT
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

/* $Id: caption.c,v 1.1 2000-12-02 19:52:20 mschimek Exp $ */

#include "ccfont.xbm"

/*
    TODO:
    - everything
 */

enum colours {
	BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE
};

enum opacity {
	TRANSPARENT_SPACE, TRANSPARENT, SEMI_TRANSPARENT, OPAQUE
};

typedef struct {
	unsigned	underline	: 1;
	unsigned	flash		: 1;
	unsigned	pad		: 6;
	unsigned	opacity		: 2;
	unsigned	foreground	: 3;
	unsigned	background	: 3;
	unsigned	glyph		: 16;
} attr_char;

render()

roll_up()

/*
 *
 */

#define NUM_COLS	34
#define NUM_ROWS	15

#define CODE_PAGE	(8 * 256)

enum mode {
	POP_ON, PAINT_ON, ROLL_UP, TEXT
};

struct caption {
	int		chan;
	attr_char	transp_space[2];	/* caption, text mode */

	struct ch_rec {
		mode		mode;
		int		col, col1;
		int		row, row1;
		int		roll;
		int		italic;
		attr_char	attr;
		attr_char *	line;
		attr_char	buffer[NUM_COLS * NUM_ROWS];
	}		channels[8];		/* caption 1-4, text 1-4 */

	bool		xds;
};

static void
put_char(struct caption *cc, struct ch_rec *ch, attr_char c)
{
	if (ch->col < NUM_COLS - 1)
		ch->line[ch->col++] = c;
	else
		ch->line[NUM_COLS - 2] = c;

    XXX render, box
}

#define switch_channel(pcc, chan)					\
	(&(pcc)->channels[(pcc)->chan = (chan)])

static inline void
set_cursor(struct ch_rec *ch, int col, int row)
{
	ch->col = ch->col1 = col;
	ch->row = ch->row1 = row;

	ch->line = ch->buffer + row * NUM_COLS;
}

static void
erase_memory(struct caption *cc, struct ch_rec *ch)
{
	attr_char c = cc->transp_space[ch >= &cc->channels[4]];
	int i;

	for (i = 0; i < NUM_ROWS * NUM_COLS; i++)
		ch->buffer[i] = c;
}

static inline void
caption_command(unsigned char c1, unsigned char c2, bool field2)
{
	struct ch_rec *ch;
	int chan, i;

	chan = (cc->chan & 4) + field2 * 2 + ((c1 >> 3) & 1);
	ch = &cc->channels[chan];

	c1 &= 7;

	if (c2 >= 0x40) {	/* Preamble Address Codes  001 crrr  1ri xxxu */
		static int row_mapping[] = {
			10, -1,  0, 1, 2, 3,  11, 12, 13, 14,  4, 5, 6, 7, 8, 9
		};

		ch->attr.underline = c2 & 1;

		if (c2 & 0x10) {
			for (i = (c2 & 14) * 2; i > 0 && ch->col < NUM_COLS - 1; i--)
				ch->line[ch->col++] = cc->transp_space[chan >> 2];

			ch->col1 = ch->col;
		} else {
			int row = row_mapping[(c1 << 1) + ((c2 >> 5) & 1)];

			if (row >= 0) {
				set_cursor(ch, 1, row);

				ch->attr.background = BLACK;
				ch->attr.opacity = OPAQUE;
				ch->attr.flash = FALSE;

				c2 = (c2 >> 1) & 7;

				if (c2 < 7) {
					ch->italic = FALSE;
					ch->attr.foreground = palette[c2];
				} else
					ch->italic = TRUE;
			}
		}

		return;
	}

	switch (c1) {
	case 0:		/* Optional Attributes		001 c000  010 xxxt */
		ch->attr.opacity = (c2 & 1) ? SEMI_TRANSPARENT : OPAQUE;
		ch->attr.background = palette[(c2 >> 1) & 7];
		return;

	case 1:
		if (c2 & 0x10) {	/* Special Characters	001 c001  011 xxxx */
			c2 &= 15;

			if (c2 == 9) {
				if (ch->col < NUM_COLS - 1) {
					ch->line[ch->col++] = cc->transp_space[chan >> 2];
					ch->col1 = ch->col;
				} else
					ch->line[NUM_COLS - 2] = cc->transp_space[chan >> 2];
			} else {
				attr_char c = ch->attr;

				c.glyph = CODE_PAGE + 16 + (ch->italic * 128) + (c2 & 15);

				put_char(cc, ch, c);
			}
		} else {		/* Midrow Codes		001 c001  010 xxxu */
			ch->attr.flash = FALSE;
			ch->attr.underline = c2 & 1;

			c2 = (c2 >> 1) & 7;

			if (c2 < 7) {
				ch->italic = FALSE;
				ch->attr.foreground = palette[c2];
			} else
				ch->italic = TRUE;
		}

		return;

	case 2, 3:	/* Optional Extended Characters	001 c01f  01x xxxx */
		/* Send specs to the maintainer of this code */
		return;

	case 4:		/* Misc Control Codes		001 c10f  010 xxxx */
	case 5:		/* Misc Control Codes		001 c10f  010 xxxx */
		/* f (field): purpose? */

		switch (c2 & 15) {
		case 0:		/* Resume Caption Loading	001 c10f  010 0000 */
			ch = switch_channel(cc, chan & 3);
			ch->mode = POP_ON;
			erase_memory(cc, ch);
			return;

		/* case 4: reserved */

		case 5, 6, 7:	/* Roll-Up Captions		001 c10f  010 0xxx */
			ch = switch_channel(cc, chan & 3);
			ch->mode = ROLL_UP;
			ch->roll = (c2 & 7) - 3;
			set_cursor(ch, 1, 14);
			return;

		case 9:		/* Resume Direct Captioning	001 c10f  010 1001 */
			ch = switch_channel(cc, chan & 3);
			ch->mode = PAINT_ON;
			return;

		case 10:	/* Text Restart			001 c10f  010 1010 */
			ch = switch_channel(cc, chan | 4);
			erase_memory(cc, ch);
			set_cursor(ch, 1, 0);
			return;

		case 11:	/* Resume Text Display		001 c10f  010 1011 */
			ch = switch_channel(cc, chan | 4);
			return;

		case 15:	/* End Of Caption		001 c10f  010 1111 */
			ch = switch_channel(cc, chan & 3);
			ch->mode = POP_ON;
			render(cc, ch);
			erase_memory(cc, ch);
			return;

		case 8:		/* Flash On			001 c10f  010 1000 */
			ch->attr.flash = TRUE;
			return;

		case 1:		/* Backspace			001 c10f  010 0001 */
			if (ch->col > 1) {
				ch->line[--ch->col] = cc->transp_space[chan >> 2];
				if (ch->col < ch->col1)
					ch->col1 = ch->col;
			}
			return;

		case 4:		/* Delete To End Of Row		001 c10f  010 0100 */
			for (i = ch->col; i <= NUM_COLS - 1; i++)
				ch->line[i] = cc->transp_space[chan >> 2];
			render(cc, ch);
			return;

		case 13:	/* Carriage Return		001 c10f  010 1101 */
			if (ch->row < 14 && ch->row < (ch->row1 + ch->roll - 1))
				set_cursor(ch, 1, cc->row + 1);
			else
				roll_up(cc, ch);
			return;

		case 12:	/* Erase Displayed Memory	001 c10f  010 1100 */
			erase_memory(cc, ch);
			return;

		case 14:	/* Erase Non-Displayed Memory	001 c10f  010 1110 */
			if (ch->mode == POP_ON)
				erase_memory(cc, ch);
			return;
		}

		return;

	/* case 6: reserved */

	case 7:
		switch (c2) {
		case 0x21 ... 0x23:	/* Misc Control Codes, Tabs	001 c111  010 00xx */
			for (i = c2 & 3; i > 0 && ch->col < NUM_ROWS - 1; i--)
				ch->line[ch->col++] = cc->transp_space[chan >> 2];
			ch->col1 = ch->col;
			return;

		case 0x2D:		/* Optional Attributes		001 c111  010 11xx */
			ch->attr.opacity = TRANSPARENT;
			break;

		case 0x2E, 0x2F:	/* Optional Attributes		001 c111  010 11xx */
			ch->attr.foreground = BLACK;
			ch->attr.underlined = c2 & 1;
			break;

		default:
			return;
		}

		if (ch->col > 1 && (ch->line[ch->col - 1].glyph & 0x7F) == 0x20) {
			attr_char c = ch->attr;

			c.glyph = CODE_PAGE + 0x20 + (ch->italic * 128);
			ch->line[ch->col - 1] = c;
		}
	}
}

static void
dispatch(struct caption *cc, unsigned char *buf, bool field2)
{
	char c1 = buf[0] & 0x7F;
	int i;

	if (field2) {
		if (odd_parity[buf[0]] && c1 && c1 <= 0x0F) {
			xds(buf);
			cc->xds = (c1 != 0x0F);
			return;
		} else if (cc->xds) {
			xds(buf);
			return;
		}
	}

	if (!odd_parity[buf[0]]) {
		buf[0] = 127; /* 'bad' glyph */
		buf[1] = 127;
	}

	switch (c1) {
		struct ch_rec *ch;
		attr_char c;

	case 0x01 ... 0x0F:
		return; /* XDS field 1?? */

	case 0x10 ... 0x1F:
		if (odd_parity[buf[1]])
			caption_command(c1, buf[1] & 0x7F, field2);
		return;

	default:
		ch = &cc->channels[cc->chan];
		c = ch->attr;

		for (i = 0; i < 2; i++) {
			char ci = odd_parity[buf[i]] ? (buf[i] & 0x7F) : 127;

			if (ci == 0) /* NUL */
				continue;

			c.glyph = CODE_PAGE + (ch->italic * 128) + ci;

			put_char(cc, ch, c);
		}
	}
}

static void
reset_caption(struct caption *cc)
{
	struct ch_rec *ch;
	int i;

	memset(cc, 0, sizeof(struct caption));

	for (i = 0; i < 2; i++) {
		cc->transp_space[i].foreground = WHITE;
		cc->transp_space[i].background = BLACK;
		cc->transp_space[i].glyph = CODE_PAGE + 0x20;
	}

	cc->transp_space[0].opacity = TRANSPARENT_SPACE;
	cc->transp_space[1].opacity = OPAQUE;

	for (i = 0; i < 8; i++) {
		ch = &cc->channels[i];

		if (i < 4) {
			ch->mode = ROLL_UP;
			ch->col1 = ch->col = 1;
			ch->row = NUM_ROWS - 1;
			ch->row1 = NUM_ROWS - 1 - 3;
			ch->roll = 3;
		} else {
			ch->mode = TEXT;
			ch->col1 = ch->col = 1;
			ch->row1 = ch->row = 0;
			ch->roll = NUM_ROWS;
		}

		ch->line = ch->buffer + ch->row * NUM_COLS;

		erase_memory(cc, ch);
	}
}
