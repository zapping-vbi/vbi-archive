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

/* $Id: view.c,v 1.2 2005-10-22 15:47:31 mschimek Exp $ */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <gnome.h>
#include <math.h>
#include "libvbi/vbi_decoder.h"
#include "libvbi/exp-gfx.h"
#include "src/i18n.h"
#include "src/zmisc.h"
#include "src/zgconf.h"
#include "src/zvbi.h"
#include "src/subtitle.h"
#include "main.h"		/* subtitle_views */
#include "preferences.h"
#include "view.h"
#include "src/remote.h"		/* python_command_printf */

/*
below video: e.g. video area += 20 % height,
then add 20 % to subtitle rel_y.
*/

#ifndef HAVE_LRINT
#  define lrint(x) ((long)(x))
#endif

#define GCONF_DIR "/apps/zapping/plugins/subtitle"

#define IS_CAPTION_PGNO(pgno) ((pgno) <= 8)
#define IS_TELETEXT_PGNO(pgno) ((pgno) > 8)

#define IS_CAPTION_PAGE(pg) (IS_CAPTION_PGNO ((pg)->pgno))
#define IS_TELETEXT_PAGE(pg) (IS_TELETEXT_PGNO ((pg)->pgno))

enum {
  POSITION_CHANGED,
  N_SIGNALS
};

static GObjectClass *		parent_class;

static guint			signals[N_SIGNALS];

static GdkCursor *		cursor_normal;
static GdkCursor *		cursor_link;

/* User options. */

static vbi3_charset_code	default_charset		= 0;
static GdkInterpType		interp_type		= GDK_INTERP_BILINEAR;
static gint			brightness		= 128;
static gint			contrast		= 64;
static vbi3_rgba		default_foreground	= -1;
static vbi3_rgba		default_background	= 0;
static gboolean			padding			= TRUE;
static gboolean			roll			= TRUE;
static gboolean			show_dheight		= TRUE;

static void
realloc_unscaled		(SubtitleView *		view,
				 const vbi3_page *	pg)
{
  guint src_width;
  guint src_height;

  /* view->unscaled_pixbuf contains no image of view->pg. */
  vbi3_page_unref (view->pg);
  view->pg = NULL;

  if (IS_TELETEXT_PAGE (pg))
    {
      src_width = pg->columns * 12;
      src_height = pg->rows * 10;
    }
  else /* Caption */
    {
      src_width = pg->columns * 16;
      src_height = pg->rows * 13;
    }

  if (view->unscaled_pixbuf)
    g_object_unref (G_OBJECT (view->unscaled_pixbuf));

  view->unscaled_pixbuf =
    gdk_pixbuf_new (GDK_COLORSPACE_RGB,
		    /* has_alpha */ TRUE,
		    /* bits_per_sample */ 8,
		    src_width,
		    src_height);

  /* Translation of pixbuf parameters for vbi3_page_draw(). */

  view->unscaled_format.width = src_width;
  view->unscaled_format.height = src_height;

  view->unscaled_format.offset = 0;

  view->unscaled_format.bytes_per_line =
    gdk_pixbuf_get_rowstride (view->unscaled_pixbuf);

  view->unscaled_format.size =
    view->unscaled_format.bytes_per_line * src_height;

  /* r8, g8, b8, a8 in memory. */
  view->unscaled_format.pixfmt = VBI3_PIXFMT_RGBA24_LE;
  view->unscaled_format.color_space = VBI3_COLOR_SPACE_UNKNOWN;
}

static void
draw_unscaled_page		(SubtitleView *		view,
				 guint *		first_row,
				 guint *		last_row,
				 const vbi3_page *	pg)
{
  if (NULL == view->unscaled_pixbuf)
    realloc_unscaled (view, pg);

  if (NULL == view->pg || view->redraw_unscaled_full)
    {
      vbi3_bool success;

      /* All rows changed in view->unscaled_pixbuf. */
      *first_row = 0;
      *last_row = pg->rows - 1;

      if (IS_TELETEXT_PAGE (pg))
	{
	  success = vbi3_page_draw_teletext
	    (pg,
	     gdk_pixbuf_get_pixels (view->unscaled_pixbuf),
	     &view->unscaled_format,
	     VBI3_BRIGHTNESS, brightness,
	     VBI3_CONTRAST, contrast,
	     VBI3_REVEAL, TRUE,
	     VBI3_FLASH_ON, TRUE,
	     VBI3_END);
	}
      else /* Caption */
	{
	  success = vbi3_page_draw_caption
	    (pg,
	     gdk_pixbuf_get_pixels (view->unscaled_pixbuf),
	     &view->unscaled_format,
	     VBI3_BRIGHTNESS, brightness,
	     VBI3_CONTRAST, contrast,
	     VBI3_END);
	}

      g_assert (success); 

      view->redraw_unscaled_full = FALSE;
    }
  else
    {
      vbi3_char *dcp;
      const vbi3_char *scp;
      guint row_size;
      guint row;

      dcp = view->pg->text;
      scp = pg->text;

      row_size = sizeof (*scp) * pg->columns;

      *first_row = pg->rows;
      *last_row = 0;

      for (row = 0; row < pg->rows; ++row)
	{
	  if (unlikely (0 != memcmp (dcp, scp, row_size)))
	    {
	      vbi3_bool success;

	      *first_row = MIN (row, *first_row);
	      *last_row = MAX (row, *last_row);

	      if (IS_TELETEXT_PAGE (pg))
		{
		  success = vbi3_page_draw_teletext_region
		    (pg,
		     gdk_pixbuf_get_pixels (view->unscaled_pixbuf),
		     &view->unscaled_format,
		     /* x */ 0,
		     /* y */ row * 10,
		     /* column */ 0,
		     row,
		     pg->columns,
		     /* n_rows */ 1,
		     VBI3_BRIGHTNESS, brightness,
		     VBI3_CONTRAST, contrast,
		     VBI3_REVEAL, TRUE,
		     VBI3_FLASH_ON, TRUE,
		     VBI3_END);
		}
	      else /* Caption */
		{
		  success = vbi3_page_draw_caption_region
		    (pg,
		     gdk_pixbuf_get_pixels (view->unscaled_pixbuf),
		     &view->unscaled_format,
		     /* x */ 0,
		     /* y */ row * 13,
		     /* column */ 0,
		     row,
		     pg->columns,
		     /* n_rows */ 1,
		     VBI3_BRIGHTNESS, brightness,
		     VBI3_CONTRAST, contrast,
		     VBI3_END);
		}

	      g_assert (success); 
	    }

	  dcp += pg->columns;
	  scp += pg->columns;
	}
    }
}

static void
text_position			(const SubtitleView *	view,
				 GtkAllocation *	position,
				 const vbi3_page *	pg,
				 const GtkAllocation *	video_bounds,
				 const GtkAllocation *	visible_bounds)
{
  gdouble width;
  gdouble height;
  gdouble max_width;
  gdouble character_cell_height;
  gdouble roll_position;

  if (IS_TELETEXT_PAGE (pg))
    {
      g_assert (pg->columns <= 768 / 16);
      g_assert (pg->rows <= 576 / 20);

      /* Maintain aspect if possible, otherwise squeeze horizontally.
	 We can't adjust height instead because that would put the
	 rows somewhere in the middle of the screen. */
      width = pg->columns * video_bounds->height * (16 / 576.0);
      height = pg->rows * video_bounds->height * (20 / 576.0);
 
      max_width = pg->columns * visible_bounds->width * (16 / 768.0);
    }
  else /* Caption */
    {
      g_assert (pg->columns <= 640 / 16);
      g_assert (pg->rows <= 480 / 26);

      width = pg->columns * video_bounds->height * (16 / 480.0);
      height = pg->rows * video_bounds->height * (26 / 480.0);

      max_width = pg->columns * visible_bounds->width * (16 / 640.0);
    }

  position->width = (gint)(MIN (width, max_width) * view->rel_size);
  position->height = (gint)(height * view->rel_size);

  position->width = MAX (16, position->width);
  position->height = MAX (16, position->height);

  character_cell_height = position->height / (gdouble) pg->rows;
  roll_position = floor ((view->roll_counter - 13)
			 * character_cell_height * (1 / 26.0));

  /* Center the view within visibility_bounds. */

  position->x = lrint ((visible_bounds->x
			- ((position->width + 1) >> 1))
		       + view->rel_x * visible_bounds->width);

  position->y = lrint ((visible_bounds->y
			- ((position->height + 1) >> 1))
		       + view->rel_y * visible_bounds->height
		       + roll_position);
}

static gboolean
realloc_scaled			(struct subt_image *	image,
				 const vbi3_page *	pg,
				 const GtkAllocation *	alloc)
{
  image->valid = FALSE;

  if (NULL == pg
      || alloc->width <= 0
      || alloc->height <= 0)
    {
      if (image->pixbuf)
	{
	  g_object_unref (G_OBJECT (image->pixbuf));
	  image->pixbuf = NULL;
	}

      return FALSE;
    }

  image->cell_width = alloc->width / (double) pg->columns;
  image->cell_height = alloc->height / (double) pg->rows;

  image->expose.width = 0;
  image->expose.height = 0;

  if (NULL != image->pixbuf)
    {
      if (alloc->width == gdk_pixbuf_get_width (image->pixbuf)
	  && alloc->height == gdk_pixbuf_get_height (image->pixbuf))
	return TRUE;

      g_object_unref (G_OBJECT (image->pixbuf));
    }

  image->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				  /* has_alpha */ TRUE,
				  /* bits_per_sample */ 8,
				  alloc->width,
				  alloc->height);

  return TRUE;
}

static gboolean
scale_image			(SubtitleView *		view,
				 struct subt_image *	image,
				 guint			first_row,
				 guint			last_row)
{
  guint height;

  g_assert (NULL != view->pg);
  g_assert (NULL != view->unscaled_pixbuf);
  g_assert (NULL != image->pixbuf);

  if (!image->valid)
    {
      /* Redraw everything. */
      first_row = 0;
      last_row = view->pg->rows - 1;
    }

  image->expose.x = 0;
  image->expose.y = lrint (first_row * image->cell_height - .5);

  if ((gint) image->expose.y < 0)
    image->expose.y = 0;

  image->expose.width = gdk_pixbuf_get_width (image->pixbuf);
  image->expose.height = lrint ((last_row + 1) * image->cell_height + .5);

  height = gdk_pixbuf_get_height (image->pixbuf);
  if ((guint) image->expose.height > height)
    image->expose.height = height;

  image->expose.height -= image->expose.y;

  if (image->expose.width <= 0
      || image->expose.height <= 0)
    return FALSE;

  gdk_pixbuf_scale (/* src */ view->unscaled_pixbuf,
		    /* dst */ image->pixbuf,
		    /* dst_x */ image->expose.x,
		    /* dst_y */ image->expose.y,
		    /* dst_width */ image->expose.width,
		    /* dst_height */ image->expose.height,
		    /* offset_x */ 0,
		    /* offset_y */ 0,
		    /* scale_x */ (gdk_pixbuf_get_width (image->pixbuf)
				   / (gdouble) view->unscaled_format.width),
		    /* scale_y */ (gdk_pixbuf_get_height (image->pixbuf)
				   / (gdouble) view->unscaled_format.height),
		    interp_type);

  image->valid = TRUE;

  return TRUE;
}

static GdkPixbuf *
get_image_			(SubtitleView *		view,
				 GdkRectangle *		expose,
				 guint			width,
				 guint			height)
{
  const vbi3_page *pg;
  GtkAllocation bounds;
  GtkAllocation position;
  struct subt_image *image;
  gint x2;
  gint y2;

  if (NULL == (pg = view->pg))
    return NULL;

  bounds.x = 0;
  bounds.y = 0;
  bounds.width = width;
  bounds.height = height;

  image = &view->capture_scaled;

  if (NULL != image->pixbuf)
    {
      GObject *object = G_OBJECT (image->pixbuf);

      if (object->ref_count > 1)
	{
	  g_object_unref (object);
	  image->pixbuf = NULL;
	}
    }

  if (NULL == image->pixbuf
      || width != (guint) gdk_pixbuf_get_width (image->pixbuf)
      || height != (guint) gdk_pixbuf_get_height (image->pixbuf))
    {
      realloc_scaled (image, pg, &bounds);
    }

  /* Set to all transparent black. */
  gdk_pixbuf_fill (image->pixbuf, 0x00000000);

  text_position (view, &position, pg,
		 /* video */ &bounds,
		 /* visible */ &bounds);

  /* Clip position against bounds. */

  x2 = position.x + position.width;
  y2 = position.y + position.height;

  if (x2 > 0 && position.x < (gint) bounds.width
      && y2 > 0 && position.y < (gint) bounds.height)
    {
      image->expose.x = MAX (0, position.x);
      image->expose.y = MAX (0, position.y);

      image->expose.width = MIN (x2, bounds.width) - image->expose.x;
      image->expose.height = MIN (y2, bounds.height) - image->expose.y;

      image->cell_width = position.width / (double) pg->columns;
      image->cell_height = position.height / (double) pg->rows;

      if (0)
	fprintf (stderr, "Unscaled %u,%u Scaled %u,%u Position %u,%u+%u,%u\n",
		 view->unscaled_format.width,
		 view->unscaled_format.height,
		 bounds.width,
		 bounds.height,
		 position.x,
		 position.y,
		 position.width,
		 position.height);

      gdk_pixbuf_scale (/* src */ view->unscaled_pixbuf,
			/* dst */ image->pixbuf,
			/* dst_x */ image->expose.x,
			/* dst_y */ image->expose.y,
			/* dst_width */ image->expose.width,
			/* dst_height */ image->expose.height,
			/* offset_x */ position.x,
			/* offset_y */ position.y,
			/* scale_x */ position.width
			  / (gdouble) view->unscaled_format.width,
			/* scale_y */ position.height
			  / (gdouble) view->unscaled_format.height,
			interp_type);
    }
  else
    {
      CLEAR (image->expose);
    }

  image->valid = TRUE;

  if (expose)
    *expose = image->expose;

  g_object_ref (G_OBJECT (image->pixbuf));

  return image->pixbuf;
}

static GdkRegion *
nontransparent_region		(const vbi3_page *	pg,
				 const struct subt_image *image)
{
  GdkRegion *region;
  GdkRectangle rect;
  const vbi3_char *cp;
  guint columns;
  guint row;
  uint64_t prev_mask;

  region = gdk_region_new ();

  cp = pg->text;
  columns = pg->columns;

  prev_mask = 0;
  g_assert (columns <= 63);

  for (row = 0; row < pg->rows; ++row)
    {
      guint column;
      uint64_t mask;

      column = 0;
      mask = 0;

      do
	{
	  double y1, xr;
	  gint y1a, y1b, y2a;
	  guint start;
	  uint64_t bridge;

	  while (likely (VBI3_TRANSPARENT_SPACE == cp[column].opacity))
	    if (unlikely (++column >= columns))
	      goto end_of_row;

	  start = column;

	  while (likely (++column < columns))
	    if (unlikely (VBI3_TRANSPARENT_SPACE == cp[column].opacity))
	      break;

	  mask |= (((uint64_t) 1 << (column - start)) - 1) << start;
	  bridge = prev_mask & mask;

	  xr = .5;
	  y1 = row * image->cell_height;

	  y1a = lrint (y1 + .5);
	  y1b = lrint (y1 - .5);
	  y2a = lrint (y1 + image->cell_height - .5);

	  while (start < column)
	    {
	      guint i;

	      for (i = start; i < column; ++i)
		if (0 != (bridge & ((uint64_t) 1 << i)))
		  break;

	      if (i > start)
		{
		  rect.x = lrint (start * image->cell_width + xr);
		  rect.y = y1a; 
		  rect.width = lrint (i * image->cell_width - .5) - rect.x;
		  rect.height = y2a - rect.y;

		  gdk_region_union_with_rect (region, &rect);

		  xr = -.5; /* close gap between blocks */
		}

	      for (start = i; i < column; ++i)
		if (0 == (bridge & ((uint64_t) 1 << i)))
		  break;

	      if (i > start)
		{
		  rect.x = lrint (start * image->cell_width + xr);
		  /* Close gap between rows. One could also shift, intersect
		     and union regions but I think this is faster. */
		  rect.y = y1b;
		  rect.width = lrint (i * image->cell_width - .5) - rect.x;
		  rect.height = y2a - rect.y;

		  gdk_region_union_with_rect (region, &rect);

		  start = i;
		  xr = -.5;
		}
	    }
	}
      while (column < columns);

    end_of_row:
      cp += columns;

      prev_mask = mask;
    }

  return region;
}

static void
update_window			(SubtitleView *		view,
				 gboolean		reposition,
				 gboolean		reshape,
				 gboolean		rescale,
				 guint			first_row,
				 guint			last_row)
{
  GtkAllocation text;
  const vbi3_page *pg;
  gdouble roll_position;
  GdkRectangle extents;
  GdkRectangle clip_rect;
  GdkRegion *region;
  gint indent;

  pg = view->pg;

  if (NULL == pg
      || 0 == view->visibility_bounds.width
      || 0 == view->visibility_bounds.height)
    {
      if (reposition)
	{
	  text.x = view->visibility_bounds.x;
	  text.y = view->visibility_bounds.y;
	  text.width = 1;
	  text.height = 1;

	  GTK_WIDGET_CLASS (parent_class)->size_allocate
	    (&view->darea.widget, &text);
	}

      return;
    }

  text = view->darea.widget.allocation;

  if (reposition)
    {
      GtkAllocation *bounds;

      bounds = view->have_video_bounds ?
	&view->video_bounds : &view->visibility_bounds;

      text_position (view, &text, pg, bounds, &view->visibility_bounds);
    }

  if (NULL == view->display_scaled.pixbuf
      || text.width != gdk_pixbuf_get_width (view->display_scaled.pixbuf)
      || text.height != gdk_pixbuf_get_height (view->display_scaled.pixbuf))
    {
      realloc_scaled (&view->display_scaled, view->pg, &text);

      reshape = TRUE; /* scaled cell size changed */

      rescale = TRUE;

      first_row = 0;
      last_row = view->pg->rows - 1;
    }
  else if (view->redraw_display_scaled)
    {
      rescale = TRUE;

      first_row = 0;
      last_row = view->pg->rows - 1;
    }

  if (rescale)
    {
      if (scale_image (view, &view->display_scaled, first_row, last_row))
	{
	  if (reshape)
	    {
	      if (view->region)
		gdk_region_destroy (view->region);

	      if (0 /* transparent background */)
		{
		  /* Create bitmap from scaled alpha,
		     region from bitmap,
		     rest as below. */
		}
	      else
		{
		  view->region =
		    nontransparent_region (view->pg, &view->display_scaled);
		}
	    }
#if 0
	  /* Set alpha to fully opaque. */

	  p = gdk_pixbuf_get_pixels (image->pixbuf);
	  rowstride = gdk_pixbuf_get_rowstride (image->pixbuf);

	  end = p + (image->expose.y + image->expose.height) * rowstride;
	  p += image->expose.y * rowstride + 3;

	  g_assert (0 == rowstride % 4);

	  while (p < end)
	    {
	      *p = 0xFF;
	      p += 4;
	    }
#endif
	}
      else /* wtf? */
	{
	  reshape = FALSE;
	  rescale = FALSE;
	}
    }

  /* Make sure the subtitles are visible
     but do not reach across view->visibility_bounds. */

  gdk_region_get_clipbox (view->region, &extents);

  clip_rect.x = 0;
  clip_rect.y = 0;

  /* Ignore empty extents with x, y zero. */
  if (extents.width | extents.height)
    {
      clip_rect.width = text.width;
      clip_rect.height = text.height;

      if (extents.width > view->visibility_bounds.width)
	{
	  /* Bugger! Won't fit, we center. */

	  indent = extents.x + ((extents.width
				 - view->visibility_bounds.width + 1) >> 1);

	  clip_rect.x = indent;
	  clip_rect.width = view->visibility_bounds.width;

	  goto limit_x;
	} 
      else if (text.x + extents.x < view->visibility_bounds.x)
	{
	  indent = extents.x;
	  goto limit_x;
	} 
      else if (text.x + extents.x + extents.width
	       > view->visibility_bounds.x + view->visibility_bounds.width)
	{
	  indent = - (view->visibility_bounds.width
		      - extents.x - extents.width);

	limit_x:
	  view->rel_x = (((text.width + 1) >> 1) - indent)
	    / (gdouble) view->visibility_bounds.width;

	  text.x = view->visibility_bounds.x - indent;

	  reposition = TRUE;
	}

      if (extents.height > view->visibility_bounds.height)
	{
	  indent = extents.y
	    + ((extents.height - view->visibility_bounds.height + 1) >> 1);

	  clip_rect.y = indent;
	  clip_rect.height = view->visibility_bounds.height;

	  goto limit_y;
	} 
      else if (text.y + extents.y < view->visibility_bounds.y)
	{
	  indent = extents.y;
	  goto limit_y;
	} 
      else if (text.y + extents.y + extents.height
	       > view->visibility_bounds.y + view->visibility_bounds.height)
	{
	  indent = - (view->visibility_bounds.height
		      - extents.y - extents.height);

	limit_y:
	  roll_position = floor ((view->roll_counter - 13)
				 * view->display_scaled.cell_height
				 * (1 / 26.0));

	  view->rel_y = (((text.height + 1) >> 1) - indent - roll_position)
	    / (gdouble) view->visibility_bounds.height;

	  text.y = view->visibility_bounds.y - indent;
	  
	  reposition = TRUE;
	}
    }

  region = view->region;

  if (clip_rect.x | clip_rect.y)
    {
      region = gdk_region_rectangle (&clip_rect);
      gdk_region_intersect (region, view->region);
    }

  /* Keep these calls together to reduce flicker. */

  if (reposition | reshape)
    {
      gdk_window_shape_combine_region (view->darea.widget.window,
				       region,
				       /* offset_x */ 0,
				       /* offset_y */ 0);
    }

  if (reposition)
    GTK_WIDGET_CLASS (parent_class)->size_allocate
      (&view->darea.widget, &text);

  if (rescale)
    {
      /* Scaled image changed, make it visible. If only the window
         moved the expose handler will redraw. */

      gdk_draw_pixbuf (view->darea.widget.window,
		       view->darea.widget.style->white_gc,
		       view->display_scaled.pixbuf,
		       /* src_x */ view->display_scaled.expose.x,
		       /* src_y */ view->display_scaled.expose.y,
		       /* dst_x */ view->display_scaled.expose.x,
		       /* dst_y */ view->display_scaled.expose.y,
		       view->display_scaled.expose.width,
		       view->display_scaled.expose.height,
		       GDK_RGB_DITHER_NONE,
		       /* x_dither */ 0,
		       /* y_dither */ 0);

      view->redraw_display_scaled = FALSE;
    }

  if (clip_rect.x | clip_rect.y)
    gdk_region_destroy (region);
}

static void
redraw_unscaled			(SubtitleView *		view)
{
  guint first_row;
  guint last_row;

  if (NULL == view->pg)
    return;

  view->redraw_unscaled_full = TRUE;

  if (view->moving | view->scaling)
    return; /* later */

  draw_unscaled_page (view, &first_row, &last_row, view->pg);

  update_window (view,
		 /* reposition */ FALSE,
		 /* reshape */ FALSE,
		 /* rescale */ TRUE,
		 first_row,
		 last_row);
}

static gboolean
reset_rolling			(SubtitleView *		view,
				 const vbi3_page *	pg)
{
  gint new_counter;

  view->roll_start = FALSE;
  view->rolling = FALSE;

  new_counter = 13;

  if (roll /* user option */
      && view->roll_enable
      && NULL != pg
      && IS_CAPTION_PAGE (pg))
    new_counter = 26 - 2;

  if (new_counter == view->roll_counter)
    return FALSE;

  view->roll_counter = new_counter;

  return TRUE; /* reposition the window */
}

static void
start_rolling_			(SubtitleView *		view)
{
  view->roll_start = TRUE;

  /* Actual start in show_page, since we must wait for new
     text (the top row disappearing after a CR) to avoid flicker. */
}

static void
set_rolling_			(SubtitleView *		view,
				 gboolean		enable)
{
  if (!enable)
    if (reset_rolling (view, view->pg))
      update_window (view,
		     /* reposition */ TRUE,
		     /* reshape */ FALSE,
		     /* rescale */ FALSE,
		     /* first_row */ 0,
		     /* last_row */ 0);

  view->roll_enable = enable;
}

static void
show_page_			(SubtitleView *		view,
				 vbi3_page *		pg)
{
  guint first_row;
  guint last_row;
  gboolean reposition;

  if (view->moving | view->scaling)
    {
      /* Show this page when done. */

      vbi3_page_unref (view->show_pg);
      view->show_pg = vbi3_page_ref (pg);

      return;
    }

  reposition = FALSE;

  if (NULL == view->pg
      || view->pg->columns != pg->columns
      || view->pg->rows != pg->rows)
    {
      realloc_unscaled (view, pg);

      if (!view->roll_start)
	reset_rolling (view, view->pg);

      /* Depends on pg. */
      reposition |= TRUE;
    }

  if (view->roll_start)
    {
      reposition |= reset_rolling (view, view->pg);

      if (roll && view->roll_enable
	  && IS_CAPTION_PAGE (pg))
	{
	  view->rolling = TRUE;
	}
    }

  draw_unscaled_page (view, &first_row, &last_row, pg);

  if (first_row > last_row)
    {
      /* No change between view->pg and pg. */
      return;
    }

  /* view->unscaled_pixbuf contains image of pg. */
  vbi3_page_unref (view->pg);
  view->pg = vbi3_page_ref (pg);

  update_window (view,
		 reposition,
		 /* reshape */ TRUE,
		 /* rescale */ TRUE,
		 first_row,
		 last_row);
}

static void
used_rows			(const vbi3_page *	pg,
				 unsigned int *		first_row,
				 unsigned int *		last_row)
{
  const vbi3_char *cp;
  const vbi3_char *end;

  g_assert (NULL != pg);
  g_assert (NULL != first_row);
  g_assert (NULL != last_row);

  for (cp = pg->text, end = cp + pg->rows * pg->columns; cp < end; ++cp)
    if (VBI3_TRANSPARENT_SPACE != cp->opacity)
      break;

  if (cp >= end)
    {
      *first_row = pg->rows;
      *last_row = 0;
      return;
    }

  *first_row = (cp - pg->text) / pg->columns;

  SWAP (cp, end);

  while (--cp > end)
    if (VBI3_TRANSPARENT_SPACE != cp->opacity)
      break;

  *last_row = (cp - pg->text) / pg->columns;
}

static void
shrink_double_height		(vbi3_page *		pg)
{
  unsigned int first_row;
  unsigned int last_row;
  vbi3_char *t;
  vbi3_char *b;
  vbi3_char *dst;
  vbi3_char *end;
  unsigned int rows;
  unsigned int columns;
  int skip_columns;
  vbi3_char ts;

  used_rows (pg, &first_row, &last_row);

  rows = last_row - first_row + 1;
  if ((int) rows < 2)
    return;

  columns = pg->columns;
  skip_columns = pg->columns;

  if (first_row > pg->rows / 2)
    {
      t = pg->text + last_row * columns;
      end = pg->text + first_row * columns;
      skip_columns = -columns;
    }
  else
    {
      t = pg->text + first_row * columns;
      end = pg->text + (last_row + 1) * columns;
    }

  dst = t;
  b = t + skip_columns;

  while (rows >= 2)
    {
      int all_normal;
      int all_double_height;
      unsigned int i;

      all_normal = 0;
      all_double_height = 0;

      for (i = 0; i < columns; ++i)
	{
	  all_normal |= VBI3_NORMAL_SIZE ^ t[i].size;

	  all_double_height |= t[i].unicode ^ b[i].unicode;
	  all_double_height |= t[i].background ^ b[i].background;
	  all_double_height |= t[i].foreground ^ b[i].foreground;
	  all_double_height |= t[i].opacity ^ b[i].opacity;
	  all_double_height |= t[i].attr ^ b[i].attr;
	}

      if (0 == all_double_height)
	{
	  for (i = 0; i < columns; ++i)
	    {
	      static const vbi3_size new_size [] =
		{
		  [VBI3_NORMAL_SIZE]		= VBI3_NORMAL_SIZE,
		  [VBI3_DOUBLE_WIDTH]		= VBI3_DOUBLE_WIDTH,
		  [VBI3_DOUBLE_HEIGHT]		= VBI3_NORMAL_SIZE,
		  [VBI3_DOUBLE_SIZE]		= VBI3_DOUBLE_WIDTH,
		  [VBI3_OVER_TOP]		= VBI3_OVER_TOP,
		  [VBI3_OVER_BOTTOM]		= VBI3_OVER_TOP,
		  [VBI3_DOUBLE_HEIGHT2]		= VBI3_NORMAL_SIZE,
		  [VBI3_DOUBLE_SIZE2]		= VBI3_DOUBLE_WIDTH,
		};

	      dst[i] = t[i];
	      dst[i].size = new_size[t[i].size];
	    }

	  t += 2 * skip_columns;
	  b += 2 * skip_columns;

	  dst += skip_columns;

	  rows -= 2;
	}
      else
	{
	  memcpy (dst, t, sizeof (*dst) * columns);
	  dst += skip_columns;

	  if (0 == all_normal)
	    {
	      t += skip_columns;
	      b += skip_columns;

	      --rows;
	    }
	  else
	    {
	      /* Mixed size. */

	      memcpy (dst, b, sizeof (*dst) * columns);
	      dst += skip_columns;

	      t += 2 * skip_columns;
	      b += 2 * skip_columns;

	      rows -= 2;
	    }
	}
    }

  if (1 == rows)
    {
      memcpy (dst, t, sizeof (*dst) * columns);
      dst += skip_columns;
    }

  CLEAR (ts);
  ts.opacity = VBI3_TRANSPARENT_SPACE;
  ts.foreground = VBI3_WHITE;
  ts.background = VBI3_BLACK;
  ts.unicode = 0x0020;

  if (skip_columns < 0)
    {
      for (dst += columns - 1; dst >= end;)
	*dst-- = ts;
    }
  else
    {
      while (dst < end)
	*dst++ = ts;
    }
}

static void
change_opacity			(vbi3_page *		pg,
				 vbi3_opacity		opacity)
{
  vbi3_opacity new_opacity [4];
  vbi3_char *cp;
  vbi3_char *end;

  new_opacity[VBI3_TRANSPARENT_SPACE] = VBI3_TRANSPARENT_SPACE;
  new_opacity[VBI3_TRANSPARENT_FULL] = opacity;
  new_opacity[VBI3_TRANSLUCENT] = opacity;
  new_opacity[VBI3_OPAQUE] = opacity;

  end = pg->text + pg->rows * pg->columns;

  for (cp = pg->text; cp < end; ++cp)
    cp->opacity = new_opacity[cp->opacity];
}

static vbi3_bool
decoder_event_handler		(const vbi3_event *	ev,
				 void *			user_data)
{
  SubtitleView *view = SUBTITLE_VIEW (user_data);

  switch (ev->type)
    {
    case VBI3_EVENT_CLOSE:
      gtk_widget_destroy (&view->darea.widget);
      break;

    case VBI3_EVENT_TIMER:
      if (view->moving | view->scaling)
	break;

      if (view->rolling && view->roll_counter >= 0)
	{
	  update_window (view,
			 /* reposition */ TRUE,
			 /* reshape */ FALSE,
			 /* rescale */ FALSE,
			 /* first_row */ 0,
			 /* last_row */ 0);

	  view->roll_counter -= 2;
	}

      break;

    case VBI3_EVENT_TTX_PAGE:
      if (ev->ev.ttx_page.pgno == view->monitor_pgno)
	view->load_page (view, view->monitor_pgno);
      break;

    case VBI3_EVENT_CC_PAGE:
      if (ev->ev.caption.channel != view->monitor_pgno)
	return FALSE; /* pass on */

      if (ev->ev.caption.flags & VBI3_START_ROLLING)
	view->start_rolling (view);

      view->load_page (view, view->monitor_pgno);

      break;

    default:
      break;
    }

  return FALSE; /* pass on */
}

static gboolean
load_page_			(SubtitleView *		view,
				 vbi3_pgno		pgno)
{
  vbi3_page *pg;
  vbi3_event_mask event_mask;

  if (view->moving | view->scaling)
    {
      vbi3_page_unref (view->show_pg);
      view->show_pg = NULL;

      view->load_pgno = pgno;

      return TRUE;
    }

  if (NULL == view->vbi)
    {
      if (NULL == (view->vbi = zvbi_get_object ()))
	return FALSE;
    }

  event_mask = 0;
  
  if (IS_TELETEXT_PGNO (pgno))
    {
      /* Override charset code from channel config, if present. */
      zvbi_cur_channel_get_ttx_encoding (&view->override_charset, pgno);

      if (VBI3_CHARSET_CODE_NONE != view->override_charset)
	{
	  pg = vbi3_decoder_get_page
	    (view->vbi, /* nk: current */ NULL,
	     pgno, VBI_ANY_SUBNO,
	     VBI3_HYPERLINKS, TRUE,
	     /* TODO: VBI3_PDC_LINKS, TRUE, */
	     VBI3_WST_LEVEL, VBI_WST_LEVEL_1p5,
	     VBI3_OVERRIDE_CHARSET_0, view->override_charset,
	     VBI3_END);
	}
      else
	{
	  pg = vbi3_decoder_get_page
	    (view->vbi, /* nk: current */ NULL,
	     pgno, VBI_ANY_SUBNO,
	     VBI3_HYPERLINKS, TRUE,
	     /* VBI3_PDC_LINKS, TRUE, */
	     VBI3_WST_LEVEL, VBI_WST_LEVEL_1p5,
	     VBI3_DEFAULT_CHARSET_0, default_charset,
	     VBI3_END);
	}

      event_mask = (VBI3_EVENT_CLOSE |
		    VBI3_EVENT_TIMER |
		    VBI3_EVENT_TTX_PAGE);
    }
  else /* Caption */
    {
      pg = vbi3_decoder_get_page
	(view->vbi, /* nk: current */ NULL,
	 pgno, /* subno */ 0,
	 VBI3_DEFAULT_FOREGROUND, default_foreground | VBI3_RGBA (0, 0, 0),
	 VBI3_DEFAULT_BACKGROUND, default_background | VBI3_RGBA (0, 0, 0),
	 VBI3_PADDING, (vbi3_bool) padding,
	 VBI3_END);

      event_mask = (VBI3_EVENT_CLOSE |
		    VBI3_EVENT_TIMER |
		    VBI3_EVENT_CC_PAGE);
    }

  if (!pg)
    return FALSE;

  vbi3_page_unref (view->show_pg);
  view->show_pg = NULL;

  if (!show_dheight && IS_TELETEXT_PAGE (pg))
    shrink_double_height (pg);

  change_opacity (pg, VBI3_OPAQUE);

  view->show_page (view, pg);

  vbi3_page_unref (pg);

  return TRUE;
}

static gboolean
monitor_page_			(SubtitleView *		view,
				 vbi3_pgno		pgno)
{
  vbi3_event_mask event_mask;

  if (NULL == view->vbi
      && (NULL == (view->vbi = zvbi_get_object ())))
    return FALSE;

  view->monitor_pgno = pgno;

  /* Error ignored. When the page is not cached yet, we'll
     load it when it arrives. */
  view->load_page (view, pgno);

  event_mask = 0;
  
  if (IS_TELETEXT_PGNO (pgno))
    {
      event_mask = (VBI3_EVENT_CLOSE |
		    VBI3_EVENT_TIMER |
		    VBI3_EVENT_TTX_PAGE);
    }
  else /* Caption */
    {
      event_mask = (VBI3_EVENT_CLOSE |
		    VBI3_EVENT_TIMER |
		    VBI3_EVENT_CC_PAGE);
    }

  /* Add an event handler or change its event mask (we need no
     Teletext events while displaying a Close Caption page and
     vice versa). */
  if (!vbi3_decoder_add_event_handler (view->vbi, event_mask,
				       decoder_event_handler, view))
    {
      return FALSE;
    }

  return TRUE;
}

/* Configuration. */

static void
set_charset_			(SubtitleView *		view,
				 vbi3_charset_code	charset_code)
{
  vbi3_page *pg;

  if (charset_code == view->override_charset)
    return;

  view->override_charset = charset_code;

  if (!(pg = view->pg))
    return;

  if (IS_CAPTION_PAGE (pg))
    return;

  /* Error ignored. */
  zvbi_cur_channel_set_ttx_encoding (pg->pgno, charset_code);

  view->load_page (view, pg->pgno);
}

static void
interp_type_notify		(GConfClient *		client,
				 guint			cnxn_id,
				 GConfEntry *		entry,
				 gpointer		user_data)
{
  client = client;
  cnxn_id = cnxn_id;
  user_data = user_data;

  if (entry->value)
    {
      const gchar *s;
      gint enum_value;

      s = gconf_value_get_string (entry->value);
      if (s && gconf_string_to_enum (subtitle_interp_enum, s, &enum_value))
	{
	  GList *p;

	  interp_type = (GdkInterpType) enum_value;

	  for (p = g_list_first (subtitle_views); p; p = p->next)
	    {
	      SubtitleView *view = p->data;

	      if (!view->pg)
		continue;

	      if (view->moving | view->scaling)
		{
		  view->redraw_display_scaled = TRUE;
		  continue;
		}

	      update_window (view,
			     /* reposition */ FALSE,
			     /* reshape */ FALSE,
			     /* rescale */ TRUE,
			     /* first_row */ 0,
			     /* last_row */ view->pg->rows - 1);
	    }
	}
    }
}

static void
default_charset_notify		(GConfClient *		client,
				 guint			cnxn_id,
				 GConfEntry *		entry,
				 gpointer		user_data)
{
  client = client;
  cnxn_id = cnxn_id;
  user_data = user_data;

  if (entry->value)
    {
      const gchar *s;
      gint enum_value;

      s = gconf_value_get_string (entry->value);
      if (s && gconf_string_to_enum (subtitle_charset_enum, s, &enum_value))
	{
	  GList *p;

	  default_charset = enum_value;

	  for (p = g_list_first (subtitle_views); p; p = p->next)
	    {
	      SubtitleView *view = p->data;
	      vbi3_page *pg;

	      if ((pg = view->pg) && IS_TELETEXT_PAGE (pg))
		{
		  view->load_page (view, pg->pgno);
		}
	    }
	}
    }
}

static void
redraw_unscaled_notify		(GConfClient *		client,
				 guint			cnxn_id,
				 GConfEntry *		entry,
				 gpointer		user_data)
{
  gboolean success = FALSE;

  client = client;
  cnxn_id = cnxn_id;
  entry = entry;
  user_data = user_data;

  success |= z_gconf_get_int (&brightness, GCONF_DIR "/brightness");
  success |= z_gconf_get_int (&contrast, GCONF_DIR "/contrast");

  if (success)
    {
      GList *p;

      for (p = g_list_first (subtitle_views); p; p = p->next)
	{
	  SubtitleView *view = p->data;

	  redraw_unscaled (view);
	}
    }
}

static gboolean
get_color			(vbi3_rgba *		rgba,
				 const gchar *		key)
{
  gchar *str;
  gchar *s;
  gboolean r;
  guint value;
  guint i;

  if (!z_gconf_get_string (&str, key))
    return FALSE;

  s = str;
  r = FALSE;

  while (g_ascii_isspace (*s))
    ++s;

  if ('#' != *s++)
    goto failure;

  while (g_ascii_isspace (*s))
    ++s;

  value = 0;

  for (i = 0; i < 6; ++i)
    {
      if (g_ascii_isdigit (*s))
	value = value * 16 + (*s - '0');
      else if (g_ascii_isxdigit (*s))
	value = value * 16 + ((*s - ('A' - 0xA)) & 0xF);
      else
	goto failure;

      ++s;
    }

  while (g_ascii_isspace (*s))
    ++s;

  if (0 == *s)
    {
      r = TRUE;

      *rgba = (((value & 0xFF) << 16) |
	       ((value & 0xFF00) << 0) |
	       ((value & 0xFF0000) >> 16));
    }

 failure:
  g_free (str);

  return r;
}

static void
caption_reload_notify		(GConfClient *		client,
				 guint			cnxn_id,
				 GConfEntry *		entry,
				 gpointer		user_data)
{
  gboolean success = FALSE;

  client = client;
  cnxn_id = cnxn_id;
  entry = entry;
  user_data = user_data;

  success |= z_gconf_get_bool (&padding, GCONF_DIR "/pad");
  success |= get_color (&default_foreground, GCONF_DIR "/foreground");
  success |= get_color (&default_background, GCONF_DIR "/background");

  if (success)
    {
      GList *p;

      for (p = g_list_first (subtitle_views); p; p = p->next)
	{
	  SubtitleView *view = p->data;
	  vbi3_page *pg;

	  if ((pg = view->pg) && IS_CAPTION_PAGE (pg))
	    {
	      view->load_page (view, pg->pgno);
	    }
	}
    }
}

static void
show_dheight_notify		(GConfClient *		client,
				 guint			cnxn_id,
				 GConfEntry *		entry,
				 gpointer		user_data)
{
  client = client;
  cnxn_id = cnxn_id;
  entry = entry;
  user_data = user_data;

  if (z_gconf_get_bool (&show_dheight, GCONF_DIR "/show_dheight"))
    {
      GList *p;

      for (p = g_list_first (subtitle_views); p; p = p->next)
	{
	  SubtitleView *view = p->data;
	  vbi3_page *pg;

	  if ((pg = view->pg) && IS_TELETEXT_PAGE (pg))
	    {
	      view->load_page (view, pg->pgno);
	    }
	}
    }
}

static void
roll_notify			(GConfClient *		client,
				 guint			cnxn_id,
				 GConfEntry *		entry,
				 gpointer		user_data)
{
  client = client;
  cnxn_id = cnxn_id;
  entry = entry;
  user_data = user_data;

  if (z_gconf_get_bool (&roll, GCONF_DIR "/roll"))
    {
      GList *p;

      for (p = g_list_first (subtitle_views); p; p = p->next)
	{
	  SubtitleView *view = p->data;

	  if (reset_rolling (view, view->pg))
	    update_window (view,
			   /* reposition */ TRUE,
			   /* reshape */ FALSE,
			   /* rescale */ FALSE,
			   /* first_row */ 0,
			   /* last_row */ 0);
	}
    }
}

/* Actions. */

static void
disable_action			(GtkAction *		action,
				 SubtitleView *		view)
{
  action = action;
  view = view;

  python_command_printf (NULL, "zapping.closed_caption(0)");
}

static void
reset_position_action		(GtkAction *		action,
				 SubtitleView *		view)
{
  action = action;

  view->rel_x = 0.5;
  view->rel_y = 0.5;

  view->rel_size = 1.0;

  g_signal_emit (view, signals[POSITION_CHANGED], 0);

  update_window (view,
		 /* reposition */ TRUE,
		 /* reshape */ FALSE,
		 /* rescale */ FALSE,
		 /* first_row */ 0,
		 /* last_row */ 0);
}

static void
preferences_action		(GtkAction *		action,
				 SubtitleView *		view)
{
  action = action;
  view = view;

  python_command_printf (NULL, "zapping.properties('Plugins','Subtitles')");
}

static GtkActionEntry
actions [] = {
  { "SubtitlePopupSubmenu", NULL, "Dummy", NULL, NULL, NULL },
  { "SubtitleDisable", NULL, N_("_Disable"), NULL, NULL,
    G_CALLBACK (disable_action) },
  { "SubtitleResetPosition", NULL, N_("_Reset position"), NULL, NULL,
    G_CALLBACK (reset_position_action) },
  { "SubtitleEncodingSubmenu", NULL, N_("_Encoding"), NULL, NULL, NULL },
  { "SubtitlePreferences", GTK_STOCK_PREFERENCES, NULL, NULL,
    NULL, G_CALLBACK (preferences_action) },
};

/* Menus. */

static const char *
popup_menu_description =
"<ui>"
" <menubar name='Popup'>"
"  <menu action='SubtitlePopupSubmenu'>"
"   <menuitem action='SubtitleDisable'/>"
"   <menuitem action='SubtitleResetPosition'/>"
"   <menuitem action='SubtitleEncodingSubmenu'/>"
"   <menuitem action='SubtitlePreferences'/>"
"  </menu>"  
" </menubar>"
"</ui>";

static void
on_encoding_menu_toggled	(GtkCheckMenuItem *	menu_item,
				 zvbi_encoding_menu *	em)
{
  SubtitleView *view = SUBTITLE_VIEW (em->user_data);

  if (menu_item->active)
    view->set_charset (view, em->code);
}

static void
create_popup_menu		(SubtitleView *		view,
				 const GdkEventButton *	event)
{
  GError *error = NULL;
  GtkUIManager *ui_manager;
  GtkWidget *item;
  GtkWidget *widget;
  GtkWidget *popup_menu;
  gboolean success;

  ui_manager = gtk_ui_manager_new ();
  gtk_ui_manager_insert_action_group (ui_manager, view->action_group, APPEND);

  success = gtk_ui_manager_add_ui_from_string (ui_manager,
					       popup_menu_description,
					       NUL_TERMINATED,
					       &error);
  if (!success || error)
    {
      if (error)
	{
	  g_message ("Cannot build popup menu:\n%s", error->message);
	  g_error_free (error);
	  error = NULL;
	}

      exit (EXIT_FAILURE);
    }

  widget = gtk_ui_manager_get_widget
    (ui_manager, "/Popup/SubtitlePopupSubmenu");
  popup_menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));

  item = gtk_ui_manager_get_widget
    (ui_manager, "/Popup/SubtitlePopupSubmenu/SubtitleEncodingSubmenu");
  if (item)
    {
      if (view->pg && IS_TELETEXT_PGNO (view->pg->pgno))
	{
	  GtkMenu *menu;

	  menu = zvbi_create_encoding_menu (on_encoding_menu_toggled, view);
	  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item),
				     GTK_WIDGET (menu));

	  zvbi_encoding_menu_set_active (menu, view->override_charset);
	}
      else
	{
	  gtk_widget_set_sensitive (item, FALSE);
	}
    }

  zvbi_menu_shell_insert_active_subtitle_pages (GTK_MENU_SHELL (popup_menu),
						/* position */ 2,
						view->monitor_pgno,
						/* separator_above */ TRUE,
						/* separator_below */ TRUE);

  gtk_menu_popup (GTK_MENU (popup_menu),
		  /* parent_menu_shell */ NULL,
		  /* parent_menu_item */ NULL,
		  /* menu position func */ NULL,
		  /* menu position data */ NULL,
		  event->button,
		  event->time);
}

/* Events. */

static void
set_position_			(SubtitleView *		view,
				 gdouble		x,
				 gdouble		y)
{
  x = CLAMP (x, -2.0, +2.0);
  y = CLAMP (y, -2.0, +2.0);

  if (fabs (x - view->rel_x) >= 1 / 1024.0
      || fabs (y - view->rel_y) >= 1 / 1024.0)
    {
      view->rel_x = x;
      view->rel_y = y;

      g_signal_emit (view, signals[POSITION_CHANGED], 0);

      update_window (view,
		     /* reposition */ TRUE,
		     /* reshape */ FALSE,
		     /* rescale */ FALSE,
		     /* first_row */ 0,
		     /* last_row */ 0);
    }
}

static void
set_size_			(SubtitleView *		view,
				 gdouble		size)
{
  size = CLAMP (size, 0.1, 2.0);

  if (fabs (size - view->rel_size) >= 1 / 1024.0)
    {
      view->rel_size = size;

      g_signal_emit (view, signals[POSITION_CHANGED], 0);

      /* Reshapes & rescales if necessary. */
      update_window (view,
		     /* reposition */ TRUE,
		     /* reshape */ FALSE,
		     /* rescale */ FALSE,
		     /* first_row */ 0,
		     /* last_row */ 0);
    }
}

/* Note on success you must vbi3_link_destroy. */
static gboolean
link_from_pointer_position	(SubtitleView *		view,
				 vbi3_link *		lk,
				 gint			x,
				 gint			y)
{
  GdkWindow *window;
  const vbi3_page *pg;
  gint width;
  gint height;
  guint row;
  guint column;

  vbi3_link_init (lk);

  if (x < 0 || y < 0)
    return FALSE;

  if (!(pg = view->pg))
    return FALSE;

  if (!(window = view->darea.widget.window))
    return FALSE;

  gdk_window_get_geometry (window,
			   /* x */ NULL,
			   /* y */ NULL,
			   &width,
			   &height,
			   /* depth */ NULL);

  if (width <= 0 || height <= 0)
    return FALSE;

  column = (x * pg->columns) / width;
  row = (y * pg->rows) / height;

  return vbi3_page_get_hyperlink (pg, lk, column, row);
}

static gboolean
motion_notify_event		(GtkWidget *		widget,
				 GdkEventMotion *	event)
{
  SubtitleView *view = SUBTITLE_VIEW (widget);
  gint x;
  gint y;
  GdkModifierType mask;
  vbi3_link link;
  gboolean success;

  event = event;

  if (view->moving | view->scaling)
    {
      gint x;
      gint y;
      GdkModifierType mask;

      gdk_window_get_pointer (gdk_get_default_root_window (), &x, &y, &mask);

      if (x == view->last_mouse_x
	  && y == view->last_mouse_y)
	return FALSE; /* pass on */

      view->last_mouse_x = x;
      view->last_mouse_y = y;

      if (view->moving)
	{
	  view->set_position (view,
			      (view->move_offset_x + x)
			      / (gdouble) view->visibility_bounds.width,
			      (view->move_offset_y + y)
			      / (gdouble) view->visibility_bounds.height);
	}
      else /* scaling */
	{
	  view->set_size (view,
			  sqrt (fabs (x - view->scale_center_x)
				* fabs (y - view->scale_center_y))
			  * view->scale_factor);
	}

      return TRUE; /* handled */
    }

  gdk_window_get_pointer (widget->window, &x, &y, &mask);

  success = link_from_pointer_position (view, &link, x, y);

  if (success)
    {
      switch (link.type)
	{
	case VBI3_LINK_PAGE:
	case VBI3_LINK_SUBPAGE:
	case VBI3_LINK_HTTP:
	case VBI3_LINK_FTP:
	case VBI3_LINK_EMAIL:
	  gdk_window_set_cursor (widget->window, cursor_link);
	  break;

	default:
	  gdk_window_set_cursor (widget->window, cursor_normal);
	  break;
	}

      vbi3_link_destroy (&link);
    }
  else
    {
      gdk_window_set_cursor (widget->window, cursor_normal);
    }

  return TRUE; /* handled */
}

static gboolean
button_release_event		(GtkWidget *		widget,
				 GdkEventButton *	event)
{
  SubtitleView *view = SUBTITLE_VIEW (widget);

  event = event;

  if (view->moving | view->scaling)
    {
      vbi3_page *pg;
      vbi3_pgno pgno;

      view->moving = FALSE;
      view->scaling = FALSE;

      if (0 != (pgno = view->load_pgno))
	{
	  view->load_pgno = 0;

	  view->load_page (view, pgno);
	}
      else if ((pg = view->show_pg))
	{
	  view->show_pg = NULL;

	  view->show_page (view, pg);

	  vbi3_page_unref (pg);
	}
      else if (view->redraw_unscaled_full)
	{
	  redraw_unscaled (view);
	}
      else if (view->redraw_display_scaled)
	{
	  update_window (view,
			 /* reposition */ FALSE,
			 /* reshape */ FALSE,
			 /* rescale */ TRUE,
			 /* first_row */ 0,
			 /* last_row */ view->pg->rows - 1);
	}
    }

  return FALSE; /* pass on */
}

static gint
decimal_subno			(vbi3_subno		subno)
{
  if (0 == subno || (guint) subno > 0x99)
    return -1; /* any */
  else
    return vbi3_bcd2dec (subno);
}

static void
scale_start			(SubtitleView *		view)
{
  gint mouse_x;
  gint mouse_y;
  GdkModifierType mask;
  gint org_x;
  gint org_y;
  gdouble d;

  view->scaling = TRUE;

  gdk_window_get_pointer (gdk_get_default_root_window (),
			  &mouse_x, &mouse_y, &mask);

  view->last_mouse_x = mouse_x;
  view->last_mouse_y = mouse_y;

  gdk_window_get_origin (view->darea.widget.window, &org_x, &org_y);

  view->scale_center_x = org_x + view->darea.widget.allocation.width / 2;
  view->scale_center_y = org_y + view->darea.widget.allocation.height / 2;

  d = sqrt (fabs (mouse_x - view->scale_center_x)
	    * fabs (mouse_y - view->scale_center_y));
  if (d < 10)
    {
      view->scaling = FALSE;
      return;
    }

  view->scale_factor = view->rel_size / d;
}

static void
move_start			(SubtitleView *		view)
{
  gint x;
  gint y;
  GdkModifierType mask;

  view->moving = TRUE;

  gdk_window_get_pointer (gdk_get_default_root_window (), &x, &y, &mask);

  view->last_mouse_x = x;
  view->last_mouse_y = y;

  view->move_offset_x = view->rel_x * view->visibility_bounds.width - x;
  view->move_offset_y = view->rel_y * view->visibility_bounds.height - y;
}

static gboolean
button_press_event		(GtkWidget *		widget,
				 GdkEventButton *	event)
{
  SubtitleView *view = SUBTITLE_VIEW (widget);
  gboolean r;

  r = FALSE; /* pass on */

  switch (event->button)
    {
    case 1: /* left button */
    case 2: /* middle button */
      if (event->state & GDK_SHIFT_MASK)
	{
	  scale_start (view);
	  r = TRUE; /* handled */
	}
      else if (event->state & (GDK_CONTROL_MASK |
			       GDK_MOD1_MASK))
	{
	  move_start (view);
	  r = TRUE;
	}
      else
	{
	  vbi3_link link;
	  gboolean success;

	  success = link_from_pointer_position
	    (view, &link, (gint) event->x, (gint) event->y);

	  if (success)
	    {
	      switch (link.type)
		{
		case VBI3_LINK_PAGE:
		case VBI3_LINK_SUBPAGE:
		  python_command_printf (widget,
					 "zapping.ttx_open_new(%x,%d)",
					 link.pgno,
					 decimal_subno (link.subno));
		  r = TRUE;
		  break;

		case VBI3_LINK_HTTP:
		case VBI3_LINK_FTP:
		case VBI3_LINK_EMAIL:
		  z_url_show (NULL, link.url);
		  r = TRUE;
		  break;

		default:
		  if (1 == event->button)
		    move_start (view);
		  else
		    scale_start (view);

		  r = TRUE;

		  break;
		}

	      vbi3_link_destroy (&link);
	    }
	  else
	    {
	      if (1 == event->button)
		move_start (view);
	      else
		scale_start (view);

	      r = TRUE;
	    }
	}

      break;

    case 3: /* right button */
      create_popup_menu (view, event);
      r = TRUE;
      break;

    default:
      break;
    }

  return r;
}

static gboolean
expose_event			(GtkWidget *		widget,
				 GdkEventExpose	*	event)
{
  SubtitleView *view = SUBTITLE_VIEW (widget);

  if (view->display_scaled.pixbuf
      && view->display_scaled.valid)
    {
      gdk_draw_pixbuf (view->darea.widget.window,
		       view->darea.widget.style->white_gc,
		       view->display_scaled.pixbuf,
		       /* src */ event->area.x, event->area.y,
		       /* dst */ event->area.x, event->area.y,
		       event->area.width,
		       event->area.height,
		       GDK_RGB_DITHER_NONE,
		       /* x_dither */ 0,
		       /* y_dither */ 0);
    }

  return TRUE;
}

/* Initialization. */

static void
instance_finalize		(GObject *		object)
{
  SubtitleView *view = SUBTITLE_VIEW (object);

  subtitle_views = g_list_remove (subtitle_views, view);

  if (NULL != view->vbi)
    vbi3_decoder_remove_event_handler
      (view->vbi, decoder_event_handler, view);

  if (view->region)
    {
      gdk_region_destroy (view->region);
      view->region = NULL;
    }

  if (view->capture_scaled.pixbuf)
    {
      g_object_unref (G_OBJECT (view->capture_scaled.pixbuf));
      view->capture_scaled.pixbuf = NULL;
    }

  if (view->display_scaled.pixbuf)
    {
      g_object_unref (G_OBJECT (view->display_scaled.pixbuf));
      view->display_scaled.pixbuf = NULL;
    }

  if (view->unscaled_pixbuf)
    {
      g_object_unref (G_OBJECT (view->unscaled_pixbuf));
      view->unscaled_pixbuf = NULL;
    }

  vbi3_page_unref (view->show_pg);
  view->show_pg = NULL;

  vbi3_page_unref (view->pg);
  view->pg = NULL;

  parent_class->finalize (object);
}

static void
set_video_bounds_		(SubtitleView *		view,
				 gint			x,
				 gint			y,
				 guint			width,
				 guint			height)
{
  if (0 == width || 0 == height)
    {
      view->have_video_bounds = FALSE;
    }
  else
    {
      view->video_bounds.x = x;
      view->video_bounds.y = y;
      view->video_bounds.width = width;
      view->video_bounds.height = height;
      view->have_video_bounds = TRUE;
    }
}

static void
size_allocate			(GtkWidget *		widget,
				 GtkAllocation *	allocation)
{
  SubtitleView *view = SUBTITLE_VIEW (widget);

  view->visibility_bounds = *allocation;

  /* Reshapes & rescales if necessary. */
  update_window (view,
		 /* reposition */ TRUE,
		 /* reshape */ FALSE,
		 /* rescale */ FALSE,
		 /* first_row */ 0,
		 /* last_row */ 0);
}

static void
size_request			(GtkWidget *		widget,
				 GtkRequisition *	requisition)
{
  widget = widget;

  /* We can scale to almost any size. */
  requisition->width = 16;
  requisition->height = 16;
}

/* We cannot initialize this until the widget (and its
   parent GtkWindow) has a window. */
static void
realize				(GtkWidget *		widget)

{
  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  /* No background, prevents flicker. */
  gdk_window_set_back_pixmap (widget->window, NULL, FALSE);
}

static void
instance_init			(GTypeInstance *	instance,
				 gpointer		g_class)
{
  SubtitleView *view = (SubtitleView *) instance;
  GtkWidget *widget;

  g_class = g_class;

  view->action_group = gtk_action_group_new ("SubtitleViewActions");
#ifdef ENABLE_NLS
  gtk_action_group_set_translation_domain (view->action_group,
					   GETTEXT_PACKAGE);
#endif					   
  gtk_action_group_add_actions (view->action_group,
				actions, G_N_ELEMENTS (actions), view);

  view->rel_x		= 0.5;
  view->rel_y		= 0.5;

  view->rel_size	= 1.0;

  view->roll_enable	= TRUE;

  view->override_charset = VBI3_CHARSET_CODE_NONE;

  view->show_page	= show_page_;
  view->load_page	= load_page_;
  view->monitor_page	= monitor_page_;
  view->start_rolling	= start_rolling_;
  view->get_image	= get_image_;
  view->set_position	= set_position_;
  view->set_size	= set_size_;
  view->set_charset	= set_charset_;
  view->set_rolling	= set_rolling_;
  view->set_video_bounds = set_video_bounds_;

  view->roll_counter	= 13;

  widget = GTK_WIDGET (view);

  gtk_widget_add_events (widget,
			 GDK_EXPOSURE_MASK |		/* redraw */
			 GDK_POINTER_MOTION_MASK |	/* cursor shape */
			 GDK_BUTTON_PRESS_MASK |	/* links, selection */
			 GDK_BUTTON_RELEASE_MASK |	/* selection */
			 0);

  subtitle_views = g_list_append (subtitle_views, view);
}

GtkWidget *
subtitle_view_new		(void)
{
  return GTK_WIDGET (g_object_new (TYPE_SUBTITLE_VIEW, NULL));
}

static void
class_init			(gpointer		g_class,
				 gpointer		class_data)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;

  class_data = class_data;

  object_class = G_OBJECT_CLASS (g_class);
  widget_class = GTK_WIDGET_CLASS (g_class);
  parent_class = g_type_class_peek_parent (g_class);

  object_class->finalize = instance_finalize;

  widget_class->realize			= realize;
  widget_class->size_request		= size_request;
  widget_class->size_allocate		= size_allocate;
  widget_class->expose_event		= expose_event;
  widget_class->button_press_event	= button_press_event;
  widget_class->button_release_event	= button_release_event;
  widget_class->motion_notify_event	= motion_notify_event;

  signals[POSITION_CHANGED] =
    g_signal_new ("z-position-changed",
		  G_TYPE_FROM_CLASS (g_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		  G_STRUCT_OFFSET (SubtitleViewClass, position_changed),
		  /* accumulator */ NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  /* return_type */ G_TYPE_NONE,
		  /* n_params */ 0);

  cursor_normal	= gdk_cursor_new (GDK_FLEUR); /* LEFT_PTR */
  cursor_link	= gdk_cursor_new (GDK_HAND2);

  /* Error ignored */
  z_gconf_notify_add (GCONF_DIR "/default_charset",
		      default_charset_notify, NULL);

  z_gconf_notify_add (GCONF_DIR "/interp_type", interp_type_notify, NULL);

  z_gconf_notify_add (GCONF_DIR "/brightness", redraw_unscaled_notify, NULL);
  z_gconf_notify_add (GCONF_DIR "/contrast", redraw_unscaled_notify, NULL);

  z_gconf_notify_add (GCONF_DIR "/pad", caption_reload_notify, NULL);
  z_gconf_notify_add (GCONF_DIR "/foreground", caption_reload_notify, NULL);
  z_gconf_notify_add (GCONF_DIR "/background", caption_reload_notify, NULL);

  z_gconf_notify_add (GCONF_DIR "/roll", roll_notify, NULL);

  z_gconf_notify_add (GCONF_DIR "/show_dheight",
		      show_dheight_notify, NULL);
}

GType
subtitle_view_get_type		(void)
{
  static GType type = 0;
  
  if (!type)
    {
      GTypeInfo info;

      CLEAR (info);

      info.class_size = sizeof (SubtitleViewClass);
      info.class_init = class_init;
      info.instance_size = sizeof (SubtitleView);
      info.instance_init = instance_init;

      type = g_type_register_static (GTK_TYPE_DRAWING_AREA,
				     "SubtitleView",
				     &info, (GTypeFlags) 0);
    }

  return type;
}
