#ifndef FONT_H
#define FONT_H

/*
  code: 0 -> Latin
  code: 1 -> Cyrillic (Russian)
  The glyphs are described in ETS 300 706, available free of charge
  from http://www.etsi.org
*/
void vbi_set_glyphs(int code);
int  vbi_get_glyphs(void);

/*
  Returns the glyph bitmap (table: [1..2])
*/
unsigned char *
vbi_get_glyph_bitmap(int table);

#include "fontsize.h"	/* the #defines from font?.xbm */

#define font_width	font1_width
#define font_height	font1_height
#define CW		(font_width/32)        /* pixel width of a character */
#define CH		(font_height/8)        /* pixel height of a character */

#endif
