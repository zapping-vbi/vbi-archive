/*
 *  libzvbi -- Teletext decoder
 *
 *  Copyright (C) 2000-2008 Michael H. Schimek
 *
 *  Originally based on AleVT 1.5.1 by Edgar Toernig
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
 *  libzvbi - Teletext decoder
 */

/* $Id: ttx_decoder.h,v 1.1.2.2 2008-08-20 12:34:28 mschimek Exp $ */

#ifndef __ZVBI_TTX_DECODER_H__
#define __ZVBI_TTX_DECODER_H__

#include <stdarg.h>
#include "zvbi/link.h"		/* vbi_link */
#include "zvbi/cache.h"		/* vbi_cache */
#include "zvbi/event.h"		/* vbi_event_cb */
#include "zvbi/sliced.h"	/* vbi_sliced */

VBI_BEGIN_DECLS

/**
 * @addtogroup Teletext
 * @{
 */

/**
 * @brief Teletext decoder.
 *
 * The contents of this structure are private.
 * Call vbi_ttx_decoder_new() to allocate a Teletext decoder.
 */
typedef struct _vbi_ttx_decoder vbi_ttx_decoder;

/**
 * @brief Values for the vbi_ttx_format_option @c VBI_TTX_LEVEL.
 */
typedef enum {
	/**
	 * Level 1 - Basic Teletext pages. All pages can be formatted
	 * like this since networks transmit Level 1 data as fallback
	 * for older Teletext decoders.
	 */
	VBI_TTX_LEVEL_1,

	/**
	 * Level 1.5 - Additional characters.
	 */
	VBI_TTX_LEVEL_1p5,

	/**
	 * Level 2.5 - Additional text styles, more colors, DRCS, side
	 * panels. You should enable Level 2.5 only if you can render
	 * or export such pages.
	 */
	VBI_TTX_LEVEL_2p5,

	/**
	 * Level 3.5 - Multicolor DRCS, proportional script.
	 */
	VBI_TTX_LEVEL_3p5
} vbi_ttx_level;

/**
 * @brief Teletext page formatting options.
 *
 * These are optional named parameters for Teletext page formatting
 * functions. Append them to the positional parameters as key/value
 * pairs. The last key must be @c VBI_END (a @c NULL pointer). For
 * example:
 *
 * @code
 * pg = vbi_ttx_decoder_get_page (td, nk, pgno, subno,
 *                                VBI_COLUMN_41, TRUE,
 *                                VBI_DEFAULT_CHARSET_0, 15,
 *                                VBI_HEADER_ONLY, (vbi_bool) header,
 *                                VBI_END);
 * @endcode
 *
 * More options may be available in the future.
 */
typedef enum {
	/**
	 * Format only the first row of a Teletext page. This is
	 * useful to display the number of the currently received page
	 * while waiting for the page requested by the user, and to
	 * update the clock in the upper right corner.
	 *
	 * Parameter: vbi_bool, default FALSE (format all rows).
	 */
	VBI_HEADER_ONLY = 0x37138F01,

	/**
	 * Teletext Level 1.0 pages use spacing attributes which
	 * frequently leave the first column blank, while text is
	 * printed up to column 39. This option adds a 41st column to
	 * give a more balanced view.
	 *
	 * Parameter: vbi_bool, default FALSE.
	 */
	VBI_COLUMN_41,

	/**
	 * Not implemented yet. Format a Teletext page with side
	 * panels, a Level 2.5 feature. This may add extra columns at
	 * the left and/or right, for a total page width between 40
	 * and 64 columns. This option takes precedence over
	 * VBI_PADDING if side panels have been transmitted.
	 *
	 * Parameter: vbi_bool, default FALSE.
	 */
	VBI_PANELS,

	/**
	 * Enable TOP or FLOF navigation in row 25.
	 * - 0 disable
	 * - 1 FLOF or TOP style 1
	 * - 2 FLOF or TOP style 2
	 *
	 * Parameter: int, default 0.
	 */
	VBI_NAVIGATION,

	/**
	 * Scan the page for page numbers, URLs, e-mail addresses
	 * etc. and turn them into hyperlinks.
	 *
	 * Parameter: vbi_bool, default FALSE.
	 */
	VBI_HYPERLINKS,

	/**
	 * Scan the page for PDC Method A/B preselection data,
	 * create a PDC table and PDC hyperlinks.
	 *
	 * Parameter: vbi_bool, default FALSE.
	 */
	VBI_PDC_LINKS,

	/**
	 * Format the page at the given Teletext implementation level.
	 * This option is useful if the output device cannot display
	 * advanced features like DRCS.
	 *
	 * Parameter: vbi_ttx_level, default VBI_TTX_LEVEL_1.
	 */
	VBI_TTX_LEVEL,

	/**
	 * The default character set code used to format the page.
	 * When the network transmits a full code this value will be
	 * ignored. When it transmits only the three least significant
	 * bits of the code, this value provides the higher bits, or
	 * if this yields no valid code all bits.
	 *
	 * Parameter: vbi_ttx_charset_code, default 0 (English).
	 */
	VBI_DEFAULT_CHARSET_0,

	/**
	 * Two character sets are available on each Teletext page,
	 * selectable by a control code embedded in the page. This
	 * option defines a default character set code like @c
	 * VBI_DEFAULT_CHARSET_0, but for the secondary character set.
	 */
	VBI_DEFAULT_CHARSET_1,

	/**
	 * This value overrides the primary character set code
	 * specified with VBI_DEFAULT_CHARSET_0 and any code
	 * transmitted by the network. The option is useful if the
	 * network transmits incorrect character set codes.
	 *
	 * Parameter: vbi_ttx_charset_code, default is the
	 * transmitted character set code.
	 */
	VBI_OVERRIDE_CHARSET_0,

	/**
	 * Same as VBI_OVERRIDE_CHARSET_0, but for the secondary
	 * character set.
	 */
	VBI_OVERRIDE_CHARSET_1,
} vbi_ttx_format_option;

extern vbi_link *
vbi_ttx_page_get_link		(const vbi_page *	pg,
				 unsigned int		indx)
  _vbi_nonnull ((1));
extern const uint8_t *
vbi_ttx_page_get_drcs_data	(const vbi_page *	pg,
				 unsigned int		unicode)
  _vbi_nonnull ((1));
extern vbi_page *
vbi_ttx_decoder_get_page_va	(vbi_ttx_decoder *	td,
				 const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 va_list		options)
  _vbi_alloc _vbi_nonnull ((1));
extern vbi_page *
vbi_ttx_decoder_get_page	(vbi_ttx_decoder *	td,
				 const vbi_network *	nk,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 ...)
#ifndef DOXYGEN_SHOULD_IGNORE_THIS
  _vbi_alloc _vbi_nonnull ((1)) _vbi_sentinel
#endif
;
extern vbi_bool
vbi_ttx_decoder_remove_event_handler
				(vbi_ttx_decoder *	td,
				 vbi_event_cb *		callback,
				 void *			user_data)
  _vbi_nonnull ((1));
extern vbi_bool
vbi_ttx_decoder_add_event_handler
				(vbi_ttx_decoder *	td,
				 vbi_event_mask		event_mask,
				 vbi_event_cb *		callback,
				 void *			user_data)
  _vbi_nonnull ((1));
extern vbi_bool
vbi_ttx_decoder_reset		(vbi_ttx_decoder *	td,
				 const vbi_network *	nk)
  _vbi_nonnull ((1));
extern vbi_bool
vbi_ttx_decoder_feed		(vbi_ttx_decoder *	td,
				 const uint8_t		buffer[42],
				 const struct timeval *	capture_time,
				 int64_t		pts)
#ifndef DOXYGEN_SHOULD_IGNORE_THIS
  _vbi_nonnull ((1, 2))
#endif
;
extern vbi_bool
vbi_ttx_decoder_feed_frame	(vbi_ttx_decoder *	td,
				 const vbi_sliced *	sliced,
				 unsigned int		n_lines,
				 const struct timeval *	capture_time,
				 int64_t		pts)
#ifndef DOXYGEN_SHOULD_IGNORE_THIS
  _vbi_nonnull ((1, 2))
#endif
;
extern void
vbi_ttx_decoder_delete		(vbi_ttx_decoder *	td);

extern vbi_ttx_decoder *
vbi_ttx_decoder_new		(vbi_cache *		ca,
				 const vbi_network *	nk)
  _vbi_alloc;

/** @} */

VBI_END_DECLS

#endif /* __ZVBI_TTX_DECODER_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
