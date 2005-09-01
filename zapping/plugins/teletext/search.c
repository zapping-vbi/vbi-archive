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

/* $Id: search.c,v 1.5 2005-09-01 01:28:59 mschimek Exp $ */

#include "src/zgconf.h"
#include "main.h"		/* td */
#include "search.h"

enum {
  SEARCH_RESPONSE_BACK = 1,
  SEARCH_RESPONSE_FORWARD,
};

#define GCONF_DIR "/apps/zapping/plugins/teletext"

static GObjectClass *		parent_class;

static GdkCursor *		cursor_normal;
static GdkCursor *		cursor_busy;

/* Substitute keywords by regex, returns a newly allocated string. */
static gchar *
substitute			(const gchar *		string)
{
  static const gchar *search_keys [][2] = {
    { "#email#", "([:alnum:]|[-~.])+@([:alnum:]|[-~.])+" },
    { "#url#", "(https?://([:alnum:]|[-~./?%_=+])+)|"
               "(www.([:alnum:]|[-~./?%_=+])+)" }
  };
  gchar *s;
  guint i;

  if (!string || !*string)
    return g_strdup ("");
  else
    s = g_strdup (string);

  for (i = 0; i < 2; ++i)
    {
      gchar *found;

      while ((found = strstr (s, search_keys[i][0])))
	{
	  gchar *s1;

	  *found = 0;

	  s1 = g_strconcat (s, search_keys[i][1],
			    found + strlen (search_keys[i][0]), NULL);
	  g_free (s);
	  s = s1;
	}
    }

  return s;
}

static void
result				(SearchDialog *		sp,
				 const gchar *		format,
				 ...)
{
  gchar *buffer;
  va_list args;

#if 1
  gdk_window_set_cursor (GTK_WIDGET (sp)->window, cursor_normal);
  gtk_widget_set_sensitive (GTK_WIDGET (sp), TRUE);
#else
  gtk_widget_set_sensitive (sp->entry, TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (sp->regexp), TRUE);
  gtk_widget_set_sensitive (GTK_WIDGET (sp->casefold), TRUE);
  gtk_widget_set_sensitive (sp->back, TRUE);
  gtk_widget_set_sensitive (sp->forward, TRUE);

  if (sp->direction < 0)
    gtk_dialog_set_default_response (GTK_DIALOG (sp), SEARCH_RESPONSE_BACK);
  else
    gtk_dialog_set_default_response (GTK_DIALOG (sp), SEARCH_RESPONSE_FORWARD);
#endif

  va_start (args, format);
  buffer = g_strdup_vprintf (format, args);
  va_end (args);

  gtk_label_set_text (sp->label, buffer);

  g_free (buffer);
}

static gboolean
idle				(gpointer		user_data)
{
  SearchDialog *sp = user_data;
  vbi3_search_status status;
  const vbi3_page *pg;
  gboolean call_again;

#if 1
  gdk_window_set_cursor (GTK_WIDGET (sp)->window, cursor_busy);
  gtk_widget_set_sensitive (GTK_WIDGET (sp), FALSE);
#else
  gtk_widget_set_sensitive (sp->entry, FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (sp->regexp), FALSE);
  gtk_widget_set_sensitive (GTK_WIDGET (sp->casefold), FALSE);
  gtk_widget_set_sensitive (sp->back, FALSE);
  gtk_widget_set_sensitive (sp->forward, FALSE);

  gtk_dialog_set_default_response (GTK_DIALOG (sp), GTK_RESPONSE_CANCEL);
#endif

  gtk_label_set_text (sp->label, _("Search text:"));

  /* XXX shouldn't we have a few format options here? */
  status = vbi3_search_next (sp->context, &pg, sp->direction,
			     VBI3_END);

  switch (status)
    {
    case VBI3_SEARCH_SUCCESS:
      sp->start_pgno = pg->pgno;
      sp->start_subno = pg->subno;

      if (sp->view)
	{
	  vbi3_page *pg2;

	  pg2 = vbi3_page_dup (pg);
	  g_assert (NULL != pg2);

	  sp->view->show_page (sp->view, pg2);
	}

      result (sp, _("Found text on page %x.%02x:"), pg->pgno, pg->subno);

      call_again = FALSE;
      sp->searching = FALSE;
      break;

    case VBI3_SEARCH_NOT_FOUND:
      result (sp, _("Not found:"));
      call_again = FALSE;
      sp->searching = FALSE;
      break;

    case VBI3_SEARCH_ABORTED:
      /* Events pending, handle them and continue. */
      call_again = TRUE;
      break;

    case VBI3_SEARCH_CACHE_EMPTY:
      result (sp, _("Page memory is empty"));
      call_again = FALSE;
      sp->searching = FALSE;
      break;

    default:
      g_message ("Unknown search status %d in %s",
		 status, __PRETTY_FUNCTION__);

      /* fall through */

    case VBI3_SEARCH_ERROR:
      call_again = FALSE;
      sp->searching = FALSE;
      break;
    }

  return call_again;
}

static void
search_restart			(SearchDialog *		sp,
				 const gchar *		text,
				 vbi3_pgno		start_pgno,
				 vbi3_subno		start_subno,
				 gboolean		regexp,
				 gboolean		casefold,
				 gboolean		all_channels _unused_)
{
  vbi3_teletext_decoder *td;
  const vbi3_network *nk;
  gchar *pattern;

  g_free (sp->text);
  sp->text = g_strdup (text);

  pattern = substitute (text);

  vbi3_search_delete (sp->context);

  nk = &sp->view->req.network;
  if (vbi3_network_is_anonymous (nk))
    nk = NULL; /* use received */

  g_assert (NULL != sp->view->vbi);
  td = vbi3_decoder_cast_to_teletext_decoder (sp->view->vbi);

  /* Progress callback: Tried first with, to permit the user cancelling
     a running search. But it seems there's a bug in libzvbi,
     vbi3_search_next does not properly resume after the progress
     callback aborted to handle pending gtk events. Calling gtk main
     from callback is suicidal. Another bug: the callback lacks a
     user_data parameter. */
  sp->context = vbi3_teletext_decoder_search_utf8_new
    (td, nk, start_pgno, start_subno,
     pattern, casefold, regexp,
     /* progress */ NULL, /* user_data */ NULL);

  g_free (pattern);
}

static void
_continue			(SearchDialog *		sp,
				 gint			direction)
{
  gboolean regexp = TRUE;
  gboolean casefold = FALSE;
  gboolean all_channels = FALSE;
  const gchar *ctext;
  gchar *text;

  ctext = gtk_entry_get_text (GTK_ENTRY (sp->entry));

  if (!ctext || !*ctext)
    {
      /* Search for what? */
      gtk_window_present (GTK_WINDOW (sp));
      gtk_widget_grab_focus (sp->entry);
      return;
    }

  text = g_strdup (ctext);

  /* Error ignored. */
  z_gconf_get_bool (&regexp, GCONF_DIR "/search/regexp");
  z_gconf_get_bool (&casefold, GCONF_DIR "/search/casefold");
  z_gconf_get_bool (&all_channels, GCONF_DIR "/search/all_channels");

  if (!sp->text || 0 != strcmp (sp->text, text))
    {
      search_restart (sp, text, 0x100, VBI3_ANY_SUBNO,
		      regexp, casefold, all_channels);
    }
  else if (regexp != sp->regexp
	   || casefold != sp->casefold
	   || all_channels != sp->all_channels)
    {
      search_restart (sp, text, sp->start_pgno, sp->start_subno,
		      regexp, casefold, all_channels);
    }

  sp->regexp = regexp;
  sp->casefold = casefold;
  sp->all_channels = all_channels;

  g_free (text);

  sp->direction = direction;

  g_idle_add (idle, sp);

  sp->searching = TRUE;
}

static void
on_next_clicked			(GtkButton *		button _unused_,
				 SearchDialog *		sp)
{
  _continue (sp, +1);
}

static void
on_prev_clicked			(GtkButton *		button _unused_,
				 SearchDialog *		sp)
{
  _continue (sp, -1);
}

static void
on_cancel_clicked		(GtkWidget *		button _unused_,
				 SearchDialog *		sp)
{
  gtk_widget_destroy (GTK_WIDGET (sp));
}

static void
on_help_clicked			(GtkWidget *		button _unused_,
				 SearchDialog *		sp _unused_)
{
  z_help_display (GTK_WINDOW (sp), "zapping", "zapzilla-search");
}

static void
instance_finalize		(GObject *		object)
{
  SearchDialog *t = SEARCH_DIALOG (object);

  if (t->searching)
    g_idle_remove_by_data (t);

  if (t->context)
    vbi3_search_delete (t->context);

  g_free (t->text);

  parent_class->finalize (object);
}
    
static void
instance_init			(GTypeInstance *	instance,
				 gpointer		g_class _unused_)
{
  SearchDialog *t = (SearchDialog *) instance;
  GtkWindow *window;
  GtkWidget *widget;
  GtkBox *vbox;

  t->start_pgno = 0x100;
  t->start_subno = VBI3_ANY_SUBNO;

  window = GTK_WINDOW (t);
  gtk_window_set_title (window, _("Search page memory"));

  widget = gtk_vbox_new (FALSE, 0);
  vbox = GTK_BOX (widget);
  gtk_container_set_border_width (GTK_CONTAINER (widget), 6);
  gtk_box_pack_start (GTK_BOX (t->dialog.vbox), widget, TRUE, TRUE, 0);

  widget = gtk_label_new (_("Search text:"));
  t->label = GTK_LABEL (widget);
  gtk_box_pack_start (vbox, widget, FALSE, FALSE, 3);

  widget = gnome_entry_new ("ure_search_history");
  t->entry = gnome_entry_gtk_entry (GNOME_ENTRY (widget));
  gtk_entry_set_activates_default (GTK_ENTRY (t->entry), TRUE);
  gtk_box_pack_start (vbox, widget, FALSE, FALSE, 3);

  widget = z_gconf_check_button_new (_("_Regular expression"),
				     GCONF_DIR "/search/regexp",
				     NULL, TRUE);
  gtk_box_pack_start (vbox, widget, FALSE, FALSE, 3);

  widget = z_gconf_check_button_new (_("Search case _insensitive"),
				     GCONF_DIR "/search/casefold",
				     NULL, FALSE);
  gtk_box_pack_start (vbox, widget, FALSE, FALSE, 3);

  /* Future stuff. */
  widget = z_gconf_check_button_new (_("_All channels"),
				     GCONF_DIR "/search/all_channels",
				     NULL, FALSE);
  gtk_widget_set_sensitive (widget, FALSE);
  gtk_box_pack_start (vbox, widget, FALSE, FALSE, 3);

  widget = gtk_button_new_from_stock (GTK_STOCK_HELP);
  gtk_dialog_add_action_widget (&t->dialog, widget, GTK_RESPONSE_HELP);
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (on_help_clicked), t);

  widget = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
  gtk_dialog_add_action_widget (&t->dialog, widget, GTK_RESPONSE_CANCEL);
  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (on_cancel_clicked), t);

  widget = gtk_button_new_from_stock (GTK_STOCK_GO_BACK);
  t->back = widget;
  gtk_dialog_add_action_widget (&t->dialog, widget, SEARCH_RESPONSE_BACK);
  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (on_prev_clicked), t);

  widget = gtk_button_new_from_stock (GTK_STOCK_GO_FORWARD);
  t->forward = widget;
  gtk_dialog_add_action_widget (&t->dialog, widget, SEARCH_RESPONSE_FORWARD);
  GTK_WIDGET_SET_FLAGS (widget, GTK_CAN_DEFAULT);
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (on_next_clicked), t);

  gtk_dialog_set_default_response (&t->dialog, SEARCH_RESPONSE_FORWARD);

  gtk_widget_grab_focus (t->entry);
}

GtkWidget *
search_dialog_new		(TeletextView *		view)
{
  SearchDialog *sp;

  sp = (SearchDialog *) g_object_new (TYPE_SEARCH_DIALOG, NULL);

  sp->view = view;

  g_signal_connect_swapped (G_OBJECT (view), "destroy",
			    G_CALLBACK (gtk_widget_destroy), sp);

  return GTK_WIDGET (sp);
}

static void
class_init			(gpointer		g_class,
				 gpointer		class_data _unused_)
{
  GObjectClass *object_class;
  SearchDialogClass *c;

  object_class = G_OBJECT_CLASS (g_class);
  parent_class = g_type_class_peek_parent (object_class);

  object_class->finalize = instance_finalize;

  c = SEARCH_DIALOG_CLASS (g_class);

  cursor_normal = gdk_cursor_new (GDK_LEFT_PTR);
  cursor_busy = gdk_cursor_new (GDK_WATCH);
}

GType
search_dialog_get_type		(void)
{
  static GType type = 0;
  
  if (!type)
    {
      GTypeInfo info;

      CLEAR (info);

      info.class_size = sizeof (SearchDialogClass);
      info.class_init = class_init;
      info.instance_size = sizeof (SearchDialog);
      info.instance_init = instance_init;

      type = g_type_register_static (GTK_TYPE_DIALOG,
				     "SearchDialog",
				     &info, (GTypeFlags) 0);
    }

  return type;
}
