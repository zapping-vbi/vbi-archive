#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "vt.h"
#include "misc.h"
#include "export.h"
#include "vbi.h"

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


/*
  Extra characters 1
  Latin G0 0x20				done
  Latin G0 0x40				done
  Latin G0 0x60				done

  Extra characters 2
  Latin G2 0x20				done
  Latin G2 0x40				done
  Latin G2 0x60				done

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

struct font_d {
	character_set	G0;
	character_set	G2;
	national_subset	subset;
} font_d_table[88] = {
	/* 0 - Western and central Europe */
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

	/* 24 - Middle and southeast Europe */
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

#warning national character subset substitution table incomplete

static int
national_subst[14][13] = {
	{ 0x23, 0x24, 0x40, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x7b, 0x7c, 0x7d, 0x7e },
	{ 0x23, 0xA000 + 'u', 0xF000 + 'c', 0xF000 + 't', 0xF000 + 'z', 0x2000 + 'y', 0x2000 + 'i', 0xF000 + 'r', 0x2000 + 'e', 0x2000 + 'a', 0xF000 + 'e', 0x2000 + 'u', 0xF000 + 's' },
	{ 0xA3, 0xA4, 0x40, 0xAC, 0xBD, 0xAE, 0xAD, '#', 0xD0, 0xBC, 0 /*** || ***/, 0xBE, 0xB8 },
	{ 0x23, 0x4000 + 'o', 0xF000 + 'S', 0x8000 + 'A', 0x8000 + 'O', 0xF000 + 'Z', 0x8000 + 'U', 0x4000 + 'O', 0xF000 + 's', 0x8000 + 'a', 0x8000 + 'o', 0xF000 + 'z', 0x8000 + 'u' },
	{ 0x2000 + 'e', 0x8000 + 'i', 0x1000 + 'a', 0x8000 + 'e', 0x3000 + 'e', 0x1000 + 'u', 0x3000 + 'i', '#', 0x1000 + 'e', 0x3000 + 'a', 0x3000 + 'o', 0x3000 + 'u', 0xB000 + 'c' },
	{ 0x23, 0xA4, 0xA7, 0x8000 + 'A', 0x8000 + 'O', 0x8000 + 'U', 0x5E, 0x5F, 0xB0, 0x8000 + 'a', 0x8000 + 'o', 0x8000 + 'u', 0xFB },
	{ 0xA3, 0xA4, 0x2000 + 'e', 0xB0, 0xB000 + 'c', 0xAE, 0xAD, '#', 0x1000 + 'u', 0x1000 + 'a', 0x1000 + 'o', 0x1000 + 'e', 0x1000 + 'i' },
	{ 0x23, 0xA4, 0xF000 + 'S', 0x7000 + 'e', 0xB000 + 'e', 0xF000 + 'Z', 0xF000 + 'c', 0x5000 + 'u', 0xF000 + 's', 0xE000 + 'a', 0xE000 + 'u', 0xF000 + 'z', 0xB000 + 'i' },
	{ 0x23, 0x2000 + 'n', 0xE000 + 'a', 0 /*** -Z- ***/, 0x2000 + 'S', 0xE8, 0x2000 + 'c', 0x2000 + 'o', 0xE000 + 'e', 0x7000 + 'z', 0x2000 + 's', 0xF8, 0x2000 + 'z' },
	{ 0xB000 + 'c', 0xA4, 0xA1, 0x2000 + 'a', 0x2000 + 'e', 0x2000 + 'i', 0x2000 + 'o', 0x2000 + 'u', 0xBF, 0x8000 + 'u', 0x4000 + 'n', 0x1000 + 'e', 0x1000 + 'a' },
	{ 0x23, 0x24, 0xB000 + 'T', 0x3000 + 'A', 0xB000 + 'S', 0xF000 + 'A', 0x3000 + 'I', 0xF5, 0xB000 + 't', 0x3000 + 'a', 0xB000 + 's', 0xF000 + 'a', 0x3000 + 'i' },
	{ 0x23, 0x8000 + 'E', 0x6000 + 'C', 0x2000 + 'C', 0x6000 + 'Z', 0xE2, 0x6000 + 'S', 0x8000 + 'e', 0x6000 + 'c', 0x2000 + 'c', 0x6000 + 'z', 0xF2, 0x6000 + 's' },
	{ 0x23, 0x24, 0x2000 + 'E', 0x8000 + 'A', 0x8000 + 'O', 0xA000 + 'A', 0x8000 + 'U', 0x5F, 0x2000 + 'e', 0x8000 + 'a', 0x8000 + 'o', 0xA000 + 'a', 0x8000 + 'u' },
	{ 0 /*** TL ***/, 0xF000 + 'g', 0x7000 + 'I', 0xB000 + 'S', 0x8000 + 'O', 0xB000 + 'C', 0x8000 + 'U', 0xF000 + 'G', 0xF5, 0xB000 + 's', 0x8000 + 'o', 0xB000 + 'c', 0x8000 + 'u' }
};

/*
    XXX needs optimization
 */
static int
glyph(struct font_d *f, int c)
{
	int i;

	if (c <= 0x1F)
		return c;
	else if (c <= 0x7F) {
		switch (f->G0) {
		case LATIN_G0:
			for (i = 0; i < 13; i++)
				if (c == national_subst[0][i])
					return national_subst[f->subset][i];
			return 0x0000 + c;

		default:
			break;
		}
	}

	return 0x0000 + '?';
}

void
fmt_page(int reveal, struct fmt_page *pg, struct vt_page *vtp)
{
    char buf[16];
    int column, y, i;
    int page_opacity;
    int char_set = 0;
    struct font_d *f;
    struct vt_extension *ext;

    sprintf(buf, "\2%x.%02x\7", vtp->pgno, vtp->subno & 0xff);

    if (vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE))
	page_opacity = TRANSPARENT_SPACE;
    else
	page_opacity = OPAQUE;


    f = font_d_table;

    if (!(ext = vtp->extension))
	ext = &vtp->vbi->magazine_extension[(vtp->pgno >> 8) - 1];

    char_set = ext->primary_char_set;

    if (char_set < 88 && font_d_table[char_set].G0)
	f = font_d_table + char_set;

    char_set = (char_set & ~7) + vtp->national;

    if (char_set < 88 && font_d_table[char_set].G0)
	f = font_d_table + char_set;

// printf("char_set %d\n", char_set);

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

    for (y = 0; y < H; y++)
    {
	int held_mosaic = ' ';
	int held_separated = FALSE;
	int hold = 0;
	attr_char at, after;
	int double_height;
	int concealed;

	at.foreground	= ext->foreground_clut + WHITE;
	at.background	= ext->background_clut + BLACK;
	at.attr = 0;
	at.flash	= FALSE;
	at.opacity	= page_opacity;
	at.size		= NORMAL;
	concealed	= FALSE;
	double_height	= FALSE;

	after = at;

	for (column = 0; column < W; ++column)
	{
	    int raw;

	    at.ch = vtp->data[0][i];
	    raw = vtp->raw[0][i++] & 0x7F;

	    if (y == 0 && column < 8)
		at.ch = raw = buf[column];

	    switch (at.ch) {
		case 0x00 ... 0x07:	/* alpha + fg color */
		    after.foreground = ext->foreground_clut + (at.ch & 7);
		    after.attr &= ~(EA_GRAPHIC | EA_SEPARATED);
		    concealed = FALSE;
		    goto ctrl;

		case 0x08:		/* flash */
		    after.flash = TRUE;
		    goto ctrl;

		case 0x09:		/* steady */
		    at.flash = FALSE;
		    after.flash = FALSE;
 		    goto ctrl;

		case 0x0a:		/* end box */
		    if (vtp->data[0][i] == 0x0a) /* double transmission, see G.3.1 */
			after.opacity = page_opacity;
		    goto ctrl;

		case 0x0b:		/* start box */
		    if (vtp->data[0][i] == 0x0b) /* double transmission, see G.3.1 */
			after.opacity = SEMI_TRANSPARENT; /* semi, tendency opaque ;-) */
		    goto ctrl;

		case 0x0c:		/* normal height */
		    at.size = NORMAL;
		    after.size = NORMAL;
		    goto ctrl;

		case 0x0d:		/* double height */
		    if (y <= 0 || y >= 23)
			    goto ctrl;

		    after.size = DOUBLE_HEIGHT;
		    double_height = TRUE;

		    goto ctrl;

		case 0x0e:		/* double width */
		    if (column < 39)
			after.size = DOUBLE_WIDTH;
		    goto ctrl;

		case 0x0f:		/* double size */
		    if (column >= 39 || y <= 0 || y >= 22)
			    goto ctrl;

		    after.size = DOUBLE_SIZE;
		    double_height = TRUE;

		    goto ctrl;

		case 0x10 ... 0x17:	/* mosaic + fg color */
		    after.foreground = ext->foreground_clut + (at.ch & 7);
		    after.attr |= EA_GRAPHIC;
		    concealed = FALSE;
		    goto ctrl;

		case 0x18:		/* conceal */
		    concealed = TRUE;
		    goto ctrl;

		case 0x19:		/* contiguous mosaics */
		    at.attr &= ~EA_SEPARATED;
		    after.attr &= ~EA_SEPARATED;
		    goto ctrl;

		case 0x1a:		/* separated mosaics */
		    at.attr |= EA_SEPARATED;
		    after.attr |= EA_SEPARATED;
		    goto ctrl;

		case 0x1c:		/* black bg */
		    at.background = ext->background_clut + BLACK;
		    after.background = ext->background_clut + BLACK;
		    goto ctrl;

		case 0x1d:		/* new bg */
		    at.background = at.foreground;
		    after.background = at.foreground;
		    goto ctrl;

		case 0x1e:		/* hold gfx */
		    hold = 1;
		    goto ctrl;
		
		case 0x1f:		/* release gfx */
		    hold = 0; // after ??
		    goto ctrl;

		case 0x1b:		/* ESC */
		    at.ch = ' ';
		    break;

		ctrl:
		    if (hold && (at.attr & EA_GRAPHIC)) {
			at.ch = held_mosaic;
			if (held_separated) /* G.3.3 */
			    at.attr |= EA_SEPARATED;
			else
			    at.attr &= ~EA_SEPARATED;
		    } else
			at.ch = raw = ' ';
		    break;
	    }

	    if ((at.attr & EA_GRAPHIC)
		&& (at.ch & 0xA0) == 0x20) {
		held_mosaic = at.ch;
		held_separated = !!(at.attr & EA_SEPARATED);
		at.ch += (at.ch & 0x40) ? 32 : -32;
	    }

	    if (concealed && !reveal)
		at.ch = raw = ' ';

	    /* XXX optimize */
	    if ((y == 0 && (vtp->flags & C7_SUPPRESS_HEADER))
		|| (y > 0 && (vtp->flags & C10_INHIBIT_DISPLAY)))
		at.opacity = TRANSPARENT_SPACE;
/*
	    if (at.attr & EA_GRAPHIC)
		    at.glyph = 0x0000 + ' ';
	    else
		    at.glyph = glyph(f, raw);
*/
#if 0 // too lazy to write a test page
	    if (column < 13 && y > 0 && y < 14) {
		at.ch = at.glyph = national_subst[y][column];
		at.foreground = BLACK;
		at.background = WHITE;
	    }
#endif

	    pg->data[y][column] = at;

	    if (at.size == DOUBLE_WIDTH	|| at.size == DOUBLE_SIZE) {
		at.size = OVER_TOP;
		pg->data[y][++column] = at;
	    }

	    at = after;
	}

	if (double_height) {
	    for (column = 0; column < W; column++) {
		at = pg->data[y][column];

		switch (at.size) {
		case DOUBLE_HEIGHT:
		    at.size = DOUBLE_HEIGHT2;
		    pg->data[y + 1][column] = at;
		    break;
		
		case DOUBLE_SIZE:
		    at.size = DOUBLE_SIZE2;
		    pg->data[y + 1][column] = at;
		    at.size = OVER_BOTTOM;
		    pg->data[y + 1][++column] = at;
		    break;

		default: /* NORMAL, DOUBLE_WIDTH, OVER_TOP */
		    at.size = NORMAL;
		    at.ch = ' ';
		    at.glyph = 0x0000 + ' ';
		    pg->data[y + 1][column] = at;
		    break;
		}
	    }

	    y++;
	    i += W;
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
