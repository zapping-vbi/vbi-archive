/*
 *  libzvbi - Export modules
 *
 *  Copyright (C) 2001, 2002 Michael H. Schimek
 *
 *  Based on code from AleVT 1.5.1
 *  Copyright (C) 1998, 1999 Edgar Toernig <froese@gmx.de>
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

/* $Id: export.c,v 1.13.2.7 2004-02-25 17:34:18 mschimek Exp $ */

#include "../config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <iconv.h>
#include <math.h>

#include "export.h"
#include "vbi.h" /* vbi_asprintf */

/**
 * @addtogroup Export Exporting formatted Teletext and Closed Caption pages
 * @ingroup Service
 * 
 * Once libzvbi received, decoded and formatted a Teletext or Closed
 * Caption page you will want to render it on screen, print it as
 * text or store it in various formats.
 *
 * Fortunately you don't have to do it all by yourself. libzvbi provides
 * export modules converting a vbi_page into the desired format or
 * rendering directly into memory.
 *
 * A minimalistic export example:
 *
 * @code
 * static void
 * export_my_page (vbi_page *pg)
 * {
 *         vbi_export *ex;
 *         char *errstr;
 *
 *         if (!(ex = vbi_export_new ("html", &errstr))) {
 *                 fprintf (stderr, "Cannot export as HTML: %s\n", errstr);
 *                 free (errstr);
 *                 return;
 *         }
 *
 *         if (!vbi_export_file (ex, "my_page.html", pg))
 *                 puts (vbi_export_errstr (ex));
 *
 *         vbi_export_delete (ex);
 * }
 * @endcode
 */

/**
 * @addtogroup Exmod Internal export module interface
 * @ingroup Export
 *
 * This is the private interface between the public libzvbi export
 * functions and export modules. libzvbi client applications
 * don't use this.
 *
 * Export modules @c #include @c "export.h" to get these
 * definitions. See example module exp-templ.c.
 */

/**
 * @addtogroup Render Teletext and Closed Caption page render functions
 * @ingroup Export
 * 
 * These are functions to render Teletext and Closed Caption pages
 * directly into memory, essentially a more direct interface to the
 * functions of some important export modules.
 */

extern vbi_export_module vbi_export_module_html;
extern vbi_export_module vbi_export_module_png;
extern vbi_export_module vbi_export_module_ppm;
extern vbi_export_module vbi_export_module_text;
extern vbi_export_module vbi_export_module_tmpl;
extern vbi_export_module vbi_export_module_vtx;

static const vbi_export_module *
export_modules [] = {
	&vbi_export_module_ppm,
#ifdef HAVE_LIBPNG
	&vbi_export_module_png,
#endif
	&vbi_export_module_html,
	&vbi_export_module_text,
	&vbi_export_module_vtx,
#if 1
	&vbi_export_module_tmpl,
#endif
};

static vbi_export_info
local_export_info [N_ELEMENTS (export_modules)];

static const vbi_option_info
generic_options [] = {
	VBI_OPTION_STRING_INITIALIZER
	("creator", NULL, PACKAGE " " VERSION, NULL),
	VBI_OPTION_STRING_INITIALIZER
	("network", NULL, "", NULL),
	VBI_OPTION_BOOL_INITIALIZER
	("reveal", NULL, FALSE, NULL)
};

static void
reset_error			(vbi_export *		e)
{
	if (e->errstr) {
		free (e->errstr);
		e->errstr = NULL;
	}
}

/**
 * @param e Initialized vbi_export object.
 * @param templ See printf().
 * @param ... See printf(). 
 * 
 * Stores an error description in the @a export object. Including the
 * current error description (to append or prepend) is safe.
 */
void
vbi_export_error_printf		(vbi_export *		e,
				 const char *		templ,
				 ...)
{
	char buf[512];
	va_list ap;

	if (!e)
		return;

	va_start (ap, templ);
	vsnprintf (buf, sizeof (buf) - 1, templ, ap);
	va_end (ap);

	reset_error (e);

	e->errstr = strdup (buf);
}

/**
 * @param e Initialized vbi_export object.
 * 
 * Like vbi_export_error_printf this function stores an error
 * description in the @a export object, after examining the errno
 * variable and choosing an appropriate message. Only export
 * modules call this function.
 */
void
vbi_export_write_error		(vbi_export *		e)
{
	char buf[512];
	char *t;

	if (!e)
		return;

	if (e->name)
		snprintf (t = buf, sizeof (buf),
			  _("Error while writing file '%s'"), e->name);
	else
 		t = _("Error while writing file");

	if (errno) {
		vbi_export_error_printf (e, "%s: Error %d, %s", t,
					 errno, strerror (errno));
	} else {
		vbi_export_error_printf (e, "%s.", t);
	}
}

static const char *
module_name			(vbi_export *		e)
{
	const vbi_export_info *xi = e->module->export_info;

	if (xi->label)
		return _(xi->label);
	else
		return xi->keyword;
}

/**
 * @param e Initialized vbi_export object.
 * @param keyword Name of the unknown option.
 * 
 * Stores an error description in the export object.
 */
void
vbi_export_unknown_option	(vbi_export *		e,
				 const char *		keyword)
{
	vbi_export_error_printf
		(e, _("Export module '%s' has no option '%s'."),
		 module_name (e), keyword);
}

/**
 * @param e Initialized vbi_export object.
 * @param keyword Name of the unknown option.
 * @param ... Invalid value, type depending on the option.
 * 
 * Stores an error description in the export object.
 */
void
vbi_export_invalid_option	(vbi_export *		e,
				 const char *		keyword,
				 ...)
{
	char buf[512];
	const vbi_option_info *oi;

	if ((oi = vbi_export_option_info_by_keyword (e, keyword))) {
		va_list ap;
		const char *s;

		va_start (ap, keyword);

		switch (oi->type) {
		case VBI_OPTION_BOOL:
		case VBI_OPTION_INT:
		case VBI_OPTION_MENU:
			snprintf (buf, sizeof (buf) - 1,
				  "'%d'", va_arg (ap, int));
			break;

		case VBI_OPTION_REAL:
			snprintf (buf, sizeof (buf) - 1,
				  "'%f'", va_arg (ap, double));
			break;

		case VBI_OPTION_STRING:
			s = va_arg (ap, const char *);

			if (!s)
				STRCOPY (buf, "NULL");
			else
				snprintf (buf, sizeof (buf) - 1,
					  "'%s'", s);
			break;

		default:
			fprintf (stderr, "%s: unknown export option type %d\n",
				 __PRETTY_FUNCTION__, oi->type);
			STRCOPY (buf, "?");
			break;
		}

		va_end (ap);
	} else {
		buf[0] = 0;
	}

	vbi_export_error_printf (e, _("Invalid argument %s "
				      "for option %s of export module %s."),
				 buf, keyword, module_name (e));
}

/**
 * @param e Initialized vbi_export object.
 * @param d If non-zero, store pointer to allocated string here. When *d
 *   is non-zero, free(*d) the old string first.
 * @param s String to be duplicated.
 * 
 * Helper function for export modules.
 *
 * Same as the libc strdup(), except for @a d argument and setting
 * the @a e error string on failure.
 * 
 * @returns
 * @c NULL on failure, pointer to malloc()ed string otherwise.
 */
char *
vbi_export_strdup		(vbi_export *		e,
				 char **		d,
				 const char *		s)
{
	char *new_string = strdup (s ? s : "");

	if (!new_string) {
		vbi_export_error_printf
			(e, _("Out of memory in export module '%s'."),
			 module_name (e));
		errno = ENOMEM;
		return NULL;
	}

	if (d) {
		if (*d)
			free (*d);
		*d = new_string;
	}

	return new_string;
}

/**
 * @param e Initialized vbi_export object.
 * 
 * @returns
 * After an export function failed, this function returns a pointer
 * to a more detailed error description. Do not free this string. It
 * remains valid until the next call of an export function.
 */
const char *
vbi_export_errstr		(vbi_export *		e)
{
	assert (NULL != e);

	if (!e->errstr)
		return _("Unknown error.");

	return e->errstr;
}

/**
 * @param e Initialized vbi_export object.
 * @param fp Buffered i/o stream to write to.
 * @param pg Page to be exported.
 * 
 * This function writes the @a pg contents, converted to the respective
 * export module format, to the stream @a fp. The caller is responsible for
 * opening and closing the stream, don't forget to check for i/o
 * errors after closing. Note this function may write incomplete files
 * when an error occurs.
 *
 * You can call this function as many times as you want, it does not
 * change vbi_export state or the vbi_page.
 * 
 * @return 
 * @c TRUE on success.
 */
vbi_bool
vbi_export_stdio		(vbi_export *		e,
				 FILE *			fp,
				 const vbi_page *	pg)
{
	vbi_bool success;

	assert (NULL != e);
	assert (NULL != fp);
	assert (NULL != pg);

	reset_error (e);

	clearerr (fp);

	success = e->module->export (e, fp, pg);

	if (success && ferror (fp)) {
		vbi_export_write_error (e);
		success = FALSE;
	}

	return success;
}

/**
 * @param export Initialized vbi_export object.
 * @param name File to be created.
 * @param pg Page to be exported.
 * 
 * Writes the @a pg contents, converted to the respective
 * export format, into a new file of the given @a name. When an error
 * occured the incomplete file will be deleted.
 * 
 * You can call this function as many times as you want, it does not
 * change vbi_export state or the vbi_page.
 * 
 * @returns
 * @c TRUE on success.
 */
vbi_bool
vbi_export_file			(vbi_export *		e,
				 const char *		name,
				 const vbi_page *	pg)
{
	struct stat st;
	vbi_bool success;
	FILE *fp;

	assert (NULL != e);
	assert (NULL != name);
	assert (NULL != pg);

	reset_error (e);

	if (!(fp = fopen (name, "w"))) {
		vbi_export_error_printf
			(e, _("Cannot create file '%s': Error %d, %s."),
			 name, errno, strerror (errno));
		return FALSE;
	}

	e->name = name;

	success = e->module->export (e, fp, pg);

	if (success && ferror (fp)) {
		vbi_export_write_error (e);
		success = FALSE;
	}

	if (fclose (fp))
		if (success) {
			vbi_export_write_error (e);
			success = FALSE;
		}

	if (!success
	    && 0 == stat (name, &st)
	    && S_ISREG (st.st_mode))
		remove (name);

	e->name = NULL;

	return success;
}

/**
 */
void
vbi_export_set_link_fn		(vbi_export *		e,
				 vbi_export_link_fn *	function,
				 void *			user_data)
{
	assert (NULL != e);

	e->link_function = function;
	e->link_user_data = user_data;
}

/**
 */
void
vbi_export_set_pdc_fn		(vbi_export *		e,
				 vbi_export_pdc_fn *	function,
				 void *			user_data)
{
	assert (NULL != e);

	e->pdc_function = function;
	e->pdc_user_data = user_data;
}

/**
 * @param export Initialized vbi_export object.
 * @param keyword Keyword identifying the option as in vbi_option_info.
 * @param entry A place to store the current menu entry.
 * 
 * Similar to vbi_export_option_get() this function queries the current
 * value of the named option, but returns the value as number of the
 * corresponding menu entry. Naturally this must be an option with
 * menu.
 * 
 * @return 
 * @c TRUE on success, otherwise @c FALSE @a *entry is unchanged.
 */
vbi_bool
vbi_export_option_menu_get	(vbi_export *		e,
				 const char *		keyword,
				 unsigned int *		entry)
{
	const vbi_option_info *oi;
	vbi_option_value val;
	vbi_bool r;
	unsigned int i;

	assert (NULL != e);
	assert (NULL != entry);

	if (!keyword)
		return FALSE;

	reset_error (e);

	if (!(oi = vbi_export_option_info_by_keyword (e, keyword)))
		return FALSE;

	if (!vbi_export_option_get (e, keyword, &val))
		return FALSE;

	r = FALSE;

	for (i = 0; i <= (unsigned int) oi->max.num; ++i) {
		switch (oi->type) {
		case VBI_OPTION_BOOL:
		case VBI_OPTION_INT:
			if (!oi->menu.num)
				return FALSE;
			r = (oi->menu.num[i] == val.num);
			break;

		case VBI_OPTION_REAL:
			if (!oi->menu.dbl)
				return FALSE;
			r = fabs (oi->menu.dbl[i] - val.dbl) < 1e-3;
			break;

		case VBI_OPTION_MENU:
			r = (i == (unsigned int) val.num);
			break;

		default:
			fprintf (stderr, "%s: unknown export option type %d\n",
				 __PRETTY_FUNCTION__, oi->type);
			exit (EXIT_FAILURE);
		}

		if (r) {
			*entry = i;
			break;
		}
	}

	return r;
}

/**
 * @param export Initialized vbi_export object.
 * @param keyword Keyword identifying the option, as in vbi_option_info.
 * @param entry Menu entry to be selected.
 * 
 * Similar to vbi_export_option_set() this function sets the value of
 * the named option, however it does so by number of the corresponding
 * menu entry. Naturally this must be an option with menu.
 *
 * @return 
 * @c TRUE on success, otherwise @c FALSE and the option is not changed.
 */
vbi_bool
vbi_export_option_menu_set	(vbi_export *		e,
				 const char *		keyword,
				 unsigned int		entry)
{
	const vbi_option_info *oi;

	assert (NULL != e);

	if (!keyword)
		return FALSE;

	reset_error (e);

	if (!(oi = vbi_export_option_info_by_keyword (e, keyword)))
		return FALSE;

	if (entry > (unsigned int) oi->max.num)
		return FALSE;

	switch (oi->type) {
	case VBI_OPTION_BOOL:
	case VBI_OPTION_INT:
		if (!oi->menu.num)
			return FALSE;
		return vbi_export_option_set
			(e, keyword, oi->menu.num[entry]);

	case VBI_OPTION_REAL:
		if (!oi->menu.dbl)
			return FALSE;
		return vbi_export_option_set
			(e, keyword, oi->menu.dbl[entry]);

	case VBI_OPTION_MENU:
		return vbi_export_option_set (e, keyword, entry);

	default:
		fprintf (stderr, "%s: unknown export option type %d\n",
			 __PRETTY_FUNCTION__, oi->type);
		exit (EXIT_FAILURE);
	}
}

#define KEYWORD(str) (0 == strcmp (keyword, str))

/**
 * @param e Initialized vbi_export object.
 * @param keyword Keyword identifying the option as in vbi_option_info.
 * @param value A place to store the current option value.
 *
 * This function queries the current value of the named option.
 * When the option is of type VBI_OPTION_STRING @a value.str must be
 * freed with free() when the string is no longer needed. When the
 * option is of type VBI_OPTION_MENU then @a value.num contains the
 * selected entry.
 *
 * @returns
 * @c TRUE on success, otherwise @c FALSE and @a value is unchanged.
 */
vbi_bool
vbi_export_option_get		(vbi_export *		e,
				 const char *		keyword,
				 vbi_option_value *	value)
{
	vbi_bool r;

	assert (NULL != e);
	assert (NULL != value);

	if (!keyword)
		return FALSE;

	reset_error (e);

	r = TRUE;

	if (KEYWORD ("reveal")) {
		value->num = e->reveal;
	} else if (KEYWORD ("network")) {
		char *s;

		s = vbi_export_strdup (e, NULL, e->network ? : "");
		if (s)
			value->str = s;
		else
			r = FALSE;
	} else if (KEYWORD ("creator")) {
		char *s;

		s = vbi_export_strdup (e, NULL, e->creator);
		if (s)
			value->str = s;
		else
			r = FALSE;
	} else {
		const vbi_export_module *xc = e->module;

		if (xc->option_get) {
			r = xc->option_get (e, keyword, value);
		} else {
			vbi_export_unknown_option (e, keyword);
			r = FALSE;
		}
	}

	return r;
}

/**
 * @param e Initialized vbi_export object.
 * @param keyword Keyword identifying the option as in vbi_option_info.
 * @param Varargs New value to set.
 *
 * Sets the value of the named option. Make sure the value is casted
 * to the correct type (int, double, char *).
 *
 * Typical usage of vbi_export_option_set():
 * @code
 * vbi_export_option_set (export, "quality", 75.5);
 * @endcode
 *
 * Mind that options of type @c VBI_OPTION_MENU must be set by menu
 * entry number (int), all other options by value. If necessary it will
 * be replaced by the closest value possible. Use function
 * vbi_export_option_menu_set() to set options with menu
 * by menu entry.
 *
 * @return
 * @c TRUE on success, otherwise @c FALSE and the option is not changed.
 */
vbi_bool
vbi_export_option_set		(vbi_export *		e,
				 const char *		keyword,
				 ...)
{
	vbi_bool r;
	va_list ap;

	assert (NULL != e);

	if (!keyword)
		return FALSE;

	reset_error (e);

	r = TRUE;

	va_start (ap, keyword);

	if (KEYWORD ("reveal")) {
		e->reveal = !!va_arg (ap, vbi_bool);
	} else if (KEYWORD ("network")) {
		const char *network = va_arg (ap, const char *);

		if (!network || !network[0]) {
			if (e->network) {
				free (e->network);
				e->network = NULL;
			}
		} else if (!vbi_export_strdup (e, &e->network, network)) {
			r = FALSE;
		}
	} else if (KEYWORD ("creator")) {
		const char *creator = va_arg (ap, const char *);

		if (!vbi_export_strdup (e, &e->creator, creator))
			r = FALSE;
	} else {
		const vbi_export_module *xc = e->module;

		if (xc->option_set)
			r = xc->option_set (e, keyword, ap);
		else
			r = FALSE;
	}

	va_end (ap);

	return r;
}

/**
 * @param e Initialized vbi_export object.
 * @param index Index into the option table 0 ... n.
 *
 * Enumerates the options available for the given export module. You
 * should start at index 0, incrementing.
 *
 * @returns
 * Pointer to a vbi_option_info structure,
 * @c NULL if @a index is out of bounds.
 */
const vbi_option_info *
vbi_export_option_info_enum	(vbi_export *		e,
				 unsigned int		index)
{
	unsigned int size;

	assert (NULL != e);

	reset_error (e);

	size = N_ELEMENTS (generic_options) + e->module->option_info_size;

	if (index >= size)
		return NULL;

	return e->local_option_info + index;
}

/**
 * @param e Initialized vbi_export object.
 * @param keyword Option identifier as in vbi_option_info.
 *
 * Like vbi_export_option_info_enum(), except this function tries to
 * find the option by keyword.
 *
 * @returns
 * Pointer to a vbi_option_info structure,
 * @c NULL if the named option was not found.
 */
const vbi_option_info *
vbi_export_option_info_by_keyword
				(vbi_export *		e,
				 const char *		keyword)
{
	unsigned int size;
	unsigned int i;

	assert (NULL != e);

	if (!keyword)
		return NULL;

	reset_error (e);

	size = N_ELEMENTS (generic_options) + e->module->option_info_size;

	for (i = 0; i < size; ++i)
		if (0 == strcmp (keyword,
				 e->local_option_info[i].keyword))
			return e->local_option_info + i;

	vbi_export_unknown_option (e, keyword);

	return NULL;
}

/**
 * @param index Index into the export module list 0 ... n.
 *
 * Enumerates available export modules. You should start at index 0,
 * incrementing. Some modules may depend on machine features or the presence
 * of certain libraries, thus the list can vary from session to session.
 *
 * @return
 * Pointer to a vbi_export_info structure,
 * @c NULL if the index is out of bounds.
 */
const vbi_export_info *
vbi_export_info_enum		(unsigned int		index)
{
	const vbi_export_module *xc;
	vbi_export_info *xi;

	if (index >= N_ELEMENTS (export_modules))
		return NULL;

	xc = export_modules[index];
	xi = local_export_info + index;

	xi->keyword	= xc->export_info->keyword;
	xi->label	= dgettext (vbi_intl_domainname,
				    xc->export_info->label);
	xi->tooltip	= dgettext (vbi_intl_domainname,
				    xc->export_info->tooltip);
	xi->mime_type	= xc->export_info->mime_type;
	xi->extension	= xc->export_info->extension;

	return xi;
}

/**
 * @param keyword Export module identifier as in vbi_export_info and
 *   vbi_export_new().
 * 
 * Like vbi_export_info_enum(), except this function tries to find
 * an export module by keyword.
 * 
 * @return
 * Pointer to a vbi_export_info structure,
 * @c NULL if the named export module was not found.
 */
const vbi_export_info *
vbi_export_info_by_keyword	(const char *		keyword)
{
	unsigned int keylen;
	unsigned int i;

	if (!keyword)
		return NULL;

	for (keylen = 0; keyword[keylen]; ++keylen)
		if (';' == keyword[keylen]
		    || ',' == keyword[keylen])
			break;

	for (i = 0; i < N_ELEMENTS (export_modules); ++i) {
		const vbi_export_info *xi;

		xi = export_modules[i]->export_info;

		if (0 == strncmp (keyword, xi->keyword, keylen)) {
			return vbi_export_info_enum (i);
		}
	}

	return NULL;
}

/**
 * @param e vbi_export object allocated with vbi_export_new().
 *
 * Returns the export module info for the given @a export object.
 *
 * @return
 * Pointer to vbi_export_info structure or
 * @c NULL if @a e is @c NULL.
 */
const vbi_export_info *
vbi_export_info_from_export	(const vbi_export *	e)
{
	assert (NULL != e);

	return e->local_export_info;
}

static void
free_option_info		(vbi_option_info *	oi,
				 unsigned int		oi_size)
{
	unsigned int i;

	for (i = 0; i < oi_size; ++i) {
		if (VBI_OPTION_MENU == oi[i].type) {
			free ((char *) oi[i].menu.str);
		}
	}

	free (oi);
}

/**
 * @param e vbi_export object allocated with vbi_export_new().
 * 
 * Frees all resources associated with the export object.
 */
void
vbi_export_delete		(vbi_export *		e)
{
	const vbi_export_module *xc;

	if (!e)
		return;

	free (e->errstr);
	free (e->network);
	free (e->creator);

 	xc = e->module;

	free_option_info (e->local_option_info,
			  N_ELEMENTS (generic_options) + xc->option_info_size);

	if (xc->_new && xc->_delete) {
		xc->_delete (e);
	} else {
		free (e);
	}
}

static vbi_option_info *
localize_option_info		(const vbi_option_info *oi,
				 unsigned int		oi_size)
{
	vbi_option_info *loi;
	unsigned int size;
	unsigned int i;

	size = (N_ELEMENTS (generic_options) + oi_size) * sizeof (*loi);

	if (!(loi = malloc (size)))
		return NULL;

	memcpy (loi, generic_options,
		N_ELEMENTS (generic_options) * sizeof (*loi));

	memcpy (loi + N_ELEMENTS (generic_options), oi,
		oi_size * sizeof (*loi));

	oi_size += N_ELEMENTS (generic_options);

	for (i = 0; i < oi_size; ++i) {
		loi[i].label = dgettext (vbi_intl_domainname, loi[i].label);
		loi[i].tooltip = dgettext (vbi_intl_domainname,
					   loi[i].tooltip);

		if (VBI_OPTION_MENU == loi[i].type) {
			unsigned int j;
			char **menu;

			size = loi[i].max.num + 1;

			if (!(menu = malloc (size * sizeof (*menu)))) {
				free_option_info (loi, i);
				return NULL;
			}

			for (j = 0; j < size; ++j)
				menu[j] = dgettext (vbi_intl_domainname,
						    loi[i].menu.str[j]);

			loi[i].menu.str = (const char * const *) menu;
		}
	}

	return loi;
}

static void
reset_options			(vbi_export *		e)
{
	const vbi_option_info *oi;
	int i;

	for (i = 0; (oi = vbi_export_option_info_enum (e, i)); ++i) {
		switch (oi->type) {
		case VBI_OPTION_BOOL:
		case VBI_OPTION_INT:
			if (oi->menu.num)
				vbi_export_option_set
					(e, oi->keyword,
					 oi->menu.num[oi->def.num]);
			else
				vbi_export_option_set
					(e, oi->keyword, oi->def.num);
			break;

		case VBI_OPTION_REAL:
			if (oi->menu.dbl)
				vbi_export_option_set
					(e, oi->keyword,
					 oi->menu.dbl[oi->def.num]);
			else
				vbi_export_option_set
					(e, oi->keyword, oi->def.dbl);
			break;

		case VBI_OPTION_STRING:
			if (oi->menu.str)
				vbi_export_option_set
					(e, oi->keyword,
					 oi->menu.str[oi->def.num]);
			else
				vbi_export_option_set
					(e, oi->keyword, oi->def.str);
			break;

		case VBI_OPTION_MENU:
			vbi_export_option_set (e, oi->keyword, oi->def.num);
			break;

		default:
			fprintf (stderr, "%s: unknown export option type %u\n",
				__PRETTY_FUNCTION__, oi->type);
			exit (EXIT_FAILURE);
		}
	}
}

static vbi_bool
option_string			(vbi_export *		e,
				 const char *		s2)
{
	const vbi_option_info *oi;
	char *s;
	char *s1;
	char *keyword;
	char *string;
	char quote;
	vbi_bool r = TRUE;

	if (!(s = s1 = vbi_export_strdup (e, NULL, s2)))
		return FALSE;

	do {
		while (isspace (*s))
			++s;

		if (',' == *s || ';' == *s) {
			++s;
			continue;
		}

		if (!*s) {
			free (s1);
			return TRUE;
		}

		keyword = s;

		while (isalnum (*s) || '_' == *s)
			++s;

		if (!*s)
			goto invalid;

		*s++ = 0;

		while (isspace (*s) || '=' == *s)
			++s;

		if (!*s) {
 invalid:
			vbi_export_error_printf
				(e, _("Invalid option string \"%s\"."), s2);
			break;
		}

		if (!(oi = vbi_export_option_info_by_keyword (e, keyword)))
			break;

		switch (oi->type) {
		case VBI_OPTION_BOOL:
		case VBI_OPTION_INT:
		case VBI_OPTION_MENU:
			r = vbi_export_option_set
				(e, keyword, (int) strtol (s, &s, 0));
			break;

		case VBI_OPTION_REAL:
			r = vbi_export_option_set
				(e, keyword, (double) strtod (s, &s));
			break;

		case VBI_OPTION_STRING:
			quote = 0;

			if ('\'' == *s || '\"' == *s) /* " */
				quote = *s++;
			string = s;

			while (*s && *s != quote
			       && (quote || (',' != *s && ';' != *s)))
				++s;
			if (*s)
				*s++ = 0;

			r = vbi_export_option_set (e, keyword, string);

			break;

		default:
			fprintf (stderr, "%s: unknown export option type %d\n",
				 __PRETTY_FUNCTION__, oi->type);
			exit (EXIT_FAILURE);
		}
	} while (r);

	free (s1);

	return FALSE;
}

/**
 * @param keyword Export module identifier as in vbi_export_info.
 * @param errstr If not @c NULL this function stores a pointer to an error
 *   description here. You must free() this string when no longer needed.
 * 
 * Creates a new export module instance to export a vbi_page in
 * the respective module format. As a special service you can
 * initialize options by appending to the @param keyword like this:
 * 
 * @code
 * vbi_export_new ("keyword; quality=75.5, comment=\"example text\"");
 * @endcode
 * 
 * @return
 * Pointer to a newly allocated vbi_export object which must be
 * freed by calling vbi_export_delete(). @c NULL is returned and
 * the @a errstr may be set (else @a NULL) if some problem occurred.
 */
vbi_export *
vbi_export_new			(const char *		keyword,
				 char **		errstr)
{
	char key[256];
	const vbi_export_module *xc;
	vbi_export *e;
	unsigned int keylen;
	unsigned int i;

	if (errstr)
		*errstr = NULL;

	if (!keyword)
		keyword = "";

	for (keylen = 0; keyword[keylen]
		     && keylen < (sizeof (key) - 1)
		     && ';' != keyword[keylen]
		     && ',' != keyword[keylen]; ++keylen)
		key[keylen] = keyword[keylen];

	key[keylen] = 0;

	for (i = 0; i < N_ELEMENTS (export_modules); ++i) {
		const vbi_export_info *xi;

		xc = export_modules[i];
		xi = xc->export_info;

		if (0 == strncmp (keyword, xi->keyword, keylen))
			break;
	}

	if (i >= N_ELEMENTS (export_modules)) {
		vbi_asprintf (errstr, _("Unknown export module '%s'."), key);
		return NULL;
	}

	if (!xc->_new)
		e = calloc (1, sizeof (*e));
	else
		e = xc->_new ();

	if (!e) {
		vbi_asprintf (errstr, _("Cannot initialize export module '%s', "
				       "probably lack of memory."),
			      xc->export_info->label ?
			      xc->export_info->label : keyword);
		return NULL;
	}

	e->module = xc;
	e->errstr = NULL;

	e->local_export_info = vbi_export_info_enum (i);

	e->local_option_info =
		localize_option_info (xc->option_info,
				      xc->option_info_size);

	if (!e->local_option_info) {
		free (e);
		vbi_asprintf (errstr, _("Cannot initialize export module "
					"'%s', out of memory."),
			      xc->export_info->label ?
			      xc->export_info->label : keyword);
		return NULL;
	}

	e->name = NULL;

	reset_options (e);

	if (keyword[keylen] && !option_string (e, keyword + keylen + 1)) {
		if (errstr)
			*errstr = strdup(vbi_export_errstr(e));
		vbi_export_delete(e);
		return NULL;
	}

	if (errstr)
		errstr = NULL;

	return e;
}
