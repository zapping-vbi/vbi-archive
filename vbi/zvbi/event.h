/*
 *  libzvbi -- Events
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the 
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
 *  Boston, MA  02110-1301  USA.
 */

/* $Id: event.h,v 1.1.2.1 2008-08-19 10:56:05 mschimek Exp $ */

#ifndef __ZVBI_EVENT_H__
#define __ZVBI_EVENT_H__

#include <sys/time.h>		/* struct timeval */

#include "zvbi/bcd.h"		/* vbi_pgno, vbi_subno */
#include "zvbi/network.h"	/* vbi_network */

VBI_BEGIN_DECLS

/**
 * @ingroup Event
 * @name Event types.
 */
typedef enum {
	/**
	 * @anchor VBI_EVENT_
	 * No event.
	 */
	VBI_EVENT_NONE			= 0,

	/**
	 * The decoding context is about to be closed. delete()
	 * functions send this event to clean up event handlers.
	 */
	VBI_EVENT_CLOSE			= (1 << 0),

	/**
	 * The decoding context has been reset. reset() functions send
	 * this event. Note a @c VBI_EVENT_NETWORK may follow.
	 */
	VBI_EVENT_RESET			= (1 << 1),

	/**
	 * The decoder received and cached a Teletext page
	 * designated by ev.ttx_page.pgno and ev.ttx_page.subno.
	 */
	VBI_EVENT_TTX_PAGE		= (1 << 2)
} vbi_event_mask;

/**
 * @brief Teletext page flags.
 */
/* The values are defined in EN 300 706, don't change. */
typedef enum {
	/**
	 * The page header is suitable for a rolling display (showing
	 * the number of the currently received page while waiting for
	 * the page requested by the user) and clock updates.
	 */
	VBI_ROLL_HEADER		= 0x000001,

	/**
	 * Newsflash page.
	 */
	VBI_NEWSFLASH		= 0x004000,

	/**
	 * Subtitle page.
	 */
	VBI_SUBTITLE		= 0x008000,

	/**
	 * The page has changed since the last transmission. This flag
	 * is under editorial control, not set by the
	 * vbi_ttx_decoder. See EN 300 706 Section A.2 for details.
	 */
	VBI_UPDATE		= 0x020000,

	/**
	 * When this flag is set, pages are transmitted with ascending
	 * page number. When the flag is cleared, pages of different
	 * magazines may be transmitted in parallel and only page
	 * numbers within a magazine (pgno & 0x0FF) are in series. In
	 * the latter case a rolling header should display only page
	 * numbers from the magazine requested by the user.
	 */
	VBI_SERIAL		= 0x100000
} vbi_ttx_page_flags; 

/**
 * @ingroup Event
 * @brief Event union.
 */
typedef struct vbi_event {
	vbi_event_mask		type;

	/**
	 * The network which transmitted the data of this event.
	 */
	const vbi_network *	network;

	/**
	 * The @a capture_time which was passed to the decoder with
	 * the data of this event, nominally the system time (UTC
	 * zone) when the data was captured.
	 */
	struct timeval		capture_time;

	/**
	 * The @a pts which was passed to the decoder with the data of
	 * this event, nominally the Presentation Time Stamp of the
	 * data. @a pts counts 1/90000 seconds from an arbitrary point
	 * in the data stream and lies in range 0 to (1 << 33) - 1
	 * inclusive (it wraps around after 26.5 hours).
	 */
	int64_t			pts;

	union {
		struct {
			vbi_pgno			pgno;
			vbi_subno			subno;
			vbi_ttx_page_flags		flags;
		}				ttx_page;

		int				_reserved1[8];
		void *				_reserved2[8];
	}			ev;
} vbi_event;

typedef vbi_bool
vbi_event_cb			(const vbi_event *	event,
				 void *			user_data);

VBI_END_DECLS

#endif /* __ZVBI_EVENT_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
