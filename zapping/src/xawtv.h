/*
 * Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2003 Michael H. Schimek
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

/* $Id: xawtv.h,v 1.1.2.1 2003-09-24 18:41:25 mschimek Exp $ */

#ifndef XAWTV_H
#define XAWTV_H

#include "frequencies.h"

extern gboolean
xawtv_config_present		(void);
extern gboolean
xawtv_import_config		(const tveng_device_info *info,
				 tveng_tuned_channel **	channel_list);

#endif /* XAWTV_H */
