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
 * Private stuff, we can play freely with this without losing binary
 * or source compatibility.
 */
#ifndef __TVENG_PRIVATE_H__
#define __TVENG_PRIVATE_H__
struct tveng_private {
  Display	*display;
  int		save_x, save_y;
  int		bpp;
  char		*default_standard;

#ifndef DISABLE_X_EXTENSIONS
  XF86VidModeModeInfo modeinfo;
  int		restore_mode;
  int		xf86vm_enabled;
#endif

  int		zapping_setup_fb_verbosity;
  int		change_mode;
};

#ifndef MAX
#define MAX(X, Y) (((X) < (Y)) ? (Y) : (X))
#endif
#endif
