/*
 *  libzvbi - Closed Caption decoder
 *
 *  Copyright (C) 2000-2005 Michael H. Schimek
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

/* $Id: caption_decoder.c,v 1.1.2.1 2006-05-07 06:04:58 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "hamm.h"
#include "page-priv.h"
#include "caption_decoder-priv.h"

#define XDS_EVENTS 0 /* TODO */
#define CAPTION_EVENTS (VBI3_EVENT_CC_PAGE |				\
			VBI3_EVENT_CC_RAW |				\
			VBI3_EVENT_PAGE_TYPE)

/*
    Resources:

    Code of Federal Regulations, Title 47 Telecommunication,
    Section 15.119 "Closed caption decoder requirements for analog
    television receivers".

    EIA 608-B "Recommended Practice for Line 21 Data Service"
    http://global.ihs.com for those with money to burn.

    Video Demystified
    http://www.video-demystified.com

    Related documents:

    47 CFR Section 15.120 "Program blocking technology requirements
    for television receivers".

    "Analog television receivers will receive program ratings
    transmitted pursuant to industry standard EIA-744 "Transport of
    Content Advisory Information Using Extended Data Service (XDS)"
    and EIA-608 "Recommended Practice for Line 21 Data Service". This
    incorporation by reference was approved by the Director of the
    Federal Register in accordance with 5 U.S.C. 522(a) and 1 CFR
    Part 51. [...] Copies of EIA-744 and EIA-608 may be obtained from:
    Global Engineering Documents, 15 Inverness Way East, Englewood,
    CO 80112-5704. Copies of EIA-744 may be inspected during normal
    business hours at the following locations: Federal Communications
    Commission, 2000 M Street, NW, Technical Information Center (Suite
    230), Washington, DC, or the Office of the Federal Register, 800
    North Capitol Street, NW, Suite 700, Washington, DC."

    47 CFR Section 15.122 "Closed caption decoder requirements for
    digital television receivers and converter boxes".

    "Digital television receivers and tuners must be capable of
    decoding closed captioning information that is delivered pursuant
    to the industry standard EIA-708-B, "Digital Television (DTV)
    Closed Captioning," Electronics Industries Association (1999). This
    incorporation by reference was approved by the Director of the
    Federal Register in accordance with 5 U.S.C. 552(a) and 1 CFR
    part 51. Digital television manufacturers may wish to view EIA-708-B
    in its entirety. Copies of EIA-708-B may be obtained from: Global
    Engineering Documents, 15 Inverness Way East, Englewood, CO
    80112-5704, http://www.global.ihs.com/. Copies of EIA-708-B may be
    inspected during regular business hours at the following locations:
    Federal Communications Commission, 445 12th Street, S.W., Washington,
    D.C. 20554, or the Office of the Federal Register, 800 N. Capitol
    Street, N.W., Washington, D.C."
*/

/* Closed Caption decoder ****************************************************/

#define ALL_DIRTY ((1 << MAX_ROWS) - 1)

#define WORD_UPDATE (VBI3_CHAR_UPDATE | VBI3_WORD_UPDATE)
#define ROW_UPDATE (WORD_UPDATE | VBI3_ROW_UPDATE)
#define PAGE_UPDATE (ROW_UPDATE | VBI3_PAGE_UPDATE)

static const vbi3_char
transparent_space [2] = {
	{
		/* Caption channels. */
		.attr			= 0,
		.size			= VBI3_NORMAL_SIZE,
		.opacity		= VBI3_TRANSPARENT_SPACE,
		.foreground		= VBI3_WHITE,
		.background		= VBI3_BLACK,
		.drcs_clut_offs		= 0,
		.unicode		= 0x0020
	}, {
		/* Text channels. */
		.attr			= 0,
		.size			= VBI3_NORMAL_SIZE,
		.opacity		= VBI3_OPAQUE,
		.foreground		= VBI3_WHITE,
		.background		= VBI3_BLACK,
		.drcs_clut_offs		= 0,
		.unicode		= 0x0020
	}
};

#define TRANSPARENT_SPACE(cd, ch)					\
	transparent_space[((ch)						\
	  >= &(cd)->cc.channel[VBI3_CAPTION_T1 - VBI3_CAPTION_CC1])]

static const vbi3_color
color_mapping [8] = {
	VBI3_WHITE, VBI3_GREEN, VBI3_BLUE, VBI3_CYAN,
	VBI3_RED, VBI3_YELLOW, VBI3_MAGENTA, VBI3_BLACK
};

#define IS_OPAQUE(c)							\
	(VBI3_TRANSPARENT_SPACE != (c).opacity && 0x0020 != (c).unicode)

/**
 */
void
vbi3_cc_channel_stat_destroy	(vbi3_cc_channel_stat *	cs)
{
	assert (NULL != cs);

	CLEAR (*cs);
}

/**
 */
void
vbi3_cc_channel_stat_init	(vbi3_cc_channel_stat *	cs)
{
	assert (NULL != cs);

	CLEAR (*cs);

	cs->page_type		= VBI3_UNKNOWN_PAGE;
	cs->last_received	= 0.0;
}

vbi3_bool
vbi3_caption_decoder_get_cc_channel_stat
				(vbi3_caption_decoder *	cd,
				 vbi3_cc_channel_stat *	cs,
				 vbi3_pgno		channel)
{
	caption_channel *ch;

	assert (NULL != cd);
	assert (NULL != cs);

	if (channel < VBI3_CAPTION_CC1 || channel > VBI3_CAPTION_T4)
		return FALSE;

	ch = &cd->cc.channel[channel - VBI3_CAPTION_CC1];

	CLEAR (*cs);

	cs->channel = channel;

	cs->page_type = (channel >= VBI3_CAPTION_T1) ?
		VBI3_NORMAL_PAGE : VBI3_SUBTITLE_PAGE;

	cs->caption_mode = ch->mode;

	/* TODO language code */

	cs->last_received = ch->last_timestamp;

	return TRUE;
}

/* Add solid spaces for legibility, according to Sec. 15.119 (d)(1),
   (f)(1)(vii), (f)(1)(viii), (f)(2)(iii), (f)(3)(ii). */
static __inline__ void
copy_with_padding		(vbi3_char *		dcp,
				 const vbi3_char *	scp,
				 vbi3_char		ts,
				 unsigned int		dirty)
{
	unsigned int rowct;

	for (rowct = MAX_ROWS; rowct > 0; --rowct) {
		if (unlikely (dirty & 1)) {
			unsigned int colct;

			dcp[0] = ts;
			dcp[1] = scp[0];
			dcp[2] = scp[1];

			if (IS_OPAQUE (scp[0])) {
				dcp[0].opacity = VBI3_OPAQUE;
				dcp[0].background = scp[0].background;
				
				if (VBI3_OPAQUE != scp[1].opacity)
					dcp[2].opacity = VBI3_OPAQUE;
			}
			
			for (colct = MAX_COLUMNS - 2; colct > 0; --colct) {
				dcp[3] = scp[2];

				if (IS_OPAQUE (scp[1])) {
					if (VBI3_OPAQUE != scp[0].opacity) {
						dcp[1].opacity = VBI3_OPAQUE;
					}

					if (VBI3_OPAQUE != scp[2].opacity) {
						dcp[3].opacity = VBI3_OPAQUE;
						dcp[3].background =
							scp[1].background;
					}
				}

				++scp;
				++dcp;
			}

			dcp[3] = ts;

			if (IS_OPAQUE (scp[1])) {
				if (VBI3_OPAQUE != scp[0].opacity)
					dcp[1].opacity = VBI3_OPAQUE;

				dcp[3].opacity = VBI3_OPAQUE;
				dcp[3].background = scp[1].background;
			}

			scp += 2;
			dcp += 4;
		} else {
			vbi3_char *end;

			end = dcp + MAX_COLUMNS + 2;

			while (dcp < end)
				*dcp++ = ts;

			scp += MAX_COLUMNS;
		}

		dirty >>= 1;
	}
}

/**
 * DOCUMENT ME
 */
vbi3_page *
vbi3_caption_decoder_get_page_va_list
				(vbi3_caption_decoder *	cd,
				 vbi3_pgno		channel,
				 va_list		format_options)
{
	/* This map corresponds to enum vbi3_color names, NOT closed
	   caption colors. We could do without color_mapping[] but it's
	   better to use the same color indices everywhere. */
	static const vbi3_rgba default_color_map [8] = {
		VBI3_RGBA (0x00, 0x00, 0x00), VBI3_RGBA (0xFF, 0x00, 0x00),
		VBI3_RGBA (0x00, 0xFF, 0x00), VBI3_RGBA (0xFF, 0xFF, 0x00),
		VBI3_RGBA (0x00, 0x00, 0xFF), VBI3_RGBA (0xFF, 0x00, 0xFF),
		VBI3_RGBA (0x00, 0xFF, 0xFF), VBI3_RGBA (0xFF, 0xFF, 0xFF)
	};
	vbi3_color option_foreground;
	vbi3_color option_background;
	vbi3_bool option_row_change;
	vbi3_page *pg;
	vbi3_page_priv *pgp;
	const vbi3_character_set *cs;
	caption_channel *ch;
	unsigned int n;
	vbi3_char ts;

	assert (NULL != cd);

	if (channel < VBI3_CAPTION_CC1 || channel > VBI3_CAPTION_T4)
		return NULL;

	ch = &cd->cc.channel[channel - VBI3_CAPTION_CC1];

	/* This clears pgp and initializes pg->priv and pg->ref_count. */
	if (!(pg = vbi3_page_new ()))
		return NULL;

	pgp = pg->priv;

	pgp->cn = cache_network_ref (cd->network);

	pgp->pg.cache = cd->cache;
	pgp->pg.network = &cd->network->network;

	pgp->pg.pgno = channel;

	pgp->pg.rows = MAX_ROWS;
	pgp->pg.columns = MAX_COLUMNS;

	assert (N_ELEMENTS (pgp->pg.text)
		>= MAX_ROWS * (MAX_COLUMNS + 2));

	/* Must be initialized for export. */
	cs = vbi3_character_set_from_code (0);
	pgp->char_set[0] = cs;
	pgp->char_set[1] = cs;

	option_foreground = -1;
	option_background = -1;
	option_row_change = FALSE;

	for (;;) {
		vbi3_format_option option;

		option = va_arg (format_options, vbi3_format_option);

		switch (option) {
		case VBI3_PADDING:
			/* Turns the text  [foo]   [bar baz]
			   into           [ foo ] [ bar baz ] */
			pgp->pg.columns =
				va_arg (format_options, vbi3_bool) ?
				MAX_COLUMNS + 2 : MAX_COLUMNS;
			break;

		case VBI3_ROW_CHANGE:
			option_row_change = va_arg (format_options, vbi3_bool);
			break;

		case VBI3_DEFAULT_FOREGROUND:
			pgp->pg.color_map[8] =
				va_arg (format_options, vbi3_rgba);

			if (VBI3_RGBA (0xFF, 0xFF, 0xFF)
			    & (default_color_map[VBI3_WHITE]
			       ^ pgp->pg.color_map[8]))
				option_foreground = VBI3_WHITE;
			break;

		case VBI3_DEFAULT_BACKGROUND:
			pgp->pg.color_map[9] =
				va_arg (format_options, vbi3_rgba);

			if (VBI3_RGBA (0xFF, 0xFF, 0xFF)
			    & (default_color_map[VBI3_BLACK]
			       ^ pgp->pg.color_map[9]))
				option_background = VBI3_BLACK;
			break;

		default:
			option = 0;
			break;
		}

		if (0 == option)
			break;
	}

	ts = TRANSPARENT_SPACE (cd, ch);

	if (option_background == ts.background)
		ts.background = 9;

	n = ch->displayed_buffer;
	if (option_row_change && VBI3_CAPTION_MODE_POP_ON != ch->mode)
		n = 2;

	if (ch->dirty[n] <= 0) {
		vbi3_char *cp;
		vbi3_char *end;

		cp = pgp->pg.text;
		end = pgp->pg.text + pgp->pg.rows * pgp->pg.columns;

		while (cp < end)
			*cp++ = ts;
	} else {
		if (pgp->pg.columns > MAX_COLUMNS) {
			copy_with_padding (pgp->pg.text,
					   ch->buffer[n][FIRST_ROW],
					   ts, ch->dirty[n]);
		} else {
			assert (sizeof (pgp->pg.text)
				>= sizeof (ch->buffer[0]));
			memcpy (pgp->pg.text,
				ch->buffer[n][FIRST_ROW],
				sizeof (ch->buffer[0]));
		}

		if ((int) option_foreground >= 0
		    || (int) option_background >= 0) {
			vbi3_char *cp;
			vbi3_char *end;

			cp = pgp->pg.text;
			end = pgp->pg.text + pgp->pg.rows * pgp->pg.columns;

			while (cp < end) {
				if (option_foreground == cp->foreground)
					cp->foreground = 8;

				if (option_background == cp->background)
					cp->background = 9;

				++cp;
			}
		}
	}

	pgp->pg.screen_color = ts.background;
	pgp->pg.screen_opacity = ts.opacity;

	assert (sizeof (pgp->pg.color_map) >= sizeof (default_color_map));
	memcpy (pgp->pg.color_map, default_color_map,
		sizeof (default_color_map));

	return pg;
}

/**
 * DOCUMENT ME
 */
vbi3_page *
vbi3_caption_decoder_get_page	(vbi3_caption_decoder *	cd,
				 vbi3_pgno		channel,
				 ...)
{
	vbi3_page *pg;
	va_list format_options;

	va_start (format_options, channel);

	pg = vbi3_caption_decoder_get_page_va_list
		(cd, channel, format_options);

	va_end (format_options);

	return pg;
}

static void
send_event			(vbi3_caption_decoder *	cd,
				 caption_channel *	ch,
				 unsigned int		event_type,
				 vbi3_cc_page_flags	flags)
{
	vbi3_event e;

	e.type = event_type;
	e.network = &cd->network->network;
	e.timestamp = cd->timestamp;

	e.ev.caption.channel = (ch - cd->cc.channel) + VBI3_CAPTION_CC1;
	e.ev.caption.flags = flags;

	_vbi3_event_handler_list_send (&cd->handlers, &e);

	cd->cc.event_pending = NULL;
}

static void
row_change_snapshot		(caption_channel *	ch)
{
	unsigned int n;

	/* Copy displayed buffer to row-change-snapshot buffer
	   (when a row is complete) for VBI3_ROW_CHANGE formatting option. */

	n = ch->displayed_buffer;

	if (ch->dirty[n] > 0)
		memcpy (ch->buffer[2], ch->buffer[n], sizeof (ch->buffer[2]));

	ch->dirty[2] = ch->dirty[n];
}

static void
send_raw_event			(vbi3_caption_decoder *	cd,
				 caption_channel *	ch,
				 unsigned int		first_row,
				 unsigned int		last_row)
{
	vbi3_char buffer[MAX_COLUMNS + 1];
	vbi3_event e;
	unsigned int row;

	e.type = VBI3_EVENT_CC_RAW;
	e.network = &cd->network->network;
	e.timestamp = cd->timestamp;
	e.ev.cc_raw.channel = (ch - cd->cc.channel) + VBI3_CAPTION_CC1;
	e.ev.cc_raw.text = buffer;
	e.ev.cc_raw.length = MAX_COLUMNS;

	CLEAR (buffer[MAX_COLUMNS]);

	for (row = first_row; row <= last_row; ++row) {
		const vbi3_char *text;
		unsigned int column;

		text = ch->buffer[ch->displayed_buffer][row];

		for (column = FIRST_COLUMN; column <= LAST_COLUMN; ++column)
			if (VBI3_TRANSPARENT_SPACE != text[column].opacity)
				break;

		if (column > LAST_COLUMN)
			continue;

		memcpy (buffer, text, MAX_COLUMNS * sizeof (*text));

		e.ev.cc_raw.row = row;

		_vbi3_event_handler_list_send (&cd->handlers, &e);
	}
}

/* NOTE ch is invalid if CHANNEL_UNKNOWN == cd->cc.curr_ch_num. */
static caption_channel *
switch_channel			(vbi3_caption_decoder *	cd,
				 caption_channel *	ch,
				 vbi3_pgno		new_ch_num)
{
	if (CHANNEL_UNKNOWN != cd->cc.curr_ch_num
	    && VBI3_CAPTION_MODE_UNKNOWN != ch->mode) {
		/* Force a display update if we do not send events
		   on every display change. */
	}

	cd->cc.curr_ch_num = new_ch_num;

	return &cd->cc.channel[new_ch_num - VBI3_CAPTION_CC1];
}

static void
reset_curr_attr			(vbi3_caption_decoder *	cd,
				 caption_channel *	ch)
{
	ch->curr_attr = TRANSPARENT_SPACE (cd, ch);
	ch->curr_attr.opacity = VBI3_OPAQUE;
}

static void
set_cursor			(caption_channel *	ch,
				 unsigned int		column,
				 unsigned int		row)
{
	ch->curr_row = row;
	ch->curr_column = column;
}

static void
erase_memory			(vbi3_caption_decoder *	cd,
				 caption_channel *	ch,
				 unsigned int		n)
{
	vbi3_char ts;
	unsigned int i;

	ts = TRANSPARENT_SPACE (cd, ch);

	for (i = 0; i < MAX_ROWS * MAX_COLUMNS; ++i)
		ch->buffer[n][0][i] = ts;

	ch->dirty[n] = 0;
}

static void
put_char			(vbi3_caption_decoder *	cd,
				 caption_channel *	ch,
				 unsigned int		unicode)
{
	unsigned int n;
	unsigned int row;
	unsigned int column;

	n = ch->displayed_buffer ^ (VBI3_CAPTION_MODE_POP_ON == ch->mode);

	/* Execute lazy erase. */
	if (ch->dirty[n] < 0)
		erase_memory (cd, ch, n);

	/* Sec. 15.119 (f)(1)(v), (1)(vi), (2)(ii), (3)(i). */

	row = ch->curr_row;
	column = ch->curr_column;

	if (column < LAST_COLUMN)
		ch->curr_column = column + 1;

	ch->curr_attr.unicode = unicode;
	ch->buffer[n][row][column] = ch->curr_attr;

	assert (sizeof (ch->dirty[0]) * 8 - 1 >= MAX_ROWS);
	ch->dirty[n] |= 1 << row;

	/* Send a display update event when the displayed buffer of
	   the current channel changed, but no more than once for
	   each pair of Closed Caption bytes. */
	if (VBI3_CAPTION_MODE_POP_ON != ch->mode) {
		cd->cc.event_pending = ch;
	}
}

static void
put_transparent_space		(vbi3_caption_decoder *	cd,
				 caption_channel *	ch)
{
	unsigned int n;
	unsigned int row;
	unsigned int column;

	/* Sec. 15.119 (f)(1)(v), (1)(vi), (2)(ii), (3)(i). */

	row = ch->curr_row;
	column = ch->curr_column;

	if (column < LAST_COLUMN)
		ch->curr_column = column + 1;

	n = ch->displayed_buffer ^ (VBI3_CAPTION_MODE_POP_ON == ch->mode);

	/* No event if the buffer will be erased to TRANSPARENT_SPACEs
	   (dirty < 0) or the row still contains only TRANSPARENT_SPACEs. */
	if (ch->dirty[n] > 0 && (ch->dirty[n] & (1 << row))) {
		ch->buffer[n][row][column] = TRANSPARENT_SPACE (cd, ch);

		if (VBI3_CAPTION_MODE_POP_ON != ch->mode) {
			cd->cc.event_pending = ch;
		}
	}
}

static void
move_window			(vbi3_caption_decoder *	cd,
				 caption_channel *	ch,
				 unsigned int		new_base_row)
{
	vbi3_char *text;
	vbi3_char ts;
	unsigned int old_max_rows;
	unsigned int new_max_rows;
	unsigned int copy_rows;
	unsigned int copy_chars;
	unsigned int erase_begin;
	unsigned int erase_end;
	unsigned int n;

	n = ch->displayed_buffer;

	/* No event if the buffer contains all TRANSPARENT_SPACEs. */
	if (ch->dirty[n] <= 0)
		return;

	old_max_rows = ch->curr_row + 1 - FIRST_ROW;
	new_max_rows = new_base_row + 1 - FIRST_ROW;
	copy_rows = MIN (MIN (old_max_rows, new_max_rows), ch->window_rows);
	copy_chars = copy_rows * MAX_COLUMNS;

	text = ch->buffer[n][FIRST_ROW];
	ts = TRANSPARENT_SPACE (cd, ch);

	if (new_base_row < ch->curr_row) {
		erase_begin = (new_base_row + 1) * MAX_COLUMNS;
		erase_end = (ch->curr_row + 1) * MAX_COLUMNS;

		memmove (text + erase_begin - copy_chars,
			 text + erase_end - copy_chars,
			 copy_chars * sizeof (*text));

		ch->dirty[n] >>= ch->curr_row - new_base_row;
	} else {
		erase_begin = FIRST_ROW * MAX_COLUMNS;
		erase_end = (new_base_row + 1) * MAX_COLUMNS - copy_chars;

		memmove (text + erase_end,
			 text + (ch->curr_row + 1) * MAX_COLUMNS - copy_chars,
			 copy_chars * sizeof (*text));

		ch->dirty[n] = (ch->dirty[n] << (new_base_row - ch->curr_row))
			& ALL_DIRTY;
	}

	while (erase_begin < erase_end) {
		text[erase_begin++] = ts;
	}

	send_event (cd, ch, VBI3_EVENT_CC_PAGE, PAGE_UPDATE);
}

static void
preamble_address_code		(vbi3_caption_decoder *	cd,
				 caption_channel *	ch,
				 unsigned int		c1,
				 unsigned int		c2)
{
	static const int row_mapping [16] = {
		/* 0 */ 10,			/* 0x1040 */
		/* 1 */ -1,			/* unassigned */
		/* 2 */ 0, 1, 2, 3,		/* 0x1140 ... 0x1260 */
		/* 6 */ 11, 12, 13, 14,		/* 0x1340 ... 0x1460 */
		/* 10 */ 4, 5, 6, 7, 8, 9	/* 0x1540 ... 0x1760 */
	};
	int row;

	/* Preamble Address Codes	001 crrr  1ri xxxu */

	row = row_mapping[(c1 & 7) * 2 + ((c2 >> 5) & 1)];

	/* Sec. 15.119 (j): No function, reject. */
	if (row < 0)
		return;

	if (c2 & 1)
		ch->curr_attr.attr |= VBI3_UNDERLINE;
	else
		ch->curr_attr.attr &= ~VBI3_UNDERLINE;

	ch->curr_attr.background = VBI3_BLACK;
	ch->curr_attr.opacity = VBI3_OPAQUE;

	if (VBI3_CAPTION_MODE_ROLL_UP == ch->mode) {
		/* Sec. 15.119 (f)(1)(ii). */
		if (row != (int) ch->curr_row) {
			move_window (cd, ch, /* new_base_row */ row);
		}
	}

	/* Sec. 15.119 (d)(1)(i) */
	set_cursor (ch, FIRST_COLUMN, row);

	if (c2 & 0x10) {
		/* Indent. */
		/* Sec. 15.119 (d)(1)(i) */

		ch->curr_column = FIRST_COLUMN + (c2 & 0x0E) * 2;
	} else {
		unsigned int color;

		color = (c2 >> 1) & 7;

		if (7 == color) {
			ch->curr_attr.attr |= VBI3_ITALIC;
			ch->curr_attr.foreground = VBI3_WHITE;
		} else {
	       		ch->curr_attr.attr &= ~VBI3_ITALIC;
			ch->curr_attr.foreground = color_mapping[color];
		}
	}
}

static void
mid_row_code			(vbi3_caption_decoder *	cd,
				 caption_channel *	ch,
				 unsigned int		c2)
{
	unsigned int color;

	/* Mid-Row Codes		001 c001  010 xxxu */

	if (c2 & 1) {
		/* Underline on. XXX correct to underline the space
		   between underlined words? */
		ch->curr_attr.attr &= ~VBI3_FLASH;
	} else {
		/* Sec. 15.119 (h)(1)(iii). */
		ch->curr_attr.attr &= ~(VBI3_FLASH | VBI3_UNDERLINE);
	}

	/* Sec. 15.119 (h)(1)(i): Is a spacing attribute. */
	put_char (cd, ch, 0x0020);

	color = (c2 >> 1) & 7;

	if (7 == color) {
		ch->curr_attr.attr |= VBI3_ITALIC;

		/* Sec. 15.119 (h)(1)(ii): No color change. */
	} else {
		/* Sec. 15.119 (h)(1)(ii). */
		ch->curr_attr.attr &= ~VBI3_ITALIC;

		ch->curr_attr.foreground = color_mapping[color];
	}

	if (c2 & 1)
		ch->curr_attr.attr |= VBI3_UNDERLINE;
}

static void
backspace			(vbi3_caption_decoder *	cd,
				 caption_channel *	ch)
{
	vbi3_char ts;
	unsigned int n;
	unsigned int row;
	unsigned int column;

	/* Backspace			001 c10f  010 0001 */
	/* Sec. 15.119 (f)(1)(vi). */

	/* XXX how should we delete spacing attributes? A simple
	   backspace or undo the attribute change (i.e. store
	   spacing attributes in the buffer[] similar to raw Teletext
	   pages)? */

	column = ch->curr_column;

	if (column <= FIRST_COLUMN)
		return;

	row = ch->curr_row;

	ch->curr_column = --column;

	n = ch->displayed_buffer ^ (VBI3_CAPTION_MODE_POP_ON == ch->mode);

	/* No event if the buffer will be erased to TRANSPARENT_SPACEs
	   (dirty < 0) or the row still contains only TRANSPARENT_SPACEs. */
	if (ch->dirty[n] > 0 && (ch->dirty[n] & (1 << row))) {
		ts = TRANSPARENT_SPACE (cd, ch);

		/* Transparent space has no attributes but background
		   color (when opaque, or when rendered opaque). */
		ts.foreground = ch->curr_attr.foreground;
		ts.background = ch->curr_attr.background;

		ch->buffer[n][row][column] = ts;

		if (VBI3_CAPTION_MODE_POP_ON != ch->mode) {
			send_event (cd, ch, VBI3_EVENT_CC_PAGE,
				    VBI3_CHAR_UPDATE);
		}
	}
}

static void
delete_to_end_of_row		(vbi3_caption_decoder *	cd,
				 caption_channel *	ch)
{
	unsigned int n;
	unsigned int row;

	/* Delete To End Of Row		001 c10f  010 0100 */
	/* Sec. 15.119 (f)(1)(vii). */

	n = ch->displayed_buffer ^ (VBI3_CAPTION_MODE_POP_ON == ch->mode);

	row = ch->curr_row;

	/* No event if the buffer will be erased to TRANSPARENT_SPACEs
	   (dirty < 0) or the row still contains only TRANSPARENT_SPACEs. */
	if (ch->dirty[n] > 0 && (ch->dirty[n] & (1 << row))) {
		vbi3_char ts;
		unsigned int column;

		ts = TRANSPARENT_SPACE (cd, ch);

		for (column = ch->curr_column;
		     column <= LAST_COLUMN; ++column) {
			/* Transparent space with default colors
			   for this row. */
			ch->buffer[n][row][column] = ts;
		}

		if (VBI3_CAPTION_MODE_POP_ON != ch->mode) {
			/* No ROW_UPDATE because row is not complete yet. */
			send_event (cd, ch, VBI3_EVENT_CC_PAGE, WORD_UPDATE);
		}
	}
}

static void
resize_window			(vbi3_caption_decoder *	cd,
				 caption_channel *	ch,
				 unsigned int		n_rows)
{
	vbi3_char *cp;
	vbi3_char *end;
	unsigned int n;

	n = ch->displayed_buffer;

	/* No event if the buffer contains all TRANSPARENT_SPACEs. */
	if (ch->dirty[n] <= 0)
		return;

	/* Nothing to do unless the window shrinks. */
	if (n_rows >= ch->window_rows)
		return;

	n_rows = MIN (ch->curr_row - FIRST_ROW + 1, n_rows);

	cp = ch->buffer[n][FIRST_ROW];
	end = cp + (ch->curr_row + 1 - n_rows) * MAX_COLUMNS;

	if (cp < end) {
		vbi3_char ts;

		ts = TRANSPARENT_SPACE (cd, ch);

		while (cp < end)
			*cp++ = ts;

		send_event (cd, ch, VBI3_EVENT_CC_PAGE, PAGE_UPDATE);
	}
}

static void
roll_up_captions		(vbi3_caption_decoder *	cd,
				 caption_channel *	ch,
				 unsigned int		c2)
{
	unsigned int n_rows;

	/* Roll-Up Captions		001 c10f  010 01xx */
	/* Sec. 15.119 (f)(1). */

	n_rows = (c2 & 7) - 3; /* 2, 3, 4 */

	if (VBI3_CAPTION_MODE_ROLL_UP == ch->mode) {
		/* Sec. 15.119 (f)(1)(iv). */
		resize_window (cd, ch, n_rows);

		ch->window_rows = n_rows;

		/* XXX NBC (sample s4) sends resume-text and roll-up commands
		   to switch between channels T2 and C1 *without* a PAC after
		   roll-up to place the cursor back at the previous column.
		   That makes sense to me but it contradicts Sec. 15.119
		   (f)(1)(ii): "The Roll-up command, in normal practice,
		   will be followed (not necessarily immediately) by a
		   Preamble Address Code indicating the base row and
		   the horizontal indent position. If no Preamble Address
		   Code is received, the base row will default to Row 15 or,
		   if a roll-up caption is currently displayed, to the same
		   base row last received, and the cursor will be placed at
		   Column 1." */
		/* set_cursor (ch, FIRST_COLUMN, ch->curr_row); */
	} else {
		int was_dirty;

		ch->mode = VBI3_CAPTION_MODE_ROLL_UP;
		ch->window_rows = n_rows;

		set_cursor (ch, FIRST_COLUMN, LAST_ROW);

		was_dirty = ch->dirty[ch->displayed_buffer];

		/* Sec. 15.119 (f)(1)(x). */

		/* Lazy erase of displayed and non-displayed buffer. */
		ch->dirty[0] = -1;
		ch->dirty[1] = -1;

		/* Copy displayed buffer to row-change-snapshot buffer. */
		ch->dirty[2] = -1;

		if (was_dirty > 0)
			send_event (cd, ch, VBI3_EVENT_CC_PAGE, PAGE_UPDATE);
	}
}

static void
erase_displayed_memory		(vbi3_caption_decoder *	cd,
				 caption_channel *	ch)
{
	unsigned int n;
	int was_dirty;

	/* Erase Displayed Memory	001 c10f  010 1100 */
	/* Sec. 15.119 (f). */

	n = ch->displayed_buffer;

	was_dirty = ch->dirty[n];

	/* Lazy erase. */
	ch->dirty[n] = -1;

	/* Copy displayed buffer to row-change-snapshot buffer. */
	ch->dirty[2] = -1;

	if (was_dirty > 0) {
		/* We don't send empty rows.
		   send_raw_event (cd, ch, FIRST_ROW, LAST_ROW); */

		send_event (cd, ch, VBI3_EVENT_CC_PAGE, PAGE_UPDATE);
	}
}

static void
carriage_return			(vbi3_caption_decoder *	cd,
				 caption_channel *	ch)
{
	vbi3_char ts;
	unsigned int n_rows;
	unsigned int first_row;
	unsigned int row;
	unsigned int n;
	vbi3_cc_page_flags flags;

	/* Carriage Return		001 c10f  010 1101 */
	/* Sec. 15.119 (f)(1)(iii). */

	n = ch->displayed_buffer;
	row = ch->curr_row;

	if (unlikely (VBI3_CAPTION_MODE_TEXT == ch->mode)) {
		if (LAST_ROW != row) {
			/* No event if the buffer will be erased to
			   TRANSPARENT_SPACEs (dirty < 0) or the row still
			   contains only TRANSPARENT_SPACEs. */
			if (ch->dirty[n] > 0 && (ch->dirty[n] & (1 << row))) {
				row_change_snapshot (ch);
				send_raw_event (cd, ch, row, row);
			}

			set_cursor (ch, FIRST_COLUMN, row + 1);

			return;
		}

		/* No event if the buffer contains all TRANSPARENT_SPACEs. */
		if (ch->dirty[n] <= 0) {
			ch->curr_column = FIRST_COLUMN;
			return;
		}

		n_rows = MAX_ROWS;
	} else {
		/* VBI3_CAPTION_MODE_ROLL_UP == ch->mode. */

		/* No event if the buffer contains all TRANSPARENT_SPACEs. */
		if (ch->dirty[n] <= 0) {
			ch->curr_column = FIRST_COLUMN;
			return;
		}

		n_rows = row - FIRST_ROW + 1;
		n_rows = MIN (n_rows, ch->window_rows);
	}

	row_change_snapshot (ch);

	/* No event if the row contains only TRANSPARENT_SPACEs. */
	if (likely (ch->dirty[n] & (1 << row))) {
		unsigned int column;

		send_raw_event (cd, ch, row, row);

		first_row = row + 1 - n_rows;

		memmove (ch->buffer[n][first_row],
			 ch->buffer[n][first_row + 1],
			 (n_rows - 1) * sizeof (ch->buffer[0][0]));

		ch->dirty[n] >>= 1;

		ts = TRANSPARENT_SPACE (cd, ch);

		for (column = FIRST_COLUMN; column <= LAST_COLUMN; ++column)
			ch->buffer[n][row][column] = ts;
	} else {
		first_row = row + 1 - n_rows;

		memmove (ch->buffer[n][first_row],
			 ch->buffer[n][first_row + 1],
			 (n_rows - 1) * sizeof (ch->buffer[0][0]));

		ch->dirty[n] >>= 1;
	}

	ch->curr_column = FIRST_COLUMN;

	flags = PAGE_UPDATE;
	if (VBI3_CAPTION_MODE_ROLL_UP == ch->mode)
		flags = PAGE_UPDATE | VBI3_START_ROLLING;

	send_event (cd, ch, VBI3_EVENT_CC_PAGE, flags);
}

static void
optional_attributes		(vbi3_caption_decoder *	cd,
				 caption_channel *	ch,
				 unsigned int		c2)
{
	vbi3_char attr;
	unsigned int column;

	attr = ch->curr_attr;

	switch (c2) {
	case 0x21 ... 0x23:
		/* Misc Control Codes, Tab Offset  001 c111  010 00xx */

		column = ch->curr_column + (c2 & 3);
		ch->curr_column = MIN (column, (unsigned int) LAST_COLUMN);

		return;

	case 0x2D:
		/* Optional Attributes		001 c111  010 1101 */
		/* From Video Demystified, not verified. */

		attr.opacity = VBI3_TRANSPARENT_FULL;

		break;

	case 0x2E:
	case 0x2F:
		/* Optional Attributes		001 c111  010 111u */
		/* From Video Demystified, not verified. */


		attr.foreground = VBI3_BLACK;

		if (c2 & 1)
			attr.attr |= VBI3_UNDERLINE;
		else
			attr.attr &= ~VBI3_UNDERLINE;
		
		break;

	default: /* ? */
		/* Sec. 15.119 (i)(1): Ignore. */
		return;
	}

	/* "Background and foreground attribute codes have
	   an automatic backspace for backward compatibility with
	   current decoders. Thus, an attribute must be preceded
	   by a standard space character. Standard decoders display
	   the space and ignore the attribute. Extended decoders
	   display the space, and on receiving the attribute,
	   backspace, then display a space that changes the color
	   and opacity." XXX what if there is no "standard space
	   character" or we're in first column? */

	column = ch->curr_column;

	if (column > FIRST_COLUMN)
		ch->curr_column = column - 1;

	/* This is a spacing attribute. */
	put_char (cd, ch, 0x0020);

	/* XXX I *think* attributes should set-after, not set-at. */
	ch->curr_attr = attr;
}

/* NOTE ch is invalid if CHANNEL_UNKNOWN == cd->cc.curr_ch_num. */
static void
misc_control_code		(vbi3_caption_decoder *	cd,
				 caption_channel *	ch,
				 unsigned int		c2,
				 unsigned int		ch_num0)
{
	/* Misc Control Codes		001 c10f  010 xxxx */

	/* XXX according to Video Demystified f = field. Purpose?
	   Sec. 15.119 does not mention this bit or caption on
	   field 2 at all. */

	switch (c2 & 15) {
	case 0:	
		/* Resume Caption Loading	001 c10f  010 0000 */

		ch = switch_channel (cd, ch, VBI3_CAPTION_CC1 + (ch_num0 & 3));

		ch->mode = VBI3_CAPTION_MODE_POP_ON;

		/* Sec. 15.119 (f)(1)(x): Memory not erased.
		   Sec. 15.119 (f)(2)(iv): Cursor position unchanged. */

		break;

	case 1:
		/* Backspace			001 c10f  010 0001 */

		if (CHANNEL_UNKNOWN == cd->cc.curr_ch_num
		    || VBI3_CAPTION_MODE_UNKNOWN == ch->mode)
			break;

		backspace (cd, ch);

		break;

	case 2: /* reserved (formerly Alarm Off) */
	case 3: /* reserved (formerly Alarm On) */
		/* Sec. 15.119 (i)(1): Ignore. */
		break;

	case 4:
		/* Delete To End Of Row		001 c10f  010 0100 */

		if (CHANNEL_UNKNOWN == cd->cc.curr_ch_num
		    || VBI3_CAPTION_MODE_UNKNOWN == ch->mode)
			break;

		delete_to_end_of_row (cd, ch);

		break;

	case 5:
	case 6:
	case 7:
		/* Roll-Up Captions		001 c10f  010 01xx */

		ch = switch_channel (cd, ch, VBI3_CAPTION_CC1 + (ch_num0 & 3));

		roll_up_captions (cd, ch, c2);

		break;

	case 8:
		/* Flash On			001 c10f  010 1000 */
		/* Sec. 15.119 (h)(1)(i). */

		if (CHANNEL_UNKNOWN == cd->cc.curr_ch_num
		    || VBI3_CAPTION_MODE_UNKNOWN == ch->mode)
			break;

		/* This is a spacing attribute. */
		put_char (cd, ch, 0x0020);

		ch->curr_attr.attr |= VBI3_FLASH;

		break;

	case 9:
		/* Resume Direct Captioning	001 c10f  010 1001 */

		ch = switch_channel (cd, ch, VBI3_CAPTION_CC1 + (ch_num0 & 3));

		/* Sec. 15.119 (f)(1)(x), (2)(vi): Memory not erased. */

		if (VBI3_CAPTION_MODE_UNKNOWN == ch->mode
		    || VBI3_CAPTION_MODE_POP_ON == ch->mode)
			row_change_snapshot (ch);

		ch->mode = VBI3_CAPTION_MODE_PAINT_ON;

		break;

	case 10:
		/* Text Restart			001 c10f  010 1010 */

		ch = switch_channel (cd, ch, VBI3_CAPTION_T1 + (ch_num0 & 3));

		/* Mode is invariably VBI3_CAPTION_MODE_TEXT. */

		set_cursor (ch, FIRST_COLUMN, FIRST_ROW);

		break;

	case 11:
		/* Resume Text Display		001 c10f  010 1011 */

		ch = switch_channel (cd, ch, VBI3_CAPTION_T1 + (ch_num0 & 3));

		/* Mode is invariably VBI3_CAPTION_MODE_TEXT. */

		break;

	case 12:
		/* Erase Displayed Memory	001 c10f  010 1100 */
		/* Sec. 15.119 (f). */

		if (CHANNEL_UNKNOWN == cd->cc.curr_ch_num
		    || VBI3_CAPTION_MODE_UNKNOWN == ch->mode)
			break;

		erase_displayed_memory (cd, ch);

		break;

	case 13:
		/* Carriage Return		001 c10f  010 1101 */

		if (CHANNEL_UNKNOWN == cd->cc.curr_ch_num)
			break;

		switch (ch->mode) {
		case VBI3_CAPTION_MODE_UNKNOWN:
			break;

		case VBI3_CAPTION_MODE_POP_ON:
		case VBI3_CAPTION_MODE_PAINT_ON:
			/* Sec. 15.119 (f)(2)(i), (3)(i): No effect. */
			break;

		case VBI3_CAPTION_MODE_ROLL_UP:
		case VBI3_CAPTION_MODE_TEXT:
			carriage_return (cd, ch);
			break;
		}

		break;

	case 14:
		/* Erase Non-Displayed Memory	001 c10f  010 1110 */
		/* Sec. 15.119 (f)(2)(v). */

		if (CHANNEL_UNKNOWN == cd->cc.curr_ch_num)
			break;

		/* Lazy erase. */
		ch->dirty[ch->displayed_buffer ^ 1] = -1;

		break;

	case 15:
		/* End Of Caption		001 c10f  010 1111 */
		/* Sec. 15.119 (f)(2). */

		ch = switch_channel (cd, ch, VBI3_CAPTION_CC1 + (ch_num0 & 3));

		/* Sec. 15.119 (f)(2). */
		ch->mode = VBI3_CAPTION_MODE_POP_ON;

		/* Swap displayed and non-displayed caption. */
		ch->displayed_buffer ^= 1;

		/* Sec. 15.119 (f)(3)(iv):
		   Does not erase non-displayed memory. */

		/* No event if both buffers are empty. */
		if (ch->dirty[0] > 0 || ch->dirty[1] > 0) {
			row_change_snapshot (ch);
			send_raw_event (cd, ch, FIRST_ROW, LAST_ROW);
			send_event (cd, ch, VBI3_EVENT_CC_PAGE, PAGE_UPDATE);
		}

		break;
	}
}

static void
caption_control_code		(vbi3_caption_decoder *	cd,
				 unsigned int		c1,
				 unsigned int		c2,
				 field_num		f)
{
	unsigned int ch_num0;
	caption_channel *ch;

	if (0)
		fprintf (stderr, "caption_control_code %02x %02x %d\n",
			 c1, c2, f);

	/* Caption / text, field 1 / 2, primary / secondary channel. */
	ch_num0 = (((cd->cc.curr_ch_num - VBI3_CAPTION_CC1) & 4)
		   + f * 2
		   + ((c1 >> 3) & 1));

	ch = &cd->cc.channel[ch_num0];

	if (c2 >= 0x40) {
		/* Preamble Address Codes	001 crrr  1ri xxxu */

		if (CHANNEL_UNKNOWN == cd->cc.curr_ch_num
		    || VBI3_CAPTION_MODE_UNKNOWN == ch->mode)
			return;

		preamble_address_code (cd, ch, c1, c2);

		return;
	}

	switch (c1 & 7) {
	case 0:
	{
		unsigned int color;

		if (c2 & 0x10) {
			/* Sec. 15.119 (i)(1): Ignore. */
		} else {
			/* Optional Attributes		001 c000  010 xxxt */
			/* From Video Demystified; not verified. */

			if (CHANNEL_UNKNOWN == cd->cc.curr_ch_num
			    || VBI3_CAPTION_MODE_UNKNOWN == ch->mode)
				break;

			color = (c2 >> 1) & 7;

			ch->curr_attr.opacity = (c2 & 1) ?
				VBI3_TRANSLUCENT : VBI3_OPAQUE;
			ch->curr_attr.background = color_mapping[color];
		}

		break;
	}

	case 1:
		if (CHANNEL_UNKNOWN == cd->cc.curr_ch_num
		    || VBI3_CAPTION_MODE_UNKNOWN == ch->mode)
			break;

		if (c2 & 0x10) {
			/* Special Characters		001 c001  011 xxxx */

			c2 &= 15;

			if (9 == c2) {
				put_transparent_space (cd, ch);
			} else {
				put_char (cd, ch, vbi3_caption_unicode (c2));
			}
		} else {
			/* Mid-Row Codes		001 c001  010 xxxu */

			mid_row_code (cd, ch, c2);
		}

		break;

	case 2: /* ? */
	case 3: /* ? */
		/* Sec. 15.119 (i)(1): Ignore. */
		break;

	case 4:
	case 5:
		if (c2 & 0x10) {
			/* Sec. 15.119 (i)(1): Ignore. */
		} else {
			misc_control_code (cd, ch, c2, ch_num0);
		}

		break;

	case 6: /* reserved */
		/* Sec. 15.119 (i)(1): Ignore. */
		break;

	case 7:
		if (CHANNEL_UNKNOWN == cd->cc.curr_ch_num
		    || VBI3_CAPTION_MODE_UNKNOWN == ch->mode)
			break;

		optional_attributes (cd, ch, c2);

		break;
	}
}

static vbi3_bool
caption_text			(vbi3_caption_decoder *	cd,
				 caption_channel *	ch,
				 int			c,
				 double			timestamp)
{
	double last_timestamp;

	if (0)
		fprintf (stderr, "caption_text %02x '%c' %f\n",
			 c, c, timestamp);

	if (0 == c) {
		if (VBI3_CAPTION_MODE_UNKNOWN == ch->mode)
			return TRUE;

		/* After x NUL characters (presumably a caption pause),
		   force a display update if we do not send events
		   on every display change. */

		return TRUE;
	}

	if (c < 0x20) {
		/* Parity error or invalid data. */

		if (c < 0 && VBI3_CAPTION_MODE_UNKNOWN != ch->mode) {
			/* Sec. 15.119 (j)(1). */
			put_char (cd, ch, vbi3_caption_unicode (0x7F));
		}

		return FALSE;
	}

	last_timestamp = ch->last_timestamp + 10.0 /* seconds */;

	/* Last activity on this channel. */
	ch->last_timestamp = timestamp;

	if ((cd->handlers.event_mask & VBI3_EVENT_PAGE_TYPE)
	    && timestamp > last_timestamp) {
		vbi3_event e;

		e.type = VBI3_EVENT_PAGE_TYPE;
		e.network = &cd->network->network;
		e.timestamp = timestamp;

		_vbi3_event_handler_list_send (&cd->handlers, &e);
	}

	if (VBI3_CAPTION_MODE_UNKNOWN != ch->mode)
		put_char (cd, ch, vbi3_caption_unicode (c));

	return TRUE;
}

/* ITV decoder ***************************************************************/

static vbi3_bool
itv_text			(vbi3_caption_decoder *	cd,
				 int			c)
{
	if (c >= 0x20) {
		/* Sample s4-NBC omitted CR. */
		if ('<' == c)
			itv_text (cd, 0);

		if (cd->itv.size > sizeof (cd->itv.buffer) - 2)
			cd->itv.size = 0;

		cd->itv.buffer[cd->itv.size++] = c;

		return TRUE;
	} else if (0 != c) {
		/* Parity error or invalid character. */
		cd->itv.size = 0;
		return FALSE;
	}

	cd->itv.buffer[cd->itv.size] = 0;
	cd->itv.size = 0;

#ifndef ZAPPING8
#warning TODO
	/* vbi3_atvef_trigger(vbi, cc->itv_buf);
	   event */
#endif

	return TRUE;
}

static void
itv_control_code		(vbi3_caption_decoder *	cd,
				 unsigned int		c1,
				 unsigned int		c2)
{
	if (c2 >= 0x40) {
		/* Preamble Address Codes	001 crrr  1ri xxxu */

		return;
	}

	switch (c1 & 7) {
	case 4:
	case 5:
		if (c2 & 0x10) {
			break;
		}

		/* Misc Control Codes		001 c10f  010 xxxx */
		/* f ("field"): purpose? */

		switch (c2 & 15) {
		case 0:
			/* Resume Caption Loading	001 c10f  010 0000 */
		case 5:
		case 6:
		case 7:
			/* Roll-Up Captions		001 c10f  010 01xx */
		case 9:
			/* Resume Direct Captioning	001 c10f  010 1001 */
		case 15:
			/* End Of Caption		001 c10f  010 1111 */

			cd->in_itv = FALSE;
			break;

		case 10:
			/* Text Restart		001 c10f  010 1010 */

			cd->itv.size = 0;

			/* Fall through. */

		case 11:
			/* Resume Text Display		001 c10f  010 1011 */

			/* First field, text mode, secondary channel ->
			   VBI3_CAPTION_T2. */
			cd->in_itv = !!(c1 & 0x08);
			break;

		case 13:
			/* Carriage Return		001 c10f  010 1101 */

			if (cd->in_itv)
				itv_text (cd, 0);
			break;

		default:
			break;
		}

		break;

	default:
		break;
	}
}

/* Demultiplexer *************************************************************/

/**
 * @internal
 * @param cd
 * @param buffer Two bytes.
 * @param line ITU-R line number this data originated from.
 * @param timestamp
 *
 * Decode two bytes of Closed Caption data (Caption, XDS, ITV),
 * updating the decoder state accordingly. May send events.
 */
vbi3_bool
vbi3_caption_decoder_feed	(vbi3_caption_decoder *	cd,
				 const uint8_t		buffer[2],
				 unsigned int		line,
				 double			timestamp)
{
	int c1;
	int c2;
	field_num f;
	vbi3_bool all_successful;

	assert (NULL != cd);
	assert (NULL != buffer);

	if (0)
		fprintf (stderr, "caption feed %02x %02x '%c%c' %3d %f\n",
			 buffer[0] & 0x7F, buffer[1] & 0x7F,
			 buffer[0] & 0x7F, buffer[1] & 0x7F, line, timestamp);

	f = FIELD_1;

	switch (line) {
	case 21: /* NTSC */
	case 22: /* PAL/SECAM */
		break;

	case 284: /* NTSC */
		f = FIELD_2;
		break;

	default:
		return FALSE;
	}

	cd->timestamp = timestamp;

// FIXME deferred reset here

	c1 = vbi3_unpar8 (buffer[0]);
	c2 = vbi3_unpar8 (buffer[1]);

	all_successful = TRUE;

	if (FIELD_1 == f) {
		/* First field. Control codes may repeat
		   to assure correct reception. */

		/* According to Sec. 15.119 (2)(i)(4). */

		if (c1 == cd->expect_ctrl[FIELD_1][0]
		    && c2 == cd->expect_ctrl[FIELD_1][1]) {
			/* Already acted upon. */
			goto finish;
		} else if (c1 < 0 && cd->expect_ctrl[FIELD_1][0]
			   && c2 == cd->expect_ctrl[FIELD_1][1]) {
			/* Parity error, probably in repeat control code. */
			goto parity_error;
		}
	} else {
		/* Second field. */

		if (cd->handlers.event_mask & XDS_EVENTS) {
/* TODO			all_successful &=
   				vbi3_xds_demux_feed (&cd->xds.demux, buffer);
*/
		}

		/* XDS bytes are in range 0x01 ... 0x0F (control codes)
		   and 0x40 ... 0x7F (data), must be filtered. */

		switch (c1) {
		case 0x01 ... 0x0E:
			/* XDS packet start or continuation. */
			cd->in_xds = TRUE;
			goto finish;

		case 0x0F:
			/* XDS packet terminator. */
			cd->in_xds = FALSE;
			goto finish;

		case 0x10 ... 0x1F:
			/* Caption control code. */
			cd->in_xds = FALSE;
			break;

		default:
			if (c1 < 0) {
				goto parity_error;
			}

			break;
		}
	}

	switch (c1) {
	case 0x10 ... 0x1F:
		/* Sec. 15.119 (i)(1), (i)(2). */
		if (c2 < 0x20) {
			/* Parity error or invalid control code.
			   Let's hope it repeats. */
			goto parity_error;
		}

		if (cd->handlers.event_mask & VBI3_EVENT_TRIGGER) {
			if (FIELD_1 == f)
				itv_control_code (cd, c1, c2);
		}

		if (cd->handlers.event_mask & CAPTION_EVENTS) {
			caption_control_code (cd, c1, c2, f);

			if (cd->cc.event_pending) {
				send_event (cd, cd->cc.event_pending,
					    VBI3_EVENT_CC_PAGE,
					    VBI3_CHAR_UPDATE);
			}
		}

		cd->expect_ctrl[f][0] = c1;
		cd->expect_ctrl[f][1] = c2;

		break;

	default:
		if (FIELD_1 != f && cd->in_xds)
			break;

		cd->expect_ctrl[f][0] = 0;

		/* Sec. 15.119 (i)(1). */
		if (c1 > 0x00 && c1 < 0x10) {
			c1 = 0;
		}

		if (cd->in_itv) {
			all_successful &= itv_text (cd, c1);
			all_successful &= itv_text (cd, c2);
		}

		if (cd->handlers.event_mask & CAPTION_EVENTS) {
			vbi3_pgno ch_num;
			caption_channel *ch;

			ch_num = cd->cc.curr_ch_num;
			if (CHANNEL_UNKNOWN == ch_num)
				break;

			ch_num = ((ch_num - VBI3_CAPTION_CC1) & 5) + f * 2;
			ch = &cd->cc.channel[ch_num];

			all_successful &= caption_text (cd, ch, c1, timestamp);
			all_successful &= caption_text (cd, ch, c2, timestamp);

			if (cd->cc.event_pending) {
				send_event (cd, cd->cc.event_pending,
					    VBI3_EVENT_CC_PAGE,
					    VBI3_CHAR_UPDATE); 
			}
		}

		break;
	}

 finish:
	cd->error_history = cd->error_history * 2 + all_successful;

	return all_successful;

 parity_error:
	cd->expect_ctrl[f][0] = 0;

	cd->error_history *= 2;

	return FALSE;
}

void
_vbi3_caption_decoder_resync	(vbi3_caption_decoder *	cd)
{
	unsigned int ch_num;

	assert (NULL != cd);

	if (0)
		fprintf (stderr, "caption resync\n");

	for (ch_num = 0; ch_num < MAX_CHANNELS; ++ch_num) {
		caption_channel *ch;

		ch = &cd->cc.channel[ch_num];

		if (ch_num <= 3)
			ch->mode = VBI3_CAPTION_MODE_UNKNOWN;
		else
			ch->mode = VBI3_CAPTION_MODE_TEXT; /* invariable */

		ch->displayed_buffer = 0;

		/* Lazy erase of all buffers. */
		memset (ch->dirty, -1, sizeof (ch->dirty));

		set_cursor (ch, FIRST_COLUMN, LAST_ROW);

		ch->window_rows = 3;

		reset_curr_attr (cd, ch);

		ch->last_timestamp = 0.0;
	}

	cd->cc.curr_ch_num = CHANNEL_UNKNOWN;

	cd->in_xds = FALSE;

	CLEAR (cd->expect_ctrl);

	cd->error_history = 0; /* all failed */
}

/** @internal */
void
cache_network_destroy_caption	(cache_network *	cn)
{
	cn = cn;

	/* TODO */
}

/**
 * @internal
 * @param cn cache_network structure to be initialized.
 *
 * Initializes the Caption fields of a cache_network structure.
 */
void
cache_network_init_caption	(cache_network *	cn)
{
	cn = cn;

	/* TODO */
}

/**
 * @internal
 * @param cd Caption decoder allocated with vbi3_caption_decoder_new().
 * @param cn New network, can be @c NULL if 0.0 != time.
 * @param time Deferred reset when time is greater than
 *   vbi3_caption_decoder_feed() timestamp. Pass a negative number to
 *   cancel a deferred reset, 0.0 to reset immediately.
 *
 * Internal reset function, called via cd->virtual_reset().
 */
static void
internal_reset			(vbi3_caption_decoder *	cd,
				 cache_network *	cn,
				 double			time)
{
	assert (NULL != cd);

	if (0)
		fprintf (stderr, "caption reset %f: %f -> %f\n",
			 cd->timestamp, cd->reset_time, time);

	if (time <= 0.0 /* reset now or cancel deferred reset */
	    || time > cd->reset_time)
		cd->reset_time = time;

	if (0.0 != time) {
		/* Don't reset now. */
		return;
	}

	assert (NULL != cn);

	cache_network_unref (cd->network);
	cd->network = cache_network_ref (cn);

	_vbi3_caption_decoder_resync (cd);

	if (internal_reset == cd->virtual_reset) {
		vbi3_event e;

		e.type		= VBI3_EVENT_RESET;
		e.network	= &cd->network->network;
		e.timestamp	= cd->timestamp;

		_vbi3_event_handler_list_send (&cd->handlers, &e);
	}
}

/**
 * @param cd Caption decoder allocated with vbi3_caption_decoder_new().
 * @param callback Function to be called on events.
 * @param user_data User pointer passed through to the @a callback function.
 * 
 * Removes an event handler from the Caption decoder, if a handler with
 * this @a callback and @a user_data has been registered. You can
 * safely call this function from a handler removing itself or another
 * handler.
 */
void
vbi3_caption_decoder_remove_event_handler
				(vbi3_caption_decoder *	cd,
				 vbi3_event_cb *	callback,
				 void *			user_data)
{
/* TODO
	vbi3_cache_remove_event_handler	(cd->cache,
					 callback,
					 user_data);
*/
	_vbi3_event_handler_list_remove_by_callback (&cd->handlers,
						     callback,
						     user_data);
}

/**
 * @param cd Caption decoder allocated with vbi3_caption_decoder_new().
 * @param event_mask Set of events (@c VBI3_EVENT_) the handler is waiting
 *   for, can be -1 for all and 0 for none.
 * @param callback Function to be called on events by
 *   vbi3_caption_decoder_feed().
 * @param user_data User pointer passed through to the @a callback function.
 * 
 * Adds a new event handler to the Caption decoder. When the @a callback
 * with @a user_data is already registered the function merely changes the
 * set of events it will receive in the future. When the @a event_mask is
 * empty the function does nothing or removes an already registered event
 * handler. You can safely call this function from an event handler.
 *
 * Any number of handlers can be added, also different handlers for the
 * same event which will be called in registration order.
 *
 * @returns
 * @c FALSE of failure (out of memory).
 */
vbi3_bool
vbi3_caption_decoder_add_event_handler
				(vbi3_caption_decoder *	cd,
				 unsigned int		event_mask,
				 vbi3_event_cb *	callback,
				 void *			user_data)
{
	unsigned int cc_mask;
	unsigned int add_mask;
	unsigned int rem_mask;

/* TODO
	if (!vbi3_cache_add_event_handler (cd->cache,
					   event_mask,
					   callback,
					   user_data))
		return FALSE;
*/
	cc_mask = event_mask & (VBI3_EVENT_CLOSE |
				VBI3_EVENT_RESET |
				VBI3_EVENT_CC_PAGE |
				VBI3_EVENT_CC_RAW |
				VBI3_EVENT_NETWORK |
				VBI3_EVENT_TRIGGER |
				VBI3_EVENT_PROG_INFO |
				VBI3_EVENT_LOCAL_TIME |
				VBI3_EVENT_PROG_ID |
				VBI3_EVENT_PAGE_TYPE);

	add_mask = cc_mask & ~cd->handlers.event_mask;
	rem_mask = cd->handlers.event_mask & ~event_mask;

	if (0 == cc_mask) {
		return TRUE;
	}

	if (NULL != _vbi3_event_handler_list_add (&cd->handlers,
						  cc_mask,
						  callback,
						  user_data)) {
		if (add_mask & (VBI3_EVENT_CC_PAGE |
				VBI3_EVENT_CC_RAW |
				VBI3_EVENT_TRIGGER)) {
			/* XXX is this really necessary? */
			_vbi3_caption_decoder_resync (cd);
		}

		return TRUE;
	} else {
/* TODO
		vbi3_cache_remove_event_handler	(cd->cache,
						 callback,
						 user_data);
*/
	}

	return FALSE;
}

/**
 * DOCUMENT ME
 */
vbi3_bool
vbi3_caption_decoder_get_network
				(vbi3_caption_decoder *	cd,
				 vbi3_network *		nk)
{
	assert (NULL != cd);
	assert (NULL != nk);

	if (!cd->network)
		return FALSE;

	return vbi3_network_copy (nk, &cd->network->network);
}

/**
 * DOCUMENT ME
 */
vbi3_cache *
vbi3_caption_decoder_get_cache	(vbi3_caption_decoder *	cd)
{
	assert (NULL != cd);

	if (!cd->cache)
		return NULL;

	return vbi3_cache_ref (cd->cache);
}

/**
 * @param cd Caption decoder allocated with vbi3_caption_decoder_new().
 * @param nk Identifies the new network, can be @c NULL.
 *
 * Resets the caption decoder, useful for example after a channel change.
 * This function sends a @c VBI3_EVENT_RESET.
 *
 * You can pass a vbi3_network structure to identify the new network in
 * advance, before the decoder receives a network ID, if ever.
 */
void
vbi3_caption_decoder_reset	(vbi3_caption_decoder *	cd,
				 const vbi3_network *	nk,
				 vbi3_videostd_set	videostd_set)
{
	cache_network *cn;

	assert (NULL != cd);

	cd->videostd_set = videostd_set;

	cn = _vbi3_cache_add_network (cd->cache, nk, videostd_set);

	cd->virtual_reset (cd, cn, 0.0 /* now */);

	cache_network_unref (cn);
}

/**
 * @internal
 * @param cd Caption decoder to be destroyed.
 *
 * Frees all resources associated with @a cd, except the structure itself.
 * This function sends a @c VBI3_EVENT_CLOSE.
 */
void
_vbi3_caption_decoder_destroy	(vbi3_caption_decoder *	cd)
{
	vbi3_event e;

	assert (NULL != cd);

	e.type		= VBI3_EVENT_CLOSE;
	e.network	= &cd->network->network;
	e.timestamp	= cd->timestamp;

	_vbi3_event_handler_list_send (&cd->handlers, &e);

	_vbi3_event_handler_list_destroy (&cd->handlers);

	cache_network_unref (cd->network);

	/* Delete if we hold the last reference. */

	vbi3_cache_unref (cd->cache);

	CLEAR (*cd);
}

/**
 * @internal
 * @param cd Caption decoder to be initialized.
 * @param ca Cache to be used by this Caption decoder, can be @c NULL.
 *   To allocate a cache call vbi3_cache_new(). Caches have a reference
 *   counter, you can vbi3_cache_unref() after calling this function.
 * @param nk Initial network (see vbi3_caption_decoder_reset()),
 *   can be @c NULL.
 *
 * Initializes a Caption decoder structure.
 *
 * @returns
 * @c FALSE on failure (out of memory).
 */
vbi3_bool
_vbi3_caption_decoder_init	(vbi3_caption_decoder *	cd,
				 vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 vbi3_videostd_set	videostd_set)
{
	cache_network *cn;

	assert (NULL != cd);

	CLEAR (*cd);

	if (ca)
		cd->cache = vbi3_cache_ref (ca);
	else
		cd->cache = vbi3_cache_new ();

	if (!cd->cache)
		return FALSE;

	cd->virtual_reset = internal_reset;

	_vbi3_event_handler_list_init (&cd->handlers);

	cd->videostd_set = videostd_set;

	cn = _vbi3_cache_add_network (cd->cache, nk, videostd_set);

	internal_reset (cd, cn, 0.0 /* now */);

	cache_network_unref (cn);

	return TRUE;
}

static void
internal_delete			(vbi3_caption_decoder *	cd)
{
	assert (NULL != cd);

	_vbi3_caption_decoder_destroy (cd);

	vbi3_free (cd);		
}

/**
 * @param cd Caption decoder context allocated with
 *   vbi3_caption_decoder_new(), can be @c NULL.
 *
 * Frees all resources associated with @a cd.
 * This function sends a @c VBI3_EVENT_CLOSE.
 */
void
vbi3_caption_decoder_delete	(vbi3_caption_decoder *	cd)
{
	if (NULL == cd)
		return;

	assert (NULL != cd->virtual_delete);

	cd->virtual_delete (cd);
}

/**
 * @param ca Cache to be used by this Caption decoder. If @a ca is @c NULL
 *  the function allocates a new cache. To allocate a cache yourself call
 *  vbi3_cache_new(). Caches have a reference counter, you can
 *  vbi3_cache_unref() after calling this function.
 *
 * Allocates a new Close Caption (EIA 608) decoder. Decoded data is
 * available through the following functions:
 * - vbi3_caption_decoder_get_page()
 *
 * To be notified when new data is available call
 * vbi3_caption_decoder_add_event_handler().
 *
 * @returns
 * Pointer to newly allocated Caption decoder which must be
 * freed with vbi3_caption_decoder_delete() when done.
 * @c NULL on failure (out of memory).
 */
vbi3_caption_decoder *
vbi3_caption_decoder_new	(vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 vbi3_videostd_set	videostd_set)
{
	vbi3_caption_decoder *cd;

	if (!(cd = malloc (sizeof (*cd)))) {
		return NULL;
	}

	_vbi3_caption_decoder_init (cd, ca, nk, videostd_set);

	cd->virtual_delete = internal_delete;

	return cd;
}
