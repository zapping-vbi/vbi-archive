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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_LIBZVBI

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define ZCONF_DOMAIN "/zapping/options/osd/"
#include "zconf.h"
#include "zmisc.h"
#include "osd.h"
#include "common/math.h"
#include "common/ucs-2.h"

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
	  gdk_pixbuf_unref(p->scaled);
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
		  gdk_pixbuf_unref(canvas);
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

  memset(p, 0, sizeof(*p));

  p->da = da;

  gtk_signal_connect(GTK_OBJECT(p->da), "expose-event",
		     GTK_SIGNAL_FUNC(expose), p);

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
    gdk_pixbuf_unref(p->scaled);
  gdk_pixbuf_unref(p->unscaled);

  gtk_signal_disconnect_by_func(GTK_OBJECT(p->da),
				GTK_SIGNAL_FUNC(expose), p);
  
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

#include "zvbi.h"

static vbi_page osd_page;
extern int osd_pipe[2];

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
  extern int zvbi_page, zvbi_subpage;

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

#if 0
  {
    int row, col;

    fprintf (stderr, "osd_event page=%d dirty.y0=%d .y1=%d .roll=%d\n",
      osd_page.pgno, osd_page.dirty.y0, osd_page.dirty.y1, osd_page.dirty.roll);
    for (row = osd_page.dirty.y0; row <= osd_page.dirty.y1; row++)
      {
        for (col = 0; col < osd_page.columns; col++)
          fputc (osd_page.text[row * osd_page.columns + col].glyph & 0x7F, stderr);
        fputc ('\n', stderr);
      }
  }
#endif

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

/* OSD text provided by the app */
#include <parser.h> /* libxml */

#define ATTR_STACK	128
typedef struct {
  unsigned int		italic;
  unsigned int		bold;
  unsigned int		flash;
  unsigned int		underline;
  unsigned int		fg[ATTR_STACK];
  unsigned int		fg_sp;
  unsigned int		opacity[ATTR_STACK];
  unsigned int		opacity_sp;

  /* For drawing in OSD mode */
  unsigned int		x, baseline; /* paint pos */
  unsigned int		ascent, descent; /* maximum used */
  GdkPixmap		*canvas;
  GdkGC			*gc;

  /* Other OSD fields */
  GdkColormap		*cmap;
  GtkWidget		*patch;
  unsigned int		start_x;
  int			lbearing;

  /* Status bar mode */
  gchar			*dest_buf;

} sax_context;

#define sec(label, entry, entry_sp) \
else if (!strcasecmp(name, #label) && ctx->entry_sp < (ATTR_STACK-1)) \
  ctx->entry[++ctx->entry_sp] = VBI_##label

static void
my_startElement (void *ptr,
		 const xmlChar *name, const xmlChar **atts)
{
  sax_context *ctx = (sax_context *) ptr;

  if (!strcasecmp(name, "i"))
    ctx->italic ++;
  else if (!strcasecmp(name, "b"))
    ctx->bold++;
  else if (!strcasecmp(name, "u"))
    ctx->underline++;
  else if (!strcasecmp(name, "f"))
    ctx->flash++;

  sec(TRANSPARENT_SPACE, opacity, opacity_sp);
  sec(TRANSPARENT_FULL, opacity, opacity_sp);
  sec(SEMI_TRANSPARENT, opacity, opacity_sp);
  sec(OPAQUE, opacity, opacity_sp);

  sec(BLACK, fg, fg_sp);
  sec(RED, fg, fg_sp);
  sec(GREEN, fg, fg_sp);
  sec(YELLOW, fg, fg_sp);
  sec(BLUE, fg, fg_sp);
  sec(MAGENTA, fg, fg_sp);
  sec(CYAN, fg, fg_sp);
  sec(WHITE, fg, fg_sp);
}

#define eec(label, entry_sp) \
else if (!strcasecmp(name, #label) && ctx->entry_sp) \
ctx->entry_sp--

static void my_endElement (void *ptr,
			   const xmlChar *name)
{
  sax_context *ctx = (sax_context *) ptr;

  if (!strcasecmp(name, "i") && ctx->italic)
    ctx->italic--;

  eec(b, bold);
  eec(u, underline);
  eec(f, flash);

  eec(TRANSPARENT_SPACE, opacity_sp);
  eec(TRANSPARENT_FULL, opacity_sp);
  eec(SEMI_TRANSPARENT, opacity_sp);
  eec(OPAQUE, opacity_sp);

  eec(BLACK, fg_sp);
  eec(RED, fg_sp);
  eec(GREEN, fg_sp);
  eec(YELLOW, fg_sp);
  eec(BLUE, fg_sp);
  eec(MAGENTA, fg_sp);
  eec(CYAN, fg_sp);
  eec(WHITE, fg_sp);
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

/* Takes foreground and produces a suitable color */
static GdkColor *
create_color_by_id	(int color_no,
			 GdkColormap *cmap)
{
  GdkColor c, *returned_color;
  const char *colors[] =
  {
    "black", "red", "green", "yellow", "blue", "magenta", "cyan",
    "white"
  };
  const char *color;
  
  if (color_no < 0 || color_no >= acount(colors))
    color = "white";
  else
    color = colors[color_no];
  
  if (gdk_color_parse(color, &c))
    returned_color = create_color(c.red / 65535.0,
				  c.green / 65535.0,
				  c.blue / 65535.0, cmap);
  else
    returned_color = create_color(1.0, 1.0, 1.0, cmap);
  
  return returned_color;
}

/**
 * Creates the given font from the given template, returns NULL on
 * error or a GdkFont* to be freed with unref_font.
 */
static GdkFont *
create_font		(const		gchar *_xlfd,
			 gboolean	bold,
			 gboolean	italic)
{
  const gchar *xlfd = _xlfd;
  gchar *buf = g_malloc0(strlen(xlfd)+30), *p=buf;
  GdkFont *result;

  if (!*xlfd)
    return NULL;

  /* Skip foundry and family */
  *(p++) = *(xlfd++);
  while (*xlfd && *xlfd != '-')
    *(p++) = *(xlfd++);
  *(p++) = '-';
  xlfd++;

  while (*xlfd && *xlfd != '-')
    *(p++) = *(xlfd++);
  *(p++) = '-';
  xlfd++;

  /* Weight and Slant */
  p += sprintf(p, "%s-%s", bold? "bold" : "medium", italic ? "i" : "r");

  while (*xlfd && *xlfd != '-')
    xlfd++;
  xlfd++;
  while (*xlfd && *xlfd != '-')
    xlfd++;

  /* the rest of the xlfd */
  strcpy(p, xlfd);

  result = gdk_font_load(buf);

  if (!result)
    result = gdk_font_load(_xlfd);

  g_free(buf);

  if (result)
    gdk_font_ref(result);

  return result;
}

/**
 * Call this when you don't need the font any longer
 */
static void
unref_font		(GdkFont *font)
{
  gdk_font_unref(font);
}

static void my_characters (void *ptr,
			   const xmlChar *ch, int n)
{
  sax_context *ctx = ptr;
  gboolean underline, bold, italic /*, flash : ignored */;
  /* int opacity; :ignored */
  int foreground;
  GdkColor *fg;
  GdkColor *bg;
  GdkFont *font;
  gchar *buf;
  gint i, j;
  gint lbearing, rbearing, width, ascent, descent;

  buf = g_malloc0(n*sizeof(char) + 1);

  for (i=j=0; i<n; i++)
    if (ch[i] != '\n' && ch[i] != '\r')
      buf[j++] = ch[i];

  buf[j] = 0;

  if (!j)
    goto done;

  underline = ctx->underline > 0;
  bold = ctx->bold > 0;
  italic = ctx->italic > 0;
  /* flash = ctx->flash > 0; */
  /* c.opacity = ctx->opacity_sp ? ctx->opacity[ctx->opacity_sp] :
     OPAQUE; */
  foreground = ctx->fg_sp ? ctx->fg[ctx->fg_sp] : -1;

  switch (zcg_int(NULL, "osd_type"))
    {
    case 1:
      {
	gchar *buf2 = g_strconcat(ctx->dest_buf, buf, NULL);
	g_free(ctx->dest_buf);
	ctx->dest_buf = buf2;
      }
      goto done;
    case 2:
      printf("%s", buf);
      goto done;
    default:
      break;
    }
  
  bg = create_color(zcg_float(NULL, "bg_r"), zcg_float(NULL, "bg_g"),
		    zcg_float(NULL, "bg_b"), ctx->cmap);

  if (foreground >= 0)
    fg = create_color_by_id(foreground, ctx->cmap);
  else
    fg = create_color(zcg_float(NULL, "fg_r"), zcg_float(NULL, "fg_g"),
		      zcg_float(NULL, "fg_b"), ctx->cmap);

  font = create_font(zcg_char(NULL, "font"), bold, italic);

  g_assert(font != NULL);

  /* Draw and increment the cursor pos */
  gdk_gc_set_background(ctx->gc, bg);
  gdk_gc_set_foreground(ctx->gc, fg);

  gdk_draw_string(ctx->canvas, font, ctx->gc,
		  ctx->x, ctx->baseline, buf);

  /* update x, ascent, descent */
  gdk_string_extents(font, buf, &lbearing, &rbearing, &width, &ascent,
		     &descent);

  if (underline)
    gdk_draw_line(ctx->canvas, ctx->gc, ctx->x, ctx->baseline+1,
		  ctx->x+width-1, ctx->baseline+1);

  ctx->x += width;
  ctx->ascent = MAX(ctx->ascent, ascent);
  ctx->descent = MAX(ctx->descent, descent);

  if (ctx->lbearing < 0)
    ctx->lbearing = lbearing;

  unref_font(font);
  unref_color(fg, ctx->cmap);
  unref_color(bg, ctx->cmap);

 done:
  g_free(buf);
}

static gboolean
prepare_context		(sax_context	*ctx,
			 const gchar	*buf)
{
  GdkFont *font;
  gchar *fname = zcg_char(NULL, "font");
  gint lbearing, rbearing, width, ascent, descent;
  gint w, h;
  GdkColor *bg;

  /* First check that the selected font is valid */
  if (!fname || !*fname ||
      !(font = create_font(fname, TRUE, TRUE)))
    {
      if (!fname || !*fname)
	ShowBox(_("Please configure a font for OSD in\n"
		  "Properties/General/OSD"),
		GNOME_MESSAGE_BOX_ERROR);
      else
	ShowBox(_("The configured font <%s> cannot be loaded, please"
		  " select another one in\n"
		  "Properties/General/OSD"),
		GNOME_MESSAGE_BOX_ERROR, fname);
      
      return FALSE;
    }

  ctx->patch = pop_window();

  ctx->gc = gdk_gc_new(ctx->patch->window);
  g_assert(ctx->gc != NULL);
  ctx->cmap = gdk_window_get_colormap(ctx->patch->window);

  gdk_string_extents(font, buf, &lbearing, &rbearing, &width, &ascent,
		     &descent);

  ctx->lbearing = -1;

  /*
    We create a pixmap much bigger than strictly neeeded, we'll only
    convert the appropiate rectangle in process_contents, where we
    know for sure the dimensions. We save some complexity this way at
    the cost of performance (not noticeable anyway).
  */
  w = width*2.5;
  ctx->start_x = ctx->x = width/4;
  h = (ascent+descent)*4;
  ctx->baseline = ascent*2;

  ctx->canvas = gdk_pixmap_new(ctx->patch->window, w, h, -1);
  g_assert(ctx->canvas);

  bg = create_color(zcg_float(NULL, "bg_r"), zcg_float(NULL, "bg_g"),
		    zcg_float(NULL, "bg_b"), ctx->cmap);
  gdk_gc_set_foreground(ctx->gc, bg);
  gdk_draw_rectangle(ctx->canvas, ctx->gc, TRUE, 0, 0, w, h);
  unref_color(bg, ctx->cmap);

  unref_font(font);
  return TRUE;
}

static gint osd_clear_timeout_id = -1;
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

/* Finish the patch by creating the gdk pixbuf, etc */
static void
process_context		(sax_context	*ctx,
			 void (* timeout_cb)(gboolean))
{
  gint w, h, x, y, hmargin, vmargin;
  float rx, ry, rw, rh;

  w = (ctx->lbearing + (ctx->x - ctx->start_x));
  h = (ctx->ascent + ctx->descent);

  hmargin = MIN(40, w*0.6);
  vmargin = MIN(40, h*0.6);

  x = ctx->start_x - (ctx->lbearing + hmargin/2);
  y = ctx->baseline - (ctx->ascent + vmargin/2);

  w += hmargin;
  h += vmargin;

  if (x<0)
    {
      w+=x;
      x=0;
    }
  if (y<0)
    {
      h+=y;
      h=0;
    }

  rh = 0.1;
  rw = (rh*w)/h;

  if (rw >= 0.9)
    {
      rw = 0.9;
      rh = (rw*h)/w;
    }

  rx = 1 - rw;
  ry = 1 - rh;

  clear_row(OSD_ROW, FALSE);

  add_piece(gdk_pixbuf_get_from_drawable
	    (NULL, ctx->canvas, ctx->cmap, x, y, 0, 0, w, h),
	    ctx->patch,
	    0, OSD_ROW, 0, 0, 0, rx, ry, rw, rh, NULL);

  zmodel_changed(osd_model);

  if (osd_clear_timeout_id > -1)
    {
      if (osd_clear_timeout_cb)
	osd_clear_timeout_cb(FALSE);

      gtk_timeout_remove(osd_clear_timeout_id);
    }

  gdk_pixmap_unref(ctx->canvas);

  osd_clear_timeout_id =
    gtk_timeout_add(zcg_float(NULL, "timeout")*1000,
		    osd_clear_timeout, NULL);

  osd_clear_timeout_cb = timeout_cb;
}

/* If given, timeout_cb(TRUE) is called when osd timed out,
   timeout_cb(FALSE) when error, replaced.
*/
void
osd_render_sgml		(void (*timeout_cb)(gboolean),
			 const char *string, ...)
{
  va_list args;
  sax_context ctx;
  xmlSAXHandler handler;
  gchar *buf, *buf2;
  int error;
  
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

  memset(&ctx, 0, sizeof(ctx));

  memset(&handler, 0, sizeof(handler));

  handler.startElement = my_startElement;
  handler.endElement = my_endElement;
  handler.characters = my_characters;

  buf2 = g_strdup_printf("<?xml version=\"1.0\" encoding=\"%s\"?>"
			 "<doc><text>%s</text></doc>    ",
			 get_locale_charset(), buf);

  /* Prepare for drawing */
  switch (zcg_int(NULL, "osd_type"))
    {
    case 0:
      if (!prepare_context(&ctx, buf))
	{
	  g_free(buf2);
	  g_free(buf);
	  goto failed;
	}
      break;
    case 1:
      ctx.dest_buf = g_strdup("");
      break;
    case 2:
      printf("OSD: ");
      break;
    default:
      g_assert_not_reached();
      break;
    }

  error = xmlSAXUserParseMemory(&handler, &ctx, buf2, strlen(buf2));

  if (error != 0)
    {
      g_warning("Couldn't parse XML string '%s' in '%s', error code %d",
		buf, buf2, error);
      g_free(buf2);
      g_free(buf);
      goto failed;
    }

  /* Create the patch with the pixbuf contents */
  switch (zcg_int(NULL, "osd_type"))
    {
    case 0:
      process_context(&ctx, timeout_cb);
      break;
    case 1:
      z_status_print(ctx.dest_buf);
      g_free(ctx.dest_buf);
      break;
    case 2:
      printf("\n");
      break;
    default:
      g_assert_not_reached();
      break;
    }

  g_free(buf2);
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
  va_list args;
  sax_context ctx;
  gchar *buf;

  if (!string || !string[0])
    goto failed;

  va_start(args, string);
  buf = g_strdup_vprintf(string, args);
  va_end(args);

  if (!buf)
    goto failed;

  if (!*buf)
    {
      g_free(buf);
      goto failed;
    }

  memset(&ctx, 0, sizeof(ctx));

  /* Prepare for drawing */
  switch (zcg_int(NULL, "osd_type"))
    {
    case 0:
      if (!prepare_context(&ctx, buf))
	{
	  g_free(buf);
	  goto failed;
	}
      break;
    case 1:
      ctx.dest_buf = g_strdup("");
      break;
    case 2:
      printf("OSD: ");
      break;
    default:
      g_assert_not_reached();
      break;
    }

  my_characters(&ctx, buf, strlen(buf));

  /* Create the patch with the pixbuf contents */
  switch (zcg_int(NULL, "osd_type"))
    {
    case 0:
      process_context(&ctx, timeout_cb);
      break;
    case 1:
      z_status_print(ctx.dest_buf);
      g_free(ctx.dest_buf);
      break;
    case 2:
      printf("\n");
      break;
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
    gtk_signal_disconnect_by_func(GTK_OBJECT(osd_window),
				  GTK_SIGNAL_FUNC(on_osd_screen_size_allocate),
				  NULL);

  osd_window = dest_window;

  coords_mode = _coords_mode;

  if (!coords_mode)
    gtk_signal_connect(GTK_OBJECT(dest_window), "size-allocate",
		       GTK_SIGNAL_FUNC(on_osd_screen_size_allocate),
		       NULL);

  geometry_update();
}

void
osd_set_window(GtkWidget *dest_window)
{
  gtk_widget_realize(dest_window);

  cx = cy = 0;
  gdk_window_get_size(dest_window->window, &cw, &ch);
  
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
    gtk_signal_disconnect_by_func(GTK_OBJECT(osd_window),
				  GTK_SIGNAL_FUNC(on_osd_screen_size_allocate),
				  NULL);

  osd_window = NULL;
  geometry_update(); /* Reparent to the orphanarium */
}

/**
 * Shutdown/startup of the OSD engine
 */
static gint		input_id = -1;
ZModel			*osd_model = NULL;

void
startup_osd(void)
{
  int i;
  GtkWidget *toplevel;

  for (i = 0; i<MAX_ROWS; i++)
    matrix[i] = g_malloc0(sizeof(row));

  if (!startup_ucs2())
    g_warning("Couldn't start Unicode, expect weird things to happen");

  printv("UNICODE: Current locale charset appears to be '%s'\n",
	 get_locale_charset());

  input_id = gdk_input_add(osd_pipe[0], GDK_INPUT_READ,
			   osd_event, NULL);

  osd_model = ZMODEL(zmodel_new());

  memset(&osd_page, 0, sizeof(osd_page));

  zcc_int(0, "Which kind of OSD should be used", "osd_type");
  zcc_char("-adobe-times-bold-r-normal-*-14-*-*-*-p-*-iso8859-1",
	   "Default font", "font");

  zcc_float(1.0, "Default fg r component", "fg_r");
  zcc_float(1.0, "Default fg g component", "fg_g");
  zcc_float(1.0, "Default fg b component", "fg_b");

  zcc_float(0.0, "Default bg r component", "bg_r");
  zcc_float(0.0, "Default bg g component", "bg_g");
  zcc_float(0.0, "Default bg b component", "bg_b");

  zcc_float(1.5, "Seconds the OSD text stays on screen", "timeout");

  orphanarium = gtk_fixed_new();
  gtk_widget_set_usize(orphanarium, 828, 271);
  toplevel = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_container_add(GTK_CONTAINER(toplevel), orphanarium);
  gtk_widget_show(orphanarium);
  gtk_widget_realize(orphanarium);
  /* toplevel will never be shown */
}

void
shutdown_osd(void)
{
  int i;

  osd_clear();
  clear_row(OSD_ROW, FALSE);

  for (i = 0; i<MAX_ROWS; i++)
    g_free(matrix[i]);

  gdk_input_remove(input_id);

  gtk_object_destroy(GTK_OBJECT(osd_model));
  osd_model = NULL;
}

#else /* !HAVE_LIBZVBI */

#include <gnome.h>
#include "osd.h"

/* just in case */
void
osd_render_sgml		(void (*timeout_cb)(gboolean),
			 const char *string, ...)
{
  if (timeout_cb)
    timeout_cb(0);
}

#endif /* !HAVE_LIBZVBI */
