/*
 *  libzvbi - Events
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998,1999 Edgar Toernig (froese@gmx.de)
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

/* $Id: event.h,v 1.5.2.11 2006-05-07 06:04:58 mschimek Exp $ */

#ifndef EVENT_H
#define EVENT_H

/* TO DO */

#include <time.h>		/* time_t */
#include "bcd.h"		/* vbi3_pgno, vbi3_subno */
#include "network.h"		/* vbi3_network */
#include "link.h"		/* vbi3_link */
#ifndef ZAPPING8
#  include "aspect_ratio.h"	/* vbi3_aspect_ratio */
#  include "program_info.h"	/* vbi3_program_info */
#endif
#include "page.h"		/* vbi3_char */
#include "pdc.h"		/* vbi3_program_id */

VBI3_BEGIN_DECLS

/**
 * @ingroup Event
 * @name Event types.
 * @{
 */

typedef unsigned int vbi3_event_mask;

/**
 * @anchor VBI3_EVENT_
 * No event.
 */
#define VBI3_EVENT_NONE 0x0000
/**
 * The decoding context is about to be closed. delete() functions
 * send this event to clean up event handlers.
 */
#define	VBI3_EVENT_CLOSE 0x0001
/**
 * The decoding context has been reset. reset() functions send
 * this event. Note a @c VBI3_EVENT_NETWORK may follow.
 */
#define VBI3_EVENT_RESET 0x0002
/**
 * The decoder received and cached another Teletext page
 * designated by ev.ttx_page.pgno and ev.ttx_page.subno.
 */
#define	VBI3_EVENT_TTX_PAGE 0x0004
/**
 * A Closed Caption page has changed and needs visual update.
 * The "page", actually CC channel, is designated by ev.caption.pgno,
 * see vbi3_pgno for details.
 */
#define VBI3_EVENT_CC_PAGE 0x0008
/**
 * XXX which other events may follow? TOP_CHANGE, ...?
 */
#define	VBI3_EVENT_NETWORK 0x0010
/**
 * @anchor VBI3_EVENT_TRIGGER
 * 
 * Triggers are sent by broadcasters to start some action on the
 * user interface of modern TVs. Until libzvbi implements all ;-) of
 * WebTV and SuperTeletext the information available are program
 * related (or unrelated) URLs, short messages and Teletext
 * page links.
 * 
 * This event is sent when a trigger has fired, ev.trigger
 * points to a vbi3_link structure describing the link in detail.
 * The structure must be read only.
 */
#define	VBI3_EVENT_TRIGGER 0x0020
/**
 * @anchor VBI3_EVENT_ASPECT
 *
 * The vbi decoder received new information (potentially from
 * PAL WSS, NTSC XDS or EIA-J CPR-1204) about the program
 * aspect ratio. ev.ratio is a pointer to a vbi3_ratio structure.
 * The structure must be read only.
 */
#define	VBI3_EVENT_ASPECT 0x0040
/**
 * We have new information about the current or next program.
 * ev.prog_info is a vbi3_program_info pointer (due to size), read only.
 *
 * Preliminary.
 *
 * XXX Info from Teletext not implemented yet.
 * XXX Change to get_prog_info. network ditto?
 */
#define	VBI3_EVENT_PROG_INFO 0x0080

/**
 * New information about Closed Caption or Teletext pages is available,
 * for example a subtitle page is in transmission now.
 */
#define VBI3_EVENT_PAGE_TYPE 0x0100
/**
 * New information for TOP navigation is available, i.e. the
 * table of page titles changed. Typically clients receive this
 * event once or maybe a few times after a channel change, when
 * the decoder first receives uncached TOP data.
 */
#define VBI3_EVENT_TOP_CHANGE 0x0200
/**
 * A new network local time (Teletext packet 8/30 format 1)
 * has been transmitted. ev.local_time.time contains UTC,
 * ev.local_time.gmtoff the local time offset in seconds east
 * of UTC. To get the local time of the network broadcasting
 * this add gmtoff to time.
 */
#define VBI3_EVENT_LOCAL_TIME 0x0400
/**
 * A new program ID (VPS, PDC or XDS impulse capture ID) has
 * been transmitted. ev.prog_id points to a vbi3_program_id
 * structure with details.
 */
#define VBI3_EVENT_PROG_ID 0x0800
/**
 * A network is about to be removed from the cache.
 * Note vbi3_event.timestamp will not be set.
 */
#define VBI3_EVENT_REMOVE_NETWORK 0x1000

/* TODO */
#define VBI3_EVENT_CC_RAW 0x400000

/* TODO */
#define VBI3_EVENT_TIMER 0x800000

/** @} */

/**
 * @brief Teletext page flags.
 */
/* Note the bit positions are defined by the Teletext standard,
   don't change. */
typedef enum {
	/**
	 * The page header is suitable for a rolling display
	 * and clock updates.
	 */
	VBI3_ROLL_HEADER	= 0x000001,
	/**
	 * Newsflash page.
	 */
	VBI3_NEWSFLASH		= 0x004000,
	/**
	 * Subtitle page.
	 */
	VBI3_SUBTITLE		= 0x008000,
	/**
	 * The page has changed since the last transmission. This
	 * flag is under editorial control, not set by the
	 * vbi3_teletext_decoder. See EN 300 706, Section A.2 for details.
	 */
	VBI3_UPDATE		= 0x020000,
	/**
	 * When this flag is set, pages are in serial transmission. When
	 * cleared, pages are in series only within their magazine (pgno &
	 * 0x0FF). In the latter case a rolling header should display only
	 * pages from the same magazine.
	 */
	VBI3_SERIAL		= 0x100000,
} vbi3_ttx_page_flags; 

typedef enum {
	VBI3_CHAR_UPDATE	= (1 << 0),
	VBI3_WORD_UPDATE	= (1 << 1), /* XXX not implemented yet */
	VBI3_ROW_UPDATE		= (1 << 2),
	VBI3_PAGE_UPDATE	= (1 << 3), /* XXX not implemented yet */
	VBI3_START_ROLLING	= (1 << 4),
} vbi3_cc_page_flags;

/**
 * @ingroup Event
 * @brief Event union.
 */
typedef struct vbi3_event {
	/** Event type, one of the VBI3_EVENT_ symbols. */
	vbi3_event_mask		type;

	/** The network this event refers to. */
	const vbi3_network *	network;

	/** Capture time passed to the decoder when the event occured. */
	double			timestamp;

	union {
		struct {
			vbi3_pgno		pgno;
			vbi3_subno		subno;
			vbi3_ttx_page_flags	flags;
	        }			ttx_page;
		struct {
			vbi3_pgno		channel;
			vbi3_cc_page_flags	flags;
		}			caption;
		struct {
			vbi3_pgno		channel;
			unsigned int		row;
			const vbi3_char *	text;
			unsigned int		length;
		}			cc_raw;
		const vbi3_link *	trigger;
#ifndef ZAPPING8
		const vbi3_aspect_ratio *aspect;
		const vbi3_program_info *prog_info;
#endif
		struct { }		page_type;
		struct { }		top_change;
		struct {
			time_t			time;
			int			gmtoff;
		}			local_time;
		const vbi3_program_id *	prog_id;
		struct { }		remove_network;

	}			ev;
} vbi3_event;

typedef vbi3_bool
vbi3_event_cb			(const vbi3_event *	event,
				 void *			user_data);

VBI3_END_DECLS

#endif /* EVENT_H */
