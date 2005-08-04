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
#  include "config.h"
#endif

#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "globals.h"
#include "x11stuff.h"

/* Progress mark, GNU coding style (www.gnu.org) error message,
   compatible with emacs M-x next-error. */
#if 1
#define PR fprintf (stderr, "%s:%u:", __FILE__, __LINE__)
#define PRF(templ, ...)							\
	fprintf (stderr, "%s:%u: " templ, __FILE__, __LINE__		\
 		 , ## __VA_ARGS__)
#else
#define PR 0
#define PRF(templ, ...) 0
#endif

#define N_ELEMENTS(array) (sizeof (array) / sizeof (*(array)))

#ifdef __GNUC__

#undef likely
#undef unlikely
#if __GNUC__ < 3
#  define likely(expr) (expr)
#  define unlikely(expr) (expr)
#else
#  define likely(expr) __builtin_expect(expr, 1)
#  define unlikely(expr) __builtin_expect(expr, 0)
#endif

#undef PARENT
#define PARENT(_ptr, _type, _member) ({					\
	__typeof__ (&((_type *) 0)->_member) _p = (_ptr);		\
	(_p != 0) ? (_type *)(((char *) _p) - offsetof (_type,		\
	  _member)) : (_type *) 0;					\
})

#undef CONST_PARENT
#define CONST_PARENT(_ptr, _type, _member) ({				\
	__typeof__ (&((const _type *) 0)->_member) _p = (_ptr);		\
	(_p != 0) ? (const _type *)(((const char *) _p) - offsetof	\
	 (const _type, _member)) : (const _type *) 0;			\
})

#undef ABS
#define ABS(n) ({							\
	register int _n = n, _t = _n;					\
	_t >>= sizeof (_t) * 8 - 1;					\
	_n ^= _t;							\
	_n -= _t;							\
})

#undef MIN
#define MIN(x, y) ({							\
	__typeof__ (x) _x = x;						\
	__typeof__ (y) _y = y;						\
	(void)(&_x == &_y); /* alert when type mismatch */		\
	(_x < _y) ? _x : _y;						\
})

#undef MAX
#define MAX(x, y) ({							\
	__typeof__ (x) _x = x;						\
	__typeof__ (y) _y = y;						\
	(void)(&_x == &_y); /* alert when type mismatch */		\
	(_x > _y) ? _x : _y;						\
})

#define SWAP(x, y)							\
do {									\
	__typeof__ (x) _x = x;						\
	x = y;								\
	y = _x;								\
} while (0)

#undef SATURATE
#ifdef __i686__ /* conditional move */
#define SATURATE(n, min, max) ({					\
	__typeof__ (n) _n = n;						\
	__typeof__ (n) _min = min;					\
	__typeof__ (n) _max = max;					\
	if (_n < _min)							\
		_n = _min;						\
	if (_n > _max)							\
		_n = _max;						\
	_n;								\
})
#else
#define SATURATE(n, min, max) ({					\
	__typeof__ (n) _n = n;						\
	__typeof__ (n) _min = min;					\
	__typeof__ (n) _max = max;					\
	if (_n < _min)							\
		_n = _min;						\
	else if (_n > _max)						\
		_n = _max;						\
	_n;								\
})
#endif

#define _unused_ __attribute__ ((unused))

/* TRUE if exactly one bit is set in signed or unsigned n. */
#define SINGLE_BIT(n) ({						\
	__typeof__ (n) _n = n;						\
	(_n > 0 && 0 == (_n & (_n - 1)));				\
})

#else /* !__GNUC__ */

#define __inline__
#define likely(expr) (expr)
#define unlikely(expr) (expr)
#define _unused_

static char *
PARENT_HELPER (char *p, unsigned int offset)
{ return (p == 0) ? 0 : p - offset; }
static const char *
CONST_PARENT_HELPER (const char *p, unsigned int offset)
{ return (p == 0) ? 0 : p - offset; }

#undef PARENT
#define PARENT(_ptr, _type, _member)					\
	((offsetof (_type, _member) == 0) ? (_type *)(_ptr)		\
	 : (_type *) PARENT_HELPER ((char *)(_ptr), offsetof (_type, _member)))
#undef CONST_PARENT
#define CONST_PARENT(_ptr, _type, _member)				\
	((offsetof (const _type, _member) == 0) ? (const _type *)(_ptr)	\
	 : (const _type *) CONST_PARENT_HELPER ((const char *)(_ptr),	\
	  offsetof (const _type, _member)))

#undef ABS
#define ABS(n) (((n) < 0) ? -(n) : (n))

#undef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#undef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#define SWAP(x, y)							\
do {									\
	long _x = x;							\
	x = y;								\
	y = _x;								\
} while (0)

#undef SATURATE
#define SATURATE(n, min, max) MIN (MAX (n, min), max)

#define SINGLE_BIT(n) ((n) > 0 && 0 == ((n) & ((n) - 1)))

#endif /* !__GNUC__ */

#define SET(var) memset (&(var), ~0, sizeof (var))
#define CLEAR(var) memset (&(var), 0, sizeof (var))
#define MOVE(d, s) memmove (d, s, sizeof (d))

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixdata.h>

#include "tveng.h"

/* With precompiler aid we can print much more useful info */
/* This shows a non-modal, non-blocking message box */
#define ShowBox(MSG, MSGTYPE, args...) \
do {			\
  GtkWidget * dialog =						\
    gtk_message_dialog_new (zapping ? GTK_WINDOW (zapping) : 0,	\
			    GTK_DIALOG_DESTROY_WITH_PARENT,	\
			    MSGTYPE,				\
			    GTK_BUTTONS_CLOSE,			\
			    MSG,##args);			\
								\
  /* Destroy the dialog when the user responds to it */		\
  /* (e.g. clicks a button) */					\
  g_signal_connect_swapped (G_OBJECT (dialog), "response",	\
			    G_CALLBACK (gtk_widget_destroy),	\
			    GTK_OBJECT (dialog));		\
								\
  gtk_widget_show (dialog);					\
} while (FALSE)

/* This one shows a modal, non-blocking message box */
#define ShowBoxModal(MSG, MSGTYPE, args...) \
do {		\
  GtkWidget * dialog =						\
    gtk_message_dialog_new (zapping ? GTK_WINDOW (zapping) : 0,	\
			    GTK_DIALOG_DESTROY_WITH_PARENT |	\
			    GTK_DIALOG_MODAL,			\
			    MSGTYPE,				\
			    GTK_BUTTONS_CLOSE,			\
			    MSG,##args);			\
								\
  g_signal_connect_swapped (G_OBJECT (dialog), "response",	\
			    G_CALLBACK (gtk_widget_destroy),	\
			    GTK_OBJECT (dialog));		\
								\
  gtk_widget_show (dialog);					\
} while (FALSE)

/* This one shows a modal, blocking message box */
#define RunBox(MSG, MSGTYPE, args...) \
do {			\
  GtkWidget * dialog =						\
    gtk_message_dialog_new (zapping ? GTK_WINDOW (zapping) : 0,	\
			    (GtkDialogFlags)			\
			    (GTK_DIALOG_DESTROY_WITH_PARENT |	\
			     GTK_DIALOG_MODAL),			\
			    MSGTYPE,				\
			    GTK_BUTTONS_CLOSE,			\
			    MSG,##args);			\
								\
  gtk_dialog_run (GTK_DIALOG (dialog));				\
  gtk_widget_destroy (dialog);					\
} while (FALSE)

extern int debug_msg;

#define D() \
do { \
  if (debug_msg) \
    fprintf(stderr, "%s:%s:%u\n", __FILE__, __PRETTY_FUNCTION__, __LINE__); \
} while (FALSE)

#undef printv
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

extern GQuark
z_misc_error_quark		(void);

#define Z_MISC_ERROR z_misc_error_quark ()

typedef enum {
  Z_MISC_ERROR_MKDIR = 1,

} ZMiscError;

extern void
z_show_non_modal_message_dialog	(GtkWindow *		parent,
				 GtkMessageType		type,
				 const gchar *		primary,
				 const gchar *		secondary,
				 ...);

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
 * Creates a GtkImageMenuItem with the desired pixmap and the
 * desired mnemonic. The icon must be a valid Gtk stock icon name.
*/
GtkWidget * z_gtk_pixmap_menu_item_new(const gchar * label,
				       const gchar * icon);

GtkTooltips *
z_tooltips_add			(GtkTooltips *		tips);
extern void
z_tooltips_active		(gboolean		enable);
#define z_tooltips_enable() z_tooltips_active (TRUE)
#define z_tooltips_disable() z_tooltips_active (FALSE)
extern void
z_tooltip_set			(GtkWidget *		widget,
				 const gchar *		tip_text);
GtkWidget *
z_tooltip_set_wrap		(GtkWidget *		widget,
				 const gchar *		tip_text);
extern void
z_set_sensitive_with_tooltip	(GtkWidget *		widget,
				 gboolean		sensitive,
				 const gchar *		on_tip,
				 const gchar *		off_tip);

/**
 * Widget: a GTK_PIXMAP_MENU_ITEM that you want to change.
 * Any of the attributes can be NULL, means don't change.
 */
void
z_change_menuitem			 (GtkWidget	*widget,
					  const gchar	*new_pixmap,
					  const gchar	*new_label,
					  const gchar	*new_tooltip);

extern void
z_set_window_bg			(GtkWidget *		widget,
				 GdkColor *		color);

/**
 * Restores the mode before the last switch_mode.
 * Returns whatever switch_mode returns.
 */
int
zmisc_restore_previous_mode(tveng_device_info *info);

/*
 * Stops the current tveng mode and shutdowns appropiate subsistems.
 */
gboolean zmisc_stop (tveng_device_info *info);

/*
  does the mode switching. Since this requires more than just using
  tveng, a new routine is needed.
  Returns whatever tveng returns, but we print the message ourselves
  too, so no need to aknowledge it to the user.
  Side efects: Stops whatever mode was being used before.
*/
int
zmisc_switch_mode(display_mode new_dmode,
		  capture_mode new_cmode,
		  tveng_device_info * info,
		  gboolean warnings);

/**
 * Like tveng_restart_everything, but updates overlay clips as necessary.
 */
int
z_restart_everything(display_mode new_dmode,
		     capture_mode new_cmode,
		     tveng_device_info * info);

/**
 * Prints the message in the status bar, optional with pango markup.
 * If the status bar is hidden, it's shown. Timeout is the time in ms to
 * wait before the status bar is cleaned or hidden again. Can be
 * 0 to keep the message.
 */
void
z_status_print			(const gchar *		message,
				 gboolean		markup,
				 guint			timeout,
				 gboolean		hide);

/**
 * Adds the given widget to the status bar, it replaces any widgets
 * present before.
 * If the appbar is hidden, it's shown.
 * Pass NULL to remove the current widget, if any.
 */
void
z_status_set_widget(GtkWidget *widget);

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

extern GtkWidget *
z_menu_shell_nth_item		(GtkMenuShell *		menu_shell,
				 guint			n);

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
				 guint nth);

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

extern gboolean
z_build_path			(const gchar *		path,
				 GError **		error);
extern gboolean
z_build_path_with_alert		(GtkWindow *		parent,
				 const gchar *		path);

void
z_electric_set_basename		(GtkWidget *		w,
				 const gchar *		basenm);
void
z_on_electric_filename		(GtkWidget *		w,
				 gpointer		user_data);
void
z_electric_replace_extension	(GtkWidget *		w,
				 const gchar *		ext);
extern gchar *
z_replace_filename_extension	(const gchar *		filename,
				 const gchar *		new_ext);



/* Same as z_pixmap_new_from_file(), but prepends PACKAGE_PIXMAP_DIR
   to name and gtk_shows the pixmap on success */
GtkWidget *
z_load_pixmap			(const gchar *	name);

/* Pointer to the main window */
GtkWindow *
z_main_window			(void);

/**
 * Scans @dir sequentially looking for files named prefix%dsuffix
 * (clip17.mpeg, for example) and returns the name of the first
 * file that made stat() fail.
 * NULL on error.
 */
gchar*
find_unused_name (const gchar * dir, const gchar * prefix,
		  const gchar * suffix);

typedef tv_device_node *
z_device_entry_open_fn		(GtkWidget *		table,
				 tv_device_node *	list,
				 const gchar *		entered,
				 gpointer		user_data);
typedef void
z_device_entry_select_fn	(GtkWidget *		table,
				 tv_device_node *	n,
				 gpointer		user_data);

/* ATTN maintains & deletes list */
extern GtkWidget *
z_device_entry_new		(const gchar *		prompt,
				 tv_device_node *	list,
				 const gchar *		current_device,
				 z_device_entry_open_fn *open_fn,
				 z_device_entry_select_fn *select_fn,
				 gpointer		user_data);
extern tv_device_node *
z_device_entry_selected		(GtkWidget *		table);
extern void
z_device_entry_grab_focus	(GtkWidget *		table);

/* Makes the given entry emit the response to the dialog when
   activated */
void
z_entry_emits_response		(GtkWidget	*entry,
				 GtkDialog	*dialog,
				 GtkResponseType response);

#define SIGNAL_BLOCK(object, signal, statement)				\
do { guint id_;								\
  id_ = g_signal_lookup (signal, G_OBJECT_TYPE (button));		\
  g_assert (0 != id_);							\
  g_signal_handlers_block_matched (object, G_SIGNAL_MATCH_ID,		\
				   id_, 0, 0, 0, 0);			\
  statement;								\
  g_signal_handlers_unblock_matched (object, G_SIGNAL_MATCH_ID,		\
				     id_, 0, 0, 0, 0);			\
} while (0)

#define SIGNAL_HANDLER_BLOCK(object, func, statement)			\
do { gulong handler_id_;						\
  handler_id_ = g_signal_handler_find (G_OBJECT (object),		\
    G_SIGNAL_MATCH_FUNC, 0, 0, 0, (gpointer) func, 0);			\
  g_assert (0 != handler_id_);						\
  g_signal_handler_block (G_OBJECT (object), handler_id_);		\
  statement;								\
  g_signal_handler_unblock (G_OBJECT (object), handler_id_);		\
} while (0)

extern GtkWidget *
z_gtk_image_new_from_pixdata	(const GdkPixdata *	pixdata);
extern gboolean
z_icon_factory_add_file		(const gchar *		stock_id,
				 const gchar *		filename);
extern gboolean
z_icon_factory_add_pixdata	(const gchar *		stock_id,
				 const GdkPixdata *	pixdata);

extern size_t
z_strlcpy			(char *			dst1,
				 const char *		src,
				 size_t			size);

extern const gchar *
z_gdk_event_name		(GdkEvent *		event);

extern void
z_toolbar_set_style_recursive	(GtkToolbar *		toolbar,
				 GtkToolbarStyle	style);

void
z_label_set_text_printf		(GtkLabel *		label,
				 const gchar *		format,
				 ...);

extern gboolean
z_tree_selection_iter_first	(GtkTreeSelection *	selection,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter);
extern gboolean
z_tree_selection_iter_last	(GtkTreeSelection *	selection,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter);
extern void
z_tree_view_remove_selected	(GtkTreeView *		tree_view,
				 GtkTreeSelection *	selection,
				 GtkTreeModel *		model);

extern gboolean
z_overwrite_file_dialog		(GtkWindow *		parent,
				 const gchar *		primary,
				 const gchar *		filename);
extern void
z_help_display			(GtkWindow *		parent,
				 const gchar *		filename,
				 const gchar *		link_id);
extern void
z_url_show			(GtkWindow *		parent,
				 const gchar *		url);

enum old_tveng_capture_mode
{
  /* We keep this for config compatibility. */
  OLD_TVENG_NO_CAPTURE,		/* Capture isn't active */
  OLD_TVENG_CAPTURE_READ,	/* Capture is through windowed read() call */
  OLD_TVENG_CAPTURE_PREVIEW,   	/* Capture is through fullscreen overlays */
  OLD_TVENG_CAPTURE_WINDOW,	/* Capture is through windowed overlays */
  OLD_TVENG_TELETEXT,		/* Teletext in window */

  OLD_TVENG_FULLSCREEN_READ,
  OLD_TVENG_FULLSCREEN_TELETEXT,
  OLD_TVENG_BACKGROUND_READ,
  OLD_TVENG_BACKGROUND_PREVIEW,
  OLD_TVENG_BACKGROUND_TELETEXT,
};

void
from_old_tveng_capture_mode	(display_mode *		dmode,
				 capture_mode *		cmode,
				 enum old_tveng_capture_mode mode);
enum old_tveng_capture_mode
to_old_tveng_capture_mode	(display_mode 		dmode,
				 capture_mode 		cmode);
extern gboolean
z_set_overlay_buffer		(tveng_device_info *	info,
				 const tv_screen *	screen,
				 const GdkWindow *	window);
extern void
z_action_set_sensitive		(GtkAction *		action,
				 gboolean		sensitive);
extern void
z_action_set_visible		(GtkAction *		action,
				 gboolean		visible);
extern void
z_show_empty_submenu		(GtkActionGroup *	action_group,
				 const gchar *		action_name);
extern void
z_menu_shell_chop_off		(GtkMenuShell *		menu_shell,
				 GtkMenuItem *		menu_item);
extern gchar *
z_strappend			(gchar *		string1,
				 const gchar *		string2,
				 ...);
extern void
z_object_set_const_data		(GObject *		object,
				 const gchar *		key,
				 const void *		data);
extern void
z_object_set_int_data		(GObject *		object,
				 const gchar *		key,
				 gint			data);
extern gint
z_object_get_int_data		(GObject *		object,
				 const gchar *		key);
extern gulong
z_signal_connect_const		(gpointer		instance,
				 const gchar *		detailed_signal,
				 GCallback		c_handler,
				 const void *		data);
extern gulong
z_signal_connect_python		(gpointer		instance,
				 const gchar *		detailed_signal,
				 const gchar *		command);

/* Common constants for item position in Gtk insert functions. */
#define PREPEND 0
#define APPEND -1
/* Common constant for string length in Gtk and Glib functions. */
#define NUL_TERMINATED -1
/* GSource ID */
#define NO_SOURCE_ID ((guint) -1)

#endif /* __ZMISC_H__ */
