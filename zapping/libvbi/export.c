#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "vt.h"
#include "export.h"
#include "vbi.h"
#include "libvbi.h"
#include "hamm.h"
#include "lang.h"
#include "../common/types.h"
#include "../common/math.h"


extern struct export_module export_txt[1];
extern struct export_module export_ansi[1];
extern struct export_module export_string[1];
extern struct export_module export_html[1];
extern struct export_module export_png;
extern struct export_module export_ppm;
extern struct export_module export_vtx[1];

struct export_module *modules[] =
{
    export_txt,
    export_ansi,
    export_string,
    export_html,
    &export_ppm,
#ifdef HAVE_LIBPNG
    &export_png,
#endif
    export_vtx,
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
find_option(vbi_export_option *xo1, char *key, char *arg)
{
	vbi_export_option *xo;

	for (xo = xo1; xo && xo->type; xo++) {
		if (strcasecmp(xo->keyword, key) == 0) {
			if ((xo->type == VBI_EXPORT_BOOL) == (arg == 0))
				return xo - xo1;
			else
				return -1;
		}
	}

	return -1;
}

int
vbi_export_set_option(struct export *exp, int index, ...)
{
	vbi_export_option *xo = exp->mod->options + index;
	va_list args;
	int r;

	va_start(args, index);

	if (xo->type == VBI_EXPORT_STRING) {
		char *s = va_arg(args, char *);

		r = exp->mod->option(exp, index, s, strtol(s, NULL, 0));
	} else {
		int n = va_arg(args, int);

		if (n < xo->min)
			n = xo->min;
		else if (n > xo->max)
			n = xo->max;

		r = exp->mod->option(exp, index, "", n);
	}

	va_end(args);

	return r;
}

struct export *
export_open(char *fmt, vbi_network *network)
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
		if (network)
			memcpy(&e->network, network, sizeof(e->network));
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
			if ((opti = find_option(em->options, opt, optarg)) > 0)
			{
			    int n = strtol(optarg, NULL, 0);

			    if (n < em->options[opti].min)
				n = em->options[opti].min;
			    else if (n > em->options[opti].max)
				n = em->options[opti].max;
			
			    if (em->option(e, opti, optarg, n))
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
adjust(char *p, char *str, char fill, int width, int deq)
{
    int c, l = width - strlen(str);

    while (l-- > 0)
	*p++ = fill;
    while ((c = *str++)) {
	if (deq && strchr(" /*?", c))
    	    c = '_';
	*p++ = c;
    }
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
		    p = adjust(p, "%", '%', width, 0);
		    break;
		case 'e':	// extension
		    p = adjust(p, e->mod->extension, '.', width, 1);
		    break;
		case 'n':	// network label
		    p = adjust(p, e->network.label, ' ', width, 1);
		    break;
		case 'p':	// pageno[.subno]
		    if (subno)
			p = adjust(p,strcat(strcat(hexnum(buf, pgno),
				"."), hexnum(buf2, subno)), ' ', width, 0);
		    else
			p = adjust(p, hexnum(buf, pgno), ' ', width, 0);
		    break;
		case 'S':	// subno
		    p = adjust(p, hexnum(buf, subno), '0', width, 0);
		    break;
		case 'P':	// pgno
		    p = adjust(p, hexnum(buf, pgno), '0', width, 0);
		    break;
		case 's':	// user strin
		    p = adjust(p, usr, ' ', width, 0);
		    break;
		//TODO: add date, ...
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
