#include <string.h>
#include "misc.h"
#include "vt.h"
#include "lang.h"

/*
  Extra characters 1
  Latin G0 0x20
  Latin G0 0x40
  Latin G0 0x60
  Extra characters 2
  Latin G2 0x20
  Latin G2 0x40
  Latin G2 0x60
  Cyrillic G0 0x40
  Cyrillic G0 0x60
  Greek G0 0x40
  Greek G0 0x60
  Arabic G0 0x40
  Arabic G0 0x60
  Arabic G2 0x20
  Hebrew G0 0x60
  Block Mosaics G1 contiguous 0x20
  Block Mosaics G1 contiguous 0x60
  Smooth Mosaics G3 0x20
  Line Drawing G3 0x40
  mark * 640 + glyph;

  font designation ->
  glyph mapping
      (eg. <Greek> G0 base + character:
       0x20 char translation to assorted Latin G0 glyphs
       0x40 mapped to Greek glyphs 0x40
       0x60 mapped to Greek glyphs 0x60
       + translation of G0 national subset characters
         to Latin or Extra glyphs (if applicable)) ->
  modifier: italic (?), bold (?), underline, separated block mosaics (?) ->
  <render> or
  translation of glyphs to ANSI/Unicode ->
  <export>

  no character translation in cache
*/

typedef enum {
	LATIN_G0 = 1,
	LATIN_G2,
	CYRILLIC_1_G0,
	CYRILLIC_2_G0,
	CYRILLIC_3_G0,
	CYRILLIC_G2,
	GREEK_G0,
	GREEK_G2,
	ARABIC_G0,
	ARABIC_G2,
	HEBREW_G0,
	BLOCK_MOSAIC_G1,
	SMOOTH_MOSAIC_G3
} character_set;

typedef enum {
	NO_SUBSET,
	CZECH_SLOVAK,
	ENGLISH,
	ESTONIAN,
	FRENCH,
	GERMAN,
	ITALIAN,
	LETT_LITH,
	POLISH,
	PORTUGUESE_SPANISH,
	RUMANIAN, 
	SERB_CRO_SLO,
	SWE_FIN_HUN,
	TURKISH
} national_subset;

/*
    ETS 300 706 Table 32, 33, 34

    Character set designation hierarchy:
    
    * primary G0/G2 font and secondary G0
      font default code = 0

    * magazine G0/G2 code,
      packet M/29/4

    * magazine secondary G0 code,
      packet M/29/4

    * magazine G0/G2 code,
      packet M/29/0

    * magazine secondary G0 code,
      packet M/29/0

    * page G0/G2 code bits 0 ... 2,
      header flags C12, C13, C14

    * page G0/G2 code, 0 ... 2 same
      as header, packet X/28/4

    * page secondary G0 code,
      packet X/28/4

    * page G0/G2 code, 0 ... 2 same
      as header, packet X/28/0-1

    * page secondary G0 code,
      packet X/28/0-1

    * level 1 alpha/mosaic spacing attribute
      and primary/secondary G0 escape code

    * enhancement data triplet
      - G0 character
      - G2 character
      - block mosaic G1 character
      - smooth mosaic G3 character
      - G0/G2 composed characters
      - G0/G2 code for current row
        (no national subset)

    ------
    Primary: G0 w/national subset, G2
    Secondary: G0 w/national subset

*/

struct {
	int		code;
	character_set	G0;
	character_set	G2;
	national_subset	subset;
} font_designation[] = {
	{ 0,	LATIN_G0, LATIN_G2, ENGLISH		},
	{ 1,	LATIN_G0, LATIN_G2, GERMAN		},
	{ 2,	LATIN_G0, LATIN_G2, SWE_FIN_HUN		},
	{ 3,	LATIN_G0, LATIN_G2, ITALIAN		},
	{ 4,	LATIN_G0, LATIN_G2, FRENCH		},
	{ 5,	LATIN_G0, LATIN_G2, PORTUGUESE_SPANISH	},
	{ 6,	LATIN_G0, LATIN_G2, CZECH_SLOVAK	},
	{ 7,	LATIN_G0, LATIN_G2, NO_SUBSET		},

	{ 8,	LATIN_G0, LATIN_G2, POLISH		},
	{ 9,	LATIN_G0, LATIN_G2, GERMAN		},
	{ 10,	LATIN_G0, LATIN_G2, SWE_FIN_HUN		},
	{ 11,	LATIN_G0, LATIN_G2, ITALIAN		},
	{ 12,	LATIN_G0, LATIN_G2, FRENCH		},
	{ 13,	LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ 14,	LATIN_G0, LATIN_G2, CZECH_SLOVAK	},
	{ 15,	LATIN_G0, LATIN_G2, NO_SUBSET		},
	
	{ 16,	LATIN_G0, LATIN_G2, ENGLISH		},
	{ 17,	LATIN_G0, LATIN_G2, GERMAN		},
	{ 18,	LATIN_G0, LATIN_G2, SWE_FIN_HUN		},
	{ 19,	LATIN_G0, LATIN_G2, ITALIAN		},
	{ 20,	LATIN_G0, LATIN_G2, FRENCH		},
	{ 21,	LATIN_G0, LATIN_G2, PORTUGUESE_SPANISH	},
	{ 22,	LATIN_G0, LATIN_G2, TURKISH		},
	{ 23,	LATIN_G0, LATIN_G2, NO_SUBSET		},

	{ 24,	LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ 25,	LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ 26,	LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ 27,	LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ 28,	LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ 29,	LATIN_G0, LATIN_G2, SERB_CRO_SLO	},
	{ 30,	LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ 31,	LATIN_G0, LATIN_G2, RUMANIAN		},

	{ 32,	CYRILLIC_1_G0, CYRILLIC_G2, NO_SUBSET	},
	{ 33,	LATIN_G0, LATIN_G2, GERMAN		},
	{ 34,	LATIN_G0, LATIN_G2, ESTONIAN		},
	{ 35,	LATIN_G0, LATIN_G2, LETT_LITH		},
	{ 36,	CYRILLIC_2_G0, CYRILLIC_G2, NO_SUBSET	},
	{ 37,	CYRILLIC_3_G0, CYRILLIC_G2, NO_SUBSET	},
	{ 38,	LATIN_G0, LATIN_G2, CZECH_SLOVAK	},

	{ 54,	LATIN_G0, LATIN_G2, TURKISH		},
	{ 55,	GREEK_G0, GREEK_G2, NO_SUBSET		},

	{ 64,	LATIN_G0, ARABIC_G2, ENGLISH		},
	{ 68,	LATIN_G0, ARABIC_G2, FRENCH		},
	{ 71,	ARABIC_G0, ARABIC_G2, NO_SUBSET		},

	{ 85,	HEBREW_G0, ARABIC_G2, NO_SUBSET		},
	{ 87,	ARABIC_G0, ARABIC_G2, NO_SUBSET		},
};















int latin1 = -1;

static u8 lang_char[256];

static u8 lang_chars[1+8+8][16] =
{
    { 0, 0x23,0x24,0x40,0x5b,0x5c,0x5d,0x5e,0x5f,0x60,0x7b,0x7c,0x7d,0x7e },

    // for latin-1 font
    // English (100%)
    { 0,  '£', '$', '@', '«', '½', '»', '¬', '#', '­', '¼', '¦', '¾', '÷' },
    // German (100%)
    { 0,  '#', '$', '§', 'Ä', 'Ö', 'Ü', '^', '_', '°', 'ä', 'ö', 'ü', 'ß' },
    // Swedish/Finnish/Hungarian (100%)
    { 0,  '#', '¤', 'É', 'Ä', 'Ö', 'Å', 'Ü', '_', 'é', 'ä', 'ö', 'å', 'ü' },
    // Italian (100%)
    { 0,  '£', '$', 'é', '°', 'ç', '»', '¬', '#', 'ù', 'à', 'ò', 'è', 'ì' },
    // French (100%)
    { 0,  'é', 'ï', 'à', 'ë', 'ê', 'ù', 'î', '#', 'è', 'â', 'ô', 'û', 'ç' },
    // Portuguese/Spanish (100%)
    { 0,  'ç', '$', '¡', 'á', 'é', 'í', 'ó', 'ú', '¿', 'ü', 'ñ', 'è', 'à' },
    // Czech/Slovak (60%)
    { 0,  '#', 'u', 'c', 't', 'z', 'ý', 'í', 'r', 'é', 'á', 'e', 'ú', 's' },
    // reserved (English mapping)
    { 0,  '£', '$', '@', '«', '½', '»', '¬', '#', '­', '¼', '¦', '¾', '÷' },

    // for latin-2 font
    // Polish (100%)
    { 0,  '#', 'ñ', '±', '¯', '¦', '£', 'æ', 'ó', 'ê', '¿', '¶', '³', '¼' },
    // German (100%)
    { 0,  '#', '$', '§', 'Ä', 'Ö', 'Ü', '^', '_', '°', 'ä', 'ö', 'ü', 'ß' },
    // Estonian (100%)
    { 0,  '#', 'õ', '©', 'Ä', 'Ö', '®', 'Ü', 'Õ', '¹', 'ä', 'ö', '¾', 'ü' },
    // Lettish/Lithuanian (90%)
    { 0,  '#', '$', '©', 'ë', 'ê', '®', 'è', 'ü', '¹', '±', 'u', '¾', 'i' },
    // French (90%)
    { 0,  'é', 'i', 'a', 'ë', 'ì', 'u', 'î', '#', 'e', 'â', 'ô', 'u', 'ç' },
    // Serbian/Croation/Slovenian (100%)
    { 0,  '#', 'Ë', 'È', 'Æ', '®', 'Ð', '©', 'ë', 'è', 'æ', '®', 'ð', '¹' },
    // Czech/Slovak (100%)
    { 0,  '#', 'ù', 'è', '»', '¾', 'ý', 'í', 'ø', 'é', 'á', 'ì', 'ú', '¹' },
    // Rumanian (95%)
    { 0,  '#', '¢', 'Þ', 'Â', 'ª', 'Ã', 'Î', 'i', 'þ', 'â', 'º', 'ã', 'î' },
};

/* Yankable latin charset :-)
     !"#$%&'()*+,-./0123456789:;<=>?
    @ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_
    `abcdefghijklmnopqrstuvwxyz{|}~
     ¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿
    ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞß
    àáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ
*/



static struct mark { u8 *g0, *latin1, *latin2; } marks[16] =
{
    /* none */		{ "#",
    			  "¤",
			  "$"					},
    /* grave - ` */	{ " aeiouAEIOU",
    			  "`àèìòùÀÈÌÒÙ",
			  "`aeiouAEIOU"				},
    /* acute - ' */	{ " aceilnorsuyzACEILNORSUYZ",
			  "'ácéílnórsúýzÁCÉÍLNÓRSÚÝZ",
			  "'áæéíåñóà¶úý¼ÁÆÉÍÅÑÓÀ¦ÚÝ¬"		},
    /* cirumflex - ^ */	{ " aeiouAEIOU",
    			  "^âêîôûÂÊÎÔÛ",
			  "^âeîôuÂEÎÔU"				},
    /* tilde - ~ */	{ " anoANO",
    			  "~ãñõÃÑÕ",
			  "~anoANO"				},
    /* ??? - ¯ */	{ "",
    			  "",
			  ""					},
    /* breve - u */	{ "aA",
    			  "aA",
			  "ãÃ"					},
    /* abovedot - · */	{ "zZ",
    			  "zZ",
			  "¿¯"					},
    /* diaeresis ¨ */	{ "aeiouAEIOU",
    			  "äëïöüÄËÏÖÜ",
			  "äëiöüÄËIÖÜ"				},
    /* ??? - . */	{ "",
    			  "",
			  ""					},
    /* ringabove - ° */	{ " auAU",
    			  "°åuÅU",
			  "°aùAÙ"				},
    /* cedilla - ¸ */	{ "cstCST",
    			  "çstÇST",
			  "çºþÇªÞ"				},
    /* ??? - _ */	{ " ",
    			  "_",
			  "_"					},
    /* dbl acute - " */	{ " ouOU",
    			  "\"ouOU",
			  "\"õûÕÛ"				},
    /* ogonek - \, */	{ "aeAE",
    			  "aeAE",
			  "±ê¡Ê"				},
    /* caron - v */	{ "cdelnrstzCDELNRSTZ",
			  "cdelnrstzCDELNRSTZ",
			  "èïìµòø¹»¾ÈÏÌ¥ÒØ©«®"			},
};

static u8 g2map_latin1[] =
   /*0123456789abcdef*/
    " ¡¢£$¥#§¤'\"«    "
    "°±²³×µ¶·÷'\"»¼½¾¿"
    " `´^~   ¨.°¸_\"  "
    "_¹®©            "
    " ÆÐªH ILLØ ºÞTNn"
    "Kædðhiillø ßþtn\x7f";

static u8 g2map_latin2[] =
   /*0123456789abcdef*/
    " icL$Y#§¤'\"<    "
    "°   ×u  ÷'\">    "
    " `´^~ ¢ÿ¨.°¸_½²·"
    "- RC            "
    "  ÐaH iL£O opTNn"
    "K ðdhiil³o ßptn\x7f";



void
lang_init(void)
{
    int i;

    memset(lang_char, 0, sizeof(lang_char));
    for (i = 1; i <= 13; i++)
	lang_char[lang_chars[0][i]] = i;
}


void
conv2latin(u8 *p, int n, int lang)
{
    int c, gfx = 0;

    while (n--)
    {
	if (lang_char[c = *p])
	{
	    if (not gfx || (c & 0xa0) != 0x20)
		*p = lang_chars[lang + 1][lang_char[c]];
	}
	else if ((c & 0xe8) == 0)
	    gfx = c & 0x10;
	p++;
    }
}



void
init_enhance(struct enhance *eh)
{
    eh->next_des = 0;
}

void
add_enhance(struct enhance *eh, int dcode, u32 *t)
{
    if (dcode == eh->next_des)
    {
	memcpy(eh->trip + dcode * 13, t, 13 * sizeof(*t));
	eh->next_des++;
    }
    else
	eh->next_des = -1;
}

void
enhance(struct enhance *eh, struct vt_page *vtp)
{
    int row = 0;
    u32 *p, *e;

    if (eh->next_des < 1)
	return;

    for (p = eh->trip, e = p + eh->next_des * 13; p < e; p++)
	if (*p % 2048 != 2047)
	{
	    int adr = *p % 64;
	    int mode = *p / 64 % 32;
	    int data = *p / 2048 % 128;

//printf("ENH %2d %02x %02x\n", adr, mode, data);

	    //printf("%2x,%d,%d ", mode, adr, data);
	    if (adr < 40)
	    {
		// col functions
		switch (mode)
		{
		    case 15: // char from G2 set
			if (adr < W && row < H) {
			    if (latin1)
				vtp->data[row][adr] = g2map_latin1[data-32];
			    else
				vtp->data[row][adr] = g2map_latin2[data-32];
			}
			break;
		    case 16 ... 31: // char from G0 set with diacritical mark
			if (adr < W && row < H)
			{
			    struct mark *mark = marks + (mode - 16);
			    u8 *x;

			    if ((x = strchr(mark->g0, data))) {
				if (latin1)
				    data = mark->latin1[x - mark->g0];
				else
				    data = mark->latin2[x - mark->g0];
			    }
			    vtp->data[row][adr] = data;
			}
			break;
		}
	    }
	    else
	    {
		// row functions
		if ((adr -= 40) == 0)
		    adr = 24;
		
		switch (mode)
		{
		    case 1: // full row color
			row = adr;
			break;
		    case 4: // set active position
			row = adr;
			break;
		    case 7: // address row 0 (+ full row color)
			if (adr == 23)
			    row = 0;
			break;
		}
	    }
	}
    //printf("\n");
}
