/* Zapping (TV viewer for the Gnome Desktop)
 * Video Widget
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

#ifndef Z_VIDEO_H
#define Z_VIDEO_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define Z_TYPE_VIDEO		(z_video_get_type ())
#define Z_VIDEO(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), Z_TYPE_VIDEO, ZVideo))
#define Z_VIDEO_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), Z_TYPE_VIDEO, ZVideoClass))
#define Z_IS_VIDEO(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), Z_TYPE_VIDEO))
#define Z_IS_VIDEO_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), Z_TYPE_VIDEO))
#define Z_VIDEO_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), Z_TYPE_VIDEO, ZVideoClass))

typedef struct _ZVideo ZVideo;
typedef struct _ZVideoClass ZVideoClass;

struct _ZVideo
{
  GtkDrawingArea	drawing_area;

  /*< private >*/

  guint			min_width;
  guint			min_height;

  guint			max_width;
  guint			max_height;

  guint			mod_x;
  guint			mod_y;
  
  gfloat		ratio;

  guint			blank_cursor_timeout;
  guint			blank_cursor_timeout_handle;
  GdkCursorType		blanked_cursor_type;

  GtkAllocation		window_alloc;
  GdkGeometry		old_hints;
};

struct _ZVideoClass
{
  GtkDrawingAreaClass	parent_class;

  void			(* cursor_blanked)		(ZVideo *);

  GdkCursor *		blank_cursor;
};

extern GType
z_video_get_type		(void) G_GNUC_CONST;
extern GtkWidget *
z_video_new			(void);
extern gboolean
z_video_set_cursor		(ZVideo *		video,
				 GdkCursorType		cursor_type);
extern void
z_video_blank_cursor		(ZVideo *		video,
				 guint			timeout);
extern void
z_video_set_min_size		(ZVideo *		video,
				 guint			width,
				 guint			height);
extern void
z_video_set_max_size		(ZVideo *		video,
				 guint			width,
				 guint			height);
extern void
z_video_set_size_inc		(ZVideo *		video,
				 guint			width,
				 guint			height);
extern void
z_video_set_aspect		(ZVideo *		video,
				 gfloat			ratio);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* Z_VIDEO_H */
