/*
 *  Zapzilla - Text export functions
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998,1999 Edgar Toernig (froese@gmx.de)
 *
 *  Zapzilla modifications
 *  Copyright (C) 2001 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

/* $Id: exp-txt.c,v 1.6 2001-01-05 03:51:52 mschimek Exp $ */

#include <stdio.h>
#include <string.h>
#include "os.h"
#include "export.h"

static int txt_open(struct export *e);
static int txt_option(struct export *e, int opt, char *arg);
static int txt_output(struct export *e, char *name, struct fmt_page *pg);

static char *txt_opts[] =	// module options
{
    "color",			// generate ansi color codes (and attributes)
    "gfx-chr=<char>",		// substitute <char> for gfx-symbols
    "fg=<0-7|none>",		// assume term has <x> as foreground color
    "bg=<0-7|none>",		// assume term has <x> as background color
    0
};

struct txt_data			// private data in struct export
{
    u8 color;
    u8 gfx_chr;
    u8 def_fg;
    u8 def_bg;
    struct fmt_char curr[1];
    FILE *fp;
};

struct export_module export_txt[1] =	// exported module definition
{
  {
    "ascii",			// id
    "txt",			// extension
    txt_opts,			// options
    sizeof(struct txt_data),	// data size
    txt_open,			// open
    0,				// close
    txt_option,			// option
    txt_output,			// output
  }
};

struct export_module export_ansi[1] =	// exported module definition
{
  {
    "ansi",			// id
    "txt",			// extension
    txt_opts,			// options
    sizeof(struct txt_data),	// data size
    txt_open,			// open
    0,				// close
    txt_option,			// option
    txt_output,			// output
  }
};

///////////////////////////////////////////////////////

#define D  ((struct txt_data *)e->data)


#ifdef BSD
char *
stpcpy(char *dst, const char *src)
{
    while (*dst = *src++)
	dst++;
    return dst;
}
#endif


static int
txt_open(struct export *e)
{
    D->gfx_chr = '#';
    D->def_fg = -1;
    D->def_bg = -1;
    if (e->mod == export_ansi)
	D->color = 1;
    return 0;
}

static int
txt_option(struct export *e, int opt, char *arg)
{
    switch (opt)
    {
	case 1: // color
	    D->color = 1;
	    break;
	case 2: // gfx-chr=
	    D->gfx_chr = *arg ?: ' ';
	    break;
	case 3: // fg=
	    D->def_fg = *arg - '0';
	    break;
	case 4: // bg=
	    D->def_bg = *arg - '0';
	    break;
    }
    return 0;
}


static void
put_attr(struct export *e, attr_char *new)
{
    char buf[64];
    char *p = buf;
    attr_char ac;
    int reset = 0;
    int i;

    if (D->color)
    {
	for (i = 0; i < sizeof(attr_char); i++)
		((char *) &ac)[i] = ((char *) D->curr)[i] ^ ((char *) new)[i];

	if (ac.foreground | ac.background | ac.flash | ac.size)
	{
	    if ((ac.flash && !new->flash)
		|| (ac.size && new->size == NORMAL))	// reset some attributes ->  reset all.
		reset = 1;
	    if (ac.foreground && new->foreground == D->def_fg)	// switch to def fg -> reset all
		reset = 1;
	    if (ac.background && new->background == D->def_bg)	// switch to def bg -> reset all
		reset = 1;

	    p = stpcpy(buf, "\e[");
	    if (reset)
	    {
		p = stpcpy(p, ";");		// "0;" but 0 isn't neccesary
		ac.flash = 1;
		ac.size = 1;			// set all attributes
		ac.foreground = new->foreground ^ D->def_fg;	// set fg if != default fg
		ac.background = new->background ^ D->def_bg;	// set bg if != default bg
	    }
	    if (ac.flash & new->flash)
		p = stpcpy(p, "5;");			// blink
	    if (ac.size && new->size != NORMAL)
		p = stpcpy(p, "1;");			// bold
	    if (ac.foreground)
		p += sprintf(p, "%d;", (new->foreground & 7) + 30);	// fg-color
	    if (ac.background)
		p += sprintf(p, "%d;", (new->background & 7) + 40);	// bg-color
	    p[-1] = 'm';	// replace last ;
	    *D->curr = *new;
	}
    }
    *p++ = new->glyph;
    *p = 0;
    fputs(buf, D->fp);
}

/*
 *  XXX suggest user option char set, currently Latin-1
 *  (in ASCII mode 8 bits too). Pointless to implement yet
 *  without a props menu.
 */

static int
txt_output(struct export *e, char *name, struct fmt_page *pg)
{
    attr_char def_ac[1];
    attr_char l[W+2];
    #define L (l+1)
    int x, y;

    D->fp = fopen(name, "w");
    if (not D->fp)
    {
	export_error("cannot create file");
	return -1;
    }

    /* initialize default colors.  these have to be restored at EOL. */
    memset(def_ac, 0, sizeof(def_ac));
    def_ac->glyph = '\n';
    def_ac->foreground = D->def_fg;
    def_ac->background = D->def_bg;
    *D->curr = *def_ac;
    L[-1] = L[W] = *def_ac;

    for (y = 0; y < H; y++)
	{
	    // character conversion
	    for (x = 0; x < W; ++x)
	    {
		attr_char ac = pg->data[y][x];

		if (ac.size > DOUBLE_SIZE) {
			ac.glyph = 0x20;
			ac.size = NORMAL;
		} else {
			ac.glyph = glyph2latin(ac.glyph);

			if (ac.glyph == 0xA0)
				ac.glyph = 0x20;
		}

		L[x] = ac;
	    }

	    if (D->color)
	    {
		// optimize color and attribute changes

		// delay fg and attr changes as far as possible
		for (x = 0; x < W; ++x)
		    if (L[x].glyph == ' ')
		    {
			L[x] = L[x-1];
			L[x].glyph = 0x20;
		    }

		// move fg and attr changes to prev bg change point
		for (x = W-1; x >= 0; x--)
		    if (L[x].glyph == ' ' && L[x].background == L[x+1].background)
		    {
			L[x] = L[x+1];
			L[x].glyph = 0x20;
		    }
	    }

	    // now emit the whole line (incl EOL)
	    for (x = 0; x < W+1; ++x)
		put_attr(e, L + x);
	}
    fclose(D->fp);
    return 0;
}
