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

/* $Id: export.h,v 1.8.2.3 2004-02-25 17:34:33 mschimek Exp $ */

#ifndef EXPORT_H
#define EXPORT_H

#include "bcd.h" /* vbi_bool */
#include "event.h" /* vbi_network */
#include "format.h" /* vbi_page */
#include "pdc.h"
#include "conv.h"

/* Public */

#include <stdio.h> /* FILE */

/**
 * @ingroup Export
 * @brief Export module instance, an opaque object.
 *
 * Allocate with vbi_export_new().
 */
typedef struct vbi_export vbi_export;

/**
 * @ingroup Export
 * @brief Information about an export module.
 *
 * Although export modules can be accessed by a static keyword (see
 * vbi_export_new()) they are by definition opaque. The client
 * can list export modules for the user and manipulate them without knowing
 * about their availability or purpose. To do so, information
 * about the module is necessary, given in this structure.
 *
 * You can obtain this information with vbi_export_info_enum().
 */
typedef struct vbi_export_info {
	/**
	 * Unique (within this library) keyword identifying
	 * this export module, a NUL terminated ASCII string.
	 */
	const char *		keyword;
	/**
	 * Localized name of the export module for the user interface. Can
	 * be @c NULL if the option is not supposed to be listed in the UI.
	 */
	const char *		label;
	/**
	 * A localized description of the option for the user,
	 * can be @c NULL.
	 */
	const char *		tooltip;
	/**
	 * Description of the export format as MIME type,
	 * a NUL terminated ASCII string, for example "text/html".
	 * Can be @c NULL if no MIME type applicable.
	 */
	const char *		mime_type;
	/**
	 * Suggested filename extension, a NUL terminated ASCII string.
	 * Multiple strings are possible, separated by comma. The first
	 * string is preferred. Example: "html,htm". Can be @c NULL.
	 */
	const char *		extension;
} vbi_export_info;

/**
 * @ingroup Export
 */
typedef enum {
	/**
	 * A boolean value, either @c TRUE (1) or @c FALSE (0).
	 * <table>
	 * <tr><td>Type:</td><td>int</td></tr>
	 * <tr><td>Default:</td><td>def.num</td></tr>
	 * <tr><td>Bounds:</td><td>min.num (0) ... max.num (1),
	 * step.num (1)</td></tr>
	 * <tr><td>Menu:</td><td>%NULL</td></tr>
	 * </table>
	 */
	VBI_OPTION_BOOL = 1,

	/**
	 * A signed integer value. When only a few discrete values rather than
	 * a range are permitted @p menu points to a vector of integers. Note the
	 * option is still set by value, not by menu index. Setting the value may
	 * fail, or it may be replaced by the closest possible.
	 * <table>
	 * <tr><td>Type:</td><td>int</td></tr>
	 * <tr><td>Default:</td><td>def.num or menu.num[def.num]</td></tr>
	 * <tr><td>Bounds:</td><td>min.num ... max.num, step.num or menu</td></tr>
	 * <tr><td>Menu:</td><td>%NULL or menu.num[min.num ... max.num],
	 * step.num (1)</td></tr>
	 * </table>
	 */
	VBI_OPTION_INT,

	/**
	 * A real value, optional a vector of suggested values.
	 * <table>
	 * <tr><td>Type:</td><td>double</td></tr>
	 * <tr><td>Default:</td><td>def.dbl or menu.dbl[def.num]</td></tr>
	 * <tr><td>Bounds:</td><td>min.dbl ... max.dbl,
	 * step.dbl or menu</td></tr>
	 * <tr><td>Menu:</td><td>%NULL or menu.dbl[min.num ... max.num],
	 * step.num (1)</td></tr>
	 * </table>
	 */
	VBI_OPTION_REAL,

	/**
	 * A null terminated string. Note the menu version differs from
	 * VBI_OPTION_MENU in its argument, which is the string itself. For example:
	 * @code
	 * menu.str[0] = "red"
	 * menu.str[1] = "blue"
	 * ... and the option may accept other color strings not explicitely listed
	 * @endcode
	 * <table>
	 * <tr><td>Type:</td><td>char *</td></tr>
	 * <tr><td>Default:</td><td>def.str or menu.str[def.num]</td></tr>
	 * <tr><td>Bounds:</td><td>not applicable</td></tr>
	 * <tr><td>Menu:</td><td>%NULL or menu.str[min.num ... max.num],
	 * step.num (1)</td></tr>
	 * </table>
	 */
	VBI_OPTION_STRING,

	/**
	 * Choice between a number of named options. For example:
	 * @code
	 * menu.str[0] = "up"
	 * menu.str[1] = "down"
	 * menu.str[2] = "strange"
	 * @endcode
	 * <table>
	 * <tr><td>Type:</td><td>int</td></tr>
	 * <tr><td>Default:</td><td>def.num</td></tr>
	 * <tr><td>Bounds:</td><td>min.num (0) ... max.num, 
	 *    step.num (1)</td></tr>
	 * <tr><td>Menu:</td><td>menu.str[min.num ... max.num],
	 *    step.num (1).
	 * The menu strings are nationalized N_("text"), client
	 * applications are encouraged to localize with dgettext("zvbi", menu.str[n]).
	 * For details see info gettext.
	 * </td></tr>
	 * </table>
	 */
	VBI_OPTION_MENU
} vbi_option_type;

/**
 * @ingroup Export
 * @brief Result of an option query.
 */
typedef union {
	int			num;
	double			dbl;
	char *			str;
} vbi_option_value;

/**
 * @ingroup Export
 * @brief Option menu types.
 */
typedef union {
	int *			num;
	double *		dbl;
	char **			str;
} vbi_option_value_ptr;

/**
 * @ingroup Export
 * @brief Information about an export option.
 *
 * Clients can access known options by keyword, or enumerate unknown
 * options and using the information in this structure for proper
 * presentation and access.
 *
 * You can obtain this information with vbi_export_option_info_enum().
 */
typedef struct {
	/** @see vbi_option_type */
  	vbi_option_type		type;

	/**
	 * Unique (within the export module) keyword to identify
	 * this option, a NUL terminated ASCII string.
	 */
	const char *		keyword;

	/**
	 * Localized name of the option for the user interface. Can
	 * be @c NULL if the option is not supposed to be listed in the UI.
	 */
	const char *		label;

	/** @see vbi_option_type */
	union {
		int			num;
		double			dbl;
		const char *		str;
	}			def;
	/** @see vbi_option_type */
	union {
		int			num;
		double			dbl;
	}			min, max, step;
	/** @see vbi_option_type */
	union {
		int *			num;
		double *		dbl;
		const char * const *	str;
	}			menu;

	/**
	 * A localized description of the option for the user,
	 * can be @c NULL.
	 */
	const char *		tooltip;
} vbi_option_info;

typedef vbi_bool
vbi_export_link_fn		(vbi_export *		ex,
				 void *			user_data,
				 FILE *			fp,
				 const vbi_link *	link,
				 const char *		text);
typedef vbi_bool
vbi_export_pdc_fn		(vbi_export *		ex,
				 void *			user_data,
				 FILE *			fp,
				 const vbi_preselection *pl,
				 const char *		text);

/**
 * @addtogroup Export
 * @{
 */
extern const vbi_export_info *
vbi_export_info_enum		(unsigned int		index);
extern const vbi_export_info *
vbi_export_info_by_keyword	(const char *		keyword);
extern const vbi_export_info *
vbi_export_info_from_export	(const vbi_export *	e);
extern void
vbi_export_delete		(vbi_export *		e);
extern vbi_export *
vbi_export_new			(const char *		keyword,
				 char **		errstr);
extern const vbi_option_info *
vbi_export_option_info_enum	(vbi_export *		e,
				 unsigned int		index);
extern const vbi_option_info *
vbi_export_option_info_by_keyword
				(vbi_export *		e,
				 const char *		keyword);
extern vbi_bool
vbi_export_option_set		(vbi_export *		e,
				 const char *		keyword,
				 ...);
extern vbi_bool
vbi_export_option_get		(vbi_export *		e,
				 const char *		keyword,
				 vbi_option_value *	value);
extern vbi_bool
vbi_export_option_menu_set	(vbi_export *		e,
				 const char *		keyword,
				 unsigned int		entry);
extern vbi_bool
vbi_export_option_menu_get	(vbi_export *		e,
				 const char *		keyword,
				 unsigned int *		entry);
extern void
vbi_export_set_link_fn		(vbi_export *		e,
				 vbi_export_link_fn *	function,
				 void *			user_data);
extern void
vbi_export_set_pdc_fn		(vbi_export *		e,
				 vbi_export_pdc_fn *	function,
				 void *			user_data);
extern vbi_bool
vbi_export_stdio		(vbi_export *		e,
				 FILE *			fp,
				 const vbi_page *	pg);
extern vbi_bool
vbi_export_file			(vbi_export *		e,
				 const char *		name,
				 const vbi_page *	pg);
extern const char *
vbi_export_errstr		(vbi_export *		e);
/** @} */

/* Private */

#include <stdarg.h>
#include <stddef.h>
#include <iconv.h>

#include "../config.h"
#include "misc.h"

typedef struct vbi_export_module vbi_export_module;

/**
 * @ingroup Exmod
 *
 * Structure representing an export module instance, part of the private
 * export module interface.
 *
 * Export modules can read, but do not normally write its fields, as
 * they are maintained by the public libzvbi export functions.
 */
struct vbi_export {
	/**
	 * Points back to export module description.
	 */
	const vbi_export_module *module;
	char *			errstr;		/**< Frontend private. */

	/**
	 * Name of the file we are writing, @c NULL if none (may be
	 * an anonymous FILE though).
	 */
	const char *		name;

	/**
	 * Generic option: Network name or @c NULL.
	 */
	char *			network;	/* network name or NULL */
	/**
	 * Generic option: Creator name [by default "libzvbi"] or @c NULL.
	 */
	char *			creator;
	/**
	 * Generic option: Reveal hidden characters.
	 */
	vbi_bool		reveal;

	vbi_export_link_fn *	link_function;
	void *			link_user_data;

	vbi_export_pdc_fn *	pdc_function;
	void *			pdc_user_data;

	const vbi_export_info *	local_export_info;
	vbi_option_info *	local_option_info;
};

/**
 * @ingroup Exmod
 *
 * Structure describing an export module, part of the private
 * export module interface. One required for each module.
 *
 * Export modules must initialize these fields (except @a next, see
 * exp-tmpl.c for a detailed discussion) and call vbi_export_register_module()
 * to become accessible.
 */
struct vbi_export_module {
	vbi_export_module *	next;

	const vbi_export_info *	export_info;

	vbi_export *		(* _new)(void);
	void			(* _delete)(vbi_export *e);

	const vbi_option_info *	option_info;
	unsigned int		option_info_size;

	vbi_bool		(* option_set)(vbi_export *e,
					       const char *keyword,
					       va_list);
	vbi_bool		(* option_get)(vbi_export *e,
					       const char *keyword,
					       vbi_option_value *value);
	vbi_bool		(* export)(vbi_export *e, FILE *fp,
					   const vbi_page *pg);
};

/**
 * @example exp-templ.c
 * @ingroup Exmod
 *
 * Template for internal export module.
 */

/*
 *  Helper functions
 */

/**
 * @addtogroup Exmod
 * @{
 */
extern void			vbi_register_export_module(vbi_export_module *);

extern void			vbi_export_write_error(vbi_export *);
extern void			vbi_export_unknown_option(vbi_export *, const char *keyword);
extern void			vbi_export_invalid_option(vbi_export *, const char *keyword, ...);
extern char *			vbi_export_strdup(vbi_export *, char **d, const char *s);
extern void			vbi_export_error_printf(vbi_export *, const char *templ, ...);


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
#define VBI_OPTION_BOOL_INITIALIZER(key_, label_, def_, tip_)		\
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
#define VBI_OPTION_INT_RANGE_INITIALIZER(key_, label_, def_, min_,	\
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
#define VBI_OPTION_INT_MENU_INITIALIZER(key_, label_, def_,		\
  menu_, elements_, tip_) { VBI_OPTION_INT, key_, label_,		\
  { .num = def_}, { .num = 0 }, { .num = (elements_) - 1 },		\
  { .num = 1 }, { .num = menu_ }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like
 * VBI_OPTION_INT_RANGE_INITIALIZER(), just with doubles but ints.
 */
#define VBI_OPTION_REAL_RANGE_INITIALIZER(key_, label_, def_, min_,	\
  max_, step_, tip_) { VBI_OPTION_REAL, key_, label_,			\
  { .dbl = def_ }, { .dbl = min_ }, { .dbl = max_ }, { .dbl = step_ },	\
  { .dbl = NULL }, tip_ }

/**
 * Helper macro for export modules to build option lists. Use like
 * VBI_OPTION_INT_MENU_INITIALIZER(), just with an array of doubles but ints.
 */
#define VBI_OPTION_REAL_MENU_INITIALIZER(key_, label_, def_,		\
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
#define VBI_OPTION_STRING_INITIALIZER(key_, label_, def_, tip_)		\
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
#define VBI_OPTION_STRING_MENU_INITIALIZER(key_, label_, def_,		\
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
#define VBI_OPTION_MENU_INITIALIZER(key_, label_, def_, menu_,		\
  elements_, tip_) { VBI_OPTION_MENU, key_, label_, { .num = def_ },	\
  { .num = 0 }, { .num = (elements_) - 1 }, { .num = 1 },		\
  { .str = menu_ }, tip_ }

/* See exp-templ.c for an example */

/** Doesn't work, sigh. */
#define VBI_AUTOREG_EXPORT_MODULE(name)
/*
#define VBI_AUTOREG_EXPORT_MODULE(name)					\
static void vbi_autoreg_##name(void) __attribute__ ((constructor));	\
static void vbi_autoreg_##name(void) {					\
	vbi_register_export_module(&name);				\
}
*/

/** @} */

#endif /* EXPORT_H */
