#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "vt.h"
#include "misc.h"
#include "export.h"

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

void
fmt_page(int reveal, struct fmt_page *pg, struct vt_page *vtp)
{
    char buf[16];
    int x, y;
    u8  *p = vtp->data[0];
    int page_opacity;

    sprintf(buf, "\2%x.%02x\7", vtp->pgno, vtp->subno & 0xff);

    if (vtp->flags & (C5_NEWSFLASH | C6_SUBTITLE))
	page_opacity = TRANSPARENT_SPACE;
    else
	page_opacity = OPAQUE;

    for (y = 0; y < H; y++)
    {
	int held_mosaic = ' ';
	int held_separated = FALSE;
	int hold = 0;
	attr_char at, after;
	int double_height;
	int concealed;

	at.foreground	= WHITE;
	at.background	= BLACK;
	at.attr = 0;
	at.flash	= FALSE;
	at.opacity	= page_opacity;
	at.size		= NORMAL;
	concealed	= FALSE;
	double_height	= FALSE;

	after = at;

	for (x = 0; x < W; ++x)
	{
	    at.ch = *p++;

	    if (y == 0 && x < 8)
		at.ch = buf[x];

	    switch (at.ch) {
		case 0x00 ... 0x07:	/* alpha + fg color */
		    after.foreground = at.ch & 7;
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
		    if (*p == 0x0a) /* double transmission, see G.3.1 */
			after.opacity = page_opacity;
		    goto ctrl;

		case 0x0b:		/* start box */
		    if (*p == 0x0b) /* double transmission, see G.3.1 */
			after.opacity = OPAQUE;
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
		    if (x < 39)
			after.size = DOUBLE_WIDTH;
		    goto ctrl;

		case 0x0f:		/* double size */
		    if (x >= 39 || y <= 0 || y >= 22)
			    goto ctrl;

		    after.size = DOUBLE_SIZE;
		    double_height = TRUE;

		    goto ctrl;

		case 0x10 ... 0x17:	/* mosaic + fg color */
		    after.foreground = at.ch & 7;
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

		case 0x1c:		/* black bf */
		    at.background = 0;
		    after.background = 0;
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
			at.ch = ' ';
		    break;
	    }

	    if ((at.attr & EA_GRAPHIC)
		&& (at.ch & 0xA0) == 0x20) {
		held_mosaic = at.ch;
		held_separated = !!(at.attr & EA_SEPARATED);
		at.ch += (at.ch & 0x40) ? 32 : -32;
	    }

	    if (concealed && !reveal)
		at.ch = ' ';

	    if ((y == 0 && (vtp->flags & C7_SUPPRESS_HEADER))
		|| (y > 0 && (vtp->flags & C10_INHIBIT_DISPLAY)))
		at.opacity = TRANSPARENT_SPACE;

	    pg->data[y][x] = at;

	    if (at.size == DOUBLE_WIDTH	|| at.size == DOUBLE_SIZE) {
		at.size = OVER_TOP;
		pg->data[y][++x] = at;
	    }

	    at = after;
	}

	if (double_height) {
	    for (x = 0; x < W; x++) {
		at = pg->data[y][x];

		switch (at.size) {
		case DOUBLE_HEIGHT:
		    at.size = DOUBLE_HEIGHT2;
		    pg->data[y + 1][x] = at;
		    break;
		
		case DOUBLE_SIZE:
		    at.size = DOUBLE_SIZE2;
		    pg->data[y + 1][x] = at;
		    at.size = OVER_BOTTOM;
		    pg->data[y + 1][++x] = at;
		    break;

		default: /* NORMAL, DOUBLE_WIDTH, OVER_TOP */
		    at.size = NORMAL;
		    at.ch = ' ';
		    pg->data[y + 1][x] = at;
		    break;
		}
	    }

	    y++;
	    p += W;
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
