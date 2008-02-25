/*
 *  libzvbi - Links
 *
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
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

/* $Id: link.h,v 1.1.2.6 2008-02-25 21:00:15 mschimek Exp $ */

#ifndef __ZVBI3_LINK_H__
#define __ZVBI3_LINK_H__

#include <stdio.h>		/* FILE */
#include "macros.h"
#include "network.h"		/* vbi3_nuid */
#include "bcd.h"		/* vbi3_pgno, vbi3_subno */

VBI3_BEGIN_DECLS

/**
 * @brief Link type.
 */
typedef enum {
	/**
	 * vbi3_resolve_link() may return a link of this type on failure.
	 */
	VBI3_LINK_NONE = 0,
	/**
	 * Not really a link, only vbi3_link->name will be set. (Probably
	 * something like "Help! Help! The station is on fire!")
	 */
	VBI3_LINK_MESSAGE,
	/**
	 * Points to a Teletext page, vbi3_link->pgno and vbi3_link->subno,
	 * eventually vbi3_link->nuid and a descriptive text in vbi3_link->name.
	 */
	VBI3_LINK_PAGE,
	/**
	 * Also a Teletext page link, but this one is used exclusively
	 * to link subpages of the page containing the link.
	 */
	VBI3_LINK_SUBPAGE,
	/**
	 * vbi3_link->url is a HTTP URL (like "http://zapping.sf.net"),
	 * eventually accompanied by a descriptive text vbi3_link->name.
	 */
	VBI3_LINK_HTTP,
	/**
	 * vbi3_link->url is a FTP URL (like "ftp://foo.bar.com/baz"),
	 * eventually accompanied by a descriptive text vbi3_link->name.
	 */
	VBI3_LINK_FTP,
	/**
	 * vbi3_link->url is an e-mail address (like "mailto:foo@bar"),
	 * eventually accompanied by a descriptive text vbi3_link->name.
	 */
	VBI3_LINK_EMAIL,
	/** Is a trigger link id. Not useful, just ignore. */
	VBI3_LINK_LID,
	/** Is a SuperTeletext link, ignore. */
	VBI3_LINK_TELEWEB
} vbi3_link_type;

extern const char *
vbi3_link_type_name		(vbi3_link_type		type)
  _vbi3_const;

/**
 * @ingroup Event
 * @brief ITV link type.
 *
 * Some ITV (WebTV, ATVEF) triggers include a type id intended
 * to filter relevant information. The names should speak for
 * themselves. EACEM triggers always have type @c VBI3_WEBLINK_UNKNOWN.
 */
typedef enum {
	VBI3_WEBLINK_UNKNOWN = 0,
	VBI3_WEBLINK_PROGRAM_RELATED,
	VBI3_WEBLINK_NETWORK_RELATED,
	VBI3_WEBLINK_STATION_RELATED,
	VBI3_WEBLINK_SPONSOR_MESSAGE,
	VBI3_WEBLINK_OPERATOR
} vbi3_itv_type;

/**
 * @ingroup Event
 *
 * General purpose link description for ATVEF (ITV, WebTV in the
 * United States) and EACEM (SuperTeletext et al in Europe) triggers,
 * Teletext TOP and FLOF navigation, and for links "guessed" by
 * libzvbi from the text (e. g. page numbers and URLs). Usually
 * not all fields will be used.
 */
typedef struct {
	/** See vbi3_link_type. */
	vbi3_link_type			type;
	/**
	 * Links can be obtained two ways, via @ref VBI3_EVENT_TRIGGER,
	 * then it arrived either through the EACEM or ATVEF transport
	 * method as flagged by this field. Or it is a navigational link
	 * returned by vbi3_resolve_link(), then this field does not apply.
	 */
	vbi3_bool			eacem;
	/**
	 * Some descriptive text, Latin-1, possibly blank.
	 */
	char *				name;
	/** ASCII */
	char *				url;
	/**
	 * A piece of ECMA script (Javascript), this may be
	 * used on WebTV or SuperTeletext pages to trigger some action.
	 * Usually blank. ASCII.
	 */
	char *				script;
	/**
	 * Teletext page links (no Closed Caption counterpart) can
	 * can actually reach across networks. That happens for example
	 * when vbi3_resolve_link() picked up a link on a page after we
	 * switch away from that channel, or with EACEM triggers
	 * deliberately pointing to a page on another network (sic!).
	 * So the network id (if known, otherwise 0) is part of the
	 * page number. See vbi3_nuid.
	 */
	vbi3_network *			network;
  /* bah. ugly */
	vbi3_bool			nk_alloc;
	/**
	 * @a pgno and @a subno Teletext page number, see vbi3_pgno, vbi3_subno.
	 * Note subno can be VBI3_ANY_SUBNO.
	 */
	vbi3_pgno			pgno;
	vbi3_subno			subno;
	/**
	 * The time in seconds and fractions since
	 * 1970-01-01 00:00 when the link should no longer be offered
	 * to the user, similar to a HTTP cache expiration date.
	 */
	double				expires;
	/**
	 * See vbi3_itv_type. This field applies only to
	 * ATVEF triggers, is otherwise @c VBI3_WEBLINK_UNKNOWN.
	 */
	vbi3_itv_type			itv_type;
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
	vbi3_bool			autoload;
} vbi3_link;

extern void
vbi3_link_destroy		(vbi3_link *		lk)
  _vbi3_nonnull ((1));
extern vbi3_bool
vbi3_link_copy			(vbi3_link *		dst,
				 const vbi3_link *	src)
  _vbi3_nonnull ((1));
extern void
vbi3_link_init			(vbi3_link *		lk)
  _vbi3_nonnull ((1));

/* Private */

extern void
_vbi3_link_dump			(const vbi3_link *	lk,
				 FILE *			fp)
  _vbi3_nonnull ((1, 2));

VBI3_END_DECLS

#endif /* __ZVBI3_LINK_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
