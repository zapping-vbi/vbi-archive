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
# include <config.h>
#endif

#include <stddef.h>
#include <string.h>
#include <assert.h>

#define N_ELEMENTS(array) (sizeof (array) / sizeof (*(array)))

#ifdef __GNUC__

#if __GNUC__ < 3
#define __builtin_expect(exp, c) (exp)
#endif

#undef PARENT
#define PARENT(_ptr, _type, _member) ({					\
	__typeof__ (&((const _type *) 0)->_member) _p = (_ptr);		\
	(_p != 0) ? (_type *)(((char *) _p) - offsetof (_type,		\
	  _member)) : (_type *) 0;					\
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

#else /* !__GNUC__ */

#define __inline__
#define __builtin_expect(exp, c) (exp)

static char *
PARENT_HELPER (char *p, unsigned int offset)
{ return (p == 0) ? 0 : p - offset; }

#undef PARENT
#define PARENT(_ptr, _type, _member)					\
	((offsetof (_type, _member) == 0) ? (_type *)(_ptr)		\
	 : (_type *) PARENT_HELPER ((char *)(_ptr), offsetof (_type, _member)))

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

#endif /* !__GNUC__ */

#define SET(var) memset (&(var), ~0, sizeof (var))
#define CLEAR(var) memset (&(var), 0, sizeof (var))
#define MOVE(d, s) memmove (d, s, sizeof (d))

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixdata.h>

#include <tveng.h>

/* With precompiler aid we can print much more useful info */
/* This shows a non-modal, non-blocking message box */
#define ShowBox(MSG, MSGTYPE, args...) \
do {			\
  GtkWidget * dialog =						\
    gtk_message_dialog_new ((GtkWindow*)main_window,		\
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
    gtk_message_dialog_new ((GtkWindow*)main_window,		\
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
    gtk_message_dialog_new ((GtkWindow*)main_window,		\
			    GTK_DIALOG_DESTROY_WITH_PARENT |	\
			    GTK_DIALOG_MODAL,			\
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
    fprintf(stderr, "Line %d, routine %s\n", __LINE__, __PRETTY_FUNCTION__); \
} while (FALSE)

/* temporary */
#define XX D

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

/**
 * Restores the mode before the last switch_mode.
 * Returns whatever switch_mode returns.
 */
int
zmisc_restore_previous_mode(tveng_device_info *info);

/*
 * Stops the current tveng mode and shutdowns appropiate subsistems.
 */
void zmisc_stop (tveng_device_info *info);

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
 * Timeout is the time in ms to wait before the status is
 * automagically hidden again. Can be <= 0, in this case the bar
 * doesn't hide automatically.
 */
void
z_status_print(const gchar *message, gint timeout);

/**
 * Same thing but pango markup is allowed.
 * if the bar is hidden, it's shown.
 */
void
z_status_print_markup(const gchar *markup, gint timeout);

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
  return (enum tveng_frame_pixformat) -1; /* ouch */
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
 * Builds the given path if it doesn't exist and checks that it's a
 * valid dir.
 * On error returns FALSE and fills in error_description with a newly
 * allocated string if it isn't NULL.
 * Upon success, TRUE is returned and error_description is untouched.
 */
gboolean
z_build_path(const gchar *path, gchar **error_description);

void
z_on_electric_filename		(GtkWidget *		w,
				 gpointer		user_data);
void
z_electric_replace_extension	(GtkWidget *		w,
				 const gchar *		ext);
extern gchar *
z_replace_filename_extension	(const gchar *		filename,
				 const gchar *		new_ext);

/**
 * Makes the toolbar modify its children toolbars to always
 * mimic its oriention and style.
 */
void
propagate_toolbar_changes	(GtkWidget	*toolbar);

/* Switchs OSD on and sets the given OSD page as the subtitles source */
void
zmisc_overlay_subtitles		(gint page);

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

/* hscale_adj and unit optional */
GtkWidget *
z_spinslider_new		(GtkAdjustment *spin_adj,
				 GtkAdjustment *hscale_adj,
				 const gchar *unit,
				 gfloat reset_value,
				 gint digits);
GtkAdjustment *
z_spinslider_get_spin_adj	(GtkWidget *hbox);
GtkAdjustment *
z_spinslider_get_hscale_adj	(GtkWidget *hbox);
#define z_spinslider_get_adjustment(hbox) \
  z_spinslider_get_spin_adj(hbox)
gfloat
z_spinslider_get_value		(GtkWidget *hbox);
/* Change both adjustments or use this */
void
z_spinslider_set_value		(GtkWidget *hbox, gfloat value);
void
z_spinslider_set_reset_value	(GtkWidget *hbox,
				 gfloat value);
/* Change both adjustments or use this */
void
z_spinslider_adjustment_changed	(GtkWidget *hbox);

typedef tv_device_node *
z_device_entry_open_fn		(GtkWidget *		table,
				 void *			user_data,
				 tv_device_node *	list,
				 const gchar *		entered);
typedef void
z_device_entry_select_fn	(GtkWidget *		table,
				 void *			user_data,
				 tv_device_node *	n);

/* ATTN maintains & deletes list */
GtkWidget *
z_device_entry_new		(const gchar *		prompt,
				 tv_device_node *	list,
				 const gchar *		current_device,
				 z_device_entry_open_fn *open_fn,
				 z_device_entry_select_fn *select_fn,
				 void *			user_data);
tv_device_node *
z_device_entry_selected		(GtkWidget *		table);

/* Makes the given entry emit the response to the dialog when
   activated */
void
z_entry_emits_response		(GtkWidget	*entry,
				 GtkDialog	*dialog,
				 GtkResponseType response);

#define SIGNAL_HANDLER_BLOCK(object, func, statement)			\
do { gulong handler_id_;						\
  handler_id_ = g_signal_handler_find (G_OBJECT (object),		\
    G_SIGNAL_MATCH_FUNC, 0, 0, 0, (gpointer) func, 0);			\
  g_assert (handler_id_ != 0);						\
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

#endif /* __ZMISC_H__ */
