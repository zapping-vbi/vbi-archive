/* Zapping (TV viewer for the Gnome Desktop)
 * Copyright (C) 2002 Iñaki García Etxebarria
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

/* $Id: channel_editor.c,v 1.40 2004-05-17 20:46:52 mschimek Exp $ */

/*
  TODO:
  * input type icon
  * dnd
  * write lock channel list
  * notify other modules (ttx bookmarks, schedule etc) about changes
  * device column
  * wizard
  * channel merging e.g.
    12 FooBar +              key
       + dev1 tuner E5 fine  key
       + dev2 tuner E5 fine  key
 */

/* XXX gtk+ 2.3 GtkOptionMenu */
#undef GTK_DISABLE_DEPRECATED

#include "site_def.h"
#include "config.h"

#include <gnome.h>

#define ZCONF_DOMAIN "/zapping/internal/properties/"
#include "zconf.h"
#include "zmisc.h"
#include "remote.h"
#include "frequencies.h"
#include "globals.h"
#include "zvbi.h"
#include "v4linterface.h"
#include "i18n.h"
#include "xawtv.h"

#include "channel_editor.h"

typedef struct station_search station_search;

struct station_search
{
  GtkDialog *		station_search;
  GtkLabel *		label;
  GtkProgressBar *	progressbar;

  guint			timeout_handle;

  tv_rf_channel		ch;
  guint			channel;

  guint			found;
  guint			iteration;
  guint			frequ;
  gint			strength;
  gint			afc;
};

struct country {
  gchar *		table_name;
  gchar *		gui_name;
};

enum {
  FL_NAME,
  FL_FREQ,
  FL_NUM_COLUMNS
};

typedef struct channel_editor channel_editor;

struct channel_editor
{
  GtkDialog *		channel_editor;

  GtkBox *		vbox;

  GtkOptionMenu *	country_menu;
  GArray *		country_table;

  GtkButton *		channel_search;
  GtkButton *		add_all_channels;
  GtkButton *		import_xawtv;

  GtkTreeView *		freq_treeview;
  GtkListStore *	freq_model;
  GtkTreeSelection *	freq_selection;

  GtkTreeView *		channel_treeview;
  GtkListStore *	channel_model;
  GtkTreeSelection *	channel_selection;

  GtkWidget *		channel_up;
  GtkWidget *		channel_down;
  GtkWidget *		channel_add;
  GtkWidget *		channel_remove;

  GtkTable *		entry_table;
  GtkEntry *		entry_name;
  GtkWidget *		entry_fine_tuning;		/* spinslider */
  GtkAdjustment *	spinslider_adj;
  GtkOptionMenu *	entry_standard;
  GtkOptionMenu *	entry_input;
  GtkWidget *		entry_accel;			/* z_key_entry */

  GtkTooltips *		tooltips;

  tveng_tuned_channel *	old_channel_list;

  station_search *	search;

  gboolean		have_tuners;
};

#define DONT_CHANGE 0

#define BLOCK(object, signal, statement)				\
  SIGNAL_HANDLER_BLOCK(ce->object,					\
    (gpointer) on_ ## object ## _ ## signal, statement)

static GtkMenu *
create_standard_menu		(channel_editor *	ce);

static GtkListStore *
create_freq_list_model		(const tv_rf_channel *	table);

static void
on_channel_selection_changed	(GtkTreeSelection *	selection,
				 channel_editor *	ce);

static channel_editor *		dialog_running;

/*
 *  Misc helpers
 */

static gboolean
tunable_input			(channel_editor *	ce,
				 const tveng_device_info *info,
				 const tveng_tuned_channel *tc)
{
  const tv_video_line *l;

  if (!ce->have_tuners)
    return FALSE;

  if (tc->input == DONT_CHANGE)
    return TRUE;

  l = tv_video_input_by_hash ((tveng_device_info *) info, tc->input);

  return (l && l->type == TV_VIDEO_LINE_TYPE_TUNER);
}

#define VALID_ITER(iter, list_store)					\
  ((iter) != NULL							\
   && (iter)->user_data != NULL						\
   && ((GTK_LIST_STORE (list_store))->stamp == (iter)->stamp))

static inline guint
tree_model_index		(GtkTreeModel *		model,
				 GtkTreeIter *		iter)
{
  GtkTreePath *path;
  guint row;

  path = gtk_tree_model_get_path (model, iter);

  row = gtk_tree_path_get_indices (path)[0];

  gtk_tree_path_free (path);

  return row;
}

static inline tveng_tuned_channel *
tree_model_tuned_channel	(GtkTreeModel *		model,
				 GtkTreeIter *		iter)
{
  return tveng_tuned_channel_nth (global_channel_list,
				  tree_model_index (model, iter));
}

/* function where are thou? */
static gboolean
tree_model_get_iter_last	(GtkTreeModel *		model,
				 GtkTreeIter *		iter)
{
  GtkTreeIter iter2;

  if (!gtk_tree_model_get_iter_first (model, iter))
    return FALSE;

  iter2 = *iter;

  do *iter = iter2;
  while (gtk_tree_model_iter_next (model, &iter2));

  return TRUE;
}

static gboolean
tree_model_iter_prev		(GtkTreeModel *		model,
				 GtkTreeIter *		iter)
{
  GtkTreePath *path;
  gboolean r;

  path = gtk_tree_model_get_path (model, iter);

  if ((r = gtk_tree_path_prev (path)))
    gtk_tree_model_get_iter (model, iter, path);

  gtk_tree_path_free (path);

  return r;
}

/*
 *  Channel list helpers
 */

static inline guint
channel_list_index		(const channel_editor *	ce,
				 GtkTreeIter *		iter)
{
  return tree_model_index (GTK_TREE_MODEL (ce->channel_model), iter);
}

static void
channel_list_scroll_to_cell	(const channel_editor *	ce,
				 GtkTreeIter *		iter,
				 gfloat			row_align)
{
  GtkTreePath *path;

  if ((path = gtk_tree_model_get_path (GTK_TREE_MODEL (ce->channel_model), iter)))
    {
      gtk_tree_view_scroll_to_cell (ce->channel_treeview, path, NULL,
				    /* use_align */ TRUE, row_align, 0.0);
      gtk_tree_path_free (path);
    }
}

static void
channel_list_rows_changed	(channel_editor *	ce,
				 GtkTreeIter *		first_iter,
				 GtkTreeIter *		last_iter)
{
  GtkTreeModel *model = GTK_TREE_MODEL (ce->channel_model);
  GtkTreeIter iter;
  GtkTreePath *path, *last_path;

  if (!last_iter)
    last_iter = first_iter;

  iter = *first_iter;

  path = gtk_tree_model_get_path (model, first_iter);
  last_path = gtk_tree_model_get_path (model, last_iter);

  do
    {
      gtk_tree_model_row_changed (model, path, &iter);

      if (!gtk_tree_model_iter_next (model, &iter))
	break;

      gtk_tree_path_next (path);
    }
  while (gtk_tree_path_compare (path, last_path) <= 0);

  gtk_tree_path_free (last_path);
  gtk_tree_path_free (path);
}

static tveng_tuned_channel *
channel_list_get_tuned_channel	(const channel_editor *	ce,
				 GtkTreeIter *		iter)
{
  tveng_tuned_channel *tc;
  guint index;

  index = channel_list_index (ce, iter);

  tc = tveng_tuned_channel_nth (global_channel_list, index);

  g_assert (tc != NULL);

  return tc;
}

static gboolean
channel_list_get_selection	(const channel_editor *	ce,
				 GtkTreeIter *		iter_first,
				 GtkTreeIter *		iter_last,
				 tveng_tuned_channel **	tc_first,
				 tveng_tuned_channel **	tc_last)
{
  GtkTreeIter iter, last;

  if (!z_tree_selection_iter_first (ce->channel_selection,
				    GTK_TREE_MODEL (ce->channel_model), &iter))
    return FALSE; /* nothing selected */

  if (iter_first)
    *iter_first = iter;

  if (tc_first)
    *tc_first = channel_list_get_tuned_channel (ce, &iter);

  if (iter_last || tc_last)
    {
      if (!iter_last)
	iter_last = &last;
      
      do
	{
	  *iter_last = iter;
	  
	  if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (ce->channel_model), &iter))
	    break;
	}
      while (gtk_tree_selection_iter_is_selected (ce->channel_selection, &iter));
      
      if (tc_last)
	*tc_last = channel_list_get_tuned_channel (ce, iter_last);
    }

  return TRUE;
}

static gchar *
rf_channel_string		(const tveng_tuned_channel *tc)
{
  return g_strdup_printf ("%s  %.2f MHz", tc->rf_name, tc->frequ / 1e6);
}

static void
channel_list_add_tuned_channel	(channel_editor *	ce,
				 tveng_tuned_channel **	list,
				 const tveng_tuned_channel *tc)
{
  GtkTreeModel *model = GTK_TREE_MODEL (ce->channel_model);
  GtkTreeIter iter;
  GtkTreePath *path;
  tveng_tuned_channel *tci;
  guint i;

  /*
   *  Don't add when this channel (by tuner freq) is already
   *  listed. Eventually update the station name.
   */
  for (i = 0; (tci = tveng_tuned_channel_nth (*list, i)); i++)
    {
      if (tci->input != tc->input)
	continue;

      if (tunable_input (ce, main_info, tc)
	  && abs (tci->frequ - tc->frequ) > 3000000)
	continue;

      gtk_tree_model_iter_nth_child (model, &iter, NULL, i);
      path = gtk_tree_model_get_path (model, &iter);
      gtk_tree_view_scroll_to_cell (ce->channel_treeview, path, NULL,
                                    /* use_align */ TRUE, 0.5, 0.0);
      gtk_tree_path_free (path);

      if (0 != strcmp (tci->name, tci->rf_name))
	return; /* user changed station name */

      if (0 == strcmp (tc->name, tci->rf_name))
	return; /* we have no station name */
      /*
      gtk_list_store_set (ce->channel_model, &iter,
			  CL_NAME, tc->name, -1);
      */
      g_free (tci->name);
      tci->name = g_strdup (tc->name);

      return;
    }

  tveng_tuned_channel_insert (list, tveng_tuned_channel_new (tc), G_MAXINT);

  gtk_list_store_append (ce->channel_model, &iter);

  channel_list_scroll_to_cell (ce, &iter, 0.5);
}

/*
 *  Dialog helpers
 */

static void
entry_fine_tuning_set		(channel_editor *	ce,
				 const tveng_device_info *info,
				 guint			frequency)
{
  GtkAdjustment *spin_adj = z_spinslider_get_spin_adj (ce->entry_fine_tuning);
  GtkAdjustment *hscale_adj = z_spinslider_get_hscale_adj (ce->entry_fine_tuning);

  if (frequency > 0)
    {
      double dfreq;

      frequency += 500;
      frequency -= frequency % 1000;

      dfreq = frequency * 1e-6; /* MHz */

      spin_adj->value = dfreq;
      spin_adj->lower = 5;
      spin_adj->upper = 1999;
      spin_adj->step_increment = 0.05;
      spin_adj->page_increment = 1;
      spin_adj->page_size = 0;

      hscale_adj->value = dfreq;
      hscale_adj->lower = dfreq - 4;
      hscale_adj->upper = dfreq + 4;
      hscale_adj->step_increment = 0.05; /* XXX use tv_video_line.u.tuner.step ? */
      hscale_adj->page_increment = 1;
      hscale_adj->page_size = 0;

      gtk_adjustment_changed (spin_adj);
      gtk_adjustment_value_changed (spin_adj);
      gtk_adjustment_changed (hscale_adj);
      gtk_adjustment_value_changed (hscale_adj);

      z_spinslider_set_reset_value (ce->entry_fine_tuning, dfreq);
    }

  if (frequency == 0
      || !info->cur_video_input
      || info->cur_video_input->type != TV_VIDEO_LINE_TYPE_TUNER)
    gtk_widget_set_sensitive (ce->entry_fine_tuning, FALSE);
  else
    gtk_widget_set_sensitive (ce->entry_fine_tuning, TRUE);
}

static void
no_channel_selected		(channel_editor *	ce)
{
  gtk_widget_set_sensitive (ce->channel_up, FALSE);
  gtk_widget_set_sensitive (ce->channel_down, FALSE);
  gtk_widget_set_sensitive (ce->channel_remove, FALSE);

  gtk_entry_set_text (ce->entry_name, "");
  z_option_menu_set_active (GTK_WIDGET (ce->entry_standard), 0);
  z_option_menu_set_active (GTK_WIDGET (ce->entry_input), 0);
  z_key_entry_set_key (ce->entry_accel, Z_KEY_NONE);
  gtk_widget_set_sensitive (GTK_WIDGET (ce->entry_table), FALSE);  
}

static void
channel_buttons_set_sensitive	(channel_editor *	ce,
				 gboolean		any_selected)
{
  GtkTreeIter iter;

  if (!gtk_tree_model_get_iter_first
      (GTK_TREE_MODEL (ce->channel_model), &iter))
    {
      no_channel_selected (ce);
      return;
    }

  gtk_widget_set_sensitive (ce->channel_up,
			    !gtk_tree_selection_iter_is_selected
			    (ce->channel_selection, &iter));

  tree_model_get_iter_last (GTK_TREE_MODEL (ce->channel_model), &iter);

  gtk_widget_set_sensitive (ce->channel_down,
			    !gtk_tree_selection_iter_is_selected
			    (ce->channel_selection, &iter));

  gtk_widget_set_sensitive (ce->channel_remove, any_selected);
}

/*
 *  Signals
 */

static void
current_rf_channel_table	(channel_editor *	ce,
				 tv_rf_channel *	ch,
				 const gchar **		rf_table)
{
  struct country *c;
  guint i;

  i = z_option_menu_get_active (GTK_WIDGET (ce->country_menu));
  c = &g_array_index (ce->country_table, struct country, i);

  if (rf_table)
    *rf_table = c->table_name;

  if (!tv_rf_channel_table_by_name (ch, c->table_name))
    g_assert_not_reached ();
}

static void
on_country_menu_changed		(GtkOptionMenu *	country_menu,
				 channel_editor *	ce)
{
  tv_rf_channel ch;
  const gchar *rf_table;

  current_rf_channel_table (ce, &ch, &rf_table);

  zconf_set_string (rf_table, "/zapping/options/main/current_country");

  zconf_set_integer (tv_rf_channel_align (&ch) ? 1 : 0,
		     "/zapping/options/main/channel_txl");

  ce->freq_model = create_freq_list_model (&ch);
  gtk_tree_view_set_model (ce->freq_treeview, GTK_TREE_MODEL (ce->freq_model));
}

static void
on_station_search_cancel_clicked (GtkButton *		cancel,
				  channel_editor *	ce)
{
  if (ce->search)
    gtk_widget_destroy (GTK_WIDGET (ce->search->station_search));
}

static void
on_station_search_destroy	(GtkObject *		unused,
				 channel_editor *	ce)
{
  if (ce->search)
    {
      gtk_timeout_remove (ce->search->timeout_handle);

      g_free (ce->search);
      ce->search = NULL;
    }

  gtk_widget_set_sensitive (GTK_WIDGET (ce->vbox), TRUE);
}

static gboolean
station_search_timeout		(gpointer		p)
{
  channel_editor *ce = p;
  station_search *cs = ce->search;
  tveng_tuned_channel tc;
  gchar *station_name;
  gint strength, afc;

  if (!(cs = ce->search))
    return FALSE;

  if (cs->iteration == 0)
    {
      /* New channel */

      gtk_progress_bar_set_fraction (cs->progressbar,
	 cs->channel / (gdouble) tv_rf_channel_table_size (&cs->ch));

      z_label_set_text_printf (cs->label,
			       _("Channel: %s   Found: %u"),
			       cs->ch.channel_name, cs->found);

      cs->frequ = cs->ch.frequency;
      cs->strength = 0;

      if (!tv_set_tuner_frequency (main_info, cs->frequ))
	goto next_channel;

#ifdef HAVE_LIBZVBI
      /* zvbi should store the station name if known from now */
      zvbi_name_unknown();
#endif
      cs->iteration = 1;
    }
  else
    {
      /* Probe */

      if (-1 == tveng_get_signal_strength (&strength, &afc, main_info))
	goto next_channel;

      if (strength > 0)
	{
	  cs->strength = strength;
	}
      else if (cs->iteration >= 5
	       && cs->strength == 0)
	{
	  goto next_channel; /* no signal after 0.5 sec */
	}

      if (afc && (afc != -cs->afc))
	{
	  cs->afc = afc;
	  cs->frequ += afc * 25000; /* should be afc*50000, but won't harm */

	  /* error ignored */
	  tv_set_tuner_frequency (main_info, cs->frequ);
	}

#ifdef HAVE_LIBZVBI
      /* if (zconf_get_boolean(NULL, "/zapping/options/vbi/use_vbi")) */
      if (1)
	{
	  if ((station_name = zvbi_get_name ()))
	    goto add_station;

	  /* How long for XDS? */
	  if (cs->iteration >= 25)
	    goto add_default; /* no name after 2.5 sec */
	}
      else
#endif
	if (cs->iteration >= 10)
	  goto add_default; /* after 1 sec afc */

      cs->iteration++;
    }

  return TRUE; /* continue */

 add_default:
  station_name = g_strdup (cs->ch.channel_name);

 add_station:
  CLEAR (tc);
  tc.name	= station_name;
  tc.rf_name	= (gchar *) cs->ch.channel_name;
  tc.rf_table	= (gchar *) cs->ch.table_name;
  tc.frequ	= cs->frequ;

  channel_list_add_tuned_channel (ce, &global_channel_list, &tc);

  g_free (station_name);
  cs->found++;

 next_channel:
  cs->channel++;
  cs->iteration = 0;

  if (!tv_rf_channel_next (&cs->ch))
    {
      gtk_widget_destroy (GTK_WIDGET (cs->station_search));
      return FALSE; /* remove timer */
    }

  return TRUE; /* continue */
}

static void
on_channel_search_clicked	(GtkButton *		search,
				 channel_editor *	ce)
{
  station_search *cs;
  GtkWidget *dialog_vbox;
  GtkWidget *vbox;
  GtkWidget *dialog_action_area;
  GtkWidget *cancel;
  const tv_video_line *l;

  if (ce->search)
    return;

  /* XXX we cannot search in Xv mode because there's no signal strength.
     Or is there? tveng should also tell in advance if this call will
     fail, so we can disable the option. */
  if (0 != zmisc_switch_mode (TVENG_CAPTURE_READ, main_info))
    return;

  cs = g_malloc (sizeof (station_search));
  ce->search = cs;
  cs->found = 0;

  cs->station_search = GTK_DIALOG (gtk_dialog_new ());
  gtk_window_set_title (GTK_WINDOW (cs->station_search), _("Searching..."));
  g_signal_connect (G_OBJECT (cs->station_search), "destroy",
                    G_CALLBACK (on_station_search_destroy), ce);

  dialog_vbox = cs->station_search->vbox;
  gtk_widget_show (dialog_vbox);

  vbox = gtk_vbox_new (FALSE, 3);
  gtk_widget_show (vbox);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), vbox, TRUE, TRUE, 0);

  cs->label = GTK_LABEL (gtk_label_new (""));
  gtk_widget_show (GTK_WIDGET (cs->label));
  gtk_box_pack_start (GTK_BOX (vbox),
		      GTK_WIDGET (cs->label),
		      FALSE, FALSE, 0);

  cs->progressbar = GTK_PROGRESS_BAR (gtk_progress_bar_new ());
  gtk_widget_show (GTK_WIDGET (cs->progressbar));
  gtk_progress_bar_set_fraction (cs->progressbar, 0.0);
  gtk_box_pack_start (GTK_BOX (vbox),
		      GTK_WIDGET (cs->progressbar),
		      FALSE, FALSE, 0);

  dialog_action_area = cs->station_search->action_area;
  gtk_widget_show (dialog_action_area);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

  cancel = gtk_button_new_from_stock (GTK_STOCK_STOP);
  gtk_widget_show (cancel);
  gtk_dialog_add_action_widget (cs->station_search, cancel, 0);
  g_signal_connect (G_OBJECT (cancel), "clicked",
		    G_CALLBACK (on_station_search_cancel_clicked), ce);

  gtk_widget_show (GTK_WIDGET (cs->station_search));

  gtk_widget_set_sensitive (GTK_WIDGET (ce->vbox), FALSE);

  current_rf_channel_table (ce, &cs->ch, NULL);
  cs->channel = 0;
  cs->iteration = 0;

  for (l = tv_next_video_input (main_info, NULL);
       l; l = tv_next_video_input (main_info, l))
    if (l->type == TV_VIDEO_LINE_TYPE_TUNER)
      break;

  g_assert (l != NULL);

  tv_set_video_input (main_info, l);
  /* XXX consider multiple tuners */

  cs->timeout_handle = gtk_timeout_add (100 /* ms */,
					station_search_timeout, ce);
}

static void
on_add_all_channels_clicked	(GtkButton *		add_all_channels,
				 channel_editor *	ce)
{
  tv_rf_channel ch;
  tveng_tuned_channel tc;
  gboolean align;

  CLEAR (tc);

  current_rf_channel_table (ce, &ch, NULL);

  align = tv_rf_channel_align (&ch);

  if (align)
    {
      GtkTreeIter iter;
      gint added;

      added = tveng_tuned_channel_num (global_channel_list);

      do
	if (g_ascii_isdigit (ch.channel_name[0]))
	  {
	    tc.name	= (gchar *) ch.channel_name;
	    tc.rf_name	= (gchar *) ch.channel_name;
	    tc.rf_table	= (gchar *) ch.table_name;
	    tc.frequ	= ch.frequency;

	    tveng_tuned_channel_replace (&global_channel_list,
					 tveng_tuned_channel_new (&tc),
					 atoi (ch.channel_name));
	  }
      while (tv_rf_channel_next (&ch));

      added = tveng_tuned_channel_num (global_channel_list) - added;

      while (added-- > 0)
	gtk_list_store_append (ce->channel_model, &iter);

      tv_rf_channel_first (&ch);
    }

  do
    if (!align || !g_ascii_isdigit (ch.channel_name[0]))
      {
	tc.name		= (gchar *) ch.channel_name;
	tc.rf_name	= (gchar *) ch.channel_name;
	tc.rf_table	= (gchar *) ch.table_name;
	tc.frequ	= ch.frequency;

	channel_list_add_tuned_channel (ce, &global_channel_list, &tc);
      }
  while (tv_rf_channel_next (&ch));
}

static void
on_import_xawtv_clicked		(GtkButton *		add_all_channels,
				 channel_editor *	ce)
{
  GtkTreeIter iter;
  guint i;

  /* XXX error ignored */
  xawtv_import_config (main_info, &global_channel_list);

  gtk_list_store_clear (ce->channel_model);

  for (i = tveng_tuned_channel_num (global_channel_list); i > 0; --i)
    gtk_list_store_append (ce->channel_model, &iter);
}

static void
on_freq_selection_changed	(GtkTreeSelection *	selection,
				 channel_editor *	ce)
{
  GtkTreeIter first, last;
  GtkTreeIter freq_iter;
  tveng_tuned_channel *tc, *tc_first, *tc_last;
  tv_rf_channel ch;
  gchar *name;
  gboolean success;

  if (!gtk_tree_selection_get_selected (selection, NULL, &freq_iter))
    return;

  if (!channel_list_get_selection (ce, &first, &last, &tc_first, &tc_last))
    return;

  gtk_tree_model_get (GTK_TREE_MODEL (ce->freq_model), &freq_iter,
		      FL_NAME, &name, -1);

  current_rf_channel_table (ce, &ch, NULL);
  success = tv_rf_channel_by_name (&ch, name);

  g_free (name);

  if (!success)
    return;

  for (tc = tc_first;; tc = tc->next)
    {
      if (0 != strcmp (tc->rf_table, ch.table_name))
	{
	  g_free (tc->rf_table);
	  tc->rf_table = g_strdup (ch.table_name);
	}

      if (0 != strcmp (tc->rf_name, ch.channel_name))
	{
	  g_free (tc->rf_name);
	  tc->rf_name = g_strdup (ch.channel_name);
	}

      tc->frequ = ch.frequency;

      if (tc == tc_last)
	break;
    }

  if (tunable_input (ce, main_info, tc_first))
    {
      entry_fine_tuning_set (ce, main_info, ch.frequency);
      tv_set_tuner_frequency (main_info, ch.frequency);
    }

  channel_list_rows_changed (ce, &first, &last);
}

static void
on_channel_up_clicked		(GtkButton *		channel_up,
				 channel_editor *	ce)
{
  GtkTreeIter dummy, first, last, first_prev;
  tveng_tuned_channel *tc;
  guint index;

  if (!channel_list_get_selection (ce, &first, &last, NULL, NULL))
    return;

  if (!tree_model_iter_prev (GTK_TREE_MODEL (ce->channel_model), &first))
    return; /* nothing above */

  tc = channel_list_get_tuned_channel (ce, &first);
  index = channel_list_index (ce, &last) + 1;
  tveng_tuned_channel_move (&global_channel_list, tc, index);

  gtk_list_store_insert_after (ce->channel_model, &dummy, &last);
  gtk_list_store_remove (ce->channel_model, &first);

  channel_list_get_selection (ce, &first, NULL, NULL, NULL);
  first_prev = first;
  if (!tree_model_iter_prev (GTK_TREE_MODEL (ce->channel_model), &first_prev))
    first_prev = first;
  channel_list_scroll_to_cell (ce, &first_prev, 0.01);

  channel_buttons_set_sensitive	(ce, TRUE);
}

static void
on_channel_down_clicked		(GtkButton *		channel_down,
				 channel_editor *	ce)
{
  GtkTreeIter dummy, first, last, last_next;
  tveng_tuned_channel *tc;
  guint index;

  if (!channel_list_get_selection (ce, &first, &last, NULL, NULL))
    return;

  if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (ce->channel_model), &last))
    return; /* nothing below */

  tc = channel_list_get_tuned_channel (ce, &last);
  index = channel_list_index (ce, &first);
  tveng_tuned_channel_move (&global_channel_list, tc, index);

  gtk_list_store_remove (ce->channel_model, &last);
  gtk_list_store_insert_before (ce->channel_model, &dummy, &first);

  channel_list_get_selection (ce, NULL, &last, NULL, NULL);
  last_next = last;
  if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (ce->channel_model), &last_next))
    last_next = last;
  channel_list_scroll_to_cell (ce, &last_next, 0.99);

  channel_buttons_set_sensitive	(ce, TRUE);
}

static void
on_channel_add_clicked		(GtkButton *		channel_add,
				 channel_editor *	ce)
{
  GtkTreeIter iter;

  if (channel_list_get_selection (ce, &iter, NULL, NULL, NULL))
    {
      tveng_tuned_channel_insert (&global_channel_list,
				  tveng_tuned_channel_new (NULL),
				  channel_list_index (ce, &iter));
      gtk_list_store_insert_before (ce->channel_model, &iter, &iter);
    }
  else
    {
      tveng_tuned_channel_insert (&global_channel_list,
				  tveng_tuned_channel_new (NULL),
				  G_MAXINT);
      gtk_list_store_append (ce->channel_model, &iter);
    }

  gtk_tree_selection_unselect_all (ce->channel_selection);
  gtk_tree_selection_select_iter (ce->channel_selection, &iter);

  channel_list_scroll_to_cell (ce, &iter, 0.5);

  channel_buttons_set_sensitive	(ce, TRUE);
}

static void
on_channel_remove_clicked	(GtkButton *		channel_remove,
				 channel_editor *	ce)
{
  tveng_tuned_channel *tc, *tc_next;
  GtkTreeIter iter;

  if (!channel_list_get_selection (ce, &iter, NULL, &tc, NULL))
    return;

  BLOCK (channel_selection, changed,
	 while (VALID_ITER (&iter, ce->channel_model))
	   {
	     if (!gtk_tree_selection_iter_is_selected (ce->channel_selection, &iter))
	       break;
    
     gtk_list_store_remove (ce->channel_model, &iter);

	     tc_next = tc->next;
	     tveng_tuned_channel_remove (&global_channel_list, tc);
	     tveng_tuned_channel_delete (tc);
	     tc = tc_next;
	   }
	 );

  if (VALID_ITER (&iter, ce->channel_model))
    {
      gtk_tree_selection_select_iter (ce->channel_selection, &iter);
      channel_list_scroll_to_cell (ce, &iter, 0.5);
    }
  else
    {
      no_channel_selected (ce);
    }
}

static void
on_entry_name_changed		(GtkEditable *		channel_name,
				 channel_editor *	ce)
{
  tveng_tuned_channel *tc, *tc_last;
  GtkTreeIter first, last;
  gchar *name;

  if (!channel_list_get_selection (ce, &first, &last, &tc, &tc_last))
    return;

  name = gtk_editable_get_chars (channel_name, 0, -1);

  g_free (tc->name);
  tc->name = name;

  while (tc != tc_last)
    {
      tc = tc->next;

      g_free (tc->name);
      tc->name = g_strdup (name);
    }

  channel_list_rows_changed (ce, &first, &last);
}

static void
on_entry_input_changed		(GtkOptionMenu *	entry_input,
				 channel_editor *	ce)
{
  tveng_tuned_channel *tc, *tc_last;
  GtkTreeIter first, last;
  guint id;

  if (!channel_list_get_selection (ce, &first, &last, &tc, &tc_last))
    return;

  id = z_option_menu_get_active (GTK_WIDGET (entry_input));

  if (id == DONT_CHANGE)
    {
      tc->input = 0;
    }
  else
    {
      const tv_video_line *l;

      l = tv_nth_video_input (main_info, id - 1);
      tc->input = l->hash;
      tv_set_video_input (main_info, l);
    }

  for (; tc_last != tc; tc_last = tc_last->prev)
    tc_last->input = tc->input;

  channel_list_rows_changed (ce, &first, &last);
}

static void
on_entry_fine_tuning_value_changed (GtkAdjustment *	spin_adj,
				    channel_editor *	ce)
{
  tveng_tuned_channel *tc, *tc_last;
  GtkTreeIter first, last;

  if (!channel_list_get_selection (ce, &first, &last, &tc, &tc_last))
    return;

  tc->frequ = (guint)(spin_adj->value * 1000000);

  tv_set_tuner_frequency (main_info, tc->frequ);

  for (; tc_last != tc; tc_last = tc_last->prev)
    {
      if (0 != strcmp (tc->rf_name, tc_last->rf_name))
	{
	  g_free (tc_last->rf_name);
	  tc_last->rf_name = g_strdup (tc->rf_name);
	}

      tc_last->frequ = tc->frequ;
    }

  channel_list_rows_changed (ce, &first, &last);
}

static void
on_entry_standard_changed	(GtkOptionMenu *	entry_standard,
				 channel_editor *	ce)
{
  tveng_tuned_channel *tc, *tc_last;
  GtkTreeIter first, last;
  guint id;

  if (!channel_list_get_selection (ce, &first, &last, &tc, &tc_last))
    return;

  id = z_option_menu_get_active (GTK_WIDGET (entry_standard));

  if (id == DONT_CHANGE)
    {
      tc->standard = 0;
    }
  else
    {
      const tv_video_standard *s;

      s = tv_nth_video_standard (main_info, id - 1);
      tc->standard = s->hash;
      tv_set_video_standard (main_info, s);
    }

  for (; tc_last != tc; tc_last = tc_last->prev)
    tc_last->standard = tc->standard;

  channel_list_rows_changed (ce, &first, &last);
}

static void
on_entry_accel_changed		(GtkEditable *		editable,
				 channel_editor *	ce)
{
  tveng_tuned_channel *tc, *tc_last;
  GtkTreeIter first, last;
  z_key key;

  if (!channel_list_get_selection (ce, &first, &last, &tc, &tc_last))
    return;

  key = z_key_entry_get_key (ce->entry_accel);

  for (;;)
    {
      tc->accel = key;
      if (tc == tc_last)
	break;
      tc = tc->next;
    }

  channel_list_rows_changed (ce, &first, &last);
}

static void
on_channel_selection_changed	(GtkTreeSelection *	selection,
				 channel_editor *	ce)
{
  GtkTreeIter iter;
  tveng_tuned_channel *tc;

  if (!channel_list_get_selection (ce, &iter, NULL, &tc, NULL))
    {
      no_channel_selected (ce);
      return;
    }

  z_switch_channel (tc, main_info);

  BLOCK (entry_name, changed,
	 gtk_entry_set_text (ce->entry_name, tc->name));

  BLOCK (entry_input, changed,
         {
	   const tv_video_line *l;
	   guint index;

	   l = NULL;
	   index = 0;

	   if (tc->input != DONT_CHANGE)
	     for (l = tv_next_video_input (main_info, NULL);
		  l; l = tv_next_video_input (main_info, l), ++index)
	       if (l->hash == tc->input)
		 break;

	   if (l)
	     z_option_menu_set_active (GTK_WIDGET (ce->entry_input), index + 1);
	 }
  );

  SIGNAL_HANDLER_BLOCK (ce->spinslider_adj,
			(gpointer) on_entry_fine_tuning_value_changed,
			entry_fine_tuning_set (ce, main_info, tc->frequ));

  BLOCK (entry_standard, changed,
	 {
	   const tv_video_standard *s;
	   guint index;

	   /* Standards depend on current input */
	   gtk_widget_destroy (gtk_option_menu_get_menu (ce->entry_standard));
	   gtk_option_menu_set_menu (ce->entry_standard,
				     GTK_WIDGET (create_standard_menu (ce)));

	   s = NULL;
	   index = 0;

	   if (tc->standard != DONT_CHANGE)
	     for (s = tv_next_video_standard (main_info, NULL);
		  s; s = tv_next_video_standard (main_info, s), ++index)
	       if (s->hash == tc->standard)
		 break;

	   if (s)
	     z_option_menu_set_active (GTK_WIDGET (ce->entry_standard), index + 1);
	 }
  );

  SIGNAL_HANDLER_BLOCK (z_key_entry_entry (ce->entry_accel),
			(gpointer) on_entry_accel_changed,
			z_key_entry_set_key (ce->entry_accel, tc->accel));

  gtk_widget_set_sensitive (GTK_WIDGET (ce->entry_table), TRUE);

  channel_buttons_set_sensitive (ce, TRUE);
}

static void
on_ok_clicked			(GtkButton *		ok,
				 channel_editor *	ce)
{
  tveng_tuned_channel_list_delete (&ce->old_channel_list);

  gtk_widget_destroy (GTK_WIDGET (ce->channel_editor));
}

static void
on_help_clicked			(GtkButton *		cancel,
				 channel_editor *	ce)
{
  /* XXX handle error */
  gnome_help_display ("zapping", "zapping-channel-editor", NULL);
}

static void
on_cancel_clicked		(GtkButton *		cancel,
				 channel_editor *	ce)
{
  gtk_widget_destroy (GTK_WIDGET (ce->channel_editor));
}

static void
on_channel_editor_destroy	(GtkObject *		unused,
				 channel_editor *	ce)
{
  if (ce->search)
    gtk_widget_destroy (GTK_WIDGET (ce->search->station_search));

  if (ce->old_channel_list)
    {
      tveng_tuned_channel_list_delete (&global_channel_list);
      global_channel_list = ce->old_channel_list;
      ce->old_channel_list = NULL;
    }

  {
    struct country *c;

    for (c = &g_array_index (ce->country_table, struct country, 0); c->table_name; c++)
      {
	g_free (c->table_name);
	g_free (c->gui_name);
      }

    g_array_free (ce->country_table, /* elements */ FALSE);
  }

  g_free (ce);

  /* Update menus. XXX should rebuild automatically when
     opened after any change. */
  zmodel_changed (z_input_model);

  dialog_running = NULL;
}

/*
 * channel_list GtkTreeCellDataFuncs
 */

static void
set_func_index			(GtkTreeViewColumn *	column,
				 GtkCellRenderer *	cell,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter,
				 gpointer		data)
{
  tveng_tuned_channel *tc = tree_model_tuned_channel (model, iter);
  gchar buf[32];

  g_snprintf (buf, sizeof (buf) - 1, "%u", tc->index);

  g_object_set (GTK_CELL_RENDERER (cell), "text", buf, NULL);
}

static void
set_func_name			(GtkTreeViewColumn *	column,
				 GtkCellRenderer *	cell,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter,
				 gpointer		data)
{
  tveng_tuned_channel *tc = tree_model_tuned_channel (model, iter);

  g_object_set (GTK_CELL_RENDERER (cell), "text", tc->name, NULL);
}

static void
set_func_input			(GtkTreeViewColumn *	column,
				 GtkCellRenderer *	cell,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter,
				 gpointer		data)
{
  tveng_tuned_channel *tc = tree_model_tuned_channel (model, iter);
  const tv_video_line *l;
  gchar *input_name = NULL;

  if (tc->input != DONT_CHANGE)
    if ((l = tv_video_input_by_hash (main_info, tc->input)))
      input_name = l->label;

  g_object_set (GTK_CELL_RENDERER (cell), "text", input_name, NULL);
}

static void
set_func_channel		(GtkTreeViewColumn *	column,
				 GtkCellRenderer *	cell,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter,
				 gpointer		data)
{
  channel_editor *ce = data;
  tveng_tuned_channel *tc = tree_model_tuned_channel (model, iter);

  if (tunable_input (ce, main_info, tc)
      && tc->frequ != 0)
    {
      gchar *rf_name;

      rf_name = rf_channel_string (tc);
      g_object_set (GTK_CELL_RENDERER (cell), "text", rf_name, NULL);
      g_free (rf_name);
    }
  else
    {
      g_object_set (GTK_CELL_RENDERER (cell), "text", NULL, NULL);
    }
}

static void
set_func_standard		(GtkTreeViewColumn *	column,
				 GtkCellRenderer *	cell,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter,
				 gpointer		data)
{
  tveng_tuned_channel *tc = tree_model_tuned_channel (model, iter);
  const tv_video_standard *s;
  gchar *standard_name = NULL;

  if (tc->standard != DONT_CHANGE)
    if ((s = tv_video_standard_by_hash (main_info, tc->standard)))
      standard_name = s->label;

  g_object_set (GTK_CELL_RENDERER (cell), "text", standard_name, NULL);
}

static void
set_func_key			(GtkTreeViewColumn *	column,
				 GtkCellRenderer *	cell,
				 GtkTreeModel *		model,
				 GtkTreeIter *		iter,
				 gpointer		data)
{
  tveng_tuned_channel *tc = tree_model_tuned_channel (model, iter);
  gchar *key_name;

  key_name = z_key_name (tc->accel);

  g_object_set (GTK_CELL_RENDERER (cell), "text", key_name, NULL);

  g_free (key_name);
}

#define LABEL(name, x, y)						\
  label = gtk_label_new (_(name));					\
  gtk_widget_show (label);						\
  gtk_table_attach (ce->entry_table, label, x, x + 1, y, y + 1,		\
		    (GtkAttachOptions) (GTK_FILL),			\
		    (GtkAttachOptions) (0), 0, 0);			\
  gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);			\
  gtk_misc_set_padding (GTK_MISC (label), 3, 0);

#define BUTTON(name, stock, sensitive)					\
  ce->name = gtk_button_new_from_stock (stock);				\
  gtk_widget_show (ce->name);						\
  gtk_widget_set_sensitive (ce->name, sensitive);			\
  gtk_box_pack_start (GTK_BOX (vbox), ce->name, FALSE, FALSE, 0);	\
  CONNECT (name, clicked);

#define CONNECT(object, signal)						\
  g_signal_connect (G_OBJECT (ce->object), #signal,			\
		    G_CALLBACK (on_ ## object ## _ ## signal), ce)

static gint
country_compare			(struct country *	c1,
				 struct country *	c2)
{
  return g_utf8_collate (c1->gui_name, c2->gui_name);
}

static GtkWidget *
create_country_menu		(channel_editor *	ce)
{
  GtkWidget *country_menu;
  GtkWidget *menu;
  tv_rf_channel ch;
  const gchar *table_name;
  const gchar *country_code;
  gchar buf[4];
  gint hist, i;
  struct country *c;

  country_menu = gtk_option_menu_new ();
  gtk_widget_show (country_menu);
  gtk_tooltips_set_tip (ce->tooltips, country_menu,
			_("Select the frequency table "
			  "used in your country"), NULL);

  menu = gtk_menu_new ();
  gtk_option_menu_set_menu (GTK_OPTION_MENU (country_menu), menu);

  tv_rf_channel_first_table (&ch);

  ce->country_table = g_array_new (/* zero_term */ TRUE,
				   /* clear */ FALSE,
				   sizeof (struct country));
  do {
    do {
      const char *country_name;

      if ((country_name = iso3166_to_country_name (ch.country_code)))
	{
	  struct country c;

	  c.table_name = g_strconcat (ch.country_code, "@", ch.table_name, NULL);

	  if (ch.domain)
	    c.gui_name = g_strdup_printf ("%s (%s)", country_name, ch.domain);
	  else
	    c.gui_name = g_strdup (country_name);

	  g_array_append_val (ce->country_table, c);
	}
    } while (tv_rf_channel_next_country (&ch));
  } while (tv_rf_channel_next_table (&ch));

  g_array_sort (ce->country_table, (GCompareFunc) country_compare);

  /*
   *  Default country, table or both
   *  from e.g. "", "FR", "FR@ccir", "ccir", "Europe" (old current_country)
   */
  table_name = zconf_get_string (NULL, "/zapping/options/main/current_country");
  country_code = locale_country ();

  if (table_name
      && g_ascii_isalpha (table_name[0])
      && g_ascii_isalpha (table_name[1])
      && '@' == table_name[2])
    {
      buf[0] = table_name[0];
      buf[1] = table_name[1];
      buf[2] = 0;
      country_code = buf;
      table_name += 3;
    }

  if (table_name
      && 0 == table_name[0])
    table_name = NULL;

  hist = -1;
  i = 0;

  for (c = &g_array_index (ce->country_table, struct country, 0);
       c->table_name; c++)
    {
      GtkWidget *item;

      item = gtk_menu_item_new_with_label (c->gui_name);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      if (hist < 0)
	if (!table_name || 0 == strcmp (c->table_name + 3, table_name))
	  if (!country_code || 0 == strncmp (c->table_name, country_code, 2))
	    hist = i;
      i++;
    }

  if (hist < 0)
    hist = 0; /* any */
  else if (!table_name)
    zconf_set_string (g_array_index (ce->country_table, struct country, hist).table_name,
		      "/zapping/options/main/current_country");

  gtk_option_menu_set_history (GTK_OPTION_MENU (country_menu), hist);

  return country_menu;
}

static GtkListStore *
create_freq_list_model		(const tv_rf_channel *	table)
{
  GtkListStore *model;
  tv_rf_channel ch;

  model = gtk_list_store_new (FL_NUM_COLUMNS,
			      G_TYPE_STRING,	/* name */
			      G_TYPE_STRING);	/* freq */
  ch = *table;

  do
    {
      gchar freq[256];
      GtkTreeIter iter;

      g_snprintf (freq, sizeof (freq) - 1, "%.2f", ch.frequency / 1e6);

      gtk_list_store_append (model, &iter);
      gtk_list_store_set (model, &iter,
			  FL_NAME, ch.channel_name,
			  FL_FREQ, freq, -1);
    }
  while (tv_rf_channel_next (&ch));

  return model;
}

static GtkWidget *
create_freq_treeview		(channel_editor *	ce)
{
  GtkWidget *scrolledwindow;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  tv_rf_channel ch;

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolledwindow);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
				  GTK_POLICY_NEVER,
				  GTK_POLICY_AUTOMATIC);

  ce->freq_treeview = GTK_TREE_VIEW (gtk_tree_view_new ());
  gtk_widget_show (GTK_WIDGET (ce->freq_treeview));
  gtk_container_add (GTK_CONTAINER (scrolledwindow), GTK_WIDGET (ce->freq_treeview));
  gtk_tree_view_set_rules_hint (ce->freq_treeview, TRUE);

  ce->freq_selection = gtk_tree_view_get_selection (ce->freq_treeview);
  gtk_tree_selection_set_mode (ce->freq_selection, GTK_SELECTION_SINGLE);
  CONNECT (freq_selection, changed);

  current_rf_channel_table (ce, &ch, NULL);
  ce->freq_model = create_freq_list_model (&ch);
  gtk_tree_view_set_model (ce->freq_treeview, GTK_TREE_MODEL (ce->freq_model));

  renderer = gtk_cell_renderer_text_new ();
  /* TRANSLATORS: RF channel name in frequency table. */
  column = gtk_tree_view_column_new_with_attributes
    (_("Ch. Name"), renderer, "text", FL_NAME, NULL);
  gtk_tree_view_append_column (ce->freq_treeview, column);  
	  
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes
    (_("Freq (MHz)"), renderer, "text", FL_FREQ, NULL);
  gtk_tree_view_append_column (ce->freq_treeview, column);  

  return scrolledwindow;
}

static GtkListStore *
create_channel_list_model	(const tveng_tuned_channel *list)
{
  GtkListStore *model;
  GtkTreeIter iter;
  guint i;

  model = gtk_list_store_new (1, G_TYPE_UINT);

  for (i = tveng_tuned_channel_num (global_channel_list); i; i--)
    gtk_list_store_append (model, &iter);

  return model;
}

static GtkWidget *
create_channel_treeview		(channel_editor *	ce)
{
  GtkWidget *scrolledwindow;

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolledwindow);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow),
				  GTK_POLICY_NEVER,
				  GTK_POLICY_AUTOMATIC);

  ce->channel_treeview = GTK_TREE_VIEW (gtk_tree_view_new ());
  gtk_widget_show (GTK_WIDGET (ce->channel_treeview));
  gtk_container_add (GTK_CONTAINER (scrolledwindow),
		     GTK_WIDGET (ce->channel_treeview));
  gtk_tree_view_set_rules_hint (ce->channel_treeview, TRUE);
  /*
  gtk_tree_view_set_search_column (ce->channel_treeview, CL_NAME);
  */
  ce->channel_selection = gtk_tree_view_get_selection (ce->channel_treeview);
  gtk_tree_selection_set_mode (ce->channel_selection, GTK_SELECTION_MULTIPLE);
  CONNECT (channel_selection, changed);

  ce->channel_model = create_channel_list_model (global_channel_list); /* XXX */
  gtk_tree_view_set_model (ce->channel_treeview,
			   GTK_TREE_MODEL (ce->channel_model));

  gtk_tree_view_insert_column_with_data_func
    (ce->channel_treeview, -1 /* append */, "",
     gtk_cell_renderer_text_new (), set_func_index, ce, NULL);

  gtk_tree_view_insert_column_with_data_func
    (ce->channel_treeview, -1 /* append */, _("Channel name"),
     gtk_cell_renderer_text_new (), set_func_name, ce, NULL);

  gtk_tree_view_insert_column_with_data_func
    (ce->channel_treeview, -1 /* append */, _("Video input"),
     gtk_cell_renderer_text_new (), set_func_input, ce, NULL);

  gtk_tree_view_insert_column_with_data_func
    (ce->channel_treeview, -1 /* append */, _("RF Channel"),
     gtk_cell_renderer_text_new (), set_func_channel, ce, NULL);

  gtk_tree_view_insert_column_with_data_func
    (ce->channel_treeview, -1 /* append */, _("Video standard"),
     gtk_cell_renderer_text_new (), set_func_standard, ce, NULL);
  
  gtk_tree_view_insert_column_with_data_func
    (ce->channel_treeview, -1 /* append */, _("Accelerator"),
     gtk_cell_renderer_text_new (), set_func_key, ce, NULL);
  
  return scrolledwindow;
}

static GtkMenu *
create_input_menu		(channel_editor *	ce)
{
  GtkMenu *menu;
  GtkWidget *menu_item;
  const tv_video_line *l;

  menu = GTK_MENU (gtk_menu_new ());

  menu_item = gtk_menu_item_new_with_label (_("Do not change input"));
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  for (l = tv_next_video_input (main_info, NULL);
       l; l = tv_next_video_input (main_info, l))
    {
      menu_item = gtk_menu_item_new_with_label (l->label);
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    }

  return menu;
}

static GtkMenu *
create_standard_menu		(channel_editor *	ce)
{
  GtkMenu *menu;
  GtkWidget *menu_item;
  const tv_video_standard *s;

  menu = GTK_MENU (gtk_menu_new ());

  menu_item = gtk_menu_item_new_with_label (_("Do not change standard"));
  gtk_widget_show (menu_item);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);

  for (s = tv_next_video_standard (main_info, NULL);
       s; s = tv_next_video_standard (main_info, s))
    {
      menu_item = gtk_menu_item_new_with_label (s->label);
      gtk_widget_show (menu_item);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    }

  return menu;
}

static GtkWidget *
create_channel_editor		(void)
{
  struct channel_editor *ce;
  const tv_video_line *l;

  ce = g_malloc (sizeof (*ce));

  ce->old_channel_list = tveng_tuned_channel_list_new (global_channel_list);

  ce->search = NULL;

  for (l = tv_next_video_input (main_info, NULL);
       l; l = tv_next_video_input (main_info, l))
    if (l->type == TV_VIDEO_LINE_TYPE_TUNER)
      break;

  ce->have_tuners = (l != NULL);

  /* Build dialog */

  ce->tooltips = gtk_tooltips_new ();

  ce->channel_editor = GTK_DIALOG (gtk_dialog_new ());
  gtk_window_set_title (GTK_WINDOW (ce->channel_editor), _("Channel editor"));
  CONNECT (channel_editor, destroy);

  {
    GtkWidget *dialog_vbox;

    dialog_vbox = GTK_DIALOG (ce->channel_editor)->vbox;
    gtk_widget_show (dialog_vbox);

    ce->vbox = GTK_BOX (gtk_vbox_new (FALSE, 3));
    gtk_widget_show (GTK_WIDGET (ce->vbox));
    gtk_box_pack_start (GTK_BOX (dialog_vbox),
			GTK_WIDGET (ce->vbox), TRUE, TRUE, 0);

    {
      GtkWidget *vpaned;

      vpaned = gtk_vpaned_new ();
      gtk_widget_show (vpaned);
      gtk_box_pack_start (ce->vbox, vpaned, TRUE, TRUE, 0);

      {
	GtkWidget *frame;

	frame = gtk_frame_new (_("Region"));
	gtk_widget_show (frame);
	gtk_container_add (GTK_CONTAINER (vpaned), frame);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 3);
	gtk_widget_set_sensitive (frame, ce->have_tuners);

	{
	  GtkWidget *hbox;
	  GtkWidget *scrolledwindow;
	  
	  hbox = gtk_hbox_new (FALSE, 0);
	  gtk_widget_show (hbox);
	  gtk_container_add (GTK_CONTAINER (frame), hbox);
	
	  {
	    GtkWidget *vbox;
#if 0
	    GtkWidget *label;
#endif	    
	    vbox = gtk_vbox_new (FALSE, 3);
	    gtk_widget_show (vbox);
	    gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
	    
	    ce->country_menu = GTK_OPTION_MENU (create_country_menu (ce));
	    gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (ce->country_menu),
				FALSE, FALSE, 0);
	    CONNECT (country_menu, changed);
	    
	    {
	      GtkWidget *hbox;
	      
	      hbox = gtk_vbox_new (TRUE, 3);
	      gtk_widget_show (hbox);
	      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	      
	      ce->channel_search = GTK_BUTTON (gtk_button_new_with_mnemonic
					       (_("Automatic station _search")));
	      gtk_widget_show (GTK_WIDGET (ce->channel_search));
	      gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (ce->channel_search),
				  TRUE, TRUE, 0);
	      CONNECT (channel_search, clicked);
	      gtk_tooltips_set_tip (ce->tooltips, GTK_WIDGET (ce->channel_search),
				    _("Select a suitable frequency table, then "
				      "click here to search through all channels "
				      "in the table and add the received stations "
				      "to the channel list."), NULL);
	      
	      ce->add_all_channels = GTK_BUTTON (gtk_button_new_with_mnemonic
						 (_("Add all _channels")));
	      gtk_widget_show (GTK_WIDGET (ce->add_all_channels));
	      gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (ce->add_all_channels),
				  TRUE, TRUE, 0);
	      CONNECT (add_all_channels, clicked);
	      gtk_tooltips_set_tip (ce->tooltips,
				    GTK_WIDGET (ce->add_all_channels),
				    _("Add all channels in the frequency "
				      "table to the channel list."), NULL);

	      ce->import_xawtv = GTK_BUTTON (gtk_button_new_with_mnemonic
					     (_("_Import XawTV configuration")));
	      gtk_widget_show (GTK_WIDGET (ce->import_xawtv));
	      gtk_widget_set_sensitive (GTK_WIDGET (ce->import_xawtv),
					xawtv_config_present ());
	      gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (ce->import_xawtv),
				  TRUE, TRUE, 0);
	      CONNECT (import_xawtv, clicked);
	    }

#if 0
	    label = gtk_label_new (("When your country is not listed or "
				     "misrepresented please send an e-mail,\n"
				     "if possible including the correct "
				     "frequency table, to zapping-misc@lists.sf.net."));
	    gtk_widget_show (label);
	    gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
#endif
	  }
	  
	  scrolledwindow = create_freq_treeview (ce);
	  gtk_box_pack_start (GTK_BOX (hbox), scrolledwindow, TRUE, TRUE, 0);
	}
      }
      
      {
	GtkWidget *frame;
	
	frame = gtk_frame_new (_("Channel List"));
	gtk_widget_show (frame);
	gtk_container_add (GTK_CONTAINER (vpaned), frame);
	gtk_container_set_border_width (GTK_CONTAINER (frame), 3);
	
	{
	  GtkWidget *hbox;
	  GtkWidget *scrolledwindow;
	  
	  hbox = gtk_hbox_new (FALSE, 3);
	  gtk_widget_show (hbox);
	  gtk_container_add (GTK_CONTAINER (frame), hbox);
	  
	  scrolledwindow = create_channel_treeview (ce);
	  gtk_box_pack_start (GTK_BOX (hbox), scrolledwindow, TRUE, TRUE, 0);
	  
	  {
	    GtkWidget *vbox;
	    
	    vbox = gtk_vbox_new (FALSE, 3);
	    gtk_widget_show (vbox);
	    gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);
	    
	    BUTTON (channel_up,	  GTK_STOCK_GO_UP,   FALSE);
	    BUTTON (channel_down,	  GTK_STOCK_GO_DOWN, FALSE);
	    BUTTON (channel_add,	  GTK_STOCK_ADD,     TRUE);
	    BUTTON (channel_remove, GTK_STOCK_REMOVE,  FALSE);
	  }
	}
      }
    }

    {
      GtkWidget *frame;

      frame = gtk_frame_new (_("Edit channel"));
      gtk_widget_show (frame);
      gtk_box_pack_start (ce->vbox, frame, FALSE, FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (frame), 3);

      {
	GtkWidget *label;
	GtkWidget *menu;
	  
	ce->entry_table = GTK_TABLE (gtk_table_new (5, 2, FALSE));
	gtk_widget_show (GTK_WIDGET (ce->entry_table));
	gtk_widget_set_sensitive (GTK_WIDGET (ce->entry_table), FALSE);
	gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (ce->entry_table));
	gtk_table_set_row_spacings (ce->entry_table, 3);
	gtk_table_set_col_spacings (ce->entry_table, 3);

	LABEL (_("Name:"), 0, 0);
  
	ce->entry_name = GTK_ENTRY (gtk_entry_new ());
	gtk_widget_show (GTK_WIDGET (ce->entry_name));
	CONNECT (entry_name, changed);
	gtk_table_attach (ce->entry_table, GTK_WIDGET (ce->entry_name), 1, 2, 0, 1,
			  (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions)(0), 0, 0);

	LABEL (_("Video input:"), 0, 1);

	ce->entry_input = GTK_OPTION_MENU (gtk_option_menu_new ());
	gtk_widget_show (GTK_WIDGET (ce->entry_input));
	CONNECT (entry_input, changed);
	gtk_table_attach (ce->entry_table, GTK_WIDGET (ce->entry_input), 1, 2, 1, 2,
			  (GtkAttachOptions)(GTK_FILL),
			  (GtkAttachOptions)(0), 0, 0);
	gtk_option_menu_set_menu (ce->entry_input,
				  GTK_WIDGET (create_input_menu (ce)));

	LABEL (_("Fine tuning:"), 0, 2);

	ce->spinslider_adj = GTK_ADJUSTMENT
	  (gtk_adjustment_new (0, 0, 0, 0, 0, 0));

	ce->entry_fine_tuning = z_spinslider_new (ce->spinslider_adj, NULL, _("MHz"), 0, 2);
	gtk_widget_show (ce->entry_fine_tuning);
	gtk_widget_set_sensitive (ce->entry_fine_tuning, FALSE);
	gtk_table_attach (ce->entry_table, ce->entry_fine_tuning, 1, 2, 2, 3,
			  (GtkAttachOptions)(GTK_FILL),
			  (GtkAttachOptions)(GTK_FILL), 0, 0);
	g_signal_connect (G_OBJECT (ce->spinslider_adj), "value-changed",
			  G_CALLBACK (on_entry_fine_tuning_value_changed), ce);

	LABEL (_("Video standard:"), 0, 3);

	ce->entry_standard = GTK_OPTION_MENU (gtk_option_menu_new ());
	gtk_widget_show (GTK_WIDGET (ce->entry_standard));
	CONNECT (entry_standard, changed);
	gtk_table_attach (ce->entry_table, GTK_WIDGET (ce->entry_standard), 1, 2, 3, 4,
			  (GtkAttachOptions)(GTK_FILL),
			  (GtkAttachOptions)(0), 0, 0);
	menu = gtk_menu_new ();
	gtk_option_menu_set_menu (ce->entry_standard, menu);

	LABEL (_("Accelerator:"), 0, 4);

	ce->entry_accel = z_key_entry_new ();
	gtk_widget_show (ce->entry_accel);
	gtk_table_attach (ce->entry_table, ce->entry_accel, 1, 2, 4, 5,
			  (GtkAttachOptions)(GTK_FILL),
			  (GtkAttachOptions)(GTK_FILL), 0, 0);
	g_signal_connect (G_OBJECT (z_key_entry_entry (ce->entry_accel)),
			  "changed", G_CALLBACK (on_entry_accel_changed), ce);
      }
    }
  }

  {
    GtkWidget *dialog_action_area;

    dialog_action_area = GTK_DIALOG (ce->channel_editor)->action_area;
    gtk_widget_show (dialog_action_area);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (dialog_action_area), GTK_BUTTONBOX_END);

    {
      GtkWidget *hbox;
      GtkWidget *ok;
      GtkWidget *cancel;
  
      hbox = gtk_hbox_new (TRUE, 15);
      gtk_widget_show (hbox);
      gtk_container_add (GTK_CONTAINER (dialog_action_area), hbox);
      
      cancel = gtk_button_new_from_stock (GTK_STOCK_HELP);
      gtk_widget_show (cancel);
      gtk_box_pack_start (GTK_BOX (hbox), cancel, FALSE, TRUE, 0);
      g_signal_connect (G_OBJECT (cancel), "clicked",
			G_CALLBACK (on_help_clicked), ce);

      cancel = gtk_button_new_from_stock (GTK_STOCK_CANCEL);
      gtk_widget_show (cancel);
      gtk_box_pack_start (GTK_BOX (hbox), cancel, FALSE, TRUE, 0);
      g_signal_connect (G_OBJECT (cancel), "clicked",
			G_CALLBACK (on_cancel_clicked), ce);

      ok = gtk_button_new_from_stock (GTK_STOCK_OK);
      gtk_widget_show (ok);
      gtk_box_pack_start (GTK_BOX (hbox), ok, FALSE, TRUE, 0);
      g_signal_connect (G_OBJECT (ok), "clicked",
			G_CALLBACK (on_ok_clicked), ce);
    }
  }

  return GTK_WIDGET (ce->channel_editor);
}

static PyObject *
py_channel_editor		(PyObject *		self,
				 PyObject *		args)
{
  if (!dialog_running)
    gtk_widget_show (create_channel_editor ());
  else
    gtk_window_present (GTK_WINDOW (dialog_running->channel_editor));

  py_return_true;
}

void
startup_channel_editor		(void)
{
  cmd_register ("channel_editor", py_channel_editor, METH_VARARGS,
		("Channel editor"), "zapping.channel_editor()");
}

void
shutdown_channel_editor		(void)
{
}
