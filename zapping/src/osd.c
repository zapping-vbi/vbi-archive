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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <pthread.h>
#include <parser.h> /* libxml */
#define ZCONF_DOMAIN "/zapping/options/osd/"
#include "zconf.h"
#include "osd.h"
#include "zmisc.h"
#include "x11stuff.h"
#include "common/fifo.h"
#include "common/math.h"
#include "zvbi.h"

#include "libvbi/export.h"

#define CELL_WIDTH 16
#define CELL_HEIGHT 26

#define MAX_COLUMNS 48
#define MAX_ROWS 26 /* 25 for TTX plus one for OSD */

#include "libvbi/libvbi.h"
extern struct vbi *zvbi_get_object(void);

ZModel *osd_model = NULL;

struct osd_piece
{
  GtkWidget	*window; /* Attached to this piece */
  GdkPixbuf	*unscaled; /* unscaled version of the rendered text */
  GdkPixbuf	*scaled; /* scaled version of the text */
  int		start; /* starting col in the row */
  int		width; /* number of characters in this piece */

  /* OSD text, all are 0..1, relative to the TV screen */
  float		x, y, w, h;
};

struct osd_row {
  struct osd_piece	pieces[MAX_COLUMNS]; /* Set of pieces to show */
  int			n_pieces; /* Number of pieces in this row */
};
static struct osd_row * osd_matrix[MAX_ROWS];
static struct fmt_page osd_page;

/* For window mode */
static GtkWidget *osd_window = NULL;
static GtkWidget *osd_parent_window = NULL;
/* For coords mode */
static gint cx, cy, cw=-1, ch=-1;

static gboolean osd_started = FALSE; /* shared between threads */
static gboolean osd_status = FALSE;

static gint input_id = 0;

extern int osd_pipe[2];

static void osd_event		(gpointer	   data,
                                 gint              source, 
				 GdkInputCondition condition);

void
startup_osd(void)
{
  int i;

  if (osd_started)
    return;

  for (i = 0; i<MAX_ROWS; i++)
    {
      osd_matrix[i] = g_malloc(sizeof(struct osd_row));
      memset(osd_matrix[i], 0, sizeof(struct osd_row));
    }

  osd_started = TRUE;

  if (!startup_ucs2())
    g_warning("Couldn't start Unicode, expect weird things to happen");

  printv("UNICODE: Current locale charset appears to be '%s'\n",
	 get_locale_charset());

  input_id = gdk_input_add(osd_pipe[0], GDK_INPUT_READ,
			   osd_event, NULL);

  osd_model = ZMODEL(zmodel_new());

  memset(&osd_page, 0, sizeof(struct fmt_page));

  zcc_int(0, "Which kind of OSD should be used", "osd_type");
  zcc_char("-adobe-times-bold-r-normal-*-14-*-*-*-p-*-iso8859-1",
	   "Default font", "font");

  zcc_float(1.0, "Default fg r component", "fg_r");
  zcc_float(1.0, "Default fg g component", "fg_g");
  zcc_float(1.0, "Default fg b component", "fg_b");

  zcc_float(0.0, "Default bg r component", "bg_r");
  zcc_float(0.0, "Default bg g component", "bg_g");
  zcc_float(0.0, "Default bg b component", "bg_b");
}

void
shutdown_osd(void)
{
  int i;

  g_assert(osd_started == TRUE);

  osd_clear();

  for (i = 0; i<MAX_ROWS; i++)
    g_free(osd_matrix[i]);

  gdk_input_remove(input_id);

  osd_started = FALSE;

  gtk_object_destroy(GTK_OBJECT(osd_model));
  osd_model = NULL;
}

static void
paint_piece (GtkWidget *widget, struct osd_piece *piece,
	     gint x, gint y, gint width, gint height)
{
  gint w, h;

  g_assert(osd_started == TRUE);
  g_assert(widget != NULL);

  gdk_window_get_size(widget->window, &w, &h);

  if (width == -1)
    {
      width = w;
      height = h;
    }

  if (piece->scaled)
    z_pixbuf_render_to_drawable(piece->scaled, widget->window,
				widget->style->white_gc,
				x, y, width, height);
}

static void
set_piece_geometry(int row, int piece)
{
  gint x, y, w, h;
  struct osd_piece * p;
  gint dest_w, dest_h, dest_x, dest_y;

  g_assert(osd_started == TRUE);
  g_assert(row >= 0);
  g_assert(row < MAX_ROWS);
  g_assert(piece >= 0);
  g_assert(piece < osd_matrix[row]->n_pieces);

  if (!osd_window && (cw < 0 || ch < 0))
    return; /* nop */

  if (osd_window)
    {
      gdk_window_get_size(osd_window->window, &w, &h);
      gdk_window_get_origin(osd_window->window, &x, &y);
    }
  else
    {
      x = cx; y = cy; w = cw; h = ch;
    }

  p = &(osd_matrix[row]->pieces[piece]);

  /* OSD */
  if (row == MAX_ROWS - 1)
    {
      dest_x = x + w*p->x;
      dest_y = y + h*p->y;
      dest_w = w*p->w;
      dest_h = h*p->h;
      goto resize;
    }

  if (osd_page.columns >= 40) /* naive cc test */
    {
      /* Text area 40x25 is (64, 38) - (703, 537) in a 768x576 screen */
      x += (64*w)/768;
      y += (38*h)/576;
      w -= (128*w)/768;
      h -= (76*h)/576;
    }
  else
    {
      /* Text area 34x15 is (48, 45) - (591, 434) in a 640x480 screen */
      x += (48*w)/640;
      y += (45*h)/480;
      w -= (96*w)/640;
      h -= (90*h)/480;
    }

  dest_w = ((p->start+p->width)*w)/osd_page.columns
           - (p->start*w)/osd_page.columns;
  dest_h = ((row+1)*h)/osd_page.rows-(row*h)/osd_page.rows;

  dest_x = x + (p->start*w)/osd_page.columns;
  dest_y = y+(row*h)/osd_page.rows;

 resize:

  gtk_widget_realize(p->window);

  gdk_window_move_resize(p->window->window,
			 dest_x, dest_y,
			 dest_w, dest_h);

  if ((!p->scaled)  ||
      (gdk_pixbuf_get_width(p->scaled) != dest_w)  ||
      (gdk_pixbuf_get_height(p->scaled) != dest_h))
    {
      if (p->scaled)
	gdk_pixbuf_unref(p->scaled);
      if (dest_h > 0)
	p->scaled = z_pixbuf_scale_simple(p->unscaled, dest_w,
					  dest_h,
					  GDK_INTERP_BILINEAR);
      else
	p->scaled = NULL;
    }

  if (p->scaled)
    paint_piece(GTK_BIN(p->window)->child, p, 0, 0, dest_w, dest_h);
}

static void
osd_geometry_update(gboolean raise_if_visible)
{
  int i, j;
  gboolean visible;

  g_assert(osd_started == TRUE);

  if (osd_window)
    visible = x11_window_viewable(osd_window->window);
  else
    visible = FALSE;

  if (cw > 0 && ch > 0)
    visible = TRUE;

  for (i=0; i<MAX_ROWS; i++)
    for (j=0; j<osd_matrix[i]->n_pieces; j++)
      {
	set_piece_geometry(i, j);
	if (visible)
	  {
	    gtk_widget_show(osd_matrix[i]->pieces[j].window);
	    if (raise_if_visible)
	      gdk_window_raise(osd_matrix[i]->pieces[j].window->window);
	  }
	else
	  gtk_widget_hide(osd_matrix[i]->pieces[j].window);
      }
}

static
gboolean on_osd_screen_configure	(GtkWidget	*widget,
					 GdkEventConfigure *event,
					 gpointer	user_data)
{
  osd_geometry_update(FALSE);

  return TRUE;
}

static
void on_osd_screen_size_allocate	(GtkWidget	*widget,
					 GtkAllocation	*allocation,
					 gpointer	ignored)
{
  osd_geometry_update(FALSE);
}

/* Handle the map/unmap logic */
static
gboolean on_osd_event			(GtkWidget	*widget,
					 GdkEvent	*event,
					 gpointer	ignored)
{
  switch (event->type)
    {
    case GDK_UNMAP:
    case GDK_MAP:
      osd_geometry_update(FALSE);
      break;
    default:
      break;
    }

  return FALSE;
}

void
osd_unset_window(void)
{
  if (!osd_window && !osd_parent_window)
    return;

  if (osd_window)
    gtk_signal_disconnect_by_func(GTK_OBJECT(osd_window),
				  GTK_SIGNAL_FUNC(on_osd_screen_size_allocate),
				  NULL);

  if (osd_parent_window) {
    gtk_signal_disconnect_by_func(GTK_OBJECT(osd_parent_window),
				  GTK_SIGNAL_FUNC(on_osd_screen_configure),
				  NULL);
    gtk_signal_disconnect_by_func(GTK_OBJECT(osd_parent_window),
				  GTK_SIGNAL_FUNC(on_osd_event),
				  NULL);
  }

  osd_window = osd_parent_window = NULL;
  osd_geometry_update(FALSE);
}

void
osd_set_window(GtkWidget *dest_window, GtkWidget *parent)
{
  g_assert(osd_started == TRUE);

  if (osd_window || osd_parent_window)
    osd_unset_window();

  g_assert(osd_window == NULL);
  g_assert(osd_parent_window == NULL);

  ch = cw = -1;

  osd_window = dest_window;
  osd_parent_window = parent;

  gtk_signal_connect(GTK_OBJECT(dest_window), "size-allocate",
		     GTK_SIGNAL_FUNC(on_osd_screen_size_allocate),
		     NULL);
  gtk_signal_connect(GTK_OBJECT(parent), "configure-event",
		     GTK_SIGNAL_FUNC(on_osd_screen_configure),
		     NULL);
  gtk_signal_connect(GTK_OBJECT(parent), "event",
		     GTK_SIGNAL_FUNC(on_osd_event), NULL);

  osd_geometry_update(TRUE);
}

void
osd_set_coords(gint x, gint y, gint w, gint h)
{
  if (osd_window || osd_parent_window)
    osd_unset_window();

  g_assert(osd_window == NULL);
  g_assert(osd_parent_window == NULL);

  cx = x;
  cy = y;
  cw = w;
  ch = h;

  osd_geometry_update(TRUE);
}

void
osd_on(GtkWidget * dest_window, GtkWidget *parent)
{
  g_assert(osd_started == TRUE);

  if (osd_status)
    return;

  osd_set_window(dest_window, parent);

  osd_status = TRUE;
}

void
osd_off(void)
{
  g_assert(osd_started == TRUE);

  osd_status = FALSE;
  osd_clear();

  if (osd_window)
    osd_unset_window();
  cw = ch = -1;
}

static
gboolean on_osd_expose_event		(GtkWidget	*widget,
					 GdkEventExpose	*event,
					 struct osd_piece *piece)
{
  g_assert(osd_started == TRUE);

  g_assert(piece != NULL);
  g_assert(widget == GTK_BIN(piece->window)->child);

  paint_piece(widget, piece, event->area.x, event->area.y,
	      event->area.width, event->area.height);

  return TRUE;
}

/* List of destroyed windows for reuse */
static GList *window_pool = NULL;
static GList *window_stack = NULL;

static GtkWidget *
get_window(void)
{
  GtkWidget *window;
  GtkWidget *da;

  g_assert(osd_started == TRUE);

  if (window_pool)
    {
      window = GTK_WIDGET(g_list_last(window_pool)->data);
      window_pool = g_list_remove(window_pool, window);
      return window;
    }

  window = gtk_window_new(GTK_WINDOW_POPUP);
  da = gtk_drawing_area_new();

  gtk_widget_realize(window);
  while (!window->window)
    z_update_gui();
  gtk_container_add(GTK_CONTAINER(window), da);

  gdk_window_set_back_pixmap(da->window, NULL, FALSE);

  gdk_window_set_decorations(window->window, 0);

  if (osd_parent_window)
    {
      gdk_window_set_transient_for(window->window,
				   osd_parent_window->window);
      gdk_window_set_group(window->window, osd_parent_window->window);
    }

  gtk_widget_add_events(da, GDK_EXPOSURE_MASK);

  gtk_widget_show(da);

  return window;
}

static void
unget_window(GtkWidget *window)
{
  g_assert(osd_started == TRUE);

  gtk_widget_hide(window);

  window_pool = g_list_append(window_pool, window);
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

static void
remove_piece(int row, int p_index, int just_push)
{
  struct osd_piece * p;
  GtkWidget *da;

  g_assert(osd_started == TRUE);

  if (row < 0 || row >= MAX_ROWS || p_index < 0 ||
      p_index >= osd_matrix[row]->n_pieces)
    return;

  p = &(osd_matrix[row]->pieces[p_index]);

  if (p->scaled)
    gdk_pixbuf_unref(p->scaled);
  gdk_pixbuf_unref(p->unscaled);

  if (p->window)
    {
      da = GTK_BIN(p->window)->child;
      gtk_signal_disconnect_by_func(GTK_OBJECT(da),
				    GTK_SIGNAL_FUNC(on_osd_expose_event), p);

      if (!just_push)
	unget_window(p->window);
      else
	push_window(p->window);
    }

  if (p_index != (osd_matrix[row]->n_pieces - 1))
    memcpy(p, p+1,
	   sizeof(struct osd_piece)*((osd_matrix[row]->n_pieces-p_index)-1));

  osd_matrix[row]->n_pieces--;
}

/* Returns the index of the piece. c has at least width chars */
static int
add_piece(int col, int row, int width, attr_char *c)
{
  struct osd_piece p;
  GtkWidget *da;
  struct osd_piece *pp;

  g_assert(osd_started == TRUE);
  g_assert(row >= 0);
  g_assert(row < MAX_COLUMNS);
  g_assert(col >= 0);
  g_assert(col+width <= MAX_COLUMNS);

  memset(&p, 0, sizeof(p));

  p.width = width;
  p.start = col;
  p.window = pop_window();
  da = GTK_BIN(p.window)->child;

  if (osd_page.columns < 40) /* naive cc test */
    {
      p.unscaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
			          CELL_WIDTH*p.width, CELL_HEIGHT);
      vbi_draw_cc_page_region(&osd_page,
          (uint32_t *) gdk_pixbuf_get_pixels(p.unscaled),
	  col, row, width, 1 /* height */,
          gdk_pixbuf_get_rowstride(p.unscaled));
    }
  else
    {
      p.unscaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
			          12*p.width, 10);
      vbi_draw_vt_page_region(&osd_page,
          (uint32_t *) gdk_pixbuf_get_pixels(p.unscaled),
          col, row, width, 1 /* height */,
	  gdk_pixbuf_get_rowstride(p.unscaled),
	  1 /* reveal */, 1 /* flash_on */);
    }

  pp = &(osd_matrix[row]->pieces[osd_matrix[row]->n_pieces]);
  memcpy(pp, &p, sizeof(struct osd_piece));

  osd_matrix[row]->n_pieces++;

  gtk_signal_connect(GTK_OBJECT(da), "expose-event",
		     GTK_SIGNAL_FUNC(on_osd_expose_event), pp);

  set_piece_geometry(row, osd_matrix[row]->n_pieces-1);

  if ((osd_window && x11_window_viewable(osd_window->window)) ||
      (cw > 0 && ch > 0))
    gtk_widget_show(pp->window);

  return osd_matrix[row]->n_pieces-1;
}

static void osd_clear_row(int row, int just_push)
{
  g_assert(osd_started == TRUE);
  g_assert(row >= 0);
  g_assert(row < MAX_ROWS);

  while (osd_matrix[row]->n_pieces)
    remove_piece(row, osd_matrix[row]->n_pieces-1, just_push);
}

/* These routines (clear, render, roll_up) are the caption.c
   counterparts, they will communicate with the CC engine using an
   event fifo. This is needed since all GDK/GTK
   calls should be done from the main thread. */
/* {mhs} Now subroutines of osd_event. */

void osd_clear(void)
{
  int i;

  g_assert(osd_started == TRUE);

  for (i=0; i<MAX_ROWS; i++)
    osd_clear_row(i, 0);

  zmodel_changed(osd_model);
}

void osd_render(void)
{
  attr_char *ac_row;
  int row, i, j;

  g_assert(osd_started == TRUE);

  row = osd_page.dirty.y0;
  ac_row = osd_page.text + row * osd_page.columns;

  for (; row <= osd_page.dirty.y1; ac_row += osd_page.columns, row++)
    {
      osd_clear_row(row, 1);
      for (i = j = 0; i < osd_page.columns; i++)
        {
	  if (ac_row[i].opacity != TRANSPARENT_SPACE)
	    j++;
	  else if (j > 0)
	    {
	      add_piece(i - j, row, j, ac_row + i - j);
	      j = 0;
	    }
	}

      if (j)
	add_piece(osd_page.columns - j, row, j,
		  ac_row + osd_page.columns - j);

      clear_stack();
    }

  zmodel_changed(osd_model);
}

void osd_roll_up(attr_char *buffer, int first_row, int last_row)
{
  gint i;
  struct osd_row *tmp;

  g_assert(osd_started == TRUE);

  for (; first_row < last_row; first_row++)
    {
      osd_clear_row(first_row, 0);
      tmp = osd_matrix[first_row];
      osd_matrix[first_row] = osd_matrix[first_row+1];
      osd_matrix[first_row+1] = tmp;
      for (i=0; i<osd_matrix[first_row]->n_pieces; i++)
	set_piece_geometry(first_row, i);
    }

  zmodel_changed(osd_model);
}

static void
osd_event			(gpointer	   data,
                                 gint              source, 
				 GdkInputCondition condition)
{
  struct vbi *vbi = zvbi_get_object();
  char dummy[16];
  extern int zvbi_page, zvbi_subpage;

  if (!vbi)
    return;

  if (read(osd_pipe[0], dummy, 16 /* flush */) <= 0
      || !osd_status)
    return;

  if (zvbi_page <= 8)
    {
      if (!vbi_fetch_cc_page(vbi, &osd_page, zvbi_page))
        return; /* trouble in outer space */
    }
  else
    {
      if (!vbi_fetch_vt_page(vbi, &osd_page, zvbi_page, zvbi_subpage,
          25 /* rows */, 1 /* nav */))
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
      osd_roll_up(osd_page.text + osd_page.dirty.y0 * osd_page.columns,
                  osd_page.dirty.y0, osd_page.dirty.y1);
      return;
    }

  g_assert(osd_page.dirty.roll == 0);
    /* currently never down or more than one row */

  osd_render();
}

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
  ctx->entry[++ctx->entry_sp] = label

static void
my_startElement (void *ptr,
		 const xmlChar *name, const xmlChar **atts)
{
  sax_context *ctx = ptr;

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
  sax_context *ctx = ptr;

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
 * FIXME: Caching? Probably not appropiate.
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
  GtkWidget *da;
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
  da = GTK_BIN(ctx->patch)->child;

  gtk_widget_realize(da);

  ctx->gc = gdk_gc_new(da->window);
  g_assert(ctx->gc != NULL);
  ctx->cmap = gdk_window_get_colormap(da->window);

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

  ctx->canvas = gdk_pixmap_new(da->window, w, h, -1);
  g_assert(ctx->canvas);

  bg = create_color(zcg_float(NULL, "bg_r"), zcg_float(NULL, "bg_g"),
		    zcg_float(NULL, "bg_b"), ctx->cmap);
  gdk_gc_set_foreground(ctx->gc, bg);
  gdk_draw_rectangle(ctx->canvas, ctx->gc, TRUE, 0, 0, w, h);
  unref_color(bg, ctx->cmap);

  unref_font(font);
  return TRUE;
}

/* Finish the patch by creating the gdk pixbuf, etc */
static void
process_context		(sax_context	*ctx)
{
  gint w, h, x, y, hmargin, vmargin;
  struct osd_piece p, *pp;
  gint row = MAX_ROWS - 1;

  memset(&p, 0, sizeof(p));

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

  p.window = ctx->patch;
  p.unscaled =
    gdk_pixbuf_get_from_drawable(NULL, ctx->canvas,
				 ctx->cmap, x, y, 0, 0, w, h);

  gdk_pixmap_unref(ctx->canvas);

  p.x = 0.6;
  p.w = 0.4;
  p.y = 0.9;
  p.h = 0.1;

  osd_clear_row(row, FALSE);

  /* Add this piece in the OSD row */
  pp = &(osd_matrix[row]->pieces[osd_matrix[row]->n_pieces]);
  memcpy(pp, &p, sizeof(struct osd_piece));

  osd_matrix[row]->n_pieces++;

  gtk_signal_connect(GTK_OBJECT(GTK_BIN(p.window)->child), "expose-event",
		     GTK_SIGNAL_FUNC(on_osd_expose_event), pp);

  set_piece_geometry(row, osd_matrix[row]->n_pieces-1);

  if ((osd_window && x11_window_viewable(osd_window->window)) ||
      (cw > 0 && ch > 0))
    gtk_widget_show(pp->window);
}

void
osd_render_sgml		(const char *string, ...)
{
  va_list args;
  sax_context ctx;
  xmlSAXHandler handler;
  gchar *buf, *buf2;
  
  if (!string || !strlen(string))
    return;

  va_start(args, string);
  buf = g_strdup_vprintf(string, args);
  va_end(args);

  memset(&ctx, 0, sizeof(ctx));

  memset(&handler, 0, sizeof(handler));

  handler.startElement = my_startElement;
  handler.endElement = my_endElement;
  handler.characters = my_characters;

  buf2 = g_strdup_printf("<?xml version=\"1.0\" encoding=\"%s\"?>\n"
			 "<doc>\n"
			 "<text>%s</text>\n"
			 "</doc>", get_locale_charset(), buf);

  /* Prepare for drawing */
  switch (zcg_int(NULL, "osd_type"))
    {
    case 0:
      if (!prepare_context(&ctx, buf))
	{
	  g_free(buf2);
	  g_free(buf);
	  return;
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

  if (xmlSAXUserParseMemory(&handler, &ctx, buf2,
			    strlen(buf2)))
    {
      g_warning("Couldn't parse XML string: %s", buf);
      g_free(buf2);
      g_free(buf);
      return;
    }

  /* Create the patch with the pixbuf contents */
  switch (zcg_int(NULL, "osd_type"))
    {
    case 0:
      process_context(&ctx);
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
}
