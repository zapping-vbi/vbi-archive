/*
 *  Zapping TV viewer
 *
 *  Copyright (C) 2000, 2001, 2002 Iñaki García Etxebarria
 *  Copyright (C) 2000, 2001, 2002, 2003, 2004 Michael H. Schimek
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* $Id: view.c,v 1.8 2004-11-11 14:34:27 mschimek Exp $ */

/*
 *  Zapping (TV viewer for the Gnome Desktop)
 *
 * Copyright (C) 2001 Iñaki García Etxebarria
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

/* $Id: view.c,v 1.8 2004-11-11 14:34:27 mschimek Exp $ */

#include "config.h"

#include <gdk/gdkx.h>

#define ZAPPING8

#include "libvbi/exp-gfx.h"
#include "libvbi/exp-txt.h"
#include "src/zmisc.h"
#include "src/subtitle.h"
#include "src/remote.h"
#include "preferences.h"	/* teletext_foo_enum[] */
#include "export.h"
#include "search.h"
#include "main.h"
#include "view.h"

#define BLINK_CYCLE 300 /* ms */

#define GCONF_DIR "/apps/zapping/plugins/teletext"

enum {
  REQUEST_CHANGED,
  CHARSET_CHANGED,
  N_SIGNALS
};

static GObjectClass *		parent_class;

static guint			signals[N_SIGNALS];

static GdkCursor *		cursor_normal;
static GdkCursor *		cursor_link;
static GdkCursor *		cursor_select;

static GdkAtom			GA_CLIPBOARD		= GDK_NONE;

static vbi3_wst_level		teletext_level		= VBI3_WST_LEVEL_1p5;
static vbi3_charset_code	default_charset		= 0;
static GdkInterpType		interp_type		= GDK_INTERP_HYPER;
static gboolean			rolling_header		= TRUE;
static gboolean			live_clock		= TRUE;
static gboolean			hex_pages		= TRUE;
static gint			brightness		= 128;
static gint			contrast		= 64;
static gint			navigation		= 2;
static gboolean			hyperlinks		= TRUE;

enum {
  TARGET_LAT1_STRING,
  TARGET_UTF8_STRING,
  TARGET_PIXMAP
};

static const GtkTargetEntry
clipboard_targets [] = {
  { "STRING",	   0, TARGET_LAT1_STRING },
  { "UTF8_STRING", 0, TARGET_UTF8_STRING },
  { "PIXMAP",	   0, TARGET_PIXMAP },
};






/*
	Scaling and drawing
*/

static void
draw_scaled_page_image		(TeletextView *		view,
				 GdkDrawable *		drawable,
				 GdkGC *		gc,
				 gint			src_x,
				 gint			src_y,
				 gint			dest_x,
				 gint			dest_y,
				 gint			width,
				 gint			height)
{
  gint sw;
  gint sh;

  if (!view->scaled_on)
    return;

  sw = gdk_pixbuf_get_width (view->scaled_on);
  sh = gdk_pixbuf_get_height (view->scaled_on);

  gdk_draw_pixbuf (/* dst */ drawable, gc,
		   /* src */ view->scaled_on,
		   src_x, src_y,
		   dest_x, dest_y,
		   MIN (width, sw),
		   MIN (height, sh),
		   GDK_RGB_DITHER_NORMAL,
		   /* dither offset */ src_x, src_y);
}

/* This whole patch stuff is overkill. It would probably suffice to create
   a view->scaled_off and just one "patch" for the header. */

#define CW 12
#define CH 10

static void
destroy_patch			(struct ttx_patch *	p)
{
  g_assert (NULL != p);

  if (p->scaled_on)
    g_object_unref (G_OBJECT (p->scaled_on));

  if (p->scaled_off)
    g_object_unref (G_OBJECT (p->scaled_off));

  if (p->unscaled_on)
    g_object_unref (G_OBJECT (p->unscaled_on));

  if (p->unscaled_off)
    g_object_unref (G_OBJECT (p->unscaled_off));

  CLEAR (*p);
}

/* Creates p->scaled_on/off from p->unscaled_on/off.
   sw, sh: view->scaled_on size.
   uw, uh: view->unscaled_on size.
*/
static void
scale_patch			(struct ttx_patch *	p,
				 guint			sw,
				 guint			sh,
				 guint			uw,
				 guint			uh)
{
  guint srcw;
  guint srch;
  guint dstw;
  guint dsth;
  gint n;

  g_assert (NULL != p);

  if (p->scaled_on)
    {
      g_object_unref (G_OBJECT (p->scaled_on));
      p->scaled_on = NULL;
    }

  if (p->scaled_off)
    {
      g_object_unref (G_OBJECT (p->scaled_off));
      p->scaled_off = NULL;
    }

  srch = p->height * CH + 10;
  dsth = (sh * srch + (uh >> 1)) / uh;
  n = (0 == p->row) ? 0 : 5;
  p->sy = dsth * n / srch;
  p->sh = ceil (dsth * (n + p->height * CH) / (double) srch) - p->sy;
  p->dy = p->sy + (int) floor (sh * p->row * CH / (double) uh
			       - dsth * n / (double) srch + .5);
 
  srcw = p->width * p->columns * CW + 10;
  dstw = (sw * srcw + (uw >> 1)) / uw;
  n = (0 == p->column) ? 0 : 5;
  p->sx = dstw * n / srcw;
  p->sw = ceil (dstw * (n + p->width * p->columns * CW)
		/ (double) srcw) - p->sx;
  p->dx = p->sx + (int) floor (sw * p->column * CW / (double) uw
			       - dstw * n / (double) srcw + .5);

  if (dstw > 0 && dsth > 0)
    {
      p->scaled_on =
	z_pixbuf_scale_simple (p->unscaled_on,
			       (gint) dstw, (gint) dsth,
			       interp_type);

      if (p->flash)
	p->scaled_off =
	  z_pixbuf_scale_simple (p->unscaled_off,
				 (gint) dstw, (gint) dsth,
				 interp_type);

      p->dirty = TRUE;
    }
}

static void
delete_patches			(TeletextView *		view)
{
  struct ttx_patch *p;
  struct ttx_patch *end;

  end = view->patches + view->n_patches;

  for (p = view->patches; p < end; ++p)
    destroy_patch (p);

  g_free (view->patches);

  view->patches = NULL;
  view->n_patches = 0;
}

/* Copies dirty or flashing view->patches into view->scaled_on.
   draw: expose changed areas for later redrawing. */
static void
apply_patches			(TeletextView *		view,
				 gboolean		draw)
{
  GdkWindow *window;
  struct ttx_patch *p;
  struct ttx_patch *end;
  guint rows;
  guint columns;

  if (!view->pg)
    return;

  window = GTK_WIDGET (view)->window;

  rows = view->pg->rows;
  columns = view->pg->columns;

  end = view->patches + view->n_patches;

  for (p = view->patches; p < end; ++p)
    {
      GdkPixbuf *scaled;

      if (p->flash)
	{
	  p->phase = (p->phase + 1) & 15;

	  if (0 == p->phase)
	    scaled = p->scaled_off;
	  else if (4 == p->phase)
	    scaled = p->scaled_on;
	  else
	    continue;
	}
      else
	{
	  if (!p->dirty || !p->scaled_on)
	    continue;

	  p->dirty = FALSE;

	  scaled = p->scaled_on;
	}

      /* Update the scaled version of the page */
      if (view->scaled_on)
	{
	  z_pixbuf_copy_area (/* src */ scaled, p->sx, p->sy, p->sw, p->sh,
			      /* dst */ view->scaled_on, p->dx, p->dy);

	  if (draw)
	    gdk_window_clear_area_e (window, p->dx, p->dy, p->sw, p->sh);
	}
    }
}

static void
scale_patches			(TeletextView *		view)
{
  struct ttx_patch *p;
  struct ttx_patch *end;
  guint sw;
  guint sh;
  guint uw;
  guint uh;

  if (!view->scaled_on)
    return;

  g_assert (NULL != view->unscaled_on);

  sw = gdk_pixbuf_get_width (view->scaled_on);
  sh = gdk_pixbuf_get_height (view->scaled_on);

  uw = gdk_pixbuf_get_width (view->unscaled_on);
  uh = gdk_pixbuf_get_height (view->unscaled_on);

  end = view->patches + view->n_patches;

  for (p = view->patches; p < end; ++p)
    scale_patch (p, sw, sh, uw, uh);
}

static void
add_patch			(TeletextView *		view,
				 guint			column,
				 guint			row,
				 guint			columns,
				 vbi3_size		size,
				 gboolean		flash)
{
  struct ttx_patch *p;
  struct ttx_patch *end;
  gint pw, ph;
  gint ux, uy;
  guint endcol;

  g_assert (NULL != view->unscaled_on);
  g_assert (NULL != view->unscaled_off);

  end = view->patches + view->n_patches;

  endcol = column + columns;

  for (p = view->patches; p < end; ++p)
    if (p->row == row
	&& p->column < endcol
        && (p->column + p->columns) > column)
      {
	/* Patches overlap, we replace the old one. */
	destroy_patch (p);
	break;
      }

  if (p >= end)
    {
      guint size;

      size = (view->n_patches + 1) * sizeof (*view->patches); 
      view->patches = g_realloc (view->patches, size);

      p = view->patches + view->n_patches;
      ++view->n_patches;
    }

  p->column		= column;
  p->row		= row;
  p->scaled_on		= NULL;
  p->scaled_off		= NULL;
  p->unscaled_off	= NULL;
  p->columns		= columns;
  p->phase		= 0;
  p->flash		= flash;
  p->dirty		= TRUE;

  switch (size)
    {
    case VBI3_DOUBLE_WIDTH:
      p->width = 2;
      p->height = 1;
      break;

    case VBI3_DOUBLE_HEIGHT:
      p->width = 1;
      p->height = 2;
      break;

    case VBI3_DOUBLE_SIZE:
      p->width = 2;
      p->height = 2;
      break;

    default:
      p->width = 1;
      p->height = 1;
      break;
    }

  ux = (0 == p->column) ? 0 : p->column * CW - 5;
  uy = (0 == p->row) ? 0 : p->row * CH - 5;

  pw = p->width * p->columns * CW + 10;
  ph = p->height * CH + 10;

  p->unscaled_on = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, pw, ph);
  g_assert (NULL != p->unscaled_on);
  z_pixbuf_copy_area (/* src */ view->unscaled_on, ux, uy, pw, ph,
		      /* dst */ p->unscaled_on, 0, 0);

  if (flash)
    {
      p->unscaled_off = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, pw, ph);
      g_assert (p->unscaled_off != NULL);
      z_pixbuf_copy_area (/* src */ view->unscaled_off, ux, uy, pw, ph,
			  /* dst */ p->unscaled_off, 0, 0);
    }

  if (view->scaled_on)
    {
      guint sw, sh;
      guint uw, uh;

      sw = gdk_pixbuf_get_width (view->scaled_on);
      sh = gdk_pixbuf_get_height (view->scaled_on);

      uw = gdk_pixbuf_get_width (view->unscaled_on);
      uh = gdk_pixbuf_get_height (view->unscaled_on);

      scale_patch (p, sw, sh, uw, uh);
    }
}

static void
build_patches			(TeletextView *		view)
{
  vbi3_char *cp;
  guint row;

  delete_patches (view);

  if (!view->pg)
    return;

  cp = view->pg->text;

  for (row = 0; row < view->pg->rows; ++row)
    {
      guint col = 0;

      while (col < view->pg->columns)
	{
	  if ((cp[col].attr & VBI3_FLASH)
	      && cp[col].size <= VBI3_DOUBLE_SIZE)
	    {
	      guint n;

	      for (n = 1; col + n < view->pg->columns; ++n)
		if (!(cp[col + n].attr & VBI3_FLASH)
		    || cp[col].size != cp[col + n].size)
		  break;

	      add_patch (view, col, row, n, cp[col].size, /* flash */ TRUE);

	      col += n;
	    }
	  else
	    {
	      ++col;
	    }
	}

      cp += view->pg->columns;
    }
}

static gboolean
vbi3_page_has_flash		(const vbi3_page *	pg)
{
  const vbi3_char *cp;
  const vbi3_char *end;
  guint attr;
 
  end = pg->text + pg->rows * pg->columns;
  attr = 0;
 
  for (cp = pg->text; cp < end; ++cp)
    attr |= cp->attr;
 
  return !!(attr & VBI3_FLASH);
}

static void
create_empty_image		(TeletextView *		view)
{
  gchar *filename;
  GdkPixbuf *pixbuf;
  gint sw;
  gint sh;
  double sx;
  double sy;

  if (!view->scaled_on)
    return;

  filename = g_strdup_printf ("%s/vt_loading%d.jpeg",
			      PACKAGE_PIXMAPS_DIR,
			      (rand () & 1) + 1);

  pixbuf = gdk_pixbuf_new_from_file (filename, NULL);

  g_free (filename);

  if (!pixbuf)
    return;

  sw = gdk_pixbuf_get_width (view->scaled_on);
  sh = gdk_pixbuf_get_height (view->scaled_on);

  sx = sw / (double) gdk_pixbuf_get_width (pixbuf);
  sy = sh / (double) gdk_pixbuf_get_height (pixbuf);

  gdk_pixbuf_scale (/* src */ pixbuf,
		    /* dst */ view->scaled_on, 0, 0, sw, sh,
		    /* offset */ 0.0, 0.0,
		    /* scale */ sx, sy,
		    interp_type);

  g_object_unref (G_OBJECT (pixbuf));

  delete_patches (view);
}

static void
create_page_images_from_pg	(TeletextView *		view)
{
  vbi3_image_format format;
  vbi3_bool success;

  if (!view->pg)
    {
      create_empty_image (view);
      return;
    }

  g_assert (NULL != view->unscaled_on);

  CLEAR (format);

  format.width = gdk_pixbuf_get_width (view->unscaled_on);
  format.height = gdk_pixbuf_get_height (view->unscaled_on);
  format.pixfmt = VBI3_PIXFMT_RGBA24_LE;
  format.bytes_per_line = gdk_pixbuf_get_rowstride (view->unscaled_on);
  format.size = format.width * format.height * 4;

  success = vbi3_page_draw_teletext
    (view->pg,
     gdk_pixbuf_get_pixels (view->unscaled_on),
     &format,
     VBI3_BRIGHTNESS, brightness,
     VBI3_CONTRAST, contrast,
     VBI3_REVEAL, (vbi3_bool) view->reveal,
     VBI3_FLASH_ON, TRUE,
     0);

  g_assert (success);

  if (view->scaled_on)
    {
      gint sw;
      gint sh;
      double sx;
      double sy;

      sw = gdk_pixbuf_get_width (view->scaled_on);
      sh = gdk_pixbuf_get_height (view->scaled_on);
  
      sx = sw / (double) format.width;
      sy = sh / (double) format.height;
 
      gdk_pixbuf_scale (/* src */ view->unscaled_on,
			/* dst */ view->scaled_on, 0, 0, sw, sh,
			/* offset */ 0.0, 0.0,
			/* scale */ sx, sy,
			interp_type);
    }

  if (vbi3_page_has_flash (view->pg))
    {
      success = vbi3_page_draw_teletext
	(view->pg,
	 gdk_pixbuf_get_pixels (view->unscaled_off),
	 &format,
	 VBI3_BRIGHTNESS, brightness,
	 VBI3_CONTRAST, contrast,
	 VBI3_REVEAL, (vbi3_bool) view->reveal,
	 VBI3_FLASH_ON, FALSE,
	 0);

      g_assert (success);

      build_patches (view);
    }
  else
    {
      delete_patches (view);
    }
}

static gboolean
resize_scaled_page_image	(TeletextView *		view,
				 gint			width,
				 gint			height)
{
  if (width <= 0 || height <= 0)
    return FALSE;

  if (!view->scaled_on
      || width != gdk_pixbuf_get_width (view->scaled_on)
      || height != gdk_pixbuf_get_height (view->scaled_on))
    {
      double sx;
      double sy;

      g_assert (NULL != view->unscaled_on);

      if (view->scaled_on)
	g_object_unref (G_OBJECT (view->scaled_on));

      view->scaled_on =
	gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
      g_assert (NULL != view->scaled_on);

      if (view->pg)
	{
	  sx = width / (double) gdk_pixbuf_get_width (view->unscaled_on);
	  sy = height / (double) gdk_pixbuf_get_height (view->unscaled_on);

	  gdk_pixbuf_scale (/* src */ view->unscaled_on,
			    /* dst */ view->scaled_on, 0, 0, width, height,
			    /* offset */ 0.0, 0.0,
			    /* scale */ sx, sy,
			    interp_type);

	  scale_patches (view);
	}
      else
	{
	  /* Renders "loading" directly into view->scaled_on. */
	  create_empty_image (view);
	}
    }

  return TRUE;
}

/*
	Browser history
*/

/* Note history.stack[top - 1] is the current page. */

static void
history_dump			(TeletextView *		view)
{
  guint i;

  fprintf (stderr, "top=%u size=%u ",
	   view->history.top,
	   view->history.size);

  for (i = 0; i < view->history.size; ++i)
    fprintf (stderr, "%03x ",
	     view->history.stack[i].pgno);

  fputc ('\n', stderr);
}

static void
history_update_gui		(TeletextView *		view)
{
  GtkAction *action;

  action = gtk_action_group_get_action (view->action_group, "HistoryBack");
  z_action_set_sensitive (action, view->history.top >= 2);

  action = gtk_action_group_get_action (view->action_group, "HistoryForward");
  z_action_set_sensitive (action, view->history.top < view->history.size);
}

/* Push *current page* onto history stack. Actually one should push
   the page being replaced, but things are simpler this way. */
static void
history_push			(TeletextView *		view,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno)
{
  guint top;

  top = view->history.top;

  if (NULL == nk)
    nk = &view->req.network;

  if (pgno < 0x100 || pgno > 0x899) 
    return;

  if (top > 0)
    {
      if (page_num_equal2 (&view->history.stack[top - 1],
			   nk, pgno, subno))
	return; /* already stacked */

      if (top >= N_ELEMENTS (view->history.stack))
	{
	  top = N_ELEMENTS (view->history.stack) - 1;

	  memmove (view->history.stack,
		   view->history.stack + 1,
		   top * sizeof (*view->history.stack));
	}
      else if (view->history.size > top)
	{
	  if (page_num_equal2 (&view->history.stack[top],
			       nk, pgno, subno))
	    {
	      /* Apparently we move forward in history, no new page. */
	      view->history.top = top + 1;
	      history_update_gui (view);
	      return;
	    }

	  /* Will discard future branch. */
	}
    }

  page_num_set (&view->history.stack[top], nk, pgno, subno);

  ++top;

  view->history.top = top;
  view->history.size = top;

  history_update_gui (view);

  if (0)
    history_dump (view);
}

static void
history_back_action		(GtkAction *		action _unused_,
				 TeletextView *		view)
{
  guint top;

  top = view->history.top;

  if (top >= 2)
    {
      page_num *sp;

      view->history.top = top - 1;
      history_update_gui (view);

      sp = &view->history.stack[top - 2];

      teletext_view_load_page (view, &sp->network, sp->pgno, sp->subno);
    }
}

static PyObject *
py_ttx_history_prev		(PyObject *		self _unused_,
				 PyObject *		args _unused_)
{
  TeletextView *view;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  history_back_action (NULL, view);

  py_return_true;
}

static void
history_forward_action		(GtkAction *		action _unused_,
				 TeletextView *		view)
{
  guint top;

  top = view->history.top;

  if (top < view->history.size)
    {
      page_num *sp;

      view->history.top = top + 1;
      history_update_gui (view);

      sp = &view->history.stack[top];

      teletext_view_load_page (view, &sp->network, sp->pgno, sp->subno);
    }
}

static PyObject *
py_ttx_history_next		(PyObject *		self _unused_,
				 PyObject *		args _unused_)
{
  TeletextView *view;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  history_forward_action (NULL, view);

  py_return_true;
}

/*
	Page rendering
*/

static void
update_cursor_shape		(TeletextView *		view)
{
  gint x;
  gint y;
  GdkModifierType mask;
  vbi3_link link;
  gchar *buffer;
  gboolean success;

  gdk_window_get_pointer (GTK_WIDGET (view)->window, &x, &y, &mask);

  link.type = VBI3_LINK_NONE;

  success = teletext_view_vbi3_link_from_pointer_position (view, &link, x, y);

  switch (link.type)
    {
    case VBI3_LINK_PAGE:
      buffer = g_strdup_printf (_(" Page %x"), link.pgno);
      goto show;

    case VBI3_LINK_SUBPAGE:
      buffer = g_strdup_printf (_(" Subpage %x"), link.subno & 0xFF);
      goto show;

    case VBI3_LINK_HTTP:
    case VBI3_LINK_FTP:
    case VBI3_LINK_EMAIL:
      buffer = g_strconcat (" ", link.url, NULL);
      goto show;

    show:
      if (!view->cursor_over_link)
	{
	  view->cursor_over_link = TRUE;

	  if (view->appbar)
	    gnome_appbar_push (GNOME_APPBAR (view->appbar), buffer);

	  gdk_window_set_cursor (GTK_WIDGET (view)->window, cursor_link);
	}
      else
	{
	  if (view->appbar)
	    gnome_appbar_set_status (GNOME_APPBAR (view->appbar), buffer);
	}

      g_free (buffer);

      break;

    default:
      if (view->cursor_over_link)
	{
	  view->cursor_over_link = FALSE;

	  if (view->appbar)
	    gnome_appbar_pop (GNOME_APPBAR (view->appbar));

	  gdk_window_set_cursor (GTK_WIDGET (view)->window, cursor_normal);
	}

      break;
    }

  if (success)
    vbi3_link_destroy (&link);
}

static gboolean
redraw_view			(TeletextView *		view)
{
  GtkAction *action;
  GdkWindow *window;
  vbi3_page *pg;
  gint width;
  gint height;

  action = gtk_action_group_get_action (view->action_group, "Export");
  z_action_set_sensitive (action,
			  NULL != vbi3_export_info_enum (0)
			  && view->pg
			  && view->pg->pgno >= 0x100);

  if (view->selecting)
    return FALSE;

  create_page_images_from_pg (view);

  apply_patches (view, /* draw */ FALSE);

  if (!(window = GTK_WIDGET (view)->window))
    return FALSE;

  gdk_window_get_geometry (window,
			   /* x */ NULL,
			   /* y */ NULL,
			   &width,
			   &height,
			   /* depth */ NULL);

  /* XXX if we have rolling header and this is an old page from
     cache we'll show an old header. */

  /* Trigger expose event for later redrawing. */
  gdk_window_clear_area_e (window, 0, 0, width, height);

  if ((pg = view->pg))
    {
      if (view->toolbar)
	{
	  if (view->freezed)
	    teletext_toolbar_set_url (view->toolbar,
				      pg->pgno,
				      pg->subno);
	  else
	    teletext_toolbar_set_url (view->toolbar,
				      view->req.pgno,
				      view->req.subno);
	}

      /* Note does nothing if this page is already at TOS. */ 
      history_push (view, pg->network, pg->pgno, pg->subno);
    }

  update_cursor_shape (view);
  
  return TRUE;
}

static void
redraw_all_views		(void)
{
  GList *p;

  for (p = g_list_first (teletext_views); p; p = p->next)
    {
      TeletextView *view = p->data;

      if (view->pg)
	redraw_view (view);
    }
}

static vbi3_page *
get_page			(const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 vbi3_charset_code	charset)
{
  vbi3_page *pg;

  if (nk && vbi3_network_is_anonymous (nk))
    nk = NULL; /* use currently received network */

  if ((int) charset >= 0)
    {
      pg = vbi3_teletext_decoder_get_page
	(td, nk, pgno, subno,
	 VBI3_41_COLUMNS, TRUE, /* add_column, */
	 /* VBI3_PANELS, FALSE, */
	 VBI3_NAVIGATION, navigation,
	 VBI3_HYPERLINKS, hyperlinks,
	 /* VBI3_PDC_LINKS, TRUE, */
	 VBI3_WST_LEVEL, teletext_level,
	 VBI3_OVERRIDE_CHARSET_0, charset,
	 0);
    }
  else
    {
      pg = vbi3_teletext_decoder_get_page
	(td, nk, pgno, subno,
	 VBI3_41_COLUMNS, TRUE, /* add_column, */
	 /* VBI3_PANELS, FALSE, */
	 VBI3_NAVIGATION, navigation,
	 VBI3_HYPERLINKS, hyperlinks,
	 /* VBI3_PDC_LINKS, TRUE, */
	 VBI3_WST_LEVEL, teletext_level,
	 VBI3_DEFAULT_CHARSET_0, default_charset,
	 0);
    }

  return pg;
}

static void
reformat_view			(TeletextView *		view)
{
  vbi3_page *pg;

  if ((pg = get_page (view->pg->network,
		      view->pg->pgno,
		      view->pg->subno,
		      view->charset)))
    {
      vbi3_page_unref (view->pg);
      view->pg = pg;

      redraw_view (view);
    }
}

static void
reformat_all_views		(void)
{
  GList *p;

  for (p = g_list_first (teletext_views); p; p = p->next)
    {
      TeletextView *view = p->data;

      if (view->selecting)
	continue;

      if (view->freezed)
	continue;

      if (view->pg)
	reformat_view (view);
    }
}

static void
update_header			(TeletextView *		view,
				 const vbi3_event *	ev)
{
  vbi3_image_format format;
  vbi3_page *pg;
  vbi3_bool success;
  guint column;
  guint i;
  uint8_t *buffer;

  if (!view->pg)
    return;

  if (view->pg->pgno == view->req.pgno
      || !rolling_header)
    {
      if (!live_clock)
	return;

      column = 32; /* only clock */
    }
  else
    {
      if (!(ev->ev.ttx_page.flags & VBI3_SERIAL))
	if (0 != ((ev->ev.ttx_page.pgno ^ view->req.pgno) & 0xF00))
	  return;

      column = 8; /* page number and clock */
    }

  if ((int) view->charset >= 0)
    {
      pg = vbi3_teletext_decoder_get_page
	(td,
	 ev->network,
	 ev->ev.ttx_page.pgno,
	 ev->ev.ttx_page.subno,
	 VBI3_41_COLUMNS, TRUE,
	 VBI3_HEADER_ONLY, TRUE,
	 VBI3_WST_LEVEL, VBI3_WST_LEVEL_1p5,
	 VBI3_OVERRIDE_CHARSET_0, view->charset,
	 0);
    } else {
      pg = vbi3_teletext_decoder_get_page
	(td,
	 ev->network,
	 ev->ev.ttx_page.pgno,
	 ev->ev.ttx_page.subno,
	 VBI3_41_COLUMNS, TRUE,
	 VBI3_HEADER_ONLY, TRUE,
	 VBI3_WST_LEVEL, VBI3_WST_LEVEL_1p5,
	 VBI3_DEFAULT_CHARSET_0, default_charset,
	 0);
    }

  if (!pg)
    return;

  for (i = column; i < 40; ++i)
    if (view->pg->text[i].unicode != pg->text[i].unicode)
      break;

  if (i >= 40) {
    vbi3_page_unref (pg);
    return;
  }

  /* Some networks put level 2.5 elements into the header, keep that. */
  if (view->pg->pgno == view->req.pgno)
    for (i = 32; i < 40; ++i)
      {
	guint unicode;

	unicode = pg->text[i].unicode;
	pg->text[i] = view->pg->text[i];
	pg->text[i].unicode = unicode;
      }

  CLEAR (format);

  format.width = gdk_pixbuf_get_width (view->unscaled_on);
  format.height = gdk_pixbuf_get_height (view->unscaled_on);
  format.pixfmt = VBI3_PIXFMT_RGBA24_LE;
  format.bytes_per_line = gdk_pixbuf_get_rowstride (view->unscaled_on);
  format.size = format.width * format.height * 4;

  buffer = gdk_pixbuf_get_pixels (view->unscaled_on);
  buffer += column * (CW * 4);

  success = vbi3_page_draw_teletext_region (pg,
					   buffer,
					   &format,
					   column,
					   /* row */ 0,
					   /* width */ 40 - column,
					   /* height */ 1,
					   VBI3_BRIGHTNESS, brightness,
					   VBI3_CONTRAST, contrast,
					   VBI3_REVEAL, TRUE,
					   VBI3_FLASH_ON, TRUE,
					   0);
  g_assert (success);

  add_patch (view,
	     column,
	     /* row */ 0,
	     /* columns */ 40 - column,
	     VBI3_NORMAL_SIZE,
	     /* flash */ FALSE);

  vbi3_page_unref (pg);
}

static vbi3_bool
view_vbi3_event_handler		(const vbi3_event *	ev,
				 void *			user_data _unused_)
{
  GList *p;

  switch (ev->type) {
  case VBI3_EVENT_NETWORK:
    if (0)
      {
	_vbi3_network_dump (ev->network, stderr);
	fputc ('\n', stderr);
      }

    for (p = g_list_first (teletext_views); p; p = p->next)
      {
	TeletextView *view;

	view = (TeletextView *) p->data;

	if (!vbi3_network_is_anonymous (&view->req.network))
	  continue;

	if ((unsigned int) -1 != view->charset)
	  {
	    /* XXX should use default charset of the new network
	       from config if one exists. */
	    view->charset = -1;

	    g_signal_emit (view, signals[CHARSET_CHANGED], 0);
	  }

	if (view->selecting)
	  continue;

	if (view->freezed)
	  continue;

	vbi3_page_unref (view->pg);

	/* Change to view of same page of the new network, such that
	   header updates match the rest of the page.  When the page
	   is not cached redraw_view() displays "loading". */
	view->pg = get_page (ev->network,
			     view->req.pgno,
			     view->req.subno,
			     view->charset);

	redraw_view (view);
      }

    break;

  case VBI3_EVENT_TTX_PAGE:
    for (p = g_list_first (teletext_views); p; p = p->next)
      {
	TeletextView *view;

	view = (TeletextView *) p->data;

	if (view->selecting)
	  continue;

	if (view->freezed)
	  continue;

	if (!vbi3_network_is_anonymous (&view->req.network)
	    && !vbi3_network_equal (&view->req.network, ev->network))
	  continue;

	if (ev->ev.ttx_page.pgno == view->req.pgno
	    && (VBI3_ANY_SUBNO == view->req.subno
		|| ev->ev.ttx_page.subno == view->req.subno))
	  {
	    vbi3_page *pg;

	    if ((pg = get_page (ev->network,
				ev->ev.ttx_page.pgno,
				ev->ev.ttx_page.subno,
				view->charset)))
	      {
		vbi3_page_unref (view->pg);
		view->pg = pg;

		redraw_view (view);
	      }
	  }
	else if (ev->ev.ttx_page.flags & VBI3_ROLL_HEADER)
	  {
	    update_header (view, ev);
	  }
      }

    break;

  default:
    break;
  }

  return FALSE; /* pass on */
}

static void
set_hold			(TeletextView *		view,
				 gboolean		hold)
{
  if (view->toolbar)
    teletext_toolbar_set_hold (view->toolbar, hold);

  if (hold != view->hold)
    {
      const vbi3_page *pg;

      view->hold = hold;

      if ((pg = view->pg))
	{
	  if (hold)
	    view->req.subno = pg->subno;
	  else
	    view->req.subno = VBI3_ANY_SUBNO;
	}
    }
}

static void
monitor_pgno			(TeletextView *		view,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno,
				 vbi3_charset_code	charset)
{
  vbi3_page *pg;

  view->freezed = FALSE;

  if (!nk)
    nk = &view->req.network;

  page_num_set (&view->req, nk, pgno, subno);

  g_signal_emit (view, signals[REQUEST_CHANGED], 0);

  pg = NULL;

  if (pgno >= 0x100 && pgno <= 0x899)
    pg = get_page (nk, pgno, subno, charset);

  if (pg || !rolling_header)
    {
      vbi3_page_unref (view->pg);
      view->pg = pg; /* can be NULL */
    }

  redraw_view (view);
}

static gboolean
deferred_load_timeout		(gpointer		user_data)
{
  TeletextView *view = user_data;

  view->deferred.timeout_id = NO_SOURCE_ID;

  monitor_pgno (view,
		&view->deferred.network,
		view->deferred.pgno,
		view->deferred.subno,
		view->charset);

  view->deferred_load = FALSE;

  return FALSE; /* don't call again */
}

void
teletext_view_load_page		(TeletextView *		view,
				 const vbi3_network *	nk,
				 vbi3_pgno		pgno,
				 vbi3_subno		subno)
{
  view->hold = (VBI3_ANY_SUBNO != subno);
  set_hold (view, view->hold);

  /* XXX this was intended to override a subtitle page character set
     code, but on a second thought resetting on page change is strange. */
  if (0)
    if ((int) view->charset >= 0)
      {
	/* XXX actually we should query config for a
	   network-pgno-subno - charset pair, reset to default
	   only if none exists. */
	view->charset = -1;
	
	g_signal_emit (view, signals[CHARSET_CHANGED], 0);
      }

  if (view->toolbar)
    teletext_toolbar_set_url (view->toolbar, pgno, subno);

  if (view->appbar)
    {
      gchar *buffer;

      if (pgno >= 0x100 && pgno <= 0x8FF)
	{
	  if (0 == subno || VBI3_ANY_SUBNO == subno)
	    buffer = g_strdup_printf (_("Loading page %X..."), pgno);
	  else
	    buffer = g_strdup_printf (_("Loading page %X.%02X..."),
				      pgno, subno & 0x7F);
	}
      else
	{
	  buffer = g_strdup_printf ("Invalid page %X.%X", pgno, subno);
	}

      gnome_appbar_set_status (view->appbar, buffer);

      g_free (buffer);
    }

  gtk_widget_grab_focus (GTK_WIDGET (view));

  if (nk)
    network_set (&view->deferred.network, nk);
  else
    network_set (&view->deferred.network, &view->req.network);

  view->deferred.pgno = pgno;
  view->deferred.subno = subno;

  if (view->deferred.timeout_id > 0)
    g_source_remove (view->deferred.timeout_id);

  if (view->deferred_load)
    {
      view->deferred.timeout_id =
	g_timeout_add (300, (GSourceFunc) deferred_load_timeout, view);
    }
  else
    {
      view->deferred.timeout_id = NO_SOURCE_ID;

      monitor_pgno (view, nk, pgno, subno, view->charset);
    }

  z_update_gui ();
}

void
teletext_view_show_page		(TeletextView *		view,
				 vbi3_page *		pg)
{
  if (NULL == pg)
    return;

  view->hold = TRUE;
  set_hold (view, view->hold);

  if (view->toolbar)
    teletext_toolbar_set_url (view->toolbar, pg->pgno, pg->subno);

  if (view->appbar)
    gnome_appbar_set_status (view->appbar, "");

  gtk_widget_grab_focus (GTK_WIDGET (view));

  if (view->deferred.timeout_id > 0)
    g_source_remove (view->deferred.timeout_id);

  page_num_set (&view->req, pg->network, pg->pgno, pg->subno);

  g_signal_emit (view, signals[REQUEST_CHANGED], 0);

  if ((int) view->charset >= 0)
    {
      /* XXX actually we should query config for a
	 network-pgno-subno - charset pair, reset to default
	 only if none exists. */
      view->charset = -1;

      g_signal_emit (view, signals[CHARSET_CHANGED], 0);
    }

  vbi3_page_unref (view->pg);
  view->pg = vbi3_page_ref (pg);

  view->freezed = TRUE;

  redraw_view (view);

  z_update_gui ();
}

/*
	User interface
*/

TeletextView *
teletext_view_from_widget	(GtkWidget *		widget)
{
  TeletextView *view;

  while (!(view = (TeletextView *)
	   g_object_get_data (G_OBJECT (widget), "TeletextView")))
    {
      if (!(widget = widget->parent))
	return NULL;
    }

  return view;
}

static void
set_transient_for		(GtkWindow *		window,
				 TeletextView *		view)
{
  GtkWidget *parent;

  parent = GTK_WIDGET (view);

  while (parent)
    {
      if (GTK_IS_WINDOW (parent))
	{
	  gtk_window_set_transient_for (window,	GTK_WINDOW (parent));
	  break;
	}

      parent = parent->parent;
    }
}

/* Note on success you must vbi3_link_destroy. */
gboolean
teletext_view_vbi3_link_from_pointer_position
				(TeletextView *		view,
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
  const vbi3_char *ac;

  lk->type = VBI3_LINK_NONE;

  if (x < 0 || y < 0)
    return FALSE;

  if (!(pg = view->pg))
    return FALSE;

  if (!(window = GTK_WIDGET (view)->window))
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

  if (column >= pg->columns
      || row >= pg->rows)
    return FALSE;

  ac = pg->text + row * pg->columns + column;

  if (!(ac->attr & VBI3_LINK))
    return FALSE;

  return vbi3_page_get_hyperlink (pg, lk, column, row);
}

/*
	Page commands
*/

static gint
decimal_subno			(vbi3_subno		subno)
{
  if (0 == subno || (guint) subno > 0x99)
    return -1; /* any */
  else
    return vbi3_bcd2dec (subno);
}

static PyObject *
py_ttx_hold			(PyObject *		self _unused_,
				 PyObject *		args)
{
  TeletextView *view;
  gint hold;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  hold = -1;

  if (!ParseTuple (args, "|i", &hold))
    g_error ("zapping.ttx_hold(|i)");

  if (hold < 0)
    hold = !view->hold;
  else
    hold = !!hold;

  set_hold (view, hold);

  py_return_true;
}

static PyObject *
py_ttx_reveal			(PyObject *		self _unused_,
				 PyObject *		args)
{
  TeletextView *view;
  gint reveal;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  reveal = -1;

  if (!ParseTuple (args, "|i", &reveal))
    g_error ("zapping.ttx_reveal(|i)");

  if (reveal < 0)
    reveal = !view->reveal;
  else
    reveal = !!reveal;

  if (view->toolbar)
    teletext_toolbar_set_reveal (view->toolbar, reveal);

  view->reveal = reveal;

  if (view->pg)
    redraw_view (view);

  py_return_true;
}

static PyObject *
py_ttx_open			(PyObject *		self _unused_,
				 PyObject *		args)
{
  TeletextView *view;
  int page;
  int subpage;
  vbi3_pgno pgno;
  vbi3_subno subno;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  page = 100;
  subpage = -1;

  if (!ParseTuple (args, "|ii", &page, &subpage))
    g_error ("zapping.ttx_open_new(|ii)");

  if (page >= 100 && page <= 899)
    pgno = vbi3_dec2bcd (page);
  else
    py_return_false;

  if (subpage < 0)
    subno = VBI3_ANY_SUBNO;
  else if ((guint) subpage <= 99)
    subno = vbi3_dec2bcd (subpage);
  else
    py_return_false;

  teletext_view_load_page (view, &view->req.network, pgno, subno);

  py_return_true;
}

static PyObject *
py_ttx_page_incr		(PyObject *		self _unused_,
				 PyObject *		args)
{
  TeletextView *view;
  vbi3_pgno pgno;
  gint value;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  value = +1;

  if (!ParseTuple (args, "|i", &value))
    g_error ("zapping.ttx_page_incr(|i)");

  if (abs (value) > 999)
    py_return_false;

  if (value < 0)
    value += 1000;

  pgno = vbi3_add_bcd (view->req.pgno, vbi3_dec2bcd (value)) & 0xFFF;

  if (pgno < 0x100)
    pgno = 0x800 + (pgno & 0xFF);
  else if (pgno > 0x899)
    pgno = 0x100 + (pgno & 0xFF);

  teletext_view_load_page (view, &view->req.network, pgno, VBI3_ANY_SUBNO);

  py_return_true;
}

static PyObject *
py_ttx_subpage_incr		(PyObject *		self _unused_,
				 PyObject *		args)
{
  TeletextView *view;
  vbi3_subno subno;
  gint value;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  value = +1;

  if (!ParseTuple (args, "|i", &value))
    g_error ("zapping.ttx_subpage_incr(|i)");

  if (abs (value) > 99)
    py_return_false;

  if (value < 0)
    value += 100; /* XXX should use actual or anounced number of subp */

  subno = view->req.subno;
  if (VBI3_ANY_SUBNO == view->req.subno)
    {
      subno = 0;
      if (view->pg)
	subno = view->pg->subno;
    }

  subno = vbi3_add_bcd (subno, vbi3_dec2bcd (value)) & 0xFF;

  teletext_view_load_page (view, &view->req.network, view->req.pgno, subno);

  py_return_true;
}

static vbi3_pgno
default_home_pgno		(void)
{
  gint value;

  value = 100;
  if (z_gconf_get_int (&value, GCONF_DIR "/home_page"))
    value = SATURATE (value, 100, 899);

  return vbi3_dec2bcd (value);
}

static void
home_action			(GtkAction *		action _unused_,
				 TeletextView *		view)
{
  const vbi3_link *lk;

  if (!view->pg)
    return;

  lk = vbi3_page_get_home_link (view->pg);

  switch (lk->type)
    {
    case VBI3_LINK_PAGE:
    case VBI3_LINK_SUBPAGE:
      if (lk->pgno)
	{
	  teletext_view_load_page (view, lk->network, lk->pgno, lk->subno);
	}
      else
	{
	  teletext_view_load_page (view,
				   &view->req.network,
				   default_home_pgno (),
				   VBI3_ANY_SUBNO);
	}

      break;

    default:
      break;
    }
}

static PyObject *
py_ttx_home			(PyObject *		self _unused_,
				 PyObject *		args _unused_)
{
  TeletextView *view;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  home_action (NULL, view);

  py_return_true;
}

gboolean
teletext_view_switch_network	(TeletextView *		view,
				 const vbi3_network *	nk)
{
  if ((unsigned int) -1 != view->charset)
    {
      view->charset = -1;

      g_signal_emit (view, signals[CHARSET_CHANGED], 0);
    }

  teletext_view_load_page (view, nk, default_home_pgno (), VBI3_ANY_SUBNO);

  return TRUE;
}

gboolean
teletext_view_set_charset	(TeletextView *		view,
				 vbi3_charset_code	code)
{
  if (view->charset != code)
    {
      view->charset = code;

      g_signal_emit (view, signals[CHARSET_CHANGED], 0);

      reformat_view (view);
    }

  return TRUE;
}


/*
	Selection
*/

/* Called when another application claims the selection
   (i.e. sends new data to the clipboard). */
static gboolean
selection_clear_event		(GtkWidget *		widget,
				 GdkEventSelection *	event)
{
  TeletextView *view = TELETEXT_VIEW (widget);

  if (event->selection == GDK_SELECTION_PRIMARY)
    view->select.in_selection = FALSE;
  else if (event->selection == GA_CLIPBOARD)
    view->select.in_clipboard = FALSE;

  return FALSE; /* pass on */
}

/* Called when another application requests our selected data
   (overriden GtkWidget method). */
static void
selection_get			(GtkWidget *		widget,
				 GtkSelectionData *	selection_data,
				 guint			info,
				 guint			time _unused_)
{
  TeletextView *view = TELETEXT_VIEW (widget);

  if ((selection_data->selection == GDK_SELECTION_PRIMARY
       && view->select.in_selection)
      || (selection_data->selection == GA_CLIPBOARD
	  && view->select.in_clipboard))
    {
      switch (info)
	{
	case TARGET_LAT1_STRING:
	case TARGET_UTF8_STRING:
	  {
	    gint width;
	    gint height;
	    unsigned int size;
	    unsigned int actual;
	    char *buffer;

	    width = view->select.column2 - view->select.column1 + 1;
	    height = view->select.row2 - view->select.row1 + 1;

	    size = 25 * 64 * 4;
	    buffer = g_malloc (size);

	    actual = vbi3_print_page_region
	      (view->select.pg,
	       buffer,
	       size,
	       (TARGET_LAT1_STRING == info) ? "ISO-8859-1" : "UTF-8",
	       NULL, 0, /* standard line separator */
	       view->select.column1,
	       view->select.row1,
	       width,
	       height,
	       VBI3_TABLE, (vbi3_bool) view->select.table_mode,
	       VBI3_RTL, (vbi3_bool) view->select.rtl_mode,
	       VBI3_REVEAL, (vbi3_bool) view->select.reveal,
	       VBI3_FLASH_ON, TRUE,
	       0);

	    if (actual > 0)
	      {
		gtk_selection_data_set (selection_data,
					GDK_SELECTION_TYPE_STRING, 8,
					buffer, actual);
	      }

	    g_free (buffer);

	    break;
	  }

	case TARGET_PIXMAP:
	  {
	    const gint cell_width = 12; /* Teletext character cell size */
	    const gint cell_height = 10;
	    gint width;
	    gint height;
	    GdkPixmap *pixmap;
	    GdkPixbuf *pixbuf;
	    gint id[2];
	    vbi3_image_format format;
	    vbi3_bool success;

	    /* Selection is open (eg. 25,5 - 15,6). */
	    if (view->select.column2 < view->select.column1)
	      break;

	    width = view->select.column2 - view->select.column1 + 1;
	    height = view->select.row2 - view->select.row1 + 1;

	    pixmap = gdk_pixmap_new (GTK_WIDGET (view)->window,
				     width * cell_width,
				     height * cell_height,
				     -1 /* same depth as window */);

	    pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				     /* has_alpha */ TRUE,
				     /* bits_per_sample */ 8,
				     width * cell_width,
				     height * cell_height);

	    CLEAR (format);

	    format.width = gdk_pixbuf_get_width (pixbuf);
	    format.height = gdk_pixbuf_get_height (pixbuf);
	    format.pixfmt = VBI3_PIXFMT_RGBA24_LE;
	    format.bytes_per_line = gdk_pixbuf_get_rowstride (pixbuf);
	    format.size = format.width * format.height * 4;

	    success = vbi3_page_draw_teletext_region
	      (view->select.pg,
	       gdk_pixbuf_get_pixels (pixbuf),
	       &format,
	       view->select.column1,
	       view->select.row1,
	       width,
	       height,
	       VBI3_BRIGHTNESS, brightness,
	       VBI3_CONTRAST, contrast,
	       VBI3_REVEAL, (vbi3_bool) view->select.reveal,
	       VBI3_FLASH_ON, TRUE,
	       0);

	    g_assert (success);

	    gdk_draw_pixbuf (pixmap,
			     GTK_WIDGET (view)->style->white_gc,
			     pixbuf,
			     /* src */ 0, 0,
			     /* dst */ 0, 0,
			     gdk_pixbuf_get_width (pixbuf),
			     gdk_pixbuf_get_height (pixbuf),
			     GDK_RGB_DITHER_NORMAL,
			     /* dither */ 0, 0);

	    id[0] = GDK_WINDOW_XWINDOW (pixmap);

	    gtk_selection_data_set (selection_data,
				    GDK_SELECTION_TYPE_PIXMAP, 32,
				    (char * ) id, 4);
	
	    g_object_unref (pixbuf);

	    break;
	  }

	default:
	  break;
	}
    }
}

static __inline__ void
select_positions		(TeletextView *		view,
				 gint			x,
				 gint			y,
				 gint *			pcols,
				 gint *			prows,
				 gint *			scol,
				 gint *			srow,
				 gint *			ccol,
				 gint *			crow)
{
  gint width, height;	/* window */
  gint columns, rows;	/* page */

  gdk_window_get_geometry (GTK_WIDGET (view)->window,
			   NULL, NULL,
			   &width, &height,
			   NULL);

  *pcols = columns = view->pg->columns;
  *prows = rows = view->pg->rows;

  *scol = SATURATE ((view->select.start_x * columns) / width, 0, columns - 1);
  *srow = SATURATE ((view->select.start_y * rows) / height, 0, rows - 1);

  *ccol = SATURATE ((x * columns) / width, 0, columns - 1);
  *crow = SATURATE ((y * rows) / height, 0, rows - 1);
}

static __inline__ gboolean
is_hidden_row			(TeletextView *		view,
				 gint			row)
{
  vbi3_char *cp;
  unsigned int column;

  if (row <= 0 || row >= 25)
    return FALSE;

  cp = view->pg->text + row * view->pg->columns;
  for (column = 0; column < view->pg->columns; ++column)
    {
      if (cp->size >= VBI3_OVER_BOTTOM)
	return TRUE;
      ++cp;
    }

  return FALSE;
}

/* Calculates a rectangle, scaling from text coordinates
   to window coordinates. */
static void
scale_rect			(GdkRectangle *		rect,
				 gint			x1,
				 gint			y1,
				 gint			x2,
				 gint			y2,
				 gint			width,
				 gint			height,
				 gint			rows,
				 gint			columns)
{
  gint ch = columns >> 1;
  gint rh = rows >> 1;

  rect->x = (x1 * width + ch) / columns;
  rect->y = (y1 * height + rh) / rows;
  rect->width = ((x2 + 1) * width + ch) / columns - rect->x;
  rect->height = ((y2 + 1) * height + rh) / rows - rect->y;
}

/* Transforms the selected region from the first rectangle to the
   second one, given in text coordinates. */
static void
select_transform		(TeletextView *		view,
				 gint			sx1,
				 gint			sy1,
				 gint			sx2,
				 gint			sy2,
				 gboolean		stable,
				 gint			dx1,
				 gint			dy1,
				 gint			dx2,
				 gint			dy2,
				 gboolean		dtable,
				 GdkRegion *		exposed)
{
  gint width, height; /* window */
  gint columns, rows; /* page */
  GdkRectangle rect;
  GdkRegion *src_region;
  GdkRegion *dst_region;

  gdk_window_get_geometry (GTK_WIDGET (view)->window,
			   NULL, NULL,
			   &width, &height,
			   NULL);

  columns = view->pg->columns;
  rows = view->pg->rows;

  gdk_gc_set_clip_origin (view->select.xor_gc, 0, 0);

  {
    gboolean h1, h2, h3, h4;

    if (sy1 > sy2)
      {
	SWAP (sx1, sx2);
	SWAP (sy1, sy2);
      }

    h1 = is_hidden_row (view, sy1);
    h2 = is_hidden_row (view, sy1 + 1);
    h3 = is_hidden_row (view, sy2);
    h4 = is_hidden_row (view, sy2 + 1);

    if (stable || sy1 == sy2 || ((sy2 - sy1) == 1 && h3))
      {
	if (sx1 > sx2)
	  SWAP (sx1, sx2);

	scale_rect (&rect,
		    sx1, sy1 - h1,
		    sx2, sy2 + h4,
		    width, height,
		    rows, columns);
	src_region = gdk_region_rectangle (&rect);
      }
    else
      {
	scale_rect (&rect,
		    sx1, sy1 - h1,
		    columns - 1, sy1 + h2,
		    width, height,
		    rows, columns);
	src_region = gdk_region_rectangle (&rect);

	scale_rect (&rect,
		    0, sy2 - h3,
		    sx2, sy2 + h4,
		    width, height,
		    rows, columns);
	gdk_region_union_with_rect (src_region, &rect);

	sy1 += h2 + 1;
	sy2 -= h3 + 1;

	if (sy2 >= sy1)
	  {
	    scale_rect (&rect,
			0, sy1,
			columns - 1, sy2,
			width, height,
			rows, columns);
	    gdk_region_union_with_rect (src_region, &rect);
	  }
      }
  }

  {
    gboolean h1, h2, h3, h4;

    if (dy1 > dy2)
      {
	SWAP(dx1, dx2);
	SWAP(dy1, dy2);
      }

    h1 = is_hidden_row (view, dy1);
    h2 = is_hidden_row (view, dy1 + 1);
    h3 = is_hidden_row (view, dy2);
    h4 = is_hidden_row (view, dy2 + 1);

    if (dtable || dy1 == dy2 || ((dy2 - dy1) == 1 && h3))
      {
	if (dx1 > dx2)
	  SWAP(dx1, dx2);

	view->select.column1 = dx1;
	view->select.row1 = dy1 -= h1;
	view->select.column2 = dx2;
	view->select.row2 = dy2 += h4;

	scale_rect (&rect,
		    dx1, dy1,
		    dx2, dy2,
		    width, height,
		    rows, columns);
	dst_region = gdk_region_rectangle (&rect);
      }
    else
      {
	scale_rect (&rect,
		    dx1, dy1 - h1,
		    columns - 1, dy1 + h2,
		    width, height,
		    rows, columns);
	dst_region = gdk_region_rectangle (&rect);

	scale_rect (&rect,
		    0, dy2 - h3,
		    dx2, dy2 + h4,
		    width, height,
		    rows, columns);
	gdk_region_union_with_rect (dst_region, &rect);

	view->select.column1 = dx1;
	view->select.row1 = dy1 + h2;
	view->select.column2 = dx2;
	view->select.row2 = dy2 - h3;

	dy1 += h2 + 1;
	dy2 -= h3 + 1;

	if (dy2 >= dy1)
	  {
	    scale_rect (&rect,
			0, dy1,
			columns - 1, dy2,
			width, height,
			rows, columns);
	    gdk_region_union_with_rect (dst_region, &rect);
	  }
      }
  }

  if (exposed)
    gdk_region_subtract (src_region, exposed);

  gdk_region_xor (src_region, dst_region);

  gdk_region_destroy (dst_region);

  gdk_gc_set_clip_region (view->select.xor_gc, src_region);

  gdk_region_destroy (src_region);

  gdk_draw_rectangle (GTK_WIDGET (view)->window,
		      view->select.xor_gc,
		      TRUE,
		      0, 0,
		      width - 1, height - 1);

  gdk_gc_set_clip_rectangle (view->select.xor_gc, NULL);
}

static void
select_stop			(TeletextView *		view)
{
  if (view->appbar)
    gnome_appbar_pop (view->appbar);

  if (view->select.last_x != -1)
    {
      gint columns, rows; /* page */
      gint scol, srow; /* start */
      gint ccol, crow; /* current */
      gint tcol1, trow1;
      gint tcol2, trow2;

      select_positions (view,
			view->select.last_x,
			view->select.last_y,
			&columns, &rows,
			&scol, &srow,
			&ccol, &crow);

      /* Save because select_transform() will reset. */
      tcol1 = view->select.column1;
      trow1 = view->select.row1;
      tcol2 = view->select.column2;
      trow2 = view->select.row2;

      select_transform (view,
			scol, srow,	/* src1 */
			ccol, crow,	/* src2 */
			view->select.table_mode,
			columns, rows,	/* dst1 */
			columns, rows,	/* dst2 */
			view->select.table_mode,
			NULL);

      /* Copy selected text. */

      vbi3_page_unref (view->select.pg);
      view->select.pg = vbi3_page_dup (view->pg);
      g_assert (NULL != view->select.pg);

      view->select.column1 = tcol1;
      view->select.row1    = trow1;
      view->select.column2 = tcol2;
      view->select.row2    = trow2;

      view->select.reveal  = view->reveal;

      if (!view->select.in_clipboard)
	if (gtk_selection_owner_set (GTK_WIDGET (view),
				     GA_CLIPBOARD,
				     GDK_CURRENT_TIME))
	  view->select.in_clipboard = TRUE;

      if (!view->select.in_selection)
	if (gtk_selection_owner_set (GTK_WIDGET (view),
				     GDK_SELECTION_PRIMARY,
				     GDK_CURRENT_TIME))
	  view->select.in_selection = TRUE;

      if (view->appbar)
	gnome_appbar_set_status (view->appbar,
				 _("Selection copied to clipboard"));
    }

  update_cursor_shape (view);

  view->selecting = FALSE;
}

static void
select_update			(TeletextView *		view,
				 gint			x,
				 gint			y,
				 guint			state)
{
  gint columns, rows; /* page */
  gint scol, srow; /* start */
  gint ocol, orow; /* last */
  gint ccol, crow; /* current */
  gboolean table;

  select_positions (view,
		    x, y,
		    &columns, &rows,
		    &scol, &srow,
		    &ccol, &crow);

  table = !!(state & GDK_SHIFT_MASK);

  if (view->select.last_x == -1)
    {
      /* First motion. */
      select_transform (view,
			columns, rows,	/* src1 */
			columns, rows,	/* src2 */
			view->select.table_mode,
			scol, srow,	/* dst1 */
			ccol, crow,	/* dst2 */
			table,
			NULL);
    }
  else
    {
      gint width, height;

      gdk_window_get_geometry (GTK_WIDGET (view)->window,
			       NULL, NULL,
			       &width, &height,
			       NULL);

      ocol = (view->select.last_x * columns) / width;
      ocol = SATURATE (ocol, 0, columns - 1);
      orow = (view->select.last_y * rows) / height;
      orow = SATURATE (orow, 0, rows - 1);

      select_transform (view,
			scol, srow,	/* src1 */	
			ocol, orow,	/* src2 */
			view->select.table_mode,
			scol, srow,	/* dst1 */
			ccol, crow,	/* dst2 */
			table,
			NULL);
    }

  view->select.last_x = MAX (0, x);
  view->select.last_y = y;

  view->select.table_mode = table;
}

static void
select_start			(TeletextView *		view,
				 gint			x,
				 gint			y,
				 guint			state)
{
  if (view->selecting || !view->pg)
    return;

  if (view->pg->pgno < 0x100)
    {
      if (view->appbar)
  	gnome_appbar_set_status (view->appbar, _("No page loaded"));
      return;
    }

  if (view->cursor_over_link)
    {
      view->cursor_over_link = FALSE;
      if (view->appbar)
	gnome_appbar_pop (view->appbar);
    }

  if (view->appbar)
    gnome_appbar_push (view->appbar,
		       _("Selecting - press Shift key for table mode"));

  gdk_window_set_cursor (GTK_WIDGET (view)->window, cursor_select);

  view->select.start_x = x;
  view->select.start_y = y;

  view->select.last_x = -1; /* not yet, wait for move event */

  view->select.table_mode = !!(state & GDK_SHIFT_MASK);
  view->select.rtl_mode = FALSE; /* XXX to do */

  view->selecting = TRUE;
}

/*
	Export
*/

static void
export_action			(GtkAction *		action _unused_,
				 TeletextView *		view)
{
  extern gchar *zvbi_get_current_network_name (void); /* XXX */
  GtkWidget *dialog;
  gchar *name;

  g_assert (view->pg && view->pg->pgno >= 0x100);

  if ((name = zvbi_get_current_network_name ()))
    {
      guint i;

      for (i = 0; i < strlen (name); ++i)
	if (!g_ascii_isalnum (name[i]))
	  name[i] = '_';

      dialog = export_dialog_new (view->pg, name, view->reveal);
    }
  else
    {
      dialog = export_dialog_new (view->pg, "Zapzilla", view->reveal);
    }

  if (dialog)
    {
      set_transient_for (GTK_WINDOW (dialog), view);
      gtk_widget_show_all (dialog);
    }
}

static PyObject *
py_ttx_export			(PyObject *		self _unused_,
				 PyObject *		args _unused_)
{
  TeletextView *view;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  export_action (NULL, view);

  py_return_true;
}

/*
	Search
*/

static void
search_action			(GtkAction *		action _unused_,
				 TeletextView *		view)
{
  GtkWidget *widget;

  if (view->search_dialog)
    {
      gtk_window_present (GTK_WINDOW (view->search_dialog));
    }
  else if ((widget = search_dialog_new (view)))
    {
      view->search_dialog = widget;

      g_signal_connect (G_OBJECT (widget), "destroy",
			G_CALLBACK (gtk_widget_destroyed),
			&view->search_dialog);

      set_transient_for (GTK_WINDOW (widget), view);

      gtk_widget_show_all (widget);
    }
}

static PyObject *
py_ttx_search			(PyObject *		self _unused_,
				 PyObject *		args _unused_)
{
  TeletextView *view;

  if (!(view = teletext_view_from_widget (python_command_widget ())))
    py_return_true;

  search_action (NULL, view);

  py_return_true;
}

/*
	Popup Menu
*/

static guint
hotlist_menu_insert		(GtkMenuShell *		menu,
				 const vbi3_network *	nk,
				 gboolean		separator,
				 gint			position)
{
  gboolean have_subtitle_index = FALSE;
  gboolean have_now_and_next   = FALSE;
  gboolean have_current_progr  = FALSE;
  gboolean have_progr_index    = FALSE;
  gboolean have_progr_schedule = FALSE;
  gboolean have_progr_warning  = FALSE;
  vbi3_pgno pgno;
  guint count;

  if (!td)
    return 0;

  count = 0;

  for (pgno = 0x100; pgno <= 0x899; pgno = vbi3_add_bcd (pgno, 0x001))
    {
      vbi3_ttx_page_stat ps;
      gboolean new_window;
      GtkWidget *menu_item;
      gchar *buffer;

      ps.page_type = VBI3_UNKNOWN_PAGE;

      /* Error ignored. */
      vbi3_teletext_decoder_get_ttx_page_stat (td, &ps, nk, pgno);

      new_window = TRUE;

      switch (ps.page_type)
	{

#undef ONCE
#define ONCE(b) if (b) continue; else b = TRUE;

	case VBI3_SUBTITLE_INDEX:
	  ONCE (have_subtitle_index);
	  menu_item = z_gtk_pixmap_menu_item_new (_("Subtitle index"),
						  GTK_STOCK_INDEX);
	  break;

	case VBI3_NOW_AND_NEXT:
	  ONCE (have_now_and_next);
	  new_window = FALSE;
	  menu_item = z_gtk_pixmap_menu_item_new (_("Now and Next"),
						  GTK_STOCK_JUSTIFY_FILL);
	  break;

	case VBI3_CURRENT_PROGR:
	  ONCE (have_current_progr);
	  menu_item = z_gtk_pixmap_menu_item_new (_("Current program"),
						  GTK_STOCK_JUSTIFY_FILL);
	  break;

	case VBI3_PROGR_INDEX:
	  ONCE (have_progr_index);
	  menu_item = z_gtk_pixmap_menu_item_new (_("Program Index"),
						  GTK_STOCK_INDEX);
	  break;

	case VBI3_PROGR_SCHEDULE:
	  ONCE (have_progr_schedule);
	  menu_item = z_gtk_pixmap_menu_item_new (_("Program Schedule"),
						  "gnome-stock-timer");
	  break;

	case VBI3_PROGR_WARNING:
	  ONCE (have_progr_warning);
	  new_window = FALSE;
	  /* TRANSLATORS: Schedule changes and the like. */
	  menu_item = z_gtk_pixmap_menu_item_new (_("Program Warning"),
						  "gnome-stock-mail");
	  break;

	default:
	  continue;
	}

      if (separator)
	{
	  GtkWidget *menu_item;

	  menu_item = gtk_separator_menu_item_new ();
	  gtk_widget_show (menu_item);
	  gtk_menu_shell_insert (menu, menu_item, position);
	  if (position >= 0)
	    ++position;

	  separator = FALSE;
	}

      gtk_widget_show (menu_item);

      {
	gchar buffer[32];

	g_snprintf (buffer, sizeof (buffer), "%x", pgno);
	z_tooltip_set (menu_item, buffer);
      }

      if (new_window)
	buffer = g_strdup_printf ("zapping.ttx_open_new(%x, -1)", pgno);
      else
	buffer = g_strdup_printf ("zapping.ttx_open(%x, -1)", pgno);

      g_signal_connect (G_OBJECT (menu_item), "activate",
			G_CALLBACK (on_python_command1), buffer);
      g_signal_connect_swapped (G_OBJECT (menu_item), "destroy",
				G_CALLBACK (g_free), buffer);

      gtk_menu_shell_insert (menu, menu_item, position);
      if (position >= 0)
	++position;

      ++count;
    }
      
  return count;
}

guint
ttxview_hotlist_menu_insert	(GtkMenuShell *		menu,
				 gboolean		separator,
				 gint			position)
{
  return hotlist_menu_insert (menu, NULL, separator, position);
}

static GnomeUIInfo
popup_open_page_uiinfo [] = {
  {
    GNOME_APP_UI_ITEM, N_("_Open"), NULL,
    G_CALLBACK (on_python_command1), NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("Open in _New Window"), NULL, 
    G_CALLBACK (on_python_command1), NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo
popup_open_url_uiinfo [] = {
  {
    GNOME_APP_UI_ITEM, N_("_Open Link"), NULL,
    G_CALLBACK (on_python_command1), NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo
popup_page_uiinfo [] = {
  {
    GNOME_APP_UI_ITEM, N_("_New Window"), NULL, 
    G_CALLBACK (on_python_command1), "zapping.ttx_open_new()", NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_NEW,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Save as..."), NULL,
    G_CALLBACK (on_python_command1), "zapping.ttx_export()", NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_SAVE_AS,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("S_earch..."), NULL, 
    G_CALLBACK (on_python_command1), "zapping.ttx_search()", NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_FIND,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("S_ubtitles"), NULL,
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_INDEX,
    0, (GdkModifierType) 0, NULL
  },
  {
    GNOME_APP_UI_ITEM, N_("_Bookmarks"), NULL,
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GTK_STOCK_INDEX,
    0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_END
};

GtkWidget *
teletext_view_popup_menu_new	(TeletextView *		view,
				 const vbi3_link *	lk,
				 gboolean		large)
{
  GtkWidget *menu;
  GtkWidget *widget;
  gint pos;

  menu = gtk_menu_new ();
  g_object_set_data (G_OBJECT (menu), "TeletextView", view);

  pos = 0;

  if (lk)
    {
      switch (lk->type)
	{
	  gint subpage;

	case VBI3_LINK_PAGE:
	case VBI3_LINK_SUBPAGE:
	  subpage = decimal_subno (lk->subno);

	  popup_open_page_uiinfo[0].user_data =
	    g_strdup_printf ("zapping.ttx_open(%x, %d)",
			     lk->pgno, subpage);
	  g_signal_connect_swapped (G_OBJECT (menu), "destroy",
				    G_CALLBACK (g_free),
				    popup_open_page_uiinfo[0].user_data);

	  popup_open_page_uiinfo[1].user_data =
	    g_strdup_printf ("zapping.ttx_open_new(%x, %d)",
			     lk->pgno, subpage);
	  g_signal_connect_swapped (G_OBJECT (menu), "destroy",
				    G_CALLBACK (g_free),
				    popup_open_page_uiinfo[1].user_data);

	  gnome_app_fill_menu (GTK_MENU_SHELL (menu),
			       popup_open_page_uiinfo,
			       /* accel */ NULL,
			       /* mnemo */ TRUE,
			       pos);
	  return menu;

	case VBI3_LINK_HTTP:
	case VBI3_LINK_FTP:
	case VBI3_LINK_EMAIL:
	  popup_open_url_uiinfo[0].user_data = g_strdup (lk->url);
	  g_signal_connect_swapped (G_OBJECT (menu), "destroy",
				    G_CALLBACK (g_free),
				    popup_open_url_uiinfo[0].user_data);

	  gnome_app_fill_menu (GTK_MENU_SHELL (menu), popup_open_url_uiinfo,
			       /* accel */ NULL, /* mnemo */ TRUE, pos);
	  return menu;

	default:
	  break;
	}
    }

  gnome_app_fill_menu (GTK_MENU_SHELL (menu), popup_page_uiinfo,
		       NULL, TRUE, pos);

  if (!vbi3_export_info_enum (0))
    gtk_widget_set_sensitive (popup_page_uiinfo[1].widget, FALSE);

  if (large)
    {
      GtkWidget *subtitles_menu;

      widget = popup_page_uiinfo[3].widget; /* subtitles */

      if ((subtitles_menu = subtitle_menu_new ()))
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (widget), subtitles_menu);
      else
	gtk_widget_set_sensitive (widget, FALSE);

      widget = popup_page_uiinfo[4].widget; /* bookmarks */

      gtk_menu_item_set_submenu (GTK_MENU_ITEM (widget),
				 bookmarks_menu_new (view));

      ttxview_hotlist_menu_insert (GTK_MENU_SHELL (menu),
				   /* separator */ TRUE, APPEND);
    }
  else
    {
      widget = popup_page_uiinfo[3].widget; /* subtitles */
      gtk_widget_set_sensitive (widget, FALSE);
      gtk_widget_hide (widget);

      widget = popup_page_uiinfo[4].widget; /* bookmarks */
      gtk_widget_set_sensitive (widget, FALSE);
      gtk_widget_hide (widget);
    }

  return menu;
}

static gboolean
blink_timeout			(gpointer		user_data)
{
  TeletextView *view = user_data;

  apply_patches (view, /* draw */ TRUE);

  return TRUE;
}

static void
size_allocate			(GtkWidget *		widget,
				 GtkAllocation *	allocation)
{
  TeletextView *view = TELETEXT_VIEW (widget);

  GTK_WIDGET_CLASS (parent_class)->size_allocate (widget, allocation);

  resize_scaled_page_image (view, allocation->width, allocation->height);
}

static gboolean
expose_event			(GtkWidget *		widget,
				 GdkEventExpose *	event)
{
  TeletextView *view = TELETEXT_VIEW (widget);
  GdkRegion *region;

  draw_scaled_page_image (view,
			  widget->window,
			  widget->style->white_gc,
			  /* src */ event->area.x, event->area.y,
			  /* dst */ event->area.x, event->area.y,
			  event->area.width,
			  event->area.height);

  if (view->selecting
      && view->select.last_x != -1)
    {
      gint columns, rows; /* page */
      gint scol, srow; /* start */
      gint ccol, crow; /* current */

      select_positions (view,
			view->select.last_x,
			view->select.last_y,
			&columns, &rows,
			&scol, &srow,
			&ccol, &crow);

      region = gdk_region_rectangle (&event->area);

      select_transform (view,
			scol, srow,	/* src1 */
			ccol, crow,	/* src2 */
			view->select.table_mode,
			scol, srow,	/* dst2 */
			ccol, crow,	/* dst2 */
			view->select.table_mode,
			region);

      gdk_region_destroy (region);
    }

  return TRUE;
}

static gboolean
motion_notify_event		(GtkWidget *		widget,
				 GdkEventMotion *	event)
{
  TeletextView *view = TELETEXT_VIEW (widget);

  if (view->selecting)
    select_update (view, (int) event->x, (int) event->y, event->state);
  else
    update_cursor_shape (view);

  return FALSE;
}

static gboolean
button_release_event		(GtkWidget *		widget,
				 GdkEventButton *	event _unused_)
{
  TeletextView *view = TELETEXT_VIEW (widget);

  if (view->selecting)
    select_stop (view);

  return FALSE; /* pass on */
}

static gboolean
button_press_event		(GtkWidget *		widget,
				 GdkEventButton *	event)
{
  TeletextView *view = TELETEXT_VIEW (widget);
  vbi3_link lk;
  gboolean success;

  switch (event->button)
    {
    case 1: /* left button */
      if (event->state & (GDK_SHIFT_MASK |
			  GDK_CONTROL_MASK |
			  GDK_MOD1_MASK))
	{
	  select_start (view,
			(gint) event->x,
			(gint) event->y,
			event->state);
	}
      else
	{
	  success = teletext_view_vbi3_link_from_pointer_position
	    (view, &lk, (int) event->x,	(int) event->y);

	  if (success)
	    {
	      switch (lk.type)
		{
		case VBI3_LINK_PAGE:
		case VBI3_LINK_SUBPAGE:
		  teletext_view_load_page (view,lk.network, lk.pgno, lk.subno);
		  break;

		case VBI3_LINK_HTTP:
		case VBI3_LINK_FTP:
		case VBI3_LINK_EMAIL:
		  z_url_show (NULL, lk.url);
		  break;
	      
		default:
		  select_start (view,
				(gint) event->x,
				(gint) event->y,
				event->state);
		  break;
		}

	      vbi3_link_destroy (&lk);
	    }
	  else
	    {
	      select_start (view,
			    (gint) event->x,
			    (gint) event->y,
			    event->state);
	    }
	}

      return TRUE; /* handled */

    case 2: /* middle button, open link in new window */
      success = teletext_view_vbi3_link_from_pointer_position
	(view, &lk, (int) event->x, (int) event->y);

      if (success)
	{
	  switch (lk.type)
	    {
	    case VBI3_LINK_PAGE:
	    case VBI3_LINK_SUBPAGE:
	      python_command_printf (widget,
				     "zapping.ttx_open_new(%x,%d)",
				     lk.pgno, decimal_subno (lk.subno));
	      vbi3_link_destroy (&lk);
	      return TRUE; /* handled */

	    case VBI3_LINK_HTTP:
	    case VBI3_LINK_FTP:
	    case VBI3_LINK_EMAIL:
	      z_url_show (NULL, lk.url);
	      vbi3_link_destroy (&lk);
	      return TRUE; /* handled */

	    default:
	      vbi3_link_destroy (&lk);
	      break;
	    }
	}

      break;

    default:
      break;
    }

  return FALSE; /* pass on */
}

/* Drawing area cannot focus, must be called by parent. */
static gboolean
teletext_view_on_key_press	(GtkWidget *		widget _unused_,
				 GdkEventKey *		event,
				 TeletextView *		view)
{
  guint digit;

  /* Loading a page takes time, possibly longer than the key
     repeat period, and repeated keys will stack up. This kludge
     effectively defers all loads until the key is released,
     and discards all but the last load request. */
  if (abs ((int) view->last_key_press_event_time - (int) event->time) < 100
      || event->length > 1)
    view->deferred_load = TRUE;

  view->last_key_press_event_time = event->time;

  digit = event->keyval - GDK_0;

  switch (event->keyval)
    {
    case GDK_a ... GDK_f:
    case GDK_A ... GDK_F:
      if (hex_pages)
	{
	  digit = (event->keyval & 7) + 9;
	  goto page_number;
	}

      break;

    case GDK_KP_0 ... GDK_KP_9:
      digit = event->keyval - GDK_KP_0;
      goto page_number;

    case GDK_0 ... GDK_9:
    page_number:
      if (event->state & (GDK_CONTROL_MASK |
			  GDK_SHIFT_MASK |
			  GDK_MOD1_MASK))
	{
	  if (digit >= 1 && digit <= 8)
	    {
	      teletext_view_load_page (view,
				       NULL,
				       (vbi3_pgno) digit * 0x100,
				       VBI3_ANY_SUBNO);

	      return TRUE; /* handled, don't pass on */
	    }

	  break;
	}

      if (view->entered_pgno >= 0x100)
	view->entered_pgno = 0;

      view->entered_pgno = (view->entered_pgno << 4) + digit;

      if (view->entered_pgno >= 0x900)
        view->entered_pgno ^= 0x800;

      if (view->entered_pgno >= 0x100)
	{
	  teletext_view_load_page (view,
				   NULL,
				   view->entered_pgno,
				   VBI3_ANY_SUBNO);
	}
      else
	{
	  /* view->freezed = TRUE; */

	  if (view->toolbar)
	    teletext_toolbar_set_url (view->toolbar, view->entered_pgno, 0);
	}

      return TRUE; /* handled */

    case GDK_S:
      /* Menu is "Save As", plain "Save" might be confusing. Now Save As
	 has accelerator Ctrl-Shift-S, omitting the Shift might be
	 confusing. But we accept Ctrl-S (of plain Save) here. */
      if (event->state & GDK_CONTROL_MASK)
	{
	  python_command_printf (GTK_WIDGET (view), "zapping.ttx_export()");
	  return TRUE; /* handled */
	}

      break;

    default:
      break;
    }

  return FALSE; /* pass on */
}

static gboolean
my_key_press			(TeletextView *		view,
				 GdkEventKey *		event)
{
  return teletext_view_on_key_press (GTK_WIDGET (view), event, view);
}

static void
client_redraw			(TeletextView *		view,
				 unsigned int		width,
				 unsigned int		height)
{
  resize_scaled_page_image (view, width, height);
  draw_scaled_page_image (view,
	       GTK_WIDGET (view)->window,
	       GTK_WIDGET (view)->style->white_gc,
	       /* src */ 0, 0,
	       /* dst */ 0, 0,
	       width, height);
}

static void
instance_finalize		(GObject *		object)
{
  TeletextView *view = TELETEXT_VIEW (object);
  GdkWindow *window;

  teletext_views = g_list_remove (teletext_views, view);

  if (view->search_dialog)
    gtk_widget_destroy (view->search_dialog);

  if (view->blink_timeout_id > 0)
    g_source_remove (view->blink_timeout_id);

  if (view->deferred.timeout_id > 0)
    g_source_remove (view->deferred.timeout_id);

  g_object_unref (view->unscaled_on);
  g_object_unref (view->unscaled_off);

  if (view->scaled_on)
    g_object_unref (view->scaled_on);

  delete_patches (view);


  g_object_unref (view->select.xor_gc);

  window = GTK_WIDGET (view)->window;

  if (view->select.in_clipboard)
    {
      if (gdk_selection_owner_get (GA_CLIPBOARD) == window)
	gtk_selection_owner_set (NULL, GA_CLIPBOARD, GDK_CURRENT_TIME);
    }

  if (view->select.in_selection)
    {
      if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) == window)
	gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY,
				 GDK_CURRENT_TIME);
    }

  vbi3_page_unref (view->select.pg);
  vbi3_page_unref (view->pg);

  vbi3_network_destroy (&view->req.network);
  vbi3_network_destroy (&view->deferred.network);

  parent_class->finalize (object);
}

static GtkActionEntry
actions [] = {
  { "GoSubmenu", NULL, N_("_Go"), NULL, NULL, NULL },
  { "HistoryBack", GTK_STOCK_GO_BACK, NULL, NULL,
    N_("Previous page in history"), G_CALLBACK (history_back_action) },
  { "HistoryForward", GTK_STOCK_GO_FORWARD, NULL, NULL,
    N_("Next page in history"), G_CALLBACK (history_forward_action) },
  { "Home", GTK_STOCK_HOME, N_("Index"), NULL,
    N_("Go to the index page"), G_CALLBACK (home_action) },
  { "Search", GTK_STOCK_FIND, NULL, NULL,
    N_("Search page memory"), G_CALLBACK (search_action) },
  { "Export", GTK_STOCK_SAVE_AS, NULL, NULL,
    N_("Save this page to a file"), G_CALLBACK (export_action) },
};

/* We cannot initialize this until the widget (and its
   parent GtkWindow) has a window. */
static void
realize				(GtkWidget *		widget)
{
  TeletextView *view = TELETEXT_VIEW (widget);

  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  /* No background, prevents flicker. */
  gdk_window_set_back_pixmap (widget->window, NULL, FALSE);

  view->select.xor_gc = gdk_gc_new (widget->window);
  gdk_gc_set_function (view->select.xor_gc, GDK_INVERT);
}

static int
cur_pgno			(TeletextView *		view)
{
  if (view->pg)
    return view->pg->pgno;
  else
    return 0;
}	

static void
instance_init			(GTypeInstance *	instance,
				 gpointer		g_class _unused_)
{
  TeletextView *view = (TeletextView *) instance;
  GtkAction *action;
  GtkWidget *widget;
  gint uw, uh;

  view->action_group = gtk_action_group_new ("TeletextViewActions");
#ifdef ENABLE_NLS
  gtk_action_group_set_translation_domain (view->action_group,
					   GETTEXT_PACKAGE);
#endif					   
  gtk_action_group_add_actions (view->action_group,
				actions, G_N_ELEMENTS (actions), view);

  action = gtk_action_group_get_action (view->action_group, "Export");
  z_action_set_sensitive (action, NULL != vbi3_export_info_enum (0));

  vbi3_network_init (&view->req.network);

  view->charset = -1; /* automatic */

  history_update_gui (view);

  widget = GTK_WIDGET (view);

  gtk_widget_add_events (widget,
			 GDK_EXPOSURE_MASK |		/* redraw */
			 GDK_POINTER_MOTION_MASK |	/* cursor shape */
			 GDK_BUTTON_PRESS_MASK |	/* links, selection */
			 GDK_BUTTON_RELEASE_MASK |	/* selection */
			 GDK_KEY_PRESS_MASK |		/* accelerators */
			 GDK_STRUCTURE_MASK |		/* resize */
			 0);

  /* Selection */

  gtk_selection_add_targets (widget, GDK_SELECTION_PRIMARY,
			     clipboard_targets,
			     N_ELEMENTS (clipboard_targets));

  gtk_selection_add_targets (widget, GA_CLIPBOARD,
			     clipboard_targets,
			     N_ELEMENTS (clipboard_targets));

  uw = 41 * CW;
  uh = 25 * CH;

  view->unscaled_on = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, uw, uh);
  view->unscaled_off = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, uw, uh);

  g_assert (view->unscaled_on != NULL);
  g_assert (view->unscaled_off != NULL);

  create_page_images_from_pg (view);

  view->deferred_load = FALSE;
  view->deferred.timeout_id = NO_SOURCE_ID;

  view->blink_timeout_id =
    g_timeout_add (BLINK_CYCLE / 4, (GSourceFunc) blink_timeout, view);

  teletext_view_load_page (view,
			   NULL,
			   default_home_pgno (),
			   VBI3_ANY_SUBNO);

  teletext_views = g_list_append (teletext_views, view);

  view->client_redraw = client_redraw;
  view->key_press = my_key_press;
  view->cur_pgno = cur_pgno;
}

GtkWidget *
teletext_view_new		(void)
{
  return GTK_WIDGET (g_object_new (TYPE_TELETEXT_VIEW, NULL));
}

static void
teletext_level_notify		(GConfClient *		client _unused_,
				 guint			cnxn_id _unused_,
				 GConfEntry *		entry,
				 gpointer		user_data _unused_)
{
  if (entry->value)
    {
      const gchar *s;
      gint enum_value;

      s = gconf_value_get_string (entry->value);
      if (s && gconf_string_to_enum (teletext_level_enum, s, &enum_value))
	{
	  teletext_level = (vbi3_wst_level) enum_value;
	  reformat_all_views ();
	}
    }
}

static void
default_charset_notify		(GConfClient *		client _unused_,
				 guint			cnxn_id _unused_,
				 GConfEntry *		entry,
				 gpointer		user_data _unused_)
{
  if (entry->value)
    {
      const gchar *s;
      gint enum_value;

      s = gconf_value_get_string (entry->value);
      if (s && gconf_string_to_enum (teletext_charset_enum, s, &enum_value))
	{
	  default_charset = enum_value;
	  reformat_all_views ();
	}
    }
}

static void
interp_type_notify		(GConfClient *		client _unused_,
				 guint			cnxn_id _unused_,
				 GConfEntry *		entry,
				 gpointer		user_data _unused_)
{
  if (entry->value)
    {
      const gchar *s;
      gint enum_value;

      s = gconf_value_get_string (entry->value);
      if (s && gconf_string_to_enum (teletext_interp_enum, s, &enum_value))
	{
	  interp_type = (GdkInterpType) enum_value;
	  redraw_all_views ();
	}
    }
}

static void
color_notify			(GConfClient *		client _unused_,
				 guint			cnxn_id _unused_,
				 GConfEntry *		entry _unused_,
				 gpointer		user_data _unused_)
{
  if (z_gconf_get_int (&brightness, GCONF_DIR "/view/brightness")
      || z_gconf_get_int (&contrast, GCONF_DIR "/view/contrast"))
    redraw_all_views ();
}

GConfEnumStringPair
navigation_enum [] = {
  { 0, "disabled" },
  { 1, "flof_top1" },
  { 2, "flof_top2" },
  { 0, NULL }
};

static void
navigation_notify		(GConfClient *		client _unused_,
				 guint			cnxn_id _unused_,
				 GConfEntry *		entry,
				 gpointer		user_data _unused_)
{
  if (entry->value)
    {
      const gchar *s;
      gint enum_value;

      s = gconf_value_get_string (entry->value);
      if (s && gconf_string_to_enum (navigation_enum, s, &enum_value))
	{
	  navigation = enum_value;
	  reformat_all_views ();
	}
    }
}

static void
class_init			(gpointer		g_class,
				 gpointer		class_data _unused_)
{
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  vbi3_bool success;

  object_class = G_OBJECT_CLASS (g_class);
  widget_class = GTK_WIDGET_CLASS (g_class);
  parent_class = g_type_class_peek_parent (g_class);

  object_class->finalize = instance_finalize;

  widget_class->realize			= realize;
  widget_class->size_allocate		= size_allocate;
  widget_class->expose_event		= expose_event;
  widget_class->button_press_event	= button_press_event;
  widget_class->button_release_event	= button_release_event;
  widget_class->motion_notify_event	= motion_notify_event;
  widget_class->selection_clear_event	= selection_clear_event;
  widget_class->selection_get		= selection_get;

  signals[REQUEST_CHANGED] =
    g_signal_new ("z-request-changed",
		  G_TYPE_FROM_CLASS (g_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		  G_STRUCT_OFFSET (TeletextViewClass, request_changed),
		  /* accumulator */ NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  /* return_type */ G_TYPE_NONE,
		  /* n_params */ 0);

  signals[CHARSET_CHANGED] =
    g_signal_new ("z-charset-changed",
		  G_TYPE_FROM_CLASS (g_class),
		  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
		  G_STRUCT_OFFSET (TeletextViewClass, charset_changed),
		  /* accumulator */ NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  /* return_type */ G_TYPE_NONE,
		  /* n_params */ 0);

  cursor_normal	= gdk_cursor_new (GDK_LEFT_PTR);
  cursor_link	= gdk_cursor_new (GDK_HAND2);
  cursor_select	= gdk_cursor_new (GDK_XTERM);

  GA_CLIPBOARD = gdk_atom_intern ("CLIPBOARD", FALSE);

  z_gconf_auto_update_bool (&rolling_header, GCONF_DIR "/view/rolling_header");
  z_gconf_auto_update_bool (&live_clock, GCONF_DIR "/view/live_clock");

  /* Error ignored */
  z_gconf_notify_add (GCONF_DIR "/level", teletext_level_notify, NULL);
  z_gconf_notify_add (GCONF_DIR "/default_charset",
		      default_charset_notify, NULL);
  z_gconf_notify_add (GCONF_DIR "/view/interp_type", interp_type_notify, NULL);
  z_gconf_notify_add (GCONF_DIR "/view/brightness", color_notify, NULL);
  z_gconf_notify_add (GCONF_DIR "/view/contrast", color_notify, NULL);
  z_gconf_notify_add (GCONF_DIR "/view/navigation", navigation_notify, NULL);

  cmd_register ("ttx_open", py_ttx_open, METH_VARARGS);
  cmd_register ("ttx_page_incr", py_ttx_page_incr, METH_VARARGS,
		("Increment Teletext page number"),
		"zapping.ttx_page_incr(+1)",
		("Decrement Teletext page number"),
		"zapping.ttx_page_incr(-1)");
  cmd_register ("ttx_subpage_incr", py_ttx_subpage_incr, METH_VARARGS,
		("Increment Teletext subpage number"),
		"zapping.ttx_subpage_incr(+1)",
		("Decrement Teletext subpage number"),
		"zapping.ttx_subpage_incr(-1)");
  cmd_register ("ttx_home", py_ttx_home, METH_VARARGS,
		("Go to Teletext index page"), "zapping.ttx_home()");
  cmd_register ("ttx_hold", py_ttx_hold, METH_VARARGS,
		("Hold Teletext subpage"), "zapping.ttx_hold()");
  cmd_register ("ttx_reveal", py_ttx_reveal, METH_VARARGS,
		("Reveal concealed text"), "zapping.ttx_reveal()");
  cmd_register ("ttx_history_prev", py_ttx_history_prev, METH_VARARGS,
		("Previous Teletext page in history"),
		"zapping.ttx_history_prev()");
  cmd_register ("ttx_history_next", py_ttx_history_next, METH_VARARGS,
		("Next Teletext page in history"),
		"zapping.ttx_history_next()");
  cmd_register ("ttx_search", py_ttx_search, METH_VARARGS,
		("Teletext search"), "zapping.ttx_search()");
  cmd_register ("ttx_export", py_ttx_export, METH_VARARGS,
		("Teletext export"), "zapping.ttx_export()");

  /* Send all events to our main event handler. */
  success = vbi3_teletext_decoder_add_event_handler
    (td,
     (VBI3_EVENT_NETWORK |
      VBI3_EVENT_TTX_PAGE),
     view_vbi3_event_handler, /* user_data */ NULL);

  g_assert (success);
}

GType
teletext_view_get_type		(void)
{
  static GType type = 0;
  
  if (!type)
    {
      GTypeInfo info;

      CLEAR (info);

      info.class_size = sizeof (TeletextViewClass);
      info.class_init = class_init;
      info.instance_size = sizeof (TeletextView);
      info.instance_init = instance_init;

      type = g_type_register_static (GTK_TYPE_DRAWING_AREA,
				     "TeletextView",
				     &info, (GTypeFlags) 0);
    }

  return type;
}
