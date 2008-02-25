/*
 *  libzvbi - Teletext and Closed Caption character set
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

/* $Id: lang.h,v 1.2.2.10 2008-02-25 20:58:02 mschimek Exp $ */

#ifndef __ZVBI3_LANG_H__
#define __ZVBI3_LANG_H__

#include "macros.h"

VBI3_BEGIN_DECLS

/**
 * @brief Teletext character set code.
 *
 * A character set code combines one vbi3_charset with
 * a vbi3_subset. See vbi3_ttx_charset_from_code().
 *
 * Defined in EN 300 706, Section 15, Table 33.
 * Valid codes are in range 0 ... 127.
 */
typedef unsigned int vbi3_ttx_charset_code;

#define VBI3_TTX_CHARSET_CODE_NONE ((vbi3_ttx_charset_code) -1)

/**
 * @brief Teletext character sets.
 * Defined in EN 300 706, Section 15.
 */
typedef enum {
	VBI3_CHARSET_NONE,
	VBI3_CHARSET_LATIN_G0,
	VBI3_CHARSET_LATIN_G2,
	VBI3_CHARSET_CYRILLIC1_G0,
	VBI3_CHARSET_CYRILLIC2_G0,
	VBI3_CHARSET_CYRILLIC3_G0,
	VBI3_CHARSET_CYRILLIC_G2,
	VBI3_CHARSET_GREEK_G0,
	VBI3_CHARSET_GREEK_G2,
	VBI3_CHARSET_ARABIC_G0,
	VBI3_CHARSET_ARABIC_G2,
	VBI3_CHARSET_HEBREW_G0,
	VBI3_CHARSET_BLOCK_MOSAIC_G1,
	VBI3_CHARSET_SMOOTH_MOSAIC_G3
} vbi3_charset;

/**
 * @brief Teletext Latin G0 national option subsets.
 * Defined in EN 300 706, Section 15.2 and Section 15.6.2 Table 36.
 */
typedef enum {
	VBI3_SUBSET_NONE,
	VBI3_SUBSET_CZECH_SLOVAK,
	VBI3_SUBSET_ENGLISH,
	VBI3_SUBSET_ESTONIAN,
	VBI3_SUBSET_FRENCH,
	VBI3_SUBSET_GERMAN,
	VBI3_SUBSET_ITALIAN,
	VBI3_SUBSET_LETTISH_LITHUANIAN,
	VBI3_SUBSET_POLISH,
	VBI3_SUBSET_PORTUGUESE_SPANISH,
	VBI3_SUBSET_RUMANIAN,
	VBI3_SUBSET_SERBIAN_CROATIAN_SLOVENIAN,
	VBI3_SUBSET_SWEDISH_FINNISH_HUNGARIAN,
	VBI3_SUBSET_TURKISH
} vbi3_subset;

/**
 * @brief Teletext character set designation.
 * Defined in EN 300 706, Section 15, Table 32, 33 and 34.
 */
typedef struct {
	vbi3_ttx_charset_code	code;

	/** Character set used for G0 characters (0x20 ... 0x7F). */
	vbi3_charset		g0;
	/** Character set used for G2 characters (0xA0 ... 0xFF). */
	vbi3_charset		g2;
	/** National character subset used with VBI3_CHARSET_LATIN_G0. */
	vbi3_subset		subset;
	/**
	 * Languages covered. These are ISO 639 two-character language code
	 * strings, for example "fr" for French. The array is terminated
	 * by a NULL pointer.
	 */
	const char *		language_code[16];
} vbi3_ttx_charset;

extern const vbi3_ttx_charset *
vbi3_ttx_charset_from_code	(vbi3_ttx_charset_code	code)
  _vbi3_const;

extern unsigned int
vbi3_teletext_unicode		(vbi3_charset		charset,
				 vbi3_subset		subset,
				 unsigned int		c)
  _vbi3_const;
extern unsigned int
vbi3_caption_unicode		(unsigned int		c,
				 vbi3_bool		to_upper)
  _vbi3_const;

/**
 * @param unicode Unicode as in vbi3_char.
 * 
 * @return
 * @c TRUE if @a unicode represents a Teletext or Closed Caption
 * printable character. This excludes Teletext Arabic characters (which
 * are represented by private codes U+E600 ... U+E7FF until the conversion
 * table is ready), the Teletext Turkish currency sign U+E800 which is not
 * representable in Unicode, the Teletext G1 Block Mosaic and G3 Smooth
 * Mosaics and Line Drawing Set, with codes U+EE00 ... U+EFFF, and
 * Teletext DRCS coded U+F000 ... U+F7FF.
 */
_vbi3_inline vbi3_bool
vbi3_is_print			(unsigned int		unicode)
{
	return unicode < 0xE600;
}

/**
 * @param unicode Unicode as in vbi3_char.
 * 
 * @return
 * @c TRUE if @a unicode represents a Teletext G1 Block Mosaic or G3 Smooth
 * Mosaics and Line Drawing Set, that is a code in range U+EE00 ... U+EFFF.
 */
_vbi3_inline vbi3_bool
vbi3_is_gfx			(unsigned int		unicode)
{
	return unicode >= 0xEE00 && unicode <= 0xEFFF;
}

/**
 * @param unicode Unicode as in vbi3_char.
 * 
 * @return
 * @c TRUE if @a unicode represents a Teletext DRCS (Dynamically
 * Redefinable Character), that is a code in range U+F000 ... U+F7FF.
 **/
_vbi3_inline vbi3_bool
vbi3_is_drcs			(unsigned int		unicode)
{
	return unicode >= 0xF000;
}

/* Private */

extern unsigned int
_vbi3_teletext_ascii_art		(unsigned int		c)
  _vbi3_const;
extern unsigned int
vbi3_teletext_composed_unicode	(unsigned int		a,
				 unsigned int		c)
  _vbi3_const;

VBI3_END_DECLS

#endif /* __ZVBI3_LANG_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
