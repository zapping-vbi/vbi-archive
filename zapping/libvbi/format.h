/*
 *  Unified render format
 *
 *  Copyright (C) 2000-2001 Michael H. Schimek
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

/* $Id: format.h,v 1.15 2001-09-02 03:25:58 mschimek Exp $ */

#ifndef FORMAT_H
#define FORMAT_H

#include <inttypes.h>

/*
 *  WST/CC base palette, actual number of entries in WST mode
 *  is up to 64, 32 of which are redefinable architectural colours,
 *  the remaining are static private colours eg. for navigational
 *  purposes.
 *
 *  WST code depends on order, don't change.
 */
typedef enum {
	BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE,
} attr_colours;

/*
 *  Standard colour map entry, 0xAABBGGRR.
 */
typedef uint32_t	attr_rgba;

/*
 *  TRANSPARENT_SPACE:
 *    Display video at said position, ie. background is
 *    transparent and glyph is GL_SPACE. Fall back to
 *    SEMI_TRANSPARENT or OPAQUE mode as desired by user
 *    or mandated by hardware limitations. TRANSPARENT_SPACE
 *    may appear in an otherwise OPAQUE display to create
 *    a 'window' effect.
 *  TRANSPARENT_FULL:
 *    Display video in place of background colour. Fall
 *    back as above.
 *  SEMI_TRANSPARENT:
 *    Alpha blend video into background colour, aka.
 *    translucent background. This is the opacity of boxed
 *    text on an otherwise TRANSPARENT_SPACE display,
 *    ie. subtitles. Fall back to OPAQUE, suffice to consider
 *    anything not TRANSPARENT_SPACE OPAQUE.
 *  OPAQUE:
 *    Display foreground and background colour. Transparent
 *    background not recommended because of unknown colour
 *    inversion (eg. block graphics).
 *
 *  Note1 we have no opacity of transparent foreground, opaque
 *  background. Note2 the WST Level 2.5 transparent colour does
 *  not indicate opacity, it shall be displayed according to
 *  opacity flag and current colour map.
 */
typedef enum {
	TRANSPARENT_SPACE, TRANSPARENT_FULL, SEMI_TRANSPARENT, OPAQUE
} attr_opacity;

/*
 *  Character size flags, scan left to right:
 *
 *	N	DW  OT	    DH	    DS  OT
 *			    DH2	    DS2 OB
 *
 *  A DH2, DB2, OT, OB attr_char has the same glyph and
 *  attributes as the left/top anchor. Partial characters
 *  (ie. DH2 only) will not appear, so DH2, DB2, OT, OB can
 *  be safely ignored.
 *
 *  Code depends on order, don't change.
 */
typedef enum {
	NORMAL,	DOUBLE_WIDTH, DOUBLE_HEIGHT, DOUBLE_SIZE,
	OVER_TOP, OVER_BOTTOM, DOUBLE_HEIGHT2, DOUBLE_SIZE2
} attr_size;

/*
 *  Flash:
 *    Display glyph or GL_SPACE, 1 s cycle time.
 *  Conceal:
 *    Replace glyph by GL_SPACE if not revealed.
 *  Proportional:
 *    No function yet, default is fixed spacing.
 *  Link:
 *    Think hyperlink, call respective resolve(row, col)
 *    function to get more info.
 */
typedef struct {
	unsigned	underline	: 1;
	unsigned	bold		: 1;
	unsigned	italic		: 1;
	unsigned	flash		: 1;
	unsigned	conceal		: 1;
	unsigned	proportional	: 1;
	unsigned	link		: 1;
	unsigned			: 1;

	unsigned	size		: 4;	/* 3 bits attr_size */
	unsigned	opacity		: 4;	/* 2 bits attr_opacity */

	unsigned	foreground	: 8;	/* 6 bits attr_colours */
	unsigned	background	: 8;	/* 6 bits attr_colours */

	unsigned	glyph		: 32;	/* see lang.c for details */
} attr_char;

struct vbi; /* opaque */
struct vbi_font_descr; /* lang.h */

#ifndef ANY_SUB
#define ANY_SUB		0x3F7F
#endif

/*
 *  Unique network id (a Zapzilla thing),
 *  0 = unknown network, bit 31 reserved for preliminary nuids.
 */
typedef unsigned int nuid;

struct fmt_page
{
	/*
	 *  Source context
	 */
	struct vbi *		vbi;
        nuid	        	nuid;

	/*
	 *  Teletext page number, pgno 0x100 ... 0x8FF, subno 0 ... 0x3F7F
	 *  or Closed Caption channel, pgno 1 ... 8, subno 0.
	 */
	int			pgno;
	int			subno;

	int			rows;
	int			columns;

	attr_char		text[1056];

	struct {
	     /* int			x0, x1; */
		int			y0, y1;
		int			roll;
	}			dirty;

	/*
	 *  Colour and opacity outside the text area.
	 */
	int			screen_colour;
	attr_opacity		screen_opacity;

	/*
	 *  text[].foreground, .background,
	 *  drcs_clut, screen_colour.
	 */
	attr_rgba 		colour_map[40];

	/*
	 *  DRCS LUTs, see exp_gfx.c.
	 */
	uint8_t *		drcs_clut;		/* 64 entries */
	/* XXX these pointers reference cache directly  */
	uint8_t *		drcs[32];		/* 16 * 48 * 12 * 10 nibbles, LSN first */

	/* Private */

	struct {
		int			pgno, subno;
	}			nav_link[6];
	char			nav_index[64];

	struct vbi_font_descr *	font[2];
	unsigned int		double_height_lower;	/* legacy */

	attr_opacity		page_opacity[2];
	attr_opacity		boxed_opacity[2];
};

#endif /* FORMAT_H */
