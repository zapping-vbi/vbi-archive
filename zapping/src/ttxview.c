/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2000 Iñaki García Etxebarria
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
 * GUI view for the Teletext data
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "interface.h"
#include "ttxview.h"
#include "zvbi.h"
#define ZCONF_DOMAIN "/zapping/ttxview/"
#include "zconf.h"
#include "zmisc.h"
#include "zmodel.h"
#include "../common/fifo.h"

extern gboolean flag_exit_program;

/*
 * BUGS: No alpha yet.
 *       This is getting too big, maybe we should split it up.
 */
static GdkCursor	*hand=NULL;
static GdkCursor	*arrow=NULL;
static GdkCursor	*xterm=NULL;
static GtkWidget	*search_progress=NULL;

typedef struct {
  GdkBitmap		*mask;
  gint			w, h;
  GtkWidget		*da;
  int			id; /* TTX client id */
  guint			timeout; /* id */
  struct fmt_page	*fmt_page; /* current page, formatted */
  gint			page; /* page we are entering */
  gint			subpage; /* current subpage */
  GList			*history_stack; /* for back, etc... */
  gint			history_stack_size; /* items in history_stack */
  gint			history_sp; /* pointer in the stack */
  gint			monitored_subpage;
  gboolean		no_history; /* don't send to history next page */
  gboolean		hold; /* hold the current subpage */
  gint			pop_pgno, pop_subno; /* for popup */
  gboolean		in_link; /* whether the cursor is over a link */
  GtkWidget		*parent; /* toplevel window */
  GtkWidget		*appbar; /* appbar, or NULL */
  GtkWidget		*toolbar; /* toolbar */
  GtkWidget		*parent_toolbar; /* parent toolbar toolbar is in */
  gboolean		popup_menu; /* whether right-click shows a popup */
  GdkGC			*xor_gc; /* graphic context for the xor mask */
  gboolean		selecting; /* TRUE if we are selecting text */
  gint		        ssx, ssy; /* starting positions for the
				     selection */
  gint			osx, osy; /* old positions for the selection */
  gchar			*clipboard_text; /* text that goes to the clipboard */
  gchar			*primary_text; /* text that goes to the
					  primary selection */
} ttxview_data;

struct bookmark {
  gint page;
  gint subpage;
  gchar *description;
};

static GList	*bookmarks=NULL;
static ZModel	*model=NULL; /* for bookmarks */
static GdkAtom	clipboard_atom = GDK_NONE;

static void on_ttxview_search_clicked(GtkButton *, ttxview_data *);

static void
add_bookmark(gint page, gint subpage, const gchar *description)
{
  struct bookmark *entry =
    g_malloc(sizeof(struct bookmark));

  entry->page = page;
  entry->subpage = subpage;
  entry->description = g_strdup(description);

  bookmarks = g_list_append(bookmarks, entry);
  zmodel_changed(model);
}

static void
remove_bookmark(gint index)
{
  GList *node = g_list_nth(bookmarks, index);

  if (!node)
    return;

  g_free(((struct bookmark*)(node->data))->description);
  g_free(node->data);
  bookmarks = g_list_remove(bookmarks, node->data);
  zmodel_changed(model);
}

gboolean
startup_ttxview (void)
{
  gint i=0;
  gchar *buffer, *buffer2;
  gint page, subpage;

  hand = gdk_cursor_new (GDK_HAND2);
  arrow = gdk_cursor_new (GDK_LEFT_PTR);
  xterm = gdk_cursor_new (GDK_XTERM);
  model = ZMODEL(zmodel_new());
  clipboard_atom = gdk_atom_intern("CLIPBOARD", FALSE);

  zcc_char(g_get_home_dir(), "Directory to export pages to",
	   "exportdir");
  zcc_bool(FALSE, "URE matches disregarding case", "ure_casefold");
  zcc_bool(FALSE, "URE search backwards", "ure_backwards");

  while (zconf_get_nth(i, &buffer, ZCONF_DOMAIN "bookmarks"))
    {
      buffer2 = g_strconcat(buffer, "/page", NULL);
      zconf_get_integer(&page, buffer2);
      g_free(buffer2);
      buffer2 = g_strconcat(buffer, "/subpage", NULL);
      zconf_get_integer(&subpage, buffer2);
      g_free(buffer2);
      buffer2 = g_strconcat(buffer, "/description", NULL);
      add_bookmark(page, subpage, zconf_get_string(NULL, buffer2));
      g_free(buffer2);

      g_free(buffer);
      i++;
    }

  return TRUE;
}

void
shutdown_ttxview (void)
{
  gchar *buffer;
  gint i=0;
  GList *p = g_list_first(bookmarks);
  struct bookmark* bookmark;

  gdk_cursor_destroy(hand);
  gdk_cursor_destroy(arrow);
  gdk_cursor_destroy(xterm);

  /* Store the bookmarks in the config */
  zconf_delete(ZCONF_DOMAIN "bookmarks");
  while (p)
    {
      bookmark = (struct bookmark*)p->data;
      buffer = g_strdup_printf(ZCONF_DOMAIN "bookmarks/%d/page", i);
      zconf_create_integer(bookmark->page, "Page", buffer);
      g_free(buffer);
      buffer = g_strdup_printf(ZCONF_DOMAIN "bookmarks/%d/subpage", i);
      zconf_create_integer(bookmark->subpage, "Subpage", buffer);
      g_free(buffer);
      buffer = g_strdup_printf(ZCONF_DOMAIN "bookmarks/%d/description", i);
      zconf_create_string(bookmark->description, "Description", buffer);
      g_free(buffer);
      p=p->next;
      i++;
    }

  while (bookmarks)
    remove_bookmark(0);

  gtk_object_destroy(GTK_OBJECT(model));
}

static
void scale_image			(GtkWidget	*wid,
					 gint		w,
					 gint		h,
					 ttxview_data	*data)
{
  if ((data->w != w) ||
      (data->h != h))
    {
      if (data->mask)
	gdk_bitmap_unref(data->mask);
      data->mask = gdk_pixmap_new(data->da->window, w, h, 1);
      g_assert(data->mask != NULL);
      resize_ttx_page(data->id, w, h);
      data->w = w;
      data->h = h;
    }
}

static int
find_prev_subpage (ttxview_data	*data, int subpage)
{
  struct vbi *vbi = zvbi_get_object();
  int start_subpage = subpage;

  if (!vbi->cache->hi_subno[data->fmt_page->vtp->pgno])
    return -1;

  do {
    subpage = add_bcd(subpage, 0x999);

    if (subpage == start_subpage)
      return -1;
    
    if (subpage < 0)
      subpage = vbi->cache->hi_subno[data->fmt_page->vtp->pgno] - 1;
  } while (!vbi->cache->op->get(vbi->cache, data->fmt_page->vtp->pgno,
				subpage, 0xffff));

  return subpage;
}

static int
find_next_subpage (ttxview_data	*data, int subpage)
{
  struct vbi *vbi = zvbi_get_object();
  int start_subpage = subpage;

  if (!vbi->cache->hi_subno[data->fmt_page->vtp->pgno])
    return -1;

  do {
    subpage = add_bcd(subpage, 0x001);

    if (subpage == start_subpage)
      return -1;
    
    if (subpage >= vbi->cache->hi_subno[data->fmt_page->vtp->pgno])
      subpage = 0;
  } while (!vbi->cache->op->get(vbi->cache, data->fmt_page->vtp->pgno,
				subpage, 0xffff));

  return subpage;
}

static
void setup_history_gui	(ttxview_data *data)
{
  GtkWidget *prev, *next, *prev_subpage, *next_subpage;

  prev = lookup_widget(data->toolbar, "ttxview_history_prev");
  next = lookup_widget(data->toolbar, "ttxview_history_next");
  prev_subpage = lookup_widget(data->toolbar, "ttxview_prev_subpage");
  next_subpage = lookup_widget(data->toolbar, "ttxview_next_subpage");

  if (data->history_stack_size > (data->history_sp+1))
    gtk_widget_set_sensitive(next, TRUE);
  else
    gtk_widget_set_sensitive(next, FALSE);
  
  if (data->history_sp > 0)
    gtk_widget_set_sensitive(prev, TRUE);
  else
    gtk_widget_set_sensitive(prev, FALSE);
}

static
void append_history	(int page, int subpage, ttxview_data *data)
{
  gint page_code, pc_subpage;

  pc_subpage = (subpage == ANY_SUB) ? 0xffff : (subpage & 0xffff);
  page_code = (page<<16)+pc_subpage;

  if ((!data->history_stack) ||
      (GPOINTER_TO_INT(g_list_nth(data->history_stack,
				  data->history_sp)->data) != page_code))
    {
      data->history_stack = g_list_append(data->history_stack,
					  GINT_TO_POINTER(page_code));
      data->history_stack_size++;
      data->history_sp = data->history_stack_size-1;
      setup_history_gui(data);
    }
}

#if 0
/* API flaw */
static
void set_stock_pixmap	(GtkWidget	*button,
			 const gchar	*new_pix)
{
  GtkWidget *widget = GTK_BIN(button)->child;
  GList *node = g_list_first(GTK_BOX(widget)->children)->next;

  widget = GTK_WIDGET(((GtkBoxChild*)(node->data))->widget);

  gnome_stock_set_icon(GNOME_STOCK(widget), new_pix);
}
#endif

static void
remove_ttxview_instance			(ttxview_data	*data)
{
  if (data->mask)
    gdk_bitmap_unref(data->mask);

  gdk_gc_unref(data->xor_gc);

  if (data->clipboard_text)
    {
      g_free(data->clipboard_text);
      if (gdk_selection_owner_get (clipboard_atom) ==
	  data->da->window)
	gtk_selection_owner_set (NULL, clipboard_atom,
				 GDK_CURRENT_TIME);
    }
  if (data->primary_text)
    {
      g_free(data->primary_text);
      if (gdk_selection_owner_get (GDK_SELECTION_PRIMARY) ==
	  data->da->window)
	gtk_selection_owner_set (NULL, GDK_SELECTION_PRIMARY,
				 GDK_CURRENT_TIME);
    }

  unregister_ttx_client(data->id);
  gtk_timeout_remove(data->timeout);
  
  g_free(data);
}

static gboolean
on_ttxview_delete_event			(GtkWidget	*widget,
					 GdkEvent	*event,
					 ttxview_data	*data)
{
  remove_ttxview_instance(data);

  return FALSE;
}

/* Called when another application claims the selection */
static gint selection_clear		(GtkWidget	*widget,
					 GdkEventSelection *event,
					 ttxview_data	*data)
{
  if (event->selection == GDK_SELECTION_PRIMARY)
    {
      g_free(data->primary_text);
      data->primary_text = NULL;
    }
  else if (event->selection == clipboard_atom)
    {
      g_free(data->clipboard_text);
      data->clipboard_text = NULL;
    }
  return TRUE;
}

static void selection_handle		(GtkWidget	*widget,
					 GtkSelectionData *selection_data,
					 guint		info,
					 guint		time_stamp,
					 ttxview_data	*data)
{
  if ((selection_data->selection == GDK_SELECTION_PRIMARY) &&
      (data->primary_text))
    gtk_selection_data_set (selection_data, GDK_SELECTION_TYPE_STRING,
			    8, data->primary_text,
			    strlen(data->primary_text));
  else if ((selection_data->selection == clipboard_atom) &&
	   (data->clipboard_text))
    gtk_selection_data_set (selection_data, GDK_SELECTION_TYPE_STRING,
			    8, data->clipboard_text,
			    strlen(data->clipboard_text));
}

static void
update_pointer (ttxview_data *data)
{
  gint x, y;
  GdkModifierType mask;
  gint w, h, col, row;
  gchar *buffer;
  GtkWidget *widget = data->da;

  gdk_window_get_pointer(widget->window, &x, &y, &mask);

  gdk_window_get_size(widget->window, &w, &h);

  if ((w <= 0) || (h <= 0))
    return;

  /* convert to fmt_page space */
  col = (x*40)/w;
  row = (y*25)/h;

  if ((col < 0) || (col >= 40) || (row < 0) || (row >= 25))
    return;

  if (data->fmt_page->data[row][col].link)
    {
      vbi_link_descr ld;

      vbi_resolve_link(data->fmt_page, col, row, &ld);

      switch (ld.type)
        {
	case VBI_LINK_PAGE:
	  buffer = g_strdup_printf(_(" Page %d"), bcd2dec(ld.pgno));
	  break;

	case VBI_LINK_SUBPAGE:
	  buffer = g_strdup_printf(_(" Subpage %d"), bcd2dec(ld.subno & 0xFF));
	  break;

	case VBI_LINK_HTTP:
	case VBI_LINK_FTP:
	case VBI_LINK_EMAIL:
	  buffer = g_strdup_printf(" %s%s",
				   ld.type == VBI_LINK_EMAIL ?
				   "mailto:" : "", ld.text);
	  break;

        default:
	  goto no_link;
	}
      if (!data->in_link)
	{
	  if (data->appbar)
	    gnome_appbar_push(GNOME_APPBAR(data->appbar), buffer);
	  data->in_link = TRUE;
	}
      else if (data->appbar)
	gnome_appbar_set_status(GNOME_APPBAR(data->appbar), buffer);
      g_free(buffer);
      gdk_window_set_cursor(widget->window, hand);
    }
  else
    {
no_link:
      if (data->in_link)
	{
	  if (data->appbar)
	    gnome_appbar_pop(GNOME_APPBAR(data->appbar));
	  data->in_link = FALSE;
	}
      gdk_window_set_cursor(widget->window, arrow);
    }
}

static gint
event_timeout				(ttxview_data	*data)
{
  enum ttx_message msg;
  gint w, h;
  GtkWidget *widget;
  gchar *buffer;

  while ((msg = peek_ttx_message(data->id)))
    {
      /* discard page received messages while selecting */
      if (data->selecting)
	continue;

      switch (msg)
	{
	case TTX_PAGE_RECEIVED:
	  gdk_window_get_size(data->da->window, &w, &h);
	  gdk_window_clear_area_e(data->da->window, 0, 0, w, h);
	  data->subpage = data->fmt_page->vtp->subno;
	  widget = lookup_widget(data->toolbar, "ttxview_subpage");
	  buffer = g_strdup_printf("S%x", data->subpage);
	  gtk_label_set_text(GTK_LABEL(widget), buffer);
	  if (!data->no_history)
	    append_history(data->fmt_page->vtp->pgno,
			   data->monitored_subpage, data);
	  data->no_history = FALSE;
	  g_free(buffer);
	  if (data->appbar)
	    gnome_appbar_set_status(GNOME_APPBAR(data->appbar), "");
	  if (data->in_link)
	    {
	      if (data->appbar)
		gnome_appbar_pop(GNOME_APPBAR(data->appbar));
	      data->in_link = FALSE;
	    }
	  if ((!data->fmt_page->vtp->pgno) &&
	      (data->appbar))
	    gnome_appbar_set_status(GNOME_APPBAR(data->appbar),
				    _("Warning: Page not valid"));
	  update_pointer(data);
	  break;
	case TTX_BROKEN_PIPE:
	  g_warning("Broken TTX pipe");
	  return FALSE;
	default:
	  g_warning("Unknown message: %d", msg);
	  break;
	}
    }

  return TRUE;
}

static void
load_page (int page, int subpage, ttxview_data *data,
	   struct fmt_page *pg)
{
  GtkWidget *ttxview_url = lookup_widget(data->toolbar, "ttxview_url");
  GtkWidget *ttxview_hold = lookup_widget(data->toolbar, "ttxview_hold");
  GtkWidget *widget;
  gchar *buffer;

  buffer = g_strdup_printf("%d", bcd2dec(page));
  gtk_label_set_text(GTK_LABEL(ttxview_url), buffer);
  g_free(buffer);

  data->hold = (subpage != ANY_SUB)?TRUE:FALSE;
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ttxview_hold),
			       data->hold);
    
  data->subpage = subpage;
  data->monitored_subpage = subpage;
  widget = lookup_widget(data->toolbar, "ttxview_subpage");
  if (subpage != ANY_SUB)
    buffer = g_strdup_printf("S%x", data->subpage);
  else
    buffer = g_strdup("");
  gtk_label_set_text(GTK_LABEL(widget), buffer);
  g_free(buffer);

  if ((page >= 0x100) && (page <= 0x899))
    {
      if (subpage == ANY_SUB)
	buffer = g_strdup_printf(_("Loading page %x..."), page);
      else
	buffer = g_strdup_printf(_("Loading subpage %x..."),
				 subpage);
    }
  else
    buffer = g_strdup_printf(_("Warning: Page not valid"));

  if (data->appbar)
    gnome_appbar_set_status(GNOME_APPBAR(data->appbar), buffer);
  g_free(buffer);

  gtk_widget_grab_focus(data->da);
  
  z_update_gui();

  if (!pg)
    monitor_ttx_page(data->id, page, subpage);
  else
    monitor_ttx_this(data->id, pg);
}

static
void on_ttxview_home_clicked		(GtkButton	*button,
					 ttxview_data	*data)
{
  vbi_link_descr ld;

  vbi_resolve_home(data->fmt_page, &ld);

  if (ld.type == VBI_LINK_PAGE || ld.type == VBI_LINK_SUBPAGE)
    load_page(ld.pgno, ld.subno, data, NULL);
  /* else VBI_LINK_HTTP, "http://zapping.sourceforge.net" :-) */
}

static
void on_ttxview_hold_toggled		(GtkToggleButton *button,
					 ttxview_data	*data)
{
  gboolean hold = gtk_toggle_button_get_active(button);

  if (hold != data->hold)
    {
      data->hold = hold;
      if (hold)
	load_page(data->fmt_page->vtp->pgno, data->fmt_page->vtp->subno,
		  data, NULL);
      else
	load_page(data->fmt_page->vtp->pgno, ANY_SUB, data, NULL);
    }
}

static
void on_ttxview_prev_sp_cache_clicked	(GtkButton	*button,
					 ttxview_data	*data)
{
  struct vbi *vbi = zvbi_get_object();
  int subpage;

  g_assert(vbi != NULL);

  if (!vbi->cache)
    {
      if (data->appbar)
	gnome_appbar_set_status(GNOME_APPBAR(data->appbar),
				_("No cache"));
      return;
    }
  
  if (((subpage = find_prev_subpage(data, data->subpage)) >= 0) &&
      (subpage != data->subpage))
    load_page(data->fmt_page->vtp->pgno, subpage, data, NULL);
  else if (data->appbar)
    gnome_appbar_set_status(GNOME_APPBAR(data->appbar),
			    _("No other subpage in the cache"));
}

static
void on_ttxview_prev_subpage_clicked	(GtkButton	*button,
					 ttxview_data	*data)
{
  gint new_subpage;
  if (data->subpage == 0)
    new_subpage = 0x99;
  else
    new_subpage = add_bcd(data->subpage, 0x99) & 0xFF;
  load_page(data->fmt_page->vtp->pgno, new_subpage, data, NULL);
}

static
void on_ttxview_next_sp_cache_clicked	(GtkButton	*button,
					 ttxview_data	*data)
{
  struct vbi *vbi = zvbi_get_object();
  int subpage;

  g_assert(vbi != NULL);

  if (!vbi->cache)
    {
      if (data->appbar)
	gnome_appbar_set_status(GNOME_APPBAR(data->appbar), _("No cache"));
      return;
    }
  
  if (((subpage = find_next_subpage(data, data->subpage)) >= 0) &&
      (subpage != data->subpage))
    load_page(data->fmt_page->vtp->pgno, subpage, data, NULL);
  else if (data->appbar)
    gnome_appbar_set_status(GNOME_APPBAR(data->appbar),
			    _("No other subpage in the cache"));
}

static
void on_ttxview_next_subpage_clicked	(GtkButton	*button,
					 ttxview_data	*data)
{
  gint new_subpage;
  if (data->subpage >= 0x99)
    new_subpage = 0x00;
  else
    new_subpage = add_bcd(data->subpage, 0x01);
  load_page(data->fmt_page->vtp->pgno, new_subpage, data, NULL);
}

static
void on_search_progress_destroy		(GtkObject	*widget,
					 gpointer	context)
{
  gpointer running = gtk_object_get_user_data(widget);

  search_progress = NULL;

  if (!running)
    vbi_delete_search(context);
}

static
void run_next				(GtkButton	*button,
					 gpointer	context,
					 gint           dir)
{
  gint return_code;
  struct fmt_page *pg;
  ttxview_data *data =
    (ttxview_data *)gtk_object_get_data(GTK_OBJECT(button),
					"ttxview_data");
  GtkWidget *search_cancel = lookup_widget(GTK_WIDGET(button),
					   "button19");
  GtkWidget *search_next = lookup_widget(GTK_WIDGET(button),
					   "button21");
  GtkWidget *search_prev = lookup_widget(GTK_WIDGET(button),
					   "button22");

  gtk_widget_set_sensitive(search_next, FALSE);
  gtk_widget_set_sensitive(search_prev, FALSE);
  gtk_widget_set_sensitive(search_cancel, TRUE);
  gtk_widget_set_sensitive(lookup_widget(search_cancel,
					 "progressbar2"), TRUE);
  gtk_label_set_text(GTK_LABEL(lookup_widget(search_cancel, "label97")),
		     "");
  gnome_dialog_set_default(GNOME_DIALOG(search_progress), 0);
  gtk_object_set_user_data(GTK_OBJECT(search_progress),
			   (gpointer)0xdeadbeef);

  switch ((return_code = vbi_next_search(context, &pg, dir)))
    {
    case 1: /* found, show the page, enable next */
      load_page(pg->vtp->pgno, pg->vtp->subno, data, pg);
      if (search_progress)
	{
	  gtk_widget_set_sensitive(search_cancel, FALSE);
	  gtk_widget_set_sensitive(search_next, TRUE);
	  gtk_widget_set_sensitive(search_prev, TRUE);
	  if (zcg_bool(NULL, "ure_backwards"))
	    gnome_dialog_set_default(GNOME_DIALOG(search_progress),
				     2);
	  else
	    gnome_dialog_set_default(GNOME_DIALOG(search_progress),
				     1);
	  gtk_label_set_text(GTK_LABEL(lookup_widget(search_cancel,
						     "label97")),
			     _("Found"));
	  gtk_widget_set_sensitive(lookup_widget(search_cancel,
						 "progressbar2"), FALSE);
	}
      break;
    case 0: /* not found */
      if (search_progress)
	{
	  gtk_widget_set_sensitive(search_cancel, FALSE);
	  gtk_widget_set_sensitive(search_next, TRUE);
	  gtk_widget_set_sensitive(search_prev, TRUE);
	  if (zcg_bool(NULL, "ure_backwards"))
	    gnome_dialog_set_default(GNOME_DIALOG(search_progress),
				     2);
	  else
	    gnome_dialog_set_default(GNOME_DIALOG(search_progress),
				     1);
	  gtk_label_set_text(GTK_LABEL(lookup_widget(search_cancel,
						     "label97")),
			     _("Not found"));
	  gtk_widget_set_sensitive(lookup_widget(search_cancel,
						 "progressbar2"), FALSE);
	}
      break;      
    case -1: /* cancelled */
      break;
    case -2: /* error */
      break;
    default:
      g_message("Unknown search return code: %d",
		return_code);
      break;
    }

  if (search_progress)
    gtk_object_set_user_data(GTK_OBJECT(search_progress), NULL);
  
  if (return_code < 0)
    {
      if (search_progress)
	gtk_widget_destroy(search_progress);

      vbi_delete_search(context);
    }
}

static
void on_new_search_clicked		(GtkWidget	*button,
					 ttxview_data	*data)
{
  gtk_signal_emit_stop_by_name(GTK_OBJECT(button), "clicked");

  gtk_widget_destroy(lookup_widget(button, "search_progress"));

  on_ttxview_search_clicked(GTK_BUTTON(lookup_widget(data->toolbar,
				       "ttxview_search")), data);
}

static
void on_search_progress_next		(GtkButton	*button,
					 gpointer	context)
{
  gtk_signal_emit_stop_by_name(GTK_OBJECT(button), "clicked");
  
  zcs_bool(FALSE, "ure_backwards");

  run_next(button, context, +1);
}

static
void on_search_progress_prev		(GtkButton	*button,
					 gpointer	context)
{
  gtk_signal_emit_stop_by_name(GTK_OBJECT(button), "clicked");

  zcs_bool(TRUE, "ure_backwards");

  run_next(button, context, -1);
}

static
void show_search_help			(GtkButton	*button,
					 ttxview_data	*data)
{
  GnomeHelpMenuEntry help_ref = { NULL,
				  "ure.html" };

  help_ref.name = gnome_app_id;
  gnome_help_display(NULL, &help_ref);

  /* do not propagate this signal to the parent widget */
  gtk_signal_emit_stop_by_name(GTK_OBJECT(button), "clicked");
}

/*
  Substitute the special search keywords by the appropiate regex,
  returns a newly allocated string, and g_free's the given string.
  Valid search keywords:
  #url#  -> Expands to "https?://([:alnum:]|[-~./?%_=+])+"
  #email# -> Expands to "([:alnum:]|[-~.])+@([:alnum:]|[-~.])+"
*/
static
gchar *subtitute_search_keywords	(gchar		*string)
{
  gint i;
  gchar *found;
  gchar *search_keys[][2] = {
    {"#email#", "([:alnum:]|[-~.])+@([:alnum:]|[-~.])+"},
    {"#url#", "https?://([:alnum:]|[-~./?%_=+])+"}
  };

  if ((!string) || (!*string))
    {
      g_free(string);
      return g_strdup("");
    }

  for (i=0; i<2; i++)
     while ((found = strstr(string, search_keys[i][0])))
     {
       gchar *p;

       *found = 0;
       
       p = g_strconcat(string, search_keys[i][1],
		       found+strlen(search_keys[i][0]), NULL);
       g_free(string);
       string = p;
     }

  return string;
}

static
int progress_update			(struct fmt_page *pg)
{
  gchar *buffer;
  GtkProgress *progress;

  if (search_progress)
    {
      buffer = g_strdup_printf(_("Scanning %x.%02x"), pg->vtp->pgno,
			       pg->vtp->subno);
      gtk_label_set_text(GTK_LABEL(lookup_widget(search_progress, "label97")),
			 buffer);
      g_free(buffer);
      progress =
	GTK_PROGRESS(lookup_widget(search_progress, "progressbar2"));
      gtk_progress_set_value(progress, 1-gtk_progress_get_value(progress));
    }
  else
    return FALSE;

  z_update_gui();
    
  if (flag_exit_program)
    return FALSE;
  else
    return TRUE;
}

static
void on_ttxview_search_clicked		(GtkButton	*button,
					 ttxview_data	*data)
{
  GnomeDialog *ure_search = GNOME_DIALOG(create_widget("ure_search"));
  GtkWidget *entry2 = gnome_entry_gtk_entry(GNOME_ENTRY(
    lookup_widget(GTK_WIDGET(ure_search), "entry2")));
  GtkWidget * button23;
  GtkToggleButton *checkbutton9 =
    GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(ure_search),
				    "checkbutton9"));
  GtkToggleButton *checkbutton10 =
    GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(ure_search),
				    "checkbutton10"));
  GtkWidget *search_progress_next;
  GtkWidget *search_progress_prev;
  gboolean result;
  gchar *needle;
  void *search_context;
  ucs2_t *pattern;

  gnome_dialog_set_parent(ure_search, GTK_WINDOW(data->parent));
  gnome_dialog_close_hides(ure_search, TRUE);
  gnome_dialog_set_default(ure_search, 0);
  gnome_dialog_editable_enters(ure_search, GTK_EDITABLE(entry2));
  gnome_dialog_button_connect(ure_search, 2,
			      GTK_SIGNAL_FUNC(show_search_help), data);

  gtk_widget_grab_focus(entry2);

  gtk_toggle_button_set_active(checkbutton9,
			       zcg_bool(NULL, "ure_casefold"));
  gtk_toggle_button_set_active(checkbutton10,
			       zcg_bool(NULL, "ure_backwards"));

  result = gnome_dialog_run_and_close(ure_search);
  needle = gtk_entry_get_text(GTK_ENTRY(entry2));
  if (needle)
    needle = g_strdup(needle);
  
  zcs_bool(gtk_toggle_button_get_active(checkbutton9), "ure_casefold");
  zcs_bool(gtk_toggle_button_get_active(checkbutton10), "ure_backwards");
  gtk_widget_destroy(GTK_WIDGET(ure_search));

  if ((!result) && (needle))
    {
      needle = subtitute_search_keywords(needle);
      pattern = local2ucs2(needle);
      g_free(needle);
      if (pattern)
	{
	  search_context =
	    vbi_new_search(zvbi_get_object(),
			   0x100, ANY_SUB, pattern,
			   zcg_bool(NULL, "ure_casefold"),
			   progress_update);
	  free(pattern);
	  if (search_context)
	    {
	      if (search_progress)
		gtk_widget_destroy(search_progress);
	      search_progress = create_widget("search_progress");
	      gtk_window_set_modal(GTK_WINDOW(search_progress), TRUE);
	      gnome_dialog_set_parent(GNOME_DIALOG(search_progress),
				      GTK_WINDOW(data->parent));
	      gtk_signal_connect(GTK_OBJECT(search_progress), "destroy",
				 GTK_SIGNAL_FUNC(on_search_progress_destroy),
				 search_context);
	      search_progress_next = lookup_widget(search_progress,
						   "button21");
	      gtk_signal_connect(GTK_OBJECT(search_progress_next), "clicked",
				 GTK_SIGNAL_FUNC(on_search_progress_next),
				 search_context);
	      gtk_object_set_data(GTK_OBJECT(search_progress_next),
				  "ttxview_data", data);
	      gtk_widget_set_sensitive(search_progress_next, FALSE);
	      search_progress_prev = lookup_widget(search_progress,
						   "button22");
	      gtk_signal_connect(GTK_OBJECT(search_progress_prev), "clicked",
				 GTK_SIGNAL_FUNC(on_search_progress_prev),
				 search_context);
	      gtk_object_set_data(GTK_OBJECT(search_progress_prev),
				  "ttxview_data", data);
	      gtk_widget_set_sensitive(search_progress_prev, FALSE);
	      button23 =
		lookup_widget(GTK_WIDGET(search_progress), "button23");
	      gtk_signal_connect(GTK_OBJECT(button23), "clicked",
				 GTK_SIGNAL_FUNC(on_new_search_clicked),
				 data);
	      gtk_widget_show(search_progress);
	      run_next(GTK_BUTTON(search_progress_next), search_context,
		zcg_bool(NULL, "ure_backwards") ? -1 : +1);
	    }
	}
    }
}

static
void on_ttxview_history_prev_clicked	(GtkButton	*button,
					 ttxview_data	*data)
{
  gint page, page_code, pc_subpage;

  if (data->history_sp == 0)
    return;

  data->history_sp--;

  page_code = GPOINTER_TO_INT(g_list_nth(data->history_stack,
					 data->history_sp)->data);
  page = page_code >> 16;
  pc_subpage = (page_code & 0xffff);
  pc_subpage = (pc_subpage == 0xffff) ? ANY_SUB : pc_subpage;

  data->no_history = TRUE;
  load_page(page, pc_subpage, data, NULL);
  setup_history_gui(data);
}

static
void on_ttxview_history_next_clicked	(GtkButton	*button,
					 ttxview_data	*data)
{
  gint page, page_code, pc_subpage;

  if (data->history_stack_size == (data->history_sp+1))
    return;
  data->history_sp++;

  page_code = GPOINTER_TO_INT(g_list_nth(data->history_stack,
					 data->history_sp)->data);
  page = page_code >> 16;
  pc_subpage = (page_code & 0xffff);
  pc_subpage = (pc_subpage == 0xffff) ? ANY_SUB : pc_subpage;

  data->no_history = TRUE;
  load_page(page, pc_subpage, data, NULL);
  setup_history_gui(data);
}

static
void on_ttxview_clone_clicked		(GtkButton	*button,
					 ttxview_data	*data)
{
  GtkWidget *dolly = build_ttxview();
  gint w, h;

  if (data->fmt_page->vtp->pgno)
    load_page(data->fmt_page->vtp->pgno, data->monitored_subpage,
	      (ttxview_data*)gtk_object_get_data(GTK_OBJECT(dolly),
						 "ttxview_data"),
	      NULL);
  else
    load_page(0x100, ANY_SUB,
	      (ttxview_data*)gtk_object_get_data(GTK_OBJECT(dolly),
						 "ttxview_data"),
	      NULL);
  gdk_window_get_size(data->parent->window, &w, &h);
  gtk_widget_realize(dolly);
  z_update_gui();
  gdk_window_resize(dolly->window, w, h);
  gtk_widget_show(dolly);
}

static
void on_ttxview_size_allocate		(GtkWidget	*widget,
					 GtkAllocation	*allocation,
					 ttxview_data	*data)
{
  scale_image(widget, allocation->width, allocation->height, data);
}

static
void popup_new_win			(GtkWidget	*widget,
					 ttxview_data	*data)
{
  GtkWidget *dolly = build_ttxview();
  load_page(data->pop_pgno, data->pop_subno,
	    (ttxview_data*)gtk_object_get_data(GTK_OBJECT(dolly),
					       "ttxview_data"),
	    NULL);
  gtk_widget_show(dolly);
}

static
void new_bookmark			(GtkWidget	*widget,
					 ttxview_data	*data)
{
  struct vbi *vbi = zvbi_get_object();
  gchar *default_description;
  gchar title[41];
  gchar *buffer;
  gint page, subpage;

  if (data->page >= 0x100)
    page = data->page;
  else
    page = data->fmt_page->vtp->pgno;
  subpage = data->monitored_subpage;

  if (vbi_page_title(vbi, page, subpage, title))
    {
      if (subpage != ANY_SUB)
        default_description =
          g_strdup_printf("%x.%x %s", page, subpage, title);
      else
        default_description = g_strdup_printf("%x %s", page, title);
    }
  else
    {
      if (subpage != ANY_SUB)
        default_description =
          g_strdup_printf("%x.%x", page, subpage);
      else
        default_description = g_strdup_printf("%x", page);
    }

  buffer = Prompt(data->parent,
		  _("New bookmark"),
		  _("Description:"),
		  default_description);
  if (buffer)
    {
      add_bookmark(page, subpage, buffer);
      default_description =
	g_strdup_printf(_("<%s> added to the bookmarks"), buffer);
      if (data->appbar)
	gnome_appbar_set_status(GNOME_APPBAR(data->appbar),
				default_description);
      g_free(default_description);
    }
  g_free(buffer);
}

static
void on_be_close			(GtkWidget	*widget,
					 ttxview_data	*data)
{
  gtk_widget_destroy(lookup_widget(widget, "bookmarks_editor"));
}

static
void on_be_delete			(GtkWidget	*widget,
					 ttxview_data	*data)
{
  GtkCList *clist = GTK_CLIST(lookup_widget(widget, "bookmarks_clist"));
  GList *rows = g_list_first(clist->row_list);
  gint *deleted=NULL;
  gint i=0, j=0, n;

  if (!clist->selection)
    return;

  n = g_list_length(clist->selection);
  deleted = g_malloc(sizeof(gint)*n);
  
  while (rows)
    {
      if (GTK_CLIST_ROW(rows)->state == GTK_STATE_SELECTED)
	deleted[j++] = i;

      rows = rows->next;
      i ++;
    }
  
  for (i=0; i<n; i++)
    remove_bookmark(deleted[i]-i);
  
  g_free(deleted);
}

static
void on_be_model_changed		(GtkObject	*model,
					 GtkWidget	*view)
{
  GtkCList *clist = GTK_CLIST(lookup_widget(view, "bookmarks_clist"));
  GList *p = g_list_first(bookmarks);
  struct bookmark *bookmark;
  gchar *buffer[3];

  gtk_clist_freeze(clist);
  gtk_clist_clear(clist);
  while (p)
    {
      bookmark = (struct bookmark*)p->data;
      buffer[0] = g_strdup_printf("%x", bookmark->page);
      if (bookmark->subpage == ANY_SUB)
	buffer[1] = g_strdup(_("Any subpage"));
      else
	buffer[1] = g_strdup_printf("%x", bookmark->subpage);
      buffer[2] = bookmark->description;
      gtk_clist_append(clist, buffer);
      g_free(buffer[0]);
      g_free(buffer[1]);
      p = p->next;
    }
  gtk_clist_thaw(clist);
}

static
void on_be_destroy			(GtkObject	*widget,
					 ttxview_data	*data)
{
  gtk_signal_disconnect_by_func(GTK_OBJECT(model),
				GTK_SIGNAL_FUNC(on_be_model_changed),
				GTK_WIDGET(widget));
}

static
void on_delete_bookmarks_activated	(GtkWidget	*widget,
					 ttxview_data	*data)
{
  GtkWidget *be = create_widget("bookmarks_editor");
  gtk_signal_connect(GTK_OBJECT(lookup_widget(be, "bookmarks_close")),
		     "clicked",
		     GTK_SIGNAL_FUNC(on_be_close), data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(be, "bookmarks_remove")),
		     "clicked",
		     GTK_SIGNAL_FUNC(on_be_delete), data);
  gtk_signal_connect(GTK_OBJECT(model), "changed",
		     GTK_SIGNAL_FUNC(on_be_model_changed), be);
  gtk_signal_connect(GTK_OBJECT(be), "destroy",
		     GTK_SIGNAL_FUNC(on_be_destroy), data);
  on_be_model_changed(GTK_OBJECT(model), be);
  gtk_widget_show(be);
}

static
void export_ttx_page			(GtkWidget	*widget,
					 ttxview_data	*data,
					 char		*fmt)
{
  struct export *exp;
  gchar *buffer, *buffer2;
  char *filename;

  buffer = zcg_char(NULL, "exportdir");

  if ((!buffer) ||
      (!strlen(buffer)))
    {
      if (data->appbar)
	gnome_appbar_set_status(GNOME_APPBAR(data->appbar),
				_("You must first set the destination dir"
				  " in the properties dialog"));
      return;
    }

  if (data->fmt_page->vtp->pgno < 0x100)
    {
      if (data->appbar)
	gnome_appbar_set_status(GNOME_APPBAR(data->appbar),
				_("No page loaded"));
      return;
    }

  if ((exp = export_open(fmt)))
    {
      /* Configure */
      gchar *prompt;
      if (exp->mod->options)
	buffer = g_strjoinv(",", exp->mod->options);
      else
	buffer = g_strdup(_("[none]"));
      prompt = g_strdup_printf(_("Global options: %s\n"
				 "Options for %s filter: %s"),
			       "reveal,hide",
			       fmt,
			       buffer);
      buffer2 = g_strconcat(ZCONF_DOMAIN, fmt, "_options", NULL);
      zconf_create_string("", "export_options", buffer2);
      g_free(buffer);
      buffer = Prompt(data->parent, _("Export options"),
		      prompt, zconf_get_string(NULL, buffer2));
      if (buffer)
	{
	  zconf_set_string(buffer, buffer2);
	  g_free(buffer2);
	  export_close(exp);
	  buffer2 = g_strconcat(fmt, ",", buffer, NULL);
	  if (!(exp = export_open(buffer2)))
	    {
	      ShowBox("Options not valid, using defaults",
		      GNOME_MESSAGE_BOX_WARNING);
	      g_assert((exp = export_open(fmt)));
	    }
	}
      g_free(buffer2);
      g_free(buffer);
      g_free(prompt);

      filename =
	export_mkname(exp, "Zapzilla-%p.%e",
		      data->fmt_page->vtp, NULL);
      zcg_char(&buffer, "exportdir");
      g_strstrip(buffer);
      if (buffer[strlen(buffer)-1] != '/')
	buffer2 = g_strconcat(buffer, "/", NULL);
      else
	buffer2 = g_strdup(buffer);
      g_free(buffer);
      zcs_char(buffer2, "exportdir");
      buffer = g_strconcat(buffer2, filename, NULL);
      g_free(buffer2);
      if (export(exp, data->fmt_page->vtp, buffer))
	{
	  buffer2 = g_strdup_printf("Export to %s failed: %s", buffer,
				    export_errstr());
	  g_warning(buffer2);
	  if (data->appbar)
	    gnome_appbar_set_status(GNOME_APPBAR(data->appbar), buffer2);
	  g_free(buffer2);
	}
      else
	{
	  buffer2 = g_strdup_printf(_("%s saved"), buffer);
	  if (data->appbar)
	    gnome_appbar_set_status(GNOME_APPBAR(data->appbar), buffer2);
	  g_free(buffer2);
	}
      free(filename);
      g_free(buffer);
      export_close(exp);
    }
  else
    {
      buffer = g_strdup_printf("Cannot create export struct: %s",
			       export_errstr());
      g_warning(buffer);
      if (data->appbar)
	gnome_appbar_set_status(GNOME_APPBAR(data->appbar), buffer);
      g_free(buffer);
    }
}

static
void export_html			(GtkWidget	*widget,
					 ttxview_data	*data)
{
  export_ttx_page(widget, data, "html");
}

static
void export_ppm				(GtkWidget	*widget,
					 ttxview_data	*data)
{
  export_ttx_page(widget, data, "ppm");
}

#ifdef HAVE_LIBPNG
static
void export_png				(GtkWidget	*widget,
					 ttxview_data	*data)
{
  export_ttx_page(widget, data, "png");
}
#endif /* HAVE_LIBPNG */

static
void export_ascii			(GtkWidget	*widget,
					 ttxview_data	*data)
{
  export_ttx_page(widget, data, "ascii");
}

static
void export_ansi			(GtkWidget	*widget,
					 ttxview_data	*data)
{
  export_ttx_page(widget, data, "ansi");
}

static
void on_bookmark_activated		(GtkWidget	*widget,
					 ttxview_data	*data)
{
  struct bookmark *bookmark = (struct bookmark*)
    gtk_object_get_user_data(GTK_OBJECT(widget));

  load_page(bookmark->page, bookmark->subpage, data, NULL);
}

static
GtkWidget *build_ttxview_popup (ttxview_data *data, gint page, gint subpage)
{
  GtkWidget *popup = create_ttxview_popup();
  GList *p = g_list_first(bookmarks);
  struct bookmark *bookmark;
  GtkWidget *menuitem;
  gchar *buffer;

  /* convert to fmt_page space */
  data->pop_pgno = page;
  data->pop_subno = subpage;

  gtk_widget_realize(popup);

  if (!page)
    {
      gtk_widget_hide(lookup_widget(popup, "open_in_new_window1"));
      gtk_widget_hide(lookup_widget(popup, "separator8"));
    }
  else
    gtk_signal_connect(GTK_OBJECT(lookup_widget(popup,
						"open_in_new_window1")),
		       "activate", GTK_SIGNAL_FUNC(popup_new_win), data);

  gtk_signal_connect(GTK_OBJECT(lookup_widget(popup, "search1")),
		     "activate",
		     GTK_SIGNAL_FUNC(on_ttxview_search_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(popup, "html1")),
		     "activate",
		     GTK_SIGNAL_FUNC(export_html), data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(popup, "ppm1")),
		     "activate",
		     GTK_SIGNAL_FUNC(export_ppm), data);
#ifdef HAVE_LIBPNG
  gtk_signal_connect(GTK_OBJECT(lookup_widget(popup, "png1")),
		     "activate",
		     GTK_SIGNAL_FUNC(export_png), data);
#else
  gtk_widget_hide(lookup_widget(popup, "png1"));
#endif
  gtk_signal_connect(GTK_OBJECT(lookup_widget(popup, "ascii1")),
		     "activate",
		     GTK_SIGNAL_FUNC(export_ascii), data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(popup, "ansi1")),
		     "activate",
		     GTK_SIGNAL_FUNC(export_ansi), data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(popup, "add_bookmark")),
		     "activate",
		     GTK_SIGNAL_FUNC(new_bookmark), data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(popup, "delete_bookmarks")),
		     "activate",
		     GTK_SIGNAL_FUNC(on_delete_bookmarks_activated),
		     data);

  /* Bookmark entries */
  if (!p)
    gtk_widget_hide(lookup_widget(popup, "separator9"));
  else
    while (p)
      {
	bookmark = (struct bookmark*)p->data;
	menuitem = z_gtk_pixmap_menu_item_new(bookmark->description,
					      GNOME_STOCK_PIXMAP_JUMP_TO);
	if (bookmark->subpage != ANY_SUB)
	  buffer = g_strdup_printf("%x.%x", bookmark->page, bookmark->subpage);
	else
	  buffer = g_strdup_printf("%x", bookmark->page);
	set_tooltip(menuitem, buffer);
	g_free(buffer);
	gtk_object_set_user_data(GTK_OBJECT(menuitem), bookmark);
	gtk_widget_show(menuitem);
	gtk_menu_append(GTK_MENU(popup), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(on_bookmark_activated),
			   data);
	p = p->next;
      }

  return popup;
}

void
process_ttxview_menu_popup		(GtkWidget	*widget,
					 GdkEventButton	*event,
					 GtkMenu	*popup)
{
  gint w, h, col, row;
  vbi_link_descr ld;
  GtkMenu *menu;
  ttxview_data *data = gtk_object_get_data(GTK_OBJECT(widget),
					   "ttxview_data");

  GtkWidget *menu_item;

  if (!data)
    return;

  gdk_window_get_size(widget->window, &w, &h);
  /* convert to fmt_page space */
  col = (event->x*40)/w;
  row = (event->y*25)/h;

  ld.pgno = ld.subno = 0;
  if (data->fmt_page->data[row][col].link)
    {
      vbi_resolve_link(data->fmt_page, col, row, &ld);

      if (ld.type != VBI_LINK_PAGE &&
          ld.type != VBI_LINK_SUBPAGE)
        ld.pgno = ld.subno = 0;
    }

  menu = GTK_MENU(build_ttxview_popup(data, ld.pgno, ld.subno));

  menu_item =
    z_gtk_pixmap_menu_item_new("Zapzilla", GNOME_STOCK_PIXMAP_ALIGN_JUSTIFY);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), GTK_WIDGET(menu));
  gtk_widget_show(GTK_WIDGET(menu));
  gtk_widget_show(menu_item);
  gtk_menu_insert(GTK_MENU(popup), menu_item, 1);
  menu_item = gtk_menu_item_new();
  gtk_widget_show(menu_item);
  gtk_menu_insert(GTK_MENU(popup), menu_item, 2);
}

/*
 * Inverts the color of the given region.
 * The region is given in to 40x25 coordinates
 * The complexity is needed to avoid integer division roundof errors.
 */
static void
select_region				(gint		x1,
					 gint		y1,
					 gint		x2,
					 gint		y2,
					 ttxview_data	*data)
{
  gint w, h; /* scaling factors */
  gint temp;

  gdk_window_get_size(data->da->window, &w, &h);

  if (y1 > y2)
    {
      temp = x1;
      x1 = x2;
      x2 = temp;
      temp = y1;
      y1 = y2;
      y2 = temp;
    }

  if (x1 > x2)
    {
      temp = x1;
      x1 = x2;
      x2 = temp;
    }

  /* transform into pixel coordinates */
  x1 = (x1*w)/40;
  y1 = (y1*h)/25;
  x2 = ((x2+1)*w)/40;
  y2 = ((y2+1)*h)/25;

  gdk_draw_rectangle(data->da->window, data->xor_gc, TRUE,
		     x1, y1, x2-x1, y2-y1);
}

static void select_start(gint x, gint y, ttxview_data * data)
{
  if (data->fmt_page->vtp->pgno < 0x100)
    {
      if (data->appbar)
  	gnome_appbar_set_status(GNOME_APPBAR(data->appbar),
  				_("No page loaded"));
      return;
    }

  if (data->selecting)
    return;

  if (data->appbar)
    gnome_appbar_push(GNOME_APPBAR(data->appbar), _("Selecting"));

  gdk_window_set_cursor(data->da->window, xterm);

  data->ssx = x;
  data->ssy = y;
  data->osx = -1; /* Selection not started yet, wait to move event */
  data->selecting = TRUE;
  ttx_freeze(data->id);
}

#define hidden_row(X) real_hidden_row(X, data)
static int real_hidden_row(int row, ttxview_data * data)
{
  if ((row <= 0) || (row >= 25))
    return FALSE;

  return (data->fmt_page->double_height_lower & (1<<row));
}

static void select_stop(ttxview_data * data)
{
  gint w, h;
  gint scol, srow, col, row, temp;
  struct export *e;
  gchar *buffer;
  char *exported;

  if (!data->selecting)
    return;

  if (data->appbar)
    gnome_appbar_pop(GNOME_APPBAR(data->appbar));

  if (data->osx != -1)
    {
      gdk_window_get_size(data->da->window, &w, &h);

      scol = (data->ssx*40)/w;
      srow = (data->ssy*25)/h;
      col = (data->osx*40)/w;
      row = (data->osy*25)/h;

      if (row > srow)
	{
	  if (hidden_row(srow))
	    srow--;
	  if (hidden_row(row+1))
	    row++;
	}
      else if (row < srow)
	{
	  if (hidden_row(srow+1))
	    srow++;
	  if (hidden_row(row))
	    row--;
	}
      else /* row == srow */
	{
	  if (hidden_row(row))
	    srow--;
	  else if (hidden_row(row+1))
	    row++;
	}

      if (col > 39) col = 39;
      if (col < 0) col = 0;
      if (row > 24) row = 24;
      if (row < 0) row = 0;
      if (scol > 39) scol = 39;
      if (scol < 0) scol = 0;
      if (srow > 24) srow = 24;
      if (srow < 0) srow = 0;

      if (srow > row)
	{
	  temp = srow;
	  srow = row;
	  row = temp;
	}
      if (scol > col)
	{
	  temp = scol;
	  scol = col;
	  col = temp;
	}

      select_region(scol, srow, col, row, data);
      
      buffer =
	g_strdup_printf("string,col=%d,row=%d,width=%d,height=%d",
			scol, srow, (col-scol)+1, (row-srow)+1);

      if ((e = export_open(buffer)))
	{
	  if ((exported = (char*)export(e, data->fmt_page->vtp, "memory")))
	    {
	      if (data->clipboard_text)
		{
		  g_free(data->clipboard_text);
		  data->clipboard_text = g_strdup(exported);
		}
	      else
		{
		  if (gtk_selection_owner_set(data->da,
					      clipboard_atom,
					      GDK_CURRENT_TIME))
		    data->clipboard_text = g_strdup(exported);
		}

	      if (data->primary_text)
		{
		  g_free(data->primary_text);
		  data->primary_text = g_strdup(exported);
		}
	      else
		{
		  if (gtk_selection_owner_set(data->da,
					      GDK_SELECTION_PRIMARY,
					      GDK_CURRENT_TIME))
		    data->primary_text = g_strdup(exported);
		}

	      if (data->appbar)
		gnome_appbar_set_status(GNOME_APPBAR(data->appbar),
					_("Selection copied to clipboard"));
	      free(exported);
	    }
	  else
	    {
	      g_warning("couldn't export data to memory");
	      
	      if (data->appbar)
		gnome_appbar_set_status(GNOME_APPBAR(data->appbar),
					"Error exporting to clipboard"
					" (couldn't export data to memory)");
	    }
	  export_close(e);
	}
      else
	{
	  g_warning("couldn't create export structure");

	  if (data->appbar)
	    gnome_appbar_set_status(GNOME_APPBAR(data->appbar),
				    "Error exporting to clipboard"
				    " (couldn't create export structure)");
	}

      g_free(buffer);
    }

  data->selecting = FALSE;
  ttx_unfreeze(data->id);

  update_pointer(data);
}

/**
 * Returns a freshly allocated region exactly like rect.
 */
static GdkRegion * region_from_rect(GdkRectangle *rect)
{
  GdkRegion *region = gdk_region_new();
  GdkRegion *result = NULL;

  result = gdk_region_union_with_rect(region, rect);
  g_free(region);

  return result;
}

/**
 * transform the selected region from the first rectangle to the
 * second one. Coordinates are in 40x25 space. No pixel is drawn more
 * than once.
 */
static void transform_region(gint sx1, gint sy1, gint sx2, gint sy2,
			     gint dx1, gint dy1, gint dx2, gint dy2,
			     ttxview_data * data)
{
  gint w, h;
  gint temp;
  /* tv_screen coordinates */
  gint nsx1, nsy1, nsx2, nsy2, ndx1, ndy1, ndx2, ndy2;
  GdkRectangle rect;
  GdkRegion *window, *temp_region;
  GdkRegion *clip_region;

  if (sy1 > sy2)
    {
      temp = sx1;
      sx1 = sx2;
      sx2 = temp;
      temp = sy1;
      sy1 = sy2;
      sy2 = temp;
    }
  if (sx1 > sx2)
    {
      temp = sx1;
      sx1 = sx2;
      sx2 = temp;
    }

  if (dy1 > dy2)
    {
      temp = dx1;
      dx1 = dx2;
      dx2 = temp;
      temp = dy1;
      dy1 = dy2;
      dy2 = temp;
    }
  if (dx1 > dx2)
    {
      temp = dx1;
      dx1 = dx2;
      dx2 = temp;
    }

  gdk_window_get_size(data->da->window, &w, &h);

#define TTX2DA_X(X, Y) Y = ((X)*w)/40
  TTX2DA_X(sx1, nsx1);
  TTX2DA_X(sx2+1, nsx2)-1;
  TTX2DA_X(dx1, ndx1);
  TTX2DA_X(dx2+1, ndx2)-1;

#define TTX2DA_Y(X, Y) Y = ((X)*h)/25
  TTX2DA_Y(sy1, nsy1);
  TTX2DA_Y(sy2+1, nsy2)-1;
  TTX2DA_Y(dy1, ndy1);
  TTX2DA_Y(dy2+1, ndy2)-1;

  gdk_gc_set_clip_origin(data->xor_gc, 0, 0);

  rect.x = rect.y = 0;
  rect.width = w; rect.height = h;

  window = region_from_rect(&rect);

  rect.x = nsx1; rect.y = nsy1;
  rect.width = (nsx2 - rect.x) +1; rect.height = (nsy2 - rect.y) +1;

  temp_region = region_from_rect(&rect);
  clip_region = gdk_regions_subtract(window, temp_region);
  gdk_gc_set_clip_region(data->xor_gc, clip_region);
  g_free(temp_region);
  g_free(clip_region);

  select_region(dx1, dy1, dx2, dy2, data);

  rect.x = ndx1; rect.y = ndy1;
  rect.width = (ndx2 - rect.x)+1; rect.height = (ndy2 - rect.y)+1;

  temp_region = region_from_rect(&rect);
  clip_region = gdk_regions_subtract(window, temp_region);
  gdk_gc_set_clip_region(data->xor_gc, clip_region);
  g_free(temp_region);
  g_free(clip_region);

  select_region(sx1, sy1, sx2, sy2, data);

  gdk_region_destroy(window);

  /* no more clip rect */
  gdk_gc_set_clip_rectangle(data->xor_gc, NULL);
}

static void select_update(gint x, gint y, ttxview_data * data)
{
  gint w, h;
  gint ocol, orow, col, row, scol, osrow, srow;

  if (!data->selecting)
    return;

  gdk_window_get_size(data->da->window, &w, &h);

  col = (x*40)/w;
  row = (y*25)/h;
  scol = (data->ssx*40)/w;
  srow = (data->ssy*25)/h;
  if (data->osx == -1)
    {
      ocol = (data->ssx*40)/w;
      orow = (data->ssy*25)/h;
    }
  else
    {
      ocol = (data->osx*40)/w;
      orow = (data->osy*25)/h;
    }

  osrow = srow;;

  if (row > srow)
    {
      if (hidden_row(srow))
	srow--;
      if (hidden_row(row+1))
	row++;
    }
  else if (row < srow)
    {
      if (hidden_row(srow+1))
	srow++;
      if (hidden_row(row))
	row--;
    }
  else /* row == srow */
    {
      if (hidden_row(row))
	srow--;
      else if (hidden_row(row+1))
	row++;
    }

  if (orow > osrow)
    {
      if (hidden_row(osrow))
	osrow--;
      if (hidden_row(orow+1))
	orow++;
    }
  else if (orow < osrow)
    {
      if (hidden_row(osrow+1))
	osrow++;
      if (hidden_row(orow))
	orow--;
    }
  else /* orow == osrow */
    {
      if (hidden_row(orow))
	osrow--;
      else if (hidden_row(orow+1))
	orow++;
    }

  if (col > 39) col = 39;
  if (col < 0) col = 0;
  if (row > 24) row = 24;
  if (row < 0) row = 0;
  if (scol > 39) scol = 39;
  if (scol < 0) scol = 0;
  if (srow > 24) srow = 24;
  if (srow < 0) srow = 0;
  if (ocol > 39) ocol = 39;
  if (ocol < 0) ocol = 0;
  if (orow > 24) orow = 24;
  if (orow < 0) orow = 0;
  if (osrow > 24) osrow = 24;
  if (osrow < 0) osrow = 0;

  /* first movement */
  if (data->osx == -1)
    {
      select_region(scol, srow, col, row, data);
      data->osx = (x < 0)?0:x;
      data->osy = y;
      return;
    }

  /* transform ocol, orow, scol, osrow  into  col, row, scol, srow */
  transform_region(ocol, orow, scol, osrow, col, row, scol, srow, data);

  data->osx = (x < 0)?0:x;
  data->osy = y;
}

static gboolean
on_ttxview_motion_notify		(GtkWidget	*widget,
					 GdkEventMotion	*event,
					 ttxview_data	*data)
{
  if (!data->selecting)
    update_pointer(data);
  else
    select_update(event->x, event->y, data);

  return FALSE;
}

static
gboolean on_ttxview_expose_event	(GtkWidget	*widget,
					 GdkEventExpose	*event,
					 ttxview_data	*data)
{
  gint w, h;
  gint scol, srow, col, row;

  render_ttx_page(data->id, widget->window, widget->style->white_gc,
		  event->area.x, event->area.y,
		  event->area.x, event->area.y,
		  event->area.width, event->area.height);
  
  if (data->selecting && data->osx != -1)
    {
      gdk_gc_set_clip_origin(data->xor_gc, 0, 0);
      gdk_gc_set_clip_rectangle(data->xor_gc, &event->area);

      gdk_window_get_size(data->da->window, &w, &h);

      scol = (data->ssx*40)/w;
      srow = (data->ssy*25)/h;
      col = (data->osx*40)/w;
      row = (data->osy*25)/h;

      if (row > srow)
	{
	  if (hidden_row(srow))
	    srow--;
	  if (hidden_row(row+1))
	    row++;
	}
      else if (row < srow)
	{
	  if (hidden_row(srow+1))
	    srow++;
	  if (hidden_row(row))
	    row--;
	}
      else /* row == srow */
	{
	  if (hidden_row(row))
	    srow--;
	  else if (hidden_row(row+1))
	    row++;
	}

      if (col > 39) col = 39;
      if (col < 0) col = 0;
      if (row > 24) row = 24;
      if (row < 0) row = 0;
      if (scol > 39) scol = 39;
      if (scol < 0) scol = 0;
      if (srow > 24) srow = 24;
      if (srow < 0) srow = 0;

      select_region(scol, srow, col, row, data);

      gdk_gc_set_clip_rectangle(data->xor_gc, NULL);
    }

  return TRUE;
}

static gboolean
on_ttxview_button_press			(GtkWidget	*widget,
					 GdkEventButton	*event,
					 ttxview_data	*data)
{
  gint w, h, col, row;
  vbi_link_descr ld;
  GtkWidget *dolly;
  GtkMenu *menu;
  gchar * buffer;

  gdk_window_get_size(widget->window, &w, &h);
  /* convert to fmt_page space */
  col = (event->x*40)/w;
  row = (event->y*25)/h;

  ld.type = VBI_LINK_NONE;
  ld.pgno = ld.subno = 0;

  /* Any modifier enters select mode */
  if ((data->fmt_page->data[row][col].link) && (!
      (event->state & GDK_SHIFT_MASK ||
       event->state & GDK_CONTROL_MASK ||
       event->state & GDK_MOD1_MASK)))
    vbi_resolve_link(data->fmt_page, col, row, &ld);

  switch (event->button)
    {
    case 1:
      switch (ld.type)
        {
	case VBI_LINK_PAGE:
	case VBI_LINK_SUBPAGE:
	  load_page(ld.pgno, ld.subno, data, NULL);
	  break;

	case VBI_LINK_HTTP:
	case VBI_LINK_FTP:
	  gnome_url_show(ld.text);
	  break;

	case VBI_LINK_EMAIL:
	  buffer = g_strconcat("mailto:", ld.text, NULL);
	  gnome_url_show(buffer);
	  g_free(buffer);
	  break;

	default:
	  /* start selecting */
	  select_start(event->x, event->y, data);
	  break;
	}
      break;
    case 2: /* middle button, open link in new window */
      switch (ld.type)
        {
	case VBI_LINK_PAGE:
	case VBI_LINK_SUBPAGE:
	  dolly = build_ttxview();
	  load_page(ld.pgno, ld.subno,
	    (ttxview_data*)gtk_object_get_data(GTK_OBJECT(dolly),
					       "ttxview_data"), NULL);
	  gtk_widget_show(dolly);
	  break;

	case VBI_LINK_HTTP:
	case VBI_LINK_FTP:
	  gnome_url_show(ld.text);
	  break;

	case VBI_LINK_EMAIL:
	  buffer = g_strconcat("mailto:", ld.text, NULL);
	  gnome_url_show(buffer);
	  g_free(buffer);
	  break;

	default:
	}
      break;
    default: /* context menu */
      if (data->popup_menu)
	{
	  if (ld.type != VBI_LINK_PAGE &&
	      ld.type != VBI_LINK_SUBPAGE)
	    ld.pgno = ld.subno = 0;

	  menu = GTK_MENU(build_ttxview_popup(data, ld.pgno,
					      ld.subno));
	  gtk_menu_popup(menu, NULL, NULL, NULL,
			 NULL, event->button, event->time);
	}
      break;
    }
  
  return FALSE;
}

static gboolean
on_ttxview_button_release		(GtkWidget	*widget,
					 GdkEventButton	*event,
					 ttxview_data	*data)
{
  select_stop(data);

  return FALSE;
}

static
gboolean on_ttxview_key_press		(GtkWidget	*widget,
					 GdkEventKey	*event,
					 ttxview_data	*data)
{
  gchar *buffer;
  GtkWidget * ttxview_hold = lookup_widget(data->toolbar, "ttxview_hold");
  gboolean active;

  switch (event->keyval)
    {
    case GDK_0 ... GDK_9:
      if (data->page >= 0x100)
	data->page = 0;
      data->page = (data->page<<4)+event->keyval-GDK_0;
      if (data->page > 0x899)
	data->page = 0x899;
      if (data->page >= 0x100)
	load_page(data->page, ANY_SUB, data, NULL);
      else
	{
	  buffer = g_strdup_printf("%d", bcd2dec(data->page));
	  gtk_label_set_text(GTK_LABEL(lookup_widget(data->toolbar,
			     "ttxview_url")), buffer);
	  g_free(buffer);
	}
      break;
    case GDK_KP_0 ... GDK_KP_9:
      if (data->page >= 0x100)
	data->page = 0;
      data->page = (data->page<<4)+event->keyval-GDK_KP_0;
      if (data->page > 0x899)
	data->page = 0x899;
      if (data->page >= 0x100)
	load_page(data->page, ANY_SUB, data, NULL);
      else
	{
	  buffer = g_strdup_printf("%d", bcd2dec(data->page));
	  gtk_label_set_text(GTK_LABEL(lookup_widget(data->toolbar,
			     "ttxview_url")), buffer);
	  g_free(buffer);
	}
      break;
    case GDK_Page_Down:
    case GDK_KP_Page_Down:
      if (data->page < 0x100)
	data->page = add_bcd(data->fmt_page->vtp->pgno, 0x010);
      else
	data->page = add_bcd(data->page, 0x010);
      if (data->page > 0x899)
	data->page = 0x100;
      load_page(data->page, ANY_SUB, data, NULL);
      break;
    case GDK_Page_Up:
    case GDK_KP_Page_Up:
      if (data->page < 0x100)
	data->page = add_bcd(data->fmt_page->vtp->pgno, 0x990);
      else
	data->page = add_bcd(data->page, 0x990);
      if (data->page < 0x100)
	data->page = 0x899;
      load_page(data->page, ANY_SUB, data, NULL);
      break;
    case GDK_KP_Up:
    case GDK_Up:
      if (data->page < 0x100)
	data->page = add_bcd(data->fmt_page->vtp->pgno, 0x999);
      else
	data->page = add_bcd(data->page, 0x999);
      if (data->page < 0x100)
	data->page = 0x899;
      load_page(data->page, ANY_SUB, data, NULL);
      break;
    case GDK_KP_Down:
    case GDK_Down:
      if (data->page < 0x100)
	data->page = add_bcd(data->fmt_page->vtp->pgno, 0x001);
      else
	data->page = add_bcd(data->page, 0x001);
      if (data->page > 0x899)
	data->page = 0x100;
      load_page(data->page, ANY_SUB, data, NULL);
      break;
    case GDK_KP_Left:
    case GDK_Left:
      on_ttxview_prev_sp_cache_clicked(GTK_BUTTON(lookup_widget(data->toolbar,
				     "ttxview_prev_sp_cache")), data);
      break;
    case GDK_KP_Right:
    case GDK_Right:
      on_ttxview_next_sp_cache_clicked(GTK_BUTTON(lookup_widget(data->toolbar,
				     "ttxview_next_sp_cache")), data);
      break;
    case GDK_Home:
    case GDK_KP_Home:
      on_ttxview_home_clicked(GTK_BUTTON(lookup_widget(data->toolbar,
						       "ttxview_home")), data);
      break;
    case GDK_H:
    case GDK_h:
      active =
	gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ttxview_hold));
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ttxview_hold), !active);
      break;
    case GDK_P:
    case GDK_p:
      select_region(30, 10, 10, 20, data);
      break;
    default:
      return FALSE;
    }

  return TRUE;
}

GtkWidget*
build_ttxview(void)
{
  GtkWidget *ttxview = create_ttxview();
  ttxview_data *data;

  if (!zvbi_get_object())
    {
      ShowBox("VBI couldn't be opened, Teletext won't work",
	      GNOME_MESSAGE_BOX_ERROR);
      return ttxview;
    }

  data = g_malloc(sizeof(ttxview_data));
  memset(data, 0, sizeof(ttxview_data));

  data->da = lookup_widget(ttxview, "drawingarea1");
  data->appbar = lookup_widget(ttxview, "appbar1");
  data->toolbar = lookup_widget(ttxview, "toolbar2");
  data->id = register_ttx_client();
  data->parent = ttxview;
  data->timeout =
    gtk_timeout_add(50, (GtkFunction)event_timeout, data);
  data->fmt_page = get_ttx_fmt_page(data->id);
  data->popup_menu = TRUE;
  gtk_object_set_data(GTK_OBJECT(ttxview), "ttxview_data", data);
  data->xor_gc = gdk_gc_new(data->da->window);
  gdk_gc_set_function(data->xor_gc, GDK_INVERT);

  /* Callbacks */
  gtk_signal_connect(GTK_OBJECT(ttxview), "delete-event",
		     GTK_SIGNAL_FUNC(on_ttxview_delete_event), data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar,
		      "ttxview_prev_subpage")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_prev_subpage_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar,
		      "ttxview_next_subpage")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_next_subpage_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar, "ttxview_home")),
		     "clicked", GTK_SIGNAL_FUNC(on_ttxview_home_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar, "ttxview_hold")),
		     "toggled", GTK_SIGNAL_FUNC(on_ttxview_hold_toggled),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar, "ttxview_clone")),
		     "clicked", GTK_SIGNAL_FUNC(on_ttxview_clone_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar,
					      "ttxview_search")),
		     "clicked", GTK_SIGNAL_FUNC(on_ttxview_search_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar,
		     "ttxview_history_prev")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_history_prev_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar,
		     "ttxview_history_next")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_history_next_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar,
		     "ttxview_prev_sp_cache")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_prev_sp_cache_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar,
		     "ttxview_next_sp_cache")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_next_sp_cache_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(data->da),
		     "size-allocate",
		     GTK_SIGNAL_FUNC(on_ttxview_size_allocate), data);
  gtk_signal_connect(GTK_OBJECT(data->da),
		     "expose-event",
		     GTK_SIGNAL_FUNC(on_ttxview_expose_event), data);
  gtk_signal_connect(GTK_OBJECT(data->da),
		     "motion-notify-event",
		     GTK_SIGNAL_FUNC(on_ttxview_motion_notify), data);
  gtk_signal_connect(GTK_OBJECT(data->da),
		     "button-press-event",
		     GTK_SIGNAL_FUNC(on_ttxview_button_press), data);
  gtk_signal_connect(GTK_OBJECT(data->da),
		     "button-release-event",
		     GTK_SIGNAL_FUNC(on_ttxview_button_release), data);
  gtk_signal_connect(GTK_OBJECT(data->parent),
		     "key-press-event",
		     GTK_SIGNAL_FUNC(on_ttxview_key_press), data);
  /* selection (aka clipboard) handling */
  gtk_signal_connect (GTK_OBJECT(data->da), "selection_clear_event",
		      GTK_SIGNAL_FUNC (selection_clear), data);
  gtk_selection_add_target (data->da,
			    GDK_SELECTION_PRIMARY,
			    GDK_SELECTION_TYPE_STRING,
		            1);
  gtk_selection_add_target(data->da, clipboard_atom,
			   GDK_SELECTION_TYPE_STRING, 1);
  gtk_signal_connect (GTK_OBJECT(data->da), "selection_get",
		      GTK_SIGNAL_FUNC (selection_handle), data);

  gtk_toolbar_set_style(GTK_TOOLBAR(data->toolbar), GTK_TOOLBAR_ICONS);
  gtk_widget_set_usize(ttxview, 360, 400);
  gtk_widget_realize(ttxview);
  gdk_window_set_back_pixmap(data->da->window, NULL, FALSE);

  load_page(0x100, ANY_SUB, data, NULL);

  return (ttxview);
}

void
ttxview_attach			(GtkWidget	*parent,
				 GtkWidget	*da,
				 GtkWidget	*toolbar,
				 GtkWidget	*appbar)
{
  ttxview_data *data =
    gtk_object_get_data(GTK_OBJECT(parent), "ttxview_data");
  gint w, h;

  if (!zvbi_get_object())
    {
      ShowBox("VBI couldn't be opened, Teletext won't work",
	      GNOME_MESSAGE_BOX_ERROR);
      return;
    }

  /* already being used as TTXView */
  if (data)
    return;

  data = g_malloc(sizeof(ttxview_data));
  memset(data, 0, sizeof(ttxview_data));

  gtk_object_set_data(GTK_OBJECT(parent), "ttxview_data", data);

  data->da = da;
  data->appbar = appbar;
  data->toolbar = create_widget("toolbar2");
  data->id = register_ttx_client();
  data->parent = parent;
  data->parent_toolbar = toolbar;
  data->timeout =
    gtk_timeout_add(50, (GtkFunction)event_timeout, data);
  data->fmt_page = get_ttx_fmt_page(data->id);
  data->popup_menu = FALSE;
  data->xor_gc = gdk_gc_new(data->da->window);
  gdk_gc_set_function(data->xor_gc, GDK_INVERT);

  /* Callbacks */
  gtk_signal_connect(GTK_OBJECT(data->parent), "delete-event",
		     GTK_SIGNAL_FUNC(on_ttxview_delete_event), data);
  gtk_signal_connect(GTK_OBJECT(data->parent), "key-press-event",
		     GTK_SIGNAL_FUNC(on_ttxview_key_press), data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar,
		     "ttxview_prev_subpage")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_prev_subpage_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar,
		      "ttxview_next_subpage")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_next_subpage_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar, "ttxview_home")),
		     "clicked", GTK_SIGNAL_FUNC(on_ttxview_home_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar, "ttxview_hold")),
		     "toggled", GTK_SIGNAL_FUNC(on_ttxview_hold_toggled),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar, "ttxview_clone")),
		     "clicked", GTK_SIGNAL_FUNC(on_ttxview_clone_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar,
					      "ttxview_search")),
		     "clicked", GTK_SIGNAL_FUNC(on_ttxview_search_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar,
		     "ttxview_history_prev")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_history_prev_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar,
		     "ttxview_history_next")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_history_next_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar,
		     "ttxview_prev_sp_cache")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_prev_sp_cache_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(data->toolbar,
		     "ttxview_next_sp_cache")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_next_sp_cache_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(data->da),
		     "size-allocate",
		     GTK_SIGNAL_FUNC(on_ttxview_size_allocate), data);
  gtk_signal_connect(GTK_OBJECT(data->da),
		     "expose-event",
		     GTK_SIGNAL_FUNC(on_ttxview_expose_event), data);
  gtk_signal_connect(GTK_OBJECT(data->da),
		     "motion-notify-event",
		     GTK_SIGNAL_FUNC(on_ttxview_motion_notify), data);
  gtk_signal_connect(GTK_OBJECT(data->da),
		     "button-press-event",
		     GTK_SIGNAL_FUNC(on_ttxview_button_press), data);
  gtk_signal_connect(GTK_OBJECT(data->da),
		     "button-release-event",
		     GTK_SIGNAL_FUNC(on_ttxview_button_release), data);
  gtk_signal_connect (GTK_OBJECT(data->da), "selection_clear_event",
		      GTK_SIGNAL_FUNC (selection_clear), data);
  gtk_selection_add_target (data->da,
			    GDK_SELECTION_PRIMARY,
			    GDK_SELECTION_TYPE_STRING,
		            1);
  gtk_selection_add_target (data->da,
			    clipboard_atom,
			    GDK_SELECTION_TYPE_STRING,
		            1);
  gtk_signal_connect (GTK_OBJECT(data->da), "selection_get",
		      GTK_SIGNAL_FUNC (selection_handle), data);

  if (data->da->window)
    {
      gdk_window_get_size(data->da->window, &w, &h);
      resize_ttx_page(data->id, w, h);
      gdk_window_clear_area_e(data->da->window, 0, 0, w, h);
    }

  gtk_toolbar_set_style(GTK_TOOLBAR(data->toolbar), GTK_TOOLBAR_ICONS);

  gtk_widget_show(data->toolbar);

  gtk_toolbar_set_style(GTK_TOOLBAR(data->parent_toolbar), GTK_TOOLBAR_ICONS);

  gtk_toolbar_append_widget(GTK_TOOLBAR(data->parent_toolbar),
			    data->toolbar, "", "");

  load_page(0x100, ANY_SUB, data, NULL);
}

void
ttxview_detach			(GtkWidget	*parent)
{
  ttxview_data *data =
    gtk_object_get_data(GTK_OBJECT(parent), "ttxview_data");

  if (!data)
    return;

  gtk_container_remove(GTK_CONTAINER(data->parent_toolbar),
		       data->toolbar);

  gtk_signal_disconnect_by_func(GTK_OBJECT(data->parent),
				GTK_SIGNAL_FUNC(on_ttxview_delete_event),
				data);
  gtk_signal_disconnect_by_func(GTK_OBJECT(data->parent),
				GTK_SIGNAL_FUNC(on_ttxview_key_press),
				data);
  gtk_signal_disconnect_by_func(GTK_OBJECT(data->da),
				GTK_SIGNAL_FUNC(on_ttxview_size_allocate),
				data);
  gtk_signal_disconnect_by_func(GTK_OBJECT(data->da),
				GTK_SIGNAL_FUNC(on_ttxview_expose_event),
				data);
  gtk_signal_disconnect_by_func(GTK_OBJECT(data->da),
				GTK_SIGNAL_FUNC(on_ttxview_motion_notify),
				data);
  gtk_signal_disconnect_by_func(GTK_OBJECT(data->da),
				GTK_SIGNAL_FUNC(on_ttxview_button_press),
				data);
  gtk_signal_disconnect_by_func(GTK_OBJECT(data->da),
				GTK_SIGNAL_FUNC(on_ttxview_button_release),
				data);
  gtk_signal_disconnect_by_func(GTK_OBJECT(data->da),
				GTK_SIGNAL_FUNC(selection_handle),
				data);
  gtk_signal_disconnect_by_func(GTK_OBJECT(data->da),
				GTK_SIGNAL_FUNC(selection_clear),
				data);

  gtk_toolbar_set_style(GTK_TOOLBAR(data->parent_toolbar),
			GTK_TOOLBAR_BOTH);

  remove_ttxview_instance(data);

  gtk_object_set_data(GTK_OBJECT(parent), "ttxview_data", NULL);
}
