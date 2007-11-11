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

/* $Id: export.c,v 1.13.2.17 2007-11-11 03:06:12 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <iconv.h>
#include <math.h>		/* fabs() */
#include <fcntl.h>		/* open() */
#include <unistd.h>		/* write(), close() */
#include <limits.h>		/* SSIZE_MAX */

#include "misc.h"
#ifdef ZAPPING8
#  include "common/intl-priv.h"
#else
#  include "version.h"
#  include "intl-priv.h"
#endif
#include "export-priv.h"
#include "conv.h"

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

/* Error functions. */

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

	if (0 != errno) {
		_vbi3_export_error_printf (e, "%s.", strerror (errno));
	} else {
		_vbi3_export_error_printf (e, _("Write error."));
	}
}

void
_vbi3_export_malloc_error	(vbi3_export *		e)
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
				STRACPY (buf, "NULL");
			else
				snprintf (buf, sizeof (buf) - 1, "'%s'", s);
			break;

		default:
			fprintf (stderr, "%s: unknown export option type %d\n",
				 __FUNCTION__, oi->type);
			STRACPY (buf, "?");
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

/* Misc. helper functions. */

const char *
_vbi3_export_codeset		(const char *		codeset)
{
	if (0 == strcmp (codeset, "locale")) {
		codeset = vbi3_locale_codeset ();
		if (NULL == codeset) {
			codeset = "ASCII";
		}
	}

	return codeset;
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


/* Output functions. */

/**
 * @internal
 * @param e Initialized vbi_export object.
 * @param min_space Space required in the output buffer in bytes.
 *
 * This function increases the capacity of the output buffer in the
 * vbi_export @a e structure to @a e->buffer.offset + min_space or
 * more. In other words, it ensures at least @a min_spaces bytes can
 * be written into the buffer at @a e->buffer.offset.
 *
 * Note on success this function may change the @a e->buffer.data
 * pointer as well as @a e->buffer.capacity.
 *
 * @returns
 * @c TRUE if the buffer capacity is already sufficient or was
 * successfully increased. @c FALSE if @a e->write_error is @c TRUE
 * or more memory could not be allocated. In this case @a e->buffer
 * remains unmodified.
 */
vbi3_bool
_vbi3_export_grow_buffer_space	(vbi3_export *		e,
				 size_t			min_space)
{
	const size_t element_size = sizeof (*e->buffer.data);
	size_t offset;
	size_t capacity;
	vbi3_bool success;

	assert (NULL != e);
	assert (0 != e->target);

	offset = e->buffer.offset;
	capacity = e->buffer.capacity;

	assert (offset <= capacity);

	if (unlikely (e->write_error))
		return FALSE;

	if (capacity >= min_space
	    && offset <= capacity - min_space)
		return TRUE;

	if (unlikely (offset > SIZE_MAX - min_space))
		goto failed;

	if (VBI3_EXPORT_TARGET_MEM == e->target) {
		char *old_data;

		/* Not enough buffer space. Change to TARGET_ALLOC
		   to calculate the actually needed amount. */

		old_data = e->buffer.data;

		e->target = VBI3_EXPORT_TARGET_ALLOC;
		e->_write = NULL;

		e->buffer.data = NULL;
		e->buffer.capacity = 0;

		success = _vbi3_grow_vector_capacity ((void **)
						     &e->buffer.data,
						     &e->buffer.capacity,
						     offset + min_space,
						     element_size);
		if (unlikely (!success))
			goto failed;

		/* Carry over the old data because the output may
		   fit after all. */
		memcpy (e->buffer.data, old_data, e->buffer.offset);

		return TRUE;
	} else {
		success = _vbi3_grow_vector_capacity ((void **)
						     &e->buffer.data,
						     &e->buffer.capacity,
						     offset + min_space,
						     element_size);
		if (likely (success))
			return TRUE;
	}

 failed:
	_vbi3_export_malloc_error (e);

	/* We do not set e->write_error here so this function can be
	   used to preallocate e->buffer.data prior to calling
	   output functions like vbi3_export_putc() without knowing
	   exactly how much memory is needed. */

	return FALSE;
}

static vbi3_bool
write_fp			(vbi3_export *		e,
				 const void *		src,
				 size_t			src_size)
{
	size_t actual;

	actual = fwrite (src, 1, src_size, e->_handle.fp);
	if (unlikely (actual != src_size)) {
		_vbi3_export_write_error (e);
		e->write_error = TRUE;
		return FALSE;
	}

	return TRUE;
}

static vbi3_bool
write_fd			(vbi3_export *		e,
				 const void *		src,
				 size_t			src_size)
{
	while (src_size > 0) {
		unsigned int retry;
		ssize_t actual;
		size_t count;

		count = src_size;
		if (unlikely (src_size > SSIZE_MAX))
			count = SSIZE_MAX & -4096;

		for (retry = 10;; --retry) {
			actual = write (e->_handle.fd, src, count);
			if (likely (actual == (ssize_t) count))
				break;

			if (0 != actual || 0 == retry) {
				_vbi3_export_write_error (e);
				e->write_error = TRUE;
				return FALSE;
			}
		}

		src = (const char *) src + actual;
		src_size -= actual;
	}

	return TRUE;
}

static vbi3_bool
fast_flush			(vbi3_export *		e)
{
	if (e->buffer.offset > 0) {
		vbi3_bool success;

		success = e->_write (e,
				     e->buffer.data,
				     e->buffer.offset);
		if (unlikely (!success)) {
			e->write_error = TRUE;
			return FALSE;
		}

		e->buffer.offset = 0;
	}

	return TRUE;
}

/**
 * @param e Initialized vbi3_export object.
 *
 * Writes the contents of the vbi3_export output buffer into the
 * target buffer or file. Only export modules and their callback
 * functions (e.g. vbi3_export_link_cb) may call this function.
 *
 * If earlier vbi3_export output functions failed, this function
 * does nothing and returns @c FALSE immediately.
 *
 * @returns
 * @c FALSE on write error. The buffer remains unmodified in
 * this case, but incomplete data may have been written into the
 * target file.
 *
 * @since 0.2.26
 */
vbi3_bool
vbi3_export_flush		(vbi3_export *		e)
{
	assert (NULL != e);
	assert (0 != e->target);

	if (unlikely (e->write_error))
		return FALSE;

	switch (e->target) {
	case VBI3_EXPORT_TARGET_MEM:
	case VBI3_EXPORT_TARGET_ALLOC:
		/* Nothing to do. */
		break;

	case VBI3_EXPORT_TARGET_FP:
	case VBI3_EXPORT_TARGET_FD:
	case VBI3_EXPORT_TARGET_FILE:
		return fast_flush (e);

	default:
		assert (0);
	}

	return TRUE;
}

/**
 * @param e Initialized vbi3_export object.
 * @param c Character (one byte) to be stored in the output buffer.
 *
 * If necessary this function increases the capacity of the vbi3_export
 * output buffer, then writes one character into it. Only export
 * modules and their callback functions (e.g. vbi3_export_link_cb) may
 * call this function.
 *
 * If earlier vbi3_export output functions failed, this function
 * does nothing and returns @c FALSE immediately.
 *
 * @returns
 * @c FALSE if the buffer capacity was insufficent and could not be
 * increased. The buffer remains unmodified in this case.
 *
 * @since 0.2.26
 */
vbi3_bool
vbi3_export_putc			(vbi3_export *		e,
				 int			c)
{
	size_t offset;

	assert (NULL != e);

	if (unlikely (!_vbi3_export_grow_buffer_space (e, 1))) {
		e->write_error = TRUE;
		return FALSE;
	}

	offset = e->buffer.offset;
	e->buffer.data[offset] = c;
	e->buffer.offset = offset + 1;

	return TRUE;
}

static vbi3_bool
fast_write			(vbi3_export *		e,
				 const void *		src,
				 size_t			src_size)
{
	if (unlikely (!fast_flush (e)))
		return FALSE;

	if (unlikely (!e->_write (e, src, src_size))) {
		e->write_error = TRUE;
		return FALSE;
	}

	return TRUE;
}

/**
 * @param e Initialized vbi3_export object.
 * @param src Data to be copied into the buffer.
 * @param src_size Number of bytes to be copied.
 *
 * If necessary this function increases the capacity of the vbi3_export
 * output buffer, then copies the data from the @a src buffer into the
 * output buffer. Only export modules and their callback functions
 * (e.g. vbi3_export_link_cb) may call this function.
 *
 * If earlier vbi3_export output function calls failed, this function
 * does nothing and returns @c FALSE immediately. If @a src_size is
 * large, this function may write the data directly into the target
 * file, with the same effects as vbi3_export_flush().
 *
 * @returns
 * @c FALSE if the buffer capacity was insufficent and could not be
 * increased, or if a write error occurred. The buffer remains
 * unmodified in this case, but incomplete data may have been written
 * into the target file.
 *
 * @since 0.2.26
 */
vbi3_bool
vbi3_export_write		(vbi3_export *		e,
				 const void *		src,
				 size_t			src_size)
{
	size_t offset;

	assert (NULL != e);
	assert (NULL != src);

	if (unlikely (e->write_error))
		return FALSE;

	switch (e->target) {
	case VBI3_EXPORT_TARGET_MEM:
	case VBI3_EXPORT_TARGET_ALLOC:
		/* Use buffered I/O. */
		break;

	case VBI3_EXPORT_TARGET_FP:
	case VBI3_EXPORT_TARGET_FD:
	case VBI3_EXPORT_TARGET_FILE:
		if (src_size >= 4096)
			return fast_write (e, src, src_size);
		break;

	default:
		assert (0);
	}

	if (unlikely (!_vbi3_export_grow_buffer_space (e, src_size))) {
		e->write_error = TRUE;
		return FALSE;
	}

	offset = e->buffer.offset;
	memcpy (e->buffer.data + offset, src, src_size);
	e->buffer.offset = offset + src_size;

	return TRUE;
}

/**
 * @param e Initialized vbi3_export object.
 * @param src NUL-terminated string to be copied into the buffer, can
 *   be @c NULL.
 *
 * If necessary this function increases the capacity of the vbi3_export
 * output buffer, then writes the string @a src into it. It does not write
 * the terminating NUL or a line feed character. Only export modules
 * and their callback functions (e.g. vbi3_export_link_cb) may call this
 * function.
 *
 * If earlier vbi3_export output function calls failed, this function
 * does nothing and returns @c FALSE immediately. If the string is
 * very long this function may write the data directly into the target
 * file, with the same effects as vbi3_export_flush().
 *
 * @returns
 * @c FALSE if the buffer capacity was insufficent and could not be
 * increased, or if a write error occurred. The buffer remains
 * unmodified in this case, but incomplete data may have been written
 * into the target file.
 *
 * @since 0.2.26
 */
vbi3_bool
vbi3_export_puts			(vbi3_export *		e,
				 const char *		src)
{
	assert (NULL != e);

	if (unlikely (e->write_error))
		return FALSE;

	if (NULL == src)
		return TRUE;

	return vbi3_export_write (e, src, strlen (src));
}

/**
 * @param e Initialized vbi3_export object.
 * @param dst_codeset Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL the default is UTF-8.
 * @param src_codeset Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL the default is UTF-8.
 * @param src Source buffer, can be @c NULL.
 * @param src_size Number of bytes in the source string (excluding
 *   the terminating NUL, if any).
 * @param repl_char UCS-2 replacement for characters which are not
 *   representable in @a dst_codeset. When zero the function will
 *   fail if the source buffer contains unrepresentable characters.
 *
 * If necessary this function increases the capacity of the vbi3_export
 * output buffer, then converts the string with iconv() and writes the
 * result into the buffer. Only export modules and their callback
 * functions (e.g. vbi3_export_link_cb) may call this function.
 *
 * If earlier vbi3_export output function calls failed, this function
 * does nothing and returns @c FALSE immediately. If the string is
 * very long this function may write the data directly into the target
 * file, with the same effects as vbi3_export_flush().
 *
 * @returns
 * @c FALSE if the conversion failed, if the buffer capacity was
 * insufficent and could not be increased, or if a write error occurred.
 * The buffer remains unmodified in this case, but incomplete data may
 * have been written into the target file.
 *
 * @since 0.2.26
 */
vbi3_bool
vbi3_export_puts_iconv		(vbi3_export *		e,
				 const char *		dst_codeset,
				 const char *		src_codeset,
				 const char *		src,
				 unsigned long		src_size,
				 int			repl_char)
{
	char *buffer;
	unsigned long out_size;
	vbi3_bool success;

	assert (NULL != e);

	if (unlikely (e->write_error))
		return FALSE;

	/* Inefficient, but shall suffice for now. */
	buffer = _vbi3_strndup_iconv (&out_size,
				     dst_codeset, src_codeset,
				     src, src_size, repl_char);
	if (unlikely (NULL == buffer)) {
		_vbi3_export_malloc_error (e);
		e->write_error = TRUE;
		return FALSE;
	}

	assert (sizeof (size_t) >= sizeof (out_size));

	success = vbi3_export_write (e, buffer, out_size);

	vbi3_free (buffer);

	return success;
}

/**
 * @param e Initialized vbi3_export object.
 * @param dst_codeset Character set name for iconv() conversion,
 *   for example "ISO-8859-1". When @c NULL the default is UTF-8.
 * @param src Source string in UCS-2 format, can be @c NULL.
 * @param src_length Number of characters (not bytes) in the source
 *   string. Can be -1 if the string is NUL terminated.
 * @param repl_char UCS-2 replacement for characters which are not
 *   representable in @a dst_codeset. When zero the function will
 *   fail if the source buffer contains unrepresentable characters.
 *
 * If necessary this function increases the capacity of the vbi3_export
 * output buffer, then converts the string with iconv() and writes the
 * result into the buffer. Only export modules and their callback
 * functions (e.g. vbi3_export_link_cb) may call this function.
 *
 * If earlier vbi3_export output functions failed, this function
 * does nothing and returns @c FALSE immediately. If the string is
 * very long this function may write the data directly into the target
 * file, with the same effects as vbi3_export_flush().
 *
 * @returns
 * @c FALSE if the conversion failed, if the buffer capacity was
 * insufficent and could not be increased, or if a write error occurred.
 * The buffer remains unmodified in this case, but incomplete data may
 * have been written into the target file.
 *
 * @since 0.2.26
 */
vbi3_bool
vbi3_export_puts_iconv_ucs2	(vbi3_export *		e,
				 const char *		dst_codeset,
				 const uint16_t *	src,
				 long			src_length,
				 int			repl_char)
{
	assert (NULL != e);

	if (unlikely (e->write_error))
		return FALSE;

	if (NULL == src)
		return TRUE;

	if (src_length < 0)
		src_length = vbi3_strlen_ucs2 (src);

	return vbi3_export_puts_iconv (e, dst_codeset, "UCS-2",
				      (const char *) src, src_length * 2,
				      repl_char);
}

/**
 * @param e Initialized vbi3_export object.
 * @param templ printf-like output template.
 * @param ap Arguments pointer.
 *
 * If necessary this function increases the capacity of the vbi3_export
 * output buffer, then formats a string as vprintf() does and writes
 * it into the buffer. Only export modules and their callback functions
 * (e.g. vbi3_export_link_cb) may call this function.
 *
 * If earlier vbi3_export output functions failed, this function
 * does nothing and returns @c FALSE immediately. If the export target
 * is a file, this function may write the data directly into the file,
 * with the same effects as vbi3_export_flush().
 *
 * @returns
 * @c FALSE if the buffer capacity was insufficent and could not be
 * increased, or if a write error occurred.
 *
 * @since 0.2.26
 */
vbi3_bool
vbi3_export_vprintf		(vbi3_export *		e,
				 const char *		templ,
				 va_list		ap)
{
	size_t offset;
	unsigned int i;

	assert (NULL != e);
	assert (NULL != templ);
	assert (0 != e->target);

	if (unlikely (e->write_error))
		return FALSE;

	if (VBI3_EXPORT_TARGET_FP == e->target) {
		if (unlikely (!fast_flush (e)))
			return FALSE;

		if (unlikely (vfprintf (e->_handle.fp, templ, ap) < 0)) {
			e->write_error = TRUE;
			return FALSE;
		}

		return TRUE;
	}

	offset = e->buffer.offset;

	for (i = 0;; ++i) {
		size_t avail = e->buffer.capacity - offset;
		int len;

		len = vsnprintf (e->buffer.data + offset,
				 avail, templ, ap);
		if (len < 0) {
			/* avail is not enough. */

			if (unlikely (avail >= (1 << 16)))
				break; /* now that's ridiculous */

			/* Note 256 is the minimum free space we want
			   but the buffer actually grows by a factor
			   two in each iteration. */
			if (!_vbi3_export_grow_buffer_space (e, 256))
				goto failed;
		} else if ((size_t) len < avail) {
			e->buffer.offset = offset + len;
			return TRUE;
		} else {
			/* Plus one because the buffer must also hold
			   a terminating NUL, although we don't need it. */
			size_t needed = (size_t) len + 1;

			if (unlikely (i > 0))
				break; /* again? */

			if (!_vbi3_export_grow_buffer_space (e, needed))
				goto failed;
		}
	}

	_vbi3_export_malloc_error (e);

 failed:
	e->write_error = TRUE;

	return FALSE;
}

/**
 * @param e Initialized vbi3_export object.
 * @param templ printf-like output template.
 * @param ... Arguments.
 *
 * If necessary this function increases the capacity of the vbi3_export
 * output buffer, then formats a string as printf() does and writes
 * it into the buffer. Only export modules and their callback functions
 * (e.g. vbi3_export_link_cb) may call this function.
 *
 * If earlier vbi3_export output functions failed, this function
 * does nothing and returns @c FALSE immediately. If the export target
 * is a file, this function may write the data directly into the file,
 * with the same effects as vbi3_export_flush().
 *
 * @returns
 * @c FALSE if the buffer capacity was insufficent and could not be
 * increased, or if a write error occurred.
 *
 * @since 0.2.26
 */
vbi3_bool
vbi3_export_printf		(vbi3_export *		e,
				 const char *		templ,
				 ...)
{
	va_list ap;
	vbi3_bool success;

	va_start (ap, templ);

	success = vbi3_export_vprintf (e, templ, ap);

	va_end (ap);

	return success;
}


/**
 * @param e Initialized vbi3_export object.
 * @param buffer Output buffer.
 * @param buffer_size Size of the output buffer in bytes.
 * @param pg Page to be exported.
 * 
 * This function writes the @a pg contents, converted to the respective
 * export module format, into the @a buffer.
 *
 * You can call this function as many times as you want, it does not
 * change vbi3_export state or the vbi3_page.
 * 
 * @return
 * On success the function returns the actual number of bytes stored in
 * the buffer. If @a buffer_size is too small it returns the required
 * size. On other errors it returns -1. The buffer contents are undefined
 * when the function failed.
 *
 * @since 0.2.26
 */
ssize_t
vbi3_export_mem			(vbi3_export *		e,
				 void *			buffer,
				 size_t			buffer_size,
				 const vbi3_page *	pg)
{
	ssize_t actual;

	assert (NULL != e);

	reset_error (e);

	e->target = VBI3_EXPORT_TARGET_MEM;
	e->_write = NULL;

	if (NULL == buffer)
		buffer_size = 0;

	e->buffer.data = buffer;
	e->buffer.offset = 0;
	e->buffer.capacity = buffer_size;

	e->write_error = FALSE;

	if (e->module->export (e, pg)) {
		if (VBI3_EXPORT_TARGET_ALLOC == e->target) {
			/* buffer_size was not enough, return the
			   actual size needed. */

			/* Or was it? We may have started to write into
			   @a buffer, so let's finish that in any case. */
			memcpy (buffer, e->buffer.data,
				MIN (e->buffer.offset, buffer_size));

			free (e->buffer.data);
		}

		if (unlikely (e->buffer.offset > (size_t) SSIZE_MAX)) {
			errno = EOVERFLOW;
			actual = -1; /* failed */
		} else {
			actual = e->buffer.offset;
		}
	} else {
		if (VBI3_EXPORT_TARGET_ALLOC == e->target)
			free (e->buffer.data);

		actual = -1; /* failed */
	}

	CLEAR (e->buffer);

	e->target = 0;

	return actual;
}

/**
 * @param e Initialized vbi3_export object.
 * @param buffer The address of the output buffer will be stored here.
 *    Can be @c NULL.
 * @param buffer_size The amount of data stored in the output buffer,
 *    in bytes, will be stored here. @a buffer_size can be @c NULL.
 * @param pg Page to be exported.
 * 
 * This function writes the @a pg contents, converted to the respective
 * export module format, into a newly allocated buffer. You must free()
 * this buffer when it is no longer needed.
 *
 * You can call this function as many times as you want, it does not
 * change vbi3_export state or the vbi3_page.
 * 
 * @returns
 * @c NULL on failure, and @a buffer and @a buffer_size remain
 * unmodified. The address of the allocated buffer on success.
 *
 * @since 0.2.26
 */
void *
vbi3_export_alloc		(vbi3_export *		e,
				 void **		buffer,
				 size_t *		buffer_size,
				 const vbi3_page *	pg)
{
	void *result;

	assert (NULL != e);

	reset_error (e);

	e->target = VBI3_EXPORT_TARGET_ALLOC;
	e->_write = NULL;

	CLEAR (e->buffer);

	e->write_error = FALSE;

	if (e->module->export (e, pg)) {
		void *data = e->buffer.data;
		size_t offset = e->buffer.offset;

		/* Let's not waste space. */
		if (e->buffer.capacity - offset >= 256) {
			data = realloc (data, offset);
			if (NULL == data)
				data = e->buffer.data;
		}

		if (NULL != buffer)
			*buffer = data;
		if (NULL != buffer_size)
			*buffer_size = offset;

		result = data;
	} else {
		free (e->buffer.data); /* if any */

		result = NULL;
	}

	CLEAR (e->buffer);

	e->target = 0;

	return result;
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
 * @c FALSE on failure, @c TRUE on success.
 */
vbi3_bool
vbi3_export_stdio		(vbi3_export *		e,
				 FILE *			fp,
				 const vbi3_page *	pg)
{
	vbi3_bool success;
	int saved_errno;

	if (NULL == e || NULL == fp || NULL == pg)
		return FALSE;

	reset_error (e);

	e->target = VBI3_EXPORT_TARGET_FP;
	e->_write = write_fp;

	e->_handle.fp = fp;
	clearerr (fp);

	CLEAR (e->buffer);

	e->write_error = FALSE;

	success = e->module->export (e, pg);

	if (success)
		success = vbi3_export_flush (e);

	saved_errno = errno;

	free (e->buffer.data);
	CLEAR (e->buffer);

	memset (&e->_handle, -1, sizeof (e->_handle));

	e->_write = NULL;
	e->target = 0;

	errno = saved_errno;

	return success;
}

static int
xclose				(int			fd)
{
	unsigned int retry = 10;

	do {
		if (likely (0 == close (fd)))
			return 0;
		if (EINTR != errno)
			break;
	} while (--retry > 0);

	return -1;
}

static int
xopen				(const char *		name,
				 int			flags,
				 mode_t			mode)
{
	unsigned int retry = 10;

	do {
		int fd = open (name, flags, mode);

		if (likely (fd >= 0))
			return fd;
		if (EINTR != errno)
			break;
	} while (--retry > 0);

	return -1;
}

/**
 * @param e Initialized vbi3_export object.
 * @param name File to be created.
 * @param pg Page to be exported.
 * 
 * Writes the @a pg contents, converted to the respective
 * export format, into a new file of the given @a name. When an error
 * occured after the file was opened, the function deletes the file.
 * 
 * You can call this function as many times as you want, it does not
 * change vbi3_export state or the vbi3_page.
 * 
 * @returns
 * @c FALSE on failure, @c TRUE on success.
 */
vbi3_bool
vbi3_export_file			(vbi3_export *		e,
				 const char *		name,
				 const vbi3_page *	pg)
{
	vbi3_bool success;
	int saved_errno;

	if (NULL == e || NULL == name || NULL == pg)
		return FALSE;

	reset_error (e);

	/* For error messages. */
	e->file_name = name;

	e->target = VBI3_EXPORT_TARGET_FILE;
	e->_write = write_fd;

	e->_handle.fd = xopen (name,
			       O_WRONLY | O_CREAT | O_TRUNC,
			       (S_IRUSR | S_IWUSR |
				S_IRGRP | S_IWGRP |
				S_IROTH | S_IWOTH));
	if (-1 == e->_handle.fd) {
		_vbi3_export_error_printf
			(e, _("Cannot create file '%s': %s."),
			 name, strerror (errno));
		return FALSE;
	}

	CLEAR (e->buffer);

	e->write_error = FALSE;

	success = e->module->export (e, pg);

	if (success)
		success = vbi3_export_flush (e);

	saved_errno = errno;

	free (e->buffer.data);
	CLEAR (e->buffer);

	if (!success) {
		struct stat st;

		/* There might be a race if we attempt to delete the
		   file after closing it, so we mark it for deletion
		   here or leave it alone when close() fails. Also
		   delete only if @a name is regular file. */
		if (0 == stat (name, &st)
		    && S_ISREG (st.st_mode))
			unlink (name);
	}

	if (-1 == xclose (e->_handle.fd)) {
		if (success) {
			saved_errno = errno;
			_vbi3_export_write_error (e);
			success = FALSE;
		}
	}

	memset (&e->_handle, -1, sizeof (e->_handle));

	e->_write = NULL;
	e->target = 0;

	e->file_name = NULL;

	errno = saved_errno;

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

	e->file_name = NULL;

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

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
