/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
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

#ifndef __CAPTURE_H__
#define __CAPTURE_H__

#include "common/fifo.h"
#include "tveng.h"
#include "x11stuff.h"
#include "zimage.h"

typedef enum {
  /* Must not change image size. */
  REQ_SIZE		= (1 << 0),
  /* Must not change pixel format. */
  REQ_PIXFMT		= (1 << 1),
} req_flags;

typedef guint
display_filter_fn		(zimage *		dst0,
				 zimage *		dst1);

extern zf_fifo			capture_fifo;

extern zimage *
retrieve_frame			(capture_frame *	frame,
				 tv_pixfmt		pixfmt,
				 gboolean		copy);
extern gboolean
remove_display_filter		(display_filter_fn *	filter);
extern gboolean
add_display_filter		(display_filter_fn *	filter,
				 tv_pixfmt		fmt,
				 guint			width,
				 guint			height);
extern gboolean
capture_stop			(void);
extern gboolean
capture_start			(tveng_device_info *	info,
				 GtkWidget *		window);
extern void
release_capture_format		(gint			id);
extern gboolean
get_capture_format		(gint			id,
				 guint *		width,
				 guint *		height,
				 tv_pixfmt *		pixfmt);
extern gint
request_capture_format		(tveng_device_info *	info,
				 guint			width,
				 guint			height,
				 tv_pixfmt_set		pixfmt_set,
				 req_flags		flags);
extern void
shutdown_capture		(void);
extern void
startup_capture			(void);

#endif
