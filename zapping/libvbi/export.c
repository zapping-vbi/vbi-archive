#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "vt.h"
#include "misc.h"
#include "export.h"
#include "vbi.h"
#include "hamm.h"

#include "../common/types.h"
#include "../common/math.h"

extern struct export_module export_txt[1];
extern struct export_module export_ansi[1];
extern struct export_module export_html[1];
extern struct export_module export_png[1];
extern struct export_module export_ppm[1];

struct export_module *modules[] =
{
    export_txt,
    export_ansi,
    export_html,
    export_ppm,
#ifdef WITH_PNG
    export_png,
#endif
    0
};

static char *glbl_opts[] =
{
    "reveal",		// show hidden text
    "hide",		// don't show hidden text (default)
    0
};

static char errbuf[64];

void
export_error(char *str, ...)
{
    va_list args;

    va_start(args, str);
    vsnprintf(errbuf, sizeof(errbuf)-1, str, args);
}

char *
export_errstr(void)
{
    return errbuf;
}


static int
find_opt(char **opts, char *opt, char *arg)
{
    int err = 0;
    char buf[256];
    char **oo, *o, *a;

    if ((oo = opts))
	while ((o = *oo++))
	{
	    if ((a = strchr(o, '=')))
	    {
		a = buf + (a - o);
		o = strcpy(buf, o);
		*a++ = 0;
	    }
	    if (strcasecmp(o, opt) == 0)
	    {
		if ((a != 0) == (arg != 0))
		    return oo - opts;
		err = -1;
	    }
	}
    return err;
}


struct export *
export_open(char *fmt)
{
    struct export_module **eem, *em;
    struct export *e;
    char *opt, *optend, *optarg;
    int opti;

    if ((fmt = strdup(fmt)))
    {
	if ((opt = strchr(fmt, ',')))
	    *opt++ = 0;
	for (eem = modules; (em = *eem); eem++)
	    if (strcasecmp(em->fmt_name, fmt) == 0)
		break;
	if (em)
	{
	    if ((e = malloc(sizeof(*e) + em->local_size)))
	    {
		e->mod = em;
		e->fmt_str = fmt;
		e->reveal = 0;
		memset(e + 1, 0, em->local_size);
		if (not em->open || em->open(e) == 0)
		{
		    for (; opt; opt = optend)
		    {
			if ((optend = strchr(opt, ',')))
			    *optend++ = 0;
			if (not *opt)
			    continue;
			if ((optarg = strchr(opt, '=')))
			    *optarg++ = 0;
			if ((opti = find_opt(glbl_opts, opt, optarg)) > 0)
			{
			    if (opti == 1) // reveal
				e->reveal = 1;
			    else if (opti == 2) // hide
				e->reveal = 0;
			}
			else if (opti == 0 &&
				(opti = find_opt(em->options, opt, optarg)) > 0)
			{
			    if (em->option(e, opti, optarg))
				break;
			}
			else
			{
			    if (opti == 0)
				export_error("%s: unknown option", opt);
			    else if (optarg)
				export_error("%s: takes no arg", opt);
			    else
				export_error("%s: missing arg", opt);
			    break;
			}
		    }
		    if (opt == 0)
			return e;

		    if (em->close)
			em->close(e);
		}
		free(e);
	    }
	    else
		export_error("out of memory");
	}
	else
	    export_error("unknown format: %s", fmt);
	free(fmt);
    }
    else
	export_error("out of memory");
    return 0;
}


void
export_close(struct export *e)
{
    if (e->mod->close)
	e->mod->close(e);
    free(e->fmt_str);
    free(e);
}


static char *
hexnum(char *buf, unsigned int num)
{
    char *p = buf + 5;

    num &= 0xffff;
    *--p = 0;
    do
    {
	*--p = "0123456789abcdef"[num % 16];
	num /= 16;
    } while (num);
    return p;
}

static char *
adjust(char *p, char *str, char fill, int width)
{
    int l = width - strlen(str);

    while (l-- > 0)
	*p++ = fill;
    while ((*p = *str++))
	p++;
    return p;
}

char *
export_mkname(struct export *e, char *fmt, struct vt_page *vtp, char *usr)
{
    char bbuf[1024];
    char *p = bbuf;

    while ((*p = *fmt++))
	if (*p++ == '%')
	{
	    char buf[32], buf2[32];
	    int width = 0;

	    p--;
	    while (*fmt >= '0' && *fmt <= '9')
		width = width*10 + *fmt++ - '0';

	    switch (*fmt++)
	    {
		case '%':
		    p = adjust(p, "%", '%', width);
		    break;
		case 'e':	// extension
		    p = adjust(p, e->mod->extension, '.', width);
		    break;
		case 'p':	// pageno[.subno]
		    if (vtp->subno)
			p = adjust(p,strcat(strcat(hexnum(buf, vtp->pgno),
				"."), hexnum(buf2, vtp->subno)), ' ', width);
		    else
			p = adjust(p, hexnum(buf, vtp->pgno), ' ', width);
		    break;
		case 'S':	// subno
		    p = adjust(p, hexnum(buf, vtp->subno), '0', width);
		    break;
		case 'P':	// pgno
		    p = adjust(p, hexnum(buf, vtp->pgno), '0', width);
		    break;
		case 's':	// user strin
		    p = adjust(p, usr, ' ', width);
		    break;
		//TODO: add date, channel name, ...
	    }
	}
    p = strdup(bbuf);
    if (not p)
	export_error("out of memory");
    return p;
}


#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))

/*
    ETS 300 706 Table 32, 33, 34

    Character set designation hierarchy:
    
    * primary G0/G2 font and secondary G0
      font default code = 0
done

    * magazine G0/G2 code,
      packet M/29/4
done

    * magazine secondary G0 code,
      packet M/29/4
done

    * magazine G0/G2 code,
      packet M/29/0
done

    * magazine secondary G0 code,
      packet M/29/0
done

    * page G0/G2 (primary) code bits 0 ... 2,
      header flags C12, C13, C14
done

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
done

    * enhancement data triplet
done
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
	PORTUG_SPANISH,
	RUMANIAN,
	SERB_CRO_SLO,
	SWE_FIN_HUN,
	TURKISH
} national_subset;

#define VALID_CHARACTER_SET(n) ((n) < 88 && font_d_table[n].G0)

struct font_d {
	character_set	G0;
	character_set	G2;
	national_subset	subset;	/* applies only to LATIN_G0 */
} font_d_table[88] = {
	/* 0 - Western and Central Europe */
	{ LATIN_G0, LATIN_G2, ENGLISH		},
	{ LATIN_G0, LATIN_G2, GERMAN		},
	{ LATIN_G0, LATIN_G2, SWE_FIN_HUN	},
	{ LATIN_G0, LATIN_G2, ITALIAN		},
	{ LATIN_G0, LATIN_G2, FRENCH		},
	{ LATIN_G0, LATIN_G2, PORTUG_SPANISH	},
	{ LATIN_G0, LATIN_G2, CZECH_SLOVAK	},
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},

	/* 8 - Eastern Europe */
	{ LATIN_G0, LATIN_G2, POLISH		},
	{ LATIN_G0, LATIN_G2, GERMAN		},
	{ LATIN_G0, LATIN_G2, SWE_FIN_HUN	},
	{ LATIN_G0, LATIN_G2, ITALIAN		},
	{ LATIN_G0, LATIN_G2, FRENCH		},
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ LATIN_G0, LATIN_G2, CZECH_SLOVAK	},
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},

	/* 16 - Western Europe and Turkey */
	{ LATIN_G0, LATIN_G2, ENGLISH		},
	{ LATIN_G0, LATIN_G2, GERMAN		},
	{ LATIN_G0, LATIN_G2, SWE_FIN_HUN	},
	{ LATIN_G0, LATIN_G2, ITALIAN		},
	{ LATIN_G0, LATIN_G2, FRENCH		},
	{ LATIN_G0, LATIN_G2, PORTUG_SPANISH	},
	{ LATIN_G0, LATIN_G2, TURKISH		},
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},

	/* 24 - Central and Southeast Europe */
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ LATIN_G0, LATIN_G2, SERB_CRO_SLO	},
	{ LATIN_G0, LATIN_G2, NO_SUBSET		},
	{ LATIN_G0, LATIN_G2, RUMANIAN		},

	/* 32 - Cyrillic */
	{ CYRILLIC_1_G0, CYRILLIC_G2, NO_SUBSET	},
	{ LATIN_G0, LATIN_G2, GERMAN		},
	{ LATIN_G0, LATIN_G2, ESTONIAN		},
	{ LATIN_G0, LATIN_G2, LETT_LITH		},
	{ CYRILLIC_2_G0, CYRILLIC_G2, NO_SUBSET	},
	{ CYRILLIC_3_G0, CYRILLIC_G2, NO_SUBSET	},
	{ LATIN_G0, LATIN_G2, CZECH_SLOVAK	},
	{ 0, 0, NO_SUBSET			},

	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},

	/* 48 - Greece and Cyprus */
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ LATIN_G0, LATIN_G2, TURKISH		},
	{ GREEK_G0, GREEK_G2, NO_SUBSET		},

	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},

	/* 64 - Arabic */
	{ LATIN_G0, ARABIC_G2, ENGLISH		},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ LATIN_G0, ARABIC_G2, FRENCH		},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ ARABIC_G0, ARABIC_G2, NO_SUBSET	},

	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},

	/* 80 - Israel */
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ 0, 0, NO_SUBSET			},
	{ HEBREW_G0, ARABIC_G2, NO_SUBSET	},
	{ 0, 0, NO_SUBSET			},
	{ ARABIC_G0, ARABIC_G2, NO_SUBSET	},
};

static const int
national_subst[14][13] = {
	{ 0x0023, 0x24, 0x40, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x7B, 0x7C, 0x7D, 0x7E },
	{ 0x0023, 0xA000 + 'u', 0xF000 + 'c', 0x16, 0xF000 + 'z', 0x2000 + 'y', 0x24F5, 0xF000 + 'r', 0x2000 + 'e', 0x2400 + 'a', 0xF000 + 'e', 0x2000 + 'u', 0xF000 + 's' },
	{ 0x00A3, 0xA4, 0x40, 0xAC, 0xBD, 0xAE, 0xAD, '#', 0xD0, 0xBC, 0x01FC, 0xBE, 0xB8 },
	{ 0x0023, 0x4000 + 'o', 0xF013, 0x8801, 0x880F, 0xF01A, 0x8815, 0x400F, 0xF000 + 's', 0x04, 0x8000 + 'o', 0xF000 + 'z', 0x8000 + 'u' },
	{ 0x2000 + 'e', 0x0D, 0x1000 + 'a', 0x8000 + 'e', 0x3000 + 'e', 0x1000 + 'u', 0x34F5, '#', 0x1000 + 'e', 0x3400 + 'a', 0x3000 + 'o', 0x3000 + 'u', 0xB000 + 'c' },
	{ 0x0023, 0xA4, 0xA7, 0x8801, 0x880F, 0x8815, 0x5E, 0x5F, 0xB0, 0x04, 0x8000 + 'o', 0x8000 + 'u', 0xFB },
	{ 0x00A3, 0xA4, 0x2000 + 'e', 0xB0, 0xB000 + 'c', 0xAE, 0xAD, '#', 0x1000 + 'u', 0x1000 + 'a', 0x1000 + 'o', 0x1000 + 'e', 0x10F5 },
	{ 0x0023, 0xA4, 0xF013, 0x7000 + 'e', 0xB000 + 'e', 0xF01A, 0xF000 + 'c',	0x5000 + 'u', 0xF000 + 's', 0xE400 + 'a', 0xE000 + 'u', 0xF000 + 'z', 0xB400 + 'i' },
	{ 0x0023, 0x2000 + 'n', 0xE400 + 'a', 0x1E, 0x2013, 0xE8, 0x2000 + 'c', 0x2000 + 'o', 0xE000 + 'e', 0x7000 + 'z', 0x2000 + 's', 0xF8, 0x2000 + 'z' },
	{ 0xB000 + 'c', 0xA4, 0xA1, 0x2400 + 'a', 0x2000 + 'e', 0x24F5, 0x2000 + 'o', 0x2000 + 'u', 0xBF, 0x8000 + 'u', 0x4000 + 'n', 0x1000 + 'e', 0x1000 + 'a' },
	{ 0x0023, 0x24, 0xB000 + 'T', 0x3002, 0xB000 + 'S', 0xF001, 0x3006, 0xF5, 0xB000 + 't', 0x3400 + 'a', 0xB000 + 's', 0xF400 + 'a', 0x34F5 },
	{ 0x0023, 0x8805, 0xF003, 0x2003, 0xF01A, 0xE2, 0xF013, 0x8000 + 'e', 0xF000 + 'c', 0x2000 + 'c', 0xF000 + 'z', 0xF2, 0xF000 + 's' },
	{ 0x0023, 0x24, 0x2005, 0x8801, 0x880F, 0x17, 0x8815, 0x5F, 0x2000 + 'e', 0x04, 0x8000 + 'o', 0xA400 + 'a', 0x8000 + 'u' },
	{ 0x001F, 0x6000 + 'g', 0x7409, 0xB000 + 'S', 0x880F, 0xB000 + 'C', 0x8815, 0x6007, 0xF5, 0xB000 + 's', 0x8000 + 'o', 0xB000 + 'c', 0x8000 + 'u' }
};

static const int
cyrillic_1_g0_alpha_subst[64] = {
	0x011E, 0x0101, 0x0102, 0x0103,	0x0104, 0x0105, 0x0106, 0x0107,
	0x0108, 0x0109,    'J', 0x010B,	0x010C, 0x010D, 0x010E, 0x010F,
	0x0080, 0x0081, 0x0112, 0x0113, 0x0114, 0x0115, 0x0117, 0x0082,
	0x0083, 0x0084, 0x011A, 0x0085, 0x0116, 0x0086, 0x011B, 0x0087,
	0x013E, 0x0121, 0x0122, 0x0123, 0x0124, 0x0125, 0x0126, 0x0127,
	0x0128, 0x0129,    'j', 0x012B,	0x012C, 0x012D, 0x012E, 0x012F,
	0x0090, 0x0091, 0x0132, 0x0133, 0x0134, 0x0135, 0x0137, 0x0092,
	0x0093, 0x0094, 0x013A, 0x0095, 0x0136, 0x0096, 0x013B, 0x013F
};

static const unsigned char
greek_g2_subst[96] = {
	0xA0,  'a',  'b', 0xA3,  'e',  'h',  'i', 0xA7,
	 ':', 0xA9, 0xAA,  'k', 0xAC, 0xAD, 0xAE, 0xAF,
	0xB0, 0xB1, 0xB2, 0xB3,  'x',  'm',  'n',  'p',
	0xB8, 0xB9, 0xBA,  't', 0xBC, 0xBD, 0xBE,  'x',
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
	0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
	 '?', 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
	0xD8, 0x89, 0x8A, 0x8B, 0xDC, 0xDD, 0xDE, 0xDF,
	 'C',  'D',  'F',  'G',  'J',  'L',  'Q',  'R',
	 'S',  'U',  'V',  'W',  'Y',  'Z', 0x8C, 0x8D,
	 'c',  'd',  'f',  'g',  'j',  'l',  'q',  'r',
	 's',  'u',  'v',  'w',  'y',  'z', 0x8E, 0xFF,
};

#define GL_LATIN_G0				(0x0000)	/* 0x00 ... 0x1F reserved */
#define GL_LATIN_G2				(0x0080)	/* 0x80 ... 0x9F reserved */
#define GL_CYRILLIC_2_G0_ALPHA			(0x0100)
#define GL_GREEK_G0_ALPHA			(0x0140)
#define GL_ARABIC_G0_ALPHA			(0x0180)
#define GL_ARABIC_G2				(0x01C0 - 0x20)	/* 0x20 ... 0x3F only */
#define GL_HEBREW_G0_LOWER			(0x01E0)
#define GL_CONTIGUOUS_BLOCK_MOSAIC_G1		(0x0200 - 0x20)
#define GL_SEPARATED_BLOCK_MOSAIC_G1		(0x0220 - 0x20)	/* interleaved 2-2-6-6 */
#define GL_SMOOTH_MOSAIC_G3			(0x0280 - 0x20)

#define GL_SPACE				(GL_LATIN_G0 + ' ')

/*
    XXX should be optimized
 */
static int
glyph(character_set s, national_subset n, int c)
{
	int i;

	switch (s) {
	case LATIN_G0:
		for (i = 0; i < 13; i++)
			if (c == national_subst[0][i])
				return national_subst[n][i];
		return GL_LATIN_G0 + c;

	case LATIN_G2:
		return GL_LATIN_G2 + c;

	case CYRILLIC_1_G0:
		if (c == 0x24)
			return 0x00A4;
		if (c <= 0x3F)
			return GL_LATIN_G0 + c;
		return cyrillic_1_g0_alpha_subst[c - 0x40];

	case CYRILLIC_2_G0:
		if (c == 0x24)
			return 0x00A4;
		if (c == 0x26)
			return 0x0097;
		if (c <= 0x3F)
			return GL_LATIN_G0 + c;
		return GL_CYRILLIC_2_G0_ALPHA - 0x40 + c;

	case CYRILLIC_3_G0:
		if (c == 0x24)
			return 0x00A4;
		if (c == 0x26)
			return 0x000D;
		if (c <= 0x3F)
			return GL_LATIN_G0 + c;
		if (c == 0x59)
			return GL_LATIN_G0 + 'I';
		if (c == 0x79)
			return GL_LATIN_G0 + 'i';
		if (c == 0x5C)
			return 0x0088;
		if (c == 0x5F)
			return 0x0010;
		if (c == 0x67)
			return 0x0099;
		if (c == 0x7C)
			return 0x0098;
		return GL_CYRILLIC_2_G0_ALPHA - 0x40 + c;

	case CYRILLIC_G2:
		if (c == 0x59)
			return 0x00E8;
		if (c == 0x5A)
			return 0x00F8;
		if (c == 0x5B)
			return 0x00FB;
		if (c <= 0x5F)
			return GL_LATIN_G2 + c;
		return GL_LATIN_G0 + "DEFGIJKLNQRSUVWZdefgijklnqrsuvwz"[c - 0x60];

	case GREEK_G0:
		if (c == 0x24)
			return 0x00A4;
		if (c == 0x3C)
			return 0x00AB;
		if (c == 0x3E)
			return 0x00BB;
		if (c <= 0x3F)
			return GL_LATIN_G0 + c;
		return GL_GREEK_G0_ALPHA - 0x40 + c;

	case GREEK_G2:
		return greek_g2_subst[c - 0x20];

	case ARABIC_G0:
		if (c == 0x23)
			return 0x00A3;
		if (c == 0x24)
			return 0x00A4;
		if (c == 0x26)
			return 0x009D;
		if (c == 0x27)
			return 0x009E;
		if (c == 0x28)
			return GL_LATIN_G0 + ')';
		if (c == 0x29)
			return GL_LATIN_G0 + '(';
		if (c == 0x2C)
			return 0x009C;
		if (c == 0x3B)
			return 0x009B;
		if (c == 0x3C)
			return GL_LATIN_G0 + '>';
		if (c == 0x3E)
			return GL_LATIN_G0 + '<';
		if (c == 0x3F)
			return 0x009F;
		return GL_ARABIC_G0_ALPHA - 0x40 + c;

	case ARABIC_G2:
		if (c <= 0x3F)
			return GL_ARABIC_G2 + c;
		if (c == 0x40)
			return GL_LATIN_G0 + 0x1000 + 'a';
		if (c == 0x60)
			return GL_LATIN_G0 + 0x2000 + 'e';
		if (c == 0x4B)
			return GL_LATIN_G0 + 0x8000 + 'e';
		if (c == 0x4C)
			return GL_LATIN_G0 + 0x3000 + 'e';
		if (c == 0x4D)
			return GL_LATIN_G0 + 0x1000 + 'u';
		if (c == 0x4E)
			return 0xF4F5;
		if (c == 0x4F)
			return 0x008F;
		if (c == 0x4B)
			return GL_LATIN_G0 + 0x3400 + 'a';
		if (c == 0x4C)
			return GL_LATIN_G0 + 0x3000 + 'o';
		if (c == 0x4D)
			return GL_LATIN_G0 + 0x3000 + 'u';
		if (c == 0x4E)
			return GL_LATIN_G0 + 0xB000 + 'c';
		return GL_LATIN_G0 + c;

	case HEBREW_G0:
		if (c == 0x23)
			return 0x00A3;
		if (c == 0x24)
			return 0x00A4;
		if (c == 0x4B)
			return 0x00AC;
		if (c == 0x4C)
			return 0x00BD;
		if (c == 0x4D)
			return 0x00AE;
		if (c == 0x4E)
			return 0x00AD;
		if (c == 0x4F)
			return GL_LATIN_G0 + '#';
		return GL_HEBREW_G0_LOWER - 0x60 + c;

	default:
		return GL_LATIN_G0 + '?';
	}
}




#undef printv
#define printv printf
// #define printv(templ, ...)

static inline int
compose(int glyph, int mark)
{
	int mm = 1 << mark;

	/* requested by the aestethics department */
	if (0xA5FE & mm) {
		if (glyph >= 0x40 && glyph <= 0x60
		    && (0x063CDFAA & (1 << glyph))) {
			if (glyph == 'I')
				glyph = (0x0048 & mm) ? 0x0406 : 0x0409;
			else if (glyph == 'A' && (0x0048 & mm))
				glyph =  0x0002;
			else
				glyph -= 0x0040; /* reduced uppercase */
			if (0x01A0 & mm)
				glyph += 0x0800;
		} else if (glyph == 'a')
			glyph = 0x0461;
		else if (glyph == 'i')
			glyph = (mark == 1) ? 0x00F5 : 0x04F5;
		else if (glyph == 'j')
			glyph = 0x0011;
	}

	glyph |= mark << 12;

	if (glyph == 0xA001)
		glyph = 0x0017;
	else if (glyph == 0x8009)
		glyph = 0x0010;
	else if (glyph == 0x8061)
		glyph = 0x0004;
	else if (glyph == 0xF074)
		glyph = 0x0016;
	else if (glyph == 0x80F5)
		glyph = 0x000D;

	return glyph;
}



static int
vbi_resolve_flof(int x, struct vt_page *vtp, int *page, int *subpage)
{
	int code= 7, i, c;
	
	if ((!vtp) || (!page) || (!subpage))
		return FALSE;
	
	if (!(vtp->_data.lop.flof))
		return FALSE;
	
	for (i=0; (i <= x) && (i<40); i++)
		if ((c = vtp->data[24][i]) < 8) /* color code */
			code = c; /* Store it for later on */
	
	if (code >= 8) /* not found ... weird */
		return FALSE;
	
	code = " \0\1\2\3 \3 "[code]; /* color->link conversion table */
	
	if ((code > 6) || ((vtp->_data.lop.link[code].pgno & 0xff) == 0xff))
		return FALSE;
	
	*page = vtp->_data.lop.link[code].pgno;
	*subpage = vtp->_data.lop.link[code].subno; /* 0x3f7f handled? */
	
	return TRUE;
}

#define notdigit(x) (!isdigit(x))

static int
vbi_check_subpage(const char *p, int x, int *n1, int *n2)
{
    p += x;

    if (x >= 0 && x < 42-5)
	if (notdigit(p[1]) || notdigit(p[0]))
	    if (isdigit(p[2]))
		if (p[3] == '/' || p[3] == ':')
		    if (isdigit(p[4]))
			if (notdigit(p[5]) || notdigit(p[6]))
			{
			    *n1 = p[2] % 16;
			    if (isdigit(p[1]))
				*n1 += p[1] % 16 * 16;
			    *n2 = p[4] % 16;
			    if (isdigit(p[5]))
				*n2 = *n2 * 16 + p[5] % 16;
			    if ((*n2 > 0x99) || (*n1 > 0x99) ||
				(*n1 > *n2))
			      return FALSE;
			    return TRUE;
			}
    return FALSE;
}

static int
vbi_check_page(const char *p, int x, int *pgno, int *subno)
{
    p += x;

    if (x >= 0 && x < 42-4)
      if (notdigit(p[0]) && notdigit(p[4]))
	if (isdigit(p[1]))
	  if (isdigit(p[2]))
	    if (isdigit(p[3]))
	      {
		*pgno = p[1] % 16 * 256 + p[2] % 16 * 16 + p[3] % 16;
		*subno = ANY_SUB;
		if (*pgno >= 0x100 && *pgno <= 0x899)
		  return TRUE;
	      }
    return FALSE;
}

/*
  Text navigation.
  Given the page, the x and y, tries to find a page number in that
  position. If succeeds, returns TRUE
*/
static int
vbi_resolve_page(int x, int y, struct vt_page *vtp, int *page,
		 int *subpage, struct fmt_page *pg)
{
	int i, n1, n2;
	char buffer[42]; /* The line and two spaces on the sides */

	if ((y > 24) || (y <= 0) || (x < 0) || (x > 39) || (!vtp)
	    || (!page) || (!subpage) || (!pg))
		return FALSE;

// {mhs}
// XXX new .ch:  123  123  112233  112233
//                    123          112233

	if (y == 24)
		return vbi_resolve_flof(x, vtp, page, subpage);

	buffer[0] = buffer[41] = ' ';

	for (i=1; i<41; i++)
		buffer[i] = pg->data[y][i-1].glyph & 0x3FF; // careful, not pure ASCII

	for (i = -2; i < 1; i++)
		if (vbi_check_page(buffer, x+i, page, subpage))
			return TRUE;

	/* try to resolve subpage */
	for (i = -4; i < 1; i++)
		if (vbi_check_subpage(buffer, x+i, &n1, &n2))
		{
			if (vtp->subno != n1)
				return FALSE; /* mismatch */
			n1 = dec2hex(hex2dec(n1)+1);
			if (n1 > n2)
				n1 = 1;
			*page = vtp->pgno;
			*subpage = n1;
			return TRUE;
		}
	
	return FALSE;
}










static vt_triplet *
resolve_obj_address(struct vbi *vbi, object_type type,
	int pgno, object_address address, page_function function)
{
	int s1, packet, pointer;
	struct vt_page *vtp;
	vt_triplet *trip;
	int i;
int k, l;

	s1 = address & 15;
	packet = ((address >> 7) & 3) + 1;
	i = ((address >> 5) & 3) * 3 + type;

	printv("obj invocation, source page %03x/%04x, "
		"pointer packet %d triplet %d\n", pgno, s1, packet, i);

	vtp = vbi->cache->op->get(vbi->cache, pgno, s1, 0x000F);

	if (!vtp) {
		printv("... page not cached\n");
		return 0;
	}

	if (vtp->function == PAGE_FUNCTION_UNKNOWN) {
/*
for (k = 0; k < 24; k++) {
for (l = 0; l < 40; l++)
    printv("%c", printable(vtp->_data.unknown.raw[k][l]));
printv("\n");
}
*/
		if (!convert_pop(vtp, function)) {
			printv("... no pop page or hamming error\n");
			return 0;
		}
	} else if (vtp->function != function) {
		printv("... source page wrong function %d, expected %d\n",
			vtp->function, function);
		return 0;
	}

//printf("pp: %d\n", (packet - 1) * 24 + i * 2 + ((address >> 4) & 1));

//for (k = 0; k < 48; k++)
//    printf("pop.pointer[%d] = %d\n", k, vtp->_data.pop.pointer[k]);

	pointer = vtp->_data.pop.pointer[
		(packet - 1) * 24 + i * 2 + ((address >> 4) & 1)];
//pointer = vtp->_data.pop.pointer[hum & 7];

printv("... triplet pointer %d\n", pointer);

	if (pointer > 506) {
		printv("... triplet pointer out of bounds (%d)\n", pointer);
		return 0;
	}

	packet = (pointer / 13) + 3;

	if (packet <= 25) {
		printv("... object start in packet %d, triplet %d (pointer %d)\n",
			packet, pointer % 13, pointer);
//		return 0;
	} else {
		printv("... object start in packet 26/%d, triplet %d (pointer %d)\n",
			packet - 26, pointer % 13, pointer);
//		return 0;
	}		

	trip = vtp->_data.pop.triplet + pointer;

	printv("obj 1st: ad 0x%02x mo 0x%04x dat %d=0x%x\n",
		trip->address, trip->mode, trip->data, trip->data);

	address ^= trip->address << 7;
	address ^= trip->data;

	if (trip->mode != (type + 0x14) || (address & 0x1FF)) {
int k, l;
		printv("... no object definition\n");

for (k = 0; k < 39; k++) {
    printv("%2d: ", k);
for (l = 0; l < 13; l++)
    printv("%02x%02x ",
	    vtp->_data.pop.triplet[k * 13 + l].address,
	    vtp->_data.pop.triplet[k * 13 + l].mode);
printv("\n");
}

		return 0;
	}

	return trip + 1;
}

static void
new_enhance(struct fmt_page *pg, struct vt_page *vtp,
	object_type type, vt_triplet *p, int inv_row, int inv_column)
{
	attr_char buf[40];
	int active_column, active_row;
	int offset_column, offset_row;
	int min_column, max_column, i;
	int foreground, background, gl;
	struct font_d *font, *g0_g2_font;
	int drcs_s1[2];
	u8 *rawp;
int tcount = 0;
	active_column = 0;
	active_row = 0;

	offset_column = 0;
	offset_row = 0;

	min_column = (inv_row == 0) ? 8 : 0;
	max_column = (type == OBJ_TYPE_ADAPTIVE) ? -1 : 39;

	rawp = vtp->_data.lop.raw[inv_row];
	memcpy(buf, pg->data[inv_row], 40 * sizeof(attr_char));

	foreground = WHITE;
	background = BLACK;

	drcs_s1[0] = 0; /* global */
	drcs_s1[1] = 0; /* normal */

	{
		vt_extension *ext;
		int char_set;

		g0_g2_font = font_d_table;

		if (!(ext = vtp->_data.unknown.extension))
			ext = &vtp->vbi->magazine[(vtp->pgno >> 8) - 1].extension;

		char_set = ext->char_set[0];

		if (VALID_CHARACTER_SET(char_set))
			g0_g2_font = font_d_table + char_set;

		char_set = (char_set & ~7) + vtp->national;

		if (VALID_CHARACTER_SET(char_set))
			g0_g2_font = font_d_table + char_set;

		printv("enh char_set = %d\n", char_set);
	}

	font = g0_g2_font;

	for (;; tcount++, p++) {
		if (p->mode == 0xFF) {
			printv("enh %d no triplet, not received (yet)?\n", tcount);
			goto finish;
		}

		printv("triplet %d %02x%02x%02x\n", tcount,
			p->address, p->mode, p->data);

		if (p->address >= 40) {
			/*
			 *  Row address triplets
			 */
			int s = p->data >> 5;
			int row = (p->address - 40) ? : 24;

			switch (p->mode) {
			case 0x00:		/* full screen colour */
				if (s == 0) {
					/* TODO */
				}

				break;

			case 0x07:		/* address display row 0 */
				if (p->address != 0x3F)
					break; /* reserved, no position */

				row = 0;

				/* fall through */

			case 0x01:		/* full row colour */
				active_column = 0;

#if 0 /* TODO */
				if (s == 0) {
					int colour = p->data & 0x1F;
					// addressed row
				} else if (s == 3) {
					int colour = p->data & 0x1F;
					// here and below
				} /* other reserved */
#endif
				goto set_active;

			/* case 0x02: reserved */
			/* case 0x03: reserved */

			case 0x04:		/* set active position */
				if (p->data >= 40)
					break; /* reserved */

				active_column = p->data;

			set_active:
				printv("enh %d set_active row %d col %d\n", tcount, row, active_column);

				if (row == active_row)
					break;

				active_row += inv_row;

				if (active_row <= 24) {
					if (max_column >= min_column)
						memcpy(&pg->data[active_row][min_column],
						       &buf[min_column],
						       (max_column - min_column + 1) * sizeof(attr_char));

					if (type == OBJ_TYPE_NONE || type == OBJ_TYPE_ACTIVE)
						font = g0_g2_font;
				}

				active_row = row;
				row += inv_row;

				min_column = (row == 0) ? 8 : 0;

				if (row <= 24) {
					rawp = vtp->_data.unknown.raw[row];
					memcpy(buf, pg->data[row], 40 * sizeof(attr_char));

					if (type == OBJ_TYPE_ADAPTIVE)
						max_column = -1;
				}

				break;

			/* case 0x05: reserved */
			/* case 0x06: reserved */
			/* case 0x08 ... 0x0F: PDC data */

			case 0x10:		/* origin modifier */
				if (p->data >= 72)
					break; /* invalid */

				offset_column = p->data;
				offset_row = p->address - 40;

				printv("enh origin modifier col %+d row %+d\n", offset_column, offset_row);

				break;

			case 0x11 ... 0x13:	/* object invocation */
			{
				int source = (p->address >> 3) & 3;
				object_type new_type = p->mode & 3;
				vt_triplet *trip; 

				/* TODO */

				printv("enh obj invocation source %d type %d\n", source, new_type);

				if (new_type <= type) { /* 13.2++ */
					printv("... priority violation\n");
					break;
				}

				if (source == 0)
					break; /* illegal */

				if (source == 1) { /* local */
					int designation = (p->data >> 4) + ((p->address & 1) << 4);
					int triplet = p->data & 15;
					
					if (triplet > 12)
						break; /* invalid */
					printv("... local obj %d/%d\n", designation, triplet);					
				} else {
					magazine *mag = vtp->vbi->magazine + (vtp->pgno >> 8) - 1;
					int pgno, row, column;

					/* XXX X/27 */

					if (source == 3)
						i = 0; /* GPOP */
					else if (!(i = mag->pop_lut[vtp->pgno & 0xFF])) {
						printv("... MOT pop_lut empty\n");
						break; /* has no link (yet?) */
					}

					pgno = mag->pop_link[i].pgno;

					if (NO_PAGE(pgno)) {
						int j;

						printv("... dead link %d/8: ", i);
						for (j = 0; j < 8; j++)
							printv("%04x ", mag->pop_link[j].pgno);
						printv("\n");
					pgno = 0x1F0;
					//	break; /* has no link (yet?) */
					}

					printv("... %s obj\n", (source == 3) ? "global" : "public");

					trip = resolve_obj_address(vtp->vbi, new_type, pgno,
						(p->address << 7) + p->data,
						(source == 3) ? PAGE_FUNCTION_GPOP : PAGE_FUNCTION_POP);

					if (!trip)
						break;

					row = inv_row + active_row;
					column = inv_column + active_column;

					if (row <= 24 && max_column >= min_column)
						memcpy(&pg->data[row][min_column], &buf[min_column],
						       (max_column - min_column + 1) * sizeof(attr_char));

					new_enhance(pg, vtp, new_type, trip,
						row + offset_row, column + offset_column);

					if (row <= 24)
						memcpy(buf, pg->data[row], 40 * sizeof(attr_char));

					printv("object done\n");
				}

				offset_row = 0;
				offset_column = 0;

				break;
			}

			/* case 0x14: reserved */

			case 0x15 ... 0x17:	/* object definition */
				printv("enh obj definition 0x%02x 0x%02x\n", p->mode, p->data);
				printv("enh terminated at #%d\n", tcount);
				goto finish;

			case 0x18:		/* drcs mode */
				printv("enh DRCS mode 0x%02x\n", p->data);
				drcs_s1[p->data >> 6] = p->data & 15;
				break;

			/* case 0x19 ... 0x1E: reserved */
			
			case 0x1F:		/* termination marker */
				printv("enh terminated at #%d\n", tcount);
				goto finish;
			}
		} else {
			/*
			 *  Column address triplets
			 */
			int s = p->data >> 5;			

			switch (p->mode) {
			case 0x00:		/* foreground colour */
				active_column = p->address;

				if (s == 0) {
					foreground = p->data & 0x1F;

					if (type != OBJ_TYPE_PASSIVE) {
						i = inv_column + active_column;

						if (i > max_column)
							max_column = i;

						for (; i < 40; i++) {
							int raw = rawp[i] & 0x78; /* XXX parity */
					    // XXX doubleh
							buf[i].foreground = foreground;

							/* spacing alpha foreground, set-after */
							/* spacing mosaic foreground, set-after */
							if (type != OBJ_TYPE_ADAPTIVE /* 13.4 */
							    && (raw == 0x00 || raw == 0x10))
								break;
						}
					}

					printv("enh %d col %d foreground %d\n", tcount, active_column, foreground);
				}

				break;

			case 0x01:		/* G1 block mosaic character */
				active_column = p->address;

				if (p->data & 0x20) {
					gl = GL_CONTIGUOUS_BLOCK_MOSAIC_G1 + p->data;
					goto store;
				} else if (p->data >= 0x40) {
					gl = glyph(font->G0, NO_SUBSET, p->data);
					goto store;
				}

				break;

			case 0x02:		/* G3 smooth mosaic or line drawing character */
			case 0x0B:
				active_column = p->address;

				if (p->data >= 0x20) {
					gl = GL_SMOOTH_MOSAIC_G3 + p->data;
					goto store;
				}

				break;

			case 0x03:		/* background colour */
				active_column = p->address;

				if (s == 0) {
					background = p->data & 0x1F;

					if (type != OBJ_TYPE_PASSIVE) {
						i = inv_column + active_column;

						if (i > max_column)
							max_column = i;

						if (i < 40) /* override spacing attribute at active position */
							buf[i++].background = background;

						for (; i < 40; i++) {
							int raw = rawp[i] & 0x7F; /* XXX parity */

							/* spacing black background, set-at */
							/* spacing new background, set-at */
							if (type != OBJ_TYPE_ADAPTIVE /* 13.4 */
							    && (raw == 0x1C || raw == 0x1D))
								break;

							buf[i].background = background;
						}
					}

					printv("enh %d col %d background %d\n", tcount, active_column, background);
				}

				break;

			/* case 0x04: reserved */
			/* case 0x05: reserved */
			/* case 0x06: pdc data */

			case 0x07:		/* additional flash functions */	
				active_column = p->address;

				if (s == 0) {
					/* huh? */

					printv("enh col %d flash 0x%02x\n", active_column, p->data);
				}

				break;

			case 0x08:		/* modified G0 and G2 character set designation */
				active_column = p->address;

				printv("enh col %d modify character set %d\n", active_column, p->data);

				if (VALID_CHARACTER_SET(p->data))
					font = font_d_table + p->data;
				else
					font = g0_g2_font;

				break;

			case 0x09:		/* G0 character */			
				active_column = p->address;

				if (p->data >= 0x20) {
					gl = glyph(font->G0, NO_SUBSET, p->data);
					goto store;
				}

				break;

			/* case 0x0A: reserved */

			case 0x0C:		/* display attributes */
				active_column = p->address;

				printv("enh col %d display attr 0x%02x\n", active_column, p->data);

				break;

			/*
				d6	double width
				d5	underline / separate
				d4	invert colour
				d3	reserved
				d2	conceal
				d1	boxing / window
				d0	double height
				set-at, 1=yes
				The action persists to the end of a display row but may be cancelled by
				the transmission of a further triplet of this type with the relevant bit set to '0', or, in
				most cases, by an appropriate spacing attribute on the Level 1 page.
				ADAPTIVE: no spacing attr.
				PASSIVE: no end of row.
			*/

			case 0x0D:		/* drcs character invocation */
{
	magazine *mag = vtp->vbi->magazine + (vtp->pgno >> 8) - 1;
	int normal = p->data >> 6;
	int offset = p->data & 0x3F;
	struct vt_page *drcs_vtp;
	page_function function;
	int pgno;
extern void drcs_conv(struct vt_page *vtp);

	active_column = p->address;

	if (offset >= 48)
		break; /* invalid */

	printv("enh col %d DRCS 0x%02x\n", active_column, p->data);

	/* XXX X/27 */

	function = PAGE_FUNCTION_DRCS;

	if (!normal) {
		function = PAGE_FUNCTION_GDRCS;
		i = 0;
	} else if (!(i = mag->drcs_lut[vtp->pgno & 0xFF])) {
		printv("... MOT drcs_lut empty\n");
		break; /* has no link (yet?) */
	}

	pgno = mag->drcs_link[i];

	if (NO_PAGE(pgno)) {
		printv("... dead link\n");
		break; /* has no link (yet?) */
	}

	printv("... %s drcs from page %x/%04x\n",
		normal ? "normal" : "global", pgno, drcs_s1[normal]);

	drcs_vtp = vtp->vbi->cache->op->get(vtp->vbi->cache, pgno, drcs_s1[normal], 0x000F);

	if (!drcs_vtp) {
		printv("... page not cached\n");
		break;
	}

	drcs_conv(drcs_vtp);

	if (drcs_vtp->function == PAGE_FUNCTION_UNKNOWN) {
/*
int k, l;

for (k = 0; k < 24; k++) {
for (l = 0; l < 40; l++)
    printv("%c", printable(drcs_vtp->_data.unknown.raw[k][l]));
printv("\n");
}
*/
//		if (!convert_pop(drcs_vtp, function)) {
			printv("... no pop page or hamming error\n");
//			break;
//		}
	} else if (drcs_vtp->function != function) {
		printv("... source page wrong function %d, expected %d\n",
			drcs_vtp->function, function);
		break;
	}

	pg->drcs = drcs_vtp->drcs_bits;
{
int k, l;

for (k = 0; k < 10; k++) {
for (l = 0; l < 6; l++)
    printv("%02x", drcs_vtp->drcs_bits[offset+2][k * 6 + l]);
printv("\n");
}
}


	gl = 0x3C0 + offset+2;
	goto store;
}

				break;

			case 0x0E:		/* font style */
				active_column = p->address;

				/* TODO */

				printv("enh col %d font style 0x%02x\n", active_column, p->data);

				break;

			case 0x0F:		/* G2 character */
				active_column = p->address;

				if (p->data >= 0x20) {
					gl = glyph(font->G2, NO_SUBSET, p->data);
					goto store;
				}

				break;

			case 0x10 ... 0x1F:	/* characters including diacritical marks */
				active_column = p->address;

				if (p->data >= 0x20) {
					gl = compose(glyph(font->G0, NO_SUBSET, p->data), p->mode - 16);

			store:
					printv("enh row %d col %d print 0x%02x/0x%02x -> 0x%04x %c\n",
						active_row, active_column, p->mode, p->data,
						gl, printable(gl & 0x7F));

					if ((i = inv_column + active_column) >= 40)
						break;

					buf[i].glyph = gl;

					if (type == OBJ_TYPE_PASSIVE) {
						buf[i].foreground = foreground;
						buf[i].background = background;
					} else
						if (active_column > max_column)
							max_column = active_column;
				}

				break;
			}
		}
	}

finish:
	active_row += inv_row;

	if (active_row <= 24)
		if (max_column >= min_column)
			memcpy(&pg->data[active_row][min_column], &buf[min_column],
			       (max_column - min_column + 1) * sizeof(attr_char));
}

void
fmt_page(int reveal, struct fmt_page *pg, struct vt_page *vtp)
{
	char buf[16];
	vt_extension *ext;
	struct font_d *g0_font[2];
	int column, row, i;
	int display_rows;

// XXX page function check

	display_rows = vtp->_data.lop.flof ? 25 : 24;

	sprintf(buf, "\2%x.%02x\7", vtp->pgno, vtp->subno & 0xff);

	g0_font[0] = font_d_table;
	g0_font[1] = font_d_table;

	if (!(ext = vtp->_data.lop.extension))
		ext = &vtp->vbi->magazine[(vtp->pgno >> 8) - 1].extension;

	for (i = 0; i < 2; i++) {
		int char_set = ext->char_set[i];

		if (VALID_CHARACTER_SET(char_set))
			g0_font[1] = font_d_table + char_set;

		char_set = (char_set & ~7) + vtp->national;

		if (VALID_CHARACTER_SET(char_set))
			g0_font[i] = font_d_table + char_set;

// printf("char_set[%d] = %d\n", char_set);
	}


    for (i = 0; i < 32; i++) {
	pg->colour_map[i] =
	    ((ext->colour_map[i] & 0xF00) << 12) 
	    | ((ext->colour_map[i] & 0xF00) << 8) 
	    | ((ext->colour_map[i] & 0xF0) << 8) 
	    | ((ext->colour_map[i] & 0xF0) << 4) 
	    | ((ext->colour_map[i] & 0xF) << 4)
	    | ((ext->colour_map[i] & 0xF) << 0)
	    | 0xFF000000;
    }

	pg->drcs_clut = ext->drcs_clut;

	i = 0;
	reveal = !!reveal;

	for (row = 0; row < display_rows; row++) {
		struct font_d *font;
		int mosaic_glyphs;
		int held_mosaic_glyph;
		opacity page_opacity, boxed_opacity;
		bool hold, mosaic;
		bool double_height, wide_char;
		attr_char ac;

		held_mosaic_glyph = GL_CONTIGUOUS_BLOCK_MOSAIC_G1 + 0; /* blank */

		page_opacity = OPAQUE;
		boxed_opacity = SEMI_TRANSPARENT;

		if (row == 0) {
			if (vtp->flags & C7_SUPPRESS_HEADER)
				boxed_opacity = TRANSPARENT_SPACE;
			if (vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE | C7_SUPPRESS_HEADER))
				page_opacity = TRANSPARENT_SPACE;
		} else {
			if (vtp->flags & C10_INHIBIT_DISPLAY)
				boxed_opacity = TRANSPARENT_SPACE;
			if (vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE | C10_INHIBIT_DISPLAY))
				page_opacity = TRANSPARENT_SPACE;
		}

		memset(&ac, 0, sizeof(ac));

		ac.foreground	= ext->foreground_clut + WHITE;
		ac.background	= ext->background_clut + BLACK;
		mosaic_glyphs	= GL_CONTIGUOUS_BLOCK_MOSAIC_G1;
		ac.opacity	= page_opacity;
		font		= g0_font[0];
		reveal	       |= 2;
		hold		= FALSE;
		mosaic		= FALSE;

		double_height	= FALSE;
		wide_char	= FALSE;

		for (column = 0; column < W; ++column) {
			int raw;

			raw = vtp->_data.lop.raw[0][i++] & 0x7F; /* XXX parity */

			if (row == 0 && column < 8)
				raw = buf[column];
//printf("RAW %d, %d: %02x\n", row, column, raw);

			/* set-at spacing attributes */

			switch (raw) {
			case 0x09:		/* steady */
				ac.flash = FALSE;
				break;

			case 0x0C:		/* normal size */
				ac.size = NORMAL;
				break;

			case 0x18:		/* conceal */
				reveal &= 1;
				break;

			case 0x19:		/* contiguous mosaics */
				mosaic_glyphs = GL_CONTIGUOUS_BLOCK_MOSAIC_G1;
				break;

			case 0x1A:		/* separated mosaics */
				mosaic_glyphs = GL_SEPARATED_BLOCK_MOSAIC_G1;
				break;

			case 0x1C:		/* black background */
				ac.background = ext->background_clut + BLACK;
				break;

			case 0x1D:		/* new background */
				ac.background = ext->background_clut + (ac.foreground & 7);
				break;

			case 0x1E:		/* hold mosaic */
				hold = TRUE;
				break;
			}

			if (raw <= 0x1F)
				ac.glyph = (hold & mosaic) ? held_mosaic_glyph : GL_SPACE;
			else
				if (mosaic && (raw & 0x20)) {
					held_mosaic_glyph = mosaic_glyphs + raw;
					ac.glyph = reveal ? held_mosaic_glyph : GL_SPACE;
				} else
					ac.glyph = reveal ? glyph(font->G0, font->subset, raw) : GL_SPACE;

			if (!wide_char) {
				pg->data[row][column] = ac;

				wide_char = /*!!*/(ac.size & DOUBLE_WIDTH);

				if (wide_char && column < 39) {
					attr_char t = ac;

					t.size = OVER_TOP;
					pg->data[row][column + 1] = t;
				}
			} else
				wide_char = FALSE;

			/* set-after spacing attributes */

			switch (raw) {
			case 0x00 ... 0x07:	/* alpha + foreground colour */
				ac.foreground = ext->foreground_clut + (raw & 7);
				mosaic_glyphs = GL_CONTIGUOUS_BLOCK_MOSAIC_G1;
				reveal |= 2;
				mosaic = FALSE;
				break;

			case 0x08:		/* flash */
				ac.flash = TRUE;
				break;

			case 0x0A:		/* end box */
			// XXX parity
				if (column < 39 && vtp->_data.lop.raw[0][i] == 0x0a)
					ac.opacity = page_opacity;
				break;

			case 0x0B:		/* start box */
				if (column < 39 && vtp->_data.lop.raw[0][i] == 0x0b)
					ac.opacity = boxed_opacity;
				break;

			case 0x0D:		/* double height */
				if (row <= 0 || row >= 23)
					break;
				ac.size = DOUBLE_HEIGHT;
				double_height = TRUE;
				break;

			case 0x0E:		/* double width */
				printv("spacing col %d row %d double width\n", column, row);

				if (column < 39)
					ac.size = DOUBLE_WIDTH;

				break;

			case 0x0F:		/* double size */
				printv("spacing col %d row %d double size\n", column, row);

				if (column >= 39 || row <= 0 || row >= 23)
					break;

				ac.size = DOUBLE_SIZE;
				double_height = TRUE;

				break;

			case 0x10 ... 0x17:	/* mosaic + foreground colour */
				ac.foreground = ext->foreground_clut + (raw & 7);
				reveal |= 2;
				mosaic = TRUE;
				break;

			case 0x1F:		/* release mosaic */
				hold = FALSE;
				break;

			case 0x1B:		/* ESC */
				font = (font == g0_font[0]) ? g0_font[1] : g0_font[0];
				break;
			}
		}

		if (double_height) {
			for (column = 0; column < W; column++) {
				ac = pg->data[row][column];

				switch (ac.size) {
				case DOUBLE_HEIGHT:
					ac.size = DOUBLE_HEIGHT2;
					pg->data[row + 1][column] = ac;
					break;
		
				case DOUBLE_SIZE:
					ac.size = DOUBLE_SIZE2;
					pg->data[row + 1][column] = ac;
					ac.size = OVER_BOTTOM;
					pg->data[row + 1][++column] = ac;
					break;

				default: /* NORMAL, DOUBLE_WIDTH, OVER_TOP */
					ac.size = NORMAL;
					ac.glyph = GL_SPACE;
					pg->data[row + 1][column] = ac;
					break;
				}
			}

			i += 40;
			row++;
		}
	}

	if (!vtp->_data.lop.flof) {
		attr_char ac;

		memset(&ac, 0, sizeof(ac));

		if (vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE | C10_INHIBIT_DISPLAY))
			ac.opacity = TRANSPARENT_SPACE;
		else
			ac.opacity = OPAQUE;
		ac.foreground	= ext->foreground_clut + WHITE;
		ac.background	= ext->background_clut + BLACK;
		ac.glyph	= GL_SPACE;

		for (column = 0; column < W; column++)
			pg->data[24][column] = ac;
	}

	new_enhance(pg, vtp, OBJ_TYPE_NONE, vtp->_data.lop.triplet, 0, 0);

	if (0) /* Test */
		for (row = 1; row < 24; row++)
			for (column = 0; column < 40; column++) {
				int page = ((vtp->pgno >> 4) & 15) * 10 + (vtp->pgno & 15);

				if (page <= 15) {
					if (row <= 23 && column <= 31) {
						pg->data[row][column].foreground = WHITE;
						pg->data[row][column].background = BLACK;
						pg->data[row][column].size = NORMAL;
						pg->data[row][column].glyph =
							compose((row - 1) * 32 + column, (page & 15));
					}
				} else if (page == 16) {
					if (row <= 14 && column <= 12) {
						pg->data[row][column].foreground = WHITE;
						pg->data[row][column].background = BLACK;
						pg->data[row][column].size = NORMAL;
						pg->data[row][column].glyph =
							national_subst[row - 1][column];
					}
				}
			}

	for (row = 0; row < display_rows; row++)
		for (column = 0; column < W; column++) {
			int page, subpage;

			if (!vbi_resolve_page(column, row, vtp, &page,
					      &subpage, pg))
				page = subpage = 0;
			pg->data[row][column].link_page = page;
			pg->data[row][column].link_subpage = subpage;
		}
}

int
export(struct export *e, struct vt_page *vtp, char *name)
{
    struct fmt_page pg[1];

    fmt_page(e->reveal, pg, vtp);
    return e->mod->output(e, name, pg);
}
