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

/* $Id: export.h,v 1.8.2.5 2004-07-09 16:10:52 mschimek Exp $ */

#ifndef __ZVBI_EXPORT_H__
#define __ZVBI_EXPORT_H__

#include <stdarg.h>		/* va_list */
#include <stdio.h>		/* FILE */
#include "macros.h"
#include "bcd.h"		/* vbi_pgno, vbi_subno */
#include "link.h"		/* vbi_link */
#include "pdc.h"		/* vbi_preselection */
#include "format.h"		/* vbi_page */
#include "vbi_decoder.h"	/* vbi_decoder */

VBI_BEGIN_DECLS

/**
 * @ingroup Export
 * @brief Export context.
 *
 * The contents of this structure are private.
 * Call vbi_export_new() to allocate an export module instance.
 */
typedef struct _vbi_export vbi_export;

/**
 * @ingroup Export
 * @brief Information about an export module.
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
	/**
	 * The file format is open, it can contain more than one
	 * vbi_page. Call vbi_export_stdio() with a NULL vbi_page
	 * pointer to finalize the file.
	 */
	vbi_bool		open_format;
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
	 * a range are permitted @p menu points to a vector of integers. Note
	 * the option is still set by value, not by menu index. Setting the
	 * value may fail, or it may be replaced by the closest possible.
	 * <table>
	 * <tr><td>Type:</td><td>int</td></tr>
	 * <tr><td>Default:</td><td>def.num or menu.num[def.num]</td></tr>
	 * <tr><td>Bounds:</td><td>min.num ... max.num, step.num or menu</td>
	 * </tr>
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
	 * VBI_OPTION_MENU in its argument, which is the string itself.
	 * For example:
	 * @code
	 * menu.str[0] = "red"
	 * menu.str[1] = "blue"
	 * ... and the option may accept other strings not explicitely listed.
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
	 * The menu strings are localized.
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
 * options and use the information in this structure for proper
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

/**
 * @param e Export context passed to vbi_export_stdio() or
 *   other output functions.
 * @param user_data User pointer passed through by
 *   vbi_export_set_link_cb().
 * @param fp Output stream.
 * @param link Structure describing the link.
 *
 * Export modules call this type of function to write a link text
 * with metadata. Might do something like this:
 * @code
 * if (VBI_LINK_HTTP == link->type)
 *         fprintf (fp, "<a href="%s">%s</a>", link->url, link->text);
 * @endcode
 *
 * @returns
 * FALSE on failure, which causes the export module to abort and
 * return FALSE to its caller.
 */
typedef vbi_bool
vbi_export_link_cb		(vbi_export *		e,
				 void *			user_data,
				 FILE *			fp,
				 const vbi_link *	link);
/**
 * @param e Export context passed to vbi_export_stdio() or
 *   other output functions.
 * @param user_data User pointer passed through by
 *   vbi_export_set_pdc_cb().
 * @param fp Output stream.
 * @param ps Structure describing the program.
 * @param text Text of the link.
 *
 * Export modules call this type of function to write a PDC
 * preselection text with metadata.
 *
 * @returns
 * FALSE on failure, which causes the export module to abort and
 * return FALSE to its caller.
 */
typedef vbi_bool
vbi_export_pdc_cb		(vbi_export *		e,
				 void *			user_data,
				 FILE *			fp,
				 const vbi_preselection *ps,
				 const char *		text);

/**
 * @addtogroup Export
 * @{
 */
extern const char *
vbi_export_errstr		(vbi_export *		e);
extern vbi_bool
vbi_export_stdio		(vbi_export *		e,
				 FILE *			fp,
				 const vbi_page *	pg);
extern vbi_bool
vbi_export_file			(vbi_export *		e,
				 const char *		name,
				 const vbi_page *	pg);
extern vbi_bool
vbi_export_stream_close		(vbi_export *		e);
extern vbi_bool
vbi_export_stream_stdio_va_list	(vbi_export *		e,
				 FILE *			fp,
				 vbi_decoder *		vbi,
	    			 vbi_pgno		pgno,
				 vbi_subno		subno,
				 va_list		format_options);
extern vbi_bool
vbi_export_stream_stdio		(vbi_export *		e,
				 FILE *			fp,
				 vbi_decoder *		vbi,
	    			 vbi_pgno		pgno,
				 vbi_subno		subno,
				 ...);
extern vbi_bool
vbi_export_stream_file_va_list	(vbi_export *		e,
				 const char *		name,
				 vbi_decoder *		vbi,
	    			 vbi_pgno		pgno,
				 vbi_subno		subno,
				 va_list		format_options);
extern vbi_bool
vbi_export_stream_file		(vbi_export *		e,
				 const char *		name,
				 vbi_decoder *		vbi,
	    			 vbi_pgno		pgno,
				 vbi_subno		subno,
				 ...);
extern void
vbi_export_set_link_cb		(vbi_export *		e,
				 vbi_export_link_cb *	callback,
				 void *			user_data);
extern void
vbi_export_set_pdc_cb		(vbi_export *		e,
				 vbi_export_pdc_cb *	callback,
				 void *			user_data);
extern void
vbi_export_set_timestamp	(vbi_export *		e,
				 double			timestamp);
extern vbi_bool
vbi_export_option_menu_get	(vbi_export *		e,
				 const char *		keyword,
				 unsigned int *		entry);
extern vbi_bool
vbi_export_option_menu_set	(vbi_export *		e,
				 const char *		keyword,
				 unsigned int		entry);
extern vbi_bool
vbi_export_option_get		(vbi_export *		e,
				 const char *		keyword,
				 vbi_option_value *	value);
extern vbi_bool
vbi_export_option_set		(vbi_export *		e,
				 const char *		keyword,
				 ...);
extern const vbi_option_info *
vbi_export_option_info_enum	(vbi_export *		e,
				 unsigned int		index);
extern const vbi_option_info *
vbi_export_option_info_by_keyword
				(vbi_export *		e,
				 const char *		keyword);
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
/** @} */

VBI_END_DECLS

#endif /* __ZVBI_EXPORT_H__ */
