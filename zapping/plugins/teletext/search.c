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

/* $Id: search.c,v 1.1 2004-09-22 21:29:07 mschimek Exp $ */

#define ZCONF_DOMAIN "/zapping/ttxview/"
#include "zconf.h"
#include "zmisc.h"
#include "zvbi.h"

#include "search.h"

static GdkCursor *	cursor_normal;
static GdkCursor *	cursor_busy;

/* XXX should be a signal. */
extern void
teletext_view_load_page		(TeletextView *		view,
				 vbi_pgno		pgno,
				 vbi_subno		subno,
				 vbi_page *		pg);

enum {
  SEARCH_RESPONSE_BACK = 1,
  SEARCH_RESPONSE_FORWARD,
};

static GObjectClass *parent_class;

/* Substitute keywords by regex, returns a newly allocated string. */
static gchar *
substitute			(const gchar *		string)
{
  static const gchar *search_keys [][2] = {
    { "#email#", "([:alnum:]|[-~.])+@([:alnum:]|[-~.])+" },
    { "#url#", "(https?://([:alnum:]|[-~./?%_=+])+)|(www.([:alnum:]|[-~./?%_=+])+)" }
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
  vbi_search_status status;
  vbi_page *pg;
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

  status = vbi_search_next (sp->context, &pg, sp->direction);

  switch (status)
    {
    case VBI_SEARCH_SUCCESS:
      sp->start_pgno = pg->pgno;
      sp->start_subno = pg->subno;
      if (sp->data)
	teletext_view_load_page (sp->data, pg->pgno, pg->subno, pg);
      result (sp, _("Found text on page %x.%02x:"), pg->pgno, pg->subno);
      call_again = FALSE;
      sp->searching = FALSE;
      break;

    case VBI_SEARCH_NOT_FOUND:
      result (sp, _("Not found:"));
      call_again = FALSE;
      sp->searching = FALSE;
      break;

    case VBI_SEARCH_CANCELED:
      /* Events pending, handle them and continue. */
      call_again = TRUE;
      break;

    case VBI_SEARCH_CACHE_EMPTY:
      result (sp, _("Page memory is empty"));
      call_again = FALSE;
      sp->searching = FALSE;
      break;

    default:
      g_message ("Unknown search status %d in %s",
		 status, __PRETTY_FUNCTION__);

      /* fall through */

    case VBI_SEARCH_ERROR:
      call_again = FALSE;
      sp->searching = FALSE;
      break;
    }

  return call_again;
}

static void
search_restart			(SearchDialog *		sp,
				 const gchar *		text,
				 vbi_pgno		start_pgno,
				 vbi_subno		start_subno,
				 gboolean		regexp,
				 gboolean		casefold)
{
  uint16_t *pattern;
  gchar *s;
  gchar *s1;
  guint i;

  g_free (sp->text);
  sp->text = g_strdup (text);

  zcs_bool (regexp, "ure_regexp");
  zcs_bool (casefold, "ure_casefold");

  s1 = substitute (text);

  /* I don't trust g_convert() to convert to the
     machine endian UCS2 we need, hence g_utf8_foo. */

  pattern = g_malloc (strlen (s1) * 2 + 2);

  i = 0;

  for (s = s1; *s; s = g_utf8_next_char (s))
    pattern[i++] = g_utf8_get_char (s);

  pattern[i] = 0;

  vbi_search_delete (sp->context);

  /* Progress callback: Tried first with, to permit the user cancelling
     a running search. But it seems there's a bug in libzvbi,
     vbi_search_next does not properly resume after the progress
     callback aborted to handle pending gtk events. Calling gtk main
     from callback is suicidal. Another bug: the callback lacks a
     user_data parameter. */
  sp->context = vbi_search_new (zvbi_get_object (),
				start_pgno,
				start_subno,
				pattern,
				casefold,
				regexp,
				/* progress */ NULL);
  g_free (pattern);

  g_free (s1);
}

static void
_continue			(SearchDialog *		sp,
				 gint			direction)
{
  const gchar *ctext;
  gchar *text;
  gboolean regexp;
  gboolean casefold;

  ctext = gtk_entry_get_text (GTK_ENTRY (sp->entry));

  if (!ctext || !*ctext)
    {
      /* Search for what? */
      gtk_window_present (GTK_WINDOW (sp));
      gtk_widget_grab_focus (sp->entry);
      return;
    }

  text = g_strdup (ctext);

  regexp = gtk_toggle_button_get_active (sp->regexp);
  casefold = gtk_toggle_button_get_active (sp->casefold);

  if (!sp->text
      || 0 != strcmp (sp->text, text))
    {
      search_restart (sp, text,
		      0x100, VBI_ANY_SUBNO,
		      regexp, casefold);
    }
  else if (casefold != zcg_bool (NULL, "ure_casefold")
	   || regexp != zcg_bool (NULL, "ure_regexp"))
    {
      search_restart (sp, text,
		      sp->start_pgno, sp->start_subno,
		      regexp, casefold);
    }

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
  /* XXX handle error */
  gnome_help_display ("zapping", "zapzilla-search", NULL); 
}

static void
instance_finalize		(GObject *		object)
{
  SearchDialog *t = SEARCH_DIALOG (object);

  if (t->searching)
    g_idle_remove_by_data (t);

  if (t->context)
    vbi_search_delete (t->context);

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
  t->start_subno = VBI_ANY_SUBNO;

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

  widget = gtk_check_button_new_with_mnemonic (_("_Regular expression"));
  t->regexp = GTK_TOGGLE_BUTTON (widget);
  gtk_toggle_button_set_active (t->regexp, zcg_bool (NULL, "ure_regexp"));
  gtk_box_pack_start (vbox, widget, FALSE, FALSE, 3);

  widget = gtk_check_button_new_with_mnemonic (_("Search case _insensitive"));
  t->casefold = GTK_TOGGLE_BUTTON (widget);
  gtk_toggle_button_set_active (t->casefold, zcg_bool (NULL, "ure_casefold"));
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
search_dialog_new		(void *		data)
{
  SearchDialog *sp;

  if (!zvbi_get_object())
    {
      ShowBox (_("VBI has been disabled"), GTK_MESSAGE_WARNING);
      return NULL;
    }

  sp = (SearchDialog *) g_object_new (TYPE_SEARCH_DIALOG, NULL);

  sp->data = data;

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
