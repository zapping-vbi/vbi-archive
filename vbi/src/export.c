/*
 *  libzvbi - Export modules
 *
 *  Copyright (C) 2001, 2002, 2003, 2004 Michael H. Schimek
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

/* $Id: export.c,v 1.13.2.14 2006-05-18 16:49:19 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <iconv.h>
#include <math.h>		/* fabs() */

#include "misc.h"
#ifdef ZAPPING8
#  include "common/intl-priv.h"
#else
#  include "intl-priv.h"
#endif
#include "export-priv.h"

/**
 * @addtogroup Export Exporting formatted Teletext and Closed Caption pages
 * @ingroup Service
 * 
 * Once libzvbi received, decoded and formatted a Teletext or Closed
 * Caption page you will want to render it on screen, print it as
 * text or store it in various formats.
 *
 * Fortunately you don't have to do it all by yourself. libzvbi provides
 * export modules converting a vbi3_page into the desired format or
 * rendering directly into memory.
 *
 * A minimalistic export example:
 *
 * @code
 * static void
 * export_my_page (vbi3_page *pg)
 * {
 *         vbi3_export *ex;
 *         char *errstr;
 *
 *         if (!(ex = vbi3_export_new ("html", &errstr))) {
 *                 fprintf (stderr, "Cannot export as HTML: %s\n", errstr);
 *                 free (errstr);
 *                 return;
 *         }
 *
 *         if (!vbi3_export_file (ex, "my_page.html", pg))
 *                 puts (vbi3_export_errstr (ex));
 *
 *         vbi3_export_delete (ex);
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

extern _vbi3_export_module _vbi3_export_module_html;
extern _vbi3_export_module _vbi3_export_module_mpsub;
extern _vbi3_export_module _vbi3_export_module_png;
extern _vbi3_export_module _vbi3_export_module_ppm;
extern _vbi3_export_module _vbi3_export_module_qttext;
extern _vbi3_export_module _vbi3_export_module_realtext;
extern _vbi3_export_module _vbi3_export_module_sami;
extern _vbi3_export_module _vbi3_export_module_subrip;
extern _vbi3_export_module _vbi3_export_module_subviewer;
extern _vbi3_export_module _vbi3_export_module_text;
extern _vbi3_export_module _vbi3_export_module_tmpl;
extern _vbi3_export_module _vbi3_export_module_vtx;

static const _vbi3_export_module *
export_modules [] = {
	&_vbi3_export_module_ppm,
#ifdef HAVE_LIBPNG
	&_vbi3_export_module_png,
#endif
	&_vbi3_export_module_html,
	&_vbi3_export_module_text,
	&_vbi3_export_module_vtx,
	&_vbi3_export_module_mpsub,
	&_vbi3_export_module_qttext,
	&_vbi3_export_module_realtext,
	&_vbi3_export_module_sami,
	&_vbi3_export_module_subrip,
	&_vbi3_export_module_subviewer,
	/* &_vbi3_export_module_tmpl, */
};

static vbi3_export_info
local_export_info [N_ELEMENTS (export_modules)];

static const vbi3_option_info
generic_options [] = {
	_VBI3_OPTION_STRING_INITIALIZER
	("creator", NULL, PACKAGE " " VERSION, NULL),
	_VBI3_OPTION_STRING_INITIALIZER
	("network", NULL, "", NULL),
	_VBI3_OPTION_BOOL_INITIALIZER
	("reveal", NULL, FALSE, NULL)
};

static void
reset_error			(vbi3_export *		e)
{
	if (e->errstr) {
		vbi3_free (e->errstr);
		e->errstr = NULL;
	}
}

/**
 * @param e Initialized vbi3_export object.
 * @param templ See printf().
 * @param ... See printf(). 
 * 
 * Stores an error description in the @a export object. Including the
 * current error description (to append or prepend) is safe.
 */
void
_vbi3_export_error_printf	(vbi3_export *		e,
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
 * @param e Initialized vbi3_export object.
 * 
 * Like vbi3_export_error_printf this function stores an error
 * description in the @a export object, after examining the errno
 * variable and choosing an appropriate message. Only export
 * modules call this function.
 */
void
_vbi3_export_write_error		(vbi3_export *		e)
{
	if (!e)
		return;

	if (errno) {
		_vbi3_export_error_printf (e, "%s.", strerror (errno));
	} else {
		_vbi3_export_error_printf (e, _("Write error."));
	}
}

void
_vbi3_export_malloc_error		(vbi3_export *		e)
{
	if (!e)
		return;

	_vbi3_export_error_printf (e, _("Out of memory."));
}

static const char *
module_name			(vbi3_export *		e)
{
	const vbi3_export_info *xi = e->module->export_info;

	if (xi->label)
		return _(xi->label);
	else
		return xi->keyword;
}

/**
 * @param e Initialized vbi3_export object.
 * @param keyword Name of the unknown option.
 * 
 * Stores an error description in the export object.
 */
void
_vbi3_export_unknown_option	(vbi3_export *		e,
				 const char *		keyword)
{
	_vbi3_export_error_printf
		(e, _("Export module %s has no option %s."),
		 module_name (e), keyword);
}

/**
 * @param e Initialized vbi3_export object.
 * @param keyword Name of the unknown option.
 * @param ... Invalid value, type depending on the option.
 * 
 * Stores an error description in the export object.
 */
void
_vbi3_export_invalid_option	(vbi3_export *		e,
				 const char *		keyword,
				 ...)
{
	char buf[512];
	const vbi3_option_info *oi;

	if ((oi = vbi3_export_option_info_by_keyword (e, keyword))) {
		va_list ap;
		const char *s;

		va_start (ap, keyword);

		switch (oi->type) {
		case VBI3_OPTION_BOOL:
		case VBI3_OPTION_INT:
		case VBI3_OPTION_MENU:
			snprintf (buf, sizeof (buf) - 1,
				  "'%d'", va_arg (ap, int));
			break;

		case VBI3_OPTION_REAL:
			snprintf (buf, sizeof (buf) - 1,
				  "'%f'", va_arg (ap, double));
			break;

		case VBI3_OPTION_STRING:
			s = va_arg (ap, const char *);

			if (!s)
				STRCOPY (buf, "NULL");
			else
				snprintf (buf, sizeof (buf) - 1,
					  "'%s'", s);
			break;

		default:
			fprintf (stderr, "%s: unknown export option type %d\n",
				 __FUNCTION__, oi->type);
			STRCOPY (buf, "?");
			break;
		}

		va_end (ap);
	} else {
		buf[0] = 0;
	}

	_vbi3_export_error_printf (e, _("Invalid argument %s "
				       "for option %s of export module %s."),
				  buf, keyword, module_name (e));
}

/**
 * @param e Initialized vbi3_export object.
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
_vbi3_export_strdup		(vbi3_export *		e,
				 char **		d,
				 const char *		s)
{
	char *new_string = strdup (s ? s : "");

	if (!new_string) {
		_vbi3_export_malloc_error (e);
		errno = ENOMEM;
		return NULL;
	}

	if (d) {
		if (*d)
			vbi3_free (*d);
		*d = new_string;
	}

	return new_string;
}

/**
 * @param e Initialized vbi3_export object.
 * 
 * @returns
 * After an export function failed, this function returns a pointer
 * to a more detailed error description. It remains valid until the
 * next call of an export function.
 */
const char *
vbi3_export_errstr		(vbi3_export *		e)
{
	assert (NULL != e);

	if (!e->errstr)
		return _("Unknown error.");

	return e->errstr;
}

/**
 * @param e Initialized vbi3_export object.
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
 * change vbi3_export state or the vbi3_page.
 * 
 * @return 
 * @c TRUE on success.
 */
vbi3_bool
vbi3_export_stdio		(vbi3_export *		e,
				 FILE *			fp,
				 const vbi3_page *	pg)
{
	vbi3_bool success;

	assert (NULL != e);
	assert (NULL != fp);

	if (!e->module->export_info->open_format) {
		if (NULL == pg)
			return TRUE;
	}

	e->fp = fp;

	reset_error (e);

	clearerr (fp);

	success = e->module->export (e, pg);

	if (success && ferror (fp)) {
		_vbi3_export_write_error (e);
		success = FALSE;
	}

	e->fp = NULL;

	return success;
}

/**
 * @param export Initialized vbi3_export object.
 * @param name File to be created.
 * @param pg Page to be exported.
 * 
 * Writes the @a pg contents, converted to the respective
 * export format, into a new file of the given @a name. When an error
 * occured the incomplete file will be deleted.
 * 
 * You can call this function as many times as you want, it does not
 * change vbi3_export state or the vbi3_page.
 * 
 * @returns
 * @c TRUE on success.
 */
vbi3_bool
vbi3_export_file			(vbi3_export *		e,
				 const char *		name,
				 const vbi3_page *	pg)
{
	struct stat st;
	vbi3_bool success;

	assert (NULL != e);
	assert (NULL != name);
	assert (NULL != pg);

	reset_error (e);

	if (!(e->fp = fopen (name, "w"))) {
		_vbi3_export_error_printf
			(e, _("Could not create %s. %s."),
			 name, strerror (errno));
		return FALSE;
	}

	e->name = name;

	success = e->module->export (e, pg);

	if (success && ferror (e->fp)) {
		_vbi3_export_write_error (e);
		success = FALSE;
	}

	if (fclose (e->fp)) {
		if (success) {
			_vbi3_export_write_error (e);
			success = FALSE;
		}
	}

	e->fp = NULL;

	if (!success
	    && 0 == stat (name, &st)
	    && S_ISREG (st.st_mode))
		remove (name);

	e->name = NULL;

	return success;
}

#ifndef ZAPPING8

static void
export_stream			(vbi3_event *		ev,
				 void *			user_data)
{
	ev = ev;
	user_data = user_data;

#if 0
	vbi3_export *e = (vbi3_export *) user_data;
	vbi3_page_private pgp;

	if (!(VBI3_EVENT_TTX_PAGE == ev->type)
	    || e->stream.pgno != ev.ttx_page.pgno)
		return;

	if (VBI3_ANY_SUBNO != e->stream.subno
	    && e->stream.subno != ev.ttx_page.subno)
		return;

	/* to do get page */

	if (!vbi3_format_vt_page (ev->vbi,
				 ev->ev.ttx_page.pgno,
				 ev->ev.ttx_page.subno,
				 0))
		return;

	if (!vbi3_export_stdio (e, e->fp, &pgp.pg)) {
		/* to do */
	}

	/* to do destroy page */
#endif
}

/**
 * @param e Initialized vbi3_export object.
 *
 * Finalizes a file created by a streaming export context. Closes the
 * file when streaming was started with vbi3_export_stream_file(). Detaches
 * the vbi3_export context from the vbi3_decoder.
 *
 * @returns
 * FALSE on failure.
 *
 * @bug
 * Not implemented yet.
 */
vbi3_bool
vbi3_export_stream_close		(vbi3_export *		e)
{
	e = e;

	/* to do */
	return FALSE;
}

/**
 * @param e Initialized vbi3_export object.
 * @param fp Buffered i/o stream to write to.
 * @param vbi VBI decoder providing vbi3_pages.
 * @param pgno Page number of the page to export.
 * @param subno Subpage number of the page to export, can be
 *   @c VBI3_ANY_SUBNO.
 * @param format_options Format option list, see
 *   vbi3_fetch_vt_page() for details.
 *
 * Attaches a vbi3_export context to a vbi3_decoder, to export page
 * @a pgno, @a subno every time it is received. This is most useful to
 * convert subtitle pages to a subtitle file.
 *
 * @returns
 * FALSE on failure.
 *
 * @bugs
 * Not implemented yet.
 */
vbi3_bool
vbi3_export_stream_stdio_va_list	(vbi3_export *		e,
				 FILE *			fp,
				 vbi3_decoder *		vbi,
	    			 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 va_list		format_options)
{
	e = e;
	vbi = vbi;
	fp = fp;
	format_options = format_options;

	e->stream.pgno		= pgno;
	e->stream.subno		= subno;

	/* to do */
	return FALSE;

	export_stream (0, 0);
}

/**
 * @param e Initialized vbi3_export object.
 * @param fp Buffered i/o stream to write to.
 * @param vbi VBI decoder providing vbi3_pages.
 * @param pgno Page number of the page to export.
 * @param subno Subpage number of the page to export, can be
 *   @c VBI3_ANY_SUBNO.
 * @param ... Format option list, see vbi3_fetch_vt_page() for details.
 *
 * Attaches a vbi3_export context to a vbi3_decoder, to export page
 * @a pgno, @a subno every time it is received. This is most useful to
 * convert subtitle pages to a subtitle file.
 *
 * @returns
 * FALSE on failure.
 *
 * @bugs
 * Not implemented yet.
 */
vbi3_bool
vbi3_export_stream_stdio		(vbi3_export *		e,
				 FILE *			fp,
				 vbi3_decoder *		vbi,
	    			 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 ...)
{
	va_list format_options;
	vbi3_bool r;

	va_start (format_options, subno);
	r = vbi3_export_stream_stdio_va_list
		(e, fp, vbi, pgno, subno, format_options);
	va_end (format_options);

	return r;
}

/**
 * @param e Initialized vbi3_export object.
 * @param name File to be created.
 * @param vbi VBI decoder providing vbi3_pages.
 * @param pgno Page number of the page to export.
 * @param subno Subpage number of the page to export, can be
 *   @c VBI3_ANY_SUBNO.
 * @param format_options Format option list, see
 *   vbi3_fetch_vt_page() for details.
 *
 * Attaches a vbi3_export context to a vbi3_decoder, to export page
 * @a pgno, @a subno every time it is received. This is most useful to
 * convert subtitle pages to a subtitle file.
 *
 * @returns
 * FALSE on failure.
 *
 * @bugs
 * Not implemented yet.
 */
vbi3_bool
vbi3_export_stream_file_va_list	(vbi3_export *		e,
				 const char *		name,
				 vbi3_decoder *		vbi,
	    			 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 va_list		format_options)
{
	e = e;
	vbi = vbi;
	name = name;
	format_options = format_options;

	e->stream.pgno		= pgno;
	e->stream.subno		= subno;

	/* to do */
	return FALSE;
}

/**
 * @param e Initialized vbi3_export object.
 * @param name File to be created.
 * @param vbi VBI decoder providing vbi3_pages.
 * @param pgno Page number of the page to export.
 * @param subno Subpage number of the page to export, can be
 *   @c VBI3_ANY_SUBNO.
 * @param ... Format option list, see vbi3_fetch_vt_page() for details.
 *
 * Attaches a vbi3_export context to a vbi3_decoder, to export page
 * @a pgno, @a subno every time it is received. This is most useful to
 * convert subtitle pages to a subtitle file.
 *
 * @returns
 * FALSE on failure.
 *
 * @bugs
 * Not implemented yet.
 */
vbi3_bool
vbi3_export_stream_file		(vbi3_export *		e,
				 const char *		name,
				 vbi3_decoder *		vbi,
	    			 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 ...)
{
	va_list format_options;
	vbi3_bool r;

	va_start (format_options, subno);
	r = vbi3_export_stream_file_va_list
		(e, name, vbi, pgno, subno, format_options);
	va_end (format_options);

	return r;
}

#endif /* !ZAPPING8 */

/**
 */
void
vbi3_export_set_link_cb		(vbi3_export *		e,
				 vbi3_export_link_cb *	callback,
				 void *			user_data)
{
	assert (NULL != e);

	e->link_callback = callback;
	e->link_user_data = user_data;
}

/**
 */
void
vbi3_export_set_pdc_cb		(vbi3_export *		e,
				 vbi3_export_pdc_cb *	callback,
				 void *			user_data)
{
	assert (NULL != e);

	e->pdc_callback = callback;
	e->pdc_user_data = user_data;
}

/**
 */
void
vbi3_export_set_timestamp	(vbi3_export *		e,
				 double			timestamp)
{
	assert (NULL != e);

	if (!e->stream.have_timestamp) {
		e->stream.start_timestamp = timestamp;
		e->stream.have_timestamp = TRUE;
	}

	e->stream.timestamp = timestamp;
}

/**
 * @param e Initialized vbi3_export object.
 * @param keyword Keyword identifying the option as in vbi3_option_info.
 * @param entry A place to store the current menu entry.
 * 
 * Similar to vbi3_export_option_get() this function queries the current
 * value of the named option, but returns the value as number of the
 * corresponding menu entry. Naturally this must be an option with
 * menu.
 * 
 * @return 
 * @c TRUE on success, otherwise @c FALSE @a *entry is unchanged.
 */
vbi3_bool
vbi3_export_option_menu_get	(vbi3_export *		e,
				 const char *		keyword,
				 unsigned int *		entry)
{
	const vbi3_option_info *oi;
	vbi3_option_value val;
	vbi3_bool r;
	unsigned int i;

	assert (NULL != e);
	assert (NULL != keyword);
	assert (NULL != entry);

	reset_error (e);

	if (!(oi = vbi3_export_option_info_by_keyword (e, keyword)))
		return FALSE;

	if (!vbi3_export_option_get (e, keyword, &val))
		return FALSE;

	r = FALSE;

	for (i = 0; i <= (unsigned int) oi->max.num; ++i) {
		switch (oi->type) {
		case VBI3_OPTION_BOOL:
		case VBI3_OPTION_INT:
			if (!oi->menu.num)
				return FALSE;
			r = (oi->menu.num[i] == val.num);
			break;

		case VBI3_OPTION_REAL:
			if (!oi->menu.dbl)
				return FALSE;
			r = fabs (oi->menu.dbl[i] - val.dbl) < 1e-3;
			break;

		case VBI3_OPTION_MENU:
			r = (i == (unsigned int) val.num);
			break;

		default:
			fprintf (stderr, "%s: unknown export option type %d\n",
				 __FUNCTION__, oi->type);
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
 * @param export Initialized vbi3_export object.
 * @param keyword Keyword identifying the option, as in vbi3_option_info.
 * @param entry Menu entry to be selected.
 * 
 * Similar to vbi3_export_option_set() this function sets the value of
 * the named option, however it does so by number of the corresponding
 * menu entry. Naturally this must be an option with menu.
 *
 * @return 
 * @c TRUE on success, otherwise @c FALSE and the option is not changed.
 */
vbi3_bool
vbi3_export_option_menu_set	(vbi3_export *		e,
				 const char *		keyword,
				 unsigned int		entry)
{
	const vbi3_option_info *oi;

	assert (NULL != e);
	assert (NULL != keyword);

	reset_error (e);

	if (!(oi = vbi3_export_option_info_by_keyword (e, keyword)))
		return FALSE;

	if (entry > (unsigned int) oi->max.num)
		return FALSE;

	switch (oi->type) {
	case VBI3_OPTION_BOOL:
	case VBI3_OPTION_INT:
		if (!oi->menu.num)
			return FALSE;
		return vbi3_export_option_set
			(e, keyword, oi->menu.num[entry]);

	case VBI3_OPTION_REAL:
		if (!oi->menu.dbl)
			return FALSE;
		return vbi3_export_option_set
			(e, keyword, oi->menu.dbl[entry]);

	case VBI3_OPTION_MENU:
		return vbi3_export_option_set (e, keyword, entry);

	default:
		fprintf (stderr, "%s: unknown export option type %d\n",
			 __FUNCTION__, oi->type);
		exit (EXIT_FAILURE);
	}
}

#define KEYWORD(str) (0 == strcmp (keyword, str))

/**
 * @param e Initialized vbi3_export object.
 * @param keyword Keyword identifying the option as in vbi3_option_info.
 * @param value A place to store the current option value.
 *
 * This function queries the current value of the named option.
 * When the option is of type VBI3_OPTION_STRING @a value.str must be
 * freed with vbi3_free() when the string is no longer needed. When the
 * option is of type VBI3_OPTION_MENU then @a value.num contains the
 * selected entry.
 *
 * @returns
 * @c TRUE on success, otherwise @c FALSE and @a value is unchanged.
 */
vbi3_bool
vbi3_export_option_get		(vbi3_export *		e,
				 const char *		keyword,
				 vbi3_option_value *	value)
{
	vbi3_bool r;

	assert (NULL != e);
	assert (NULL != keyword);
	assert (NULL != value);

	reset_error (e);

	r = TRUE;

	if (KEYWORD ("reveal")) {
		value->num = e->reveal;
	} else if (KEYWORD ("network")) {
		char *s;

		s = _vbi3_export_strdup (e, NULL, e->network ? : "");
		if (s)
			value->str = s;
		else
			r = FALSE;
	} else if (KEYWORD ("creator")) {
		char *s;

		s = _vbi3_export_strdup (e, NULL, e->creator);
		if (s)
			value->str = s;
		else
			r = FALSE;
	} else {
		const _vbi3_export_module *xc;

		xc = e->module;

		if (xc->option_get) {
			r = xc->option_get (e, keyword, value);
		} else {
			_vbi3_export_unknown_option (e, keyword);
			r = FALSE;
		}
	}

	return r;
}

/**
 * @param e Initialized vbi3_export object.
 * @param keyword Keyword identifying the option as in vbi3_option_info.
 * @param Varargs New value to set.
 *
 * Sets the value of the named option. Make sure the value is casted
 * to the correct type (int, double, char *).
 *
 * Typical usage of vbi3_export_option_set():
 * @code
 * vbi3_export_option_set (export, "quality", 75.5);
 * @endcode
 *
 * Mind that options of type @c VBI3_OPTION_MENU must be set by menu
 * entry number (int), all other options by value. If necessary it will
 * be replaced by the closest value possible. Use function
 * vbi3_export_option_menu_set() to set options with menu
 * by menu entry.
 *
 * @return
 * @c TRUE on success, otherwise @c FALSE and the option is not changed.
 */
vbi3_bool
vbi3_export_option_set		(vbi3_export *		e,
				 const char *		keyword,
				 ...)
{
	vbi3_bool r;
	va_list ap;

	assert (NULL != e);
	assert (NULL != keyword);

	reset_error (e);

	r = TRUE;

	va_start (ap, keyword);

	if (KEYWORD ("reveal")) {
		e->reveal = !!va_arg (ap, vbi3_bool);
	} else if (KEYWORD ("network")) {
		const char *network = va_arg (ap, const char *);

		if (!network || !network[0]) {
			if (e->network) {
				vbi3_free (e->network);
				e->network = NULL;
			}
		} else if (!_vbi3_export_strdup (e, &e->network, network)) {
			r = FALSE;
		}
	} else if (KEYWORD ("creator")) {
		const char *creator = va_arg (ap, const char *);

		if (!_vbi3_export_strdup (e, &e->creator, creator))
			r = FALSE;
	} else {
		const _vbi3_export_module *xc;

		xc = e->module;

		if (xc->option_set)
			r = xc->option_set (e, keyword, ap);
		else
			r = FALSE;
	}

	va_end (ap);

	return r;
}

/**
 * @param e Initialized vbi3_export object.
 * @param indx Index into the option table 0 ... n.
 *
 * Enumerates the options available for the given export module. You
 * should start at index 0, incrementing.
 *
 * @returns
 * Pointer to a vbi3_option_info structure,
 * @c NULL if @a indx is out of bounds.
 */
const vbi3_option_info *
vbi3_export_option_info_enum	(vbi3_export *		e,
				 unsigned int		indx)
{
	unsigned int size;

	assert (NULL != e);

	reset_error (e);

	size = N_ELEMENTS (generic_options) + e->module->option_info_size;

	if (indx >= size)
		return NULL;

	return e->local_option_info + indx;
}

/**
 * @param e Initialized vbi3_export object.
 * @param keyword Option identifier as in vbi3_option_info.
 *
 * Like vbi3_export_option_info_enum(), except this function tries to
 * find the option by keyword.
 *
 * @returns
 * Pointer to a vbi3_option_info structure,
 * @c NULL if the named option was not found.
 */
const vbi3_option_info *
vbi3_export_option_info_by_keyword
				(vbi3_export *		e,
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

	_vbi3_export_unknown_option (e, keyword);

	return NULL;
}

/**
 * @param indx Index into the export module list 0 ... n.
 *
 * Enumerates available export modules. You should start at index 0,
 * incrementing. Some modules may depend on machine features or the presence
 * of certain libraries, thus the list can vary from session to session.
 *
 * @return
 * Pointer to a vbi3_export_info structure,
 * @c NULL if the @a indx is out of bounds.
 */
const vbi3_export_info *
vbi3_export_info_enum		(unsigned int		indx)
{
	const _vbi3_export_module *xc;
	vbi3_export_info *xi;

	if (indx >= N_ELEMENTS (export_modules))
		return NULL;

	xc = export_modules[indx];
	xi = local_export_info + indx;

	xi->keyword	= xc->export_info->keyword;
	xi->label	= _(xc->export_info->label);
	xi->tooltip	= _(xc->export_info->tooltip);
	xi->mime_type	= xc->export_info->mime_type;
	xi->extension	= xc->export_info->extension;
	xi->open_format	= xc->export_info->open_format;

	return xi;
}

/**
 * @param keyword Export module identifier as in vbi3_export_info and
 *   vbi3_export_new().
 * 
 * Like vbi3_export_info_enum(), except this function tries to find
 * an export module by keyword.
 * 
 * @return
 * Pointer to a vbi3_export_info structure,
 * @c NULL if the named export module was not found.
 */
const vbi3_export_info *
vbi3_export_info_by_keyword	(const char *		keyword)
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
		const vbi3_export_info *xi;

		xi = export_modules[i]->export_info;

		if (0 == strncmp (keyword, xi->keyword, keylen)) {
			return vbi3_export_info_enum (i);
		}
	}

	return NULL;
}

/**
 * @param e vbi3_export object allocated with vbi3_export_new().
 *
 * Returns the export module info for the given @a export object.
 *
 * @return
 * Pointer to vbi3_export_info structure or
 * @c NULL if @a e is @c NULL.
 */
const vbi3_export_info *
vbi3_export_info_from_export	(const vbi3_export *	e)
{
	assert (NULL != e);

	return e->local_export_info;
}

static void
free_option_info		(vbi3_option_info *	oi,
				 unsigned int		oi_size)
{
	unsigned int i;

	for (i = 0; i < oi_size; ++i) {
		if (VBI3_OPTION_MENU == oi[i].type) {
			vbi3_free (oi[i].menu.str);
		}
	}

	vbi3_free (oi);
}

/**
 * @param e vbi3_export object allocated with vbi3_export_new().
 * 
 * Frees all resources associated with the export object.
 */
void
vbi3_export_delete		(vbi3_export *		e)
{
	const _vbi3_export_module *xc;

	if (!e)
		return;

	vbi3_free (e->errstr);
	vbi3_free (e->network);
	vbi3_free (e->creator);

 	xc = e->module;

	free_option_info (e->local_option_info,
			  N_ELEMENTS (generic_options) + xc->option_info_size);

	if (xc->_new && xc->_delete) {
		xc->_delete (e);
	} else {
		vbi3_free (e);
	}
}

static vbi3_option_info *
localize_option_info		(const vbi3_option_info *oi,
				 unsigned int		oi_size)
{
	vbi3_option_info *loi;
	unsigned int size;
	unsigned int i;

	size = (N_ELEMENTS (generic_options) + oi_size) * sizeof (*loi);

	if (!(loi = vbi3_malloc (size)))
		return NULL;

	memcpy (loi, generic_options,
		N_ELEMENTS (generic_options) * sizeof (*loi));

	memcpy (loi + N_ELEMENTS (generic_options), oi,
		oi_size * sizeof (*loi));

	oi_size += N_ELEMENTS (generic_options);

	for (i = 0; i < oi_size; ++i) {
		loi[i].label = _(loi[i].label);
		loi[i].tooltip = _(loi[i].tooltip);

		if (VBI3_OPTION_MENU == loi[i].type) {
			unsigned int j;
			const char **menu;

			size = loi[i].max.num + 1;

			if (!(menu = vbi3_malloc (size * sizeof (*menu)))) {
				free_option_info (loi, i);
				return NULL;
			}

			for (j = 0; j < size; ++j)
				menu[j] = _(loi[i].menu.str[j]);

			loi[i].menu.str = menu;
		}
	}

	return loi;
}

static void
reset_options			(vbi3_export *		e)
{
	const vbi3_option_info *oi;
	unsigned int i;

	for (i = 0; (oi = vbi3_export_option_info_enum (e, i)); ++i) {
		switch (oi->type) {
		case VBI3_OPTION_BOOL:
		case VBI3_OPTION_INT:
			if (oi->menu.num)
				vbi3_export_option_set
					(e, oi->keyword,
					 oi->menu.num[oi->def.num]);
			else
				vbi3_export_option_set
					(e, oi->keyword, oi->def.num);
			break;

		case VBI3_OPTION_REAL:
			if (oi->menu.dbl)
				vbi3_export_option_set
					(e, oi->keyword,
					 oi->menu.dbl[oi->def.num]);
			else
				vbi3_export_option_set
					(e, oi->keyword, oi->def.dbl);
			break;

		case VBI3_OPTION_STRING:
			if (oi->menu.str)
				vbi3_export_option_set
					(e, oi->keyword,
					 oi->menu.str[oi->def.num]);
			else
				vbi3_export_option_set
					(e, oi->keyword, oi->def.str);
			break;

		case VBI3_OPTION_MENU:
			vbi3_export_option_set (e, oi->keyword, oi->def.num);
			break;

		default:
			fprintf (stderr, "%s: unknown export option type %u\n",
				__FUNCTION__, oi->type);
			exit (EXIT_FAILURE);
		}
	}
}

static vbi3_bool
option_string			(vbi3_export *		e,
				 const char *		s2)
{
	const vbi3_option_info *oi;
	char *s;
	char *s1;
	char *keyword;
	char *string;
	char quote;
	vbi3_bool r = TRUE;

	if (!(s = s1 = _vbi3_export_strdup (e, NULL, s2)))
		return FALSE;

	do {
		while (isspace (*s))
			++s;

		if (',' == *s || ';' == *s) {
			++s;
			continue;
		}

		if (!*s) {
			vbi3_free (s1);
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
			_vbi3_export_error_printf
				(e, _("Invalid option string \"%s\"."), s2);
			break;
		}

		if (!(oi = vbi3_export_option_info_by_keyword (e, keyword)))
			break;

		switch (oi->type) {
		case VBI3_OPTION_BOOL:
		case VBI3_OPTION_INT:
		case VBI3_OPTION_MENU:
			r = vbi3_export_option_set
				(e, keyword, (int) strtol (s, &s, 0));
			break;

		case VBI3_OPTION_REAL:
			r = vbi3_export_option_set
				(e, keyword, (double) strtod (s, &s));
			break;

		case VBI3_OPTION_STRING:
			quote = 0;

			if ('\'' == *s || '\"' == *s) /* " */
				quote = *s++;
			string = s;

			while (*s && *s != quote
			       && (quote || (',' != *s && ';' != *s)))
				++s;
			if (*s)
				*s++ = 0;

			r = vbi3_export_option_set (e, keyword, string);

			break;

		default:
			fprintf (stderr, "%s: unknown export option type %d\n",
				 __FUNCTION__, oi->type);
			exit (EXIT_FAILURE);
		}
	} while (r);

	vbi3_free (s1);

	return FALSE;
}

/**
 * @param keyword Export module identifier as in vbi3_export_info.
 * @param errstr If not @c NULL this function stores a pointer to an error
 *   description here. You must free() this string when no longer needed.
 * 
 * Creates a new export module instance to export a vbi3_page in
 * the respective module format. As a special service you can
 * initialize options by appending to the @param keyword like this:
 * 
 * @code
 * vbi3_export_new ("keyword; quality=75.5, comment=\"example text\"");
 * @endcode
 * 
 * @return
 * Pointer to a newly allocated vbi3_export object which must be
 * freed by calling vbi3_export_delete(). @c NULL is returned and
 * the @a errstr may be set (else @a NULL) if some problem occurred.
 */
vbi3_export *
vbi3_export_new			(const char *		keyword,
				 char **		errstr)
{
	char key[256];
	const _vbi3_export_module *xc;
	vbi3_export *e;
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
		const vbi3_export_info *xi;

		xc = export_modules[i];
		xi = xc->export_info;

		if (0 == strncmp (keyword, xi->keyword, keylen))
			break;
	}

	if (i >= N_ELEMENTS (export_modules)) {
		if (errstr)
			asprintf (errstr,
				  _("Unknown export module '%s'."), key);
		return NULL;
	}

	if (!xc->_new) {
		if ((e = vbi3_malloc (sizeof (*e))))
			CLEAR (*e);
	} else {
		e = xc->_new (xc);
	}

	if (!e) {
		if (errstr)
			asprintf (errstr,
				  _("Cannot initialize export "
				    "module '%s', "
				    "probably lack of memory."),
				  xc->export_info->label ?
				  xc->export_info->label : keyword);
		return NULL;
	}

	e->module = xc;
	e->errstr = NULL;

	e->stream.start_timestamp = 0.0;
	e->stream.timestamp = 0.0;

	e->local_export_info = vbi3_export_info_enum (i);

	e->local_option_info =
		localize_option_info (xc->option_info,
				      xc->option_info_size);

	if (!e->local_option_info) {
		vbi3_free (e);
		if (errstr)
			asprintf (errstr,
				  _("Cannot initialize export module "
				    "'%s', out of memory."),
				  xc->export_info->label ?
				  xc->export_info->label : keyword);
		return NULL;
	}

	e->name = NULL;

	reset_options (e);

	if (keyword[keylen] && !option_string (e, keyword + keylen + 1)) {
		if (errstr)
			*errstr = strdup(vbi3_export_errstr(e));
		vbi3_export_delete(e);
		return NULL;
	}

	if (errstr)
		errstr = NULL;

	return e;
}
