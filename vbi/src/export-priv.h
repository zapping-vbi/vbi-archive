/*
 *  libzvbi - Export modules
 *
 *  Copyright (C) 2001-2004 Michael H. Schimek
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

/* $Id: export-priv.h,v 1.1.2.1 2004-04-09 05:17:20 mschimek Exp $ */

#ifndef EXPORT_PRIV_H
#define EXPORT_PRIV_H

#include "export.h"

typedef struct _vbi_export_module _vbi_export_module;

/**
 * @ingroup Exmod
 *
 * Structure representing an export module instance, part of the private
 * export module interface.
 *
 * Export modules can read, but do not normally write its fields, as
 * they are maintained by the public libzvbi export functions.
 */
struct _vbi_export {
	/**
	 * Points back to export module description.
	 */
	const _vbi_export_module *module;

	/**
	 * Use _vbi_export_write_error(),
	 * _vbi_export_unknown_option(),
	 * _vbi_export_invalid_option(),
	 * _vbi_export_error_printf().
	 */
	char *			errstr;

	/** Name of the file we are writing, @c NULL if anonymous. */
	const char *		name;

	/** File we are writing. */
	FILE *			fp;

	/** Generic option: Network name or @c NULL. */
	char *			network;
	/** Generic option: Creator name [by default "libzvbi"] or @c NULL. */
	char *			creator;
	/** Generic option: Reveal hidden characters. */
	vbi_bool		reveal;
	/** Generic option: Add page header. */
	vbi_bool		header;

	struct {
		/** Not implemented yet. */
		vbi_pgno		pgno;
		vbi_subno		subno;

		/** Timestamp. */
		double			timestamp;
	}			stream;

	/** See vbi_export_set_link_cb(). */
	vbi_export_link_cb *	link_callback;
	void *			link_user_data;

	/** See vbi_export_set_pdc_cb(). */
	vbi_export_pdc_cb *	pdc_callback;
	void *			pdc_user_data;

	/** @internal */
	const vbi_export_info *	local_export_info;
	vbi_option_info *	local_option_info;
};

/**
 * @ingroup Exmod
 *
 * Structure describing an export module, part of the private
 * export module interface. One required for each module.
 */
struct _vbi_export_module {
	const vbi_export_info *	export_info;

	vbi_export *		(* _new)(const _vbi_export_module *em);
	void			(* _delete)(vbi_export *e);

	const vbi_option_info *	option_info;
	unsigned int		option_info_size;

	vbi_bool		(* option_set)(vbi_export *e,
					       const char *keyword,
					       va_list);
	vbi_bool		(* option_get)(vbi_export *e,
					       const char *keyword,
					       vbi_option_value *value);
	vbi_bool		(* export)(vbi_export *e,
					   const vbi_page *pg);
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
extern void
_vbi_export_write_error		(vbi_export *		e);
extern void
_vbi_export_unknown_option	(vbi_export *		e,
				 const char *		keyword);
extern void
_vbi_export_invalid_option	(vbi_export *		e,
				 const char *		keyword,
				 ...);
extern void
_vbi_export_error_printf	(vbi_export *		e,
				 const char *		template,
				 ...);
extern char *
_vbi_export_strdup		(vbi_export *		e,
				 char **		d,
				 const char *		s);

/* Option info building */

/**
 * Helper macro for export modules to build option lists. Use like this:
 *
 * @code
 * vbi_option_info myinfo = VBI_OPTION_BOOL_INITIALIZER
 *   ("mute", N_("Switch sound on/off"), FALSE, N_("I am a tooltip"));
 * @endcode
 *
 * N_() marks the string for i18n, see info gettext for details.
 */
#define _VBI_OPTION_BOOL_INITIALIZER(key_, label_, def_, tip_)		\
  { VBI_OPTION_BOOL, key_, label_, { .num = def_ }, { .num = 0 },	\
  { .num = 1 }, { .num = 1 }, { .num = NULL }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like this:
 *
 * @code
 * vbi_option_info myinfo = VBI_OPTION_INT_RANGE_INITIALIZER
 *   ("sampling", N_("Sampling rate"), 44100, 8000, 48000, 100, NULL);
 * @endcode
 *
 * Here we have no tooltip (@c NULL).
 */
#define _VBI_OPTION_INT_RANGE_INITIALIZER(key_, label_, def_, min_,	\
  max_,	step_, tip_) { VBI_OPTION_INT, key_, label_,			\
  { .num = def_ }, { .num = min_ }, { .num = max_ }, { .num = step_ },	\
  { .num = NULL }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like this:
 *
 * @code
 * int mymenu[] = { 29, 30, 31 };
 *
 * vbi_option_info myinfo = VBI_OPTION_INT_MENU_INITIALIZER
 *   ("days", NULL, 1, mymenu, N_ELEMENTS (mymenu), NULL);
 * @endcode
 *
 * No label and tooltip (@c NULL), i. e. this option is not to be
 * listed in the user interface. Default is entry 1 ("30") of 3 entries. 
 */
#define _VBI_OPTION_INT_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, elements_, tip_) { VBI_OPTION_INT, key_, label_,		\
  { .num = def_}, { .num = 0 }, { .num = (elements_) - 1 },		\
  { .num = 1 }, { .num = menu_ }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like
 * VBI_OPTION_INT_RANGE_INITIALIZER(), just with doubles but ints.
 */
#define _VBI_OPTION_REAL_RANGE_INITIALIZER(key_, label_, def_, min_,	\
  max_, step_, tip_) { VBI_OPTION_REAL, key_, label_,			\
  { .dbl = def_ }, { .dbl = min_ }, { .dbl = max_ }, { .dbl = step_ },	\
  { .dbl = NULL }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like
 * VBI_OPTION_INT_MENU_INITIALIZER(), just with an array of doubles but ints.
 */
#define _VBI_OPTION_REAL_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, elements_, tip_) { VBI_OPTION_REAL, key_, label_,		\
  { .num = def_ }, { .num = 0 }, { .num = (elements_) - 1 },		\
  { .num = 1 }, { .dbl = menu_ }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like this:
 *
 * @code
 * vbi_option_info myinfo = VBI_OPTION_STRING_INITIALIZER
 *   ("comment", N_("Comment"), "bububaba", "Please enter a string");
 * @endcode
 */
#define _VBI_OPTION_STRING_INITIALIZER(key_, label_, def_, tip_)	\
  { VBI_OPTION_STRING, key_, label_, { .str = def_ }, { .num = 0 },	\
  { .num = 0 }, { .num = 0 }, { .str = 0 }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like this:
 *
 * @code
 * char *mymenu[] = { "txt", "html" };
 *
 * vbi_option_info myinfo = VBI_OPTION_STRING_MENU_INITIALIZER
 *   ("extension", "Ext", 0, mymenu, N_ELEMENTS (mymenu),
 *   N_("Select an extension"));
 * @endcode
 *
 * Remember this is like VBI_OPTION_STRING_INITIALIZER() in the sense
 * that the vbi client can pass any string as option value, not just those
 * proposed in the menu. In contrast a plain menu option as with
 * VBI_OPTION_MENU_INITIALIZER() expects menu indices as input.
 */
#define _VBI_OPTION_STRING_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, elements_, tip_) { VBI_OPTION_STRING, key_, label_,		\
  { .str = def_ }, { .num = 0 }, { .num = (elements_) - 1 },		\
  { .num = 1 }, { .str = menu_ }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like this:
 *
 * @code
 * char *mymenu [] = { N_("Monday"), N_("Tuesday") };
 *
 * vbi_option_info myinfo = VBI_OPTION_MENU_INITIALIZER
 *   ("weekday", "Weekday", 0, mymenu, N_ELEMENTS (mymenu),
 *   N_("Select a weekday"));
 * @endcode
 */
#define _VBI_OPTION_MENU_INITIALIZER(key_, label_, def_, menu_,		\
  elements_, tip_) { VBI_OPTION_MENU, key_, label_, { .num = def_ },	\
  { .num = 0 }, { .num = (elements_) - 1 }, { .num = 1 },		\
  { .str = menu_ }, tip_ }

/** @} */

#endif /* EXPORT_PRIV_H */
