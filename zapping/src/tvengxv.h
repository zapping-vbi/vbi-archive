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

#ifndef __TVENGXV_H__
#define __TVENGXV_H__

#include "tveng_private.h"

#define NO_PORT ((XvPortID) None)
#define ANY_PORT ((XvPortID) None)

extern tv_bool
_tv_xv_stop_video		(tveng_device_info *	info,
				 Window			window);
extern tv_bool
_tv_xv_put_video		(tveng_device_info *	info,
				 Window			window,
				 GC			gc,
				 int			src_x,
				 int			src_y,
				 unsigned int		src_width,
				 unsigned int		src_height);
extern tv_bool
_tv_xv_get_port_attribute	(tveng_device_info *	info,
				 Atom			atom,
				 int *			value);
extern tv_bool
_tv_xv_set_port_attribute	(tveng_device_info *	info,
				 Atom			atom,
				 int			value,
				 tv_bool		sync);
extern tv_bool
_tv_xv_ungrab_port		(tveng_device_info *	info);
extern tv_bool
_tv_xv_grab_port		(tveng_device_info *	info);

extern tv_device_node *
tvengxv_port_scan		(Display *		display,
				 FILE *			log);

/*
  Inits the XVideo module, and fills in the given table.
*/
void tvengxv_init_module(struct tveng_module_info *module_info);

/*
  Prototypes for forward declaration, used only in tvengxv.c
*/
#ifdef TVENGXV_PROTOTYPES
/*
  Associates the given tveng_device_info with the given video
  device. On error it returns -1 and sets info->errno, info->error to
  the correct values.
  device_file: The file used to access the video device (usually
  /dev/video)
  attach_mode: Specifies the mode to open the device file
  depth: The color depth the capture will be in, -1 means let tveng
  decide based on the current display depth.
  info: The structure to be associated with the device
*/
static
int tvengxv_attach_device(const char* device_file,
			  Window window,
			 enum tveng_attach_mode attach_mode,
			 tveng_device_info * info);

/*
  Closes the video device asocciated to the device info object. Should
  be called before reattaching a video device to the same object, but
  there is no need to call this before calling tveng_device_info_destroy.
*/
static void tvengxv_close_device(tveng_device_info* info);

#endif /* TVENGXV_PROTOTYPES */
#endif /* TVENGXV.H */
