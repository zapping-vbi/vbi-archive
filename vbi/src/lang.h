/*
 *  libzvbi - Teletext and Closed Caption character set
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

/* $Id: lang.h,v 1.2.2.5 2004-04-05 04:42:27 mschimek Exp $ */

#ifndef __ZVBI_LANG_H__
#define __ZVBI_LANG_H__

#include "macros.h"

/* Public */

VBI_BEGIN_DECLS

/**
 * @brief Teletext character set code.
 *
 * A character set code combines one vbi_charset with
 * a vbi_subset. See vbi_character_set_from_code().
 *
 * Defined in EN 300 706, Section 15, Table 33.
 * Valid codes are in range 0 ... 127.
 */
typedef unsigned int vbi_character_set_code;

/**
 * @brief Teletext character sets.
 * Defined in EN 300 706, Section 15.
 */
typedef enum {
	VBI_CHARSET_NONE,
	VBI_CHARSET_LATIN_G0,
	VBI_CHARSET_LATIN_G2,
	VBI_CHARSET_CYRILLIC1_G0,
	VBI_CHARSET_CYRILLIC2_G0,
	VBI_CHARSET_CYRILLIC3_G0,
	VBI_CHARSET_CYRILLIC_G2,
	VBI_CHARSET_GREEK_G0,
	VBI_CHARSET_GREEK_G2,
	VBI_CHARSET_ARABIC_G0,
	VBI_CHARSET_ARABIC_G2,
	VBI_CHARSET_HEBREW_G0,
	VBI_CHARSET_BLOCK_MOSAIC_G1,
	VBI_CHARSET_SMOOTH_MOSAIC_G3
} vbi_charset;

/**
 * @brief Teletext Latin G0 national option subsets.
 * Defined in EN 300 706, Section 15.2 and Section 15.6.2 Table 36.
 */
typedef enum {
	VBI_SUBSET_NONE,
	VBI_SUBSET_CZECH_SLOVAK,
	VBI_SUBSET_ENGLISH,
	VBI_SUBSET_ESTONIAN,
	VBI_SUBSET_FRENCH,
	VBI_SUBSET_GERMAN,
	VBI_SUBSET_ITALIAN,
	VBI_SUBSET_LETTISH_LITHUANIAN,
	VBI_SUBSET_POLISH,
	VBI_SUBSET_PORTUGUESE_SPANISH,
	VBI_SUBSET_RUMANIAN,
	VBI_SUBSET_SERBIAN_CROATIAN_SLOVENIAN,
	VBI_SUBSET_SWEDISH_FINNISH_HUNGARIAN,
	VBI_SUBSET_TURKISH
} vbi_subset;

/**
 * @brief Teletext character set designation.
 * Defined in EN 300 706, Section 15, Table 32, 33 and 34.
 */
typedef struct {
	vbi_character_set_code	code;

	/** Character set used for G0 characters (0x20 ... 0x7F). */
	vbi_charset		g0;
	/** Character set used for G2 characters (0xA0 ... 0xFF). */
	vbi_charset		g2;
	/** National character subset used with VBI_CHARSET_LATIN_G0. */
	vbi_subset		subset;
	/**
	 * Languages covered. These are ISO 639 two-character language code
	 * strings, for example "fr" for French. The array is terminated
	 * by a NULL pointer.
	 */
	const char *		language_code[16];
} vbi_character_set;

extern const vbi_character_set *
vbi_character_set_from_code	(vbi_character_set_code code);

extern unsigned int
vbi_teletext_unicode		(vbi_charset		charset,
				 vbi_subset		subset,
				 unsigned int		c);
extern unsigned int
vbi_caption_unicode		(unsigned int		c);

/**
 * @param unicode Unicode as in vbi_char.
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
vbi_inline vbi_bool
vbi_is_print			(unsigned int		unicode)
{
	return unicode < 0xE600;
}

/**
 * @param unicode Unicode as in vbi_char.
 * 
 * @return
 * @c TRUE if @a unicode represents a Teletext G1 Block Mosaic or G3 Smooth
 * Mosaics and Line Drawing Set, that is a code in range U+EE00 ... U+EFFF.
 */
vbi_inline vbi_bool
vbi_is_gfx			(unsigned int		unicode)
{
	return unicode >= 0xEE00 && unicode <= 0xEFFF;
}

/**
 * @param unicode Unicode as in vbi_char.
 * 
 * @return
 * @c TRUE if @a unicode represents a Teletext DRCS (Dynamically
 * Redefinable Character), that is a code in range U+F000 ... U+F7FF.
 **/
vbi_inline vbi_bool
vbi_is_drcs			(unsigned int		unicode)
{
	return unicode >= 0xF000;
}

/* Private */

extern unsigned int
_vbi_teletext_ascii_art		(unsigned int		c);
extern unsigned int
_vbi_teletext_composed_unicode	(unsigned int		a,
				 unsigned int		c);
/* Public */

VBI_END_DECLS

/* Private */

#endif /* __ZVBI_LANG_H__ */
