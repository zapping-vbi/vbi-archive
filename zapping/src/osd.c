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
#include "osd.h"
#include "zmisc.h"
#include "../common/fifo.h"

#include "../libvbi/export.h"

#define CELL_WIDTH 16
#define CELL_HEIGHT 26

#define MAX_COLUMNS 40
#define MAX_ROWS 25

static pthread_mutex_t osd_mutex = PTHREAD_MUTEX_INITIALIZER;

#include "../libvbi/libvbi.h"
extern struct vbi *zvbi_get_object(void);

struct osd_piece
{
  GtkWidget	*window; /* Attached to this piece */
  GdkPixbuf	*unscaled; /* unscaled version of the rendered text */
  GdkPixbuf	*scaled; /* scaled version of the text */
  int		start; /* starting col in the row */
  int		width; /* number of characters in this piece */
};

struct osd_row {
  struct osd_piece	pieces[MAX_COLUMNS]; /* Set of pieces to show */
  int			n_pieces; /* Number of pieces in this row */
};
static struct osd_row * osd_matrix[MAX_ROWS];
static struct fmt_page osd_page;

static GtkWidget *osd_window = NULL;
static GtkWidget *osd_parent_window = NULL;
static gboolean osd_started = FALSE; /* shared between threads */
static gboolean osd_status = FALSE;

static gint input_id = 0;

static gint page = 1, subpage = ANY_SUB;	// caption 1 ... 8
// static gint page = 0x777, subpage = ANY_SUB;	// ttx 0x100 ... 0x8FF

extern int test_pipe[2];

static void osd_event		(gpointer	   data,
                                 gint              source, 
				 GdkInputCondition condition);

void
startup_osd(void)
{
  struct vbi *vbi = zvbi_get_object();
  int i;

  if (osd_started)
    return;

  pthread_mutex_lock(&osd_mutex);

  for (i = 0; i<MAX_ROWS; i++)
    {
      osd_matrix[i] = g_malloc(sizeof(struct osd_row));
      memset(osd_matrix[i], 0, sizeof(struct osd_row));
    }

  osd_started = TRUE;

  input_id = gdk_input_add(test_pipe[0], GDK_INPUT_READ,
			   osd_event, NULL);

  if (vbi) /* FIXME: This doesn't belong here, but zvbi (osd isn't vbi
	      specific, vbi can be opened at a later time) */
    g_assert(vbi_event_handler(vbi,
			       VBI_EVENT_CAPTION | VBI_EVENT_PAGE,
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

  if (vbi)
    vbi_event_handler(vbi, 0, cc_event, NULL);

  osd_clear();

  for (i = 0; i<MAX_ROWS; i++)
    g_free(osd_matrix[i]);

  gdk_input_remove(input_id);

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
  g_assert(row < MAX_ROWS);
  g_assert(piece >= 0);
  g_assert(piece < osd_matrix[row]->n_pieces);
  g_assert(osd_window != NULL);

  gdk_window_get_size(osd_window->window, &w, &h);
  gdk_window_get_origin(osd_window->window, &x, &y);

  if (osd_page.columns < 40) /* naive cc test */
    {
      /* Text area 34x15 is (48, 45) - (591, 434) in a 640x480 screen */
      x += (48*w)/640;
      y += (45*h)/480;
      w -= (96*w)/640;
      h -= (90*h)/480;
    }
  else
    {
      /* Text area 40x25 is (64, 38) - (703, 537) in a 768x576 screen */
      x += (64*w)/768;
      y += (38*h)/576;
      w -= (128*w)/768;
      h -= (76*h)/576;
    }

  p = &(osd_matrix[row]->pieces[piece]);

  gtk_widget_realize(p->window);

  dest_w = ((p->start+p->width)*w)/osd_page.columns
           - (p->start*w)/osd_page.columns;
  dest_h = ((row+1)*h)/osd_page.rows-(row*h)/osd_page.rows;

  gdk_window_move_resize(p->window->window,
			 x+(p->start*w)/osd_page.columns,
			 y+(row*h)/osd_page.rows,
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

  for (i=0; i<osd_page.rows; i++)
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

  if (row < 0 || row >= MAX_ROWS || p_index < 0 ||
      p_index >= osd_matrix[row]->n_pieces)
    return;

  p = &(osd_matrix[row]->pieces[p_index]);

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
          gdk_pixbuf_get_pixels(p.unscaled),
	  col, row, width, 1 /* height */,
          gdk_pixbuf_get_rowstride(p.unscaled));
    }
  else
    {
      p.unscaled = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
			          12*p.width, 10);
      vbi_draw_page_region(&osd_page,
          gdk_pixbuf_get_pixels(p.unscaled),
	  1 /* reveal */, col, row, width, 1 /* height */,
          gdk_pixbuf_get_rowstride(p.unscaled), 1 /* flash_on */);
    }

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

  for (i=0; i<osd_page.rows; i++)
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

static void
osd_event			(gpointer	   data,
                                 gint              source, 
				 GdkInputCondition condition)
{
  struct vbi *vbi = zvbi_get_object();
  char dummy[16];

  if (read(test_pipe[0], dummy, 16 /* flush */) <= 0
      || !osd_status)
    return;

  if (page <= 8)
    {
      if (!vbi_fetch_cc_page(vbi, &osd_page, page))
        return; /* trouble in outer space */
    }
  else
    {
      if (!vbi_fetch_vt_page(vbi, &osd_page, page, subpage,
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

  osd_render2();
}

void cc_event(vbi_event *ev, void *data)
{
  int *pipe = data;

  if (ev->pgno != page)
    return;

  /* Shouldn't block when the pipe buffer is full... ? */
  write(pipe[1], "", 1);
}
