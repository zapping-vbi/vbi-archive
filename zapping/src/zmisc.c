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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "site_def.h"

#include <gdk/gdkx.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <ctype.h>

#define ZCONF_DOMAIN "/zapping/options/main/"
#include "zmisc.h"
#include "zconf.h"
#include "tveng.h"
#include "interface.h"
#include "callbacks.h"
#include "x11stuff.h"
#include "overlay.h"
#include "capture.h"
#include "fullscreen.h"
#include "v4linterface.h"
#include "ttxview.h"
#include "zvbi.h"
#include "osd.h"
#include "remote.h"
#include "keyboard.h"
#include "globals.h"
#include "audio.h"
#include "mixer.h"

extern tveng_device_info * main_info;
extern volatile gboolean flag_exit_program;
extern gint disable_preview; /* TRUE if preview won't work */
extern gboolean xv_present;

gchar*
Prompt (GtkWidget *main_window, const gchar *title,
	const gchar *prompt,  const gchar *default_text)
{
  GtkWidget * dialog;
  GtkBox *vbox;
  GtkWidget *label, *entry;
  gchar *buffer = NULL;

  dialog = gtk_dialog_new_with_buttons
    (title, GTK_WINDOW (main_window),
     GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
     GTK_STOCK_OK, GTK_RESPONSE_OK,
     NULL);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  vbox = GTK_BOX (GTK_DIALOG (dialog) -> vbox);

  if (prompt)
    {
      label = gtk_label_new (prompt);
      gtk_box_pack_start_defaults (vbox, label);
      gtk_widget_show(label);
    }
  entry = gtk_entry_new();
  gtk_box_pack_start_defaults(GTK_BOX(vbox), entry);
  gtk_widget_show(entry);
  gtk_widget_grab_focus(entry);
  if (default_text)
    {
      gtk_entry_set_text (GTK_ENTRY(entry), default_text);
      gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
    }

  z_entry_emits_response (entry, GTK_DIALOG (dialog),
			  GTK_RESPONSE_OK);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    buffer = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));

  gtk_widget_destroy(dialog);
  
  return buffer;
}

GtkWidget * z_gtk_pixmap_menu_item_new(const gchar * mnemonic,
				       const gchar * icon)
{
  GtkWidget * imi;
  GtkWidget * image;

  imi = gtk_image_menu_item_new_with_mnemonic (mnemonic);

  if (icon)
    {
      image = gtk_image_new_from_stock (icon, GTK_ICON_SIZE_MENU);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (imi),
				     image);
      /* not sure whether this is necessary, but won't harm */
      gtk_widget_show (image);
    }

  return (imi);
}

/*
 *  Zapping Global Tooltips
 */

static GList *			tooltips_list = NULL;
static GtkTooltips *		tooltips_default = NULL;
static gboolean			tooltips_enabled = TRUE;

static void
tooltips_destroy_notify		(gpointer	data,
				 GObject	*where_the_object_was)
{
  g_list_remove (tooltips_list, data);
}

GtkTooltips *
z_tooltips_add			(GtkTooltips *		tips)
{
  if (!tips)
    tips = gtk_tooltips_new (); /* XXX destroy at exit */

  tooltips_list = g_list_append (tooltips_list, (gpointer) tips);

  g_object_weak_ref (G_OBJECT (tips),
		     tooltips_destroy_notify,
		     (gpointer) tips);

  if (tooltips_enabled)
    gtk_tooltips_enable (tips);
  else
    gtk_tooltips_disable (tips);

  return tips;
}

void
z_tooltips_active		(gboolean		enable)
{
  GList *list;

  tooltips_enabled = enable;

  for (list = tooltips_list; list; list = list->next)
    {
      if (enable)
	gtk_tooltips_enable (GTK_TOOLTIPS (list->data));
      else
	gtk_tooltips_disable (GTK_TOOLTIPS (list->data));
    }
}

void
z_tooltip_set			(GtkWidget *		widget,
				 const gchar *		tip_text)
{
  if (!tooltips_default)
    tooltips_default = z_tooltips_add (NULL);

#ifndef ZMISC_TOOLTIP_WARNING
#define ZMISC_TOOLTIP_WARNING 0
#endif

  if (ZMISC_TOOLTIP_WARNING && GTK_WIDGET_NO_WINDOW(widget))
    fprintf(stderr, "Warning: tooltip <%s> for "
            "widget without window\n", tip_text);

  gtk_tooltips_set_tip (tooltips_default, widget, tip_text, "private tip");
}

GtkWidget *
z_tooltip_set_wrap		(GtkWidget *		widget,
				 const gchar *		tip_text)
{
  if (!tooltips_default)
    tooltips_default = z_tooltips_add (NULL);

  if (GTK_WIDGET_NO_WINDOW(widget))
    {
      GtkWidget *event_box = gtk_event_box_new ();

      gtk_widget_show (event_box);
      gtk_container_add (GTK_CONTAINER (event_box), widget);
      widget = event_box;
    }

  gtk_tooltips_set_tip (tooltips_default, widget, tip_text, "private tip");

  return widget;
}

void
z_set_sensitive_with_tooltip	(GtkWidget *		widget,
				 gboolean		sensitive,
				 const gchar *		on_tip,
				 const gchar *		off_tip)
{
  const gchar *new_tip;

  if (!tooltips_default)
    tooltips_default = z_tooltips_add (NULL);

  gtk_widget_set_sensitive (widget, sensitive);

  new_tip = sensitive ? on_tip : off_tip; /* can be NULL */

  gtk_tooltips_set_tip (tooltips_default, widget, new_tip, NULL);
}

/**************************************************************************/
int
zmisc_restore_previous_mode(tveng_device_info * info)
{
  return zmisc_switch_mode(zcg_int(NULL, "previous_mode"), info);
}

void
zmisc_stop (tveng_device_info *info)
{
  /* Stop current capture mode */
  switch (info->current_mode)
    {
    case TVENG_CAPTURE_PREVIEW:
      tveng_stop_everything(info);
      fullscreen_stop(info);
      break;
    case TVENG_CAPTURE_READ:
      capture_stop();
      video_uninit ();
      tveng_stop_everything(info);
      break;
    case TVENG_CAPTURE_WINDOW:
      tveng_stop_everything(info);
      overlay_stop(info);
      break;
    default:
      tveng_stop_everything(info);
      break;
    }
}

/*
  does the mode switching. Since this requires more than just using
  tveng, a new routine is needed.
  Returns whatever tveng returns, but we print the message ourselves
  too, so no need to aknowledge it to the user.
*/
int
zmisc_switch_mode(enum tveng_capture_mode new_mode,
		  tveng_device_info * info)
{
  GtkWidget * tv_screen;
  int return_value = 0;
  gint x, y, w, h;
  enum tveng_frame_pixformat format;
  gint muted;
  gchar * old_name = NULL;
  enum tveng_capture_mode mode;
  extern int disable_overlay;

  g_assert(info != NULL);
  g_assert(main_window != NULL);
  tv_screen = lookup_widget(main_window, "tv_screen");
  g_assert(tv_screen != NULL);

  if ((info->current_mode == new_mode) &&
      (new_mode != TVENG_NO_CAPTURE))
    return 0; /* success */

  /* save this input name for later retrieval */
  if (info->num_inputs > 0)
    old_name = g_strdup(info->inputs[info->cur_input].name);

  gdk_window_get_geometry(tv_screen->window, NULL, NULL, &w, &h, NULL);
  gdk_window_get_origin(tv_screen->window, &x, &y);

  // FIXME
  if (!audio_get_mute (&muted))
    muted = -1;

  mode = info->current_mode;

  zmisc_stop (info);

#ifdef HAVE_LIBZVBI
  if (!flag_exit_program)
    {
      GtkWidget *button = lookup_widget (main_window, "videotext3");
      GtkWidget *pixmap;

      if (new_mode != TVENG_NO_CAPTURE)
	{
	  gtk_widget_hide (lookup_widget (main_window, "appbar2"));

	  ttxview_detach (main_window);

	  if ((pixmap = z_load_pixmap ("teletext.png")))
	    {
	      gtk_container_remove (GTK_CONTAINER (button),
	                            gtk_bin_get_child (GTK_BIN (button)));
	      gtk_container_add (GTK_CONTAINER (button), pixmap);
	    }
	  else
	    {
	      set_stock_pixmap (button, GTK_STOCK_JUSTIFY_FILL);
	    }

	  z_tooltip_set (button, _("Use Zapping as a Teletext navigator"));
	}
      else
	{
	  set_stock_pixmap (button, GTK_STOCK_REDO);
	  z_tooltip_set (button, _("Return to windowed mode and use the current "
			   "page as subtitles"));
	}
    }
#endif /* HAVE_LIBZVBI */

  if (new_mode != TVENG_CAPTURE_PREVIEW &&
      new_mode != TVENG_NO_CAPTURE)
    osd_set_window(tv_screen);
  else if (new_mode == TVENG_NO_CAPTURE)
    {
      osd_clear();
      osd_unset_window();
    }

  switch (new_mode)
    {
    case TVENG_CAPTURE_READ:
      if (info->current_controller == TVENG_CONTROLLER_XV ||
	  info->current_controller == TVENG_CONTROLLER_NONE)
	{
	  if (info->current_controller != TVENG_CONTROLLER_NONE)
	    tveng_close_device(info);
	  if (tveng_attach_device(zcg_char(NULL, "video_device"),
				  TVENG_ATTACH_READ, info) == -1)
	    {
	      /* Try restoring as XVideo as a last resort */
	      if (tveng_attach_device(zcg_char(NULL, "video_device"),
				      TVENG_ATTACH_XV, info) == -1)
		{
		  RunBox("%s couldn't be opened\n:%s,\naborting",
			 GTK_MESSAGE_ERROR,
			 zcg_char(NULL, "video_device"), info->error);
		  exit(1);
		}
	      else
		ShowBox("Capture mode not available:\n%s",
			GTK_MESSAGE_ERROR, info->error);
	    }
	}
      tveng_set_capture_size(w, h, info);
      return_value = capture_start(info);
      video_init (tv_screen, tv_screen->style->black_gc);
      video_suggest_format ();
      break;
    case TVENG_CAPTURE_WINDOW:
      if (disable_preview || disable_overlay) {
	ShowBox("preview has been disabled", GTK_MESSAGE_WARNING);
	g_free(old_name);
	return -1;
      }

      if (info->current_controller != TVENG_CONTROLLER_XV &&
	  xv_present)
	{
	  tveng_close_device(info);
	  if (tveng_attach_device(zcg_char(NULL, "video_device"),
				  TVENG_ATTACH_XV, info)==-1)
	    {
	      RunBox("%s couldn't be opened\n:%s, aborting",
		     GTK_MESSAGE_ERROR,
		     zcg_char(NULL, "video_device"), info->error);
	      exit(1);
	    }
	}

      if (!tveng_detect_preview(info))
	{
	  ShowBox(_("Preview will not work: %s"),
		  GTK_MESSAGE_ERROR, info->error);
	  return -1;
	}

      format = zmisc_resolve_pixformat(tveng_get_display_depth(info),
				       x11_get_byte_order());

      if ((format != -1) &&
	  (info->current_controller != TVENG_CONTROLLER_XV))
	{
	  info->format.pixformat = format;
	  if ((tveng_set_capture_format(info) == -1) ||
	      (info->format.pixformat != format))
	    g_warning("Preview format invalid: %s (%d, %d)", info->error,
		      info->format.pixformat, format);
	  printv("prev: setting %d, got %d\n", format,
		 info->format.pixformat);
	}

      info->window.x = x;
      info->window.y = y;
      info->window.width = w;
      info->window.height = h;
      info->window.clipcount = 0;
      info->window.win = GDK_WINDOW_XWINDOW(tv_screen->window);
      info->window.gc = GDK_GC_XGC(tv_screen->style->white_gc);
      tveng_set_preview_window(info);
      return_value = tveng_start_window(info);
      if (return_value != -1)
	{
	  startup_overlay(tv_screen, main_window, info);
	  overlay_sync(TRUE);
	}
      else
	g_warning(info->error);
      break;
    case TVENG_CAPTURE_PREVIEW:
      if (disable_preview || disable_overlay) {
	ShowBox("preview has been disabled", GTK_MESSAGE_WARNING);
	g_free(old_name);
	return -1;
      }

      if (info->current_controller != TVENG_CONTROLLER_XV &&
	  xv_present)
	{
	  tveng_close_device(info);
	  if (tveng_attach_device(zcg_char(NULL, "video_device"),
				  TVENG_ATTACH_XV, info)==-1)
	    {
	      RunBox("%s couldn't be opened\n:%s, aborting",
		     GTK_MESSAGE_ERROR,
		     zcg_char(NULL, "video_device"), info->error);
	      exit(1);
	    }
	}

      format = zmisc_resolve_pixformat(tveng_get_display_depth(info),
				       x11_get_byte_order());

      if ((format != -1) &&
	  (info->current_controller != TVENG_CONTROLLER_XV))
	{
	  info->format.pixformat = format;
	  if ((tveng_set_capture_format(info) == -1) ||
	      (info->format.pixformat != format))
	    g_warning("Fullscreen format invalid: %s (%d, %d)", info->error,
		      info->format.pixformat, format);
	  printv("fulls: setting %d, got %d\n", format,
		 info->format.pixformat);
	}

      return_value = fullscreen_start(info);
      if (return_value == -1)
	g_warning("couldn't start fullscreen mode");
      break;
    default:
      if (!flag_exit_program) /* Just closing */
	{
#ifdef HAVE_LIBZVBI
	  if (zvbi_get_object())
	    {
	      /* start vbi code */
	      gtk_widget_show(lookup_widget(main_window, "appbar2"));
	      ttxview_attach(main_window, lookup_widget(main_window, "tv_screen"),
			     lookup_widget(main_window, "toolbar1"),
			     lookup_widget(main_window, "appbar2"));
	    }
	  else
#endif
	    {
	      ShowBox(_("VBI has been disabled, or it doesn't work."),
		      GTK_MESSAGE_INFO);
	      break;
	    }
	}
      break; /* TVENG_NO_CAPTURE */
    }

  /* Restore old input if we found it earlier */
  if (old_name != NULL)
    if (-1 == tveng_set_input_by_name(old_name, info))
      g_warning("couldn't restore old input");

  g_free (old_name);

  if (mode != new_mode)
    zcs_int(mode, "previous_mode");

  /* Update the standards, channels, etc */
  zmodel_changed(z_input_model);
  /* Updating the properties is not so useful, and it isn't so easy,
     since there might be multiple properties dialogs open */

  /* Restore mute state */
  if (muted >= 0)
    set_mute (!!muted, FALSE, FALSE);

  /* Update the controls window if it's open */
  update_control_box(info);

  /* Find optimum size for widgets */
  gtk_widget_queue_resize(main_window);

  return return_value;
}

int
z_restart_everything(enum tveng_capture_mode mode,
		     tveng_device_info * info)
{
  int result = tveng_restart_everything(mode, info);

  if (result)
    return result;

  if (info->current_mode == TVENG_CAPTURE_WINDOW)
    overlay_sync(FALSE);

  return 0;
}

void set_stock_pixmap	(GtkWidget	*button,
			 const gchar	*new_pix)
{
  gtk_button_set_use_stock (GTK_BUTTON (button), TRUE);
  gtk_button_set_label (GTK_BUTTON (button), new_pix);
}

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
				 gint		dest_y)
{
  gint src_w = gdk_pixbuf_get_width(src_pixbuf);
  gint src_h = gdk_pixbuf_get_height(src_pixbuf);
  gint dest_w = gdk_pixbuf_get_width(dest_pixbuf);
  gint dest_h = gdk_pixbuf_get_height(dest_pixbuf);

  if (src_x < 0)
    {
      width += src_x;
      dest_x -= src_x;
      src_x = 0;
    }
  if (src_y < 0)
    {
      height += src_y;
      dest_y -= src_y;
      src_y = 0;
    }

  if (src_x + width > src_w)
    width = src_w - src_x;
  if (src_y + height > src_h)
    height = src_h - src_y;

  if (dest_x < 0)
    {
      src_x -= dest_x;
      width += dest_x;
      dest_x = 0;
    }
  if (dest_y < 0)
    {
      src_y -= dest_y;
      height += dest_y;
      dest_y = 0;
    }

  if (dest_x + width > dest_w)
    width = dest_w - dest_x;
  if (dest_y + height > dest_h)
    height = dest_h - dest_y;

  if ((width <= 0) || (height <= 0))
    return;

  gdk_pixbuf_copy_area(src_pixbuf, src_x, src_y, width, height,
		       dest_pixbuf, dest_x, dest_y);
}

void
z_pixbuf_render_to_drawable	(GdkPixbuf	*pixbuf,
				 GdkWindow	*window,
				 GdkGC		*gc,
				 gint		x,
				 gint		y,
				 gint		width,
				 gint		height)
{
  gint w, h;

  if (!pixbuf)
    return;

  w = gdk_pixbuf_get_width(pixbuf);
  h = gdk_pixbuf_get_height(pixbuf);

  if (x < 0)
    {
      width += x;
      x = 0;
    }
  if (y < 0)
    {
      height += 0;
      y = 0;
    }

  if (x + width > w)
    width = w - x;
  if (y + height > h)
    height = h - y;

  if (width < 0 || height < 0)
    return;

  gdk_pixbuf_render_to_drawable(pixbuf, window, gc, x, y, x, y, width,
				height, GDK_RGB_DITHER_NORMAL, x, y);
}

gint
z_menu_get_index		(GtkWidget	*menu,
				 GtkWidget	*item)
{
  gint return_value =
    g_list_index(GTK_MENU_SHELL(menu)->children, item);

  return return_value ? return_value : -1;
}

gint
z_option_menu_get_active	(GtkWidget	*option_menu)
{
  return gtk_option_menu_get_history (GTK_OPTION_MENU (option_menu));
}

void
z_option_menu_set_active	(GtkWidget	*option_menu,
				 gint		index)
{
  gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), index);
}

static GtkAccelGroup *accel_group = NULL;

static void
change_pixmenuitem_label		(GtkWidget	*menuitem,
					 const gchar	*new_label)
{
  GtkWidget *widget = GTK_BIN(menuitem)->child;

  gtk_label_set_text(GTK_LABEL(widget), new_label);
}

void
z_change_menuitem			 (GtkWidget	*widget,
					  const gchar	*new_pixmap,
					  const gchar	*new_label,
					  const gchar	*new_tooltip)
{
  GtkWidget *image;

  if (new_label)
    change_pixmenuitem_label(widget, new_label);
  if (new_tooltip)
    z_tooltip_set(widget, new_tooltip);
  if (new_pixmap)
    {
      image = gtk_image_new_from_stock (new_pixmap, GTK_ICON_SIZE_MENU);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (widget),
				     image);
      /* not sure whether this is necessary, but won't harm */
      gtk_widget_show (image);
     }
}

static void
appbar_hide(GtkWidget *appbar)
{
  gtk_widget_hide(appbar);
  gtk_widget_queue_resize(main_window);
}

static void
add_hide(GtkWidget *appbar)
{
  GtkWidget *old =
    g_object_get_data(G_OBJECT(appbar), "hide_button");
  GtkWidget *widget;

  if (old)
    return;

  widget = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  z_tooltip_set(widget, _("Hide the status bar"));

  if (widget)
    gtk_box_pack_end(GTK_BOX(appbar), widget, FALSE, FALSE, 0);

  gtk_widget_show(widget);
  g_signal_connect_swapped(G_OBJECT(widget), "clicked",
			   G_CALLBACK(appbar_hide),
			   appbar);

  g_object_set_data(G_OBJECT(appbar), "hide_button", widget);
}

static gint status_hide_timeout_id = -1;

static gint
status_hide_timeout	(void		*ignored)
{
  GtkWidget *appbar2 =
    lookup_widget(main_window, "appbar2");

  appbar_hide (appbar2);

  status_hide_timeout_id = -1;
  return FALSE; /* Do not call me again */
}

void z_status_print(const gchar *message, gint timeout)
{
  GtkWidget *appbar2 =
    lookup_widget(main_window, "appbar2");
  GtkWidget *status = gnome_appbar_get_status (GNOME_APPBAR (appbar2));

  add_hide(appbar2);

  gtk_label_set_text (GTK_LABEL (status), message);
  gtk_widget_show(appbar2);

  if (status_hide_timeout_id > -1)
    gtk_timeout_remove(status_hide_timeout_id);

  if (timeout > 0)
    status_hide_timeout_id =
      gtk_timeout_add(timeout, status_hide_timeout, NULL);
  else
    status_hide_timeout_id = -1;
}

void z_status_print_markup (const gchar *markup, gint timeout)
{
  GtkWidget *appbar2 =
    lookup_widget(main_window, "appbar2");
  GtkWidget *status = gnome_appbar_get_status (GNOME_APPBAR (appbar2));

  add_hide(appbar2);

  gtk_label_set_markup (GTK_LABEL (status), markup);
  gtk_widget_show(appbar2);

  if (status_hide_timeout_id > -1)
    gtk_timeout_remove(status_hide_timeout_id);

  if (timeout > 0)
    status_hide_timeout_id =
      gtk_timeout_add(timeout, status_hide_timeout, NULL);
  else
    status_hide_timeout_id = -1;
}

/* FIXME: [Hide] button */
void z_status_set_widget(GtkWidget * widget)
{
  GtkWidget *appbar2 =
    lookup_widget(main_window, "appbar2");
  GtkWidget *old =
    g_object_get_data(G_OBJECT(appbar2), "old_widget");

  if (old)
    gtk_container_remove(GTK_CONTAINER(appbar2), old);

  if (widget)
    gtk_box_pack_end(GTK_BOX(appbar2), widget, FALSE, FALSE, 0);

  add_hide(appbar2);

  g_object_set_data(G_OBJECT(appbar2), "old_widget", widget);

  gtk_widget_show(appbar2);
}

gboolean
z_build_path(const gchar *path, gchar **error_description)
{
  struct stat sb;
  gchar *b;
  gint i;

  if (!path || *path != '/')
    {
      /* FIXME */
      if (error_description)
	*error_description =
	  g_strdup(_("The path must start with /"));
      return FALSE;
    }
    
  for (i=1; path[i]; i++)
    if (path[i] == '/' || !path[i+1])
      {
	b = g_strndup(path, i+1);

	if (stat(b, &sb) < 0)
	  {
	    if (mkdir(b, S_IRUSR | S_IWUSR | S_IXUSR) < 0)
	      {
		if (error_description)
		  *error_description =
		    g_strdup_printf(_("Cannot create %s: %s"), b,
				    strerror(errno));
		g_free(b);
		return FALSE;
	      }
	    else
	      g_assert(stat(b, &sb) >= 0);
	  }

	if (!S_ISDIR(sb.st_mode))
	  {
	    if (error_description)
	      *error_description =
		g_strdup_printf(_("%s is not a directory"), b);
	    g_free(b);
	    return FALSE;
	  }

	g_free(b);
      }

  return TRUE;
}

gchar *
z_replace_filename_extension (gchar *filename, gchar *new_ext)
{
  gchar *name, *ext;
  gint len;

  if (!filename)
    return NULL;

  len = strlen (filename);

  /* last '.' in last part of name */
  for (ext = filename + len - 1;
       ext > filename && *ext != '.' && *ext != '/';
       ext--);

  if (*ext != '.')
    return g_strdup (filename);

  len = ext - filename;

  if (!new_ext)
    return g_strndup(filename, len);

  name = g_malloc (len + strlen (new_ext) + 2);
  memcpy (name, filename, len + 1);
  strcpy (name + len + 1, new_ext);

  return name;
}

gchar *
z_build_filename (const gchar *dirname, const gchar *filename)
{
  gchar *name, *dir;
  gint trailing_slashes = 0, i;

  if (!dirname || strlen (dirname) == 0)
    return g_strdup (filename);

  dir = g_strdup (dirname);
  g_strstrip (dir);

  for (i = strlen (dir); i > 0 && dir[i - 1] == '/'; i--)
    trailing_slashes++;

  if (trailing_slashes <= 0)
    name = g_strconcat (dir, "/", filename, NULL);
  else if (trailing_slashes == 1)
    name = g_strconcat (dir, filename, NULL);
  else
    {
      gchar *temp = g_strndup (dir, i + 1);
      name = g_strconcat (dir, filename, NULL);
      g_free (temp);
    }

  g_free (dir);

  return name;
}

/* Why has this function been deprecated in gtk 2 is a mistery to me */
static void
z_entry_append_text	(gpointer e, const gchar *text)
{
  GtkEditable * _e = GTK_EDITABLE (e);
  gint pos;

  gtk_editable_set_position (_e, -1);
  pos = gtk_editable_get_position (_e);
  gtk_editable_insert_text (_e, text, strlen (text), &pos);
}

/* See ttx export or screenshot for a demo */
void
z_on_electric_filename (GtkWidget *w, gpointer user_data)
{
  gchar **bpp = (gchar **) user_data;
  gchar *basename = (gchar *)
    g_object_get_data (G_OBJECT (w), "basename");
  const gchar *name = gtk_entry_get_text (GTK_ENTRY (w));
  const gchar *ext;
  gchar *baseext;
  gint len, baselen, baseextlen;

  g_assert(basename != NULL);
  baselen = strlen(basename);
  /* last '.' in basename */
  for (baseext = basename + baselen - 1; baseext > basename
	 && *baseext != '.'; baseext--);
  baseextlen = (*baseext == '.') ?
    baselen - (baseext - basename) : 0;

  /* This function is usually a callback handler for the "changed"
     signal in a GtkEditable. Since we will change the editable too,
     block the signal emission while we are editing */
  g_signal_handlers_block_by_func (G_OBJECT (w), 
				   z_on_electric_filename, user_data);

  len = strlen(name);
  /* last '/' in name */
  for (ext = name + len - 1; ext > name && *ext != '/'; ext--);
  /* first '.' in last part of name */
  for (; *ext && *ext != '.'; ext++);

  /* Tack basename on if no name or ends with '/' */
  if (len == 0 || name[len - 1] == '/')
    {
      z_entry_append_text (w, basename);
      gtk_editable_set_position (GTK_EDITABLE (w), len);
    }
  /* Cut off basename if not prepended by '/' */
  else if (len > baselen
	   && strcmp(&name[len - baselen], basename) == 0
	   && name[len - baselen - 1] != '/')
    {
      gchar *buf = g_strndup(name, len - baselen);
      gtk_entry_set_text (GTK_ENTRY (w), buf);
      /* Attach baseext if none already left of basename */
      if (baseextlen > 0 && ext < (buf + len - baselen))
	{
	  z_entry_append_text (w, baseext);
	  gtk_editable_set_position (GTK_EDITABLE (w), len - baselen);
	}
      g_free(buf);
    }
  /* Cut off baseext when duplicate */
  else if (baseextlen > 0 && len > baseextlen
	   && strcmp(&name[len - baseextlen], baseext) == 0
	   && ext < (name + len - baseextlen))
    {
      gchar *buf = g_strndup(name, len - baseextlen);
      gtk_entry_set_text (GTK_ENTRY (w), buf);
      g_free(buf);
    }
  else if (bpp)
    {
      g_free(*bpp);
      *bpp = g_strdup(name);
    }

  /* unblock signal emission */
  g_signal_handlers_unblock_by_func (G_OBJECT (w), 
				     z_on_electric_filename,
				     user_data);
  g_signal_stop_emission_by_name (G_OBJECT (w), "changed");
}

static void
set_orientation_recursive	(GtkToolbar	*toolbar,
				 GtkOrientation orientation)
{
  GList *p = toolbar->children;
  GtkToolbarChild *child;

  while (p)
    {
      child = (GtkToolbarChild*)p->data;
      
      if (child->type == GTK_TOOLBAR_CHILD_WIDGET &&
	  GTK_IS_TOOLBAR(child->widget))
	set_orientation_recursive(GTK_TOOLBAR(child->widget), orientation);
      p = p->next;
    }

  gtk_toolbar_set_orientation(toolbar, orientation);
}

static void
on_orientation_changed		(GtkToolbar	*toolbar,
				 GtkOrientation	orientation,
				 gpointer	data)
{
  GList *p;
  GtkToolbarChild *child;

  if (!toolbar)
    return;

  p = toolbar->children;

  while (p)
    {
      child = (GtkToolbarChild*)p->data;

      if (child->type == GTK_TOOLBAR_CHILD_WIDGET &&
	  GTK_IS_TOOLBAR(child->widget))
	set_orientation_recursive(GTK_TOOLBAR(child->widget), orientation);
      p = p->next;
    }  
}

static void
set_style_recursive		(GtkToolbar	*toolbar,
				 GtkToolbarStyle style)
{
  GList *p = toolbar->children;
  GtkToolbarChild *child;

  while (p)
    {
      child = (GtkToolbarChild*)p->data;
      
      if (child->type == GTK_TOOLBAR_CHILD_WIDGET &&
	  GTK_IS_TOOLBAR(child->widget))
	set_style_recursive(GTK_TOOLBAR(child->widget), style);
      p = p->next;
    }

  gtk_toolbar_set_style(toolbar, style);
}

static void
on_style_changed		(GtkToolbar	*toolbar,
				 GtkToolbarStyle style,
				 gpointer	data)
{
  GList *p;
  GtkToolbarChild *child;

  if (!toolbar)
    return;

  p = toolbar->children;

  while (p)
    {
      child = (GtkToolbarChild*)p->data;

      if (child->type == GTK_TOOLBAR_CHILD_WIDGET &&
	  GTK_IS_TOOLBAR(child->widget))
	set_style_recursive(GTK_TOOLBAR(child->widget), style);
      p = p->next;
    }
}

void
propagate_toolbar_changes	(GtkWidget	*toolbar)
{
  g_return_if_fail (GTK_IS_TOOLBAR(toolbar));

  g_signal_connect(G_OBJECT(toolbar), "style-changed",
		   GTK_SIGNAL_FUNC(on_style_changed),
		   NULL);
  g_signal_connect(G_OBJECT(toolbar), "orientation-changed",
		   GTK_SIGNAL_FUNC(on_orientation_changed),
		   NULL);
}

void zmisc_overlay_subtitles	(gint page)
{
#ifdef HAVE_LIBZVBI
  GtkWidget *closed_caption1;

  zvbi_page = page;
  
  zconf_set_integer(zvbi_page,
		    "/zapping/internal/callbacks/zvbi_page");
  zconf_set_boolean(TRUE, "/zapping/internal/callbacks/closed_caption");

  if (main_info->current_mode == TVENG_NO_CAPTURE)
    zmisc_restore_previous_mode(main_info);

  osd_clear();
  
  closed_caption1 =
    lookup_widget(main_window, "closed_caption1");
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(closed_caption1),
				 TRUE);
#endif /* HAVE_LIBZVBI */
}

void
z_set_cursor	(GdkWindow	*window,
		 guint		cid)
{
  GdkCursor *cursor;

  /* blank cursor */
  if (cid == 0)
    {
#define empty_cursor_width 16
#define empty_cursor_height 16
      unsigned char empty_cursor_bits[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      unsigned char empty_cursor_mask[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      GdkColor fg = {0, 0, 0, 0};
      GdkColor bg = {0, 0, 0, 0};
      GdkPixmap *source, *mask;

      source = gdk_bitmap_create_from_data(NULL, empty_cursor_bits,
					   empty_cursor_width,
					   empty_cursor_height);

      mask = gdk_bitmap_create_from_data(NULL, empty_cursor_mask,
					 empty_cursor_width,
					 empty_cursor_height);

      cursor = gdk_cursor_new_from_pixmap(source, mask, &fg, &bg, 8, 8);
      
      g_object_unref (G_OBJECT (source));
      g_object_unref (G_OBJECT (mask));
    }
  else
    {
      if (cid >= GDK_LAST_CURSOR)
	cid = GDK_LAST_CURSOR-2;
      cid &= ~1;
      cursor = gdk_cursor_new(cid);
    }

  if (!cursor)
    return;

  gdk_window_set_cursor(window, cursor);
  gdk_cursor_unref(cursor);
}

GtkWidget *
z_load_pixmap			(const gchar *		name)
{
  GtkWidget *image;
  gchar *path;

  path = g_strconcat (PACKAGE_PIXMAPS_DIR "/", name, NULL);
  image = gtk_image_new_from_file (path);
  g_free (path);

  gtk_widget_show (image);

  return image;
}

GtkWindow *
z_main_window		(void)
{
  return GTK_WINDOW(main_window);
}

gchar *
find_unused_name		(const gchar *		dir,
				 const gchar *		file,
				 const gchar *		ext)
{
  gchar *buf = NULL;
  gchar *name;
  const gchar *slash = "";
  const gchar *dot = "";
  gint index = 0;
  const gchar *s;
  gint n;
  struct stat sb;

  if (!dir)
    dir = "";
  else if (dir[0] && dir[strlen (dir) - 1] != '/')
    slash = "/";

  if (!file || !file[0])
    return g_strconcat (dir, slash, NULL);

  n = strlen (file);

  /* cut off existing extension from @file */
  for (s = file + n; s > file;)
    if (*--s == '.')
      {
	if (s == file || s[-1] == '/')
	  return g_strconcat (dir, slash, NULL);
	else
	  break;
      }

  if (s == file) /* has no extension */
    s = file + n;
  else if (!ext) /* no new ext, keep old */
    ext = s;

  /* parse off existing numeral suffix */
  for (n = 1; s > file && n < 10000000; n *= 10)
    if (isdigit(s[-1]))
      index += (*--s & 15) * n;
    else
      break;

  name = g_strndup (file, s - file);

  if (!ext)
    ext = "";
  else if (ext[0] && ext[0] != '.')
    dot = ".";

  if (index == 0 && n == 1) /* had no suffix */
    {
      /* Try first without numeral suffix */
      buf = g_strdup_printf ("%s%s%s%s%s",
			     dir, slash, name, dot, ext);
      index = 2; /* foo, foo2, foo3, ... */
    }
  /* else fooN, fooN+1, fooN+2 */

  for (n = 10000; n > 0; n--) /* eventually abort */
    {
      if (!buf)
	buf = g_strdup_printf("%s%s%s%d%s%s",
			      dir, slash, name, index++, dot, ext);

      /* Try to query file availability */
      /*
       * Note: This is easy to break, but since there's no good(tm)
       * way to predict an available file name, just do the simple thing.
       */
      if (stat (buf, &sb) == -1)
	{
	  switch (errno)
	    {
	    case ENOENT:
	    case ENOTDIR:
	      /* take this */
	      break;

	    default:
	      /* give up */
	      g_free (buf);
	      buf = NULL;
	      break;
	    }

	  break;
	}
      else
	{
	  /* exists, try other */
	  g_free (buf);
	  buf = NULL;
	}
    }

  g_free (name);

  return buf ? buf : g_strconcat (dir, slash, NULL);
}

/*
 *  "Spinslider"
 *
 *  [12345___|<>] unit [-<>--------][Reset]
 */

typedef struct z_spinslider z_spinslider;

struct z_spinslider
{
  GtkWidget *		hbox;
  GtkAdjustment *	spin_adj;
  GtkAdjustment *	hscale_adj;

  gfloat		history[3];
  guint			reset_state;
  gboolean		in_reset;
};

static inline z_spinslider *
get_spinslider			(GtkWidget *		hbox)
{
  z_spinslider *sp;

  sp = g_object_get_data (G_OBJECT (hbox), "z_spinslider");
  g_assert (sp != NULL);

  return sp;
}

GtkAdjustment *
z_spinslider_get_spin_adj	(GtkWidget *		hbox)
{
  return get_spinslider (hbox)->spin_adj;
}

GtkAdjustment *
z_spinslider_get_hscale_adj	(GtkWidget *		hbox)
{
  return get_spinslider (hbox)->hscale_adj;
}

gfloat
z_spinslider_get_value		(GtkWidget *		hbox)
{
  return get_spinslider (hbox)->spin_adj->value;
}

void
z_spinslider_set_value		(GtkWidget *		hbox,
				 gfloat			value)
{
  z_spinslider *sp = get_spinslider (hbox);

  gtk_adjustment_set_value (sp->spin_adj, value);
  gtk_adjustment_set_value (sp->hscale_adj, value);
}

void
z_spinslider_set_reset_value	(GtkWidget *		hbox,
				 gfloat			value)
{
  z_spinslider *sp = get_spinslider (hbox);

  sp->history[sp->reset_state] = value;

  gtk_adjustment_set_value (sp->spin_adj, value);
  gtk_adjustment_set_value (sp->hscale_adj, value);
}

void
z_spinslider_adjustment_changed	(GtkWidget *		hbox)
{
  z_spinslider *sp = get_spinslider (hbox);

  sp->hscale_adj->value = sp->spin_adj->value;
  sp->hscale_adj->lower = sp->spin_adj->lower;
  sp->hscale_adj->upper = sp->spin_adj->upper + sp->spin_adj->page_size;
  sp->hscale_adj->step_increment = sp->spin_adj->step_increment;
  sp->hscale_adj->page_increment = sp->spin_adj->page_increment;
  sp->hscale_adj->page_size = sp->spin_adj->page_size;
  gtk_adjustment_changed (sp->spin_adj);
  gtk_adjustment_changed (sp->hscale_adj);
}

static void
on_z_spinslider_hscale_changed	(GtkWidget *		widget,
				 z_spinslider *		sp)
{
  if (sp->spin_adj->value != sp->hscale_adj->value)
    gtk_adjustment_set_value (sp->spin_adj, sp->hscale_adj->value);
}

static void
on_z_spinslider_spinbutton_changed (GtkWidget *		widget,
				    z_spinslider *	sp)
{
  if (!sp->in_reset)
    {
      if (sp->reset_state != 0)
	{
	  sp->history[0] = sp->history[1];
	  sp->history[1] = sp->history[2];
	  sp->reset_state--;
	}
      sp->history[2] = sp->spin_adj->value;
    }

  if (sp->spin_adj->value != sp->hscale_adj->value)
    gtk_adjustment_set_value (sp->hscale_adj, sp->spin_adj->value);
}

static void
on_z_spinslider_reset		(GtkWidget *		widget,
				 z_spinslider *		sp)
{
  gfloat current_value;

  current_value = sp->history[2];
  sp->history[2] = sp->history[1];
  sp->history[1] = sp->history[0];
  sp->history[0] = current_value;

  sp->in_reset = TRUE;

  gtk_adjustment_set_value (sp->spin_adj, sp->history[2]);
  gtk_adjustment_set_value (sp->hscale_adj, sp->history[2]);

  sp->in_reset = FALSE;

  if (sp->reset_state == 0
      && fabs (sp->history[0] - sp->history[1]) < 1e-6)
    sp->reset_state = 2;
  else
    sp->reset_state = (sp->reset_state + 1) % 3;
}

GtkWidget *
z_spinslider_new		(GtkAdjustment *	spin_adj,
				 GtkAdjustment *	hscale_adj,
				 const gchar *		unit,
				 gfloat			reset,
				 gint			digits)
{
  z_spinslider *sp;

  g_assert (spin_adj != NULL);

  sp = g_malloc (sizeof (*sp));

  sp->spin_adj = spin_adj;
  sp->hscale_adj = hscale_adj;

  sp->hbox = gtk_hbox_new (FALSE, 0);
  g_object_set_data_full (G_OBJECT (sp->hbox), "z_spinslider", sp,
			  (GDestroyNotify) g_free);

  /*
    fprintf(stderr, "zss_new %f %f...%f  %f %f  %f  %d\n",
    spin_adj->value,
    spin_adj->lower, spin_adj->upper,
    spin_adj->step_increment, spin_adj->page_increment,
    spin_adj->page_size, digits);
  */

  /* Spin button */

  {
    GtkWidget *spinbutton;

    spinbutton = gtk_spin_button_new (sp->spin_adj,
				      sp->spin_adj->step_increment, digits);
    gtk_widget_show (spinbutton);
    /* I don't see how to set "as much as needed", so hacking this up */
    gtk_widget_set_size_request (spinbutton, 80, -1);
    gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (spinbutton),
				       GTK_UPDATE_IF_VALID);
    gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbutton), TRUE);
    gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinbutton), TRUE);
    gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (spinbutton), TRUE);
    gtk_box_pack_start (GTK_BOX (sp->hbox), spinbutton, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (sp->spin_adj), "value-changed",
		      G_CALLBACK (on_z_spinslider_spinbutton_changed), sp);
  }

  /* Unit name */

  if (unit)
    {
      GtkWidget *label;

      label = gtk_label_new (unit);
      gtk_widget_show (label);
      gtk_box_pack_start (GTK_BOX (sp->hbox), label, FALSE, FALSE, 3);
    }

  /* Slider */

  /* Necessary to reach spin_adj->upper with slider */
  if (!hscale_adj)
    sp->hscale_adj = GTK_ADJUSTMENT (gtk_adjustment_new
	(sp->spin_adj->value,
	 sp->spin_adj->lower,
	 sp->spin_adj->upper + sp->spin_adj->page_size,
	 sp->spin_adj->step_increment,
	 sp->spin_adj->page_increment,
	 sp->spin_adj->page_size));

  {
    GtkWidget *hscale;

    hscale = gtk_hscale_new (sp->hscale_adj);
    /* Another hack */
    gtk_widget_set_size_request (hscale, 80, -1);
    gtk_widget_show (hscale);
    gtk_scale_set_draw_value (GTK_SCALE (hscale), FALSE);
    gtk_scale_set_digits (GTK_SCALE (hscale), -digits);
    gtk_box_pack_start (GTK_BOX (sp->hbox), hscale, TRUE, TRUE, 3);
    g_signal_connect (G_OBJECT (sp->hscale_adj), "value-changed",
		      G_CALLBACK (on_z_spinslider_hscale_changed), sp);
  }

  /* Reset button */

  {
    GtkWidget *button;
    GtkWidget *pixmap;

    sp->history[0] = reset;
    sp->history[1] = reset;
    sp->history[2] = reset;
    sp->reset_state = 0;
    sp->in_reset = FALSE;

    if ((pixmap = z_load_pixmap ("reset.png")))
      {
	button = gtk_button_new ();
	gtk_container_add (GTK_CONTAINER (button), pixmap);
	z_tooltip_set (button, _("Reset"));
      }
    else
      {
	button = gtk_button_new_with_label (_("Reset"));
      }

    gtk_widget_show (button);
    gtk_box_pack_start (GTK_BOX (sp->hbox), button, FALSE, FALSE, 0);
    g_signal_connect (G_OBJECT (button), "pressed",
		      G_CALLBACK (on_z_spinslider_reset), sp);
  }

  return sp->hbox;
}

/**
 * z_widget_add_accelerator:
 * @widget: 
 * @accel_signal: 
 * @accel_key: 
 * @accel_mods: 
 * 
 * Like gtk_widget_add_accelerator() but takes care of creating the
 * accel group.
 **/
void
z_widget_add_accelerator	(GtkWidget	*widget,
				 const gchar	*accel_signal,
				 guint		accel_key,
				 guint		accel_mods)
{
  if (!accel_group)
    {
      accel_group = gtk_accel_group_new();
      gtk_window_add_accel_group(GTK_WINDOW(main_window),
				 accel_group);
    }

  gtk_widget_add_accelerator(widget, accel_signal, accel_group,
			     accel_key, accel_mods, GTK_ACCEL_VISIBLE);
}

static void
on_entry_activated (GObject *entry, GtkDialog *dialog)
{
  gtk_dialog_response (dialog, GPOINTER_TO_INT
		       (g_object_get_data (entry, "zmisc-response")));
}

void
z_entry_emits_response		(GtkWidget	*entry,
				 GtkDialog	*dialog,
				 GtkResponseType response)
{
  g_signal_connect (G_OBJECT (entry), "activate",
		    G_CALLBACK (on_entry_activated),
		    dialog);

  g_object_set_data (G_OBJECT (entry), "zmisc-response",
		     GINT_TO_POINTER (response));
}

/*
 *  Application stock icons
 */

static gboolean
icon_factory_add_file		(GtkIconFactory *	factory,
				 const gchar *		stock_id,
				 const gchar *		filename)
{
  GtkIconSet *icon_set;
  GdkPixbuf *pixbuf;
  GError *err;
  gchar *path;
 
  path = g_strconcat (PACKAGE_PIXMAPS_DIR "/", filename, NULL);

  pixbuf = gdk_pixbuf_new_from_file (path, &err);

  g_free (path);

  if (!pixbuf && err)
    {
#ifdef ZMISC_DEBUG_STOCK /* FIXME */
      fprintf (stderr, "Cannot read image file '%s':\n%s\n",
	       err->message);
#endif
      g_error_free (err);
      return FALSE;
    }

  g_assert (pixbuf && !err);

  icon_set = gtk_icon_set_new_from_pixbuf (pixbuf);

  gtk_icon_factory_add (factory, stock_id, icon_set);

  return TRUE;
}

#if 0

static gboolean
icon_factory_add_pixdata	(GtkIconFactory *	factory,
				 const gchar *		stock_id,
				 const GdkPixdata *	pixdata)
{
  GtkIconSet *icon_set;
  GdkPixbuf *pixbuf;
  GError *err;

  pixbuf = gdk_pixbuf_from_pixdata (pixdata, /* copy_pixels */ FALSE, &err);

  if (!pixbuf && err)
    {
#ifdef ZMISC_DEBUG_STOCK /* FIXME */
      fprintf (stderr, "Cannot read pixdata:\n%s\n", err->message);
#endif
      g_error_free (err);
      return FALSE;
    }

  g_assert (pixbuf && !err);

  iconset = gtk_icon_set_new_from_pixbuf (pixbuf);

  gtk_icon_factory_add (factory, stock_id, icon_set);

  return TRUE;
}

#endif

gboolean
z_icon_factory_add_default_files
				(const gchar *		stock_id,
				 const gchar *		filename,
				 ...)
{
  GtkIconFactory *factory;
  va_list ap;

  factory = gtk_icon_factory_new ();

  if (!icon_factory_add_file (factory, stock_id, filename))
    goto failure;

  va_start (ap, filename);

  for (;;)
    {
      stock_id = va_arg (ap, const gchar *);

      if (!stock_id)
	break;

      filename = va_arg (ap, const gchar *);

      if (!icon_factory_add_file (factory, stock_id, filename))
	goto failure;
    }

  va_end (ap);

  gtk_icon_factory_add_default (factory);

  return TRUE;

 failure:
  va_end (ap);
  g_object_unref (G_OBJECT (factory));
  return FALSE;
}
