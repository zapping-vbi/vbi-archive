/*
 *  libzvbi -- Links
 *
 *  Copyright (C) 2000-2008 Michael H. Schimek
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

/* $Id: link.h,v 1.1.2.2 2008-08-22 07:59:03 mschimek Exp $ */

#ifndef __ZVBI_LINK_H__
#define __ZVBI_LINK_H__

#include <stdio.h>		/* FILE */
#include <time.h>		/* time_t */

#include "zvbi/network.h"	/* vbi_network */
#include "zvbi/bcd.h"		/* vbi_pgno, vbi_subno */

VBI_BEGIN_DECLS

/**
 * @brief Link type.
 */
typedef enum {
	VBI_LINK_NONE = 0,

	/**
	 * This is just a message for display to the user. The @a
	 * vbi_link->name field points to the message text.
	 */
	VBI_LINK_MESSAGE,

	/**
	 * A link to a Teletext page. @a vbi_link->lk.ttx.pgno and
	 * lk.ttx.subno will be set. The @a lk.ttx.network field may
	 * identify the network transmitting the page, which may not
	 * be the network transmitting the link. The @a name field may
	 * point to a description of the page for display to the user.
	 */
	VBI_LINK_TTX_PAGE,

	/**
	 * A link to a webpage. @a vbi_link->lk.web.url is a HTTP URL
	 * like "http://zapping.sf.net". The @a name field may point
	 * to a description of the page for display to the user.
	 */
	VBI_LINK_HTTP,

	/**
	 * @a vbi_link->lk.web.url is a FTP URL like
	 * "ftp://foo.bar.com/baz". The @a name field may point to a
	 * descriptive text for display to the user.
	 */
	VBI_LINK_FTP,

	/**
	 * @a vbi_link->lk.web.url is an e-mail address like
	 * "mailto:foo@bar". The @a name field may point to a
	 * descriptive text for display to the user.
	 */
	VBI_LINK_EMAIL
} vbi_link_type;

extern const char *
_vbi_link_type_name		(vbi_link_type		type)
  _vbi_const;

/**
 * @ingroup Event
 *
 * This structure describes a link transmitted by the network.
 */
typedef struct {
	/** See vbi_link_type. */
	vbi_link_type			type;

	int				reserved1;

	/**
	 * Some descriptive text, a NUL-terminated string in locale
	 * encoding. Can be @c NULL if no description is available.
	 */
	char *				name;

	union {
		struct {
			/**
			 * If this is a @c VBI_LINK_TTX_PAGE, this
			 * field identifies the network transmitting
			 * the page. Can be @c NULL if the network is
			 * unknown.
			 */
			vbi_network *			network;

			/** A Teletext page number. */
			vbi_pgno			pgno;

			/**
			 * A Teletext subpage number. Can be @a
			 * VBI_ANY_SUBNO.
			 */
			vbi_subno			subno;
		}				ttx;

		struct {
			/**
			 * A Uniform Resource Locator representing a
			 * resource available on the Internet. This is
			 * a NUL-terminated string in locale
			 * encoding.
			 */
			char *				url;

			/**
			 * A piece of ECMAscript (ECMA-262) which may
			 * trigger some action on WebTV or
			 * SuperTeletext pages. This is a
			 * NUL-terminated string in locale encoding.
			 * Can be @c NULL if no script was
			 * transmitted.
			 */
			char *				script;

			int				reserved2;
		}				web;

		void *				reserved3[4];
	}				lk;

	/**
	 * The time in seconds since 1970-01-01 00:00 UTC after which
	 * the link should no longer be offered to the user.
	 */
	time_t				expiration_time;

	void *				reserved4[2];
	int				reserved5[2];
} vbi_link;

extern vbi_bool
vbi_link_copy			(vbi_link *		dst,
				 const vbi_link *	src)
  _vbi_nonnull ((1));
extern vbi_bool
vbi_link_destroy		(vbi_link *		lk)
  _vbi_nonnull ((1));
extern vbi_bool
vbi_link_init			(vbi_link *		lk)
  _vbi_nonnull ((1));
extern vbi_link *
vbi_link_dup			(const vbi_link *	lk)
  _vbi_alloc;
extern void
vbi_link_delete			(vbi_link *		lk)
  _vbi_nonnull ((1));
extern vbi_link *
vbi_link_new			(void)
  _vbi_alloc;

/* Private */

extern vbi_bool
_vbi_link_dump			(const vbi_link *	lk,
				 FILE *			fp)
  _vbi_nonnull ((1, 2));
extern vbi_link *
_vbi_ttx_link_new		(const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno)
  _vbi_nonnull ((1));

VBI_END_DECLS

#endif /* __ZVBI_LINK_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
