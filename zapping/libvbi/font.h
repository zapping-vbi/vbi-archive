#ifndef FONT_H
#define FONT_H

/* Uncomment this line to get Cyrillic Russian fonts */
// #define FONTS_RUSSIAN 1
/*
  FIXME: This is a hot fix, we should allow dynamic switching between
  fonts, not difficult once we have the glyphs ;-)
*/
/* Replace the latin glyphs with the russian ones */
#ifdef FONTS_RUSSIAN
#define font1_bits font3_bits
#define font2_bits font4_bits
#define font1_width font3_width
#define font1_height font3_height
#define font2_width font4_width
#define font2_height font4_height
#endif


#include "fontsize.h"	/* the #defines from font?.xbm */

#if font1_width != font2_width || font1_height != font2_height
#error different font sizes.
#endif

extern unsigned char font1_bits[];
extern unsigned char font2_bits[];

#define font_width	font1_width
#define font_height	font1_height
#define CW		(font_width/32)        /* pixel width of a character */
#define CH		(font_height/8)        /* pixel height of a character */

#endif
