/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000-2001 Iñaki García Etxebarria
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
  Misc stuff for zapping that didn't fit properly in any other place,
  but was commonly used.
*/

#ifndef __ZMISC_H__
#define __ZMISC_H__

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <tveng.h>
#include <frequencies.h>

/* in error_console.c, just adds the given message to the console */
void ec_add_message(const gchar *text, gboolean show,
		    GdkColor *color);

/* With precompiler aid we can print much more useful info */
/* This shows a non-modal, non-blocking message box */
#define ShowBox(MSG, MSGTYPE, args...) do { \
  gchar * tmp_str = g_strdup_printf(MSG,##args); \
  gchar *level; \
  gchar *buffer; \
  if (!strcasecmp(MSGTYPE, GNOME_MESSAGE_BOX_ERROR)) \
    level = _("Error: "); \
  else if (!strcasecmp(MSGTYPE, GNOME_MESSAGE_BOX_WARNING)) \
    level = _("Warning: "); \
  else \
    { \
      ShowBoxReal(__FILE__, __LINE__, G_GNUC_PRETTY_FUNCTION, \
		  tmp_str, MSGTYPE, FALSE, FALSE); \
      g_free(tmp_str); \
      break; \
    } \
  buffer = \
    g_strdup_printf("%s%s (%d) [%s]:\n%s", level, \
		    __FILE__, __LINE__, G_GNUC_PRETTY_FUNCTION, \
		    tmp_str); \
  ec_add_message(buffer, TRUE, NULL); \
  g_free(tmp_str); \
  g_free(buffer); \
} while (FALSE)

/* This one shows a modal, non-blocking message box */
#define ShowBoxModal(MSG, MSGTYPE, args...) \
do { \
  gchar * tmp_str = g_strdup_printf(MSG,##args); \
  ShowBoxReal(__FILE__, __LINE__, G_GNUC_PRETTY_FUNCTION, \
	      tmp_str, MSGTYPE, FALSE, TRUE); \
  g_free(tmp_str); \
} while (FALSE)


/* This one shows a modal, blocking message box */
#define RunBox(MSG, MSGTYPE, args...) \
do { \
  gchar * tmp_str = g_strdup_printf(MSG,##args); \
  ShowBoxReal(__FILE__, __LINE__, G_GNUC_PRETTY_FUNCTION, \
	      tmp_str, MSGTYPE, TRUE, TRUE); \
  g_free(tmp_str); \
} while (FALSE)

/* Some debug messages to track the startup */
extern int /* gboolean */ debug_msg;

#define D() \
do { \
  if (debug_msg) \
    fprintf(stderr, "Line %d, routine %s\n", __LINE__, __PRETTY_FUNCTION__); \
} while (FALSE)

#define printv(format, args...) \
do { \
  if (debug_msg) { \
    fprintf(stderr, format ,##args); \
  fflush(stderr); } \
} while (FALSE)

#define z_update_gui() \
do { \
  gint z_num_events_pending = gtk_events_pending(); \
  for (; z_num_events_pending >= 0; z_num_events_pending--) \
    gtk_main_iteration(); \
} while (FALSE)

/*
  Prints a message box showing an error, with the location of the code
  that called the function.
*/
GtkWidget * ShowBoxReal(const gchar * sourcefile,
			const gint line,
			const gchar * func,
			const gchar * message,
			const gchar * message_box_type,
			gboolean blocking, gboolean modal);

/**
 * Asks for a string, returning it, or NULL if it the operation was
 * cancelled
 * main_window: Parent of the Dialog to be created, or NULL
 * title: Title of the dialog
 * prompt: Prompt, or NULL if it shouldn't be shown
 * default_text: Default text to appear, or NULL
 * Returns: The returned string should be g_free'd
 */
gchar*
Prompt (GtkWidget *main_window, const gchar *title,
	const gchar *prompt, const gchar *default_text);

/**
 * Creates a GtkPixmapMenuEntry with the desired pixmap and the
 * desired label. The pixmap is a stock GNOME pixmap.
*/
GtkWidget * z_gtk_pixmap_menu_item_new(const gchar * label,
				       const gchar * icon);

/**
 * Sets the tooltip of the given widget.
 */
void set_tooltip	(GtkWidget	*widget,
			 const gchar	*new_tip);

/**
 * Widget: a GTK_PIXMAP_MENU_ITEM that you want to change.
 * Any of the attributes can be NULL, means don't change.
 */
void
z_change_menuitem			 (GtkWidget	*widget,
					  const gchar	*new_pixmap,
					  const gchar	*new_label,
					  const gchar	*new_tooltip);

/**
 * Restores the mode before the last switch_mode.
 * Returns whatever switch_mode returns.
 */
int
zmisc_restore_previous_mode(tveng_device_info *info);

/*
  does the mode switching. Since this requires more than just using
  tveng, a new routine is needed.
  Returns whatever tveng returns, but we print the message ourselves
  too, so no need to aknowledge it to the user.
  Side efects: Stops whatever mode was being used before.
*/
int
zmisc_switch_mode(enum tveng_capture_mode new_mode,
		  tveng_device_info * info);

/**
 * Like tveng_restart_everything, but updates overlay clips as necessary.
 */
int
z_restart_everything(enum tveng_capture_mode mode,
		     tveng_device_info * info);

/**
 * Prints the message in the status bar.
 * if the bar is hidden, it's shown.
 */
void
z_status_print(const gchar *message);

/**
 * Adds the given widget to the status bar, it replaces any widgets
 * present before.
 * If the appbar is hidden, it's shown.
 * Pass NULL to remove the current widget, if any.
 */
void
z_status_set_widget(GtkWidget *widget);

/*
  Given a bpp (bites per pixel) and the endianess, returns the proper
  TVeng RGB mode.
  returns -1 if the mode is unknown.
*/
static inline enum tveng_frame_pixformat
zmisc_resolve_pixformat(int bpp, GdkByteOrder byte_order)
{
  switch (bpp)
    {
    case 15:
      return TVENG_PIX_RGB555;
      break;
    case 16:
      return TVENG_PIX_RGB565;
      break;
    case 24:
      if (byte_order == GDK_MSB_FIRST)
	return TVENG_PIX_RGB24;
      else
	return TVENG_PIX_BGR24;
      break;
    case 32:
      if (byte_order == GDK_MSB_FIRST)
	return TVENG_PIX_RGB32;
      else
	return TVENG_PIX_BGR32;
      break;
    default:
      g_warning("Unrecognized image bpp: %d", bpp);
      break;
    }
  return -1;
}

/**
 * Changes the pixmap of a pixbutton (buttons in the toolbar, for example)
 */
void set_stock_pixmap	(GtkWidget	*button,
			 const gchar	*new_pix);

/**
 * Just like gdk_pixbuf_copy_area but does clipping.
 */
void
z_pixbuf_copy_area		(GdkPixbuf	*src_pixbuf,
				 gint		src_x,
				 gint		src_y,
				 gint		width,
				 gint		height,
				 GdkPixbuf	*dest_pixbuf,
				 gint		dest_x,
				 gint		dest_y);

/**
 * Just like gdk_pixbuf_render_to_drawable but does clipping
 */
void
z_pixbuf_render_to_drawable	(GdkPixbuf	*pixbuf,
				 GdkWindow	*window,
				 GdkGC		*gc,
				 gint		x,
				 gint		y,
				 gint		width,
				 gint		height);

/**
 * Returns the index of the given widget in the menu, or -1
 */
gint
z_menu_get_index		(GtkWidget	*menu,
				 GtkWidget	*item);

/**
 * Returns the index of the selected entry in a GtkOptionMenu.
 */
gint
z_option_menu_get_active	(GtkWidget	*option_menu);

/**
 * Sets (visually too) a given item as active.
 */
void
z_option_menu_set_active	(GtkWidget	*option_menu,
				 int index);

/**
 * Error checking scale_simple.
 */
static inline GdkPixbuf*
z_pixbuf_scale_simple		(GdkPixbuf	*source,
				 gint		destw,
				 gint		desth,
				 GdkInterpType	interp)
{
  if (desth < 5 || destw < 5)
    return NULL;

  return gdk_pixbuf_scale_simple(source, destw, desth, interp);
}

/**
 * Like gtk_widget_add_accelerator but takes care of creating the
 * accel group.
 */
void
z_widget_add_accelerator	(GtkWidget	*widget,
				 const gchar	*accel_signal,
				 guint		accel_key,
				 guint		accel_mods);

/**
 * Builds the given path if it doesn't exist and checks that it's a
 * valid dir.
 * On error returns FALSE and fills in error_description with a newly
 * allocated string if it isn't NULL.
 * Upon success, TRUE is returned and error_description is untouched.
 */
gboolean
z_build_path(const gchar *path, gchar **error_description);

/* See ttxview.c
 */
void
z_on_electric_filename (GtkWidget *w, gpointer user_data);

/**
 * Makes the toolbar modify its children toolbars to always
 * mimic its oriention and style.
 */
void
propagate_toolbar_changes	(GtkWidget	*toolbar);

/* Switchs OSD on and sets the given OSD page as the subtitles source */
void
zmisc_overlay_subtitles		(gint page);

/* Sets the given X cursor to the window */
void
z_set_cursor			(GdkWindow	*window,
				 guint		cid);

/* Creates a GtkPixmap from the given filename, NULL if not found */
GtkWidget *
z_pixmap_new_from_file		(const gchar	*file);

/* Pointer to the main window */
GtkWindow *
z_main_window			(void);

#endif /* ZMISC.H */
