/*
 *  Real Time Encoding Library
 *
 *  Copyright (C) 2000, 2001 Iñaki García Etxebarria
 *  Copyright (C) 2000, 2001, 2002 Michael H. Schimek
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

/* $Id: option.h,v 1.4 2002-09-26 20:47:35 mschimek Exp $ */

#ifndef OPTIONS_H
#define OPTIONS_H

/* Public */

#include <stdint.h>

/* doxygen sees static and ignores it... */
#define static_inline static inline

/**
 * @ingroup Common
 * @name Boolean type
 * @{
 */
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

typedef unsigned int rte_bool;
/** @} */

/**
 * @ingroup Option
 * Option type.
 */
typedef enum {
	/**
	 * A boolean value, either \c TRUE (1) or @c FALSE (0).
	 * <table>
	 * <tr><td>Type:</td><td>int</td></tr>
	 * <tr><td>Default:</td><td>def.num</td></tr>
	 * <tr><td>Bounds:</td><td>min.num (0) ... max.num (1),
	 * step.num (1)</td></tr>
	 * <tr><td>Menu:</td><td>%NULL</td></tr>
	 * </table>
	 */
	RTE_OPTION_BOOL = 1,

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
	RTE_OPTION_INT,

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
	RTE_OPTION_REAL,

	/**
	 * A null terminated string. Note the menu version differs from
	 * RTE_OPTION_MENU in its argument, which is the string itself. For example:
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
	RTE_OPTION_STRING,

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
	 * applications are encouraged to localize with dgettext("rte", menu.str[n]).
	 * For details see info gettext.
	 * </td></tr>
	 * </table>
	 */
	RTE_OPTION_MENU
} rte_option_type;

/**
 * @ingroup Option
 * Result of an option query.
 */
typedef union {
	int			num;
	double			dbl;
	char *			str;
} rte_option_value;

/**
 * @ingroup Option
 * Common menu types.
 */
typedef union {
	int *			num;
	double *		dbl;
	char **			str;
} rte_option_value_ptr;

/**
 * @ingroup Option
 * Although export options can be accessed by a static keyword they are
 * by definition opaque: the client can present them to the user and
 * manipulate them without knowing about their presence or purpose.
 * To do so, some information about the option is necessary,
 * given in this structure.
 * 
 * You can obtain this information with rte_context_option_info_enum()
 * or rte_codec_option_info_enum().
 */
typedef struct {
  	rte_option_type		type;	/**< @see rte_option_type */

	/**
	 * Unique (within the respective context or codec) keyword to identify
	 * this option. Can be stored in configuration files.
	 */
	char *			keyword;

	/**
	 * Name of the option to be shown to the user.
	 * This can be @c NULL to indicate this option shall not be listed.
	 * Can be localized with dgettext("rte", label).
	 */
	char *			label;

	rte_option_value	def;	/**< @see rte_option_type */
	rte_option_value	min;	/**< @see rte_option_type */
	rte_option_value	max;	/**< @see rte_option_type */
	rte_option_value	step;	/**< @see rte_option_type */
	rte_option_value_ptr	menu;	/**< @see rte_option_type */

	/**
	 * A brief description (or @c NULL) for the user.
	 *  Can be localized with dgettext("rte", tooltip).
	 */
	char *			tooltip;
} rte_option_info;

/**
 * @ingroup Status
 * @name Status flags
 * These defines correspond to fields in rte_status and
 * flag which contain valid information.
 * @{
 */
#define RTE_STATUS_FRAMES_IN		(1 << 3)
#define RTE_STATUS_FRAMES_OUT		(1 << 4)
#define RTE_STATUS_FRAMES_DROPPED	(1 << 5)
#define RTE_STATUS_BYTES_IN		(1 << 6)
#define RTE_STATUS_BYTES_OUT		(1 << 7)
#define RTE_STATUS_CAPTURED_TIME	(1 << 8)
#define RTE_STATUS_CODED_TIME		(1 << 9)
/** @} */

/**
 * @ingroup Status
 * When encoding, codecs and contexts accumulate status information
 * which can be polled with the rte_codec_status() and
 * rte_context_status() functions.
 */
typedef struct {
	/**
	 * Set of RTE_STATUS_FRAMES_IN et al flags, corresponding to the structure
	 * fields. Only those fields flagged as valid will contain
	 * status information, possibly none of them.
	 */
	unsigned int			valid;

	unsigned int			bytes_per_frame_out;
	double				time_per_frame_out;	/**< Seconds and fractions */

	uint64_t			frames_in;
	uint64_t			frames_out;
	uint64_t			frames_dropped;		/**< When context, frames dropped by all codecs */

	uint64_t			bytes_in;
	uint64_t			bytes_out;

	/**
	 * Seconds and fractions since 1970-01-01 00:00.
	 * Practically this is the timestamp of the last processed
	 * input buffer.
	 */
	double				captured_time;

	double				coded_time;		/**< Seconds and fractions since start of encoding */

	/* future extensions */

} rte_status;

/* Private */

#endif /* OPTIONS_H */
