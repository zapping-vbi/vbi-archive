/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2005 Michael H. Schimek
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

/* $Id: view.h,v 1.2 2007-08-30 12:22:01 mschimek Exp $ */

#ifndef SUBTITLE_VIEW_H
#define SUBTITLE_VIEW_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gtk/gtk.h>
#include "libvbi/page.h"
#include "libvbi/lang.h"
#include "libvbi/exp-gfx.h"
#include "libvbi/vbi_decoder.h"

G_BEGIN_DECLS

#define TYPE_SUBTITLE_VIEW (subtitle_view_get_type ())
#define SUBTITLE_VIEW(obj)						\
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_SUBTITLE_VIEW, SubtitleView))
#define SUBTITLE_VIEW_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_SUBTITLE_VIEW, SubtitleViewClass))
#define IS_SUBTITLE_VIEW(obj)						\
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_SUBTITLE_VIEW))
#define IS_SUBTITLE_VIEW_CLASS(klass)					\
  (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_SUBTITLE_VIEW))
#define SUBTITLE_VIEW_GET_CLASS(obj)					\
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_SUBTITLE_VIEW, SubtitleViewClass))

typedef struct _SubtitleView SubtitleView;
typedef struct _SubtitleViewClass SubtitleViewClass;

struct subt_image
{
  /* Scaled image of pg or NULL. */
  GdkPixbuf *		pixbuf;

  gboolean		valid;

  /* Scaled character cell size. */
  gdouble		cell_width;
  gdouble		cell_height;

  /* Shortcut: redraw only the changed area. */
  GdkRectangle		expose;
};

struct _SubtitleView
{
  GtkDrawingArea	darea;

  /* Subtitle position (whole page, not just opaque text)
     relative to bounds.  Default 0.5, 0.5. */
  gdouble		rel_x;
  gdouble		rel_y;

  /* Subtitle size (whole page) relative to nominal size, which is
     some arbitrary fraction of bounds.  Default 1.0.  Must be > 0.0. */
  gdouble		rel_size;

  gboolean		roll_enable;

  GtkActionGroup *	action_group;

  vbi3_ttx_charset_code	override_charset;

  void
  (* show_page)		(SubtitleView *		view,
			 vbi3_page *		pg);
  /* Current network, any subno. */
  gboolean
  (* load_page)		(SubtitleView *		view,
			 vbi3_pgno		pgno);
  /* Current network, any subno. */
  gboolean
  (* monitor_page)	(SubtitleView *		view,
			 vbi3_pgno		pgno);
  void
  (* start_rolling)	(SubtitleView *		view);
  GdkPixbuf *
  (* get_image)		(SubtitleView *		view,
			 GdkRectangle *		expose,
			 guint			width,
			 guint			height);
  void
  (* set_position)	(SubtitleView *		view,
			 gdouble		x,
			 gdouble		y);
  void
  (* set_size)		(SubtitleView *		view,
			 gdouble		size);
  void
  (* set_charset)	(SubtitleView *		view,
			 vbi3_ttx_charset_code	charset_code);
  void
  (* set_rolling)	(SubtitleView *		view,
			 gboolean		enable);
  void
  (* set_video_bounds)	(SubtitleView *		view,
			 gint			x,
			 gint			y,
			 guint			width,
			 guint			height);

  /*< private >*/

  vbi3_decoder *	vbi;

  vbi3_pgno		monitor_pgno;

  /* The area allocated for subtitles.  Actual size of the subtitle
     window is darea.widget.allocation, which can be larger or smaller. */
  GtkAllocation		visibility_bounds;

  /* Size and position of the video to scale the subtitles accordingly.
     If not given we use visibility_bounds. */
  GtkAllocation		video_bounds;
  gboolean		have_video_bounds;

  /* Displayed page or NULL. */
  vbi3_page *		pg;

  guint			first_opaque_row;
  guint			last_opaque_row;

  /* Unscaled image of pg or NULL. */
  GdkPixbuf *		unscaled_pixbuf;
  vbi3_image_format	unscaled_format;

  /* Scaled image of pg. */
  struct subt_image	capture_scaled;
  struct subt_image	display_scaled;

  /* Non-transparent parts of darea. */
  GdkRegion *		region;

  /* Closed Caption rolling. */

  gboolean		roll_start;
  gboolean		rolling;
  gint			roll_counter;

  /* Interactive subtitle moving and scaling. */

  gboolean		moving;
  gboolean		scaling;

  gint			last_mouse_x;
  gint			last_mouse_y;

  /* Load / show this page when done. */
  vbi3_pgno		load_pgno;
  vbi3_page *		show_pg;
  /* Redraw / display the current page when done. */
  gboolean		redraw_unscaled_full;
  gboolean		redraw_display_scaled;

  gint			move_offset_x;
  gint			move_offset_y;

  gint			scale_center_x;
  gint			scale_center_y;
  gdouble		scale_factor;
};

struct _SubtitleViewClass
{
  GtkDrawingAreaClass	parent_class;

  /* Signals. */

  void (*position_changed)(SubtitleView *view);
};

extern GType
subtitle_view_get_type		(void);
extern GtkWidget *
subtitle_view_new		(void);

G_END_DECLS

#endif /* SUBTITLE_VIEW_H */

/*
Local variables:
c-set-style: gnu
c-basic-offset: 2
End:
*/
