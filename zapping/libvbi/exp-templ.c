/*
 *  Template for export modules
 *
 *  Placed in the public domain.
 */

/* $Id: exp-templ.c,v 1.5 2001-09-11 07:13:41 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "../config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "export.h"

typedef struct
{
	int foo;

} templ_data;

/* Called at vbi_export_open, optional. */

static bool
templ_open(vbi_export *e)
{
	templ_data *d = (templ_data *) e->data;

	/*
	 *  It's not necessary to reset options, vbi_export_open
	 *  calls templ_set_option for all options, passing
	 *  their respective defaults.
	 */

	return TRUE; /* success */
}

/* Called at vbi_export_close, optional. */

static void
templ_close(vbi_export *e)
{
	templ_data *d = (templ_data *) e->data;
}

/* Set options if there are any. */

static bool
templ_set_option(vbi_export *e, int opt, char *str_arg, int num_arg)
{
	templ_data *d = (templ_data *) e->data;

	/*
	 *  Options <opt> are numbered 0 ... n. The argument is
	 *  passed as <num_arg> for bool, int and menu options, as
	 *  <str_arg> for string options.
	 */

	switch (opt) {
	}

	return TRUE; /* success */
}

/*
 *  N_(), _() are i18n functions, see info gettext.
 */
static char *
menu_items[] = {
	N_("Sunday"), N_("Monday"), N_("Tuesday"),
	N_("Wednesday"), N_("Thursday"), N_("Friday"), N_("Saturday")
};

static vbi_export_option
tmpl_opts[] = {
		/*
		 *  type, unique keyword (for command line etc), label,
		 *  default (union), minimum, maximum, menu, tooltip
		 */
	{
		VBI_EXPORT_BOOL,	"john",		N_("Freedom"),
		{ .num = FALSE }, FALSE, TRUE, NULL, N_("Click here if you need any")
	}, {
		VBI_EXPORT_INT,		"paul",		N_("Acceptable bugs per feature"),
		{ .num = 1000 }, 0, 1000000, NULL, N_("Fixes available really soon now")
	}, {
		VBI_EXPORT_MENU,	"george",	N_("Prefered crash day"),
		{ .num = 1 }, 0, 6, menu_items, NULL /* no tooltip */
	}, {
		VBI_EXPORT_STRING,	"ringo",	N_("Your name"),
		{ .str = "Bill" }, 0, 0, NULL, N_("Will be reported to improve "
						  "your web experience")
	}, {
		0 /* End */
	}
};

/* Only needed to enumerate options dynamically. */

static vbi_export_option *
tmpl_query_option(vbi_export *, int opt)
{
	if (opt >= 0 && opt <= 3)
		return &templ_opts[opt];
	else
		return NULL;
}

/*
 *  The output function, mandatory.
 */
static bool
tmpl_output(vbi_export *e, FILE *fp, char *name, struct fmt_page *pgp)
{
	templ_data *d = (templ_data *) e->data;

	/*
	 *  When <fp> is non-zero, write to this file. Otherwise
	 *  use <name> to create a file or whatever. <pgp> is the
	 *  page to export.
	 */

	/*
	 *  Should any of the module functions return unsuccessful
	 *  they better post a description of the problem.
	 *  Parameters like printf, no linefeeds '\n' please.
	 */
	set_errstr_printf(_("Cannot export: %s"), strerror(errno));

	return FALSE; /* no success */
}

/*
 *  Let's describe us.
 *  You can leave away assignments unless mandatory.
 */
vbi_export_module_priv
export_tmpl = {
	.pub = {
		.keyword	= "templ",		/* Must be unique, mandatory */
		.label		= N_("Template"),	/* For the UI */
		.tooltip	= N_("This is just an export template"),
	},

	.extension		= "tmpl",		/* -> "filename.tmpl" */
	.local_size		= sizeof(tmpl_data),	/* Private data */

	/* Functions to be called at vbi_export_open and _close. */
	.open			= tmpl_open,
	.close			= tmpl_close,

	/*
	 *  Either a static table of <options> or a function
	 *  <query_option> enumerating available options.  
	 */
	.options		= tmpl_opts,
	.query_option		= tmpl_query_option,

	/* Function to set an option. */
	.set_option		= tmpl_set_option,

	/* Function to export a page, mandatory. */
	.output			= tmpl_output,
};

/*
 *  Doesn't work, sigh. Register in export.c.
 */
// VBI_AUTOREG_EXPORT_MODULE(export_tmpl)



