/*
 *  libzvbi -- Text page
 *
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
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

/* $Id: page.h,v 1.1.2.1 2008-08-19 10:56:06 mschimek Exp $ */

#ifndef __ZVBI_PAGE_H__
#define __ZVBI_PAGE_H__

#include "zvbi/cache.h"		/* vbi_cache */
#include "zvbi/network.h"	/* vbi_network */
#include "zvbi/bcd.h"		/* vbi_pgno, vbi_subno */

VBI_BEGIN_DECLS

/* Public */

#include <inttypes.h>

/**
 * @addtogroup Page Formatted text page
 * @ingroup Service
 */

/**
 * @ingroup Page
 * @brief Index into the vbi_page->color_map.
 *
 * The enumerated color names
 * refer to the Teletext and Closed Caption base palette of eight
 * colors. Note however the color_map really has 40 entries for
 * Teletext Level 2.5+, 32 of which are redefinable, the remaining
 * eight are private colors of libzvbi e. g. for
 * navigational information. So these symbols may not necessarily
 * correspond to the respective color.
 */
/* Code depends on order, don't change. */
typedef enum {
	VBI_BLACK,
	VBI_RED,
	VBI_GREEN,
	VBI_YELLOW,
	VBI_BLUE,
	VBI_MAGENTA,
	VBI_CYAN,
	VBI_WHITE
} vbi_color;

/**
 * @ingroup Page
 * @brief Colormap entry: 0xAABBGGRR. libzvbi sets the alpha channel
 * always to 0xFF.
 */
typedef uint32_t vbi_rgba;

/* Private */

#define VBI_RGBA(r, g, b)						\
	((((r) & 0xFFU) << 0) | (((g) & 0xFFU) << 8)			\
	 | (((b) & 0xFFU) << 16) | (0xFFU << 24))
#define VBI_R(rgba) (((rgba) >> 0) & 0xFFU)
#define VBI_G(rgba) (((rgba) >> 8) & 0xFFU)
#define VBI_B(rgba) (((rgba) >> 16) & 0xFFU)
#define VBI_A(rgba) (((rgba) >> 24) & 0xFFU)

/* Public */

/**
 * @ingroup Page
 * @brief Defines the opacity of a vbi_char and vbi_page border.
 *
 * Teletext Level 2.5 defines a special transparent color which
 * permits unusual characters with transparent foreground, opaque
 * background. For simplicity this type of opacity has been omitted. Also
 * renderers shall rely on the opacity attribute and not attempt to
 * interpret the color value as transparency indicator.
 */
typedef enum {
	/**
	 * This page is supposed to be overlayed onto
	 * video, with video displayed in place of this character (or the page
	 * border). In other words the character is a space (vbi_char->unicode =
	 * U+0020) and the glyph background is transparent. If desired the
	 * renderer may also fall back to VBI_TRANSLUCENT or VBI_OPAQUE
	 * mode. For this case vbi_char->background names the color to use as
	 * the translucent or opaque background.
	 *
	 * VBI_TRANSPARENT_SPACE is the opacity of subtitle pages (both border and
	 * characters, while the 'boxed' words are marked as VBI_TRANSLUCENT),
	 * but can also occur on a mainly VBI_OPAQUE page to create a 'window'
	 * effect.
 	 */
	VBI_TRANSPARENT_SPACE,

	/**
	 * Display video instead of the background color.
	 * Here the character is <em>not</em> a space and shall be displayed
	 * in vbi_char->foreground color. Only in the background of the character
	 * video shall look through. Again the renderer may fall back to
	 * VBI_TRANSLUCENT or VBI_OPAQUE.
	 */
	VBI_TRANSPARENT_FULL,

	/**
	 * Alpha blend video into background color, the
	 * character background becomes translucent. This is the opacity used
	 * for 'boxed' text on an otherwise VBI_TRANSPARENT_SPACE page, typically
	 * a subtitle or Teletext newsflash page. The renderer may fall back
	 * to VBI_OPAQUE.
	 */
	VBI_TRANSLUCENT,

	/**
	 * Display foreground and background color. Showing
	 * foreground or background transparent instead is not recommended because
	 * the editor may have swapped foreground and background color, then
	 * replaced a glyph by its inverse image, so one cannot really know if
	 * the character foreground or background will appear transparent.
	 */
	VBI_OPAQUE
} vbi_opacity;

/**
 * @ingroup Page
 * @brief Defines the size of a vbi_char in a vbi_page.
 *
 * Double width or height characters expand into the next
 * column right and/or next row below.
 * 
 * Scanning two rows left to right, you will find<br>
 * <pre>
 * VBI_NORMAL_SIZE | VBI_DOUBLE_WIDTH VBI_OVER_TOP | VBI_DOUBLE_HEIGHT  | VBI_DOUBLE_SIZE  VBI_OVER_TOP
 *       x         |         x             x       | VBI_DOUBLE_HEIGHT2 | VBI_DOUBLE_SIZE2 VBI_OVER_BOTTOM
 * </pre>
 * 
 * A VBI_DOUBLE_HEIGHT2, VBI_DOUBLE_SIZE2, VBI_OVER_TOP, VBI_OVER_BOTTOM
 * vbi_char has the same character unicode and attributes as the top/left anchor.
 * Partial characters (like a single VBI_DOUBLE_HEIGHT2) will not appear, so
 * VBI_DOUBLE_HEIGHT2, VBI_DOUBLE_SIZE2, VBI_OVER_TOP, VBI_OVER_BOTTOM
 * can be safely ignored when scanning the page.
 */
/* Code depends on order, don't change. */
typedef enum {
	VBI_NORMAL_SIZE, VBI_DOUBLE_WIDTH, VBI_DOUBLE_HEIGHT, VBI_DOUBLE_SIZE,
	VBI_OVER_TOP, VBI_OVER_BOTTOM, VBI_DOUBLE_HEIGHT2, VBI_DOUBLE_SIZE2
} vbi_size;

typedef enum {
	VBI_UNDERLINE	= (1 << 0), /**< Display character underlined. */
	VBI_BOLD	= (1 << 1), /**< Display character bold. */
	VBI_ITALIC	= (1 << 2), /**< Display character slanted right. */

	/**
	 * Display character or space (U+0020). One second cycle time.
	 */
	VBI_FLASH	= (1 << 3),

	/**
	 * Replace character by space (U+0020) if not revealed.
	 * This is used for example to hide text on question & answer pages.
	 */
	VBI_CONCEAL	= (1 << 4),

	/**
	 * No function yet, default is fixed spacing.
	 */
	VBI_PROPORTIONAL = (1 << 5),

	/**
	 * This character is part of a hyperlink. Call vbi_resolve_link()
	 * to get more information.
	 */
	VBI_LINK	= (1 << 6),

	/**
	 * PDC link.
	 */
	VBI_PDC		= (1 << 7)
} vbi_attr;

/**
 * @ingroup Page
 * @brief Attributed character.
 */
typedef struct vbi_char {
	/**
	 * Character attribute, see vbi_attr.
	 */
	uint8_t		attr;
	/**
	 * Character size, see vbi_size.
	 */
	uint8_t		size;
	/**
	 * Character opacity, see vbi_opacity. Both @a foreground
	 * and @a background color are valid independent of @a opacity.
	 */
	uint8_t		opacity;
	/**
	 * Character foreground color, a vbi_color index
	 * into the vbi_page->color_map.
	 */
	uint8_t		foreground;
	/**
	 * Character background color, a vbi_color index
	 * into the vbi_page->color_map.
	 */
	uint8_t		background;
	/**
	 * DRCS color look-up table offset, see vbi_page for details.
	 */
	uint8_t		drcs_clut_offs;
	/**
	 * Character code according to ISO 10646 UCS-2 (not UTF-16).
	 * 
	 * All Closed Caption characters can be represented in Unicode,
	 * but unfortunately not all Teletext characters.
	 * 
	 * <a href="http://www.etsi.org">ETS 300 706
	 * </a> Table 36 Latin National Subset Turkish, character
	 * 0x23 "Turkish currency symbol" is not representable in Unicode,
	 * thus translated to private code U+E800. I was unable to identify
	 * all Arabic glyphs in Table 44 and 45 Arabic G0 and G2, so for now
	 * these are mapped to private code U+E620 ... U+E67F and U+E720 ...
	 * U+E77F respectively. Table 47 G1 Block Mosaic is not representable
	 * in Unicode, translated to private code U+EE00 ... U+EE7F. That is,
	 * the contiguous form has bit 5 (0x20) set, the separate form
	 * cleared.
	 * Table 48 G3 "Smooth Mosaics and Line Drawing Set" is not
	 * representable in Unicode, translated to private code
	 * U+EF20 ... U+EF7F.
	 * 
	 * Teletext Level 2.5+ DRCS are represented by private code
	 * U+F000 ... U+F7FF. The 6 lsb select character 0x00 ... 0x3F
	 * from a DRCS plane, the 5 msb select DRCS plane 0 ... 31, see
	 * vbi_page for details.
	 *
	 * @bug
	 * Some Teletext character sets contain complementary
	 * Latin characters. For example the Greek capital letters Alpha
	 * and Beta are reused as Latin capital letter A and B, while a
	 * separate code exists for the Latin capital letter C. libzvbi will
	 * not analyse the page contents, so Greek A and B are always
	 * translated to Greek Alpha and Beta, C to Latin C, even if they
	 * appear in a pure Latin character word.
	 */
	uint16_t	unicode;
} vbi_char;

/**
 * @ingroup Page
 * @brief Formatted Teletext page.
 */
typedef struct {
	vbi_cache *		cache;

	unsigned int		ref_count;

	/**
	 * Identifies the network broadcasting this page.
	 */
	const vbi_network *	network;

	/**
	 * Page number, see vbi_pgno.
	 */
 	vbi_pgno		pgno;

	/**
	 * Subpage number, see vbi_subno. Only applicable
	 * if this is a Teletext page.
	 */
	vbi_subno		subno;

	/**
	 * Number of character rows in the page.
	 */
	unsigned int		rows;

	/**
	 * Number of character columns in the page.
	 */
	unsigned int		columns;

	/**
	 * The page contents, these are @a rows x @a columns without
	 * padding between the rows. See vbi_char for details.
	 */
	vbi_char		text[26 * 64];

	/**
	 */
	vbi_color		border_color;

	/**
	 */
	vbi_opacity		border_opacity;

	/**
	 * Ordinary characters are bi-level colored, with pixels assuming
	 * vbi_char->foreground or vbi_char->background color.
	 *
	 * The pixels of Teletext DRCS (Dynamically Redefinable Characters)
	 * can assume one color of a set of 2, 4 or 16.
	 *
	 * For bi-level DRCS vbi_char->drcs_clut_offs is 0. '0' pixels
	 * shall be painted in vbi_char->background color, '1' pixels in
	 * vbi_char->foreground color.
	 *
	 * For 4- and 16-level DRCS the color is determined by one of
	 * four color look-up tables. Here vbi_char->drcs_clut_offs is
	 * >= 2, and the color is calculated like this:
	 * @code
	 * vbi_page->drcs_clut[vbi_char->drcs_clut_offs + pixel value]
	 * @endcode
	 */
	vbi_color		drcs_clut[2 + 2 * 4 + 2 * 16];

	/**
	 * This is the color palette indexed by vbi_color in
	 * vbi_char and elsewhere, with colors defined as vbi_rgba.
	 *
	 * Note the palette may not correspond to the vbi_color
	 * enumeration names since Teletext allows editors to redefine
	 * the entire palette, depending on the Teletext level.
	 *
	 * Closed Caption and Teletext Level 1.0 / 1.5 pages use
	 * entries 0 ... 7. Teletext Level 2.5 / 3.5 pages use entries
	 * 0 ... 31. Teletext navigation (TOP, FLOF) added by libzvbi
	 * in row 25 uses entries 32 ... 39.
	 */
	vbi_rgba		color_map[40];

	/**
	 * Used by libzvbi, please don't touch.
	 */
	void *			_private;

	void *			reserved1[2];
	int			reserved2[2];
} vbi_page;

extern void
vbi_page_unref			(vbi_page *		pg)
  _vbi_nonnull ((1));
extern vbi_page *
vbi_page_ref			(vbi_page *		pg)
  _vbi_nonnull ((1));
extern void
vbi_page_delete			(vbi_page *		pg);
extern vbi_page *
vbi_page_dup			(const vbi_page *	pg)
  _vbi_alloc _vbi_nonnull ((1));
extern vbi_page *
vbi_page_new			(void)
  _vbi_alloc;

VBI_END_DECLS

#endif /* __ZVBI_PAGE_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
