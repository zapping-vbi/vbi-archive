/*
 *  libzvbi - Unified text buffer format
 *
 *  Copyright (C) 2000, 2001 Michael H. Schimek
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

/* $Id: format.h,v 1.4.2.10 2004-05-01 13:51:35 mschimek Exp $ */

#ifndef FORMAT_H
#define FORMAT_H

#include "macros.h"
#include "cache.h"	/* vbi_cache */
#include "network.h"	/* vbi_nuid */
#include "link.h" 	/* vbi_link */
#include "pdc.h"        /* vbi_preselection */
#include "lang.h"	/* vbi_character_set */

VBI_BEGIN_DECLS

#ifndef VBI_DECODER
#define VBI_DECODER
typedef struct vbi_decoder vbi_decoder;
#endif

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
	((((r) & 0xFF) << 0) | (((g) & 0xFF) << 8)			\
	 | (((b) & 0xFF) << 16) | (0xFF << 24))
#define VBI_R(rgba) (((rgba) >> 0) & 0xFF)
#define VBI_G(rgba) (((rgba) >> 8) & 0xFF)
#define VBI_B(rgba) (((rgba) >> 16) & 0xFF)
#define VBI_A(rgba) (((rgba) >> 24) & 0xFF)

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
	 * renderer may also fall back to VBI_SEMI_TRANSPARENT or VBI_OPAQUE
	 * mode. For this case vbi_char->background names the color to use as
	 * the semi-transparent or opaque background.
	 *
	 * VBI_TRANSPARENT_SPACE is the opacity of subtitle pages (both border and
	 * characters, while the 'boxed' words are marked as VBI_SEMI_TRANSPARENT),
	 * but can also occur on a mainly VBI_OPAQUE page to create a 'window'
	 * effect.
 	 */
	VBI_TRANSPARENT_SPACE,
	/**
	 * Display video instead of the background color.
	 * Here the character is <em>not</em> a space and shall be displayed
	 * in vbi_char->foreground color. Only in the background of the character
	 * video shall look through. Again the renderer may fall back to
	 * VBI_SEMI_TRANSPARENT or VBI_OPAQUE.
	 */
	VBI_TRANSPARENT_FULL,
	/**
	 * Alpha blend video into background color, the
	 * character background becomes translucent. This is the opacity used
	 * for 'boxed' text on an otherwise VBI_TRANSPARENT_SPACE page, typically
	 * a subtitle or Teletext newsflash page. The renderer may fall back
	 * to VBI_OPAQUE.
	 */
	VBI_SEMI_TRANSPARENT,
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
	VBI_BOLD	= (1 << 1),	/**< Display character bold. */
	VBI_ITALIC	= (1 << 2),	/**< Display character slanted right. */
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
	VBI_PDC		= (1 << 7),
} vbi_attr;

/**
 * @ingroup Page
 * @brief Attributed character.
 */
typedef struct vbi_char {
	/**
	 * Character attribute, see vbi_attr.
	 */
	unsigned	attr		: 8;
	/**
	 * Character size, see vbi_size.
	 */
	unsigned	size		: 8;
	/**
	 * Character opacity, see vbi_opacity. Both @a foreground
	 * and @a background color are valid independent of @a opacity.
	 */
	unsigned	opacity		: 8;
	/**
	 * Character foreground color, a vbi_color index
	 * into the vbi_page->color_map.
	 */
	unsigned	foreground	: 8;
	/**
	 * Character background color, a vbi_color index
	 * into the vbi_page->color_map.
	 */
	unsigned	background	: 8;
	/**
	 * DRCS color look-up table offset, see vbi_page for details.
	 */
	unsigned	drcs_clut_offs	: 8;
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
	unsigned	unicode		: 16;
} vbi_char;

typedef struct _vbi_page_private vbi_page_private;

/**
 * @ingroup Page
 * @brief Formatted Teletext or Closed Caption page.
 *
 * Clients can fetch pages
 * from the respective cache using vbi_fetch_vt_page() or
 * vbi_fetch_cc_page() for evaluation, display or output. Since
 * the page may reference other objects in cache which are locked
 * by the fetch functions, vbi_unref_page() must be called when done.
 * Note this structure is large, some 10 KB.
 */
typedef struct {
	/* to be removed */
	vbi_decoder *		vbi;

	/**
	 * Points back to the source context.
	 */ 
	vbi_cache *		cache;

	/**
	 * Identifies the network broadcasting this page.
	 */
        vbi_nuid	       	nuid;

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
	 * @a screen_color is a vbi_color index into the @a color_map.
	 */
	vbi_color		screen_color;

	/**
	 * The 'border' can also have a distinct opacity. Usually it will
	 * be VBI_OPAQUE, but pages intended for overlay onto video
	 * (Teletext subtitles, newsflash, Caption pages) will have a
	 * @a screen_opacity of VBI_TRANSPARENT_SPACE.
	 * See vbi_opacity for details.
	 */
	vbi_opacity		screen_opacity;

	/**
	 * Ordinary characters are bi-level colored, with pixels assuming
	 * vbi_char->foreground or vbi_char->background color.
	 *
	 * The pixels of DRCS (Dynamically Redefinable Characters) can
	 * assume one color of a set of 2, 4 or 16.
	 *
	 * For bi-level DRCS vbi_char->drcs_clut_offs is 0. Zero pixels
	 * shall be painted in vbi_char->background color, one pixels in
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
	 * enumeration since Teletext allows editors to redefine
	 * the entire palette, depending on the Teletext level.
	 *
	 * Closed Caption and Teletext Level 1.0 / 1.5 pages use
	 * entries 0 ... 7. Teletext Level 2.5 / 3.5 pages use entries
	 * 0 ... 31. Navigation text (TOP, FLOF) added by libzvbi
	 * in row 25 uses entries 32 ... 39.
	 */
	vbi_rgba 		color_map[40];

	vbi_page_private *	priv;
} vbi_page;

extern vbi_page *
vbi_page_new			(void);
extern void
vbi_page_delete			(vbi_page *		pg);
extern vbi_page *
vbi_page_dup			(const vbi_page *	pg);
extern const uint8_t *
vbi_page_drcs_data		(const vbi_page *	pg,
				 unsigned int		unicode);
extern int
vbi_page_title(vbi_decoder *vbi, int pgno, int subno, char *buf);

extern vbi_bool
vbi_page_hyperlink		(const vbi_page *	pg,
				 vbi_link *		ld,
				 unsigned int		column,
				 unsigned int		row);
extern vbi_bool
vbi_page_nav_enum		(const vbi_page *	pg,
				 vbi_link *		ld,
				 unsigned int		index);
vbi_inline void
vbi_page_home_link		(const vbi_page *	pg,
				 vbi_link *		ld)
{
	vbi_page_nav_enum (pg, ld, 5);
}
vbi_bool
vbi_page_pdc_link		(const vbi_page *	pg,
				 vbi_preselection *	p,
				 unsigned int		column,
				 unsigned int		row);
vbi_bool
vbi_page_pdc_enum		(const vbi_page *	pg,
				 vbi_preselection *	p,
				 unsigned int		index);
extern const vbi_character_set *
vbi_page_character_set		(const vbi_page *	pg,
				 unsigned int		level);

typedef enum {
	VBI_TABLE = 0x32f54a00,
	VBI_RTL,
	VBI_SCALE,
	VBI_REVEAL,
	VBI_FLASH_ON,
	VBI_BRIGHTNESS,
	VBI_CONTRAST,
} vbi_export_option;

/* Private */

VBI_END_DECLS

#endif /* FORMAT_H */
