/*
 *  libzvbi - Unified text buffer
 *
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
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

/* $Id: page.h,v 1.1.2.4 2008-02-25 20:59:53 mschimek Exp $ */

#ifndef __ZVBI3_PAGE_H__
#define __ZVBI3_PAGE_H__

#include "macros.h"
#include "bcd.h"	/* vbi3_pgno, vbi3_subno */
#include "network.h"	/* vbi3_nuid */
#include "link.h" 	/* vbi3_link */
#include "pdc.h"        /* vbi3_preselection */
#include "lang.h"	/* vbi3_ttx_charset */

VBI3_BEGIN_DECLS

typedef struct _vbi3_cache vbi3_cache;

/* Public */

#include <inttypes.h>

/**
 * @addtogroup Page Formatted text page
 * @ingroup Service
 */

/**
 * @ingroup Page
 * @brief Index into the vbi3_page->color_map.
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
	VBI3_BLACK,
	VBI3_RED,
	VBI3_GREEN,
	VBI3_YELLOW,
	VBI3_BLUE,
	VBI3_MAGENTA,
	VBI3_CYAN,
	VBI3_WHITE
} vbi3_color;

/**
 * @ingroup Page
 * @brief Colormap entry: 0xAABBGGRR. libzvbi sets the alpha channel
 * always to 0xFF.
 */
typedef uint32_t vbi3_rgba;

/* Private */

#define VBI3_RGBA(r, g, b)						\
	((((r) & 0xFFU) << 0) | (((g) & 0xFFU) << 8)			\
	 | (((b) & 0xFFU) << 16) | (0xFFU << 24))
#define VBI3_R(rgba) (((rgba) >> 0) & 0xFFU)
#define VBI3_G(rgba) (((rgba) >> 8) & 0xFFU)
#define VBI3_B(rgba) (((rgba) >> 16) & 0xFFU)
#define VBI3_A(rgba) (((rgba) >> 24) & 0xFFU)

/* Public */

/**
 * @ingroup Page
 * @brief Defines the opacity of a vbi3_char and vbi3_page border.
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
	 * border). In other words the character is a space (vbi3_char->unicode =
	 * U+0020) and the glyph background is transparent. If desired the
	 * renderer may also fall back to VBI3_TRANSLUCENT or VBI3_OPAQUE
	 * mode. For this case vbi3_char->background names the color to use as
	 * the translucent or opaque background.
	 *
	 * VBI3_TRANSPARENT_SPACE is the opacity of subtitle pages (both border and
	 * characters, while the 'boxed' words are marked as VBI3_TRANSLUCENT),
	 * but can also occur on a mainly VBI3_OPAQUE page to create a 'window'
	 * effect.
 	 */
	VBI3_TRANSPARENT_SPACE,
	/**
	 * Display video instead of the background color.
	 * Here the character is <em>not</em> a space and shall be displayed
	 * in vbi3_char->foreground color. Only in the background of the character
	 * video shall look through. Again the renderer may fall back to
	 * VBI3_TRANSLUCENT or VBI3_OPAQUE.
	 */
	VBI3_TRANSPARENT_FULL,
	/**
	 * Alpha blend video into background color, the
	 * character background becomes translucent. This is the opacity used
	 * for 'boxed' text on an otherwise VBI3_TRANSPARENT_SPACE page, typically
	 * a subtitle or Teletext newsflash page. The renderer may fall back
	 * to VBI3_OPAQUE.
	 */
	VBI3_TRANSLUCENT,
	/**
	 * Display foreground and background color. Showing
	 * foreground or background transparent instead is not recommended because
	 * the editor may have swapped foreground and background color, then
	 * replaced a glyph by its inverse image, so one cannot really know if
	 * the character foreground or background will appear transparent.
	 */
	VBI3_OPAQUE
} vbi3_opacity;

/**
 * @ingroup Page
 * @brief Defines the size of a vbi3_char in a vbi3_page.
 *
 * Double width or height characters expand into the next
 * column right and/or next row below.
 * 
 * Scanning two rows left to right, you will find<br>
 * <pre>
 * VBI3_NORMAL_SIZE | VBI3_DOUBLE_WIDTH VBI3_OVER_TOP | VBI3_DOUBLE_HEIGHT  | VBI3_DOUBLE_SIZE  VBI3_OVER_TOP
 *       x         |         x             x       | VBI3_DOUBLE_HEIGHT2 | VBI3_DOUBLE_SIZE2 VBI3_OVER_BOTTOM
 * </pre>
 * 
 * A VBI3_DOUBLE_HEIGHT2, VBI3_DOUBLE_SIZE2, VBI3_OVER_TOP, VBI3_OVER_BOTTOM
 * vbi3_char has the same character unicode and attributes as the top/left anchor.
 * Partial characters (like a single VBI3_DOUBLE_HEIGHT2) will not appear, so
 * VBI3_DOUBLE_HEIGHT2, VBI3_DOUBLE_SIZE2, VBI3_OVER_TOP, VBI3_OVER_BOTTOM
 * can be safely ignored when scanning the page.
 */
/* Code depends on order, don't change. */
typedef enum {
	VBI3_NORMAL_SIZE, VBI3_DOUBLE_WIDTH, VBI3_DOUBLE_HEIGHT, VBI3_DOUBLE_SIZE,
	VBI3_OVER_TOP, VBI3_OVER_BOTTOM, VBI3_DOUBLE_HEIGHT2, VBI3_DOUBLE_SIZE2
} vbi3_size;

typedef enum {
	VBI3_UNDERLINE	= (1 << 0), /**< Display character underlined. */
	VBI3_BOLD	= (1 << 1),	/**< Display character bold. */
	VBI3_ITALIC	= (1 << 2),	/**< Display character slanted right. */
	/**
	 * Display character or space (U+0020). One second cycle time.
	 */
	VBI3_FLASH	= (1 << 3),
	/**
	 * Replace character by space (U+0020) if not revealed.
	 * This is used for example to hide text on question & answer pages.
	 */
	VBI3_CONCEAL	= (1 << 4),
	/**
	 * No function yet, default is fixed spacing.
	 */
	VBI3_PROPORTIONAL = (1 << 5),
	/**
	 * This character is part of a hyperlink. Call vbi3_resolve_link()
	 * to get more information.
	 */
	VBI3_LINK	= (1 << 6),
	/**
	 * PDC link.
	 */
	VBI3_PDC		= (1 << 7)
} vbi3_attr;

/**
 * @ingroup Page
 * @brief Attributed character.
 */
typedef struct vbi3_char {
	/**
	 * Character attribute, see vbi3_attr.
	 */
	uint8_t		attr;
	/**
	 * Character size, see vbi3_size.
	 */
	uint8_t		size;
	/**
	 * Character opacity, see vbi3_opacity. Both @a foreground
	 * and @a background color are valid independent of @a opacity.
	 */
	uint8_t		opacity;
	/**
	 * Character foreground color, a vbi3_color index
	 * into the vbi3_page->color_map.
	 */
	uint8_t		foreground;
	/**
	 * Character background color, a vbi3_color index
	 * into the vbi3_page->color_map.
	 */
	uint8_t		background;
	/**
	 * DRCS color look-up table offset, see vbi3_page for details.
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
	 * vbi3_page for details.
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
} vbi3_char;

typedef struct _vbi3_page_priv vbi3_page_priv;

/**
 * @ingroup Page
 * @brief Formatted Teletext or Closed Caption page.
 *
 * Clients can fetch pages
 * from the respective cache using vbi3_fetch_vt_page() or
 * vbi3_fetch_cc_page() for evaluation, display or output. Since
 * the page may reference other objects in cache which are locked
 * by the fetch functions, vbi3_unref_page() must be called when done.
 * Note this structure is large, some 10 KB.
 */
typedef struct {
	/**
	 * Points back to the source context.
	 */ 
	vbi3_cache *		cache;

	unsigned int		ref_count;

	/**
	 * Identifies the network broadcasting this page.
	 */
	const vbi3_network *	network;

	/**
	 * Page number, see vbi3_pgno.
	 */
 	vbi3_pgno		pgno;

	/**
	 * Subpage number, see vbi3_subno. Only applicable
	 * if this is a Teletext page.
	 */
	vbi3_subno		subno;

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
	 * padding between the rows. See vbi3_char for details.
	 */
	vbi3_char		text[26 * 64];

	/**
	 * To speed up rendering these variables mark the rows
	 * which actually changed since the page has been last fetched
	 * from cache. @a y0 ... @a y1 are the first to last row changed,
	 * inclusive, in range 0 ... @a rows - 1. @a roll indicates the
	 * page has been vertically scrolled this number of rows,
	 * negative numbers up (towards lower row numbers), positive
	 * numbers down. For example -1 means row @a y0 + 1 ... @a y1
	 * moved to @a y0 ... @a y1 - 1, erasing row @a y1 to all spaces.
	 * 
	 * Practically this is only used in Closed Caption roll-up
	 * mode, otherwise all rows are always marked dirty. Clients
	 * are free to ignore this information.
	 */
	struct {
	     /* int			x0, x1; */
		int			y0, y1;
		int			roll;
	}			dirty;

	/**
	 * When a TV displays Teletext or Closed Caption only a subsection
	 * of the screen is covered by the character matrix. The 'border'
	 * around this area can have a distinct color, usually it is black.
	 * @a screen_color is a vbi3_color index into the @a color_map.
	 */
	vbi3_color		screen_color;

	/**
	 * The 'border' can also have a distinct opacity. Usually it will
	 * be VBI3_OPAQUE, but pages intended for overlay onto video
	 * (Teletext subtitles, newsflash, Caption pages) will have a
	 * @a screen_opacity of VBI3_TRANSPARENT_SPACE.
	 * See vbi3_opacity for details.
	 */
	vbi3_opacity		screen_opacity;

	/**
	 * Ordinary characters are bi-level colored, with pixels assuming
	 * vbi3_char->foreground or vbi3_char->background color.
	 *
	 * The pixels of Teletext DRCS (Dynamically Redefinable Characters)
	 * can assume one color of a set of 2, 4 or 16.
	 *
	 * For bi-level DRCS vbi3_char->drcs_clut_offs is 0. '0' pixels
	 * shall be painted in vbi3_char->background color, '1' pixels in
	 * vbi3_char->foreground color.
	 *
	 * For 4- and 16-level DRCS the color is determined by one of
	 * four color look-up tables. Here vbi3_char->drcs_clut_offs is
	 * >= 2, and the color is calculated like this:
	 * @code
	 * vbi3_page->drcs_clut[vbi3_char->drcs_clut_offs + pixel value]
	 * @endcode
	 */
	vbi3_color		drcs_clut[2 + 2 * 4 + 2 * 16];

	/**
	 * This is the color palette indexed by vbi3_color in
	 * vbi3_char and elsewhere, with colors defined as vbi3_rgba.
	 *
	 * Note the palette may not correspond to the vbi3_color
	 * enumeration names since Teletext allows editors to redefine
	 * the entire palette, depending on the Teletext level.
	 *
	 * Closed Caption and Teletext Level 1.0 / 1.5 pages use
	 * entries 0 ... 7. Teletext Level 2.5 / 3.5 pages use entries
	 * 0 ... 31. Teletext navigation (TOP, FLOF) added by libzvbi
	 * in row 25 uses entries 32 ... 39.
	 */
	vbi3_rgba 		color_map[40];

	vbi3_page_priv *		priv;
} vbi3_page;

extern vbi3_bool
vbi3_page_get_hyperlink		(const vbi3_page *	pg,
				 vbi3_link *		ld,
				 unsigned int		column,
				 unsigned int		row)
  _vbi3_nonnull ((1, 2));
extern const vbi3_link *
vbi3_page_get_teletext_link	(const vbi3_page *	pg,
				 unsigned int		indx)
  _vbi3_nonnull ((1));
_vbi3_inline const vbi3_link *
vbi3_page_get_home_link		(const vbi3_page *	pg)
{
	return vbi3_page_get_teletext_link (pg, 5);
}
extern const vbi3_preselection *
vbi3_page_get_pdc_link		(const vbi3_page *	pg,
				 unsigned int		column,
				 unsigned int		row)
  _vbi3_nonnull ((1));
extern const vbi3_preselection *
vbi3_page_get_preselections	(const vbi3_page *	pg,
				 unsigned int *		n_elements)
  _vbi3_nonnull ((1));
extern const uint8_t *
vbi3_page_get_drcs_data		(const vbi3_page *	pg,
				 unsigned int		unicode)
  _vbi3_nonnull ((1));
extern const vbi3_ttx_charset *
vbi3_page_get_ttx_charset	(const vbi3_page *	pg,
				 unsigned int		level)
  _vbi3_nonnull ((1));
extern void
vbi3_page_unref			(vbi3_page *		pg)
  _vbi3_nonnull ((1));
extern vbi3_page *
vbi3_page_ref			(vbi3_page *		pg)
  _vbi3_nonnull ((1));
extern void
vbi3_page_delete			(vbi3_page *		pg);
extern vbi3_page *
vbi3_page_dup			(const vbi3_page *	pg)
  _vbi3_alloc _vbi3_nonnull ((1));
extern vbi3_page *
vbi3_page_new			(void)
  _vbi3_alloc;

typedef enum {
	VBI3_TABLE = 0x32f54a00,
	VBI3_RTL,
	VBI3_SCALE,
	VBI3_REVEAL,
	VBI3_FLASH_ON,
	VBI3_BRIGHTNESS,
	VBI3_CONTRAST
} vbi3_export_option;

VBI3_END_DECLS

#endif /* __ZVBI3_PAGE_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
