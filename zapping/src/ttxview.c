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
#include "zmisc.h"
#include "zconf.h"
#include "zmodel.h"
#include "../common/fifo.h"

/*
  TODO:
	better handling of loading... page
	Search
	Export filters
	ttxview in main window (alpha et al)
*/

static GdkCursor	*hand=NULL;
static GdkCursor	*arrow=NULL;

typedef struct {
  GdkPixmap		*scaled;
  GdkBitmap		*mask;
  gint			w, h;
  GtkWidget		*da;
  int			id; /* TTX client id */
  guint			timeout; /* id */
  struct fmt_page	*fmt_page; /* current page, formatted */
  gint			page; /* page we are entering */
  gint			subpage; /* current subpage */
  gboolean		extra_controls; /* TRUE: Show extra controls */
  GList			*history_stack; /* for back, etc... */
  gint			history_stack_size; /* items in history_stack */
  gint			history_sp; /* pointer in the stack */
  gint			monitored_subpage;
  gboolean		no_history; /* don't send to history next page */
  gboolean		hold; /* hold the current subpage */
  gint			pop_pgno, pop_subno; /* for popup */
} ttxview_data;

struct bookmark {
  gint page;
  gint subpage;
  gchar *description;
};

static GList	*bookmarks=NULL;
static ZModel	*model=NULL; /* for bookmarks */

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
  arrow = gdk_cursor_new(GDK_LEFT_PTR);
  model = ZMODEL(zmodel_new());

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
  GtkWidget *widget = lookup_widget(wid, "ttxview");

  if ((data->w != w) ||
      (data->h != h))
    {
      if (data->mask)
	gdk_bitmap_unref(data->mask);
      if (data->scaled)
	gdk_pixmap_unref(data->scaled);
      data->scaled = gdk_pixmap_new(widget->window, w, h, -1);
      data->mask = gdk_pixmap_new(widget->window, w, h, 1);
      data->w = w;
      data->h = h;
    }

  if (data->scaled)
    {
      render_ttx_page(data->id, data->scaled,
		      widget->style->white_gc, data->mask, w, h);
      gdk_window_shape_combine_mask(data->da->window, data->mask, 0, 0);
    }
}

static int
find_prev_subpage (ttxview_data	*data, int subpage)
{
  struct vbi *vbi = zvbi_get_object();

  if ((!vbi->cache->hi_subno[data->fmt_page->vtp->pgno]) ||
      (!vbi->cache->op->get(vbi->cache, data->fmt_page->vtp->pgno,
			    subpage, 0xffff)))
    return -1;

  do {
    subpage = dec2hex(hex2dec(subpage) - 1);
    
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

  if ((!vbi->cache->hi_subno[data->fmt_page->vtp->pgno]) ||
      (!vbi->cache->op->get(vbi->cache, data->fmt_page->vtp->pgno,
			    subpage, 0xffff)))
    return -1;

  do {
    subpage = dec2hex(hex2dec(subpage) + 1);
    
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

  prev = lookup_widget(data->da, "ttxview_history_prev");
  next = lookup_widget(data->da, "ttxview_history_next");
  prev_subpage = lookup_widget(data->da, "ttxview_prev_subpage");
  next_subpage = lookup_widget(data->da, "ttxview_next_subpage");

  if (data->history_stack_size > (data->history_sp+1))
    gtk_widget_set_sensitive(next, TRUE);
  else
    gtk_widget_set_sensitive(next, FALSE);
  
  if (data->history_sp > 0)
    gtk_widget_set_sensitive(prev, TRUE);
  else
    gtk_widget_set_sensitive(prev, FALSE);

  /* FIXME: add status for subpages */
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

static
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

static gboolean
on_ttxview_delete_event			(GtkWidget	*widget,
					 GdkEvent	*event,
					 ttxview_data	*data)
{
  if (data->scaled)
    gdk_pixmap_unref(data->scaled);
  if (data->mask)
    gdk_bitmap_unref(data->mask);

  unregister_ttx_client(data->id);
  gtk_timeout_remove(data->timeout);

  g_free(data);

  return FALSE;
}


static void
update_pointer (ttxview_data *data)
{
  gint x, y;
  GdkModifierType mask;
  gint w, h, col, row;
  gint page, subpage;
  gchar *buffer;
  GtkWidget *widget = data->da;
  GtkWidget *appbar1 = lookup_widget(widget, "appbar1");
  
  gdk_window_get_pointer(widget->window, &x, &y, &mask);

  gdk_window_get_size(widget->window, &w, &h);
  /* convert to fmt_page space */
  col = (x*40)/w;
  row = (y*25)/h;
  page = data->fmt_page->data[row][col].link_page;
  subpage = data->fmt_page->data[row][col].link_subpage;

  if (page)
    {
      if (subpage == (guchar)ANY_SUB)
	buffer = g_strdup_printf(_("Page %d"), hex2dec(page));
      else
	buffer = g_strdup_printf(_("Subpage %d"), hex2dec(subpage));
      gnome_appbar_set_status(GNOME_APPBAR(appbar1), buffer);
      g_free(buffer);
      gdk_window_set_cursor(widget->window, hand);
    }
  else
    {
      gnome_appbar_set_status(GNOME_APPBAR(appbar1), "");
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
      switch (msg)
	{
	case TTX_PAGE_RECEIVED:
	  gdk_window_get_size(data->da->window, &w, &h);
	  scale_image(data->da, w, h, data);
	  gdk_window_clear_area_e(data->da->window, 0, 0, w, h);
	  data->subpage = data->fmt_page->vtp->subno;
	  widget = lookup_widget(data->da, "ttxview_subpage");
	  buffer = g_strdup_printf("S%x", data->subpage);
	  gtk_label_set_text(GTK_LABEL(widget), buffer);
	  if (!data->no_history)
	    append_history(data->fmt_page->vtp->pgno,
			   data->monitored_subpage, data);
	  data->no_history = FALSE;
	  g_free(buffer);
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
load_page (int page, int subpage, ttxview_data *data)
{
  GtkWidget *appbar1 = lookup_widget(data->da, "appbar1");
  GtkWidget *ttxview_url = lookup_widget(data->da, "ttxview_url");
  GtkWidget *ttxview_hold = lookup_widget(data->da, "ttxview_hold");
  GtkWidget *widget;
  gchar *buffer;

  buffer = g_strdup_printf("%d", hex2dec(page));
  gtk_label_set_text(GTK_LABEL(ttxview_url), buffer);
  g_free(buffer);

  data->hold = (subpage != ANY_SUB)?TRUE:FALSE;
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ttxview_hold),
			       data->hold);
    
  data->subpage = subpage;
  data->monitored_subpage = subpage;
  widget = lookup_widget(data->da, "ttxview_subpage");
  if (subpage != ANY_SUB)
    buffer = g_strdup_printf("S%x", data->subpage);
  else
    buffer = g_strdup(_(""));
  gtk_label_set_text(GTK_LABEL(widget), buffer);
  g_free(buffer);

  if (subpage == ANY_SUB)
    buffer = g_strdup_printf(_("Loading page %d..."), hex2dec(page));
  else
    buffer = g_strdup_printf(_("Loading subpage %d..."),
			     hex2dec(subpage));
  gnome_appbar_push(GNOME_APPBAR(appbar1), buffer);
  g_free(buffer);

  gtk_widget_grab_focus(data->da);
  
  while (gtk_events_pending())
    gtk_main_iteration(); /* make all the changes show now */

  monitor_ttx_page(data->id, page, subpage);
}

static
void on_ttxview_home_clicked		(GtkButton	*button,
					 ttxview_data	*data)
{
  int page, subpage;

  get_ttx_index(data->id, &page, &subpage);
  load_page(page, subpage, data);
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
		  data);
      else
	load_page(data->fmt_page->vtp->pgno, ANY_SUB, data);
    }
}

static
void on_ttxview_prev_sp_cache_clicked	(GtkButton	*button,
					 ttxview_data	*data)
{
  struct vbi *vbi = zvbi_get_object();
  GtkWidget *appbar1 = lookup_widget(data->da, "appbar1");
  int subpage;

  g_assert(vbi != NULL);

  if (!vbi->cache)
    {
      gnome_appbar_set_status(GNOME_APPBAR(appbar1), _("No cache"));
      return;
    }
  
  if (((subpage = find_prev_subpage(data, data->subpage)) >= 0) &&
      (subpage != data->subpage))
    load_page(data->fmt_page->vtp->pgno, subpage, data);
  else
    gnome_appbar_set_status(GNOME_APPBAR(appbar1),
			    _("No other subpage in the cache"));
}

static
void on_ttxview_prev_subpage_clicked	(GtkButton	*button,
					 ttxview_data	*data)
{
  gint new_subpage = dec2hex(hex2dec(data->subpage) - 1);
  if (new_subpage < 0)
    new_subpage = 0x99;
  load_page(data->fmt_page->vtp->pgno, new_subpage, data);
}

static
void on_ttxview_next_sp_cache_clicked	(GtkButton	*button,
					 ttxview_data	*data)
{
  struct vbi *vbi = zvbi_get_object();
  GtkWidget *appbar1 = lookup_widget(data->da, "appbar1");
  int subpage;

  g_assert(vbi != NULL);

  if (!vbi->cache)
    {
      gnome_appbar_set_status(GNOME_APPBAR(appbar1), _("No cache"));
      return;
    }
  
  if (((subpage = find_next_subpage(data, data->subpage)) >= 0) &&
      (subpage != data->subpage))
    load_page(data->fmt_page->vtp->pgno, subpage, data);
  else
    gnome_appbar_set_status(GNOME_APPBAR(appbar1),
			    _("No other subpage in the cache"));
}

static
void on_ttxview_next_subpage_clicked	(GtkButton	*button,
					 ttxview_data	*data)
{
  gint new_subpage = dec2hex(hex2dec(data->subpage) + 1);
  if (new_subpage > 0x99)
    new_subpage = 0;
  load_page(data->fmt_page->vtp->pgno, new_subpage, data);
}

static
void on_ttxview_search_clicked		(GtkButton	*button,
					 ttxview_data	*data)
{
  ShowBox("Not done yet", GNOME_MESSAGE_BOX_INFO);
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
  load_page(page, pc_subpage, data);
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
  load_page(page, pc_subpage, data);
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
	      (ttxview_data*)gtk_object_get_user_data(GTK_OBJECT(dolly)));
  else
    load_page(0x100, ANY_SUB,
	      (ttxview_data*)gtk_object_get_user_data(GTK_OBJECT(dolly)));
  gdk_window_get_size(lookup_widget(GTK_WIDGET(button),
				    "ttxview")->window, &w,
		      &h);
  gtk_widget_realize(dolly);
  while (gtk_events_pending())
    gtk_main_iteration();
  gdk_window_resize(dolly->window, w, h);
  gtk_widget_show(dolly);
}

static
void on_ttxview_size_allocate		(GtkWidget	*widget,
					 GtkAllocation	*allocation,
					 ttxview_data	*data)
{
  scale_image(widget, allocation->width, allocation->height, data);

  if (data->scaled)
    gdk_draw_pixmap(widget->window, widget->style->white_gc,
		    data->scaled, 0, 0, 0, 0,
		    allocation->width, allocation->height);
}

static
gboolean on_ttxview_expose_event	(GtkWidget	*widget,
					 GdkEventExpose	*event,
					 ttxview_data	*data)
{
  if (data->scaled)
    gdk_draw_pixmap(widget->window, widget->style->white_gc,
		    data->scaled, event->area.x, event->area.y,
		    event->area.x, event->area.y,
		    event->area.width, event->area.height);

  return TRUE;
}

static gboolean
on_ttxview_motion_notify		(GtkWidget	*widget,
					 GdkEventMotion	*event,
					 ttxview_data	*data)
{
  update_pointer(data);

  return FALSE;
}

static
void popup_new_win			(GtkWidget	*widget,
					 ttxview_data	*data)
{
  GtkWidget *dolly = build_ttxview();
  load_page(data->pop_pgno, data->pop_subno,
	    (ttxview_data*)gtk_object_get_user_data(GTK_OBJECT(dolly)));
  gtk_widget_show(dolly);
}

static
void new_bookmark			(GtkWidget	*widget,
					 ttxview_data	*data)
{
  gchar *default_description;
  gchar *buffer;
  gint page, subpage;

  if (data->page >= 0x100)
    page = data->page;
  else
    page = data->fmt_page->vtp->pgno;
  subpage = data->monitored_subpage;

  if (subpage != ANY_SUB)
    default_description =
      g_strdup_printf("%x.%x", page, subpage);
  else
    default_description = g_strdup_printf("%x", page);

  buffer = Prompt(lookup_widget(data->da, "ttxview"),
		  _("New bookmark"),
		  _("Description:"),
		  default_description);
  if (buffer)
    {
      add_bookmark(page, subpage, buffer);
      default_description =
	g_strdup_printf(_("<%s> added to the bookmarks"), buffer);
      gnome_appbar_set_status(GNOME_APPBAR(lookup_widget(data->da,
							 "appbar1")),
			      default_description);
      g_free(default_description);
    }
  g_free(buffer);
}

static
void not_done_yet(gpointer ping, gpointer pong)
{
  ShowBox("Not done yet", GNOME_MESSAGE_BOX_INFO);
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
void on_edit_bookmarks_activated	(GtkWidget	*widget,
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
void on_bookmark_activated		(GtkWidget	*widget,
					 ttxview_data	*data)
{
  struct bookmark *bookmark = (struct bookmark*)
    gtk_object_get_user_data(GTK_OBJECT(widget));

  load_page(bookmark->page, bookmark->subpage, data);
}

static
GtkWidget *build_ttxview_popup (ttxview_data *data, gint page, gint subpage)
{
  GtkWidget *popup = create_ttxview_popup();
  GList *p = g_list_first(bookmarks);
  struct bookmark *bookmark;
  GtkWidget *menuitem;
  gchar *buffer;
  GtkWidget *menu = lookup_widget(popup, "bookmarks1");
  menu = GTK_MENU_ITEM(menu)->submenu;

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
		     GTK_SIGNAL_FUNC(not_done_yet), data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(popup, "ppm1")),
		     "activate",
		     GTK_SIGNAL_FUNC(not_done_yet), data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(popup, "ascii1")),
		     "activate",
		     GTK_SIGNAL_FUNC(not_done_yet), data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(popup, "add_bookmark")),
		     "activate",
		     GTK_SIGNAL_FUNC(new_bookmark), data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(popup, "edit_bookmarks")),
		     "activate",
		     GTK_SIGNAL_FUNC(on_edit_bookmarks_activated),
		     data);

  /* Bookmark entries */
  if (!p)
    gtk_widget_hide(lookup_widget(popup, "separator9"));
  else
    while (p)
      {
	bookmark = (struct bookmark*)p->data;
	menuitem = z_gtk_pixmap_menu_item_new(bookmark->description,
					      GNOME_STOCK_PIXMAP_EXEC);
	if (bookmark->subpage != ANY_SUB)
	  buffer = g_strdup_printf("%x.%x", bookmark->page, bookmark->subpage);
	else
	  buffer = g_strdup_printf("%x", bookmark->page);
	set_tooltip(menuitem, buffer);
	g_free(buffer);
	gtk_object_set_user_data(GTK_OBJECT(menuitem), bookmark);
	gtk_widget_show(menuitem);
	gtk_menu_append(GTK_MENU(menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(on_bookmark_activated),
			   data);
	p = p->next;
      }

  return popup;
}

static gboolean
on_ttxview_button_press			(GtkWidget	*widget,
					 GdkEventButton	*event,
					 ttxview_data	*data)
{
  gint w, h, col, row, page, subpage;
  GtkWidget *dolly;
  GtkMenu *menu;

  gdk_window_get_size(widget->window, &w, &h);
  /* convert to fmt_page space */
  col = (event->x*40)/w;
  row = (event->y*25)/h;
  page = data->fmt_page->data[row][col].link_page;
  subpage = data->fmt_page->data[row][col].link_subpage;
  if (subpage == (guchar)ANY_SUB)
    subpage = ANY_SUB;

  switch (event->button)
    {
    case 1:
      if (page)
	load_page(page, subpage, data);
      break;
    case 2: /* middle button, open link in new window */
      if (page)
	{
	  dolly = build_ttxview();
	  load_page(page, subpage,
	    (ttxview_data*)gtk_object_get_user_data(GTK_OBJECT(dolly)));
	  gtk_widget_show(dolly);
	}
      break;
    default: /* context menu */
      menu = GTK_MENU(build_ttxview_popup(data, page,
					  subpage));
      gtk_menu_popup(menu, NULL, NULL, NULL,
		     NULL, event->button, event->time);
      break;
    }
  
  return FALSE;
}

static
gboolean on_ttxview_key_press		(GtkWidget	*widget,
					 GdkEventKey	*event,
					 ttxview_data	*data)
{
  gchar *buffer;

  switch (event->keyval)
    {
    case GDK_0...GDK_9:
      if (data->page >= 0x100)
	data->page = 0;
      data->page = (data->page<<4)+event->keyval-GDK_0;
      if (data->page > 0x899)
	data->page = 0x899;
      if (data->page >= 0x100)
	load_page(data->page, ANY_SUB, data);
      else
	{
	  buffer = g_strdup_printf("%d", hex2dec(data->page));
	  gtk_label_set_text(GTK_LABEL(lookup_widget(widget,
			     "ttxview_url")), buffer);
	  g_free(buffer);
	}
      break;
    case GDK_KP_0...GDK_KP_9:
      if (data->page >= 0x100)
	data->page = 0;
      data->page = (data->page<<4)+event->keyval-GDK_KP_0;
      if (data->page > 0x899)
	data->page = 0x899;
      if (data->page >= 0x100)
	load_page(data->page, ANY_SUB, data);
      else
	{
	  buffer = g_strdup_printf("%d", hex2dec(data->page));
	  gtk_label_set_text(GTK_LABEL(lookup_widget(widget,
			     "ttxview_url")), buffer);
	  g_free(buffer);
	}
      break;
    case GDK_Page_Down:
    case GDK_KP_Page_Down:
      if (data->page < 0x100)
	data->page = data->fmt_page->vtp->pgno + 0x10;
      else
	data->page += 0x10;
      if (data->page > 0x899)
	data->page = 0x100;
      load_page(data->page, ANY_SUB, data);
      break;
    case GDK_Page_Up:
    case GDK_KP_Page_Up:
      if (data->page < 0x100)
	data->page = data->fmt_page->vtp->pgno - 0x10;
      else
	data->page = data->page - 0x10;
      if (data->page < 0x100)
	data->page = 0x899;
      load_page(data->page, ANY_SUB, data);
      break;
    case GDK_KP_Up:
    case GDK_Up:
      if (data->page < 0x100)
	data->page = dec2hex(hex2dec(data->fmt_page->vtp->pgno) - 1);
      else
	data->page = dec2hex(hex2dec(data->page) - 1);
      if (data->page < 0x100)
	data->page = 0x899;
      load_page(data->page, ANY_SUB, data);
      break;
    case GDK_KP_Down:
    case GDK_Down:
      if (data->page < 0x100)
	data->page = dec2hex(hex2dec(data->fmt_page->vtp->pgno) + 1);
      else
	data->page = dec2hex(hex2dec(data->page) + 1);
      if (data->page > 0x899)
	data->page = 0x100;
      load_page(data->page, ANY_SUB, data);
      break;
    case GDK_KP_Left:
    case GDK_Left:
      on_ttxview_prev_sp_cache_clicked(GTK_BUTTON(lookup_widget(widget,
				     "ttxview_prev_sp_cache")), data);
      break;
    case GDK_KP_Right:
    case GDK_Right:
      on_ttxview_next_sp_cache_clicked(GTK_BUTTON(lookup_widget(widget,
				     "ttxview_next_sp_cache")), data);
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
  data->id = register_ttx_client();
  data->timeout =
    gtk_timeout_add(50, (GtkFunction)event_timeout, data);
  data->fmt_page = get_ttx_fmt_page(data->id);
  gtk_object_set_user_data(GTK_OBJECT(ttxview), data);

  /* Callbacks */
  gtk_signal_connect(GTK_OBJECT(ttxview), "delete-event",
		     GTK_SIGNAL_FUNC(on_ttxview_delete_event), data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview,
		      "ttxview_prev_subpage")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_prev_subpage_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview,
		      "ttxview_next_subpage")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_next_subpage_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview, "ttxview_home")),
		     "clicked", GTK_SIGNAL_FUNC(on_ttxview_home_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview, "ttxview_hold")),
		     "toggled", GTK_SIGNAL_FUNC(on_ttxview_hold_toggled),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview, "ttxview_clone")),
		     "clicked", GTK_SIGNAL_FUNC(on_ttxview_clone_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview, "ttxview_search")),
		     "clicked", GTK_SIGNAL_FUNC(on_ttxview_search_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview,
		     "ttxview_history_prev")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_history_prev_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview,
		     "ttxview_history_next")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_history_next_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview,
		     "ttxview_prev_sp_cache")), "clicked",
		     GTK_SIGNAL_FUNC(on_ttxview_prev_sp_cache_clicked),
		     data);
  gtk_signal_connect(GTK_OBJECT(lookup_widget(ttxview,
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
  gtk_signal_connect(GTK_OBJECT(ttxview),
		     "key-press-event",
		     GTK_SIGNAL_FUNC(on_ttxview_key_press), data);

  gtk_toolbar_set_style(GTK_TOOLBAR(lookup_widget(ttxview,
			  "toolbar2")),	GTK_TOOLBAR_ICONS);
  gtk_widget_set_usize(ttxview, 360, 400);
  gtk_widget_realize(ttxview);
  gdk_window_set_back_pixmap(data->da->window, NULL, FALSE);

  load_page(0x100, ANY_SUB, data);

  return (ttxview);
}
