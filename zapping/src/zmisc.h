/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 Iñaki García Etxebarria
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
  Misc stuff for zapping that didn't fit properly in any other place,
  but where commonly used.
*/

#ifndef __ZMISC_H__
#define __ZMISC_H__

#include <gnome.h>

/* With precompiler aid we can print much more useful info */
#define ShowBox(MSG, MSGTYPE) \
ShowBoxReal(__FILE__, __LINE__, G_GNUC_PRETTY_FUNCTION, \
MSG, MSGTYPE)

/*
  Prints a message box showing an error, with the location of the code
  that called the function.
*/
int ShowBoxReal(const gchar * sourcefile,
		const gint line,
		const gchar * func,
		const gchar * message,
		const gchar * message_box_type);

#endif /* ZMISC.H */
