/*
 *  Closed Caption Decoder  DRAFT
 *
 *  gcc -g -ocaption caption.c -L/usr/X11R6/lib -lX11
 *
 *  Copyright (C) 2000 Michael H. Schimek
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

/* $Id: caption.c,v 1.3 2000-12-04 16:23:30 mschimek Exp $ */

#include "ccfont.xbm"

#define TEST 1

/*
    TODO:
    - a lot
 */

#include <stdio.h>
#include <stdlib.h>

typedef char bool;
enum { FALSE, TRUE };

unsigned char
odd_parity[256] =
{
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0
};




/*
 *  WST/CC BGR palette, prepare for 32 random entries in the
 *  future: Store palette in render context, depth and endianess
 *  hardware specific (e.g. 8:8:8 or CI8), remember palette
 *  until we switch stations (decoder will fetch station palette
 *  from cache and reload all entries), export a load function
 *  (int index, uchar r, g, b).
 */
typedef enum {
	BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE
} colours;

/*
 *  TRANSPARENT_SPACE:
 *    Display video if possible, otherwise override opacity
 *    as desired and display as normal glyph (all attributes
 *    and colours valid, character is 0x20 space on given
 *    code page)
 *  other: overlay glyph as requested, otherwise override
 *    opacity as desired by user or mandated by hardware
 *    limitations.
 */
typedef enum {
	TRANSPARENT_SPACE, TRANSPARENT, SEMI_TRANSPARENT, OPAQUE
} opacity;

typedef struct {
	unsigned	underline	: 1;
	unsigned	flash		: 1;  /* how, if? */
	unsigned	pad		: 6;
	unsigned	opacity		: 2;
	unsigned	foreground	: 3;
	unsigned	background	: 3;
	unsigned	glyph		: 16;
} attr_char;

/*
 *  Render <row> 0 ... 14 or -1 all rows, from <buffer> if you're
 *  monitoring <page>, which is CC_PAGE_BASE +
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
 *                   not program related"
 *  6:  "Text 3"    "Third and forth text services, to be used only
 *  7:  "Text 4"     if Text 1 and Text 2 are not sufficient"
 *
 *  Remember unscaled image or you'll have to redraw all if row != -1;
 *  Don't dereference <buffer> pointer after returning.
 *
 *  The decoder doesn't filter channels, all 'pages' in cache are live
 *  cf. WST, except for POP_ON mode because we only cache the hidden
 *  buffer we're writing while the unscaled image is our visible buffer.
 *  render() is intended to be a callback (function pointer), we can
 *  export a fetch() function calling render() in turn to update the
 *  screen immediately after a switch.
 *
 *  If the same render() is used for WST we may need a bool indicating
 *  the required layout.
 */
extern void render(int page, attr_char *buffer, int row);

/*
 *  Start soft scrolling, move <first_row> + 1 ... <last_row> (inclusive)
 *  up to <first_row>. Rows numbered 0 ... 14. Feel free to scroll a row
 *  at once, render from the buffer, move unscaled image data, window
 *  contents, windows, whatever. first_row is provided in buffer to render,
 *  the decoder will roll <buffer> *after* returning and continue writing
 *  in last_row or another. Don't dereference <buffer> pointer after
 *  returning. Soft scrolling finished or not, render() can be called
 *  any time later, any row, so prepare.
 */
extern void roll_up(int page, attr_char *buffer, int first_row, int last_row);

/*
 *
 */

#define NUM_COLS	34
#define NUM_ROWS	15

#define CODE_PAGE	(8 * 256)
/*
 *  Any, refers to the 256 glyphs in ccfont.xbm.
 *  The character cell is 16 x 26 pixels matching a
 *  square pixel 640 x 480 display, text area at
 *  (48, 45) - (591, 434), display video outside.
 *  Must scale text for other aspects accordingly.
 */

#define CC_PAGE_BASE	1

typedef enum {
	POP_ON, PAINT_ON, ROLL_UP, TEXT
} mode;

struct caption {
	int		chan;
	attr_char	transp_space[2];	/* caption, text mode */

	struct ch_rec {
		mode		mode;
		int		col, col1;
		int		row, row1;
		int		roll;
		bool		redraw_all;
		bool		italic;
		attr_char	attr;
		attr_char *	line;
		attr_char	buffer[NUM_COLS * NUM_ROWS];
	}		channels[8];		/* caption 1-4, text 1-4 */

	bool		xds;
};

static void
word_break(struct caption *cc, struct ch_rec *ch)
{
	/* box, iff */

	render(CC_PAGE_BASE + (ch - cc->channels), ch->buffer, ch->redraw_all ? -1 : ch->row);
	ch->redraw_all = FALSE;
}

static void
put_char(struct caption *cc, struct ch_rec *ch, attr_char c)
{
	if (ch->col < NUM_COLS - 1)
		ch->line[ch->col++] = c;
	else
		ch->line[NUM_COLS - 2] = c;

	if ((c.glyph & 0x7F) == 0x20 /* || word too long? */)
		word_break(cc, ch);
}

#define switch_channel(cc, new_chan) \
	(&((cc)->channels[(cc)->chan = (new_chan)]))

static inline void
set_cursor(struct ch_rec *ch, int col, int row)
{
	ch->col = ch->col1 = col;
	ch->row = row;

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
caption_command(struct caption *cc,
	unsigned char c1, unsigned char c2, bool field2)
{
	static const colours palette[8] = {
		WHITE, GREEN, BLUE, CYAN, RED, YELLOW, MAGENTA, BLACK
	};
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
				word_break(cc, ch);
				set_cursor(ch, 1, row);
				ch->row1 = row;

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

	case 2:		/* Optional Extended Characters	001 c01f  01x xxxx */
	case 3:
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

		case 5:		/* Roll-Up Captions		001 c10f  010 0xxx */
		case 6:
		case 7:
			ch = switch_channel(cc, chan & 3);
			ch->mode = ROLL_UP;

			ch->roll = (c2 & 7) - 3;

			word_break(cc, ch);
			set_cursor(ch, 1, 14);
			ch->row1 = 14 - ch->roll + 1;

			return;

		case 9:		/* Resume Direct Captioning	001 c10f  010 1001 */
			ch = switch_channel(cc, chan & 3);
			ch->mode = PAINT_ON;
			return;

		case 10:	/* Text Restart			001 c10f  010 1010 */
			ch = switch_channel(cc, chan | 4);

			erase_memory(cc, ch);
			ch->redraw_all = TRUE;

			set_cursor(ch, 1, 0);
			return;

		case 11:	/* Resume Text Display		001 c10f  010 1011 */
			ch = switch_channel(cc, chan | 4);
			return;

		case 15:	/* End Of Caption		001 c10f  010 1111 */
			ch = switch_channel(cc, chan & 3);
			ch->mode = POP_ON;

			render(CC_PAGE_BASE + chan, ch->buffer, -1);
			ch->redraw_all = FALSE;

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

		case 13:	/* Carriage Return		001 c10f  010 1101 */
		{
			int last_row = ch->row1 + ch->roll - 1;

			if (last_row > NUM_ROWS - 1)
				last_row = NUM_ROWS - 1;

			word_break(cc, ch);

			if (ch->row < last_row)
				set_cursor(ch, 1, ch->row + 1);
			else {
				if (ch->mode != POP_ON)
					roll_up(CC_PAGE_BASE + chan, ch->buffer, ch->row1, last_row);

				memmove(ch->buffer + ch->row1 * NUM_COLS,
					ch->buffer + (ch->row1 + 1) * NUM_COLS,
					(ch->roll - 1) * NUM_COLS);

				for (i = 1; i <= NUM_COLS - 1 /* ! */; i++)
					ch->line[i] = cc->transp_space[chan >> 2];

				ch->col1 = ch->col = 1;
			}

			return;
		}

		case 4:		/* Delete To End Of Row		001 c10f  010 0100 */
			for (i = ch->col; i <= NUM_COLS - 1; i++)
				ch->line[i] = cc->transp_space[chan >> 2];

			word_break(cc, ch);

			render(CC_PAGE_BASE + chan, ch->buffer, ch->redraw_all ? -1 : ch->row);
			ch->redraw_all = FALSE;

			return;

		case 12:	/* Erase Displayed Memory	001 c10f  010 1100 */
			erase_memory(cc, ch);
			ch->redraw_all = TRUE; /* override render row */
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

		case 0x2E:		/* Optional Attributes		001 c111  010 11xx */
		case 0x2F:
			ch->attr.foreground = BLACK;
			ch->attr.underline = c2 & 1;
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

void
xds(unsigned char *buf)
{
}

void
dispatch_caption(struct caption *cc, unsigned char *buf, bool field2)
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
		buf[0] = 127; /* traditional 'bad' glyph, ccfont has */
		buf[1] = 127; /*  room, design a special glyph? */
	}

	switch (c1) {
		struct ch_rec *ch;
		attr_char c;

	case 0x01 ... 0x0F:
		return; /* XDS field 1?? */

	case 0x10 ... 0x1F:
		if (odd_parity[buf[1]])
			caption_command(cc, c1, buf[1] & 0x7F, field2);
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

void
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
			ch->row1 = NUM_ROWS - 3;
			ch->roll = 3;
		} else {
			ch->mode = TEXT;
			ch->col1 = ch->col = 1;
			ch->row1 = ch->row = 0;
			ch->roll = NUM_ROWS;
		}

		ch->attr.opacity = OPAQUE;
		ch->attr.foreground = WHITE;
		ch->attr.background = BLACK;

		ch->line = ch->buffer + ch->row * NUM_COLS;

		erase_memory(cc, ch);
	}
}

#if TEST

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

struct caption		caption;
int			draw_page;
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

/* ushort NOT PORTABLE, use explicit 16 or 32 bit type */
static inline void
draw_char(ushort *p, unsigned int c, ushort *pen, int underline)
{
	ushort *s = ((unsigned short *) bitmap_bits)
		+ (c & 31) + (c >> 5) * 32 * CELL_HEIGHT;
	int x, y, b;

	for (y = 0; y < CELL_HEIGHT; y++) {
		b = *s;
		s += 32;

		if (underline && (y >= 24 && y <= 25))
			b = ~0;

		for (x = 0; x < CELL_WIDTH; x++) {
			p[x] = pen[b & 1];
			b >>= 1;
		}

		p += DISP_WIDTH;
	}
}

static void
draw_tspaces(ushort *p, int num_chars)
{
	int x, y;

	for (y = 0; y < CELL_HEIGHT; y++) {
		for (x = 0; x < CELL_WIDTH * num_chars; x++)
			p[x] = CHROMAKEY;

		p += DISP_WIDTH;
	}
}

static void
draw_row(ushort *p, attr_char *line)
{
	int i, num_tspaces = 0;
	ushort pen[2];

	for (i = 0; i < NUM_COLS; i++) {
		if (line[i].opacity == TRANSPARENT_SPACE) {
			num_tspaces++;
			continue;
		}

		if (num_tspaces > 0) {
			draw_tspaces(p, num_tspaces);
			p += num_tspaces * CELL_WIDTH;
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

		draw_char(p, line[i].glyph & 0xFF, pen, line[i].underline);
		p += CELL_WIDTH;
	}

	if (num_tspaces > 0)
		draw_tspaces(p, num_tspaces);
}

void
bump(int n, bool draw)
{
	ushort *p = ximgdata + 45 * DISP_WIDTH;
	int i;

//	if (page != draw_page)
//		return;

	if (shift < n)
		n = shift;

	if (shift <= 0 || n <= 0)
		return;

	memmove(p + (sh_first * CELL_HEIGHT) * DISP_WIDTH,
		p + (sh_first * CELL_HEIGHT + n) * DISP_WIDTH,
		((sh_last - sh_first + 1) * CELL_HEIGHT - n) * DISP_WIDTH * 2);

	if (draw)
		XPutImage(display, window, gc, ximage,
			0, 0, 0, 0, DISP_WIDTH, DISP_HEIGHT);

	shift -= n;
}

void
render(int page, attr_char *buffer, int row)
{
	ushort *p = ximgdata + 48 + 45 * DISP_WIDTH;
	int i;

//	if (page != draw_page)
//		return;

	if (shift > 0) {
		bump(shift, FALSE);
		draw_tspaces(ximgdata + 48 + (45 + (sh_last * CELL_HEIGHT))
			* DISP_WIDTH, 34);
	}

	if (row < 0)
		for (i = 0; i < NUM_ROWS; i++)
			draw_row(ximgdata + 48 + (45 + i * CELL_HEIGHT)
				 * DISP_WIDTH, buffer + i * NUM_COLS);
	else
		draw_row(ximgdata + 48 + (45 + row * CELL_HEIGHT)
			 * DISP_WIDTH, buffer + row * NUM_COLS);

	XPutImage(display, window, gc, ximage,
		0, 0, 0, 0, DISP_WIDTH, DISP_HEIGHT);
}

void
roll_up(int page, attr_char *buffer, int first_row, int last_row)
{
	ushort *p = ximgdata + 45 * DISP_WIDTH;
	int i;

//	if (page != draw_page)
//		return;

#if 1
	sh_first = first_row;
	sh_last = last_row;
	shift = 26;
	bump(step, FALSE);

	p += ((last_row * CELL_HEIGHT) + CELL_HEIGHT - step) * DISP_WIDTH;

	for (i = 0; i < DISP_WIDTH * step; i++)
		p[i] = CHROMAKEY;
#else
	memmove(p + first_row * CELL_HEIGHT * DISP_WIDTH,
		p + (first_row + 1) * CELL_HEIGHT * DISP_WIDTH,
		(last_row - first_row) * CELL_HEIGHT * DISP_WIDTH * 2);

	draw_tspaces(ximgdata + 48 + (45 + (last_row * CELL_HEIGHT))
		* DISP_WIDTH, 34);
#endif
	XPutImage(display, window, gc, ximage,
		0, 0, 0, 0, DISP_WIDTH, DISP_HEIGHT);
}

static void
xevent(void)
{
	while (XPending(display)) {
		XNextEvent(display, &event);

		switch (event.type) {
		case KeyPress:
		{
			switch (XLookupKeysym(&event.xkey, 0)) {
			case 'q':
			case 'c':
				exit(EXIT_SUCCESS);
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

	usleep(40000);
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
		fprintf(stderr, "Can only run at colour depth 16\n");
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
	XStoreName(display, window, "Caption Test");

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
	dispatch_caption(&caption, buf, FALSE);

	xevent();
}

static void
print(char *s)
{
	unsigned char buf[2];

	for (; s[0] && s[1]; s += 2) {
		buf[0] = odd(s[0]);
		buf[1] = odd(s[1]);
		dispatch_caption(&caption, buf, FALSE);
	}

	if (s[0]) {
		buf[0] = odd(s[0]);
		buf[1] = 0x80;
		dispatch_caption(&caption, buf, FALSE);
	}

	xevent();
}

static void
cmd(unsigned int n)
{
	unsigned char buf[2];

	buf[0] = odd(n >> 8);
	buf[1] = odd(n & 0x7F);

	dispatch_caption(&caption, buf, FALSE);

	xevent();
}

enum {
	white, green, red, yellow, blue, cyan, magenta, black
};

#define italic 7
#define underline 1
#define opaque 0
#define semi_transp 1

#define BACKG(bg, t)		(cmd(0x2000), cmd(0x1020 + ((ch & 1) << 11) + (bg << 1) + t))
#define PREAMBLE(r, fg, u)	cmd(0x1040 + ((ch & 1) << 11) + ((r & 14) << 7) + ((r & 1) << 5) + (fg << 1) + u)
#define INDENT(r, fg, u)	cmd(0x1050 + ((ch & 1) << 11) + ((r & 14) << 7) + ((r & 1) << 5) + ((fg / 4) << 1) + u)
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
		xevent();
}

int
main(int ac, char **av)
{
	int ch = 0;
	int i;

	if (!init_window(ac, av))
		exit(EXIT_FAILURE);

	reset_caption(&caption);

	print(" HELLO WORLD! ");
	PAUSE(30);

	ch = 4;
	TEXT_RESTART;
	print("Character set - Text 1");
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
	print("Special: ");
	for (i = 0; i <= 15; i++) {
		SPECIAL_CHAR(i);
	}
	CR;
	print("DONE - Text 1 ");
	PAUSE(50);

	ch = 5;
	TEXT_RESTART;
	print("Styles - Text 2");
	CR; CR;
	MIDROW(white, 0); print("WHITE"); CR;
	MIDROW(red, 0); print("RED"); CR;
	MIDROW(green, 0); print("GREEN"); CR;
	MIDROW(blue, 0); print("BLUE"); CR;
	MIDROW(yellow, 0); print("YELLOW"); CR;
	MIDROW(cyan, 0); print("CYAN"); CR;
	MIDROW(magenta, 0); print("MAGENTA"); BLACK(0); CR;
	BACKG(white, opaque); print("WHITE"); BACKG(black, opaque); CR;
	BACKG(red, opaque); print("RED"); BACKG(black, opaque); CR;
	BACKG(green, opaque); print("GREEN"); BACKG(black, opaque); CR;
	BACKG(blue, opaque); print("BLUE"); BACKG(black, opaque); CR;
	BACKG(yellow, opaque); print("YELLOW"); BACKG(black, opaque); CR;
	BACKG(cyan, opaque); print("CYAN"); BACKG(black, opaque); CR;
	BACKG(magenta, opaque); print("MAGENTA"); BACKG(black, opaque); CR;
	TRANSP;
	print(" TRANSPARENT BACKGROUND ");
	BACKG(black, opaque); CR;
	MIDROW(white, 0); print("DONE - Text 2 ");
	PAUSE(50);

	ch = 0;
	ROLL_UP(4);
	ERASE_DISPLAY;
	print(" HELLO AGAIN! "); PAUSE(20);
	print("I'M YOUR ROLL-UP "); CR; PAUSE(40);
	print(" CAPTION! "); CR; PAUSE(30);
	print(" >> MARY HAD A LITTLE LAMB. "); CR; PAUSE(30);
	print(" HER SENTENTENCE WAS MUCH TOO LONG TO FIT IN A SINGLE LINE. "); CR; PAUSE(30);
	print(" >> EAT MY SHORT"); PAUSE(20);
	print(" DOUBLES "); CR; PAUSE(30);
	print(" DONE - Caption 1 ");
	PAUSE(30);

	ch = 1;
	ROLL_UP(3);
	ERASE_DISPLAY;
	PREAMBLE(2, yellow, 0);
	INDENT(0, 10, 0); print(" FOO "); CR;
	INDENT(0, 10, 0); print(" MIKE WAS HERE "); CR; PAUSE(20);
	PREAMBLE(5, red, 0);
	INDENT(0, 13, 0); print(" AND NOW... "); CR;
	INDENT(0, 13, 0); print(" HE'S HERE "); CR; PAUSE(20);
	PREAMBLE(12, red, 0);
	MIDROW(cyan, 0);
	print("01234567890123456789012345678901234567890123456789"); CR;
	MIDROW(white, 0);
	print(" DONE - Caption 2 "); CR;
	print(" Not impressed. Hit [Q]");	
	PAUSE(30);

	for (;;)
		xevent();

	exit(EXIT_SUCCESS);
}

#endif
