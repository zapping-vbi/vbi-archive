#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "vt.h"
#include "misc.h"
#include "export.h"
#include "vbi.h"

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
	{ 0x23, 0x24, 0x40, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x7b, 0x7c, 0x7d, 0x7e },
	{ 0x23, 0xA000 + 'u', 0xF000 + 'c', 0x0B, 0xF000 + 'z', 0x2000 + 'y', 0x09, 0xF000 + 'r', 0x2000 + 'e', 0x07, 0xF000 + 'e', 0x2000 + 'u', 0xF000 + 's' },
	{ 0xA3, 0xA4, 0x40, 0xAC, 0xBD, 0xAE, 0xAD, '#', 0xD0, 0xBC, 0x01FC, 0xBE, 0xB8 },
	{ 0x23, 0x4000 + 'o', 0xF01D, 0x12, 0x15, 0xF01F, 0x16, 0x401C, 0xF000 + 's', 0x08, 0x8000 + 'o', 0xF000 + 'z', 0x8000 + 'u' },
	{ 0x2000 + 'e', 0x0C, 0x1000 + 'a', 0x8000 + 'e', 0x3000 + 'e', 0x1000 + 'u', 0x09, '#', 0x1000 + 'e', 0x3000 + 'a', 0x3000 + 'o', 0x3000 + 'u', 0xB000 + 'c' },
	{ 0x23, 0xA4, 0xA7, 0x12, 0x15, 0x16, 0x5E, 0x5F, 0xB0, 0x08, 0x8000 + 'o', 0x8000 + 'u', 0xFB },
	{ 0xA3, 0xA4, 0x2000 + 'e', 0xB0, 0xB000 + 'c', 0xAE, 0xAD, '#', 0x1000 + 'u', 0x1000 + 'a', 0x1000 + 'o', 0x1000 + 'e', 0x1000 + 'i' },
	{ 0x23, 0xA4, 0xF01D, 0x7000 + 'e', 0xB000 + 'e', 0xF01F, 0xF000 + 'c', 0x5000 + 'u', 0xF000 + 's', 0xE000 + 'a', 0xE000 + 'u', 0xF000 + 'z', 0xB000 + 'i' },
	{ 0x23, 0x2000 + 'n', 0xE000 + 'a', 0x0E, 0x201D, 0xE8, 0x2000 + 'c', 0x2000 + 'o', 0xE000 + 'e', 0x7000 + 'z', 0x2000 + 's', 0xF8, 0x2000 + 'z' },
	{ 0xB000 + 'c', 0xA4, 0xA1, 0x07, 0x2000 + 'e', 0x2000 + 'i', 0x2000 + 'o', 0x2000 + 'u', 0xBF, 0x8000 + 'u', 0x4000 + 'n', 0x1000 + 'e', 0x1000 + 'a' },
	{ 0x23, 0x24, 0xB000 + 'T', 0x10, 0xB000 + 'S', 0xF017, 0x11, 0xF5, 0xB000 + 't', 0x3000 + 'a', 0xB000 + 's', 0xF000 + 'a', 0x09 },
	{ 0x23, 0x13, 0xF018, 0x2018, 0xF01F, 0xE2, 0xF01D, 0x8000 + 'e', 0xF000 + 'c', 0x2000 + 'c', 0xF000 + 'z', 0xF2, 0xF000 + 's' },
	{ 0x23, 0x24, 0x2019, 0x12, 0x15, 0x0D, 0x16, 0x5F, 0x2000 + 'e', 0x08, 0x8000 + 'o', 0xA000 + 'a', 0x8000 + 'u' },
	{ 0x0F, 0xF000 + 'g', 0x06, 0xB000 + 'S', 0x15, 0xB000 + 'C', 0x16, 0xF01A, 0xF5, 0xB000 + 's', 0x8000 + 'o', 0xB000 + 'c', 0x8000 + 'u' }
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
			return c;
		return GL_CYRILLIC_2_G0_ALPHA - 0x40 + c;

	case CYRILLIC_3_G0:
		if (c == 0x24)
			return 0x00A4;
		if (c == 0x26)
			return 0x0C;
		if (c <= 0x3F)
			return c;
		if (c == 0x59)
			return 'I';
		if (c == 0x79)
			return 'i';
		if (c == 0x5C)
			return 0x0088;
		if (c == 0x5F)
			return 0x14;
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
			return c;
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
			return ')';
		if (c == 0x29)
			return '(';
		if (c == 0x2C)
			return 0x009C;
		if (c == 0x3B)
			return 0x009B;
		if (c == 0x3C)
			return '>';
		if (c == 0x3E)
			return '<';
		if (c == 0x3F)
			return 0x009F;
		return GL_ARABIC_G0_ALPHA - 0x40 + c;

	case ARABIC_G2:
		if (c <= 0x3F)
			return GL_ARABIC_G2 + c;
		if (c == 0x40)
			return 0x1000 + 'a';
		if (c == 0x60)
			return 0x2000 + 'e';
		if (c == 0x4B)
			return 0x8000 + 'e';
		if (c == 0x4C)
			return 0x3000 + 'e';
		if (c == 0x4D)
			return 0x1000 + 'u';
		if (c == 0x4E)
			return 0x0A;
		if (c == 0x4F)
			return 0x008F;
		if (c == 0x4B)
			return 0x3000 + 'a';
		if (c == 0x4C)
			return 0x3000 + 'o';
		if (c == 0x4D)
			return 0x3000 + 'u';
		if (c == 0x4E)
			return 0xB000 + 'c';
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
			return '#';
		return GL_HEBREW_G0_LOWER - 0x60 + c;

	default:
		return GL_LATIN_G0 + '?';
	}
}

#undef printv
#define printv printf
// #define printv(templ, ...)

static void
new_enhance(struct fmt_page *pg, struct vt_page *vtp,
	int invc_row, int invc_column)
{
	vt_triplet *p;
	int active_column, active_row;
	int offset_column, offset_row;
	int min_column, i, j;
	struct font_d *font, *g0_g2_font;
	int gl;
	attr_char *acp;
	u8 *rawp;

	if (vtp->num_triplets <= 0) /* XXX */
		return;

	active_column = 0;
	active_row = 0;

	offset_column = 0;
	offset_row = 0;

	rawp = vtp->raw[0];
	acp = pg->data[0];
	min_column = 0;

	{
		struct vt_extension *ext;
		int char_set;

		g0_g2_font = font_d_table;

		if (!(ext = vtp->extension))
			ext = &vtp->vbi->magazine_extension[(vtp->pgno >> 8) - 1];

		char_set = ext->char_set[0];

		if (VALID_CHARACTER_SET(char_set))
			g0_g2_font = font_d_table + char_set;

		char_set = (char_set & ~7) + vtp->national;

		if (VALID_CHARACTER_SET(char_set))
			g0_g2_font = font_d_table + char_set;

		printv("enh char_set = %d\n", char_set);
	}

	font = g0_g2_font;

	/* XXX */
	for (p = vtp->triplets, i = 0; i < vtp->num_triplets;/**/ p++, i++) {
		if (p->stop)
			break;

		if (p->address >= 40) {
			/* row address triplets */

			int s = p->data >> 5;
			int row;

			switch (p->mode) {
			case 0x00:		/* full screen colour */
				if (s == 0) {
					/* TODO */
				}

				break;

			case 0x07:		/* address display row 0 */
				if (p->address != 0x3F)
					break; /* reserved, no position */

			case 0x01:		/* full row colour */
				active_column = 0;

				if (p->mode == 7)
					row = 0;
				else
					row = (p->address - 40) ? : 24;
#if 0
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
				row = (p->address - 40) ? : 24;

			set_active:
				if (row != active_row) {
					active_row = row;

					font = g0_g2_font;

					if (invc_row + row > 24)
						acp = NULL;
					else {
						rawp = vtp->raw[invc_row + row];
						acp = pg->data[invc_row + row];

						min_column = (invc_row + row == 0) ? 8 : 0;
					}
				}

				printv("enh set_active row %d col %d\n", active_row, active_column);

				break;

			/* case 0x05: reserved */
			/* case 0x06: reserved */
			/* case 0x08 ... 0x0F: PDC data */

			case 0x10:		/* origin modifier */
				if (p->data >= 72)
					break; /* invalid */
				
				offset_column = p->data;
				offset_row = p->address - 40;

				printv("enh origin modifier %d %d\n", offset_column, offset_row);

				break;

			case 0x11 ... 0x13:	/* object invocation */
				/* TODO */

				printv("enh obj invocation 0x%02x 0x%02x\n", p->mode, p->data);

				offset_column = 0;
				offset_row = 0;

				break;

			/* case 0x14: reserved */

			case 0x15 ... 0x17:	/* object definition */
				/* TODO (skip here? abort?) */

				printv("enh obj definition 0x%02x 0x%02x\n", p->mode, p->data);
				
				break;

			case 0x18:		/* drcs mode */
				/* TODO */

				printv("enh DRCS mode 0x%02x\n", p->data);

				break;

			/* case 0x19 ... 0x1E: reserved */
			
			case 0x1F:		/* termination marker */
				i = vtp->num_triplets; /* XXX */
				break;
			}
		} else {
			/* column address triplets */

			int s = p->data >> 5;			

			switch (p->mode) {
			case 0x00:			/* foreground colour */
				active_column = p->address;

				if (s == 0 && acp) {
					int foreground = p->data & 0x1F;

					j = MAX(min_column, invc_column + active_column);

					for (; j < 40; j++) {
						int raw = rawp[j] & 0x78; /* XXX parity */

						acp[j].foreground = foreground;

						/* spacing alpha foreground, set-after */
						/* spacing mosaic foreground, set-after */
						if (raw == 0x00 || raw == 0x10)
							break;
					}

					printv("enh col %d foreground %d\n", active_column, foreground);
				}
				
				break;

			case 0x01:			/* G1 block mosaic character */
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

				if (s == 0 && acp) {
					int background = p->data & 0x1F;

					j = MAX(min_column, invc_column + active_column);

					for (; j < 40; j++) {
						int raw = rawp[j] & 0x7F; /* XXX parity */

						/* spacing black background, set-at */
						/* spacing new background, set-at */
						if (raw == 0x1C || raw == 0x1D)
							break;

						acp[j].background = background;
					}

					printv("enh col %d background %d\n", active_column, background);
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
			*/

			case 0x0D:		/* drcs character invocation */
				active_column = p->address;

				/* TODO */

				printv("enh col %d DRCS 0x%02x %c\n", active_column, p->data, printable(p->data));

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
					static const char *reduced_uppercase = "ACEGIOSUZ";
					static const int composed_subst[32] = {
						0, 0, 0, 0, 0, 0, 0x701B, 0x1000 + 'a',
						0x8000 + 'a', 0x20F5, 0x30F5, 0xF000 + 't', 0x80F5, 0xA017, 0, 0,
						0x3017,	0x301B, 0x8017, 0x8019, 0x801B, 0x801C, 0x801E
					};
					int mark = p->mode - 16;
					char *s;

					gl = glyph(font->G0, NO_SUBSET, p->data);

					/* requested by the aestehics department */
					if (0xA5FE & (1 << mark)) {
						if (gl <= 0x60 && (s = strchr(reduced_uppercase, gl)))
							gl = 0x17 + (s - reduced_uppercase);
						else if (gl == 'i')
							gl = 0xF5;
					}

					if (mark > 0) 

					gl |= (p->mode - 16) << 12;

					/* again */
					for (j = 6; j <= 22; j++)
						if (gl == composed_subst[j]) {
							gl = j;
							break;
						}
			store:
					printv("enh row %d col %d print 0x%02x/0x%02x -> 0x%04x %c\n",
						active_row, active_column, p->mode, p->data, gl, printable(gl & 0x7F));

					j = invc_column + active_column;

					if (j < min_column || j > 40)
						break;

					acp[j].glyph = gl;
				}

				break;
			}
		}
	}
}

void
fmt_page(int reveal, struct fmt_page *pg, struct vt_page *vtp)
{
	char buf[16];
	struct vt_extension *ext;
	struct font_d *g0_font[2];
	int column, row, i;

	sprintf(buf, "\2%x.%02x\7", vtp->pgno, vtp->subno & 0xff);

	g0_font[0] = font_d_table;
	g0_font[1] = font_d_table;

	if (!(ext = vtp->extension))
		ext = &vtp->vbi->magazine_extension[(vtp->pgno >> 8) - 1];

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
	    | ((ext->colour_map[i] & 0xF) << 0);
    }

	i = 0;
	reveal = !!reveal;

	for (row = 0; row < H; row++) {
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

			raw = vtp->raw[0][i++] & 0x7F; /* XXX parity */

			if (row == 0 && column < 8)
				raw = buf[column];

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

			if (!wide_char)
				pg->data[row][column] = ac;

			wide_char = /*!!*/(ac.size & DOUBLE_WIDTH);

			if (wide_char && column < 39) {
				attr_char t = ac;

				t.size = OVER_TOP;
				pg->data[row][column + 1] = t;
			}

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
				if (column < 39 && vtp->raw[0][i] == 0x0a)
					ac.opacity = page_opacity;
				break;

			case 0x0B:		/* start box */
				if (column < 39 && vtp->raw[0][i] == 0x0b)
					ac.opacity = boxed_opacity;
				break;

			case 0x0D:		/* double height */
				if (row <= 0 || row >= 23)
					break;
				ac.size = DOUBLE_HEIGHT;
				double_height = TRUE;
				break;

			case 0x0E:		/* double width */
				if (column < 39)
					ac.size = DOUBLE_WIDTH;
				break;

			case 0x0F:		/* double size */
				if (column >= 39 || row <= 0 || row >= 22)
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

	new_enhance(pg, vtp, 0, 0);

	if (1)
		return;

	/* Test */

	for (row = 1; row < 24; row++)
		for (column = 0; column < 40; column++) {
			int page = ((vtp->pgno >> 4) & 15) * 10 + (vtp->pgno & 15);

			if (page <= 15) {
				if (row <= 23 && column <= 31) {
					pg->data[row][column].foreground = WHITE;
					pg->data[row][column].background = BLACK;
					pg->data[row][column].size = NORMAL;
					pg->data[row][column].glyph =
						((page & 15) << 12) + (row - 1) * 32 + column;
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
}

int
export(struct export *e, struct vt_page *vtp, char *name)
{
    struct fmt_page pg[1];

    fmt_page(e->reveal, pg, vtp);
    return e->mod->output(e, name, pg);
}
