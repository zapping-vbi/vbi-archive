/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2001 Iñaki García Etxebarria
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
 * OSD routines.
 */

/* XXX gtk+ 2.3 GtkOptionMenu, Gnome font picker, color picker */
/* gdk_input_add/remove */
#undef GTK_DISABLE_DEPRECATED
#undef GNOME_DISABLE_DEPRECATED
#undef GDK_DISABLE_DEPRECATED

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#define ZCONF_DOMAIN "/zapping/options/osd/"
#include "zconf.h"
#include "zmisc.h"
#include "osd.h"
#include "remote.h"
#include "common/math.h"
#include "properties.h"
#include "interface.h"
#include "globals.h"

#define MAX_COLUMNS 48 /* TTX */
#define MAX_ROWS 26 /* 25 for TTX plus one for OSD */

typedef struct _piece {
  /* The drawing area */
  GtkWidget	*da;

  /* The rendered text */
  GdkPixbuf	*unscaled;
  GdkPixbuf	*scaled;

  /* geometry in the osd matrix, interesting for TTX and CC location */
  int		column, row, width, max_rows, max_columns;

  /* relative geometry in the OSD window (0...1) */
  float		x, y, w, h;

  /* sw is the width we scale p->unscaled to. Then the columns in
     "double_columns" are duplicated to build scaled */
  float		sw;

  /* Columns to duplicate, ignored if sw = w */
  int		*double_columns;
  int		num_double_columns;

  /* called before the piece geometry is set to allow changing of
     x, y, w, h */
  void		(*position)(struct _piece *p);
} piece;

typedef struct {
  piece		pieces[MAX_COLUMNS]; /* Set of pieces to show */
  int		n_pieces; /* Pieces in this row */
} row;

static row *matrix[MAX_ROWS];

static GtkWidget *osd_window = NULL;
/* Subrectangle in the osd_window we are drawing to */
static gint cx, cy, cw, ch;

/**
 * Handling of OSD.
 */
static void
paint_piece			(piece		*p,
				 gint x,	gint y,
				 gint w,	gint h)
{
  z_pixbuf_render_to_drawable(p->scaled, p->da->window,
			      p->da->style->white_gc, x, y, w, h);
}

static gboolean
expose				(GtkWidget	*da,
				 GdkEventExpose	*event,
				 piece		*p)
{
  paint_piece(p, event->area.x, event->area.y,
	      event->area.width, event->area.height);

  return TRUE;
}

/**
 * A temporary place to keep child windows without a parent, created
 * by the non-profit routine startup_osd.
 * cf. Futurama
 */
static GtkWidget *orphanarium = NULL;

static void
set_piece_geometry		(piece		*p)
{
  gint dest_x, dest_y, dest_w, dest_h, dest_sw;

  if (p->double_columns)
    g_free(p->double_columns);
  p->double_columns = NULL;
  p->num_double_columns = 0;

  if (p->position)
    {
      p->position(p);
      dest_x = (gint) p->x;
      dest_y = (gint) p->y;
      dest_w = (gint) p->w;
      dest_h = (gint) p->h;
      dest_sw = (gint) p->sw;
    }
  else
    {
      dest_x = (gint)(cx + p->x * cw);
      dest_y = (gint)(cy + p->y * ch);
      dest_sw = dest_w = (gint)(p->w * cw);
      dest_h =(gint)(p->h * ch);
    }

  if (osd_window && ((!p->scaled)  ||
      (gdk_pixbuf_get_width(p->scaled) != dest_w)  ||
      (gdk_pixbuf_get_height(p->scaled) != dest_h)))
    {
      if (p->scaled)
	{
	  g_object_unref (G_OBJECT (p->scaled));
	  p->scaled = NULL;
	}
      if (dest_h > 0)
	{
	  if (dest_w == dest_sw)
	    p->scaled = z_pixbuf_scale_simple(p->unscaled,
					      dest_w, dest_h,
					      GDK_INTERP_BILINEAR);
	  else
	    {
	      GdkPixbuf * canvas = z_pixbuf_scale_simple(p->unscaled,
							 dest_sw, dest_h,
							 GDK_INTERP_BILINEAR);
	      /* Scaling with line duplication */
	      if (canvas)
		{
		  int i, last_column = 0, scaled_x=0;
		  p->scaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
					     dest_w, dest_h);

		  /* do the line duplication */
		  for (i=0; i<p->num_double_columns; i++)
		    {
		      int width;

		      width = p->double_columns[i] - last_column;
		      if (width)
			{
			  z_pixbuf_copy_area(canvas, last_column,
					     0, width, dest_h,
					     p->scaled,
					     scaled_x, 0);
			  scaled_x += width;
			}
		      z_pixbuf_copy_area(canvas, p->double_columns[i], 0,
					 1, dest_h,
					 p->scaled, scaled_x, 0);
		      scaled_x ++;
		      last_column = p->double_columns[i];
		    }
		  /* Copy the remaining */
		  if (p->num_double_columns)
		    {
		      int width = dest_sw - last_column;
		      if (width > 0)
			{
			  z_pixbuf_copy_area(canvas, last_column, 0,
					     width, dest_h,
					     p->scaled, scaled_x, 0);
			  scaled_x += width; /* for checking */
			}
		    }

		  /* We should always draw the whole p->scaled, no
		     more, no less. Otherwise sth is severely b0rken. */
		  g_assert(scaled_x == dest_w);
		  g_object_unref (G_OBJECT (canvas));
		}
	    }
	}
    }

  if (!osd_window)
    {
      if (gdk_window_get_parent(p->da->window) != orphanarium->window)
	gdk_window_reparent(p->da->window, orphanarium->window, 0, 0);
      return;
    }
  else if (osd_window->window != gdk_window_get_parent(p->da->window))
    {
      gdk_window_reparent(p->da->window, osd_window->window, dest_x,
			  dest_y);
      gdk_window_show(p->da->window);
    }

  gdk_window_move_resize(p->da->window,
			 dest_x, dest_y, dest_w, dest_h);

  if (p->scaled)
    paint_piece(p, 0, 0, dest_w, dest_h);
}

/**
 * Call this when:
 *	a) The osd_window parent is changed
 *	b) There's a resize of the osd_window
 */
static void
geometry_update			(void)
{
  int i, j;

  for (i = 0; i < MAX_ROWS; i++)
    if (matrix[i])
      for (j = 0; j < matrix[i]->n_pieces; j++)
	set_piece_geometry(&(matrix[i]->pieces[j]));
}

/**
 * Widget creation/destruction with caching (for reducing flicker)
 */
/* List of destroyed windows for reuse */
static GList *window_stack = NULL;

static GtkWidget *
get_window(void)
{
  GtkWidget *da;

  da = gtk_drawing_area_new();

  gtk_widget_add_events(da, GDK_EXPOSURE_MASK);

  gtk_fixed_put(GTK_FIXED(orphanarium), da, 0, 0);

  gtk_widget_realize(da);
  gdk_window_set_back_pixmap(da->window, NULL, FALSE);

  return da;
}

static void
unget_window(GtkWidget *window)
{
  if (gdk_window_get_parent(window->window) != orphanarium->window)
    gdk_window_reparent(window->window, orphanarium->window, 0, 0);

  gtk_widget_destroy(window);
}

static void
push_window(GtkWidget *window)
{
  window_stack = g_list_append(window_stack, window);
}

/* Unget all windows in the stack */
static void clear_stack (void) __attribute__ ((unused));
static void
clear_stack(void)
{
  GtkWidget *window;

  while (window_stack)
    {
      window = GTK_WIDGET(g_list_last(window_stack)->data);
      unget_window(window);
      window_stack = g_list_remove(window_stack, window);
    }
}

static GtkWidget *
pop_window(void)
{
  GtkWidget *window = NULL;

  if (window_stack)
    {
      window = GTK_WIDGET(g_list_last(window_stack)->data);
      window_stack = g_list_remove(window_stack, window);
    }

  if (!window)
    window = get_window();

  return window;
}

/**
 * Add/delete pieces.
 */
static void
add_piece			(GdkPixbuf	*unscaled,
				 GtkWidget	*da,
				 int col,	int row,
				 int width,	int max_rows,
				 int max_columns,
				 float x,	float y,
				 float w,	float h,
				 void		(*position)(piece *p))
{
  piece *p;

  g_assert(col < MAX_COLUMNS);
  g_assert(row < MAX_ROWS);

  p = matrix[row]->pieces + matrix[row]->n_pieces;
  matrix[row]->n_pieces++;

  CLEAR (*p);

  p->da = da;

  g_signal_connect(G_OBJECT(p->da), "expose-event",
		   G_CALLBACK(expose), p);

  p->x = x;
  p->y = y;
  p->w = w;
  p->h = h;

  p->row = row;
  p->column = col;
  p->width = width;
  p->max_rows = max_rows;
  p->max_columns = max_columns;
  p->position = position;

  p->unscaled = unscaled;

  set_piece_geometry(p);
}

static void
remove_piece			(int		row,
				 int		p_index,
				 gboolean	just_push)
{
  piece *p = &(matrix[row]->pieces[p_index]);

  if (p->scaled)
    g_object_unref (G_OBJECT (p->scaled));
  g_object_unref (G_OBJECT (p->unscaled));

  g_signal_handlers_disconnect_matched (G_OBJECT(p->da),
					G_SIGNAL_MATCH_FUNC |
					G_SIGNAL_MATCH_DATA,
					0, 0, NULL,
					G_CALLBACK (expose), p);
  
  if (!just_push)
    unget_window(p->da);
  else
    push_window(p->da);

  if (p->double_columns)
    g_free(p->double_columns);

  if (p_index != (matrix[row]->n_pieces - 1))
    memcpy(p, p+1,
	   sizeof(piece)*((matrix[row]->n_pieces-p_index)-1));

  matrix[row]->n_pieces--;
}

static void
clear_row			(int		row,
				 gboolean	just_push)
{
  while (matrix[row]->n_pieces)
    remove_piece(row, matrix[row]->n_pieces-1, just_push);
}

void
osd_clear			(void)
{
  int i;

  for (i=0; i<(MAX_ROWS-1); i++)
    clear_row(i, FALSE);

  zmodel_changed(osd_model);
}

static void roll_up(int first_row, int last_row) __attribute__ ((unused));
static void
roll_up(int first_row, int last_row)
{
  gint i;
  float y = 0, h = 0;
  gboolean y_set = FALSE;

  if (first_row >= last_row)
    return;

  /* This code assumes all pieces in a row have the same height */
  if (matrix[first_row]->n_pieces)
    {
      y = matrix[first_row]->pieces[0].y;
      h = matrix[first_row]->pieces[0].h;
      y_set = TRUE;
    }

  clear_row(first_row, FALSE);
  
  for (; first_row < last_row; first_row++)
    {
      float tmp = 0, tmph = 0;
      gboolean tmp_set;

      swap(matrix[first_row], matrix[first_row + 1]);

      if (matrix[first_row]->n_pieces)
	{
	  tmp = matrix[first_row]->pieces[0].y;
	  tmph = matrix[first_row]->pieces[0].h;
	  tmp_set = TRUE;
	}
      else
	tmp_set = FALSE;

      for (i=0; i<matrix[first_row]->n_pieces; i++)
	{
	  piece *p = matrix[first_row]->pieces + i;

	  p->row --;

	  if (y_set)
	    {
	      p->y = y;
	      p->h = h;
	    }
	  else
	    p->y -= p->h;

	  set_piece_geometry(p);
	}

      y = tmp;
      h = tmph;
      y_set = tmp_set;
    }

  zmodel_changed(osd_model);
}

/**
 * OSD sources.
 */

#if HAVE_LIBZVBI
#include "zvbi.h"

static vbi_page osd_page;
extern int osd_pipe[2];
static gint input_id = -1;

static void
ttx_position		(piece		*p)
{
  gint x = 0, y = 0, w = cw, h = ch;

  /* Text area 40x25 is (64, 38) - (703, 537) in a 768x576 screen */
  x += (64*w)/768;
  y += (38*h)/576;
  w -= (128*w)/768;
  h -= (76*h)/576;

  p->sw = p->w = ((p->column+p->width)*w)/p->max_columns
    - (p->column*w)/p->max_columns;
  p->h = ((p->row+1)*h)/p->max_rows-(p->row*h)/p->max_rows;

  p->x = x + (p->column*w)/p->max_columns;
  p->y = y + (p->row*h)/p->max_rows;
}

static void
cc_position		(piece		*p)
{
  gint x = 0, y = 0, w = cw, h = ch;
  gint width0 /* min width of each char */,
    extra /* pixels remaining for completing total width */,
    total /* total width of the line */;
  gint i, j=0 /* internal checking */;

  /* Text area 34x15 is (48, 45) - (591, 434) in a 640x480 screen */
  x += (48*w)/640;
  y += (45*h)/480;
  w -= (96*w)/640;
  h -= (90*h)/480;

  total = w;
  width0 = total/p->max_columns;
  extra = total - width0*p->max_columns;

  p->num_double_columns = MIN(MAX(0, extra - p->column), p->width);
  if (p->num_double_columns)
    p->double_columns = (int *)
      g_malloc(p->num_double_columns * sizeof(p->double_columns[0]));

  p->w = 0;
  p->x = x;
  for (i=0; i<p->column+p->width; i++)
    {
      int w = width0 + (i<extra);

      if ((i-p->column) < p->num_double_columns &&
	  (i-p->column) >= 0)
	{
	  p->double_columns[i-p->column] = (i-p->column)*width0;
	  j++;
	}

      p->w += w * (i >= p->column) * (i < (p->column+p->width));
      p->x += w * (i < p->column);
    }

  g_assert(j == p->num_double_columns);

  p->sw = width0 * p->width;
  p->h = ((p->row+1)*h)/p->max_rows-(p->row*h)/p->max_rows;

  p->y = y + (p->row*h)/p->max_rows;
}

static void
add_piece_vbi		(int col, int row, int width)
{
  GdkPixbuf *buf;
  GtkWidget *da;

  da = pop_window();

  if (osd_page.columns < 40) /* naive cc test */
    {
      buf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
			   16 * width, 26);
      vbi_draw_cc_page_region(&osd_page,
			      VBI_PIXFMT_RGBA32_LE,
			      (uint32_t *) gdk_pixbuf_get_pixels(buf),
			      gdk_pixbuf_get_rowstride(buf),
			      col, row, width, 1 /* height */);
    }
  else
    {
      buf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
			   12 * width, 10);
      vbi_draw_vt_page_region(&osd_page,
			      VBI_PIXFMT_RGBA32_LE,
			      (uint32_t *) gdk_pixbuf_get_pixels(buf),
			      gdk_pixbuf_get_rowstride(buf),
			      col, row, width, 1 /* height */,
			      1 /* reveal */, 1 /* flash_on */);
    }

  add_piece(buf, da, col, row, width, osd_page.rows,
	    osd_page.columns, 0, 0, 0, 0,
	    (osd_page.columns < 40) ? cc_position : ttx_position);
}

static void
render_page		(void)
{
  vbi_char *ac_row;
  int row, i, j;
  gboolean dirty = FALSE;

  row = osd_page.dirty.y0;
  ac_row = osd_page.text + row * osd_page.columns;

  for (; row <= osd_page.dirty.y1; ac_row += osd_page.columns, row++)
    {
      clear_row(row, TRUE);
      for (i = j = 0; i < osd_page.columns; i++)
        {
	  if (ac_row[i].opacity != VBI_TRANSPARENT_SPACE)
	    j++;
	  else if (j > 0)
	    {
	      add_piece_vbi(i - j, row, j);
	      dirty = TRUE;
	      j = 0;
	    }
	}

      if (j)
	{
	  dirty = TRUE;
	  add_piece_vbi(osd_page.columns - j, row, j);
	}
    }

  clear_stack();

  if (dirty)
    zmodel_changed(osd_model);
}

static void
osd_event		(gpointer	   data,
			 gint              source, 
			 GdkInputCondition condition)
{
  vbi_decoder *vbi = zvbi_get_object();
  char dummy[16];
  extern vbi_pgno zvbi_page;
  extern vbi_subno zvbi_subpage;

  if (!vbi)
    return;

  if (read(osd_pipe[0], dummy, 16 /* flush */) <= 0)
    return;

  if (!zconf_get_boolean(NULL, "/zapping/internal/callbacks/closed_caption"))
    return;

  if (zvbi_page <= 8)
    {
      if (!vbi_fetch_cc_page(vbi, &osd_page, zvbi_page, TRUE))
        return; /* trouble in outer space */
    }
  else
    {
      if (!vbi_fetch_vt_page(vbi, &osd_page, zvbi_page, zvbi_subpage,
			     zvbi_teletext_level(), 25 /* rows */, TRUE /* nav */))
        return;
    }

  if (osd_page.dirty.y0 > osd_page.dirty.y1)
    return; /* not dirty (caption only) */

  if (abs(osd_page.dirty.roll) >= osd_page.rows)
    {
      osd_clear();
      return;
    }

  if (osd_page.dirty.roll == -1)
    {
      roll_up(osd_page.dirty.y0, osd_page.dirty.y1);
      return;
    }

  g_assert(osd_page.dirty.roll == 0);
    /* currently never down or more than one row */

  render_page();
}
#endif /* HAVE_LIBZVBI */

#include <libxml/parser.h>

static guint osd_clear_timeout_id = -1;
static void (* osd_clear_timeout_cb)(gboolean);

#define OSD_ROW (MAX_ROWS - 1)

static gint
osd_clear_timeout	(void		*ignored)
{
  clear_row(OSD_ROW, FALSE);

  osd_clear_timeout_id = -1;

  zmodel_changed(osd_model);

  if (osd_clear_timeout_cb)
    osd_clear_timeout_cb(TRUE);

  return FALSE;
}

/**
 * Given the rgb and the colormap creates a suitable color that should
 * be unref by unref_color
 */
static
GdkColor *create_color	  (float r, float g, float b,
			   GdkColormap *cmap)
{
  GdkColor *ret = g_malloc0(sizeof(GdkColor));

  ret->red = r*65535;
  ret->green = g*65535;
  ret->blue = b*65535;

  gdk_colormap_alloc_color(cmap, ret, FALSE, TRUE);

  return ret;
}

/**
 * Decreases the reference count of the given color, allocated with
 * create_color.
 */
static void
unref_color		(GdkColor *color, GdkColormap *cmap)
{
  gdk_colormap_free_colors(cmap, color, 1);
  g_free(color);
}

static void my_characters (void *ptr,
			   const xmlChar *ch, int n)
{
  gchar *buf;
  gint i, j;

  buf = g_malloc0(n*sizeof(char) + 1);

  /* Skip line breaks */
  for (i=j=0; i<n; i++)
    if (ch[i] != '\n' && ch[i] != '\r')
      buf[j++] = ch[i];

  buf[j] = 0;

  if (j > 0)
    printf ("%s", buf);

  g_free (buf);
}

/* Render in text mode, throw away markup */
static void
osd_render_markup_text	(gchar *buf)
{
  gchar *buf2;
  xmlSAXHandler handler;

  CLEAR (handler);

  handler.startElement = NULL;
  handler.endElement = NULL;
  handler.characters = my_characters;

  buf2 = g_strdup_printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
			 "<doc><text>%s</text></doc>    ",
			 buf);

  if (xmlSAXUserParseMemory(&handler, NULL, buf2, strlen(buf2)))
    {
      fprintf (stderr, "Cannot parse, printing literally:\n");
      printf ("%s\n", buf2);
    }

  g_free (buf2);
}

/* Render in OSD mode. Put markup to TRUE if the text contains pango
   markup to be interpreted. */
static void
osd_render_osd		(void (*timeout_cb)(gboolean),
			 const gchar *src, gboolean markup)
{
  GdkDrawable *canvas;
  GtkWidget *patch = pop_window ();
  PangoContext *context = gtk_widget_get_pango_context (patch);
  PangoLayout *layout = pango_layout_new (context);
  PangoFontDescription *pfd = NULL;
  GdkGC *gc = gdk_gc_new (patch->window);
  GdkColormap *cmap = gdk_drawable_get_colormap (patch->window);
  const gchar *fname = zcg_char (NULL, "font");
  PangoRectangle logical, ink;
  GdkColor *bg;
  gint w, h;
  gfloat rx, ry, rw, rh;
  gchar *buf = g_strdup_printf ("<span foreground=\"#%02x%02x%02x\">%s</span>",
				(int)(zcg_float(NULL, "fg_r")*255),
				(int)(zcg_float(NULL, "fg_g")*255),
				(int)(zcg_float(NULL, "fg_b")*255), src);

  /* First check that the selected font is valid */
  if (!fname || !*fname ||
      !(pfd = pango_font_description_from_string (fname)))
    {
      if (!fname || !*fname)
	ShowBox(_("Please configure a font for OSD in\n"
		  "Properties/General/OSD"),
		GTK_MESSAGE_ERROR);
      else
	ShowBox(_("The configured font <%s> cannot be loaded, please"
		  " select another one in\n"
		  "Properties/General/OSD"),
		GTK_MESSAGE_ERROR, fname);

      /* Some common fonts */
      pfd = pango_font_description_from_string ("Arial 36");
      if (!pfd)
	pfd = pango_font_description_from_string ("Sans 36");
      if (!pfd)
	{
	  ShowBox(_("No fallback font could be loaded, make sure\n"
		    "your system is properly configured."),
		  GTK_MESSAGE_ERROR);
	  unget_window (patch);
	  g_object_unref (G_OBJECT (layout));
	  g_object_unref (G_OBJECT (gc));
	  g_free (buf);
	  return;
	}
    }

  pango_layout_set_font_description (layout, pfd);

  if (markup)
    pango_layout_set_markup (layout, buf, -1);
  else
    pango_layout_set_text (layout, buf, -1);

  /* Get the text extents, compute the geometry and build the canvas */
  pango_layout_get_pixel_extents (layout, &ink, &logical);

  w = logical.width;
  h = logical.height;

  canvas = gdk_pixmap_new (patch->window, w, h, -1);

  /* Draw the canvas contents */
  bg = create_color(zcg_float(NULL, "bg_r"), zcg_float(NULL, "bg_g"),
		    zcg_float(NULL, "bg_b"), cmap);

  gdk_gc_set_foreground(gc, bg);
  gdk_draw_rectangle(canvas, gc, TRUE, 0, 0, w, h);
  unref_color(bg, cmap);

  gdk_draw_layout (canvas, gc, 0, 0, layout);

  /* Compute the resulting patch geometry */
  rh = 0.1;
  rw = (rh*w)/h;

  if (rw >= 0.9)
    {
      rw = 0.9;
      rh = (rw*h)/w;
    }

  rx = 1 - rw;
  ry = 1 - rh;

  /* Create the patch */
  add_piece(gdk_pixbuf_get_from_drawable
	    (NULL, canvas, cmap, 0, 0, 0, 0, w, h),
	    patch, 0, OSD_ROW, 0, 0, 0, rx, ry, rw, rh, NULL);

  zmodel_changed(osd_model);

  /* Schedule the destruction of the patch */
  if (osd_clear_timeout_id > -1)
    {
      if (osd_clear_timeout_cb)
	osd_clear_timeout_cb(FALSE);

      g_source_remove (osd_clear_timeout_id);
    }

  osd_clear_timeout_id =
    g_timeout_add (zcg_float (NULL, "timeout") * 1000,
		   (GSourceFunc) osd_clear_timeout, NULL);

  osd_clear_timeout_cb = timeout_cb;

  /* Cleanup */
  pango_font_description_free (pfd);
  g_object_unref (G_OBJECT (layout));
  g_object_unref (G_OBJECT (canvas));
  g_object_unref (G_OBJECT (gc));
  g_free (buf);
}

/* If given, timeout_cb(TRUE) is called when osd timed out,
   timeout_cb(FALSE) when error, replaced.
*/
void
osd_render_markup (void (*timeout_cb)(gboolean),
		   const char *string, ...)
{
  gchar *buf;
  va_list args;

  if (!string || !string[0])
    goto failed;

  va_start(args, string);
  buf = g_strdup_vprintf(string, args);
  va_end(args);

  if (!buf)
    goto failed;

  if (!buf[0])
    {
      g_free(buf);
      goto failed;
    }

  /* The different ways of drawing */
  switch (zcg_int(NULL, "osd_type"))
    {
    case 0: /* OSD */
      clear_row(OSD_ROW, TRUE);
      osd_render_osd (timeout_cb, buf, TRUE);
      break;
    case 1: /* Statusbar */
      z_status_print_markup (buf, zcg_float(NULL, "timeout")*1000);
      break;
    case 2: /* Console */
      osd_render_markup_text (buf);
      printf("\n");
      break;
    case 3:
      break; /* Ignore */
    default:
      g_assert_not_reached();
      break;
    }

  g_free(buf);

  return;

 failed:

  if (timeout_cb)
    timeout_cb(FALSE);

  return;
}

void
osd_render		(void (*timeout_cb)(gboolean),
			 const char *string, ...)
{
  gchar *buf;
  va_list args;

  if (!string || !string[0])
    goto failed;

  va_start(args, string);
  buf = g_strdup_vprintf(string, args);
  va_end(args);

  if (!buf)
    goto failed;

  if (!buf[0])
    {
      g_free(buf);
      goto failed;
    }

  /* The different ways of drawing */
  switch (zcg_int(NULL, "osd_type"))
    {
    case 0: /* OSD */ 
      clear_row(OSD_ROW, TRUE);
      osd_render_osd (timeout_cb, buf, FALSE);
      break;
    case 1: /* Statusbar */
      z_status_print (buf, zcg_float(NULL, "timeout")*1000);
      break;
    case 2: /* Console */
      printf("%s\n", buf);
      break;
    case 3:
      break; /* Ignore */
    default:
      g_assert_not_reached();
      break;
    }

  g_free(buf);

  return;

 failed:

  if (timeout_cb)
    timeout_cb(FALSE);

  return;
}

/**
 * Interaction with the OSD window.
 */
gboolean coords_mode = FALSE;

static
void on_osd_screen_size_allocate	(GtkWidget	*widget,
					 GtkAllocation	*allocation,
					 gpointer	ignored)
{
  if (cw == allocation->width &&
      ch == allocation->height)
    return;

  cw = allocation->width;
  ch = allocation->height;

  geometry_update();
}

static void
set_window(GtkWidget *dest_window, gboolean _coords_mode)
{
  if (osd_window && !coords_mode)
    g_signal_handlers_disconnect_matched (G_OBJECT(osd_window),
					  G_SIGNAL_MATCH_FUNC |
					  G_SIGNAL_MATCH_DATA,
					  0, 0, NULL,
					  G_CALLBACK
					  (on_osd_screen_size_allocate),
					  NULL);

  osd_window = dest_window;

  coords_mode = _coords_mode;

  if (!coords_mode)
    g_signal_connect(G_OBJECT(dest_window), "size-allocate",
		     G_CALLBACK(on_osd_screen_size_allocate),
		     NULL);

  geometry_update();
}

void
osd_set_window(GtkWidget *dest_window)
{
  gtk_widget_realize(dest_window);

  cx = cy = 0;
  gdk_drawable_get_size(dest_window->window, &cw, &ch);
  
  set_window(dest_window, FALSE);
}

void
osd_set_coords(GtkWidget *dest_window,
	       gint x, gint y, gint w, gint h)
{
  cx = x;
  cy = y;
  cw = w;
  ch = h;

  set_window(dest_window, TRUE);
}

void
osd_unset_window(void)
{
  if (!osd_window)
    return;

  if (!coords_mode)
    g_signal_handlers_disconnect_matched (G_OBJECT(osd_window),
					  G_SIGNAL_MATCH_FUNC |
					  G_SIGNAL_MATCH_DATA,
					  0, 0, NULL,
					  G_CALLBACK (on_osd_screen_size_allocate),
					  NULL);

  osd_window = NULL;
  geometry_update(); /* Reparent to the orphanarium */
}

/* Python wrappers for the OSD renderer */
static PyObject* py_osd_render (PyObject *self, PyObject *args)
{
  char *string;
  int ok = PyArg_ParseTuple (args, "s", &string);

  if (!ok)
    g_error ("zapping.osd_render(s)");

  osd_render (NULL, string);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject* py_osd_render_markup (PyObject *self, PyObject *args)
{
  char *string;
  int ok = PyArg_ParseTuple (args, "s", &string);

  if (!ok)
    g_error ("zapping.osd_render_markup(s)");

  osd_render_markup (NULL, string);

  Py_INCREF(Py_None);
  return Py_None;
}

static void
on_osd_type_changed	(GtkWidget	*widget,
			 GtkWidget	*page)
{
  GtkWidget *w;

  gboolean sensitive1 = FALSE;
  gboolean sensitive2 = FALSE;

  widget = lookup_widget(widget, "optionmenu22");

  switch (z_option_menu_get_active (widget))
    {
    case 0:
      sensitive1 = TRUE;
    case 1:
      sensitive2 = TRUE;
      break;
    default:
      break;
    }

  /* XXX Ugh. */

  w = lookup_widget (widget, "general-osd-font-label");
  gtk_widget_set_sensitive (w, sensitive1);
  w = lookup_widget (widget, "general-osd-font-selector");
  gtk_widget_set_sensitive (w, sensitive1);
  w = lookup_widget (widget, "general-osd-foreground-label");
  gtk_widget_set_sensitive (w, sensitive1);
  w = lookup_widget (widget, "general-osd-foreground-selector");
  gtk_widget_set_sensitive (w, sensitive1);
  w = lookup_widget (widget, "general-osd-background-label");
  gtk_widget_set_sensitive (w, sensitive1);
  w = lookup_widget (widget, "general-osd-background-selector");
  gtk_widget_set_sensitive (w, sensitive1);

  w = lookup_widget (widget, "general-osd-timeout-label");
  gtk_widget_set_sensitive (w, sensitive2);
  w = lookup_widget (widget, "general-osd-timeout-selector");
  gtk_widget_set_sensitive (w, sensitive2);
}

/* OSD properties */
static void
osd_setup		(GtkWidget	*page)
{
  GtkWidget *widget;

  /* OSD type */
  widget = lookup_widget(page, "optionmenu22");
  gtk_option_menu_set_history(GTK_OPTION_MENU(widget),
			      zcg_int(NULL, "osd_type"));
  on_osd_type_changed (page, page);

  g_signal_connect(G_OBJECT (widget), "changed",
		   G_CALLBACK(on_osd_type_changed),
		   page);

  /* OSD font */
  widget = lookup_widget(page, "general-osd-font-selector");
  if (zcg_char(NULL, "font"))
    gnome_font_picker_set_font_name(GNOME_FONT_PICKER(widget),
				    zcg_char(NULL, "font"));

  /* OSD foreground color */
  widget = lookup_widget(page, "general-osd-foreground-selector");
  gnome_color_picker_set_d(GNOME_COLOR_PICKER(widget),
		   zcg_float(NULL, "fg_r"),
		   zcg_float(NULL, "fg_g"),
		   zcg_float(NULL, "fg_b"),
		   0);

  /* OSD background color */
  widget = lookup_widget(page, "general-osd-background-selector");
  gnome_color_picker_set_d(GNOME_COLOR_PICKER(widget),
		   zcg_float(NULL, "bg_r"),
		   zcg_float(NULL, "bg_g"),
		   zcg_float(NULL, "bg_b"),
		   0);

  /* OSD timeout in seconds */
  widget = lookup_widget(page, "spinbutton2");
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget),
			    zcg_float(NULL, "timeout"));
}

static void
osd_apply		(GtkWidget	*page)
{
  GtkWidget *widget;
  gdouble r, g, b, a;

  widget = lookup_widget(page, "optionmenu22"); /* osd type */
  zcs_int(z_option_menu_get_active(widget),
	  "osd_type");

  widget = lookup_widget(page, "general-osd-font-selector");
  zcs_char(gnome_font_picker_get_font_name(GNOME_FONT_PICKER(widget)),
	   "font");

  widget = lookup_widget(page, "general-osd-foreground-selector");
  gnome_color_picker_get_d(GNOME_COLOR_PICKER(widget), &r, &g, &b,
			   &a);
  zcs_float(r, "fg_r");
  zcs_float(g, "fg_g");
  zcs_float(b, "fg_b");

  widget = lookup_widget(page, "general-osd-background-selector");
  gnome_color_picker_get_d(GNOME_COLOR_PICKER(widget), &r, &g, &b,
			   &a);
  zcs_float(r, "bg_r");
  zcs_float(g, "bg_g");
  zcs_float(b, "bg_b");

  widget = lookup_widget(page, "spinbutton2"); /* osd timeout */
  zcs_float(gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)),
	    "timeout");

}

static void
add				(GtkDialog *		dialog)
{
  SidebarEntry general_options[] = {
    { N_("OSD"), "gnome-oscilloscope.png",
      "general-osd-table", osd_setup, osd_apply }
  };
  SidebarGroup groups[] = {
    { N_("General Options"), general_options, acount (general_options) }
  };


  standard_properties_add (dialog, groups, acount (groups),
			   "zapping.glade2");
}

/**
 * Shutdown/startup of the OSD engine
 */
ZModel			*osd_model = NULL;

void
startup_osd(void)
{
  int i;
  GtkWidget *toplevel;
  property_handler osd_handler = {
    add: add
  };

  for (i = 0; i<MAX_ROWS; i++)
    matrix[i] = g_malloc0(sizeof(row));

#ifdef HAVE_LIBZVBI
  input_id = gdk_input_add(osd_pipe[0], GDK_INPUT_READ,
			   osd_event, NULL);
  CLEAR (osd_page);
#endif

  osd_model = ZMODEL(zmodel_new());

  zcc_int(0, "Which kind of OSD should be used", "osd_type");
  zcc_char("times new roman Bold 36", "Default font", "font");

  zcc_float(1.0, "Default fg r component", "fg_r");
  zcc_float(1.0, "Default fg g component", "fg_g");
  zcc_float(1.0, "Default fg b component", "fg_b");

  zcc_float(0.0, "Default bg r component", "bg_r");
  zcc_float(0.0, "Default bg g component", "bg_g");
  zcc_float(0.0, "Default bg b component", "bg_b");

  zcc_float(1.5, "Seconds the OSD text stays on screen", "timeout");

  orphanarium = gtk_fixed_new();
  gtk_widget_set_size_request (orphanarium, 828, 271);
  toplevel = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_container_add(GTK_CONTAINER(toplevel), orphanarium);
  gtk_widget_show(orphanarium);
  gtk_widget_realize(orphanarium);
  /* toplevel will never be shown */

  /* Add Python interface to our routines */
  cmd_register ("osd_render_markup", py_osd_render_markup, METH_VARARGS);
  cmd_register ("osd_render", py_osd_render, METH_VARARGS);

  /* Register our properties handler */
  prepend_property_handler (&osd_handler);
}

void
shutdown_osd(void)
{
  int i;

  osd_clear();
  clear_row(OSD_ROW, FALSE);

  for (i = 0; i<MAX_ROWS; i++)
    g_free(matrix[i]);

#ifdef HAVE_LIBZVBI
  gdk_input_remove(input_id);
#endif

  g_object_unref (G_OBJECT (osd_model));
  osd_model = NULL;
}
