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
#include <pthread.h>
#include "osd.h"
#include "zmisc.h"
#include "../common/fifo.h"
#include "../libvbi/ccfont.xbm"

#define CELL_WIDTH 16
#define CELL_HEIGHT 26

enum osd_code {OSD_NOTHING, OSD_EVENT};

#define NUM_COLS 34
#define NUM_ROWS 15

/* command fifo */
static fifo osd_fifo;
static pthread_mutex_t osd_mutex = PTHREAD_MUTEX_INITIALIZER;

#include "../libvbi/libvbi.h"
extern struct vbi *zvbi_get_object(void);

/*
 * Exact meaning of the fields might change, they cover the render,
 * clear and roll_up commands.
 */
struct osd_command
{
  enum osd_code		command;
  int			first_row;
  int			last_row;
  attr_char		buffer[NUM_COLS*NUM_ROWS];
};

struct osd_piece
{
  GtkWidget	*window; /* Attached to this piece */
  GdkPixbuf	*unscaled; /* unscaled version of the rendered text */
  GdkPixbuf	*scaled; /* scaled version of the text */
//  attr_char	*c; /* characters in the row */
// -> &osd_page.text[row * osd_page.columns + start]
// osd_render|clear|roll_up just have to update
// pieces to match the page passed by caption.c
  int		start; /* starting col in the row */
  int		width; /* number of characters in this piece */
};

struct osd_row {
  struct osd_piece	pieces[NUM_COLS]; /* Set of pieces to show */
  int			n_pieces; /* Number of pieces in this row */
};
static struct osd_row * osd_matrix[NUM_ROWS];
static struct fmt_page osd_page;

static GtkWidget *osd_window = NULL;
static GtkWidget *osd_parent_window = NULL;
static gboolean osd_started = FALSE; /* shared between threads */
static gboolean osd_status = FALSE;

static gint keeper_id = 0;
static gint input_id = 0;

/* Gets the events in the fifo */
static gint
the_kommand_keeper		(gpointer	data)
{
  buffer *b;
  struct osd_command *c;

  while ((b = recv_full_buffer(&osd_fifo)))
    {
      c = (struct osd_command*)b->data;

      if (!osd_status)
	c->command = OSD_NOTHING; /* discard */

      switch (c->command)
	{
	case OSD_EVENT:
	  osd_event();
	  break;
	default:
	  g_warning("Internal error processing OSD commands");
	  break;
	}

      send_empty_buffer(&osd_fifo, b);
    }

  return TRUE;
}

extern int test_pipe[2];

/* Gets the events in the pipe */
static void
the_kommand_batter		(gpointer	   data,
                                 gint              source, 
				 GdkInputCondition condition)
{
  char dummy[16];

  if (read(test_pipe[0], dummy, 16 /* flush */) <= 0 || !osd_status)
    return;
 
  osd_event();
}

void
startup_osd(void)
{
  struct vbi *vbi = zvbi_get_object();
  int i;

  if (osd_started)
    return;

  pthread_mutex_lock(&osd_mutex);

  g_assert(init_buffered_fifo(&osd_fifo, NULL, 10,
			      sizeof(struct osd_command)) > 2);

  for (i = 0; i<NUM_ROWS; i++)
    {
      osd_matrix[i] = g_malloc(sizeof(struct osd_row));
      memset(osd_matrix[i], 0, sizeof(struct osd_row));
    }

  osd_started = TRUE;

#if 1
  input_id = gdk_input_add(test_pipe[0], GDK_INPUT_READ,
			   the_kommand_batter, NULL);
#else
  keeper_id = gtk_timeout_add(50, the_kommand_keeper, NULL);
#endif

  g_assert(vbi_event_handler(vbi, VBI_EVENT_CAPTION,
                             cc_event, test_pipe) != 0);

  pthread_mutex_unlock(&osd_mutex);
}

void
shutdown_osd(void)
{
  struct vbi *vbi = zvbi_get_object();
  int i;

  g_assert(osd_started == TRUE);

  pthread_mutex_lock(&osd_mutex);

// oops: vbi == 0 here?
//  vbi_event_handler(vbi, 0, cc_event, NULL);

  osd_clear();

  for (i = 0; i<NUM_ROWS; i++)
    g_free(osd_matrix[i]);

  uninit_fifo(&osd_fifo);

#if 1
  gdk_input_remove(input_id);
#else
  gtk_timeout_remove(keeper_id);
#endif

  osd_started = FALSE;

  pthread_mutex_unlock(&osd_mutex);
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

  if ((!p->scaled)  ||
      (gdk_pixbuf_get_width(p->scaled) != dest_w)  ||
      (gdk_pixbuf_get_height(p->scaled) != dest_h))
    {
      if (p->scaled)
	gdk_pixbuf_unref(p->scaled);
      p->scaled = gdk_pixbuf_scale_simple(p->unscaled, dest_w,
					  dest_h, GDK_INTERP_BILINEAR);
    }

  paint_piece(p->window, p, 0, 0, dest_w, dest_h);
}

static void
osd_geometry_update(void)
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

static
gboolean on_osd_screen_configure	(GtkWidget	*widget,
					 GdkEventConfigure *event,
					 gpointer	user_data)
{
  osd_geometry_update();

  return TRUE;
}

static
void on_osd_screen_size_allocate	(GtkWidget	*widget,
					 GtkAllocation	*allocation,
					 gpointer	ignored)
{
  osd_geometry_update();
}

void
osd_set_window(GtkWidget *dest_window, GtkWidget *parent)
{
  g_assert(osd_started == TRUE);

  osd_window = dest_window;
  osd_parent_window = parent;

  gtk_signal_connect(GTK_OBJECT(dest_window), "size-allocate",
		     GTK_SIGNAL_FUNC(on_osd_screen_size_allocate),
		     NULL);
  gtk_signal_connect(GTK_OBJECT(parent), "configure-event",
		     GTK_SIGNAL_FUNC(on_osd_screen_configure),
		     NULL);

  osd_geometry_update();
}

static void
osd_unset_window(void)
{
  if (!osd_window || !osd_parent_window || osd_status)
    return;

  gtk_signal_disconnect_by_func(GTK_OBJECT(osd_window),
				GTK_SIGNAL_FUNC(on_osd_screen_size_allocate),
				NULL);
  gtk_signal_disconnect_by_func(GTK_OBJECT(osd_parent_window),
				GTK_SIGNAL_FUNC(on_osd_screen_configure),
				NULL);

  osd_window = NULL;
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

  osd_unset_window();
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
	  g_assert_not_reached();
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
  gtk_container_add(GTK_CONTAINER(window), da);
  gtk_widget_realize(window);

  gdk_window_set_back_pixmap(da->window, NULL, FALSE);

  gdk_window_set_decorations(window->window, 0);

  gtk_window_set_transient_for(GTK_WINDOW(window),
			       GTK_WINDOW(osd_parent_window));

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

  if (row < 0 || row >= NUM_ROWS || p_index < 0 ||
      p_index >= osd_matrix[row]->n_pieces)
    return;

  p = &(osd_matrix[row]->pieces[p_index]);

//  if (p->width > 0 && p->c)
//    g_free(p->c);

  if (p->scaled)
    gdk_pixbuf_unref(p->scaled);
  gdk_pixbuf_unref(p->unscaled);

  da = GTK_BIN(p->window)->child;
  gtk_signal_disconnect_by_func(GTK_OBJECT(da),
				GTK_SIGNAL_FUNC(on_osd_expose_event), p);

  if (p->window)
    {
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
  g_assert(row < NUM_ROWS);
  g_assert(col >= 0);
  g_assert(col+width <= NUM_COLS);

  memset(&p, 0, sizeof(p));

  p.width = width;
  p.start = col;
//  p.c = g_malloc(width*sizeof(attr_char));
//  memcpy(p.c, c, width*sizeof(attr_char));
  p.window = pop_window();
  da = GTK_BIN(p.window)->child;

  p.unscaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
			       CELL_WIDTH*p.width, CELL_HEIGHT);
  draw_row(gdk_pixbuf_get_pixels(p.unscaled), 
           c, width,
  	   gdk_pixbuf_get_rowstride(p.unscaled));

  pp = &(osd_matrix[row]->pieces[osd_matrix[row]->n_pieces]);
  memcpy(pp, &p, sizeof(struct osd_piece));

  osd_matrix[row]->n_pieces++;

  gtk_signal_connect(GTK_OBJECT(da), "expose-event",
		     GTK_SIGNAL_FUNC(on_osd_expose_event), pp);

  set_piece_geometry(row, osd_matrix[row]->n_pieces-1);

  gtk_widget_show(pp->window);

  return osd_matrix[row]->n_pieces-1;
}

static void osd_clear_row(int row, int just_push)
{
  g_assert(osd_started == TRUE);
  g_assert(row >= 0);
  g_assert(row < NUM_ROWS);

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

  for (i=0; i<NUM_ROWS; i++)
    osd_clear_row(i, 0);
}

void osd_render2(void)
{
  attr_char *ac_row;
  int row, i, j;

  g_assert(osd_started == TRUE);

  row = osd_page.dirty.y0;
  ac_row = osd_page.text + row * osd_page.columns;

  for (; row <= osd_page.dirty.y1; ac_row += osd_page.columns, row++)
    {
      /* FIXME: This produces flicker, we should allow rendering from
	 an arbitrary row, there's no way to know from osd */
      osd_clear_row(row, 1);
      for (i = j = 0; i<NUM_COLS; i++)
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
}

void osd_event(void)
{
  struct vbi *vbi = zvbi_get_object();
  int i;

  if (!vbi_fetch_cc_page(vbi, &osd_page, 1 /* XXX pgno 1 ... 8 */))
    return; /* trouble in outer space */

  if (osd_page.dirty.y0 > osd_page.dirty.y1)
    return; /* not dirty */

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

  osd_render2();
}

static void
send_cc_command(struct osd_command *c)
{
  buffer *b;

  if (!osd_status || !osd_started)
    return;

  pthread_mutex_lock(&osd_mutex);

  b = wait_empty_buffer(&osd_fifo);
  memcpy(b->data, c, sizeof(struct osd_command));
  send_full_buffer(&osd_fifo, b);

  pthread_mutex_unlock(&osd_mutex);
}

void cc_event(vbi_event *ev, void *data)
{
  struct osd_command c;
  int *pipe = data;

  /* Discard or flag update. pg->dirty tracks changes
     between calls to vbi_fetch_cc_page */
  if (ev->pgno != 1 /* XXX 1 ... 8 */)
    return;

#if 1
  /* Shouldn't block when the pipe buffer is full... ? */
  write(pipe[1], "", 1);
#else
  c.command = OSD_EVENT;
  send_cc_command(&c);
#endif
}
