#include "font.h"

#define static /* I want them global */
#ifndef FONTS_RUSSIAN
#include "font1.xbm"
#include "font2.xbm"
#else /* use russian glyphs instead */
#include "font3.xbm"
#include "font4.xbm"
#endif
