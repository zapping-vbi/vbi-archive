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

/* $Id: exp-txt.c,v 1.15 2001-07-02 16:17:13 garetxe Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "os.h"
#include "lang.h"
#include "export.h"

static bool txt_open(vbi_export *e);
static bool txt_set_option(vbi_export *e, int opt, char *str_arg, int num_arg);
static bool txt_output(vbi_export *e, FILE *fp, char *name, struct fmt_page *pg);
static bool string_set_option(vbi_export *e, int opt, char *str_arg, int num_arg);
static bool string_output(vbi_export *e, FILE *fp, char *name, struct fmt_page *pg);

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
		VBI_EXPORT_INT,	"col1",	  "col1",	{ .num = 0 }, 0, 39, NULL, NULL
	}, {
		VBI_EXPORT_INT,	"row1",	  "row1",	{ .num = 0 }, 0, 24, NULL, NULL
	}, {
		VBI_EXPORT_INT,	"col2",	  "col2",	{ .num = 0 }, 0, 39, NULL, NULL
	}, {
		VBI_EXPORT_INT,	"row2",   "row2",	{ .num = 0 }, 0, 24, NULL, NULL
	}, {
		VBI_EXPORT_BOOL, "table", "table",	{ .num = 1 }, 0, 1, NULL, NULL
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
	int col1;
	int row1;
	int col2;
	int row2;
	int table;
	FILE *fp;
};

vbi_export_module_priv
export_txt = {
	.pub = {
		.keyword	= "ascii",
		.label		= N_("ASCII"),
		.tooltip	= N_("Export this page as ASCII text"),
	},
	.extension		= "txt",
	.options		= txt_opts,
	.local_size		= sizeof(struct txt_data),
	.open			= txt_open,
	.set_option		= txt_set_option,
	.output			= txt_output,
};

vbi_export_module_priv
export_ansi = {
	.pub = {
		.keyword	= "ansi",
		.label		= N_("ANSI"),
		.tooltip	= N_("Export this page as ANSI text"),
	},
	.extension		= "txt",
	.options		= ansi_opts,
	.local_size		= sizeof(struct txt_data),
	.open			= txt_open,
	.set_option		= txt_set_option,
	.output			= txt_output,
};

vbi_export_module_priv
export_string = {
	.pub = {
		.keyword	= "string",
	},
	.extension		= "txt",
	.options		= string_opts,
	.local_size		= sizeof(struct string_data),
	.set_option		= string_set_option,
	.output			= string_output,
};

///////////////////////////////////////////////////////

static char *
k_stpcpy(char *dst, const char *src)
{
	while ((*dst = *src++))
		dst++;
	return dst;
}

static bool
txt_open(vbi_export *e)
{
	struct txt_data *d = (struct txt_data *) e->data;

	if (e->mod == &export_ansi)
		d->color = 1;

	return TRUE;
}

static bool
txt_set_option(vbi_export *e, int opt, char *str_arg, int num_arg)
{
	struct txt_data *d = (struct txt_data *) e->data;

    switch (opt)
    {
	case 0: /* gfx-chr */
		if (strlen(str_arg) == 1)
			num_arg = *str_arg;
		if (num_arg >= 0x100)
			num_arg = '#';
		d->gfx_chr = MAX(num_arg, 0x20);
		break;
	case 1: // fg=
	    d->def_fg = (num_arg < 0) ? -1 : (num_arg > 7) ? -1 : num_arg;
	    break;
	case 2: // bg=
	    d->def_bg = (num_arg < 0) ? -1 : (num_arg > 7) ? -1 : num_arg;
	    break;
	case 3: // color
	    d->color = !!num_arg;
	    break;
	default:
	    return FALSE;
    }
    return TRUE;
}

static bool
string_set_option(vbi_export *e, int opt, char *str_arg, int num_arg)
{
	struct string_data *d = (struct string_data *) e->data;

    switch (opt)
    {
	case 0: // column1
	    d->col1 = num_arg;
	    break;
	case 1: // row1
	    d->row1 = num_arg;
	    break;
	case 2: // column2
	    d->col2 = num_arg;
	    break;
	case 3: // row2
	    d->row2 = num_arg;
	    break;
	case 4: // table mode
	    d->table = !!num_arg;
	    break;
	default:
	    return FALSE;
    }
    return TRUE;
}

static void
put_attr(vbi_export *e, attr_char *new)
{
	struct txt_data *d = (struct txt_data *) e->data;
    char buf[64];
    char *p = buf;
    attr_char ac;
    int reset = 0;
    int i;

    if (d->color)
    {
	for (i = 0; i < sizeof(attr_char); i++)
		((char *) &ac)[i] = ((char *) d->curr)[i] ^ ((char *) new)[i];

	if (ac.foreground | ac.background | ac.flash | ac.size)
	{
	    if ((ac.flash && !new->flash)
		|| (ac.size && new->size == NORMAL))	// reset some attributes ->  reset all.
		reset = 1;
	    if (ac.foreground && new->foreground == d->def_fg)	// switch to def fg -> reset all
		reset = 1;
	    if (ac.background && new->background == d->def_bg)	// switch to def bg -> reset all
		reset = 1;

	    p = k_stpcpy(buf, "\e[");
	    if (reset)
	    {
		p = k_stpcpy(p, ";");	/* "0;", but 0 isn't necessary */
		ac.flash = 1;
		ac.size = 1;			// set all attributes
		ac.foreground = new->foreground ^ d->def_fg;	// set fg if != default fg
		ac.background = new->background ^ d->def_bg;	// set bg if != default bg
	    }
	    if (ac.flash & new->flash)
		p = k_stpcpy(p, "5;");			// blink
	    if (ac.size && new->size != NORMAL)
		p = k_stpcpy(p, "1;");			// bold
	    if (ac.foreground)
		p += sprintf(p, "%d;", (new->foreground & 7) + 30);	// fg-color
	    if (ac.background)
		p += sprintf(p, "%d;", (new->background & 7) + 40);	// bg-color
	    p[-1] = 'm';	// replace last ;
	    *d->curr = *new;
	}
    }
    *p++ = new->glyph;
    *p = 0;
    fputs(buf, d->fp);
}

/*
 *  XXX suggest user option char set, currently Latin-1
 *  (in ASCII mode 8 bits too).
 */

#define MAX_COLUMNS 64

static bool
txt_output(vbi_export *e, FILE *fp, char *name, struct fmt_page *pg)
{
	struct txt_data *d = (struct txt_data *) e->data;
    attr_char def_ac[1];
    attr_char l[MAX_COLUMNS+2];
    #define L (l+1)
    int x, y;

    if (!name)
	d->fp = fp;
    else if (!(d->fp = fopen(name, "w"))) {
	vbi_export_error(e, "Cannot create file");
	return FALSE;
    }

    /* initialize default colors.  these have to be restored at EOL. */
    memset(def_ac, 0, sizeof(def_ac));
    def_ac->glyph = '\n';
    def_ac->foreground = d->def_fg;
    def_ac->background = d->def_bg;
    *d->curr = *def_ac;
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

	    if (d->color)
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
    if (name)
	fclose(d->fp);
    return TRUE;
}

static bool
string_output(vbi_export *e, FILE *fp, char *name, struct fmt_page *pg)
{
	struct string_data *d = (struct string_data *) e->data;
	int x, y;
	int spaces;

    if (d->col1 < 0 || d->col1 > 39 || d->col2 < 0 || d->col2 > 39)
	return FALSE;

    if (d->row1 < 0 || d->row1 > 24 || d->row2 < 0 || d->row2 > 24)
	return FALSE;

    if (!name)
	d->fp = fp;
    else if (!(d->fp = fopen(name, "w"))) {
	vbi_export_error(e, "Cannot create file");
	return FALSE;
    }

	for (y = d->row1; y <= d->row2; y++) {
		int x0, x1;

		x0 = (d->table || y == d->row1) ? d->col1 : 0;
		x1 = (d->table || y == d->row2) ? d->col2 : pg->columns;

		spaces = 0;

		// character conversion
		for (x = x0; x <= x1; ++x) {
			attr_char ac = pg->text[y * pg->columns + x];

			if (d->table) {
				if (ac.size > DOUBLE_SIZE)
					ac.glyph = 0x20;
			} else if (ac.size == OVER_TOP || ac.size == OVER_BOTTOM)
				continue; /* double-width/size right */
			else if (ac.size >= DOUBLE_HEIGHT2)
				/* double-height/size lower */
				if (y > d->row1)
					ac.glyph = 0x20;

			ac.glyph = glyph2latin(ac.glyph);

			if (ac.glyph == 0xA0)
				ac.glyph = 0x20;

			if (d->table)
				fputc(ac.glyph, d->fp);
			else if (ac.glyph == 0x20)
				spaces++;
			else {
				if ((x0 + spaces) < x || y == d->row1) {
					for (; spaces > 0; spaces--)
						fputc(0x20, d->fp);
				} else
					spaces = 0; /* discard leading sp. */

				fputc(ac.glyph, d->fp);
			}
		}

		/* if (!d->table) discard trailing spaces and blank lines */

		if (y < d->row2)
			fputc(d->table ? '\n' : ' ', d->fp);
		else
			for (; spaces > 0; spaces--)
				fputc(0x20, d->fp);
	}

    /* Nope. fputc(0, d->fp); */

    if (name)
	fclose(d->fp);

    return TRUE;
}
