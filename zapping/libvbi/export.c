/*
 *  Zapzilla - Export modules interface
 *
 *  Copyright (C) 2001 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998,1999 Edgar Toernig (froese@gmx.de)
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

/* $Id: export.c,v 1.38 2001-06-23 02:50:44 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "export.h"
#include "libvbi.h"

extern vbi_export_module_priv	export_html;
extern vbi_export_module_priv	export_ppm;
extern vbi_export_module_priv	export_png;
extern vbi_export_module_priv	export_txt;
extern vbi_export_module_priv	export_ansi;
extern vbi_export_module_priv	export_string;
extern vbi_export_module_priv	export_vtx;

static vbi_export_module_priv *
vbi_export_modules[] = {
	&export_html,
	&export_ppm,
	&export_png,
	&export_txt,
	&export_ansi,
	&export_string,
	&export_vtx,
	NULL
};

#if 0

static vbi_export_module *	vbi_export_modules;

void
vbi_register_export_module(vbi_export_module *new)
{
	vbi_export_module_priv **xmp;

	for (xmp = &vbi_export_modules; *xmp; xmp = &(*xmp)->next)
		if (strcasecmp(new->keyword, (*xmp)->keyword) < 0)
			break;

	new->next = *xmp;
	*xmp = new;
}

#endif

void
vbi_export_error(vbi_export *e, char *templ, ...)
{
	va_list args;

	if (e->err_str)
		free(e->err_str);

	va_start(args, templ);
	vasprintf(&e->err_str, templ, args);
	va_end(args);
}

void
vbi_export_write_error(vbi_export *e, char *name)
{
	char *t;

	if (name)
		asprintf(&t, _("Error while writing file '%s'"), name);
	else
		t = _("Error while writing file");

	if (errno) {
		vbi_export_error(e, "%s: %s", t, strerror(errno));
		if (name)
			free(t);
	} else {
		if (e->err_str)
			free(e->err_str);
		e->err_str = t;
	}
}

char *
vbi_export_errstr(vbi_export *e)
{
        if (!e->err_str)
    		fprintf(stderr, "Uncritical bug. Either vbi_export_errstr()\n"
			"has been called without an error, or the export\n"
			"module didn't set an error string.\n");

	return e->err_str ? e->err_str : _("Unknown");
}

vbi_export_module *
vbi_export_info(vbi_export *e)
{
	return &e->mod->pub;
}

static int
find_option(vbi_export_option *xo1, char *key, char *arg)
{
	vbi_export_option *xo;

	for (xo = xo1; xo && xo->type; xo++)
		if (strcasecmp(xo->keyword, key) == 0)
			return xo - xo1;

	return -1;
}

int
vbi_export_set_option(vbi_export *e, int index, ...)
{
	vbi_export_option *xo = e->mod->options + index;
	va_list args;
	int r;

	if (!e->mod->set_option)
		return 0;

	va_start(args, index);

	if (xo->type == VBI_EXPORT_STRING) {
		char *s = va_arg(args, char *);

		r = e->mod->set_option(e, index, s, strtol(s, NULL, 0));
	} else {
		int n = va_arg(args, int);

		if (n < xo->min)
			n = xo->min;
		else if (n > xo->max)
			n = xo->max;

		r = e->mod->set_option(e, index, "", n);
	}

	va_end(args);

	return r;
}

vbi_export_option *
vbi_export_query_option(vbi_export *e, int index)
{
	vbi_export_module_priv *xm = e->mod;
	vbi_export_option *xo = xm->options;

	if (xm->query_option)
		return e->mod->query_option(e, index);

	if (!xo)
		return NULL;

	for (; xo->type && index > 0; xo++, index--);

	return xo->type ? xo : NULL;
}

static void
reset_options(vbi_export *e)
{
	vbi_export_option *xo;
	int i;

	for (i = 0; (xo = vbi_export_query_option(e, i)); i++)
		if (xo->type == VBI_EXPORT_STRING)
			vbi_export_set_option(e, i, xo->def.str);
		else
			vbi_export_set_option(e, i, xo->def.num);
}

vbi_export_module *
vbi_export_enum(int index)
{
	vbi_export_module_priv **xmp;

	for (xmp = vbi_export_modules; *xmp && index > 0; xmp++, index--);

	return *xmp ? &(*xmp)->pub : NULL;
}

vbi_export *
vbi_export_open(char *fmt, vbi_network *network, char **errstr)
{
	vbi_export_module_priv **eem, *em;
	vbi_export *e = NULL;
	char *opt, *optend, *optarg;
	int opti;

	if (!(fmt = strdup(fmt)))
		goto no_mem;

	if ((opt = strchr(fmt, ',')))
		*opt++ = 0;

	for (eem = vbi_export_modules; (em = *eem); eem++)
//	for (em = vbi_export_modules; em; em = em->next)
		if (strcasecmp(em->pub.keyword, fmt) == 0)
			break;

	if (!em) {
		if (errstr)
			asprintf(errstr, _("Unknown export format '%s'"), fmt);
		free(fmt);
		return NULL;
	}

	if (!(e = calloc(sizeof(*e) + em->local_size, 1))) {
		free(fmt);
		goto no_mem;
	}

	e->mod = em;
	e->fmt_str = fmt;

	if (network)
		memcpy(&e->network, network, sizeof(e->network));

	e->reveal = 0;
	memset(e + 1, 0, em->local_size);

	reset_options(e);

	if (!em->open || em->open(e)) {
		for (; opt; opt = optend) {
			if ((optend = strchr(opt, ',')))
			    *optend++ = 0;
			if (! *opt)
			    continue;
			if ((optarg = strchr(opt, '=')))
			    *optarg++ = 0;
			if ((opti = find_option(em->options, opt, optarg)) >= 0) {
				int n;
				
				if (!optarg) {
					if (em->options[opti].type == VBI_EXPORT_BOOL)
						n = TRUE;
					else {
						if (errstr)
							asprintf(errstr, _("Option '%s' for export format '%s' requires an argument"), opt, fmt);
						break;
					}
				} else
					n = strtol(optarg, NULL, 0);

				if (n < em->options[opti].min)
					n = em->options[opti].min;
				else if (n > em->options[opti].max)
					n = em->options[opti].max;

				if (!em->set_option(e, opti, optarg, n))
					break;
			} else {
				if (errstr)
					asprintf(errstr, _("Unknown option '%s' for export format '%s'"), opt, fmt);
				break;
			}
		}

		if (opt == 0)
			return e;

		if (em->close)
			em->close(e);
	}

	free(e);

	free(fmt);

	return NULL;

no_mem:
	if (errstr)
		*errstr = strdup(_("Out of memory"));

	return NULL;
}

void
vbi_export_close(vbi_export *e)
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
vbi_export_mkname(vbi_export *e, char *fmt,
	int pgno, int subno, char *usr)
{
    char bbuf[1024];
    char *s = bbuf;

    while ((*s = *fmt++))
	if (*s++ == '%')
	{
	    char buf[32], buf2[32];
	    int width = 0;

	    s--;
	    while (*fmt >= '0' && *fmt <= '9')
		width = width*10 + *fmt++ - '0';

	    switch (*fmt++)
	    {
		case '%':
		    s = adjust(s, "%", '%', width, 0);
		    break;
		case 'e':	// extension
		    s = adjust(s, e->mod->extension, '.', width, 1);
		    break;
		case 'n':	// network label
		    s = adjust(s, e->network.label, ' ', width, 1);
		    break;
		case 'p':	// pageno[.subno]
		    if (subno)
			s = adjust(s,strcat(strcat(hexnum(buf, pgno),
				"."), hexnum(buf2, subno)), ' ', width, 0);
		    else
			s = adjust(s, hexnum(buf, pgno), ' ', width, 0);
		    break;
		case 'S':	// subno
		    s = adjust(s, hexnum(buf, subno), '0', width, 0);
		    break;
		case 'P':	// pgno
		    s = adjust(s, hexnum(buf, pgno), '0', width, 0);
		    break;
		case 's':	// user strin
		    s = adjust(s, usr, ' ', width, 0);
		    break;
		//TODO: add date, ...
	    }
	}
    s = strdup(bbuf);
    if (! s)
	vbi_export_error(e, "out of memory");
    return s;
}

/*
 *  Create a file <name>, or do whatever the module thinks
 *  the name is useful for.
 */
int
vbi_export_name(vbi_export *e, char *name, struct fmt_page *pg)
{
	return e->mod->output(e, NULL, name, pg);
}

/*
 *  To concat files eg. headerless HTML, to store files
 *  in memory (open_memstream), ...
 */
int
vbi_export_file(vbi_export *e, FILE *fp, struct fmt_page *pg)
{
	return e->mod->output(e, fp, NULL, pg);
}
