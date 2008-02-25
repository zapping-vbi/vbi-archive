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

/* $Id: export-priv.h,v 1.1.2.5 2008-02-25 21:00:40 mschimek Exp $ */

#ifndef EXPORT_PRIV_H
#define EXPORT_PRIV_H

#include "export.h"

typedef struct _vbi3_export_module _vbi3_export_module;

/** The export target. */
enum _vbi3_export_target {
	/** Exporting to a client supplied buffer in memory. */
	VBI3_EXPORT_TARGET_MEM = 1,

	/** Exporting to a newly allocated buffer. */
	VBI3_EXPORT_TARGET_ALLOC,

	/** Exporting to a client supplied file pointer. */
	VBI3_EXPORT_TARGET_FP,

	/** Exporting to a client supplied file descriptor. */
	VBI3_EXPORT_TARGET_FD,

	/** Exporting to a file. */
	VBI3_EXPORT_TARGET_FILE,
};

typedef vbi3_bool
_vbi3_export_write_fn		(vbi3_export *		e,
				 const void *		s,
				 size_t			n_bytes);

/**
 * @ingroup Exmod
 *
 * Structure representing an export module instance, part of the private
 * export module interface.
 *
 * Export modules can read, but do not normally write its fields, as
 * they are maintained by the public libzvbi export functions.
 */
struct _vbi3_export {
	/**
	 * Points back to export module description.
	 */
	const _vbi3_export_module *module;

	/**
	 * Use _vbi3_export_write_error(),
	 * _vbi3_export_unknown_option(),
	 * _vbi3_export_invalid_option(),
	 * _vbi3_export_error_printf().
	 */
	char *			errstr;

	/**
	 * If @c target is @c VBI3_EXPORT_FILE the name of the file
	 * we are writing as supplied by the client. Otherwise @c NULL.
	 * This is intended for debugging and error messages.
	 */
	const char *		file_name;

	/** Generic option: Network name or @c NULL. */
	char *			network;
	/** Generic option: Creator name [by default "libzvbi"] or @c NULL. */
	char *			creator;
	/** Generic option: Reveal hidden characters. */
	vbi3_bool		reveal;
	/** Generic option: Add page header. */
	vbi3_bool		header;

	struct {
		/** Not implemented yet. */
		vbi3_pgno		pgno;
		vbi3_subno		subno;

		double			start_timestamp;
		double			timestamp;

		vbi3_bool		have_timestamp;
	}			stream;

	/** See vbi3_export_set_link_cb(). */
	vbi3_export_link_cb *	link_callback;
	void *			link_user_data;

	/** See vbi3_export_set_pdc_cb(). */
	vbi3_export_pdc_cb *	pdc_callback;
	void *			pdc_user_data;

	/**
	 * The export target. Note _vbi_export_grow_buffer_space() may
	 * change the target from TARGET_MEM to TARGET_ALLOC if the
	 * buffer supplied by the application is too small.
	 */
	enum _vbi3_export_target	target;

	/**
	 * If @a target is @c VBI_EXPORT_TARGET_FP or
	 * @c VBI_EXPORT_TARGET_FD the file pointer or file descriptor
	 * supplied by the client. If @c VBI_EXPORT_TARGET_FILE the
	 * file descriptor of the file we opened. Otherwise undefined.
	 *
	 * Private field. Not to be accessed by export modules.
	 */
	union {
		FILE *			fp;
		int			fd;
	}			_handle;

	/**
	 * Function to write data into @a _handle.
	 *
	 * Private field. Not to be accessed by export modules.
	 */
	_vbi3_export_write_fn *	_write;

	/**
	 * Output buffer. Export modules can write into this buffer
	 * directly after ensuring sufficient capacity, and/or call
	 * the vbi_export_putc() etc functions. Keep in mind these
	 * functions may call realloc(), changing the @a data pointer,
	 * and/or vbi_export_flush(), changing the @a offset.
	 */
	struct {
		/**
		 * Pointer to the start of the buffer in memory.
		 * @c NULL if @a capacity is zero.
		 */
		char *			data;

		/**
		 * The number of bytes written into the buffer
		 * so far. Must be <= @c capacity.
		 */
		size_t			offset;

		/**
		 * Number of bytes we can store in the buffer, may be
		 * zero.
		 *
		 * Call _vbi_export_grow_buffer_space() to increase the
		 * capacity. Keep in mind this may change the @a data
		 * pointer.
		 */
		size_t			capacity;
	}			buffer;

	/** A write error occurred (use like ferror()). */
	vbi3_bool		write_error;

	/** @internal */
	const vbi3_export_info *	local_export_info;
	vbi3_option_info *	local_option_info;
};

/**
 * @ingroup Exmod
 *
 * Structure describing an export module, part of the private
 * export module interface. One required for each module.
 */
struct _vbi3_export_module {
	const vbi3_export_info *	export_info;

	vbi3_export *		(* _new)(const _vbi3_export_module *em);
	void			(* _delete)(vbi3_export *e);

	const vbi3_option_info *	option_info;
	unsigned int		option_info_size;

	vbi3_bool		(* option_set)(vbi3_export *e,
					       const char *keyword,
					       va_list);
	vbi3_bool		(* option_get)(vbi3_export *e,
					       const char *keyword,
					       vbi3_option_value *value);
	vbi3_bool		(* export)(vbi3_export *e,
					   const vbi3_page *pg);
};

/**
 * @example exp-templ.c
 * @ingroup Exmod
 *
 * Template for internal export module.
 */

/*
	Helper functions
 */

/**
 * @addtogroup Exmod
 * @{
 */

/* Error functions. */

extern void
_vbi3_export_error_printf	(vbi3_export *		e,
				 const char *		templ,
				 ...)
  _vbi3_nonnull ((1, 2)) _vbi3_format ((printf, 2, 3));
extern void
_vbi3_export_write_error	(vbi3_export *		e)
  _vbi3_nonnull ((1));
extern void
_vbi3_export_malloc_error	(vbi3_export *		e)
  _vbi3_nonnull ((1));
extern void
_vbi3_export_unknown_option	(vbi3_export *		e,
				 const char *		keyword)
  _vbi3_nonnull ((1));
extern void
_vbi3_export_invalid_option	(vbi3_export *		e,
				 const char *		keyword,
				 ...)
  _vbi3_nonnull ((1));

/* Output functions. */

extern vbi3_bool
_vbi3_export_grow_buffer_space	(vbi3_export *		e,
				 size_t			min_space)
  _vbi3_nonnull ((1));

/* Misc. helper functions. */

extern char *
_vbi3_export_strdup		(vbi3_export *		e,
				 char **		d,
				 const char *		s)
  _vbi3_nonnull ((1, 3)); /* sic */
extern const char *
_vbi3_export_codeset		(const char *		codeset);

/* Option info building */

/**
 * Helper macro for export modules to build option lists. Use like this:
 *
 * @code
 * vbi3_option_info myinfo = VBI3_OPTION_BOOL_INITIALIZER
 *   ("mute", N_("Switch sound on/off"), FALSE, N_("I am a tooltip"));
 * @endcode
 *
 * N_() marks the string for i18n, see info gettext for details.
 */
#define _VBI3_OPTION_BOOL_INITIALIZER(key_, label_, def_, tip_)		\
  { VBI3_OPTION_BOOL, key_, label_, { .num = def_ }, { .num = 0 },	\
  { .num = 1 }, { .num = 1 }, { .num = NULL }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like this:
 *
 * @code
 * vbi3_option_info myinfo = VBI3_OPTION_INT_RANGE_INITIALIZER
 *   ("sampling", N_("Sampling rate"), 44100, 8000, 48000, 100, NULL);
 * @endcode
 *
 * Here we have no tooltip (@c NULL).
 */
#define _VBI3_OPTION_INT_RANGE_INITIALIZER(key_, label_, def_, min_,	\
  max_,	step_, tip_) { VBI3_OPTION_INT, key_, label_,			\
  { .num = def_ }, { .num = min_ }, { .num = max_ }, { .num = step_ },	\
  { .num = NULL }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like this:
 *
 * @code
 * int mymenu[] = { 29, 30, 31 };
 *
 * vbi3_option_info myinfo = VBI3_OPTION_INT_MENU_INITIALIZER
 *   ("days", NULL, 1, mymenu, N_ELEMENTS (mymenu), NULL);
 * @endcode
 *
 * No label and tooltip (@c NULL), i. e. this option is not to be
 * listed in the user interface. Default is entry 1 ("30") of 3 entries. 
 */
#define _VBI3_OPTION_INT_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, elements_, tip_) { VBI3_OPTION_INT, key_, label_,		\
  { .num = def_}, { .num = 0 }, { .num = (elements_) - 1 },		\
  { .num = 1 }, { .num = menu_ }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like
 * VBI3_OPTION_INT_RANGE_INITIALIZER(), just with doubles but ints.
 */
#define _VBI3_OPTION_REAL_RANGE_INITIALIZER(key_, label_, def_, min_,	\
  max_, step_, tip_) { VBI3_OPTION_REAL, key_, label_,			\
  { .dbl = def_ }, { .dbl = min_ }, { .dbl = max_ }, { .dbl = step_ },	\
  { .dbl = NULL }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like
 * VBI3_OPTION_INT_MENU_INITIALIZER(), just with an array of doubles but ints.
 */
#define _VBI3_OPTION_REAL_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, elements_, tip_) { VBI3_OPTION_REAL, key_, label_,		\
  { .num = def_ }, { .num = 0 }, { .num = (elements_) - 1 },		\
  { .num = 1 }, { .dbl = menu_ }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like this:
 *
 * @code
 * vbi3_option_info myinfo = VBI3_OPTION_STRING_INITIALIZER
 *   ("comment", N_("Comment"), "bububaba", "Please enter a string");
 * @endcode
 */
#define _VBI3_OPTION_STRING_INITIALIZER(key_, label_, def_, tip_)	\
  { VBI3_OPTION_STRING, key_, label_, { .str = def_ }, { .num = 0 },	\
  { .num = 0 }, { .num = 0 }, { .str = 0 }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like this:
 *
 * @code
 * char *mymenu[] = { "txt", "html" };
 *
 * vbi3_option_info myinfo = VBI3_OPTION_STRING_MENU_INITIALIZER
 *   ("extension", "Ext", 0, mymenu, N_ELEMENTS (mymenu),
 *   N_("Select an extension"));
 * @endcode
 *
 * Remember this is like VBI3_OPTION_STRING_INITIALIZER() in the sense
 * that the vbi client can pass any string as option value, not just those
 * proposed in the menu. In contrast a plain menu option as with
 * VBI3_OPTION_MENU_INITIALIZER() expects menu indices as input.
 */
#define _VBI3_OPTION_STRING_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, elements_, tip_) { VBI3_OPTION_STRING, key_, label_,		\
  { .str = def_ }, { .num = 0 }, { .num = (elements_) - 1 },		\
  { .num = 1 }, { .str = menu_ }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like this:
 *
 * @code
 * char *mymenu [] = { N_("Monday"), N_("Tuesday") };
 *
 * vbi3_option_info myinfo = VBI3_OPTION_MENU_INITIALIZER
 *   ("weekday", "Weekday", 0, mymenu, N_ELEMENTS (mymenu),
 *   N_("Select a weekday"));
 * @endcode
 */
#define _VBI3_OPTION_MENU_INITIALIZER(key_, label_, def_, menu_,		\
  elements_, tip_) { VBI3_OPTION_MENU, key_, label_, { .num = def_ },	\
  { .num = 0 }, { .num = (elements_) - 1 }, { .num = 1 },		\
  { .str = menu_ }, tip_ }

/** @} */

#endif /* EXPORT_PRIV_H */

/*
Local variables:
c-set-style: K&R
c-basic-offset: 8
End:
*/
