/*
 *  libzvbi -- Teletext character set
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

/* $Id: ttx_charset.h,v 1.1.2.1 2008-08-19 10:56:06 mschimek Exp $ */

#ifndef __ZVBI_TTX_CHARSET_H__
#define __ZVBI_TTX_CHARSET_H__

#include "zvbi/macros.h"

VBI_BEGIN_DECLS

/**
 * @brief Teletext character set code.
 *
 * A character set code combines a vbi_charset with a vbi_subset.
 * Character set codes are defined in EN 300 706 Section 15, Table
 * 33. Valid codes are in range 0 ... 127.
 *
 * See also vbi_ttx_charset_from_code().
 */
typedef unsigned int vbi_ttx_charset_code;

#define VBI_TTX_CHARSET_CODE_NONE ((vbi_ttx_charset_code) -1)

/**
 * @brief Teletext character sets.
 * EN 300 706 Section 15.
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
 * EN 300 706 Section 15.2 and Section 15.6.2 Table 36.
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
 * EN 300 706 Section 15, Table 32, 33 and 34.
 */
typedef struct {
	vbi_ttx_charset_code	code;

	/** Character set used for G0 characters (0x20 ... 0x7F). */
	vbi_charset		g0;

	/** Character set used for G2 characters (0xA0 ... 0xFF). */
	vbi_charset		g2;

	/** National character subset used with VBI_CHARSET_LATIN_G0. */
	vbi_subset		subset;

	/**
	 * Teletext character set codes do not directly correspond to
	 * languages, but they may be the only information available
	 * about the language of Teletext subtitles. (Networks may
	 * transmit a Teletext descriptor with a proper language code
	 * in the DVB SI table as defined in EN 300 468 Section
	 * 6.2.42.)
	 * 
	 * The languages covered by this character set according to EN
	 * 300 706. These are NUL-terminated ISO 639-2 Alpha-3 codes,
	 * for example "fra" for French.
	 */
	const char *		language_code[8];

	/**
	 * Number of language codes in the @a language_code array.
	 */
	unsigned int		n_language_codes;
} vbi_ttx_charset;

extern const vbi_ttx_charset *
vbi_ttx_charset_from_code	(vbi_ttx_charset_code	code)
  _vbi_const;
extern unsigned int
vbi_ttx_unicode			(vbi_charset		charset,
				 vbi_subset		subset,
				 unsigned int		c)
  _vbi_const;
extern unsigned int
vbi_ttx_composed_unicode	(unsigned int		a,
				 unsigned int		c)
  _vbi_const;
extern vbi_bool
vbi_is_print			(unsigned int		unicode)
  _vbi_const;
extern vbi_bool
vbi_is_gfx			(unsigned int		unicode)
  _vbi_const;
extern vbi_bool
vbi_is_drcs			(unsigned int		unicode)
  _vbi_const;

VBI_END_DECLS

#endif /* __ZVBI_TTX_CHARSET_H__ */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
