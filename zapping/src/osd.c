/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2001 I�aki Garc�a Etxebarria
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
#include "osd.h"
#include "../libvbi/ccfont.xbm"

#define NUM_COLS 34
#define NUM_ROWS 15

#define CELL_WIDTH 16
#define CELL_HEIGHT 26

struct osd_piece
{
  GtkWidget	*window; /* Attached to this piece */
  GdkPixbuf	*unscaled; /* unscaled version of the rendered text */
  GdkPixbuf	*scaled; /* scaled version of the text */
  attr_char	*c; /* characters in the row */
  int		start; /* starting col in the row */
  int		width; /* number of characters in this piece */
};

struct osd_row {
  struct osd_piece	pieces[NUM_COLS]; /* Set of pieces to show */
  int			n_pieces; /* Number of pieces in this row */
};
static struct osd_row * osd_matrix[NUM_ROWS];

static GtkWidget *osd_window =NULL;
static gboolean osd_started = FALSE;

void
startup_osd(void)
{
  int i;

  if (osd_started)
    return;

  for (i = 0; i<NUM_ROWS; i++)
    {
      osd_matrix[i] = g_malloc(sizeof(struct osd_row));
      memset(osd_matrix[i], 0, sizeof(struct osd_row));
    }
  /* do nothing for the moment, will allocate window pool */

  osd_started = TRUE;
}

void
shutdown_osd(void)
{
  int i;

  g_assert(osd_started == TRUE);
  
  for (i = 0; i<NUM_ROWS; i++)
    g_free(osd_matrix[i]);

  osd_clear();
  /* will free window pool */
}

static void
set_piece_geometry(int row, int piece)
{
  gint x, y, w, h;
  struct osd_piece * p;
  gint dest_w, dest_h;

  g_assert(osd_started == TRUE);
  g_assert(row >= 0);
  g_assert(row < NUM_ROWS);
  g_assert(piece >= 0);
  g_assert(piece < osd_matrix[row]->n_pieces);
  g_assert(osd_window != NULL);

  gdk_window_get_size(osd_window->window, &w, &h);
  gdk_window_get_origin(osd_window->window, &x, &y);

  /* Text area is (48, 45) - (591, 434) in a 640x480 screen */
  x += (48*w)/640;
  y += (45*h)/480;
  w -= (96*w)/640;
  h -= (90*h)/480;

  p = &(osd_matrix[row]->pieces[piece]);

  gtk_widget_realize(p->window);

  dest_w = ((p->start+p->width)*w)/NUM_COLS - (p->start*w)/NUM_COLS;
  dest_h = ((row+1)*h)/NUM_ROWS-(row*h)/NUM_ROWS;

  gdk_window_move_resize(p->window->window,
			 x+(p->start*w)/NUM_COLS,
			 y+(row*h)/NUM_ROWS,
			 dest_w, dest_h);

  gdk_window_clear_area_e(p->window->window, 0, 0, w, h);
}

static void
recompute_all_geometries(void)
{
  int i, j;

  g_assert(osd_started == TRUE);
  g_assert(osd_window != NULL);

  for (i=0; i<NUM_ROWS; i++)
    for (j=0; j<osd_matrix[i]->n_pieces; j++)
      {
	set_piece_geometry(i, j);
	gtk_widget_show(osd_matrix[i]->pieces[j].window);
      }
}

void
osd_on(GtkWidget * dest_window)
{
  g_assert(osd_started == TRUE);
  g_assert(dest_window != NULL);

  osd_window = dest_window;

  recompute_all_geometries();
}

void
osd_off(void)
{
  g_assert(osd_started == TRUE);
  osd_window = NULL;
}

static const unsigned char palette[8][3] = {
  {0x00, 0x00, 0x00},
  {0xff, 0x00, 0x00},
  {0x00, 0xff, 0x00},
  {0xff, 0xff, 0x00},
  {0x00, 0x00, 0xff},
  {0xff, 0x00, 0xff},
  {0x00, 0xff, 0xff},
  {0xff, 0xff, 0xff}
};

static inline void
draw_char(unsigned char *canvas, unsigned int c, unsigned char *pen,
	  int underline, int rowstride)
{
  unsigned short *s = ((unsigned short *) bitmap_bits)
    + (c & 31) + (c >> 5) * 32 * CELL_HEIGHT;
  int x, y, b;
  
  for (y = 0; y < CELL_HEIGHT; y++) {
    b = *s;
    s += 32;
    
    if (underline && (y >= 24 && y <= 25))
      b = ~0;
    
    for (x = 0; x < CELL_WIDTH; x++) {
      canvas[x*3+0] = pen[(b & 1)*3+0];
      canvas[x*3+1] = pen[(b & 1)*3+1];
      canvas[x*3+2] = pen[(b & 1)*3+2];
      b >>= 1;
    }

    canvas += rowstride;
  }
}

static void
draw_row(unsigned char *canvas, attr_char *line, int width, int rowstride)
{
  int i;
  unsigned char pen[6];
  
  for (i = 0; i < width; i++)
    {
      switch (line[i].opacity)
	{
	case TRANSPARENT_SPACE:
	  g_assert_not_reached(); /* programming error */
	  break;

	case TRANSPARENT:
	case SEMI_TRANSPARENT:
	  /* Transparency not implemented */
	  /* It could be done in some cases by setting the background
	     to the XVideo chroma */
	  pen[0] = palette[0][0];
	  pen[1] = palette[0][1];
	  pen[2] = palette[0][2];
	  pen[3] = palette[line[i].foreground][0];
	  pen[4] = palette[line[i].foreground][1];
	  pen[5] = palette[line[i].foreground][2];
	  break;
	    
	default:
	  pen[0] = palette[line[i].background][0];
	  pen[1] = palette[line[i].background][1];
	  pen[2] = palette[line[i].background][2];
	  pen[3] = palette[line[i].foreground][0];
	  pen[4] = palette[line[i].foreground][1];
	  pen[5] = palette[line[i].foreground][2];
	  break;
	}
      
      draw_char(canvas, line[i].glyph & 0xFF, pen, line[i].underline,
		rowstride);
      canvas += CELL_WIDTH*3;
    }
}

static
gboolean on_osd_expose_event		(GtkWidget	*widget,
					 GdkEventExpose	*event,
					 struct osd_piece *piece)
{
  gint w, h;

  g_assert(osd_started == TRUE);

  gdk_window_get_size(widget->window, &w, &h);

  g_message("exposing");

  g_assert(piece != NULL);
  g_assert(widget == GTK_BIN(piece->window)->child);
 
  if ((!piece->scaled)  ||
      (gdk_pixbuf_get_width(piece->scaled) != w)  ||
      (gdk_pixbuf_get_height(piece->scaled) != h))
    {
      if (piece->scaled)
	{
	  g_message("old scaled was %dx%d",
		    gdk_pixbuf_get_width(piece->scaled),
		    gdk_pixbuf_get_height(piece->scaled));
	  gdk_pixbuf_unref(piece->scaled);
	}
      g_message("scaling piece: %d, %d, %d, %d (%p, %p)",
		gdk_pixbuf_get_width(piece->unscaled),
		gdk_pixbuf_get_height(piece->unscaled),
		gdk_pixbuf_get_rowstride(piece->unscaled),
		piece->width, piece->scaled, piece->unscaled);
      piece->scaled = gdk_pixbuf_scale_simple(piece->unscaled, w, h,
					      GDK_INTERP_BILINEAR);
    }
  if (piece->scaled)
    gdk_pixbuf_render_to_drawable(piece->scaled, widget->window,
				  widget->style->white_gc,
				  event->area.x, event->area.y,
				  event->area.x, event->area.y,
				  event->area.width, event->area.height,
				  GDK_RGB_DITHER_NORMAL,
				  event->area.x, event->area.y);

  return TRUE;
}

/* FIXME: We should keep some windows for future use, and create only
   when required */
static GtkWidget *
get_window(void)
{
  GtkWidget *window = gtk_window_new(GTK_WINDOW_POPUP);
  GtkWidget *da = gtk_drawing_area_new();

  g_assert(osd_started == TRUE);

  gtk_widget_realize(window);
  gtk_container_add(GTK_CONTAINER(window), da);
  gtk_widget_realize(window);

  gdk_window_set_back_pixmap(da->window, NULL, FALSE);
  gdk_window_set_decorations(window->window, 0);

  gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(osd_window));

  gtk_widget_add_events(da, GDK_EXPOSURE_MASK);

  gtk_widget_show(da);

  return window;
}

static void
unget_window(GtkWidget *window)
{
  g_assert(osd_started == TRUE);

  gtk_widget_destroy(window);
}

static void
remove_piece(int row, int p_index)
{
  struct osd_piece * p;
  GtkWidget *da;

  g_assert(osd_started == TRUE);

  if (row < 0 || row >= NUM_ROWS || p_index < 0 ||
      p_index >= osd_matrix[row]->n_pieces)
    return;

  p = &(osd_matrix[row]->pieces[p_index]);

  if (p->width > 0 && p->c)
    g_free(p->c);

  if (p->scaled)
    gdk_pixbuf_unref(p->scaled);
  gdk_pixbuf_unref(p->unscaled);

  da = GTK_BIN(p->window)->child;
  gtk_signal_disconnect_by_func(GTK_OBJECT(da),
				GTK_SIGNAL_FUNC(on_osd_expose_event), p);

  if (p->window)
    unget_window(p->window);

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
  g_assert(row < NUM_ROWS);
  g_assert(col >= 0);
  g_assert(col+width <= NUM_COLS);

  memset(&p, 0, sizeof(p));

  p.width = width;
  p.start = col;
  p.c = g_malloc(width*sizeof(attr_char));
  memcpy(p.c, c, width*sizeof(attr_char));
  p.window = get_window();
  da = GTK_BIN(p.window)->child;

  p.unscaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
			       CELL_WIDTH*p.width, CELL_HEIGHT);
  draw_row(gdk_pixbuf_get_pixels(p.unscaled), p.c, p.width,
  	   gdk_pixbuf_get_rowstride(p.unscaled));

  pp = &(osd_matrix[row]->pieces[osd_matrix[row]->n_pieces]);
  memcpy(pp, &p, sizeof(struct osd_piece));

  osd_matrix[row]->n_pieces++;

  set_piece_geometry(row, osd_matrix[row]->n_pieces-1);
  gtk_signal_connect(GTK_OBJECT(da), "expose-event",
		     GTK_SIGNAL_FUNC(on_osd_expose_event), pp);

  g_message("allocated piece [%d, %d]: %d, %d, %d, %d (%p, %p)",
	    row, osd_matrix[row]->n_pieces,
	    gdk_pixbuf_get_width(pp->unscaled),
	    gdk_pixbuf_get_height(pp->unscaled),
	    gdk_pixbuf_get_rowstride(pp->unscaled),
	    pp->width, pp->scaled, pp->unscaled);

  gtk_widget_show(pp->window);

  g_assert(GTK_BIN(pp->window)->child  == da);

  return osd_matrix[row]->n_pieces-1;
}

static void osd_clear_row(int row)
{
  g_assert(osd_started == TRUE);
  g_assert(row >= 0);
  g_assert(row < NUM_ROWS);

  while (osd_matrix[row]->n_pieces)
    remove_piece(row, osd_matrix[row]->n_pieces-1);
}

/* These routines (clear, render, roll_up) are the caption.c
   counterparts, they will communicate with the CC engine using an
   event fifo (in zvbi, probably). This is needed since all GDK/GTK
   calls should be done from the main thread. */
void osd_clear(void)
{
  int i;

  g_assert(osd_started == TRUE);

  for (i=0; i<NUM_ROWS; i++)
    osd_clear_row(i);
}

void osd_render(attr_char *buffer, int row)
{
  int height=1;
  int i, j;
  attr_char piece_buffer[NUM_COLS];

  g_assert(osd_started == TRUE);

  if (row == -1)
    {
      row = -1;
      height = NUM_ROWS;
    }

  for (; row < (row+height); row++)
    {
      osd_clear_row(row);
      for (i = j = 0; i<NUM_COLS; i++)
	{
	  if ((buffer[i].opacity == TRANSPARENT_SPACE) &&
	      (j))
	    {
	      add_piece(i-j, row, j, piece_buffer);
	      j = 0;
	    }
	  else
	    piece_buffer[j++] = buffer[i];
	}
    }
}

void osd_roll_up(attr_char *buffer, int first_row, int last_row)
{
  gint i;
  struct osd_row *tmp;

  g_assert(osd_started == TRUE);

  for (; first_row < last_row; first_row++)
    {
      osd_clear_row(first_row);
      tmp = osd_matrix[first_row];
      osd_matrix[first_row] = osd_matrix[first_row+1];
      osd_matrix[first_row+1] = tmp;
      for (i=0; i<osd_matrix[first_row]->n_pieces; i++)
	set_piece_geometry(first_row, i);
    }
}

#if 0
static gboolean
on_main_configure_event		(GtkWidget	*window,
				 GdkEventConfigure *conf,
				 gpointer data)
{
  recompute_all_geometries();
  
  return TRUE;
}

static gint
actions_timeout			(gpointer	data)
{
  static gint counter = 0;
  int i;
  attr_char buffer[60];

  memset(buffer, 0, sizeof(buffer[0])*60);

  for (i=0; i<60; i++)
    {
      buffer[i].opacity = OPAQUE;
      buffer[i].underline = rand()%2;
      buffer[i].glyph = "abcdefghijklmnopq"[rand()%17];
      buffer[i].foreground = rand()%8;
      buffer[i].background = rand()%8;
    }

  switch (counter)
    {
    case 0:
      add_piece(5, 7, 5, buffer);
      add_piece(0, 0, 10, buffer+10);
      add_piece(11, 0, NUM_COLS-15, buffer);
      break;
    case 3:
      osd_clear_row(0);
      break;
    case 4:
      add_piece(1, 1, NUM_COLS-1, buffer+4);
      break;
    case 5 ... 9:
      osd_roll_up(NULL, 0, NUM_ROWS-1);
      add_piece(1, NUM_ROWS-1, NUM_COLS-2, buffer+(rand()%6));
      break;
    default:
      break;
    }
  
  counter++;
  return TRUE;
}

int main(int argc, char *argv[])
{
  GtkWidget *main_window;
  
  gtk_init(&argc, &argv);

  main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_widget_show(main_window);
  gtk_signal_connect(GTK_OBJECT(main_window), "configure-event",
		     GTK_SIGNAL_FUNC(on_main_configure_event), NULL);

  startup_osd();
  osd_on(main_window);

  gtk_timeout_add(1000, actions_timeout, 0);

  gtk_main();

  osd_off();
  shutdown_osd();

  return 0;
}
#endif
