/*
 * Screenshot saving plugin for Zapping
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
 * Copyright (C) 2001 Michael H. Schimek
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

#include "src/plugin_common.h"
#include <pthread.h>

typedef struct screenshot_data screenshot_data;
typedef struct screenshot_backend screenshot_backend;
typedef struct backend_private backend_private;

struct screenshot_data
{
  gint			status;

  screenshot_backend *	backend;

  GtkWidget *		dialog;
  GtkWidget *		drawingarea;
  GtkWidget *		size_label;
  GtkWidget *		quality_slider;
  GtkEntry  *		entry;
  GdkPixbuf *		pixbuf;
  gchar *		auto_filename;

  gdouble		size_est;

  tveng_image_data	data;
  tv_image_format	format;	/* Format of the grabbed image */

  gpointer		deint_data;

  gchar *		io_buffer;
  gint			io_buffer_size;	/* Allocated */
  gint			io_buffer_used;

  gboolean		(* io_flush)(screenshot_data *, gint);
  FILE *		io_fp;

  gchar *		error;

  pthread_t		saving_thread;
  gboolean		thread_abort;	/* TRUE when the thread shall abort */

  GtkWidget *		status_window;	/* Progressbar */

  gint			lines;		/* Lines saved by thread (progress) */

  gchar *		command;	/* To be executed on completion */
  gchar *		filename;	/* The file we save */

  gchar			private[0];	/* Backend data */
};

struct screenshot_backend
{
  gchar *		keyword;	/* Canonical */
  gchar *		label;		/* gettext()ized */
  gchar *		extension;

  gint			sizeof_private;

  gboolean		quality;	/* Supports quality option? */
  gdouble		bpp_est;	/* Static output size estimation */
  					/*  if no preview supported */

  gboolean		(* init)(screenshot_data *data, gboolean write,
				 gint quality); /* 0...100 */
  void			(* save)(screenshot_data *data);

  /* For preview, optional */
  void			(* load)(screenshot_data *data,
				 gchar *pixels, gint rowstride);
};

extern gboolean screenshot_close_everything;

extern gchar *screenshot_deinterlace (screenshot_data *, gint);

/*
 *  Encoding backends
 */
extern screenshot_backend screenshot_backend_jpeg;
extern screenshot_backend screenshot_backend_ppm;

