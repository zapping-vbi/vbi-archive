#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "libvbi.h"
#include "vt.h"
#include "export.h"
#include "vbi.h"
#include "hamm.h"
#include "lang.h"
#include "../common/types.h"
#include "../common/math.h"


extern struct export_module export_txt[1];
extern struct export_module export_ansi[1];
extern struct export_module export_string[1];
extern struct export_module export_html[1];
extern struct export_module export_png[1];
extern struct export_module export_ppm[1];

struct export_module *modules[] =
{
    export_txt,
    export_ansi,
    export_string,
    export_html,
    export_ppm,
#ifdef HAVE_LIBPNG
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

void
export_error(struct export *e, char *str, ...)
{
	va_list args;

	if (e->err_str)
		free(e->err_str);

	va_start(args, str);
	vasprintf(&e->err_str, str, args);
	va_end(args);
}

char *
export_errstr(struct export *e)
{
	return e->err_str ? e->err_str : "cause unknown";
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
    struct export *e = NULL;
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
	    if ((e = calloc(sizeof(*e) + em->local_size, 1)))
	    {
		e->mod = em;
		e->fmt_str = fmt;
		e->reveal = 0;
		memset(e + 1, 0, em->local_size);
		if (! em->open || em->open(e) == 0)
		{
		    for (; opt; opt = optend)
		    {
			if ((optend = strchr(opt, ',')))
			    *optend++ = 0;
			if (! *opt)
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
				export_error(e, "%s: unknown option", opt);
			    else if (optarg)
				export_error(e, "%s: takes no arg", opt);
			    else
				export_error(e, "%s: missing arg", opt);
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
		export_error(e, "out of memory");
	}
	else
	    export_error(e, "unknown format: %s", fmt);
	free(fmt);
    }
    else
	export_error(e, "out of memory");
    return 0;
}

void
export_close(struct export *e)
{
    if (e->mod->close)
	e->mod->close(e);
    if (e->err_str)
	free(e->err_str);
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
export_mkname(struct export *e, char *fmt,
	int pgno, int subno, char *usr)
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
		    if (subno)
			p = adjust(p,strcat(strcat(hexnum(buf, pgno),
				"."), hexnum(buf2, subno)), ' ', width);
		    else
			p = adjust(p, hexnum(buf, pgno), ' ', width);
		    break;
		case 'S':	// subno
		    p = adjust(p, hexnum(buf, subno), '0', width);
		    break;
		case 'P':	// pgno
		    p = adjust(p, hexnum(buf, pgno), '0', width);
		    break;
		case 's':	// user strin
		    p = adjust(p, usr, ' ', width);
		    break;
		//TODO: add date, channel name, ...
	    }
	}
    p = strdup(bbuf);
    if (! p)
	export_error(e, "out of memory");
    return p;
}


int
export(struct export *e, struct fmt_page *pg, char *name)
{
	return e->mod->output(e, name, pg);
}
