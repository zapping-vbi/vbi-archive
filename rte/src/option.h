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

#ifndef OPTIONS_H
#define OPTIONS_H

/* Public */

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

typedef int rte_bool;

/**
 * rte_option_type:
 * @RTE_OPTION_BOOL:
 *   A boolean value, either %TRUE (1) or %FALSE (0).
 *   <informaltable frame=none><tgroup cols=2><tbody>
 *   <row><entry>Type:</><entry>int</></row>
 *   <row><entry>Default:</><entry>def.num</></row>
 *   <row><entry>Bounds:</><entry>min.num (0) ... max.num (1),
 *     step.num (1)</></row>
 *   <row><entry>Menu:</><entry>%NULL</></row>
 *   </tbody></tgroup></informaltable>
 * @RTE_OPTION_INT:
 *   A signed integer value. When only a few discrete values rather than
 *   a range are permitted @menu points to a vector of integers. Note the
 *   option is still set by value, not by menu index, which may be rejected
 *   or replaced by the closest possible.
 *   <informaltable frame=none><tgroup cols=2><tbody>
 *   <row><entry>Type:</><entry>int</></row>
 *   <row><entry>Default:</><entry>def.num or menu.num[def.num]</></row>
 *   <row><entry>Bounds:</><entry>min.num ... max.num, step.num or menu</></row>
 *   <row><entry>Menu:</><entry>%NULL or menu.num[min.num ... max.num],
 *     step.num (1)</></row>
 *   </tbody></tgroup></informaltable>
 * @RTE_OPTION_REAL:
 *   A real value, optional a vector of possible values.
 *   <informaltable frame=none><tgroup cols=2><tbody>
 *   <row><entry>Type:</><entry>double</></row>
 *   <row><entry>Default:</><entry>def.dbl or menu.dbl[def.num]</></row>
 *   <row><entry>Bounds:</><entry>min.dbl ... max.dbl,
 *      step.dbl or menu</></row>
 *   <row><entry>Menu:</><entry>%NULL or menu.dbl[min.num ... max.num],
 *      step.num (1)</></row>
 *   </tbody></tgroup></informaltable>
 * @RTE_OPTION_STRING:
 *   A null terminated string. Note the menu version differs from
 *   RTE_OPTION_MENU in its argument, which is the string itself. For example:
 *   <programlisting>
 *   menu.str[0] = "red"
 *   menu.str[1] = "blue"
 *   ... and perhaps other colors not explicitely listed
 *   </programlisting>
 *   <informaltable frame=none><tgroup cols=2><tbody>
 *   <row><entry>Type:</><entry>char *</></row>
 *   <row><entry>Default:</><entry>def.str or menu.str[def.num]</></row>
 *   <row><entry>Bounds:</><entry>not applicable</></row>
 *   <row><entry>Menu:</><entry>%NULL or menu.str[min.num ... max.num],
 *     step.num (1)</></row>
 *   </tbody></tgroup></informaltable>
 * @RTE_OPTION_MENU:
 *   Choice between a number of named options. For example:
 *   <programlisting>
 *   menu.str[0] = "up"
 *   menu.str[1] = "down"
 *   menu.str[2] = "strange"
 *   </programlisting>
 *   <informaltable frame=none><tgroup cols=2><tbody>
 *   <row><entry>Type:</><entry>int</></row>
 *   <row><entry>Default:</><entry>def.num</></row>
 *   <row><entry>Bounds:</><entry>min.num (0) ... max.num, 
 *      step.num (1)</></row>
 *   <row><entry>Menu:</><entry>menu.str[min.num ... max.num],
 *      step.num (1).
 *      These strings are gettext'ized N_(), see the gettext() manuals
 *      for details.</></row>
 *   </tbody></tgroup></informaltable>
 **/
typedef enum {
	RTE_OPTION_BOOL = 1,
	RTE_OPTION_INT,
	RTE_OPTION_REAL,
	RTE_OPTION_STRING,
	RTE_OPTION_MENU
} rte_option_type;

typedef union rte_option_value {
	int			num;
	double			dbl;
	char *			str;
} rte_option_value;

typedef union rte_option_value_ptr {
	int *			num;
	double *		dbl;
	char **			str;
} rte_option_value_ptr;

/**
 * rte_option_info:
 * 
 * Although export options can be accessed by a static keyword they are
 * by definition opaque: the client can present them to the user and
 * manipulate them without knowing about their presence or purpose.
 * To do so, some amount of information about the option is necessary,
 * given in this structure.
 * 
 * You can obtain this information with rte_context_option_info_enum()
 * or rte_codec_option_info_enum().
 * 
 * @type: Type of the option, see #rte_option_type for details.
 *
 * @keyword: Unique (within this context or codec) keyword to identify
 *   this option. Can be stored in configuration files.
 *
 * @label: Name of the option to be shown to the user.
 *   This can be %NULL to indicate this option shall not be listed.
 *   gettext()ized N_(), see the gettext manual.
 *
 * @def, @min, @max, @step, @menu: See #rte_option_type for details.
 *
 * @tooltip: A brief description (or %NULL) for the user.
 *   gettext()ized N_(), see the gettext manual.
 **/
typedef struct {
	rte_option_type		type;
	char *			keyword;
	char *			label;
	rte_option_value	def;
	rte_option_value	min;
	rte_option_value	max;
	rte_option_value	step;
	rte_option_value_ptr	menu;
	char *			tooltip;
} rte_option_info;

#if 1 /* deprecated */
/* use with option varargs to make sure the correct cast is done */
typedef int rte_int;
typedef double rte_real;
typedef char* rte_string;
typedef int rte_menu;
typedef void* rte_pointer;
#endif

typedef enum {
  RTE_BOOL,
  RTE_INT,
  RTE_REAL,
  RTE_STRING
} rte_basic_type;

typedef struct {
  char *		keyword;
  char *		label;
  rte_basic_type	type;
  rte_option_value	val;
} rte_status_info;

/**
 * rte_status_free:
 * @status: Pointer to a rte_status object, or %NULL.
 *
 * Frees all the memory associated with @status. Does nothing if
 * @status is %NULL.
 **/
void
rte_status_free(rte_status_info *status);

/* Private */

#endif /* OPTIONS_H */
