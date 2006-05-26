/*
 *  Template for export modules
 */

/* $Id: exp-templ.c,v 1.6.2.7 2006-05-26 00:43:05 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <limits.h>		/* UINT_MAX */

#include "misc.h"
#include "version.h"
#include "intl-priv.h"
#include "export-priv.h"

typedef struct tmpl_instance
{
	/* Common to all export modules */
	vbi3_export		export;

	/* Our private stuff */

	/* Options */
	int			flip;
	int			day;
	int			prime;
	double			quality;
	char *			comment;
	int			weekday;

	int			counter;
} tmpl_instance;

static vbi3_export *
tmpl_new			(const _vbi3_export_module *unused)
{
	tmpl_instance *tmpl;

	unused = unused;

	if (!(tmpl = vbi3_malloc (sizeof (*tmpl))))
		return NULL;

	CLEAR (*tmpl);

	/*
	 *  The caller will initialize tmpl->export.class for us
	 *  and reset all options to their defaults, we only
	 *  have to initialize our private stuff.
	 */

	tmpl->counter = 0;

	return &tmpl->export;
}

static void
tmpl_delete			(vbi3_export *		e)
{
/* Safer than tmpl_instance *tmpl = (tmpl_instance *)(vbi3_export *) e */
	tmpl_instance *tmpl = PARENT (e, tmpl_instance, export);

	/* Uninitialize our private stuff and options */

	vbi3_free (tmpl->comment);

	vbi3_free (tmpl);
}

/* N_(), _() are NLS functions, see info gettext. */
static const char *
string_menu_items [] = {
	N_("Sunday"),
	N_("Monday"),
	N_("Tuesday"),
	N_("Wednesday"),
	N_("Thursday"),
	N_("Friday"),
	N_("Saturday")
};

static int
int_menu_items [] = {
	1, 3, 5, 7, 11, 13, 17, 19
};

static const vbi3_option_info
option_info [] = {
	_VBI3_OPTION_BOOL_INITIALIZER
	  /*
	   *  Option keywords must be unique within their module
	   *  and shall contain only "AZaz09_".
	   *  Note "network", "creator" and "reveal" are reserved generic
	   *  options, filtered by the export api functions.
	   */
	("flip", N_("Boolean option"),
	 FALSE, N_("This is a boolean option")),
	_VBI3_OPTION_INT_RANGE_INITIALIZER
	("day", N_("Select a month day"),
	 /* default, min, max, step, has no tooltip */
	    13,       1,   31,  1,      NULL),
	_VBI3_OPTION_INT_MENU_INITIALIZER
	("prime", N_("Select a prime"),
	 0, int_menu_items, N_ELEMENTS (int_menu_items),
	 N_("Default is the first, '1'")),
	_VBI3_OPTION_REAL_RANGE_INITIALIZER
	("quality", N_("Compression quality"),
	 100, 1, 100, 0.01, NULL),
	/* VBI3_OPTION_REAL_MENU_INITIALIZER like int */
	_VBI3_OPTION_STRING_INITIALIZER
	("comment", N_("Add a comment"),
	 "default comment", N_("Another tooltip")),
	_VBI3_OPTION_MENU_INITIALIZER
	("weekday", N_("Select a weekday"),
	 2, string_menu_items, 7, N_("Default is Tuesday"))
};

#define KEYWORD(str) (0 == strcmp (keyword, str))

/* Get an option (optional if we have no options). */
static vbi3_bool
option_get			(vbi3_export *		e,
				 const char *		keyword,
				 vbi3_option_value *	value)
{
	tmpl_instance *tmpl = PARENT (e, tmpl_instance, export);

	if (KEYWORD ("flip")) {
		value->num = tmpl->flip;
	} else if (KEYWORD ("day")) {
		value->num = tmpl->day;
	} else if (KEYWORD ("prime")) {
		value->num = tmpl->prime;
	} else if (KEYWORD ("quality")) {
		value->dbl = tmpl->quality;
	} else if (KEYWORD ("comment")) {
		if (!(value->str = _vbi3_export_strdup (e, NULL,
			tmpl->comment ? tmpl->comment : "")))
			return FALSE;
	} else if (KEYWORD ("weekday")) {
		value->num = tmpl->weekday;
	} else {
		_vbi3_export_unknown_option (e, keyword);
		return FALSE;
	}

	return TRUE; /* success */
}

/* Set an option (optional if we have no options). */
static vbi3_bool
option_set			(vbi3_export *		e,
				 const char *		keyword,
				 va_list		ap)
{
	tmpl_instance *tmpl = PARENT (e, tmpl_instance, export);

	if (KEYWORD ("flip")) {
		tmpl->flip = !!va_arg (ap, vbi3_bool);
	} else if (KEYWORD ("day")) {
		int day = va_arg (ap, int);

		if (day < 1 || day > 31) {
			_vbi3_export_invalid_option (e, keyword, day);
			return FALSE;
		}

		tmpl->day = day;
	} else if (KEYWORD("prime")) {
		unsigned int i;
		unsigned int d, dmin = UINT_MAX;
		int value = va_arg (ap, int);

		/* or return an error */
		for (i = 0; i < N_ELEMENTS (int_menu_items); ++i) {
			if ((d = abs (int_menu_items[i] - value)) < dmin) {
				tmpl->prime = int_menu_items[i];
				dmin = d;
			}
		}
	} else if (KEYWORD ("quality")) {
		double quality = va_arg (ap, double);

		/* or return an error */
		if (quality < 1)
			quality = 1;
		else if (quality > 100)
			quality = 100;

		tmpl->quality = quality;
	} else if (KEYWORD ("comment")) {
		const char *comment = va_arg (ap, const char *);

		/* Note the option remains unchanged in case of error */
		if (!_vbi3_export_strdup (e, &tmpl->comment, comment))
			return FALSE;
	} else if (KEYWORD ("weekday")) {
		unsigned int day = va_arg (ap, unsigned int);

		/* or return an error */
		tmpl->weekday = day % 7;
	} else {
		_vbi3_export_unknown_option (e, keyword);
		return FALSE;
	}

	return TRUE; /* success */
}

/* The output function, mandatory. */
static vbi3_bool
export				(vbi3_export *		e,
				 const vbi3_page *	pg)
{
	tmpl_instance *tmpl = PARENT (e, tmpl_instance, export);

	/* Write pg to e->fp, that's all. */
	fprintf (e->fp, "Page %x.%x\n", pg->pgno, pg->subno);

	tmpl->counter++; /* just for fun */

	/* Should any of the module functions return unsuccessful
	   they better post a description of the problem.
	   Parameters like printf, no linefeeds '\n' please. */
	/*
	vbi3_export_error_printf (_("Writing failed: %s"), strerror (errno));
         */

	return FALSE; /* no success (since we didn't write anything) */
}

/* Let's describe us. You can leave away assignments unless mandatory. */
static const vbi3_export_info
export_info = {
	/* The mandatory keyword must be unique and shall
           contain only "AZaz09_" */
	.keyword		= "templ",
	/* When omitted this module can still be used by
	   libzvbi clients but won't be listed in a UI. */
	.label			= N_("Template"),
	.tooltip		= N_("This is just an export template"),

	.mime_type		= "misc/example",
	.extension		= "tmpl",
};

const _vbi3_export_module
_vbi3_export_module_tmpl = {
	.export_info		= &export_info,

	/* Functions to allocate and free a tmpl_class vbi3_export instance.
	   When you omit these, libzvbi will allocate a bare struct
	   vbi3_export */
	._new			= tmpl_new,
	._delete		= tmpl_delete,

	/* Pointer to option array, if any. */
	.option_info		= option_info,
	.option_info_size	= N_ELEMENTS (option_info),

	.option_get		= option_get,
	.option_set		= option_set,

	/* Function to export a page, mandatory */
	.export			= export
};
