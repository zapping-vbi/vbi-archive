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

/* $Id: exp-txt.c,v 1.11 2001-03-17 07:44:29 mschimek Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "os.h"
#include "export.h"

static int txt_open(struct export *e);
static int txt_option(struct export *e, int opt, char *str_arg, int num_arg);
static int txt_output(struct export *e, char *name, struct fmt_page *pg);
static int string_option(struct export *e, int opt, char *str_arg, int num_arg);
static int string_output(struct export *e, char *name, struct fmt_page *pg);

/* future */
#undef _
#define _(String) (String)
#undef N_
#define N_(String) (String)

#define MAX(a, b) ((a) > (b) ? (a) : (b))

static char *
colour_names[] = {
	N_("Black"), N_("Red"), N_("Green"), N_("Yellow"),
	N_("Blue"), N_("Magenta"), N_("Cyan"), N_("White"),
	N_("Any")
};

static vbi_export_option txt_opts[] = {
	{
		VBI_EXPORT_STRING,	"gfx-chr",	N_("Graphics char"),
		{ .str = "#" }, 0, 0, NULL, N_("Replacement for block graphic characters: a single character or decimal (32) or hex (0x20) code")
	}, {
		VBI_EXPORT_MENU,	"fg",		N_("Foreground"),
		{ .num = 8 }, 0, 8, colour_names, N_("Assumed terminal foreground color")
	}, {
		VBI_EXPORT_MENU,	"bg",		N_("Background"),
		{ .num = 8 }, 0, 8, colour_names, N_("Assumed terminal background color")
	}, {
		VBI_EXPORT_BOOL,	"color",	N_("ANSI colors"),
		{ .num = FALSE }, FALSE, TRUE, NULL, N_("Insert ANSI color escape codes")
	}, {
		0
	}
};

static vbi_export_option ansi_opts[] = {
	{
		VBI_EXPORT_STRING,	"gfx-chr",	N_("Graphics char"),
		{ .str = "#" }, 0, 0, NULL, N_("Replacement for block graphic characters: a single character or decimal (32) or hex (0x20) code")
	}, {
		VBI_EXPORT_MENU,	"fg",		N_("Foreground"),
		{ .num = 8 }, 0, 8, colour_names, N_("Assumed terminal foreground color")
	}, {
		VBI_EXPORT_MENU,	"bg",		N_("Background"),
		{ .num = 8 }, 0, 8, colour_names, N_("Assumed terminal background color")
	}, {
		0
	}
};

/*
 *  For internal use only.
 */
static vbi_export_option string_opts[] = {
	{
		VBI_EXPORT_INT,	"col",	  "col",	{ .num = 0 }, 0, 39, NULL, NULL
	}, {
		VBI_EXPORT_INT,	"row",	  "row",	{ .num = 0 }, 0, 24, NULL, NULL
	}, {
		VBI_EXPORT_INT,	"width",  "width",	{ .num = 40 }, 1, 40, NULL, NULL
	}, {
		VBI_EXPORT_INT,	"height", "height",	{ .num = 25 }, 1, 25, NULL, NULL
	}, {
		0
	}
};

struct txt_data			// private data in struct export
{
    unsigned char color;
    unsigned char gfx_chr;
    unsigned char def_fg;
    unsigned char def_bg;
    attr_char curr[1];
    FILE *fp;
};

struct string_data
{
	int col;
	int row;
	int width;
	int height;
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
    ansi_opts,			// options
    sizeof(struct txt_data),	// data size
    txt_open,			// open
    0,				// close
    txt_option,			// option
    txt_output,			// output
  }
};

struct export_module export_string[1] =	// exported module definition
{
  {
    "string",			// id
    "txt",			// extension
    string_opts,			// options
    sizeof(struct string_data),	// data size
    0,			// open
    0,				// close
    string_option,		// option
    string_output,		// output
  }
};

///////////////////////////////////////////////////////

#define D  ((struct txt_data *)e->data)
#define S  ((struct string_data *)e->data)

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

static int number(const char *arg)
{
	int result = 0;

	for (;*arg >= '0' && *arg <= '9'; arg++)
		result = result*10 + (*arg-'0');

	return result;
}

static int
txt_option(struct export *e, int opt, char *str_arg, int num_arg)
{
    switch (opt)
    {
	case 0: /* gfx-chr */
		if (strlen(str_arg) == 1)
			num_arg = *str_arg;
		if (num_arg >= 0x100)
			num_arg = '#';
		D->gfx_chr = MAX(num_arg, 0x20);
		break;
	case 1: // fg=
	    D->def_fg = (num_arg < 0) ? -1 : (num_arg > 7) ? -1 : num_arg;
	    break;
	case 2: // bg=
	    D->def_bg = (num_arg < 0) ? -1 : (num_arg > 7) ? -1 : num_arg;
	    break;
	case 3: // color
	    D->color = !!num_arg;
	    break;
    }
    return 0;
}

static int
string_option(struct export *e, int opt, char *str_arg, int num_arg)
{
    switch (opt)
    {
	case 1: // column
	    S->col = num_arg;
	    break;
	case 2: // row
	    S->row = num_arg;
	    break;
	case 3: // width
	    S->width = num_arg;
	    break;
	case 4: // height
	    S->height = num_arg;
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

#define MAX_COLUMNS 64

static int
txt_output(struct export *e, char *name, struct fmt_page *pg)
{
    attr_char def_ac[1];
    attr_char l[MAX_COLUMNS+2];
    #define L (l+1)
    int x, y;

    D->fp = fopen(name, "w");
    if (! D->fp)
    {
	export_error(e, "cannot create file");
	return -1;
    }

    /* initialize default colors.  these have to be restored at EOL. */
    memset(def_ac, 0, sizeof(def_ac));
    def_ac->glyph = '\n';
    def_ac->foreground = D->def_fg;
    def_ac->background = D->def_bg;
    *D->curr = *def_ac;
    L[-1] = L[pg->columns] = *def_ac;

    for (y = 0; y < pg->rows; y++)
	{
	    // character conversion
	    for (x = 0; x < pg->columns; ++x)
	    {
		attr_char ac = pg->text[y * pg->columns + x];

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
		for (x = 0; x < pg->columns; ++x)
		    if (L[x].glyph == ' ')
		    {
			L[x] = L[x-1];
			L[x].glyph = 0x20;
		    }

		// move fg and attr changes to prev bg change point
		for (x = pg->columns-1; x >= 0; x--)
		    if (L[x].glyph == ' ' && L[x].background == L[x+1].background)
		    {
			L[x] = L[x+1];
			L[x].glyph = 0x20;
		    }
	    }

	    // now emit the whole line (incl EOL)
	    for (x = 0; x < pg->columns+1; ++x)
		put_attr(e, L + x);
	}
    fclose(D->fp);
    return 0;
}

static int
string_output(struct export *e, char *name, struct fmt_page *pg)
{
    int x, y;
    char *dest_buf, *p;

    if (S->col < 0  ||  S->col > 39  || S->col+S->width > 40  ||
	S->width < 0)
	return 0;

    if (S->row < 0  ||  S->row > 24  || S->row+S->height > 25 ||
	S->height < 0)
	return 0;

    dest_buf = malloc((S->width+1)*S->height*sizeof(char) + 1);
    if (!dest_buf)
	return 0;
    p = dest_buf;

    for (y = S->row; y < S->row+S->height; y++)
	{
	    if (pg->double_height_lower & (1<<y))
		continue;

	    // character conversion
	    for (x = S->col; x < S->col+S->width; ++x)
		{
		    attr_char ac = pg->text[y * pg->columns + x];
		    
		    if (ac.size > DOUBLE_SIZE) {
			ac.glyph = 0x20;
			ac.size = NORMAL;
		    } else {
			ac.glyph = glyph2latin(ac.glyph);
			
			if (ac.glyph == 0xA0)
			    ac.glyph = 0x20;
		    }
		    
		    *p++ = ac.glyph;
		}
	    
	    if (y < S->row+S->height-1)
		*p++ = '\n';
	}
    
    *p = 0;
    
    /* hope the architecture supports this (most will) */
    return (int)dest_buf;
}
