/*
 *  Copyright (C) 2001-2004 Michael H. Schimek
 *  Copyright (C) 2000-2003 Iñaki García Etxebarria
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

/* $Id: screen.h,v 1.2 2005-01-31 07:12:15 mschimek Exp $ */

#ifndef __ZTV_SCREEN_H__
#define __ZTV_SCREEN_H__

#include "overlay_buffer.h"

TV_BEGIN_DECLS

/* Xinerama / DGA routines */

typedef struct _tv_screen tv_screen;

struct _tv_screen {
  /** List of screens if Xinerama is enabled, single screen otherwise. */
  tv_screen *		next;

  /** X11 screen number. */
  int			screen_number;

  /**
   * Screen position in root window coordinates. Note a
   * Xinerama display can have gaps and screens can overlap.
   */
  unsigned int		x;
  unsigned int		y;
  unsigned int		width;
  unsigned int		height;

  /** DMA target, if DGA is enabled. */
  tv_overlay_buffer	target;
};

tv_inline tv_bool
tv_screen_is_target		(const tv_screen *	xs)
{
  /* assert (NULL != xs); */

  return (0 != xs->target.base);
}

extern const tv_screen *
tv_screen_list_find		(const tv_screen *	list,
				 int			x,
				 int			y,
				 unsigned int		width,
				 unsigned int		height);
extern void
tv_screen_list_delete		(tv_screen *		list);
extern tv_screen *
tv_screen_list_new		(const char *		display_name,
				 int			bpp_hint)
  __attribute__ ((malloc,
		  _tv_nonnull (1)));

TV_END_DECLS

#endif /* __ZTV_SCREEN_H__ */
