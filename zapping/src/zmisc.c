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

#include <gdk/gdkx.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>

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

extern tveng_device_info * main_info;
extern volatile gboolean flag_exit_program;
extern GtkWidget * main_window;
extern gint disable_preview; /* TRUE if preview won't work */
extern gboolean xv_present;
int debug_msg = FALSE; /* Debugging messages on or off */

/*
  Prints a message box showing an error, with the location of the code
  that called the function. If run is TRUE, gnome_dialog_run will be
  called instead of just showing the dialog. Keep in mind that this
  will block the capture.
*/
GtkWidget * ShowBoxReal(const gchar * sourcefile,
			const gint line,
			const gchar * func,
			const gchar * message,
			const gchar * message_box_type,
			gboolean blocking, gboolean modal)
{
  GtkWidget * dialog;
  gchar * str;
  gchar buffer[256];

  buffer[255] = 0;

  g_snprintf(buffer, 255, " (%d)", line);

  str = g_strconcat(sourcefile, buffer, ": [", func, "]", NULL);

  if (!str)
    return NULL;

  dialog = gnome_message_box_new(message, message_box_type,
				 GNOME_STOCK_BUTTON_OK,
				 NULL);


  gtk_window_set_title(GTK_WINDOW (dialog), str);

  /*
    I know this isn't usual for a dialog box, but this way we can see
    all the title bar if we want to
  */
  gtk_window_set_policy(GTK_WINDOW (dialog), FALSE, TRUE, TRUE);

  g_free(str);

  if (blocking)
    {
      gnome_dialog_run_and_close(GNOME_DIALOG(dialog));
      return NULL;
    }

  gtk_window_set_modal(GTK_WINDOW (dialog), modal);
  gtk_widget_show(dialog);

  return dialog;
}

gchar*
Prompt (GtkWidget *main_window, const gchar *title,
	const gchar *prompt,  const gchar *default_text)
{
  GtkWidget * dialog;
  GtkVBox * vbox;
  GtkWidget *label, *entry;
  gchar *buffer = NULL;

  dialog = gnome_dialog_new(title,
			    GNOME_STOCK_BUTTON_OK,
			    GNOME_STOCK_BUTTON_CANCEL,
			    NULL);
  if (main_window)
    gnome_dialog_set_parent(GNOME_DIALOG (dialog), GTK_WINDOW(main_window));
  gnome_dialog_close_hides(GNOME_DIALOG (dialog), TRUE);
  gnome_dialog_set_default(GNOME_DIALOG (dialog), 0);

  gtk_window_set_title(GTK_WINDOW (dialog), title);
  gtk_window_set_modal(GTK_WINDOW (dialog), TRUE);
  vbox = GTK_VBOX(GNOME_DIALOG(dialog)->vbox);
  if (prompt)
    {
      label = gtk_label_new(prompt);
      gtk_box_pack_start_defaults(GTK_BOX(vbox), label);
      gtk_widget_show(label);
    }
  entry = gtk_entry_new();
  gtk_box_pack_start_defaults(GTK_BOX(vbox), entry);
  gtk_widget_show(entry);
  gnome_dialog_editable_enters(GNOME_DIALOG(dialog), GTK_EDITABLE (entry));
  gtk_widget_grab_focus(entry);
  if (default_text)
    {
      gtk_entry_set_text(GTK_ENTRY(entry), default_text);
      gtk_entry_select_region(GTK_ENTRY(entry), 0, -1);
    }

  if (!gnome_dialog_run_and_close(GNOME_DIALOG(dialog)))
    buffer = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));

  gtk_widget_destroy(dialog);
  
  return buffer;
}

/*
  Creates a GtkPixmapMenuEntry with the desired pixmap and the
  desired label.
*/
GtkWidget * z_gtk_pixmap_menu_item_new(const gchar * label,
				       const gchar * icon)
{
  GtkWidget * pixmap_menu_item;
  GtkWidget * accel_label;
  GtkWidget * pixmap;

  g_assert(label != NULL);

  pixmap_menu_item = gtk_pixmap_menu_item_new();
  accel_label = gtk_accel_label_new (label);
  gtk_misc_set_alignment(GTK_MISC(accel_label), 0.0, 0.5);
  gtk_container_add(GTK_CONTAINER (pixmap_menu_item), accel_label);
  gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label),
				    pixmap_menu_item);
  gtk_widget_show (accel_label);

  if (icon)
    {
      /* if i don't specify the size, the pixmap is too big, but the
	 one libgnomeui creates isn't... why? */
      pixmap = gnome_stock_pixmap_widget_at_size (pixmap_menu_item,
						  icon, 16, 16);
      
      gtk_pixmap_menu_item_set_pixmap(GTK_PIXMAP_MENU_ITEM(pixmap_menu_item),
				      pixmap);
      gtk_widget_show(pixmap);
    }

  return (pixmap_menu_item);
}

void set_tooltip	(GtkWidget	*widget,
			 const gchar	*new_tip)
{
  GtkTooltipsData *td = gtk_tooltips_data_get(widget);
  GtkTooltips *tips;

  if ((!td) || (!td->tooltips))
    tips = gtk_tooltips_new();
  else
    tips = td->tooltips;

  gtk_tooltips_set_tip(tips, widget, new_tip,
		       "private tip, or, er, just babbling, you know");
}

int
zmisc_restore_previous_mode(tveng_device_info * info)
{
  return zmisc_switch_mode(zcg_int(NULL, "previous_mode"), info);
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

  gdk_window_get_size(tv_screen->window, &w, &h);
  gdk_window_get_origin(tv_screen->window, &x, &y);

  muted = tveng_get_mute(info);
  mode = info->current_mode;

  /* Stop current capture mode */
  switch (mode)
    {
    case TVENG_CAPTURE_PREVIEW:
      tveng_stop_everything(info);
      fullscreen_stop(info);
      break;
    case TVENG_CAPTURE_READ:
      capture_stop(info);
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

#ifdef HAVE_LIBZVBI
  if (!flag_exit_program)
    {
      if (new_mode != TVENG_NO_CAPTURE)
	{
	  gtk_widget_hide(lookup_widget(main_window, "appbar2"));
	  ttxview_detach(main_window);
	  set_stock_pixmap(lookup_widget(main_window, "videotext3"),
			   GNOME_STOCK_PIXMAP_ALIGN_JUSTIFY);
	  set_tooltip(lookup_widget(main_window, "videotext3"),
		      _("Use Zapping as a Teletext navigator [Ctrl+Alt+T]"));
	}
      else
	{
	  set_stock_pixmap(lookup_widget(main_window, "videotext3"),
			   GNOME_STOCK_PIXMAP_TABLE_FILL);
	  set_tooltip(lookup_widget(main_window, "videotext3"),
		      _("Return to windowed mode and use the current "
			"page as subtitles"));
	}
    }

  if (new_mode != TVENG_CAPTURE_PREVIEW &&
      new_mode != TVENG_NO_CAPTURE)
    osd_set_window(tv_screen);
  else if (new_mode == TVENG_NO_CAPTURE)
    {
      osd_clear();
      osd_unset_window();
    }
#endif /* HAVE_LIBZVBI */

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
			 GNOME_MESSAGE_BOX_ERROR,
			 zcg_char(NULL, "video_device"), info->error);
		  exit(1);
		}
	      else
		ShowBox("Capture mode not available:\n%s",
			GNOME_MESSAGE_BOX_ERROR, info->error);
	    }
	}
      tveng_set_capture_size(w, h, info);
      return_value = capture_start(tv_screen, info);
      break;
    case TVENG_CAPTURE_WINDOW:
      if (disable_preview) {
	ShowBox("preview has been disabled", GNOME_MESSAGE_BOX_WARNING);
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
		     GNOME_MESSAGE_BOX_ERROR,
		     zcg_char(NULL, "video_device"), info->error);
	      exit(1);
	    }
	}

      if (!tveng_detect_preview(info))
	{
	  ShowBox(_("Preview will not work: %s"),
		  GNOME_MESSAGE_BOX_ERROR, info->error);
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
      if (disable_preview) {
	ShowBox("preview has been disabled", GNOME_MESSAGE_BOX_WARNING);
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
		     GNOME_MESSAGE_BOX_ERROR,
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
		      GNOME_MESSAGE_BOX_INFO);
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

  if (muted != -1)
    set_mute1(!!muted, FALSE, FALSE);
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

/* API flaw */
void set_stock_pixmap	(GtkWidget	*button,
			 const gchar	*new_pix)
{
  GtkWidget *widget = GTK_BIN(button)->child;
  GList *node = g_list_first(GTK_BOX(widget)->children)->next;

  widget = GTK_WIDGET(((GtkBoxChild*)(node->data))->widget);

  gnome_stock_set_icon(GNOME_STOCK(widget), new_pix);
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
  option_menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(option_menu));

  return g_list_index(GTK_MENU_SHELL(option_menu)->children,
		      gtk_menu_get_active(GTK_MENU(option_menu)));
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
  GtkWidget *spixmap;

  if (new_label)
    change_pixmenuitem_label(widget, new_label);
  if (new_tooltip)
    set_tooltip(widget, new_tooltip);
  if (new_pixmap)
    {
      spixmap = gnome_stock_pixmap_widget_at_size(widget, new_pixmap, 16, 16);
      
      /************* THIS SHOULD NEVER BE DONE ***********/
      gtk_object_destroy(GTK_OBJECT(GTK_PIXMAP_MENU_ITEM(widget)->pixmap));
      GTK_PIXMAP_MENU_ITEM(widget)->pixmap = NULL;
      
      /********** BUT THERE'S NO OTHER WAY TO DO IT ******/
      gtk_widget_show(spixmap);
      gtk_pixmap_menu_item_set_pixmap(GTK_PIXMAP_MENU_ITEM(widget),
				      spixmap);
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
    gtk_object_get_data(GTK_OBJECT(appbar), "hide_button");
  GtkWidget *widget;

  if (old)
    return;

  widget = gnome_stock_button(GNOME_STOCK_BUTTON_CLOSE);
  set_tooltip(widget, _("Hide the status bar"));

  if (widget)
    gtk_box_pack_end(GTK_BOX(appbar), widget, FALSE, FALSE, 0);

  gtk_widget_show(widget);
  gtk_signal_connect_object(GTK_OBJECT(widget), "clicked",
			    GTK_SIGNAL_FUNC(appbar_hide),
			    GTK_OBJECT(appbar));

  gtk_object_set_data(GTK_OBJECT(appbar), "hide_button", widget);
}

void z_status_print(const gchar *message)
{
  GtkWidget *appbar2 =
    lookup_widget(main_window, "appbar2");

  add_hide(appbar2);

  gnome_appbar_set_status(GNOME_APPBAR(appbar2), message);
  gtk_widget_show(appbar2);
}

/* FIXME: [Hide] button */
void z_status_set_widget(GtkWidget * widget)
{
  GtkWidget *appbar2 =
    lookup_widget(main_window, "appbar2");
  GtkWidget *old =
    gtk_object_get_data(GTK_OBJECT(appbar2), "old_widget");

  if (old)
    gtk_container_remove(GTK_CONTAINER(appbar2), old);

  if (widget)
    gtk_box_pack_end(GTK_BOX(appbar2), widget, FALSE, FALSE, 0);

  add_hide(appbar2);

  gtk_object_set_data(GTK_OBJECT(appbar2), "old_widget", widget);

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
z_build_filename (gchar *dirname, gchar *filename)
{
  gchar *name;
  gint trailing_slashes = 0, i;

  if (!dirname || strlen (dirname) == 0)
    return g_strdup (filename);

  dirname = g_strdup (dirname);
  g_strstrip (dirname);

  for (i = strlen (dirname); i > 0 && dirname[i - 1] == '/'; i--)
    trailing_slashes++;

  if (trailing_slashes <= 0)
    name = g_strconcat (dirname, "/", filename, NULL);
  else if (trailing_slashes == 1)
    name = g_strconcat (dirname, filename, NULL);
  else
    {
      gchar *temp = g_strndup (dirname, i + 1);
      name = g_strconcat (dirname, filename, NULL);
      g_free (temp);
    }

  g_free (dirname);

  return name;
}

/* See ttx export or screenshot for a demo */
void
z_on_electric_filename (GtkWidget *w, gpointer user_data)
{
  gchar **bpp = (gchar **) user_data;
  gchar *basename = (gchar *)
    gtk_object_get_data (GTK_OBJECT (w), "basename");
  gchar *name = gtk_entry_get_text (GTK_ENTRY (w));
  gchar *baseext, *ext;
  gint len, baselen, baseextlen;

  g_assert(basename != NULL);
  baselen = strlen(basename);
  /* last '.' in basename */
  for (baseext = basename + baselen - 1; baseext > basename
	 && *baseext != '.'; baseext--);
  baseextlen = (*baseext == '.') ?
    baselen - (baseext - basename) : 0;

  len = strlen(name);
  /* last '/' in name */
  for (ext = name + len - 1; ext > name && *ext != '/'; ext--);
  /* first '.' in last part of name */
  for (; *ext && *ext != '.'; ext++);

  /* Tack basename on if no name or ends with '/' */
  if (len == 0 || name[len - 1] == '/')
    {
      gtk_entry_append_text (GTK_ENTRY (w), basename);
      gtk_entry_set_position (GTK_ENTRY (w), len);
    }
  /* Cut off basename if not prepended by '/' */
  else if (len > baselen
	   && strcmp(&name[len - baselen], basename) == 0
	   && name[len - baselen - 1] != '/')
    {
      name = g_strndup(name, len - baselen);
      gtk_entry_set_text (GTK_ENTRY (w), name);
      /* Attach baseext if none already left of basename */
      if (baseextlen > 0 && ext < (name + len - baselen))
	{
	  gtk_entry_append_text (GTK_ENTRY (w), baseext);
	  gtk_entry_set_position (GTK_ENTRY (w), len - baselen);
	}
      g_free(name);
    }
  /* Cut off baseext when duplicate */
  else if (baseextlen > 0 && len > baseextlen
	   && strcmp(&name[len - baseextlen], baseext) == 0
	   && ext < (name + len - baseextlen))
    {
      name = g_strndup(name, len - baseextlen);
      gtk_entry_set_text (GTK_ENTRY (w), name);
      g_free(name);
    }
  else if (bpp)
    {
      g_free(*bpp);
      *bpp = g_strdup(name);
    }
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

  gtk_signal_connect(GTK_OBJECT(toolbar), "style-changed",
		     GTK_SIGNAL_FUNC(on_style_changed),
		     NULL);
  gtk_signal_connect(GTK_OBJECT(toolbar), "orientation-changed",
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
      
      gdk_pixmap_unref(source);
      gdk_pixmap_unref(mask);
    }
  else
    {
      if (cid >= GDK_NUM_GLYPHS)
	cid = GDK_NUM_GLYPHS-2;
      cid &= ~1;
      cursor = gdk_cursor_new(cid);
    }

  if (!cursor)
    return;

  gdk_window_set_cursor(window, cursor);
  gdk_cursor_destroy(cursor);
}

GtkWidget *
z_pixmap_new_from_file		(const gchar	*file)
{
  GdkBitmap *mask;
  GdkPixmap *pixmap;
  GdkPixbuf *pb;
  GtkWidget *pix;

  pb = gdk_pixbuf_new_from_file(file);

  if (!pb) {
    printv("Unable to load pixmap \"%s\", errno %d: %s\n",
	   file, errno, strerror(errno));
    return NULL;
  }

  gdk_pixbuf_render_pixmap_and_mask(pb, &pixmap, &mask, 128);
  gdk_pixbuf_unref(pb);
  pix = gtk_pixmap_new(pixmap, mask);
  if (mask)
    gdk_bitmap_unref(mask);
  gdk_pixmap_unref(pixmap);

  return pix;
}

GtkWindow *
z_main_window		(void)
{
  return GTK_WINDOW(main_window);
}

gchar*
find_unused_name (const gchar * dir, const gchar * prefix,
		  const gchar * suffix)
{
  gint index = 1;
  gchar * buf = NULL, *dot;
  struct stat sb;

  dot = (suffix[0] == '.') ? "" : ".";

  while (TRUE)
    {
      /* Add a slash if needed */
      if ((!*dir) || (dir[strlen(dir)-1] != '/'))
	buf = g_strdup_printf("%s/%s%d%s%s", dir, prefix, index++,
			      dot, suffix);
      else
	buf = g_strdup_printf("%s%s%d%s%s", dir, prefix, index++,
			      dot, suffix);

      /* Try to query file availability */
      /*
       * Note: This is easy to break, but since there's no good(tm)
       * way to predict an available file name, just do the simple thing.
       */
      if (stat(buf, &sb))
	break;

      g_free(buf);
    };

  return buf;
}

/*
 *  "Spinslider"
 */

GtkAdjustment *
z_spinslider_get_spin_adj		(GtkWidget *hbox)
{
  GtkAdjustment * adj;

  adj = gtk_object_get_data (GTK_OBJECT (hbox), "spin_adj");
  g_assert (adj);
  return adj;
}

GtkAdjustment *
z_spinslider_get_hscale_adj		(GtkWidget *hbox)
{
  GtkAdjustment * adj;

  adj = gtk_object_get_data (GTK_OBJECT (hbox), "hscale_adj");
  g_assert (adj);
  return adj;
}

gfloat
z_spinslider_get_value			(GtkWidget *hbox)
{
  GtkAdjustment * adj;

  adj = gtk_object_get_data (GTK_OBJECT (hbox), "spin_adj");
  g_assert (adj);
  return adj->value;
}

void
z_spinslider_set_value			(GtkWidget *hbox,
					 gfloat value)
{
  GtkAdjustment * spin_adj, * hscale_adj;

  spin_adj = gtk_object_get_data (GTK_OBJECT (hbox), "spin_adj");
  hscale_adj = gtk_object_get_data (GTK_OBJECT (hbox), "hscale_adj");
  g_assert (spin_adj && hscale_adj);
  gtk_adjustment_set_value (spin_adj, value);
  gtk_adjustment_set_value (hscale_adj, value);
}

void
z_spinslider_set_reset_value		(GtkWidget *hbox,
					 gfloat value)
{
  gfloat *reset_value =
    gtk_object_get_data (GTK_OBJECT (hbox), "reset_value");

  g_assert (reset_value);
  *reset_value = value;
}

void
z_spinslider_adjustment_changed		(GtkWidget *hbox)
{
  GtkAdjustment *spin_adj, *hscale_adj;

  spin_adj = gtk_object_get_data (GTK_OBJECT (hbox), "spin_adj");
  hscale_adj = gtk_object_get_data (GTK_OBJECT (hbox), "hscale_adj");
  g_assert (spin_adj && hscale_adj);
  hscale_adj->value = spin_adj->value;
  hscale_adj->lower = spin_adj->lower;
  hscale_adj->upper = spin_adj->upper + spin_adj->page_size;
  hscale_adj->step_increment = spin_adj->step_increment;
  hscale_adj->page_increment = spin_adj->page_increment;
  hscale_adj->page_size = spin_adj->page_size;
  gtk_adjustment_changed (spin_adj);
  gtk_adjustment_changed (hscale_adj);
}

static void
on_z_spinslider_hscale_changed		(GtkWidget *widget,
					 GtkWidget *hbox)
{
  GtkAdjustment * spin_adj, * hscale_adj;

  spin_adj = gtk_object_get_data (GTK_OBJECT (hbox), "spin_adj");
  hscale_adj = gtk_object_get_data (GTK_OBJECT (hbox), "hscale_adj");
  g_assert (spin_adj && hscale_adj);
  if (spin_adj->value != hscale_adj->value)
    gtk_adjustment_set_value (spin_adj, hscale_adj->value);
}

static void
on_z_spinslider_spinbutton_changed	(GtkWidget *widget,
					 GtkWidget *hbox)
{
  GtkAdjustment * spin_adj, * hscale_adj;

  spin_adj = gtk_object_get_data (GTK_OBJECT (hbox), "spin_adj");
  hscale_adj = gtk_object_get_data (GTK_OBJECT (hbox), "hscale_adj");
  g_assert (spin_adj && hscale_adj);
  if (spin_adj->value != hscale_adj->value)
    gtk_adjustment_set_value (hscale_adj, spin_adj->value);
}

static void
on_z_spinslider_reset			(GtkWidget *widget,
					 GtkWidget *hbox)
{
  gfloat *reset_value =
    gtk_object_get_data (GTK_OBJECT (hbox), "reset_value");

  g_assert (reset_value);
  z_spinslider_set_value (hbox, *reset_value);
}

GtkWidget *
z_spinslider_new			(GtkAdjustment * spin_adj,
					 GtkAdjustment * hscale_adj,
					 gchar *unit, gfloat reset)
{
  GtkWidget * hbox;
  GtkWidget * hscale;
  GtkWidget * spinbutton;
  GtkWidget * label;
  GtkWidget * button;
  gfloat * reset_value;
  gint digits;

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_object_set_data (GTK_OBJECT (hbox), "spin_adj", spin_adj);

  /* Set decimal digits so that step increments < 1.0 become visible */
  if (spin_adj->step_increment == 0.0
      || (digits = floor(log10(spin_adj->step_increment))) > 0)
    digits = 0;
  /*
  fprintf(stderr, "zss_new %f %f...%f  %f %f  %f  %d\n",
	  spin_adj->value,
	  spin_adj->lower, spin_adj->upper,
	  spin_adj->step_increment, spin_adj->page_increment,
	  spin_adj->page_size, digits);
  */
  spinbutton = gtk_spin_button_new (spin_adj, 1, -digits);
  gtk_widget_show (spinbutton);
  /* I don't see how to set "as much as needed", so hacking this up */
  gtk_widget_set_usize (spinbutton, 80, -1);
  gtk_spin_button_set_update_policy (GTK_SPIN_BUTTON (spinbutton),
				     GTK_UPDATE_IF_VALID);
  gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbutton), TRUE);
  gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinbutton), TRUE);
  gtk_spin_button_set_snap_to_ticks (GTK_SPIN_BUTTON (spinbutton), TRUE);
  gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinbutton),
				   GTK_SHADOW_NONE);
  gtk_box_pack_start(GTK_BOX (hbox), spinbutton, FALSE, FALSE, 0);
  gtk_signal_connect (GTK_OBJECT (spin_adj), "value-changed",
		      GTK_SIGNAL_FUNC (on_z_spinslider_spinbutton_changed), hbox);

  if (unit)
    {
      label = gtk_label_new (unit);
      gtk_widget_show (label);
      gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 3);
    }

  /* Necessary to reach spin_adj->upper with slider */
  if (!hscale_adj)
    hscale_adj = GTK_ADJUSTMENT (gtk_adjustment_new (spin_adj->value,
      spin_adj->lower, spin_adj->upper + spin_adj->page_size,
      spin_adj->step_increment, spin_adj->page_increment,
      spin_adj->page_size));
  gtk_object_set_data (GTK_OBJECT (hbox), "hscale_adj", hscale_adj);
  hscale = gtk_hscale_new (hscale_adj);
  gtk_widget_show (hscale);
  gtk_scale_set_draw_value (GTK_SCALE (hscale), FALSE);
  gtk_scale_set_digits (GTK_SCALE (hscale), -digits);
  gtk_box_pack_start (GTK_BOX (hbox), hscale, TRUE, TRUE, 3);
  gtk_signal_connect (GTK_OBJECT (hscale_adj), "value-changed",
		      GTK_SIGNAL_FUNC (on_z_spinslider_hscale_changed), hbox);

  /*
    button = gtk_button_new ();
    gtk_widget_show (button);
    pixmap =
    gtk_widget_show (pixmap);
    gtk_container_add (GTK_CONTAINER (button), pixmap);
  */
  button = gtk_button_new_with_label (_("Reset"));
  gtk_widget_show (button);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  /* Sigh */
  reset_value = g_malloc (sizeof (gfloat));
  *reset_value = reset;
  gtk_object_set_data_full (GTK_OBJECT (hbox), "reset_value", reset_value,
			    (GtkDestroyNotify) g_free);
  gtk_signal_connect (GTK_OBJECT (button), "pressed",
		      GTK_SIGNAL_FUNC (on_z_spinslider_reset), hbox);

  return hbox;
}

/*
 *  Accelerator helpers
 */

/**
 * z_key_name:
 * @key: 
 * 
 * Similar to gdk_keyval_name(), including key modifiers.
 * 
 * Return value: 
 **/
gchar *
z_key_name				(z_key key)
{
  gchar *name;

  name = gdk_keyval_name(key.key);

  if (!name)
    return NULL;

  return g_strdup_printf("%s%s%s%s",
			 (key.mask & GDK_CONTROL_MASK) ? "Ctrl+" : "",
			 (key.mask & GDK_MOD1_MASK) ? "Alt+" : "",
			 (key.mask & GDK_SHIFT_MASK) ? "Shift+" : "",
			 name);
}

/**
 * z_key_from_name:
 * @name: 
 * 
 * Similar to gdk_keyval_from_name(), including key modifiers.
 * Resist the temptation to create a z_key yourself, the type
 * is only public for speed. Use this function instead.
 * 
 * Return value: 
 * A z_key, representing GDK_VoidSymbol if the name is invalid.
 **/
z_key
z_key_from_name				(gchar *name)
{
  z_key key;

  key.mask = 0;

  for (;;)
    {
      if (g_strncasecmp(name, "Ctrl+", 5) == 0)
	{
	  key.mask |= GDK_CONTROL_MASK;
	  name += 5;
	}
      else if (g_strncasecmp(name, "Alt+", 4) == 0)
	{
	  key.mask |= GDK_MOD1_MASK;
	  name += 4;
	}
      else if (g_strncasecmp(name, "Shift+", 6) == 0)
	{
	  key.mask |= GDK_SHIFT_MASK;
	  name += 6;
	}
      else
	break;
    }

  key.key = gdk_keyval_from_name(name);

  if (key.key == GDK_VoidSymbol)
    key.mask = 0;

  return key;
}

/* Configuration */

/**
 * zconf_create_z_key:
 * @key: 
 * @desc: 
 * @path: 
 * 
 * Similar to other zconf_create_.. functions for z_key types.
 * Give a path like ZCONF_DOMAIN "/foo/bar/accel", this
 * function appends "_key" and "_mask".
 **/
void
zconf_create_z_key			(z_key		key,
					 const gchar * 	desc,
					 const gchar *	path)
{
  gchar *s;

  g_assert(path != NULL);

  s = g_strjoin (NULL, path, "_key", NULL);
  zconf_create_integer ((gint) key.key, desc, s);
  g_free (s);

  if (zconf_error())
    return;

  s = g_strjoin (NULL, path, "_mask", NULL);
  zconf_create_integer ((gint) key.mask, NULL, s);
  g_free (s);
}

/**
 * zconf_set_z_key:
 * @key: 
 * @path: 
 **/
void
zconf_set_z_key				(z_key		key,
					 const gchar *	path)
{
  gchar *s;

  g_assert(path != NULL);

  s = g_strjoin (NULL, path, "_key", NULL);
  zconf_set_integer ((gint) key.key, s);
  g_free (s);

  if (zconf_error())
    return;

  s = g_strjoin (NULL, path, "_mask", NULL);
  zconf_set_integer ((gint) key.mask, s);
  g_free (s);
}

z_key
zconf_get_z_key				(z_key *	keyp,
					 const gchar *	path)
{
  z_key key;
  gchar *s;

  g_assert(path != NULL);

  s = g_strjoin (NULL, path, "_key", NULL);
  zconf_get_integer ((gint *) &key.key, s);
  g_free (s);

  if (!zconf_error())
    {
      s = g_strjoin (NULL, path, "_mask", NULL);
      zconf_get_integer ((gint *) &key.mask, s);
      g_free (s);
    }

  if (zconf_error())
    {
      key.key = GDK_VoidSymbol;
      key.mask = 0;
    }
  else if (keyp)
    *keyp = key;

  return key;
}

/* Key entry dialog */

struct z_key_entry {
  GtkWidget *			ctrl;
  GtkWidget *			alt;
  GtkWidget *			shift;
  GtkWidget *			entry;
  z_key	*			keyp;
  guint				mask;
};

static void
on_mask_toggled				(GtkWidget	*w,
					 gpointer	user_data)
{
  struct z_key_entry *ke = user_data;
  guint mask;

  if (w == ke->ctrl)
    mask = GDK_CONTROL_MASK;
  else if (w == ke->alt)
    mask = GDK_MOD1_MASK;
  else if (w == ke->shift)
    mask = GDK_SHIFT_MASK;
  else
    mask = 0;

  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)))
    ke->mask |= mask;
  else
    ke->mask &= ~mask;

  if (ke->keyp->key != GDK_VoidSymbol)
    ke->keyp->mask = ke->mask;
}

static void
on_keyval_changed			(GtkWidget *	w,
					 gpointer	user_data)
{
  struct z_key_entry *ke = user_data;
  gchar *name = gtk_entry_get_text (GTK_ENTRY (w));

  ke->keyp->key = gdk_keyval_from_name(name);

  if (ke->keyp->key == GDK_VoidSymbol)
    ke->keyp->mask = 0;
  else
    ke->keyp->mask = ke->mask;
}

#include "keysyms.h"

static void
on_key_table_clicked			(GtkWidget *	w,
					 gpointer	user_data)
{
  struct z_key_entry *ke = user_data;
  GtkWidget *dialog = create_widget("choose_key");
  GtkWidget *key_clist = lookup_widget(dialog, "key_clist");
  gchar *name, *tmp[1];
  gint i, selected = -1;

  name = gtk_entry_get_text (GTK_ENTRY (ke->entry));

  /* XXX Can we display a keyboard? */
  /* XXX Can we enum names using GDK? */

  for (i = 0; i < num_keysyms; i++)
    {
      tmp[0] = keysyms[i];
      gtk_clist_append (GTK_CLIST (key_clist), tmp);
      if (name && !strcasecmp (name, keysyms[i]))
	selected = i;
    }

  if (selected >= 0)
    {
      gtk_clist_moveto (GTK_CLIST (key_clist), selected, 0, 0.5, 0.5);
      gtk_clist_select_row (GTK_CLIST (key_clist), selected, 0);
    }

  if (!gnome_dialog_run_and_close (GNOME_DIALOG (dialog)))
    {
      GList *ptr;

      ptr = GTK_CLIST (key_clist)->row_list;
      i = 0;

      /* get first selected row */
      while (ptr)
	{
	  if (GTK_CLIST_ROW (ptr)->state == GTK_STATE_SELECTED)
	    break;

	  ptr = ptr->next;
	  i++;
	}

      if (ptr)
	gtk_entry_set_text (GTK_ENTRY (ke->entry), keysyms[i]);
    }

  gtk_widget_destroy(dialog);
}

/**
 * z_key_entry_new:
 * @keyp: 
 * 
 * Creates a "key entry" widget, initial settings taken from
 * @keyp, all changes stored in @keyp. You can initialize
 * @keyp with zconf_get_z_key() or z_key_from_name().
 * 
 * Return value: 
 * GtkWidget pointer, gtk_destroy as usual.
 **/
GtkWidget *
z_key_entry_new				(z_key *	keyp)
{
  GtkWidget *hbox;
  GtkWidget *button;
  struct z_key_entry *ke;

  g_assert(keyp != NULL);

  ke = g_malloc (sizeof(*ke));
  ke->keyp = keyp;
  ke->mask = (keyp->key == GDK_VoidSymbol) ? 0 : keyp->mask;

  hbox = gtk_hbox_new (FALSE, 0);
  gtk_object_set_data_full (GTK_OBJECT (hbox), "z_key_entry", ke,
			    (GtkDestroyNotify) g_free);

  ke->ctrl = gtk_check_button_new_with_label (("Ctrl"));
  gtk_box_pack_start (GTK_BOX (hbox), ke->ctrl, FALSE, FALSE, 3);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ke->ctrl),
				!!(ke->mask & GDK_CONTROL_MASK));
  gtk_signal_connect (GTK_OBJECT (ke->ctrl), "toggled",
		      GTK_SIGNAL_FUNC (on_mask_toggled), ke);

  ke->alt = gtk_check_button_new_with_label (("Alt"));
  gtk_box_pack_start (GTK_BOX (hbox), ke->alt, FALSE, FALSE, 3);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ke->alt),
				!!(ke->mask & GDK_MOD1_MASK));
  gtk_signal_connect (GTK_OBJECT (ke->alt), "toggled",
		      GTK_SIGNAL_FUNC (on_mask_toggled), ke);

  ke->shift = gtk_check_button_new_with_label (("Shift"));
  gtk_box_pack_start (GTK_BOX (hbox), ke->shift, FALSE, FALSE, 3);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ke->shift),
				!!(ke->mask & GDK_SHIFT_MASK));
  gtk_signal_connect (GTK_OBJECT (ke->shift), "toggled",
		      GTK_SIGNAL_FUNC (on_mask_toggled), ke);

  ke->entry = gtk_entry_new();
  gtk_box_pack_start (GTK_BOX (hbox), ke->entry, TRUE, TRUE, 3);
  gtk_entry_set_text (GTK_ENTRY (ke->entry),
		      gdk_keyval_name(keyp->key) ?: "");
  gtk_signal_connect (GTK_OBJECT (ke->entry), "changed",
		      GTK_SIGNAL_FUNC (on_keyval_changed), ke);

  button = gtk_button_new_with_label (("Key table"));
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 3);
  gtk_signal_connect (GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC (on_key_table_clicked), ke);

  return hbox;
}

/* Accelerators */

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
