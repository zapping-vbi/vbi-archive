/*
 *  libzvbi
 *
 *  Copyright (C) 2000-2004 Michael H. Schimek
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

/* $Id: link.h,v 1.1.2.2 2004-04-08 23:36:25 mschimek Exp $ */

#ifndef __ZVBI_LINK_H__
#define __ZVBI_LINK_H__

#include "macros.h"
#include "network.h"		/* vbi_nuid */
#include "bcd.h"		/* vbi_pgno, vbi_subno */

VBI_BEGIN_DECLS

/**
 * @brief Link type.
 */
typedef enum {
	/**
	 * vbi_resolve_link() may return a link of this type on failure.
	 */
	VBI_LINK_NONE = 0,
	/**
	 * Not really a link, only vbi_link->name will be set. (Probably
	 * something like "Help! Help! The station is on fire!")
	 */
	VBI_LINK_MESSAGE,
	/**
	 * Points to a Teletext page, vbi_link->pgno and vbi_link->subno,
	 * eventually vbi_link->nuid and a descriptive text in vbi_link->name.
	 */
	VBI_LINK_PAGE,
	/**
	 * Also a Teletext page link, but this one is used exclusively
	 * to link subpages of the page containing the link.
	 */
	VBI_LINK_SUBPAGE,
	/**
	 * vbi_link->url is a HTTP URL (like "http://zapping.sf.net"),
	 * eventually accompanied by a descriptive text vbi_link->name.
	 */
	VBI_LINK_HTTP,
	/**
	 * vbi_link->url is a FTP URL (like "ftp://foo.bar.com/baz"),
	 * eventually accompanied by a descriptive text vbi_link->name.
	 */
	VBI_LINK_FTP,
	/**
	 * vbi_link->url is an e-mail address (like "mailto:foo@bar"),
	 * eventually accompanied by a descriptive text vbi_link->name.
	 */
	VBI_LINK_EMAIL,
	/** Is a trigger link id. Not useful, just ignore. */
	VBI_LINK_LID,
	/** Is a SuperTeletext link, ignore. */
	VBI_LINK_TELEWEB
} vbi_link_type;

/**
 * @ingroup Event
 * @brief ITV link type.
 *
 * Some ITV (WebTV, ATVEF) triggers include a type id intended
 * to filter relevant information. The names should speak for
 * themselves. EACEM triggers always have type @c VBI_WEBLINK_UNKNOWN.
 */
typedef enum {
	VBI_WEBLINK_UNKNOWN = 0,
	VBI_WEBLINK_PROGRAM_RELATED,
	VBI_WEBLINK_NETWORK_RELATED,
	VBI_WEBLINK_STATION_RELATED,
	VBI_WEBLINK_SPONSOR_MESSAGE,
	VBI_WEBLINK_OPERATOR
} vbi_itv_type;

/**
 * @ingroup Event
 *
 * General purpose link description for ATVEF (ITV, WebTV in the
 * United States) and EACEM (SuperTeletext et al in Europe) triggers,
 * Teletext TOP and FLOF navigation, and for links "guessed" by
 * libzvbi from the text (e. g. page numbers and URLs). Usually
 * not all fields will be used.
 */
typedef struct vbi_link {
	/** See vbi_link_type. */
	vbi_link_type			type;
	/**
	 * Links can be obtained two ways, via @ref VBI_EVENT_TRIGGER,
	 * then it arrived either through the EACEM or ATVEF transport
	 * method as flagged by this field. Or it is a navigational link
	 * returned by vbi_resolve_link(), then this field does not apply.
	 */
	vbi_bool			eacem;
	/**
	 * Some descriptive text, Latin-1, possibly blank.
	 */
	char 				name[80];
	char				url[256];
	/**
	 * A piece of ECMA script (Javascript), this may be
	 * used on WebTV or SuperTeletext pages to trigger some action.
	 * Usually blank.
	 */
	char				script[256];
	/**
	 * Teletext page links (no Closed Caption counterpart) can
	 * can actually reach across networks. That happens for example
	 * when vbi_resolve_link() picked up a link on a page after we
	 * switch away from that channel, or with EACEM triggers
	 * deliberately pointing to a page on another network (sic!).
	 * So the network id (if known, otherwise 0) is part of the
	 * page number. See vbi_nuid.
	 */
	vbi_nuid			nuid;
	/**
	 * @a pgno and @a subno Teletext page number, see vbi_pgno, vbi_subno.
	 * Note subno can be VBI_ANY_SUBNO.
	 */
	vbi_pgno			pgno;
	vbi_subno			subno;
	/**
	 * The time in seconds and fractions since
	 * 1970-01-01 00:00 when the link should no longer be offered
	 * to the user, similar to a HTTP cache expiration date.
	 */
	double				expires;
	/**
	 * See vbi_itv_type. This field applies only to
	 * ATVEF triggers, is otherwise @c VBI_WEBLINK_UNKNOWN.
	 */
	vbi_itv_type			itv_type;
	/**
	 * Trigger priority. 0 = emergency, should never be
	 * blocked. 1 or 2 = "high", 3 ... 5 = "medium", 6 ... 9 =
	 * "low". The default is 9. Apart of filtering triggers, this
	 * is also used to determine at which priority multiple links
	 * should be presented to the user. This field applies only to
	 * EACEM triggers, is otherwise 9.
	 */
	int				priority;
	/**
	 * Open the target without user confirmation. (Supposedly
	 * this flag will be used to trigger scripts, not to open pages,
	 * but I have yet to see such a trigger.)
	 */
	vbi_bool			autoload;
} vbi_link;

VBI_END_DECLS

#endif /* __ZVBI_LINK_H__ */
