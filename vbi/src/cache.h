/*
 *  libzvbi - Network cache
 *
 *  Copyright (C) 2001, 2002, 2003, 2004 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig
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

/* $Id: cache.h,v 1.2.2.17 2008-02-25 21:01:20 mschimek Exp $ */

#ifndef __ZVBI3_CACHE_H__
#define __ZVBI3_CACHE_H__

#include <stdarg.h>
#include "network.h"
#include "page.h"
#include "top_title.h"
#include "event.h"

VBI3_BEGIN_DECLS

/* in format.h */
/* typedef struct _vbi3_cache vbi3_cache; */

/**
 * @brief Teletext page type.
 *
 * Some networks provide information to classify Teletext pages,
 * this can be used for example to automatically find program schedule
 * and subtitle pages. See also x().
 */
typedef enum {
	VBI3_NO_PAGE		= 0x00,
	VBI3_NORMAL_PAGE		= 0x01,

	VBI3_TOP_BLOCK		= 0x60,		/* libzvbi private */
	VBI3_TOP_GROUP		= 0x61,		/* libzvbi private */
	VBI3_NEWSFLASH_PAGE	= 0x62,		/* libzvbi private */

	VBI3_SUBTITLE_PAGE	= 0x70,
	VBI3_SUBTITLE_INDEX	= 0x78,
	VBI3_NONSTD_SUBPAGES	= 0x79,
	VBI3_PROGR_WARNING	= 0x7A,
	VBI3_CURRENT_PROGR	= 0x7C,
	VBI3_NOW_AND_NEXT	= 0x7D,
	VBI3_PROGR_INDEX		= 0x7F,
	VBI3_NOT_PUBLIC		= 0x80,
	VBI3_PROGR_SCHEDULE	= 0x81,
	VBI3_CA_DATA		= 0xE0,
	VBI3_PFC_EPG_DATA	= 0xE3,
	VBI3_PFC_DATA		= 0xE4,
	VBI3_DRCS_PAGE		= 0xE5,
	VBI3_POP_PAGE		= 0xE6,
	VBI3_SYSTEM_PAGE		= 0xE7,
	VBI3_KEYWORD_SEARCH_LIST = 0xF9,
	VBI3_TRIGGER_DATA	= 0xFC,
	VBI3_ACI_PAGE		= 0xFD,
	VBI3_TOP_PAGE		= 0xFE,		/* MPT, AIT, MPT-EX */

	VBI3_UNKNOWN_PAGE	= 0xFF		/* libzvbi private */
} vbi3_page_type;

/* in packet.c */
extern const char *
vbi3_page_type_name		(vbi3_page_type		type)
  _vbi3_const;

/**
 * @brief Meta data and statistical info about a cached Teletext page.
 *
 * Note the page this information refers to may not be cached yet
 * (e.g. data from Teletext page inventory tables) or not anymore.
 */
typedef struct {
  /* XXX add pgno and timestamp? */

	/** Teletext page type. */
	vbi3_page_type		page_type;
	/**
	 * Primary character set used on the page. You might use
	 * this as a subtitle language hint. @c NULL if unknown.
	 */
	const vbi3_ttx_charset *ttx_charset;
	/** Expected number of subpages: 0 or 2 ... 79. */
	unsigned int		subpages;
	/** Lowest subno received yet. */
	vbi3_subno		subno_min;
	/** Highest subno received yet. */
	vbi3_subno		subno_max;

	void *			_reserved1[2];
	int			_reserved2[2];
} vbi3_ttx_page_stat;

extern void
vbi3_ttx_page_stat_destroy	(vbi3_ttx_page_stat *	ps)
  _vbi3_nonnull ((1));
extern void
vbi3_ttx_page_stat_init		(vbi3_ttx_page_stat *	ps)
  _vbi3_nonnull ((1));
extern vbi3_bool
vbi3_cache_get_ttx_page_stat	(vbi3_cache *		ca,
				 vbi3_ttx_page_stat *	ps,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno)
  _vbi3_nonnull ((1, 2, 3));
extern vbi3_bool
vbi3_cache_get_top_title		(vbi3_cache *		ca,
				 vbi3_top_title *	tt,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno)
  _vbi3_nonnull ((1, 2, 3));
extern vbi3_top_title *
vbi3_cache_get_top_titles	(vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 unsigned int *		n_elements)
  _vbi3_nonnull ((1, 2, 3));

/**
 * @brief Values for the vbi3_format_option @c VBI3_WST_LEVEL.
 */
typedef enum {
	/**
	 * Level 1 - Basic Teletext pages. All pages can be formatted
	 * like this since networks transmit Level 1 data as fallback
	 * for older Teletext decoders.
	 */
	VBI3_WST_LEVEL_1,
	/**
	 * Level 1.5 - Additional national and graphics characters.
	 */
	VBI3_WST_LEVEL_1p5,
	/**
	 * Level 2.5 - Additional text styles, more colors, DRCS, side
	 * panels. You should enable Level 2.5 only if you can render
	 * and/or export such pages.
	 */
	VBI3_WST_LEVEL_2p5,
	/**
	 * Level 3.5 - Multicolor DRCS, proportional script.
	 */
	VBI3_WST_LEVEL_3p5
} vbi3_wst_level;

/**
 * @brief Page formatting options.
 *
 * Pass formatting options as a vector of option pairs, consisting
 * of an option number and value. The last option number must be @c VBI3_END.
 *
 * function (foo, bar,
 *           VBI3_PADDING, TRUE,
 *           VBI3_DEFAULT_CHARSET_0, 15,
 *           VBI3_HEADER_ONLY, FALSE,
 *           VBI3_END);
 */
/* We use random numbering assuming the variadic functions using these
   values stop reading when they encounter an unknown number (VBI3_END is
   zero). Parameters shall be only int or pointer (vbi3_bool is an int,
   enum is an int) for proper automatic casts. */
typedef enum {
	/**
	 * Format only the first row.
	 * Parameter: vbi3_bool, default FALSE.
	 */
	VBI3_HEADER_ONLY = 0x37138F00,
	/**
	 * Often column 0 of a page contains all black spaces,
	 * unlike column 39. Enabling this option will result in
	 * a more balanced view.
	 * Parameter: vbi3_bool, default FALSE.
	 */
	VBI3_PADDING,
	/**
	 * Not implemented yet.
	 * Format the page with side panels if it has any. This
	 * option takes precedence over VBI3_41_COLUMNS if side panels
	 * are present.
	 * Parameter: vbi3_bool, default FALSE.
	 */
	VBI3_PANELS,
	/**
	 * Enable TOP or FLOF navigation in row 25.
	 * - 0 disable
	 * - 1 FLOF or TOP style 1
	 * - 2 FLOF or TOP style 2
	 * Parameter: int, default 0.
	 */
	VBI3_NAVIGATION,
	/**
	 * Scan the page for page numbers, URLs, e-mail addresses
	 * etc. and create hyperlinks.
	 * Parameter: vbi3_bool, default FALSE.
	 */
	VBI3_HYPERLINKS,
	/**
	 * Scan the page for PDC Method A/B preselection data
	 * and create a PDC table and links.
	 * Parameter: vbi3_bool, default FALSE.
	 */
	VBI3_PDC_LINKS,
	/**
	 * Format the page at the given Teletext implementation level.
	 * Parameter: vbi3_wst_level, default VBI3_WST_LEVEL_1.
	 */
	VBI3_WST_LEVEL,
	/**
	 * The default character set code. Codes transmitted by the
	 * network take precedence. When the network transmits only the
	 * three last significant bits, this value provides the higher
	 * bits, or if this yields no valid code all bits.
	 * Parameter: vbi3_ttx_charset_code, default 0 (English).
	 */
	VBI3_DEFAULT_CHARSET_0,
	/**
	 * Same as VBI3_DEFAULT_CHARSET_0, for secondary character set.
	 */
	VBI3_DEFAULT_CHARSET_1,
	/**
	 * Overrides the primary character set code of a page. This takes
	 * precedence over VBI3_DEFAULT_CHARSET_0 and any code transmitted
	 * by the network.
	 * Parameter: vbi3_ttx_charset_code, default is transmitted value.
	 */
	VBI3_OVERRIDE_CHARSET_0,
	/**
	 * Same as VBI3_OVERRIDE_CHARSET_0, for secondary character set.
	 */
	VBI3_OVERRIDE_CHARSET_1,
	VBI3_DEFAULT_FOREGROUND, /* XXX document me */
	VBI3_DEFAULT_BACKGROUND,
	VBI3_ROW_CHANGE
} vbi3_format_option;

/* in teletext.c */
extern vbi3_page *
vbi3_cache_get_teletext_page_va_list
				(vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 va_list		format_options)
  _vbi3_nonnull ((1, 2));
extern vbi3_page *
vbi3_cache_get_teletext_page	(vbi3_cache *		ca,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 ...)
  _vbi3_nonnull ((1, 2));
/* in cache.c */
extern vbi3_network *
vbi3_cache_get_networks		(vbi3_cache *		ca,
				 unsigned int *		n_elements)
  _vbi3_nonnull ((1, 2));
/* in cache.c */
extern void
vbi3_cache_remove_event_handler	(vbi3_cache *		ca,
				 vbi3_event_cb *	callback,
				 void *			user_data)
  _vbi3_nonnull ((1));
extern vbi3_bool
vbi3_cache_add_event_handler	(vbi3_cache *		ca,
				 vbi3_event_mask	event_mask,
				 vbi3_event_cb *	callback,
				 void *			user_data)
  _vbi3_nonnull ((1));
extern void
vbi3_cache_set_memory_limit	(vbi3_cache *		ca,
				 unsigned long		limit)
  _vbi3_nonnull ((1));
extern void
vbi3_cache_set_network_limit	(vbi3_cache *		ca,
				 unsigned int		limit)
  _vbi3_nonnull ((1));
extern void
vbi3_cache_unref		(vbi3_cache *		ca);
extern vbi3_cache *
vbi3_cache_ref			(vbi3_cache *		ca)
  _vbi3_nonnull ((1));
extern void
vbi3_cache_delete		(vbi3_cache *		ca);
extern vbi3_cache *
vbi3_cache_new			(void)
  _vbi3_alloc;

VBI3_END_DECLS

#endif /* __ZVBI3_CACHE_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
