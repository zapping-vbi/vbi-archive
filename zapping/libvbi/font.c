#include "font.h"


static int lang_code=0; /* Start with latin glyphs */

/*
  code: 0 -> Latin
  code: 1 -> Cyrillic (Russian)
*/
void vbi_set_glyphs(int code)
{
  if (code < 0)
    code = 0;
  if (code > 1)
    code = 1;
  lang_code = code;
}

int vbi_get_glyphs(void)
{
  return lang_code;
}

unsigned char *
vbi_get_glyph_bitmap(int table)
{
  if (table < 1)
    table = 1;
  if (table > 2)
    table = 2;
  switch (table+(lang_code*2))
    {
    case 2:
      return font2_bits;
    case 3:
      return font3_bits;
    case 4:
      return font4_bits;
    }

  return font1_bits;
}
