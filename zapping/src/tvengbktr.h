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

#ifndef __TVENGBKTR_H__
#define __TVENGBKTR_H__

#include "tveng_private.h"

/*
  Inits the V4L2 module, and fills in the given table.
*/
void tvengbktr_init_module(struct tveng_module_info *module_info);

/*
  Prototypes for forward declaration, used only in tvengbktr.c
*/
#ifdef TVENGBKTR_PROTOTYPES
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
int tvengbktr_attach_device(const char* device_file,
			 enum tveng_attach_mode attach_mode,
			 tveng_device_info * info);

/*
  Closes the video device asocciated to the device info object. Should
  be called before reattaching a video device to the same object, but
  there is no need to call this before calling tveng_device_info_destroy.
*/
static void tvengbktr_close_device(tveng_device_info* info);

extern tv_device_node *
tvengbktr_device_scan		(FILE *			log);

#endif /* TVENGBKTR_PROTOTYPES */
#endif /* TVENGBKTR.H */

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
